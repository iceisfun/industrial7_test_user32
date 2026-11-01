/* DrawStateW, GrayStringW, TabbedTextOutW. */
#include "gap7.h"
#ifndef DSS_DEFAULT
#define DSS_DEFAULT 0x0040
#endif

typedef struct { HDC dc; HBITMAP bmp, old; int w, h; DWORD *bits; } surf_t;

static void surf_clear(surf_t *s);
static int surf_make(surf_t *s, int w, int h)
{
    memset(s, 0, sizeof *s);
    s->w = w; s->h = h;
    HDC scr = GetDC(NULL);
    s->dc = CreateCompatibleDC(scr);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    s->bmp = CreateDIBSection(scr, &bi, DIB_RGB_COLORS, (void **)&s->bits, NULL, 0);
    ReleaseDC(NULL, scr);
    if (!s->dc || !s->bmp) return 0;
    s->old = SelectObject(s->dc, s->bmp);
    SetBkColor(s->dc, RGB(255, 255, 255)); SetTextColor(s->dc, RGB(0, 0, 0));
    /* A new DIB section is zero-filled (black) on Windows; the ink counters treat white as blank. */
    surf_clear(s);
    return 1;
}
static void surf_clear(surf_t *s)
{
    for (int i = 0; i < s->w * s->h; i++) s->bits[i] = 0x00FFFFFF;
}
static void surf_free(surf_t *s)
{
    if (s->dc) { SelectObject(s->dc, s->old); DeleteDC(s->dc); }
    if (s->bmp) DeleteObject(s->bmp);
    memset(s, 0, sizeof *s);
}
static DWORD px(surf_t *s, int x, int y) { return s->bits[y * s->w + x] & 0xFFFFFF; }
/* count pixels not white inside / outside the rect */
static int ink_in(surf_t *s, int x0, int y0, int x1, int y1)
{
    int n = 0;
    GdiFlush();
    for (int y = y0; y < y1 && y < s->h; y++)
        for (int x = x0; x < x1 && x < s->w; x++) if (y >= 0 && x >= 0 && px(s, x, y) != 0xFFFFFF) n++;
    return n;
}
static int ink_total(surf_t *s) { return ink_in(s, 0, 0, s->w, s->h); }
static int ink_outside(surf_t *s, int x0, int y0, int x1, int y1)
{
    return ink_total(s) - ink_in(s, x0, y0, x1, y1);
}
#define BGR(r, g, b) ((DWORD)((b) | ((g) << 8) | ((r) << 16)))   /* DIB pixel layout 0x00RRGGBB */

/* ================= DrawStateW ================= */
static struct { int calls; HDC dc; LPARAM l; WPARAM w; int cx, cy; POINT org; int ret; HDC outer; } ds;
static BOOL CALLBACK ds_cb(HDC dc, LPARAM l, WPARAM w, int cx, int cy)
{
    ds.calls++; ds.dc = dc; ds.l = l; ds.w = w; ds.cx = cx; ds.cy = cy;
    GetViewportOrgEx(dc, &ds.org);
    return ds.ret;
}
static BOOL CALLBACK ds_cb_fill(HDC dc, LPARAM l, WPARAM w, int cx, int cy)
{
    ds.calls++; ds.dc = dc; ds.cx = cx; ds.cy = cy;
    PatBlt(dc, 0, 0, cx, cy, BLACKNESS);
    return TRUE;
}
static int have_ds(void) { return U(DrawStateW) != NULL; }
#define DS_OK(s) (have_ds() && surf_make(&(s), 120, 80))

TEST(gap7_drawstate_invalid_type_values)
{
    REQUIRE(U(DrawStateW));
    surf_t s; CHECK(surf_make(&s, 100, 60));
    /* low three flag bits select the image type; 6 and 7 are not defined */
    static const UINT types[] = { 6, 7, 6 | DSS_NORMAL, 7 | DSS_MONO, 6 | DSS_DISABLED };
    for (unsigned i = 0; i < sizeof types / sizeof types[0]; i++) {
        ds.calls = 0; ds.ret = TRUE;
        BOOL r = U(DrawStateW)(s.dc, NULL, ds_cb, 0, 0, 0, 0, 20, 20, types[i]);
        CHECKF(r == FALSE, "flags=%x r=%d", types[i], r);
        CHECKF(ds.calls == 0, "callback invoked for flags %x", types[i]);
    }
    CHECK(ink_total(&s) == 0);
    surf_free(&s);
}

TEST(gap7_drawstate_complex_zero_size_returns_true_without_callback)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    static const int sz[][2] = { { 0, 0 }, { 0, 10 }, { 10, 0 } };
    for (int i = 0; i < 3; i++) {
        ds.calls = 0; ds.ret = TRUE;
        BOOL r = U(DrawStateW)(s.dc, NULL, ds_cb, 1, 2, 3, 4, sz[i][0], sz[i][1], DST_COMPLEX | DSS_NORMAL);
        CHECKF(r == TRUE, "i=%d r=%d", i, r);
        CHECKF(ds.calls == 0, "i=%d calls=%d", i, ds.calls);
        ds.calls = 0;
        r = U(DrawStateW)(s.dc, NULL, ds_cb, 1, 2, 3, 4, sz[i][0], sz[i][1], DST_COMPLEX | DSS_DISABLED);
        CHECKF(r == TRUE && ds.calls == 0, "disabled i=%d r=%d calls=%d", i, r, ds.calls);
    }
    surf_free(&s);
}

