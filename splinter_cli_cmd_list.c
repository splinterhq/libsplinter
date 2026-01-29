/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_list.c
 * @brief Implements the CLI 'list' command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "splinter_cli.h"
#include "3rdparty/grawk.h"

static const char *modname = "list";

static splinter_header_snapshot_t snap = {0};

void help_cmd_list(unsigned int level) {
    (void) level;
    printf("%s lists keys in the currently selected store.\n", modname);
    printf("Usage: %s [pattern] [max_lines]\n", modname);
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

int cmd_list(int argc, char *argv[]) {
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

        // Sort so the most-updated keys are at the top of the list
        qsort(slots, x, sizeof(splinter_slot_snapshot_t), compare_slots_by_epoch);

        printf("%-33s | %-10s | %-10s | %-15s\n",
            "Key Name",
            "Epoch",
            "Val Len",
            "Named Type"
        );
        for (i = 0; i < 85; i++)
            putchar('-');
        putchar('\n');
        
        for (i = 0; i < x && slots[i].epoch > 0; i++) {
            printf("%-33s | %-10lu | %-10u | %-15s\n", 
                slots[i].key,
                slots[i].epoch,
                slots[i].val_len,
                cli_show_key_type(slots[i].type_flag)
            );
        }

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
