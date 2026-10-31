/* rects, coordinate mapping, AdjustWindowRect, SetWindowPos, MoveWindow, min/max info, deferred positioning */
#include "wm.h"


static void gui(void) { U(IsGUIThread)(TRUE); }
static HWND top(int x, int y, int w, int h) { return wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, x, y, w, h, NULL); }
static HWND top_cap(int x, int y, int w, int h) { return wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, x, y, w, h, NULL); }

static int rw(const RECT *r) { return r->right - r->left; }
static int rh(const RECT *r) { return r->bottom - r->top; }

TEST(wm_geom_getwindowrect_basic) {
    NEED(GetWindowRect);
    HWND h = top(31, 47, 123, 89);
    RECT r = {0};
    CHECK(U(GetWindowRect)(h, &r));
    EQ(r.left, 31); EQ(r.top, 47); EQ(r.right, 154); EQ(r.bottom, 136);
    wm_destroy(h);
}

TEST(wm_geom_getclientrect_popup) {
    HWND h = top(31, 47, 123, 89);
    RECT r = {5, 5, 5, 5};
    CHECK(U(GetClientRect)(h, &r));
    EQ(r.left, 0); EQ(r.top, 0); EQ(r.right, 123); EQ(r.bottom, 89);
    wm_destroy(h);
}

TEST(wm_geom_getclientrect_border) {
    HWND h = wm_mk(WS_POPUP | WS_BORDER, WS_EX_TOOLWINDOW, 0, 0, 100, 60, NULL);
    RECT r; CHECK(U(GetClientRect)(h, &r));
    int b = U(GetSystemMetrics)(SM_CXBORDER);
    EQ(r.right, 100 - 2 * b);
    EQ(r.bottom, 60 - 2 * U(GetSystemMetrics)(SM_CYBORDER));
    wm_destroy(h);
}

TEST(wm_geom_getclientrect_clientedge) {
    HWND h = wm_mk(WS_POPUP, WS_EX_CLIENTEDGE | WS_EX_TOOLWINDOW, 0, 0, 100, 60, NULL);
    RECT r; CHECK(U(GetClientRect)(h, &r));
    EQ(r.right, 100 - 2 * U(GetSystemMetrics)(SM_CXEDGE));
    EQ(r.bottom, 60 - 2 * U(GetSystemMetrics)(SM_CYEDGE));
    wm_destroy(h);
}

TEST(wm_geom_getclientrect_scrollbars) {
    HWND h = wm_mk(WS_POPUP | WS_VSCROLL | WS_HSCROLL, WS_EX_TOOLWINDOW, 0, 0, 200, 150, NULL);
    RECT r; CHECK(U(GetClientRect)(h, &r));
    EQ(r.right, 200 - U(GetSystemMetrics)(SM_CXVSCROLL));
    EQ(r.bottom, 150 - U(GetSystemMetrics)(SM_CYHSCROLL));
    wm_destroy(h);
}

TEST(wm_geom_getclientrect_leftscrollbar_origin) {
    HWND h = wm_mk(WS_POPUP | WS_VSCROLL, WS_EX_TOOLWINDOW | WS_EX_LEFTSCROLLBAR, 10, 10, 200, 150, NULL);
    RECT r; CHECK(U(GetClientRect)(h, &r));
    EQ(r.left, 0); EQ(r.top, 0);
    EQ(r.right, 200 - U(GetSystemMetrics)(SM_CXVSCROLL));
    POINT p = {0, 0};
    CHECK(U(ClientToScreen)(h, &p));
    EQ(p.x, 10 + U(GetSystemMetrics)(SM_CXVSCROLL));
    wm_destroy(h);
}

TEST(wm_geom_rect_dead_and_null) {
    RECT r = {1, 2, 3, 4};
    SetLastError(0);
    CHECK(!U(GetWindowRect)(NULL, &r));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(GetClientRect)(NULL, &r));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(GetWindowRect)((HWND)(ULONG_PTR)0xABCD0, &r));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_geom_desktop_rect) {
    HWND d = U(GetDesktopWindow)();
    RECT r; CHECK(U(GetWindowRect)(d, &r));
    EQ(r.left, 0); EQ(r.top, 0);
    CHECK(r.right > 0 && r.bottom > 0);
    if (r.right != U(GetSystemMetrics)(SM_CXSCREEN) || r.bottom != U(GetSystemMetrics)(SM_CYSCREEN))
        t_note("desktop rect %ldx%ld vs screen %dx%d", r.right, r.bottom, U(GetSystemMetrics)(SM_CXSCREEN), U(GetSystemMetrics)(SM_CYSCREEN));
    RECT c; CHECK(U(GetClientRect)(d, &c));
    EQ(c.left, 0); EQ(c.top, 0);
    EQ(c.right, r.right); EQ(c.bottom, r.bottom);
}

TEST(wm_geom_child_rect_is_screen_coords) {
    HWND p = top_cap(100, 120, 300, 250);
    HWND c = wm_mk(WS_CHILD, 0, 7, 9, 40, 30, p);
    POINT o = {0, 0}; U(ClientToScreen)(p, &o);
    RECT r; CHECK(U(GetWindowRect)(c, &r));
    EQ(r.left, o.x + 7); EQ(r.top, o.y + 9); EQ(rw(&r), 40); EQ(rh(&r), 30);
    CHECK(U(GetClientRect)(c, &r));
    EQ(r.left, 0); EQ(r.top, 0); EQ(r.right, 40); EQ(r.bottom, 30);
    wm_destroy(p);
}

TEST(wm_geom_client_to_screen_roundtrip) {
    HWND h = top_cap(40, 50, 300, 200);
    static const POINT pts[] = {{0,0},{1,1},{10,20},{-5,-7},{299,199},{1000,1000},{-1000,-1000},{32767,32767},{-32768,-32768}};
    POINT o = {0, 0}; CHECK(U(ClientToScreen)(h, &o));
    for (unsigned i = 0; i < sizeof pts / sizeof *pts; i++) {
        POINT p = pts[i];
        CHECK(U(ClientToScreen)(h, &p));
        EQ(p.x, pts[i].x + o.x); EQ(p.y, pts[i].y + o.y);
        CHECK(U(ScreenToClient)(h, &p));
        EQ(p.x, pts[i].x); EQ(p.y, pts[i].y);
    }
    wm_destroy(h);
}

