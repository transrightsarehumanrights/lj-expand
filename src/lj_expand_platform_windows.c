#include "lj_expand_platform.h"
#if LJE_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <wincrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include "lj_expand_log.h"

#pragma comment(lib, "psapi")
#pragma comment(lib, "dbghelp")
#pragma comment(lib, "shell32")
#pragma comment(lib, "user32")
#pragma comment(lib, "advapi32")

#define LJE_PLAT_TODO(what) LJE_ERROR("lje_plat: %s is not implemented on this platform", what)

/* ===== §init ===== */

static HMODULE self_handle = NULL;
static size_t page_sz = 0;

void lje_plat_init(void* native_self_handle)
{
  if (native_self_handle) {
    self_handle = (HMODULE)native_self_handle;
  } else {
    self_handle = GetModuleHandleA(LJE_SELF_MODULE);
  }

  SYSTEM_INFO si;
  GetSystemInfo(&si);
  page_sz = (size_t)si.dwPageSize;

  SymInitialize(GetCurrentProcess(), NULL, TRUE);
}

/* ===== §modules ===== */

int lje_plat_module_find(const char* name, LJEPlatModule* out)
{
  HMODULE h = GetModuleHandleA(name);
  if (!h) return 0;
  MODULEINFO mi;
  if (!GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi)))
    return 0;
  out->base   = (uintptr_t)mi.lpBaseOfDll;
  out->size   = (size_t)mi.SizeOfImage;
  out->handle = (void*)h;
  return 1;
}

void* lje_plat_module_sym(const LJEPlatModule* m, const char* sym)
{
  return (void*)GetProcAddress((HMODULE)m->handle, sym);
}

void* lje_plat_module_load(const char* path)
{
  return (void*)LoadLibraryA(path);
}

void* lje_plat_module_sym_raw(void* handle, const char* sym)
{
  return (void*)GetProcAddress((HMODULE)handle, sym);
}

void lje_plat_module_unload(void* handle)
{
  FreeLibrary((HMODULE)handle);
}

void lje_plat_module_search_dir(const char* dir)
{
  if (dir)
    SetDllDirectoryA(dir);
  else
    SetDllDirectoryA(NULL);
}

int lje_plat_module_name_from_addr(const void* addr, char* out, size_t n)
{
  HMODULE h;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCSTR)addr, &h))
    return 0;
  char buf[LJE_PATH_MAX];
  DWORD ret = GetModuleFileNameA(h, buf, (DWORD)sizeof(buf));
  if (ret == 0) return 0;
  const char* leaf = strrchr(buf, '\\');
  if (!leaf) leaf = buf; else leaf++;
  lje_strlcpy(out, leaf, n);
  return 1;
}

int lje_plat_self_range(uintptr_t* base, size_t* size)
{
  if (!self_handle) return 0;
  MODULEINFO mi;
  if (!GetModuleInformation(GetCurrentProcess(), self_handle, &mi, sizeof(mi)))
    return 0;
  *base = (uintptr_t)mi.lpBaseOfDll;
  *size = (size_t)mi.SizeOfImage;
  return 1;
}

/* ===== §memory ===== */

size_t lje_plat_page_size(void)
{
  return page_sz;
}

int lje_plat_protect(void* addr, size_t len, int prot)
{
  /* Align addr DOWN to page boundary and round len UP so the full
     [addr, addr+len) span is covered. */
  uintptr_t start = (uintptr_t)addr & ~(page_sz - 1);
  size_t aligned_len = ((uintptr_t)addr + len - start + page_sz - 1) & ~(page_sz - 1);

  DWORD win_prot;
  switch (prot) {
  case LJE_PROT_RX:  win_prot = PAGE_EXECUTE_READ;       break;
  case LJE_PROT_RW:  win_prot = PAGE_READWRITE;           break;
  case LJE_PROT_RWX: win_prot = PAGE_EXECUTE_READWRITE;   break;
  default: return 0;
  }

  DWORD old;
  return VirtualProtect((LPVOID)start, aligned_len, win_prot, &old) ? 1 : 0;
}

void lje_plat_flush_icache(void* addr, size_t len)
{
  FlushInstructionCache(GetCurrentProcess(), addr, len);
}

