# -*- coding: utf-8 -*-
"""Fiend Folio's own sounds that the monsters play (mod.Sounds.<Key> in their Lua) -> scripts/ff/ffsounds.h + the wavs list for the installer.
    python3 tools/ff_sounds.py --ff <FF mod> --lua <lua file> [--lua ...] --out scripts/ff/ffsounds.h --files <out>/files_sounds.txt
A sound record of the game holds at most 7 samples (ffsound.c), so a Fiend Folio sound with more is split into records of 7 and ffsound_play
picks one of the parts at random (same effect as the weighted random pick of the .xml's samples, all weights being 1 there).
Ids start after the nine of ffsound.c (833..841): 842...
"""
import argparse, re, os, sys
import xml.etree.ElementTree as ET
ap = argparse.ArgumentParser(); ap.add_argument('--ff', required=True); ap.add_argument('--lua', action='append', default=[]); ap.add_argument('--out', required=True); ap.add_argument('--files', required=True)
o = ap.parse_args()
root = ET.parse(os.path.join(o.ff, 'content-repentogon', 'sounds.xml')).getroot()
S = {s.get('name'): [x.get('path') for x in s.findall('sample')] for s in root.iter('sound')}
used = []
for f in o.lua:
    for m in re.finditer(r'(?:mod|FiendFolio)\.Sounds\.(\w+)', open(f, encoding='utf8', errors='ignore').read()):
        if m.group(1) not in used: used.append(m.group(1))
used.sort()
nid = 842; defs = []; recs = []; files = []; missing = []
for k in used:
    ps = S.get(k)
    if not ps: missing.append(k); continue
    parts = [ps[i:i + 7] for i in range(0, len(ps), 7)]
    ids = []
    for p in parts:
        ids.append(nid); recs.append((nid, p)); nid += 1
        for x in p:
            if os.path.exists(os.path.join(o.ff, 'resources', 'sounds', x)): files.append(x)
            else: print('wav missing in the mod:', x, file=sys.stderr)
    defs.append((k, ids))
with open(o.out, 'w') as f:
    f.write('/* Written by tools/ff_sounds.py - do not edit by hand. FFSND_<key> = the id to give ffsound_play; a sound with several parts (7 samples each) has FFSND_<key>_PARTS = n and the parts are consecutive ids. */\n')
    f.write('#define FF_NSOUNDS_EXTRA %d\n' % len(recs))
    for k, ids in defs:
        f.write('#define FFSND_%s %d\n' % (k, ids[0]))
        if len(ids) > 1: f.write('#define FFSND_%s_PARTS %d\n' % (k, len(ids)))
    f.write('static const struct { int id; const char *files[7]; } FF_SND_X[] = {\n')
    for i, p in recs: f.write('    { %d, { %s } },\n' % (i, ', '.join('"%s"' % x for x in p)))
    f.write('    { 0, { 0 } } };\n')
    f.write('/* parts of the multi-part sounds: first id, count (ffsound_play picks one at random) */\nstatic const struct { int first, n; } FF_SND_PARTS[] = {\n')
    for k, ids in defs:
        if len(ids) > 1: f.write('    { %d, %d },\n' % (ids[0], len(ids)))
    f.write('    { 0, 0 } };\n')
open(o.files, 'w').write('\n'.join(sorted(set(files))) + '\n')
print(len(defs), 'sounds,', len(recs), 'records,', len(set(files)), 'wavs; names the xml lacks:', missing)
