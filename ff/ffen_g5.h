/* ------------------------------------------------------------------------------------------ Cellar (27): the Lua of each, with the pieces the engine side does not give marked "not done".
 * Included by ffenemy.h after ffen_g4.h. Field use per monster is noted at its AI (s->cd = StateFrame/timers, s->i[], s->f[] = the Lua's d.<field>). */

/* mod:isScare = FEAR | SHRINK; mod:isConfuse = CONFUSION */
static int en_scare(unsigned long e) { return en_flag(e, EF_FLAG_FEAR | EF_FLAG_SHRINK); }
static int en_scare_conf(unsigned long e) { return en_flag(e, EF_FLAG_FEAR | EF_FLAG_SHRINK | EF_FLAG_CONFUSION); }
static int en_player_has(int id) { unsigned long p = g_first_player ? g_first_player() : 0; return p && g_has_item && g_has_item(p, id, 0); }
static float *en_target_vel(void) { unsigned long p = g_first_player ? g_first_player() : 0; return p ? fvel(p) : 0; }
/* Room:GetRandomPosition(1): a random point inside the room (the walls' inner edge, 1 px in) */
static void en_random_room_pos(float *x, float *y) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
    float x0 = en_room_x(1) - 20.0f, x1 = en_room_x(w - 2) + 20.0f, y0 = en_room_y(1) - 20.0f, y1 = en_room_y(h - 2) + 20.0f;
    *x = x0 + frand() * (x1 - x0); *y = y0 + frand() * (y1 - y0);
}
/* mod:randomConfuse(npc, pos): a random room position every call while confused */
static void en_random_confuse(unsigned long e, const float *pos, float *out) { if (en_confused(e)) en_random_room_pos(&out[0], &out[1]); else { out[0] = pos[0]; out[1] = pos[1]; } }
/* mod:confusePos(npc, pos): while confused a nearby wandering point, kept for 10 frames (d.confusedEffectPos = f[10], f[11], i[10] = set) */
static void en_confuse_pos(en_t *s, unsigned long e, const float *pos, float *out) {
    float *p = fpos(e);
    if (!en_confused(e)) { s->i[10] = 0; out[0] = pos[0]; out[1] = pos[1]; return; }
    if (s->age % 10 == 0) s->i[10] = 0;
    if (s->i[10] && vlen(p[0] - s->f[10], p[1] - s->f[11]) < 2.0f) { s->f[10] = p[0]; s->f[11] = p[1]; }
    if (!s->i[10]) { float x, y; en_rand_vec(&x, &y, (float) rint_(5, 15)); s->f[10] = p[0] + x; s->f[11] = p[1] + y; s->i[10] = 1; }
    out[0] = s->f[10]; out[1] = s->f[11];
}
/* mod:diagonalMove(npc, speed, true, xmult): the diagonal velocity it asks for (scared: away from the player within 100; confused: a wandering vector) */
static void en_diag_vec(en_t *s, unsigned long e, float speed, float xmult, float *ox, float *oy) {
    float *v = fvel(e), *p = fpos(e), *t = en_target(e);
    float xv = speed * xmult, yv = speed;
    if (v[0] < 0) xv = -xv;
    if (v[1] < 0) yv = -yv;
    if (en_scare(e) && t) {
        float d = vlen(t[0] - p[0], t[1] - p[1]);
        if (d < 100.0f) { float x = p[0] - t[0], y = p[1] - t[1]; vresize(&x, &y, fmaxf(5.0f, 10.0f - d / 20.0f)); xv = x; yv = y; }
    }
    if (en_confused(e)) {                                                       /* confusePos(npc, vec, nil, true): a random vector 2..5 long, new every 10 frames */
        if (s->age % 10 == 0) s->i[11] = 0;
        if (!s->i[11]) { float x, y; en_rand_vec(&x, &y, (float) rint_(2, 5)); s->f[8] = x; s->f[9] = y; s->i[11] = 1; }
        xv = s->f[8]; yv = s->f[9];
    }
    *ox = xv; *oy = yv;
}
/* Entity grid at a world position: the GridEntity* of the room's table (room + 0x30, W = room + 0x14, measured in eid.c) or 0 */
static unsigned long en_grid_ent(float x, float y) {
    unsigned long room = g_room ? g_room() : 0; if (!room) return 0;
    int w = *(const int *) (room + 0x14), h = *(const int *) (room + 0x18);
    if (w < 1 || w > 64 || h < 1 || h > 64) return 0;
    int c = (int) floorf((x - 20.0f) / 40.0f), r = (int) floorf((y - 100.0f) / 40.0f), idx = r * w + c;
    if (c < 0 || r < 0 || c >= w || r >= h || idx < 0 || idx >= 448) return 0;
    return *(const unsigned long *) (room + 0x30 + (unsigned long) idx * 8);
}
static int en_grid_type_at(float x, float y) { unsigned long g = en_grid_ent(x, y); return g ? *(const int *) (g + 0x08) : -1; }
/* the launched-enemy system (ferrium/misc/launchedStuff.lua), one step per AI call: la_hasvel bits 1 = fixed velocity, 2 = collision threshold, 4 = SpriteOffset instead of PositionOffset */
static void en_launch(en_t *s, float zvel, float accel, int vel, float vx, float vy, int col, float colv, int spriteoff) {
    s->la_on = 1; s->la_z = zvel; s->la_acc = accel; s->la_vx = vx; s->la_vy = vy; s->la_col = colv;
    s->la_hasvel = (vel ? 1 : 0) | (col ? 2 : 0) | (spriteoff ? 4 : 0);
    s->la_h = spriteoff ? s->so[1] : s->po[1];
}
static int en_launch_tick(en_t *s, unsigned long e, void (*land)(en_t *, unsigned long)) {
    if (!s->la_on) return 0;
    float *v = fvel(e); int sp = (s->la_hasvel & 4) != 0, landed = 0;
    if (s->la_hasvel & 1) { v[0] = s->la_vx; v[1] = s->la_vy; }
    float off = s->la_h + s->la_z + s->la_acc * 0.5f;
    if (sp) en_sprite_offset(s, e, s->so[0], off); else en_position_offset(s, e, s->po[0], off);
    s->la_h = off; s->la_z += s->la_acc;
    if (off > 0) {
        s->la_on = 0; landed = 1;
        if (land) land(s, e);
        if (sp) en_sprite_offset(s, e, s->so[0], 0); else en_position_offset(s, e, s->po[0], 0);
        off = 0;
    }
    if (s->la_hasvel & 2) {
        if (off < s->la_col) { en_set_gridcoll(e, GRIDCOLL_WALLS); en_set_collision(e, ENTCOLL_NONE); }
        else { en_set_gridcoll(e, GRIDCOLL_GROUND); en_set_collision(e, ENTCOLL_ALL); }
    }
    return landed;
}
/* mod:chooserandomlocationforskuzz(npc, rangehop, rangenpc, targetplayer) (main.lua 14710): a random free tile within rangehop that is within rangenpc of the player
 * (beyond it when scared); with the player out of hop range or no targeting, any free tile in range. */
