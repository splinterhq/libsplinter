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

// make terminal non-blocking so ctrl-] works
void setup_terminal(void) {
    struct termios temp_tio;
    
    temp_tio = thisuser.term;
    
    // disable canonical mode and echo
    temp_tio.c_lflag &= ~(ICANON | ECHO);
    temp_tio.c_cc[VMIN] = 0;   // Non-blocking read
    temp_tio.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSANOW, &temp_tio);
    
    // stdin must be non-blocking
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    return;
}

void restore_terminal(void) {
    // revert to original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &thisuser.term);
    
    // revert stdin to blocking again
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    
    // flush pending input
    tcflush(STDIN_FILENO, TCIFLUSH);
}

void help_cmd_watch(unsigned int level) {
    (void) level;

    printf("Usage: %s <key_name_to_watch> [--oneshot]\n", modname);
    printf("       %s --group <signal_group_id> [--oneshot]\n", modname);
    printf("%s watches a single key or a signal group in the current store for changes.\n", modname);
    puts("If --oneshot is specified, watch will exit after one event.");
    puts("\nPressing CTRL-] will terminate any waiting watches in this terminal.\n");
    
    return;
}

static const struct option long_options[] = {
    { "help", optional_argument, NULL, 'h' },
    { "oneshot", no_argument, NULL, 'o' },
    { "group", required_argument, NULL, 'g' },
    {NULL, 0, NULL, 0}
};

static const char *optstring = "h:og:";

int cmd_watch(int argc, char *argv[]) {
    size_t msg_sz = 0;
    char c, msg[4096], key[SPLINTER_KEY_MAX] = { 0 };
    char *tmp = getenv("SPLINTER_NS_PREFIX");
    int rc = -1, opt = 0, watch_group = -1;
    unsigned int oneshot = 0;

    optind = 0;

    while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                help_cmd_watch(1);
                return 0;
            case 'o':
                oneshot = 1;
                break;
            case 'g':
                watch_group = cli_safer_atoi(optarg);
                if (watch_group < 0 || watch_group >= SPLINTER_MAX_GROUPS) {
                    fprintf(stderr, "%s: invalid group. Must be 0-%d\n", modname, SPLINTER_MAX_GROUPS - 1);
                    return -1;
                }
                break;
            case '?':
                help_cmd_watch(1);
                return -1;
            default:
                fprintf(stderr, "%s: unknown argument '%c'\n", modname, opt);
                break;
        }
    }

    if (optind < argc)
        snprintf(key, sizeof(key) -1, "%s%s", tmp == NULL ? "" : tmp, argv[optind++]);

    if (! key[0] && watch_group == -1) {
        fprintf(stderr, "Usage: %s <key> [--oneshot] OR %s --group <id> [--oneshot]\nTry 'help watch' for help.\n", modname, modname);
        return -1;
    }

    setup_terminal();
    
    if (watch_group >= 0) {
        // signal group poll loop
        uint64_t last_count = splinter_get_signal_count((uint8_t)watch_group);
        while (! thisuser.abort) {
            if (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == 29) {  // Ctrl-] is ASCII 29
                    tcflush(STDERR_FILENO, TCIFLUSH);
                    thisuser.abort = 1;
                    break;
                }
            }
            
            uint64_t current_count = splinter_get_signal_count((uint8_t)watch_group);
            if (current_count != last_count) {
                fprintf(stdout, "Signal group %d pulsed! (Total pulses: %lu)\n", watch_group, current_count);
                fflush(stdout);
                last_count = current_count;
                if (oneshot) {
                    thisuser.abort = 1;
                }
            } else {
                usleep(50000); // 50ms polling interval
            }
        }
    } else {
        // hyper-focused poll loop
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
    }

    puts(""); // GET ends with one blank line, so we emulate that here as well.
    thisuser.abort = 0;
    restore_terminal();
    
    return 0;
}