/* gap_00: DrawFrame, DrawCaptionTempA/W, DrawMenuBarTemp */
#include "gap0.h"

UDECL(BOOL, DrawFrame, (HDC, RECT *, int, UINT));
UDECL(BOOL, DrawCaptionTempA, (HWND, HDC, const RECT *, HFONT, HICON, LPCSTR, UINT));
UDECL(BOOL, DrawCaptionTempW, (HWND, HDC, const RECT *, HFONT, HICON, LPCWSTR, UINT));
UDECL(int, DrawMenuBarTemp, (HWND, HDC, RECT *, HMENU, HFONT));

#define MAG 0x00FF00FFu
#define DF(c) ((UINT)(c) << 3)

/* ---------- DrawFrame ---------- */
static void ring_check(G0Surf *s, int x0, int y0, int x1, int y1, int w, DWORD want, DWORD interior, const char *tag) {
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        int in_ring = x < x0 + w || x >= x1 - w || y < y0 + w || y >= y1 - w;
        DWORD p = g0_px(s, x, y), e = in_ring ? want : interior;
        if (p != e) { CHECKF(0, "%s: pixel %d,%d = %06lx want %06lx", tag, x, y, p, e); return; }
    }
    CHECK(1);
}
TEST(gap0_drawframe_window_frame_width1) {
    REQUIRE(UN(DrawFrame)); REQUIRE(U(GetSysColor));
    G0Surf s; REQUIRE(g0_surf_make(&s, 40, 40, MAG));
    RECT rc = { 5, 5, 25, 25 };
    BOOL r = UN(DrawFrame)(s.dc, &rc, 1, DF(COLOR_WINDOWFRAME));
    EXPECT_EQ(r, 1);
    ring_check(&s, 5, 5, 25, 25, 1, g0_rgb(U(GetSysColor)(COLOR_WINDOWFRAME)), MAG, "w1");
    CHECK(g0_px(&s, 4, 4) == MAG && g0_px(&s, 25, 25) == MAG && g0_px(&s, 4, 15) == MAG && g0_px(&s, 25, 15) == MAG);
    g0_surf_free(&s);
}
TEST(gap0_drawframe_width_2_3) {
    REQUIRE(UN(DrawFrame)); REQUIRE(U(GetSysColor));
    for (int w = 2; w <= 3; w++) {
        G0Surf s; REQUIRE(g0_surf_make(&s, 40, 40, MAG));
        RECT rc = { 3, 4, 33, 30 };
        EXPECT_EQ(UN(DrawFrame)(s.dc, &rc, w, DF(COLOR_HIGHLIGHT)), 1);
        ring_check(&s, 3, 4, 33, 30, w, g0_rgb(U(GetSysColor)(COLOR_HIGHLIGHT)), MAG, "wN");
        g0_surf_free(&s);
    }
}
TEST(gap0_drawframe_many_colors) {
    REQUIRE(UN(DrawFrame)); REQUIRE(U(GetSysColor));
    static const int c[] = { COLOR_BACKGROUND, COLOR_ACTIVECAPTION, COLOR_MENU, COLOR_WINDOW, COLOR_WINDOWTEXT, COLOR_BTNFACE, COLOR_BTNSHADOW, COLOR_BTNTEXT, COLOR_HIGHLIGHT, COLOR_BTNHIGHLIGHT };
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        G0Surf s; REQUIRE(g0_surf_make(&s, 20, 20, MAG));
        RECT rc = { 0, 0, 20, 20 };
        UN(DrawFrame)(s.dc, &rc, 1, DF(c[i]));
        DWORD want = g0_rgb(U(GetSysColor)(c[i]));
        CHECKF(g0_px(&s, 0, 0) == want && g0_px(&s, 19, 0) == want && g0_px(&s, 0, 19) == want && g0_px(&s, 19, 19) == want && g0_px(&s, 10, 0) == want && g0_px(&s, 0, 10) == want,
               "color index %d corner %06lx want %06lx", c[i], g0_px(&s, 0, 0), want);
        CHECKF(g0_px(&s, 10, 10) == MAG, "color %d interior", c[i]);
        g0_surf_free(&s);
    }
}
TEST(gap0_drawframe_invert_flag) {
    REQUIRE(UN(DrawFrame)); REQUIRE(U(GetSysColor));
    G0Surf s; REQUIRE(g0_surf_make(&s, 20, 20, MAG));
    RECT rc = { 0, 0, 20, 20 };
    UN(DrawFrame)(s.dc, &rc, 1, DF(COLOR_WINDOWFRAME) | 4);
    DWORD want = (MAG ^ g0_rgb(U(GetSysColor)(COLOR_WINDOWFRAME))) & 0xFFFFFF;
    CHECKF(g0_px(&s, 0, 0) == want, "invert corner %06lx want %06lx", g0_px(&s, 0, 0), want);
    CHECKF(g0_px(&s, 10, 10) == MAG, "interior");
    /* applying it twice restores the surface */
    UN(DrawFrame)(s.dc, &rc, 1, DF(COLOR_WINDOWFRAME) | 4);
    CHECK(g0_px(&s, 0, 0) == MAG && g0_px(&s, 19, 10) == MAG && g0_px(&s, 10, 19) == MAG);
    g0_surf_free(&s);
}
TEST(gap0_drawframe_low_bits_ignored) {
    REQUIRE(UN(DrawFrame));
    /* bits 0-1 are not used by the implementation */
    G0Surf a, b; REQUIRE(g0_surf_make(&a, 20, 20, MAG)); REQUIRE(g0_surf_make(&b, 20, 20, MAG));
    RECT rc = { 1, 1, 18, 18 };
    UN(DrawFrame)(a.dc, &rc, 2, DF(COLOR_BTNSHADOW));
    UN(DrawFrame)(b.dc, &rc, 2, DF(COLOR_BTNSHADOW) | 3);
    GdiFlush();
    CHECK(!memcmp(a.bits, b.bits, 20 * 20 * 4));
    g0_surf_free(&a); g0_surf_free(&b);
}
TEST(gap0_drawframe_returns_one_always) {
    REQUIRE(UN(DrawFrame));
    G0Surf s; REQUIRE(g0_surf_make(&s, 20, 20, MAG));
    RECT rc = { 0, 0, 20, 20 };
    EXPECT_EQ(UN(DrawFrame)(s.dc, &rc, 0, 0), 1);
    EXPECT_EQ(UN(DrawFrame)(s.dc, &rc, 1, 0), 1);
    EXPECT_EQ(UN(DrawFrame)(NULL, &rc, 1, DF(COLOR_WINDOW)), 1);
    EXPECT_EQ(UN(DrawFrame)((HDC)(ULONG_PTR)0x1234, &rc, 1, DF(COLOR_WINDOW)), 1);
    g0_surf_free(&s);
}
TEST(gap0_drawframe_width_zero_changes_nothing) {
    REQUIRE(UN(DrawFrame));
    G0Surf s; REQUIRE(g0_surf_make(&s, 20, 20, MAG));
    RECT rc = { 2, 2, 18, 18 };
    UN(DrawFrame)(s.dc, &rc, 0, DF(COLOR_WINDOWFRAME));
    int diff = 0;
    for (int i = 0; i < 400; i++) if ((s.bits[i] & 0xFFFFFF) != MAG) diff++;
    EXPECT_EQ(diff, 0);
    g0_surf_free(&s);
}
TEST(gap0_drawframe_does_not_touch_outside) {
    REQUIRE(UN(DrawFrame));
    G0Surf s; REQUIRE(g0_surf_make(&s, 30, 30, MAG));
    RECT rc = { 10, 10, 20, 20 };
    UN(DrawFrame)(s.dc, &rc, 2, DF(COLOR_WINDOWTEXT));
    int outside = 0;
    for (int y = 0; y < 30; y++) for (int x = 0; x < 30; x++)
        if ((x < 10 || x >= 20 || y < 10 || y >= 20) && g0_px(&s, x, y) != MAG) outside++;
    EXPECT_EQ(outside, 0);
    g0_surf_free(&s);
}
TEST(gap0_drawframe_rect_unmodified) {
    REQUIRE(UN(DrawFrame));
    G0Surf s; REQUIRE(g0_surf_make(&s, 30, 30, MAG));
    RECT rc = { 3, 4, 25, 26 };
    UN(DrawFrame)(s.dc, &rc, 2, DF(COLOR_WINDOW));
    CHECK(rc.left == 3 && rc.top == 4 && rc.right == 25 && rc.bottom == 26);
    g0_surf_free(&s);
}
TEST(gap0_drawframe_degenerate_rect) {
    REQUIRE(UN(DrawFrame));
    G0Surf s; REQUIRE(g0_surf_make(&s, 30, 30, MAG));
    RECT rc = { 5, 5, 5, 5 };
    EXPECT_EQ(UN(DrawFrame)(s.dc, &rc, 1, DF(COLOR_WINDOW)), 1);
    RECT inv = { 20, 20, 10, 10 };
    EXPECT_EQ(UN(DrawFrame)(s.dc, &inv, 1, DF(COLOR_WINDOW)), 1);
    g0_surf_free(&s);
}

