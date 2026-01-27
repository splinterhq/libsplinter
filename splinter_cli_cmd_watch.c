/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_watch.c
 * @brief Implements the CLI 'watch' command.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>

#include "splinter_cli.h"
#include "splinter.h"

static const char *modname = "watch";

// make terminal non-blocking
void setup_terminal(void) {
    struct termios temp_tio;
    
    temp_tio = thisuser.term;
    
    // Disable canonical mode and echo
    temp_tio.c_lflag &= ~(ICANON | ECHO);
    temp_tio.c_cc[VMIN] = 0;   // Non-blocking read
    temp_tio.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSANOW, &temp_tio);
    
    // Make stdin non-blocking
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    return;
}

// restore terminal to (whatever it was before)
void restore_terminal(void) {
    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &thisuser.term);
    
    // Make stdin blocking again
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    
    // Flush any pending input
    tcflush(STDIN_FILENO, TCIFLUSH);
}

void help_cmd_watch(unsigned int level) {
    (void) level;

    printf("Usage: %s <key_name_to_watch> [--oneshot]\n", modname);
    printf("%s watches a single key in the current store for changes.\n", modname);
    puts("If --oneshot is specified, watch will exit after one event.");
    puts("\nMulti-key watches are coming in Splinter 0.9.4, anticipated May 2026.");
    puts("\nPressing CTRL-] will terminate a waiting watch.\n");
    
    return;
}

static const struct option long_options[] = {
    { "help", optional_argument, NULL, 'h' },
    { "oneshot", no_argument, NULL, 'o' },
    {NULL, 0, NULL, 0}
};

static const char *optstring = "h:o";

int cmd_watch(int argc, char *argv[]) {
    size_t msg_sz = 0;
    char c, msg[4096], key[SPLINTER_KEY_MAX] = { 0 };
    char *tmp = getenv("SPLINTER_NS_PREFIX");
    int rc = -1, opt = 0;
    unsigned int oneshot = 0;

    optind = 0;

    while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                help_cmd_watch(1);
                break;
            case 'o':
                oneshot = 1;
                break;
            case '?':
                help_cmd_watch(1);
                break;
            default:
                fprintf(stderr, "%s: unknown argument '%c'\n", modname, opt);
                break;
        }
    }

    if (optind < argc)
        snprintf(key, sizeof(key) -1, "%s%s", tmp == NULL ? "" : tmp, argv[optind++]);

    if (! key[0]) {
        fprintf(stderr, "Usage: %s <key> [--oneshot]\nTry 'help ext watch' for help.\n", modname);
        return -1;
    }

    setup_terminal();
    
    while (! thisuser.abort) {
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 29) {  // Ctrl-] is ASCII 29
                tcflush(STDERR_FILENO, TCIFLUSH);
                thisuser.abort = 1;
                break;
            }
        }
        
        rc = splinter_poll(key, 100);
        if (rc == -1) {
            fprintf(stderr, "%s: invalid key: '%s'\n", modname, key);
            restore_terminal();
            return -1;
        }

        if (rc == 0) {
            if (splinter_get(key, msg, sizeof(msg), &msg_sz) != 0) {
                fprintf(stderr,
                        "splinter_logtee: failed to read key %s after update.\n",
                        key);
                return -1;
            }
            fprintf(stdout, "%lu:", msg_sz);
            fwrite(msg, 1, msg_sz, stdout);
            fputc('\n', stdout);
            fflush(stdout);
            if (oneshot) {
                // just raise it on behalf of the user since they specified it
                thisuser.abort = 1;
            }
        }
    }

    puts(""); // GET ends with one blank line, so we emulate that here as well.
    thisuser.abort = 0;
    restore_terminal();
    
    return 0;
}
