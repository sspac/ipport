/* Group 4: Slammer 151.0 / Wimpy 151.1 (erfly/enemies/slammer.lua), Drumstick 15.450, Cheeky 451.350 */

/* Room:FindFreeTilePosition(pos, 40): the centre of the nearest free tile (the tile of pos when it is free) */
static void en_free_tile_pos(float *x, float *y) {
    en_clamp_room(x, y, 0);
    if (!en_xy(*x, *y, 0)) return;
    for (int r = 1; r < 8; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (abs(dx) != r && abs(dy) != r) continue;
        float cx = *x + 40.0f * (float) dx, cy = *y + 40.0f * (float) dy; en_clamp_room(&cx, &cy, 0);
        if (!en_xy(cx, cy, 0)) { *x = cx; *y = cy; return; }
    }
}
/* ---- Slammer: s->state 0 idle (State 3) 1 jump (8) 2 fall (9); i[0] IsJumping, i[1] Wait, i[2] waitrng, i[3] JumpTimer, f[0..1] PlayerPos, f[2..3] targetVelocity ---- */
static void ai_slammer(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return; int var = s->variant;
    if (s->i[0] == 1) {
        s->i[3]++; float dx = s->f[0] - p[0], dy = s->f[1] - p[1], dist = vlen(dx, dy);
        ENLOG(32, 1, "ffenemy: not done: Slammer Friction=1 while jumping, FRIENDLY/Marzy/Doomer/glacier variants");
        if (dist < 20.0f || s->i[3] > 60) {
            float x = dx, y = dy; vresize(&x, &y, 5.0f); s->f[2] = x; s->f[3] = y;
            if (var == 0) { s->state = 2; s->i[0] = 0; s->f[2] = s->f[3] = 0; }
        } else if (var == 0 && dist < 40.0f) { float x = dx, y = dy; vresize(&x, &y, 2.0f); s->f[2] = x; s->f[3] = y; }
        else {
            float nx = dx, ny = dy; vresize(&nx, &ny, 1.0f); float k = var == 0 ? dist / 65.0f : dist / 20.0f;
            s->f[2] += nx * k; s->f[3] += ny * k;
            if (vlen(s->f[2], s->f[3]) >= dist) vresize(&s->f[2], &s->f[3], dist);
        }
    } else { s->f[2] = s->f[3] = 0; s->i[3] = 0; }
    if (!s->init) { s->init = 1; s->i[1] = 0; s->state = 0; }
    else if (s->state == 0) {
        en_set_gridcoll(e, GRIDCOLL_GROUND);
        if (s->i[1] == 0) { en_play(s, "Idle"); s->i[2] = var == 1 ? rint_(1, 10) + 15 : rint_(1, 10); }
        s->i[1]++;
        if (!(en_scared(e) || en_confused(e)) && s->i[1] > (var == 1 ? 1 : 10) + s->i[2]) { s->state = 1; s->i[1] = 0; }
    } else if (s->state == 1) {
        if (en_finished(s, "Jump")) { if (var > 0 && var < 5) { s->state = 2; s->i[0] = 0; s->f[2] = s->f[3] = 0; } }
        else if (!en_playing(s, "Jump")) en_play(s, "Jump");
        else if (en_event(s, "Jump", "GetPlayer")) {
            float dx = t[0] - p[0], dy = t[1] - p[1], d = vlen(dx, dy), m = var == 1 ? 200.0f : 300.0f;
            float x = dx, y = dy; vresize(&x, &y, d < m ? d : m); x += p[0]; y += p[1]; en_free_tile_pos(&x, &y);
            float rx, ry; en_rand_vec(&rx, &ry, 1.0f); s->f[0] = x + rx; s->f[1] = y + ry;
            s->i[0] = 1; en_set_collision(e, ENTCOLL_NONE); en_set_gridcoll(e, GRIDCOLL_NONE);
        } else if (en_event(s, "Jump", "Sound")) en_sound(SOUND_MEAT_JUMPS, 1.0f, var == 1 ? 1.4f : 1.0f);
    } else if (s->state == 2) {
        if (en_finished(s, "Fall")) s->state = 0; else if (!en_playing(s, "Fall")) en_play(s, "Fall");
        if (en_event(s, "Fall", "Land")) {
            en_sound(var == 1 ? SOUND_MEAT_JUMPS : SOUND_MEAT_IMPACTS, 1.0f, var == 1 ? 1.4f : 1.0f);
            en_set_collision(e, ENTCOLL_ALL); en_set_gridcoll(e, GRIDCOLL_GROUND);
        }
    }
    v[0] = s->f[2] * 0.3f + v[0] * 0.6f; v[1] = s->f[3] * 0.3f + v[1] * 0.6f;
}

