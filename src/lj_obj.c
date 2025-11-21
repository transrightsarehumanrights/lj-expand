/*
** Miscellaneous object handling.
** Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_obj_c
#define LUA_CORE

#include "lj_obj.h"

/* Object type names. */
LJ_DATADEF const char *const lj_obj_typename[] = {  /* ORDER LUA_T */
  "no value", "nil", "boolean", "userdata", "number", "string",
  "table", "function", "userdata", "thread", "proto", "cdata"
};

LJ_DATADEF const char *const lj_obj_itypename[] = {  /* ORDER LJ_T */
  "nil", "boolean", "boolean", "userdata", "string", "upval", "thread",
  "proto", "function", "trace", "cdata", "table", "userdata", "number"
};

/* Compare two objects without calling metamethods. */
int LJ_FASTCALL lj_obj_equal(cTValue *o1, cTValue *o2)
{
  if (itype(o1) == itype(o2)) {
    if (tvispri(o1))
      return 1;
    if (!tvisnum(o1))
    {
      return gcrefeq(o1->gcr, o2->gcr);
    }
  } else if (!tvisnumber(o1) || !tvisnumber(o2)) {
    return 0;
  }
  return numberVnum(o1) == numberVnum(o2);
}

/* Return pointer to object or its object data. */
const void * LJ_FASTCALL lj_obj_ptr(cTValue *o)
{
  void* result = NULL;
  if (tvisudata(o))
    result = uddata(udataV(o));
  else if (tvislightud(o))
    result = lightudV(o);
  else if (LJ_HASFFI && tviscdata(o))
    result = cdataptr(cdataV(o));
  else if (tvisgcv(o))
    /* LJE: Check if a spoofed lua function. If so, return the spoofed target pointer */
      if (tvisfunc(o))
      {
        GCfunc* fn = funcV(o);
        if (isluafunc(fn))
        {
          LJEfunc* ljeFn = funcextend(fn);
          GCfunc* target = gcrefp(ljeFn->spoof, GCfunc);
          if (target != NULL)
          {
            result = (void*)target;
          }
          else
          {
            result = gcV(o);
          }
        } else
        {
          // a C function, just return normally
          result = gcV(o);
        }
      } else
      {
        result = gcV(o);
      }
  else
    return NULL;

  /* LJE: Stupid change by GMod. We need to OR the pointer with a mask to avoid noticeable differences */
  void* maskedResult = (void*)((uintptr_t)result | GMOD_PTR_MASK);
  return maskedResult;
}

