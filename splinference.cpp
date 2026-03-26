/**
 * Sidecar inference daemon for libsplinter.
 * Listens to a Splinter signal group, computes embeddings for modified keys
 * using llama.cpp (Nomic Text v2 (Quantized, 1.3B params)), and writes the 
 * 768-d vector back to the slot.
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

#ifndef SPLINTER_EMBEDDINGS
#define SPLINTER_EMBEDDINGS
#endif

// Now Splinter / llama 
#include "splinter.h"
#include "llama-cpp.h"

volatile sig_atomic_t keep_running = 1;

// Helper to check if a vector is zeroed out
bool needs_embedding(const float* vec, size_t len) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) sum += vec[i] * vec[i];
    return std::sqrt(sum) < 1e-6; // Effectively zero
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
    // We need at least 4 arguments: <bin> <bus> <model> <group>
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " [--backfill-text-keys] [--oneshot] <bus_name> <path_to_nomic_gguf> <signal_group_id>\n";
        return 1;
    }

    bool backfill = false;
    bool oneshot = false;
    std::vector<char*> positionals;

    // Robust parsing: Separate flags from positional arguments
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

    // Ensure we have exactly the 3 required positional arguments
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

    if (oneshot) {
        splinter_close();
        return 0;
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

            // just continue if already processed:
            if (processed_epochs[keys[i]] >= current_epoch) continue;

            // If we've never seen this key, or its epoch increased, it needs processing.
            if (processed_epochs.find(key_str) == processed_epochs.end() || 
                processed_epochs[key_str] < current_epoch) {
                
                if (process_key(keys[i], ctx, vocab)) {
                    // Update tracker with the epoch created by our write
                    processed_epochs[key_str] = splinter_get_epoch(keys[i]);
                    
                    char lane_name[32]; 
                    std::snprintf(lane_name, sizeof(lane_name), "__lane_dw_%u", signal_group);
                    
                    // Grab internal tick waypoint (for duration/latency)
                    int64_t tick_start = splinter_now(); 

                    // Wake up the watchers
                    splinter_pulse_keygroup(lane_name);
                                        
                    // Grab Wall-Clock Unix Epoch (for TTL/LRU)
                    auto duration = std::chrono::system_clock::now().time_since_epoch();
                    uint64_t unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
                    
                    // Grab end tick to calculate delta
                    int64_t tick_end = splinter_now();
                    
                    // Calculate delta and cast to size_t for the metadata
                    size_t processing_delta = static_cast<size_t>(tick_end - tick_start);

                    // Commit: Set the Access/Creation time and the latency delta
                    splinter_set_slot_time(keys[i], SPL_TIME_CTIME, unix_timestamp, processing_delta);
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