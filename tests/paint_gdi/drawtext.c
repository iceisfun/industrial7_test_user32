#include "pg.h"

#define DTW 400
#define DTH 200
typedef struct { HDC dc; HBITMAP bm, old; HFONT oldf; DWORD *bits; int cw, th, el; } dtctx;

static int dt_init(dtctx *c) {
    memset(c, 0, sizeof *c);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = DTW; bi.bmiHeader.biHeight = -DTH;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    c->dc = CreateCompatibleDC(NULL);
    c->bm = CreateDIBSection(c->dc, &bi, DIB_RGB_COLORS, (void **)&c->bits, NULL, 0);
    if (!c->dc || !c->bm) return 0;
    c->old = SelectObject(c->dc, c->bm);
    c->oldf = SelectObject(c->dc, GetStockObject(SYSTEM_FIXED_FONT));
    SetBkMode(c->dc, TRANSPARENT);
    SetTextColor(c->dc, 0);
    SetTextAlign(c->dc, TA_LEFT | TA_TOP);
    TEXTMETRICA tm; if (!GetTextMetricsA(c->dc, &tm)) return 0;
    c->cw = tm.tmAveCharWidth; c->th = tm.tmHeight; c->el = tm.tmExternalLeading;
    SIZE a, b;
    GetTextExtentPoint32A(c->dc, "i", 1, &a); GetTextExtentPoint32A(c->dc, "W", 1, &b);
    if (a.cx != b.cx || a.cx != c->cw) { t_note("system fixed font is not fixed pitch (%d/%d/%d)", (int)a.cx, (int)b.cx, c->cw); return 0; }
    return 1;
}
static void dt_clear(dtctx *c) { for (int i = 0; i < DTW * DTH; i++) c->bits[i] = 0xffffff; }
static void dt_done(dtctx *c) {
    if (c->dc) { if (c->oldf) SelectObject(c->dc, c->oldf); if (c->old) SelectObject(c->dc, c->old); DeleteDC(c->dc); }
    if (c->bm) DeleteObject(c->bm);
}
/* bounding box of non-white pixels; returns 0 if none */
static int dt_bbox(dtctx *c, RECT *r) {
    GdiFlush();
    int l = DTW, t = DTH, rr = -1, b = -1;
    for (int y = 0; y < DTH; y++) for (int x = 0; x < DTW; x++)
        if ((c->bits[y * DTW + x] & 0xffffff) != 0xffffff) { if (x < l) l = x; if (x > rr) rr = x; if (y < t) t = y; if (y > b) b = y; }
    if (rr < 0) return 0;
    r->left = l; r->top = t; r->right = rr + 1; r->bottom = b + 1;
    return 1;
}
#define SETUP() dtctx c; if (!dt_init(&c)) { dt_done(&c); t_skip("cannot set up fixed font context"); return; } dt_clear(&c)
#define DTA U(DrawTextA)
#define DTWF U(DrawTextW)

static int calc_a(dtctx *c, const char *s, int n, RECT *r, UINT f) {
    return DTA(c->dc, s, n, r, f | DT_CALCRECT);
}

