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
#include "config.h"
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
#include <stdlib.h>
#ifdef SPLINTER_NUMA_AFFINITY
#include <numa.h>
#include <numaif.h>
#endif // SPLINTER_NUMA_AFFINITY

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
        atomic_store_explicit(&S[i].watcher_mask, 0, memory_order_relaxed);
        // we don't want brand new slots getting pulsed due to garbage in the bloom
        // auto_scrub doesn't fully solve for this alone (and is optional,) so we
        // do it here at the cost of a small loop.
        for (int b = 0; b < 64; b++) {
            atomic_store_explicit(&H->bloom_watches[b], 0xFF, memory_order_relaxed);
        }
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

#ifdef SPLINTER_NUMA_AFFINITY
/**
 * @brief Opens the Splinter bus and binds it to a specific NUMA node.
 * This ensures all memory pages for the VALUES arena and slots 
 * stay local to the target socket's memory controller.
 */
void* splinter_open_numa(const char *name, int target_node) {
    if (numa_available() < 0) return NULL; // No NUMA support

    // Open as usual
    int fd = shm_open(name, O_RDWR, 0666);
    struct stat st;
    fstat(fd, &st);
    void *addr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    // Prepare the nodemask for mbind
    unsigned long mask = (1UL << target_node);
    unsigned long maxnode = numa_max_node() + 1;

    // Bind the memory region to the specific physical node
    // MPOL_BIND: Forces allocation strictly on these nodes
    // MPOL_MF_STRICT: Fail if pages are already elsewhere
    if (mbind(addr, st.st_size, MPOL_BIND, &mask, maxnode, MPOL_MF_STRICT | MPOL_MF_MOVE) != 0) {
        perror("mbind failed");
    }

    return addr;
}
#endif //SPLINTER_NUMA_AFFINITY
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

    if (mode == 1) {
        splinter_config_set(H, SPL_SYS_AUTO_SCRUB);
        return 0;
    } else if (mode == 0) {
        // Clear both flags simultaneously 
        atomic_fetch_and(&H->core_flags, ~(SPL_SYS_AUTO_SCRUB | SPL_SYS_HYBRID_SCRUB));
        return 0;
    }

    // The only reason this fails is mode being unexpected.
    errno = ENOTSUP;
    return -1;
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
 * @brief Just like splinter_set_av(), but also engages the hybrid bit atomically / simultaneously
 * @return int
 */
int splinter_set_hybrid_av(void) {
    if (!H) return -2;

    // Set both bits simultaneously. This opens the gate AND 
    // selects the 64-byte mop in one atomic cycle.
    atomic_fetch_or(&H->core_flags, SPL_SYS_AUTO_SCRUB | SPL_SYS_HYBRID_SCRUB); //
    
    return 0;
}

/**
 * @brief Check if the bus has hybrid scrub enabled
 * @return int
 */
int splinter_get_hybrid_av(void) {
    if (!H) return -2;

    return (int) splinter_config_test(H, SPL_SYS_HYBRID_SCRUB);
}

/**
 * @brief Performs a high-efficiency hygiene sweep.
 */
