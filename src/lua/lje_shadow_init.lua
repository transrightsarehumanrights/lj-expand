-- Seeds every shadow registry with the stubs GMod's C API expects to find there.
-- Runs at the start of both the menu boot phase and the client preinit phase, so
-- menu-phase calls are covered and a closed client state gets re-seeded.

local registry = lje.util.get_registry()
local shadowClient = registry["__lje_shadow_registry_client"]
local shadowMenu = registry["__lje_shadow_registry_menu"]

for _, shadow in ipairs({ shadowClient, shadowMenu }) do
  shadow[2] = { hook = { Call = function() end } } -- hook ref GMod uses
  shadow[13371010] = function() return end        -- Dummy Lua function
end

shadowClient[1337153] = function() return "hello" end
