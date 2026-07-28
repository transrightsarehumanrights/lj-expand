#include "lj_expand_binary_module.h"

#include <stdio.h>

#define LJE_NO_OPAQUE_STATE
#include "lauxlib.h"
#include "lje_sdk.h"
#include "lj_expand_dirs.h"
#include "lj_expand_frame.h"
#include "lj_expand_globals.h"
#include "lj_expand_log.h"
#include "lj_expand_platform.h"
#include "lua.h"

#define VIRUS_TOTAL_FILE_URL "https://www.virustotal.com/gui/file/%s"
static LjeApi* create_module_api();
static LjeApi* g_cached_module_api = NULL;

static LJEBinaryModule* load_module(const char* full_path, const char* name)
{
  char module_hash[65];
  if (!lje_plat_sha256_file(full_path, module_hash))
  {
    LJE_ERROR("Failed to hash binary module: %s", full_path);
    return NULL;
  }

  void* handle = lje_plat_module_load(full_path);
  if (!handle)
  {
    LJE_ERROR("Failed to load binary module: %s", full_path);
    return NULL;
  }

  LJEBinaryModule* module = (LJEBinaryModule*)malloc(sizeof(LJEBinaryModule));
  module->handle = handle;
  module->path = lje_strdup(full_path);
  module->name = lje_strdup(name);
  module->hash = lje_strdup(module_hash);

  /* Remove module extension */
  size_t name_len = strlen(module->name);
  size_t ext_len = strlen(LJE_MODULE_EXT);
  if (name_len > ext_len)
    module->name[name_len - ext_len] = '\0';

  /* Call init if it exists */
  LjeModuleInitFunc init_func =
    (LjeModuleInitFunc)lje_plat_module_sym_raw(
      handle,
      "lje_module_init"
    );

  if (init_func)
  {
    LjeApi* api = create_module_api();
    int init_result = init_func(api);
    if (init_result != LJE_RESULT_OK)
    {
      LJE_ERROR("Binary module %s failed to initialize (code %d)", full_path, init_result);
      if (init_result == LJE_RESULT_INCOMPATIBLE_SDK_VERSION)
      {
        LJE_ERROR("Incompatible module version! Expected %d", LJE_SDK_VERSION);
      }

      lje_plat_module_unload(handle);
      free((void*)module->path);
      free(module);
      return NULL;
    }
  }

  LJE_SUCCESS("Loaded binary module: %s", full_path);
  LJE_INFO("   - Hash: %s", module->hash);
  LJE_INFO("   - VirusTotal (may not exist): " VIRUS_TOTAL_FILE_URL, module->hash);
  return module;
}

static TValue* stkindex2adr(lua_State* L, int idx)
{
  if (idx > 0)
  {
    TValue* o = L->base + (idx - 1);
    return o < L->top ? o : niltv(L);
  }
  else
  {
    api_check(L, idx != 0 && -idx <= L->top - L->base);
    return L->top + idx;
  }
}

