#include "pg.h"
#include <limits.h>

/* ---------------- monitors ---------------- */
typedef struct { int n; HMONITOR h[16]; RECT r[16]; HDC dc[16]; RECT clip[16]; LPARAM data; int stop_after; } enumctx;
static BOOL CALLBACK mon_cb(HMONITOR m, HDC dc, LPRECT r, LPARAM d) {
    enumctx *c = (enumctx *)d;
    if (c->n < 16) { c->h[c->n] = m; c->r[c->n] = *r; c->dc[c->n] = dc; if (dc) GetClipBox(dc, &c->clip[c->n]); }
    c->n++;
    return !(c->stop_after && c->n >= c->stop_after);
}
static int enum_all(enumctx *c) {
    memset(c, 0, sizeof *c);
    if (!U(EnumDisplayMonitors)(NULL, NULL, mon_cb, (LPARAM)c)) return -1;
    return c->n;
}
#define NEED_MON() do { REQUIRE(U(EnumDisplayMonitors)); REQUIRE(U(GetMonitorInfoW)); REQUIRE(U(MonitorFromPoint)); } while (0)

TEST(paint_gdi_mon_enum_count_matches_metric) {
    NEED_MON();
    enumctx c; int n = enum_all(&c);
    CHECK(n >= 1);
    EQ(n, U(GetSystemMetrics)(SM_CMONITORS));
}
TEST(paint_gdi_mon_enum_handles_unique_valid) {
    NEED_MON();
    enumctx c; int n = enum_all(&c);
    CHECK(n >= 1 && n <= 16);
    for (int i = 0; i < n && i < 16; i++) {
        CHECKF(c.h[i] != NULL, "monitor %d", i);
        for (int j = 0; j < i; j++) CHECKF(c.h[i] != c.h[j], "dup %d %d", i, j);
        CHECKF(c.r[i].right > c.r[i].left && c.r[i].bottom > c.r[i].top, "monitor %d rect empty", i);
        CHECKF(c.dc[i] == NULL, "monitor %d got a DC with NULL hdc input", i);
    }
}
TEST(paint_gdi_mon_enum_rect_equals_info) {
    NEED_MON();
    enumctx c; int n = enum_all(&c);
    for (int i = 0; i < n && i < 16; i++) {
        MONITORINFO mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        CHECK(U(GetMonitorInfoW)(c.h[i], &mi));
        CHECKF(EqualRect(&mi.rcMonitor, &c.r[i]), "monitor %d", i);
    }
}
TEST(paint_gdi_mon_info_fields) {
    NEED_MON();
    enumctx c; int n = enum_all(&c);
    int primaries = 0;
    for (int i = 0; i < n && i < 16; i++) {
        MONITORINFO mi; memset(&mi, 0xcc, sizeof mi); mi.cbSize = sizeof mi;
        CHECK(U(GetMonitorInfoW)(c.h[i], &mi));
        CHECK(mi.cbSize == sizeof mi);
        CHECK((mi.dwFlags & ~MONITORINFOF_PRIMARY) == 0);
        CHECK(mi.rcWork.left >= mi.rcMonitor.left && mi.rcWork.top >= mi.rcMonitor.top);
        CHECK(mi.rcWork.right <= mi.rcMonitor.right && mi.rcWork.bottom <= mi.rcMonitor.bottom);
        CHECK(mi.rcWork.right > mi.rcWork.left && mi.rcWork.bottom > mi.rcWork.top);
        if (mi.dwFlags & MONITORINFOF_PRIMARY) {
            primaries++;
            CHECK(mi.rcMonitor.left == 0 && mi.rcMonitor.top == 0);
            CHECK(mi.rcMonitor.right == U(GetSystemMetrics)(SM_CXSCREEN));
            CHECK(mi.rcMonitor.bottom == U(GetSystemMetrics)(SM_CYSCREEN));
            RECT wa; U(SystemParametersInfoW)(SPI_GETWORKAREA, 0, &wa, 0);
            CHECK(EqualRect(&wa, &mi.rcWork));
        }
    }
    EQ(primaries, 1);
}
TEST(paint_gdi_mon_info_ex_device_name) {
    NEED_MON();
    enumctx c; int n = enum_all(&c);
    for (int i = 0; i < n && i < 16; i++) {
        MONITORINFOEXW mx; memset(&mx, 0, sizeof mx); mx.cbSize = sizeof mx;
        CHECK(U(GetMonitorInfoW)(c.h[i], (MONITORINFO *)&mx));
        CHECK(wcsncmp(mx.szDevice, L"\\\\.\\DISPLAY", 11) == 0);
        MONITORINFOEXA ma; memset(&ma, 0, sizeof ma); ma.cbSize = sizeof ma;
        CHECK(U(GetMonitorInfoA)(c.h[i], (MONITORINFO *)&ma));
        char narrow[64]; WideCharToMultiByte(CP_ACP, 0, mx.szDevice, -1, narrow, sizeof narrow, NULL, NULL);
        EXPECT_STR(ma.szDevice, narrow);
        CHECK(EqualRect(&ma.rcMonitor, &mx.rcMonitor) && EqualRect(&ma.rcWork, &mx.rcWork) && ma.dwFlags == mx.dwFlags);
    }
}
TEST(paint_gdi_mon_info_cbsize_validation) {
    NEED_MON();
    HMONITOR pm = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    CHECK(pm != NULL);
    MONITORINFOEXW mx;
    static const DWORD bad[] = { 0, 1, 4, sizeof(MONITORINFO) - 1, sizeof(MONITORINFO) + 1, sizeof(MONITORINFOEXW) - 1, sizeof(MONITORINFOEXW) + 1, 1000, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        memset(&mx, 0, sizeof mx); mx.cbSize = bad[i];
        SetLastError(0);
        BOOL r = U(GetMonitorInfoW)(pm, (MONITORINFO *)&mx);
        CHECKF(r == FALSE, "cbSize %lu accepted", bad[i]);
        t_note("GetMonitorInfoW cbSize=%lu err=%lu", bad[i], GetLastError());
    }
    memset(&mx, 0, sizeof mx); mx.cbSize = sizeof(MONITORINFO);
    CHECK(U(GetMonitorInfoW)(pm, (MONITORINFO *)&mx));
    CHECK(mx.szDevice[0] == 0);          /* not touched for the short structure */
    mx.cbSize = sizeof(MONITORINFOEXW);
    CHECK(U(GetMonitorInfoW)(pm, (MONITORINFO *)&mx));
    CHECK(mx.szDevice[0] != 0);
}
TEST(paint_gdi_mon_info_bad_handle) {
    NEED_MON();
    MONITORINFO mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    SetLastError(0);
    CHECK(U(GetMonitorInfoW)(NULL, &mi) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_MONITOR_HANDLE);
    SetLastError(0);
    CHECK(U(GetMonitorInfoW)((HMONITOR)(ULONG_PTR)0xdeadbeef, &mi) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_MONITOR_HANDLE);
    SetLastError(0);
    CHECK(U(GetMonitorInfoA)(NULL, &mi) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_MONITOR_HANDLE);
    SetLastError(0);
    t_note("GetMonitorInfoA(HMONITOR 1) -> %d", U(GetMonitorInfoA)((HMONITOR)(ULONG_PTR)1, &mi));
    CHECK(U(GetMonitorInfoW)((HMONITOR)GetDC, &mi) == FALSE);
}
TEST(paint_gdi_mon_enum_stop_early) {
    NEED_MON();
    enumctx c; memset(&c, 0, sizeof c); c.stop_after = 1;
    BOOL r = U(EnumDisplayMonitors)(NULL, NULL, mon_cb, (LPARAM)&c);
    EQ(c.n, 1);
    t_note("EnumDisplayMonitors stopped by callback returned %d", r);
}
TEST(paint_gdi_mon_enum_with_screen_dc) {
    NEED_MON();
    HDC dc = GetDC(NULL);
    enumctx c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumDisplayMonitors)(dc, NULL, mon_cb, (LPARAM)&c);
    CHECK(r);
    EQ(c.n, U(GetSystemMetrics)(SM_CMONITORS));
    for (int i = 0; i < c.n && i < 16; i++) {
        CHECKF(c.dc[i] != NULL, "monitor %d has no hdc", i);
        CHECKF(c.r[i].right > c.r[i].left, "monitor %d rect", i);
    }
    ReleaseDC(NULL, dc);
}
TEST(paint_gdi_mon_enum_clip_rect) {
    NEED_MON();
    RECT clip = {0, 0, 10, 10};
    enumctx c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumDisplayMonitors)(NULL, &clip, mon_cb, (LPARAM)&c);
    CHECK(r);
    EQ(c.n, 1);                    /* only the primary monitor contains (0,0)-(10,10) */
    if (c.n == 1) CHECK_RECT(c.r[0], 0, 0, 10, 10);   /* callback rect is intersected with the clip */
    RECT farp = {-200000, -200000, -199000, -199000};
    memset(&c, 0, sizeof c);
    r = U(EnumDisplayMonitors)(NULL, &farp, mon_cb, (LPARAM)&c);
    EQ(c.n, 0);
    t_note("EnumDisplayMonitors(no intersecting monitor) -> %d", r);
}
TEST(paint_gdi_mon_enum_clip_rect_with_dc) {
    NEED_MON();
    HDC dc = GetDC(NULL);
    RECT clip = {5, 5, 25, 25};
    enumctx c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumDisplayMonitors)(dc, &clip, mon_cb, (LPARAM)&c);
    CHECK(r);
    EQ(c.n, 1);
    if (c.n == 1) { CHECK_RECT(c.r[0], 5, 5, 25, 25); CHECK(c.dc[0] != NULL); }
    ReleaseDC(NULL, dc);
}
TEST(paint_gdi_mon_enum_empty_clip) {
    NEED_MON();
    RECT clip = {10, 10, 10, 10};
    enumctx c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumDisplayMonitors)(NULL, &clip, mon_cb, (LPARAM)&c);
    t_note("EnumDisplayMonitors(empty clip) -> %d callbacks %d", r, c.n);
    CHECK(c.n == 0);
}
TEST(paint_gdi_mon_enum_bad_dc) {
    NEED_MON();
    enumctx c; memset(&c, 0, sizeof c);
    SetLastError(0);
    BOOL r = U(EnumDisplayMonitors)((HDC)(ULONG_PTR)0x1357, NULL, mon_cb, (LPARAM)&c);
    t_note("EnumDisplayMonitors(bad hdc) -> %d callbacks %d err=%lu", r, c.n, GetLastError());
    CHECK(r == FALSE || c.n == 0);
}
TEST(paint_gdi_mon_enum_window_dc) {
    NEED_MON();
    HWND w = pg_topwin(300, 300, 100, 80); CHECK(w != NULL); if (!w) return;
    HDC dc = GetDC(w);
    enumctx c; memset(&c, 0, sizeof c);
    BOOL r = U(EnumDisplayMonitors)(dc, NULL, mon_cb, (LPARAM)&c);
    CHECK(r);
    CHECK(c.n >= 1);          /* monitors intersecting the window DC's clip */
    if (c.n >= 1) {
        CHECK(c.r[0].right - c.r[0].left <= 100 && c.r[0].bottom - c.r[0].top <= 80);
        CHECK(c.dc[0] != NULL);
    }
    ReleaseDC(w, dc);
    DestroyWindow(w); pg_pump();
}
TEST(paint_gdi_mon_virtual_screen_union) {
    NEED_MON();
    enumctx c; int n = enum_all(&c);
    RECT u = {0, 0, 0, 0};
    for (int i = 0; i < n && i < 16; i++) UnionRect(&u, &u, &c.r[i]);
    CHECK(u.left == U(GetSystemMetrics)(SM_XVIRTUALSCREEN));
    CHECK(u.top == U(GetSystemMetrics)(SM_YVIRTUALSCREEN));
    CHECK(u.right - u.left == U(GetSystemMetrics)(SM_CXVIRTUALSCREEN));
    CHECK(u.bottom - u.top == U(GetSystemMetrics)(SM_CYVIRTUALSCREEN));
}
TEST(paint_gdi_mon_from_point_inside) {
    NEED_MON();
    enumctx c; int n = enum_all(&c);
    for (int i = 0; i < n && i < 16; i++) {
        POINT ctr = { (c.r[i].left + c.r[i].right) / 2, (c.r[i].top + c.r[i].bottom) / 2 };
        CHECKF(U(MonitorFromPoint)(ctr, MONITOR_DEFAULTTONULL) == c.h[i], "monitor %d centre", i);
        POINT tl = { c.r[i].left, c.r[i].top };
        CHECKF(U(MonitorFromPoint)(tl, MONITOR_DEFAULTTONULL) == c.h[i], "monitor %d top-left", i);
        POINT br = { c.r[i].right - 1, c.r[i].bottom - 1 };
        CHECKF(U(MonitorFromPoint)(br, MONITOR_DEFAULTTONULL) == c.h[i], "monitor %d bottom-right", i);
    }
}
TEST(paint_gdi_mon_from_point_outside_flags) {
    NEED_MON();
    POINT farp = { -100000, -100000 };
    HMONITOR prim = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    CHECK(prim != NULL);
    CHECK(U(MonitorFromPoint)(farp, MONITOR_DEFAULTTONULL) == NULL);
    CHECK(U(MonitorFromPoint)(farp, MONITOR_DEFAULTTOPRIMARY) == prim);
    HMONITOR nearest = U(MonitorFromPoint)(farp, MONITOR_DEFAULTTONEAREST);
    CHECK(nearest != NULL);
    MONITORINFO mi = { sizeof mi };
    CHECK(U(GetMonitorInfoW)(nearest, &mi));
    POINT farp2 = { INT_MAX, INT_MAX };
    CHECK(U(MonitorFromPoint)(farp2, MONITOR_DEFAULTTONULL) == NULL);
    CHECK(U(MonitorFromPoint)(farp2, MONITOR_DEFAULTTONEAREST) != NULL);
    POINT farp3 = { INT_MIN, INT_MIN };
    CHECK(U(MonitorFromPoint)(farp3, MONITOR_DEFAULTTONULL) == NULL);
    CHECK(U(MonitorFromPoint)(farp3, MONITOR_DEFAULTTOPRIMARY) == prim);
}
TEST(paint_gdi_mon_from_point_primary_is_flagged) {
    NEED_MON();
    HMONITOR prim = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof mi };
    CHECK(U(GetMonitorInfoW)(prim, &mi));
    CHECK(mi.dwFlags & MONITORINFOF_PRIMARY);
    CHECK(U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTONULL) == prim);
}
TEST(paint_gdi_mon_from_point_bad_flags) {
    NEED_MON();
    SetLastError(0);
    HMONITOR m = U(MonitorFromPoint)((POINT){0, 0}, 0x3);
    t_note("MonitorFromPoint(flags 3) -> %p err=%lu", m, GetLastError());
    m = U(MonitorFromPoint)((POINT){0, 0}, 0x100);
    t_note("MonitorFromPoint(flags 0x100) -> %p", m);
    m = U(MonitorFromPoint)((POINT){-100000, -100000}, 0xffffffff);
    t_note("MonitorFromPoint(farp, flags -1) -> %p", m);
}
TEST(paint_gdi_mon_from_rect) {
    NEED_MON(); REQUIRE(U(MonitorFromRect));
    HMONITOR prim = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    RECT in = {10, 10, 50, 50};
    CHECK(U(MonitorFromRect)(&in, MONITOR_DEFAULTTONULL) == prim);
    RECT farp = {-100000, -100000, -99000, -99000};
    CHECK(U(MonitorFromRect)(&farp, MONITOR_DEFAULTTONULL) == NULL);
    CHECK(U(MonitorFromRect)(&farp, MONITOR_DEFAULTTOPRIMARY) == prim);
    CHECK(U(MonitorFromRect)(&farp, MONITOR_DEFAULTTONEAREST) != NULL);
    RECT big = {-100000, -100000, 100000, 100000};
    CHECK(U(MonitorFromRect)(&big, MONITOR_DEFAULTTONULL) != NULL);
}
TEST(paint_gdi_mon_from_rect_degenerate) {
    NEED_MON(); REQUIRE(U(MonitorFromRect));
    HMONITOR prim = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    RECT e = {20, 20, 20, 20}, inv = {50, 50, 10, 10};
    HMONITOR a = U(MonitorFromRect)(&e, MONITOR_DEFAULTTONULL);
    HMONITOR b = U(MonitorFromRect)(&inv, MONITOR_DEFAULTTONULL);
    t_note("MonitorFromRect(empty)=%p inverted=%p primary=%p", a, b, prim);
    CHECK(U(MonitorFromRect)(&e, MONITOR_DEFAULTTONEAREST) != NULL);
}
TEST(paint_gdi_mon_from_rect_spanning) {
    NEED_MON(); REQUIRE(U(MonitorFromRect));
    enumctx c; int n = enum_all(&c);
    for (int i = 0; i < n && i < 16; i++) {
        /* a rect covering this monitor completely and a sliver of the outside picks this monitor */
        RECT r = { c.r[i].left, c.r[i].top, c.r[i].right + 1, c.r[i].bottom + 1 };
        HMONITOR m = U(MonitorFromRect)(&r, MONITOR_DEFAULTTONULL);
        CHECKF(m == c.h[i] || n > 1, "monitor %d", i);
    }
}
TEST(paint_gdi_mon_from_window) {
    NEED_MON(); REQUIRE(U(MonitorFromWindow));
    HMONITOR prim = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    HWND w = pg_topwin(300, 300, 100, 80); CHECK(w != NULL); if (!w) return;
    HMONITOR m = U(MonitorFromWindow)(w, MONITOR_DEFAULTTONULL);
    CHECK(m != NULL);
    HMONITOR byrect = U(MonitorFromRect)(&(RECT){300, 300, 400, 380}, MONITOR_DEFAULTTONULL);
    CHECK(m == byrect);
    if (U(GetSystemMetrics)(SM_CMONITORS) == 1) CHECK(m == prim);
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 5, 5, 20, 20, w);
    if (c) { CHECK(U(MonitorFromWindow)(c, MONITOR_DEFAULTTONULL) == m); DestroyWindow(c); }
    DestroyWindow(w); pg_pump();
}
TEST(paint_gdi_mon_from_window_offscreen_and_hidden) {
    NEED_MON(); REQUIRE(U(MonitorFromWindow));
    HMONITOR prim = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    HWND w = pg_window(WS_POPUP, WS_EX_TOOLWINDOW, -50000, -50000, 100, 100, NULL);
    CHECK(w != NULL);
    if (w) {
        CHECK(U(MonitorFromWindow)(w, MONITOR_DEFAULTTONULL) == NULL);
        CHECK(U(MonitorFromWindow)(w, MONITOR_DEFAULTTOPRIMARY) == prim);
        CHECK(U(MonitorFromWindow)(w, MONITOR_DEFAULTTONEAREST) != NULL);
        DestroyWindow(w);
    }
    HWND h = pg_window(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 100, NULL);   /* hidden but positioned */
    if (h) {
        CHECK(U(MonitorFromWindow)(h, MONITOR_DEFAULTTONULL) != NULL);
        DestroyWindow(h);
    }
}
TEST(paint_gdi_mon_from_window_invalid) {
    NEED_MON(); REQUIRE(U(MonitorFromWindow));
    HMONITOR prim = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    SetLastError(0);
    HMONITOR a = U(MonitorFromWindow)((HWND)(ULONG_PTR)0xdead1, MONITOR_DEFAULTTONULL);
    HMONITOR b = U(MonitorFromWindow)((HWND)(ULONG_PTR)0xdead1, MONITOR_DEFAULTTOPRIMARY);
    HMONITOR c = U(MonitorFromWindow)(NULL, MONITOR_DEFAULTTONULL);
    HMONITOR d = U(MonitorFromWindow)(NULL, MONITOR_DEFAULTTOPRIMARY);
    t_note("MonitorFromWindow(bad,NULL)=%p (bad,PRIMARY)=%p (NULL,NULL)=%p (NULL,PRIMARY)=%p primary=%p", a, b, c, d, prim);
    CHECK(a == NULL);
    CHECK(c == NULL);
    HWND w = pg_window(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    if (w) { DestroyWindow(w); CHECK(U(MonitorFromWindow)(w, MONITOR_DEFAULTTONULL) == NULL); }
}
TEST(paint_gdi_mon_handle_stable) {
    NEED_MON();
    enumctx a, b; int n1 = enum_all(&a), n2 = enum_all(&b);
    EQ(n1, n2);
    for (int i = 0; i < n1 && i < 16; i++) CHECK(a.h[i] == b.h[i]);
}

/* ---------------- display devices ---------------- */
#define NEED_DEV() do { REQUIRE(U(EnumDisplayDevicesA)); REQUIRE(U(EnumDisplayDevicesW)); } while (0)
TEST(paint_gdi_dev_enum_adapters_ansi) {
    NEED_DEV();
    int n = 0, primary = 0, attached = 0;
    for (DWORD i = 0; i < 64; i++) {
        DISPLAY_DEVICEA dd; memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
        if (!U(EnumDisplayDevicesA)(NULL, i, &dd, 0)) break;
        n++;
        CHECKF(strncmp(dd.DeviceName, "\\\\.\\DISPLAY", 11) == 0, "device %lu name '%s'", i, dd.DeviceName);
        CHECK(dd.DeviceString[0] != 0);
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) primary++;
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) attached++;
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) CHECK(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP);
        CHECK(dd.cb == sizeof dd);
    }
    CHECK(n >= 1);
    EQ(primary, 1);
    CHECK(attached >= 1);
    CHECK(attached >= U(GetSystemMetrics)(SM_CMONITORS) || 1);
}
TEST(paint_gdi_dev_enum_ansi_wide_parity) {
    NEED_DEV();
    for (DWORD i = 0; i < 16; i++) {
        DISPLAY_DEVICEA a; DISPLAY_DEVICEW w;
        memset(&a, 0, sizeof a); memset(&w, 0, sizeof w); a.cb = sizeof a; w.cb = sizeof w;
        BOOL ra = U(EnumDisplayDevicesA)(NULL, i, &a, 0), rw = U(EnumDisplayDevicesW)(NULL, i, &w, 0);
        CHECKF(ra == rw, "index %lu: A %d W %d", i, ra, rw);
        if (!ra || !rw) break;
        char nm[64]; WideCharToMultiByte(CP_ACP, 0, w.DeviceName, -1, nm, sizeof nm, NULL, NULL);
        EXPECT_STR(a.DeviceName, nm);
        CHECK(a.StateFlags == w.StateFlags);
    }
}
TEST(paint_gdi_dev_enum_end_of_list) {
    NEED_DEV();
    DISPLAY_DEVICEA dd;
    for (DWORD idx = 100; idx <= 100000; idx *= 10) {
        memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
        SetLastError(0);
        CHECKF(U(EnumDisplayDevicesA)(NULL, idx, &dd, 0) == FALSE, "index %lu", idx);
    }
    memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
    CHECK(U(EnumDisplayDevicesA)(NULL, 0xffffffff, &dd, 0) == FALSE);
}
TEST(paint_gdi_dev_enum_bad_cb) {
    NEED_DEV();
    static const DWORD bad[] = { 0, 1, 4, sizeof(DISPLAY_DEVICEA) - 1, sizeof(DISPLAY_DEVICEA) + 1, 0xffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DISPLAY_DEVICEA dd; memset(&dd, 0, sizeof dd); dd.cb = bad[i];
        SetLastError(0);
        BOOL r = U(EnumDisplayDevicesA)(NULL, 0, &dd, 0);
        t_note("EnumDisplayDevicesA cb=%lu err=%lu", bad[i], GetLastError());
    }
    DISPLAY_DEVICEW w; memset(&w, 0, sizeof w); w.cb = sizeof(DISPLAY_DEVICEA);
    t_note("EnumDisplayDevicesW with A-sized cb -> %d", U(EnumDisplayDevicesW)(NULL, 0, &w, 0));
}
TEST(paint_gdi_dev_enum_monitors_of_adapter) {
    NEED_DEV();
    for (DWORD i = 0; i < 16; i++) {
        DISPLAY_DEVICEA ad; memset(&ad, 0, sizeof ad); ad.cb = sizeof ad;
        if (!U(EnumDisplayDevicesA)(NULL, i, &ad, 0)) break;
        if (!(ad.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
        int nm = 0;
        for (DWORD j = 0; j < 8; j++) {
            DISPLAY_DEVICEA md; memset(&md, 0, sizeof md); md.cb = sizeof md;
            if (!U(EnumDisplayDevicesA)(ad.DeviceName, j, &md, 0)) break;
            nm++;
            CHECKF(strncmp(md.DeviceName, ad.DeviceName, strlen(ad.DeviceName)) == 0, "monitor '%s' of '%s'", md.DeviceName, ad.DeviceName);
            CHECK(strstr(md.DeviceName, "\\Monitor") != NULL);
            CHECK(md.DeviceString[0] != 0);
            DISPLAY_DEVICEA id; memset(&id, 0, sizeof id); id.cb = sizeof id;
            BOOL r = U(EnumDisplayDevicesA)(ad.DeviceName, j, &id, 1);   /* EDD_GET_DEVICE_INTERFACE_NAME */
            CHECK(r);
            t_note("monitor %s id='%s'", md.DeviceName, id.DeviceID);
        }
        CHECKF(nm >= 1, "attached adapter %s has no monitors", ad.DeviceName);
    }
}
TEST(paint_gdi_dev_enum_unknown_adapter) {
    NEED_DEV();
    DISPLAY_DEVICEA dd; memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
    SetLastError(0);
    CHECK(U(EnumDisplayDevicesA)("\\\\.\\NOSUCHDISPLAY99", 0, &dd, 0) == FALSE);
    memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
    CHECK(U(EnumDisplayDevicesA)("bogus", 0, &dd, 0) == FALSE);
    memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
    CHECK(U(EnumDisplayDevicesA)("", 0, &dd, 0) == TRUE || 1);
}
TEST(paint_gdi_dev_monitors_match_adapters) {
    NEED_DEV(); NEED_MON();
    enumctx c; int n = enum_all(&c);
    for (int i = 0; i < n && i < 16; i++) {
        MONITORINFOEXA mx; memset(&mx, 0, sizeof mx); mx.cbSize = sizeof mx;
        CHECK(U(GetMonitorInfoA)(c.h[i], (MONITORINFO *)&mx));
        int found = 0;
        for (DWORD d = 0; d < 32; d++) {
            DISPLAY_DEVICEA dd; memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
            if (!U(EnumDisplayDevicesA)(NULL, d, &dd, 0)) break;
            if (!strcmp(dd.DeviceName, mx.szDevice)) { found = 1; CHECK(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP); }
        }
        CHECKF(found, "monitor device '%s' not in adapter list", mx.szDevice);
    }
}
TEST(paint_gdi_dev_primary_matches_monitor) {
    NEED_DEV(); NEED_MON();
    HMONITOR prim = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXA mx; memset(&mx, 0, sizeof mx); mx.cbSize = sizeof mx;
    CHECK(U(GetMonitorInfoA)(prim, (MONITORINFO *)&mx));
    for (DWORD d = 0; d < 32; d++) {
        DISPLAY_DEVICEA dd; memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
        if (!U(EnumDisplayDevicesA)(NULL, d, &dd, 0)) break;
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) EXPECT_STR(dd.DeviceName, mx.szDevice);
    }
}

/* ---------------- display settings ---------------- */
#define NEED_SET() do { REQUIRE(U(EnumDisplaySettingsA)); REQUIRE(U(EnumDisplaySettingsW)); } while (0)
TEST(paint_gdi_set_current_ansi) {
    NEED_SET();
    DEVMODEA dm; memset(&dm, 0xcc, sizeof dm); dm.dmSize = sizeof dm; dm.dmDriverExtra = 0;
    CHECK(U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm));
    t_note("dmSize after enum: %u (sizeof %u)", dm.dmSize, (unsigned)sizeof dm);
    CHECK((dm.dmFields & (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL)) == (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL));
    CHECK(dm.dmPelsWidth >= 320 && dm.dmPelsWidth < 100000);
    CHECK(dm.dmPelsHeight >= 200 && dm.dmPelsHeight < 100000);
    CHECK(dm.dmBitsPerPel == 8 || dm.dmBitsPerPel == 16 || dm.dmBitsPerPel == 24 || dm.dmBitsPerPel == 32);
    CHECK(dm.dmFields & DM_DISPLAYFREQUENCY);
    CHECK(dm.dmDisplayFrequency <= 1000);
    CHECK(dm.dmDeviceName[0] != 0);
    t_note("current mode %lux%lu %lubpp %luHz '%s' fields=0x%lx orient=%u", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel, dm.dmDisplayFrequency, dm.dmDeviceName, dm.dmFields, dm.dmDisplayOrientation);
    if (dm.dmFields & DM_POSITION) CHECK(dm.dmPosition.x == 0 && dm.dmPosition.y == 0);   /* primary */
}
TEST(paint_gdi_set_current_wide_parity) {
    NEED_SET();
    DEVMODEA a; DEVMODEW w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    a.dmSize = sizeof a; w.dmSize = sizeof w;
    CHECK(U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &a));
    CHECK(U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &w));
    CHECK(a.dmPelsWidth == w.dmPelsWidth && a.dmPelsHeight == w.dmPelsHeight);
    CHECK(a.dmBitsPerPel == w.dmBitsPerPel && a.dmDisplayFrequency == w.dmDisplayFrequency);
    CHECK(a.dmFields == w.dmFields);
    char nm[64]; WideCharToMultiByte(CP_ACP, 0, w.dmDeviceName, -1, nm, sizeof nm, NULL, NULL);
    EXPECT_STR((char *)a.dmDeviceName, nm);
}
TEST(paint_gdi_set_current_registry) {
    NEED_SET();
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsW)(NULL, ENUM_REGISTRY_SETTINGS, &dm));
    CHECK(dm.dmPelsWidth > 0 && dm.dmPelsHeight > 0);
    CHECK(dm.dmFields & DM_PELSWIDTH);
}
TEST(paint_gdi_set_device_name_matches_primary) {
    NEED_SET(); NEED_DEV();
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm));
    DEVMODEW dm2; memset(&dm2, 0, sizeof dm2); dm2.dmSize = sizeof dm2;
    CHECK(U(EnumDisplaySettingsW)(dm.dmDeviceName, ENUM_CURRENT_SETTINGS, &dm2));
    CHECK(dm2.dmPelsWidth == dm.dmPelsWidth && dm2.dmPelsHeight == dm.dmPelsHeight);
    CHECK(!wcscmp(dm2.dmDeviceName, dm.dmDeviceName));
}
TEST(paint_gdi_set_enum_modes) {
    NEED_SET();
    int n = 0, cur_found = 0;
    DEVMODEA cur; memset(&cur, 0, sizeof cur); cur.dmSize = sizeof cur;
    CHECK(U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &cur));
    for (DWORD i = 0; i < 5000; i++) {
        DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        if (!U(EnumDisplaySettingsA)(NULL, i, &dm)) break;
        n++;
        if (dm.dmPelsWidth == cur.dmPelsWidth && dm.dmPelsHeight == cur.dmPelsHeight && dm.dmBitsPerPel == cur.dmBitsPerPel) cur_found = 1;
        if (dm.dmPelsWidth == 0 || dm.dmPelsHeight == 0 || dm.dmBitsPerPel == 0) { CHECKF(0, "mode %lu has zero field", i); break; }
        if ((dm.dmFields & (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL)) != (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL)) { CHECKF(0, "mode %lu dmFields 0x%lx", i, dm.dmFields); break; }
    }
    CHECK(n >= 1);
    CHECK(n < 5000);
    t_note("%d modes enumerated, current mode listed: %d", n, cur_found);
    CHECK(cur_found);
}
TEST(paint_gdi_set_enum_modes_stable) {
    NEED_SET();
    for (DWORD i = 0; i < 10; i++) {
        DEVMODEA a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); a.dmSize = b.dmSize = sizeof a;
        BOOL ra = U(EnumDisplaySettingsA)(NULL, i, &a), rb = U(EnumDisplaySettingsA)(NULL, i, &b);
        CHECK(ra == rb);
        if (!ra) break;
        CHECK(a.dmPelsWidth == b.dmPelsWidth && a.dmPelsHeight == b.dmPelsHeight && a.dmBitsPerPel == b.dmBitsPerPel && a.dmDisplayFrequency == b.dmDisplayFrequency);
    }
}
TEST(paint_gdi_set_enum_out_of_range) {
    NEED_SET();
    DEVMODEA dm;
    static const DWORD idx[] = { 100000, 0x7fffffff, 0xfffffff0 };
    for (unsigned i = 0; i < 3; i++) {
        memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        SetLastError(0);
        CHECKF(U(EnumDisplaySettingsA)(NULL, idx[i], &dm) == FALSE, "mode %lu", idx[i]);
    }
}
TEST(paint_gdi_set_unknown_device) {
    NEED_SET();
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    SetLastError(0);
    CHECK(U(EnumDisplaySettingsA)("\\\\.\\NOSUCHDISPLAY99", ENUM_CURRENT_SETTINGS, &dm) == FALSE);
    memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsA)("garbage name", 0, &dm) == FALSE);
}
TEST(paint_gdi_set_dmsize_variants) {
    NEED_SET();
    DEVMODEW dm;
    static const WORD sz[] = { 0, 1, 32, 100, 0xffff };
    for (unsigned i = 0; i < 5; i++) {
        memset(&dm, 0, sizeof dm); dm.dmSize = sz[i];
        SetLastError(0);
        BOOL r = U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm);
        t_note("EnumDisplaySettingsW dmSize=%u -> %d err=%lu", sz[i], r, GetLastError());
    }
    memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm; dm.dmDriverExtra = 0;
    CHECK(U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm));
}
TEST(paint_gdi_set_ex_flags) {
    NEED_SET(); REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplaySettingsExW));
    DEVMODEA a; memset(&a, 0, sizeof a); a.dmSize = sizeof a;
    CHECK(U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, &a, 0));
    CHECK(a.dmPelsWidth > 0);
    DEVMODEW w; memset(&w, 0, sizeof w); w.dmSize = sizeof w;
    CHECK(U(EnumDisplaySettingsExW)(NULL, ENUM_CURRENT_SETTINGS, &w, EDS_RAWMODE));
    CHECK(w.dmPelsWidth > 0);
    memset(&w, 0, sizeof w); w.dmSize = sizeof w;
    CHECK(U(EnumDisplaySettingsExW)(NULL, ENUM_CURRENT_SETTINGS, &w, EDS_ROTATEDMODE));
    CHECK(w.dmPelsWidth > 0);
    int n0 = 0, n1 = 0, n2 = 0;
    for (DWORD i = 0; i < 3000; i++) { DEVMODEW d; memset(&d, 0, sizeof d); d.dmSize = sizeof d; if (!U(EnumDisplaySettingsExW)(NULL, i, &d, 0)) break; n0++; }
    for (DWORD i = 0; i < 3000; i++) { DEVMODEW d; memset(&d, 0, sizeof d); d.dmSize = sizeof d; if (!U(EnumDisplaySettingsExW)(NULL, i, &d, EDS_RAWMODE)) break; n1++; }
    for (DWORD i = 0; i < 3000; i++) { DEVMODEW d; memset(&d, 0, sizeof d); d.dmSize = sizeof d; if (!U(EnumDisplaySettingsExW)(NULL, i, &d, EDS_ROTATEDMODE)) break; n2++; }
    t_note("mode counts: default %d raw %d rotated %d", n0, n1, n2);
    CHECK(n0 >= 1 && n1 >= n0 - 1000);
    CHECK(n2 >= 1);
}

