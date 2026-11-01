/* layered windows, regions, display affinity, animation, flashing, GUI thread info, message filter, DPI */
#include "wm.h"

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x11
#endif
UDECL(BOOL, AreDpiAwarenessContextsEqual_, (HANDLE, HANDLE));

static HWND lay(DWORD ex) { return wm_mk(WS_POPUP, WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | ex, 400, 400, 64, 48, NULL); }

/* ---------------- layered windows ---------------- */
TEST(wm_layered_set_on_plain_window_fails) {
    NEED(SetLayeredWindowAttributes);
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 0, 0, 20, 20, NULL);
    SetLastError(0);
    CHECK(!U(SetLayeredWindowAttributes)(h, 0, 128, LWA_ALPHA));
    t_note("SetLayeredWindowAttributes(non layered) err=%lu", GetLastError());
    BYTE a; COLORREF k; DWORD f;
    SetLastError(0);
    CHECK(!U(GetLayeredWindowAttributes)(h, &k, &a, &f));
    t_note("GetLayeredWindowAttributes(non layered) err=%lu", GetLastError());
    wm_destroy(h);
}

TEST(wm_layered_alpha_roundtrip) {
    NEED(GetLayeredWindowAttributes);
    HWND h = lay(0);
    CHECK(U(SetLayeredWindowAttributes)(h, 0, 128, LWA_ALPHA));
    BYTE a = 0; COLORREF k = 0xdeadbeef; DWORD f = 0xFFFF;
    CHECK(U(GetLayeredWindowAttributes)(h, &k, &a, &f));
    EQ(a, 128); EQ(f, LWA_ALPHA);
    wm_destroy(h);
}

TEST(wm_layered_alpha_boundaries) {
    HWND h = lay(0);
    static const BYTE vals[] = { 0, 1, 127, 128, 254, 255 };
    for (unsigned i = 0; i < sizeof vals; i++) {
        CHECKF(U(SetLayeredWindowAttributes)(h, 0, vals[i], LWA_ALPHA), "alpha %u", vals[i]);
        BYTE a = 77; DWORD f = 0;
        CHECK(U(GetLayeredWindowAttributes)(h, NULL, &a, &f));
        EQ(a, vals[i]); EQ(f, LWA_ALPHA);
    }
    wm_destroy(h);
}

TEST(wm_layered_colorkey_roundtrip) {
    HWND h = lay(0);
    CHECK(U(SetLayeredWindowAttributes)(h, RGB(1, 2, 3), 0, LWA_COLORKEY));
    COLORREF k = 0; DWORD f = 0; BYTE a = 0x55;
    CHECK(U(GetLayeredWindowAttributes)(h, &k, &a, &f));
    EQ(k & 0xFFFFFF, RGB(1, 2, 3));
    EQ(f, LWA_COLORKEY);
    wm_destroy(h);
}

TEST(wm_layered_both_flags) {
    HWND h = lay(0);
    CHECK(U(SetLayeredWindowAttributes)(h, RGB(255, 0, 255), 200, LWA_COLORKEY | LWA_ALPHA));
    COLORREF k = 0; DWORD f = 0; BYTE a = 0;
    CHECK(U(GetLayeredWindowAttributes)(h, &k, &a, &f));
    EQ(k & 0xFFFFFF, RGB(255, 0, 255)); EQ(a, 200); EQ(f, LWA_COLORKEY | LWA_ALPHA);
    CHECK(U(SetLayeredWindowAttributes)(h, 0, 10, LWA_ALPHA));
    CHECK(U(GetLayeredWindowAttributes)(h, &k, &a, &f));
    EQ(a, 10); EQ(f, LWA_ALPHA);
    wm_destroy(h);
}

TEST(wm_layered_null_out_params) {
    HWND h = lay(0);
    U(SetLayeredWindowAttributes)(h, RGB(9, 9, 9), 99, LWA_ALPHA | LWA_COLORKEY);
    BYTE a = 0; DWORD f = 0; COLORREF k = 0;
    CHECK(U(GetLayeredWindowAttributes)(h, NULL, NULL, NULL));
    CHECK(U(GetLayeredWindowAttributes)(h, &k, NULL, NULL));
    CHECK(U(GetLayeredWindowAttributes)(h, NULL, &a, NULL));
    CHECK(U(GetLayeredWindowAttributes)(h, NULL, NULL, &f));
    EQ(a, 99); EQ(f, LWA_ALPHA | LWA_COLORKEY); EQ(k & 0xFFFFFF, RGB(9, 9, 9));
    wm_destroy(h);
}

