# Mitigations

LJE is designed around stealth and evasion of Lua-based AC systems. It assumes that, the environment is compromised and hostile, and takes extensive measures to ensure that its presence is not detectable.
There are benefits to this approach, such as being able to run alongside normal Lua scripts without many performance issues or proxying problems (a la CitizenHack). However, it also means that LJE must take extra precautions to avoid detection.

To achieve this, LJE operates on the idea that there are an enumerable set of primitives that can be used to detect unauthorized code execution or modifications to the Lua environment.
LJE simply implements a variety of mitigations to counteract these detection methods, in turn making it difficult to detect LJE's presence even in complicated anti-cheat systems.

Some of the key mitigations implemented in LJE include:
- [Bytecode patching](#bytecode-patching)
- [Hash manipulation](#hash-manipulation)
- [Stack spoofing](#stack-spoofing)
- [Metatable hardening](#metatable-hardening)
- [PRNG state capture](#prng-state-capture)
- [GC manipulation](#gc-manipulation)
- [Function spoofing](#function-spoofing)
- [JIT blacklisting](#jit-blacklisting)
- And probably more, but these are the most relevant ones.

# Bytecode patching

Certain bytecode instructions are implemented in assembly to speed up execution. However, some of these instructions can be used to bypass spoofing or reveal unauthorized code.
In particular, `BC_ISEQV` and `BC_ISNEV` can be used to compare function pointers directly, which can reveal detours or spoofed functions.
To counteract this, LJE patches these instructions at the VM level to ensure that they behave as expected when comparing spoofed functions.

However, not all instructions are patched, only the most relevant ones to maintain performance and stability.

# Hash manipulation

Any spoofed function can be detected by checking its hash. Some anti-cheat systems may assign functions to tables as keys, which will reveal the presence of spoofed functions if their hashes do not match the original.
```lua
local t = {}
t[original_function] = true
if not t[supposedly_original_function] then
    print("Detected spoofed function!")
end
```

LJE overrides the hashing algorithm to ensure that spoofed functions have the same hash as their original counterparts.

# Stack spoofing

There are many places where Lua directly gets to do things or inspect the call stack. For example, debug hooks can inspect the stack depth and function names, which can reveal the presence of detours.
Additionally, functions like `debug.getinfo`, `debug.getlocal`, and `debug.getupvalue` can be used to inspect the call stack and reveal unauthorized code execution.
LJE patches these functions at the VM level to ensure that they see the expected stack depth and function names.

Additionally, LJE modifies these at the root (e.g: `lj_debug_frame`, `lj_debug_funcname`, `callhook`, etc.) to ensure that all debug functionality is covered.
A technique I like to call "frame stitching" is also employed to ensure that any gaps in the stack caused by detours are hidden, and LuaJIT can resolve function names across tailcall frames and other tricky situations.

# Metatable hardening

During any vacuum of LJE code execution, all metatables globally are (depending on situation) temporarily disabled. This makes it very difficult for anti-cheat systems to pass in
a modified table/userdata with a custom metatable that hooks into `__newindex` or `__index` to detect LJE code execution.

All metatables in this state are simply not resolved and the raw table/userdata is used instead. If a metatable is required, LJE stores a copy of original known good metatables and uses them raw instead of syntax sugar.

# PRNG state capture

If any LJE script uses `math.random(seed)`, it can step and modify the global PRNG state. This can be detected by anti-cheat systems that monitor the PRNG state for unexpected changes.
To counteract this, LJE captures and restores the PRNG state around startup code and pre-init code. But, scripts can also manually capture and restore the PRNG state via `lje.env.save_random_state()` and `lje.env.restore_random_state()`.

This makes it so that any changes to the PRNG state made by LJE scripts are not detectable by anti-cheat systems, which is very important if a script uses `math.random` at pre-init, because the first `math.random` call in `init.lua` is constant.

# GC manipulation

GC is still a problematic area for stealthy code execution. If an LJE script creates a lot of garbage, it can trigger a GC cycle that can be detected by anti-cheat systems.
This is still a problem in the works, but LJE attempts to minimize garbage creation as much as possible. Additionally, LJE can manually trigger GC cycles at safe points to ensure that any garbage created by LJE scripts is collected before anti-cheat systems can detect it.

# Function spoofing

The core principle of LJE is function spoofing. I believe LJE's approach is fairly novel, but we do not create C functions or modify existing Lua functions. Instead, we simply modify *many* places in the LuaJIT VM to ensure that detours and spoofed functions are indistinguishable from original functions.
This way, detours have a very little footprint and almost no overhead, and spoofed functions behave exactly like original functions in every way.

Spoofs are implemented by keeping a linked list of spoofed functions, and modifying many places in the VM to check this list when resolving function pointers, getting function names, getting function info, etc.
The linked list makes it very efficient to check for spoofed functions, and the modifications to the VM ensure that spoofed functions appear and equal original functions in every way.

# JIT blacklisting

If there is a trace in progress, LJE will look for any patterns that may bypass spoofing or reveal unauthorized code execution. If such a pattern is detected, LJE will blacklist the trace, preventing it from being recorded.
This ensures that any JIT-compiled code cannot be used to detect LJE's presence, at the cost of little performance overhead.