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
 * You can use it like semantic "breadboard" - Have a good time!
 * https://splinterhq.github.io for docs
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
#include <limits.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <poll.h>

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
/** @brief Process-local eventfd used to signal epoch changes; -1 if not initialized. */
static int g_event_fd = -1;

/* Forward declaration — defined near splinter_pulse_watchers */
static void splinter_event_bus_notify(size_t physical_idx);

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

int splinter_create(const char *name_or_path, size_t slots, size_t max_value_sz) {
    int fd;

    if (slots <= 0 || max_value_sz <= 0) {
        errno = ENOTSUP;
        return -2;
    }

#ifdef SPLINTER_PERSISTENT
    fd = open(name_or_path, O_RDWR | O_CREAT, 0666);
#else
    fd = shm_open(name_or_path, O_RDWR | O_CREAT | O_EXCL, 0666);
#endif
    if (fd < 0) return -1;
    size_t region_sz = slots * max_value_sz;
    size_t total_sz  = sizeof(struct splinter_header) + slots * sizeof(struct splinter_slot) + region_sz;
    if (ftruncate(fd, (off_t)total_sz) != 0) return -1;
    if (map_fd(fd, total_sz) != 0) return -1;
    
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

    // Initialize event bus
    for (size_t m = 0; m < SPLINTER_EVENT_BUS_MASK_WORDS; m++)
        atomic_store_explicit(&H->event_bus.dirty_mask[m], 0, memory_order_relaxed);
    atomic_store_explicit(&H->event_bus.owner_fd,  -1, memory_order_relaxed);
    atomic_store_explicit(&H->event_bus.owner_pid,  0, memory_order_relaxed);

    size_t i;
    for (i = 0; i < slots; ++i) {
        atomic_fetch_or(&S[i].type_flag, SPL_SLOT_DEFAULT_TYPE);
        atomic_store_explicit(&S[i].hash, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].epoch, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].ctime, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].atime, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].user_flag, 0, memory_order_relaxed);
        atomic_store_explicit(&S[i].watcher_mask, 0, memory_order_relaxed);
        for (int b = 0; b < 64; b++) {
            atomic_store_explicit(&H->bloom_watches[b], 0xFF, memory_order_relaxed);
        }
        S[i].val_off = (uint32_t)(i * max_value_sz);
        atomic_store_explicit(&S[i].val_len, 0, memory_order_relaxed);
        S[i].key[0] = '\0';      
    }
    return 0;
}

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
    if (H->magic != SPLINTER_MAGIC || H->version != SPLINTER_VER) return -1;
    return 0;
}

#ifdef SPLINTER_NUMA_AFFINITY
void* splinter_open_numa(const char *name, int target_node) {
    if (numa_available() < 0) return NULL;
    int fd = shm_open(name, O_RDWR, 0666);
    struct stat st;
    fstat(fd, &st);
    void *addr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    unsigned long mask = (1UL << target_node);
    unsigned long maxnode = numa_max_node() + 1;
    if (mbind(addr, st.st_size, MPOL_BIND, &mask, maxnode, MPOL_MF_STRICT | MPOL_MF_MOVE) != 0) {
        perror("mbind failed");
    }
    return addr;
}
#endif //SPLINTER_NUMA_AFFINITY

int splinter_create_or_open(const char *name_or_path, size_t slots, size_t max_value_sz) {
    int ret = splinter_create(name_or_path, slots, max_value_sz);
    return (ret == 0 ? ret : splinter_open(name_or_path));
}

int splinter_open_or_create(const char *name_or_path, size_t slots, size_t max_value_sz) {
    int ret = splinter_open(name_or_path);
    return (ret == 0 ? ret : splinter_create(name_or_path, slots, max_value_sz));
}

int splinter_set_mop(unsigned int mode) {
    if (!H) return -2;
    switch (mode) {
        case 0:
            atomic_fetch_and(&H->core_flags, ~(SPL_SYS_AUTO_SCRUB | SPL_SYS_HYBRID_SCRUB));
            break;
        case 1:
            atomic_fetch_or(&H->core_flags, SPL_SYS_AUTO_SCRUB | SPL_SYS_HYBRID_SCRUB);
            break;
        case 2:
            splinter_config_set(H, SPL_SYS_AUTO_SCRUB);
            break;
        default:
            errno = EOPNOTSUPP;
            return -1;
    }
    return 0;
}