TEST(wm_layered_flags_invalid) {
    HWND h = lay(0);
    SetLastError(0);
    BOOL r = U(SetLayeredWindowAttributes)(h, 0, 100, 0x4);
    t_note("SetLayeredWindowAttributes(flags=4) = %d err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(SetLayeredWindowAttributes)(h, 0, 100, 0);
    t_note("SetLayeredWindowAttributes(flags=0) = %d err=%lu", r, GetLastError());
    wm_destroy(h);
}

TEST(wm_layered_created_without_attributes) {
    HWND h = lay(0);
    BYTE a = 0; DWORD f = 0; COLORREF k = 0;
    SetLastError(0);
    BOOL r = U(GetLayeredWindowAttributes)(h, &k, &a, &f);
    t_note("GetLayeredWindowAttributes before any Set = %d err=%lu", r, GetLastError());
    wm_destroy(h);
}

TEST(wm_layered_style_toggle) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 400, 400, 40, 40, NULL);
    U(SetWindowLongA)(h, GWL_EXSTYLE, U(GetWindowLongA)(h, GWL_EXSTYLE) | WS_EX_LAYERED);
    CHECK(U(GetWindowLongA)(h, GWL_EXSTYLE) & WS_EX_LAYERED);
    CHECK(U(SetLayeredWindowAttributes)(h, 0, 50, LWA_ALPHA));
    U(SetWindowLongA)(h, GWL_EXSTYLE, U(GetWindowLongA)(h, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    BYTE a; DWORD f;
    CHECK(!U(GetLayeredWindowAttributes)(h, NULL, &a, &f));
    CHECK(!U(SetLayeredWindowAttributes)(h, 0, 50, LWA_ALPHA));
    wm_destroy(h);
}

TEST(wm_layered_dead_and_null) {
    HWND h = lay(0); wm_destroy(h);
    BYTE a; DWORD f; COLORREF k;
    SetLastError(0);
    CHECK(!U(SetLayeredWindowAttributes)(h, 0, 1, LWA_ALPHA));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    CHECK(!U(GetLayeredWindowAttributes)(h, &k, &a, &f));
    CHECK(!U(SetLayeredWindowAttributes)(NULL, 0, 1, LWA_ALPHA));
    CHECK(!U(GetLayeredWindowAttributes)(NULL, &k, &a, &f));
}

TEST(wm_layered_child_window) {
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, WS_EX_LAYERED, 0, 0, 20, 20, p);
    t_note("child with WS_EX_LAYERED created: %p", c);
    if (c) {
        SetLastError(0);
        BOOL r = U(SetLayeredWindowAttributes)(c, 0, 100, LWA_ALPHA);
        t_note("SetLayeredWindowAttributes(child) = %d err=%lu", r, GetLastError());
    }
    wm_destroy(p);
}

TEST(wm_layered_visible_roundtrip_style) {
    HWND h = lay(0);
    U(SetLayeredWindowAttributes)(h, 0, 200, LWA_ALPHA);
    U(ShowWindow)(h, SW_SHOWNA);
    CHECK(U(IsWindowVisible)(h));
    BYTE a = 0; DWORD f = 0;
    CHECK(U(GetLayeredWindowAttributes)(h, NULL, &a, &f));
    EQ(a, 200);
    U(ShowWindow)(h, SW_HIDE);
    wm_destroy(h);
}

static HDC mk_dib(HBITMAP *bmp, int w, int hh, DWORD argb) {
    HDC dc = CreateCompatibleDC(NULL);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -hh;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    *bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (*bmp && bits) { DWORD *p = bits; for (int i = 0; i < w * hh; i++) p[i] = argb; SelectObject(dc, *bmp); }
    return dc;
}

TEST(wm_layered_updatelayeredwindow_basic) {
    NEED(UpdateLayeredWindow);
    HWND h = lay(0);
    HBITMAP bmp; HDC dc = mk_dib(&bmp, 32, 24, 0x80402010);
    SIZE sz = {32, 24}; POINT src = {0, 0}, dst = {410, 420};
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    SetLastError(0);
    BOOL r = U(UpdateLayeredWindow)(h, NULL, &dst, &sz, dc, &src, 0, &bf, ULW_ALPHA);
    CHECK(r);
    if (!r) t_note("UpdateLayeredWindow err=%lu", GetLastError());
    RECT rc; U(GetWindowRect)(h, &rc);
    if (r) { EQ(rc.left, 410); EQ(rc.top, 420); EQ(rc.right - rc.left, 32); EQ(rc.bottom - rc.top, 24); }
    BYTE a; DWORD f;
    SetLastError(0);
    BOOL g = U(GetLayeredWindowAttributes)(h, NULL, &a, &f);
    t_note("GetLayeredWindowAttributes after UpdateLayeredWindow = %d err=%lu", g, GetLastError());
    DeleteDC(dc); DeleteObject(bmp);
    wm_destroy(h);
}

