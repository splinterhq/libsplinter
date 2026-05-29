# Splinter: A Cooperative Userspace Hypervisor for Semantic Workloads

Local Large Language Model (LLM) inference is currently choking on the "Socket and Lock" tax. Standard IPC tools and databases require heavy context switching, serialization, and kernel interrupts just to synchronize state. When you are generating text or evaluating semantic alignment at token speeds, that overhead isn't just a bottleneck—it's a wall. 

Splinter dismantles that wall. It is a lock-free, cooperative userspace hypervisor designed from the ground up for strict mechanical sympathy with modern CPU cache hierarchies (x86_64, ARM, and RISC). It puts your governance, your vector storage, and your inference engine in the exact same physical memory lane. 

Think of Splinter as a **semantic breadboard**. It provides a passive, shared-memory manifold where thousands of context or classification windows for multimodal inference run simultaneously with 100% non-blocking throughput. 

## The Core Concept: Cooperative Virtualization

Splinter treats your local workspace not as a database pipeline, but as an execution topology:

* **The Privilege Topology:** When run as `root`, Splinter is **ringless**, bypassing standard OS arbitration barriers to align directly with CPU L1/L3 cache lines and pin NUMA nodes. Under an underprivileged user, it acts as a **single-ring** virtualized environment, managing its own ecosystem of agents and shards without trapping down into Ring 0.
* **Cooperative Scheduling:** Rather than using aggressive, preemptive hardware interrupts that cause CPU thrashing, Splinter coordinates access via an aligned 32-slot bid table. Shards check a shared sovereignty table and voluntarily yield memory regions using a protocol backed by POSIX `madvise` primitives.
* **The Shell Block Hypercall:** Governance is handled natively via the operating system. When an agent signals an unaligned intent, the supervisor blocks it inside a native shell call. The kernel parks the thread at zero CPU or token cost—freezing the agent in place until the compliance protocol issues a green-light ACK to resume execution.

## Architectural Features

* **Zero-Copy Substrate:** Multi-dimensional arrays mapped via `mmap` are treated as raw, continuous memory lanes. Client-side WASM engines (via WASMEdge) or local Lua scripts execute fixed-width SIMD instructions directly over the shared pointers without intermediate serialization.
* **Mechanical Sympathy:** Lock-free, 64-byte aligned architecture utilizing sequence locks (inspired by the Xen hypervisor) ensures readers never block writers, maintaining pristine L3 cache residency.
* **In-Place Atomics:** Execute bitwise and mathematical operations directly on `BIGUINT` keys within the shared memory pool.
* **Externalized Hippocampus:** Agents can dynamically spin up local semantic scratchpads in `.splinter/` to offload raw context retrieval from their expensive API windows, dropping token consumption by up to 45% while driving down hallucination rates.

## The Splinter Toolchain

Splinter is accompanied by a minimalist, bare-metal C toolchain designed for production-grade telemetry and control:

* **`splinference`**: An embedding inference engine that maps directly to the bus with no socket layer, managed natively by `systemd`.
* **`splainference`**: A completion and conversational runtime that maps system prompts, generation windows, and active RAG contexts directly to the shared substrate.
* **`splinterctl` & `splinterpctl`**: A lightning-fast CLI and REPL that completely isolates administrative interaction from core storage performance.
* **`sidecar`**: A DevOps monitoring tool providing real-time visibility into the semantic bus, tracking active slots, bid windows, and `evProcessor` tension metrics.

## Building

On Debian/Ubuntu you can run `scripts/bigbang.sh` to automatically install Splinter with vector storage
and `llama.cpp` from source and install both on your system. This will also download Nomic Text 1.5 for 
you from Hugging Face, which is all you need to get started with Splinter as a semantic breadboard.

If you want to build advanced things (lua, WASMEdge, NUMA) then run `./configure --help` to see how to 
enable those options. A `--install-deb-deps` option is provided by the script to automatically install
all the Debian packages you need to build Splinter's optional dependencies.

A typical build:

```
./configure --enable-llama --enable-vectors --enable-lua --enable-wasm --enable-numa
make
sudo -E make install
```

Valgrind will be used during `make tests` if you have it on your system. See `splinter_chi_sao` for
benchmarks.

## Performance Under Fire

Tested rigorously under strict hardware constraints (including fanless, low-tier Chromebook development environments):

* **Multi-Reader, Single-Writer (MRSW):** Sustains **3.2 million operations/second** with zero data corruption.
* **Multi-Reader, Multi-Writer (MRMW):** Utilizing the disjointed-lane collision resolution protocol (`splinter_chi_sao`), 32 concurrent writers sustain **15.6 million operations/second** with zero data corruption.

## Project Status & The Road to 1.2.0

Splinter is currently >95% implemented with high test-coverage across its primary primitives. 

### The 1.2.0 Milestone
Version 1.2.0 represents the feature-complete graduation of the core engine. The final roadmap includes:
* Finalizing Shard accounting and POSIX `madvise` univocality boundaries.
* Integrating the `evProcessor` governance loop directly into active `splainference` RAG and data-narration workflows.
* Standardizing the base protocol for client-side meta-keys (`__on_write`, `__on_read`) for Lua data-transformation.

### Governance and Open Source Commitment
Splinter is open-source because transparent, text-driven governance and systemic bias auditing should be fundamental infrastructure, not a locked enterprise feature. 

Once version 1.2.0 ships (Target: Mid-May/Early June 2026), the core repository will move into long-term **maintenance mode**. It will receive continuous updates, performance optimizations, and community-driven enhancements and fixes to existing features, but the feature set will remain broadly locked. Splinter can't
keep its original identity and be a kitchen sink.

**Splinter will never be abandoned.** By design, memory lifecycle management is kept ruthlessly clean: long-term, high-priority semantic stores are explicitly tracked via Git, while transient working memory scratchpads are purged safely by the operator using standard development tooling like `make distclean`. Commercial development will focus entirely on the high-level semantic classification layers and application ecosystems built *on top* of this bedrock, leaving the open substrate pristine, public, and free. 

After 1.2.0, improvements would be so specific to the author's use case that very few others would benefit from
them. For the most part, even the commercial code will remain open, just no longer part of _this_ project.