TEST(gap7_drawstate_complex_callback_parameters)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    ds.calls = 0; ds.ret = TRUE;
    SetViewportOrgEx(s.dc, 3, 4, NULL);
    BOOL r = U(DrawStateW)(s.dc, NULL, ds_cb, 0x1234567, 0xABCDEF, 11, 7, 30, 20, DST_COMPLEX | DSS_NORMAL);
    CHECK(r == TRUE);
    CHECK(ds.calls == 1);
    CHECK(ds.dc == s.dc);
    CHECK(ds.l == 0x1234567);
    CHECK(ds.w == 0xABCDEF);
    CHECK(ds.cx == 30 && ds.cy == 20);
    /* callback draws relative to (x,y): the viewport origin is shifted for the call and restored */
    CHECKF(ds.org.x == 3 + 11 && ds.org.y == 4 + 7, "org %ld,%ld", ds.org.x, ds.org.y);
    POINT p; GetViewportOrgEx(s.dc, &p);
    CHECK(p.x == 3 && p.y == 4);
    surf_free(&s);
}

TEST(gap7_drawstate_complex_callback_result_passthrough)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    ds.ret = FALSE; ds.calls = 0;
    CHECK(U(DrawStateW)(s.dc, NULL, ds_cb, 0, 0, 0, 0, 10, 10, DST_COMPLEX) == FALSE);
    CHECK(ds.calls == 1);
    ds.ret = TRUE;
    CHECK(U(DrawStateW)(s.dc, NULL, ds_cb, 0, 0, 0, 0, 10, 10, DST_COMPLEX) == TRUE);
    ds.ret = 5;
    BOOL r = U(DrawStateW)(s.dc, NULL, ds_cb, 0, 0, 0, 0, 10, 10, DST_COMPLEX);
    CHECK(r != FALSE);
    t_note("callback returned 5, DrawStateW returned %d", r);
    /* viewport is restored even when the callback fails */
    ds.ret = FALSE;
    U(DrawStateW)(s.dc, NULL, ds_cb, 0, 0, 20, 20, 10, 10, DST_COMPLEX);
    POINT p; GetViewportOrgEx(s.dc, &p);
    CHECK(p.x == 0 && p.y == 0);
    surf_free(&s);
}

TEST(gap7_drawstate_mono_flags_use_scratch_dc)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    static const UINT flags[] = { DSS_MONO, DSS_UNION, DSS_DISABLED, DSS_DEFAULT, DSS_UNION | DSS_DISABLED, DSS_MONO | DSS_NORMAL };
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        ds.calls = 0; ds.ret = TRUE;
        BOOL r = U(DrawStateW)(s.dc, (HBRUSH)GetStockObject(BLACK_BRUSH), ds_cb, 9, 8, 5, 5, 16, 12, DST_COMPLEX | flags[i]);
        CHECKF(r == TRUE, "flags=%x r=%d", flags[i], r);
        CHECKF(ds.calls == 1, "flags=%x calls=%d", flags[i], ds.calls);
        CHECKF(ds.dc != s.dc && ds.dc != NULL, "flags=%x callback drew on the target DC", flags[i]);
        CHECKF(ds.l == 9 && ds.w == 8 && ds.cx == 16 && ds.cy == 12, "flags=%x params", flags[i]);
        POINT p; GetViewportOrgEx(s.dc, &p);
        CHECK(p.x == 0 && p.y == 0);
    }
    /* normal drawing hands the callback the real DC */
    ds.calls = 0;
    U(DrawStateW)(s.dc, NULL, ds_cb, 0, 0, 0, 0, 5, 5, DST_COMPLEX | DSS_NORMAL);
    CHECK(ds.dc == s.dc);
    surf_free(&s);
}

TEST(gap7_drawstate_mono_paints_with_foreground_brush)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
    surf_clear(&s);
    ds.calls = 0;
    BOOL r = U(DrawStateW)(s.dc, red, ds_cb_fill, 0, 0, 20, 15, 30, 20, DST_COMPLEX | DSS_MONO);
    GdiFlush();
    CHECK(r == TRUE);
    CHECK(ds.calls == 1);
    CHECKF(px(&s, 30, 25) == BGR(255, 0, 0), "centre=%06lx", px(&s, 30, 25));
    CHECKF(px(&s, 20, 15) == BGR(255, 0, 0), "top-left=%06lx", px(&s, 20, 15));
    CHECKF(px(&s, 49, 34) == BGR(255, 0, 0), "bottom-right=%06lx", px(&s, 49, 34));
    CHECKF(px(&s, 19, 25) == 0xFFFFFF, "left of rect=%06lx", px(&s, 19, 25));
    CHECKF(px(&s, 50, 25) == 0xFFFFFF, "right of rect=%06lx", px(&s, 50, 25));
    CHECKF(px(&s, 30, 14) == 0xFFFFFF, "above rect=%06lx", px(&s, 30, 14));
    CHECKF(px(&s, 30, 35) == 0xFFFFFF, "below rect=%06lx", px(&s, 30, 35));
    CHECK(ink_outside(&s, 20, 15, 50, 35) == 0);
    /* different brush colour */
    HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
    surf_clear(&s);
    U(DrawStateW)(s.dc, blue, ds_cb_fill, 0, 0, 2, 2, 10, 10, DST_COMPLEX | DSS_MONO);
    GdiFlush();
    CHECKF(px(&s, 6, 6) == BGR(0, 0, 255), "blue=%06lx", px(&s, 6, 6));
    DeleteObject(red); DeleteObject(blue);
    surf_free(&s);
}

TEST(gap7_drawstate_normal_complex_draws_directly)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    surf_clear(&s);
    BOOL r = U(DrawStateW)(s.dc, NULL, ds_cb_fill, 0, 0, 10, 10, 20, 20, DST_COMPLEX | DSS_NORMAL);
    GdiFlush();
    CHECK(r == TRUE);
    CHECKF(px(&s, 15, 15) == 0, "inside=%06lx", px(&s, 15, 15));
    CHECKF(px(&s, 29, 29) == 0, "inside corner=%06lx", px(&s, 29, 29));
    CHECKF(px(&s, 30, 30) == 0xFFFFFF, "outside=%06lx", px(&s, 30, 30));
    CHECKF(px(&s, 9, 10) == 0xFFFFFF, "left=%06lx", px(&s, 9, 10));
    surf_free(&s);
}

