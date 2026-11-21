/* ffen_g9.h - Fiend Folio's PUZZLE LOCKS: the coloured locks (183.1013 blue / 1014 green / 1015 red / 1034 gray), the chain blocks that break with them (183.1017 / 1018 / 1019 / 1035),
 * the puzzle keys (5.752.0 blue / 1 green / 2 red) and the key ghosts that carry a key to its lock. Lua: ffscripts/puzzlelocks.lua.
 * The mod draws the locks as StageAPI custom grids (GRID_WALL); the port runs them as NPC pseudo-grids that never move and block (ffen_g8.h's way), the ghosts as NPCs
 * 183.1901-1903 (the mod's familiars 3.1062-1064 are real familiars; the port has no custom familiars, so the ghost is an NPC that follows the player's trail).
 * Included by ffenemy.h after ffen_g8.h.
 *
 * lock   s->state 0 standing / 1 breaking / 2 broken; i[0] = FrameCount, i[1] = BreakStarted, i[2] = a break is scheduled, i[3] = BreakTriggered, i[4] = Unlock asked by the ghost,
 *        i[5] = BreakDelay (the placeholder's subtype); ref = the ghost that was sent to it.
 * ghost  s->state 0; ref = its key, ref2 = the lock it was sent to, i[1] = serial. */

#define G9_GHOST 1901
static const int g9_lock_var[4] = { 1013, 1014, 1015, 1034 }, g9_chain_var[4] = { 1017, 1018, 1019, 1035 };
static int g9_col(int var, int *chain) {
    for (int i = 0; i < 4; i++) { if (g9_lock_var[i] == var) { *chain = 0; return i; } if (g9_chain_var[i] == var) { *chain = 1; return i; } }
    return -1;
}

/* where the first player has been: the ghosts follow it (the Lua: familiar:FollowParent()) */
static float g9_trail[128][2]; static unsigned g9_trail_i;
static int g9_serial;

static void g9_hurt_none(en_t *s, unsigned long e, float dmg) { (void) e; (void) dmg; s->hurt_block = 1; }

/* ---- breaking the chain blocks next to a lock that was opened: a breadth first walk over the 4 neighbours (puzzlelocks.lua lockAI) */
static void g9_chain_break(unsigned long lock, int col) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    struct { int off; float x, y; } q[512]; int qh = 0, qt = 0;
    float vis[512][2]; int nv = 0;
    const float *lp = fpos(lock);
    static const float dx[4] = { -40, 40, 0, 0 }, dy[4] = { 0, 0, -40, 40 };
    for (int k = 0; k < 4; k++) { q[qt].off = 0; q[qt].x = lp[0] + dx[k]; q[qt].y = lp[1] + dy[k]; qt++; }
    while (qh < qt) {
        int off = q[qh].off; float x = q[qh].x, y = q[qh].y; qh++;
        int seen = 0; for (int i = 0; i < nv; i++) if (fabsf(vis[i][0] - x) < 8.0f && fabsf(vis[i][1] - y) < 8.0f) { seen = 1; break; }
        if (seen) continue;
        if (nv < 512) { vis[nv][0] = x; vis[nv][1] = y; nv++; }
        for (unsigned k = 0; k < n; k++) {
            unsigned long c = snap[k]; if (!alive(c) || *(const int *) (c + E_TYPE) != 183 || *(const int *) (c + E_VARIANT) != g9_chain_var[col]) continue;
            const float *cp = fpos(c); if (fabsf(cp[0] - x) > 12.0f || fabsf(cp[1] - y) > 12.0f) continue;
            en_t *cs = en_slot(c); if (!cs->init) continue;
            int started = cs->i[0] + off + cs->i[5];
            if (!cs->i[2] || started < cs->i[1]) cs->i[1] = started;
            cs->i[2] = 1;
            int nextoff = cs->i[1] - cs->i[0];
            for (int d = 0; d < 4 && qt < 512; d++) { q[qt].off = nextoff; q[qt].x = cp[0] + dx[d]; q[qt].y = cp[1] + dy[d]; qt++; }
            break;
        }
    }
}

/* the lock waits for a player carrying a ghost of its colour */
static void g9_lock_seek(en_t *s, unsigned long e, int col) {
    unsigned long pl = g_first_player ? g_first_player() : 0; if (!pl) return;
    const float *p = fpos(e), *pp = fpos(pl), *pv = fvel(pl);
    float radius = *(const float *) (pl + 0x344) + 26.0f;
    if (vlen(pp[0] + pv[0] - p[0], pp[1] + pv[1] - p[1]) > radius) return;
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long g = snap[k]; if (!alive(g) || *(const int *) (g + E_TYPE) != 183 || *(const int *) (g + E_VARIANT) != G9_GHOST + col) continue;
        en_t *gs = en_slot(g); if (!gs->init || gs->ref2) continue;
        gs->ref2 = e; s->ref = g; return;
    }
}

