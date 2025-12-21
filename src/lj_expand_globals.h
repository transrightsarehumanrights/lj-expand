#ifndef _LJ_EXPAND_GLOBALS_H
#define _LJ_EXPAND_GLOBALS_H
#include "lua.h"
#include "lj_obj.h"
#include "lj_expand_script.h"

/* LJE: Fast linked list to hold spoofed functions. Useful for doing a fast lookup of either
 * spoof to target or target to spoof.
 */
typedef struct LJESpoofRecord
{
   GCfunc* spoof; // e.g: lua function pretending to be target
    GCfunc* target; // e.g: original function being spoofed, can be C function or lua function
   struct LJESpoofRecord* next;
} LJESpoofRecord;

typedef struct LJERandomState
{
    /* LJE: It's easier copying the PRNG struct then having to expose it publicly. */
    uint64_t gen[4];
    int valid;
} LJERandomState;

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
    lua_State* main_state;
    int skip_hooks;
    GCRef ignore_fn_on_hook;
    int in_hook;
    int waiting_for_init_call;
    int original_gc;
    LJESpoofRecord spoof_record_root;
    LJEScript* loaded_scripts;
    size_t loaded_script_count;
    LJEScript** script_load_order;
    /* Used for script execution context */
    LJEScript* current_script;
    int show_special_frames;
    int disable_metatables;
    /* Used for random state rollback */
    LJERandomState random_state;
} LJEGlobalState;

#define LJEG() (lje_get_global_state())
LJEGlobalState* lje_get_global_state();

void lje_clear_global_refs();
void lje_insert_spoof_record(GCfunc* spoof, GCfunc* target);
GCfunc* lje_find_spoof_by_target(GCfunc* target);
void lje_remove_spoof_record_by_spoof(GCfunc* spoof);
void lje_remove_spoof_record_by_target(GCfunc* target);
void lje_clear_spoof_records();
char* lje_concat_path(const char* relative_path);

/* LJE: No-ops if environment is not set. */
void lje_set_random_state(LJERandomState* prev, LJERandomState* new);

#define lje_save_random_state() \
    lje_set_random_state(NULL, &LJEG()->random_state)

#define lje_restore_random_state() \
    lje_set_random_state(&LJEG()->random_state, NULL)

#endif