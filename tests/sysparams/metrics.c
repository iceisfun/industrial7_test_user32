/* GetSystemMetrics / GetSystemMetricsForDpi */
#include "sp.h"

#ifndef SM_MOUSEHORIZONTALWHEELPRESENT
#define SM_MOUSEHORIZONTALWHEELPRESENT 91
#endif
#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER 92
#endif
#ifndef SM_DIGITIZER
#define SM_DIGITIZER 94
#endif
#ifndef SM_MAXIMUMTOUCHES
#define SM_MAXIMUMTOUCHES 95
#endif
#ifndef SM_CONVERTIBLESLATEMODE
#define SM_CONVERTIBLESLATEMODE 0x2003
#endif
#ifndef SM_SYSTEMDOCKED
#define SM_SYSTEMDOCKED 0x2004
#endif

enum { K_POS, K_NONNEG, K_BOOL, K_ZERO, K_ANY, K_RANGE_0_2, K_NET };

static void check_metric(int idx, int kind, const char *name) {
    int v, v2;
    REQUIRE(U(GetSystemMetrics));
    SetLastError(0xdeadbeef);
    v = U(GetSystemMetrics)(idx);
    v2 = U(GetSystemMetrics)(idx);
    CHECKF(v == v2, "%s: unstable %d vs %d", name, v, v2);
    switch (kind) {
    case K_POS: CHECKF(v > 0, "%s = %d", name, v); CHECKF(v < 100000, "%s = %d too large", name, v); break;
    case K_NONNEG: CHECKF(v >= 0, "%s = %d", name, v); break;
    case K_BOOL: CHECKF(v == 0 || v == 1, "%s = %d", name, v); break;
    case K_ZERO: CHECKF(v == 0, "%s = %d", name, v); break;
    case K_RANGE_0_2: CHECKF(v >= 0 && v <= 2, "%s = %d", name, v); break;
    case K_NET: CHECKF((v & ~1) == 0, "%s = %d", name, v); break;
    default: break;
    }
    /* ForDpi at the same DPI as the process is not required to match for all indices; just must not crash */
    if (U(GetSystemMetricsForDpi)) {
        int d = U(GetSystemMetricsForDpi)(idx, 96);
        if (kind == K_POS || kind == K_NONNEG) CHECKF(d >= 0, "%s ForDpi(96) = %d", name, d);
        if (kind == K_BOOL) CHECKF(d == 0 || d == 1, "%s ForDpi(96) = %d", name, d);
    }
}
#define SMT(n, kind) TEST(sm_##n) { check_metric(SM_##n, kind, "SM_" #n); }

