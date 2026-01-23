#include "lj_expand_lib.h"

#include "lauxlib.h"
#include "lj_debug.h"
#include "lj_dispatch.h"
#include "lj_lib.h"
#include "lj_err.h"
#include "lj_expand_frame.h"
#include "lj_expand_globals.h"
#include "lj_expand_startup.h"
#include "lj_frame.h"
#include "lj_gc.h"
#include "lj_tab.h"
#include "lj_trace.h"
#include "lj_vm.h"

int lje_spoof_debug_info(lua_State* L)
{
  GCfunc* spoof = lj_lib_checkfunc(L, 1);
  GCfunc* target = lj_lib_checkfunc(L, 2);
  if (!isluafunc(spoof))
  {
    lj_err_arg(L, 1, LJ_ERR_NOLFUNC);
  }

  LJEfunc* ljeTarget = funcextend(spoof);
  setgcrefp(ljeTarget->spoof, target);

  /* Remove any pre-existing spoof records, can cause memory corruption */
  lje_remove_spoof_record_by_spoof(spoof);
  lje_remove_spoof_record_by_target(target);

  lje_insert_spoof_record(spoof, target);

  return 0;
}

int lje_is_function_spoofed(lua_State* L)
{
  GCfunc* func = lj_lib_checkfunc(L, 1);
  GCfunc* spoof = lje_find_spoof_by_target(func);

  if (!spoof)
  {
    lua_pushboolean(L, 0);
    return 1;
  }

  lua_pushboolean(L, 1);
  lua_pushvalue(L, 1); // Original function
  return 2;
}

int lje_mark_special(lua_State* L)
{
  GCfunc* func = lj_lib_checkfunc(L, 1);
  if (!isluafunc(func))
  {
    lj_err_arg(L, 1, LJ_ERR_NOLFUNC);
  }

  funcextend(func)->is_special = 1;
  return 0;
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

  funcextend(callback)->is_special = 1;
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

int lje_patch_bytecodes(lua_State* L)
{
  // Patch bytecodes for this state
  GG_State* gg = L2GG(L);
  lj_trace_flushall(L);

  /* ISEQV/ISNEV are used for fast equality comparisons. This
   * will break for spoofs, so we patch them to use
   * our spoof-aware versions.
   */
  lje_patch_bytecode(gg, BC_ISEQV);
  lje_patch_bytecode(gg, BC_ISNEV);
  /* TGETS is used for metatable lookups. We need to
   * patch this to our own version that blocks metatable
   * lookups when LJE is involved and/or remaps are set.
   */

  lje_patch_bytecode(gg, BC_TGETS);

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
  if (lje_frame_is_lje_involved(L, frame_offset))
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

  funcextend(callback)->is_special = 1;
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
  if (!isluafunc(callback))
  {
    lj_err_arg(L, 1, LJ_ERR_NOLFUNC);
  }

  funcextend(callback)->is_special = 1;
  LJEG()->engine_call_hook_ref_id = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

int lje_compile_string(lua_State* L)
{
  const char* script = luaL_checkstring(L, 1);
  if (!script)
  {
    lj_err_arg(L, 1, LJ_ERR_NOVAL);
  }

  if (!lje_startup_compile(L, script))
    lua_pushnil(L);

  return 1; // Return compiled function
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

  /* func: anything to do with functions, e.g: spoofing, stealth */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("spoof", lje_spoof_debug_info);
    LJE_SET_FUNC("is_spoofed", lje_is_function_spoofed);
    LJE_SET_FUNC("mark_special", lje_mark_special);
    LJE_SET_FUNC("compile", lje_compile_string);
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
  LJE_END_SECTION("env");

  /* util: unassorted utility functions */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("get_bytecode_hash", lje_get_bytecode_hash);
    LJE_SET_FUNC("get_call_stack", lje_get_call_stack);
    LJE_SET_FUNC("get_registry", lje_get_registry);
    LJE_SET_FUNC("set_push_string_callback", lje_set_push_string_callback);
    LJE_SET_FUNC("set_script_hook_callback", lje_set_script_hook_callback);
    LJE_SET_FUNC("set_engine_call_hook", lje_set_engine_call_hook);
  LJE_END_SECTION("util");

  /* gc: garbage collector manipulation */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("get_total", lje_get_gc_total);
    LJE_SET_FUNC("set_total", lje_set_gc_total);
    LJE_SET_FUNC("run_full_gc", lje_run_full_gc);
  LJE_END_SECTION("gc");

  /* vm: virtual machine manipulation */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("patch_bytecodes", lje_patch_bytecodes);
  LJE_END_SECTION("vm");

  /* data: simple data storage API */
  LJE_NEW_SECTION()
    LJE_SET_FUNC("write", lje_data_write);
    LJE_SET_FUNC("read", lje_data_read);
  LJE_END_SECTION("data");

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