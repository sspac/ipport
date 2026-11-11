/*
 * Fiend Folio sounds (ffsound.c). The port has no Lua and no mod sounds table (sounds.xml is fixed at 833 entries), so the
 * FF sounds are played by the port itself: the .wav files are laid into the pack (port_patch rows "file ff-sounds/NAME"),
 * decoded here (16 bit PCM) into OpenAL buffers and played on a small pool of OpenAL sources of the audio shim.
 *
 * What plays (Lua references in the delivery notes):
 *   - the hurt (vanilla id 55) and death (217) sounds of the Fiend characters are replaced (main.lua FF_PLAYER_HURT/DEATH_SOUND):
 *     Fiend/Classic Fiend: FiendHurt .8 pitch 1.0, death .8/1.0 (Classic .8/1.1); Skeletal Fiend: hurt .5 + vanilla bone sounds;
 *     Fend .8/.7; China .8/1.5.
 *   - the Immoral Heart pickup: its own sound (1 in 1000 the rare one), the vanilla pickup sound is swallowed.
 *   - the minion turning back into a heart: Fiend Heart Drop .8.
 *   - a minion hit: the vanilla "meaty deaths" sound (id 77) at volume .2 pitch 1.5-2.0; a crit: MinionSoundscape .6 pitch .8-1.2.
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <android/log.h>
#include "diag.h"
#include "ffsay.h"

#define FN_SFX_PLAY   0x7f06cc
#define GAME_PTR      0xac3b90
#define GAME_PLAYERS  0x25c40
#define GAME_PLAYER_LIST 0x25c50
#define FN_PLAYER_AT  0x4da574
#define PLAYER_TYPE   0x1738
#define SND_HURT 55
#define SND_DEATH 217
#define SND_BONE_SNAP 462
#define SND_BONE_BREAK 27
#define SND_MEATY 77

void shim_hook(void *at, void *to);
const void *shim_pack_peek(const char *inner, long *length);
void shim_pack_add(const char *path, const void *data, long length);

/* the new sound ids, appended after the game's 833 (sounds.xml gets them, in memory only, the table is made 833 + FF_NSOUNDS long) */
#define FF_NSOUNDS 9
/* a sound record holds at most 7 samples (0x7f02bc: count at record+456, cmp #7 -> "exceeding max samples"), so the 8 hurt
 * samples are two sounds of 4 and one of them is picked at random */
enum { ID_HURT_A = 833, ID_HURT_B, ID_DEATH, ID_DROP, ID_PICKUP, ID_PICKUP_RARE, ID_MINION, ID_FB_LAUNCH, ID_FB_LAND };
static const struct { int id; const char *files[4]; } FF_SND[FF_NSOUNDS] = {
    { ID_HURT_A, { "fiiiiend/fiend_hurt01.wav", "fiiiiend/fiend_hurt02.wav", "fiiiiend/fiend_hurt03.wav", "fiiiiend/fiend_hurt04.wav" } },
    { ID_HURT_B, { "fiiiiend/fiend_hurt05.wav", "fiiiiend/fiend_hurt06.wav", "fiiiiend/fiend_hurt07.wav", "fiiiiend/fiend_hurt08.wav" } },
    { ID_DEATH, { "fiiiiend/fiend_death.wav" } },
    { ID_DROP, { "fiend_heart_drop.wav" } },
    { ID_PICKUP, { "fiend_heart_pickup.wav" } },
    { ID_PICKUP_RARE, { "fiend_heart_pickup2.wav" } },
    { ID_MINION, { "minion1.wav", "minion2.wav", "minion3.wav" } },
    { ID_FB_LAUNCH, { "FireBallLaunch.wav" } },          /* mod.Sounds.FireballLaunch (fiend.lua LaunchFireball) */
    { ID_FB_LAND, { "FireballLand.wav" } },              /* mod.Sounds.FireballLand (fiend.lua explodeFiendProj) */
};
#define ID_HURT hurt_id()
static int hurt_id(void);

/* Called by pack.c the first time the game asks for sounds.xml: gives it the new <sound> records and lays the wavs where the
 * game will look for them (<dir of sounds.xml>sfx/fiendfolio/<path as in the mod>); the files themselves are the mod's sounds, kept under assets/fiendfolio/sounds. inner = sounds.xml's key in the pack. */
