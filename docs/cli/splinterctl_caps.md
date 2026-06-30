---
title: "caps | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `caps` CLI User's Reference

The purpose of `caps` is to print the version, build, and compiled-in feature flags, one `key=value` per line for scripting.

### Arguments & Switches

None — this command takes no arguments.

### Example Uses

**Console:**
```
splinter_debug # caps
version=...
build=...
lua=yes
wasm=no
embeddings=yes
llama=yes
numa=no
persistent=no
```

**Shell:**
```
$ splinterctl caps
```

### Additional Information And Rationale

**Additional Info (Or None):**
Reported feature keys are `version`, `build`, `lua`, `wasm`, `embeddings`, `llama`, `numa`, and `persistent`. Each feature is `yes` or `no` depending on the build's compile-time flags.

**Rationale (Or None):**
Output is `key=value`, one per line, suitable for scripting.

### See Also

**Related Commands (Or None):**
[config](splinterctl_config.md), [help](splinterctl_help.md)