static void en_skuzz_loc(unsigned long e, float range_hop, float range_npc, int targetplayer, float *out) {
    float *p = fpos(e), *pt = en_target(e), tp[2] = { -1000.0f, -1000.0f };
    int fear = en_flag(e, EF_FLAG_FEAR);
    if (targetplayer) { if (pt) { tp[0] = pt[0]; tp[1] = pt[1]; } }
    else if (fear && pt) { tp[0] = pt[0]; tp[1] = pt[1]; targetplayer = 1; }
    int inrange = vlen(tp[0] - p[0], tp[1] - p[1]) < range_hop;
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9, n = 0, pick;
    static float cx[448], cy[448];
    for (int r = 0; r < h && n < 448; r++) for (int c = 0; c < w && n < 448; c++) {
        float x = en_room_x(c), y = en_room_y(r);
        if (vlen(x - p[0], y - p[1]) >= range_hop || en_xy(x, y, 0) || !en_in_room_walk(x, y, 15.0f)) continue;
        if (targetplayer && inrange) {
            float d = vlen(x - tp[0], y - tp[1]);
            if (!fear ? d < range_npc : d > range_npc) { cx[n] = x; cy[n] = y; n++; }
        } else { cx[n] = x; cy[n] = y; n++; }
    }
    if (!n) { out[0] = p[0]; out[1] = p[1]; return; }
    pick = (int) rnd((unsigned) n); out[0] = cx[pick]; out[1] = cy[pick];
}
/* mod:CatheryPathFinding(ent, target, {Speed, Accel, GiveUp}) (erfly/enemies/_proapi.lua 65): straight at the target when the line is clear, else along the path; the velocity lerps to it */
static void en_cathery(en_t *s, unsigned long e, const float *t, float speed, float accel) {
    float *p = fpos(e), *v = fvel(e), tp[2] = { t[0], t[1] }, wx, wy;
    if (s->age < 1) return;
    if (!en_line_clear(p[0], p[1], t[0], t[1], 0)) {
        if (path_step(p[0], p[1], t[0], t[1], 0, &wx, &wy)) { tp[0] = wx; tp[1] = wy; }
        else { v[0] = lerpf(v[0], 0, accel); v[1] = lerpf(v[1], 0, accel); return; }       /* GiveUp: no way there, it stops */
    }
    float dx = tp[0] - p[0], dy = tp[1] - p[1], d = vlen(dx, dy);
    if (d < speed) { v[0] = lerpf(v[0], 0, accel); v[1] = lerpf(v[1], 0, accel); }
    else { v[0] = lerpf(v[0], dx * (speed / d), accel); v[1] = lerpf(v[1], dy * (speed / d), accel); }
}
/* mod:FindRandomFreePos(npc, radius, findnear, avoidplayer, avoidfire, maxrad) (erfly/enemies/_shared.lua 1776): a random free tile centre farther than radius (nearer when findnear),
 * within maxrad when given (maxrad 0 = none); any free tile when none qualifies; the target is only used for avoidplayer, not done here (every FF caller in this file passes nil) */
