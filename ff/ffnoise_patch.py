#!/usr/bin/env python3
"""Fiend Folio: keep the profiler's "who" / "where" lines (objc.c) and the "haptic*" lines out of isaac-start.txt, so the report is the
FF categories and not hundreds of lines of CPU breakdown. One anchored edit in loader.c's report_line (the single place every report line goes
through). A build with -DFF_NOISE=1 keeps them. Idempotent: run twice, the second says so.   python3 ffnoise_patch.py <path to loader.c>"""
import sys
path = sys.argv[1]
s = open(path, encoding='utf-8', newline='').read()
if 'FF_NOISE' in s:
    print('already applied: ' + path); sys.exit(0)
head = 'void report_line(const char *kind, const char *text) {'
i = s.find(head)
if i < 0 or s.count(head) != 1:
    raise SystemExit('report_line not found exactly once in %s' % path)
nl = '\r\n' if '\r\n' in s[i:i + 200] else '\n'
guard = '    if (g_report_fd < 0) return;'
j = s.find(guard, i)
if j < 0 or j - i > 200:
    raise SystemExit('the guard line of report_line is not where expected in %s' % path)
k = j + len(guard)
add = (nl + '    /* Fiend Folio: the profiler\'s "who"/"where" breakdown and the haptics lines are left out of the report (-DFF_NOISE=1 keeps them) */' + nl +
       '#ifndef FF_NOISE' + nl +
       '    if (!strcmp(kind, "who") || !strcmp(kind, "where") || !strncmp(kind, "haptic", 6)) return;' + nl +
       '#endif')
open(path, 'w', encoding='utf-8', newline='').write(s[:k] + add + s[k:])
print('report noise filter added: ' + path)
