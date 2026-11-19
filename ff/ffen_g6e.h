/* ffen_g6e.h - Burning Basement, part E: Fried 240.700 (main.lua friedAI), Big Smoke 208.963 (bigSmokeAI), Smogger 817.140 (main.lua 13739),
 * Commission 180.21 (budj/buster.lua commissionAI), Flaming Minecart 965.67 (catinsurance/enemies/flaming_minecart.lua). Included by ffenemy.h after ffen_g6d.h. */

#define G6E_SMALL_LEECH 810
#define G6E_LITTLE_HORN 404
static float g6e_rot(unsigned long e) { return *(const float *) (e + E_SPRITE + 0xe4); }
/* mod:runIfFear(npc, vec): a frightened monster runs from the player at the same speed */
static void g6e_run_if_fear(unsigned long e, float *x, float *y) {
    const float *t = en_target(e), *p = fpos(e); if (!t || !en_flag(e, EF_FLAG_FEAR)) return;
    float l = vlen(*x, *y), ax = p[0] - t[0], ay = p[1] - t[1]; vresize(&ax, &ay, l); *x = ax; *y = ay;
}

/* ------------------------------------------------------------------------------------------ Fried 240.700: a wall creep that breathes fire along its wall
 * state 0 idle / 1 AttackStart / 2 attack (the engine's own wall-creep logic turns State into 8 when it wants to attack); cd2 = the Lua's StateFrame of the attack */
static void ai_fried(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    if (!s->init) { s->init = 1; s->state = 0; en_set_statef(e, 60); }
    float rot = g6e_rot(e); int est = en_get_state(e);
    if (s->state == 0) {
        if (est == 8) { s->state = 1; en_set_state(e, 5); } else en_set_state(e, 4);
    } else if (s->state == 1) {
        if (en_finished(s, "AttackStart")) { s->state = 2; en_loop_start(FFSND_FriedLoop, 1.0f, 1.0f, e); s->cd2 = 100; en_set_statef(e, 100); }
        else if (en_playing(s, "AttackStart") && s->af == 7) en_sound(FFSND_FriedStart, 1.0f, 1.0f);
        else en_play(s, "AttackStart");
    } else {
        en_set_state(e, 4); s->cd2--;
        if (s->cd2 < 1) { s->state = 0; en_set_statef(e, 60); }
        else if (s->cd2 == 10) { en_loop_stop(FFSND_FriedLoop); en_sound(FFSND_FriedEnd, 1.0f, 1.0f); }
        else if (s->cd2 > 10) en_loop_start(FFSND_FriedLoop, 1.0f, 1.0f, e);
        if (vlen(v[0], v[1]) > 3.0f) vresize(&v[0], &v[1], 3.0f);
        float ox = 0, oy = 0;
        if (fabsf(rot) < 1.0f) { ox = 0; oy = -20.0f; } else if (fabsf(rot - 90.0f) < 1.0f) { ox = 20.0f; oy = -15.0f; }
        else if (fabsf(rot + 90.0f) < 1.0f) { ox = -20.0f; oy = -15.0f; } else if (fabsf(fabsf(rot) - 180.0f) < 1.0f) { ox = 0; oy = 20.0f; }
        float fx, fy; vrot(0, 8.0f, -15.0f + (float) rint_(1, 30) + rot, &fx, &fy);
        fire_t *f = en_fire_new(e, p[0] + ox, p[1] + oy, fx, fy, 0);
        if (f) { f->flamethrower = 1; f->timer = 17; f->has_grid = 1; f->grid_coll = 0; en_fire_set_color(f, -100.0f / 255.0f, 70.0f / 255.0f, 455.0f / 255.0f, 10); en_fire_go(f); }
        if (s->age % 2 == 0) {
            float ex, ey; vrot(0, (float) rint_(4, 7), -35.0f + (float) rint_(1, 70) + rot, &ex, &ey);
            en_spawn(1000, EV_EMBER_PARTICLE, p[0] + ox, p[1] + oy, ex, ey, e, 0);
            vrot(0, (float) rint_(4, 7), -35.0f + (float) rint_(1, 70) + rot, &ex, &ey);
            unsigned long sm = en_smoke(e, p[0] + ox, p[1] + oy, ex, ey, 0.0f, (float) rint_(1, 360), 1, 1, 1, 1, 0.3f, 75.0f / 255.0f, 70.0f / 255.0f, 50.0f / 255.0f);
            if (sm) { float sx, sy; vrot(0, 3.0f, rot, &sx, &sy); en_spr_off(sm, sx, sy); }
        }
        if (vlen(v[0], v[1]) < 1.0f) en_set_frame(s, "AttackLoop", s->age % 4); else en_set_frame(s, "AttackWalk", s->age % 8);
    }
}
static void die_fried(en_t *s, unsigned long e) { (void) s; (void) e; en_loop_stop(FFSND_FriedLoop); }

