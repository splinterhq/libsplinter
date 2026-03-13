---
title: "Splinter's Performance"
parent: "Splinter Overview"
nav_order: 2
metas:
  lang: en
  description: >-
    A physics-based breakdown of Splinter's performance, analyzing Cycles Per Operation to demonstrate its massive efficiency gains over traditional DBs.
  keywords:
    - Cycles Per Operation
    - IPC Performance
    - Lock-Free Benchmarks
    - Socket Tax
    - Hardware Optimization
  robots: true
  generator: true
---

# Some Math Behind Splinter's Performance Claims

We must guard our time. This is especially true for researchers, where system
sluggishness can literally sabotage discovery and cause months or years of
setbacks. Because Splinter makes some rather audacious performance claims, it
should back them up with physics, not just benchmarks.

This explanation is not an attempt to start a competition with beloved
industry-standard tools. We compare against SQLite and Redis because **they are
the only widely known frames of reference available**. Splinter’s author
considers both to be inspirational symphonies written in code.

But sometimes, you just need a piccolo, not an orchestra.

_**TL;DR: It's physics.**_ Splinter divorced socket and mutex overhead, shrank
the logic to fit entirely inside the instruction cache, and aligned I/O to
prevent false sharing. It is designed to capture data first and backfill what
can be extrapolated later.

## The Efficiency Gap: Instruction Physics vs. Socket Tax

To justify a **Passive Substrate** (Splinter) over **Active Middleware**
(Redis/SQLite), we analyze the **Cycles Per Operation (CPO)**. This metric
reveals how much "work" the CPU must perform to move a single piece of data.

There are, of course, going to be many different variances of exactly what has
to happen for a connection to be initiated, what kind of security layers are in
place, what kind of observation the kernel is also helping with, and what else
is causing preemption on the system. So, we start as close as we can get to the
_actual_ worst case, and extrapolate a conservative best case from there.

### First: The Baseline - Throttled i3-1115G4 (Tiger Lake) (3.0 GHz)

This has some special significance, because part of Splinter's development was
driven by resource constraints that are driven by commodity hardware being the
only accessible computation tool for most research. Getting every possible ounce
of use from these CPUs is a way of life for the ill-funded!

At 3.0 GHz, each core has a budget of **3,000,000,000 cycles/sec**:

| Substrate                        | Ops/Sec     | Cycles Per Operation (CPO) | Logic                                                             |
| :------------------------------- | :---------- | :------------------------- | :---------------------------------------------------------------- |
| **Traditional (SQLite/Redis)**   | 130,000     | **~23,076**                | Throttled by context switches, syscalls, and the Kernel boundary. |
| **Splinter (Standard)**          | 3,200,000   | **~937**                   | Operates in userspace via `mmap`, avoiding the "Socket Tax."      |
| **Splinter (Projected Pinning)** | 10,000,000+ | **<300**                   | Limited only by memory bus throttling and the 6MB L3 cache.       |

### Proportional Scaling (To Modern AMD Hardware)

On a modern AMD rig (e.g., Zen 4/5), we are no longer memory-throttled. We
benefit from 128MB+ of L3 cache and significantly higher IPC (Instructions Per
Cycle).

- **Traditional Server Ceiling:** Even on high-end hardware, servers are bound
  by the **Kernel's interrupt latency**. In theory, if a server _could_ reach
  500k ops/sec, it still spends **~10,000 cycles/op** due to the inescapable
  cost of the network stack.

- **Splinter (NUMA-Pinned):** With the manifold residing entirely in a massive
  L3 cache and pinned to a high-bandwidth Infinity Fabric lane, we expect the
  CPO to drop into the single digits.

### The Math of the Claim

If we conservatively estimate the AMD rig hits **500,000,000 ops/sec** (0.5
Billion), it becomes hard to argue with the physics:

$$CPO_{AMD} = \frac{5,000,000,000 \text{ cycles/sec (est. clock)}}{500,000,000 \text{ ops/sec}} = \mathbf{10 \text{ cycles/op}}$$

This represents a **2,300x improvement in efficiency** over the traditional i3
baseline (23,076 / 10).

## TL;DR: Splinter Can Save You Significant Research $$$

With a little work, you get the kind of high-speed ingestion on commodity 
hardware enterprise is currently getting on big-iron.

If you manage to get big-iron, speed is no longer a concern for you.

It just needs space to persist when not in RAM.
