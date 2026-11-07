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
 * THE KINDS' ENTITIES AND ART ship INSIDE THE GAME, not as a mod: the five entities are appended to the
 * game's entities2.xml (integrar_ff.sh builds the whole file from the game's own pack and index.txt replaces
 * it with an "over" row), and the art sits in assets/fiendfolio/gfx/items/pick ups/ with a "file" row each.
 * Without them the replacements cannot be made and a converted heart would not look like anything.
 *
 * THE IMMORAL HEART AS A KIND OF HEALTH OF ITS OWN (stage 2c) - for EVERY character, not for Fiend only.
 * Under the hood it is a soul-type heart (that is how the mod's CustomHealthAPI builds it too: RegisterSoulHealth),
 * so the game's own rules - the order hearts are lost in, the 12 of them at most, the pickup's reach - apply. What
 * makes it its own kind is a MASK THE GAME DOES NOT HAVE: one bit per whole soul heart, kept here per player
 * (g_pl), parallel to the game's own black mask (+0x16c8). A soul heart is then one of three: blue (neither bit),
 * black (the game's bit) or immoral (ours). Every change of soul health goes through AddSoulHearts (a hit too: the
 * game takes the LAST halves, whatever their colour), so keeping the mask is mirroring that one function:
 *   - a gain made while an Immoral Heart pickup is collected (g_immoral) marks the hearts it creates immoral; any
 *     other gain marks them not immoral; a half that only completes the last heart leaves its kind as it was;
 *   - a loss clears the bits of the hearts that are gone; no soul halves at all clears the mask.
 * The mask is also trimmed to the hearts that exist whenever the bar is drawn, so a path that writes the game's
 * counters without this function (a character swap, a new run) cannot leave a heart marked that is not there.
 * KNOWN LIMIT: the mask lives in this process, not in the save: a Continue turns immoral hearts into blue ones.
 *
 * FIEND'S OWN RULES on top (fiend.lua): a direct soul heart or a container is BLACK; he starts with two black
 * hearts (players.xml black="4") and ONE immoral heart (AddImmoralHearts(player, 2): two halves - one heart): given
 * by w_black the first time a Fiend with no soul hearts is handed those four black halves.
 *
 * THE HEALTH BAR: the game builds the row of hearts it draws as a table of slots, 0x18 bytes each, in the function
 * at 0x6e45d8(layout, slots, count, player) - read in the image: one slot per heart in order; the soul-type slots
 * name their animation at +0x08, "BlueHeartFull" (0xa5aadb) / "BlueHeartHalf" (0xa5aacd) for a blue one and
 * "BlackHeartFull" / "BlackHeartHalf" for a black one. After the game has built the table, the n-th soul-type slot
 * of any player whose mask has bit n gets "ImmoralHeartFull" / "ImmoralHeartHalf". Those animations are not in the
 * game's ui_hearts.anm2: an anm2 patch of the port (portpatch.c) ADDS them with the sheet ui_fiendish_heart.png
 * (integrar_ff.sh). If it is not in the build the animation does not exist and immoral hearts are drawn empty.
 *
 * LEFT AS THE GAME DOES IT (on purpose): bone hearts when collected (the mod: bone orbitals), eternal,
 * golden and rotten hearts when added directly; the rooms where the mod does not convert (a Super Secret
 * Room with the Angel backdrop, RoomForcesSoulHeart).
 *
 * NOT HERE: immoral hearts appearing as ordinary drops for every character (the drop table), the mask in the save, the minions made from
 * the immoral hearts a hit takes, the three orbiting fireballs, crits, bone orbitals, the pickup sounds.
 *
 * The page holding the copied starts of the eight functions is made executable in one go; if ANY of the
 * eight does not start as measured, none is touched.
 */
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <android/log.h>
#include "diag.h"

#include "ffsay.h"

void shim_hook(void *at, void *to);

#define PLAYER_TYPE   0x1738
#define P_SOUL        0x16c4                /* soul-type halves, black ones inside */
#define P_BLACK_MASK  0x16c8                /* bit i: whole soul heart i is black (AddBlackHearts writes it, 0x4ef230) */
#define P_RED         0x16b8                /* red halves */
#define V_MINION      750
#define STR_BLUE_HALF 0xa5aacd              /* "BlueHeartHalf" in the image: a plain soul heart */
#define STR_BLUE_FULL 0xa5aadb              /* "BlueHeartFull" */
#define STR_BLACK_HALF 0xa5aaaf             /* "BlackHeartHalf" */
#define STR_BLACK_FULL 0xa5aabe             /* "BlackHeartFull" */
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
static int g_immoral;                       /* > 0 while an Immoral Heart pickup is being collected */

/* the immoral mask of each player: one bit per whole soul heart, in the order of the hearts */
#define MAX_PL 8
static struct { unsigned long p; unsigned mask; } g_pl[MAX_PL];

static unsigned *imm_slot(unsigned long p) {
    int free = -1;
    for (int i = 0; i < MAX_PL; i++) {
        if (g_pl[i].p == p) return &g_pl[i].mask;
        if (!g_pl[i].p && free < 0) free = i;
    }
    int i = free >= 0 ? free : 0;                       /* a ninth player does not exist; never overrun */
    g_pl[i].p = p; g_pl[i].mask = 0;
    return &g_pl[i].mask;
}
static int soul_halves(unsigned long p) { int h = *(const int *) (p + P_SOUL); return h < 0 ? 0 : h; }
static unsigned keep_hearts(unsigned m, int hearts) { return hearts >= 32 ? m : hearts <= 0 ? 0 : (m & ((1u << hearts) - 1u)); }

/* mirrors one AddSoulHearts: `immoral` says whether a GAIN was an immoral heart */
static void imm_after(unsigned long p, int before, int after, int immoral) {
    unsigned *m = imm_slot(p);
    int hb = (before + 1) / 2, ha = (after + 1) / 2;
    if (after <= 0) { *m = 0; return; }
    if (after < before) { *m = keep_hearts(*m, ha); return; }
    for (int i = hb; i < ha && i < 32; i++) { if (immoral) *m |= 1u << i; else *m &= ~(1u << i); }
}

static int is_fiend(unsigned long p) { return *(const int *) (p + PLAYER_TYPE) == FF_FIEND_TYPE; }

/* a gain of health that turns into black hearts, the same halves */
static void as_black(unsigned long p, int halves) {
    g_conv++;
    o_black(p, halves);
    g_conv--;
}

static void poll_note(unsigned long p);

static void w_max(unsigned long p, int halves, int ignore) {
    if (halves > 0 && !g_conv && is_fiend(p)) { as_black(p, halves); poll_note(p); return; }
    o_max(p, halves, ignore);
    poll_note(p);
}

static void w_red(unsigned long p, int halves, int flag) {
    if (halves > 0 && is_fiend(p)) return;                  /* dropped */
    o_red(p, halves, flag);
}

/* ------------------------------------------------------------------------------ the minions (customhealth.lua:643-820) */
int ffminion_spawn(unsigned long player, int sub);   /* ffminion.c */
int ffminion_room_active(void);
#define SUB_MINION_BLACK 1
#define SUB_MINION_IMMORAL 2
#define SUB_MINION_HALF 3

/* the hearts of the top run, from the top: hp of heart i, given the halves left */
static int hp_of(int i, int halves) { return (i == (halves + 1) / 2 - 1 && (halves & 1)) ? 1 : 2; }

/*
 * A hit has just taken `before - after` halves off the soul chain. The Lua asks whether the LAST DAMAGED heart was an
 * immoral one (or, for a Fiend, a black one) and, if so, walks the remaining chain from its top and turns the whole
 * contiguous run of that kind into minions: 2 halves -> a whole minion, a leftover half -> a half one. The immoral run
 * keeps one half when it is all the health left (customhealth.lua:664-668); the black run, when nothing is left, hands one
 * half back as a black heart (:757-762). `mask`/`black` are the masks as they were BEFORE the hit.
 */
static void hit_makes_minions(unsigned long p, int before, unsigned mask, unsigned black) {
    if (before <= 0) return;
    int top = (before + 1) / 2 - 1;                         /* the heart the hit landed on */
    int imm = top < 32 && ((mask >> top) & 1u);
    int blk = !imm && top < 32 && ((black >> top) & 1u) && is_fiend(p);
    if (!imm && !blk) return;
    int minions = 0;                                        /* halves */
    unsigned *mp = imm_slot(p);
    for (;;) {
        int s = soul_halves(p);
        if (s <= 0) break;
        int i = (s + 1) / 2 - 1, hp = hp_of(i, s);
        int is_imm = i < 32 && ((*mp >> i) & 1u);
        int is_blk = i < 32 && ((*(const unsigned *) (p + P_BLACK_MASK) >> i) & 1u) && !is_imm;
        if (imm ? !is_imm : !is_blk) break;
        if (imm && *(const int *) (p + P_RED) + s <= hp) {   /* the heart is all the health there is: it keeps one half */
            minions += hp - 1;
            if (hp > 1) { o_soul(p, -(hp - 1)); imm_after(p, s, soul_halves(p), 0); }
            break;
        }
        minions += hp;
        o_soul(p, -hp);
        imm_after(p, s, soul_halves(p), 0);
    }
    if (blk && minions > 0 && *(const int *) (p + P_RED) + soul_halves(p) <= 0) {     /* a Fiend never dies of this */
        g_conv++; o_black(p, 1); g_conv--;
        minions -= 1;
    }
    for (; minions > 0; minions -= 2) {
        if (imm)      ffminion_spawn(p, minions >= 2 ? SUB_MINION_IMMORAL : SUB_MINION_HALF);
        else if (minions >= 2) ffminion_spawn(p, SUB_MINION_BLACK);
        /* a leftover black half is an EFFECT minion in the Lua (FIEND_MINION_EFFECT): not ported yet */
    }
}

static int g_minion_busy;

static int g_hit_logs;
static unsigned char *g_base;

/*
 * THE HIT, SEEN FROM OUTSIDE. In Repentance the damage code does not go through AddSoulHearts (the log of a real hit of a Fiend
 * with 2 black + 1 immoral heart showed no soul loss at all), so the loss is also detected here: once per frame (called from
 * ffminion's Pickup::Update wrapper) every player's soul halves are compared with what this file last saw. Every path of ours
 * that changes them calls poll_note afterwards, so what is left is the game's own change - a hit.
 */
static struct { unsigned long p; int soul; unsigned black; int ok; } g_seen[MAX_PL];
static void poll_note(unsigned long p) {
    for (int i = 0; i < MAX_PL; i++)
        if (g_seen[i].p == p || !g_seen[i].p) {
            g_seen[i].p = p; g_seen[i].soul = soul_halves(p); g_seen[i].black = *(const unsigned *) (p + P_BLACK_MASK); g_seen[i].ok = 1;
            return;
        }
}
void ffheart_poll(void) {
    if (!g_base || g_conv || g_minion_busy) return;
    unsigned long game = *(const unsigned long *) (g_base + GAME_PTR);
    if (!game) return;
    const unsigned long *list = (const unsigned long *) (game + GAME_PLAYER_LIST);
    if (!list[0] || list[1] < list[0]) return;
    unsigned n = (unsigned) ((list[1] - list[0]) / 8);
    for (unsigned i = 0; i < n && i < MAX_PL; i++) {
        unsigned long p = ((unsigned long (*)(unsigned long, int)) (g_base + FN_PLAYER_AT))(game + GAME_PLAYERS, (int) i);
        if (!p) continue;
        int k = -1;
        for (int j = 0; j < MAX_PL; j++) if (g_seen[j].p == p) { k = j; break; }
        if (k < 0 || !g_seen[k].ok) { poll_note(p); continue; }
        int before = g_seen[k].soul, now = soul_halves(p);
        unsigned black_before = g_seen[k].black;
        if (now == before) { g_seen[k].black = *(const unsigned *) (p + P_BLACK_MASK); continue; }
        unsigned mask = *imm_slot(p);
        if (g_hit_logs++ < 12)
            LOG("ffheart: (seen from outside) soul %d -> %d, imm mask %#x, black mask %#x, room active %d, fiend %d",
                before, now, mask, black_before, ffminion_room_active(), is_fiend(p));
        if (now < before) {
            imm_after(p, before, now, 0);
            if (ffminion_room_active()) {
                g_minion_busy++;
                hit_makes_minions(p, before, mask, black_before);
                g_minion_busy--;
            }
        }
        poll_note(p);
    }
}
static void w_soul(unsigned long p, int halves) {
    ff_flush();
    /* a Fiend's direct soul heart is black (his rule); an immoral pickup being collected is the one thing that is not */
    if (halves > 0 && !g_conv && !g_immoral && is_fiend(p)) { as_black(p, halves); poll_note(p); return; }
    int before = soul_halves(p);
    unsigned mask = *imm_slot(p), black = *(const unsigned *) (p + P_BLACK_MASK);
    o_soul(p, halves);
    imm_after(p, before, soul_halves(p), g_immoral > 0 && halves > 0);
    if (halves < 0 && g_hit_logs++ < 6)
        LOG("ffheart: soul %d -> %d (halves %d), imm mask %#x, black mask %#x, room active %d, fiend %d, conv %d",
            before, soul_halves(p), halves, mask, black, ffminion_room_active(), is_fiend(p), g_conv);
    if (halves < 0 && !g_conv && !g_minion_busy && soul_halves(p) < before && ffminion_room_active()) {
        g_minion_busy++;
        hit_makes_minions(p, before, mask, black);
        g_minion_busy--;
    }
    poll_note(p);
}

static void w_black(unsigned long p, int halves) {          /* calls AddSoulHearts inside */
    int before = soul_halves(p);
    g_conv++;
    o_black(p, halves);
    g_conv--;
    poll_note(p);
    /* the run's start for a Fiend: his two black hearts are in, and ONE immoral heart (two halves) follows them */
    if (halves == 4 && before == 0 && is_fiend(p)) { g_immoral++; w_soul(p, 2); g_immoral--; }
}

static void w_bone(unsigned long p, int wholes) {           /* calls AddSoulHearts inside */
    g_conv++;
    o_bone(p, wholes);
    g_conv--;
    poll_note(p);
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

static int g_spawn_logs;
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
    if (variant == V_MINION && *(const int *) (pk + E_TYPE) == PICKUP) return 0;   /* a minion is not picked up */
    int sub = soul_form(variant);
    if (!sub || *(const int *) (other + E_TYPE) != 1 /* the player: type 1 */) return o_collide(pk, other, low);
    int was = *(const int *) (pk + E_SUBTYPE);
    int immoral = variant == V_IMMORAL || variant == V_HALF_IMMORAL || variant == V_BLENDED_IMMORAL;
    *(int *) (pk + E_VARIANT) = V_HEART;
    *(int *) (pk + E_SUBTYPE) = sub;
    if (immoral) g_immoral++;
    int r = o_collide(pk, other, low);
    if (immoral) g_immoral--;
    if (!*(const unsigned char *) (pk + E_DEAD)) {          /* not taken: it is what it was */
        *(int *) (pk + E_VARIANT) = variant;
        *(int *) (pk + E_SUBTYPE) = was;
    }
    return r;
}

/* ------------------------------------------------------------------------------ the health bar (2c) */
#define LAYOUT_SLOT_BYTES   0x18
#define LAYOUT_SLOT_NAME    0x08
typedef void (*fn_layout)(unsigned long, unsigned char *, int, unsigned long);
static fn_layout o_layout;
static const char NAME_IMMORAL_FULL[] = "ImmoralHeartFull";
static const char NAME_IMMORAL_HALF[] = "ImmoralHeartHalf";

static void w_layout(unsigned long layout, unsigned char *slots, int count, unsigned long player) {
    o_layout(layout, slots, count, player);
    if (!slots || count <= 0 || count > 64 || !g_base) return;
    unsigned *mp = imm_slot(player);
    *mp = keep_hearts(*mp, (soul_halves(player) + 1) / 2);  /* only the hearts that are there */
    if (!*mp) return;
    const char *bh = (const char *) (g_base + STR_BLUE_HALF), *bf = (const char *) (g_base + STR_BLUE_FULL);
    const char *kh = (const char *) (g_base + STR_BLACK_HALF), *kf = (const char *) (g_base + STR_BLACK_FULL);
    int k = 0;                                              /* the soul-type slots are the hearts, in order */
    for (int i = 0; i < count; i++) {
        const char **name = (const char **) (slots + (unsigned long) i * LAYOUT_SLOT_BYTES + LAYOUT_SLOT_NAME);
        int blue = *name == bf || *name == bh, black = *name == kf || *name == kh;
        if (!blue && !black) continue;
        if (blue && k < 32 && ((*mp >> k) & 1u)) *name = *name == bf ? NAME_IMMORAL_FULL : NAME_IMMORAL_HALF;
        k++;
    }
}

#ifndef FF_HOST_TEST
typedef struct { unsigned off; uint32_t want[4]; void *wrapper; void **orig; } det_t;

static det_t g_det[8] = {
    { 0x4ee8a0, { 0xa9bd57f6u, 0xa9014ff4u, 0xa9027bfdu, 0x910083fdu }, (void *) w_max,   (void **) &o_max   },
    { 0x4f0338, { 0xa9bd57f6u, 0xa9014ff4u, 0xa9027bfdu, 0x910083fdu }, (void *) w_red,   (void **) &o_red   },
    { 0x4ef2d0, { 0xa9ba6ffcu, 0xa90167fau, 0xa9025ff8u, 0xa90357f6u }, (void *) w_soul,  (void **) &o_soul  },
    { 0x4ef078, { 0xa9bc5ff8u, 0xa90157f6u, 0xa9024ff4u, 0xa9037bfdu }, (void *) w_black, (void **) &o_black },
    { 0x4ef5c4, { 0xa9bc5ff8u, 0xa90157f6u, 0xa9024ff4u, 0xa9037bfdu }, (void *) w_bone,  (void **) &o_bone  },
    { 0x88fcbc, { 0xd10283ffu, 0xa9046ffcu, 0xa90567fau, 0xa9065ff8u }, (void *) w_spawn,   (void **) &o_spawn   },
    { 0x71e310, { 0xd10783ffu, 0x6d162bebu, 0x6d1723e9u, 0xa9186ffcu }, (void *) w_collide, (void **) &o_collide },
    { 0x6e45d8, { 0xd102c3ffu, 0xa9056ffcu, 0xa90667fau, 0xa9075ff8u }, (void *) w_layout,  (void **) &o_layout  },
};
#define N_DET 8

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
    LOG("ffheart: Fiend's health rules, heart pickups and health bar are on");
}
#endif
