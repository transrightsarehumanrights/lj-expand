-- Secure preinit script. No need for a custom environment as this takes place entirely within a
-- separate Lua universe. All we do here is ensure that all the C API is pulled in, add a hook library,
-- ensure metatables exist in registry and perform some GMod-specific registry hacks.

lje.con_print("Running secure preinit script...")

local registry = lje.util.get_registry()

-- Helpers

-- Neatly print out any value using lje.con_print. Handles nested tables with
-- indentation and guards against recursive (cyclic) references.
function lje.util.inspect(x)
    local seen = {}

    local function valueToString(v, indent)
        local t = type(v)

        if t == "string" then
            return '"' .. v .. '"'
        elseif t == "number" or t == "boolean" then
            return tostring(v)
        elseif t == "nil" then
            return "nil"
        elseif t ~= "table" then
            -- function, userdata, thread, cdata, etc.
            return "<" .. t .. ": " .. tostring(v) .. ">"
        end

        -- It's a table from here on.
        if seen[v] then
            return "<recursion: " .. tostring(v) .. ">"
        end
        seen[v] = true

        local childIndent = indent .. "  "
        local out = "{\n"
        local isEmpty = true

        for k, val in pairs(v) do
            isEmpty = false

            local keyStr
            if type(k) == "string" then
                keyStr = k
            else
                keyStr = "[" .. tostring(k) .. "]"
            end

            out = out .. childIndent .. keyStr .. " = " .. valueToString(val, childIndent) .. ",\n"
        end

        -- Allow this table to be visited again on sibling branches once we're done with it.
        seen[v] = nil

        if isEmpty then
            return "{}"
        end

        return out .. indent .. "}"
    end

    lje.con_print(valueToString(x, ""))
end

