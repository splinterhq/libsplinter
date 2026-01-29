/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_type.c
 * @brief Implements the CLI 'type' command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "splinter_cli.h"

static const char *modname = "type";

void help_cmd_type(unsigned int level) {
    (void) level;
    printf("%s displays or sets key type naming in the store.\n", modname);
    printf("Usage: %s <key_name>\n       %s <key_name> <type>\n", modname, modname);
    printf("\nTypes can be (one) of:\n  'void', 'bigint', 'biguint', 'json', 'binary', 'img', 'audio', 'vartext'\n");
    return;
}

int cmd_type(int argc, char *argv[]) {
    char *tmp = getenv("SPLINTER_NS_PREFIX");
    char key[SPLINTER_KEY_MAX] = { 0 };
    uint16_t bitmask = (1u << 0);
    splinter_slot_snapshot_t snap = { 0 };

    if (argc < 2) {
        help_cmd_type(1);
        return -1;
    }

    snprintf(key, sizeof(key) - 1, "%s%s", tmp == NULL ? "" : tmp, argv[1]);
    if (splinter_get_slot_snapshot(key, &snap) != 0) {
        perror("splinter_get_slot_snapshot");
        return -1;
    }

    if (argc == 2) {
        printf("%s:%s\n", cli_show_key_type(snap.type_flag), key);
        puts("");
        return 0;
    } else if (argc == 3) {
        bitmask = cli_type_to_bitmask(argv[2]);
        if (!bitmask) {
            fprintf(stderr, "%s: invalid bitmask alias: '%s'\n", modname, argv[2]);
            return -1;
        }
        return splinter_set_named_type(key, bitmask);
    }

    help_cmd_type(1);
    return 1;
}
