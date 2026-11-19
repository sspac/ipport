/* ffen_g6.h - Burning Basement, part B: the monsters. Included by ffenemy.h after ffen_g6a.h.
 *   erfly/enemies/woodburner.lua, flare.lua (Mr Flare, Mr Crisply), spark.lua, mullikaboom.lua, charlie.lua, sooty.lua; main.lua 12279 spitroastAI, 12323 powderkegAI
 * Field use: s->cd = StateFrame (npc.StateFrame), s->i[] / s->f[] = the Lua's d.<field> noted at each AI. */

/* ------------------------------------------------------------------------------------------ small pieces
 * the poof the Sooty tears leave (1000.1722, sub 0 coal / sub 1 coal2): checkSootyTearPoof plays "Destroy" and removes it */
static void g6_poof(float x, float y, int sub, const char *anm2) {
    unsigned long f = en_spawn(1000, 1722, x, y, 0, 0, 0, sub); if (!f) return;
    if (anm2 && g_load) g_load((void *) (f + E_SPRITE), anm2);
    if (g_play) g_play((void *) (f + E_SPRITE), "Destroy", 1);
    int len = en_anim_len((1000u << 20) | (1722u << 4) | (unsigned) sub, "Destroy"); if (len <= 0) len = 12;
    en_fx_add(f, FX_SOOTYPOOF, len, 0);
}
/* mod:LazySpawnProjectile(1, 0, pos, vel, npc) + the coal fields: the shot, its sprite ("spin"), and what to do where it lands (en_coal_land) */
static unsigned long g6_coal(unsigned long e, float x, float y, float vx, float vy, int kind, float fspeed, float faccel, const char *anm2, float a) {
    en_pp_t pp; memset(&pp, 0, sizeof pp); pp.variant = 1; pp.has_fspeed = 1; pp.fspeed = fspeed; pp.has_faccel = 1; pp.faccel = faccel;
    unsigned long pr = en_shoot_p(e, x, y, vx, vy, &pp); if (!pr) return 0;
    if (g_load) { g_load((void *) (pr + E_SPRITE), anm2); if (g_play) g_play((void *) (pr + E_SPRITE), "spin", 1); }
    else ENLOG(185, 1, "ffenemy: not done: Sprite::Load is not installed: the coal shots have the bone-tear sprite");
    pj_add(pr, e, PJ_COAL, 0); en_proj_watch2(pr, kind, e, a, 0);
    if (en_flag(e, EF_FLAG_FRIENDLY | EF_FLAG_CHARM)) ENLOG(186, 1, "ffenemy: not done: a charmed/friendly coal thrower's shot hurting enemies instead of the player (makeProjectileConsiderFriend)");
    return pr;
}
/* the coal reaches the ground (ProjectileCallbacks "coal" / "coal2" / "coalRadioactive" Update with v:IsDead()) */
static void en_coal_land(int kind, unsigned long owner, float x, float y, float a, float b) {
    (void) b;
    unsigned long own = owner && alive(owner) ? owner : 0;
    if (kind == PW_COAL) {
        en_gunpowder(own, x, y, 200, 50, -1.0f, 0);
        float vx, vy; en_rand_vec(&vx, &vy, 20.0f);
        for (int i = 1; i <= 6; i++) {
            float rx, ry; vrot(vx, vy, 60.0f * (float) i - 15.0f + (float) rint_(1, 30), &rx, &ry); vresize(&rx, &ry, (float) rint_(25, 40));
            en_gunpowder(own, x + rx, y + ry, 200, 50, -1.0f, 0);
            en_smoke(0, x + rx * 0.2f, y + ry * 0.2f, rx * 0.15f, ry * 0.15f, 0.0f, (float) rint_(1, 360), 0, 0, 0, 0, 0, 0, 0, 0);
        }
        en_sound(SOUND_ROCK_CRUMBLE, 0.15f, 1.6f);
        g6_poof(x, y, 0, 0);
    } else if (kind == PW_COAL2) {
        float vx, vy; en_rand_vec(&vx, &vy, 8.0f); if (a > 0.0f) vresize(&vx, &vy, a);
        for (int i = 1; i <= 6; i++) {
            float rx, ry; vrot(vx, vy, 60.0f * (float) i, &rx, &ry);
            fire_t *f = en_fire_new(own, x, y, rx, ry, 1); if (!f) continue;
            f->parent = own; en_fire_go(f);
        }
        en_sound(SOUND_ROCK_CRUMBLE, 0.15f, 1.5f); en_sound(FFSND_FireFizzle, 0.4f, 1.3f);
        g6_poof(x, y, 1, 0);
    } else if (kind == PW_REDCREEP) {                                        /* the Fire Fly's red bullets: projectiles.lua OnProjDeath, creeptype "red" -> CREEP_RED at the landing spot */
        en_spawn(1000, EV_CREEP_RED, x, y, 0, 0, own, 0);
    } else if (kind == PW_COALRAD) {
        float d; gp_t *g = en_unlit_powder(x, y, &d);
        if (g && d < 40.0f) g->flaming = 1;
        en_sound(SOUND_ROCK_CRUMBLE, 0.15f, 1.5f); en_sound(FFSND_FireFizzle, 0.4f, 1.3f);
        g6_poof(x, y, 1, "gfx/projectiles/sooty_tear_radioactive.anm2");
        for (int i = -30; i <= 30; i += 30) { float rx, ry; vrot(0, -10.0f, (float) i, &rx, &ry); en_smoke(0, x, y, rx, ry, -10.0f, 0, 1, 0.5f, 1.5f, 1.0f, 1.0f, 0, 0, 0); }
    }
}

