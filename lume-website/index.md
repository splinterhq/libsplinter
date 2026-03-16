---
title: "Splinter Overview"
nav_order: 1
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

# Splinter ⚡ L3-Speed Shared Memory Vector & KV Store

Splinter is a minimalist, persistable lock-free and bloomable key-value store designed to handle high-frequency data and vector ingestion/retrieval across disjointed runtimes.

It is built on the belief that for local inter-process communication (IPC), the
kernel’s networking stack and arbitration services are expensive and unnecessary 
couplings. Splinter provides "just enough" safety for processes to swim in the 
same address space without conflict, and most importantly, without latency.

Splinter emerged out of frustration resulting from attempting to stretch tools
over gaps as they broke. It wasn't a question of more tuning; it was a need to 
cut out the socket layer and kernel arbitration completely.

It was a choice between dismantling and re-imagining SQLite, or creating
something completely different. Given the sparse availability of options,
different seemed most beneficial to both the current need as well as the current
ecosystem.

## The Swimming Pool Analogy: Understanding Splinter "Visually"

There is nothing quite like Splinter, so it helps to be able to visualize the
architecture so you see how the features work together.

Imagine you aren't just storing data; you are designing a high-performance 
Olympic-sized swimming pool. In fact imagine `splinter_create()` returned 
a multi-lane pool that rivals any YMCA.

But ... with some twists.

### 1. Pre-Allocated Lanes (Zero Overhead)

Instead of building a new pool every time you have a guest (dynamic allocation), 
we build one massive pool at the start. We divide it into perfectly equal lanes. 
If you need a key, you don't "request" it from a lifeguard (the OS/Kernel); 
you simply look at **Lane 4**. You have a direct line of sight to the water 
because the memory is already mapped to your process.

### 2. The Diving Boards (Lock-Free Access)

Each lane has a diving board. Because of Splinter's atomic sequence epochs, 
32 divers  can jump into their own lanes at the exact same time. They never collide. 
If two divers try to hit the same lane simultaneously, the second one doesn't 
crash; they simply "bounce" back to the board (`EAGAIN`) and try again a 
nanosecond later. No one ever stops the flow of the meet to wait for a key.

### 3. The Signal Pulse (The "Aha!" Moment)

Now, imagine the water is connected. When a diver hits the water in Lane 1, 
a **ripple** (signal) travels instantly across the surface. A coach 
(inference) sitting at the far end doesn't have to keep staring at the lane; 
they just wait to feel the ripple. The moment they feel it, they know exactly 
which lane to look at. 

### 4. No Jumping Out (Zero Copy)

In a traditional system (like Redis or SQLite), if you want to inspect a 
diver, you have to pull them out of the water, dry them off, and carry 
them to a different building (Serialization/Network/Memcpy). In Splinter, 
the inspectors are already in the water with the divers. You just use 
a pointer. No carrying, no constant re-serializing, no delays.

## Low Complexity + Mechanical Sympathy = Speed!

Modern software has become complacent with IAAS marketing, assuming that CPU
cycles and memory bandwidth are infinite. We invoke help from the kernel's
socket layer to transfer a value that we already have in memory to another
region in the same physical memory as standard practice.

And now we're doing that with 768-dimensional vectors 😱. Splinter is a gesture
back in the direction of efficiency for systems development. Here are the core
tenets that set it apart (_**aka:
[why Splinter is so fast](/splinter-performance/)**_):

- **Splinter Is a Passive Substrate:** Splinter is not a daemon. It is a
  memory-mapped region that acts as a mutual option for every process on the
  system.
- **(DRYD) Zero-Copy Intent:** _(D)on't (R)epeat (Y)our (D)ata._ In Splinter,
  information is never "sent"; it is published. Readers access the raw memory
  directly, crossing only the minimal checkpoints needed for safe coordination,
  eliminating the energetic tax of serialization and context switching.
- **Static Geometry:** By using a fixed-geometry arena, Splinter eliminates the
  complications that stem from dynamic heap fragmentation and background garbage
  collection.
- **Lock-Free Practicality:** No time is wasted acquiring blocking mutex locks;
  Splinter uses standard portable atomic sequence locks (`epoch`).
- **Unopinionated & Agnostic:** Implement LRU or TTL eviction how you like in a
  loadable shard, or no eviction at all. Splinter doesn't care.

### A Vector / K/V "Anti-Database" Design

Most database designs combine storage and computation in one server layer, and
then provide a client that can interact with it and abstract transactions.

Splinter takes another approach - in order to minimize disruption needed to 
complete a transaction, the _client_ is required to do whatever computational 
heap allocation is necessary while the storage layer remains gapped and only 
performs only bitwise operations on keys in-place (twiddling existing bits).

Splinter does not provide automatic ANN clustering; it leaves that as an
exercise for the client, if the client needs it. See [Splinter Shards](/shards/)
for more about how this works. Splinter will include a set of default shards
to handle basic clustering and eviction strategies as examples of how the library
is intended to work.

While "clients" usually reside on the same machine, they have their own _heap_
in addition to the shared pool, and that's what Splinter utilizes for computation.
We never "slosh" in still water, and design to prevent thundering hurds through
cooperative logic scheduling. 

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
[more about why Splinter and Linux are great friends](/splinter-and-linux/).

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

**See [Building & Intstalling Splinter](/installing/)**. Provided you have a
working compiler (like GCC), Splinter has very few (and all optional)
dependencies.

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

See [use cases](/use-cases/) for an exhaustive list!

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
  via `insmod`-like tooling (Coming soon!).

### Contact The Author

I'm Tim Post (former Stack Overflow Employee & Community Leader). You can reach
me at `timthepost@protonmail.com` if you have questions.
