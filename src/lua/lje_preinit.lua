-- Preinit script. Nothing is running here, so we dont even need to disable hooks.
-- That *does* mean only the raw C GMod API is available here, so be careful. Absolutely no other
-- lua functions or libraries are accessible.
lje.con_print("Preinit script running.. creating env")
local safeEnv = {}

local function cloneTable(tbl, dest, visited)
    visited = visited or {}
    dest = dest or {}
    visited[tbl] = dest

    for k, v in pairs(tbl) do
        if type(v) == "table" then
            if visited[v] then
                dest[k] = visited[v]
            else
                dest[k] = cloneTable(v, nil, visited)
            end
        else
            dest[k] = v
        end
    end

    return dest
end

cloneTable(_G, safeEnv)
safeEnv._G = _G -- expose original _G

lje.con_print("Done! Setting up safe metatables...")
local function cloneBaseMt(mt)
    local newMt = {}
    for k, v in pairs(mt) do
        newMt[k] = v
    end
    return newMt
end

local function cloneMetaTable(name, base)
    local mt = FindMetaTable(name)

    local newMt = {}
    local function deepCopy(orig)
        if type(orig) ~= "table" then
            return orig
        end

        local copy = {}
        for k, v in pairs(orig) do
            if type(v) == "table" and v ~= orig then
                copy[k] = deepCopy(v)
            else
                copy[k] = v
            end
        end
        return copy
    end

    newMt = deepCopy(mt)
    -- link to cloned base metatable if exists
    if base then
        newMt.BaseMetaClass = base
        -- Additionally we need to merge the base metatable functions
        for k, v in pairs(base) do
          if newMt[k] == nil then -- avoids overwriting important functions
            newMt[k] = v
          end
        end
    end

    newMt.__oldIndex = mt.__index
    newMt.__index = newMt
    return newMt
end

safeEnv.cloned_mts = {}
safeEnv.cloned_basemts = {}
-- We'll add more later
safeEnv.cloned_mts["Entity"] = cloneMetaTable("Entity")
safeEnv.cloned_mts["CSEnt"] = cloneMetaTable("CSEnt", safeEnv.cloned_mts["Entity"])
safeEnv.cloned_mts["Player"] = cloneMetaTable("Player", safeEnv.cloned_mts["Entity"])
safeEnv.cloned_mts["NPC"] = cloneMetaTable("NPC", safeEnv.cloned_mts["Entity"])
safeEnv.cloned_mts["Vector"] = cloneMetaTable("Vector")
safeEnv.cloned_mts["Angle"] = cloneMetaTable("Angle")
safeEnv.cloned_mts["CUserCmd"] = cloneMetaTable("CUserCmd")
safeEnv.cloned_mts["File"] = cloneMetaTable("File")
safeEnv.cloned_mts["ConVar"] = cloneMetaTable("ConVar")
safeEnv.cloned_mts["VMatrix"] = cloneMetaTable("VMatrix")
safeEnv.cloned_mts["Weapon"] = cloneMetaTable("Weapon", safeEnv.cloned_mts["Entity"])
safeEnv.cloned_mts["IMaterial"] = cloneMetaTable("IMaterial")
safeEnv.cloned_mts["ITexture"] = cloneMetaTable("ITexture")
safeEnv.cloned_mts["PhysObj"] = cloneMetaTable("PhysObj")
-- You shouldn't ever really use panels, since they are *incredibly* detectable, but just in case
safeEnv.cloned_mts["Panel"] = cloneMetaTable("Panel")

for name, mt in pairs(safeEnv.cloned_mts) do
  lje.con_print("Remapping metatable for " .. name)
  lje.env.remap_metatable(name, mt)
end

safeEnv.cloned_basemts["string"] = cloneBaseMt(debug.getmetatable(""))
safeEnv.insecure_mts = {}

safeEnv.lje.use_safe_basemts = function()
    local curStringMt = debug.getmetatable("")
    insecure_mts["string"] = curStringMt

    debug.setmetatable("", cloned_basemts["string"])
end

safeEnv.lje.restore_basemts = function()
    local insecureStringMt = insecure_mts["string"]
    if insecureStringMt then
        debug.setmetatable("", insecureStringMt)
    end
end

setfenv(safeEnv.lje.use_safe_basemts, safeEnv)
setfenv(safeEnv.lje.restore_basemts, safeEnv)

safeEnv.lje.detour = function(origFn, detourFn)
    if lje.func.type(origFn) ~= 0 then
        -- C function or fast function. Means we need to prohibit any JIT compilation.
        jit.off(detourFn)
    end

    lje.func.mark_special(detourFn)
    lje.func.spoof(detourFn, origFn)
    return detourFn
end

setfenv(safeEnv.lje.detour, safeEnv)

safeEnv.lje.includeCache = {}
safeEnv.lje.require = function(path)
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

setfenv(safeEnv.lje.require, safeEnv)

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
safeEnv.lje.con_printf = function(fmt, ...)
  -- First, replace color codes
  local result = string.format(fmt, ...)
  local coloredResult = string.gsub(result, COLOR_PATTERN, function(colorName, text)
    local colorCode = ANSI_COLORS[string.lower(colorName)] or ANSI_COLORS["default"]
    return "\x1b[" .. colorCode .. string.sub(text, 2, -2) .. "\x1b[0m" -- remove braces
  end)

  lje.con_print(coloredResult .. "\x1b[0m") -- Reset color at the end
end

setfenv(safeEnv.lje.con_printf, safeEnv)

local type = type
local rawget = rawget
local istable = istable
local _G = _G
safeEnv.lje.get_global = function(...)
    -- Basically just a wrapper over rawget to traverse global tables safely
    -- For a faster version, see lje.get_global_static which does no dynamic allocations and doesn't rely on a vararg
    local paths = {...}
    local count = #paths
    local current = _G

    local i = 1
    ::iterateGlobals::
    current = rawget(current, paths[i])
    if current then
        if i == count then
            return current
        elseif istable(current) then
            i = i + 1
            goto iterateGlobals
        else
            return nil
        end
    else
        return nil
    end
end

safeEnv.lje.get_global_static = function(paths, count)
    -- Same behaviour as lje.get_global, but instead of using a vararg,
    -- you give the function the path as a list (avoid dynamic creation of this as that defeats the purpose of the function),
    -- along with the number of elements in the list to be traversed
    local current = _G

    local i = 1
    ::iterateGlobals::
    current = rawget(current, paths[i])
    if current then
        if i == count then
            return current
        elseif istable(current) then
            i = i + 1
            goto iterateGlobals
        else
            return nil
        end
    else
        return nil
    end
end

setfenv(safeEnv.lje.get_global, safeEnv)
setfenv(safeEnv.lje.get_global_static, safeEnv)

-- Add a circular reference to the safe environment in the safeEnv
safeEnv._L = safeEnv

lje.con_print("Safe environment ready!")
lje.env.set(safeEnv)

lje.con_print("Patching bytecodes...")
lje.vm.patch_bytecodes()

lje.con_print("Hiding common callers...")
for _, func in ipairs({pcall, xpcall, ProtectedCall}) do
  lje.func.hide_caller(func)
end
lje.con_print("Callers hidden!")

-- Detour SysTime to subtract the clock deficit from it.
local origSysTime = SysTime
local clock_deficit = lje.env.clock_deficit
local function sysTimeHk()
  return origSysTime() - clock_deficit()
end

safeEnv._G.SysTime = safeEnv.lje.detour(origSysTime, sysTimeHk)

lje.con_print("Preinit script finished!")