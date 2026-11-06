/*
 * FIEND FOLIO - THE CHARACTER SELECT AND DEATH SCREEN, inside the game (no mods/ folder).
 *
 * WHAT THE ENGINE DOES (read in the image, sha 217be349..., offsets = addresses):
 *   0x45b348(this)  the content loader of the mod manager. For each ENABLED mod it builds "<content dir>/gfx/" and calls
 *   0x45bbd8(this, std::string dir) (the call is at 0x45bb2c). That routine builds dir+"CharacterMenu.anm2",
 *   dir+"CharacterPortraits.anm2" and dir+"Death Screen.anm2" (strings 0xa629e2, 0xa629f5, 0xa62a0d), asks the game's
 *   resource system (0x8dfac4) that all three exist, loads them into a container of their own (0x45d5e4) and walks the
 *   character table from index 0x12 (stride 0xe0, name at +8; the table is [[0xac1768]+0x32958 .. +0x32960]): every
 *   character whose NAME is an animation of all three files is put in a map (0x45ef7c) that points at that container.
 *   The container draws with ITS OWN sheets (the Spritesheet of each .anm2), so a picture a player changes in the game's
 *   own menu files never touches ours: this is how Fiend Folio does it on the PC (content/gfx/charactermenu.anm2 + its own
 *   charactermenu.png).
 *
 * WHAT THIS FILE DOES: after the original loader has run, it calls 0x45bbd8 once more with a folder of the GAME's own
 * resources, "gfx/ui/main menu/ff/", where integrar_ff_menu.sh puts Fiend Folio's three files untouched (index.txt "file"
 * rows, the same road as every other file of the port). No mod, no list of mods, no switch.
 *
 * WHAT IS NOT MEASURED (the build with diagnostics says which one fails, in logcat, tag "isaac"):
 *   - that the character table already holds the names of players.xml when the loader runs (the count is logged; with the
 *     table not past index 0x12 nothing is called);
 *   - that 0x8dfac4 finds "gfx/ui/main menu/ff/..." (the vanilla files are asked for as "gfx/ui/main menu/..."); if one of
 *     the three is missing the routine returns without registering anyone - nothing breaks, the menu is as it was;
 *   - that each png is resolved next to its .anm2.
 *
 * THE STRING: libc++ in this image keeps a short string in the 24 bytes themselves: data from byte 0, length in byte 0x17
 * (the sign bit of that byte flags a long one: the code tests it with ldrsb). "gfx/ui/main menu/ff/" is 20 bytes.
 */
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <android/log.h>
#include "diag.h"

#include "ffsay.h"

void shim_hook(void *at, void *to);

#define FN_LOAD_CONTENT   0x45b348      /* (this) */
#define FN_REGISTER_DIR   0x45bbd8      /* (this, std::string *dir) */
#define MGR_PTR           0xac1768      /* [base + this] -> the object that holds the character table */
#define CHAR_TABLE_BEGIN  0x32958       /* begin and end pointers, stride 0xe0 */
#define CHAR_STRIDE       0xe0
#define FIRST_MENU_CHAR   0x12

#define FF_MENU_DIR "gfx/ui/main menu/ff/"

typedef void (*fn_load)(unsigned long);
typedef void (*fn_register)(unsigned long, void *);
static fn_load o_load;
static unsigned char *g_base;

/* a libc++ short string: 24 bytes, data from 0, length at 0x17 */
static int make_short_string(unsigned char out[24], const char *s) {
    size_t n = strlen(s);
    if (n > 22) return 0;
    memset(out, 0, 24);
    memcpy(out, s, n);
    out[0x17] = (unsigned char) n;
    return 1;
}

static unsigned table_count(void) {
    if (!g_base) return 0;
    unsigned long mgr = *(const unsigned long *) (g_base + MGR_PTR);
    if (!mgr) return 0;
    const unsigned long *t = (const unsigned long *) (mgr + CHAR_TABLE_BEGIN);
    if (!t[0] || t[1] < t[0]) return 0;
    return (unsigned) ((t[1] - t[0]) / CHAR_STRIDE);
}

static void (*g_register)(unsigned long, void *);       /* the host test puts its own here */

