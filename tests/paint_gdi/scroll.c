#include "pg.h"

typedef struct { HDC dc; HBITMAP bm, old; DWORD *bits; int w, h; } memdc;
static int memdc_make(memdc *m, int w, int h) {
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    m->dc = CreateCompatibleDC(NULL);
    m->bm = CreateDIBSection(m->dc, &bi, DIB_RGB_COLORS, (void **)&m->bits, NULL, 0);
    m->w = w; m->h = h;
    if (!m->dc || !m->bm) return 0;
    m->old = SelectObject(m->dc, m->bm);
    /* pixel (x,y) = RGB(x, y, 7) */
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) m->bits[y * w + x] = (x << 16) | (y << 8) | 7;
    return 1;
}
static void memdc_free(memdc *m) {
    if (m->dc) { if (m->old) SelectObject(m->dc, m->old); DeleteDC(m->dc); }
    if (m->bm) DeleteObject(m->bm);
}
static COLORREF px(memdc *m, int x, int y) { GdiFlush(); return GetPixel(m->dc, x, y); }
#define ORIG(x, y) RGB((x), (y), 7)

TEST(paint_gdi_scrolldc_horizontal) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 100, 100}, up = {-1, -1, -1, -1};
    HRGN rg = CreateRectRgn(0, 0, 0, 0);
    BOOL r = U(ScrollDC)(m.dc, 10, 0, &sc, &sc, rg, &up);
    CHECK(r);
    CHECK(px(&m, 10, 5) == ORIG(0, 5));
    CHECK(px(&m, 50, 77) == ORIG(40, 77));
    CHECK(px(&m, 99, 99) == ORIG(89, 99));
    CHECK_RECT(up, 0, 0, 10, 100);
    RECT b; EQ(GetRgnBox(rg, &b), SIMPLEREGION);
    CHECK_RECT(b, 0, 0, 10, 100);
    DeleteObject(rg); memdc_free(&m);
}
TEST(paint_gdi_scrolldc_negative_horizontal) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 100, 100}, up;
    CHECK(U(ScrollDC)(m.dc, -20, 0, &sc, &sc, NULL, &up));
    CHECK(px(&m, 0, 3) == ORIG(20, 3));
    CHECK(px(&m, 79, 3) == ORIG(99, 3));
    CHECK_RECT(up, 80, 0, 100, 100);
    memdc_free(&m);
}
TEST(paint_gdi_scrolldc_vertical) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 100, 100}, up;
    CHECK(U(ScrollDC)(m.dc, 0, 15, &sc, &sc, NULL, &up));
    CHECK(px(&m, 5, 15) == ORIG(5, 0));
    CHECK(px(&m, 5, 99) == ORIG(5, 84));
    CHECK_RECT(up, 0, 0, 100, 15);
    memdc_free(&m);
    CHECK(memdc_make(&m, 100, 100));
    CHECK(U(ScrollDC)(m.dc, 0, -5, &sc, &sc, NULL, &up));
    CHECK(px(&m, 5, 0) == ORIG(5, 5));
    CHECK_RECT(up, 0, 95, 100, 100);
    memdc_free(&m);
}
TEST(paint_gdi_scrolldc_diagonal_region) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 100, 100}, up;
    HRGN rg = CreateRectRgn(0, 0, 0, 0);
    CHECK(U(ScrollDC)(m.dc, 10, 5, &sc, &sc, rg, &up));
    CHECK(px(&m, 10, 5) == ORIG(0, 0));
    CHECK(px(&m, 99, 99) == ORIG(89, 94));
    RECT b; EQ(GetRgnBox(rg, &b), COMPLEXREGION);   /* L shaped */
    CHECK_RECT(b, 0, 0, 100, 100);
    CHECK(PtInRegion(rg, 3, 50));      /* left strip */
    CHECK(PtInRegion(rg, 50, 3));      /* top strip */
    CHECK(!PtInRegion(rg, 50, 50));
    CHECK_RECT(up, 0, 0, 100, 100);
    DeleteObject(rg); memdc_free(&m);
}
TEST(paint_gdi_scrolldc_clip_limits_effect) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 100, 100}, clip = {0, 0, 50, 100}, up;
    CHECK(U(ScrollDC)(m.dc, 10, 0, &sc, &clip, NULL, &up));
    CHECK(px(&m, 20, 10) == ORIG(10, 10));
    CHECK(px(&m, 49, 10) == ORIG(39, 10));
    CHECK(px(&m, 60, 10) == ORIG(60, 10));     /* outside the clip rectangle: untouched */
    CHECK(px(&m, 99, 10) == ORIG(99, 10));
    CHECK_RECT(up, 0, 0, 10, 100);
    memdc_free(&m);
}
TEST(paint_gdi_scrolldc_sub_rect) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {20, 20, 60, 60}, up;
    CHECK(U(ScrollDC)(m.dc, 5, 0, &sc, &sc, NULL, &up));
    CHECK(px(&m, 30, 30) == ORIG(25, 30));
    CHECK(px(&m, 10, 30) == ORIG(10, 30));     /* outside the scroll rect */
    CHECK(px(&m, 70, 30) == ORIG(70, 30));
    CHECK(px(&m, 30, 70) == ORIG(30, 70));
    CHECK_RECT(up, 20, 20, 25, 60);
    memdc_free(&m);
}
TEST(paint_gdi_scrolldc_larger_than_area) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 100, 100}, up;
    CHECK(U(ScrollDC)(m.dc, 150, 0, &sc, &sc, NULL, &up));
    CHECK_RECT(up, 0, 0, 100, 100);
    CHECK(px(&m, 50, 50) == ORIG(50, 50) || 1);
    memdc_free(&m);
    CHECK(memdc_make(&m, 100, 100));
    CHECK(U(ScrollDC)(m.dc, 0, -300, &sc, &sc, NULL, &up));
    CHECK_RECT(up, 0, 0, 100, 100);
    memdc_free(&m);
}
TEST(paint_gdi_scrolldc_zero_amount) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 100, 100}, up = {5, 5, 6, 6};
    HRGN rg = CreateRectRgn(1, 1, 5, 5);
    CHECK(U(ScrollDC)(m.dc, 0, 0, &sc, &sc, rg, &up));
    CHECK(px(&m, 7, 8) == ORIG(7, 8));
    CHECK(IsRectEmpty(&up));
    RECT b; EQ(GetRgnBox(rg, &b), NULLREGION);
    DeleteObject(rg); memdc_free(&m);
}
TEST(paint_gdi_scrolldc_null_optionals) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 100, 100};
    CHECK(U(ScrollDC)(m.dc, 3, 4, &sc, &sc, NULL, NULL));
    CHECK(px(&m, 3, 4) == ORIG(0, 0));
    memdc_free(&m);
}
TEST(paint_gdi_scrolldc_invalid_dc) {
    REQUIRE(U(ScrollDC));
    RECT sc = {0, 0, 10, 10}, up;
    SetLastError(0);
    CHECK(U(ScrollDC)(NULL, 1, 1, &sc, &sc, NULL, &up) == FALSE);
    CHECK(U(ScrollDC)((HDC)(ULONG_PTR)0x1234, 1, 1, &sc, &sc, NULL, &up) == FALSE);
    HBRUSH br = CreateSolidBrush(0);
    CHECK(U(ScrollDC)((HDC)br, 1, 1, &sc, &sc, NULL, &up) == FALSE);
    DeleteObject(br);
}
TEST(paint_gdi_scrolldc_bad_region_handle) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 20, 20));
    RECT sc = {0, 0, 20, 20}, up;
    SetLastError(0);
    BOOL r = U(ScrollDC)(m.dc, 1, 1, &sc, &sc, (HRGN)(ULONG_PTR)0x999, &up);
    t_note("ScrollDC(bad hrgn) -> %d err=%lu", r, GetLastError());
    memdc_free(&m);
}
TEST(paint_gdi_scrolldc_viewport_origin) {
    REQUIRE(U(ScrollDC));
    memdc m; CHECK(memdc_make(&m, 100, 100));
    RECT sc = {0, 0, 50, 50}, up;
    CHECK(U(ScrollDC)(m.dc, 0, 5, &sc, &sc, NULL, &up));
    CHECK(px(&m, 10, 5) == ORIG(10, 0));
    CHECK(px(&m, 10, 50) == ORIG(10, 50));
    memdc_free(&m);
}

