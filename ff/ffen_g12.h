/*
 * Fiend Folio's machines and beggars (entity type 6, "slots"): ferrium/items/slots/*.lua, erfly/slots/*.lua, extraSlots.lua.
 *
 * How they run. The Lua runs each machine from MC_POST_SLOT_UPDATE (its state machine and sprite) and MC_POST_SLOT_COLLISION (FiendFolio.onMachineTouch:
 * pay, hug...). The port has neither callback, so the machines are polled once per tick like the puzzle keys (ffen_g9.h): every type-6 entity whose variant is
 * one of the mod's is found in the room's list, its animation is kept by the port's own frame counter (en_play / en_finished / en_event, the table of
 * ffanim_table.h) and written to the sprite every tick (en_slot_sprite: the engine's Entity_Slot::Update does not know these variants), and "touching" is the
 * Lua's own test, the machine's Size plus the player's Size (the engine's slot collision fires while they overlap; the machine is solid so the player stays at
 * that distance - a margin of 2 covers the push).
 *
 * What a machine remembers for the floor (FiendFolio.savedata.run.level.SlotData[InitSeed]) is kept in g_sd, keyed by variant, position and the room's
 * descriptor, and forgotten when the floor changes. The Lua's RNG (seeded by InitSeed) is a xorshift seeded by that key.
 * Bombs: the Lua's FiendFolio.OverrideExplosionHack asks whether the machine's GridCollisionClass became GRIDCOLL_GROUND (5), which the engine does when an
 * explosion reaches a slot; the machine's DropFunc runs once and (unless it asked not to) the machine is removed.
 *
 * Not done (each logs "ffslot: not done" once): music (the Jukebox only plays its animations: no MusicManager in the port), Bethany's essence wisps
 * (doBethEssenceCheck), the shampoo bottle's texts, fortune texts, Daemon's Tail / Thank You Fossil payout boosts, STATE_BUM_KILLED / BUM_LEFT level flags, the
 * mod's own cards / items / trinkets as prizes (a random vanilla card / item / trinket takes the place), Magdalene B's skin, friend tears killing the Hug Beggar.
 */
#include <stdlib.h>

/* ---- the player's purse: Entity_Player::AddCoins 0x4f0a14 / AddKeys 0x4f0ca0 / AddBombs 0x4f0d20 (measured: int at +0x16e0 / +0x16dc / +0x16d4, clamped 0..99) */
#define OFF_PL_COINS 0x16e0
#define OFF_PL_KEYS  0x16dc
#define OFF_PL_BOMBS 0x16d4
static void (*g_add_coins)(unsigned long, int);
static void (*g_add_keys)(unsigned long, int);
static void (*g_add_bombs)(unsigned long, int);
static void (*g_slot_hook_init)(void);
static int pl_get(unsigned long pl, int off) { return *(const int *) (pl + off); }
static void pl_add(unsigned long pl, int off, int n, void (*fn)(unsigned long, int)) {
    if (fn) { fn(pl, n); return; }                                    /* the host tests have no engine: the same clamp, by hand */
    int v = pl_get(pl, off) + n; if (v > 99) v = 99; if (v < 0) v = 0; *(int *) (pl + off) = v;
}
static int  pl_coins(unsigned long pl) { return pl_get(pl, OFF_PL_COINS); }
static int  pl_keys(unsigned long pl)  { return pl_get(pl, OFF_PL_KEYS); }
static int  pl_bombs(unsigned long pl) { return pl_get(pl, OFF_PL_BOMBS); }
static void pl_add_coins(unsigned long pl, int n) { pl_add(pl, OFF_PL_COINS, n, g_add_coins); }
static void pl_add_keys(unsigned long pl, int n)  { pl_add(pl, OFF_PL_KEYS, n, g_add_keys); }
static void pl_add_bombs(unsigned long pl, int n) { pl_add(pl, OFF_PL_BOMBS, n, g_add_bombs); }

/* the players: Game+0x25c40 is the list the game's PlayerManager keeps; 4da574(list, i) is its accessor (first_player() uses i = 0) */
static unsigned long (*g_player_n)(int);
static unsigned long sl_player(int i) {
    if (g_player_n) return g_player_n(i);
    return i == 0 && g_first_player ? g_first_player() : 0;
}
static int sl_nplayers(void) {
    unsigned long game = en_game(); if (!game || !g_base) return g_first_player && g_first_player() ? 1 : 0;
    const unsigned long *l = (const unsigned long *) (game + GAME_PLAYER_LIST);
    if (!l[0] || l[1] < l[0]) return 0;
    int n = (int) ((l[1] - l[0]) / 8); return n > 4 ? 4 : n;
}

/* ---- what a machine remembers for the floor */
typedef struct { int used, var, x, y; unsigned long room; int a[8]; unsigned rng; } sd_t;
#define SD_N 96
static sd_t g_sd[SD_N]; static int g_sd_floor = -1, g_sd_next;
static unsigned long sl_room_key(void) {
    unsigned long r = g_room ? g_room() : 0;
    return r ? *(const unsigned long *) (r + 8) : 0;                  /* Room+8 = the room's descriptor (eid_craft.c ROOM_DESCRIPTOR): one per room of a floor */
}
static sd_t *sd_get(unsigned long e) {
    unsigned long game = en_game();
    int floor = game ? *(const int *) game * 16 + *(const int *) (game + 4) : 0;       /* Game+0 stage, +4 its type (Level::Init reads them there) */
    if (floor != g_sd_floor) { memset(g_sd, 0, sizeof g_sd); g_sd_floor = floor; g_sd_next = 0; }
    int var = *(const int *) (e + E_VARIANT), x = (int) fpos(e)[0], y = (int) fpos(e)[1]; unsigned long room = sl_room_key();
    for (int i = 0; i < SD_N; i++) {
        sd_t *d = &g_sd[i];
        if (d->used && d->var == var && d->room == room && abs(d->x - x) < 8 && abs(d->y - y) < 8) return d;
    }
    sd_t *d = &g_sd[g_sd_next++ % SD_N]; memset(d, 0, sizeof *d);
    d->used = 1; d->var = var; d->x = x; d->y = y; d->room = room;
    d->rng = (unsigned) (var * 2654435761u) ^ (unsigned) (x * 40503) ^ (unsigned) (y * 9973) ^ (unsigned) room; if (!d->rng) d->rng = 1;
    return d;
}
static unsigned sd_next(sd_t *d) { unsigned x = d->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; d->rng = x; return x; }
static int sd_rand(sd_t *d, int n) { return n > 0 ? (int) (sd_next(d) % (unsigned) n) : 0; }              /* RNG:RandomInt(n) */
static int sd_range(sd_t *d, int lo, int hi) { return lo + sd_rand(d, hi - lo + 1); }                    /* mod:RandomInt(lo, hi, rng) */
static int sd_roll(sd_t *d, int lo, int hi) { return sd_range(d, lo, hi); }                              /* mod:getRoll */
static int sd_croll(sd_t *d, int lo, int hi, int chance) { return sd_roll(d, 1, 100) <= chance ? sd_roll(d, lo, hi) : 0; }   /* one row of mod:compoundRoll: how many, 0 if the roll failed */

/* ---- a machine's sprite: the port's own frame counter drives it every tick */
static void sl_sprite(en_t *s, unsigned long e) {
    if (!g_setframe || !s->anim[0] || s->alen < 1) return;
    int f = s->aloop ? s->af % s->alen : (s->af < s->alen ? s->af : s->alen - 1);
    g_setframe((void *) (e + E_SPRITE), s->anim, f);
    if (s->ov[0]) {
        const ffa_t *a = ffa_find(s->key, s->ov);
        if (a && a->len > 0) en_set_overlay_frame(e, s->ov, a->loop ? s->ovf % a->len : (s->ovf < a->len ? s->ovf : a->len - 1));
    }
}
static void sl_overlay_restart(en_t *s, const char *name) { s->ov[0] = 0; en_overlay(s, name); }     /* sprite:PlayOverlay(name, true) */
static int sl_overlay_playing(const en_t *s, const char *name) {
    const ffa_t *a = ffa_find(s->key, name); return a && !strcmp(s->ov, name) && (a->loop || s->ovf < a->len);
}
static void sl_tick_frames(en_t *s) {
    s->af++; if (s->ov[0]) s->ovf++;
    if (s->aloop && s->alen > 0 && s->af >= s->alen) s->af = 0;
}