void ffsound_on_open(const char *inner) {
    static int done;
    if (done) return;
    done = 1;
    long n = 0;
    const char *xml = (const char *) shim_pack_peek(inner, &n);
    if (!xml || n <= 0) { LOG("ffsound: %s is not in the pack", inner); return; }
    char dir[300]; snprintf(dir, sizeof dir, "%s", inner);
    char *sl = strrchr(dir, '/'); if (sl) sl[1] = 0; else dir[0] = 0;
    const char *end = 0;
    for (const char *p = xml; p + 9 <= xml + n; p++) if (!memcmp(p, "</sounds>", 9)) end = p;
    if (!end) { LOG("ffsound: no </sounds> in %s", inner); return; }
    char add[4096]; int al = 0;
    for (int i = 0; i < FF_NSOUNDS; i++) {
        al += snprintf(add + al, sizeof add - al, "\t<sound id=\"%d\">\n", FF_SND[i].id);
        for (int k = 0; k < 4 && FF_SND[i].files[k]; k++) {
            al += snprintf(add + al, sizeof add - al, "\t\t<sample weight=\"1\" path=\"fiendfolio/%s\" />\n", FF_SND[i].files[k]);
            char from[96], to[400]; long wl = 0;
            snprintf(from, sizeof from, "fiendfolio/sounds/%s", FF_SND[i].files[k]);
            snprintf(to, sizeof to, "%ssfx/fiendfolio/%s", dir, FF_SND[i].files[k]);
            const void *w = shim_pack_peek(from, &wl);
            if (w && wl > 0) {
                shim_pack_add(to, w, wl);
                /* the base game's sounds may be looked for under either resources folder of the pack */
                static const char *const alt[] = { "rebirth-resources/data/", "repentance-resources/data/" };
                for (int a = 0; a < 2; a++) { char t2[400]; snprintf(t2, sizeof t2, "%ssfx/fiendfolio/%s", alt[a], FF_SND[i].files[k]); if (strcmp(t2, to)) shim_pack_add(t2, w, wl); }
            } else LOG("ffsound: %s is missing from the assets", from);
        }
        al += snprintf(add + al, sizeof add - al, "\t</sound>\n");
    }
    long on = (long) (end - xml) + al + (n - (end - xml));
    char *out = (char *) malloc((size_t) on);
    if (!out) return;
    memcpy(out, xml, (size_t) (end - xml));
    memcpy(out + (end - xml), add, (size_t) al);
    memcpy(out + (end - xml) + al, end, (size_t) (n - (end - xml)));
    shim_pack_add(inner, out, on);                      /* never freed: the pack keeps the pointer */
    LOG("ffsound: %s now has %d more sounds (ids %d..%d); wavs under %ssfx/fiendfolio/ (and the other resources folder)", inner, FF_NSOUNDS, ID_HURT_A, ID_FB_LAND, dir);
}

#ifndef FF_HOST_TEST
#include <sys/mman.h>

static unsigned char *g_base;
typedef void (*sfx_fn)(unsigned long, int, int, int, float, float, float);
static sfx_fn o_sfx;
static unsigned long g_mgr;
static int g_swallow;                    /* > 0: an immoral heart is being collected: its own sound replaces the soul heart's (customhealth.lua CollectSound) */
static int g_own;                        /* > 0: our own call into the engine, not to be replaced */

static unsigned g_rng = 2463534242u;
static unsigned rnd(unsigned n) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return n ? g_rng % n : 0; }
static float frnd(float a, float b) { return a + (b - a) * (float) (rnd(1000)) / 1000.0f; }

/* the engine's own sound, not replaced by us */
static void engine_sound(int id, float vol, float pitch) {
    static int told;
    if (told < 14) { told++; LOG("ffsound: our own play of sound %d (vol %.2f, pitch %.2f)%s", id, (double) vol, (double) pitch, g_mgr ? "" : " - NO sound manager yet"); }
    if (!o_sfx || !g_mgr) return;
    g_own++; o_sfx(g_mgr, id, 0, 0, vol, pitch, 0.0f); g_own--;
}

static int ff_char(void) {                       /* the first player's character if it is a Fiend Folio one, else 0 */
    if (!g_base) return 0;
    unsigned long game = *(const unsigned long *) (g_base + GAME_PTR);
    if (!game) return 0;
    const unsigned long *l = (const unsigned long *) (game + GAME_PLAYER_LIST);
    if (!l[0] || l[1] < l[0]) return 0;
    unsigned long p = ((unsigned long (*)(unsigned long, int)) (g_base + FN_PLAYER_AT))(game + GAME_PLAYERS, 0);
    if (!p) return 0;
    int t = *(const int *) (p + PLAYER_TYPE);
    return t >= 41 && t <= 60 ? t : 0;
}

enum { C_FIEND = 41, C_CHINA = 50, C_CLASSIC = 52, C_SKELETAL = 54, C_FEND = 55 };

static int hurt_id(void) { return ID_HURT_A + (int) rnd(2); }

