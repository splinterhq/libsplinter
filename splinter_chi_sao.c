/**
 * MRMW stress test using a disjointed-lane strategy.
 * Each writer owns an exclusive, non-overlapping slice of the keyspace so
 * write-write contention is zero by construction.  Readers sample the full
 * keyspace randomly, same as splinter_stress.c.
 *
 * "Chi Sao" (sticky hands) — simultaneous multi-limb engagement.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "splinter.h"
#include "config.h"

#ifdef HAVE_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

#define MAX_WRITERS 32

typedef struct {
    const char *store_name;
    int slots;
    int max_value_size;
    int num_threads;
    int num_writers;
    int test_duration_ms;
    int num_keys;
    int writer_period_us;
    int scrub;
} cfg_t;

typedef struct {
    atomic_int total_gets;
    atomic_int total_sets;
    atomic_int get_ok;
    atomic_int set_ok;
    atomic_int get_fail;
    atomic_int set_fail;
    atomic_int integrity_fail;
    atomic_int retries;
    atomic_int get_miss;
    atomic_int get_oversize;
    atomic_int set_full;
    atomic_int set_too_big;
} counters_t;

typedef struct {
    cfg_t *cfg;
    counters_t *ctr;
    volatile int *running;
    char **keys;
    int num_keys;
} shared_t;

/* Per-writer context carrying lane bounds within the full keyspace. */
typedef struct {
    shared_t *sh;
    int lane_start;
    int lane_len;
    int writer_id;
} writer_ctx_t;

static inline long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec*1000LL + ts.tv_nsec/1000000LL);
}

static void *writer_main(void *arg) {
    int i;
    writer_ctx_t *ctx = (writer_ctx_t *)arg;
    shared_t *sh = ctx->sh;
    cfg_t *cfg = sh->cfg;
    char *buf = malloc(cfg->max_value_size);
    if (!buf) { perror("malloc"); return NULL; }
    unsigned ver = 1;
    size_t payload_len = cfg->max_value_size / 2;
    if (payload_len < 64) payload_len = 64;

    int lane_end = ctx->lane_start + ctx->lane_len;

    do {
        for (i = ctx->lane_start; i < lane_end && *sh->running; i++) {
            unsigned long nonce = (unsigned long)now_ms();

            int n = snprintf(
                buf,
                cfg->max_value_size,
                "ver:%u|nonce:%lu|w:%d|data:", ver, nonce, ctx->writer_id);

            if (n <= 0 || n >= cfg->max_value_size) {
                atomic_fetch_add(&sh->ctr->set_too_big, 1);
                atomic_fetch_add(&sh->ctr->set_fail, 1);
                continue;
            }

            size_t remain = cfg->max_value_size - (size_t)n - 1;
            size_t fill = payload_len < remain ? payload_len : remain;
            memset(buf + n, 'A' + (ver % 26), fill);
            buf[n + fill] = '\0';
            size_t len = n + fill;

            int rc = splinter_set(sh->keys[i], buf, len);
            atomic_fetch_add(&sh->ctr->total_sets, 1);
            if (rc == 0) {
                atomic_fetch_add(&sh->ctr->set_ok, 1);
            } else {
                atomic_fetch_add(&sh->ctr->set_fail, 1);
                if (len > (size_t)cfg->max_value_size)
                    atomic_fetch_add(&sh->ctr->set_too_big, 1);
                else
                    atomic_fetch_add(&sh->ctr->set_full, 1);
            }
            if (cfg->writer_period_us > 0) usleep(cfg->writer_period_us);
        }
        ver++;
    } while (*sh->running);

    free(buf);
    return NULL;
}

static bool parse_ver(const char *val, size_t len, unsigned *out_ver) {
    char tmp[16] = {0};
    size_t i = 4, j = 0;

    if (memcmp(val, "ver:", 4) != 0)
        return false;

    while (i < len && j < sizeof(tmp)-1 && val[i] >= '0' && val[i] <= '9') {
        tmp[j++] = val[i++];
    }

    if (i >= len || val[i] != '|') return false;

    *out_ver = (unsigned)strtoul(tmp, NULL, 10);

    return true;
}

