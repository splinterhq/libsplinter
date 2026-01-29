/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter.c
 * @brief Main implementation of the libsplinter shared memory key-value store.
 *
 * libsplinter provides a high-performance, lock-free, shared-memory key-value
 * store and message bus. It is designed for efficient inter-process
 * communication (IPC), particularly for building process communities around 
 * local Large Language Model (LLM) runtimes.
 * 
 * See docs for more.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "splinter.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include "config.h"

/** @brief Base pointer to the memory-mapped region. */
static void *g_base = NULL;
/** @brief Total size of the memory-mapped region. */
static size_t g_total_sz = 0;
/** @brief Pointer to the header within the mapped region. */
static struct splinter_header *H;
/** @brief Pointer to the array of slots within the mapped region. */
static struct splinter_slot *S;
/** @brief Pointer to the start of the value storage area. */
static uint8_t *VALUES;

/**
 * @brief Computes the 64-bit FNV-1a hash of a string.
 * @param s The null-terminated string to hash.
 * @return The 64-bit hash value.
 */
static uint64_t fnv1a(const char *s) {
    uint64_t h = 14695981039346656037ULL;
    for (; *s; ++s) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
    return h;
}

/**
 * @brief Calculates the initial slot index for a given hash.
 * @param hash The hash of the key.
 * @param slots The total number of slots in the store.
 * @return The calculated slot index.
 */
static inline size_t slot_idx(uint64_t hash, uint32_t slots) {
    return (size_t)(hash % slots);
}

/**
 * @brief Adds a specified number of milliseconds to a timespec struct.
 * @param ts Pointer to the timespec struct to modify.
 * @param ms The number of milliseconds to add.
 */
static void add_ms(struct timespec *ts, uint64_t ms) {
    ts->tv_nsec += (ms % 1000) * NS_PER_MS;
    ts->tv_sec  += ms / 1000;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_nsec -= 1000000000L;
        ts->tv_sec  += 1;
    }
}

/**
 * @brief Internal helper to memory-map a file descriptor and set up global pointers.
 * @param fd The file descriptor to map.
 * @param size The size of the region to map.
 * @return 0 on success, -1 on failure.
 */
static int map_fd(int fd, size_t size) {
    g_total_sz = size;
    g_base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (g_base == MAP_FAILED) return -1;
    H = (struct splinter_header *)g_base;
    S = (struct splinter_slot *)(H + 1);
    VALUES = (uint8_t *)(S + H->slots);
    return 0;
}

/**
 * @brief Creates and initializes a new splinter store.
 *
 * The store is created as a shared memory object (`/dev/shm/...`) unless the
 * `SPLINTER_PERSISTENT` macro is defined, in which case it's a regular file.
 * The function fails if the store already exists.
 *
 * @param name_or_path The name of the shared memory object or path to the file.
 * @param slots The total number of key-value slots to allocate.
 * @param max_value_sz The maximum size in bytes for any single value.
 * @return 0 on success, -1 on failure.
 */
int splinter_create(const char *name_or_path, size_t slots, size_t max_value_sz) {
    int fd;

    if (slots <= 0 || max_value_sz <= 0) {
        errno = ENOTSUP;
        return -2;
    }

#ifdef SPLINTER_PERSISTENT
    fd = open(name_or_path, O_RDWR | O_CREAT, 0666);
#else
    // O_EXCL ensures this fails if the object already exists.
    fd = shm_open(name_or_path, O_RDWR | O_CREAT | O_EXCL, 0666);
#endif
    if (fd < 0) return -1;
    size_t region_sz = slots * max_value_sz;
    size_t total_sz  = sizeof(struct splinter_header) + slots * sizeof(struct splinter_slot) + region_sz;
    if (ftruncate(fd, (off_t)total_sz) != 0) return -1;
    if (map_fd(fd, total_sz) != 0) return -1;
    
    // Initialize header
    H->magic = SPLINTER_MAGIC;
    H->version = SPLINTER_VER;
    H->slots = (uint32_t)slots;
    H->max_val_sz = (uint32_t)max_value_sz;
    H->val_sz = total_sz;
    atomic_store_explicit(&H->val_brk, 0, memory_order_relaxed);
    atomic_store_explicit(&H->epoch, 1, memory_order_relaxed);
    atomic_store_explicit(&H->core_flags, 0, memory_order_relaxed);
    atomic_store_explicit(&H->user_flags, 0, memory_order_relaxed);
    atomic_store_explicit(&H->parse_failures, 0, memory_order_relaxed);
    atomic_store_explicit(&H->last_failure_epoch, 0, memory_order_relaxed);
    
    // Initialize slots
    size_t i;
    for (i = 0; i < slots; ++i) {
        atomic_fetch_or(&S[i].type_flag, SPL_SLOT_DEFAULT_TYPE);
        atomic_store_explicit(&S[i].hash, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].epoch, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].ctime, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].atime, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].user_flag, 0, memory_order_relaxed);
        S[i].val_off = (uint32_t)(i * max_value_sz);
        atomic_store_explicit(&S[i].val_len, 0, memory_order_relaxed);
        S[i].key[0] = '\0';      
    }
    return 0;
}

