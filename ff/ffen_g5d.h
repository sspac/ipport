/* ffen_g5d.h - Cellar monsters whose behaviour lives in main.lua (not in the erfly/ferrium/... files):
 *   Spooter 85.960 / Super Spooter 85.961 / Mega Spooter 85.963  (mod:spooterAI, main.lua 10420)
 *   Baby Spider 85.962 (mod:babySpiderAI / squishBabySpider / checkSpiderCollision, main.lua 10640-10700)
 *   Sticky Sack 30.960, Walking / Stumbling Sticky Sack 88.960 / 88.961 (main.lua 10557-10640)
 *   Honey Eye 140.0 (mod:honeyeyeAI, main.lua 13678) and the HoneyCreep projectile (ffscripts/callbacks/projectiles.lua 533)
 * The movement of Spooter / Baby Spider (the engine's own spider walk, State 4) and of the sacks (the Boil / Gaper-like state machine) stays with the engine, exactly as it does
 * under the Lua: the AI only adds what the Lua adds on top of it. */

/* ------------------------------------------------------------------------------------------ a projectile whose landing leaves something (ProjectileCallbacks Death)
 * The game has no hook for a projectile dying here: the ones asked for are remembered and, once one is gone from the room, its last position gets the effect. */
#define PW_N 32
static struct { unsigned long p, owner; int kind; float x, y, a, b; } g_pw[PW_N];
static void en_proj_watch2(unsigned long p, int kind, unsigned long owner, float a, float b) {
    if (!p) return;
    for (int k = 0; k < PW_N; k++) if (!g_pw[k].p) { g_pw[k].p = p; g_pw[k].kind = kind; g_pw[k].owner = owner; g_pw[k].a = a; g_pw[k].b = b; g_pw[k].x = fpos(p)[0]; g_pw[k].y = fpos(p)[1]; return; }
    ENLOG(70, 1, "ffenemy: not done: more than %d watched projectiles at once (the landing effect of the extra ones is dropped)", PW_N);
}
static void en_proj_watch(unsigned long p, int kind) { en_proj_watch2(p, kind, 0, 0, 0); }
static void en_coal_land(int kind, unsigned long owner, float x, float y, float a, float b);
/* ferrium/enemies/brood.lua broodProj / broodProjColl: where the egg lands a web grows (a free tile of the room), the hatch sound plays, and - when the spot is open - two
 * spiders are thrown (EntityNPC.ThrowSpider: they appear on the landing spot) and a white poof (1000.2.2) is left. */
static void brood_land(float x, float y) {
    if (en_in_room_walk(x, y, 0.0f) && !en_grid_ent(x, y)) en_web_at(x, y);
    en_sound(SOUND_BOIL_HATCH, 0.6f, (float) rint_(80, 120) / 100.0f);
    if (!en_xy(x, y, 0)) {
        for (int i = 0; i < 2; i++) {
            float rx, ry; en_rand_vec(&rx, &ry, (float) rint_(50, 100));
            float sx = x + rx, sy = y + ry; en_clamp_room(&sx, &sy, 10.0f); en_free_tile(&sx, &sy, 120.0f);
            unsigned long c = en_spawn(85, 0, sx, sy, 0, 0, 0, 0); if (c) en_flag_clear(c, EF_FLAG_APPEAR);
        }
        ENLOG(77, 1, "ffenemy: not done: Brood's thrown spiders are only thrown when a path to the player exists (HasPathToPos on a hidden pathfinder), and the white poof's colour");
    }
    en_spawn(1000, 2, x, y, 0, 0, 0, 2);
}
static void en_proj_tick(void) {
    for (int k = 0; k < PW_N; k++) {
        unsigned long p = g_pw[k].p; if (!p) continue;
        if (alive(p) && *(const int *) (p + E_TYPE) == 9) { g_pw[k].x = fpos(p)[0]; g_pw[k].y = fpos(p)[1]; continue; }
        { pj_t *pj = pj_find(p); if (pj) pj->p = 0; }                                  /* the address may come back as another projectile: forget its colour entry */
        g_pw[k].p = 0;
        if (g_pw[k].kind == PW_BROOD) brood_land(g_pw[k].x, g_pw[k].y);
        if (g_pw[k].kind >= 3) en_coal_land(g_pw[k].kind, g_pw[k].owner, g_pw[k].x, g_pw[k].y, g_pw[k].a, g_pw[k].b);
        if (g_pw[k].kind == PW_HONEY) {                                         /* projectiles.lua 534: a wide brown creep (scale 2 x 1.25, honey coloured, 90 frames longer) */
            unsigned long c = en_spawn(1000, EV_CREEP_BROWN, g_pw[k].x, g_pw[k].y, 0, 0, 0, 0);
            if (c) { en_scale(c, 2.0f, 1.25f); en_colorize(c, 5.5f, 3.5f, 1.0f, 1.0f); en_fx_add(c, FX_CREEPTINT, 0, 0); }
            ENLOG(71, 1, "ffenemy: not done: HoneyCreep's Timeout + 90 (the creep lasts as long as a plain brown one)");
        }
    }
}

