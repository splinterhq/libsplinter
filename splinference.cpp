/**
 * Sidecar inference daemon for libsplinter.
 * Listens to a Splinter signal group, computes embeddings for modified keys
 * using llama.cpp (Nomic Text v2 (Quantized, 1.3B params)), and writes the 
 * 768-d vector back to the slot.
 */

#include <atomic>
#include <math.h>

// Bridge the C/C++ atomic divide before including the C header
using atomic_uint_least64_t = std::atomic_uint_least64_t;
using atomic_uint_least32_t = std::atomic_uint_least32_t;
using atomic_uint_least8_t  = std::atomic_uint_least8_t;

#include "splinter.h"
#include "llama.h"

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <csignal>
#include <chrono>
#include <thread>

volatile sig_atomic_t keep_running = 1;

// Helper to check if a vector is zeroed out
bool needs_embedding(const float* vec, size_t len) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) sum += vec[i] * vec[i];
    return std::sqrt(sum) < 1e-6; // Effectively zero
}

void record_tokens(const std::vector<llama_token>& tokens) {
    size_t len = 0;
    // Get the raw pointer to the reserved 2k slot
    uint64_t* table = (uint64_t*)splinter_get_raw_ptr("__sys_res_gp", &len, nullptr);
    if (!table || len < 1600) return;

    for (auto t : tokens) {
       // this is top-100 max so .. it doesn't have to be supersonic
        uint64_t tid = (uint64_t)t;
        bool found = false;
        for (int i = 0; i < 100; i++) {
            // table[i*2] is the ID, table[i*2 + 1] is the Count
            if (table[i*2] == tid) {
                __atomic_fetch_add(&table[i*2 + 1], 1, __ATOMIC_RELAXED);
                found = true;
                break;
            }
            if (table[i*2] == 0) {
                uint64_t expected = 0;
                if (__atomic_compare_exchange_n(&table[i*2], &expected, tid, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
                    __atomic_fetch_add(&table[i*2 + 1], 1, __ATOMIC_RELAXED);
                    found = true;
                    break;
                }
            }
        }
        if (! found) {
            // we're overflowing tokens at this point
            std::cout << "The top-100 log table is full - tokens are being dropped from logit scope.\n";
        }
    }
}

bool process_key(const char* key, llama_context* ctx, const llama_vocab* vocab) {
    size_t val_len = 0;
    uint64_t current_epoch = splinter_get_epoch(key);

    // odd epochs mean busy writers
    if (current_epoch & 1) return false;

    const void* raw_ptr = splinter_get_raw_ptr(key, &val_len, nullptr);
    if (!raw_ptr || val_len == 0) return false;

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
        return false;
    }

    // inference
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    if (llama_decode(ctx, batch) != 0) {
        return false;
    }

    // commit
    float* embedding = llama_get_embeddings_seq(ctx, 0);
    if (embedding && splinter_set_embedding(key, embedding) == 0) {
        return true;
    }

    return false;
}

void perform_backfill(llama_context* ctx, const llama_vocab* vocab) {
    std::cout << "Starting backfill for VARTEXT keys...\n";
    
    char *keys[1024];
    size_t key_count = 0;
    if (splinter_list(keys, 1024, &key_count) != 0) return;

    for (size_t i = 0; i < key_count; ++i) {
        splinter_slot_snapshot_t snap = {};
        if (splinter_get_slot_snapshot(keys[i], &snap) != 0) continue;

        // Only process if it's explicitly VARTEXT and hasn't been embedded yet
        if ((snap.type_flag & SPL_SLOT_TYPE_VARTEXT) && needs_embedding(snap.embedding, SPLINTER_EMBED_DIM)) {
            std::cout << "Backfilling: " << keys[i] << "...\n";
            process_key(keys[i], ctx, vocab);
        }
    }
    std::cout << "Backfill complete.\n";
}

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0;
    }
}

int main(int argc, char **argv) {
if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " [--backfill-text-keys] <bus_name> <path_to_nomic_gguf> <signal_group_id>\n";
        return 1;
    }

    bool backfill = false;
    int arg_offset = 1;

    if (std::string(argv[1]) == "--backfill-text-keys") {
        backfill = true;
        arg_offset = 2; // shift
        
        // Ensure we still have the 3 required positional arguments after the flag
        if (argc < 5) {
            std::cerr << "Error: Missing required arguments after --backfill-text-keys\n";
            return 1;
        }
    }

    const char* bus_name = argv[arg_offset];
    const char* model_path = argv[arg_offset + 1];
    uint8_t signal_group = static_cast<uint8_t>(std::stoi(argv[arg_offset + 2]));

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

    std::cout << "Loading model (this may take a moment)...\n";
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
    llama_context *ctx = llama_init_from_model(model, ctx_params);
    const llama_vocab *vocab = llama_model_get_vocab(model);

    if (backfill) {
        perform_backfill(ctx, vocab);
    }

    std::cout << "Daemon active. Listening on signal group " << (int)signal_group << "...\n";

    // state tracking
    std::unordered_map<std::string, uint64_t> processed_epochs;
    uint64_t last_signal_count = splinter_get_signal_count(signal_group);

    // main inference event loop
    while (keep_running) {
        uint64_t current_signal_count = splinter_get_signal_count(signal_group);
        
        // if the atomic counter hasn't bumped, sleep and yield the core
        if (current_signal_count == last_signal_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        last_signal_count = current_signal_count;
        std::cout << "Pulse received! Scanning bus for changed epochs...\n";

        // Grab a list of all active keys
        char *keys[1024]; 
        size_t key_count = 0;
        if (splinter_list(keys, 1024, &key_count) != 0) continue;

        for (size_t i = 0; i < key_count; ++i) {
            std::string key_str(keys[i]);
            uint64_t current_epoch = splinter_get_epoch(keys[i]);

            // If we've never seen this key, or its epoch increased, it needs processing.
            if (processed_epochs.find(key_str) == processed_epochs.end() || 
                processed_epochs[key_str] < current_epoch) {
                
                // FIX: Use 'ctx' and 'vocab' (the variables), NOT the types.
                if (process_key(keys[i], ctx, vocab)) {
                    // Update tracker with the NEW epoch created by our write
                    processed_epochs[key_str] = splinter_get_epoch(keys[i]);
                }
            }
        }

    }

    std::cout << "\nShutting down splinference daemon safely...\n";
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    splinter_close();

    return 0;
}