SMT(CXSCREEN, K_POS) SMT(CYSCREEN, K_POS) SMT(CXVSCROLL, K_POS) SMT(CYHSCROLL, K_POS)
SMT(CYCAPTION, K_POS) SMT(CXBORDER, K_POS) SMT(CYBORDER, K_POS) SMT(CXDLGFRAME, K_POS)
SMT(CYDLGFRAME, K_POS) SMT(CYVTHUMB, K_POS) SMT(CXHTHUMB, K_POS) SMT(CXICON, K_POS)
SMT(CYICON, K_POS) SMT(CXCURSOR, K_POS) SMT(CYCURSOR, K_POS) SMT(CYMENU, K_POS)
SMT(CXFULLSCREEN, K_POS) SMT(CYFULLSCREEN, K_POS) SMT(CYKANJIWINDOW, K_NONNEG)
SMT(MOUSEPRESENT, K_BOOL) SMT(CYVSCROLL, K_POS) SMT(CXHSCROLL, K_POS)
SMT(DEBUG, K_BOOL) SMT(SWAPBUTTON, K_BOOL) SMT(CXMIN, K_POS) SMT(CYMIN, K_POS)
SMT(CXSIZE, K_POS) SMT(CYSIZE, K_POS) SMT(CXFRAME, K_POS) SMT(CYFRAME, K_POS)
SMT(CXMINTRACK, K_POS) SMT(CYMINTRACK, K_POS) SMT(CXDOUBLECLK, K_POS) SMT(CYDOUBLECLK, K_POS)
SMT(CXICONSPACING, K_POS) SMT(CYICONSPACING, K_POS) SMT(MENUDROPALIGNMENT, K_BOOL)
SMT(PENWINDOWS, K_BOOL) SMT(DBCSENABLED, K_BOOL) SMT(CMOUSEBUTTONS, K_NONNEG)
SMT(SECURE, K_ZERO) SMT(CXEDGE, K_POS) SMT(CYEDGE, K_POS) SMT(CXMINSPACING, K_POS)
SMT(CYMINSPACING, K_POS) SMT(CXSMICON, K_POS) SMT(CYSMICON, K_POS) SMT(CYSMCAPTION, K_POS)
SMT(CXSMSIZE, K_POS) SMT(CYSMSIZE, K_POS) SMT(CXMENUSIZE, K_POS) SMT(CYMENUSIZE, K_POS)
SMT(ARRANGE, K_NONNEG) SMT(CXMINIMIZED, K_POS) SMT(CYMINIMIZED, K_POS)
SMT(CXMAXTRACK, K_POS) SMT(CYMAXTRACK, K_POS) SMT(CXMAXIMIZED, K_POS) SMT(CYMAXIMIZED, K_POS)
SMT(NETWORK, K_ANY) SMT(CLEANBOOT, K_RANGE_0_2) SMT(CXDRAG, K_POS) SMT(CYDRAG, K_POS)
SMT(SHOWSOUNDS, K_BOOL) SMT(CXMENUCHECK, K_POS) SMT(CYMENUCHECK, K_POS)
SMT(SLOWMACHINE, K_BOOL) SMT(MIDEASTENABLED, K_BOOL) SMT(MOUSEWHEELPRESENT, K_BOOL)
SMT(XVIRTUALSCREEN, K_ANY) SMT(YVIRTUALSCREEN, K_ANY) SMT(CXVIRTUALSCREEN, K_POS)
SMT(CYVIRTUALSCREEN, K_POS) SMT(CMONITORS, K_POS) SMT(SAMEDISPLAYFORMAT, K_BOOL)
SMT(IMMENABLED, K_BOOL) SMT(CXFOCUSBORDER, K_POS) SMT(CYFOCUSBORDER, K_POS)
SMT(TABLETPC, K_NONNEG) SMT(MEDIACENTER, K_NONNEG) SMT(STARTER, K_BOOL) SMT(SERVERR2, K_BOOL)
SMT(MOUSEHORIZONTALWHEELPRESENT, K_BOOL) SMT(CXPADDEDBORDER, K_NONNEG)
SMT(DIGITIZER, K_NONNEG) SMT(MAXIMUMTOUCHES, K_NONNEG)
SMT(REMOTESESSION, K_BOOL) SMT(SHUTTINGDOWN, K_BOOL) SMT(REMOTECONTROL, K_BOOL)
SMT(CARETBLINKINGENABLED, K_BOOL) SMT(CONVERTIBLESLATEMODE, K_BOOL) SMT(SYSTEMDOCKED, K_BOOL)

/* The reserved slots. */
TEST(sm_reserved_slots) {
    int i;
    REQUIRE(U(GetSystemMetrics));
    for (i = SM_RESERVED1; i <= SM_RESERVED4; i++) CHECKF(gsm(i) == 0, "reserved SM %d = %d", i, gsm(i));
}
TEST(sm_unassigned_indices_observed) {
    static const int idx[] = { 64, 65, 66, 85, 93, 96, 97, 98, 99, 100 };
    unsigned i;
    REQUIRE(U(GetSystemMetrics));
    for (i = 0; i < sizeof idx / sizeof *idx; i++) {
        int v = gsm(idx[i]);
        if (v) t_note("unassigned SM %d = %d", idx[i], v);
    }
}
TEST(sm_cmetrics_constant) {
    /* header constant differs by WINVER; Win10 recognises the full range up to 95 */
    REQUIRE(U(GetSystemMetrics));
    CHECK(SM_CMETRICS >= 76);
    t_note("SM_CMETRICS=%d", SM_CMETRICS);
}

