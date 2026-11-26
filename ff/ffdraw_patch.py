#!/usr/bin/env python3
"""Fiend Folio: one anchored edit in trace.c - the HUD pass (mark 'X', before 0x6eeb78 runs, where eid.c and the mods draw) also calls ffchest_draw(),
the Glass Chest's preview of its contents (ffen_g16.h). The call is a weak reference: a build without ffminion.c links and simply skips it.
Idempotent: run twice, the second says so.   python3 ffdraw_patch.py <path to trace.c>"""
import sys
path = sys.argv[1]
s = open(path, encoding='utf-8', newline='').read()
if 'ffchest_draw' in s:
    print('already applied: ' + path); sys.exit(0)
old = 'void shim_mods_draw(void); shim_mods_draw(); }'
if s.count(old) != 1:
    raise SystemExit('the HUD hook of trace.c (shim_mods_draw) was not found exactly once in %s' % path)
nl = '\r\n' if '\r\n' in s else '\n'
new = ('void shim_mods_draw(void); shim_mods_draw();' + nl +
       '                          { void ffchest_draw(void) __attribute__((weak)); if (ffchest_draw) ffchest_draw(); } }   /* Fiend Folio: the Glass Chest\'s preview */')
open(path, 'w', encoding='utf-8', newline='').write(s.replace(old, new))
print('Glass Chest preview hook added: ' + path)
