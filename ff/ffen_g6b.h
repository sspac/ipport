/* ffen_g6b.h - Burning Basement, part B: Fumegeist 160.350, Mote 160.351, Smokin 160.441, Flamin 160.442 / Chain 160.443 (erfly/enemies/fumegeist.lua, smokin.lua).
 * Included by ffenemy.h after ffen_g6.h. */

/* ------------------------------------------------------------------------------------------ Fumegeist 160.350
 * state 0 idle / 1 shoot / 2 SetCloud / 3 BecomeFire / 4 Fire / 5 BecomeNormal; cd = StateFrame; s->flaming = d.flaming;
 * f[0..1] = d.targetvel; i[0] = d.snaps, i[1] = d.finishedShoot, i[2] = d.running, i[3] = d.numbaubles (-1 = nil) */
static void fume_flip(unsigned long e, const float *p, const float *t) { en_flip(e, t[0] > p[0]); }
static void ai_fumegeist(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) {
        s->init = 1; s->state = 0; float gx, gy; en_air_pos(t, 120.0f, 99999.0f, &gx, &gy);
        s->f[0] = gx - p[0]; s->f[1] = gy - p[1]; vresize(&s->f[0], &s->f[1], 5.0f); s->i[0] = 0; s->i[3] = -1;
    } else s->cd++;
    if (!s->flaming) {
        ENLOG(200, 1, "ffenemy: not done: the Fumegeist/Mote splat colour (SplatColor charred / fiery fade) and the smoke's PlaybackSpeed");
        if (s->age % 3 == 0) {
            float ex, ey; en_rand_vec(&ex, &ey, (float) rint_(1, 4));
            unsigned long sm = en_smoke(e, p[0], p[1], ex, ey, -25.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); if (sm) en_scale(sm, 1.5f, 1.5f);
        }
    } else if (s->cd % 15 == 0) { fire_t *f = en_fire_new(e, p[0], p[1], 0, 0, 0); if (f) { f->parent = e; en_fire_go(f); } }
    if (s->state == 0) {
        en_play(s, "Idle"); s->i[1] = 0;
        if (!en_scare_conf(e) && fabsf(fabsf(p[0]) - fabsf(t[0])) > 20.0f) fume_flip(e, p, t);
        if (vlen(p[0] - t[0], p[1] - t[1]) < 80.0f || en_scare(e)) {
            s->f[0] = -(t[0] - p[0]); s->f[1] = -(t[1] - p[1]); vresize(&s->f[0], &s->f[1], 2.0f);
            if (en_scare(e)) en_flip(e, s->f[0] > 0);
            s->i[2] = 1;
        } else if (s->cd % 90 == 0 || s->i[2] || (en_confused(e) && s->cd % 5 == 1)) {
            float gx, gy; en_air_pos(t, 120.0f, 99999.0f, &gx, &gy);
            s->f[0] = gx - p[0]; s->f[1] = gy - p[1]; vresize(&s->f[0], &s->f[1], 1.5f); s->i[2] = 0;
        }
        v[0] = lerpf(v[0], s->f[0], 0.1f); v[1] = lerpf(v[1], s->f[1], 0.1f);
        if (s->i[3] < 0) s->i[3] = en_count_type_var(160, 351);
        if (!en_scare_conf(e)) {
            if (s->cd > 70 && rnd(20) == 0) {
                s->i[0] = 0;
                if ((s->i[3] > 1 && rnd(3) == 0) || s->i[3] >= 3 || en_count_type_var(160, 351) >= 3) { s->state = 3; s->i[3] = 0; }
                else { s->state = 1; s->i[3]++; fume_flip(e, p, t); }
            }
        }
    } else if (s->state == 1) {
        v[0] *= 0.7f; v[1] *= 0.7f;
        if (en_finished(s, "Shoot")) { s->state = 0; s->cd = 0; s->i[2] = 1; }
        else if (en_event(s, "Shoot", "Cough")) {
            s->i[1] = 1; en_sound(SOUND_WHEEZY_COUGH, 1.0f, 0.6f);
            float dx = t[0] - p[0], dy = t[1] - p[1]; vresize(&dx, &dy, 4.0f);
            unsigned long m = en_spawn(160, 351, p[0] + dx, p[1] + dy, dx, dy, e, 0);
            if (m) { en_t *ms = en_adopt(m); if (ms) ms->ref = e; if (g_set_parent) g_set_parent(m, e); }
            fume_flip(e, p, t);
        } else en_play(s, "Shoot");
    } else if (s->state == 2) {
        v[0] *= 0.7f; v[1] *= 0.7f;
        if (en_finished(s, "SetCloud")) { s->state = 0; s->cd = 30; s->i[2] = 1; }
        else if (en_event(s, "SetCloud", "SetCloud")) {
            int killed = 0; unsigned long snap[SNAP]; unsigned n = snapshot(snap);
            for (unsigned k = 0; k < n; k++) {
                unsigned long m = snap[k]; if (!alive(m) || *(const int *) (m + E_TYPE) != 160 || *(const int *) (m + E_VARIANT) != 351) continue;
                en_t *ms = en_slot(m); if (ms->flaming) continue;
                const float *mp = fpos(m); float mx = mp[0], my = mp[1];
                unsigned long sm = en_smoke(e, mx, my, 0, 0, -50.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0); if (sm) en_scale(sm, 3.5f, 3.5f);
                float vx, vy; en_rand_vec(&vx, &vy, 20.0f);
                for (int i = 120; i <= 360; i += 120) { float rx, ry; vrot(vx, vy, (float) i, &rx, &ry); en_gunpowder(e, mx + rx, my + ry, 500, 200, -1.0f, 0); }
                killed++; en_kill(m);
            }
            if (killed > 0) en_sound(FFSND_ForeseerClap, 1.0f, 0.5f);
        } else en_play(s, "SetCloud");
    } else if (s->state == 3) {
        v[0] *= 0.7f; v[1] *= 0.7f;
        if (en_finished(s, "BecomeFire")) { s->cd = 0; s->state = 4; }
        else if (en_event(s, "BecomeFire", "DMG")) { s->flaming = 1; en_set_room_alight(e); en_sound(FFSND_FireLight, 0.8f, 0.8f); }
        else en_play(s, "BecomeFire");
    } else if (s->state == 4) {
        if ((s->cd > 140 && rnd(20) == 0) || s->cd > 180) s->state = 5;
        float d; gp_t *g = en_unlit_powder(p[0], p[1], &d); const float *ft = g ? fpos(g->e) : t;
        float tx = ft[0] - p[0], ty = ft[1] - p[1]; vresize(&tx, &ty, 7.0f);
        v[0] = lerpf(v[0], tx, 0.1f); v[1] = lerpf(v[1], ty, 0.1f);
        en_play(s, "Fire"); en_flip(e, v[0] > 0);
    } else {
        v[0] *= 0.7f; v[1] *= 0.7f;
        if (en_finished(s, "BecomeNormal")) { s->cd = 0; s->state = 0; }
        else if (en_event(s, "BecomeNormal", "NoDMG")) { en_flip(e, 0); s->flaming = 0; en_sound(FFSND_FireFizzle, 0.8f, 0.8f); }
        else en_play(s, "BecomeNormal");
    }
}

