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

# ffsound: a Fiend Folio path the game asks for and the pack does not hold is reported with its real spelling
s = open(sys.argv[1], encoding='utf-8', newline='').read()
if 'ffsound: miss' not in s:
    old = "    if (inner_path(path, inside, sizeof(inside)) == (size_t) -1) return NULL;\n    return find(inside);\n}"
    new = """    if (inner_path(path, inside, sizeof(inside)) == (size_t) -1) return NULL;
    const entry_t *hit_ = find(inside);
    if (!hit_ && strstr(path, "fiendfolio")) {            /* ffsound: miss */
        int shim_report_active(void);
        void report_line(const char *kind, const char *text);
        static int told_;
        if (shim_report_active() && told_ < 12) { told_++; char l_[400]; snprintf(l_, sizeof(l_), "the game asked for \\"%s\\" (inside the pack: \\"%s\\") and it is not there", path, inside); report_line("pack", l_); }
    }
    return hit_;
}"""
    if s.count(old) != 1: sys.exit('pack.c: ffsound miss anchor found %d times' % s.count(old))
    open(sys.argv[1], 'w', encoding='utf-8', newline='').write(s.replace(old, new, 1))
    print('ffsound miss log added')

# ffsound: the game opens a sample by a RELATIVE path ("sfx/fiendfolio/...": the log says Failed to open sample), which inner_path
# rejects because it does not start with the resource root. inner_path is what every entry point (open, fopen, stat, access, opendir)
# goes through, so it is fixed THERE: a relative "sfx/..." is read as living under repentance-resources/data/. Every ask that names
# fiendfolio is logged (first 40), with whatever the game spelled.
s = open(sys.argv[1], encoding='utf-8', newline='').read()
if 'ffsound: relative' not in s:
    old = "static size_t inner_path(const char *path, char *out, size_t capacity) {\n    if (strncmp(path, g_root, g_root_len)) return (size_t) -1;\n    const char *p = path + g_root_len;\n"
    new = """static size_t inner_path(const char *path, char *out, size_t capacity) {
    if (strstr(path, "fiendfolio")) {                     /* ffsound: relative */
        int shim_report_active(void); void report_line(const char *kind, const char *text);
        static int raw_;
        if (raw_ < 40 && shim_report_active()) { raw_++; char r2_[400]; snprintf(r2_, sizeof(r2_), "the game asks for \\"%s\\"", path); report_line("pack", r2_); }
    }
    if (path[0] != '/' && !strncmp(path, "sfx/", 4)) {
        snprintf(out, capacity, "repentance-resources/data/%s", path);
        return strlen(out);
    }
    if (strncmp(path, g_root, g_root_len)) return (size_t) -1;
    const char *p = path + g_root_len;
"""
    if s.count(old) != 1: sys.exit('pack.c: ffsound inner_path anchor found %d times' % s.count(old))
    open(sys.argv[1], 'w', encoding='utf-8', newline='').write(s.replace(old, new, 1))
    print('ffsound relative lookup added')
