/* Group 3: Slim 160.820.0 / Limb 160.820.2 (erfly/enemies/slim.lua), Bowler 160.760.0 / Loafer 160.760.1 (bowler.lua), Weaver 155.0.0 (weaver.lua), Tap 160.490 (tap.lua) */

/* Room:GetGridCollisionAtPos: 0 free ... (no grid known: everything is free) */
static int en_tile(float x, float y) { return (g_gw || g_grid_class) ? fb_tile(x, y) : 0; }
/* ---- Slim / Limb ---- */
static const char *const SLIM_DIR[5] = { "Right", "Left", "Down", "Up", "UpRightDown" };
static int slim_dir_of(float x, float y) { return fabsf(x) > fabsf(y) ? (x > 0 ? 0 : 1) : (y > 0 ? 2 : 3); }
static void snap90(float *x, float *y) { float l = vlen(*x, *y), a = en_angle(*x, *y); a = 90.0f * floorf(a / 90.0f + 0.5f); *x = cosf(a / 57.29578f) * l; *y = sinf(a / 57.29578f) * l; }
/* SlimArmHurt (the plain Slim): the arm snaps, state OW */
static void slim_armhurt(en_t *s) { en_sound(SOUND_BONE_SNAP, 0.6f, (float) rint_(8, 12) / 10.0f); s->i[2] = 1; s->state = 3; s->i[1] = 0; }
static void ai_limb(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; if (s->state != 4) s->state = 0; s->cd = 0; if (rnd(2) == 0) en_flip(e, 1); ENLOG(26, 1, "ffenemy: not done: Limb SpriteOffset(0,-1), Pale/Bone limb variants"); } else s->cd++;
    if (s->state == 0) {
        v[0] *= 0.8f; v[1] *= 0.8f;
        if (!en_playing(s, "Idle2")) { en_play(s, "Idle"); if (rnd(50) == 0) en_play(s, "Idle2"); }
        if (vlen(t[0] - p[0], t[1] - p[1]) < 120.0f && s->cd > 30) { s->state = 1; s->cd = 0; }
    } else if (s->state == 1) {
        v[0] *= 0.9f; v[1] *= 0.9f;
        if (en_finished(s, "Leap")) { s->state = 0; s->cd = 0; }
        else if (en_event(s, "Leap", "Jump")) {
            float x = t[0] - p[0], y = t[1] - p[1], rx, ry; vresize(&x, &y, 10.0f); vrot(x, y, (float) (-60 + rint_(1, 120)), &rx, &ry); v[0] = rx; v[1] = ry; en_flip(e, !(v[0] > 0));
        } else if (en_event(s, "Leap", "Land")) { v[0] *= 0.5f; v[1] *= 0.5f; }
        else en_play(s, "Leap");
    } else {                                                                              /* drop (state 4) */
        v[0] *= 0.9f; v[1] *= 0.9f;
        if (en_finished(s, "Spawn")) s->state = 0;
        else if (en_event(s, "Spawn", "Land")) { v[0] *= 0.5f; v[1] *= 0.5f; }
        else en_play(s, "Spawn");
    }
}
static void ai_slim(en_t *s, unsigned long e) {
    if (s->sub == 2) { ai_limb(s, e); return; }
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    char an[40]; float dist = vlen(t[0] - p[0], t[1] - p[1]);
    if (!s->init) { s->init = 1; s->state = 0; } else s->cd++;
    if (s->state == 0) {
        if (vlen(v[0], v[1]) > 0.05f) { s->dir = slim_dir_of(v[0], v[1]); snprintf(an, sizeof an, "Walk %s", SLIM_DIR[s->dir]); en_play(s, an); s->i[0] = 0; }
        else if (s->sub == 1) en_set_frame(s, "Walk Down", 18);
        else if (s->i[0]) { if (en_finished(s, "Idle2")) s->i[0] = 0; else en_play(s, "Idle2"); }
        else { en_play(s, "Idle"); if (rnd(150) == 0) s->i[0] = 1; }
        if (s->sub == 1 || en_confused(e)) {                                            /* wandering */
            if (s->cd > 160 || !s->i[3]) { en_rand_pos(&s->f[3], &s->f[4]); s->i[3] = 1; s->cd = 0; }
            if (vlen(p[0] - s->f[3], p[1] - s->f[4]) > 30.0f) {
                if (en_line_clear(p[0], p[1], s->f[3], s->f[4], 0) || en_scared(e)) {
                    float x = s->f[3] - p[0], y = s->f[4] - p[1]; vresize(&x, &y, 2.0f); if (en_scared(e)) { x = -x; y = -y; }
                    v[0] = lerpf(v[0], x, 0.2f); v[1] = lerpf(v[1], y, 0.2f);
                } else en_walk_to(e, &s->f[3], 0.25f, 0.25f, 0);
            } else { v[0] *= 0.8f; v[1] *= 0.8f; s->cd += 2; }
        } else {
            if (en_line_clear(p[0], p[1], t[0], t[1], 0) || en_scared(e)) {
                float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 3.0f); if (en_scared(e)) { x = -x; y = -y; }
                v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
            } else en_walk_to(e, t, 0.4f, 0.4f, 0);
        }
        if (!(en_scared(e) || en_confused(e)) && ((s->sub != 1 && dist < 90.0f) || (s->sub == 1 && rnd(20) == 0)) && s->cd > 25) {
            float x = t[0] - p[0], y = t[1] - p[1];
            if (s->sub == 1) en_rand_vec(&x, &y, 1.0f);
            vresize(&x, &y, 25.0f); snap90(&x, &y); s->f[0] = x; s->f[1] = y; s->dir = slim_dir_of(x, y);
            s->state = 1; en_sound(FFSND_WingFlap, 1.0f, (float) rint_(120, 130) / 100.0f);
            float ax = x, ay = y; vresize(&ax, &ay, 5.5f); v[0] += ax; v[1] += ay; s->i[0] = 0;
            ENLOG(27, 1, "ffenemy: not done: Slim's dust puff effect (FFWhiteSmoke), player-type exceptions of shouldPlayerSlapHurt, spiked rock/poop punch branches (grid entity types not measured)");
        }
    } else if (s->state == 1) {
        snprintf(an, sizeof an, "Strike %s", SLIM_DIR[s->dir]);
        if (en_was_event(s, an, "Hit")) { v[0] *= 0.5f; v[1] *= 0.5f; } else { v[0] *= 0.9f; v[1] *= 0.9f; }
        if (en_finished(s, an)) { s->state = 0; s->cd = 0; }
        else if (en_event(s, an, "Hit")) {
            v[0] *= 0.5f; v[1] *= 0.5f;
            float qx = s->f[0], qy = s->f[1]; vresize(&qx, &qy, 40.0f);
            if (en_tile(p[0] + qx, p[1] + qy) > 1) slim_armhurt(s);
            else {
                int hit = 0; unsigned long pl = en_player_ptr();
                if (pl && vlen(fpos(pl)[0] - (p[0] + s->f[0]), fpos(pl)[1] - (p[1] + s->f[1])) < 40.0f) {
                    en_sound(FFSND_EpicPunch, 1.0f, 1.0f); hit = 1; hit_enemy(pl, 1.0f);
                    float sx = s->f[0], sy = s->f[1]; vresize(&sx, &sy, 2.0f); fvel(pl)[0] += sx; fvel(pl)[1] += sy;
                }
                if (!hit) en_sound(SOUND_SHELLGAME, 1.0f, (float) rint_(65, 75) / 100.0f);
            }
        } else en_play(s, an);
    } else if (s->state == 3) {                                                          /* OW */
        v[0] *= 0.1f; v[1] *= 0.1f;
        if (!s->i[1]) {
            snprintf(an, sizeof an, "Ouch %s", SLIM_DIR[s->dir]);
            if (en_finished(s, an)) { s->i[1] = 1; s->cd = 0; if (s->dir != 1) { s->dir = 4; s->f[2] = 15.0f; } else s->f[2] = -15.0f; }
            else en_play(s, an);
        } else if (s->i[1] == 1) {
            snprintf(an, sizeof an, "Hurt %s", SLIM_DIR[s->dir]); en_play(s, an);
            if (s->cd % 4 == 1) {
                float x, y; en_rand_vec(&x, &y, 3.0f);
                en_pp_t pp; memset(&pp, 0, sizeof pp); pp.has_scale = 1; pp.scale = (float) rint_(7, 9) / 10.0f;
                pp.has_fspeed_mod = 1; pp.fspeed_mod = -15.0f - (float) rint_(1, 10) / 10.0f; pp.has_faccel_mod = 1; pp.faccel_mod = 1.4f + (float) rint_(1, 2) / 10.0f;
                en_shoot_p(e, p[0] + s->f[2], p[1] + 1.0f, x, y, &pp);
            }
            if (s->cd > 50) s->i[1] = 2;
        } else {
            snprintf(an, sizeof an, "Recover %s", SLIM_DIR[s->dir]);
            if (en_finished(s, an)) { s->state = 0; s->cd = 0; s->i[1] = 0; } else en_play(s, an);
        }
    }
}
static void die_slim(en_t *s, unsigned long e) {                                         /* npc:IsDead(): 1 in 5 (1 in 8 once its arm was hurt) drops a Limb */
    if (s->sub == 2) return;
    if (rint_(1, s->i[2] ? 8 : 5) != 1) return;
    float *p = fpos(e), vx, vy; en_rand_vec(&vx, &vy, 10.0f);
    unsigned long l = en_spawn(160, 820, p[0], p[1], vx, vy, e, 2);
    if (l) { en_flag_clear(l, EF_FLAG_APPEAR); en_t *ls = en_adopt(l); if (ls) { ls->state = 4; } }
}

