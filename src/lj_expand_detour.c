#include "lj_expand_detour.h"
#include "lj_expand_platform.h"

#define DETOUR_SIZE 12

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
    if (!lje_plat_protect(target, DETOUR_SIZE, LJE_PROT_RW))
        return 0; // Failed to change page permission to RW

    if (!write_detour_mcode(target, detour))
        return 0; // Failed to write detour machine code

    if (!lje_plat_protect(target, DETOUR_SIZE, LJE_PROT_RX))
        return 0; // Failed to change page permission back to RX

    lje_plat_flush_icache(target, DETOUR_SIZE);

    return 1; // Success
}

int lje_detour(void* target, void* detour)
{
    return detour_func(target, detour);
}

int lje_detour_trampoline(void* target, void* detour, void** original)
{
    return lje_plat_hook_trampoline(target, detour, original);
}
