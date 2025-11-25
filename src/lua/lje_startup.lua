-- Startup script. For testing. This is still going to be a library,
-- so this will be removed when the library is stable.

-- This script is marked special, so no hook can catch any execution inside this function.


lje.disable_hooks()
lje.con_print("Initialized. Running startup script...")

local startup = lje.include("lje_startup.lua", false) -- dont execute
setfenv(startup, lje.get_env())
lje.con_print("Running startup script...")
startup()
lje.con_print("Startup script finished.")
lje.enable_hooks()

--[[
local enableHooks = enable_hooks
local disableHooks = disable_hooks
local ignoreFnOnHook = ignore_fn_on_hook

local playerGetAll = player.GetAll
local getPos = FindMetaTable("Entity").GetPos
local obbCenter = FindMetaTable("Entity").OBBCenter
local setEyeAngles = FindMetaTable("Player").SetEyeAngles
local getShootPos = FindMetaTable("Player").GetShootPos

local angleVector = FindMetaTable("Vector").Angle
local toScreen = FindMetaTable("Vector").ToScreen
local add = FindMetaTable("Vector").__add
local sub = FindMetaTable("Vector").__sub
local distance = FindMetaTable("Vector").Distance
local playerNick = FindMetaTable("Player").Nick
local playerHealth = FindMetaTable("Entity").Health
local rawget = rawget
local rawset = rawset
local ipairs = ipairs

local setFont = surface.SetFont
local setTextPos = surface.SetTextPos
local setTextColor = surface.SetTextColor
local setDrawColor = surface.SetDrawColor
local drawText = surface.DrawText
local drawOutlinedRect = surface.DrawOutlinedRect

local origHookCall = hook.Call
local concat = table.concat
local tostring = tostring
local localPlayer = LocalPlayer

local start2D = cam.Start2D
local end2D = cam.End2D

local abs = math.abs
local spoof_debug_info = spoof_debug_info
local conPrint = con_print
local hookCallHk

local keyH = KEY_H
local tableSort = table.sort
local isKeyDown = input.IsKeyDown
local vector = Vector

local lookupBone = FindMetaTable("Entity").LookupBone
local getBonePosition = FindMetaTable("Entity").GetBonePosition
local getHitBoxBone = FindMetaTable("Entity").GetHitBoxBone

local targetLock = nil

hookCallHk = function(name, gm, ...)
    local hook = {Call = origHookCall}
    local a, b, c, d, e, f = hook.Call(name, gm, ...)
    disableHooks()
    if name == "PostRender" then
        start2D()
        local us = localPlayer()
        local targets = {}
        for _, ply in ipairs(playerGetAll()) do
            local plyPos = getBonePosition(ply, getHitBoxBone(ply, 0, 0))
            local pt1 = toScreen(plyPos)
            if pt1.visible and ply ~= us then
                local x1 = pt1.x
                local y1 = pt1.y

                local w = 15
                local h = 15

                if playerHealth(ply) > 0 then
                    setDrawColor(255, 100, 100, 255)
                    drawOutlinedRect(x1, y1, w, h, 1)
                end
                if distance(getPos(ply), getPos(us)) < 1200 and playerHealth(ply) > 0 then
                    setFont("ChatFont")
                    setTextPos(x1 + 20, y1)
                    setTextColor(0, 255, 50)
                    drawText(concat({playerNick(ply), " (", playerHealth(ply), " hp)"}))
                end
            end

            if playerHealth(ply) > 0 and ply ~= us then
                targets[#targets + 1] = ply
            end
        end

        if isKeyDown(keyH) and #targets > 0 then
            if not targetLock then
                tableSort(targets, function(a, b) return distance(getPos(us), getPos(a)) < distance(getPos(us), getPos(b)) end)
            end
            local targetPos = getBonePosition(targets[1], getHitBoxBone(targets[1], 0, 0))
            local ang = angleVector(sub(targetPos, getShootPos(us)))
            setEyeAngles(us, ang)
        end
        end2D()
    end
    enableHooks()

    return a, b, c, d, e, f
end
mark_special(hookCallHk) -- since we create an extra frame.
spoof_debug_info(hookCallHk, origHookCall)
hook.Call = hookCallHk

local origRenderCapture = render.Capture
local function renderCaptureHk(data)
    disableHooks()
    data = data or {}
    conPrint("render.Capture called. Messing with it.")
    enableHooks()
    return origRenderCapture({format = data.format, x = 30, y = 30, w = 1024, h = 32, quality = data.quality, alpha = data.alpha})
end

mark_special(renderCaptureHk)
spoof_debug_info(renderCaptureHk, origRenderCapture)
render.Capture = renderCaptureHk

local enableHooks = enable_hooks
local disableHooks = disable_hooks
local origNetStart = net.Start
local function netStartHk(msg, unreliable)
    disableHooks()
    conPrint(concat({"Net message ", msg, " started."}))
    enableHooks()
    return origNetStart(msg, unreliable)
end

mark_special(netStartHk)
spoof_debug_info(netStartHk, origNetStart)
net.Start = netStartHk

set_push_string_callback(function(str)
    disableHooks()
    -- We'll do random checks here since it's safe.
    local hk = rawget(_G, "hook")
    local call = rawget(hk, "Call")

    if call ~= hookCallHk then
        conPrint("hook.Call was modified!!!")
        spoof_debug_info(hookCallHk, call)
        rawset(hk, "Call", hookCallHk)
    end
    enableHooks()
end)
]]