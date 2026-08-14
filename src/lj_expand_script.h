#ifndef _LJ_EXPAND_SCRIPT_H
#define _LJ_EXPAND_SCRIPT_H
#include <stdint.h>

#include "lj_expand_host.h"

#define LJE_SCRIPT_MAIN "main.lua"
#define LJE_SCRIPT_PREINIT "preinit.lua"
#define LJE_SCRIPT_BOOT "boot.lua"
#define LJE_SCRIPT_SETTINGS_DEFAULT "settings.default.toml"
#define LJE_SCRIPT_FOLDER ".lje_scripts"
#define LJE_SCRIPT_DATA ".lje_script_data"
#define LJE_SCRIPT_MAX_ENGINE_CALL_HOOKS 16

typedef struct LJEScriptDependency
{
    char* name;
    char* author;
    // We can add version requirements later if needed
} LJEScriptDependency;

typedef struct LJEBinaryModuleDependency
{
    char* name;
} LJEBinaryModuleDependency;

typedef struct LJEScriptInfo
{
    const char* name;
    const char* version;
    const char* author;
    LJEScriptDependency* dependencies;
    size_t dependency_count;
    LJEBinaryModuleDependency* binary_dependencies;
    size_t binary_dependency_count;
} LJEScriptInfo;

typedef struct LJEEngineCallHook
{
  int is_post;
  int ref;
  /* Which universe's engine calls this hook listens to. */
  LJEHostId host;
  /* Registered from boot.lua, which runs once per process rather than once per
     client state. Those hooks outlive a client swap; everything else is dropped
     and re-registered when main.lua re-runs. */
  int boot_scoped;
} LJEEngineCallHook;

typedef struct LJEScriptExtraInfo
{
    int cleanup_ref_id;
    LJEEngineCallHook engine_call_hooks[LJE_SCRIPT_MAX_ENGINE_CALL_HOOKS];
    size_t engine_call_hook_count;
} LJEScriptExtraInfo;

/* LJEScripts represent a folder containing Lua scripts to be loaded by LJE.
 * Each script has atleast one file, 'main.lua', which is the entrypoint. Additional files can be
 * included as needed relative to the script's path.
 */
typedef struct LJEScript
{
    const char* folder;
    const char* main_path;
    const char* preinit_path;
    const char* boot_path;
    const char* name;
    LJEScriptInfo* info;
    LJEScriptExtraInfo* extra;
} LJEScript;

void lje_script_resolve_base(char* out_buffer, size_t buffer_size);
void lje_script_data_resolve_base(char* out_buffer, size_t buffer_size);
int lje_script_folder_exists();
int lje_script_folder_create();
int lje_script_data_folder_exists();
int lje_script_data_folder_create();
int lje_script_data_write_file(const char* relative_path, const char* data, size_t data_size);

/* Caller is responsible for freeing the returned data buffer. */
char* lje_script_data_read_file(const char* relative_path, size_t* out_data_size);

LJEScriptInfo* lje_script_parse_info(const char* info_path);
LJEScript* lje_script_load_all_scripts(size_t* out_script_count);
/* Computes a load order for the given scripts based on their dependencies.
 * Returns a newly allocated array of LJEScript in load order.
 */
LJEScript** lje_script_compute_load_order(size_t script_count, LJEScript* scripts);
void lje_script_free_scripts(LJEScript* scripts, size_t script_count);
char** lje_script_find(LJEScript* script, const char* relative_path, size_t* out_path_count);
/* Reads a script file relatively. */
char* lje_script_read(LJEScript* script, const char* relative_path, size_t* out_size);

#endif