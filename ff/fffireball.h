/*
 * fffireball.h - Fiend's fireballs (fiend.lua 325-830, main.lua 18695-18765, 7539-7552, 14800-14815, controls.lua 98-150).
 * Included at the end of the helpers of ffminion.c (it uses its grid, snapshot, spawn, sprite and damage helpers).
 *
 * WHAT THE LUA DOES, AND WHAT IS HERE
 *   charge   Every hit a Fiend (or what it shot) lands adds min(damage, the enemy's HitPoints AFTER the hit) to a sum
 *            (PostTakeDmgHandleFiendFireballs, fiend.lua:823-862 - the min is taken after the damage, so a killing blow adds a
 *            zero or a negative number; kept). At the next update of that enemy, if it lives, the sum >= 40 + 10 * max(stage, 1)
 *            (main.lua:18755): sum = 0, the flash effect 1000.668 follows the player for its 9 frames, FLAME_BURST at 0.3 and
 *            pitch 1.5-1.6, and one more orbiting fireball, up to 3 (a Fiend has no Pyromancy or Imp Finger here).
 *   orbit    Familiar 3.1019, radius 30 around player.Position + player.Velocity, -0.08 rad a frame, spread evenly, sprite
 *            offset (0,-14), alpha 0 -> 1 over 5 frames, anim MoveHori; CANDLE_LIGHT when one is born (fiend.lua:748-790).
 *   launch   A double tap (the port's own gesture, the game's detector with its window opened by sticks.c) with a ball left
 *            and 10 frames since the last: the ball nearest to player.Position + dir * 10 goes, and a shot leaves at 30 px a frame
 *            (fiend.lua:472-505 LaunchFireball 506-538), damage max(3.5, Damage) * 3, FireballLaunch at 0.4.
 *   shot     Not falling, YIN_YANG homing (Strength 0.02), a trail every second frame, dies on a wall, on a speed below 5, or on
 *            an enemy; the hit does the 3x damage, then (fiendTearCollision) 2 to each enemy within 37.5 + size with a crater of
 *            0.5 and an explosion that throws embers; the death does 6 to each within 56.25 + size, a 0.75 crater and its embers.
 *   embers   Effect 1000.7015: 1.4 (or the tear's share) fire damage to what it touches every frame, size - 1 and scale - 0.1 a
 *            frame, alpha size/10 under 10, gone at size 0 (fiend.lua:702-731).
 *   hearts   What a shot or its embers kill drops a half immoral heart half the time (main.lua:18700-18715), gone after 60 frames.
 *
 * WHAT IS NOT THE LUA, SAID PLAINLY
 *   - The shot is an effect (1000.1752) that this file moves, not a tear: the tear's internals (falling, size, flags) are not
 *     measured, and the effect carries the same fireball art. TearFlagsLib is not in ff-test, so YIN_YANG's Strength is read as
 *     "turn the velocity towards the nearest enemy by Strength of the speed each frame" - an assumption.
 *   - The trail's visual is the Lua's Fire Jet (1000.147) with CollisionDamage 0; that field's offset is not known and the jet
 *     does 10, so the trail is drawn with short-lived embers and its damage is done directly.
 *   - Knockback is applied to every enemy (the NO_KNOCKBACK flags' bits are not known). The orbiters keep the familiar's own
 *     collision class (ENEMIES in the Lua; its field is not known). Aim is the four directions of the double tap.
 *   - The enemy's HitPoints offset is LEARNED at run time (the float that drops by the damage dealt, three hits running); until
 *     then the raw damage is credited and no heart is rolled. Log: "fffireball: HitPoints".
 */
#ifndef FFFIREBALL_H
#define FFFIREBALL_H

#define FB_FIEND_TYPE 41
#define FB_PLAYER_TYPE 0x1738
#define FB_CTRL 0x19ec
#define FB_TAP_LAST 0x26e8
#define FB_TAP_COUNT 0x26ec
#define FB_SPAWNER 0x400
#define FB_REF_SOURCE 0x28
#define FB_GAME_FRAME 0x1af34c
#define FN_TRIGGERED 0x671c6c
#define FB_ENTITY_SCALE 0x12c

#define V_WISP 1019
#define V_SHOT 1752
#define V_FLASH 668
#define V_EMBER 7015
#define V_BOOM 7018
#define V_CRATER 18
#define DMG_FIRE 2ull
#define DMG_EXPLOSION 4ull
#define FB_CAP 3
#define FB_COOLDOWN 10
#define FB_SPEED 30.0f
#define FB_ORBIT_R 30.0f
#define FB_ORBIT_STEP (-0.08f)
#define FB_TEAR_MULT 3.0f
#define FB_SHOT_RADIUS 16.0f
#define FB_SHOT_SCALE 1.0f                    /* the sheet's own size: its balls are ~20 px in a 64 px cell. 1.5 was seen on a real run and was far too big */
#define FB_PARENT 0x390
#define FB_HOMING 0.02f

enum { SND_LAUNCH = 0, SND_LAND = 1, SND_BURST = 2, SND_FLAMEEND = 3, SND_BOSSEXP = 4, SND_CANDLE = 5 };
void ffsound_fireball(int what, float vol, float pitch) __attribute__((weak));
static void fb_sound(int what, float vol, float pitch) { if (ffsound_fireball) ffsound_fireball(what, vol, pitch); }

static int g_fbl[16];
#define FBLOGN(k, n, ...) do { if (g_fbl[k]++ < (n)) LOG(__VA_ARGS__); } while (0)
#define FBLOG(k, ...) do { if (g_fbl[k]++ < 4) LOG(__VA_ARGS__); } while (0)
static int (*g_trigger)(int action, int controller);
static int (*g_has_item)(unsigned long player, int id, int ignore);       /* Player::HasCollectible (0x4eeb74, ffchars.c) */
static void (*g_sheet)(void *sprite, const char *path);                   /* Sprite:ReplaceSpritesheet(0, path) + LoadGraphics (0x8a0228, 0x8a0284: marked.c) */
#define ITEM_BIRTHRIGHT 619
#define FB_SHEET_WISP_FIENDISH "gfx/fiend_fireball/wisp_fiendish.png"
#define FB_SHEET_SHOT_FIENDISH "gfx/fiend_fireball/shot_fiendish.png"
#define FB_HOMING_FIENDISH 0.08f
static int fb_birthright(unsigned long p) { return g_has_item && g_has_item(p, ITEM_BIRTHRIGHT, 0); }
static int (*g_frame_fn)(void);
static int fb_real_frame(void) { unsigned long game = *(const unsigned long *) (g_base + GAME_PTR); return game ? *(const int *) (game + FB_GAME_FRAME) : 0; }
static int fb_stage(void) { unsigned long game = *(const unsigned long *) (g_base + GAME_PTR); return game ? *(const int *) game : 0; }