/* ------------------------------------------------------------------------------------------ Mote 160.351
 * s->ref = d.Parent (the Fumegeist that coughed it); i[0] = d.fwoomed, i[1] = d.youAlreadyDidThisBro, i[2] = d.count; s->flaming */
static void ai_mote(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; en_sprite_offset(s, e, 0.0f, -15.0f); } else s->cd++;
    float speed = 3.5f, lerp = 0.1f; const char *extra = "";
    int friend = en_flag(e, EF_FLAG_FRIENDLY | EF_FLAG_CHARM);
    if (s->flaming) {
        if (!s->i[0]) { en_sound(FFSND_FireLight, 0.6f, 1.2f); s->i[0] = 1; }
        en_set_collision(e, ENTCOLL_PLAYEROBJECTS);
        speed = 6.0f; extra = "Fire"; lerp = 0.08f;
        if (s->cd % 22 == 0 && !en_scare_conf(e)) {
            fire_t *f = en_fire_new(e, p[0], p[1], 0, 0, 0); if (f) { f->parent = e; en_fire_go(f); }
            if (friend) { s->i[2]++; if (s->i[2] >= 15) en_kill(e); }
        }
    }
    if (en_confused(e)) speed *= 0.7f;
    char an[24]; snprintf(an, sizeof an, "%s%s", v[1] < 0 ? "MoveUp" : "MoveDown", extra); en_play(s, an);
    en_flip(e, v[0] > 0);
    float tx, ty; en_diag_vec(s, e, speed, 1.0f, &tx, &ty);
    if (en_scare(e) && vlen(t[0] - p[0], t[1] - p[1]) < 100.0f) { tx = p[0] - t[0]; ty = p[1] - t[1]; vresize(&tx, &ty, speed); }
    v[0] = lerpf(v[0], tx, lerp); v[1] = lerpf(v[1], ty, lerp);
    if (!s->flaming) {
        s->i[0] = 0;
        float ex = -v[0], ey = -v[1], rx, ry, ox = v[0], oy = v[1]; vrot(ex, ey, (float) (rint_(1, 40) - 20), &rx, &ry); vresize(&ox, &oy, 10.0f);
        en_smoke(e, p[0] + ox, p[1] + oy, rx, ry, -20.0f, (float) rint_(1, 360), 0, 0, 0, 0, 0, 0, 0, 0);
    }
    /* moteColl: a Mote that touches a player (not lit, not yet used) asks a Fumegeist (its parent or a random one) to set a cloud */
    if (!s->flaming && !s->i[1] && vlen(p[0] - t[0], p[1] - t[1]) < *(const float *) (e + 0x344) + 13.0f) {
        unsigned long par = en_parent_alive(s->ref);
        if (!par) {
            unsigned long fg[16]; int nf = 0; unsigned long snap[SNAP]; unsigned n = snapshot(snap);
            for (unsigned k = 0; k < n && nf < 16; k++) if (alive(snap[k]) && *(const int *) (snap[k] + E_TYPE) == 160 && *(const int *) (snap[k] + E_VARIANT) == 350) fg[nf++] = snap[k];
            if (nf) par = fg[rnd((unsigned) nf)];
        }
        if (par) { en_t *ps = en_slot(par); if (ps->state == 0 || (ps->state == 1 && ps->i[1])) { ps->state = 2; s->i[1] = 1; } }
    }
}
static void hurt_mote(en_t *s, unsigned long e, float dmg) {
    (void) dmg; s->hurt_block = 0;
    if ((g_hit_flags & DMG_FIRE) && en_fire_from_not_player()) { s->flaming = 1; s->hurt_block = 1; }
    (void) e;
}

