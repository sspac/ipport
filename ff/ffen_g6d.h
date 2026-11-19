/* ffen_g6d.h - Burning Basement, part D: S'More 151.5, Septic 151.6 (the Slammer's fire / slime kin: erfly/enemies/slammer.lua), Wick 750.110 (xalum/enemies/wick.lua),
 * Brisket 114.33 (ferrium/enemies/brisket.lua). Included by ffenemy.h after ffen_g6c.h. s->cd = StateFrame. */

/* ------------------------------------------------------------------------------------------ small pieces */
/* mod:FindRandomFreePos(npc, radius, nil, true): a free tile centre farther than `radius` from the npc and more than 60 from where the player will be in 3 frames */
static void g6d_free_pos(unsigned long e, float radius, float *ox, float *oy) {
    const float *p = fpos(e), *t = en_target(e), *tv = en_target_vel(); static float fx[448], fy[448], ax[448], ay[448]; int nf = 0, na = 0;
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
        float x = en_room_x(c), y = en_room_y(r);
        if (en_xy(x, y, 0) || !en_in_room_walk(x, y, 0)) continue;
        if (t && vlen(t[0] + (tv ? tv[0] : 0) * 3.0f - x, t[1] + (tv ? tv[1] : 0) * 3.0f - y) <= 60.0f) continue;
        ax[na] = x; ay[na] = y; na++;
        if (vlen(x - p[0], y - p[1]) > radius) { fx[nf] = x; fy[nf] = y; nf++; }
    }
    if (nf) { int k = (int) rnd((unsigned) nf); *ox = fx[k]; *oy = fy[k]; }
    else if (na) { int k = (int) rnd((unsigned) na); *ox = ax[k]; *oy = ay[k]; }
    else { *ox = p[0]; *oy = p[1]; }
}
/* mod:FindRandomValidPathPosition(npc, 3, nil, nearposses): a reachable free cell 30..nearposses from the npc (any reachable one when none) */
static void g6d_near_pos(unsigned long e, float nearr, float *ox, float *oy) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9; const float *p = fpos(e);
    float nx[15 * 9 + 16], ny[15 * 9 + 16], ax[15 * 9 + 16], ay[15 * 9 + 16]; int nn = 0, na = 0;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
        float x = en_room_x(c), y = en_room_y(r);
        if (en_xy(x, y, 0) || !en_in_room_walk(x, y, 0.0f) || !en_line_clear(p[0], p[1], x, y, 0)) continue;
        float d = vlen(x - p[0], y - p[1]);
        if (na < (int) (sizeof ax / sizeof ax[0])) { ax[na] = x; ay[na] = y; na++; }
        if (d > 30.0f && d < nearr && nn < (int) (sizeof nx / sizeof nx[0])) { nx[nn] = x; ny[nn] = y; nn++; }
    }
    if (nn) { int k = (int) rnd((unsigned) nn); *ox = nx[k]; *oy = ny[k]; }
    else if (na) { int k = (int) rnd((unsigned) na); *ox = ax[k]; *oy = ay[k]; }
    else { *ox = p[0]; *oy = p[1]; }
}
/* the player behind a hit's source (mod:GetPlayerSource): the source itself, or what shot/threw it */
static unsigned long g6d_player_src(unsigned long src) {
    if (!src || !g_readable((const void *) src, 0x410)) return 0;
    if (*(const int *) (src + E_TYPE) == 1) return src;
    unsigned long sp = *(const unsigned long *) (src + 0x400);
    if (sp && g_readable((const void *) sp, 0x40) && *(const int *) (sp + E_TYPE) == 1) return sp;
    return 0;
}

/* ------------------------------------------------------------------------------------------ effects that follow a monster (EntityEffect:FollowParent) and the white smoke's own animation
 * kind 0: just follows (the engine animates it); kind 1: FF White Smoke 1000.1960.12 (erfly/main.lua FFWhiteSmokeAI: plays "Fade"..4/5 once, then Remove) */