/* ---------------------------------------------------------------------------------------------- state */
#define FB_MAXP 4
#define FB_MAXB 12
typedef struct {
    unsigned long p;
    float dealt, ang;
    int balls, ticks, last_launch;
    unsigned long fam[FB_MAXB]; int fage[FB_MAXB], fis[FB_MAXB];       /* fis: IsFiendishFire, for good once set (fiend.lua:778-783) */
} fbp_t;
static fbp_t g_fp[FB_MAXP];
static int g_fb_last_frame = -1, g_fb_world_frame = -1, g_wt;          /* g_wt: world ticks, one a game frame */
static unsigned long g_fb_room;
static int g_fb_any;                                                   /* a Fiend is in the run: the damage hook only works then */

typedef struct { unsigned long e; int type, variant, until; unsigned long follow; } tm_t;
static tm_t g_tm[96];
typedef struct { int used, age, anim, fiendish; unsigned long e, owner; float x, y, vx, vy, dmg; } shot_t;
static shot_t g_sh[8];
typedef struct { int used, age, fiendish; unsigned long e, owner; float x, y, vx, vy, size, dmg; int heart; } ember_t;
static ember_t g_em[160];

static fbp_t *fb_state(unsigned long p, int create) {
    int free = -1;
    for (int i = 0; i < FB_MAXP; i++) { if (g_fp[i].p == p) return &g_fp[i]; if (!g_fp[i].p && free < 0) free = i; }
    if (!create || free < 0) return 0;
    memset(&g_fp[free], 0, sizeof g_fp[free]); g_fp[free].p = p; g_fp[free].last_launch = -1000;
    return &g_fp[free];
}
static int fb_is_fiend(unsigned long p) { return p && *(const int *) (p + FB_PLAYER_TYPE) == FB_FIEND_TYPE; }
static void fb_kill(unsigned long e) { *(unsigned char *) (e + E_DEAD) = 1; *(unsigned char *) (e + E_EXISTS) = 0; }
static int fb_is(unsigned long e, int type, int variant) { return e && g_readable((const void *) e, 0x1d0) && *(const int *) (e + E_TYPE) == type && *(const int *) (e + E_VARIANT) == variant && alive(e); }
static unsigned long fb_make(int type, int variant, int sub, float x, float y, float vx, float vy, unsigned long spawner) {
    float pos[2] = { x, y }, vel[2] = { vx, vy };
    return g_spawn(*(const unsigned long *) (g_base + GAME_PTR), type, variant, pos, vel, spawner, sub, 0, 0);
}
static void fb_scale(unsigned long e, float s) { float *sc = (float *) (e + FB_ENTITY_SCALE); sc[0] = s; sc[1] = s; }
static float fb_rf(float a, float b) { return a + (b - a) * (float) rnd(1000) / 1000.0f; }
static void tm_add(unsigned long e, int type, int variant, int frames, unsigned long follow) {
    for (int i = 0; i < 96; i++) if (!g_tm[i].e) { g_tm[i].e = e; g_tm[i].type = type; g_tm[i].variant = variant; g_tm[i].until = g_wt + frames; g_tm[i].follow = follow; return; }
}
static void tm_run(void) {
    for (int i = 0; i < 96; i++) {
        if (!g_tm[i].e) continue;
        unsigned long e = g_tm[i].e;
        if (!fb_is(e, g_tm[i].type, g_tm[i].variant)) { g_tm[i].e = 0; continue; }
        if (g_tm[i].follow) { fpos(e)[0] = fpos(g_tm[i].follow)[0] + fvel(g_tm[i].follow)[0]; fpos(e)[1] = fpos(g_tm[i].follow)[1] + fvel(g_tm[i].follow)[1]; }
        if (g_wt >= g_tm[i].until) { fb_kill(e); g_tm[i].e = 0; }
    }
}

/* ---------------------------------------------------------------------------------------------- damage and the enemy's HitPoints */
#define HP_FROM 0x300      /* a real run never found HitPoints in 0x340..0x400: look in 0x300..0x700 */
#define HP_N 256
/* Entity::HitPoints is entity+0x354 and MaxHitPoints +0x358 (NPC::TakeDamage 0x69876c: 0x69aa54 ldr s0,[x20,#0x358] ... fcsel ... str s0,[x20,#0x354] clamps the HP to the max).
 * The run-time search for it (kept as a fallback if this is ever zeroed) found nothing in 64 real hits. */
#define FB_HP_OFF 0x354
static int g_hp_off = FB_HP_OFF, g_hp_votes[HP_N];
static float fb_hp(unsigned long e) { return g_hp_off ? *(const float *) (e + g_hp_off) : 1e9f; }
static int hit_ex(unsigned long e, float damage, unsigned long long flags, int countdown) {
    static unsigned char ref[0x100];                                   /* an EntityRef with no source: the fireball's own hits are not credited twice */
    memset(ref, 0, sizeof ref);
    const unsigned long *vt = *(const unsigned long *const *) e;
    return vt ? ((fn_take) vt[VT_TAKE_DAMAGE / 8])(e, damage, flags, ref, countdown) : 0;
}
static void fb_learn_hp(const float *before, unsigned long e, float dmg) {
    if (g_hp_off || dmg <= 0.0f) return;
    float after[HP_N];
    if (!safe_copy(after, (const void *) (e + HP_FROM), sizeof after)) return;
    int best = -1, bestv = 0, second = 0;
    for (int i = 0; i < HP_N; i++) {
        float d = before[i] - after[i];
        if (before[i] > 0.0f && before[i] < 1e7f && d > 0.001f && d <= dmg * 2.0f + 0.01f) g_hp_votes[i]++;
        if (g_hp_votes[i] > bestv) { second = bestv; bestv = g_hp_votes[i]; best = i; } else if (g_hp_votes[i] > second) second = g_hp_votes[i];
    }
    static int tries; tries++;
    if (tries % 8 == 0 && tries <= 64) LOG("fffireball: HitPoints search, %d hits seen, best entity+%#x with %d votes, runner-up %d", tries, best >= 0 ? HP_FROM + 4 * best : 0, bestv, second);
    if (best >= 0 && bestv >= 3 && bestv > second + 1) { g_hp_off = HP_FROM + 4 * best; LOG("fffireball: HitPoints looks like entity+%#x (%d votes, runner-up %d)", g_hp_off, bestv, second); }
}

