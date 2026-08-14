---
id: your-first-script
title: Your First Script
---

# Your First Script

LJE scripts are just folders in `%USERPROFILE%\.lje\scripts`. They are automatically loaded by LJE and are structured as follows:

```
myscript/
├── info.toml
├── boot.lua
└── main.lua
```

## info.toml

This is your script's information file. It contains metadata about your script using the TOML format.
```toml
[script]
name = "myscript"
version = "1.0.0"
author = "yourname"
dependencies = ["coolguy.coolscript"]
binaries = ["lje-mymodule"]
```

- **name**: The name of your script. This is used to identify it as a dependency target.
- **version**: The version of your script. This is not currently enforced, but it's good practice to follow semantic versioning.
- **author**: The author of your script. Combined with `name` to form the unique identity `author.name`.
- **dependencies**: A list of scripts this script depends on. Use `[]` if there are none. Dependencies are formatted as `<author>.<name>`.
- **binaries**: A list of binary module names (excluding `.dll`) this script requires. These modules must be placed in the `%USERPROFILE%\.lje\binaries` folder.

## boot.lua

This is the earliest entry point for your script. It runs during the main menu, before GMod is finished loading. **There is no client GMod API at this stage** — no `surface`, `ents`, `hook` or anything else the [preinit](/api/secure#pull) pulls in, because the client state does not exist yet.

The **menu** state does exist by then, and is reachable:

```lua
-- lje.state.client is nil here; lje.state.menu is not.
local openURL = lje.secure.pull("gui.OpenURL", lje.state.menu)
```

See [lje.state](/api/state) for the details on which universe is available when.

This is really only useful for:
- Reading, calling into, or [hooking](/api/vm#add_engine_call_hook) the main menu. Hooks registered here last for the whole process, unlike ones registered from `main.lua`.
- Setting up deep engine detours (with unsafe LJE).
- Amortizing something expensive that you don't want to run during normal execution.
- Setting up some kind of early state that your script needs.

## main.lua

This is the main entry point for your script. It runs before GMod runs the `init.lua` file. The GMod API is fully available at this point, so this is where the bulk of your script should go. You can use hooks and do typical GMod things here.

## Organization

It is up to you how to organize your script, but only the aforementioned three files are required (except for boot.lua). You can add any additional files or folders you want, and [lje.require](/api/lua#require) or [lje.include](/api/base#include) them as needed. Just be sure to keep your script organized in a way that makes sense to you and is easy to navigate. All relative paths start from the script's root folder for simplicity. If you have a module in `myscript/helpers/foo.lua`, you would write `local foo = lje.require("helpers/foo.lua)` to load it.

### Detours

Detours, as provided by unsafe LJE in `lje-ffi`, can be organized specially. You can write your detour in a separate file and load it as a string using [lje.env.read_script_file](/api/env#read_script_file).

```lua
local detour_code = lje.env.read_script_file("detours/mydetour.c")

my_detour = ffi.detour.create(address, detour_code)
```

This way, you do not have to mix any languages in one source file, and you can use an LSP for C in your IDE.

## Examples

The main example script for LJE is [gilbhax](https://github.com/lj-expand/gilbhax). This is a toy script that demonstrates a lot of LJE's features, and is a good starting point for learning how to write LJE scripts. Additionally, it uses unsafe LJE, so it demonstrates how to use `lje-ffi` as well.

Generally speaking though, the bulk of your code *should* remain natural and almost identical to normal GMod scripts.