static void w_sfx(unsigned long mgr, int id, int delay, int loop, float vol, float pitch, float pan) {
    if (!g_mgr) g_mgr = mgr;
    if (g_own) { o_sfx(mgr, id, delay, loop, vol, pitch, pan); return; }
    if (g_swallow > 0) return;
    /* Lua main.lua:4806-4825 (PRE_SFX_PLAY on ISAACDIES) and 21150+ (sfx:Stop(HURT_GRUNT)): only for a Fiend Folio character,
     * the vanilla grunt/death is cut and the character's own sound plays instead. Everyone else keeps the game's. */
    if (id == SND_HURT || id == SND_DEATH) {
        int c = ff_char();
        static int seen; if (seen < 8) { seen++; LOG("ffsound: the game plays %s: first player is Fiend Folio character %d (0 = not one)", id == SND_HURT ? "HURT_GRUNT" : "ISAACDIES", c); }
        if (c) {
            int hurt = id == SND_HURT;
            switch (c) {
            case C_FIEND:   engine_sound(hurt ? ID_HURT : ID_DEATH, 0.8f, 1.0f); return;
            case C_CLASSIC: engine_sound(hurt ? ID_HURT : ID_DEATH, 0.8f, hurt ? 1.0f : 1.1f); return;
            case C_FEND:    engine_sound(hurt ? ID_HURT : ID_DEATH, 0.8f, 0.7f); return;
            case C_CHINA:   if (hurt) { engine_sound(ID_HURT, 0.8f, 1.5f); return; } break;
            case C_SKELETAL:
                if (hurt) { engine_sound(ID_HURT, 0.5f, 0.5f); engine_sound(SND_BONE_BREAK, 1.0f, 1.0f); engine_sound(SND_BONE_SNAP, 1.0f, 1.0f); }
                return;                                           /* no death sound */
            default: break;
            }
        }
    }
    o_sfx(mgr, id, delay, loop, vol, pitch, pan);
}

void ffsound_pickup_begin(void) { g_swallow++; }
void ffsound_pickup_end(int collected) { if (g_swallow > 0) g_swallow--; if (collected) engine_sound(rnd(1000) == 0 ? ID_PICKUP_RARE : ID_PICKUP, 1.0f, 1.0f); }
void ffsound_heart_drop(void) { engine_sound(ID_DROP, 0.8f, 1.0f); }
void ffsound_minion_hit(void) { engine_sound(SND_MEATY, 0.2f, frnd(1.5f, 2.0f)); }
/* the fireballs (fffireball.h): the mod's own two sounds and the game's own ids, played by us (vanilla ids measured from the game's sounds.xml: 483 flame_burst, 536 flamethrower end, 182 boss explosions, 471 candle_light) */
void ffsound_fireball(int what, float vol, float pitch) {
    static const int ids[] = { ID_FB_LAUNCH, ID_FB_LAND, 483, 536, 182, 471 };
    if (what >= 0 && what < 6) engine_sound(ids[what], vol, pitch);
}
void ffsound_minion_crit(void) { engine_sound(ID_MINION, 0.6f, frnd(0.8f, 1.2f)); }

/* make the game's sounds table FF_NSOUNDS longer: resize(833) at 0x7eff74 and the two id bounds (cmp #833, cmp #832) */
static int patch_imm(unsigned char *base, unsigned off, uint32_t was, uint32_t now) {
    uint32_t *w = (uint32_t *) (base + off);
    if (*w != was) { LOG("ffsound: 0x%x is %08x, expected %08x - the sounds table keeps its size", off, *w, was); return 0; }
    *w = now;
    return 1;
}
static void grow_table(unsigned char *base) {
    const uint32_t N = FF_NSOUNDS;
    /* mov w1,#imm = 0x52800001 | imm<<5 ; cmp w20,#imm = 0x7100029f | imm<<10 */
    if (*(uint32_t *) (base + 0x7eff74) != (0x52800001u | (833u << 5)) || *(uint32_t *) (base + 0x7f01a0) != (0x7100029fu | (833u << 10)) ||
        *(uint32_t *) (base + 0x7f01bc) != (0x7100029fu | (832u << 10))) { LOG("ffsound: the sounds table loader is not as measured - not grown"); return; }
    patch_imm(base, 0x7eff74, 0x52800001u | (833u << 5), 0x52800001u | ((833u + N) << 5));
    patch_imm(base, 0x7f01a0, 0x7100029fu | (833u << 10), 0x7100029fu | ((833u + N) << 10));
    patch_imm(base, 0x7f01bc, 0x7100029fu | (832u << 10), 0x7100029fu | ((832u + N) << 10));
    __builtin___clear_cache((char *) base + 0x7eff74, (char *) base + 0x7eff78);
    __builtin___clear_cache((char *) base + 0x7f01a0, (char *) base + 0x7f01c0);
    LOG("ffsound: the sounds table is now %d long", 833 + FF_NSOUNDS);
}

void shim_ffsound_install(unsigned char *base) {
    static const uint32_t want[4] = { 0xd10243ff, 0x6d022beb, 0x6d0323e9, 0xa90467fa };
    g_base = base;
    grow_table(base);
    const uint32_t *fn = (const uint32_t *) (base + FN_SFX_PLAY);
    for (int k = 0; k < 4; k++) if (fn[k] != want[k]) { LOG("ffsound: 0x%x does not start as measured - nothing touched", FN_SFX_PLAY); return; }
    uint8_t *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) { LOG("ffsound: no page for the continuation"); return; }
    memcpy(page, fn, 16);
    shim_hook(page + 16, (void *) (fn + 4));
    if (mprotect(page, 4096, PROT_READ | PROT_EXEC) != 0) { LOG("ffsound: mprotect failed"); munmap(page, 4096); return; }
    __builtin___clear_cache((char *) page, (char *) page + 32);
    o_sfx = (sfx_fn) page;
    shim_hook((void *) fn, (void *) w_sfx);
    LOG("ffsound: the sound manager's play call is wrapped");
}
#endif
