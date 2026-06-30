---
title: "wasm | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `wasm` CLI User's Reference

The purpose of `wasm` is to execute a WASM module (via WASMEdge) with access to the Splinter bus (available in builds with WASM enabled).

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<plugin.wasm>` | Yes | Path to the WASM module to execute. |
| `[function_name]` | No | The function to invoke within the module. |

### Example Uses

**Console:**
```
splinter_debug # wasm test.wasm
```

**Shell:**
```
$ splinterctl wasm test.wasm my_func
```

### Additional Information And Rationale

**Additional Info (Or None):**
This command is only present in builds compiled with WASM support (`HAVE_WASM`).

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[lua](splinterctl_lua.md)