int lje_plat_addr_readable(const void* addr)
{
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(addr, &mbi, sizeof(mbi)))
    return 0;
  return (mbi.State & MEM_COMMIT) &&
         !(mbi.Protect & PAGE_NOACCESS) &&
         !(mbi.Protect & PAGE_GUARD);
}

/* ===== §fs ===== */

int lje_plat_fs_kind(const char* path)
{
  DWORD attr = GetFileAttributesA(path);
  if (attr == INVALID_FILE_ATTRIBUTES)
    return LJE_FS_MISSING;
  if (attr & FILE_ATTRIBUTE_DIRECTORY)
    return LJE_FS_DIR;
  return LJE_FS_FILE;
}

int lje_plat_mkdir(const char* path)
{
  if (CreateDirectoryA(path, NULL))
    return 1;
  return (GetLastError() == ERROR_ALREADY_EXISTS) ? 1 : 0;
}

int lje_plat_mkdirs(const char* path)
{
  int result = SHCreateDirectoryExA(NULL, path, NULL);
  return (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS) ? 1 : 0;
}

int lje_plat_copy_file(const char* src, const char* dst)
{
  return CopyFileA(src, dst, FALSE) ? 1 : 0;
}

int lje_plat_home_dir(char* out, size_t n)
{
  DWORD ret = ExpandEnvironmentStringsA("%USERPROFILE%", out, (DWORD)n);
  if (ret == 0 || ret > n) return 0;
  /* Strip trailing separator if present */
  size_t len = strlen(out);
  while (len > 0 && (out[len-1] == '\\' || out[len-1] == '/'))
    out[--len] = '\0';
  return 1;
}

int lje_plat_expand_env(const char* in, char* out, size_t n)
{
  DWORD ret = ExpandEnvironmentStringsA(in, out, (DWORD)n);
  return (ret > 0 && ret <= n) ? 1 : 0;
}

int lje_plat_sha256_file(const char* path, char out_hex[65])
{
  HCRYPTPROV prov = 0;
  HCRYPTHASH hash = 0;
  HANDLE file = INVALID_HANDLE_VALUE;
  BYTE buffer[4096];
  DWORD bytes_read;
  DWORD hash_len = 32;
  BYTE hash_out[32];
  int ok = 0;

  file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                     OPEN_EXISTING, 0, NULL);
  if (file == INVALID_HANDLE_VALUE)
    goto cleanup;

  if (!CryptAcquireContextW(&prov, NULL, NULL, PROV_RSA_AES,
                            CRYPT_VERIFYCONTEXT))
    goto cleanup;

  if (!CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash))
    goto cleanup;

  while (ReadFile(file, buffer, sizeof(buffer), &bytes_read, NULL) &&
         bytes_read > 0)
  {
    if (!CryptHashData(hash, buffer, bytes_read, 0))
      goto cleanup;
  }

  if (CryptGetHashParam(hash, HP_HASHVAL, hash_out, &hash_len, 0))
  {
    for (DWORD i = 0; i < hash_len; i++)
      sprintf_s(&out_hex[i * 2], 3, "%02x", hash_out[i]);
    out_hex[hash_len * 2] = '\0';
    ok = 1;
  }

cleanup:
  if (hash) CryptDestroyHash(hash);
  if (prov) CryptReleaseContext(prov, 0);
  if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
  return ok;
}

struct LJEPlatDir {
  HANDLE h;
  WIN32_FIND_DATAA fd;
  int primed;
};

LJEPlatDir* lje_plat_dir_open(const char* dir, const char* pattern)
{
  LJEPlatDir* d = (LJEPlatDir*)malloc(sizeof(LJEPlatDir));
  if (!d) return NULL;

  char buf[LJE_PATH_MAX];
  if (pattern)
    snprintf(buf, sizeof(buf), "%s\\%s", dir, pattern);
  else
    snprintf(buf, sizeof(buf), "%s\\*", dir);

  d->h = FindFirstFileA(buf, &d->fd);
  if (d->h == INVALID_HANDLE_VALUE) {
    free(d);
    return NULL;
  }
  d->primed = 1;
  return d;
}

