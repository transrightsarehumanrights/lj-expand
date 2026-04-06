#include "lj_expand_crash_handler.h"

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <Windows.h>
#include <DbgHelp.h>
#include <Psapi.h>

#include "lj_debug.h"
#include "lj_expand_dirs.h"
#include "lj_expand_globals.h"
#include "lj_frame.h"
#include "lj_obj.h"

#pragma comment(lib, "dbghelp")
#pragma comment(lib, "psapi")

#define LJE_CRASH_DUMPS_PATH ".lje_crash_dumps"

typedef struct {
    ULONG_PTR base;
    ULONG_PTR end;
} ModuleRange;

static ModuleRange get_module_range(const char* name)
{
    ModuleRange range = {0, 0};
    HMODULE mod = GetModuleHandleA(name);
    if (!mod) return range;

    MODULEINFO info;
    if (GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info)))
    {
        range.base = (ULONG_PTR)info.lpBaseOfDll;
        range.end = range.base + info.SizeOfImage;
    }
    return range;
}

static int addr_in_range(ULONG_PTR addr, ModuleRange range)
{
    return range.base && addr >= range.base && addr < range.end;
}

static int lje_is_relevant_crash(const struct _EXCEPTION_POINTERS* ep)
{
    ModuleRange lje = get_module_range("lje-w64.dll");
    ModuleRange lua = get_module_range("lua_shared.dll");

    ULONG_PTR fault_addr = (ULONG_PTR)ep->ExceptionRecord->ExceptionAddress;
    if (addr_in_range(fault_addr, lje) || addr_in_range(fault_addr, lua))
        return 1;

    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 frame;
    memset(&frame, 0, sizeof(frame));

    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    for (int i = 0; i < 64; i++)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx,
                         NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
            break;

        if (frame.AddrPC.Offset == 0)
            break;

        if (addr_in_range((ULONG_PTR)frame.AddrPC.Offset, lje) ||
            addr_in_range((ULONG_PTR)frame.AddrPC.Offset, lua))
            return 1;
    }

    return 0;
}

static char expand_crash_dump_path(char* out, size_t out_size)
{
    return lje_directory_get(LJE_DIR_CRASH_DUMPS, out, out_size);
}

static char ensure_crash_dump_directory_exists()
{
    char path[MAX_PATH] = { 0 };
    if (!expand_crash_dump_path(path, MAX_PATH))
    {
        return 0;
    }

    DWORD attribs = GetFileAttributesA(path);
    if (attribs == INVALID_FILE_ATTRIBUTES)
    {
        if (!CreateDirectoryA(path, NULL))
        {
            return 0;
        }
    }
    else if (!(attribs & FILE_ATTRIBUTE_DIRECTORY))
    {
        return 0;
    }

    return 1;
}

static void make_dump_filename(char* out, size_t out_size)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(out, out_size, "lje_crash_%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

static char is_valid_code(DWORD exception_code)
{
    return exception_code == EXCEPTION_ACCESS_VIOLATION ||
           exception_code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
           exception_code == EXCEPTION_BREAKPOINT ||
           exception_code == EXCEPTION_DATATYPE_MISALIGNMENT ||
           exception_code == EXCEPTION_FLT_DENORMAL_OPERAND ||
           exception_code == EXCEPTION_FLT_DIVIDE_BY_ZERO ||
           exception_code == EXCEPTION_FLT_INEXACT_RESULT ||
           exception_code == EXCEPTION_FLT_INVALID_OPERATION ||
           exception_code == EXCEPTION_FLT_OVERFLOW ||
           exception_code == EXCEPTION_FLT_STACK_CHECK ||
           exception_code == EXCEPTION_FLT_UNDERFLOW ||
           exception_code == EXCEPTION_ILLEGAL_INSTRUCTION ||
           exception_code == EXCEPTION_IN_PAGE_ERROR ||
           exception_code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
           exception_code == EXCEPTION_INT_OVERFLOW ||
           exception_code == EXCEPTION_INVALID_DISPOSITION ||
           exception_code == EXCEPTION_NONCONTINUABLE_EXCEPTION ||
           exception_code == EXCEPTION_PRIV_INSTRUCTION ||
           exception_code == EXCEPTION_SINGLE_STEP ||
           exception_code == EXCEPTION_STACK_OVERFLOW;
}

static char ptr_valid(void* ptr)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0)
    {
        return 0;
    }

    return (mbi.State & MEM_COMMIT) && !(mbi.Protect & PAGE_NOACCESS);
}

