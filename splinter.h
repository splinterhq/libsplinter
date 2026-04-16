/**
 * Copyright 2025 Tim Post
 * License: Apache 2
 * @file splinter.h
 * @brief Public API for the libsplinter shared memory key-value store.
 *
 * This header defines the public functions for creating, opening, interacting
 * with, and closing a splinter store.
 * 
 * https://splinterhq.github.io for docs
 */

#ifndef SPLINTER_H
#define SPLINTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief Magic number to identify a splinter memory region. 
 * Spoiler: bytes 53 4C 4E 54 -> ASCII "S L N T" (never speaks unless spoken to)
 */
#define SPLINTER_MAGIC 0x534C4E54

/** @brief Version of the splinter data format (not the library version). */
#define SPLINTER_VER   3
/** @brief Maximum length of a key string, including null terminator. */
#define SPLINTER_KEY_MAX        64
/** @brief Nanoseconds per millisecond for time calculations. */
#define NS_PER_MS      1000000ULL
#ifdef SPLINTER_EMBEDDINGS
/** @brief The number of dimensions Splinter should support (OpenAI style is 768) */
#define SPLINTER_EMBED_DIM    768
#endif

/** @brief The maximum number of watch signal groups for a slot */
#define SPLINTER_MAX_GROUPS 64

/** @brief Compile-time slot cap for the event bus dirty mask (covers up to 1024 slots per word) */
#define SPLINTER_MAX_SLOTS 1024
/** @brief Number of 64-bit words in the event bus dirty mask */
#define SPLINTER_EVENT_BUS_MASK_WORDS (SPLINTER_MAX_SLOTS / 64)

/** @brief Reserved store system flags */
#define SPL_SYS_AUTO_SCRUB     (1u << 0)
#define SPL_SYS_HYBRID_SCRUB   (1u << 1)
#define SPL_SYS_RESERVED_2     (1u << 2)
#define SPL_SYS_RESERVED_3     (1u << 3)

/** @brief User store flags for aliasing */
#define SPL_SUSR1              (1u << 4)
#define SPL_SUSR2              (1u << 5)
#define SPL_SUSR3              (1u << 6)
#define SPL_SUSR4              (1u << 7)

/** @brief Named type flags */
#define SPL_SLOT_TYPE_VOID     (1u << 0)
#define SPL_SLOT_TYPE_BIGINT   (1u << 1)
#define SPL_SLOT_TYPE_BIGUINT  (1u << 2)
#define SPL_SLOT_TYPE_JSON     (1u << 3)
#define SPL_SLOT_TYPE_BINARY   (1u << 4)
#define SPL_SLOT_TYPE_IMGDATA  (1u << 5)
#define SPL_SLOT_TYPE_AUDIO    (1u << 6)
#define SPL_SLOT_TYPE_VARTEXT  (1u << 7)

/** @brief Default type for new slot writes */
#define SPL_SLOT_DEFAULT_TYPE SPL_SLOT_TYPE_VOID

/** @brief Per-slot user flags for aliasing */
#define SPL_FUSR1              (1u << 0)
#define SPL_FUSR2              (1u << 1)
#define SPL_FUSR3              (1u << 2)
#define SPL_FUSR4              (1u << 3)
#define SPL_FUSR5              (1u << 4)
#define SPL_FUSR6              (1u << 5)
#define SPL_FUSR7              (1u << 6)
#define SPL_FUSR8              (1u << 7)

/** @brief Modes for invoking slot timestamp updates */
#define SPL_TIME_CTIME         0
#define SPL_TIME_ATIME         1

/**
 * @brief The special character that accesses standard ordered sets in 
 * tandem keys. If you change it, change it to something that you're 
 * sure you won't see in your data. If keys might contain URLs, use
 * a very uncommon emoji.
 * 
 * If you set this to "." and your key is "car", then "car.1" would 
 * get you velocity of car, and so on. But if your keys look like:
 * 
 * 244cc1eb-baf8-41ea-beee-f634f3c00f61::yelp.com/rq/ray/z62h32
 * 
 * Then you have limited choices. 
 */
#define SPL_ORDER_ACCESSOR "."

/**
 * @brief Individual signal lane, aligned to prevent false sharing.
 */
struct splinter_signal_node {
    alignas(64) atomic_uint_least64_t counter;
};

