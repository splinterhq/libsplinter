---
title: "splinter_integer_op"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_integer_op` Splinter API Reference

The purpose of `splinter_integer_op` is to perform a bitwise or arithmetic operation in place on a key named as a big unsigned integer.

### Forward Declaration & Use

`int splinter_integer_op(const char *key, splinter_integer_op_t op, const void *mask)` `<splinter.h>`

```
uint64_t one = 1;
splinter_integer_op("counter", SPL_OP_INC, &one);
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -1 on internal error, or -2 on caller error. Valid ops are `SPL_OP_AND`, `SPL_OP_OR`, `SPL_OP_XOR`, `SPL_OP_NOT`, `SPL_OP_INC`, `SPL_OP_DEC`.

**Errno Behavior:**
The CLI `math` command treats `errno == EPROTOTYPE` as "the key is not a BIGUINT slot" and `errno == EAGAIN` as a collision that should be retried.

**Rationale (Or None):**
Operating directly on `BIGUINT` keys in shared memory avoids any read-modify-write round trip through a separate service.

### See Also

**Relevant Symbols (Or None):**
[splinter_set_named_type](splinter_set_named_type.md), [splinter_set](splinter_set.md)
