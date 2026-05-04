#pragma once
#include <stdint.h>

#include "lj_obj.h"

/*
 * low-overhead proxy system for interacting with complex GCOs in the GMod client state.
 * only tables/userdata are proxied as they typically involve the most allocations or complexity (GC barriers).
 *
 * this makes it so reflecting any engine calls to the isolated state is incredibly low-cost and doesn't involve any
 * deep copying. proxy objects are usually materialized into the isolated state if needed, removing any unnecessary
 * copies.
 *
 * all proxy objects are given as light userdatas to the isolation state to avoid copying and GC overhead,
 * they're also pre-allocated as a memory arena to avoid overhead of any new individual allocations.
 *
 * HOWEVER, proxies do not pin their referent objects to avoid noticeable GC alteration, so all lifetimes
 * must be managed manually. this is fine as we only use it in engine call hooks for now so their
 * lifetime is exclusively scoped to the duration of the engine call.
 *
 * this is why proxies are fire-and-forget allocated, then all can be freed. it makes it possible to expand
 * and fan out more proxies like for fetcihng keys of a proxy'd table or values without worrying about those
 * independent lifetimes. everything goes so long as it is within the lifetime of the engine call.
 */

#define LJE_PROXY_ARENA_INIT_SIZE 2048

typedef struct LJEProxy
{
    uint8_t host_type;
    GCobj* host_obj;
} LJEProxy;

void lje_proxy_arena_init();
LJEProxy* lje_proxy(cTValue* val);
void lje_proxy_release_all();