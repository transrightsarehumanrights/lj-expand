#ifndef _LJ_EXPAND_MODULE_H
#define _LJ_EXPAND_MODULE_H

#include "lj_obj.h"
#include "lj_expand_platform.h"

typedef LJEPlatModule lje_Module;

LJ_FUNC lje_Module* lje_module_find(const char* name);
LJ_FUNC void lje_module_free(lje_Module* module);

LJ_FUNC void* lje_module_get_func(lje_Module* module, const char* func_name);
// Ghidra byte strings, e.g: "41 89 5C 24 08 55 56 57 48 83 EC 20"
LJ_FUNC void* lje_module_scan(lje_Module* module, const char* pattern);
#endif