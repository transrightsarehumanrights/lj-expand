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

    newMt.__index = newMt
    return newMt
end

safeEnv.cloned_mts = {}
safeEnv.cloned_basemts = {}
-- We'll add more later
safeEnv.cloned_mts["Entity"] = cloneMetaTable("Entity")
safeEnv.cloned_mts["Player"] = cloneMetaTable("Player", safeEnv.cloned_mts["Entity"])
safeEnv.cloned_mts["Vector"] = cloneMetaTable("Vector")
safeEnv.cloned_mts["Angle"] = cloneMetaTable("Angle")
safeEnv.cloned_mts["CUserCmd"] = cloneMetaTable("CUserCmd")
safeEnv.cloned_mts["File"] = cloneMetaTable("File")
safeEnv.cloned_mts["ConVar"] = cloneMetaTable("ConVar")
safeEnv.cloned_mts["VMatrix"] = cloneMetaTable("VMatrix")
safeEnv.cloned_mts["Weapon"] = cloneMetaTable("Weapon", safeEnv.cloned_mts["Entity"])

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
    lje.func.mark_special(detourFn)
    lje.func.spoof(detourFn, origFn)
    return detourFn
end

setfenv(safeEnv.lje.detour, safeEnv)

local includeCache = {}
safeEnv.lje.require = function(path)
  local currentScript = lje.env.current_script()
  if not currentScript then
    lje.con_print("Error: lje.require called outside of a script context!")
    return
  end

  includeCache[currentScript] = includeCache[currentScript] or {}
  local scriptCache = includeCache[currentScript]
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

safeEnv.lje.get_global = function(path)
    -- Basically just a wrapper over rawget to traverse global tables safely
    -- So given concommand.Run it's gonna rawget concommand, then rawget Run
    local current = _G
    for part in string.gmatch(path, "[^%.]+") do
        if type(current) ~= "table" then
            return nil
        end
        current = rawget(current, part)
        if current == nil then
            return nil
        end
    end

    return current
end

setfenv(safeEnv.lje.get_global, safeEnv)

local engineCallHooks = {}
safeEnv.lje.vm.add_engine_call_hook = function(fn)
  table.insert(engineCallHooks, fn)
end

setfenv(safeEnv.lje.vm.add_engine_call_hook, safeEnv)

local function engineCallHookDispatcher(...)
  for _, hookFn in ipairs(engineCallHooks) do
    -- Really important that we set up a LJE-pcall, we cant handle errors here normally since
    -- well, it's in the middle of a pcall and it'll just crash the engine otherwise.
    local results = {pcall(hookFn, ...)}
    if not results[1] then
      lje.con_printf("$red{Error in engine call hook}: " .. tostring(results[2]))
    else
      if results[1] and not results[2] then
          return unpack(results, 2) -- early out if hook returns false, means someone wants to block the call
      end
    end
  end

  return true
end

setfenv(engineCallHookDispatcher, safeEnv)
lje.vm.set_engine_call_hook(engineCallHookDispatcher)
lje.con_print("Engine call hook set!")

-- Add a circular reference to the safe environment in the safeEnv
safeEnv._L = safeEnv

lje.con_print("Safe environment ready!")
lje.env.set(safeEnv)

lje.con_print("Patching bytecodes...")
lje.vm.patch_bytecodes()
lje.con_print("Preinit script finished!")