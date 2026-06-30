---
title: "Splinter CLI Reference"
nav_order: 2
date: 2026-06-30
updated: 2026-06-30
---

## Splinter CLI Reference Index

Every user-invokable command, grouped by purpose. The same commands are available
in both surfaces of the one binary: `splinter_cli` (interactive REPL) and
`splinterctl` (one-shot shell invocation). Commands marked *(build-gated)* are only
present when their feature was compiled in.

### Session & Store

- [use](splinterctl_use.md) — select a store to be the current store.
- [init](splinterctl_init.md) — create a store with default or specific geometry.
- [config](splinterctl_config.md) — display bus settings or set a bus feature flag.
- [caps](splinterctl_caps.md) — print version, build, and compiled-in feature flags.

### Reading & Inspection

- [get](splinterctl_get.md) — retrieve and print a key's value (type-aware).
- [head](splinterctl_head.md) — display a key's metadata only.
- [list](splinterctl_list.md) — list keys, most-recently-updated first.
- [type](splinterctl_type.md) — display or set a key's named slot type.
- [export](splinterctl_export.md) — export the store (currently JSON) to stdout.

### Writing & Editing

- [set](splinterctl_set.md) — set a key's value.
- [append](splinterctl_append.md) — append to an existing key's value.
- [unset](splinterctl_unset.md) — delete a key (optionally its tandem keys).

### Atomic Math

- [math](splinterctl_math.md) — incr/decr and bitwise ops on `biguint` keys.

### Ordered (Tandem) Sets

- [orders](splinterctl_orders.md) — manage the tandem orders of a key for coupled vectors.

### Bloom Labels & Signals

- [label](splinterctl_label.md) — apply a Bloom label to a key.
- [bind](splinterctl_bind.md) — map a Bloom label to a signal group.
- [bump](splinterctl_bump.md) — advance a key's epoch (e.g. after relabeling).
- [watch](splinterctl_watch.md) — watch a key or signal group for changes.

### Vectors & Semantic Search

- [search](splinterctl_search.md) — search embedded keys by semantic similarity *(build-gated: embeddings)*.
- [ingest](splinterctl_ingest.md) — chunk a file/stdin into tandem slots for splinference *(build-gated: embeddings)*.
- [retrain](splinterctl_retrain.md) — zero a key's vectors and rewind its epoch to republish.

### Cooperative Memory Advisement

- [shard](splinterctl_shard.md) — inspect and seed the Logic Shard bid table / election.

### Scripting

- [wasm](splinterctl_wasm.md) — run a WASM module via WASMEdge *(build-gated: WASM)*.
- [lua](splinterctl_lua.md) — run a Lua script *(build-gated: Lua)*.

### Utilities & Shell

- [clear](splinterctl_clear.md) — clear the screen.
- [cls](splinterctl_cls.md) — alias of `clear`.
- [help](splinterctl_help.md) — help for commands and features.
- [hist](splinterctl_hist.md) — show (and filter) command history.
- [uuid](splinterctl_uuid.md) — generate a UUID v4.
