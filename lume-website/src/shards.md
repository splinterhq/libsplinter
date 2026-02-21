---
title: What Are Splinter Logic Shards?
date: "2026-02-20"
author: Tim Post
draft: false
metas:
  lang: en
  description: >-
    Learn about Splinter Logic Shards, loadable modules that allow dynamic extension of the CLI architecture for custom data processing and transformation.
  keywords:
    - Logic Shards
    - Loadable Modules
    - Dynamic Extension
    - C Plugins
    - Data Transformation
  robots: true
  generator: true
---
# What Are Splinter Logic Shards?

Splinter's CLI is easily extended, but, you might want to link against
_considerably more_ than Splinter's build system allows for, and it's usually
just for one or several very specific tasks.

In short, you need to be able to load modules into the CLI architecture that can
handle your custom processing, transformation, observation, _**whatever**_
without having to wrangle linkage and that didn't need to be located in-tree to
be compiled.

If you want to develop a `foo` command, you should be able to just do so and
point `splinterctl` or `splinter_cli` at the `.so` file and, provided it
conforms to a symbol map, loads it.

This gives you access to Splinter with all the CLI structure and whatever else
you want to bring in, without having to modify Splinter or the tools.

## The Solution: Loadable Shards (a type-punned derivative of a "splinter" 🤓)

Design constraints prevent making the CLI's module list dynamic entirely; some
platforms disallow heap allocation entirely so Splinter tries hard to not
require it for anything essential, even in tooling.

We need to instead have an optional dynamic module structure that can advertise
and enter these 'shards' for the user. As long as they contain the correct entry
points and metadata, they can be compiled anywhere `splinter.h` can be found.
This `splinter_shard_t` will be the only coupling of tooling to the core store
(found in the core header, not CLI).

## Progress: Figured Out Mostly and Coming Soon!

Implementation requires a little juggling of what the CLI currently understands
a use session to look like as far as different users loading different shards
that have the same topological access to the same store.

There's some memory expectation setting and accounting to tackle as shards load
and prepare workloads that might compete with each other, as well as some other
debt refactoring in the CLI that's already planned to consider (which is mostly
done).

I may add store-level accounting bits so that individual shards each calling 
`posix_madvise()` regarding what they intend to do with the store doesn't 
result in the kernel being sent a stream of erratic mixed signals. The kernel
can't be put in a state where it has to constantly re-evaluate its page cache
strategy for the same region just because a cron job was ill-conceived.

We don't care what individual shards advise regarding their own heap, but a
centralized advisory governor might be needed. Shards 'register' their intended 
access patterns rather than calling the kernel directly. This ensures that a backfill 
task and a real-time monitor don't send conflicting signals to the page cache, 
maintaining the L3 pipeline's integrity even under (perhaps unintentionally) heavy
workloads.

Some macros to help make sure memory advisement is coordinated and (upon finishing) 
examined for expectation meeting reality should make this pretty much invisible to 
clients, if it turns out anything is needed at all. At the cost of maybe 16 or 32 
extra bits in the store global header along with the structure. The rest really 
is just the simple `dlopen()` loader and unloader code.

Shards are expected Late February / Early March 2026.

## What Can We \*DO\* With Them, Though?!

Practical examples would be sidecar analysis, backfill, a shard could even
expose the store as a RESTful interface (similar to how it's currently done 
with `Deno -> FFI -> Oak` or even `Bun -> FFI -> Express`, except we remove
runtimes and TypeScript from the service loop by doing it with C and a micro
http server that supports chunking, with something like GhostTunnel if SSL
or mTLS is needed). This will likely be the "best bet" for k8s deployments,
unless someone wants to write a gRPC version. 

There's lots of possibilities, think "micro services" that run as shared 
objects plugged into an L3 data pipeline.

They have access to Lua, too `:)`

Stay tuned!