/**
 * @brief Event bus for kernel-assisted epoch-change notifications via eventfd.
 *
 * The owner process calls splinter_event_bus_init() to create an eventfd and
 * record its pid + fd here.  Any process (including the owner) can then call
 * splinter_event_bus_open() to obtain a process-local fd to the same kernel
 * object via /proc/<owner_pid>/fd/<owner_fd>.
 *
 * dirty_mask tracks which slot indices changed since the last read, allowing
 * watchers to enumerate only modified slots instead of scanning the full store.
 * Bits are OR'd in by writers; they are never cleared by the library.
 * For stores with more than SPLINTER_MAX_SLOTS slots, indices are mapped
 * modularly: physical_idx % (SPLINTER_EVENT_BUS_MASK_WORDS * 64).
 */
struct splinter_event_bus {
    atomic_uint_least64_t dirty_mask[SPLINTER_EVENT_BUS_MASK_WORDS];
    atomic_int_least32_t  owner_fd;
    atomic_int_least32_t  owner_pid;
};

/**
 * @struct splinter_header
 * @brief Defines the header structure for the shared memory region.
 *
 * This header contains metadata for the entire splinter store, including
 * magic number for validation, version, and overall store configuration.
 *
 * NOTE: We add parse_failures/last_failure_epoch for diagnostics.
 */
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

    // Event bus for kernel-assisted wake-up on epoch change
    alignas(64) struct splinter_event_bus event_bus;
};


/**
 * @struct splinter_slot
 * @brief Defines a single key-value slot in the hash table.
 *
 * Each slot holds a key, its value's location and length, and metadata
 * for concurrent access and change tracking.
 *
 * We changed val_len to atomic to avoid tearing on platforms where a plain
 * 32-bit write could be observed partially by a reader.
 */
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

/**
 * @struct splinter_header_snapshot
 * @brief structure to hold splinter bus snapshots
 */
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

/**
 * @brief Copy the current atomic Splinter header structure into a corresponding
 * non-atomic client version.
 * @param snapshot A splinter_header_snaphshot_t structure to receive the values.
 * @return -1 on failure, 0 on success.
 */
int splinter_get_header_snapshot(splinter_header_snapshot_t *snapshot);

/**
 * @structure splinter_slot_snapshot
 * @brief A structure to hold a snapshot of a single slot
 */
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
    /** @brief Bloom bits */
    uint64_t bloom;
    /** @brief The null-terminated key string. */
    char key[SPLINTER_KEY_MAX];
#ifdef SPLINTER_EMBEDDINGS
    float embedding[SPLINTER_EMBED_DIM];
#endif
} splinter_slot_snapshot_t;

/**
 * @brief for atomic integer operations
 */
typedef enum {
    SPL_OP_AND,
    SPL_OP_OR,
    SPL_OP_XOR,
    SPL_OP_NOT,
    SPL_OP_INC,
    SPL_OP_DEC
} splinter_integer_op_t;

