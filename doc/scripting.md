# Scripting

Table of Contents
- [Entrypoints](#entrypoints)
- [Example script structure](#example-script-structure)
- [Dependencies](#dependencies)
- [Writing safe scripts](#writing-safe-scripts)
- [API Reference](#api-reference)

LJE executes units of startup and pre-init code as "scripts," which are folders in `%USERPROFILE%\.lje_scripts`.
To make it as seamless as possible, LJE provides a unified environment for scripts to run in, which mimics the global environment as closely as possible.
The only differences are:

- `lje` table: This table contains all LJE-specific functionality, such as environment management, logging, and utility functions.
- `_L` table: This is a circular reference *back* to the scripting environment.
- `_G` table: This is a reference to the actual GMod global environment, useful for detouring or accessing global functions directly.
- No Lua-based API functions included: LJE does not include any Lua-based API functions (e.g., `hook.Add`, `timer.Create`, etc.) in the scripting environment. This is to prevent easy detection of LJE via these functions.

Otherwise, everything else is identical and the entire C-based API is available as normal.
You can monkeypatch/share global functions and variables, and other scripts will see the changes.
To provide library behavior, LJE includes a load order system that allows scripts to specify dependencies on other scripts.

Scripts first declare their metadata in a `info.toml` file, which is parsed by LJE to determine the script's name, version, author and dependencies.

Here is the structure of an `info.toml` file, taken from [gilbhax](https://github.com/yogwoggf/gilbhax):
```toml
# info.toml allows you to specify metadata about your script.

[script]
name = "gilbhax"
version = "1.0.0"
author = "yogwoggf"
url = "https://github.com/yogwoggf/gilbhax" # Not required, in fact not even used, but can be helpful for automation/documentation.
dependencies = [
    "Eyoko1.ljeutil"
]
```

## Entrypoints

There are two entrypoints for scripts: `main.lua` and `preinit.lua`.
- `main.lua`: This file is executed during the startup phase of LJE. This is after `init.lua` has executed, but before any other scripts or addons have loaded. This is the main entrypoint for most scripts.
- `preinit.lua`: This file is executed during the pre-init phase of LJE. This is before `init.lua` has executed, and is useful for scripts that need to set up detours or modify the environment before any other code runs.

Note, `main.lua` means any code from the server *could* have executed before it. Be cautious in what you trust from the global environment in `main.lua`. 
`preinit.lua` is safer in this regard, but not many things are available at that point.


## Example script structure

Here is an example structure of a script named `MyScript`:
```
.lje_scripts
└── MyScript
    ├── info.toml
    ├── main.lua
    └── preinit.lua
```

## Dependencies

Dependencies are specified in the `info.toml` file under the `dependencies` field.
When a script is loaded, LJE will ensure that all dependencies are loaded first.
This allows scripts to share functionality and build upon each other like GMod addons, where one script provide sharing via the global environment.

# Writing safe scripts

When writing scripts for LJE, it is important to consider the following best practices to ensure compatibility and safety:
- Wrap runtime code in `lje.hooks.disable` and `lje.hooks.enable` to avoid detection via debug hooks.
- Avoid modifying global state unless absolutely necessary.
- Use `lje.env.save_random_state()` and `lje.env.restore_random_state()` when using `math.random` to avoid PRNG state detection.
- Test scripts thoroughly to ensure they do not cause instability or detection issues.
- If you are writing code that operates with *objects*, use metatable copies in `cloned_mts` and *do not* use `object:method()` syntax sugar. Instead, use `cloned_mts.type.method(object, ...)` syntax to avoid metatable detection.
- To ensure greater metatable safety, consider using `lje.env.disable_metatables()` and `lje.env.enable_metatables()` around code that interacts with tables/userdata that may have custom metatables.

Note that all entrypoint code is automatically wrapped in `lje.hooks.disable` and `lje.hooks.enable`, so you do not need to do this manually in `main.lua` or `preinit.lua`.
The random state is also automatically saved and restored around entrypoint code.

## Writing safe detours

When writing detours for functions, it is important to ensure that the detour is as undetectable as possible.
This is typically very simple if you wrap your detour code in `lje.hooks.disable` and `lje.hooks.enable`, as this will prevent debug hooks from seeing the detour.
Additionally, spoofing makes it even more difficult to detect detours at face value.

Here is a simple detour that is detectable:
```lua
_G.blah = lje.detour(_G.blah, function(...)
    print("Detoured!")
    return _G.blah(...) -- This is bad!
end)
```

This is how you would write it safely with LJE:
```lua
local original_blah = _G.blah
_G.blah = lje.detour(original_blah, function(...)
    lje.hooks.disable()
    print("Detoured!")
    lje.hooks.enable()
    return original_blah(...) -- This is safe!
end)
```

Callers cannot access your detour frame's locals or upvalues, and debug hooks will not see the detour at all. Additionally, it wont even
be present in `debug.getinfo` calls or tracebacks, making it very difficult to detect.

If you are using object methods, ensure you use the correct syntax to avoid metatable detection:
```lua
local P = cloned_mts.Player -- Shorthand
local original_SetHealth = P.SetHealth

FindMetaTable("Player").SetHealth = lje.detour(original_SetHealth, function(self, health)
    lje.hooks.disable()
    lje.con_printf("Setting health to $red{%d}", health)
    lje.hooks.enable()
    return original_SetHealth(self, health) -- Safe!
end)
```

If you do anything else with the object, use `lje.env.disable_metatables()` and `lje.env.enable_metatables()` around the code to avoid any potential  metatable detection as well.
And, for math.random, use PRNG state capture functions if necessary.

```lua
--- ... detour code ...
lje.env.save_random_state()
math.randomseed(os.time())
local rand = math.random(1, 100)
-- do something with rand
lje.env.restore_random_state()
```

# API Reference

The `lje` table contains all LJE-specific functionality for scripts.

## base functions

- `lje.include(path: string, execute: boolean = true) -> any`: Includes and optionally executes a Lua file from the script's folder. The path is relative to the script's root folder. If `execute` is false, the file is only loaded and not executed.
- `lje.con_print(message: string)`: Prints a message to the LJE console with LJE formatting.
- (lua) `lje.con_printf(format: string, ...)`: Prints a formatted message to the LJE console with LJE formatting. You can wrap text in color tags like `$red{this is red}`.
- (lua) `lje.detour(original: function, detour: function) -> function`: Detours a function. The detour function should call the original function as needed. Returns the detoured function.
- (lua) `lje.require(path: string) -> any`: Requires a Lua module from the script's folder. The difference between this and `lje.include` is that `lje.require` caches the module, so subsequent calls to `lje.require` with the same path will return the cached module instead of reloading it.
- (lua) `cloned_mts`: A table containing copies of known good metatables for various object types. This is useful for interacting with objects without triggering metatable-based detections.

## `func` API

- `lje.func.spoof(spoof: function, target: function) -> function`: Spoofs a function to appear as another function.
- `lje.func.is_spoofed(func: function) -> boolean, function`: Checks if a function is spoofed. If it is, returns true and the original function.
- `lje.func.mark_special(func: function)`: Marks a function as special, which means it is protected from detections like debug hooks (although only the code itself, not other functions it calls) and stack inspections.

### Note

Typically `spoof` and `mark_special` are used together to create undetectable functions. There are internal reasons for splitting them up like this, but from a scripting perspective, you can think of them as a single operation.

## `hooks` API

- `lje.hooks.disable()`: Disables debug hooks.
- `lje.hooks.enable()`: Enables debug hooks.
- `lje.hooks.ignore_fn_once(func: function)`: Ignores the next call to the specified function in debug hooks. This is useful for detours that call something else at return, like `unpack` or similar.

### Note

These functions are protected from debug hooks themselves, so you can safely call them from detours or other sensitive code.

## `env` API

- `lje.env.set(tbl: table)`: Sets the scripting environment to the specified table. This is typically only used internally by LJE.
- `lje.env.get() -> table`: Gets the current scripting environment table. This is equal to `_L`.
- `lje.env.disable_metatables()`: Disables metatable resolution globally. This is useful for code that interacts with tables/userdata that may have custom metatables.
- `lje.env.enable_metatables()`: Enables metatable resolution globally. This should be called
- `lje.env.save_random_state()`: Saves the current PRNG state.
- `lje.env.restore_random_state()`: Restores the previously saved PRNG state.
- `lje.env.current_script() -> string`: Returns the name of the currently executing script.
- `lje.env.is_lua_involved(offset: integer = 0) -> boolean`: Checks if Lua code is involved in the current call stack. The optional `offset` parameter allows you to skip a number of frames from the top of the stack.

## `util` API

- `lje.util.get_bytecode_hash(func: function) -> number`: Returns the bytecode hash of a function. This is useful for verifying the integrity of functions. It is a simple 32-bit integer hash of the function's bytecode.
- `lje.util.get_call_stack() -> table`: Returns a table representing the current call stack. Each entry in the table contains information about a stack frame, similar to what `debug.getinfo` returns. Very expensive, use sparingly.
- `lje.util.get_registry() -> table`: Returns the actual Lua registry table. Use with caution.
- `lje.util.set_push_string_callback(callback: function)`: Sets a callback function that is called whenever a string is pushed onto the Lua stack. Its use is very limited, and primarily intended for advanced use cases. It is more or less like `debug.sethook` but for string pushes only.
- `lje.util.set_script_hook_callback(callback: fun(name: string, content: string) -> boolean)`: Sets a callback function that is called whenever a script is about to be executed. The callback receives the script's name and content, and should return true to allow execution or false to block it. This is useful for implementing custom script loading logic or security checks.

## `gc` API

- `lje.gc.get_total() -> number`: Returns the total memory used by Lua in bytes.
- `lje.gc.set_total(bytes: number)`: Sets the total memory used by Lua to the specified number of bytes. This is primarily useful for debugging only. Do not use in production code.
- `lje.gc.run_full_gc()`: Simple shorthand for running a full garbage collection cycle.

## `vm` API

- `lje.vm.patch_bytecodes()`: Patches relevant bytecode instructions in the current Lua VM. This is used internally, do not call it manually.

## `data` API

Data is written to `%USERPROFILE%\.lje_data`. They are flat blobs of data, each ending in `.dat` with no subdirectories allowed.
Funky characters in names are rejected to avoid path traversal.

- `lje.data.write(name: string, data: string)`: Writes data to a file with the specified name.
- `lje.data.read(name: string) -> string | nil`: Reads data from a file with the specified name. Returns nil if the file does not exist.