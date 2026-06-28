---
id: settings
title: Settings
---

# Settings

LJE gives every script an official settings interface, so you don't have to roll your own config system. A script ships sensible **defaults**, and the user can **override** them in a file that lives outside the script's folder — which means their changes survive a `git pull` when they update the script.

## The two files

Settings are layered from two TOML files:

| File | Who edits it | Where it lives |
|------|--------------|----------------|
| `settings.default.toml` | The script author | In the script's own folder, committed to the repo |
| `<author>.<name>.toml`  | The user | `%USERPROFILE%\.lje\settings\`, outside the script |

The author ships `settings.default.toml` alongside `main.lua`. Because the user's overrides live in a separate directory, updating a cloned script never clobbers their settings, and their settings never show up as local changes in the script's repo.

The override filename is a **slug** built from the script's identity (`author.name` from [`info.toml`](info-toml)): it is lowercased, and any character outside `a-z`, `0-9`, `_`, or `-` becomes `-`. If LJE has to sanitize anything it prints a warning showing the resulting slug.

The first time a script's settings are read, LJE auto-creates an empty override file with a short header comment. It is intentionally empty — anything you don't set falls back to the defaults (see merging below), so an empty override means "use all defaults".

### Example

`settings.default.toml`, shipped by the author:

```toml
[render]
enabled = true
color = "white"

[combat]
max_targets = 3
```

A user's `%USERPROFILE%\.lje\settings\yourname.myscript.toml`:

```toml
[render]
color = "red"
```

The merged result the script sees:

```toml
[render]
enabled = true   # from defaults
color = "red"    # overridden by the user

[combat]
max_targets = 3  # from defaults
```

## Merging

The two files are merged with **user overrides winning**:

- Tables are deep-merged, so the user only has to set the keys they care about.
- Scalars and plain arrays are replaced wholesale by the override (arrays of tables are appended).
- Keys that exist only in the defaults are preserved — so when an author adds a new default in an update, every user picks it up automatically without touching their override file.
- If either file is missing, the other is used on its own. If a file is malformed, LJE warns and treats that layer as empty. If both are absent, settings are simply an empty table.

## Reading settings

Use `lje.settings.get` with a **dotted path** to read a single value, or `lje.settings.all` to get the whole merged table:

```lua
local enabled = lje.settings.get("render.enabled")      -- true
local color   = lje.settings.get("render.color", "white") -- "red"
local missing = lje.settings.get("render.nope", 0)       -- 0 (fallback)

local cfg = lje.settings.all()
print(cfg.combat.max_targets)                            -- 3
```

`get` walks the dotted key through nested tables. If any segment is missing (or isn't a table), it returns your `default`, or `nil` if you didn't give one.

## The current-script problem

`lje.settings.get`, `.all`, and `.reload` operate on the *currently executing* script. LJE only knows which script that is **while your script's code is actually running** — which is fine at load time, but **not** inside deferred callbacks like hooks, render functions, or timers. Those fire long after your script finished loading, when there is no current script, and the bare functions will log a warning and act as if you have no settings:

```lua
-- BROKEN: no current script when HUDPaint fires
hook.Add("HUDPaint", "x", function()
  if lje.settings.get("render.enabled") then draw() end
end)
```

## Settings objects

The fix is to **bind a settings object at load time** and reuse it anywhere. Call `lje.settings.open()` once while your script is loading; it captures your script and returns an object whose methods work regardless of context:

```lua
local settings = lje.settings.open()   -- at the top level of main.lua

hook.Add("HUDPaint", "x", function()
  if settings:get("render.enabled") then       -- note the colon
    draw(settings:get("render.color", "white"))
  end
end)
```

The object exposes `:get(key, default)`, `:all()`, and `:reload()` — the same behaviour as the namespace functions, but always resolved to the script that opened it. It stores the script's name (not a pointer), so it keeps working across hot-reloads. `lje.settings.bind(name)` is the lower-level primitive if you ever need to bind to a script other than the caller; `open()` is just `bind(lje.env.current_script())`.

## Caching and reloading

Merged settings are parsed once and cached per script. The cache is busted automatically when:

- a script is **hot-reloaded** (its files change on disk), and
- LJE re-initializes its scripts.

You can also call `lje.settings.reload()` (or `settings:reload()` on an object) to drop the current script's cache entry and force the next read to re-parse both files from disk.

## Inspecting a script's own metadata

Related to settings, `lje.script.info()` returns the script's parsed [`info.toml`](info-toml):

```lua
local me = lje.script.info()
print(me.name, me.version, me.author)
for _, dep in ipairs(me.dependencies) do print("depends on", dep) end
```

Like the bare settings functions, `lje.script.info()` reads the current script and returns `nil` if there is no active script context.
