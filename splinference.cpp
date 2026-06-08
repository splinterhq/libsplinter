/**
 * Sidecar inference daemon for libsplinter.
 * Listens to a Splinter signal group, computes embeddings for modified keys
 * using llama.cpp (Nomic Text v2 (Quantized, 1.3B params)), and writes the 
 * 768-d vector back to the slot.
 * 
 * Copyright (C) 2026 Tim Post <timthepost@protonmail.com>
 * License: Apache 2
 * 
 * TODO:
 * 
 *  - Multi-thread (one per signal group) operation to centralize model
 *    use. (Low Priority)
 *
 *  - Implement argparse (see 3rdparty/) for argument parsing. (Low Priority)
 */

#include <atomic>
#include <math.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <csignal>
#include <chrono>
#include <thread>

// Bridge the C/C++ atomic divide before including the C header
using atomic_uint_least64_t = std::atomic_uint_least64_t;
using atomic_uint_least32_t = std::atomic_uint_least32_t;
using atomic_uint_least8_t  = std::atomic_uint_least8_t;
using atomic_int_least32_t  = std::atomic_int_least32_t;

// Already defined at build (required for this target), but
// set explicitly for the language server's comfort.
#ifndef SPLINTER_EMBEDDINGS
#define SPLINTER_EMBEDDINGS
#endif

// Now Splinter / llama (last)
#include "splinter.h"
#include "llama-cpp.h"

#include <sys/mman.h>   // POSIX_MADV_* for splinter_madvise()

volatile sig_atomic_t keep_running = 1;

// --- Logic Shard cooperative memory advisement -----------------------------
// This embedding sidecar registers as a well-behaved Logic Shard: it declares
// WILLNEED at a low priority during the live loop (so the latency-sensitive
// completer always preempts it) and SEQUENTIAL during the backfill sweep.
// Its bid is non-blocking — it never fights a higher-priority WILLNEED holder.
#define SHARD_ID           0x5F10u   // "SP-embed"
// Declared windows in splinter_now() ticks. Re-bid frequently, so the window
// only needs to outlast one loop iteration; backfill gets a much longer window.
#define SHARD_DUR_LIVE     (1ULL << 30)
#define SHARD_DUR_BACKFILL (1ULL << 36)
#define SHARD_PRIO_LIVE     40        // below the completer (200)
#define SHARD_PRIO_BACKFILL 20        // below live

// The scorer (semsage app/scorer) deposits each specimen under a fresh UUID and
// sets the WAITING label (0x40) to mark the slot "awaiting inference". The label
// lifecycle in splinter.h has the sidecar clear WAITING once the vector lands;
// that cleared bit is the scorer's authoritative "THIS deposit was serviced"
// signal. Without it the scorer falls back to magnitude-only polling, which on a
// reused slot can mistake a prior tenant's residual vector for a fresh result
// (stale scores against a fresh UUID + char count). Keep this in sync with
// scorer.cpp's WAITING_LABEL.
#define WAITING_LABEL 0x40ULL

// Upper bound on the keys[] array we hand to splinter_list(). We size the array
// to the store's actual slot geometry (see dynamic_key_capacity()), but never
// allocate more than this regardless of how large the store declares itself.
// This is expected to diverge from splainference's own ceiling over time.
#define MAX_DYNAMIC_KEYS 500000u

// Translate a declared splinter_intent_t into a raw POSIX advice int.
static inline int advice_for(uint8_t intent) {
    switch (intent) {
        case SPL_INTENT_WILLNEED:   return POSIX_MADV_WILLNEED;
        case SPL_INTENT_SEQUENTIAL: return POSIX_MADV_SEQUENTIAL;
        case SPL_INTENT_RANDOM:     return POSIX_MADV_RANDOM;
        case SPL_INTENT_DONTNEED:   return POSIX_MADV_DONTNEED;
        default:                    return POSIX_MADV_NORMAL;
    }
}

// Decide how many key slots to allocate for splinter_list(), based on the
// store's declared geometry and capped at MAX_DYNAMIC_KEYS. If the store is
// larger than we can read, warn once to stderr and read up to the cap. The
// store must already be open before this is called.
static size_t dynamic_key_capacity() {
    splinter_header_snapshot_t hdr = {};
    if (splinter_get_header_snapshot(&hdr) != 0) {
        // Geometry unavailable; fall back to the hard ceiling so we still run.
        return MAX_DYNAMIC_KEYS;
    }
    if (hdr.slots > MAX_DYNAMIC_KEYS) {
        std::cerr << "[Warning]: store declares " << hdr.slots << " slots but this "
                  << "build reads at most " << MAX_DYNAMIC_KEYS
                  << "; only the first " << MAX_DYNAMIC_KEYS
                  << " keys will be processed.\n";
        return MAX_DYNAMIC_KEYS;
    }
    return hdr.slots;
}

