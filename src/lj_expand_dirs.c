#include "lj_expand_dirs.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static const char* _lje_dir_mappings[] = {
#define DIRDEF(_, path, __) path,
#include "lje_directories.h"
#undef DIRDEF
};

static const char* _lje_legacy_mappings[] = {
#define DIRDEF(_, __, old) old,
#include "lje_directories.h"
#undef DIRDEF
};

static int copy_recursive(const char* src, const char* dst)
{
    CreateDirectoryA(dst, NULL);

    char search[MAX_PATH];
    snprintf(search, MAX_PATH, "%s\\*", src);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    int ok = 1;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        char src_path[MAX_PATH], dst_path[MAX_PATH];
        snprintf(src_path, MAX_PATH, "%s\\%s", src, fd.cFileName);
        snprintf(dst_path, MAX_PATH, "%s\\%s", dst, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!copy_recursive(src_path, dst_path))
                ok = 0;
        }
        else
        {
            if (!CopyFileA(src_path, dst_path, FALSE))
                ok = 0;
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return ok;
}

static int try_create_dir(const char* path)
{
    int result = SHCreateDirectoryExA(NULL, path, NULL);
    if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS)
        return 1;  // Directory created or already exists

    return 0;  // Failed to create directory
}

int lje_directory_get(LJEDirectory dir, char* out_buffer, size_t buffer_size)
{
    out_buffer[0] = '\0';  // Ensure output buffer is null-terminated

    char resolved_path[MAX_PATH];
    int result = ExpandEnvironmentStringsA(LJE_ROOT_DIR, resolved_path, MAX_PATH);
    if (result == 0 || result > MAX_PATH)
        return 0;  // Failed to resolve environment variable

    strncat_s(resolved_path, MAX_PATH, _lje_dir_mappings[dir], _TRUNCATE);

    // Try to create
    if (!try_create_dir(resolved_path))
        return 0;  // Failed to create directory

    strncpy_s(out_buffer, buffer_size, resolved_path, _TRUNCATE);
    return 1;  // Success
}

static int get_migration_flag()
{
    char flag_path[MAX_PATH];
    int result = ExpandEnvironmentStringsA(LJE_ROOT_DIR, flag_path, MAX_PATH);
    if (result == 0 || result > MAX_PATH)
    {
        return 1;  // Failed to resolve environment variable, assume migration is done to avoid potential data loss
    }

    strncat_s(flag_path, MAX_PATH, "migration.txt", _TRUNCATE);
    if (GetFileAttributesA(flag_path) != INVALID_FILE_ATTRIBUTES)
    {
        return 1;  // Migration flag file exists, migration already done
    }

    // Create the flag file to indicate migration has been completed
    FILE* flag_file = NULL;
    fopen_s(&flag_file, flag_path, "w");
    if (flag_file)
    {
        char message[1024];
        // time
        char time_str[64];
        SYSTEMTIME st;
        GetLocalTime(&st);
        snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        snprintf(message, sizeof(message),
            "A migration of legacy LJE directories has been completed. Old directories have been copied to the new location under %s.\n\n"
            "You can safely delete the old directories if you no longer need them.\n"
            "Migration completed at: %s\n",
            LJE_ROOT_DIR, time_str);
        fprintf(flag_file, "%s", message);
        fclose(flag_file);
        return 0;  // Migration needed and flag file created
    }

    return 1;  // Failed to create flag file, assume migration is done to avoid potential data loss
}

int lje_migrate_legacy_dirs()
{
    if (get_migration_flag())
    {
        printf("[LJE INFO] Migration already completed or not needed.\n");
        return 1;  // Migration already done or not needed
    }

    int count = sizeof(_lje_dir_mappings) / sizeof(_lje_dir_mappings[0]);
    for (int i = 0; i < count; i++)
    {
        if (!_lje_legacy_mappings[i]) continue;

        char old_resolved[MAX_PATH];
        int result = ExpandEnvironmentStringsA(_lje_legacy_mappings[i], old_resolved, MAX_PATH);
        if (result == 0 || result > MAX_PATH) {
            printf("[LJE WARNING] Failed to resolve legacy path for migration: %s\n", _lje_legacy_mappings[i]);
            continue;  // Failed to resolve environment variable, skip migration for this directory
        }

        // Has to exist
        if (GetFileAttributesA(old_resolved) == INVALID_FILE_ATTRIBUTES) {
            printf("[LJE INFO] Legacy directory does not exist, skipping migration: %s\n", old_resolved);
            continue;  // Legacy directory does not exist, skip migration for this directory
        }

        char new_resolved[MAX_PATH];
        if (!lje_directory_get((LJEDirectory)i, new_resolved, MAX_PATH)) {
            printf("[LJE WARNING] Failed to resolve new path for migration: %s\n", _lje_dir_mappings[i]);
            continue;  // Failed to resolve new path, skip migration for this directory
        }

        printf("[LJE INFO] Migrating data from %s to %s\n", old_resolved, new_resolved);
        if (copy_recursive(old_resolved, new_resolved))
        {
            printf("[LJE INFO] Successfully migrated data from %s to %s\n", old_resolved, new_resolved);
        }
        else
        {
            printf("[LJE WARNING] Failed to migrate data from %s to %s\n", old_resolved, new_resolved);
        }
    }

    return 1;  // Migration attempted (success or failure)
}