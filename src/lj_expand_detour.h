#ifndef _LJ_EXPAND_DETOUR_H
#define _LJ_EXPAND_DETOUR_H

#include "lj_obj.h"

// Dead simple detours. No original function is provided.
LJ_FUNC int lje_detour(void* target, void* detour);

#endif