/* ------------------------------------------------------------------------------------------ Smokin 160.441 / Flamin 160.442 / Flamin Chain 160.443 (smokin.lua)
 * they orbit the closest fireplace (33.x); the colour of the fireplace (0 normal, 1 red, 2 blue, 3 purple) picks their sheet */
static float g6b_hp(unsigned long e) { return g_hp_off ? *(const float *) (e + g_hp_off) : 0.0f; }
static unsigned long g6b_closest_fireplace(const float *p) {
    unsigned long best = 0; float bd = 9999.0f; unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) { unsigned long v = snap[k]; if (!alive(v) || *(const int *) (v + E_TYPE) != 33) continue;
        float d = vlen(fpos(v)[0] - p[0], fpos(v)[1] - p[1]); if (d < bd) { bd = d; best = v; } }
    return best;
}
static const char *g6b_fire_suffix(int var) { return var == 1 ? "_red" : var == 2 ? "_blue" : var == 3 ? "_purple" : ""; }
static void g6b_splat(unsigned long e, int col) {                                  /* Isaac.Spawn(1000, 2, 10, pos) coloured with mod.flaminSplatCols[col] */
    float *p = fpos(e); unsigned long b = en_spawn(1000, 2, p[0], p[1], 0, 0, e, 10); if (!b) return;
    if (col == 2) en_paint(b, 1, 1, 1, 1, 0, 0, 0, 0, 0.0f, 150.0f / 255.0f, 250.0f / 255.0f);
    else if (col == 3) en_paint(b, 1, 1, 1, 1, 0, 0, 0, 0, 50.0f / 255.0f, 0.0f, 150.0f / 255.0f);
    else en_paint(b, 1, 1, 1, 1, 0, 0, 0, 0, 20.0f / 255.0f, 50.0f / 255.0f, -50.0f / 255.0f);
}
static float g6b_angle(float x, float y) { return atan2f(y, x) * 57.29578f; }
/* the shared orbit: parent position + parent velocity * 10 + Vector(-1,1):Resized(distance):Rotated(offset + 45); velocity = toward it at max(speed/3, dist/10) */
static void g6b_orbit(en_t *s, unsigned long e, unsigned long par, float distance, float speed, int reverse) {
    float *p = fpos(e), *v = fvel(e); const float *pp = fpos(par), *pv = fvel(par);
    if (!s->i[0]) { s->f[0] = g6b_angle(pp[0] - p[0], pp[1] - p[1]); s->i[0] = 1; }
    s->f[0] += reverse ? -speed : speed;
    float ox, oy; vrot(-1.0f, 1.0f, s->f[0] + 45.0f, &ox, &oy); vresize(&ox, &oy, distance);
    float tx = pp[0] + pv[0] * 10.0f + ox - p[0], ty = pp[1] + pv[1] * 10.0f + oy - p[1], len = vlen(tx, ty);
    vresize(&tx, &ty, fmaxf(speed / 3.0f, len / 10.0f)); v[0] = tx; v[1] = ty;
}

