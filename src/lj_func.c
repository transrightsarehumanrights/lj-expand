/*
** Function handling (prototypes, functions and upvalues).
** Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
**
** Portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_func_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_func.h"
#include "lj_frame.h"
#include "lj_debug.h"
#include "lj_expand_frame.h"

#include "lj_expand_globals.h"
#include "lj_expand_startup.h"
#include "lj_tab.h"
#include "lj_str.h"
#include "stdio.h"

/* -- Prototypes ---------------------------------------------------------- */

void LJ_FASTCALL lj_func_freeproto(global_State *g, GCproto *pt)
{
  /* LJE: We already accounted for the size of LJEproto when allocating the proto */
  lj_mem_free(g, pt, pt->sizept - sizeof(LJEproto));
}

/* -- Upvalues ------------------------------------------------------------ */

static void unlinkuv(GCupval *uv)
{
  lua_assert(uvprev(uvnext(uv)) == uv && uvnext(uvprev(uv)) == uv);
  setgcrefr(uvnext(uv)->prev, uv->prev);
  setgcrefr(uvprev(uv)->next, uv->next);
}

/* Find existing open upvalue for a stack slot or create a new one. */
static GCupval *func_finduv(lua_State *L, TValue *slot)
{
  global_State *g = G(L);
  GCRef *pp = &L->openupval;
  GCupval *p;
  GCupval *uv;
  /* Search the sorted list of open upvalues. */
  while (gcref(*pp) != NULL && uvval((p = gco2uv(gcref(*pp)))) >= slot) {
    lua_assert(!p->closed && uvval(p) != &p->tv);
    if (uvval(p) == slot) {  /* Found open upvalue pointing to same slot? */
      if (isdead(g, obj2gco(p)))  /* Resurrect it, if it's dead. */
	flipwhite(obj2gco(p));
      return p;
    }
    pp = &p->nextgc;
  }
  /* No matching upvalue found. Create a new one. */
  uv = lj_mem_newt(L, sizeof(GCupval), GCupval);
  newwhite(g, uv);
  uv->gct = ~LJ_TUPVAL;
  uv->closed = 0;  /* Still open. */
  setmref(uv->v, slot);  /* Pointing to the stack slot. */
  /* NOBARRIER: The GCupval is new (marked white) and open. */
  setgcrefr(uv->nextgc, *pp);  /* Insert into sorted list of open upvalues. */
  setgcref(*pp, obj2gco(uv));
  setgcref(uv->prev, obj2gco(&g->uvhead));  /* Insert into GC list, too. */
  setgcrefr(uv->next, g->uvhead.next);
  setgcref(uvnext(uv)->prev, obj2gco(uv));
  setgcref(g->uvhead.next, obj2gco(uv));
  lua_assert(uvprev(uvnext(uv)) == uv && uvnext(uvprev(uv)) == uv);
  return uv;
}

/* Create an empty and closed upvalue. */
static GCupval *func_emptyuv(lua_State *L)
{
  GCupval *uv = (GCupval *)lj_mem_newgco(L, sizeof(GCupval));
  uv->gct = ~LJ_TUPVAL;
  uv->closed = 1;
  setnilV(&uv->tv);
  setmref(uv->v, &uv->tv);
  return uv;
}

/* Close all open upvalues pointing to some stack level or above. */
void LJ_FASTCALL lj_func_closeuv(lua_State *L, TValue *level)
{
  GCupval *uv;
  global_State *g = G(L);
  while (gcref(L->openupval) != NULL &&
	 uvval((uv = gco2uv(gcref(L->openupval)))) >= level) {
    GCobj *o = obj2gco(uv);
    lua_assert(!isblack(o) && !uv->closed && uvval(uv) != &uv->tv);
    setgcrefr(L->openupval, uv->nextgc);  /* No longer in open list. */
    if (isdead(g, o)) {
      lj_func_freeuv(g, uv);
    } else {
      unlinkuv(uv);
      lj_gc_closeuv(g, uv);
    }
  }
}

void LJ_FASTCALL lj_func_freeuv(global_State *g, GCupval *uv)
{
  if (!uv->closed)
    unlinkuv(uv);
  lj_mem_freet(g, uv);
}

/* -- Functions (closures) ------------------------------------------------ */

GCfunc *lj_func_newC(lua_State *L, MSize nelems, GCtab *env)
{
  GCfunc *fn = (GCfunc *)lj_mem_newgco(L, sizeCfunc(nelems));
  fn->c.gct = ~LJ_TFUNC;
  fn->c.ffid = FF_C;
  fn->c.nupvalues = (uint8_t)nelems;
  /* NOBARRIER: The GCfunc is new (marked white). */
  setmref(fn->c.pc, &G(L)->bc_cfunc_ext);
  setgcref(fn->c.env, obj2gco(env));
  return fn;
}