/* the smoke a dead Charlie / Sooty keeps throwing every frame it is dead (`if npc:IsDead() then` runs each update until the corpse is gone) */
typedef struct { unsigned long e; int left; } dsm_t;
static dsm_t g_dsm[8];
static void g6_death_smoke(unsigned long e) {
    const float *p = fpos(e);
    for (int i = -30; i <= 30; i += 30) { float rx, ry; vrot(0, -10.0f, (float) i, &rx, &ry); en_smoke(e, p[0], p[1], rx, ry, -10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); }
}
static void g6_death_smoke_on(unsigned long e, int frames) {
    g6_death_smoke(e);
    for (int k = 0; k < 8; k++) if (!g_dsm[k].e) { g_dsm[k].e = e; g_dsm[k].left = frames; return; }
}
/* the explosions Mullikaboom's death chains (smokingMulliganExplosionHandling): 4 at 40 px on tick +5, 2 at 80 px on tick +10 */
typedef struct { float x, y; int age; } mk_t;
static mk_t g_mk[8]; static int g_mk_n;
static void en_g6c_tick(void);
static void en_g6d_tick(void);
static void en_g6_tick(void) {
    en_g6c_tick();
    en_g6d_tick();
    for (int k = 0; k < 8; k++) if (g_dsm[k].e) {
        if (!alive(g_dsm[k].e) || --g_dsm[k].left < 0) { g_dsm[k].e = 0; continue; }
        g6_death_smoke(g_dsm[k].e);
    }
    for (int k = 0; k < g_mk_n; ) {
        mk_t *m = &g_mk[k]; m->age++;
        if (m->age == 5) for (int i = 1; i <= 4; i++) { float rx, ry; vrot(40.0f, 0, 90.0f * (float) i, &rx, &ry); en_explode(0, m->x + rx, m->y + ry, 10.0f, 0.5f, 1); en_spawn(1000, 1, m->x + rx, m->y + ry, 0, 0, 0, 0); }
        else if (m->age == 10) for (int i = 1; i <= 2; i++) { float rx, ry; vrot(80.0f, 0, 180.0f * (float) i, &rx, &ry); en_explode(0, m->x + rx, m->y + ry, 5.0f, 0.5f, 1); en_spawn(1000, 1, m->x + rx, m->y + ry, 0, 0, 0, 0); }
        if (m->age >= 11) { g_mk[k] = g_mk[--g_mk_n]; continue; }
        k++;
    }
}

