---
title: "ingest"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `ingest` CLI User's Reference

The purpose of `ingest` is to read a file or stdin and store it as chunked tandem slots for splinference pickup (available in builds with embeddings enabled).

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `[file]` | No (required if not using stdin) | The file to ingest. When omitted, reads from stdin (and `--key` is required). |
| `--key <key>` | Required for stdin | Base key name. Defaults to `basename(file)` when a file is given. |
| `--label <hex>` | No | Override the chunk label bit (default `SPL_INGEST_CHUNK`). |

### Example Uses

**Console:**
```
splinter_debug # ingest notes.txt
```

**Shell:**
```
$ cat notes.txt | splinterctl ingest --key notes
```

### Additional Information And Rationale

**Additional Info (Or None):**
Chunks are typed `VARTEXT` and labeled for splinference pickup; a JSON metadata slot is written to the base key (no suffix). Chunk size is derived from the store's `max_val_sz` at runtime. Keys longer than 56 chars are truncated to leave room for the order-accessor suffix (`.<N>` up to `.999`). The default chunk label tells splinference to embed the slot; the meta label marks the document index (not embedded). This command is only present in builds compiled with embeddings (`HAVE_EMBEDDINGS`).

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[search](splinterctl_search.md), [orders](splinterctl_orders.md)