static void ai_lock(en_t *s, unsigned long e) {
    int chain = 0, col = g9_col(s->variant, &chain); if (col < 0) return;
    float *p = fpos(e), *v = fvel(e);
    if (!s->init) {
        s->init = 1; s->state = 0; s->f[10] = p[0]; s->f[11] = p[1]; s->i[5] = s->sub;
        en_flag_clear(e, EF_FLAG_APPEAR);
        en_set_gridcoll(e, GRIDCOLL_NONE);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET);
        en_set_collision(e, ENTCOLL_ALL);
        en_play(s, "Idle");
        ENLOG(340, 1, "ffenemy: not done: the Tricolor Key item, the Dungeon Master / Key Fiend unlocking, the colour-blind sheets, the puzzle-lock achievements and the lock as a path-finding obstacle");
    }
    p[0] = s->f[10]; p[1] = s->f[11]; v[0] = v[1] = 0;
    s->i[0]++;
    if (s->i[2]) {                                                                              /* commonBlockAI */
        if (!s->i[3] && s->i[0] > s->i[1]) { s->i[3] = 1; s->state = 1; en_replay(s, "Breaking"); }
        if (s->i[3]) {
            if (en_event(s, "Breaking", "SndPlay")) en_sound(SOUND_METAL_BLOCKBREAK, 0.9f, 1.25f + frand() * 0.1f);
            if (en_event(s, "Breaking", "Unlock")) {
                en_sound(SOUND_UNLOCK00, 1.2f, 1.2f + frand() * 0.2f);
                s->state = 2; en_set_collision(e, ENTCOLL_NONE);                                /* the wall is gone; the effect keeps playing the break on the floor */
            }
            if (s->state == 2 && en_finished(s, "Breaking")) en_flag_set(e, EF_FLAG_RENDER_FLOOR);
        }
    }
    if (chain || s->state >= 1) return;
    if (s->ref && !alive(s->ref) && !s->i[3]) s->ref = 0;                                       /* the friend is gone: the lock can still open */
    if (!s->ref) { if (col < 3) g9_lock_seek(s, e, col); }
    else if (s->i[4]) {
        s->i[4] = 0; s->i[1] = s->i[0]; s->i[2] = 1;
        g9_chain_break(e, col);
    }
}

/* ---- the key ghost (the familiar's UseKey / Poof animations) */
static void ai_kghost(en_t *s, unsigned long e) {
    int col = s->variant - G9_GHOST;
    float *p = fpos(e), *v = fvel(e);
    if (!s->init) {
        s->init = 1; s->state = 0; s->i[1] = ++g9_serial;
        en_flag_clear(e, EF_FLAG_APPEAR);
        en_set_gridcoll(e, GRIDCOLL_NONE); en_set_collision(e, ENTCOLL_NONE);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET | EF_FLAG_FRIENDLY);
        en_sprite_offset(s, e, 0, -10.0f);
        en_play(s, "Appear");
        ENLOG(341, 1, "ffenemy: not done: the key ghosts as real familiars (they follow the player's trail here; no co-op owner, no Esau/T.Lazarus form switch)");
    }
    if (en_finished(s, "Appear")) en_play(s, "Move");
    if (en_finished(s, "Poof")) { en_remove(e); return; }
    en_flip(e, v[0] <= 0.0f);
    if (en_event(s, "Poof", "UseKey")) {
        if (s->ref2 && alive(s->ref2)) en_slot(s->ref2)->i[4] = 1;
        if (s->ref && alive(s->ref)) en_remove(s->ref);                                          /* the key is used up */
        s->ref = 0;
    }
    if (s->ref2 && !alive(s->ref2) && strcmp(s->anim, "Poof")) s->ref2 = 0;
    if (s->ref2) {
        const float *lp = fpos(s->ref2);
        v[0] = (lp[0] - p[0]) * 0.2f; v[1] = (lp[1] - p[1]) * 0.2f;
        float sz = *(const float *) (e + 0x344) + 26.0f;
        float dx = lp[0] - p[0], dy = lp[1] - p[1];
        if (dx * dx + dy * dy <= sz * sz) en_play(s, "Poof");
    } else {
        unsigned long pl = g_first_player ? g_first_player() : 0;
        if (!pl) return;
        unsigned long snap[SNAP]; unsigned n = snapshot(snap); int rank = 0;
        for (unsigned k = 0; k < n; k++) {
            unsigned long g = snap[k]; if (g == e || !alive(g) || *(const int *) (g + E_TYPE) != 183) continue;
            int gv = *(const int *) (g + E_VARIANT); if (gv < G9_GHOST || gv > G9_GHOST + 2) continue;
            en_t *gs = en_slot(g); if (gs->init && gs->i[1] < s->i[1] && !gs->ref2) rank++;
        }
        const float *t = g9_trail[(g9_trail_i - (unsigned) (10 * (rank + 1))) & 127];
        float tx = t[0], ty = t[1]; if (g9_trail_i < 10u * (unsigned) (rank + 1)) { tx = fpos(pl)[0]; ty = fpos(pl)[1]; }
        v[0] = (tx - p[0]) * 0.2f; v[1] = (ty - p[1]) * 0.2f;
    }
    (void) col;
}