/* ---- touching: FiendFolio.onMachineTouch (MC_POST_SLOT_COLLISION): the machine's Size + the player's Size, or the box of a machine with SizeMulti */
static int sl_touch(unsigned long e, unsigned long pl, float mx, float my) {
    const float *p = fpos(e), *q = fpos(pl);
    float sz = *(const float *) (e + E_SIZE), pz = *(const float *) (pl + E_SIZE), dx = q[0] - p[0], dy = q[1] - p[1];
    if (mx != 1.0f || my != 1.0f) return fabsf(dx) <= sz * mx + pz + 2.0f && fabsf(dy) <= sz * my + pz + 2.0f;
    return vlen(dx, dy) <= sz + pz + 2.0f;
}
static unsigned long sl_toucher(unsigned long e, float mx, float my) {
    int n = sl_nplayers();
    for (int i = 0; i < n; i++) { unsigned long pl = sl_player(i); if (pl && sl_touch(e, pl, mx, my)) return pl; }
    return 0;
}
static int sl_exploded(unsigned long e) { return ent_gcoll_ok() && *(const int *) (e + OFF_ENT_GRIDCOLL) == 5; }          /* FiendFolio.OverrideExplosionHack's test */
static void sl_toss(unsigned long e, sd_t *d, int var, int sub, float x, float y) {                       /* Vector(0, rand(25,50)/10):Rotated(rand(-50,50)) */
    float vx, vy; vrot(0, sd_range(d, 25, 50) / 10.0f, (float) sd_range(d, -50, 50), &vx, &vy);
    en_spawn(5, var, x, y, vx, vy, e, sub);
}
static void sl_scatter(unsigned long e, sd_t *d, int var, int sub, float x, float y) {                    /* RandomVector() * rand(45) / 10 */
    float vx, vy; en_rand_vec(&vx, &vy, sd_rand(d, 46) / 10.0f);
    en_spawn(5, var, x, y, vx, vy, e, sub);
}
static void sl_free_pos(float x, float y, float *ox, float *oy) { *ox = x; *oy = y; }                       /* Room:FindFreePickupSpawnPosition: the pedestal goes where asked (the room's own rule moves it off rocks when it spawns) */
#define SL_NOTDONE(msg) FFLOG(FFC_NOT_DONE, 70000 + __LINE__, "ffslot: not done: %s", msg)      /* msg is data, never the format (two of them hold a %) */

/* ============================================================================================ Penny Press 6.1043 (ferrium/items/slots/pennypress.lua)
 * Idle1 -> (11 coins) Initiate (clapping, a trinket at its Prize event) -> Idle2. A bomb breaks it (Death) and it pays coins. state: 0 Idle, 1 Playing, 2 Played, 3 Broken */
#define SLV_PENNY 1043
static void sl_penny(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) {
        s->init = 1; s->age = 0; s->state = d->a[0] ? 2 : 0; en_replay(s, "Idle1");
        if (!d->a[7]) { d->a[7] = 1; d->a[1] = 0; }
    }
    if (s->state != 3 && sl_exploded(e)) {                              /* DropFunc: 1-3 coins (100%), 1-2 (40%), 1 (50%) */
        int n = sd_croll(d, 1, 3, 100) + sd_croll(d, 1, 2, 40) + sd_croll(d, 1, 1, 50);
        for (int j = 0; j < n; j++) { float vx, vy; en_rand_vec(&vx, &vy, sd_rand(d, 45) / 10.0f); en_spawn(5, 20, p[0], p[1], vx, vy, e, 0); }
        s->state = 3;
    }
    if (s->state == 0) en_play(s, "Idle1");
    else if (s->state == 2) en_play(s, "Idle2");
    else if (s->state == 3) en_play(s, "Death");
    else if (s->state == 1) {
        if (en_finished(s, "Initiate")) s->state = 2;
        else if (en_event(s, "Initiate", "Prize")) {
            en_sound(SOUND_SLOTSPAWN, 1.0f, 1.0f); d->a[0] = 1; s->i[0] = 0;
            float vx, vy; vrot(0, sd_range(d, 25, 50) / 10.0f, (float) sd_range(d, -50, 50), &vx, &vy);
            en_spawn(5, 350, p[0], p[1], vx, vy, e, 0);                 /* the mod's penny trinkets: a random trinket */
            SL_NOTDONE("Penny Press: the prize is a random vanilla trinket (the mod's penny trinkets are not ported)");
        } else if (en_event(s, "Initiate", "Sound1")) en_sound(SOUND_CLAP, 0.6f, sd_range(d, 220, 240) / 100.0f);
        else if (en_event(s, "Initiate", "Sound2")) s->i[0] = 1;
        else en_play(s, "Initiate");
    }
    if (s->i[0]) en_sound(SOUND_CLAP, 0.6f, sd_range(d, 350, 380) / 100.0f);     /* data.clicking */
    if (s->state == 0) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl && pl_coins(pl) > 10) { pl_add_coins(pl, -11); en_sound(SOUND_COIN_SLOT, 1.0f, 1.0f); s->state = 1; }
    }
}

/* ============================================================================================ Dice Dispenser 6.1042 (ferrium/items/slots/dicedispenser.lua)
 * 4 coins: 3 * plays % to break it (Initiate -> an explosion, 3 glass dice or a dice-pool item), otherwise a glass die (Prize event). state: 0 Idle, 1 Initiate, 2 Prizes, 3 Breaking, 4 Broken */
#define SLV_DICE 1042
static void sl_dice(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) { s->init = 1; s->age = 0; s->state = 0; en_replay(s, "Idle"); }
    if (s->state < 3 && sl_exploded(e) && !s->i[2]) {                   /* DropFunc: a die (100%), another (30%), coins for the two rows the Lua's i > 2 branch takes (25% x1, 40% x1-2) */
        s->i[2] = 1;
        int dice = sd_croll(d, 1, 1, 100) + sd_croll(d, 1, 1, 30), coins = sd_croll(d, 1, 1, 25) + sd_croll(d, 1, 2, 40);
        for (int j = 0; j < dice; j++) sl_scatter(e, d, 300, 0, p[0], p[1]);
        for (int j = 0; j < coins; j++) sl_scatter(e, d, 20, 0, p[0], p[1]);
        s->state = 3; s->i[3] = 1;
    }
    if (s->state == 0) en_play(s, "Idle");
    else if (s->state == 2) {
        if (en_finished(s, "Prize")) s->state = 0;
        else if (en_event(s, "Prize", "Prize")) { en_sound(SOUND_SLOTSPAWN, 1.0f, 1.0f); float vx, vy; vrot(0, sd_range(d, 25, 50) / 10.0f, (float) sd_range(d, -50, 50), &vx, &vy); en_spawn(5, 300, p[0] + 10, p[1], vx, vy, e, 0); }
        else en_play(s, "Prize");
    } else if (s->state == 1) {
        if (en_finished(s, "Initiate")) {
            if (s->i[0] == 0) s->state = 2;                             /* Prize == "Dice" */
            else {                                                      /* "Break" */
                s->state = 3; s->i[2] = 1; s->i[3] = 1;
                en_spawn(1000, 1, p[0], p[1], 0, 0, e, 0); en_sound(SOUND_BOSS1_EXPLOSIONS, 1.0f, 1.0f);       /* Isaac.Explode(pos, slot, 0): the effect and the boom (it hurts nobody) */
                if (sd_rand(d, 100) < 85 && !s->i[1]) for (int j = 0; j < 3; j++) sl_scatter(e, d, 300, 0, p[0], p[1]);
                else { en_spawn(5, 100, p[0], p[1], 0, 0, e, 0); en_remove(e); return; }   /* a dice-pool item (the mod's pool: a random item of the room's) */
            }
        } else en_play(s, "Initiate");
    } else if (s->state == 3) {
        if (en_finished(s, "Death")) s->state = 4; else en_play(s, "Death");
    } else if (s->state == 4) en_replay(s, "Broken");
    if (s->state == 0) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl && pl_coins(pl) > 3) {
            d->a[1]++; pl_add_coins(pl, -4); en_sound(SOUND_COIN_SLOT, 1.0f, 1.0f);
            int prize = sd_rand(d, 100), num = 3 * d->a[1], tn = 3;
            if (g_has_item && g_has_item(pl, 202, 0)) tn *= 2;        /* Lucky Foot */
            s->state = 1;
            if (prize > num) s->i[0] = 0; else { if (num <= tn) s->i[1] = 1; s->i[0] = 1; }
            SL_NOTDONE("Dice Dispenser: the prizes are a random vanilla card (the mod's glass dice are not ported)");
        }
    }
}

/* ============================================================================================ Jukebox 6.1041 (erfly/slots/jukebox.lua)
 * Idle follows the beat (frame = FrameCount * 16 / speed), the overlay plays; touching it plays Change Song. The music itself is not done. state: 0 Idle, 1 Switch */