typedef struct { unsigned long fx, owner; float dy; int kind, age, len, anim; } fol_t;
static fol_t g_fol[24];
static void g6d_follow(unsigned long fx, unsigned long owner, float dy, int kind, int len, int anim) {
    if (!fx) return;
    for (int k = 0; k < 24; k++) if (!g_fol[k].fx || !alive(g_fol[k].fx)) { fol_t *f = &g_fol[k]; f->fx = fx; f->owner = owner; f->dy = dy; f->kind = kind; f->age = 0; f->len = len; f->anim = anim; return; }
    if (kind == 1) en_remove(fx);
}
/* Wick's shot: the white smoke and the wax coloured burst that stay on it */
static void g6d_wick_effects(unsigned long e) {
    const float *p = fpos(e);
    unsigned long sm = en_spawn(1000, 1960, p[0], p[1] - 25.0f, 0, 0, 0, 12);
    if (sm) {
        int an = rint_(4, 5); if (rint_(1, 2) == 1) en_flip(sm, 1);
        char nm[8]; snprintf(nm, sizeof nm, "Fade%d", an); if (g_play) g_play((void *) (sm + E_SPRITE), nm, 1);
        en_paint(sm, 0.5f, 0.5f, 0.5f, 1.0f, 0, 0, 0, 0, 0, 0, 0);
        g6d_follow(sm, e, -25.0f, 1, an == 4 ? 21 : 16, an);
    }
    unsigned long bl = en_spawn(1000, 2, p[0], p[1], 0, 0, e, 1);
    if (bl) { en_spr_off(bl, 0.0f, -15.0f); en_paint(bl, 1, 1, 1, 1, 0, 0, 0, 0, 0.8f, 0.7f, 0.5f); g6d_follow(bl, e, 0.0f, 0, 0, 0); }
}

/* ------------------------------------------------------------------------------------------ S'More's ash trail (mod.SpawnAsh + mod:AshCreepAI, main.lua 9026)
 * 1000.26.7000: every ash knows the one laid before it; when the last one is told to `asplode` it lights a fire (1000.7005) and tells the previous one, so the whole trail
 * burns back to where the jump began, one ash per frame. */
typedef struct { unsigned long e, parent, spawner; int asplode, spawned, sf; } ash_t;
#define ASH_N 96
static ash_t g_ash[ASH_N];
static ash_t *g6d_ash_of(unsigned long e) { if (!e) return 0; for (int k = 0; k < ASH_N; k++) if (g_ash[k].e == e) return &g_ash[k]; return 0; }
static unsigned long g6d_spawn_ash(unsigned long npc, float x, float y, unsigned long prev) {
    unsigned long a = en_spawn(1000, EV_CREEP_BLACK, x, y, 0, 0, npc, 7000); if (!a) return 0;
    if (g_load) { char nm[16]; g_load((void *) (a + E_SPRITE), "gfx/effects/1000.092_creep (ash).anm2"); snprintf(nm, sizeof nm, "SmallBlood0%d", rint_(1, 6)); if (g_play) g_play((void *) (a + E_SPRITE), nm, 1); }
    else ENLOG(260, 1, "ffenemy: not done: Sprite::Load is not installed: S'More's ash trail has the plain creep sprite");
    for (int k = 0; k < ASH_N; k++) if (!g_ash[k].e || !alive(g_ash[k].e)) { ash_t *s = &g_ash[k]; memset(s, 0, sizeof *s); s->e = a; s->parent = prev; s->spawner = npc; return a; }
    ENLOG(261, 1, "ffenemy: not done: more than %d ash creeps of S'More at once (the extra one does not burn with the trail)", ASH_N);
    return a;
}

/* ------------------------------------------------------------------------------------------ S'More 151.5 / Septic 151.6 (slammer.lua, the var 5 / 6 branches)
 * state 0 idle (State 3) / 1 jump (8) / 2 fall (9); i[0] = IsJumping, i[1] = Wait, i[2] = waitrng, i[3] = JumpTimer, i[4] = AshLeave, i[5] = LastAshSpawn set, f[0..1] = PlayerPos,
 * f[2..3] = targetVelocity, f[4..5] = LastAshSpawn; s->ref = the last ash of d.AshTable (0 = the table is empty) */