/* ---- the puzzle key pickup 5.752.sub: Appear, Idle; a player touching it takes it (Collect) and a ghost of its colour comes out of it */
static void g9_key(unsigned long e) {
    en_t *s = en_slot(e); float *p = fpos(e);
    if (!s->init) { s->init = 1; s->state = 0; en_set_collision(e, ENTCOLL_NONE); en_replay(s, "Appear"); s->age = 0; }
    s->age++; s->af++;
    if (s->aloop && s->alen > 0 && s->af >= s->alen) s->af = 0;
    if (s->state == 0) {
        if (s->age == 25) en_sound(SOUND_KEY_DROP0, 1.0f, 1.25f);
        if (en_finished(s, "Appear")) en_play(s, "Idle");
        unsigned long pl = g_first_player ? g_first_player() : 0;
        if (pl && s->age >= 25) {
            const float *pp = fpos(pl);
            if (vlen(pp[0] - p[0], pp[1] - p[1]) < *(const float *) (pl + 0x344) + 12.0f + 4.0f) {
                s->state = 1; fvel(e)[0] = fvel(e)[1] = 0; en_replay(s, "Collect");
                en_sound(SOUND_KEYPICKUP_GAUNTLET, 1.0f, 1.47f + frand() * 0.06f);
                unsigned long g = en_spawn(183, G9_GHOST + (s->sub < 3 ? s->sub : 0), pp[0], pp[1], 0, 0, 0, 0);
                if (g) { en_t *gs = en_slot(g); gs->ref = e; }
            }
        }
    } else if (s->state == 1 && s->af >= 5) { en_visible(e, 0); s->state = 2; }
}

static void en_g9_tick(void) {
    unsigned long pl = g_first_player ? g_first_player() : 0;
    if (pl) { g9_trail_i++; g9_trail[g9_trail_i & 127][0] = fpos(pl)[0]; g9_trail[g9_trail_i & 127][1] = fpos(pl)[1]; }
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long e = snap[k]; if (!alive(e)) continue;
        if (*(const int *) (e + E_TYPE) == 5 && *(const int *) (e + E_VARIANT) == 752) g9_key(e);
    }
}

static const enemy_t g_tab_g9[] = {
    { 183, 1013, ai_lock, 0, g9_hurt_none, 0, 0, 0 }, { 183, 1014, ai_lock, 0, g9_hurt_none, 0, 0, 0 }, { 183, 1015, ai_lock, 0, g9_hurt_none, 0, 0, 0 }, { 183, 1034, ai_lock, 0, g9_hurt_none, 0, 0, 0 },
    { 183, 1017, ai_lock, 0, g9_hurt_none, 0, 0, 0 }, { 183, 1018, ai_lock, 0, g9_hurt_none, 0, 0, 0 }, { 183, 1019, ai_lock, 0, g9_hurt_none, 0, 0, 0 }, { 183, 1035, ai_lock, 0, g9_hurt_none, 0, 0, 0 },
    { 183, 1901, ai_kghost, 0, g9_hurt_none, 0, 1, 0 }, { 183, 1902, ai_kghost, 0, g9_hurt_none, 0, 1, 0 }, { 183, 1903, ai_kghost, 0, g9_hurt_none, 0, 1, 0 },
};
#define N_TAB_G9 ((int) (sizeof g_tab_g9 / sizeof g_tab_g9[0]))
