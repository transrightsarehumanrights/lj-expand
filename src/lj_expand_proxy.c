#include "lj_expand_proxy.h"
#include "lj_expand_log.h"

#include <stdio.h>

static LJEProxy* proxy_arena = NULL;
static size_t proxy_arena_size = 0;
static size_t proxy_arena_used = 0;

static void resize_arena(size_t new_size)
{
    LJEProxy* new_arena = (LJEProxy*)realloc(proxy_arena, sizeof(LJEProxy) * new_size);
    if (!new_arena)
    {
        LJE_ERROR("Failed to resize proxy arena to size: %zu", new_size);
        return;
    }

    // Zero out the new portion of the arena
    if (new_size > proxy_arena_size)
    {
        memset(new_arena + proxy_arena_size, 0, sizeof(LJEProxy) * (new_size - proxy_arena_size));
    }

    proxy_arena = new_arena;
    proxy_arena_size = new_size;

    LJE_DEBUG("Resized proxy arena to size: %zu", new_size);
}

void lje_proxy_arena_init()
{
    proxy_arena_size = LJE_PROXY_ARENA_INIT_SIZE;
    proxy_arena = (LJEProxy*)malloc(sizeof(LJEProxy) * proxy_arena_size);
    if (!proxy_arena)
    {
        LJE_ERROR("Failed to initialize proxy arena.");
        return;
    }

    memset(proxy_arena, 0, sizeof(LJEProxy) * proxy_arena_size);
}

LJEProxy* lje_proxy(cTValue* val)
{
    if (!proxy_arena)
    {
        LJE_ERROR("Proxy arena not initialized!");
        return NULL;
    }

    GCobj* obj = gcV(val);
    if (obj->gch.gct != ~LJ_TTAB && obj->gch.gct != ~LJ_TUDATA)
    {
        LJE_WARN("Attempted to create proxy for unsupported type (gct: %d)", ~obj->gch.gct);
        return NULL;
    }

    // Find a free slot in the arena
    for (size_t i = 0; i < proxy_arena_size; i++)
    {
        if (proxy_arena[i].host_obj == NULL)
        {
            // Initialize the proxy
            proxy_arena[i].host_type = obj->gch.gct;
            proxy_arena[i].host_obj = obj;

            proxy_arena_used++;
            return &proxy_arena[i];
        }
    }

    // No free slot found, need to resize the arena
    resize_arena(proxy_arena_size * 2);
    return lje_proxy(val); // Try again after resizing
}

void lje_proxy_release_all()
{
    if (!proxy_arena)
    {
        return;
    }

    if (proxy_arena_used == 0)
    {
        return;
    }

    memset(proxy_arena, 0, sizeof(LJEProxy) * proxy_arena_used);
    proxy_arena_used = 0;
}