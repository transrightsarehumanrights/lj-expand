#include "lj_expand_lib.h"

#include "lauxlib.h"
#include "lj_lib.h"
#include "lj_err.h"

int lje_set_ffid(lua_State* L) {
  lua_Integer ffid = luaL_checkinteger(L, 2);
  GCfunc* func = lj_lib_checkfunc(L, 1);
  func->c.ffid = (uint8_t)ffid;

  return 0;
}

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

  LJEfunc* ljeFn = (LJEfunc*)((char*)func + sizeLfunc((MSize)func->l.nupvalues));
  ljeFn->is_special = 1;
  printf("[LJE] Marked function %p as special\n", (void*)func);
  return 0;
}


#define LJE_SET_FUNC(name, func) \
  lua_pushcfunction(L, func); \
  lua_setfield(L, -2, name);

void lje_addfuncs(lua_State* L) {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  LJE_SET_FUNC("set_ffid", lje_set_ffid);
  LJE_SET_FUNC("spoof_debug_info", lje_spoof_debug_info);
  LJE_SET_FUNC("mark_special", lje_mark_special);
  lua_pop(L, 1); // Pop globals table
}