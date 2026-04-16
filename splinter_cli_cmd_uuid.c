/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_uuid.c
 * @brief Implements the CLI 'uuid' command.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "3rdparty/uuid4.h"
#include "splinter_cli.h"

static const char *modname = "uuid";

void help_cmd_uuid(unsigned int level) {
    (void) level;

    printf("%s provides a UUID v4 unique identifier.\n",
        modname);
}

int cmd_uuid(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    char buff[UUID4_LEN] = { 0 };
    if (uuid4_init() < 0) return 1;
    uuid4_generate(buff);
    printf("%s\n", buff);
    return 0;
}
