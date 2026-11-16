/* Cellar, part 2. Included by ffenemy.h after ffen_g5.h. */

/* ------------------------------------------------------------------------------------------ Zingling 160.401 (erfly/enemies/colonel.lua zinglingAI, 393)
 * Only the branch without a Colonel (npc.Parent nil) is done: the Colonel that spawns the orbiting ones is not ported (Cellar's Zingling spawns alone).
 * state 0 idle / 1 attack(shoot) / 2 suicide / 3 drop / 4 twitch / 5 dead / 6 chargestart / 7 charge; cd = StateFrame, i[0] = deadstate, i[1] = colltimer */
static int en_frame_is(const en_t *s, const char *name, int fr) { return !strcmp(s->anim, name) && s->af == fr; }
static void ai_zingling(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tv[2]; if (!t) return;
    char nm[16];
    if (!s->init) { s->init = 1; s->state = 0; s->i[1] = 0; } else s->cd++;
    if (!(s->i[0] || s->state == 7)) en_flip(e, t[0] < p[0]);
    if (!s->i[0]) {                                                             /* not d.deadstate */
        tv[0] = t[0] - p[0]; tv[1] = t[1] - p[1]; vresize(&tv[0], &tv[1], 1.0f);
        if (en_scare(e)) { tv[0] *= -3.0f; tv[1] *= -3.0f; }
        else if (en_confused(e)) { en_rand_vec(&tv[0], &tv[1], 1.0f); tv[0] *= 3.0f; tv[1] *= 3.0f; }
        v[0] = lerpf(v[0], tv[0], 0.1f); v[1] = lerpf(v[1], tv[1], 0.1f);
    }
    if (s->state == 0) {
        en_play(s, "Fly");
        if (((rint_(1, 10) == 1 && s->cd > 10) || s->cd > 60) && en_line(t, p, 0) && !(en_scare_conf(e) || SKATOLE_HELD)) s->state = fb_hp(e) < *(const float *) (e + 0x358) ? 2 : 1;
    } else if (s->state == 1) {
        if (en_finished(s, "Attack01")) { s->state = 0; s->cd = 0; }
        else if (en_event(s, "Attack01", "Shoot")) { float x = t[0] - p[0], y = t[1] - p[1]; en_sound(SOUND_BLOODSHOOT, 1.0f, 1.0f); vresize(&x, &y, 9.0f); en_shoot(e, 0, p[0], p[1], x, y); }
        else en_play(s, "Attack01");
    } else if (s->state == 2) {
        if (en_finished(s, "StingShoot")) { s->state = 3; s->i[0] = rint_(1, 3); en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK); }
        else if (en_event(s, "StingShoot", "Shoot")) { float x = t[0] - p[0], y = t[1] - p[1]; en_sound(FFSND_FrogShoot, 0.7f, (float) rint_(12, 14) / 10.0f); vresize(&x, &y, 9.0f); en_shoot_stinger(e, p[0], p[1], x, y, 5); }
        else en_play(s, "StingShoot");
    } else if (s->state == 3) {
        v[0] = v[1] = 0; snprintf(nm, sizeof nm, "Drop0%d", s->i[0]);
        if (en_finished(s, nm)) {
            if (en_has_path(e, t)) { en_set_can_shut_doors(e, 0); s->state = 4; }
            else { ENLOG(47, 1, "ffenemy: not done: Zingling's BloodExplode gibs"); en_remove(e); }
        } else if (en_frame_is(s, nm, 3)) {                                       /* splat */
            if (en_xy(p[0], p[1], 0)) { en_remove(e); }
            else { en_set_gridcoll(e, GRIDCOLL_GROUND); en_spawn(1000, 22, p[0], p[1], 0, 0, e, 0); }
        } else en_play(s, nm);
    } else if (s->state == 4) {
        v[0] = v[1] = 0; snprintf(nm, sizeof nm, "Twitch0%d", s->i[0]);
        if (en_finished(s, nm)) { s->state = 5; s->cd = 0; }
        else if (en_frame_is(s, nm, 8)) en_spawn(1000, 22, p[0], p[1], 0, 0, e, 0);
        else en_play(s, nm);
    } else if (s->state == 5) {
        v[0] = v[1] = 0; snprintf(nm, sizeof nm, "Dead0%d", s->i[0]); en_play(s, nm);
        if (rint_(0, 59) == 1 && s->cd > 50) s->state = 4;
    } else if (s->state == 6) {
        if (en_finished(s, "ChargeStart")) { s->state = 7; v[0] = v[1] = 0; en_play(s, "Charge"); s->cd = 0; float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 20.0f); v[0] = lerpf(0, x, 0.4f); v[1] = lerpf(0, y, 0.4f); }
        else en_play(s, "ChargeStart");
    } else if (s->state == 7) {
        float x = t[0] - p[0], y = t[1] - p[1];
        en_flip(e, v[0] < 0); en_play(s, "Charge"); vresize(&x, &y, 20.0f);
        v[0] = lerpf(v[0], x, 0.03f); v[1] = lerpf(v[1], y, 0.03f);
        if (s->cd > 50) s->state = 0;
        if (en_collides_grid(e)) { if (vlen(v[0], v[1]) > 6.0f && s->i[1] > 10) en_kill(e); s->i[1] = 0; } else s->i[1]++;
    } else { s->state = 0; s->cd = 0; }
    ENLOG(48, 1, "ffenemy: not done: Zingling's Colonel-parent branch (the Colonel is not ported) and its CollisionDamage 1/0 while charging");
}

