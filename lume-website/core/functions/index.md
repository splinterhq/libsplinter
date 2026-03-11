---
title: Function Reference
parent: "Core Library"
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

# Splinter's Function Reference

**Splinter is designed to be gutted.** Have no mercy and throw out what's not
needed.

No, seriously - throw out _anything_ you don't actually need for your
application if keeping basic read / write functionality within instruction cache
really matters. The Splinter MVP (Minimal Viable Product) consists of:

- `splinter_open()`
- `splinter_create()`
- `splinter_get()`
- `splinter_set()`

Anything else is, essentially, a luxury. Note that even `splinter_close()` was
omitted because it's not strictly necessary for proper stable functionality. If
you remove things from the structures, double-check their byte-alignment as
staying 64 (this should be automatic thanks to macros, but a unit test will fail
if not). Just note that changing structures means needing to update everything
else.

## Build Configuration Matters

Some functions are not enabled unless Splinter is compiled with support for
embeddings, or support for Numa-pinning. However, we present them all and only
mention they are gated on their respective entry pages.

## Compiler Optimization Tweaks

Feel free to experiment, but avoid loop unrolling unless you specifically know
why it won't be problematic for cache timing.

Splinter is one of those very, very _special_ places where tiny optimizations
can result in a huge magnitude of performance improvement, but those moments
have mostly passed, so don't get stuck too far down a rabbit hole.

If you gain some advantages, please reach out via Github or email and let me
know!

## Please Be An Editor - Not A Critic

A lot of rote work went into creating this documentation so there's surely some
mistake or omission. Feel free to send a PR or open an issue to point it out. If
you have
