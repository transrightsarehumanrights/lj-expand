/* These are all internal GMod LuaJIT signatures from November 2025 */
/* They should not really change often, but if they do, updating them is necessary to keep GMod working */

SIGDEF(lj_func_newL_gc, "4c 89 44 24 18 48 89 54 24 10 53 56 57 41 54 41 55 48 83 ec 60 4c 8b 49 10 49 8b f0 48 8b fa 4c 8b e9 49 8b 41 28 49 39 41 20")
SIGDEF(lj_func_newL_empty, "48 89 5c 24 18 48 89 6c 24 20 57 41 54 41 56 48 83 ec 20 48 8b ea 49 8b d8 0f b6 52 3c 4c 8b e1 48 8d 14 d5 28 00 00 00")
SIGDEF(lj_func_newC, "48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 8b da 49 8b f0 48 8b f9 48 8d 14 dd 30 00 00 00")
SIGDEF(lj_func_free, "80 7a 0a 00 b8 30 00 00 00 4c 8b d1 41 b8 28 00 00 00 44 0f 45 c0 0f b6 42 0b 45 33 c9 4d 8d 04 c0 4c 29 41 20")
SIGDEF(lj_func_freeproto, "44 8b 42 38 48 8b c1 4c 29 41 20 45 33 c9 48 8b 49 18")
// Not needed as of now.
// SIGDEF(propagatemark, "40 53 48 83 ec 20 48 8b 59 48 4c 8b c9 0f b6 4b 09 80 4b 08 04 48 8b 43 18 49 89 41 48 80 f9 0b")
SIGDEF(callhook, "40 53 56 57 48 81 ec f0 00 00 00 48 ?? ?? ?? ?? ?? ?? 48 33 c4 48 89 84 24 e0 00 00 00 48 8b 59 10 48 8b f9 48 8b b3 38 01 00 00 48 85 f6")
SIGDEF(lj_cf_debug_getinfo, "48 89 5c 24 10 48 89 6c 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 81 ec 00 01 00 00 48 ?? ?? ?? ?? ?? ?? 48 33 c4 48 89 84 24 f0 00 00 00 48 8b 71 20 48 8b d9")
SIGDEF(lj_debug_frame, "48 89 5c 24 08 48 89 74 24 10 48 89 7c 24 18 4c 8b 59 38 49 8b f8 48 8b 41 20 49 83 c3 08 48 83 e8 08 44 8b ca 48 8b d9 4c 8b d0")
SIGDEF(lj_strfmt_obj, "40 56 57 48 83 ec 58 48 ?? ?? ?? ?? ?? ?? 48 33 c4 48 89 44 24 40 48 8b 02 48 8b f9 48 8b c8 48 8b f2 48 c1 f9 2f 83 f9 fb 75 12 48 b9 ff ff ff ff ff 7f 00 00 48 23 c1")
SIGDEF(lj_cf_jit_util_funcinfo, "48 89 5c 24 18 55 56 57 48 83 ec 20 48 8b 59 20 48 8b f9 48 39 59 28")
SIGDEF(lj_cf_jit_util_funcbc, "48 89 5c 24 08 57 48 83 ec 20 48 8b 59 20 48 8b f9 48 39 59 28")
SIGDEF(lj_cf_debug_getlocal, "48 89 5c 24 10 48 89 6c 24 18 56 57 41 56 48 81 ec f0 00 00 00 48 ?? ?? ?? ?? ?? ?? 48 33 c4 48 89 84 24 e0 00 00 00 48 8b 79 20")
// This is a really niche bug, but this is necessary to fix crashes inside GMod's custom Lua C code.
// It's something to do with how lj_debug_funcname can get called, but it wont handle spoofing properly.
// Then, it'll try to read bytecodes past the valid memory region and crash.
SIGDEF(lj_debug_funcname, "48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 48 8b 41 38 49 8b f0 48 83 c0 08 48 8b d9 48 3b d0")
// Spoofs equality for spoofed functions.
SIGDEF(lj_obj_equal, "4c 8b 09 4c 8b 12 49 8b c1 4d 8b c2 48 c1 f8 2f 49 c1 f8 2f 41 3b c0")
SIGDEF(lj_cf_string_dump, "48 89 5c 24 08 57 48 83 ec 30 ba 01 00 00 00 48 8b d9")
// (debug.)(set|get)fenv.
SIGDEF(lj_cf_setfenv, "48 89 5c 24 10 48 89 74 24 18 57 48 83 ec 20 ba 02 00 00 00 48 8b f9 e8 ?? ?? ?? ?? 48 8b 5f 20 48 8b f0 48 3b 5f 28 73 0f 48 8b 1b 48 8b d3 48 c1 fa 2f 83 fa f7")
SIGDEF(lj_cf_getfenv, "40 53 48 83 ec 20 48 8b d9 48 8b 49 20 4c 8b 43 28 49 3b c8 73 0f 48 8b 09 48 8b c1 48 c1 f8 2f 83 f8 f7 74 30")
SIGDEF(lj_cf_debug_getfenv, "40 53 48 83 ec 20 ba 01 00 00 00 48 8b d9 e8 ?? ?? ?? ?? ba 01 00 00 00 48 8b cb e8 ?? ?? ?? ?? b8 01 00 00 00 48 83 c4 20 5b c3")
SIGDEF(lj_cf_debug_setfenv, "40 53 48 83 ec 20 ba 02 00 00 00 48 8b d9 e8 ?? ?? ?? ?? 48 8b 43 20 ba 01 00 00 00 48 83 c0 10 48 8b cb 48 89 43 28 e8 ?? ?? ?? ?? 85 c0 74 0b b8 01 00 00 00")
SIGDEF(hashkey, "4c 8b 02 4c 8b c9 49 8b d0 48 c1 fa 2f 83 fa fb")
SIGDEF(lj_tab_newkey, "48 89 5c 24 10 48 89 6c 24 18 48 89 74 24 20 41 56 48 83 ec 20 48 8b da 48 8b e9 48 8b cb 49 8b d0")
// For overriding metatable lookups.
SIGDEF(lj_meta_lookup, "40 53 48 83 ec 20 4c 8b 0a 48 8b d9 49 8b c1 48 c1 f8 2f 83 f8 f4 74 05 83 f8 f3")
SIGDEF(lj_record_mm_lookup, "48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18 48 89 7c 24 20 41 54 41 56 41 57 48 83 ec 70 4c 8b 0a 48 8b f9 8b 4a 30")