/* ------------------------------------------------------------------------------------------ Hover 160.610 (erfly/enemies/hover.lua)
 * state 0 idle / 1 chargestart / 2 charge / 3 stopcharge; cd = StateFrame; i[0] = dirX, i[1] = dirY, i[2] = dirString, i[3] = follow; f[0..1] = d.vec */
static void ai_hover(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tv[2], pos[2], tp[2]; if (!t) return;
    char nm[16];
    if (!s->init) { s->init = 1; s->state = 0; ENLOG(49, 1, "ffenemy: not done: Hover's backwards sprite sheet (ReplaceSpritesheet, 1/150 or subtype 160) and Skatole CollisionDamage 0"); } else s->cd++;
    if (s->state == 0) {
        en_play(s, "Idle");
        en_flip(e, t[0] > p[0]);
        float *tvel = en_target_vel();
        pos[0] = t[0] + (tvel ? tvel[0] : 0) * 40.0f; pos[1] = t[1] + (tvel ? tvel[1] : 0) * 40.0f;
        en_confuse_pos(s, e, pos, tp);
        tv[0] = tp[0] - p[0]; tv[1] = tp[1] - p[1]; vresize(&tv[0], &tv[1], 4.0f);
        if (en_scared(e)) { tv[0] = -tv[0]; tv[1] = -tv[1]; }
        v[0] = lerpf(v[0], tv[0], 0.25f); v[1] = lerpf(v[1], tv[1], 0.25f);
        if (s->cd > 6 && vlen(p[0] - t[0], p[1] - t[1]) < 80.0f && !en_scare_conf(e)) {
            s->state = 1; en_sound(FFSND_BeeBuzzPrep, 1.0f, (float) rint_(140, 160) / 100.0f);
            s->i[3] = 1;
            if (p[1] > t[1]) { s->i[1] = 1; s->i[2] = 2; } else { s->i[1] = -1; s->i[2] = 1; }
            s->i[0] = en_flipped(e) ? -1 : 1;
        }
    } else if (s->state == 1) {
        snprintf(nm, sizeof nm, "ChargeStart0%d", s->i[2]); en_play(s, nm);
        if (s->i[3]) {
            tp[0] = t[0] + (float) s->i[0] * 80.0f; tp[1] = t[1] + (float) s->i[1] * 80.0f;
            tv[0] = tp[0] - p[0]; tv[1] = tp[1] - p[1]; vresize(&tv[0], &tv[1], 8.0f);
            v[0] = lerpf(v[0], tv[0], 0.25f); v[1] = lerpf(v[1], tv[1], 0.25f);
        } else { v[0] *= 0.7f; v[1] *= 0.7f; }
        if (en_event(s, nm, "charge")) {
            s->state = 2; en_sound(FFSND_BeeBuzz, 1.0f, (float) rint_(140, 160) / 100.0f); s->cd = 0;
            s->f[0] = (float) -s->i[0]; s->f[1] = (float) -s->i[1];
            tv[0] = s->f[0]; tv[1] = s->f[1]; vresize(&tv[0], &tv[1], 6.0f); v[0] = tv[0]; v[1] = tv[1];
        } else if (en_event(s, nm, "stopfollow")) s->i[3] = 0;
    } else if (s->state == 2) {
        snprintf(nm, sizeof nm, "Charge0%d", s->i[2]); en_play(s, nm);
        tv[0] = s->f[0]; tv[1] = s->f[1]; vresize(&tv[0], &tv[1], 12.0f + fmaxf(5.0f, (float) s->cd * 0.3f));
        v[0] = lerpf(v[0], tv[0], 0.2f); v[1] = lerpf(v[1], tv[1], 0.2f);
        if (s->cd > 20 && vlen(p[0] - t[0], p[1] - t[1]) > 120.0f) { s->state = 3; s->cd = 0; }
        else if (s->cd > 5 && en_collides_grid(e)) { s->state = 0; s->cd = 0; }
    } else if (s->state == 3) {
        snprintf(nm, sizeof nm, "Charge0%d", s->i[2]); en_play(s, nm);
        v[0] *= 0.8f; v[1] *= 0.8f;
        if (s->cd > 5 || en_collides_grid(e)) { s->state = 0; s->cd = 0; }
    }
    en_sprite_offset(s, e, 0, -15.0f);
}

