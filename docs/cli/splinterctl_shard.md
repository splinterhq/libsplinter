---
title: "shard"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `shard` CLI User's Reference

The purpose of `shard` is to inspect and seed the Logic Shard bid table (cooperative memory advisement / election).

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `table` | Yes (one subcommand) | Pretty-print all 32 bid slots. |
| `who` | Yes (one subcommand) | Print the current sovereign and its intent. |
| `claim <id> <intent> <prio> <dur_ticks>` | Yes (one subcommand) | Register a bid. |
| `rebid <id> <intent> <prio> <dur_ticks>` | Yes (one subcommand) | Refresh an existing bid's window. |
| `release <id>` | Yes (one subcommand) | Release a bid. |
| `advise <id> <intent> [nowait]` | Yes (one subcommand) | Cooperative `madvise` on the whole arena. |

### Example Uses

**Console:**
```
splinter_debug # shard who
```

**Shell:**
```
$ splinterctl shard table
```

### Additional Information And Rationale

**Additional Info (Or None):**
`intent` is one of `willneed`, `sequential`, `random`, `dontneed`. Ids are non-zero hex or decimal (e.g. `0x5F1A` or `24346`). Because the CLI is a single short-lived process, `claim`/`release` in one invocation do NOT persist across invocations (the bid is released when the process exits). `table` and `who` are the everyday commands.

**Rationale (Or None):**
`shard` is primarily an inspection/diagnostic surface plus a way to seed bids for testing.

### See Also

**Related Commands (Or None):**
[config](splinterctl_config.md)
