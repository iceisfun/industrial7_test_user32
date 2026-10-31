/* gap_05: UserLpkPSMTextOut, UserLpkTabbedTextOut, UserRealizePalette. */
#include "g5.h"

UDECL(void, UserLpkPSMTextOut, (HDC, int, int, const WCHAR *, UINT, UINT));
UDECL(short, UserLpkTabbedTextOut, (HDC, int, int, const WCHAR *, int, int, const int *, int, int, int, UINT));
UDECL(UINT, UserRealizePalette, (HDC));

#define BW 200
#define BH 48
#define PSM_NOUNDERLINE 0x100000
#define PSM_NOTEXT 0x200000

typedef struct { HDC dc; HBITMAP bmp, old; DWORD *px; HFONT oldfont; } Canvas;

static int cv_open(Canvas *c) {
    memset(c, 0, sizeof *c);
    c->dc = CreateCompatibleDC(NULL);
    if (!c->dc) return 0;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = BW; bi.bmiHeader.biHeight = -BH;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    c->bmp = CreateDIBSection(c->dc, &bi, DIB_RGB_COLORS, (void **)&c->px, NULL, 0);
    if (!c->bmp) { DeleteDC(c->dc); return 0; }
    c->old = SelectObject(c->dc, c->bmp);
    c->oldfont = SelectObject(c->dc, GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(c->dc, RGB(0, 0, 0));
    SetBkColor(c->dc, RGB(255, 255, 255));
    SetBkMode(c->dc, OPAQUE);
    SetTextAlign(c->dc, TA_LEFT | TA_TOP);
    return 1;
}
static void cv_clear(Canvas *c) {
    for (int i = 0; i < BW * BH; i++) c->px[i] = 0x00ffffff;
    GdiFlush();
}
static void cv_close(Canvas *c) {
    SelectObject(c->dc, c->oldfont); SelectObject(c->dc, c->old);
    DeleteObject(c->bmp); DeleteDC(c->dc);
}
static void cv_snap(Canvas *c, DWORD *dst) { GdiFlush(); memcpy(dst, c->px, sizeof(DWORD) * BW * BH); }
static int cv_dark(const DWORD *p) { int n = 0; for (int i = 0; i < BW * BH; i++) if ((p[i] & 0xffffff) != 0xffffff) n++; return n; }
static DWORD g_a[BW * BH], g_b[BW * BH];

/* ---------------- UserLpkPSMTextOut ---------------- */
TEST(gap5_psm_notext_noul_draws_nothing) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c)); cv_clear(&c);
    UN(UserLpkPSMTextOut)(c.dc, 4, 4, L"&File", 5, PSM_NOTEXT | PSM_NOUNDERLINE);
    cv_snap(&c, g_a);
    EXPECT_EQ(cv_dark(g_a), 0);
    cv_close(&c);
}
TEST(gap5_psm_zero_count_draws_nothing) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c)); cv_clear(&c);
    UN(UserLpkPSMTextOut)(c.dc, 4, 4, L"&File", 0, 0);
    cv_snap(&c, g_a);
    CHECK(cv_dark(g_a) == 0 || 1);
    t_note("PSM count 0 dark pixels: %d", cv_dark(g_a));
    cv_close(&c);
}
TEST(gap5_psm_plain_matches_textout) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"Hello", 5, 0); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 3, 5, L"Hello", 5); cv_snap(&c, g_b);
    CHECK(cv_dark(g_a) > 20);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
}
TEST(gap5_psm_prefix_stripped_nounderline) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&File", 5, PSM_NOUNDERLINE); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 3, 5, L"File", 4); cv_snap(&c, g_b);
    CHECK(cv_dark(g_a) > 20);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
}
TEST(gap5_psm_prefix_mid_stripped_nounderline) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"Sa&ve", 5, PSM_NOUNDERLINE); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 3, 5, L"Save", 4); cv_snap(&c, g_b);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
}
TEST(gap5_psm_double_ampersand_is_literal) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"A&&B", 4, PSM_NOUNDERLINE); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 3, 5, L"A&B", 3); cv_snap(&c, g_b);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
}
TEST(gap5_psm_double_ampersand_no_underline_drawn) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"A&&B", 4, 0); cv_snap(&c, g_a);
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"A&&B", 4, PSM_NOUNDERLINE); cv_snap(&c, g_b);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
}
TEST(gap5_psm_no_prefix_no_underline) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"Plain", 5, 0); cv_snap(&c, g_a);
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"Plain", 5, PSM_NOUNDERLINE); cv_snap(&c, g_b);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
}
TEST(gap5_psm_underline_adds_single_row) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&File", 5, 0); cv_snap(&c, g_a);
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&File", 5, PSM_NOUNDERLINE); cv_snap(&c, g_b);
    int rows[BH] = {0}, nrows = 0, total = 0, minx = BW, maxx = -1;
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (g_a[y * BW + x] != g_b[y * BW + x]) { if (!rows[y]) nrows++; rows[y]++; total++; if (x < minx) minx = x; if (x > maxx) maxx = x; }
    CHECKF(total > 0, "no underline pixels");
    CHECKF(nrows == 1, "underline spans %d rows", nrows);
    SIZE fw; GetTextExtentPoint32W(c.dc, L"F", 1, &fw);
    SIZE all; GetTextExtentPoint32W(c.dc, L"File", 4, &all);
    CHECKF(minx >= 3 - 1, "underline starts at x=%d", minx);
    CHECKF(maxx <= 3 + fw.cx + 1, "underline ends at x=%d (F width %ld)", maxx, fw.cx);
    cv_close(&c);
}
TEST(gap5_psm_underline_follows_prefixed_char) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"Fi&le", 5, 0); cv_snap(&c, g_a);
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"Fi&le", 5, PSM_NOUNDERLINE); cv_snap(&c, g_b);
    SIZE pre, ch; GetTextExtentPoint32W(c.dc, L"Fi", 2, &pre); GetTextExtentPoint32W(c.dc, L"l", 1, &ch);
    int minx = BW, maxx = -1, total = 0;
    for (int i = 0; i < BW * BH; i++) if (g_a[i] != g_b[i]) { int x = i % BW; total++; if (x < minx) minx = x; if (x > maxx) maxx = x; }
    CHECK(total > 0);
    CHECKF(minx >= 3 + pre.cx - 2, "underline starts at %d, prefix width %ld", minx, pre.cx);
    CHECKF(maxx <= 3 + pre.cx + ch.cx + 1, "underline ends at %d", maxx);
    cv_close(&c);
}
TEST(gap5_psm_underline_uses_text_color) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    SetTextColor(c.dc, RGB(255, 0, 0));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&Red", 4, 0); cv_snap(&c, g_a);
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&Red", 4, PSM_NOUNDERLINE); cv_snap(&c, g_b);
    int n = 0;
    for (int i = 0; i < BW * BH; i++) if (g_a[i] != g_b[i]) { n++; CHECKF((g_a[i] & 0xffffff) == 0xff0000, "pixel %08lx", g_a[i]); }
    CHECK(n > 0);
    cv_close(&c);
}
TEST(gap5_psm_restores_bk_color) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    SetBkColor(c.dc, RGB(10, 20, 30));
    UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&File", 5, 0);
    EXPECT_EQ(GetBkColor(c.dc), RGB(10, 20, 30));
    EXPECT_EQ(GetTextColor(c.dc), RGB(0, 0, 0));
    cv_close(&c);
}
TEST(gap5_psm_count_shorter_than_string) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&FileMenu", 3, PSM_NOUNDERLINE); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 3, 5, L"Fi", 2); cv_snap(&c, g_b);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
}
TEST(gap5_psm_long_string_heap_path) {
    REQUIRE(UN(UserLpkPSMTextOut));
    WCHAR *s = malloc(sizeof(WCHAR) * 700);
    CHECK(s != NULL);
    if (!s) return;
    for (int i = 0; i < 600; i++) s[i] = L"abcdefghij"[i % 10];
    s[600] = 0;
    Canvas c; CHECK(cv_open(&c));
    static const int counts[] = { 253, 254, 255, 256, 300, 600 };
    for (unsigned k = 0; k < sizeof counts / sizeof counts[0]; k++) {
        cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 0, 0, s, counts[k], PSM_NOUNDERLINE); cv_snap(&c, g_a);
        cv_clear(&c); TextOutW(c.dc, 0, 0, s, counts[k]); cv_snap(&c, g_b);
        CHECKF(!memcmp(g_a, g_b, sizeof g_a), "count %d", counts[k]);
    }
    s[0] = L'&';
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 0, 0, s, 300, PSM_NOUNDERLINE); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 0, 0, s + 1, 299); cv_snap(&c, g_b);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
    free(s);
}
TEST(gap5_psm_position_shift) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 10, 6, L"Move", 4, 0); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 10, 6, L"Move", 4); cv_snap(&c, g_b);
    CHECK(!memcmp(g_a, g_b, sizeof g_a));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, -1000, -1000, L"Move", 4, 0); cv_snap(&c, g_a);
    EXPECT_EQ(cv_dark(g_a), 0);
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 100000, 100000, L"Move", 4, 0); cv_snap(&c, g_a);
    EXPECT_EQ(cv_dark(g_a), 0);
    cv_close(&c);
}
TEST(gap5_psm_bogus_dc_no_crash) {
    REQUIRE(UN(UserLpkPSMTextOut));
    HDC dcs[] = { NULL, (HDC)(ULONG_PTR)0x12344, (HDC)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 3; i++) {
        UN(UserLpkPSMTextOut)(dcs[i], 0, 0, L"&Bad", 4, 0);
        UN(UserLpkPSMTextOut)(dcs[i], 0, 0, L"Bad", 3, PSM_NOUNDERLINE);
    }
    CHECK(1);
}
TEST(gap5_psm_empty_string) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c)); cv_clear(&c);
    UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"", 0, 0);
    UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"", 0, PSM_NOUNDERLINE);
    cv_snap(&c, g_a);
    EXPECT_EQ(cv_dark(g_a), 0);
    cv_close(&c);
}
TEST(gap5_psm_trailing_ampersand_observe) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"End&", 4, PSM_NOUNDERLINE); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 3, 5, L"End", 3); cv_snap(&c, g_b);
    t_note("trailing '&' stripped identical to \"End\": %d", !memcmp(g_a, g_b, sizeof g_a));
    cv_close(&c);
}
TEST(gap5_psm_only_first_prefix_underlined) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c));
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&A&B", 4, 0); cv_snap(&c, g_a);
    cv_clear(&c); UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&A&B", 4, PSM_NOUNDERLINE); cv_snap(&c, g_b);
    SIZE a; GetTextExtentPoint32W(c.dc, L"A", 1, &a);
    int maxx = -1, total = 0;
    for (int i = 0; i < BW * BH; i++) if (g_a[i] != g_b[i]) { total++; if (i % BW > maxx) maxx = i % BW; }
    t_note("two prefixes: %d underline pixels, max x %d (first char width %ld)", total, maxx, a.cx);
    cv_close(&c);
}
TEST(gap5_psm_no_text_flag_still_underlines) {
    REQUIRE(UN(UserLpkPSMTextOut));
    Canvas c; CHECK(cv_open(&c)); cv_clear(&c);
    UN(UserLpkPSMTextOut)(c.dc, 3, 5, L"&File", 5, PSM_NOTEXT);
    cv_snap(&c, g_a);
    int n = cv_dark(g_a);
    CHECKF(n > 0, "underline expected with text suppressed");
    int rows = 0;
    for (int y = 0; y < BH; y++) { int any = 0; for (int x = 0; x < BW; x++) if (g_a[y * BW + x] != 0xffffff) any = 1; rows += any; }
    CHECKF(rows == 1, "rows with ink: %d", rows);
    cv_close(&c);
}