int splinter_get_mop(void) {
    if (!H) return -2;
    if (splinter_config_test(H, SPL_SYS_HYBRID_SCRUB)) return 1;
    if (splinter_config_test(H, SPL_SYS_AUTO_SCRUB)) return 2;
    return 0;
}

int splinter_set_av(unsigned int mode) {
    if (!H) return -2;
    if (mode == 1) {
        splinter_config_set(H, SPL_SYS_AUTO_SCRUB);
        return 0;
    } else if (mode == 0) {
        atomic_fetch_and(&H->core_flags, ~(SPL_SYS_AUTO_SCRUB | SPL_SYS_HYBRID_SCRUB));
        return 0;
    }
    errno = ENOTSUP;
    return -1;
}

int splinter_get_av(void) {
    if (!H) return -2;
    return (int) splinter_config_test(H, SPL_SYS_AUTO_SCRUB);
}

int splinter_set_hybrid_av(void) {
    if (!H) return -2;
    atomic_fetch_or(&H->core_flags, SPL_SYS_AUTO_SCRUB | SPL_SYS_HYBRID_SCRUB);
    return 0;
}

int splinter_get_hybrid_av(void) {
    if (!H) return -2;
    return (int) splinter_config_test(H, SPL_SYS_HYBRID_SCRUB);
}

void splinter_purge(void) {
    if (!H || !S || !VALUES) return;
    for (uint32_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[i];
        uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_acquire);
        if (e & 1ull) continue;
        if (!atomic_compare_exchange_strong(&slot->epoch, &e, e + 1)) continue;
        uint32_t len = atomic_load_explicit(&slot->val_len, memory_order_relaxed);
        uint8_t *dst = VALUES + slot->val_off;
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == 0) {
            memset(dst, 0, H->max_val_sz);
        } else if (len < H->max_val_sz) {
            memset(dst + len, 0, H->max_val_sz - len);
        }
        atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
    }
}

void splinter_close(void) {
    if (g_event_fd >= 0) { close(g_event_fd); g_event_fd = -1; }
    if (g_base) munmap(g_base, g_total_sz);
    g_base = NULL; H = NULL; S = NULL; VALUES = NULL; g_total_sz = 0;
}

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
            if (start_epoch & 1) { errno = EAGAIN; return -1; }
            int ret = (int)atomic_load_explicit(&slot->val_len, memory_order_acquire);
            atomic_store_explicit(&slot->hash, 0, memory_order_release);
            if (splinter_config_test(H, SPL_SYS_AUTO_SCRUB)) {
                memset(VALUES + slot->val_off, 0, H->max_val_sz);
                memset(slot->key, 0, SPLINTER_KEY_MAX);
            } else {
                slot->key[0] = '\0';
            }
            atomic_store_explicit(&slot->type_flag, 0, memory_order_release);
            atomic_fetch_or(&slot->type_flag, SPL_SLOT_DEFAULT_TYPE);
            atomic_store_explicit(&slot->epoch, 0, memory_order_release);
            atomic_store_explicit(&slot->val_len, 0, memory_order_release);
            atomic_store_explicit(&slot->ctime, 0, memory_order_release);
            atomic_store_explicit(&slot->atime, 0, memory_order_release);
            atomic_store_explicit(&slot->user_flag, 0, memory_order_release);
            atomic_store_explicit(&slot->watcher_mask, 0, memory_order_release);
            atomic_store_explicit(&slot->bloom, 0, memory_order_release);
            atomic_fetch_add_explicit(&slot->epoch, 2, memory_order_release);
            return ret;
        }
    }
    return -1;
}

