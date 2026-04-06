#include "lj_expand_binary_module.h"
#include <windows.h> /* Let's be honest, Linux support is not coming anytime soon */
#include <wincrypt.h>

#include <stdio.h>
#define LJE_NO_OPAQUE_STATE
#include "lauxlib.h"
#include "lje_sdk.h"
#include "lj_expand_dirs.h"
#include "lj_expand_frame.h"
#include "lj_expand_globals.h"
#include "lua.h"

#define VIRUS_TOTAL_FILE_URL "https://www.virustotal.com/gui/file/%s"
static LjeApi* create_module_api();
static LjeApi* g_cached_module_api = NULL;

static char* hash_module(const char* full_path);
static LJEBinaryModule* load_module(const char* full_path, const char* name)
{
    char* module_hash = hash_module(full_path);
    if (!module_hash)
    {
        printf("[LJE] Failed to hash binary module: %s\n", full_path);
        return NULL;
    }

    HMODULE handle = LoadLibraryA(full_path);
    if (!handle) {
        printf("[LJE] Failed to load binary module: %s\n", full_path);
        return NULL;
    }

    LJEBinaryModule* module = (LJEBinaryModule*)malloc(sizeof(LJEBinaryModule));
    module->handle = handle;
    module->path = _strdup(full_path);
    module->name = _strdup(name);
    module->hash = module_hash;
    /* Remove .dll */
    module->name[strlen(module->name) - 4] = '\0';

    /* Call init if it exists */
    LjeModuleInitFunc init_func =
        (LjeModuleInitFunc)GetProcAddress(
            handle,
            "lje_module_init"
        );

    if (init_func)
    {
        LjeApi* api = create_module_api();
        int init_result = init_func(api);
        if (init_result != LJE_RESULT_OK)
        {
            printf("[LJE] Binary module %s failed to initialize (code %d)\n", full_path, init_result);
            if (init_result == LJE_RESULT_INCOMPATIBLE_SDK_VERSION)
            {
                printf("[LJE] Incompatible module version! Expected %d\n", LJE_SDK_VERSION);
            }

            FreeLibrary(handle);
            free((void*)module->path);
            free(module);
            return NULL;
        }
    }

    printf("[LJE] Loaded binary module: %s\n", full_path);
    printf("[LJE]    - Hash: %s\n", module->hash);
    printf("[LJE]    - VirusTotal (may not exist): " VIRUS_TOTAL_FILE_URL "\n", module->hash);
    return module;
}

static TValue *stkindex2adr(lua_State *L, int idx)
{
    if (idx > 0) {
        TValue *o = L->base + (idx - 1);
        return o < L->top ? o : niltv(L);
    } else {
        api_check(L, idx != 0 && -idx <= L->top - L->base);
        return L->top + idx;
    }
}

static void pushljeenv(lua_State* L)
{
    if (LJEG()->main_state != L)
    {
        printf("[LJE] Warning: pushljeenv called on non-main state!\n");
        return;
    }

    if (LJEG()->env_ref_id == LUA_NOREF)
    {
        printf("[LJE] Warning: pushljeenv called but no LJE environment set!\n");
        lua_pushnil(L);
        return;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, LJEG()->env_ref_id);
}

static void pop(lua_State* L)
{
    lua_pop(L, 1);
}

static int isnil(lua_State* L, int idx)
{
    return lua_isnil(L, idx);
}

static void mark_special(lua_State* L, int idx)
{
    cTValue* obj = stkindex2adr(L, idx);
    if (tvisfunc(obj))
    {
        GCfunc* func = funcV(obj);
        if (isluafunc(func))
            funcextend(func)->is_special = 1;
        else if (iscfunc(func))
            funcextendc(func)->is_special = 1;
        else
            printf("[LJE] Warning: Tried to mark non-function as special!\n");
    }
}

