/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_unset.c
 * @brief Implements the CLI 'unset' command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "splinter_cli.h"

static const char *modname = "unset";

void help_cmd_unset(unsigned int level) {
    (void) level;
    printf("%s un-sets the value of a key in the store\n", modname);
    printf("Usage: %s [-r] <key_name>\n", modname);
    printf("  -r  recursively remove tandem keys (key%s1, key%s2, ...)\n",
           SPL_ORDER_ACCESSOR, SPL_ORDER_ACCESSOR);
    return;
}

int cmd_unset(int argc, char *argv[]) {
    size_t deleted = 0;
    char key[SPLINTER_KEY_MAX] = { 0 };
    char *tmp = getenv("SPLINTER_NS_PREFIX");
    int recursive = 0;
    const char *key_arg = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            recursive = 1;
        } else if (key_arg == NULL) {
            key_arg = argv[i];
        } else {
            help_cmd_unset(1);
            return 1;
        }
    }

    if (key_arg == NULL) {
        help_cmd_unset(1);
        return 1;
    }

    snprintf(key, sizeof(key) - 1, "%s%s", tmp == NULL ? "" : tmp, key_arg);

    int ret = splinter_unset(key);
    if (ret < 0)
        return ret;
    deleted += (size_t) ret;

    if (recursive) {
        char tandem_key[SPLINTER_KEY_MAX];
        for (uint8_t i = 1; i != 0; i++) {
            snprintf(tandem_key, sizeof(tandem_key) - 1, "%s%s%u", key, SPL_ORDER_ACCESSOR, i);
            ret = splinter_unset(tandem_key);
            if (ret < 0)
                break;
            deleted += (size_t) ret;
        }
    }

    printf("%lu bytes deleted.\n", deleted);

    return 0;
}