int splinter_set(const char *key, const void *val, size_t len) {
    if (!H || !key) return -2;
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
                if (splinter_config_test(H, SPL_SYS_HYBRID_SCRUB)) {
                    size_t scrub_len = (len + 63) & ~63;
                    if (scrub_len > H->max_val_sz) scrub_len = H->max_val_sz;
                    memset(dst, 0, scrub_len);
                } else {
                    memset(dst, 0, H->max_val_sz);
                }
            }
            
            memcpy(dst, val, len);
            atomic_store_explicit(&slot->val_len, (uint32_t)len, memory_order_release);

            slot->key[0] = '\0';
            strncpy(slot->key, key, SPLINTER_KEY_MAX - 1);
            slot->key[SPLINTER_KEY_MAX - 1] = '\0';

            atomic_thread_fence(memory_order_release);
            atomic_store_explicit(&slot->hash, h, memory_order_release);
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            
            splinter_pulse_watchers(slot);
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
            splinter_event_bus_notify((idx + i) % H->slots);

            return 0;
        }
    }
    return -1;
}
int splinter_get(const char *key, void *buf, size_t buf_sz, size_t *out_sz) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    size_t i;
    for (i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];

        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            uint64_t start = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            if (start & 1) { errno = EAGAIN; return -1; }

            atomic_thread_fence(memory_order_acquire);

            size_t len = (size_t)atomic_load_explicit(&slot->val_len, memory_order_acquire);
            if (out_sz) *out_sz = len;

            if (buf) {
                if (buf_sz < len) { errno = EMSGSIZE; return -1; }
                memcpy(buf, VALUES + slot->val_off, len);
            }

            uint64_t end = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            if (start == end && !(end & 1)) return 0;

            errno = EAGAIN;
            return -1;
        }
    }
    return -1;
}

int splinter_list(char **out_keys, size_t max_keys, size_t *out_count) {
    if (!H || !out_keys || !out_count) return -2;
    size_t count = 0, i;
    for (i = 0; i < H->slots && count < max_keys; ++i) {
        if (atomic_load_explicit(&S[i].hash, memory_order_acquire) &&
            atomic_load_explicit(&S[i].val_len, memory_order_acquire) > 0) {
            out_keys[count++] = S[i].key;
        }
    }
    *out_count = count;
    return 0;
}

int splinter_poll(const char *key, uint64_t timeout_ms) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    struct splinter_slot *slot = NULL;
    size_t i;
    for (i = 0; i < H->slots; ++i) {
        struct splinter_slot *s = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&s->hash, memory_order_acquire) == h &&
            strncmp(s->key, key, SPLINTER_KEY_MAX) == 0) {
            slot = s;
            break;
        }
    }
    if (!slot) return -1;

    uint64_t start_epoch = atomic_load_explicit(&slot->epoch, memory_order_acquire);
    if (start_epoch & 1) { errno = EAGAIN; return -2; }

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    add_ms(&deadline, timeout_ms);

    struct timespec sleep_ts = {0, 10 * NS_PER_MS};
    while (1) {
        uint64_t cur_epoch = atomic_load_explicit(&slot->epoch, memory_order_acquire);
        if (cur_epoch & 1) { errno = EAGAIN; return -2; }
        if (cur_epoch != start_epoch) return 0;
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

int splinter_get_header_snapshot(splinter_header_snapshot_t *snapshot) {
    if (!H) return -2;
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

int splinter_get_slot_snapshot(const char *key, splinter_slot_snapshot_t *snapshot) {
    if (!H || !key || !snapshot) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots), i = 0;
    for (i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            uint64_t start = 0, end = 0;
            do {
                start = atomic_load_explicit(&slot->epoch, memory_order_acquire);
                if (start & 1) continue;
                snapshot->hash = h;
                snapshot->epoch = start;
                snapshot->val_off = slot->val_off;
                snapshot->val_len = atomic_load_explicit(&slot->val_len, memory_order_relaxed);
                snapshot->type_flag = atomic_load_explicit(&slot->type_flag, memory_order_acquire);
                snapshot->user_flag = atomic_load_explicit(&slot->user_flag, memory_order_acquire);
                snapshot->ctime = atomic_load_explicit(&slot->ctime, memory_order_acquire);
                snapshot->atime = atomic_load_explicit(&slot->atime, memory_order_acquire);
                snapshot->bloom = atomic_load_explicit(&slot->bloom, memory_order_acquire);
                strncpy(snapshot->key, slot->key, SPLINTER_KEY_MAX);
#ifdef SPLINTER_EMBEDDINGS                
                memcpy(snapshot->embedding, slot->embedding, sizeof(float) * SPLINTER_EMBED_DIM);
#endif
                atomic_thread_fence(memory_order_acquire);
                end = atomic_load_explicit(&slot->epoch, memory_order_acquire);
            } while (start != end);
            return 0;
        }
    }
    return -1;
}