void splinter_purge(void) {
    if (!H || !S || !VALUES) return;

    for (uint32_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[i];
        
        // 1. Snapshot the state to avoid 'squatting' on a busy slot
        uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_acquire);
        if (e & 1ull) continue; // Strike was already in progress

        // 2. Acquire the seqlock
        if (!atomic_compare_exchange_strong(&slot->epoch, &e, e + 1)) continue;

        uint32_t len = atomic_load_explicit(&slot->val_len, memory_order_relaxed);
        uint8_t *dst = VALUES + slot->val_off;

        // 3. The Sweep: If active, mop the tail; if empty, boil the slot.
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == 0) {
            memset(dst, 0, H->max_val_sz);
        } else if (len < H->max_val_sz) {
            // Only mop the 'dirty' remainder beyond current data
            memset(dst + len, 0, H->max_val_sz - len);
        }

        // 4. Release and return to silence
        atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
    }
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
            
            atomic_store_explicit(&slot->epoch, 0, memory_order_release);
            atomic_store_explicit(&slot->val_len, 0, memory_order_release);
            atomic_store_explicit(&slot->ctime, 0, memory_order_release);
            atomic_store_explicit(&slot->atime, 0, memory_order_release);
            atomic_store_explicit(&slot->user_flag, 0, memory_order_release);
            atomic_store_explicit(&slot->watcher_mask, 0, memory_order_release);
            atomic_store_explicit(&slot->bloom, 0, memory_order_release);
            
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
    if (len == 0 || len > H->max_val_sz) return -1;

    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    const size_t arena_sz = (size_t)H->slots * (size_t)H->max_val_sz;

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        uint64_t slot_hash = atomic_load_explicit(&slot->hash, memory_order_acquire);

        if (slot_hash == 0 || (slot_hash == h && strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0)) {
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1ull) continue;

            if (!atomic_compare_exchange_weak_explicit(&slot->epoch, &e, e + 1,
                                                      memory_order_acq_rel, memory_order_relaxed)) {
                continue;
            }

            if ((size_t)slot->val_off >= arena_sz || (size_t)slot->val_off + len > arena_sz) {
                atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
                return -1;
            }

            uint8_t *dst = (uint8_t *)VALUES + slot->val_off;

            if (splinter_config_test(H, SPL_SYS_AUTO_SCRUB)) {
                // Determine if we do a full scrub or a fast cache-line scrub
                if (splinter_config_test(H, SPL_SYS_HYBRID_SCRUB)) {
                    // Round up to next 64-byte boundary: (len + 63) & ~63
                    size_t scrub_len = (len + 63) & ~63;
                    if (scrub_len > H->max_val_sz) scrub_len = H->max_val_sz;
                    memset(dst, 0, scrub_len);
                } else {
                    // Full boil mode: I wish hotels could do this!
                    memset(dst, 0, H->max_val_sz);
                }
            }
            
            memcpy(dst, val, len);
            atomic_store_explicit(&slot->val_len, (uint32_t)len, memory_order_release);

            // Update key and publish
            slot->key[0] = '\0';
            strncpy(slot->key, key, SPLINTER_KEY_MAX - 1);
            slot->key[SPLINTER_KEY_MAX - 1] = '\0';

            atomic_thread_fence(memory_order_release);
            atomic_store_explicit(&slot->hash, h, memory_order_release);
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            
            splinter_pulse_watchers(slot);
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);

            return 0;
        }
    }
    return -1;
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
                uint32_t new_off = atomic_fetch_add(&H->val_brk, 8);
                if (new_off + 8 > H->val_sz) {
                    atomic_fetch_add(&slot->epoch, 1); 
                    errno = ENOMEM; return -1;
                }

                uint8_t *old_ptr = VALUES + slot->val_off;
                uint64_t converted_val = 0;

                // Check if it's a numeric string that needs parsing
                if (current_len > 0 && old_ptr[0] >= '0' && old_ptr[0] <= '9') {
                    char tmp_buf[16] = {0};
                    memcpy(tmp_buf, old_ptr, (current_len < 15) ? current_len : 15);
                    converted_val = strtoull(tmp_buf, NULL, 0);
                } else {
                    // Fallback: just move the raw bytes
                    memcpy(&converted_val, old_ptr, (current_len < 8) ? current_len : 8);
                }

                uint64_t *new_ptr = (uint64_t *)(VALUES + new_off);
                *new_ptr = converted_val; // Now it's a real 64-bit integer

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

/**
 * @brief Atomically apply a label mask to a slot's Bloom filter.
 * @return 0 on success, -1 if key not found.
 */
int splinter_set_label(const char *key, uint64_t mask) {
    if (!H || !key) return -1;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            
            // Atomic OR ensures we don't wipe existing labels
            atomic_fetch_or_explicit(&slot->bloom, mask, memory_order_release);
            
            // Optional: Bump global epoch to alert watchers of metadata change
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}

/**
 * @brief Client-side helper to write multiple orders of a key.
 * * Since we've backed out library-side linking, this helper manages
 * the naming convention for the caller.
 */
