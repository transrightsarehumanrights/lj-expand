#include "lj_expand_dirs.h"
#include "lj_expand_log.h"
#include "lj_expand_platform.h"

#include <stdio.h>
#include <string.h>

/* Maximum recursion depth for copy_recursive to prevent stack overflow
 * when LJE_PATH_MAX (1024) stack buffers are used. */
#define COPY_RECURSIVE_MAX_DEPTH 32

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

/* Resolve the LJE root directory: ~/.lje (or %USERPROFILE%\.lje on Windows).
 * Returns 1 on success, 0 on failure. out is always NUL-terminated. */
static int resolve_lje_root(char* out, size_t n)
{
    char home[LJE_PATH_MAX];
    if (!lje_plat_home_dir(home, sizeof(home)))
        return 0;
    return lje_path_join(out, n, home, LJE_ROOT_DIR);
}

/* Recursive copy with depth cap. depth starts at 0. */
static int copy_recursive(const char* src, const char* dst, int depth)
{
    if (depth >= COPY_RECURSIVE_MAX_DEPTH) {
        LJE_ERROR("copy_recursive: max depth (%d) exceeded for %s", COPY_RECURSIVE_MAX_DEPTH, src);
        return 0;
    }

    lje_plat_mkdir(dst);

    LJEPlatDir* d = lje_plat_dir_open(src, "*");
    if (!d)
        return 0;

    int ok = 1;
    LJEPlatDirEntry entry;
    while (lje_plat_dir_next(d, &entry)) {
        char src_path[LJE_PATH_MAX];
        char dst_path[LJE_PATH_MAX];
        if (!lje_path_join(src_path, sizeof(src_path), src, entry.name) ||
            !lje_path_join(dst_path, sizeof(dst_path), dst, entry.name)) {
            ok = 0;
            continue;
        }

        if (entry.is_dir) {
            if (!copy_recursive(src_path, dst_path, depth + 1))
                ok = 0;
        } else {
            if (!lje_plat_copy_file(src_path, dst_path))
                ok = 0;
        }
    }

    lje_plat_dir_close(d);
    return ok;
}

int lje_directory_get(LJEDirectory dir, char* out_buffer, size_t buffer_size)
{
    if (!out_buffer || buffer_size == 0)
        return 0;

    out_buffer[0] = '\0';

    char root[LJE_PATH_MAX];
    if (!resolve_lje_root(root, sizeof(root)))
        return 0;

    char full_path[LJE_PATH_MAX];
    if (!lje_path_join(full_path, sizeof(full_path), root,
                        _lje_dir_mappings[dir]))
        return 0;

    /* Try to create the directory (idempotent) */
    if (!lje_plat_mkdirs(full_path))
        return 0;

    lje_strlcpy(out_buffer, full_path, buffer_size);
    return 1;
}

static int get_migration_flag(void)
{
    char root[LJE_PATH_MAX];
    if (!resolve_lje_root(root, sizeof(root)))
        return 1;  /* Assume done to avoid potential data loss */

    char flag_path[LJE_PATH_MAX];
    if (!lje_path_join(flag_path, sizeof(flag_path), root, "migration.txt"))
        return 1;

    if (lje_plat_fs_kind(flag_path) != LJE_FS_MISSING)
        return 1;  /* Migration flag file exists, migration already done */

    /* Create the flag file to indicate migration has been completed */
    FILE* flag_file = fopen(flag_path, "w");
    if (flag_file)
    {
        char time_str[64];
        LJEPlatTime lt;
        lje_plat_local_time(&lt);
        snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d",
                 lt.year, lt.month, lt.day, lt.hour, lt.minute, lt.second);

        char message[1024];
        snprintf(message, sizeof(message),
            "A migration of legacy LJE directories has been completed. Old directories have been copied to the new location under %s.\n\n"
            "You can safely delete the old directories if you no longer need them.\n"
            "Migration completed at: %s\n",
            "%USERPROFILE%\\.lje\\", time_str);
        fprintf(flag_file, "%s", message);
        fclose(flag_file);
        return 0;  /* Migration needed and flag file created */
    }

    return 1;  /* Failed to create flag file, assume migration is done */
}

int lje_migrate_legacy_dirs(void)
{
    if (get_migration_flag())
    {
        LJE_INFO("Migration already completed or not needed.");
        return 1;
    }

    int count = (int)(sizeof(_lje_dir_mappings) / sizeof(_lje_dir_mappings[0]));
    for (int i = 0; i < count; i++)
    {
        if (!_lje_legacy_mappings[i]) continue;

        char old_resolved[LJE_PATH_MAX];
        if (!lje_plat_expand_env(_lje_legacy_mappings[i], old_resolved, sizeof(old_resolved)))
        {
            LJE_WARN("Failed to resolve legacy path for migration: %s", _lje_legacy_mappings[i]);
            continue;
        }

        /* Has to exist */
        if (lje_plat_fs_kind(old_resolved) == LJE_FS_MISSING)
        {
            LJE_INFO("Legacy directory does not exist, skipping migration: %s", old_resolved);
            continue;
        }

        char new_resolved[LJE_PATH_MAX];
        if (!lje_directory_get((LJEDirectory)i, new_resolved, sizeof(new_resolved)))
        {
            LJE_WARN("Failed to resolve new path for migration: %s", _lje_dir_mappings[i]);
            continue;
        }

        LJE_INFO("Migrating data from %s to %s", old_resolved, new_resolved);
        if (copy_recursive(old_resolved, new_resolved, 0))
        {
            LJE_SUCCESS("Successfully migrated data from %s to %s", old_resolved, new_resolved);
        }
        else
        {
            LJE_WARN("Failed to migrate data from %s to %s", old_resolved, new_resolved);
        }
    }

    return 1;
}