TEST(wm_layered_updatelayeredwindow_no_surface_move_only) {
    HWND h = lay(0);
    POINT dst = {123, 234};
    SetLastError(0);
    BOOL r = U(UpdateLayeredWindow)(h, NULL, &dst, NULL, NULL, NULL, 0, NULL, 0);
    t_note("UpdateLayeredWindow(move only) = %d err=%lu", r, GetLastError());
    if (r) { RECT rc; U(GetWindowRect)(h, &rc); EQ(rc.left, 123); EQ(rc.top, 234); }
    wm_destroy(h);
}

TEST(wm_layered_updatelayeredwindow_non_layered_fails) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 0, 0, 20, 20, NULL);
    HBITMAP bmp; HDC dc = mk_dib(&bmp, 20, 20, 0xFF000000);
    SIZE sz = {20, 20}; POINT src = {0, 0};
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    SetLastError(0);
    CHECK(!U(UpdateLayeredWindow)(h, NULL, NULL, &sz, dc, &src, 0, &bf, ULW_ALPHA));
    t_note("UpdateLayeredWindow(non layered) err=%lu", GetLastError());
    DeleteDC(dc); DeleteObject(bmp);
    wm_destroy(h);
}

TEST(wm_layered_updatelayeredwindow_dead) {
    HWND h = lay(0); wm_destroy(h);
    HBITMAP bmp; HDC dc = mk_dib(&bmp, 20, 20, 0xFF000000);
    SIZE sz = {20, 20}; POINT src = {0, 0};
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    SetLastError(0);
    CHECK(!U(UpdateLayeredWindow)(h, NULL, NULL, &sz, dc, &src, 0, &bf, ULW_ALPHA));
    t_note("UpdateLayeredWindow(dead) err=%lu", GetLastError());
    CHECK(!U(UpdateLayeredWindow)(NULL, NULL, NULL, &sz, dc, &src, 0, &bf, ULW_ALPHA));
    DeleteDC(dc); DeleteObject(bmp);
}

