---
title: Lua
parent: "Language Bindings (FFI)"
nav_order: 4
metas:
  lang: en
  description: >-
    Splinter is a minimalist, lock-free key-value manifold designed for high-frequency data ingestion and retrieval across disjointed runtimes using IPC.
  keywords:
    - Vector Anti-Database
    - Shared-Memory Substrate
    - Lock-Free IPC
    - LLM Semantic Memory
    - Inter-Process Communication
  robots: true
  generator: true
---

# Lua

Just like you'd expect, mostly:

```lua
local result = bus.set("test_multi", multi) or -1
print(result)

bus.set("test_integer", 1)
bus.math("test_integer", "inc", 65535)
```

See `splinter_cli_cmd_lua.c` for the actual coupling and breakout box to add
more definitions and functionality.

If someone wants to expose the whole store as a table in Lua, I'd love a patch!
