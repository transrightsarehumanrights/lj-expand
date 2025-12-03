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
            if type(v) == "table" then
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
    end

    return newMt
end

safeEnv.cloned_mts = {}
safeEnv.cloned_basemts = {}
-- We'll add more later
safeEnv.cloned_mts["Entity"] = cloneMetaTable("Entity")
safeEnv.cloned_mts["Player"] = cloneMetaTable("Player", safeEnv.cloned_mts["Entity"])
safeEnv.cloned_mts["Vector"] = cloneMetaTable("Vector")
safeEnv.cloned_mts["Angle"] = cloneMetaTable("Angle")
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

lje.con_print("Safe environment ready!")
lje.env.set(safeEnv)

lje.con_print("Patching bytecodes...")
lje.vm.patch_bytecodes()
lje.con_print("Preinit script finished!")