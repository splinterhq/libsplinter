---
title: "splinter_set_embedding | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_set_embedding` Splinter API Reference

The purpose of `splinter_set_embedding` is to set the embedding vector for a specific key from an array of floats.

### Forward Declaration & Use

`int splinter_set_embedding(const char *key, const float *embedding)` `<splinter.h>`

```
/* Requires a build with SPLINTER_EMBEDDINGS. embedding holds SPLINTER_EMBED_DIM (768) floats. */
float vec[768] = { /* ... */ };
if (splinter_set_embedding("doc:42", vec) != 0) {
    perror("splinter_set_embedding");
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
The embedding array is `SPLINTER_EMBED_DIM` (768) floats wide, matching the per-slot `embedding` field compiled in under `SPLINTER_EMBEDDINGS`.

### See Also

**Relevant Symbols (Or None):**
[splinter_get_embedding](splinter_get_embedding.md), [splinter_retrain_slot](splinter_retrain_slot.md)
