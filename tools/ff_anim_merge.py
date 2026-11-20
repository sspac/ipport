# -*- coding: utf-8 -*-
"""Adds to scripts/ff/ffanim_table.h the animation rows of keys it does not have yet (from a freshly generated table), keeping every hand-kept row.
    python3 tools/ff_anim_merge.py scripts/ff/ffanim_table.h new_table.h"""
import re, sys
def load(p):
    t = open(p).read()
    evs = re.findall(r'^  \{ "([^"]*)", (\d+) \},', t.split('ffa_tab')[0], re.M)
    rows = [(int(a), b, int(c), int(d), int(e), int(f)) for a, b, c, d, e, f in re.findall(r'^  \{ (\d+)u, "([^"]*)", (\d+), (\d+), (\d+), (\d+) \},', t, re.M)]
    return evs, rows
oe, orw = load(sys.argv[1]); ne, nrw = load(sys.argv[2])
have = {r[0] for r in orw}; added = 0
for r in nrw:
    if r[0] in have: continue
    ev0 = len(oe); oe.extend(ne[r[4]:r[4] + r[5]]); orw.append((r[0], r[1], r[2], r[3], ev0, r[5])); added += 1
with open(sys.argv[1], 'w') as f:
    f.write('/* Written by tools/ff_anim_table.py (+ tools/ff_anim_merge.py) - do not edit by hand. */\n')
    f.write('typedef struct { unsigned key; const char *name; short len; char loop; short ev0, evn; } ffa_t;\ntypedef struct { const char *name; short frame; } ffe_t;\n')
    f.write('static const ffe_t ffe_tab[] = {\n' + ''.join('  { "%s", %s },\n' % e for e in oe) + '  { 0, 0 } };\n')
    f.write('static const ffa_t ffa_tab[] = {\n' + ''.join('  { %du, "%s", %d, %d, %d, %d },\n' % r for r in orw) + '  { 0, 0, 0, 0, 0, 0 } };\n')
print('added rows', added)
