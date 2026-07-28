#include "lj_expand_log.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  const char* tag;
  LJELogColor color;
} LJELogLevelConfig;

static LJELogLevelConfig lje_log_config[LJE_LOG_LEVEL_COUNT] = {
  /* LJE_LOG_DEBUG   */ { "DEBUG", LJE_COLOR_GRAY },
  /* LJE_LOG_INFO    */ { "INFO",  LJE_COLOR_GREEN },
  /* LJE_LOG_SUCCESS */ { "OK",    LJE_COLOR_BRIGHT_GREEN },
  /* LJE_LOG_WARN    */ { "WARN",  LJE_COLOR_YELLOW },
  /* LJE_LOG_ERROR   */ { "ERROR", LJE_COLOR_BRIGHT_RED },
};

static const char* lje_log_prefix = "LJE";
static LJELogLevel lje_log_min_level = LJE_LOG_INFO;

/*
** Whether ANSI color escapes are emitted.
**   -1 = undecided (auto-detect on first use)
**    0 = disabled
**    1 = enabled
*/
static int lje_log_colors = -1;

/* SGR foreground codes indexed by LJELogColor. */
static const int lje_ansi_codes[LJE_COLOR_COUNT] = {
  39, /* DEFAULT */
  30, 31, 32, 33, 34, 35, 36, 37, /* BLACK .. WHITE */
  90, 91, 92, 93, 94, 95, 96, 97  /* GRAY .. BRIGHT_WHITE */
};

static void lje_log_autodetect_colors(void)
{
  if (lje_log_colors != -1)
    return; /* already decided, or explicitly overridden */

  /* Console setup (lje_plat_console_init) already enabled VT processing
   * on Windows; on POSIX terminals ANSI is the standard. */
  lje_log_colors = 1;
}

static void lje_log_emit(LJELogLevel level, const char* fmt, va_list args)
{
  const LJELogLevelConfig* cfg;

  if (level < lje_log_min_level || level < 0 || level >= LJE_LOG_LEVEL_COUNT)
    return;

  lje_log_autodetect_colors();
  cfg = &lje_log_config[level];

  /* Print the colorized "[LJE TAG] " prefix. */
  if (lje_log_colors == 1)
    printf("\x1b[%dm", lje_ansi_codes[cfg->color]);

  if (lje_log_prefix && lje_log_prefix[0] != '\0')
    printf("[%s %s]", lje_log_prefix, cfg->tag);
  else
    printf("[%s]", cfg->tag);

  if (lje_log_colors == 1)
    printf("\x1b[0m"); /* reset before the message so it stays uncolored */

  printf(" ");

  vprintf(fmt, args);
  printf("\n");
}

void lje_log(LJELogLevel level, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  lje_log_emit(level, fmt, args);
  va_end(args);
}

void lje_log_raw(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

void lje_log_banner(void)
{
  static const char* const banner[] = {
    "  _          _ ______ ",
    " | |        | |  ____|",
    " | |        | | |__   ",
    " | |    _   | |  __|  ",
    " | |___| |__| | |____ ",
    " |______\\____/|______|",
  };
  size_t i;

  lje_log_autodetect_colors();
  for (i = 0; i < sizeof(banner) / sizeof(banner[0]); i++) {
    if (lje_log_colors == 1)
      printf("\x1b[%dm%s\x1b[0m\n", lje_ansi_codes[LJE_COLOR_BRIGHT_GREEN], banner[i]);
    else
      printf("%s\n", banner[i]);
  }
}

void lje_log_set_min_level(LJELogLevel level)
{
  if (level < 0 || level >= LJE_LOG_LEVEL_COUNT)
    return;
  lje_log_min_level = level;
}
