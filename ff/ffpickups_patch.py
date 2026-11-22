#!/usr/bin/env python3
"""Fiend Folio's pickups in the console's ITEMS tab (run from the repo root by apply_ff.sh).

The game's table (spawner_pickups.h, written by tools/spawner_pickups.py) gets FF's rows appended (ffpickups_table.h, written by
tools/spawner_ff_pickups.py): immoral hearts, the FF pennies, spicy keys, puzzle keys, copper bombs and the FF batteries. Each row
carries the section of the game's own pickup, so the list shows it under HEARTS / COINS / KEYS / BOMBS / BATTERIES after the game's rows; they
spawn like any pickup (Game::Spawn, then the player takes it: ffheart.c w_collide). spawner.c learns one thing: a row from FF_PICKUP_FIRST
on draws its icon from gfx/port_ff_pickup_icons.anm2 ("Icons" frame = row - FF_PICKUP_FIRST).
Every edit demands its text exactly once; a second run does nothing.
"""
import sys
C = sys.argv[1].rstrip('/') + '/'
def rd(p): return open(p, encoding='utf-8', newline='').read()
def wr(p, s): open(p, 'w', encoding='utf-8', newline='').write(s)
def once(s, old, new, where):
    if s.count(old) != 1: raise SystemExit('ffpickups_patch: %r found %d times in %s' % (old[:60], s.count(old), where))
    return s.replace(old, new)

h = rd(C + 'spawner_pickups.h')
if 'FF_PICKUP_ROWS' in h:
    print('pickups: spawner_pickups.h already has the FF rows')
else:
    w = 'spawner_pickups.h'
    h = once(h, '#define SP_PICKUPS 47\n', '#include "ffpickups_table.h"\n#define FF_PICKUP_FIRST 47\n#define SP_PICKUPS (47 + FF_PICKUP_ROWS)\n', w)
    i = h.index('} SP_PICKUP[SP_PICKUPS] = {'); j = h.index('\n};', i)
    h = h[:j + 1] + '#define FF_PICKUP_BODY\n#include "ffpickups_table.h"\n#undef FF_PICKUP_BODY' + h[j:]
    wr(C + 'spawner_pickups.h', h)
    print('pickups: FF rows added to spawner_pickups.h')

s = rd(C + 'spawner.c')
if 'g_ffp_art' in s:
    print('pickups: spawner.c already patched')
else:
    w = 'spawner.c'
    s = once(s, 'static unsigned char g_pickup_art[SPRITE_BYTES] __attribute__((aligned(16)));   /* port_pickup_icons */\n',
             'static unsigned char g_pickup_art[SPRITE_BYTES] __attribute__((aligned(16)));   /* port_pickup_icons */\n'
             'static unsigned char g_ffp_art[SPRITE_BYTES] __attribute__((aligned(16)));      /* Fiend Folio\'s pickup icons */\n'
             'static signed char g_ffp_state;\n', w)
    s = once(s, '        case KIND_PICKUP:\n            if (icons && r->id < SP_PICKUPS)\n'
                '                shim_picture_draw(g_pickup_art, "Icons", r->id, floorf(cx - (float) SP_PICKUP[r->id].w * 0.5f),\n'
                '                                  floorf(row + (h - (float) SP_PICKUP[r->id].h) * 0.5f), alpha);\n            break;\n',
             '        case KIND_PICKUP:\n            if (r->id >= FF_PICKUP_FIRST && r->id < SP_PICKUPS) {          /* Fiend Folio\'s: its own atlas */\n'
             '                if (shim_picture_ready(g_ffp_art, &g_ffp_state, "gfx/port_ff_pickup_icons.anm2"))\n'
             '                    shim_picture_draw(g_ffp_art, "Icons", r->id - FF_PICKUP_FIRST, floorf(cx - (float) SP_PICKUP[r->id].w * 0.5f),\n'
             '                                      floorf(row + (h - (float) SP_PICKUP[r->id].h) * 0.5f), alpha);\n'
             '            } else if (icons && r->id < SP_PICKUPS)\n'
             '                shim_picture_draw(g_pickup_art, "Icons", r->id, floorf(cx - (float) SP_PICKUP[r->id].w * 0.5f),\n'
             '                                  floorf(row + (h - (float) SP_PICKUP[r->id].h) * 0.5f), alpha);\n            break;\n', w)
    wr(C + 'spawner.c', s)
    print('pickups: spawner.c patched')
