# Splinter's Executive Summary

Splinter acts as a ringless or single-ring, compute-less hypervisor where inference and governance swim in adjacent lanes in the same mediated, lock-free pool.

It offers the following functionality at L3 speeds, in a form that fits inside most CPU instruction hot paths:

 - Key/Value storage indexed by bloom, user-defined bitmask feature flags and user-defined namespaces along with references to the boundaries of the data in the shared, mediated pool,
 - Atomic bitwise and math operations in-place on BIGUINT keys,
 - Vector storage with side-car embeddings that run on the bus driven by signal events and eventfd-backed or poll-driven watches, 
 - Knowledge graph through 64 bloom tags and tandem slots that allow storing data at egdes, 
 - Practical, lock-free atomic access through sequence locks very much like those used in Xen (inspired by Keir's paper)
 - Semantically searchhable, instantaneous memory for agentic runtimes that are capable of issuing shell commands to interact with it.

In addition, it provides the following utilities:

 - `splinference` - An embedding inference engine that works through splinter (no sockets) and is managed by `systemd`
 - `splainference` - A completion / conversational inference engine that utilizes splinter (no sockets) and stores system prompts and in-process conversations in the store in a way that becomes immediately semantically-accessible to the model and is managed by `systemd`
 - `splinterctl / splinter_cli` - A production-quality mini-shell and REPL written in C specifically to provide the computational layer that completes Splinter, isolated from the core storage, in a way that can be accessed conveniently by humans and agents alike. `splinterpctl / splinterp_cli` are the persistennt versions. Think `iptables/ebtables-like` but also with an interactive modern CLI similar to Redis / Mongodb.
 - `sidecar` - A DevOps tool to monitor system usage and debug chatter on the bus while training.

Splinter has the ability to ingest and chunk arbritrary UTF-8 where the chunk size is simply controlled by the geometry of the bus (slot count and max value length per slot are defined on initialization). This content becomes available to the built in goemetry-driven semantic search which provides Euclidean distance as a faster approximator than approximate next neighbor (sorting by similarity first, then by magnitude), but can also easily implement any kind of next-neighbor search as a shard.

Backing Splinter's claims are plenty of tests and a full thesis paper:

 - `splinter_stress` - MRSW where many readers access while a single writer constantly updates. 3.2 million operations/second on an i3 Tiger Lake with 0 corruptions.
 - `splinter_chi_sao` - MRMW withh disjointed-lane contract where 32 writers pound on slots while readers consume the updates (_double sticky hands_). 15.6 million operations/second on an i3 Tiger Lake with 0 corruptions.

 - `splinter_thesis.pdf` - Contains the idea, rationale, planning, proposed governance and inference convergence, provides a working emotional valence classifier that can run during inference, along with the implementation of univocality in posix memory use advisement between cooperative elections and voluntary yielding between shards. 

Splinter supports SIMD payloads as keys via WASMEdge, and data transformation on I/O through Lua integration with the store. 

The current status of the research is:

 - Over 95% working and implemented with impressive test coverage.
 - Shard accounting & madvise univocality still need to go in,
 - `Splainference` needs to incorporate both the governance strategy as well as provide completion in RAG workflows, or narration for data analysis with data in the store,
 - SIMD workloads need more real world testing (currentlt only setting or getting is avilable to compiled WASM),
 - Meta-style Lua keys `__on_write, __on_read, etc` are an exercise for the client, but a standard base protocol should at least be defined in the paper,
 - A lot of documentation and a design-oriented approach to helping people understand it as a new kind of co-architecture. A "service lane" or the hallways behind the stores at the malls, are great metaphors that understate it just enough to make it comfortable while not concealing what it unlocks.

Splinter is expected to be 100% operational (but still needing documentation) by mid-May to early June 2026. Further public development is not planned after 1.2.0.