/* ---------------------------------------------------------------------------------------------- charge */
static void fb_flash(unsigned long player) {
    unsigned long f = fb_make(T_EFFECT, V_FLASH, 0, fpos(player)[0], fpos(player)[1], 0, 0, player);
    if (f) tm_add(f, T_EFFECT, V_FLASH, 9, player);
    fb_sound(SND_BURST, 0.3f, fb_rf(1.5f, 1.6f));
}
static void fb_check_charge(fbp_t *st) {
    int stage = fb_stage(); if (stage < 1) stage = 1;
    if (st->dealt >= (float) (40 + 10 * stage)) {
        st->dealt = 0;
        fb_flash(st->p);
        if (st->balls < FB_CAP) st->balls++;
        FBLOG(2, "fffireball: charged - %d orbiting fireball(s) now", st->balls);
    }
}
static void fb_credit(unsigned long player, float amount, int survived) {
    fbp_t *st = fb_state(player, 0); if (!st) return;
    st->dealt += amount;
    if (survived) fb_check_charge(st);
}

/* a half immoral heart half the time for what a fireball kills; it stays 60 frames (heart.Timeout = 60) */
/* A kill by fire (fiend.lua:827 flags what the fireball or its embers hit; main.lua:18700-18720 checks the flagged NPC on its next update):
 * HasMortalDamage and not NO_REWARD, and it must have been there when the room started (SpawnedAtRoomStart): then 50% a half immoral
 * heart that times out after 60 frames. A real run showed the damage may not be applied inside TakeDamage (HitPoints read the same right
 * after the call), so the flagged NPCs are watched from frame to frame: HitPoints <= 0, dead, or gone from the room with damage >= its HitPoints. */
#define KW_N 32
typedef struct { unsigned long e; int type, variant, age, immediate, start; float x, y, hp, dmg; } kw_t;
static kw_t g_kw[KW_N];
/* The Room object can be reused from room to room, so the pointer alone misses changes: fold in the game's index of the room being played (Game+0x21558). */
static unsigned long fb_room_sig(void) {
    unsigned long rm = g_room ? g_room() : 0;
    if (g_base) { unsigned long game = *(const unsigned long *) (g_base + GAME_PTR); if (game) rm ^= (unsigned long) (unsigned) *(const int *) (game + 0x21558) << 48 ^ (unsigned long) (*(const int *) (game + 0x0) + 1) << 40; }
    return rm;
}
static unsigned long g_start[SNAP]; static unsigned g_nstart; static int g_start_wait;
static int fb_was_at_start(unsigned long e) { for (unsigned i = 0; i < g_nstart; i++) if (g_start[i] == e) return 1; return 0; }
static void fb_heart_at(float x, float y) {
    if (rnd(2) >= 1) { FBLOGN(14, 12, "fffireball: a kill by fire, the 50%% roll was lost"); return; }
    float a = fb_rf(0, 6.2831853f), sp = fb_rf(0.1f, 2.0f);
    unsigned long h = fb_make(T_PICKUP, V_HALF_IMMORAL, 0, x, y, cosf(a) * sp, sinf(a) * sp, first_player());
    if (h) { tm_add(h, T_PICKUP, V_HALF_IMMORAL, 60, 0); FBLOGN(3, 4, "fffireball: a half immoral heart for a kill by fire"); }
    else FBLOGN(15, 4, "fffireball: a half immoral heart was due but the spawn gave nothing");
}
static void fb_mark(unsigned long e, float dmg, float hp0, float hp1, unsigned long player) {
    if (!fb_is_fiend(player)) return;
    kw_t *k = 0;
    for (int i = 0; i < KW_N; i++) if (g_kw[i].e == e) { k = &g_kw[i]; break; }
    if (!k) for (int i = 0; i < KW_N; i++) if (!g_kw[i].e) { k = &g_kw[i]; break; }
    if (!k) return;
    if (k->e != e) {
        memset(k, 0, sizeof *k); k->e = e; k->type = *(const int *) (e + E_TYPE); k->variant = *(const int *) (e + E_VARIANT);
        k->hp = hp0; k->start = fb_was_at_start(e);
    }
    if (hp1 < hp0 - 0.01f) { k->immediate = 1; k->hp = hp1; }
    k->dmg += dmg; k->age = 0; k->x = fpos(e)[0]; k->y = fpos(e)[1];
}
static void fb_kill_watch(void) {
    if (g_start_wait > 0 && --g_start_wait == 0) g_nstart = snapshot(g_start);       /* the room's own monsters */
    int any = 0; for (int i = 0; i < KW_N; i++) if (g_kw[i].e) { any = 1; break; }
    if (!any) return;
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (int i = 0; i < KW_N; i++) {
        kw_t *k = &g_kw[i]; if (!k->e) continue;
        int found = 0; for (unsigned j = 0; j < n; j++) if (snap[j] == k->e && *(const int *) (k->e + E_TYPE) == k->type) { found = 1; break; }
        int dead;
        if (found) {
            float hp = fb_hp(k->e);
            k->x = fpos(k->e)[0]; k->y = fpos(k->e)[1];
            if (k->immediate || hp < k->hp - 0.01f) { k->immediate = 1; k->hp = hp; }
            dead = !alive(k->e) || (g_hp_off && hp <= 0.0f);
            if (!dead && ++k->age > 90) { k->e = 0; continue; }                      /* it survived */
        } else dead = k->immediate ? k->hp <= 0.0f : k->dmg >= k->hp;
        if (!dead) continue;
        FBLOGN(13, 16, "fffireball: %d.%d died by fire (HitPoints %.2f, damage %.2f, %s, room's own %d)", k->type, k->variant, k->hp, k->dmg, found ? "seen" : "gone", k->start);
        if (k->start) fb_heart_at(k->x, k->y);
        k->e = 0;
    }
}
/* one hit by what the fireball made: damages it; nothing the fireball makes credits the charge (a real run showed it snowballing). mark: the
 * shot's own hit and its embers flag the NPC for the heart (fiend.lua:453 / 597) */
static void fb_deal2(unsigned long e, float dmg, unsigned long long flags, int cd, unsigned long player, int credit, int mark) {
    float hp0 = fb_hp(e);
    hit_ex(e, dmg, flags, cd);
    float hp1 = fb_hp(e);
    if (credit) fb_credit(player, hp1 < dmg ? hp1 : dmg, hp1 > 0.0f);
    if (mark) fb_mark(e, dmg, hp0, hp1, player);
}
static void fb_deal(unsigned long e, float dmg, unsigned long long flags, int cd, unsigned long player, int credit) { fb_deal2(e, dmg, flags, cd, player, credit, 0); }