/* ---- Bowler 160.760.0 / Loafer 160.760.1 (headless) / ball 160.760.2 (bowler.lua). s->state 0 appear 1 idle 2 throwheadstart 3 throwhead 4 pickup;
 *      i[0] headstate, f[0] headhealth, i[1] moving, i[2] thrown, i[3] hoppin, i[4] picked, i[5] throwdir, i[6] has randir, f[1..2] vec, f[3..4] randir, dir 0 Down 1 Up 2 Hori ---- */
static const char *const BOWL_DIR[3] = { "Down", "Up", "Hori" };
static unsigned long bowler_closest_head(unsigned long e) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); unsigned long best = 0; float bd = 1e30f, *p = fpos(e);
    for (unsigned k = 0; k < n; k++) { unsigned long c = snap[k]; if (c == e || !alive(c)) continue;
        if (*(int *) (c + 0x38) != 160 || *(int *) (c + 0x3c) != 760 || *(int *) (c + 0x40) != 2) continue;
        float d = vlen(fpos(c)[0] - p[0], fpos(c)[1] - p[1]); if (d < bd) { bd = d; best = c; } }
    return best;
}
static void bowler_headless(en_t *s) { s->state = 1; s->i[0] = 2; s->dir = 0; }
static void ai_bowlerhead(en_t *s, unsigned long e);
static void ai_bowler(en_t *s, unsigned long e) {
    if (s->sub == 2) { ai_bowlerhead(s, e); return; }
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    char an[40];
    if (!s->init) { s->state = 0; s->i[0] = s->sub == 1 ? 2 : 1; snprintf(an, sizeof an, "WalkDown0%d", s->i[0]); en_set_frame(s, an, 0); s->f[0] = *(float *) (e + 0x358); s->cd = 40; s->init = 1;
        ENLOG(28, 1, "ffenemy: not done: Bowler BloodExplode on losing the head, RolyPolyRoll looping sound, SpriteOffset"); } else s->cd++;
    if (s->state == 0) { snprintf(an, sizeof an, "WalkDown0%d", s->i[0]); en_set_frame(s, an, 0); s->state = 1; s->cd = 0; }
    else if (s->state == 1) {
        int sframe = s->cd % 32;
        if (s->i[0] == 1 && s->f[0] < 0) { s->i[0] = 2; }
        if (s->i[1]) {
            float speed = 4.0f; unsigned long tt = 0; float *tp = t;
            if (s->i[0] == 2 && !(en_scared(e) || en_confused(e))) {
                speed = 5.0f; tt = bowler_closest_head(e); tp = tt ? fpos(tt) : 0;
                if (tt && vlen(p[0] - tp[0], p[1] - tp[1]) < 20.0f) { s->state = 4; s->ref = tt; en_t *hs = en_adopt(tt); if (hs) hs->ref = e; }
            }
            if (tp && !en_confused(e)) {
                if (en_line_clear(p[0], p[1], tp[0], tp[1], 0) || en_scared(e)) {
                    float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, speed); if (en_scared(e)) { x = -x; y = -y; }
                    v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
                } else if (path_step(p[0], p[1], tp[0], tp[1], 0, &s->f[5], &s->f[5])) en_walk_to(e, tp, 0.6f, 0.6f, 0);
                else { if (!s->i[6]) { en_rand_vec(&s->f[3], &s->f[4], speed); s->i[6] = 1; } v[0] = lerpf(v[0], s->f[3], 0.25f); v[1] = lerpf(v[1], s->f[4], 0.25f); }
            } else { if (!s->i[6]) { en_rand_vec(&s->f[3], &s->f[4], speed); s->i[6] = 1; } v[0] = lerpf(v[0], s->f[3], 0.25f); v[1] = lerpf(v[1], s->f[4], 0.25f); }
            if (fabsf(v[1]) > fabsf(v[0])) s->dir = v[1] > 0 ? 0 : 1; else { s->dir = 2; en_flip(e, !(v[0] > 0)); }
        } else { v[0] *= 0.75f; v[1] *= 0.75f; s->i[6] = 0; }
        snprintf(an, sizeof an, "Walk%s0%d", BOWL_DIR[s->dir], s->i[0]); en_set_frame(s, an, sframe);
        if (sframe == 2 || sframe == 18) s->i[1] = 1;
        else if (sframe == 9 || sframe == 25) { s->i[1] = 0; en_sound(SOUND_SCAMPER, 0.3f, 0.6f); }
        if (s->cd > 90 && s->i[0] == 1 && en_line_clear(p[0], p[1], t[0], t[1], 0) && vlen(p[0] - t[0], p[1] - t[1]) < 200.0f && !(en_scared(e) || en_confused(e))) {
            s->state = 2; en_flip(e, t[0] > p[0]);
        }
    } else if (s->state == 2) {
        v[0] *= 0.9f; v[1] *= 0.9f;
        if (s->f[0] < 0) { s->state = 1; s->dir = 0; }
        if (en_finished(s, "HeadThrowStart")) {
            en_sound(SOUND_SHELLGAME, 1.0f, 0.7f); s->state = 3; s->f[1] = t[0] - p[0]; s->f[2] = t[1] - p[1];
            s->i[5] = (fabsf(s->f[2]) > fabsf(s->f[1]) && s->f[2] < 0) ? 2 : 1; en_flip(e, s->f[1] > 0);
        } else en_play(s, "HeadThrowStart");
    } else if (s->state == 3) {
        snprintf(an, sizeof an, "HeadThrowShoot0%d", s->i[5]);
        if (s->f[0] < 0 && !s->i[2]) { s->state = 1; s->dir = 0; s->i[0] = 2; s->i[2] = 0; }
        if (en_finished(s, an)) { s->state = 1; s->dir = 0; s->i[2] = 0; s->i[0] = 2; s->cd = 0; }
        else if (en_event(s, an, "Shoot")) {
            float x = s->f[1], y = s->f[2]; vresize(&x, &y, 10.0f); float vx = s->f[1], vy = s->f[2]; vresize(&vx, &vy, 12.0f);
            unsigned long h = en_spawn(160, 760, p[0] + x, p[1] + y, vx, vy, e, 2);
            if (h) { en_flag_clear(h, EF_FLAG_APPEAR); *(float *) (h + FB_HP_OFF) = s->f[0]; en_t *hs = en_adopt(h); if (hs) hs->f[0] = s->f[0]; }
            s->i[0] = 2; s->i[2] = 1;
        } else if (en_event(s, an, "Hop")) s->i[3] = 1;
        else if (en_event(s, an, "Land")) { s->i[3] = 0; en_sound(SOUND_SCAMPER, 0.3f, 0.6f); }
        else en_play(s, an);
        if (s->i[3]) { float x = s->f[1], y = s->f[2]; vresize(&x, &y, 4.0f); v[0] = lerpf(v[0], x, 0.3f); v[1] = lerpf(v[1], y, 0.3f); } else { v[0] *= 0.75f; v[1] *= 0.75f; }
    } else {                                                                              /* pickup */
        if (!(s->ref || s->i[4])) { s->state = 1; s->i[4] = 0; s->dir = 0; s->i[0] = 2; s->cd = 0; }
        else if (s->i[4]) { v[0] *= 0.75f; v[1] *= 0.75f; if (s->f[0] < 0) { s->state = 1; s->i[4] = 0; s->dir = 0; s->i[0] = 2; } }
        else if (s->ref) { float *cp = fpos(s->ref); float x = cp[0] - p[0], y = cp[1] - 10.0f - p[1], l = vlen(x, y); vresize(&x, &y, l / 3.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
        if (s->state == 4) {
            if (en_finished(s, "Recover")) { s->state = 1; s->dir = 0; s->i[0] = 1; s->cd = 0; s->i[4] = 0; }
            else if (en_event(s, "Recover", "Pick") && s->ref) {
                en_sound(SOUND_SCAMPER, 1.0f, 1.0f); s->f[0] = *(float *) (s->ref + FB_HP_OFF); en_remove(s->ref); s->ref = 0; s->i[0] = 1; s->i[4] = 1;
            } else en_play(s, "Recover");
        }
    }
}
/* the rolling ball: f[0] headhealth is unused; i[0] = frame counter parity, cd = StateFrame */
static void ai_bowlerhead(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    en_set_collision(e, ENTCOLL_PLAYEROBJECTS);
    if (!s->init) { s->init = 1; ENLOG(29, 1, "ffenemy: not done: Bowler ball SpriteOffset(0,-8) and RolyPolyRoll looping sound"); }
    s->i[0]++;
    if (s->ref) { v[0] *= 0.1f; v[1] *= 0.1f; }
    float speed = vlen(v[0], v[1]), x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, speed);
    if (speed < 0.5f) { v[0] *= 0.9f; v[1] *= 0.9f; }
    else {
        v[0] = lerpf(v[0], x, 0.15f) * 0.98f; v[1] = lerpf(v[1], y, 0.15f) * 0.98f;
        if (speed > 3.0f) s->cd++; else if (s->i[0] % 2 == 1) s->cd++;
    }
    if (fabsf(v[0]) > fabsf(v[1])) { s->dir = 2; en_flip(e, !(v[0] > 0)); } else s->dir = v[1] > 0 ? 0 : 1;
    char an[40]; snprintf(an, sizeof an, "HeadRoll%s", BOWL_DIR[s->dir]); en_set_frame(s, an, s->cd % 16);
}
static void hurt_bowler(en_t *s, unsigned long e, float dmg) { (void) e; if (s->sub != 2) s->f[0] -= dmg; }

