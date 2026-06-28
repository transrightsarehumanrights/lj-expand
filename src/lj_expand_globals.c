#include "lj_expand_globals.h"
#include "lj_expand_log.h"

#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>

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
        script->extra->engine_call_hook_ref_id = LUA_NOREF;
        script->extra->cleanup_ref_id = LUA_NOREF;
    }

    if (LJEG()->script_watcher)
    {
        lje_watcher_destroy(LJEG()->script_watcher);
        LJEG()->script_watcher = NULL;
    }
}

static uintptr_t lje_base_addr = 0;
static size_t lje_addr_range = 0;
static const char* lje_module_name = "lje-w64.dll";

int lje_is_addr_in_lje(uintptr_t addr)
{
    if (lje_base_addr == 0)
    {
        HMODULE hModule = GetModuleHandleA(lje_module_name);
        if (hModule)
        {
            MODULEINFO modInfo;
            if (GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo)))
            {
                lje_base_addr = (uintptr_t)modInfo.lpBaseOfDll;
                lje_addr_range = (size_t)modInfo.SizeOfImage;
            }
        }
    }

    return (addr >= lje_base_addr) && (addr < (lje_base_addr + lje_addr_range));
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