/* the game's own hits: the NPC TakeDamage hook (ffminion.c) calls these */
static int fb_pre_hit(unsigned long e, float *before) {
    if (!g_fb_any || !*(void *const *) e) return 0;
    return safe_copy(before, (const void *) (e + HP_FROM), HP_N * 4);
}
/* The Fiend a hit belongs to (fiend.lua:837-851): the source is the player itself, or - when the source is not an NPC, pickup or familiar -
 * its spawner is. A tear's owner is found through both links the game keeps (Parent 0x390, SpawnerEntity 0x400), a few steps up. */
static unsigned long fb_owner_of(unsigned long src) {
    if (!src || !g_readable((const void *) src, 0x410)) return 0;
    int t = *(const int *) (src + E_TYPE);
    if (t == 1) return src;
    if (t == 3 || t == 5 || (t >= 10 && t < 1000)) return 0;
    unsigned long cur = src;
    for (int depth = 0; depth < 4 && cur; depth++) {
        static const int links[2] = { FB_SPAWNER, FB_PARENT };
        unsigned long next = 0;
        for (int k = 0; k < 2; k++) {
            unsigned long c = *(const unsigned long *) (cur + links[k]);
            if (!c || c == cur || !g_readable((const void *) c, 0x410)) continue;
            int ct = *(const int *) (c + E_TYPE);
            if (ct == 1 && *(const int *) (c + E_VARIANT) == 0) return c;
            if (ct != 3 && ct != 5 && !(ct >= 10 && ct < 1000) && !next) next = c;
        }
        cur = next;
    }
    return 0;
}
static void fb_post_hit(unsigned long e, float dmg, void *ref, const float *before) {
    if (!g_fb_any) return;
    fb_learn_hp(before, e, dmg);                                                      /* any hit teaches the HitPoints offset, not only a Fiend's */
    { int t = *(const int *) (e + E_TYPE); static int n; if (t != 33 && t != 245 && n++ < 24) LOG("fffireball: hit on %d.%d dmg %.2f: HitPoints %.2f -> %.2f right after the call", t, *(const int *) (e + E_VARIANT), dmg, before[(FB_HP_OFF - HP_FROM) / 4], *(const float *) (e + FB_HP_OFF)); }
    if (!ref || !g_readable(ref, 0x30)) return;
    unsigned long player = fb_owner_of(*(const unsigned long *) ((unsigned char *) ref + FB_REF_SOURCE));
    if (!fb_is_fiend(player)) return;
    float hp1 = fb_hp(e);
    fb_credit(player, hp1 < dmg ? hp1 : dmg, hp1 > 0.0f);
}
/* a minion's hit (fiend.lua:1204-1206 adds the hit, and its TakeDamage with the player as source is credited once more by the callback) */
static void fb_minion_credit(unsigned long player, float dmg, unsigned long e) {
    if (!g_fb_any || !fb_is_fiend(player)) return;
    float hp1 = fb_hp(e);
    fb_credit(player, dmg + (hp1 < dmg ? hp1 : dmg), hp1 > 0.0f);
}

/* ---------------------------------------------------------------------------------------------- the explosion and the embers */
/* A blast throws 18 embers and a shot drops one every other frame; spawning an effect for each (sprite, animation, entity list) is what made
 * the game stutter while fireballs were about. The visuals are pooled: a spent ember is parked (invisible, far away) and the next one reuses it. */
#define EMBER_POOL 48
static unsigned long g_pool[EMBER_POOL];
static void ember_release(unsigned long e) {
    for (int i = 0; i < EMBER_POOL; i++) if (!g_pool[i]) {
        ((float *) (e + E_SPRITE + 0xe8))[3] = 0.0f; fpos(e)[0] = -4000.0f; fpos(e)[1] = -4000.0f; fvel(e)[0] = 0; fvel(e)[1] = 0;
        g_pool[i] = e; return;
    }
    fb_kill(e);
}
static unsigned long ember_acquire(float x, float y, unsigned long owner) {
    for (int i = 0; i < EMBER_POOL; i++) if (g_pool[i]) {
        unsigned long e = g_pool[i]; g_pool[i] = 0;
        if (!fb_is(e, T_EFFECT, V_EMBER)) continue;
        fpos(e)[0] = x; fpos(e)[1] = y; g_play((void *) (e + E_SPRITE), "Idle", 1);
        return e;
    }
    return fb_make(T_EFFECT, V_EMBER, 0, x, y, 0, 0, owner);
}
static void ember_make(float x, float y, float vx, float vy, float size, float dmg, unsigned long owner, int heart, int visual, int fiendish) {
    for (int i = 0; i < 160; i++) if (!g_em[i].used) {
        ember_t *m = &g_em[i]; memset(m, 0, sizeof *m);
        m->used = 1; m->fiendish = fiendish; m->x = x; m->y = y; m->vx = vx; m->vy = vy; m->size = size; m->dmg = dmg; m->owner = owner; m->heart = heart;
        if (visual) {
            m->e = ember_acquire(x, y, owner);
            if (m->e) {
                *(float *) (m->e + E_SIZE) = size;
                float *tn = (float *) (m->e + E_SPRITE + 0xe8), *o = (float *) (m->e + E_SPRITE + 0x108), *sc = (float *) (m->e + E_SPRITE + 0xdc);
                tn[0] = tn[1] = tn[2] = tn[3] = 1.0f; sc[0] = sc[1] = 1.0f;
                if (fiendish) { o[0] = 66.0f / 255.0f; o[1] = 13.0f / 255.0f; o[2] = 202.0f / 255.0f; } else { o[0] = o[1] = o[2] = 0.0f; }
            }
        }
        return;
    }
}
/* explodeFiendProj: a fiendish ball's blast makes a hollow minion a third of the time (fiend.lua:567-585; the hollow one is our 5.750.16) */
static void fb_hollow(unsigned long owner, float x, float y) {
    float pos[2] = { x, y }, vel[2] = { 0, 0 };
    unsigned long e = g_spawn(*(const unsigned long *) (g_base + GAME_PTR), T_PICKUP, V_MINION, pos, vel, owner, SUB_HALF_BLACK, 0, 0);
    if (!e) { FBLOG(7, "fffireball: Spawn(5.750.%d) for a fiendish blast returned nothing", SUB_HALF_BLACK); return; }
    for (int i = 0; i < MAXM; i++) if (g_m[i].e == e) g_m[i].e = 0;
    slot_of(e, 1, owner);
    FBLOG(7, "fffireball: a fiendish blast made a hollow minion (5.750.%d) at %.0f,%.0f", SUB_HALF_BLACK, x, y);
}
static void fb_explode(float x, float y, unsigned long owner, float ember_dmg, float spread, int fiendish) {          /* explodeFiendProj */
    fb_sound(SND_LAND, 1.4f, fb_rf(0.9f, 1.1f));
    unsigned long b = fb_make(T_EFFECT, V_BOOM, 0, x, y, 0, 0, 0);
    if (b) tm_add(b, T_EFFECT, V_BOOM, 19, 0);                                       /* removed when "Explosion" is finished: 19 frames */
    if (fiendish && rnd(3) == 0) fb_hollow(owner, x, y);
    float rot = (float) (1 + (int) rnd(60));
    for (int i = 1; i <= 6; i++)
        for (int j = 0; j < 3; j++) {
            float speed = (30.0f - (j > 0 ? 10.0f : 0.0f)) / spread;
            float a = (((float) i - 0.33f * (float) j) * 60.0f + rot) * 0.0174532925f;
            ember_make(x, y, -sinf(a) * speed, cosf(a) * speed, 16.0f, ember_dmg, owner, 1, 1, fiendish);        /* Vector(0,1):Resized(speed):Rotated(a) */
        }
}
static void fb_crater(float x, float y, float s) {
    unsigned long c = fb_make(T_EFFECT, V_CRATER, 0, x, y, 0, 0, 0);
    if (c) fb_scale(c, s);
}
/* What stops a fireball: a wall, a rock, a pillar, poop... (grid collision 2 object, 3 solid, 4 wall). NOT a pit (1): it flies over it - blocking on
 * every non-free tile (what xy_blocked(.., 0) does for a walker) made it burst in mid-air over holes. The room's grid unknown: nothing blocks. */
