---
title: "splinter_get"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_get` Splinter API Reference

The purpose of `splinter_get` is to retrieve the value associated with a key into a caller-supplied buffer, optionally reporting the value's true length through `out_sz`.

### Forward Declaration & Use

`int splinter_get(const char *key, void *buf, size_t buf_sz, size_t *out_sz)` `<splinter.h>`

```
char buf[4096];
size_t len = 0;
if (splinter_get("greeting", buf, sizeof(buf), &len) == 0) {
    fwrite(buf, 1, len, stdout);
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure. `buf` may be NULL to query only the size; `out_sz` may be NULL.

**Errno Behavior:**
If `buf_sz` is too small to hold the value, `errno` is set to `EMSGSIZE`. Per the AI Primer, a return of -1 with `errno == EAGAIN` means the slot is momentarily contested and the call should be retried.

**Rationale (Or None):**
`splinter_get` always honors the slot's live `val_len`, unlike `splinter_get_raw_ptr`, so it never returns stale trailing bytes.

### See Also

**Relevant Symbols (Or None):**
[splinter_set](splinter_set.md), [splinter_get_raw_ptr](splinter_get_raw_ptr.md), [splinter_get_slot_snapshot](splinter_get_slot_snapshot.md)