#ifdef SPLINTER_EMBEDDINGS
int splinter_set_embedding(const char *key, const float *vec) {
    if (!H || !key || !vec) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1ull) return -1;
            uint64_t want = e + 1;
            if (!atomic_compare_exchange_strong(&slot->epoch, &e, want)) return -1;
            memcpy(slot->embedding, vec, sizeof(float) * SPLINTER_EMBED_DIM);
            atomic_thread_fence(memory_order_release);
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
            splinter_event_bus_notify((idx + i) % H->slots);
            return 0;
        }
    }
    return -1;
}

int splinter_get_embedding(const char *key, float *embedding_out) {
    if (!H || !key || !embedding_out) return -2;
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

void splinter_config_set(struct splinter_header *hdr, uint8_t mask) {
    atomic_fetch_or(&hdr->core_flags, mask);
}
void splinter_config_clear(struct splinter_header *hdr, uint8_t mask) {
    atomic_fetch_and(&hdr->core_flags, ~mask);
}
int splinter_config_test(struct splinter_header *hdr, uint8_t mask) {
    return (atomic_load(&hdr->core_flags) & mask) != 0;
}
uint8_t splinter_config_snapshot(struct splinter_header *hdr) {
    return atomic_load(&hdr->core_flags);
}
void splinter_slot_usr_set(struct splinter_slot *slot, uint16_t mask) {
  atomic_fetch_or(&slot->user_flag, mask);
}
void splinter_slot_usr_clear(struct splinter_slot *slot, uint16_t mask) {
  atomic_fetch_and(&slot->user_flag, ~mask);
}
int splinter_slot_usr_test(struct splinter_slot *slot, uint16_t mask) {
  return (atomic_load(&slot->user_flag) & mask) != 0;
}
uint16_t splinter_slot_usr_snapshot(struct splinter_slot *slot) {
  return atomic_load(&slot->user_flag);
}

int splinter_set_named_type(const char *key, uint16_t mask) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1) { errno = EAGAIN; return -1; }
            if (!atomic_compare_exchange_strong(&slot->epoch, &e, e + 1)) {
                errno = EAGAIN; return -1;
            }
            atomic_thread_fence(memory_order_acquire);
            uint32_t current_len = atomic_load(&slot->val_len);
            if ((mask & SPL_SLOT_TYPE_BIGUINT) && current_len < 8) {
                uint32_t new_off = atomic_fetch_add(&H->val_brk, 8);
                if (new_off + 8 > H->val_sz) {
                    atomic_fetch_add(&slot->epoch, 1); 
                    errno = ENOMEM; return -1;
                }
                uint8_t *old_ptr = VALUES + slot->val_off;
                uint64_t converted_val = 0;
                if (current_len > 0 && old_ptr[0] >= '0' && old_ptr[0] <= '9') {
                    char tmp_buf[16] = {0};
                    memcpy(tmp_buf, old_ptr, (current_len < 15) ? current_len : 15);
                    converted_val = strtoull(tmp_buf, NULL, 0);
                } else {
                    memcpy(&converted_val, old_ptr, (current_len < 8) ? current_len : 8);
                }
                uint64_t *new_ptr = (uint64_t *)(VALUES + new_off);
                *new_ptr = converted_val;
                slot->val_off = new_off;
                atomic_store_explicit(&slot->val_len, 8, memory_order_relaxed);
            }
            atomic_store_explicit(&slot->type_flag, mask, memory_order_release);
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            atomic_fetch_add(&H->epoch, 1);
            splinter_event_bus_notify((idx + i) % H->slots);
            return 0;
        }
    }
    return -1;
}

