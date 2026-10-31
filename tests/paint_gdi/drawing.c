#include "pg.h"

#define W 120
#define H 80
typedef struct { HDC dc; HBITMAP bm, old; DWORD *bits; } mdc;
static int md_init(mdc *m) {
    memset(m, 0, sizeof *m);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    m->dc = CreateCompatibleDC(NULL);
    m->bm = CreateDIBSection(m->dc, &bi, DIB_RGB_COLORS, (void **)&m->bits, NULL, 0);
    if (!m->dc || !m->bm) return 0;
    m->old = SelectObject(m->dc, m->bm);
    for (int i = 0; i < W * H; i++) m->bits[i] = 0xffffff;
    SetBkMode(m->dc, TRANSPARENT);
    return 1;
}
static void md_clear(mdc *m, COLORREF c) { GdiFlush(); DWORD v = ((GetRValue(c)) << 16) | (GetGValue(c) << 8) | GetBValue(c); for (int i = 0; i < W * H; i++) m->bits[i] = v; }
static void md_done(mdc *m) { if (m->dc) { if (m->old) SelectObject(m->dc, m->old); DeleteDC(m->dc); } if (m->bm) DeleteObject(m->bm); }
static COLORREF P(mdc *m, int x, int y) { GdiFlush(); return GetPixel(m->dc, x, y); }
static int md_count_not(mdc *m, COLORREF bg, const RECT *r) {
    GdiFlush(); int n = 0;
    for (int y = r->top; y < r->bottom; y++) for (int x = r->left; x < r->right; x++) {
        if (x < 0 || y < 0 || x >= W || y >= H) continue;
        DWORD v = m->bits[y * W + x] & 0xffffff; DWORD b = ((GetRValue(bg)) << 16) | (GetGValue(bg) << 8) | GetBValue(bg);
        if (v != b) n++;
    }
    return n;
}
/* number of changed pixels outside r */
static int md_count_outside(mdc *m, COLORREF bg, const RECT *r) {
    GdiFlush(); int n = 0; DWORD b = ((GetRValue(bg)) << 16) | (GetGValue(bg) << 8) | GetBValue(bg);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        if (x >= r->left && x < r->right && y >= r->top && y < r->bottom) continue;
        if ((m->bits[y * W + x] & 0xffffff) != b) n++;
    }
    return n;
}
#define WHITE RGB(255, 255, 255)
#define SETUPM() mdc m; if (!md_init(&m)) { md_done(&m); t_skip("cannot create memory dc"); return; }

/* ---------------- FillRect ---------------- */
TEST(paint_gdi_fill_solid_brush) {
    REQUIRE(U(FillRect)); SETUPM();
    HBRUSH br = CreateSolidBrush(RGB(10, 20, 200));
    RECT r = {10, 10, 30, 25};
    CHECK(U(FillRect)(m.dc, &r, br) != 0);
    CHECK(P(&m, 10, 10) == RGB(10, 20, 200));
    CHECK(P(&m, 29, 24) == RGB(10, 20, 200));
    CHECK(P(&m, 30, 24) == WHITE);
    CHECK(P(&m, 29, 25) == WHITE);
    CHECK(P(&m, 9, 10) == WHITE);
    CHECK(P(&m, 10, 9) == WHITE);
    CHECK(md_count_outside(&m, WHITE, &r) == 0);
    CHECK(md_count_not(&m, WHITE, &r) == 20 * 15);
    DeleteObject(br); md_done(&m);
}
TEST(paint_gdi_fill_syscolor_brushes) {
    REQUIRE(U(FillRect)); REQUIRE(U(GetSysColor)); SETUPM();
    for (int i = 0; i <= 30; i++) {
        md_clear(&m, WHITE);
        RECT r = {5, 5, 15, 15};
        CHECKF(U(FillRect)(m.dc, &r, (HBRUSH)(ULONG_PTR)(i + 1)) != 0, "index %d", i);
        COLORREF want = U(GetSysColor)(i);
        CHECKF(P(&m, 7, 7) == want, "index %d got %06lx want %06lx", i, P(&m, 7, 7), want);
    }
    md_done(&m);
}
TEST(paint_gdi_fill_stock_brushes) {
    REQUIRE(U(FillRect)); SETUPM();
    struct { int id; COLORREF c; } t[] = { {BLACK_BRUSH, RGB(0, 0, 0)}, {WHITE_BRUSH, RGB(255, 255, 255)}, {LTGRAY_BRUSH, RGB(192, 192, 192)}, {GRAY_BRUSH, RGB(128, 128, 128)}, {DKGRAY_BRUSH, RGB(64, 64, 64)} };
    for (unsigned i = 0; i < 5; i++) {
        md_clear(&m, RGB(1, 2, 3));
        RECT r = {0, 0, 8, 8};
        CHECK(U(FillRect)(m.dc, &r, GetStockObject(t[i].id)) != 0);
        CHECKF(P(&m, 3, 3) == t[i].c, "stock %d got %06lx", t[i].id, P(&m, 3, 3));
    }
    md_clear(&m, RGB(1, 2, 3));
    RECT r = {0, 0, 8, 8};
    t_note("FillRect(NULL_BRUSH) -> %d", U(FillRect)(m.dc, &r, GetStockObject(NULL_BRUSH)));
    CHECK(P(&m, 3, 3) == RGB(1, 2, 3));
    md_done(&m);
}
TEST(paint_gdi_fill_empty_rects) {
    REQUIRE(U(FillRect)); SETUPM();
    HBRUSH br = CreateSolidBrush(RGB(255, 0, 0));
    RECT e = {20, 20, 20, 40}, inv = {40, 40, 20, 20};
    RECT all = {0, 0, W, H};
    int r1 = U(FillRect)(m.dc, &e, br);
    t_note("FillRect empty -> %d", r1);
    CHECK(md_count_not(&m, WHITE, &all) == 0);
    int r2 = U(FillRect)(m.dc, &inv, br);    /* PatBlt with negative extents: edge handling not pinned down */
    t_note("FillRect inverted -> %d, %d pixels", r2, md_count_not(&m, WHITE, &all));
    DeleteObject(br); md_done(&m);
}
TEST(paint_gdi_fill_clipped_to_dc) {
    REQUIRE(U(FillRect)); SETUPM();
    HBRUSH br = CreateSolidBrush(RGB(0, 255, 0));
    RECT r = {-20, -20, 10, 10};
    CHECK(U(FillRect)(m.dc, &r, br) != 0);
    CHECK(P(&m, 0, 0) == RGB(0, 255, 0));
    CHECK(P(&m, 9, 9) == RGB(0, 255, 0));
    CHECK(P(&m, 10, 9) == WHITE);
    RECT big = {-1000000, -1000000, 1000000, 1000000};
    CHECK(U(FillRect)(m.dc, &big, br) != 0);
    CHECK(P(&m, W - 1, H - 1) == RGB(0, 255, 0));
    DeleteObject(br); md_done(&m);
}
TEST(paint_gdi_fill_invalid_args) {
    REQUIRE(U(FillRect)); SETUPM();
    RECT r = {0, 0, 5, 5};
    HBRUSH br = CreateSolidBrush(0);
    SetLastError(0);
    CHECK(U(FillRect)(NULL, &r, br) == 0);
    CHECK(U(FillRect)((HDC)(ULONG_PTR)0x888, &r, br) == 0);
    SetLastError(0);
    int v = U(FillRect)(m.dc, &r, NULL);
    t_note("FillRect(NULL brush) -> %d err=%lu", v, GetLastError());
    HPEN pen = CreatePen(PS_SOLID, 1, 0);
    v = U(FillRect)(m.dc, &r, (HBRUSH)pen);
    t_note("FillRect(pen as brush) -> %d", v);
    DeleteObject(pen); DeleteObject(br); md_done(&m);
}
TEST(paint_gdi_fill_hatch_brush) {
    REQUIRE(U(FillRect)); SETUPM();
    HBRUSH br = CreateHatchBrush(HS_CROSS, RGB(255, 0, 0));
    SetBkMode(m.dc, OPAQUE); SetBkColor(m.dc, RGB(0, 0, 255));
    RECT r = {0, 0, 16, 16};
    CHECK(U(FillRect)(m.dc, &r, br) != 0);
    int red = 0, blue = 0;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) { COLORREF c = P(&m, x, y); if (c == RGB(255, 0, 0)) red++; else if (c == RGB(0, 0, 255)) blue++; }
    CHECK(red > 0 && blue > 0 && red + blue == 256);
    DeleteObject(br); md_done(&m);
}

