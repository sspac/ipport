/* Group 2: Bumbler (erfly/enemies/bumbler.lua; Phoenix/Buckshot of the same file are later-floor variants), then Dung, Tall Boi, Shitling, Potluck, Poobottle */

/* the walls a flyer is about to hit: npc:CollidesWithGrid() (the next step lands in a wall) */
static int en_collides_grid(unsigned long e) { float *p = fpos(e), *v = fvel(e); return en_box(p[0] + v[0], p[1] + v[1], 1); }
static unsigned long en_player_ptr(void) { return g_first_player(); }

/* ---- Bumbler 160.700: s->dir = targrel (0 down 1 right-of-npc(Hori, flipped) 2 up 3 hori), s->cd = StateFrame, s->state: 0 idle 1 attack 2 travelling 3 collide; i[0] = d.shooting ---- */
static int bumbler_targrel(const float *p, const float *t, float leeway) {
    float dx = p[0] - t[0], dy = p[1] - t[1]; int r;
    if (fabsf(dx) > fabsf(dy)) r = dx < 0 ? 3 : 1; else r = dy < 0 ? 0 : 2;
    if (leeway > 0) { if (r % 2 == 0) { if (fabsf(fabsf(p[0]) - fabsf(t[0])) > leeway) return -1; } else if (fabsf(fabsf(p[1]) - fabsf(t[1])) > leeway) return -1; }
    return r;
}
static void ai_bumbler(en_t *s, unsigned long e) {
    static const char *sfx[4] = { "Down", "Hori", "Up", "Hori" }; static const int flipx[4] = { 0, 1, 0, 0 };
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    unsigned long pl = en_player_ptr(); float *tv = pl ? fvel(pl) : v;
    char an[40];
    if (!s->init) { s->init = 1; s->state = 0; en_set_gridcoll(e, GRIDCOLL_WALLS); } else s->cd++;
    ENLOG(20, 1, "ffenemy: not done: Bumbler's SpriteOffset(0,-10) (not measured), Skatole passivity and Beelzebub check");
    if (s->state == 0) {
        en_play(s, "Idle");
        float tx = t[0] - tv[0] * 40.0f, ty = t[1] - tv[1] * 40.0f;                     /* confusePos(): the target itself unless confused */
        float x = tx - p[0], y = ty - p[1]; vresize(&x, &y, 4.0f); if (en_scared(e)) { x = -x; y = -y; }
        v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
        if (s->cd > 30) {
            int tr = bumbler_targrel(p, t, 50.0f);
            if (vlen(p[0] - t[0], p[1] - t[1]) < 180.0f && tr >= 0 && !(en_scared(e) || en_confused(e))) { s->dir = tr; s->state = 1; s->cd = 0; }
        }
    } else if (s->state == 1) {
        snprintf(an, sizeof an, "Shoot%s", sfx[s->dir]);
        if (!s->i[0]) { v[0] *= 0.3f; v[1] *= 0.3f; }
        en_flip(e, flipx[s->dir]);
        if (en_finished(s, an)) s->state = 2;
        else if (en_playing(s, an) && s->af == 8) en_sound(SOUND_FART_GURG, 0.5f, (float) rint_(175, 185) / 100.0f);
        else if (en_event(s, an, "Shoot")) s->i[0] = 1;
        else en_play(s, an);
    } else if (s->state == 2) { snprintf(an, sizeof an, "Shoot%sLoop", sfx[s->dir]); en_play(s, an); }
    else {                                                                              /* collide */
        v[0] *= 0.2f; v[1] *= 0.2f; snprintf(an, sizeof an, "Shoot%sEnd", sfx[s->dir]);
        if (en_finished(s, an)) { s->state = 0; s->cd = 0; } else en_play(s, an);
    }
    if (s->i[0]) {
        float mx, my; vrot(0, -13.0f, (float) s->dir * 90.0f, &mx, &my);
        v[0] = lerpf(v[0], mx, 0.25f); v[1] = lerpf(v[1], my, 0.25f);
        if (s->cd % 3 == 1) {
            float vx, vy; en_sound(SOUND_BLOODSHOOT, 1.0f, (float) rint_(90, 110) / 100.0f);
            vrot(0, 10.0f, (float) (rint_(1, 20) - 10) + (float) s->dir * 90.0f, &vx, &vy);
            en_shoot(e, 0, p[0], p[1], vx, vy);
        }
        if (en_collides_grid(e)) {
            en_sound(SOUND_MEAT_IMPACTS, 1.0f, 1.0f);
            s->state = 3; s->i[0] = 0;
        } else if (s->cd > 200) { s->state = 0; s->cd = 0; s->i[0] = 0; }
    }
}