static void en_find_random_free(unsigned long e, const float *target, float radius, int findnear, float maxrad, float *out) {
    float *p = fpos(e); static float fx[448], fy[448], nx[448], ny[448], ax[448], ay[448]; int nf = 0, nn = 0, na = 0;
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9; (void) target;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
        float x = en_room_x(c), y = en_room_y(r), d = vlen(x - p[0], y - p[1]);
        if (en_xy(x, y, 0) || !en_in_room_walk(x, y, 0)) continue;
        ax[na] = x; ay[na] = y; na++;
        if (d > radius && (maxrad <= 0 || d < maxrad)) { fx[nf] = x; fy[nf] = y; nf++; }
        if (d < radius && (maxrad <= 0 || d > maxrad)) { nx[nn] = x; ny[nn] = y; nn++; }
    }
    if (nn && findnear) { int k = (int) rnd((unsigned) nn); out[0] = nx[k]; out[1] = ny[k]; }
    else if (nf && !findnear) { int k = (int) rnd((unsigned) nf); out[0] = fx[k]; out[1] = fy[k]; }
    else if (na) { int k = (int) rnd((unsigned) na); out[0] = ax[k]; out[1] = ay[k]; }
    else en_random_room_pos(&out[0], &out[1]);
}
/* the Skatole item the Lua checks (FF's, id unknown to the port) is never held */
#define SKATOLE_HELD 0

/* ------------------------------------------------------------------------------------------ Skuzz 666.60 (mini/enemies/skuzz.lua)
 * state 0 idle / 1 hop; cd = d.jumpytimer; f[0] = d.stateframe (the hop's arc counter) */
static void ai_skuzz(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e); (void) p;
    if (!s->init) {
        s->init = 1; en_spr_off(e, 0, 0); s->cd = rint_(45, 90); s->state = 0; s->f[0] = 0;
        en_set_collision(e, ENTCOLL_PLAYEROBJECTS); en_set_gridcoll(e, GRIDCOLL_GROUND);
        if (en_player_has(377)) ENLOG(40, 2, "ffenemy: not done: Skuzz with Bursting Sack (friendly sheet, Sprite::Load; CollisionDamage 0)");
    }
    if (s->state == 0) {
        if (!en_playing(s, "land")) { en_play(s, "idle"); v[0] = lerpf(v[0], 0, 0.75f); v[1] = lerpf(v[1], 0, 0.75f); }
        else { v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f); }
        if (s->cd <= 0) {
            s->state = 1; en_sound(SOUND_FETUS_LAND, 0.6f, 2.7f); en_play(s, "hopstart"); s->f[0] = 0;
            float feard = en_flag(e, EF_FLAG_FEAR) ? 50.0f : 0.0f, loc[2];
            en_skuzz_loc(e, 150.0f + feard / 2.0f, 50.0f + feard, !(en_flag(e, EF_FLAG_FEAR) || en_confused(e)), loc);
            v[0] = (loc[0] - p[0]) / 15.0f * 0.90f; v[1] = (loc[1] - p[1]) / 15.0f * 0.90f;
            en_set_collision(e, ENTCOLL_NONE); en_set_gridcoll(e, GRIDCOLL_NONE);
        } else s->cd--;
    } else {
        if (!en_playing(s, "hopstart")) en_play(s, "hop");
        float oy = -2.0f * (-0.025f * (s->f[0] - 30.0f) * (s->f[0] - 30.0f) + 29.5f);
        en_spr_off(e, 0, oy);
        if (oy > 0) {
            en_spr_off(e, 0, 0); s->state = 0; en_replay(s, "land"); en_sound(SOUND_FETUS_LAND, 0.6f, 1.7f); s->cd = rint_(75, 120);
            en_set_collision(e, ENTCOLL_PLAYEROBJECTS); en_set_gridcoll(e, GRIDCOLL_GROUND);
        } else s->f[0] += 4.0f;
    }
}

/* ------------------------------------------------------------------------------------------ Army (Conga) Skuzz 114.44 (ferrium/enemies/congaskuzz.lua)
 * i[0] = npc.I1 (the place in the line), ref = Parent, ref2 = Child, i[1] = leadingMember, i[2] = hopCount, f[0],f[1] = originalPos, f[2],f[3] = targetPos, cd = StateFrame
 * state 0 Idle / 1 Hop / 2 Start / 3 Land */
