/* ffen_g11.h - Fiend Folio's "(WAITING)" monsters: the hidden spiders / demons (150.10.x, Lua mod:waitingSpiderAI in main.lua) and worms (150.16.x, mod:waitingWormAI),
 * and the trigger they all wait for, mod.CanIComeOutYet() (ffscripts/erfly/enemies/_shared.lua). Included by ffenemy.h after ffen_g10.h.
 *
 * CanIComeOutYet: room frame > 1; the trigger pressure plates pressed (NOT checked here: the port reads no grid entity states); every "active enemy" of the room that is not a
 * blacklisted one (mod.BadEnts: fireplaces, grimaces, masks, Technical entities ...) dead for 2 + 6 consecutive calls (comeconsecutivecheck, comeconsecutivecheck2 > 5; the Lua
 * counts one step per CALL, i.e. per waiting monster per frame, and so does this). Once true it stays true for the room (waitingEnemiesTriggered).
 * A new room is told by a waiting monster starting more than 2 frames after the last one did (all of a room's monsters start on its first frames).
 *
 * spider  150.10.sub: 0 Lvl2 Spider, 1 Ticking Spider, 2 Baby Long Legs, 3 Small BLL, 4 Crazy Long Legs, 5 Small CLL, 6 Full Spider, 7 Null, 8 Bone Knight, 9 Black Globin,
 *                     10 Doomer, 11 Loki, 12 Imp, 13 Soul Spooter, 14 Tiyanak.   i[0] = emerging (1), cd = StateFrame.
 * worm    150.16.sub: 1 Round Worm, 2 Nightcrawler, 3 Ulcer, 4 Roundy, 5 Tube Worm, 6 Parabite, 7 Scarred Parabite, 8 Bone Worm, 9 Drink Worm, 10 Lump, 11 Fred, 12 Weaver, 13 Weaver Sr.,
 *                     14 Pin, 15 Scolex, 16 Frail, 17 Kingpin, 18 Bunker Worm, 19 Drunk Worm.
 * A target the port does not have (ok = 0) is not spawned: the rooms that hold such a waiting monster are not made (tools/ff_rooms.py judges them 'unported'). */

static int g11_trig, g11_c1, g11_c2 = -1, g11_start = -100000, g11_lastinit = -100000, g11_ticks;
static int g11_now(void) { return g_frame_fn ? g_frame_fn() : g11_ticks; }

/* mod.dontpreventwaiting(entity) == false: does this entity keep the waiting monsters in? */
static int g11_blocks(unsigned long e) {
    if (!alive(e)) return 0;
    int type = *(const int *) (e + E_TYPE), var = *(const int *) (e + E_VARIANT);
    if (type < 10 || type >= 1000) return 0;                                           /* NPCs only */
    if (*(const unsigned long long *) (e + E_FLAGS) & EF_FLAG_FRIENDLY) return 0;
    if (type == 33 || type == 42 || type == 93 || type == 96 || type == 218 || type == 291 || type == 302) return 0;   /* fireplace, grimaces, masks, Eternal Fly, wall huggers, Pitfall, Stoney */
    if ((type == 79 && var == 12) || (type == 409 && var == 1)) return 0;               /* Blighted Ovum, purple ball */
    if (type == 150 || type == 183 || type == 184) return 0;                           /* Technical entities (the waiting monsters themselves too) and the port's pseudo-grids */
    if (type == 114 && var == 1004) return 0;                                          /* Ferrium waiting helper */
    return vcall(e, VT_ACTIVE_ENEMY);
}

static int g11_can_come_out(void) {
    if (g11_now() - g11_start <= 1) return 0;
    if (g11_trig) return 1;
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    int good = 1;
    for (unsigned k = 0; k < n; k++) if (g11_blocks(snap[k])) { good = 0; break; }
    if (!good) { g11_c1 = 0; g11_c2 = -1; return 0; }
    if (!g11_c1) { g11_c1 = 1; return 0; }
    if (g11_c2 > 5) { g11_trig = 1; return 1; }
    g11_c2 = g11_c2 < 0 ? 1 : g11_c2 + 1;
    return 0;
}