/**
 * @brief Opens an existing splinter store.
 *
 * The function fails if the store does not exist or if the header metadata
 * (magic number, version) is invalid.
 *
 * @param name_or_path The name of the shared memory object or path to the file.
 * @return 0 on success, -1 on failure.
 */
int splinter_open(const char *name_or_path) {
    int fd;
#ifdef SPLINTER_PERSISTENT
    fd = open(name_or_path, O_RDWR);
#else
    fd = shm_open(name_or_path, O_RDWR, 0666);
#endif
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) != 0) return -1;
    if (map_fd(fd, (size_t)st.st_size) != 0) return -1;
    
    // Validate header
    if (H->magic != SPLINTER_MAGIC || H->version != SPLINTER_VER) return -1;
    return 0;
}

/**
 * @brief Creates a new splinter store, or opens it if it already exists.
 *
 * Tries to create first, and on failure, tries to open.
 *
 * @param name_or_path The name of the shared memory object or path to the file.
 * @param slots The total number of key-value slots if creating.
 * @param max_value_sz The maximum value size in bytes if creating.
 * @return 0 on success, -1 on failure.
 */
int splinter_create_or_open(const char *name_or_path, size_t slots, size_t max_value_sz) {
    int ret = splinter_create(name_or_path, slots, max_value_sz);
    return (ret == 0 ? ret : splinter_open(name_or_path));
}

/**
 * @brief Opens an existing splinter store, or creates it if it does not exist.
 *
 * Tries to open first, and on failure, tries to create.
 *
 * @param name_or_path The name of the shared memory object or path to the file.
 * @param slots The total number of key-value slots if creating.
 * @param max_value_sz The maximum value size in bytes if creating.
 * @return 0 on success, -1 on failure.
 */
int splinter_open_or_create(const char *name_or_path, size_t slots, size_t max_value_sz) {
    int ret = splinter_open(name_or_path);
    return (ret == 0 ? ret : splinter_create(name_or_path, slots, max_value_sz));
}

/**
 * @brief Sets the auto scrub atomic feature flag of the current bus (0 or 1)
 * @return -2 if the bus is unavailable, 0 otherwise.
 */
int splinter_set_av(unsigned int mode) {
    if (!H) return -2;

    switch (mode) {
        case 1:
            splinter_config_set(H, SPL_SYS_AUTO_SCRUB);
            return 0;
        case 0:
            splinter_config_clear(H, SPL_SYS_AUTO_SCRUB);
            return 0;
        default:
            errno = ENOTSUP;
            return -1;
    }
}

/**
 * @brief Get the auto scrub atomic feature flag of the current bus, as int.
 * @return -2 if the bus is unavailable, value of the (unsigned) flag otherwise. 
 */
int splinter_get_av(void) {
    if (!H) return -2;

    return (int) splinter_config_test(H, SPL_SYS_AUTO_SCRUB);
}

/**
 * @brief Closes the splinter store and unmaps the shared memory region.
 */
void splinter_close(void) {
    if (g_base) munmap(g_base, g_total_sz);
    g_base = NULL; H = NULL; S = NULL; VALUES = NULL; g_total_sz = 0;
}

/**
 * @brief "unsets" a key (delete).
 *
 * This function atomically marks the slot as free. With seqlock semantics,
 * if the slot is observed in the middle of a write (odd epoch), it returns
 * -1 with errno = EAGAIN so the caller can retry.
 *
 * @param key The null-terminated key string.
 * @return length of value deleted on success,
 *         -1 if key not found,
 *         -2 if store or key are invalid,
 *         -1 with errno = EAGAIN if writer in progress.
 */
