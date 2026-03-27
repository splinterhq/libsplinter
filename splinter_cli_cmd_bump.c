/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_bump.c
 * @brief Implements the CLI 'bump' command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "splinter_cli.h"

static const char *modname = "bump";

void help_cmd_bump(unsigned int level) {
    (void) level;
    printf("%s advances a key's epoch by one operation (a value of 2).\n", modname);
    printf("%s is designed to be used after all bloom labels have been adjusted.\n", modname);
    printf("Usage: %s <key_name>\n", modname);
    return;
}

int cmd_bump(int argc, char *argv[]) {
    char *tmp = getenv("SPLINTER_NS_PREFIX");
    char key[SPLINTER_KEY_MAX] = { 0 };
    int rc = -1;

    if (argc != 2) {
        help_cmd_bump(1);
        return -1;
    }

    snprintf(key, sizeof(key) - 1, "%s%s", tmp == NULL ? "" : tmp, argv[1]);
    rc = splinter_bump_slot(key);
    cli_show_key_config(key, modname);
    
    return rc;
}