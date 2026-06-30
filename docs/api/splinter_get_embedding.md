---
title: "splinter_get_embedding | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_get_embedding` Splinter API Reference

The purpose of `splinter_get_embedding` is to retrieve the embedding vector for a specific key into a caller-supplied float buffer.

### Forward Declaration & Use

`int splinter_get_embedding(const char *key, float *embedding_out)` `<splinter.h>`

```
/* Requires a build with SPLINTER_EMBEDDINGS. embedding_out holds 768 floats. */
float vec[768];
if (splinter_get_embedding("doc:42", vec) == 0) {
    /* use vec ... */
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
The output buffer must hold `SPLINTER_EMBED_DIM` (768) floats, matching the per-slot embedding width.

### See Also

**Relevant Symbols (Or None):**
[splinter_set_embedding](splinter_set_embedding.md)