/* Room:GetRandomPosition(1): a random free tile's centre */
static void en_rand_pos(float *x, float *y) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
    for (int k = 0; k < 30; k++) { *x = en_room_x(1 + (int) rnd((unsigned) (w - 2))); *y = en_room_y(1 + (int) rnd((unsigned) (h - 2))); if (!en_xy(*x, *y, 0)) return; }
}
/* ---- Dung 160.10 (erfly/enemies/dung.lua clotterAI): s->state 4 wander, 8 shoot, 9 loop, 10 pant; cd = StateFrame, cd2 = waitTime, i[0] = keeploop, i[1] = attacking, f[0..1] = target ---- */
static void dung_corn(en_t *s, unsigned long e) {
    float *p = fpos(e); (void) s;
    en_sound(SOUND_PLOP, 0.5f, (float) rint_(8, 10) / 10.0f);
    float vx, vy; en_rand_vec(&vx, &vy, 5.0f);
    en_pp_t pp; memset(&pp, 0, sizeof pp); pp.variant = 5;
    pp.has_scale = 1; pp.scale = (float) rint_(11, 12) / 10.0f;
    pp.has_fspeed_mod = 1; pp.fspeed_mod = -15.0f - (float) rint_(1, 10) / 10.0f;
    pp.has_faccel_mod = 1; pp.faccel_mod = 1.4f + (float) rint_(1, 2) / 10.0f;
    pp.has_height_mod = 1; pp.height_mod = 20.0f;
    en_shoot_p(e, p[0], p[1], vx, vy, &pp);
}
static void ai_dung(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    float wx, wy;
    if (!s->init) { en_rand_pos(&s->f[0], &s->f[1]); s->state = 4; s->cd2 = rint_(1, 50); s->init = 1; ENLOG(21, 1, "ffenemy: not done: Dung's splat colour (SplatColor not measured)"); }
    else s->cd++;
    if (s->state == 4) {
        if (en_event(s, s->anim, "newpos")) {
            en_rand_pos(&s->f[0], &s->f[1]); if (en_scared(e)) { s->f[0] = 2 * p[0] - s->f[0]; s->f[1] = 2 * p[1] - s->f[1]; en_clamp_room(&s->f[0], &s->f[1], 0); }
            v[0] = v[1] = 0; en_flip(e, !(s->f[0] > p[0]));
        } else if (en_event(s, s->anim, "scoot")) {
            if (en_line_clear(p[0], p[1], s->f[0], s->f[1], 0)) { float x = s->f[0] - p[0], y = s->f[1] - p[1]; vresize(&x, &y, 7.0f); v[0] = x; v[1] = y; }
            else en_walk_to(e, s->f, 7.0f, 3.0f, 1);
            en_flip(e, !(v[0] > 0));
        }
        if (en_line_clear(p[0], p[1], s->f[0], s->f[1], 0) || path_step(p[0], p[1], s->f[0], s->f[1], 0, &wx, &wy)) en_play(s, "Hop"); else en_play(s, "Idle");
        v[0] *= 0.95f; v[1] *= 0.95f;
        if (rnd(3) == 0 && s->cd > 25 + s->cd2 && !en_scared(e)) { en_flip(e, t[0] < p[0]); s->state = 8; }
    } else if (s->state == 8) {
        v[0] = v[1] = 0;
        if (en_finished(s, "ShootFull")) { s->i[0] = rint_(1, 10) + 20; s->state = 9; }
        else if (en_event(s, "ShootFull", "startattack")) { s->cd = 0; s->i[1] = 1; }
        else if (en_event(s, "ShootFull", "shoot")) dung_corn(s, e);
        else en_play(s, "ShootFull");
    } else if (s->state == 9) {
        v[0] = v[1] = 0; en_play(s, "ShootLoop");
        if (s->cd > s->i[0] || en_scared(e)) { s->state = 10; s->cd = 0; s->i[1] = 0; }
    } else if (s->state == 10) {
        v[0] = v[1] = 0;
        if (s->cd > 75) { en_rand_pos(&s->f[0], &s->f[1]); s->cd2 = rint_(1, 50); s->cd = 0; s->state = 4; } else en_play(s, "Pant");
    }
    if (s->i[1] && s->cd % 2 == 1) {
        en_sound(SOUND_BLOODSHOOT, 1.0f, 1.3f);
        float x = (t[0] - p[0]) * 0.03f, y = (t[1] - p[1]) * 0.03f, rx, ry; vrot(x, y, (float) (-10 + rint_(1, 20)), &rx, &ry);
        en_pp_t pp; memset(&pp, 0, sizeof pp); pp.variant = 3;
        pp.has_scale = 1; pp.scale = (float) rint_(2, 10) / 10.0f;
        pp.has_fspeed_mod = 1; pp.fspeed_mod = -50.0f + (float) rint_(1, 10);
        pp.has_faccel_mod = 1; pp.faccel_mod = 1.4f + (float) rint_(1, 2) / 10.0f;
        pp.has_height_mod = 1; pp.height_mod = 20.0f;
        en_shoot_p(e, p[0], p[1], rx, ry, &pp);
    }
}

