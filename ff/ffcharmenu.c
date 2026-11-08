/*
 * FIEND FOLIO - THE CHARACTER SELECT SCREEN, by giving the characters the two sprites the engine already draws for them.
 *
 * WHAT THE ENGINE DOES (read in the image, sha 217be349..., offsets = addresses):
 *   - the select screen builds the list of extra characters (slots 0x12 and up) from the player table
 *     [[0xac2428]+0x56150 ...] (record = 0x786d74(table, id), stride 0x178, from id 41 on). To draw one it does NOT use the
 *     vanilla table of animation names: the centre picture is 0x775ca4(table, id, pos, menu sprite) and the ring slot is
 *     0x77597c(table, id, pos, tint, scale); both take the player's RECORD and draw the sprite stored in it:
 *         record+0x150  the centre picture's Sprite (plays the animation named after the character: 0x781f18(rec, 0x14))
 *         record+0x158  the ring portrait's Sprite
 *     and both do nothing visible when those pointers are 0. Nothing in the port fills them: on the PC the mod loader does it
 *     (CharacterMenu.anm2 / CharacterPortraits.anm2 of the mod, its own sheets), which is why a character is selectable but
 *     empty here, with every file in place (the log shows the game asking for them and the pack holding them).
 *
 * WHAT THIS FILE DOES: before each update of the select screen (0x5e65a8), for every Fiend Folio record (ids 41..60) whose
 * two pointers are 0 it makes a Sprite (constructor 0x89ca08, then 0x89fe24 = load an .anm2, as the screen itself does for its
 * own sprites) from the game's resources: gfx/ui/main menu/charactermenu.anm2 + characterportraits.anm2, or the ...alt.anm2
 * ones for the tainted records; those are the files that carry the Fiend Folio animations ("Fiend", "Golem"...) and their own
 * sheet. Nothing is drawn by this code: the engine draws them the way it draws any character with those sprites.
 *
 * NOT MEASURED: the size of a Sprite (0x158 by the spacing of the screen's own sprites; 0x200 are reserved) and what the
 * engine does with the pointers when it destroys a record (the memory comes from calloc, which is what operator delete frees).
 * The log (tag "isaac", lines "ffcharmenu:") says what was made and whether the load worked.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <android/log.h>
#include "diag.h"

#include "ffsay.h"

void shim_hook(void *at, void *to);

#define FN_UPDATE        0x5e65a8      /* (menu) */
#define FN_RECORD        0x786d74      /* (table, id) -> record, stride 0x178 */
#define FN_SPRITE_CTOR   0x89ca08      /* (sprite) */
#define FN_SPRITE_LOAD   0x89fe24      /* (sprite, std::string *path, int loadGraphics) */
#define FN_STRING_CTOR   0x88e4        /* (std::string *out, const char *) */
#define MGR_PTR          0xac2428
#define TABLE_OFF        0x56150
#define SPRITE_BYTES     0x200

#define R_CENTRE 0x150
#define R_RING   0x158

#define FF_FIRST 41
#define FF_COUNT 20

/* players.xml order of Fiend Folio: the odd ones among the first eight are the tainted variants */
static int is_alt(unsigned id) { return id >= FF_FIRST && id < FF_FIRST + 8 && ((id - FF_FIRST) & 1); }

typedef void (*fn_menu)(unsigned long);
typedef unsigned long (*fn_record)(unsigned long, unsigned);
typedef void (*fn_ctor)(void *);
typedef void (*fn_load)(void *, void *, int);
typedef void (*fn_str)(void *, const char *);

static unsigned char *g_base;
static fn_menu o_update;
static int g_done, g_logs;

/*
 * A first version made its own Sprites for the records and the game CRASHED on the first draw of the ring (CRASH at 0x89d630,
 * called from 0x89e530 <- 0x775a50 <- 0x5e9670: the layer loop read sprite[0]+0xa8 with sprite[0] == 0): a Sprite made by hand
 * was not complete. So now the records point at the two Sprites the select screen has ALREADY loaded and draws with itself:
 * m+0x78 (CharacterMenu.anm2, the centre picture) and m+0xf60 (CharacterPortraits.anm2, the ring). The pack's copies of those
 * files carry the Fiend Folio animations ("Fiend", "Golem"...), so the engine plays them like any other character's.
 */