static GCfunc *func_newL(lua_State *L, GCproto *pt, GCtab *env)
{
  uint32_t count;
  GCfunc *fn = (GCfunc *)lj_mem_newgco(L, sizeLfunc((MSize)pt->sizeuv) + sizeof(LJEfunc));
  /* LJE: Reduce reported memory usage for functions to avoid detection */
  G(L)->gc.total -= sizeof(LJEfunc);

  fn->l.gct = ~LJ_TFUNC;
  fn->l.ffid = FF_LUA;
  fn->l.nupvalues = 0;  /* Set to zero until upvalues are initialized. */
  /* NOBARRIER: Really a setgcref. But the GCfunc is new (marked white). */
  setmref(fn->l.pc, proto_bc(pt));
  setgcref(fn->l.env, obj2gco(env));
  /* Saturating 3 bit counter (0..7) for created closures. */
  count = (uint32_t)pt->flags + PROTO_CLCOUNT;
  pt->flags = (uint8_t)(count - ((count >> PROTO_CLC_BITS) & PROTO_CLCOUNT));

  LJEfunc* ljeFn = (LJEfunc*)((char*)fn + sizeLfunc((MSize)pt->sizeuv));
  memset(ljeFn, 0, sizeof(LJEfunc));

  return fn;
}

int check_proto_chunkname(GCproto* pt, const char* name)
{
  if (strcmp(proto_chunknamestr(pt), name) == 0)
  {
    return 1;
  }

  return 0;
}

int is_init_lua(const char* chunkname)
{
  // We check backwards since it may come from any path, like an addon.
  size_t len = strlen(chunkname);
  size_t target_len = strlen("lua/includes/init.lua");

  return len >= target_len && strcmp(chunkname + len - target_len, "lua/includes/init.lua") == 0;
}

/* Create a new Lua function with empty upvalues. */
GCfunc *lj_func_newL_empty(lua_State *L, GCproto *pt, GCtab *env)
{
  if (is_init_lua(proto_chunknamestr(pt)) && !lje_frame_is_lua_involved(L, 0))
  {
    GCtab* globalEnv = tabref(L->env);
    cTValue* clientBool = lj_tab_getstr(globalEnv, lj_str_newlit(L, "CLIENT"));
    if (clientBool && tvistrue(clientBool))
    {
      // Keep track of original GC total. CLIENT is seven bytes
      LJEG()->original_gc = G(L)->gc.total - (7 + sizeof(GCstr));

      printf("[LJE] Pre-initializing Lua...\n");
      LJEG()->waiting_for_init_call = 1;
    }
  }

  if (check_proto_chunkname(pt, "@Startup") && !lje_frame_is_lua_involved(L, 0))
  {
    /* LJE: Create script watcher since now we're going to be running. */
    LJEG()->script_watcher = lje_watcher_create();
    for (int i = 0; i < LJEG()->loaded_script_count; i++)
    {
      LJEScript* script = LJEG()->script_load_order[i];
      lje_watcher_add_script(LJEG()->script_watcher, script);
    }

    lje_watcher_start(LJEG()->script_watcher);
    printf("[LJE] Created script watcher for startup scripts.\n");

    printf("[LJE] Starting up Lua...\n");
    lje_clear_spoof_records();
    lje_save_random_state();
    LJEG()->using_error_reporter = 1;
    for (int i = 0; i < LJEG()->loaded_script_count; i++)
    {
      LJEScript* script = LJEG()->script_load_order[i];
      lje_startup_execute(L, script, NULL);
    }
    LJEG()->using_error_reporter = 0;
    lje_restore_random_state();
  }

  GCfunc *fn = func_newL(L, pt, env);
  MSize i, nuv = pt->sizeuv;
  /* NOBARRIER: The GCfunc is new (marked white). */
  for (i = 0; i < nuv; i++) {
    GCupval *uv = func_emptyuv(L);
    int32_t v = proto_uv(pt)[i];
    uv->immutable = ((v / PROTO_UV_IMMUTABLE) & 1);
    uv->dhash = (uint32_t)(uintptr_t)pt ^ (v << 24);
    setgcref(fn->l.uvptr[i], obj2gco(uv));
  }
  fn->l.nupvalues = (uint8_t)nuv;
  return fn;
}

/* Do a GC check and create a new Lua function with inherited upvalues. */
GCfunc *lj_func_newL_gc(lua_State *L, GCproto *pt, GCfuncL *parent)
{
  GCfunc *fn;
  GCRef *puv;
  MSize i, nuv;
  TValue *base;
  lj_gc_check_fixtop(L);
  fn = func_newL(L, pt, tabref(parent->env));
  /* NOBARRIER: The GCfunc is new (marked white). */
  puv = parent->uvptr;
  nuv = pt->sizeuv;
  base = L->base;
  for (i = 0; i < nuv; i++) {
    uint32_t v = proto_uv(pt)[i];
    GCupval *uv;
    if ((v & PROTO_UV_LOCAL)) {
      uv = func_finduv(L, base + (v & 0xff));
      uv->immutable = ((v / PROTO_UV_IMMUTABLE) & 1);
      uv->dhash = (uint32_t)(uintptr_t)mref(parent->pc, char) ^ (v << 24);
    } else {
      uv = &gcref(puv[v])->uv;
    }
    setgcref(fn->l.uvptr[i], obj2gco(uv));
  }
  fn->l.nupvalues = (uint8_t)nuv;
  return fn;
}

void LJ_FASTCALL lj_func_free(global_State *g, GCfunc *fn)
{
  MSize size = isluafunc(fn) ? sizeLfunc((MSize)fn->l.nupvalues) :
			       sizeCfunc((MSize)fn->c.nupvalues);
  /* LJE: Remove spoof record. */
  if (isluafunc(fn) && funcspoof(fn))
  {
    lje_remove_spoof_record_by_spoof(fn);
  }

  lj_mem_free(g, fn, size);
}

