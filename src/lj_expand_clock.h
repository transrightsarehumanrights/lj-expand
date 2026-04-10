#pragma once

#include <stddef.h>
#include <stdint.h>

uint64_t lje_clock_get_ticks();
double lje_clock_seconds_to_ticks(double seconds);
double lje_clock_ticks_to_seconds(uint64_t ticks);

