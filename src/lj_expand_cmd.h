#pragma once

#define DISABLE_SCRIPTS_SWITCH "--lje-disable-scripts"

typedef struct LJECommandLineOptions
{
#define CMDDEF(_, name) char name;
#include "lje_command_options.h"
#undef CMDDEF
} LJECommandLineOptions;

LJECommandLineOptions* lje_get_command_line_options();