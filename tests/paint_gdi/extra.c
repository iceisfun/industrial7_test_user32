#include "pg.h"
#include <limits.h>

/* ------------------------------------------------------------------
 * Reference models for the rectangle helpers (written from the observed
 * documented semantics) used to cross-check user32 on many random inputs.
 * ------------------------------------------------------------------ */
static int r_empty(const RECT *r) { return r->right <= r->left || r->bottom <= r->top; }
static int r_eq(const RECT *a, const RECT *b) { return a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom; }
static int r_isect(RECT *d, const RECT *a, const RECT *b) {
    RECT t;
    t.left = a->left > b->left ? a->left : b->left;
    t.right = a->right < b->right ? a->right : b->right;
    t.top = a->top > b->top ? a->top : b->top;
    t.bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (t.left < t.right && t.top < t.bottom) { *d = t; return 1; }
    memset(d, 0, sizeof *d);
    return 0;
}
static int r_union(RECT *d, const RECT *a, const RECT *b) {
    int ea = r_empty(a), eb = r_empty(b);
    if (ea && eb) { memset(d, 0, sizeof *d); return 0; }
    if (ea) { *d = *b; return 1; }
    if (eb) { *d = *a; return 1; }
    d->left = a->left < b->left ? a->left : b->left;
    d->top = a->top < b->top ? a->top : b->top;
    d->right = a->right > b->right ? a->right : b->right;
    d->bottom = a->bottom > b->bottom ? a->bottom : b->bottom;
    return 1;
}
static int r_sub(RECT *d, const RECT *a, const RECT *b) {
    RECT i; int hit = r_isect(&i, a, b);
    *d = *a;
    if (hit) {
        int n = (i.left <= a->left) + (i.top <= a->top) + (i.right >= a->right) + (i.bottom >= a->bottom);
        if (n == 4) { memset(d, 0, sizeof *d); return 0; }
        if (n == 3) {
            if (a->left < i.left) d->right = i.left;
            else if (i.right < a->right) d->left = i.right;
            else if (a->top < i.top) d->bottom = i.top;
            else if (i.bottom < a->bottom) d->top = i.bottom;
        }
    }
    return d->left < d->right && d->top < d->bottom;
}
static unsigned g_seed;
static int rnd(int range) { g_seed = g_seed * 1664525u + 1013904223u; return (int)((g_seed >> 12) % (unsigned)range); }
static RECT rnd_rect(int span) { RECT r = { rnd(span) - span / 2, rnd(span) - span / 2, rnd(span) - span / 2, rnd(span) - span / 2 }; return r; }
/* biased towards touching edges so that all the branches are hit */
static RECT rnd_rect_small(void) { RECT r = { rnd(6), rnd(6), rnd(6) + 2, rnd(6) + 2 }; return r; }

