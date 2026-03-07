#ifndef _LJ_EXPAND_FRAME_H
#define _LJ_EXPAND_FRAME_H

#include "lj_obj.h"

typedef int(*LJEFrameCheckFunc)(lua_State* L, GCfunc* func, int level);

int lje_frame_is_lua_involved(lua_State* L, int frame_offset);
int lje_frame_is_lje_involved(lua_State* L, int frame_offset, int max_level);
int lje_frame_is_lje(lua_State* L, int frame_offset);
int lje_frame_walk(lua_State* L, int frame_offset, LJEFrameCheckFunc check_func);
#endif