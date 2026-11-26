/* ffen_g16.h - the Glass Chest's view of what is inside it: the Guppy's Eye effect (erfly/pickups/glasschest.lua, MC_POST_PICKUP_RENDER, mod.GuppyEyeOffsets).
 * Each thing the chest holds is drawn as a small half-transparent icon over the chest, laid out in the rows of GuppyEyeOffsets, and fades with the distance
 * of the nearest player (0.8 within 150 px down to 0.45). Drawn from the HUD pass (trace.c's mark 'X', through ffchest_draw - ffdraw_patch.py adds the call),
 * with the engine's own sprites, the road eid.c takes for its icons:
 *   0x89c16c Sprite::Sprite   0x89fe24 Sprite::Load   0x8a0228 ReplaceSpritesheet + 0x8a0284 LoadGraphics   0x89eab8 SetFrame   0x89ee7c the overlay's
 *   0x89e4c0 Sprite::Render(this, pos, zero, zero)   +0xe8 colour (4 floats)   +0xdc scale (2 floats)   +0x149 "loaded" flag   0x158 bytes a Sprite
 *   0x7f1fa0 (vector *, round): the world position -> the canvas position the engine draws at: (canvas - 338) / 2 + (p - (60, 140)) * 0.65 - read from its code.
 * What is NOT the Lua's: the item and trinket the chest will give are chosen by the engine when it opens (the room's own pedestal, ffen_g13.h), so before that
 * they are drawn as the Lua draws an item under the Curse of the Blind - the question mark (an item) - or as the mod's own mystery pickup (a trinket, a card
 * without an identity). Everything else is exactly what the chest rolled. A sprite whose file is missing is reported once (ASSET_MISSING) and skipped. */

typedef void (*pv_new_t)(void *);
typedef int  (*pv_load_t)(void *spr, const char *anm2);              /* 1 = the engine found the file */
typedef void (*pv_render_t)(void *spr, const float *screen);
typedef void (*pv_screen_t)(const float *world, float *screen);
static pv_new_t g_pv_new; static pv_load_t g_pv_load; static pv_render_t g_pv_render; static pv_screen_t g_pv_screen;