/* ------------------------------------------------------------------------------------------ Woodburner 160.310 (woodburner.lua)
 * state 0 idle / 1 spinstart / 2 spinloop / 3 spinstop; cd = StateFrame; i[0] = d.spinning; f[0..1] = d.gopos */
static void ai_woodburner(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    en_random_confuse(e, t, tp);
    if (!s->init) { s->init = 1; s->state = 0; en_overlay(s, "Head"); float x, y; en_rand_vec(&x, &y, 40.0f); s->f[0] = x; s->f[1] = y; }
    else s->cd++;
    if (s->i[0]) {
        en_flip(e, 1);
        en_loop_start(SOUND_ULTRA_GREED_SPINNING, 0.3f, 1.5f, e);
        float x = tp[0] - p[0], y = tp[1] - p[1];
        if (en_scare(e)) { vresize(&x, &y, -12.0f); float rx, ry; vrot(x, y, (float) rint_(-24, 25), &rx, &ry); v[0] = lerpf(v[0], rx, 0.04f); v[1] = lerpf(v[1], ry, 0.04f); }
        else if (en_line_clear(p[0], p[1], tp[0], tp[1], 0) && vlen(x, y) < 150.0f) { vresize(&x, &y, 12.0f); float rx, ry; vrot(x, y, (float) rint_(-24, 25), &rx, &ry); v[0] = lerpf(v[0], rx, 0.04f); v[1] = lerpf(v[1], ry, 0.04f); }
        else en_cathery(s, e, s->f, 12.0f, 0.04f);
        if (s->cd % 30 == 0) en_vpos(e, 2, &s->f[0], &s->f[1]);
        if (s->cd % 10 == 0) {                                                       /* the three neck fires that circle the monster */
            float vx, vy; en_rand_vec(&vx, &vy, 5.0f);
            for (int i = 1; i <= 3; i++) {
                float rx, ry; vrot(vx, vy, 120.0f * (float) i, &rx, &ry);
                fire_t *f = en_fire_new(e, p[0], p[1], rx, ry, 10); if (!f) continue;
                f->parent = e; f->ivx = rx; f->ivy = ry;
            }
        }
    } else {
        if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0);
    }
    if (s->state == 0) {
        en_overlay(s, "Head");
        float gx = s->f[0] - p[0], gy = s->f[1] - p[1];
        if (en_scare(e)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -7.0f); float rx, ry; vrot(x, y, (float) rint_(-24, 25), &rx, &ry); v[0] = lerpf(v[0], rx, 0.07f); v[1] = lerpf(v[1], ry, 0.07f); }
        else if (en_line_clear(p[0], p[1], s->f[0], s->f[1], 0)) { float x = gx, y = gy; vresize(&x, &y, 7.0f); float rx, ry; vrot(x, y, (float) rint_(-24, 25), &rx, &ry); v[0] = lerpf(v[0], rx, 0.07f); v[1] = lerpf(v[1], ry, 0.07f); }
        else if (s->age % 3 == 1) en_cathery(s, e, s->f, 7.0f, 0.14f);
        if (s->cd % 20 == 0) en_vpos(e, 3, &s->f[0], &s->f[1]);
        if (s->cd > 30 && rnd(30) == 0 && !en_confused(e)) s->state = 1;
    } else if (s->state == 1) {
        if (!s->i[0]) { v[0] *= 0.8f; v[1] *= 0.8f; }
        if (en_overlay_finished(s, "SpinStart")) { s->state = 2; s->cd = 0; }
        else if (!strcmp(s->ov, "SpinStart") && s->ovf == 13) {
            en_sound(FFSND_MonsterYellFlash, 1.0f, (float) rint_(110, 120) / 100.0f);
            s->i[0] = 1; en_vpos(e, 2, &s->f[0], &s->f[1]);
        } else en_overlay(s, "SpinStart");
    } else if (s->state == 2) {
        en_overlay(s, "SpinLoop");
        if (s->cd > 40 || (s->cd > 20 && rnd(40) == 0)) s->state = 3;
    } else {
        if (!s->i[0]) { v[0] *= 0.8f; v[1] *= 0.8f; }
        if (en_overlay_finished(s, "SpinStop")) { s->state = 0; en_vpos(e, 3, &s->f[0], &s->f[1]); s->cd = 0; }
        else if (!strcmp(s->ov, "SpinStop") && s->ovf == 9) { en_loop_stop(SOUND_ULTRA_GREED_SPINNING); s->i[0] = 0; }
        else en_overlay(s, "SpinStop");
    }
}
static void die_woodburner(en_t *s, unsigned long e) {
    (void) s; (void) e; en_loop_stop(SOUND_ULTRA_GREED_SPINNING);
    ENLOG(187, 1, "ffenemy: not done: the charred splat colour (npc.SplatColor = ColorCharred) of the Burning monsters");
}

