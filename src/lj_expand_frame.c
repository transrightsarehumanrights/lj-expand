#include "lj_expand_frame.h"
#include "lj_debug.h"
#include "lj_frame.h"

int lje_frame_is_lua_involved(lua_State* L)
{
    for (int i = 0; ; i++)
    {
        int size = 0;
        cTValue* frame = lj_debug_frame(L, i, &size);
        if (frame == NULL)
        {
            break;
        }

        if (frame_islua(frame))
        {
            return 1;
        }
    }

    return 0;
}