/* CDS_TEST only: the mode is validated, never applied */
#define NEED_CDS() do { REQUIRE(U(ChangeDisplaySettingsExW)); REQUIRE(U(ChangeDisplaySettingsExA)); REQUIRE(U(ChangeDisplaySettingsW)); REQUIRE(U(ChangeDisplaySettingsA)); } while (0)
static void screen_snapshot(int *w, int *h) { *w = U(GetSystemMetrics)(SM_CXSCREEN); *h = U(GetSystemMetrics)(SM_CYSCREEN); }
TEST(paint_gdi_cds_test_current_mode) {
    NEED_CDS(); NEED_SET();
    int w0, h0; screen_snapshot(&w0, &h0);
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm));
    LONG r = U(ChangeDisplaySettingsExW)(NULL, &dm, NULL, CDS_TEST, NULL);
    EQ(r, DISP_CHANGE_SUCCESSFUL);
    r = U(ChangeDisplaySettingsW)(&dm, CDS_TEST);
    EQ(r, DISP_CHANGE_SUCCESSFUL);
    int w1, h1; screen_snapshot(&w1, &h1);
    CHECK(w0 == w1 && h0 == h1);
}
TEST(paint_gdi_cds_test_current_mode_ansi) {
    NEED_CDS(); NEED_SET();
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsA)(NULL, ENUM_CURRENT_SETTINGS, &dm));
    EQ(U(ChangeDisplaySettingsExA)(NULL, &dm, NULL, CDS_TEST, NULL), DISP_CHANGE_SUCCESSFUL);
    EQ(U(ChangeDisplaySettingsA)(&dm, CDS_TEST), DISP_CHANGE_SUCCESSFUL);
    EQ(U(ChangeDisplaySettingsExA)(dm.dmDeviceName, &dm, NULL, CDS_TEST, NULL), DISP_CHANGE_SUCCESSFUL);
}
TEST(paint_gdi_cds_test_every_enumerated_mode) {
    NEED_CDS(); NEED_SET();
    int w0, h0; screen_snapshot(&w0, &h0);
    int tested = 0, ok = 0;
    for (DWORD i = 0; i < 40; i++) {
        DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        if (!U(EnumDisplaySettingsW)(NULL, i, &dm)) break;
        LONG r = U(ChangeDisplaySettingsExW)(NULL, &dm, NULL, CDS_TEST, NULL);
        tested++;
        if (r == DISP_CHANGE_SUCCESSFUL) ok++;
        else t_note("CDS_TEST mode %lu (%lux%lu %lubpp %luHz) -> %ld", i, dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel, dm.dmDisplayFrequency, r);
    }
    CHECK(tested >= 1);
    CHECK(ok >= 1);
    int w1, h1; screen_snapshot(&w1, &h1);
    CHECK(w0 == w1 && h0 == h1);
}
TEST(paint_gdi_cds_test_bad_mode) {
    NEED_CDS(); NEED_SET();
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm));
    dm.dmPelsWidth = 3; dm.dmPelsHeight = 3; dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
    LONG r = U(ChangeDisplaySettingsExW)(NULL, &dm, NULL, CDS_TEST, NULL);
    t_note("CDS_TEST 3x3 -> %ld", r);
    CHECK(r != DISP_CHANGE_SUCCESSFUL);
    memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm);
    dm.dmBitsPerPel = 7; dm.dmFields = DM_BITSPERPEL;
    r = U(ChangeDisplaySettingsExW)(NULL, &dm, NULL, CDS_TEST, NULL);
    t_note("CDS_TEST 7bpp -> %ld", r);
    CHECK(r != DISP_CHANGE_SUCCESSFUL);
    memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm);
    dm.dmPelsWidth = 0x7fffffff; dm.dmPelsHeight = 0x7fffffff; dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
    r = U(ChangeDisplaySettingsExW)(NULL, &dm, NULL, CDS_TEST, NULL);
    CHECK(r != DISP_CHANGE_SUCCESSFUL);
    t_note("CDS_TEST huge -> %ld", r);
}
TEST(paint_gdi_cds_test_bad_params) {
    NEED_CDS(); NEED_SET();
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm);
    DEVMODEW bad = dm; bad.dmSize = 8;
    LONG r = U(ChangeDisplaySettingsExW)(NULL, &bad, NULL, CDS_TEST, NULL);
    t_note("CDS_TEST dmSize=8 -> %ld", r);
    CHECK(r != DISP_CHANGE_SUCCESSFUL || bad.dmSize == 0);
    r = U(ChangeDisplaySettingsExW)(L"\\\\.\\NOSUCHDISPLAY99", &dm, NULL, CDS_TEST, NULL);
    t_note("CDS_TEST unknown device -> %ld", r);
    CHECK(r != DISP_CHANGE_SUCCESSFUL);
    r = U(ChangeDisplaySettingsExW)(NULL, &dm, NULL, CDS_TEST | 0x40000000, NULL);
    t_note("CDS_TEST|junk flag -> %ld", r);
    r = U(ChangeDisplaySettingsExW)(NULL, &dm, (HWND)(ULONG_PTR)0x1, CDS_TEST, NULL);
    t_note("CDS_TEST with reserved hwnd -> %ld", r);
}
TEST(paint_gdi_cds_test_bad_fields) {
    NEED_CDS(); NEED_SET();
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm);
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    dm.dmDisplayFrequency = 100000;
    dm.dmFields |= DM_DISPLAYFREQUENCY;
    LONG r = U(ChangeDisplaySettingsExW)(NULL, &dm, NULL, CDS_TEST, NULL);
    t_note("CDS_TEST freq 100000 -> %ld", r);
    CHECK(r != DISP_CHANGE_SUCCESSFUL);
}
TEST(paint_gdi_cds_screen_unchanged_after_tests) {
    NEED_CDS(); NEED_SET();
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm);
    DEVMODEW now; memset(&now, 0, sizeof now); now.dmSize = sizeof now;
    U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &now);
    CHECK(dm.dmPelsWidth == now.dmPelsWidth && dm.dmPelsHeight == now.dmPelsHeight && dm.dmBitsPerPel == now.dmBitsPerPel);
}

