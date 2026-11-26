/* ffen_g15.h - Fiend Folio's custom grids, part D: a WHOLE-TILE SOLID and a MEMORY PER ROOM for the NPC pseudo-grids of ffen_g8.h / g9 / g10
 * (poops, rocks, the fire pot 183.1016, the coloured locks 183.1013-1015 / 1034 and their chain blocks 1017-1019 / 1035, the totems and their puzzle).
 * Lua: the mod draws all of them as StageAPI custom grids - real grids: a lock blocks its whole 40 px tile like the game's own key lock, and a grid that
 * was broken / used stays that way when the room is left and entered again. The port runs them as NPC entities (a circle of radius 14-20), which
 * is neither: the player crossed the corner of a tile, and the room's entities of a cleared room are not spawned again, so they were gone.
 * Included by ffenemy.h after ffen_g13.h; en_g15_tick() runs once per frame from en_burn_tick().
 *
 * SOLID. A pseudo-grid is solid exactly when its entity collision class is ALL: every AI of g8 / g9 / g10 already sets that (a lock while it stands,
 * a totem while it is raised, a rock / poop until it breaks, the fire pot) and clears it when the grid stops blocking. Each frame, from that
 * same fact, the 40 x 40 square of the tile pushes out what the engine's circle alone let in: the players, the ground monsters (grid collision
 * class GROUND) and, when their grid class says they collide with solids, tears and projectiles that ended up inside it (the corners of the
 * square the circle does not cover). Flyers (class WALLS) cross it, as they cross the game's own rocks and locks.
 * Not touched: the engine's collision table at room+0xe98 - the function that reads it (0x5a1cd0) is Room::CheckLine's (a line of sight / path
 * check with a threshold), not the physical collision; writing there would only change what the AI sees.
 *
 * MEMORY. One record per (floor, room descriptor, type, variant, tile): its state, its counters and whether it is gone (a lock / chain that broke).
 * It is kept up to date every tick of the entity (gm_note), so what the player did is never older than one frame when the room is left. A few
 * frames after entering a room (GM_WAIT, when the engine has spawned what the room file holds) gm_restore() compares the records with the room:
 * a grid that is missing is spawned again; one the file spawned again that had broken is removed; the saved state is applied to the rest once
 * the entity's own AI has initialised (it would overwrite it before). A new floor or a new run (the frame counter goes back) empties the records.
 * The engine's own way (the room descriptor's saved entities) is not used: NPCs of a cleared room are never spawned from it, and there is no
 * measured field for an "entity persists" flag. This is the same per-room store the machines use (sd_get in ffen_g12.h), now for the grids. */

#define GS_N 128

static int gs_is_grid(unsigned long e) {
    int t = *(const int *) (e + E_TYPE), v = *(const int *) (e + E_VARIANT);
    return (t == 183 || t == 184) && v >= 1000 && v < 1900;                  /* 1901-1903 are the key ghosts (G9_GHOST), not grids */
}
static int gs_solid(unsigned long e) {
    return gs_is_grid(e) && alive(e) && ent_coll_ok() && *(const int *) (e + OFF_ENT_COLLCLASS) == ENTCOLL_ALL;
}
/* the room's grid: tile (c, r) is centred on (40 + 40 c, 120 + 40 r) */
static float gs_cx(float x) { return 40.0f + 40.0f * floorf((x - 20.0f) / 40.0f); }
static float gs_cy(float y) { return 120.0f + 40.0f * floorf((y - 100.0f) / 40.0f); }

/* push the circle (p, r) out of the square of half side 20 centred (cx, cy): the shortest way out; 1 = it moved, d = how far */
static int gs_push(float *p, float r, float cx, float cy, float *d) {
    float nx = p[0] < cx - 20.0f ? cx - 20.0f : (p[0] > cx + 20.0f ? cx + 20.0f : p[0]);        /* the point of the square nearest to the circle's centre */
    float ny = p[1] < cy - 20.0f ? cy - 20.0f : (p[1] > cy + 20.0f ? cy + 20.0f : p[1]);
    float ex = p[0] - nx, ey = p[1] - ny, d2 = ex * ex + ey * ey, ox = p[0], oy = p[1];
    if (d2 >= r * r) return 0;
    if (d2 > 0.0001f) { float l = sqrtf(d2), k = (r - l) / l; p[0] += ex * k; p[1] += ey * k; }                 /* outside the square, inside r of it */
    else {                                                                                                     /* the centre is inside the square: out through the nearest side */
        float px = 20.0f - fabsf(p[0] - cx), py = 20.0f - fabsf(p[1] - cy);
        if (px < py) p[0] = cx + (p[0] >= cx ? 20.0f + r : -(20.0f + r)); else p[1] = cy + (p[1] >= cy ? 20.0f + r : -(20.0f + r));
    }
    *d = vlen(p[0] - ox, p[1] - oy);
    return 1;
}

