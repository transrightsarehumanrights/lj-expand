---
id: how-it-works
title: How It Works
slug: /how-it-works
---

# Background

**Note:** This is a technical explanation of how LJE works. If you are not interested in the technical details, you can skip this section.

LJE is designed to be a secure environment to run Lua code in Garry's Mod, but we first have to define what "secure" means in this context.

LJE typically has to deal with "anticheats" or "detections" (adversarial scripts) as its main threat model. These scripts are designed to detect and prevent unauthorized code execution in Garry's Mod. LJE's goal is to provide a way to run Lua code without being detected by these adversarial scripts. (Cheating is not condoned, and is an entirely different problem, but the ability to run your own code on your own machine is a fundamental unalienable right.)

Adversarial scripts rely on **observable signals** to detect unauthorized code execution. They can range from simple checks for the presence of certain files or binary modules, to more complex behavioral analysis of the Lua environment. The goal of most software in this particular space is to minimize the number of observable signals that can be used to detect unauthorized code execution.

LJE takes a different approach. Instead of trying to minimize an endless list of observable signals, LJE focuses on **removing itself** from the observable environment entirely.

## Isolation

Isolation is how LJE achieves this. LJE creates a **separate Lua state** entirely, which is isolated from the main Lua state that Garry's Mod runs in. This means that any code running in LJE cannot be observed or interfered with by the main Lua state, and vice versa. It completely eliminates an entire class of observable signals, specifically ones focusing on the Lua environment itself, e.g: stack frames, locals, garbage collection, etc.

### VM manipulation

Creating a new Lua state is easy - figuring out how to expose the GMod APIs to it is the hard part. Some choose to expose a subset of the GMod APIs by manually implementing them in C++. This is a lot of work, and it is also very error-prone. Instead, LJE uses **VM manipulation**, which is simply the act of tricking the user of the LuaJIT API (in this case, GMod) into thinking that our isolated Lua state is actually the main Lua state. This allows us to expose the entire GMod API to our isolated Lua state without having to manually implement any of it.

Every single LuaJIT API function that GMod uses is reimplemented (not hooked!) in LJE to redirect calls properly to our isolated Lua state. This is why LJE is a fork of LuaJIT specifically tuned to create near-identical compilations of LuaJIT functions.

So, if a script calls `Entity` for example, it will be redirected to our isolated Lua state, and the `Entity` function will be called in that state. The result is then returned to the script as if it was called in the main Lua state. This is how LJE achieves full API exposure without having to manually implement any of it and the most important part is that it does this without **proxying**. Proxying can kill performance and is how regular Lua execution engines do it from the menu state.

### Shadow registry

GMod operates primarily through helper classes such as `CLuaTable` or `CLuaObject`. These were evidently written by someone who was not a fan of the Lua stack API. The main problem we tackle with this method is that they constantly `ref` and `unref` objects in the Lua registry, which is a global table that stores references to Lua objects. This means that if we were to create a new Lua state, it would have its own registry, and any references to objects in the main Lua state would be invalid in our isolated Lua state.

To fix this, LJE implements a **shadow registry**. This is a separate table that mirrors the main Lua state's registry, and it is used to store references to objects in the main Lua state. When an object is referenced in the main Lua state, it is also referenced in the shadow registry. When an object is unreferenced in the main Lua state, it is also unreferenced in the shadow registry. This allows us to maintain valid references to objects in the main Lua state while still being able to create a new Lua state. All of this machinery is carefully implemented in `lua_rawgeti`, `lua_rawseti`, and `luaL_ref` to ensure that the shadow registry is always in sync with the main Lua state's registry.

### Deep copies

In some cases, LJE may find it necessary to create a copy of an object referenced in the main Lua state. This is done through a highly optimized deep copy routine that uses LuaJIT's internals to avoid having to go through the public Lua API which can be much slower than using the internals directly. This is especially important for large tables (gamemode table) or metatables.

Speaking of metatables, LJE also has to deal with the fact that some objects in the main Lua state may have metatables that are not present in the isolated Lua state. This is especially true for objects that are created by the GMod API, such as `Entity` or `Player`. In these cases, LJE will create a copy of the metatable in the isolated Lua state and set it on the copied object. This ensures that any metamethods that are called on the object in the isolated Lua state will behave as expected. Additionally, all metatables are **locked** in the isolated Lua state to prevent any tampering with them. In any case - Lua functions are **never** allowed to pass the bridge between the main Lua state and the isolated Lua state, so metatable tampering which is often performed by adversarial scripts is not possible in LJE.

### Engine call hooks

To implement functionality similar to the standard `hook` library, LJE exposes the engine call hook - an instrument LJE scripts can use to, well, hook into any engine calls. Engine calls are any engine-initiated call into a Lua function, such as `hook.Call` or `concommand.Run`. All arguments are **proxied** into the isolated Lua state, and returning is strictly prohibited as it can break the isolation invariant. Note, proxying in LJE is only used for engine call hooks, and not for any other API calls. This is to avoid performance problems by never copying in large tables or objects that nobody will use.

Engine call hooks *can* be suppressed, this is useful for specific situations where an adversarial script is trying to detect unauthorized console commands for example. The engine will call `concommand.Run` with a LJE command, but suppressing the engine call hook will prevent the adversarial script from seeing it.

The main problem with engine call hooks however is modifying return values. This is an observable signal by function - not by inherent properties of the Lua state. This is why LJE does not allow returning from engine call hooks, as it would create an observable signal (i.e. an anticheat checking if a return value is modified).

### FFI and other advanced features

To maintain the isolation invariant, LJE prohibits entire parts of functionality that some people may be used to. This includes:
- Detours
- Changing engine call hook return values
- Accessing Lua variables from the main Lua state

Of these prohibited features, detours are the most important. Many people use detours to change the behavior of the game and to do things such as inspecting network messages or lying about identifying data points - but one thing is universally true: detours are **always** observable. They fundamentally involve the main Lua state, there is no reasonable way to implement them without touching the main Lua state. This is why LJE does not allow detours, and instead provides an escape hatch for people who want to use them: unsafe LJE. Unsafe LJE is an optional, but still recommended, combo of LJE and `lje-ffi`.

`lje-ffi` provides scripts the ability to endlessly extend the functionality of LJE by providing unfiltered, raw access to the process. For detours, scripts can write C code and compile it live into a detour that can be used to replace the missing functionality. This is the only way to implement detours in LJE, and it is also the only way to implement them in a way that is not observable. The reason for this is that C detours do not touch the main Lua state, and they have much less timing sensitivity than Lua detours. This means that they are much harder to detect, and they can be used to implement functionality that would otherwise be impossible in LJE.

This is also where the bulk of the power of LJE comes from. By providing a secure environment to run Lua code, and by providing an escape hatch for people who want to use detours, LJE allows people to do things that would otherwise be impossible in a regular Lua environment. Need to send convincing mouse and keyboard inputs? Use FFI and `SendInput`. Need to inspect network messages? Use FFI and detour `net.Send*`. Need to lie about the file system? Use FFI and detour `CBaseFileSystem`. While many other similar tools exist - the majority provide fixed, arbitrary interfaces to GMod that do not allow for the same level of flexibility and power that LJE provides.

## Conclusion

While that can not possibly cover all the technical details (and the many tearful lessons learned along the way), it should give you a good idea of how LJE works and why it is designed the way it is. The main takeaway is that LJE is designed to be a secure environment to run Lua code in Garry's Mod, and it achieves this by isolating itself from the main Lua state.