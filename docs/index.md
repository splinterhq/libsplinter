---
title: "Splinter Overview"
date: 2026-06-30
updated: 2026-06-30
---

# Splinter - A Breadboard For Complex Vector Workloads

**A Cooperative Userspace Hypervisor for Inference & Other Semantic Workloads.**

Local LLM inference chokes on the "Socket and Lock" tax — the context switches,
serialization, and kernel interrupts that standard IPC tools and databases demand
just to synchronize state. At token speeds, that overhead isn't a bottleneck, it's
a wall.

Splinter dismantles that wall. It is a lock-free, cooperative userspace hypervisor
built for strict mechanical sympathy with modern CPU cache hierarchies (x86_64,
ARM, and RISC), putting your governance, your vector storage, and your inference
engine in the exact same physical memory lane. Think of it as a **semantic
breadboard**: a passive, shared-memory manifold where thousands of context or
classification windows run simultaneously with 100% non-blocking throughput. There
is no central arbiter, no socket, and no copy — reads and writes happen at L3 cache
speeds, directly on the hardware bus.

If this is your first stop, you're welcome here. Splinter is open source, actively
researched, and built in the open. Whatever brought you — embedding the library,
driving the CLI, wiring up another language, or just understanding how a lock-free
shared-memory substrate holds together — start with the section that fits.

## Reference Documentation

- [**API Reference**](api/index.md) — every public function in `splinter.h`, grouped
  by purpose: store lifecycle, key/value operations, the seqlock epoch protocol,
  Bloom labels, the signal arena and event bus, and the Logic Shard election that
  governs cooperative `madvise`.
- [**CLI Reference**](cli/index.md) — every command for `splinter_cli` (the
  interactive REPL) and `splinterctl` (one-shot shell invocation), from `get`/`set`
  to `shard`, `ingest`, and semantic `search`.
- [**Language Bindings (FFI)**](bindings/index.md) — Splinter is a very
  `dlopen()`-friendly library, so any runtime that can read a Linux DSO joins the
  same manifold as a first-class peer: Rust, TypeScript (Deno / Bun), Python, Java,
  Lua, and even POSIX shell.
- [**Environment Variables**](environment.md) — runtime knobs honored by both the
  library and the CLI, including `SPLINTER_DEFAULT_UMASK` (permissions of newly
  created stores) and `SPLINTER_NS_PREFIX` (CLI key namespacing).

## The Design, In Depth

Every architectural decision in Splinter — the ringless privilege topology,
the seqlock that lets readers never block writers, the cooperative bid table that
replaces preemptive interrupts, the mop modes that keep stale bytes out of raw
reads — is explained in detail, with its rationale, in the thesis:

- [**Splinter: The Thesis**](splinter_thesis.pdf) — the full design treatment and
  the reasoning behind each decision.

## Collaborating

Contributions of every kind are welcome — code, docs, benchmarks, bindings, bug
reports, and grounded "oddball" ideas. Please read `CODE_OF_CONDUCT.md` and
`CONTRIBUTING.md` before opening a pull request. The maintainer is a brain-cancer
survivor doing active research, so pull requests may surge forward and then go quiet
for a while; that's normal here, and GitHub issues and pull requests are the way in.

---

## How to Cite

If Splinter is useful in your work or research, please cite it:

```
Splinter: A Cooperative Userspace Hypervisor for Inference & Other Semantic Workloads.
Tim Post. https://splinterhq.github.io
```

Attribution and the latest documentation live at
[splinterhq.github.io](https://splinterhq.github.io).