TEST(paint_gdi_rectx_intersect_vs_model) {
    REQUIRE(U(IntersectRect));
    g_seed = 1;
    int bad = 0;
    for (int i = 0; i < 6000 && bad < 5; i++) {
        RECT a = (i & 1) ? rnd_rect(40) : rnd_rect_small(), b = (i & 1) ? rnd_rect(40) : rnd_rect_small(), d = {55, 55, 55, 55}, w;
        BOOL r = U(IntersectRect)(&d, &a, &b);
        int wr = r_isect(&w, &a, &b);
        if (!!r != wr || !r_eq(&d, &w)) { bad++; CHECKF(0, "iter %d a=(%ld,%ld,%ld,%ld) b=(%ld,%ld,%ld,%ld) got %d (%ld,%ld,%ld,%ld) want %d (%ld,%ld,%ld,%ld)", i, RECT_STR(a), RECT_STR(b), r, RECT_STR(d), wr, RECT_STR(w)); }
    }
    CHECK(bad == 0);
}
TEST(paint_gdi_rectx_union_vs_model) {
    REQUIRE(U(UnionRect));
    g_seed = 2;
    int bad = 0;
    for (int i = 0; i < 6000 && bad < 5; i++) {
        RECT a = (i & 1) ? rnd_rect(40) : rnd_rect_small(), b = (i & 1) ? rnd_rect(40) : rnd_rect_small(), d = {55, 55, 55, 55}, w;
        BOOL r = U(UnionRect)(&d, &a, &b);
        int wr = r_union(&w, &a, &b);
        if (!!r != wr || !r_eq(&d, &w)) { bad++; CHECKF(0, "iter %d a=(%ld,%ld,%ld,%ld) b=(%ld,%ld,%ld,%ld) got %d (%ld,%ld,%ld,%ld) want %d (%ld,%ld,%ld,%ld)", i, RECT_STR(a), RECT_STR(b), r, RECT_STR(d), wr, RECT_STR(w)); }
    }
    CHECK(bad == 0);
}
TEST(paint_gdi_rectx_subtract_vs_model) {
    REQUIRE(U(SubtractRect));
    g_seed = 3;
    int bad = 0;
    for (int i = 0; i < 6000 && bad < 5; i++) {
        RECT a = (i & 1) ? rnd_rect(40) : rnd_rect_small(), b = (i & 1) ? rnd_rect(40) : rnd_rect_small(), d = {55, 55, 55, 55}, w;
        BOOL r = U(SubtractRect)(&d, &a, &b);
        int wr = r_sub(&w, &a, &b);
        if (!!r != wr || !r_eq(&d, &w)) { bad++; CHECKF(0, "iter %d a=(%ld,%ld,%ld,%ld) b=(%ld,%ld,%ld,%ld) got %d (%ld,%ld,%ld,%ld) want %d (%ld,%ld,%ld,%ld)", i, RECT_STR(a), RECT_STR(b), r, RECT_STR(d), wr, RECT_STR(w)); }
    }
    CHECK(bad == 0);
}
TEST(paint_gdi_rectx_ptinrect_vs_model) {
    REQUIRE(U(PtInRect));
    g_seed = 4;
    int bad = 0;
    for (int i = 0; i < 8000 && bad < 5; i++) {
        RECT a = (i & 1) ? rnd_rect(30) : rnd_rect_small();
        POINT p = { rnd(30) - 15, rnd(30) - 15 };
        int want = a.left <= p.x && p.x < a.right && a.top <= p.y && p.y < a.bottom;
        BOOL r = U(PtInRect)(&a, p);
        if (!!r != want) { bad++; CHECKF(0, "iter %d rect (%ld,%ld,%ld,%ld) pt (%ld,%ld) got %d", i, RECT_STR(a), p.x, p.y, r); }
    }
    CHECK(bad == 0);
}
TEST(paint_gdi_rectx_isempty_equal_vs_model) {
    REQUIRE(U(IsRectEmpty)); REQUIRE(U(EqualRect));
    g_seed = 5;
    int bad = 0;
    for (int i = 0; i < 4000 && bad < 5; i++) {
        RECT a = rnd_rect_small(), b = (i % 3 == 0) ? a : rnd_rect_small();
        if (i % 7 == 0) { a.right = a.left - 1; }
        if (!!U(IsRectEmpty)(&a) != r_empty(&a)) { bad++; CHECKF(0, "empty iter %d", i); }
        if (!!U(EqualRect)(&a, &b) != r_eq(&a, &b)) { bad++; CHECKF(0, "equal iter %d", i); }
    }
    CHECK(bad == 0);
}
TEST(paint_gdi_rectx_inflate_offset_inverse) {
    REQUIRE(U(InflateRect)); REQUIRE(U(OffsetRect));
    g_seed = 6;
    for (int i = 0; i < 500; i++) {
        RECT a = rnd_rect(200), o = a;
        int dx = rnd(100) - 50, dy = rnd(100) - 50;
        CHECK(U(InflateRect)(&o, dx, dy));
        CHECK(U(InflateRect)(&o, -dx, -dy));
        CHECK(r_eq(&o, &a));
        CHECK(U(OffsetRect)(&o, dx, dy));
        CHECK(U(OffsetRect)(&o, -dx, -dy));
        CHECK(r_eq(&o, &a));
    }
}
TEST(paint_gdi_rectx_copy_set_roundtrip) {
    REQUIRE(U(SetRect)); REQUIRE(U(CopyRect)); REQUIRE(U(EqualRect));
    g_seed = 7;
    for (int i = 0; i < 300; i++) {
        RECT a, b, c;
        CHECK(U(SetRect)(&a, rnd(1000) - 500, rnd(1000) - 500, rnd(1000) - 500, rnd(1000) - 500));
        CHECK(U(CopyRect)(&b, &a));
        CHECK(U(EqualRect)(&a, &b));
        CHECK(U(CopyRect)(&c, &b));
        CHECK(r_eq(&a, &c));
    }
}
TEST(paint_gdi_rectx_union_dest_aliases_src2) {
    REQUIRE(U(UnionRect)); REQUIRE(U(IntersectRect)); REQUIRE(U(SubtractRect));
    RECT a = {0, 0, 5, 5}, b = {3, 3, 9, 9};
    CHECK(U(UnionRect)(&b, &a, &b));
    CHECK_RECT(b, 0, 0, 9, 9);
    RECT c = {0, 0, 5, 5}, d = {3, 3, 9, 9};
    CHECK(U(IntersectRect)(&d, &c, &d));
    CHECK_RECT(d, 3, 3, 5, 5);
    RECT e = {0, 0, 10, 10}, f = {5, -5, 20, 20};
    CHECK(U(SubtractRect)(&f, &e, &f));
    CHECK_RECT(f, 0, 0, 5, 10);
}
TEST(paint_gdi_rectx_all_same_object) {
    REQUIRE(U(UnionRect)); REQUIRE(U(IntersectRect));
    RECT a = {1, 2, 8, 9};
    CHECK(U(UnionRect)(&a, &a, &a));
    CHECK_RECT(a, 1, 2, 8, 9);
    CHECK(U(IntersectRect)(&a, &a, &a));
    CHECK_RECT(a, 1, 2, 8, 9);
    RECT e = {4, 4, 4, 4};
    CHECK(!U(UnionRect)(&e, &e, &e));
    CHECK_RECT(e, 0, 0, 0, 0);
    RECT e2 = {4, 4, 4, 4};
    CHECK(!U(IntersectRect)(&e2, &e2, &e2));
    CHECK_RECT(e2, 0, 0, 0, 0);
}
TEST(paint_gdi_rectx_subtract_strips) {
    REQUIRE(U(SubtractRect));
    RECT a = {0, 0, 10, 10}, d;
    /* subtrahend spans the full height on the right: left part remains */
    CHECK(U(SubtractRect)(&d, &a, &(RECT){6, -1, 50, 50})); CHECK_RECT(d, 0, 0, 6, 10);
    /* full height on the left */
    CHECK(U(SubtractRect)(&d, &a, &(RECT){-9, -1, 4, 50})); CHECK_RECT(d, 4, 0, 10, 10);
    /* full width at the top */
    CHECK(U(SubtractRect)(&d, &a, &(RECT){-9, -9, 50, 3})); CHECK_RECT(d, 0, 3, 10, 10);
    /* full width at the bottom */
    CHECK(U(SubtractRect)(&d, &a, &(RECT){-9, 7, 50, 50})); CHECK_RECT(d, 0, 0, 10, 7);
    /* spans height but touches neither edge horizontally: a hole in the middle cannot be expressed */
    CHECK(U(SubtractRect)(&d, &a, &(RECT){3, -1, 7, 11})); CHECK_RECT(d, 0, 0, 10, 10);
    /* spans width but touches neither edge vertically */
    CHECK(U(SubtractRect)(&d, &a, &(RECT){-1, 3, 11, 7})); CHECK_RECT(d, 0, 0, 10, 10);
}

