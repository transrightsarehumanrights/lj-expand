/*
** Load and dump code.
** Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
*/

#include <errno.h>
#include <stdio.h>

#define lj_load_c
#define LUA_CORE

#include "lua.h"
#include "lauxlib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_expand_frame.h"
#include "lj_expand_log.h"
#include "lj_frame.h"
#include "lj_vm.h"
#include "lj_lex.h"
#include "lj_bcdump.h"
#include "lj_parse.h"
#include "lj_expand_globals.h"
#include "lj_expand_module.h"
#include "lj_expand_path.h"

/* -- Load Lua source code and bytecode ----------------------------------- */

static TValue *cpparser(lua_State *L, lua_CFunction dummy, void *ud)
{
  LexState *ls = (LexState *)ud;
  GCproto *pt;
  GCfunc *fn;
  int bc;
  UNUSED(dummy);
  cframe_errfunc(L->cframe) = -1;  /* Inherit error function. */
  bc = lj_lex_setup(L, ls);
  if (ls->mode && !strchr(ls->mode, bc ? 'b' : 't')) {
    setstrV(L, L->top++, lj_err_str(L, LJ_ERR_XMODE));
    lj_err_throw(L, LUA_ERRSYNTAX);
  }
  pt = bc ? lj_bcread(ls) : lj_parse(ls);
  fn = lj_func_newL_empty(L, pt, tabref(L->env));
  /* Don't combine above/below into one statement. */
  setfuncV(L, L->top++, fn);
  return NULL;
}

LUA_API int lua_loadx(lua_State *L, lua_Reader reader, void *data,
		      const char *chunkname, const char *mode)
{
  LexState ls;
  int status;
  ls.rfunc = reader;
  ls.rdata = data;
  ls.chunkarg = chunkname ? chunkname : "?";
  ls.mode = mode;
  lj_buf_init(L, &ls.sb);
  status = lj_vm_cpcall(L, NULL, &ls, cpparser);
  lj_lex_cleanup(L, &ls);
  lj_gc_check(L);
  return status;
}

LUA_API int lua_load(lua_State *L, lua_Reader reader, void *data,
		     const char *chunkname)
{
  return lua_loadx(L, reader, data, chunkname, NULL);
}

typedef struct FileReaderCtx {
  FILE *fp;
  char buf[LUAL_BUFFERSIZE];
} FileReaderCtx;

static const char *reader_file(lua_State *L, void *ud, size_t *size)
{
  FileReaderCtx *ctx = (FileReaderCtx *)ud;
  UNUSED(L);
  if (feof(ctx->fp)) return NULL;
  *size = fread(ctx->buf, 1, sizeof(ctx->buf), ctx->fp);
  return *size > 0 ? ctx->buf : NULL;
}

LUALIB_API int luaL_loadfilex(lua_State *L, const char *filename,
			      const char *mode)
{
  FileReaderCtx ctx;
  int status;
  const char *chunkname;
  if (filename) {
    ctx.fp = fopen(filename, "rb");
    if (ctx.fp == NULL) {
      lua_pushfstring(L, "cannot open %s: %s", filename, strerror(errno));
      return LUA_ERRFILE;
    }
    chunkname = lua_pushfstring(L, "@%s", filename);
  } else {
    ctx.fp = stdin;
    chunkname = "=stdin";
  }
  status = lua_loadx(L, reader_file, &ctx, chunkname, mode);
  if (ferror(ctx.fp)) {
    L->top -= filename ? 2 : 1;
    lua_pushfstring(L, "cannot read %s: %s", chunkname+1, strerror(errno));
    if (filename)
      fclose(ctx.fp);
    return LUA_ERRFILE;
  }
  if (filename) {
    L->top--;
    copyTV(L, L->top-1, L->top);
    fclose(ctx.fp);
  }
  return status;
}

LUALIB_API int luaL_loadfile(lua_State *L, const char *filename)
{
  return luaL_loadfilex(L, filename, NULL);
}

typedef struct StringReaderCtx {
  const char *str;
  size_t size;
} StringReaderCtx;

static const char *reader_string(lua_State *L, void *ud, size_t *size)
{
  StringReaderCtx *ctx = (StringReaderCtx *)ud;
  UNUSED(L);
  if (ctx->size == 0) return NULL;
  *size = ctx->size;
  ctx->size = 0;
  return ctx->str;
}

/* LJE: Awkward workaround to ensure we use GMod's modified bytecode loader */
/* We use lua_loadx, since luaL_loadbufferx does not actually do much, so we can override it completely. */
typedef int (*lua_loadx_t)(lua_State *L, lua_Reader reader, void *data,
        const char *chunkname, const char *mode);

