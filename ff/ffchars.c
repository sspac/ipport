/*
 * FIEND FOLIO CHARACTERS, WITHOUT LUA - stage 1: stats (1a), tear flags and colours (1b).
 *
 * Fiend Folio (PC, Lua) gives every character its stats inside MC_EVALUATE_CACHE callbacks that run
 * AFTER the game has worked a stat out. This file does the same thing natively: it wraps the game's
 * EvaluateItems (0x4f0ee4, player) - the function that recomputes a player's stats - and, once the
 * original is done, applies the character's changes to the stats that were asked for.
 *
 * WHAT IS MEASURED (statsplus.c and the image, 02-03.11.2026):
 *   player+0x1738 type, +0x1834 MaxFireDelay, +0x1838 ShotSpeed, +0x1844 Damage, +0x1854 TearRange,
 *   +0x194c MoveSpeed, +0x1950 Luck, +0x1958 the stat flags waiting for EvaluateItems (it clears them),
 *   +0x1dbc the D8 range multiplier.
 *   1b: +0x1858 TearFlags (a 128-bit set, 16 bytes; the tear-flag section of EvaluateItems resets it
 *       and ORs one constant per item straight into it), +0x1868 TearColor and +0x1894 LaserColor.
 *   A Color is 0x2c bytes, found from its constructor (0x7f3338) and setters (0x7f3350/335c/3368):
 *       +0x00 tint r,g,b,a    +0x10 colorize r,g,b,a    +0x20 offset r,g,b
 *   The flag bits are the game's own, read from the section tests of EvaluateItems (0x4f1480, 0x4f4960,
 *   0x4f53ac, 0x4f5e58, 0x4fa4c0, 0x4fa880): 1 damage, 2 fire delay, 4 shot speed, 8 range, 16 speed,
 *   32 tear flags, 64 tear colour, 1<<10 luck, 1<<11 size.
 *   The tear-flag BIT NUMBERS are the PC enum's. Checked against the image: the constant ORed for Fire
 *   Mind (257) is bit 22 (BURN), Godhead (331) sets bits 2 and 31 (HOMING, GLOW), Mysterious Liquid
 *   (317) bit 33. Poison (4), Grow (7), Confusion (14) and Shielded (34) are taken from the same enum.
 *   HasCollectible is 0x4eeb74(player, id, 0).
 *
 * WHY THE FLAGS ARE READ FIRST: EvaluateItems clears them, and a stat that was not recomputed must not
 * be changed again - a multiplier applied twice to a damage nobody recomputed would grow every call.
 * The same goes for the colours: they are rebuilt from the default each time bit 64 is evaluated.
 *
 * THE CHARACTERS are ids 41..60, in the order of the merged players.xml. The table is made that large HERE
 * (shim_ffchars_install: the word at 0x7855dc), always, not by a switch of the MODS menu. Biend/Briend/Binaba/Slippy/Image/Freezer Jr/Peat have no stat lines in the
 * mod's player files, or only conditional ones, and stay as the game computes them. The Skeletal Fiend
 * (54) is an Epiphany-compatibility character: its slot stays so the ids do not move, its numbers do not.
 *
 * COLOURS: the mod writes "player.TearColor * X". With the default colour underneath (the usual case:
 * nothing else tinted the tears) that is X itself, and that is what is written. If an item tinted the
 * tears first, the product is approximated (tints multiplied, X's offset added after X's tint, X's
 * colorize kept when it has alpha) - the game's own Color multiplication was not measured.
 *
 * NOT HERE YET: size (China x0.8 is entity SizeMulti, not found yet), laser/tear colours that depend on
 * the mod's state (Briend's grit, Biend's copy of its parent, Friend's coloured shots), the things that
 * depend on the mod's own save data (Inaba's power, Golem's rocks, Briend's guns) and every mechanic
 * that is not a cache stat (Fiend's red/blood tears are made when a tear is FIRED, not here).
 *
 * HOW IT IS HOOKED: the first four words of EvaluateItems (stp x4, position independent, checked
 * below) are copied to a page of ours followed by a jump to the fifth, and the function's start becomes
 * a jump to ff_evaluate. The page is made executable at once.
 */
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <android/log.h>
#include "diag.h"

