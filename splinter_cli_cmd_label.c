/**
 * @file splinter_cli_cmd_label.c
 * @brief Implements the CLI 'label' command to tag keys via Bloom filter.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "splinter_cli.h"

static const char *modname = "label";

void help_cmd_label(unsigned int level) {
    (void) level;
    printf("Usage: %s <key> <label_name>\n", modname);
    printf("Labels are defined in ~/.splinterrc and apply to the 64-bit Bloom filter.\n");
    puts("");
}

int cmd_label(int argc, char *argv[]) {
    if (argc < 3) {
        help_cmd_label(1);
        return 1;
    }

    const char *key = argv[1];
    const char *label_name = argv[2];
    uint64_t mask = 0;
    bool found = false;

    // 1. Resolve Label from .splinterrc
    for (int i = 0; i < thisuser.label_count; i++) {
        if (!strcasecmp(label_name, thisuser.labels[i].name)) {
            mask = thisuser.labels[i].mask;
            found = true;
            break;
        }
    }

    if (!found) {
        // Fallback to hex parsing if not a named label
        char *endptr;
        mask = strtoull(label_name, &endptr, 0);
        if (*endptr != '\0' || mask == 0) {
            fprintf(stderr, "%s: unknown label or invalid mask '%s'\n", modname, label_name);
            return 1;
        }
    }

    // 2. Execute the atomic label update
    if (splinter_set_label(key, mask) == 0) {
        printf("Label '%s' (0x%lx) applied to '%s'.\n", label_name, (unsigned long)mask, key);
        return 0;
    } else {
        fprintf(stderr, "%s: failed to apply label to '%s' (errno: %d)\n", modname, key, errno);
        return 1;
    }
}