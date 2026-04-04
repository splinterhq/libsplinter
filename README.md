# Splinter ⚡ L3-Speed Shared Memory Cache, Vector, Context, Semantic Transcript, KV Store & Sidecar Inference Pipeline

Most AI stacks connect their components with the equivalent of garden hoses:
sockets, gRPC calls, serialization overhead, kernel arbitration. Every piece
of context your inference stack needs gets pumped through that plumbing, one
request at a time.

## Splinter Provies "Tightly-Coupled" Inference & Governance Oversight

Splinter takes the opposite approach. Put your processes in the same pool.
Let them share memory directly, without copying, without sockets, without a
daemon in the way. Writers publish; readers observe. The kernel only gets
involved when absolutely necessary, and even then, only to deliver a wake-up
signal, not to arbitrate a transaction.

Link nearby pools with RDMA for near-local speeds across machines. Bridge
distant pools asynchronously with REST; Splinter's persistence model makes
that straightforward. The core stays simple regardless of how far you scale.

---

## What You Get In the Pool

All of the following live in the same NUMA-aware shared memory region, with
a core that fits in your CPU's instruction cache:

- **Key-value storage** with per-slot feature flags, bloom labels, and epoch
  tracking. Useful for building context windows, routing tables, or any
  structure where you need to filter and observe without scanning everything.
- **Atomic integer operations**: `INC`, `DEC`, `OR`, `XOR`, `AND`, `NOT`
  directly in shared memory, without locking.
- **768-dimension vector storage**, natively sized for Nomic and compatible
  LLM embeddings, in the same slot as your key-value data.
- **Tandem keys** for multi-order signals on the same base key. Velocity and
  acceleration from the same sensor, or text and image embeddings in the same
  conceptual slot, live in the same vector space without coordination overhead.
- **Sidecar inference** via `splinference`, a small C++ daemon that watches
  a signal group, calls Nomic Text locally via llama.cpp, and writes the
  resulting embedding back to the slot. Zero socket overhead, fully
  asynchronous from the writer's perspective.

---

## Understanding Splinter Visually: The Pool

Because there is nothing quite like Splinter in the ecosystem, a concrete
mental model helps before the technical details do.

Imagine `splinter_create()` returned an Olympic-sized swimming pool divided
into perfectly equal lanes.

**Pre-Allocated Lanes (Zero Overhead).** Instead of building a new pool for
every guest, one large pool is built at startup and divided into fixed lanes.
If you need a key, you do not request it from a lifeguard (the OS). You look
directly at Lane 4. The memory is already mapped to your process; there is
nothing to wait for.

**The Diving Boards (Lock-Free Access).** Each lane has a diving board. Up to
32 writers can enter their own lanes simultaneously without collision. If two
writers try to hit the same lane at once, the second one bounces back with
`EAGAIN` and retries in nanoseconds. Nothing stops, nothing blocks.

**The Signal Pulse.** When a writer enters Lane 1, a ripple travels across
the connected water. An inference sidecar sitting at the far end does not poll
constantly; it waits for the ripple. The moment it arrives, the sidecar knows
exactly which lane changed and acts immediately. This is the signal arena:
`epoll`-backed, bloom-filtered, with up to 64 independent groups.

**No Jumping Out (Zero Copy).** Traditional systems pull data out of storage,
serialize it, carry it across a socket, and deserialize it on the other side.
In Splinter, the reader is already in the water. Access is a pointer
dereference, not a round trip.

---

## Splinter as Semantic Breadboard

The pool analogy explains the mechanics. The more useful mental model for AI
and research work is a semantic breadboard.

A breadboard lets you prototype circuits by pushing components into a shared
substrate. No soldering, no permanent commitment, full observability of every
signal. Splinter works the same way for processes: push your keys into the
shared region, wire up signal groups and bloom labels to route notifications,
attach sidecars for inference or analysis. Reconfigure without restarting
anything.

This makes Splinter well-suited for:

- Local LLM inference pipelines where latency between tokenization, embedding,
  and retrieval is a bottleneck.
- Physics and linguistic research where multiple processes need to observe and
  update a shared state without coordination overhead.
- Any system where you want to build first and enforce structure later,
  because Splinter's type system is advisory rather than enforced.

---

## Feature Overview

### Performance and Scale

- **Throughput:** Validated at 3.2M+ ops/sec on throttled consumer hardware
  (the kind found in underfunded physics labs, which is most physics labs).
  With NUMA pinning on modern hardware, near 500M ops/sec is achievable.
- **Latency:** Resolves at L3 cache speeds via `memfd()` with graceful
  fallback to `mmap()`.
- **Concurrency:** MRMW (Multiple Reader, Multiple Writer) semantics via
  per-slot atomic sequence locks. No global mutex, no daemon bottleneck.

### Storage and Types

- **Static geometry:** Fixed-size slots eliminate heap fragmentation and
  garbage collection pauses. The arena is deterministic from creation.
- **Loose type naming:** Slots can be declared as `VARTEXT`, `BIGUINT`,
  `JSON`, `BINARY`, `IMGDATA`, `AUDIO`, or `VIDEO`. Types inform behavior
  (embedding eligibility, integer expansion, print safety) without enforcing
  schema.