// Helper to check if a vector is zeroed out
bool needs_embedding(const float* vec, size_t len) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) sum += (double)vec[i] * vec[i];
    return std::sqrt(sum) < 1e-6; // Effectively zero
}

uint64_t process_key(const char* key, llama_context* ctx, const llama_vocab* vocab) {
    size_t val_len = 0;
    uint64_t current_epoch = splinter_get_epoch(key);

    // odd epochs mean busy writers
    if (current_epoch & 1) return 0;

    uint64_t ptr_epoch = 0;
    const void* raw_ptr = splinter_get_raw_ptr(key, &val_len, &ptr_epoch);
    if (!raw_ptr || val_len == 0) return 0;
    if (ptr_epoch != current_epoch) return 0;

    // tokenization
    std::vector<llama_token> tokens(val_len + 8);
    int n_tokens = llama_tokenize(vocab, static_cast<const char*>(raw_ptr), val_len, 
                                  tokens.data(), tokens.size(), true, false);

    if (n_tokens < 0) {
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(vocab, static_cast<const char*>(raw_ptr), val_len, 
                                  tokens.data(), tokens.size(), true, false);
    }

    if (splinter_get_epoch(key) != current_epoch) {
        return 0;
    }

    // inference
    // Wipe any residual state from a prior key before decoding. Every key is
    // decoded into seq_id 0, and Nomic is a non-causal (bidirectional) +
    // mean-pooled embedding model, so leftover tokens from a longer previous
    // sequence stay resident in the cache and get attended to here — silently
    // contaminating the pooled embedding. Without this, the vector for a given
    // text depends on what was decoded before it (processing order / history),
    // so the same text yields different vectors run-to-run.
    llama_memory_seq_rm(llama_get_memory(ctx), 0, -1, -1);

    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    if (llama_decode(ctx, batch) != 0) {
        return 0;
    }

    // commit
    float* embedding = llama_get_embeddings_seq(ctx, 0);
    if (!embedding || splinter_set_embedding(key, embedding) != 0) {
        return 0;
    }

    // splinter_set_embedding() advances the slot epoch by exactly 2
    // (even -> odd -> even). Return THAT post-write epoch so the caller records
    // it: otherwise the caller stores the pre-write epoch, the slot now reads
    // two higher, and every subsequent pulse re-embeds this key forever (also
    // re-pulsing __lane_dw_2). If the epoch isn't exactly current_epoch + 2, a
    // content writer raced us during decode and our embedding is already stale —
    // return 0 so the next scan re-embeds against the newest content.
    uint64_t post_epoch = splinter_get_epoch(key);
    if (post_epoch != current_epoch + 2) {
        return 0;
    }
    return post_epoch;
}

void perform_backfill(llama_context* ctx, const llama_vocab* vocab) {
    std::cout << "Starting backfill for VARTEXT keys...\n";

    // This is the SEQUENTIAL sweep the thesis names. Re-bid as SEQUENTIAL for a
    // long window, then issue a non-blocking advisement: if a live completer
    // holds sovereignty we simply proceed without forcing our advice (deferring
    // is correct; backfill is latency-tolerant and must not fight WILLNEED).
    splinter_shard_rebid(SHARD_ID, SPL_INTENT_SEQUENTIAL, SHARD_PRIO_BACKFILL, SHARD_DUR_BACKFILL);
    splinter_madvise(SHARD_ID, NULL, 0, POSIX_MADV_SEQUENTIAL, /*timeout*/0);

    std::vector<char*> keys(dynamic_key_capacity());
    size_t key_count = 0;
    if (splinter_list(keys.data(), keys.size(), &key_count) != 0) return;

    size_t embedded = 0, failed = 0;
    for (size_t i = 0; i < key_count; ++i) {
        splinter_slot_snapshot_t snap = {};
        if (splinter_get_slot_snapshot(keys[i], &snap) != 0) continue;

        // only process if it's explicitly VARTEXT and hasn't been embedded yet
        if ((snap.type_flag & SPL_SLOT_TYPE_VARTEXT) && needs_embedding(snap.embedding, SPLINTER_EMBED_DIM)) {
            std::cout << "Backfilling: " << keys[i] << "..." << std::flush;
            // process_key() returns 0 on any failure/skip (no embedding produced,
            // epoch raced, decode failed). Report the real outcome rather than
            // assuming success.
            if (process_key(keys[i], ctx, vocab) != 0) {
                ++embedded;
                std::cout << " ok\n";
            } else {
                ++failed;
                std::cout << " FAILED (no vector written)\n";
            }
        }
    }
    std::cout << "Backfill complete: " << embedded << " embedded, "
              << failed << " failed.\n";
}

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0;
    }
}

