#pragma once

#define LJE_BINARY_MODULE_FOLDER ".lje_modules"
#define LJE_WARNED_FLAG ".lje_bin_warned"

#include <stdio.h>

#include "lua.h"

typedef struct LJEBinaryModule
{
    void* handle;
    const char* path;
    char* name;
    /* Hex-encoded SHA256 hash of the module file for integrity checking */
    const char *hash;
} LJEBinaryModule;

void lje_binary_module_ensure_folder_exists(void);

void lje_binary_module_load_all(
    size_t* out_module_count,
    LJEBinaryModule** out_modules
);

void lje_binary_module_unload(LJEBinaryModule* module);

void lje_binary_module_run_preinit(LJEBinaryModule* module, lua_State* L);