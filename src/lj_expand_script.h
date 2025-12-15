#ifndef _LJ_EXPAND_SCRIPT_H
#define _LJ_EXPAND_SCRIPT_H
#include <stdint.h>

#define LJE_SCRIPT_MAIN "main.lua"
#define LJE_SCRIPT_PREINIT "preinit.lua"
#define LJE_SCRIPT_FOLDER ".lje_scripts"

typedef struct LJEScriptInfo
{
    const char* name;
    const char* version;
    const char* author;
} LJEScriptInfo;

/* LJEScripts represent a folder containing Lua scripts to be loaded by LJE.
 * Each script has atleast one file, 'main.lua', which is the entrypoint. Additional files can be
 * included as needed relative to the script's path.
 */
typedef struct LJEScript
{
    const char* folder;
    const char* main_path;
    const char* preinit_path;
    const char* name;
    LJEScriptInfo* info;
} LJEScript;

void lje_script_resolve_base(char* out_buffer, size_t buffer_size);
int lje_script_folder_exists();
int lje_script_folder_create();
LJEScriptInfo* lje_script_parse_info(const char* info_path);
LJEScript* lje_script_load_all_scripts(size_t* out_script_count);
void lje_script_free_scripts(LJEScript* scripts, size_t script_count);

#endif