static void *reader_main(void *arg) {
    int t;
    shared_t *sh = (shared_t*)arg;
    cfg_t *cfg = sh->cfg;

    char *buf = malloc((size_t)cfg->max_value_size + 1);
    if (!buf) {
        perror("malloc");
        return NULL;
    }

    unsigned *observed = calloc((size_t)cfg->num_keys, sizeof(unsigned));
    if (!observed) {
        perror("calloc");
        free(buf);
        return NULL;
    }

    size_t got_size = 0;
    do {
        for (t = 0; t < 256 && *sh->running; t++) {
            int idx = rand() % sh->num_keys;
            for (;;) {
                if (!*sh->running) break;

                int rc = splinter_get(sh->keys[idx], buf,
                                      (size_t)cfg->max_value_size, &got_size);
                atomic_fetch_add(&sh->ctr->total_gets, 1);

                if (rc == 0) {
                    atomic_fetch_add(&sh->ctr->get_ok, 1);
                    unsigned ver = 0;
                    if (!parse_ver(buf, got_size, &ver)) {
                        atomic_fetch_add(&sh->ctr->integrity_fail, 1);
                    }
                    break;
                } else if (errno == EAGAIN) {
                    atomic_fetch_add(&sh->ctr->retries, 1);
                    continue;
                } else {
                    atomic_fetch_add(&sh->ctr->get_fail, 1);
                    if (errno == ENOENT)
                        atomic_fetch_add(&sh->ctr->get_miss, 1);
                    else if (errno == EMSGSIZE)
                        atomic_fetch_add(&sh->ctr->get_oversize, 1);
                    break;
                }
            }
        }
    } while (*sh->running);

    free(observed);
    free(buf);
    return NULL;
}

static void print_stats(cfg_t *cfg, counters_t *c, long ms) {
    int gets = atomic_load(&c->total_gets);
    int sets = atomic_load(&c->total_sets);
    int okg  = atomic_load(&c->get_ok);
    int oks  = atomic_load(&c->set_ok);
    int fget = atomic_load(&c->get_fail);
    int fset = atomic_load(&c->set_fail);
    int bad  = atomic_load(&c->integrity_fail);
    int retries = atomic_load(&c->retries);

    int gmiss    = atomic_load(&c->get_miss);
    int goversize = atomic_load(&c->get_oversize);
    int sfull    = atomic_load(&c->set_full);
    int stbig    = atomic_load(&c->set_too_big);

    int num_readers = cfg->num_threads - cfg->num_writers;
    double sec = ms / 1000.0;
    double ops = (gets + sets) / sec;

#if HAVE_VALGRIND_H
    if (RUNNING_ON_VALGRIND) {
        printf("\n\n===== VALGRIND DETECTED! =====\n");
        printf("We test on Valgrind too! All official releases are violation-and-warning-free.\n");
        printf("This test run is for violations only; profiling voids the validity of the rest.\n");
        printf("\n");
    }
#endif
    puts("===== MRMW CHI SAO STRESS RESULTS =====");
    printf("Threads            : %d (readers=%d, writers=%d)\n",
           cfg->num_threads, num_readers, cfg->num_writers);
    printf("Duration           : %d ms\n", cfg->test_duration_ms);
    printf("Hot keys           : %d\n", cfg->num_keys);
    printf("Writer Backoff     : %d us\n", cfg->writer_period_us);
    printf("Total ops          : %d (gets=%d, sets=%d)\n", gets + sets, gets, sets);
    printf("Throughput         : %.0f ops/sec\n", ops);
    printf("Hybrid Scrub       : %s\n", (cfg->scrub == 1) ? "Yes" : "No");
    printf("Get                : ok=%d fail=%d (miss=%d, oversize=%d)\n", okg, fget, gmiss, goversize);
    printf("Set                : ok=%d fail=%d (full=%d, too_big=%d)\n", oks, fset, sfull, stbig);
    printf("Integrity failures : %d\n", bad);
    printf("Retries (EAGAIN)   : %d (%.2f%% of gets, %.2f per successful get)\n\n",
           retries,
           gets ? (100.0 * retries / gets) : 0.0,
           okg ? ((double)retries / okg) : 0.0);
    if (bad) exit(bad);
}

static void prepopulate(shared_t *sh) {
    char v[128];
    int i;
    for (i = 0; i < sh->num_keys; i++) {
        int n = snprintf(v, sizeof(v), "ver:%u|nonce:%lu|w:0|data:SEED",
                         1u, (unsigned long)now_ms());
        if (n < 0) n = 0;
        (void)splinter_set(sh->keys[i], v, (size_t)n);
    }
}