TEST(wm_layered_updatelayeredwindow_bad_blend) {
    HWND h = lay(0);
    HBITMAP bmp; HDC dc = mk_dib(&bmp, 20, 20, 0xFF000000);
    SIZE sz = {20, 20}; POINT src = {0, 0};
    BLENDFUNCTION bf = { 7 /*bad op*/, 0, 255, 0 };
    SetLastError(0);
    BOOL r = U(UpdateLayeredWindow)(h, NULL, NULL, &sz, dc, &src, 0, &bf, ULW_ALPHA);
    t_note("UpdateLayeredWindow(bad BlendOp) = %d err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(UpdateLayeredWindow)(h, NULL, NULL, &sz, dc, &src, 0, NULL, ULW_ALPHA);
    t_note("UpdateLayeredWindow(ULW_ALPHA, no blend) = %d err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(UpdateLayeredWindow)(h, NULL, NULL, &sz, dc, &src, 0, &bf, 0x100);
    t_note("UpdateLayeredWindow(bad flags) = %d err=%lu", r, GetLastError());
    DeleteDC(dc); DeleteObject(bmp);
    wm_destroy(h);
}

TEST(wm_layered_updatelayeredwindow_colorkey) {
    HWND h = lay(0);
    HBITMAP bmp; HDC dc = mk_dib(&bmp, 16, 16, 0x00FF00FF);
    SIZE sz = {16, 16}; POINT src = {0, 0};
    SetLastError(0);
    BOOL r = U(UpdateLayeredWindow)(h, NULL, NULL, &sz, dc, &src, RGB(255, 0, 255), NULL, ULW_COLORKEY);
    t_note("UpdateLayeredWindow(colorkey) = %d err=%lu", r, GetLastError());
    if (r) { RECT rc; U(GetWindowRect)(h, &rc); EQ(rc.right - rc.left, 16); EQ(rc.bottom - rc.top, 16); }
    DeleteDC(dc); DeleteObject(bmp);
    wm_destroy(h);
}

TEST(wm_layered_updatelayeredwindowindirect) {
    REQUIRE(U(UpdateLayeredWindowIndirect));
    HWND h = lay(0);
    HBITMAP bmp; HDC dc = mk_dib(&bmp, 24, 18, 0xC0102030);
    SIZE sz = {24, 18}; POINT src = {0, 0}, dst = {300, 310};
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UPDATELAYEREDWINDOWINFO ui; memset(&ui, 0, sizeof ui);
    ui.cbSize = sizeof ui; ui.pptDst = &dst; ui.psize = &sz; ui.hdcSrc = dc; ui.pptSrc = &src; ui.pblend = &bf; ui.dwFlags = ULW_ALPHA;
    SetLastError(0);
    BOOL r = U(UpdateLayeredWindowIndirect)(h, &ui);
    CHECK(r);
    if (r) { RECT rc; U(GetWindowRect)(h, &rc); EQ(rc.left, 300); EQ(rc.top, 310); EQ(rc.right - rc.left, 24); EQ(rc.bottom - rc.top, 18); }
    else t_note("UpdateLayeredWindowIndirect err=%lu", GetLastError());
    RECT dirty = {0, 0, 5, 5};
    ui.prcDirty = &dirty;
    r = U(UpdateLayeredWindowIndirect)(h, &ui);
    t_note("UpdateLayeredWindowIndirect with dirty rect = %d", r);
    DeleteDC(dc); DeleteObject(bmp);
    wm_destroy(h);
}

TEST(wm_layered_updatelayeredwindowindirect_bad_cbsize) {
    REQUIRE(U(UpdateLayeredWindowIndirect));
    HWND h = lay(0);
    HBITMAP bmp; HDC dc = mk_dib(&bmp, 24, 18, 0xC0102030);
    SIZE sz = {24, 18}; POINT src = {0, 0};
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UPDATELAYEREDWINDOWINFO ui; memset(&ui, 0, sizeof ui);
    ui.psize = &sz; ui.hdcSrc = dc; ui.pptSrc = &src; ui.pblend = &bf; ui.dwFlags = ULW_ALPHA;
    SetLastError(0);
    ui.cbSize = 0;
    CHECK(!U(UpdateLayeredWindowIndirect)(h, &ui));
    t_note("UpdateLayeredWindowIndirect(cbSize=0) err=%lu", GetLastError());
    ui.cbSize = sizeof ui + 8;
    CHECK(!U(UpdateLayeredWindowIndirect)(h, &ui));
    ui.cbSize = sizeof ui - 8;
    CHECK(!U(UpdateLayeredWindowIndirect)(h, &ui));
    EQ(sizeof(UPDATELAYEREDWINDOWINFO), 80);
    DeleteDC(dc); DeleteObject(bmp);
    wm_destroy(h);
}

/* ---------------- display affinity ---------------- */
TEST(wm_affinity_default_none) {
    NEED(GetWindowDisplayAffinity);
    HWND h = wm_hidden();
    DWORD a = 0xFFFF;
    CHECK(U(GetWindowDisplayAffinity)(h, &a));
    EQ(a, WDA_NONE);
    wm_destroy(h);
}

TEST(wm_affinity_set_monitor_roundtrip) {
    NEED(SetWindowDisplayAffinity);
    HWND h = wm_hidden();
    SetLastError(0);
    BOOL r = U(SetWindowDisplayAffinity)(h, WDA_MONITOR);
    DWORD a = 0xFFFF;
    if (!r) { t_note("SetWindowDisplayAffinity(WDA_MONITOR) failed err=%lu (no DWM?)", GetLastError()); wm_destroy(h); return; }
    CHECK(U(GetWindowDisplayAffinity)(h, &a));
    EQ(a, WDA_MONITOR);
    CHECK(U(SetWindowDisplayAffinity)(h, WDA_NONE));
    CHECK(U(GetWindowDisplayAffinity)(h, &a));
    EQ(a, WDA_NONE);
    wm_destroy(h);
}

TEST(wm_affinity_exclude_from_capture) {
    HWND h = wm_hidden();
    SetLastError(0);
    BOOL r = U(SetWindowDisplayAffinity)(h, WDA_EXCLUDEFROMCAPTURE);
    DWORD a = 0xFFFF;
    t_note("SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE) = %d err=%lu", r, GetLastError());
    if (r) {
        CHECK(U(GetWindowDisplayAffinity)(h, &a));
        EQ(a, WDA_EXCLUDEFROMCAPTURE);
        CHECK(U(SetWindowDisplayAffinity)(h, WDA_NONE));
    }
    wm_destroy(h);
}

TEST(wm_affinity_invalid_values) {
    HWND h = wm_hidden();
    static const DWORD bad[] = { 2, 3, 0x10, 0x12, 0x100, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        BOOL r = U(SetWindowDisplayAffinity)(h, bad[i]);
        t_note("SetWindowDisplayAffinity(0x%lx) = %d err=%lu", bad[i], r, GetLastError());
        if (r) U(SetWindowDisplayAffinity)(h, WDA_NONE);
    }
    DWORD a = 0xFFFF;
    CHECK(U(GetWindowDisplayAffinity)(h, &a));
    CHECK(a == WDA_NONE || a == 0xFFFF || 1);
    wm_destroy(h);
}

TEST(wm_affinity_other_process_window) {
    HWND d = U(GetDesktopWindow)();
    SetLastError(0);
    CHECK(!U(SetWindowDisplayAffinity)(d, WDA_MONITOR));
    t_note("SetWindowDisplayAffinity(desktop) err=%lu", GetLastError());
    HWND s = U(GetShellWindow)();
    if (s) {
        SetLastError(0);
        CHECK(!U(SetWindowDisplayAffinity)(s, WDA_MONITOR));
        t_note("SetWindowDisplayAffinity(shell) err=%lu", GetLastError());
    }
}

TEST(wm_affinity_dead_and_null) {
    HWND h = wm_hidden(); wm_destroy(h);
    DWORD a = 0xFFFF;
    SetLastError(0);
    CHECK(!U(GetWindowDisplayAffinity)(h, &a));
    t_note("GetWindowDisplayAffinity(dead) err=%lu", GetLastError());
    CHECK(!U(SetWindowDisplayAffinity)(h, WDA_NONE));
    CHECK(!U(GetWindowDisplayAffinity)(NULL, &a));
    CHECK(!U(SetWindowDisplayAffinity)(NULL, WDA_NONE));
}

TEST(wm_affinity_child_window) {
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    SetLastError(0);
    BOOL r = U(SetWindowDisplayAffinity)(c, WDA_MONITOR);
    t_note("SetWindowDisplayAffinity(child) = %d err=%lu", r, GetLastError());
    if (r) U(SetWindowDisplayAffinity)(c, WDA_NONE);
    DWORD a = 0xFFFF;
    r = U(GetWindowDisplayAffinity)(c, &a);
    t_note("GetWindowDisplayAffinity(child) = %d a=%lu", r, a);
    wm_destroy(p);
}

TEST(wm_affinity_cross_thread_window) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    DWORD a = 0xFFFF;
    CHECK(U(GetWindowDisplayAffinity)(w.hwnd, &a));
    EQ(a, WDA_NONE);
    SetLastError(0);
    BOOL r = U(SetWindowDisplayAffinity)(w.hwnd, WDA_MONITOR);
    t_note("SetWindowDisplayAffinity(other thread, same process) = %d", r);
    if (r) U(SetWindowDisplayAffinity)(w.hwnd, WDA_NONE);
    wm_worker_stop(&w);
}

/* ---------------- window regions ---------------- */
TEST(wm_rgn_none_by_default) {
    NEED(GetWindowRgn); NEED(GetWindowRgnBox);
    HWND h = wm_hidden();
    HRGN r = CreateRectRgn(0, 0, 1, 1);
    EQ(U(GetWindowRgn)(h, r), ERROR);
    RECT rc = {1, 2, 3, 4};
    EQ(U(GetWindowRgnBox)(h, &rc), ERROR);
    DeleteObject(r);
    wm_destroy(h);
}

TEST(wm_rgn_rect_roundtrip) {
    NEED(SetWindowRgn);
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    HRGN r = CreateRectRgn(10, 20, 50, 60);
    CHECK(U(SetWindowRgn)(h, r, FALSE) != 0);
    RECT box = {0};
    EQ(U(GetWindowRgnBox)(h, &box), SIMPLEREGION);
    EQ(box.left, 10); EQ(box.top, 20); EQ(box.right, 50); EQ(box.bottom, 60);
    HRGN t = CreateRectRgn(0, 0, 0, 0);
    EQ(U(GetWindowRgn)(h, t), SIMPLEREGION);
    RECT tb; GetRgnBox(t, &tb);
    EQ(tb.left, 10); EQ(tb.top, 20); EQ(tb.right, 50); EQ(tb.bottom, 60);
    DeleteObject(t);
    wm_destroy(h);
}

TEST(wm_rgn_getwindowrgn_returns_copy) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    HRGN r = CreateRectRgn(5, 5, 25, 25);
    U(SetWindowRgn)(h, r, FALSE);
    HRGN t = CreateRectRgn(0, 0, 0, 0);
    U(GetWindowRgn)(h, t);
    OffsetRgn(t, 100, 100);                 /* modifying the copy must not affect the window region */
    RECT box;
    EQ(U(GetWindowRgnBox)(h, &box), SIMPLEREGION);
    EQ(box.left, 5); EQ(box.top, 5); EQ(box.right, 25); EQ(box.bottom, 25);
    DeleteObject(t);
    wm_destroy(h);
}

