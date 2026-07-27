#ifndef _LJ_EXPAND_PLATFORM_H
#define _LJ_EXPAND_PLATFORM_H

/* LJE: The single seam between LJE and the operating system. Nothing outside
 * lj_expand_platform_windows.c (and a future _posix.c) may include <windows.h>.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* -- platform selection & names ------------------------------------------- */

#if defined(_WIN32)
#define LJE_PLATFORM_WINDOWS 1
#else
#define LJE_PLATFORM_WINDOWS 0
#endif

#define LJE_PATH_MAX 1024
#define LJE_NAME_MAX 256

#if LJE_PLATFORM_WINDOWS
#define LJE_PATH_SEP      '\\'
#define LJE_PATH_SEP_STR  "\\"
#define LJE_MODULE_EXT    ".dll"
#define LJE_LUA_MODULE    "lua_shared.dll"
#define LJE_SELF_MODULE   "lje-w64.dll"
#define LJE_BINMODULE_GLOB "lje-*" LJE_MODULE_EXT
#define LJE_SYM_ADV_ERROR_REPORTER "?AdvancedLuaErrorReporter@@YAHPEAUlua_State@@@Z"
#else
#define LJE_PATH_SEP      '/'
#define LJE_PATH_SEP_STR  "/"
#define LJE_MODULE_EXT    ".so"
#define LJE_LUA_MODULE    "lua_shared.so"
#define LJE_SELF_MODULE   "lje.so"
#define LJE_BINMODULE_GLOB "lje-*" LJE_MODULE_EXT
#define LJE_SYM_ADV_ERROR_REPORTER "_Z24AdvancedLuaErrorReporterP9lua_State" /* TODO: verify */
#endif

/* Called once at bootstrap. native_self_handle is the platform's own handle for
 * this module (HMODULE from DllMain on Windows); may be NULL. */
void lje_plat_init(void* native_self_handle);

/* -- modules & symbols ----------------------------------------------------- */

typedef struct { uintptr_t base; size_t size; void* handle; } LJEPlatModule;

/* Find an already-loaded module by file name. 1 on success. */
int   lje_plat_module_find(const char* name, LJEPlatModule* out);
void* lje_plat_module_sym(const LJEPlatModule* m, const char* sym);

/* Load/unload a module from disk (binary modules). */
void* lje_plat_module_load(const char* path);
void* lje_plat_module_sym_raw(void* handle, const char* sym);
void  lje_plat_module_unload(void* handle);
/* Extra directory searched when resolving a loaded module's own dependencies.
 * NULL resets to default. */
void  lje_plat_module_search_dir(const char* dir);

/* Owning module of a code address (file name only, not full path). 1 on success. */
int   lje_plat_module_name_from_addr(const void* addr, char* out, size_t n);
/* Address range of LJE itself. 1 on success. */
int   lje_plat_self_range(uintptr_t* base, size_t* size);

/* -- memory & code patching ------------------------------------------------ */

enum { LJE_PROT_RX = 0, LJE_PROT_RW, LJE_PROT_RWX };

size_t lje_plat_page_size(void);
/* Page-aligns addr/len internally. 1 on success. */
int    lje_plat_protect(void* addr, size_t len, int prot);
void   lje_plat_flush_icache(void* addr, size_t len);
/* Cheap "is this committed and readable" probe for crash-time introspection. */
int    lje_plat_addr_readable(const void* addr);

/* Trampolining hook (MinHook on Windows). 1 on success. */
int    lje_plat_hook_trampoline(void* target, void* detour, void** out_orig);

/* -- filesystem ------------------------------------------------------------ */

enum { LJE_FS_MISSING = 0, LJE_FS_FILE, LJE_FS_DIR };

int  lje_plat_fs_kind(const char* path);
int  lje_plat_mkdir(const char* path);         /* single level; 1 if created OR exists */
int  lje_plat_mkdirs(const char* path);        /* recursive;    1 if created OR exists */
int  lje_plat_copy_file(const char* src, const char* dst);
/* User home dir, no trailing separator. 1 on success. */
int  lje_plat_home_dir(char* out, size_t n);
/* Expands %VAR% / $VAR forms in `in`. 1 on success. */
int  lje_plat_expand_env(const char* in, char* out, size_t n);
/* Lowercase hex SHA-256 of a file, NUL-terminated (64 chars + NUL). 1 on success. */
int  lje_plat_sha256_file(const char* path, char out_hex[65]);

typedef struct LJEPlatDir LJEPlatDir;
typedef struct { char name[LJE_NAME_MAX]; int is_dir; } LJEPlatDirEntry;

/* pattern may be NULL (all entries) or a glob ("*", "lje-*.dll").
 * "." and ".." are never returned. */
