/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_init.c
 * @brief Implements the CLI 'init' command.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <stdalign.h>

#include "config.h"
#include "splinter.h"
#include "splinter_cli.h"

static const char *modname = "init";

void help_cmd_init(unsigned int level) {
    (void) level;

    printf("Usage: %s [store_name] [--slots num_slots] [--maxlen max_val_len]\n", modname);
    printf("%s creates a Splinter store to default or specific geometry.\n", modname);
    puts("If arguments are omitted, these compiled-in defaults are used:");
    printf("\nname:  %s\nslots:  %lu\nmaxlen: %lu\nalignment: %zu\n",
        DEFAULT_BUS,
        (unsigned long) DEFAULT_SLOTS,
        (unsigned long) DEFAULT_VAL_MAXLEN,
	    alignof(struct splinter_slot)
	);

    return;
}

static const struct option long_options[] = {
    { "help", no_argument, NULL, 'h' },
    { "slots", required_argument, NULL, 's' },
    { "maxlen", required_argument, NULL, 'l' },
    { NULL, 0, NULL, 0 }
};

static const char *optstring = "hs:l:";

int cmd_init(int argc, char *argv[]) {
    char *buff = NULL, save[64] = { 0 }, store[64] = { 0 };
    int rc = 0, opt = 0;
    unsigned int prev_conn = 0;
    unsigned long max_slots = DEFAULT_SLOTS, max_val = DEFAULT_VAL_MAXLEN;

    if (thisuser.store_conn) {
        strncpy(save, thisuser.store, 64);
        prev_conn = 1;
    }

    optind = 0;

    while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
            case '?':
                help_cmd_init(1);
                rc = 0;
                goto restore_conn;
                break;
            case 'o':
                max_slots = strtoul(optarg, &buff, 10);
                break;
            case 'l':
                max_val = strtoul(optarg, &buff, 10);
                break;
        }
    }

    if (optind < argc)
        snprintf(store, sizeof(store) -1, "%s", argv[optind++]);

    if (! store[0])
        snprintf(store, sizeof(store) - 1, DEFAULT_BUS);

    
    // Inside cmd_init after parsing options
    size_t slot_sz = sizeof(struct splinter_slot); 
    size_t arena_sz = max_slots * max_val;
    size_t total_est = sizeof(struct splinter_header) + (max_slots * slot_sz) + arena_sz;

    printf("Initializing store: %s\n", store);
    printf("  - Slots: %lu (%zu bytes each, %zu byte alignment)\n", 
        max_slots,
        max_val,
        alignof(struct splinter_slot));
    printf("  - Value Arena: %lu bytes, SRS: %zu bytes (~%.2f MB)\n", 
        arena_sz,
        total_est, 
        (double) total_est / 1048576.0);

    rc = splinter_create(store, max_slots, max_val);

    if (rc < 0)
        perror("splinter_create");
  
    splinter_close();
    goto restore_conn;

restore_conn:
    if (prev_conn) {
        rc = splinter_open(save);
        if (rc != 0) {
            perror("splinter_open");
            fprintf(stderr, 
                "warning: could not re-attach to %s, did something else remove it?",
                save);
            thisuser.store_conn = 0;
            return rc;
        } else {
            strncpy(thisuser.store, save, 64);
            thisuser.store_conn = 1;
        }
    }

    return rc;
}
