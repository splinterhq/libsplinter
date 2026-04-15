# Splinter ⚡ L3-Speed Shared Memory Vector & KV Store For Tight Inference Deployments

Splinter is a minimalist, persist-able lock-free and bloom-able key-value store 
designed to handle high-frequency data and vector ingestion/retrieval across 
disjointed runtimes, and to provide latency-free transparency into LLM inference
on both a token and vector scale.

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

## Quick Start:

Splinter uses CMake, so please make sure you have that installed. But, you don't have
to manage configurations manually. There's an included `./configure` style script:

```bash
git clone git@github.com:splinterhq/libsplinter.git
cd libsplinter
./configure --with-embeddings
make
sudo -E make install
```

You can use `configure --help` for  more options. See below for packages Splinter
can link against and use.

## The Swimming Pool Analogy: Understanding Splinter "Visually"

There is nothing quite like Splinter, so it helps to be able to visualize the
architecture so you see how the features work together.

Imagine you aren't just storing data; you are designing a high-performance 
Olympic-sized swimming pool. In fact imagine `splinter_create()` returned 
a multi-lane pool that rivals any YMCA.

But ... with some twists.

### Pre-Allocated Lanes (Zero Overhead)

Instead of building a new pool every time you have a guest (dynamic allocation), 
we build one massive pool at the start. We divide it into perfectly equal lanes. 
If you need a key, you don't "request" it from a lifeguard (the OS/Kernel); 
you simply look at Lane 4. You have a direct line of sight to the water 
because the memory is already mapped to your process.

### The Diving Boards (Lock-Free Access)

Each lane has a diving board. Because of Splinter's atomic sequence epochs, 
32 divers  can jump into their own lanes at the exact same time. They never collide. 
If two divers try to hit the same lane simultaneously, the second one doesn't 
crash; they simply "bounce" back to the board (`EAGAIN`) and try again a 
nanosecond later. No one ever stops the flow of the meet to wait for a key.

### The Signal Pulse (IPC answerbox-style signaling / Agentic Coordination)

Now, imagine the water is connected. When a diver hits the water in Lane 1, 
a ripple (signal) travels instantly across the surface. A coach 
(inference) sitting at the far end doesn't have to keep staring at the lane; 
they just wait to feel the ripple. The moment they feel it, they know exactly 
which lane to look at. 

### No Jumping Out (Zero Copy)

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

And lately, we're repeating this practice by adding vector embeddings - let's think
more about what we actually need in this process. Splinter is a gesture back in the direction of efficiency for systems development. 

Here's a list of the main things that set it apart:

- **Splinter Is a Passive Substrate**: Splinter is not a daemon. It is a
  memory-mapped region that acts as a mutual option for every process on the
  system.

- **(DRYD) Zero-Copy Intent**: _(D)on't (R)epeat (Y)our (D)ata_ In Splinter,
  information is never "sent"; it is published. Readers access the raw memory
  directly, crossing only the minimal checkpoints needed for safe coordination,
  eliminating the energetic tax of serialization and context switching.

- **Static Geometry**: By using a fixed-geometry arena, Splinter eliminates the
  problems created by dynamic heap fragmentation and background garbage
  collection.

- **Lock-Free Practicality**: No time is wasted acquiring mutex locks; Splinter
  uses standard portable atomic sequence locks.

- **In-Place Atomic Operations**: `INCR/DECR` as well as `AND`, `OR`, `XOR` and
  `NOT` happen in-place, atomically.

- **NUMA Compatible**: Splinter can optionally utilize NUMA pinning on more
  modern hardware that, if combined with writers using the same affinity, could
  reach write speeds of near 500MM ops/sec.

- **Persistent or RAM-only**: Splinter persists easily to/from disk using the
  included CLI or just regular Unix tools like `dd`.

- **Inference Included**: Splinter includes a "sidecar" embedding engine that 
  works asynchronously in the store on a signal group, utilizing a quantized
  version of Nomic Text (`.gguf`) with a tiny llama.cpp wrapper.

- **Lua Integration**: `splinter_cli` and `splinterctl` both feature easy lua
  scripting for data transformation.

- **WASM Integration**: Run WASM directly on the bus (get/set exposed so far)
  via WASMEdge embedding client-side.

- **Doesn't Corrupt**: Doesn't require checking, operates cleanly.

- **Unopinionated**: Implement LRU or TTL eviction how you like in a loadable
  shard, or no eviction at all. Splinter doesn't care.

- **Scales Across RDMA**: Splinter uses UNIX permissions and scales "out" easily
  so that other systems on the same RDMA network can access it at near-local
  speeds too. The same seqlock protection covers it all.

- **Comes With Inference**: Run any `.GGUF` model right on the store with inference
  tuned to signal groups and directly-accessible keys instead of sockets. Includes
  almost-production-ready example code (based on the llama.cpp libraries).

- **Ships With Debugging Tools**: Splinter's `sidecar` monitors let you keep an eye
  on critical system metrics while observing maintenance chatter in real-time on 
  your bus.

### The "Good Process" Approach:

_(Even though technically only the CLI or client code is the process because
Splinter itself is just a place, not a process)_

Splinter assumes **informed intent**. It does not try to outsmart the kernel
with `O_DIRECT` or complex paging logic. It provides the metadata (`ctime`,
`atime`, `epoch`) and the memory region, then gets out of your way. It is a tool
for engineers who would rather spend their thermal budget on the math, not the
management.