/* ---- Weaver 155.0 (weaver.lua weaverAI, variant 0): head (sub 0) tunnels / surfaces, its butt (sub 1) and tail (sub 2) replay its position history.
 *      s->state 0 underground 1 surface; cd = StateFrame; i[0] = headnum, i[1] = staystill; f[0] = SpriteOffset.Y; segments: ref = head, i[0] = timeset ---- */
#define WV_HIST 8
typedef struct { unsigned long e; int n; struct { int state; float x, y, vx, vy, off, offe; } h[WV_HIST]; } wv_t;
static wv_t g_wv[8];
static wv_t *wv_get(unsigned long e, int create) {
    int fr = -1; for (int k = 0; k < 8; k++) { if (g_wv[k].e == e) return &g_wv[k]; if (!g_wv[k].e || !alive(g_wv[k].e)) { if (fr < 0) fr = k; } }
    if (!create || fr < 0) return 0; memset(&g_wv[fr], 0, sizeof g_wv[fr]); g_wv[fr].e = e; return &g_wv[fr];
}
static void weaver_orbit(unsigned long e, float *tx, float *ty, int under) {            /* FiendFolio.WackierWeavers: several weavers circle the player */
    unsigned long snap[SNAP]; unsigned n = snapshot(snap), cnt = 0; unsigned long list[16]; float sum = 0; int first = 1;
    for (unsigned k = 0; k < n && cnt < 16; k++) { unsigned long c = snap[k]; if (!alive(c) || *(int *) (c + 0x38) != 155) continue; int sub = *(int *) (c + 0x40);
        if (sub == 0 || sub > 3) { list[cnt++] = c; sum += (float) (((c >> 4) & 0xffff) % 360); if (first) { en_t *cs = en_adopt(c); sum += cs ? (float) (cs->age % 360) : 0; first = 0; } } }
    if (cnt > 1) for (unsigned k = 1; k < cnt; k++) if (list[k] == e) { float off = (360.0f / (float) (cnt - 1)) * (float) (k), rx, ry; vrot(under ? 65.0f : 30.0f, 0, off + sum, &rx, &ry); *tx += rx; *ty += ry; }
}
static void ai_weaver(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    const int groundtime = 10, nbutts = 1, ntails = 1; const float speed = 6.0f, curvar = 1.3f, lerpv = 0.07f; const int curframe = 19;
    if (s->sub == 1 || s->sub == 2) {
        wv_t *w = s->ref ? wv_get(s->ref, 0) : 0; en_t *ps = s->ref && alive(s->ref) ? en_adopt(s->ref) : 0;
        en_play(s, s->sub == 2 ? "BodyTail" : "BodyMiddle");
        if (s->sub == 2 && ps && ps->i[5]) { ps->i[5] = 0; en_remove(e); return; }       /* the parent's killButt */
        if (!ps || en_dead(s->ref)) { en_kill(e); return; }
        if (w && w->n >= s->i[0]) {
            int k = s->i[0] - 1; int gone = w->h[k].state == 0 || w->h[k].state == 2;
            if (gone) { en_remove(e); return; }
            float off = w->h[k].off + w->h[k].offe * (float) s->sub;
            en_set_collision(e, (off <= -25.0f || off > 0) ? ENTCOLL_NONE : ENTCOLL_PLAYEROBJECTS);
            p[0] = w->h[k].x; p[1] = w->h[k].y; v[0] = w->h[k].vx; v[1] = w->h[k].vy;
        }
        return;
    }
    wv_t *w = wv_get(e, 1); if (!w) return;
    if (!s->init) { s->init = 1; s->cd = -10; s->state = 0; w->n = 0; ENLOG(30, 1, "ffenemy: not done: Weaver SpriteOffset hop arc / alpha fade, Visible toggling, dirt pile effects (1000.146), approximated orbit seeds"); } else s->cd++;
    float tx = t[0], ty = t[1]; weaver_orbit(e, &tx, &ty, s->state == 0);
    float x = tx - p[0], y = ty - p[1]; vresize(&x, &y, speed); if (en_scared(e)) { x = -x; y = -y; }
    v[0] = lerpf(v[0], x, lerpv); v[1] = lerpf(v[1], y, lerpv);
    if (s->state == 0) {
        en_play(s, "Empty"); en_set_gridcoll(e, GRIDCOLL_WALLS); en_set_collision(e, ENTCOLL_NONE);
        if (s->cd > groundtime && en_tile(p[0], p[1]) == 0) {
            en_sound(SOUND_MAGGOT_BURST_OUT, 0.3f, 1.3f + (float) rint_(1, 3) / 10.0f);
            s->state = 1; s->i[0] = rint_(1, 15) == 1 ? 2 : 1; s->cd = 0;
        }
    } else {
        char an[24]; snprintf(an, sizeof an, "Head%s%d", v[1] < 0 ? "Up" : "Down", s->i[0]); en_play(s, an); en_flip(e, v[0] < 0);
        float curve = 0.0f - sinf(0.2f * (float) s->cd / curvar) * 20.0f; int coll = en_tile(p[0], p[1]);
        if (s->cd > curframe && coll >= 2) { s->cd = curframe; s->i[1] = 1; }
        s->f[0] = curve; en_set_collision(e, curve <= -25.0f ? ENTCOLL_NONE : ENTCOLL_ALL);
        if (s->cd > curframe + 2) { en_sound(SOUND_MAGGOT_ENTER_GROUND, 0.3f, 1.3f + (float) rint_(1, 3) / 10.0f); s->state = 0; s->cd = 0; }
    }
    float offe = s->i[1] ? 0.0f : ((float) s->cd - 7.5f) * -0.5f;
    memmove(&w->h[1], &w->h[0], sizeof w->h[0] * (WV_HIST - 1)); if (w->n < WV_HIST) w->n++;
    w->h[0].state = s->state == 0 ? 0 : 1; w->h[0].x = p[0]; w->h[0].y = p[1]; w->h[0].vx = v[0]; w->h[0].vy = v[1]; w->h[0].off = s->f[0]; w->h[0].offe = offe;
    int totalbutts = 3 * nbutts, totalmore = totalbutts + 3 * ntails; if (w->n > totalmore + 1) w->n = totalmore + 1;
    for (int pass = 0; pass < 2; pass++) {                                                 /* butts, then the tail */
        int th = pass == 0 ? 3 : 3 + totalbutts;
        if (w->n > th && w->h[th - 1].state == 1 && w->h[th].state == 0) {
            unsigned long c = en_spawn(155, 0, w->h[th - 1].x, w->h[th - 1].y, w->h[th - 1].vx, w->h[th - 1].vy, e, pass == 0 ? 1 : 2);
            if (c) { en_flag_clear(c, EF_FLAG_APPEAR); *(float *) (c + FB_HP_OFF) = fb_hp(e); en_set_gridcoll(c, GRIDCOLL_WALLS); en_set_collision(c, ENTCOLL_PLAYEROBJECTS);
                en_t *cs = en_adopt(c); if (cs) { cs->ref = e; cs->i[0] = th; } }
        }
    }
}

