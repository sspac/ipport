/* ffen_g10.h - Fiend Folio's TOTEMS: the red / blue pillars that go up and down (183.1039 / 1040 big, 1044 / 1045 lil), the totem spikes (1046 / 1047), the lever (1041), the pressure switch (1042),
 * the timed button (1048) and the auto timer (1049). Lua: ffscripts/guwah/grids/totems.lua (FiendFolio:SwitchTotems()).
 * The mod makes them StageAPI custom grids; the port runs them as pinned NPC pseudo-grids (ffen_g8.h's way). Included by ffenemy.h after ffen_g9.h.
 *
 * totem   i[0] = IsActive, i[1] = QueuedActiveState (-1 none, 0 / 1), i[3] = blue.      spikes  i[0] = IsActive, i[2] = Poking.
 * lever / switch  i[0] = IsActive (the Lua starts with nil = false), i[2] = ActivateCooldown (lever) / SteppedOn (switch).
 * timed button  i[2] = Timer (-1 = nil), i[3] = the tick frame last beeped.     auto timer  i[0] = "On" (1) / "Off", i[2] = Timer, i[3] = CurrentTimeStep, i[4] = TimeSteps. */

#define G10_PRESS 150
#define G10_INTERVAL 30
static int g10_is_totem(int v) { return v == 1039 || v == 1040 || v == 1044 || v == 1045; }
static int g10_is_spikes(int v) { return v == 1046 || v == 1047; }

static void g10_pin(en_t *s, unsigned long e) { float *p = fpos(e), *v = fvel(e); if (!s->init) { s->f[10] = p[0]; s->f[11] = p[1]; } p[0] = s->f[10]; p[1] = s->f[11]; v[0] = v[1] = 0; }
static int g10_on_tile(unsigned long e, unsigned long pl) { const float *p = fpos(e), *q = fpos(pl); return fabsf(q[0] - p[0]) < 20.0f && fabsf(q[1] - p[1]) < 20.0f; }
static int g10_playing(const en_t *s) { return s->alen && (s->aloop || s->af < s->alen); }

/* FiendFolio:SwitchTotems(): everything that belongs to the totem puzzle changes state */
static void g10_switch(void) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long e = snap[k]; if (!alive(e) || *(const int *) (e + E_TYPE) != 183) continue;
        int v = *(const int *) (e + E_VARIANT); if (v < 1039 || v > 1049) continue;
        en_t *s = en_slot(e); if (!s->init) continue;
        char nm[40];
        if (g10_is_totem(v)) {
            int active = s->i[0]; if (s->i[1] >= 0) active = s->i[1];
            snprintf(nm, sizeof nm, "%s_%s", s->i[3] ? "OffPillar" : "OnPillar", active ? "Lower" : "Raise");
            en_replay(s, nm); en_sound(FFSND_GolemDoorClose, 2.0f, 0.8f); s->i[1] = active ? 0 : 1;
        } else if (g10_is_spikes(v)) {
            snprintf(nm, sizeof nm, "spikes_%s_%s", s->i[0] ? "unsummon" : "summon", s->i[3] ? "blue" : "red");
            en_replay(s, nm); s->i[0] = !s->i[0];
        } else if (v == 1042) { en_replay(s, s->i[0] ? "Button_SwitchedOn" : "Button_SwitchedOff"); s->i[0] = !s->i[0]; }
        else if (v == 1041) { en_replay(s, s->i[0] ? "Lever_SwitchedOn" : "Lever_SwitchedOff"); s->i[0] = !s->i[0]; s->i[2] = 10; }
        else if (v == 1048) {
            if (s->i[2] >= 0) { s->i[2] = -1; en_replay(s, "TimedButton_SwitchedOn"); } else { s->i[2] = G10_PRESS; en_replay(s, "TimedButton_SwitchedOff"); }
        } else if (v == 1049) {
            en_replay(s, s->i[0] ? "Timer_SwitchedOff" : "Timer_SwitchedOn"); s->i[0] = !s->i[0]; s->i[2] = G10_INTERVAL; s->i[3] = s->i[4];
        }
    }
}

static void g10_hurt_none(en_t *s, unsigned long e, float dmg) { (void) e; (void) dmg; s->hurt_block = 1; }