Relational databases attempt to shield themselves from the kernel trying to be
the kernel. Splinter goes out of its way to not bother the kernel unless it
must, and its logic shards inform the kernel of how the memory is intended for
use at every step of the way.

You can read more on 
[why Splinter and Linux get along](https://splinterhq.github.io/splinter_and_linux/) 
so well on Splinter's doc site, as well as 
[an overview](https://splinterhq.github.io/splinter_performance/) on why
the gains are so appreciable (tl;dr: it's practical computational physics, not just 
code efficiency). 

### Comparison With Related Tools:

| Feature        | Splinter                                                       | Traditional Vector DBs   |
| -------------- | -------------------------------------------------------------- | ------------------------ |
| **Transport**  | `memfd()` that degrades gracefully to `mmap()` (L3 Speed)      | TCP/gRPC (Network Stack) |
| **Daemon**     | None (Passive)                                                 | Active Service (Heavy)   |
| **Footprint**  | Static & Deterministic                                         | Dynamic & Volatile       |
| **Complexity** | 890 Lines of obsessively-optimized C (Will never exceed 999) | 100k+ Lines of Code      |

It's not fair to other stores to compare them competitively against Splinter
because they'd be competing with their "hands tied" due to mutexes and socket
layers. And, it's not fair to Splinter, because Splinter deliberately eschews
any calculation on-write that it can't do with simple bitwise math; Splinter
_**tries**_ to stay boring.

As far as feature references go, Splinter is somewhat like Redis in how it
stores data and the kind of data it stores, but doesn't have nearly as much of
an advanced concept of types, or even really enforceable types, mostly because
it just doesn't need them. Splinter is designed to be used _**with**_ other
things, not always necessarily in lieu of them.

Splinter works perfectly fine on DenoKV with FFI, but it's local-only to
whatever isolate its running on, so it's definitely no replacement for DenoKV
(it's just a good caching system in that scenario).

Thinking in terms of other stores limits how you'll think to use Splinter. Think
about what you can do once TypeScript, Rust, Python and Go can all share the
same address space and embeddings **safely**, without socket or even `memcpy()`
overhead instead `:)`.

### Supported Platforms:

Splinter is designed to work on any modern GNU/Linux flavor. Windows users could
consider using WSL with a slight penalty; MacOS would require some questionable
shimming around the lack of `memfd` (You'd have to somehow force anonymous file
descriptors to work), but it _should_ otherwise work perfectly.

### Optional Linkage:

These are _not_ required - but Splinter can use them if they're installed and
you enable them during the build:

- NUMA (`libnuma-dev`) for NUMA affinity | `WITH_NUMA=1` during build
- LUA (`lua5.4-dev`) for LUA integration | `WITH_LUA=1` during build
- llama.cpp ([Github](https://github.com/ggml-org/llama.cpp)) if you want to
  enable the nomic inference shard | `WITH_LLAMA=1` during build
- Valgrind (`libvalgrind-dev`) for tighter Valgrind test integration |
  `WITH_VALGRIND=1` during build
- WASM (Via WasmEdge). You'll need [their code](https://github.com/WasmEdge/WasmEdge) 
  as well as the `wabt` system package (available via `apt-get`)

Splinter can be configured to just be KV (no space partitioned for embeddings)
by passing `WITH_EMBEDDINGS=0` to the build command (for very lean
configurations). To generate this configuration automatically, simply run `./configure`
with no other arguments. Valgrind will still be auto-detected.

### Exhaustive Feature Overview:

#### 1. Performance & Scale

- **Throughput**: Validated at **3.2M+ ops/sec** on early-gen, throttled
  consumer hardware commonly found in data loggers and underfunded physics labs,
  which are the majority of physics labs.
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
  signals (e.g., `sensor.1` for velocity of `sensor`, `sensor.2` for acceleration).
- **Lua** And **WASM**: Client-side exposure of Splinter to WASMEdge and Lua5x.

#### 3. Mechanical Hygiene (Auto-Scrubbing)

Splinter offers three levels of sanitation to balance data integrity with
computational thermodynamics:

- **None**: Minimal movement; assumes readers strictly honor value length
  metadata.
- **Hybrid (Fast Mop)**: Zeroes the incoming byte length plus a **64-byte
  aligned** tail to satisfy SIMD/Vectorized loads.
- **Full (Boil)**: Zeroes the entire pre-allocated value region, ensuring
  absolute isolation for verifiable research. If only hotels could do this
  to their rooms between guests `:D`

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

Have a look at [use cases on Splinter's documentation site](https://splinterhq.github.io/use-cases/)
for the most up-to-date information (Splinter is under active development).

### RDMA Scaling

Splinter works (in theory) perfectly well over RDMA. The value arena is easily
exported via `ibv_*`, but the author lacks the hardware to bake the actual
implementation in (even simulations exceed what I have access to currently).

It *should not* be hard to map the arena, plus slot metadata, and most globals
that make sense to be shared. There's some timing and accounting to do 
(maybe 200 LOC? That sounds actually about right). We just want to avoid excess
traffic and "shivering" on the bus. But it's a half-day project, most likely.

If you have hardware to spare, please reach out!

## Documentation

Splinter has a [work-in-progress documentation site](https://splinterhq.github.io).

### Contact The Author

I'm Tim Post (former Stack Overflow Employee & Community Leader). You can reach
me at `timthepost@protonmail.com` if you have questions.