/* ------------------------------------------------------------------------------------------ small shared helpers */
static int en_count_type_var(int type, int variant) {                          /* Isaac.CountEntities(nil, type, variant, 0) */
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); int c = 0;
    for (unsigned k = 0; k < n; k++) if (alive(snap[k]) && *(int *) (snap[k] + 0x38) == type && *(int *) (snap[k] + 0x3c) == variant) c++;
    return c;
}
static int en_count_type(int type) {                                           /* mod.GetEntityCount(type) */
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); int c = 0;
    for (unsigned k = 0; k < n; k++) if (alive(snap[k]) && *(int *) (snap[k] + 0x38) == type) c++;
    return c;
}
/* mod:FindRandomValidPathPosition(npc, 3): a free, reachable cell 30..80 from the npc (mode 3), otherwise any reachable free cell */
static void en_near_pos(unsigned long e, float *ox, float *oy) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9; const float *p = fpos(e);
    float nx[15 * 9 + 16], ny[15 * 9 + 16], ax[15 * 9 + 16], ay[15 * 9 + 16]; int nn = 0, na = 0;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
        float x = en_room_x(c), y = en_room_y(r);
        if (en_xy(x, y, 0) || !en_in_room_walk(x, y, 0.0f)) continue;
        if (!en_line_clear(p[0], p[1], x, y, 0)) continue;                      /* HasPathToPos: a clear line stands in for the path */
        float d = vlen(x - p[0], y - p[1]);
        if (na < (int) (sizeof ax / sizeof ax[0])) { ax[na] = x; ay[na] = y; na++; }
        if (d > 30.0f && d < 80.0f && nn < (int) (sizeof nx / sizeof nx[0])) { nx[nn] = x; ny[nn] = y; nn++; }
    }
    if (nn) { int k = (int) rnd((unsigned) nn); *ox = nx[k]; *oy = ny[k]; }
    else if (na) { int k = (int) rnd((unsigned) na); *ox = ax[k]; *oy = ay[k]; }
    else { *ox = p[0]; *oy = p[1]; }
}
/* mod:FindRandomFreePosAirNoGrids(pos, minr, maxr): a cell a flyer may be in, minr..maxr from pos */
static void en_air_pos(const float *pos, float minr, float maxr, float *ox, float *oy) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
    float fx[15 * 9 + 16], fy[15 * 9 + 16], ax[15 * 9 + 16], ay[15 * 9 + 16]; int nf = 0, na = 0;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
        float x = en_room_x(c), y = en_room_y(r);
        if (en_xy(x, y, 1) || !en_in_room_walk(x, y, 0.0f)) continue;
        if (na < (int) (sizeof ax / sizeof ax[0])) { ax[na] = x; ay[na] = y; na++; }
        float d = vlen(x - pos[0], y - pos[1]);
        if (d > minr && d < maxr && nf < (int) (sizeof fx / sizeof fx[0])) { fx[nf] = x; fy[nf] = y; nf++; }
    }
    if (nf) { int k = (int) rnd((unsigned) nf); *ox = fx[k]; *oy = fy[k]; }
    else if (na) { int k = (int) rnd((unsigned) na); *ox = ax[k]; *oy = ay[k]; }
    else { *ox = pos[0]; *oy = pos[1]; }
}

/* ------------------------------------------------------------------------------------------ Baby Spider 85.962
 * It dies to any hit and is SQUISHED when the player (or a friend) walks over it: the walker is only slowed by the engine's own collision. */
