#include "lj_expand_isolation.h"

#include "lauxlib.h"
#include "lj_expand_globals.h"
#include "lj_expand_lib.h"
#include "lualib.h"

#include <windows.h>

#include "lj_func.h"
#include "lj_gc.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_udata.h"

typedef lua_State* (*luaL_newstate_t)(void);
typedef void       (*luaL_openlibs_t)(lua_State*);

lua_State* lje_create_isolated_state() {
    HMODULE gmod_lua = GetModuleHandleA("lua_shared.dll");
    if (!gmod_lua)
    {
        printf("[LJE] Failed to locate GMod's lua_shared.dll for isolated state creation.\n");
        return NULL;
    }

    luaL_newstate_t gmod_newstate = (luaL_newstate_t)GetProcAddress(gmod_lua, "luaL_newstate");
    luaL_openlibs_t gmod_openlibs = (luaL_openlibs_t)GetProcAddress(gmod_lua, "luaL_openlibs");
    if (!gmod_newstate || !gmod_openlibs)
    {
        printf("[LJE] Failed to resolve GMod luaL_newstate/luaL_openlibs exports.\n");
        return NULL;
    }

    // We deliberately use GMod's luaL_newstate (and luaL_openlibs) instead of our own
    // bundled LuaJIT's, because the engine's loader/parser/lexer all operate against
    // GMod's lua_State and global_State layouts. A state allocated by our LuaJIT would
    // be the wrong shape and the keyword table indices wouldn't match the engine's
    // TK_* enum, causing nonsense parser errors (e.g. "near 'in'" on every script).
    lua_State* L = gmod_newstate();
    if (!L)
    {
        printf("[LJE] Failed to create isolated Lua state.\n");
        return NULL;
    }

    // An isolated state basically just means we create an entirely separated Lua universe
    // for LJE only. Then we pull in our API functions only.

    LJEG()->isolated_state = L;
    gmod_openlibs(L); // Open standard libraries (using GMod's openlibs, since this is a GMod-shaped state)
    lje_addfuncs(L);  // Add LJE-specific functions to the isolated state

    printf("[LJE] Created isolated Lua state: %p\n", (void*)L);
    return L;
}

static TValue *stkindex2adr(lua_State *L, int idx)
{
    if (idx > 0) {
        TValue *o = L->base + (idx - 1);
        return o < L->top ? o : niltv(L);
    } else {
        return L->top + idx;
    }
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
    printf("[LJE] Securely calling function at %p\n", func_ptr);
    LJEG()->redirect_to_isolation = 1;
    int results = func(LJEG()->main_state); /* make it think it's running in the main state, it won't know any better! */
    LJEG()->redirect_to_isolation = 0;
    printf("[LJE] Function call complete, got %d results\n", results);
    return results;
}

int lje_push_safe_cfunction(lua_State* L, lua_CFunction func)
{
    // We push a wrapper C function that securely redirects to the main state, and we store the real function pointer in an upvalue.
    lua_pushlightuserdata(L, (void*)func);
    lua_pushcclosure(L, lje_secure_gmod_api, 1);
    return 1;
}

static int copy_to_isolated_state(lua_State* from, lua_State* to, cTValue* val, TValue* dst);

static int copy_to_isolated_state(lua_State* from, lua_State* to, cTValue* val, TValue* dst);

/* Recover hbits from hmask. hmask is always (1<<hbits)-1 or 0 for the empty sentinel. */
static uint32_t hmask_to_hbits(uint32_t hmask)
{
    if (hmask == 0) return 0;
    uint32_t hbits = 0;
    uint32_t n = hmask + 1;
    while (n > 1) { n >>= 1; hbits++; }
    return hbits;
}