/* The class of the tile under a point, as the engine itself says it (GridCollisionClass: 0 none, 1 pit, 2 object, 3 solid, 4 wall, 5 wall except player).
 * The grid table found at room+0xe98 is the pathfinder's and cannot tell a rock from a hole. No engine (the host tests): the table, 0/1000 -> 0/3. */
static int fb_tile(float x, float y) {
    if (g_grid_index && g_grid_class && g_room) {
        unsigned long rm = g_room();
        if (rm) { float p[2] = { x, y }; int idx = g_grid_index(rm, p); return idx < 0 ? 4 : g_grid_class(rm, idx); }
    }
    return tile_at((int) __builtin_floorf((x - 20.0f) / 40.0f), (int) __builtin_floorf((y - 100.0f) / 40.0f));
}
static int fb_blocked(float x, float y) {
    if (!g_gw && !g_grid_class) return 0;                                        /* no grid known: nothing blocks */
    int v = fb_tile(x, y); return v >= 2 && v <= 5;
}
static int fb_clear(float x0, float y0, float x1, float y1) { return !g_gw || line_clear(x0, y0, x1, y1, 1); }   /* CheckLine(.., 2) */
/* What the fireballs may hurt: the room's enemies, found ONCE a game frame (every ember asking the engine about every entity cost frames) and
 * never a friend: anything the player or a familiar made or follows (a real run had fireballs bursting on the Fiend's own helpers). */
static unsigned long g_tg[SNAP], g_fire[32]; static unsigned g_ntg, g_nfire; static int g_tg_wt = -2;
static void fb_targets_dirty(void) { g_tg_wt = -2; }
static int fb_ally(unsigned long e) {
    static const int links[2] = { FB_SPAWNER, FB_PARENT };
    for (int k = 0; k < 2; k++) {
        unsigned long c = *(const unsigned long *) (e + links[k]);
        if (c && g_readable((const void *) c, 0x40)) { int t = *(const int *) (c + E_TYPE); if (t == 1 || t == 3) return 1; }
    }
    return 0;
}
static unsigned fb_targets(unsigned long **out) {
    if (g_tg_wt != g_wt) {
        unsigned long snap[SNAP]; unsigned n = snapshot(snap); int lvl = target_level(snap, n, 0);
        g_ntg = 0; g_nfire = 0; g_tg_wt = g_wt;
        for (unsigned k = 0; k < n; k++) if (g_nfire < 32 && alive(snap[k]) && *(const int *) (snap[k] + E_TYPE) == 33) g_fire[g_nfire++] = snap[k];      /* fireplaces: a fireball bursts on them */
        for (unsigned k = 0; lvl >= 1 && k < n; k++)
            if (target_ok(snap[k], lvl) && !fb_ally(snap[k])) g_tg[g_ntg++] = snap[k];
    }
    *out = g_tg; return g_ntg;
}
static int fb_enemy_ok(unsigned long e, int lvl) { (void) lvl; return alive(e); }

static void ember_tick(void) {
    unsigned long *snap = 0; unsigned n = 0; int lvl = 2, loaded = 0;
    for (int i = 0; i < 160; i++) {
        ember_t *m = &g_em[i]; if (!m->used) continue;
        if (m->e && !fb_is(m->e, T_EFFECT, V_EMBER)) { m->e = 0; m->used = 0; continue; }
        if (!loaded) { n = fb_targets(&snap); loaded = 1; }
        for (unsigned k = 0; k < n; k++) {                                          /* the Lua: IsEnemy, IsVulnerableEnemy, distance - Size - Size <= 0 */
            if (!fb_enemy_ok(snap[k], lvl)) continue;
            float dx = fpos(snap[k])[0] - m->x, dy = fpos(snap[k])[1] - m->y;
            float r = m->size + fsize(snap[k]);
            if (dx * dx + dy * dy <= r * r) fb_deal2(snap[k], m->dmg, DMG_FIRE, 4, m->owner, 0, 1);
        }
        m->x += m->vx; m->y += m->vy;
        m->size -= 1.0f; m->age++;
        if (m->e) {
            fpos(m->e)[0] = m->x; fpos(m->e)[1] = m->y; fvel(m->e)[0] = 0; fvel(m->e)[1] = 0;
            *(float *) (m->e + E_SIZE) = m->size;
            float k = (1.414214f - 0.1f * (float) m->age) / 1.414214f; if (k < 0.0f) k = 0.0f;       /* Scale:Resized(length - 0.1) a frame */
            float *sc = (float *) (m->e + E_SPRITE + 0xdc); sc[0] = k; sc[1] = k;
            if (m->size < 10.0f) ((float *) (m->e + E_SPRITE + 0xe8))[3] = m->size > 0 ? m->size / 10.0f : 0.0f;
        }
        if (m->size <= 0.0f) { if (m->e) ember_release(m->e); m->e = 0; m->used = 0; }
    }
}