static lua_loadx_t original_luaL_loadx = NULL;
static lua_loadx_t get_original_lua_loadx() {
    if (original_luaL_loadx == NULL) {
        lje_Module* mod = lje_module_find(LJE_LUA_MODULE);
        if (mod) {
            original_luaL_loadx = (lua_loadx_t)lje_module_get_func(mod, "lua_loadx");
        }
    }

    return original_luaL_loadx;
}

static int is_init_lua(const char* chunkname)
{
  // We check backwards since it may come from any path, like an addon.
  size_t len = strlen(chunkname);
  size_t target_len = strlen("lua/includes/init.lua");

  return len >= target_len && strcmp(chunkname + len - target_len, "lua/includes/init.lua") == 0;
}

/* LJE: Watch the engine's boot chunks go by so we know which phase we're in.
 * This used to hang off lj_func_newL_empty, which needed a byte signature; the
 * chunkname is identical here and luaL_loadbufferx is an export, so it doesn't.
 * The frame check keeps Lua-initiated loads (loadstring et al) from spoofing it. */
static void lje_check_boot_chunk(lua_State* L, const char* name)
{
  if (!name || lje_frame_is_lua_involved(L, 0))
    return;

  if (is_init_lua(name))
  {
    GCtab* globalEnv = tabref(L->env);
    cTValue* clientBool = lj_tab_getstr_lit(globalEnv, "CLIENT"); // Doesn't create any GC size
    if (clientBool && tvistrue(clientBool))
    {
      printf("[LJE] Pre-initializing Lua...\n");
      LJEG()->waiting_for_init_call = 1;
    }
  }
  else if (strcmp(name, "@Startup") == 0)
  {
    LJEG()->waiting_for_startup_call = 1;
  }
  else if (strcmp(name, "@lua/includes/init_menu.lua") == 0)
  {
    LJE_WARN("Detected menu state at %p", L);
    LJEG()->menu_state = L;
    LJEG()->waiting_for_menu_call = 1;

    /* The menu state comes up long before the client one, so bind lje.state.menu
       now rather than waiting for the client init call that may never arrive. */
    if (LJEG()->isolated_state)
      lje_path_install_state_globals(LJEG()->isolated_state);
  }
}

LUALIB_API int luaL_loadbufferx(lua_State *L, const char *buf, size_t size,
				const char *name, const char *mode)
{
  StringReaderCtx ctx;
  ctx.str = buf;
  ctx.size = size;
  lua_loadx_t original_loadx = get_original_lua_loadx();
  if (original_loadx == NULL) {
    // Fallback to default implementation if we couldn't find GMod's lua_loadx
    printf("[LJE WARNING] Could not find original lua_loadx, falling back to default implementation.\n");
    return lua_loadx(L, reader_string, &ctx, name, mode);
  }

  if (LJEG()->main_state == L && LJEG()->script_hook_ref_id != LUA_NOREF)
  {
    /* LJE: Prepare to run script hook */
    lua_State* I = LJEG()->isolated_state;
    lua_rawgeti(I, LUA_REGISTRYINDEX, LJEG()->script_hook_ref_id);
    lua_pushstring(I, name ? name : "unknown");
    lua_pushstring(I, buf);
    if (lua_pcall(I, 2, 1, 0) != LUA_OK)
    {
      const char* err = lua_tostring(I, -1);
      printf("[LJE ERROR] Error in script hook callback: %s\n", err);
      lua_pop(I, 1); // Pop the error
    }

    // Ensure the returned value is a string
    if (!lua_isstring(I, -1))
    {
      printf("[LJE ERROR] Script hook did not return a string, aborting loadbufferx.\n");
      lua_pop(I, 1); // Pop the invalid return value
    } else
    {
      // Replace the buffer with the modified one
      size_t new_size = 0;
      const char* new_buf = lua_tolstring(I, -1, &new_size);
      ctx.str = new_buf;
      ctx.size = new_size;
      lua_pop(I, 1); // Pop the returned string
    }
  }

  int status = original_loadx(L, reader_string, &ctx, name, mode);
  if (status == LUA_OK)
    lje_check_boot_chunk(L, name);

  return status;
}

LUALIB_API int luaL_loadbuffer(lua_State *L, const char *buf, size_t size,
			       const char *name)
{
  return luaL_loadbufferx(L, buf, size, name, NULL);
}

LUALIB_API int luaL_loadstring(lua_State *L, const char *s)
{
  return luaL_loadbuffer(L, s, strlen(s), s);
}

/* -- Dump bytecode ------------------------------------------------------- */

LUA_API int lua_dump(lua_State *L, lua_Writer writer, void *data)
{
  cTValue *o = L->top-1;
  api_check(L, L->top > L->base);
  if (tvisfunc(o) && isluafunc(funcV(o)))
    return lj_bcwrite(L, funcproto(funcV(o)), writer, data, 0);
  else
    return 1;
}