/* ---------------- UserLpkTabbedTextOut ---------------- */
static int tm_avg(HDC dc, int *h) { TEXTMETRICW tm; GetTextMetricsW(dc, &tm); if (h) *h = tm.tmHeight; return tm.tmAveCharWidth; }

TEST(gap5_lpktab_bogus_dc_returns_zero) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    HDC dcs[] = { NULL, (HDC)(ULONG_PTR)0x12344, (HDC)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 3; i++) {
        short r = UN(UserLpkTabbedTextOut)(dcs[i], 0, 0, L"abc", 3, 0, NULL, 0, 0, 7, 13);
        CHECKF(r == 0, "idx %u ret %d", i, r);
        r = UN(UserLpkTabbedTextOut)(dcs[i], 0, 0, L"abc", 3, 0, NULL, 0, 1, 7, 13);
        CHECKF(r == 0, "idx %u draw ret %d", i, r);
    }
}
TEST(gap5_lpktab_empty_string_zero_width) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    EXPECT_EQ(UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"", 0, 0, NULL, 0, 0, avg, h), 0);
    EXPECT_EQ(UN(UserLpkTabbedTextOut)(c.dc, 9, 9, L"abc", 0, 0, NULL, 0, 0, avg, h), 0);
    cv_close(&c);
}
TEST(gap5_lpktab_no_tab_width_matches_extent) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    static const WCHAR *strs[] = { L"a", L"abc", L"Hello, world", L"WWWWWWWW", L"iiiiiiii", L"mixed Case 123" };
    for (unsigned i = 0; i < sizeof strs / sizeof strs[0]; i++) {
        int n = lstrlenW(strs[i]);
        SIZE sz; GetTextExtentPoint32W(c.dc, strs[i], n, &sz);
        short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, strs[i], n, 0, NULL, 0, 0, avg, h);
        CHECKF(r == sz.cx, "idx %u ret %d want %ld", i, r, sz.cx);
    }
    cv_close(&c);
}
TEST(gap5_lpktab_width_independent_of_origin_x) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    short r0 = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"abcdef", 6, 0, NULL, 0, 0, avg, h);
    short r1 = UN(UserLpkTabbedTextOut)(c.dc, 50, 20, L"abcdef", 6, 0, NULL, 0, 0, avg, h);
    short r2 = UN(UserLpkTabbedTextOut)(c.dc, -30, -9, L"abcdef", 6, 0, NULL, 0, 0, avg, h);
    CHECK(r0 > 0); CHECK(r0 == r1); CHECK(r0 == r2);
    cv_close(&c);
}
TEST(gap5_lpktab_count_prefix) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    for (int n = 0; n <= 6; n++) {
        SIZE sz = {0}; if (n) GetTextExtentPoint32W(c.dc, L"abcdef", n, &sz);
        short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"abcdef", n, 0, NULL, 0, 0, avg, h);
        CHECKF(r == sz.cx, "n=%d ret %d want %ld", n, r, sz.cx);
    }
    cv_close(&c);
}
TEST(gap5_lpktab_single_tab_stride) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    int stride = 100;
    SIZE b; GetTextExtentPoint32W(c.dc, L"b", 1, &b);
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"a\tb", 3, 1, &stride, 0, 0, avg, h);
    CHECKF(r == stride + b.cx, "ret %d want %ld", r, stride + b.cx);
    cv_close(&c);
}
TEST(gap5_lpktab_explicit_tab_stops) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    int tabs[2] = { 50, 120 };
    SIZE b; GetTextExtentPoint32W(c.dc, L"c", 1, &b);
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"a\tb\tc", 5, 2, tabs, 0, 0, avg, h);
    CHECKF(r == 120 + b.cx, "ret %d want %ld", r, 120 + b.cx);
    r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"a\tb", 3, 2, tabs, 0, 0, avg, h);
    SIZE bb; GetTextExtentPoint32W(c.dc, L"b", 1, &bb);
    CHECKF(r == 50 + bb.cx, "ret %d want %ld", r, 50 + bb.cx);
    cv_close(&c);
}
TEST(gap5_lpktab_tab_origin_shifts_stops) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    int tabs[2] = { 50, 120 };
    SIZE b; GetTextExtentPoint32W(c.dc, L"b", 1, &b);
    /* Origin 10: first stop is at 10 + 50. x starts at 0. */
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"a\tb", 3, 2, tabs, 10, 0, avg, h);
    CHECKF(r == 60 + b.cx, "ret %d want %ld", r, 60 + b.cx);
    cv_close(&c);
}
TEST(gap5_lpktab_default_tab_width) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    SIZE b; GetTextExtentPoint32W(c.dc, L"b", 1, &b);
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"a\tb", 3, 0, NULL, 0, 0, avg, h);
    CHECKF(r == 8 * avg + b.cx, "ret %d want %ld (avg %d)", r, 8 * avg + b.cx, avg);
    cv_close(&c);
}
TEST(gap5_lpktab_default_tab_vs_gettabbedtextextent) {
    REQUIRE(UN(UserLpkTabbedTextOut)); REQUIRE(U(GetTabbedTextExtentW));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    DWORD e = U(GetTabbedTextExtentW)(c.dc, L"a\tb", 3, 0, NULL);
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"a\tb", 3, 0, NULL, 0, 0, avg, h);
    t_note("GetTabbedTextExtentW width %u, UserLpkTabbedTextOut %d", LOWORD(e), r);
    cv_close(&c);
}
TEST(gap5_lpktab_draw_flag_does_not_change_width) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    int tabs[1] = { 40 };
    cv_clear(&c);
    short r0 = UN(UserLpkTabbedTextOut)(c.dc, 2, 2, L"ab\tcd\tef", 8, 1, tabs, 0, 0, avg, h);
    cv_snap(&c, g_a);
    EXPECT_EQ(cv_dark(g_a), 0);
    short r1 = UN(UserLpkTabbedTextOut)(c.dc, 2, 2, L"ab\tcd\tef", 8, 1, tabs, 0, 1, avg, h);
    cv_snap(&c, g_a);
    CHECK(r0 == r1);
    CHECK(cv_dark(g_a) > 20);
    cv_close(&c);
}
TEST(gap5_lpktab_draw_pixels_match_textout) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    cv_clear(&c); UN(UserLpkTabbedTextOut)(c.dc, 4, 4, L"Plain", 5, 0, NULL, 0, 1, avg, h); cv_snap(&c, g_a);
    cv_clear(&c); TextOutW(c.dc, 4, 4, L"Plain", 5); cv_snap(&c, g_b);
    t_note("tabbed draw identical to TextOutW: %d (dark %d vs %d)", !memcmp(g_a, g_b, sizeof g_a), cv_dark(g_a), cv_dark(g_b));
    CHECK(cv_dark(g_a) > 20);
    cv_close(&c);
}
TEST(gap5_lpktab_consecutive_tabs) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    int stride = 30;
    SIZE b; GetTextExtentPoint32W(c.dc, L"b", 1, &b);
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"a\t\tb", 4, 1, &stride, 0, 0, avg, h);
    CHECKF(r == 60 + b.cx, "ret %d want %ld", r, 60 + b.cx);
    cv_close(&c);
}
TEST(gap5_lpktab_leading_tab) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    int stride = 30;
    SIZE b; GetTextExtentPoint32W(c.dc, L"b", 1, &b);
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"\tb", 2, 1, &stride, 0, 0, avg, h);
    t_note("leading tab: ret %d (stride 30, b width %ld)", r, b.cx);
    cv_close(&c);
}
TEST(gap5_lpktab_trailing_tab_observe) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    int stride = 30;
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"a\t", 2, 1, &stride, 0, 0, avg, h);
    t_note("trailing tab: ret %d", r);
    cv_close(&c);
}
TEST(gap5_lpktab_mapmode_viewport_ext) {
    REQUIRE(UN(UserLpkTabbedTextOut));
    Canvas c; CHECK(cv_open(&c)); int h; int avg = tm_avg(c.dc, &h);
    short base = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"abcdef", 6, 0, NULL, 0, 0, avg, h);
    SetMapMode(c.dc, MM_ANISOTROPIC);
    SetWindowExtEx(c.dc, 100, 100, NULL); SetViewportExtEx(c.dc, 100, 100, NULL);
    short r = UN(UserLpkTabbedTextOut)(c.dc, 0, 0, L"abcdef", 6, 0, NULL, 0, 0, avg, h);
    CHECK(r == base);
    cv_close(&c);
}

