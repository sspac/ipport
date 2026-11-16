/* Cellar, part 3. Included by ffenemy.h after ffen_g5b.h. */

/* Entity:TakeDamage(dmg, 0, EntityRef(source), cd) on the player: through the player'"'"'s own vtable slot 9 like hit_enemy does for a monster (the call is the same, the player'"'"'s damage cooldown is the engine'"'"'s) */
static void en_hit_player(float dmg) { unsigned long pl = g_first_player ? g_first_player() : 0; if (pl) { ENLOG(62, 2, "ffenemy: damaging the player through its vtable (first use)"); hit_enemy(pl, dmg); } }

/* ------------------------------------------------------------------------------------------ Ransacked 666.70 (mini/enemies/ransacked.lua)
 * state 0 idle / 1 quiver / 2 hatch; i[0] = framesuntilquiver, i[1] = lastframesuntilquiver */
static void ai_ransacked(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK);
    if (!s->init) { s->init = 1; s->i[0] = 70; s->i[1] = 70; s->state = 0; }
    v[0] = v[1] = 0;
    if (s->state == 0) {
        en_play(s, "idle");
        s->i[0]--;
        if (s->i[0] <= 0) {
            s->state = 1; en_play(s, "quiver");
            s->i[0] = s->i[1] - rint_(8, 10); s->i[1] = s->i[0];
            if (s->i[0] <= 0) s->state = 2;
        }
    } else if (s->state == 1) { if (en_finished(s, "quiver")) s->state = 0; }
    else en_play(s, "hatch");
    if (en_event(s, s->state == 2 ? "hatch" : s->anim, "hatch")) {
        if (s->sub == 0) {                                                      /* ransackedtable: Sackboy 20, Bony 30, Dweller 20, Resident 10 */
            int r = (int) rnd(80), ty, va;
            if (r < 20) { ty = 750; va = 80; } else if (r < 50) { ty = 227; va = 0; } else if (r < 70) { ty = 160; va = 80; } else { ty = 160; va = 82; }
            unsigned long c = en_spawn(ty, va, p[0], p[1], 0, 0, e, 0); if (c) en_flag_clear(c, EF_FLAG_APPEAR);
            ENLOG(51, 1, "ffenemy: Ransacked hatches Sackboy/Dweller/Resident whose own AI is not in the port yet (their rows are installed, they act as the engine's default)");
        }
        en_spawn(1000, 667, p[0], p[1] - 8.0f, 0, 0, e, 0);
        en_kill(e);
    }
}
/* the web effect left behind (ransackedwebai): plays "webcorpse" */
static void ai_ransacked_web(en_t *s, unsigned long e) { (void) e; en_play(s, "webcorpse"); }

/* ------------------------------------------------------------------------------------------ Marge 666.180 (mini/enemies/marge.lua): f[0] = confuseangle (degrees) */
static void die_marge(en_t *s, unsigned long e) {
    float *p = fpos(e); (void) s;
    int n = rint_(4, 5);
    ENLOG(52, 1, "ffenemy: not done: Marge's flies are thrown (EntityNPC.ThrowSpider + Morph in cheekyspawn); they are spawned on their landing spot");
    for (int k = 0; k < n; k++) {
        float x, y; en_rand_vec(&x, &y, 1.0f);
        float d = (float) rint_(25, 45);
        unsigned long c = en_spawn(281, 0, p[0] + x * d, p[1] + y * d, 0, 0, e, 0);
        if (c) { en_flag_clear(c, EF_FLAG_APPEAR); }
    }
}
static void ai_marge(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    if (!s->init) { s->init = 1; s->f[0] = (float) rint_(1, 360); }
    s->f[0] += (float) rint_(-30, 30);
    en_random_confuse(e, t, tp);
    if (en_line_clear(p[0], p[1], tp[0], tp[1], 0) || en_scared(e) || en_confused(e)) {
        float x = tp[0] - p[0], y = tp[1] - p[1], lerp = en_confused(e) ? 0.4f : 0.3f; vresize(&x, &y, 3.5f);
        if (en_scared(e)) { x = -x; y = -y; } else if (en_confused(e)) vrot(3.5f, 0, s->f[0], &x, &y);
        v[0] = lerpf(v[0], x, lerp); v[1] = lerpf(v[1], y, lerp);
    } else { float wx, wy; if (path_step(p[0], p[1], tp[0], tp[1], 0, &wx, &wy)) { float x = wx - p[0], y = wy - p[1]; vresize(&x, &y, 0.5f); v[0] += x; v[1] += y; } }
    en_flip(e, 0);
    if (vlen(v[0], v[1]) < 0.25f) en_set_frame(s, "Appear", 6);
    else if (fabsf(v[0]) >= fabsf(v[1])) en_set_frame(s, v[0] < 0 ? "WalkLeft" : "WalkRight", s->age % 22);
    else en_set_frame(s, "WalkVert", s->age % 22);
}