/* ---- Tap 160.490 (tap.lua tapAI). It stands on an urn it spawns itself (SpawnGridEntity GRID_ROCK_ALT) and hops between urns (GetUnoccupiedPot); neither the grid spawn nor the
 *      urn list is measured here, so it stays on its spot (the "no other urn" case) and the "urn is gone -> Remove" checks are skipped. s->state 0 idle 1 burrow 2 emerge 3 spew; cd = StateFrame; i[0] = leavecreep ---- */
static void ai_tap(en_t *s, unsigned long e) {
    float *p = fpos(e);
    if (!s->init) { s->init = 1; s->state = 0; s->cd = -1; en_set_collision(e, ENTCOLL_NONE);
        ENLOG(31, 1, "ffenemy: not done: Tap urn grid entity (SpawnGridEntity/GetUnoccupiedPot not measured: it never moves to another urn), looping TapLoop sound, creep Timeout 40, SpriteOffset(-1,-11), no-knockback flags"); } else s->cd++;
    if (en_event(s, s->anim, "Place")) en_sound(FFSND_TapTap, 0.3f, (float) rint_(90, 100) / 100.0f);
    if (s->state == 0) {
        en_play(s, "Idle");
        if (s->cd > 20 && rnd(5) == 0) { s->state = 1; en_sound(FFSND_TapEnd, 0.6f, (float) rint_(90, 100) / 100.0f); }
    } else if (s->state == 1) {
        if (en_finished(s, "Burrow")) {
            if (en_room_clear()) { en_remove(e); return; }
            s->state = 2; en_sound(FFSND_TapStart, 0.6f, (float) rint_(90, 100) / 100.0f);
        } else en_play(s, "Burrow");
    } else if (s->state == 2) {
        if (en_finished(s, "Emerge")) s->state = 3; else en_play(s, "Emerge");
    } else {
        if (en_finished(s, "Spew")) { s->state = 0; s->cd = 0; }
        else if (en_event(s, "Spew", "Creep Start")) { s->i[0] = 1; s->cd = 0; }
        else if (en_event(s, "Spew", "Creep Stop")) s->i[0] = 0;
        else en_play(s, "Spew");
    }
    if (s->i[0] && s->cd % 2 == 0) {
        float ang = (90.0f / 8.0f) * (float) s->cd, x, y; vrot(0, 30.0f, -ang, &x, &y);
        en_spawn(1000, 22, p[0] + x, p[1] + y, 0, 0, e, 0);
    }
}
static const enemy_t g_tab_g3[] = {
    { 160, 820, ai_slim, die_slim, 0, 0 },
    { 160, 760, ai_bowler, 0, hurt_bowler, 0 },
    { 155, 0, ai_weaver, 0, 0, 0 },
    { 160, 490, ai_tap, 0, 0, 1 },
};
#define N_TAB_G3 ((int) (sizeof g_tab_g3 / sizeof g_tab_g3[0]))
