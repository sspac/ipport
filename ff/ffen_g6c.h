/* ffen_g6c.h - Burning Basement, part C: Chili 451.150, Litling 451.151, Trailblazer 451.100 (+ flame segments), Fire Fly 451.270 (guwah/enemies/chili.lua, trailblazer.lua, firefly.lua).
 * Included by ffenemy.h after ffen_g6.h. s->cd = StateFrame; the slot use of each AI is noted above it. */

/* ------------------------------------------------------------------------------------------ small pieces */
/* mod:FindRandomValidPathPosition(npc, mode, avoidplayer): en_vpos plus the player filter (no cell closer than `avoid` to the player) */
static int g6c_vpos(unsigned long e, int mode, float avoid, float *ox, float *oy) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9; const float *p = fpos(e), *t = en_target(e);
    float ax[15 * 9 + 16], ay[15 * 9 + 16], mx[15 * 9 + 16], my[15 * 9 + 16]; int na = 0, nm = 0;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
        float x = en_room_x(c), y = en_room_y(r);
        if (en_xy(x, y, 0) || !en_in_room_walk(x, y, 0.0f)) continue;
        if (!en_line_clear(p[0], p[1], x, y, 0)) continue;
        if (avoid > 0.0f && t && vlen(x - t[0], y - t[1]) < avoid) continue;
        float d = vlen(x - p[0], y - p[1]);
        if (na < (int) (sizeof ax / sizeof ax[0])) { ax[na] = x; ay[na] = y; na++; }
        int ok = (mode == 2 && d > 150.0f) || (mode == 3 && d > 30.0f && d < 80.0f);
        if (ok && nm < (int) (sizeof mx / sizeof mx[0])) { mx[nm] = x; my[nm] = y; nm++; }
    }
    if (nm) { int k = (int) rnd((unsigned) nm); *ox = mx[k]; *oy = my[k]; return 1; }
    if (na) { int k = (int) rnd((unsigned) na); *ox = ax[k]; *oy = ay[k]; return 1; }
    *ox = p[0]; *oy = p[1]; return 1;
}
/* room:CheckLine(a, b, 0, ...) second result: the last point of the line that is still free */
static void g6c_line_end(float x0, float y0, float x1, float y1, float *ox, float *oy) {
    float dx = x1 - x0, dy = y1 - y0, d = vlen(dx, dy); int steps = (int) (d / 10.0f) + 1; *ox = x0; *oy = y0;
    for (int k = 1; k <= steps; k++) { float x = x0 + dx * k / steps, y = y0 + dy * k / steps; if (en_box(x, y, 0)) return; *ox = x; *oy = y; }
}
static int g6c_exists(unsigned long c) {                                       /* a pointer we kept is still an entity of the room */
    if (!c) return 0;
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) if (snap[k] == c) return *(const unsigned char *) (c + E_EXISTS) != 0;
    return 0;
}
static unsigned long g6c_nearest(const float *p, int type, int variant) {
    unsigned long best = 0; float bd = 10000.0f; unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) { unsigned long v = snap[k]; if (!alive(v) || *(const int *) (v + E_TYPE) != type || *(const int *) (v + E_VARIANT) != variant) continue;
        float d = vlen(fpos(v)[0] - p[0], fpos(v)[1] - p[1]); if (d < bd) { bd = d; best = v; } }
    return best;
}
static fire_t *g6c_fire_of(unsigned long e) { for (int k = 0; k < FIRE_N; k++) if (g_gfire[k].e == e) return &g_gfire[k]; return 0; }
static void g6c_splat(unsigned long e, int charred, float scale) {            /* Isaac.Spawn(1000, 7, 0, pos), splat.Color = ColorChili / ColorCharred, splat:Update() */
    float *p = fpos(e); unsigned long b = en_spawn(1000, EV_BLOOD_SPLAT, p[0], p[1], 0, 0, e, 0); if (!b) return;
    if (charred) en_paint(b, 0.1f, 0.1f, 0.1f, 1, 0, 0, 0, 0, 0, 0, 0); else en_paint(b, 0, 0, 0, 1, 0, 0, 0, 0, 80.0f / 255.0f, 35.0f / 255.0f, 30.0f / 255.0f);
    if (scale != 1.0f) en_scale(b, scale, scale);
}
static float g6c_hp(unsigned long e) { return g_hp_off ? *(const float *) (e + g_hp_off) : 0.0f; }

