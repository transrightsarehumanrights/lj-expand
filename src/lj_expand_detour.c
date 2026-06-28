#include "lj_expand_detour.h"

#ifdef LJ_TARGET_WINDOWS
#include <windows.h>
#endif

#include <MinHook.h>

enum
{
    PAGE_RW,
    PAGE_RX,
};

static int change_page_permission(void* addr, int permission)
{
#ifdef LJ_TARGET_WINDOWS
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    size_t pageSize = sysInfo.dwPageSize;

    uintptr_t startAddr = (uintptr_t)addr & ~(pageSize - 1);
    DWORD oldProtect;
    DWORD newProtect = permission == PAGE_RW ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    if (VirtualProtect((LPVOID)startAddr, pageSize, newProtect, &oldProtect) == 0) {
        return 0; // Failed to change memory protection
    }
    return 1; // Success
#else
#error "make_page_rw is only implemented for Windows."
#endif
}

// Necessary in some situations
static int flush_instruction_cache(void* addr, size_t size)
{
#ifdef LJ_TARGET_WINDOWS
    if (FlushInstructionCache(GetCurrentProcess(), addr, size) == 0) {
        return 0; // Failed to flush instruction cache
    }
    return 1; // Success
#else
#error "flush_instruction_cache is only implemented for Windows."
#endif
}

static int write_detour_mcode(void* target, void* detour)
{
    if (target == NULL || detour == NULL) {
        return 0; // Invalid parameters
    }

    // Since we do not care about calling conventions, this is cross-platform atleast for 64-bit Windows and Linux
    // as both mark RAX as a caller-saved register.
    uint8_t* p = (uint8_t*)target;
    p[0] = 0x48; // REX.W prefix
    p[1] = 0xB8; // MOV RAX, imm64
    uint64_t detourAddr = (uint64_t)(uintptr_t)detour;
    *(uint64_t*)&p[2] = detourAddr; // imm64
    p[10] = 0xFF; // JMP
    p[11] = 0xE0; // JMP RAX

    return 1; // Success
}

static int detour_func(void* target, void* detour)
{
    if (!change_page_permission(target, PAGE_RW)) {
        return 0; // Failed to change page permission to RW
    }

    if (!write_detour_mcode(target, detour)) {
        return 0; // Failed to write detour machine code
    }

    if (!change_page_permission(target, PAGE_RX)) {
        return 0; // Failed to change page permission back to RX
    }

    if (!flush_instruction_cache(target, 12)) {
        return 0; // Failed to flush instruction cache
    }

    return 1; // Success
}

int lje_detour(void* target, void* detour)
{
    return detour_func(target, detour);
}

static int minhook_initialized = 0;

int lje_detour_trampoline(void* target, void* detour, void** original)
{
    if (!minhook_initialized) {
        if (MH_Initialize() != MH_OK) {
            return 0; // Failed to initialize MinHook
        }
        minhook_initialized = 1;
    }

    if (MH_CreateHook(target, detour, original) != MH_OK) {
        return 0; // Failed to create hook
    }

    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target); // Clean up on failure
        return 0; // Failed to enable hook
    }

    return 1; // Success
}
