#ifndef LJ_EXPAND_LOG_H
#define LJ_EXPAND_LOG_H

/*
** LJE logging system.
**
** Prints colorized, tagged log lines to the console, for example:
**
**     [LJE INFO]  Loaded binary module: foo.dll
**     [LJE WARN]  Something looks off...
**     [LJE ERROR] Failed to do the thing
**
** The bracketed tag is colorized per log level. Both the tag text and its
** color are fully customizable -- see the "Customization" section below, or
** just edit the lje_log_config table at the top of lj_expand_log.c.
**
** Typical usage is via the convenience macros:
**
**     LJE_INFO("Loaded %d scripts", count);
**     LJE_ERROR("Failed to open '%s'", path);
*/

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

/* Log severity levels. Each has its own customizable tag text and color. */
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
/* Log a formatted message without the leading tag or a trailing newline.
** Useful for building up a single line from multiple calls. */
void lje_log_raw(const char* fmt, ...);
/* Print the LJE ASCII-art logo (colorized when colors are enabled). */
void lje_log_banner(void);

/* ---- Customization ------------------------------------------------------ */

/* Change the color used to paint a level's tag. */
void lje_log_set_color(LJELogLevel level, LJELogColor color);
/* Change the text shown inside a level's tag (e.g. "WARN" -> "WARNING"). */
void lje_log_set_tag(LJELogLevel level, const char* tag);
/* Change the brand prefix shown before every tag (default "LJE"). Pass an
** empty string to drop the prefix entirely (tags become just "[INFO]"). */
void lje_log_set_prefix(const char* prefix);
/* Force ANSI colors on or off. Colors are auto-enabled when the console
** supports them, so this is only needed to override that decision. */
void lje_log_set_colors_enabled(int enabled);
/* Suppress any log below the given level (default LJE_LOG_DEBUG, i.e. all). */
void lje_log_set_min_level(LJELogLevel level);

/* Convenience macros -- prefer these over calling lje_log directly. */
#define LJE_DEBUG(...)   lje_log(LJE_LOG_DEBUG, __VA_ARGS__)
#define LJE_INFO(...)    lje_log(LJE_LOG_INFO, __VA_ARGS__)
#define LJE_SUCCESS(...) lje_log(LJE_LOG_SUCCESS, __VA_ARGS__)
#define LJE_WARN(...)    lje_log(LJE_LOG_WARN, __VA_ARGS__)
#define LJE_ERROR(...)   lje_log(LJE_LOG_ERROR, __VA_ARGS__)

#endif /* LJ_EXPAND_LOG_H */
