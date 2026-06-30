---
title: "search | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `search` CLI User's Reference

The purpose of `search` is to search embedded keys by semantic similarity (available in builds with embeddings enabled).

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<query>` \| `-` | Yes (or `--file`) | Quoted query string. Use `-` to read the query from stdin. |
| `--file PATH` | No | Read query text from PATH (use `-` for stdin). Mutually exclusive with the positional query. |
| `--json` | No | Output results as JSON. |
| `--limit N` | No | Maximum number of results (default: unlimited). |
| `--distance F` | No | Maximum euclidean distance to include (default: no filter). |
| `--similarity F` | No | Minimum cosine similarity to include (default: no filter). |
| `--bloom MASK` | No | Hex bloom mask; uses `enumerate_matches()` for pre-filtering. |
| `--regex PATTERN` | No | Regex filter applied to key names before scoring. |

### Example Uses

**Console:**
```
splinter_debug # search "quarterly revenue" --limit 5 --json
```

**Shell:**
```
$ echo "quarterly revenue" | splinterctl search - --limit 5
```

### Additional Information And Rationale

**Additional Info (Or None):**
This command is only present in builds compiled with embeddings (`HAVE_EMBEDDINGS`).

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[ingest](splinterctl_ingest.md), [retrain](splinterctl_retrain.md)