#define PV_N 64
#define PV_SPRITE_BYTES 0x158
#define PV_COLOUR 0xe8
#define PV_SCALE 0xdc
typedef struct { int t, v, s; const char *anm2, *anim; int frame; const char *ov; int ovf; const char *sheet; int layer; float scale, ox, oy; } pv_spec_t;
#define MYST "gfx/items/pick ups/mystery_pickup.anm2"
static const pv_spec_t PV_SPEC[] = {
    /* coins (glasschest.lua RandomChestOutcomes.Coins) */
    { 5, 20, 1,   "gfx/005.021_penny.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },        { 5, 20, 2, "gfx/005.022_nickel.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 20, 3,   "gfx/005.023_dime.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },         { 5, 20, 4, "gfx/005.024_double penny.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 20, 5,   "gfx/005.026_lucky penny.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 }, { 5, 20, 6, "gfx/005.025_sticky nickel.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 20, 7,   "gfx/005.027_golden penny.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 20, 213, "gfx/items/pick ups/cursed_penny.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 }, { 5, 20, 214, "gfx/items/pick ups/haunted_penny.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 20, 216, "gfx/items/pick ups/goldfiend_penny.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    /* bombs */
    { 5, 40, 1,   "gfx/005.041_bomb.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },        { 5, 40, 2, "gfx/005.042_double bomb.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 40, 4,   "gfx/005.043_golden bomb.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 }, { 5, 40, 923, "gfx/items/pick ups/bombs/copper/_pickup.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 40, 924, "gfx/items/pick ups/bomb_copper_double.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 4, 3, 0,    "gfx/004.003_troll bomb.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },   { 4, 4, 0, "gfx/004.004_megatroll bomb.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 4, 3, 160,  "gfx/items/pick ups/bomb_copper_troll.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 42, 0,   "gfx/005.042_poop nugget.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },  { 5, 42, 1, "gfx/005.042_big poop nugget.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    /* keys (offset (0, 2)); the charged key shows its frame 14 */
    { 5, 30, 1,   "gfx/005.031_key.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 2 },         { 5, 30, 2, "gfx/005.032_golden key.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 2 },
    { 5, 30, 3,   "gfx/005.034_chargedkey.anm2", "Idle", 14, 0, 0, 0, 0, 0.5f, 0, 2 },
    /* hearts */
    { 5, 10, 1,   "gfx/005.011_heart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },       { 5, 10, 2, "gfx/005.012_heart (half).anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 10, 3,   "gfx/005.013_heart (soul).anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 }, { 5, 10, 4, "gfx/005.014_heart (eternal).anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 10, 5,   "gfx/005.015_double heart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 }, { 5, 10, 6, "gfx/005.016_black heart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 10, 7,   "gfx/005.017_goldheart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },   { 5, 10, 8, "gfx/005.018_heart (halfsoul).anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 10, 9,   "gfx/005.020_scared heart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 }, { 5, 10, 10, "gfx/005.019_blended heart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 10, 11,  "gfx/005.01a_bone heart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },  { 5, 10, 12, "gfx/005.01b_rotten heart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 1024, 0, "gfx/items/pick ups/fiendish_heart.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 5, 69, 1,   "gfx/005.069_grabbag.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    { 3, 43, 0,   "gfx/003.043_attack fly.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 5 },
    /* an item (the question mark of the Curse of the Blind), a trinket / a card the engine has not chosen yet (the mod's mystery pickup) */
    { 5, 100, 0,  "gfx/005.100_collectible.anm2", "Idle", 0, 0, 0, "gfx/items/collectibles/questionmark.png", 1, 0.5f, 0, 12 },
    { 5, 350, 0,  MYST, "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },                           { 5, 300, 0, MYST, "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    /* a rune shard, the runes and the black rune, the souls (cards ids: 32..41 runes, 55 shard, 81..97 souls) */
    { 5, 300, 55, "gfx/005.313_rune shard.anm2", "Idle", 0, 0, 0, 0, 0, 0.5f, 0, 0 },
    /* monsters (scale 0.3, offset (0, 5)): a Monstro, a Frowning Gaper with its head frame 16, an attack fly */
    { 20, 0, 0,   "gfx/020.000_monstro.anm2", "Walk", 0, 0, 0, 0, 0, 0.3f, 0, 5 },
    { 10, 1, 0,   "gfx/010.000_frowning gaper.anm2", "WalkVert", 0, "Head", 16, 0, 0, 0.3f, 0, 5 },
    { 18, 0, 0,   "gfx/018.000_attack fly.anm2", "Fly", 0, 0, 0, 0, 0, 0.5f, 0, 5 },
};
#define PV_NSPEC ((int) (sizeof PV_SPEC / sizeof PV_SPEC[0]))
static const char *const PV_SOULS[17] = { "isaac", "magdalene", "cain", "judas", "blue baby", "eve", "samson", "azazel", "lazarus", "eden", "the lost", "lilith", "the keeper", "apollyon", "the forgotten", "bethany", "jacob" };

/* the sprite a drop (type, variant, subtype) is drawn with: -1 = none known; cards by identity are built here (their names are not a table row each) */
static int pv_find(int t, int v, int s, pv_spec_t *out, char *buf, size_t n) {
    if (t == 5 && v == 300 && s >= 32 && s <= 41) {                                   /* runes: the first four share a sheet, the rest and the blank rune another, the black rune its own */
        out->t = t; out->v = v; out->s = s; out->anim = "Idle"; out->frame = 0; out->ov = 0; out->ovf = 0; out->sheet = 0; out->layer = 0; out->scale = 0.5f; out->ox = 0; out->oy = 0;
        out->anm2 = s == 41 ? "gfx/005.307_blackrune.anm2" : s <= 35 ? "gfx/005.303_rune1.anm2" : "gfx/005.304_rune2.anm2"; return 0;
    }
    if (t == 5 && v == 300 && s >= 81 && s <= 97) {
        snprintf(buf, n, "gfx/005.300.%d_soul of %s.anm2", s - 63, PV_SOULS[s - 81]);
        out->t = t; out->v = v; out->s = s; out->anm2 = buf; out->anim = "Idle"; out->frame = 0; out->ov = 0; out->ovf = 0; out->sheet = 0; out->layer = 0; out->scale = 0.5f; out->ox = 0; out->oy = 0; return 0;
    }
    for (int i = 0; i < PV_NSPEC; i++) if (PV_SPEC[i].t == t && PV_SPEC[i].v == v && PV_SPEC[i].s == s) { *out = PV_SPEC[i]; return i + 1; }
    return -1;
}

/* mod.GuppyEyeOffsets: where each of the (up to 16) icons goes, by how many there are */
static const signed char PV_OFF[16][16][2] = {
    { { 0, 4 } },
    { { 8, 4 }, { -8, 4 } },
    { { 0, 8 }, { -12, 0 }, { 12, 0 } },
    { { 0, 12 }, { -12, 6 }, { 12, 6 }, { 0, 0 } },
    { { 0, 16 }, { -9, 9 }, { 9, 9 }, { -6, 0 }, { 6, 0 } },
    { { -6, 16 }, { 6, 16 }, { -9, 8 }, { 9, 8 }, { -6, 0 }, { 6, 0 } },
    { { -6, 16 }, { 6, 16 }, { -12, 8 }, { 0, 8 }, { 12, 8 }, { -6, 0 }, { 6, 0 } },
    { { 0, 16 }, { 9, 14 }, { -9, 14 }, { 12, 8 }, { -12, 8 }, { 9, 2 }, { -9, 2 }, { 0, 0 } },
    { { -12, 16 }, { 0, 16 }, { 12, 16 }, { -12, 8 }, { 0, 8 }, { 12, 8 }, { -12, 0 }, { 0, 0 }, { 12, 0 } },
    { { -8, 16 }, { 0, 16 }, { 8, 16 }, { -12, 8 }, { -4, 8 }, { 4, 8 }, { 12, 8 }, { -8, 0 }, { 0, 0 }, { 8, 0 } },
    { { -12, 16 }, { -4, 16 }, { 4, 16 }, { 12, 16 }, { -8, 8 }, { 0, 8 }, { 8, 8 }, { -12, 0 }, { -4, 0 }, { 4, 0 }, { 12, 0 } },
    { { -12, 12 }, { -4, 12 }, { 4, 12 }, { 12, 12 }, { -12, 6 }, { -4, 6 }, { 4, 6 }, { 12, 6 }, { -12, 0 }, { -4, 0 }, { 4, 0 }, { 12, 0 } },
    { { -6, 15 }, { 6, 15 }, { -12, 10 }, { -4, 10 }, { 4, 10 }, { 12, 10 }, { -12, 5 }, { -4, 5 }, { 4, 5 }, { 12, 5 }, { -8, 0 }, { 0, 0 }, { 8, 0 } },
    { { -8, 15 }, { 0, 15 }, { 8, 15 }, { -12, 10 }, { -4, 10 }, { 4, 10 }, { 12, 10 }, { -12, 5 }, { -4, 5 }, { 4, 5 }, { 12, 5 }, { -8, 0 }, { 0, 0 }, { 8, 0 } },
    { { -8, 15 }, { 0, 15 }, { 8, 15 }, { -12, 10 }, { -4, 10 }, { 4, 10 }, { 12, 10 }, { -12, 5 }, { -4, 5 }, { 4, 5 }, { 12, 5 }, { -12, 0 }, { -4, 0 }, { 4, 0 }, { 12, 0 } },
    { { -12, 15 }, { -4, 15 }, { 4, 15 }, { 12, 15 }, { -12, 10 }, { -4, 10 }, { 4, 10 }, { 12, 10 }, { -12, 5 }, { -4, 5 }, { 4, 5 }, { 12, 5 }, { -12, 0 }, { -4, 0 }, { 4, 0 }, { 12, 0 } },
};

static unsigned char g_pv_spr[PV_N][PV_SPRITE_BYTES] __attribute__((aligned(16)));
static char g_pv_name[PV_N][96];                                                      /* what each sprite holds now (anm2 + sheet): it is loaded again only when that changes */
static signed char g_pv_ok[PV_N];                                                     /* 0 unused, 1 loaded, -1 the engine did not find the file */
static float g_pv_alpha[EN_N];
static int g_pv_drawn, g_pv_logged;

static float pv_lerp(float a, float b, float t) { return a + (b - a) * t; }
/* the nearest player's distance (glasschest.lua: Game():GetNearestPlayer) */
static float pv_nearest(const float *p) {
    float best = 1e9f;
    for (int i = 0, np = sl_nplayers(); i < np; i++) {
        unsigned long pl = sl_player(i); if (!pl || !alive(pl)) continue;
        float dd = vlen(fpos(pl)[0] - p[0], fpos(pl)[1] - p[1]); if (dd < best) best = dd;
    }
    return best < 1e8f ? best : 0.0f;
}

/* draw one icon: sprite slot k, spec sp, at the world position w with the alpha a; 1 = drawn */
static int pv_icon(int k, const pv_spec_t *sp, const float *w, float a, int owner_key) {
    if (!g_pv_new || !g_pv_load || !g_pv_render || !g_pv_screen) return 0;
    char want[96]; snprintf(want, sizeof want, "%s|%s", sp->anm2, sp->sheet ? sp->sheet : "");
    if (strcmp(g_pv_name[k], want)) {
        memset(g_pv_spr[k], 0, PV_SPRITE_BYTES); g_pv_new(g_pv_spr[k]);
        g_pv_ok[k] = (signed char) (g_pv_load(g_pv_spr[k], sp->anm2) ? 1 : -1);
        snprintf(g_pv_name[k], sizeof g_pv_name[k], "%s", want);
        if (g_pv_ok[k] > 0) {
            if (sp->sheet && g_sheet_l) g_sheet_l(g_pv_spr[k], sp->layer, sp->sheet);
            g_setframe(g_pv_spr[k], sp->anim, sp->frame);
            if (sp->ov && g_setoframe) g_setoframe(g_pv_spr[k], sp->ov, sp->ovf);
        } else FFLOG(FFC_ASSET_MISSING, FFKEY(5, 713) + (unsigned) owner_key, "the Glass Chest's preview icon %s: the engine did not find the file", sp->anm2);
    }
    if (g_pv_ok[k] <= 0) return 0;
    float *col = (float *) (g_pv_spr[k] + PV_COLOUR), *sc = (float *) (g_pv_spr[k] + PV_SCALE);
    col[0] = col[1] = col[2] = 1.0f; col[3] = a; sc[0] = sc[1] = sp->scale;
    float sp2[2]; g_pv_screen(w, sp2);
    g_pv_render(g_pv_spr[k], sp2);
    return 1;
}

/* every frame, from the HUD pass: the preview of each Glass Chest that is still closed */
void ffchest_draw(void) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); int used = 0; g_pv_drawn = 0;
    for (unsigned k = 0; k < n; k++) {
        unsigned long e = snap[k];
        if (!alive(e) || *(const int *) (e + E_TYPE) != 5 || *(const int *) (e + E_VARIANT) != CHV_GLASS) continue;
        en_t *s = en_slot(e); sd_t *d = sd_get(e); if (!s->init || d->a[6] || d->a[4] <= 0) continue;           /* opened (a[6]) or nothing in it */
        float *p = fpos(e); int slot = (int) (s - g_en), count = d->a[4] > 16 ? 16 : d->a[4];
        float dist = pv_nearest(p), target = dist < 150.0f ? 0.8f : (0.8f - 0.003f * (dist - 150.0f) > 0.45f ? 0.8f - 0.003f * (dist - 150.0f) : 0.45f);
        if (g_pv_alpha[slot] <= 0.0f) g_pv_alpha[slot] = target;                                           /* gd.Alpha = gd.Alpha or newAlpha */
        g_pv_alpha[slot] = pv_lerp(g_pv_alpha[slot], target, 0.75f);
        for (int i = 0; i < count && used < PV_N; i++) {
            chd_t c = g_chd[d - g_sd][i]; pv_spec_t sp; char buf[96]; int id = pv_find(c.t, c.v, c.s, &sp, buf, sizeof buf);
            if (id < 0) { if (c.t == 5 && c.v == 300) id = pv_find(5, 300, 0, &sp, buf, sizeof buf); else continue; }
            float w[2] = { p[0] + sp.ox - (float) PV_OFF[count - 1][i][0], p[1] + sp.oy - (float) PV_OFF[count - 1][i][1] };
            if (pv_icon(used, &sp, w, g_pv_alpha[slot], i)) { g_pv_drawn++; if (!g_pv_logged) { g_pv_logged = 1; float sc[2]; if (g_pv_screen) g_pv_screen(w, sc); else sc[0] = sc[1] = 0;
                FFLOG(FFC_STATE, FFKEY(5, 713), "Glass Chest preview: first icon %s at world (%.0f,%.0f) -> canvas (%.1f,%.1f), alpha %.2f, %d icons", sp.anm2, (double) w[0], (double) w[1], (double) sc[0], (double) sc[1], (double) g_pv_alpha[slot], count); } }
            used++;
        }
    }
}
