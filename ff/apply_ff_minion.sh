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
# every header the C code includes: the whole ffen_g*.h family by glob (g12/g13/g14 were missing from the old explicit list) plus the tables
for h in "$HERE"/ffen_g*.h "$HERE"/ffenums.h "$HERE"/ffoffsets.h "$HERE"/ffsounds.h "$HERE"/ffvending_table.h "$HERE"/ffenemies_table.h; do [ -f "$h" ] && cp "$h" "$CPP/$(basename "$h")"; done
cp "$HERE/ffsay.h" "$CPP/ffsay.h"
[ -f "$HERE/ffsound.c" ] && cp "$HERE/ffsound.c" "$CPP/ffsound.c"      # its SOUND / SOUND_MISS lines
python3 "$HERE/ffnoise_patch.py" "$CPP/loader.c"       # idempotent: the profiler's who/where lines and the haptics lines stay out of the report
python3 "$HERE/ffdraw_patch.py" "$CPP/trace.c"       # idempotent: the Glass Chest preview hook
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