/* Woodburner (Easy Mode) 160.311: wanders, charms itself to a player that touches it, leaves a rainbow poof. i[0] = removed, i[1] = charmed */
static void ai_woodburner_easy(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) {
        s->init = 1; s->state = 0; en_overlay(s, "Head2"); float x, y; en_rand_vec(&x, &y, 40.0f); s->f[0] = x; s->f[1] = y;
        if (!en_has_path(e, t)) { en_remove(e); s->i[0] = 1; return; }
    } else s->cd++;
    en_flag_clear(e, EF_FLAG_PERSISTENT);
    if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0);
    en_overlay(s, "Head2");
    if (en_line_clear(p[0], p[1], s->f[0], s->f[1], 0)) { float x = s->f[0] - p[0], y = s->f[1] - p[1]; vresize(&x, &y, 7.0f); float rx, ry; vrot(x, y, (float) rint_(-24, 25), &rx, &ry); v[0] = lerpf(v[0], rx, 0.07f); v[1] = lerpf(v[1], ry, 0.07f); }
    else if (s->age % 3 == 1) en_cathery(s, e, s->f, 7.0f, 0.07f);
    if (s->cd % 20 == 0) en_vpos(e, 3, &s->f[0], &s->f[1]);
    if (!s->i[1] && vlen(p[0] - t[0], p[1] - t[1]) < *(const float *) (e + 0x344) + 13.0f) {      /* woodburnerEasyColl */
        en_sound(SOUND_BROWNIE_LAUGH, 1.0f, (float) rint_(10, 16) / 10.0f);
        en_flag_set(e, EF_FLAG_CHARM | EF_FLAG_FRIENDLY); *(int *) (e + E_SUBTYPE) = 1; s->i[1] = 1;
    }
}
static void die_woodburner_easy(en_t *s, unsigned long e) {
    if (s->i[0]) return;
    float *p = fpos(e); en_spawn(1000, 104, p[0], p[1], 0, 0, e, 0); en_sound(SOUND_HAPPY_RAINBOW, 1.0f, 1.0f);
}

/* ------------------------------------------------------------------------------------------ Spark 160.152 (spark.lua)
 * state 0 Idle / 1 Squidge; cd = StateFrame; i[0] = randwait; ->flaming = d.flaming (a lit Spark lights gunpowder) */