/* ------------------------------------------------------------------------------------------ Brood 114.26 (ferrium/enemies/brood.lua)
 * state 0 Idle / 1 Toss / 2 Appear; cd = StateFrame; i[0] = eggState (the subtype), f[0..1] = flightDir */
static void ai_brood(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    char nm[16];
    en_random_confuse(e, t, tp);
    if (!s->init) { s->init = 1; en_flag_clear(e, EF_FLAG_APPEAR); s->state = 2; en_spawn(1000, 15, p[0], p[1], 0, 0, e, 0); s->i[0] = s->sub; }
    else s->cd++;
    if (s->state == 0) {
        if (s->cd % 50 == 0) {
            float dx = tp[0] - p[0], dy = tp[1] - p[1], x = dx, y = dy;
            en_sound(FFSND_BeeBuzzDown, 0.15f, (float) rint_(120, 160) / 100.0f);
            vresize(&x, &y, 5.0f); s->f[0] = x + dx * 0.025f; s->f[1] = y + dy * 0.025f;
            s->cd = -5 + (int) rnd(10);
        }
        if (vlen(s->f[0], s->f[1]) > 0.1f) { s->f[0] *= 0.96f; s->f[1] *= 0.96f; }
        if (en_scare(e)) { v[0] = lerpf(v[0], -s->f[0] * 1.5f, 0.15f); v[1] = lerpf(v[1], -s->f[1] * 1.5f, 0.15f); }
        else { v[0] = lerpf(v[0], s->f[0], 0.15f); v[1] = lerpf(v[1], s->f[1], 0.15f); }
        if (vlen(p[0] - t[0], p[1] - t[1]) < 100.0f && !en_scare_conf(e) && s->i[0] == 0) s->state = 1;
        else { snprintf(nm, sizeof nm, "Idle%d", s->i[0]); en_play(s, nm); }
    } else if (s->state == 1) {
        if (en_finished(s, "Toss")) { s->state = 0; s->i[0] = 1; s->cd = 10; s->f[0] = s->f[1] = 0; }
        else if (en_event(s, "Toss", "Toss")) {
            en_pp_t pp; memset(&pp, 0, sizeof pp);
            en_sound(SOUND_ANGEL_WING, 0.4f, (float) rint_(120, 130) / 100.0f);
            pp.variant = 8; pp.has_fspeed_mod = 1; pp.fspeed_mod = -25.0f; pp.has_faccel_mod = 1; pp.faccel_mod = 1.0f;
            en_shoot_p(e, p[0], p[1] - 10.0f, (t[0] - p[0]) * 0.03f, (t[1] - p[1]) * 0.03f, &pp);
            ENLOG(53, 1, "ffenemy: not done: Brood's egg sprite (projectile_broodEgg.anm2 via Sprite::Load), NO_WALL_COLLIDE for 2 frames and what happens when it lands: the web grid entity and the two thrown spiders (projectile death and EntityNPC.ThrowSpider are not hooked)");
        } else en_play(s, "Toss");
        v[0] = lerpf(v[0], 0, 0.1f); v[1] = lerpf(v[1], 0, 0.1f);
    } else {
        v[0] = lerpf(v[0], 0, 0.3f); v[1] = lerpf(v[1], 0, 0.3f);
        snprintf(nm, sizeof nm, "Appear%d", s->i[0]);
        if (en_finished(s, nm)) { s->state = 0; s->f[0] = s->f[1] = 0; s->cd = 35; } else en_play(s, nm);
    }
}