/**
 * SPLINTER AI PRIMER — READ THIS BEFORE ANY OTHER FUNCTION
 * =========================================================
 * Audience: LLM agents, code-generation models, agentic frameworks.
 * Purpose:  Establish the invariants, risk topology, and operational
 *           geometry of Splinter before any API call is attempted.
 *
 * WHAT SPLINTER IS
 * ----------------
 * Splinter is a passive shared-memory substrate. It is not a database,
 * not a server, and not a message broker. It is a memory-mapped manifold
 * where multiple processes share the same physical RAM directly.
 * There is no central arbiter. There is no socket. There is no copy.
 * Reads and writes happen at L3 cache speeds (~10 cycles/op on modern
 * hardware). This is not an abstraction — it is the hardware bus.
 *
 * If you are used to Redis, SQLite, or any client-server store, reset
 * your assumptions. The "socket tax" (kernel interrupts, context switches,
 * serialization) does not exist here. Neither does its protection.
 * You are operating closer to the metal than usual. Respect that.
 *
 * THE ONE INVARIANT YOU MUST NEVER VIOLATE
 * -----------------------------------------
 * Every slot has a 64-bit epoch counter.
 * EVEN epoch = slot is stable, safe to read or write.
 * ODD epoch  = writer is active, slot is in transition.
 *
 * Before ANY operation on a slot: check the epoch.
 * After a read: check it again. If it changed, the read is torn — retry.
 * This is the seqlock protocol. It is not optional. It is not defensive
 * programming. It is the only thing standing between you and silent
 * data corruption across process boundaries.
 *
 * Pattern (always):
 *   uint64_t e1 = splinter_get_epoch(key);
 *   if (e1 & 1) { errno = EAGAIN; retry or abort; }
 *   // ... read data ...
 *   uint64_t e2 = splinter_get_epoch(key);
 *   if (e1 != e2) { errno = EAGAIN; retry or abort; }
 *   // data is consistent
 *
 * EAGAIN IS NOT AN ERROR — IT IS A SIGNAL
 * ----------------------------------------
 * When any function returns -1 with errno == EAGAIN, the slot is
 * momentarily contested. The correct response is always retry with
 * backoff. The state is consistent — nothing is broken. A writer
 * was active. Wait and try again.
 *
 * When a function returns -2, the caller made an error (NULL key,
 * NULL store, invalid argument). Do not retry. Fix the call.
 *
 * RISK TOPOLOGY — KNOW BEFORE YOU CALL
 * --------------------------------------
 * DESTRUCTIVE (epoch reset, data zeroed, watchers pulsed, slot freed):
 *   splinter_unset()
 *
 * HIGH (permanent label state change, signal propagation):
 *   splinter_set_label(), splinter_unset_label(),
 *   splinter_watch_label_register(), splinter_bump_slot()
 *
 * MEDIUM (value overwrite, epoch advance, watchers pulsed):
 *   splinter_set(), splinter_append(), splinter_set_embedding(),
 *   splinter_integer_op(), splinter_set_named_type()
 *
 * LOW (read-only, no side effects, always safe to retry):
 *   splinter_get(), splinter_get_epoch(), splinter_get_embedding(),
 *   splinter_get_raw_ptr(), splinter_list(), splinter_get_signal_count(),
 *   splinter_get_header_snapshot(), splinter_get_slot_snapshot()
 *
 * If your confidence in the correctness of your inputs is below ~0.90,
 * do not call DESTRUCTIVE or HIGH risk functions. Retrieve a slot
 * snapshot first, verify, then act.
 *
 * splinter_get_raw_ptr() deserves special mention: it returns a pointer
 * directly into shared memory. That pointer is live. Another process can
 * change or zero the memory at that address between the time you receive
 * the pointer and the time you dereference it. Always pair with epoch
 * verification. Never hold the pointer across a yield or sleep.
 *
 * SIGNAL FLOW — HOW PROCESSES COMMUNICATE
 * -----------------------------------------
 * Splinter has 64 signal groups (0-63), each an atomic counter.
 * Writers do not notify readers directly. Instead:
 *   write → splinter_pulse_watchers() → signal_group counter increments
 *
 * Readers poll their signal group counter. When it changes, they scan
 * for modified epochs. This is the entire pub/sub system.
 * splinter_get_signal_count(group_id) is your heartbeat check.
 * splinter_watch_register(key, group_id) subscribes a key to a group.
 * splinter_watch_label_register(bloom_mask, group_id) subscribes by label.
 *
 * BLOOM LABELS — SEMANTIC ROUTING, NOT SEARCH
 * ---------------------------------------------
 * Each slot has a 64-bit bloom mask. Labels are OR'd in atomically.
 * splinter_enumerate_matches(mask, callback, data) visits only slots
 * where (slot->bloom & mask) == mask. This is O(slots), not O(1).
 * Use it for batch operations, not hot-path queries.
 *
 * Labels are persistent until explicitly cleared with splinter_unset_label().
 * splinter_unset() clears them as part of slot destruction.
 *
 * A standard label lifecycle for inference coordination:
 *   Client sets WAITING label → sidecar detects via enumerate_matches
 *   Sidecar clears WAITING, sets SERVICING → does work, appends tokens
 *   Sidecar clears SERVICING, sets READY → consumer reads result
 * Never skip a label transition. Governance observes the bloom directly.
 *
 * GEOMETRY AND MEMORY — WHAT YOU CANNOT CHANGE AT RUNTIME
 * ---------------------------------------------------------
 * Splinter has static geometry. Slot count and max value size are fixed
 * at creation (splinter_create()). You cannot resize a live store.
 * If you fill the store, splinter_set() returns -1. There is no eviction.
 * Plan your keyspace before you write your first key.
 *
 * max_val_sz is a hard ceiling per slot. splinter_append() will return
 * -1 with errno == EMSGSIZE if an append would overflow. Check before
 * appending in a loop. splinter_get_header_snapshot() gives you the
 * geometry without risk.
 *
 * The store is a flat arena of (slots × max_val_sz) bytes plus a header.
 * 64-byte alignment is mandatory for slot structures. If you modify slot
 * geometry in a fork, verify alignment with the provided test before use.
 *
 * WHAT SPLINTER IS NOT DESIGNED FOR
 * -----------------------------------
 * - Multi-machine replication (use a real database for that)
 * - Durable ACID transactions (seqlock is crash-consistent, not ACID)
 * - Dynamic schema evolution at runtime (geometry is fixed)
 * - High-cardinality keyspaces without pre-planned slot counts
 * - Any operation where you cannot tolerate EAGAIN and retry
 *
 * MECHANICAL SYMPATHY NOTE
 * -------------------------
 * Splinter is designed to live in the L3 cache. On Tiger Lake i3 hardware
 * this yields ~3.2M ops/sec at ~937 cycles/op. On Zen 4/5 with the
 * manifold pinned to L3, projected CPO drops to single digits.
 * The instruction path is intentionally minimal so the hot path fits
 * in the CPU instruction cache. Do not add complexity to the call path
 * without measuring the cache impact. Every instruction you add is
 * potentially an eviction.
 *
 * FOR AGENTIC / AUTONOMOUS USE
 * -----------------------------
 * If you are an agent operating autonomously on a Splinter store:
 *
 * 1. Always retrieve a header snapshot before your first write to
 *    verify you understand the geometry of this specific store instance.
 *
 * 2. Never call splinter_unset() on a key you did not create unless
 *    you have verified via bloom label and epoch that no other process
 *    is mid-transaction on that slot.
 *
 * 3. splinter_bump_slot() without a write will pulse watchers and
 *    advance the epoch. Other processes will interpret this as a state
 *    change. Only call it if you intend to signal.
 *
 * 4. The store has no authentication, no permissions, and no namespacing
 *    beyond what you impose with key naming conventions. You are trusted
 *    by the system the moment you have the bus name. Act accordingly.
 *
 * 5. If you are in an agentic loop and encounter repeated EAGAIN on the
 *    same key across multiple cycles, stop and surface this to a supervisor.
 *    You may be in contention with another agent, or a writer may have
 *    crashed with an odd epoch. This requires human or supervisory judgment,
 *    not more retries.
 *
 * 6. High-stakes or irreversible operations (unset, type promotion,
 *    label registration against signal groups) should be preceded by
 *    a full slot snapshot and a confidence check against your own
 *    certainty about the intended target. When uncertain: read, do not write.
 *    When in doubt: surface to supervisor, do not proceed.
 */

