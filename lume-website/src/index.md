---
title: Splinter ⚡ A Vector Anti-Database & Shared-Memory Substrate
date: "2026-02-20"
author: Tim Post
draft: false
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
# Splinter ⚡ A Vector Anti-Database & Shared-Memory Substrate

Splinter is a minimalist, lock-free key-value manifold designed to facilitate
high-frequency data ingestion and retrieval across disjointed runtimes. It is
built on the belief that for local inter-process communication (IPC), the
kernel’s networking stack is an expensive and unnecessary coupling.

Splinter emerged out of frustration resulting from attempting to stretch tools
over gaps that they simply were never designed to cover. It wasn't a question of
more tuning; it was a need to cut out the socket layer and kernel arbitration
completely.

It was either completely dismantle and re-imagine SQLite, or write something
completely different. Given the sparse availability of options, different seemed
most beneficial to both the current need as well as the current ecosystem.

## Design Philosophy: Low Complexity + Systemic Sympathy = Speed!

Modern software has become complacent with IAAS marketing, assuming that CPU
cycles and memory bandwidth are infinite. We invoke help from the kernel's
socket layer to transfer a value that we already have in memory to another
region in the same physical memory as standard practice.

And now we're doing that with 768-dimensional vectors 😱. Splinter is a gesture
back in the direction of efficiency for systems development. Here are the core
tenets that set it apart (_**aka:
[why Splinter is so damn fast](/splinter_performance)**_):

- **Splinter Is a Passive Substrate:** Splinter is not a daemon. It is a
  memory-mapped region that acts as a mutual option for every process on the
  system.
- **(DRYD) Zero-Copy Intent:** _(D)on't (R)epeat (Y)our (D)ata._ In Splinter,
  information is never "sent"; it is published. Readers access the raw memory
  directly, crossing only the minimal checkpoints needed for safe coordination,
  eliminating the energetic tax of serialization and context switching.
- **Static Geometry:** By using a fixed-geometry arena, Splinter eliminates the
  "learned negligence" of dynamic heap fragmentation and background garbage
  collection.
- **Lock-Free Practicality:** No time is wasted acquiring blocking mutex locks;
  Splinter uses standard portable atomic sequence locks (`epoch`).
- **Unopinionated & Agnostic:** Implement LRU or TTL eviction how you like in a
  loadable shard, or no eviction at all. Splinter doesn't care.

### The "Good Process Neighbor" Approach

_(Even though technically only the CLI or client code is the process because
Splinter itself is just a place, not a process)_

Splinter assumes **informed intent**. It does not try to outsmart the kernel
with `O_DIRECT` or complex paging logic. It provides the metadata (`ctime`,
`atime`, `epoch`) and the memory region, then gets out of your way. It is a tool
for engineers who would rather spend their thermal budget on the math, not the
management.

Relational databases attempt to shield themselves from the kernel by trying to
_be_ the kernel. Splinter goes out of its way to not bother the kernel unless it
must, and its logic shards inform the kernel of how the memory is intended for
use at every step of the way. Here's
[more about why Splinter and Linux are great friends](/splinter_and_linux/).

## Supported Platforms & Linkage

Splinter is designed to work on any modern GNU/Linux flavor. Windows users can
use WSL with a slight penalty. MacOS requires some questionable shimming around
the lack of `memfd` (forcing anonymous file descriptors to work), but it
_should_ otherwise function perfectly.

**Optional Linkage (Enable during build):**

- NUMA (`libnuma-dev`) for NUMA affinity | `WITH_NUMA=1`
- LUA (`lua5.4-dev`) for LUA integration | `WITH_LUA=1`
- llama.cpp for the nomic inference shard | `WITH_LLAMA=1`
- Valgrind (`libvalgrind-dev`) for tighter Valgrind test integration |
  `WITH_VALGRIND=1`

## Quick Start / Building & Installing

If you want to build Splinter with "everything", just clone the repo, enter `libsplinter/`
and just type **`make`**. This will configure a build with `-DWITH_NUMA=ON`, `-DWITH_LUA=ON`, 
`-DWITH_EMBEDDINGS=ON`, `-DWITH_LLAMA=ON` (for inference) and `-DWITH_RUST=ON` for bindings.

This also means you need to have all of those prerequisites installed and want to build with
them. If you want to pick and choose yourself, or enable nothing at all, then do this:

```bash
git clone git@bitbucket.org:tinkertim/libsplinter.git

cd libsplinter
mkdir build
cd build

cmake -D{your flags} ..
ctest --output-on-failure
sudo -E make install
```

The `-E` option tells Sudo to preserve your environment, which is required
if you're running install targets with Rust enabled. Once you have installed,
you can verify with `splinterctl --version`. 

From there, check out [the CLI](/cli) and then [the C API](/core) and 
[bindings](/bindings). You can find examples of most functions in 
`splinter_test.c` if the doxygen-style comments aren't enough.

## Comparison With Related Tools

It's not entirely fair to compare active databases to Splinter because they are
competing with their "hands tied" by mutexes and socket layers. Splinter
deliberately eschews any calculation on-write that it can't do with simple
bitwise math; Splinter _**tries**_ to stay boring.