/* ------------------------------------------------------------------------------------------ Big Smoke 208.963: a Fatty that sheds gunpowder
 * the engine runs its attack (State 8); the "Shoot" event of the attack animation is frame 21 of AttackVert / AttackHori; i[0] = the engine frame seen last tick */
static void g6e_powder_ring(unsigned long e, int n, int rmin, int rmax) {
    const float *p = fpos(e); float vx, vy; en_rand_vec(&vx, &vy, 20.0f);
    for (int i = 1; i <= n; i++) {
        float rx, ry; vrot(vx, vy, 360.0f / (float) n * (float) i - 15.0f + (float) rint_(1, 30), &rx, &ry); vresize(&rx, &ry, (float) rint_(rmin, rmax));
        en_gunpowder(e, p[0] + rx, p[1] + ry, 300, 150, -1.0f, 0);
    }
}
static void ai_bigsmoke(en_t *s, unsigned long e) {
    if (!s->init) { s->init = 1; ENLOG(290, 1, "ffenemy: not done: Big Smoke's splat colour (SplatColor charred)"); }
    int fi = (int) *(const float *) (e + E_SPR_FRAME);
    if (en_get_state(e) == 8 && *(const unsigned char *) (e + E_SPR_PLAYING)) {
        if (fi == 21 && s->i[0] != 21) g6e_powder_ring(e, 6, 25, 40);
        s->i[0] = fi;
    } else s->i[0] = -1;
}
static void die_bigsmoke(en_t *s, unsigned long e) {
    (void) s; const float *p = fpos(e);
    unsigned long sm = en_smoke(e, p[0], p[1], 0, 0, -25.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); if (sm) en_scale(sm, 3.0f, 3.0f);
    g6e_powder_ring(e, 6, 15, 30);
    { float vx, vy; en_rand_vec(&vx, &vy, 20.0f);
      for (int i = 1; i <= 8; i++) { float rx, ry; vrot(vx, vy, 360.0f / 8.0f * (float) i - 15.0f + (float) rint_(1, 30), &rx, &ry); vresize(&rx, &ry, (float) rint_(35, 60)); en_gunpowder(e, p[0] + rx, p[1] + ry, 300, 150, -1.0f, 0); } }
}

/* ------------------------------------------------------------------------------------------ Smogger 817.140
 * the engine's states: 4 idle, 8 attack; the Lua's own 25 = burning. i[0] = d.Burning (0 not, 1 catching, 2 flaming); flaming = s->flaming (the fire kin check in en_is_fire_source) */