#include <stdlib.h>
static const char *const FF_NAMES[20] = { "Fiend","Biend","Golem","Bolem","Friend","Briend","Inaba","Binaba","Slippy","China",
    "Craig","Classic Fiend","Image","Skeletal Fiend","Fend","Frond","Fient","Mi","Freezer Jr.","Peat" };
#define FF_FIRST_ID 41

/* The AB+ table is empty in a Repentance run (players.xml fills another one), so the loader would walk nothing. Give it a
 * vector with every id up to the last FF one: unnamed rows stay zero (the loader looks the empty name up, finds nothing, skips). */
static void fill_table(void) {
    unsigned long mgr = *(const unsigned long *) (g_base + MGR_PTR);
    if (!mgr) return;
    unsigned long *t = (unsigned long *) (mgr + CHAR_TABLE_BEGIN);
    unsigned want = FF_FIRST_ID + 20;
    unsigned have = (t[0] && t[1] >= t[0]) ? (unsigned) ((t[1] - t[0]) / CHAR_STRIDE) : 0;
    if (have >= want) return;
    unsigned char *v = calloc(want, CHAR_STRIDE);
    if (!v) return;
    if (have) memcpy(v, (void *) t[0], (size_t) have * CHAR_STRIDE);
    for (int k = 0; k < 20; k++) make_short_string(v + (size_t) (FF_FIRST_ID + k) * CHAR_STRIDE + 8, FF_NAMES[k]);
    t[0] = (unsigned long) v;                       /* the old block is left alone: it may be owned by someone else */
    t[1] = (unsigned long) v + (size_t) want * CHAR_STRIDE;
    if (t[2] < t[1]) t[2] = t[1];
    LOG("ffmenu: character table was %u entries, now %u (names of the 20 FF characters at ids 41..60)", have, want);
}

static void w_load(unsigned long self) {
    o_load(self);
    LOG("ffmenu: the content loader ran");
    fill_table();
    unsigned n = table_count();
    if (n <= FIRST_MENU_CHAR) { LOG("ffmenu: the character table holds %u entries - nothing registered", n); return; }
    unsigned char dir[24];
    if (!make_short_string(dir, FF_MENU_DIR)) return;
    g_register(self, dir);
    LOG("ffmenu: Fiend Folio's menu files registered from %s (character table: %u entries)", FF_MENU_DIR, n);
}

#ifndef FF_HOST_TEST
static void real_register(unsigned long self, void *dir) {
    ((fn_register) (g_base + FN_REGISTER_DIR))(self, dir);
}

/* at load, before the image is closed */
void shim_ffmenu_install(unsigned char *base) {
    static const uint32_t want[4] = { 0xd10283ffu, 0xa9046ffcu, 0xa90567fau, 0xa9065ff8u };
    const uint32_t *fn = (const uint32_t *) (base + FN_LOAD_CONTENT);
    for (int k = 0; k < 4; k++)
        if (fn[k] != want[k]) { LOG("ffmenu: 0x%x does not start as measured - nothing touched", FN_LOAD_CONTENT); return; }
    uint32_t *reg = (uint32_t *) (base + FN_REGISTER_DIR);
    if ((reg[0] & 0xffc003e0u) != 0xa98003e0u) {         /* stp xA, xB, [sp, #-N]! */
        LOG("ffmenu: 0x%x does not look like a function start - nothing touched", FN_REGISTER_DIR);
        return;
    }
    g_base = base;
    g_register = real_register;
    uint8_t *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) { LOG("ffmenu: no page for the continuation"); return; }
    memcpy(page, fn, 16);                       /* the four words the jump will cover */
    shim_hook(page + 16, (void *) (fn + 4));    /* ...and on to the fifth */
    if (mprotect(page, 4096, PROT_READ | PROT_EXEC) != 0) { LOG("ffmenu: mprotect failed"); munmap(page, 4096); return; }
    __builtin___clear_cache((char *) page, (char *) page + 32);
    o_load = (fn_load) page;
    shim_hook((void *) fn, (void *) w_load);
    LOG("ffmenu: the content loader is wrapped");
}
#endif
