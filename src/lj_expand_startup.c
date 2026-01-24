#include "lj_expand_startup.h"

#include "lauxlib.h"
#include "lj_expand_globals.h"
#include "lj_expand_lib.h"
#include "lj_expand_module.h"
#include "stdio.h"

#include "generated/lje_preinit.h"

static char* load_lua_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("[LJE] Failed to open script file: %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

// Bit of a hack, so we want to run with GMod's loadbufferx because their bytecode compiler
// is heavily modified to support stupid shit like C operators and continue.
typedef int (*luaL_loadbufferx_t)(lua_State* L, const char* buff, size_t size, const char* name, const char* mode);
// Same thing for pcall, unfortunately
typedef int (*lua_pcall_t)(lua_State* L, int nargs, int nresults, int errfunc);

static int resolve_original_functions(luaL_loadbufferx_t* out_loadbufferx, lua_pcall_t* out_pcall)
{
    lje_Module* mod = lje_module_find("lua_shared.dll");
    if (mod)
    {
        *out_loadbufferx = (luaL_loadbufferx_t)lje_module_get_func(mod, "luaL_loadbufferx");
        if (out_pcall)
            *out_pcall = (lua_pcall_t)lje_module_get_func(mod, "lua_pcall");

        return 1;
    }

    return 0;
}

void lje_startup_execute(lua_State* L, LJEScript* script, const char* path) {
    LJEG()->main_state = L;
    LJEG()->current_script = script;

    luaL_loadbufferx_t original_loadbufferx = NULL;
    lua_pcall_t original_pcall = NULL;
    if (!resolve_original_functions(&original_loadbufferx, &original_pcall))
    {
        printf("[LJE] Failed to resolve original startup functions necessary...\n");
        return;
    }

    path = path ? path : script->main_path;
    char* script_file = load_lua_file(path);

    char chunkname[LUA_IDSIZE] = { 0 };
    strncat_s(chunkname, LUA_IDSIZE, "@lje_script:", _TRUNCATE);
    strncat_s(chunkname, LUA_IDSIZE, script->info->name, _TRUNCATE);
    chunkname[LUA_IDSIZE - 1] = '\0';

    if (script_file)
    {
        printf("[LJE] Executing script '%s'...\n", script->name);
        LJEG()->flag_lje_protos = 1;
        if (original_loadbufferx(L, script_file, strlen(script_file), chunkname, NULL) == 0)
        {
            LJEG()->flag_lje_protos = 0;
            // Mark it as a special function first
            GCfunc* func = funcV(L->top-1);
            LJEfunc* ljeFn = funcextend(func); // guaranteed to exist since it's a Lua function
            ljeFn->is_special = 1;

            if (LJEG()->env_ref_id != LUA_NOREF)
            {
                // Set the environment if it exists
                lua_rawgeti(L, LUA_REGISTRYINDEX, LJEG()->env_ref_id);
                lua_setfenv(L, -2);
            } else
            {
                printf("[LJE WARNING] No custom environment set for script? Probably not intentional.\n");
            }

            // Disable hooks during execution
            LJEG()->skip_hooks = 1;
            if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) /* mental note: figure out why this seems to randomly not work? */
            {
                LJEG()->skip_hooks = 0;

                printf("[LJE] Error executing script: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1); // Pop error message
            } else
            {
                LJEG()->skip_hooks = 0;
            }
        }
        else
        {
            LJEG()->flag_lje_protos = 0;
            printf("[LJE] Error loading script: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1); // Pop error message
        }

        free(script_file);
    }

    LJEG()->current_script = NULL;
}

int lje_startup_include(lua_State* L, const char* relative_path, int execute) {
    luaL_loadbufferx_t original_loadbufferx = NULL;
    lua_pcall_t original_pcall = NULL;
    if (!resolve_original_functions(&original_loadbufferx, &original_pcall))
    {
        printf("[LJE] Failed to resolve original startup functions necessary...\n");
        return 0;
    }

    if (!LJEG()->current_script)
    {
        printf("[LJE] No current script context for include!\n");
        return 0;
    }

    // compute relative to current script
    char full_path[512] = { 0 };
    strncat_s(full_path, 512, LJEG()->current_script->folder, _TRUNCATE);
    strncat_s(full_path, 512, relative_path, _TRUNCATE);

    char chunkname[LUA_IDSIZE] = { 0 };
    strncat_s(chunkname, LUA_IDSIZE, "@lje_include:", _TRUNCATE);
    strncat_s(chunkname, LUA_IDSIZE, relative_path, _TRUNCATE);

    char* buffer = load_lua_file(full_path);
    LJEG()->flag_lje_protos = 1;
    if (original_loadbufferx(L, buffer, strlen(buffer), chunkname, NULL) == 0)
    {
        LJEG()->flag_lje_protos = 0;
        if (LJEG()->env_ref_id != LUA_NOREF)
        {
            // Set the environment if it exists
            lua_rawgeti(L, LUA_REGISTRYINDEX, LJEG()->env_ref_id);
            lua_setfenv(L, -2);
        }

        if (!execute)
        {
            // Just return the loaded function
            free(buffer);
            return 1;
        }

        if (original_pcall(L, 0, LUA_MULTRET, 0) != 0)
        {
            printf("[LJE] Error executing include script: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1); // Pop error message
        } else
        {
            free(buffer);
            return lua_gettop(L) - 1;
        }
    }
    else
    {
        LJEG()->flag_lje_protos = 0;
        printf("[LJE] Error loading include script: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); // Pop error message
    }

    free(buffer);
    return 0;
}

void lje_startup_preinit(lua_State* L) {
    printf("[LJE] Running pre-initialization script...\n");
    LJEG()->main_state = L;

    luaL_loadbufferx_t original_loadbufferx = NULL;
    lua_pcall_t original_pcall = NULL;
    if (!resolve_original_functions(&original_loadbufferx, &original_pcall))
    {
        printf("[LJE] Failed to resolve original startup functions necessary...\n");
        return;
    }

    char* script = lje_preinit_data;
    LJEG()->flag_lje_protos = 1;
    if (original_loadbufferx(L, script, strlen(script), "@lje_preinit", NULL) == 0)
    {
        LJEG()->flag_lje_protos = 0;
        if (original_pcall(L, 0, 0, 0) != 0)
        {
            printf("[LJE ERROR] Error executing pre-initialization script: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1); // Pop error message
        } else
        {
            printf("[LJE] Pre-initialization script executed successfully.\n");
            lje_removefuncs(L); // Remove our global functions after preinit, as it is not secure to leave them there
            printf("[LJE] Removed LJE global functions after pre-initialization.\n");
        }
    }
    else
    {
        LJEG()->flag_lje_protos = 0;
        printf("[LJE ERROR] Error loading pre-initialization script: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); // Pop error message
    }

    return;
}

int lje_startup_compile(lua_State* L, const char* source) {
    luaL_loadbufferx_t original_loadbufferx = NULL;
    if (!resolve_original_functions(&original_loadbufferx, NULL))
    {
        printf("[LJE] Failed to resolve original startup functions necessary...\n");
        return 0;
    }

    LJEG()->flag_lje_protos = 1;
    if (original_loadbufferx(L, source, strlen(source), "@lje_dynamic_compile", NULL) == 0)
    {
        LJEG()->flag_lje_protos = 0;
        // Mark it as a special function first
        GCfunc* func = funcV(L->top-1);
        LJEfunc* ljeFn = funcextend(func); // guaranteed to exist since it's a Lua function
        ljeFn->is_special = 1;

        return 1; // success, function is on top of stack
    }
    else
    {
        LJEG()->flag_lje_protos = 0;
        printf("[LJE ERROR] Error compiling dynamic script: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); // Pop error message
    }

    return 0;
}