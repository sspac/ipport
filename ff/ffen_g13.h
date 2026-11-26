/* ffen_g13.h - Fiend Folio's chests, type 5 variants 711..714 (erfly/pickups/shopchest.lua, direchest.lua, glasschest.lua, _chestshared.lua,
 * ferrium/items/pickups/enginechest.lua). Polled the way the machines of ffen_g12.h are: en_slot(e) keyed by pointer + subtype, the port's own frame counter drives
 * the sprite, a chest's contents live in the floor memory (sd_t + g_chd) so they are the same every time the room is entered.
 * The subtype is never written (it is the Lua's state number; here it is s->state) - rooms that carry subtype 1 / 2 (already opened) are read at init.
 *
 * NOT DONE (each one is a log line "ffslot: not done: ..." the first time it matters):
 *   - the item / trinket / achievement conditions: Daemon's Tail, Poker Chip, Mom's Key, Lucky Toe, Rusted Key, Child's Heart, Match Stick, Ace of Spades, Sack Head, Faulty Fuse,
 *     Humbleing Bundle, Pay To Play, Paper Clip, Magneto, Guppy's Eye, Broken Padlock, Flat File; the persistent-game-data unlocks (everything counts as unlocked);
 *   - the custom item pools (Glass Chest / Dire Chest / Engine Chest / Shop Chest): an item is the room's own pedestal (5.100.0), a trinket 5.350.0; the Fiend Folio cards / soul stones
 *     (5.300.9xx) are a random card (5.300.0);
 *   - the glass chest's preview of its contents (render), its Shatter effect, the Mama Mega / Siren shattering; the sharp key (a key tear) opening chests;
 *   - the Engine Chest takes its three charges from the player's active items / soul charge / Dad's Battery / storage batteries: the port cannot read those, so each touch is one pip;
 *   - the Shop Chest's room-type spritesheets and its secret-room items (5.0.3 / pedestal with prices): it always spawns shop items (5.150); the keeper it kills when blown up;
 *   - the Dire Chest's friendly Attack Skuzz, Blots (170.40 only if the port runs it), the Fortune and the Card of Humanity use on the player, the Samael anger bonus.
 */
#define CHV_SHOP 711
#define CHV_DIRE 712
#define CHV_GLASS 713
#define CHV_ENGINE 714
#define CH_MAXD 16
typedef struct { int t, v, s; } chd_t;
static chd_t g_chd[SD_N][CH_MAXD];
static unsigned long g_chitems[EN_N][5];                                   /* the shop chest's items, by the chest's en_t slot */
static int ch_ndrops(sd_t *d) { return d->a[4]; }
static void ch_add(sd_t *d, int t, int v, int s) {
    if (d->a[4] >= CH_MAXD) return;
    chd_t *c = &g_chd[d - g_sd][d->a[4]++]; c->t = t; c->v = v; c->s = s;
}
static void ch_vel(sd_t *d, int i, int n, float base, float *vx, float *vy) {         /* RandomVector():Rotated(i / n * 360 + rand(-90, 90)):Resized(rand(3, 6)) */
    float len = (float) sd_range(d, 3, 6); vrot(len, 0, base + (float) i / (float) (n ? n : 1) * 360.0f + (float) sd_range(d, -90, 90), vx, vy);
}
/* what the players hold (the Lua's mod.anyPlayerHas / HasTrinket / allPlayersAre...): trinkets are two ints at pl+0x1ab0 (+32768 when golden) */
static int pl_trinket(unsigned long pl, int id) { const int *t = (const int *) (pl + OFF_PL_TRINKETS); return (t[0] & 0x7fff) == id || (t[1] & 0x7fff) == id; }
static int any_trinket(int id) { for (int i = 0, n = sl_nplayers(); i < n; i++) { unsigned long pl = sl_player(i); if (pl && alive(pl) && pl_trinket(pl, id)) return 1; } return 0; }
static int any_item(int id) { for (int i = 0, n = sl_nplayers(); i < n; i++) { unsigned long pl = sl_player(i); if (pl && alive(pl) && g_has_item && g_has_item(pl, id, 0)) return 1; } return 0; }
static int all_players_type(int a, int b) {                                          /* every player is character a or b (the Lua's allPlayersAreBBlueBaby / allPlayersAreKeeper) */
    int n = sl_nplayers(); if (n <= 0) return 0;
    for (int i = 0; i < n; i++) { unsigned long pl = sl_player(i); if (!pl) return 0; int t = pl_type(pl); if (t != a && t != b) return 0; }
    return 1;
}
#define TRK_PAPER_CLIP 19
#define TRK_DAEMONS_TAIL 22
#define TRK_CHILDS_HEART 34
#define TRK_RUSTED_KEY 36
#define TRK_MATCH_STICK 41
#define TRK_LUCKY_TOE 42
#define TRK_ACE_SPADES 45
#define TRK_POKER_CHIP 76
#define TRK_BROKEN_PADLOCK 136
#define TRK_FLAT_FILE 151
#define COL_MOMS_KEY 199
#define COL_HUMBLEING_BUNDLE 203
#define COL_BOGO_BOMBS 250
#define COL_PAY_TO_PLAY 380
#define COL_SACK_HEAD 424
static int g_ch_mult = 1;                                                            /* Poker Chip's x2 for the pickups of the roll in course */
static void ch_random_card(sd_t *d, int pool) {                                       /* 0 rune (32..41, else the Rune Shard 55), 1 soul (81..97) */
    int id = pool == 0 ? 32 + sd_rand(d, 10) : 81 + sd_rand(d, 17);
    ch_add(d, 5, 300, id);
}

