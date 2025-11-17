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

  if (!isluafunc(source))
  {
    lj_err_arg(L, 2, LJ_ERR_NOLFUNC);
  }

  GCproto* target_proto = funcproto(target);
  GCproto* source_proto = funcproto(source);

  target_proto->chunkname = source_proto->chunkname;
  target_proto->firstline = source_proto->firstline;
  target_proto->numline = source_proto->numline;
  target_proto->lineinfo = source_proto->lineinfo;
  target_proto->uvinfo = source_proto->uvinfo;
  target_proto->varinfo = source_proto->varinfo;

  return 0;
}

#define LJE_SET_FUNC(name, func) \
  lua_pushcfunction(L, func); \
  lua_setfield(L, -2, name);

void lje_addfuncs(lua_State* L) {
  // Placeholder implementation
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  LJE_SET_FUNC("set_ffid", lje_set_ffid);
  LJE_SET_FUNC("spoof_debug_info", lje_spoof_debug_info);
  lua_pop(L, 1); // Pop globals table
}