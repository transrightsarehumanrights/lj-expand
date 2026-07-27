/* settings overrides in %USERPROFILE%\.lje\settings\<author>.<name>.toml. */

#include "lj_expand_settings.h"

#include "lauxlib.h"
#include "lj_expand_dirs.h"
#include "lj_expand_globals.h"
#include "lj_expand_log.h"
#include "lj_expand_platform.h"
#include "lj_expand_script.h"
#include "tomlc17.h"

#include <stdio.h>
#include <string.h>

#define LJE_SETTINGS_CACHE "__lje_settings_cache"
#define LJE_SETTINGS_MT "__lje_settings_obj"

// Normalizes the slug entry
static size_t append_slug(char* out, size_t size, size_t pos, const char* s, int* changed)
{
    for (; *s && pos + 1 < size; s++)
    {
        char c = *s;
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');

        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
        {
            out[pos++] = c;
        }
        else
        {
            out[pos++] = '-';
            *changed = 1;
        }
    }
    return pos;
}

static int resolve_default_path(LJEScript* s, char* out, size_t size)
{
    if (!s || !s->folder)
        return 0;
    return lje_path_join(out, size, s->folder, LJE_SCRIPT_SETTINGS_DEFAULT);
}

static int resolve_override_path(LJEScript* s, char* out, size_t size)
{
    if (!s || !s->info)
        return 0;

    char dir[LJE_PATH_MAX];
    if (!lje_directory_get(LJE_DIR_SETTINGS, dir, sizeof(dir)))
        return 0;

    char slug[256];
    int changed = 0;
    size_t pos = append_slug(slug, sizeof(slug), 0, s->info->author, &changed);
    if (pos + 1 < sizeof(slug))
        slug[pos++] = '.';
    pos = append_slug(slug, sizeof(slug), pos, s->info->name, &changed);
    slug[pos] = '\0';

    if (changed)
        LJE_WARN("settings: sanitized identity for script '%s' to slug '%s'", s->name, slug);

    if (!lje_path_join(out, size, dir, slug))
        return 0;
    lje_strlcat(out, ".toml", size);
    return 1;
}

static void ensure_override_template(const char* path, LJEScript* s)
{
    if (lje_plat_fs_kind(path) != LJE_FS_MISSING)
        return;

    FILE* f = fopen(path, "w");
    if (!f)
        return;

    fprintf(f,
        "# Settings overrides for %s by %s\n"
        "# Add keys below to override the script defaults (see %s in the script folder).\n"
        "# Anything you don't set falls back to those defaults.\n",
        s->info->name, s->info->author, LJE_SCRIPT_SETTINGS_DEFAULT);
    fclose(f);
}

static void push_toml_value(lua_State* L, toml_datum_t d)
{
    switch (d.type)
    {
        case TOML_STRING:
            lua_pushlstring(L, d.u.str.ptr, d.u.str.len);
            break;
        case TOML_INT64:
            lua_pushinteger(L, (lua_Integer)d.u.int64);
            break;
        case TOML_FP64:
            lua_pushnumber(L, d.u.fp64);
            break;
        case TOML_BOOLEAN:
            lua_pushboolean(L, d.u.boolean);
            break;
        case TOML_ARRAY:
            lua_createtable(L, d.u.arr.size, 0);
            for (int i = 0; i < d.u.arr.size; i++)
            {
                push_toml_value(L, d.u.arr.elem[i]);
                lua_rawseti(L, -2, i + 1);
            }
            break;
        case TOML_TABLE:
            lua_createtable(L, 0, d.u.tab.size);
            for (int i = 0; i < d.u.tab.size; i++)
            {
                lua_pushlstring(L, d.u.tab.key[i], d.u.tab.len[i]);
                push_toml_value(L, d.u.tab.value[i]);
                lua_rawset(L, -3);
            }
            break;
        case TOML_DATE:
        case TOML_TIME:
        case TOML_DATETIME:
        case TOML_DATETIMETZ:
        {
            char b[40];
            snprintf(b, sizeof(b), "%04d-%02d-%02dT%02d:%02d:%02d",
                d.u.ts.year, d.u.ts.month, d.u.ts.day,
                d.u.ts.hour, d.u.ts.minute, d.u.ts.second);
            lua_pushstring(L, b);
            break;
        }
        default:
            lua_pushnil(L);
            break;
    }
}

static void push_merged(lua_State* L, LJEScript* script)
{
    char def_path[LJE_PATH_MAX];
    char ovr_path[LJE_PATH_MAX];
    int have_def = resolve_default_path(script, def_path, sizeof(def_path))
        && lje_plat_fs_kind(def_path) != LJE_FS_MISSING;
    int have_ovr = resolve_override_path(script, ovr_path, sizeof(ovr_path));
    if (have_ovr)
    {
        ensure_override_template(ovr_path, script);
        have_ovr = lje_plat_fs_kind(ovr_path) != LJE_FS_MISSING;
    }

    toml_result_t rd, ro;
    int ok_d = 0, ok_o = 0;
    if (have_def)
    {
        rd = toml_parse_file_ex(def_path);
        ok_d = rd.ok;
        if (!ok_d)
            LJE_WARN("Settings: failed to parse %s: %s", def_path, rd.errmsg);
    }
    if (have_ovr)
    {
        ro = toml_parse_file_ex(ovr_path);
        ok_o = ro.ok;
        if (!ok_o)
            LJE_WARN("Settings: failed to parse %s: %s", ovr_path, ro.errmsg);
    }

    /* push merged value */
    if (ok_d && ok_o)
    {
        toml_result_t merged = toml_merge(&rd, &ro);
        push_toml_value(L, merged.toptab);
        toml_free(merged);
    }
    else if (ok_d)
    {
        push_toml_value(L, rd.toptab);
    }
    else if (ok_o)
    {
        push_toml_value(L, ro.toptab);
    }
    else
    {
        lua_newtable(L);
    }

    if (have_def)
        toml_free(rd);
    if (have_ovr)
        toml_free(ro);
}