static void ai_smogger(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t0 = en_target(e), tp[2]; if (!t0) return;
    if (!s->init) {
        s->init = 1; s->i[0] = 0; en_set_gridcoll(e, GRIDCOLL_NOPITS); en_sprite_offset(s, e, 0.0f, -2.0f);
        ENLOG(291, 1, "ffenemy: not done: Smogger's splat colour, its V1 (attack range 30 / 130-150 / 140), the collision damage of the flaming champion and the dark red champion revival");
    }
    /* the leeches it spawns (Small Leech 810) become half-health Little Horns 404.1 (POST_NPC_INIT) */
    { unsigned long snap[SNAP]; unsigned n = snapshot(snap);
      for (unsigned k = 0; k < n; k++) { unsigned long c = snap[k]; if (!alive(c) || *(const int *) (c + E_TYPE) != G6E_SMALL_LEECH) continue;
        if (*(const unsigned long *) (c + 0x400) != e) continue;
        unsigned long h = en_morph(c, G6E_LITTLE_HORN, 1, 0);
        if (h) { *(float *) (h + 0x358) /= 2.0f; en_set_gridcoll(h, GRIDCOLL_WALLS); } } }
    if (s->age > 1) {
        int est = en_get_state(e);
        if (s->i[0] > 0 && est != 25) { v[0] = v[1] = 0; en_set_state(e, 25); est = 25; }
        if (est == 4 || est == 8) {
            en_play(s, "WalkBody"); if (est != 8) en_overlay(s, "HeadWalk");
            float size = *(const float *) (e + 0x344);
            if (en_fire_within(p[0], p[1], size * 2.0f)) s->i[0] = 1;
        }
        if (est == 25) {
            if (s->i[0] == 2) {
                en_overlay(s, "HeadFlame"); en_walk_anim(s, e, "WalkFlame", "WalkFlame", 0.0f); en_play(s, "WalkFlame");
                en_confuse_pos(s, e, t0, tp);
                float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 4.5f); if (en_scare(e)) { x = -x; y = -y; }
                v[0] = lerpf(x, v[0], 0.8f); v[1] = lerpf(y, v[1], 0.8f);
                if (s->age % 5 == 0) { fire_t *f = en_fire_new(e, p[0], p[1], 0, 0, 20); if (f) { f->timer = 10; f->has_grid = 1; f->grid_coll = 0; f->scale = 0.5f; f->parent = e; en_fire_go(f); } }
            } else if (s->i[0] == 1) {
                en_play(s, "Burning"); en_overlay_off(s);
                if (en_event(s, "Burning", "Burning")) {
                    if (g_hp_off) *(float *) (e + g_hp_off) *= 1.25f;
                    en_set_gridcoll(e, GRIDCOLL_WALLS); en_sound(SOUND_FLAMETHROWER_END, 1.0f, 1.0f); s->flaming = 1;
                }
                if (en_finished(s, "Burning")) { s->i[0] = 2; en_overlay(s, "HeadFlame"); }
            }
        }
    }
}
static void die_smogger(en_t *s, unsigned long e) {
    const float *p = fpos(e);
    if (s->flaming) { s->i[0] = 2; en_spawn(33, 10, p[0], p[1], 0, 0, e, 0); return; }
    unsigned long sm = en_smoke(e, p[0], p[1], 0, 0, -25.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); if (sm) en_scale(sm, 3.0f, 3.0f);
    g6e_powder_ring(e, 3, 5, 20); g6e_powder_ring(e, 5, 25, 50);
}
static void hurt_smogger(en_t *s, unsigned long e, float dmg) {
    (void) e; (void) dmg; s->hurt_block = en_fire_from_not_player();
    if (s->hurt_block && s->i[0] == 0) s->i[0] = 1;
}

/* ------------------------------------------------------------------------------------------ Commission 180.21 (Buster's helper)
 * state 1 fly-in / 2 idle; i[0] = d.Exploding, i[1] = d.Berserk, i[2] = FlewIn, i[3] = exploded, f[0..1] = FlyInDirection, cd = ExplodeTimer (-1: not set) */