/* ------------------------------------------------------------------------------------------ Receptor 160.1390 (erfly/enemies/receptor.lua): one of them in the room leads, the others copy its velocity
 * the leader is the one nearest to its target among those with a path (re-chosen every 10 room frames); ref of nobody, g_mainrec holds it */
static unsigned long g_mainrec; static int g_mainrec_frame = -1;
static void receptor_update(unsigned long self) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); unsigned long best = self; float bd = 99999.0f;
    for (unsigned k = 0; k < n; k++) {
        unsigned long c = snap[k]; if (!alive(c) || *(const int *) (c + E_TYPE) != 160 || *(const int *) (c + E_VARIANT) != 1390 || en_flag(c, EF_FLAG_FRIENDLY)) continue;
        float *t = en_target(c), *cp = fpos(c); if (!t || !en_has_path(c, t)) continue;
        float d = vlen(t[0] - cp[0], t[1] - cp[1]); if (d < bd) { bd = d; best = c; }
    }
    if (best != g_mainrec) {
        g_mainrec = best;
        en_sound(FFSND_FrayedAlert, 0.1f, (float) rint_(120, 150) / 100.0f); en_sound(FFSND_DevilDaggerGemCollect, 0.3f, (float) rint_(150, 250) / 100.0f); en_sound(FFSND_Foresee, 0.3f, (float) rint_(200, 300) / 100.0f);
        ENLOG(54, 1, "ffenemy: not done: Receptor's red flash effect (1000.1960.20 following the new leader)");
    }
}
static void ai_receptor(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    if (!s->init) { s->init = 1; en_set_frame(s, "WalkVert", 0); en_overlay(s, "HeadIdle"); }
    if (s->age < 1) return;
    if (!g_mainrec || !en_parent_alive(g_mainrec)) { g_mainrec = 0; receptor_update(e); }
    else { int fr = g_frame_fn ? g_frame_fn() : s->age; if (fr % 10 == 0 && fr != g_mainrec_frame) { g_mainrec_frame = fr; receptor_update(e); } }
    en_random_confuse(e, t, tp);
    if (g_mainrec == e) {
        en_overlay(s, "HeadActive");
        if (en_scare(e)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -5.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
        else if (en_line_clear(p[0], p[1], tp[0], tp[1], 0)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 4.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
        else { float wx, wy; if (path_step(p[0], p[1], tp[0], tp[1], 0, &wx, &wy)) { float x = wx - p[0], y = wy - p[1]; vresize(&x, &y, 0.6f); v[0] += x; v[1] += y; } }
    } else if (g_mainrec) {
        en_overlay(s, "HeadIdle");
        float *lv = fvel(g_mainrec); v[0] = lerpf(v[0], lv[0], en_collides_grid(e) ? 0.25f : 0.75f); v[1] = lerpf(v[1], lv[1], en_collides_grid(e) ? 0.25f : 0.75f);
    } else { v[0] *= 0.9f; v[1] *= 0.9f; }
    if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0);
}

/* ------------------------------------------------------------------------------------------ Haunted 160.230 / Yawner 160.231 (erfly/enemies/haunted.lua)
 * Haunted: state 0 idle / 1 shoot; cd = StateFrame. Yawner has no state. */
static void ai_haunted(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    if (!s->init) { s->init = 1; s->state = 0; } else s->cd++;
    en_random_confuse(e, t, tp);
    if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0);
    if (en_scare(e)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -5.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
    else if (en_line_clear(p[0], p[1], tp[0], tp[1], 0)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 4.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f); }
    else { float wx, wy; if (path_step(p[0], p[1], tp[0], tp[1], 0, &wx, &wy)) { float x = wx - p[0], y = wy - p[1]; vresize(&x, &y, 0.6f); v[0] += x; v[1] += y; } }
    if (s->state == 0) {
        en_overlay(s, "Head");
        if (s->cd > 30 && rint_(1, 20) == 1 && vlen(tp[0] - p[0], tp[1] - p[1]) < 200.0f && !en_scare_conf(e)) s->state = 1;
    } else {
        if (en_overlay_finished(s, "Shoot")) { s->state = 0; s->cd = 0; }
        else if (!strcmp(s->ov, "Shoot") && s->ovf == 10) {
            en_pp_t pp; memset(&pp, 0, sizeof pp); float x = tp[0] - p[0], y = tp[1] - p[1];
            pp.variant = 4; pp.flags = PF_GHOST;
            en_sound(SOUND_BLOODSHOOT, 1.0f, 1.0f); vresize(&x, &y, 11.0f); en_shoot_p(e, p[0], p[1], x, y, &pp);
        } else en_overlay(s, "Shoot");
    }
}
static void die_haunted(en_t *s, unsigned long e) {
    float *p = fpos(e); (void) s;
    unsigned long c = en_spawn(160, 231, p[0], p[1], 0, 0, e, 0);
    ENLOG(55, 1, "ffenemy: not done: Yawner inherits Haunted's champion (MakeChampion 69) and ChangedHP/HPIncrease 0.1");
    (void) c;
}
static void ai_yawner(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    en_random_confuse(e, t, tp);
    en_sprite_offset(s, e, 0, -5.0f); en_play(s, "Move");
    if (en_scare(e)) {
        if (vlen(p[0] - t[0], p[1] - t[1]) < 300.0f) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -11.0f); v[0] = lerpf(v[0], x, 0.05f); v[1] = lerpf(v[1], y, 0.05f); }
        else { v[0] *= 0.9f; v[1] *= 0.9f; }
    } else { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 11.0f); v[0] = lerpf(v[0], x, 0.05f); v[1] = lerpf(v[1], y, 0.05f); }
}

