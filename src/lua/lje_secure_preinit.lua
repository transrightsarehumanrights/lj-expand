-- Secure preinit script. No need for a custom environment as this takes place entirely within a
-- separate Lua universe. All we do here is ensure that all the C API is pulled in, add a hook library,
-- ensure metatables exist in registry and perform some GMod-specific registry hacks.

lje.con_print("Running secure preinit script...")

local registry = lje.util.get_registry()
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