/* ------------------------------------------------------------------------------------------ the Litling's flame
 * The Lua keeps a second Sprite (monster_litling.anm2 "Flame") and renders it under the Litling at 0.5 + (StateFrame/FuseTimer)/2 of its scale. The port has no second render:
 * an effect (1000.7014, which the game has no code for) loads that anm2, follows the Litling and is scaled the same way. */
typedef struct { unsigned long lit, fx; int age; } lf_t;
static lf_t g_lf[16];
static void g6c_lf_start(unsigned long lit) {
    for (int k = 0; k < 16; k++) if (!g_lf[k].lit || !alive(g_lf[k].lit)) {
        if (g_lf[k].fx && alive(g_lf[k].fx)) en_remove(g_lf[k].fx);
        const float *p = fpos(lit); unsigned long f = en_spawn(1000, 7014, p[0], p[1], 0, 0, lit, 2); if (!f) return;
        if (g_load) g_load((void *) (f + E_SPRITE), "gfx/enemies/chili/monster_litling.anm2"); else ENLOG(232, 1, "ffenemy: not done: Sprite::Load is not installed: the Litling's flame");
        if (g_play) g_play((void *) (f + E_SPRITE), "Flame", 1);
        g_lf[k].lit = lit; g_lf[k].fx = f; g_lf[k].age = 0; return;
    }
}
static void g6c_lf_stop(unsigned long lit) {
    for (int k = 0; k < 16; k++) if (g_lf[k].lit == lit) { if (g_lf[k].fx && alive(g_lf[k].fx)) en_remove(g_lf[k].fx); g_lf[k].lit = 0; g_lf[k].fx = 0; }
}
static void en_g6c_tick(void) {
    for (int k = 0; k < 16; k++) {
        lf_t *l = &g_lf[k]; if (!l->lit) continue;
        if (!alive(l->lit) || !g_lf[k].fx || !alive(l->fx)) { if (l->fx && alive(l->fx)) en_remove(l->fx); l->lit = 0; l->fx = 0; continue; }
        en_t *s = en_slot(l->lit); if (s->e != l->lit) continue;
        float *fp = fpos(l->fx), *lp = fpos(l->lit); fp[0] = lp[0]; fp[1] = lp[1]; fvel(l->fx)[0] = 0; fvel(l->fx)[1] = 0;
        float fuse = s->i[0] > 0 ? (float) s->i[0] : 1.0f, sc = 0.5f + ((float) s->cd / fuse) / 2.0f; if (sc < 0) sc = 0;
        const float *ls = (const float *) (l->lit + E_SPRITE + 0xdc); en_scale(l->fx, ls[0] * sc, ls[1] * sc);
        const float *c = (const float *) (l->lit + E_SPRITE + 0xe8); en_paint(l->fx, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10]);
        int applen = en_anim_len((451u << 20) | (151u << 4), "Appear"); if (applen <= 0) applen = 10;
        en_visible(l->fx, !(en_flag(l->lit, EF_FLAG_APPEAR) && (s->age < 5 || s->age < applen)));
        int len = en_anim_len((451u << 20) | (151u << 4), "Flame"); if (len <= 0) len = 12;
        if (g_setframe) g_setframe((void *) (l->fx + E_SPRITE), "Flame", l->age % len);
        if (!(en_flag(l->lit, EF_FLAG_APPEAR) && s->age < applen)) l->age++;                  /* data.FireSprite:Update() only when not playing "Appear" */
    }
}

/* ------------------------------------------------------------------------------------------ Chili 451.150 (chili.lua ChiliAI)
 * state 0 Idle / 1 Attack; cd = StateFrame; i[0] = d.WalkPos set, f[0..1] = d.WalkPos */