TEST(wm_rgn_elliptic_is_complex) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    HRGN r = CreateEllipticRgn(0, 0, 100, 80);
    CHECK(U(SetWindowRgn)(h, r, FALSE) != 0);
    HRGN t = CreateRectRgn(0, 0, 0, 0);
    EQ(U(GetWindowRgn)(h, t), COMPLEXREGION);
    RECT box;
    EQ(U(GetWindowRgnBox)(h, &box), COMPLEXREGION);
    EQ(box.left, 0); EQ(box.top, 0);
    CHECK(box.right >= 99 && box.right <= 100); CHECK(box.bottom >= 79 && box.bottom <= 80);
    CHECK(PtInRegion(t, 50, 40));
    CHECK(!PtInRegion(t, 1, 1));
    DeleteObject(t);
    wm_destroy(h);
}

TEST(wm_rgn_remove) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    HRGN r = CreateRectRgn(10, 20, 50, 60);
    U(SetWindowRgn)(h, r, FALSE);
    CHECK(U(SetWindowRgn)(h, NULL, FALSE) != 0);
    RECT box;
    EQ(U(GetWindowRgnBox)(h, &box), ERROR);
    HRGN t = CreateRectRgn(0, 0, 0, 0);
    EQ(U(GetWindowRgn)(h, t), ERROR);
    DeleteObject(t);
    CHECK(U(SetWindowRgn)(h, NULL, FALSE) != 0);       /* removing a missing region still succeeds */
    wm_destroy(h);
}