static void ai_spark(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; s->state = 0; s->i[0] = (int) rnd(5); } else s->cd++;
    if (s->state == 0) {
        en_flip(e, !(v[0] > 0)); en_play(s, "Idle");
        v[0] *= 0.92f; v[1] *= 0.92f;
        if (s->cd > 20 + s->i[0]) s->state = 1;
    } else {
        v[0] *= 0.99f; v[1] *= 0.99f;
        if (en_finished(s, "Move")) { s->state = 0; s->i[0] = (int) rnd(5); s->cd = 0; }
        else if (en_event(s, "Move", "Move it babie")) {
            fire_t *f = en_fire_new(e, p[0], p[1], 0, 0, 0);                                  /* Gimme a fire */
            if (f) { f->timer = 100; en_fire_go(f); }
            en_sound(FFSND_FireLight, 1.0f, 2.0f); s->flaming = 1;
            float gx, gy; int have = en_vpos(e, 10, &gx, &gy);                              /* Gimme a move */
            if (en_scare(e)) { float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, -6.0f); v[0] = x; v[1] = y; }
            else if (have && rint_(1, 4) == 1) {
                if (en_line_clear(p[0], p[1], gx, gy, 0) && vlen(gx - p[0], gy - p[1]) < 100.0f) { float x = gx - p[0], y = gy - p[1]; vresize(&x, &y, 4.0f); v[0] = x; v[1] = y; }
                else { float wx, wy; if (path_step(p[0], p[1], gx, gy, 0, &wx, &wy)) { float x = wx - p[0], y = wy - p[1]; vresize(&x, &y, 2.0f); v[0] += x; v[1] += y; } }
            } else { float x, y; en_rand_vec(&x, &y, 4.0f); v[0] = x; v[1] = y; }
            en_flip(e, !(v[0] > 0));                                                           /* Gimme a flip */
        } else if (en_event(s, "Move", "Done")) s->flaming = 0;
        else en_play(s, "Move");
    }
}
static void hurt_spark(en_t *s, unsigned long e, float dmg) {
    (void) dmg; hurt_fireproof(s, e, dmg);
    if (!s->hurt_block && rnd(5) == 0) en_sound(SOUND_BABY_HURT, 1.0f, 1.0f);
}

/* ------------------------------------------------------------------------------------------ Mr. Flare 160.280 (flare.lua): state 0 idle / 1 shoot; cd = StateFrame */
static void ai_flare(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; s->state = 0; en_flip(e, p[0] > t[0]); } else s->cd++;
    v[0] *= 0.3f; v[1] *= 0.3f;
    if (s->state == 0) {
        en_play(s, "Idle");
        if (s->cd > 40 && vlen(t[0] - p[0], t[1] - p[1]) < 180.0f && !en_scare_conf(e)) s->state = 1;
    } else {
        if (en_finished(s, "Shoot")) { s->state = 0; s->cd = 0; }
        else if (en_event(s, "Shoot", "Shoot")) {
            en_flip(e, p[0] > t[0]);
            en_sound(FFSND_FlashShakeyKidRoar, 1.5f, 1.0f);
            int count = rint_(3, 7);
            for (int i = 1; i <= count; i++) {
                float rx, ry; vrot(5.0f, 0, 360.0f / (float) count * (float) i - 25.0f + (float) rint_(1, 50), &rx, &ry);
                fire_t *f = en_fire_new(e, p[0], p[1], rx, ry, 20); if (!f) continue;
                f->timer = 50; f->has_grid = 1; f->grid_coll = 0; f->parent = e;
            }
        } else en_play(s, "Shoot");
    }
}