static void squish_baby(unsigned long e, int remove) {
    float *p = fpos(e);
    en_spawn(1000, 38, p[0], p[1], 0, 0, e, 7000);                              /* Baby Spider Explosion */
    en_sound(SOUND_DEATH_BURST_SMALL, 0.8f, 1.0f);
    ENLOG(72, 1, "ffenemy: not done: the Baby Spider's splat (MakeSplat 0.5 in squishBabySpider)");
    if (remove) en_remove(e);
}
static void ai_babyspider(en_t *s, unsigned long e) {
    if (!s->init) { s->init = 1; en_flag_set(e, EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_REWARD); }
    if (en_flag(e, EF_FLAG_FRIENDLY)) return;                                   /* checkSpiderCollision: not for a friendly spider */
    unsigned long pl = g_first_player ? g_first_player() : 0; if (!pl) return;
    const float *p = fpos(e), *q = fpos(pl), *v = fvel(pl);
    if (vlen(q[0] - p[0], q[1] - p[1]) > 21.0f) return;                         /* player radius 13 + spider 8: the collision callback's touch */
    if (en_player_has(236)) { en_remove(e); return; }                           /* COLLECTIBLE_E_COLI: the spiders just vanish */
    if (v[0] * v[0] + v[1] * v[1] > 1.0f) squish_baby(e, 1);                    /* a player who stands still does not squish it */
    ENLOG(73, 1, "ffenemy: not done: a friendly walker squishing Baby Spiders (only the player is checked: no collision callback)");
}
static void die_babyspider(en_t *s, unsigned long e) { (void) s; squish_baby(e, 0); }

/* ------------------------------------------------------------------------------------------ Spooter 85.960 / Super 85.961 / Mega 85.963
 * The engine walks it (State 4); i[0] = shot fired this attack. */
static void ai_spooter(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; ENLOG(74, 1, "ffenemy: not done: the Bursting Sack variant of Spooters (other sheet, harmless): ReplaceSpritesheet is not hooked"); }
    float speed = (s->variant == 961 || s->variant == 963) ? 7.0f : 5.0f;
    if (vlen(v[0], v[1]) > speed) vresize(&v[0], &v[1], speed);
    int st = en_get_state(e);
    if (en_event(s, "Attack", "Shoot")) {
        float x = t[0] - p[0], y = t[1] - p[1];
        en_sound(SOUND_BLOODSHOOT, 1.0f, 1.0f);
        vresize(&x, &y, 7.0f);
        if (s->variant == 960 || s->variant == 963) en_shoot(e, 0, p[0], p[1], x, y);
        if (s->variant == 961 || s->variant == 963) for (int i = 1; i <= 2; i++) { float rx, ry; vrot(x, y, (float) (-48 + i * 32), &rx, &ry); en_shoot(e, 0, p[0], p[1], rx, ry); }
    }
    if (st == 4 && rint_(1, 20) == 1 && vlen(t[0] - p[0], t[1] - p[1]) < 200.0f && en_line_clear(t[0], t[1], p[0], p[1], 1) && !en_scare_conf(e)) { en_set_state(e, 8); st = 8; }
    if (st == 8) {
        if (en_finished(s, "Attack")) en_set_state(e, 4);
        else if (!en_playing(s, "Attack")) { en_flip(e, t[0] < p[0]); en_play(s, "Attack"); }
    }
}

/* ------------------------------------------------------------------------------------------ Sticky Sack 30.960, Walking 88.960 / Stumbling 88.961 Sticky Sack
 * The sack throws 2-3 spiders (EntityNPC.ThrowSpider + Morph 85.962 in the Lua); ThrowSpider is not hooked, so the Baby Spiders appear on the landing spot.
 * i[0] = the throw of this attack was done, i[1] = the check at frame 0 was done. When it dies it bursts into 4-10 Baby Spiders. */
