/*
 * One place for the Fiend Folio modules' log lines. In a report build (ISAAC_DIAG=1) every line goes to logcat AND, when the
 * launch report is open, to Downloads/isaac-start.txt (kind "ff") - so a phone without adb can still show what happened.
 * The modules install at load, before the report is open: those lines are kept and handed over at the module's first hook call
 * (ff_flush). In the clean build (ISAAC_DIAG=0) all of it compiles to nothing.
 */
#ifndef FFSAY_H
#define FFSAY_H
#include <string.h>
#include <stdio.h>
#if ISAAC_DIAG
int shim_report_active(void);
void report_line(const char *kind, const char *text);
#define FF_EARLY_MAX 1600
/* one buffer for all the modules: whichever hook runs first hands everything over */
char ff_early[FF_EARLY_MAX] __attribute__((weak));
int ff_early_n __attribute__((weak));
static void ff_say(const char *line) {
    __android_log_print(ANDROID_LOG_INFO, "isaac", "%s", line);
    if (shim_report_active()) { report_line("ff", line); return; }
    int n = (int) strlen(line);
    if (ff_early_n + n + 2 >= FF_EARLY_MAX) return;
    memcpy(ff_early + ff_early_n, line, (size_t) n);
    ff_early_n += n;
    ff_early[ff_early_n++] = '\n';
    ff_early[ff_early_n] = 0;
}
static void ff_flush(void) {
    if (!ff_early_n || !shim_report_active()) return;
    char *p = ff_early;
    while (*p) {
        char *e = strchr(p, '\n');
        if (!e) break;
        *e = 0;
        report_line("ff", p);
        p = e + 1;
    }
    ff_early_n = 0; ff_early[0] = 0;
}
#define LOG(...) do { char ffb_[360]; snprintf(ffb_, sizeof ffb_, __VA_ARGS__); ff_say(ffb_); } while (0)

/*
 * Log by CATEGORY (every Fiend Folio module uses these; all of it is in this one header, as ffsay.h always was).
 *   FFLOG(FFC_X, key, "text %d", ...)   prints "[X] text", but only while the category has shown fewer than FFC_CAP lines and
 *                                      this key (an entity as FFKEY(type, variant), a sound id, a room...) fewer than FFC_KEYCAP.
 * EVERY call is counted, shown or not: ff_log_summary() prints, per category, how many events there were, how many lines were
 * shown and the three keys with most events - so a cap never hides how often something happened.
 * The counters are WEAK globals (like ff_early): ffchars.c, ffheart.c, ffminion.c, ffsound.c ... are separate files and share one set.
 * In the clean build (ISAAC_DIAG=0) FFLOG and ff_log_summary compile to nothing.
 */
enum { FFC_SPAWN, FFC_UNHANDLED, FFC_STATE, FFC_GRID_SAVE, FFC_GRID_RESTORE, FFC_GRID_LOST, FFC_GRID_COLLISION, FFC_DEATH, FFC_DEATH_NO_ANIM,
       FFC_ASSET_MISSING, FFC_SOUND, FFC_SOUND_MISS, FFC_TELEPORT, FFC_OUT_OF_ROOM, FFC_SPEED, FFC_SPEED_SPIKE, FFC_AI_TARGET, FFC_STUCK,
       FFC_SPRITE_LAYER, FFC_ANIM_GAP, FFC_SPRITE_STATIC, FFC_ROOM, FFC_NOT_DONE, FFC_N };
static const char *const FFC_NAME[FFC_N] = { "SPAWN", "UNHANDLED", "STATE", "GRID_SAVE", "GRID_RESTORE", "GRID_LOST", "GRID_COLLISION", "DEATH", "DEATH_NO_ANIM",
       "ASSET_MISSING", "SOUND", "SOUND_MISS", "TELEPORT", "OUT_OF_ROOM", "SPEED", "SPEED_SPIKE", "AI_TARGET", "STUCK",
       "SPRITE_LAYER", "ANIM_GAP", "SPRITE_STATIC", "ROOM", "NOT_DONE" };
