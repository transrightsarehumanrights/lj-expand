#ifndef _LJ_EXPAND_MODULE_H
#define _LJ_EXPAND_MODULE_H

#include "lj_obj.h"

typedef struct
{
    uintptr_t base;
    size_t size;
    void* handle;
} lje_Module;

LJ_FUNC lje_Module* lje_module_find(const char* name);
LJ_FUNC void lje_module_free(lje_Module* module);

LJ_FUNC void* lje_module_get_func(lje_Module* module, const char* func_name);

#endif