static void ai_smore(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return; int var = s->variant;
    s->key = (151u << 20) | ((unsigned) var << 4);
    if (!s->init) ENLOG(262, 1, "ffenemy: not done: S'More/Septic as a friend (State 3 at room start), as a waiting spawn (makeWaitFerr), Friction = 1 while jumping and the water-grid extinguishing of S'More (unignited)");
    if (s->i[4]) {                                                                              /* npcd.AshLeave: the trail */
        float dl = s->i[5] ? vlen(p[0] - s->f[4], p[1] - s->f[5]) : 0.0f;
        if (!s->i[5] || dl > 10.0f) {
            if (var == 5) { unsigned long prev = en_parent_alive(s->ref) ? s->ref : (s->ref && g6d_ash_of(s->ref) ? s->ref : 0); s->ref = g6d_spawn_ash(e, p[0], p[1], prev); }
            else { unsigned long c = en_spawn(1000, EV_CREEP_GREEN, p[0], p[1], 0, 0, e, 0); if (c) en_set_timeout(c, 20); }
            s->f[4] = p[0]; s->f[5] = p[1]; s->i[5] = 1;
        }
    }
    if (s->i[0] == 1) {                                                                         /* npcd.IsJumping == 1 */
        s->i[3]++;
        float dx = s->f[0] - p[0], dy = s->f[1] - p[1], dist = vlen(dx, dy);
        if (dist < 20.0f || s->i[3] > 60) { s->state = 2; s->i[0] = 0; s->f[2] = s->f[3] = 0; }
        else if (dist < 40.0f) { float x = dx, y = dy; vresize(&x, &y, 2.0f); s->f[2] = x; s->f[3] = y; }
        else {
            float nx = dx, ny = dy; vresize(&nx, &ny, 1.0f); float k = var == 5 ? dist / 65.0f : dist / 30.0f;
            s->f[2] += nx * k; s->f[3] += ny * k;
            if (vlen(s->f[2], s->f[3]) >= dist) vresize(&s->f[2], &s->f[3], dist);
        }
    } else { s->f[2] = s->f[3] = 0; s->i[3] = 0; }
    if (!s->init) { s->init = 1; s->i[1] = 0; s->state = 0; }
    else if (s->state == 0) {
        en_set_gridcoll(e, GRIDCOLL_GROUND);
        if (s->i[1] == 0) { en_play(s, "Idle"); s->i[2] = rint_(5, 20) + 20; }
        s->i[1]++;
        if (s->i[1] > 10 + s->i[2] && !en_flag(e, EF_FLAG_CONFUSION | EF_FLAG_FEAR | EF_FLAG_SHRINK)) { s->state = 1; s->i[1] = 0; }
    } else if (s->state == 1) {
        if (en_finished(s, "Jump")) { /* the jump is ended by the distance check above */ }
        else if (!en_playing(s, "Jump")) en_play(s, "Jump");
        else if (en_event(s, "Jump", "GetPlayer")) {
            float rx, ry; g6d_free_pos(e, 200.0f, &rx, &ry);
            float dx = rx - p[0], dy = ry - p[1], d = vlen(dx, dy); vresize(&dx, &dy, d < 300.0f ? d : 300.0f);
            float x = p[0] + dx, y = p[1] + dy; en_free_tile_pos(&x, &y);
            float ux, uy; en_rand_vec(&ux, &uy, 1.0f); s->f[0] = x + ux; s->f[1] = y + uy;
            s->i[4] = 1; s->i[5] = 0; s->ref = 0; s->i[0] = 1;
            en_set_collision(e, ENTCOLL_NONE); en_set_gridcoll(e, GRIDCOLL_NONE); en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK);
        } else if (en_event(s, "Jump", "Sound")) en_sound(SOUND_MEAT_JUMPS, 1.0f, 1.0f);
    } else {
        if (en_finished(s, "Fall")) s->state = 0;
        else if (!en_playing(s, "Fall")) en_play(s, "Fall");
        else if (en_event(s, "Fall", "Blaze")) {                                                /* the last ash lights: the trail burns backwards */
            s->i[4] = 0; ash_t *a = g6d_ash_of(s->ref); if (a) a->asplode = 1;
        }
        if (en_event(s, "Fall", "Land")) {
            en_sound(SOUND_MEAT_IMPACTS, 1.0f, 1.0f);
            if (var == 5) { unsigned long sm = en_smoke(e, p[0], p[1], 0, 0, -10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); if (sm) en_scale(sm, 2.0f, 2.0f); }
            en_set_collision(e, ENTCOLL_ALL); en_set_gridcoll(e, GRIDCOLL_GROUND); en_flag_clear(e, EF_FLAG_NO_PHYSICS_KNOCKBACK);
        }
    }
    v[0] = s->f[2] * 0.3f + v[0] * 0.6f; v[1] = s->f[3] * 0.3f + v[1] * 0.6f;
}
static void hurt_smore(en_t *s, unsigned long e, float dmg) {                                   /* slammerHurt: S'More is not hurt by fire from monsters */
    (void) e; (void) dmg; s->hurt_block = (s->variant == 5 && en_fire_from_not_player()) ? 1 : 0;
}