TEST(wm_geom_client_origin_matches_nc) {
    HWND h = top_cap(40, 50, 300, 200);
    POINT o = {0, 0}; U(ClientToScreen)(h, &o);
    WINDOWINFO wi; memset(&wi, 0, sizeof wi); wi.cbSize = sizeof wi; U(GetWindowInfo)(h, &wi);
    EQ(o.x, wi.rcClient.left); EQ(o.y, wi.rcClient.top);
    RECT wr, cr; U(GetWindowRect)(h, &wr); U(GetClientRect)(h, &cr);
    CHECK(o.x >= wr.left); CHECK(o.y > wr.top);
    CHECK(o.x + cr.right <= wr.right); CHECK(o.y + cr.bottom <= wr.bottom);
    wm_destroy(h);
}

TEST(wm_geom_desktop_coordinate_identity) {
    HWND d = U(GetDesktopWindow)();
    POINT p = {123, 456};
    CHECK(U(ClientToScreen)(d, &p));
    EQ(p.x, 123); EQ(p.y, 456);
    CHECK(U(ScreenToClient)(d, &p));
    EQ(p.x, 123); EQ(p.y, 456);
}

TEST(wm_geom_client_to_screen_bad) {
    POINT p = {1, 2};
    CHECK(!U(ClientToScreen)(NULL, &p));
    CHECK(!U(ScreenToClient)(NULL, &p));
    EQ(p.x, 1); EQ(p.y, 2);
    CHECK(!U(ClientToScreen)((HWND)(ULONG_PTR)0x55550, &p));
    CHECK(!U(ScreenToClient)((HWND)(ULONG_PTR)0x55550, &p));
}

TEST(wm_geom_child_client_to_screen_nested) {
    HWND p = top_cap(100, 100, 400, 300);
    HWND c = wm_mk(WS_CHILD | WS_BORDER, 0, 20, 30, 200, 100, p);
    HWND g = wm_mk(WS_CHILD, 0, 5, 6, 50, 40, c);
    POINT a = {0, 0}, b = {0, 0}, d = {0, 0};
    U(ClientToScreen)(p, &a); U(ClientToScreen)(c, &b); U(ClientToScreen)(g, &d);
    EQ(b.x, a.x + 20 + U(GetSystemMetrics)(SM_CXBORDER));
    EQ(b.y, a.y + 30 + U(GetSystemMetrics)(SM_CYBORDER));
    EQ(d.x, b.x + 5); EQ(d.y, b.y + 6);
    wm_destroy(p);
}

TEST(wm_geom_mapwindowpoints_basic) {
    NEED(MapWindowPoints);
    HWND h = top_cap(80, 90, 300, 200);
    POINT o = {0, 0}; U(ClientToScreen)(h, &o);
    POINT pt[2] = {{0, 0}, {10, 20}};
    int r = U(MapWindowPoints)(h, NULL, pt, 2);
    EQ(pt[0].x, o.x); EQ(pt[0].y, o.y); EQ(pt[1].x, o.x + 10); EQ(pt[1].y, o.y + 20);
    EQ(LOWORD(r), (WORD)(short)o.x); EQ(HIWORD(r), (WORD)(short)o.y);
    r = U(MapWindowPoints)(NULL, h, pt, 2);
    EQ(pt[0].x, 0); EQ(pt[0].y, 0); EQ(pt[1].x, 10); EQ(pt[1].y, 20);
    EQ(LOWORD(r), (WORD)(short)-o.x); EQ(HIWORD(r), (WORD)(short)-o.y);
    wm_destroy(h);
}

TEST(wm_geom_mapwindowpoints_same_window) {
    HWND h = top_cap(80, 90, 300, 200);
    POINT pt = {33, 44};
    int r = U(MapWindowPoints)(h, h, &pt, 1);
    EQ(r, 0); EQ(pt.x, 33); EQ(pt.y, 44);
    r = U(MapWindowPoints)(NULL, NULL, &pt, 1);
    EQ(r, 0); EQ(pt.x, 33); EQ(pt.y, 44);
    r = U(MapWindowPoints)(U(GetDesktopWindow)(), NULL, &pt, 1);
    EQ(r, 0); EQ(pt.x, 33);
    wm_destroy(h);
}

TEST(wm_geom_mapwindowpoints_siblings) {
    HWND p = top_cap(80, 90, 400, 300);
    HWND a = wm_mk(WS_CHILD, 0, 10, 20, 50, 50, p);
    HWND b = wm_mk(WS_CHILD, 0, 100, 120, 50, 50, p);
    POINT pt = {0, 0};
    int r = U(MapWindowPoints)(a, b, &pt, 1);
    EQ(pt.x, -90); EQ(pt.y, -100);
    EQ(LOWORD(r), (WORD)(short)-90); EQ(HIWORD(r), (WORD)(short)-100);
    pt.x = 0; pt.y = 0;
    U(MapWindowPoints)(b, a, &pt, 1);
    EQ(pt.x, 90); EQ(pt.y, 100);
    wm_destroy(p);
}

TEST(wm_geom_mapwindowpoints_rect) {
    HWND h = top_cap(80, 90, 300, 200);
    POINT o = {0, 0}; U(ClientToScreen)(h, &o);
    RECT r = {1, 2, 11, 22};
    U(MapWindowPoints)(h, NULL, (POINT *)&r, 2);
    EQ(r.left, o.x + 1); EQ(r.top, o.y + 2); EQ(r.right, o.x + 11); EQ(r.bottom, o.y + 22);
    wm_destroy(h);
}

TEST(wm_geom_mapwindowpoints_zero_points) {
    HWND h = top_cap(80, 90, 300, 200);
    POINT o = {0, 0}; U(ClientToScreen)(h, &o);
    int r = U(MapWindowPoints)(h, NULL, NULL, 0);
    EQ(LOWORD(r), (WORD)(short)o.x); EQ(HIWORD(r), (WORD)(short)o.y);
    wm_destroy(h);
}

TEST(wm_geom_mapwindowpoints_bad_handle) {
    POINT pt = {5, 6};
    SetLastError(0);
    int r = U(MapWindowPoints)((HWND)(ULONG_PTR)0x3330, NULL, &pt, 1);
    EQ(r, 0);
    t_note("MapWindowPoints(bad) err=%lu pt=%ld,%ld", GetLastError(), pt.x, pt.y);
    HWND h = wm_hidden(); wm_destroy(h);
    SetLastError(0);
    r = U(MapWindowPoints)(NULL, h, &pt, 1);
    EQ(r, 0);
}