int main(int argc, char **argv) {
    size_t key_count = -1;
    // Sized to the store's geometry once the bus is open (see below).
    std::vector<char*> keys;

    // we need at least 4 arguments: <bin> <bus> <model> <group>
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " [--backfill-text-keys] [--oneshot] <bus_name> <path_to_nomic_gguf> <signal_group_id>\n";
        return 1;
    }

    bool backfill = false;
    bool oneshot = false;
    std::vector<char*> positionals;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--backfill-text-keys") {
            backfill = true;
        } else if (arg == "--oneshot") {
            oneshot = true;
        } else {
            positionals.push_back(argv[i]);
        }
    }

    // make sure we have required positional arguments
    if (positionals.size() < 3) {
        std::cerr << "Error: Missing required positional arguments (bus_name, model_path, signal_group_id)\n";
        return 1;
    }

    const char* bus_name = positionals[0];
    const char* model_path = positionals[1];
    
    // std::stoi is safe here as we've confirmed the positional exists
    uint8_t signal_group = 0;
    try {
        signal_group = static_cast<uint8_t>(std::stoi(positionals[2]));
    } catch (...) {
        std::cerr << "Error: signal_group_id must be a number.\n";
        return 1;
    }

    if (signal_group >= SPLINTER_MAX_GROUPS) {
        std::cerr << "Invalid signal group. Must be 0-" << (SPLINTER_MAX_GROUPS - 1) << ".\n";
        return 1;
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    if (splinter_open(bus_name) != 0) {
        std::cerr << "Failed to connect to Splinter bus: " << bus_name << "\n";
        return 1;
    }

    // Now that the bus is open, size the keys[] array to the store's geometry.
    keys.resize(dynamic_key_capacity());

    // Register as a cooperating Logic Shard: WILLNEED at a low priority with a
    // short window so the live completer (higher priority) always preempts us.
    if (splinter_shard_claim(SHARD_ID, SPL_INTENT_WILLNEED, SHARD_PRIO_LIVE, SHARD_DUR_LIVE) != 0) {
        std::cerr << "[Warn]: could not claim a shard bid slot (table full?); "
                  << "continuing without cooperative advisement.\n";
    }

    // Map the "embed this" convention label (bit 0 / 0x1) to our signal group
    // so that splinter_bump_slot() on a key with this label wakes us up.
    splinter_watch_label_register(1ULL, signal_group);

    std::cout << "[Startup]: Loading GGUF model `" << model_path << "` (this may take a moment) ...\n";
    llama_backend_init();

    llama_log_set([](ggml_log_level level, const char * text, void * user_data) {
        (void) user_data;
        
        if (level == GGML_LOG_LEVEL_ERROR) {
            fputs(text, stderr);
            fflush(stderr);
        }
    }, nullptr);

    llama_model_params model_params = llama_model_default_params();
    llama_model *model = llama_model_load_from_file(model_path, model_params);
    if (!model) {
        std::cerr << "Failed to load model.\n";
        return 1;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.embeddings = true;
    // Force mean pooling. Without this, pooling_type stays UNSPECIFIED and falls
    // back to whatever the GGUF declares; embedding GGUFs that omit the pooling
    // metadata resolve to NONE, and llama_get_embeddings_seq() then returns NULL
    // (see llama.h) — so process_key() silently writes nothing. Nomic embedding
    // models use mean pooling, so pin it explicitly.
    ctx_params.pooling_type = LLAMA_POOLING_TYPE_MEAN;
    // Match the context window the model was trained with so long VARTEXT
    // values aren't silently truncated at the llama default of 512 tokens.
    // Embeddings need the full sequence in one batch, so n_batch/n_ubatch
    // track n_ctx as well.
    const int n_ctx_train = llama_model_n_ctx_train(model);
    ctx_params.n_ctx    = n_ctx_train;
    ctx_params.n_batch  = n_ctx_train;
    ctx_params.n_ubatch = n_ctx_train;
    std::cout << "[Startup]: Model trained context = " << n_ctx_train << " tokens.\n";
    llama_context *ctx = llama_init_from_model(model, ctx_params);
    const llama_vocab *vocab = llama_model_get_vocab(model);

    uint64_t last_signal_count = splinter_get_signal_count(signal_group);
    std::unordered_map<std::string, uint64_t> processed_epochs;

    if (backfill) {
        perform_backfill(ctx, vocab);
    }

    if (oneshot) {
        splinter_shard_release(SHARD_ID);
        splinter_close();
        return 0;
    }

    if (processed_epochs.empty()) {
        std::cout << "[Startup]: Cold start detected; populating baseline epochs ...\n";
        size_t pending = 0;
        splinter_list(keys.data(), keys.size(), &key_count);
        for (size_t i = 0; i < key_count; ++i) {
            std::string key_str(keys[i]);

            std::cout << "[Startup]: Checking key: `" << keys[i] << "` ...\n" << std::flush;

            splinter_slot_snapshot_t snap = {};
            if (splinter_get_slot_snapshot(keys[i], &snap) != 0) continue;
            if (snap.epoch == 0) continue;

            // Only baseline keys that already carry a vector. A key that still
            // needs an embedding must NOT be recorded here: doing so marks it as
            // "already processed" at its current epoch, so the live loop's
            // `processed >= current` guard skips it forever and it never gets
            // embedded unless its epoch changes again. Leaving it out keeps it
            // pending so the next pulse to this group embeds it.
            if (needs_embedding(snap.embedding, SPLINTER_EMBED_DIM)) {
                ++pending;
                continue;
            }
            processed_epochs[key_str] = snap.epoch;
        }
        if (pending > 0) {
            std::cout << "[Startup]: " << pending << " key(s) still need embedding; "
                      << "they will be embedded on the next pulse to group "
                      << (int)signal_group << " (or run with --backfill-text-keys).\n";
        }
    }

    last_signal_count = splinter_get_signal_count(signal_group);

    std::cout << "[Active]: Last signal count on group " << (int)signal_group;
    std::cout << " was " << last_signal_count << ".\n" << std::flush;


    // main inference event loop
    while (keep_running) {
        uint64_t current_signal_count = splinter_get_signal_count(signal_group);
        
        if (current_signal_count == last_signal_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::cout << "[Pulse Received]: Signal Count (" << current_signal_count << ") scanning bus for changed epochs ...\n";

        // Re-bid to refresh our short WILLNEED window ("WILLNEED with a short
        // window, re-bid frequently"), then a single non-blocking advisement to
        // warm the arena. Non-blocking: if a completer is sovereign we defer.
        splinter_shard_rebid(SHARD_ID, SPL_INTENT_WILLNEED, SHARD_PRIO_LIVE, SHARD_DUR_LIVE);
        splinter_madvise(SHARD_ID, NULL, 0, POSIX_MADV_WILLNEED, /*timeout*/0);

        key_count = 0;
        
        if (splinter_list(keys.data(), keys.size(), &key_count) != 0) continue;
        for (size_t i = 0; i < key_count; ++i) {
            std::string key_str(keys[i]);
            uint64_t current_epoch = splinter_get_epoch(keys[i]);

            auto it = processed_epochs.find(key_str);
            if (it != processed_epochs.end() && it->second >= current_epoch) {
                continue;
            }

            uint64_t tick_start = splinter_now();
            uint64_t observed_epoch = process_key(keys[i], ctx, vocab);  // 0 = failed/skipped
            if (observed_epoch != 0) {
                auto duration = std::chrono::system_clock::now().time_since_epoch();
                uint64_t unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
                int64_t tick_end = splinter_now();
                size_t processing_delta = static_cast<size_t>(tick_end - tick_start);
                splinter_set_slot_time(keys[i], SPL_TIME_CTIME, unix_timestamp, processing_delta);
                processed_epochs[key_str] = observed_epoch;  // post-write epoch from process_key (slot's current even epoch)
                // The vector is now committed (process_key confirmed the +2 epoch),
                // so clear the client's WAITING label: this is the documented
                // "sidecar clears WAITING" transition and the signal the scorer
                // waits on to know its own deposit — not residue from a reused
                // slot — was embedded. Harmless no-op for keys never labelled
                // WAITING (e.g. backfilled corpus keys).
                splinter_unset_label(keys[i], WAITING_LABEL);
                // TODO - make this command line set-able (pulse after update)
                splinter_pulse_keygroup("__lane_dw_2");
                std::cout << "[Processed]: Key " << keys[i] << " embedded after update.\n" << std::flush;
            }
        }
        last_signal_count = current_signal_count;
    }

    std::cout << "\n[Signal Received]: Shutting down splinference daemon safely...\n";

    splinter_shard_release(SHARD_ID);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    splinter_close();

    return 0;
}
