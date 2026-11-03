#!/usr/bin/env bash
# Adds the Fiend Folio native modules to the port source (run from the repo root, after apply_yama.sh).
#   bash ff/apply_ff.sh
# Stage 1a: ffchars.c - the 20 characters' stats (see the header of that file).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CPP=port/app/src/main/cpp
cp "$HERE/ffchars.c" "$CPP/ffchars.c"
python3 - <<'PY'
def edit(path, old, new):
    s = open(path, encoding='utf-8', newline='').read()
    if s.count(old) != 1:
        raise SystemExit("expected text found %d times in %s: %r" % (s.count(old), path, old[:70]))
    open(path, 'w', encoding='utf-8', newline='').write(s.replace(old, new))
C = 'port/app/src/main/cpp/'
edit(C + 'CMakeLists.txt', ' rangefix.c ', ' rangefix.c ffchars.c ')
edit(C + 'loader.c',
     '        { void shim_rangefix_install(unsigned char *); shim_rangefix_install(im.base); }\n',
     '        { void shim_rangefix_install(unsigned char *); shim_rangefix_install(im.base); }\n'
     '        /* Fiend Folio characters: their stats (ffchars.c) */\n'
     '        { void shim_ffchars_install(unsigned char *); shim_ffchars_install(im.base); }\n')
print("fiend folio modules applied")
PY
