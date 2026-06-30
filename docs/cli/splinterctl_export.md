---
title: "export"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `export` CLI User's Reference

The purpose of `export` is to export the store in various formats to standard output.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `[format]` | No | Output format. Currently `json` (the default). |
| `[max_lines]` | No | Maximum number of lines to export. Defaults to `0` (unlimited). |

### Example Uses

**Console:**
```
splinter_debug # export json 100
```

**Shell:**
```
$ splinterctl export > dump.json
```

### Additional Information And Rationale

**Additional Info (Or None):**
The help notes that more formats are coming; at present `json` is the documented format.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[list](splinterctl_list.md)