/* ---- mod:WanderAbout (contestant.lua): s->i[1] has a walk position f[0..1], s->i[2] = the age it was last used; true while it is on its way ---- */
static int en_wander(en_t *s, unsigned long e, float speed, float idletime, float movelerp) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e);
    if (s->i[1] && !(s->i[2] && s->age - s->i[2] > 10)) {
        float vx = 0, vy = 0; int has = 0; s->i[2] = s->age ? s->age : 1;
        if (en_scared(e)) { vx = p[0] - t[0]; vy = p[1] - t[1]; if (vlen(vx, vy) > speed) vresize(&vx, &vy, speed); has = 1; s->i[1] = 0; }
        else if (en_line_clear(p[0], p[1], s->f[0], s->f[1], 0)) { vx = s->f[0] - p[0]; vy = s->f[1] - p[1]; if (vlen(vx, vy) > speed) vresize(&vx, &vy, speed); has = 1; }
        else if (path_step(p[0], p[1], s->f[0], s->f[1], 0, &vx, &vy)) { float wx = vx - p[0], wy = vy - p[1]; vresize(&wx, &wy, speed * 0.1f + 0.2f); v[0] += wx; v[1] += wy; has = 0; }
        else s->i[1] = 0;
        if (has) { v[0] = lerpf(v[0], vx, movelerp); v[1] = lerpf(v[1], vy, movelerp); }
        if (s->i[1] && vlen(p[0] - s->f[0], p[1] - s->f[1]) <= 10.0f) { if (frand() <= idletime) s->i[1] = 0; return 0; }
        return 1;
    }
    s->i[2] = s->age ? s->age : 1; en_rand_pos(&s->f[0], &s->f[1]); s->i[1] = 1; return 1;
}
/* ---- Cheeky 451.350: s->state 0 idle 1 shoot; cd = StateFrame; i[0] = Moving ---- */
static void ai_cheeky(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; s->state = 0; s->i[0] = 1; s->cd = rint_(30, 45); ENLOG(33, 1, "ffenemy: not done: Cheeky's poop grid entity on death (GridSpawn not measured), the Dip's splat colour and Dip particles' colour"); }
    if (s->state == 0) {
        if (en_wander(s, e, 2.0f, 0.02f, 0.3f)) {
            en_play(s, "Walk");
            if (s->i[0]) en_flip(e, v[0] > 0); else { v[0] *= 0.2f; v[1] *= 0.2f; }
            if (en_event(s, "Walk", "Start")) s->i[0] = 1;
            else if (en_event(s, "Walk", "Stop")) { s->i[0] = 0; en_sound(SOUND_SCAMPER, 0.4f, 0.2f); }
        } else { s->i[0] = 1; v[0] *= 0.5f; v[1] *= 0.5f; en_play(s, "Idle"); }
        s->cd--;
        if (s->cd <= 0) {
            if (en_count(217, 0) < en_count(451, 350) * 3 && !0) s->state = 1; else s->cd = rint_(30, 45);
        }
    } else {
        v[0] *= 0.5f; v[1] *= 0.5f;
        int target_ok = vlen(t[0] - p[0], t[1] - p[1]) <= 200.0f && en_line_clear(p[0], p[1], t[0], t[1], 0);
        if (en_finished(s, "Shoot")) { s->state = 0; s->i[0] = 1; s->cd = rint_(60, 120); }
        else if (en_event(s, "Shoot", "Sound")) { en_sound(SOUND_MEAT_JUMPS, 1.0f, 1.0f); if (target_ok) en_flip(e, t[0] > p[0]); }
        else if (en_event(s, "Shoot", "Shoot")) {
            float vx, vy; en_rand_vec(&vx, &vy, 5.0f);
            if (target_ok) { vx = t[0] - p[0]; vy = t[1] - p[1]; vresize(&vx, &vy, 5.0f); }
            float ox = vx, oy = vy; vresize(&ox, &oy, 20.0f);
            unsigned long d = en_spawn(217, 0, p[0] + ox, p[1] + oy, vx, vy, e, 0);
            if (d) { en_flag_clear(d, EF_FLAG_APPEAR); *(int *) (d + OFF_NPC_STATE) = STATE_MOVE; }
            en_flip(e, vx > 0);
            for (int k = 0; k < 3; k++) { float sp = (float) rint_(3, 6), rx, ry; vrot(vx, vy, (float) rint_(-20, 20), &rx, &ry); vresize(&rx, &ry, sp); en_spawn(1000, EV_POOP_PARTICLE, p[0], p[1], rx, ry, e, 0); }
            en_sound(SOUND_PLOP, 1.0f, 1.0f); en_sound(SOUND_FART, 1.0f, 1.0f);
        } else en_play(s, "Shoot");
    }
}
static void die_cheeky(en_t *s, unsigned long e) { (void) s; (void) e; en_sound(SOUND_PLOP, 1.0f, 1.0f); }