static void skuzz_land(en_t *s, unsigned long e) { en_sound(SOUND_FETUS_LAND, 0.6f, 1.7f); s->state = 3; float *v = fvel(e); v[0] = v[1] = 0; en_set_collision(e, ENTCOLL_PLAYEROBJECTS); en_set_gridcoll(e, GRIDCOLL_GROUND); }
static void conga_shift(float *px, float *py, float x, float y, int hops, float *ox, float *oy) {   /* congaSkuzzShift */
    float vx = x - px[0], vy = y - py[0], rx, ry; vresize(&vx, &vy, 40.0f); vrot(vx, vy, hops % 2 == 0 ? -90.0f : 90.0f, &rx, &ry);
    float nx = x + rx, ny = y + ry; en_free_tile(&nx, &ny, 20.0f); *ox = nx; *oy = ny;
}
static void ai_congaskuzz(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) {
        s->init = 1; s->state = 0; en_set_collision(e, ENTCOLL_PLAYEROBJECTS);
        if (en_player_has(379)) ENLOG(41, 2, "ffenemy: not done: Army Skuzz with Bursting Sack (friendly sheet, Sprite::Load)");
        s->i[3] = s->sub > 0; s->i[2] = 0;                                    /* i[3] = trueLeader */
    } else s->cd++;
    unsigned long par = en_parent_alive(s->ref);
    if ((s->i[0] == 0 && s->age > 1) || s->i[3]) {
        if (!s->ref2 && !s->i[1]) {                                           /* build the line from this one: the nearest free member within 60 of the last */
            s->i[1] = 1;
            unsigned long snap[SNAP]; unsigned n = snapshot(snap); unsigned long curr = e;
            for (int guard = 0; curr && guard < 32; guard++) {
                unsigned long closest = 0; float dist = 60.0f;
                for (unsigned k = 0; k < n; k++) {
                    unsigned long c = snap[k]; if (c == curr || !alive(c) || *(const int *) (c + E_TYPE) != s->type || *(const int *) (c + E_VARIANT) != s->variant) continue;
                    en_t *cs = en_adopt(c); float *cp = fpos(c), *cc = fpos(curr);
                    if (cs && vlen(cp[0] - cc[0], cp[1] - cc[1]) < dist && !cs->i[1] && cs->i[0] == 0 && *(const int *) (c + E_SUBTYPE) == 0) { closest = c; dist = vlen(cp[0] - cc[0], cp[1] - cc[1]); }
                }
                if (!closest) break;
                en_t *cs = en_adopt(closest), *cu = en_adopt(curr);
                if (cu) cu->ref2 = closest;
                cs->ref = curr; cs->i[0] = (cu ? cu->i[0] : 0) + 1; curr = closest;
            }
        }
    } else if (s->age > 1) {
        if (par) { if (en_dead(par)) { s->i[1] = 1; s->i[0] = 0; } }
        else { s->i[1] = 1; s->i[0] = 0; }
    }
    if (s->state == 0) {
        v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f);
        if (s->cd > 80 && s->i[0] == 0) s->state = 2;
        else if (par) { en_t *ps = en_slot(par); if (ps && ps->state == 1) s->state = 2; }
        en_play(s, "idle");
    } else if (s->state == 1) en_play(s, "hop");
    else if (s->state == 2) {
        v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f);
        if (en_finished(s, "hopstart")) {
            float tp[2];
            en_sound(SOUND_FETUS_LAND, 0.6f, 2.7f); s->state = 1;
            if (s->i[0] > 0 && par) { en_t *ps = en_slot(par); float *pp = fpos(par); tp[0] = ps && ps->i[4] ? ps->f[0] : pp[0]; tp[1] = ps && ps->i[4] ? ps->f[1] : pp[1]; }
            else if (en_scare(e)) { tp[0] = t[0]; tp[1] = t[1]; /* FindClosestFreePosition(mode 1) below */
                float best = -1, bx = p[0], by = p[1]; int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
                for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) { float x = en_room_x(c), y = en_room_y(r); if (en_xy(x, y, 0) || vlen(x - p[0], y - p[1]) >= 120.0f) continue; float d = vlen(x - t[0], y - t[1]); if (d > best) { best = d; bx = x; by = y; } }
                tp[0] = bx; tp[1] = by; }
            else if (en_confused(e)) en_skuzz_loc(e, 150.0f, 50.0f, !(en_flag(e, EF_FLAG_FEAR) || en_confused(e)), tp);
            else {                                                              /* FindClosestFreePosition(mode 0): the free tile within 120 nearest the player */
                float best = 1e9f, bx = p[0], by = p[1]; int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
                for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) { float x = en_room_x(c), y = en_room_y(r); if (en_xy(x, y, 0) || vlen(x - p[0], y - p[1]) >= 120.0f) continue; float d = vlen(x - t[0], y - t[1]); if (d < best) { best = d; bx = x; by = y; } }
                tp[0] = bx; tp[1] = by;
                if (vlen(p[0] - t[0], p[1] - t[1]) > 100.0f) { float ox, oy; conga_shift(&p[0], &p[1], tp[0], tp[1], s->i[2], &ox, &oy); tp[0] = ox; tp[1] = oy; }
            }
            float vx = (tp[0] - p[0]) / 15.0f * 0.90f, vy = (tp[1] - p[1]) / 15.0f * 0.90f;
            v[0] = vx; v[1] = vy; s->i[2]++; s->f[0] = p[0]; s->f[1] = p[1]; s->i[4] = 1;
            en_set_gridcoll(e, GRIDCOLL_WALLS);
            en_launch(s, -11.0f, 0.65f, 1, vx, vy, 1, 0.0f, 0);
        } else en_play(s, "hopstart");
    } else if (s->state == 3) {
        v[0] = v[1] = 0;
        if (en_finished(s, "land")) { s->state = 0; if (s->i[2] < 3) s->cd = 70; else { s->i[2] = 0; s->cd = 0; } }
        else en_play(s, "land");
    }
    en_launch_tick(s, e, skuzz_land);
}