int splinter_unset(const char *key) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    size_t i;
    for (i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        uint64_t slot_hash = atomic_load_explicit(&slot->hash, memory_order_acquire);

        if (slot_hash == h && strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            uint64_t start_epoch = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            if (start_epoch & 1) {
                // Writer in progress
                errno = EAGAIN;
                return -1;
            }

            int ret = (int)atomic_load_explicit(&slot->val_len, memory_order_acquire);
            atomic_store_explicit(&slot->hash, 0, memory_order_release);
            if (splinter_config_test(H, SPL_SYS_AUTO_SCRUB)) {
                memset(VALUES + slot->val_off, 0, H->max_val_sz);
                memset(slot->key, 0, SPLINTER_KEY_MAX);
            } else {
                slot->key[0] = '\0';
            }

            // we first zero out the type flag, then set it to the default.
            atomic_store_explicit(&slot->type_flag, 0, memory_order_release);
            atomic_fetch_or(&slot->type_flag, SPL_SLOT_DEFAULT_TYPE);

            // now the rest can be initialized
            atomic_store_explicit(&slot->val_len, 0, memory_order_release);
            atomic_store_explicit(&slot->ctime, 0, memory_order_release);
            atomic_store_explicit(&slot->atime, 0, memory_order_release);
            atomic_store_explicit(&slot->user_flag, 0, memory_order_release);

            // Increment slot epoch to mark the change (leave even)
            atomic_fetch_add_explicit(&slot->epoch, 2, memory_order_release);
            return ret;
        }
    }

    return -1; // didn't find it
}

/**
 * @brief Sets or updates a key-value pair in the store.
 *
 * This function uses linear probing to resolve hash collisions. It searches for
 * an empty slot or a slot with a matching key starting from the key's
 * natural hash position. If the store is full, the operation will fail.
 *
 * @param key The null-terminated key string.
 * @param val Pointer to the value data.
 * @param len The length of the value data. Must not exceed `max_val_sz`.
 * @return 0 on success, -1 on failure (e.g., store is full, len is too large).
 */
int splinter_set(const char *key, const void *val, size_t len) {
    if (!H || !key) return -1;
    if (len == 0 || len > H->max_val_sz) return -1; // require non-zero len

    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    const size_t arena_sz = (size_t)H->slots * (size_t)H->max_val_sz;

    size_t i;
    for (i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        uint64_t slot_hash = atomic_load_explicit(&slot->hash, memory_order_acquire);

        if (slot_hash == 0 || (slot_hash == h && strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0)) {
            // Try to acquire the slot's seqlock: flip epoch from even -> odd.
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1ull) {
                // writer already in progress for this slot; skip and continue probing
                continue;
            }
            // attempt to CAS epoch: e -> e+1 (make odd)
            uint64_t want = e + 1;
            if (!atomic_compare_exchange_weak_explicit(&slot->epoch, &e, want,
                                                      memory_order_acq_rel, memory_order_relaxed)) {
                // CAS failed (epoch changed), retry next slot
                continue;
            }

            // We have the slot in "writer active" (odd epoch) state.
            // Now validate the offset/range before touching memory.
            if ((size_t)slot->val_off >= arena_sz || (size_t)slot->val_off + len > arena_sz) {
                // leave epoch balanced (make it even again) and fail safely
                atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
                return -1;
            }

            // Perform the write: value -> val_len -> key -> publish hash -> complete epoch
            uint8_t *dst = (uint8_t *)VALUES + slot->val_off;

            // Clear full slot value region (keeps old tail bytes from leaking).
            if (splinter_config_test(H, SPL_SYS_AUTO_SCRUB)) {
                memset(VALUES + slot->val_off, 0, H->max_val_sz);
            }
            memcpy(dst, val, len);

            // Publish length atomically (release so readers see full bytes)
            atomic_store_explicit(&slot->val_len, (uint32_t)len, memory_order_release);

            // Update key (write full key buffer so readers can't see a partial key)
            if (splinter_config_test(H, SPL_SYS_AUTO_SCRUB)) {
                memset(slot->key, 0, SPLINTER_KEY_MAX);
            } else {
                slot->key[0] = '\0';
            }
            strncpy(slot->key, key, SPLINTER_KEY_MAX - 1);
            slot->key[SPLINTER_KEY_MAX - 1] = '\0';

            // Ensure prior stores are visible before publishing hash
            atomic_thread_fence(memory_order_release);

            // Only now publish the hash so readers will match only once value+key are in place.
            atomic_store_explicit(&slot->hash, h, memory_order_release);

            // End seqlock: bump epoch to even (writer done). Use release to publish writes.
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);

            // Update global epoch (best-effort, relaxed).
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);

            return 0;
        }
    }
    return -1; // store full / no suitable slot
}

