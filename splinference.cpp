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
 *  - Uncertainty & Confidence Logging (High-entropy low-probability lysis 
 *    events) for governance. (High Priority)
 *
 *  - Implement argparse (see 3rdparty/) for argument parsing. (Low Priority)
 * 
 *  - Dynamically allocate keys[] array in main() based on the store's 
 *    geometry. (High Priority)
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

volatile sig_atomic_t keep_running = 1;

// Helper to check if a vector is zeroed out
bool needs_embedding(const float* vec, size_t len) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) sum += vec[i] * vec[i];
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
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    if (llama_decode(ctx, batch) != 0) {
        return 0;
    }

    // commit
    float* embedding = llama_get_embeddings_seq(ctx, 0);
    if (embedding && splinter_set_embedding(key, embedding) == 0) {
        return current_epoch;
    }

    return 0;
}

void perform_backfill(llama_context* ctx, const llama_vocab* vocab) {
    std::cout << "Starting backfill for VARTEXT keys...\n";
    
    char *keys[1024];
    size_t key_count = 0;
    if (splinter_list(keys, 1024, &key_count) != 0) return;

    for (size_t i = 0; i < key_count; ++i) {
        splinter_slot_snapshot_t snap = {};
        if (splinter_get_slot_snapshot(keys[i], &snap) != 0) continue;

        // only process if it's explicitly VARTEXT and hasn't been embedded yet
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
    size_t key_count = -1;
    char *keys[1024] = { 0 };

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
    llama_context *ctx = llama_init_from_model(model, ctx_params);
    const llama_vocab *vocab = llama_model_get_vocab(model);

    uint64_t last_signal_count = splinter_get_signal_count(signal_group);
    std::unordered_map<std::string, uint64_t> processed_epochs;

    if (backfill) {
        perform_backfill(ctx, vocab);
    }

    if (oneshot) {
        splinter_close();
        return 0;
    }

    if (processed_epochs.empty()) {
        std::cout << "[Startup]: Cold start detected; populating baseline epochs ...\n";
        splinter_list(keys, 1024, &key_count);
        for (size_t i = 0; i < key_count; ++i) {
            std::string key_str(keys[i]);
            uint64_t current_epoch = splinter_get_epoch(keys[i]);
            
            std::cout << "[Startup]: Checking key: `" << keys[i] << "` ...\n" << std::flush;
            
            auto it = processed_epochs.find(key_str);
            if (it != processed_epochs.end() && it->second >= current_epoch) {
                continue;
            }

            uint64_t observed_epoch = splinter_get_epoch(keys[i]);
            if (observed_epoch != 0) {
                processed_epochs[key_str] = observed_epoch;
            }
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

        key_count = 0;
        
        if (splinter_list(keys, 1024, &key_count) != 0) continue;
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
                processed_epochs[key_str] = observed_epoch;  // The epoch process_key captured, pre-write
                // TODO - make this command line set-able (pulse after update)
                splinter_pulse_keygroup("__lane_dw_2");
                std::cout << "[Processed]: Key " << keys[i] << " embedded after update.\n" << std::flush;
            }
        }
        last_signal_count = current_signal_count;
    }

    std::cout << "\n[Signal Received]: Shutting down splinference daemon safely...\n";

    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    splinter_close();

    return 0;
}