/* ------------------------------------------------------------------------------------------ Honeydrip 160.810 (erfly/enemies/honeydrip.lua)
 * state 0 idle / 1 slam; cd = StateFrame */
static void ai_honeydrip(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), tv[2];
    if (!s->init) { s->init = 1; s->state = 0; } else s->cd++;
    en_sprite_offset(s, e, 0, -15.0f);
    if (s->state == 0) {
        en_play(s, "Fly");
        en_diag_vec(s, e, 2.5f, 1.0f, &tv[0], &tv[1]); v[0] = tv[0]; v[1] = tv[1];
        if (!en_scare_conf(e) && s->cd > 50 && en_grid_type_at(p[0], p[1]) != 7) s->state = 1;      /* not over a pit */
    } else {
        v[0] *= 0.8f; v[1] *= 0.8f;
        if (en_finished(s, "Slam")) { s->cd = 0; s->state = 0; }
        else if (en_playing(s, "Slam") && s->af == 1) en_sound(FFSND_BeeBuzzPrep, 1.0f, (float) rint_(110, 120) / 100.0f);
        else if (en_playing(s, "Slam") && s->af == 10) en_sound(FFSND_BeeBuzzDown, 1.0f, (float) rint_(140, 150) / 100.0f);
        else if (en_event(s, "Slam", "Slam")) {
            en_sound(SOUND_HELLBOSS_GROUNDPOUND, 0.7f, (float) rint_(165, 175) / 100.0f);
            ENLOG(50, 1, "ffenemy: not done: Honeydrip's shockwave (effect 61 with MinRadius 20 / MaxRadius 40 / Timeout 2: the effect's radius fields are not measured)");
        } else if (en_event(s, "Slam", "GetOut")) {
            en_sound(SOUND_PLOP, 0.7f, (float) rint_(8, 12) / 10.0f);
            if (en_count(256, -1) < 7) {
                float x, y; en_sound(SOUND_SUMMONSOUND, 0.6f, 1.0f); en_rand_vec(&x, &y, 3.0f);
                unsigned long b = en_spawn(256, 0, p[0], p[1], x, y, e, 0); if (b) en_flag_clear(b, EF_FLAG_APPEAR);
            }
        } else en_play(s, "Slam");
    }
}

