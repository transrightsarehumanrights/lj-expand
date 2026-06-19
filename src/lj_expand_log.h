#ifndef LJ_EXPAND_LOG_H
#define LJ_EXPAND_LOG_H

#include <stdarg.h>

/* Standard ANSI terminal foreground colors used to paint log tags. */
typedef enum {
  LJE_COLOR_DEFAULT = 0, /* the terminal's default foreground color */
  LJE_COLOR_BLACK,
  LJE_COLOR_RED,
  LJE_COLOR_GREEN,
  LJE_COLOR_YELLOW,
  LJE_COLOR_BLUE,
  LJE_COLOR_MAGENTA,
  LJE_COLOR_CYAN,
  LJE_COLOR_WHITE,
  LJE_COLOR_GRAY, /* a.k.a. bright black */
  LJE_COLOR_BRIGHT_RED,
  LJE_COLOR_BRIGHT_GREEN,
  LJE_COLOR_BRIGHT_YELLOW,
  LJE_COLOR_BRIGHT_BLUE,
  LJE_COLOR_BRIGHT_MAGENTA,
  LJE_COLOR_BRIGHT_CYAN,
  LJE_COLOR_BRIGHT_WHITE,
  LJE_COLOR_COUNT
} LJELogColor;

typedef enum {
  LJE_LOG_DEBUG = 0,
  LJE_LOG_INFO,
  LJE_LOG_SUCCESS,
  LJE_LOG_WARN,
  LJE_LOG_ERROR,
  LJE_LOG_LEVEL_COUNT
} LJELogLevel;

/* Log a formatted line at the given level (a trailing newline is added). */
void lje_log(LJELogLevel level, const char* fmt, ...);
void lje_log_raw(const char* fmt, ...);
void lje_log_banner(void);

/* Suppress any log below the given level (default LJE_LOG_DEBUG, i.e. all). */
void lje_log_set_min_level(LJELogLevel level);

/* Convenience macros -- prefer these over calling lje_log directly. */
#define LJE_DEBUG(...)   lje_log(LJE_LOG_DEBUG, __VA_ARGS__)
#define LJE_INFO(...)    lje_log(LJE_LOG_INFO, __VA_ARGS__)
#define LJE_SUCCESS(...) lje_log(LJE_LOG_SUCCESS, __VA_ARGS__)
#define LJE_WARN(...)    lje_log(LJE_LOG_WARN, __VA_ARGS__)
#define LJE_ERROR(...)   lje_log(LJE_LOG_ERROR, __VA_ARGS__)

#endif /* LJ_EXPAND_LOG_H */
