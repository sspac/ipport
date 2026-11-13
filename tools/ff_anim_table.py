# -*- coding: utf-8 -*-
"""Animation data of Fiend Folio's monsters for scripts/ff/ffenemy.h.

The game's Sprite is not read for its frame (not measured); the AI keeps its own frame counter per animation, so it needs
each animation's length, its Loop flag and the frames its Triggers fire at (the Lua's sprite:IsEventTriggered("Shoot")).
Those are in the .anm2, so they are written down here, once:

    python3 tools/ff_anim_table.py --ff <FF mod folder> --rows scripts/ff/ffenemies_table.h --out scripts/ff/ffanim_table.h [--extra 160:2101:0,...]

Key of an entity = (type << 20) | (variant << 4) | subtype (subtype < 16).
"""
import argparse, os, re, sys
import xml.etree.ElementTree as ET


def ents(ff):
    out = {}
    for e in ET.parse(os.path.join(ff, 'content-repentogon', 'entities2.xml')).getroot().findall('entity'):
        a = dict(e.attrib)
        try: out[(int(a['id']), int(a.get('variant', 0)), int(a.get('subtype', 0)))] = a
        except (KeyError, ValueError): pass
    return out


def find_anm(ff, rel):
    rel = rel.replace('\\', '/')
    for root in ('resources/gfx', 'resources/gfx/monsters', 'resources/gfx/enemies'):
        p = os.path.join(ff, root, rel)
        if os.path.exists(p): return p
    base = os.path.basename(rel)
    for dp, _, fs in os.walk(os.path.join(ff, 'resources')):
        for f in fs:
            if f.lower() == base.lower() and rel.split('/')[0] in dp: return os.path.join(dp, f)
    for dp, _, fs in os.walk(os.path.join(ff, 'resources')):
        for f in fs:
            if f.lower() == base.lower(): return os.path.join(dp, f)
    return None


def parse(path):
    r = ET.parse(path).getroot()
    names = {int(e.get('Id')): e.get('Name') for e in r.iter('Event')}
    anims = []
    for a in r.iter('Animation'):
        evs = []
        for t in a.iter('Trigger'):
            evs.append((names.get(int(t.get('EventId')), '?'), int(t.get('AtFrame'))))
        anims.append((a.get('Name'), int(a.get('FrameNum', 1)), 1 if a.get('Loop', 'true').lower() == 'true' else 0, evs))
    return anims


def keys_from_rows(path):
    out = []
    for m in re.finditer(r'\{\s*(\d+),\s*(\d+),\s*(\d+),\s*SP_GROUP_FF', open(path).read()):
        out.append(tuple(int(x) for x in m.groups()))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ff', required=True); ap.add_argument('--rows'); ap.add_argument('--out', required=True); ap.add_argument('--extra', default='')
    o = ap.parse_args()
    E = ents(o.ff); keys = keys_from_rows(o.rows) if o.rows else []
    for t in o.extra.split(','):
        if t.strip(): keys.append(tuple(int(x) for x in t.split(':')))
    seen = []; [seen.append(k) for k in keys if k not in seen]
    rows, evs, lines = [], [], []
    for k in seen:
        a = E.get(k)
        if not a: print('no entity', k, file=sys.stderr); continue
        p = find_anm(o.ff, a['anm2path'])
        if not p: print('no anm2', k, a['anm2path'], file=sys.stderr); continue
        key = (k[0] << 20) | (k[1] << 4) | k[2]
        for (n, ln, lp, ev) in parse(p):
            rows.append((key, n, ln, lp, len(evs), len(ev))); evs.extend(ev)
    with open(o.out, 'w') as f:
        f.write('/* Written by tools/ff_anim_table.py - do not edit by hand. */\n')
        f.write('typedef struct { unsigned key; const char *name; short len; char loop; short ev0, evn; } ffa_t;\ntypedef struct { const char *name; short frame; } ffe_t;\n')
        f.write('static const ffe_t ffe_tab[] = {\n' + ''.join('  { "%s", %d },\n' % e for e in evs) + '  { 0, 0 } };\n')
        f.write('static const ffa_t ffa_tab[] = {\n' + ''.join('  { %du, "%s", %d, %d, %d, %d },\n' % r for r in rows) + '  { 0, 0, 0, 0, 0, 0 } };\n')
    print('animations', len(rows), 'events', len(evs))

main()