| Feature        | Splinter                                                       | Traditional Vector DBs   |
| -------------- | -------------------------------------------------------------- | ------------------------ |
| **Transport**  | `memfd()` that degrades gracefully to `mmap()` (L3 Speed)      | TCP/gRPC (Network Stack) |
| **Daemon**     | None (Passive)                                                 | Active Service (Heavy)   |
| **Footprint**  | Static & Deterministic                                         | Dynamic & Volatile       |
| **Complexity** | ~ 875 Lines of obsessively-optimized C (Will never exceed 999) | 100k+ Lines of Code      |

Think about what you can do once TypeScript, Rust, Python and Go can all share
the same address space and embeddings **safely**, without socket or even
`memcpy()` overhead instead `:)`.

## The Main Splinter Use Cases (What's it Good For?)

Splinter can be anything from a simple configuration store to a Rank-2 tensor
model scaffold. It's designed for **vector-heavy workflows** like Artificial
Intelligence (AI) inference or high-resolution physics and linguistic research.

#### 1. High-Res Physics Of All Kinds

Do you like vectors? Of course you do.

Splinter was built around the idea of capturing raw data exceptionally well
while making backfill easy. It allows up to 64 signal groups per bus,
ctags-style labeling, and built-in per-slot Bloom filters. Slot coupling allows
for simple standard ordered sets (e.g. `foo_key.1`, `foo_key.2` for velocity and
acceleration). You can record high-frequency data at L3 speeds without hardware 
aliasing, and have many keys in tandem with vectors all representing
a single fraction of a second, if you have the room. 

Splinter was built primarily around [GDELT](https://www.gdeltproject.org/) 
consumption, to set expectations. High-rank tensors? No problem, that's routine
in the author's use and splinter makes sharing them fast and safe.

#### 2. The Semantic Hippocampus (LLM Orchestrated Memory)

Splinter functions remarkably well as semantic short-to-long-term memory for
Large Language Models. LRU-based movement helps "forget" ephemera quickly while
making sure stuff that actually matters (as viewed by access time and epoch)
settles into long-term memory. You can run inference directly on the bus,
accessing embeddings using Splinter's supervised raw pointers so operations
require zero `memcpy()`.

Plus, well, "psychic search" being fundamental to design is quite attractive
for such a feature. Inference (on the bus) via `.gguf` is included.

#### 3. Configurations, Registries & Edge Caching

Splinter's epochs and feature flags lend very well to application configuration
on Linux systems. You can also compile Splinter to simply ignore embeddings
(`WITH_EMBEDDINGS=0`) and use it as a local, socket-less cache server. (The
author uses Splinter to trickle into Redis based on key activity).

#### 4. Embedded IoT Use

Splinter is great for environmental loggers or system ring buffers because its
static geometry is vastly superior for flash-based storage than relational
databases. At just 875 lines of code, it stays in the "hot path" for most modern
edge processors.

## Exhaustive Feature Overview

#### 1. Performance & Scale

- **Throughput:** Validated at **3.2M+ ops/sec** on early-gen, throttled
  consumer hardware. With proper NUMA configuration on modern fast hardware, it
  can reach near 500M ops/sec.
- **Latency:** Resolves at L3 cache speeds by utilizing `memfd() / mmap()` for
  the primary transport.
- **Scalability:** Supports disjointed MRMW (Multiple Reader, Multiple Writer)
  semantics via per-slot atomic sequence locks.

#### 2. Vector & Math Native

- **Dimensional Storage:** Native support for **768-dimension vectors**
  (optimized for Nomic v2/LLM embeddings).
- **In-Place Atomic Ops:** Keys tagged as `BIGUINT` support atomic `INC`, `DEC`,
  `OR`, `XOR`, `AND`, and `NOT` operations directly in shared memory.
- **Tandem Keys:** Multi-order support allows for atomic updates to related
  signals (e.g., `sensor.1`, `sensor.2`).

#### 3. Mechanical Hygiene (Auto-Scrubbing)

Splinter offers three levels of sanitation to balance data integrity with
computational thermodynamics:

- **None:** Minimal movement; assumes readers strictly honor value length
  metadata.
- **Hybrid (Fast Mop):** Zeroes the incoming byte length plus a **64-byte
  aligned** tail to satisfy SIMD/Vectorized loads.
- **Full (Boil):** Zeroes the entire pre-allocated value region, ensuring
  absolute isolation for verifiable research.

#### 4. The Signal Arena

- **Pulse Groups:** Up to 64 independent signal groups for `epoll()`-backed
  notifications.
- **Bloom Labeling:** High-performance key tagging allows watchers to filter for
  specific signal "vibrations" without scanning the entire store.

#### 5. Sidecars & Loadable Shards

- **Inference Included:** Splinter ships with a tiny inference daemon
  (`splinference.cpp`) that loads Nomic Text locally and butlers embeddings for
  you on a defined signal group—with zero socket overhead.
- **Modular Logic:** Side-load specialized C logic (DSP, ANN search, Inference)
  via `insmod`.

### Contact The Author

I'm Tim Post (former Stack Overflow Employee & Community Leader). You can reach
me at `timthepost@protonmail.com` if you have questions.