static void sack_throw(unsigned long e) {
    float *p = fpos(e);
    for (int i = rint_(2, 3); i > 0; i--) {
        float tx, ty; en_near_pos(e, &tx, &ty);
        float rx, ry; en_rand_vec(&rx, &ry, (float) rint_(0, 20));
        en_sound(FFSND_Plorp, 0.9f, (float) rint_(95, 105) / 100.0f);
        if (en_flag(e, EF_FLAG_FRIENDLY)) { ENLOG(75, 1, "ffenemy: not done: a friendly sack's Blue Baby Spider familiars (the spiders stay hostile Baby Spiders)"); }
        unsigned long c = en_spawn(85, 962, tx + rx, ty + ry, 0, 0, e, 0);
        if (c) en_flag_clear(c, EF_FLAG_APPEAR);
    }
    ENLOG(76, 1, "ffenemy: not done: the sack's spiders are thrown in an arc (EntityNPC.ThrowSpider): they appear on the landing spot");
    (void) p;
}
static void sack_burst(unsigned long e) {
    float *p = fpos(e);
    for (int i = rint_(4, 10); i > 0; i--) {                                    /* mod.spawnent(npc, pos, RandomVector()*math.random(20), 85, 962) */
        float x, y; en_rand_vec(&x, &y, (float) rint_(1, 20));
        unsigned long c = en_spawn(85, 962, p[0], p[1], x, y, e, 0);
        if (c) en_flag_clear(c, EF_FLAG_APPEAR);
    }
}
static void ai_stickysack(en_t *s, unsigned long e) {
    int st = en_get_state(e);
    if (st == 3) {
        s->cd++;                                                                /* npc.StateFrame (the engine counts it too: the Lua adds one more each frame) */
        s->i[0] = s->i[1] = 0;
        if (fb_hp(e) == *(const float *) (e + 0x358) && s->cd > 30 && en_count_type(85) < 40) { en_set_state(e, 8); en_set_statef(e, 0); s->cd = 0; }
    } else if (st == 8) {
        int fr = (int) *(const float *) (e + E_SPR_FRAME);
        if (fr == 0 && !s->i[1]) {
            s->i[1] = 1;
            if (en_count_type(85) > 40) { en_set_state(e, 3); en_replay(s, "Shake"); }
        }
        if (fr >= 10 && !s->i[0]) { s->i[0] = 1; sack_throw(e); }
    } else { s->i[0] = s->i[1] = 0; }
}
static void die_stickysack(en_t *s, unsigned long e) { (void) s; sack_burst(e); }

/* Walking / Stumbling: the throw is at frame 10 of the "Shoot" animation; variant 961 (Stumbling) starts a throw 1 frame in 15 when there are < 40 spiders. */
static void ai_walkingsack(en_t *s, unsigned long e) {
    int st = en_get_state(e);
    if (st == STATE_ATTACK) {                                                   /* sprite:IsPlaying("Shoot"): the engine's attack */
        if ((int) *(const float *) (e + E_SPR_FRAME) >= 10 && !s->i[0]) { s->i[0] = 1; sack_throw(e); }
    } else {
        s->i[0] = 0;
        if (s->variant == 961 && s->age % 15 == (int) rnd(15) + 1 && !en_flag(e, EF_FLAG_FRIENDLY) && en_count_type(85) < 40) en_set_state(e, STATE_ATTACK);
    }
}
static void die_walkingsack(en_t *s, unsigned long e) { (void) s; sack_burst(e); }

/* ------------------------------------------------------------------------------------------ Honey Eye 140.0 (main.lua 13678)
 * state 0 move / 1 attack; cd = StateFrame; f[0],f[1] = targetvel; i[0] = Run */
static void ai_honeyeye(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) {
        s->init = 1; s->state = 0;
        float x, y; en_air_pos(t, 50.0f, 200.0f, &x, &y);
        s->f[0] = x - p[0]; s->f[1] = y - p[1]; vresize(&s->f[0], &s->f[1], 3.0f);
    } else s->cd++;
    if (s->state == 0) {
        en_play(s, "Fly");
        float dist = vlen(t[0] - p[0], t[1] - p[1]);
        if (dist <= 50.0f && !en_confused(e)) { s->f[0] = t[0] - p[0]; s->f[1] = t[1] - p[1]; vresize(&s->f[0], &s->f[1], -6.0f); s->i[0] = 1; }
        else if (s->cd % 30 == 0 || s->i[0]) {
            float x, y; en_air_pos(t, 50.0f, 200.0f, &x, &y);
            s->f[0] = x - p[0]; s->f[1] = y - p[1]; vresize(&s->f[0], &s->f[1], 4.5f); s->i[0] = 0;
        }
        v[0] = lerpf(v[0], s->f[0], 0.075f); v[1] = lerpf(v[1], s->f[1], 0.075f);
        if (dist <= 285.0f && s->cd >= 85 && !en_scare_conf(e)) s->state = 1;
    } else {
        v[0] *= 0.95f; v[1] *= 0.95f;
        en_play(s, "Shoot");
        if (en_event(s, "Shoot", "Shoot")) {
            float x = (t[0] - p[0]) * 0.05f, y = (t[1] - p[1]) * 0.05f;
            if (vlen(x, y) > 10.0f) vresize(&x, &y, 10.0f);
            en_sound(SOUND_BLOODSHOOT, 1.0f, 1.0f);
            en_pp_t pp; memset(&pp, 0, sizeof pp);
            pp.has_fspeed = 1; pp.fspeed = -25.0f; pp.has_faccel = 1; pp.faccel = 1.5f; pp.has_scale = 1; pp.scale = 2.0f;
            unsigned long pr = en_shoot_p(e, p[0], p[1], x, y, &pp);
            if (pr) pj_add(pr, e, PJ_HONEY, 0);
            en_proj_watch(pr, PW_HONEY);
        }
        if (en_finished(s, "Shoot")) { s->state = 0; s->cd = 0; }
    }
}