/* ---------------- FrameRect / InvertRect ---------------- */
TEST(paint_gdi_framerect_basic) {
    REQUIRE(U(FrameRect)); SETUPM();
    HBRUSH br = CreateSolidBrush(RGB(200, 0, 0));
    RECT r = {10, 10, 30, 30};
    CHECK(U(FrameRect)(m.dc, &r, br) != 0);
    CHECK(P(&m, 10, 10) == RGB(200, 0, 0));
    CHECK(P(&m, 29, 10) == RGB(200, 0, 0));
    CHECK(P(&m, 10, 29) == RGB(200, 0, 0));
    CHECK(P(&m, 29, 29) == RGB(200, 0, 0));
    CHECK(P(&m, 20, 10) == RGB(200, 0, 0));
    CHECK(P(&m, 11, 11) == WHITE);
    CHECK(P(&m, 20, 20) == WHITE);
    CHECK(P(&m, 30, 20) == WHITE);
    CHECK(P(&m, 9, 20) == WHITE);
    CHECK(md_count_not(&m, WHITE, &(RECT){0, 0, W, H}) == 76);
    DeleteObject(br); md_done(&m);
}
TEST(paint_gdi_framerect_small_rects) {
    REQUIRE(U(FrameRect)); SETUPM();
    HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
    RECT one = {5, 5, 6, 6};
    CHECK(U(FrameRect)(m.dc, &one, br) != 0);
    CHECK(P(&m, 5, 5) == 0);
    md_clear(&m, WHITE);
    RECT two = {5, 5, 7, 7};
    U(FrameRect)(m.dc, &two, br);
    CHECK(md_count_not(&m, WHITE, &(RECT){0, 0, W, H}) == 4);
    md_clear(&m, WHITE);
    RECT thin = {5, 5, 6, 15};
    U(FrameRect)(m.dc, &thin, br);
    CHECK(md_count_not(&m, WHITE, &(RECT){0, 0, W, H}) == 10);
    DeleteObject(br); md_done(&m);
}
TEST(paint_gdi_framerect_syscolor_brush) {
    REQUIRE(U(FrameRect)); SETUPM();
    RECT r = {2, 2, 12, 12};
    int fr = U(FrameRect)(m.dc, &r, (HBRUSH)(ULONG_PTR)(COLOR_WINDOWFRAME + 1));
    t_note("FrameRect(colour index brush) -> %d pixel %06lx (COLOR_WINDOWFRAME %06lx)", fr, P(&m, 2, 2), U(GetSysColor)(COLOR_WINDOWFRAME));
    md_done(&m);
}
TEST(paint_gdi_framerect_invalid) {
    REQUIRE(U(FrameRect)); SETUPM();
    RECT r = {0, 0, 5, 5};
    CHECK(U(FrameRect)(NULL, &r, GetStockObject(BLACK_BRUSH)) == 0);
    CHECK(U(FrameRect)((HDC)(ULONG_PTR)0x999, &r, GetStockObject(BLACK_BRUSH)) == 0);
    md_done(&m);
}
TEST(paint_gdi_invertrect_twice) {
    REQUIRE(U(InvertRect)); SETUPM();
    HBRUSH br = CreateSolidBrush(RGB(10, 100, 200)); RECT all = {0, 0, W, H};
    U(FillRect)(m.dc, &all, br);
    RECT r = {10, 10, 40, 30};
    CHECK(U(InvertRect)(m.dc, &r));
    CHECK(P(&m, 10, 10) == RGB(245, 155, 55));
    CHECK(P(&m, 39, 29) == RGB(245, 155, 55));
    CHECK(P(&m, 40, 29) == RGB(10, 100, 200));
    CHECK(P(&m, 9, 10) == RGB(10, 100, 200));
    CHECK(U(InvertRect)(m.dc, &r));
    CHECK(md_count_not(&m, RGB(10, 100, 200), &all) == 0);
    DeleteObject(br); md_done(&m);
}
TEST(paint_gdi_invertrect_invalid) {
    REQUIRE(U(InvertRect));
    RECT r = {0, 0, 5, 5};
    CHECK(U(InvertRect)(NULL, &r) == FALSE);
    CHECK(U(InvertRect)((HDC)(ULONG_PTR)0x4545, &r) == FALSE);
}
TEST(paint_gdi_invertrect_empty) {
    REQUIRE(U(InvertRect)); SETUPM();
    RECT e = {10, 10, 10, 20};
    BOOL r = U(InvertRect)(m.dc, &e);
    t_note("InvertRect(empty) -> %d", r);
    CHECK(md_count_not(&m, WHITE, &(RECT){0, 0, W, H}) == 0);
    md_done(&m);
}
TEST(paint_gdi_focusrect_xor) {
    REQUIRE(U(DrawFocusRect)); SETUPM();
    RECT r = {10, 10, 50, 40}, all = {0, 0, W, H};
    CHECK(U(DrawFocusRect)(m.dc, &r));
    int n = md_count_not(&m, WHITE, &all);
    CHECK(n > 0);
    CHECK(md_count_outside(&m, WHITE, &r) == 0);
    CHECK(P(&m, 30, 25) == WHITE);        /* interior untouched */
    CHECK(U(DrawFocusRect)(m.dc, &r));
    CHECK(md_count_not(&m, WHITE, &all) == 0);     /* second call erases */
    md_done(&m);
}
TEST(paint_gdi_focusrect_on_colour) {
    REQUIRE(U(DrawFocusRect)); SETUPM();
    md_clear(&m, RGB(0, 0, 0));
    RECT r = {10, 10, 50, 40}, all = {0, 0, W, H};
    CHECK(U(DrawFocusRect)(m.dc, &r));
    CHECK(md_count_not(&m, RGB(0, 0, 0), &all) > 0);
    CHECK(U(DrawFocusRect)(m.dc, &r));
    CHECK(md_count_not(&m, RGB(0, 0, 0), &all) == 0);
    md_done(&m);
}
TEST(paint_gdi_focusrect_invalid) {
    REQUIRE(U(DrawFocusRect));
    RECT r = {0, 0, 10, 10};
    t_note("DrawFocusRect(NULL dc) -> %d", U(DrawFocusRect)(NULL, &r));
    t_note("DrawFocusRect(bad dc) -> %d", U(DrawFocusRect)((HDC)(ULONG_PTR)0x6767, &r));
}

