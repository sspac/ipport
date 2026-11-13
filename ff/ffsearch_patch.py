#!/usr/bin/env python3
"""Category search for the console (ITEMS and ENEMIES tabs), run from apply_ff.sh on spawner.c (argument: the cpp folder).

Typing a category - the WHOLE query equal to a category word, in any case, singular or plural - lists the complete category
where a name match used to be, in the list's normal order and under its normal headings:

  ENEMIES tab   enemies | enemy            every row (all groups, with their headings)
                bosses | boss              the bosses
                mini bosses | miniboss(es) | mini-boss    the minibosses
                monsters | monster         the monsters
                training | training dummy | dummy         the training dummy
                fiend folio | fiendfolio | ff            Fiend Folio's rows (the group SP_GROUP_FF, ffconsole_patch.py)
  ITEMS tab     items | collectibles       the ITEMS section        keys, bombs, coins, hearts, batteries    those sections
                cards, runes, pills, trinkets, loot                 those sections     pickups   keys..batteries and loot
                fiend folio | fiendfolio | ff   every row whose id is past the game's own (collectible > SP_COLLECTIBLE_TOP,
                trinket > SP_TRINKET_TOP, card > EID_CARD_MAX) - the rows the description database lists for mods

Anything else is searched by name exactly as before. Every edit demands its anchor exactly once (a changed spawner.c stops the build
here); a second run does nothing.
"""
import sys
C = sys.argv[1].rstrip('/') + '/'
def rd(p): return open(p, encoding='utf-8', newline='').read()
def wr(p, s): open(p, 'w', encoding='utf-8', newline='').write(s)
def once(s, old, new):
    if s.count(old) != 1: raise SystemExit('ffsearch_patch: %r found %d times in spawner.c' % (old[:70], s.count(old)))
    return s.replace(old, new)

HELPERS = r'''
/*
 * CATEGORY WORDS (Fiend Folio port): a query that is a whole category name - "enemies", "mini bosses", "keys", "fiend folio" - lists the
 * complete category in its normal order and under its normal headings; any other text is searched by name as before.
 */
static int cat_norm(const char *q, char *out, int room) {                /* lower case; dash, dot, apostrophe and underscore count as a space; spaces trimmed and joined */
    int n = 0, gap = 0;
    for (; *q && n < room - 2; q++) {
        unsigned char c = (unsigned char) *q;
        if (c >= 'A' && c <= 'Z') c = (unsigned char) (c + 32);
        if (c == ' ' || c == '-' || c == '.' || c == '\'' || c == '_') { gap = n > 0; continue; }
        if (gap) { out[n++] = ' '; gap = 0; }
        out[n++] = (char) c;
    }
    out[n] = 0;
    return n;
}
/* the word, its plural (s, es, y -> ies) */
static int cat_is(const char *q, const char *word) {
    size_t n = strlen(word);
    if (!strcmp(q, word)) return 1;
    if (!strncmp(q, word, n) && (!strcmp(q + n, "s") || !strcmp(q + n, "es"))) return 1;
    if (n > 1 && word[n - 1] == 'y' && !strncmp(q, word, n - 1) && !strcmp(q + n - 1, "ies")) return 1;
    return 0;
}
typedef struct { const char *word; unsigned mask; } cat_word_t;
static unsigned cat_mask(const char *query, const cat_word_t *t, int n) {
    char q[64];
    if (!cat_norm(query, q, sizeof q)) return 0;
    unsigned m = 0;
    for (int i = 0; i < n; i++) if (cat_is(q, t[i].word)) m |= t[i].mask;
    return m;
}
#define CAT_FF 0x80000000u

#ifdef SP_GROUP_FF
#define CAT_ALL_ENEMIES ((1u << SP_GROUP_BOSS) | (1u << SP_GROUP_MINIBOSS) | (1u << SP_GROUP_MONSTER) | (1u << SP_GROUP_TRAINING) | (1u << SP_GROUP_FF))
#define CAT_ENEMY_FF (1u << SP_GROUP_FF)
#else
#define CAT_ALL_ENEMIES ((1u << SP_GROUP_BOSS) | (1u << SP_GROUP_MINIBOSS) | (1u << SP_GROUP_MONSTER) | (1u << SP_GROUP_TRAINING))
#define CAT_ENEMY_FF 0u
#endif
/* the groups of SP_ENEMY a query names, as a bit mask (0: not a category) */
static unsigned enemy_category(const char *query) {
    static const cat_word_t T[] = {
        { "enemy", CAT_ALL_ENEMIES },
        { "boss", 1u << SP_GROUP_BOSS },
        { "mini boss", 1u << SP_GROUP_MINIBOSS }, { "miniboss", 1u << SP_GROUP_MINIBOSS },
        { "monster", 1u << SP_GROUP_MONSTER },
        { "training", 1u << SP_GROUP_TRAINING }, { "training dummy", 1u << SP_GROUP_TRAINING }, { "dummy", 1u << SP_GROUP_TRAINING },
        { "fiend folio", CAT_ENEMY_FF }, { "fiendfolio", CAT_ENEMY_FF }, { "ff", CAT_ENEMY_FF },
    };
    return cat_mask(query, T, (int) (sizeof T / sizeof T[0]));
}

/* the ITEMS tab: the whole list, cut to the sections named (or, for Fiend Folio, to the rows past the game's own ids); 0 when the query is no category */
static int items_category(const char *query) {
    static const cat_word_t T[] = {
        { "item", 1u << SP_SEC_ITEMS }, { "collectible", 1u << SP_SEC_ITEMS },
        { "key", 1u << SP_SEC_KEYS }, { "bomb", 1u << SP_SEC_BOMBS }, { "coin", 1u << SP_SEC_COINS }, { "heart", 1u << SP_SEC_HEARTS },
        { "battery", 1u << SP_SEC_BATTERIES }, { "card", 1u << SP_SEC_CARDS }, { "rune", 1u << SP_SEC_RUNES }, { "pill", 1u << SP_SEC_PILLS },
        { "trinket", 1u << SP_SEC_TRINKETS }, { "loot", 1u << SP_SEC_LOOT },
        { "pickup", (1u << SP_SEC_KEYS) | (1u << SP_SEC_BOMBS) | (1u << SP_SEC_COINS) | (1u << SP_SEC_HEARTS) | (1u << SP_SEC_BATTERIES) | (1u << SP_SEC_LOOT) },
        { "fiend folio", CAT_FF }, { "fiendfolio", CAT_FF }, { "ff", CAT_FF },
    };
    unsigned m = cat_mask(query, T, (int) (sizeof T / sizeof T[0]));
    if (!m) return 0;
    the_whole_list();
    int w = 0, head = -1, put = 0;
    for (int i = 0; i < g_rows_n; i++) {
        const hit_t r = g_rows[i];
        if (r.kind == KIND_HEAD) {
            head = i; put = 0;
            if (!(m & CAT_FF) && (m >> r.id & 1)) { g_rows[w++] = r; put = 1; }        /* a section asked for by name: its heading, then every row */
            continue;
        }
        int keep;
        if (m & CAT_FF) {
            unsigned id = r.id;
            keep = (r.kind == KIND_ITEM && id > SP_COLLECTIBLE_TOP) || (r.kind == KIND_TRINKET && (id & ~(unsigned) TRINKET_GOLD) > SP_TRINKET_TOP) ||
                   (r.kind == KIND_CARD && id > EID_CARD_MAX);
        } else keep = put;
        if (!keep) continue;
        if (!put) { g_rows[w++] = g_rows[head]; put = 1; }                              /* Fiend Folio: the section's heading once, when it has a row */
        g_rows[w++] = r;
    }
    g_rows_n = w;
    return 1;
}
'''