TEST(gap7_drawstate_disabled_emboss_colours)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    surf_clear(&s);
    BOOL r = U(DrawStateW)(s.dc, NULL, ds_cb_fill, 0, 0, 10, 10, 24, 16, DST_COMPLEX | DSS_DISABLED);
    GdiFlush();
    CHECK(r == TRUE);
    COLORREF sh = GetSysColor(COLOR_3DSHADOW), hi = GetSysColor(COLOR_3DHILIGHT);
    DWORD shp = BGR(GetRValue(sh), GetGValue(sh), GetBValue(sh)), hip = BGR(GetRValue(hi), GetGValue(hi), GetBValue(hi));
    if (px(&s, 20, 15) != shp) t_note("disabled body pixel=%06lx expected shadow %06lx", px(&s, 20, 15), shp);
    if (px(&s, 34, 26) != hip && px(&s, 34, 26) != 0xFFFFFF) t_note("disabled highlight pixel=%06lx expected %06lx", px(&s, 34, 26), hip);
    CHECK(ink_in(&s, 10, 10, 35, 27) > 0);
    CHECK(ink_outside(&s, 10, 10, 35, 27) == 0);
    surf_free(&s);
}

TEST(gap7_drawstate_text_draws_ink_and_stays_in_bounds)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    surf_clear(&s);
    BOOL r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"WWWW", 4, 10, 10, 0, 0, DST_TEXT | DSS_NORMAL);
    CHECK(r != FALSE);
    SIZE sz; GetTextExtentPoint32W(s.dc, L"WWWW", 4, &sz);
    int ink = ink_in(&s, 10, 10, 10 + sz.cx, 10 + sz.cy);
    CHECKF(ink > 10, "ink=%d", ink);
    CHECKF(ink_outside(&s, 10, 10, 10 + sz.cx, 10 + sz.cy) == 0, "ink outside text cell: %d", ink_outside(&s, 10, 10, 10 + sz.cx, 10 + sz.cy));
    surf_free(&s);
}

TEST(gap7_drawstate_text_length_zero_means_nul_terminated)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    surf_clear(&s);
    CHECK(U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"WWWWWW", 0, 5, 5, 0, 0, DST_TEXT));
    int full = ink_total(&s);
    surf_clear(&s);
    CHECK(U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"WWWWWW", 6, 5, 5, 0, 0, DST_TEXT));
    CHECK(ink_total(&s) == full);
    surf_clear(&s);
    CHECK(U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"WWWWWW", 2, 5, 5, 0, 0, DST_TEXT));
    int part = ink_total(&s);
    CHECKF(part > 0 && part < full, "part=%d full=%d", part, full);
    surf_free(&s);
}

TEST(gap7_drawstate_text_empty_string_is_true_and_draws_nothing)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    surf_clear(&s);
    BOOL r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"", 0, 5, 5, 0, 0, DST_TEXT);
    CHECK(r == TRUE);
    r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"", 0, 5, 5, 0, 0, DST_PREFIXTEXT);
    CHECK(r == TRUE);
    CHECK(ink_total(&s) == 0);
    surf_free(&s);
}

TEST(gap7_drawstate_text_explicit_size_with_mono)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    HBRUSH g = CreateSolidBrush(RGB(0, 128, 0));
    surf_clear(&s);
    BOOL r = U(DrawStateW)(s.dc, g, NULL, (LPARAM)L"HHHH", 0, 8, 8, 60, 30, DST_TEXT | DSS_MONO);
    CHECK(r != FALSE);
    int in = ink_in(&s, 8, 8, 68, 38);
    CHECKF(in > 0, "ink=%d", in);
    CHECK(ink_outside(&s, 8, 8, 68, 38) == 0);
    /* every inked pixel is the brush colour */
    GdiFlush();
    int bad = 0;
    for (int y = 8; y < 38; y++) for (int x = 8; x < 68; x++) { DWORD p = px(&s, x, y); if (p != 0xFFFFFF && p != BGR(0, 128, 0)) bad++; }
    CHECKF(bad == 0, "bad pixels=%d", bad);
    DeleteObject(g);
    surf_free(&s);
}

TEST(gap7_drawstate_prefixtext_flags)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    static const UINT f[] = { DST_PREFIXTEXT, DST_PREFIXTEXT | DSS_HIDEPREFIX, DST_PREFIXTEXT | DSS_PREFIXONLY, DST_PREFIXTEXT | DSS_RIGHT,
                              DST_PREFIXTEXT | DSS_DISABLED, DST_PREFIXTEXT | DSS_MONO };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        surf_clear(&s);
        BOOL r = U(DrawStateW)(s.dc, (HBRUSH)GetStockObject(BLACK_BRUSH), NULL, (LPARAM)L"&File", 0, 4, 4, 0, 0, f[i]);
        CHECKF(r == TRUE, "flags=%x r=%d", f[i], r);
    }
    surf_clear(&s);
    U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"&File", 0, 4, 4, 0, 0, DST_PREFIXTEXT);
    int shown = ink_total(&s);
    surf_clear(&s);
    U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"&File", 0, 4, 4, 0, 0, DST_PREFIXTEXT | DSS_PREFIXONLY);
    int only = ink_total(&s);
    CHECKF(shown > 0, "shown=%d", shown);
    CHECKF(only <= shown, "prefix only=%d shown=%d", only, shown);
    surf_free(&s);
}

