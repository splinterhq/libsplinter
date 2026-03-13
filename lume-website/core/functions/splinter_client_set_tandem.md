---
title: splinter_client_set_tandem
parent: "Function Reference"
nav_order: 26
metas:
  lang: en
  description: >-
    Documentation on combining keys as tandem for standard order storage in Splinter, an atomic L3-speed key-value & vector store that resides in Linux shared memory and optionally persists.
  keywords:
    - Vector Anti-Database
    - Vector Storage
    - Shared-Memory Substrate
    - Lock-Free IPC
    - LLM Semantic Memory
    - Inter-Process Communication
  robots: true
  generator: true
---

# `splinter_client_set_tandem()`: Tandem Slot Config

Tandem slots are useful when you need to group independent signals from the same sensor together with  independent vectors per-sensor.Or, more laconically: tandem slots let you capture standard orders and their vectors as independent copies of the same variable.

In your experiment, where you have: 

 - x(thing): Normalized (thing)
 - ẋ(thing): Velocity Of (thing)
 - ẍ(thing): Acceleration Of (thing)

You could access these and their vectors in splinter as `x`, `x.1` and `x.2` independently and respectively.

Tandem setup makes sure housekeeping works on all I/O (including unset) so that you don't have to worry about it. There's no hard limit to how many orders of the same key you can have, except the number of slots.

See the [constants](/core/constants/) documentation for information on how to change the special accessor (`.`) to something less likely to appear in your own key space.

## Prototype & Rationale

```c
int splinter_client_set_tandem(const char *base_key, const void **vals const size_t *lens, uint8_t orders);
```

The `base_key` is the string identifier for the parent (`x`) slot. `vals` is an array of void pointers, to temporarily hold the child keys while the tandem slots are created. `lens` is an array of `size_t` used side-by-side with `vals` to track the arena length. `orders` are the number of tandem slots being requested.

## Return Value:

On success, 0 is returned. Return values less than zero indicate failure. `errno` currently is not set, but future versions will likely change that.

## Example:

See CLI & Tooling Examples.

### CLI & Tooling Example Uses:

To see `splinter_client_set_tandem()` in-use within the tools that ship with Splinter, see the following files:

| File | What `splinter_client_set_tandem()` Does There |
| ---- | ---- |
| `splinter.c` | Base library implementation |
| `splinter_cli_cmd_orders.c` | The CLI `orders` command. |

### See Also:

The following is also relevant: [`splinter_client_unset_tandem()`](/core/functions/splinter-client-unset-tandem/)