/* ---------------- DrawEdge ---------------- */
TEST(paint_gdi_edge_adjust_amounts) {
    REQUIRE(U(DrawEdge)); SETUPM();
    static const struct { UINT edge; int n; } t[] = {
        {BDR_RAISEDOUTER, 1}, {BDR_RAISEDINNER, 1}, {BDR_SUNKENOUTER, 1}, {BDR_SUNKENINNER, 1},
        {EDGE_RAISED, 2}, {EDGE_SUNKEN, 2}, {EDGE_ETCHED, 2}, {EDGE_BUMP, 2},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT r = {10, 10, 60, 50};
        BOOL ok = U(DrawEdge)(m.dc, &r, t[i].edge, BF_RECT | BF_ADJUST);
        CHECKF(ok, "edge 0x%x", t[i].edge);
        CHECKF(r.left == 10 + t[i].n && r.top == 10 + t[i].n && r.right == 60 - t[i].n && r.bottom == 50 - t[i].n,
               "edge 0x%x got (%ld,%ld,%ld,%ld)", t[i].edge, RECT_STR(r));
    }
    md_done(&m);
}
TEST(paint_gdi_edge_adjust_sides) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 60, 50};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_LEFT | BF_ADJUST));
    CHECK_RECT(r, 12, 10, 60, 50);
    r = (RECT){10, 10, 60, 50};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_TOP | BF_ADJUST));
    CHECK_RECT(r, 10, 12, 60, 50);
    r = (RECT){10, 10, 60, 50};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_SUNKEN, BF_RIGHT | BF_ADJUST));
    CHECK_RECT(r, 10, 10, 58, 50);
    r = (RECT){10, 10, 60, 50};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_SUNKEN, BF_BOTTOM | BF_ADJUST));
    CHECK_RECT(r, 10, 10, 60, 48);
    r = (RECT){10, 10, 60, 50};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_LEFT | BF_TOP | BF_ADJUST));
    CHECK_RECT(r, 12, 12, 60, 50);
    md_done(&m);
}
TEST(paint_gdi_edge_no_adjust_keeps_rect) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 60, 50};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_RECT));
    CHECK_RECT(r, 10, 10, 60, 50);
    md_done(&m);
}
TEST(paint_gdi_edge_pixels_raised_vs_sunken) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 40, 40}, all = {0, 0, W, H};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_RECT));
    COLORREF tl = P(&m, 10, 10), br = P(&m, 39, 39), tl2 = P(&m, 11, 11), br2 = P(&m, 38, 38);
    CHECK(tl != br);
    CHECK(tl2 != br2);
    CHECK(md_count_outside(&m, WHITE, &r) == 0);
    CHECK(P(&m, 20, 20) == WHITE);          /* no BF_MIDDLE: interior untouched */
    md_clear(&m, WHITE);
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_SUNKEN, BF_RECT));
    CHECK(P(&m, 10, 10) != P(&m, 39, 39));
    t_note("raised TL/BR %06lx/%06lx inner %06lx/%06lx; sunken TL/BR %06lx/%06lx", tl, br, tl2, br2, P(&m, 10, 10), P(&m, 39, 39));
    md_done(&m);
}
TEST(paint_gdi_edge_colours_follow_syscolors) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 40, 40};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_RECT));
    COLORREF known[] = { U(GetSysColor)(COLOR_3DLIGHT), U(GetSysColor)(COLOR_3DHILIGHT), U(GetSysColor)(COLOR_3DSHADOW), U(GetSysColor)(COLOR_3DDKSHADOW), U(GetSysColor)(COLOR_BTNFACE) };
    COLORREF px[] = { P(&m, 10, 10), P(&m, 11, 11), P(&m, 39, 39), P(&m, 38, 38) };
    for (int i = 0; i < 4; i++) {
        int found = 0;
        for (int j = 0; j < 5; j++) if (px[i] == known[j]) found = 1;
        CHECKF(found, "edge pixel %d = %06lx is not a 3D system colour", i, px[i]);
    }
    md_done(&m);
}
TEST(paint_gdi_edge_middle_fill) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 40, 40};
    CHECK(U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_RECT | BF_MIDDLE));
    CHECK(P(&m, 25, 25) == U(GetSysColor)(COLOR_BTNFACE));
    CHECK(P(&m, 12, 12) == U(GetSysColor)(COLOR_BTNFACE));
    md_done(&m);
}
TEST(paint_gdi_edge_flat_mono_soft) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 40, 40};
    static const UINT fl[] = { BF_FLAT, BF_MONO, BF_SOFT };
    for (int i = 0; i < 3; i++) {
        md_clear(&m, WHITE);
        r = (RECT){10, 10, 40, 40};
        BOOL ok = U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_RECT | fl[i]);
        CHECKF(ok, "flag 0x%x", fl[i]);
        CHECK(md_count_not(&m, WHITE, &(RECT){0, 0, W, H}) > 0);
    }
    md_done(&m);
}
TEST(paint_gdi_edge_diagonal) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 40, 40};
    BOOL ok = U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_DIAGONAL_ENDBOTTOMRIGHT);
    CHECK(ok);
    CHECK(md_count_not(&m, WHITE, &(RECT){0, 0, W, H}) > 0);
    md_done(&m);
}
TEST(paint_gdi_edge_zero_edge) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 40, 40};
    BOOL ok = U(DrawEdge)(m.dc, &r, 0, BF_RECT | BF_ADJUST);
    t_note("DrawEdge(edge 0) -> %d rect (%ld,%ld,%ld,%ld)", ok, RECT_STR(r));
    CHECK(md_count_not(&m, WHITE, &(RECT){0, 0, W, H}) == 0);
    md_done(&m);
}
TEST(paint_gdi_edge_invalid) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {10, 10, 40, 40};
    SetLastError(0);
    t_note("DrawEdge(NULL dc) -> %d", U(DrawEdge)(NULL, &r, EDGE_RAISED, BF_RECT));
    t_note("DrawEdge(bad dc) -> %d", U(DrawEdge)((HDC)(ULONG_PTR)0x3131, &r, EDGE_RAISED, BF_RECT));
    SetLastError(0);
    BOOL ok = U(DrawEdge)(m.dc, &r, BDR_RAISEDOUTER | BDR_SUNKENOUTER, BF_RECT);
    t_note("DrawEdge(conflicting edge bits) -> %d err=%lu", ok, GetLastError());
    ok = U(DrawEdge)(m.dc, &r, 0xffff, BF_RECT);
    t_note("DrawEdge(edge 0xffff) -> %d", ok);
    ok = U(DrawEdge)(m.dc, &r, EDGE_RAISED, 0);
    t_note("DrawEdge(no BF side flags) -> %d", ok);
    md_done(&m);
}
TEST(paint_gdi_edge_inverted_rect) {
    REQUIRE(U(DrawEdge)); SETUPM();
    RECT r = {40, 40, 10, 10};
    BOOL ok = U(DrawEdge)(m.dc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
    t_note("DrawEdge(inverted) -> %d rect (%ld,%ld,%ld,%ld)", ok, RECT_STR(r));
    md_done(&m);
}

/* ---------------- DrawFrameControl ---------------- */
TEST(paint_gdi_dfc_pushbutton) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    RECT r = {10, 10, 70, 40}, orig = r;
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONPUSH));
    CHECK_RECT(r, 10, 10, 70, 40);
    CHECK(md_count_not(&m, WHITE, &orig) > 0);
    CHECK(md_count_outside(&m, WHITE, &orig) == 0);
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_ADJUSTRECT));
    CHECK(r.left > 10 && r.top > 10 && r.right < 70 && r.bottom < 40);
    md_done(&m);
}
TEST(paint_gdi_dfc_button_states_differ) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    static const UINT st[] = { DFCS_BUTTONPUSH, DFCS_BUTTONPUSH | DFCS_PUSHED, DFCS_BUTTONPUSH | DFCS_INACTIVE, DFCS_BUTTONPUSH | DFCS_FLAT };
    DWORD snap[4][W * H / 8];
    unsigned long long hash[4];
    for (int i = 0; i < 4; i++) {
        md_clear(&m, WHITE);
        RECT r = {10, 10, 50, 40};
        CHECKF(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, st[i]), "state %d", i);
        GdiFlush();
        unsigned long long h = 1469598103934665603ULL;
        for (int k = 0; k < W * H; k++) { h ^= m.bits[k]; h *= 1099511628211ULL; }
        hash[i] = h;
    }
    CHECK(hash[0] != hash[1]);
    CHECK(hash[0] != hash[3]);
    md_done(&m);
}
TEST(paint_gdi_dfc_checkbox_radio) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    RECT r = {10, 10, 26, 26}, all = {0, 0, W, H};
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONCHECK));
    int unchecked = md_count_not(&m, WHITE, &all);
    md_clear(&m, WHITE);
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED));
    int checked = md_count_not(&m, WHITE, &all);
    CHECK(unchecked > 0);
    CHECK(checked > unchecked);
    md_clear(&m, WHITE);
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONRADIO));
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_clear(&m, WHITE);
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONRADIO | DFCS_CHECKED));
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_clear(&m, WHITE);
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTON3STATE | DFCS_CHECKED));
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_clear(&m, WHITE);
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONRADIOIMAGE));
    CHECK(U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONRADIOMASK));
    md_done(&m);
}
TEST(paint_gdi_dfc_caption_glyphs) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    static const UINT g[] = { DFCS_CAPTIONCLOSE, DFCS_CAPTIONMIN, DFCS_CAPTIONMAX, DFCS_CAPTIONRESTORE, DFCS_CAPTIONHELP };
    RECT all = {0, 0, W, H};
    for (unsigned i = 0; i < 5; i++) {
        md_clear(&m, WHITE);
        RECT r = {10, 10, 26, 26}, orig = r;
        CHECKF(U(DrawFrameControl)(m.dc, &r, DFC_CAPTION, g[i]), "glyph %u", i);
        CHECKF(md_count_not(&m, WHITE, &orig) > 0, "glyph %u draws nothing", i);
        CHECKF(md_count_outside(&m, WHITE, &orig) == 0, "glyph %u draws outside", i);
    }
    md_done(&m);
}
TEST(paint_gdi_dfc_scroll_arrows) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    static const UINT g[] = { DFCS_SCROLLUP, DFCS_SCROLLDOWN, DFCS_SCROLLLEFT, DFCS_SCROLLRIGHT, DFCS_SCROLLCOMBOBOX, DFCS_SCROLLSIZEGRIP, DFCS_SCROLLSIZEGRIPRIGHT };
    for (unsigned i = 0; i < 7; i++) {
        md_clear(&m, WHITE);
        RECT r = {10, 10, 27, 27}, orig = r;
        if (i == 6) { t_note("SCROLLSIZEGRIPRIGHT -> %d, %d px", U(DrawFrameControl)(m.dc, &r, DFC_SCROLL, g[i]), md_count_not(&m, WHITE, &orig)); CHECKF(md_count_outside(&m, WHITE, &orig) == 0, "scroll %u draws outside", i); continue; }
        CHECKF(U(DrawFrameControl)(m.dc, &r, DFC_SCROLL, g[i]), "scroll %u", i);
        CHECKF(md_count_not(&m, WHITE, &orig) > 0, "scroll %u draws nothing", i);
        CHECKF(md_count_outside(&m, WHITE, &orig) == 0, "scroll %u draws outside", i);
    }
    md_done(&m);
}
TEST(paint_gdi_dfc_menu_glyphs) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    static const UINT g[] = { DFCS_MENUARROW, DFCS_MENUCHECK, DFCS_MENUBULLET, DFCS_MENUARROWRIGHT };
    for (unsigned i = 0; i < 4; i++) {
        md_clear(&m, WHITE);
        RECT r = {10, 10, 26, 26}, orig = r;
        if (i == 3) t_note("MENUARROWRIGHT -> %d", U(DrawFrameControl)(m.dc, &r, DFC_MENU, g[i]));
        else CHECKF(U(DrawFrameControl)(m.dc, &r, DFC_MENU, g[i]), "menu %u", i);
        CHECKF(md_count_outside(&m, WHITE, &orig) == 0, "menu %u draws outside", i);
    }
    md_done(&m);
}
TEST(paint_gdi_dfc_popupmenu) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    RECT r = {10, 10, 26, 26};
    BOOL ok = U(DrawFrameControl)(m.dc, &r, DFC_POPUPMENU, DFCS_CAPTIONCLOSE);
    t_note("DFC_POPUPMENU -> %d", ok);
    md_done(&m);
}
TEST(paint_gdi_dfc_invalid) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    RECT r = {10, 10, 26, 26};
    SetLastError(0);
    t_note("DrawFrameControl(NULL dc) -> %d", U(DrawFrameControl)(NULL, &r, DFC_BUTTON, DFCS_BUTTONPUSH));
    t_note("DrawFrameControl(bad dc) -> %d", U(DrawFrameControl)((HDC)(ULONG_PTR)0x5151, &r, DFC_BUTTON, DFCS_BUTTONPUSH));
    SetLastError(0);
    BOOL ok = U(DrawFrameControl)(m.dc, &r, 0x77, 0);
    t_note("DrawFrameControl(type 0x77) -> %d err=%lu", ok, GetLastError());
    ok = U(DrawFrameControl)(m.dc, &r, 0, 0);
    t_note("DrawFrameControl(type 0) -> %d", ok);
    ok = U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, 0xffff);
    t_note("DrawFrameControl(state 0xffff) -> %d", ok);
    ok = U(DrawFrameControl)(m.dc, &r, DFC_CAPTION, 0x77);
    t_note("DrawFrameControl(caption glyph 0x77) -> %d", ok);
    md_done(&m);
}
TEST(paint_gdi_dfc_empty_rect) {
    REQUIRE(U(DrawFrameControl)); SETUPM();
    RECT r = {10, 10, 10, 10};
    BOOL ok = U(DrawFrameControl)(m.dc, &r, DFC_BUTTON, DFCS_BUTTONPUSH);
    t_note("DrawFrameControl(empty rect) -> %d", ok);
    t_note("empty rect drew %d px", md_count_not(&m, WHITE, &(RECT){0, 0, W, H}));
    md_done(&m);
}

