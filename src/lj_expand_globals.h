#ifndef _LJ_EXPAND_GLOBALS_H
#define _LJ_EXPAND_GLOBALS_H

#include "lua.h"
#include "lj_obj.h"
#include "lj_expand_script.h"
#include "lj_expand_script_watcher.h"
#include "lj_expand_binary_module.h"

typedef struct LJERandomState
{
    /* LJE: It's easier copying the PRNG struct then having to expose it publicly. */
    uint64_t gen[4];
    int valid;
} LJERandomState;

#define MAX_REMAP_TYPES 16
#define MAX_AUTH_METATABLES 128
#define MAX_HIDDEN_CALLERS 128

typedef struct LJEMetatableRemap
{
    /* LJE: MUST BE A STRING LITERAL */
    const char* type_name;
    GCtab* replacement;
} LJEMetatableRemap;

typedef struct LJEGCTracker
{
    GCobj *root_sentinel;
    GCobj *ud_sentinel;
    GCSize saved_total;
    GCSize saved_threshold;
    MSize saved_sbuf_sz;
    int active;
} LJEGCTracker;

/* LJE: This is our own global state, sysmalloc'd without any
 * interference with LuaJIT's own global_State. This is because
 * it has very *very* specific and precise allocation to facilitate JITed
 * code performance, and we don't want to mess with that.
 */
typedef struct LJEGlobalState
{
    int push_string_ref_id;
    int env_ref_id;
    int script_hook_ref_id;
    int engine_call_hook_ref_id;
    lua_State* main_state;
    int skip_hooks;
    GCRef ignore_fn_on_hook;
    int in_hook;
    int waiting_for_init_call;
    int waiting_for_startup_call;
    int original_gc;
    LJEScript* loaded_scripts;
    size_t loaded_script_count;
    LJEScript** script_load_order;
    /* Used for script execution context */
    LJEScript* current_script;
    int show_special_frames;
    int disable_metatables;
    /* Used for random state rollback */
    LJERandomState random_state;
    /* Used for flagging protos */
    int flag_lje_protos;
    /* Metatable remapping */
    LJEMetatableRemap metatable_remaps[MAX_REMAP_TYPES];
    GCtab* auth_metatables[MAX_AUTH_METATABLES];
    size_t auth_metatable_count;
    lua_CFunction adv_error_reporter;
    int using_error_reporter;
    int is_engine_call_handled;
    GCfunc* hidden_callers[MAX_HIDDEN_CALLERS];
    size_t hidden_caller_count;
    /* Hot reloading */
    LJEScriptWatcher* script_watcher;
    /* Loaded binary modules */
    LJEBinaryModule* loaded_binary_modules;
    size_t loaded_binary_module_count;
    /* GC */
    GCSize gc_compensation;
    LJEGCTracker gc_tracker;
    /* Timing attacks */
    double clock_deficit;
    /* Isolated state */
    lua_State* isolated_state;
    char redirect_to_isolation;
} LJEGlobalState;

#define LJEG() (lje_get_global_state())
LJEGlobalState* lje_get_global_state();

void lje_clear_global_refs();
char* lje_concat_path(const char* relative_path);

/* LJE: No-ops if environment is not set. */
void lje_set_random_state(LJERandomState* prev, LJERandomState* new);

#define lje_save_random_state() \
    lje_set_random_state(NULL, &LJEG()->random_state)

#define lje_restore_random_state() \
    lje_set_random_state(&LJEG()->random_state, NULL)

LJEMetatableRemap* lje_get_metatable_remap(const char* type_name);
void lje_set_metatable_remap(const char* type_name, GCtab* replacement);

void lje_auth_metatable(GCtab* mt);
int lje_is_metatable_authorized(GCtab* mt);

void lje_hide_caller(GCfunc* func);
int lje_is_caller_hidden(GCfunc* func);

int lje_is_addr_in_lje(uintptr_t addr);

#endif