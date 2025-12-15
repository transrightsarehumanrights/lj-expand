# lj-expand - stealthy lua execution for 64-bit gmod 🪛

lj-expand (LJE) is a custom LuaJIT fork for Garry's Mod's 64-bit branch, focused on stealth and safety when executing arbitrary Lua code.
Simply put, LJE makes it significantly harder for anti-cheat systems to detect unauthorized code execution, while also providing
mechanisms to run and write secure Lua code that cannot be easily tampered with or inspected, while retaining typical patterns and idioms.

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
I don't condone cheating, or exploiting a server. I do however believe that you should have the freedom to audit and run your own code on your own machine.

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
- Bytecode patching
  - Certain bytecode instructions are patched at the VM level to improve stealth.
  - This includes instructions that might bypass spoofing or reveal unauthorized code.
  - Not all instructions are patched, only the most relevant ones to maintain performance and stability.
- Hash manipulation
  - Some anticheats may detect even the best spoofed functions by checking function hashes by assigning them to tables as keys.
  - LJE has overriden the hashing algorithm to ensure that spoofed functions have the same hash as their original counterparts.
- Debug hook stack adjustments
  - It is possible to determine if a detour is present by checking the stack depth and name resolution in a debug hook.
  - This is also patched in LJE at the VM level to ensure that hooks see the expected stack depth and function names.
- Global metatable hardening
  - During any LJE code execution, all metatables globally are (depending on situation) temporarily disabled.
  - This makes it very difficult to detect LJE code execution via metatable hooks like `__newindex` or `__index`.
- Probably more...

These functions are modified to leverage the above primitives:
- [x] debug.getinfo
- [x] debug.getlocal
- [x] debug.getupvalue
- [x] debug.sethook
- [x] debug.traceback
- [x] tostring
- [x] string.dump
- [x] jit.util.funcinfo
- [x] jit.util.funcbc

And the following internal functions, which cover most of the debug functionality:
- [x] lj_debug_frame
- [x] lj_debug_funcname
- [x] callhook
- [x] lj_func_*

# Scripting

Scripting in LJE is a bit bare, and the API is also particularly unstable at the moment, but you can create your own projects with LJE already.
To get started, create a new folder in the `%USERPROFILE%\.lje_scripts\` directory. Inside that folder, create a `main.lua` file. This file will be executed
when the game loads startup Lua files (not pre-init).

Then, add a `info.toml` file. This is a simple TOML file that describes your script. An example `info.toml` file:
```toml
# info.toml allows you to specify metadata about your script.

[script]
name = "gilbhax-utils"
version = "1.0.0"
author = "yogwoggf"
dependencies = []
```

Dependencies are specified in a format of `author.name`. So for that example you'd refer to it as `yogwoggf.gilbhax-utils` in other scripts.
Dependencies are automatically loaded before your script is executed.

The entire environment **is secured**, and contains no Lua functions by default. If you need to use one, you will probably need to rewrite it since any external Lua function
can detect the presence of LJE. Every GMod C-implemented API function is in the environment by default, so you can use those freely.

The API is fairly simple, the two most important functions are `lje.include` and `lje.detour`.
- `lje.include(path: string)`: Includes and runs a Lua file from the script's folder. The path is relative to the script's root folder.
- `lje.detour(target: function, detour: function): function`: Detours a target, returns the detour function which is fully spoofed to appear as the target function.

The rest are undocumented, but you can see them [here](https://github.com/yogwoggf/lj-expand/blob/expansion/src/lj_expand_lib.c#L263).
There is unfortunately no system for hooking GMod hooks yet, so you will manually need to do it, like in [gilbhax](https://github.com/yogwoggf/gilbhax).

# Licensing

No license file on purpose since it is a fork of LuaJIT, which is MIT licensed anyways, which means this project is also MIT licensed.