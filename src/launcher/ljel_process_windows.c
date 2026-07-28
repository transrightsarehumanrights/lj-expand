#include "ljel_process.h"
#include <windows.h>
#include <Psapi.h>
#include "ljel_log.h"

/* menusystem.dll loads after lua_shared.dll, so we wait for it to load before injecting. */

static const char* TARGET_DLL = "menusystem.dll";
int is_dll_path_target(const char* dll_path)
{
    // Check from the end, as the path may contain directories
    size_t dll_path_len = strlen(dll_path);
    size_t target_dll_len = strlen(TARGET_DLL);
    if (dll_path_len < target_dll_len)
    {
        return 0;
    }

    return _stricmp(dll_path + dll_path_len - target_dll_len, TARGET_DLL) == 0;
}

void print_error_info()
{
    DWORD error_code = GetLastError();
    LPVOID msg_buffer;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msg_buffer,
        0,
        NULL
    );

    ljel_log_noline("Error %d: %s", error_code, (char*)msg_buffer);
    LocalFree(msg_buffer);
}

/* Very simple RemoteThread injection! */
void inject_into_process(HANDLE hProcess, const char* dll_path)
{
    size_t path_len = strlen(dll_path) + 1;
    LPVOID remote_mem = VirtualAllocEx(hProcess, NULL, path_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_mem == NULL)
    {
        print_error_info();
        ljel_panic("Failed to allocate memory in target process");
    }

    ljel_log("Allocated memory at %p in target process", remote_mem);
    if (!WriteProcessMemory(hProcess, remote_mem, dll_path, path_len, NULL))
    {
        print_error_info();
        ljel_panic("Failed to write DLL path to target process memory");
    }

    ljel_log("Wrote DLL path to target process memory");

    HANDLE hThread = CreateRemoteThread(
        hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"),
        remote_mem,
        0,
        NULL
    );

    if (hThread == NULL)
    {
        print_error_info();
        ljel_panic("Failed to create remote thread in target process");
    }

    ljel_log("Created remote thread in target process, waiting for it to finish...");

    DWORD pid = GetProcessId(hProcess);
    // Force debug to continue so it can run
    DebugActiveProcessStop(pid);

    WaitForSingleObject(hThread, INFINITE);
    ljel_log("Remote thread finished execution");
    VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    ljel_log("DLL injected successfully into process.");
}

void launch_and_inject(const char* gmod_path, const char* lje_path, int argc, char** argv)
{
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);

    DEBUG_EVENT debug_event = { 0 };

    // Build command line from argc/argv
    char* cmdline = NULL;
    size_t cmdline_len = strlen(gmod_path) + 1; // +1 for space or null terminator

    // Calculate total length needed
    for (int i = 0; i < argc; i++)
    {
        cmdline_len += strlen(argv[i]) + 3; // +3 for quotes and space
    }

    cmdline = (char*)malloc(cmdline_len);
    if (!cmdline)
    {
        ljel_panic("Failed to allocate memory for command line");
    }

    // Build the command line string
    strcpy(cmdline, gmod_path);
    for (int i = 0; i < argc; i++)
    {
        strcat(cmdline, " ");
        // Add quotes if argument contains spaces
        if (strchr(argv[i], ' '))
        {
            strcat(cmdline, "\"");
            strcat(cmdline, argv[i]);
            strcat(cmdline, "\"");
        }
        else
        {
            strcat(cmdline, argv[i]);
        }
    }

    if (!CreateProcessA(
            gmod_path,
            cmdline,
            NULL,
            NULL,
            FALSE,
            DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED,
            NULL,
            NULL,
            &si,
            &pi))
    {
        free(cmdline);
        print_error_info();
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            ljel_panic("GMod was not found. Are you sure you are on the x86-64 branch?");
        } else
        {
            ljel_panic("Failed to create process");
        }
    }

    free(cmdline);
    ljel_log("Process created, PID: %d", pi.dwProcessId);
    ResumeThread(pi.hThread);

    ljel_log("Waiting for the time to inject DLL...");
    while (1)
    {
        if (!WaitForDebugEvent(&debug_event, INFINITE))
        {
            print_error_info();
            ljel_panic("Failed to wait for debug event");
        }

        if (debug_event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        {
            char dll_path[MAX_PATH] = { 0 };
            if (!GetMappedFileNameA(
                    pi.hProcess,
                    debug_event.u.LoadDll.lpBaseOfDll,
                    dll_path,
                    MAX_PATH))
            {
                print_error_info();
                ljel_panic("Failed to get mapped file name");
            }

            if (is_dll_path_target(dll_path))
            {
                ljel_log("Target DLL loaded: %s", dll_path);
                inject_into_process(pi.hProcess, lje_path);
                ljel_log("LJE is now loaded...");
                break;
            }
        }

        /* CRUCIAL: LuaJIT uses exceptions for control flow, so we must not interfere with them. */
        ContinueDebugEvent(debug_event.dwProcessId, debug_event.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);

        if (debug_event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
        {
            // This is unexpected. We terminate the loop at injection, it should not reach here.
            // If it *does*, it probably means the user accidentally tried injecting into GMod's own
            // launcher which immediately exits.
            ljel_log("******* UNEXPECTED PROCESS EXIT *******");
            ljel_log("Target process exited before injection could occur.");
            ljel_log("This usually happens when trying to inject into GMod's own launcher.");
            ljel_log("Make sure to inject into bin/win64/gmod.exe instead.");
            break;
        }
    }

    return;
}

int ljel_full_path(const char* relative, char* out, size_t n)
{
  DWORD ret = GetFullPathNameA(relative, (DWORD)n, out, NULL);
  return (ret > 0 && ret < (DWORD)n) ? 1 : 0;
}

void ljel_alert(const char* title, const char* text)
{
  MessageBoxA(NULL, text, title, MB_OK | MB_ICONERROR);
}