static void ai_totem(en_t *s, unsigned long e) {
    int blue = s->variant == 1040 || s->variant == 1045;
    g10_pin(s, e);
    if (!s->init) {
        s->init = 1; s->i[3] = blue; s->i[0] = !blue; s->i[1] = -1;
        en_flag_clear(e, EF_FLAG_APPEAR);
        en_set_gridcoll(e, GRIDCOLL_NONE);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET);
        en_play(s, blue ? "OffPillar_Down" : "OnPillar_Up");
        ENLOG(350, 1, "ffenemy: not done: totems - the per-backdrop colour skins (amber / dark blue ...), the launch of the player standing on a rising totem (it is pushed aside instead), the lil totem letting flyers pass, Giga Bomb removal");
    }
    const char *pre = blue ? "OffPillar" : "OnPillar"; char nm[40];
    snprintf(nm, sizeof nm, "%s_Raise", pre);
    if (en_finished(s, nm)) { snprintf(nm, sizeof nm, "%s_Up", pre); en_play(s, nm); }
    else {
        snprintf(nm, sizeof nm, "%s_Lower", pre);
        if (en_finished(s, nm)) { snprintf(nm, sizeof nm, "%s_Down", pre); en_play(s, nm); }
        else if (en_event(s, nm, "Down")) { s->i[0] = 0; s->i[1] = -1; }
        else {
            snprintf(nm, sizeof nm, "%s_Raise", pre);
            if (en_event(s, nm, "Up")) {
                s->i[0] = 1; s->i[1] = -1;
                unsigned long pl = g_first_player ? g_first_player() : 0;
                if (pl && g10_on_tile(e, pl)) {                                         /* the Lua throws the player up; here it is pushed out of the tile */
                    const float *p = fpos(e), *q = fpos(pl); float *pv = fvel(pl);
                    float dx = q[0] - p[0], dy = q[1] - p[1], l = vlen(dx, dy); if (l < 1.0f) { dx = 0; dy = 1; l = 1; }
                    pv[0] += dx / l * 9.0f; pv[1] += dy / l * 9.0f;
                    en_sound(FFSND_SmashHitWeak, 1.0f, 1.0f); en_sound(FFSND_ZoomAway, 0.6f, 1.0f);
                }
            }
        }
    }
    en_set_collision(e, s->i[0] ? ENTCOLL_ALL : ENTCOLL_NONE);
    if (s->i[0]) en_flag_clear(e, EF_FLAG_RENDER_FLOOR); else en_flag_set(e, EF_FLAG_RENDER_FLOOR);
}

static void ai_tspikes(en_t *s, unsigned long e) {
    int blue = s->variant == 1047; char nm[40];
    g10_pin(s, e);
    if (!s->init) {
        s->init = 1; s->i[3] = blue;
        en_flag_clear(e, EF_FLAG_APPEAR); en_set_gridcoll(e, GRIDCOLL_NONE); en_set_collision(e, ENTCOLL_NONE);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET);
        if (blue) en_play(s, "spikes_off_blue"); else { en_play(s, "spikes_red"); s->i[0] = 1; s->i[2] = 1; }
        en_flag_set(e, EF_FLAG_RENDER_FLOOR);
    }
    snprintf(nm, sizeof nm, "spikes_summon_%s", blue ? "blue" : "red"); if (en_event(s, nm, "Up")) s->i[2] = 1;
    snprintf(nm, sizeof nm, "spikes_unsummon_%s", blue ? "blue" : "red"); if (en_event(s, nm, "Down")) s->i[2] = 0;
    if (s->i[2]) {
        unsigned long pl = g_first_player ? g_first_player() : 0;
        if (pl) {
            const float *p = fpos(e), *q = fpos(pl);
            if (vlen(q[0] - p[0], q[1] - p[1]) <= *(const float *) (pl + 0x344) + 11.0f) {
                en_hit_player(2.0f);
                float dx = q[0] - p[0], dy = q[1] - p[1], l = vlen(dx, dy); if (l < 1.0f) { dx = 1; dy = 0; l = 1; }
                fvel(pl)[0] += dx / l * 1.5f; fvel(pl)[1] += dy / l * 1.5f;
            }
        }
    }
}

static void ai_tlever(en_t *s, unsigned long e) {
    g10_pin(s, e);
    if (!s->init) {
        s->init = 1; s->i[2] = 0;
        en_flag_clear(e, EF_FLAG_APPEAR); en_set_gridcoll(e, GRIDCOLL_NONE); en_set_collision(e, ENTCOLL_ALL);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET | EF_FLAG_RENDER_FLOOR);
        en_play(s, "Lever_On");
        ENLOG(351, 1, "ffenemy: not done: the totem lever is not pulled by a player touching it (only by what hurts it: tears, bombs, lasers), nor by the Sanguine Hook");
    }
    if (!g10_playing(s)) s->i[2]--;                                                     /* IsFinished(): the 1-frame loop of "Lever_On" counts as finished in the Lua */
}
static void hurt_tlever(en_t *s, unsigned long e, float dmg) {
    (void) e; s->hurt_block = 1;
    if (dmg > 0 && s->i[2] <= 0) { en_sound(SOUND_METAL_DOOR_OPEN, 0.6f, 2.0f); g10_switch(); }
}

