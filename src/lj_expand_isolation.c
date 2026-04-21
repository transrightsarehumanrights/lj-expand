#include "lj_expand_isolation.h"

#include "lauxlib.h"
#include "lj_expand_globals.h"
#include "lj_expand_lib.h"
#include "lualib.h"

#include <windows.h>

typedef lua_State* (*luaL_newstate_t)(void);
typedef void       (*luaL_openlibs_t)(lua_State*);

lua_State* lje_create_isolated_state() {
    HMODULE gmod_lua = GetModuleHandleA("lua_shared.dll");
    if (!gmod_lua)
    {
        printf("[LJE] Failed to locate GMod's lua_shared.dll for isolated state creation.\n");
        return NULL;
    }

    luaL_newstate_t gmod_newstate = (luaL_newstate_t)GetProcAddress(gmod_lua, "luaL_newstate");
    luaL_openlibs_t gmod_openlibs = (luaL_openlibs_t)GetProcAddress(gmod_lua, "luaL_openlibs");
    if (!gmod_newstate || !gmod_openlibs)
    {
        printf("[LJE] Failed to resolve GMod luaL_newstate/luaL_openlibs exports.\n");
        return NULL;
    }

    // We deliberately use GMod's luaL_newstate (and luaL_openlibs) instead of our own
    // bundled LuaJIT's, because the engine's loader/parser/lexer all operate against
    // GMod's lua_State and global_State layouts. A state allocated by our LuaJIT would
    // be the wrong shape and the keyword table indices wouldn't match the engine's
    // TK_* enum, causing nonsense parser errors (e.g. "near 'in'" on every script).
    lua_State* L = gmod_newstate();
    if (!L)
    {
        printf("[LJE] Failed to create isolated Lua state.\n");
        return NULL;
    }

    // An isolated state basically just means we create an entirely separated Lua universe
    // for LJE only. Then we pull in our API functions only.

    LJEG()->isolated_state = L;
    gmod_openlibs(L); // Open standard libraries (using GMod's openlibs, since this is a GMod-shaped state)
    lje_addfuncs(L);  // Add LJE-specific functions to the isolated state

    printf("[LJE] Created isolated Lua state: %p\n", (void*)L);
    return L;
}