/* ------------------------------------------------------------------------------------------ Wick 750.110 (wick.lua)
 * state 0 idle / 1 shoot; cd = StateFrame; i[0] = d.Shooted; f[0] = d.FlameBuffer */
static void ai_wick(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t0 = en_target(e), tp[2]; if (!t0) return;
    en_confuse_pos(s, e, t0, tp);
    if (!s->init) { s->init = 1; s->state = 0; ENLOG(263, 1, "ffenemy: not done: Wick's splat colour (SplatColor wax) and the grid-path marker (QuickSetEntityGridPath)"); } else s->cd++;
    en_walk_anim(s, e, "WalkHori", "WalkVert", 0.3f);
    if (en_scare(e)) {
        float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -6.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
    } else if (en_line_clear(p[0], p[1], tp[0], tp[1], 0)) {
        float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 3.5f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
    } else en_walk_to(e, tp, 0.45f, 0.45f, 0);
    if (s->cd % 8 == 0) {
        unsigned long b = en_spawn(1000, EV_BLOOD_SPLAT, p[0], p[1], 0, 0, e, 0);
        if (b) { en_scale(b, 0.5f, 0.5f); en_paint(b, 1, 1, 1, 1, 0, 0, 0, 0, 0.8f, 0.7f, 0.5f); }
    }
    if (s->state == 0) {
        en_overlay(s, "Head");
        if (s->cd > 90 && rint_(1, 10) == 1 && en_line_clear(p[0], p[1], tp[0], tp[1], 1) && !en_flag(e, EF_FLAG_CONFUSION | EF_FLAG_FEAR | EF_FLAG_SHRINK)) s->state = 1;
    } else {
        en_overlay(s, "Shoot");
        if (en_overlay_finished(s, "Shoot")) { s->state = 0; s->i[0] = 0; s->cd = 0; }
        if (s->ovf == 12 && !s->i[0]) {
            float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 10.0f);
            unsigned long fl = en_spawn(33, 10, p[0], p[1], x, y, e, 0);
            if (fl) en_set_gridcoll(fl, GRIDCOLL_NOPITS);
            en_sound(SOUND_MONSTER_GRUNT_0, 0.8f, 1.2f);
            g6d_wick_effects(e);
            s->i[0] = 1;
        }
    }
}
static void hurt_wick(en_t *s, unsigned long e, float dmg) {
    s->hurt_block = 0;
    if (g_hit_flags & DMG_FIRE) { s->hurt_block = 1; return; }                                  /* any fire: no damage */
    s->f[0] += dmg;
    if (s->f[0] > 2.5f) {                                                                       /* every 2.5 damage taken the Wick loses a flame (a fireplace 33.10) */
        float vx = 0, vy = 0; unsigned long pl = g6d_player_src(g_hit_src);
        const float *p = fpos(e);
        if (pl) { vx = p[0] - fpos(pl)[0]; vy = p[1] - fpos(pl)[1]; }
        if (vx != 0.0f || vy != 0.0f) { vresize(&vx, &vy, (float) rint_(1, 3)); float rx, ry; vrot(vx, vy, (float) (50 + rint_(-40, 40)), &rx, &ry); vx = rx; vy = ry; }
        unsigned long fl = en_spawn(33, 10, p[0], p[1], vx, vy, e, 0);
        if (fl) {
            if (g_hp_off) { if (g6c_hp(e) - dmg <= 0.0f) *(float *) (fl + g_hp_off) /= 1.2f; else *(float *) (fl + g_hp_off) /= 2.0f; }
            en_set_gridcoll(fl, GRIDCOLL_NOPITS);
        }
        en_sound(SOUND_FIREDEATH_HISS, 0.18f, 2.0f);
        s->f[0] = 0;
    }
}

