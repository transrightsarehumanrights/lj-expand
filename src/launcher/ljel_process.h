#pragma once

#include <stddef.h>

/* A bit ugly, but we'll define a single function to handle injection so its easier to do cross platform */
void launch_and_inject(const char* gmod_path, const char* lje_path, int argc, char** argv);

/* Resolve a relative path to an absolute path. Returns 1 on success, 0 on failure.
 * out is always NUL-terminated. */
int ljel_full_path(const char* relative, char* out, size_t n);

/* Show a simple error message box to the user (no-op on non-GUI platforms). */
void ljel_alert(const char* title, const char* text);