static void ai_chili(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t0 = en_target(e), tp[2]; if (!t0) return;
    en_confuse_pos(s, e, t0, tp);
    if (!s->init) {
        s->init = 1; ENLOG(230, 1, "ffenemy: not done: the Chili/Litling splat colour (SplatColor) and sprite overlay render priority");
        s->cd = rint_(30, 60); s->state = 0;
    }
    if (s->state == 0) {
        en_play(s, "Idle"); v[0] *= 0.9f; v[1] *= 0.9f;
        if (!s->i[0]) { g6c_vpos(e, 2, 60.0f, &s->f[0], &s->f[1]); s->i[0] = 1; }
        else if (en_event(s, "Idle", "Move")) {
            if (en_scare(e) || (vlen(tp[0] - p[0], tp[1] - p[1]) <= 100.0f && en_line_clear(p[0], p[1], tp[0], tp[1], 0))) {
                v[0] = p[0] - tp[0]; v[1] = p[1] - tp[1]; vresize(&v[0], &v[1], 3.0f); s->i[0] = 0;
            } else { float w[2] = { s->f[0], s->f[1] }; en_walk_to(e, w, 0.8f, 0.8f, 0); }
            en_flip(e, v[0] < 0);
        } else if (vlen(p[0] - s->f[0], p[1] - s->f[1]) <= 20.0f) s->i[0] = 0;
        s->cd--;
        if (s->cd <= 0 && en_line_clear(p[0], p[1], tp[0], tp[1], 0) && vlen(tp[0] - p[0], tp[1] - p[1]) <= 300.0f
            && en_count_type_var(451, 151) < en_count_type_var(451, 150) * 2) { s->state = 1; en_flip(e, p[0] > tp[0]); }
    } else if (s->state == 1) {
        v[0] *= 0.9f; v[1] *= 0.9f;
        if (en_finished(s, "Attack")) { s->cd = rint_(60, 90); s->state = 0; }
        else if (en_event(s, "Attack", "Sound")) en_sound(SOUND_MEAT_JUMPS, 0.7f, 1.0f);
        else if (en_event(s, "Attack", "Shoot")) {
            en_sound(SOUND_WORM_SPIT, 0.8f, 1.0f); en_sound(SOUND_BEAST_LAVABALL_RISE, 0.8f, 1.0f);
            float vx = tp[0] - p[0], vy = tp[1] - p[1]; vresize(&vx, &vy, 10.0f);
            unsigned long l = en_spawn(451, 151, p[0] + vx * 2.0f, p[1] + vy * 2.0f, vx, vy, e, 0);
            if (l) {
                en_flag_clear(l, EF_FLAG_APPEAR);
                en_t *ls = en_adopt(l);
                if (ls) { ls->state = 1; ls->i[0] = rint_(60, 120); ls->i[1] = 1; en_replay(ls, "Move"); en_set_frame(ls, "Move", 7); }
                en_flip(l, vx < 0);
            }
            en_flip(e, p[0] > tp[0]);
        } else en_play(s, "Attack");
    }
    en_overlay(s, "Flame");
    if (s->age % 5 == 1) g6c_splat(e, 0, 1.0f);
}

/* ------------------------------------------------------------------------------------------ Litling 451.151 (chili.lua LitlingAI)
 * state 0 Idle / 1 Move; cd = StateFrame (counts down from the fuse); i[0] = d.FuseTimer (0 = nil), i[1] = d.State was preset by the Chili, i[2] = it already blew up (the timer ran out) */
