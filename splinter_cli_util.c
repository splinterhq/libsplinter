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

/**
 * See if a key is eligible to be printed out to the console without
 * being serialized (and without creating a problem)
 */
unsigned int cli_key_is_printable_unserialized(unsigned short flags) {
    switch(flags) {
        case SPL_SLOT_TYPE_BIGINT:
        case SPL_SLOT_TYPE_BIGUINT:
        case SPL_SLOT_TYPE_VARTEXT:
            return 1;
        default:
            return  0;
    }
}

/**
 * Take a given flag, and return a statically-allocated string representing
 * its symbol.
 */
char * cli_show_key_type(unsigned short flags) {
    if ((flags & SPL_SLOT_TYPE_BIGINT)  != 0) return "SPL_SLOT_TYPE_BIGINT";
    if ((flags & SPL_SLOT_TYPE_BIGUINT) != 0) return "SPL_SLOT_TYPE_BIGUINT";
    if ((flags & SPL_SLOT_TYPE_BINARY)  != 0) return "SPL_SLOT_TYPE_BINARY";
    if ((flags & SPL_SLOT_TYPE_IMGDATA) != 0) return "SPL_SLOT_TYPE_IMGDATA";
    if ((flags & SPL_SLOT_TYPE_VARTEXT) != 0) return "SPL_SLOT_TYPE_VARTEXT";
    if ((flags & SPL_SLOT_TYPE_AUDIO)   != 0) return "SPL_SLOT_TYPE_AUDIO";
    if ((flags & SPL_SLOT_TYPE_JSON)    != 0) return "SPL_SLOT_TYPE_JSON";
    if ((flags & SPL_SLOT_TYPE_VOID)    != 0) return "SPL_SLOT_TYPE_VOID";

    return "UNNAMED";
}

/**
 * Take a string representing a type bitmask, and turn it into the appropriate
 * bitmask.
 */
uint16_t cli_type_to_bitmask(const char *type) {
    if (!strncmp(type, "bigint", 6)) return SPL_SLOT_TYPE_BIGINT;
    if (!strncmp(type, "biguint", 7)) return SPL_SLOT_TYPE_BIGUINT; 
    if (!strncmp(type, "binary", 6)) return SPL_SLOT_TYPE_BINARY;
    if (!strncmp(type, "imgdata", 7)) return SPL_SLOT_TYPE_IMGDATA;
    if (!strncmp(type, "vartext", 7)) return SPL_SLOT_TYPE_VARTEXT;
    if (!strncmp(type, "audio", 5)) return SPL_SLOT_TYPE_AUDIO;
    if (!strncmp(type, "json", 4)) return SPL_SLOT_TYPE_JSON;
    if (!strncmp(type, "void", 4)) return SPL_SLOT_TYPE_VOID;

    return 0; // not found
}

/**
 * Dump a key's configuration.
 */
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

/**
 * Don't let atoi() ruin our day without having to call strtol each time.
 */
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
