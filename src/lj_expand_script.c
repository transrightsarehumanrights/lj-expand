#include "lj_expand_script.h"
#include <stdio.h>
#ifdef _WIN64
#include <windows.h>
#endif

void lje_script_resolve_base(char* out_buffer, size_t buffer_size) {
#ifdef _WIN64
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
#ifdef _WIN64
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

int lje_script_folder_create()
{
#ifdef _WIN64
    char path[MAX_PATH] = { 0 };
    lje_script_resolve_base(path, MAX_PATH);
    if (CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 1; // Created or already exists
    }
#else
#error "lje_script_folder_create not implemented for this platform"
#endif
    return 0;
}

LJEScript* lje_script_load_all_scripts(size_t* out_script_count) {
#ifdef _WIN64
    char search_path[MAX_PATH] = { 0 };
    lje_script_resolve_base(search_path, MAX_PATH);
    strncat_s(search_path, MAX_PATH, "\\*", _TRUNCATE);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        printf("[LJE] No scripts found in %s\n", search_path);
        *out_script_count = 0;
        return NULL; // No scripts found
    }

    LJEScript* scripts = NULL;
    size_t script_count = 0;

    do
    {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Skip '.' and '..' directories
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
                continue;

            printf("[LJE] Found script folder: %s\n", find_data.cFileName);
            // Check if main.lua exists in this directory
            char main_lua_path[MAX_PATH] = { 0 };
            lje_script_resolve_base(main_lua_path, MAX_PATH);
            strncat_s(main_lua_path, MAX_PATH, "\\", _TRUNCATE);
            strncat_s(main_lua_path, MAX_PATH, find_data.cFileName, _TRUNCATE);
            strncat_s(main_lua_path, MAX_PATH, "\\", _TRUNCATE);
            char folder_path[MAX_PATH] = { 0 };
            strcpy_s(folder_path, MAX_PATH, main_lua_path); // Save folder path
            strncat_s(main_lua_path, MAX_PATH, LJE_SCRIPT_MAIN, _TRUNCATE);

            DWORD attribs = GetFileAttributesA(main_lua_path);
            if (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY))
            {
                // Found a valid script
                scripts = (LJEScript*)realloc(scripts, sizeof(LJEScript) * (script_count + 1));
                scripts[script_count].folder = _strdup(folder_path);
                scripts[script_count].main_path = _strdup(main_lua_path);
                scripts[script_count].name = _strdup(find_data.cFileName);
                script_count++;
            }
        }
    } while (FindNextFileA(hFind, &find_data) != 0);
    FindClose(hFind);

    *out_script_count = script_count;
    return scripts;
#else
#error "lje_script_load_all_scripts not implemented for this platform"
#endif
}