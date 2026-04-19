# Spec: `splinter_recreate()` — In-Place Store Reset (Issue #14)

**Status:** Ready for implementation  
**Target release:** 1.2.0  
**Labels:** enhancement

---

## Complexity Assessment

### Scope — 2/5
Only two subsystems are touched: the core library (`splinter.h` / `splinter.c`) gains the new function, and the CLI gains a `recreate` command. The backing file, mmap region, and signal/event-bus infrastructure are reused without structural change. No CMake feature gates, no new build variants.

### Uncertainty — 2/5
The requirement is narrow and well-described by the owner. The one genuine ambiguity is whether the caller is allowed to change `max_val_sz` (not mentioned in the issue); the safe conservative answer is no — only `slots` may be adjusted within the existing arena. The `splinter_create()` init sequence is the authoritative reference for what "pristine" means.

### Risk — 3/5
Risk is moderate. `splinter_recreate()` is a destructive in-place operation: if a concurrent reader or writer holds a reference and the header is being rewritten, it will see a partially-reset store. The correct mitigation is to document the function as **single-owner / quiesced** — no concurrent accessors — and perform all header writes with `memory_order_relaxed` followed by a single `msync` + `mfence`, matching the behavior of `splinter_create()`. The event-bus `owner_fd` / `owner_pid` reset also requires care: the old eventfd must be closed before being cleared from the header so no dangling fd remains in `g_event_fd`.

### Effort — S (1–2 dev-days)
The implementation is largely a subset of `splinter_create()` with the `ftruncate` call removed and a slot-count bounds check added. New CLI command follows the same pattern as existing single-argument commands.