/* ------------------------------------------------------------------
 * DPI awareness context encodings (ValidateDpiAwarenessContextEx).
 * ------------------------------------------------------------------ */
#define CTX(n) ((DPI_AWARENESS_CONTEXT)(LONG_PTR)(n))
TEST(paint_gdi_dpix_system_aware_raw_contexts) {
    pg_gui();
    REQUIRE(U(IsValidDpiAwarenessContext)); REQUIRE(U(GetDpiFromDpiAwarenessContext)); REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    static const UINT dpis[] = { 96, 120, 144, 168, 192, 240, 288, 384, 480 };
    for (unsigned i = 0; i < sizeof dpis / sizeof dpis[0]; i++) {
        LONG_PTR raw = (LONG_PTR)((dpis[i] << 8) | 0x11);
        CHECKF(U(IsValidDpiAwarenessContext)(CTX(raw)), "raw 0x%llx", (long long)raw);
        EQ(U(GetDpiFromDpiAwarenessContext)(CTX(raw)), dpis[i]);
        EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(raw)), DPI_AWARENESS_SYSTEM_AWARE);
    }
}
TEST(paint_gdi_dpix_awareness_nibble) {
    pg_gui();
    REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(0x6010)), DPI_AWARENESS_UNAWARE);
    EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(0x40006010)), DPI_AWARENESS_UNAWARE);
    EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(0x12)), DPI_AWARENESS_PER_MONITOR_AWARE);
    EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(0x22)), DPI_AWARENESS_PER_MONITOR_AWARE);
    EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(0x6011)), DPI_AWARENESS_SYSTEM_AWARE);
}
TEST(paint_gdi_dpix_dpi_from_fixed_contexts) {
    pg_gui();
    REQUIRE(U(GetDpiFromDpiAwarenessContext));
    EQ(U(GetDpiFromDpiAwarenessContext)(CTX(0x6010)), 96);
    EQ(U(GetDpiFromDpiAwarenessContext)(CTX(0x40006010)), 96);
    EQ(U(GetDpiFromDpiAwarenessContext)(CTX(0x12)), 0);
    EQ(U(GetDpiFromDpiAwarenessContext)(CTX(0x22)), 0);
}
TEST(paint_gdi_dpix_invalid_sets_error_87) {
    pg_gui();
    REQUIRE(U(IsValidDpiAwarenessContext)); REQUIRE(U(GetAwarenessFromDpiAwarenessContext)); REQUIRE(U(GetDpiFromDpiAwarenessContext));
    static const LONG_PTR bad[] = { 0, 1, -6, -7, 0x13000, 0x7777, 0x10000, 0x6000, 0x10006010 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(!U(IsValidDpiAwarenessContext)(CTX(bad[i])), "valid? 0x%llx", (long long)bad[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        SetLastError(0);
        EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(bad[i])), -1);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        SetLastError(0);
        EQ(U(GetDpiFromDpiAwarenessContext)(CTX(bad[i])), 0);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}
TEST(paint_gdi_dpix_valid_does_not_set_error) {
    pg_gui();
    REQUIRE(U(IsValidDpiAwarenessContext));
    SetLastError(0xabcd);
    CHECK(U(IsValidDpiAwarenessContext)(CTX(-3)));
    CHECK(GetLastError() == 0xabcd);
}
TEST(paint_gdi_dpix_system_pseudo_equals_raw) {
    pg_gui();
    REQUIRE(U(AreDpiAwarenessContextsEqual)); REQUIRE(U(GetDpiFromDpiAwarenessContext));
    UINT d = U(GetDpiFromDpiAwarenessContext)(CTX(-2));
    CHECK(d >= 96);
    CHECK(U(AreDpiAwarenessContextsEqual)(CTX(-2), CTX((d << 8) | 0x11)));
    CHECK(!U(AreDpiAwarenessContextsEqual)(CTX(-2), CTX(-3)));
    CHECK(!U(AreDpiAwarenessContextsEqual)(CTX(-2), CTX(-1)));
}
TEST(paint_gdi_dpix_equal_symmetric) {
    pg_gui();
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    static const LONG_PTR v[] = { -1, -2, -3, -4, -5, 0x6010, 0x12, 0x22, 0x40006010, 0, -6 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++)
        for (unsigned j = 0; j < sizeof v / sizeof v[0]; j++)
            CHECKF(U(AreDpiAwarenessContextsEqual)(CTX(v[i]), CTX(v[j])) == U(AreDpiAwarenessContextsEqual)(CTX(v[j]), CTX(v[i])), "%u/%u", i, j);
}
TEST(paint_gdi_dpix_adjust_null_rect_no_border) {
    pg_gui();
    REQUIRE(U(AdjustWindowRectExForDpi));
    /* a window style without any non-client area needs no write through the rectangle */
    HWND w = pg_window(WS_POPUP, 0, 0, 0, 10, 10, NULL);   /* make sure the thread is a GUI thread */
    CHECK(w != NULL);
    SetLastError(0);
    BOOL r = U(AdjustWindowRectExForDpi)(NULL, WS_POPUP, FALSE, 0, 96);
    t_note("AdjustWindowRectExForDpi(NULL rect, WS_POPUP) -> %d", r);
    if (w) U(DestroyWindow)(w);
}
TEST(paint_gdi_dpix_adjust_zero_dpi_all_styles) {
    pg_gui();
    REQUIRE(U(AdjustWindowRectExForDpi)); pg_gui();
    static const DWORD st[] = { 0, WS_POPUP, WS_BORDER, WS_CAPTION, WS_OVERLAPPEDWINDOW, WS_THICKFRAME | WS_POPUP };
    for (unsigned i = 0; i < sizeof st / sizeof st[0]; i++) {
        RECT r = {1, 2, 3, 4};
        SetLastError(0);
        CHECKF(U(AdjustWindowRectExForDpi)(&r, st[i], FALSE, 0, 0) == FALSE, "style 0x%lx", st[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        CHECK_RECT(r, 1, 2, 3, 4);
    }
}
TEST(paint_gdi_dpix_metrics_match_system_dpi) {
    pg_gui();
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(GetSystemMetricsForDpi)); REQUIRE(U(GetDpiForSystem));
    UINT sys = U(GetDpiForSystem)();
    static const int idx[] = { SM_CXVSCROLL, SM_CYHSCROLL, SM_CYCAPTION, SM_CXSIZE, SM_CYSIZE, SM_CYSMCAPTION, SM_CXSMSIZE, SM_CYSMSIZE,
        SM_CXICON, SM_CYICON, SM_CXSMICON, SM_CYSMICON, SM_CXCURSOR, SM_CYCURSOR, SM_CYMENU, SM_CXMENUSIZE, SM_CYMENUSIZE,
        SM_CXSIZEFRAME, SM_CYSIZEFRAME, SM_CXEDGE, SM_CYEDGE, SM_CXFIXEDFRAME, SM_CYFIXEDFRAME, SM_CXBORDER, SM_CYBORDER, SM_CXPADDEDBORDER };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        int a = U(GetSystemMetrics)(idx[i]), b = U(GetSystemMetricsForDpi)(idx[i], sys);
        if (a != b) t_note("sm %d: GetSystemMetrics %d, ForDpi(%u) %d", idx[i], a, sys, b);
    }
    /* at the system DPI the two agree for the main layout metrics */
    CHECK(U(GetSystemMetrics)(SM_CXVSCROLL) == U(GetSystemMetricsForDpi)(SM_CXVSCROLL, sys));
    CHECK(U(GetSystemMetrics)(SM_CYCAPTION) == U(GetSystemMetricsForDpi)(SM_CYCAPTION, sys));
}

/* ------------------------------------------------------------------
 * DrawText corner cases that are visible in the user32 thunks
 * ------------------------------------------------------------------ */
static HDC mk_dc(HBITMAP *bm, HGDIOBJ *oldb, HGDIOBJ *oldf) {
    HDC dc = CreateCompatibleDC(NULL), sc = GetDC(NULL);
    *bm = CreateCompatibleBitmap(sc, 64, 32);
    ReleaseDC(NULL, sc);
    *oldb = SelectObject(dc, *bm);
    *oldf = SelectObject(dc, GetStockObject(SYSTEM_FIXED_FONT));
    return dc;
}
static void rel_dc(HDC dc, HBITMAP bm, HGDIOBJ ob, HGDIOBJ of) { SelectObject(dc, of); SelectObject(dc, ob); DeleteDC(dc); DeleteObject(bm); }
TEST(paint_gdi_dtx_negative_count_rejected) {
    REQUIRE(U(DrawTextA)); REQUIRE(U(DrawTextW)); REQUIRE(U(DrawTextExA)); REQUIRE(U(DrawTextExW));
    HBITMAP bm; HGDIOBJ ob, of; HDC dc = mk_dc(&bm, &ob, &of);
    RECT r = {0, 0, 100, 50};
    static const int cnt[] = { -2, -3, -100, INT_MIN };
    for (unsigned i = 0; i < 4; i++) {
        r = (RECT){0, 0, 100, 50};
        CHECKF(U(DrawTextA)(dc, "abc", cnt[i], &r, DT_CALCRECT) == 0, "A cnt %d", cnt[i]);
        CHECKF(U(DrawTextExA)(dc, (LPSTR)"abc", cnt[i], &r, DT_CALCRECT, NULL) == 0, "ExA cnt %d", cnt[i]);
        CHECKF(r.right == 100 && r.bottom == 50, "rect modified for cnt %d", cnt[i]);
    }
    rel_dc(dc, bm, ob, of);
}
TEST(paint_gdi_dtx_minus_one_is_nul_terminated) {
    REQUIRE(U(DrawTextA)); REQUIRE(U(DrawTextW));
    HBITMAP bm; HGDIOBJ ob, of; HDC dc = mk_dc(&bm, &ob, &of);
    TEXTMETRICA tm; GetTextMetricsA(dc, &tm);
    RECT r = {0, 0, 400, 100};
    int h = U(DrawTextA)(dc, "hello", -1, &r, DT_CALCRECT | DT_SINGLELINE);
    EQ(h, tm.tmHeight);
    CHECK(r.right == 5 * tm.tmAveCharWidth);
    r = (RECT){0, 0, 400, 100};
    h = U(DrawTextW)(dc, L"hello", -1, &r, DT_CALCRECT | DT_SINGLELINE);
    EQ(h, tm.tmHeight);
    CHECK(r.right == 5 * tm.tmAveCharWidth);
    rel_dc(dc, bm, ob, of);
}
TEST(paint_gdi_dtx_calcrect_leaves_bitmap) {
    REQUIRE(U(DrawTextW));
    HBITMAP bm; HGDIOBJ ob, of; HDC dc = mk_dc(&bm, &ob, &of);
    PatBlt(dc, 0, 0, 64, 32, WHITENESS);
    RECT r = {0, 0, 64, 32};
    U(DrawTextW)(dc, L"XXXXXXXX", -1, &r, DT_CALCRECT);
    int dirty = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) if (GetPixel(dc, x, y) != RGB(255, 255, 255)) dirty++;
    CHECK(dirty == 0);
    rel_dc(dc, bm, ob, of);
}
TEST(paint_gdi_dtx_return_is_height_for_each_line_count) {
    REQUIRE(U(DrawTextA));
    HBITMAP bm; HGDIOBJ ob, of; HDC dc = mk_dc(&bm, &ob, &of);
    TEXTMETRICA tm; GetTextMetricsA(dc, &tm);
    static const char *s[] = { "a", "a\nb", "a\nb\nc", "a\nb\nc\nd", "a\nb\nc\nd\ne" };
    for (int n = 1; n <= 5; n++) {
        RECT r = {0, 0, 300, 300};
        int h = U(DrawTextA)(dc, s[n - 1], -1, &r, DT_CALCRECT);
        CHECKF(h == n * tm.tmHeight, "%d lines -> %d (line %ld)", n, h, tm.tmHeight);
        CHECKF(r.bottom - r.top == h, "rect height %ld", r.bottom - r.top);
    }
    rel_dc(dc, bm, ob, of);
}
TEST(paint_gdi_dtx_prefix_flags_calc_widths) {
    REQUIRE(U(DrawTextA));
    HBITMAP bm; HGDIOBJ ob, of; HDC dc = mk_dc(&bm, &ob, &of);
    TEXTMETRICA tm; GetTextMetricsA(dc, &tm);
    static const struct { const char *s; UINT fl; int chars; } t[] = {
        { "&&", DT_SINGLELINE, 1 }, { "&&", DT_SINGLELINE | DT_NOPREFIX, 2 }, { "a&b", DT_SINGLELINE, 2 },
        { "&&&a", DT_SINGLELINE, 2 }, { "ab", DT_SINGLELINE | DT_HIDEPREFIX, 2 }, { "a&b", DT_SINGLELINE | DT_HIDEPREFIX, 2 },
        { "a&b", DT_SINGLELINE | DT_PREFIXONLY, 2 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT r = {0, 0, 400, 100};
        U(DrawTextA)(dc, t[i].s, -1, &r, t[i].fl | DT_CALCRECT);
        if (t[i].fl & DT_PREFIXONLY) { t_note("PREFIXONLY calc width %ld", r.right); continue; }
        CHECKF(r.right == t[i].chars * tm.tmAveCharWidth, "case %u '%s': width %ld want %d", i, t[i].s, r.right, t[i].chars * tm.tmAveCharWidth);
    }
    rel_dc(dc, bm, ob, of);
}
TEST(paint_gdi_dtx_tabstop_format_byte) {
    REQUIRE(U(DrawTextA));
    HBITMAP bm; HGDIOBJ ob, of; HDC dc = mk_dc(&bm, &ob, &of);
    TEXTMETRICA tm; GetTextMetricsA(dc, &tm);
    /* tab stops of 2 characters through the DT_TABSTOP high byte; calc rect with the flag is documented as unsupported, so only draw */
    RECT r = {0, 0, 64, 32};
    PatBlt(dc, 0, 0, 64, 32, WHITENESS);
    int h = U(DrawTextA)(dc, "a\tb", -1, &r, DT_SINGLELINE | DT_EXPANDTABS | DT_TABSTOP | (2 << 8));
    CHECK(h > 0);
    rel_dc(dc, bm, ob, of);
}

/* ------------------------------------------------------------------
 * Display configuration: virtual-mode-aware sizing and misc
 * ------------------------------------------------------------------ */
TEST(paint_gdi_dcx_virtual_mode_aware_sizes) {
    REQUIRE(U(GetDisplayConfigBufferSizes));
    UINT32 np = 0, nm = 0, np2 = 0, nm2 = 0;
    LONG r = U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &np, &nm);
    LONG r2 = U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE, &np2, &nm2);
    t_note("plain: %ld %u/%u; virtual aware: %ld %u/%u", r, np, nm, r2, np2, nm2);
    if (r == ERROR_SUCCESS) EQ(nm, np * 2);
    if (r2 == ERROR_SUCCESS) {
        EQ(nm2, np2 * 3);
        CHECK(np2 == np);
    }
}
TEST(paint_gdi_dcx_buffer_sizes_error_codes_stable) {
    REQUIRE(U(GetDisplayConfigBufferSizes));
    UINT32 np, nm;
    for (int i = 0; i < 20; i++) {
        EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, NULL, &nm), ERROR_INVALID_PARAMETER);
        EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &np, NULL), ERROR_INVALID_PARAMETER);
    }
}
TEST(paint_gdi_dcx_buffer_sizes_repeatable) {
    REQUIRE(U(GetDisplayConfigBufferSizes));
    UINT32 a = 0, b = 0, c = 0, d = 0;
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &a, &b), ERROR_SUCCESS);
    EQ(U(GetDisplayConfigBufferSizes)(QDC_ONLY_ACTIVE_PATHS, &c, &d), ERROR_SUCCESS);
    CHECK(a == c && b == d);
}