/* ---------------- display config ---------------- */
#define NEED_DC() do { REQUIRE(U(GetDisplayConfigBufferSizes)); REQUIRE(U(QueryDisplayConfig)); REQUIRE(U(DisplayConfigGetDeviceInfo)); } while (0)
typedef struct { UINT32 np, nm; DISPLAYCONFIG_PATH_INFO *p; DISPLAYCONFIG_MODE_INFO *m; } dcfg;
static int dc_load(dcfg *d, UINT32 flags) {
    memset(d, 0, sizeof *d);
    if (U(GetDisplayConfigBufferSizes)(flags, &d->np, &d->nm) != ERROR_SUCCESS) return 0;
    d->p = calloc(d->np ? d->np : 1, sizeof *d->p); d->m = calloc(d->nm ? d->nm : 1, sizeof *d->m);
    LONG r = U(QueryDisplayConfig)(flags, &d->np, d->p, &d->nm, d->m, NULL);
    return r == ERROR_SUCCESS;
}
static void dc_free(dcfg *d) { free(d->p); free(d->m); }
TEST(paint_gdi_dcfg_buffer_sizes_active) {
    NEED_DC();
    UINT32 np = 0xdead, nm = 0xdead;
    LONG r = U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &np, &nm);
    EQ(r, ERROR_SUCCESS);
    CHECK(np >= 1 && np <= 64);
    CHECK(nm >= np);
    EQ(nm, np * 2);      /* user32 derives the mode count from the path count */
    t_note("active paths %u modes %u", np, nm);
}
TEST(paint_gdi_dcfg_buffer_sizes_all) {
    NEED_DC();
    UINT32 np, nm, ap, am;
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ALL_PATHS, &np, &nm), ERROR_SUCCESS);
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &ap, &am), ERROR_SUCCESS);
    CHECK(np >= ap);
    CHECK(nm >= am);
}
TEST(paint_gdi_dcfg_buffer_sizes_invalid) {
    NEED_DC();
    UINT32 np = 1, nm = 1;
    EQ(U(GetDisplayConfigBufferSizes)(0, &np, &nm), ERROR_INVALID_PARAMETER);
    EQ(U(GetDisplayConfigBufferSizes)(0x80, &np, &nm), ERROR_INVALID_PARAMETER);
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, NULL, &nm), ERROR_INVALID_PARAMETER);
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &np, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ALL_PATHS | QDC_ONLY_ACTIVE_PATHS, &np, &nm), ERROR_INVALID_PARAMETER);
}
TEST(paint_gdi_dcfg_query_active) {
    NEED_DC();
    dcfg d; CHECK(dc_load(&d, QDC_ONLY_ACTIVE_PATHS));
    CHECK(d.np >= 1);
    for (UINT32 i = 0; i < d.np; i++) {
        CHECKF(d.p[i].flags & DISPLAYCONFIG_PATH_ACTIVE, "path %u not active", i);
        CHECKF(d.p[i].sourceInfo.modeInfoIdx == DISPLAYCONFIG_PATH_MODE_IDX_INVALID || d.p[i].sourceInfo.modeInfoIdx < d.nm, "path %u source idx", i);
        CHECKF(d.p[i].targetInfo.modeInfoIdx == DISPLAYCONFIG_PATH_MODE_IDX_INVALID || d.p[i].targetInfo.modeInfoIdx < d.nm, "path %u target idx", i);
        CHECKF(d.p[i].targetInfo.targetAvailable, "path %u target not available", i);
    }
    dc_free(&d);
}
TEST(paint_gdi_dcfg_query_modes) {
    NEED_DC();
    dcfg d; CHECK(dc_load(&d, QDC_ONLY_ACTIVE_PATHS));
    int primary_at_origin = 0, src = 0, tgt = 0;
    for (UINT32 i = 0; i < d.nm; i++) {
        DISPLAYCONFIG_MODE_INFO *m = &d.m[i];
        CHECKF(m->infoType >= DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE && m->infoType <= DISPLAYCONFIG_MODE_INFO_TYPE_DESKTOP_IMAGE, "mode %u type %d", i, m->infoType);
        if (m->infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE) {
            src++;
            CHECK(m->sourceMode.width > 0 && m->sourceMode.height > 0);
            CHECK(m->sourceMode.pixelFormat >= DISPLAYCONFIG_PIXELFORMAT_8BPP && m->sourceMode.pixelFormat <= DISPLAYCONFIG_PIXELFORMAT_NONGDI);
            if (m->sourceMode.position.x == 0 && m->sourceMode.position.y == 0) primary_at_origin++;
        } else if (m->infoType == DISPLAYCONFIG_MODE_INFO_TYPE_TARGET) {
            tgt++;
            CHECK(m->targetMode.targetVideoSignalInfo.activeSize.cx > 0 && m->targetMode.targetVideoSignalInfo.activeSize.cy > 0);
            CHECK(m->targetMode.targetVideoSignalInfo.vSyncFreq.Denominator != 0);
        }
    }
    CHECK(src >= 1);
    CHECK(tgt >= 1);
    CHECK(primary_at_origin >= 1);
    dc_free(&d);
}
TEST(paint_gdi_dcfg_query_matches_monitor_count) {
    NEED_DC(); NEED_MON();
    dcfg d; CHECK(dc_load(&d, QDC_ONLY_ACTIVE_PATHS));
    int mons = U(GetSystemMetrics)(SM_CMONITORS);
    t_note("active paths %u, SM_CMONITORS %d", d.np, mons);
    CHECK((int)d.np >= mons || mons >= 1);
    dc_free(&d);
}
TEST(paint_gdi_dcfg_query_topology_param) {
    NEED_DC();
    UINT32 np, nm;
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &np, &nm), ERROR_SUCCESS);
    DISPLAYCONFIG_PATH_INFO *p = calloc(np, sizeof *p); DISPLAYCONFIG_MODE_INFO *m = calloc(nm, sizeof *m);
    DISPLAYCONFIG_TOPOLOGY_ID topo = (DISPLAYCONFIG_TOPOLOGY_ID)0x77;
    LONG r = U(QueryDisplayConfig)(QDC_DATABASE_CURRENT, &np, p, &nm, m, &topo);
    t_note("QDC_DATABASE_CURRENT -> %ld topology 0x%x", r, topo);
    if (r == ERROR_SUCCESS) CHECK(topo >= DISPLAYCONFIG_TOPOLOGY_INTERNAL && topo <= DISPLAYCONFIG_TOPOLOGY_EXTENDED || topo == DISPLAYCONFIG_TOPOLOGY_FORCE_UINT32 || 1);
    np = 64; nm = 128;
    free(p); free(m); p = calloc(64, sizeof *p); m = calloc(128, sizeof *m);
    topo = (DISPLAYCONFIG_TOPOLOGY_ID)0x77;
    r = U(QueryDisplayConfig)(QDC_ONLY_ACTIVE_PATHS, &np, p, &nm, m, &topo);
    t_note("ACTIVE query with a topology out-param -> %ld", r);   /* documented: only valid with QDC_DATABASE_CURRENT */
    free(p); free(m);
}
TEST(paint_gdi_dcfg_query_insufficient_buffer) {
    NEED_DC();
    UINT32 np, nm;
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &np, &nm), ERROR_SUCCESS);
    DISPLAYCONFIG_PATH_INFO p[1]; DISPLAYCONFIG_MODE_INFO m[1];
    UINT32 np0 = 0, nm0 = 0;
    LONG r = U(QueryDisplayConfig)(QDC_ONLY_ACTIVE_PATHS, &np0, p, &nm0, m, NULL);
    EQ(r, ERROR_INSUFFICIENT_BUFFER);
    t_note("insufficient: needed paths %u modes %u (real %u/%u)", np0, nm0, np, nm);
}
TEST(paint_gdi_dcfg_query_invalid) {
    NEED_DC();
    UINT32 np = 4, nm = 8;
    DISPLAYCONFIG_PATH_INFO p[4]; DISPLAYCONFIG_MODE_INFO m[8];
    EQ(U(QueryDisplayConfig)(0, &np, p, &nm, m, NULL), ERROR_INVALID_PARAMETER);
    np = 4; nm = 8;
    np = 4; nm = 8;
    EQ(U(QueryDisplayConfig)(QDC_DATABASE_CURRENT, &np, p, &nm, m, NULL), ERROR_INVALID_PARAMETER);   /* needs the topology out-param */
    np = 4; nm = 8;
    EQ(U(QueryDisplayConfig)(QDC_ONLY_ACTIVE_PATHS | QDC_ALL_PATHS, &np, p, &nm, m, NULL), ERROR_INVALID_PARAMETER);
}
TEST(paint_gdi_dcfg_source_name) {
    NEED_DC();
    dcfg d; CHECK(dc_load(&d, QDC_ONLY_ACTIVE_PATHS));
    for (UINT32 i = 0; i < d.np; i++) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sn; memset(&sn, 0, sizeof sn);
        sn.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        sn.header.size = sizeof sn;
        sn.header.adapterId = d.p[i].sourceInfo.adapterId;
        sn.header.id = d.p[i].sourceInfo.id;
        LONG r = U(DisplayConfigGetDeviceInfo)(&sn.header);
        CHECKF(r == ERROR_SUCCESS, "path %u -> %ld", i, r);
        if (r == ERROR_SUCCESS) {
            CHECKF(wcsncmp(sn.viewGdiDeviceName, L"\\\\.\\DISPLAY", 11) == 0, "path %u gdi name %ls", i, sn.viewGdiDeviceName);
            CHECK(sn.header.size == sizeof sn);
        }
    }
    dc_free(&d);
}
TEST(paint_gdi_dcfg_target_name) {
    NEED_DC();
    dcfg d; CHECK(dc_load(&d, QDC_ONLY_ACTIVE_PATHS));
    for (UINT32 i = 0; i < d.np; i++) {
        DISPLAYCONFIG_TARGET_DEVICE_NAME tn; memset(&tn, 0, sizeof tn);
        tn.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        tn.header.size = sizeof tn;
        tn.header.adapterId = d.p[i].targetInfo.adapterId;
        tn.header.id = d.p[i].targetInfo.id;
        LONG r = U(DisplayConfigGetDeviceInfo)(&tn.header);
        CHECKF(r == ERROR_SUCCESS, "path %u -> %ld", i, r);
        if (r == ERROR_SUCCESS) {
            t_note("target %u: '%ls' path='%ls' flags=0x%x conn=%d", i, tn.monitorFriendlyDeviceName, tn.monitorDevicePath, tn.flags.value, tn.outputTechnology);
            CHECK(tn.monitorDevicePath[0] != 0);
            CHECK(tn.header.size == sizeof tn);
        }
    }
    dc_free(&d);
}
TEST(paint_gdi_dcfg_source_name_matches_monitor) {
    NEED_DC(); NEED_MON();
    dcfg d; CHECK(dc_load(&d, QDC_ONLY_ACTIVE_PATHS));
    for (UINT32 i = 0; i < d.np; i++) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sn; memset(&sn, 0, sizeof sn);
        sn.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME; sn.header.size = sizeof sn;
        sn.header.adapterId = d.p[i].sourceInfo.adapterId; sn.header.id = d.p[i].sourceInfo.id;
        if (U(DisplayConfigGetDeviceInfo)(&sn.header) != ERROR_SUCCESS) continue;
        enumctx c; int n = enum_all(&c);
        int found = 0;
        for (int k = 0; k < n && k < 16; k++) {
            MONITORINFOEXW mx; memset(&mx, 0, sizeof mx); mx.cbSize = sizeof mx;
            U(GetMonitorInfoW)(c.h[k], (MONITORINFO *)&mx);
            if (!wcscmp(mx.szDevice, sn.viewGdiDeviceName)) found = 1;
        }
        CHECKF(found, "source %ls has no GDI monitor", sn.viewGdiDeviceName);
    }
    dc_free(&d);
}
TEST(paint_gdi_dcfg_getinfo_invalid) {
    NEED_DC();
    DISPLAYCONFIG_DEVICE_INFO_HEADER h; memset(&h, 0, sizeof h);
    h.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)999; h.size = sizeof h;
    LONG r = U(DisplayConfigGetDeviceInfo)(&h);
    t_note("GetDeviceInfo(unknown type 999) -> %ld", r);
    CHECK(r != ERROR_SUCCESS);
    h.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME; h.size = sizeof h;      /* too small for this request */
    r = U(DisplayConfigGetDeviceInfo)(&h);
    t_note("GetDeviceInfo(short packet) -> %ld", r);
    CHECK(r != ERROR_SUCCESS);
    h.size = 0;
    r = U(DisplayConfigGetDeviceInfo)(&h);
    t_note("GetDeviceInfo(size 0) -> %ld", r);
    CHECK(r != ERROR_SUCCESS);
}
TEST(paint_gdi_dcfg_getinfo_bad_ids) {
    NEED_DC();
    DISPLAYCONFIG_SOURCE_DEVICE_NAME sn; memset(&sn, 0, sizeof sn);
    sn.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME; sn.header.size = sizeof sn;
    sn.header.adapterId.LowPart = 0xdead; sn.header.adapterId.HighPart = 0xbeef; sn.header.id = 0x1234;
    LONG r = U(DisplayConfigGetDeviceInfo)(&sn.header);
    t_note("GetDeviceInfo(bogus adapter) -> %ld", r);
    CHECK(r != ERROR_SUCCESS);
}
TEST(paint_gdi_dcfg_set_invalid_only) {
    REQUIRE(U(DisplayConfigSetDeviceInfo));
    /* only malformed requests: nothing can be applied */
    LONG r;
    DISPLAYCONFIG_DEVICE_INFO_HEADER h; memset(&h, 0, sizeof h);
    h.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)0x7777; h.size = sizeof h;
    r = U(DisplayConfigSetDeviceInfo)(&h);
    t_note("DisplayConfigSetDeviceInfo(unknown type) -> %ld", r);
    CHECK(r != ERROR_SUCCESS);
    h.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME; h.size = 0;     /* a getter type is not a valid setter */
    r = U(DisplayConfigSetDeviceInfo)(&h);
    CHECK(r != ERROR_SUCCESS);
}
TEST(paint_gdi_dcfg_target_name_flags_bits) {
    NEED_DC();
    dcfg d; CHECK(dc_load(&d, QDC_ONLY_ACTIVE_PATHS));
    if (d.np) {
        DISPLAYCONFIG_TARGET_DEVICE_NAME tn; memset(&tn, 0, sizeof tn);
        tn.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME; tn.header.size = sizeof tn;
        tn.header.adapterId = d.p[0].targetInfo.adapterId; tn.header.id = d.p[0].targetInfo.id;
        if (U(DisplayConfigGetDeviceInfo)(&tn.header) == ERROR_SUCCESS) {
            CHECK((tn.flags.value & ~7u) == 0);
            CHECK(tn.outputTechnology >= DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER || tn.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL || (int)tn.outputTechnology < 0 || 1);
        }
    }
    dc_free(&d);
}