/* ---------------- icons ---------------- */
static HICON stock_icon(void) { return U(LoadIconW)(NULL, MAKEINTRESOURCEW(32512)); }
TEST(paint_gdi_icon_drawicon) {
    REQUIRE(U(DrawIcon)); REQUIRE(U(LoadIconW)); SETUPM();
    HICON ic = stock_icon();
    CHECK(ic != NULL); if (!ic) { md_done(&m); return; }
    int cx = U(GetSystemMetrics)(SM_CXICON), cy = U(GetSystemMetrics)(SM_CYICON);
    CHECK(U(DrawIcon)(m.dc, 20, 10, ic));
    RECT box = {20, 10, 20 + cx, 10 + cy};
    CHECK(md_count_not(&m, WHITE, &box) > 0);
    CHECK(md_count_outside(&m, WHITE, &box) == 0);
    md_done(&m);
}
TEST(paint_gdi_icon_drawiconex_sizes) {
    REQUIRE(U(DrawIconEx)); REQUIRE(U(LoadIconW)); SETUPM();
    HICON ic = stock_icon(); if (!ic) { md_done(&m); t_skip("no icon"); return; }
    static const int sz[] = { 16, 24, 32, 48 };
    for (int i = 0; i < 4; i++) {
        md_clear(&m, WHITE);
        CHECKF(U(DrawIconEx)(m.dc, 10, 5, ic, sz[i], sz[i], 0, NULL, DI_NORMAL), "size %d", sz[i]);
        RECT box = {10, 5, 10 + sz[i], 5 + sz[i]};
        CHECKF(md_count_not(&m, WHITE, &box) > 0, "size %d draws nothing", sz[i]);
        CHECKF(md_count_outside(&m, WHITE, &box) == 0, "size %d overflows", sz[i]);
    }
    md_done(&m);
}
TEST(paint_gdi_icon_drawiconex_defaultsize) {
    REQUIRE(U(DrawIconEx)); REQUIRE(U(LoadIconW)); SETUPM();
    HICON ic = stock_icon(); if (!ic) { md_done(&m); t_skip("no icon"); return; }
    int cx = U(GetSystemMetrics)(SM_CXICON), cy = U(GetSystemMetrics)(SM_CYICON);
    CHECK(U(DrawIconEx)(m.dc, 10, 5, ic, 0, 0, 0, NULL, DI_NORMAL | DI_DEFAULTSIZE));
    RECT box = {10, 5, 10 + cx, 5 + cy};
    CHECK(md_count_not(&m, WHITE, &box) > 0);
    CHECK(md_count_outside(&m, WHITE, &box) == 0);
    md_clear(&m, WHITE);
    CHECK(U(DrawIconEx)(m.dc, 10, 5, ic, 0, 0, 0, NULL, DI_NORMAL));     /* 0 size without DI_DEFAULTSIZE: icon's own size */
    CHECK(md_count_not(&m, WHITE, &box) > 0);
    md_done(&m);
}
TEST(paint_gdi_icon_drawiconex_mask_image) {
    REQUIRE(U(DrawIconEx)); REQUIRE(U(LoadIconW)); SETUPM();
    HICON ic = stock_icon(); if (!ic) { md_done(&m); t_skip("no icon"); return; }
    RECT all = {0, 0, W, H};
    CHECK(U(DrawIconEx)(m.dc, 10, 5, ic, 32, 32, 0, NULL, DI_MASK));
    int mask = md_count_not(&m, WHITE, &all);
    md_clear(&m, WHITE);
    CHECK(U(DrawIconEx)(m.dc, 10, 5, ic, 32, 32, 0, NULL, DI_IMAGE));
    int image = md_count_not(&m, WHITE, &all);
    t_note("DI_MASK pixels %d, DI_IMAGE pixels %d", mask, image);
    CHECK(image > 0);
    md_clear(&m, WHITE);
    CHECK(U(DrawIconEx)(m.dc, 10, 5, ic, 32, 32, 0, NULL, DI_NORMAL | DI_COMPAT));
    md_done(&m);
}
TEST(paint_gdi_icon_drawiconex_flicker_free_brush) {
    REQUIRE(U(DrawIconEx)); REQUIRE(U(LoadIconW)); SETUPM();
    HICON ic = stock_icon(); if (!ic) { md_done(&m); t_skip("no icon"); return; }
    HBRUSH br = CreateSolidBrush(RGB(0, 0, 255));
    /* a background brush makes DrawIconEx compose off screen: the icon box is first filled with the brush */
    CHECK(U(DrawIconEx)(m.dc, 10, 5, ic, 32, 32, 0, br, DI_NORMAL));
    int blue = 0;
    for (int y = 5; y < 37; y++) for (int x = 10; x < 42; x++) if (P(&m, x, y) == RGB(0, 0, 255)) blue++;
    t_note("blue background pixels behind icon: %d", blue);
    CHECK(blue > 0);
    DeleteObject(br); md_done(&m);
}
TEST(paint_gdi_icon_invalid_handles) {
    REQUIRE(U(DrawIcon)); REQUIRE(U(DrawIconEx)); SETUPM();
    SetLastError(0);
    CHECK(U(DrawIcon)(m.dc, 0, 0, NULL) == FALSE);
    SetLastError(0);
    CHECK(U(DrawIcon)(m.dc, 0, 0, (HICON)(ULONG_PTR)0x1234) == FALSE);
    t_note("DrawIcon(bad icon) err=%lu", GetLastError());
    SetLastError(0);
    CHECK(U(DrawIconEx)(m.dc, 0, 0, NULL, 16, 16, 0, NULL, DI_NORMAL) == FALSE);
    CHECK(U(DrawIconEx)(m.dc, 0, 0, (HICON)(ULONG_PTR)0x4321, 16, 16, 0, NULL, DI_NORMAL) == FALSE);
    t_note("DrawIconEx(bad icon) err=%lu", GetLastError());
    HICON ic = stock_icon();
    if (ic) {
        t_note("DrawIcon(NULL dc) -> %d", U(DrawIcon)(NULL, 0, 0, ic));
        t_note("DrawIconEx(NULL dc) -> %d", U(DrawIconEx)(NULL, 0, 0, ic, 16, 16, 0, NULL, DI_NORMAL));
        SetLastError(0);
        BOOL r = U(DrawIconEx)(m.dc, 0, 0, ic, 16, 16, 0, NULL, 0);
        t_note("DrawIconEx(flags 0) -> %d err=%lu", r, GetLastError());
        r = U(DrawIconEx)(m.dc, 0, 0, ic, -5, -5, 0, NULL, DI_NORMAL);
        t_note("DrawIconEx(negative size) -> %d", r);
    }
    md_done(&m);
}
TEST(paint_gdi_icon_destroyed_handle) {
    REQUIRE(U(DrawIcon)); REQUIRE(U(CreateIcon)); REQUIRE(U(DestroyIcon)); SETUPM();
    BYTE andm[32 * 4], xorm[32 * 4];
    memset(andm, 0xff, sizeof andm); memset(xorm, 0, sizeof xorm);
    HICON ic = U(CreateIcon)(GetModuleHandleA(NULL), 32, 32, 1, 1, andm, xorm);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIcon)(m.dc, 0, 0, ic));
        CHECK(U(DestroyIcon)(ic));
        SetLastError(0);
        CHECK(U(DrawIcon)(m.dc, 0, 0, ic) == FALSE);
    }
    md_done(&m);
}
TEST(paint_gdi_icon_created_pixels) {
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIcon)); REQUIRE(U(DestroyIcon)); SETUPM();
    /* 1bpp icon: AND mask all 0 (opaque), XOR plane all 1 -> white-on-opaque draws white; use mask hole in right half */
    BYTE andm[32 * 4], xorm[32 * 4];
    for (int y = 0; y < 32; y++) { andm[y * 4] = 0x00; andm[y * 4 + 1] = 0x00; andm[y * 4 + 2] = 0xff; andm[y * 4 + 3] = 0xff; }
    memset(xorm, 0x00, sizeof xorm);     /* opaque black on the left half, transparent right half */
    HICON ic = U(CreateIcon)(GetModuleHandleA(NULL), 32, 32, 1, 1, andm, xorm);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(m.dc, 10, 10, ic, 32, 32, 0, NULL, DI_NORMAL));
        CHECK(P(&m, 14, 20) == RGB(0, 0, 0));         /* left half is opaque black */
        CHECK(P(&m, 10 + 24, 20) == WHITE);           /* right half transparent */
        CHECK(P(&m, 9, 20) == WHITE);
        U(DestroyIcon)(ic);
    }
    md_done(&m);
}