#define SLV_JUKEBOX 1041
static void sl_jukebox(en_t *s, unsigned long e) {
    float *p = fpos(e); (void) p;
    if (!s->init) { s->init = 1; s->age = 0; s->state = 0; en_replay(s, "Idle"); s->f[0] = 17.31f; SL_NOTDONE("Jukebox: it only plays its animations, the songs are not played"); }
    if (s->i[3] == 0 && sl_exploded(e)) {                              /* DropFunc: Death; 1/20 a random object / Broken Record, 2/20 1-2 coins */
        s->i[3] = 1; en_overlay_off(s); en_play(s, "Death");
        int r = rint_(0, 19);
        if (r == 0) en_spawn(5, 100, p[0], p[1], 0, 0, e, 0);
        else if (r <= 2) for (int k = rint_(1, 2); k > 0; k--) { float vx, vy; en_rand_vec(&vx, &vy, 1.0f); en_spawn(5, 20, p[0] + vx, p[1] + vy, vx * 5, vy * 5, e, 0); }
    }
    if (s->i[3]) { if (!en_playing(s, "Death")) en_play(s, "Broken"); return; }
    if (s->state == 0) {
        float calc = (float) s->age * (16.0f / s->f[0]); int frame = (int) fmodf(calc, 16.0f);
        en_set_frame(s, "Idle", frame); s->aloop = 1; s->alen = 16; s->af = frame;
        en_overlay(s, "Idle Overlay");
    } else {
        if (en_finished(s, "Change Song")) { static const float speed[10] = { 17.31f, 14.0f, 10.1f, 19.14f, 15.0f, 12.27f, 17.0f, 14.25f, 17.85f, 12.75f };
            s->state = 0; s->i[4] = (s->i[4] + 1) % 10; s->f[0] = speed[s->i[4]]; s->i[5] = 1; en_replay(s, "Idle"); }
        else en_play(s, "Change Song");
    }
    if (s->state == 0 && !s->i[5]) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl) { s->state = 1; en_overlay_off(s); en_replay(s, "Change Song"); }
    }
    s->i[5] = 0;
}

/* ============================================================================================ Hug Beggar 6.1037 (erfly/slots/hugbeggar.lua)
 * Touch it and it hugs you: the player is pulled to its side (Hug<Side>); walking away after the hug ends makes it leave (Teleport) leaving a heart (1 in 90 it is a
 * Mug Beggar, which takes a coin: TeleportMug). A bomb pays the heart and blows it up. state: 0 waiting, 1 hugging, 2 leaving */
#define SLV_HUG 1037
static void sl_hug_blow(unsigned long e, sd_t *d, float *p) {            /* explodeHugBeggar */
    (void) d;
    en_spawn(1000, 2, p[0], p[1], 0, 0, e, 0); en_spawn(1000, 16, p[0], p[1], 0, 0, e, 3); en_spawn(1000, 16, p[0], p[1], 0, 0, e, 4);
    for (int i = 0; i < 10; i++) { float vx, vy; en_rand_vec(&vx, &vy, (float) rint_(5, 50) / 10.0f); en_spawn(1000, 5, p[0], p[1], vx, vy, e, rint_(0, 2)); }
}
static void sl_hug(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) {
        s->init = 1; s->age = 0; s->state = 0; en_replay(s, "IdleSad");
        if (sd_rand(d, 90) == 0) { s->i[0] = 1; SL_NOTDONE("Hug Beggar: the Mug Beggar's skin (layer crop) is not switched"); }
    }
    if (s->state != 2 && sl_exploded(e)) {                                  /* DropFunc, then explodeHugBeggar */
        if (s->state != 2) en_spawn(5, 10, p[0], p[1], 0, 0, e, 0);
        sl_hug_blow(e, d, p); en_remove(e); return;
    }
    if (s->state == 2) {
        if (en_finished(s, "Teleport") || en_finished(s, "TeleportMug")) { en_remove(e); return; }
        en_play(s, s->i[1] ? "TeleportMug" : "Teleport");
    } else if (s->state == 1 && s->ref) {
        unsigned long pl = s->ref; const char *hug = s->i[2] ? "HugRight" : "HugLeft";
        float *pp = fpos(pl), *pv = fvel(pl);
        if (en_finished(s, hug) && vlen(pv[0], pv[1]) > 1.0f) {
            s->state = 2; en_set_collision(e, ENTCOLL_NONE);
            float vx, vy; vrot(0, 2.0f, (float) rint_(-30, 30), &vx, &vy);
            en_spawn(5, 10, p[0], p[1], vx, vy, e, 0);
            en_sound(SOUND_FETUS_JUMP, 1.0f, 1.0f);
            if (s->i[0] && pl_coins(pl) > 0) { pl_add_coins(pl, -1); s->i[1] = 1; }                 /* the Mug Beggar: cursed penny sound is the mod's, not played */
        } else {
            en_play(s, hug);
            float tx = p[0] + (s->i[2] ? 25.0f : -25.0f), ty = p[1] - 5.0f;
            pv[0] = lerpf(pv[0], tx - pp[0], 0.1f); pv[1] = lerpf(pv[1], ty - pp[1], 0.1f);
            pp[0] = lerpf(pp[0], tx, 0.3f); pp[1] = lerpf(pp[1], ty, 0.3f);
        }
    } else {
        if (d->a[0]) { en_remove(e); return; }                              /* it has hugged: gone when you come back */
        unsigned long best = 0; float bd = 300.0f;
        for (int i = 0; i < sl_nplayers(); i++) { unsigned long pl = sl_player(i); if (!pl) continue; float dd = vlen(fpos(pl)[0] - p[0], fpos(pl)[1] - p[1]); if (dd < bd) { bd = dd; best = pl; } }
        if (best && bd < 125.0f) { if (en_playing(s, "IdleSad")) en_replay(s, "Happy"); }
        else if (best && bd > 150.0f) { if (en_playing(s, "IdleHappy")) en_replay(s, "Sad"); }
        if (en_finished(s, "Happy")) en_replay(s, "IdleHappy"); else if (en_finished(s, "Sad")) en_replay(s, "IdleSad");
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl) { s->ref = pl; s->state = 1; s->i[2] = fpos(pl)[0] > p[0]; d->a[0] = 1; en_sound(SOUND_FETUS_LAND, 1.0f, 1.0f); }
    }
}

/* ============================================================================================ Cell Game 6.1036 (ferrium/items/slots/cellgame.lua)
 * A key buys a shuffle (PayShuffle with a Prize<n> overlay); touching it again picks the shell (by x): win = the prize, lose = a Skuzz. 3% the machine leaves with a
 * pedestal (Sayonara). state: 0 Idle, 1 Reveal, 2 Shuffled, 3 Prizes, 4 Sayonara. i[0] prize, i[1] shell, i[2] win. A box 3 x 0.75 of its Size is its touching area. */
