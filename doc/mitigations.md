# Mitigations

LJE is designed around stealth and evasion of Lua-based AC systems. It assumes that, the environment is compromised and hostile, and takes extensive measures to ensure that its presence is not detectable.
There are benefits to this approach, such as being able to run alongside normal Lua scripts without many performance issues or proxying problems (a la CitizenHack). However, it also means that LJE must take extra precautions to avoid detection.

To achieve this, LJE operates on the idea that there are an enumerable set of primitives that can be used to detect unauthorized code execution or modifications to the Lua environment.
LJE simply implements a variety of mitigations to counteract these detection methods, in turn making it difficult to detect LJE's presence even in complicated anti-cheat systems.

Some of the key mitigations implemented in LJE include:
- [Bytecode patching](#bytecode-patching)
- [Hash manipulation](#hash-manipulation)
- [Stack spoofing](#stack-spoofing)
- [Metatable remapping](#metatable-remapping)
- [PRNG state isolation](#prng-state-isolation)
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

# Metatable remapping

LJE adds new behavior to both JITed and interpreted code to ensure any LJE code using metatables is properly remapped to safe and known-good metatables.
This prevents anti-cheat systems from detecting unauthorized code execution by modifying global metatables and checking for unexpected behavior.

This is also implemented at the trace level to ensure JITed code works correctly with remapped metatables.

# PRNG state isolation

`math.random` and `math.randomseed` both use a new LJE-specific PRNG state, isolated from the global Lua state. This prevents anti-cheat systems from detecting unauthorized code execution by seeding the PRNG with known values
and checking for unexpected random values. It is also implemented at the trace level to ensure JITed code uses the correct PRNG state.

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