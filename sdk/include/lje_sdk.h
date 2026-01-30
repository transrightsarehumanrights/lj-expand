/* LJE SDK: See documentation in the README.md file */
#ifndef _LJE_SDK_H
#define _LJE_SDK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LJE_SDK_VERSION 100 /* SemVer: 1.0.0 */

#ifndef LJE_NO_OPAQUE_STATE /* LJE includes this file internally where lua_State is already defined */
typedef void* lua_State;
#endif
typedef struct LjeApi LjeApi;
typedef struct LjeLuaApi LjeLuaApi;

/* Simple re-exporting of selected Lua C API functions. Most are actually going directly to LuaJIT's internal functions. */
struct LjeLuaApi
{
    void (*pushstring)(void* L, const char* str);
    const char* (*tolstring)(void* L, int idx, size_t* len);
    void (*pushnumber)(void* L, double n);
    double (*tonumber)(void* L, int idx);
    void (*pushinteger)(void* L, int64_t n);
    int64_t (*tointeger)(void* L, int idx);
    void (*pushlightuserdata)(void* L, void* p);
    void* (*tolightuserdata)(void* L, int idx);
    int (*gettop)(void* L);
    void (*settop)(void* L, int idx);
    void (*call)(void* L, int nargs, int nresults);
    int (*pcall)(void* L, int nargs, int nresults, int errfunc);
    void (*getglobal)(void* L, const char* name);
    void (*setglobal)(void* L, const char* name);

    /* LJE-specific extensions */
    void (*pushljeenv)(void* L);
};

struct LjeApi
{
    uint32_t version; /* Version of the LJE SDK */
    struct LjeLuaApi lua; /* Lua C API functions */

    /* Currently, there are no LJE-specific API functions here. Everything necessary can be done with the Lua C API. */
};

typedef enum LjeResult
{
    LJE_RESULT_OK = 0,
    LJE_RESULT_ERROR = 1,
    LJE_RESULT_INCOMPATIBLE_SDK_VERSION = 2,
} LjeResult;

/* Entrypoints */

#ifdef LJE_SDK_IMPLEMENTATION
__declspec(dllexport)
#endif
LjeResult lje_module_init(LjeApi* api); /* Called when the module is loaded into memory at startup. */

#ifdef LJE_SDK_IMPLEMENTATION
__declspec(dllexport)
#endif
LjeResult lje_module_shutdown(); /* Called when the module is unloaded from memory at shutdown. */

#ifdef LJE_SDK_IMPLEMENTATION
__declspec(dllexport)
#endif
LjeResult lje_module_preinit(lua_State* L); /* Called before any user scripts are run, after Lua state is created, and after the built-in preinit script is run. */

/* Entrypoint macros for easier declaration */

#define LJE_MODULE_INIT() \
    LjeResult lje_module_init(LjeApi* api)

#define LJE_MODULE_SHUTDOWN() \
    LjeResult lje_module_shutdown()

#define LJE_MODULE_PREINIT() \
    LjeResult lje_module_preinit(lua_State* L)

#ifdef __cplusplus
}
#endif

#endif