static void g6e_commission_explode(en_t *s, unsigned long e) {
    const float *p = fpos(e); float x = p[0], y = p[1]; s->i[3] = 1;
    en_explode(e, x, y, 40.0f, 1.0f, 0);
    fire_t *f = en_fire_new(e, x, y, 0, 0, 0); if (f) { f->timer = 45; en_fire_go(f); }
}
static void ai_commission(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    if (!s->init) {
        s->init = 1; s->cd = -1; *(float *) (e + OFF_ENT_MASS) = 40.0f; en_sprite_offset(s, e, 0.0f, -10.0f);
        { char nm[64]; snprintf(nm, sizeof nm, "gfx/bosses/buster/monster_commission%d.png", rint_(1, 3)); en_sheet(e, 0, nm); }
        if (!en_in_room_walk(p[0], p[1], 0.0f)) {
            s->state = 1; s->f[0] = s->f[1] = 0;
            float x0 = en_room_x(1) - 20.0f, x1 = en_room_x((g_gw ? (int) g_gw : 15) - 2) + 20.0f, y1 = en_room_y((g_gh ? (int) g_gh : 9) - 2) + 20.0f;
            if (p[0] < x0) s->f[0] = 1.0f; else if (p[0] > x1) s->f[0] = -1.0f; else if (p[1] > y1) s->f[1] = -1.0f; else s->f[1] = 1.0f;
        }
        ENLOG(292, 1, "ffenemy: not done: Commission's orbit around its Buster (Buster is not ported: it is always an orphan, which is what the Lua does too without a parent), terror, spin and SplatColor");
    }
    en_set_collision(e, s->state == 1 ? ENTCOLL_NONE : ENTCOLL_ALL);
    if (s->i[0]) {
        en_play(s, "Pulse");
        if (s->cd < 0) s->cd = 45;
        s->cd--;
        const float *t = en_target(e); float size = *(const float *) (e + 0x344);
        if (s->cd <= 0 || (t && vlen(t[0] - p[0], t[1] - p[1]) < size + 5.0f)) { en_kill(e); g6e_commission_explode(s, e); return; }
    }
    if (s->state == 1) {
        if (s->i[2]) en_play(s, "Idle");
        else if (en_finished(s, "FlyIn")) s->i[2] = 1;
        else en_play(s, "FlyIn");
        v[0] = s->f[0] * 10.0f; v[1] = s->f[1] * 10.0f;
        if (en_in_room_walk(p[0] - s->f[0] * 20.0f, p[1] - s->f[1] * 20.0f, 0.0f)) { s->state = 0; en_play(s, "Idle"); en_set_gridcoll(e, GRIDCOLL_WALLS); }
    }
    int period = s->i[1] ? 15 : 8;
    if (s->age % period == 0 && en_in_room_walk(p[0], p[1], 0.0f)) en_gunpowder(e, p[0], p[1], 35, 45, -1.0f, 0);
    s->state = 2;                                                                                /* no Buster parent: d.State = 'idle' */
    if (s->i[1]) {
        en_set_friction(e, 0.8f);
        const float *t = en_target(e);
        if (t) { float bx = p[0] - t[0], by = p[1] - t[1], bl = vlen(bx, by);
                 if (bl < 36.0f && bl > 1e-6f) { float m = (72.0f - bl) / bl; v[0] += bx * m; v[1] += by * m; } }
        if (s->age % 10 == 0) {
            float sp = en_flag(e, EF_FLAG_CONFUSION) ? 18.0f * 0.6f : 18.0f, rx, ry; en_rand_vec(&rx, &ry, 3.0f);
            float x = v[0] * 0.8f + rx, y = v[1] * 0.8f + ry; vresize(&x, &y, sp); g6e_run_if_fear(e, &x, &y); v[0] = x; v[1] = y;
        }
    } else {
        en_play(s, "Idle"); en_set_friction(e, 0.9f);
        if (s->age % 25 == 0) {
            float sp = en_flag(e, EF_FLAG_CONFUSION) ? 6.0f : 10.0f, rx, ry; en_rand_vec(&rx, &ry, 3.0f);
            float x = v[0] * 0.8f + rx, y = v[1] * 0.8f + ry; g6e_run_if_fear(e, &x, &y); vresize(&x, &y, sp); v[0] = x; v[1] = y;
        }
    }
}
static void die_commission(en_t *s, unsigned long e) { if (s->i[0] && !s->i[3]) g6e_commission_explode(s, e); }
static void hurt_commission(en_t *s, unsigned long e, float dmg) {                               /* any fire sets it off; the gunpowder fire itself does not hurt it */
    (void) e; (void) dmg; s->hurt_block = 0;
    if (g_hit_flags & DMG_FIRE) {
        s->i[0] = 1; s->i[1] = 1;
        s->hurt_block = en_hit_by_gunpowder_fire();
    }
}

/* ------------------------------------------------------------------------------------------ Flaming Minecart 965.67: the engine moves the cart along its rails; the Lua keeps it at 9.75 px/frame and hittable */
static void ai_flaming_minecart(en_t *s, unsigned long e) {
    float *v = fvel(e);
    if (!s->init) { s->init = 1; ENLOG(293, 1, "ffenemy: not done: the Flaming Minecart's rails (the room spawn callback that lays the rail / pit-rail grid under it), its red tint and killing its rider over a pit"); }
    en_set_collision(e, ENTCOLL_ALL);
    if (vlen(v[0], v[1]) > 1e-6f) vresize(&v[0], &v[1], 6.5f * 1.5f);
}

static const enemy_t g_tab_g6e[] = {
    { 240, 700, ai_fried, die_fried, hurt_fireproof, 0, 0, 3 },
    { 208, 963, ai_bigsmoke, die_bigsmoke, 0, 0, 0, 3 },
    { 817, 140, ai_smogger, die_smogger, hurt_smogger, 0, 0, 3 },
    { 180, 21, ai_commission, die_commission, hurt_commission, 0, 1, 3 },
    { 965, 67, ai_flaming_minecart, 0, 0, 0, 0, 0 },
};
#define N_TAB_G6E ((int) (sizeof g_tab_g6e / sizeof g_tab_g6e[0]))
