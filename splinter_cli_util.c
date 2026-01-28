/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_util.c
 * @brief Splinter CLI utilties for working with modules
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "splinter_cli.h"

/**
 * Returns the index of the module in the modules array given its 
 * name, or -1 if not found.
 */
int cli_find_module(const char *name) {
    unsigned int i;
    
    for (i = 0; command_modules[i].name != NULL; i++) {
        if (! strncmp(name, command_modules[i].name, command_modules[i].name_len))
            return command_modules[i].id;
    }

    errno = EINVAL;

    return -1;
}

/**
 * See if the module at index idx is an alias by returning the index
 * of the module its aliased to, or -1 if not an alias. 
 */
int cli_find_alias(int idx) {
    if (! command_modules[idx].name) {
        errno = EINVAL;
        return -1;
    }
    return command_modules[idx].alias_of;
}

/**
 * Run the module at the specified index by its defined entry point and 
 * proxy its return value. Sets errno if idx is invalid.
 */
int cli_run_module(int idx, int argc, char *argv[]) {
    if (! command_modules[idx].name || ! command_modules[idx].entry) {
        errno = EINVAL;
        return -1;
    }
    return command_modules[idx].entry(argc, argv);
}

/**
 * Show a modules help (to the specified level) by index.
 */
void cli_show_module_help(int idx, unsigned int level) {
    if (! command_modules[idx].name || ! command_modules[idx].help) {
        errno = EINVAL;
        return;
    }
    command_modules[idx].help(level);
    return;
}

/**
 * A simple way to list modules
 */
#define LIST_BAR "--------------------"
void cli_show_modules(void) {
    int i;

    fprintf(stdout, "\n %-10s | %-60s\n%s%s%s%s\n",
        "Module", 
        "Description",
        LIST_BAR,
        LIST_BAR,
        LIST_BAR,
        LIST_BAR);

    for (i = 0; command_modules[i].name; i++) {
        fprintf(stdout, " %-10s | %-60s\n",
            command_modules[i].name,
            command_modules[i].description);
    }
    
    return;
}

char * cli_show_key_type(unsigned short flags) {
    if (flags & SPL_SLOT_TYPE_VOID) return "SPL_SLOT_TYPE_VOID";
    if (flags & SPL_SLOT_TYPE_BIGINT) return "SPL_SLOT_TYPE_BIGINT";
    if (flags & SPL_SLOT_TYPE_BIGUINT) return "SPL_SLOT_TYPE_BIGUINT";
    if (flags & SPL_SLOT_TYPE_BINARY) return "SPL_SLOT_TYPE_BINARY";
    if (flags & SPL_SLOT_TYPE_IMGDATA) return "SPL_SLOT_TYPE_IMGDATA";
    if (flags & SPL_SLOT_TYPE_VARTEXT) return "SPL_SLOT_TYPE_VARTEXT";
    if (flags & SPL_SLOT_TYPE_AUDIO) return "SPL_SLOT_TYPE_AUDIO";
    if (flags & SPL_SLOT_TYPE_JSON) return "SPL_SLOT_TYPE_JSON";

    return "SPL_SLOT_TYPE_VOID";
}

void cli_show_key_config(const char *key, const char *caller) {
    splinter_slot_snapshot_t snap = { 0 };

    splinter_get_slot_snapshot(key, &snap);
    if (snap.epoch == 0) {
        fprintf(stderr, "%s: invalid key: %s\n", caller == NULL ? "Splinter; " : caller, key);
        return;
    }

    printf("hash:     %lu\n", snap.hash);
    printf("epoch:    %lu\n", snap.epoch);
    printf("val_off:  %u\n", snap.val_off);
    printf("val_len:  %u\n", snap.val_len);
    printf("ctime:    %lu\n", snap.ctime);
    printf("atime:    %lu\n", snap.atime);
    printf("type:     %s\n", cli_show_key_type(snap.type_flag));
    printf("key:      %s\n", snap.key);
    puts("");

    return;
}

// halts execution if strtol would overflow an integer.
int cli_safer_atoi(const char *string) {

    char *buff;
    long l;

    l = strtol(string, &buff, 10);
    if (l <= INT_MAX) {
        return (int) l;
    } else {
        fprintf(stderr, "Value or argument would overflow an integer. Exiting.\n");
        exit(EXIT_FAILURE);
    }
}