int splinter_set_slot_time(const char *key, unsigned short mode, uint64_t epoch, size_t offset) {
  if (!H || !key) return -2;
  uint64_t h = fnv1a(key);
  size_t idx = slot_idx(h, H->slots), i;
  for (i = 0; i < H->slots; ++i) {
    struct splinter_slot *slot = &S[(idx + i) % H->slots];
    if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
      strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
        uint64_t start = atomic_load_explicit(&slot->epoch, memory_order_acquire);
        if (start & 1) { errno = EAGAIN; return -1; }
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
  return -1;
}

int splinter_integer_op(const char *key, splinter_integer_op_t op, const void *mask) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    uint64_t m64 = 0;
    if (mask) memcpy(&m64, mask, sizeof(uint64_t));
    atomic_thread_fence(memory_order_acquire);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h && 
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            uint8_t type = atomic_load_explicit(&slot->type_flag, memory_order_relaxed);
            if (!(type & SPL_SLOT_TYPE_BIGUINT)) { errno = EPROTOTYPE; return -1; }
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1ull) { errno = EAGAIN; return -1; }
            if (!atomic_compare_exchange_strong_explicit(&slot->epoch, &e, e + 1,
                                                        memory_order_acquire, 
                                                        memory_order_relaxed)) {
                errno = EAGAIN; return -1;
            }
            uint64_t *val = (uint64_t *)(VALUES + slot->val_off);
            switch (op) {
                case SPL_OP_OR:  *val |= m64;  break;
                case SPL_OP_AND: *val &= m64;  break;
                case SPL_OP_XOR: *val ^= m64;  break;
                case SPL_OP_NOT: *val = ~(*val); break;
                case SPL_OP_INC: *val += m64;  break;
                case SPL_OP_DEC: *val -= m64;  break;
            }
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
            splinter_event_bus_notify((idx + i) % H->slots);
            return 0;
        }
    }
    return -1;
}

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

int splinter_bump_slot(const char *key) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            if (e & 1ull) return -1; 
            uint64_t want = e + 1;
            if (!atomic_compare_exchange_strong(&slot->epoch, &e, want)) return -1;
            atomic_thread_fence(memory_order_release);
            splinter_pulse_watchers(slot);
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            return 0;
        }
    }
    return -1;
}

int splinter_set_label(const char *key, uint64_t mask) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            atomic_fetch_or_explicit(&slot->bloom, mask, memory_order_release);
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
            splinter_event_bus_notify((idx + i) % H->slots);
            return 0;
        }
    }
    return -1;
}

int splinter_unset_label(const char *key, uint64_t mask) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            atomic_fetch_and_explicit(&slot->bloom, ~mask, memory_order_release);
            atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
            splinter_event_bus_notify((idx + i) % H->slots);
            return 0;
        }
    }
    return -1;
}

int splinter_client_set_tandem(const char *base_key, const void **vals, 
                               const size_t *lens, uint8_t orders) {
    char tandem_name[SPLINTER_KEY_MAX];
    if (splinter_set(base_key, vals[0], lens[0]) != 0) return -1;
    for (uint8_t i = 1; i < orders; i++) {
        snprintf(tandem_name, sizeof(tandem_name), "%s%s%u", base_key, SPL_ORDER_ACCESSOR, i);
        if (splinter_set(tandem_name, vals[i], lens[i]) != 0) return -1;
    }
    return 0;
}

void splinter_client_unset_tandem(const char *base_key, uint8_t orders) {
    char tandem_name[SPLINTER_KEY_MAX];
    splinter_unset(base_key);
    for (uint8_t i = 1; i < orders; i++) {
        snprintf(tandem_name, sizeof(tandem_name), "%s%s%u", base_key, SPL_ORDER_ACCESSOR, i);
        splinter_unset(tandem_name);
    }
}

int splinter_watch_register(const char *key, uint8_t group_id) {
    if (!H || !key) return -2;
    if (group_id >= SPLINTER_MAX_GROUPS) { errno = EINVAL; return -2; }
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            atomic_fetch_or_explicit(&slot->watcher_mask, (1ULL << group_id), memory_order_release);
            return 0;
        }
    }
    return -1;
}

