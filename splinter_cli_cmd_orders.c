/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_hist.c
 * @brief Implements the CLI 'hist' command.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "splinter_cli.h"

static const char *modname = "orders";

void help_cmd_orders(unsigned int level) {
    (void) level;
    printf("%s manages standard orders of a key for vector storage.\n", modname);
    printf("Usage: %s <set|unset> <key> <count>\n", modname);
    return;
}

int cmd_orders(int argc, char *argv[]) {
    if (argc < 4) {
        help_cmd_orders(1);
        return 1;
    }

    const char *mode = argv[1];
    const char *key = argv[2];
    uint8_t count = (uint8_t)atoi(argv[3]);

    if (strcmp(mode, "set") == 0) {
        if (argc < 5) return printf("Error: 'set' requires a value_prefix\n"), 1;
        
        const char *val_prefix = argv[4];
        char val_buf[1024];
        const void *vals[count];
        size_t lens[count];

        for (int i = 0; i < count; i++) {
            snprintf(val_buf, sizeof(val_buf), "%s_%d", val_prefix, i);
            vals[i] = strdup(val_buf); // Simplification for CLI
            lens[i] = strlen(val_buf);
        }

        int ret = splinter_client_set_tandem(key, vals, lens, count);
        printf("Tandem set for %s with %d orders: %s\n", key, count, ret == 0 ? "OK" : "FAIL");
    } 
    else if (strcmp(mode, "unset") == 0) {
        splinter_client_unset_tandem(key, count);
        printf("Tandem unset for %s (%d orders) requested.\n", key, count);
    }

    return 0;
}
