#ifndef _LJ_EXPAND_GLOBALS_H
#define _LJ_EXPAND_GLOBALS_H

#include "lua.h"
#include "lj_obj.h"
#include "lj_expand_script.h"
#include "lj_expand_script_watcher.h"
#include "lj_expand_binary_module.h"

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

/* LJE: This is our own global state, sysmalloc'd without any
 * interference with LuaJIT's own global_State. This is because
 * it has very *very* specific and precise allocation to facilitate JITed
 * code performance, and we don't want to mess with that.
 */
typedef struct LJEGlobalState
{
    int script_hook_ref_id;
    lua_State* main_state;
    lua_State* menu_state;
    int waiting_for_init_call;
    int waiting_for_startup_call;
    int waiting_for_menu_call;
    LJEScript* loaded_scripts;
    size_t loaded_script_count;
    LJEScript** script_load_order;
    /* Used for script execution context */
    LJEScript* current_script;
    /* Used for flagging protos */
    lua_CFunction adv_error_reporter;
    int using_error_reporter;
    /* Engine call hooks: in_pre_engine_call_hook is set while pre hooks run, so
       lje.vm.suppress_engine_call() can tell it is in a valid context. */
    char in_pre_engine_call_hook;
    char engine_call_suppressed;
    /* Hot reloading */
    LJEScriptWatcher* script_watcher;
    /* Loaded binary modules */
    LJEBinaryModule* loaded_binary_modules;
    size_t loaded_binary_module_count;
    /* Isolated state */
    lua_State* isolated_state;
    char redirect_to_isolation;
    /* Which host the current redirect is targeted towards. */
    LJEHostId redirect_host;
    LJEHostView hosts[LJE_HOST_COUNT];
    /* Monotonic allocator for fresh shadow-registry refs (luaL_ref under redirect).
       lua_objlen on the shadow registry returns 0 (its keys live in the hash part),
       so size-based fresh refs would all collide on the same slot. Shared across
       hosts so a ref that leaks between views misses instead of aliasing. */
    int isolated_ref_counter;
} LJEGlobalState;

#define LJEG() (lje_get_global_state())
LJEGlobalState* lje_get_global_state();

/* Shadow registry of the host the current redirect belongs to. */
#define LJE_SHADOW() (LJEG()->hosts[LJEG()->redirect_host].shadow_registry)

LJEHostView* lje_host_view(LJEHostId id);
lua_State* lje_host_state(LJEHostId id);
/* Maps a live GMod state to its host id. Returns 0 if L is not a host state. */
int lje_host_id_of(lua_State* L, LJEHostId* out);
const char* lje_host_name(LJEHostId id);

void lje_clear_global_refs();

int lje_is_addr_in_lje(uintptr_t addr);
void lje_print_stack(lua_State* L);

#define lje_iterate_scripts() \
  for (size_t i = 0; i < LJEG()->loaded_script_count; i++) { \
    LJEScript* script = LJEG()->script_load_order[i]; \

#define lje_iterate_scripts_end() \
  }

#endif