// For preventing JIT bypassing spoof equality.
SIGDEF(lj_record_objcmp, "48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18 48 89 7c 24 20 41 56 48 83 ec 20 8b ea 48 8b d9 48 8b 54 24 50")
// For modifying protos during parsing
SIGDEF(fs_finish, "40 55 56 41 54 41 55 41 56 41 57 48 83 ec 38 48 8b 31")
// For running a parallel RNG state
SIGDEF(lj_cf_math_random, "48 89 5c 24 18 56 48 83 ec 40 48 8b 41 20 48 8b d9 48 8b 71 28")
SIGDEF(lj_cf_math_randomseed, "40 53 48 83 ec 20 48 8b 41 20 48 8b 50 f0 48 b8 ff ff ff ff ff 7f 00 00")
// Allows LJE code to get JITed but still use the alternate random state.
SIGDEF(recff_math_random, "48 89 5c 24 10 48 89 6c 24 18 48 89 7c 24 20 41 56 48 83 ec 20 48 8b 81 90 00 00 00 41 b8 0c 00 00 00 48 8b f9 48 8b 58 30")

// Allows LJE to block errors
SIGDEF(lj_err_run, "48 89 5c 24 08 57 48 83 ec 20 4c 8b 49 38 48 8b f9 4c 8b 41 20")
SIGDEF(setjitmode, "48 89 5c 24 08 57 48 83 ec 20 4c 8b 41 20 8b da 4c 8b 49 28 33 d2 48 8b f9 44 8b d2")