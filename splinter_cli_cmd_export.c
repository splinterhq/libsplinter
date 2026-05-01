/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_export.c
 * @brief Implements the CLI 'export' command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "splinter_cli.h"
#include "3rdparty/grawk.h"

static const char *modname = "export";

static splinter_header_snapshot_t snap = {0};

void help_cmd_export(unsigned int level) {
    (void) level;
    printf("%s exports the store in various formats to standard output.\n", modname);
    printf("Usage: %s [format (default=json)] [max_lines (default=0/unlimited)]\n", modname);
    printf("Format can be one of: json (more coming soon)\n");
    return;
}

static int compare_slots_by_epoch(const void *a, const void *b) {
    const splinter_slot_snapshot_t *slot_a = (const splinter_slot_snapshot_t *)a;
    const splinter_slot_snapshot_t *slot_b = (const splinter_slot_snapshot_t *)b;
    
    // Descending order: b - a
    if (slot_b->epoch > slot_a->epoch) return 1;
    if (slot_b->epoch < slot_a->epoch) return -1;
    
    return 0;
}

/**
 * @brief Emit a buffer as a JSON-escaped single-line string (no surrounding quotes).
 *
 * Handles RFC 8259 mandatory escapes (", \, control chars), turns newlines/tabs
 * into their short forms, and emits remaining control bytes as \u00XX. Bytes
 * >= 0x80 are passed through verbatim — VARTEXT is expected to be UTF-8.
 */
static void json_print_escaped(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        switch (c) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\b': fputs("\\b",  stdout); break;
            case '\f': fputs("\\f",  stdout); break;
            case '\n': fputs("\\n",  stdout); break;
            case '\r': fputs("\\r",  stdout); break;
            case '\t': fputs("\\t",  stdout); break;
            default:
                if (c < 0x20) {
                    printf("\\u%04x", c);
                } else {
                    fputc(c, stdout);
                }
                break;
        }
    }
}

/**
 * @brief Prints slot snapshots in JSON format
 * @param slots Sorted array of slot snapshots
 * @param slot_count Number of valid slots in the array
 * @param snap Pointer to bus header snapshot
 *
 * For VARTEXT-typed slots, the slot's value is fetched and emitted as a
 * single-line JSON string under the "value" field. Other types omit the
 * field — printability isn't guaranteed and the consumer can re-fetch
 * by key if needed.
 */
static void print_json(const splinter_slot_snapshot_t *slots, size_t slot_count,
                       const splinter_header_snapshot_t *snap) {
    size_t i;

    /* Reusable buffer for VARTEXT value reads. Sized to max_val_sz so any
     * slot fits; allocated once, freed at the end. */
    char *valbuf = NULL;
    size_t valbuf_sz = (snap->max_val_sz > 0) ? snap->max_val_sz : 0;
    if (valbuf_sz > 0) {
        valbuf = (char *)malloc(valbuf_sz);
        /* Falling through with valbuf == NULL just means we skip value
         * emission rather than failing the whole export. */
    }

    printf("{\n");
    printf("  \"store\": {\n");
    printf("    \"total_slots\": %u,\n", snap->slots);
    printf("    \"active_keys\": %zu\n", slot_count);
    printf("  },\n");
    printf("  \"keys\": [\n");

    for (i = 0; i < slot_count; i++) {
        // Skip empty/invalid entries
        if (slots[i].epoch == 0) {
            continue;
        }

        int is_vartext = (slots[i].type_flag & SPL_SLOT_TYPE_VARTEXT) != 0;
        int emit_value = (is_vartext && valbuf != NULL && slots[i].val_len > 0);

        printf("    {\n");
        printf("      \"key\": \"%s\",\n", slots[i].key);
        printf("      \"type\": \"%s\",\n", cli_show_key_type(slots[i].type_flag));
        printf("      \"epoch\": %lu,\n", slots[i].epoch);
        if (emit_value) {
            printf("      \"value_length\": %u,\n", slots[i].val_len);
            size_t got = 0;
            if (splinter_get(slots[i].key, valbuf, valbuf_sz, &got) == 0) {
                printf("      \"value\": \"");
                json_print_escaped(valbuf, got);
                printf("\"\n");
            } else {
                /* Read failed (slot moved, key missing, etc.) — degrade to null. */
                printf("      \"value\": null\n");
            }
        } else {
            printf("      \"value_length\": %u\n", slots[i].val_len);
        }

        // Add comma unless this is the last entry
        if (i < slot_count - 1 && slots[i + 1].epoch > 0) {
            printf("    },\n");
        } else {
            printf("    }\n");
        }
    }

    printf("  ]\n");
    printf("}\n");

    free(valbuf);
}

int cmd_export(int argc, char *argv[]) {
    grawk_t *g = NULL;
    awk_pat_t *filter = NULL;
    grawk_opts_t opts = { 
        .ignore_case = 0,
        .invert_match = 0,
        .quiet = 1
    };

    splinter_slot_snapshot_t *slots = NULL;
    char **keynames = NULL;
    size_t entry_count = 0;
    size_t max_keys = 0;
    int rc = -1, i, x = 0;

    if (argc > 2) {
        help_cmd_list(1);
        return -1;
    }

    // Get header snapshot to determine allocation size
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

    rc = splinter_list(keynames, max_keys, &entry_count);
    if (rc == 0) {
        g = grawk_init();
        if (g == NULL) {
            fprintf(stderr, "%s: unable to allocate memory to filter keys.\n", modname);
            errno = ENOMEM;
            rc = -1;
            goto cleanup;
        }
        
        grawk_set_options(g, &opts);
        if (argc == 2) {
            filter = grawk_build_pattern(argv[1]);
            grawk_set_pattern(g, filter);
        }

        for (i = 0; keynames[i]; i++) {
            if (keynames[i][0] != '\0') {
                // if there's no filter, just add it
                if (filter == NULL) {
                    splinter_get_slot_snapshot(keynames[i], &slots[x]);
                    x++;
                } else {
                    // only add if filter matches
                    if (grawk_match(g, keynames[i])) {
                        splinter_get_slot_snapshot(keynames[i], &slots[x]);
                        x++;
                    }
                }
            }
        }

        qsort(slots, x, sizeof(splinter_slot_snapshot_t), compare_slots_by_epoch);
        
        // TODO: Other formats / arguments
        print_json(slots, x, &snap);

        // Empty line is intentional (and uniform throughout commands) 
        puts("");
        rc = 0;
    }

cleanup:
    // Free grawk resources
    if (g != NULL) {
        grawk_free(g);
    }

    if (slots != NULL) {
        free(slots);
    }
    
    if (keynames != NULL) {
        free(keynames);
    }

    return rc;
}
