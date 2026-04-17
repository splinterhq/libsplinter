/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_caps.c
 * @brief Implements the CLI 'caps' command.
 */

#include <stdio.h>

#include "splinter_cli.h"
#include "config.h"

static const char *modname = "caps";

void help_cmd_caps(unsigned int level) {
    (void) level;
    printf("%s prints version, build, and compiled-in feature flags.\n", modname);
    printf("Usage: %s\n", modname);
    printf("Output is key=value, one per line; suitable for scripting.\n");
}

int cmd_caps(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    printf("version=%s\n", SPLINTER_VERSION);
    printf("build=%s\n",   SPLINTER_BUILD);

#ifdef HAVE_LUA
    printf("lua=yes\n");
#else
    printf("lua=no\n");
#endif

#ifdef HAVE_WASM
    printf("wasm=yes\n");
#else
    printf("wasm=no\n");
#endif

#ifdef HAVE_EMBEDDINGS
    printf("embeddings=yes\n");
#else
    printf("embeddings=no\n");
#endif

#ifdef HAVE_LLAMA
    printf("llama=yes\n");
#else
    printf("llama=no\n");
#endif

#ifdef HAVE_NUMA
    printf("numa=yes\n");
#else
    printf("numa=no\n");
#endif

#ifdef SPLINTER_PERSISTENT
    printf("persistent=yes\n");
#else
    printf("persistent=no\n");
#endif

    return 0;
}
