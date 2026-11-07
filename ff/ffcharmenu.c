/*
 * FIEND FOLIO - THE CHARACTER SELECT SCREEN, drawn by this code (no mods/ folder, no data the engine has to find).
 *
 * WHAT THE ENGINE DOES (read in the image, sha 217be349..., offsets = addresses; the menu object `m` is the one the
 * functions below receive in x0):
 *   - the first 18 slots are the vanilla characters. Their art comes from a static table at 0xa8cb58 (variant stride 0x240,
 *     entry stride 0x20: {const char *animation "NN_Name", u32 player id, flag}). The centre picture is the sprite at m+0x78
 *     (CharacterMenu.anm2) told to play that animation by the update 0x5e65a8 (the call is at 0x5e6bd8); each slot of the
 *     ring is the sprite at m+0xf60 (CharacterPortraits.anm2) playing the same name, drawn by the render 0x5e7890
 *     (0x5e93b8 .. 0x5e9568).
 *   - slots 0x12 and up are the characters of players.xml from id 41 (0x5e5854..0x5e59d0 builds the list from the player
 *     table [[0xac2428]+0x56168], stride 0x178, skipping the entries flagged at +0x13c or with a text at +0x120). The list is
 *     m+0x1560 .. m+0x1568, 8 bytes per entry: the player id of the normal variant and, in the high half, the id of the
 *     alternate (tainted) one; the variant being shown is the int at m+0x1298. For these slots the update plays NOTHING on
 *     m+0x78 (the branch at 0x5e6bb0 skips it), and the render hands the ring slot to 0x77597c(obj, id, pos, tint, scale),
 *     which draws a sprite of the player's own record (a name label), not the portrait: that is why they came out empty even
 *     with every file in place (the log of the run shows the game asking for CharacterMenu.anm2, its sheet and
 *     CharacterPortraits.anm2, all present).
 *
 * WHAT THIS FILE DOES: the same thing the engine does for the vanilla slots, for ids 41..60:
 *   - after the update 0x5e65a8, if the selected slot is one of ours, it plays that character's animation on m+0x78;
 *   - while the render 0x5e7890 runs, 0x77597c is routed here: for our ids it plays the animation on m+0xf60, copies the
 *     tint and scale the way 0x77597c does for its own sprite (+0xe8, 0x30 bytes; +0xdc, a vec2) and draws it at `pos`.
 *     Any other id, or a call outside that render, goes to the original.
 * The animation names are those of Fiend Folio's own CharacterMenu.anm2 / CharacterPortraits.anm2 (the player's name; the
 * alternate files use the same names).
 *
 * NOT MEASURED: that the centre sprite is drawn for slots >= 0x12 once it plays something (the render has one draw for it
 * that this reading did not check as unconditional); the log says what ran, tag "isaac", lines "ffcharmenu:".
 */
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <android/log.h>
#include "diag.h"

#include "ffsay.h"

void shim_hook(void *at, void *to);

#define FN_UPDATE        0x5e65a8      /* (menu) */
#define FN_RENDER        0x5e7890      /* (menu) */
#define FN_NAME_DRAW     0x77597c      /* (obj, u32 id, vec2 *pos, void *tint, vec2 *scale) */
#define FN_PLAY          0x89e82c      /* (sprite, const char *, int) */
#define FN_SPRITE_RENDER 0x89e4c0      /* (sprite, vec2 *pos, vec2 *, vec2 *) */
#define VEC2_ZERO        0x95e7a0      /* the constant 0x77597c passes as the two last arguments */

#define M_SELECTED   0x40
#define M_VARIANT    0x1298
#define M_LIST_BEGIN 0x1560
#define M_LIST_END   0x1568
#define M_CENTRE     0x78
#define M_RING       0xf60
#define S_SCALE      0xdc
#define S_TINT       0xe8
#define TINT_BYTES   0x2c
#define FIRST_EXTRA  0x12

#define FF_FIRST 41
#define FF_COUNT 20

/* players.xml order of Fiend Folio (content-repentogon/players.xml): the alternates carry the animation of their normal one */
static const char *const FF_ANIM[FF_COUNT] = {
    "Fiend", "Fiend", "Golem", "Golem", "Friend", "Friend", "Inaba", "Inaba", "Slippy", "China",
    "Craig", "Classic Fiend", "Image", "The Skeletal Fiend", "Fend", "Frond", "Fient", "Mi", "Freezer Jr.", "Peat"
};

const char *ffcharmenu_anim_of(unsigned id) { return (id >= FF_FIRST && id < FF_FIRST + FF_COUNT) ? FF_ANIM[id - FF_FIRST] : 0; }

typedef void (*fn_menu)(unsigned long);
typedef void (*fn_name_draw)(unsigned long, unsigned, const float *, const void *, const float *);
typedef void (*fn_play)(void *, const char *, int);
typedef void (*fn_srender)(void *, const float *, const float *, const float *);

static unsigned char *g_base;
static fn_menu o_update, o_render;
static fn_name_draw o_name_draw;
static fn_play g_play;
static fn_srender g_srender;
static unsigned long g_in_render;                       /* the menu object while 0x5e7890 runs, else 0 */
static int g_log_update, g_log_ring, g_log_centre;

