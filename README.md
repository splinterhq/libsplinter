# Splinter: A Vector Anti-Database & Shared-Memory Substrate

Splinter is a minimalist, lock-free key-value manifold designed to facilitate
high-frequency data ingestion and retrieval across disjointed runtimes. It is
built on the belief that for local inter-process communication (IPC), the
kernel’s networking stack is an expensive and unnecessary middleman.

### Design Philosophy: The Natural Settling of Frugality is Speed!

Modern software has become arrogant, assuming that CPU cycles and memory
bandwidth are infinite. We invoke help from the kernel's socket layer to
transfer a value that we already have in memory to somewhere else in memory as
standard practice.

And, now we're doing that with 768-dimensional vectors. Splinter is a gesture
back in the direction of adulthood for systems development. Here's a list of the
main things that set it apart (_**aka: why Splinter is do damn fast**_):

- **Splinter Is a Passive Substrate**: Splinter is not a daemon. It is a
  memory-mapped region that acts as a mutual option for every process on the
  system.
- **(DRYD) Zero-Copy Intent**: _(D)on't (R)epeat (Y)our (D)ata_ In Splinter,
  information is never "sent"; it is published. Readers access the raw memory
  directly, crossing only the minimal checkpoints needed for safe coordination,
  eliminating the energetic tax of serialization and context switching.
- **Static Geometry**: By using a fixed-geometry arena, Splinter eliminates the
  "learned negligence" of dynamic heap fragmentation and background garbage
  collection.
- **Lock-Free Practicality**: No time is wasted acquiring mutex locks; Splinter
  uses standard portable atomic sequence locks.
- **In-Place Atomic Operations**: `INCR/DECR` as well as `AND`, `OR`, `XOR` and
  `NOT` happen in-place, atomically.
- **Persistent or RAM-only**: Splinter persists easily to/from disk using the
  included CLI or just regular Unix tools like `dd`.
- **Lua Integration**: `splinter_cli` and `splinterctl` both feature easy lua
  scripting for data transformation.
- **Doesn't Corrupt**: Doesn't require checking, operates cleanly.
- **Unopinionated**: Implement LRU or TTL eviction how you like in a loadable
  shard, or no eviction at all. Splinter doesn't care.
- **Scales Across RDMA**: Splinter uses UNIX permissions and scales "out" easily 
  so that other systems on the same RDMA network can access it at near-local
  speeds too. The same seqlock protection covers it all.

### The "Good Citizen" Protocol

Splinter assumes **informed intent**. It does not try to outsmart the kernel
with `O_DIRECT` or complex paging logic. It provides the metadata (`ctime`,
`atime`, `epoch`) and the memory region, then gets out of your way. It is a tool
for engineers who would rather spend their thermal budget on the math, not the
management.

### Comparison: The Anti-Database Edge

| Feature        | Splinter               | Traditional Vector DBs   |
| -------------- | ---------------------- | ------------------------ |
| **Transport**  | `mmap()` (Local)       | TCP/gRPC (Network Stack) |
| **Daemon**     | None (Passive)         | Active Service (Heavy)   |
| **Footprint**  | Static & Deterministic | Dynamic & Volatile       |
| **Complexity** | 874 Lines of C         | 100k+ Lines of Code      |

### Exhaustive Feature Overview

#### 1. Performance & Scale

- **Throughput**: Validated at **3.2M+ ops/sec** on early-gen, throttled
  consumer hardware.
- **Latency**: Resolves at L3 cache speeds by utilizing `memfd() / mmap()` for
  the primary transport.
- **Scalability**: Supports disjointed MRMW (Multiple Reader, Multiple Writer)
  semantics via per-slot atomic sequence locks.

#### 2. Vector & Math Native

- **Dimensional Storage**: Native support for **768-dimension vectors**
  (optimized for Nomic v2/LLM embeddings).
- **In-Place Atomic Ops**: Keys tagged as `BIGUINT` support atomic `INC`, `DEC`,
  `OR`, `XOR`, `AND`, and `NOT` operations directly in shared memory.
- **Tandem Keys**: Multi-order support allows for atomic updates to related
  signals (e.g., `sensor.1`, `sensor.2`).

#### 3. Mechanical Hygiene (Auto-Scrubbing)

Splinter offers three levels of sanitation to balance data integrity with
computational thermodynamics:

- **None**: Minimal movement; assumes readers strictly honor value length
  metadata.
- **Hybrid (Fast Mop)**: Zeroes the incoming byte length plus a **64-byte
  aligned** tail to satisfy SIMD/Vectorized loads.
- **Full (Boil)**: Zeroes the entire pre-allocated value region, ensuring
  absolute isolation for verifiable research.

#### 4. The Signal Arena

- **Pulse Groups**: Up to 64 independent signal groups for `epoll()`-backed
  notifications.
- **Bloom Labeling**: High-performance key tagging allows watchers to filter for
  specific signal "vibrations" without scanning the entire store.

#### 5. Extensibility (Loadable Shards)

- **Modular Logic**: Side-load specialized C logic (DSP, ANN search, Inference)
  via `insmod`.
- **Zero Core Bloat**: Keeps the core library under **1,000 LOC**, ensuring the
  hot path stays in the instruction cache.

#### 6. Loose Type Naming System

Splinter allows you to "inform" a slot of the type of key it will be hosting.
This has certain benefits, like:

1. Automatically converting key bit-depth on conversion to unsigned integers
   (`BIGUINT`)
2. Knowing which keys are eligible for embeddings (`VARTEXT`)
3. Knowing which keys contain serialized data (`JSON`)
4. Knowing which keys can't be printed to the console (`BINARY`, `IMGDATA`,
   `AUDIO`, `VIDEO`)

These can be extended (or replaced entirely) at the user's discretion.

### The Main Splinter Use Cases (What's it Good For?)

Splinter can be anything from a simple configuration or KV store to a Rank - 2
tensor model scaffold. It's designed for **vector-heavy workflows** like those
typically found in Artificial Intelligence (AI) inference and training or
high-resolution physics and linguistic research (anything that benefits from
easy semantic relation).

The following cases stand out as those that the author himself uses Splinter for
right now, or specifically wrote Splinter to handle with competence and ease:

#### Sociophysics Research

Splinter allows up to 64 signal groups per bus along with ctags-style labeling
and selection through built-in per-slot bloom filters. It also attaches a number
of user- defined feature flags per slot for convenience, integrates easily with
`btrfs` snapshot schemes and is built around the idea of capturing raw data
exceptionally well while making backfill easy, even with temporal offset.

#### LLM Orchestrated Memory

Splinter functions remarkably well as semantic short -> long-term memory for
large language models. LRU-based movement helps "forget" ephemera quickly while
making sure stuff that actually matters (as viewed by access time and epoch)
stays in short term memory a little longer and eventually settles into
long-term.

You can also run inference directly on the bus, accessing the embeddings using
Splinter's supervised raw pointers and epoch counter, so embedding operations do
not require `memcpy()` operations. Splinter can significantly reduce inference
loads.

#### Configurations And Registries

Splinter's epochs and feature flags lend very well to application configuration
on Linux systems, and you can wrap REST endpoints around them to expose it to
management tools like Configu or others.

#### Embedded Use

Great for environmental loggers or system ring buffers because it's much better
for flash-based storage than relational databases, thanks to static geometry.

Splinter is small enough at 875 lines of code that it stays in the "hot path"
for most modern processors.

### Comparison With Other Stores

It's not fair to other stores to compare them competitively against Splinter
because they'd be competing with their "hands tied" due to mutexes and
socket layers. And, it's not fair to Splinter, because Splinter deliberately 
eschews any calculation on-write that it can't do with simple bitwise math; 
Splinter ***tries*** to stay boring.

As far as feature references go, Splinter is somewhat like Redis in how it 
stores data and the kind of data it stores, but doesn't have nearly as much of
an advanced concept of types, or even really enforceable types, mostly because
it just doesn't need them. Splinter is designed to be used ***with*** other
things, not always necessarily in lieu of them.

Splinter works perfectly fine on DenoKV with FFI, but it's local-only to whatever
isolate its running on, so it's definitely no replacement for DenoKV (it's just 
a good caching system in that scenario).

Thinking in terms of other stores limits how you'll think to use Splinter. Think
about what you can do once TypeScript, Rust, Python and Go can all share the same
address space and embeddings **safely**, without socket or even `memcpy()` overhead 
instead `:)`.

### Contact The Author

I'm Tim Post (former Stack Overflow Employee & Community Leader). You can reach me
at `timthepost@protonmail.com` if you have questions.
