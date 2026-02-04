# LJE Binary Module SDK

This SDK provides a single, easy-to-use header, `lje_sdk.h`, which exposes the necessary DLL exports and functions to interact with LJE from a binary module.
This system is essentially a replica of the GMod module SDK, but adapted to work with LJE's internals and be undetectable, since anticheats can easily
detect the use of standard GMod binary modules.

## Getting Started

Because of the nature of LJE, I assume many users of the SDK will likely not be using things like CMake or other build systems.
The SDK is designed to be as simple as possible to integrate into your existing build system, as it is a single header file compatible with C and C++.
To get started, simply download the `lje_sdk.h` file from the LJE repository and include it in your project, or submodule LJE directly into your repository for easier updates.

There are three states you need to be aware of when building an LJE module:
1. **Initialization**: This is when GMod is launched and LJE is loaded. There is no Lua state or anything yet, just the module being loaded into memory. Initialize any global state here.
2. **Preinit**: This is when GMod is loading and LJE has ran its preinit code. The Lua state is available and passed in, but the game is not fully loaded yet. **This is where you should register any global functions, metatables, or other Lua state modifications.**
3. **Unloading**: This is when GMod is shutting down and LJE is unloading. Clean up any global state here. However, this is only called during proper shutdown. To be completely honest, you probably don't need to do much here unless you're managing resources outside of Lua. The process is going to exit anyway.

There is no explicit "close" or "deinit" state. This may change in the future, but for now, for every preinit, you can assume a new Lua state was created and if called previously, that old state is closed and gone.

You **should** define `LJE_SDK_IMPLEMENTATION` in **one** source file to include the implementation of the SDK.
```c
#define LJE_SDK_IMPLEMENTATION
#include "lje_sdk.h"

// You want to hold onto the global function table for later use.
static LjeApi* g_ljeApi = NULL;

LJE_MODULE_INIT()
{
    // The API is passed in as a pointer, accessible via `api`.
    // You return an enum value of type `LjeModuleInitResult`.
    g_ljeApi = api;
    
    if (api->version != LJE_SDK_VERSION) // alternatively, you can use <= if you want to allower newer versions (backwards compatibility)
    {
        // The SDK version does not match the LJE version!
        return LJE_MODULE_INIT_RESULT_VERSION_MISMATCH;
    }
    
    return LJE_MODULE_INIT_RESULT_SUCCESS;
}

static int my_c_function(lua_State* L)
{
    LjeLuaApi* lua = g_ljeApi->lua;
    lua->pushstring(L, "Hello from my_c_function!");
    return 1; // Number of return values
}

LJE_MODULE_PREINIT(lua_State* L)
{
    // The Lua state is passed in as `L`.
    // Most of the public Lua API is simply re-exposed by LJE, so you can use it as normal.
    // Since it is very much exactly like the public Lua C API, just re-exported, it is contained in the `lua` field.
    LjeLuaApi* lua = g_ljeApi->lua;
    lua->pushljeenv(L); /* LJE-specific function to push the global LJE environment */
    lua->pushcclosure(L, my_c_function, 0);
    lua->setfield(L, -2, "my_c_function");
    lua->pop(L, 1); /* Pop the global environment */
    
    return LJE_MODULE_PREINIT_RESULT_SUCCESS;
}

LJE_MODULE_UNLOAD()
{
    // Clean up any global state here.
    return LJE_MODULE_UNLOAD_RESULT_SUCCESS;
}
```