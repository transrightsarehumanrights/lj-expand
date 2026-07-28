#include "lj_expand_crash_handler.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lj_debug.h"
#include "lj_expand_dirs.h"
#include "lj_expand_globals.h"
#include "lj_expand_platform.h"
#include "lj_frame.h"
#include "lj_obj.h"

/* Register names indexed by LJE_REG_* enum order. */
static const char* const reg_names[LJE_REG_COUNT] = {
  "RAX", "RBX", "RCX", "RDX",
  "RSI", "RDI", "RBP", "RSP",
  "R8",  "R9",  "R10", "R11",
  "R12", "R13", "R14", "R15",
  "RIP"
};

/* ------------------------------------------------------------------ */

static void make_dump_filename(char* out, size_t out_size)
{
    LJEPlatTime lt;
    lje_plat_local_time(&lt);
    snprintf(out, out_size, "lje_crash_%04d%02d%02d_%02d%02d%02d",
             lt.year, lt.month, lt.day, lt.hour, lt.minute, lt.second);
}

/* Write a crash state text dump into the LJE crash directory. */
static void lje_write_state_dump(const LJEPlatCrashInfo* info, const char* dump_path)
{
    FILE* f = fopen(dump_path, "w");
    if (!f)
        return;

    fprintf(f, "LJE Crash Dump\n================\n\n");
    fprintf(f, "Exception code: 0x%08X (%s)\n", info->native_code, info->name ? info->name : "UNKNOWN");
    fprintf(f, "Fault address: 0x%p\n", info->fault_addr);
    fprintf(f, "Frame count: %zu\n", info->frame_count);

    fprintf(f, "\nRegisters:\n");
    for (int i = 0; i < LJE_REG_COUNT; i++) {
        fprintf(f, "  %-4s: 0x%016llX\n", reg_names[i], (unsigned long long)info->regs[i]);
    }

    fprintf(f, "\nLJE dump\n================\n\n");

    lua_State* L = LJEG()->isolated_state;
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

    if (!lje_plat_addr_readable(func))
    {
        fprintf(f, "\nCurrent function pointer (0x%p) is not valid. Stack may be too corrupted to retrieve.\n", (void*)func);
        fclose(f);
        return;
    }

    fprintf(f, "\nStack trace:\n");
    for (int level = 0; ; level++)
    {
        int size = 0;

        cTValue* frame = lj_debug_frame(L, level, &size);

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
            fprintf(f, "  [%d] C function: %p\n", level, (void*)fn);
            char mod_name[LJE_PATH_MAX];
            if (lje_plat_module_name_from_addr((void*)fn->c.f, mod_name, sizeof(mod_name)))
            {
                fprintf(f, "    (Module: %s)\n", mod_name);
            }
            else
            {
                fprintf(f, "    (Module: unknown)\n");
            }
        }
        else
        {
            fprintf(f, "  [%d] Unknown function type (ffid=%d): %p\n", level, fn->c.ffid, (void*)fn);
        }
    }

    global_State* gl = G(L);
    fprintf(f, "\nHooks:\n");
    fprintf(f, "  Hook function: %p\n", (void*)gl->hookf);
    fprintf(f, "  Hook mask: 0x%02X\n", gl->hookmask);
    if (gl->hookmask & HOOK_GC)
    {
        fprintf(f, "    (GC hook active)\n");
    }
    fprintf(f, "  Hook count: %d\n", gl->hookcount);
    fprintf(f, "vmstate: %d\n", gl->vmstate);
    if (gl->jit_base.ptr64)
    {
        fprintf(f, "  JIT base (JIT code is running): %p\n", (void*)(uintptr_t)gl->jit_base.ptr64);
    }
    fclose(f);
}

/* Crash callback registered with the platform layer.
 * Returns 0 to continue search (preserving EXCEPTION_CONTINUE_SEARCH semantics). */
static int lje_on_crash(const LJEPlatCrashInfo* info, void* ud)
{
    (void)ud;

    /* Determine if this crash is relevant to LJE or LuaJIT. */
    int is_relevant = 0;
    LJEPlatModule luamod;
    uintptr_t base;
    size_t size;

    /* Check fault address against LJE's own range. */
    if (lje_plat_self_range(&base, &size))
    {
        uintptr_t fault = (uintptr_t)info->fault_addr;
        if (fault >= base && fault < base + size)
            is_relevant = 1;
    }

    /* Check fault address against LuaJIT's range. */
    if (!is_relevant && lje_plat_module_find(LJE_LUA_MODULE, &luamod))
    {
        uintptr_t fault = (uintptr_t)info->fault_addr;
        if (fault >= luamod.base && fault < luamod.base + luamod.size)
            is_relevant = 1;
    }

    /* Check each frame address against both ranges. */
    if (!is_relevant)
    {
        for (size_t i = 0; i < info->frame_count; i++)
        {
            uintptr_t frame = (uintptr_t)info->frames[i];
            if (frame == 0)
                continue;
            if (lje_plat_self_range(&base, &size) &&
                frame >= base && frame < base + size)
            {
                is_relevant = 1;
                break;
            }
            if (lje_plat_module_find(LJE_LUA_MODULE, &luamod) &&
                frame >= luamod.base && frame < luamod.base + luamod.size)
            {
                is_relevant = 1;
                break;
            }
        }
    }

    if (!is_relevant)
        return 0;

    /* Ensure crash dump directory exists and build paths. */
    char dumps_dir[LJE_PATH_MAX];
    if (!lje_directory_get(LJE_DIR_CRASH_DUMPS, dumps_dir, sizeof(dumps_dir)))
        return 0;

    char filename[LJE_PATH_MAX];
    make_dump_filename(filename, sizeof(filename));

    char state_path[LJE_PATH_MAX];
    if (!lje_path_join(state_path, sizeof(state_path), dumps_dir, filename))
        return 0;
    lje_strlcat(state_path, ".txt", sizeof(state_path));

    /* Write the state text dump. */
    lje_write_state_dump(info, state_path);

    /* Write the platform-native minidump. */
    char minidump_path[LJE_PATH_MAX];
    if (lje_path_join(minidump_path, sizeof(minidump_path), dumps_dir, filename))
    {
        lje_strlcat(minidump_path, ".dmp", sizeof(minidump_path));
        lje_plat_crash_write_native_dump(info, minidump_path);
    }

    /* Show a message box to the user. */
    lje_plat_message_box(
        "LJE Crash Handler",
        "LJE has encountered a crash. A crash dump will be generated in your LJE crash directory to help fix the issue.\n\n"
        "Please consider sharing the crash dump to help improve LJE. It will not automatically be shared.\n\n"
    );

    return 0; /* Let the OS continue searching for other handlers */
}

/* ------------------------------------------------------------------ */

void lje_crash_handler_init(void)
{
    lje_plat_crash_install(lje_on_crash, NULL);
}
