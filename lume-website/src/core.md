# Splinter's Core & Benchmarks

Philosophically, Splinter is built under the following constraints:

1. It is not a database; it is an active storage substrate. Clients do all
   calculating.
2. The main hot path of the code should strive to fit in a modern processor's
   instruction cache.

This set of constraints has kept the core library very lean, currently at around
~ 845 lines of actual code, less if you remove support for embeddings, even less
if you take out the bitwise operators if you don't need them, etc. It's designed
to be embedded and customized.

It's recommended that you remove whatever you don't require from the public API,
but leave the data structures as they are (aligned), or make very sure you
preserve the alignment if you change anything (a unit test will fail if you do).

## Benchmarks

All tests were conducted on a Tiger Lake (i3-1115G4) with 6GB total usable RAM.

| Test | Memory Model | Hygiene Model | Duration (ms) | Writer Backoff (us) | Threads | Operations/Sec | EAGAIN % | Corruptions |
| ---- | ------------ | ------------- | ------------- | ------------------- | ------- | -------------- | -------- | ----------- |
| MRSW | in-memory    | none          | 60000         | 0                   | 64      | 3.2mm          | 21.15    | 0           |
| MRSW | im-memory    | hybrid        | 60000         | 0                   | 64      | 3.2mm          | 21.52    | 0           |
| MRSW | in-memory    | none          | 60000         | 0                   | 32      | 3.2mm          | 21.15    | 0           |
| MRSW | im-memory    | hybrid        | 60000         | 0                   | 32      | 3.2mm          | 21.52    | 0           |
| MRSW | file-backed  | none          | 30000         | 150                 | 16      | 2.7mm          | 18.3     | 0           |
| MRSW | file-backed  | hybrid        | 30000         | 150                 | 16      | 2.7mm          | 18.3     | 0           |

<sub><em>`*` We expect fewer `EAGAIN` results with a 150us backoff because the
work just aligns with the seqlock (and lane) better, albeit with less velocity
than in-memory. See the `splinter_stress` tool that ships with Splinter for
more!</em></sub>

## Core API examples:

The best ways to see the public API being exercised is to look at
`splinter_test.c` as well as the source code to the individual CLI commands.
Splinter's CLi is designed to be a production-grade tool, no bones about it, but
it's also a useful resource to see how the code is intended for use.

Here are some examples so you can see if Splinter is something that would
interest you.

TODO: Coming Soon.

---