/* a waiting monster starts: the first one of a room resets the room's trigger */
static void g11_start_room(void) {
    int now = g11_now();
    if (now - g11_lastinit > 2) { g11_trig = 0; g11_c1 = 0; g11_c2 = -1; g11_start = now; }
    g11_lastinit = now;
}
/* mod.farFromAllPlayers(pos, dist) */
static int g11_far(unsigned long e, float dist) {
    unsigned long pl = g_first_player ? g_first_player() : 0; if (!pl) return 1;
    const float *p = fpos(e), *q = fpos(pl);
    return vlen(q[0] - p[0], q[1] - p[1]) > dist;
}

#define G11_WAIT_FLAGS (EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET | EF_FLAG_DONT_COUNT_BOSS_HP)
typedef struct { int type, variant, ok; } g11_tg_t;
static const g11_tg_t g11_spider[15] = {
    { 215, 0, 1 }, { 250, 0, 1 }, { 206, 0, 1 }, { 206, 1, 1 }, { 207, 0, 1 }, { 207, 1, 1 }, { 215, 710, 1 },
    { ET_ENTITY_NULLS, 0, 1 }, { ET_ENTITY_BONE_KNIGHT, 0, 1 }, { ET_ENTITY_BLACK_GLOBIN, 0, 1 }, { 151, 8, 0 }, { ET_ENTITY_LOKI, 0, 1 }, { ET_ENTITY_IMP, 0, 1 }, { 215, 114, 0 }, { 0, 0, 0 } };
static const g11_tg_t g11_worm[20] = {
    { 0, 0, 0 },
    { 244, 0, 1 }, { 255, 0, 1 }, { 289, 0, 1 }, { 276, 0, 1 }, { 244, 1, 1 }, { 58, 0, 1 }, { 58, 1, 1 }, { 244, 960, 0 }, { 666, 100, 1 }, { 56, 0, 1 },
    { 59, 0, 1 }, { 155, 0, 1 }, { 155, 1, 0 }, { 62, 0, 0 }, { 62, 1, 0 }, { 62, 2, 0 }, { 180, 80, 0 }, { 160, 1240, 0 }, { 666, 101, 0 } };

static void g11_init(en_t *s, unsigned long e) {
    s->init = 1; s->cd = 0;
    g11_start_room();
    en_flag_clear(e, EF_FLAG_APPEAR); en_set_collision(e, ENTCOLL_NONE); en_flag_set(e, G11_WAIT_FLAGS); en_visible(e, 0);
    fvel(e)[0] = fvel(e)[1] = 0;
    ENLOG(360, 1, "ffenemy: not done: waiting monsters - the trigger pressure plates are not checked, a Pin / Kingpin / Bone Worm / Doomer / Soul Spooter / Tiyanak / Bunker & Drunk Worm / Full Spider / Weaver Sr. target is not spawned, and the vanilla worms and Lump / Fred come out with the engine's own appear animation instead of the Lua's Appear / Emerge / DigUp");
}

static void ai_wait_spider(en_t *s, unsigned long e) {
    if (!s->init) g11_init(s, e); else s->cd++;
    int sub = s->sub; if (sub < 0 || sub > 14) { en_remove(e); return; }
    float *v = fvel(e);
    if (!s->i[0]) {
        v[0] = v[1] = 0;
        if (g11_can_come_out()) {
            if (s->cd > 15 && g11_far(e, 60.0f)) {
                s->i[0] = 1; en_flag_clear(e, G11_WAIT_FLAGS); en_play(s, "Drop"); en_visible(e, 1);
                if (sub >= 7) en_sound(SOUND_PORTAL_OPEN, 1.0f, 1.0f);
            }
        } else s->cd = 0;
    } else {
        v[0] = v[1] = 0;
        if (en_finished(s, "Drop")) {
            const g11_tg_t *t = &g11_spider[sub];
            if (t->ok) { en_flag_clear(e, EF_FLAG_APPEAR); en_morph(e, t->type, t->variant, 0); }
            else { ENLOG(361, 6, "ffenemy: not done: waiting spider %d.10.%d turns into a monster the port does not have", s->type, sub); en_remove(e); }
        } else if (en_event(s, "Drop", "DMG")) en_set_collision(e, ENTCOLL_ALL);
        else en_play(s, "Drop");
    }
}
static void die_wait_spider(en_t *s, unsigned long e) {
    float *p = fpos(e);
    if (s->sub == 1) en_explode(e, p[0], p[1], 20.0f, 0.5f, 0);                               /* Isaac.Explode(pos, npc, 20) */
    else if (s->sub == 6) { float a, b; vrot(8.0f, 0, frand() * 360.0f, &a, &b); for (int i = 60; i <= 360; i += 60) { float vx, vy; vrot(a, b, (float) i, &vx, &vy); en_shoot(e, 0, p[0], p[1], vx, vy); } }
}

