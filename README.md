# lj-expand

A highly experimental method of unauthorized Lua execution within Garry's Mod. It works by compiling a custom LuaJIT VM and selectively
remapping Garry's Mod's LuaJIT functions to it, allowing for VM-level access and manipulation of the game's Lua state. This means it is
possible to essentially mask unauthorized Lua code and evade detection by anti-cheat systems that rely on using introspection functions or
debugging hooks to facilitate detection. Of course, this is still highly experimental and may not work in all scenarios. Unsafe code will always
be possible, so unfortunately there is no perfect solution. It is however much more complicated to detect and mitigate against since this project
operates at a lower level than traditional methods.

# Installation

**DO NOT INSTALL THIS IN THE GARRY'S MOD FOLDER.** Instead, keep it in a separate folder somewhere else on your computer. Be aware that you will need
to manually update this folder. Right now, there is no versioning as it is very early in development. You will need to manually download artifacts.

1. Look at the latest commit for a green checkmark icon. Click on it to go to the artifacts page.
2. At the left sidebar, click on "Summary"
3. Click on the `lje-***.zip` file to download the latest build.
4. Extract the zip file somewhere safe.
5. Setup the `GMOD_PATH` environment variable to point to the GMod 64-bit executable in `bin\win64\gmod.exe`. **DO NOT** point it to the `gmod.exe` in the root folder.
6. From now on, you can run `lje-launcher.exe` to launch Garry's Mod with lj-expand.

## Disclaimer
I don't condone cheating. I do however believe that you should have the freedom to audit and run your own code on your own machine.

## Mitigations
The main primitive that lj-expand adds are:
- Spoofing
  - Functions that are spoofed will appear and act as their original function, making it difficult to detect that they have been detoured.
  - This includes stack traces, debug info, bytecode and more.
- Special marks
  - Special marks are added to functions that need stealth.
  - These marks are read by the custom VM to hide unauthorized code from introspection functions.
  - Currently, special functions get hidden from the stack trace and debug hooks.
- Debug hook manipulation
  - Completely enable/disable debug hooks on a per-function basis.
  - Tailcall hook bypassing, specifically allows for the `return unpack(foobar)` idiom in detours.
- GC spoofing
  - Extra metadata is associated with functions to enable the VM to identify our own functions and treat them differently.
  - This does not interfere with the GC total, making it difficult to detect this extra metadata.
  - It also means the initialization GC total is equal to the vanilla GC total.
  - Pre-init total is spoofed to match vanilla as well. This means it's difficult/impossible to detect the presence of lj-expand via collectgarbage checks at pre-initialization.
  - Technically, you can try it after initialization, but at that point there are nondeterministic factors that can affect the GC total (material loading times, http requests, etc). 
- Call stack authorization
  - LJE functions can verify whether they were called from authorized code or not.
  - This allows for stuff like PostRender hooks to be restricted to when the engine is calling them, preventing anti-cheat code from executing them and detecting unauthorized behavior.

These functions are modified to leverage the above primitives:
- [x] debug.getinfo
- [x] debug.getlocal
- [x] debug.getupvalue
- [x] tostring
- [x] string.dump
- [x] jit.util.funcinfo
- [x] jit.util.funcbc

And the following internal functions, which cover most of the debug functionality:
- [x] lj_debug_frame
- [x] lj_debug_funcname
- [x] callhook
- [x] lj_func_*
