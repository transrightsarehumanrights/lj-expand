/* These are all internal GMod LuaJIT signatures from November 2025 */
/* They should not really change often, but if they do, updating them is necessary to keep GMod working */

// For tracking the game's function allocation state
SIGDEF(lj_func_newL_gc, "4c 89 44 24 18 48 89 54 24 10 53 56 57 41 54 41 55 48 83 ec 60 4c 8b 49 10 49 8b f0 48 8b fa 4c 8b e9 49 8b 41 28 49 39 41 20")
SIGDEF(lj_func_newL_empty, "48 89 5c 24 18 48 89 6c 24 20 57 41 54 41 56 48 83 ec 20 48 8b ea 49 8b d8 0f b6 52 3c 4c 8b e1 48 8d 14 d5 28 00 00 00")
SIGDEF(lj_func_newC, "48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 8b da 49 8b f0 48 8b f9 48 8d 14 dd 30 00 00 00")
SIGDEF(lj_func_free, "80 7a 0a 00 b8 30 00 00 00 4c 8b d1 41 b8 28 00 00 00 44 0f 45 c0 0f b6 42 0b 45 33 c9 4d 8d 04 c0 4c 29 41 20")
SIGDEF(lj_func_freeproto, "44 8b 42 38 48 8b c1 4c 29 41 20 45 33 c9 48 8b 49 18")

// Not needed as of now.
// SIGDEF(propagatemark, "40 53 48 83 ec 20 48 8b 59 48 4c 8b c9 0f b6 4b 09 80 4b 08 04 48 8b 43 18 49 89 41 48 80 f9 0b")
// This is a really niche bug, but this is necessary to fix crashes inside GMod's custom Lua C code.
// It's something to do with how lj_debug_funcname can get called, but it wont handle spoofing properly.
// Then, it'll try to read bytecodes past the valid memory region and crash.
// For overriding metatable lookups.

// For modifying protos during parsing
SIGDEF(fs_finish, "40 55 56 41 54 41 55 41 56 41 57 48 83 ec 38 48 8b 31")