static void ai_litling(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t0 = en_target(e), tp[2]; if (!t0) return;
    en_confuse_pos(s, e, t0, tp);
    if (!s->init) {
        s->init = 1; if (s->i[0] <= 0) s->i[0] = rint_(150, 210);
        s->cd = s->i[0]; if (!s->i[1]) s->state = 0;
        g6c_lf_start(e);
    }
    if (s->state == 0) {
        float rx, ry; en_rand_vec(&rx, &ry, 3.0f); v[0] = lerpf(v[0], rx, 0.05f); v[1] = lerpf(v[1], ry, 0.05f);
        en_play(s, "Idle");
        if (frand() <= 0.2f + 0.8f * (1.0f - (float) s->cd / (float) s->i[0])) s->state = 1;
    } else {
        v[0] *= 0.85f; v[1] *= 0.85f;
        if (en_finished(s, "Move")) s->state = 0;
        else if (en_event(s, "Move", "Move")) {
            if (vlen(tp[0] - p[0], tp[1] - p[1]) <= 250.0f && en_line_clear(p[0], p[1], tp[0], tp[1], 0)) {
                float dx = en_scare(e) ? p[0] - tp[0] : tp[0] - p[0], dy = en_scare(e) ? p[1] - tp[1] : tp[1] - p[1];
                vresize(&dx, &dy, 11.0f); vrot(dx, dy, (float) rint_(-30, 30), &v[0], &v[1]);
            } else {
                unsigned long ch = g6c_nearest(p, 451, 150);
                if (ch && vlen(fpos(ch)[0] - p[0], fpos(ch)[1] - p[1]) < 100.0f) {
                    for (int i = 1; i <= 5; i++) {                                            /* try to avoid the Chili */
                        float ax = p[0] - fpos(ch)[0], ay = p[1] - fpos(ch)[1], rx, ry, mx, my; vresize(&ax, &ay, 100.0f); vrot(ax, ay, (float) rint_(-30, 30), &rx, &ry);
                        g6c_line_end(p[0], p[1], p[0] + rx, p[1] + ry, &mx, &my);
                        if (vlen(mx - p[0], my - p[1]) > 80.0f) { v[0] = rx; v[1] = ry; vresize(&v[0], &v[1], 10.0f); }
                        else if (i == 5) { float qx, qy; en_vpos(e, 0, &qx, &qy); float dx = qx - p[0], dy = qy - p[1]; vresize(&dx, &dy, 10.0f); vrot(dx, dy, (float) rint_(-30, 30), &v[0], &v[1]); }
                    }
                } else { float qx, qy; en_vpos(e, 0, &qx, &qy); float dx = qx - p[0], dy = qy - p[1]; vresize(&dx, &dy, 10.0f); vrot(dx, dy, (float) rint_(-30, 30), &v[0], &v[1]); }
            }
            en_flip(e, v[0] < 0);
        } else en_play(s, "Move");
    }
    s->cd--;
    if (s->age % 3 == 1) g6c_splat(e, 1, 0.4f);
    /* DieOnPlayerCollision (MC_PRE_NPC_COLLISION): the port has no collision callback, so the touch is measured */
    { unsigned long pl = g_first_player ? g_first_player() : 0;
      if (pl && vlen(fpos(pl)[0] - p[0], fpos(pl)[1] - p[1]) < *(const float *) (pl + 0x344) + *(const float *) (e + 0x344)) { en_kill(e); return; } }
    if (s->cd <= 0) {                                                                         /* the fuse ran out: it blows up and kills itself */
        en_explode(e, p[0], p[1], 1.0f, 0.5f, 1);
        en_spawn(33, 10, p[0], p[1], 0, 0, e, 0);
        s->i[2] = 1; en_kill(e);
    }
}
static void die_litling(en_t *s, unsigned long e) {                                          /* npc:IsDead() and shouldDoDeathEffect: the same blast, the flame with 0.66 of its HP */
    float *p = fpos(e);
    g6c_lf_stop(e);
    if (s->i[2]) return;
    en_explode(e, p[0], p[1], 1.0f, 0.5f, 1);
    unsigned long f = en_spawn(33, 10, p[0], p[1], 0, 0, e, 0);
    if (f && g_hp_off) *(float *) (f + g_hp_off) *= 0.66f;
}

/* ------------------------------------------------------------------------------------------ Trailblazer 451.100 (trailblazer.lua)
 * The head (i[0] = 0) spawns three segments (ref, ref2, ref3). A segment (i[0] = 1) has ref = the head, i[1] = d.MoveDelay, i[2] = its animation (0 Body1, 1 Body2, 2 Tail).
 * Head: state 0 Idle / 1 Charge; cd = StateFrame; f[0..1] = d.newhome (i[1] = set), f[2..3] = d.ChargeVec. Segment: f[4] = d.SwayFrame, f[5..6] = its SpriteOffset. */