int lje_plat_dir_next(LJEPlatDir* d, LJEPlatDirEntry* out)
{
  for (;;) {
    if (d->primed) {
      d->primed = 0;
    } else {
      if (!FindNextFileA(d->h, &d->fd))
        return 0;
    }
    /* Skip . and .. */
    if (strcmp(d->fd.cFileName, ".") == 0 ||
        strcmp(d->fd.cFileName, "..") == 0)
      continue;
    lje_strlcpy(out->name, d->fd.cFileName, sizeof(out->name));
    out->is_dir = (d->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
    return 1;
  }
}

void lje_plat_dir_close(LJEPlatDir* d)
{
  if (d) {
    FindClose(d->h);
    free(d);
  }
}

/* ===== §watch ===== */

struct LJEPlatWatch {
  HANDLE dir;
  OVERLAPPED ov;
  char buf[4096];
  FILE_NOTIFY_INFORMATION* cursor;   /* NULL = not draining a buffer */
};

LJEPlatWatch* lje_plat_watch_open(const char* dir)
{
  /* Convert UTF-8 directory path to wide */
  int wide_len = MultiByteToWideChar(CP_UTF8, 0, dir, -1, NULL, 0);
  wchar_t* wide = (wchar_t*)malloc(wide_len * sizeof(wchar_t));
  if (!wide) return NULL;
  MultiByteToWideChar(CP_UTF8, 0, dir, -1, wide, wide_len);

  LJEPlatWatch* w = (LJEPlatWatch*)calloc(1, sizeof(LJEPlatWatch));
  if (!w) { free(wide); return NULL; }

  w->dir = CreateFileW(
    wide,
    FILE_LIST_DIRECTORY,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
    NULL
  );
  free(wide);

  if (w->dir == INVALID_HANDLE_VALUE) {
    free(w);
    return NULL;
  }

  w->ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (!w->ov.hEvent) {
    CloseHandle(w->dir);
    free(w);
    return NULL;
  }

  w->cursor = NULL;
  memset(w->buf, 0, sizeof(w->buf));

  /* Arm the first read */
  ResetEvent(w->ov.hEvent);
  ReadDirectoryChangesW(
    w->dir, w->buf, sizeof(w->buf), TRUE,
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
    NULL, &w->ov, NULL
  );

  return w;
}

int lje_plat_watch_poll(LJEPlatWatch* w, char* name_out, size_t n)
{
  /* Phase 1: Drain entries from the previous buffer (cursor is live).
   *        There is already a re-armed I/O pending, so we do NOT re-arm here. */
  if (w->cursor) {
    FILE_NOTIFY_INFORMATION* fni = w->cursor;

    /* Convert file name from UTF-16 to UTF-8 */
    int wide_len = fni->FileNameLength / sizeof(wchar_t);
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0,
                                        fni->FileName, wide_len,
                                        name_out, (int)n - 1, NULL, NULL);
    if (utf8_len > 0)
      name_out[utf8_len] = '\0';
    else
      name_out[0] = '\0';

    /* Advance cursor */
    if (fni->NextEntryOffset != 0)
      w->cursor = (FILE_NOTIFY_INFORMATION*)((char*)fni + fni->NextEntryOffset);
    else
      w->cursor = NULL;

    return LJE_WATCH_EVENT;
  }

  /* Phase 2: Check the pending async I/O result */
  DWORD bytes = 0;
  if (GetOverlappedResult(w->dir, &w->ov, &bytes, FALSE)) {
    if (bytes > 0) {
      /* New data — point cursor at buffer start and return first entry */
      w->cursor = (FILE_NOTIFY_INFORMATION*)w->buf;

      FILE_NOTIFY_INFORMATION* fni = w->cursor;
      int wide_len = fni->FileNameLength / sizeof(wchar_t);
      int utf8_len = WideCharToMultiByte(CP_UTF8, 0,
                                          fni->FileName, wide_len,
                                          name_out, (int)n - 1, NULL, NULL);
      if (utf8_len > 0)
        name_out[utf8_len] = '\0';
      else
        name_out[0] = '\0';

      /* Advance cursor past this entry */
      if (fni->NextEntryOffset != 0)
        w->cursor = (FILE_NOTIFY_INFORMATION*)((char*)fni + fni->NextEntryOffset);
      else
        w->cursor = NULL;

      /* Re-arm the watch (mandatory — it dies silently otherwise) */
      goto rearm;
    }

    /* Zero bytes: re-arm and return none */
    goto rearm_none;
  }

  { /* GetOverlappedResult failed */
    DWORD err = GetLastError();
    if (err == ERROR_IO_INCOMPLETE)
      return LJE_WATCH_NONE;   /* Still pending, try again next poll */

    /* Re-arm on any error */
    ResetEvent(w->ov.hEvent);
    ReadDirectoryChangesW(
      w->dir, w->buf, sizeof(w->buf), TRUE,
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
      NULL, &w->ov, NULL
    );

    if (err == ERROR_NOTIFY_ENUM_DIR)
      return LJE_WATCH_OVERFLOW;
    return LJE_WATCH_ERROR;
  }

rearm:
  ResetEvent(w->ov.hEvent);
  ReadDirectoryChangesW(
    w->dir, w->buf, sizeof(w->buf), TRUE,
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
    NULL, &w->ov, NULL
  );
  return LJE_WATCH_EVENT;

rearm_none:
  ResetEvent(w->ov.hEvent);
  ReadDirectoryChangesW(
    w->dir, w->buf, sizeof(w->buf), TRUE,
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
    NULL, &w->ov, NULL
  );
  return LJE_WATCH_NONE;
}