/* mod:GetRandomChestItem (glasschest.lua 120-300), every branch: a coin / bomb / key / heart row of the room's usual pickups */
enum { PK_DEFAULT, PK_DOUBLE };
static void ch_random_pickup(sd_t *d, int forced) {                                   /* forced: 0 none, 1 coins, 2 bombs, 3 keys, 4 hearts */
    int rand = sd_rand(d, 20), rep = 1, kind = 1;                                      /* kind: 1 coins, 2 bombs, 3 poops, 4 keys, 5 hearts, 6 flies */
    int t = 5, v = 20, sub = 1, isdef = 0, copper = 0;
    int allbb = all_players_type(4, 25), allkeeper = all_players_type(14, 33);
    if (!forced && any_trinket(TRK_DAEMONS_TAIL)) {                                    /* Daemon's Tail: the keys (13..15) are rerolled four times out of five */
        if (sd_rand(d, 5) != 1 && rand >= 13 && rand < 16) while (rand >= 13 && rand < 16) rand = sd_rand(d, 20);
    }
    if (forced) rand = 1000000;
    if (rand < 7 || forced == 1) {                                                     /* coins */
        kind = 1; v = 20; rand = sd_rand(d, 100);
        if (rand < 5) sub = 2; else if (rand < 6) sub = 3; else if (rand < 7) sub = 7; else if (rand < 8) sub = 214; else { isdef = 1; rep += sd_rand(d, 3); }
    } else if (rand < 13 || forced == 2) {                                             /* bombs */
        int out;                                                                       /* 0 default, 1 double, 2 troll, 3 megatroll, 4 golden, 5 copper */
        out = 0; rand = sd_rand(d, 100);
        if (rand < 20) out = 1; else if (rand < 25) out = 2; else if (rand < 26) out = 3; else if (rand < 27 && !allbb) out = 4; else if (rand < 28) out = 5;
        if (allbb && (out == 0 || out == 1)) { kind = 3; t = 5; v = 42; sub = out == 1 ? 1 : 0; isdef = out == 0; }              /* Blue Baby: poop nuggets instead */
        else {
            kind = 2; v = 40; sub = 1;
            if (out == 1) sub = 2; else if (out == 2) { t = 4; v = 3; sub = 0; } else if (out == 3) { t = 4; v = 4; sub = 0; } else if (out == 4) sub = 4; else if (out == 5) { sub = 923; copper = 1; }
            isdef = out == 0;
        }
    } else if (rand < 16 || forced == 3) {                                             /* keys */
        kind = 4; v = 30; rand = sd_rand(d, 100);
        if (rand == 1) sub = 2; else if (rand == 2) sub = 3; else isdef = 1;
    } else {                                                                           /* hearts */
        kind = 5; v = 10;
        if (allkeeper) { kind = 6; t = 3; v = 43; sub = 0; }                           /* Keeper: a friendly attack fly */
        else if (any_trinket(TRK_DAEMONS_TAIL)) sub = 6;
        else {
            rand = sd_rand(d, 100);
            if (rand == 0) sub = 5;
            else if (rand <= 10) {
                rand = sd_rand(d, 100);
                if (rand < 10) sub = 6; else if (rand < 20) sub = 8; else if (rand < 30) sub = 12; else if (rand < 35) { v = 1024; sub = 0; }
                else if (rand < 40) sub = 4; else if (rand < 41) sub = 7; else if (rand < 50) sub = 11;
                else if (rand < 51) { sub = 3; SL_NOTDONE("chests: the Steven Heart is a soul heart here (the pickup is not in the port)"); }
                else sub = 3;
            } else if (rand <= 60) sub = 2;
            else isdef = 1;
        }
    }
    if (isdef || (kind == 2 && copper)) {                                              /* Humbleing Bundle (all), Bogo Bombs (bombs and poops): the default becomes the double one */
        if (any_item(COL_HUMBLEING_BUNDLE) || (any_item(COL_BOGO_BOMBS) && (kind == 2 || kind == 3))) {
            if (kind == 2 && copper) sub = 924;
            else if (kind == 1) sub = 4; else if (kind == 2) sub = 2; else if (kind == 3) sub = 1; else if (kind == 4) sub = 2; else if (kind == 5) sub = 5;
        }
    }
    if (!forced && any_item(COL_SACK_HEAD)) {                                          /* Sack Head: 1 in 10 for coins, 2 in 10 for the rest, a grab bag instead */
        if (sd_rand(d, 10) < (kind == 1 ? 1 : 2)) { rep = 1; t = 5; v = 69; sub = 1; }
    }
    for (int k = 0; k < rep * g_ch_mult; k++) ch_add(d, t, v, sub);
}
static int g_ch_pedestal;                                                             /* 1 while a Glass Chest spills its drops: its items stand on its own pedestal */
static void ch_burst(unsigned long e, sd_t *d, float *p, int from) {                  /* the drops are tossed out around the chest */
    int n = ch_ndrops(d); float base = (float) sd_rand(d, 360);
    for (int i = from; i < n; i++) {
        chd_t c = g_chd[d - g_sd][i]; float vx, vy;
        if (c.t == 5 && c.v == 100) {
            unsigned long it = en_spawn(5, 100, p[0], p[1], 0, 0, e, 0);
            if (it && g_ch_pedestal && g_sheet_l) { g_sheet_l((void *) (it + E_SPRITE), 5, "gfx/items/slots/ff_chest_pedestals.png"); if (g_setoframe) g_setoframe((void *) (it + E_SPRITE), "Alternates", 4); }   /* glasschest.lua openGlassChest */
            continue;
        }
        ch_vel(d, i, n, base, &vx, &vy);
        if (c.t == 5 && c.v == 350) c.s = 0;
        en_spawn(c.t, c.v, p[0], p[1], vx, vy, e, c.s);
    }
}
static int ch_take_key(unsigned long pl) { if (pl_keys(pl) > 0) { pl_add_keys(pl, -1); return 1; } return 0; }
/* collider:HasTrinket(TRINKET_PAPER_CLIP) or collider:TryUseKey(): the player that touches the chest, and only that one. TryUseKey costs nothing with a golden key. The golden-key flag is the
 * byte at pl+0x16d9 (the golden-bomb flag is the one before it, +0x16d8): read from the code that swaps bombs and keys (0x53d450 swaps both counters and both bytes) - derived, not
 * seen alone; if it is wrong the golden key only fails to be free, a chest never opens without a key. */
