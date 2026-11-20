# -*- coding: utf-8 -*-
"""Fiend Folio's rooms for the port: which of the mod's rooms the game can run as they are, and the STB1 files with only those.

    python3 tools/ff_rooms.py --ff <FF mod folder> --game-ref <repplus folder with entities2.xml> --ported <entities2 of the ported FF monsters>
                              [--out <folder>] [--files "01.basement,02.cellar,03.burning basement,00.special rooms"]

A spawn is kept when the game already has what it asks for:
  - nothing (type 0), a grid entity (type >= 1000, the game's own), an entity the game's entities2.xml knows (type + variant; a pickup only
    with a subtype the game has),
  - an FF monster the port has (the --ported table).
A spawn of the mod's STAGE METADATA is not a thing that stands in the room: type 150 variant 1000 is a backdrop marker (Lua/StageAPI picks the
backdrop from it) - it is dropped and counted ("markers"), the room keeps its vanilla backdrop until the backdrops are hooked.
Everything else (FF placeholders 183/184 = the mod's custom grids, FF pickups, FF items, FF monsters of other floors, boss entities...) makes the room
"not ready": it stays out of the output file. Rooms of the game itself (those the mod copied) pass the same test.
"""
import argparse, collections, os, sys, xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stb

def entities(path):
    out = set()
    for e in ET.parse(path).getroot().iter('entity'):
        try: out.add((int(e.attrib['id']), int(e.attrib.get('variant', 0)), int(e.attrib.get('subtype', 0))))
        except (KeyError, ValueError): pass
    return out

class Judge:
    def __init__(self, game, ported):
        self.game = game; self.ported = ported
        self.tv = {(t, v) for t, v, s in game}; self.ptv = {(t, v) for t, v, s in ported}
        self.maxsub = collections.defaultdict(int)
        for t, v, s in game:
            if t == 5: self.maxsub[v] = max(self.maxsub[v], s)
    def kind(self, t, v, s):
        if t == 0: return 'empty'
        if t >= 1000: return 'grid'
        if t == 150 and v == 1000: return 'marker'
        if t == 5 and v == 0: return 'vanilla'                 # 5.0: a random pickup
        if t == 5:
            if (5, v) in self.tv and (s == 0 or s <= self.maxsub[v] and (5, v, s) in self.game or (v in (100, 300, 350) and s <= self.maxsub[v])): return 'vanilla'
            return 'ff_pickup'
        if (t, v) in self.ptv and (t, v, s) in self.ported: return 'ported'
        if (t, v) in self.tv: return 'vanilla'
        if (t, v) in self.ptv: return 'ported'
        return 'blocked'

def run(rooms, judge):
    keep, why = [], collections.Counter(); marks = 0
    for r in rooms:
        if r.type == 0: why[(0, 0, 0)] += 1; continue           # type 0 = a divider/comment row of the mod's room list, not a room
        bad = None; spawns = []
        for x, y, stack in r.spawns:
            ns = []
            for t, v, s, w in stack:
                k = judge.kind(t, v, s)
                if k == 'marker': marks += 1; continue
                if k in ('blocked', 'ff_pickup'): bad = bad or (t, v, s); break
                ns.append((t, v, s, w))
            else:
                if ns: spawns.append((x, y, ns))
                continue
            break
        if bad: why[bad] += 1; continue
        r.spawns = spawns; keep.append(r)
    return keep, why, marks

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--ff', required=True); ap.add_argument('--game-ref', required=True); ap.add_argument('--ported', required=True)
    ap.add_argument('--out'); ap.add_argument('--files', default='01.basement,02.cellar,03.burning basement,00.special rooms')
    a = ap.parse_args()
    j = Judge(entities(os.path.join(a.game_ref, 'entities2.xml')), entities(a.ported))
    for name in a.files.split(','):
        rooms = stb.read(os.path.join(a.ff, 'content-repentogon', 'rooms', name + '.stb'))
        keep, why, marks = run(rooms, j)
        print('%-22s %5d rooms -> %5d ready (%d blocked), %d backdrop markers dropped' % (name, len(rooms) , len(keep), len(rooms) - len(keep), marks))
        print('    blocked by:', ', '.join('%d.%d.%d x%d' % (k + (n,)) for k, n in why.most_common(10)))
        if a.out:
            os.makedirs(a.out, exist_ok=True)
            open(os.path.join(a.out, name + '.stb'), 'wb').write(stb.write(keep))
