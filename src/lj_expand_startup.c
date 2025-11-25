#include "lj_expand_startup.h"

#include "lauxlib.h"
#include "lj_expand_globals.h"
#include "lj_expand_lib.h"
#include "lj_expand_module.h"
#include "lj_lib.h"
#include "stdio.h"

static char* load_lua_file(const char* relative_path)
{
    char* path = lje_concat_path(relative_path);
    FILE* file = fopen(path, "rb");
    free(path);

    if (!file) {
        printf("[LJE] Failed to open startup script: %s\n", path);
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
        *out_pcall = (lua_pcall_t)lje_module_get_func(mod, "lua_pcall");

        return 1;
    }

    return 0;
}

void lje_startup_execute(lua_State* L) {
    LJEG()->main_state = L;

    luaL_loadbufferx_t original_loadbufferx = NULL;
    lua_pcall_t original_pcall = NULL;
    if (!resolve_original_functions(&original_loadbufferx, &original_pcall))
    {
        printf("[LJE] Failed to resolve original startup functions necessary...\n");
        return;
    }

    char* script = load_lua_file(LJE_STARTUP_FILE);
    if (script)
    {
        printf("[LJE] Executing startup script...\n");
        if (original_loadbufferx(L, script, strlen(script), "@lje_startup", NULL) == 0)
        {
            // Mark it as a special function first
            GCfunc* func = funcV(L->top-1);
            LJEfunc* ljeFn = funcextend(func); // guaranteed to exist since it's a Lua function
            ljeFn->is_special = 1;

            if (LJEG()->env_ref_id != 0)
            {
                // Set the environment if it exists
                lua_rawgeti(L, LUA_REGISTRYINDEX, LJEG()->env_ref_id);
                lua_setfenv(L, -2);
            } else
            {
                printf("[LJE] No custom environment set for startup script? Probably not intentional.\n");
            }

            if (original_pcall(L, 0, 0, 0) != 0)
            {
                printf("[LJE] Error executing startup script: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1); // Pop error message
            } else
            {
                printf("[LJE] Startup script executed successfully.\n");
            }
        }
        else
        {
            printf("[LJE] Error loading startup script: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1); // Pop error message
        }

        free(script);
    }
}

int lje_startup_include(lua_State* L, const char* relative_path, int execute) {
    luaL_loadbufferx_t original_loadbufferx = NULL;
    lua_pcall_t original_pcall = NULL;
    if (!resolve_original_functions(&original_loadbufferx, &original_pcall))
    {
        printf("[LJE] Failed to resolve original startup functions necessary...\n");
        return 0;
    }

    char* buffer = load_lua_file(relative_path);
    printf("[LJE] Including script: %s\n", relative_path);
    if (original_loadbufferx(L, buffer, strlen(buffer), "@lje_include", NULL) == 0)
    {
        if (!execute)
        {
            // Just return the loaded function
            printf("[LJE] Include script loaded successfully (not executed).\n");
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
            printf("[LJE] Include script executed successfully.\n");
            return lua_gettop(L);
        }
    }
    else
    {
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

    char* script = load_lua_file(LJE_PREINIT_FILE);
    if (!script)
    {
        printf("[LJE ERROR] No pre-initialization script found, skipping.\n");
        return;
    }

    if (original_loadbufferx(L, script, strlen(script), "@lje_preinit", NULL) == 0)
    {
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
        printf("[LJE ERROR] Error loading pre-initialization script: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); // Pop error message
    }

    free(script);
    return;
}