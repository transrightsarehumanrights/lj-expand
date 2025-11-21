#ifndef _LJ_EXPAND_LIB_H
#define _LJ_EXPAND_LIB_H

#include "lj_obj.h"

int lje_enable_hooks(lua_State* L);
int lje_disable_hooks(lua_State* L);
LJ_FUNC void lje_addfuncs(lua_State* L);
LJ_FUNC void lje_removefuncs(lua_State* L);

#endif