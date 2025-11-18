#ifndef _LJ_EXPAND_DETOUR_H
#define _LJ_EXPAND_DETOUR_H

#include "lj_obj.h"
#include "lj_expand_module.h"

#define lje_detour_export(mod, name, func) \
    void* orig_##name = lje_module_get_func(mod, #name); \
    if (orig_##name) { \
        lje_detour(orig_##name, (void*)func); \
    }

// Macro for easily remapping a specific LuaJIT function to our own. Requires a signature.
#define lje_remap(mod, name) \
    void* orig_##name = lje_module_scan(mod, lje_sig(name)); \
    if (orig_##name) { \
        printf("[LJE] Remapping " #name " from %p to %p\n", orig_##name, (void*)name); \
        lje_detour(orig_##name, (void*)name); \
    } else { \
        printf("[LJE ]Failed to find " #name " for detouring!\n"); \
    } \

// Dead simple detours. No original function is provided.
LJ_FUNC int lje_detour(void* target, void* detour);

#endif