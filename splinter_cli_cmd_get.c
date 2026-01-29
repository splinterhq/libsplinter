/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_get.c
 * @brief Implements the 'get' CLI command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "splinter_cli.h"

static const char *modname = "get";

void help_cmd_get(unsigned int level) {
    (void) level;

    printf("%s gets the value of a key in the store.\n", modname);
    printf("Usage: %s <key_name>\n", modname);
    return;
}

int cmd_get(int argc, char *argv[]) {
    char buf[4096] = { 0 };
    char key[SPLINTER_KEY_MAX] = { 0 };
    char *tmp = getenv("SPLINTER_NS_PREFIX");
    size_t received = 0;
    splinter_slot_snapshot_t snap = { 0 };
    int rc = -1;

    if (argc != 2) {
        help_cmd_get(1);
        return -1;
    }
    
    snprintf(key, sizeof(key) -1, "%s%s", tmp == NULL ? "" : tmp, argv[1]);

    // 1. Get the data
    rc = splinter_get(key, buf, sizeof(buf), &received);
    if (rc != 0) {
        fprintf(stderr, "%s: unable to retrieve key '%s'\n", modname, key);
        return rc;
    }

    // 2. Get the type metadata
    if (splinter_get_slot_snapshot(key, &snap) != 0) {
        // Fallback to string if snapshot fails
        printf("%lu:%s\n", received, buf);
    } else {
        // 3. Type-Aware Printing
        if (snap.type_flag & SPL_SLOT_TYPE_BIGUINT) {
            uint64_t val = *(uint64_t *)buf;
            printf("%lu:%llu\n", received, (unsigned long long)val);
        } else {
            // Standard string output
            printf("%lu:%s\n", received, buf);
        }
    }

    puts("");
    return 0;
}