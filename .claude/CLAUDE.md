# Project memory: Splinter

This project ships with a local Splinter memory bus at `.claude/memory.spl`.
It is a lock-free shared-memory store with semantic-search backed by a
locally-running Nomic embedding model (the `claudevisor` user service).

You — the agent — should treat this as durable cross-run memory. Plain
files in `~/.claude/.../memory/` give you recall by exact name; this gives
you recall by **meaning**. Searching "the FFI gotcha I hit with BigInt"
returns the journal entry whether or not it has those exact words.

## Bootstrap (run once per project)

```
spl-bootstrap                # creates .claude/memory.spl if missing
spl-status                   # confirms claudevisor is active
```

If `claudevisor` is inactive, embeddings will not happen and search will
return empty. Tell the user; do not silently work without recall.

## When to use it

### Before non-trivial work — recall first

```
spl-recall "<topic>"
```

This is a first-class step, not an afterthought. The journal may already
contain the answer to the question you are about to investigate. Run it
before starting any task that involves a decision, a debugging session,
an API you have not used in this project before, or a piece of code you
think you may have touched in a previous run.

### During work — journal at decision points

```
spl-journal "auth-middleware-import"   "Trying X because Y."     0x10
spl-journal "auth-middleware-import.1" "Worked because Z."       0x10
```

The base key holds intent; `.1`, `.2`, … hold outcomes via tandem orders.
The script labels both as journal entries automatically; you only pass an
optional category bloom (see table below) to make later searches more
precise.

Journal:
- before any approach you considered and rejected
- when a command fails and you try a different one
- when you discover something surprising about the environment
- when a decision is not obvious from the spec

Do NOT journal routine reads/writes that succeeded as expected.

### For large files — ingest, then search

For files you will revisit across turns (logs ≥300 lines, source files
≥500 lines, transcripts, design docs):

```
spl-ingest path/to/big_log.txt
# ... do other work for a few seconds ...
spl-search "connection reset during shard rebalance" --scope ingest
```

This replaces the pattern of repeated `Read` + `grep` against the same
file — local nomic inference + cosine match is dramatically cheaper than
re-reading 5000 lines into your context every turn.

### For telemetry — bump signal slots

```
spl-signal task_start
spl-signal task_advance
spl-signal task_finish
spl-signal error
spl-signal checkpoint
```

These bump signal groups so the user can observe progress externally.

## The contemporaneous rule

Splinference embeds writes **asynchronously**, out of process. A write
followed immediately by a search in the same shell command may miss — the
embedding has not landed yet. In practice:

| When you write | When it is searchable |
|---|---|
| Same shell command | Maybe (race) — do not rely on it |
| Next tool call | Yes (typically <1s) |
| Next turn | Always |
| Next run | Always |

The point of the bus is **between** units of work, not within a single
write+read. Plan accordingly: write now, search next turn.

## Bloom label categories (optional third arg to spl-journal)

| Mask | Category |
|---|---|
| `0x1`  | build-related |
| `0x2`  | runtime-related |
| `0x4`  | test-related |
| `0x8`  | dependency-related |
| `0x10` | logic-related |
| `0x20` | security-related |
| `0x40` | conflict-related |
| `0x80` | exception-related |

`spl-journal` always applies `0x100` (journal kind) plus `0x1000` (intent)
or `0x2000` (outcome) automatically based on the key suffix. Your category
is OR'd on top.

## IPC between agents

When you spawn subagents that run in parallel, they can leave structured
breadcrumbs on the bus instead of returning everything in one giant string:

```
spl-journal "ipc/<from>/<to>/<topic>" "message body"
```

Read them back with `spl-search "<topic>" --scope ipc`. Useful for
hand-offs between research agents and the synthesizing parent.

## Quick reference

| Script | Purpose |
|---|---|
| `spl-bootstrap` | Idempotent setup |
| `spl-status` | Health check |
| `spl-recall <q>` | Journal-only semantic search |
| `spl-search <q> [--scope]` | Scoped semantic search across all kinds |
| `spl-ingest <file>` | Chunk and embed a file for later searching |
| `spl-journal <key> <text>` | Write intent / outcome |
| `spl-signal <event>` | Bump a signal slot for telemetry |