#define SLV_CELL 1036
static int sl_count(int type, int variant) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); int c = 0;
    for (unsigned k = 0; k < n; k++) if (alive(snap[k]) && *(const int *) (snap[k] + E_TYPE) == type && *(const int *) (snap[k] + E_VARIANT) == variant) c++;
    return c;
}
static void sl_cell(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) { s->init = 1; s->age = 0; s->state = 0; en_replay(s, "Idle"); }
    if (s->state != 4 && sl_exploded(e)) {                                  /* DropFunc: blood, then coins / keys / pickups, and it is gone */
        int c = sd_croll(d, 1, 1, 100) + sd_croll(d, 1, 3, 30), k = sd_croll(d, 1, 2, 40), m = sd_croll(d, 1, 1, 40) + sd_croll(d, 1, 2, 25);
        en_spawn(1000, 2, p[0], p[1] - 20, 0, 0, e, 3);
        for (int j = 0; j < c; j++) sl_scatter(e, d, 20, 0, p[0], p[1]);
        for (int j = 0; j < k; j++) sl_scatter(e, d, 30, 0, p[0], p[1]);
        for (int j = 0; j < m; j++) sl_scatter(e, d, 0, 1, p[0], p[1]);
        en_remove(e); return;
    }
    int shellx = (int) (-2 + s->i[1]) * 25;
    if (s->state == 0) en_play(s, "Idle");
    else if (s->state == 1) {
        if (en_finished(s, "PayShuffle")) { en_overlay_off(s); s->state = 2; }
        else if (en_event(s, "PayShuffle", "Shuffle")) en_sound(SOUND_SHELLGAME, 1.0f, 1.0f);
        else { char o[16]; en_play(s, "PayShuffle"); snprintf(o, sizeof o, "Prize%d", s->i[0]); en_overlay(s, o); }
    } else if (s->state == 3) {
        char a[24]; snprintf(a, sizeof a, "Shell%dPrize", s->i[1]);
        if (en_finished(s, a)) s->state = 0;
        else if (en_event(s, a, "Prize")) {
            float sx = p[0] + (float) shellx, sy = p[1] + 10.0f;
            if (s->i[2]) {
                en_sound(SOUND_SLOTSPAWN, 1.0f, 1.0f);
                int common = sd_rand(d, 2) + 3, pr = s->i[0];
                if (pr == 0) for (int i = 0; i < common + 2; i++) sl_toss(e, d, 20, 0, sx, sy);
                else if (pr == 1) for (int i = 0; i < common; i++) sl_toss(e, d, 30, 0, sx, sy);
                else if (pr == 2) for (int i = 0; i < common; i++) sl_toss(e, d, 10, 0, sx, sy);
                else if (pr == 3) for (int i = 0; i < common; i++) sl_toss(e, d, 40, 0, sx, sy);
                else if (pr == 4) sl_toss(e, d, 752, 0, sx, sy);
                else if (pr == 5 || pr == 8) sl_toss(e, d, 752, 1, sx, sy);
                else if (pr == 6) sl_toss(e, d, 752, 2, sx, sy);
            } else {
                en_sound(SOUND_BOSS2INTRO_ERRORBUZZ, 1.0f, 1.0f);
                if (en_find(666, 60)) { unsigned long k = en_spawn(666, 60, sx, sy, 0, 0, e, 0); if (k) en_set_collision(k, ENTCOLL_PLAYEROBJECTS); }
                else SL_NOTDONE("Cell Game: the losing shell's Skuzz (666.60) is not ported");
            }
        } else en_play(s, a);
    } else if (s->state == 4) {
        if (en_finished(s, "Teleport")) { en_remove(e); return; }
        if (en_event(s, "Teleport", "Disappear")) {
            en_set_collision(e, ENTCOLL_NONE); en_spawn(5, 100, p[0], p[1] + 40.0f, 0, 0, e, 17); en_sound(SOUND_SLOTSPAWN, 1.0f, 1.0f);
        } else en_play(s, "Teleport");
    }
    if (s->state == 0 || s->state == 2) {
        unsigned long pl = sl_toucher(e, 3.0f, 0.75f);
        if (!pl) return;
        if (s->state == 0) {
            if (pl_keys(pl) > 0) {
                pl_add_keys(pl, -1); s->state = 1; en_sound(SOUND_KEY_DROP0, 1.0f, 1.0f);
                int prize = sd_roll(d, 1, 100), thresh = 60 - d->a[1] * 3; if (thresh < 40) thresh = 40;
                if (prize < thresh) s->i[0] = sd_rand(d, 4);
                else if (prize > 97) s->i[0] = 7;
                else {
                    int blue = sl_count(183, 1013), green = sl_count(183, 1014), red = sl_count(183, 1015), tot = blue + green + red;
                    if (tot > 0) { int k = sd_rand(d, tot) + 1; s->i[0] = k <= blue ? 4 : k <= blue + green ? 5 : 6; }
                    else s->i[0] = prize > 95 ? 7 : sd_rand(d, 4);
                }
            }
        } else {
            d->a[1]++;
            s->i[1] = fabsf(fpos(pl)[0] - p[0]) < 25.0f ? 2 : fpos(pl)[0] > p[0] ? 3 : 1;
            int chance = sd_rand(d, 4), lucky = g_has_item && g_has_item(pl, 202, 0);
            s->i[2] = lucky ? chance > 0 : chance < 2;
            s->state = (s->i[2] && s->i[0] == 7) ? 4 : 3;
        }
    }
}

/* an event of the OVERLAY animation (the Zodiac Beggar's prize comes from the overlay) */
static int sl_ov_event(const en_t *s, const char *name, const char *ev) {
    const ffa_t *a = ffa_find(s->key, name); if (!a || strcmp(s->ov, name)) return 0;
    for (int i = 0; i < a->evn; i++) if (!strcmp(ffe_tab[a->ev0 + i].name, ev) && ffe_tab[a->ev0 + i].frame == s->ovf) return 1;
    return 0;
}
#define PL_TYPE_LOST 10
#define PL_TYPE_KEEPER 14
#define PL_TYPE_LOST_B 31
#define PL_TYPE_KEEPER_B 33
#define OFF_PL_TYPE 0x1738
#define OFF_PL_MAXH 0x16b8
#define OFF_PL_SOUL 0x16c4
#define OFF_PL_TRINKETS 0x1ab0
static void (*g_add_maxhearts)(unsigned long, int, int);
static void (*g_add_soulhearts)(unsigned long, int);
static int  pl_type(unsigned long pl) { return *(const int *) (pl + OFF_PL_TYPE); }
static int  pl_maxh(unsigned long pl) { return *(const int *) (pl + OFF_PL_MAXH); }
static int  pl_soulh(unsigned long pl) { return *(const int *) (pl + OFF_PL_SOUL); }
static void pl_add_maxh(unsigned long pl, int h) { if (g_add_maxhearts) g_add_maxhearts(pl, h, 0); else *(int *) (pl + OFF_PL_MAXH) += h; }
static void pl_add_soul(unsigned long pl, int h) { if (g_add_soulhearts) g_add_soulhearts(pl, h); else *(int *) (pl + OFF_PL_SOUL) += h; }

/* ============================================================================================ Zodiac (Horoscope) Beggar 6.1031 (extraSlots.lua)
 * A coin per try: a prize roll (50%, or the 3rd try running); after 100 coins' worth ("coinsReceived", with the luck of 1/30, 1/6, 1/3 as it grows) it pays an item and leaves.
 * The main sprite loops Halo Idle; the overlay does the work (Idle, PayPrize, PayNothing, Prize, Teleport). d->a[0] coins, a[1] pity, a[2] gone. */
#define SLV_ZODIAC 1031
static void sl_zodiac_pay(unsigned long e, sd_t *d, float *p) {                 /* payoutZodiacBeggar */
    int r = sd_rand(d, 25);
    if (r < 3) sl_toss(e, d, 10, 3, p[0], p[1]);                                  /* a soul heart */
    else if (r < 5) { sl_toss(e, d, 350, 0, p[0], p[1]); SL_NOTDONE("Zodiac Beggar: its trinkets (Telescope Lens, Mom's Pearl, Shooting Star, Fushigi, Cancer) are a random trinket"); }
    else if (r < 7) sl_toss(e, d, 300, 0, p[0], p[1]);                            /* a rune: 5.301 is a random card here */
    else if (r < 12) SL_NOTDONE("Zodiac Beggar: 'The stars align' (Planetarium chance +5%) is not applied");
    else SL_NOTDONE("Zodiac Beggar: the wisp (AddWisp / AddItemWisp) is not spawned");
}
static void sl_zodiac(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) { s->init = 1; s->age = 0; en_replay(s, "Idle"); if (d->a[2]) { en_remove(e); return; } }
    if (s->i[7] == 0 && sl_exploded(e)) {                                         /* DropFunc: a rune shard, wisps (here the effect only) */
        s->i[7] = 1; sl_toss(e, d, 300, 55, p[0], p[1]); en_sound(SOUND_DEMON_HIT, 1.0f, 1.5f); en_spawn(1000, 15, p[0], p[1], 0, 0, e, 0);
        en_remove(e); return;
    }
    if (en_playing(s, "Idle")) { en_overlay(s, "Idle"); en_replay(s, "Halo Idle"); s->aloop = 1; }
    else if (en_overlay_finished(s, "PayPrize")) sl_overlay_restart(s, "Prize");
    else if (en_overlay_finished(s, "PayNothing")) sl_overlay_restart(s, "Idle");
    else if ((!strcmp(s->ov, "PayPrize") || !strcmp(s->ov, "PayNothing")) && s->ovf == 13) en_sound(SOUND_SOUL_PICKUP, 1.0f, sd_range(d, 7, 10) / 10.0f);
    else if (en_overlay_finished(s, "Teleport")) { en_remove(e); return; }
    else if (!strcmp(s->ov, "Prize") && sl_overlay_playing(s, "Prize")) {
        if (sl_ov_event(s, "Prize", "Prize") || s->ovf == 27) {
            if (d->a[0] >= 100) {
                en_spawn(5, 100, p[0], p[1] + 40.0f, 0, 0, e, 0);               /* a Planetarium item 1 in 5, otherwise the mod's Zodiac pool: an item of the room's pool */
                en_sound(SOUND_THUMBSUP, 1.0f, 1.0f); d->a[2] = 1; SL_NOTDONE("Zodiac Beggar: its pedestal is the room's pool item (no Planetarium / Zodiac pool)");
            } else { sl_zodiac_pay(e, d, p); d->a[1] = 0; en_sound(SOUND_SLOTSPAWN, 1.0f, 1.0f); }
        }
    } else if (en_overlay_finished(s, "Prize")) {
        if (d->a[0] >= 100) { sl_overlay_restart(s, "Teleport"); en_replay(s, "Halo Teleport"); } else sl_overlay_restart(s, "Idle");
    }
    if (!strcmp(s->ov, "Idle")) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl && pl_coins(pl) >= 1) {
            pl_add_coins(pl, -1); d->a[0]++; d->a[1]++; en_sound(SOUND_SCAMPER, 1.0f, 1.0f);
            if (sd_rand(d, 2) == 0 || d->a[1] >= 3) {
                if (d->a[0] >= 50) { if (sd_rand(d, 3) == 0) d->a[0] = 100; }
                else if (d->a[0] >= 20) { if (sd_rand(d, 6) == 0) d->a[0] = 100; }
                else if (d->a[0] >= 3) { if (sd_rand(d, 30) == 0) d->a[0] = 100; }
                sl_overlay_restart(s, "PayPrize");
            } else if (d->a[0] == 100 || d->a[1] == 6) sl_overlay_restart(s, "PayPrize");
            else sl_overlay_restart(s, "PayNothing");
        }
    }
}