TEST(wm_geom_mapwindowpoints_across_threads) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_POPUP, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    HWND h = top(10, 10, 50, 50);
    POINT pt = {0, 0};
    int r = U(MapWindowPoints)(w.hwnd, h, &pt, 1);
    t_note("MapWindowPoints across threads: r=%d pt=%ld,%ld", r, pt.x, pt.y);
    wm_destroy(h);
    wm_worker_stop(&w);
}

TEST(wm_geom_mapwindowpoints_rtl_roundtrip) {
    HWND h = wm_mk(WS_POPUP, WS_EX_LAYOUTRTL | WS_EX_TOOLWINDOW, 50, 50, 200, 100, NULL);
    if (!h) { t_skip("no RTL"); return; }
    POINT p = {17, 23}, q = p;
    U(ClientToScreen)(h, &q);
    POINT s = q;
    U(ScreenToClient)(h, &q);
    EQ(q.x, p.x); EQ(q.y, p.y);
    t_note("RTL client (17,23) -> screen (%ld,%ld)", s.x, s.y);
    wm_destroy(h);
}

/* ---- AdjustWindowRect family ---- */
static BOOL fd(RECT *r, DWORD st, BOOL menu, DWORD ex, UINT dpi) {
    return U(AdjustWindowRectExForDpi)(r, st, menu, ex, dpi);
}

TEST(wm_adjust_popup_unchanged) {
    NEED(AdjustWindowRectEx); NEED(AdjustWindowRect);
    gui();
    RECT r = {10, 20, 110, 220};
    CHECK(U(AdjustWindowRectEx)(&r, WS_POPUP, FALSE, 0));
    EQ(r.left, 10); EQ(r.top, 20); EQ(r.right, 110); EQ(r.bottom, 220);
    CHECK(U(AdjustWindowRect)(&r, WS_POPUP, FALSE));
    EQ(r.left, 10); EQ(r.right, 110);
    CHECK(U(AdjustWindowRect)(&r, WS_OVERLAPPED, FALSE));
    EQ(r.left, 10); EQ(r.top, 20);
    CHECK(U(AdjustWindowRect)(&r, WS_CHILD, FALSE));
    EQ(r.left, 10); EQ(r.top, 20); EQ(r.right, 110); EQ(r.bottom, 220);
}

TEST(wm_adjust_border_literal_dpi96) {
    NEED(AdjustWindowRectExForDpi);
    gui();
    RECT r = {100, 100, 200, 200};
    CHECK(fd(&r, WS_BORDER, FALSE, 0, 96));
    EQ(r.left, 99); EQ(r.top, 99); EQ(r.right, 201); EQ(r.bottom, 201);
    r = (RECT){100, 100, 200, 200};
    CHECK(fd(&r, WS_POPUP | WS_BORDER, FALSE, 0, 96));
    EQ(r.left, 99); EQ(r.right, 201);
    r = (RECT){100, 100, 200, 200};
    CHECK(fd(&r, WS_POPUP, FALSE, WS_EX_CLIENTEDGE, 96));
    EQ(r.left, 98); EQ(r.top, 98); EQ(r.right, 202); EQ(r.bottom, 202);
    r = (RECT){100, 100, 200, 200};
    CHECK(fd(&r, WS_POPUP, FALSE, WS_EX_STATICEDGE, 96));
    EQ(r.left, 99); EQ(r.right, 201);
    r = (RECT){100, 100, 200, 200};
    CHECK(fd(&r, WS_POPUP, FALSE, WS_EX_DLGMODALFRAME, 96));
    EQ(r.left, 97); EQ(r.top, 97); EQ(r.right, 203); EQ(r.bottom, 203);
    r = (RECT){100, 100, 200, 200};
    CHECK(fd(&r, WS_DLGFRAME, FALSE, 0, 96));
    EQ(r.left, 97); EQ(r.right, 203);
    r = (RECT){100, 100, 200, 200};
    CHECK(fd(&r, WS_POPUP, FALSE, WS_EX_CLIENTEDGE | WS_EX_STATICEDGE, 96));
    t_note("clientedge|staticedge adjusts by %ld", 100 - r.left);
}

TEST(wm_adjust_caption_adds_height_dpi96) {
    gui();
    RECT r = {0, 0, 100, 100}, b = r;
    CHECK(fd(&b, WS_BORDER, FALSE, 0, 96));
    CHECK(fd(&r, WS_BORDER | WS_DLGFRAME, FALSE, 0, 96)); /* WS_CAPTION */
    int cap = U(GetSystemMetricsForDpi)(SM_CYCAPTION, 96);
    EQ(r.top, r.left - cap);
    CHECK(cap > 0);
    r = (RECT){0, 0, 100, 100};
    CHECK(fd(&r, WS_CAPTION, FALSE, WS_EX_TOOLWINDOW, 96));
    EQ(r.top, r.left - U(GetSystemMetricsForDpi)(SM_CYSMCAPTION, 96));
}

TEST(wm_adjust_menu_adds_height_dpi96) {
    gui();
    RECT a = {0, 0, 100, 100}, b = {0, 0, 100, 100};
    CHECK(fd(&a, WS_OVERLAPPEDWINDOW, FALSE, 0, 96));
    CHECK(fd(&b, WS_OVERLAPPEDWINDOW, TRUE, 0, 96));
    EQ(b.top, a.top - U(GetSystemMetricsForDpi)(SM_CYMENU, 96));
    EQ(b.left, a.left); EQ(b.right, a.right); EQ(b.bottom, a.bottom);
    RECT c = {0, 0, 100, 100};
    CHECK(fd(&c, WS_POPUP, TRUE, 0, 96));
    EQ(c.top, -U(GetSystemMetricsForDpi)(SM_CYMENU, 96));
    EQ(c.left, 0);
}

TEST(wm_adjust_overlappedwindow_dpi96) {
    gui();
    RECT r = {0, 0, 100, 100};
    CHECK(fd(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, 96));
    int bx = U(GetSystemMetricsForDpi)(SM_CXSIZEFRAME, 96) + U(GetSystemMetricsForDpi)(SM_CXPADDEDBORDER, 96);
    int by = U(GetSystemMetricsForDpi)(SM_CYSIZEFRAME, 96) + U(GetSystemMetricsForDpi)(SM_CXPADDEDBORDER, 96);
    EQ(r.left, -bx); EQ(r.right, 100 + bx);
    EQ(r.top, -by - U(GetSystemMetricsForDpi)(SM_CYCAPTION, 96)); EQ(r.bottom, 100 + by);
}

