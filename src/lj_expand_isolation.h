#pragma once

#include "lj_obj.h"

lua_State* lje_create_isolated_state();
int lje_push_safe_cfunction(lua_State* L, lua_CFunction func);
int lje_copy_to_isolated_state(lua_State* from, lua_State* to, int idx, int allow_lua_functions);
int lje_copy_to_isolated_state_tv(lua_State* from, lua_State* to, cTValue* val, int allow_lua_functions);