#define M_CENTRE 0x78
#define M_RING   0xf60

static void fill(unsigned long m) {
    unsigned long mgr = *(const unsigned long *) (g_base + MGR_PTR);
    if (!mgr || !m) return;
    unsigned long centre_sprite = m + M_CENTRE, ring_sprite = m + M_RING;
    if (!*(const unsigned long *) centre_sprite || !*(const unsigned long *) ring_sprite) return;     /* not loaded (yet): do nothing */
    unsigned long table = mgr + TABLE_OFF;
    int made = 0, seen = 0;
    for (unsigned id = FF_FIRST; id < FF_FIRST + FF_COUNT; id++) {
        unsigned long rec = ((fn_record) (g_base + FN_RECORD))(table, id);
        if (!rec) continue;
        seen++;
        unsigned long *centre = (unsigned long *) (rec + R_CENTRE), *ring = (unsigned long *) (rec + R_RING);
        if (*centre == centre_sprite && *ring == ring_sprite) continue;
        *centre = centre_sprite; *ring = ring_sprite;
        made++;
    }
    if (made && g_logs++ < 4) LOG("ffcharmenu: %d of %d Fiend Folio records now use the screen's own centre (m+0x78) and ring (m+0xf60) sprites", made, seen);
    if (seen == FF_COUNT && !made) g_done = 1;
}

/* DIAGNOSTIC, once: where the select screen keeps its other Sprites (the tainted/alt ones are not referenced by string in the
 * screen's init). A Sprite's first qword is its .anm2 data, shared by every Sprite loaded from the same file, so: which offsets
 * hold the same data as m+0x78 (CharacterMenu) and m+0xf60 (CharacterPortraits), and then every non-zero qword of the object. */
static int g_dumped;
static void dump_menu(unsigned long m) {
    if (g_dumped || !*(const unsigned long *) (m + M_RING) || !*(const unsigned long *) (m + M_CENTRE)) return;
    g_dumped = 1;
    unsigned long dm = *(const unsigned long *) (m + M_CENTRE), dp = *(const unsigned long *) (m + M_RING);
    char line[512]; int n = 0; line[0] = 0;
    for (unsigned off = 0x40; off < 0x1700; off += 8) {
        unsigned long v = *(const unsigned long *) (m + off);
        if (v == dm || v == dp) n += snprintf(line + n, sizeof line - n, " %#x=%s", off, v == dm ? "MENU" : "PORT");
        if (n > 400) { LOG("ffcharmenu: dump same-anm2 offsets:%s", line); n = 0; line[0] = 0; }
    }
    if (n) LOG("ffcharmenu: dump same-anm2 offsets:%s", line);
    n = 0; line[0] = 0;
    for (unsigned off = 0x40; off < 0x1700; off += 8) {
        unsigned long v = *(const unsigned long *) (m + off);
        if (!v) continue;
        n += snprintf(line + n, sizeof line - n, " %x:%lx", off, v);
        if (n > 400) { LOG("ffcharmenu: dump%s", line); n = 0; line[0] = 0; }
    }
    if (n) LOG("ffcharmenu: dump%s", line);
}

static void w_update(unsigned long m) {
    dump_menu(m);
    if (!g_done) fill(m);
    o_update(m);
}

void shim_ffcharmenu_install(unsigned char *base) {
    static const uint32_t w_upd[4] = { 0xd10443ffu, 0x6d073befu, 0x6d0833edu, 0x6d092bebu };
    if (o_update) return;
    const uint32_t *fn = (const uint32_t *) (base + FN_UPDATE);
    for (int k = 0; k < 4; k++)
        if (fn[k] != w_upd[k]) { LOG("ffcharmenu: 0x%x does not start as measured - nothing touched", FN_UPDATE); return; }
    g_base = base;
    uint8_t *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) { LOG("ffcharmenu: no page for the continuation"); return; }
    memcpy(page, fn, 16);
    shim_hook(page + 16, (void *) (fn + 4));
    if (mprotect(page, 4096, PROT_READ | PROT_EXEC) != 0) { munmap(page, 4096); return; }
    __builtin___clear_cache((char *) page, (char *) page + 32);
    o_update = (fn_menu) page;
    shim_hook((void *) fn, (void *) w_update);
    LOG("ffcharmenu: character select wrapped; the Fiend Folio records get their sprites before the first draw");
}
