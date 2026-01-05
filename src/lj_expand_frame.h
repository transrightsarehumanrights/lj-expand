#ifndef _LJ_EXPAND_FRAME_H
#define _LJ_EXPAND_FRAME_H

#include "lj_obj.h"

int lje_frame_is_lua_involved(lua_State* L, int frame_offset);
int lje_frame_is_lje_involved(lua_State* L, int frame_offset);

#endif