static void ai_wait_worm(en_t *s, unsigned long e) {
    if (!s->init) g11_init(s, e); else s->cd++;
    int sub = s->sub; if (sub < 1 || sub > 19) { en_remove(e); return; }
    float *v = fvel(e);
    if (!s->i[0]) {
        v[0] = v[1] = 0;
        if (g11_can_come_out()) {
            if (s->cd > 15 && g11_far(e, 60.0f)) {
                s->i[0] = 1; en_flag_clear(e, G11_WAIT_FLAGS & ~(EF_FLAG_NO_TARGET | EF_FLAG_DONT_COUNT_BOSS_HP)); en_visible(e, 1);
                const g11_tg_t *t = &g11_worm[sub];
                if (!t->ok) { ENLOG(362, 8, "ffenemy: not done: waiting worm 150.16.%d turns into a monster the port does not have", sub); en_remove(e); return; }
                if (sub == 12) en_play(s, "Empty");                                                            /* the Weaver: nothing to see, then it is there */
                else if (sub == 9) { en_set_collision(e, ENTCOLL_ALL); en_play(s, "Appear"); }                  /* Drink Worm */
                else {                                                                                       /* the vanilla worms: the engine's own appear */
                    float *p = fpos(e); unsigned long n = en_spawn(t->type, t->variant, p[0], p[1], 0, 0, 0, 0);
                    en_remove(e); (void) n; return;
                }
            }
        } else s->cd = 0;
    } else {
        v[0] = v[1] = 0;
        const g11_tg_t *t = &g11_worm[sub];
        if (sub == 12 && en_finished(s, "Empty")) { en_flag_clear(e, EF_FLAG_APPEAR); en_morph(e, t->type, t->variant, 0); }
        else if (sub == 9 && en_finished(s, "Appear")) { en_morph(e, t->type, t->variant, 0); }
    }
}

static void en_g11_tick(void) { g11_ticks++; }

static const enemy_t g_tab_g11[] = {
    { 150, 10, ai_wait_spider, die_wait_spider, 0, 0 },
    { 150, 16, ai_wait_worm, 0, 0, 0 },
};
#define N_TAB_G11 ((int) (sizeof g_tab_g11 / sizeof g_tab_g11[0]))

/* ------------------------------------------------------------------------------------------ Hunk of Gunk 150.39 (ferrium/enemies/hunkofgunk.lua)
 * sub bit 0 = small, bit 1 = flies.  state 0 idle, 1 collapse, 2 shake.  i[0] = small, i[1] = fly, i[2] = poop chance increase, i[3] = death done.  f[0], f[1] = its place.  cd = StateFrame. */
