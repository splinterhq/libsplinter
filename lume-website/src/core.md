---
title: Splinter's Core, Benchmarks & Code Walkthrough
date: "2026-02-20"
author: Tim Post
draft: false
metas:
  lang: en
  description: >-
    Explore the core architecture, C code examples, and performance benchmarks of Splinter, including its application as an LLM semantic hippocampus.
  keywords:
    - Splinter Core
    - C Programming
    - Memory Benchmarks
    - Lock-Free Architecture
    - Retrieval-Augmented Generation
  robots: true
  generator: true
---
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

All tests were conducted on a Tiger Lake (i3-1115G4) with 6GB total usable RAM
while performing other experiments (worst case workday):

| Test | Memory Model | Hygiene | Duration (ms) | W/Backoff (us) | Threads | Ops/Sec   | EAGAIN% | NumKeys | Corrupt |
| ---- | ------------ | ------- | ------------- | -------------- | ------- | --------- | ------- | ------- | ------- |
| MRSW | in-memory    | none    | 60000         | 0              | 63 + 1  | 3,259,500 | 23.72   | 20K     | 0       |
| MRSW | im-memory    | hybrid  | 60000         | 0              | 63 + 1  | 3,620,886 | 30.50   | 20K     | 0       |
| MRSW | in-memory    | none    | 60000         | 0              | 31 + 1  | 3,245,405 | 20.76   | 20K     | 0       |
| MRSW | im-memory    | hybrid  | 60000         | 0              | 31 + 1  | 3,273,807 | 20.60   | 20K     | 0       |
| MRSW | in-memory    | none    | 30000         | 0              | 15 + 1  | 4,094,896 | 31.94   | 10K     | 0       |
| MRSW | file-backed  | none    | 30000         | 0              | 15 + 1  | 4,768,989 | 29.63   | 10K     | 0       |
| MRSW | file-backed  | none    | 30000         | 150            | 15 + 1  | 2,992,652 | 13.94   | 10K     | 0       |
| MRSW | file-backed  | hybrid  | 30000         | 150            | 15 + 1  | 3,189,306 | 25.91   | 10K     | 0       |

Keys were limited in file tests due to space and wear constraints, but an
in-memory reference is provided.

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

A little levity can make drab analysis marathons more tolerable. Send in the
poop, the clowns, the beds, or whatever it takes in the name of discovery.

## Some C Examples of Splinter During an "Average Work Day":

### 1. The Ingestor: Telemetry & Atomic Global State

This program simulates a high-frequency data loop. It records raw sensor values
while using splinter_integer_op to maintain a global "Event Count" that other
processes can monitor without needing a centralized lock:

```c
#include "splinter.h"
#include <stdio.h>

int main() {
    // open pre-existing project bus
    if (splinter_open("/dev/shm/physics_bus") != 0) return 1;

    // use a BIGUINT to track global events across 100+ processes
    // this allows L3-speed atomic increments
    uint64_t inc = 1;
    double sensor_val = 0.0;
    char key[SPLINTER_KEY_MAX];

    for (int i = 0; i < 1000000; i++) {
        sensor_val = read_spectrometer(); // hypothetical hardware call
        snprintf(key, sizeof(key), "sensor.alpha.%d", i % 100);

        // write the telemetry point
        splinter_set(key, &sensor_val, sizeof(sensor_val));

        // atomic increment of the shared global counter
        // no mutex required; Splinter handles the atomic transition.
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

    // update the high-dimensional vector in shared memory
    // uses sequence locks to ensure readers don't get 'torn' vectors.
    splinter_set_embedding(id, state_vector);

    // some kind of anomaly detection (simulated)
    if (state_vector[0] > 0.95f) { 
        // atomically-apply a Bloom label
        // this also triggers a pulse to the signal arena for anyone watching bit 7
        splinter_set_label(id, LABEL_UNSTABLE);
        // elusive splinter particle "sparticle" :P
        printf("Sparticle %s is drifting! Label applied.\n", id);
    }
}
```