void lje_plat_watch_close(LJEPlatWatch* w)
{
  if (w) {
    CancelIo(w->dir);
    CloseHandle(w->dir);
    CloseHandle(w->ov.hEvent);
    free(w);
  }
}

/* ===== §thread ===== */

struct LJEPlatThread {
  HANDLE handle;
  LJEPlatThreadFn fn;
  void* ud;
};

struct LJEPlatMutex {
  CRITICAL_SECTION cs;
};

static DWORD WINAPI thread_trampoline(LPVOID arg)
{
  LJEPlatThread* t = (LJEPlatThread*)arg;
  t->fn(t->ud);
  return 0;
}

LJEPlatThread* lje_plat_thread_start(LJEPlatThreadFn fn, void* ud)
{
  LJEPlatThread* t = (LJEPlatThread*)malloc(sizeof(LJEPlatThread));
  if (!t) return NULL;
  t->fn = fn;
  t->ud = ud;
  t->handle = CreateThread(NULL, 0, thread_trampoline, t, 0, NULL);
  if (!t->handle) {
    free(t);
    return NULL;
  }
  return t;
}

void lje_plat_thread_join(LJEPlatThread* t)
{
  if (t) {
    WaitForSingleObject(t->handle, INFINITE);
    CloseHandle(t->handle);
    free(t);
  }
}

LJEPlatMutex* lje_plat_mutex_create(void)
{
  LJEPlatMutex* m = (LJEPlatMutex*)malloc(sizeof(LJEPlatMutex));
  if (m) InitializeCriticalSection(&m->cs);
  return m;
}

void lje_plat_mutex_lock(LJEPlatMutex* m)
{
  EnterCriticalSection(&m->cs);
}

void lje_plat_mutex_unlock(LJEPlatMutex* m)
{
  LeaveCriticalSection(&m->cs);
}

void lje_plat_mutex_destroy(LJEPlatMutex* m)
{
  if (m) {
    DeleteCriticalSection(&m->cs);
    free(m);
  }
}

void lje_plat_barrier(void)
{
  MemoryBarrier();
}

void lje_plat_sleep_ms(unsigned ms)
{
  Sleep(ms);
}

uint64_t lje_plat_ticks_ms(void)
{
  return (uint64_t)GetTickCount64();
}

void lje_plat_local_time(LJEPlatTime* out)
{
  SYSTEMTIME st;
  GetLocalTime(&st);
  out->year   = st.wYear;
  out->month  = st.wMonth;
  out->day    = st.wDay;
  out->hour   = st.wHour;
  out->minute = st.wMinute;
  out->second = st.wSecond;
}

/* ===== §console ===== */

void lje_plat_console_init(const char* title)
{
  /* AllocConsole is safe to call multiple times — after the first it
   * returns FALSE and we carry on. */
  AllocConsole();

  FILE* d;
  freopen_s(&d, "CONOUT$", "w", stdout);
  freopen_s(&d, "CONOUT$", "w", stderr);

  SetWindowTextA(GetConsoleWindow(), title);

  /* Enable ANSI colour processing on the console handle so printf/VT
   * sequences work without a terminal emulator. */
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode;
  if (GetConsoleMode(h, &mode)) {
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h, mode);
  }
}

const char* lje_plat_command_line(void)
{
  return GetCommandLineA();
}

void lje_plat_message_box(const char* title, const char* text)
{
  MessageBoxA(NULL, text, title, MB_OK | MB_ICONERROR);
}