#define OFF_PL_GOLDENKEY 0x16d9
static int ch_try_use_key(unsigned long pl) {
    if (pl_trinket(pl, TRK_PAPER_CLIP)) return 1;
    if (*(const unsigned char *) (pl + OFF_PL_GOLDENKEY) == 1) return 1;
    return ch_take_key(pl);
}
static void ch_touch_open_common(unsigned long e) { (void) e; en_sound(SOUND_CHEST_OPEN, 1.0f, 1.0f); en_sound(SOUND_UNLOCK00, 1.0f, 1.0f); }
static int ch_explosion_near(float *p, float r) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long v = snap[k]; if (!alive(v) || *(const int *) (v + E_TYPE) != 1000 || *(const int *) (v + E_VARIANT) != 1) continue;
        if (vlen(fpos(v)[0] - p[0], fpos(v)[1] - p[1]) < r) return 1;
    }
    return 0;
}

/* ============================================================================================ Shop Chest 5.711 */
enum { SC_CLOSED, SC_OPENING, SC_OPENED, SC_LEAVING, SC_DONE, SC_EXPLODED };
static const signed char SHOPCH[] = { 1,1,1,1,1, 2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 4,4, 5 };
static const float SHOPROT[6][3] = { {0,0,0}, {0,40,0}, {-40,0,-180}, {-40,0,-90}, {-40,-20,-75}, {-40,0,-45} };      /* start x, y, rotation per item */
static void sc_items(en_t *s, unsigned long e, sd_t *d, float *p, int dont) {
    en_sound(SOUND_SUMMONSOUND, 1.0f, 1.0f);
    if (dont) return;
    int n = SHOPCH[sd_rand(d, (int) sizeof SHOPCH)], slot = (int) (s - g_en);
    for (int i = 0; i < n; i++) {
        float sx, sy; vrot(SHOPROT[n][0], SHOPROT[n][1], SHOPROT[n][2] * (float) i, &sx, &sy);
        unsigned long it = en_spawn(5, 150, p[0] + sx, p[1] + sy, 0, 0, e, 0);
        en_spawn(1000, 15, p[0] + sx, p[1] + sy, 0, 0, 0, 15);
        g_chitems[slot][i] = it;
    }
    s->i[1] = n; s->i[2] = n;
    SL_NOTDONE("Shop Chest: secret-room items and the room-type spritesheets are not done (it always spawns shop items 5.150)");
}
static int sc_alive_items(en_t *s) {
    int slot = (int) (s - g_en), n = 0;
    for (int i = 0; i < 5; i++) { unsigned long it = g_chitems[slot][i]; if (it && alive(it) && *(const int *) (it + E_TYPE) == 5) n++; }
    return n;
}
static void ch_shop(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e); int slot = (int) (s - g_en);
    if (!s->init) {
        s->init = 1; s->age = 0; s->key = ((unsigned) 5 << 20) | ((unsigned) CHV_SHOP << 4); s->i[0] = s->sub; memset(g_chitems[slot], 0, sizeof g_chitems[slot]);
        if (d->a[6] && s->sub == 0) { en_remove(e); return; }                      /* opened and left before: the chest does not come back */
        if (s->sub > 2) { if (s->sub == SC_LEAVING && sd_rand(d, 10) <= 7) en_spawn(5, 0, p[0], p[1], 0, 0, 0, 0); en_remove(e); return; }
        else if (s->sub == 1) { en_replay(s, "Opened"); sc_items(s, e, d, p, 0); s->state = SC_OPENED; }
        else if (s->sub == 2) { en_replay(s, "Opened"); s->state = SC_OPENED; }
        else { en_replay(s, "Appear"); s->state = SC_CLOSED; }
    }
    if (en_finished(s, "Appear")) en_replay(s, "Idle");
    if (en_finished(s, "Open")) en_replay(s, "Opened");
    if (en_event(s, "Appear", "DropSound") || en_event(s, "Close", "DropSound")) en_sound(SOUND_CHEST_DROP, 1.0f, 1.0f);
    if (en_event(s, "Open", "Spawnem")) { sc_items(s, e, d, p, s->i[0] > 1); s->state = SC_OPENED; }
    if (s->state == SC_OPENED) {
        int cnt = sc_alive_items(s);
        if (cnt < 1) { en_replay(s, "Close"); s->state = SC_LEAVING; }
        else if (cnt < s->i[2]) { s->i[2] = cnt; en_replay(s, "OpenedHappy"); }
        if (s->state == SC_OPENED && ch_explosion_near(p, 80.0f)) { s->state = SC_EXPLODED; en_replay(s, "Destroyed"); }
    }
    if (!strcmp(s->anim, "Close") && s->af == 21) { s->state = SC_DONE; if (sd_rand(d, 10) <= 7) en_spawn(5, 0, p[0], p[1], 0, 0, 0, 0); }
    if (en_finished(s, "Close")) { en_remove(e); return; }
    if (s->state == SC_CLOSED && (!strcmp(s->anim, "Idle") || (!strcmp(s->anim, "Appear") && s->af > 7)) && !d->a[6]) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl && ch_take_key(pl)) { d->a[6] = 1; en_replay(s, "Open"); s->state = SC_OPENING; ch_touch_open_common(e); }
    }
}