/* ------------------------------------------------------------------------------------------ Crotchety 114.16 (ferrium/enemies/crotchety.lua)
 * state 0 Idle / 1 Revenge / 2 Revenge Two (the ghost); i[0] = ghost, i[1] = shot; its death leaves a ghost of the same kind that plays "Death" and fires a spread */
static void hurt_crotchety(en_t *s, unsigned long e, float dmg) {
    if (dmg > 0 && s->state == 0 && !en_scare_conf(e)) s->state = 1;
    s->hurt_block = s->i[0];
}
static void ai_crotchety(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    if (!s->init) {
        s->init = 1;
        if (!s->i[0] && !s->ref3) s->state = 0;
        else { s->i[0] = 1; s->state = 2; en_set_collision(e, ENTCOLL_PLAYERONLY); en_set_gridcoll(e, GRIDCOLL_NONE); }
    }
    s->hurt_block = s->i[0];
    en_random_confuse(e, t, tp);
    if (!s->i[0]) { if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.0f); else en_set_frame(s, "WalkVert", 0); }
    if (s->state == 0 || s->state == 1) {
        float spd = s->state == 0 ? 3.0f : 2.5f, lr = s->state == 0 ? 0.25f : 0.25f;
        if (s->state == 0) en_overlay(s, "Head");
        else if (en_overlay_finished(s, "Retaliate")) { s->state = 0; s->cd = 0; s->i[1] = 0; }
        else if (s->ovf == 7 && !strcmp(s->ov, "Retaliate") && !s->i[1]) {
            s->i[1] = 1; float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 6.0f); en_shoot(e, 0, p[0], p[1], x, y); en_sound(SOUND_LITTLE_SPIT, 1.0f, 1.0f);
        } else en_overlay(s, "Retaliate");
        if (en_scare(e)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -5.0f); v[0] = lerpf(v[0], x, 0.3f); v[1] = lerpf(v[1], y, 0.3f); }
        else if (en_line_clear(p[0], p[1], tp[0], tp[1], 0)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, spd); v[0] = lerpf(v[0], x, lr); v[1] = lerpf(v[1], y, lr); }
        else { float wx, wy; if (path_step(p[0], p[1], tp[0], tp[1], 0, &wx, &wy)) { float x = wx - p[0], y = wy - p[1]; vresize(&x, &y, 0.4f); v[0] += x; v[1] += y; } }
    } else {
        if (en_finished(s, "Death")) en_remove(e);
        else if (en_event(s, "Death", "Shoot")) {
            en_pp_t pp; memset(&pp, 0, sizeof pp); float x = t[0] - p[0], y = t[1] - p[1];
            en_sound(FFSND_FlashShakeyKidRoar, 1.0f, 1.0f); en_set_collision(e, ENTCOLL_NONE);
            pp.variant = 4; pp.flags = PF_GHOST; pp.has_scale = 1; pp.scale = 1.8f;
            vresize(&x, &y, 6.0f);
            for (int k = -1; k <= 1; k++) { float rx, ry; vrot(x, y, 3.3f * 3.0f * (float) k, &rx, &ry); en_shoot_p(e, p[0], p[1], rx, ry, &pp); }
            ENLOG(56, 1, "ffenemy: not done: the ghost's spread shot is a 3-shot fan by rotation (FireProjectiles mode 2 spread), and each projectile's CollisionDamage 1");
        } else en_play(s, "Death");
        if (en_scare(e)) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -5.0f); v[0] = lerpf(v[0], x, 0.3f); v[1] = lerpf(v[1], y, 0.3f); }
        else { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 5.0f); v[0] = lerpf(v[0], x, 0.07f); v[1] = lerpf(v[1], y, 0.07f); }
    }
}
static void die_crotchety(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e);
    if (s->i[0]) return;
    float x = t ? p[0] - t[0] : 1.0f, y = t ? p[1] - t[1] : 0.0f; vresize(&x, &y, 7.0f);
    unsigned long g = en_spawn(114, 16, p[0], p[1], x, y, e, 0); (void) v;
    if (g) { en_flag_clear(g, EF_FLAG_APPEAR); en_t *gs = en_adopt(g); gs->i[0] = 1; gs->ref3 = 1; if (en_flag(e, EF_FLAG_FRIENDLY)) en_flag_set(g, EF_FLAG_FRIENDLY); }
    ENLOG(57, 1, "ffenemy: not done: Crotchety's ghost has HitPoints 0 in the Lua (here it keeps its HP and ignores hits), the B-Keeper coin and the Vade Retro ghost on death");
}

