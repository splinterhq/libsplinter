/**
 * Splainference - Completion sidecar daemon for libsplinter.
 *
 * Monitors a Splinter signal group for keys labeled inference-waiting,
 * performs streaming text completion using llama.cpp, and writes tokens
 * back to the originating slot via splinter_append() as they are decoded.
 *
 * Label lifecycle for a UUID key:
 *   0x1000000000000000  inference-waiting   (client posts, bumps)
 *   0x2000000000000000  inference-servicing (we set before first token)
 *   0x4000000000000000  inference-ready     (we set on completion)
 *
 * Labels 0x2 and 0x1 are cleared as the state advances. Governance
 * observes the bloom directly; it does not need to poll epochs.
 *
 * Copyright (C) 2026 Tim Post <timthepost@protonmail.com>
 * License: Apache 2
 *
 * TODO:
 *   - argparse (see 3rdparty/) for cleaner argument handling
 *   - Per-slot context window reuse (keep KV cache warm between appends
 *     for the same UUID if the slot hasn't been unset)
 *   - Configurable sampler parameters via bus keys (temp, top-p, etc.)
 *   - Multi-group threading (one completer thread per signal group)
 */

#include <atomic>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <csignal>
#include <chrono>
#include <thread>
#include <cstring>

using atomic_uint_least64_t = std::atomic_uint_least64_t;
using atomic_uint_least32_t = std::atomic_uint_least32_t;
using atomic_uint_least8_t  = std::atomic_uint_least8_t;

#ifndef SPLINTER_EMBEDDINGS
#define SPLINTER_EMBEDDINGS
#endif

#include "splinter.h"
#include "llama-cpp.h"

// We use the last 3 labels since the highest quadrant
// is down by one because the last one is reserved. This
// is perfect for a "trifecta" and uses the odd group.
#define SPLAIN_LABEL_WAITING    0x1000000000000000ULL
#define SPLAIN_LABEL_SERVICING  0x2000000000000000ULL
#define SPLAIN_LABEL_READY      0x4000000000000000ULL

// How many tokens to accumulate before flushing to the bus.
// Word-boundary mode means we flush when a decoded piece starts with
// a space (BPE word boundary signal) or contains a newline.
// This value is the fallback maximum if no boundary is seen.
#define SPLAIN_TOKEN_FLUSH_MAX  8

volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) keep_running = 0;
}

static void debug_post(const std::string &msg) {
    const std::string line = msg + "\n";
    // Try append first; if key doesn't exist yet, set it.
    if (splinter_append("__debug", line.c_str(), line.size(), nullptr) != 0) {
        splinter_set("__debug", line.c_str(), line.size());
    }
}

static bool is_word_boundary(const std::string &piece) {
    if (piece.empty()) return false;
    if (piece[0] == ' ' || piece[0] == '\n') return true;
    for (char c : piece) {
        if (c == '\n') return true;
    }
    return false;
}

static std::string build_prompt(llama_model *model, const std::string &user_msg) {
    const char *tmpl = llama_model_chat_template(model, nullptr);

    if (!tmpl) {
        debug_post("[splainference][WARN]: No chat template found, using bare fallback.");
        return "<user>\n" + user_msg + "\n<assistant>\n";
    }

    // Build a single-turn conversation
    llama_chat_message messages[1];
    messages[0].role    = "user";
    messages[0].content = user_msg.c_str();

    // First call: measure required buffer size
    int required = llama_chat_apply_template(tmpl, messages, 1, true, nullptr, 0);
    if (required < 0) {
        debug_post("[splainference][WARN]: llama_chat_apply_template sizing failed, using bare fallback.");
        return "<user>\n" + user_msg + "\n<assistant>\n";
    }

    std::vector<char> buf(required + 1, 0);
    llama_chat_apply_template(tmpl, messages, 1, true, buf.data(), buf.size());
    return std::string(buf.data());
}


