#include <stdio.h>

#include "ljel_log.h"
#include "ljel_process.h"
#ifdef _WIN64
#include <windows.h>
#endif
#define GMOD_PATH "E:\\SteamLibrary\\steamapps\\common\\GarrysMod\\bin\\win64\\gmod.exe"
#define LJE_DLL "gmcl_lj-expand_win64.dll"

char* resolve_lje_path()
{
    // We need to get an absolute path to the lje DLL from *our cwd
#ifdef _WIN64
    char fullLjePath[MAX_PATH];
    if (GetFullPathNameA(LJE_DLL, MAX_PATH, fullLjePath, NULL) == 0)
    {
        return NULL;
    }

    return _strdup(fullLjePath);
#else
#error "Unsupported platform"
#endif

}
int main()
{
    char* dll_path = resolve_lje_path();
    if (dll_path == NULL)
    {
        ljel_panic("Failed to resolve LJE DLL path");
    }

    ljel_log("Launching GMod from path: %s", GMOD_PATH);
    launch_and_inject(GMOD_PATH, dll_path);
    return 0;
}