/* ------------------------------------------------------------------------------------------ Brisket 114.33 (brisket.lua)
 * state 0 Idle / 1 Move / 2 Attack / 3 Appear; cd = StateFrame; i[0] = d.sprite (0 or 1), i[1] = d.attackSoon, i[2] = d.attackLimit, i[3] = d.startMoving (1 true),
 * f[0..1] = d.targetPosition, f[2] = d.rotation (0 or 45) */
typedef struct { float x, y, rot; int fc, barred[4]; } bp_t;
static bp_t g_bp[8];
static void g6d_brisket_dust(unsigned long e) {                                                 /* the smoke and the dust it leaves when it dies */
    const float *p = fpos(e);
    for (int i = -30; i <= 30; i += 30) { float rx, ry; vrot(0, -10.0f, (float) i, &rx, &ry); unsigned long sm = en_smoke(e, p[0], p[1], rx, ry, -5.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); (void) sm; }
}
static void ai_brisket(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    s->key &= ~15u;                                                                             /* the sub 1 / 2 Brisket use the sub 0 animations */
    if (!s->init) {
        s->init = 1; ENLOG(264, 1, "ffenemy: not done: Brisket's splat colour (SplatColor) and customProjSplat (the Lua never reads it either)");
        if (s->sub == 1) { s->i[0] = 1; en_flag_clear(e, EF_FLAG_APPEAR); en_visible(e, 0); s->f[2] = 45.0f; en_spawn(1000, 15, p[0], p[1], 0, 0, e, 0); s->state = 3; }
        else if (s->sub == 2) {
            s->i[0] = (int) rnd(2);
            if (s->i[0] == 0) { s->state = 0; s->f[2] = 0; }
            else { s->state = 3; en_visible(e, 0); en_flag_clear(e, EF_FLAG_APPEAR); s->f[2] = 45.0f; en_spawn(1000, 15, p[0], p[1], 0, 0, e, 0); }
        } else { s->i[0] = 0; s->state = 0; s->f[2] = 0; }
        s->i[1] = 0; s->i[2] = 0;
    } else { s->cd++; s->i[2]++; }
    char an[16];
    if (s->state == 0) {
        if (s->i[1] == 1 || (s->i[2] > 100 && !en_flag(e, EF_FLAG_CONFUSION | EF_FLAG_FEAR | EF_FLAG_SHRINK))) s->state = 2;
        else if (s->cd > 45) { s->state = 1; s->i[3] = 0; }
        else { snprintf(an, sizeof an, "Idle%d", s->i[0]); en_play(s, an); }
        v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f);
    } else if (s->state == 1) {
        snprintf(an, sizeof an, "Move%d", s->i[0]);
        if (en_finished(s, an)) { s->state = 0; s->cd = (int) rnd(25) - 10; s->i[1] = (int) rnd(4); }
        else if (en_event(s, an, "Move")) {
            g6d_near_pos(e, 100.0f, &s->f[0], &s->f[1]); s->i[3] = 1; en_flip(e, !(s->f[0] - p[0] > 0));
        } else if (en_event(s, an, "Shoot")) s->i[3] = 0;
        else en_play(s, an);
        if (s->i[3] == 0) { v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f); }
        else {
            float x, y;
            if (en_scare(e)) { x = t[0] - p[0]; y = t[1] - p[1]; vresize(&x, &y, -4.0f); }
            else { x = s->f[0] - p[0]; y = s->f[1] - p[1]; vresize(&x, &y, 4.0f); }
            v[0] = lerpf(v[0], x, 0.4f); v[1] = lerpf(v[1], y, 0.4f);
            if (s->age % 3 == 0) { unsigned long b = en_spawn(1000, EV_BLOOD_SPLAT, p[0], p[1], 0, 0, e, 0); if (b) en_paint(b, 0, 0, 0, 0.75f, 0, 0, 0, 0, 0.1f, 0.1f, 0.1f); }
        }
    } else if (s->state == 2) {
        snprintf(an, sizeof an, "Attack%d", s->i[0]);
        if (en_finished(s, an)) { s->state = 0; s->i[1] = (int) rnd(6); s->i[2] = 0; }
        else if (en_event(s, an, "Shoot")) {
            en_sound(SOUND_ANIMAL_SQUISH, 1.0f, (float) rint_(75, 90) / 100.0f);
            en_pp_t pp; memset(&pp, 0, sizeof pp); pp.has_fspeed_mod = 1; pp.fspeed_mod = -0.1f; pp.has_faccel_mod = 1; pp.faccel_mod = -0.165f; pp.has_height_mod = 1; pp.height_mod = 15.0f; pp.has_scale = 1; pp.scale = 1.4f;
            for (int i = 90; i <= 360; i += 90) {
                float rx, ry; vrot(0, 2.2f, (float) i + s->f[2], &rx, &ry);
                unsigned long pr = en_shoot_p(e, p[0], p[1], rx, ry, &pp); if (pr) en_sheet(pr, 0, "gfx/projectiles/brisket_tear.png");     /* the shots of this frame get the brisket sheet */
            }
            unsigned long sp = en_spawn(1000, 2, p[0], p[1], 0, 0, e, 960); if (sp) en_paint(sp, 0, 0, 0, 0.75f, 0, 0, 0, 0, 0.1f, 0.1f, 0.1f);
            for (int i = -30; i <= 30; i += 30) {
                float rx, ry; vrot(0, -4.5f, (float) i, &rx, &ry);
                unsigned long sm = en_smoke(e, p[0], p[1], rx, ry, -15.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); if (sm) en_set_timeout(sm, 20);
            }
        } else en_play(s, an);
        v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f);
    } else {
        if (s->age > 1 && !*(const unsigned char *) (e + E_VISIBLE)) en_visible(e, 1);
        if (en_finished(s, "Appear1")) s->state = 0; else en_play(s, "Appear1");
        v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f);
    }
}
static void die_brisket(en_t *s, unsigned long e) {
    en_sound(SOUND_BLACK_POOF, 0.4f, 1.6f); g6d_brisket_dust(e);
    for (int k = 0; k < 8; k++) if (!g_bp[k].fc && !g_bp[k].rot && !g_bp[k].x && !g_bp[k].y) {
        const float *p = fpos(e); g_bp[k].x = p[0]; g_bp[k].y = p[1]; g_bp[k].rot = s->f[2]; g_bp[k].fc = 1; memset(g_bp[k].barred, 0, sizeof g_bp[k].barred); return;       /* fc starts at 1: 0 = free slot, the first step is fc 0 */
    }
}
/* mod:spreadBrisketSeasoning: every frame, in the four directions, a gunpowder every 20 px (every 9 frames) until a wall or 120 frames */
static void g6d_brisket_step(bp_t *b) {
    int fc = b->fc - 1, total = 0;
    for (int i = 1; i <= 4; i++) {
        if (b->barred[i - 1]) { total++; continue; }
        float vx, vy; vrot(0, 20.0f, (float) (i * 90) + b->rot, &vx, &vy); float k = (float) fc / 9.0f, px = b->x + vx * k, py = b->y + vy * k;
        if (!en_xy(px, py, 0)) {
            if (fc % 9 == 0) {
                unsigned long a = en_gunpowder(0, px, py, 140, 45, -1.0f, 0);
                if (a) { float sc = (130.0f - (float) fc) / 100.0f; if (sc < 0.7f) sc = 0.7f; en_scale(a, sc, sc); }
                float sx, sy; en_rand_vec(&sx, &sy, 2.0f);
                unsigned long sm = en_smoke(0, px, py, sx, sy, 0.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); if (sm) en_set_timeout(sm, 40);
            }
        } else b->barred[i - 1] = 1;
    }
    b->fc++;
    if (b->fc - 1 > 120 || total == 4 || en_room_clear()) { b->fc = 0; b->x = b->y = b->rot = 0; }
}

