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
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <bus_name> <key_name> <path_to_nomic_gguf>\n";
        return 1;
    }

    const char* bus_name = argv[1];
    const char* target_key = argv[2];
    const char* model_path = argv[3];

    // 1. Connect to the Splinter Substrate
    if (splinter_open(bus_name) != 0) {
        std::cerr << "Failed to connect to Splinter bus: " << bus_name << "\n";
        return 1;
    }

    // 2. Initialize llama.cpp
    llama_backend_init();
    
    llama_model_params model_params = llama_model_default_params();
    // API UPDATE: Pass params by value, updated function name
    llama_model *model = llama_model_load_from_file(model_path, model_params);
    if (!model) {
        std::cerr << "Failed to load model from " << model_path << "\n";
        return 1;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.embeddings = true; 
    // API UPDATE: Pass params by value, updated function name
    llama_context *ctx = llama_init_from_model(model, ctx_params);

    // 3. The Epoch Dance: Zero-Copy Read
    size_t val_len = 0;
    uint64_t start_epoch = 0;
    
    const void *raw_ptr = splinter_get_raw_ptr(target_key, &val_len, &start_epoch);
    if (!raw_ptr || val_len == 0) {
        std::cerr << "Key not found or empty: " << target_key << "\n";
        return 1;
    }

    if (start_epoch & 1) {
        std::cerr << "Writer currently active on key. Try again.\n";
        return 1;
    }

    // 4. Tokenize directly from the shared memory pointer
    std::vector<llama_token> tokens(val_len + 8); 
    
    // API UPDATE: Extract vocab from the model
    const llama_vocab *vocab = llama_model_get_vocab(model);
    
    // API UPDATE: Pass vocab instead of model
    int n_tokens = llama_tokenize(
        vocab, 
        static_cast<const char*>(raw_ptr), 
        val_len, 
        tokens.data(), 
        tokens.size(), 
        true,  
        false  
    );

    if (n_tokens < 0) {
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(vocab, static_cast<const char*>(raw_ptr), val_len, tokens.data(), tokens.size(), true, false);
    }

    // 5. Verify the Epoch hasn't mutated
    uint64_t end_epoch = splinter_get_epoch(target_key);
    if (start_epoch != end_epoch) {
        std::cerr << "Torn read detected! The value mutated during tokenization.\n";
        return 1;
    }

    // 6. Run Inference
    // API UPDATE: llama_batch_get_one no longer takes trailing default zeros
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    if (llama_decode(ctx, batch) != 0) {
        std::cerr << "Failed to decode/run inference.\n";
        return 1;
    }

    // 7. Retrieve the embedding
    float *embedding = llama_get_embeddings_seq(ctx, 0);
    if (!embedding) {
        std::cerr << "Failed to extract embeddings. Ensure model is an embedding model.\n";
        return 1;
    }

    // 8. Publish the Vector back to Splinter
    if (splinter_set_embedding(target_key, embedding) != 0) {
        std::cerr << "Failed to write embedding back to Splinter bus.\n";
        return 1;
    }

    std::cout << "Successfully generated and published embedding for '" << target_key << "'.\n";

    // Cleanup (API UPDATE: llama_model_free)
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    splinter_close();

    return 0;
}