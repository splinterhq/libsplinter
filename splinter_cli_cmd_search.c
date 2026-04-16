/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_search.c
 * @brief Implements the CLI 'search' command.
 */

#ifdef HAVE_EMBEDDINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <getopt.h>
#include <unistd.h>

#include "splinter_cli.h"
#include "3rdparty/grawk.h"

static const char *modname = "search";

static splinter_header_snapshot_t snap = {0};

typedef struct {
    char key[SPLINTER_KEY_MAX];
    float similarity;
    float distance;
    uint64_t epoch;
    uint32_t val_len;
    uint8_t type_flag;
    uint64_t bloom;
    int has_embedding;
} search_result_t;

typedef struct {
    char **keynames;
    size_t count;
    size_t max;
} enum_ctx_t;

static float cosine_similarity(const float *a, const float *b, int dim) {
    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
    for (int i = 0; i < dim; i++) {
        dot   += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }
    float denom = sqrtf(mag_a) * sqrtf(mag_b);
    if (denom < 1e-9f) return 0.0f;
    return dot / denom;
}

static float euclidean_distance(const float *a, const float *b, int dim) {
    float sum = 0.0f;
    for (int i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

static int compare_results(const void *a, const void *b) {
    const search_result_t *ra = (const search_result_t *)a;
    const search_result_t *rb = (const search_result_t *)b;
    if (rb->similarity > ra->similarity) return  1;
    if (rb->similarity < ra->similarity) return -1;
    if (ra->distance   < rb->distance)   return -1;
    if (ra->distance   > rb->distance)   return  1;
    return 0;
}

static void enum_callback(const char *key, uint64_t version, void *data) {
    (void)version;
    enum_ctx_t *ctx = (enum_ctx_t *)data;
    if (ctx->count >= ctx->max) return;
    ctx->keynames[ctx->count] = (char *)key;
    ctx->count++;
}

void help_cmd_search(unsigned int level) {
    (void) level;
    printf("%s searches embedded keys by semantic similarity.\n", modname);
    printf("Usage: %s \"<query>\" [--json] [--limit N] [--distance F] [--similarity F] [--bloom MASK] [--regex PATTERN]\n", modname);
    printf("  --json          Output results as JSON\n");
    printf("  --limit N       Maximum number of results (default: unlimited)\n");
    printf("  --distance F    Maximum euclidean distance to include (default: no filter)\n");
    printf("  --similarity F  Minimum cosine similarity to include (default: no filter)\n");
    printf("  --bloom MASK    Hex bloom mask; uses enumerate_matches() for pre-filtering\n");
    printf("  --regex PATTERN Regex filter applied to key names before scoring\n");
    return;
}

static const struct option search_long_options[] = {
    { "json",       no_argument,       NULL, 'j' },
    { "limit",      required_argument, NULL, 'L' },
    { "distance",   required_argument, NULL, 'd' },
    { "similarity", required_argument, NULL, 's' },
    { "bloom",      required_argument, NULL, 'B' },
    { "regex",      required_argument, NULL, 'r' },
    { NULL, 0, NULL, 0 }
};

int cmd_search(int argc, char *argv[]) {
    grawk_t *g = NULL;
    awk_pat_t *filter = NULL;
    grawk_opts_t opts = {
        .ignore_case = 0,
        .invert_match = 0,
        .quiet = 1
    };

    splinter_slot_snapshot_t *slots = NULL;
    char **keynames = NULL;
    search_result_t *results = NULL;
    size_t entry_count = 0;
    size_t max_keys = 0;
    int rc = -1, i, x = 0, result_count = 0;
    int scratch_written = 0;

    char *query_text = NULL;
    int use_json = 0;
    int limit = 0;
    float max_distance = 0.0f;
    float min_similarity = 0.0f;
    uint64_t bloom_mask = 0;
    char *regex_pattern = NULL;
    int embedding_available = 1;

    char scratch_key[SPLINTER_KEY_MAX];
    snprintf(scratch_key, sizeof(scratch_key), "__sqtmp_%d", (int)getpid());

    /* Query text must be argv[1] and must not look like a flag */
    if (argc < 2 || argv[1][0] == '-') {
        help_cmd_search(1);
        return -1;
    }
    query_text = argv[1];

    /* Parse flags starting after the positional query argument */
    optind = 2;
    int opt;
    while ((opt = getopt_long(argc, argv, "jL:d:s:B:r:", search_long_options, NULL)) != -1) {
        switch (opt) {
            case 'j':
                use_json = 1;
                break;
            case 'L':
                limit = atoi(optarg);
                break;
            case 'd':
                max_distance = (float)atof(optarg);
                break;
            case 's':
                min_similarity = (float)atof(optarg);
                break;
            case 'B': {
                char *endptr;
                bloom_mask = strtoull(optarg, &endptr, 16);
                break;
            }
            case 'r':
                regex_pattern = optarg;
                break;
            default:
                break;
        }
    }

    if (!thisuser.store_conn) {
        fprintf(stderr, "%s: not connected to a store.\n", modname);
        return -1;
    }

    /* Get header snapshot to determine allocation size */
    splinter_get_header_snapshot(&snap);
    max_keys = snap.slots;

    if (max_keys == 0) {
        fprintf(stderr, "%s: no slots available in current store.\n", modname);
        return -1;
    }

    keynames = (char **)calloc(max_keys, sizeof(char *));
    if (keynames == NULL) {
        fprintf(stderr, "%s: unable to allocate memory for key names.\n", modname);
        errno = ENOMEM;
        return -1;
    }

    slots = (splinter_slot_snapshot_t *)calloc(max_keys, sizeof(splinter_slot_snapshot_t));
    if (slots == NULL) {
        fprintf(stderr, "%s: unable to allocate memory for slot snapshots.\n", modname);
        errno = ENOMEM;
        free(keynames);
        return -1;
    }

    results = (search_result_t *)calloc(max_keys, sizeof(search_result_t));
    if (results == NULL) {
        fprintf(stderr, "%s: unable to allocate memory for results.\n", modname);
        errno = ENOMEM;
        free(slots);
        free(keynames);
        return -1;
    }

    /* Write query to scratch key and trigger splinference */
    splinter_set(scratch_key, query_text, strlen(query_text));
    splinter_set_label(scratch_key, 1ULL);
    splinter_bump_slot(scratch_key);
    scratch_written = 1;

    int poll_rc = splinter_poll(scratch_key, 2000);

    float query_vec[SPLINTER_EMBED_DIM];

    if (poll_rc == 0) {
        if (splinter_get_embedding(scratch_key, query_vec) != 0) {
            fprintf(stderr, "%s: warning: could not retrieve query embedding; scoring unavailable.\n", modname);
            embedding_available = 0;
        }
    } else if (poll_rc == -1 && errno == ETIMEDOUT) {
        fprintf(stderr, "%s: warning: splinference timed out; embedding scoring unavailable.\n", modname);
        embedding_available = 0;
    } else if (poll_rc == -2) {
        fprintf(stderr, "%s: store error during poll.\n", modname);
        rc = -1;
        goto cleanup;
    }

    /* Without embedding and no secondary filters, nothing to do */
    if (!embedding_available && regex_pattern == NULL && bloom_mask == 0) {
        fprintf(stderr, "%s: splinference unavailable and no regex/bloom filter; no results.\n", modname);
        rc = 0;
        goto cleanup;
    }

    /* Store traversal */
    if (bloom_mask != 0) {
        enum_ctx_t ctx = { keynames, 0, max_keys };
        splinter_enumerate_matches(bloom_mask, enum_callback, &ctx);
        entry_count = ctx.count;
    } else {
        rc = splinter_list(keynames, max_keys, &entry_count);
        if (rc != 0) {
            fprintf(stderr, "%s: failed to list keys.\n", modname);
            goto cleanup;
        }
    }

    /* Set up grawk for optional regex filtering */
    g = grawk_init();
    if (g == NULL) {
        fprintf(stderr, "%s: unable to allocate memory to filter keys.\n", modname);
        errno = ENOMEM;
        rc = -1;
        goto cleanup;
    }

    grawk_set_options(g, &opts);
    if (regex_pattern != NULL) {
        filter = grawk_build_pattern(regex_pattern);
        grawk_set_pattern(g, filter);
    }

    /* Build slot snapshot array, skipping scratch key and applying regex */
    for (i = 0; keynames[i] && (size_t)i < max_keys; i++) {
        if (keynames[i][0] == '\0') continue;
        if (strncmp(keynames[i], scratch_key, SPLINTER_KEY_MAX) == 0) continue;
        if (filter != NULL && !grawk_match(g, keynames[i])) continue;
        splinter_get_slot_snapshot(keynames[i], &slots[x]);
        x++;
    }

    /* Score each candidate slot */
    for (i = 0; i < x; i++) {
        if (slots[i].epoch == 0) continue;

        search_result_t res;
        memset(&res, 0, sizeof(res));
        strncpy(res.key, slots[i].key, SPLINTER_KEY_MAX - 1);
        res.epoch     = slots[i].epoch;
        res.val_len   = slots[i].val_len;
        res.type_flag = slots[i].type_flag;
        res.bloom     = slots[i].bloom;

        if (embedding_available) {
            float slot_vec[SPLINTER_EMBED_DIM];
            int has_embed = (splinter_get_embedding(slots[i].key, slot_vec) == 0);
            if (!has_embed) {
                res.similarity    = 0.0f;
                res.distance      = FLT_MAX;
                res.has_embedding = 0;
                /* Skip silently if any score filter is active */
                if (min_similarity > 0.0f || max_distance > 0.0f) continue;
            } else {
                res.similarity    = cosine_similarity(query_vec, slot_vec, SPLINTER_EMBED_DIM);
                res.distance      = euclidean_distance(query_vec, slot_vec, SPLINTER_EMBED_DIM);
                res.has_embedding = 1;
                if (min_similarity > 0.0f && res.similarity < min_similarity) continue;
                if (max_distance   > 0.0f && res.distance   > max_distance)   continue;
            }
        } else {
            res.similarity    = 0.0f;
            res.distance      = FLT_MAX;
            res.has_embedding = 0;
            if (min_similarity > 0.0f || max_distance > 0.0f) continue;
        }

        results[result_count++] = res;
    }

    /* Sort descending by similarity, ascending by distance */
    qsort(results, result_count, sizeof(search_result_t), compare_results);

    /* Apply limit */
    int print_count = result_count;
    if (limit > 0 && print_count > limit) print_count = limit;

    /* Output */
    if (use_json) {
        printf("{\n");
        printf("  \"query\": \"%s\",\n", query_text);
        printf("  \"store\": {\n");
        printf("    \"total_slots\": %u,\n", snap.slots);
        printf("    \"active_keys\": %d\n", result_count);
        printf("  },\n");
        printf("  \"results\": [\n");
        for (i = 0; i < print_count; i++) {
            printf("    {\n");
            printf("      \"key\": \"%s\",\n", results[i].key);
            if (results[i].has_embedding) {
                printf("      \"similarity\": %.4f,\n", results[i].similarity);
                printf("      \"distance\": %.4f,\n", results[i].distance);
            } else {
                printf("      \"similarity\": null,\n");
                printf("      \"distance\": null,\n");
            }
            printf("      \"epoch\": %lu,\n", results[i].epoch);
            printf("      \"value_length\": %u,\n", results[i].val_len);
            printf("      \"type\": \"%s\",\n", cli_show_key_type((unsigned short)results[i].type_flag));
            printf("      \"bloom\": \"0x%016llx\",\n", (unsigned long long)results[i].bloom);
            printf("      \"has_embedding\": %s\n", results[i].has_embedding ? "true" : "false");
            if (i < print_count - 1) {
                printf("    },\n");
            } else {
                printf("    }\n");
            }
        }
        printf("  ]\n");
        printf("}\n");
    } else {
        printf("%-44s %-10s %-10s %-6s %-6s %s\n",
            "Name", "Similarity", "Distance", "Epoch", "Len", "Type");
        for (i = 0; i < print_count; i++) {
            if (results[i].has_embedding) {
                printf("%-44s %-10.4f %-10.4f %-6lu %-6u %s\n",
                    results[i].key,
                    results[i].similarity,
                    results[i].distance,
                    results[i].epoch,
                    results[i].val_len,
                    cli_show_key_type((unsigned short)results[i].type_flag));
            } else {
                printf("%-44s %-10s %-10s %-6lu %-6u %s\n",
                    results[i].key,
                    "-",
                    "-",
                    results[i].epoch,
                    results[i].val_len,
                    cli_show_key_type((unsigned short)results[i].type_flag));
            }
        }
        puts("");
    }

    rc = 0;

cleanup:
    if (scratch_written) splinter_unset(scratch_key);
    if (g != NULL) grawk_free(g);
    if (results != NULL) free(results);
    if (slots != NULL) free(slots);
    if (keynames != NULL) free(keynames);

    return rc;
}
#endif // HAVE_EMBEDDINGS