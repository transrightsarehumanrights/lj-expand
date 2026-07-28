#include "lj_expand_detour.h"
#include "lj_expand_platform.h"

static void build_jump(uint8_t out[LJE_DETOUR_SIZE], void* detour)
{
    // Since we do not care about calling conventions, this is cross-platform atleast for 64-bit Windows and Linux
    // as both mark RAX as a caller-saved register.
    uint64_t addr = (uint64_t)(uintptr_t)detour;
    out[0] = 0x48; // REX.W prefix
    out[1] = 0xB8; // MOV RAX, imm64
    memcpy(&out[2], &addr, sizeof(addr));
    out[10] = 0xFF; // JMP
    out[11] = 0xE0; // JMP RAX
}

static int patch_bytes(void* target, const uint8_t* bytes)
{
    /* RWX keeps neighbouring functions on the page executable while we write.
     * A hardened kernel may refuse it, in which case RW is the fallback. */
    if (!lje_plat_protect(target, LJE_DETOUR_SIZE, LJE_PROT_RWX) &&
        !lje_plat_protect(target, LJE_DETOUR_SIZE, LJE_PROT_RW))
        return 0;

    memcpy(target, bytes, LJE_DETOUR_SIZE);

    if (!lje_plat_protect(target, LJE_DETOUR_SIZE, LJE_PROT_RX))
        return 0;

    lje_plat_flush_icache(target, LJE_DETOUR_SIZE);
    return 1;
}

int lje_detour(void* target, void* detour)
{
    if (target == NULL || detour == NULL)
        return 0;

    uint8_t code[LJE_DETOUR_SIZE];
    build_jump(code, detour);
    return patch_bytes(target, code);
}

int lje_detour_resume(LJEDetourHook* hook)
{
    if (hook == NULL || hook->installed)
        return 0;

    uint8_t code[LJE_DETOUR_SIZE];
    build_jump(code, hook->detour);
    if (!patch_bytes(hook->target, code))
        return 0;

    hook->installed = 1;
    return 1;
}

int lje_detour_suspend(LJEDetourHook* hook)
{
    if (hook == NULL || !hook->installed)
        return 0;

    if (!patch_bytes(hook->target, hook->original))
        return 0;

    hook->installed = 0;
    return 1;
}

int lje_detour_hook(LJEDetourHook* hook, void* target, void* detour)
{
    if (hook == NULL || target == NULL || detour == NULL)
        return 0;

    memcpy(hook->original, target, LJE_DETOUR_SIZE);
    hook->target = target;
    hook->detour = detour;
    hook->installed = 0;

    return lje_detour_resume(hook);
}
