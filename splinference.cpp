/**
 * Sidecar inference daemon for libsplinter.
 * Listens to a Splinter signal group, computes embeddings for modified keys
 * using llama.cpp (Nomic Text v2 (Quantized, 1.3B params)), and writes the 
 * 768-d vector back to the slot.
 */

#include <atomic>
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

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0;
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <bus_name> <path_to_nomic_gguf> <signal_group_id>\n";
        return 1;
    }

    const char* bus_name = argv[1];
    const char* model_path = argv[2];
    uint8_t signal_group = static_cast<uint8_t>(std::stoi(argv[3]));

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

            // skip if a writer is currently locking the slot (odd epoch)
            if (current_epoch & 1) continue; 

            // if we've never seen this key, or its epoch increased, it needs processed.
            if (processed_epochs.find(key_str) == processed_epochs.end() || 
                processed_epochs[key_str] < current_epoch) {
                
                size_t val_len = 0;
                const void *raw_ptr = splinter_get_raw_ptr(keys[i], &val_len, nullptr);
                
                if (raw_ptr && val_len > 0) {
                    std::vector<llama_token> tokens(val_len + 8); 
                    int n_tokens = llama_tokenize(vocab, static_cast<const char*>(raw_ptr), val_len, tokens.data(), tokens.size(), true, false);

                    if (n_tokens < 0) {
                        tokens.resize(-n_tokens);
                        n_tokens = llama_tokenize(vocab, static_cast<const char*>(raw_ptr), val_len, tokens.data(), tokens.size(), true, false);
                    }

                    // verify seqlock hasn't torn during tokenization
                    if (splinter_get_epoch(keys[i]) != current_epoch) {
                        std::cerr << "Torn read on " << key_str << ", skipping.\n";
                        continue;
                    }

                    // finally, run batch for whatever changed
                    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
                    if (llama_decode(ctx, batch) == 0) {
                        float *embedding = llama_get_embeddings_seq(ctx, 0);
                        if (embedding && splinter_set_embedding(keys[i], embedding) == 0) {
                            std::cout << "Updated embedding for: " << key_str << "\n";

                            // we MUST read the brand new epoch created by our write,
                            // so we don't trigger ourselves on the next loop.
                            processed_epochs[key_str] = splinter_get_epoch(keys[i]);
                        }
                    }
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