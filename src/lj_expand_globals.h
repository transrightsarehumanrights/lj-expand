#ifndef _LJ_EXPAND_GLOBALS_H
#define _LJ_EXPAND_GLOBALS_H
#include "lua.h"

/* LJE: This is our own global state, sysmalloc'd without any
 * interference with LuaJIT's own global_State. This is because
 * it has very *very* specific and precise allocation to facilitate JITed
 * code performance, and we don't want to mess with that.
 */
typedef struct LJEGlobalState
{
    int push_string_ref_id;
    lua_State* main_state;
} LJEGlobalState;

#define LJEG() (lje_get_global_state())
LJEGlobalState* lje_get_global_state();

#endif