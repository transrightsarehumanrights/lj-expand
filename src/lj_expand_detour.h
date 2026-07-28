#ifndef _LJ_EXPAND_DETOUR_H
#define _LJ_EXPAND_DETOUR_H

#include "lj_obj.h"
#include "lj_expand_module.h"

#define lje_detour_export(mod, name, func) \
    void* orig_##name = lje_module_get_func(mod, #name); \
    if (orig_##name) { \
        lje_detour(orig_##name, (void*)func); \
    }

#define LJE_DETOUR_SIZE 12

// Dead simple detours. No original function is provided.
LJ_FUNC int lje_detour(void* target, void* detour);

/* Keeps the original bytes of the hooked function */
typedef struct {
    void* target;
    void* detour;
    uint8_t original[LJE_DETOUR_SIZE];
    int installed;
} LJEDetourHook;

int lje_detour_hook(LJEDetourHook* hook, void* target, void* detour);
int lje_detour_suspend(LJEDetourHook* hook);
int lje_detour_resume(LJEDetourHook* hook);

#endif