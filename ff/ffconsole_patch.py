#!/usr/bin/env python3
"""Fiend Folio's monsters in the console's ENEMIES tab (run from the repo root by apply_ff.sh).

The game's table (spawner_enemies.h, generated from the game's bestiary) gets FF's rows appended (ffenemies_table.h, written by
tools/spawner_ff_enemies.py) under a heading of their own, and spawner.c learns three things: a row whose icon is >= 10000 draws
that frame of gfx/port_ff_enemy_icons.anm2; the group SP_GROUP_FF has a text heading; and it spawns like a monster.
Every edit demands its text exactly once (a changed spawner.c stops the build here, not in the game); a second run does nothing.
"""
import re, sys
C = sys.argv[1].rstrip('/') + '/'
def rd(p): return open(p, encoding='utf-8', newline='').read()
def wr(p, s): open(p, 'w', encoding='utf-8', newline='').write(s)
def once(s, old, new, where):
    if s.count(old) != 1: raise SystemExit('ffconsole_patch: %r found %d times in %s' % (old[:60], s.count(old), where))
    return s.replace(old, new)

h = rd(C + 'spawner_enemies.h')
if 'FF_ENEMY_ROWS' in h:
    print('console: spawner_enemies.h already has the FF rows')
else:
    m = re.search(r'#define SP_ENEMIES (\d+)', h)
    if not m: raise SystemExit('ffconsole_patch: no SP_ENEMIES in spawner_enemies.h')
    h = h.replace(m.group(0), '#define SP_GROUP_FF 4\n#include "ffenemies_table.h"\n#define SP_ENEMIES (%s + FF_ENEMY_ROWS)' % m.group(1))
    i = h.index('} SP_ENEMY[SP_ENEMIES] = {'); j = h.index('\n};', i)
    h = h[:j + 1] + '#define FF_ENEMY_BODY\n#include "ffenemies_table.h"\n#undef FF_ENEMY_BODY' + h[j:]
    wr(C + 'spawner_enemies.h', h)
    print('console: FF rows added to spawner_enemies.h')

s = rd(C + 'spawner.c')
if 'g_ff_art' in s:
    print('console: spawner.c already patched')
else:
    w = 'spawner.c'
    for old in ('static float g_pos_enemies[600 + 1];', 'g_pos_enemies, 600 }', 'static short g_enemy_row[600];', 'static short g_enemy_sub[600];',
                'static float g_enemy_real[600];', 'static float g_enemy_h[600];', 'static short g_base_row[600];'):
        s = once(s, old, old.replace('600', '1024'), w)                       # room for the FF rows and an open sub-list
    s = once(s, 'static signed char g_enemy_state;\n', 'static signed char g_enemy_state;\nstatic unsigned char g_ff_art[SPRITE_BYTES] __attribute__((aligned(16)));   /* Fiend Folio\'s icons */\nstatic signed char g_ff_state;\n', w)
    s = once(s, 'SP_ENEMY[id].group != SP_GROUP_MONSTER && SP_ENEMY[id].group != SP_GROUP_TRAINING;',
             'SP_ENEMY[id].group != SP_GROUP_MONSTER && SP_ENEMY[id].group != SP_GROUP_TRAINING\n                    && SP_ENEMY[id].group != SP_GROUP_FF;', w)
    s = once(s, '(float) SP_HEAD[set][-1 - e].h + HEAD_AIR;', '((-1 - e == SP_GROUP_FF) ? 14.0f : (float) SP_HEAD[set][-1 - e].h) + HEAD_AIR;', w)
    s = once(s, '            int g = -1 - e;\n            if (!art) continue;\n',
             '            int g = -1 - e;\n            if (g == SP_GROUP_FF) { write_text("FIEND FOLIO", x, row + 2.0f, row_colour(k, 0), alpha); continue; }\n            if (!art) continue;\n', w)
    s = once(s, '        if (icons && SP_ENEMY[e].icon >= 0)\n            shim_picture_draw(g_enemy_art, "Icons", SP_ENEMY[e].icon,\n'
                '                              x + floorf((ENEMY_COLUMN - (float) SP_ENEMY[e].w) * 0.5f),\n'
                '                              floorf(row + (h - (float) SP_ENEMY[e].h) * 0.5f), alpha);\n',
             '        if (SP_ENEMY[e].icon >= 10000) {                          /* Fiend Folio\'s: its own atlas */\n'
             '            if (shim_picture_ready(g_ff_art, &g_ff_state, "gfx/port_ff_enemy_icons.anm2"))\n'
             '                shim_picture_draw(g_ff_art, "Icons", SP_ENEMY[e].icon - 10000,\n'
             '                                  x + floorf((ENEMY_COLUMN - (float) SP_ENEMY[e].w) * 0.5f),\n'
             '                                  floorf(row + (h - (float) SP_ENEMY[e].h) * 0.5f), alpha);\n'
             '        } else if (icons && SP_ENEMY[e].icon >= 0)\n'
             '            shim_picture_draw(g_enemy_art, "Icons", SP_ENEMY[e].icon,\n'
             '                              x + floorf((ENEMY_COLUMN - (float) SP_ENEMY[e].w) * 0.5f),\n'
             '                              floorf(row + (h - (float) SP_ENEMY[e].h) * 0.5f), alpha);\n', w)
    wr(C + 'spawner.c', s)
    print('console: spawner.c patched')
