# -*- coding: utf-8 -*-
"""The console's ENEMIES tab, Fiend Folio's part: a table of the FF monsters and the icon atlas that goes with it.

The game's own table (spawner_enemies.h) is the bestiary of the game's files. FF's monsters mostly have no <bestiary> entry
(72 of the 82 of chapter 1), so this list is OUR OWN: the floor lists of the FF wiki (https://fiendfolio.wiki.gg/wiki/Enemies),
checked against the rooms of the mod (rooms/*.stb) and its entities2.xml.

    python3 tools/spawner_ff_enemies.py --ff <FF mod folder> --out <folder> [--floors basement,cellar,burning]

writes into <folder>:
    ff/ffenemies_table.h                                            the rows (appended to SP_ENEMY by apply_ff.sh)
    assets/fiendfolio/gfx/port_ff_enemy_icons.png / .anm2           the icons, "Icons" frame k = row k, pivot top left

Icons: each monster's resting pose (the game's own rule: "Idle", else the default animation; an entrance takes its last
frame), drawn from FF's own gfx, cropped to the ink, a body over 48 px brought down by the smallest whole factor, each block
its majority colour - the same as the game's list.
"""
import argparse, importlib.util, math, os, re, sys, xml.etree.ElementTree as ET
from collections import Counter
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
FLOORS = {
 'basement': "Slammer|Wimpy|Red Horf|Mutant Horf|Mr. Horf|Mr. Red Horf|Morsel|Pester|Slim|Limb|Bowler|Loafer|Slobber|Bleeder|Casualty|Dung|Tall Boi|Shitling|Shot Fly|Shoter|Square Fly|Bumbler|Potluck|Poobottle|Tap|Butt Fly|Weaver|Drink Worm|Benign|Drumstick|Cheeky",
 'cellar': "Spooter|Super Spooter|Skuzz|Ring Skuzz|Army Skuzz|Baby Spider|Sticky Sack|Walking Sticky Sack|Ransacked|Coby|Nanny Long Legs|Brood|Zingling|Beeter|Hover|Honey Eye|Stingler|Homer|Honeydrip|Marge|Receptor|Haunted|Yawner|Crotchety|Dim|Spoop|Shirk",
 'burning': "S'More|Woodburner|Wick|Mr. Flare|Mr. Crisply|Powderkeg|Mullikaboom|Fried|Fire Fly|Spitroast|Spark|Charlie|Sooty|Big Smoke|Brisket|Smogger|Fumegeist|Mote|Smokin|Flamin|Trailblazer|Chili|Litling|Commission",
}
# the entity of a wiki name where the name alone is not unique or differs (type, variant, subtype)
PICK = {'Weaver': (155, 0, 0)}
# entities the behaviours spawn (not rows of the console): they still need their <entity> in entities2.xml and their art
SUPPORT = [(160, 461, 0), (160, 771, 0), (160, 773, 0), (160, 760, 2), (155, 0, 1), (155, 0, 2)]          # Pester's Eternal Fly (orbits it)
ICON_BASE = 10000          # SP_ENEMY[].icon >= this: frame (icon - ICON_BASE) of port_ff_enemy_icons.anm2 (spawner.c patch)
BAND = 48


def load_renderer(ff):
    spec = importlib.util.spec_from_file_location('anm2render', os.path.join(HERE, 'anm2render.py'))
    ar = importlib.util.module_from_spec(spec); spec.loader.exec_module(ar)
    ar.ROOTS = [os.path.join(ff, 'resources')]; ar._index = None; ar._sheets.clear()
    return ar


def entities(ff):
    out = {}
    for e in ET.parse(os.path.join(ff, 'content-repentogon', 'entities2.xml')).getroot().findall('entity'):
        a = dict(e.attrib)
        try: out[(int(a['id']), int(a.get('variant', 0)), int(a.get('subtype', 0)))] = a
        except (KeyError, ValueError): pass
    return out


