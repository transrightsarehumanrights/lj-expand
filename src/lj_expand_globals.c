#include "lj_expand_globals.h"

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
    LJEG()->env_ref_id = LUA_NOREF;
    LJEG()->push_string_ref_id = LUA_NOREF;
    LJEG()->script_hook_ref_id = LUA_NOREF;
    LJEG()->engine_call_hook_ref_id = LUA_NOREF;
    for (size_t i = 0; i < LJEG()->loaded_script_count; i++)
    {
        LJEScript* script = LJEG()->script_load_order[i];
        script->extra->engine_call_hook_ref_id = LUA_NOREF;
    }

    memset(&LJEG()->metatable_remaps[0], 0, sizeof(LJEG()->metatable_remaps));
    memset(&LJEG()->auth_metatables[0], 0, sizeof(LJEG()->auth_metatables));
    LJEG()->auth_metatable_count = 0;
    memset(&LJEG()->hidden_callers[0], 0, sizeof(LJEG()->hidden_callers));
    LJEG()->hidden_caller_count = 0;

    if (LJEG()->script_watcher)
    {
        lje_watcher_destroy(LJEG()->script_watcher);
        LJEG()->script_watcher = NULL;
    }
}

void lje_insert_spoof_record(GCfunc* spoof, GCfunc* target) {
    LJESpoofRecord* newRecord = (LJESpoofRecord*)malloc(sizeof(LJESpoofRecord));
    newRecord->spoof = spoof;
    newRecord->target = target;
    newRecord->next = LJEG()->spoof_record_root.next;
    LJEG()->spoof_record_root.next = newRecord;
}

GCfunc* lje_find_spoof_by_target(GCfunc* target) {
    for (LJESpoofRecord* current = LJEG()->spoof_record_root.next; current != NULL; current = current->next) {
        if (current->target == target) {
            return current->spoof;
        }
    }

    return NULL;
}

void lje_remove_spoof_record_by_spoof(GCfunc* spoof) {
    LJESpoofRecord* current = &LJEG()->spoof_record_root;
    while (current->next) {
        if (current->next->spoof == spoof) {
            LJESpoofRecord* toDelete = current->next;
            current->next = toDelete->next;
            free(toDelete);
            return;
        }

        current = current->next;
    }
}

void lje_remove_spoof_record_by_target(GCfunc* target) {
    LJESpoofRecord* current = &LJEG()->spoof_record_root;
    while (current->next) {
        if (current->next->target == target) {
            LJESpoofRecord* toDelete = current->next;
            current->next = toDelete->next;
            free(toDelete);
            return;
        }

        current = current->next;
    }
}

void lje_clear_spoof_records() {
    LJESpoofRecord* current = LJEG()->spoof_record_root.next;
    while (current) {
        LJESpoofRecord* toDelete = current;
        current = current->next;
        free(toDelete);
    }

    LJEG()->spoof_record_root.next = NULL;
}

void lje_set_random_state(LJERandomState* prev, LJERandomState* new)
{
   return; // NO-OP for now
}

LJEMetatableRemap* lje_get_metatable_remap(const char* type_name)
{
    for (int i = 0; i < MAX_REMAP_TYPES; i++) {
        if (LJEG()->metatable_remaps[i].type_name &&
            strcmp(LJEG()->metatable_remaps[i].type_name, type_name) == 0) {
            return &LJEG()->metatable_remaps[i];
        }
    }
    return NULL;
}

void lje_set_metatable_remap(const char* type_name, GCtab* replacement)
{
    LJEMetatableRemap* remap = lje_get_metatable_remap(type_name);
    if (remap) {
        remap->replacement = replacement;
    } else {
        for (int i = 0; i < MAX_REMAP_TYPES; i++) {
            if (LJEG()->metatable_remaps[i].type_name == NULL) {
                LJEG()->metatable_remaps[i].type_name = type_name; /* string passed in is always a literal, so this is safe */
                LJEG()->metatable_remaps[i].replacement = replacement;
                break;
            }
        }
    }
}

void lje_auth_metatable(GCtab* mt)
{
    if (LJEG()->auth_metatable_count < MAX_AUTH_METATABLES) {
        LJEG()->auth_metatables[LJEG()->auth_metatable_count++] = mt;
    }
}

int lje_is_metatable_authorized(GCtab* mt)
{
    for (size_t i = 0; i < LJEG()->auth_metatable_count; i++) {
        if (LJEG()->auth_metatables[i] == mt) {
            return 1;
        }
    }
    return 0;
}

void lje_hide_caller(GCfunc* fn)
{
    if (LJEG()->hidden_caller_count < MAX_HIDDEN_CALLERS) {
        LJEG()->hidden_callers[LJEG()->hidden_caller_count++] = fn;
    }
}

int lje_is_caller_hidden(GCfunc* fn)
{
    for (size_t i = 0; i < LJEG()->hidden_caller_count; i++) {
        /* Little bit of a complicated equality check, but we need address handling since some GCfuncs are duped in the call stack. */
        GCfunc* hidden_fn = LJEG()->hidden_callers[i];
        if (isffunc(hidden_fn) && isffunc(fn) && hidden_fn->c.ffid == fn->c.ffid) {
            return 1;
        } else if (iscfunc(hidden_fn) && iscfunc(fn) && hidden_fn->c.f == fn->c.f) {
            return 1;
        } else if (isluafunc(hidden_fn) && isluafunc(fn) && funcproto(hidden_fn) == funcproto(fn))
        {
            return 1;
        }
    }
    return 0;
}