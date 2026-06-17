/**
 * Copyright 2026 Tim Post
 * License: Apache 2
 *
 * @file splinter_cli_cmd_retrain.c
 * @brief Implements the CLI 'retrain' command; scrubs a key's vectors and
 * flips its epoch back to 4, releasing the sequence lock and republishing the
 * key so clients and watchers revalidate it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "splinter_cli.h"

static const char *modname = "retrain";

void help_cmd_retrain(unsigned int level) {
    (void) level;
    printf("%s prepares a key for retraining: it zeroes the key's vectors and\n", modname);
    printf("flips its epoch back to 4, releasing the sequence lock and republishing it.\n");
    printf("The epoch moving backwards signals clients and watchers to revalidate the key.\n");
    printf("Works even when vectors are not enabled; in that case only the epoch is flipped.\n");
    printf("Usage: %s <key_name>\n", modname);
    return;
}

int cmd_retrain(int argc, char *argv[]) {
    char *tmp = getenv("SPLINTER_NS_PREFIX");
    char key[SPLINTER_KEY_MAX] = { 0 };
    int rc = -1;

    if (argc != 2) {
        help_cmd_retrain(1);
        return -1;
    }

    snprintf(key, sizeof(key) - 1, "%s%s", tmp == NULL ? "" : tmp, argv[1]);
    rc = splinter_retrain_slot(key);
    if (rc != 0)
        fprintf(stderr, "%s: could not retrain '%s' (key not found?)\n", modname, key);

    return rc;
}
