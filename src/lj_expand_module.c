#include "lj_expand_module.h"
#include <stdlib.h>
#include <string.h>

lje_Module* lje_module_find(const char* name)
{
    lje_Module* module = (lje_Module*)malloc(sizeof(lje_Module));
    if (module == NULL) return NULL;

    if (!lje_plat_module_find(name, module)) {
        free(module);
        return NULL;
    }

    return module;
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

    return lje_plat_module_sym(module, func_name);
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