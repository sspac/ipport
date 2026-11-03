/*
 * FIEND FOLIO - FIEND'S HEALTH, stage 2a: the rules, with black hearts standing in for immoral ones.
 *
 * WHAT THE MOD DOES (ffscripts/players/fiend.lua, PRE_ADD_HEALTH and GET_MAX_HP_CONVERSION, default config):
 *   - a red heart gained is dropped;
 *   - a heart container (max health) becomes a BLACK heart, the same number of halves;
 *   - any soul-type heart other than the immoral and the black one becomes a black heart;
 *   - the immoral heart is Fiend's own kind of soul heart (a new health type; NOT here yet).
 * It starts with two immoral hearts; players.xml already gives the same two as black ones (black="4").
 *
 * WHAT THIS FILE DOES (stage 2a): the same three conversions, natively, by wrapping the game's own adders (addresses
 * and the field each one writes are from spawner_stats.c, prologues read in the image):
 *   AddMaxHearts 0x4ee8a0(p, halves, ignoreKeeper)   AddHearts 0x4f0338(p, halves, flag)
 *   AddSoulHearts 0x4ef2d0(p, halves)                AddBlackHearts 0x4ef078(p, halves)
 *   AddBoneHearts 0x4ef5c4(p, wholes)
 * only for player type 41 (Fiend; not Biend) and only when health is GAINED (halves > 0): a loss goes
 * through untouched, as a hit does.
 *
 * THE LOOP TO AVOID: AddBlackHearts calls AddSoulHearts (and so does AddBoneHearts) - read in the image.
 * Turning a soul heart into a black one by calling AddBlackHearts would come back through the wrapper
 * of AddSoulHearts and convert again. g_conv is raised for the whole time the game or this file is
 * inside AddBlackHearts / AddBoneHearts / a conversion, and the wrappers let everything through then.
 *
 * THE PICKUPS (stage 2b): the mod turns the hearts that appear on the floor into Immoral Hearts (a purple
 * soul heart, entities 5.1024 / 1025 half / 1026 blended) or Black Hearts, by chance (fiend.lua,
 * POST_PICKUP_INIT). Here, natively:
 *   - Spawn 0x88fcbc: when a heart (5.10.x) has just been made while a Fiend is in the game, the game has
 *     already worked its subtype out (a random heart is subtype 0 at the call). The rules, from the Lua:
 *       one draw u: u < 0.10 -> the BLACK branch, else the IMMORAL branch;
 *       soul / half soul / blended        always converted (black: 10.6, half black 5.1022, blended black 5.1023;
 *                                          immoral: 5.1024, 5.1025, 5.1026);
 *       eternal / golden                  always converted to a full heart of the branch;
 *       bone                              1 in 5 converted to a full one; otherwise it falls to the red rule;
 *       red (full, half, double, scared, rotten)  converted with chance 33 + 33*luck/13 percent
 *                                          (luck = the best Fiend's, kept to 0..13): at 13 luck about 59% immoral and
 *                                          6.6% black overall; a half red becomes a half one.
 *     A converted heart is made again with the new kind (so the game builds its sprite from entities2.xml) and
 *     the first one is withdrawn (exists = 0, dead = 1).
 *   - Entity_Pickup::HandleCollision 0x71e310(pickup, other, low): the five new kinds are taken as the soul
 *     heart they stand for (full 10.3, half 10.8, blended 10.10): for the length of the call the pickup
 *     answers as that heart, and if the game did not take it (a full house of hearts) it is put back.
 *     The game's own code does the rest - the sound, "Collect", removing it - and AddSoulHearts turns it
 *     into a black heart for Fiend (above).
 *
 * THE KINDS' ENTITIES AND ART are in a port mod folder (ff-immoral-hearts/: mod.json, content/entities2.xml,
 * resources/gfx/items/pick ups/, anm2 and png) - see its LEEME. Without it the replacements cannot be made and a
 * converted heart would not look like anything; it is checked by the reader of the first sprite, not here.
 *
 * LEFT AS THE GAME DOES IT (on purpose): bone hearts when collected (the mod: bone orbitals), eternal,
 * golden and rotten hearts when added directly; the rooms where the mod does not convert (a Super Secret
 * Room with the Angel backdrop, RoomForcesSoulHeart); the health bar still draws black hearts, not purple
 * ones (the HUD needs its own hook).
 *
 * NOT HERE: the immoral heart as a health TYPE (it is a black heart for the game), the minions made from
 * the immoral hearts a hit takes, the three orbiting fireballs, crits, bone orbitals, the pickup sounds.
 *
 * The page holding the copied starts of the seven functions is made executable in one go; if ANY of the
 * seven does not start as measured, none is touched.
 */
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

