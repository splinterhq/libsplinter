---
title: Example Workday Uses
parent: "Core Library"
nav_order: 4
metas:
  lang: en
  description: >-
    Splinter is a minimalist, lock-free key-value manifold designed for high-frequency data ingestion and retrieval across disjointed runtimes using IPC.
  keywords:
    - Vector Anti-Database
    - Shared-Memory Substrate
    - Lock-Free IPC
    - LLM Semantic Memory
    - Inter-Process Communication
  robots: true
  generator: true
---

# Examples of Splinter During an "Average Work Day":

This is presented in C, but transliterates to any language for the most part.
**Note that code is provided "as-is" for illustrative use; it may require
polishing and further implementation to compile.**

## 1. The Ingestor: Telemetry & Atomic Global State

This program simulates a high-frequency data loop. It records raw sensor values
while using splinter_integer_op to maintain a global "Event Count" that other
processes can monitor without needing a centralized lock:

```c
#include "splinter.h"
#include <stdio.h>

int main() {
    // open pre-existing project bus
    if (splinter_open("physics_bus") != 0) return 1;

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

## 2. The Analyzer: Vector State & Bloom Signaling

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

The speed isn't theoretical, though [here's the math](/splinter-performance/)
that explains it. FFI isn't scary when you just use C, and loadable modules are
the very next major feature on the way. Until then, the CLI is very hackable.

See also `splinference.cpp` as well as `splinter_stress.c` for examples of
batching and a multi-threaded mosh pit that never corrupts.

## 3. The Monitor: NUMA-Local Watching

This program represents the "Safety Observer." It runs on a specific NUMA node
to minimize memory latency. It doesn't poll; it waits for the Signal Arena to
indicate that an "Unstable" label was applied anywhere on the bus.

```c
#include "splinter.h"
#include <unistd.h>

int main() {
    // bind the bus to NUMA node 1 for local memory controller performance
    splinter_open_numa("experiment_bus", 1);

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
