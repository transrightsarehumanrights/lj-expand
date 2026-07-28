#include "lj_expand_script_watcher.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lj_expand_log.h"
#include "lj_expand_platform.h"

/* Lock-free circular queue for reload requests */
#define LJE_RELOAD_QUEUE_SIZE 64  /* Must be power of 2 */

typedef struct LJEReloadQueue
{
    LJEScript* scripts[LJE_RELOAD_QUEUE_SIZE];
    volatile int head;
    volatile int tail;
} LJEReloadQueue;

static void lje_reload_queue_init(LJEReloadQueue* queue)
{
    memset(queue, 0, sizeof(LJEReloadQueue));
}

static int lje_reload_queue_push(LJEReloadQueue* queue, LJEScript* script)
{
    int head = queue->head;
    int next_head = (head + 1) & (LJE_RELOAD_QUEUE_SIZE - 1);

    if (next_head == queue->tail) {
        return 0;  /* queue full */
    }

    queue->scripts[head] = script;
    lje_plat_barrier();
    queue->head = next_head;
    return 1;
}

static LJEScript* lje_reload_queue_pop(LJEReloadQueue* queue)
{
    int tail = queue->tail;

    if (tail == queue->head) {
        return NULL;
    }

    LJEScript* script = queue->scripts[tail];
    lje_plat_barrier();
    queue->tail = (tail + 1) & (LJE_RELOAD_QUEUE_SIZE - 1);
    return script;
}

static size_t lje_reload_queue_length(LJEReloadQueue* queue)
{
    int head = queue->head;
    int tail = queue->tail;
    return (head - tail + LJE_RELOAD_QUEUE_SIZE) & (LJE_RELOAD_QUEUE_SIZE - 1);
}

#define LJE_DEBOUNCE_MS 100

/* Per-directory watch state. The LJEPlatWatch (from the platform layer)
 * owns all HANDLE/OVERLAPPED/buffer/wide-path machinery internally. */
typedef struct LJEScriptWatch
{
    LJEPlatWatch* watch;
    LJEScript* script;
    uint64_t last_change_time;
    int pending_reload;
} LJEScriptWatch;

struct LJEScriptWatcher
{
    LJEScriptWatch* watches;
    size_t watch_count;

    LJEPlatThread* thread;
    LJEPlatMutex* cs;
    volatile int running;

    LJEReloadQueue reload_queue;
};

static void watcher_thread(void* arg);

/* ============================================================================
 * Public API
 * ============================================================================ */

LJEScriptWatcher* lje_watcher_create(void)
{
    LJEScriptWatcher* watcher = calloc(1, sizeof(LJEScriptWatcher));
    if (!watcher) return NULL;

    watcher->cs = lje_plat_mutex_create();
    if (!watcher->cs) {
        free(watcher);
        return NULL;
    }

    watcher->running = 0;
    lje_reload_queue_init(&watcher->reload_queue);

    return watcher;
}

int lje_watcher_add_script(LJEScriptWatcher* watcher, LJEScript* script)
{
    lje_plat_mutex_lock(watcher->cs);

    LJEPlatWatch* pw = lje_plat_watch_open(script->folder);
    if (!pw) {
        lje_plat_mutex_unlock(watcher->cs);
        return -1;
    }

    size_t new_count = watcher->watch_count + 1;
    LJEScriptWatch* watches = realloc(watcher->watches,
                                       new_count * sizeof(LJEScriptWatch));
    if (!watches) {
        lje_plat_watch_close(pw);
        lje_plat_mutex_unlock(watcher->cs);
        return -1;
    }
    watcher->watches = watches;

    LJEScriptWatch* watch = &watcher->watches[watcher->watch_count];
    watch->watch = pw;
    watch->script = script;
    watch->last_change_time = 0;
    watch->pending_reload = 0;

    watcher->watch_count = new_count;

    lje_plat_mutex_unlock(watcher->cs);
    return 0;
}

int lje_watcher_start(LJEScriptWatcher* watcher)
{
    watcher->running = 1;
    lje_plat_barrier();
    watcher->thread = lje_plat_thread_start(watcher_thread, watcher);
    return watcher->thread ? 0 : -1;
}

void lje_watcher_destroy(LJEScriptWatcher* watcher)
{
    if (!watcher) return;

    /* Signal the thread to stop */
    watcher->running = 0;
    lje_plat_barrier();

    /* Join the watcher thread (blocks until it exits) */
    if (watcher->thread) {
        lje_plat_thread_join(watcher->thread);
        watcher->thread = NULL;
    }

    /* Close all per-directory watches */
    lje_plat_mutex_lock(watcher->cs);
    for (size_t i = 0; i < watcher->watch_count; i++) {
        if (watcher->watches[i].watch)
            lje_plat_watch_close(watcher->watches[i].watch);
    }
    free(watcher->watches);
    watcher->watches = NULL;
    watcher->watch_count = 0;
    lje_plat_mutex_unlock(watcher->cs);

    lje_plat_mutex_destroy(watcher->cs);
    watcher->cs = NULL;

    free(watcher);
}

size_t lje_watcher_reload_count(LJEScriptWatcher* watcher)
{
    return lje_reload_queue_length(&watcher->reload_queue);
}

LJEScript* lje_watcher_pop_reload(LJEScriptWatcher* watcher)
{
    return lje_reload_queue_pop(&watcher->reload_queue);
}

/* ============================================================================
 * Internal
 * ============================================================================ */

static int is_lua_file(const char* name)
{
    size_t len = strlen(name);
    if (len < 4) return 0;
    return lje_stricmp(name + len - 4, ".lua") == 0;
}

static void watcher_thread(void* arg)
{
    LJEScriptWatcher* watcher = (LJEScriptWatcher*)arg;

    while (watcher->running) {
        uint64_t now = lje_plat_ticks_ms();

        lje_plat_mutex_lock(watcher->cs);

        for (size_t i = 0; i < watcher->watch_count; i++) {
            LJEScriptWatch* watch = &watcher->watches[i];

            /* Process pending reload with debounce */
            if (watch->pending_reload) {
                if (now - watch->last_change_time >= LJE_DEBOUNCE_MS) {
                    lje_reload_queue_push(&watcher->reload_queue, watch->script);
                    LJE_INFO("Change buffered for script %s (reload queued)",
                             watch->script->name);
                    watch->pending_reload = 0;
                }
            }

            /* Drain all available events from the platform watch.
             * watch_poll returns LJE_WATCH_EVENT for each individual changed
             * file; we filter for .lua only. */
            char name_buf[LJE_NAME_MAX];
            int found_lua = 0;
            int result;
            while ((result = lje_plat_watch_poll(watch->watch,
                                                  name_buf,
                                                  sizeof(name_buf))) == LJE_WATCH_EVENT) {
                if (is_lua_file(name_buf)) {
                    LJE_INFO("Change detected in script %s: %s",
                             watch->script->name, name_buf);
                    found_lua = 1;
                }
            }

            if (result == LJE_WATCH_OVERFLOW) {
                /* Too many changes at once — force reload */
                LJE_INFO("Change buffer overflowed for script %s; forcing reload",
                         watch->script->name);
                watch->last_change_time = now;
                watch->pending_reload = 1;
            } else if (found_lua) {
                watch->last_change_time = now;
                watch->pending_reload = 1;
            }
        }

        lje_plat_mutex_unlock(watcher->cs);

        /* Sleep ~16 ms between polls (matches original Sleep(16) timing) */
        lje_plat_sleep_ms(16);
    }
}