/* ------------------------------------------------------------------------------------------ the per-frame pieces of this group (called from en_g6_tick) */
static void en_g6d_tick(void) {
    for (int k = 0; k < ASH_N; k++) {
        ash_t *a = &g_ash[k]; if (!a->e) continue;
        if (!alive(a->e)) { a->e = 0; continue; }
        if (!a->asplode) continue;
        const float *p = fpos(a->e);
        if (!a->spawned) {
            unsigned long sp = a->spawner && alive(a->spawner) ? a->spawner : 0;
            fire_t *f = en_fire_new(sp, p[0], p[1], 0, 0, 0); if (f) { f->parent = sp; en_fire_go(f); en_set_collision(f->e, ENTCOLL_PLAYEROBJECTS); }
            ash_t *par = g6d_ash_of(a->parent); if (par && par->e && alive(par->e)) par->asplode = 1;
            a->spawned = 1; a->sf = 0;
        } else {
            a->sf++;
            if (a->sf > 30) { en_remove(a->e); a->e = 0; }
            else if (a->sf > 20) { float sc = 1.0f - (float) (a->sf - 20) / 10.0f; en_scale(a->e, sc, sc); }
        }
    }
    for (int k = 0; k < 24; k++) {
        fol_t *f = &g_fol[k]; if (!f->fx) continue;
        if (!alive(f->fx)) { f->fx = 0; continue; }
        if (!f->owner || !alive(f->owner)) { if (f->kind == 1) en_remove(f->fx); f->fx = 0; continue; }
        float *fp = fpos(f->fx); const float *op = fpos(f->owner); fp[0] = op[0]; fp[1] = op[1] + f->dy; fvel(f->fx)[0] = 0; fvel(f->fx)[1] = 0;
        if (f->kind == 1) {
            char nm[8]; snprintf(nm, sizeof nm, "Fade%d", f->anim);
            if (f->age >= f->len) { en_remove(f->fx); f->fx = 0; continue; }
            if (g_setframe) g_setframe((void *) (f->fx + E_SPRITE), nm, f->age);
        } else if (f->age > 40) { f->fx = 0; continue; }
        f->age++;
    }
    for (int k = 0; k < 8; k++) if (g_bp[k].fc) g6d_brisket_step(&g_bp[k]);
}

static const enemy_t g_tab_g6d[] = {
    { 151, 5, ai_smore, 0, hurt_smore, 0, 0, 3 },
    { 151, 6, ai_smore, 0, 0, 0, 0, 3 },
    { 750, 110, ai_wick, 0, hurt_wick, 0, 0, 3 },
    { 114, 33, ai_brisket, die_brisket, 0, 0, 0, 3 },
};
#define N_TAB_G6D ((int) (sizeof g_tab_g6d / sizeof g_tab_g6d[0]))