/* ============================================================================================ Evil (Beast) Beggar 6.1033 (extraSlots.lua)
 * It takes a heart container (2), 4 soul halves, a Keeper's 15 coins; then a 40 + chanceBonus % (-16, +16 for each payment) chance of a Devil-pool item and it leaves.
 * state: 0 Idle, 1 paid (waiting for the anim), 2 Prize, 3 Teleport, 4 Bombed. d->a[0] payout roll, a[1] chanceBonus+16 stored as chanceBonus, a[2] leaveOnInit, a[3] has chanceBonus */
#define SLV_EVIL 1033
static void sl_evil(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) {
        s->init = 1; s->age = 0; s->state = 0; en_replay(s, "Idle");
        if (d->a[4] && d->a[3] && !d->a[2] && (d->a[0] <= 40 + d->a[1] || d->a[1] == 32)) {      /* it was paid and left before the item came: the item comes now */
            en_spawn(5, 100, p[0], p[1] + 40.0f, 0, 0, e, 0); en_remove(e); return;
        }
    }
    if (s->state != 4 && sl_exploded(e)) { s->state = 4; en_replay(s, "Bombed"); fvel(e)[0] *= 0.3f; fvel(e)[1] *= 0.3f; }
    if (s->state == 4) {
        if (en_finished(s, "Bombed")) { en_remove(e); return; }
        if (en_event(s, "Bombed", "Disappear")) { float vx, vy; vrot(0, 4.0f, (float) sd_range(d, 30, 150) - 90.0f, &vx, &vy); en_spawn(4, 3, p[0], p[1], vx, vy, e, 0); }
        return;
    }
    if (s->state == 0) { if (d->a[2]) { en_remove(e); return; } en_play(s, "Idle"); }
    else if (s->state == 1) {
        if (en_finished(s, "PayNothing")) { s->state = 0; en_replay(s, "Idle"); }
        else if (en_finished(s, "PayRedHeart") || en_finished(s, "PaySoulHeart") || en_finished(s, "PayCoin") || en_finished(s, "PaySoul")) {
            d->a[0] = sd_roll(d, 1, 100); d->a[4] = 1;
            if (d->a[0] <= 40 + d->a[1] || d->a[1] == 32) { s->state = 2; en_replay(s, "Prize"); } else { s->state = 0; en_replay(s, "Idle"); }
        }
    } else if (s->state == 2) {
        if (en_finished(s, "Prize")) { s->state = 3; en_replay(s, "Teleport"); }
        else if (en_event(s, "Prize", "Prize")) {
            en_spawn(5, 100, p[0], p[1] + 40.0f, 0, 0, e, 0); en_sound(SOUND_SLOTSPAWN, 1.0f, 1.0f); en_sound(SOUND_THUMBSUP, 1.0f, 1.0f); d->a[2] = 1;
            SL_NOTDONE("Evil Beggar: its item is the room's pool item (not the Devil pool); Thank You Fossil extra items");
        }
    } else if (s->state == 3) { if (en_finished(s, "Teleport")) { en_remove(e); return; } en_play(s, "Teleport"); }
    if (s->state == 0) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f); int t = pl ? pl_type(pl) : 0;
        if (pl && t != PL_TYPE_LOST && t != PL_TYPE_LOST_B) {
            if (!d->a[3]) { d->a[3] = 1; d->a[1] = -16; }
            int paid = 0, keeper = t == PL_TYPE_KEEPER || t == PL_TYPE_KEEPER_B;
            if (keeper) { if (pl_coins(pl) >= 15) { pl_add_coins(pl, -15); en_replay(s, "PayCoin"); paid = 1; } }
            else if (pl_maxh(pl) != 0) { pl_add_maxh(pl, -2); en_replay(s, "PayRedHeart"); paid = 2; }
            else if (pl_soulh(pl) >= 4) { pl_add_soul(pl, -4); en_replay(s, "PaySoulHeart"); paid = 2; }
            if (paid) {
                en_sound(SOUND_SCAMPER, 1.0f, 1.0f); d->a[1] += 16; s->state = 1;
                if (paid == 2) SL_NOTDONE("Evil Beggar: its Mega Blast-like active (UseActiveItem 486), Your Soul / bone hearts payment, and the player dying with no health left");
            }
        }
    }
}

/* ============================================================================================ Robot Teller 6.1032 (extraSlots.lua)
 * A coin starts Initiate -> Wiggle -> a roll: 60% a card (with a 1 in 5 Malfunction that uses it on you), 1 in 27 it blows up into a pedestal, else a plain prize (curse clear /
 * soul heart, maps, portal, Planetarium). The Lua forces prizeQueue = "Portal" every update, so every Prize event spawns a portal (1000.161); the same here.
 * state: 0 Idle, 1 IdleCard, 2 Initiate, 3 Wiggle, 4 prize anim (i[0] its name index), 5 Death. i[1] the card of the last prize (0 none), i[2] reverse */
#define SLV_ROBOT 1032
static int g_robot_map;                                                         /* savedata.robotMapLevel: 0 none, 1 map shown, 2 compass shown, 3 see forever */
static const char *const ROBOT_PRIZES[] = { "PrizeDefault", "Malfunction", "PrizeSecret", "PrizeCompass", "PrizeMap", "PrizeSuperSecret", "PrizePlanetarium", "PrizeCard" };
static void sl_robot_drop(unsigned long e, en_t *s, sd_t *d, float *p) {         /* DropFunc */
    float vx, vy;
    if (s->i[1]) { vrot(0, 4.0f, (float) sd_range(d, -29, 30), &vx, &vy); en_spawn(5, 300, p[0], p[1], vx, vy, e, s->i[1]); }
    int chance = sd_range(d, 1, 5);
    for (int k = 0; k < (chance <= 3 ? 1 : 0) + (chance == 3 ? 1 : 0); k++) {
        int card = sd_range(d, 1, 22); vrot((float) sd_range(d, 4, 7), 0, (float) sd_range(d, 1, 180), &vx, &vy); en_spawn(5, 300, p[0], p[1], vx, vy, e, card);
    }
    for (int k = 0; k < chance; k++) { vrot((float) sd_range(d, 4, 7), 0, (float) sd_range(d, 1, 180), &vx, &vy); en_spawn(5, 20, p[0], p[1], vx, vy, e, 1); }
}
static void sl_robot(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) { s->init = 1; s->age = 0; s->state = 0; en_replay(s, "Idle"); }
    if (s->state != 5 && sl_exploded(e)) { s->state = 5; en_replay(s, "Death"); sl_robot_drop(e, s, d, p); }
    switch (s->state) {
    case 0: en_play(s, "Idle"); break;
    case 1: en_play(s, "IdleCard"); break;
    case 2:
        if (en_finished(s, "Initiate") || en_finished(s, "InitiateCard")) { s->state = 3; en_replay(s, "Wiggle"); }
        else en_play(s, s->i[3] ? "InitiateCard" : "Initiate");
        break;
    case 3:
        if (en_finished(s, "Wiggle")) {
            int payout = sd_range(d, 1, 100), blow = sd_range(d, 1, 27);
            if (payout <= 60) {                                                   /* a card */
                payout = sd_range(d, 1, 5); int rev = sd_range(d, 1, 15); int idx = sd_range(d, 1, 22);
                s->i[1] = rev == 1 ? idx + 56 : idx;
                s->state = 4; s->i[0] = (payout == 1 && rev != 1) ? 1 : 7; en_replay(s, ROBOT_PRIZES[s->i[0]]);
                if (s->i[0] == 1) SL_NOTDONE("Robot Teller: the Malfunction's UseCard on the player is not applied");
            } else if (blow == 1) {
                en_spawn(1000, 1, p[0], p[1], 0, 0, e, 0); en_spawn(5, 100, p[0], p[1] - 4.0f, 0, 0, e, 0); en_remove(e); return;
                /* the mod's Robo Teller pool: the room's pool item */
            } else {
                payout = sd_range(d, 1, 10);
                if (payout <= 2) s->i[0] = 0;
                else if (payout <= 8) { s->i[0] = g_robot_map == 0 ? 4 : g_robot_map == 1 ? 3 : 0; if (g_robot_map < 3) g_robot_map++; SL_NOTDONE("Robot Teller: the map / compass / Secret Room reveal and See Forever are not applied"); }
                else if (payout == 10) s->i[0] = 6;
                else s->i[0] = 0;
                s->state = 4; en_replay(s, ROBOT_PRIZES[s->i[0]]);
            }
        } else en_play(s, "Wiggle");
        break;
    case 4: {
        const char *a = ROBOT_PRIZES[s->i[0]];
        if (en_finished(s, a)) { if (s->i[0] == 7) { s->state = 1; en_replay(s, "IdleCard"); } else { s->state = s->i[1] ? 1 : 0; en_replay(s, s->i[1] ? "IdleCard" : "Idle"); } }
        else {
            en_play(s, a);
            if (en_event(s, a, "Prize")) {
                en_sound(SOUND_SUMMONSOUND, 1.0f, 1.0f);
                int stage = en_game() ? *(const int *) en_game() : 1, f = stage > 6 ? sd_range(d, 1, 2) + 1 : sd_range(d, 1, 3);
                unsigned long snap[SNAP]; unsigned n = snapshot(snap);
                for (unsigned k = 0; k < n; k++) if (alive(snap[k]) && *(const int *) (snap[k] + E_TYPE) == 1000 && *(const int *) (snap[k] + E_VARIANT) == 161) en_remove(snap[k]);
                en_spawn(1000, 161, p[0] + 80.0f, p[1] + 40.0f, 0, 0, e, f - 1);
            }
            if (en_event(s, a, "Card")) en_sound(SOUND_SHELLGAME, 1.0f, 1.0f);
            if (en_event(s, a, "Glitching") || en_event(s, a, "CardZoom")) en_sound(SOUND_REDLIGHTNING_ZAP, 1.0f, 1.0f);
            if (en_event(s, a, "Glitching2")) en_sound(SOUND_EDEN_GLITCH, 1.0f, 1.0f);
            if (en_event(s, a, "Activate")) { en_sound(SOUND_1UP, 1.0f, 1.0f); SL_NOTDONE("Robot Teller: UseCard on the player (the card shown) is not applied"); }
        }
        break; }
    case 5: if (!en_finished(s, "Death")) en_play(s, "Death"); else en_replay(s, "Broken"); break;
    }
    if (s->state == 0 || s->state == 1) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl && pl_coins(pl) >= 1) {
            pl_add_coins(pl, -1); en_sound(SOUND_COIN_SLOT, 0.8f, 1.0f); s->i[3] = s->state == 1; s->state = 2; en_replay(s, s->i[3] ? "InitiateCard" : "Initiate");
        }
    }
}