typedef struct { unsigned long e; int tag[32]; float x[32], y[32]; } tbl_t;
static tbl_t g_tbl[8];
static tbl_t *g6c_log_of(unsigned long head, int create) {
    for (int k = 0; k < 8; k++) if (g_tbl[k].e == head) return &g_tbl[k];
    if (!create) return 0;
    for (int k = 0; k < 8; k++) if (!g_tbl[k].e || !alive(g_tbl[k].e)) { memset(&g_tbl[k], 0, sizeof g_tbl[k]); for (int q = 0; q < 32; q++) g_tbl[k].tag[q] = -1; g_tbl[k].e = head; return &g_tbl[k]; }
    return 0;
}
/* mod:GetNewPosAligned(pos, true): a random cell on the four straight lines from pos (every 40 px, up to 25 cells) that a flyer can be in */
static void g6c_aligned_pos(const float *pos, float *ox, float *oy) {
    float px[110], py[110]; int n = 0;
    for (int i = 1; i <= 4; i++) {
        float vx, vy; vrot(0, 40.0f, 90.0f * (float) i, &vx, &vy);
        for (int dist = 1; ; dist++) {
            float x = pos[0] + vx * (float) dist, y = pos[1] + vy * (float) dist;
            if (en_xy(x, y, 1) || dist > 25) break;
            if (n < 110) { px[n] = x; py[n] = y; n++; }
        }
    }
    if (n) { int k = rint_(1, n) - 1; *ox = px[k]; *oy = py[k]; } else { *ox = pos[0]; *oy = pos[1]; }
}
static const char *g6c_move_str(float x, float y) { if (fabsf(y) > fabsf(x)) return y > 0 ? "Down" : "Up"; return x > 0 ? "Right" : "Left"; }   /* mod:GetMoveString(vec) */
static void g6c_snap90(float x, float y, float *ox, float *oy) {                                  /* mod:SnapVector(vec, 90) */
    float a = en_angle_deg(x, y), sn = floorf((a + 45.0f) / 90.0f) * 90.0f; vrot(x, y, sn - a, ox, oy);
}
static void ai_trailblazer(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    s->key &= ~15u;                                                                              /* the flame segments (sub 1) use the head's animations */
    if (!s->init) {
        s->init = 1;
        ENLOG(231, 1, "ffenemy: not done: Trailblazer parts ignoring each other's collision (TrailblazerColl), status effects copied head -> segments, segment DepthOffset and the flame segment's SplatColor");
        if (!s->i[0]) {
            unsigned long *refs[3] = { &s->ref, &s->ref2, &s->ref3 };
            for (int i = 1; i <= 3; i++) {
                unsigned long b = en_spawn(451, 100, p[0], p[1], 0, 0, e, i > 1 ? 1 : 0); *refs[i - 1] = b; if (!b) continue;
                if (g_set_parent) g_set_parent(b, e);
                en_flag_clear(b, EF_FLAG_APPEAR);
                en_t *bs = en_adopt(b); if (bs) { bs->i[0] = 1; bs->i[1] = i * 3; bs->i[2] = i - 1; bs->ref = e; }
            }
            en_play(s, "HeadDown"); g6c_log_of(e, 1);
        }
        s->cd = 0; s->f[4] = 0; s->state = 0;
    }
    if (s->i[0]) {                                                                               /* a segment */
        unsigned long par = s->ref && g6c_exists(s->ref) && !en_dead(s->ref) ? s->ref : 0;
        if (!par) { en_kill(e); return; }
        tbl_t *lg = g6c_log_of(par, 0); int idx = s->age - s->i[1];
        if (lg && idx >= 0 && lg->tag[idx & 31] == idx) { v[0] = lg->x[idx & 31] - p[0]; v[1] = lg->y[idx & 31] - p[1]; }
        memcpy((float *) (e + E_SPRITE + 0xe8), (const float *) (par + E_SPRITE + 0xe8), 11 * sizeof(float));
        static const char *an[3] = { "Body1", "Body2", "Tail" }; en_play(s, an[s->i[2] < 3 ? s->i[2] : 0]);
        /* TrailblazerRender: the segments sway across their direction of travel */
        s->f[4] += 0.1f;
        float sway = sinf(s->f[4] + ((float) s->i[1] / 2.0f) * 3.14159265f) * (2.0f + (float) s->i[1] / 5.0f);
        float tx = fabsf(v[0]) > fabsf(v[1]) ? 0.0f : sway, ty = fabsf(v[0]) > fabsf(v[1]) ? sway : 0.0f;
        s->f[5] = lerpf(s->f[5], tx, 0.25f); s->f[6] = lerpf(s->f[6], ty, 0.25f); en_sprite_offset(s, e, s->f[5], s->f[6]);
        return;
    }
    unsigned long segs[3] = { s->ref, s->ref2, s->ref3 };
    for (int i = 0; i < 3; i++) if (segs[i] && (!g6c_exists(segs[i]) || en_dead(segs[i]))) { en_kill(e); break; }
    if (s->state == 0) {
        if (!s->i[1]) { g6c_aligned_pos(p, &s->f[0], &s->f[1]); s->i[1] = 1; }
        if (vlen(p[0] - s->f[0], p[1] - s->f[1]) < 20.0f || vlen(v[0], v[1]) < 0.3f || (en_flag(e, EF_FLAG_CONFUSION | EF_FLAG_FEAR | EF_FLAG_SHRINK) && s->cd % 10 == 0)) g6c_aligned_pos(p, &s->f[0], &s->f[1]);
        float tx = s->f[0] - p[0], ty = s->f[1] - p[1]; vresize(&tx, &ty, 5.0f);
        v[0] = lerpf(v[0], tx, 0.3f); v[1] = lerpf(v[1], ty, 0.3f);
        s->cd--;
        if (s->age > 0) { char an[16]; snprintf(an, sizeof an, "Head%s", g6c_move_str(v[0], v[1])); en_play(s, an); }
    } else {
        char an[16]; snprintf(an, sizeof an, "Charge%s", g6c_move_str(s->f[2], s->f[3]));
        if (en_finished(s, an)) { s->cd = 30; s->i[1] = 0; s->state = 0; }
        else if (en_event(s, an, "Shoot")) {
            en_sound(FFSND_FireLight, 1.0f, 1.0f); en_sound(SOUND_MONSTER_ROAR_0, 1.0f, 0.5f);
            v[0] = s->f[2]; v[1] = s->f[3];
        } else en_play(s, an);
        if (en_was_event(s, an, "Shoot")) {
            v[0] *= 0.95f; v[1] *= 0.95f;
            if (s->af % 2 == 0) {
                float rx, ry; en_rand_vec(&rx, &ry, (float) rint_(0, 5));
                fire_t *f = en_fire_new(e, p[0] + rx, p[1] + ry, 0, 0, 20);
                if (f) { f->timer = 120; f->scale = (float) rint_(50, 75) * 0.01f; f->parent = e; en_fire_go(f); }
            }
        } else { v[0] *= 0.6f; v[1] *= 0.6f; }
    }
    tbl_t *lg = g6c_log_of(e, 1); if (lg) { lg->tag[s->age & 31] = s->age; lg->x[s->age & 31] = p[0]; lg->y[s->age & 31] = p[1]; }
}
static void hurt_trailblazer(en_t *s, unsigned long e, float dmg) {
    s->hurt_block = 0;
    unsigned long long fl = g_hit_flags; unsigned long src = g_hit_src;
    if (en_fire_from_not_player()) { s->hurt_block = 1; return; }
    if (!s->init) return;
    if (!s->i[0]) {                                                                              /* head: the segments take the same hit (DAMAGE_CLONES), then it may charge */
        unsigned long segs[3] = { s->ref, s->ref2, s->ref3 };
        for (int i = 0; i < 3; i++) if (segs[i] && g6c_exists(segs[i]) && !en_dead(segs[i])) hit_enemy_f(segs[i], dmg, fl | 256ull, src);
        if (s->state == 0 && s->cd <= 0 && !(g6c_hp(e) - dmg <= 0.0f)) {
            float *v = fvel(e), sx, sy; g6c_snap90(v[0], v[1], &sx, &sy); vresize(&sx, &sy, 1.0f);
            float ux, uy, lx, ly; vrot(sx, sy, 90.0f, &ux, &uy); vresize(&ux, &uy, 80.0f); vrot(sx, sy, -90.0f, &lx, &ly); vresize(&lx, &ly, 80.0f);
            const float *p = fpos(e); float angle;
            if (!en_line_clear(p[0], p[1], p[0] + ux, p[1] + uy, 1)) angle = -90.0f;
            else if (!en_line_clear(p[0], p[1], p[0] + lx, p[1] + ly, 1)) angle = 90.0f;
            else angle = frand() <= 0.5f ? 90.0f : -90.0f;
            float cx, cy; vrot(sx, sy, angle, &cx, &cy); vresize(&cx, &cy, 12.0f); s->f[2] = cx; s->f[3] = cy;
            s->state = 1; en_sound(SOUND_FLAME_BURST, 1.0f, 1.0f);
        }
    } else if (!(fl & 256ull)) {                                                                 /* a segment: the head takes it, the segment does not */
        unsigned long par = s->ref && g6c_exists(s->ref) ? s->ref : 0;
        if (par) hit_enemy_f(par, dmg, fl, src);
        s->hurt_block = 1;
    }
}