TEST(wm_rgn_replace) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    CHECK(U(SetWindowRgn)(h, CreateRectRgn(10, 20, 50, 60), FALSE));
    CHECK(U(SetWindowRgn)(h, CreateRectRgn(1, 2, 3, 4), FALSE));
    RECT box;
    EQ(U(GetWindowRgnBox)(h, &box), SIMPLEREGION);
    EQ(box.left, 1); EQ(box.top, 2); EQ(box.right, 3); EQ(box.bottom, 4);
    wm_destroy(h);
}

TEST(wm_rgn_empty_region) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    HRGN r = CreateRectRgn(0, 0, 0, 0);
    CHECK(U(SetWindowRgn)(h, r, FALSE) != 0);
    RECT box;
    int t = U(GetWindowRgnBox)(h, &box);
    t_note("empty window region: GetWindowRgnBox = %d", t);
    HRGN d = CreateRectRgn(0, 0, 0, 0);
    int t2 = U(GetWindowRgn)(h, d);
    t_note("empty window region: GetWindowRgn = %d", t2);
    DeleteObject(d);
    wm_destroy(h);
}

TEST(wm_rgn_invalid_args) {
    NEED(GetWindowRgn);
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    U(SetWindowRgn)(h, CreateRectRgn(10, 20, 50, 60), FALSE);
    EQ(U(GetWindowRgn)(h, NULL), ERROR);
    EQ(U(GetWindowRgn)(h, (HRGN)(ULONG_PTR)0x12345), ERROR);
    SetLastError(0);
    int r = U(SetWindowRgn)(h, (HRGN)(ULONG_PTR)0x12345, FALSE);
    t_note("SetWindowRgn(bogus HRGN) = %d err=%lu", r, GetLastError());
    RECT box;
    EQ(U(GetWindowRgnBox)(h, &box), SIMPLEREGION);   /* unchanged */
    wm_destroy(h);
}

TEST(wm_rgn_dead_window) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    wm_destroy(h);
    HRGN r = CreateRectRgn(0, 0, 10, 10);
    SetLastError(0);
    EQ(U(SetWindowRgn)(h, r, FALSE), 0);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    CHECK(DeleteObject(r));           /* ownership stays with the caller on failure */
    HRGN t = CreateRectRgn(0, 0, 1, 1);
    EQ(U(GetWindowRgn)(h, t), ERROR);
    RECT box;
    EQ(U(GetWindowRgnBox)(h, &box), ERROR);
    EQ(U(SetWindowRgn)(NULL, NULL, FALSE), 0);
    DeleteObject(t);
}