static int en_count(int type, int variant) {                                    /* mod.GetEntityCount(type, variant) */
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); int c = 0;
    for (unsigned k = 0; k < n; k++) if (alive(snap[k]) && *(int *) (snap[k] + 0x38) == type && *(int *) (snap[k] + 0x3c) == variant) c++;
    return c;
}
/* mod:FindRandomValidPathPosition(npc, 2, 60): a free tile >= 150 from the npc, not within 60 of the player, reachable */
static void en_far_pos(unsigned long e, float *ox, float *oy) {
    float *p = fpos(e), *t = en_target(e), wx, wy, x = p[0], y = p[1];
    for (int k = 0; k < 40; k++) {
        float cx, cy; en_rand_pos(&cx, &cy);
        if (vlen(cx - p[0], cy - p[1]) < 150.0f || (t && vlen(cx - t[0], cy - t[1]) < 60.0f)) continue;
        if (!en_line_clear(p[0], p[1], cx, cy, 0) && !path_step(p[0], p[1], cx, cy, 0, &wx, &wy)) continue;
        x = cx; y = cy; break;
    }
    *ox = x; *oy = y;
}
/* ---- Tall Boi 160.500 (tallboi.lua tallBoiAI): s->i[2] 0 idle 1 sing 2 defend 3 run; i[0] walking, i[1] rot, i[3] spawnedShitCount, f[2] orbcount, f[0..1] target, cd = StateFrame, ref = 2nd shitling ---- */
static void tall_sing_start(en_t *s) { s->state = 1; s->f[2] = 0; }
static void ai_tallboi(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    const int maxshit = 7; float dist = vlen(t[0] - p[0], t[1] - p[1]);
    if (!s->init) { s->init = 1; s->state = 0; s->f[2] = 0; s->i[3] = 0; ENLOG(22, 1, "ffenemy: not done: Tall Boi splat colour"); } else s->cd++;
    v[0] *= 0.9f; v[1] *= 0.9f;
    if (s->state == 0) {
        if (!s->i[0]) {
            en_play(s, "Idle");
            if ((s->cd > 50 && rnd(21) == 0) || (en_confused(e) && s->cd > 10 + s->i[3] * 30)) { s->i[0] = 1; en_far_pos(e, &s->f[0], &s->f[1]); s->cd = 0; }
        } else {
            en_play(s, "Walk");
            if (en_event(s, "Walk", "Waddle")) {
                en_walk_to(e, s->f, 0.6f, 0.6f, 0);
                float x, y; vrot(v[0], v[1], s->i[1] ? 30.0f : -30.0f, &x, &y); v[0] = x; v[1] = y; s->i[1] = !s->i[1];
            }
            if (vlen(p[0] - s->f[0], p[1] - s->f[1]) < 20.0f || s->cd > 50) { s->i[0] = 0; s->cd = 0; }
        }
        en_flip(e, v[0] < 0);
        s->f[2] += 0.5f;
        if (dist < 100.0f + s->f[2] || en_scared(e)) {
            if (en_count(160, 501) < maxshit && !(en_scared(e) || en_confused(e)) && s->cd > s->i[3] * 30) tall_sing_start(s);
            else { s->state = 3; s->cd = 0; }
        }
    } else if (s->state == 1) {
        if (en_finished(s, "Sing")) s->state = 2;
        else if (en_event(s, "Sing", "Whistle")) {
            en_sound(SOUND_WHISTLE, 1.0f, 1.3f); s->i[4] = 1; s->i[3]++; s->ref = 0;
            float vx = t[0] - p[0], vy = t[1] - p[1]; vresize(&vx, &vy, 30.0f);
            for (int k = 0, a = -50; a <= 50; a += 50, k++) {
                float rx, ry, px, py; vrot(vx, vy, (float) a, &rx, &ry); px = p[0] + rx; py = p[1] + ry;
                if (vlen(px - t[0], py - t[1]) < 50.0f) { float qx = vx, qy = vy; vresize(&qx, &qy, 10.0f); vrot(qx, qy, (float) a, &rx, &ry); px = p[0] + rx; py = p[1] + ry; }
                else { float ex = px - p[0], ey = py - p[1]; vresize(&ex, &ey, 15.0f); px += ex; py += ey; }
                unsigned long sh = en_spawn(160, 501, px, py, 0, 0, e, 0);
                if (k == 1) s->ref = sh;
            }
        } else en_play(s, "Sing");
        if (!s->i[4]) en_flip(e, !(t[0] > p[0]));
    } else if (s->state == 2) {
        en_play(s, "Idle"); en_flip(e, !(t[0] > p[0]));
        int lost = 0;
        if (s->ref) { if (en_dead(s->ref)) lost = 1; } else lost = 1;
        s->f[2] += 0.4f;
        if (dist < 75.0f + s->f[2]) lost = 1;
        if (lost) { s->state = 3; en_sound(SOUND_BABY_HURT, 1.0f, 0.7f); s->cd = 0; }
    } else {
        en_play(s, "Walk"); en_flip(e, !(v[0] > 0));
        if (en_event(s, "Walk", "Waddle")) {
            float x = p[0] - t[0], y = p[1] - t[1]; vresize(&x, &y, 3.0f); float rx, ry; vrot(x, y, s->i[1] ? 30.0f : -30.0f, &rx, &ry); s->i[1] = !s->i[1];
            v[0] = lerpf(v[0], rx, 0.4f); v[1] = lerpf(v[1], ry, 0.4f);
        }
        if (s->cd > 75) {
            if (en_count(160, 501) < maxshit && !(en_scared(e) || en_confused(e)) && dist < 100.0f && s->cd > s->i[3] * 50) tall_sing_start(s);
            if (s->cd > 150 || en_collides_grid(e)) { s->state = 0; s->f[2] = 0; s->cd = 0; s->i[0] = 0; }
        }
    }
}
static void hurt_tallboi(en_t *s, unsigned long e, float dmg) { (void) e; (void) dmg; if (s->state == 0) tall_sing_start(s); }
/* ---- Shitling 160.501 (tallboi.lua shitlingAI): state 0 idle, 1 spit ---- */
static void ai_shitling(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; s->state = 0; ENLOG(23, 1, "ffenemy: not done: Shitling's no-knockback flags (flag bits not measured)"); } else { s->cd++; v[0] = v[1] = 0; }
    if (t[0] > p[0] + 40.0f) en_flip(e, 0); else if (t[0] < p[0] - 40.0f) en_flip(e, 1);
    if (en_event(s, s->anim, "spit")) {
        en_sound(SOUND_LITTLE_SPIT, 1.0f, 1.3f);
        en_pp_t pp; memset(&pp, 0, sizeof pp); pp.variant = 3; pp.has_height_mod = 1; pp.height_mod = 15.0f; pp.has_scale = 1; pp.scale = 0.7f;
        float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 7.0f); en_shoot_p(e, p[0], p[1], x, y, &pp);
    }
    if (s->state == 0) {
        en_play(s, "Idle");
        if (s->cd > 10 && rnd(20) == 0 && vlen(t[0] - p[0], t[1] - p[1]) < 200.0f && en_line_clear(t[0], t[1], p[0], p[1], 0) && !(en_scared(e) || en_confused(e))) s->state = 1;
    } else {
        if (en_finished(s, "Spit")) { s->state = 0; s->cd = 0; } else en_play(s, "Spit");
    }
}