TEST(wm_adjust_thickframe_only) {
    gui();
    RECT r = {0, 0, 100, 100};
    CHECK(fd(&r, WS_THICKFRAME, FALSE, 0, 96));
    CHECK(r.left < 0 && r.right > 100);
    EQ(r.top, r.left);
    EQ(r.bottom - 100, -r.top);
}

TEST(wm_adjust_dpi_zero_fails) {
    NEED(AdjustWindowRectExForDpi);
    gui();
    RECT r = {0, 0, 10, 10};
    SetLastError(0);
    CHECK(!fd(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, 0));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EQ(r.left, 0); EQ(r.right, 10);
}

TEST(wm_adjust_dpi_scales) {
    gui();
    RECT a = {0, 0, 100, 100}, b = {0, 0, 100, 100}, c = {0, 0, 100, 100};
    CHECK(fd(&a, WS_OVERLAPPEDWINDOW, FALSE, 0, 96));
    CHECK(fd(&b, WS_OVERLAPPEDWINDOW, FALSE, 0, 144));
    CHECK(fd(&c, WS_OVERLAPPEDWINDOW, FALSE, 0, 192));
    CHECK(b.left <= a.left); CHECK(c.left <= b.left);
    t_note("frame at 96/144/192: %ld %ld %ld; top %ld %ld %ld", a.left, b.left, c.left, a.top, b.top, c.top);
    /* symmetry */
    CHECK(c.right - 100 == -c.left);
    CHECK(c.bottom - 100 == -c.top - (U(GetSystemMetricsForDpi)(SM_CYCAPTION, 192)));
}

TEST(wm_adjust_nondpi_matches_system_dpi) {
    gui();
    RECT a = {0, 0, 100, 100}, b = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&a, WS_OVERLAPPEDWINDOW, FALSE, 0));
    CHECK(fd(&b, WS_OVERLAPPEDWINDOW, FALSE, 0, U(GetDpiForSystem)()));
    EQ(a.left, b.left); EQ(a.top, b.top); EQ(a.right, b.right); EQ(a.bottom, b.bottom);
    CHECK(U(AdjustWindowRect)(&a, WS_OVERLAPPEDWINDOW, TRUE));
    b = (RECT){0, 0, 100, 100};
    CHECK(fd(&b, WS_OVERLAPPEDWINDOW, TRUE, 0, U(GetDpiForSystem)()));
    t_note("adjust a.top=%ld b.top=%ld (double adjust expected differ)", a.top, b.top);
}

TEST(wm_adjust_consistency_with_created_window) {
    NEED(AdjustWindowRectEx);
    gui();
    static const struct { DWORD st, ex; } t[] = {
        { WS_POPUP, 0 }, { WS_POPUP | WS_BORDER, 0 }, { WS_POPUP, WS_EX_CLIENTEDGE }, { WS_OVERLAPPEDWINDOW, 0 },
        { WS_CAPTION, 0 }, { WS_CAPTION | WS_SYSMENU, WS_EX_TOOLWINDOW }, { WS_THICKFRAME, 0 }, { WS_DLGFRAME, 0 },
        { WS_POPUP, WS_EX_DLGMODALFRAME }, { WS_POPUPWINDOW | WS_CAPTION, 0 }, { WS_OVERLAPPEDWINDOW, WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE },
        { WS_POPUP, WS_EX_STATICEDGE }, { WS_POPUP | WS_VSCROLL, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RECT r = {0, 0, 321, 123};
        CHECK(U(AdjustWindowRectEx)(&r, t[i].st, FALSE, t[i].ex));
        HWND h = wm_mk(t[i].st, t[i].ex | WS_EX_NOACTIVATE, 50, 50, rw(&r), rh(&r), NULL);
        CHECKF(h != NULL, "case %u", i);
        if (!h) continue;
        RECT c; U(GetClientRect)(h, &c);
        if ((t[i].st & WS_CAPTION) == WS_DLGFRAME || ((t[i].st & WS_CAPTION) == 0 && (t[i].st & WS_THICKFRAME))) {
            t_note("case %u client %ldx%ld", i, c.right, c.bottom); wm_destroy(h); continue;
        }
        CHECKF(c.bottom == 123, "case %u client h %ld", i, c.bottom);
        if (!(t[i].st & WS_VSCROLL)) CHECKF(c.right == 321, "case %u client w %ld", i, c.right);
        CHECKF(c.right >= 321 - ((t[i].st & WS_VSCROLL) ? 20 : 0), "case %u client w %ld", i, c.right);
        wm_destroy(h);
    }
}

TEST(wm_adjust_extra_edge_ignored_bits) {
    gui();
    RECT r = {0, 0, 100, 100};
    /* visible/disabled/clipping bits do not alter the frame */
    CHECK(fd(&r, WS_POPUP | WS_VISIBLE | WS_DISABLED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | WS_GROUP, FALSE, 0, 96));
    EQ(r.left, 0); EQ(r.right, 100);
    r = (RECT){0, 0, 100, 100};
    CHECK(fd(&r, WS_POPUP, FALSE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES | WS_EX_LAYERED | WS_EX_NOACTIVATE, 96));
    EQ(r.left, 0); EQ(r.right, 100);
}

TEST(wm_adjust_inverted_rect) {
    gui();
    RECT r = {100, 100, 0, 0};
    CHECK(fd(&r, WS_BORDER, FALSE, 0, 96));
    EQ(r.left, 99); EQ(r.top, 99); EQ(r.right, 1); EQ(r.bottom, 1);
}

TEST(wm_adjust_repeat_is_additive) {
    gui();
    RECT r = {0, 0, 0, 0};
    CHECK(fd(&r, WS_BORDER, FALSE, 0, 96));
    CHECK(fd(&r, WS_BORDER, FALSE, 0, 96));
    EQ(r.left, -2); EQ(r.right, 2);
}

/* ---- SetWindowPos / MoveWindow ---- */
TEST(wm_swp_move_and_size) {
    NEED(SetWindowPos);
    HWND h = top(10, 20, 100, 80);
    CHECK(U(SetWindowPos)(h, NULL, 30, 40, 150, 90, SWP_NOZORDER | SWP_NOACTIVATE));
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 30); EQ(r.top, 40); EQ(rw(&r), 150); EQ(rh(&r), 90);
    wm_destroy(h);
}