/**
 * @brief Retrieves the value associated with a key (seqlock aware).
 *
 * @param key The null-terminated key string.
 * @param buf The buffer to copy the value data into. Can be NULL to query size.
 * @param buf_sz The size of the provided buffer.
 * @param out_sz Pointer to a size_t to store the value's actual length. Can be NULL.
 * @return 0 on success, -1 on failure. On retry condition, returns -1 and sets
 * errno = EAGAIN. If the buffer is too small, returns -1 and sets errno = EMSGSIZE.
 */
int splinter_get(const char *key, void *buf, size_t buf_sz, size_t *out_sz) {
    if (!H || !key) return -1;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    size_t i;
    for (i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];

        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            uint64_t start = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            if (start & 1) {
                // writer in progress
                errno = EAGAIN;
                return -1;
            }

	        atomic_thread_fence(memory_order_acquire);

            /* load length atomically */
            size_t len = (size_t)atomic_load_explicit(&slot->val_len, memory_order_acquire);
            if (out_sz) *out_sz = len;

            if (buf) {
                if (buf_sz < len) {
                    errno = EMSGSIZE;
                    return -1;
                }
                memcpy(buf, VALUES + slot->val_off, len);
            }

            uint64_t end = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            if (start == end && !(end & 1)) {
                // consistent snapshot
                return 0;
            }

            // inconsistent snapshot, ask caller to retry
            errno = EAGAIN;
            return -1;
        }
    }

    return -1; // Not found
}

/**
 * @brief Lists all keys currently in the store.
 *
 * @param out_keys An array of `char*` to be filled with pointers to the keys
 * within the shared memory. These pointers are only valid as
 * long as the store is open.
 * @param max_keys The maximum number of keys to write to `out_keys`.
 * @param out_count Pointer to a size_t to store the number of keys found.
 * @return 0 on success, -1 on failure.
 */
int splinter_list(char **out_keys, size_t max_keys, size_t *out_count) {
    if (!H || !out_keys || !out_count) return -1;
    size_t count = 0, i;
    
    for (i = 0; i < H->slots && count < max_keys; ++i) {
        // A non-zero hash and value length indicates a valid, active key.
        if (atomic_load_explicit(&S[i].hash, memory_order_acquire) &&
            atomic_load_explicit(&S[i].val_len, memory_order_acquire) > 0) {
            out_keys[count++] = S[i].key;
        }
    }
    *out_count = count;
    return 0;
}

/**
 * @brief Waits for a key's value to be changed (updated).
 *
 * This function provides a publish-subscribe mechanism. It blocks until the
 * per-slot epoch for the given key is incremented by a `splinter_set` call.
 *
 * With seqlock semantics, if the slot is observed in the middle of a write
 * (odd epoch), this call returns immediately with errno = EAGAIN so the
 * caller can retry cleanly.
 *
 * @param key The key to monitor for changes.
 * @param timeout_ms The maximum time to wait in milliseconds.
 * @return 0 if the value changed, -1 on timeout, -1 with errno = EAGAIN
 *         if a write was observed in progress.
 */
int splinter_poll(const char *key, uint64_t timeout_ms) {
    if (!H || !key) return -1;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    struct splinter_slot *slot = NULL;

    // Find the slot corresponding to the key
    size_t i;
    for (i = 0; i < H->slots; ++i) {
        struct splinter_slot *s = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&s->hash, memory_order_acquire) == h &&
            strncmp(s->key, key, SPLINTER_KEY_MAX) == 0) {
            slot = s;
            break;
        }
    }
    if (!slot) return -1; // Key does not exist.

    uint64_t start_epoch = atomic_load_explicit(&slot->epoch, memory_order_acquire);
    if (start_epoch & 1) {
        // Writer in progress
        errno = EAGAIN;
        return -2;
    }

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    add_ms(&deadline, timeout_ms);

    struct timespec sleep_ts = {0, 10 * NS_PER_MS}; // 10ms sleep
    while (1) {
        uint64_t cur_epoch = atomic_load_explicit(&slot->epoch, memory_order_acquire);
        if (cur_epoch & 1) {
            errno = EAGAIN;
            return -2; // Writer still in progress
        }
        if (cur_epoch != start_epoch) {
            return 0; // Value changed
        }

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if ((now.tv_sec > deadline.tv_sec) ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
            errno = ETIMEDOUT;
            return -2;
        }
        nanosleep(&sleep_ts, NULL);
    }
}

