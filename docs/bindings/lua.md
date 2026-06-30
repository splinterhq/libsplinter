---
title: Lua | Splinter FFI Bindings
date: 2026-06-30
updated: 2026-06-30
---

## Lua

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
