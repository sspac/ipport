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
#else
#define LOG(...) do { } while (0)
static void ff_flush(void) { }
#endif
#endif