/* lines shown per category, and per key inside a category */
static const unsigned short FFC_CAP[FFC_N]    = { 700, 300, 500, 300, 300, 300, 300, 300, 300,   400, 1500, 300, 300, 300, 400, 500, 400, 400,   300, 400, 400, 700, 300 };
static const unsigned char  FFC_KEYCAP[FFC_N] = {   3,   4,  12,   6,   6,   6,   3,   4,   4,     2,    8,   3,   6,   6,   4,   6,   4,   4,     3,   3,   4,   3,   1 };
/* 1: the key is FFKEY(type, variant) and the summary prints it as type.variant */
static const unsigned char  FFC_ENTKEY[FFC_N] = { 1, 1, 1, 1, 1, 1, 1, 1, 1,   1, 0, 0, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0 };
#define FFKEY(type, var) ((((unsigned) (type)) << 16) | (((unsigned) (var)) & 0xffffu))
#define FF_KEYS 1024
int ff_cat_seen[FFC_N] __attribute__((weak));
int ff_cat_shown[FFC_N] __attribute__((weak));
unsigned short ff_key_cat[FF_KEYS] __attribute__((weak));
unsigned ff_key_id[FF_KEYS] __attribute__((weak));
unsigned short ff_key_n[FF_KEYS] __attribute__((weak));
/* counts the event; 1 = print it. A full key table (never seen) leaves only the category cap. */
static int ff_gate(int cat, unsigned key) {
    ff_cat_seen[cat]++;
    int kn = -1;
    unsigned h = (key * 2654435761u + (unsigned) cat * 40503u) % FF_KEYS;
    for (int i = 0; i < FF_KEYS; i++) {
        unsigned j = (h + (unsigned) i) % FF_KEYS;
        if (!ff_key_n[j]) { ff_key_cat[j] = (unsigned short) cat; ff_key_id[j] = key; ff_key_n[j] = 1; kn = 1; break; }
        if (ff_key_cat[j] == cat && ff_key_id[j] == key) { if (ff_key_n[j] < 65535) ff_key_n[j]++; kn = ff_key_n[j]; break; }
    }
    if (kn > (int) FFC_KEYCAP[cat]) return 0;
    if (ff_cat_shown[cat] >= (int) FFC_CAP[cat]) return 0;
    ff_cat_shown[cat]++;
    return 1;
}
#define FFLOG(cat, key, ...) do { if (ff_gate((cat), (unsigned) (key))) { char ffb_[360]; int ffn_ = snprintf(ffb_, sizeof ffb_, "[%s] ", FFC_NAME[(cat)]); \
    if (ffn_ > 0 && ffn_ < (int) sizeof ffb_) snprintf(ffb_ + ffn_, sizeof ffb_ - (size_t) ffn_, __VA_ARGS__); ff_say(ffb_); } } while (0)
/* one line per category that had events: how many, how many were shown, and the three keys with most events */
static void ff_log_summary(const char *why) {
    for (int c = 0; c < FFC_N; c++) {
        if (!ff_cat_seen[c]) continue;
        int top[3] = { -1, -1, -1 };
        for (int j = 0; j < FF_KEYS; j++) {
            if (!ff_key_n[j] || ff_key_cat[j] != c) continue;
            for (int t = 0; t < 3; t++) {
                if (top[t] < 0 || ff_key_n[j] > ff_key_n[top[t]]) { for (int u = 2; u > t; u--) top[u] = top[u - 1]; top[t] = j; break; }
            }
        }
        char b[360]; int n = snprintf(b, sizeof b, "[SUMMARY] %s %s: %d events, %d lines shown; most:", why ? why : "", FFC_NAME[c], ff_cat_seen[c], ff_cat_shown[c]);
        for (int t = 0; t < 3 && top[t] >= 0 && n > 0 && n < (int) sizeof b; t++) {
            unsigned id = ff_key_id[top[t]];
            if (FFC_ENTKEY[c]) n += snprintf(b + n, sizeof b - (size_t) n, " %u.%u x%d", id >> 16, id & 0xffffu, ff_key_n[top[t]]);
            else n += snprintf(b + n, sizeof b - (size_t) n, " %u x%d", id, ff_key_n[top[t]]);
        }
        ff_say(b);
    }
}
#else
#define LOG(...) do { } while (0)
static void ff_flush(void) { }
#define FFKEY(type, var) 0u
#define FFLOG(cat, key, ...) do { } while (0)
static void ff_log_summary(const char *why) { (void) why; }
enum { FFC_SPAWN, FFC_UNHANDLED, FFC_STATE, FFC_GRID_SAVE, FFC_GRID_RESTORE, FFC_GRID_LOST, FFC_GRID_COLLISION, FFC_DEATH, FFC_DEATH_NO_ANIM,
       FFC_ASSET_MISSING, FFC_SOUND, FFC_SOUND_MISS, FFC_TELEPORT, FFC_OUT_OF_ROOM, FFC_SPEED, FFC_SPEED_SPIKE, FFC_AI_TARGET, FFC_STUCK,
       FFC_SPRITE_LAYER, FFC_ANIM_GAP, FFC_SPRITE_STATIC, FFC_ROOM, FFC_NOT_DONE, FFC_N };
#endif
#endif
