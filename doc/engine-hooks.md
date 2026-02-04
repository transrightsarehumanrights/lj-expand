# Engine hooks

LJE provides a key instrument, the engine call hook, which allows users to, at a low level,
intercept and modify any Lua call made by GMod. This is particularly useful for creating undetectable
systems like using hooks without the `hook` library, or adding concommands without the `concommand` library (highly detectable).

**Each script only gets one engine hook. To make multiple engine hooks for one script, you should just chain them together inside a single hook.**

## Overview

Engine hooks are called by LJE whenever GMod makes a Lua call. You can register a callback like so:

```lua
lje.vm.set_engine_call_hook(function(func, nargs, nresults, ...)
    if nargs > 0 then
        local name = select(1, ...)
        if func == lje.get_global("hook", "Call") and name == "CalcView" then
            -- Let us call it.
            -- NOTE: **NEVER** pcall or xpcall func, as this will make it detectable!
            local a, b, c, d, e, f = func(...)

            if a then
                a.fov = 144
            end
               
            lje.vm.handle_engine_call() -- Signals to the VM that we're going to handle this call.
            return a, b, c, d, e, f
        end
    end
end)
```

This example intercepts calls to `hook.Call` with the first argument being `"CalcView"`, and modifies the FOV of the returned view table to be 144.
To tell the VM that you are handling the call, you **MUST** call `lje.vm.handle_engine_call()` before returning. Otherwise, the call will be dispatched as normal.

Here is another example that intercepts console commands:

```lua
AddConsoleCommand("gilbhax_fart", "Hi!", 0)

lje.vm.set_engine_call_hook(function(func, nargs, nresults, ...)
    if nargs > 0 then
        if func == lje.get_global("concommand", "Run") then
            local ply, cmd, args, argStr = ...
            if cmd == "gilbhax_fart" then
                lje.con_printf("Fart completed.")
                lje.vm.handle_engine_call() -- Signals to the VM that we're going to handle this call.
                return true -- This mimics the original concommand.Run return value, which is true if successful.
            end
        end
    end
    
    -- You do nothing if you don't want to handle it. It will then be dispatched as normal.
end)
```

This example intercepts calls to `concommand.Run`, and if the command is `"gilbhax_fart"`, it prints a message to the console and blocks the original function call,
returning `true` to indicate success (this is specific to `concommand.Run`, which returns a boolean indicating success).

All engine hooks are marked special, so they won't be detectable or visible to the called function, instead it will appear as if it was called normally via
the engine.

There are many things to note about engine hooks:
- They override the original call. You **MUST** dispatch the original function if you are handling the call, otherwise return `true` to pass it through.
- nargs *can* be 0, in which case there are no arguments to read.
- nreturns being -1 means that the function is being called in a vararg context, and you should return all results.
- You can return multiple values from the hook, which will be passed back to the original caller

And most importantly:
**DO NOT** `pcall` OR `xpcall` THE PASSED FUNCTION INSIDE THE HOOK. This will make it detectable since an anti-cheat can simply
error inside that function and then wait for a `OnLuaError` hook to be called to see if the error was caught correctly. Pcalling
the function inside the hook will break this behavior and make it detectable.

Instead, you will want to call it directly. Anything else may be pcalled at will, but not the function itself. If your
engine hook happens to error, there may be instability or crashes, so be careful. However, LJE will sink your errors to prevent
detection as much as possible.

If, for example, you're calling `hook.Call`, any hook that errors will naturally propagate the error up to the engine, even
if you manually called it from the engine hook. This is expected behavior and is not detectable, and it looks exactly like how GMod would behave
if there was no engine hook.

## Blocking a call that returns values

Sometimes, you may want to block a call entirely, but it may need to return values. In this case, you **must** supply
return values according to `nresults`. If `nresults` is -1, you can return any number of values. Otherwise, you risk
instability or crashes since the engine won't get the expected number of return values.

### Stealth considerations
When creating engine hooks, you must be very careful to avoid creating detectable behavior.

You also should ensure that you do not accidentally create an oracle that reveals your engine hook. For example,
if you are blocking a call to `matproxy.Call` (which runs a user-defined Lua function), and you **don't** check
for LJE involvement or an LJE caller, you may create a stealth leak.

An anti-cheat could create a material with a proxy that calls a function, and then use a C function (like `surface.DrawLine`) which
may trigger the material proxy to be called. If your engine hook blocks the call to the proxy function, the anti-cheat can
detect that the proxy function was never called, revealing your engine hook.

Fixing this is a simple matter of checking for LJE involvement, with functions like `lje.env.is_lje_frame` or `lje.env.is_lje_involved`.
This way, you can ensure that your engine hook only blocks calls when LJE is involved, preventing stealth leaks.