/* ------------------------------------------------------------------------------------------ Ring Skuzz 114.45 (ferrium/enemies/ringskuzz.lua)
 * subtype = the ring; i[0] = position in the ring, f[0] = rotatedAngle, f[1] = newRotatedAngle, f[2] = ringRadius, cd = StateFrame; state as the Army */
static void ai_ringskuzz(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    if (!s->init) {
        s->init = 1; s->state = 0; en_set_collision(e, ENTCOLL_PLAYEROBJECTS);
        if (en_player_has(379)) ENLOG(42, 2, "ffenemy: not done: Ring Skuzz with Bursting Sack (friendly sheet, Sprite::Load)");
        int count = 0, pos = 0;
        for (unsigned k = 0; k < n; k++) { unsigned long c = snap[k]; if (alive(c) && *(const int *) (c + E_TYPE) == s->type && *(const int *) (c + E_VARIANT) == s->variant && *(const int *) (c + E_SUBTYPE) == s->sub) { count++; if (c == e || en_adopt(c)->init) pos++; } }
        s->f[0] = (float) pos * (360.0f / (float) (count ? count : 1)); s->f[2] = 70.0f;
    } else s->cd++;
    if (s->state == 0) {
        v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f);
        if (s->cd > 30) s->state = 2;
        en_play(s, "idle");
    } else if (s->state == 1) en_play(s, "hop");
    else if (s->state == 2) {
        v[0] = lerpf(v[0], 0, 0.4f); v[1] = lerpf(v[1], 0, 0.4f);
        if (en_finished(s, "hopstart")) {
            float tp[2], cen[2] = { p[0], p[1] }; int cnt = 0; float avg[2] = { 0, 0 };
            en_sound(SOUND_FETUS_LAND, 0.6f, 2.7f); s->state = 1;
            if (en_confused(e)) en_skuzz_loc(e, 150.0f, 50.0f, !(en_flag(e, EF_FLAG_FEAR) || en_confused(e)), tp);
            else {
                for (unsigned k = 0; k < n; k++) { unsigned long c = snap[k]; if (alive(c) && *(const int *) (c + E_TYPE) == s->type && *(const int *) (c + E_VARIANT) == s->variant && *(const int *) (c + E_SUBTYPE) == s->sub) {
                    float *cp = fpos(c); if (!cnt) { avg[0] = cp[0]; avg[1] = cp[1]; } else { avg[0] = cp[0] + (avg[0] - cp[0]) / 2.0f; avg[1] = cp[1] + (avg[1] - cp[1]) / 2.0f; } cnt++; } }
                if (cnt) { cen[0] = avg[0]; cen[1] = avg[1]; }
                /* recalculateRingSkuzzOrbit: the living ones evenly spread from the first's angle; the angle taken is this one's slot + 40 */
                int cntl = 0, myslot = 0; float first = 1e9f;
                for (unsigned k = 0; k < n; k++) { unsigned long c = snap[k]; if (alive(c) && !en_dead(c) && *(const int *) (c + E_TYPE) == s->type && *(const int *) (c + E_VARIANT) == s->variant && *(const int *) (c + E_SUBTYPE) == s->sub) { en_t *cs = en_adopt(c); if (cs->f[0] < first) first = cs->f[0]; cntl++; } }
                for (unsigned k = 0; k < n; k++) { unsigned long c = snap[k]; if (alive(c) && !en_dead(c) && *(const int *) (c + E_TYPE) == s->type && *(const int *) (c + E_VARIANT) == s->variant && *(const int *) (c + E_SUBTYPE) == s->sub) { en_t *cs = en_adopt(c); if (cs->f[0] < s->f[0] || (cs->f[0] == s->f[0] && c < e)) myslot++; } }
                s->f[1] = cntl ? first + (360.0f / (float) cntl) * (float) myslot : 0.0f;
                s->f[0] = s->f[1] + 40.0f;
                float dir = en_scare(e) ? -75.0f : 75.0f, ox, oy, tx = t[0] - cen[0], ty = t[1] - cen[1];
                vrot(0, s->f[2], s->f[0], &ox, &oy); vresize(&tx, &ty, dir);
                tp[0] = cen[0] + ox + tx; tp[1] = cen[1] + oy + ty;
            }
            s->f[2] = s->f[2] == 70.0f ? 40.0f : 70.0f;
            if (vlen(tp[0] - p[0], tp[1] - p[1]) > 120.0f) { float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 120.0f); tp[0] = x + p[0]; tp[1] = y + p[1]; }
            en_free_tile(&tp[0], &tp[1], 0.0f);
            float vx = (tp[0] - p[0]) / 15.0f * 0.90f, vy = (tp[1] - p[1]) / 15.0f * 0.90f;
            v[0] = vx; v[1] = vy; en_set_gridcoll(e, GRIDCOLL_WALLS);
            en_launch(s, -11.0f, 0.65f, 1, vx, vy, 1, 0.0f, 0);
        } else en_play(s, "hopstart");
    } else if (s->state == 3) {
        v[0] = v[1] = 0;
        if (en_finished(s, "land")) { s->state = 0; s->cd = 0; } else en_play(s, "land");
    }
    en_launch_tick(s, e, skuzz_land);
}

