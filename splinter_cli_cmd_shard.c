/**
 * Copyright 2025 Tim Post
 * License: Apache 2 (MIT available upon request to timthepost@protonmail.com)
 *
 * @file splinter_cli_cmd_shard.c
 * @brief Implements the CLI 'shard' command: inspect and seed the Logic Shard
 *        bid table (cooperative memory advisement / election).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include "splinter_cli.h"

static const char *modname = "shard";

/* Map an intent name <-> splinter_intent_t. */
static int shard_intent_from_name(const char *name) {
    if (strcmp(name, "willneed")   == 0) return SPL_INTENT_WILLNEED;
    if (strcmp(name, "sequential") == 0) return SPL_INTENT_SEQUENTIAL;
    if (strcmp(name, "random")     == 0) return SPL_INTENT_RANDOM;
    if (strcmp(name, "dontneed")   == 0) return SPL_INTENT_DONTNEED;
    if (strcmp(name, "none")       == 0) return SPL_INTENT_NONE;
    return -1;
}

static const char *shard_intent_name(uint8_t intent) {
    switch (intent) {
        case SPL_INTENT_WILLNEED:   return "willneed";
        case SPL_INTENT_SEQUENTIAL: return "sequential";
        case SPL_INTENT_RANDOM:     return "random";
        case SPL_INTENT_DONTNEED:   return "dontneed";
        case SPL_INTENT_NONE:       return "none";
        default:                    return "?";
    }
}

/* Translate a declared intent into the raw POSIX advice for `shard advise`. */
static int shard_advice_for(uint8_t intent) {
    switch (intent) {
        case SPL_INTENT_WILLNEED:   return POSIX_MADV_WILLNEED;
        case SPL_INTENT_SEQUENTIAL: return POSIX_MADV_SEQUENTIAL;
        case SPL_INTENT_RANDOM:     return POSIX_MADV_RANDOM;
        case SPL_INTENT_DONTNEED:   return POSIX_MADV_DONTNEED;
        case SPL_INTENT_NONE:
        default:                    return POSIX_MADV_NORMAL;
    }
}

void help_cmd_shard(unsigned int level) {
    (void) level;
    printf("%s inspects and seeds the Logic Shard bid table (cooperative\n", modname);
    printf("memory advisement / election).\n");
    printf("Usage:\n");
    printf("  %s table                         pretty-print all 32 bid slots\n", modname);
    printf("  %s who                           print current sovereign + intent\n", modname);
    printf("  %s claim   <id> <intent> <prio> <dur_ticks>\n", modname);
    printf("  %s rebid   <id> <intent> <prio> <dur_ticks>\n", modname);
    printf("  %s release <id>\n", modname);
    printf("  %s advise  <id> <intent> [nowait]   cooperative madvise on the whole arena\n", modname);
    printf("\n");
    printf("  intent: willneed | sequential | random | dontneed\n");
    printf("  ids are non-zero hex or decimal (e.g. 0x5F1A or 24346).\n");
    printf("\n");
    printf("NOTE: the CLI is a single short-lived process, so claim/release in one\n");
    printf("invocation do NOT persist across invocations (the bid is released when the\n");
    printf("process exits). 'shard' is primarily an inspection/diagnostic surface plus a\n");
    printf("way to seed bids for testing; 'table' and 'who' are the everyday commands.\n");
}

static int shard_cmd_table(void) {
    struct splinter_shard_bid_snapshot snap[SPLINTER_MAX_SHARDS] = {0};
    int n = splinter_shard_table_snapshot(snap, SPLINTER_MAX_SHARDS);
    if (n < 0) {
        fprintf(stderr, "%s: no store mapped.\n", modname);
        return -1;
    }

    printf("%-4s %-10s %-8s %-11s %-4s %-20s %-20s %-7s %-9s\n",
           "slot", "shard_id", "pid", "intent", "prio",
           "claimed_at", "duration", "expired", "sovereign");
    for (int b = 0; b < n; b++) {
        if (snap[b].shard_id == 0) continue;
        printf("%-4d 0x%-8x %-8u %-11s %-4u %-20llu %-20llu %-7s %-9s\n",
               b, snap[b].shard_id, snap[b].pid,
               shard_intent_name(snap[b].intent), snap[b].priority,
               (unsigned long long)snap[b].claimed_at,
               (unsigned long long)snap[b].duration_tsc,
               snap[b].expired ? "yes" : "no",
               snap[b].sovereign ? "yes" : "no");
    }
    return 0;
}