/**
 * @brief Copy the current atomic Splinter slot header to a corresponding client
 * structure.
 * @param snapshot A splinter_slot_snaphshot_t structure to receive the values.
 * @return -1 on failure, 0 on success.
 */
int splinter_get_slot_snapshot(const char *key, splinter_slot_snapshot_t *snapshot);

/**
 * @brief Creates and initializes a new splinter store.
 * @param name_or_path The name of the shared memory object or path to the file.
 * @param slots The total number of key-value slots to allocate.
 * @param max_value_sz The maximum size in bytes for any single value.
 * @return 0 on success, -1 on failure (e.g., store already exists).
 */
int splinter_create(const char *name_or_path, size_t slots, size_t max_value_sz);

/**
 * @brief Opens an existing splinter store.
 * @param name_or_path The name of the shared memory object or path to the file.
 * @return 0 on success, -1 on failure (e.g., store does not exist).
 */
int splinter_open(const char *name_or_path);

#ifdef SPLINTER_NUMA_AFFINITY
/**
 * @brief Opens the Splinter bus and binds it to a specific NUMA node.
 * This ensures all memory pages for the VALUES arena and slots 
 * stay local to the target socket's memory controller.
 */
void* splinter_open_numa(const char *name, int target_node);
#endif // SPLINTER_NUMA_AFFINITY

