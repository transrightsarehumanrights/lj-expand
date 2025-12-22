<div align="center">
  <img src="doc/lje-logo.png" width="128" height="128" alt="LJE Logo" />
  <h3>lj-expand</h3>
  <p>Stealthy code execution tool for 64-bit Garry's Mod</p>
  <a href="https://discord.gg/ZXP2tG8J"><img alt="Discord" src="https://img.shields.io/discord/1450731263412670518"></a>
  <hr />
</div>

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

You can write/load data blobs using `lje.data.write(name: string, data: string)` and `lje.data.read(name: string): string | nil`. They are stored in `%USERPROFILE%\.lje_script_data`.
No subdirectories or anything fancy, just flat files that are all named `.dat` for safety.

# More Information

See [doc/mitigations.md](doc/mitigations.md) for more information about the various anti-detection techniques used in LJE.

# Licensing

No license file on purpose since it is a fork of LuaJIT, which is MIT licensed anyways, which means this project is also MIT licensed.