/* the id shown in slot `sel` (>= 0x12) for the variant on screen, 0 if there is none */
static unsigned id_of_slot(unsigned long m, int sel) {
    if (sel < FIRST_EXTRA) return 0;
    unsigned long b = *(const unsigned long *) (m + M_LIST_BEGIN), e = *(const unsigned long *) (m + M_LIST_END);
    if (!b || e < b) return 0;
    unsigned idx = (unsigned) (sel - FIRST_EXTRA);
    if (idx >= (e - b) / 8) return 0;
    int v = *(const int *) (m + M_VARIANT);
    if (v < 0 || v > 1) return 0;
    return ((const unsigned *) (b + 8ul * idx))[v];
}

static void w_update(unsigned long m) {
    o_update(m);
    int sel = *(const int *) (m + M_SELECTED);
    if (g_log_update < 3) {
        unsigned long b = *(const unsigned long *) (m + M_LIST_BEGIN), e = *(const unsigned long *) (m + M_LIST_END);
        LOG("ffcharmenu: menu update, slot %d, variant %d, %lu extra characters in the list", sel,
            *(const int *) (m + M_VARIANT), e >= b ? (e - b) / 8 : 0ul);
        g_log_update++;
    }
    unsigned id = id_of_slot(m, sel);
    const char *a = ffcharmenu_anim_of(id);
    if (!a) return;
    g_play((void *) (m + M_CENTRE), a, 0);
    if (g_log_centre < 4) { LOG("ffcharmenu: centre picture plays \"%s\" (id %u)", a, id); g_log_centre++; }
}

static void w_render(unsigned long m) {
    unsigned long old = g_in_render;
    g_in_render = m;
    o_render(m);
    g_in_render = old;
}

static void w_name_draw(unsigned long obj, unsigned id, const float *pos, const void *tint, const float *scale) {
    const char *a = g_in_render ? ffcharmenu_anim_of(id) : 0;
    if (!a) { o_name_draw(obj, id, pos, tint, scale); return; }
    unsigned char *s = (unsigned char *) (g_in_render + M_RING);
    g_play(s, a, 1);
    memcpy(s + S_TINT, tint, TINT_BYTES);
    memcpy(s + S_SCALE, scale, 8);
    g_srender(s, pos, (const float *) (g_base + VEC2_ZERO), (const float *) (g_base + VEC2_ZERO));
    if (g_log_ring < 6) { LOG("ffcharmenu: ring slot drawn with \"%s\" (id %u) at %.0f,%.0f", a, id, pos[0], pos[1]); g_log_ring++; }
}

/* a detour on `fn` that keeps the four words it overwrites; returns the entry to call the original, or 0 */
static void *wrap(unsigned char *base, unsigned off, const uint32_t want[4], void *to, const char *what) {
    const uint32_t *fn = (const uint32_t *) (base + off);
    for (int k = 0; k < 4; k++)
        if (fn[k] != want[k]) { LOG("ffcharmenu: %s at 0x%x does not start as measured - nothing touched", what, off); return 0; }
    uint8_t *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) return 0;
    memcpy(page, fn, 16);
    shim_hook(page + 16, (void *) (fn + 4));
    if (mprotect(page, 4096, PROT_READ | PROT_EXEC) != 0) { munmap(page, 4096); return 0; }
    __builtin___clear_cache((char *) page, (char *) page + 32);
    shim_hook((void *) fn, to);
    return page;
}

void shim_ffcharmenu_install(unsigned char *base) {
    static const uint32_t w_upd[4] = { 0xd10443ffu, 0x6d073befu, 0x6d0833edu, 0x6d092bebu };
    static const uint32_t w_ren[4] = { 0xa9ba6ffcu, 0xa90167fau, 0xa9025ff8u, 0xa90357f6u };
    static const uint32_t w_nam[4] = { 0xd10183ffu, 0xa9025ff8u, 0xa90357f6u, 0xa9044ff4u };
    if (o_update) return;
    g_base = base;
    g_play = (fn_play) (base + FN_PLAY);
    g_srender = (fn_srender) (base + FN_SPRITE_RENDER);
    /* all three or none: a half-installed menu would play animations nobody draws */
    for (int k = 0; k < 4; k++) {
        if (((const uint32_t *) (base + FN_UPDATE))[k] != w_upd[k] || ((const uint32_t *) (base + FN_RENDER))[k] != w_ren[k] ||
            ((const uint32_t *) (base + FN_NAME_DRAW))[k] != w_nam[k]) {
            LOG("ffcharmenu: the menu functions do not start as measured - nothing touched");
            return;
        }
    }
    o_name_draw = (fn_name_draw) wrap(base, FN_NAME_DRAW, w_nam, (void *) w_name_draw, "name draw");
    o_render = (fn_menu) wrap(base, FN_RENDER, w_ren, (void *) w_render, "render");
    o_update = (fn_menu) wrap(base, FN_UPDATE, w_upd, (void *) w_update, "update");
    LOG("ffcharmenu: character select wrapped (update %s, render %s, ring %s)", o_update ? "yes" : "NO", o_render ? "yes" : "NO",
        o_name_draw ? "yes" : "NO");
}