/* ---------- DrawCaptionTemp ---------- */
static int surf_diff(G0Surf *s, int x0, int y0, int x1, int y1, DWORD base) {
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (g0_px(s, x, y) != base) n++;
    return n;
}
TEST(gap0_drawcaption_w_active_text) {
    REQUIRE(UN(DrawCaptionTempW));
    G0Surf s; REQUIRE(g0_surf_make(&s, 200, 60, MAG));
    RECT rc = { 0, 0, 180, 24 };
    BOOL r = UN(DrawCaptionTempW)(NULL, s.dc, &rc, NULL, NULL, L"Caption", DC_ACTIVE | DC_TEXT);
    CHECK(r);
    int inside = surf_diff(&s, 0, 0, 180, 24, MAG), outside = surf_diff(&s, 0, 24, 200, 60, MAG) + surf_diff(&s, 180, 0, 200, 24, MAG);
    CHECKF(inside > 180 * 24 / 2, "only %d pixels changed", inside);
    EXPECT_EQ(outside, 0);
    DWORD p = g0_px(&s, 170, 20);
    if (p != g0_rgb(U(GetSysColor)(COLOR_ACTIVECAPTION))) t_note("caption pixel %06lx vs COLOR_ACTIVECAPTION %06lx (gradient/theme?)", p, g0_rgb(U(GetSysColor)(COLOR_ACTIVECAPTION)));
    g0_surf_free(&s);
}
TEST(gap0_drawcaption_a_w_identical) {
    REQUIRE(UN(DrawCaptionTempA)); REQUIRE(UN(DrawCaptionTempW));
    G0Surf a, b; REQUIRE(g0_surf_make(&a, 200, 40, MAG)); REQUIRE(g0_surf_make(&b, 200, 40, MAG));
    RECT rc = { 0, 0, 190, 30 };
    CHECK(UN(DrawCaptionTempA)(NULL, a.dc, &rc, NULL, NULL, "Hello caption", DC_ACTIVE | DC_TEXT));
    CHECK(UN(DrawCaptionTempW)(NULL, b.dc, &rc, NULL, NULL, L"Hello caption", DC_ACTIVE | DC_TEXT));
    GdiFlush();
    CHECK(!memcmp(a.bits, b.bits, 200 * 40 * 4));
    g0_surf_free(&a); g0_surf_free(&b);
}
TEST(gap0_drawcaption_text_changes_pixels) {
    REQUIRE(UN(DrawCaptionTempW));
    G0Surf a, b; REQUIRE(g0_surf_make(&a, 200, 40, MAG)); REQUIRE(g0_surf_make(&b, 200, 40, MAG));
    RECT rc = { 0, 0, 190, 30 };
    CHECK(UN(DrawCaptionTempW)(NULL, a.dc, &rc, NULL, NULL, L"Some text here", DC_ACTIVE | DC_TEXT));
    CHECK(UN(DrawCaptionTempW)(NULL, b.dc, &rc, NULL, NULL, NULL, DC_ACTIVE));
    GdiFlush();
    CHECK(memcmp(a.bits, b.bits, 200 * 40 * 4) != 0);
    g0_surf_free(&a); g0_surf_free(&b);
}
TEST(gap0_drawcaption_no_text_flag_ignores_text) {
    REQUIRE(UN(DrawCaptionTempW));
    G0Surf a, b; REQUIRE(g0_surf_make(&a, 200, 40, MAG)); REQUIRE(g0_surf_make(&b, 200, 40, MAG));
    RECT rc = { 0, 0, 190, 30 };
    UN(DrawCaptionTempW)(NULL, a.dc, &rc, NULL, NULL, L"Some text here", DC_ACTIVE);
    UN(DrawCaptionTempW)(NULL, b.dc, &rc, NULL, NULL, NULL, DC_ACTIVE);
    GdiFlush();
    CHECK(!memcmp(a.bits, b.bits, 200 * 40 * 4));
    g0_surf_free(&a); g0_surf_free(&b);
}
TEST(gap0_drawcaption_active_vs_inactive) {
    REQUIRE(UN(DrawCaptionTempW)); REQUIRE(U(GetSysColor));
    G0Surf a, b; REQUIRE(g0_surf_make(&a, 100, 30, MAG)); REQUIRE(g0_surf_make(&b, 100, 30, MAG));
    RECT rc = { 0, 0, 100, 30 };
    UN(DrawCaptionTempW)(NULL, a.dc, &rc, NULL, NULL, NULL, DC_ACTIVE);
    UN(DrawCaptionTempW)(NULL, b.dc, &rc, NULL, NULL, NULL, 0);
    GdiFlush();
    if (U(GetSysColor)(COLOR_ACTIVECAPTION) != U(GetSysColor)(COLOR_INACTIVECAPTION)) CHECK(memcmp(a.bits, b.bits, 100 * 30 * 4) != 0);
    g0_surf_free(&a); g0_surf_free(&b);
}
TEST(gap0_drawcaption_with_real_window) {
    REQUIRE(UN(DrawCaptionTempW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 0, 200, 100, NULL);
    REQUIRE(w);
    G0Surf s; REQUIRE(g0_surf_make(&s, 200, 40, MAG));
    RECT rc = { 0, 0, 200, 30 };
    CHECK(UN(DrawCaptionTempW)(w, s.dc, &rc, NULL, NULL, L"win", DC_ACTIVE | DC_TEXT));
    CHECK(surf_diff(&s, 0, 0, 200, 30, MAG) > 1000);
    g0_surf_free(&s);
    U(DestroyWindow)(w);
}
TEST(gap0_drawcaption_custom_font) {
    REQUIRE(UN(DrawCaptionTempW));
    HFONT f = CreateFontW(-16, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
    REQUIRE(f);
    G0Surf s; REQUIRE(g0_surf_make(&s, 200, 40, MAG));
    RECT rc = { 0, 0, 190, 30 };
    CHECK(UN(DrawCaptionTempW)(NULL, s.dc, &rc, f, NULL, L"Font", DC_ACTIVE | DC_TEXT));
    CHECK(surf_diff(&s, 0, 0, 190, 30, MAG) > 0);
    g0_surf_free(&s);
    DeleteObject(f);
}
TEST(gap0_drawcaption_small_caption_flag) {
    REQUIRE(UN(DrawCaptionTempW));
    G0Surf s; REQUIRE(g0_surf_make(&s, 200, 40, MAG));
    RECT rc = { 0, 0, 190, 20 };
    CHECK(UN(DrawCaptionTempW)(NULL, s.dc, &rc, NULL, NULL, L"small", DC_ACTIVE | DC_SMALLCAP | DC_TEXT));
    CHECK(surf_diff(&s, 0, 0, 190, 20, MAG) > 0);
    EXPECT_EQ(surf_diff(&s, 0, 20, 200, 40, MAG), 0);
    g0_surf_free(&s);
}
TEST(gap0_drawcaption_invalid_dc_w) {
    REQUIRE(UN(DrawCaptionTempW));
    RECT rc = { 0, 0, 50, 20 };
    EXPECT_EQ(UN(DrawCaptionTempW)(NULL, NULL, &rc, NULL, NULL, L"x", DC_ACTIVE | DC_TEXT), 0);
    EXPECT_EQ(UN(DrawCaptionTempW)(NULL, (HDC)(ULONG_PTR)0x1234, &rc, NULL, NULL, L"x", DC_ACTIVE | DC_TEXT), 0);
    EXPECT_EQ(UN(DrawCaptionTempW)(NULL, (HDC)(ULONG_PTR)-1, &rc, NULL, NULL, L"x", DC_ACTIVE), 0);
}
TEST(gap0_drawcaption_invalid_dc_a) {
    REQUIRE(UN(DrawCaptionTempA));
    RECT rc = { 0, 0, 50, 20 };
    EXPECT_EQ(UN(DrawCaptionTempA)(NULL, NULL, &rc, NULL, NULL, "x", DC_ACTIVE | DC_TEXT), 0);
    EXPECT_EQ(UN(DrawCaptionTempA)(NULL, (HDC)(ULONG_PTR)0x1234, &rc, NULL, NULL, "x", DC_ACTIVE | DC_TEXT), 0);
}
TEST(gap0_drawcaption_deleted_dc) {
    REQUIRE(UN(DrawCaptionTempW)); REQUIRE(UN(DrawCaptionTempA));
    G0Surf s; REQUIRE(g0_surf_make(&s, 50, 30, MAG));
    HDC dc = s.dc;
    g0_surf_free(&s);
    RECT rc = { 0, 0, 50, 20 };
    EXPECT_EQ(UN(DrawCaptionTempW)(NULL, dc, &rc, NULL, NULL, L"x", DC_ACTIVE), 0);
    EXPECT_EQ(UN(DrawCaptionTempA)(NULL, dc, &rc, NULL, NULL, "x", DC_ACTIVE), 0);
}
TEST(gap0_drawcaption_metafile_dc) {
    REQUIRE(UN(DrawCaptionTempW)); REQUIRE(UN(DrawCaptionTempA));
    HDC mf = CreateMetaFileW(NULL);
    REQUIRE(mf);
    RECT rc = { 0, 0, 50, 20 };
    EXPECT_EQ(UN(DrawCaptionTempW)(NULL, mf, &rc, NULL, NULL, L"x", DC_ACTIVE | DC_TEXT), 0);
    EXPECT_EQ(UN(DrawCaptionTempA)(NULL, mf, &rc, NULL, NULL, "x", DC_ACTIVE | DC_TEXT), 0);
    HMETAFILE h = CloseMetaFile(mf);
    if (h) DeleteMetaFile(h);
}
TEST(gap0_drawcaption_null_and_atom_text) {
    REQUIRE(UN(DrawCaptionTempW)); REQUIRE(UN(DrawCaptionTempA));
    /* text values below 0x10000 are treated as an empty string, not dereferenced */
    G0Surf s; REQUIRE(g0_surf_make(&s, 100, 30, MAG));
    RECT rc = { 0, 0, 100, 30 };
    CHECK(UN(DrawCaptionTempW)(NULL, s.dc, &rc, NULL, NULL, NULL, DC_ACTIVE | DC_TEXT));
    CHECK(UN(DrawCaptionTempW)(NULL, s.dc, &rc, NULL, NULL, (LPCWSTR)(ULONG_PTR)0x1234, DC_ACTIVE | DC_TEXT));
    CHECK(UN(DrawCaptionTempA)(NULL, s.dc, &rc, NULL, NULL, NULL, DC_ACTIVE | DC_TEXT));
    CHECK(UN(DrawCaptionTempA)(NULL, s.dc, &rc, NULL, NULL, (LPCSTR)(ULONG_PTR)0x1234, DC_ACTIVE | DC_TEXT));
    g0_surf_free(&s);
}
TEST(gap0_drawcaption_empty_string) {
    REQUIRE(UN(DrawCaptionTempW)); REQUIRE(UN(DrawCaptionTempA));
    G0Surf a, b; REQUIRE(g0_surf_make(&a, 100, 30, MAG)); REQUIRE(g0_surf_make(&b, 100, 30, MAG));
    RECT rc = { 0, 0, 100, 30 };
    CHECK(UN(DrawCaptionTempW)(NULL, a.dc, &rc, NULL, NULL, L"", DC_ACTIVE | DC_TEXT));
    CHECK(UN(DrawCaptionTempA)(NULL, b.dc, &rc, NULL, NULL, "", DC_ACTIVE | DC_TEXT));
    GdiFlush();
    CHECK(!memcmp(a.bits, b.bits, 100 * 30 * 4));
    g0_surf_free(&a); g0_surf_free(&b);
}
TEST(gap0_drawcaption_long_text_clipped) {
    REQUIRE(UN(DrawCaptionTempW));
    WCHAR t[2000]; for (int i = 0; i < 1999; i++) t[i] = L'W'; t[1999] = 0;
    G0Surf s; REQUIRE(g0_surf_make(&s, 120, 40, MAG));
    RECT rc = { 0, 0, 100, 30 };
    CHECK(UN(DrawCaptionTempW)(NULL, s.dc, &rc, NULL, NULL, t, DC_ACTIVE | DC_TEXT));
    t_note("long caption text overdraw: right %d bottom %d", surf_diff(&s, 100, 0, 120, 40, MAG), surf_diff(&s, 0, 30, 120, 40, MAG));
    g0_surf_free(&s);
}
TEST(gap0_drawcaption_repeat) {
    REQUIRE(UN(DrawCaptionTempW));
    G0Surf s; REQUIRE(g0_surf_make(&s, 100, 30, MAG));
    RECT rc = { 0, 0, 100, 30 };
    int bad = 0;
    for (int i = 0; i < 300; i++) if (!UN(DrawCaptionTempW)(NULL, s.dc, &rc, NULL, NULL, L"loop", DC_ACTIVE | DC_TEXT)) bad++;
    EXPECT_EQ(bad, 0);
    g0_surf_free(&s);
}

/* ---------- DrawMenuBarTemp ---------- */
TEST(gap0_drawmenubartemp_null_menu) {
    REQUIRE(UN(DrawMenuBarTemp));
    G0Surf s; REQUIRE(g0_surf_make(&s, 100, 30, MAG));
    RECT rc = { 0, 0, 100, 30 };
    EXPECT_EQ(UN(DrawMenuBarTemp)(NULL, s.dc, &rc, NULL, NULL), -1);
    EXPECT_EQ(surf_diff(&s, 0, 0, 100, 30, MAG), 0);
    g0_surf_free(&s);
}
TEST(gap0_drawmenubartemp_invalid_dc) {
    REQUIRE(UN(DrawMenuBarTemp)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    REQUIRE(m);
    RECT rc = { 0, 0, 100, 30 };
    EXPECT_EQ(UN(DrawMenuBarTemp)(NULL, NULL, &rc, m, NULL), -1);
    EXPECT_EQ(UN(DrawMenuBarTemp)(NULL, (HDC)(ULONG_PTR)0x1234, &rc, m, NULL), -1);
    U(DestroyMenu)(m);
}
TEST(gap0_drawmenubartemp_deleted_dc) {
    REQUIRE(UN(DrawMenuBarTemp)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    REQUIRE(m);
    G0Surf s; REQUIRE(g0_surf_make(&s, 20, 20, MAG));
    HDC dc = s.dc; g0_surf_free(&s);
    RECT rc = { 0, 0, 100, 30 };
    EXPECT_EQ(UN(DrawMenuBarTemp)(NULL, dc, &rc, m, NULL), -1);
    U(DestroyMenu)(m);
}
TEST(gap0_drawmenubartemp_metafile_dc) {
    REQUIRE(UN(DrawMenuBarTemp)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    REQUIRE(m);
    HDC mf = CreateMetaFileW(NULL);
    REQUIRE(mf);
    RECT rc = { 0, 0, 100, 30 };
    EXPECT_EQ(UN(DrawMenuBarTemp)(NULL, mf, &rc, m, NULL), -1);
    HMETAFILE h = CloseMetaFile(mf);
    if (h) DeleteMetaFile(h);
    U(DestroyMenu)(m);
}
TEST(gap0_drawmenubartemp_bogus_menu_observed) {
    REQUIRE(UN(DrawMenuBarTemp));
    G0Surf s; REQUIRE(g0_surf_make(&s, 100, 30, MAG));
    RECT rc = { 0, 0, 100, 30 };
    SET_ERR(0);
    int r = UN(DrawMenuBarTemp)(NULL, s.dc, &rc, (HMENU)(ULONG_PTR)0xDEAD0, NULL);
    t_note("DrawMenuBarTemp(bogus HMENU) -> %d err %lu", r, GetLastError());
    g0_surf_free(&s);
}
TEST(gap0_drawmenubartemp_draws_items) {
    REQUIRE(UN(DrawMenuBarTemp)); REQUIRE(U(CreateMenu)); REQUIRE(U(AppendMenuW)); REQUIRE(U(DestroyMenu)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 300, 100, NULL);
    REQUIRE(w);
    HMENU m = U(CreateMenu)();
    REQUIRE(m);
    U(AppendMenuW)(m, MF_STRING, 1, L"File");
    U(AppendMenuW)(m, MF_STRING, 2, L"Edit");
    U(AppendMenuW)(m, MF_STRING, 3, L"Help");
    G0Surf s; REQUIRE(g0_surf_make(&s, 300, 60, MAG));
    RECT rc = { 0, 0, 300, 30 };
    int r = UN(DrawMenuBarTemp)(w, s.dc, &rc, m, NULL);
    t_note("DrawMenuBarTemp -> %d", r);
    t_note("menu bar pixels drawn: %d", surf_diff(&s, 0, 0, 300, 60, MAG));
    g0_surf_free(&s);
    U(DestroyMenu)(m);
    U(DestroyWindow)(w);
}
TEST(gap0_drawmenubartemp_empty_menu_observed) {
    REQUIRE(UN(DrawMenuBarTemp)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 300, 100, NULL);
    REQUIRE(w);
    HMENU m = U(CreateMenu)();
    REQUIRE(m);
    G0Surf s; REQUIRE(g0_surf_make(&s, 300, 60, MAG));
    RECT rc = { 0, 0, 300, 30 };
    int r = UN(DrawMenuBarTemp)(w, s.dc, &rc, m, NULL);
    t_note("DrawMenuBarTemp(empty menu) -> %d", r);
    g0_surf_free(&s);
    U(DestroyMenu)(m);
    U(DestroyWindow)(w);
    CHECK(1);
}
TEST(gap0_drawmenubartemp_destroyed_menu) {
    REQUIRE(UN(DrawMenuBarTemp)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    REQUIRE(m);
    U(DestroyMenu)(m);
    G0Surf s; REQUIRE(g0_surf_make(&s, 100, 30, MAG));
    RECT rc = { 0, 0, 100, 30 };
    SET_ERR(0);
    int r = UN(DrawMenuBarTemp)(NULL, s.dc, &rc, m, NULL);
    t_note("DrawMenuBarTemp(destroyed menu) -> %d err %lu", r, GetLastError());
    g0_surf_free(&s);
    CHECK(1);
}
