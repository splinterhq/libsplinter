-- get and print a known key (quick lua check)

local bus = require("splinter")

-- Invocation args arrive in the standard Lua 'arg' table:
--   arg[0]  is the script path
--   arg[1..n] are whatever followed it, e.g.
--     lua test.lua mykey 42  -->  arg[1]="mykey", arg[2]="42"
print("script: " .. (arg and arg[0] or "?"))
if arg and #arg > 0 then
    print("received " .. #arg .. " arg(s):")
    for i = 1, #arg do
        print(string.format("  arg[%d] = %s", i, arg[i]))
    end
else
    print("no args (try: lua test.lua mykey 42)")
end
local test = bus.get("test_key") or 0
print("Test result:" .. test)

local multi = "1, 2, 3, 4, 5"
local result = bus.set("test_multi", multi) or -1
print(result)

bus.set("test_integer", 1)
bus.math("test_integer", "inc", 65535)