static int shard_cmd_who(void) {
    uint8_t intent = SPL_INTENT_NONE;
    uint32_t id = splinter_shard_election(&intent);
    if (id == 0) {
        printf("no current sovereign\n");
        return 0;
    }
    printf("sovereign=0x%x intent=%s\n", id, shard_intent_name(intent));
    return 0;
}

int cmd_shard(int argc, char *argv[]) {
    if (argc < 2) {
        help_cmd_shard(1);
        return 1;
    }

    if (!thisuser.store_conn) {
        fprintf(stderr, "%s: not connected to a store.\n", modname);
        return -1;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "table") == 0) {
        return shard_cmd_table();
    }

    if (strcmp(sub, "who") == 0) {
        return shard_cmd_who();
    }

    if (strcmp(sub, "claim") == 0 || strcmp(sub, "rebid") == 0) {
        if (argc < 6) {
            help_cmd_shard(1);
            return 1;
        }
        uint32_t id = (uint32_t)strtoul(argv[2], NULL, 0);
        int intent = shard_intent_from_name(argv[3]);
        if (id == 0 || intent < 0) {
            fprintf(stderr, "%s: bad id or intent.\n", modname);
            return 1;
        }
        uint8_t  prio = (uint8_t)strtoul(argv[4], NULL, 0);
        uint64_t dur  = (uint64_t)strtoull(argv[5], NULL, 0);

        int rc = (strcmp(sub, "claim") == 0)
            ? splinter_shard_claim(id, (uint8_t)intent, prio, dur)
            : splinter_shard_rebid(id, (uint8_t)intent, prio, dur);
        if (rc != 0) {
            fprintf(stderr, "%s %s failed (rc=%d, errno=%s)\n",
                    modname, sub, rc, strerror(errno));
            return 1;
        }
        printf("%s 0x%x %s prio=%u dur=%llu OK\n", sub, id,
               shard_intent_name((uint8_t)intent), prio, (unsigned long long)dur);
        return 0;
    }

    if (strcmp(sub, "release") == 0) {
        if (argc < 3) {
            help_cmd_shard(1);
            return 1;
        }
        uint32_t id = (uint32_t)strtoul(argv[2], NULL, 0);
        int rc = splinter_shard_release(id);
        if (rc != 0) {
            fprintf(stderr, "%s release failed (rc=%d): no such bid\n", modname, rc);
            return 1;
        }
        printf("release 0x%x OK\n", id);
        return 0;
    }

    if (strcmp(sub, "advise") == 0) {
        if (argc < 4) {
            help_cmd_shard(1);
            return 1;
        }
        uint32_t id = (uint32_t)strtoul(argv[2], NULL, 0);
        int intent = shard_intent_from_name(argv[3]);
        if (id == 0 || intent < 0) {
            fprintf(stderr, "%s: bad id or intent.\n", modname);
            return 1;
        }
        uint64_t timeout = UINT64_MAX;
        if (argc >= 5 && strcmp(argv[4], "nowait") == 0) timeout = 0;

        int rc = splinter_madvise(id, NULL, 0, shard_advice_for((uint8_t)intent), timeout);
        if (rc == 0) {
            printf("advise 0x%x %s issued (sovereign)\n", id, shard_intent_name((uint8_t)intent));
            return 0;
        }
        fprintf(stderr, "advise 0x%x deferred/failed (rc=%d, errno=%s)\n",
                id, rc, strerror(errno));
        return 1;
    }

    fprintf(stderr, "%s: unknown subcommand '%s'.\n", modname, sub);
    help_cmd_shard(1);
    return 1;
}