static void usage(const char *prog) {
    fprintf(stderr,
        "\nUsage: %s [arguments]\nWhere arguments are:\n"
        "\t  [--threads N] [--writers W] [--duration-ms D] [--keys K]\n"
        "\t  [--slots S] [--max-value B] [--writer-us U]\n"
        "\t  [--quiet] [--keep-test-store] [--scrub] [--store NAME]\n"
        "\n"
        "\t  --writers W  : number of concurrent writers (1..%d, default 1)\n"
        "\t  --threads N  : total threads; readers = N - writers (default: see below)\n"
        "\n",
        prog, MAX_WRITERS);
}

int main(int argc, char **argv) {
    pid_t pid;
    unsigned int keep_store = 0;
    char store[64] = { 0 }, store_path[128] = { 0 };

    pid = getpid();
    if (!pid) return 1;

    snprintf(store, sizeof(store) - 1, "mrmw_test_%u", pid);
    int i, quiet = 0;
    unsigned int scrub = 0;

#ifndef SPLINTER_PERSISTENT
    cfg_t cfg = {
        .store_name = store,
        .slots = 50000,
        .max_value_size = 4096,
        .num_threads = 32,         /* default: 1 writer + 31 readers */
        .num_writers = 1,
        .test_duration_ms = 60000,
        .num_keys = 20000,
        .writer_period_us = 0,
        .scrub = 0,
    };
#else
    cfg_t cfg = {
        .store_name = store,
        .slots = 25000,
        .max_value_size = 2048,
        .num_threads = 16,         /* default: 1 writer + 15 readers */
        .num_writers = 1,
        .test_duration_ms = 30000,
        .num_keys = 10000,
        .writer_period_us = 0,
        .scrub = 0,
    };
#endif

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--threads") && i+1 < argc)       cfg.num_threads = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--writers") && i+1 < argc)  cfg.num_writers = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--duration-ms") && i+1 < argc) cfg.test_duration_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keys") && i+1 < argc)     cfg.num_keys = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--store") && i+1 < argc)    cfg.store_name = argv[++i];
        else if (!strcmp(argv[i], "--slots") && i+1 < argc)    cfg.slots = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--max-value") && i+1 < argc) cfg.max_value_size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--writer-us") && i+1 < argc) cfg.writer_period_us = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--quiet"))                  quiet = 1;
        else if (!strcmp(argv[i], "--keep-test-store"))        keep_store = 1;
        else if (!strcmp(argv[i], "--scrub"))                  scrub = 1;
        else { usage(argv[0]); return 2; }
    }

    cfg.scrub = scrub;

    /* Clamp writers to valid range. */
    if (cfg.num_writers < 1)         cfg.num_writers = 1;
    if (cfg.num_writers > MAX_WRITERS) cfg.num_writers = MAX_WRITERS;

    /* Need at least one reader in addition to the writers. */
    if (cfg.num_threads < cfg.num_writers + 1)
        cfg.num_threads = cfg.num_writers + 1;

    /* Need enough keys to give each writer at least one slot. */
    if (cfg.num_keys < cfg.num_writers) cfg.num_keys = cfg.num_writers;

    int num_readers = cfg.num_threads - cfg.num_writers;

    if (splinter_create_or_open(cfg.store_name, cfg.slots, cfg.max_value_size) != 0) {
        perror("splinter_create_or_open");
        return 1;
    }

    if (cfg.scrub) splinter_set_mop(1);

    char **keys = calloc((size_t)cfg.num_keys, sizeof(char*));
    if (!keys) { perror("calloc"); return 1; }

    for (i = 0; i < cfg.num_keys; i++) {
        keys[i] = malloc(32);
        if (!keys[i]) { perror("malloc"); return 1; }
        snprintf(keys[i], 32, "k%08d", i);
    }

    counters_t ctr = {0};
    volatile int running = 1;
    shared_t sh = {
        .cfg = &cfg,
        .ctr = &ctr,
        .running = &running,
        .keys = keys,
        .num_keys = cfg.num_keys,
    };

    puts("===== MRMW CHI SAO STRESS TEST PLAN =====");
    printf(
        "Store    : %s\nThreads  : %d (writers=%d, readers=%d)\nDuration : %d ms\n"
        "Slots    : %d\nH-Scrub  : %s\nHot Keys : %d\nW/Backoff: %d us\nMax Val  : %d bytes\n",
        cfg.store_name,
        cfg.num_threads, cfg.num_writers, num_readers,
        cfg.test_duration_ms,
        cfg.slots,
        (cfg.scrub == 1) ? "Yes" : "No",
        cfg.num_keys,
        cfg.writer_period_us,
        cfg.max_value_size
    );

