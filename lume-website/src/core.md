# Splinter's Core, Benchmarks & Code Walkthrough

## "Core" Means `splinter.c` and `splinter.h` only.

Philosophically, Splinter is built under the following constraints:

1. It is not a database; it is an active storage substrate. Clients do all
   calculating.
2. The main hot path of the code should strive to fit in a modern processor's
   instruction cache.

This set of constraints has kept the core library very lean, currently at around
845 lines of actual code, less if you remove support for embeddings, even less
if you take out the bitwise operators if you don't need them, etc. It's designed
to be embedded and customized. Splinter provides a stable, high-speed substrate
in less code than most line editors occupy.

### Embedding Splinter

You have the option to link against `libsplinter.a` or `libsplinter_p.a` if you
have them in your linker path. You also have the option of just adding
`splinter.c` as a dependency and including its header file, `splinter.h`, like
any other tiny C library.

It's recommended that you first make a copy of them and then _**take out**_ any
of the public API functions and forward declarations that you don't use for
maximum efficiency; the smaller it gets, the more likely it will be optimally
cached by the CPU.

Leave the originals as-is, so you can still build and use the CLI to manage the
stores. Additionally, **double check alignment at 64 bytes** (there's a test for
that) if you do anything to the slot structures.

## Benchmarks

All tests were conducted on a Tiger Lake (i3-1115G4) with 6GB total usable RAM.

| Test | Memory Model | Hygiene Model | Duration (ms) | Writer Backoff (us) | Threads | Operations/Sec | EAGAIN % | Corruptions |
| ---- | ------------ | ------------- | ------------- | ------------------- | ------- | -------------- | -------- | ----------- |
| MRSW | in-memory    | none          | 60000         | 0                   | 64      | 3.2mm          | 21.15    | 0           |
| MRSW | im-memory    | hybrid        | 60000         | 0                   | 64      | 3.2mm          | 21.52    | 0           |
| MRSW | in-memory    | none          | 60000         | 0                   | 32      | 3.2mm          | 21.15    | 0           |
| MRSW | im-memory    | hybrid        | 60000         | 0                   | 32      | 3.2mm          | 21.52    | 0           |
| MRSW | file-backed  | none          | 30000         | 150                 | 16      | 2.7mm          | 18.3     | 0           |
| MRSW | file-backed  | hybrid        | 30000         | 150                 | 16      | 2.7mm          | 18.3     | 0           |

<sub><em>`*` We expect fewer `EAGAIN` results with a 150us backoff because the
work just aligns with the seqlock (and lane) better, albeit with less velocity
than in-memory. See the `splinter_stress` tool that ships with Splinter for
more!</em></sub>

## A Note On Keyspaces

The `key` part of `key -> value` really is the key to success in your schema (no
puns intended, honestly).

The most basic use of a key is something like `foo`. And if you used tandem
slots, you'd be able to access foo, its velocity, its acceleration and its
jitter through `foo, foo.1, foo.2` and `foo.3` respectively.

That's easy enough, but what if `foo` collided with something else named `foo`
that you also wanted to track? That's why it's handy to prepend some kind of
namespace prior to the key. The author uses something like this:

`type_name::experiment_name::run::variable::`

Just .. _be kind to your future self when naming keys_ and remember you can
change the order access operator if needed. You can change it to something that
won't appear in your normal data, like 💩; the separator is stored as
`SPL_ORDER_SEPARATOR` in `splinter.h`.

A little levity can make drab analysis marathons more tolerable.

## Core API examples:

The best ways to see the public API being exercised is to look at
`splinter_test.c` as well as the source code to the individual CLI commands.
Splinter's CLi is designed to be a production-grade tool, no bones about it, but
it's also a useful resource to see how the code is intended for use.

Here are some examples so you can see if Splinter is something that would
interest you.

### 1. The Ingestor: Telemetry & Atomic Global State

This program simulates a high-frequency data loop. It records raw sensor values
while using splinter_integer_op to maintain a global "Event Count" that other
processes can monitor without needing a centralized lock:

```c
#include "splinter.h"
#include <stdio.h>

int main() {
    // Open the pre-existing research bus
    if (splinter_open("/dev/shm/physics_bus") != 0) return 1;

    // We use a BIGUINT to track global events across 100+ processes
    // This allows L3-speed atomic increments
    uint64_t inc = 1;
    double sensor_val = 0.0;
    char key[SPLINTER_KEY_MAX];

    for (int i = 0; i < 1000000; i++) {
        sensor_val = read_spectrometer(); // Hypothetical hardware call
        snprintf(key, sizeof(key), "sensor.alpha.%d", i % 100);

        // 1. Write the telemetry point
        splinter_set(key, &sensor_val, sizeof(sensor_val));

        // 2. Atomic increment of the shared global counter
        // No mutex required; Splinter handles the atomic transition.
        splinter_integer_op("global_event_count", SPL_OP_INC, &inc);
    }

    splinter_close();
    return 0;
}
```

### 2. The Analyzer: Vector State & Bloom Signaling

In this scenario, we process the telemetry and generate a state vector
(embedding). We use Bloom Labels to categorize particles. If a particle is
"Unstable," we tag it, which automatically pulses a specific Signal Group to
wake up a safety-shutdown process:

```c
#include "splinter.h"

#define LABEL_UNSTABLE (1ULL << 7) // We use bit 7 for instability tracking
#define GROUP_SAFETY    0          // Signal group for the emergency process

void analyze_particle_state(const char *id) {
    float state_vector[SPL_EMBED_DIM];
    compute_particle_physics(id, state_vector); // Hypothetical math

    // 1. Update the high-dimensional vector in shared memory
    // Uses Seqlocks to ensure readers don't get 'torn' vectors.
    splinter_set_embedding(id, state_vector);

    // 2. Perform anomaly detection
    if (state_vector[0] > 0.95f) { 
        // 3. Atomically apply a Bloom label
        // This triggers a 'pulse' to the signal arena for anyone watching bit 7.
        splinter_set_label(id, LABEL_UNSTABLE);
        printf("Particle %s is drifting! Label applied.\n", id);
    }
}
```

### 3. The Monitor: NUMA-Local Watching

This program represents the "Safety Observer." It runs on a specific NUMA node
to minimize memory latency. It doesn't poll; it waits for the Signal Arena to
indicate that an "Unstable" label was applied anywhere on the bus.

```c
#include "splinter.h"
#include <unistd.h>

int main() {
    // Bind the bus to NUMA node 1 for local memory controller performance
    splinter_open_numa("/dev/shm/physics_bus", 1);

    // Map Bloom bit 7 (Unstable) to Signal Group 0 (Safety)
    splinter_watch_label_register(LABEL_UNSTABLE, GROUP_SAFETY);

    uint64_t last_count = splinter_get_signal_count(GROUP_SAFETY);

    while (1) {
        uint64_t current = splinter_get_signal_count(GROUP_SAFETY);
        
        if (current > last_count) {
            // A writer somewhere just set the UNSTABLE bit on a key
            printf("SAFETY ALERT: %lu new unstable detections!\n", current - last_count);
            
            // Snapshot the header to check parse_failures or global epoch
            splinter_header_snapshot_t snap;
            splinter_get_header_snapshot(&snap);
            
            last_count = current;
        }
        
        // High-frequency "Nap" - in production, you'd use epoll on an eventfd 
        // linked to the signal group counter.
        usleep(500); 
    }

    return 0;
}
```

## LLM Runtime Use

When you run inference _on the bus_ things tend to speed up exponentially. This
example takes a bus/key name and the location of a Nomic Text GGUF model. It 
loads the model and deposits the embeddings without actually moving any memory
around in the same lane:

```c++
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

    // there's also splinter_create_or_open() as well as open_or_create()
    if (splinter_open(bus_name) != 0) {
        std::cerr << "Failed to connect to Splinter bus: " << bus_name << "\n";
        return 1;
    }

    // the rest of this pretty much just copies llama-server's embedding route
    llama_backend_init();
    
    llama_model_params model_params = llama_model_default_params();
    llama_model *model = llama_model_load_from_file(model_path, model_params);
    if (!model) {
        std::cerr << "Failed to load model from " << model_path << "\n";
        return 1;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.embeddings = true; 
    llama_context *ctx = llama_init_from_model(model, ctx_params);

    // here we have to do "chi sao" (double sticky hands) to observe movement
    // around the raw pointer

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

    // GOOD! Tokenize from what's in memory (don't copy it!)
    std::vector<llama_token> tokens(val_len + 8); 
    
    // get vocab from the model
    const llama_vocab *vocab = llama_model_get_vocab(model);
    
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

    // here is the 'double' part of the dance. Make sure the epoch hasn't changed while we peeked.
    uint64_t end_epoch = splinter_get_epoch(target_key);
    if (start_epoch != end_epoch) {
        std::cerr << "Torn read detected! The value mutated during tokenization.\n";
        return 1;
    }

    // now we can actually run inference
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    if (llama_decode(ctx, batch) != 0) {
        std::cerr << "Failed to decode/run inference.\n";
        return 1;
    }

    // this isn't a "copy" since the model generates it; here we're as close as we can 
    // get to the model just generating right into the target address space as possible.
    float *embedding = llama_get_embeddings_seq(ctx, 0);
    if (!embedding) {
        std::cerr << "Failed to extract embeddings. Ensure model is an embedding model.\n";
        return 1;
    }

    // now the vectors can be published
    if (splinter_set_embedding(target_key, embedding) != 0) {
        std::cerr << "Failed to write embedding back to Splinter bus.\n";
        return 1;
    }

    std::cout << "Successfully generated and published embedding for '" << target_key << "'.\n";

    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    
    // this is non-destructive (check /dev/shm)
    splinter_close();

    return 0;
}
```

This would be way more practical inside a daemon that listened to the bus for
new updates on `VARTEXT`-named keys (you could set up a signal group). That 
same code will work identically with on-disk stores if linked against the 
persistent compilation. 

## Don't Forget FFI!

All of the functions available in C are also available via FFI in any language
that allows dynamic loading into the runtime. You can use Splinter to store and
lazy-load vectors if all you need is storage, or write your own cosine
similarity or ANN search shards to access the embeddings through direct pointers
instead of memory copying.

There really are a lot of possibilities and you can use whatever language you want.
