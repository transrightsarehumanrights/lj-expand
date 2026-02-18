/* LJE SDK: See documentation in the README.md file */
#ifndef _LJE_SDK_H
#define _LJE_SDK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LJE_SDK_VERSION 160 /* SemVer: 1.5.0 */

#ifndef LJE_NO_OPAQUE_STATE /* LJE includes this file internally where lua_State is already defined */
typedef void* lua_State;
/* Also provide ref constants */
#define LUA_REGISTRYINDEX  (-10000)
#define LUA_GLOBALSINDEX   (-10002)
#define LUA_ENVIRONINDEX  (-10001)

#define LUA_OK		0
#define LUA_YIELD	1
#define LUA_ERRRUN	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
#define LUA_ERRERR	5

#define LUA_NOREF       (-2)
#define LUA_REFNIL      (-1)

/*
** basic types
*/
#define LUA_TNONE		(-1)

#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8
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
    void (*pushnil)(void* L);
    void (*pushcclosure)(void* L, void* fn, int n);
    int (*gettop)(void* L);
    void (*settop)(void* L, int idx);
    void (*call)(void* L, int nargs, int nresults);
    int (*pcall)(void* L, int nargs, int nresults, int errfunc);
    void (*getfield)(void* L, int idx, const char* k);
    void (*setfield)(void* L, int idx, const char* k);

    void (*pushboolean)(void* L, int b);
    int (*toboolean)(void* L, int idx);

    int (*ref)(void* L, int t);
    void (*unref)(void* L, int t, int ref);
    void (*rawgeti)(void* L, int idx, int n);
    void (*rawseti)(void* L, int idx, int n);
    void (*pushvalue)(void* L, int idx);

    void (*createtable)(void* L, int narray, int nrec);
    int (*isnil)(void* L, int idx);
    int (*type)(void* L, int idx);
    const char* (*typename_)(void* L, int tp);

    size_t (*objlen)(void* L, int idx);
    /* LJE-specific extensions */
    void (*pop)(void* L, int n);
    void (*pushljeenv)(void* L);

    /* NOTE: luaL_newmetatable is not included because its internal logic
     * is detectable. If you want to create a metatable, just createtable and set it in the registry yourself,
     * without a string key since those are detectable. */
    void* (*newuserdata)(void* L, size_t sz);
    void* (*touserdata)(void* L, int idx);
    void (*setmetatable)(void* L, int idx);

    const char* (*getupvalue)(void* L, int idx, int n);
    int (*is_lje_involved)(void* L, int offset, int max_level); // Returns 1 if any of the call frames up to max_level above the current frame + offset are LJE frames.
    void (*mark_special)(void* L, int idx); // Marks a function, C-closures supported, as special.
    void (*pushlstring)(void* L, const char* str, size_t len); // Like pushstring but for strings with embedded nulls.
};

struct LjeApi
{
    uint32_t version; /* Version of the LJE SDK */
    LjeLuaApi* lua; /* Lua C API functions */

    /* Currently, there are no LJE-specific API functions here. Everything necessary can be done with the Lua C API. */
};

typedef enum LjeResult
{
    LJE_RESULT_OK = 0,
    LJE_RESULT_ERROR = 1,
    LJE_RESULT_INCOMPATIBLE_SDK_VERSION = 2,
} LjeResult;

/* Entrypoints */

typedef LjeResult (*LjeModuleInitFunc)(LjeApi* api);
typedef LjeResult (*LjeModuleShutdownFunc)();
typedef LjeResult (*LjeModulePreinitFunc)(lua_State* L);

#ifdef LJE_SDK_IMPLEMENTATION
__declspec(dllexport)
LjeResult lje_module_init(LjeApi* api); /* Called when the module is loaded into memory at startup. */

__declspec(dllexport)
LjeResult lje_module_shutdown(); /* Called when the module is unloaded from memory at shutdown. */

__declspec(dllexport)
LjeResult lje_module_preinit(lua_State* L); /* Called before any user scripts are run, after Lua state is created, and after the built-in preinit script is run. */
#endif

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