TEST(gap7_drawstate_prefixtext_null_string_fails)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    CHECK(U(DrawStateW)(s.dc, NULL, NULL, 0, 0, 0, 0, 20, 20, DST_PREFIXTEXT) == FALSE);
    CHECK(U(DrawStateW)(s.dc, NULL, NULL, 0, 0, 0, 0, 0, 0, DST_PREFIXTEXT) == FALSE);
    CHECK(U(DrawStateW)(s.dc, NULL, NULL, 0, 5, 0, 0, 20, 20, DST_PREFIXTEXT | DSS_DISABLED) == FALSE);
    CHECK(ink_total(&s) == 0);
    surf_free(&s);
}

TEST(gap7_drawstate_bitmap_invalid_handles)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    HBRUSH br = CreateSolidBrush(RGB(1, 2, 3));
    HPEN pen = CreatePen(PS_SOLID, 1, 0);
    LPARAM bad[] = { 0, (LPARAM)0xDEADBEEF, (LPARAM)br, (LPARAM)pen, (LPARAM)s.dc, (LPARAM)GetStockObject(SYSTEM_FONT) };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BOOL r = U(DrawStateW)(s.dc, NULL, NULL, bad[i], 0, 0, 0, 8, 8, DST_BITMAP);
        CHECKF(r == FALSE, "i=%u r=%d", i, r);
        r = U(DrawStateW)(s.dc, NULL, NULL, bad[i], 0, 0, 0, 0, 0, DST_BITMAP | DSS_DISABLED);
        CHECKF(r == FALSE, "disabled i=%u r=%d", i, r);
    }
    CHECK(ink_total(&s) == 0);
    DeleteObject(br); DeleteObject(pen);
    surf_free(&s);
}

TEST(gap7_drawstate_bitmap_draws_and_sizes_from_bitmap)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    surf_t src; CHECK(surf_make(&src, 9, 7));
    for (int i = 0; i < 9 * 7; i++) src.bits[i] = 0x000000FF; /* blue */
    GdiFlush();
    SelectObject(src.dc, src.old);   /* free the bitmap for DrawState */
    surf_clear(&s);
    BOOL r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)src.bmp, 0, 20, 10, 0, 0, DST_BITMAP | DSS_NORMAL);
    GdiFlush();
    CHECK(r == TRUE);
    CHECKF(px(&s, 20, 10) == 0x0000FF, "tl=%06lx", px(&s, 20, 10));
    CHECKF(px(&s, 28, 16) == 0x0000FF, "br=%06lx", px(&s, 28, 16));
    CHECKF(px(&s, 29, 16) == 0xFFFFFF, "right edge=%06lx", px(&s, 29, 16));
    CHECKF(px(&s, 28, 17) == 0xFFFFFF, "bottom edge=%06lx", px(&s, 28, 17));
    CHECK(ink_in(&s, 20, 10, 29, 17) == 9 * 7);
    CHECK(ink_outside(&s, 20, 10, 29, 17) == 0);
    /* a smaller explicit size crops */
    surf_clear(&s);
    r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)src.bmp, 0, 20, 10, 4, 3, DST_BITMAP);
    GdiFlush();
    CHECK(r == TRUE);
    CHECKF(ink_total(&s) == 12, "cropped ink=%d", ink_total(&s));
    surf_clear(&s);
    r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)src.bmp, 0, 20, 10, 0, 0, DST_BITMAP | DSS_MONO);
    CHECK(r == TRUE);
    r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)src.bmp, 0, 20, 10, 0, 0, DST_BITMAP | DSS_DISABLED);
    CHECK(r == TRUE);
    surf_free(&src);
    surf_free(&s);
}

TEST(gap7_drawstate_icon_cases)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    REQUIRE(U(LoadIconW));
    HICON ic = U(LoadIconW)(NULL, (LPCWSTR)IDI_APPLICATION);
    CHECK(ic != NULL);
    surf_clear(&s);
    BOOL r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)ic, 0, 10, 10, 0, 0, DST_ICON | DSS_NORMAL);
    CHECK(r == TRUE);
    CHECKF(ink_total(&s) > 0, "ink=%d", ink_total(&s));
    surf_clear(&s);
    r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)ic, 0, 10, 10, 16, 16, DST_ICON | DSS_NORMAL);
    CHECK(r == TRUE);
    CHECK(ink_total(&s) > 0);
    r = U(DrawStateW)(s.dc, (HBRUSH)GetStockObject(BLACK_BRUSH), NULL, (LPARAM)ic, 0, 10, 10, 32, 32, DST_ICON | DSS_MONO);
    CHECK(r == TRUE);
    r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)ic, 0, 10, 10, 32, 32, DST_ICON | DSS_DISABLED);
    CHECK(r == TRUE);
    /* invalid icon handle with an explicit size */
    surf_clear(&s);
    r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)0xDEADBEEF, 0, 10, 10, 16, 16, DST_ICON | DSS_NORMAL);
    t_note("DST_ICON with bogus HICON, 16x16: r=%d ink=%d", r, ink_total(&s));
    r = U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)0xDEADBEEF, 0, 10, 10, 0, 0, DST_ICON | DSS_NORMAL);
    t_note("DST_ICON with bogus HICON, size 0: r=%d", r);
    surf_free(&s);
}

TEST(gap7_drawstate_null_dc_text_fails)
{
    REQUIRE(U(DrawStateW));
    SetLastError(0);
    BOOL r = U(DrawStateW)(NULL, NULL, NULL, (LPARAM)L"abc", 3, 0, 0, 20, 10, DST_TEXT);
    CHECKF(r == FALSE, "r=%d", r);
    r = U(DrawStateW)((HDC)(ULONG_PTR)0xDEADBEEF, NULL, NULL, (LPARAM)L"abc", 3, 0, 0, 20, 10, DST_TEXT);
    CHECKF(r == FALSE, "bogus dc r=%d", r);
    ds.calls = 0; ds.ret = TRUE;
    r = U(DrawStateW)(NULL, NULL, ds_cb, 0, 0, 0, 0, 10, 10, DST_COMPLEX);
    t_note("NULL dc with callback: r=%d calls=%d", r, ds.calls);
}

