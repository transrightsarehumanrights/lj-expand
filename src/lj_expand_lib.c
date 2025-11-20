#include "lj_expand_lib.h"

#include "lauxlib.h"
#include "lj_lib.h"
#include "lj_err.h"
#include "lj_expand_globals.h"


int lje_spoof_debug_info(lua_State* L)
{
  GCfunc* target = lj_lib_checkfunc(L, 1);
  GCfunc* source = lj_lib_checkfunc(L, 2);
  if (!isluafunc(target))
  {
    lj_err_arg(L, 1, LJ_ERR_NOLFUNC);
  }

  LJEfunc* ljeTarget = funcextend(target);
  setgcrefp(ljeTarget->spoof, source);

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

#define LJE_SET_FUNC(name, func) \
  lua_pushcfunction(L, func); \
  lua_setfield(L, -2, name);

#define LJE_REMOVE_FUNC(name) \
  lua_pushnil(L); \
  lua_setfield(L, -2, name);

void lje_addfuncs(lua_State* L) {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  LJE_SET_FUNC("spoof_debug_info", lje_spoof_debug_info);
  LJE_SET_FUNC("mark_special", lje_mark_special);
  LJE_SET_FUNC("con_print", lje_con_print);
  lua_pop(L, 1); // Pop globals table
}

void lje_removefuncs(lua_State* L) {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  LJE_REMOVE_FUNC("spoof_debug_info");
  LJE_REMOVE_FUNC("mark_special");
  LJE_REMOVE_FUNC("con_print");
  lua_pop(L, 1); // Pop globals table
}