void lje_plat_debug_break(void)
{
  __debugbreak();
}

/* ===== §crash ===== */

/* Registered crash callback (NULL = not installed). */
static LJEPlatCrashFn crash_cb = NULL;
static void* crash_cb_ud = NULL;

/* Exception-code-to-name mapping and allow-list. Returns NULL for codes
 * that should NOT be handled by LJE's crash handler. */
static const char* exception_code_name(DWORD code)
{
  switch (code) {
  case EXCEPTION_ACCESS_VIOLATION:        return "ACCESS_VIOLATION";
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:   return "ARRAY_BOUNDS_EXCEEDED";
  case EXCEPTION_BREAKPOINT:              return "BREAKPOINT";
  case EXCEPTION_DATATYPE_MISALIGNMENT:   return "DATATYPE_MISALIGNMENT";
  case EXCEPTION_FLT_DENORMAL_OPERAND:    return "FLOAT_DENORMAL_OPERAND";
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:      return "FLOAT_DIVIDE_BY_ZERO";
  case EXCEPTION_FLT_INEXACT_RESULT:      return "FLOAT_INEXACT_RESULT";
  case EXCEPTION_FLT_INVALID_OPERATION:   return "FLOAT_INVALID_OPERATION";
  case EXCEPTION_FLT_OVERFLOW:            return "FLOAT_OVERFLOW";
  case EXCEPTION_FLT_STACK_CHECK:         return "FLOAT_STACK_CHECK";
  case EXCEPTION_FLT_UNDERFLOW:           return "FLOAT_UNDERFLOW";
  case EXCEPTION_ILLEGAL_INSTRUCTION:     return "ILLEGAL_INSTRUCTION";
  case EXCEPTION_IN_PAGE_ERROR:           return "IN_PAGE_ERROR";
  case EXCEPTION_INT_DIVIDE_BY_ZERO:      return "INT_DIVIDE_BY_ZERO";
  case EXCEPTION_INT_OVERFLOW:            return "INT_OVERFLOW";
  case EXCEPTION_INVALID_DISPOSITION:     return "INVALID_DISPOSITION";
  case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
  case EXCEPTION_PRIV_INSTRUCTION:        return "PRIV_INSTRUCTION";
  case EXCEPTION_SINGLE_STEP:             return "SINGLE_STEP";
  case EXCEPTION_STACK_OVERFLOW:          return "STACK_OVERFLOW";
  default:                                return NULL;
  }
}

/* The SEH top-level filter. Called by the OS on an unhandled exception. */
static LONG WINAPI lje_seh_filter(EXCEPTION_POINTERS* ep)
{
  /* 1. Reject unrecognised exception codes immediately. */
  DWORD code = ep->ExceptionRecord->ExceptionCode;
  const char* name = exception_code_name(code);
  if (!name)
    return EXCEPTION_CONTINUE_SEARCH;

  /* 2. Fill LJEPlatCrashInfo. */
  LJEPlatCrashInfo info;
  memset(&info, 0, sizeof(info));

  info.native_code = code;
  info.name = name;
  info.fault_addr = ep->ExceptionRecord->ExceptionAddress;
  info.native = (void*)ep;

  /* Copy registers in the exact LJE_REG_* enum order (defined in the header). */
  {
    CONTEXT* ctx = ep->ContextRecord;
    info.regs[LJE_REG_RAX] = ctx->Rax;
    info.regs[LJE_REG_RBX] = ctx->Rbx;
    info.regs[LJE_REG_RCX] = ctx->Rcx;
    info.regs[LJE_REG_RDX] = ctx->Rdx;
    info.regs[LJE_REG_RSI] = ctx->Rsi;
    info.regs[LJE_REG_RDI] = ctx->Rdi;
    info.regs[LJE_REG_RBP] = ctx->Rbp;
    info.regs[LJE_REG_RSP] = ctx->Rsp;
    info.regs[LJE_REG_R8]  = ctx->R8;
    info.regs[LJE_REG_R9]  = ctx->R9;
    info.regs[LJE_REG_R10] = ctx->R10;
    info.regs[LJE_REG_R11] = ctx->R11;
    info.regs[LJE_REG_R12] = ctx->R12;
    info.regs[LJE_REG_R13] = ctx->R13;
    info.regs[LJE_REG_R14] = ctx->R14;
    info.regs[LJE_REG_R15] = ctx->R15;
    info.regs[LJE_REG_RIP] = ctx->Rip;
  }

  /* 3. Walk stack using StackWalk64 into frames[].
   *    NOTE: StackWalk64 is NOT thread-safe. The caller must ensure
   *    only one thread runs this SEH filter at a time (which is the
   *    natural behaviour of SEH on the faulting thread).
   *    SymInitialize was called once in lje_plat_init. */
  {
    CONTEXT ctx_copy = *ep->ContextRecord;
    STACKFRAME64 frame;
    memset(&frame, 0, sizeof(frame));
    frame.AddrPC.Offset    = ctx_copy.Rip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = ctx_copy.Rbp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = ctx_copy.Rsp;
    frame.AddrStack.Mode   = AddrModeFlat;

    HANDLE process = GetCurrentProcess();
    HANDLE thread  = GetCurrentThread();
    size_t count = 0;

    while (count < LJE_CRASH_MAX_FRAMES &&
           StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread,
                       &frame, &ctx_copy, NULL,
                       SymFunctionTableAccess64, SymGetModuleBase64, NULL))
    {
      if (frame.AddrPC.Offset == 0)
        break;
      info.frames[count++] = (void*)frame.AddrPC.Offset;
    }
    info.frame_count = count;
  }

  /* 4. Call the registered callback. */
  if (crash_cb && crash_cb(&info, crash_cb_ud))
    return EXCEPTION_EXECUTE_HANDLER;
  return EXCEPTION_CONTINUE_SEARCH;
}

