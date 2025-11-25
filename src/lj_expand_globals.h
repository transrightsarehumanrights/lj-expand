#ifndef _LJ_EXPAND_GLOBALS_H
#define _LJ_EXPAND_GLOBALS_H
#include "lua.h"
#include "lj_obj.h"

#define LJE_LUA_BASE_PATH "C:/LJE/"

/* LJE: Fast linked list to hold spoofed functions. Useful for doing a fast lookup of either
 * spoof to target or target to spoof.
 */
typedef struct LJESpoofRecord
{
   GCfunc* spoof; // e.g: lua function pretending to be target
    GCfunc* target; // e.g: original function being spoofed, can be C function or lua function
   struct LJESpoofRecord* next;
} LJESpoofRecord;

/* LJE: This is our own global state, sysmalloc'd without any
 * interference with LuaJIT's own global_State. This is because
 * it has very *very* specific and precise allocation to facilitate JITed
 * code performance, and we don't want to mess with that.
 */
typedef struct LJEGlobalState
{
    int push_string_ref_id;
    int env_ref_id;
    lua_State* main_state;
    int skip_hooks;
    GCRef ignore_fn_on_hook;
    int in_hook;
    int waiting_for_init_call;
    LJESpoofRecord spoof_record_root;
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

#endif