/* ------------------------------------------------------------------------------------------ Coby 160.1050 (erfly/enemies/coby.lua) */
static void ai_coby(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    if (!s->init) s->init = 1;
    if (vlen(v[0], v[1]) > 0.1f) en_walk_anim(s, e, "WalkHori", "WalkVert", 0.1f); else en_set_frame(s, "WalkVert", 0);
    en_confuse_pos(s, e, t, tp);
    if (en_line_clear(p[0], p[1], tp[0], tp[1], 0) || en_scare(e)) {
        float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 3.0f);
        if (en_scare(e)) { x = -x; y = -y; }
        v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
    } else en_walk_to(e, tp, 0.4f, 0.4f, 0);
}
static void die_coby(en_t *s, unsigned long e) {
    float *p = fpos(e); (void) s;
    ENLOG(43, 2, "ffenemy: not done: Coby's death webs (Isaac.GridSpawn(10) on its tile and the four around it: the engine's grid spawn is not measured); the Baby Spider is spawned");
    en_spawn(85, 962, p[0], p[1], 0, 0, e, 0);
}

/* ------------------------------------------------------------------------------------------ Nanny Long Legs 160.1040 (erfly/enemies/nannylonglegs.lua)
 * state 0 idle / 1 shoot / 2 pissed / 3 cautious; cd = StateFrame; f[0] = playerStepTimer. "Stepping" = the player stands on a spider web grid entity (type 10). */
static void ai_nanny(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; s->state = 0; } else s->cd++;
    int stepping = 0; s->f[0] += 1.0f;
    if (en_grid_type_at(t[0], t[1]) == 10) { stepping = 1; s->f[0] = 0; }
    if (s->state == 0) {
        v[0] *= 0.75f; v[1] *= 0.75f;
        if (!(en_playing(s, "Blink") || en_playing(s, "Angry Stop"))) { en_play(s, "Idle"); if (rint_(1, 30) == 1) en_play(s, "Blink"); }
        if (stepping) { s->state = 2; s->cd = 0; en_play(s, "Angry Start"); }
    } else if (s->state == 1) {
        v[0] *= 0.75f; v[1] *= 0.75f;
        if (en_finished(s, "Shoot")) { s->cd = 0; s->state = 0; }
        else if (en_event(s, "Shoot", "Shoot")) { float x = t[0] - p[0], y = t[1] - p[1]; en_sound(SOUND_LITTLE_SPIT, 1.0f, 0.9f); vresize(&x, &y, 8.0f); en_shoot(e, 0, p[0], p[1], x, y); }
        else en_play(s, "Shoot");
    } else if (s->state == 2) {
        if (!en_playing(s, "Angry Start")) {
            en_cathery(s, e, t, 10.0f, 0.1f);
            if (vlen(v[0], v[1]) > 0.1f) en_play(s, "Walk"); else en_play(s, "Idle Angry");
            en_sound(FFSND_CobWeb, 1.0f + 2.0f * (90.0f - fminf((float) s->cd, 90.0f)) / 90.0f, 0.8f + frand() * 0.4f);
            if (!stepping && s->f[0] > 7.0f) { s->state = 3; s->cd = 0; }
        }
    } else if (s->state == 3) {
        v[0] *= 0.75f; v[1] *= 0.75f;
        if (s->cd > 5) { s->state = 0; en_play(s, "Angry Stop"); } else en_play(s, "Idle Angry");
        if (stepping) { s->state = 2; s->cd = 0; }
    }
    ENLOG(44, 1, "ffenemy: not done: Nanny Long Legs' web sway animation (grid sprite scale) and the web-under-Nanny trigger; the player-on-a-web trigger reads the room's grid table");
}
static void hurt_nanny(en_t *s, unsigned long e, float dmg) { (void) e; if (dmg > 0 && s->state == 0 && s->cd > 10) s->state = 1; }

/* ------------------------------------------------------------------------------------------ Beeter 160.180 (erfly/enemies/beeter.lua)
 * state 0 idle / 1 attack; i[0] = astate (1..2), i[1] = count, f[0] = speed, f[1] = attackang, cd = StateFrame */