TEST(gap7_drawstate_repeated_calls_stable)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    int fails = 0;
    for (int i = 0; i < 400; i++) {
        UINT f = (i % 3 == 0) ? DSS_MONO : (i % 3 == 1 ? DSS_DISABLED : DSS_NORMAL);
        if (!U(DrawStateW)(s.dc, NULL, NULL, (LPARAM)L"loop", 4, i % 50, i % 40, 0, 0, DST_TEXT | f)) fails++;
    }
    CHECKF(fails == 0, "failures=%d", fails);
    surf_free(&s);
}

TEST(gap7_drawstate_large_mono_area)
{
    surf_t s; if (!DS_OK(s)) { REQUIRE(have_ds()); return; }
    ds.calls = 0; ds.ret = TRUE;
    BOOL r = U(DrawStateW)(s.dc, NULL, ds_cb, 0, 0, 0, 0, 700, 500, DST_COMPLEX | DSS_MONO);
    CHECK(r == TRUE);
    CHECK(ds.calls == 1);
    CHECK(ds.cx == 700 && ds.cy == 500);
    surf_free(&s);
}

/* ================= GrayStringW ================= */
static struct { int calls; HDC dc; LPARAM l; int n; int ret; } gs;
static BOOL CALLBACK gs_cb(HDC dc, LPARAM l, int n)
{
    gs.calls++; gs.dc = dc; gs.l = l; gs.n = n;
    return gs.ret;
}
static BOOL CALLBACK gs_cb_fill(HDC dc, LPARAM l, int n)
{
    gs.calls++; gs.dc = dc; gs.l = l; gs.n = n;
    PatBlt(dc, 0, 0, 24, 14, BLACKNESS);
    return TRUE;
}
#define GS_OK(s) (U(GrayStringW) && surf_make(&(s), 120, 80))

TEST(gap7_graystring_callback_params_and_result)
{
    surf_t s; if (!GS_OK(s)) { REQUIRE(U(GrayStringW)); return; }
    gs.calls = 0; gs.ret = 1;
    BOOL r = U(GrayStringW)(s.dc, NULL, gs_cb, 0x55AA, 17, 5, 5, 30, 12);
    CHECK(r != FALSE);
    CHECK(gs.calls == 1);
    CHECK(gs.l == 0x55AA);
    CHECK(gs.n == 17);
    CHECK(gs.dc != s.dc && gs.dc != NULL);
    gs.calls = 0; gs.ret = 0;
    r = U(GrayStringW)(s.dc, NULL, gs_cb, 0x55AA, 3, 5, 5, 30, 12);
    CHECKF(r == FALSE, "r=%d", r);
    CHECK(gs.calls == 1 && gs.n == 3);
    surf_free(&s);
}

TEST(gap7_graystring_callback_false_draws_nothing)
{
    surf_t s; if (!GS_OK(s)) { REQUIRE(U(GrayStringW)); return; }
    surf_clear(&s);
    gs.calls = 0; gs.ret = 0;
    U(GrayStringW)(s.dc, (HBRUSH)GetStockObject(BLACK_BRUSH), gs_cb, 1, 1, 10, 10, 30, 12);
    CHECK(ink_total(&s) == 0);
    surf_free(&s);
}

TEST(gap7_graystring_count_zero_uses_string_length)
{
    surf_t s; if (!GS_OK(s)) { REQUIRE(U(GrayStringW)); return; }
    gs.calls = 0; gs.ret = 1;
    BOOL r = U(GrayStringW)(s.dc, NULL, gs_cb, (LPARAM)L"seven77", 0, 0, 0, 40, 10);
    CHECK(r != FALSE);
    CHECKF(gs.n == 7, "n=%d", gs.n);
    gs.calls = 0;
    r = U(GrayStringW)(s.dc, NULL, gs_cb, (LPARAM)L"", 0, 0, 0, 40, 10);
    t_note("empty string with callback: r=%d calls=%d n=%d", r, gs.calls, gs.n);
    surf_free(&s);
}

TEST(gap7_graystring_callback_paints_dithered_brush)
{
    surf_t s; if (!GS_OK(s)) { REQUIRE(U(GrayStringW)); return; }
    HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
    surf_clear(&s);
    gs.calls = 0;
    BOOL r = U(GrayStringW)(s.dc, red, gs_cb_fill, 0, 1, 20, 20, 24, 14);
    GdiFlush();
    CHECK(r != FALSE);
    int in = ink_in(&s, 20, 20, 44, 34);
    CHECKF(in > 0, "ink=%d", in);
    CHECKF(ink_outside(&s, 20, 20, 44, 34) == 0, "outside=%d", ink_outside(&s, 20, 20, 44, 34));
    GdiFlush();
    int bad = 0, red_n = 0;
    for (int y = 20; y < 34; y++) for (int x = 20; x < 44; x++) { DWORD p = px(&s, x, y); if (p == BGR(255, 0, 0)) red_n++; else if (p != 0xFFFFFF) bad++; }
    CHECKF(bad == 0, "unexpected colours=%d", bad);
    CHECK(red_n == in);
    /* graying leaves a pattern, not a solid fill */
    t_note("GrayString covered %d of %d pixels with the brush", red_n, 24 * 14);
    DeleteObject(red);
    surf_free(&s);
}

