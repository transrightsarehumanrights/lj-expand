#pragma once

#include <stddef.h>
#define LJE_ROOT_DIR "%USERPROFILE%\\.lje\\"
typedef enum
{
#define DIRDEF(name, _, __) name,
#include "lje_directories.h"
#undef DIRDEF
} LJEDirectory;

int lje_directory_get(LJEDirectory dir, char* out_buffer, size_t buffer_size);
int lje_migrate_legacy_dirs();