#include "ffsay.h"

void shim_hook(void *at, void *to);

#define FN_EVALUATE_ITEMS   0x4f0ee4
#define FN_HAS_COLLECTIBLE  0x4eeb74
#define PLAYER_TYPE         0x1738
#define PLAYER_FIRE_DELAY   0x1834
#define PLAYER_SHOT_SPEED   0x1838
#define PLAYER_DAMAGE       0x1844
#define PLAYER_RANGE        0x1854
#define PLAYER_SPEED        0x194c
#define PLAYER_LUCK         0x1950
#define PLAYER_TEAR_FLAGS   0x1858
#define PLAYER_TEAR_COLOR   0x1868
#define PLAYER_LASER_COLOR  0x1894
#define PLAYER_CACHE_FLAGS  0x1958
#define PLAYER_D8_RANGE     0x1dbc

#define CF_DAMAGE     (1u << 0)
#define CF_FIREDELAY  (1u << 1)
#define CF_SHOTSPEED  (1u << 2)
#define CF_RANGE      (1u << 3)
#define CF_SPEED      (1u << 4)
#define CF_TEARFLAG   (1u << 5)
#define CF_TEARCOLOR  (1u << 6)
#define CF_LUCK       (1u << 10)

#define ITEM_NUMBER_ONE     6
#define ITEM_IPECAC         149
#define ITEM_CRICKETS_BODY  224
#define ITEM_HAEMOLACRIA    531
#define ITEM_BIRTHRIGHT     619

#define FF_FIRST  41
#define FF_COUNT  20

/*
 * One line per character, in id order. A zero is "no change" for every field except where a one is
 * the neutral value (the multipliers).
 *   tears_abp   the mod's own number: a FIRE DELAY multiplier written as for Antibirth+ (1.3 = Fiend
 *               shoots slower, 0.75 = China shoots faster). It is converted the way the mod does
 *               (convertTearMultABPToRep) and applied to the fire RATE (tearsMult), never to the fire
 *               delay directly, so it behaves at negative delays.
 *   range_sub   TearRange - range_sub * basemulti, basemulti being the D8 range multiplier x0.8 for
 *               Number One x0.8 for Ipecac / Haemolacria / Cricket's Body.
 */
/* a Color as the game stores it: 11 floats, 0x2c bytes */
typedef struct { float tint[4], colorize[4], offset[3]; } ff_color_t;
_Static_assert(sizeof(ff_color_t) == 0x2c, "a Color is 0x2c bytes in the game");

#define TF(n) (1ull << (n))
#define TF_HOMING     TF(2)
#define TF_POISON     TF(4)
#define TF_GROW       TF(7)
#define TF_CONFUSION  TF(14)
#define TF_SHIELDED   TF(34)

enum { COL_NONE = 0, COL_SET = 1, COL_MUL = 2 };

/* the mod's constants (ffscripts/constants/constants.lua), Color(r,g,b,a,ro,go,bo) + SetColorize */
static const ff_color_t C_CLASSIC_TEAR  = { {1, 1, 0.5f, 1}, {0, 0, 0, 0},            {200 / 255.0f, 0, 0} };
static const ff_color_t C_CLASSIC_LASER = { {1, 1, 0.5f, 1}, {0, 0, 0, 0},            {200 / 255.0f, 100 / 255.0f, 0} };
static const ff_color_t C_CHINA_YELLOW  = { {1, 1, 1, 1},    {0.95f, 0.8f, 0.3f, 1},  {0.4f, 0.3f, 0.1f} };
static const ff_color_t C_CRAIG_TEARS   = { {1, 1, 0.5f, 1}, {1, 1, 0, 1},            {0.4f, 0.3f, 0.1f} };
static const ff_color_t C_ELECTRIC_YEL  = { {0.8f, 1, 1, 1}, {0, 0, 0, 0},            {0.8f, 1, 0} };
static const ff_color_t C_GREYSCALE     = { {1, 1, 1, 1},    {1, 1, 1, 1},            {0, 0, 0} };
static const ff_color_t C_FIEND_GUTS    = { {0.3f, 0.5f, 1, 1}, {1, 1, 2, 1},         {0.35f, 0.15f, 0.35f} };