/* ------------------------------------------------------------------------------------------ Dim 451.90 / Dim Ghost 451.91 (guwah/enemies/dim.lua) */
static void ai_dim(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    en_confuse_pos(s, e, t, tp);
    en_walk_anim(s, e, "WalkHori", "WalkVert", 0.1f); en_overlay(s, "Head");
    if (en_line_clear(p[0], p[1], tp[0], tp[1], 0) || en_scare(e)) {
        float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 4.0f); if (en_scared(e)) { x = -x; y = -y; }
        v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
    } else { float wx, wy; if (path_step(p[0], p[1], tp[0], tp[1], 0, &wx, &wy)) { float x = wx - p[0], y = wy - p[1]; vresize(&x, &y, 0.6f); v[0] += x; v[1] += y; } }
}
static void die_dim(en_t *s, unsigned long e) { float *p = fpos(e); (void) s; en_spawn(451, 91, p[0], p[1], 0, 0, e, 0); ENLOG(58, 1, "ffenemy: not done: Dim Ghost inherits the champion colour"); }
/* ghost: f[0] = alpha */
static void ai_dimghost(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    if (!s->init) { s->init = 1; en_set_collision(e, ENTCOLL_PLAYEROBJECTS); en_flag_set(e, EF_FLAG_NO_TARGET); en_sprite_offset(s, e, 0, -5.0f); s->state = 0; s->f[0] = 1.0f; }
    en_confuse_pos(s, e, t, tp);
    if (s->state == 0) {
        en_play(s, "Move"); s->f[0] -= 0.008f;
        ENLOG(59, 1, "ffenemy: not done: Dim Ghost's fade (SetColor alpha 1 -> 0.25 over ~94 frames); the timing is kept, the colour is not applied");
        if (s->f[0] <= 0.25f) { en_set_collision(e, ENTCOLL_NONE); en_set_can_shut_doors(e, 0); s->state = 1; }
        else if (en_scare(e)) {
            if (vlen(p[0] - t[0], p[1] - t[1]) < 300.0f) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, -11.0f); v[0] = lerpf(v[0], x, 0.05f); v[1] = lerpf(v[1], y, 0.05f); }
            else { v[0] *= 0.9f; v[1] *= 0.9f; }
        } else { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 11.0f); v[0] = lerpf(v[0], x, 0.05f); v[1] = lerpf(v[1], y, 0.05f); }
    } else {
        v[0] *= 0.5f; v[1] *= 0.5f;
        if (en_finished(s, "Die")) en_remove(e); else en_play(s, "Die");
    }
}