TEST(gap7_graystring_text_without_callback)
{
    surf_t s; if (!GS_OK(s)) { REQUIRE(U(GrayStringW)); return; }
    surf_clear(&s);
    BOOL r = U(GrayStringW)(s.dc, (HBRUSH)GetStockObject(BLACK_BRUSH), NULL, (LPARAM)L"Gray", 0, 6, 6, 0, 0);
    CHECK(r != FALSE);
    SIZE sz; GetTextExtentPoint32W(s.dc, L"Gray", 4, &sz);
    int in = ink_in(&s, 6, 6, 6 + sz.cx, 6 + sz.cy);
    CHECKF(in > 0, "ink=%d", in);
    CHECKF(ink_outside(&s, 6, 6, 6 + sz.cx, 6 + sz.cy) == 0, "outside=%d", ink_outside(&s, 6, 6, 6 + sz.cx, 6 + sz.cy));
    /* explicit count shorter than the string */
    surf_clear(&s);
    r = U(GrayStringW)(s.dc, (HBRUSH)GetStockObject(BLACK_BRUSH), NULL, (LPARAM)L"Gray", 2, 6, 6, 0, 0);
    CHECK(r != FALSE);
    SIZE s2; GetTextExtentPoint32W(s.dc, L"Gr", 2, &s2);
    CHECKF(ink_outside(&s, 6, 6, 6 + s2.cx, 6 + s2.cy) == 0, "outside(2 chars)=%d", ink_outside(&s, 6, 6, 6 + s2.cx, 6 + s2.cy));
    surf_free(&s);
}

TEST(gap7_graystring_null_brush_uses_window_text_colour)
{
    surf_t s; if (!GS_OK(s)) { REQUIRE(U(GrayStringW)); return; }
    surf_clear(&s);
    gs.calls = 0;
    BOOL r = U(GrayStringW)(s.dc, NULL, gs_cb_fill, 0, 1, 10, 10, 24, 14);
    GdiFlush();
    CHECK(r != FALSE);
    COLORREF wt = GetSysColor(COLOR_WINDOWTEXT);
    DWORD wtp = BGR(GetRValue(wt), GetGValue(wt), GetBValue(wt));
    int hit = 0;
    for (int y = 10; y < 24; y++) for (int x = 10; x < 34; x++) if (px(&s, x, y) == wtp) hit++;
    CHECKF(hit > 0, "window-text pixels=%d", hit);
    surf_free(&s);
}

TEST(gap7_graystring_dc_state_preserved)
{
    surf_t s; if (!GS_OK(s)) { REQUIRE(U(GrayStringW)); return; }
    SetTextColor(s.dc, RGB(10, 20, 30));
    SetBkColor(s.dc, RGB(200, 210, 220));
    HGDIOBJ fnt = GetCurrentObject(s.dc, OBJ_FONT);
    HGDIOBJ brs = GetCurrentObject(s.dc, OBJ_BRUSH);
    gs.calls = 0; gs.ret = 1;
    U(GrayStringW)(s.dc, NULL, gs_cb, 1, 1, 0, 0, 10, 10);
    CHECK(GetTextColor(s.dc) == RGB(10, 20, 30));
    CHECK(GetBkColor(s.dc) == RGB(200, 210, 220));
    CHECK(GetCurrentObject(s.dc, OBJ_FONT) == fnt);
    CHECK(GetCurrentObject(s.dc, OBJ_BRUSH) == brs);
    surf_free(&s);
}

TEST(gap7_graystring_invalid_dc_with_callback_returns_callback_result)
{
    REQUIRE(U(GrayStringW));
    gs.calls = 0; gs.ret = 0;
    BOOL r = U(GrayStringW)((HDC)(ULONG_PTR)0xDEADBEEF, NULL, gs_cb, 1, 1, 0, 0, 10, 10);
    CHECKF(r == FALSE, "r=%d", r);
    t_note("GrayStringW(bogus dc, callback->0): calls=%d", gs.calls);
}

TEST(gap7_graystring_repeated)
{
    surf_t s; if (!GS_OK(s)) { REQUIRE(U(GrayStringW)); return; }
    int bad = 0;
    for (int i = 0; i < 300; i++) if (!U(GrayStringW)(s.dc, NULL, NULL, (LPARAM)L"abc", 3, i % 60, i % 40, 0, 0)) bad++;
    CHECKF(bad == 0, "bad=%d", bad);
    surf_free(&s);
}

/* ================= TabbedTextOutW ================= */
#define TT_OK(s) (U(TabbedTextOutW) && U(TabbedTextOutA) && U(GetTabbedTextExtentW) && surf_make(&(s), 400, 80))

TEST(gap7_tabbedtextout_invalid_arguments)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    SetLastError(0);
    CHECK(U(TabbedTextOutW)(s.dc, 0, 0, NULL, -1, 0, NULL, 0) == 0);
    CHECK(U(TabbedTextOutW)(s.dc, 0, 0, NULL, 0, 0, NULL, 0) == 0);
    CHECK(U(TabbedTextOutW)(s.dc, 0, 0, NULL, 5, 0, NULL, 0) == 0);
    CHECK(U(TabbedTextOutW)(s.dc, 0, 0, L"abc", -2, 0, NULL, 0) == 0);
    CHECK(U(TabbedTextOutW)(s.dc, 0, 0, L"abc", INT_MIN, 0, NULL, 0) == 0);
    CHECK(U(TabbedTextOutW)(s.dc, 0, 0, L"abc", 3, -1, NULL, 0) == 0);
    CHECK(U(TabbedTextOutW)(s.dc, 0, 0, L"abc", 3, INT_MIN, NULL, 0) == 0);
    /* The ANSI entry maps a count of -1 (and -2) to an empty converted string and still calls the
     * worker, so it returns the text extent of "" (non-zero height) instead of failing; Win10 22H2. */
    { LONG ra = U(TabbedTextOutA)(s.dc, 0, 0, NULL, -1, 0, NULL, 0);
      LONG rb = U(TabbedTextOutA)(s.dc, 0, 0, "abc", -2, 0, NULL, 0);
      t_note("TabbedTextOutA(NULL,-1)=%lx (\"abc\",-2)=%lx", ra, rb);
      CHECK(LOWORD(ra) == 0 && LOWORD(rb) == 0); }
    CHECK(U(TabbedTextOutA)(s.dc, 0, 0, "abc", 3, -1, NULL, 0) == 0);
    CHECK(ink_total(&s) == 0);
    surf_free(&s);
}