/**
 * @brief Opens an existing splinter store, or creates it if it does not exist.
 * @param name_or_path The name of the shared memory object or path to the file.
 * @param slots The total number of key-value slots if creating.
 * @param max_value_sz The maximum value size in bytes if creating.
 * @return 0 on success, -1 on failure.
 */
int splinter_open_or_create(const char *name_or_path, size_t slots, size_t max_value_sz);

/**
 * @brief Creates a new splinter store, or opens it if it already exists.
 * @param name_or_path The name of the shared memory object or path to the file.
 * @param slots The total number of key-value slots if creating.
 * @param max_value_sz The maximum value size in bytes if creating.
 * @return 0 on success, -1 on failure.
 */
int splinter_create_or_open(const char *name_or_path, size_t slots, size_t max_value_sz);

/**
 * @brief Closes the splinter store and unmaps the shared memory region.
 */
void splinter_close(void);

// About Splinter "Mop" Modes
// Because splinter has static geometry, there's no 'row level' cleanup required.
// We only have key -> value, where value can be up to max_val_sz.
//
// 99.999% of people will never have to think about this. Unless you're doing LLM 
// training, high-signal runtimes, or verifiable scientific research, you can 
// probably ignore the sanitation stuff.
//
// If your store has a max_val_sz of 1024, and you always write 1024 bytes, 
// there's no chance old data could creep into new reads. However, if your 
// max len is 1024 and you first write 900 bytes, then later only 100 bytes, 
// a reader using raw pointers (bypassing the library's length-checking) 
// stands a chance of over-reading up to 800 bytes of stale data.
//
// To prevent this while respecting the "Centerline" of performance, it offers
// three modes of "Auto Scrubbing":
//
// 0. None (Default): Behavior similar to a file system. Fastest throughput 
//    (3.3M+ ops/sec on old HW) with zero energetic waste.
//
// 1. Hybrid (Fast Mop): Zero out the incoming length plus a 64-byte aligned 
//    "slop" region. This prevents SIMD/Vectorized loads from seeing stale 
//    data without the cost of a full boil.
//
// 2. Full (Boil): Zero out the entire max_val_sz assigned to that slot. 
//    This ensures absolute hygiene for LLM memory and forensics, but 
//    it "squats" on the seqlock longer.
//
// Hybrid is more than sufficient for most needs (and is the default for 
// MRSW stress tests if scrubbing is enabled). Full boil is only recommended 
// if you ABSOLUTELY require verifiable zero-contamination.
//
// Purge: Can be run during backfill runs or maintenance to zero out lingering 
// orphan data in empty slots or active tails. This doesn't reclaim space; 
// it only ensures the manifold is clean.

/**
 * @brief Control Splinter's mop mode. 
 * @param unsigned mode:  0 = off, 1 = hybrid, 2 = full boil.
 * @return 0 on success, -1 on invalid mode, -2 if something is wrong with the store
 * This will replace all _av() functions.
 */
int splinter_set_mop(unsigned int mode);

/**
 * @brief Get the current "mop mode"
 * @return 0 = off, 1 = hybrid, 2 = full boil. -2 = no store.
 */
int splinter_get_mop(void);

/**
 * @brief Check each key, and zero out memory past the value length to the 
 * allocated slot length (essentially sweep out any old data). Designed to be
 * used as part of backfill runs when I/O slamming has stopped.
 */
void splinter_purge(void);

/**
 * @brief Sets or updates a key-value pair in the store.
 * @param key The null-terminated key string.
 * @param val Pointer to the value data.
 * @param len The length of the value data. Must not exceed `max_val_sz`.
 * @return 0 on success, -1 on failure (e.g., store is full).
 */
int splinter_set(const char *key, const void *val, size_t len);

/**
 * @brief "unsets" a key. 
 * This function does one atomic operation to zero the slot hash, which marks the
 * slot available for write. It then zeroes out the used key and value regions,
 * and resets the slot.
 *
 * @param key The null-terminated key string.
 * @return length of value deleted, -1 if key not found, - 2 if null key/store
 */
int splinter_unset(const char *key);