/*
 * One line per character, in id order. A zero is "no change" for every field except where a one is
 * the neutral value (the multipliers).
 *   tears_abp   the mod's own number: a FIRE DELAY multiplier written as for Antibirth+ (1.3 = Fiend
 *               shoots slower, 0.75 = China shoots faster). It is converted the way the mod does
 *               (convertTearMultABPToRep) and applied to the fire RATE (tearsMult), never to the fire
 *               delay directly, so it behaves at negative delays.
 *   range_sub   TearRange - range_sub * basemulti, basemulti being the D8 range multiplier x0.8 for
 *               Number One x0.8 for Ipecac / Haemolacria / Cricket's Body.
 *   tear_flags  ORed into the player's TearFlags when the game evaluates them.
 *   tear_col / laser_col with their mode (COL_SET writes it, COL_MUL multiplies it into what is there).
 */
typedef struct {
    const char *name;
    float dmg_mul, dmg_add;
    float tears_abp;                /* 0 = none */
    float speed_add;
    float shot_mul, shot_add;
    float luck_add, luck_birthright;/* luck_birthright is used instead when the player has Birthright, if nonzero */
    float range_sub, range_mul;     /* range_mul 0 = none */
    uint64_t tear_flags;
    int tear_mode;  const ff_color_t *tear_col;
    int laser_mode; const ff_color_t *laser_col;
} ff_char_t;

#define NOFX 0, 0, 0, 0, 0        /* no tear flags, no tear colour, no laser colour */
static const ff_char_t g_chars[FF_COUNT] = {
    /* 41 */ { "Fiend",          1.2f, 0, 1.3f, 0.2f,  1, 0.25f, -1,  0,   0,   0, NOFX },
    /* 42 */ { "Biend",          1,    0, 0,    0,     1, 0,      0,  0,   0,   0, NOFX },
    /* 43 */ { "Golem",          1,    0, 0,   -0.15f, 1, 0,     -1,  1,  20,   0, NOFX },
    /* 44 */ { "Bolem",          1,    0.7f, 0, 0,     1, 0,     -1,  0,  20,   0, NOFX },
    /* 45 */ { "Friend",         1.4f, 0, 1.2f, 0,     1, 1,     -1,  0,   0,   0, NOFX },   /* luck -2 + the Lucky Penny's +1 */
    /* 46 */ { "Briend",         1,    0, 0,    0,     1, 1,      0,  0,   0,   0, NOFX },
    /* 47 */ { "Inaba",          0.75f,0, 0,    0,     1, 0,      0,  0,   0,   0, NOFX },
    /* 48 */ { "Binaba",         0.95f,0, 0,    0,     1, 0,      0,  0,   0,   0, NOFX },
    /* 49 */ { "Slippy",         1,    0, 0,    0,     1, 0,      0,  0,   0,   0, NOFX },
    /* 50 */ { "China",          1,    0, 0.75f, 0,    1, 0,      0,  0,   0,   0, 0, COL_MUL, &C_CHINA_YELLOW, 0, 0 },
    /* 51 */ { "Craig",          0.5f, 0, 1.5f, 0.3f,  1, 0,      0,  0,   0,   0.5f, 0, COL_MUL, &C_CRAIG_TEARS, COL_MUL, &C_ELECTRIC_YEL },
    /* 52 */ { "Classic Fiend",  1,    0, 0,    0,     1, 0,      0,  0,   0,   0, TF_HOMING, COL_SET, &C_CLASSIC_TEAR, COL_SET, &C_CLASSIC_LASER },
    /* 53 */ { "Image",          1,    0, 0,    0,     1, 0,      0,  0,   0,   0, NOFX },
    /* 54 */ { "Skeletal Fiend", 1,    0, 0,    0,     1, 0,      0,  0,   0,   0, NOFX },   /* Epiphany compatibility: left as the game computes it */
    /* 55 */ { "Fend",           0.2f, 0, 0,    0,     1, 0,      0,  0,   0,   0, TF_SHIELDED, 0, 0, 0, 0 },
    /* 56 */ { "Frond",          0.5f, 0, 1.9f, 0.2f,  0.5f, 0,   0,  0, 100,   0, TF_POISON | TF_GROW, 0, 0, 0, 0 },
    /* 57 */ { "Fient",          4.057f,0, 3.0f, 0,    0.42f, 0,  0,  0,  92,   0, TF_CONFUSION, COL_MUL, &C_GREYSCALE, 0, 0 },
    /* 58 */ { "Mi",             0.8f, 0, 0.8f, 0,     0.6f, 0,   0,  0,   0,   0, 0, COL_MUL, &C_FIEND_GUTS, 0, 0 },
    /* 59 */ { "Freezer Jr.",    1,    0, 0,    0,     1, 0,      0,  0,   0,   0, NOFX },
    /* 60 */ { "Peat",           1,    0, 0,    0,     1, 0,      0,  0,   0,   0, NOFX },
};

