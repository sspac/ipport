#!/usr/bin/env python3
"""Adds a diagnostic line to pack.c: which character menu / FF files the game asks for, and whether the pack holds them.
Idempotent. Usage: python3 ffwatch_patch.py port/app/src/main/cpp/pack.c"""
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8', newline='').read()
if 'ffwatch' in s:
    print('ffwatch already there'); sys.exit(0)
old = '''    g_pack_data++;
    const entry_t *e = by_path(path);
    if (!e) return NULL;
'''
new = '''    g_pack_data++;
    const entry_t *e = by_path(path);
#if ISAAC_DIAG
    { /* ffwatch: the character menu and Fiend Folio files the game asks for, and whether the pack has them */
        static int told;
        int shim_report_active(void);
        void report_line(const char *kind, const char *text);
        if (told < 60 && shim_report_active() && strstr(path, "main menu")) {
            char low[200]; size_t k = 0;
            for (; path[k] && k < sizeof low - 1; k++) low[k] = (path[k] >= 'A' && path[k] <= 'Z') ? path[k] + 32 : path[k];
            low[k] = 0;
            if (strstr(low, "character") || strstr(low, "/ff/") || strstr(low, "ff_") || strstr(low, "death")) {
                told++;
                char line[300];
                snprintf(line, sizeof line, "ffwatch: asked for \\"%s\\" - %s", path, e ? "the pack has it" : "NOT in the pack");
                report_line("pack", line);
            }
        }
    }
#endif
    if (!e) return NULL;
'''
if s.count(old) != 1: sys.exit('pack.c: expected text found %d times' % s.count(old))
open(p, 'w', encoding='utf-8', newline='').write(s.replace(old, new))
print('ffwatch added')
