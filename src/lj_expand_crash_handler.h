#pragma once

/* LJE crash handler - registers a portable callback with the platform layer
 * to capture LJE/LuaJIT-related crashes. */
void lje_crash_handler_init(void);