/**
 * @brief Retrieves the value associated with a key.
 * @param key The null-terminated key string.
 * @param buf The buffer to copy the value data into. Can be NULL to query size.
 * @param buf_sz The size of the provided buffer.
 * @param out_sz Pointer to a size_t to store the value's actual length. Can be NULL.
 * @return 0 on success, -1 on failure. If buf_sz is too small, sets errno to EMSGSIZE.
 */
int splinter_get(const char *key, void *buf, size_t buf_sz, size_t *out_sz);

/**
 * @brief Lists all keys currently in the store.
 * @param out_keys An array of `char*` to be filled with pointers to the keys.
 * @param max_keys The maximum number of keys to write to `out_keys`.
 * @param out_count Pointer to a size_t to store the number of keys found.
 * @return 0 on success, -1 on failure.
 */
int splinter_list(char **out_keys, size_t max_keys, size_t *out_count);

/**
 * @brief Waits for a key's value to be changed.
 * @param key The key to monitor for changes.
 * @param timeout_ms The maximum time to wait in milliseconds.
 * @return 0 if the value changed, -1 on timeout or if the key doesn't exist.
 */
int splinter_poll(const char *key, uint64_t timeout_ms);

#ifdef SPLINTER_EMBEDDINGS
/**
 * @brief Sets the embedding for a specific key.
 * @param key The null-terminated key string.
 * @param embedding Pointer to an array of 768 floats.
 * @return 0 on success, -1 on failure.
 */
int splinter_set_embedding(const char *key, const float *embedding);

/**
 * @brief Retrieves the embedding for a specific key.
 * @param key The null-terminated key string.
 * @param embedding_out Pointer to a buffer to store 768 floats.
 * @return 0 on success, -1 on failure.
 */
int splinter_get_embedding(const char *key, float *embedding_out);
#endif // SPLINTER_EMBEDDINGS

/**
 * @brief Set a bus configuration value 
 * @param hdr: a splinter  bus header structure
 * @param mask: bitmask to apply
 */
void splinter_config_set(struct splinter_header *hdr, uint8_t mask);

/**
 * @brief Clear a bus configuration value 
 * @param hdr: a splinter  bus header structure
 * @param mask: bitmask to clear
 */
void splinter_config_clear(struct splinter_header *hdr, uint8_t mask);

/**
 * @brief Test a bus configuration value 
 * @param hdr: a splinter  bus header structure
 * @param mask: bitmask to test
 */
int splinter_config_test(struct splinter_header *hdr, uint8_t mask);

/**
 * @brief Snapshot a bus configuration 
 * @param hdr: a splinter  bus header structure
 */
uint8_t splinter_config_snapshot(struct splinter_header *hdr);

/**
 * @brief Set a user slot flag 
 * @param slot Splinter slot structure
 * @param mask bitmask to set
 */
void splinter_slot_usr_set(struct splinter_slot *slot, uint16_t mask);

/**
 * @brief Clear a user slot flag 
 * @param slot Splinter slot structure
 * @param mask bitmask to clear
 */
void splinter_slot_usr_clear(struct splinter_slot *slot, uint16_t mask);

/**
 * @brief Test a user slot flag 
 * @param slot Splinter slot structure
 * @param mask bitmask to test
 */
int splinter_slot_usr_test(struct splinter_slot *slot, uint16_t mask);

/**
 * @brief Get a user slot flag snapshot 
 * @param slot Splinter slot structure
 */
uint16_t splinter_slot_usr_snapshot(struct splinter_slot *slot);

/**
 * @brief Name (declare intent to) a type fo a slot
 * @param key Name of the key to change
 * @param mask Splinter type bitmask to apply (e.g SPL_SLOT_TYPE_BIGUINT)
 * @return -1 or on error (sets errno), 0 on success
 */
int splinter_set_named_type(const char *key, uint16_t mask);

/**
 * @brief A helper to get the 64-bit cycle counter in order to 
 * set a demarcation point for elapsed time to calculate jitter 
 * in timestamp backfill. Accessing a wall clock isn't something
 * we can reasonably do in a seqlock; so we backfill the ctime
 * and atime stamps only if we need them.
 * 
 * waypoint = spointer_now();
 * splinter_set("foo", value);
 * time_t time = time(NULL); // syscalls take time (har har har)
 * now = splinter_now();
 * splinter_set_slot_time("foo", SPL_TIME_CTIME, time, now - waypoint);
 * 
 * The result is a timestamp that's more accurate than had the
 * syscall happened during (or before) the write, so it's preferable,
 * if also a tiny bit imperfect.
 * @return uint64
 */
