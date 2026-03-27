#include "lj_expand_script.h"
#include "tomlc17.h"

#include <stdio.h>

#include "lauxlib.h"
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

void lje_script_data_resolve_base(char* out_buffer, size_t buffer_size)
{
#ifdef _WIN64
    char base_path[MAX_PATH] = { 0 };
    strncat_s(base_path, MAX_PATH, "%USERPROFILE%\\", _TRUNCATE);
    strncat_s(base_path, MAX_PATH, LJE_SCRIPT_DATA, _TRUNCATE);

    DWORD result = ExpandEnvironmentStringsA(base_path, out_buffer, (DWORD)buffer_size);
    if (result == 0 || result > buffer_size)
    {
        // Failed to expand or buffer too small
        out_buffer[0] = '\0';
    }
#else
#error "lje_script_data_resolve_base not implemented for this platform"
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

int lje_script_data_folder_exists()
{
    // Basically just a copy of lje_script_folder_exists but for data
#ifdef _WIN64
    char path[MAX_PATH] = { 0 };
    lje_script_data_resolve_base(path, MAX_PATH);
    DWORD attribs = GetFileAttributesA(path);
    if ((attribs != INVALID_FILE_ATTRIBUTES) && (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        return 1; // Exists and is a directory
    }
#else
#error "lje_script_data_folder_exists not implemented for this platform"
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

int lje_script_data_folder_create()
{
#ifdef _WIN64
    char path[MAX_PATH] = { 0 };
    lje_script_data_resolve_base(path, MAX_PATH);
    if (CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 1; // Created or already exists
    }
#else
#error "lje_script_data_folder_create not implemented for this platform"
#endif
    return 0;
}

static char is_data_path_safe(const char* relative_path)
{
    if (strlen(relative_path) == 0 || strlen(relative_path) > 128)
    {
        return 0; // Too long or empty
    }
    // Only accept alphanumeric, underscore and hyphen characters
    // We specifically forbid subdirectories or path weaseling (e.g. ../)
    // It might seem excessive, but having a completely flat namespace for script data
    // is much safer and easier to manage.
    for (const char* p = relative_path; *p; p++)
    {
        if (!((*p >= 'a' && *p <= 'z') ||
              (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') ||
              (*p == '_') ||
              (*p == '-') ))
        {
            return 0; // Unsafe character found
        }
    }

    return 1; // Safe
}

int lje_script_data_write_file(const char* relative_path, const char* data, size_t data_size)
{
    // We can use standard C file IO here, so no cross-platform issues
    if (!is_data_path_safe(relative_path))
    {
        return 0; // Unsafe path
    }

    char full_path[MAX_PATH] = { 0 };
    lje_script_data_resolve_base(full_path, MAX_PATH);
    strncat_s(full_path, MAX_PATH, "\\", _TRUNCATE);
    strncat_s(full_path, MAX_PATH, relative_path, _TRUNCATE);
    strncat_s(full_path, MAX_PATH, ".dat", _TRUNCATE); // No custom extensions allowed here!

    FILE* file = NULL;
    if (fopen_s(&file, full_path, "wb") != 0 || !file)
    {
        return 0; // Failed to open file
    }

    size_t written = fwrite(data, 1, data_size, file);
    fclose(file);

    return written == data_size;
}

char* lje_script_data_read_file(const char* relative_path, size_t* out_data_size)
{
    // We can use standard C file IO here, so no cross-platform issues
    if (!is_data_path_safe(relative_path))
    {
        return NULL; // Unsafe path
    }

    char full_path[MAX_PATH] = { 0 };
    lje_script_data_resolve_base(full_path, MAX_PATH);
    strncat_s(full_path, MAX_PATH, "\\", _TRUNCATE);
    strncat_s(full_path, MAX_PATH, relative_path, _TRUNCATE);
    strncat_s(full_path, MAX_PATH, ".dat", _TRUNCATE); // No custom extensions allowed here!

    FILE* file = NULL;
    if (fopen_s(&file, full_path, "rb") != 0 || !file)
    {
        return NULL; // Failed to open file
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, length, file);
    fclose(file);

    if (read_size != length)
    {
        free(buffer);
        return NULL; // Read error
    }

    buffer[length] = '\0';
    if (out_data_size)
    {
        *out_data_size = length;
    }
    return buffer;
}

static void script_info_parse_error(const char* property, const char* details)
{
    printf("[LJE] Error parsing script info file: '%s' - %s\n", property, details);
}

static LJEScriptDependency* script_info_parse_dep(const char* dep_str)
{
    // Deps are formatted like:
    // author.name

    char* dot_pos = strchr(dep_str, '.');
    if (!dot_pos)
        return NULL;

    size_t author_start = 0;
    size_t author_length = dot_pos - dep_str;
    size_t name_start = author_length + 1;
    size_t name_length = strlen(dep_str) - name_start;

    LJEScriptDependency* dep = (LJEScriptDependency*)malloc(sizeof(LJEScriptDependency));
    dep->author = (char*)malloc(author_length + 1);
    dep->name = (char*)malloc(name_length + 1);

    strncpy_s((char*)dep->author, author_length + 1, dep_str + author_start, author_length);
    dep->author[author_length] = '\0';
    strncpy_s((char*)dep->name, name_length + 1, dep_str + name_start, name_length);
    dep->name[name_length] = '\0';

    return dep;
}
#define TYPE_CHECK_PROPERTY(prop, ctype) if (prop.type != ctype) { script_info_parse_error(#prop, "Invalid type or missing"); return NULL; }
LJEScriptInfo* lje_script_parse_info(const char* info_path)
{
    // Check if file exists
    if (GetFileAttributesA(info_path) == INVALID_FILE_ATTRIBUTES)
    {
        return NULL;
    }

    toml_result_t res = toml_parse_file_ex(info_path);
    if (!res.ok)
    {
        script_info_parse_error("file", "Failed to parse TOML file");
        return NULL;
    }

    toml_datum_t name = toml_seek(res.toptab, "script.name");
    toml_datum_t version = toml_seek(res.toptab, "script.version");
    toml_datum_t author = toml_seek(res.toptab, "script.author");
    toml_datum_t dependencies = toml_seek(res.toptab, "script.dependencies");

    TYPE_CHECK_PROPERTY(name, TOML_STRING);
    TYPE_CHECK_PROPERTY(version, TOML_STRING);
    TYPE_CHECK_PROPERTY(author, TOML_STRING);
    TYPE_CHECK_PROPERTY(dependencies, TOML_ARRAY);

    LJEScriptInfo* info = (LJEScriptInfo*)malloc(sizeof(LJEScriptInfo));
    info->name = _strdup(name.u.s);
    info->version = _strdup(version.u.s);
    info->author = _strdup(author.u.s);

    // Parse dependencies
    info->dependency_count = dependencies.u.arr.size;
    info->dependencies = (LJEScriptDependency*)malloc(sizeof(LJEScriptDependency) * info->dependency_count);
    for (size_t i = 0; i < info->dependency_count; i++)
    {
        toml_datum_t dep_item = dependencies.u.arr.elem[i];
        TYPE_CHECK_PROPERTY(dep_item, TOML_STRING);
        LJEScriptDependency* dep = script_info_parse_dep(dep_item.u.s);
        if (!dep)
        {
            script_info_parse_error("script.dependencies", "Invalid dependency format");
            free(info);
            return NULL;
        }

        info->dependencies[i] = *dep;
        free(dep);
    }

    toml_free(res);
    return info;
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
            char info_path[MAX_PATH] = { 0 };
            strcpy_s(info_path, MAX_PATH, folder_path);
            strncat_s(info_path, MAX_PATH, "info.toml", _TRUNCATE);

            DWORD attribs = GetFileAttributesA(main_lua_path);
            if (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY))
            {
                // Found a valid script
                LJEScriptInfo* info = lje_script_parse_info(info_path);
                if (!info)
                {
                    printf("[LJE] Warning: Script '%s' is missing a valid info.toml file. Skipping.\n", find_data.cFileName);
                    continue;
                }

                scripts = (LJEScript*)realloc(scripts, sizeof(LJEScript) * (script_count + 1));
                scripts[script_count].folder = _strdup(folder_path);
                scripts[script_count].main_path = _strdup(main_lua_path);
                scripts[script_count].name = _strdup(find_data.cFileName);
                scripts[script_count].info = info;

                LJEScriptExtraInfo* extra = (LJEScriptExtraInfo*)malloc(sizeof(LJEScriptExtraInfo));
                extra->engine_call_hook_ref_id = LUA_NOREF;
                scripts[script_count].extra = extra;

                // Check for preinit.lua
                char preinit_lua_path[MAX_PATH] = { 0 };
                strcpy_s(preinit_lua_path, MAX_PATH, folder_path);
                strncat_s(preinit_lua_path, MAX_PATH, LJE_SCRIPT_PREINIT, _TRUNCATE);
                attribs = GetFileAttributesA(preinit_lua_path);
                if (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY))
                {
                    scripts[script_count].preinit_path = _strdup(preinit_lua_path);
                }
                else
                {
                    scripts[script_count].preinit_path = NULL;
                }

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

static char is_script_in_load_order(LJEScript** load_order, size_t load_order_count, LJEScript* script)
{
    for (size_t i = 0; i < load_order_count; i++)
    {
        if (load_order[i] == script)
            return 1;
    }

    return 0;
}

LJEScript** lje_script_compute_load_order(size_t script_count, LJEScript* scripts)
{
    printf("[LJE] Computing script load order for %llu scripts...\n", script_count);
    // Simple dependency resolution, we traverse scripts on some random, undefined order
    // and for each newly encountered dependency, we insert it before the script that depends on it.
    // It is not necessarily the best algorithm, but it works for now. And of course, if a dependency has already
    // been added, we skip it.

    LJEScript** load_order = (LJEScript**)malloc(sizeof(LJEScript*) * script_count);
    size_t load_order_index = 0;
    // Ensure zeroed out
    for (size_t i = 0; i < script_count; i++)
        load_order[i] = NULL;

    // Script traversal
    for (size_t i = 0; i < script_count; i++)
    {
        printf("[LJE] Processing script for load order: %s by %s\n",
            scripts[i].info->name,
            scripts[i].info->author);
        if (is_script_in_load_order(load_order, script_count, &scripts[i]))
            continue; // Already added

        // Process dependencies first
        LJEScript* current_script = &scripts[i];
        for (size_t j = 0; j < current_script->info->dependency_count; j++)
        {
            printf("[LJE] - Checking dependency: %s by %s\n",
                current_script->info->dependencies[j].name,
                current_script->info->dependencies[j].author);
            LJEScriptDependency* dep = &current_script->info->dependencies[j];
            char found_dep = 0;
            // Find the script that matches this dependency
            for (size_t k = 0; k < script_count; k++)
            {
                printf("[LJE]   - Comparing against script: %s by %s\n",
                    scripts[k].info->name,
                    scripts[k].info->author);
                if (strcmp(scripts[k].info->name, dep->name) == 0 &&
                    strcmp(scripts[k].info->author, dep->author) == 0)
                {
                    printf("[LJE]   - Found matching dependency script: %s by %s\n",
                        scripts[k].info->name,
                        scripts[k].info->author);
                    // Found the dependency script
                    if (!is_script_in_load_order(load_order, script_count, &scripts[k]))
                    {
                        printf("[LJE]   - Inserting dependency script into load order: %s by %s\n",
                            scripts[k].info->name,
                            scripts[k].info->author);
                        // Insert dependency first
                        load_order[load_order_index++] = &scripts[k];
                    }

                    found_dep = 1;
                    break;
                }
            }

            if (!found_dep)
            {
                // Warn user they are missing a dependency
                printf("[LJE] Warning: Script '%s' by '%s' is missing dependency '%s' by '%s'\n",
                    current_script->info->name,
                    current_script->info->author,
                    dep->name,
                    dep->author);
            }
        }

        // Insert this script
        printf("[LJE] load_order_index = %llu, inserting script: %s by %s\n",
            load_order_index,
            current_script->info->name,
            current_script->info->author);
        load_order[load_order_index++] = current_script;
    }

    return load_order;
}

char** lje_script_find(LJEScript* script, const char* relative_path, size_t* out_path_count)
{
    if (strstr(relative_path, ".."))
    {
        // Forbid path weaseling
        printf("[LJE] lje_script_find: Forbidding path weaseling in path '%s'\n", relative_path);
        *out_path_count = 0;
        return NULL;
    }

    // Simple search for files in the script's folder matching the given relative path
    // Supports wildcards (* and ?)
    // Returns relative paths from the script's folder
#ifdef _WIN64
    char search_path[MAX_PATH] = { 0 };
    strcpy_s(search_path, MAX_PATH, script->folder);
    strncat_s(search_path, MAX_PATH, "\\", _TRUNCATE);
    strncat_s(search_path, MAX_PATH, relative_path, _TRUNCATE);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        *out_path_count = 0;
        return NULL; // No files found
    }

    char** found_paths = NULL;
    size_t found_count = 0;

    do
    {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            // Found a matching file, add to results (and make it relative to script folder)
            found_paths = (char**)realloc(found_paths, sizeof(char*) * (found_count + 1));
            char* full_path = (char*)malloc(MAX_PATH);
            full_path[0] = '\0';

            // Get directory portion of relative_path (everything before the last slash)
            const char* last_slash = strrchr(relative_path, '\\');
            const char* last_fslash = strrchr(relative_path, '/');
            if (last_fslash > last_slash) last_slash = last_fslash;

            if (last_slash)
            {
                // Copy the directory part (e.g., "modules/" from "modules/*")
                size_t dir_len = last_slash - relative_path + 1;
                strncpy_s(full_path, MAX_PATH, relative_path, dir_len);
                full_path[dir_len] = '\0';
            }

            strncat_s(full_path, MAX_PATH, find_data.cFileName, _TRUNCATE);
            found_paths[found_count++] = full_path;
        }
    } while (FindNextFileA(hFind, &find_data) != 0);
    FindClose(hFind);

    *out_path_count = found_count;
    return found_paths;
#else
#error "lje_script_find not implemented for this platform"
#endif
}

void lje_script_get_path(LJEScript* script, char* out_buffer, size_t buffer_size)
{
    // Simply return the main.lua path for now, we can expand this later if needed
    strncpy_s(out_buffer, buffer_size, script->main_path, _TRUNCATE);
}