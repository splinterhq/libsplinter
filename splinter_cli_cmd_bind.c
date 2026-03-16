/**
 * @file splinter_cli_cmd_bind.c
 * @brief Implements the CLI 'bind' command to map Bloom labels to signal groups.
 * License: Apache 2
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>

#include "splinter_cli.h"
#include "splinter.h"

static const char *modname = "bind";

/**
 * @brief Display help for the bind command.
 */
void help_cmd_bind(unsigned int level) {
    (void) level;
    printf("Usage: %s [label_name | mask] <group_id> [--bloom]\n", modname);
    printf("Maps a semantic Bloom label to a signal group (0-63).\n");
    printf("When a key's label matches the mask, the signal group is pulsed.\n");
    puts("\nOptions:");
    puts("  -b, --bloom    Explicitly indicate this is a Bloom-to-Group binding.");
    puts("");
}

static const struct option long_options[] = {
    { "help",  no_argument,       NULL, 'h' },
    { "bloom", no_argument,       NULL, 'b' },
    { NULL,    0,                 NULL, 0   }
};

static const char *optstring = "hb";

/**
 * @brief CLI entry point for the 'bind' command.
 */
int cmd_bind(int argc, char *argv[]) {
    int opt = 0, group_id = -1;
    uint64_t mask = 0;
    bool found = false;
    bool bloom_mode = true;

    // Reset getopt state for REPL safety
    optind = 0;

    while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                help_cmd_bind(1);
                return 0;
            case 'b':
                // right now this is the only mode
                // that may change to allow external triggers
                // still debating (so it's here for now)
                bloom_mode = true;
                break;
            default:
                help_cmd_bind(1);
                return 1;
        }
    }

    // We need at least the label/mask and the group_id
    if (argc - optind < 2) {
        help_cmd_bind(1);
        return 1;
    }

    if (! bloom_mode) {
        return 1;
    }
    
    const char *label_target = argv[optind++];
    const char *group_target = argv[optind++];

    // resolve the Label/Mask (same logic from cmd_label)
    for (int i = 0; i < thisuser.label_count; i++) {
        if (!strcasecmp(label_target, thisuser.labels[i].name)) {
            mask = thisuser.labels[i].mask;
            found = true;
            break;
        }
    }

    if (!found) {
        char *endptr;
        mask = strtoull(label_target, &endptr, 0);
        if (*endptr != '\0' || mask == 0) {
            fprintf(stderr, "%s: unknown label or invalid hex mask '%s'\n", modname, label_target);
            return 1;
        }
    }

    // Resolve Group ID
    group_id = atoi(group_target);
    if (group_id < 0 || group_id >= 64) {
        fprintf(stderr, "%s: invalid signal group '%d' (must be 0-63)\n", modname, group_id);
        return 1;
    }

    // Now we can register the watch 
    // THIS API MAY CHANGE - It may be better to return the bitmask itself? Debating.
    if (splinter_watch_label_register(mask, (uint8_t)group_id) == 0) {
        printf("Binding applied: Label '%s' (0x%lx) -> Signal Group %d\n", 
               label_target, (unsigned long)mask, group_id);
        return 0;
    } else {
        fprintf(stderr, "%s: failed to register watch for mask 0x%lx (check store connection)\n", 
                modname, (unsigned long)mask);
        return 1;
    }
}