/**
 * @brief Copy the current atomic Splinter header structure into a corresponding
 * non-atomic client version.
 * @param snapshot A splinter_header_snaphshot_t structure to receive the values.
 * @return void
 */
int splinter_get_header_snapshot(splinter_header_snapshot_t *snapshot) {
    if (!H) return -1;

    snapshot->magic = H->magic;
    snapshot->version = H->version;
    snapshot->slots = H->slots;
    snapshot->max_val_sz = H->max_val_sz;
    
    snapshot->core_flags = atomic_load_explicit(&H->core_flags, memory_order_acquire);
    snapshot->user_flags = atomic_load_explicit(&H->user_flags, memory_order_acquire);
    snapshot->epoch = atomic_load_explicit(&H->epoch, memory_order_acquire);
    snapshot->parse_failures = atomic_load_explicit(&H->parse_failures, memory_order_relaxed);
    snapshot->last_failure_epoch = atomic_load_explicit(&H->last_failure_epoch, memory_order_relaxed);

    return 0;
}

/**
 * @brief Copy the current atomic Splinter slot header to a corresponding client
 * structure.
 * @param snapshot A splinter_slot_snaphshot_t structure to receive the values.
 * @return -1 on failure, 0 on success.
 */
int splinter_get_slot_snapshot(const char *key, splinter_slot_snapshot_t *snapshot) {
    if (!H || !key || !snapshot) return -1;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots), i = 0;
    
    for (i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            
            uint64_t start = 0, end = 0;
            do {
                start = atomic_load_explicit(&slot->epoch, memory_order_acquire);
                if (start & 1) {
                    // Writer active, spin briefly or return EAGAIN for the test to retry
                    continue; 
                }

                snapshot->hash = h;
                snapshot->epoch = start;
                snapshot->val_off = slot->val_off;
                snapshot->val_len = atomic_load_explicit(&slot->val_len, memory_order_relaxed);
                snapshot->type_flag = atomic_load_explicit(&slot->type_flag, memory_order_acquire);
                snapshot->user_flag = atomic_load_explicit(&slot->user_flag, memory_order_acquire);
                snapshot->ctime = atomic_load_explicit(&slot->ctime, memory_order_acquire);
                snapshot->atime = atomic_load_explicit(&slot->atime, memory_order_acquire);
                strncpy(snapshot->key, slot->key, SPLINTER_KEY_MAX);
#ifdef SPLINTER_EMBEDDINGS                
                // Copy the large vector (the high-risk area for tearing)
                memcpy(snapshot->embedding, slot->embedding, sizeof(float) * SPLINTER_EMBED_DIM);
#endif
                atomic_thread_fence(memory_order_acquire);
                end = atomic_load_explicit(&slot->epoch, memory_order_acquire);

            } while (start != end); // Loop until we get a clean, non-torn read

            return 0;
        }
    }
    return -1;
}

#ifdef SPLINTER_EMBEDDINGS
int splinter_set_embedding(const char *key, const float *vec) {
    if (!H || !key || !vec) return -1;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1ull) return -1; // Writer already active

            uint64_t want = e + 1;
            if (!atomic_compare_exchange_strong(&slot->epoch, &e, want)) return -1;

            // Perform the copy into the slot
            memcpy(slot->embedding, vec, sizeof(float) * SPLINTER_EMBED_DIM);

            /* RELEASE FENCE: Ensures all bytes of the embedding are written 
               to memory before the epoch is set back to an even number. */
            atomic_thread_fence(memory_order_release);

            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            
            // Global epoch update for bus tracking
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
            return 0;
        }
    }
    return -1;
}

int splinter_get_embedding(const char *key, float *embedding_out) {
    if (!H || !key || !embedding_out) return -1;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            
            uint64_t start = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            if (start & 1) { errno = EAGAIN; return -1; }

            atomic_thread_fence(memory_order_acquire);

            memcpy(embedding_out, slot->embedding, sizeof(float) * SPLINTER_EMBED_DIM);

            uint64_t end = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            if (start == end) return 0;

            errno = EAGAIN;
            return -1;
        }
    }
    return -1;
}
#endif // SPLINTER_EMBEDDINGS


