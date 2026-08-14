#ifndef _LJ_EXPAND_HOST_H
#define _LJ_EXPAND_HOST_H

#include "lj_obj.h"

/* The GMod Lua states LJE can borrow functions from and listen to. */
typedef enum LJEHostId
{
    LJE_HOST_CLIENT = 0,
    LJE_HOST_MENU = 1,
    LJE_HOST_COUNT
} LJEHostId;

// Each state has to have its own shadow registry
typedef struct LJEHostView
{
    GCtab* shadow_registry;
} LJEHostView;

LJEHostView* lje_host_view(LJEHostId id);
lua_State* lje_host_state(LJEHostId id);
/* Maps a live GMod state to its host id. Returns 0 if L is not a host state. */
int lje_host_id_of(lua_State* L, LJEHostId* out);
const char* lje_host_name(LJEHostId id);

#endif
