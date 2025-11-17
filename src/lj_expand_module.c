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

void* lje_module_scan(lje_Module* module, const char* pattern)
{
    if (module == NULL || pattern == NULL) {
        return NULL;
    }

    size_t patternLen = strlen(pattern);
    uint8_t* scanStart = (uint8_t*)module->base;
    uint8_t* scanEnd = scanStart + module->size - patternLen;

    // construct byte pattern from string
    size_t byteStringSize = 0;
    for (int i = 0; i < patternLen; i++) {
        if (pattern[i] != ' ') {
            byteStringSize++;
            i += 1; // Skip next character as it's part of the byte
        }
    }

    uint8_t* bytePattern = (uint8_t*)malloc(byteStringSize);
    if (bytePattern == NULL)
    {
        return NULL;
    }

    for (int i = 0, j = 0; i < patternLen; ) {
        if (pattern[i] == ' ') {
            i++;
            continue;
        }

        char byteStr[3] = { pattern[i], pattern[i + 1], '\0' };
        bytePattern[j++] = (uint8_t)strtoul(byteStr, NULL, 16);
        i += 2;
    }

    for (uint8_t* p = scanStart; p <= scanEnd; p++) {
        if (memcmp(p, bytePattern, byteStringSize) == 0) {
            free(bytePattern);
            return (void*)p;
        }
    }

    free(bytePattern);
    return NULL;
}