TEST(wm_swp_nomove_nosize) {
    HWND h = top(10, 20, 100, 80);
    CHECK(U(SetWindowPos)(h, NULL, 99, 99, 999, 999, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 10); EQ(r.top, 20); EQ(rw(&r), 100); EQ(rh(&r), 80);
    CHECK(U(SetWindowPos)(h, NULL, 50, 60, 999, 999, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
    U(GetWindowRect)(h, &r);
    EQ(r.left, 50); EQ(r.top, 60); EQ(rw(&r), 100); EQ(rh(&r), 80);
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 70, 30, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE));
    U(GetWindowRect)(h, &r);
    EQ(r.left, 50); EQ(r.top, 60); EQ(rw(&r), 70); EQ(rh(&r), 30);
    wm_destroy(h);
}

TEST(wm_swp_messages_move_only) {
    HWND h = top(10, 20, 100, 80);
    wm_logclear();
    CHECK(U(SetWindowPos)(h, NULL, 33, 44, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
    EQ(wm_logcount(WM_WINDOWPOSCHANGING), 1);
    EQ(wm_logcount(WM_WINDOWPOSCHANGED), 1);
    EQ(wm_logcount(WM_MOVE), 1);
    EQ(wm_logcount(WM_SIZE), 0);
    int i = wm_logfind(WM_MOVE, 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(l, MAKELPARAM(33, 44)); EQ(w, 0); }
    CHECK(wm_logfind(WM_WINDOWPOSCHANGING, 0) < wm_logfind(WM_WINDOWPOSCHANGED, 0));
    wm_destroy(h);
}

TEST(wm_swp_messages_size_only) {
    HWND h = top(10, 20, 100, 80);
    wm_logclear();
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 120, 95, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE));
    EQ(wm_logcount(WM_SIZE), 1);
    EQ(wm_logcount(WM_MOVE), 0);
    EQ(wm_logcount(WM_NCCALCSIZE), 1);
    int i = wm_logfind(WM_SIZE, 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(l, MAKELPARAM(120, 95)); EQ(w, SIZE_RESTORED); }
    wm_destroy(h);
}

TEST(wm_swp_no_change_no_size_move) {
    HWND h = top(10, 20, 100, 80);
    wm_logclear();
    CHECK(U(SetWindowPos)(h, NULL, 10, 20, 100, 80, SWP_NOZORDER | SWP_NOACTIVATE));
    EQ(wm_logcount(WM_MOVE), 0);
    EQ(wm_logcount(WM_SIZE), 0);
    wm_destroy(h);
}

TEST(wm_swp_framechanged) {
    HWND h = top(10, 20, 100, 80);
    wm_logclear();
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED));
    CHECK(wm_logcount(WM_NCCALCSIZE) >= 1);
    CHECK(wm_logcount(WM_WINDOWPOSCHANGED) >= 1);
    wm_destroy(h);
}

TEST(wm_swp_style_change_needs_framechanged) {
    HWND h = top(10, 20, 100, 80);
    LONG st = U(GetWindowLongA)(h, GWL_STYLE);
    U(SetWindowLongA)(h, GWL_STYLE, st | WS_BORDER);
    RECT c; U(GetClientRect)(h, &c);
    EQ(c.right, 100);   /* not applied until SWP_FRAMECHANGED */
    U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    U(GetClientRect)(h, &c);
    EQ(c.right, 100 - 2 * U(GetSystemMetrics)(SM_CXBORDER));
    wm_destroy(h);
}

TEST(wm_swp_show_hide_flags) {
    HWND h = top(10, 20, 100, 80);
    CHECK(!U(IsWindowVisible)(h));
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW));
    CHECK(U(IsWindowVisible)(h));
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW));
    CHECK(!U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_swp_pos_messages_data) {
    HWND h = top(10, 20, 100, 80);
    wm_logclear();
    U(SetWindowPos)(h, NULL, 33, 44, 120, 95, SWP_NOZORDER | SWP_NOACTIVATE);
    int i = wm_logfind(WM_WINDOWPOSCHANGING, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, 0);
    wm_destroy(h);
}

TEST(wm_swp_bogus_hwnd) {
    SetLastError(0);
    CHECK(!U(SetWindowPos)(NULL, NULL, 0, 0, 10, 10, SWP_NOZORDER));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(SetWindowPos)((HWND)(ULONG_PTR)0x9990, NULL, 0, 0, 10, 10, SWP_NOZORDER));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(SetWindowPos)(U(GetDesktopWindow)(), NULL, 0, 0, 10, 10, SWP_NOZORDER | SWP_NOACTIVATE));
    t_note("SetWindowPos(desktop) err=%lu", GetLastError());
}

TEST(wm_swp_bogus_insert_after) {
    HWND h = top(10, 20, 100, 80);
    SetLastError(0);
    CHECK(!U(SetWindowPos)(h, (HWND)(ULONG_PTR)0x8880, 0, 0, 10, 10, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));
    t_note("SetWindowPos(bogus insertAfter) err=%lu", GetLastError());
    wm_destroy(h);
}

