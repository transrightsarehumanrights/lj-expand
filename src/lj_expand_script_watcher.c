#include "lj_expand_script_watcher.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Lock-free circular queue for reload requests */
#define LJE_RELOAD_QUEUE_SIZE 64  // Must be power of 2

typedef struct LJEReloadQueue
{
    LJEScript* scripts[LJE_RELOAD_QUEUE_SIZE];
    volatile LONG head;
    volatile LONG tail;
} LJEReloadQueue;

static void lje_reload_queue_init(LJEReloadQueue* queue)
{
    memset(queue, 0, sizeof(LJEReloadQueue));
}

static BOOL lje_reload_queue_push(LJEReloadQueue* queue, LJEScript* script)
{
    LONG head = queue->head;
    LONG next_head = (head + 1) & (LJE_RELOAD_QUEUE_SIZE - 1);

    if (next_head == queue->tail) {
        return FALSE;
    }

    queue->scripts[head] = script;
    MemoryBarrier();
    queue->head = next_head;
    return TRUE;
}

static LJEScript* lje_reload_queue_pop(LJEReloadQueue* queue)
{
    LONG tail = queue->tail;

    if (tail == queue->head) {
        return NULL;
    }

    LJEScript* script = queue->scripts[tail];
    MemoryBarrier();
    queue->tail = (tail + 1) & (LJE_RELOAD_QUEUE_SIZE - 1);
    return script;
}

static size_t lje_reload_queue_length(LJEReloadQueue* queue)
{
    LONG head = queue->head;
    LONG tail = queue->tail;
    return (head - tail + LJE_RELOAD_QUEUE_SIZE) & (LJE_RELOAD_QUEUE_SIZE - 1);
}

#define LJE_DEBOUNCE_MS 100

typedef struct LJEScriptWatch
{
    HANDLE directory_handle;
    OVERLAPPED overlapped;
    char buffer[4096];
    LJEScript* script;
    wchar_t* folder_wide;
    ULONGLONG last_change_time;
    BOOL pending_reload;
} LJEScriptWatch;

struct LJEScriptWatcher
{
    LJEScriptWatch* watches;
    size_t watch_count;

    HANDLE thread;
    HANDLE stop_event;
    CRITICAL_SECTION cs;
    volatile BOOL running;

    LJEReloadQueue reload_queue;
};

static DWORD WINAPI watcher_thread(LPVOID arg);
static void start_watch(LJEScriptWatch* watch);

static wchar_t* utf8_to_wide(const char* str)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t* wide = malloc(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wide, len);
    return wide;
}

// ============================================================================
// Public API
// ============================================================================

LJEScriptWatcher* lje_watcher_create(void)
{
    LJEScriptWatcher* watcher = calloc(1, sizeof(LJEScriptWatcher));
    if (!watcher) return NULL;

    InitializeCriticalSection(&watcher->cs);
    watcher->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    watcher->running = FALSE;
    lje_reload_queue_init(&watcher->reload_queue);

    return watcher;
}

