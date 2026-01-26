/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_config.c
 * @brief Implements the CLI 'config' command.
 */

#include <stdio.h>
#include <string.h>
#include <stdalign.h>
#include "splinter_cli.h"

static const char *modname = "config";

void help_cmd_config(unsigned int level) {
    (void) level;
    printf("Usage: %s\n       %s [feature_flag] [flag_value]\n", modname, modname);
    printf("If no other arguments are given, %s displays the current bus settings.\n", modname);
    printf("Supported flags:\n\t\"av\" -> 1 or 0\n\n");
    return;
}

static void show_bus_config(void) {
    splinter_header_snapshot_t snap = {0};

    splinter_get_header_snapshot(&snap);

    printf("magic:       %u\n", snap.magic);
    printf("version:     %u\n", snap.version);
    printf("slots:       %u\n", snap.slots);
    printf("alignment:   %zu\n", alignof(struct splinter_slot));
    printf("max_val_sz:  %u\n", snap.max_val_sz);
    printf("epoch:       %lu\n", snap.epoch);
    printf("auto_vacuum: %u\n", snap.auto_vacuum);
    puts("");
    
    return;
}

int cmd_config(int argc, char *argv[]) {
    (void) argv;

    if (argc == 1) {
        show_bus_config();
        return 0;
    }

    if (argc == 3) {
        // okay for now, but will need more robust argument parsing here.
        // ideally we can get current values by passing just the key, for instance.
        // later on ...
        int opt = cli_safer_atoi(argv[2]);
        if (!strncmp(argv[1], "av", 2)) {
            if (opt > 1 || opt < 0) {
                fprintf(stderr, "Invalid setting flag (0 = off, 1 = on)");
                return 1;
            }
            return splinter_set_av(opt);
        } else {
            fprintf(stderr, "Invalid configuration token: %s\n", argv[1]);
            return 1;
        }
    }

    help_cmd_config(1);
    
    return 1;
}