static void ai_hunk(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    char nm[32];
    if (!s->init) {
        s->init = 1; s->cd = 0; s->state = 0; s->f[0] = p[0]; s->f[1] = p[1];
        s->key = (150u << 20) | (39u << 4);                                                 /* one animation set for every subtype (the table is keyed by subtype & 15) */
        if (rint_(0, 1) == 0) en_flip(e, 1);
        en_flag_clear(e, EF_FLAG_APPEAR); en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET);
        s->i[1] = (s->sub >> 1) & 1;
        if (s->sub & 1) { s->i[0] = 1; *(float *) (e + 0x354) = 12.0f; *(float *) (e + 0x358) = 12.0f; }
        ENLOG(363, 1, "ffenemy: not done: Hunk of Gunk - its state kept per room (damage / collapsed / gone when you come back), the pathfinding block, Petrified Poop / Butt Penny death bonus, thrown friendly dips fly out (they appear on the spot), flies sink out of it (PositionOffset)");
    } else s->cd++;
    v[0] = s->f[0] - p[0]; v[1] = s->f[1] - p[1];                                           /* npc.Velocity = initPos - npc.Position */
    const char *pre = s->i[0] ? "Small" : "Large", *suf = s->i[1] ? "Fly" : "";
    if (s->state == 0) { snprintf(nm, sizeof nm, "%s%s", pre, suf); en_play(s, nm); }
    else if (s->state == 1) {
        snprintf(nm, sizeof nm, "LargeCollapse%s", suf);
        if (en_finished(s, nm)) { s->state = 0; s->i[0] = 1; }
        else if (en_event(s, nm, "Spawn")) en_sound(SOUND_PLOP, 1.0f, 1.0f);
        else en_play(s, nm);
    } else {
        snprintf(nm, sizeof nm, "%sShake%s", pre, suf);
        if (en_finished(s, nm)) s->state = 0; else en_play(s, nm);
    }
}
static unsigned long hunk_spawn(en_t *s, unsigned long e, int type, int set_dir) {
    float *p = fpos(e); float dx = 0, dy = 0;
    if (set_dir) vrot(0, (float) rint_(350, 500) / 100.0f, (float) rint_(0, 359), &dx, &dy);
    unsigned long n = en_spawn(type, 0, p[0], p[1], dx, dy, e, 0); (void) s;
    if (n) en_flag_clear(n, EF_FLAG_APPEAR);
    return n;
}
static void hunk_dip(en_t *s, unsigned long e) { unsigned long d = hunk_spawn(s, e, ET_ENTITY_DIP, 0); if (d) en_set_state(d, 4); }
static void hurt_hunk(en_t *s, unsigned long e, float dmg) {
    float hp = fb_hp(e);
    if (hp - dmg < 12.0f && !s->i[0]) s->state = 1;
    if (s->cd > 5 && dmg < hp) {
        if (s->state != 1) s->state = 2;
        s->cd = 0;
        int ch = rint_(0, 99); float *p = fpos(e);
        if (s->i[1]) {
            hunk_spawn(s, e, ch < 10 ? ET_ENTITY_POOTER : ch < 40 ? ET_ENTITY_ATTACKFLY : ET_ENTITY_FLY, 1);
            en_sound(SOUND_MEAT_JUMPS, 0.3f, (float) rint_(120, 140) / 100.0f);
        } else {
            s->i[2] += 20;
            if (ch < 30 + s->i[2]) {
                s->i[2] = 0; hunk_dip(s, e);
                en_sound(SOUND_BLOBBY_WIGGLE, 0.4f, (float) rint_(135, 155) / 100.0f); en_sound(SOUND_MEAT_JUMPS, 0.3f, (float) rint_(120, 140) / 100.0f);
                en_spawn(1000, 2, p[0], p[1], 0, 0, e, 100);
            }
        }
    }
}
static void die_hunk(en_t *s, unsigned long e) {
    if (s->i[3]) return;
    s->i[3] = 1; en_sound(SOUND_PLOP, 1.0f, 1.5f);
    float *p = fpos(e), r = frand() * 4.8f;
    if (s->i[1]) {                                                                          /* Pooter 1, friendly dips 2, coin 1, heart 0.8 */
        if (r < 1.0f) { unsigned long f = hunk_spawn(s, e, ET_ENTITY_POOTER, 0); (void) f; }
        else if (r < 3.0f) r = -1; else if (r < 4.0f) en_spawn(5, 20, p[0], p[1], 0, 0, e, 0); else en_spawn(5, 10, p[0], p[1], 0, 0, e, 0);
    } else {                                                                                /* Dip 1, friendly dips 2, coin 1, heart 0.8 */
        if (r < 1.0f) hunk_dip(s, e);
        else if (r < 3.0f) r = -1; else if (r < 4.0f) en_spawn(5, 20, p[0], p[1], 0, 0, e, 0); else en_spawn(5, 10, p[0], p[1], 0, 0, e, 0);
    }
    if (r < 0) for (int i = 0; i < 3; i++) { float dx, dy; vrot(0, (float) rint_(40, 60), (float) rint_(0, 359), &dx, &dy); unsigned long d = en_spawn(ET_ENTITY_DIP, 0, p[0] + dx, p[1] + dy, 0, 0, e, 0); if (d) { en_flag_clear(d, EF_FLAG_APPEAR); en_flag_set(d, EF_FLAG_FRIENDLY); en_set_state(d, 4); } }
}

static const enemy_t g_tab_g11b[] = { { 150, 39, ai_hunk, die_hunk, hurt_hunk, 0 } };
#define N_TAB_G11B ((int) (sizeof g_tab_g11b / sizeof g_tab_g11b[0]))
