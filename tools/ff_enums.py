# -*- coding: utf-8 -*-
"""scripts/ff/ffenums.h from the Isaac API docs' enum pages (Wofsauge/IsaacDocs, docs/enums/*.md): the numbers the Lua of Fiend Folio uses by name.
    python3 tools/ff_enums.py <IsaacDocs/docs/enums> scripts/ff/ffenums.h
"""
import re, sys, os
src, out = sys.argv[1:3]
ENUMS = [('SoundEffect', ''), ('EntityFlag', 'EF_'), ('EntityCollisionClass', ''), ('EntityGridCollisionClass', ''), ('ProjectileFlags', 'PF_'), ('NpcState', ''),
         ('ProjectileVariant', 'PV_'), ('EffectVariant', 'EV_'), ('EntityType', 'ET_')]
o = ['/* Written by tools/ff_enums.py from the Isaac API docs - do not edit by hand. */', '#ifndef FFENUMS_H', '#define FFENUMS_H']
for name, pre in ENUMS:
    seen = set()
    for l in open(os.path.join(src, name + '.md'), encoding='utf-8'):
        if not l.startswith('|'): continue
        c = [x.strip() for x in l.split('|')]
        if len(c) < 4: continue
        m = re.match(r'^(\d+|1\s*<<\s*\d+|0x[0-9a-fA-F]+)\s*$', c[2]); n = re.match(r'^([A-Za-z0-9_]+)', c[3])
        if not (m and n): continue
        nm = pre + n.group(1)
        if nm in seen: continue
        seen.add(nm); v = re.sub(r'\s+', '', m.group(1))
        o.append('#define %s %s' % (nm, ('(1ull<<%s)' % v.split('<<')[1]) if '<<' in v else v))
o.append('#endif')
open(out, 'w').write('\n'.join(o) + '\n')
print(len(o), 'lines')
