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
point `splinterctl` or `splinter_cli_` at the `.so` file and, provided it
conforms to a symbol map, loads it.

This gives you access to Splinter with all the CLI structure and whatever else
you want to bring in, without having to modify Splinter or the tools.

## The Solution: Loadable Shards

Design constraints prevent making the CLI's module list dynamic entirely; some
platforms disallow heap allocation entirely so Splinter tries hard to not
require it for anything essential, even in tooling.

We need to instead have an optional dynamic module structure that can advertise
and enter these 'shards' for the user. As long as they contain the correct entry
points and metadata, they can be compiled anywhere `splinter.h` can be found.
This `splinter_shard_t` will be the only coupling of tooling to the core store
(found in the core header, not CLI).

## Progress: Coming Soon!

Implementation requires a little juggling of what the CLI currently understands
a use session to look like as far as different users loading different shards
that have the same topological access to the same store.

There's some memory expectation setting and accounting to tackle as shards load
and prepare workloads that might compete with each other, as well as some other
debt refactoring in the CLI that's already planned to consider (which is mostly
done).

Shards are expected Late February / Early March 2026.

Practical examples would be sidecar analysis, backfill, a shard could even
expose the store as a RESTful interface (`Deno -> FFI -> Oak` or even
`Bun -> FFI -> Express`). There's lots of possibilities, think "micro services"
that run as shared objects.

They have access to Lua, too `:)`

Stay tuned!
