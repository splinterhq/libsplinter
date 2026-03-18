/**
 * Splinference: The Unified Inference Sidecar Daemon for libsplinter
 * License: MIT
 * 
 * Provides inference without memcpy() overhead associated with moving
 * tokens and embeddings around. Highly performant with a quantized 
 * version of Nomic Text.
 * 
 * Splinter interface by Tim Post
 * GGML/llama.cpp wrapper mostly by Gemini-3
 * 
 * (Was written while llama.cpp was completely re-doing their KV API)
 */

#include <atomic>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <csignal>
#include <chrono>
#include <thread>

// Bridge the C/C++ atomic divide
using atomic_uint_least64_t = std::atomic_uint_least64_t;
using atomic_uint_least32_t = std::atomic_uint_least32_t;
using atomic_uint_least8_t  = std::atomic_uint_least8_t;

extern "C" {
    #include "splinter.h"
    #include "llama.h"
}
#include "llama-cpp.h"

volatile sig_atomic_t keep_running = 1;
void handle_signal(int sig) { (void)sig; keep_running = 0; }

struct SplinferenceContext {
    llama_model_ptr model;
    llama_context_ptr ctx;
    const struct llama_vocab * vocab = nullptr;
    std::unordered_map<std::string, uint64_t> processed_epochs;
    std::unordered_map<llama_token, int> token_counts;
    int surge_threshold = 100;
};

// Rolling ID to ensure the model treats every update as a fresh start
static llama_seq_id global_seq_id = 0;

bool is_empty_embedding(const char* key) {
    size_t  rawPtrSize = 0, rawPtrEpoch = 0;
    float* emb = (float*)splinter_get_raw_ptr(key, &rawPtrSize, &rawPtrEpoch);
    if (!emb) return true;
    for (int i = 0; i < 768; i++) {
        if (emb[i] != 0.0f) return false;
    }
    return true;
}

// llama was in the middle of a major KV cache overhaul upstream when
// this was written. 
bool process_slot(const char* key, SplinferenceContext& sc) {
    size_t val_len = 0;
    uint64_t current_epoch = 0;
    
    const void* raw_ptr = splinter_get_raw_ptr(key, &val_len, &current_epoch);
    if (!raw_ptr || val_len == 0) return false;

    std::vector<llama_token> tokens(val_len + 32); 
    int n_tokens = llama_tokenize(sc.vocab, (const char*)raw_ptr, (int)val_len, 
                                  tokens.data(), (int)tokens.size(), true, false);
    
    if (n_tokens <= 0) return false;
    tokens.resize(n_tokens);

    // Prepare batch
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);

    // Fix the pointer gymnastics for seq_id
    // batch.seq_id[i] is a pointer to an array of IDs for token i.
    // Since we use llama_batch_get_one, we only have one sequence to worry about.
    llama_seq_id current_id = global_seq_id++;
    for (int i = 0; i < n_tokens; ++i) {
        batch.seq_id[i][0] = current_id;
    }

    // Since we don't have llama_kv_cache_seq_rm, we rely on the 
    // fact that we are using a brand new sequence ID for every call.
    if (llama_decode(sc.ctx.get(), batch) == 0) {
        float* embd = llama_get_embeddings_seq(sc.ctx.get(), current_id);
        if (embd) {
            splinter_set_embedding(key, embd);
            return true;
        }
    }
    
    // Safety: If sequence IDs get too high, wrap around to avoid overflow
    if (global_seq_id > 1000000) global_seq_id = 0;

    return false;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <bus> <model_path> <intake_group> <surge_limit> [--backfill]\n";
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    const char* bus_name = argv[1];
    const char* model_path = argv[2];
    uint8_t intake_group = (uint8_t)std::stoi(argv[3]);
    
    SplinferenceContext sc;
    sc.surge_threshold = std::stoi(argv[4]);

    if (splinter_open(bus_name) != 0) return 1;

    llama_backend_init();
    llama_log_set([](ggml_log_level level, const char * text, void * user_data) {
        (void) user_data;
        
        if (level == GGML_LOG_LEVEL_ERROR) {
            fputs(text, stderr);
            fflush(stderr);
        }
    }, nullptr);

    llama_model_params mparams = llama_model_default_params();
    sc.model.reset(llama_model_load_from_file(model_path, mparams));
    if (!sc.model) return 1;

    sc.vocab = llama_model_get_vocab(sc.model.get());

    llama_context_params cparams = llama_context_default_params();
    cparams.embeddings = true; 
    sc.ctx.reset(llama_init_from_model(sc.model.get(), cparams));
    if (!sc.ctx) return 1;

    bool backfill_requested = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--backfill") backfill_requested = true;
    }

    if (backfill_requested) {
        std::cout << "Starting initial bus scan for backfill...\n";
        char* keys[2048]; // Bumped for larger stores
        size_t count = 0;
        splinter_list(keys, 2048, &count);

        for (size_t i = 0; i < count; i++) {
            // Check if it actually needs an embedding
            if (is_empty_embedding(keys[i])) {
                std::cout << "Backfilling: " << keys[i] << "...\n";
                if (process_slot(keys[i], sc)) {
                    sc.processed_epochs[keys[i]] = splinter_get_epoch(keys[i]);
                }
            } else {
                // If it already has one, just track the current epoch so we don't 
                // re-process it immediately when the loop starts.
                sc.processed_epochs[keys[i]] = splinter_get_epoch(keys[i]);
            }
        }
        std::cout << "Initial backfill complete.\n";
    }

    std::cout << "Running on signal group " << (int)intake_group << "\n";
    uint64_t last_p0 = 0, last_pi = 0;

    while (keep_running) {
        uint64_t cur_p0 = splinter_get_signal_count(0);
        uint64_t cur_pi = splinter_get_signal_count(intake_group);

        if (cur_p0 == last_p0 && cur_pi == last_pi) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        last_p0 = cur_p0;
        last_pi = cur_pi;

        char* keys[1024];
        size_t count = 0;
        splinter_list(keys, 1024, &count);

        for (size_t i = 0; i < count; i++) {
            uint64_t ep = splinter_get_epoch(keys[i]);
            if (sc.processed_epochs[keys[i]] < ep) {
                if (process_slot(keys[i], sc)) {
                    sc.processed_epochs[keys[i]] = splinter_get_epoch(keys[i]);
                }
            }
        }
    }

    splinter_close();
    llama_backend_free();
    return 0;
}
