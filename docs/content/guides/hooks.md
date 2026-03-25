---
id: hooks
name: Hooks
---

# Hooks

LJE tries not to know what goes on in GMod's Lua specifically, so using hooks is something you need to do yourself.
However, there is a strong community library built to allow for undetectable hooks that you probably can just use instead.

## ljeutil

You want to add [ljeutil](https://github.com/Eyoko1/ljeutil) into your `~/.lje_scripts` folder so it loads. Then declare a dependency on it in your `info.toml`. For example:
```toml
[script]
name = "example"
version = "1.0.0"
author = "example"
dependencies = ["Eyoko1.ljeutil"]
```

You can consult ljeutil's repository for more info, but using it is fairly simple. Here's how to render safely using a hook provided by ljeutil:
```lua
hook.pre("ljeutil/render", "gilbhax.ui", function()
    cam.Start2D()
    render.PushRenderTarget(lje.util.rendertarget)
        surface.SetFont("ChatFont")
        surface.SetTextPos(10, 10)
        surface.SetTextColor(100, 255, 100, 255)
        surface.DrawText("LJE ROCKS!!!")
    render.PopRenderTarget()
    cam.End2D()
end)
```

This is probably a common pattern you'll want to use, but it renders all of this in a way where anti-cheats cannot screengrab it.

For anything else, you can use the normal GMod hook name with `hook.pre/post` depending on when you want to run, here's an example from `gilbhax`:
```lua
hook.pre("CreateMove", "gilbhax.bhop", function(cmd)
    lje.gc.begin_track()
    if input.WasKeyPressed(KEY_P) then
        freecam.toggle()
    end
    
    bhop.run(cmd)
    aimbot.run(cmd)
    triggerbot.run(cmd, aimbot.target)
    lje.gc.end_track()
end)
```

## Engine call hooks

Engine call hooks are a *very* powerful instrument in LJE. You can use it to intercept *any* Engine → Lua call made in GMod.

This is a really useful tool for hooking into systems that rely on Lua-defined functions (e.g: `concommand`, even `hook.Run`) which are challenging and dangerous to detour.
Instead, you can hook those functions at the engine level, which is one layer above Lua and means you can hook these functions from the engine making it impossible to detect or mitigate.

To begin intercepting engine calls, you need to use [lje.vm.set_engine_call_hook](/api/vm#set_engine_call_hook) to make an engine call hook for the current script.
Here is an example of an engine call hook which we'll break down:
```lua
AddConsoleCommand("lje", "Says Yay!", 0) -- Needed so the engine knows it's a valid command

lje.vm.set_engine_call_hook(function(func, _, _, ...)
    if func == lje.get_global("concommand", "Run") then
        local _, cmdName, _, _ = ...
        lje.con_printf("$yellow{[interceptor]} Intercepted console command: %s", cmdName)
        if cmdName == "lje" then
            lje.con_printf("$yellow{[interceptor]} Yay!")
            lje.vm.handle_engine_call() -- Signal to LJE we're handling the call
            return true -- True is what the normal concommand handler returns
        end
    end
end)

-- Run `lje` in console and "Yay!" will be printed to LJE's console.
```

As you can see, this engine call hook intercepts the engine's call to `concommand.Run` without detouring it.
LJE invisibly redirects this call to your engine call hook without the engine knowing about it, so you can call [lje.vm.handle_engine_call](/api/vm#handle_engine_call) to handle it.

This function tells LJE that your hook is going to take over the engine call. From this point, you can run the original function again if you need to. In this example, we don't and instead
return `true` which is how `concommand.Run` signals success.

### Dispatching

Dispatching is when you need to call the original function yourself during an engine call hook. This is fine, but *you must* ensure it is not a protected call.
Protecting any function can lead to detection, so never wrap the original function in:
- `pcall`
- `xpcall`
- `ProtectedCall`

That being said, dispatching is optional and you can have LJE dispatch it like normal just by returning and not calling `lje.vm.handle_engine_call`.