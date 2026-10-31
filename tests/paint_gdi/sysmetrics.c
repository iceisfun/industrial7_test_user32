#include "pg.h"

#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER 92
#endif
#ifndef SM_CARETBLINKINGENABLED
#define SM_CARETBLINKINGENABLED 0x2002
#endif
#ifndef SM_CONVERTIBLESLATEMODE
#define SM_CONVERTIBLESLATEMODE 0x2003
#endif
#ifndef SM_SYSTEMDOCKED
#define SM_SYSTEMDOCKED 0x2004
#endif
#define GSM U(GetSystemMetrics)

TEST(paint_gdi_sm_screen_positive) {
    REQUIRE(GSM);
    CHECK(GSM(SM_CXSCREEN) > 0);
    CHECK(GSM(SM_CYSCREEN) > 0);
    CHECK(GSM(SM_CXSCREEN) >= 640);
    CHECK(GSM(SM_CYSCREEN) >= 480);
    CHECK(GSM(SM_CXFULLSCREEN) > 0 && GSM(SM_CXFULLSCREEN) <= GSM(SM_CXSCREEN));
    CHECK(GSM(SM_CYFULLSCREEN) > 0 && GSM(SM_CYFULLSCREEN) <= GSM(SM_CYSCREEN));
}
TEST(paint_gdi_sm_virtual_screen) {
    REQUIRE(GSM);
    int vx = GSM(SM_XVIRTUALSCREEN), vy = GSM(SM_YVIRTUALSCREEN);
    int vw = GSM(SM_CXVIRTUALSCREEN), vh = GSM(SM_CYVIRTUALSCREEN);
    CHECK(vw >= GSM(SM_CXSCREEN));
    CHECK(vh >= GSM(SM_CYSCREEN));
    CHECK(vx <= 0 && vy <= 0);    /* the primary monitor origin (0,0) is inside the virtual screen */
    CHECK(vx + vw > 0 && vy + vh > 0);
    CHECK(GSM(SM_CMONITORS) >= 1);
    if (GSM(SM_CMONITORS) == 1) { CHECK(vw == GSM(SM_CXSCREEN)); CHECK(vh == GSM(SM_CYSCREEN)); CHECK(vx == 0 && vy == 0); }
    int same = GSM(SM_SAMEDISPLAYFORMAT);
    CHECK(same == 0 || same == 1);
}
TEST(paint_gdi_sm_work_area_vs_screen) {
    REQUIRE(GSM); REQUIRE(U(SystemParametersInfoW));
    RECT wa = {0};
    CHECK(U(SystemParametersInfoW)(SPI_GETWORKAREA, 0, &wa, 0));
    CHECK(wa.right - wa.left <= GSM(SM_CXSCREEN));
    CHECK(wa.bottom - wa.top <= GSM(SM_CYSCREEN));
    CHECK(wa.right > wa.left && wa.bottom > wa.top);
    CHECK(GSM(SM_CXMAXIMIZED) >= wa.right - wa.left - 16);
    CHECK(GSM(SM_CXFULLSCREEN) <= GSM(SM_CXSCREEN));
}
TEST(paint_gdi_sm_border_edge_relations) {
    REQUIRE(GSM);
    int b = GSM(SM_CXBORDER);
    CHECK(b >= 1);
    CHECK(GSM(SM_CYBORDER) >= 1);
    CHECK(GSM(SM_CXEDGE) == 2 * GSM(SM_CXBORDER));
    CHECK(GSM(SM_CYEDGE) == 2 * GSM(SM_CYBORDER));
    CHECK(GSM(SM_CXFIXEDFRAME) == GSM(SM_CXDLGFRAME));
    CHECK(GSM(SM_CYFIXEDFRAME) == GSM(SM_CYDLGFRAME));
    CHECK(GSM(SM_CXFRAME) == GSM(SM_CXSIZEFRAME));
    CHECK(GSM(SM_CYFRAME) == GSM(SM_CYSIZEFRAME));
    CHECK(GSM(SM_CXDLGFRAME) >= GSM(SM_CXBORDER));
    CHECK(GSM(SM_CXFOCUSBORDER) >= 1);
    CHECK(GSM(SM_CYFOCUSBORDER) >= 1);
}
TEST(paint_gdi_sm_scrollbar_metrics) {
    REQUIRE(GSM);
    CHECK(GSM(SM_CXVSCROLL) > 0);
    CHECK(GSM(SM_CYHSCROLL) > 0);
    CHECK(GSM(SM_CXHSCROLL) > 0);
    CHECK(GSM(SM_CYVSCROLL) > 0);
    if (GSM(SM_CXHSCROLL) != GSM(SM_CXVSCROLL) || GSM(SM_CYVSCROLL) != GSM(SM_CYHSCROLL)) t_note("scrollbar arrow metrics are asymmetric");
    CHECK(GSM(SM_CYVTHUMB) > 0);
    CHECK(GSM(SM_CXHTHUMB) > 0);
    CHECK(GSM(SM_CXVSCROLL) < GSM(SM_CXSCREEN) / 4);
}
TEST(paint_gdi_sm_caption_menu_metrics) {
    REQUIRE(GSM);
    CHECK(GSM(SM_CYCAPTION) > 0);
    CHECK(GSM(SM_CYSMCAPTION) > 0);
    CHECK(GSM(SM_CYMENU) > 0);
    CHECK(GSM(SM_CXSIZE) > 0 && GSM(SM_CYSIZE) > 0);
    CHECK(GSM(SM_CXSMSIZE) > 0 && GSM(SM_CYSMSIZE) > 0);
    CHECK(GSM(SM_CXMENUSIZE) > 0 && GSM(SM_CYMENUSIZE) > 0);
    CHECK(GSM(SM_CXMENUCHECK) > 0 && GSM(SM_CYMENUCHECK) > 0);
    CHECK(GSM(SM_CYSMCAPTION) <= GSM(SM_CYCAPTION));
    CHECK(GSM(SM_CXMINIMIZED) > 0 && GSM(SM_CYMINIMIZED) > 0);
    CHECK(GSM(SM_CXMINSPACING) > 0 && GSM(SM_CYMINSPACING) > 0);
    CHECK(GSM(SM_CXMINSPACING) >= GSM(SM_CXMINIMIZED));
    CHECK(GSM(SM_CYMINSPACING) >= GSM(SM_CYMINIMIZED));
}
TEST(paint_gdi_sm_icon_cursor_metrics) {
    REQUIRE(GSM);
    CHECK(GSM(SM_CXICON) > 0 && GSM(SM_CYICON) > 0);
    CHECK(GSM(SM_CXICON) == GSM(SM_CYICON));
    CHECK(GSM(SM_CXSMICON) > 0 && GSM(SM_CYSMICON) > 0);
    CHECK(GSM(SM_CXSMICON) <= GSM(SM_CXICON));
    CHECK(GSM(SM_CXCURSOR) > 0 && GSM(SM_CYCURSOR) > 0);
    CHECK(GSM(SM_CXICONSPACING) >= GSM(SM_CXICON));
    CHECK(GSM(SM_CYICONSPACING) >= GSM(SM_CYICON));
    CHECK(GSM(SM_CXDOUBLECLK) > 0 && GSM(SM_CYDOUBLECLK) > 0);
    CHECK(GSM(SM_CXDRAG) >= 0 && GSM(SM_CYDRAG) >= 0);
}
TEST(paint_gdi_sm_track_sizes) {
    REQUIRE(GSM);
    CHECK(GSM(SM_CXMIN) > 0 && GSM(SM_CYMIN) > 0);
    CHECK(GSM(SM_CXMINTRACK) > 0 && GSM(SM_CYMINTRACK) > 0);
    CHECK(GSM(SM_CXMAXTRACK) >= GSM(SM_CXSCREEN));
    CHECK(GSM(SM_CYMAXTRACK) >= GSM(SM_CYSCREEN));
    CHECK(GSM(SM_CXMAXIMIZED) >= GSM(SM_CXFULLSCREEN));
    CHECK(GSM(SM_CYMAXIMIZED) >= GSM(SM_CYFULLSCREEN));
    CHECK(GSM(SM_CXMAXTRACK) >= GSM(SM_CXMINTRACK));
    CHECK(GSM(SM_CYMAXTRACK) >= GSM(SM_CYMINTRACK));
}
TEST(paint_gdi_sm_boolean_metrics) {
    REQUIRE(GSM);
    static const int idx[] = { SM_DEBUG, SM_SWAPBUTTON, SM_SECURE, SM_MIDEASTENABLED, SM_IMMENABLED, SM_DBCSENABLED,
        SM_SHOWSOUNDS, SM_REMOTESESSION, SM_REMOTECONTROL, SM_SHUTTINGDOWN, SM_CARETBLINKINGENABLED };
    /* documented as "nonzero if" only: must not be negative */
    static const int nz[] = { SM_MOUSEPRESENT, SM_MOUSEWHEELPRESENT, SM_PENWINDOWS, SM_SLOWMACHINE, SM_TABLETPC, SM_MEDIACENTER, SM_STARTER, SM_SERVERR2 };
    for (unsigned i = 0; i < sizeof nz / sizeof nz[0]; i++) { int v = GSM(nz[i]); CHECKF(v >= 0, "sm %d = %d", nz[i], v); }
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        int v = GSM(idx[i]);
        CHECKF(v == 0 || v == 1, "sm %d = %d", idx[i], v);
    }
    CHECK(GSM(SM_SECURE) == 0);
    CHECK(GSM(SM_DEBUG) == 0);     /* retail Windows */
    CHECK(GSM(SM_REMOTECONTROL) == 0);
    CHECK(GSM(SM_SHUTTINGDOWN) == 0);
}
TEST(paint_gdi_sm_network_cleanboot) {
    REQUIRE(GSM);
    int n = GSM(SM_NETWORK);
    t_note("SM_NETWORK=0x%x", n);
    CHECK(n >= 0);
    int cb = GSM(SM_CLEANBOOT);
    CHECK(cb == 0);   /* normal boot */
}
TEST(paint_gdi_sm_mouse_buttons) {
    REQUIRE(GSM);
    int n = GSM(SM_CMOUSEBUTTONS);
    CHECK(n >= 0 && n <= 32);
    if (GSM(SM_MOUSEPRESENT)) CHECK(n >= 2);
    int sw = GSM(SM_SWAPBUTTON);
    CHECK(sw == 0 || sw == 1);
}
TEST(paint_gdi_sm_digitizer_touch) {
    REQUIRE(GSM);
    int dg = GSM(SM_DIGITIZER);
    CHECK((dg & ~(0x1 | 0x2 | 0x4 | 0x8 | 0x40 | 0x80)) == 0);
    int mt = GSM(SM_MAXIMUMTOUCHES);
    CHECK(mt >= 0 && mt <= 256);
    t_note("SM_DIGITIZER=0x%x SM_MAXIMUMTOUCHES=%d", dg, mt);
}
TEST(paint_gdi_sm_menudropalign) {
    REQUIRE(GSM);
    int v = GSM(SM_MENUDROPALIGNMENT);
    CHECK(v == 0 || v == 1);
}
TEST(paint_gdi_sm_invalid_indices) {
    REQUIRE(GSM);
    static const int bad[] = { -1, -2, -100, 0x7fffffff, (int)0x80000000, 1000, 4095, 0x1001, 0x3000, 0x1801, 0x10000, 255 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        int v = GSM(bad[i]);
        CHECKF(v == 0, "index 0x%x -> %d", bad[i], v);
        t_note("GetSystemMetrics(0x%x) lasterr=%lu", bad[i], GetLastError());
    }
}
TEST(paint_gdi_sm_sweep_stable) {
    REQUIRE(GSM);
    int a[128], b[128];
    for (int i = 0; i < 128; i++) a[i] = GSM(i);
    for (int i = 0; i < 128; i++) b[i] = GSM(i);
    for (int i = 0; i < 128; i++) if (a[i] != b[i]) { CHECKF(0, "sm %d changed %d -> %d", i, a[i], b[i]); }
    CHECK(1);
}
TEST(paint_gdi_sm_unassigned_indices_zero) {
    REQUIRE(GSM);
    /* indices 77..91 are unassigned on Win10 except those documented; 93 too */
    for (int i = 97; i < 200; i++) {
        int v = GSM(i);
        CHECKF(v == 0, "sm %d = %d", i, v);
    }
}
TEST(paint_gdi_sm_cmetrics_boundary) {
    REQUIRE(GSM);
    int m = GSM(SM_CMETRICS);
    t_note("SM_CMETRICS=%d, GetSystemMetrics(SM_CMETRICS)=%d", SM_CMETRICS, m);
    CHECK(GSM(SM_CXPADDEDBORDER) >= 0);
    CHECK(GSM(SM_CXVIRTUALSCREEN) > 0);
}
TEST(paint_gdi_sm_padded_border) {
    REQUIRE(GSM);
    int p = GSM(SM_CXPADDEDBORDER);
    CHECK(p >= 0 && p <= 64);
    /* sizing frame includes the resize border; the padded border is separate */
    CHECK(GSM(SM_CXSIZEFRAME) > 0 && GSM(SM_CYSIZEFRAME) > 0);
    RECT r = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&r, WS_OVERLAPPEDWINDOW, FALSE, 0));
    CHECK(-r.left == r.right - 100);
    CHECK(-r.left >= GSM(SM_CXSIZEFRAME));
}
TEST(paint_gdi_sm_adjust_vs_metrics_dlgframe) {
    REQUIRE(GSM); REQUIRE(U(AdjustWindowRectEx));
    RECT r = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&r, WS_POPUP | WS_DLGFRAME, FALSE, 0));
    CHECK_RECT(r, -GSM(SM_CXFIXEDFRAME), -GSM(SM_CYFIXEDFRAME), 100 + GSM(SM_CXFIXEDFRAME), 100 + GSM(SM_CYFIXEDFRAME));
    RECT b = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&b, WS_POPUP | WS_BORDER, FALSE, 0));
    CHECK_RECT(b, -GSM(SM_CXBORDER), -GSM(SM_CYBORDER), 100 + GSM(SM_CXBORDER), 100 + GSM(SM_CYBORDER));
    RECT e = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&e, WS_POPUP, FALSE, WS_EX_CLIENTEDGE));
    CHECK_RECT(e, -GSM(SM_CXEDGE), -GSM(SM_CYEDGE), 100 + GSM(SM_CXEDGE), 100 + GSM(SM_CYEDGE));
}
TEST(paint_gdi_sm_adjust_caption_vs_metric) {
    REQUIRE(GSM); REQUIRE(U(AdjustWindowRectEx));
    RECT r = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&r, WS_CAPTION, FALSE, 0));
    CHECK(-r.top >= GSM(SM_CYCAPTION));
    RECT m = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&m, WS_POPUP, TRUE, 0));
    EQ(-m.top, GSM(SM_CYMENU));
    RECT tw = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&tw, WS_POPUP | WS_CAPTION, FALSE, WS_EX_TOOLWINDOW));
    t_note("toolwindow caption adj %ld smcaption %d", -tw.top, GSM(SM_CYSMCAPTION));
}
TEST(paint_gdi_sm_window_client_vs_screen) {
    REQUIRE(GSM); REQUIRE(U(GetClientRect)); REQUIRE(U(GetDesktopWindow));
    RECT r;
    HWND d = U(GetDesktopWindow)();
    CHECK(U(GetClientRect)(d, &r));
    t_note("desktop client %ldx%ld screen %dx%d", r.right, r.bottom, GSM(SM_CXSCREEN), GSM(SM_CYSCREEN));
    CHECK(r.right > 0 && r.bottom > 0);
    CHECK(r.right == GSM(SM_CXSCREEN) && r.bottom == GSM(SM_CYSCREEN));
}
TEST(paint_gdi_sm_minimized_window_placement) {
    REQUIRE(GSM); REQUIRE(U(GetWindowRect)); REQUIRE(U(ShowWindow));
    HWND w = pg_window(WS_OVERLAPPEDWINDOW, 0, 10, 10, 200, 200, NULL);
    CHECK(w != NULL);
    if (!w) return;
    RECT mn = {0};
    U(ShowWindow)(w, SW_SHOWMINNOACTIVE);
    WINDOWPLACEMENT wp = { sizeof wp };
    CHECK(U(GetWindowPlacement)(w, &wp));
    CHECK(wp.showCmd == SW_SHOWMINIMIZED);
    U(GetWindowRect)(w, &mn);
    t_note("minimized rect %ldx%ld; SM_CXMINIMIZED=%d", mn.right - mn.left, mn.bottom - mn.top, GSM(SM_CXMINIMIZED));
    U(DestroyWindow)(w);
}
TEST(paint_gdi_sm_tablet_flags_consistency) {
    REQUIRE(GSM);
    int slate = GSM(SM_CONVERTIBLESLATEMODE), dock = GSM(SM_SYSTEMDOCKED);
    CHECK(slate == 0 || slate == 1);
    CHECK(dock == 0 || dock == 1);
    t_note("SM_CONVERTIBLESLATEMODE=%d SM_SYSTEMDOCKED=%d SM_TABLETPC=%d", slate, dock, GSM(SM_TABLETPC));
}
TEST(paint_gdi_sm_remote_session_vs_process) {
    REQUIRE(GSM);
    int r = GSM(SM_REMOTESESSION);
    DWORD sid = 0;
    BOOL ok = ProcessIdToSessionId(GetCurrentProcessId(), &sid);
    CHECK(ok);
    t_note("SM_REMOTESESSION=%d session=%lu", r, sid);
    CHECK(r == 0 || r == 1);
}
TEST(paint_gdi_sm_spi_consistency) {
    REQUIRE(GSM); REQUIRE(U(SystemParametersInfoW));
    NONCLIENTMETRICSW nc; memset(&nc, 0, sizeof nc); nc.cbSize = sizeof nc;
    CHECK(U(SystemParametersInfoW)(SPI_GETNONCLIENTMETRICS, sizeof nc, &nc, 0));
    CHECK(nc.iBorderWidth >= 1);
    CHECK(nc.iScrollWidth == GSM(SM_CXVSCROLL));
    CHECK(nc.iScrollHeight == GSM(SM_CYHSCROLL));
    CHECK(nc.iCaptionWidth == GSM(SM_CXSIZE));
    CHECK(nc.iCaptionHeight == GSM(SM_CYSIZE));
    CHECK(nc.iSmCaptionWidth == GSM(SM_CXSMSIZE));
    CHECK(nc.iSmCaptionHeight == GSM(SM_CYSMSIZE));
    CHECK(nc.iMenuWidth == GSM(SM_CXMENUSIZE));
    CHECK(nc.iMenuHeight == GSM(SM_CYMENUSIZE));
    CHECK(nc.iPaddedBorderWidth == GSM(SM_CXPADDEDBORDER));
}
TEST(paint_gdi_sm_cursor_icon_sizes_vs_spi) {
    REQUIRE(GSM); REQUIRE(U(SystemParametersInfoW));
    ICONMETRICSW im; memset(&im, 0, sizeof im); im.cbSize = sizeof im;
    CHECK(U(SystemParametersInfoW)(SPI_GETICONMETRICS, sizeof im, &im, 0));
    CHECK(im.iHorzSpacing == GSM(SM_CXICONSPACING));
    CHECK(im.iVertSpacing == GSM(SM_CYICONSPACING));
    MINIMIZEDMETRICS mm; memset(&mm, 0, sizeof mm); mm.cbSize = sizeof mm;
    CHECK(U(SystemParametersInfoW)(SPI_GETMINIMIZEDMETRICS, sizeof mm, &mm, 0));
    CHECK(mm.iWidth + mm.iHorzGap == GSM(SM_CXMINSPACING));
    t_note("SM_CYMINSPACING=%d SM_CYMINIMIZED=%d iVertGap=%d", GSM(SM_CYMINSPACING), GSM(SM_CYMINIMIZED), mm.iVertGap);
    CHECK(GSM(SM_CYMINIMIZED) + mm.iVertGap == GSM(SM_CYMINSPACING));
}
TEST(paint_gdi_sm_for_dpi_96_vs_system) {
    REQUIRE(GSM); REQUIRE(U(GetSystemMetricsForDpi)); REQUIRE(U(GetDpiForSystem));
    UINT sys = U(GetDpiForSystem)();
    static const int idx[] = { SM_CXVSCROLL, SM_CYCAPTION, SM_CXICON, SM_CXSMICON, SM_CYMENU, SM_CXSIZEFRAME, SM_CXMENUCHECK };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        int a = GSM(idx[i]), b = U(GetSystemMetricsForDpi)(idx[i], sys);
        CHECKF(a == b, "sm %d: GetSystemMetrics %d vs ForDpi(system dpi %u) %d", idx[i], a, sys, b);
    }
}
TEST(paint_gdi_sm_non_dpi_dependent_for_dpi) {
    REQUIRE(U(GetSystemMetricsForDpi));
    /* flags are not scaled */
    CHECK(U(GetSystemMetricsForDpi)(SM_MOUSEPRESENT, 96) == U(GetSystemMetricsForDpi)(SM_MOUSEPRESENT, 192));
    CHECK(U(GetSystemMetricsForDpi)(SM_CMOUSEBUTTONS, 96) == U(GetSystemMetricsForDpi)(SM_CMOUSEBUTTONS, 192));
    CHECK(U(GetSystemMetricsForDpi)(SM_SWAPBUTTON, 96) == U(GetSystemMetricsForDpi)(SM_SWAPBUTTON, 192));
}