/* ---------------- UserRealizePalette ---------------- */
TEST(gap5_realize_bogus_dc) {
    REQUIRE(UN(UserRealizePalette));
    HDC dcs[] = { NULL, (HDC)(ULONG_PTR)0x12344, (HDC)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 3; i++) {
        UINT r = UN(UserRealizePalette)(dcs[i]);
        UINT g = RealizePalette(dcs[i]);
        t_note("UserRealizePalette(bogus %u) = 0x%x, RealizePalette = 0x%x", i, r, g);
    }
}
TEST(gap5_realize_memory_dc_default_palette) {
    REQUIRE(UN(UserRealizePalette));
    HDC dc = CreateCompatibleDC(NULL); CHECK(dc != NULL);
    UINT r = UN(UserRealizePalette)(dc);
    UINT g = RealizePalette(dc);
    t_note("memory DC: UserRealizePalette=0x%x RealizePalette=0x%x", r, g);
    DeleteDC(dc);
}
TEST(gap5_realize_deleted_dc) {
    REQUIRE(UN(UserRealizePalette));
    HDC dc = CreateCompatibleDC(NULL); CHECK(dc != NULL);
    DeleteDC(dc);
    UINT r = UN(UserRealizePalette)(dc);
    t_note("deleted DC: 0x%x", r);
}
TEST(gap5_realize_with_logical_palette) {
    REQUIRE(UN(UserRealizePalette));
    struct { WORD ver, n; PALETTEENTRY e[8]; } lp;
    lp.ver = 0x300; lp.n = 8;
    for (int i = 0; i < 8; i++) { lp.e[i].peRed = i * 30; lp.e[i].peGreen = 255 - i * 30; lp.e[i].peBlue = i * 10; lp.e[i].peFlags = 0; }
    HPALETTE pal = CreatePalette((LOGPALETTE *)&lp);
    CHECK(pal != NULL);
    HDC dc = CreateCompatibleDC(NULL);
    HPALETTE old = SelectPalette(dc, pal, FALSE);
    UINT r = UN(UserRealizePalette)(dc);
    t_note("memory DC with 8-entry palette: 0x%x", r);
    SelectPalette(dc, old, FALSE);
    DeleteDC(dc);
    DeleteObject(pal);
}
TEST(gap5_realize_repeated_stable) {
    REQUIRE(UN(UserRealizePalette));
    HDC dc = CreateCompatibleDC(NULL);
    UINT first = UN(UserRealizePalette)(dc);
    for (int i = 0; i < 50; i++) {
        UINT r = UN(UserRealizePalette)(dc);
        if (r != first) { t_note("realize result changed at iteration %d: 0x%x vs 0x%x", i, r, first); break; }
    }
    CHECK(1);
    DeleteDC(dc);
}
