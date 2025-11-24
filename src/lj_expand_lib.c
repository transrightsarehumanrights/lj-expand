#include "lj_expand_lib.h"

#include "lauxlib.h"
#include "lj_lib.h"
#include "lj_err.h"
#include "lj_expand_globals.h"


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

  printf("[LJE] Spoofing %p -> %p\n", (void*)spoof, (void*)target);
  /* Remove any pre-existing spoof records, can cause memory corruption */
  lje_remove_spoof_record_by_spoof(spoof);
  lje_remove_spoof_record_by_target(target);

  lje_insert_spoof_record(spoof, target);

  return 0;
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

#define LJE_SET_FUNC(name, func) \
  lua_pushcfunction(L, func); \
  lua_setfield(L, -2, name);

#define LJE_REMOVE_FUNC(name) \
  lua_pushnil(L); \
  lua_setfield(L, -2, name);

void lje_addfuncs(lua_State* L) {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_createtable(L, 0, 0);
  LJE_SET_FUNC("spoof_debug_info", lje_spoof_debug_info);
  LJE_SET_FUNC("mark_special", lje_mark_special);
  LJE_SET_FUNC("con_print", lje_con_print);
  LJE_SET_FUNC("set_push_string_callback", lje_set_push_string_callback);
  LJE_SET_FUNC("enable_hooks", lje_enable_hooks);
  LJE_SET_FUNC("disable_hooks", lje_disable_hooks);
  LJE_SET_FUNC("ignore_fn_on_hook", lje_ignore_fn_on_hook);
  lua_setfield(L, -2, "lje");
  lua_pop(L, 1); // Pop globals table
}

void lje_removefuncs(lua_State* L) {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_pushnil(L);
  lua_setfield(L, -2, "lje");
  lua_pop(L, 1); // Pop globals table
}