/* ============================================================================================ Grid Restock Machine 6.880 (main.lua "Grid Restock")
 * A bomb: BombInsert, then 15 + 25 * bombs % Wiggle -> Prize (the room's rocks / poop are rerolled), else Idle. After 4 rerolls it breaks 50% of the time. The reroll of the
 * grids (mod.reroll_grids) is not done. state: 1 Idle, 2 inserts, 3 insert anim, 4 Prize, 5 dead, 6 killed. d->a[0] rerolls left (+1), a[1] bombs stored */
#define SLV_RESTOCK 880
static void sl_restock(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) { s->init = 1; s->age = 0; s->state = 1; en_replay(s, "Idle"); if (!d->a[7]) { d->a[7] = 1; d->a[0] = 5; d->a[1] = 0; } SL_NOTDONE("Grid Restock: the rocks / poop reroll is not done"); }
    if (s->i[0] > 0) s->i[0]--;                                                                  /* Countdown */
    if (s->state != 5 && s->state != 6 && sl_exploded(e)) { s->state = 6; en_replay(s, "Broken"); }
    switch (s->state) {
    case 2: d->a[1]++; en_replay(s, "BombInsert"); s->state = 3; break;
    case 3:
        if (en_finished(s, "BombInsert")) {
            if (sd_roll(d, 1, 100) <= 15 + 25 * d->a[1]) { en_replay(s, "Wiggle"); s->state = 7; }
            else { s->state = 1; en_replay(s, "Idle"); s->i[0] = 10; }
        } else { en_play(s, "BombInsert"); if (en_event(s, "BombInsert", "BombPlace")) en_sound(201, 1.0f, 0.8f); if (en_event(s, "BombInsert", "BombInsert")) en_sound(24, 1.0f, 0.9f); }
        break;
    case 7: if (en_finished(s, "Wiggle")) { s->state = 4; en_replay(s, "Prize"); } else en_play(s, "Wiggle"); break;
    case 4:
        if (en_finished(s, "Prize")) { s->state = 1; en_replay(s, "Idle"); }
        else {
            en_play(s, "Prize");
            if (en_event(s, "Prize", "Prize")) {
                if (d->a[0] != 1 || sd_rand(d, 2) == 0) { en_sound(268, 1.0f, 0.9f); if (d->a[0] > 1) d->a[0]--; d->a[1] = 0; }
                else {
                    s->state = 5; en_spawn(1000, 1, p[0], p[1], 0, 0, e, 0);
                    if (sd_rand(d, 5) == 0) { en_spawn(5, 100, p[0], p[1], 0, 0, e, 386); en_remove(e); return; }
                    en_replay(s, "Broken"); d->a[0] = 0; d->a[7] = 0;
                }
            }
        }
        break;
    default: if (s->state == 1) en_play(s, "Idle"); else if (s->state == 5 || s->state == 6) en_play(s, "Broken"); break;
    }
    if (s->state == 1 && s->i[0] == 0) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl && pl_bombs(pl) > 0) { pl_add_bombs(pl, -1); s->state = 2; }
    }
}

/* ============================================================================================ Blacksmith 6.1030 (extraSlots.lua)
 * Hand it your most recent trinket (two of them): at the second one it hammers (PayPrize) and gives a trinket; after 3 trades it leaves a pedestal and goes. state: 0 Idle, 1 paying, 2 Prize, 3 Teleport, 4 Destroyed
 * d->a[0] trinketInSack, a[1] received, a[2] trades */
#define SLV_SMITH 1030
static int pl_take_trinket(unsigned long pl) {                                     /* FiendFolio.GetMostRecentTrinket + TryRemoveTrinket: the last held slot */
    int *t = (int *) (pl + OFF_PL_TRINKETS), id = 0;
    for (int i = 1; i >= 0; i--) if (t[i]) { id = t[i]; t[i] = 0; break; }
    return id;
}
static void sl_smith(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) { s->init = 1; s->age = 0; s->state = 0; en_replay(s, "Idle"); }
    if (s->state != 4 && sl_exploded(e)) {                                         /* DropFunc: Destroyed, blood, 1-2 rock spiders (818), the trinket in the sack */
        s->state = 4; en_replay(s, "Destroyed"); en_spawn(1000, 2, p[0], p[1], 0, 0, e, 0);
        for (int k = sd_range(d, 1, 2); k > 0; k--) { float vx, vy; en_rand_vec(&vx, &vy, (float) sd_range(d, 2, 3)); en_spawn(818, 0, p[0], p[1], vx, vy, e, 0); }
        if (d->a[0]) { float vx, vy; vrot(0, 4.0f, (float) sd_range(d, 30, 150) - 90.0f, &vx, &vy); en_spawn(5, 350, p[0], p[1], vx, vy, e, d->a[0]); d->a[0] = 0; }
    }
    switch (s->state) {
    case 0: en_play(s, "Idle"); break;
    case 1:
        if (en_finished(s, "PayPrize") || en_finished(s, "PayNothing")) { if (d->a[1] == 2) { s->state = 2; en_replay(s, "Prize"); } else { s->state = 0; en_replay(s, "Idle"); } }
        else en_play(s, s->i[0] ? "PayPrize" : "PayNothing");
        break;
    case 2:
        if (en_finished(s, "Prize")) {
            if (d->a[2] == 3) { s->state = 3; en_replay(s, "Teleport"); en_spawn(5, 100, p[0], p[1] + 60.0f, 0, 0, e, 0); }
            else { s->state = 0; en_replay(s, "Idle"); }
        } else {
            en_play(s, "Prize");
            if (en_event(s, "Prize", "Prize")) { float vx, vy; vrot(0, 4.0f, (float) sd_range(d, 30, 150) - 90.0f, &vx, &vy); en_spawn(5, 350, p[0], p[1], vx, vy, e, 0); d->a[0] = 0; d->a[1] = 0; en_sound(SOUND_SLOTSPAWN, 1.0f, 1.0f); }
            if (en_event(s, "Prize", "Sound")) en_sound(SOUND_PUNCH, 1.3f, 1.0f);
            if (en_event(s, "Prize", "Donk")) en_sound(SOUND_PUNCH, 0.6f, 1.2f);
        }
        break;
    case 3: if (en_finished(s, "Teleport")) { en_remove(e); return; } en_play(s, "Teleport"); break;
    case 4: en_play(s, "Destroyed"); break;
    }
    if (s->state == 0) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl) {
            int t = pl_take_trinket(pl);
            if (t) {
                d->a[0] = t; d->a[1]++; en_sound(SOUND_SCAMPER, 1.0f, 1.0f); s->state = 1; s->i[0] = d->a[1] == 2;
                if (d->a[1] == 2) d->a[2]++;
                SL_NOTDONE("Blacksmith: the mod's Blacksmith pool pedestal / the trinket it hands back are a random vanilla one");
            }
        }
    }
}

