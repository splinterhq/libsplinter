# Splinter 🐀 (v1.0.0)

**Splinter** is a minimalist, high-speed lock-free message bus and subscribable
KV store. It operates in shared memory (`memfd`) or uses memory-mapped files for
persistent backing. It is a shared-memory manifold for when you need to move
data between processes faster than the Linux kernel usually wants to let you.

As a vector store, Splinter is approximately **16x faster than Redis** and **33x
faster than SQLite3** `:memory:`. On production-grade hardware, Splinter 1.0.0
pushes **3.39 million ops/sec** under extreme contention (128 threads) with
**zero integrity failures**.

## 🚀 1.0.0 Performance Benchmarks

Recent stress tests on 1.0.0 demonstrate the efficiency of the Seqlock and
64-byte alignment:

- **Throughput**: 3,397,771 ops/sec.
- **Contention**: 128 concurrent threads (127 readers, 1 writer).
- **Efficiency**: Only 29.2% retry rate (EAGAIN) under maximum load.
- **Reliability**: 0 integrity failures across 200 million+ operations.

## 🧠 For Researchers: The Sociophysics Workflow

Splinter was developed to treat social data like a **magnetorheological fluid**.
It is designed to capture "live" vibrations and allow for background "processed"
narrative backfills.

- **Tandem Slots**: Store and query multi-order vector data (velocity,
  acceleration) with a single call.
- **Signal Arena**: Watch up to **64 signal groups** simultaneously with
  `epoll()`-backed event watchers.
- **Bloom Labels**: Populate watches with easy Bloom filter tags for
  high-resolution event markers.
- **Temporal Compensation**: Update timestamps (`ctime`, `atime`) independently
  to compensate for temporal jitter during ingestion.

- **Toggle-able Auto Scrub**: One config call causes splinter to   zero-out the max
  length of each slot arena offset on every new wr ite or update, at the cost of a 
  memset during the sequence lock. 
  
  It can be toggled on or off instantly as-neede because it provides hygiene, not 
  reclamation. At the cost of lower performance,splinter "boils" key slots just 
  before new data lands. If only hotels could do that for rooms ...

## ✨ Key Features

- **Zero Noise**: Speaks only when spoken to (SLNT).
- **No Maintenance**: No daemons, no background "vacuuming," and no cron jobs.
- **Atomic Ops**: Bitwise operations and INCR/DECR are performed atomically
  in-place.
- **Memory Efficiency**: Safe shared pointers to vector storage allow clients to
  avoid `memcpy()`.
- **Lua & Rust Integration**: Ships with a production-quality CLI and a
  fully-exported Lua bridge.

## 🛠 Quick Orientation (CLI)

```bash
make
sudo make install
splinterctl init demo
# New in 1.0.0: Manage multi-part tandem orders
splinterctl orders set sensor_alpha 3 "v_data"
# Monitor signal groups via Lua
splinterctl lua monitor_tension.lua
```

You then want to explore the C API, then Lua / TypeScript & other bindings.

## 📋 System Requirements

- **OS**: GNU/Linux (Debian recommended).
- **Compiler**: GCC 12.1+ with glibc 2.35+.
- **Dependencies**: None (Optional: `libnuma` for NUMA affinity, Lua 5.4,
  Valgrind).
