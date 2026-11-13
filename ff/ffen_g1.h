/* Group 1: Morsel (erfly/enemies/morsel.lua), Mr. Horf / Mr. Red Horf and their heads (erfly/enemies/mrhorf.lua), Mutant Horf (julia/enemies/mutanthorf.lua). */

/* ---- Morsel 160.50 ---- */
static void ai_morsel(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) {
        if (s->sub > 1) {                                                       /* a bigger one: that many Morsels around it, and it goes */
            float vx, vy; en_rand_vec(&vx, &vy, 10.0f);
            for (int i = 1; i <= s->sub; i++) {
                float ox, oy; vrot(vx, vy, 360.0f / (float) i, &ox, &oy);
                unsigned long b = en_spawn(160, 50, p[0] + ox, p[1] + oy, 0, 0, e, 0);
                if (b) { *(unsigned long *) (b + 0x400) = *(const unsigned long *) (e + 0x400); en_flag_clear(b, EF_FLAG_APPEAR); }
            }
            en_flag_clear(e, EF_FLAG_APPEAR); en_remove(e); return;
        }
        s->init = 1; s->i[0] = (int) rnd(177);                                    /* d.head = math.random(177) - 1 */
    }
    en_set_overlay_frame(e, "Head", s->i[0]);
    float tp[2] = { t[0], t[1] };                                              /* mod:confusePos(): the target itself unless confused */
    if (en_scared(e)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -5.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
    else if (en_line_clear(p[0], p[1], tp[0], tp[1], 0)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 5.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
    else en_walk_to(e, tp, 0.7f, 0.7f, 0);
    if (rnd(175) == 0) en_sound(165, 0.3f, 2.0f);
    if (vlen(v[0], v[1]) > 1.0f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0);
}

/* ---- Mr. Horf 160.770 / Mr. Red Horf 160.772: it throws its head (160.771 / .773) which lands as a Horf (12.0) / Red Horf, the body becomes a Gusher/Pacer (11) ---- */
/* body: state 0 idle, 1 throw; f[0..1] = start pos of the throw (unused), i[0] = randwait, i[1] = bounding, i[2] = hasthrown */
static void ai_mrhorf(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    int red = s->variant == 772; float sp0 = red ? 6.0f : 4.0f, sp1 = red ? 0.8f : 0.6f;
    if (!s->init) { s->init = 1; s->state = 0; s->i[0] = 1 + (int) rnd(30); en_overlay(s, "Head"); }
    else en_set_statef(e, en_get_statef(e) + 1);
    if (en_overlay_finished(s, "Head")) en_overlay(s, "HeadIdle");
    int sf = en_get_statef(e); if (!npc_statef_ok()) sf = s->age;
    if (s->state == 0) {
        if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0);
        if (en_line_clear(p[0], p[1], t[0], t[1], 0) || en_scared(e)) {
            float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, sp0); if (en_scared(e)) { x = -x; y = -y; }
            v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
        } else en_walk_to(e, t, sp1, sp1, 0);
        if (!en_scared(e) && !en_confused(e) && sf > 10 + s->i[0] && rnd(15) == 0 && !en_xy(p[0], p[1], 0)) {
            s->state = 1; en_flip(e, !(t[0] > p[0]));
        }
    } else {
        if (en_finished(s, "HeadThrow")) { int nv = 1; if (rnd(3) == 0) nv = 0; en_morph(e, 11, nv, s->variant); return; }
        else if (!strcmp(s->anim, "HeadThrow") && s->af == 6) en_sound(FFSND_ClothRip, 0.3f, (float) rint_(155, 175) / 100.0f);
        else if (en_event(s, "HeadThrow", "Shoot")) {
            s->i[2] = 1;
            en_sound(SOUND_SHAKEY_KID_ROAR, 1.0f, 0.9f); en_sound(SOUND_SHELLGAME, 0.8f, 0.8f);
            unsigned long h = en_spawn(160, red ? 773 : 771, p[0], p[1], 0, 0, e, 0);
            float tx, ty; float rx, ry;
            en_rand_vec(&rx, &ry, frand() * 120.0f); tx = t[0] + rx; ty = t[1] + ry; en_clamp_room(&tx, &ty, -10.0f); en_free_tile(&tx, &ty, 120.0f);
            if (!en_in_room(tx, ty, 10.0f)) {
                float x = p[0] - t[0], y = p[1] - t[1]; vresize(&x, &y, 10.0f); tx = t[0] + x; ty = t[1] + y; en_clamp_room(&tx, &ty, -10.0f); en_free_tile(&tx, &ty, 120.0f);
                if (!en_in_room(tx, ty, 10.0f)) { en_rand_vec(&rx, &ry, 15.0f); tx = p[0] + rx; ty = p[1] + ry; }
            }
            if (h) {
                en_t *hs = en_adopt(h); en_flag_clear(h, EF_FLAG_APPEAR);
                en_set_collision(h, ENTCOLL_NONE); en_set_gridcoll(h, GRIDCOLL_WALLS);
                hs->init = 1; hs->state = 1; hs->f[0] = p[0]; hs->f[1] = p[1]; hs->f[2] = tx; hs->f[3] = ty; hs->ref = e;
                en_replay(hs, "HeadSpin");
                *(float *) (h + 0x354) = fb_hp(e);                                   /* head.HitPoints = npc.HitPoints */
            }
            float x = tx - p[0], y = ty - p[1]; vresize(&x, &y, 7.0f); v[0] = x; v[1] = y; s->i[1] = 1;
            en_flip(e, !(tx > p[0]));
            *(float *) (e + 0x354) = *(const float *) (e + 0x358);                  /* npc.HitPoints = npc.MaxHitPoints */
        } else if (en_event(s, "HeadThrow", "Bomp")) s->i[1] = 0;
        else { en_play(s, "HeadThrow"); en_overlay_off(s); }
        if (!s->i[1]) { v[0] *= 0.1f; v[1] *= 0.1f; } else { v[0] *= 0.9f; v[1] *= 0.9f; }
    }
}
static void die_mrhorf(en_t *s, unsigned long e) {                              /* mrHorfKill: dies before the throw: becomes the Gusher/Pacer at once */
    if (s->i[2]) return;
    int nv = 1; if (rnd(3) == 0) nv = 0;
    float *p = fpos(e), *v = fvel(e);
    unsigned long n = en_spawn(11, nv, p[0], p[1], v[0], v[1], 0, s->variant);
    if (n) { *(float *) (n + 0x354) = *(const float *) (n + 0x358); en_flag_clear(n, EF_FLAG_APPEAR); if (en_flag(e, EF_FLAG_FRIENDLY)) en_flag_set(n, EF_FLAG_FRIENDLY); en_remove(e); }
}
/* head: state 1 = in the air (the arc), then 'Land' and Horf; f[0..1] start, f[2..3] target */
static void ai_mrhorfhead(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    int red = s->variant == 773, airtime = 15;
    if (!s->init) { s->init = 1; if (s->state != 1) { en_remove(e); return; } }              /* a head that was not thrown vanishes */
    if (s->state == 1) {
        en_play(s, "HeadSpin"); en_flip(e, v[0] > 0);
        float dx = s->f[2] - s->f[0], dy = s->f[3] - s->f[1], dist = vlen(dx, dy);
        float speed = dist / (float) airtime; if (speed > 10.0f) speed = 10.0f;
        float ux = dx, uy = dy; vresize(&ux, &uy, speed);
        p[0] = s->f[0] + ux * (float) s->age; p[1] = s->f[1] + uy * (float) s->age; v[0] = ux; v[1] = uy;
        ENLOG(18, 1, "ffenemy: not done: the head's arc (SpriteOffset is not measured)");
        if (s->age > airtime) { s->state = 0; en_set_collision(e, ENTCOLL_ALL); en_set_gridcoll(e, GRIDCOLL_GROUND); }
    } else {
        en_flip(e, 0); v[0] *= 0.1f; v[1] *= 0.1f;
        if (en_finished(s, "Land")) { unsigned long n = en_morph(e, red ? 160 : 12, red ? 630 : 0, 0); if (n && red) { en_t *ns = en_adopt(n); ns->f[0] = 95.0f; } }
        else en_play(s, "Land");
    }
}

