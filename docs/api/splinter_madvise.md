---
title: "splinter_madvise"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_madvise` Splinter API Reference

The purpose of `splinter_madvise` is the cooperative `posix_madvise()` entry point (the voluntary-yield path): it runs the election and, if the caller is sovereign, issues the advice; otherwise it defers, blocks, or times out per `timeout_ticks`.

### Forward Declaration & Use

`int splinter_madvise(uint32_t shard_id, void *addr, size_t len, int advice, uint64_t timeout_ticks)` `<splinter.h>`

```
/* Advise the whole value arena if sovereign; defer (don't block) otherwise. */
splinter_madvise(0x5F1A, NULL, 0, POSIX_MADV_WILLNEED, 0);
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success (advisement issued), -1 on EAGAIN/timeout/`posix_madvise` failure (errno set), or -2 on bad args / no store / unknown shard_id. If `addr` is NULL it advises the whole value arena.

**Errno Behavior:**
If the caller is not sovereign and `timeout_ticks == 0`, it does not block and returns -1 with `errno = EAGAIN` (defer). With `UINT64_MAX` it blocks until sovereign; otherwise it blocks up to `timeout_ticks`, re-electing on each wake.

**Rationale (Or None):**
Independent `posix_madvise()` calls on a shared arena make the kernel page cache thrash and destroy L3 residency; routing advice through the election ensures only the sovereign's advice lands. Never call raw `posix_madvise()` on a shared store. Blocking uses the eventfd broker when armed, otherwise a TSC-polled nanosleep fallback.

### See Also

**Relevant Symbols (Or None):**
[splinter_shard_election](splinter_shard_election.md), [splinter_shard_claim](splinter_shard_claim.md), [splinter_shard_is_sovereign](splinter_shard_is_sovereign.md)
