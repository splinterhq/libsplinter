---
title: Structures & Types
parent: "Core Library"
nav_order: 2
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

# Main Structures

All structures in `splinter.h` are commented thoroughly; this "walk through"
documentation serves as a point of reference for the API documentation and as a
way to draw attention to important consideration when making changes.

> Changing structures also means needing to change client code or the build will
> break. { .warning }

If you embed Splinter and don't build the tools with your modifications, then
this is a non-issue, of course! However, many people don't realize the tools
that ship with splinter expect these structures to be as they are; if you intend
to keep in step with the tool development, it's recommended that you do not
modify these.

<!-- TOC -->

## `struct splinter_header`

This is the "global" Splinter header that contains all of the metadata around
the upkeep and state of the store.

```c
struct splinter_header {
    /** @brief Magic number (SPLINTER_MAGIC) to verify integrity. */
    uint32_t magic;
    /** @brief Data layout version (SPLINTER_VER). */
    uint32_t version;
    /** @brief Total number of available key-value slots. */
    uint32_t slots;
    /** @brief Maximum size for any single value. */
    uint32_t max_val_sz;
    /** @brief Global epoch, incremented on any write. Used for change detection. */
    atomic_uint_least64_t epoch;
    /** @brief Core feature flags  */
    atomic_uint_least8_t core_flags;
    /** @brief User-defined feature flags */
    atomic_uint_least8_t user_flags;
    /** @brief Track the next-available value region */
    atomic_uint_least32_t val_brk;
    /** @brief Running total size of the arena */
    uint32_t val_sz;
    /** @brief Memory alignment (e.g  64) */
    uint32_t alignment;

    /* Diagnostics: counts of parse failures reported by clients / harnesses */
    atomic_uint_least64_t parse_failures;
    atomic_uint_least64_t last_failure_epoch;

    // Maps each of the 64 bloom bits to a specific Signal Group (0-63)
    // 0xFF indicates no watch for that bit.
    atomic_uint_least8_t bloom_watches[64];

    // The Signal Arena for epoll-backed notifications
    alignas(64) struct splinter_signal_node signal_groups[SPLINTER_MAX_GROUPS];
};
```

## `struct splinter_slot`

The main mechanical mechanism behind Splinter. This stores the slot key, value
arena length + starting offset, bloom bits, feature flats and some low-level
configuration options.

Each slot has its own epoch for sequence locking and update notifications.

```c
struct splinter_slot {
    /** @brief The FNV-1a hash of the key. 0 indicates an empty slot. */
    alignas(64) atomic_uint_least64_t hash;
    /** @brief Per-slot epoch, incremented on write to this slot. Used for polling. */
    atomic_uint_least64_t epoch;
    /** @brief Offset into the VALUES region where the value data is stored. */
    uint32_t val_off;
    /** @brief The actual length of the stored value data (atomic). */
    atomic_uint_least32_t val_len;
    /** @brief The type-naming flags for slot typing */
    atomic_uint_least8_t type_flag;
    /** @brief The user-defined flags for slot features */
    atomic_uint_least8_t user_flag;
    /** @brief Watcher signal group for multi-watching */
    atomic_uint_least64_t watcher_mask;
    /** @brief The time a slot was created (optional; must be set by the client) */
    atomic_uint_least64_t ctime;
    /** @brief The last time the slot was meaningfully accessed (optional; must be set by the client) */
    atomic_uint_least64_t atime;
    /** @brief The 64-bit Bloom filter / Label mask */
    atomic_uint_least64_t bloom;
    /** @brief The null-terminated key string. */
    char key[SPLINTER_KEY_MAX];
#ifdef SPLINTER_EMBEDDINGS
    float embedding[SPLINTER_EMBED_DIM];
#endif
};
```

## `struct splinter_header_snapshot`

This is the non-atomic version of `struct splinter_header`, which is used to
receive a copy of the atomic one (similar to `ioctl()`).

```c
typedef struct splinter_header_snapshot {
    /** @brief Magic number (SPLINTER_MAGIC) to verify integrity. */
    uint32_t magic;
    /** @brief Data layout version (SPLINTER_VER). */
    uint32_t version;
    /** @brief Total number of available key-value slots. */
    uint32_t slots;
    /** @brief Maximum size for any single value. */
    uint32_t max_val_sz;
    /** @brief Global epoch, incremented on any write. Used for change detection. */
    uint64_t epoch;
    /** @Brief holds the slot type flags */
    uint8_t core_flags;
    /** @Brief holds the slot user flags */
    uint8_t user_flags;

    /* Diagnostics: counts of parse failures reported by clients / harnesses */
    uint64_t parse_failures;
    uint64_t last_failure_epoch;
} splinter_header_snapshot_t;
```

## `struct splinter_slot_snapshot`

This is the non-atomic version of `struct splinter_snapshot`, which is used to
receive a copy of the atomic one (similar to `splinter_header_snapshot`).

```c
typedef struct splinter_slot_snapshot {
    /** @brief The FNV-1a hash of the key. 0 indicates an empty slot. */
    uint64_t hash;
    /** @brief Per-slot epoch, incremented on write to this slot. Used for polling. */
    uint64_t epoch;
    /** @brief Offset into the VALUES region where the value data is stored. */
    uint32_t val_off;
    /** @brief The actual length of the stored value data (atomic). */
    uint32_t val_len;
    /** @brief The slot type flags */
    uint8_t type_flag;
    /** @brief The slot user flags */
    uint8_t user_flag;
    /** @brief Storage for creation time */
    uint64_t ctime;
    /** @brief Storage for access time */
    uint64_t atime;
    /** @brief The null-terminated key string. */
    char key[SPLINTER_KEY_MAX];
#ifdef SPLINTER_EMBEDDINGS
    float embedding[SPLINTER_EMBED_DIM];
#endif
} splinter_slot_snapshot_t;
```