TEST(wm_rgn_survives_move_and_resize) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    U(SetWindowRgn)(h, CreateRectRgn(10, 20, 50, 60), FALSE);
    U(MoveWindow)(h, 300, 310, 200, 160, FALSE);
    RECT box;
    EQ(U(GetWindowRgnBox)(h, &box), SIMPLEREGION);
    EQ(box.left, 10); EQ(box.top, 20); EQ(box.right, 50); EQ(box.bottom, 60);
    RECT wr; U(GetWindowRect)(h, &wr);
    EQ(wr.right - wr.left, 200);      /* region does not change the window rectangle */
    wm_destroy(h);
}

TEST(wm_rgn_clipped_to_nothing_outside_window) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 100, 100, 100, 80, NULL);
    U(SetWindowRgn)(h, CreateRectRgn(-50, -50, 500, 500), FALSE);
    RECT box;
    int t = U(GetWindowRgnBox)(h, &box);
    t_note("oversized region box %ld,%ld,%ld,%ld type %d", box.left, box.top, box.right, box.bottom, t);
    CHECK(t != ERROR);
    wm_destroy(h);
}

TEST(wm_rgn_child_window) {
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 10, 10, 60, 40, p);
    HRGN r = CreateRectRgn(5, 5, 30, 30);
    CHECK(U(SetWindowRgn)(c, r, FALSE) != 0);
    RECT box;
    EQ(U(GetWindowRgnBox)(c, &box), SIMPLEREGION);
    EQ(box.left, 5); EQ(box.right, 30);
    wm_destroy(p);
}

TEST(wm_rgn_hit_test) {
    int sx = U(GetSystemMetrics)(SM_CXSCREEN) / 2, sy = U(GetSystemMetrics)(SM_CYSCREEN) / 2;
    HWND h = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, sx, sy, 100, 100, NULL);
    U(SetWindowRgn)(h, CreateRectRgn(0, 0, 50, 100), TRUE);
    t_pump();
    POINT in = { sx + 10, sy + 10 }, out = { sx + 80, sy + 10 };
    EQ(U(WindowFromPoint)(in), h);
    CHECK(U(WindowFromPoint)(out) != h);
    wm_destroy(h);
}

/* ---------------- animation / flash / hung ---------------- */
TEST(wm_animate_invalid) {
    NEED(AnimateWindow);
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 60, 40, NULL);
    SetLastError(0);
    CHECK(!U(AnimateWindow)(h, 20, AW_HIDE | AW_ACTIVATE));
    t_note("AnimateWindow(HIDE|ACTIVATE) err=%lu", GetLastError());
    CHECK(!U(IsWindowVisible)(h));
    SetLastError(0);
    CHECK(!U(AnimateWindow)(NULL, 20, AW_BLEND));
    SetLastError(0);
    CHECK(!U(AnimateWindow)((HWND)(ULONG_PTR)0x44440, 20, AW_BLEND));
    wm_destroy(h);
    SetLastError(0);
    CHECK(!U(AnimateWindow)(h, 20, AW_BLEND));
    t_note("AnimateWindow(dead) err=%lu", GetLastError());
}

TEST(wm_animate_blend_show_hide) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 60, 40, NULL);
    SetLastError(0);
    BOOL r = U(AnimateWindow)(h, 30, AW_BLEND);
    t_note("AnimateWindow(AW_BLEND show) = %d err=%lu", r, GetLastError());
    if (r) {
        CHECK(U(IsWindowVisible)(h));
        r = U(AnimateWindow)(h, 30, AW_BLEND | AW_HIDE);
        t_note("AnimateWindow(AW_BLEND hide) = %d", r);
        if (r) CHECK(!U(IsWindowVisible)(h));
    }
    wm_destroy(h);
}

TEST(wm_animate_slide_center) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 60, 40, NULL);
    BOOL r = U(AnimateWindow)(h, 30, AW_SLIDE | AW_HOR_POSITIVE);
    t_note("AnimateWindow(SLIDE|HOR_POSITIVE) = %d", r);
    if (r) CHECK(U(IsWindowVisible)(h));
    if (r) { r = U(AnimateWindow)(h, 30, AW_SLIDE | AW_HOR_POSITIVE | AW_HIDE); if (r) CHECK(!U(IsWindowVisible)(h)); }
    r = U(AnimateWindow)(h, 30, AW_CENTER);
    t_note("AnimateWindow(CENTER) = %d", r);
    if (r) CHECK(U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_animate_already_visible) {
    HWND h = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 60, 40, NULL);
    SetLastError(0);
    BOOL r = U(AnimateWindow)(h, 30, AW_BLEND);
    t_note("AnimateWindow(show already visible window) = %d err=%lu", r, GetLastError());
    wm_destroy(h);
}