- **In-place atomic ops:** `BIGUINT` slots support all six arithmetic and
  bitwise operations directly in shared memory.
- **Tandem keys:** Multi-order key notation (`sensor.1`, `sensor.2`) allows
  atomic grouping of related signals without a separate coordination layer.

### Signals and Observability

- **Signal arena:** Up to 64 independent `epoll`-backed signal groups.
  Writers pulse groups; readers wake immediately.
- **Bloom labeling:** 64-bit per-slot bloom filters allow watchers to filter
  for specific labels without scanning the full store.
- **Epoch tracking:** Every slot carries a monotonically increasing epoch.
  Readers can detect changes without polling values. The global header epoch
  tracks bus-level activity.
- **Slot timing:** `ctime` and `atime` per slot, with writer-supplied
  processing delta for audit and governance use.

### Hygiene and Safety

Splinter offers three scrub levels to balance integrity with throughput:

- **None:** Assumes readers honor value length metadata. Maximum speed.
- **Hybrid (Fast Mop):** Zeroes the incoming byte length plus a 64-byte
  aligned tail. Satisfies SIMD and vectorized loads without a full zero.
- **Full (Boil):** Zeroes the entire pre-allocated value region. Required
  for verifiable research where slot isolation must be absolute.

The seqlock protocol (`epoch` odd = write in progress, even = stable)
ensures readers always get a consistent snapshot or an explicit `EAGAIN`
to retry. There is no silent corruption path.

### Inference Sidecar (Splinference)

`splinference` is a C++ daemon that attaches to a Splinter bus and watches a
signal group. When a `VARTEXT` key is written, the sidecar tokenizes the
value, runs it through a quantized Nomic Text model via llama.cpp, and writes
the resulting 768-dimensional embedding back to the same slot via
`splinter_set_embedding()`. The writer never blocks. The sidecar operates
entirely asynchronously.

Backfill mode (`--backfill-text-keys`) will embed all existing `VARTEXT` keys
that have a zero vector, making cold-start and recovery straightforward.

### Extensibility

- **Loadable shards:** Attach specialized C logic (ANN search, DSP, custom
  eviction) without modifying the core. The core stays under 1,000 lines and
  remains in the instruction cache.
- **Lua integration:** Both `splinter_cli` and `splinterctl` support Lua
  scripting for data transformation and automation.
- **NUMA affinity:** Optional `libnuma` integration pins the arena to a
  specific socket. Combined with writer affinity, this is where the 500M
  ops/sec figure comes from.
- **RDMA scaling:** The value arena maps cleanly onto `ibv_*` primitives.
  Hardware to validate the full implementation is the only missing ingredient;
  the protocol surface is small (estimated 200 lines).
- **Persistence:** Save and restore with the CLI or standard Unix tools like
  `dd`. The on-disk format is the in-memory format.

---

## Comparison With Related Tools

Comparing Splinter directly to active vector databases is not quite fair in
either direction: they are competing with their hands tied by socket layers
and mutex overhead, and Splinter deliberately avoids features they provide
(query planning, distributed consensus, schema enforcement). They are different
tools for different constraints.

| Feature        | Splinter                                          | Traditional Vector DBs   |
| -------------- | ------------------------------------------------- | ------------------------ |
| **Transport**  | `memfd()` / `mmap()` (L3 Speed)                   | TCP/gRPC (Network Stack) |
| **Daemon**     | None (Passive Substrate)                          | Active Service (Heavy)   |
| **Footprint**  | Static and Deterministic                          | Dynamic and Volatile     |
| **Complexity** | ~890 lines of C; hard ceiling of 999              | 100k+ Lines of Code      |
| **Eviction**   | Your choice, via loadable shard                   | Built-in, opinionated    |
| **Embedding**  | Async sidecar, zero socket overhead               | Network round-trip       |

Splinter works with other tools rather than replacing them. It is a natural
fit as a high-speed local cache or semantic bus in front of a persistent store
that handles durability and distribution.

---

## Supported Platforms

Any modern GNU/Linux flavor. WSL works with a minor penalty. macOS requires
shimming around the absence of `memfd` but should otherwise function. RDMA
support is architecture-ready, pending hardware access for validation.

## Optional Build Dependencies

None of these are required. Splinter builds and runs without them.

- `libnuma-dev` for NUMA affinity: `WITH_NUMA=1`
- `lua5.4-dev` for Lua scripting: `WITH_LUA=1`
- llama.cpp for the inference sidecar: `WITH_LLAMA=1`
- `libvalgrind-dev` for tighter Valgrind integration: `WITH_VALGRIND=1`
- Embedding storage can be disabled entirely for lean configurations:
  `WITH_EMBEDDINGS=0`

---

## Documentation

Full documentation is available at [splinterhq.github.io](https://splinterhq.github.io).

## Contact

Tim Post, former Stack Overflow Director of Community Strategy. Reach me at
`timthepost@protonmail.com` with questions, RDMA hardware offers, or
interesting use cases.
