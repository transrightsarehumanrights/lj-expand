#ifndef _LJ_EXPAND_DETOUR_H
#define _LJ_EXPAND_DETOUR_H

#include "lj_obj.h"
#include "lj_expand_module.h"

#define lje_detour_export(mod, name, func) \
    void* orig_##name = lje_module_get_func(mod, #name); \
    if (orig_##name) { \
        lje_detour(orig_##name, (void*)func); \
    }

// Dead simple detours. No original function is provided.
LJ_FUNC int lje_detour(void* target, void* detour);

#endif