/* Out-of-range indices return zero. */
static const int bad_idx[] = { -1, -2, -100, -32768, (int)0x80000000, (int)0xFFFFFFFF, 0x7FFFFFFF, 200, 255, 256, 1000, 4095, 0x1001, 0x1FFF, 0x2005, 0x2010, 0x3000, 0x10000, 0x7000 };
TEST(sm_invalid_indices_zero) {
    unsigned i;
    REQUIRE(U(GetSystemMetrics));
    for (i = 0; i < sizeof bad_idx / sizeof *bad_idx; i++) {
        int v;
        SetLastError(0xdeadbeef);
        v = U(GetSystemMetrics)(bad_idx[i]);
        CHECKF(v == 0, "GetSystemMetrics(%d) = %d", bad_idx[i], v);
        if (GetLastError() != 0xdeadbeef) t_note("GetSystemMetrics(%d) set last error %lu", bad_idx[i], GetLastError());
    }
}
TEST(sm_invalid_indices_fordpi) {
    unsigned i;
    REQUIRE(U(GetSystemMetricsForDpi));
    for (i = 0; i < sizeof bad_idx / sizeof *bad_idx; i++) {
        int v;
        SetLastError(0xdeadbeef);
        v = U(GetSystemMetricsForDpi)(bad_idx[i], 96);
        CHECKF(v == 0, "GetSystemMetricsForDpi(%d,96) = %d", bad_idx[i], v);
    }
}
TEST(sm_sweep_0_to_511_no_crash) {
    int i, nonzero = 0;
    REQUIRE(U(GetSystemMetrics));
    for (i = 0; i < 512; i++) if (U(GetSystemMetrics)(i)) nonzero++;
    CHECK(nonzero >= 50);
    CHECK(nonzero < 120);
}
TEST(sm_sweep_special_range) {
    int i, nonzero = 0;
    REQUIRE(U(GetSystemMetrics));
    for (i = 0x1000; i < 0x1100; i++) if (U(GetSystemMetrics)(i)) nonzero++;
    for (i = 0x2000; i < 0x2100; i++) if (U(GetSystemMetrics)(i)) nonzero++;
    t_note("nonzero special indices: %d", nonzero);
}