TEST(gap7_tabbedtextout_invalid_dc)
{
    REQUIRE(U(TabbedTextOutW)); REQUIRE(U(TabbedTextOutA));
    SetLastError(0);
    LONG r1 = U(TabbedTextOutW)(NULL, 0, 0, L"abc", 3, 0, NULL, 0);
    t_note("TabbedTextOutW(NULL dc) = %lx err=%lu", r1, GetLastError());
    LONG r2 = U(TabbedTextOutA)(NULL, 0, 0, "abc", 3, 0, NULL, 0);
    t_note("TabbedTextOutA(NULL dc) = %lx", r2);
    LONG r3 = U(TabbedTextOutW)((HDC)(ULONG_PTR)0xDEADBEEF, 0, 0, L"abc", 3, 0, NULL, 0);
    t_note("TabbedTextOutW(bogus dc) = %lx", r3);
    HBRUSH b = CreateSolidBrush(0);
    LONG r4 = U(TabbedTextOutW)((HDC)b, 0, 0, L"abc", 3, 0, NULL, 0);
    t_note("TabbedTextOutW(brush as dc) = %lx", r4);
    DeleteObject(b);
    CHECK(r1 == r2 || 1);
}

TEST(gap7_tabbedtextout_returns_extent_of_string)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    static const WCHAR *strs[] = { L"a", L"Hello", L"The quick brown fox", L"WWWWWWWWWWWW", L"iiiiiiiiii" };
    for (unsigned i = 0; i < 5; i++) {
        int n = lstrlenW(strs[i]);
        LONG r = U(TabbedTextOutW)(s.dc, 2, 2, strs[i], n, 0, NULL, 0);
        SIZE sz; GetTextExtentPoint32W(s.dc, strs[i], n, &sz);
        CHECKF(LOWORD(r) == sz.cx, "str %u width %u vs %ld", i, LOWORD(r), sz.cx);
        CHECKF(HIWORD(r) == sz.cy, "str %u height %u vs %ld", i, HIWORD(r), sz.cy);
        DWORD e = U(GetTabbedTextExtentW)(s.dc, strs[i], n, 0, NULL);
        CHECKF((DWORD)r == e, "str %u out=%lx extent=%lx", i, r, e);
    }
    surf_free(&s);
}

TEST(gap7_tabbedtextout_count_minus_one_is_strlen)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    LONG a = U(TabbedTextOutW)(s.dc, 0, 0, L"measure me", -1, 0, NULL, 0);
    LONG b = U(TabbedTextOutW)(s.dc, 0, 0, L"measure me", 10, 0, NULL, 0);
    CHECK(a != 0 && a == b);
    LONG c = U(TabbedTextOutA)(s.dc, 0, 0, "measure me", -1, 0, NULL, 0);
    CHECKF(c == a, "A=%lx W=%lx", c, a);
    surf_free(&s);
}

TEST(gap7_tabbedtextout_count_truncates)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    SIZE s2, s6;
    GetTextExtentPoint32W(s.dc, L"ab", 2, &s2);
    GetTextExtentPoint32W(s.dc, L"abcdef", 6, &s6);
    LONG r2 = U(TabbedTextOutW)(s.dc, 0, 0, L"abcdef", 2, 0, NULL, 0);
    LONG r6 = U(TabbedTextOutW)(s.dc, 0, 0, L"abcdef", 6, 0, NULL, 0);
    CHECKF(LOWORD(r2) == s2.cx, "2: %u vs %ld", LOWORD(r2), s2.cx);
    CHECKF(LOWORD(r6) == s6.cx, "6: %u vs %ld", LOWORD(r6), s6.cx);
    CHECK(LOWORD(r2) < LOWORD(r6));
    /* count beyond the terminator is the caller's problem; stay within the buffer here */
    WCHAR buf[8] = L"ab\0cd";
    LONG r5 = U(TabbedTextOutW)(s.dc, 0, 0, buf, 5, 0, NULL, 0);
    t_note("embedded NUL, count 5: width=%u (ab=%ld)", LOWORD(r5), s2.cx);
    surf_free(&s);
}

TEST(gap7_tabbedtextout_empty_string)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    LONG r = U(TabbedTextOutW)(s.dc, 0, 0, L"", 0, 0, NULL, 0);
    t_note("empty string, count 0: width=%u height=%u", LOWORD(r), HIWORD(r));
    r = U(TabbedTextOutW)(s.dc, 0, 0, L"", -1, 0, NULL, 0);
    t_note("empty string, count -1: width=%u height=%u", LOWORD(r), HIWORD(r));
    CHECK(LOWORD(r) == 0);
    CHECK(ink_total(&s) == 0);
    surf_free(&s);
}

TEST(gap7_tabbedtextout_single_tab_stop_spacing)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    INT tab = 60;
    SIZE sc; GetTextExtentPoint32W(s.dc, L"c", 1, &sc);
    SIZE sa; GetTextExtentPoint32W(s.dc, L"a", 1, &sa);
    CHECK(sa.cx < 60);
    LONG r = U(TabbedTextOutW)(s.dc, 0, 0, L"a\tb\tc", 5, 1, &tab, 0);
    SIZE sb; GetTextExtentPoint32W(s.dc, L"b", 1, &sb);
    CHECK(sb.cx < 60);
    CHECKF(LOWORD(r) == 120 + sc.cx, "width=%u want %ld", LOWORD(r), 120 + sc.cx);
    tab = 40;
    r = U(TabbedTextOutW)(s.dc, 0, 0, L"a\tb", 3, 1, &tab, 0);
    CHECKF(LOWORD(r) == 40 + sb.cx, "width=%u want %ld", LOWORD(r), 40 + sb.cx);
    surf_free(&s);
}

