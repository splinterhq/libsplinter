-- get and print a known key (quick lua check)

local bus = require("splinter")
local test = bus.get("test_key") or 0
print("Test result:" .. test)

local multi = "1, 2, 3, 4, 5"
local result = bus.set("test_multi", multi) or -1
print(result)

bus.set("test_integer", 1)
bus.math("test_integer", "inc", 65535)

