/**
 * Copyright 2026 Tim Post
 * License: Apache 2
 *
 * @file splinter_cli_cmd_ingest.c
 * @brief Implements the CLI 'ingest' command.
 *
 * Reads a file (or stdin) and stores it in the current Splinter store as a
 * set of tandem slots sized to the store's max_val_sz geometry:
 *
 *   <key>        JSON metadata: chunk count, byte size, source filename,
 *                ingestion timestamp. Typed JSON, labeled SPL_INGEST_META.
 *                This is "slot 0" by convention — no order accessor suffix.
 *
 *   <key>.1      First content chunk. Typed VARTEXT, labeled SPL_INGEST_CHUNK.
 *                Bumped after write so splinference picks it up immediately.
 *   <key>.2 ...  Subsequent chunks, same treatment.
 *
 * The key name defaults to the basename of the file argument. For stdin,
 * a key name must be supplied via --key.
 *
 * Usage:
 *   ingest <file>               key = basename(file)
 *   ingest <file> --key mykey   key = mykey
 *   ingest --key mykey          reads from stdin
 *   ingest --key mykey --label 0x200   override chunk label
 *
 * Label conventions (matching main.ts / splainference):
 *   SPL_INGEST_CHUNK  0x200   tells splinference: embed this slot
 *   SPL_INGEST_META   0x400   tells governance: this is a document index
 *
 * Both labels are OR'd atomically; they do not interfere with user labels
 * already present on the store.
 */

#ifdef HAVE_EMBEDDINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include <getopt.h>

#include "splinter_cli.h"
#include "splinter.h"

/* Default labels — match main.ts pulse convention and leave headroom. */
#define SPL_INGEST_CHUNK  0x200ULL
#define SPL_INGEST_META   0x400ULL

static const char *modname = "ingest";

void help_cmd_ingest(unsigned int level) {
    printf("Usage: %s [file] [--key <key>] [--label <hex>]\n", modname);
    printf("       %s --key <key>              (reads from stdin)\n", modname);
    puts("");
    puts("Reads a file or stdin and stores it as chunked tandem slots.");
    puts("Chunks are typed VARTEXT and labeled for splinference pickup.");
    puts("A JSON metadata slot is written to the base key (no suffix).");
    if (level) {
        puts("");
        puts("Key name defaults to basename(file). For stdin --key is required.");
        puts("Chunk size is derived from the store's max_val_sz at runtime.");
        puts("Keys longer than 56 chars will be truncated to leave room for");
        puts("the order accessor suffix (.<N> up to .999).");
        puts("");
        puts("Label bits (default):");
        printf("  chunk label: 0x%llx  (tells splinference: embed this slot)\n",
               (unsigned long long)SPL_INGEST_CHUNK);
        printf("  meta  label: 0x%llx  (document index; not embedded)\n",
               (unsigned long long)SPL_INGEST_META);
        puts("");
        puts("Override the chunk label with --label if your .splinterrc uses");
        puts("a different bit for incoming VARTEXT data.");
    }
}

/* -------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------*/

/*
 * Truncate key to leave room for the order accessor suffix.
 * Worst case suffix is ".999\0" = 5 bytes, so cap base key at
 * SPLINTER_KEY_MAX - 5.
 */
