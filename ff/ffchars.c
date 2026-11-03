/*
 * FIEND FOLIO CHARACTERS, WITHOUT LUA - stage 1a: the numbers.
 *
 * Fiend Folio (PC, Lua) gives every character its stats inside MC_EVALUATE_CACHE callbacks that run
 * AFTER the game has worked a stat out. This file does the same thing natively: it wraps the game's
 * EvaluateItems (0x4f0ee4, player) - the function that recomputes a player's stats - and, once the
 * original is done, applies the character's changes to the stats that were asked for.
 *
 * WHAT IS MEASURED (statsplus.c and the image, 02.11.2026):
 *   player+0x1738 type, +0x1834 MaxFireDelay, +0x1838 ShotSpeed, +0x1844 Damage, +0x1854 TearRange,
 *   +0x194c MoveSpeed, +0x1950 Luck, +0x1958 the stat flags waiting for EvaluateItems (it clears them),
 *   +0x1dbc the D8 range multiplier. The flag bits are the game's own, read from the dispatch at
 *   0x4f1480 and the section tests (0x4f4960, 0x4f53ac, 0x4f5e58): 1 damage, 2 fire delay, 4 shot
 *   speed, 8 range, 16 speed, 32 tear flags, 64 tear colour; luck is 1<<10 as on PC.
 *   HasCollectible is 0x4eeb74(player, id, 0).
 *
 * WHY THE FLAGS ARE READ FIRST: EvaluateItems clears them, and a stat that was not recomputed must not
 * be changed again - a multiplier applied twice to a damage nobody recomputed would grow every call.
 *
 * THE CHARACTERS are ids 41..60, in the order of the merged players.xml (the "charslots" patch makes
 * the table that large). Biend/Briend/Binaba/Slippy/Image/Freezer Jr/Peat have no stat lines in the
 * mod's player files, or only conditional ones, and stay as the game computes them.
 *
 * NOT HERE YET (stage 1b, needs the offsets measured first): tear flags, tear colour, laser colour,
 * size (China x0.8), and the things that depend on the mod's own save data (Skeletal Fiend's buff and
 * its achievement-gated +0.1s, Inaba's power, Golem's rocks, Briend's guns).
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

#if ISAAC_DIAG
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "isaac", __VA_ARGS__)
#else
#define LOG(...) do { } while (0)
#endif

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
#define PLAYER_CACHE_FLAGS  0x1958
#define PLAYER_D8_RANGE     0x1dbc

#define CF_DAMAGE     (1u << 0)
#define CF_FIREDELAY  (1u << 1)
#define CF_SHOTSPEED  (1u << 2)
#define CF_RANGE      (1u << 3)
#define CF_SPEED      (1u << 4)
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
typedef struct {
    const char *name;
    float dmg_mul, dmg_add;
    float tears_abp;                /* 0 = none */
    float speed_add;
    float shot_mul, shot_add;
    float luck_add, luck_birthright;/* luck_birthright is used instead when the player has Birthright, if nonzero */
    float range_sub, range_mul;     /* range_mul 0 = none */
} ff_char_t;

static const ff_char_t g_chars[FF_COUNT] = {
    /* 41 */ { "Fiend",          1.2f, 0, 1.3f, 0.2f,  1, 0.25f, -1,  0,   0,   0 },
    /* 42 */ { "Biend",          1,    0, 0,    0,     1, 0,      0,  0,   0,   0 },
    /* 43 */ { "Golem",          1,    0, 0,   -0.15f, 1, 0,     -1,  1,  20,   0 },
    /* 44 */ { "Bolem",          1,    0.7f, 0, 0,     1, 0,     -1,  0,  20,   0 },
    /* 45 */ { "Friend",         1.4f, 0, 1.2f, 0,     1, 1,     -1,  0,   0,   0 },   /* luck -2 + the Lucky Penny's +1 */
    /* 46 */ { "Briend",         1,    0, 0,    0,     1, 1,      0,  0,   0,   0 },
    /* 47 */ { "Inaba",          0.75f,0, 0,    0,     1, 0,      0,  0,   0,   0 },
    /* 48 */ { "Binaba",         0.95f,0, 0,    0,     1, 0,      0,  0,   0,   0 },
    /* 49 */ { "Slippy",         1,    0, 0,    0,     1, 0,      0,  0,   0,   0 },
    /* 50 */ { "China",          1,    0, 0.75f, 0,    1, 0,      0,  0,   0,   0 },
    /* 51 */ { "Craig",          0.5f, 0, 1.5f, 0.3f,  1, 0,      0,  0,   0,   0.5f },
    /* 52 */ { "Classic Fiend",  1,    0, 0,    0,     1, 0,      0,  0,   0,   0 },
    /* 53 */ { "Image",          1,    0, 0,    0,     1, 0,      0,  0,   0,   0 },
    /* 54 */ { "Skeletal Fiend", 1.5f, 0, 1.5f, 0,     1, 0,      0,  0,  92,   0 },
    /* 55 */ { "Fend",           0.2f, 0, 0,    0,     1, 0,      0,  0,   0,   0 },
    /* 56 */ { "Frond",          0.5f, 0, 1.9f, 0.2f,  0.5f, 0,   0,  0, 100,   0 },
    /* 57 */ { "Fient",          4.057f,0, 3.0f, 0,    0.42f, 0,  0,  0,  92,   0 },
    /* 58 */ { "Mi",             0.8f, 0, 0.8f, 0,     0.6f, 0,   0,  0,   0,   0 },
    /* 59 */ { "Freezer Jr.",    1,    0, 0,    0,     1, 0,      0,  0,   0,   0 },
    /* 60 */ { "Peat",           1,    0, 0,    0,     1, 0,      0,  0,   0,   0 },
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
}

/* every call of the game's EvaluateItems lands here */
static void ff_evaluate(unsigned long p) {
    unsigned fl = *(const unsigned *) (p + PLAYER_CACHE_FLAGS);     /* before the original clears it */
    g_eval(p);
    int t = *(const int *) (p + PLAYER_TYPE) - FF_FIRST;
    if (t < 0 || t >= FF_COUNT) return;
    ff_apply(p, &g_chars[t], fl);
}

#ifndef FF_HOST_TEST
/* at load, before the image is closed: EvaluateItems is ours */
void shim_ffchars_install(unsigned char *base) {
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