/* the sprite's Color is tint 0xe8 (rgba), colorize 0xf8 (rgba), offset 0x108 (rgb) from the sprite (ffchars.c's 0x2c-byte Color at sprite+0xe8) */
static void fb_color_dump(const char *what, unsigned long e) {
    const float *c = (const float *) (e + E_SPRITE + 0xe8);
    FBLOGN(9, 8, "fffireball: %s colour tint %.2f %.2f %.2f %.2f colorize %.2f %.2f %.2f %.2f offset %.2f %.2f %.2f", what, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10]);
}
static void fb_color_plain(unsigned long e, float alpha) {
    float *c = (float *) (e + E_SPRITE + 0xe8);
    c[0] = c[1] = c[2] = 1.0f; c[3] = alpha; c[4] = c[5] = c[6] = c[7] = 0.0f; c[8] = c[9] = c[10] = 0.0f;
}

/* ---------------------------------------------------------------------------------------------- the shot */
static void shot_anim(shot_t *s) {
    int a = (s->vx * s->vx >= s->vy * s->vy) ? (s->vx > 0 ? 0 : 1) : (s->vy > 0 ? 2 : 3);     /* 0 right 1 left 2 down 3 up */
    if (a == s->anim || !s->e) return;
    static const char *const names[4] = { "MoveRight", "MoveLeft", "MoveDown", "MoveUp" };
    g_play((void *) (s->e + E_SPRITE), names[a], 1);
    s->anim = a;
}
static void shot_die(shot_t *s, int hit_enemy_e, unsigned long player) {
    /* fireballTears (tear:IsDead): 6 to everything within 75*0.75 + size with a clear line, a crater and the explosion */
    unsigned long *snap; unsigned n = fb_targets(&snap); int lvl = 2;
    float tx = s->x, ty = s->y;
    for (unsigned k = 0; k < n; k++) {
        if (!fb_enemy_ok(snap[k], lvl)) continue;
        float dx = fpos(snap[k])[0] - tx, dy = fpos(snap[k])[1] - ty, r = 56.25f + fsize(snap[k]);
        if (dx * dx + dy * dy < r * r && fb_clear(tx, ty, fpos(snap[k])[0], fpos(snap[k])[1])) {
            fb_deal(snap[k], 6.0f, DMG_EXPLOSION, 0, player, 0);
            float d = sqrtf(dx * dx + dy * dy); if (d > 0.01f) { fvel(snap[k])[0] += dx / d * 8.0f; fvel(snap[k])[1] += dy / d * 8.0f; }
        }
    }
    (void) hit_enemy_e;
    fb_crater(tx, ty, 0.75f);
    fb_sound(SND_BOSSEXP, 1.0f, 1.0f);
    fb_explode(tx, ty, player, 1.0f / FB_TEAR_MULT, 5.0f, s->fiendish);
    if (s->e) fb_kill(s->e);
    s->e = 0; s->used = 0;
}
static void shot_hit(shot_t *s, unsigned long hit, unsigned long player) {
    fb_deal2(hit, s->dmg, 0, 0, player, 0, 1);                                           /* the tear's own collision damage */
    /* fiendTearCollision: 2 to each enemy within 75*0.5 + size, with its crater, sound and embers - each time */
    unsigned long *snap; unsigned n = fb_targets(&snap); int lvl = 2;
    for (unsigned k = 0; k < n; k++) {
        if (!fb_enemy_ok(snap[k], lvl)) continue;
        float dx = fpos(snap[k])[0] - s->x, dy = fpos(snap[k])[1] - s->y, r = 37.5f + fsize(snap[k]);
        if (dx * dx + dy * dy < r * r && fb_clear(s->x, s->y, fpos(snap[k])[0], fpos(snap[k])[1])) {
            fb_deal(snap[k], 2.0f, DMG_EXPLOSION, 0, player, 0);
            fb_crater(s->x, s->y, 0.5f);
            fb_sound(SND_BOSSEXP, 1.0f, 1.0f);
            fb_explode(s->x, s->y, player, s->dmg / 12.0f, 10.0f, s->fiendish);
            float d = sqrtf(dx * dx + dy * dy); if (d > 0.01f) { fvel(snap[k])[0] += dx / d * 8.0f; fvel(snap[k])[1] += dy / d * 8.0f; }
        }
    }
    shot_die(s, 1, player);                                                          /* the tear is dead: its own death follows */
}
static void shot_tick(shot_t *s) {
    unsigned long player = s->owner;
    s->age++;
    if (s->e && !fb_is(s->e, T_EFFECT, V_SHOT)) s->e = 0;                            /* the engine took it away: never write through a pointer that may be another entity now */
    unsigned long *snap; unsigned n = fb_targets(&snap); int lvl = 2;
    /* YIN_YANG homing: towards the nearest enemy */
    unsigned long best = 0; float bd = 1e30f;
    for (unsigned k = 0; k < n; k++) {
        if (!fb_enemy_ok(snap[k], lvl)) continue;
        float dx = fpos(snap[k])[0] - s->x, dy = fpos(snap[k])[1] - s->y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = snap[k]; }
    }
    if (best) {
        float dx = fpos(best)[0] - s->x, dy = fpos(best)[1] - s->y, d = sqrtf(dx * dx + dy * dy), sp = sqrtf(s->vx * s->vx + s->vy * s->vy);
        if (d > 0.01f && sp > 0.01f) {
            float hs = s->fiendish ? FB_HOMING_FIENDISH : FB_HOMING;
            float nx = s->vx + dx / d * sp * hs, ny = s->vy + dy / d * sp * hs, nl = sqrtf(nx * nx + ny * ny);
            if (nl > 0.01f) { s->vx = nx / nl * sp; s->vy = ny / nl * sp; }
        }
    }
    if (s->age % 2 == 1) {                                                           /* tear.FrameCount % 2 == 1: the embers' trail */
        for (unsigned k = 0; k < n; k++) {                                           /* its Size-1 fire hurts once: 1.4, fire, countdown 4 */
            if (!fb_enemy_ok(snap[k], lvl)) continue;
            float dx = fpos(snap[k])[0] - s->x, dy = fpos(snap[k])[1] - s->y, r = 1.0f + fsize(snap[k]);
            if (dx * dx + dy * dy <= r * r) fb_deal2(snap[k], 1.4f, DMG_FIRE, 4, player, 0, 1);
        }
        ember_make(s->x, s->y, 0, 0, 7.0f, 0.0f, player, 0, 1, s->fiendish);                      /* the Lua's Fire Jet plume, drawn with a short ember */
    }
    float sp = sqrtf(s->vx * s->vx + s->vy * s->vy);
    if (sp < 5.0f) { shot_die(s, 0, player); return; }
    int steps = (int) (sp / 8.0f) + 1; float sx = s->vx / (float) steps, sy = s->vy / (float) steps;
    for (int st = 0; st < steps; st++) {
        float nx = s->x + sx, ny = s->y + sy;
        if (fb_blocked(nx, ny)) { FBLOGN(10, 12, "fffireball: shot burst on grid collision class %d (pathfinder cell %#x) at %.0f,%.0f", fb_tile(nx, ny), grid_raw_at(nx, ny), nx, ny); shot_die(s, 0, player); return; }              /* tear:Die on a wall or a rock */
        s->x = nx; s->y = ny;
        for (unsigned k = 0; k < g_nfire; k++) {                                       /* a fireplace is an obstacle that is not on the grid */
            if (!alive(g_fire[k])) continue;
            float dx = fpos(g_fire[k])[0] - s->x, dy = fpos(g_fire[k])[1] - s->y, r = FB_SHOT_RADIUS + 0.6f * fsize(g_fire[k]);
            if (dx * dx + dy * dy < r * r) { FBLOGN(11, 6, "fffireball: shot burst on a fireplace"); shot_die(s, 0, player); return; }
        }
        for (unsigned k = 0; k < n; k++) {
            if (!fb_enemy_ok(snap[k], lvl)) continue;
            float dx = fpos(snap[k])[0] - s->x, dy = fpos(snap[k])[1] - s->y, r = FB_SHOT_RADIUS + fsize(snap[k]);
            if (dx * dx + dy * dy < r * r) { if (s->e) { fpos(s->e)[0] = s->x; fpos(s->e)[1] = s->y; } shot_hit(s, snap[k], player); return; }
        }
    }
    if (s->e) { fpos(s->e)[0] = s->x; fpos(s->e)[1] = s->y; fvel(s->e)[0] = 0; fvel(s->e)[1] = 0; shot_anim(s); if (s->age == 1 || s->age == 4) fb_color_dump("shot", s->e); fb_color_plain(s->e, 1.0f); }
    if (s->age > 400) shot_die(s, 0, player);                                        /* a shot that never lands is not left flying */
}