### Ambiguities that would block implementation
1. **`max_val_sz` change**: Can the caller request a different per-value size? If the total arena bytes stay fixed, a different `max_val_sz` would require re-computing `val_off` for every slot — permitted by the spec, but the issue doesn't mention it. **Recommendation:** disallow; accept only `slots` as the variable parameter, and derive `max_val_sz` from `total_arena / slots` at recreate time (or require it to stay the same and error otherwise).
2. **Live-store safety**: The issue implies single-process use on flash media. `splinter_recreate()` must not be called while other processes hold the store open. The API should document this contract but does not need to enforce it (no cross-process lock exists in Splinter's design).
3. **Event bus cleanup**: If `splinter_event_bus_init()` was called before `splinter_recreate()`, the old eventfd must be closed. Decide whether `splinter_recreate()` silently closes it or returns an error requiring the caller to close it first.

---

## Problem Statement

Splinter targets embedded and flash-backed deployments where file-system wear is a first-class concern. The current API provides `splinter_create()` to lay down a new arena and `splinter_close()` / `splinter_open()` to release and reattach. There is no way to atomically reset an existing store to a pristine state without re-issuing `ftruncate`, which causes the OS to reallocate or zero flash pages unnecessarily. On NAND flash, writes are the scarce resource; on rotating media, an `ftruncate`-triggered zeroing pass can take tens of seconds. Users who want to cycle a store through multiple experiments or reset it between service restarts currently have no wear-friendly option.

---

## Proposed Solution

Add `splinter_recreate(size_t new_slots)` to the public API. The function:

1. Verifies that `sizeof(splinter_header) + new_slots * sizeof(splinter_slot) + new_slots * max_val_sz` fits within the existing mapped region.
2. Closes and clears the event bus if it was initialized.
3. Re-initialises the header in place (same field writes as `splinter_create()`), preserving `magic`, `version`, and `val_sz`.
4. Re-initialises all `new_slots` slot descriptors, recalculating `val_off` uniformly.
5. Issues `msync(MS_SYNC)` once across the full region to flush to the backing medium in a single pass.
6. Does **not** call `ftruncate` — the file size on the medium is unchanged.

A `recreate <new_slots>` CLI command wraps this for interactive and scripted use.

---

## Functional Requirements

1. `splinter_recreate(size_t new_slots)` is a new public function declared in `splinter.h`.
2. It requires an already-open store (`splinter_open()` or `splinter_create()` must have been called); returns `-1` / `ENODEV` if no store is mapped.
3. The requested layout (`header + new_slots * (sizeof(slot) + max_val_sz)`) must fit within `H->val_sz`; if not, returns `-2` / `ENOSPC`.
4. `new_slots` must be ≥ 1; returns `-3` / `EINVAL` otherwise.
5. On success, all slots are zeroed/initialised identically to `splinter_create()`, including: `hash`, `epoch`, `ctime`, `atime`, `type_flag`, `user_flag`, `watcher_mask`, `bloom`, `val_off`, `val_len`, and `key`.
6. The header fields `magic`, `version`, and `val_sz` are preserved unchanged; all other header fields are reset (epoch → 1, flags → 0, event bus → uninitialised, bloom_watches → 0xFF, val_brk → 0).
7. `H->slots` is updated to `new_slots`.
8. If an event bus was previously initialised (`g_event_fd >= 0`), it is closed and cleared before the header is rewritten.
9. A single `msync(g_base, g_total_sz, MS_SYNC)` is issued after all writes; `splinter_recreate()` returns `msync`'s return value (0 on success, -1 on failure).
10. The function is documented as requiring exclusive access — concurrent readers and writers produce undefined behavior during the call.
11. The CLI exposes `recreate <new_slots>` as a command; it opens the named store, calls `splinter_recreate()`, and exits with the return code.

---

## Out of Scope

- Changing `max_val_sz` at recreate time — the per-slot value size is derived from the existing arena geometry.
- Cross-process quiescing / advisory locking — callers are responsible for ensuring no other process holds the store.
- Partial reset (reset a subset of slots) — a distinct feature; not needed for the flash-wear use case.
- Windows / macOS portability — Linux-only per project constraints.
- `splinter_recreate()` under `SPLINTER_PERSISTENT` vs. shared-memory variants — both are in scope; `msync` is a no-op on `MAP_SHARED` over `/dev/shm` but harmless.

---

## Open Questions

1. **`max_val_sz` mutability:** Should `splinter_recreate()` accept an optional `new_max_val_sz` parameter and recompute `val_off` per slot? Adds flexibility for users who want different slot geometry on the same medium, at the cost of a more complex signature and additional bounds arithmetic.
2. **Return value on `msync` failure:** Should the function return `-1` on `msync` failure (after having already reset the in-memory state), or should it attempt to restore the previous header? Given the target use case (flash), partial writes are expected to be durable; a best-effort msync failure return is the pragmatic choice.
3. **Signal group reset:** `signal_groups` in the header contain epoll fds that are process-local. Should `splinter_recreate()` zero them (forcing callers to re-register watchers) or leave them intact? Zeroing is safer; leaving them intact is a no-op for most flash use cases where there are no watchers.
4. **CLI command name:** `recreate` vs. `reset` vs. `reinit` — the issue uses "re-set" (hyphenated); `recreate` mirrors the API name and avoids confusion with `reset` (which might imply only value clearing, not slot metadata).

---

## Acceptance Criteria

- [ ] `splinter_recreate(new_slots)` is declared in `splinter.h` and implemented in `splinter.c`.
- [ ] Calling `splinter_recreate(N)` on a store created with `slots=500000` and `N ≤ 500000` succeeds and sets `H->slots == N`.
- [ ] Calling `splinter_recreate(N)` where the resulting layout would exceed `H->val_sz` returns a non-zero error code without modifying the store.
- [ ] After `splinter_recreate()`, all slot keys are empty, all hashes are 0, all epochs are 0, and `H->epoch == 1`.
- [ ] `make tests` passes with zero Valgrind errors; no new memory leaks introduced by the event-bus fd cleanup path.
- [ ] The CLI `recreate <N>` command correctly calls `splinter_recreate(N)` and exits non-zero on error.
- [ ] `splinter_recreate()` does **not** call `ftruncate`; the file size on disk is unchanged (verified by `fstat` before and after in a test).
- [ ] Regression: existing `splinter_create()`, `splinter_open()`, and `splinter_close()` tests continue to pass unchanged.

---

## Implementation Plan

### Task 1 — `splinter_recreate()` core implementation
**Entry:** `splinter.h` / `splinter.c` as they exist; `splinter_create()` as the reference init sequence.  
**Work:**
- Declare `int splinter_recreate(size_t new_slots);` in `splinter.h` with doc comment.
- Implement in `splinter.c`:
  - Guard: return `-1` if `!H`.
  - Bounds check: compute layout size, return `-2` if it exceeds `H->val_sz`.
  - Validate `new_slots >= 1`, return `-3` otherwise.
  - Close event bus: if `g_event_fd >= 0`, `close(g_event_fd)`, set `g_event_fd = -1`.
  - Derive `new_max_val_sz = (H->val_sz - sizeof(splinter_header) - new_slots * sizeof(splinter_slot)) / new_slots`.
  - Rewrite header fields (preserve `magic`, `version`, `val_sz`; reset all others).
  - Zero and reinitialise `new_slots` slot descriptors with recalculated `val_off`.
  - Zero trailing slots beyond `new_slots` up to old `H->slots` (prevents stale keys from being visible if slot count decreases).
  - `msync(g_base, g_total_sz, MS_SYNC)`.
  - Return 0 on success.  
**Exit:** Unit test exercises success path, oversized-slot rejection, and zero-slot rejection; Valgrind-clean.  
**Blockers:** None.

### Task 2 — Unit tests
**Entry:** `splinter_test.c` (both standard and persistent variants via `SPLINTER_PERSISTENT`).  
**Work:**
- Add a `recreate` test block after the existing `splinter_create()` / write / close cycle.
- Test: create store with `slots=64`, write 3 keys, call `splinter_recreate(32)`, assert `H->slots == 32`, assert all 32 slots are empty, assert old keys are gone.
- Test: call `splinter_recreate(0)` — expect non-zero return.
- Test: call `splinter_recreate(99999999)` on a small store — expect non-zero return.
- Test: `fstat` before and after to confirm file size unchanged.  
**Exit:** `make tests` green for both `splinter_test` and `splinterp_test`; Valgrind-clean.  
**Blockers:** Task 1.

### Task 3 — CLI `recreate` command
**Entry:** CLI command framework (`splinter_cli_main.c`, `splinter_cli.h`); existing single-argument commands as template.  
**Work:**
- Add `splinter_cli_cmd_recreate.c`.
- Parse `recreate <new_slots>` (one required integer argument).
- Open the active store, call `splinter_recreate(new_slots)`, print success or error message, return exit code.
- Register in the CLI dispatch table.  
**Exit:** `splinterctl recreate 64` on a live store resets it; `splinterctl_tests.sh` gains at least one test case covering success and invalid-argument paths.  
**Blockers:** Task 1.
