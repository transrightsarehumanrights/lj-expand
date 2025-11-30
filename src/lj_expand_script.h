#ifndef _LJ_EXPAND_SCRIPT_H
#define _LJ_EXPAND_SCRIPT_H
#include <stdint.h>

#define LJE_SCRIPT_MAIN "main.lua"
#define LJE_SCRIPT_FOLDER ".lje_scripts"

/* LJEScripts represent a folder containing Lua scripts to be loaded by LJE.
 * Each script has atleast one file, 'main.lua', which is the entrypoint. Additional files can be
 * included as needed relative to the script's path.
 */
typedef struct LJEScript
{
    const char* path;
    const char* name;
} LJEScript;

void lje_script_resolve_base(char* out_buffer, size_t buffer_size);
int lje_script_folder_exists();

#endif