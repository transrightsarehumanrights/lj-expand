---
id: detours
title: Detours
---

# Detours

Detours are your main instrument in LJE to change how certain functions behave. You can replace any function with your own implementation, and then
call it whenever you want. This is invaluable for a multitude of reasons, but you really want to know how to be safe and stealthy with it.

**Always** detour in `preinit.lua`, as **nothing** else runs during this time and you can freely access/modify any function without worrying about detections yet.
Detouring in `main.lua` requires a lot more caution and care. Arbitrary code could have ran already and stored references to the original function.

Before we demonstrate how to use detours, we need to understand what *not* to detour.

## Bad: Fast functions

Detouring fast functions, anything that when printed is printed as: `function: builtin#<id>`, is a bad idea.

A brief but non-exhaustive list of these functions includes:
- `pairs`
- `ipairs`
- `next`
- `assert`
- `error`
- `pcall`
- `xpcall`

Technically speaking... you could detour `setfenv` or `getfenv` as well, but
there's usually no purpose to that. The main problem at play here is avoiding detouring commonly called functions as they will *noticeably* slow down, enough to be detected by anti-cheat. If you want to detour these functions, you need to be cautious about how you do it, and make sure to restore the original function as soon as possible.

## Bad: Lua-defined functions

It is 100% possible to detour Lua-defined functions safely, but you should know that Lua functions have no single source of truth.

They can be reinstantiated or redefined and your detour goes away because it is an entirely new function. This is common with hook libraries,
so detouring `hook.Run` for example would fail because a hook library can just rerun `hook.lua` or make its own hook.Run and your detour goes squat. Additionally, it can call back to your detour function and cause an infinite loop if you aren't careful. If you want to detour Lua-defined functions, you need to be cautious about how you do it, and make sure to restore the original function as soon as possible.

Really need to hook something implemented in Lua? Consider using [engine call hooks](/guides/hooks#engine-call-hooks), a precise instrument in LJE that allows you to hook whenever the engine itself is calling a Lua function, which is much more stealthy and reliable.

## Good: C functions

This is where most detours will be useful anyway. Detouring C functions is safe and stealthy, and does not suffer from any of the previous pain points mentioned. You can also generally spend more time in the detour function itself without worrying about performance, as C functions are generally not called as frequently as fast functions or Lua-defined functions.

Taking too long though will open you up to timing detections, so be mindful of that.

## Examples

### Detouring `system.GetCountry`

```lua
-- Changes our country to international waters.

_G.system.GetCountry = lje.detour(_G.system.GetCountry, function()
    return "XZ" -- Country code for international waters, because why not?
end)
```

### Detouring `render.Capture`

This is taken from `gilbhax`, but shows you how to use detours for a realistic purpose.
```lua
-- Hooks render.Capture to determine if anyone is trying to take a screenshot

local screengrab = {}
local origCapture = render.Capture

screengrab.last_screengrab_time = 0
screengrab.threshold = 10 -- seconds

function screengrab.is_screengrab_recent()
    return (SysTime() - screengrab.last_screengrab_time) <= screengrab.threshold
end

function screengrab.get_time_since_last_screengrab()
    return SysTime() - screengrab.last_screengrab_time
end

local function captureHk(tbl)
    screengrab.last_screengrab_time = os.clock()
    return origCapture(tbl)
end

_G.render.Capture = lje.detour(origCapture, captureHk)

return screengrab
```

### Detouring `HTTP`

```lua
local urls = lje.require("config/urls.lua")
local origHttp = HTTP
local function httpHk(params)
    lje.gc.begin_track() -- Only necessary if it generates noticeable GC pressure.
    if not params then
        lje.gc.end_track()
        return origHttp(params) -- if it's not a table, just call the original function
    end
    
    local url = rawget(params, "url") or "" -- ALWAYS write defensive code, errors are noticeable.
    if type(url) ~= "string" then
        url = tostring(url)
    end

    -- Logs and blocks HTTP requests.
    lje.con_printf("[HTTP] HTTP request to URL: $yellow{%s}", url)
    if not urls.is_url_allowed(url) then
        lje.con_printf("[HTTP] Blocked HTTP request to URL: $red{%s}", url)
        lje.gc.end_track()
        return true -- make them think it was sent
    end

    lje.gc.end_track()
    return origHttp(params)
end

_G.HTTP = lje.detour(origHttp, httpHk)
```

## Technical Details

LJE creates a side structure for each `GCfunc` in the VM, `LJEfunc`, which allows the VM to spoof function objects.
That's right, there *is* no actual detouring performed. We replace the function with your hook, and everything calls that hook.

It's fairly high-performance since we don't detour anything. However, JIT is enabled/disabled for the hook depending if the original function supports it or not.
To evade detection, every possible means of detection is simply just replaced with the original function at runtime, so `debug.getinfo` will return the hook, likewise calling `getinfo` on the hook itself returns the info of the hook's original function.
Many functions are modified specifically to simply just replace the target function with the original one, with the rest of the functionality untouched. This makes it virtually impossible for anyone to operate on the hook itself or introspect the hook.

Also, all the debug functions forcibly have had their perception of the call stack modified to ignore any spoofs, so the hook will never appear in the stack no matter what the function or case may be.