/* ------------------------------------------------------------------------------------------ Mr. Crisply 160.281: i[1] = the death burst was done */
static void crisply_burst(en_t *s, unsigned long e) {
    float *p = fpos(e); if (s->i[1]) return; s->i[1] = 1;
    int count = rint_(4, 5);
    en_sound(FFSND_FlashShakeyKidRoar, 1.5f, 1.0f);
    for (int i = 1; i <= count; i++) {
        float rx, ry; vrot(2.5f, 0, 360.0f / (float) count * (float) i - 25.0f + (float) rint_(1, 50), &rx, &ry);
        unsigned long f = en_spawn(33, 10, p[0], p[1], rx, ry, e, 0);
        if (f && g_hp_off) *(float *) (f + g_hp_off) /= 1.25f;
    }
}
static void ai_crisply(en_t *s, unsigned long e) {
    float *v = fvel(e);
    if (!s->init) { s->init = 1; s->state = 0; }
    else if (g_hp_off) {
        s->cd++;
        float *hp = (float *) (e + g_hp_off), mx = *(const float *) (e + 0x358);
        *hp *= (float) rint_(*hp < mx / 10.0f ? 950 : 990, 999) / 1000.0f;
    }
    v[0] *= 0.93f; v[1] *= 0.93f;
    en_flip(e, v[0] > 0);
    en_sprite_offset(s, e, (float) (s->age % 2), 0.0f);
    en_play(s, "Idle");
    if (g_hp_off) {
        float hp = fb_hp(e), mx = *(const float *) (e + 0x358);
        int ft = (int) ceilf((mx - hp) / mx * 300.0f); if (ft < 0) ft = 0; if (ft > 300) ft = 300;
        en_set_overlay_frame(e, "OverlayFunnies", ft);
        if (hp < 0.1f && !s->i[1]) { crisply_burst(s, e); en_kill(e); }
    }
}
static void die_crisply(en_t *s, unsigned long e) { crisply_burst(s, e); }

/* ------------------------------------------------------------------------------------------ Mullikaboom 160.550 (mullikaboom.lua): ->i[0] = exploded */
static void ai_mullikaboom(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    en_confuse_pos(s, e, t, tp);
    if (vlen(v[0], v[1]) > 0.1f) { en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); en_overlay(s, "Walk"); }
    else { en_set_frame(s, "WalkVert", 0); en_set_overlay_frame(e, "Walk", 0); s->ov[0] = 0; }
    float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 4.0f);
    if (en_scared(e)) { x = -x; y = -y; }
    float ax = tp[0] - x / vlen(x, y) * 5.0f, ay = tp[1] - y / vlen(x, y) * 5.0f;
    if (en_line_clear(p[0], p[1], ax, ay, 0)) { v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
    else en_walk_to(e, tp, 0.0f, 0.6f, 0);
    if (!en_charmed(e) && vlen(p[0] - t[0], p[1] - t[1]) < *(const float *) (e + 0x344) + 13.0f) en_kill(e);   /* mullikaboomColl: touching a player kills it */
}
static void die_mullikaboom(en_t *s, unsigned long e) {
    float *p = fpos(e); (void) s;
    en_explode(e, p[0], p[1], 15.0f, 0.5f, 1);
    en_spawn(1000, 1, p[0], p[1], 0, 0, e, 0);
    if (g_mk_n < 8) { g_mk[g_mk_n].x = p[0]; g_mk[g_mk_n].y = p[1]; g_mk[g_mk_n].age = 0; g_mk_n++; }
}

