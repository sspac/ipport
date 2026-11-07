#!/usr/bin/env bash
# Adds the Fiend Folio native modules to the port source (run from the repo root, after apply_yama.sh).
#   bash ff/apply_ff.sh
# ffchars.c  - the characters' stats, tear flags and colours (see the header of that file)
# ffheart.c  - Fiend's health rules (red dropped, containers and souls become black hearts), immoral hearts
# ffminion.c - the Fiend minions (immoral/black hearts that turn into minions when hit) and their critical hits
# ffmenu.c   - registers the character select / death screen art through the engine's own content loader (logs say if it runs)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CPP=port/app/src/main/cpp
cp "$HERE/ffchars.c" "$CPP/ffchars.c"
cp "$HERE/ffheart.c" "$CPP/ffheart.c"
cp "$HERE/ffsay.h" "$CPP/ffsay.h"
cp "$HERE/ffminion.c" "$CPP/ffminion.c"
cp "$HERE/ffmenu.c" "$CPP/ffmenu.c"
python3 - <<'PY'
def edit(path, old, new):
    s = open(path, encoding='utf-8', newline='').read()
    if s.count(old) != 1:
        raise SystemExit("expected text found %d times in %s: %r" % (s.count(old), path, old[:70]))
    open(path, 'w', encoding='utf-8', newline='').write(s.replace(old, new))
C = 'port/app/src/main/cpp/'
edit(C + 'CMakeLists.txt', ' rangefix.c ', ' rangefix.c ffchars.c ffheart.c ffminion.c ffmenu.c ')
edit(C + 'loader.c',
     '        { void shim_rangefix_install(unsigned char *); shim_rangefix_install(im.base); }\n',
     '        { void shim_rangefix_install(unsigned char *); shim_rangefix_install(im.base); }\n'
     '        /* Fiend Folio characters: their stats (ffchars.c) */\n'
     '        { void shim_ffchars_install(unsigned char *); shim_ffchars_install(im.base); }\n'
     '        /* Fiend Folio: Fiend\'s health rules (ffheart.c) */\n'
     '        { void shim_ffheart_install(unsigned char *); shim_ffheart_install(im.base); }\n'
     '        /* Fiend Folio: the Fiend minions and their crits (ffminion.c) */\n'
     '        { void shim_ffminion_install(unsigned char *); shim_ffminion_install(im.base); }\n'
     '        /* Fiend Folio: character select and death screen art (ffmenu.c) */\n'
     '        { void shim_ffmenu_install(unsigned char *); shim_ffmenu_install(im.base); }\n')
print("fiend folio modules applied")
PY
python3 "$HERE/ffwatch_patch.py" "$CPP/pack.c"