static LjeLuaApi* create_lua_api()
{
    LjeLuaApi* api = (LjeLuaApi*)malloc(sizeof(LjeLuaApi));

    api->pushstring = lua_pushstring;
    api->tolstring = lua_tolstring;
    api->pushnumber = lua_pushnumber;
    api->tonumber = lua_tonumber;
    api->pushinteger = lua_pushinteger;
    api->tointeger = lua_tointeger;
    api->pushlightuserdata = lua_pushlightuserdata;
    api->tolightuserdata = lua_touserdata;
    api->pushnil = lua_pushnil;
    api->pushcclosure = lua_pushcclosure;
    api->gettop = lua_gettop;
    api->settop = lua_settop;
    api->getfield = lua_getfield;
    api->setfield = lua_setfield;
    api->call = lua_call;
    api->pcall = lua_pcall;
    api->pushboolean = lua_pushboolean;
    api->toboolean = lua_toboolean;
    api->ref = luaL_ref;
    api->unref = luaL_unref;
    api->rawgeti = lua_rawgeti;
    api->rawseti = lua_rawseti;
    api->pushvalue = lua_pushvalue;
    api->createtable = lua_createtable;
    api->isnil = isnil;
    api->type = lua_type;
    api->typename_ = lua_typename;
    api->objlen = lua_objlen;
    api->pop = pop;
    api->pushljeenv = pushljeenv;
    api->newuserdata = lua_newuserdata;
    api->touserdata = lua_touserdata;
    api->setmetatable = lua_setmetatable;
    api->getupvalue = lua_getupvalue;
    api->is_lje_involved = lje_frame_is_lje_involved;
    api->mark_special = mark_special;
    api->pushlstring = lua_pushlstring;

    return api;
}

static LjeApi* create_module_api()
{
    if (g_cached_module_api)
        return g_cached_module_api;

    LjeApi* api = (LjeApi*)malloc(sizeof(LjeApi));
    api->version = LJE_SDK_VERSION;
    LjeLuaApi* lua_api = create_lua_api();
    api->lua = lua_api;

    g_cached_module_api = api;
    return api;
}