/* ------------------------------------------------------------------------------------------ Spoop 750.210 (xalum/enemies/spoop.lua)
 * anim "Idle" wanders, "Move" (scared by a player within 140) hops away, "Prepare" then "Charge": a dash at where the player was, exploding on arrival.
 * f[0..1] = direction, f[2..3] = playerPosition, i[0] = has direction, i[1] = charging */
static void spoop_die(en_t *s, unsigned long e) {
    float *p = fpos(e), *t = en_target(e); (void) s;
    en_sound(SOUND_DEMON_HIT, 1.0f, 1.0f);
    if (t && vlen(t[0] - p[0], t[1] - p[1]) < 40.0f + 12.0f) en_hit_player(1.0f);
    ENLOG(60, 1, "ffenemy: not done: Spoop's explosion effects (ghost explosion + fart poofs) and its trail/outline segments; the 40px area damage to the player is done");
    en_kill(e);
}
static void ai_spoop(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; en_play(s, "Idle"); }
    if (en_finished(s, "Move")) en_play(s, "Idle");
    else if (en_finished(s, "Prepare")) {
        float x = p[0] - t[0], y = p[1] - t[1], rx, ry;
        en_set_frame(s, "Charge", 4); en_flag_set(e, EF_FLAG_NO_TARGET | EF_FLAG_NO_STATUS_EFFECTS); en_sound(SOUND_FLOATY_BABY_ROAR, 1.0f, 2.0f);
        s->f[2] = t[0]; s->f[3] = t[1];
        vrot(x, y, 60.0f - 120.0f * (float) rint_(0, 1), &rx, &ry); vresize(&rx, &ry, 16.0f); v[0] = rx; v[1] = ry;
        en_set_gridcoll(e, GRIDCOLL_NONE); en_set_collision(e, ENTCOLL_PLAYEROBJECTS); s->i[1] = 1;
    }
    if (en_playing(s, "Idle")) {
        if (s->age % 60 == 0 || !s->i[0] || en_collides_grid(e)) {
            int k = 0; float dx, dy;
            do { en_rand_vec(&dx, &dy, 3.0f); k++; } while (!en_in_room_walk(p[0] + dx * 8.0f, p[1] + dy * 8.0f, 0) && k < 32);
            if (k >= 32) { dx = 440.0f - p[0]; dy = 280.0f - p[1]; vresize(&dx, &dy, 3.0f); }
            s->f[0] = dx; s->f[1] = dy; s->i[0] = 1;
        }
        { float x = s->f[0], y = s->f[1]; vresize(&x, &y, 4.0f); v[0] = lerpf(v[0], x, 0.025f); v[1] = lerpf(v[1], y, 0.025f); }
        if (s->age > 5 && vlen(p[0] - t[0], p[1] - t[1]) < 140.0f) { en_play(s, "Move"); en_sound(SOUND_BABY_HURT, 1.0f, 1.0f); }
        if (s->age >= s->sub * 30) { en_play(s, "Prepare"); en_sound(SOUND_BABY_HURT, 1.0f, 1.2f); }
    } else if (en_playing(s, "Move")) {
        if (s->af == 4) {
            float x = p[0] - t[0], y = p[1] - t[1], cx = 440.0f - p[0], cy = 280.0f - p[1], rx, ry;
            vresize(&x, &y, 5.0f); vrot(cx, cy, 90.0f * (float) rint_(0, 1), &rx, &ry); vresize(&rx, &ry, 5.0f);
            x += rx; y += ry; vresize(&x, &y, 5.0f); v[0] = x; v[1] = y;
        }
    } else if (en_playing(s, "Prepare")) { v[0] *= 0.9f; v[1] *= 0.9f; }
    else if (s->i[1]) {
        float x = s->f[2] - p[0], y = s->f[3] - p[1]; vresize(&x, &y, 16.0f); v[0] = lerpf(v[0], x, 0.1f); v[1] = lerpf(v[1], y, 0.1f);
        if (vlen(p[0] - s->f[2], p[1] - s->f[3]) < 15.0f) spoop_die(s, e);
    }
    /* Collision callback: a player or familiar touching it makes it explode (the port has no collision callback: the touch is its own radius check) */
    if (vlen(p[0] - t[0], p[1] - t[1]) < 25.0f) spoop_die(s, e);
}

