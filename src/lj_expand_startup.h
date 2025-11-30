#ifndef _LJ_EXPAND_STARTUP_H
#define _LJ_EXPAND_STARTUP_H
#include "lj_expand_script.h"
#include "lj_obj.h"

LJ_FUNC void lje_startup_execute(lua_State* L, LJEScript* script);
LJ_FUNC void lje_startup_preinit(lua_State* L);
LJ_FUNC int lje_startup_include(lua_State* L, const char* relative_path, int execute);
#endif