/* ---- Poobottle 160.560 (poobottle.lua poobottleAI). The poop grid entities (GetGridEntityFromPos / Desc.Type / Hurt) are NOT measured in this binary, so the "no poop found" path is
 *      what runs: it hovers ('Fly') for 15 frames, then flies away, fading and growing, and is removed outside the room. s->state 0 idle, 1 flyaway; cd = StateFrame; f[0..1] = d.vec; f[2] = ColorVal ---- */
static void ai_poobottle(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    if (!s->init) { s->init = 1; s->state = 0; s->f[2] = 1.0f; en_set_collision(e, ENTCOLL_NONE); } else s->cd++;
    ENLOG(24, 1, "ffenemy: not done: Poobottle/Drainfly poop interaction (grid entity API not measured; it flies away as when no poop exists), SpriteOffset, Color");
    if (s->state == 0) {
        en_play(s, "Fly");
        if (en_scared(e)) { s->state = 1; s->cd = 0; }
        else { v[0] *= 0.9f; v[1] *= 0.9f; if (s->cd > 15) { s->state = 1; s->cd = 0; } }
    } else {
        en_set_gridcoll(e, 0);
        if (!s->i[0]) { en_rand_vec(&s->f[0], &s->f[1], 15.0f); s->i[0] = 1; }
        v[0] = lerpf(v[0], s->f[0], 0.03f); v[1] = lerpf(v[1], s->f[1], 0.03f);
        s->f[2] = lerpf(s->f[2], 0.0f, 0.02f);
        int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
        if (p[0] < -100.0f || p[0] > (float) w * 40.0f + 100.0f || p[1] > (float) h * 40.0f + 300.0f || p[1] < -300.0f) en_remove(e);
        if (s->cd > 10) en_set_can_shut_doors(e, 0);
    }
}
/* ---- Potluck 451.60 (potluck.lua PotluckAI). Urn ("pot") grid entities are NOT measured, so no pot is ever found: the Lua's "no pot" branch runs (flees from the player at speed 8, removed 500px away) ---- */
static void ai_potluck(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; s->state = 0; }
    ENLOG(25, 1, "ffenemy: not done: Potluck's pot search/enter/shoot cycle (urn grid entities not measured); only the no-pot flee branch runs");
    en_play(s, "Idle01");
    float x = p[0] - t[0], y = p[1] - t[1]; vresize(&x, &y, 8.0f);
    v[0] = lerpf(v[0], x, 0.1f); v[1] = lerpf(v[1], y, 0.1f);
    if (vlen(p[0] - t[0], p[1] - t[1]) > 500.0f) en_remove(e);
}
static const enemy_t g_tab_g2[] = {
    { 160, 700, ai_bumbler, 0, 0, 0 },
    { 160, 10, ai_dung, 0, 0, 0 },
    { 160, 500, ai_tallboi, 0, hurt_tallboi, 0 },
    { 160, 501, ai_shitling, 0, 0, 0 },
    { 160, 560, ai_poobottle, 0, 0, 0, 1 },
    { 451, 60, ai_potluck, 0, 0, 0, 1 },
};
#define N_TAB_G2 ((int) (sizeof g_tab_g2 / sizeof g_tab_g2[0]))