/* ------------------------------------------------------------------------------------------ Shirk 450.13 / Shirk Spot 150.451 (guwah/enemies/shirk.lua)
 * Spots: g_shirk_spots[idx] 0 none / 1 Open / 2 Closed; a Shirk teleports to a random open spot not near the player. i[0] = Index, i[1] = I1 (teleport lock), cd = StateFrame, f[0..1] = TargetPosition */
static unsigned char g_shirk_spots[448];
static int en_tile_idx(float x, float y) {
    int w = g_gw ? (int) g_gw : 15, c = (int) floorf((x - 20.0f) / 40.0f), r = (int) floorf((y - 100.0f) / 40.0f); return r * w + c;
}
static float en_tile_cx(int idx) { int w = g_gw ? (int) g_gw : 15; return en_room_x(idx % w); }
static float en_tile_cy(int idx) { int w = g_gw ? (int) g_gw : 15; return en_room_y(idx / w); }
static int shirk_safe(float sx, float sy, const float *t, float avoid) {
    float *pl = en_target(0); float near = pl ? vlen(pl[0] - sx, pl[1] - sy) : 9999.0f;
    return !(near < fminf(avoid, 80.0f) || (near < avoid && en_line_clear(sx, sy, t[0], t[1], 0)));
}
static void shirk_teleport(en_t *s, unsigned long e) {
    (void) e;
    if (s->i[1] <= 0) { en_play(s, "Leave"); s->cd = 20; s->i[1] = (g_gw && g_gw > 15) ? 80 : 40; }
}
static void hurt_shirk(en_t *s, unsigned long e, float dmg) { (void) dmg; if (s->init) shirk_teleport(s, e); }
static void ai_shirk(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    en_confuse_pos(s, e, t, tp);
    if (!s->init) {
        s->init = 1; en_flag_clear(e, EF_FLAG_APPEAR); en_flag_set(e, EF_FLAG_NO_TARGET | EF_FLAG_HIDE_HP_BAR); en_set_collision(e, ENTCOLL_NONE);
        en_play(s, "Appear"); en_spawn(150, 451, p[0], p[1], 0, 0, e, 0);
        s->i[0] = en_tile_idx(p[0], p[1]); if (s->i[0] >= 0 && s->i[0] < 448) g_shirk_spots[s->i[0]] = 2;
        if (s->sub == 1) ENLOG(61, 1, "ffenemy: not done: Shirk subtype 1 (waits for the room, makeWaitFerr)");
    }
    v[0] *= 0.7f; v[1] *= 0.7f;
    if (en_finished(s, "Appear") || en_finished(s, "Shoot")) { en_play(s, "Idle"); s->cd = rint_(20, 30); }
    else if (en_finished(s, "Leave")) {
        en_visible(e, 0);
        if (s->cd <= 0) {
            int cnt = 0, pick = -1; static int open[448];
            for (int k = 0; k < 448; k++) if (g_shirk_spots[k] == 1 && shirk_safe(en_tile_cx(k), en_tile_cy(k), t, 100.0f)) open[cnt++] = k;
            if (cnt) pick = open[rnd((unsigned) cnt)];
            if (pick < 0) pick = s->i[0];
            if (s->i[0] >= 0 && s->i[0] < 448) g_shirk_spots[s->i[0]] = 1;
            s->i[0] = pick; if (pick >= 0 && pick < 448) g_shirk_spots[pick] = 2;
            p[0] = en_tile_cx(pick); p[1] = en_tile_cy(pick);
            en_play(s, "Appear"); en_visible(e, 1);
        } else s->cd--;
    }
    if (en_playing(s, "Idle")) {
        if (!shirk_safe(p[0], p[1], tp, 60.0f)) shirk_teleport(s, e);
        if (s->cd <= 0 && vlen(tp[0] - p[0], tp[1] - p[1]) < 200.0f) en_play(s, "Shoot"); else s->cd--;
        s->i[1]--;
    }
    if (en_event(s, s->anim, "Shoot")) {
        en_pp_t pp; memset(&pp, 0, sizeof pp); float x = tp[0] - p[0], y = tp[1] - p[1];
        pp.variant = 4; pp.flags = PF_GHOST; vresize(&x, &y, 9.0f); en_shoot_p(e, p[0], p[1], x, y, &pp);
        en_sound(FFSND_FlashShakeyKidRoar, 1.5f, 1.5f); en_sound(SOUND_BLOODSHOOT, 1.0f, 1.0f);
        en_spawn(1000, 2, p[0], p[1] - 6.0f, 0, 0, e, 5);
        en_flip(e, p[0] > tp[0]);
    } else if (en_event(s, s->anim, "NoDMG")) { en_set_collision(e, ENTCOLL_NONE); en_flag_set(e, EF_FLAG_NO_TARGET | EF_FLAG_HIDE_HP_BAR); en_sound(SOUND_FLOATY_BABY_ROAR, 3.0f, 0.5f); }
    else if (en_event(s, s->anim, "DMG")) { en_set_collision(e, ENTCOLL_ALL); en_flag_clear(e, EF_FLAG_NO_TARGET | EF_FLAG_HIDE_HP_BAR); }
}
static void die_shirk(en_t *s, unsigned long e) { (void) e; if (s->i[0] >= 0 && s->i[0] < 448) g_shirk_spots[s->i[0]] = 1; }
/* the spot: stays on its tile, invisible; the first frame it registers itself as an open spot */
static void ai_shirkspot(en_t *s, unsigned long e) {
    float *p = fpos(e);
    if (!s->init) { s->init = 1; en_visible(e, 0); en_set_collision(e, ENTCOLL_NONE); en_flag_clear(e, EF_FLAG_APPEAR); en_flag_set(e, EF_FLAG_NO_TARGET | EF_FLAG_HIDE_HP_BAR); s->i[0] = en_tile_idx(p[0], p[1]); s->f[0] = en_tile_cx(s->i[0]); s->f[1] = en_tile_cy(s->i[0]); if (s->i[0] >= 0 && s->i[0] < 448 && !g_shirk_spots[s->i[0]]) g_shirk_spots[s->i[0]] = 1; }
    p[0] = s->f[0]; p[1] = s->f[1];
}

