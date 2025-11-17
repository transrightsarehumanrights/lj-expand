#include "lj_expand_module.h"

#ifdef LJ_TARGET_WINDOWS
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
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

typedef struct
{
    uint8_t value;
    uint8_t mask;
} PatternByte;

PatternByte* make_pattern_from_str(const char* pattern, size_t *byteLengthOut)
{
    size_t byteLength = 0;
    size_t patternLen = strlen(pattern);
    for (char* c = pattern; c < pattern + patternLen; )
    {
        if (*c == ' ')
        {
            c++;
            continue;
        }

        byteLength++;
        c += 2;
    }

    PatternByte* patternBytes = (PatternByte*)malloc(sizeof(PatternByte) * byteLength);
    if (patternBytes == NULL) {
        return NULL;
    }

    memset(patternBytes, 0, sizeof(PatternByte) * byteLength);

    for (size_t i = 0, j = 0; i < patternLen; )
    {
        if (pattern[i] == ' ')
        {
            i++;
            continue;
        }

        if (pattern[i] == '?' && pattern[i + 1] == '?')
        {
            patternBytes[j].value = 0x00;
            patternBytes[j].mask = 0x00;
        }
        else
        {
            char byteStr[3] = { pattern[i], pattern[i + 1], '\0' };
            patternBytes[j].value = (uint8_t)strtoul(byteStr, NULL, 16);
            patternBytes[j].mask = 0xFF;
        }

        i += 2;
        j++;
    }

    *byteLengthOut = byteLength;
    return patternBytes;
}

void* lje_module_scan(lje_Module* module, const char* pattern)
{
    if (module == NULL || pattern == NULL) {
        return NULL;
    }

    size_t patternLen = strlen(pattern);
    uint8_t* scanStart = (uint8_t*)module->base;
    uint8_t* scanEnd = scanStart + module->size;

    size_t patternLenBytes = 0;
    PatternByte* patternBytes = make_pattern_from_str(pattern, &patternLenBytes);
    if (patternBytes == NULL)
    {
        return NULL;
    }

    for (uint8_t* p = scanStart; p <= scanEnd - patternLen; p++) {
        size_t i;

        for (i = 0; i < patternLenBytes; i++) {
            if ((p[i] & patternBytes[i].mask) != patternBytes[i].value) {
                break;
            }
        }

        if (i == patternLenBytes) {
            free(patternBytes);
            return (void*)p;
        }
    }

    free(patternBytes);
    return NULL;
}