/* ---------------------------------------------------------------------------------------------- launch and orbit */
static void fb_drop_orbiter(fbp_t *st, int k) {
    if (st->fam[k] && fb_is(st->fam[k], 3, V_WISP)) fb_kill(st->fam[k]);
    for (int i = k; i < FB_MAXB - 1; i++) { st->fam[i] = st->fam[i + 1]; st->fage[i] = st->fage[i + 1]; st->fis[i] = st->fis[i + 1]; }
    st->fam[FB_MAXB - 1] = 0; st->fage[FB_MAXB - 1] = 0; st->fis[FB_MAXB - 1] = 0;
}
static int fb_launch(fbp_t *st, unsigned long p, float dx, float dy) {
    if (st->balls <= 0 || st->ticks - st->last_launch < FB_COOLDOWN) return 0;
    float ix = fpos(p)[0] + dx * 10.0f, iy = fpos(p)[1] + dy * 10.0f, bd = 1e30f; int pick = -1;
    for (int k = 0; k < FB_MAXB; k++) {
        if (!st->fam[k] || !fb_is(st->fam[k], 3, V_WISP)) continue;
        float ex = fpos(st->fam[k])[0] - ix, ey = fpos(st->fam[k])[1] - iy, d = ex * ex + ey * ey;
        if (d < bd) { bd = d; pick = k; }
    }
    int slot = -1; for (int i = 0; i < 8; i++) if (!g_sh[i].used) { slot = i; break; }
    if (slot < 0) return 0;
    shot_t *s = &g_sh[slot]; memset(s, 0, sizeof *s);
    s->used = 1; s->fiendish = pick >= 0 && st->fis[pick]; s->owner = p; s->x = fpos(p)[0]; s->y = fpos(p)[1]; s->vx = dx * FB_SPEED; s->vy = dy * FB_SPEED; s->anim = -1;
    float d = *(const float *) (p + P_DAMAGE); s->dmg = (d > 3.5f ? d : 3.5f) * FB_TEAR_MULT;
    s->e = fb_make(T_EFFECT, V_SHOT, 0, s->x, s->y, 0, 0, p);
    if (s->e) { fb_scale(s->e, FB_SHOT_SCALE); { float *tn = (float *) (s->e + E_SPRITE + 0xe8), *of = (float *) (s->e + E_SPRITE + 0x108); tn[0] = tn[1] = tn[2] = tn[3] = 1.0f; of[0] = of[1] = of[2] = 0.0f; } if (s->fiendish && g_sheet) g_sheet((void *) (s->e + E_SPRITE), FB_SHEET_SHOT_FIENDISH); shot_anim(s); }
    fb_sound(SND_LAUNCH, 0.4f, fb_rf(0.9f, 1.1f));
    fb_sound(SND_FLAMEEND, 0.3f, fb_rf(0.9f, 1.1f));
    fb_sound(SND_BURST, 0.3f, fb_rf(0.9f, 1.1f));
    if (pick >= 0) fb_drop_orbiter(st, pick);
    st->balls--; st->last_launch = st->ticks;
    FBLOG(4, "fffireball: a %sfireball launched (%.0f,%.0f) -> %.0f,%.0f, damage %.2f, %d left%s", s->fiendish ? "FIENDISH " : "", s->x, s->y, dx, dy, s->dmg, st->balls, s->e ? "" : " - NO shot entity (is 1000.1752 in entities2.xml?)");
    return 1;
}
/* before the game's own update: the same question its detector asks (0x5185a8, window opened by the port: count <= 0) */
static void fb_dtap(unsigned long p, fbp_t *st) {
    static const int acts[4] = { 4, 6, 5, 7 };                                       /* shoot left, up, right, down */
    static const float dirs[4][2] = { { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 } };
    int ctl = *(const int *) (p + FB_CTRL), last = *(const int *) (p + FB_TAP_LAST), cnt = *(const int *) (p + FB_TAP_COUNT);
    if (!g_trigger || st->balls <= 0) return;
    for (int d = 0; d < 4; d++)
        if (g_trigger(acts[d], ctl) && last == d && cnt <= 0) { fb_launch(st, p, dirs[d][0], dirs[d][1]); return; }
}
static void fb_sync_orbiters(fbp_t *st, unsigned long p) {
    int have = 0;
    for (int k = 0; k < FB_MAXB; k++) {
        if (st->fam[k] && !fb_is(st->fam[k], 3, V_WISP)) { st->fam[k] = 0; }
    }
    for (int k = 0, o = 0; k < FB_MAXB; k++) if (st->fam[k]) { st->fam[o] = st->fam[k]; st->fage[o] = st->fage[k]; st->fis[o] = st->fis[k]; if (o != k) { st->fam[k] = 0; st->fage[k] = 0; st->fis[k] = 0; } o++; }
    for (int k = 0; k < FB_MAXB; k++) if (st->fam[k]) have++;
    while (have > st->balls && have > 0) { fb_drop_orbiter(st, have - 1); have--; }
    while (have < st->balls && have < FB_MAXB) {
        unsigned long f = fb_make(3, V_WISP, 0, fpos(p)[0], fpos(p)[1], 0, 0, p);
        if (!f) { FBLOG(5, "fffireball: Spawn(3.%d) returned nothing - is the familiar in entities2.xml?", V_WISP); break; }
        *(unsigned long *) (f + FAMILIAR_PLAYER) = p;
        ((float *) (f + E_SPRITE + 0xe8))[3] = 0.0f;
        st->fam[have] = f; st->fage[have] = 0; st->fis[have] = 0; have++;
        fb_sound(SND_CANDLE, 1.0f, 1.0f);
        FBLOG(6, "fffireball: an orbiting fireball (3.%d) made", V_WISP);
    }
}

