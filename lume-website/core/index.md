---
title: Core Library
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

# Splinter's Core & Benchmarks

Splinter strongly separates storage and calculation concerns by design. Thus,
"core" in splinter is a relative term that always refers to `splinter.c` and
`splinter.h`; relative to how much of the included code is left.

A system that's "20% Core" has roughly 20% of the original code left in it after
removal of unused functions.

## Core Excludes All Tooling & Tests.

In _core_, we care about the size of the code that establishes and maintains the
store, not the code that consumes it. This means referring to core automatically
excludes the CLI, etc.

## Embedding Splinter's Core

You have the option to link against `libsplinter.a` or `libsplinter_p.a` if you
have them in your linker path. You also have the option of just adding
`splinter.c` as a dependency and including its header file, `splinter.h`, like
any other tiny C library.

It's recommended that you first make a copy of them and then _**take out**_ any
of the public API functions and forward declarations that you don't use for
maximum efficiency; the smaller it gets, the more likely it will be optimally
cached by the CPU.

Leave the originals as-is, so you can still build and use the CLI to manage the
stores. Additionally, **double check alignment at 64 bytes** (there's a test for
that) if you do anything to the slot structures.

## Real-World Commodity Hardware Benchmarks

All tests were conducted on a Tiger Lake (i3-1115G4) with 6GB total usable RAM
while performing other experiments (worst case workday):

| Test | Memory Model | Hygiene | Duration (ms) | W/Backoff (us) | Threads | Ops/Sec   | EAGAIN% | NumKeys | Corrupt |
| ---- | ------------ | ------- | ------------- | -------------- | ------- | --------- | ------- | ------- | ------- |
| MRSW | in-memory    | none    | 60000         | 0              | 63 + 1  | 3,259,500 | 23.72   | 20K     | 0       |
| MRSW | im-memory    | hybrid  | 60000         | 0              | 63 + 1  | 3,620,886 | 30.50   | 20K     | 0       |
| MRSW | in-memory    | none    | 60000         | 0              | 31 + 1  | 3,245,405 | 20.76   | 20K     | 0       |
| MRSW | im-memory    | hybrid  | 60000         | 0              | 31 + 1  | 3,273,807 | 20.60   | 20K     | 0       |
| MRSW | in-memory    | none    | 30000         | 0              | 15 + 1  | 4,094,896 | 31.94   | 10K     | 0       |
| MRSW | file-backed  | none    | 30000         | 0              | 15 + 1  | 4,768,989 | 29.63   | 10K     | 0       |
| MRSW | file-backed  | none    | 30000         | 150            | 15 + 1  | 2,992,652 | 13.94   | 10K     | 0       |
| MRSW | file-backed  | hybrid  | 30000         | 150            | 15 + 1  | 3,189,306 | 25.91   | 10K     | 0       |

Keys were limited in file tests due to space and wear constraints, but an
in-memory reference is provided.

## A Note On Keyspaces

The `key` part of `key -> value` really is the key to success in your schema (no
puns intended, honestly).

The most basic use of a key is something like `foo`. And if you used tandem
slots, you'd be able to access foo, its velocity, its acceleration and its
jitter through `foo, foo.1, foo.2` and `foo.3` respectively.

That's easy enough, but what if `foo` collided with something else named `foo`
that you also wanted to track? That's why it's handy to prepend some kind of
namespace prior to the key. The author uses something like this:

`type_name::experiment_name::run::variable::`

Just .. _be kind to your future self when naming keys_ and remember you can
change the order access operator if needed. You can change it to something that
won't appear in your normal data, like 💩; the separator is stored as
`SPL_ORDER_SEPARATOR` in `splinter.h`.

A little levity can make drab analysis marathons more tolerable. Send in the
poop, the clowns, the beds, or whatever it takes in the name of discovery.