LJEPlatDir* lje_plat_dir_open(const char* dir, const char* pattern);
int         lje_plat_dir_next(LJEPlatDir* d, LJEPlatDirEntry* out); /* 1 = entry, 0 = end */
void        lje_plat_dir_close(LJEPlatDir* d);

/* -- directory watching ---------------------------------------------------- */

enum {
  LJE_WATCH_ERROR = -1,
  LJE_WATCH_NONE = 0,   /* nothing pending */
  LJE_WATCH_EVENT,      /* name_out filled with a changed file name */
  LJE_WATCH_OVERFLOW    /* events were dropped; caller should assume everything changed */
};

typedef struct LJEPlatWatch LJEPlatWatch;

LJEPlatWatch* lje_plat_watch_open(const char* dir);  /* recursive, non-blocking */
int           lje_plat_watch_poll(LJEPlatWatch* w, char* name_out, size_t n);
void          lje_plat_watch_close(LJEPlatWatch* w);

/* -- threads, sync, time --------------------------------------------------- */

typedef struct LJEPlatThread LJEPlatThread;
typedef struct LJEPlatMutex  LJEPlatMutex;
typedef void (*LJEPlatThreadFn)(void* ud);

LJEPlatThread* lje_plat_thread_start(LJEPlatThreadFn fn, void* ud);
void           lje_plat_thread_join(LJEPlatThread* t);   /* also frees the handle */

LJEPlatMutex*  lje_plat_mutex_create(void);
void           lje_plat_mutex_lock(LJEPlatMutex* m);
void           lje_plat_mutex_unlock(LJEPlatMutex* m);
void           lje_plat_mutex_destroy(LJEPlatMutex* m);

void           lje_plat_barrier(void);          /* full memory fence */
void           lje_plat_sleep_ms(unsigned ms);
uint64_t       lje_plat_ticks_ms(void);         /* monotonic */

typedef struct { int year, month, day, hour, minute, second; } LJEPlatTime;
void           lje_plat_local_time(LJEPlatTime* out);

/* -- console & process ----------------------------------------------------- */

/* Allocates/attaches a console and enables ANSI colour. No-op where stdout is
 * already a terminal. */
void        lje_plat_console_init(const char* title);
/* Whole command line as one string; owned by the platform layer. */
const char* lje_plat_command_line(void);
void        lje_plat_message_box(const char* title, const char* text);
void        lje_plat_debug_break(void);

/* -- crash handling -------------------------------------------------------- */

enum {
  LJE_REG_RAX = 0, LJE_REG_RBX, LJE_REG_RCX, LJE_REG_RDX,
  LJE_REG_RSI, LJE_REG_RDI, LJE_REG_RBP, LJE_REG_RSP,
  LJE_REG_R8,  LJE_REG_R9,  LJE_REG_R10, LJE_REG_R11,
  LJE_REG_R12, LJE_REG_R13, LJE_REG_R14, LJE_REG_R15,
  LJE_REG_RIP, LJE_REG_COUNT
};

#define LJE_CRASH_MAX_FRAMES 64

typedef struct {
  uint32_t native_code;                    /* SEH exception code / signal number */
  const char* name;                        /* e.g. "ACCESS_VIOLATION", "SIGSEGV" */
  void*    fault_addr;
  uint64_t regs[LJE_REG_COUNT];
  size_t   frame_count;
  void*    frames[LJE_CRASH_MAX_FRAMES];   /* return addresses, innermost first */
  void*    native;                         /* EXCEPTION_POINTERS* / ucontext_t* */
} LJEPlatCrashInfo;

/* Return 1 to mark the crash handled, 0 to let the OS continue searching. */
typedef int (*LJEPlatCrashFn)(const LJEPlatCrashInfo* info, void* ud);

int lje_plat_crash_install(LJEPlatCrashFn fn, void* ud);
/* Writes a platform-native dump (minidump on Windows). 0 if unsupported. */
int lje_plat_crash_write_native_dump(const LJEPlatCrashInfo* info, const char* path);

/* -- strings & paths (CRT portability, not OS calls) ----------------------- */

/* Always NUL-terminates. Returns the length it wanted to write (truncation
 * detectable via >= n), matching BSD strlcpy/strlcat semantics. */
size_t lje_strlcpy(char* dst, const char* src, size_t n);
size_t lje_strlcat(char* dst, const char* src, size_t n);
/* Joins with exactly one LJE_PATH_SEP between parts. 1 on success. */
int    lje_path_join(char* out, size_t n, const char* a, const char* b);

#if LJE_PLATFORM_WINDOWS
#define lje_strdup  _strdup
#define lje_stricmp _stricmp
#else
#define lje_strdup  strdup
#define lje_stricmp strcasecmp
#endif

#endif
