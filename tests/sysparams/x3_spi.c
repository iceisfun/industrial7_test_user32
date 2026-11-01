/* sysparams: read-only probes of obsolete / environment-dependent SPI queries and metrics.
 * Values are class G (environment) so they are logged; only the call protocol is asserted. */
#include "sp.h"

#define SPI_X3_GETLOWPOWERTIMEOUT 79
#define SPI_X3_GETPOWEROFFTIMEOUT 80
#define SPI_X3_GETLOWPOWERACTIVE 83
#define SPI_X3_GETPOWEROFFACTIVE 84
#define SPI_X3_GETDEFAULTINPUTLANG 89

TEST(sysparams_x3_obsolete_power_queries_note) {
    static const struct { const char *n; UINT a; } q[] = {
        {"GETLOWPOWERTIMEOUT", SPI_X3_GETLOWPOWERTIMEOUT}, {"GETPOWEROFFTIMEOUT", SPI_X3_GETPOWEROFFTIMEOUT},
        {"GETLOWPOWERACTIVE", SPI_X3_GETLOWPOWERACTIVE}, {"GETPOWEROFFACTIVE", SPI_X3_GETPOWEROFFACTIVE},
    };
    for (int i = 0; i < 4; i++) {
        int v = (int)SENT32;
        SetLastError(0);
        BOOL r = sW(q[i].a, 0, &v, 0);
        t_note("SPI_%s -> %d value %d err %lu", q[i].n, r, v, GetLastError());
    }
}

TEST(sysparams_x3_getdefaultinputlang_returns_hkl) {
    HKL h = (HKL)(ULONG_PTR)0xCDCDCDCD;
    SetLastError(0);
    BOOL r = sW(SPI_X3_GETDEFAULTINPUTLANG, 0, &h, 0);
    CHECK(r);
    if (r) CHECK(h != NULL && (ULONG_PTR)h != 0xCDCDCDCD);
    t_note("default input language HKL %p", (void *)h);
}

TEST(sysparams_x3_kanji_window_metric_class_g) {
    int v = gsm(SM_CYKANJIWINDOW);
    t_note("SM_CYKANJIWINDOW=%d SM_DBCSENABLED=%d", v, gsm(SM_DBCSENABLED));
    CHECK(v >= 0);
    if (!gsm(SM_DBCSENABLED)) CHECK(v == 0);      /* documented: height of the Kanji window, only on DBCS systems */
}

TEST(sysparams_x3_getworkarea_inside_screen) {
    RECT w = {0}; 
    CHECK(sW(SPI_GETWORKAREA, 0, &w, 0));
    int sx = gsm(SM_CXSCREEN), sy = gsm(SM_CYSCREEN);
    CHECK(w.right > w.left && w.bottom > w.top);
    CHECK(w.right - w.left <= sx && w.bottom - w.top <= sy);
    t_note("work area %ld,%ld,%ld,%ld on %dx%d", w.left, w.top, w.right, w.bottom, sx, sy);
}

TEST(sysparams_x3_invalid_action_codes_fail_without_crash) {
    int v = 0;
    static const UINT bad[] = { 0, 0x7FFF, 0xFFFF, 0x10000 };
    for (int i = 0; i < 4; i++) {
        SetLastError(0);
        BOOL r = sW(bad[i], 0, &v, 0);
        t_note("SPI action 0x%x -> %d err %lu", bad[i], r, GetLastError());
    }
}