typedef int (*has_fn)(unsigned long player, int id, int ignore);
static has_fn g_has;
typedef void (*eval_fn)(unsigned long player);
static eval_fn g_eval;

/*
 * The mod's tearsMult(delay, convertTearMultABPToRep(m)), exactly:
 *   rate = 30 / (delay + 1);  conv = rate(10 m) / rate(10) = 11 / (10 m + 1);
 *   newDelay = max(30 / (rate * conv) - 1, -0.99)
 */
static float tears_mult_abp(float delay, float m) {
    float conv = 11.0f / (10.0f * m + 1.0f);
    float rate = 30.0f / (delay + 1.0f);
    float now = rate * conv;
    float d = 30.0f / now - 1.0f;
    return d < -0.99f ? -0.99f : d;
}

static int color_is_default(const ff_color_t *c) {
    static const ff_color_t d = { {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0} };
    return memcmp(c, &d, sizeof d) == 0;
}

/* "player.TearColor = player.TearColor * x" / "= x". Underneath a default colour the product is x itself. */
static void ff_color(unsigned long addr, int mode, const ff_color_t *x) {
    ff_color_t *c = (ff_color_t *) addr;
    if (mode == COL_NONE) return;
    if (mode == COL_SET || color_is_default(c)) { *c = *x; return; }
    ff_color_t r;
    for (int i = 0; i < 4; i++) r.tint[i] = c->tint[i] * x->tint[i];
    for (int i = 0; i < 3; i++) r.offset[i] = c->offset[i] * x->tint[i] + x->offset[i];
    for (int i = 0; i < 4; i++) r.colorize[i] = x->colorize[3] > 0 ? x->colorize[i] : c->colorize[i];
    *c = r;
}

static float range_basemulti(unsigned long p) {
    float m = *(const float *) (p + PLAYER_D8_RANGE);
    if (g_has(p, ITEM_NUMBER_ONE, 0)) m *= 0.8f;
    if (g_has(p, ITEM_IPECAC, 0) || g_has(p, ITEM_HAEMOLACRIA, 0) || g_has(p, ITEM_CRICKETS_BODY, 0)) m *= 0.8f;
    return m;
}