/* ============================================================================================ Massive Amethyst Cluster 6.1050 / Floating 6.1051 (xalum/items/massive_amethyst.lua)
 * A decoration that stands where a shopkeeper would (secret shops): a random sprite ("Shopkeeper 1..9" / "Guy1..9"); a bomb breaks it into 1+ Rune Shards, 1-2 runes and gibs.
 * (The trinket that turns shopkeepers into clusters is not done: the rooms place them directly.) */
#define SLV_AMETH 1050
#define SLV_AMETHF 1051
static void sl_amethyst(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e); char nm[24];
    if (!s->init) {
        s->init = 1; s->age = 0; snprintf(nm, sizeof nm, *(const int *) (e + E_VARIANT) == SLV_AMETH ? "Shopkeeper %d" : "Guy%d", 1 + sd_rand(d, 9)); en_replay(s, nm);
    }
    if (sl_exploded(e)) {
        int n = sd_rand(d, 3);                                                 /* GetTotalTrinketMultiplier (0 without the trinket) + rng:RandomInt(3), `for i = 1, n` */
        for (int k = 0; k < n; k++) { float vx, vy; en_rand_vec(&vx, &vy, 1.0f); en_spawn(5, 300, p[0], p[1], vx, vy, e, 55); }
        for (int k = 0; k <= sd_rand(d, 2); k++) { float vx, vy; en_rand_vec(&vx, &vy, 1.0f); en_spawn(5, 300, p[0], p[1], vx, vy, e, 0); }
        en_sound(SOUND_BOSS1_EXPLOSIONS, 0.6f, 1.6f);
        for (int k = sd_range(d, 4, 9); k > 0; k--) { float vx, vy; en_rand_vec(&vx, &vy, 3.0f); en_spawn(1000, 35, p[0], p[1], vx, vy, e, 0); }
        SL_NOTDONE("Amethyst Cluster: its gibs use the blood gib effect (the amethyst gib sheet is not set), no break sound of its own");
        en_remove(e); return;
    }
}

/* ============================================================================================ Poker Table 6.160 + its three wagers (erfly/slots/pokertable.lua)
 * Stand on the Coin / Key / Bomb spot (still) to stack a wager, touch the table to play: you win double (and it pays once per win, leaving after its 3rd loss) or lose the wager.
 * The wagers are three effects 1000.1960.40 (anm2 = the table's own): s->ref/ref2/ref3 of the table hold them, a wager holds its parent in ref.
 * Table (the persistent floor memory d): a[0] losses, a[1] totalstakes, a[2] coins staked, a[3] keys, a[4] bombs, a[5] 1 = it was beaten and left, a[7] state (pokerTableStates).
 * Table state s->state: 0 idle, 1 wagering, 2 playhand, 3 LOSER, 4 shityouwon, 5 leaving; s->i[0] smug, i[1] fearing life.
 * Wager (s->i): [0] wagering counter (-2 = nil), [1] wagered, [2] wagerCount, [3] choiceBet (-1 Coin, 0 Bomb, 1 Key) */