static GCtab* deep_copy_table(lua_State* from, lua_State* to, GCtab* from_table)
{
    Node* from_nodes = noderef(from_table->node);
    int has_hash_part = (from_nodes != &G(from)->nilnode);
    uint32_t hbits = has_hash_part ? hmask_to_hbits(from_table->hmask) : 0;

    GCtab* new = lj_tab_new(to, from_table->asize, hbits);

    /* Sanity: lj_tab_new should have produced the same shape we asked for. */
    lua_assert(new->asize == from_table->asize);
    lua_assert(!has_hash_part || new->hmask == from_table->hmask);

    if (from_table->asize > 0)
    {
        TValue* from_array = tvref(from_table->array);
        TValue* to_array = tvref(new->array);
        for (uint32_t i = 0; i < from_table->asize; i++)
        {
            copy_to_isolated_state(from, to, &from_array[i], &to_array[i]);
        }
    }

    if (has_hash_part)
    {
        Node* to_nodes = noderef(new->node);
        for (uint32_t i = 0; i <= from_table->hmask; i++)
        {
            /* Skip empty slots: nil key means the slot is unused. Copying
               them is harmless but wasteful, and keeps `next` clean. */
            if (tvisnil(&from_nodes[i].key))
            {
                setnilV(&to_nodes[i].key);
                setnilV(&to_nodes[i].val);
                setmref(to_nodes[i].next, NULL);
                continue;
            }

            copy_to_isolated_state(from, to, &from_nodes[i].key, &to_nodes[i].key);
            copy_to_isolated_state(from, to, &from_nodes[i].val, &to_nodes[i].val);

            /* Translate the next pointer into the destination's node array,
               preserving the hash collision chain. */
            Node* src_next = nextnode(&from_nodes[i]);
            if (src_next)
            {
                ptrdiff_t idx = src_next - from_nodes;
                lua_assert(idx >= 0 && (uint32_t)idx <= from_table->hmask);
                setmref(to_nodes[i].next, &to_nodes[idx]);
            }
            else
            {
                setmref(to_nodes[i].next, NULL);
            }
        }
    }

    /* Copy the metatable. Assumes copy_to_isolated_state handles cycles via
       a seen-map; otherwise a self-referential metatable will recurse forever. */
    if (gcref(from_table->metatable))
    {
        GCtab* mt_src = tabref(from_table->metatable);
        GCtab* mt_dst = deep_copy_table(from, to, mt_src);
        setgcref(new->metatable, obj2gco(mt_dst));
        lj_gc_objbarriert(to, new, mt_dst);
    }

    /* Preserve the nomm cache so metamethod lookups on the copy behave
       the same as on the original. */
    new->nomm = from_table->nomm;

    return new;
}

static int copy_to_isolated_state(lua_State* from, lua_State* to, cTValue* val, TValue* dst)
{
    // LJE: Fast routine to copy a value from one state to the other. Functions are obviously not supported.
    // The main issue with other tools trying to do this is the inane amount of function calls to the
    // Lua C API that it requires since they're exports and can't be inlined. This is faster cause
    // we can just use the internal functions.

    if (tvisnumber(val))
    {
        // It has no direct type, so we skip the switch statement
        setnumV(dst, numV(val));
        return 1;
    }

    int unsupported = 0;
    switch (itype(val))
    {
        case LJ_TNIL:
            setnilV(dst);
            break;
        case LJ_TFALSE:
            setboolV(dst, 0);
            break;
        case LJ_TTRUE:
            setboolV(dst, 1);
            break;
        case LJ_TLIGHTUD:
            setlightudV(dst, lightudV(val));
            break;
        case LJ_TSTR:
            /* string interning is *not* consistent between universes, must copy */
            GCstr* copy = lj_str_new(to, strdata(strV(val)), strV(val)->len);
            setstrV(to, dst, copy);
            break;
        case LJ_TTAB:
            settabV(to, dst, deep_copy_table(from, to, tabV(val)));
            break;
        case LJ_TUDATA:
            GCudata* from_ud = udataV(val);
            GCudata* to_ud = lj_udata_new(to, from_ud->len, tabref(to->env));
            printf("[LJE] Copying userdata of size %d from main state to isolated state.\n", from_ud->len);
            memcpy(uddata(to_ud), uddata(from_ud), from_ud->len);
            setudataV(to, dst, to_ud);
            break;
        case LJ_TFUNC:
            // Copying over C functions is fine. We just need to wrap it.
            GCfunc* from_fn = funcV(val);
            if (iscfunc(from_fn))
            {
                GCfunc* new = lj_func_newC(to, 1, tabref(to->env));
                new->c.f = lje_secure_gmod_api;
                setlightudV(&new->c.upvalue[0], from_fn->c.f);
                setfuncV(to, dst, new);
                break;
            }
            printf("[LJE] Warning: Attempting to copy Lua/fast function from main state to isolated state, which is not supported. Copying as nil instead.\n");
        default:
            unsupported = 1;
            break;
    }

    if (unsupported)
    {
        printf("[LJE] Warning: Unsupported type %d in copy_to_isolated_state, copying as nil.\n", itype(val));
        setnilV(dst);
        return 1;
    }

    return 1;
}

int lje_copy_to_isolated_state(lua_State* from, lua_State* to, int idx)
{
    cTValue* val = stkindex2adr(from, idx);
    if (tvisnil(val))
    {
        printf("[LJE] Warning: Attempting to copy nil value from main state to isolated state at index %d.\n", idx);
        return 0;
    }

    return copy_to_isolated_state(from, to, val, to->top++);
}

int lje_copy_to_isolated_state_tv(lua_State* from, lua_State* to, cTValue* val)
{
    if (tvisnil(val))
    {
        printf("[LJE] Warning: Attempting to copy nil value from main state to isolated state.\n");
        return 0;
    }

    return copy_to_isolated_state(from, to, val, to->top++);
}