/**
 * @brief Set a bus configuration value 
 * @param hdr: a splinter  bus header structure
 * @param mask: bitmask to apply
 */
void splinter_config_set(struct splinter_header *hdr, uint8_t mask) {
    atomic_fetch_or(&hdr->core_flags, mask);
}

/**
 * @brief Clear a bus configuration value 
 * @param hdr: a splinter  bus header structure
 * @param mask: bitmask to clear
 */
void splinter_config_clear(struct splinter_header *hdr, uint8_t mask) {
    atomic_fetch_and(&hdr->core_flags, ~mask);
}

/**
 * @brief Test a bus configuration value 
 * @param hdr: a splinter  bus header structure
 * @param mask: bitmask to test
 */
int splinter_config_test(struct splinter_header *hdr, uint8_t mask) {
    return (atomic_load(&hdr->core_flags) & mask) != 0;
}

/**
 * @brief Snapshot a bus configuration 
 * @param hdr: a splinter  bus header structure
 */
uint8_t splinter_config_snapshot(struct splinter_header *hdr) {
    return atomic_load(&hdr->core_flags);
}

/**
 * @brief Set a user slot flag 
 * @param slot Splinter slot structure
 * @param mask bitmask to set
 */
void splinter_slot_usr_set(struct splinter_slot *slot, uint16_t mask) {
  atomic_fetch_or(&slot->user_flag, mask);
}

/**
 * @brief Clear a user slot flag 
 * @param slot Splinter slot structure
 * @param mask bitmask to clear
 */
void splinter_slot_usr_clear(struct splinter_slot *slot, uint16_t mask) {
  atomic_fetch_and(&slot->user_flag, ~mask);
}

/**
 * @brief Test a user slot flag 
 * @param slot Splinter slot structure
 * @param mask bitmask to test
 */
int splinter_slot_usr_test(struct splinter_slot *slot, uint16_t mask) {
  return (atomic_load(&slot->user_flag) & mask) != 0;
}

/**
 * @brief Get a user slot flag snapshot 
 * @param slot Splinter slot structure
 */
uint16_t splinter_slot_usr_snapshot(struct splinter_slot *slot) {
  return atomic_load(&slot->user_flag);
}

/**
 * @brief Name (declare intent to) a type fo a slot
 * @param key Name of the key to change
 * @param mask Splinter type bitmask to apply (e.g SPL_SLOT_TYPE_BIGUINT)
 * @return -1 or on error (sets errno), 0 on success
 */
int splinter_set_named_type(const char *key, uint16_t mask) {
    if (!H || !key) return -1;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {

            // 1. Writer Check & Lock
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1) { errno = EAGAIN; return -1; }
            if (!atomic_compare_exchange_strong(&slot->epoch, &e, e + 1)) {
                errno = EAGAIN; return -1;
            }

            atomic_thread_fence(memory_order_acquire);

            // 2. Expansion Logic for BIGUINT
            uint32_t current_len = atomic_load(&slot->val_len);
            if ((mask & SPL_SLOT_TYPE_BIGUINT) && current_len < 8) {
                // Find fresh 8-byte space in arena
                uint32_t new_off = atomic_fetch_add(&H->val_brk, 8);
                
                // Safety check for arena overflow
                if (new_off + 8 > H->val_sz) {
                    atomic_fetch_add(&slot->epoch, 1); // Unlock
                    errno = ENOMEM;
                    return -1;
                }

                uint8_t *old_ptr = VALUES + slot->val_off;
                uint8_t *new_ptr = VALUES + new_off;
                
                // Move existing data and zero-fill the rest
                memset(new_ptr, 0, 8);
                memcpy(new_ptr, old_ptr, current_len);

                slot->val_off = new_off;
                atomic_store_explicit(&slot->val_len, 8, memory_order_relaxed);
            }

            // 3. Apply Type and Unlock
            atomic_store_explicit(&slot->type_flag, mask, memory_order_release);
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            
            // Global epoch update
            atomic_fetch_add(&H->epoch, 1); 
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}