/* Aliases and structural relations. */
TEST(sm_alias_fixedframe) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(SM_CXFIXEDFRAME == SM_CXDLGFRAME && SM_CYFIXEDFRAME == SM_CYDLGFRAME);
    CHECK(SM_CXSIZEFRAME == SM_CXFRAME && SM_CYSIZEFRAME == SM_CYFRAME);
    CHECK(gsm(SM_CXFIXEDFRAME) == gsm(SM_CXDLGFRAME));
    CHECK(gsm(SM_CYSIZEFRAME) == gsm(SM_CYFRAME));
}
TEST(sm_relation_scrollbars) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CYVSCROLL) > 0 && gsm(SM_CXHSCROLL) > 0);
    CHECK(gsm(SM_CXVSCROLL) == gsm(SM_CYHSCROLL));
    CHECK(gsm(SM_CYVTHUMB) == gsm(SM_CXHTHUMB));
    CHECK(gsm(SM_CXVSCROLL) >= gsm(SM_CXBORDER) * 2);
}
TEST(sm_relation_screen_vs_virtual) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CXVIRTUALSCREEN) >= gsm(SM_CXSCREEN));
    CHECK(gsm(SM_CYVIRTUALSCREEN) >= gsm(SM_CYSCREEN));
    CHECK(gsm(SM_XVIRTUALSCREEN) <= 0);
    CHECK(gsm(SM_YVIRTUALSCREEN) <= 0);
    CHECK(gsm(SM_XVIRTUALSCREEN) + gsm(SM_CXVIRTUALSCREEN) >= gsm(SM_CXSCREEN));
    CHECK(gsm(SM_YVIRTUALSCREEN) + gsm(SM_CYVIRTUALSCREEN) >= gsm(SM_CYSCREEN));
}
TEST(sm_relation_fullscreen_maximized) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CXFULLSCREEN) <= gsm(SM_CXSCREEN));
    CHECK(gsm(SM_CYFULLSCREEN) <= gsm(SM_CYSCREEN));
    CHECK(gsm(SM_CXMAXIMIZED) <= gsm(SM_CXMAXTRACK));
    t_note("CYMAXIMIZED=%d CYMAXTRACK=%d", gsm(SM_CYMAXIMIZED), gsm(SM_CYMAXTRACK));
    CHECK(gsm(SM_CXMAXTRACK) >= gsm(SM_CXMINTRACK));
    CHECK(gsm(SM_CYMAXTRACK) >= gsm(SM_CYMINTRACK));
    CHECK(gsm(SM_CXMINTRACK) >= gsm(SM_CXMIN));
    CHECK(gsm(SM_CYMINTRACK) >= gsm(SM_CYMIN));
}
TEST(sm_relation_maximized_covers_work_area) {
    RECT wa;
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETWORKAREA, 0, &wa, 0));
    /* a maximized window is exactly work area plus its sizing borders */
    CHECK(gsm(SM_CXMAXIMIZED) >= wa.right - wa.left);
    t_note("CXMAXIMIZED=%d CYMAXIMIZED=%d work=%ldx%ld", gsm(SM_CXMAXIMIZED), gsm(SM_CYMAXIMIZED), wa.right - wa.left, wa.bottom - wa.top);
}
TEST(sm_relation_small_vs_large_icon) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CXSMICON) <= gsm(SM_CXICON));
    CHECK(gsm(SM_CYSMICON) <= gsm(SM_CYICON));
    CHECK(gsm(SM_CXICON) >= 16 && gsm(SM_CYICON) >= 16);
    CHECK(gsm(SM_CXSMICON) >= 8);
}
TEST(sm_relation_icon_square) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CXICON) == gsm(SM_CYICON));
    CHECK(gsm(SM_CXSMICON) == gsm(SM_CYSMICON));
    CHECK(gsm(SM_CXCURSOR) == gsm(SM_CYCURSOR));
}
TEST(sm_relation_caption_sizes) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CYSMCAPTION) <= gsm(SM_CYCAPTION));
    CHECK(gsm(SM_CYSIZE) > 0 && gsm(SM_CXSIZE) > 0);
    CHECK(gsm(SM_CXSMSIZE) > 0 && gsm(SM_CYSMSIZE) > 0);
    CHECK(gsm(SM_CYSIZE) <= gsm(SM_CYCAPTION));
    CHECK(gsm(SM_CYSMSIZE) <= gsm(SM_CYSMCAPTION));
}
TEST(sm_relation_edge_border) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CXEDGE) >= gsm(SM_CXBORDER));
    CHECK(gsm(SM_CYEDGE) >= gsm(SM_CYBORDER));
    CHECK(gsm(SM_CXBORDER) >= 1 && gsm(SM_CYBORDER) >= 1);
    CHECK(gsm(SM_CXFRAME) >= gsm(SM_CXDLGFRAME));
}
TEST(sm_relation_minimized_spacing) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CXMINSPACING) >= gsm(SM_CXMINIMIZED));
    CHECK(gsm(SM_CYMINSPACING) >= gsm(SM_CYMINIMIZED));
    CHECK(gsm(SM_CXMINIMIZED) >= gsm(SM_CXMINTRACK) - 100);
}
TEST(sm_relation_menu_metrics) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CYMENU) > 0);
    CHECK(gsm(SM_CXMENUCHECK) > 0 && gsm(SM_CYMENUCHECK) > 0);
    CHECK(gsm(SM_CXMENUSIZE) > 0);
}
TEST(sm_relation_mouse) {
    REQUIRE(U(GetSystemMetrics));
    if (gsm(SM_MOUSEPRESENT)) CHECK(gsm(SM_CMOUSEBUTTONS) >= 2);
    else t_note("no mouse present; buttons=%d", gsm(SM_CMOUSEBUTTONS));
}
TEST(sm_network_low_bit) {
    int v;
    REQUIRE(U(GetSystemMetrics));
    v = gsm(SM_NETWORK);
    CHECK(v >= 0);
    t_note("SM_NETWORK=%d (only bit 0 documented)", v);
}
TEST(sm_relation_tablet_digitizer) {
    REQUIRE(U(GetSystemMetrics));
    CHECK((gsm(SM_DIGITIZER) & ~0xFF) == 0);
}
TEST(sm_relation_cleanboot_normal) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CLEANBOOT) == 0 || gsm(SM_CLEANBOOT) == 1 || gsm(SM_CLEANBOOT) == 2);
}
TEST(sm_relation_shuttingdown_zero) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_SHUTTINGDOWN) == 0);
}
TEST(sm_relation_drag_vs_spi) {
    UINT w = 0;
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETMOUSEHOVERWIDTH, 0, &w, 0));
    CHECK(gsm(SM_CXDRAG) >= 1 && gsm(SM_CYDRAG) >= 1);
}
TEST(sm_relation_focus_border) {
    UINT w = 0, h = 0;
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETFOCUSBORDERWIDTH, 0, &w, 0));
    CHECK(sW(SPI_GETFOCUSBORDERHEIGHT, 0, &h, 0));
    CHECK((int)w == gsm(SM_CXFOCUSBORDER));
    CHECK((int)h == gsm(SM_CYFOCUSBORDER));
}
TEST(sm_relation_arrange_vs_minimized_metrics) {
    MINIMIZEDMETRICS m;
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(SystemParametersInfoW));
    memset(&m, 0, sizeof m); m.cbSize = sizeof m;
    CHECK(sW(SPI_GETMINIMIZEDMETRICS, sizeof m, &m, 0));
    CHECK((int)m.iArrange == gsm(SM_ARRANGE));
}
TEST(sm_relation_doubleclick_vs_spi) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(gsm(SM_CXDOUBLECLK) >= 1 && gsm(SM_CYDOUBLECLK) >= 1);
    CHECK(gsm(SM_CXDOUBLECLK) < 200);
}