// process_completion: the core work unit.
//
// 1. Reads the key's current value as the user prompt.
// 2. Applies the chat template.
// 3. Tokenizes and runs the decode loop, appending at word boundaries.
// 4. Advances the label state machine throughout.
// 5. Writes timing to ctime via splinter_set_slot_time.
//
// Returns the epoch we processed at (non-zero = success), 0 = skip/fail.
static uint64_t process_completion(
    const char      *key,
    llama_model     *model,
    llama_context   *ctx,
    const llama_vocab *vocab,
    int              n_threads)
{
    (void) n_threads;
    // --- Epoch consistency check before we touch anything ---
    uint64_t start_epoch = splinter_get_epoch(key);
    if (start_epoch & 1) {
        debug_post(std::string("[splainference][SKIP]: ") + key + " has odd epoch (writer active).");
        return 0;
    }

    size_t   val_len   = 0;
    uint64_t ptr_epoch = 0;
    const void *raw = splinter_get_raw_ptr(key, &val_len, &ptr_epoch);

    if (!raw || val_len == 0) {
        debug_post(std::string("[splainference][SKIP]: ") + key + " is empty.");
        return 0;
    }
    if (ptr_epoch != start_epoch) {
        debug_post(std::string("[splainference][SKIP]: ") + key + " epoch shifted during read.");
        return 0;
    }

    std::string user_msg(static_cast<const char*>(raw), val_len);
    std::string prompt = build_prompt(model, user_msg);

    debug_post(std::string("[splainference][START]: Processing key: ") + key);

    // --- Label transition: waiting -> servicing ---
    splinter_unset_label(key, SPLAIN_LABEL_WAITING);
    splinter_set_label(key,   SPLAIN_LABEL_SERVICING);
    splinter_bump_slot(key);

    // --- Tokenize the prompt ---
    std::vector<llama_token> tokens(prompt.size() + 16);
    int n_tokens = llama_tokenize(
        vocab,
        prompt.c_str(), prompt.size(),
        tokens.data(), tokens.size(),
        true,  // add_special (BOS)
        true   // parse_special
    );

    if (n_tokens < 0) {
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(
            vocab,
            prompt.c_str(), prompt.size(),
            tokens.data(), tokens.size(),
            true, true
        );
    }

    if (n_tokens <= 0) {
        debug_post(std::string("[splainference][ERROR]: Tokenization failed for key: ") + key);
        splinter_unset_label(key, SPLAIN_LABEL_SERVICING);
        splinter_set_label(key,   SPLAIN_LABEL_READY); // mark done even on failure
        splinter_bump_slot(key);
        return 0;
    }

    tokens.resize(n_tokens);

    //  Overwrite the slot with the formatted prompt so the client
    //  can see the full exchange including the assistant prefix.
    //  Completion tokens will then be appended after this.
    splinter_set(key, prompt.c_str(), prompt.size());

    // Build sampler chain
    llama_sampler *slotSampler = llama_sampler_chain_init(llama_sampler_chain_default_params());

    // Add individual samplers
    llama_sampler_chain_add(slotSampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(slotSampler, llama_sampler_init_temp(0.7f));

    // To select a token based on the final distribution:
    llama_sampler_chain_add(slotSampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // Decode loop
    uint64_t tick_start = splinter_now();

    // Prefill the prompt
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    if (llama_decode(ctx, batch) != 0) {
        debug_post(std::string("[splainference][ERROR]: Prefill decode failed for key: ") + key);
        llama_sampler_free(slotSampler);
        splinter_unset_label(key, SPLAIN_LABEL_SERVICING);
        splinter_set_label(key,   SPLAIN_LABEL_READY);
        splinter_bump_slot(key);
        return 0;
    }

    // Get slot geometry so we know when to stop appending
    splinter_header_snapshot_t hdr = {};
    splinter_get_header_snapshot(&hdr);
    const size_t max_val = hdr.max_val_sz;

    std::string   chunk_buf;       // accumulates pieces until flush
    size_t        written   = prompt.size(); // bytes already in the slot
    int           token_run = 0;   // fallback flush counter
    bool          oom       = false;

    while (keep_running) {
        llama_token new_token = llama_sampler_sample(slotSampler, ctx, -1);

        // EOS / EOG -> done
        if (llama_vocab_is_eog(vocab, new_token)) break;

        // Decode token to text piece
        char   piece_buf[256] = {0};
        int    piece_len = llama_token_to_piece(vocab, new_token, piece_buf, sizeof(piece_buf), 0, true);
        if (piece_len < 0) {
            // Buffer too small (shouldn't happen at 256 for BPE pieces)
            debug_post(std::string("[splainference][WARN]: piece_buf overflow on key: ") + key);
            break;
        }

        std::string piece(piece_buf, piece_len);
        chunk_buf += piece;
        token_run++;

        // Accept the token into the KV cache
        llama_batch single = llama_batch_get_one(&new_token, 1);
        if (llama_decode(ctx, single) != 0) {
            debug_post(std::string("[splainference][WARN]: Decode step failed, stopping early: ") + key);
            break;
        }

        // Flush at word boundary or fallback max
        bool flush = is_word_boundary(piece) || (token_run >= SPLAIN_TOKEN_FLUSH_MAX);

        if (flush && !chunk_buf.empty()) {
            if (written + chunk_buf.size() > max_val) {
                debug_post(std::string("[splainference][WARN]: Slot full, truncating completion: ") + key);
                // Write as much as fits
                size_t remaining = max_val - written;
                if (remaining > 0) {
                    splinter_append(key, chunk_buf.c_str(), remaining, nullptr);
                }
                oom = true;
                break;
            }

            size_t new_len = 0;
            if (splinter_append(key, chunk_buf.c_str(), chunk_buf.size(), &new_len) != 0) {
                debug_post(std::string("[splainference][WARN]: splinter_append failed on key: ") + key);
                break;
            }

            written   = new_len;
            chunk_buf.clear();
            token_run = 0;
        }
    }

    // Flush any remaining partial chunk
    if (!chunk_buf.empty() && !oom) {
        size_t remaining = max_val - written;
        size_t to_write  = std::min(chunk_buf.size(), remaining);
        if (to_write > 0) {
            splinter_append(key, chunk_buf.c_str(), to_write, nullptr);
        }
    }

    llama_sampler_free(slotSampler);

    // Get and free the memory object from the context along with metadata ((true) in clear).
    llama_memory_t mem = llama_get_memory(ctx);
    llama_memory_clear(mem, true);
    
    // Timing backfill
    uint64_t tick_end = splinter_now();
    size_t processing_delta = static_cast<size_t>(tick_end - tick_start);
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    uint64_t unix_ts = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    splinter_set_slot_time(key, SPL_TIME_CTIME, unix_ts, processing_delta);

    // Label transition: servicing -> ready
    splinter_unset_label(key, SPLAIN_LABEL_SERVICING);
    splinter_set_label(key,   SPLAIN_LABEL_READY);
    splinter_bump_slot(key);

    debug_post(std::string("[splainference][DONE]: Completion written to key: ") + key);

    return start_epoch;
}

struct WaitingCollector {
    std::vector<std::string> keys;
};

static void collect_waiting(const char *key, uint64_t /*epoch*/, void *user_data) {
    auto *wc = static_cast<WaitingCollector*>(user_data);
    wc->keys.emplace_back(key);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0]
                  << " [--oneshot] <bus_name> <path_to_gguf> <signal_group_id>\n";
        return 1;
    }

    bool oneshot = false;
    std::vector<char*> positionals;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--oneshot") {
            oneshot = true;
        } else {
            positionals.push_back(argv[i]);
        }
    }

    if (positionals.size() < 3) {
        std::cerr << "Error: Missing required positional arguments.\n";
        return 1;
    }

    const char *bus_name   = positionals[0];
    const char *model_path = positionals[1];

    uint8_t signal_group = 0;
    try {
        signal_group = static_cast<uint8_t>(std::stoi(positionals[2]));
    } catch (...) {
        std::cerr << "Error: signal_group_id must be a number 0-63.\n";
        return 1;
    }

    if (signal_group >= SPLINTER_MAX_GROUPS) {
        std::cerr << "Error: signal_group_id must be 0-" << (SPLINTER_MAX_GROUPS - 1) << ".\n";
        return 1;
    }

    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    if (splinter_open(bus_name) != 0) {
        std::cerr << "Failed to connect to Splinter bus: " << bus_name << "\n";
        return 1;
    }

    std::cout << "[Startup]: Loading GGUF model `" << model_path << "` ...\n";
    llama_backend_init();

    // Silence llama.cpp noise except errors
    llama_log_set([](ggml_log_level level, const char *text, void*) {
        if (level == GGML_LOG_LEVEL_ERROR) {
            fputs(text, stderr);
            fflush(stderr);
        }
    }, nullptr);

    llama_model_params model_params  = llama_model_default_params();
    llama_model       *model         = llama_model_load_from_file(model_path, model_params);
    if (!model) {
        std::cerr << "Failed to load model from: " << model_path << "\n";
        return 1;
    }

    llama_context_params ctx_params  = llama_context_default_params();
    ctx_params.embeddings            = false; // completion, not embedding
    ctx_params.n_threads             = 4;     // sensible default for i3 Tiger Lake
    ctx_params.n_threads_batch       = 4;

    llama_context    *ctx   = llama_init_from_model(model, ctx_params);
    const llama_vocab *vocab = llama_model_get_vocab(model);

    if (!ctx) {
        std::cerr << "Failed to create llama context.\n";
        llama_model_free(model);
        return 1;
    }

    // Check for any keys already labeled waiting at startup (cold-start backfill)
    {
        WaitingCollector wc;
        splinter_enumerate_matches(SPLAIN_LABEL_WAITING, collect_waiting, &wc);
        if (!wc.keys.empty()) {
            std::cout << "[Startup]: " << wc.keys.size() << " waiting key(s) found at cold start.\n";
            for (const auto &k : wc.keys) {
                process_completion(k.c_str(), model, ctx, vocab, ctx_params.n_threads);
            }
        }
    }

    if (oneshot) {
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        splinter_close();
        return 0;
    }

    uint64_t last_signal_count = splinter_get_signal_count(signal_group);

    std::cout << "[Active]: Watching signal group " << (int)signal_group
              << " (count: " << last_signal_count << ")\n" << std::flush;

    debug_post("[splainference][Active]: Completion daemon online.");

    // --- Main event loop ---
    while (keep_running) {
        uint64_t current_signal_count = splinter_get_signal_count(signal_group);

        if (current_signal_count == last_signal_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::cout << "[Pulse]: Signal count " << current_signal_count
                  << ", scanning for waiting keys...\n" << std::flush;

        // Use bloom enumeration rather than a full key scan —
        // only slots labeled inference-waiting will be visited.
        WaitingCollector wc;
        splinter_enumerate_matches(SPLAIN_LABEL_WAITING, collect_waiting, &wc);

        for (const auto &key : wc.keys) {
            if (!keep_running) break;
            process_completion(key.c_str(), model, ctx, vocab, ctx_params.n_threads);
        }

        last_signal_count = current_signal_count;
    }

    std::cout << "\n[Signal]: Shutting down splainference safely...\n";
    debug_post("[splainference]: Daemon shutting down.");

    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    splinter_close();

    return 0;
}