static void pushljeenv(lua_State* L)
{
  // Used to actually push it, but LJE 2 removes the notion of a LJE-specific environment, so just return global env.
  lua_pushvalue(L, LUA_GLOBALSINDEX);
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
  // Binary module functions are registered into the registry — no special marking needed.
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
  api->tocfunction = lua_tocfunction;

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

static int has_user_been_warned()
{
  /* Check if the warning flag file is there */
  char warning_flag_path[LJE_PATH_MAX];
  char base[LJE_PATH_MAX];
  if (!lje_directory_get(LJE_DIR_BINARIES, base, sizeof(base)))
  {
    return 0; /* Failed to resolve, assume not warned to be safe */
  }

  if (!lje_path_join(warning_flag_path, sizeof(warning_flag_path), base, LJE_WARNED_FLAG))
    return 0;

  return lje_plat_fs_kind(warning_flag_path) != LJE_FS_MISSING;
}

static void write_warning()
{
  /* Create the warning flag file */
  char warning_flag_path[LJE_PATH_MAX];
  char base[LJE_PATH_MAX];
  if (!lje_directory_get(LJE_DIR_BINARIES, base, sizeof(base)))
  {
    return; /* Failed to resolve, can't write warning but at least we tried */
  }

  if (!lje_path_join(warning_flag_path, sizeof(warning_flag_path), base, LJE_WARNED_FLAG))
    return;

  FILE* f = fopen(warning_flag_path, "w");
  if (f) fclose(f);
}

static void resolve_base(char* path, size_t path_size)
{
  if (!lje_directory_get(LJE_DIR_BINARIES, path, path_size))
  {
    LJE_ERROR("Failed to resolve binary module folder path!");
  }
}

void lje_binary_module_ensure_folder_exists()
{
  char base_path[LJE_PATH_MAX];
  resolve_base(base_path, sizeof(base_path));

  if (lje_plat_fs_kind(base_path) != LJE_FS_DIR)
  {
    LJE_INFO("Binary module folder not found, creating it now...");
    if (!lje_plat_mkdir(base_path))
    {
      LJE_ERROR("Failed to create binary module folder at %s", base_path);
    }
    else
    {
      LJE_SUCCESS("Successfully created binary module folder at %s", base_path);
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
    LJE_WARN("************* WARNING *************");
    LJE_WARN("Loading binary modules from '%s'!", LJE_BINARY_MODULE_FOLDER);
    LJE_WARN(
      "Make sure you **ALWAYS** verify the integrity of any binary modules you download from third-party sources!");
    LJE_WARN("Malicious modules can compromise your system security and personal data!");
    LJE_WARN(
      "Ideally, only use modules from trusted sources or those that are open-source and publicly auditable.");
    LJE_WARN("You have been warned.");
    LJE_WARN("***********************************");
    write_warning();

    lje_log_raw("\a"); /* Beep to get attention */
    lje_plat_sleep_ms(5000); /* Give user time to read the warning */
  }

  char base_path[LJE_PATH_MAX];
  resolve_base(base_path, sizeof(base_path));

  // Add a base directory in case the binary module has additional dependencies. We want them to
  // be able to load them from the same folder as the main module, so we don't add anything to GMod's
  // directory which is detectable.
  lje_plat_module_search_dir(base_path);

  LJEPlatDir* d = lje_plat_dir_open(base_path, LJE_BINMODULE_GLOB);
  if (!d)
  {
    *out_module_count = 0;
    *out_modules = NULL;
    return;
  }

  size_t capacity = 10;
  size_t count = 0;
  LJEBinaryModule* modules = (LJEBinaryModule*)malloc(sizeof(LJEBinaryModule) * capacity);

  LJEPlatDirEntry entry;
  while (lje_plat_dir_next(d, &entry))
  {
    if (count >= capacity)
    {
      capacity *= 2;
      modules = (LJEBinaryModule*)realloc(modules, sizeof(LJEBinaryModule) * capacity);
    }

    char full_path[LJE_PATH_MAX];
    if (!lje_path_join(full_path, sizeof(full_path), base_path, entry.name))
      continue;

    LJEBinaryModule* module = load_module(full_path, entry.name);
    if (module)
    {
      modules[count++] = *module;
      free(module);
    }
  }

  lje_plat_module_search_dir(NULL); /* Reset DLL directory to avoid affecting other loads */

  lje_plat_dir_close(d);

  *out_module_count = count;
  *out_modules = modules;
}

void lje_binary_module_unload(LJEBinaryModule* module)
{
  if (!module) return;

  LjeModuleShutdownFunc shutdown_func =
    (LjeModuleShutdownFunc)lje_plat_module_sym_raw(
      module->handle,
      "lje_module_shutdown"
    );

  if (shutdown_func)
  {
    shutdown_func();
  }

  lje_plat_module_unload(module->handle);
  free((void*)module->path);
  free(module);
}

void lje_binary_module_run_preinit(LJEBinaryModule* module, lua_State* L)
{
  if (!module || !module->handle) return;

  LjeModulePreinitFunc preinit_func =
    (LjeModulePreinitFunc)lje_plat_module_sym_raw(
      module->handle,
      "lje_module_preinit"
    );

  if (!preinit_func)
  {
    LJE_INFO("Binary module %s has no preinit function.", module->path);
    return;
  }

  preinit_func(L);
}
