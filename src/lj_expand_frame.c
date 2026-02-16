#include "lj_expand_frame.h"
#include "lj_debug.h"
#include "lj_expand_globals.h"
#include "lj_frame.h"

int lje_frame_is_lua_involved(lua_State* L, int frame_offset)
{
    for (int i = frame_offset; ; i++)
    {
        int size = 0;
        cTValue* frame = lj_debug_frame(L, i, &size);
        if (frame == NULL)
        {
            break;
        }

        if (frame_islua(frame) || isluafunc(frame_func(frame)))
        {
            return 1;
        }
    }

    return 0;
}

int lje_frame_is_lje_involved(lua_State* L, int frame_offset, int max_level)
{
    if (max_level < 0)
    {
        max_level = INT32_MAX - frame_offset; /* so we don't overflow! */
    }

    for (int i = frame_offset; i < frame_offset + max_level; i++)
    {
        int size = 0;
        LJEG()->show_special_frames = 1;
        cTValue* frame = lj_debug_frame(L, i, &size);
        LJEG()->show_special_frames = 0;
        if (frame == NULL)
        {
            break;
        }

        GCfunc* fn = frame_func(frame);
        if (isluafunc(fn))
        {
            LJEproto* pt = protoextend(funcproto(fn));
            if (pt->is_from_lje)
                return 1;
        }
    }

    return 0;
}

int lje_frame_is_lje(lua_State* L, int frame_offset)
{
    int size = 0;
    LJEG()->show_special_frames = 1;
    cTValue* frame = lj_debug_frame(L, frame_offset, &size); // Caller is frame 1
    LJEG()->show_special_frames = 0;
    if (frame == NULL)
    {
        return 0;
    }

    GCfunc* fn = frame_func(frame);
    if (isluafunc(fn))
    {
        LJEproto* pt = protoextend(funcproto(fn));
        if (pt->is_from_lje)
            return 1;
    }

    return 0;
}