static void push_cache(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, LJE_SETTINGS_CACHE);
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, LJE_SETTINGS_CACHE);
    }
}

static LJEScript* resolve_by_name(const char* name)
{
    for (size_t i = 0; i < LJEG()->loaded_script_count; i++)
    {
        if (strcmp(LJEG()->loaded_scripts[i].name, name) == 0)
            return &LJEG()->loaded_scripts[i];
    }
    return NULL;
}

static LJEScript* current_or_warn(void)
{
    LJEScript* s = LJEG()->current_script;
    if (!s)
        LJE_WARN("settings: no current script context; use lje.settings.open() for deferred calls (hooks/render/timers)");
    return s;
}

static void push_all_for(lua_State* L, LJEScript* s)
{
    if (!s)
    {
        lua_newtable(L);
        return;
    }

    push_cache(L);
    lua_getfield(L, -1, s->name);
    if (lua_istable(L, -1))
    {
        lua_remove(L, -2);
        return;
    }
    lua_pop(L, 1);

    push_merged(L, s);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, s->name);
    lua_remove(L, -2);
}

static int push_get_for(lua_State* L, LJEScript* s, int key_idx, int default_idx)
{
    const char* key = luaL_checkstring(L, key_idx);
    push_all_for(L, s);

    /* Walk a dotted key (e.g. "blah.foo") down through nested tables. */
    for (const char* seg = key;;)
    {
        const char* dot = strchr(seg, '.');
        size_t len = dot ? (size_t)(dot - seg) : strlen(seg);

        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_pushnil(L);
            break;
        }

        lua_pushlstring(L, seg, len);
        lua_rawget(L, -2);
        lua_remove(L, -2);

        if (!dot)
            break;
        seg = dot + 1;
    }

    if (lua_isnil(L, -1) && !lua_isnoneornil(L, default_idx))
    {
        lua_pop(L, 1);
        lua_pushvalue(L, default_idx);
    }
    return 1;
}

static void reload_for(lua_State* L, LJEScript* s)
{
    if (!s)
        return;

    push_cache(L);
    lua_pushnil(L);
    lua_setfield(L, -2, s->name);
    lua_pop(L, 1);
}

int lje_settings_all(lua_State* L)
{
    push_all_for(L, current_or_warn());
    return 1;
}

int lje_settings_get(lua_State* L)
{
    return push_get_for(L, current_or_warn(), 1, 2);
}

int lje_settings_reload(lua_State* L)
{
    reload_for(L, current_or_warn());
    return 0;
}

static const char* check_obj(lua_State* L)
{
    return (const char*)luaL_checkudata(L, 1, LJE_SETTINGS_MT);
}

static int obj_all(lua_State* L)
{
    push_all_for(L, resolve_by_name(check_obj(L)));
    return 1;
}

static int obj_get(lua_State* L)
{
    return push_get_for(L, resolve_by_name(check_obj(L)), 2, 3);
}

static int obj_reload(lua_State* L)
{
    reload_for(L, resolve_by_name(check_obj(L)));
    return 0;
}

int lje_settings_bind(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    size_t len = strlen(name);
    void* ud = lua_newuserdata(L, len + 1);
    memcpy(ud, name, len + 1);

    if (luaL_newmetatable(L, LJE_SETTINGS_MT))
    {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, obj_get);
        lua_setfield(L, -2, "get");
        lua_pushcfunction(L, obj_all);
        lua_setfield(L, -2, "all");
        lua_pushcfunction(L, obj_reload);
        lua_setfield(L, -2, "reload");
    }
    lua_setmetatable(L, -2);
    return 1;
}

int lje_script_info(lua_State* L)
{
    LJEScript* s = LJEG()->current_script;
    if (!s || !s->info)
    {
        lua_pushnil(L);
        return 1;
    }

    LJEScriptInfo* in = s->info;
    lua_createtable(L, 0, 4);
    lua_pushstring(L, in->name);
    lua_setfield(L, -2, "name");
    lua_pushstring(L, in->version);
    lua_setfield(L, -2, "version");
    lua_pushstring(L, in->author);
    lua_setfield(L, -2, "author");

    lua_createtable(L, (int)in->dependency_count, 0);
    for (size_t i = 0; i < in->dependency_count; i++)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s.%s", in->dependencies[i].author, in->dependencies[i].name);
        lua_pushstring(L, buf);
        lua_rawseti(L, -2, (int)i + 1);
    }
    lua_setfield(L, -2, "dependencies");
    return 1;
}

void lje_settings_clear_cache(lua_State* L)
{
  lua_pushstring(L, LJE_SETTINGS_CACHE);
  lua_pushnil(L);
  lua_rawset(L, LUA_REGISTRYINDEX);
}
