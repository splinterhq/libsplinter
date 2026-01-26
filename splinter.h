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
 * @brief (SLNT) Magic number to identify a splinter memory region. 
 * Spoiler: bytes 53 4C 4E 54 -> ASCII "S L N T" (never speaks unless spoken to)
 */
#define SPLINTER_MAGIC 0x534C4E54

/** @brief Version of the splinter data format (not the library version). */
#define SPLINTER_VER   2
/** @brief Maximum length of a key string, including null terminator. */
#define SPLINTER_KEY_MAX        64
/** @brief Nanoseconds per millisecond for time calculations. */
#define NS_PER_MS      1000000ULL

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
    /** @brief toggle for zeroing out the value region prior to writing there. */
    atomic_uint_least32_t auto_vacuum;

    /* Diagnostics: counts of parse failures reported by clients / harnesses */
    atomic_uint_least64_t parse_failures;
    atomic_uint_least64_t last_failure_epoch;
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
    /** @brief The null-terminated key string. */
    char key[SPLINTER_KEY_MAX];
};

/**
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
    /** @brief toggle for zeroing out the value region prior to writing there. */
    uint32_t auto_vacuum;

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

typedef struct splinter_slot_snapshot {
    /** @brief The FNV-1a hash of the key. 0 indicates an empty slot. */
    uint64_t hash;
    /** @brief Per-slot epoch, incremented on write to this slot. Used for polling. */
    uint64_t epoch;
    /** @brief Offset into the VALUES region where the value data is stored. */
    uint32_t val_off;
    /** @brief The actual length of the stored value data (atomic). */
    uint32_t val_len;
    /** @brief The null-terminated key string. */
    char key[SPLINTER_KEY_MAX];
} splinter_slot_snapshot_t;

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

/**
 * @brief Set the value of the auto_vacuum flag on the current bus. 
 */
 int splinter_set_av(unsigned int mode);

 /**
  * @brief Get the value of the auto_vacuum flag on the current bus as integer.
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

#ifdef __cplusplus
}
#endif

#endif // SPLINTER_H