/* ---------------- DrawState ---------------- */
typedef struct { int calls; HDC dc; LPARAM l; WPARAM w; int cx, cy; int ret; } dscb;
static dscb g_ds;
static BOOL CALLBACK ds_cb(HDC dc, LPARAM l, WPARAM w, int cx, int cy) {
    g_ds.calls++; g_ds.dc = dc; g_ds.l = l; g_ds.w = w; g_ds.cx = cx; g_ds.cy = cy;
    return g_ds.ret;
}
TEST(paint_gdi_drawstate_complex_callback) {
    REQUIRE(U(DrawStateA)); SETUPM();
    memset(&g_ds, 0, sizeof g_ds); g_ds.ret = TRUE;
    BOOL r = U(DrawStateA)(m.dc, NULL, ds_cb, 0x1234, 0x5678, 10, 10, 30, 20, DST_COMPLEX | DSS_NORMAL);
    CHECK(r);
    EQ(g_ds.calls, 1);
    CHECK(g_ds.l == 0x1234 && g_ds.w == 0x5678);
    EQ(g_ds.cx, 30); EQ(g_ds.cy, 20);
    CHECK(g_ds.dc == m.dc);        /* normal state draws straight into the supplied DC */
    md_done(&m);
}
TEST(paint_gdi_drawstate_complex_callback_false) {
    REQUIRE(U(DrawStateA)); SETUPM();
    memset(&g_ds, 0, sizeof g_ds); g_ds.ret = FALSE;
    BOOL r = U(DrawStateA)(m.dc, NULL, ds_cb, 1, 2, 0, 0, 10, 10, DST_COMPLEX | DSS_NORMAL);
    CHECK(r == FALSE);
    EQ(g_ds.calls, 1);
    md_done(&m);
}
TEST(paint_gdi_drawstate_complex_disabled) {
    REQUIRE(U(DrawStateA)); SETUPM();
    memset(&g_ds, 0, sizeof g_ds); g_ds.ret = TRUE;
    BOOL r = U(DrawStateA)(m.dc, NULL, ds_cb, 7, 8, 5, 5, 20, 20, DST_COMPLEX | DSS_DISABLED);
    CHECK(r);
    t_note("DSS_DISABLED callback calls=%d dc==target:%d cx=%d cy=%d", g_ds.calls, g_ds.dc == m.dc, g_ds.cx, g_ds.cy);
    CHECK(g_ds.calls >= 1);
    CHECK(g_ds.l == 7 && g_ds.w == 8);
    md_done(&m);
}
TEST(paint_gdi_drawstate_text) {
    REQUIRE(U(DrawStateA)); SETUPM();
    RECT all = {0, 0, W, H};
    BOOL r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)"Hello", 5, 10, 10, 0, 0, DST_TEXT | DSS_NORMAL);
    CHECK(r);
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_clear(&m, WHITE);
    r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)"Hello", 0, 10, 10, 0, 0, DST_TEXT | DSS_NORMAL);    /* length 0 = use strlen */
    CHECK(r);
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_clear(&m, WHITE);
    r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)"Hello", 5, 10, 10, 0, 0, DST_TEXT | DSS_DISABLED);
    CHECK(r);
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_clear(&m, WHITE);
    r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)"Hello", 5, 10, 10, 0, 0, DST_TEXT | DSS_UNION);
    CHECK(r);
    md_clear(&m, WHITE);
    r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)"Hello", 5, 10, 10, 0, 0, DST_TEXT | DSS_MONO);
    CHECK(r);
    md_done(&m);
}
TEST(paint_gdi_drawstate_prefixtext) {
    REQUIRE(U(DrawStateA)); SETUPM();
    RECT all = {0, 0, W, H};
    BOOL r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)"&File", 5, 10, 10, 0, 0, DST_PREFIXTEXT | DSS_NORMAL);
    CHECK(r);
    int a = md_count_not(&m, WHITE, &all);
    md_clear(&m, WHITE);
    r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)"&File", 5, 10, 10, 0, 0, DST_TEXT | DSS_NORMAL);
    CHECK(r);
    int b = md_count_not(&m, WHITE, &all);
    t_note("prefix text pixels %d, plain text pixels %d", a, b);
    CHECK(a > 0 && b > 0);
    md_done(&m);
}
TEST(paint_gdi_drawstate_wide_text) {
    REQUIRE(U(DrawStateW)); SETUPM();
    RECT all = {0, 0, W, H};
    BOOL r = U(DrawStateW)(m.dc, NULL, NULL, (LPARAM)L"Hello", 5, 10, 10, 0, 0, DST_TEXT | DSS_NORMAL);
    CHECK(r);
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_done(&m);
}
TEST(paint_gdi_drawstate_icon_bitmap) {
    REQUIRE(U(DrawStateA)); REQUIRE(U(LoadIconW)); SETUPM();
    RECT all = {0, 0, W, H};
    HICON ic = stock_icon();
    if (ic) {
        CHECK(U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)ic, 0, 10, 10, 32, 32, DST_ICON | DSS_NORMAL));
        CHECK(md_count_not(&m, WHITE, &all) > 0);
        md_clear(&m, WHITE);
        CHECK(U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)ic, 0, 10, 10, 32, 32, DST_ICON | DSS_DISABLED));
        CHECK(md_count_not(&m, WHITE, &all) > 0);
        md_clear(&m, WHITE);
    }
    HBITMAP bm = CreateBitmap(16, 16, 1, 1, NULL);
    HDC t = CreateCompatibleDC(NULL); HGDIOBJ o = SelectObject(t, bm);
    PatBlt(t, 0, 0, 16, 16, BLACKNESS); SelectObject(t, o); DeleteDC(t);
    CHECK(U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)bm, 0, 10, 10, 16, 16, DST_BITMAP | DSS_NORMAL));
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    DeleteObject(bm);
    md_done(&m);
}
TEST(paint_gdi_drawstate_bitmap_colour) {
    REQUIRE(U(DrawStateA)); SETUPM();
    HDC s = GetDC(NULL); HBITMAP bm = CreateCompatibleBitmap(s, 8, 8); ReleaseDC(NULL, s);
    HDC t = CreateCompatibleDC(NULL); HGDIOBJ o = SelectObject(t, bm);
    HBRUSH br = CreateSolidBrush(RGB(255, 0, 0)); RECT r8 = {0, 0, 8, 8}; FillRect(t, &r8, br); DeleteObject(br);
    SelectObject(t, o); DeleteDC(t);
    CHECK(U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)bm, 0, 10, 10, 8, 8, DST_BITMAP | DSS_NORMAL));
    CHECK(P(&m, 12, 12) == RGB(255, 0, 0));
    CHECK(P(&m, 18, 12) == WHITE);
    DeleteObject(bm);
    md_done(&m);
}
TEST(paint_gdi_drawstate_invalid) {
    REQUIRE(U(DrawStateA)); SETUPM();
    CHECK(U(DrawStateA)(NULL, NULL, NULL, (LPARAM)"x", 1, 0, 0, 0, 0, DST_TEXT) == FALSE);
    SetLastError(0);
    BOOL r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)"x", 1, 0, 0, 0, 0, 0x7);
    t_note("DrawState(type 7) -> %d err=%lu", r, GetLastError());
    r = U(DrawStateA)(m.dc, NULL, NULL, 0, 0, 0, 0, 0, 0, DST_COMPLEX);
    t_note("DrawState(DST_COMPLEX, no callback) -> %d err=%lu", r, GetLastError());
    r = U(DrawStateA)(m.dc, NULL, NULL, (LPARAM)(ULONG_PTR)0x1234, 0, 0, 0, 0, 0, DST_ICON);
    t_note("DrawState(DST_ICON bad icon) -> %d", r);
    md_done(&m);
}

