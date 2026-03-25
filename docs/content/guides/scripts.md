---
id: scripts
name: Scripts
---

# Scripts

Scripts are self-contained folders that contain two primary entrypoints for code execution in LJE:
- `preinit.lua`: This file runs **before** init.lua runs. **Nothing** else runs in this file other than other LJE scripts. Anything is safe in this file. However, **the GMod Lua API is unavailable**, functions defined in Lua will not exist.
- `main.lua`: This file runs **after** init.lua runs. Servers can ship custom code which will **already have executed** at this point. Be aware and cautious when accessing or running things in this stage.

Generally, you want to create detours in `preinit.lua`, and general hooks in `main.lua`. Detours in `main.lua` risk having scripts caching the old function, there is basically no reason to detour in `main.lua`, unless you're going after a Lua function which is ill-advised.

## Structure

You can structure your LJE scripts any way you want. [lje.require](/api/lua#require) and [lje.include](/api/base#include) searches at the root of the script folder no matter what for simplicity.

This is how the average LJE script may look:
```
--- hax/
---    detours/
---       capture.lua
---    utils/
---       foo.lua
--- main.lua
--- preinit.lua
--- info.toml
```

In either main.lua or preinit.lua, this script would require files like so:
```lua
-- For running a detour:
lje.require("detours/capture.lua")
-- For getting foo:
local foo = lje.require("utils/foo.lua")
```

## `info.toml`

This is a file which contains script metadata for LJE when it is loaded. Here is an example `info.toml` file:
```toml
# luadump - exactly what it sounds like
[script]
name = "luadump"
version = "1.0.0"
author = "yogwoggf"
dependencies = ["Eyoko1.ljeutil"]
```

The `dependencies` key is likely the most important one you'll need. Dependencies are formatted as `<author>.<name>`. This script in particular requires Eyoko's `ljeutil` script.

LJE sees this and constructs a load order so that `ljeutil` is **guaranteed** to have ran before this script does. This is how dependencies are established in LJE, allowing for familiar GMod-esque global libraries.