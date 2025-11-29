#include "ljel_log.h"
#include <stdarg.h>
#include <stdio.h>

void ljel_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[LJE-LAUNCHER]: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void ljel_log_noline(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[LJE-LAUNCHER]: ");
    vprintf(fmt, args);
    va_end(args);
}

void ljel_panic(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[LJE-LAUNCHER PANIC]: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
}