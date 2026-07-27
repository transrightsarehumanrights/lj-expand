#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ljel_log.h"
#include "ljel_process.h"

#define GMOD_PATH_ENV_NAME "GMOD_PATH"
#define LJE_DLL "lje-w64.dll"

char* resolve_lje_path()
{
    char fullLjePath[260];
    if (!ljel_full_path(LJE_DLL, fullLjePath, sizeof(fullLjePath)))
    {
        return NULL;
    }

    return _strdup(fullLjePath);
}

char* resolve_gmod_path()
{
    // Get the GMod path from the environment variable
    char* gmod_path = getenv(GMOD_PATH_ENV_NAME);
    if (gmod_path == NULL)
    {
        return NULL;
    }

    return _strdup(gmod_path);
}

int main(int argc, char* argv[])
{
    char* dll_path = resolve_lje_path();
    if (dll_path == NULL)
    {
        ljel_panic("Failed to resolve LJE DLL path");
    }

    char* gmod_path = resolve_gmod_path();
    if (gmod_path == NULL)
    {
        ljel_log("GMOD_PATH is not set. Please set this environment variable to the GMod executable path in bin/win64.");

        ljel_alert(
          "Error",
          "GMOD_PATH is not set.\nPlease set this environment variable to the GMod executable path in bin/win64."
        );

        return 1;
    }

    ljel_log("Launching GMod from path: %s", gmod_path);
    launch_and_inject(gmod_path, dll_path, argc, argv); /* forward command line args to gmod for convenience */
    return 0;
}
