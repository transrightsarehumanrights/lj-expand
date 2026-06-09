#ifndef _LJ_EXPAND_GLOBALS_H
#define _LJ_EXPAND_GLOBALS_H

#include "lua.h"
#include "lj_obj.h"
#include "lj_expand_script.h"
#include "lj_expand_script_watcher.h"
#include "lj_expand_binary_module.h"

/* LJE: This is our own global state, sysmalloc'd without any
 * interference with LuaJIT's own global_State. This is because
 * it has very *very* specific and precise allocation to facilitate JITed
 * code performance, and we don't want to mess with that.
 */
typedef struct LJEGlobalState
{
    int script_hook_ref_id;
    lua_State* main_state;
    int waiting_for_init_call;
    int waiting_for_startup_call;
    LJEScript* loaded_scripts;
    size_t loaded_script_count;
    LJEScript** script_load_order;
    /* Used for script execution context */
    LJEScript* current_script;
    int show_special_frames;
    /* Used for random state rollback */
    /* Used for flagging protos */
    int flag_lje_protos;
    /* Metatable remapping */
    lua_CFunction adv_error_reporter;
    int using_error_reporter;
    int is_engine_call_handled;
    /* Hot reloading */
    LJEScriptWatcher* script_watcher;
    /* Loaded binary modules */
    LJEBinaryModule* loaded_binary_modules;
    size_t loaded_binary_module_count;
    /* Isolated state */
    lua_State* isolated_state;
    char redirect_to_isolation;
    GCtab* shadow_registry;
    /* Monotonic allocator for fresh shadow-registry refs (luaL_ref under redirect).
       lua_objlen on the shadow registry returns 0 (its keys live in the hash part),
       so size-based fresh refs would all collide on the same slot. */
    int isolated_ref_counter;
} LJEGlobalState;

#define LJEG() (lje_get_global_state())
LJEGlobalState* lje_get_global_state();

void lje_clear_global_refs();
char* lje_concat_path(const char* relative_path);

int lje_is_addr_in_lje(uintptr_t addr);
void lje_print_stack(lua_State* L);

#define lje_iterate_scripts() \
  for (size_t i = 0; i < LJEG()->loaded_script_count; i++) { \
    LJEScript* script = LJEG()->script_load_order[i]; \

#define lje_iterate_scripts_end() \
  }

#endif