/* the stats the game asked for (fl) get the character's changes; everything else is left alone */
static void ff_apply(unsigned long p, const ff_char_t *c, unsigned fl) {
    if (fl & CF_DAMAGE) {
        float *d = (float *) (p + PLAYER_DAMAGE);
        *d = *d * c->dmg_mul + c->dmg_add;
    }
    if ((fl & CF_FIREDELAY) && c->tears_abp > 0) {
        float *f = (float *) (p + PLAYER_FIRE_DELAY);
        *f = tears_mult_abp(*f, c->tears_abp);
    }
    if (fl & CF_SHOTSPEED) {
        float *s = (float *) (p + PLAYER_SHOT_SPEED);
        *s = *s * c->shot_mul + c->shot_add;
    }
    if (fl & CF_SPEED) {
        *(float *) (p + PLAYER_SPEED) += c->speed_add;
    }
    if (fl & CF_RANGE) {
        float *r = (float *) (p + PLAYER_RANGE);
        if (c->range_mul > 0) *r *= c->range_mul;
        if (c->range_sub != 0) *r -= c->range_sub * range_basemulti(p);
    }
    if (fl & CF_LUCK) {
        float l = c->luck_add;
        if (c->luck_birthright != 0 && g_has(p, ITEM_BIRTHRIGHT, 0)) l = c->luck_birthright;
        *(float *) (p + PLAYER_LUCK) += l;
    }
    if ((fl & CF_TEARFLAG) && c->tear_flags) {
        *(uint64_t *) (p + PLAYER_TEAR_FLAGS) |= c->tear_flags;         /* low half of the 128-bit set */
    }
    if (fl & CF_TEARCOLOR) {
        ff_color(p + PLAYER_TEAR_COLOR, c->tear_mode, c->tear_col);
        ff_color(p + PLAYER_LASER_COLOR, c->laser_mode, c->laser_col);
    }
}

/* every call of the game's EvaluateItems lands here */
static void ff_evaluate(unsigned long p) {
    unsigned fl = *(const unsigned *) (p + PLAYER_CACHE_FLAGS);     /* before the original clears it */
    g_eval(p);
    static int last_type = -999;
    int ty = *(const int *) (p + PLAYER_TYPE);
    if (ty != last_type) { last_type = ty; LOG("ffchars: a player is character %d (FF ones are %d..%d)", ty, FF_FIRST, FF_FIRST + FF_COUNT - 1); }
    int t = ty - FF_FIRST;
    if (t < 0 || t >= FF_COUNT) return;
    ff_apply(p, &g_chars[t], fl);
}

#ifndef FF_HOST_TEST
int shim_patch_word(unsigned long off, uint32_t was_a, uint32_t was_b, uint32_t now);   /* loader.c */

/*
 * THE CHARACTER SLOTS, ALWAYS ON. The game fills its table of characters with resize(41) - a "mov w1,#41" at
 * 0x7855dc (0x52800521). Fiend Folio's 20 characters are ids 41..60, so it becomes "mov w1,#61" (0x528007a1).
 * This was Yama's "charslots" group, a switch of the MODS menu; it is part of the game's build now, with no
 * switch: the word may be the original or already ours (Yama's loader writes the same one), anything else
 * and the patch is cancelled by shim_patch_word.
 */
#define FN_CHAR_TABLE_SIZE  0x7855dc
#define MOV_W1_41           0x52800521u
#define MOV_W1_61           0x528007a1u

/* at load, before the image is closed: the slots, and EvaluateItems is ours */
void shim_ffchars_install(unsigned char *base) {
    if (!shim_patch_word(FN_CHAR_TABLE_SIZE, MOV_W1_41, MOV_W1_61, MOV_W1_61))
        LOG("ffchars: the character table could not be widened");
    uint32_t *fn = (uint32_t *) (base + FN_EVALUATE_ITEMS);
    /* stp d15,d14 / d13,d12 / d11,d10 / d9,d8: the image this was measured on; anything else is left alone */
    static const uint32_t want[4] = { 0x6db63befu, 0x6d0133edu, 0x6d022bebu, 0x6d0323e9u };
    for (int i = 0; i < 4; i++)
        if (fn[i] != want[i]) { LOG("ffchars: EvaluateItems does not start as measured - left alone"); return; }
    uint8_t *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) { LOG("ffchars: no page for the continuation"); return; }
    memcpy(page, fn, 16);                       /* the four words the jump will cover */
    shim_hook(page + 16, fn + 4);               /* ...and on to the fifth */
    if (mprotect(page, 4096, PROT_READ | PROT_EXEC) != 0) { LOG("ffchars: mprotect failed"); munmap(page, 4096); return; }
    __builtin___clear_cache((char *) page, (char *) page + 32);
    g_eval = (eval_fn) page;
    g_has = (has_fn) (base + FN_HAS_COLLECTIBLE);
    shim_hook(fn, (void *) ff_evaluate);
    LOG("ffchars: EvaluateItems is ours (%d characters)", FF_COUNT);
}
#endif
