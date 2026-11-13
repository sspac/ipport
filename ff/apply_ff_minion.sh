#!/usr/bin/env bash
# For a tree where apply_ff.sh already ran (ffchars/ffheart/ffmenu are in): adds ONLY the minions. Idempotent.
#   bash ff/apply_ff_minion.sh      (from the repo root)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CPP=port/app/src/main/cpp
cp "$HERE/ffheart.c" "$CPP/ffheart.c"        # the hit hook and the minion collision block live here
cp "$HERE/ffminion.c" "$CPP/ffminion.c"
[ -f "$HERE/fffireball.h" ] && cp "$HERE/fffireball.h" "$CPP/fffireball.h"
[ -f "$HERE/ffenemy.h" ] && cp "$HERE/ffenemy.h" "$CPP/ffenemy.h"
[ -f "$HERE/ffanim_table.h" ] && cp "$HERE/ffanim_table.h" "$CPP/ffanim_table.h"
for h in ffenums.h ffoffsets.h ffsounds.h ffen_g1.h ffen_g2.h ffen_g3.h ffen_g4.h; do [ -f "$HERE/$h" ] && cp "$HERE/$h" "$CPP/$h"; done
cp "$HERE/ffsay.h" "$CPP/ffsay.h"
cp "$HERE/ffchars.c" "$CPP/ffchars.c"
python3 - <<'PY'
C = 'port/app/src/main/cpp/'
def rd(p): return open(p, encoding='utf-8', newline='').read()
def wr(p, s): open(p, 'w', encoding='utf-8', newline='').write(s)
cm = rd(C + 'CMakeLists.txt')
if 'ffminion.c' not in cm:
    if cm.count('ffheart.c') != 1: raise SystemExit("ffheart.c is not once in CMakeLists.txt (apply_ff.sh not run?)")
    wr(C + 'CMakeLists.txt', cm.replace('ffheart.c', 'ffheart.c ffminion.c'))
    print("CMakeLists: ffminion.c added")
ld = rd(C + 'loader.c')
if 'shim_ffminion_install' not in ld:
    key = '{ void shim_ffheart_install(unsigned char *); shim_ffheart_install(im.base); }\n'
    if ld.count(key) != 1: raise SystemExit("the ffheart install line is not once in loader.c (apply_ff.sh not run?)")
    wr(C + 'loader.c', ld.replace(key, key + '        /* Fiend Folio: the Fiend minions and their crits (ffminion.c) */\n'
        '        { void shim_ffminion_install(unsigned char *); shim_ffminion_install(im.base); }\n'))
    print("loader.c: shim_ffminion_install added")
print("ffminion applied")
PY