/* ---- Drumstick 15.450: the game's own Clotty AI runs (npc.State); this is the Lua around it. cd = Cooldown, i[0] = was attacking ---- */
static void ai_drumstick(en_t *s, unsigned long e) {
    float *p = fpos(e); int *st = (int *) (e + OFF_NPC_STATE);
    if (!s->init) { s->init = 1; s->cd = rint_(15, 45); ENLOG(34, 1, "ffenemy: not done: Drumstick TinyShoot timing is emulated from the NPC state (the Sprite frame is not read), burnt/holy variants, projectile mode 6 assumed to be a 4-way cross"); }
    if (*st == 8) {
        if (s->cd > 0) *st = 4;
    } else s->cd--;
    if (*st == 8) {
        if (!s->i[0]) { s->i[0] = 1; strcpy(s->anim, "Attack"); s->af = 0; const ffa_t *a = ffa_find(s->key, "Attack"); s->alen = a ? a->len : 0; s->aloop = 0; }
    } else s->i[0] = 0;
    if (s->i[0] && en_event(s, "Attack", "TinyShoot")) {
        en_pp_t pp; memset(&pp, 0, sizeof pp); pp.has_scale = 1; pp.scale = 0.5f; pp.has_height_mod = 1; pp.height_mod = 10.0f;
        for (int a = 0; a < 360; a += 90) { float x, y; vrot(6.0f, 0, (float) a, &x, &y); en_shoot_p(e, p[0], p[1], x, y, &pp); }
        en_spawn(1000, 2, p[0], p[1] - 10.0f, 0, 0, e, 1);
        en_sound(SOUND_ANIMAL_SQUISH, 1.0f, 1.5f);
    } else if (s->i[0] && en_finished(s, "Attack")) { s->cd = rint_(15, 45); s->i[0] = 0; }
}
static void die_drumstick(en_t *s, unsigned long e) {
    float *p = fpos(e), *t = en_target(e); (void) s; if (!t) return;
    en_pp_t pp; memset(&pp, 0, sizeof pp); pp.variant = 1; pp.has_fspeed_mod = 1; pp.fspeed_mod = -20.0f; pp.has_faccel_mod = 1; pp.faccel_mod = 1.5f;
    float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 5.0f); en_shoot_p(e, p[0], p[1], x, y, &pp);
    en_sound(SOUND_PLOP, 0.7f, 1.5f);
}
static const enemy_t g_tab_g4[] = {
    { 151, 0, ai_slammer, 0, 0, 0 },
    { 151, 1, ai_slammer, 0, 0, 0 },
    { 15, 450, ai_drumstick, die_drumstick, 0, 0 },
    { 451, 350, ai_cheeky, die_cheeky, 0, 0 },
};
#define N_TAB_G4 ((int) (sizeof g_tab_g4 / sizeof g_tab_g4[0]))