/* ============================================================================================ Dire Chest 5.712 (a gamble: every open may be the last) */
static void ch_dire(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) { s->init = 1; s->age = 0; s->key = ((unsigned) 5 << 20) | ((unsigned) CHV_DIRE << 4); en_replay(s, "Appear"); s->state = 0; if (d->a[3] == 2) { en_replay(s, "Opened"); s->state = 3; } }
    if (en_event(s, "Appear", "DropSound") || en_event(s, "CloseBack", "DropSound")) en_sound(SOUND_CHEST_DROP, 1.0f, 1.0f);
    if (en_finished(s, "Appear")) en_replay(s, "Idle");
    if (en_event(s, "Open", "Leave") && s->state == 2) { en_remove(e); return; }
    if (en_finished(s, "Open") && !d->a[2]) en_replay(s, "CloseBack");
    if (en_finished(s, "CloseBack")) en_replay(s, "Idle");
    if (en_finished(s, "OpenNothing")) en_replay(s, "OpenedNothing");
    if (!strcmp(s->anim, "Idle") && s->state == 0) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl) {
            en_sound(SOUND_CHEST_OPEN, 1.0f, 1.0f);
            int chance = sd_rand(d, 11);
            if (chance <= d->a[0]) {                                              /* the end: positive 1 in 5, else negative */
                if (sd_rand(d, 5) == 0) {
                    en_replay(s, "Open"); en_sound(SOUND_THUMBSUP, 0.4f, 1.0f); d->a[2] = 1; d->a[3] = 1;
                    en_spawn(4, 3, p[0], p[1], 0, 0, e, 0);
                    if (sd_rand(d, 3) == 0) { en_spawn(5, 100, p[0], p[1], 0, 0, e, 0); s->state = 2; }                      /* the pool item; the chest leaves at the "Leave" event */
                    else { float vx, vy; vrot((float) sd_range(d, 4, 7), 0, 50.0f + (float) sd_range(d, 10, 50), &vx, &vy); en_spawn(5, 350, p[0], p[1], vx, vy, e, 0); }
                } else {
                    en_replay(s, "OpenNothing"); en_sound(SOUND_MOM_VOX_EVILLAUGH, 1.0f, 1.5f); d->a[2] = 1; d->a[3] = 2; s->state = 3;
                    en_explode(e, p[0], p[1], 4.0f, 1.0f, 0);
                    if (sd_rand(d, 4) == 1) SL_NOTDONE("Dire Chest: the Card of Humanity is not used on the player");
                }
            } else {
                en_replay(s, "Open"); d->a[0]++;
                int n = 1; if (0) n = 2;
                for (int k = 0; k < n; k++) {
                    int drop = sd_rand(d, 13); float vx, vy;
                    switch (drop) {
                    case 0: case 1: SL_NOTDONE("Dire Chest: the friendly Attack Skuzz (familiar 3.1026) are not spawned"); break;
                    case 2: case 9: if (en_find(666, 60)) for (int j = 0; j < 2; j++) { en_rand_vec(&vx, &vy, (float) sd_range(d, 5, 15)); en_spawn(666, 60, p[0] + vx, p[1] + vy, 0, 0, e, 0); } break;
                    case 3: case 4: for (int j = 1 + sd_rand(d, 3); j > 0; j--) { vrot((float) sd_range(d, 4, 7), 0, (float) (sd_range(d, 1, 90) + 45), &vx, &vy); en_spawn(5, 20, p[0], p[1], vx, vy, e, 213); } break;
                    case 5: vrot((float) sd_range(d, 3, 4), 0, 90.0f, &vx, &vy); en_spawn(5, 30, p[0], p[1], vx, vy, e, 185); break;
                    case 6: sd_rand(d, 8); vrot((float) sd_range(d, 3, 4), 0, 90.0f, &vx, &vy); en_spawn(5, 300, p[0], p[1], vx, vy, e, 0); break;
                    case 7: vrot((float) sd_range(d, 3, 4), 0, 90.0f, &vx, &vy); en_spawn(5, 1024, p[0], p[1], vx, vy, e, 0); break;
                    case 8: SL_NOTDONE("Dire Chest: the two Blots (170.40) are not spawned"); break;
                    case 10: if (sd_rand(d, 2) == 1) { vrot((float) sd_range(d, 4, 7), 0, 90.0f, &vx, &vy); en_spawn(5, 300, p[0], p[1], vx, vy, e, 44); } else SL_NOTDONE("Dire Chest: the Fortune is not shown"); break;
                    case 11: vrot((float) sd_range(d, 3, 4), 0, 90.0f, &vx, &vy); en_spawn(5, 40, p[0], p[1], vx, vy, e, 923); break;
                    case 12: for (int j = 0; j <= sd_rand(d, 3); j++) { en_rand_vec(&vx, &vy, 3.0f); en_spawn(5, 903, p[0], p[1], vx, vy, e, 0); } break;
                    }
                }
            }
        }
    }
}

