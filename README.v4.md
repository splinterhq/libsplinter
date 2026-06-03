# Splinter Format v4 — Logic Shard Election & Voluntary Yield

Format v4 adds the **Logic Shard bid table** to the store header: a fixed,
32-slot region that lets independently-loaded shards coordinate *memory intent*
(`posix_madvise` advice) cooperatively, with no central arbiter, no syscall on
the hot path, and no dynamic allocation. This document covers what changed on
disk, the new public API, and the `shard` CLI command.

---

## The v3 → v4 format break

The bid table is appended to `struct splinter_header`, which changes the
on-shared-memory layout and `sizeof(struct splinter_header)`. **v4 is not
backward compatible with v3 stores.**

- `splinter_open()` rejects a v3 store cleanly: it checks `H->version` against
  `SPLINTER_VER` and returns `-1` rather than mis-mapping the older layout.
- There is **no in-place upgrade**. A v3 store must be drained and re-created as
  v4 (the value data is the operator's; Splinter does not migrate it for you).
  In practice: stop the writers, dump anything you need with `splinter export`,
  `make distclean` / `rm` the old shm object, re-`init`, and re-ingest.
- All field offsets *before* the table are undisturbed — the table is the last
  member of the header — but because `sizeof(header)` grew, the slot array and
  value arena now start at a different absolute offset. That is exactly why the
  version gate exists.

If `splinter_open()` starts returning `-1` on a store that used to open, check
its version first; a v3 store is the most likely cause.

---

## What the protocol does

The thesis problem: many processes issuing independent `posix_madvise()` hints
for the *same* region (live inference wanting `WILLNEED`, a backfill sweep
wanting `SEQUENTIAL`, a purge wanting `DONTNEED`) forces the kernel page-cache
manager to thrash and destroys L3 residency.

The fix is **memory-intent coordination only** (CPU scheduling stays with
Linux/cgroups). Each shard declares a *bid*; an election picks one **sovereign**
allowed to issue the current advisement.

- **Bid table.** 32 slots (`SPLINTER_MAX_SHARDS`), each a `splinter_shard_bid`:
  `shard_id` (0 = empty), `pid`, `intent`, `priority` (0–255), `duration_tsc`,
  `claimed_at`. Claim via CAS on `shard_id`; release on unload/yield. ~1 KB.
- **The election** (`splinter_shard_election`) is a read-only O(32) scan:
  highest-priority **unexpired** bid wins. A bid is expired when
  `splinter_now() − claimed_at > duration_tsc`. Ties broken by earliest
  `claimed_at`, final tie-break **lowest PID**. Every shard computes the same
  sovereign independently from static bid data.
- **Voluntary yield** (`splinter_madvise`) runs the election; if the caller is
  sovereign it issues the advice, else it blocks (eventfd broker when armed,
  else a TSC-polled sleep) until it can win — or defers immediately.
- **32-shard ceiling** is deliberate: it matches the seqlock writer ceiling.
  The table is never larger than the concurrency the epoch protocol supports.

### The `DONTNEED` soft bumper

`DONTNEED` is hostile in an active deployment. A `DONTNEED` bid participates in
the election normally but **cannot win sovereignty while any unexpired
`WILLNEED`/`SEQUENTIAL` bid exists**, regardless of its priority. It still
occupies a slot and is visible in audits. This ships on by default. It is a
*safety* mechanism, not access control — there is no authentication on the
table; the OS trust boundary (who can open the bus) is who can bid. Treat
`DONTNEED` as maintenance-window-only.

---

## Public API (`splinter.h`)

All functions are `extern "C"`-safe (the `.cpp` daemons call them). Return
convention matches the rest of the library: `0` success, `-1`
recoverable/contested (`errno` set — `EAGAIN`/`ENOSPC`/`ETIMEDOUT`), `-2` caller
error (NULL/invalid arg or no store).

```c
int      splinter_shard_claim(uint32_t shard_id, uint8_t intent,
                              uint8_t priority, uint64_t duration_tsc);
int      splinter_shard_claim_ex(uint32_t shard_id, uint32_t pid, uint8_t intent,
                                 uint8_t priority, uint64_t duration_tsc,
                                 uint64_t claimed_at);
int      splinter_shard_rebid(uint32_t shard_id, uint8_t intent,
                              uint8_t priority, uint64_t duration_tsc);
int      splinter_shard_release(uint32_t shard_id);
uint32_t splinter_shard_election(uint8_t *out_intent);
int      splinter_shard_is_sovereign(uint32_t shard_id);
int      splinter_shard_table_snapshot(struct splinter_shard_bid_snapshot *out, size_t max);
int      splinter_madvise(uint32_t shard_id, void *addr, size_t len,
                          int advice, uint64_t timeout_ticks);
```

Intent classes (`splinter_intent_t`): `SPL_INTENT_NONE`, `_WILLNEED`,
`_SEQUENTIAL`, `_RANDOM`, `_DONTNEED`. They map to `POSIX_MADV_*` advice.

`splinter_madvise()` `timeout_ticks` sentinels mirror `splinter_event_bus_wait`:
`0` = non-blocking/defer (returns `-1`/`EAGAIN` if not sovereign),
`UINT64_MAX` = wait forever, anything else = bounded wait (`ETIMEDOUT` on
expiry). When `addr == NULL` it advises the whole value arena. The range is
page-aligned for you before the kernel call.

### Recommended usage patterns

- **Latency-sensitive callers** (live completion): declare `WILLNEED` at high
  priority with a **short** window and **re-bid frequently** rather than holding
  sovereignty across a long pass. Advise **non-blocking** — never block a live
  path on a hint.
- **Backfill / sweeps**: declare `SEQUENTIAL` at low priority, advise
  non-blocking — if a live completer is sovereign, just proceed; deferring is
  correct, backfill is latency-tolerant.
- The reference daemons already do this: `splainference` (completion) claims
  shard `0x5F1A` at priority 200; `splinference` (embeddings) claims `0x5F10` at
  priority 40 (live) / 20 (backfill).

---

## The `shard` CLI command

`shard` is the human face of the bid table — the "auditable at the
infrastructure level" surface. Intent is *declared, recorded, and observable
before it is acted on*.

```
shard table                                 # pretty-print all 32 bid slots
shard who                                   # current sovereign shard_id + intent
shard claim   <id> <intent> <prio> <dur>    # intent: willneed|sequential|random|dontneed
shard rebid   <id> <intent> <prio> <dur>
shard release <id>
shard advise  <id> <intent> [nowait]        # cooperative madvise over the whole arena
```

`shard table` columns: slot, shard_id, pid, intent, priority, claimed_at,
duration, expired?, sovereign?. `claimed_at` plus actual release timing is the
raw data a future over-committer feedback loop would consume.

> **Caveat:** the CLI is a single short-lived process, so `claim`/`release` in
> one invocation do **not** persist across invocations — the bid is released
> when the process exits. `shard` is primarily an inspection/diagnostic surface
> plus a way to seed bids for testing. `shard table` and `shard who` are the
> everyday commands.

Example:

```
$ splinterctl
> use mystore
> shard claim 0x5F1A willneed 200 1152921504606846976
> shard claim 0x5F10 sequential 20 1152921504606846976
> shard who
sovereign=0x5f1a intent=willneed
> shard advise 0x5F10 willneed nowait
advise 0x5f10 deferred/failed (rc=-1, errno=Resource temporarily unavailable)
```

The deferral is the protocol working: `0x5F10` is not sovereign, and a
non-blocking advise correctly declines to fight the `WILLNEED` holder.

---

## Notes for operators

- **No authentication, by design** (thesis "What Splinter Does Not Do"): any
  process that can open the bus can bid. Enforce isolation at the
  namespace/cgroup level.
- **Bid operations never bump `H->epoch`.** They are scheduling coordination
  metadata, not data writes, so watchers are not perturbed by claims/releases.
- **Out-of-scope (recorded, not yet enforced):** the over-committer penalty/
  feedback loop. `claimed_at` and release timing are recorded so the data is
  available; policy is future work.
