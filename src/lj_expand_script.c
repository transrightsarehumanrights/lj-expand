#include "lj_expand_script.h"
#ifdef _WINDOWS
#include <windows.h>
#endif

void lje_script_resolve_base(char* out_buffer, size_t buffer_size) {
#ifdef _WINDOWS
    char base_path[MAX_PATH] = { 0 };
    strncat_s(base_path, MAX_PATH, "%USERPROFILE%\\", _TRUNCATE);
    strncat_s(base_path, MAX_PATH, LJE_SCRIPT_FOLDER, _TRUNCATE);

    DWORD result = ExpandEnvironmentStringsA(base_path, out_buffer, (DWORD)buffer_size);
    if (result == 0 || result > buffer_size)
    {
        // Failed to expand or buffer too small
        out_buffer[0] = '\0';
    }
#else
#error "lje_script_resolve_base not implemented for this platform"
#endif
}

int lje_script_folder_exists() {
    // Simple check for folder existence
#ifdef _WINDOWS
    char path[MAX_PATH] = { 0 };
    lje_script_resolve_base(path, MAX_PATH);
    DWORD attribs = GetFileAttributesA(path);
    if ((attribs != INVALID_FILE_ATTRIBUTES) && (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        return 1; // Exists and is a directory
    }
#else
#error "lje_script_folder_exists not implemented for this platform"
#endif
    return 0;
}