/* ============================================================================================ Glass Chest 5.713 */
static void ch_glass_fill(sd_t *d) {                                                  /* the Lua's d.drops (glasschest.lua 328-465), rolled once at the first Update */
    d->a[4] = 0; g_ch_mult = 1;
    int pokerfly = 0, pokermulti = 1, momsmulti = 1;
    if (any_trinket(TRK_POKER_CHIP)) { if (sd_rand(d, 2) == 0) pokerfly = 1; else pokermulti = 2; }
    if (any_item(COL_MOMS_KEY)) momsmulti = 2;
    if (pokerfly) { ch_add(d, 18, 0, 0); return; }                                    /* a lone attack fly */
    if (sd_rand(d, 50) == 1) {
        int rand = sd_rand(d, 50);
        if (rand == 1) ch_add(d, 5, 100, 0);                                          /* GLASS_CHEST_RARE: the room's pedestal here (the pool is the engine's) */
        else if (rand < 10) for (int k = 0; k < pokermulti; k++) ch_add(d, 5, 350, 0);
        else ch_add(d, 5, 100, 0);                                                    /* GLASS_CHEST_COMMON */
        return;
    }
    int rand = sd_rand(d, 20), cnt = 0;
    if (rand < 1) for (int k = 0; k < pokermulti; k++) ch_random_card(d, 0);
    else if (rand < 2) for (int k = 0; k < pokermulti; k++) ch_random_card(d, 1);
    else if (rand < 5) for (int k = 0; k < pokermulti; k++) ch_add(d, 5, 300, 0);       /* a glass dice card (the mod's): a random card here */
    else if (rand < 10) cnt = 2; else if (rand < 16) cnt = 3; else if (rand < 20) cnt = 4;
    else { ch_add(d, 20, 0, 0); for (int i = 0; i < 3; i++) ch_add(d, 10, 1, 0); }      /* a Monstro and three Frowning Gapers - the Lua's roll never gets here either (RandomInt(20) is 0..19) */
    if (!cnt) return;
    if (any_trinket(TRK_LUCKY_TOE) && sd_rand(d, 3) == 0) cnt++;
    cnt *= momsmulti; g_ch_mult = pokermulti;
    for (int i = 0; i < cnt; i++) ch_random_pickup(d, 0);                             /* every pickup is repeated Repeat x Poker Chip's multiplier (g_ch_mult) */
    g_ch_mult = 1;
    if (any_trinket(TRK_RUSTED_KEY) && sd_rand(d, 3) == 0) ch_add(d, 5, 30, 1);          /* insertPickup(chest, d, "Keys"): the default key */
    if (any_trinket(TRK_CHILDS_HEART) && sd_rand(d, 3) == 0) ch_add(d, 5, 10, 1);        /* "Hearts": the full heart */
    if (any_trinket(TRK_MATCH_STICK) && sd_rand(d, 3) == 0) ch_add(d, 5, 40, 1);         /* "Bombs": the default bomb */
    if (any_trinket(TRK_ACE_SPADES) && sd_rand(d, 3) == 0) ch_random_card(d, sd_rand(d, 2) == 1 ? 0 : 1);   /* a rune or a soul */
}
static int ch_has_item_drop(sd_t *d) { for (int i = 0; i < ch_ndrops(d); i++) if (g_chd[d - g_sd][i].t == 5 && g_chd[d - g_sd][i].v == 100) return 1; return 0; }
enum { GL_CLOSED, GL_OPENED, GL_SHATTER };
/* openGlassChest: the drops come out, the chest stays opened (it is removed when the room is entered again; with an item among the drops it goes at once) */
static void ch_glass_open(en_t *s, unsigned long e, sd_t *d, float *p, int animate) {
    d->a[6] = 2; s->state = GL_OPENED;
    if (animate) { ch_touch_open_common(e); en_replay(s, "Open"); }
    g_ch_pedestal = 1; ch_burst(e, d, p, 0); g_ch_pedestal = 0;
    if (ch_has_item_drop(d)) en_remove(e);
}
/* an explosion within 90 px, or (when it is true) a sharp key tear touching it: the glass breaks - Shatter, or ShatterFade with a Flat File - and, with a Broken Padlock, it opens first */
static void ch_glass_shatter(en_t *s, unsigned long e, sd_t *d, float *p) {
    en_sound(SOUND_MIRROR_BREAK, 0.8f, (float) sd_range(d, 110, 130) / 100.0f);
    en_replay(s, any_trinket(TRK_FLAT_FILE) ? "ShatterFade" : "Shatter");
    en_set_collision(e, ENTCOLL_NONE);
    if (!d->a[6] && any_trinket(TRK_BROKEN_PADLOCK)) ch_glass_open(s, e, d, p, 0);
    d->a[6] = 2; s->state = GL_SHATTER;                                               /* gone for good, loot or not */
}
static int ch_key_tear_near(const float *p, float size) {                             /* a sharp key's tear (variant 43 KEY, 44 KEY_BLOOD) within its own radius of the chest */
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long t = snap[k]; if (!alive(t) || *(const int *) (t + E_TYPE) != 2) continue;
        int tv = *(const int *) (t + E_VARIANT); if (tv != 43 && tv != 44) continue;
        if (vlen(fpos(t)[0] - p[0], fpos(t)[1] - p[1]) < *(const float *) (t + E_SIZE) + size) return 1;
    }
    return 0;
}
static void ch_glass(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    if (!s->init) {
        s->init = 1; s->age = 0; s->key = ((unsigned) 5 << 20) | ((unsigned) CHV_GLASS << 4); s->state = GL_CLOSED;
        if (d->a[6] == 2 || s->sub == 1) { en_remove(e); return; }                    /* already opened or broken: the chest is gone */
        if (!d->a[5]) { d->a[5] = 1; ch_glass_fill(d); }
        en_replay(s, "Appear");
        if (any_item(COL_PAY_TO_PLAY)) {                                              /* Pay To Play: it takes a coin, not a key, and has its own sheet */
            s->i[0] = 1; if (g_sheet_l) g_sheet_l((void *) (e + E_SPRITE), 0, "gfx/items/slots/glass_chest_paytoplay.png");
        }
    }
    if (s->state == GL_SHATTER) { if (en_finished(s, "Shatter") || en_finished(s, "ShatterFade")) en_remove(e); return; }
    if (en_event(s, "Appear", "DropSound")) en_sound(SOUND_CHEST_DROP, 1.0f, 1.0f);
    if (en_finished(s, "Appear")) en_replay(s, "Idle");
    if (en_finished(s, "Open")) en_replay(s, "Opened");
    if (s->age >= 5 && ch_explosion_near(p, 90.0f)) { ch_glass_shatter(s, e, d, p); return; }
    if (s->state == GL_CLOSED && !d->a[6] && (!strcmp(s->anim, "Idle") || en_event(s, "Appear", "DropSound") || s->af > 7)) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl) {
            int paid = s->i[0] ? (pl_coins(pl) > 0) : ch_try_use_key(pl);              /* the player that touches it, and only that one */
            if (paid && s->i[0]) pl_add_coins(pl, -1);
            if (paid) { ch_glass_open(s, e, d, p, 1); return; }
        }
        if (ch_key_tear_near(p, *(const float *) (e + E_SIZE))) { ch_glass_open(s, e, d, p, 1); return; }   /* a sharp key opens it */
    }
}