static void ai_tswitch(en_t *s, unsigned long e) {
    g10_pin(s, e);
    if (!s->init) {
        s->init = 1;
        en_flag_clear(e, EF_FLAG_APPEAR); en_set_gridcoll(e, GRIDCOLL_NONE); en_set_collision(e, ENTCOLL_NONE);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET | EF_FLAG_RENDER_FLOOR);
        en_play(s, "Button_On");
        ENLOG(352, 1, "ffenemy: not done: the totem buttons are pressed only by the player (not by Dople / Last Judgement / Aper)");
    }
    unsigned long pl = g_first_player ? g_first_player() : 0; int on = pl && g10_on_tile(e, pl);
    if (s->i[2]) { if (!on) { en_sound(SOUND_BUTTON_PRESS, 1.0f, 1.0f); s->i[2] = 0; } }
    else if (on) { g10_switch(); s->i[2] = 1; en_sound(SOUND_BUTTON_PRESS, 1.0f, 1.0f); }
}

static void ai_ttimed(en_t *s, unsigned long e) {
    g10_pin(s, e);
    if (!s->init) {
        s->init = 1; s->i[2] = -1;
        en_flag_clear(e, EF_FLAG_APPEAR); en_set_gridcoll(e, GRIDCOLL_NONE); en_set_collision(e, ENTCOLL_NONE);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET | EF_FLAG_RENDER_FLOOR);
        en_play(s, "TimedButton_On");
    }
    if (s->i[2] >= 0) {
        s->i[2]--;
        if (s->i[2] <= 0) {
            en_replay(s, "TimedButton_SwitchedOn"); en_sound(SOUND_BUTTON_PRESS, 1.0f, 1.0f);
            en_sound(SOUND_BEEP, 0.25f, 0.5f + (float) (G10_PRESS - s->i[2]) / 300.0f);
            g10_switch(); s->i[2] = -1;
        } else if (!g10_playing(s)) {
            int frame = s->i[2] / (G10_PRESS / 4) + 1;
            if (s->i[3] != frame) en_sound(SOUND_BEEP, 0.25f, 0.5f + (float) (G10_PRESS - s->i[2]) / 300.0f);
            s->i[3] = frame;
            en_set_frame(s, "TimedButton_TickingOn", frame);
        }
    } else {
        unsigned long pl = g_first_player ? g_first_player() : 0;
        if (pl && g10_on_tile(e, pl)) { g10_switch(); s->i[2] = G10_PRESS; en_sound(SOUND_BUTTON_PRESS, 1.0f, 1.0f); }
    }
}

static void ai_ttimer(en_t *s, unsigned long e) {
    g10_pin(s, e);
    if (!s->init) {
        s->init = 1; s->i[0] = 1; s->i[4] = s->sub; s->i[3] = s->sub; s->i[2] = 30;
        en_flag_clear(e, EF_FLAG_APPEAR); en_set_gridcoll(e, GRIDCOLL_NONE); en_set_collision(e, ENTCOLL_ALL);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET);
        en_set_frame(s, "Timer_On", 9 - s->i[3]);
    }
    if (!(s->alen && s->af < s->alen && !strncmp(s->anim, "Timer_Switched", 14))) {
        char nm[24]; snprintf(nm, sizeof nm, "Timer_%s", s->i[0] ? "On" : "Off");
        en_set_frame(s, nm, 9 - s->i[3]);
    }
    s->i[2]--;
    if (s->i[2] <= 0 && s->i[4] > 0) {
        s->i[3]--;
        if (s->i[3] <= 0) { s->i[3] = s->i[4]; g10_switch(); en_sound(SOUND_STATIC, 0.25f, 1.0f); }
        float pitch = 0.5f + 0.5f * (1.0f - (float) (s->i[3] + 1) / (float) s->i[4]);
        en_sound(SOUND_BEEP, 0.25f, pitch);
        s->i[2] = G10_INTERVAL;
    }
}

static const enemy_t g_tab_g10[] = {
    { 183, 1039, ai_totem, 0, g10_hurt_none, 0, 0, 0 }, { 183, 1040, ai_totem, 0, g10_hurt_none, 0, 0, 0 },
    { 183, 1044, ai_totem, 0, g10_hurt_none, 0, 0, 0 }, { 183, 1045, ai_totem, 0, g10_hurt_none, 0, 0, 0 },
    { 183, 1046, ai_tspikes, 0, g10_hurt_none, 0, 0, 0 }, { 183, 1047, ai_tspikes, 0, g10_hurt_none, 0, 0, 0 },
    { 183, 1041, ai_tlever, 0, hurt_tlever, 0, 0, 0 }, { 183, 1042, ai_tswitch, 0, g10_hurt_none, 0, 0, 0 },
    { 183, 1048, ai_ttimed, 0, g10_hurt_none, 0, 0, 0 }, { 183, 1049, ai_ttimer, 0, g10_hurt_none, 0, 0, 0 },
};
#define N_TAB_G10 ((int) (sizeof g_tab_g10 / sizeof g_tab_g10[0]))
