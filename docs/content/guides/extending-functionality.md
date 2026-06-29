---
id: extending-functionality
title: Extending Functionality
---

# Extending Functionality

Unsafe LJE allows you to, literally, extend your scripts functionality to virtually any capability you can imagine.

The [`lje-ffi`](https://github.com/lj-expand/lje-ffi) module is a massive FFI library to give you the tools to do anything you want in your script.
The main feature is that you can call any function you want, yes **any.** You can call functions in GMod, in LuaJIT, in Windows API, in any loaded DLL, etc. The point is to make it so that nobody needs to write their own custom binary module for anything, they can just use the FFI to call whatever they want to maintain a simple, single-source script.

## Sending Mouse Inputs

Let's explore an example of this, avoiding the need to override `CUserCmd` in `CreateMove` to move the aim around by using the FFI module.

**Again:** You need reverse-engineering knowledge, but this particular example is quite light.

```lua
-- Mouse input library, uses FFI to send mouse input to the game such that the engine
-- correctly processes it and we can implement an aimbot without messing with anything that will
-- create observable side effects like view angle changes or engine prediction errors.

local user32 = ffi.module.find("user32.dll")
local SendInput = ffi.module.bind_export(user32, "SendInput", "uupi")

-- INPUT struct, no keyboard inputs because well we really don't have a need for it
-- Matches C's union padding though with 8 bytes of padding at the end.
ffi.struct.define([[
struct INPUT {
  uint32_t type;
  padding[4];

  long dx;
  long dy;
  uint32_t mouseData;
  uint32_t dwFlags;
  uint32_t time;
  uintptr_t dwExtraInfo;
};
]])

local INPUT_SIZE = ffi.struct.sizeof("INPUT")
local INPUT_PTR = ffi.mem.alloc(INPUT_SIZE)
local INPUT_MOUSE = 0
local MOUSEEVENTF_MOVE = 0x0001

local mouse = {}

function mouse.move(dx, dy)
  ffi.struct.write(INPUT_PTR, "INPUT", {
    type = INPUT_MOUSE,
    dx = dx,
    dy = dy,
    mouseData = 0,
    dwFlags = MOUSEEVENTF_MOVE,
    time = 0,
    dwExtraInfo = 0,
  })

  local result = SendInput(1, INPUT_PTR, INPUT_SIZE)
  if result == 0 then
    lje.con_print("SendInput failed.")
  end
end

return mouse
```

Let's break down what's going on here:
- We bind the `SendInput` function from `user32.dll` using the FFI module. This allows us to call `SendInput` directly from Lua.
- We define the `INPUT` struct that `SendInput` expects. This struct has a specific layout that we need to match, including padding to ensure the correct size.
- We create a function `mouse.move(dx, dy)` that fills in an `INPUT` struct with the appropriate values to indicate a mouse movement, and then calls `SendInput` to send that input to the game.

LJE FFI has no documentation right now, but that will come soon. The `uupi` in the `bind_export` call is a type signature that indicates the types of the function's parameters and return value. The first letter is always the return value, so this means:

- `u`: unsigned int (the return type of `SendInput`)
- `u`: unsigned int (the first parameter, `nInputs`)
- `p`: pointer (the second parameter, `pInputs`)
- `i`: int (the third parameter, `cbSize`)

Fairly simple! The only thing that requires special knowledge is the layout of the `INPUT` struct and how to use `SendInput`, which can be found in the Windows API documentation. The FFI overhead is also very minimal, so this is a very efficient way to send mouse inputs to the game without any of the side effects that come with messing with `CUserCmd` or other GMod-specific systems.

This is just one example of how you can use the FFI module to extend your script's functionality in ways that would be impossible with just Lua. You can call any function you want, so the possibilities are truly endless. Just be sure to understand the risks and have the necessary knowledge before diving into unsafe LJE and the FFI module!
