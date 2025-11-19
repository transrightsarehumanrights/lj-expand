#include "lj_expand_startup.h"

#include "lj_expand_module.h"
#include "lj_lib.h"
#include "stdio.h"

static char* load_startup_file()
{
    const char* path = LJE_STARTUP_SCRIPT_PATH;
    FILE* file = fopen(path, "rb");
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

// Bit of a hack, so we want to run with GMod's loadbufferx. I know.
typedef int (*luaL_loadbufferx_t)(lua_State* L, const char* buff, size_t size, const char* name, const char* mode);

static luaL_loadbufferx_t resolve_original_loadbufferx()
{
    lje_Module* mod = lje_module_find("lua_shared.dll");
    if (mod)
    {
        return (luaL_loadbufferx_t)lje_module_get_func(mod, "luaL_loadbufferx");
    }

    return NULL;
}

void lje_startup_execute(lua_State* L) {
    // Currently empty, but can be used for startup initialization code.
    luaL_loadbufferx_t original_loadbufferx = resolve_original_loadbufferx();
    if (!original_loadbufferx)
    {
        printf("[LJE] Failed to resolve original luaL_loadbufferx!\n");
        return;
    }

    char* script = load_startup_file();
    if (script)
    {
        printf("[LJE] Executing startup script...\n");
        if (original_loadbufferx(L, script, strlen(script), "@lje_startup", NULL) == 0)
        {
            // Mark it as a special function first
            GCfunc* func = funcV(L->top-1);
            LJEfunc* ljeFn = funcextend(func); // guaranteed to exist since it's a Lua function
            ljeFn->is_special = 1;

            if (lua_pcall(L, 0, 0, 0) != 0)
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