#ifdef SPLINTER_PERSISTENT
    puts("");
    puts("*** WARNING: Persistent Mode Detected ***");
    puts("");
    puts("Running this test can cause considerable wear on rotating media and older SSDs");
    puts("Additionally, it should not be run over rDMA or NFS.");
    puts("Sleeping five seconds in case you need to abort ...");
    puts("");
    sleep(5);
#endif

    printf("Pre-populating store with indexed backfill (%d keys) ...\n", cfg.num_keys);
    prepopulate(&sh);

    /*
     * Partition keys into disjoint lanes, one per writer.
     * Integer division floors; last writer absorbs the remainder so no key
     * is left unowned.
     */
    int lane_base = cfg.num_keys / cfg.num_writers;
    int lane_rem  = cfg.num_keys % cfg.num_writers;

    writer_ctx_t *wctx = calloc((size_t)cfg.num_writers, sizeof(writer_ctx_t));
    if (!wctx) { perror("calloc"); return 1; }

    int lane_off = 0;
    for (i = 0; i < cfg.num_writers; i++) {
        wctx[i].sh        = &sh;
        wctx[i].writer_id = i;
        wctx[i].lane_start = lane_off;
        wctx[i].lane_len   = lane_base + (i == cfg.num_writers - 1 ? lane_rem : 0);
        lane_off += wctx[i].lane_len;
    }

    puts("Creating threadpool ...");
    pthread_t *th = calloc((size_t)cfg.num_threads, sizeof(pthread_t));
    if (!th) { perror("calloc"); return 1; }

    long start = now_ms();

    printf(" -> Writers - (%d): ", cfg.num_writers);
    for (i = 0; i < cfg.num_writers; i++) {
        if (pthread_create(&th[i], NULL, writer_main, &wctx[i]) != 0) {
            perror("pthread_create writer");
            running = 0;
            goto cleanup;
        }
        fputc('+', stdout);
        fflush(stdout);
    }

    printf("\n -> Readers - (%d): ", num_readers);
    for (i = cfg.num_writers; i < cfg.num_threads; i++) {
        if (pthread_create(&th[i], NULL, reader_main, &sh) != 0) {
            perror("pthread_create reader");
            running = 0;
            break;
        }
        fputc('+', stdout);
        fflush(stdout);
    }

    puts("");
    puts("Test is now running ...");
    long seq = 0;
    char kibble[3] = { 0 };

    while (now_ms() - start < cfg.test_duration_ms) {
        seq++;
        usleep(10000);
        if (!quiet) {
            snprintf(kibble, sizeof(kibble) - 1, "%c", seq % 15 == 0 ? '.' : '\0');
            fputc(kibble[0], stdout);
            fflush(stdout);
            if (seq % 500 == 0) {
                fputc('\n', stdout);
                fflush(stdout);
            }
            if (seq % 3000 == 0) {
                fprintf(stdout, "\nThese dots indicate the passage of time while %d threads rip on a store.\n", cfg.num_threads);
                fprintf(stdout, "Thanks for your patience! The test was set to run for %d seconds total.\n\n", cfg.test_duration_ms / 1000);
                fflush(stdout);
            }
        }
    }
    running = 0;

    puts("");
    puts("\nCleaning up ...");

cleanup:
    for (i = 0; i < cfg.num_threads; i++) pthread_join(th[i], NULL);
    long elapsed = now_ms() - start;
    splinter_close();
    if (!keep_store) {
#ifndef SPLINTER_PERSISTENT
        snprintf(store_path, sizeof(store_path) - 1, "/dev/shm/%s", cfg.store_name);
#else
        snprintf(store_path, sizeof(store_path) - 1, "./%s", cfg.store_name);
#endif
        unlink(store_path);
    }
    for (i = 0; i < cfg.num_keys; i++) free(keys[i]);
    free(keys);
    free(wctx);
    free(th);

    puts("");
    print_stats(&cfg, &ctr, elapsed);

#ifdef HAVE_VALGRIND_H
    return VALGRIND_COUNT_ERRORS;
#else
    return 0;
#endif
}