static const enemy_t g_tab_g5[] = {
    { 666, 60, ai_skuzz, 0, 0, 0 },
    { 114, 44, ai_congaskuzz, 0, 0, 0 },
    { 114, 45, ai_ringskuzz, 0, 0, 0 },
    { 160, 1050, ai_coby, die_coby, 0, 0 },
    { 160, 1040, ai_nanny, 0, hurt_nanny, 0 },
    { 160, 180, ai_beeter, 0, 0, 0 },
    { 150, 6, ai_stinger, 0, 0, 0 },
    { 160, 681, ai_stingler, 0, hurt_stingler, 0 },
    { 160, 590, ai_homer, 0, 0, 0 },
    { 160, 401, ai_zingling, 0, 0, 0 },
    { 160, 610, ai_hover, 0, 0, 0 },
    { 160, 810, ai_honeydrip, 0, 0, 0 },
    { 666, 70, ai_ransacked, 0, 0, 0 },
    { 1000, 667, ai_ransacked_web, 0, 0, 0 },
    { 666, 180, ai_marge, die_marge, 0, 0 },
    { 114, 26, ai_brood, 0, 0, 0 },
    { 160, 1390, ai_receptor, 0, 0, 0 },
    { 160, 230, ai_haunted, die_haunted, 0, 0 },
    { 160, 231, ai_yawner, 0, 0, 0 },
    { 114, 16, ai_crotchety, die_crotchety, hurt_crotchety, 0 },
    { 451, 90, ai_dim, die_dim, 0, 0 },
    { 451, 91, ai_dimghost, 0, 0, 0 },
    { 750, 210, ai_spoop, 0, 0, 0 },
    { 450, 13, ai_shirk, die_shirk, hurt_shirk, 0 },
    { 150, 451, ai_shirkspot, 0, 0, 0 },
};
#define N_TAB_G5 ((int) (sizeof g_tab_g5 / sizeof g_tab_g5[0]))