/* ---- Mutant Horf 170.70: two orbiting projectiles that it renews (the orbit itself: w_proj_update in ffenemy.h) ---- */
/* i[0] first shot pending (1), ref/i: bullet pointers in f? -> kept in s->ref (bullet 1) and s->ref2 (bullet 2) */
static unsigned long en_exists(unsigned long x) { return x && alive(x) ? x : 0; }
static float wrap_angle(float a, float add) { if (a + add < 0) return 360.0f + (a + add); if (a + add > 360.0f) return (a + add) - 360.0f; return a + add; }
static void mutant_bullet(en_t *s, unsigned long e, int second) {
    float *p = fpos(e); unsigned long *slot = second ? &s->ref2 : &s->ref, *other = second ? &s->ref : &s->ref2;
    unsigned long b = en_spawn(9, 0, p[0], p[1], 0, second ? -5.0f : 5.0f, e, 0);
    if (!b) return;
    float off = second ? 180.0f : 0.0f;
    if (!s->i[0]) { pj_t *oj = en_exists(*other) ? pj_find(*other) : 0; if (oj) off = wrap_angle(oj->a, second ? 180.0f : -180.0f); }
    pj_add(b, e, PJ_MUTANT_ORBITAL, off);
    *slot = b;
}
static void ai_mutanthorf(en_t *s, unsigned long e) {
    float *v = fvel(e);
    if (!s->init) { s->init = 1; s->i[0] = 1; s->ref = s->ref2 = 0; en_replay(s, "Appear"); }
    if (en_finished(s, "Appear")) en_play(s, "DoubleAttack");
    if (en_finished(s, "Attack") || en_finished(s, "Attack2") || en_finished(s, "DoubleAttack")) en_play(s, "Shake");
    if (en_event(s, s->anim, "Shoot")) {
        en_sound(SOUND_SHAKEY_KID_ROAR, 1.0f, 1.0f);
        if (s->i[0] || !en_exists(s->ref)) mutant_bullet(s, e, 0);
        float *p = fpos(e); unsigned long ef = en_spawn(1000, 2, p[0], p[1], 0, 0, e, 5); (void) ef;
        ENLOG(19, 2, "ffenemy: not done: Mutant Horf's mouth effect SpriteOffset/DepthOffset/Color");
    }
    if (en_event(s, s->anim, "Shoot2")) {
        en_sound(SOUND_SHAKEY_KID_ROAR, 1.0f, 1.0f);
        if (s->i[0] || !en_exists(s->ref2)) mutant_bullet(s, e, 1);
        s->i[0] = 0;
        float *p = fpos(e); unsigned long ef = en_spawn(1000, 2, p[0], p[1], 0, 0, e, 5); (void) ef;
    }
    if (s->ref && s->ref2 && (!en_exists(s->ref) || !en_exists(s->ref2))) {
        if (!en_exists(s->ref) && !en_exists(s->ref2)) en_play(s, "DoubleAttack");
        else if (en_exists(s->ref)) en_play(s, "Attack2");
        else en_play(s, "Attack");
    }
    v[0] *= 0.5f; v[1] *= 0.5f;
}

static const enemy_t g_tab_g1[] = {
    { 160, 50, ai_morsel, 0, 0, 0 },
    { 160, 770, ai_mrhorf, die_mrhorf, 0, 0 },
    { 160, 772, ai_mrhorf, die_mrhorf, 0, 0 },
    { 160, 771, ai_mrhorfhead, 0, 0, 0 },
    { 160, 773, ai_mrhorfhead, 0, 0, 0 },
    { 170, 70, ai_mutanthorf, 0, 0, 0 },
};
#define N_TAB_G1 ((int) (sizeof g_tab_g1 / sizeof g_tab_g1[0]))
