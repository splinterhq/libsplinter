-- get and print a known key (quick lua check)
local bus = require("splinter")
local test = bus.get("test_key") or 0
print("Test result:" .. test)