static void ai_beeter(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2], tv[2]; if (!t) return;
    char nm[16];
    if (!s->init) { s->init = 1; s->i[0] = 1; s->state = 0; s->f[0] = 3.0f; s->f[1] = frand() * 360.0f; } else s->cd++;
    en_flip(e, t[0] < p[0]);
    en_random_confuse(e, t, tp);
    tv[0] = tp[0] - p[0]; tv[1] = tp[1] - p[1]; vresize(&tv[0], &tv[1], s->f[0]);
    v[0] = lerpf(v[0], tv[0], 0.1f); v[1] = lerpf(v[1], tv[1], 0.1f);
    if (s->state == 0) {
        s->f[0] = en_scare(e) ? -3.0f : 3.0f;
        snprintf(nm, sizeof nm, "Fly%d", s->i[0]); en_play(s, nm);
        if (rint_(1, 10) == 1 && s->cd > 10 && vlen(t[0] - p[0], t[1] - p[1]) < 250.0f && en_line(t, p, 0) && !(en_scare_conf(e) || SKATOLE_HELD)) { s->state = 1; s->i[1] = 0; }
    } else {
        s->f[0] = en_scare(e) ? -1.0f : 1.0f;
        snprintf(nm, sizeof nm, "Shoot%d", s->i[0]);
        char fin[16]; snprintf(fin, sizeof fin, "Shoot%d", s->i[0]);
        if (en_finished(s, fin)) {
            s->i[0]++; s->i[1]++;
            if (s->i[0] > 2) s->i[0] = 1;
            if (s->i[1] > 5) { s->state = 0; s->cd = 0; }
        } else if (en_event(s, nm, "Shoot")) {
            en_pp_t pp; memset(&pp, 0, sizeof pp);
            en_sound(SOUND_BLOODSHOOT, 1.0f, 1.0f);
            pp.has_faccel_mod = 1; pp.faccel_mod = 0.4f; pp.has_scale = 1; pp.scale = 0.7f;
            for (int i = 1; i <= 3; i++) { float x, y; vrot(7.0f, 0, 120.0f * (float) i + s->f[1], &x, &y); en_shoot_p(e, p[0], p[1], x, y, &pp); }
            s->f[1] += 30.0f;
        } else en_play(s, nm);
    }
}

/* ------------------------------------------------------------------------------------------ Stinger 150.6 (erfly/enemies/colonel.lua 631 and 659): the bouncing shot of Stingler/Zingling, sub 1 the homing one of Homer
 * f[0] = d.speed, i[0] = d.count, i[1] = d.maxcount */
static unsigned long en_shoot_stinger(unsigned long e, float x, float y, float vx, float vy, int maxcount) {
    unsigned long c = en_spawn(150, 6, x, y, vx, vy, e, 0);
    if (!c) return 0;
    en_t *cs = en_adopt(c); cs->i[1] = maxcount;
    en_flag_set(c, EF_FLAG_NO_PHYSICS_KNOCKBACK); en_set_collision(c, ENTCOLL_PLAYERONLY); en_set_gridcoll(c, GRIDCOLL_NOPITS);
    return c;
}
static void ai_stinger_homing(en_t *s, unsigned long e);
static void ai_stinger(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e); (void) p;
    if (s->sub == 1) { ai_stinger_homing(s, e); return; }
    if (!s->init) { s->init = 1; s->f[0] = vlen(v[0], v[1]); if (!s->i[1]) s->i[1] = 10; s->i[0] = 0; en_sprite_offset(s, e, 0, -15.0f); en_flag_set(e, EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET); }
    en_play(s, "Idle");
    if (en_collides_grid(e)) {
        s->i[0]++;
        if (s->i[0] > s->i[1]) { en_spawn(1000, 7014, p[0], p[1], 0, 0, 0, 0); en_remove(e); ENLOG(45, 1, "ffenemy: not done: the Stinger's SpriteRotation (the rotated sprite; effect 1000.7014 gets none)"); return; }
        en_sound(FFSND_Ricochet, 1.0f, 1.0f);
    }
    vresize(&v[0], &v[1], s->f[0]);
}
/* Homing stinger 150.6.1 (colonel.lua homingStingerProjectileAI): chases the player, dies with its parent (ref) */
static void ai_stinger_homing(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; en_flag_set(e, EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET); s->f[1] = (float) rint_(800, 1000) / 10000.0f; }
    en_sprite_offset(s, e, 0, -15.0f);
    s->f[0] = 8.5f + sinf((float) s->age / 10.0f) * 6.0f;
    en_play(s, "Idle");
    if (!en_parent_alive(s->ref)) { en_spawn(1000, 7014, p[0], p[1], 0, 0, 0, 1); en_remove(e); return; }
    float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, s->f[0] * 1.0f);
    v[0] = lerpf(v[0], x, s->f[1]); v[1] = lerpf(v[1], y, s->f[1]);
    if (vlen(v[0], v[1]) > 10.0f) vresize(&v[0], &v[1], 10.0f);
    ENLOG(46, 1, "ffenemy: not done: homing Stinger's looping sound, slowestPlayerSpeed scaling (taken as 1) and SpriteRotation");
}