/* the familiar's own update has run: put it on its ring (fiend.lua:769-790) */
void fb_familiar_update(unsigned long f) {
    if (*(const int *) (f + E_TYPE) != 3 || *(const int *) (f + E_VARIANT) != V_WISP) return;
    unsigned long p = *(const unsigned long *) (f + FAMILIAR_PLAYER);
    fbp_t *st = p ? fb_state(p, 0) : 0;
    int idx = -1, n = 0;
    if (st) for (int k = 0; k < FB_MAXB; k++) if (st->fam[k]) { if (st->fam[k] == f) idx = n; n++; }
    if (idx < 0) { fb_kill(f); FBLOG(1, "fffireball: an orbiter nobody owns was removed (it would stay where it is)"); return; }
    float a = st->ang + 6.2831853f * (float) idx / (float) n;
    fpos(f)[0] = fpos(p)[0] + fvel(p)[0] + FB_ORBIT_R * cosf(a);
    fpos(f)[1] = fpos(p)[1] + fvel(p)[1] + FB_ORBIT_R * sinf(a);
    fvel(f)[0] = 0; fvel(f)[1] = 0;
    /* NOT sprite+0x108: that is the Color's OFFSET rgb (ffchars.c: tint 0, colorize 0x10, offset 0x20 from the Color at sprite+0xe8). Writing the Lua's
     * SpriteOffset (0,-14) there made green -14 - the red orange of the first real run. The lift is baked into wisp.anm2 (root frame Y -14). */
    g_play((void *) (f + E_SPRITE), "MoveHori", 0);
    for (int k = 0; k < FB_MAXB; k++) if (st->fam[k] == f) {
        if (!st->fis[k] && fb_is_fiend(p) && fb_birthright(p)) {                    /* fiend.lua:778-783: Birthright makes it a fiendish fire, sprite and all */
            st->fis[k] = 1;
            if (g_sheet) g_sheet((void *) (f + E_SPRITE), FB_SHEET_WISP_FIENDISH);
            FBLOG(3, "fffireball: an orbiter turned fiendish (Birthright)");
        }
        { static int dumped; if (st->fage[k] == 2 && dumped++ < 4) fb_color_dump("orbiter", f); }
        fb_color_plain(f, st->fage[k] <= 5 ? (float) st->fage[k] / 5.0f : 1.0f);
        { static int told; if (told++ < 3) LOG("fffireball: orbiter %p at %.0f,%.0f age %d", (void *) f, fpos(f)[0], fpos(f)[1], st->fage[k]); }
    }
}

/* ---------------------------------------------------------------------------------------------- the player's frame */
void fb_before_update(unsigned long p) {
    if (!fb_is_fiend(p)) return;
    fbp_t *st = fb_state(p, 1); if (!st) return;
    g_fb_any = 1;
    int frame = g_frame_fn ? g_frame_fn() : 0;
    if (g_fb_last_frame > 60 && frame <= 30) {                           /* a new run: nothing of the last one stays */
        memset(g_fp, 0, sizeof g_fp); memset(g_sh, 0, sizeof g_sh); memset(g_em, 0, sizeof g_em); memset(g_tm, 0, sizeof g_tm); memset(g_pool, 0, sizeof g_pool);
        fb_targets_dirty(); st = fb_state(p, 1); LOG("fffireball: the frame count went back (%d -> %d): a new run, the fireballs start from none", g_fb_last_frame, frame);
    }
    (void) frame;
    fb_dtap(p, st);
}
void fb_after_update(unsigned long p) {
    if (!fb_is_fiend(p)) return;
    fbp_t *st = fb_state(p, 1); if (!st) return;
    int frame = g_frame_fn ? g_frame_fn() : 0;
    st->ticks++;
    fb_sync_orbiters(st, p);
    if (frame == g_fb_world_frame) return;                                           /* the world below moves once a game frame, not once a player */
    g_fb_world_frame = frame; g_fb_last_frame = frame; g_wt++;
    { unsigned long rm = fb_room_sig(); if (rm != g_fb_room) { g_fb_room = rm; g_grid_dump_in = 20; g_start_wait = 4; g_nstart = 0; memset(g_kw, 0, sizeof g_kw); FBLOGN(12, 12, "fffireball: new room, grid %dx%d, the player (%.0f,%.0f) stands on a tile of value %d", g_gw, g_gh, fpos(p)[0], fpos(p)[1], g_gw ? fb_tile(fpos(p)[0], fpos(p)[1]) : -1); memset(g_sh, 0, sizeof g_sh); memset(g_em, 0, sizeof g_em); memset(g_tm, 0, sizeof g_tm); memset(g_pool, 0, sizeof g_pool); fb_targets_dirty(); } }   /* a new room: what was flying is gone with it */
    for (int i = 0; i < FB_MAXP; i++) if (g_fp[i].p) {
        g_fp[i].ang += FB_ORBIT_STEP;
        for (int k = 0; k < FB_MAXB; k++) if (g_fp[i].fam[k]) g_fp[i].fage[k]++;
    }
    for (int i = 0; i < 8; i++) if (g_sh[i].used) shot_tick(&g_sh[i]);
    ember_tick();
    fb_kill_watch();
    tm_run();
}
#endif