TEST(paint_gdi_dt_calcrect_single_line) {
    REQUIRE(DTA); SETUP();
    RECT r = {10, 20, 300, 100};
    int h = calc_a(&c, "hello", -1, &r, DT_SINGLELINE | DT_LEFT | DT_TOP);
    EQ(h, c.th);
    CHECK_RECT(r, 10, 20, 10 + 5 * c.cw, 20 + c.th);
    dt_done(&c);
}
TEST(paint_gdi_dt_calcrect_counts) {
    REQUIRE(DTA); SETUP();
    for (int n = 1; n <= 12; n++) {
        RECT r = {0, 0, 400, 100};
        int h = calc_a(&c, "abcdefghijkl", n, &r, DT_SINGLELINE);
        CHECKF(h == c.th, "n=%d h=%d", n, h);
        CHECKF(r.right == n * c.cw && r.bottom == c.th, "n=%d got %ld,%ld", n, r.right, r.bottom);
    }
    RECT r = {0, 0, 400, 100};
    calc_a(&c, "abc", -1, &r, DT_SINGLELINE);
    CHECK(r.right == 3 * c.cw);
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "abcdef", 3, &r, DT_SINGLELINE);   /* only the first 3 characters */
    CHECK(r.right == 3 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_calcrect_does_not_draw) {
    REQUIRE(DTA); SETUP();
    RECT r = {10, 10, 300, 100}, b;
    calc_a(&c, "hello world", -1, &r, DT_SINGLELINE);
    CHECK(!dt_bbox(&c, &b));
    dt_done(&c);
}
TEST(paint_gdi_dt_draw_marks_pixels) {
    REQUIRE(DTA); SETUP();
    RECT r = {10, 10, 300, 100}, b;
    int h = DTA(c.dc, "Hello", -1, &r, DT_SINGLELINE | DT_LEFT | DT_TOP);
    EQ(h, c.th);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.left >= 10 && b.right <= 10 + 5 * c.cw);
    CHECK(b.top >= 10 && b.bottom <= 10 + c.th);
    CHECK_RECT(r, 10, 10, 300, 100);   /* rect untouched without CALCRECT */
    dt_done(&c);
}
TEST(paint_gdi_dt_wide_ansi_parity) {
    REQUIRE(DTA); REQUIRE(DTWF); SETUP();
    static const char *txt[] = { "a", "hello", "two words", "a b c d e f g", "x\ty", "line1\nline2", "&Pre&&fix" };
    for (unsigned i = 0; i < sizeof txt / sizeof txt[0]; i++) {
        WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, txt[i], -1, w, 64);
        for (int fl = 0; fl < 3; fl++) {
            UINT f = DT_CALCRECT | (fl == 0 ? DT_SINGLELINE : fl == 1 ? DT_WORDBREAK : DT_EXPANDTABS | DT_SINGLELINE);
            RECT a = {0, 0, 5 * c.cw, 200}, b = a;
            int ha = DTA(c.dc, txt[i], -1, &a, f), hb = DTWF(c.dc, w, -1, &b, f);
            CHECKF(ha == hb && a.right == b.right && a.bottom == b.bottom, "text %u flags %u: A %d (%ld,%ld) W %d (%ld,%ld)", i, fl, ha, a.right, a.bottom, hb, b.right, b.bottom);
        }
    }
    dt_done(&c);
}
TEST(paint_gdi_dt_newlines_multiline) {
    REQUIRE(DTA); SETUP();
    struct { const char *s; int lines, width; } t[] = {
        {"a\nb", 2, 1}, {"a\r\nb", 2, 1}, {"abc\nde\nf", 3, 3}, {"ab\n\ncd", 3, 2}, {"one", 1, 3},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT r = {0, 0, 400, 200};
        int h = calc_a(&c, t[i].s, -1, &r, DT_LEFT);
        CHECKF(h == t[i].lines * c.th, "case %u: h=%d want %d", i, h, t[i].lines * c.th);
        CHECKF(r.bottom == t[i].lines * c.th, "case %u: bottom %ld", i, r.bottom);
        CHECKF(r.right == t[i].width * c.cw, "case %u: right %ld want %d", i, r.right, t[i].width * c.cw);
    }
    dt_done(&c);
}
TEST(paint_gdi_dt_externalleading) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 400, 200};
    int h1 = calc_a(&c, "a\nb", -1, &r, DT_LEFT);
    r = (RECT){0, 0, 400, 200};
    int h2 = calc_a(&c, "a\nb", -1, &r, DT_LEFT | DT_EXTERNALLEADING);
    EQ(h1, 2 * c.th);
    EQ(h2, 2 * (c.th + c.el));
    dt_done(&c);
}
TEST(paint_gdi_dt_wordbreak_basic) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 10 * c.cw, 200};
    int h = calc_a(&c, "aaaa bbbb cccc", -1, &r, DT_WORDBREAK);
    EQ(h, 2 * c.th);
    CHECK(r.right == 9 * c.cw || r.right == 10 * c.cw);
    CHECK(r.bottom == 2 * c.th);
    r = (RECT){0, 0, 10 * c.cw, 200};
    h = calc_a(&c, "aaa bbb ccc ddd eee", -1, &r, DT_WORDBREAK);
    EQ(h, 3 * c.th);   /* "aaa bbb" / "ccc ddd" / "eee" */
    r = (RECT){0, 0, 100 * c.cw, 200};
    h = calc_a(&c, "aaa bbb ccc", -1, &r, DT_WORDBREAK);
    EQ(h, c.th);
    dt_done(&c);
}
TEST(paint_gdi_dt_wordbreak_long_word) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 10 * c.cw, 200};
    /* Win10 22H2 oracle: DT_WORDBREAK only breaks between words; a single word wider than the
     * rectangle is NOT split mid-word (height stays one line). */
    int h = calc_a(&c, "aaaaaaaaaaaaaaaaaaaa", -1, &r, DT_WORDBREAK);   /* 20 chars in 10 columns */
    EQ(h, c.th);
    r = (RECT){0, 0, 10 * c.cw, 200};
    h = calc_a(&c, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", -1, &r, DT_WORDBREAK);
    EQ(h, c.th);
    dt_done(&c);
}
TEST(paint_gdi_dt_wordbreak_narrow) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, c.cw, 200};
    int h = calc_a(&c, "abcd", -1, &r, DT_WORDBREAK);
    EQ(h, c.th);     /* Win10 22H2 oracle: a single word is never split, even when narrower than one glyph */
    dt_done(&c);
}
TEST(paint_gdi_dt_nowordbreak_ignores_width) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 2 * c.cw, 200};
    int h = calc_a(&c, "aaaa bbbb cccc", -1, &r, DT_LEFT);
    EQ(h, c.th);
    CHECK(r.right == 14 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_singleline_ignores_newline) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 400, 200};
    int h = calc_a(&c, "a\nb", -1, &r, DT_SINGLELINE);
    EQ(h, c.th);
    CHECK(r.bottom == c.th);
    dt_done(&c);
}
TEST(paint_gdi_dt_prefix_calc) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 400, 100};
    calc_a(&c, "&abc", -1, &r, DT_SINGLELINE);
    CHECK(r.right == 3 * c.cw);                         /* & consumed */
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "&abc", -1, &r, DT_SINGLELINE | DT_NOPREFIX);
    CHECK(r.right == 4 * c.cw);
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "a&&b", -1, &r, DT_SINGLELINE);
    CHECK(r.right == 3 * c.cw);                         /* && -> & */
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "a&&b", -1, &r, DT_SINGLELINE | DT_NOPREFIX);
    CHECK(r.right == 4 * c.cw);
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "abc&", -1, &r, DT_SINGLELINE);
    t_note("\"abc&\" width %ld (cw %d)", r.right, c.cw);
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "&a&b&c", -1, &r, DT_SINGLELINE);
    CHECK(r.right == 3 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_prefix_underline_pixels) {
    REQUIRE(DTA); SETUP();
    RECT r = {10, 10, 300, 100}, b1, b2;
    DTA(c.dc, "abc", -1, &r, DT_SINGLELINE | DT_NOPREFIX);
    CHECK(dt_bbox(&c, &b1));
    dt_clear(&c);
    r = (RECT){10, 10, 300, 100};
    DTA(c.dc, "a&bc", -1, &r, DT_SINGLELINE);
    CHECK(dt_bbox(&c, &b2));
    CHECK(b2.bottom >= b1.bottom);      /* underline reaches at or below the baseline row of plain text */
    dt_clear(&c);
    r = (RECT){10, 10, 300, 100};
    DTA(c.dc, "a&bc", -1, &r, DT_SINGLELINE | DT_HIDEPREFIX);
    RECT b3; CHECK(dt_bbox(&c, &b3));
    CHECK(b3.bottom <= b2.bottom);
    dt_done(&c);
}
TEST(paint_gdi_dt_tabs_expand) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 400, 100};
    int h = calc_a(&c, "a\tb", -1, &r, DT_SINGLELINE | DT_EXPANDTABS);
    EQ(h, c.th);
    CHECK(r.right == 9 * c.cw);       /* tab to column 8, then b */
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "\tb", -1, &r, DT_SINGLELINE | DT_EXPANDTABS);
    CHECK(r.right == 9 * c.cw);
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "abcdefgh\tb", -1, &r, DT_SINGLELINE | DT_EXPANDTABS);
    CHECK(r.right == 17 * c.cw);      /* 8 chars sit exactly on a stop: next stop at 16 */
    r = (RECT){0, 0, 400, 100};
    calc_a(&c, "a\t\tb", -1, &r, DT_SINGLELINE | DT_EXPANDTABS);
    CHECK(r.right == 17 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_tabs_not_expanded) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 400, 100};
    calc_a(&c, "a\tb", -1, &r, DT_SINGLELINE);
    t_note("a<TAB>b without DT_EXPANDTABS width %ld (cw %d)", r.right, c.cw);
    CHECK(r.right > 0);
    dt_done(&c);
}
TEST(paint_gdi_dt_tabstop_in_format) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 400, 100};
    DTA(c.dc, "a\tb", -1, &r, DT_SINGLELINE | DT_EXPANDTABS | DT_TABSTOP | (4 << 8));   /* no DT_CALCRECT allowed with TABSTOP */
    dt_clear(&c);
    r = (RECT){0, 0, 400, 100};
    DTA(c.dc, "a\tX", -1, &r, DT_SINGLELINE | DT_EXPANDTABS | DT_TABSTOP | (4 << 8));
    RECT b; CHECK(dt_bbox(&c, &b));
    CHECK(b.right <= 5 * c.cw + 1 && b.right > 4 * c.cw);     /* X occupies the 5th column */
    dt_clear(&c);
    r = (RECT){0, 0, 400, 100};
    DTA(c.dc, "a\tX", -1, &r, DT_SINGLELINE | DT_EXPANDTABS);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.right <= 9 * c.cw + 1 && b.right > 8 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_alignment_pixels) {
    REQUIRE(DTA); SETUP();
    RECT rc = {20, 20, 220, 120}, b;
    DTA(c.dc, "XXXX", -1, &(RECT){20, 20, 220, 120}, DT_SINGLELINE | DT_LEFT | DT_TOP);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.left >= 20 && b.left <= 20 + c.cw);
    CHECK(b.top >= 20 && b.top <= 20 + c.th / 2);
    dt_clear(&c);
    DTA(c.dc, "XXXX", -1, &(RECT){20, 20, 220, 120}, DT_SINGLELINE | DT_RIGHT | DT_TOP);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.right <= 220 && b.right >= 220 - c.cw);
    dt_clear(&c);
    DTA(c.dc, "XXXX", -1, &(RECT){20, 20, 220, 120}, DT_SINGLELINE | DT_CENTER | DT_TOP);
    CHECK(dt_bbox(&c, &b));
    CHECK(abs((b.left + b.right) / 2 - 120) <= c.cw);
    dt_clear(&c);
    DTA(c.dc, "XXXX", -1, &(RECT){20, 20, 220, 120}, DT_SINGLELINE | DT_LEFT | DT_BOTTOM);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.bottom <= 120 && b.bottom >= 120 - c.th);
    dt_clear(&c);
    DTA(c.dc, "XXXX", -1, &(RECT){20, 20, 220, 120}, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    CHECK(dt_bbox(&c, &b));
    CHECK(abs((b.top + b.bottom) / 2 - 70) <= c.th / 2 + 1);
    dt_clear(&c);
    DTA(c.dc, "XXXX", -1, &(RECT){20, 20, 220, 120}, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    CHECK(dt_bbox(&c, &b));
    CHECK(abs((b.left + b.right) / 2 - 120) <= c.cw);
    CHECK(abs((b.top + b.bottom) / 2 - 70) <= c.th / 2 + 1);
    dt_done(&c);
}
TEST(paint_gdi_dt_return_value_vcenter_bottom) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 200, 100};
    int h = DTA(c.dc, "X", -1, &r, DT_SINGLELINE | DT_VCENTER);
    CHECKF(abs(h - ((100 - c.th) / 2 + c.th)) <= 1, "vcenter returned %d", h);
    dt_clear(&c);
    r = (RECT){0, 0, 200, 100};
    h = DTA(c.dc, "X", -1, &r, DT_SINGLELINE | DT_BOTTOM);
    CHECKF(h == 100 || h == c.th + (100 - c.th), "bottom returned %d", h);
    r = (RECT){0, 50, 200, 150};
    h = DTA(c.dc, "X", -1, &r, DT_SINGLELINE | DT_BOTTOM);
    CHECKF(h == 100, "bottom with top offset returned %d", h);
    dt_done(&c);
}
TEST(paint_gdi_dt_clip_and_noclip) {
    REQUIRE(DTA); SETUP();
    RECT rc = {10, 10, 10 + 3 * c.cw, 10 + c.th}, b;
    DTA(c.dc, "XXXXXXXXXXXXXXXX", -1, &rc, DT_SINGLELINE);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.right <= 10 + 3 * c.cw);
    dt_clear(&c);
    rc = (RECT){10, 10, 10 + 3 * c.cw, 10 + c.th};
    DTA(c.dc, "XXXXXXXXXXXXXXXX", -1, &rc, DT_SINGLELINE | DT_NOCLIP);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.right > 10 + 3 * c.cw + c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_clip_vertical) {
    REQUIRE(DTA); SETUP();
    RECT rc = {10, 10, 200, 10 + c.th + c.th / 2}, b;
    DTA(c.dc, "XX\nXX\nXX\nXX", -1, &rc, DT_LEFT);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.bottom <= 10 + c.th + c.th / 2);
    dt_done(&c);
}
TEST(paint_gdi_dt_end_ellipsis_modify) {
    REQUIRE(DTA); SETUP();
    char buf[64]; strcpy(buf, "abcdefghijklmnopqrstuvwxyz");
    RECT r = {0, 0, 10 * c.cw, 100};
    int h = DTA(c.dc, buf, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_MODIFYSTRING);
    EQ(h, c.th);
    EXPECT_STR(buf, "abcdefg...");
    dt_done(&c);
}
TEST(paint_gdi_dt_end_ellipsis_no_modify) {
    REQUIRE(DTA); SETUP();
    char buf[64]; strcpy(buf, "abcdefghijklmnopqrstuvwxyz");
    RECT r = {0, 0, 10 * c.cw, 100};
    DTA(c.dc, buf, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS);
    EXPECT_STR(buf, "abcdefghijklmnopqrstuvwxyz");
    dt_done(&c);
}
TEST(paint_gdi_dt_end_ellipsis_fits) {
    REQUIRE(DTA); SETUP();
    char buf[64]; strcpy(buf, "abcdefghij");
    RECT r = {0, 0, 10 * c.cw, 100};
    DTA(c.dc, buf, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_MODIFYSTRING);
    EXPECT_STR(buf, "abcdefghij");
    strcpy(buf, "abcdefghijk");
    DTA(c.dc, buf, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_MODIFYSTRING);
    EXPECT_STR(buf, "abcdefg...");
    dt_done(&c);
}
TEST(paint_gdi_dt_end_ellipsis_wide) {
    REQUIRE(DTWF); SETUP();
    WCHAR buf[64]; MultiByteToWideChar(CP_ACP, 0, "abcdefghijklmnopqrstuvwxyz", -1, buf, 64);
    RECT r = {0, 0, 10 * c.cw, 100};
    DTWF(c.dc, buf, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_MODIFYSTRING);
    WCHAR want[] = L"abcdefg...";
    EXPECT_WSTR(buf, want);
    dt_done(&c);
}
TEST(paint_gdi_dt_ellipsis_tiny_width) {
    REQUIRE(DTA); SETUP();
    for (int cols = 0; cols <= 5; cols++) {
        char buf[64]; strcpy(buf, "abcdefghijklmnop");
        RECT r = {0, 0, cols * c.cw, 100};
        int h = DTA(c.dc, buf, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_MODIFYSTRING);
        CHECKF(strlen(buf) <= 16, "cols %d -> \"%s\" (h=%d)", cols, buf, h);
        t_note("end-ellipsis in %d columns -> \"%s\"", cols, buf);
    }
    dt_done(&c);
}
TEST(paint_gdi_dt_path_ellipsis) {
    REQUIRE(DTA); SETUP();
    char buf[128]; strcpy(buf, "C:\\dir\\subdir\\anotherdir\\file.txt");
    RECT r = {0, 0, 20 * c.cw, 100};
    DTA(c.dc, buf, -1, &r, DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_MODIFYSTRING);
    t_note("path ellipsis -> \"%s\"", buf);
    SIZE sz; GetTextExtentPoint32A(c.dc, buf, (int)strlen(buf), &sz);
    CHECK(sz.cx <= 20 * c.cw);
    CHECK(strstr(buf, "file.txt") != NULL);
    CHECK(strstr(buf, "...") != NULL);
    dt_done(&c);
}
TEST(paint_gdi_dt_word_ellipsis) {
    REQUIRE(DTA); SETUP();
    char buf[128]; strcpy(buf, "alpha beta gamma delta epsilon");
    RECT r = {0, 0, 12 * c.cw, 100};
    DTA(c.dc, buf, -1, &r, DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_MODIFYSTRING);
    t_note("word ellipsis -> \"%s\"", buf);
    SIZE sz; GetTextExtentPoint32A(c.dc, buf, (int)strlen(buf), &sz);
    CHECK(sz.cx <= 12 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_ellipsis_multiline_wordbreak) {
    REQUIRE(DTA); SETUP();
    char buf[128]; strcpy(buf, "aaa bbb ccc ddd eee fff ggg");
    RECT r = {0, 0, 8 * c.cw, 2 * c.th};
    int h = DTA(c.dc, buf, -1, &r, DT_WORDBREAK | DT_END_ELLIPSIS | DT_MODIFYSTRING);
    t_note("wordbreak+ellipsis h=%d text=\"%s\"", h, buf);
    CHECK(h > 0);
    dt_done(&c);
}
TEST(paint_gdi_dt_empty_and_zero_count) {
    REQUIRE(DTA); SETUP();
    RECT r = {5, 5, 100, 100};
    int h = DTA(c.dc, "", -1, &r, DT_SINGLELINE | DT_CALCRECT);
    t_note("DrawText(\"\") CALCRECT -> %d rect (%ld,%ld,%ld,%ld)", h, RECT_STR(r));
    r = (RECT){5, 5, 100, 100};
    h = DTA(c.dc, "abc", 0, &r, DT_SINGLELINE | DT_CALCRECT);
    t_note("DrawText(count 0) CALCRECT -> %d rect (%ld,%ld,%ld,%ld)", h, RECT_STR(r));
    dt_done(&c);
}
TEST(paint_gdi_dt_invalid_dc) {
    REQUIRE(DTA);
    RECT r = {0, 0, 100, 100};
    SetLastError(0);
    int h = DTA(NULL, "abc", -1, &r, DT_CALCRECT);
    CHECK(h == 0);
    t_note("DrawText(NULL dc) err=%lu", GetLastError());
    /* Win10 22H2 oracle: only NULL is rejected; a bogus or wrong-type handle still returns a
     * (non-zero) height for DT_CALCRECT. Observed, not asserted. */
    h = DTA((HDC)(ULONG_PTR)0x4444, "abc", -1, &r, DT_CALCRECT);
    t_note("DrawText(bogus dc) CALCRECT = %d", h);
    HBRUSH br = CreateSolidBrush(0);
    h = DTA((HDC)br, "abc", -1, &r, DT_CALCRECT);
    t_note("DrawText(brush as dc) CALCRECT = %d", h);
    DeleteObject(br);
}
TEST(paint_gdi_dt_wide_unicode_text) {
    REQUIRE(DTWF); SETUP();
    WCHAR s[] = { 0x0041, 0x00e9, 0x00fc, 0 };    /* A e-acute u-umlaut */
    RECT r = {0, 0, 400, 100};
    int h = DTWF(c.dc, s, -1, &r, DT_SINGLELINE | DT_CALCRECT);
    EQ(h, c.th);
    SIZE sz; GetTextExtentPoint32W(c.dc, s, 3, &sz);
    /* Win10 22H2 oracle: DrawText's computed width for accented Latin differs from the plain
     * GetTextExtentPoint32W sum; the difference is recorded and bounded to one character cell. */
    t_note("DrawTextW CALCRECT width %ld vs GetTextExtentPoint32W %ld", r.right - r.left, sz.cx);
    CHECK(labs((r.right - r.left) - sz.cx) <= c.cw);
    WCHAR m[] = { 'a', 0x20ac, 'b', 0 };
    r = (RECT){0, 0, 400, 100};
    h = DTWF(c.dc, m, -1, &r, DT_SINGLELINE | DT_CALCRECT);
    CHECK(h > 0);
    GetTextExtentPoint32W(c.dc, m, 3, &sz);
    /* Same Win10 22H2 behaviour as above for a glyph outside the base font (font fallback): bounded to one cell. */
    t_note("DrawTextW CALCRECT euro width %ld vs GetTextExtentPoint32W %ld", r.right - r.left, sz.cx);
    CHECK(labs((r.right - r.left) - sz.cx) <= c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_wide_count_vs_nul) {
    REQUIRE(DTWF); SETUP();
    WCHAR s[] = L"abcdef";
    RECT r = {0, 0, 400, 100};
    DTWF(c.dc, s, 4, &r, DT_SINGLELINE | DT_CALCRECT);
    CHECK(r.right == 4 * c.cw);
    r = (RECT){0, 0, 400, 100};
    DTWF(c.dc, s, -1, &r, DT_SINGLELINE | DT_CALCRECT);
    CHECK(r.right == 6 * c.cw);
    WCHAR emb[] = { 'a', 'b', 0, 'c', 'd', 0 };
    r = (RECT){0, 0, 400, 100};
    DTWF(c.dc, emb, 5, &r, DT_SINGLELINE | DT_CALCRECT);
    t_note("embedded NUL, count 5 -> width %ld (cw %d)", r.right, c.cw);
    r = (RECT){0, 0, 400, 100};
    DTWF(c.dc, emb, -1, &r, DT_SINGLELINE | DT_CALCRECT);
    CHECK(r.right == 2 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dt_calcrect_after_origin) {
    REQUIRE(DTA); SETUP();
    RECT r = {-50, -60, 40, 30};
    calc_a(&c, "ab", -1, &r, DT_SINGLELINE);
    CHECK_RECT(r, -50, -60, -50 + 2 * c.cw, -60 + c.th);
    r = (RECT){100, 100, 100, 100};   /* empty rect with WORDBREAK */
    int h = calc_a(&c, "abc", -1, &r, DT_SINGLELINE);
    CHECK(h == c.th);
    dt_done(&c);
}
TEST(paint_gdi_dt_calcrect_alignment_flags) {
    REQUIRE(DTA); SETUP();
    RECT r = {0, 0, 200, 100};
    calc_a(&c, "abcd", -1, &r, DT_SINGLELINE | DT_RIGHT);
    t_note("CALCRECT|DT_RIGHT -> (%ld,%ld,%ld,%ld)", RECT_STR(r));
    CHECK(r.right - r.left == 4 * c.cw);
    r = (RECT){0, 0, 200, 100};
    calc_a(&c, "abcd", -1, &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    t_note("CALCRECT|CENTER|VCENTER -> (%ld,%ld,%ld,%ld)", RECT_STR(r));
    t_note("CENTER|VCENTER CALCRECT size %ldx%ld", r.right - r.left, r.bottom - r.top);
    dt_done(&c);
}
TEST(paint_gdi_dt_bkmode_opaque_fills) {
    REQUIRE(DTA); SETUP();
    SetBkMode(c.dc, OPAQUE);
    SetBkColor(c.dc, RGB(255, 0, 0));
    RECT r = {10, 10, 200, 100}, b;
    DTA(c.dc, "XX", -1, &r, DT_SINGLELINE | DT_LEFT | DT_TOP);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.left == 10 && b.top == 10);
    CHECK(b.right >= 10 + 2 * c.cw - 1);
    CHECK(b.bottom >= 10 + c.th - 1);
    GdiFlush();
    CHECK((c.bits[12 * DTW + 11] & 0xffffff) != 0xffffff);    /* background painted under glyph cell */
    dt_done(&c);
}
TEST(paint_gdi_dt_textcolor_applied) {
    REQUIRE(DTA); SETUP();
    SetTextColor(c.dc, RGB(0, 0, 255));
    RECT r = {10, 10, 200, 100};
    DTA(c.dc, "XXXX", -1, &r, DT_SINGLELINE);
    GdiFlush();
    int blue = 0, other = 0;
    for (int i = 0; i < DTW * DTH; i++) {
        DWORD p = c.bits[i] & 0xffffff;
        if (p == 0xffffff) continue;
        if (p == 0xff) blue++; else other++;
    }
    CHECK(blue > 0);
    t_note("text colour pixels exact=%d other(AA)=%d", blue, other);
    dt_done(&c);
}

/* ---------------- DrawTextEx ---------------- */
TEST(paint_gdi_dtex_null_params) {
    REQUIRE(U(DrawTextExA)); SETUP();
    RECT r = {0, 0, 400, 100};
    int h = U(DrawTextExA)(c.dc, (LPSTR)"hello", -1, &r, DT_SINGLELINE | DT_CALCRECT, NULL);
    EQ(h, c.th);
    CHECK(r.right == 5 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dtex_lengthdrawn) {
    REQUIRE(U(DrawTextExA)); SETUP();
    DRAWTEXTPARAMS p; memset(&p, 0, sizeof p); p.cbSize = sizeof p; p.uiLengthDrawn = 0xdead;
    RECT r = {0, 0, 400, 100};
    int h = U(DrawTextExA)(c.dc, (LPSTR)"hello", -1, &r, DT_SINGLELINE, &p);
    EQ(h, c.th);
    EQ(p.uiLengthDrawn, 5);
    p.uiLengthDrawn = 0xdead;
    r = (RECT){0, 0, 400, 100};
    U(DrawTextExA)(c.dc, (LPSTR)"hello world", 5, &r, DT_SINGLELINE, &p);
    EQ(p.uiLengthDrawn, 5);
    dt_done(&c);
}
TEST(paint_gdi_dtex_lengthdrawn_clipped_lines) {
    REQUIRE(U(DrawTextExA)); SETUP();
    DRAWTEXTPARAMS p; memset(&p, 0, sizeof p); p.cbSize = sizeof p;
    RECT r = {0, 0, 100, c.th};     /* room for exactly one line */
    int h = U(DrawTextExA)(c.dc, (LPSTR)"one\ntwo\nthree", -1, &r, DT_LEFT, &p);
    t_note("DrawTextEx clipped lines: h=%d lengthDrawn=%u", h, p.uiLengthDrawn);
    CHECK(p.uiLengthDrawn >= 3 && p.uiLengthDrawn <= 13);
    dt_clear(&c);
    r = (RECT){0, 0, 100, c.th};
    U(DrawTextExA)(c.dc, (LPSTR)"one\ntwo\nthree", -1, &r, DT_LEFT | DT_NOCLIP, &p);
    EQ(p.uiLengthDrawn, 13);      /* always the whole string with DT_NOCLIP */
    dt_done(&c);
}
TEST(paint_gdi_dtex_margins) {
    REQUIRE(U(DrawTextExA)); SETUP();
    DRAWTEXTPARAMS p; memset(&p, 0, sizeof p); p.cbSize = sizeof p; p.iLeftMargin = 3; p.iRightMargin = 2;
    RECT r = {10, 10, 300, 100}, b;
    U(DrawTextExA)(c.dc, (LPSTR)"XX", -1, &r, DT_SINGLELINE | DT_LEFT | DT_TOP, &p);
    CHECK(dt_bbox(&c, &b));
    /* Win10 22H2 oracle: the margins behave as pixels (CALCRECT widens by exactly left+right = 5,
     * not by 5 average character widths as documented). */
    CHECK(b.left >= 10 + 3);
    CHECK(b.left <= 10 + 3 + c.cw);
    dt_clear(&c);
    r = (RECT){10, 10, 300, 100};
    U(DrawTextExA)(c.dc, (LPSTR)"XX", -1, &r, DT_SINGLELINE | DT_RIGHT | DT_TOP, &p);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.right <= 300 - 2);
    dt_done(&c);
}
TEST(paint_gdi_dtex_margins_calcrect) {
    REQUIRE(U(DrawTextExA)); SETUP();
    DRAWTEXTPARAMS p; memset(&p, 0, sizeof p); p.cbSize = sizeof p; p.iLeftMargin = 3; p.iRightMargin = 2;
    RECT r = {0, 0, 300, 100};
    U(DrawTextExA)(c.dc, (LPSTR)"XX", -1, &r, DT_SINGLELINE | DT_CALCRECT, &p);
    t_note("DrawTextEx CALCRECT with margins -> width %ld (text %d, margins %d)", r.right, 2 * c.cw, 5 * c.cw);
    CHECK(r.right >= 2 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_dtex_tablength) {
    REQUIRE(U(DrawTextExA)); SETUP();
    DRAWTEXTPARAMS p; memset(&p, 0, sizeof p); p.cbSize = sizeof p; p.iTabLength = 4;
    RECT r = {0, 0, 400, 100}, b;
    U(DrawTextExA)(c.dc, (LPSTR)"a\tX", -1, &r, DT_SINGLELINE | DT_EXPANDTABS | DT_TABSTOP, &p);
    CHECK(dt_bbox(&c, &b));
    CHECK(b.right > 4 * c.cw && b.right <= 5 * c.cw + 1);
    dt_done(&c);
}
TEST(paint_gdi_dtex_bad_cbsize) {
    REQUIRE(U(DrawTextExA)); SETUP();
    DRAWTEXTPARAMS p; memset(&p, 0, sizeof p);
    RECT r = {0, 0, 400, 100};
    static const UINT sizes[] = { 0, 1, 4, 19, 21, 100 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        p.cbSize = sizes[i];
        SetLastError(0);
        int h = U(DrawTextExA)(c.dc, (LPSTR)"abc", -1, &r, DT_SINGLELINE | DT_CALCRECT, &p);
        t_note("DrawTextEx cbSize=%u -> %d err=%lu", sizes[i], h, GetLastError());
    }
    dt_done(&c);
}
TEST(paint_gdi_dtex_wide_matches_ansi) {
    REQUIRE(U(DrawTextExA)); REQUIRE(U(DrawTextExW)); SETUP();
    DRAWTEXTPARAMS p; memset(&p, 0, sizeof p); p.cbSize = sizeof p; p.iLeftMargin = 1;
    RECT a = {0, 0, 80, 100}, b = a;
    int ha = U(DrawTextExA)(c.dc, (LPSTR)"some words to wrap around", -1, &a, DT_WORDBREAK | DT_CALCRECT, &p);
    int hb = U(DrawTextExW)(c.dc, L"some words to wrap around", -1, &b, DT_WORDBREAK | DT_CALCRECT, &p);
    CHECK(ha == hb);
    CHECK(a.right == b.right && a.bottom == b.bottom);
    dt_done(&c);
}
TEST(paint_gdi_dtex_modify_string_ellipsis) {
    REQUIRE(U(DrawTextExW)); SETUP();
    WCHAR buf[64]; MultiByteToWideChar(CP_ACP, 0, "abcdefghijklmnopqrstuvwxyz", -1, buf, 64);
    RECT r = {0, 0, 10 * c.cw, 100};
    U(DrawTextExW)(c.dc, buf, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_MODIFYSTRING, NULL);
    CHECK(buf[7] == '.' && buf[8] == '.' && buf[9] == '.' && buf[10] == 0);
    dt_done(&c);
}
TEST(paint_gdi_dtex_invalid_dc) {
    REQUIRE(U(DrawTextExA));
    RECT r = {0, 0, 100, 100};
    SetLastError(0);
    CHECK(U(DrawTextExA)(NULL, (LPSTR)"abc", -1, &r, DT_CALCRECT, NULL) == 0);
    /* Win10 22H2 oracle: a bogus (non-NULL) dc is not rejected; observed only */
    t_note("DrawTextEx(bogus dc) CALCRECT = %d", U(DrawTextExA)((HDC)(ULONG_PTR)0x4444, (LPSTR)"abc", -1, &r, DT_CALCRECT, NULL));
}

/* ---------------- Tabbed text ---------------- */
#define TW(v) ((int)LOWORD(v))
#define TH(v) ((int)HIWORD(v))
TEST(paint_gdi_tabbed_extent_default_tabs) {
    REQUIRE(U(GetTabbedTextExtentA)); SETUP();
    LONG v = U(GetTabbedTextExtentA)(c.dc, "abc", 3, 0, NULL);
    EQ(TW(v), 3 * c.cw); EQ(TH(v), c.th);
    v = U(GetTabbedTextExtentA)(c.dc, "a\tb", 3, 0, NULL);
    EQ(TW(v), 9 * c.cw); EQ(TH(v), c.th);
    v = U(GetTabbedTextExtentA)(c.dc, "\t", 1, 0, NULL);
    EQ(TW(v), 8 * c.cw);
    v = U(GetTabbedTextExtentA)(c.dc, "abcdefgh\ti", 10, 0, NULL);
    EQ(TW(v), 17 * c.cw);
    v = U(GetTabbedTextExtentA)(c.dc, "a\t\tb", 4, 0, NULL);
    EQ(TW(v), 17 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_tabbed_extent_single_stop) {
    REQUIRE(U(GetTabbedTextExtentA)); SETUP();
    INT tab = 50;
    LONG v = U(GetTabbedTextExtentA)(c.dc, "a\tb", 3, 1, &tab);    /* single value: fixed interval */
    EQ(TW(v), 50 + c.cw);
    v = U(GetTabbedTextExtentA)(c.dc, "a\tb\tc", 5, 1, &tab);
    EQ(TW(v), 100 + c.cw);
    dt_done(&c);
}
TEST(paint_gdi_tabbed_extent_stop_list) {
    REQUIRE(U(GetTabbedTextExtentA)); SETUP();
    INT tabs[] = { 20, 60 };
    LONG v = U(GetTabbedTextExtentA)(c.dc, "a\tb\tc", 5, 2, tabs);
    EQ(TW(v), 60 + c.cw);
    v = U(GetTabbedTextExtentA)(c.dc, "a\tb", 3, 2, tabs);
    EQ(TW(v), 20 + c.cw);
    v = U(GetTabbedTextExtentA)(c.dc, "\tb", 2, 2, tabs);
    EQ(TW(v), 20 + c.cw);
    dt_done(&c);
}
TEST(paint_gdi_tabbed_extent_wide_parity) {
    REQUIRE(U(GetTabbedTextExtentA)); REQUIRE(U(GetTabbedTextExtentW)); SETUP();
    INT tabs[] = { 30, 70, 120 };
    static const char *s[] = { "a\tb", "abc\tdefg\thi", "\t\t", "x" };
    for (unsigned i = 0; i < 4; i++) {
        WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, s[i], -1, w, 64);
        int n = (int)strlen(s[i]);
        LONG a = U(GetTabbedTextExtentA)(c.dc, s[i], n, 3, tabs), b = U(GetTabbedTextExtentW)(c.dc, w, n, 3, tabs);
        CHECKF(a == b, "case %u: A 0x%lx W 0x%lx", i, a, b);
        a = U(GetTabbedTextExtentA)(c.dc, s[i], n, 0, NULL); b = U(GetTabbedTextExtentW)(c.dc, w, n, 0, NULL);
        CHECKF(a == b, "case %u default: A 0x%lx W 0x%lx", i, a, b);
    }
    dt_done(&c);
}
TEST(paint_gdi_tabbed_out_matches_extent) {
    REQUIRE(U(TabbedTextOutA)); REQUIRE(U(GetTabbedTextExtentA)); SETUP();
    INT tabs[] = { 20, 60 };
    LONG e = U(GetTabbedTextExtentA)(c.dc, "ab\tcd\tef", 8, 2, tabs);
    /* tab stops are measured from nTabOrigin; use the start x so the layout equals the extent's */
    LONG o = U(TabbedTextOutA)(c.dc, 10, 10, "ab\tcd\tef", 8, 2, tabs, 10);
    EQ(o, e);
    RECT b; CHECK(dt_bbox(&c, &b));
    CHECK(b.left >= 10 && b.top >= 10);
    CHECK(b.right <= 10 + TW(e) + 1);
    CHECK(b.bottom <= 10 + TH(e) + 1);
    dt_done(&c);
}
TEST(paint_gdi_tabbed_out_origin) {
    REQUIRE(U(TabbedTextOutA)); REQUIRE(U(GetTabbedTextExtentA)); SETUP();
    INT tabs[] = { 40 };
    LONG o0 = U(TabbedTextOutA)(c.dc, 0, 0, "a\tb", 3, 1, tabs, 0);
    dt_clear(&c);
    LONG o1 = U(TabbedTextOutA)(c.dc, 100, 0, "a\tb", 3, 1, tabs, 100);   /* origin = start x: same layout */
    EQ(TW(o0), 40 + c.cw);
    EQ(TW(o1), 40 + c.cw);
    RECT b; CHECK(dt_bbox(&c, &b));
    CHECK(b.left >= 100);
    dt_clear(&c);
    U(TabbedTextOutA)(c.dc, 100, 0, "a\tX", 3, 1, tabs, 0);   /* origin 0: stop at x=40 is already behind the text */
    CHECK(dt_bbox(&c, &b));
    t_note("TabbedTextOut origin 0, start 100 -> bbox right %ld", b.right);
    dt_done(&c);
}
TEST(paint_gdi_tabbed_out_wide) {
    REQUIRE(U(TabbedTextOutW)); SETUP();
    INT tabs[] = { 25 };
    LONG o = U(TabbedTextOutW)(c.dc, 5, 5, L"a\tb", 3, 1, tabs, 5);   /* tab origin = start x */
    EQ(TW(o), 25 + c.cw);
    EQ(TH(o), c.th);
    dt_done(&c);
}
TEST(paint_gdi_tabbed_zero_length) {
    REQUIRE(U(GetTabbedTextExtentA)); REQUIRE(U(TabbedTextOutA)); SETUP();
    LONG v = U(GetTabbedTextExtentA)(c.dc, "abc", 0, 0, NULL);
    t_note("GetTabbedTextExtent(len 0) = 0x%lx", v);
    CHECK(TW(v) == 0);
    v = U(TabbedTextOutA)(c.dc, 0, 0, "abc", 0, 0, NULL, 0);
    CHECK(TW(v) == 0);
    RECT b; CHECK(!dt_bbox(&c, &b));
    dt_done(&c);
}
TEST(paint_gdi_tabbed_invalid_dc) {
    REQUIRE(U(GetTabbedTextExtentA)); REQUIRE(U(TabbedTextOutA));
    SetLastError(0);
    CHECK(U(GetTabbedTextExtentA)(NULL, "abc", 3, 0, NULL) == 0);
    CHECK(U(TabbedTextOutA)(NULL, 0, 0, "abc", 3, 0, NULL, 0) == 0);
    CHECK(U(GetTabbedTextExtentA)((HDC)(ULONG_PTR)0x777, "abc", 3, 0, NULL) == 0);
}
TEST(paint_gdi_tabbed_negative_stops) {
    REQUIRE(U(GetTabbedTextExtentA)); SETUP();
    INT tabs[] = { -60 };    /* negative: right-aligned tab stop */
    LONG v = U(GetTabbedTextExtentA)(c.dc, "a\tb", 3, 1, tabs);
    t_note("negative tab stop -> width %d (cw %d)", TW(v), c.cw);
    CHECK(TW(v) >= 2 * c.cw);
    dt_done(&c);
}
TEST(paint_gdi_tabbed_long_string) {
    REQUIRE(U(GetTabbedTextExtentA)); SETUP();
    char buf[2001]; memset(buf, 'a', 2000); buf[2000] = 0;
    LONG v = U(GetTabbedTextExtentA)(c.dc, buf, 2000, 0, NULL);
    t_note("2000 chars extent 0x%lx", v);
    CHECK(TH(v) == c.th);
    dt_done(&c);
}
TEST(paint_gdi_tabbed_newline_is_glyph) {
    REQUIRE(U(GetTabbedTextExtentA)); SETUP();
    LONG v = U(GetTabbedTextExtentA)(c.dc, "a\nb", 3, 0, NULL);
    CHECK(TH(v) == c.th);     /* no line breaking in tabbed text */
    dt_done(&c);
}
