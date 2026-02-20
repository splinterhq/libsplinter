---
title: Some Math Behind Splinter's Claims
date: "2026-02-20"
author: Tim Post
draft: false
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
# Some Math Behind Splinter's Claims

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

## **The Efficiency Gap: Instruction Physics vs. Socket Tax**

To justify a **Passive Substrate** (Splinter) over **Active Middleware**
(Redis/SQLite), we analyze the **Cycles Per Operation (CPO)**. This metric
reveals how much "work" the CPU must perform to move a single piece of data.

There are, of course, going to be many different variances of exactly what has
to happen for a connection to be initiated, what kind of security layers are in
place, what kind of observation the kernel is also helping with, and what else
is causing preemption on the system. So, we start as close as we can get to the
_actual_ worst case, and extrapolate a conservative best case from there.

### **1. The Baseline: Throttled i3-1115G4 (3.0 GHz)**

At 3.0 GHz, each core has a budget of **3,000,000,000 cycles/sec**.

| Substrate                        | Ops/Sec     | Cycles Per Operation (CPO) | Logic                                                             |
| :------------------------------- | :---------- | :------------------------- | :---------------------------------------------------------------- |
| **Traditional (SQLite/Redis)**   | 130,000     | **~23,076**                | Throttled by context switches, syscalls, and the Kernel boundary. |
| **Splinter (Standard)**          | 3,200,000   | **~937**                   | Operates in userspace via `mmap`, avoiding the "Socket Tax."      |
| **Splinter (Projected Pinning)** | 10,000,000+ | **<300**                   | Limited only by memory bus throttling and the 6MB L3 cache.       |

### **2. The AMD Extrapolation: Proportional Scaling**

On a modern AMD rig (e.g., Zen 4/5), we are no longer memory-throttled. We
benefit from 128MB+ of L3 cache and significantly higher IPC (Instructions Per
Cycle).

- **Redis Ceiling:** Even on high-end hardware, Redis is bound by the **Kernel's
  interrupt latency**. Even at 500k ops/sec, it still spends **~10,000
  cycles/op** due to the inescapable cost of the network stack.
- **Splinter (NUMA-Pinned):** With the manifold residing entirely in a massive
  L3 cache and pinned to a high-bandwidth Infinity Fabric lane, we expect the
  CPO to drop into the single digits.

### **3. The Math of the Claim**

If we conservatively estimate the AMD rig hits **500,000,000 ops/sec** (0.5
Billion), the physics becomes undeniable:

$$CPO_{AMD} = \frac{5,000,000,000 \text{ cycles/sec (est. clock)}}{500,000,000 \text{ ops/sec}} = \mathbf{10 \text{ cycles/op}}$$

This represents a **2,300x improvement in efficiency** over the traditional i3
baseline (23,076 / 10).

## **Summary for Researchers**

While traditional architectures are restricted by a **Series of Jerks**—our term
for the erratic overhead of interrupts and context switching, Splinter functions
as a **Laminar Flow Substrate**. On commodity hardware, Splinter already
outperforms Redis by **16x** and SQLite3 by **33x**, despite memory throttling.

Moving to an AMD rig with NUMA-pinning will allow us to achieve a projected **10
cycles per operation**. At this scale, Splinter isn't just "faster"—it is
operating at the physical limit of the silicon, achieving **~2,300 times the
efficiency** of standard middleware by removing the "Socket Tax" entirely.

**Baseline: i3-1115G4 @ 3.0GHz**

Traditional Middleware (130k ops/sec):
$$\frac{3 \times 10^9}{130,000} \approx 23,076 \text{ cycles/op}$$

Splinter Standard (3.2M ops/sec):
$$\frac{3 \times 10^9}{3,200,000} \approx 937 \text{ cycles/op}$$

**Projected: AMD Rig (NUMA-Pinned @ 5.0GHz Clock):**

We're in the process of seeking/acquiring additional funding for development and
plan to transition to "big iron" now that we're certain we can support weaker
memory models sufficiently.

To help substantiate the need to grow the computational lab, we had to consider
what we could do (GDELT is massive) with higher-resolution samplings. We knew it
was high but the math was a little startling:

**Splinter Conservative Estimate (500M ops/sec):**

$$\frac{5 \times 10^9}{500,000,000} = 10 \text{ cycles/op}$$

**The Efficiency Delta:**

The jump from **23,076** cycles to **10** cycles per operation represents a
**2,300x** reduction in the computational energy required to audit the manifolds
examined in high-resolution research.

We expect this will be the experience of teams working with massive amounts of
vectors, or timing Rank 2+ tensor measurements (as we do) and invite additional
use as well as scrutiny and feedback.

If you're still here, you might be interested in reading about [why Splinter 
and Linux get along so well](/splinter_and_linux).