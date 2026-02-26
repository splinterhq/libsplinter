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

Design constraints prevent making the CLI's module list dynamic; some
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
and prepare workloads that might compete with each other, or send the kernel 
conflicting signals when it comes to the same region of memory.

I'm going to add store level accounting bits so that shards cooperatively schedule
themselves based on how they need to use memory for how long and at what scheduling
priority. This election determines what shards run when, and what is ultimately 
communicated to the kernel via `posix_madvise()`.

Shards will use a `splinter_madvise()` that votes in the election, and can optionally
block instead of voluntarily yielding, depending on what kind of API is being 
called.

**Put plainly:** The kernel can't be put in a state where it has to constantly 
re-evaluate its page cache strategy for the same region just because a cron job was 
ill-conceived, but we need "quick and sometimes dirty load-ables" for experiments
or production inference loads. Shards are a great compromise answer.

The shape of the helpers / macros is still wildly kinetic, but functionality and 
design have been mostly finalized.

Shards are expected to 'land' Late February / Early March 2026.

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
