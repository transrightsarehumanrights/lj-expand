#include "lj_expand_globals.h"
#include "lj_expand_log.h"
#include "lj_expand_platform.h"

#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"

static LJEGlobalState* lje_global_state = NULL;

LJEGlobalState* lje_get_global_state() {
    if (!lje_global_state) {
        lje_global_state = (LJEGlobalState*)malloc(sizeof(LJEGlobalState));
        memset(lje_global_state, 0, sizeof(LJEGlobalState));
        lje_clear_global_refs();
    }

    return lje_global_state;
}

void lje_clear_global_refs() {
    LJEG()->script_hook_ref_id = LUA_NOREF;
    for (size_t i = 0; i < LJEG()->loaded_script_count; i++)
    {
        LJEScript* script = LJEG()->script_load_order[i];
        script->extra->engine_call_hook_count = 0;
        memset(script->extra->engine_call_hooks, 0, sizeof(LJEEngineCallHook) * LJE_SCRIPT_MAX_ENGINE_CALL_HOOKS);
        script->extra->cleanup_ref_id = LUA_NOREF;
    }

    if (LJEG()->script_watcher)
    {
        lje_watcher_destroy(LJEG()->script_watcher);
        LJEG()->script_watcher = NULL;
    }
}

LJEHostView* lje_host_view(LJEHostId id)
{
    if ((int)id < 0 || (int)id >= LJE_HOST_COUNT)
        id = LJE_HOST_CLIENT;
    return &LJEG()->hosts[id];
}

/* We only support menu or client for now. */
lua_State* lje_host_state(LJEHostId id)
{
    return id == LJE_HOST_MENU ? LJEG()->menu_state : LJEG()->main_state;
}

int lje_host_id_of(lua_State* L, LJEHostId* out)
{
    if (!L)
        return 0;

    for (int i = 0; i < LJE_HOST_COUNT; i++)
    {
        if (lje_host_state((LJEHostId)i) == L)
        {
            if (out) *out = (LJEHostId)i;
            return 1;
        }
    }

    return 0;
}

const char* lje_host_name(LJEHostId id)
{
    return id == LJE_HOST_MENU ? "menu" : "client";
}

int lje_is_addr_in_lje(uintptr_t addr)
{
    uintptr_t base;
    size_t size;
    if (!lje_plat_self_range(&base, &size))
        return 0;
    return (addr >= base) && (addr < (base + size));
}

void lje_print_stack(lua_State* L)
{
  // Print like [number][table][userdata].
  LJE_INFO("%d elements on stack.", (int)(L->top - L->base));
  LJE_INFO("Stack contents:");
  for (TValue* o = L->base; o < L->top; o++)
  {
    lje_log_raw("[%s]", itype(o) == LJ_TSTR ? strdata(strV(o)) : lj_typename(o));
  }
  lje_log_raw("\n");
}