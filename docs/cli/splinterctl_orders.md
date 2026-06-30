---
title: "orders | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `orders` CLI User's Reference

The purpose of `orders` is to manage the standard orders (tandem keys) of a key for coupled vector storage.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `set` \| `unset` | Yes | `set` creates the tandem orders; `unset` removes them. |
| `<key>` | Yes | The base key. |
| `<count>` | Yes | Number of orders to create or remove. |

### Example Uses

**Console:**
```
splinter_debug # orders set car 3
Tandem set for car with 3 orders: OK
```

**Shell:**
```
$ splinterctl orders unset car 3
```

### Additional Information And Rationale

**Additional Info (Or None):**
`set` writes the tandem members via `splinter_client_set_tandem`; `unset` removes them via `splinter_client_unset_tandem`. Any mode other than `set` or `unset` is rejected.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[unset](splinterctl_unset.md), [set](splinterctl_set.md)