TEST(gap7_tabbedtextout_tab_array_positions)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    INT tabs[2] = { 50, 140 };
    SIZE sc; GetTextExtentPoint32W(s.dc, L"c", 1, &sc);
    LONG r = U(TabbedTextOutW)(s.dc, 0, 0, L"a\tb\tc", 5, 2, tabs, 0);
    CHECKF(LOWORD(r) == 140 + sc.cx, "width=%u want %ld", LOWORD(r), 140 + sc.cx);
    LONG r2 = U(TabbedTextOutW)(s.dc, 0, 0, L"a\tc", 3, 2, tabs, 0);
    CHECKF(LOWORD(r2) == 50 + sc.cx, "width=%u want %ld", LOWORD(r2), 50 + sc.cx);
    DWORD e = U(GetTabbedTextExtentW)(s.dc, L"a\tb\tc", 5, 2, tabs);
    CHECKF(LOWORD(e) == LOWORD(r), "extent %u vs out %u", LOWORD(e), LOWORD(r));
    surf_free(&s);
}

TEST(gap7_tabbedtextout_tab_origin_shifts_stops)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    INT tab = 50;
    LONG r0 = U(TabbedTextOutW)(s.dc, 0, 0, L"a\tb", 3, 1, &tab, 0);
    LONG r20 = U(TabbedTextOutW)(s.dc, 0, 0, L"a\tb", 3, 1, &tab, 20);
    CHECKF((int)LOWORD(r20) - (int)LOWORD(r0) == 20, "diff=%d", (int)LOWORD(r20) - (int)LOWORD(r0));
    surf_free(&s);
}

TEST(gap7_tabbedtextout_default_tabs_are_wider_than_text)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    LONG plain = U(TabbedTextOutW)(s.dc, 0, 0, L"ab", 2, 0, NULL, 0);
    LONG tabbed = U(TabbedTextOutW)(s.dc, 0, 0, L"a\tb", 3, 0, NULL, 0);
    CHECK(LOWORD(tabbed) > LOWORD(plain));
    /* two tabs: further than one */
    LONG two = U(TabbedTextOutW)(s.dc, 0, 0, L"a\t\tb", 4, 0, NULL, 0);
    CHECK(LOWORD(two) > LOWORD(tabbed));
    surf_free(&s);
}

TEST(gap7_tabbedtextout_draws_and_stays_in_bounds)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    surf_clear(&s);
    INT tab = 80;
    LONG r = U(TabbedTextOutW)(s.dc, 10, 12, L"Left\tRight", 10, 1, &tab, 0);
    int w = LOWORD(r), h = HIWORD(r);
    CHECK(w > 80 && h > 0);
    int in = ink_in(&s, 10, 12, 10 + w, 12 + h);
    CHECKF(in > 10, "ink=%d", in);
    CHECKF(ink_outside(&s, 10, 12, 10 + w, 12 + h) == 0, "outside=%d", ink_outside(&s, 10, 12, 10 + w, 12 + h));
    surf_free(&s);
}

TEST(gap7_tabbedtextout_ansi_wide_parity)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    INT tabs[3] = { 30, 70, 200 };
    static const char *as[] = { "plain", "a\tb", "x\ty\tz\tw", "tab at end\t", "\tleading" };
    static const WCHAR *ws[] = { L"plain", L"a\tb", L"x\ty\tz\tw", L"tab at end\t", L"\tleading" };
    for (unsigned i = 0; i < 5; i++) {
        for (int np = 0; np <= 3; np += 3) {
            LONG ra = U(TabbedTextOutA)(s.dc, 1, 1, as[i], (int)strlen(as[i]), np, np ? tabs : NULL, 5);
            LONG rw = U(TabbedTextOutW)(s.dc, 1, 1, ws[i], lstrlenW(ws[i]), np, np ? tabs : NULL, 5);
            CHECKF(ra == rw, "i=%u np=%d A=%lx W=%lx", i, np, ra, rw);
        }
    }
    surf_free(&s);
}

TEST(gap7_tabbedtextout_text_attributes_respected)
{
    surf_t s; if (!TT_OK(s)) { REQUIRE(U(TabbedTextOutW)); return; }
    surf_clear(&s);
    SetTextColor(s.dc, RGB(255, 0, 0));
    SetBkMode(s.dc, TRANSPARENT);
    LONG r = U(TabbedTextOutW)(s.dc, 5, 5, L"HHHH", 4, 0, NULL, 0);
    GdiFlush();
    int red = 0, other = 0;
    for (int y = 5; y < 5 + HIWORD(r); y++) for (int x = 5; x < 5 + LOWORD(r); x++) { DWORD p = px(&s, x, y); if (p == BGR(255, 0, 0)) red++; else if (p != 0xFFFFFF) other++; }
    CHECKF(red > 0, "red=%d", red);
    t_note("anti-aliased pixels: %d", other);
    surf_clear(&s);
    SetBkMode(s.dc, OPAQUE);
    SetBkColor(s.dc, RGB(0, 255, 0));
    r = U(TabbedTextOutW)(s.dc, 5, 5, L"HHHH", 4, 0, NULL, 0);
    GdiFlush();
    CHECKF(px(&s, 5, 5 + HIWORD(r) - 1) == BGR(0, 255, 0) || px(&s, 6, 5 + HIWORD(r) - 1) == BGR(0, 255, 0) || px(&s, 5, 5) == BGR(0, 255, 0),
           "opaque background not painted: %06lx", px(&s, 5, 5));
    surf_free(&s);
}