/* ------------------------------------------------------------------------------------------ Charlie 160.430 (charlie.lua): state 0 idle / 1 attack; cd = StateFrame */
static void ai_charlie(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    en_confuse_pos(s, e, t, tp);
    if (!s->init) { s->init = 1; s->state = 0; char nm[64]; snprintf(nm, sizeof nm, "gfx/enemies/smokey/charlie_head%d.png", rint_(1, 10)); en_sheet(e, 1, nm); }
    else s->cd++;
    if (s->state == 0) {
        if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0);
        float x = tp[0] - p[0], y = tp[1] - p[1];
        if (en_scare(e)) { vresize(&x, &y, -4.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
        else if (en_line_clear(p[0], p[1], tp[0], tp[1], 0)) { vresize(&x, &y, 4.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
        else en_walk_to(e, tp, 0.0f, 0.5f, 0);
        if (s->cd > 50 && !en_scare_conf(e) && rnd(20) == 0) { s->state = 1; en_play(s, "Attack"); }
    } else {
        v[0] *= 0.9f; v[1] *= 0.9f;
        if (en_finished(s, "Attack")) { s->state = 0; s->cd = 0; }
        else if (s->af == 5 && !strcmp(s->anim, "Attack")) {
            en_sound(FFSND_CoughRasp, 1.0f, (float) rint_(90, 100) / 100.0f);
            float x = (tp[0] - p[0]) / 30.0f, y = (tp[1] - p[1]) / 30.0f; if (vlen(x, y) > 13.0f) vresize(&x, &y, 13.0f);
            g6_coal(e, p[0], p[1], x, y, PW_COAL, -20.0f, 1.2f, "gfx/projectiles/sooty_tear.anm2", 0);
            for (int i = 0; i < 5; i++) {
                float rx, ry; vrot(0, (float) rint_(4, 7), (float) rint_(-59, 60), &rx, &ry);
                en_smoke(e, p[0], p[1], v[0] * 1.3f + rx, v[1] * 1.3f + ry, -16.0f, (float) rint_(1, 360), 1, 1, 1, 1, 0.6f, 0, 0, 0);
            }
        } else en_play(s, "Attack");
    }
}
static void die_charlie(en_t *s, unsigned long e) { (void) s; g6_death_smoke_on(e, 2); }

/* ------------------------------------------------------------------------------------------ Sooty 160.431 (sooty.lua): state 0 idle / 1 attack; cd = StateFrame */
static void ai_sooty(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    en_confuse_pos(s, e, t, tp);
    if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0);
    if (!s->init) { s->init = 1; s->state = 0; en_overlay(s, "Head"); } else s->cd++;
    float x = tp[0] - p[0], y = tp[1] - p[1];
    if (en_scare(e)) { vresize(&x, &y, -3.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
    else if (en_line_clear(p[0], p[1], tp[0], tp[1], 0)) { vresize(&x, &y, 3.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
    else en_walk_to(e, tp, 0.0f, 0.4f, 0);
    if (s->state == 0) {
        en_overlay(s, "Head");
        if (s->cd > 50 && !en_scare_conf(e) && rnd(40) == 0) s->state = 1;
    } else {
        if (en_overlay_finished(s, "Attack")) { s->state = 0; s->cd = 0; }
        else if (!strcmp(s->ov, "Attack") && s->ovf == 3) {
            en_sound(FFSND_CoughRasp, 1.0f, (float) rint_(90, 100) / 100.0f);
            float ax = tp[0] - p[0], ay = tp[1] - p[1]; vresize(&ax, &ay, 7.0f);
            g6_coal(e, p[0], p[1], ax, ay, PW_COAL2, -15.0f, 1.0f, "gfx/projectiles/sooty_tear2.anm2", 5.0f);
            for (int i = 0; i < 5; i++) {
                float rx, ry; vrot(0, (float) rint_(4, 7), (float) rint_(-59, 60), &rx, &ry);
                en_smoke(e, p[0], p[1], v[0] * 1.3f + rx, v[1] * 1.3f + ry, -26.0f, (float) rint_(1, 360), 1, 2, 2, 2, 0.3f, 180.0f / 255.0f, 95.0f / 255.0f, -55.0f / 255.0f);
                float ex, ey; vrot(0, (float) rint_(2, 4), (float) rint_(-59, 60), &ex, &ey);
                unsigned long em = en_spawn(1000, EV_EMBER_PARTICLE, p[0], p[1], v[0] + ex * 0.7f, v[1] + ey * 0.7f, e, 0); (void) em;
            }
        } else en_overlay(s, "Attack");
    }
}
static void die_sooty(en_t *s, unsigned long e) { (void) s; g6_death_smoke_on(e, 2); }

/* ------------------------------------------------------------------------------------------ Powderkeg 227.961 (main.lua 12323): f[0..1] = d.lastpos */
static void ai_powderkeg(en_t *s, unsigned long e) {
    float *p = fpos(e);
    if (!s->init) { s->init = 1; s->f[0] = p[0]; s->f[1] = p[1]; } else en_set_state(e, 4);
    if (vlen(s->f[0] - p[0], s->f[1] - p[1]) > 20.0f) { en_gunpowder(e, p[0], p[1], -1, -1, -1.0f, 0); s->f[0] = p[0]; s->f[1] = p[1]; }
}
static void die_powderkeg(en_t *s, unsigned long e) {
    float *p = fpos(e); (void) s;
    en_fire_new(e, p[0], p[1], 0, 0, 0);
    for (int i = 60; i <= 360; i += 60) { float rx, ry; vrot(25.0f, 0, (float) i, &rx, &ry); en_gunpowder(e, p[0] + rx, p[1] + ry, 1, 30, -1.0f, 0); }
    en_explode(e, p[0], p[1], 10.0f, 1.0f, 0);
}
static void hurt_powderkeg(en_t *s, unsigned long e, float dmg) { (void) s; (void) dmg; if (g_hit_flags & DMG_FIRE) en_kill(e); }   /* checkBonyHurt: any fire damage Kills it */

/* ------------------------------------------------------------------------------------------ Spitroast 61.960 / Spitfire 61.961 (main.lua 12279) */
static void ai_spitroast(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    if (s->variant == 960 && vlen(v[0], v[1]) > 2.0f) vresize(&v[0], &v[1], 2.0f);
    if (s->variant == 961 && s->age % 25 == 15) {
        for (int i = 1; i <= 5; i++) {
            float rx, ry; vrot(6.0f, 0, 72.0f * (float) i, &rx, &ry);
            en_fire_go(en_fire_new(e, p[0], p[1], rx, ry, 1));
        }
    }
}
static void die_spitroast(en_t *s, unsigned long e) {
    float *p = fpos(e);
    if (s->variant == 961) {
        en_sound(FFSND_FireFizzle, 1.2f, 1.0f);
        int n = rint_(12, 15);
        for (int i = 1; i <= n; i++) { float rx, ry; vrot((float) rint_(6, 16), 0, 360.0f / (float) n * (float) i, &rx, &ry); en_fire_go(en_fire_new(e, p[0], p[1], rx, ry, 1)); }
    } else {
        en_sound(FFSND_FireFizzle, 1.0f, 1.3f);
        for (int i = 1; i <= 10; i++) { float rx, ry; vrot(8.0f, 0, 36.0f * (float) i, &rx, &ry); en_fire_go(en_fire_new(e, p[0], p[1], rx, ry, 1)); }
    }
}
/* checksuckerhurt: fire damage from a monster does nothing, from the player it hurts twice as much */
static void hurt_spitroast(en_t *s, unsigned long e, float dmg) {
    s->hurt_block = 0;
    if (!(g_hit_flags & DMG_FIRE)) return;
    if (en_fire_from_not_player()) { s->hurt_block = 1; return; }
    hit_enemy_f(e, dmg * 2.0f, 0, 0);
}

static const enemy_t g_tab_g6[] = {
    { 160, 310, ai_woodburner, die_woodburner, hurt_fireproof, 0, 0, 3 },
    { 160, 311, ai_woodburner_easy, die_woodburner_easy, 0, 0, 0, 3 },
    { 160, 152, ai_spark, 0, hurt_spark, 0, 0, 3 },
    { 160, 280, ai_flare, 0, hurt_fireproof, 0, 0, 0 },
    { 160, 281, ai_crisply, die_crisply, 0, 0, 0, 3 },
    { 160, 550, ai_mullikaboom, die_mullikaboom, 0, 0, 0, 3 },
    { 160, 430, ai_charlie, die_charlie, 0, 0, 0, 3 },
    { 160, 431, ai_sooty, die_sooty, 0, 0, 0, 3 },
    { 227, 961, ai_powderkeg, die_powderkeg, hurt_powderkeg, 0, 0, 0 },
    { 61, 960, ai_spitroast, die_spitroast, hurt_spitroast, 0, 0, 0 },
    { 61, 961, ai_spitroast, die_spitroast, hurt_spitroast, 0, 0, 0 },
};
#define N_TAB_G6 ((int) (sizeof g_tab_g6 / sizeof g_tab_g6[0]))
