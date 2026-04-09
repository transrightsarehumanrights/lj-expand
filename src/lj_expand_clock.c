#include "lj_expand_clock.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static LARGE_INTEGER lje_frequency = {0};

uint64_t lje_clock_get_ticks()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)counter.QuadPart;
}

double lje_clock_seconds_to_ticks(double seconds)
{
    if (lje_frequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&lje_frequency);
    }

    return seconds * (double)lje_frequency.QuadPart;
}

double lje_clock_ticks_to_seconds(uint64_t ticks)
{
    if (lje_frequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&lje_frequency);
    }

    return (double)ticks / (double)lje_frequency.QuadPart;
}