int splinter_watch_label_register(uint64_t bloom_mask, uint8_t group_id) {
    if (!H || group_id >= SPLINTER_MAX_GROUPS) return -2;
    for (int i = 0; i < 64; i++) {
        if (bloom_mask & (1ULL << i))
            atomic_store_explicit(&H->bloom_watches[i], group_id, memory_order_release);
    }
    return 0;
}

int splinter_pulse_keygroup(const char *key) {
    if (!H || !key) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);  
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            splinter_pulse_watchers(slot);
            return 0;
        }
    }
    return -1;
}

static void splinter_event_bus_notify(size_t physical_idx) {
    if (g_event_fd < 0 || !H) return;
    size_t mapped = physical_idx % (SPLINTER_EVENT_BUS_MASK_WORDS * 64);
    atomic_fetch_or_explicit(
        &H->event_bus.dirty_mask[mapped / 64],
        (1ULL << (mapped % 64)),
        memory_order_release);
    uint64_t u = 1;
    int wr = (int)write(g_event_fd, &u, sizeof(u));
    (void)wr;
}

void splinter_pulse_watchers(struct splinter_slot *slot) {
    uint64_t mask = atomic_load_explicit(&slot->watcher_mask, memory_order_acquire);
    for (int i = 0; i < SPLINTER_MAX_GROUPS; i++) {
        if (mask & (1ULL << i))
            atomic_fetch_add_explicit(&H->signal_groups[i].counter, 1, memory_order_release);
    }
    uint64_t bloom = slot->bloom; 
    for (int b = 0; b < 64; b++) {
        if (bloom & (1ULL << b)) {
            uint8_t g = atomic_load_explicit(&H->bloom_watches[b], memory_order_acquire);
            if (g < SPLINTER_MAX_GROUPS)
                atomic_fetch_add_explicit(&H->signal_groups[g].counter, 1, memory_order_release);
        }
    }
}

int splinter_watch_unregister(const char *key, uint8_t group_id) {
    if (!H || !key || group_id >= SPLINTER_MAX_GROUPS) return -2;
    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
            atomic_fetch_and_explicit(&slot->watcher_mask, ~(1ULL << group_id), memory_order_release);
            return 0;
        }
    }
    return -1;
}

uint64_t splinter_get_signal_count(uint8_t group_id) {
    if (!H || group_id >= SPLINTER_MAX_GROUPS) return 0;
    return atomic_load_explicit(&H->signal_groups[group_id].counter, memory_order_acquire);
}

void splinter_enumerate_matches(uint64_t mask, 
    void (*callback)(const char *key, uint64_t epoch, void *data), void *user_data) 
{
    if (!H || !S) return;
    for (size_t i = 0; i < H->slots; i++) {
        struct splinter_slot *slot = &S[i];
        uint64_t h = atomic_load_explicit(&slot->hash, memory_order_acquire);
        if (h != 0 && (atomic_load_explicit(&slot->bloom, memory_order_acquire) & mask) == mask) {
            uint64_t ep = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
            callback(slot->key, ep, user_data);
        }
    }
}

/* -------------------------------------------------------------------------
 * Event bus API
 * -------------------------------------------------------------------------*/

int splinter_event_bus_init(void) {
    if (!H) return -1;
    int fd = eventfd(0, EFD_CLOEXEC);
    if (fd < 0) return -1;
    atomic_store_explicit(&H->event_bus.owner_fd,  (int32_t)fd,        memory_order_release);
    atomic_store_explicit(&H->event_bus.owner_pid, (int32_t)getpid(), memory_order_release);
    g_event_fd = fd;
    return 0;
}