#define PLAYER_TYPE   0x1738
#define FF_FIEND_TYPE 41

#define GAME_PTR          0xac3b90          /* base + this: the pointer to Game (spawner.c) */
#define GAME_PLAYERS      0x25c40
#define GAME_PLAYER_LIST  0x25c50           /* begin, end of the players */
#define FN_PLAYER_AT      0x4da574
#define PLAYER_LUCK       0x1950
#define E_TYPE            0x38
#define E_VARIANT         0x3c
#define E_SUBTYPE         0x40
#define E_EXISTS          0x1c2
#define E_DEAD            0x1c3

#define T_PICKUP          1
#define PICKUP            5
#define V_HEART           10
#define V_HALF_BLACK      1022
#define V_BLENDED_BLACK   1023
#define V_IMMORAL         1024
#define V_HALF_IMMORAL    1025
#define V_BLENDED_IMMORAL 1026
#define ST_FULL 1
#define ST_HALF 2
#define ST_SOUL 3
#define ST_ETERNAL 4
#define ST_BLACK 6
#define ST_GOLDEN 7
#define ST_HALF_SOUL 8
#define ST_BLENDED 10
#define ST_BONE 11

typedef void (*fn_pii)(unsigned long, int);
typedef void (*fn_piii)(unsigned long, int, int);

static fn_piii o_max, o_red;
static fn_pii  o_soul, o_black, o_bone;
static int g_conv;                          /* > 0 while inside a conversion or an adder that calls AddSoulHearts */

static int is_fiend(unsigned long p) { return *(const int *) (p + PLAYER_TYPE) == FF_FIEND_TYPE; }

/* a gain of health that turns into black hearts, the same halves */
static void as_black(unsigned long p, int halves) {
    g_conv++;
    o_black(p, halves);
    g_conv--;
}

static void w_max(unsigned long p, int halves, int ignore) {
    if (halves > 0 && !g_conv && is_fiend(p)) { as_black(p, halves); return; }
    o_max(p, halves, ignore);
}

static void w_red(unsigned long p, int halves, int flag) {
    if (halves > 0 && is_fiend(p)) return;                  /* dropped */
    o_red(p, halves, flag);
}

static void w_soul(unsigned long p, int halves) {
    if (halves > 0 && !g_conv && is_fiend(p)) { as_black(p, halves); return; }
    o_soul(p, halves);
}

static void w_black(unsigned long p, int halves) {          /* calls AddSoulHearts inside */
    g_conv++;
    o_black(p, halves);
    g_conv--;
}

static void w_bone(unsigned long p, int wholes) {           /* calls AddSoulHearts inside */
    g_conv++;
    o_bone(p, wholes);
    g_conv--;
}

/* ------------------------------------------------------------------------------ the pickups (2b) */
typedef unsigned long (*fn_spawn)(unsigned long, int, int, const float *, const float *, unsigned long, int, int, int);
typedef int (*fn_collide)(unsigned long, unsigned long, int);
static fn_spawn o_spawn;
static fn_collide o_collide;
static unsigned char *g_base;
static int g_in_spawn;