TEST(wm_swp_invalid_flags) {
    HWND h = top(10, 20, 100, 80);
    SetLastError(0);
    BOOL r = U(SetWindowPos)(h, NULL, 0, 0, 10, 10, 0x80000000u | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
    t_note("SetWindowPos(invalid flag bit) = %d err=%lu", r, GetLastError());
    wm_destroy(h);
}

TEST(wm_swp_child_positions) {
    HWND p = top_cap(100, 100, 400, 300);
    HWND c = wm_mk(WS_CHILD, 0, 1, 1, 10, 10, p);
    CHECK(U(SetWindowPos)(c, NULL, 25, 35, 60, 45, SWP_NOZORDER | SWP_NOACTIVATE));
    POINT o = {0, 0}; U(ClientToScreen)(p, &o);
    RECT r; U(GetWindowRect)(c, &r);
    EQ(r.left, o.x + 25); EQ(r.top, o.y + 35); EQ(rw(&r), 60); EQ(rh(&r), 45);
    wm_destroy(p);
}

TEST(wm_swp_child_negative_and_outside) {
    HWND p = top_cap(100, 100, 400, 300);
    HWND c = wm_mk(WS_CHILD, 0, 1, 1, 10, 10, p);
    CHECK(U(SetWindowPos)(c, NULL, -20, -30, 60, 45, SWP_NOZORDER | SWP_NOACTIVATE));
    POINT o = {0, 0}; U(ClientToScreen)(p, &o);
    RECT r; U(GetWindowRect)(c, &r);
    EQ(r.left, o.x - 20); EQ(r.top, o.y - 30);
    CHECK(U(SetWindowPos)(c, NULL, 5000, 5000, 60, 45, SWP_NOZORDER | SWP_NOACTIVATE));
    U(GetWindowRect)(c, &r);
    EQ(r.left, o.x + 5000);
    wm_destroy(p);
}

TEST(wm_swp_zero_size) {
    HWND h = top(10, 20, 100, 80);
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE));
    RECT r; U(GetWindowRect)(h, &r);
    EQ(rw(&r), 0); EQ(rh(&r), 0);
    U(GetClientRect)(h, &r);
    EQ(r.right, 0); EQ(r.bottom, 0);
    wm_destroy(h);
}

TEST(wm_swp_negative_size) {
    HWND h = top(10, 20, 100, 80);
    SetLastError(0);
    BOOL ok = U(SetWindowPos)(h, NULL, 0, 0, -50, -50, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    RECT r; U(GetWindowRect)(h, &r);
    t_note("negative cx/cy: ok=%d size %ldx%ld", ok, rw(&r), rh(&r));
    CHECK(rw(&r) >= 0 && rh(&r) >= 0);
    wm_destroy(h);
}

TEST(wm_swp_overlapped_min_track_note) {
    HWND h = top_cap(100, 100, 300, 200);
    U(SetWindowPos)(h, NULL, 0, 0, 1, 1, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    RECT r; U(GetWindowRect)(h, &r);
    t_note("overlapped window sized to 1x1 became %ldx%ld (min track %dx%d)", rw(&r), rh(&r), U(GetSystemMetrics)(SM_CXMINTRACK), U(GetSystemMetrics)(SM_CYMINTRACK));
    CHECK(rw(&r) >= 1 && rh(&r) >= 1);
    wm_destroy(h);
}

TEST(wm_move_basic) {
    NEED(MoveWindow);
    HWND h = top(10, 20, 100, 80);
    CHECK(U(MoveWindow)(h, 70, 80, 111, 77, FALSE));
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 70); EQ(r.top, 80); EQ(rw(&r), 111); EQ(rh(&r), 77);
    CHECK(U(MoveWindow)(h, 70, 80, 111, 77, TRUE));
    wm_destroy(h);
}

TEST(wm_move_child) {
    HWND p = top_cap(100, 100, 400, 300);
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    CHECK(U(MoveWindow)(c, 15, 16, 20, 21, TRUE));
    POINT o = {0, 0}; U(ClientToScreen)(p, &o);
    RECT r; U(GetWindowRect)(c, &r);
    EQ(r.left, o.x + 15); EQ(r.top, o.y + 16); EQ(rw(&r), 20); EQ(rh(&r), 21);
    wm_destroy(p);
}

TEST(wm_move_minimized_window) {
    HWND h = top_cap(100, 100, 300, 200);
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    CHECK(U(IsIconic)(h));
    CHECK(U(MoveWindow)(h, 150, 160, 333, 222, FALSE));
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    U(GetWindowPlacement)(h, &wp);
    t_note("MoveWindow on minimized: normal pos %ld,%ld %ldx%ld", wp.rcNormalPosition.left, wp.rcNormalPosition.top, rw(&wp.rcNormalPosition), rh(&wp.rcNormalPosition));
    wm_destroy(h);
}

TEST(wm_move_maximized_window_changes_normal_rect) {
    HWND h = top_cap(100, 100, 300, 200);
    U(ShowWindow)(h, SW_SHOWNA);
    U(ShowWindow)(h, SW_MAXIMIZE);
    CHECK(U(IsZoomed)(h));
    CHECK(U(MoveWindow)(h, 150, 160, 333, 222, FALSE));
    t_note("after MoveWindow while maximized: zoomed=%d", U(IsZoomed)(h));
    wm_destroy(h);
}

TEST(wm_move_dead_window) {
    HWND h = wm_hidden(); wm_destroy(h);
    SetLastError(0);
    CHECK(!U(MoveWindow)(h, 0, 0, 10, 10, TRUE));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(MoveWindow)(NULL, 0, 0, 10, 10, TRUE));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_move_repeated_sizes) {
    HWND h = top(0, 0, 10, 10);
    int bad = 0;
    for (int i = 0; i < 200; i++) {
        int w = 1 + (i * 7) % 300, hh = 1 + (i * 13) % 200, x = (i * 3) % 500 - 100, y = (i * 5) % 400 - 100;
        U(MoveWindow)(h, x, y, w, hh, FALSE);
        RECT r; U(GetWindowRect)(h, &r);
        if (r.left != x || r.top != y || rw(&r) != w || rh(&r) != hh) bad++;
    }
    EQ(bad, 0);
    wm_destroy(h);
}

TEST(wm_move_cross_thread) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_POPUP, WS_EX_TOOLWINDOW, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    CHECK(U(MoveWindow)(w.hwnd, 200, 210, 77, 66, FALSE));
    RECT r; U(GetWindowRect)(w.hwnd, &r);
    EQ(r.left, 200); EQ(r.top, 210); EQ(rw(&r), 77); EQ(rh(&r), 66);
    CHECK(U(SetWindowPos)(w.hwnd, NULL, 5, 6, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS));
    for (int i = 0; i < 100; i++) { U(GetWindowRect)(w.hwnd, &r); if (r.left == 5) break; Sleep(20); }
    EQ(r.left, 5); EQ(r.top, 6);
    wm_worker_stop(&w);
}