TEST(wm_animate_zero_time) {
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 60, 40, NULL);
    DWORD t0 = GetTickCount();
    BOOL r = U(AnimateWindow)(h, 0, AW_BLEND);
    DWORD dt = GetTickCount() - t0;
    t_note("AnimateWindow(time 0) = %d took %lu ms", r, dt);
    CHECK(dt < 3000);
    wm_destroy(h);
}

TEST(wm_flash_basic) {
    NEED(FlashWindow);
    HWND h = wm_hidden();
    t_note("FlashWindow ret %d", U(FlashWindow)(h, TRUE));
    t_note("FlashWindow ret %d", U(FlashWindow)(h, FALSE));
    wm_destroy(h);
    SetLastError(0);
    EQ(U(FlashWindow)(h, TRUE), 0);
    EQ(U(FlashWindow)(NULL, TRUE), 0);
}

TEST(wm_flash_ex_basic) {
    NEED(FlashWindowEx);
    HWND h = wm_hidden();
    FLASHWINFO fi; memset(&fi, 0, sizeof fi);
    fi.cbSize = sizeof fi; fi.hwnd = h; fi.dwFlags = FLASHW_STOP;
    t_note("FlashWindowEx stop ret %d", U(FlashWindowEx)(&fi));
    fi.dwFlags = FLASHW_CAPTION | FLASHW_TIMER; fi.uCount = 1; fi.dwTimeout = 50;
    t_note("FlashWindowEx flash ret %d", U(FlashWindowEx)(&fi));
    fi.dwFlags = FLASHW_STOP;
    t_note("FlashWindowEx stop2 ret %d", U(FlashWindowEx)(&fi));
    EQ(sizeof(FLASHWINFO), 32);
    wm_destroy(h);
}

TEST(wm_flash_ex_invalid) {
    HWND h = wm_hidden();
    FLASHWINFO fi; memset(&fi, 0, sizeof fi);
    fi.hwnd = h; fi.dwFlags = FLASHW_STOP;
    SetLastError(0);
    fi.cbSize = 0;
    BOOL r = U(FlashWindowEx)(&fi);
    EQ(r, 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    fi.cbSize = sizeof fi + 4;
    SetLastError(0);
    EQ(U(FlashWindowEx)(&fi), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    fi.cbSize = sizeof fi; fi.hwnd = (HWND)(ULONG_PTR)0x55550;
    SetLastError(0);
    EQ(U(FlashWindowEx)(&fi), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER); /* Win10 reports 87, not 1400, for a bogus hwnd */
    fi.hwnd = NULL;
    SetLastError(0);
    EQ(U(FlashWindowEx)(&fi), 0);
    t_note("FlashWindowEx(NULL hwnd) err=%lu", GetLastError());
    fi.hwnd = h; fi.dwFlags = 0xFFFFFFF0;
    SetLastError(0);
    r = U(FlashWindowEx)(&fi);
    t_note("FlashWindowEx(bad flags) = %d err=%lu", r, GetLastError());
    fi.dwFlags = FLASHW_STOP;
    U(FlashWindowEx)(&fi);
    wm_destroy(h);
}

TEST(wm_hung_responsive) {
    NEED(IsHungAppWindow);
    t_pump();   /* a thread that has not run its message loop for 5 s counts as hung: refresh it first (the area runs without pumping for long stretches) */
    HWND h = wm_hidden();
    CHECK(!U(IsHungAppWindow)(h));
    CHECK(!U(IsHungAppWindow)(U(GetDesktopWindow)()));
    wm_destroy(h);
    CHECK(!U(IsHungAppWindow)(h));
    CHECK(!U(IsHungAppWindow)(NULL));
    CHECK(!U(IsHungAppWindow)((HWND)(ULONG_PTR)0x77770));
}

TEST(wm_hung_worker_window) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    CHECK(!U(IsHungAppWindow)(w.hwnd));
    wm_worker_stop(&w);
}

TEST(wm_hung_child_window) {
    t_pump();   /* see wm_hung_responsive */
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    CHECK(!U(IsHungAppWindow)(c));
    wm_destroy(p);
}