/* Smokin: state 0 idle (orbits the fireplace) / 1 FUCKINGPISSED (hunts the player); ref = d.Parent; f[0] = orbitoffset (i[0] set); i[1] = attacking, i[2] = flamin, i[3] = savedFireCol (-1 nil);
 * f[1] = value, f[2] = currenthealth (i[4] set) */
static void ai_smokin(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    en_sprite_offset(s, e, 0.0f, -10.0f);
    if (!s->init) {
        en_set_gridcoll(e, 0); en_set_collision(e, ENTCOLL_NONE); s->cd = 30; s->i[3] = -1;
        unsigned long fp = g6b_closest_fireplace(p);
        if (fp && g6b_hp(fp) > 1.0f) { s->state = 0; s->ref = fp; s->f[2] = g6b_hp(fp); s->i[4] = 1; }
        else { en_flag_set(e, EF_FLAG_NO_REWARD); en_remove(e); s->init = 1; return; }
        s->init = 1;
    } else s->cd++;
    float distance = 20.0f, speed = 3.0f;
    unsigned long par = en_parent_alive(s->ref);
    if (s->state == 0) {
        if (par) {
            if (!s->i[4]) { s->f[2] = g6b_hp(par); s->i[4] = 1; }
            if (g6b_hp(par) < 2.0f) {
                s->state = 1; s->cd = 0; en_replay(s, "Angered"); s->i[0] = 0; en_set_collision(e, ENTCOLL_PLAYEROBJECTS);
                s->ref = g_first_player ? g_first_player() : 0; par = s->ref;
            } else if (g6b_hp(par) < s->f[2]) { s->i[4] = 0; en_replay(s, "Angy"); }              /* d.currenthealth = npc.Parent.Hitpoints: a typo in the Lua, it becomes nil */
            else if (en_finished(s, "Angy") || strcmp(s->anim, "Angy")) en_play(s, "Float");
            if (en_finished(s, "Angered")) en_play(s, "FloatAnger");
            if (en_event(s, "Angy", "Shoot")) {
                en_sound(SOUND_BLOODSHOOT, 1.0f, 1.0f);
                float dx = t[0] - p[0], dy = t[1] - p[1]; vresize(&dx, &dy, 7.0f);
                en_pp_t pp; memset(&pp, 0, sizeof pp); unsigned long pr = en_shoot_p(e, p[0], p[1], dx, dy, &pp);
                if (pr) en_sheet(pr, 0, "gfx/projectiles/brisket_tear.png");
            }
        } else {
            s->state = 1; s->cd = 0; en_replay(s, "FireStart"); s->i[0] = 0; en_set_collision(e, ENTCOLL_PLAYEROBJECTS);
            s->ref = g_first_player ? g_first_player() : 0; par = s->ref;
        }
    } else {
        distance = 140.0f; speed = 5.0f;
        if (s->i[1]) {
            if (s->f[1] < 4.0f) s->f[1] += 0.3f;
            speed = 5.0f - s->f[1];
            if (en_finished(s, "FireStart")) { s->i[1] = 0; s->cd = 0; }
            else if (en_event(s, "FireStart", "FWOOM")) { en_sound(FFSND_FireLight, 1.5f, (float) rint_(110, 130) / 100.0f); s->i[2] = 1; }
            else if (en_event(s, "FireStart", "Firestop")) s->i[2] = 0;
            else en_play(s, "FireStart");
        } else {
            en_play(s, "FloatAnger");
            if (en_scare_conf(e)) { if (s->f[1] < 4.0f) s->f[1] += 0.3f; } else if (s->f[1] > 0.0f) s->f[1] -= 0.3f;
            speed = 5.0f - s->f[1];
            if (s->cd > 15 && !en_scare_conf(e)) s->i[1] = 1;
        }
    }
    if (s->state == 0 || s->state == 1) {
        if (s->i[2]) { if (s->age % 2 == 1) { fire_t *f = en_fire_new(e, p[0], p[1], 0, 0, 0); if (f) { f->parent = e; en_fire_go(f); } } }
        else {
            float rx, ry; vrot(-v[0], -v[1], (float) (rint_(1, 40) - 20), &rx, &ry);
            en_smoke(e, p[0], p[1], rx, ry, -10.0f, (float) rint_(1, 360), 0, 0, 0, 0, 0, 0, 0, 0);
        }
    }
    if (par) {
        g6b_orbit(s, e, par, distance, speed, 0);
        if (par == (g_first_player ? g_first_player() : 0)) return;                                   /* a player has no fire colour */
        int col = *(const int *) (par + E_VARIANT);
        if (col != s->i[3]) { char nm[64]; snprintf(nm, sizeof nm, "gfx/enemies/smokin/monster_smokin%s.png", g6b_fire_suffix(col)); en_sheet(e, 0, nm); s->i[3] = col; }
    } else s->ref = g_first_player ? g_first_player() : 0;
}

