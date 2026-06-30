---
title: "splinter_retrain_slot | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_retrain_slot` Splinter API Reference

The purpose of `splinter_retrain_slot` is to reset a slot for retraining: it scrubs the slot's embedding, drives the seqlock back to a known-good even epoch of 4, and republishes the slot.

### Forward Declaration & Use

`int splinter_retrain_slot(const char *key)` `<splinter.h>`

```
/* Revalidate a key and release any seqlock stuck odd by a dead trainer. */
splinter_retrain_slot("doc:42");
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -1 if the key is not found, or -2 on bad arguments. The epoch is stored outright (not advanced), so it succeeds regardless of the slot's current state; watchers and the event bus are pulsed.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
The epoch moving *backwards* is the documented signal to clients and watchers that the key must be revalidated, and it is the only sanctioned way to free a seqlock left stuck odd by a crashed writer. It is the wrong tool for an ordinary refresh (use `splinter_set` / `splinter_append`). Works even without embeddings compiled in, in which case it only resets the epoch and republishes. Classified as DESTRUCTIVE in the AI Primer.

### See Also

**Relevant Symbols (Or None):**
[splinter_get_epoch](splinter_get_epoch.md), [splinter_set_embedding](splinter_set_embedding.md), [splinter_set](splinter_set.md)