int lje_plat_crash_install(LJEPlatCrashFn fn, void* ud)
{
  crash_cb = fn;
  crash_cb_ud = ud;
  SetUnhandledExceptionFilter(lje_seh_filter);
  return 1;
}

int lje_plat_crash_write_native_dump(const LJEPlatCrashInfo* info, const char* path)
{
  /* Reconstruct EXCEPTION_POINTERS from the saved native pointer. */
  EXCEPTION_POINTERS* ep = (EXCEPTION_POINTERS*)info->native;
  if (!ep)
    return 0;

  HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
    return 0;

  MINIDUMP_EXCEPTION_INFORMATION mei;
  memset(&mei, 0, sizeof(mei));
  mei.ThreadId = GetCurrentThreadId();
  mei.ExceptionPointers = ep;
  mei.ClientPointers = FALSE;

  BOOL ok = MiniDumpWriteDump(
    GetCurrentProcess(), GetCurrentProcessId(), hFile,
    MiniDumpNormal | MiniDumpWithDataSegs |
    MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo,
    &mei, NULL, NULL);

  CloseHandle(hFile);
  return ok ? 1 : 0;
}

/* ===== §strings ===== */

size_t lje_strlcpy(char* dst, const char* src, size_t n)
{
  if (n == 0) return strlen(src);
  size_t i;
  for (i = 0; i < n - 1 && src[i]; i++)
    dst[i] = src[i];
  dst[i] = '\0';
  /* Count remaining chars in src past what we copied */
  while (src[i]) i++;
  return i;
}

size_t lje_strlcat(char* dst, const char* src, size_t n)
{
  size_t dlen = 0;
  /* Find dst end, but don't go past n */
  while (dlen < n && dst[dlen])
    dlen++;
  if (dlen == n) return dlen + strlen(src);
  size_t i;
  for (i = 0; dlen + i < n - 1 && src[i]; i++)
    dst[dlen + i] = src[i];
  dst[dlen + i] = '\0';
  return dlen + (i + strlen(src + i));
}

int lje_path_join(char* out, size_t n, const char* a, const char* b)
{
  size_t alen = strlen(a);
  size_t blen = strlen(b);
  int need_sep = (alen > 0 && a[alen - 1] != '/' && a[alen - 1] != '\\');
  size_t total = alen + (need_sep ? 1 : 0) + blen;
  if (total >= n) return 0;
  size_t pos = 0;
  size_t i;
  for (i = 0; i < alen; i++)
    out[pos++] = a[i];
  if (need_sep)
    out[pos++] = LJE_PATH_SEP;
  for (i = 0; i < blen; i++)
    out[pos++] = b[i];
  out[pos] = '\0';
  return 1;
}

#endif /* LJE_PLATFORM_WINDOWS */
