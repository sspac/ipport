#!/usr/bin/env python3
"""Adds a diagnostic line to pack.c: which character menu / FF files the game asks for, and whether the pack holds them.
Idempotent. Usage: python3 ffwatch_patch.py port/app/src/main/cpp/pack.c"""
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8', newline='').read()
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
if 'ffwatch' in s: print('ffwatch already there')
elif s.count(old) != 1: sys.exit('pack.c: expected text found %d times' % s.count(old))
else:
    open(p, 'w', encoding='utf-8', newline='').write(s.replace(old, new))
    print('ffwatch added')

# ffsound: the first time the game asks for sounds.xml the FF sounds are added to it (ffsound.c)
s = open(sys.argv[1], encoding='utf-8', newline='').read()
if 'ffsound_on_open' not in s:
    old = "    g_pack_data++;\n    const entry_t *e = by_path(path);\n"
    new = """    g_pack_data++;
    { /* ffsound: sounds.xml gets the Fiend Folio sounds the first time it is asked for */
        void ffsound_on_open(const char *inner) __attribute__((weak));
        size_t pl = strlen(path);
        char fin[1024];
        if (ffsound_on_open && pl > 10 && same_path(path + pl - 10, "sounds.xml") && inner_path(path, fin, sizeof(fin)) != (size_t) -1)
            ffsound_on_open(fin);
    }
    const entry_t *e = by_path(path);
"""
    if s.count(old) != 1: sys.exit('pack.c: ffsound anchor found %d times' % s.count(old))
    open(sys.argv[1], 'w', encoding='utf-8', newline='').write(s.replace(old, new, 1))
    print('ffsound hook added')