/* ------------------------------------------------------------------
 * Monitors: consistency of the three MonitorFrom* helpers
 * ------------------------------------------------------------------ */
TEST(paint_gdi_monx_from_point_matches_from_rect_1px) {
    REQUIRE(U(MonitorFromPoint)); REQUIRE(U(MonitorFromRect));
    static const POINT pts[] = { {0, 0}, {1, 1}, {100, 100}, {-1, -1}, {-5000, 20}, {20000, 20}, {500, 400} };
    for (unsigned i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        RECT r = { pts[i].x, pts[i].y, pts[i].x + 1, pts[i].y + 1 };
        HMONITOR a = U(MonitorFromPoint)(pts[i], MONITOR_DEFAULTTONULL), b = U(MonitorFromRect)(&r, MONITOR_DEFAULTTONULL);
        CHECKF(a == b, "point (%ld,%ld): %p vs %p", pts[i].x, pts[i].y, a, b);
        a = U(MonitorFromPoint)(pts[i], MONITOR_DEFAULTTONEAREST); b = U(MonitorFromRect)(&r, MONITOR_DEFAULTTONEAREST);
        CHECKF(a == b && a != NULL, "nearest (%ld,%ld)", pts[i].x, pts[i].y);
        a = U(MonitorFromPoint)(pts[i], MONITOR_DEFAULTTOPRIMARY); b = U(MonitorFromRect)(&r, MONITOR_DEFAULTTOPRIMARY);
        CHECKF(a == b && a != NULL, "primary (%ld,%ld)", pts[i].x, pts[i].y);
    }
}
TEST(paint_gdi_monx_nearest_is_real_monitor) {
    REQUIRE(U(MonitorFromPoint)); REQUIRE(U(GetMonitorInfoW)); REQUIRE(U(EnumDisplayMonitors));
    HMONITOR n = U(MonitorFromPoint)((POINT){-30000, 30000}, MONITOR_DEFAULTTONEAREST);
    CHECK(n != NULL);
    MONITORINFO mi = { sizeof mi };
    CHECK(U(GetMonitorInfoW)(n, &mi));
    CHECK(mi.rcMonitor.right > mi.rcMonitor.left);
}
TEST(paint_gdi_monx_monitor_info_stable_across_calls) {
    REQUIRE(U(MonitorFromPoint)); REQUIRE(U(GetMonitorInfoW));
    HMONITOR p = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW a, b;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    a.cbSize = b.cbSize = sizeof a;
    CHECK(U(GetMonitorInfoW)(p, (MONITORINFO *)&a));
    CHECK(U(GetMonitorInfoW)(p, (MONITORINFO *)&b));
    CHECK(!memcmp(&a, &b, sizeof a));
}
TEST(paint_gdi_monx_info_does_not_touch_ex_tail_for_small_size) {
    REQUIRE(U(MonitorFromPoint)); REQUIRE(U(GetMonitorInfoW)); REQUIRE(U(GetMonitorInfoA));
    HMONITOR p = U(MonitorFromPoint)((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXA a; memset(&a, 0x5a, sizeof a); a.cbSize = sizeof(MONITORINFO);
    CHECK(U(GetMonitorInfoA)(p, (MONITORINFO *)&a));
    CHECK((unsigned char)a.szDevice[0] == 0x5a && (unsigned char)a.szDevice[31] == 0x5a);
    MONITORINFOEXW w; memset(&w, 0x5a, sizeof w); w.cbSize = sizeof(MONITORINFO);
    CHECK(U(GetMonitorInfoW)(p, (MONITORINFO *)&w));
    CHECK(w.szDevice[0] == 0x5a5a && w.szDevice[31] == 0x5a5a);
}

/* ------------------------------------------------------------------
 * System colours
 * ------------------------------------------------------------------ */
TEST(paint_gdi_colx_getsyscolor_error_code) {
    REQUIRE(U(GetSysColor));
    static const int bad[] = { 31, 32, 64, 1000, -1, INT_MIN, INT_MAX };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(U(GetSysColor)(bad[i]) == 0, "index %d", bad[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}
TEST(paint_gdi_colx_getsyscolor_valid_leaves_error) {
    REQUIRE(U(GetSysColor));
    SetLastError(0x1357);
    for (int i = 0; i <= 30; i++) U(GetSysColor)(i);
    CHECK(GetLastError() == 0x1357);
}
TEST(paint_gdi_colx_brush_not_null_for_all_valid) {
    REQUIRE(U(GetSysColorBrush));
    for (int i = 0; i <= 30; i++) CHECKF(U(GetSysColorBrush)(i) != NULL, "index %d", i);
    CHECK(U(GetSysColorBrush)(31) == NULL);
    CHECK(U(GetSysColorBrush)(INT_MAX) == NULL);
    CHECK(U(GetSysColorBrush)(INT_MIN) == NULL);
}
TEST(paint_gdi_colx_brush_is_not_deletable_stock_like) {
    REQUIRE(U(GetSysColorBrush));
    HBRUSH b = U(GetSysColorBrush)(COLOR_WINDOW);
    CHECK(b != NULL);
    /* system brushes belong to user32: deleting them must not destroy them */
    DeleteObject(b);
    CHECK(GetObjectType(U(GetSysColorBrush)(COLOR_WINDOW)) == OBJ_BRUSH);
}
TEST(paint_gdi_colx_color_alias_pairs) {
    REQUIRE(U(GetSysColor));
    static const int pairs[][2] = { { COLOR_BACKGROUND, COLOR_DESKTOP }, { COLOR_3DFACE, COLOR_BTNFACE }, { COLOR_3DSHADOW, COLOR_BTNSHADOW },
        { COLOR_3DHIGHLIGHT, COLOR_BTNHIGHLIGHT }, { COLOR_3DHILIGHT, COLOR_BTNHILIGHT }, { COLOR_3DHILIGHT, COLOR_BTNHIGHLIGHT } };
    for (unsigned i = 0; i < sizeof pairs / sizeof pairs[0]; i++)
        CHECKF(U(GetSysColor)(pairs[i][0]) == U(GetSysColor)(pairs[i][1]), "pair %u", i);
}

/* ------------------------------------------------------------------
 * Painting: child / parent behaviour of the update region
 * ------------------------------------------------------------------ */
static HWND px_top(void) { pg_reset(); HWND w = pg_topwin(300, 300, 200, 150); pg_reset(); return w; }
static void px_kill(HWND w) { if (w) U(DestroyWindow)(w); pg_pump(); }
TEST(paint_gdi_pnt_child_invalidation_not_in_parent) {
    REQUIRE(U(InvalidateRect)); REQUIRE(U(GetUpdateRect));
    HWND p = px_top(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    CHECK(c != NULL);
    if (c) {
        pg_flush(p); pg_flush(c);
        RECT in = {5, 5, 25, 25};
        CHECK(U(InvalidateRect)(c, &in, FALSE));
        RECT r; CHECK(U(GetUpdateRect)(c, &r, FALSE));
        CHECK_RECT(r, 5, 5, 25, 25);                          /* child client coordinates */
        CHECK(!U(GetUpdateRect)(p, NULL, FALSE));
        U(ValidateRect)(c, NULL);
        U(DestroyWindow)(c);
    }
    px_kill(p);
}
TEST(paint_gdi_pnt_parent_invalidation_reaches_unclipped_child) {
    REQUIRE(U(InvalidateRect)); REQUIRE(U(GetUpdateRect));
    HWND p = px_top(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    CHECK(c != NULL);
    if (c) {
        pg_flush(p); pg_flush(c);
        CHECK(U(InvalidateRect)(p, NULL, FALSE));
        RECT r; CHECK(U(GetUpdateRect)(p, &r, FALSE));
        CHECK_RECT(r, 0, 0, 200, 150);
        t_note("child has update region after parent invalidation: %d", U(GetUpdateRect)(c, NULL, FALSE));
        U(ValidateRect)(p, NULL); U(ValidateRect)(c, NULL);
        U(DestroyWindow)(c);
    }
    px_kill(p);
}
TEST(paint_gdi_pnt_updatewindow_child_only) {
    REQUIRE(U(UpdateWindow)); REQUIRE(U(InvalidateRect));
    HWND p = px_top(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_recwindow(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    CHECK(c != NULL);
    if (c) {
        pg_flush(p); pg_flush(c);
        pg_reset();
        U(InvalidateRect)(c, NULL, FALSE);
        CHECK(U(UpdateWindow)(c));
        EQ(pg_r.paint, 1);
        CHECK(!U(GetUpdateRect)(c, NULL, FALSE));
        U(DestroyWindow)(c);
    }
    px_kill(p);
}
TEST(paint_gdi_pnt_beginpaint_child_rcpaint_client_coords) {
    REQUIRE(U(BeginPaint)); REQUIRE(U(EndPaint));
    HWND p = px_top(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    CHECK(c != NULL);
    if (c) {
        pg_flush(p); pg_flush(c);
        RECT in = {10, 12, 40, 50};
        U(InvalidateRect)(c, &in, FALSE);
        PAINTSTRUCT ps; memset(&ps, 0, sizeof ps);
        HDC dc = U(BeginPaint)(c, &ps);
        CHECK(dc != NULL);
        CHECK_RECT(ps.rcPaint, 10, 12, 40, 50);
        CHECK(!ps.fErase);
        CHECK(!U(GetUpdateRect)(c, NULL, FALSE));
        U(EndPaint)(c, &ps);
        U(DestroyWindow)(c);
    }
    px_kill(p);
}
TEST(paint_gdi_pnt_beginpaint_erase_flag_set_by_invalidate) {
    REQUIRE(U(BeginPaint)); REQUIRE(U(EndPaint));
    HWND w = px_top(); CHECK(w != NULL); if (!w) return;
    pg_r.do_paint = 2;
    pg_r.erase_ret = 0;
    U(InvalidateRect)(w, NULL, TRUE);
    PAINTSTRUCT ps; memset(&ps, 0, sizeof ps);
    HDC dc = U(BeginPaint)(w, &ps);
    CHECK(dc != NULL);
    CHECK(ps.fErase == TRUE);        /* class window proc did not handle the erase */
    EQ(pg_r.erase, 1);
    U(EndPaint)(w, &ps);
    pg_r.do_paint = 1;
    px_kill(w);
}
TEST(paint_gdi_pnt_endpaint_returns_true) {
    REQUIRE(U(BeginPaint)); REQUIRE(U(EndPaint));
    HWND w = px_top(); CHECK(w != NULL); if (!w) return;
    PAINTSTRUCT ps; memset(&ps, 0, sizeof ps);
    HDC dc = U(BeginPaint)(w, &ps);
    CHECK(dc != NULL);
    CHECK(U(EndPaint)(w, &ps) == TRUE);
    px_kill(w);
}
TEST(paint_gdi_pnt_getdc_window_and_client_differ_on_frame) {
    REQUIRE(U(GetDC)); REQUIRE(U(GetWindowDC));
    HWND w = pg_recwindow(WS_OVERLAPPEDWINDOW, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 240, 200, NULL);
    CHECK(w != NULL); if (!w) return;
    U(ShowWindow)(w, SW_SHOWNA); pg_flush(w);
    RECT cr, wr; U(GetClientRect)(w, &cr); U(GetWindowRect)(w, &wr);
    HDC c = U(GetDC)(w), f = U(GetWindowDC)(w);
    CHECK(c && f && c != f);
    /* the origin of the window DC is the window's top-left; the client DC is moved by the frame */
    POINT oc, of; GetDCOrgEx(c, &oc); GetDCOrgEx(f, &of);
    CHECK(oc.x > of.x && oc.y > of.y);
    U(ReleaseDC)(w, c); U(ReleaseDC)(w, f);
    U(DestroyWindow)(w); pg_pump();
}
TEST(paint_gdi_pnt_validate_rgn_null_and_empty) {
    REQUIRE(U(InvalidateRgn)); REQUIRE(U(ValidateRgn)); REQUIRE(U(GetUpdateRect));
    HWND w = px_top(); CHECK(w != NULL); if (!w) return;
    CHECK(U(InvalidateRgn)(w, NULL, FALSE));
    HRGN empty = CreateRectRgn(0, 0, 0, 0);
    CHECK(U(ValidateRgn)(w, empty));
    CHECK(U(GetUpdateRect)(w, NULL, FALSE));      /* validating an empty region changes nothing */
    CHECK(U(ValidateRgn)(w, NULL));
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));
    DeleteObject(empty);
    px_kill(w);
}
TEST(paint_gdi_pnt_invalidate_union_of_many) {
    REQUIRE(U(InvalidateRect)); REQUIRE(U(GetUpdateRgn));
    HWND w = px_top(); CHECK(w != NULL); if (!w) return;
    for (int i = 0; i < 10; i++) {
        RECT r = { i * 20, i * 14, i * 20 + 10, i * 14 + 8 };
        CHECK(U(InvalidateRect)(w, &r, FALSE));
    }
    HRGN rg = CreateRectRgn(0, 0, 0, 0);
    EQ(U(GetUpdateRgn)(w, rg, FALSE), COMPLEXREGION);
    for (int i = 0; i < 10; i++) {
        CHECKF(PtInRegion(rg, i * 20 + 5, i * 14 + 4), "rect %d", i);
        CHECKF(!PtInRegion(rg, i * 20 + 15, i * 14 + 4), "gap %d", i);
    }
    DeleteObject(rg);
    U(ValidateRect)(w, NULL);
    px_kill(w);
}
TEST(paint_gdi_pnt_getupdatergn_region_is_overwritten) {
    REQUIRE(U(GetUpdateRgn)); REQUIRE(U(InvalidateRect));
    HWND w = px_top(); CHECK(w != NULL); if (!w) return;
    HRGN rg = CreateRectRgn(500, 500, 900, 900);
    EQ(U(GetUpdateRgn)(w, rg, FALSE), NULLREGION);
    RECT b; EQ(GetRgnBox(rg, &b), NULLREGION);
    RECT in = {1, 2, 30, 40};
    U(InvalidateRect)(w, &in, FALSE);
    EQ(U(GetUpdateRgn)(w, rg, FALSE), SIMPLEREGION);
    GetRgnBox(rg, &b);
    CHECK_RECT(b, 1, 2, 30, 40);
    U(ValidateRect)(w, NULL);
    DeleteObject(rg);
    px_kill(w);
}
TEST(paint_gdi_pnt_redraw_no_flags_is_noop) {
    REQUIRE(U(RedrawWindow)); REQUIRE(U(GetUpdateRect));
    HWND w = px_top(); CHECK(w != NULL); if (!w) return;
    BOOL r = U(RedrawWindow)(w, NULL, NULL, 0);
    t_note("RedrawWindow(flags 0) -> %d", r);
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));
    px_kill(w);
}
TEST(paint_gdi_pnt_redraw_validate_partial_region) {
    REQUIRE(U(RedrawWindow)); REQUIRE(U(GetUpdateRect));
    HWND w = px_top(); CHECK(w != NULL); if (!w) return;
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE));
    HRGN rg = CreateRectRgn(0, 0, 200, 75);
    CHECK(U(RedrawWindow)(w, NULL, rg, RDW_VALIDATE));
    RECT r; CHECK(U(GetUpdateRect)(w, &r, FALSE));
    CHECK_RECT(r, 0, 75, 200, 150);
    DeleteObject(rg);
    U(ValidateRect)(w, NULL);
    px_kill(w);
}
TEST(paint_gdi_pnt_scrolldc_returns_region_rect_consistent) {
    REQUIRE(U(ScrollDC));
    HDC s = GetDC(NULL);
    HDC m = CreateCompatibleDC(s);
    HBITMAP bm = CreateCompatibleBitmap(s, 40, 40);
    ReleaseDC(NULL, s);
    HGDIOBJ old = SelectObject(m, bm);
    PatBlt(m, 0, 0, 40, 40, WHITENESS);
    RECT sc = {0, 0, 40, 40}, up;
    HRGN rg = CreateRectRgn(0, 0, 0, 0);
    CHECK(U(ScrollDC)(m, 7, -3, &sc, &sc, rg, &up));
    RECT b; GetRgnBox(rg, &b);
    CHECK(r_eq(&b, &up));
    CHECK_RECT(up, 0, 0, 40, 40);
    SelectObject(m, old); DeleteDC(m); DeleteObject(bm); DeleteObject(rg);
}