s = rd(C + 'spawner.c')
if 'cat_norm' in s:
    print('search: spawner.c already has the category search')
else:
    s = once(s, 'static float section_h(int set, int section) {', HELPERS + '\nstatic float section_h(int set, int section) {')
    s = once(s, '    if (*q) {\n        g_rows_n = 0;\n        for (int i = 0; i < g_hit_count; i++) row_put(g_hits[i].kind, g_hits[i].id);\n    } else {',
             '    if (*q && items_category(q)) {\n        /* a category word: the complete category, g_rows already built */\n    } else if (*q) {\n        g_rows_n = 0;\n        for (int i = 0; i < g_hit_count; i++) row_put(g_hits[i].kind, g_hits[i].id);\n    } else {')
    s = once(s, '    const char *q = g_queries[1];\n    while (*q == \' \') q++;\n    if (*q) {\n        hit_t hits[HITS_MAX];\n        int found = shim_spawner_enemy_search(g_queries[1], hits, HITS_MAX);',
             '    const char *q = g_queries[1];\n    while (*q == \' \') q++;\n    unsigned cm = *q ? enemy_category(q) : 0;\n'
             '    if (cm) {                                                  /* a category word: every row of it under its headings */\n'
             '        int group = -1;\n'
             '        for (unsigned i = 0; i < SP_ENEMIES && n < l->room - 1; i++) {\n'
             '            if (!(cm >> SP_ENEMY[i].group & 1)) continue;\n'
             '            if (SP_ENEMY[i].group != group) {\n'
             '                group = SP_ENEMY[i].group;\n'
             '                g_base_row[n++] = (short) (-1 - group);\n'
             '            }\n'
             '            g_base_row[n++] = (short) i;\n'
             '        }\n'
             '    } else if (*q) {\n        hit_t hits[HITS_MAX];\n        int found = shim_spawner_enemy_search(g_queries[1], hits, HITS_MAX);')
    wr(C + 'spawner.c', s)
    print('search: spawner.c patched (categories in the ITEMS and ENEMIES tabs)')