/* Flamin: state 0 idle; ref = fireplace (d.Parent), f[0] = orbitoffset (i[0] set), i[3] = savedFireCol, i[5] = numchains; the chain is spawned as 160.443 NPCs */
static void ai_flamin(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    en_sprite_offset(s, e, 0.0f, -10.0f);
    if (!s->init) {
        en_set_gridcoll(e, 0); en_set_collision(e, ENTCOLL_PLAYERONLY);
        en_flag_set(e, EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET | EF_FLAG_NO_REWARD); s->flaming = 1; s->cd = 30;
        unsigned long fp = g6b_closest_fireplace(p); s->init = 1;
        if (fp && g6b_hp(fp) > 1.0f) {
            s->state = 0; s->i[3] = *(const int *) (fp + E_VARIANT); s->ref = fp;
            const char *suf = g6b_fire_suffix(s->i[3]);
            if (suf[0] && g_load) { char nm[80]; snprintf(nm, sizeof nm, "gfx/enemies/smokin/monster_flamin%s.anm2", suf); g_load((void *) (e + E_SPRITE), nm); }
            float vx = fpos(fp)[0] - p[0], vy = fpos(fp)[1] - p[1];
            s->i[5] = ((*(const int *) (e + E_SUBTYPE) >> 1) & 15) + 1;
            for (int i = 1; i <= s->i[5]; i++) {
                unsigned long b = en_spawn(160, 443, p[0] + vx * (float) i / (float) (s->i[5] + 1), p[1] + vy * (float) i / (float) (s->i[5] + 1), 0, 0, e, 0); if (!b) continue;
                en_set_collision(b, ENTCOLL_PLAYERONLY); en_set_gridcoll(b, 0);
                en_t *bs = en_adopt(b); bs->f[1] = (float) i; bs->f[2] = (float) (s->i[5] + 1); bs->i[3] = s->i[3]; bs->ref = e; bs->ref2 = fp;
                if (suf[0] && g_load) { char nm[80]; snprintf(nm, sizeof nm, "gfx/enemies/smokin/chain%s.anm2", suf); g_load((void *) (b + E_SPRITE), nm); }
                en_flag_clear(b, EF_FLAG_APPEAR);
            }
        } else { en_kill(e); return; }
        ENLOG(201, 1, "ffenemy: not done: Flamin's CollisionDamage = 0 once its fireplace goes out (the field is not measured)");
    } else s->cd++;
    float distance = s->i[5] ? 25.0f * (float) s->i[5] : 50.0f, speed = 5.0f;
    if (en_scare_conf(e)) speed *= 0.5f;
    unsigned long par = en_parent_alive(s->ref);
    if (s->state == 0) {
        if (par && g6b_hp(par) > 1.0f) {
            en_play(s, "Fire");
            float hp = g6b_hp(par);
            if (hp < 2.0f) { s->ref = 0; par = 0; en_set_collision(e, ENTCOLL_NONE); }
            else if (hp < 3.0f) { speed = 7.0f; distance *= 2.0f; }
            else if (hp < 4.0f) { speed = 6.0f; distance *= 1.6f; }
            else if (hp < 5.0f) { speed = 5.5f; distance *= 1.3f; }
        } else { v[0] = v[1] = 0; g6b_splat(e, s->i[3]); en_kill(e); }
    }
    if (par) {
        g6b_orbit(s, e, par, distance, speed, *(const int *) (e + E_SUBTYPE) & 1);
        int col = *(const int *) (par + E_VARIANT);
        if (col != s->i[3]) { char nm[64]; snprintf(nm, sizeof nm, "gfx/enemies/smokin/monster_smokin%s.png", g6b_fire_suffix(col)); en_sheet(e, 0, nm); s->i[3] = col; }
    }
}
static void ai_flamin_chain(en_t *s, unsigned long e) {
    if (!s->i[6]) {
        s->i[6] = 1; s->flaming = 1;
        en_flag_set(e, EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET | EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_DEATH_TRIGGER | EF_FLAG_HIDE_HP_BAR | EF_FLAG_NO_FLASH_ON_DAMAGE | EF_FLAG_NO_REWARD);
        float *sc = (float *) (e + E_SPRITE + 0xdc); en_scale(e, sc[0] * 0.2f, sc[1] * 0.2f);
    }
    en_play(s, "Idle");
    unsigned long par = en_parent_alive(s->ref), ch = en_parent_alive(s->ref2);
    if (ch && par && g6b_hp(ch) > 1.0f) {
        float *p = fpos(e); const float *p1 = fpos(par), *p2 = fpos(ch); float vx = p2[0] - p1[0], vy = p2[1] - p1[1], k = s->f[1] / s->f[2];
        p[0] = p2[0] - vx * k; p[1] = p2[1] - vy * k; en_sprite_offset(s, e, 0.0f, k * -10.0f);
        int col = *(const int *) (ch + E_VARIANT);
        if (col != s->i[3]) { char nm[64]; snprintf(nm, sizeof nm, "gfx/enemies/smokin/chain%s.png", g6b_fire_suffix(col)); en_sheet(e, 0, nm); s->i[3] = col; }
    } else { g6b_splat(e, s->i[3]); en_kill(e); }
}


static const enemy_t g_tab_g6b[] = {
    { 160, 350, ai_fumegeist, 0, hurt_fireproof, 0, 0, 3 },
    { 160, 351, ai_mote, 0, hurt_mote, 0, 0, 3 },
    { 160, 441, ai_smokin, 0, 0, 0, 0, 3 },
    { 160, 442, ai_flamin, 0, 0, 0, 0, 3 },
    { 160, 443, ai_flamin_chain, 0, 0, 0, 0, 3 },
};
#define N_TAB_G6B ((int) (sizeof g_tab_g6b / sizeof g_tab_g6b[0]))