def find_entity(ents, name):
    if name in PICK: return PICK[name], ents[PICK[name]]
    hits = [(k, a) for k, a in ents.items() if a.get('name') == name and 'Waiting' not in name]
    if not hits: raise SystemExit('no entities2.xml entry named %r' % name)
    hits.sort(key=lambda h: (h[0][0] in (4, 5, 6, 1000), h[0]))
    return hits[0]


def pose(ar, a):
    """the resting pose of a monster: (image, how). "Idle", else the default animation, else the bestiary's own ("Beastiary", as FF
    spells it), else the first animation with something drawn; an entrance takes its last frame."""
    rel = a['anm2path'].replace('\\', '/')
    try: an = ar.Anm2(rel)
    except FileNotFoundError: return None, 'anm2 not found: ' + rel
    miss = an.missing()
    order = [n for n in (a.get('bestiaryAnim'), 'Idle', 'idle', an.default, 'Beastiary', 'Bestiary') if n in an.anims]
    order += [n for n in an.anims if n not in order]
    for name in order:
        n = int(an.anims[name].get('FrameNum', 1))
        entrance = re.search(r'appear|spawn|entr|emerge', name, re.I)
        best = None
        for t in ([n - 1] if entrance else [0, n // 2]):
            c = Image.new('RGBA', (256, 256)); an.draw(c, name, max(t, 0), 0, 0, 1.0, (128, 128))
            ink = sum(1 for v in c.getchannel('A').tobytes()[::1] if v > 128) if False else sum(1 for v in c.getchannel('A').get_flattened_data() if v > 128) if hasattr(c, 'get_flattened_data') else sum(1 for v in c.getchannel('A').getdata() if v > 128)
            if ink > 20 and (best is None or ink > best[0]): best = (ink, c)
        if best: return best[1], '%s%s' % (name, ' (missing sheets %s)' % miss if miss else '')
    return None, 'nothing drawn in any animation%s' % (' (missing sheets %s)' % miss if miss else '')


def crop_ink(c):
    a = c.getchannel('A').point(lambda v: 255 if v > 40 else 0)
    box = a.getbbox()
    return c.crop(box) if box else c


def reduce(im):
    """a body over BAND comes down by the smallest whole factor, each block its majority colour"""
    f = max(1, math.ceil(max(im.size) / BAND))
    if f == 1: return im
    w, h = (im.width + f - 1) // f, (im.height + f - 1) // f
    out = Image.new('RGBA', (w, h)); px = im.load(); po = out.load()
    for y in range(h):
        for x in range(w):
            cnt = Counter(px[xx, yy] for yy in range(y * f, min(im.height, y * f + f)) for xx in range(x * f, min(im.width, x * f + f)) if px[xx, yy][3] > 40)
            if len(cnt) and sum(cnt.values()) * 2 >= f * f * 0.5: po[x, y] = cnt.most_common(1)[0][0]
    return out


def entity_text(raw, key):
    """the <entity> element of FF's entities2.xml for (id, variant, subtype), verbatim"""
    tag = re.compile(r'[ \t]*<entity\b[^>]*?/>[ \t]*\n?|[ \t]*<entity\b[^>]*[^/]>.*?</entity>[ \t]*\n?', re.S)
    for m in tag.finditer(raw):
        at = dict(re.findall(r'(\w+)="([^"]*)"', m.group(0).split('>')[0]))
        if (at.get('id'), at.get('variant', '0'), at.get('subtype', '0')) == tuple(str(k) for k in key): return m.group(0)
    return None


def art_files(ff, a):
    """the anm2 of a monster and the sheets it names, as paths under gfx/ that exist in FF's resources"""
    gfx = os.path.join(ff, 'resources', 'gfx'); out = []
    rel = a['anm2path'].replace('\\', '/')
    if not os.path.exists(os.path.join(gfx, rel)): return out, [rel]
    out.append(rel)
    base = os.path.dirname(rel); lost = []
    for sp in re.findall(r'Spritesheet Path="([^"]*)"', open(os.path.join(gfx, rel), encoding='utf-8', errors='ignore').read()):
        r = os.path.normpath(os.path.join(base, sp.replace('\\', '/'))).replace('\\', '/')
        if r.startswith('grid/'): continue                      # the grid's sheets are installed by instalar_grid.sh
        (out if os.path.exists(os.path.join(gfx, r)) else lost).append(r)
    return out, lost


def anm2_text(png, boxes):
    T = '\t'
    fr = ''.join(T * 5 + '<Frame XPosition="0" YPosition="0" XPivot="0" YPivot="0" XCrop="%d" YCrop="%d" Width="%d" Height="%d" XScale="100" YScale="100" Delay="%d" Visible="true" RedTint="255" GreenTint="255" BlueTint="255" AlphaTint="255" RedOffset="0" GreenOffset="0" BlueOffset="0" Rotation="0" Interpolated="false"/>\n'
                 % (x, y, w, h, 1) for x, y, w, h in boxes)
    n = len(boxes)
    return ('<AnimatedActor>\n\t<Info CreatedBy="Isaac Android port" CreatedOn="Fiend Folio" Version="2" Fps="30"/>\n\t<Content>\n\t\t<Spritesheets>\n'
            '\t\t\t<Spritesheet Path="%s" Id="0"/>\n\t\t</Spritesheets>\n\t\t<Layers>\n\t\t\t<Layer Name="Icons" Id="0" SpritesheetId="0"/>\n\t\t</Layers>\n\t\t<Nulls/>\n\t\t<Events/>\n\t</Content>\n'
            '\t<Animations DefaultAnimation="Icons">\n\t\t<Animation Name="Icons" FrameNum="%d" Loop="false">\n\t\t\t<RootAnimation>\n'
            '\t\t\t\t<Frame XPosition="0" YPosition="0" XScale="100" YScale="100" Delay="%d" Visible="true" RedTint="255" GreenTint="255" BlueTint="255" AlphaTint="255" RedOffset="0" GreenOffset="0" BlueOffset="0" Rotation="0" Interpolated="false"/>\n'
            '\t\t\t</RootAnimation>\n\t\t\t<LayerAnimations>\n\t\t\t\t<LayerAnimation LayerId="0" Visible="true">\n%s\t\t\t\t</LayerAnimation>\n\t\t\t</LayerAnimations>\n\t\t\t<NullAnimations/>\n\t\t\t<Triggers/>\n\t\t</Animation>\n\t</Animations>\n</AnimatedActor>\n'
            % (png, n, n, fr))


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--ff', required=True); ap.add_argument('--out', required=True)
    ap.add_argument('--floors', default='basement'); ap.add_argument('--preview')
    o = ap.parse_args()
    ar = load_renderer(o.ff); ents = entities(o.ff)
    rows, icons, seen = [], [], set()
    for fl in o.floors.split(','):
        for name in FLOORS[fl].split('|'):
            key, a = find_entity(ents, name)
            if key in seen: continue
            seen.add(key)
            im, how = pose(ar, a)
            print('%-10s %-22s %s.%s.%s  %s' % (fl, name, *key, how))
            if im is None: icons.append(None); rows.append((name, key, fl)); continue
            icons.append(reduce(crop_ink(im))); rows.append((name, key, fl))
    # the atlas: icons side by side in rows of 16, frame k = row k
    ok = [i for i in icons if i is not None]
    cellw = max(i.width for i in ok); cellh = max(i.height for i in ok)
    cols = 8; nrows = math.ceil(len(icons) / cols)
    sheet = Image.new('RGBA', (cols * cellw, nrows * cellh)); boxes = []
    for k, ic in enumerate(icons):
        x, y = (k % cols) * cellw, (k // cols) * cellh
        if ic is None: boxes.append((x, y, 1, 1)); continue
        sheet.alpha_composite(ic, (x, y)); boxes.append((x, y, ic.width, ic.height))
    # what each monster needs in the game: its <entity> (into fiendfolio/data/entities2.xml) and its art (gfx/ of the mod)
    raw = open(os.path.join(o.ff, 'content-repentogon', 'entities2.xml'), encoding='utf-8-sig', errors='ignore').read()
    gd = os.path.join(o.out, 'assets', 'fiendfolio', 'gfx'); os.makedirs(gd, exist_ok=True)
    files, texts, lostall = [], [], {}
    import shutil
    for name, key, fl in rows:
        t = entity_text(raw, key)
        if t is None: raise SystemExit('no <entity> element for %s %s' % (name, key))
        texts.append(t if t.endswith('\n') else t + '\n')
        fs, lost = art_files(o.ff, ents[key]); files += fs
        if lost: lostall[name] = lost
    for key in SUPPORT:
        if key not in ents: continue
        t = entity_text(raw, key)
        if t is None: continue
        texts.append(t if t.endswith('\n') else t + '\n')
        fs, lost = art_files(o.ff, ents[key]); files += fs
    for f in sorted(set(files)):
        d = os.path.join(gd, f); os.makedirs(os.path.dirname(d), exist_ok=True); shutil.copyfile(os.path.join(o.ff, 'resources', 'gfx', f), d)
    dd = os.path.join(o.out, 'assets', 'fiendfolio', 'ffenemies'); os.makedirs(dd, exist_ok=True)
    open(os.path.join(dd, 'entities2_%s.xml' % o.floors.replace(',', '_')), 'w', encoding='utf-8').write('<entities>\n' + '\n'.join(texts) + '</entities>\n')
    open(os.path.join(dd, 'files_%s.txt' % o.floors.replace(',', '_')), 'w').write('\n'.join(sorted(set(files))) + '\n')
    for n, l in lostall.items(): print('  not in FF resources (the game has them, or they are unused): %s -> %s' % (n, ', '.join(l)))
    print('%d art files, %d entities' % (len(set(files)), len(texts)))
    sheet.save(os.path.join(gd, 'port_ff_enemy_icons.png'))
    open(os.path.join(gd, 'port_ff_enemy_icons.anm2'), 'w').write(anm2_text('port_ff_enemy_icons.png', boxes))
    fd = os.path.join(o.out, 'ff'); os.makedirs(fd, exist_ok=True)
    L = ['/* Written by tools/spawner_ff_enemies.py - do not edit by hand. Included twice by spawner_enemies.h (ffconsole_patch.py):',
         ' * once for the count, once inside the SP_ENEMY initializer (group SP_GROUP_FF = FIEND FOLIO). */',
         '#ifndef FF_ENEMY_ROWS', '#define FF_ENEMY_ROWS %d' % len(rows), '#endif', '#ifdef FF_ENEMY_BODY']
    for k, (name, (t, v, s), fl) in enumerate(rows):
        w, h = (boxes[k][2], boxes[k][3]) if icons[k] is not None else (0, 0)
        n = name.replace('\\', '\\\\').replace('"', '\\"')
        L.append('    { %d, %d, %d, SP_GROUP_FF, %d, %d, %d, -1, -1, 0, { "%s", "%s", "%s", "%s", "%s", "%s", "%s" } },   /* %s */'
                 % (t, v, s, ICON_BASE + k if icons[k] is not None else -1, w, h, *([n] * 7), fl))
    open(os.path.join(fd, 'ffenemies_table.h'), 'w').write('\n'.join(L) + '\n#endif\n')
    if o.preview:
        pv = Image.new('RGBA', sheet.size, (233, 218, 223, 255)); pv.alpha_composite(sheet); pv = pv.resize((pv.width * 3, pv.height * 3), Image.NEAREST); pv.save(o.preview)
    print('%d rows, atlas %s' % (len(rows), sheet.size))

if __name__ == '__main__': main()