int splinter_event_bus_open(void) {
    if (!H) return -1;
    int32_t stored_fd  = atomic_load_explicit(&H->event_bus.owner_fd,  memory_order_acquire);
    int32_t stored_pid = atomic_load_explicit(&H->event_bus.owner_pid, memory_order_acquire);
    if (stored_fd < 0 || stored_pid <= 0) { errno = ENODEV; return -1; }

    /* Same process: dup() is the trivial path */
    if ((pid_t)stored_pid == getpid())
        return dup((int)stored_fd);

    /* Cross-process: use pidfd_getfd (Linux >= 5.6). */
#if defined(SYS_pidfd_open) && defined(SYS_pidfd_getfd)
    int pidfd = (int)syscall(SYS_pidfd_open, (pid_t)stored_pid, 0);
    if (pidfd < 0) return -1;
    int fd = (int)syscall(SYS_pidfd_getfd, pidfd, (int)stored_fd, 0);
    close(pidfd);
    return fd;
#else
    errno = ENOSYS;
    return -1;
#endif
}

int splinter_event_bus_wait(int fd, uint64_t timeout_ms) {
    if (fd < 0) return -1;
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int t = (timeout_ms == UINT64_MAX) ? -1 :
            (timeout_ms > (uint64_t)INT_MAX) ? INT_MAX : (int)timeout_ms;
    if (poll(&pfd, 1, t) <= 0) return -1;
    uint64_t val;
    return (read(fd, &val, sizeof(val)) == (ssize_t)sizeof(val)) ? 0 : -1;
}

void splinter_event_bus_close(int fd) {
    if (fd >= 0) close(fd);
}

void splinter_event_bus_get_dirty(uint64_t *out, size_t words) {
    if (!H || !out) return;
    size_t n = (words < SPLINTER_EVENT_BUS_MASK_WORDS) ? words : SPLINTER_EVENT_BUS_MASK_WORDS;
    for (size_t i = 0; i < n; i++)
        out[i] = atomic_load_explicit(&H->event_bus.dirty_mask[i], memory_order_acquire);
}

/* -------------------------------------------------------------------------*/

int splinter_set_as_system(const char *key) {
    if (!H || !S) return -2;
    size_t idx = 0;
    uint64_t h = fnv1a(key);
    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];
        if (atomic_load_explicit(&slot->hash, memory_order_acquire) == h &&
            strncmp(slot->key, key, SPLINTER_KEY_MAX) == 0) {
                atomic_store_explicit(&slot->type_flag, SPL_SLOT_TYPE_BINARY, memory_order_release);
                uint32_t system_sz = H->max_val_sz;
                atomic_store_explicit(&slot->val_len, system_sz, memory_order_release);
                return 0;
        }
    }
    return -1;
}

int splinter_append(const char *key, const void *data, size_t data_len, size_t *new_len) {
    if (!H || !key || !data) return -2;
    if (data_len == 0) return -2;

    uint64_t h = fnv1a(key);
    size_t idx = slot_idx(h, H->slots);

    for (size_t i = 0; i < H->slots; ++i) {
        struct splinter_slot *slot = &S[(idx + i) % H->slots];

        if (atomic_load_explicit(&slot->hash, memory_order_acquire) != h) continue;
        if (strncmp(slot->key, key, SPLINTER_KEY_MAX) != 0) continue;

        uint64_t e = atomic_load_explicit(&slot->epoch, memory_order_relaxed);
        if (e & 1ull) { errno = EAGAIN; return -1; }

        if (!atomic_compare_exchange_weak_explicit(&slot->epoch, &e, e + 1,
                                                   memory_order_acq_rel,
                                                   memory_order_relaxed)) {
            errno = EAGAIN;
            return -1;
        }

        size_t cur_len = (size_t)atomic_load_explicit(&slot->val_len, memory_order_relaxed);
        if (cur_len + data_len > (size_t)H->max_val_sz) {
            atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
            errno = EMSGSIZE;
            return -1;
        }

        uint8_t *dst = VALUES + slot->val_off + cur_len;
        memcpy(dst, data, data_len);

        size_t total = cur_len + data_len;
        atomic_store_explicit(&slot->val_len, (uint32_t)total, memory_order_release);

        if (new_len) *new_len = total;

        atomic_fetch_add_explicit(&slot->epoch, 1, memory_order_release);
        splinter_pulse_watchers(slot);
        atomic_fetch_add_explicit(&H->epoch, 1, memory_order_relaxed);
        splinter_event_bus_notify((idx + i) % H->slots);

        return 0;
    }
    return -1;
}