static void safe_key(const char *src, char *dst, size_t dst_sz) {
    size_t max_base = dst_sz - 5;
    size_t len = strnlen(src, max_base);
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/*
 * Write the JSON metadata slot (base key, no suffix).
 * JSON is hand-built to avoid a dependency on any JSON library.
 * Format is intentionally minimal and forward-compatible:
 *   {"chunks":<n>,"bytes":<b>,"source":"<s>","ingested":<t>}
 */
static int write_metadata(const char *key,
                          size_t      chunks,
                          size_t      total_bytes,
                          const char *source,
                          time_t      ts) {
    char meta[512];
    int n = snprintf(meta, sizeof(meta),
        "{\"chunks\":%zu,\"bytes\":%zu,\"source\":\"%s\",\"ingested\":%lld}",
        chunks,
        total_bytes,
        source,
        (long long)ts);

    if (n <= 0 || (size_t)n >= sizeof(meta)) {
        fprintf(stderr, "%s: metadata too large to fit in buffer\n", modname);
        return -1;
    }

    if (splinter_set(key, meta, (size_t)n) != 0) {
        fprintf(stderr, "%s: failed to write metadata slot '%s'\n", modname, key);
        return -1;
    }

    if (splinter_set_named_type(key, SPL_SLOT_TYPE_JSON) != 0) {
        fprintf(stderr, "%s: warning: could not type metadata slot as JSON\n", modname);
    }

    if (splinter_set_label(key, SPL_INGEST_META) != 0) {
        fprintf(stderr, "%s: warning: could not label metadata slot\n", modname);
    }

    return 0;
}

/*
 * Write one content chunk as key.<order> (order is 1-based).
 * Types it VARTEXT, labels it for splinference, then bumps.
 */
static int write_chunk(const char *base_key,
                       size_t      order,
                       const void *data,
                       size_t      len,
                       uint64_t    chunk_label) {
    char slot_key[SPLINTER_KEY_MAX];
    snprintf(slot_key, sizeof(slot_key), "%s%s%zu",
             base_key, SPL_ORDER_ACCESSOR, order);

    if (splinter_set(slot_key, data, len) != 0) {
        fprintf(stderr, "%s: failed to write chunk %zu ('%s')\n",
                modname, order, slot_key);
        return -1;
    }

    if (splinter_set_named_type(slot_key, SPL_SLOT_TYPE_VARTEXT) != 0) {
        fprintf(stderr, "%s: warning: could not type chunk %zu as VARTEXT\n",
                modname, order);
    }

    if (splinter_set_label(slot_key, chunk_label) != 0) {
        fprintf(stderr, "%s: warning: could not label chunk %zu\n",
                modname, order);
    }

    /* Bump fires splinference watchers registered on this label. */
    if (splinter_bump_slot(slot_key) != 0) {
        fprintf(stderr, "%s: warning: bump failed on chunk %zu\n",
                modname, order);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------*/

static const struct option long_options[] = {
    { "key",   required_argument, NULL, 'k' },
    { "label", required_argument, NULL, 'l' },
    { "help",  no_argument,       NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

static const char *optstring = "k:l:h";

int cmd_ingest(int argc, char *argv[]) {
    char        base_key[SPLINTER_KEY_MAX] = { 0 };
    char        key_arg[SPLINTER_KEY_MAX]  = { 0 };
    const char *file_arg   = NULL;
    uint64_t    chunk_label = SPL_INGEST_CHUNK;
    int         opt;

    optind = 0;
    while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
        switch (opt) {
            case 'k':
                strncpy(key_arg, optarg, sizeof(key_arg) - 1);
                break;
            case 'l': {
                char *end;
                chunk_label = strtoull(optarg, &end, 0);
                if (*end != '\0' || chunk_label == 0) {
                    fprintf(stderr, "%s: invalid label '%s'\n", modname, optarg);
                    return 1;
                }
                break;
            }
            case 'h':
                help_cmd_ingest(1);
                return 0;
            default:
                help_cmd_ingest(0);
                return 1;
        }
    }

    /* First non-option argument is the file path. */
    if (optind < argc) {
        file_arg = argv[optind];
    }

    /* Resolve key name. */
    if (key_arg[0] != '\0') {
        safe_key(key_arg, base_key, sizeof(base_key));
    } else if (file_arg != NULL) {
        /* basename() may modify its argument; work on a copy. */
        char path_copy[4096];
        strncpy(path_copy, file_arg, sizeof(path_copy) - 1);
        safe_key(basename(path_copy), base_key, sizeof(base_key));
    } else {
        fprintf(stderr, "%s: stdin input requires --key <name>\n", modname);
        help_cmd_ingest(0);
        return 1;
    }

    /* Apply namespace prefix if set. */
    char *ns = getenv("SPLINTER_NS_PREFIX");
    if (ns && ns[0]) {
        char prefixed[SPLINTER_KEY_MAX];
        snprintf(prefixed, sizeof(prefixed), "%s%s", ns, base_key);
        strncpy(base_key, prefixed, sizeof(base_key) - 1);
        base_key[sizeof(base_key) - 1] = '\0';
    }

    /* Open input. */
    FILE *fp = stdin;
    if (file_arg != NULL) {
        fp = fopen(file_arg, "rb");
        if (!fp) {
            fprintf(stderr, "%s: cannot open '%s': %s\n",
                    modname, file_arg, strerror(errno));
            return 1;
        }
    }

    /* Read the store geometry so chunks fit. */
    splinter_header_snapshot_t snap = { 0 };
    if (splinter_get_header_snapshot(&snap) != 0 || snap.max_val_sz == 0) {
        fprintf(stderr, "%s: could not read store geometry; is a store open?\n",
                modname);
        if (fp != stdin) fclose(fp);
        return 1;
    }

    /*
     * Leave a small safety margin below max_val_sz so we don't race
     * the slot boundary. 64 bytes is enough for any incidental bookkeeping.
     */
    size_t chunk_sz = (snap.max_val_sz > 64) ? snap.max_val_sz - 64 : snap.max_val_sz;

    char  *chunk_buf = malloc(chunk_sz);
    if (!chunk_buf) {
        fprintf(stderr, "%s: out of memory allocating chunk buffer\n", modname);
        if (fp != stdin) fclose(fp);
        return 1;
    }

    size_t total_bytes  = 0;
    size_t chunk_count  = 0;
    int    rc           = 0;
    time_t ingest_time  = time(NULL);
    const char *source  = (file_arg != NULL) ? file_arg : "(stdin)";

    printf("[ingest] key='%s' chunk_sz=%zu source='%s'\n",
           base_key, chunk_sz, source);

    /* Read and write chunks. */
    size_t nread;
    while ((nread = fread(chunk_buf, 1, chunk_sz, fp)) > 0) {
        chunk_count++;
        total_bytes += nread;

        if (write_chunk(base_key, chunk_count, chunk_buf, nread, chunk_label) != 0) {
            rc = 1;
            goto cleanup;
        }

        printf("[ingest] chunk %zu: %zu bytes\n", chunk_count, nread);
    }

    if (ferror(fp)) {
        fprintf(stderr, "%s: read error on '%s': %s\n",
                modname, source, strerror(errno));
        rc = 1;
        goto cleanup;
    }

    if (chunk_count == 0) {
        fprintf(stderr, "%s: input was empty, nothing ingested\n", modname);
        rc = 1;
        goto cleanup;
    }

    /* Write metadata slot last so chunk_count is accurate. */
    if (write_metadata(base_key, chunk_count, total_bytes, source, ingest_time) != 0) {
        rc = 1;
        goto cleanup;
    }

    printf("[ingest] done: %zu chunk(s), %zu bytes total -> '%s'\n",
           chunk_count, total_bytes, base_key);

cleanup:
    free(chunk_buf);
    if (fp != stdin) fclose(fp);
    return rc;
}

#endif // HAVE_EMBEDDINGS