static void lje_write_dump_state(const struct _EXCEPTION_POINTERS* exception_pointers, const char* dump_path)
{
    FILE* f = NULL;
    fopen_s(&f, dump_path, "w");
    if (!f)
    {
        return;
    }

    fprintf(f, "LJE Crash Dump\n================\n\n");
    fprintf(f, "Exception code: 0x%08X\n", exception_pointers->ExceptionRecord->ExceptionCode);
    fprintf(f, "Exception address: 0x%p\n", exception_pointers->ExceptionRecord->ExceptionAddress);
    fprintf(f, "Number of parameters: %d\n", exception_pointers->ExceptionRecord->NumberParameters);
    for (DWORD i = 0; i < exception_pointers->ExceptionRecord->NumberParameters; i++)
    {
        fprintf(f, "  Parameter %d: 0x%p\n", i, exception_pointers->ExceptionRecord->ExceptionInformation[i]);
    }

    fprintf(f, "\nRegisters:\n");
    fprintf(f, "  RAX: 0x%016llX\n", exception_pointers->ContextRecord->Rax);
    fprintf(f, "  RBX: 0x%016llX\n", exception_pointers->ContextRecord->Rbx);
    fprintf(f, "  RCX: 0x%016llX\n", exception_pointers->ContextRecord->Rcx);
    fprintf(f, "  RDX: 0x%016llX\n", exception_pointers->ContextRecord->Rdx);
    fprintf(f, "  R8:  0x%016llX\n", exception_pointers->ContextRecord->R8);
    fprintf(f, "  R9:  0x%016llX\n", exception_pointers->ContextRecord->R9);
    fprintf(f, "  R10: 0x%016llX\n", exception_pointers->ContextRecord->R10);
    fprintf(f, "  R11: 0x%016llX\n", exception_pointers->ContextRecord->R11);
    fprintf(f, "  R12: 0x%016llX\n", exception_pointers->ContextRecord->R12);
    fprintf(f, "  R13: 0x%016llX\n", exception_pointers->ContextRecord->R13);
    fprintf(f, "  R14: 0x%016llX\n", exception_pointers->ContextRecord->R14);
    fprintf(f, "  R15: 0x%016llX\n", exception_pointers->ContextRecord->R15);
    fprintf(f, "  RSP: 0x%016llX\n", exception_pointers->ContextRecord->Rsp);
    fprintf(f, "  RBP: 0x%016llX\n", exception_pointers->ContextRecord->Rbp);
    fprintf(f, "  RIP: 0x%016llX\n", exception_pointers->ContextRecord->Rip);

    fprintf(f, "\nLJE dump\n================\n\n");

    lua_State* L = LJEG()->main_state;
    if (!L)
    {
        fprintf(f, "Lua state not available. LJE was not active at the time of crash.\n");
        fclose(f);
        return;
    }

    fprintf(f, "VM status at time of crash: %d\n", L->status);
    fprintf(f, "Top of stack (up to 10 values):\n");
    int top = lua_gettop(L);
    for (int i = top; i > top - 10 && i > 0; i--)
    {
        int type = lua_type(L, i);
        const char* type_name = lua_typename(L, type);
        fprintf(f, "  [%d] %s: ", i, type_name);
        switch (type)
        {
        case LUA_TSTRING:
            fprintf(f, "\"%s\"", lua_tostring(L, i));
            break;
        case LUA_TNUMBER:
            fprintf(f, "%f", lua_tonumber(L, i));
            break;
        case LUA_TBOOLEAN:
            fprintf(f, "%s", lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TTABLE:
            fprintf(f, "table (pointer: 0x%p)", lua_topointer(L, i));
            break;
        case LUA_TFUNCTION:
            fprintf(f, "function (pointer: 0x%p)", lua_topointer(L, i));
            break;
        default:
            fprintf(f, "%s", type_name);
            break;
        }
        fprintf(f, "\n");
    }

    GCfunc* func = curr_func(L);
    if (!func)
    {
        fprintf(f, "\nNo current function. Stack may be too corrupted to retrieve.\n");
        fclose(f);
        return;
    }

    if (!ptr_valid(func))
    {
        fprintf(f, "\nCurrent function pointer (0x%p) is not valid. Stack may be too corrupted to retrieve.\n", func);
        fclose(f);
        return;
    }

    fprintf(f, "\nStack trace:\n");
    for (int level = 0; ; level++)
    {
        int size = 0;

        LJEG()->show_special_frames = 1;
        cTValue* frame = lj_debug_frame(L, level, &size);
        LJEG()->show_special_frames = 0;

        if (!frame)
        {
            break;
        }

        GCfunc* fn = frame_func(frame);
        if (isluafunc(fn))
        {
            const char* chunkname = proto_chunknamestr(funcproto(fn));
            fprintf(f, "  [%d] Lua function: %s\n", level, chunkname);
            if (isljefunc(fn))
                fprintf(f, "    (This function is from a LJE script)\n");
        } else if (iscfunc(fn))
        {
            fprintf(f, "  [%d] C function: %p\n", level, fn);
            HMODULE mod = NULL;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)fn->c.f, &mod))
            {
                char mod_name[MAX_PATH];
                if (GetModuleFileNameA(mod, mod_name, MAX_PATH))
                {
                    char* filename = strrchr(mod_name, '\\');
                    if (filename)
                    {
                        filename++;
                    } else
                    {
                        filename = mod_name;
                    }

                    fprintf(f, "    (Module: %s)\n", filename);
                }
                else
                {
                    fprintf(f, "    (Module: unknown)\n");
                }
            }
        }
        else
        {
            fprintf(f, "  [%d] Unknown function type: %p\n", level, fn);
        }
    }

    global_State* gl = G(L);
    fprintf(f, "\nHooks:\n");
    fprintf(f, "  Hook function: %p\n", gl->hookf);
    fprintf(f, "  Hook mask: 0x%02X\n", gl->hookmask);
    if (gl->hookmask & HOOK_GC)
    {
        fprintf(f, "    (GC hook active)\n");
    }
    fprintf(f, "  Hook count: %d\n", gl->hookcount);
    fprintf(f, "vmstate: %d\n", gl->vmstate);
    if (gl->jit_base.ptr64)
    {
        fprintf(f, "  JIT base (JIT code is running): %p\n", (void*)gl->jit_base.ptr64);
    }
    fclose(f);
}

