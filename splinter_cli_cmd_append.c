/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_set.c
 * @brief Implements the CLI 'append' command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "splinter_cli.h"

static const char *modname = "append";

void help_cmd_append(unsigned int level) {
    printf("%s appends a value to a key in the store\n", modname);
    printf("Usage: %s <key_name> \"<value_to_append>\"\n", modname);
    if (level)
        puts("\nKeys without spaces do not need to be quoted.");
    return;
}

int cmd_append(int argc, char *argv[]) {
    char key[SPLINTER_KEY_MAX] = { 0 };
    char *tmp = getenv("SPLINTER_NS_PREFIX");
    size_t result = 0;
    if (argc < 3) {
        help_cmd_append(1);
        return 1;
    }

    snprintf(key, sizeof(key) -1, "%s%s", tmp == NULL ? "" : tmp, argv[1]);
    return splinter_append(key, argv[2], strnlen(argv[2], 4096), &result);
}