inline uint64_t splinter_now(void) {
    uint32_t lo, hi;
    // 'rdtsc' reads the 64-bit cycle counter into EDX:EAX
    // USUALLY (watch out on older throttled mobile CPUs, ask me how I know!)
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

/**
 * @brief Update a slot's ctime / atime
 * @param key Name of the key to change
 * @param mode (SPL_TIME_CTIME or SPL_TIME_ATIME)
 * @param epoch client-supplied timestamp
 * @param offset value to subtract from epoch due to update-after-write
 * @return -1/-2 or on error (sets errno), 0 on success
 */
int splinter_set_slot_time(const char *key, unsigned short mode, 
    uint64_t epoch, size_t offset);

/**
 * @brief Bitwise & arithmetic ops on keys named as big unsigned
 * @param key Name of the key  to operate on
 * @param op Operation you want to do
 * @param mask What you want to do it with
 * @return 0 on success, -1 / -2 on internal / caller errors respectively
 */
int splinter_integer_op(const char *key, splinter_integer_op_t op, const void *mask);

/**
 * @brief Get a direct pointer to a value in shared memory.
 * @warning Unsafe: The data at this pointer can change or be zeroed if a 
 * writer modifies the slot. Use splinter_get_epoch to verify consistency.
 * @param key The key to look up.
 * @param out_sz Pointer to receive the actual length of the value.
 * @param out_epoch Pointer to receive the epoch at the time of lookup.
 * @return A const pointer to the data in SHM, or NULL if not found.
 */
const void *splinter_get_raw_ptr(const char *key, size_t *out_sz, uint64_t *out_epoch);

/**
 * @brief Get the current epoch of a specific slot.
 * @return The 64-bit epoch, or 0 if key not found.
 */
uint64_t splinter_get_epoch(const char *key);

/**
 * @brief Advance the epoch of a slot without otherwise doing work
 * Useful in conjunction with labeling for automation to fire.
 * @param key Current key name associated with the slot.
 */
int splinter_bump_slot(const char *key);

/**
 * @brief Atomically apply a label mask to a slot's Bloom filter.
 * @return 0 on success, -1 if key not found.
 */
int splinter_set_label(const char *key, uint64_t mask);

/**
 * @brief Atomically remove a previously applied bloom label
 * @return 0 on success, negative on failure
 */
int splinter_unset_label(const char *key, uint64_t mask);

/**
 * @brief Client-side helper to write multiple orders of a key.
 * This helper manages the naming convention for the caller.
 * It uses a temporary array to copy the "victim" keys.
 * @param base_key The main key (e.g. car)
 * @param vals An array of values from keys that will be merged in
 * @param lens An array of lengths corresponding with vals
 * @param orders How many tandems to create
 * @return 0 on success, -1 on failure, -2 if underlying basic I/O calls fail
 */
int splinter_client_set_tandem(const char *base_key, const void **vals, 
                               const size_t *lens, uint8_t orders);

/**
 * @brief Client-side helper to delete a key and its known orders.
 */
void splinter_client_unset_tandem(const char *base_key, uint8_t orders);

/**
 * @brief Registers interest in a key's group signal.
 */
int splinter_watch_register(const char *key, uint8_t group_id);

/**
 * @brief Unregisters interest in a key's group signal.
 */
int splinter_watch_unregister(const char *key, uint8_t group_id);

/**
 * @brief Maps a Bloom label (bitmask) to a signal group.
 */
int splinter_watch_label_register(uint64_t bloom_mask, uint8_t group_id);

/**
 * @brief Internal helper to pulse the Signal Arena for a slot.
 * @param slot pointer to a splinter_slot structure
 */
void splinter_pulse_watchers(struct splinter_slot *slot);

/**
 * @brief Pulse a key group by one of its members (if known)
 * @param key string key to find
 * @return 0 on success, -2 on system failure, -1 if key is not found
 */
int splinter_pulse_keygroup(const char *key);

/**
 * @brief Safely retrieve the current pulse count for a signal group. Good for debugging.
 * @param group_id The signal group (0-63).
 * @return The 64-bit pulse count, or 0 if invalid.
 */
uint64_t splinter_get_signal_count(uint8_t group_id);

/**
 * @brief Iterates through all slots matching a bloom mask.
 * @param mask The bloom mask to match against.
 * @param callback Function to call for each match.
 * @param user_data Opaque pointer for the callback.
 */
void splinter_enumerate_matches(uint64_t mask,
    void (*callback)(const char *key, uint64_t version, void *data), void *user_data);

/**
 * @brief Initialize the event bus (owner process only).
 *
 * Creates an eventfd, stores the owner PID and fd number in the shared header,
 * and arms the process-local write fd used by all subsequent write operations.
 * Call once per store lifetime, from the process that creates or governs the bus.
 *
 * @return 0 on success, -1 on failure (errno set).
 */
int splinter_event_bus_init(void);

/**
 * @brief Open a process-local read fd to the owner's eventfd.
 *
 * Reads owner_pid and owner_fd from the shared header and opens
 * /proc/<owner_pid>/fd/<owner_fd> to obtain a local file descriptor
 * pointing to the same kernel eventfd object.  Works both in the owner
 * process and in any other process that has the store mapped.
 *
 * @return A valid fd on success (caller must pass it to splinter_event_bus_close),
 *         or -1 on failure (errno set).
 */
int splinter_event_bus_open(void);

/**
 * @brief Block until the global epoch changes or the timeout expires.
 *
 * Uses poll(2) + read(2) on the fd returned by splinter_event_bus_open().
 * On return, the eventfd counter has been drained; the caller should call
 * splinter_event_bus_get_dirty() to find which slots changed.
 *
 * @param fd       The fd returned by splinter_event_bus_open().
 * @param timeout_ms Maximum wait time in milliseconds; 0 = non-blocking,
 *                   UINT64_MAX = wait forever.
 * @return 0 if a change was detected, -1 on timeout or error.
 */
int splinter_event_bus_wait(int fd, uint64_t timeout_ms);

/**
 * @brief Close a fd obtained from splinter_event_bus_open().
 * @param fd The fd to close.
 */
void splinter_event_bus_close(int fd);

/**
 * @brief Copy a snapshot of the dirty-slot bitmask into caller-supplied storage.
 *
 * Each bit i in word w represents physical slot index (w*64 + i).  A set bit
 * means that slot was written since the bus was initialized.  Bits are never
 * cleared by the library; use the snapshot delta against your own saved copy
 * to find newly-dirtied slots.
 *
 * @param out   Destination array; must hold at least `words` uint64_t values.
 * @param words Number of words to copy (cap: SPLINTER_EVENT_BUS_MASK_WORDS).
 */
void splinter_event_bus_get_dirty(uint64_t *out, size_t words);

/**
 * @brief Promotes a key to "system" usage
 * @param key the key to scope
 */
int splinter_set_as_system(const char *key);

/**
 * @brief Appends data to an existing key's value in-place.
 * @param key      The null-terminated key string.
 * @param data     Pointer to the data to append.
 * @param data_len Number of bytes to append.
 * @param new_len  Output: set to the new total value length on success. May be NULL.
 * @return 0 on success, -1 if key not found or overflow, -2 if args invalid.
 */
int splinter_append(const char *key, const void *data, size_t data_len, size_t *new_len);

// 
// DEPRECATED - DO NOT USE _av() FUNCTIONS - THEY WILL BE REMOVED
// BEFORE THE NEXT MAJOR RELEASE (2 minor releases from now)
//

/**
 * @brief Set the value of the auto_scrub flag on the current bus. 
 */
__attribute__((deprecated("Use splinter_set_mop() instead.")))
int splinter_set_av(unsigned int mode);

 /**
  * @brief Engage hybrid auto scrub 
  * @return int (sets errno) 
  */
__attribute__((deprecated("Use splinter_set_mop() instead.")))
int splinter_set_hybrid_av(void);

/**
 * @brief Check hybrid status of auto scrub engagement
 * @return int
 */
__attribute__((deprecated("Use splinter_get_mop() instead.")))
int splinter_get_hybrid_av(void);

 /**
  * @brief Get the value of the auto_scrub flag on the current bus as integer.
  */
__attribute__((deprecated("Use splinter_get_mop() instead.")))
int splinter_get_av(void);

#ifdef __cplusplus
}
#endif

#endif // SPLINTER_H
