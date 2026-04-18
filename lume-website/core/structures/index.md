---
title: Structures & Types
parent: "Core Library"
nav_order: 2
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

# Main Structures

All structures in `splinter.h` are commented thoroughly; this "walk through"
documentation serves as a point of reference for the API documentation and as a
way to draw attention to important consideration when making changes.

> Changing structures also means needing to change client code or the build will
> break. { .warning }

If you embed Splinter and don't build the tools with your modifications, then
this is a non-issue, of course! However, many people don't realize the tools
that ship with splinter expect these structures to be as they are; if you intend
to keep in step with the tool development, it's recommended that you do not
modify these.

See the [inline documentation](https://github.com/splinterhq/libsplinter/blob/main/splinter.h) on Github for now.

When Splinter releases `1.2.0` I'll make web docs, once things have settled.  