#pragma once

#include <stddef.h>

/* Subfolder name under the user home directory. Resolved at runtime
 * via lje_plat_home_dir() + lje_path_join(). */
#define LJE_ROOT_DIR ".lje"

typedef enum
{
#define DIRDEF(name, _, __) name,
#include "lje_directories.h"
#undef DIRDEF
} LJEDirectory;

int lje_directory_get(LJEDirectory dir, char* out_buffer, size_t buffer_size);
int lje_migrate_legacy_dirs();