static char* hash_module(const char* full_path)
{
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    BYTE buffer[4096];
    DWORD bytes_read;
    DWORD hash_len = 32;
    BYTE hash_out[32];

    file = CreateFileA(full_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
        goto cleanup;

    if (!CryptAcquireContextW(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        goto cleanup;

    if (!CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash))
        goto cleanup;

    while (ReadFile(file, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
        if (!CryptHashData(hash, buffer, bytes_read, 0))
            goto cleanup;
    }

    if (CryptGetHashParam(hash, HP_HASHVAL, hash_out, &hash_len, 0))
    {
        char* hash_str = (char*)malloc(hash_len * 2 + 1);
        for (DWORD i = 0; i < hash_len; i++) {
            sprintf_s(&hash_str[i * 2], 3, "%02x", hash_out[i]);
        }
        hash_str[hash_len * 2] = '\0';
        return hash_str;
    }

    cleanup:
    if (hash) CryptDestroyHash(hash);
    if (prov) CryptReleaseContext(prov, 0);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return NULL;
}

static int has_user_been_warned()
{
    /* Check if the warning flag file is there */
    char warning_flag_path[MAX_PATH] = { 0 };
    char temp_path[MAX_PATH] = "%USERPROFILE%\\" LJE_BINARY_MODULE_FOLDER "\\" LJE_WARNED_FLAG;
    DWORD result = ExpandEnvironmentStringsA(temp_path, warning_flag_path, (DWORD)MAX_PATH);
    if (result == 0 || result > MAX_PATH)
    {
        return 0; /* Failed to expand, assume not warned */
    }

    DWORD attrs = GetFileAttributesA(warning_flag_path);
    return (attrs != INVALID_FILE_ATTRIBUTES);
}

static void write_warning()
{
    /* Create the warning flag file */
    char warning_flag_path[MAX_PATH] = { 0 };
    char temp_path[MAX_PATH] = "%USERPROFILE%\\" LJE_BINARY_MODULE_FOLDER "\\" LJE_WARNED_FLAG;
    DWORD result = ExpandEnvironmentStringsA(temp_path, warning_flag_path, (DWORD)MAX_PATH);
    if (result == 0 || result > MAX_PATH)
    {
        return; /* Failed to expand, cannot write */
    }

    HANDLE file = CreateFileA(
        warning_flag_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
}

static void resolve_base(char* path, size_t path_size)
{
    if (!lje_directory_get(LJE_DIR_BINARIES, path, path_size))
    {
        printf("[LJE] Failed to resolve binary module folder path!\n");
    }
}

void lje_binary_module_ensure_folder_exists()
{
    char base_path[MAX_PATH] = { 0 };
    resolve_base(base_path, MAX_PATH);

    DWORD attrs = GetFileAttributesA(base_path);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        printf("[LJE] Binary module folder not found, creating it now...\n");
        if (!CreateDirectoryA(base_path, NULL))
        {
            printf("[LJE] Failed to create binary module folder at %s\n", base_path);
        } else {
            printf("[LJE] Successfully created binary module folder at %s\n", base_path);
        }
    }
}

void lje_binary_module_load_all(
    size_t* out_module_count,
    LJEBinaryModule** out_modules
)
{
    if (!has_user_been_warned())
    {
        printf("[LJE] ************* WARNING *************\n");
        printf("[LJE] Loading binary modules from '%s'!\n", LJE_BINARY_MODULE_FOLDER);
        printf("[LJE] Make sure you **ALWAYS** verify the integrity of any binary modules you download from third-party sources!\n");
        printf("[LJE] Malicious modules can compromise your system security and personal data!\n");
        printf("[LJE] Ideally, only use modules from trusted sources or those that are open-source and publicly auditable.\n");
        printf("[LJE] You have been warned.\n");
        printf("[LJE] ***********************************\n");
        write_warning();

        printf("\a"); /* Beep to get attention */
        Sleep(5000); /* Give user time to read the warning */
    }

    char base_path[MAX_PATH] = { 0 };
    resolve_base(base_path, MAX_PATH);

    // Add a base directory in case the binary module has additional dependencies. We want them to
    // be able to load them from the same folder as the main module, so we don't add anything to GMod's
    // directory which is detectable.
    SetDllDirectoryA(base_path);
    WIN32_FIND_DATAA find_data;
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\lje-*.dll", base_path);

    HANDLE find_handle = FindFirstFileA(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
        *out_module_count = 0;
        *out_modules = NULL;
        return;
    }

    size_t capacity = 10;
    size_t count = 0;
    LJEBinaryModule* modules = (LJEBinaryModule*)malloc(sizeof(LJEBinaryModule) * capacity);

    do
    {
        if (count >= capacity)
        {
            capacity *= 2;
            modules = (LJEBinaryModule*)realloc(modules, sizeof(LJEBinaryModule) * capacity);
        }

        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s\\%s", base_path, find_data.cFileName);

        LJEBinaryModule* module = load_module(full_path, find_data.cFileName);
        if (module)
        {
            modules[count++] = *module;
            free(module);
        }
    } while (FindNextFileA(find_handle, &find_data));

    SetDllDirectoryA(NULL); /* Reset DLL directory to avoid affecting other loads */

    FindClose(find_handle);

    *out_module_count = count;
    *out_modules = modules;
}

void lje_binary_module_unload(LJEBinaryModule* module)
{
    if (!module) return;

    LjeModuleShutdownFunc shutdown_func =
        (LjeModuleShutdownFunc)GetProcAddress(
            (HMODULE)module->handle,
            "lje_module_shutdown"
        );

    if (shutdown_func)
    {
        shutdown_func();
    }

    FreeLibrary((HMODULE)module->handle);
    free((void*)module->path);
    free(module);
}

void lje_binary_module_run_preinit(LJEBinaryModule* module, lua_State* L)
{
    if (!module || !module->handle) return;

    LjeModulePreinitFunc preinit_func =
        (LjeModulePreinitFunc)GetProcAddress(
            (HMODULE)module->handle,
            "lje_module_preinit"
        );

    if (!preinit_func)
    {
        printf("[LJE] Binary module %s has no preinit function.\n", module->path);
        return;
    }

    preinit_func(L);
}