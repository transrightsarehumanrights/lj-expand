#include "lj_expand_module.h"

#ifdef LJ_TARGET_WINDOWS
#include <windows.h>
#include <psapi.h>
#endif

lje_Module* lje_module_find(const char* name)
{
#ifdef LJ_TARGET_WINDOWS
    HMODULE hModule = GetModuleHandleA(name);
    if (hModule == NULL) {
        return NULL;
    }

    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) {
        return NULL;
    }

    lje_Module* module = (lje_Module*)malloc(sizeof(lje_Module));
    if (module == NULL) {
        return NULL;
    }

    module->base = (uintptr_t)modInfo.lpBaseOfDll;
    module->size = (size_t)modInfo.SizeOfImage;
    module->handle = (void*)hModule;

    return module;
#else
#error "lje_module_find is only implemented for Windows."
#endif
}

void lje_module_free(lje_Module* module)
{
    if (module) {
        free(module);
    }
}

void* lje_module_get_func(lje_Module* module, const char* func_name)
{
    if (module == NULL || func_name == NULL) {
        return NULL;
    }

#ifdef LJ_TARGET_WINDOWS
    HMODULE hModule = (HMODULE)module->handle;
    return (void*)GetProcAddress(hModule, func_name);
#else
#error "lje_module_get_func is only implemented for Windows."
#endif
}