-- Recursively deep copy a value. Tables are duplicated key-by-key; everything
-- else (functions, userdata, cdata, etc.) is referenced as-is since those can't
-- be meaningfully duplicated. Cyclic references are preserved via the `seen` map,
-- and metatables are kept (by reference, as they're shared).
local function deepCopy(value, seen)
    if type(value) ~= "table" then
        return value
    end

    seen = seen or {}
    if seen[value] then
        return seen[value]
    end

    local copy = {}
    seen[value] = copy

    for k, v in pairs(value) do
        copy[k] = deepCopy(v, seen)
    end

    local mt = getmetatable(value)
    if mt ~= nil then
        setmetatable(copy, mt)
    end

    return copy
end

function __LJE_RESTORE_REGISTRY()
    for k in pairs(registry) do
        registry[k] = nil
    end
    for k, v in pairs(__LJE_REGISTRY_SAVED) do
        registry[k] = deepCopy(v)
    end
end

if not __LJE_REGISTRY_SAVED then
    -- First game session: snapshot the pristine registry by deep copying every
    -- value into a permanent global latch. It's written once at the first game
    -- joined and stays here forever, surviving across sessions.
    __LJE_REGISTRY_SAVED = deepCopy(registry)
else
    -- Subsequent sessions: the live registry may contain stale entries left over
    -- from the previous game. Wipe it clean and restore the pristine snapshot by
    -- copying the saved values back in. Each value is deep copied again so the
    -- snapshot itself stays pristine for any future restores.
    __LJE_RESTORE_REGISTRY()
end

registry[2] = { hook = {Call = function() end} } -- hook ref GMod uses
registry[1337153] = function() return "hello" end
  registry[13371010] = function() return end -- Dummy Lua function

-- Only a subset of necessary GMod C APIs are pulled in. We have our own versions of any base library as well.
achievements = lje.secure.pull("achievements")
cam = lje.secure.pull("cam")
chat = lje.secure.pull("chat")
engine = lje.secure.pull("engine")
ents = lje.secure.pull("ents")
file = lje.secure.pull("file")
game = lje.secure.pull("game")
gameevent = lje.secure.pull("gameevent")
gmod = lje.secure.pull("gmod")
gui = lje.secure.pull("gui")
input = lje.secure.pull("input")
net = lje.secure.pull("net")
render = lje.secure.pull("render")
sound = lje.secure.pull("sound")
sql = lje.secure.pull("sql")
surface = lje.secure.pull("surface")
steamworks = lje.secure.pull("steamworks")
system = lje.secure.pull("system")
timer = lje.secure.pull("timer")
util = lje.secure.pull("util")
player = lje.secure.pull("player")
vgui = lje.secure.pull("vgui")

-- Globals
HTTP = lje.secure.pull("HTTP")
Entity = lje.secure.pull("Entity")
Player = lje.secure.pull("Player")
Vector = lje.secure.pull("Vector")
Angle = lje.secure.pull("Angle")
Matrix = lje.secure.pull("Matrix")
LocalPlayer = lje.secure.pull("LocalPlayer")
Material = lje.secure.pull("Material")
GetConVar_Internal = lje.secure.pull("GetConVar_Internal")
SysTime = lje.secure.pull("SysTime")
CurTime = lje.secure.pull("CurTime")
RealTime = lje.secure.pull("RealTime")
FrameTime = lje.secure.pull("FrameTime")
AddConsoleCommand = lje.secure.pull("AddConsoleCommand")

-- Classes
registry.Entity = lje.secure.pull("_R.Entity")
registry.Player = lje.secure.pull("_R.Player")
registry.Vector = lje.secure.pull("_R.Vector")
registry.Angle = lje.secure.pull("_R.Angle")
registry.CUserCmd = lje.secure.pull("_R.CUserCmd")
registry.File = lje.secure.pull("_R.File")
registry.ConVar = lje.secure.pull("_R.ConVar")
registry.VMatrix = lje.secure.pull("_R.VMatrix")
registry.Weapon = lje.secure.pull("_R.Weapon")
registry.IMaterial = lje.secure.pull("_R.IMaterial")
registry.ITexture = lje.secure.pull("_R.ITexture")
registry.PhysObj = lje.secure.pull("_R.PhysObj")
registry.Vehicle = lje.secure.pull("_R.Vehicle")
registry.Panel = lje.secure.pull("_R.Panel")
registry.CSEnt = lje.secure.pull("_R.CSEnt")
registry.NPC = lje.secure.pull("_R.NPC")

NULL = lje.secure.pull("NULL") -- For some reason, the null entity is a special userdata..?

-- Polyfills for common things
local cam2D = { type = "2D" }
cam.Start2D = function()
    cam.Start(cam2D)
end

-- Small hook library replacement, no returns for now.
hook = {}
hook._listeners = {}

function hook.Add(event, identifier, func)
  if not hook._listeners[event] then
    hook._listeners[event] = {}
  end
  hook._listeners[event][identifier] = func
end

function hook.Listen()
  lje.vm.set_engine_call_hook(function(func, nargs, nresults, ...)
    local name = ...
    if hook._listeners[name] then
      for _, listener in pairs(hook._listeners[name]) do
        listener(...)
      end
    end
  end)
end

function hook.Call() --[[no-op]] end



lje.includeCache = {}
lje.require = function(path)
    local currentScript = lje.env.current_script()
    if not currentScript then
        lje.con_print("Error: lje.require called outside of a script context!")
        return
    end

    lje.includeCache[currentScript] = lje.includeCache[currentScript] or {}
    local scriptCache = lje.includeCache[currentScript]
    if scriptCache[path] then
        return scriptCache[path]
    end

    local result = lje.include(path)
    scriptCache[path] = result
    return result
end

  -- Little printf console helper with color parsing
  -- Usage: lje.con_printf("$red{Error}: Something happened!")
  local ANSI_COLORS = {
    black = "1;30m",
    red = "1;31m",
    green = "1;32m",
    yellow = "1;33m",
    blue = "1;34m",
    magenta = "1;35m",
    cyan = "1;36m",
    white = "1;37m",
    default = "1;39m",
  }

  local COLOR_PATTERN = "%$(%a+)(%b{})"
lje.con_printf = function(fmt, ...)
    -- First, replace color codes
    local result = string.format(fmt, ...)
    local coloredResult = string.gsub(result, COLOR_PATTERN, function(colorName, text)
        local colorCode = ANSI_COLORS[string.lower(colorName)] or ANSI_COLORS["default"]
        return "\x1b[" .. colorCode .. string.sub(text, 2, -2) .. "\x1b[0m" -- remove braces
    end)

    lje.con_print(coloredResult .. "\x1b[0m") -- Reset color at the end
end