/* ------------------------------------------------------------------------------------------ Stingler 160.681 (erfly/enemies/stingler.lua): f[0] = d.hurt, cd = StateFrame */
static void ai_stingler(en_t *s, unsigned long e) {
    float *v = fvel(e), *p = fpos(e), tv[2];
    s->cd++;
    if (s->i[0]) {                                                             /* d.hurt */
        if (en_scare_conf(e)) s->i[0] = 0;
        if (en_finished(s, "Stinger")) { s->i[0] = 0; s->cd = -30; }
        else if (en_event(s, "Stinger", "Shoot")) {
            float x, y; en_rand_vec(&x, &y, 1.0f);
            en_sound(FFSND_FrogShoot, 0.7f, (float) rint_(12, 14) / 10.0f);
            unsigned long sp = *(const unsigned long *) (e + 0x400);
            int small = sp && *(const int *) (sp + E_TYPE) == 71 && *(const int *) (sp + E_VARIANT) == 0;
            en_shoot_stinger(e, p[0] + x * 15.0f, p[1] + y * 15.0f, x * 9.0f, y * 9.0f, small ? 2 : 3);
        } else en_play(s, "Stinger");
    } else en_play(s, "Fly");
    en_diag_vec(s, e, 3.0f, 1.0f, &tv[0], &tv[1]);
    if (SKATOLE_HELD) { tv[0] /= 2; tv[1] /= 2; }
    v[0] = lerpf(v[0], tv[0], 0.1f); v[1] = lerpf(v[1], tv[1], 0.1f);
}
static void hurt_stingler(en_t *s, unsigned long e, float dmg) { (void) e; if (dmg > 0 && s->cd > 0) s->i[0] = 1; }

/* ------------------------------------------------------------------------------------------ Homer 160.590 (erfly/enemies/homer.lua)
 * state 0 idle / 1 attack / 2 assless; cd = StateFrame; i[0] = keepflippin; i[1] = running; f[0],f[1] = gridtarget */
static void ai_homer(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e), tp[2]; if (!t) return;
    if (!s->init) { s->init = 1; s->state = 0; } else s->cd++;
    en_sprite_offset(s, e, 0, -10.0f);
    if (s->state == 0) {
        en_play(s, "Idle01");
        en_flip(e, v[0] > 0);
        en_confuse_pos(s, e, t, tp);
        float x = tp[0] - p[0], y = tp[1] - p[1]; vresize(&x, &y, 2.0f);
        if (en_scare(e)) { x = -x; y = -y; }
        v[0] = lerpf(v[0], x, 0.05f); v[1] = lerpf(v[1], y, 0.05f);
        if (!(en_scare_conf(e) || SKATOLE_HELD) && ((rint_(1, 10) == 1 && s->cd > 10) || s->cd > 60) && en_line(t, p, 0)) { s->state = 1; s->i[0] = 1; }
    } else if (s->state == 1) {
        if (s->i[0]) en_flip(e, t[0] > p[0]);
        if (en_finished(s, "Shoot")) s->state = 2;
        else if (en_event(s, "Shoot", "Shoot")) {
            s->i[0] = 0; en_sound(FFSND_FrogShoot, 1.0f, (float) rint_(9, 11) / 10.0f);
            float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 7.0f);
            float nx = x, ny = y; vresize(&nx, &ny, 20.0f);
            unsigned long c = en_spawn(150, 6, p[0] + nx, p[1] + ny, x, y, e, 1);
            if (c) { en_flag_set(c, EF_FLAG_NO_PHYSICS_KNOCKBACK); en_set_collision(c, ENTCOLL_PLAYERONLY); en_set_gridcoll(c, GRIDCOLL_NONE); en_t *cs = en_adopt(c); cs->ref = e; }
        } else en_play(s, "Shoot");
    } else {                                                                   /* asslessChap */
        float tv[2];
        en_play(s, "Idle02");
        en_flip(e, v[0] > 0);
        if (!s->i[3]) { en_find_random_free(e, t, 120.0f, 0, 0.0f, &s->f[0]); s->i[3] = 1; }
        tv[0] = s->f[0] - p[0]; tv[1] = s->f[1] - p[1]; vresize(&tv[0], &tv[1], 2.0f);
        if (g_frame_fn ? (g_frame_fn() % 3 == 1) : (s->age % 3 == 1)) { unsigned long b = en_spawn(1000, 7, p[0], p[1], 0, 0, e, 0); if (b) en_scale(b, 0.4f, 0.4f); }
        if (s->age % 30 == 1) {
            hit_enemy(e, 0.5f);
            for (int k = 0; k < 2; k++) { float x, y; en_rand_vec(&x, &y, frand() * 6.0f); en_spawn(1000, 5, p[0], p[1], x, y, e, 0); }
        }
        if (vlen(p[0] - t[0], p[1] - t[1]) < 120.0f || en_scare(e)) { tv[0] = t[0] - p[0]; tv[1] = t[1] - p[1]; vresize(&tv[0], &tv[1], -8.0f); s->i[1] = 1; }
        else if (vlen(p[0] - s->f[0], p[1] - s->f[1]) < 20.0f || (s->cd % 120 == 0 && !en_xy(p[0], p[1], 0)) || s->i[1] || (en_confused(e) && s->cd % 10 == 0)) {
            en_find_random_free(e, t, 120.0f, 0, 0.0f, &s->f[0]);
            tv[0] = s->f[0] - p[0]; tv[1] = s->f[1] - p[1]; vresize(&tv[0], &tv[1], 2.0f); s->i[1] = 0;
        }
        v[0] = lerpf(v[0], tv[0], 0.05f); v[1] = lerpf(v[1], tv[1], 0.05f);
    }
}

