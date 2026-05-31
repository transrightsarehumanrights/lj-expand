#include "lj_expand_lib.h"

#include "lauxlib.h"
#include "lj_buf.h"
#include "lj_debug.h"
#include "lj_dispatch.h"
#include "lj_lib.h"
#include "lj_err.h"
#include "lj_expand_cmd.h"
#include "lj_expand_frame.h"
#include "lj_expand_globals.h"
#include "lj_expand_isolation.h"
#include "lj_expand_proxy.h"
#include "lj_expand_startup.h"
#include "lj_frame.h"
#include "lj_gc.h"
#include "lj_tab.h"
#include "lj_trace.h"
#include "lj_vm.h"

int lje_get_func_type(lua_State* L)
{
  GCfunc* func = lj_lib_checkfunc(L, 1);
  lua_pushnumber(L, func->c.ffid);
  return 1;
}

int lje_con_print(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    printf("[LJE CONSOLE] %s\n", msg);
    return 0;
}

int lje_set_push_string_callback(lua_State* L)
{
  GCfunc* callback = lj_lib_checkfunc(L, 1);
  if (!isluafunc(callback))
  {
    lj_err_arg(L, 1, LJ_ERR_NOLFUNC);
  }

  LJEG()->push_string_ref_id = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

int lje_enable_hooks(lua_State* L)
{
  LJEG()->skip_hooks = 0;
  return 0;
}

int lje_disable_hooks(lua_State* L)
{
  LJEG()->skip_hooks = 1;
  return 0;
}

int lje_ignore_fn_on_hook(lua_State* L)
{
  GCfunc* func = lj_lib_checkfunc(L, 1);
  setgcrefp(LJEG()->ignore_fn_on_hook, func);
  return 0;
}

int lje_include(lua_State* L)
{
  const char* relative_path = luaL_checkstring(L, 1);
  int execute = lua_gettop(L) < 2 || lua_toboolean(L, 2);

  return lje_startup_include(L, relative_path, execute);
}

int lje_get_env(lua_State* L)
{
  if (LJEG()->env_ref_id != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, LJEG()->env_ref_id);
    return 1;
  }

  return 0;
}