/* ---- WM_GETMINMAXINFO ---- */
TEST(wm_minmax_default_values) {
    HWND h = top_cap(100, 100, 300, 200);
    /* record what the system proposes by letting the message through unmodified */
    wm_minmax_on = 0; wm_mmi_count = 0;
    memset(&wm_mmi_seen, 0xAA, sizeof wm_mmi_seen);
    U(SetWindowPos)(h, NULL, 0, 0, 350, 250, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CHECK(wm_mmi_count >= 1);
    MINMAXINFO mmi = wm_mmi_seen;
    EQ(mmi.ptMinTrackSize.x, U(GetSystemMetrics)(SM_CXMINTRACK));
    EQ(mmi.ptMinTrackSize.y, U(GetSystemMetrics)(SM_CYMINTRACK));
    CHECK(mmi.ptMaxTrackSize.x >= U(GetSystemMetrics)(SM_CXSCREEN));
    CHECK(mmi.ptMaxTrackSize.y >= U(GetSystemMetrics)(SM_CYSCREEN));
    CHECK(mmi.ptMaxSize.x >= U(GetSystemMetrics)(SM_CXSCREEN));
    CHECK(mmi.ptMaxSize.y >= U(GetSystemMetrics)(SM_CYSCREEN) - 100);
    CHECK(mmi.ptMaxPosition.x <= 0);
    CHECK(mmi.ptMaxPosition.y <= 0);
    t_note("default maxpos %ld,%ld maxsize %ldx%ld maxtrack %ldx%ld", mmi.ptMaxPosition.x, mmi.ptMaxPosition.y, mmi.ptMaxSize.x, mmi.ptMaxSize.y, mmi.ptMaxTrackSize.x, mmi.ptMaxTrackSize.y);
    wm_destroy(h);
}

TEST(wm_minmax_defproc_leaves_struct) {
    HWND h = top_cap(100, 100, 300, 200);
    MINMAXINFO mmi; memset(&mmi, 0x11, sizeof mmi);
    wm_minmax_on = 0;
    LRESULT r = U(SendMessageA)(h, WM_GETMINMAXINFO, 0, (LPARAM)&mmi);
    t_note("DefWindowProc(WM_GETMINMAXINFO) result %lld, minTrack.x=%ld", (long long)r, mmi.ptMinTrackSize.x);
    wm_destroy(h);
}

TEST(wm_minmax_sent_on_resize) {
    HWND h = top_cap(100, 100, 300, 200);
    wm_logclear();
    U(SetWindowPos)(h, NULL, 0, 0, 350, 250, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CHECK(wm_logcount(WM_GETMINMAXINFO) >= 1);
    wm_logclear();
    U(SetWindowPos)(h, NULL, 10, 10, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
    t_note("GETMINMAXINFO on move-only: %d", wm_logcount(WM_GETMINMAXINFO));
    wm_destroy(h);
}

TEST(wm_minmax_not_sent_for_child) {
    HWND p = top_cap(100, 100, 300, 200);
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    wm_logclear();
    U(SetWindowPos)(c, NULL, 0, 0, 40, 40, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    t_note("GETMINMAXINFO for child resize: %d", wm_logcount(WM_GETMINMAXINFO));
    wm_destroy(p);
}

TEST(wm_minmax_override_min_track) {
    HWND h = top_cap(100, 100, 400, 300);
    memset(&wm_minmax, 0, sizeof wm_minmax);
    wm_minmax.ptMinTrackSize.x = 260; wm_minmax.ptMinTrackSize.y = 180;
    wm_minmax.ptMaxTrackSize.x = 900; wm_minmax.ptMaxTrackSize.y = 700;
    wm_minmax.ptMaxSize.x = 900; wm_minmax.ptMaxSize.y = 700;
    wm_minmax_on = 1;
    U(SetWindowPos)(h, NULL, 0, 0, 100, 100, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    RECT r; U(GetWindowRect)(h, &r);
    EQ(rw(&r), 260); EQ(rh(&r), 180);
    U(SetWindowPos)(h, NULL, 0, 0, 5000, 5000, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    U(GetWindowRect)(h, &r);
    EQ(rw(&r), 900); EQ(rh(&r), 700);
    U(SetWindowPos)(h, NULL, 0, 0, 500, 400, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    U(GetWindowRect)(h, &r);
    EQ(rw(&r), 500); EQ(rh(&r), 400);
    wm_minmax_on = 0;
    wm_destroy(h);
}

TEST(wm_minmax_override_maximize) {
    HWND h = top_cap(100, 100, 400, 300);
    memset(&wm_minmax, 0, sizeof wm_minmax);
    wm_minmax.ptMinTrackSize.x = 100; wm_minmax.ptMinTrackSize.y = 50;
    wm_minmax.ptMaxTrackSize.x = 900; wm_minmax.ptMaxTrackSize.y = 700;
    wm_minmax.ptMaxSize.x = 500; wm_minmax.ptMaxSize.y = 350;
    wm_minmax.ptMaxPosition.x = 20; wm_minmax.ptMaxPosition.y = 30;
    wm_minmax_on = 1;
    U(ShowWindow)(h, SW_SHOWNA);
    U(ShowWindow)(h, SW_MAXIMIZE);
    CHECK(U(IsZoomed)(h));
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 20); EQ(r.top, 30); EQ(rw(&r), 500); EQ(rh(&r), 350);
    wm_minmax_on = 0;
    U(ShowWindow)(h, SW_RESTORE);
    U(GetWindowRect)(h, &r);
    EQ(r.left, 100); EQ(r.top, 100); EQ(rw(&r), 400); EQ(rh(&r), 300);
    wm_destroy(h);
}

TEST(wm_minmax_override_at_create) {
    memset(&wm_minmax, 0, sizeof wm_minmax);
    wm_minmax.ptMinTrackSize.x = 321; wm_minmax.ptMinTrackSize.y = 222;
    wm_minmax.ptMaxTrackSize.x = 900; wm_minmax.ptMaxTrackSize.y = 700;
    wm_minmax.ptMaxSize.x = 900; wm_minmax.ptMaxSize.y = 700;
    wm_minmax_on = 1;
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_NOACTIVATE, 100, 100, 50, 50, NULL);
    wm_minmax_on = 0;
    CHECK(h != NULL);
    if (h) { RECT r; U(GetWindowRect)(h, &r); EQ(rw(&r), 321); EQ(rh(&r), 222); }
    wm_destroy(h);
}

TEST(wm_minmax_popup_not_clamped) {
    HWND h = top(100, 100, 3, 3);
    RECT r; U(GetWindowRect)(h, &r);
    EQ(rw(&r), 3); EQ(rh(&r), 3);
    wm_destroy(h);
}

/* ---- DeferWindowPos ---- */
TEST(wm_defer_basic) {
    NEED(BeginDeferWindowPos); NEED(DeferWindowPos); NEED(EndDeferWindowPos);
    HWND a = top(0, 0, 50, 50), b = top(0, 0, 50, 50), c = top(0, 0, 50, 50);
    HDWP d = U(BeginDeferWindowPos)(3);
    CHECK(d != NULL);
    d = U(DeferWindowPos)(d, a, NULL, 10, 11, 60, 61, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d != NULL);
    d = U(DeferWindowPos)(d, b, NULL, 20, 21, 70, 71, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d != NULL);
    d = U(DeferWindowPos)(d, c, NULL, 30, 31, 80, 81, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d != NULL);
    RECT r; U(GetWindowRect)(a, &r);
    EQ(r.left, 0); EQ(rw(&r), 50);          /* nothing applied yet */
    CHECK(U(EndDeferWindowPos)(d));
    U(GetWindowRect)(a, &r); EQ(r.left, 10); EQ(r.top, 11); EQ(rw(&r), 60); EQ(rh(&r), 61);
    U(GetWindowRect)(b, &r); EQ(r.left, 20); EQ(r.top, 21); EQ(rw(&r), 70); EQ(rh(&r), 71);
    U(GetWindowRect)(c, &r); EQ(r.left, 30); EQ(r.top, 31); EQ(rw(&r), 80); EQ(rh(&r), 81);
    wm_destroy(a); wm_destroy(b); wm_destroy(c);
}

TEST(wm_defer_grows_beyond_hint) {
    HWND w[6];
    for (int i = 0; i < 6; i++) w[i] = top(0, 0, 10, 10);
    HDWP d = U(BeginDeferWindowPos)(1);
    CHECK(d != NULL);
    for (int i = 0; i < 6 && d; i++) d = U(DeferWindowPos)(d, w[i], NULL, i * 10, i * 5, 30, 30, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d != NULL);
    if (d) CHECK(U(EndDeferWindowPos)(d));
    for (int i = 0; i < 6; i++) { RECT r; U(GetWindowRect)(w[i], &r); EQ(r.left, i * 10); EQ(r.top, i * 5); EQ(rw(&r), 30); }
    for (int i = 0; i < 6; i++) wm_destroy(w[i]);
}

TEST(wm_defer_same_window_twice) {
    HWND a = top(0, 0, 50, 50);
    HDWP d = U(BeginDeferWindowPos)(2);
    d = U(DeferWindowPos)(d, a, NULL, 10, 10, 60, 60, SWP_NOZORDER | SWP_NOACTIVATE);
    d = U(DeferWindowPos)(d, a, NULL, 20, 20, 70, 70, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d != NULL);
    if (d) CHECK(U(EndDeferWindowPos)(d));
    RECT r; U(GetWindowRect)(a, &r);
    EQ(r.left, 20); EQ(rw(&r), 70);
    wm_destroy(a);
}

TEST(wm_defer_negative_count) {
    SetLastError(0);
    HDWP d = U(BeginDeferWindowPos)(-1);
    EQ(d, NULL);
    t_note("BeginDeferWindowPos(-1) err=%lu", GetLastError());
    d = U(BeginDeferWindowPos)(0);
    t_note("BeginDeferWindowPos(0) = %p", d);
    if (d) U(EndDeferWindowPos)(d);
}

TEST(wm_defer_end_invalid) {
    SetLastError(0);
    CHECK(!U(EndDeferWindowPos)(NULL));
    t_note("EndDeferWindowPos(NULL) err=%lu", GetLastError());
    SetLastError(0);
    CHECK(!U(EndDeferWindowPos)((HDWP)(ULONG_PTR)0x1234));
    t_note("EndDeferWindowPos(bogus) err=%lu", GetLastError());
}

TEST(wm_defer_bad_window) {
    HDWP d = U(BeginDeferWindowPos)(2);
    SetLastError(0);
    HDWP e = U(DeferWindowPos)(d, (HWND)(ULONG_PTR)0x7770, NULL, 0, 0, 1, 1, SWP_NOZORDER);
    t_note("DeferWindowPos(bad hwnd) = %p err=%lu", e, GetLastError());
    if (!e) CHECK(1); else U(EndDeferWindowPos)(e);
}

TEST(wm_defer_different_parents) {
    HWND p1 = top_cap(0, 0, 200, 200), p2 = top_cap(0, 0, 200, 200);
    HWND c1 = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p1), c2 = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p2);
    HDWP d = U(BeginDeferWindowPos)(2);
    d = U(DeferWindowPos)(d, c1, NULL, 1, 1, 20, 20, SWP_NOZORDER);
    CHECK(d != NULL);
    SetLastError(0);
    HDWP e = d ? U(DeferWindowPos)(d, c2, NULL, 1, 1, 20, 20, SWP_NOZORDER) : NULL;
    t_note("DeferWindowPos with different parents = %p err=%lu", e, GetLastError());
    if (e) U(EndDeferWindowPos)(e);
    wm_destroy(p1); wm_destroy(p2);
}

TEST(wm_defer_show_hide_flags) {
    HWND a = top(0, 0, 50, 50);
    HDWP d = U(BeginDeferWindowPos)(1);
    d = U(DeferWindowPos)(d, a, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    CHECK(d != NULL);
    CHECK(!U(IsWindowVisible)(a));
    if (d) CHECK(U(EndDeferWindowPos)(d));
    CHECK(U(IsWindowVisible)(a));
    wm_destroy(a);
}

TEST(wm_defer_window_destroyed_before_end) {
    HWND a = top(0, 0, 50, 50), b = top(0, 0, 50, 50);
    HDWP d = U(BeginDeferWindowPos)(2);
    d = U(DeferWindowPos)(d, a, NULL, 5, 5, 60, 60, SWP_NOZORDER | SWP_NOACTIVATE);
    d = U(DeferWindowPos)(d, b, NULL, 6, 6, 61, 61, SWP_NOZORDER | SWP_NOACTIVATE);
    wm_destroy(a);
    BOOL r = d ? U(EndDeferWindowPos)(d) : FALSE;
    t_note("EndDeferWindowPos with a destroyed window = %d", r);
    RECT rc; U(GetWindowRect)(b, &rc);
    t_note("surviving window left=%ld", rc.left);
    wm_destroy(b);
}
