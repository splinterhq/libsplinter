---
title: "Index | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## Splinter API Reference Index

Every public function declared in `splinter.h`, grouped by purpose. See the
AI Primer at the top of `splinter.h` for the seqlock invariant, errno model, and
risk topology that govern all of these.

### Store Lifecycle & Geometry

- [splinter_create](splinter_create.md) — create and initialize a new store with fixed geometry.
- [splinter_open](splinter_open.md) — open an existing store by name or path.
- [splinter_open_numa](splinter_open_numa.md) — open and pin a store to a NUMA node.
- [splinter_open_or_create](splinter_open_or_create.md) — open, or create if missing.
- [splinter_create_or_open](splinter_create_or_open.md) — create, or open if it already exists.
- [splinter_close](splinter_close.md) — close the store and unmap shared memory.
- [splinter_get_header_snapshot](splinter_get_header_snapshot.md) — copy the header (geometry/metadata) for safe inspection.

### Key/Value Operations

- [splinter_set](splinter_set.md) — set or update a key's value.
- [splinter_get](splinter_get.md) — retrieve a key's value (honors `val_len`).
- [splinter_unset](splinter_unset.md) — delete a key and clear its slot.
- [splinter_append](splinter_append.md) — append to an existing value in place.
- [splinter_list](splinter_list.md) — list all keys in the store.
- [splinter_get_slot_snapshot](splinter_get_slot_snapshot.md) — copy a slot's metadata for inspection.
- [splinter_get_raw_ptr](splinter_get_raw_ptr.md) — direct (unsafe) pointer into shared memory.

### Epoch & Consistency

- [splinter_get_epoch](splinter_get_epoch.md) — read a slot's seqlock epoch.
- [splinter_bump_slot](splinter_bump_slot.md) — advance a slot's epoch without other work.
- [splinter_poll](splinter_poll.md) — wait for a key's value to change.
- [splinter_retrain_slot](splinter_retrain_slot.md) — scrub vectors and rewind the epoch to republish.

### Value Hygiene (Mop)

- [splinter_set_mop](splinter_set_mop.md) — set the auto-scrub mode (off/hybrid/full boil).
- [splinter_get_mop](splinter_get_mop.md) — read the current mop mode.
- [splinter_purge](splinter_purge.md) — sweep stale bytes past each value's length.

### Slot Typing, Time & System Scope

- [splinter_set_named_type](splinter_set_named_type.md) — declare a slot's named type.
- [splinter_now](splinter_now.md) — read the 64-bit cycle counter.
- [splinter_set_slot_time](splinter_set_slot_time.md) — set a slot's ctime/atime.
- [splinter_set_as_system](splinter_set_as_system.md) — promote a key to system usage.

### Atomic Integer Operations

- [splinter_integer_op](splinter_integer_op.md) — in-place bitwise/arithmetic ops on `BIGUINT` keys.

### Embeddings

- [splinter_set_embedding](splinter_set_embedding.md) — set a key's embedding vector.
- [splinter_get_embedding](splinter_get_embedding.md) — retrieve a key's embedding vector.

### Bloom Labels & Semantic Routing

- [splinter_set_label](splinter_set_label.md) — OR a label mask into a slot's Bloom filter.
- [splinter_unset_label](splinter_unset_label.md) — remove a label mask from a slot.
- [splinter_enumerate_matches](splinter_enumerate_matches.md) — visit slots matching a Bloom mask.

### Ordered (Tandem) Sets

- [splinter_client_set_tandem](splinter_client_set_tandem.md) — write multiple ordered values for a base key.
- [splinter_client_unset_tandem](splinter_client_unset_tandem.md) — delete a base key and its orders.

### Configuration & User Flags

- [splinter_config_set](splinter_config_set.md) — set bus config flags.
- [splinter_config_clear](splinter_config_clear.md) — clear bus config flags.
- [splinter_config_test](splinter_config_test.md) — test bus config flags.
- [splinter_config_snapshot](splinter_config_snapshot.md) — snapshot the bus config byte.
- [splinter_slot_usr_set](splinter_slot_usr_set.md) — set a slot's user flags.
- [splinter_slot_usr_clear](splinter_slot_usr_clear.md) — clear a slot's user flags.
- [splinter_slot_usr_test](splinter_slot_usr_test.md) — test a slot's user flags.
- [splinter_slot_usr_snapshot](splinter_slot_usr_snapshot.md) — snapshot a slot's user flags.

### Signals & Pub/Sub

- [splinter_watch_register](splinter_watch_register.md) — subscribe a key to a signal group.
- [splinter_watch_unregister](splinter_watch_unregister.md) — unsubscribe a key from a signal group.
- [splinter_watch_label_register](splinter_watch_label_register.md) — map a Bloom label to a signal group.
- [splinter_pulse_watchers](splinter_pulse_watchers.md) — pulse the Signal Arena for a slot.
- [splinter_pulse_keygroup](splinter_pulse_keygroup.md) — pulse a key's signal group.
- [splinter_get_signal_count](splinter_get_signal_count.md) — read a signal group's pulse count.

### Event Bus (eventfd)

- [splinter_event_bus_init](splinter_event_bus_init.md) — arm the owner's eventfd (once per store).
- [splinter_event_bus_open](splinter_event_bus_open.md) — open a process-local fd to the eventfd.
- [splinter_event_bus_wait](splinter_event_bus_wait.md) — block until the global epoch changes.
- [splinter_event_bus_close](splinter_event_bus_close.md) — close a bus fd.
- [splinter_event_bus_get_dirty](splinter_event_bus_get_dirty.md) — copy the dirty-slot bitmask.

### Logic Shard Election & Cooperative madvise

- [splinter_shard_claim](splinter_shard_claim.md) — claim a bid slot and declare memory intent.
- [splinter_shard_claim_ex](splinter_shard_claim_ex.md) — claim with explicit pid/claimed_at.
- [splinter_shard_rebid](splinter_shard_rebid.md) — refresh an existing claim's window.
- [splinter_shard_release](splinter_shard_release.md) — voluntarily release a bid slot.
- [splinter_shard_election](splinter_shard_election.md) — run the read-only election scan.
- [splinter_shard_is_sovereign](splinter_shard_is_sovereign.md) — check if a shard is the sovereign.
- [splinter_shard_table_snapshot](splinter_shard_table_snapshot.md) — snapshot the bid table for audit.
- [splinter_madvise](splinter_madvise.md) — cooperative `posix_madvise()` via the election.
