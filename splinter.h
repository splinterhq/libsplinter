/**
 * Copyright 2025 Tim Post
 * License: Apache 2
 * @file splinter.h
 * @brief Public API for the libsplinter shared memory key-value store.
 *
 * This header defines the public functions for creating, opening, interacting
 * with, and closing a splinter store.
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
#define SPLINTER_VER   2
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
 * @brief Individual signal lane, aligned to prevent false sharing.
 */
struct splinter_signal_node {
    alignas(64) atomic_uint_least64_t counter;
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


// About Splinter AV / Auto Scrub
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
// 1. None (Default): Behavior similar to a file system. Fastest throughput 
//    (3.3M+ ops/sec on old HW) with zero energetic waste.
//
// 2. Hybrid (Fast Mop): Zero out the incoming length plus a 64-byte aligned 
//    "slop" region. This prevents SIMD/Vectorized loads from seeing stale 
//    data without the cost of a full boil.
//
// 3. Full (Boil): Zero out the entire max_val_sz assigned to that slot. 
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
 * @brief Set the value of the auto_scrub flag on the current bus. 
 */
 int splinter_set_av(unsigned int mode);

 /**
  * @brief Engage hybrid auto scrub 
  * @return int (sets errno) 
  */
int splinter_set_hybrid_av(void);

/**
 * @brief Check hybrid status of auto scrub engagement
 * @return int
 */
int splinter_get_hybrid_av(void);

/**
 * @brief Check each key, and zero out memory past the value length to the 
 * allocated slot length (essentially sweep out any old data). Designed to be
 * used as part of backfill runs when I/O slamming has stopped.
 */
void splinter_purge(void);

 /**
  * @brief Get the value of the auto_scrub flag on the current bus as integer.
  */
int splinter_get_av(void);

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
 * @brief Atomically apply a label mask to a slot's Bloom filter.
 * @return 0 on success, -1 if key not found.
 */
int splinter_set_label(const char *key, uint64_t mask);


// Splinter client functions (combined library calls for convenience)

/**
 * @brief Client-side helper to write multiple orders of a key.
 * * Since we've backed out library-side linking, this helper manages
 * the naming convention for the caller.
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
 */
void splinter_pulse_watchers(struct splinter_slot *slot);

/**
 * @brief Safely retrieve the current pulse count for a signal group. Good for debugging.
 * @param group_id The signal group (0-63).
 * @return The 64-bit pulse count, or 0 if invalid.
 */
uint64_t splinter_get_signal_count(uint8_t group_id);

#ifdef __cplusplus
}
#endif

#endif // SPLINTER_H