int lje_watcher_add_script(LJEScriptWatcher* watcher, LJEScript* script)
{
    EnterCriticalSection(&watcher->cs);

    wchar_t* folder_wide = utf8_to_wide(script->folder);

    HANDLE dir_handle = CreateFileW(
        folder_wide,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (dir_handle == INVALID_HANDLE_VALUE) {
        free(folder_wide);
        LeaveCriticalSection(&watcher->cs);
        return -1;
    }

    size_t new_count = watcher->watch_count + 1;
    watcher->watches = realloc(watcher->watches, new_count * sizeof(LJEScriptWatch));

    LJEScriptWatch* watch = &watcher->watches[watcher->watch_count];
    memset(watch, 0, sizeof(LJEScriptWatch));
    watch->directory_handle = dir_handle;
    watch->script = script;
    watch->folder_wide = folder_wide;
    watch->overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    watch->last_change_time = 0;
    watch->pending_reload = FALSE;

    watcher->watch_count = new_count;

    if (watcher->running) {
        start_watch(watch);
    }

    LeaveCriticalSection(&watcher->cs);
    return 0;
}

int lje_watcher_start(LJEScriptWatcher* watcher)
{
    EnterCriticalSection(&watcher->cs);

    for (size_t i = 0; i < watcher->watch_count; i++) {
        start_watch(&watcher->watches[i]);
    }

    watcher->running = TRUE;
    LeaveCriticalSection(&watcher->cs);

    watcher->thread = CreateThread(NULL, 0, watcher_thread, watcher, 0, NULL);
    return watcher->thread ? 0 : -1;
}

void lje_watcher_destroy(LJEScriptWatcher* watcher)
{
    if (!watcher) return;

    watcher->running = FALSE;
    SetEvent(watcher->stop_event);

    if (watcher->thread) {
        WaitForSingleObject(watcher->thread, INFINITE);
        CloseHandle(watcher->thread);
    }

    EnterCriticalSection(&watcher->cs);
    for (size_t i = 0; i < watcher->watch_count; i++) {
        CancelIo(watcher->watches[i].directory_handle);
        CloseHandle(watcher->watches[i].directory_handle);
        CloseHandle(watcher->watches[i].overlapped.hEvent);
        free(watcher->watches[i].folder_wide);
    }
    free(watcher->watches);
    LeaveCriticalSection(&watcher->cs);

    CloseHandle(watcher->stop_event);
    DeleteCriticalSection(&watcher->cs);
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

// ============================================================================
// Internal
// ============================================================================

static void start_watch(LJEScriptWatch* watch)
{
    ResetEvent(watch->overlapped.hEvent);
    ReadDirectoryChangesW(
        watch->directory_handle,
        watch->buffer,
        sizeof(watch->buffer),
        TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
        NULL,
        &watch->overlapped,
        NULL
    );
}

static BOOL is_lua_file(FILE_NOTIFY_INFORMATION* fni)
{
    size_t name_len = fni->FileNameLength / sizeof(wchar_t);
    if (name_len < 4) return FALSE;

    wchar_t* ext = fni->FileName + name_len - 4;
    return _wcsicmp(ext, L".lua") == 0;
}

static DWORD WINAPI watcher_thread(LPVOID arg)
{
    LJEScriptWatcher* watcher = (LJEScriptWatcher*)arg;

    while (watcher->running) {
        if (WaitForSingleObject(watcher->stop_event, 0) == WAIT_OBJECT_0) {
            break;
        }

        ULONGLONG now = GetTickCount64();

        EnterCriticalSection(&watcher->cs);

        for (size_t i = 0; i < watcher->watch_count; i++) {
            LJEScriptWatch* watch = &watcher->watches[i];

            if (watch->pending_reload) {
                if (now - watch->last_change_time >= LJE_DEBOUNCE_MS) {
                    lje_reload_queue_push(&watcher->reload_queue, watch->script);
                    watch->pending_reload = FALSE;
                }
            }

            DWORD bytes_transferred;
            if (GetOverlappedResult(watch->directory_handle, &watch->overlapped,
                                    &bytes_transferred, FALSE)) {
                if (bytes_transferred > 0) {
                    FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)watch->buffer;

                    BOOL found_lua = FALSE;
                    do {
                        if (is_lua_file(fni)) {
                            found_lua = TRUE;
                            break;
                        }
                        if (fni->NextEntryOffset == 0) break;
                        fni = (FILE_NOTIFY_INFORMATION*)((char*)fni + fni->NextEntryOffset);
                    } while (1);

                    if (found_lua) {
                        watch->last_change_time = now;
                        watch->pending_reload = TRUE;
                    }
                }

                start_watch(watch);
            }
        }

        LeaveCriticalSection(&watcher->cs);

        Sleep(16);
    }

    return 0;
}