/* ------------------------------------------------------------------------------------------ Fire Fly 451.270 (firefly.lua)
 * state 0 Idle / 1 Suck / 2 Whiff / 3 Full / 4 Shoot; s->ref = npc.Child (the flame it goes for); i[0] = d.FullOfFire, i[1] = FireData.Type, i[2] = FireData.Variant,
 * i[3] = d.WalkPos set, f[0..1] = d.WalkPos */
static int g6c_fire_valid(unsigned long c, int allow_fading) {                                   /* mod:IsFireValid */
    if (!c || !g6c_exists(c)) return 0;
    int t = *(const int *) (c + E_TYPE), var = *(const int *) (c + E_VARIANT);
    if (t == 33) return (en_get_state(c) != 3 || var >= 10) && var != 11;
    if (t == 1000 && var == 7005) { fire_t *f = g6c_fire_of(c); return allow_fading || !f || !(f->age > f->timer); }
    return 0;
}
static unsigned long g6c_find_flame(const float *p) {                                           /* mod:FindFireFlyFlame */
    float dist = 9999.0f; unsigned long flame = 0, snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) { unsigned long v = snap[k]; if (!alive(v) || *(const int *) (v + E_TYPE) != 33) continue;
        float d = vlen(fpos(v)[0] - p[0], fpos(v)[1] - p[1]); if (d < dist && g6c_fire_valid(v, 0)) { flame = v; dist = d; } }
    for (unsigned k = 0; k < n; k++) { unsigned long v = snap[k]; if (!alive(v) || *(const int *) (v + E_TYPE) != 1000 || *(const int *) (v + E_VARIANT) != 7005) continue;
        float d = vlen(fpos(v)[0] - p[0], fpos(v)[1] - p[1]); if (d < dist && vlen(fvel(v)[0], fvel(v)[1]) < 5.0f) { flame = v; dist = d; } }
    return flame;
}
static void ai_firefly(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t0 = en_target(e), tp[2]; if (!t0) return;
    en_confuse_pos(s, e, t0, tp);
    if (!s->init) { s->init = 1; s->state = 0; }
    const unsigned long long NOKB = EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_PHYSICS_KNOCKBACK;
    if (s->state == 0) {
        en_play(s, "Idle");
        if (g6c_fire_valid(s->ref, 0)) {
            const float *cp = fpos(s->ref); float dx = cp[0] - p[0], dy = cp[1] - p[1]; vresize(&dx, &dy, 5.0f);
            v[0] = lerpf(v[0], dx, 0.1f); v[1] = lerpf(v[1], dy, 0.1f);
            if (vlen(p[0] - cp[0], p[1] - cp[1]) <= 20.0f) { en_flag_set(e, NOKB); s->state = 1; }
        } else {
            s->ref = g6c_find_flame(p);
            /* mod:WanderAboutAir(npc, data, 5, 0.1, 60, 120) */
            if (s->i[3]) {
                float vx = 0, vy = 0; int have = 0;
                if (en_scare(e)) { vx = p[0] - tp[0]; vy = p[1] - tp[1]; vresize(&vx, &vy, 5.0f); have = 1; s->i[3] = 0; }
                else if (vlen(s->f[0] - p[0], s->f[1] - p[1]) <= 10.0f) s->i[3] = 0;
                else { vx = s->f[0] - p[0]; vy = s->f[1] - p[1]; vresize(&vx, &vy, 5.0f); have = 1; }
                if (have) { v[0] = lerpf(v[0], vx, 0.1f); v[1] = lerpf(v[1], vy, 0.1f); }
            } else { en_air_pos(tp, 60.0f, 120.0f, &s->f[0], &s->f[1]); s->i[3] = 1; }
        }
    } else if (s->state == 1) {
        v[0] *= 0.5f; v[1] *= 0.5f;
        if (en_finished(s, "FireSuck")) s->state = 3;
        else if (en_event(s, "FireSuck", "Shoot")) {
            if (!g6c_fire_valid(s->ref, 1)) { en_flag_clear(e, NOKB); en_replay(s, "FireSuckWhiff"); s->state = 2; }
            else { s->i[1] = *(const int *) (s->ref + E_TYPE); s->i[2] = *(const int *) (s->ref + E_VARIANT); s->i[0] = 1; fvel(s->ref)[0] = 0; fvel(s->ref)[1] = 0; }
            en_sound(FFSND_BaloonShort, 2.0f, 1.0f);
        } else if (en_event(s, "FireSuck", "Stop")) en_flag_clear(e, NOKB);
        else en_play(s, "FireSuck");
        if (s->state == 1 && en_was_event(s, "FireSuck", "Shoot") && !en_was_event(s, "FireSuck", "Stop") && s->age % 2 == 0 && g6c_fire_valid(s->ref, 0)) {
            if (*(const int *) (s->ref + E_TYPE) == 33 && s->age % 4 == 0) {
                int var = *(const int *) (s->ref + E_VARIANT);
                hit_enemy_f(s->ref, 1.0f, (var == 2 || var == 3 || var == 12 || var == 13) ? DMG_EXPLOSION : 0ull, e);
            } else if (*(const int *) (s->ref + E_TYPE) == 1000 && *(const int *) (s->ref + E_VARIANT) == 7005) {   /* gunpowder creep fire: it shrinks by 0.1 */
                fire_t *f = g6c_fire_of(s->ref); if (f) f->scale -= 0.1f;
            }
        }
    } else if (s->state == 2) {
        v[0] *= 0.5f; v[1] *= 0.5f;
        if (en_finished(s, "FireSuckWhiff")) s->state = 0; else en_play(s, "FireSuckWhiff");
    } else if (s->state == 3) {
        en_play(s, "Idle2");
        if (vlen(p[0] - tp[0], p[1] - tp[1]) <= 160.0f && en_line_clear(p[0], p[1], tp[0], tp[1], 1)) s->state = 4;
        else { float dx = tp[0] - p[0], dy = tp[1] - p[1]; vresize(&dx, &dy, 3.0f); v[0] = lerpf(v[0], dx, 0.3f); v[1] = lerpf(v[1], dy, 0.3f); }
    } else {
        v[0] *= 0.8f; v[1] *= 0.8f;
        if (en_finished(s, "Shoot")) s->state = 0;
        else if (en_event(s, "Shoot", "Shoot")) { en_sound(FFSND_FriedStart, 0.8f, 1.0f); en_sound(SOUND_FLAME_BURST, 1.0f, 0.5f); s->i[0] = 0; }
        else if (en_event(s, "Shoot", "Stop")) en_sound(SOUND_CANDLE_LIGHT, 1.0f, 1.5f);
        else en_play(s, "Shoot");
        if (s->state == 4 && en_was_event(s, "Shoot", "Shoot") && !en_was_event(s, "Shoot", "Stop") && s->age % 2 == 0) {
            const char *sheet = 0; int bullets = 0; en_pp_t pp; memset(&pp, 0, sizeof pp);
            if (s->i[1] == 33) {
                int var = s->i[2];
                if (var == 1) { sheet = "gfx/effects/effect_005_fire_red.png"; bullets = 1; }
                else if (var == 2 || var == 12) sheet = "gfx/effects/effect_005_fire_blue.png";
                else if (var == 3 || var == 13) { sheet = "gfx/effects/effect_005_fire_purple.png"; bullets = 1; pp.flags = PF_SMART; }
            }
            float fx = tp[0] - p[0], fy = tp[1] - p[1]; vresize(&fx, &fy, 8.0f); float rx, ry; vrot(fx, fy, (float) rint_(-15, 15), &rx, &ry);
            en_flame_shot(e, p[0], p[1], rx, ry, &pp, sheet);
            if (bullets) {
                en_pp_t bp; memset(&bp, 0, sizeof bp); bp.has_faccel_mod = 1; bp.faccel_mod = 0.6f;
                bp.has_fspeed_mod = 1; bp.fspeed_mod = (float) rint_(-8, -5);
                bp.has_scale = 1; bp.scale = (float) rint_(16, 30) * 0.05f;
                float sp = (float) rint_(5, 12); int ang = rint_(-20, 20);
                float bx = tp[0] - p[0], by = tp[1] - p[1]; vresize(&bx, &by, sp); float qx, qy; vrot(bx, by, (float) ang, &qx, &qy);
                if (s->i[2] == 3 || s->i[2] == 13) bp.flags = PF_SMART;
                unsigned long pr = en_shoot_p(e, p[0], p[1], qx, qy, &bp);
                if (pr && s->i[2] == 1) en_proj_watch2(pr, PW_REDCREEP, e, 0, 0);               /* creeptype "red": a red creep where it lands */
            }
        }
    }
}
static void die_firefly(en_t *s, unsigned long e) {                                              /* it was full of fire: a fireplace is left behind */
    if (!s->i[0]) return;
    int firevar = (s->i[2] == 2 || s->i[2] == 12) ? 12 : 10;
    const float *p = fpos(e); en_spawn(33, firevar, p[0], p[1], 0, 0, e, 0);
}

static const enemy_t g_tab_g6c[] = {
    { 451, 150, ai_chili, 0, 0, 0, 0, 3 },
    { 451, 151, ai_litling, die_litling, 0, 0, 0, 3 },
    { 451, 100, ai_trailblazer, 0, hurt_trailblazer, 0, 0, 3 },
    { 451, 270, ai_firefly, die_firefly, 0, 0, 0, 3 },
};
#define N_TAB_G6C ((int) (sizeof g_tab_g6c / sizeof g_tab_g6c[0]))
