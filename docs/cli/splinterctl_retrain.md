---
title: "retrain"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `retrain` CLI User's Reference

The purpose of `retrain` is to prepare a key for retraining: it zeroes the key's vectors and flips its epoch back to 4, releasing the sequence lock and republishing it.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key_name>` | Yes | The key to retrain. |

### Example Uses

**Console:**
```
splinter_debug # retrain doc:42
```

**Shell:**
```
$ splinterctl retrain doc:42
```

### Additional Information And Rationale

**Additional Info (Or None):**
The epoch moving backwards signals clients and watchers to revalidate the key. This works even when vectors are not enabled, in which case only the epoch is flipped. If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[bump](splinterctl_bump.md), [search](splinterctl_search.md)
