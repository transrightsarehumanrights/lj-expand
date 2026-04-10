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

This is what you will be detouring 99% of the time. C functions are implemented by the engine and have a single source of truth. They cannot be redefined or reinstantiated, so once you detour them, they will stay detoured until you restore them. This makes them much safer to detour, as you don't have to worry about your detour being bypassed by a redefinition or reinstantiation.

However, just like anything involving the client environment, **they can lead to detections**. The next section will cover detour stealth which is an entire topic in itself.

# Detour stealth

Any deviation from the original function will cause a difference in execution time. **This can lead to detections**.

## Early Returns

For example, say you detour `HTTP` to block certain requests. Naively, you might do something like this:

```lua
local originalHTTP = HTTP

local function httpHk(params)
  -- do checks to ensure
  -- params isnt invalid
  if isBad(params) then
    return originalHTTP(params)
  end
  
  if isBlocked(params) then
    return -- block the request
  end
  
  return originalHTTP(params)
end

_G.HTTP = lje.detour(originalHTTP, httpHk)
```

This will work, but there will be a significant time difference between any blocked request and a normal request.
Early returning completely skips the original function, which is what some anti-cheats look for.

This is still unsolved, but generally you *need* to call the original function no matter what, even if it is just to do some dummy work. For example, you could do something like this:

```lua
local originalHTTP = HTTP
local FAKE_HTTP_INPUT = {
  failed = function(reason) end,
  success = function(code, body, headers) end,
  method = "GET",
  url = "https://example.com",
}

local function httpHk(params)
  -- do checks to ensure
  -- params isnt invalid
  if isBad(params) then
    return originalHTTP(params)
  end
  
  if isBlocked(params) then
    return originalHTTP(FAKE_HTTP_INPUT)
  end
  
  return originalHTTP(params)
end

_G.HTTP = lje.detour(originalHTTP, httpHk)
```

Now every path of execution calls the original function, and there is no significant time difference between a blocked request and a normal request. This is much stealthier and less likely to be detected by anti-cheat.

This is fine, but once you add anything which adds considerable overhead, like logging or complex checks, you start to get into detection territory again.
Overhead is unavoidable, so LJE equips you with the timing functions in `lje.env` to hide this overhead. Instead, you pay it back later over multiple frames to avoid detections.

## Paying back overhead

Here is an example of how you might do this with HTTP again:

```lua
local originalHTTP = HTTP
local FAKE_HTTP_INPUT = {
  failed = function(reason) end,
  success = function(code, body, headers) end,
  method = "GET",
  url = "https://example.com",
}

-- These should be localized to avoid any initial global lookups.
local start_timing, end_timing = lje.env.start_timing, lje.env.end_timing
local function httpHk(params)
  -- do checks to ensure
  -- params isnt invalid
  start_timing() -- Begin counting our detour overhead.
  if isBad(params) then
    end_timing() -- End timing and pay back the overhead we just added.
    return originalHTTP(params)
  end
  
  if isBlockedViaComplicatedCheck(params) then
    end_timing()
    return originalHTTP(FAKE_HTTP_INPUT)
  end
  
  end_timing()
  return originalHTTP(params)
end

_G.HTTP = lje.detour(originalHTTP, httpHk)
```

Now, the overhead of our if checks and any other code we add to the detour is hidden from anti-cheat, as it is paid back later and not in a vacuum of detection code.
This is far stealthier and less likely to be detected, and you **should do this for all detours that have considerable overhead**.

## GC stealth

Ok, so lets say you need to push a copy of the parameters to a queue for logging/processing later.
This would lead to a *noticeable* GC memory usage spike that an anti-cheat could easily detect.

To avoid this, you can use LJE's GC stealth functions with [lje.gc.begin_track()](/api/gc#begin_track) and [lje.gc.end_track()](/api/gc#end_track) to track all GC allocations in the detour.
They're not "paid back", but just hidden from the GC total and thus anti-cheat. Ideally, you want to minimize the amount of GC allocations in your detours, but if you need to do it, this is how you can do it stealthily.

```lua
local originalHTTP = HTTP
local FAKE_HTTP_INPUT = {
  failed = function(reason) end,
  success = function(code, body, headers) end,
  method = "GET",
  url = "https://example.com",
}

local QUEUE = {}
local function pushForProcessing(params)
  local copy = {}
  for k, v in pairs(params) do
    copy[k] = v
  end
  table.insert(QUEUE, copy)
end

-- These should be localized to avoid any initial global lookups.
local start_timing, end_timing = lje.env.start_timing, lje.env.end_timing
local begin_track, end_track = lje.gc.begin_track, lje.gc.end_track
local function httpHk(params)
  -- do checks to ensure
  -- params isnt invalid
  start_timing() -- Begin counting our detour overhead.
  start_track() -- Start tracking GC allocations for this detour.
  if isBad(params) then
    end_track() -- End GC tracking and hide any GC allocations we just did.
    end_timing() -- End timing and pay back the overhead we just added.
    return originalHTTP(params)
  end
  pushForProcessing(params) -- This function does GC allocations, but they're hidden from anti-cheat because of the GC tracking.
  if isBlockedViaComplicatedCheck(params) then
    end_track()
    end_timing()
    return originalHTTP(FAKE_HTTP_INPUT)
  end
  
  end_track()
  end_timing()
  return originalHTTP(params)
end

_G.HTTP = lje.detour(originalHTTP, httpHk)
```

Now, this detour is resilient to timing, GC and early return detections, and is much stealthier than a naive detour. You should strive to make all of your detours as stealthy as possible to avoid detections, and LJE provides you with the tools to do so.
Of course, there are still ways to detect some detours (behavioral detections, etc), but following these practices will make your detours much more resilient to detection.

The best detour is no detour at all, so always consider if an engine call hook or some other instrument could achieve your goal before resorting to detours.