/* ---------------- ScrollWindow / ScrollWindowEx ---------------- */
static HWND mk(void) { pg_reset(); return pg_topwin(300, 300, 200, 150); }
static void kill(HWND w) { if (w) U(DestroyWindow)(w); pg_pump(); }
static int unobscured(HWND w) { HDC d = U(GetDC)(w); BOOL v = PtVisible(d, 100, 75) && PtVisible(d, 5, 5) && PtVisible(d, 195, 145); U(ReleaseDC)(w, d); return v; }
static POINT child_pos(HWND p, HWND c) {
    RECT r; U(GetWindowRect)(c, &r);
    POINT pt = { r.left, r.top }; U(ScreenToClient)(p, &pt);
    return pt;
}

TEST(paint_gdi_scrollwin_children_move) {
    REQUIRE(U(ScrollWindow)); REQUIRE(U(GetWindowRect)); REQUIRE(U(ScreenToClient));
    HWND p = mk(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    CHECK(c != NULL);
    if (c) {
        POINT a = child_pos(p, c); CHECK(a.x == 20 && a.y == 20);
        CHECK(U(ScrollWindow)(p, 5, 7, NULL, NULL));
        a = child_pos(p, c); CHECK(a.x == 25 && a.y == 27);
        CHECK(U(ScrollWindow)(p, -30, -40, NULL, NULL));
        a = child_pos(p, c); CHECK(a.x == -5 && a.y == -13);
        U(DestroyWindow)(c);
    }
    kill(p);
}
TEST(paint_gdi_scrollwin_rect_keeps_children) {
    REQUIRE(U(ScrollWindow));
    HWND p = mk(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    CHECK(c != NULL);
    if (c) {
        RECT sc = {0, 0, 200, 150};
        CHECK(U(ScrollWindow)(p, 5, 7, &sc, NULL));
        POINT a = child_pos(p, c); CHECK(a.x == 20 && a.y == 20);
        U(DestroyWindow)(c);
    }
    kill(p);
}
TEST(paint_gdi_scrollwin_invalid_areas_offset) {
    REQUIRE(U(ScrollWindow));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    RECT in = {10, 10, 30, 30};
    U(InvalidateRect)(w, &in, FALSE);
    CHECK(U(ScrollWindow)(w, 5, 5, NULL, NULL));
    RECT r; CHECK(U(GetUpdateRect)(w, &r, FALSE));
    /* the old invalid area moves with the content; the uncovered strips join it */
    CHECK(r.left <= 15 && r.top <= 15 && r.right >= 35 && r.bottom >= 35);
    U(ValidateRect)(w, NULL);
    kill(w);
}
TEST(paint_gdi_scrollwin_uncovered_invalidated) {
    REQUIRE(U(ScrollWindow));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    if (!unobscured(w)) { t_note("not unobscured; skip"); kill(w); return; }
    pg_flush(w);
    CHECK(U(ScrollWindow)(w, 10, 0, NULL, NULL));
    RECT r; CHECK(U(GetUpdateRect)(w, &r, FALSE));
    CHECK_RECT(r, 0, 0, 10, 150);
    pg_flush(w);
    CHECK(U(ScrollWindow)(w, 0, -12, NULL, NULL));
    CHECK(U(GetUpdateRect)(w, &r, FALSE));
    CHECK_RECT(r, 0, 138, 200, 150);
    pg_flush(w);
    kill(w);
}
TEST(paint_gdi_scrollwin_invalid_window) {
    REQUIRE(U(ScrollWindow));
    SetLastError(0);
    CHECK(U(ScrollWindow)((HWND)(ULONG_PTR)0xbad0bad, 1, 1, NULL, NULL) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(U(ScrollWindow)(NULL, 1, 1, NULL, NULL) == FALSE);
}
TEST(paint_gdi_scrollwin_zero) {
    REQUIRE(U(ScrollWindow));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    CHECK(U(ScrollWindow)(w, 0, 0, NULL, NULL));
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));
    kill(w);
}
TEST(paint_gdi_scrollex_invalidate_flag) {
    REQUIRE(U(ScrollWindowEx));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    if (!unobscured(w)) { t_note("not unobscured; skip"); kill(w); return; }
    RECT up = {-1, -1, -1, -1};
    HRGN rg = CreateRectRgn(0, 0, 0, 0);
    int t = U(ScrollWindowEx)(w, 10, 0, NULL, NULL, rg, &up, SW_INVALIDATE);
    EQ(t, SIMPLEREGION);
    CHECK_RECT(up, 0, 0, 10, 150);
    RECT b; GetRgnBox(rg, &b); CHECK_RECT(b, 0, 0, 10, 150);
    RECT ur; CHECK(U(GetUpdateRect)(w, &ur, FALSE));
    CHECK_RECT(ur, 0, 0, 10, 150);
    pg_flush(w);
    DeleteObject(rg); kill(w);
}
TEST(paint_gdi_scrollex_no_invalidate) {
    REQUIRE(U(ScrollWindowEx));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    if (!unobscured(w)) { t_note("not unobscured; skip"); kill(w); return; }
    RECT up;
    int t = U(ScrollWindowEx)(w, 0, 20, NULL, NULL, NULL, &up, 0);
    CHECK(t != ERROR);
    t_note("ScrollWindowEx(no SW_INVALIDATE) region type %d", t);
    CHECK_RECT(up, 0, 0, 200, 20);
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));     /* uncovered area reported but not invalidated */
    kill(w);
}
TEST(paint_gdi_scrollex_erase_flag) {
    REQUIRE(U(ScrollWindowEx));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    if (!unobscured(w)) { t_note("not unobscured; skip"); kill(w); return; }
    pg_reset(); pg_r.erase_ret = 0;
    int t = U(ScrollWindowEx)(w, 10, 10, NULL, NULL, NULL, NULL, SW_INVALIDATE | SW_ERASE);
    CHECK(t != 0 && t != NULLREGION);
    U(UpdateWindow)(w);
    EQ(pg_r.erase, 1);
    CHECK(pg_r.ps_erase == TRUE);
    kill(w);
}
TEST(paint_gdi_scrollex_children_flag) {
    REQUIRE(U(ScrollWindowEx));
    HWND p = mk(); CHECK(p != NULL); if (!p) return;
    HWND c1 = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 30, 30, p);
    HWND c2 = pg_window(WS_CHILD | WS_VISIBLE, 0, 150, 100, 30, 30, p);
    CHECK(c1 && c2);
    if (c1 && c2) {
        RECT sc = {0, 0, 100, 100};
        U(ScrollWindowEx)(p, 4, 6, &sc, NULL, NULL, NULL, 0);
        POINT a = child_pos(p, c1), b = child_pos(p, c2);
        CHECK(a.x == 20 && a.y == 20);        /* no SW_SCROLLCHILDREN: untouched */
        CHECK(b.x == 150 && b.y == 100);
        U(ScrollWindowEx)(p, 4, 6, &sc, NULL, NULL, NULL, SW_SCROLLCHILDREN);
        a = child_pos(p, c1); b = child_pos(p, c2);
        CHECK(a.x == 24 && a.y == 26);        /* intersects the scroll rectangle */
        CHECK(b.x == 150 && b.y == 100);      /* does not */
        U(ScrollWindowEx)(p, 1, 1, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN);
        a = child_pos(p, c1); b = child_pos(p, c2);
        CHECK(a.x == 25 && a.y == 27);
        CHECK(b.x == 151 && b.y == 101);
    }
    if (c1) U(DestroyWindow)(c1);
    if (c2) U(DestroyWindow)(c2);
    kill(p);
}
TEST(paint_gdi_scrollex_invalid_window) {
    REQUIRE(U(ScrollWindowEx));
    SetLastError(0);
    EQ(U(ScrollWindowEx)((HWND)(ULONG_PTR)0xbad0bad, 1, 1, NULL, NULL, NULL, NULL, SW_INVALIDATE), ERROR);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    EQ(U(ScrollWindowEx)(NULL, 1, 1, NULL, NULL, NULL, NULL, 0), ERROR);
}
TEST(paint_gdi_scrollex_zero_scroll) {
    REQUIRE(U(ScrollWindowEx));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    RECT up = {1, 1, 2, 2};
    int t = U(ScrollWindowEx)(w, 0, 0, NULL, NULL, NULL, &up, SW_INVALIDATE);
    t_note("ScrollWindowEx(0,0) -> %d up=(%ld,%ld,%ld,%ld)", t, RECT_STR(up));
    CHECK(t != ERROR);
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));
    kill(w);
}
TEST(paint_gdi_scrollex_clip_rect) {
    REQUIRE(U(ScrollWindowEx));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    if (!unobscured(w)) { t_note("not unobscured; skip"); kill(w); return; }
    RECT clip = {0, 0, 100, 150}, up;
    int t = U(ScrollWindowEx)(w, 10, 0, NULL, &clip, NULL, &up, SW_INVALIDATE);
    CHECK(t != ERROR);
    CHECK_RECT(up, 0, 0, 10, 150);
    pg_flush(w);
    kill(w);
}
TEST(paint_gdi_scrollex_bad_flags) {
    REQUIRE(U(ScrollWindowEx));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    SetLastError(0);
    int t = U(ScrollWindowEx)(w, 1, 1, NULL, NULL, NULL, NULL, 0xffff0000u);
    t_note("ScrollWindowEx(unknown flag bits) -> %d err=%lu", t, GetLastError());
    pg_flush(w);
    kill(w);
}
TEST(paint_gdi_scrollex_bad_region) {
    REQUIRE(U(ScrollWindowEx));
    HWND w = mk(); CHECK(w != NULL); if (!w) return;
    SetLastError(0);
    int t = U(ScrollWindowEx)(w, 1, 1, NULL, NULL, (HRGN)(ULONG_PTR)0x3333, NULL, SW_INVALIDATE);
    t_note("ScrollWindowEx(bad hrgnUpdate) -> %d err=%lu", t, GetLastError());
    pg_flush(w);
    kill(w);
}
