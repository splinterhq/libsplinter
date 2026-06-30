---
title: "lua | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `lua` CLI User's Reference

The purpose of `lua` is to run a Lua script with access to the Splinter bus (available in builds with Lua enabled).

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<script.lua>` | Yes | Path to the Lua script to run. |
| `[args...]` | No | Arguments passed to the script via the standard Lua `arg` table. |

### Example Uses

**Console:**
```
splinter_debug # lua tag.lua mykey 42
```

**Shell:**
```
$ splinterctl lua tag.lua mykey 42
```

### Additional Information And Rationale

**Additional Info (Or None):**
Any args after the script are exposed to it via the standard Lua `arg` table: `arg[0]` is the script path, `arg[1..n]` are the args. This command is only present in builds compiled with Lua support (`HAVE_LUA`).

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[wasm](splinterctl_wasm.md)