/* ------------------------------------------------------------------------------------------ Sackboy 750.80 (xalum/enemies/sackboy.lua), what Ransacked hatches
 * Walks to the player with the head overlay; now and then plays the overlay "Shoot" and at its frame 18 throws spiders (vanilla spiders 85.0 while they are < 4 per Sackboy,
 * then Baby Spiders 85.962: the Lua's babyreplace). Dying throws more. EntityNPC.ThrowSpider is not hooked: they appear on the landing spot. */
static void sackboy_throw(unsigned long e, int type, int variant, int n) {
    float *p = fpos(e);
    for (int i = 0; i < n; i++) {
        float x = p[0] + (float) rint_(-40, 40), y = p[1] + (float) rint_(-40, 40);
        en_clamp_room(&x, &y, 10.0f);
        unsigned long c = en_spawn(type, variant, x, y, 0, 0, e, 0);
        if (c) en_flag_clear(c, EF_FLAG_APPEAR);
    }
}
static void ai_sackboy(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; en_overlay(s, "Head"); }
    en_walk_anim(s, e, "WalkHori", "WalkVert", 0.3f);
    if (en_overlay_finished(s, "Shoot")) en_overlay(s, "Head");
    if (!strcmp(s->ov, "Head")) {
        float tp[2]; en_confuse_pos(s, e, t, tp);
        if (en_scare_conf(e) || (en_line(p, t, 0) && !en_collides_grid(e))) {
            float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, en_scared(e) ? -0.5f : 0.5f);
            v[0] = v[0] * 0.8f + x; v[1] = v[1] * 0.8f + y;
        } else {
            en_walk_to(e, t, 0.0f, 3.0f, 0);                                      /* Pathfinder:FindGridPath(target, 3, 1, false) */
            v[0] *= 1.2f; v[1] *= 1.2f;
            if (vlen(v[0], v[1]) > 4.0f) vresize(&v[0], &v[1], 4.0f);
        }
        if (s->age % 8 == 0 && rint_(1, 20) == rint_(1, 20) && !en_scare_conf(e)) en_overlay(s, "Shoot");
    } else { v[0] *= 0.9f; v[1] *= 0.9f; }
    if (!strcmp(s->ov, "Shoot") && s->ovf == 18) {
        en_sound(SOUND_BOIL_HATCH, 0.85f, 1.0f); en_sound(SOUND_WHEEZY_COUGH, 0.5f, 0.7f);
        int sackboys = en_count_type_var(750, 80), spiders = en_count_type_var(85, 0);
        int few = spiders < 4 * sackboys;
        if (few) sackboy_throw(e, 85, 0, (int) ceilf(frand() * 2.0f));
        if (!en_flag(e, EF_FLAG_FRIENDLY) || en_count_type_var(85, 962) < 5) sackboy_throw(e, 85, 962, 4 + rint_(1, 3) + (few ? 0 : 3));
    }
}
static void die_sackboy(en_t *s, unsigned long e) {
    (void) s;
    en_sound(SOUND_BOIL_HATCH, 0.85f, 1.0f);
    sackboy_throw(e, 85, 0, (int) floorf(frand() * 3.0f));
    sackboy_throw(e, 85, 962, 7 + rint_(1, 5));
}

static const enemy_t g_tab_g5d[] = {
    { 85, 960, ai_spooter, 0, 0, 0 },
    { 85, 961, ai_spooter, 0, 0, 0 },
    { 85, 963, ai_spooter, 0, 0, 0 },
    { 85, 962, ai_babyspider, die_babyspider, 0, 0 },
    { 30, 960, ai_stickysack, die_stickysack, 0, 0 },
    { 88, 960, ai_walkingsack, die_walkingsack, 0, 0 },
    { 88, 961, ai_walkingsack, die_walkingsack, 0, 0 },
    { 140, 0, ai_honeyeye, 0, 0, 0 },
    { 750, 80, ai_sackboy, die_sackboy, 0, 0 },
};
#define N_TAB_G5D ((int) (sizeof g_tab_g5d / sizeof g_tab_g5d[0]))
