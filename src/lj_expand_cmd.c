#include "lj_expand_cmd.h"

#include <stdlib.h>
#include <string.h>

#include "lj_expand_platform.h"

static LJECommandLineOptions* lje_global_command_line_options = NULL;

static void lje_parse_command_line() {
    lje_global_command_line_options = (LJECommandLineOptions*)malloc(sizeof(LJECommandLineOptions));
    memset(lje_global_command_line_options, 0, sizeof(LJECommandLineOptions));

    LJECommandLineOptions* options = lje_global_command_line_options; // Shorter alias

    const char* cmd_line = lje_plat_command_line();

#define CMDDEF(switch_str, name) \
    options->name = (strstr(cmd_line, switch_str) != NULL);
#include "lje_command_options.h"
#undef CMDDEF
}

LJECommandLineOptions* lje_get_command_line_options() {
    if (!lje_global_command_line_options) {
        lje_parse_command_line();
    }

    return lje_global_command_line_options;
}