#define SLV_POKER 160
#define EFV_WAGER 1960
enum { PT_IDLE, PT_SMUG, PT_PLAYING, PT_PAYOUT, PT_LEAVING, PT_DONE };
static unsigned long pt_wager(en_t *t, int i) { return i == 0 ? t->ref : i == 1 ? t->ref2 : t->ref3; }
static int pt_cap(int visual) { int c = visual == 0 ? 20 : 10; return c > 99 ? 99 : c; }                 /* WagerCaps; the Pak Choi / Jevil's Tail bonuses are not applied */
static void pt_reset_wagers(en_t *t) {
    for (int i = 0; i < 3; i++) { unsigned long w = pt_wager(t, i); if (w && alive(w)) { en_t *ws = en_slot(w); ws->i[1] = 0; ws->i[2] = 0; ws->i[0] = -2; } }
}
static void pt_payout(sd_t *d) {                                                                              /* mod:pokerTablePayout */
    unsigned long pl = sl_player(0); if (!pl) return;
    if (d->a[2]) { pl_add_coins(pl, d->a[2] * 2); d->a[2] = 0; }
    if (d->a[3]) { pl_add_keys(pl, d->a[3] * 2); d->a[3] = 0; }
    if (d->a[4]) { pl_add_bombs(pl, d->a[4] * 2); d->a[4] = 0; }
}
static void pt_gift(unsigned long e, sd_t *d, float *p) {                                                    /* runningAwayGift */
    int st = d->a[1], c;
    #define PTP(var, sub, vx, vy) en_spawn(5, var, p[0], p[1], vx, vy, e, sub)
    float vx, vy;
    if (st >= 90) {
        c = sd_rand(d, 6);
        if (c <= 2) for (int k = sd_rand(d, 3) + 2; k > 0; k--) { en_rand_vec(&vx, &vy, (float) sd_range(d, 30, 40) / 10.0f); PTP(300, 0, vx, vy); }
        else if (c <= 3) PTP(0, 0, 0, 0);                                                                  /* 5.666.11 (a Fiend Folio sack): a random pickup */
        else if (c <= 4) { sd_rand(d, 8); PTP(350, 0, 0, 0); }
        else if (c <= 5) { if (sd_rand(d, 2) == 0) PTP(100, 286, 0, 0); else PTP(100, 0, 0, 0); }
    } else if (st >= 50) {
        c = sd_rand(d, 5);
        if (c <= 2) for (int k = sd_rand(d, 3) + 1; k > 0; k--) { en_rand_vec(&vx, &vy, (float) sd_range(d, 30, 40) / 10.0f); PTP(300, 0, vx, vy); }
        else if (c <= 3) PTP(0, 0, 0, 0);
        else if (c <= 4) { sd_rand(d, 8); PTP(350, 0, 0, 0); }
    } else if (st >= 10) {
        c = sd_rand(d, 5);
        if (c <= 2) for (int k = sd_rand(d, 2) + 1; k > 0; k--) { en_rand_vec(&vx, &vy, (float) sd_range(d, 30, 40) / 10.0f); PTP(20 + sd_rand(d, 3) * 10, 0, vx, vy); }
        else if (c <= 3) PTP(300, 0, 0, 0);
        else PTP(300, 44, 0, 0);                                                                           /* Card.CARD_RULES */
    } else PTP(20 + sd_rand(d, 3) * 10, 0, 0, 0);
    #undef PTP
    SL_NOTDONE("Poker Table: the 52 Deck cards, Ace of Spades / Poker Chip / Jokers (its trinket pool) and the Clear Case are a random card / trinket / pedestal, its sounds are not played, the Golden Watch part is not done");
}
static void pt_decide(en_t *t, sd_t *d, int reenter) {                                                         /* determinPokerTableWinner */
    float his = 1.0f + (reenter ? 2.0f : 0.0f) + (float) d->a[0], yours = 1.0f;
    unsigned long pl = sl_player(0);
    if (pl && g_has_item) {
        if (g_has_item(pl, 202, 0)) yours += 1.0f;                                                         /* Lucky Foot */
        if (g_has_item(pl, 76, 0) || g_has_item(pl, 212, 0)) yours += 2.0f;                                 /* X-Ray Vision / Guppy's Eye */
    }
    float roll = (float) (sd_next(d) & 0xffffff) / 16777216.0f * (yours + his);
    if (roll > yours) { t->state = 3; d->a[7] = PT_SMUG; d->a[2] = d->a[3] = d->a[4] = 0; }
    else {
        d->a[0]++;
        if (d->a[0] >= 3) { t->state = 5; d->a[7] = PT_LEAVING; }
        else { t->state = 4; d->a[7] = PT_PAYOUT; }
    }
}
static void sl_poker(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) {
        s->init = 1; s->age = 0;
        for (int i = -1; i <= 1; i++) {
            unsigned long w = en_spawn(1000, EFV_WAGER, p[0] + (float) (i * 32), p[1] + 46.0f, 0, 0, e, 40);
            if (w) { en_t *ws = en_slot(w); ws->ref = e; ws->i[0] = -2; ws->i[3] = i; if (i == -1) s->ref = w; else if (i == 0) s->ref2 = w; else s->ref3 = w; }
        }
        if (d->a[7] == PT_LEAVING) s->state = 5;
        else if (d->a[7] == PT_DONE) { en_remove(e); return; }
        else if (d->a[7] == PT_PLAYING) pt_decide(s, d, 1);
        else if (d->a[7] == PT_PAYOUT) s->state = 4;
        else { s->i[0] = d->a[7] == PT_SMUG; s->state = 0; }
        en_play(s, "Idle");
    }
    const char *sm = s->i[0] ? "Smug" : "";
    char nm[24];
    if (s->state == 0) {
        unsigned long snap[SNAP]; unsigned n = snapshot(snap); int bomb = 0;
        for (unsigned k = 0; k < n; k++) if (alive(snap[k]) && *(const int *) (snap[k] + E_TYPE) == 4 && vlen(fpos(snap[k])[0] - p[0], fpos(snap[k])[1] - p[1]) < 85.0f) bomb = 1;
        if (bomb && !s->i[0]) { s->i[1] = 1; s->i[0] = 1; sm = "Smug"; }
        if (s->i[1]) { if (en_finished(s, "ItsOverBegin")) { s->i[1] = 0; s->i[0] = 1; } else en_play(s, "ItsOverBegin"); }
        else { snprintf(nm, sizeof nm, "Idle%s", s->i[0] ? "Smug" : ""); en_play(s, nm); }
        for (int i = 0; i < 3; i++) { unsigned long w = pt_wager(s, i); if (w && alive(w) && (en_slot(w)->i[0] != -2 || en_slot(w)->i[1])) { s->state = 1; snprintf(nm, sizeof nm, "WageringBegin%s", s->i[0] ? "Smug" : ""); en_replay(s, nm); } }
    } else if (s->state == 1) {
        snprintf(nm, sizeof nm, "WageringBegin%s", sm);
        if (!(!strcmp(s->anim, nm) && !en_finished(s, nm))) en_play(s, "WageringLoop");
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);                                                       /* onMachineTouch(160): take the stakes and play */
        if (pl) {
            for (int i = 0; i < 3; i++) {
                unsigned long w = pt_wager(s, i); if (!w || !alive(w)) continue;
                en_t *ws = en_slot(w);
                if (ws->i[0] == -2 && !ws->i[1]) continue;
                int vis = ws->i[3], have = vis == -1 ? pl_coins(pl) : vis == 1 ? pl_keys(pl) : pl_bombs(pl);
                if (have >= 1) {
                    int rem = have < ws->i[2] ? have : ws->i[2];
                    if (vis == -1) { pl_add_coins(pl, -rem); d->a[2] = rem; } else if (vis == 1) { pl_add_keys(pl, -rem); d->a[3] = rem; } else { pl_add_bombs(pl, -rem); d->a[4] = rem; }
                    d->a[1] += rem;
                } else { ws->i[0] = -2; ws->i[1] = 0; ws->i[2] = 0; }
            }
            s->state = 2; s->i[0] = 0; en_replay(s, "Play"); d->a[7] = PT_PLAYING;
        }
    } else if (s->state == 2) {
        if (en_finished(s, "Play")) pt_decide(s, d, 0); else en_play(s, "Play");
    } else if (s->state == 3) {
        if (en_finished(s, "YouLose")) { s->state = 0; s->i[0] = 1; pt_reset_wagers(s); en_replay(s, "IdleSmug"); }
        else en_play(s, "YouLose");
    } else if (s->state == 4) {
        if (en_finished(s, "YouWin")) { s->state = 0; pt_reset_wagers(s); en_replay(s, "Idle"); }
        else {
            en_play(s, "YouWin");
            if (en_event(s, "YouWin", "Gasp")) { pt_payout(d); d->a[7] = PT_IDLE; }
        }
    } else if (s->state == 5) {
        if (en_finished(s, "Defeat")) { for (int i = 0; i < 3; i++) { unsigned long w = pt_wager(s, i); if (w && alive(w)) en_remove(w); } en_remove(e); return; }
        en_play(s, "Defeat");
        if (en_event(s, "Defeat", "Gasp")) { pt_payout(d); d->a[7] = PT_DONE; }
        if (en_event(s, "Defeat", "Sound")) pt_gift(e, d, p);
    }
    if (s->state != 5 && sl_exploded(e)) {                                                                   /* DropFunc: blood, a coin / key / bomb */
        float vx, vy; en_rand_vec(&vx, &vy, (float) sd_range(d, 20, 30) / 10.0f);
        en_spawn(1000, 2, p[0], p[1] - 20.0f, 0, 0, e, 3); en_spawn(5, 20 + sd_rand(d, 3) * 10, p[0], p[1], vx, vy, e, 0);
        for (int i = 0; i < 3; i++) { unsigned long w = pt_wager(s, i); if (w && alive(w)) en_remove(w); }
        en_remove(e); return;
    }
}
static void sl_wager(en_t *s, unsigned long e) {                                                              /* mod:pokerWagerAI */
    unsigned long par = s->ref;
    if (!s->init) { s->init = 1; s->age = 0; s->i[0] = -2; if (!par) { en_remove(e); return; } }
    if (!par || !alive(par) || *(const int *) (par + E_TYPE) != 6) { en_remove(e); return; }
    en_t *t = en_slot(par);
    int vis = s->i[3] == -1 ? 0 : s->i[3] == 1 ? 1 : 2;                                                        /* 0 Coin, 1 Key, 2 Bomb */
    static const char *const NM[] = { "Coin", "Key", "Bomb" };
    unsigned long pl = sl_player(0); int tw = 0, aw = 0;
    float *p = fpos(e);
    if (pl && vlen(fpos(pl)[0] - p[0], fpos(pl)[1] - p[1]) < 15.0f && vlen(fvel(pl)[0], fvel(pl)[1]) < 1.0f) {
        int have = vis == 0 ? pl_coins(pl) : vis == 1 ? pl_keys(pl) : pl_bombs(pl);
        if (have >= 1 && (t->state == 0 || t->state == 1)) {
            tw = 1;
            if (s->i[2] <= pt_cap(vis) && have > s->i[2]) {
                if (s->i[0] == -2) s->i[0] = -1;
                s->i[0]++; if (s->i[0] == 0) s->i[2]++;
                aw = 1; s->i[1] = 0;
            }
        }
    } else { if (s->i[0] != -2) s->i[1] = 1; s->i[0] = -2; }
    if (aw && s->i[0] > 10) { int m = vis == 0 ? 3 : vis == 2 ? 4 : 5; if (s->i[0] % m == 0) s->i[2]++; }
    if (s->i[2] > pt_cap(vis)) s->i[2] = pt_cap(vis);
    if (s->i[2] > 0) { if (strcmp(s->ov, "Number")) en_overlay(s, "Number"); s->ovf = s->i[2]; }
    else if (s->ov[0]) en_overlay_off(s);
    char a[24]; snprintf(a, sizeof a, "Bet%s%s", NM[vis], (aw || (!s->i[1] && tw)) ? "Selecting" : s->i[1] ? "Wagered" : "Idle");
    en_play(s, a);
}

#include "ffen_g14.h"
/* ============================================================================================ the tick */
typedef void (*sl_fn)(en_t *, unsigned long);
static const struct { int var; sl_fn fn; } SL_TAB[] = {
    { SLV_PENNY, sl_penny }, { SLV_DICE, sl_dice }, { SLV_JUKEBOX, sl_jukebox }, { SLV_HUG, sl_hug }, { SLV_CELL, sl_cell },
    { SLV_POKER, sl_poker }, { SLV_AMETH, sl_amethyst }, { SLV_AMETHF, sl_amethyst }, { SLV_ZODIAC, sl_zodiac }, { SLV_EVIL, sl_evil }, { SLV_ROBOT, sl_robot }, { SLV_RESTOCK, sl_restock }, { SLV_SMITH, sl_smith }, { SLV_VEND, sl_vend },
};
static sl_fn sl_find(int var) { for (unsigned i = 0; i < sizeof SL_TAB / sizeof SL_TAB[0]; i++) if (SL_TAB[i].var == var) return SL_TAB[i].fn; return 0; }
static void en_g12_tick(void) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long e = snap[k]; if (!alive(e)) continue;
        sl_fn fn = 0; int ty = *(const int *) (e + E_TYPE);
        if (ty == 6) fn = sl_find(*(const int *) (e + E_VARIANT));
        else if (ty == 1000 && *(const int *) (e + E_VARIANT) == EFV_WAGER && *(const int *) (e + E_SUBTYPE) == 40) fn = sl_wager;
        if (!fn) continue;
        en_t *s = en_slot(e);
        if (s->init) sl_tick_frames(s);
        fn(s, e);
        en_state_watch(s);
        if (!alive(e) || s->e != e) continue;
        s->age++; sl_sprite(s, e);
    }
}