int lje_set_env(lua_State* L)
{
  lj_lib_checktab(L, 1);
  if (LJEG()->env_ref_id != LUA_NOREF)
  {
    luaL_unref(L, LUA_REGISTRYINDEX, LJEG()->env_ref_id);
    LJEG()->env_ref_id = LUA_NOREF;
  }

  lua_pushvalue(L, 1);
  LJEG()->env_ref_id = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

int lje_get_bytecode_hash(lua_State* L)
{
  GCfunc* func = lj_lib_checkfunc(L, 1);
  if (!isluafunc(func))
  {
    lj_err_arg(L, 1, LJ_ERR_NOLFUNC);
  }

  GCproto* pt = funcproto(func);
  // Use a simple hash to create a unique identifier for the bytecode
  uint32_t hash = 2166136261u;
  const uint8_t* bytecode = proto_bc(pt);
  size_t size = pt->sizebc * sizeof(BCIns);
  for (size_t i = 0; i < size; i++)
  {
    hash ^= bytecode[i];
    hash *= 16777619;
  }

  hash = hash ^ (hash >> 16);
  lua_pushinteger(L, hash);

  return 1;
}

int lje_get_call_stack(lua_State* L)
{
  // Quick way to get the call stack as a table of structures.
  // You technically can do this from Lua, but this is easier and faster.
  // Plus it deals with the fact call stacks aren't fixed so you dont need to implement
  // a while (true) loop in Lua.

  lua_newtable(L);
  for (int i = 0; ; i++)
  {
    int size = 0;
    cTValue* frame = lj_debug_frame(L, i, &size);

    if (frame == NULL)
    {
      break;
    }

    lua_newtable(L);
    lua_pushinteger(L, i + 1);
    lua_setfield(L, -2, "level");

    // If-else chain might not be pretty, but these macros don't necessarily operate
    // on the same data.
    GCfunc* func = frame_func(frame);
    if (func)
    {
      if (isluafunc(func))
      {
        lua_pushliteral(L, "lua");
        lua_setfield(L, -2, "type");

        GCproto* pt = funcproto(func);
        if (pt && proto_chunkname(pt))
        {
          lua_pushstring(L, proto_chunknamestr(pt));
          lua_setfield(L, -2, "chunkname");
        }

        setfuncV(L, L->top++, func);
        lua_setfield(L, -2, "func");
      } else
      {
        lua_pushliteral(L, "c");
        lua_setfield(L, -2, "type");

        setfuncV(L, L->top++, func);
        lua_setfield(L, -2, "func");
      }
    }

    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

int lje_get_registry(lua_State* L)
{
  lua_pushvalue(L, LUA_REGISTRYINDEX);
  return 1;
}

int lje_get_gc_total(lua_State* L)
{
  lua_pushinteger(L, (lua_Integer)G(L)->gc.total);
  return 1;
}

int lje_set_gc_total(lua_State* L)
{
  size_t new_total = (size_t)luaL_checkinteger(L, 1);
  G(L)->gc.total = new_total;
  return 0;
}

int lje_run_full_gc(lua_State* L)
{
  lj_gc_fullgc(L);
  return 0;
}

int lje_get_current_script(lua_State* L)
{
  LJEScript* script = LJEG()->current_script;
  if (script)
  {
    lua_pushstring(L, script->name);
    return 1;
  }

  return 0;
}

int lje_get_current_script_path(lua_State* L)
{
  LJEScript* script = LJEG()->current_script;
  if (script)
  {
    lua_pushstring(L, script->folder);
    return 1;
  }

  return 0;
}

int lje_find_script_files(lua_State* L)
{
  if (!LJEG()->current_script)
  {
    lua_pushnil(L);
    return 1;
  }

  const char* search_path = luaL_checkstring(L, 1);
  size_t file_count = 0;
  char** files = lje_script_find(LJEG()->current_script, search_path, &file_count);

  lua_newtable(L);
  for (size_t i = 0; i < file_count; i++)
  {
    lua_pushstring(L, files[i]);
    lua_rawseti(L, -2, i + 1);
    free(files[i]);
  }

  free(files);
  return 1;
}

int lje_is_lua_involved(lua_State* L)
{
  int frame_offset = luaL_optinteger(L, 1, 1);
  if (lje_frame_is_lua_involved(L, frame_offset))
  {
    lua_pushboolean(L, 1);
  } else
  {
    lua_pushboolean(L, 0);
  }

  return 1;
}

int lje_is_lje_involved(lua_State* L)
{
  int frame_offset = luaL_optinteger(L, 1, 1);
  int max_depth = luaL_optinteger(L, 2, -1);
  if (lje_frame_is_lje_involved(L, frame_offset, max_depth))
  {
    lua_pushboolean(L, 1);
  } else
  {
    lua_pushboolean(L, 0);
  }

  return 1;
}

int lje_is_lje_frame(lua_State* L)
{
  int frame_offset = luaL_optinteger(L, 1, 1);
  if (lje_frame_is_lje(L, frame_offset))
  {
    lua_pushboolean(L, 1);
  } else
  {
    lua_pushboolean(L, 0);
  }

  return 1;
}

int lje_disable_metatables(lua_State* L) {
  LJEG()->disable_metatables = 1;
  return 0;
}

int lje_enable_metatables(lua_State* L) {
  LJEG()->disable_metatables = 0;
  return 0;
}

int lje_set_script_hook_callback(lua_State* L)
{
  GCfunc* callback = lj_lib_checkfunc(L, 1);
  if (!isluafunc(callback))
  {
    lj_err_arg(L, 1, LJ_ERR_NOLFUNC);
  }

  LJEG()->script_hook_ref_id = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

int lje_data_write(lua_State* L)
{
  // Simple blob storage, give a name and data, stores it in the `.lje_script_data` folder
  const char* name = luaL_checkstring(L, 1);
  size_t data_size = 0;
  const char* data = luaL_checklstring(L, 2, &data_size);

  if (!lje_script_data_folder_exists())
  {
    if (!lje_script_data_folder_create())
    {
      printf("[LJE ERROR] Failed to create .lje_script_data folder!\n");
      lua_pushboolean(L, 0);
      return 1;
    }
  }

  // Always print out whats going on for the user to see
  printf("[LJE DATA]: Writing data to '%s' (%zu bytes)\n", name, data_size);
  if (lje_script_data_write_file(name, data, data_size))
  {
    lua_pushboolean(L, 1);
  } else
  {
    printf("[LJE DATA ERROR]: Failed to write data to '%s'\n", name);
    lua_pushboolean(L, 0);
  }

  return 1;
}

int lje_data_read(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);

  if (!lje_script_data_folder_exists())
  {
    printf("[LJE DATA]: .lje_script_data folder does not exist, cannot read data '%s'\n", name);
    return 0;
  }

  size_t data_size = 0;
  char* data = lje_script_data_read_file(name, &data_size);
  if (data)
  {
    lua_pushlstring(L, data, data_size);
    free(data);
    return 1;
  }

  return 0;
}

int lje_random_save(lua_State* L) { /*lje_save_random_state();*/ return 0;}
int lje_random_restore(lua_State* L) { /*lje_restore_random_state();*/ return 0;}

int lje_remap_metatable(lua_State* L)
{
  const char* type_name = luaL_checkstring(L, 1);
  GCtab* replacement = lj_lib_checktab(L, 2);

  lje_set_metatable_remap(type_name, replacement);
  printf("[LJE] Remapped metatable for type '%s'\n", type_name);
  return 0;
}

int lje_lib_auth_metatable(lua_State* L)
{
  GCtab* metatable = lj_lib_checktab(L, 1);
  lje_auth_metatable(metatable);
  printf("[LJE] Authorized metatable %p\n", (void*)metatable);

  return 0;
}

int lje_set_engine_call_hook(lua_State* L)
{
  GCfunc* callback = lj_lib_checkfunc(L, 1);
  LJEScript* current_script = LJEG()->current_script;
  if (!current_script)
  {
    lj_err_msg(L, LJ_ERR_LJE_NOSCRIPT);
  }

  if (!isluafunc(callback))
  {
    lj_err_arg(L, 1, LJ_ERR_NOLFUNC);
  }

  current_script->extra->engine_call_hook_ref_id = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

int lje_set_engine_call_hook_post(lua_State* L)
{
  int post = lua_toboolean(L, 1);
  LJEScript* current_script = LJEG()->current_script;
  if (!current_script)
  {
    lj_err_msg(L, LJ_ERR_LJE_NOSCRIPT);
  }

  current_script->extra->engine_call_post = post;
  return 0;
}

int lje_handle_engine_call(lua_State* L) { LJEG()->is_engine_call_handled = 1; return 0; }

int lje_compile_string(lua_State* L)
{
  const char* script = luaL_checkstring(L, 1);
  if (!script)
  {
    lj_err_arg(L, 1, LJ_ERR_NOVAL);
  }

  if (!lje_startup_compile(L, script))
    lua_pushnil(L);

  if (!lua_isnil(L, -1))
  {
    /* Set the environment if it exists */
    if (LJEG()->env_ref_id != LUA_NOREF)
    {
      lua_rawgeti(L, LUA_REGISTRYINDEX, LJEG()->env_ref_id);
      lua_setfenv(L, -2);
    }
  }

  return 1; // Return compiled function
}

int lje_set_show_special_frames(lua_State* L)
{
  int show = lua_toboolean(L, 1);
  LJEG()->show_special_frames = show;
  return 0;
}

static int lje_create_table(lua_State* L)
{
  int narr = luaL_optinteger(L, 1, 0);
  int nrec = luaL_optinteger(L, 2, 0);
  lua_createtable(L, narr, nrec);
  return 1;
}


// Wraps every C function we pull so we can securely call it.
static int lje_secure_gmod_api(lua_State* L)
{
  void* func_ptr = lua_touserdata(L, lua_upvalueindex(1));
  if (!func_ptr)
  {
    lua_pushnil(L);
    return 1;
  }

  // zero-copy function call! state redirection handles this,
  // we just need to dispatch the c function raw.
  lua_CFunction func = (lua_CFunction)func_ptr;
  // Latch incase isolation is being forced
  int redirection_required = LJEG()->redirect_to_isolation == 0;

  if (redirection_required)
    LJEG()->redirect_to_isolation = 1;
  int results = func(LJEG()->main_state); /* make it think it's running in the main state, it won't know any better! */
  if (redirection_required)
    LJEG()->redirect_to_isolation = 0;
  return results;
}

static int lje_secure_pull(lua_State* L)
{
  // Pulls a global out of the client state and returns it to the secure state.
  // This is necessary for secure scripts to be able to interact with the client state
  // in a limited way, without exposing the full global environment.
  if (!LJEG()->main_state)
  {
    luaL_error(L, "main state not set for secure pull");
    return 0;
  }

  const char* name = luaL_checkstring(L, 1); // e.g: "Msg" or "player.GetAll"
  if (!name)
  {
    lua_pushnil(L);
    return 1;
  }

  lua_State* L2 = LJEG()->main_state;
  global_State* g = G(L2);

  // We cannot interact with the main state directly. Since there is no active function call,
  // we must directly interface with the global environment. No stack at all.
  GCtab* current_tab = tabref(L2->env);
  if (name[0] == '_' && name[1] == 'R' && name[2] == '.')
  {
    // _R. is a special case to go to registry.
    TValue* registry = &g->registrytv;
    current_tab = tabV(registry);
    name += 3; // Skip past "_R."
  }

  cTValue* value = NULL;

  // Walk the dot-separated path. For each segment, look it up in current_tab.
  // If we hit a non-table mid-path, that's a failure. The final segment's value
  // is what we return.
  const char* segment_start = name;
  const char* p = name;

  for (;;)
  {
    // Advance p to the next '.' or end of string
    while (*p != '\0' && *p != '.') p++;

    size_t segment_len = (size_t)(p - segment_start);
    if (segment_len == 0)
    {
      printf("[LJE] Secure pull: empty path segment in '%s'\n", name);
      lua_pushnil(L);
      return 1;
    }

    value = lj_tab_getstr_raw(current_tab, segment_start, segment_len);

    if (!value || tvisnil(value))
    {
      printf("[LJE] Secure pull: '%.*s' not found in path '%s'\n",
             (int)segment_len, segment_start, name);
      lua_pushnil(L);
      return 1;
    }

    if (*p == '\0')
    {
      // This was the final segment.
      break;
    }

    // More segments to come — current value must be a table to descend into.
    if (!tvistab(value))
    {
      printf("[LJE] Secure pull: '%.*s' in path '%s' is not a table, cannot descend\n",
             (int)segment_len, segment_start, name);
      lua_pushnil(L);
      return 1;
    }

    current_tab = tabV(value);
    p++;  // skip the '.'
    segment_start = p;
  }

  printf("[LJE] Secure pull for '%s': %p\n", name, (void*)value);

  // Now `value` is the final resolved value. Currently we only allow C functions through.
  if (tvisfunc(value))
  {
    GCfunc* func = funcV(value);
    if (!iscfunc(func))
    {
      printf("[LJE] Secure pull for '%s' is not a C function, rejecting.\n", name);
      lua_pushnil(L);
      return 1;
    }

    lua_pushlightuserdata(L, func->c.f);
    lua_pushcclosure(L, lje_secure_gmod_api, 1);
    return 1;
  }

  // Not a function, so just copy it.
  lje_copy_to_isolated_state_tv(L2, L, value);
  return 1;
}

static int lje_secure_isolate(lua_State* L)
{
  int force_isolation = lua_toboolean(L, 1);
  LJEG()->redirect_to_isolation = force_isolation;
  return 0;
}

static int lje_proxy_type(lua_State* L)
{
  LJEProxy* proxy = lua_touserdata(L, 1);
  // We need as much performance as possible, so no type checking here.

  switch (proxy->host_type)
  {
  case ~LJ_TTAB:
    lua_pushliteral(L, "table");
    break;
  case ~LJ_TUDATA:
    lua_pushliteral(L, "userdata");
    break;
  default:
    lua_pushliteral(L, "unknown");
    break;
  }

  return 1;
}

static int lje_proxy_copy(lua_State* L)
{
  LJEProxy* proxy = lua_touserdata(L, 1);
  // Copies the given proxied object into the secure state.

  TValue proxy_object;
  setgcV(L, &proxy_object, proxy->host_obj, ~proxy->host_type);

  lje_copy_to_isolated_state_tv(LJEG()->main_state, L, &proxy_object);
  return 1;
}

#define LJE_SET_FUNC(name, func) \
  lua_pushcfunction(L, func); \
  lua_setfield(L, -2, name);

#define LJE_REMOVE_FUNC(name) \
  lua_pushnil(L); \
  lua_setfield(L, -2, name);

#define LJE_NEW_SECTION() \
  lua_createtable(L, 0, 0);

#define LJE_END_SECTION(section_name) \
  lua_setfield(L, -2, section_name);

void lje_addfuncs(lua_State* L) {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_createtable(L, 0, 0);

  /* LJE API START */

  /* base: global functions */
  LJE_SET_FUNC("include", lje_include);
  LJE_SET_FUNC("con_print", lje_con_print);

  /* func: anything to do with functions */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("compile", lje_compile_string);
    LJE_SET_FUNC("type", lje_get_func_type);
  LJE_END_SECTION("func");

  /* hooks: anything to do particularly with LuaJIT's debug hook functionality */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("enable", lje_enable_hooks);
    LJE_SET_FUNC("disable", lje_disable_hooks);
    LJE_SET_FUNC("ignore_fn_once", lje_ignore_fn_on_hook);
  LJE_END_SECTION("hooks");

  /* env: environment management, from lje to global.  */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("get", lje_get_env);
    LJE_SET_FUNC("set", lje_set_env);
    LJE_SET_FUNC("current_script", lje_get_current_script);
    LJE_SET_FUNC("current_script_path", lje_get_current_script_path);
    LJE_SET_FUNC("find_script_files", lje_find_script_files);
    LJE_SET_FUNC("is_lua_involved", lje_is_lua_involved);
    LJE_SET_FUNC("is_lje_involved", lje_is_lje_involved);
    LJE_SET_FUNC("is_lje_frame", lje_is_lje_frame);
    /* the following affect the global environment. */
    LJE_SET_FUNC("disable_metatables", lje_disable_metatables);
    LJE_SET_FUNC("enable_metatables", lje_enable_metatables);
    /* NOTE:
     * These functions essentially create a branch of the PRNG state, where
     * your calls in between a save and restore will only affect the state temporarily.
     *
     * This is useful for scripts that want to use randomness without detection, but do be warned:
     * If you save and restore once, you essentially erase all the randomness that happened in between.
     * If you immediately save and restore again, you'll get the same sequence of random numbers, which may be undesirable.
     *
     * Instead, you want to really only do this maybe once a frame or so. Either that, or you can seed the PRNG to force a new
     * sequence of random numbers, but you would have to manage that yourself. However, once your vacuum of code is over, it is likely
     * that other scripts will step the PRNG state, so randomness will *eventually* come back to normal.
     */
    LJE_SET_FUNC("save_random_state", lje_random_save);
    LJE_SET_FUNC("restore_random_state", lje_random_restore);
    LJE_SET_FUNC("remap_metatable", lje_remap_metatable);
    LJE_SET_FUNC("auth_metatable", lje_lib_auth_metatable);
    LJE_SET_FUNC("show_special_frames", lje_set_show_special_frames); /* useful for making inter-LJE debug introspection work */
  LJE_END_SECTION("env");

  /* util: unassorted utility functions */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("get_bytecode_hash", lje_get_bytecode_hash);
    LJE_SET_FUNC("get_call_stack", lje_get_call_stack);
    LJE_SET_FUNC("get_registry", lje_get_registry);
    LJE_SET_FUNC("set_push_string_callback", lje_set_push_string_callback);
    LJE_SET_FUNC("set_script_hook_callback", lje_set_script_hook_callback);
    LJE_SET_FUNC("create_table", lje_create_table);
  LJE_END_SECTION("util");

  /* gc: garbage collector manipulation */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("get_total", lje_get_gc_total);
    LJE_SET_FUNC("set_total", lje_set_gc_total);
    LJE_SET_FUNC("run_full_gc", lje_run_full_gc);
  LJE_END_SECTION("gc");

  /* vm: virtual machine manipulation */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("set_engine_call_hook", lje_set_engine_call_hook);
    LJE_SET_FUNC("set_engine_call_hook_post", lje_set_engine_call_hook_post);
    LJE_SET_FUNC("handle_engine_call", lje_handle_engine_call);
  LJE_END_SECTION("vm");

  /* data: simple data storage API */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("write", lje_data_write);
    LJE_SET_FUNC("read", lje_data_read);
  LJE_END_SECTION("data");

  if (L == LJEG()->isolated_state)
  {
    /* secure: API for secure state only tasks */
    LJE_NEW_SECTION()
      LJE_SET_FUNC("pull", lje_secure_pull);
      LJE_SET_FUNC("isolate", lje_secure_isolate);
    LJE_END_SECTION("secure");

    /* proxy: API for interacting with proxy objects in the secure state */
    LJE_NEW_SECTION()
      LJE_SET_FUNC("type", lje_proxy_type);
      LJE_SET_FUNC("copy", lje_proxy_copy);
    LJE_END_SECTION("proxy");
  }

  /* LJE API END */

  lua_setfield(L, -2, "lje");
  lua_pop(L, 1); // Pop globals table
}

void lje_removefuncs(lua_State* L) {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_pushnil(L);
  lua_setfield(L, -2, "lje");
  lua_pop(L, 1); // Pop globals table
}