/* Monitors. */
static BOOL CALLBACK count_mon(HMONITOR m, HDC dc, LPRECT r, LPARAM lp) { (*(int *)lp)++; return TRUE; }
TEST(sm_cmonitors_vs_enum) {
    int n = 0;
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(EnumDisplayMonitors));
    CHECK(U(EnumDisplayMonitors)(NULL, NULL, count_mon, (LPARAM)&n));
    CHECK(n >= 1);
    CHECK(gsm(SM_CMONITORS) == n);
}
TEST(sm_primary_matches_monitorinfo) {
    MONITORINFO mi; POINT pt = { 0, 0 }; HMONITOR hm;
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(MonitorFromPoint)); REQUIRE(U(GetMonitorInfoW));
    hm = U(MonitorFromPoint)(pt, MONITOR_DEFAULTTOPRIMARY);
    memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    CHECK(hm && U(GetMonitorInfoW)(hm, &mi));
    CHECK(mi.rcMonitor.left == 0 && mi.rcMonitor.top == 0);
    CHECK(mi.rcMonitor.right == gsm(SM_CXSCREEN));
    CHECK(mi.rcMonitor.bottom == gsm(SM_CYSCREEN));
}
static BOOL CALLBACK union_mon(HMONITOR m, HDC dc, LPRECT r, LPARAM lp) { UnionRect((RECT *)lp, (RECT *)lp, r); return TRUE; }
TEST(sm_virtualscreen_is_union_of_monitors) {
    RECT u = { 0, 0, 0, 0 };
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(EnumDisplayMonitors));
    CHECK(U(EnumDisplayMonitors)(NULL, NULL, union_mon, (LPARAM)&u));
    CHECK(u.left == gsm(SM_XVIRTUALSCREEN));
    CHECK(u.top == gsm(SM_YVIRTUALSCREEN));
    CHECK(u.right - u.left == gsm(SM_CXVIRTUALSCREEN));
    CHECK(u.bottom - u.top == gsm(SM_CYVIRTUALSCREEN));
}
TEST(sm_getdevicecaps_matches_screen) {
    HDC dc; 
    REQUIRE(U(GetSystemMetrics));
    dc = CreateICA("DISPLAY", NULL, NULL, NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    CHECK(GetDeviceCaps(dc, HORZRES) == gsm(SM_CXSCREEN));
    CHECK(GetDeviceCaps(dc, VERTRES) == gsm(SM_CYSCREEN));
    DeleteDC(dc);
}

/* GetSystemMetricsForDpi */
static const UINT dpis[] = { 48, 72, 96, 120, 144, 168, 192, 240, 288, 384, 480 };
static const int scaled_idx[] = { SM_CXVSCROLL, SM_CYHSCROLL, SM_CYCAPTION, SM_CXICON, SM_CYICON, SM_CXSMICON, SM_CYSMICON,
    SM_CYMENU, SM_CXFRAME, SM_CYFRAME, SM_CXSIZE, SM_CYSIZE, SM_CXMINTRACK, SM_CXCURSOR, SM_CYCURSOR, SM_CXHTHUMB,
    SM_CXMENUCHECK, SM_CYMENUCHECK, SM_CYSMCAPTION, SM_CXPADDEDBORDER, SM_CXSMSIZE, SM_CYSMSIZE, SM_CXEDGE, SM_CXBORDER };
TEST(smdpi_monotonic_scaled_metrics) {
    unsigned i, j;
    REQUIRE(U(GetSystemMetricsForDpi));
    for (i = 0; i < sizeof scaled_idx / sizeof *scaled_idx; i++) {
        int prev = -1;
        for (j = 0; j < sizeof dpis / sizeof *dpis; j++) {
            int v = U(GetSystemMetricsForDpi)(scaled_idx[i], dpis[j]);
            CHECKF(v >= prev, "idx %d dpi %u: %d < previous %d", scaled_idx[i], dpis[j], v, prev);
            prev = v;
        }
    }
}
TEST(smdpi_icon_sizes) {
    REQUIRE(U(GetSystemMetricsForDpi));
    CHECK(gsm_dpi(SM_CXICON, 96) == 32 && gsm_dpi(SM_CYICON, 96) == 32);
    CHECK(gsm_dpi(SM_CXSMICON, 96) == 16 && gsm_dpi(SM_CYSMICON, 96) == 16);
    CHECK(gsm_dpi(SM_CXICON, 192) == 64);
    CHECK(gsm_dpi(SM_CXSMICON, 192) == 32);
    CHECK(gsm_dpi(SM_CXICON, 144) == 48);
    CHECK(gsm_dpi(SM_CXSMICON, 144) == 24);
    CHECK(gsm_dpi(SM_CXICON, 120) == 40);
    CHECK(gsm_dpi(SM_CXSMICON, 120) == 20);
}
TEST(smdpi_icon_exact_scaling_table) {
    unsigned j;
    REQUIRE(U(GetSystemMetricsForDpi));
    for (j = 0; j < sizeof dpis / sizeof *dpis; j++) {
        int want = MulDiv(32, dpis[j], 96);
        int got = gsm_dpi(SM_CXICON, dpis[j]);
        CHECKF(got == want, "SM_CXICON@%u = %d want %d", dpis[j], got, want);
        want = MulDiv(16, dpis[j], 96);
        got = gsm_dpi(SM_CXSMICON, dpis[j]);
        CHECKF(got == want, "SM_CXSMICON@%u = %d want %d", dpis[j], got, want);
    }
}
TEST(smdpi_square_icons) {
    unsigned j;
    REQUIRE(U(GetSystemMetricsForDpi));
    for (j = 0; j < sizeof dpis / sizeof *dpis; j++) {
        CHECK(gsm_dpi(SM_CXICON, dpis[j]) == gsm_dpi(SM_CYICON, dpis[j]));
        CHECK(gsm_dpi(SM_CXSMICON, dpis[j]) == gsm_dpi(SM_CYSMICON, dpis[j]));
    }
}
TEST(smdpi_scrollbar_scales) {
    int a, b;
    REQUIRE(U(GetSystemMetricsForDpi));
    a = gsm_dpi(SM_CXVSCROLL, 96); b = gsm_dpi(SM_CXVSCROLL, 192);
    CHECK(a > 0);
    CHECK(b >= a * 2 - 2 && b <= a * 2 + 2);
    CHECK(gsm_dpi(SM_CXVSCROLL, 96) == gsm_dpi(SM_CYHSCROLL, 96));
    CHECK(gsm_dpi(SM_CXVSCROLL, 192) == gsm_dpi(SM_CYHSCROLL, 192));
}
TEST(smdpi_caption_scales) {
    int a, b;
    REQUIRE(U(GetSystemMetricsForDpi));
    a = gsm_dpi(SM_CYCAPTION, 96); b = gsm_dpi(SM_CYCAPTION, 192);
    CHECK(a > 0);
    CHECK(b >= a * 2 - 4 && b <= a * 2 + 4);
}
TEST(smdpi_unscaled_metrics_constant) {
    static const int idx[] = { SM_CMOUSEBUTTONS, SM_MOUSEPRESENT, SM_CMONITORS, SM_SWAPBUTTON, SM_CLEANBOOT, SM_SECURE, SM_DEBUG };
    unsigned i;
    REQUIRE(U(GetSystemMetricsForDpi));
    for (i = 0; i < sizeof idx / sizeof *idx; i++) {
        int a = gsm_dpi(idx[i], 96), b = gsm_dpi(idx[i], 192);
        CHECKF(a == b, "idx %d: %d vs %d", idx[i], a, b);
    }
}
TEST(smdpi_screen_metrics_not_scaled) {
    int a, b;
    REQUIRE(U(GetSystemMetricsForDpi));
    a = gsm_dpi(SM_CXSCREEN, 96); b = gsm_dpi(SM_CXSCREEN, 192);
    t_note("SM_CXSCREEN@96=%d @192=%d", a, b);
    CHECK(a > 0 && b > 0);
}
TEST(smdpi_zero_dpi) {
    int v; DWORD e;
    REQUIRE(U(GetSystemMetricsForDpi));
    SetLastError(0xdeadbeef);
    v = U(GetSystemMetricsForDpi)(SM_CXICON, 0);
    e = GetLastError();
    t_note("GetSystemMetricsForDpi(CXICON,0) = %d err=%lu", v, e);
}
TEST(smdpi_huge_dpi) {
    int v;
    REQUIRE(U(GetSystemMetricsForDpi));
    v = U(GetSystemMetricsForDpi)(SM_CXICON, 0xFFFFFFFF);
    t_note("GetSystemMetricsForDpi(CXICON,-1) = %d", v);
    v = U(GetSystemMetricsForDpi)(SM_CXICON, 1000000);
    t_note("GetSystemMetricsForDpi(CXICON,1000000) = %d", v);
}
TEST(smdpi_repeat_deterministic) {
    int i, bad = 0, first;
    REQUIRE(U(GetSystemMetricsForDpi));
    first = gsm_dpi(SM_CYCAPTION, 144);
    for (i = 0; i < 2000; i++) if (gsm_dpi(SM_CYCAPTION, 144) != first) bad++;
    CHECK(bad == 0);
}
TEST(smdpi_all_indices_no_crash) {
    int i, j;
    REQUIRE(U(GetSystemMetricsForDpi));
    for (i = 0; i < 100; i++) for (j = 0; j < 3; j++) (void)gsm_dpi(i, dpis[j + 2]);
    CHECK(1);
}
TEST(smdpi_matches_system_dpi) {
    UINT sysdpi;
    REQUIRE(U(GetSystemMetricsForDpi)); REQUIRE(U(GetDpiForSystem));
    sysdpi = U(GetDpiForSystem)();
    CHECK(sysdpi >= 96 && sysdpi <= 960);
    CHECK(gsm_dpi(SM_CXICON, sysdpi) == MulDiv(32, sysdpi, 96));
    t_note("system dpi %u: CXVSCROLL=%d vs ForDpi=%d", sysdpi, gsm(SM_CXVSCROLL), gsm_dpi(SM_CXVSCROLL, sysdpi));
}

/* extra: invalid indices and repeat stability */
TEST(sm_invalid_indices_zero_extra) {
    static const int idx[] = { -1, -100, 0x7fffffff, (int)0x80000000, 200, 1000, 65535, 65536 };
    unsigned i;
    REQUIRE(U(GetSystemMetrics));
    for (i = 0; i < sizeof idx / sizeof *idx; i++)
        CHECKF(gsm(idx[i]) == 0, "SM %d nonzero", idx[i]);
}
TEST(sm_stable_repeated) {
    int i, a;
    REQUIRE(U(GetSystemMetrics));
    for (i = 0; i < 100; i++) {
        a = gsm(SM_CXSCREEN);
        CHECK(a == gsm(SM_CXSCREEN) && a > 0);
    }
}
