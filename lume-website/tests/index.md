---
title: Testing Splinter
nav_order: 5
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

# Testing Splinter

You can run Splinter's tests using `make tests`. If you have enabled Valgrind
integration, splinter's unit and integration tests will also be scrutinized for
leaks and access errors. For less than 1k lines of total core code, Splinter is
extremely well-tested. Splinter uses CMake's test runner with a few scripts at
the end.

Performance benchmarks can be obtained with `splinter_stress` and
`splinterp_stress` for in-memory and file-backed respectively, as shown above.
