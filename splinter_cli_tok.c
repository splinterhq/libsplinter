/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_tok.c
 * @brief Splinter CLI input tokenizers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "splinter_cli.h"

/**
 * Dealing with potentially-quoted strings in C is always a pain. 
 * 
 * strtok() clobbers strings in-place, which is .. not ideal with dealing with
 * potentially quoted SET commands, etc. So, a custom "unrolling" helper
 * that deals with simple quotes.
 * 
 * This is meant to deal with things like:
 *     > set foo_key "fooo \"bar baz boo\" bar foo foobar"
 * It does NOT deal with single or nested quotes. 
 */
char **cli_unroll_argv(const char *input, int *out_argc) {
    int cap = 8, argc = 0, i;
    char **argv;

    if (!input) return NULL;
    const char *p = input;
    
    argv = malloc(cap * sizeof(char*));
    if (!argv) return NULL;

    while (*p) {
        /* skip whitespace */
        while (isspace((unsigned char)*p)) ++p;
        if (!*p) break;

        /* decide whether token is quoted */
        char *token = NULL;
        size_t tlen = 0;
        if (*p == '"') {
            ++p; /* skip opening quote */
            const char *start = p;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) p += 2; /* allow escaped chars inside quotes */
                else ++p;
            }
            tlen = p - start;
            token = malloc(tlen + 1);
            if (!token) goto fail;
            /* simple copy handling backslash escapes for \" and \\ */
            const char *q = start;
            char *w = token;
            while (q < p) {
                // advance past escape
                if (*q == '\\' && (q + 1) < p) ++q;
                // now just a simple copy
                *w++ = *q++;
            }
            *w = '\0';
            if (*p == '"') ++p; /* skip closing quote */
        } else {
            const char *start = p;
            while (*p && !isspace((unsigned char)*p)) {
                ++p;
            }
            tlen = p - start;
            token = malloc(tlen + 1);
            if (!token) goto fail;
            memcpy(token, start, tlen);
            token[tlen] = '\0';
        }

        if (argc >= cap) {
            cap *= 2;
            char **tmp = realloc(argv, cap * sizeof(char*));
            if (!tmp) { free(token); goto fail; }
            argv = tmp;
        }
        argv[argc++] = token;
    }

    /* NULL-terminate */
    char **tmp = realloc(argv, (argc + 1) * sizeof(char*));
    if (!tmp) goto fail;
    argv = tmp;
    argv[argc] = NULL;
    *out_argc = argc;
    return argv;

fail:
    if (argv) {
        for (i = 0; i < argc; ++i) free(argv[i]);
        free(argv);
    }
    return NULL;
}

/**
 * Slice the last n elements of an array (usually args) into
 * a new array beginning at the given offset as [0].
 */
char **cli_slice_args(char *const src[], int start_index, int count) {
    if (!src) return NULL;

    char **dst = malloc((count + 1) * sizeof(char *));
    if (!dst) return NULL;

    for (int i = 0; i < count; ++i) {
        dst[i] = src[start_index + i];
    }
    dst[count] = NULL;

    return dst;
}

/**
 * This is a helper that tries to re-construct a command line as it was before
 * the calling shell stripped quotes as it broke down arguments into the argument
 * array. It's not a perfect exact science, but it lets us (usually) keep interactive
 * and non-interactive history the same.
 */
char *cli_rejoin_args(char *const src[]) {
    char *empty = malloc(1);

    if (empty == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    empty[0] = '\0';

    if (!src) return empty;

    size_t total = 0, count = 0, i;

    // First pass: count items and calculate total size needed
    for (i = 0; src[i] != NULL; ++i) {
        size_t item_len = strlen(src[i]);
        int has_spaces = (strchr(src[i], ' ') != NULL);
        int quote_count = 0;
        
        // Count existing quotes that need escaping
        const char *p_scan = src[i];
        while ((p_scan = strchr(p_scan, '"')) != NULL) {
            quote_count++;
            p_scan++;
        }
        
        // Add base item length plus escapes for existing quotes
        total += item_len + quote_count;
        
        // If item has spaces, add 2 for wrapping quotes
        if (has_spaces) {
            total += 2;
        }
        
        ++count;
    }

    if (count == 0) return empty;

    free(empty);

    // spaces between items = count - 1 
    total += (count > 1) ? (count - 1) : 0;
    total += 1; // terminating NULL

    char *out = malloc(total);
    if (!out) return NULL;

    char *p = out;
    for (i = 0; i < count; ++i) {
        const char *item = src[i];
        int has_spaces = (strchr(item, ' ') != NULL);
        
        // Add opening quote if item has spaces
        if (has_spaces) {
            *p++ = '"';
        }
        
        // Copy item character by character, escaping quotes
        while (*item) {
            if (*item == '"') {
                *p++ = '\\';  // Add escape character
            }
            *p++ = *item++;
        }
        
        // Add closing quote if item has spaces
        if (has_spaces) {
            *p++ = '"';
        }
        
        // Add space separator if not the last item
        if (i + 1 < count) {
            *p++ = ' ';
        }
    }

    *p = '\0';
    
    return out;
}

/**
 * Helper to free an allocated argument stack
 */
void cli_free_argv(char *argv[]) {
    int i;

    if (!argv) return;
    for (i = 0; argv[i]; ++i) free(argv[i]);
    free(argv);
}