int splinter_client_set_tandem(const char *base_key, const void **vals, 
                               const size_t *lens, uint8_t orders) {
    char tandem_name[SPLINTER_KEY_MAX];
    
    // Write Order 0 (The base key)
    if (splinter_set(base_key, vals[0], lens[0]) != 0) return -1;

    // Write subsequent orders using ".n" notation
    for (uint8_t i = 1; i < orders; i++) {
        snprintf(tandem_name, sizeof(tandem_name), "%s%s%u", base_key, SPL_ORDER_ACCESSOR, i);
        if (splinter_set(tandem_name, vals[i], lens[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Client-side helper to delete a key and its known orders.
 */
void splinter_client_unset_tandem(const char *base_key, uint8_t orders) {
    char tandem_name[SPLINTER_KEY_MAX];
    
    // Unset the base
    splinter_unset(base_key);

    // Unset the orders
    for (uint8_t i = 1; i < orders; i++) {
        snprintf(tandem_name, sizeof(tandem_name), "%s%s%u", base_key, SPL_ORDER_ACCESSOR, i);
        splinter_unset(tandem_name);
    }
}

// HERE BE DRAGONS!
// splinter_poll() is okay for devOps workflows and smarter shell scripts, but
// it's too pedestrian for orchestrating real signal processing. We may need
// to watch the whole vector space of a Rank 2 tensor 'simultaneously', so 
// we need to set up signal groups that can coordinate with client-backed 
// eventfd / epoll() assistance from the kernel. This is the only place 
// where Splinter deliberately talks to Linux, and it's only to ask for 
// wake-up service, not arbitration or sockets :)
//
// To pull this off, we have to be able to pulse FD references based on bitmask
// subscription (and unsubscription) within the time that we can 'stand' on
// the seqlock with a syscall. If we stand on it *too* long, other writers will
// spin in EAGAIN loops unless they have exponential backoff logic, and readers
// are way more likely to see torn reads even with deliberate and defensive 
// atomic fencing.
//
// It is 99.9% bitmask traversal and .1% write() (as a process). If you try
// to cram any more into it than what's here, expect subtle problems. 

/**
 * @brief Register the current process's interest in a key's group signal.
 * @param key The key to watch.
 * @param group_id The signal group index (0-63).
 * @return 0 on success, -1 if key not found, -2 if invalid group.
 */
int splinter_watch_register(const char *key, uint8_t group_id) {
    if (!H || !key) return -1;
    if (group_id >= SPLINTER_MAX_GROUPS) {
        errno = EINVAL;
        return -2;
    }

    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        
        // Find the target slot
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            
            // Atomically set the bit for the desired group
            // This ensures we don't wipe out other watchers in different groups.
            atomic_fetch_or_explicit(&slot->watcher_mask, (1ULL << group_id), memory_order_release);
            
            return 0;
        }
    }

    return -1; // Key not found
}

/**
 * @brief Maps a Bloom label (bitmask) to a signal group.
 * @param bloom_mask A 64-bit mask where each set bit represents a label.
 * @param group_id The signal group (0-63) to pulse when this label matches.
 * @return 0 on success, -1 on invalid group.
 */
int splinter_watch_label_register(uint64_t bloom_mask, uint8_t group_id) {
    if (!H || group_id >= SPLINTER_MAX_GROUPS) return -1;

    // Iterate through all 64 possible bloom bits
    for (int i = 0; i < 64; i++) {
        // If the bit is set in the provided mask, map it to the group_id
        if (bloom_mask & (1ULL << i)) {
            atomic_store_explicit(&H->bloom_watches[i], group_id, memory_order_release);
        }
    }

    return 0;
}

/**
 * @brief pulse the watchers of a slot 
 */
void splinter_pulse_watchers(struct splinter_slot *slot) {
    // Pulse based on specific key watches (Direct bitmask)
    uint64_t mask = atomic_load_explicit(&slot->watcher_mask, memory_order_acquire);
    for (int i = 0; i < SPLINTER_MAX_GROUPS; i++) {
        if (mask & (1ULL << i)) {
            atomic_fetch_add_explicit(&H->signal_groups[i].counter, 1, memory_order_release);
        }
    }

    // Pulse based on Bloom Label matches
    // We assume the slot stores the bloom filter calculated at set-time
    uint64_t bloom = slot->bloom; 
    for (int b = 0; b < 64; b++) {
        if (bloom & (1ULL << b)) {
            uint8_t g = atomic_load_explicit(&H->bloom_watches[b], memory_order_acquire);
            // 0xFF (255) typically represents "no watch" for this bit
            if (g < SPLINTER_MAX_GROUPS) {
                atomic_fetch_add_explicit(&H->signal_groups[g].counter, 1, memory_order_release);
            }
        }
    }
}

/**
 * @brief Unregister interest in a key's group signal.
 * @param key The key to stop watching.
 * @param group_id The signal group index (0-63).
 * @return 0 on success, -1 if key not found or invalid group.
 */
int splinter_watch_unregister(const char *key, uint8_t group_id) {
    if (!H || !key || group_id >= SPLINTER_MAX_GROUPS) return -1;

    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        
        // Find the slot matching the key
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            
            // Atomically clear ONLY the bit for this specific group_id
            // We use bitwise AND with the bitwise NOT of our group bit
            atomic_fetch_and_explicit(&slot->watcher_mask, ~(1ULL << group_id), memory_order_release);
            
            return 0;
        }
    }

    return -1; // Key not found
}

/**
 * @brief Safely retrieve the current pulse count for a signal group. Good for debugging.
 * @param group_id The signal group (0-63).
 * @return The 64-bit pulse count, or 0 if invalid.
 */
uint64_t splinter_get_signal_count(uint8_t group_id) {
    if (!H || group_id >= SPLINTER_MAX_GROUPS) return 0;
    
    // Explicitly load with acquire semantics to ensure we see the latest pulses
    return atomic_load_explicit(&H->signal_groups[group_id].counter, memory_order_acquire);
}