The speed isn't theoretical, though [here's the math](/splinter_performance)
that explains it. FFI isn't scary when you just use C, and loadable modules are
the very next major feature on the way. Until then, the CLI is very hackable.

See also `splinference.cpp` as well as `splinter_stress.c` for examples of
batching and a multi-threaded mosh pit that never corrupts.

### 3. The Monitor: NUMA-Local Watching

This program represents the "Safety Observer." It runs on a specific NUMA node
to minimize memory latency. It doesn't poll; it waits for the Signal Arena to
indicate that an "Unstable" label was applied anywhere on the bus.

```c
#include "splinter.h"
#include <unistd.h>

int main() {
    // bind the bus to NUMA node 1 for local memory controller performance
    splinter_open_numa("/dev/shm/physics_bus", 1);

    // map Bloom bit 7 (Unstable) to Signal Group 0 (Safety)
    splinter_watch_label_register(LABEL_UNSTABLE, GROUP_SAFETY);

    uint64_t last_count = splinter_get_signal_count(GROUP_SAFETY);

    while (1) {
        uint64_t current = splinter_get_signal_count(GROUP_SAFETY);
        
        if (current > last_count) {
            // a writer somewhere just set the UNSTABLE bit on a key
            printf("SAFETY ALERT: %lu new unstable detections!\n", current - last_count);
            
            // snapshot the header to check parse_failures or global epoch
            splinter_header_snapshot_t snap;
            splinter_get_header_snapshot(&snap);
            
            last_count = current;
        }
        
        // in the real world we would use epoll() on an eventfd
        // linked to the signal group. 
        usleep(500); 
    }

    return 0;
}
```

## LLM Runtime / RAG / Training Uses

Large Language Models are brilliant, but they suffer from severe amnesia. The
current industry solution to this is to bolt on a traditional Vector Database,
which forces the LLM to pause its thinking, serialize a JSON request, send it
over a network socket, and wait for a database to reply.

Splinter allows you to build a **Semantic Hippocampus**: a shared memory space
where the LLM’s short-term context and long-term memories live in the exact same
physical RAM, accessible instantly without a single `memcpy()`.

To understand how to use it, you don't need a PhD in linear algebra. You just
need to know your toolkit:

- **Cosine Similarity is your Steam Shovel:** It grabs massive, general scoops
  of memory that are semantically pointing in the same direction.
- **Euclidean Distance is your Sifter:** It shakes out the exact literal matches
  from that massive bucket of context.
- **Feature Flags are your Post-It Notes:** You can slap a 64-bit integer onto
  any memory slot to instantly track metadata (e.g.,
  `flag_user_frustrated = 1`).
- **Bloom Filters are your C-Style Tags:** They allow you to instantly filter
  the entire memory bus for specific concepts without scanning every individual
  slot.

So, what can you actually _build_ with those tools?

### 1. The Hallucination Governor (Preventing Narrative Lysis)

Because Splinter operates at L3 cache speeds, it can sit _inside_ the LLM's
auto-regressive generation loop. Using your Steam Shovel and Sifter, you can
constantly measure the "Tension" of the text the LLM is generating. If the LLM
starts speaking highly confidently about a subject where it has zero factual
grounding in its memory slots, a Splinter watcher detects that "Vacuum State."
It can physically halt the LLM mid-sentence before a hallucination escapes into
the user's terminal.

Logging uncertainty at inference and emitting special `<UNC_explanation>` tokens
lets you watch for these events _very_ specifically. Custom information physics
engines can provide even greater accuracy, such as those that Splinter is being
used to research.

### 2. True Zero-Latency RAG (Retrieval-Augmented Generation)

In traditional RAG, embedding vectors are serialized and dragged across the
kernel boundary. With Splinter, the inference engine (like `llama.cpp`) simply
casts a raw C-pointer to the L3 cache. When a user asks a 70B parameter model a
question, the context injection happens instantly. You are feeding the AI its
own memories at the speed of the hardware bus.

### 3. User-space Conversational Memory

Splinter gives you a cheap semantic key->value store for personalizations
that would work blazingly fast persistently, too. If ctags style bloom isn't
enough, a 1024x1024 store just for their needs works perfectly. 