/* ============================================================================================ Engine Chest 5.714 */
static void ch_engine_fill(sd_t *d) {
    static const struct { const char *k; int w; } T[] = { {"item", 11}, {"pickups", 56}, {"trinket", 10}, {"card", 10}, {"battery", 11}, {"chest", 2} };
    d->a[4] = 0;
    int roll = sd_roll(d, 1, 100), acc = 0, k = 0;
    for (; k < 6; k++) { acc += T[k].w; if (roll <= acc) break; }
    if (k > 5) k = 1;
    if (k == 0) ch_add(d, 5, 100, 0);
    else if (k == 2) ch_add(d, 5, 350, 0);
    else if (k == 3) ch_add(d, 5, 300, 0);
    else if (k == 4) {
        int amount = sd_range(d, 2, 3);
        for (int j = 0; j < amount; j++) {
            int r = sd_roll(d, 1, 100);
            if (r <= 40) ch_add(d, 5, 90, 1); else if (r <= 80) ch_add(d, 5, 90, 3); else if (r <= 85) ch_add(d, 5, 90, 2); else if (r <= 90) ch_add(d, 5, 90, 4); else ch_add(d, 5, 300, 0);
        }
    } else if (k == 5) { if (sd_rand(d, 2) == 0) ch_add(d, 5, 50, 1); else ch_add(d, 5, 60, 0); }
    else {
        int amount = sd_range(d, 2, 6);
        for (int j = 0; j < amount; j++) {
            int r = sd_roll(d, 1, 100);                                              /* weights 35 / 20 / 15 / 30 (coins, hearts, keys, bombs) */
            ch_random_pickup(d, r <= 35 ? 1 : r <= 55 ? 4 : r <= 70 ? 3 : 2);
        }
    }
}
static void ch_engine(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e);
    static const char *const IDLE[] = { "Idle_01", "Idle_02", "Idle_03" }, *const PIP[] = { "Pip_01", "Pip_02" };
    if (!s->init) {
        s->init = 1; s->age = 0; s->key = ((unsigned) 5 << 20) | ((unsigned) CHV_ENGINE << 4);
        if (d->a[6] == 2 || s->sub == 1) { en_remove(e); return; }
        if (!d->a[5]) { d->a[5] = 1; ch_engine_fill(d); }
        en_replay(s, "Appear"); s->state = 0;
        SL_NOTDONE("Engine Chest: it is charged by touching it (one pip a touch), not from the player's active items / batteries (unreadable here)");
    }
    if (en_event(s, "Appear", "DropSound")) en_sound(SOUND_CHEST_DROP, 0.9f, 1.0f);
    if (en_finished(s, "Appear")) { en_replay(s, IDLE[d->a[0] > 2 ? 2 : d->a[0]]); s->state = 0; }
    if (s->state == 1 && (en_finished(s, "Pip_01") || en_finished(s, "Pip_02"))) { en_replay(s, IDLE[d->a[0] > 2 ? 2 : d->a[0]]); s->state = 0; }
    if (en_finished(s, "Open")) en_replay(s, "Opened");
    if (s->state == 0 && !d->a[6] && !strncmp(s->anim, "Idle_0", 6)) {
        unsigned long pl = sl_toucher(e, 1.0f, 1.0f);
        if (pl) {
            d->a[0]++;
            if (d->a[0] >= 3) {
                d->a[6] = 2; s->state = 2; en_replay(s, "Open"); en_sound(SOUND_CHEST_OPEN, 1.0f, 1.0f); en_sound(SOUND_BATTERYCHARGE, 0.6f, 0.9f);
                ch_burst(e, d, p, 0); en_remove(e); return;
            }
            en_sound(SOUND_BEEP, 0.8f, (float) sd_range(d, 80, 90) / 100.0f); en_replay(s, PIP[d->a[0] - 1]); s->state = 1;
        }
    }
}

static const struct { int var; void (*fn)(en_t *, unsigned long); } CH_TAB[] = {
    { CHV_SHOP, ch_shop }, { CHV_DIRE, ch_dire }, { CHV_GLASS, ch_glass }, { CHV_ENGINE, ch_engine },
};
static void en_g13_tick(void) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long e = snap[k]; if (!alive(e) || *(const int *) (e + E_TYPE) != 5) continue;
        int var = *(const int *) (e + E_VARIANT);
        for (unsigned i = 0; i < sizeof CH_TAB / sizeof CH_TAB[0]; i++) {
            if (CH_TAB[i].var != var) continue;
            en_t *s = en_slot(e);
            if (s->init) sl_tick_frames(s);
            CH_TAB[i].fn(s, e);
            en_state_watch(s);
            if (alive(e) && s->init) { s->age++; sl_sprite(s, e); }
            break;
        }
    }
}