/**
 * @brief Update a slot's ctime / atime
 * @param key Name of the key to change
 * @param mode (SPL_TIME_CTIME or SPL_TIME_ATIME)
 * @param epoch client-supplied timestamp
 * @param offset value to subtract from epoch due to update-after-write
 * @return -1/-2 or on error (sets errno), 0 on success
 */
int splinter_set_slot_time(const char *key, unsigned short mode, uint64_t epoch, size_t offset) {
  if (!H || !key) return -1;
  uint64_t h = fnv1a(key);
  size_t idx = slot_idx(h, H->slots), i;

  for (i = 0; i < H->slots; ++i) {
    struct splinter_slot *slot = &S[(idx + i) % H->slots];
    if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
      strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
        uint64_t start = atomic_load_explicit(&slot->epoch, memory_order_acquire);
        if (start & 1) {
          // writer in progress
          errno = EAGAIN;
          return -1;
        }
	    atomic_thread_fence(memory_order_acquire);
        switch (mode) {
          case SPL_TIME_CTIME:
            atomic_store_explicit(&slot->ctime, epoch - offset, memory_order_release);
            return 0;
          case SPL_TIME_ATIME:
            atomic_store_explicit(&slot->atime, epoch - offset, memory_order_release);
            return 0;
          default:
            errno = ENOTSUP;
            return -2;
        }
    }
  }
  errno = ENOENT;
  return -1;
}

/**
 * @brief Bitwise & arithmetic ops on keys named as big unsigned
 * @param key Name of the key  to operate on
 * @param op Operation you want to do
 * @param mask What you want to do it with
 * @return 0 on success, -1 / -2 on internal / caller errors respectively
 */
int splinter_integer_op(const char *key, splinter_integer_op_t op, const void *mask) {
    if (!H || !key) return -2;

    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    uint64_t m64 = 0;
    // uint64_t m64 = *(const uint64_t *)mask;

    if (mask) {
        memcpy(&m64, mask, sizeof(uint64_t));
    }

    // proactive fence for weak-memory hardware (e.g. chromebook consumer grade)
    atomic_thread_fence(memory_order_acquire);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h && 
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            
            // We expect a named type biguint
            uint8_t type = atomic_load_explicit(&slot->type_flag, memory_order_relaxed);
            if (!(type & SPL_SLOT_TYPE_BIGUINT)) {
                errno = EPROTOTYPE; 
                return -1;
            }

            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1ull) { errno = EAGAIN; return -1; }
            
            if (!atomic_compare_exchange_strong_explicit(&slot->epoch, &e, e + 1,
                                                        memory_order_acquire, 
                                                        memory_order_relaxed)) {
                errno = EAGAIN;
                return -1;
            }

            // fast-track for the 64 bit lane, smaller lengths would require 
            // different handling (possible future TODO if it's absolutely
            // ever needed)
            uint64_t *val = (uint64_t *)(VALUES + slot->val_off);
            switch (op) {
                case SPL_OP_OR:  *val |= m64;  break;
                case SPL_OP_AND: *val &= m64;  break;
                case SPL_OP_XOR: *val ^= m64;  break;
                case SPL_OP_NOT: *val = ~(*val); break;
                case SPL_OP_INC: *val += m64;  break;
                case SPL_OP_DEC: *val -= m64;  break;
            }

            // now make visible
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}

/**
 * @brief Get a direct pointer to a value in shared memory.
 * @warning Unsafe: The data at this pointer can change or be zeroed if a 
 * writer modifies the slot. Use splinter_get_epoch to verify consistency.
 * @param key The key to look up.
 * @param out_sz Pointer to receive the actual length of the value.
 * @param out_epoch Pointer to receive the epoch at the time of lookup.
 * @return A const pointer to the data in SHM, or NULL if not found.
 */
const void *splinter_get_raw_ptr(const char *key, size_t *out_sz, uint64_t *out_epoch) {
    if (!H || !key) return NULL;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            if (out_epoch) *out_epoch = e;
            if (out_sz) *out_sz = (size_t)atomic_load_explicit(&slot->val_len, memory_order_relaxed);
            
            return (const void *)(VALUES + slot->val_off);
        }
    }
    return NULL;
}

/**
 * @brief Get the current epoch of a specific slot.
 * @return The 64-bit epoch, or 0 if key not found.
 */
uint64_t splinter_get_epoch(const char *key) {
    if (!H || !key) return 0;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            return atomic_load_explicit(&slot->epoch, memory_order_acquire);
        }
    }
    return 0;
}