static void gs_tick(void) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    unsigned long te[GS_N]; float tx[GS_N], ty[GS_N]; int nt = 0;
    for (unsigned k = 0; k < n && nt < GS_N; k++) {
        unsigned long o = snap[k]; if (!gs_solid(o)) continue;
        (void) te; te[nt] = o; tx[nt] = gs_cx(fpos(o)[0]); ty[nt] = gs_cy(fpos(o)[1]); nt++;
    }
    if (!nt) return;
    for (int i = 0, np = sl_nplayers(); i < np; i++) {                                           /* the players: always blocked (a flying player is blocked by rocks too) */
        unsigned long pl = sl_player(i); if (!pl || !alive(pl)) continue;
        float r = *(const float *) (pl + E_SIZE);
        for (int pass = 0; pass < 2; pass++) for (int t = 0; t < nt; t++) {
            float dd; if (!gs_push(fpos(pl), r, tx[t], ty[t], &dd)) continue;
            FFLOG(FFC_GRID_COLLISION, FFKEY(*(const int *) (te[t] + E_TYPE), *(const int *) (te[t] + E_VARIANT)), "player pushed %.1f px out of the tile of %d.%d at (%.0f,%.0f): whole-tile solid", (double) dd, *(const int *) (te[t] + E_TYPE), *(const int *) (te[t] + E_VARIANT), (double) tx[t], (double) ty[t]);
        }
    }
    for (unsigned k = 0; k < n; k++) {
        unsigned long o = snap[k]; if (!alive(o) || !ent_gcoll_ok()) continue;
        int ty_ = *(const int *) (o + E_TYPE), gc = *(const int *) (o + OFF_ENT_GRIDCOLL);
        if (ty_ == 2 || ty_ == 9) {                                                             /* tears, projectiles: inside a solid tile, and they collide with solids */
            if (gc != GRIDCOLL_BULLET && gc != GRIDCOLL_GROUND && gc != GRIDCOLL_NOPITS) continue;
            const float *p = fpos(o);
            for (int t = 0; t < nt; t++) if (fabsf(p[0] - tx[t]) < 19.0f && fabsf(p[1] - ty[t]) < 19.0f) {
                FFLOG(FFC_GRID_COLLISION, FFKEY(ty_, *(const int *) (o + E_VARIANT)), "%s %d.%d stopped inside the tile of %d.%d at (%.0f,%.0f) (the corner the circle does not cover)", ty_ == 2 ? "tear" : "projectile", ty_, *(const int *) (o + E_VARIANT), *(const int *) (te[t] + E_TYPE), *(const int *) (te[t] + E_VARIANT), (double) tx[t], (double) ty[t]);
                en_remove(o); break;
            }
            continue;
        }
        if (ty_ < 10 || ty_ >= 1000 || ty_ == 33 || ty_ == 183 || ty_ == 184 || gc != GRIDCOLL_GROUND) continue;     /* ground monsters (class GROUND); flyers have WALLS */
        float r = *(const float *) (o + E_SIZE);
        for (int pass = 0; pass < 2; pass++) for (int t = 0; t < nt; t++) {
            float dd; if (!gs_push(fpos(o), r, tx[t], ty[t], &dd)) continue;
            FFLOG(FFC_GRID_COLLISION, FFKEY(ty_, *(const int *) (o + E_VARIANT)), "%d.%d pushed %.1f px out of the tile of %d.%d at (%.0f,%.0f): whole-tile solid", ty_, *(const int *) (o + E_VARIANT), (double) dd, *(const int *) (te[t] + E_TYPE), *(const int *) (te[t] + E_VARIANT), (double) tx[t], (double) ty[t]);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------------------------ memory per room */
#define GM_N 160
#define GM_WAIT 6
typedef struct { int used, floor; unsigned long room; int type, var, sub, tx, ty; float x, y; int state, i[6], gone; } gm_t;
static gm_t g_gm[GM_N];
static int g_gm_floor = -1, g_gm_next, g_gm_lastfr = -1, g_gm_wait = -1; static unsigned long g_gm_room;

static int gm_tx(float x) { return (int) floorf((x - 20.0f) / 40.0f); }
static int gm_ty(float y) { return (int) floorf((y - 100.0f) / 40.0f); }
static int gm_floor_now(void) { unsigned long game = en_game(); return game ? *(const int *) game * 16 + *(const int *) (game + 4) : 0; }
static gm_t *gm_find(int type, int var, int tx, int ty) {
    for (int i = 0; i < GM_N; i++) { gm_t *r = &g_gm[i]; if (r->used && r->floor == g_gm_floor && r->room == g_gm_room && r->type == type && r->var == var && r->tx == tx && r->ty == ty) return r; }
    return 0;
}
static gm_t *gm_new(int type, int var, int sub, float x, float y) {
    gm_t *r = 0; for (int i = 0; i < GM_N && !r; i++) if (!g_gm[i].used) r = &g_gm[i];
    if (!r) r = &g_gm[g_gm_next++ % GM_N];
    memset(r, 0, sizeof *r); r->used = 1; r->floor = g_gm_floor; r->room = g_gm_room; r->type = type; r->var = var; r->sub = sub; r->x = x; r->y = y; r->tx = gm_tx(x); r->ty = gm_ty(y);
    return r;
}
static int gm_is_lock(int var) { int chain; return g9_col(var, &chain) >= 0; }                 /* a lock or a chain block: when it breaks it is gone */

/* the state of a grid that broke, without its loot or effects (pg_destroy minus those): what the room shows when it is entered again */
static void gm_pg_broken(en_t *s, unsigned long e) {
    const pg_def_t *d = pg_def(s->type, s->variant); if (!d) return;
    s->state = 1; s->cd2 = 0; en_set_collision(e, ENTCOLL_NONE); en_flag_set(e, EF_FLAG_RENDER_FLOOR);
    if (d->kind == PG_POOP) pg_poop_frame(s, e, 4); else if (d->kind == PG_ROCK) en_play(s, "rubble_alt");
}
/* give a grid that was respawned (or spawned again by the room file) what it had when the room was left; its AI has initialised already */
static void gm_apply(en_t *s, unsigned long e, const gm_t *r) {
    int v = s->variant;
    const pg_def_t *d = pg_def(s->type, v);
    if (d && d->kind != PG_FX) { s->i[0] = r->i[0]; if (r->state == 1) gm_pg_broken(s, e); }          /* hits taken; rubble when it broke (the AI keeps drawing the frames) */
    else if (g10_is_totem(v)) {
        int active = r->i[0] ? 1 : 0; char nm[40]; s->i[0] = active; s->i[1] = -1;
        snprintf(nm, sizeof nm, "%s_%s", s->i[3] ? "OffPillar" : "OnPillar", active ? "Up" : "Down"); en_play(s, nm);       /* its AI sets the collision and the layer from i[0] */
    } else if (g10_is_spikes(v)) {
        char nm[40]; s->i[0] = r->i[0]; s->i[2] = r->i[2];
        snprintf(nm, sizeof nm, "spikes_%s%s", s->i[0] ? "" : "off_", s->i[3] ? "blue" : "red"); en_play(s, nm);
    } else if (v == 1049) { s->i[0] = r->i[0]; s->i[2] = r->i[2]; s->i[3] = r->i[3]; }                    /* the auto timer: its frames are drawn from these every tick */
    else if (v == 1048) { s->i[2] = r->i[2]; }                                                            /* the timed button's countdown */
    FFLOG(FFC_GRID_RESTORE, FFKEY(s->type, v), "room %lx: %d.%d.%d at (%.0f,%.0f) got its saved state back (state %d, hits/active %d)", g_gm_room, s->type, v, s->sub, (double) r->x, (double) r->y, r->state, r->i[0]);
}

/* every tick of a pseudo-grid, after its AI: keep its record current (and give it its saved state the first time) */
static void gm_note(en_t *s, unsigned long e) {
    if (g_gm_wait <= GM_WAIT || !s->init) return;                                                        /* before the restore the room is not ours yet */
    float *p = fpos(e); int tx = gm_tx(p[0]), ty = gm_ty(p[1]);
    gm_t *r = gm_find(s->type, s->variant, tx, ty);
    if (!r) {
        r = gm_new(s->type, s->variant, s->sub, p[0], p[1]);
        FFLOG(FFC_GRID_SAVE, FFKEY(s->type, s->variant), "room %lx: %d.%d.%d at (%.0f,%.0f) is now remembered for this room", g_gm_room, s->type, s->variant, s->sub, (double) p[0], (double) p[1]);
    }
    if (s->gm_apply) { s->gm_apply = 0; gm_apply(s, e, r); return; }
    int was = r->state;
    r->state = s->state; for (int k = 0; k < 6; k++) r->i[k] = s->i[k]; r->sub = s->sub;
    if (g10_is_totem(s->variant)) r->i[0] = s->i[1] >= 0 ? s->i[1] : s->i[0];                           /* a totem in the middle of switching is saved as where it is going */
    if (gm_is_lock(s->variant) && s->state >= 1) r->gone = 1;                                            /* breaking or broken: it will not come back */
    if (was != r->state) FFLOG(FFC_GRID_SAVE, FFKEY(s->type, s->variant), "room %lx: %d.%d.%d at (%.0f,%.0f) changed state %d -> %d%s", g_gm_room, s->type, s->variant, s->sub, (double) p[0], (double) p[1], was, r->state, r->gone ? " (gone for good)" : "");
}

/* a few frames after entering a room: what the room should hold against what the engine spawned */
static void gm_restore(void) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {                                                                    /* what the file spawned: remember the new, deal with the known */
        unsigned long o = snap[k]; if (!alive(o) || !gs_is_grid(o)) continue;
        int t = *(const int *) (o + E_TYPE), v = *(const int *) (o + E_VARIANT); float *p = fpos(o);
        gm_t *r = gm_find(t, v, gm_tx(p[0]), gm_ty(p[1]));
        if (!r) { gm_new(t, v, *(const int *) (o + E_SUBTYPE), p[0], p[1]); continue; }
        if (r->gone) {
            FFLOG(FFC_GRID_RESTORE, FFKEY(t, v), "room %lx: %d.%d at (%.0f,%.0f) was spawned again by the room file but it had broken: removed", g_gm_room, t, v, (double) p[0], (double) p[1]);
            en_remove(o); continue;
        }
        if (en_find(t, v)) en_slot(o)->gm_apply = 1;
    }
    for (int i = 0; i < GM_N; i++) {                                                                      /* what is missing: spawn it again */
        gm_t *r = &g_gm[i]; if (!r->used || r->floor != g_gm_floor || r->room != g_gm_room || r->gone || !en_find(r->type, r->var)) continue;
        int found = 0;
        for (unsigned k = 0; k < n && !found; k++) {
            unsigned long o = snap[k]; if (!alive(o) || !gs_is_grid(o)) continue;
            if (*(const int *) (o + E_TYPE) == r->type && *(const int *) (o + E_VARIANT) == r->var && gm_tx(fpos(o)[0]) == r->tx && gm_ty(fpos(o)[1]) == r->ty) found = 1;
        }
        if (found) continue;
        unsigned long ne = en_spawn(r->type, r->var, r->x, r->y, 0, 0, 0, r->sub);
        if (ne) { en_slot(ne)->gm_apply = 1; FFLOG(FFC_GRID_RESTORE, FFKEY(r->type, r->var), "room %lx: %d.%d.%d at (%.0f,%.0f) was not there on entering: spawned again", g_gm_room, r->type, r->var, r->sub, (double) r->x, (double) r->y); }
        else FFLOG(FFC_GRID_LOST, FFKEY(r->type, r->var), "room %lx: %d.%d.%d at (%.0f,%.0f) could not be spawned again", g_gm_room, r->type, r->var, r->sub, (double) r->x, (double) r->y);
    }
}

static void en_g15_tick(void) {
    unsigned long room = sl_room_key(); if (!room) { g_gm_wait = -1; return; }
    int fl = gm_floor_now(), fr = g_frame_fn ? g_frame_fn() : -1;
    if (fl != g_gm_floor || (fr >= 0 && g_gm_lastfr >= 0 && fr + 30 < g_gm_lastfr)) {                 /* a new floor, or a new run (the frame counter went back) */
        memset(g_gm, 0, sizeof g_gm); g_gm_floor = fl; g_gm_next = 0; g_gm_room = 0;
    }
    g_gm_lastfr = fr;
    if (room != g_gm_room) { g_gm_room = room; g_gm_wait = 0; }
    else if (g_gm_wait >= 0 && g_gm_wait < 1000000) g_gm_wait++;
    if (g_gm_wait == GM_WAIT) gm_restore();
    gs_tick();
}
