#ifndef _LJ_EXPAND_SETTINGS_H
#define _LJ_EXPAND_SETTINGS_H
#include "lj_obj.h"

int lje_settings_all(lua_State* L);
int lje_settings_get(lua_State* L);
int lje_settings_reload(lua_State* L);
int lje_settings_bind(lua_State* L);
int lje_script_info(lua_State* L);

void lje_settings_clear_cache(lua_State* L);

#endif