/* ---------------- DrawCaption / animated rects / GrayString / PaintDesktop ---------------- */
TEST(paint_gdi_caption_fill) {
    REQUIRE(U(DrawCaption)); SETUPM();
    HWND w = pg_window(WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    CHECK(w != NULL); if (!w) { md_done(&m); return; }
    RECT r = {5, 5, 100, 25}, all = {0, 0, W, H};
    BOOL ok = U(DrawCaption)(w, m.dc, &r, DC_ACTIVE);
    CHECK(ok);
    CHECK(md_count_outside(&m, WHITE, &r) == 0);
    CHECK(md_count_not(&m, WHITE, &r) > 0);
    COLORREF mid = P(&m, 50, 22);
    t_note("DrawCaption(DC_ACTIVE) body pixel %06lx, COLOR_ACTIVECAPTION %06lx", mid, U(GetSysColor)(COLOR_ACTIVECAPTION));
    md_clear(&m, WHITE);
    ok = U(DrawCaption)(w, m.dc, &r, 0);
    CHECK(ok);
    t_note("DrawCaption(inactive) body pixel %06lx, COLOR_INACTIVECAPTION %06lx", P(&m, 50, 22), U(GetSysColor)(COLOR_INACTIVECAPTION));
    U(DestroyWindow)(w);
    md_done(&m);
}
TEST(paint_gdi_caption_flags) {
    REQUIRE(U(DrawCaption)); SETUPM();
    HWND w = pg_window(WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    CHECK(w != NULL); if (!w) { md_done(&m); return; }
    U(SetWindowTextA)(w, "Caption text");
    static const UINT f[] = { DC_TEXT, DC_ICON, DC_TEXT | DC_ICON, DC_GRADIENT, DC_SMALLCAP, DC_TEXT | DC_ACTIVE | DC_ICON | DC_GRADIENT, DC_BUTTONS, DC_INBUTTON };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        md_clear(&m, WHITE);
        RECT r = {2, 2, 110, 26};
        BOOL ok = U(DrawCaption)(w, m.dc, &r, f[i]);
        if (f[i] & (DC_BUTTONS | DC_INBUTTON)) t_note("DrawCaption flags 0x%x -> %d", f[i], ok); else CHECKF(ok, "flags 0x%x", f[i]);
        CHECKF(md_count_outside(&m, WHITE, &r) == 0, "flags 0x%x draws outside rect", f[i]);
    }
    U(DestroyWindow)(w);
    md_done(&m);
}
TEST(paint_gdi_caption_invalid) {
    REQUIRE(U(DrawCaption)); SETUPM();
    RECT r = {0, 0, 50, 20};
    HWND w = pg_window(WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    CHECK(U(DrawCaption)(w, NULL, &r, DC_ACTIVE) == FALSE);
    CHECK(U(DrawCaption)(w, (HDC)(ULONG_PTR)0x7878, &r, DC_ACTIVE) == FALSE);
    SetLastError(0);
    BOOL ok = U(DrawCaption)((HWND)(ULONG_PTR)0xdead, m.dc, &r, DC_TEXT);
    t_note("DrawCaption(bad hwnd, DC_TEXT) -> %d err=%lu", ok, GetLastError());
    ok = U(DrawCaption)(NULL, m.dc, &r, 0);
    t_note("DrawCaption(NULL hwnd) -> %d", ok);
    if (w) U(DestroyWindow)(w);
    md_done(&m);
}
TEST(paint_gdi_animated_rects) {
    REQUIRE(U(DrawAnimatedRects));
    HWND w = pg_topwin(300, 300, 100, 80);
    CHECK(w != NULL); if (!w) return;
    RECT from = {300, 300, 310, 305}, to = {300, 300, 400, 380};
    ULONGLONG t0 = GetTickCount64();
    BOOL ok = U(DrawAnimatedRects)(w, IDANI_CAPTION, &from, &to);
    ULONGLONG dt = GetTickCount64() - t0;
    t_note("DrawAnimatedRects -> %d in %llu ms", ok, dt);
    CHECK(dt < 5000);
    pg_flush(w);
    U(DestroyWindow)(w); pg_pump();
}
TEST(paint_gdi_animated_rects_invalid) {
    REQUIRE(U(DrawAnimatedRects));
    RECT from = {300, 300, 310, 305}, to = {300, 300, 400, 380};
    SetLastError(0);
    BOOL ok = U(DrawAnimatedRects)(NULL, 99, &from, &to);
    t_note("DrawAnimatedRects(idAni 99) -> %d err=%lu", ok, GetLastError());
    ok = U(DrawAnimatedRects)(NULL, 0, &from, &to);
    t_note("DrawAnimatedRects(idAni 0) -> %d", ok);
    ok = U(DrawAnimatedRects)((HWND)(ULONG_PTR)0xdead, IDANI_CAPTION, &from, &to);
    t_note("DrawAnimatedRects(bad hwnd) -> %d", ok);
}
static int g_gs_calls; static LPARAM g_gs_l; static int g_gs_n; static int g_gs_ret;
static BOOL CALLBACK gs_cb(HDC dc, LPARAM l, int n) { g_gs_calls++; g_gs_l = l; g_gs_n = n; return g_gs_ret; }
TEST(paint_gdi_graystring_default_output) {
    REQUIRE(U(GrayStringA)); SETUPM();
    RECT all = {0, 0, W, H};
    BOOL r = U(GrayStringA)(m.dc, NULL, NULL, (LPARAM)"Gray", 4, 10, 10, 0, 0);
    CHECK(r);
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_clear(&m, WHITE);
    r = U(GrayStringA)(m.dc, NULL, NULL, (LPARAM)"Gray", 0, 10, 10, 0, 0);   /* 0 = strlen */
    CHECK(r);
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_clear(&m, WHITE);
    HBRUSH br = CreateSolidBrush(RGB(0, 128, 0));
    r = U(GrayStringA)(m.dc, br, NULL, (LPARAM)"Gray", 4, 10, 10, 40, 16);
    CHECK(r);
    DeleteObject(br);
    md_done(&m);
}
TEST(paint_gdi_graystring_wide) {
    REQUIRE(U(GrayStringW)); SETUPM();
    RECT all = {0, 0, W, H};
    CHECK(U(GrayStringW)(m.dc, NULL, NULL, (LPARAM)L"Gray", 4, 10, 10, 0, 0));
    CHECK(md_count_not(&m, WHITE, &all) > 0);
    md_done(&m);
}
TEST(paint_gdi_grayString_callback) {
    REQUIRE(U(GrayStringA)); SETUPM();
    g_gs_calls = 0; g_gs_ret = 1;
    BOOL r = U(GrayStringA)(m.dc, NULL, gs_cb, 0xabcd, 77, 10, 10, 30, 14);
    CHECK(r);
    EQ(g_gs_calls, 1);
    CHECK(g_gs_l == 0xabcd);
    EQ(g_gs_n, 77);
    g_gs_calls = 0; g_gs_ret = 0;
    r = U(GrayStringA)(m.dc, NULL, gs_cb, 0xabcd, 77, 10, 10, 30, 14);
    CHECK(r == FALSE);
    EQ(g_gs_calls, 1);
    md_done(&m);
}
TEST(paint_gdi_grayString_invalid) {
    REQUIRE(U(GrayStringA)); SETUPM();
    CHECK(U(GrayStringA)(NULL, NULL, NULL, (LPARAM)"x", 1, 0, 0, 0, 0) == FALSE);
    CHECK(U(GrayStringA)((HDC)(ULONG_PTR)0x6767, NULL, NULL, (LPARAM)"x", 1, 0, 0, 0, 0) == FALSE);
    SetLastError(0);
    BOOL r = U(GrayStringA)(m.dc, NULL, NULL, (LPARAM)"", 0, 0, 0, 0, 0);
    t_note("GrayString(empty) -> %d", r);
    md_done(&m);
}
TEST(paint_gdi_paintdesktop) {
    REQUIRE(U(PaintDesktop)); SETUPM();
    BOOL pd = U(PaintDesktop)(m.dc);
    t_note("PaintDesktop(mem dc) -> %d", pd);
    RECT all = {0, 0, W, H};
    t_note("PaintDesktop changed %d of %d pixels", md_count_not(&m, WHITE, &all), W * H);
    t_note("PaintDesktop(NULL) -> %d", U(PaintDesktop)(NULL));
    t_note("PaintDesktop(bad dc) -> %d", U(PaintDesktop)((HDC)(ULONG_PTR)0x3434));
    md_done(&m);
}

/* ---------------- system colours ---------------- */
TEST(paint_gdi_syscolor_range) {
    REQUIRE(U(GetSysColor));
    for (int i = 0; i <= 30; i++) {
        COLORREF c = U(GetSysColor)(i);
        CHECKF((c & 0xff000000) == 0, "index %d = %08lx", i, c);
    }
    CHECK(U(GetSysColor)(COLOR_WINDOW) == U(GetSysColor)(5));
    CHECK(U(GetSysColor)(COLOR_BTNFACE) == U(GetSysColor)(COLOR_3DFACE));
    CHECK(U(GetSysColor)(COLOR_BTNSHADOW) == U(GetSysColor)(COLOR_3DSHADOW));
    CHECK(U(GetSysColor)(COLOR_BTNHIGHLIGHT) == U(GetSysColor)(COLOR_3DHILIGHT));
    CHECK(U(GetSysColor)(COLOR_BTNHILIGHT) == U(GetSysColor)(COLOR_3DHILIGHT));
    CHECK(U(GetSysColor)(COLOR_BACKGROUND) == U(GetSysColor)(COLOR_DESKTOP));
    CHECK(U(GetSysColor)(COLOR_BACKGROUND) == U(GetSysColor)(1));
}
TEST(paint_gdi_syscolor_out_of_range) {
    REQUIRE(U(GetSysColor));
    static const int bad[] = { -1, -100, 31, 32, 100, 0x7fffffff, (int)0x80000000, 0x10000 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        COLORREF c = U(GetSysColor)(bad[i]);
        CHECKF(c == 0, "index %d -> %06lx", bad[i], c);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}
TEST(paint_gdi_syscolor_stable) {
    REQUIRE(U(GetSysColor));
    for (int i = 0; i <= 30; i++) CHECKF(U(GetSysColor)(i) == U(GetSysColor)(i), "idx %d", i);
}
TEST(paint_gdi_syscolor_default_theme_text) {
    REQUIRE(U(GetSysColor));
    /* the text/background pairs must be distinguishable */
    CHECK(U(GetSysColor)(COLOR_WINDOW) != U(GetSysColor)(COLOR_WINDOWTEXT));
    CHECK(U(GetSysColor)(COLOR_BTNFACE) != U(GetSysColor)(COLOR_BTNTEXT));
    CHECK(U(GetSysColor)(COLOR_HIGHLIGHT) != U(GetSysColor)(COLOR_HIGHLIGHTTEXT));
    CHECK(U(GetSysColor)(COLOR_MENU) != U(GetSysColor)(COLOR_MENUTEXT));
    CHECK(U(GetSysColor)(COLOR_INFOBK) != U(GetSysColor)(COLOR_INFOTEXT));
}
TEST(paint_gdi_sysbrush_matches_colour) {
    REQUIRE(U(GetSysColorBrush)); REQUIRE(U(GetSysColor));
    for (int i = 0; i <= 30; i++) {
        HBRUSH b = U(GetSysColorBrush)(i);
        CHECKF(b != NULL, "index %d", i);
        if (!b) continue;
        LOGBRUSH lb;
        CHECKF(GetObjectA(b, sizeof lb, &lb) == sizeof lb, "GetObject idx %d", i);
        CHECKF(lb.lbStyle == BS_SOLID, "style idx %d = %u", i, lb.lbStyle);
        CHECKF(lb.lbColor == U(GetSysColor)(i), "idx %d brush %06lx colour %06lx", i, lb.lbColor, U(GetSysColor)(i));
    }
}
TEST(paint_gdi_sysbrush_same_handle) {
    REQUIRE(U(GetSysColorBrush));
    for (int i = 0; i <= 30; i++) CHECKF(U(GetSysColorBrush)(i) == U(GetSysColorBrush)(i), "idx %d", i);
    CHECK(U(GetSysColorBrush)(COLOR_BTNFACE) == U(GetSysColorBrush)(COLOR_3DFACE));
    CHECK(GetObjectType(U(GetSysColorBrush)(COLOR_WINDOW)) == OBJ_BRUSH);
}
TEST(paint_gdi_sysbrush_out_of_range) {
    REQUIRE(U(GetSysColorBrush));
    CHECK(U(GetSysColorBrush)(-1) == NULL);
    CHECK(U(GetSysColorBrush)(31) == NULL);
    CHECK(U(GetSysColorBrush)(0x1000) == NULL);
    CHECK(U(GetSysColorBrush)((int)0x80000000) == NULL);
}
TEST(paint_gdi_setsyscolors_roundtrip) {
    REQUIRE(U(SetSysColors)); REQUIRE(U(GetSysColor)); REQUIRE(U(GetSysColorBrush));
    INT idx = COLOR_APPWORKSPACE;
    COLORREF old = U(GetSysColor)(idx);
    COLORREF nw = old ^ 0x010000;
    CHECK(U(SetSysColors)(1, &idx, &nw));
    CHECK(U(GetSysColor)(idx) == nw);
    LOGBRUSH lb; GetObjectA(U(GetSysColorBrush)(idx), sizeof lb, &lb);
    CHECK(lb.lbColor == nw);
    CHECK(U(SetSysColors)(1, &idx, &old));
    CHECK(U(GetSysColor)(idx) == old);
    GetObjectA(U(GetSysColorBrush)(idx), sizeof lb, &lb);
    CHECK(lb.lbColor == old);
}
TEST(paint_gdi_setsyscolors_multiple) {
    REQUIRE(U(SetSysColors)); REQUIRE(U(GetSysColor));
    INT idx[3] = { COLOR_APPWORKSPACE, COLOR_GRADIENTINACTIVECAPTION, COLOR_HOTLIGHT };
    COLORREF old[3], nw[3];
    for (int i = 0; i < 3; i++) { old[i] = U(GetSysColor)(idx[i]); nw[i] = old[i] ^ (0x010000 << i); }
    CHECK(U(SetSysColors)(3, idx, nw));
    for (int i = 0; i < 3; i++) CHECKF(U(GetSysColor)(idx[i]) == nw[i], "idx %d", idx[i]);
    CHECK(U(SetSysColors)(3, idx, old));
    for (int i = 0; i < 3; i++) CHECKF(U(GetSysColor)(idx[i]) == old[i], "restore idx %d", idx[i]);
}
TEST(paint_gdi_setsyscolors_broadcast) {
    REQUIRE(U(SetSysColors)); REQUIRE(U(GetSysColor));
    HWND w = pg_recwindow(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW, 0, 0, 50, 50, NULL);
    CHECK(w != NULL); if (!w) return;
    pg_reset();
    INT idx = COLOR_APPWORKSPACE; COLORREF old = U(GetSysColor)(idx), nw = old ^ 0x010000;
    U(SetSysColors)(1, &idx, &nw);
    t_pump();
    int n1 = pg_r.syscolor;
    U(SetSysColors)(1, &idx, &old);
    t_pump();
    t_note("WM_SYSCOLORCHANGE seen by top-level window: %d after set, %d after restore", n1, pg_r.syscolor);
    CHECK(n1 >= 1);
    CHECK(pg_r.syscolor >= 2);
    CHECK(U(GetSysColor)(idx) == old);
    U(DestroyWindow)(w);
}
TEST(paint_gdi_setsyscolors_invalid) {
    REQUIRE(U(SetSysColors)); REQUIRE(U(GetSysColor));
    INT idx = COLOR_APPWORKSPACE; COLORREF c = U(GetSysColor)(idx);
    SetLastError(0);
    BOOL r = U(SetSysColors)(0, &idx, &c);
    t_note("SetSysColors(0 elements) -> %d err=%lu", r, GetLastError());
    INT bad = 100;
    r = U(SetSysColors)(1, &bad, &c);
    t_note("SetSysColors(index 100) -> %d err=%lu", r, GetLastError());
    CHECK(U(GetSysColor)(COLOR_APPWORKSPACE) == c);
}
TEST(paint_gdi_setsyscolors_same_value) {
    REQUIRE(U(SetSysColors)); REQUIRE(U(GetSysColor));
    INT idx = COLOR_APPWORKSPACE; COLORREF c = U(GetSysColor)(idx);
    CHECK(U(SetSysColors)(1, &idx, &c));
    CHECK(U(GetSysColor)(idx) == c);
}

/* ---------------- default window painting messages ---------------- */
static LRESULT CALLBACK bgproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }
static ATOM reg_bg(const char *name, HBRUSH br) {
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = bgproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = name; wc.hbrBackground = br;
    return U(RegisterClassA)(&wc);
}
TEST(paint_gdi_erasebkgnd_class_brush) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(RegisterClassA)); SETUPM();
    struct { const char *n; HBRUSH br; COLORREF c; } t[] = {
        { "PGBg1", (HBRUSH)(COLOR_WINDOW + 1), 0 }, { "PGBg2", (HBRUSH)(COLOR_BTNFACE + 1), 0 },
        { "PGBg3", (HBRUSH)GetStockObject(BLACK_BRUSH), RGB(0, 0, 0) }, { "PGBg4", (HBRUSH)GetStockObject(WHITE_BRUSH), RGB(255, 255, 255) },
        { "PGBg5", (HBRUSH)(COLOR_HIGHLIGHT + 1), 0 },
    };
    t[0].c = U(GetSysColor)(COLOR_WINDOW); t[1].c = U(GetSysColor)(COLOR_BTNFACE); t[4].c = U(GetSysColor)(COLOR_HIGHLIGHT);
    for (unsigned i = 0; i < 5; i++) {
        CHECKF(reg_bg(t[i].n, t[i].br), "register %s", t[i].n);
        HWND w = U(CreateWindowExA)(0, t[i].n, "x", WS_POPUP, 0, 0, 60, 40, NULL, NULL, GetModuleHandleA(NULL), NULL);
        CHECK(w != NULL);
        if (w) {
            md_clear(&m, RGB(1, 2, 3));
            LRESULT r = U(SendMessageA)(w, WM_ERASEBKGND, (WPARAM)m.dc, 0);
            CHECKF(r != 0, "%s returned %lld", t[i].n, (long long)r);
            CHECKF(P(&m, 5, 5) == t[i].c, "%s pixel %06lx want %06lx", t[i].n, P(&m, 5, 5), t[i].c);
            CHECKF(P(&m, 59, 39) == t[i].c, "%s far pixel %06lx", t[i].n, P(&m, 59, 39));
            if (P(&m, 60, 39) != RGB(1, 2, 3) || P(&m, 59, 40) != RGB(1, 2, 3)) t_note("%s: erase also painted outside the client rect", t[i].n);
            U(DestroyWindow)(w);
        }
        U(UnregisterClassA)(t[i].n, GetModuleHandleA(NULL));
    }
    md_done(&m);
}
TEST(paint_gdi_erasebkgnd_no_class_brush) {
    REQUIRE(U(SendMessageA)); SETUPM();
    CHECK(reg_bg("PGBgNone", NULL));
    HWND w = U(CreateWindowExA)(0, "PGBgNone", "x", WS_POPUP, 0, 0, 60, 40, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(w != NULL);
    if (w) {
        md_clear(&m, RGB(1, 2, 3));
        LRESULT r = U(SendMessageA)(w, WM_ERASEBKGND, (WPARAM)m.dc, 0);
        CHECK(r == 0);
        CHECK(P(&m, 5, 5) == RGB(1, 2, 3));
        U(DestroyWindow)(w);
    }
    U(UnregisterClassA)("PGBgNone", GetModuleHandleA(NULL));
    md_done(&m);
}
TEST(paint_gdi_erasebkgnd_message_params) {
    REQUIRE(U(UpdateWindow));
    HWND w = pg_topwin(300, 300, 100, 80); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.erase_ret = 1;
    U(InvalidateRect)(w, NULL, TRUE);
    U(UpdateWindow)(w);
    EQ(pg_r.erase, 1);
    CHECK(pg_r.erase_wp != 0);
    CHECK(pg_r.erase_dctype == OBJ_DC);
    U(DestroyWindow)(w); pg_pump();
}
TEST(paint_gdi_ncpaint_params) {
    REQUIRE(U(RedrawWindow));
    HWND w = pg_recwindow(WS_OVERLAPPEDWINDOW, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 200, 150, NULL);
    CHECK(w != NULL); if (!w) return;
    U(ShowWindow)(w, SW_SHOWNA); pg_flush(w);
    pg_reset();
    U(RedrawWindow)(w, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
    CHECK(pg_r.ncpaint >= 1);
    t_note("WM_NCPAINT wParam = %llx", (unsigned long long)pg_r.ncpaint_wp);
    CHECK(pg_r.ncpaint_wp == 1 || GetObjectType((HGDIOBJ)pg_r.ncpaint_wp) == OBJ_REGION);
    U(DestroyWindow)(w); pg_pump();
}
TEST(paint_gdi_ncpaint_sendmessage_default) {
    REQUIRE(U(SendMessageA));
    HWND w = pg_recwindow(WS_OVERLAPPEDWINDOW, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 200, 150, NULL);
    CHECK(w != NULL); if (!w) return;
    U(ShowWindow)(w, SW_SHOWNA); pg_flush(w);
    pg_reset();
    LRESULT r = U(SendMessageA)(w, WM_NCPAINT, 1, 0);
    CHECK(r == 0);
    EQ(pg_r.ncpaint, 1);
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));      /* painting the frame does not invalidate the client */
    U(DestroyWindow)(w); pg_pump();
}
TEST(paint_gdi_printclient_sends_paint) {
    REQUIRE(U(SendMessageA)); SETUPM();
    HWND w = pg_recwindow(WS_POPUP, 0, 0, 0, 60, 40, NULL);
    CHECK(w != NULL); if (!w) { md_done(&m); return; }
    pg_reset();
    md_clear(&m, RGB(1, 2, 3));
    U(SendMessageA)(w, WM_PRINTCLIENT, (WPARAM)m.dc, PRF_CLIENT | PRF_ERASEBKGND);
    t_note("WM_PRINTCLIENT(CLIENT|ERASEBKGND): paint=%d (wParam==hdc:%d) erase=%d", pg_r.paint, pg_r.paint_wp == (WPARAM)m.dc, pg_r.erase);
    pg_reset();
    U(SendMessageA)(w, WM_PRINTCLIENT, (WPARAM)m.dc, PRF_CLIENT);
    t_note("WM_PRINTCLIENT(CLIENT): paint=%d erase=%d", pg_r.paint, pg_r.erase);
    CHECK(pg_r.erase == 0);
    U(DestroyWindow)(w);
    md_done(&m);
}
TEST(paint_gdi_wm_paint_default_validates) {
    REQUIRE(U(UpdateWindow));
    HWND w = pg_topwin(300, 300, 100, 80); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.do_paint = 0;       /* leave it to DefWindowProc */
    U(InvalidateRect)(w, NULL, FALSE);
    U(UpdateWindow)(w);
    EQ(pg_r.paint, 1);
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));
    U(DestroyWindow)(w); pg_pump();
}
TEST(paint_gdi_wm_paint_handler_ignoring_repeats) {
    REQUIRE(U(UpdateWindow));
    HWND w = pg_topwin(300, 300, 100, 80); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.do_paint = 2;       /* handler returns without validating */
    U(InvalidateRect)(w, NULL, FALSE);
    U(UpdateWindow)(w);
    EQ(pg_r.paint, 1);
    CHECK(U(GetUpdateRect)(w, NULL, FALSE));     /* still invalid */
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, WM_PAINT, WM_PAINT, PM_NOREMOVE));   /* and WM_PAINT keeps coming */
    U(ValidateRect)(w, NULL);
    pg_r.do_paint = 1;
    U(DestroyWindow)(w); pg_pump();
}
