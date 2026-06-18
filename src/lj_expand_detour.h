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
        LJE_INFO("Remapping " #name " from %p to %p", orig_##name, (void*)name); \
        lje_detour(orig_##name, (void*)name); \
    } else { \
        LJE_ERROR("Failed to find " #name " for detouring!"); \
    } \

// Dead simple detours. No original function is provided.
LJ_FUNC int lje_detour(void* target, void* detour);
// Detour, but the ability to trampoline back to it. Useful basically only for state-related functions
int lje_detour_trampoline(void* target, void* detour, void** out_trampoline);

#endif