static void lje_write_minidump(const struct _EXCEPTION_POINTERS* exception_pointers, const char* dump_path)
{
    HANDLE hFile = CreateFileA(dump_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION exception_info;
    exception_info.ThreadId = GetCurrentThreadId();
    exception_info.ExceptionPointers = (PEXCEPTION_POINTERS)exception_pointers;
    exception_info.ClientPointers = FALSE;

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                      MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo,
                      &exception_info, NULL, NULL);
    CloseHandle(hFile);
}

static LONG WINAPI lje_exception_handler(const struct _EXCEPTION_POINTERS* exception_pointers)
{
    if (!is_valid_code(exception_pointers->ExceptionRecord->ExceptionCode))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (!lje_is_relevant_crash(exception_pointers))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    ensure_crash_dump_directory_exists();
    char minidump_path[MAX_PATH] = { 0 };
    char state_path[MAX_PATH] = { 0 };

    char filename[MAX_PATH] = { 0 };
    make_dump_filename(filename, MAX_PATH);

    expand_crash_dump_path(minidump_path, MAX_PATH);
    expand_crash_dump_path(state_path, MAX_PATH);

    strncat_s(minidump_path, MAX_PATH, "\\", _TRUNCATE);
    strncat_s(state_path, MAX_PATH, "\\", _TRUNCATE);

    strncat_s(minidump_path, MAX_PATH, filename, _TRUNCATE);
    strncat_s(state_path, MAX_PATH, filename, _TRUNCATE);

    strncat_s(minidump_path, MAX_PATH, ".dmp", _TRUNCATE);
    strncat_s(state_path, MAX_PATH, ".txt", _TRUNCATE);

    lje_write_dump_state(exception_pointers, state_path);
    lje_write_minidump(exception_pointers, minidump_path);

    MessageBoxA(
        NULL,
        "LJE has encountered a crash. A crash dump will be generated in %USERPROFILE%/" LJE_CRASH_DUMPS_PATH " to help fix the issue.\n\n"
        "Please consider sharing the crash dump to help improve LJE. It will not automatically be shared.\n\n",
        "LJE Crash Handler",
        MB_OK | MB_ICONERROR
    );

    return EXCEPTION_CONTINUE_SEARCH;
}

void lje_crash_handler_init()
{
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
    SetUnhandledExceptionFilter(lje_exception_handler);
}