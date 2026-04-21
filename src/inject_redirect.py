#!/usr/bin/env python3
"""
Injects `lje_redirect_state(L);` as the first statement of every LUA_API /
LUALIB_API function body whose first parameter is `lua_State *L`.

Idempotent: skips functions that already have the call.
Skips functions where the first param isn't `L` (e.g. luaL_addchar takes a Buffer*).
"""

import re
import sys
from pathlib import Path

REDIRECT_CALL = "lje_redirect_state(L);"

# Match: LUA_API or LUALIB_API, return type, function name, params, opening brace
# Captures: full signature, params, brace position
FUNC_RE = re.compile(
  r'(LUA_API|LUALIB_API)\s+'           # API marker
  r'([\w\s\*]+?)\s+'                    # return type
  r'(\w+)\s*'                           # function name
  r'\(([^)]*)\)\s*'                     # params
  r'\{',                                # opening brace
  re.MULTILINE
)

def first_param_is_lua_state_L(params: str) -> bool:
  """Check if the first param is `lua_State *L` (with any whitespace)."""
  first = params.split(',')[0].strip()
  # Normalize whitespace and check
  normalized = re.sub(r'\s+', ' ', first)
  return normalized in ('lua_State *L', 'lua_State* L', 'lua_State *L')

def patch_file(path: Path) -> int:
  text = path.read_text()
  original = text
  patches = 0

  # Walk matches in reverse so insertion offsets don't shift earlier matches
  matches = list(FUNC_RE.finditer(text))
  for m in reversed(matches):
    params = m.group(4)
    if not first_param_is_lua_state_L(params):
      continue

    brace_pos = m.end()  # position right after `{`

    # Find the next non-whitespace content after the brace
    rest = text[brace_pos:brace_pos + 200]
    if REDIRECT_CALL in rest.split('\n', 5)[0:5].__str__():
      # Already patched (rough check — refine if needed)
      continue
    # Better idempotency check: look at the next ~5 lines
    next_lines = text[brace_pos:brace_pos+500]
    if REDIRECT_CALL in next_lines[:300]:
      continue

    # Inject
    injection = f"\n  {REDIRECT_CALL}"
    text = text[:brace_pos] + injection + text[brace_pos:]
    patches += 1

  if text != original:
    path.write_text(text)
    print(f"  patched {path.name}: {patches} functions")
  return patches

def main():
  if len(sys.argv) < 2:
    print("Usage: inject_redirect.py <luajit_src_dir> [file2.c ...]")
    sys.exit(1)

  targets = []
  for arg in sys.argv[1:]:
    p = Path(arg)
    if p.is_dir():
      targets.extend(p.glob('*.c'))
    else:
      targets.append(p)

  total = 0
  for path in targets:
    total += patch_file(path)
  print(f"Done. {total} functions patched across {len(targets)} files.")

if __name__ == '__main__':
  main()