/* is any player a Fiend, and the best Fiend's luck; the game's own list of players (spawner.c) */
static int real_fiend_luck(float *luck) {
    if (!g_base) return 0;
    unsigned long game = *(const unsigned long *) (g_base + GAME_PTR);
    if (!game) return 0;
    const unsigned long *list = (const unsigned long *) (game + GAME_PLAYER_LIST);
    if (!list[0] || list[1] < list[0]) return 0;
    unsigned n = (unsigned) ((list[1] - list[0]) / 8);
    int found = 0;
    float best = -1000.0f;
    for (unsigned i = 0; i < n && i < 8; i++) {
        unsigned long p = ((unsigned long (*)(unsigned long, int)) (g_base + FN_PLAYER_AT))(game + GAME_PLAYERS, (int) i);
        if (!p || !is_fiend(p)) continue;
        float l = *(const float *) (p + PLAYER_LUCK);
        if (!found || l > best) best = l;
        found = 1;
    }
    if (found) *luck = best;
    return found;
}
static int (*g_fiend_luck)(float *) = real_fiend_luck;      /* the host test puts its own here */

/* the Lua's RNG is not reproduced; a xorshift of the pickup's seed gives the same odds */
static unsigned xs(unsigned *s) { unsigned x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }

/*
 * What a heart of this subtype becomes for a Fiend with this luck: 0 = stays, 1 = becomes (*nv, *ns).
 * The order of the draws follows fiend.lua: the branch first, then the bone's 1 in 5, then the red chance.
 */
static int morph_decide(int sub, unsigned seed, float luck, int *nv, int *ns) {
    unsigned s = seed ? seed ^ 0x9e3779b9u : 0x9e3779b9u;
    xs(&s); xs(&s);
    int black = (xs(&s) % 1000u) < 100u;                    /* u < 0.10 */
    float hl = luck < 0 ? 0 : luck > 13 ? 13 : luck;
    switch (sub) {
    case ST_BLACK: return 0;
    case ST_SOUL:        if (black) { *nv = V_HEART; *ns = ST_BLACK; } else { *nv = V_IMMORAL; *ns = 0; } return 1;
    case ST_HALF_SOUL:   *nv = black ? V_HALF_BLACK : V_HALF_IMMORAL; *ns = 0; return 1;
    case ST_BLENDED:     *nv = black ? V_BLENDED_BLACK : V_BLENDED_IMMORAL; *ns = 0; return 1;
    case ST_ETERNAL:
    case ST_GOLDEN:      if (black) { *nv = V_HEART; *ns = ST_BLACK; } else { *nv = V_IMMORAL; *ns = 0; } return 1;
    default: break;
    }
    if (sub == ST_BONE && xs(&s) % 5u == 0u) {              /* 1 in 5 */
        if (black) { *nv = V_HEART; *ns = ST_BLACK; } else { *nv = V_IMMORAL; *ns = 0; }
        return 1;
    }
    if ((int) (xs(&s) % 99u) + 1 <= (int) (33.0f + 33.0f * hl / 13.0f)) {     /* 33 + 33 * luck/13 */
        if (sub == ST_HALF) { *nv = black ? V_HALF_BLACK : V_HALF_IMMORAL; *ns = 0; }
        else if (black)     { *nv = V_HEART; *ns = ST_BLACK; }
        else                { *nv = V_IMMORAL; *ns = 0; }
        return 1;
    }
    return 0;
}

static unsigned long w_spawn(unsigned long game, int type, int variant, const float *pos, const float *vel,
                             unsigned long spawner, int subtype, int seed, int flag) {
    unsigned long e = o_spawn(game, type, variant, pos, vel, spawner, subtype, seed, flag);
    if (!e || g_in_spawn || type != PICKUP || variant != V_HEART) return e;
    float luck;
    if (!g_fiend_luck(&luck)) return e;
    int nv, ns;
    if (!morph_decide(*(const int *) (e + E_SUBTYPE), (unsigned) seed, luck, &nv, &ns)) return e;
    g_in_spawn++;
    unsigned long ne = o_spawn(game, PICKUP, nv, pos, vel, spawner, ns, seed, flag);
    g_in_spawn--;
    if (!ne) return e;                                      /* could not be made: the heart stays as it was */
    *(unsigned char *) (e + E_DEAD) = 1;                    /* the first one is withdrawn */
    *(unsigned char *) (e + E_EXISTS) = 0;
    return ne;
}

/* the five new kinds answer as the soul heart they stand for, for the length of the game's own collision */
static int soul_form(int variant) {
    switch (variant) {
    case V_IMMORAL:         return ST_SOUL;
    case V_HALF_IMMORAL:
    case V_HALF_BLACK:      return ST_HALF_SOUL;
    case V_BLENDED_IMMORAL:
    case V_BLENDED_BLACK:   return ST_BLENDED;
    default:                return 0;
    }
}

static int w_collide(unsigned long pk, unsigned long other, int low) {
    int variant = *(const int *) (pk + E_VARIANT);
    int sub = soul_form(variant);
    if (!sub || *(const int *) (other + E_TYPE) != 1 /* the player: type 1 */) return o_collide(pk, other, low);
    int was = *(const int *) (pk + E_SUBTYPE);
    *(int *) (pk + E_VARIANT) = V_HEART;
    *(int *) (pk + E_SUBTYPE) = sub;
    int r = o_collide(pk, other, low);
    if (!*(const unsigned char *) (pk + E_DEAD)) {          /* not taken: it is what it was */
        *(int *) (pk + E_VARIANT) = variant;
        *(int *) (pk + E_SUBTYPE) = was;
    }
    return r;
}

#ifndef FF_HOST_TEST
typedef struct { unsigned off; uint32_t want[4]; void *wrapper; void **orig; } det_t;

static det_t g_det[7] = {
    { 0x4ee8a0, { 0xa9bd57f6u, 0xa9014ff4u, 0xa9027bfdu, 0x910083fdu }, (void *) w_max,   (void **) &o_max   },
    { 0x4f0338, { 0xa9bd57f6u, 0xa9014ff4u, 0xa9027bfdu, 0x910083fdu }, (void *) w_red,   (void **) &o_red   },
    { 0x4ef2d0, { 0xa9ba6ffcu, 0xa90167fau, 0xa9025ff8u, 0xa90357f6u }, (void *) w_soul,  (void **) &o_soul  },
    { 0x4ef078, { 0xa9bc5ff8u, 0xa90157f6u, 0xa9024ff4u, 0xa9037bfdu }, (void *) w_black, (void **) &o_black },
    { 0x4ef5c4, { 0xa9bc5ff8u, 0xa90157f6u, 0xa9024ff4u, 0xa9037bfdu }, (void *) w_bone,  (void **) &o_bone  },
    { 0x88fcbc, { 0xd10283ffu, 0xa9046ffcu, 0xa90567fau, 0xa9065ff8u }, (void *) w_spawn,   (void **) &o_spawn   },
    { 0x71e310, { 0xd10783ffu, 0x6d162bebu, 0x6d1723e9u, 0xa9186ffcu }, (void *) w_collide, (void **) &o_collide },
};
#define N_DET 7

/* at load, before the image is closed */
void shim_ffheart_install(unsigned char *base) {
    g_base = base;
    for (int i = 0; i < N_DET; i++) {
        const uint32_t *fn = (const uint32_t *) (base + g_det[i].off);
        for (int k = 0; k < 4; k++)
            if (fn[k] != g_det[i].want[k]) { LOG("ffheart: 0x%x does not start as measured - nothing touched", g_det[i].off); return; }
    }
    uint8_t *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) { LOG("ffheart: no page for the continuations"); return; }
    for (int i = 0; i < N_DET; i++) {
        uint32_t *fn = (uint32_t *) (base + g_det[i].off);
        uint8_t *slot = page + i * 32;
        memcpy(slot, fn, 16);                   /* the four words the jump will cover */
        shim_hook(slot + 16, fn + 4);           /* ...and on to the fifth */
    }
    if (mprotect(page, 4096, PROT_READ | PROT_EXEC) != 0) { LOG("ffheart: mprotect failed"); munmap(page, 4096); return; }
    __builtin___clear_cache((char *) page, (char *) page + N_DET * 32);
    for (int i = 0; i < N_DET; i++) *g_det[i].orig = page + i * 32;
    for (int i = 0; i < N_DET; i++) shim_hook(base + g_det[i].off, g_det[i].wrapper);
    LOG("ffheart: Fiend's health rules and heart pickups are on");
}
#endif
