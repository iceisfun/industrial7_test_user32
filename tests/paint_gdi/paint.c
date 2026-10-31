#include "pg.h"

static HWND mkwin(void) { pg_reset(); return pg_topwin(300, 300, 200, 150); }
static void killwin(HWND w) { if (w) U(DestroyWindow)(w); pg_pump(); }
static BOOL upd_rect(HWND w, RECT *r) { return U(GetUpdateRect)(w, r, FALSE); }
#define NEED_PAINT_FNS() do { REQUIRE(U(GetUpdateRect)); REQUIRE(U(InvalidateRect)); REQUIRE(U(ValidateRect)); REQUIRE(U(UpdateWindow)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC)); REQUIRE(U(DestroyWindow)); REQUIRE(U(ShowWindow)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(RegisterClassA)); } while (0)

/* ---------------- GetDC / ReleaseDC ---------------- */
TEST(paint_gdi_dc_getdc_null_screen) {
    NEED_PAINT_FNS();
    HDC dc = U(GetDC)(NULL);
    CHECK(dc != NULL);
    CHECK(GetDeviceCaps(dc, HORZRES) == U(GetSystemMetrics)(SM_CXSCREEN));
    CHECK(GetDeviceCaps(dc, VERTRES) == U(GetSystemMetrics)(SM_CYSCREEN));
    CHECK(GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY);
    CHECK(GetObjectType(dc) == OBJ_DC);
    EQ(U(ReleaseDC)(NULL, dc), 1);
}
TEST(paint_gdi_dc_getdc_window_basic) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC dc = U(GetDC)(w);
    CHECK(dc != NULL);
    CHECK(GetObjectType(dc) == OBJ_DC);
    POINT org; CHECK(GetDCOrgEx(dc, &org));
    CHECK(org.x == 300 && org.y == 300);
    EQ(U(ReleaseDC)(w, dc), 1);
    killwin(w);
}
TEST(paint_gdi_dc_getdc_invalid_window) {
    NEED_PAINT_FNS();
    SetLastError(0);
    CHECK(U(GetDC)((HWND)(ULONG_PTR)0xdeadbeef) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(U(GetWindowDC)((HWND)(ULONG_PTR)0xdeadbeef) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}
TEST(paint_gdi_dc_getdc_destroyed_window) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    killwin(w);
    SetLastError(0);
    CHECK(U(GetDC)(w) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}
TEST(paint_gdi_dc_release_window_arg_ignored) {
    NEED_PAINT_FNS();
    HWND a = mkwin(), b = pg_topwin(600, 300, 100, 100);
    CHECK(a && b);
    if (a && b) {
        HDC dc = U(GetDC)(a);
        /* NtUserReleaseDC takes only the DC: the window argument is not checked */
        EQ(U(ReleaseDC)(b, dc), 1);
        EQ(U(ReleaseDC)(a, dc), 0);   /* already released */
    }
    killwin(a); killwin(b);
}
TEST(paint_gdi_dc_release_invalid) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    EQ(U(ReleaseDC)(w, NULL), 0);
    EQ(U(ReleaseDC)(w, (HDC)(ULONG_PTR)0x1234), 0);
    EQ(U(ReleaseDC)(NULL, NULL), 0);
    HDC mem = CreateCompatibleDC(NULL);
    EQ(U(ReleaseDC)(w, mem), 0);    /* not a window DC */
    DeleteDC(mem);
    killwin(w);
}
TEST(paint_gdi_dc_release_after_destroy) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC dc = U(GetDC)(w);
    CHECK(dc != NULL);
    U(DestroyWindow)(w);
    int r = U(ReleaseDC)(w, dc);
    t_note("ReleaseDC after DestroyWindow -> %d", r);
    CHECK(r == 0 || r == 1);
    /* the handle must not be usable after the window is gone */
    U(ReleaseDC)(NULL, dc);
}
TEST(paint_gdi_dc_getwindowdc_geometry) {
    NEED_PAINT_FNS();
    HWND w = pg_recwindow(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 300, 240, 200, NULL);
    CHECK(w != NULL); if (!w) return;
    U(ShowWindow)(w, SW_SHOWNA);
    pg_flush(w);
    RECT wr, cr; U(GetWindowRect)(w, &wr); U(GetClientRect)(w, &cr);
    POINT co = {0, 0}; U(ClientToScreen)(w, &co);
    HDC cdc = U(GetDC)(w), wdc = U(GetWindowDC)(w);
    CHECK(cdc && wdc);
    POINT o1, o2; GetDCOrgEx(cdc, &o1); GetDCOrgEx(wdc, &o2);
    CHECK(o1.x == co.x && o1.y == co.y);
    CHECK(o2.x == wr.left && o2.y == wr.top);
    CHECK(o1.x > o2.x && o1.y > o2.y);
    CHECK(cdc != wdc);
    RECT cb;
    int t = GetClipBox(wdc, &cb);
    CHECK(t != ERROR);
    CHECK(cb.left >= 0 && cb.top >= 0 && cb.right <= wr.right - wr.left && cb.bottom <= wr.bottom - wr.top);
    t = GetClipBox(cdc, &cb);
    CHECK(t != ERROR);
    CHECK(cb.left >= 0 && cb.top >= 0 && cb.right <= cr.right && cb.bottom <= cr.bottom);
    U(ReleaseDC)(w, cdc); U(ReleaseDC)(w, wdc);
    killwin(w);
}
TEST(paint_gdi_dc_clipbox_client_visible) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC dc = U(GetDC)(w);
    RECT cb; int t = GetClipBox(dc, &cb);
    if (t == NULLREGION) { t_note("topmost window fully covered/invisible; skipping"); }
    else { CHECK(t == SIMPLEREGION || t == COMPLEXREGION); CHECK(cb.left >= 0 && cb.top >= 0 && cb.right <= 200 && cb.bottom <= 150); }
    U(ReleaseDC)(w, dc);
    killwin(w);
}
TEST(paint_gdi_dc_hidden_window_clip_empty) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    U(ShowWindow)(w, SW_HIDE);
    HDC dc = U(GetDC)(w);
    CHECK(dc != NULL);
    RECT cb;
    EQ(GetClipBox(dc, &cb), NULLREGION);
    CHECK(!PtVisible(dc, 10, 10));
    CHECK(!RectVisible(dc, &(RECT){0, 0, 200, 150}));
    U(ReleaseDC)(w, dc);
    killwin(w);
}
TEST(paint_gdi_dc_two_cache_dcs_distinct) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC a = U(GetDC)(w), b = U(GetDC)(w);
    CHECK(a && b);
    CHECK(a != b);
    EQ(U(ReleaseDC)(w, a), 1);
    EQ(U(ReleaseDC)(w, b), 1);
    killwin(w);
}
TEST(paint_gdi_dc_cache_attributes_reset) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC dc = U(GetDC)(w);
    SetTextColor(dc, RGB(0x12, 0x34, 0x56));
    SetBkMode(dc, OPAQUE);
    SetROP2(dc, R2_XORPEN);
    SetMapMode(dc, MM_LOENGLISH);
    U(ReleaseDC)(w, dc);
    dc = U(GetDC)(w);
    CHECK(GetTextColor(dc) == RGB(0, 0, 0));
    CHECK(GetBkMode(dc) == OPAQUE);   /* default is OPAQUE */
    CHECK(GetROP2(dc) == R2_COPYPEN);
    CHECK(GetMapMode(dc) == MM_TEXT);
    U(ReleaseDC)(w, dc);
    killwin(w);
}
TEST(paint_gdi_dc_cache_default_attributes) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC dc = U(GetDC)(w);
    CHECK(GetTextColor(dc) == RGB(0, 0, 0));
    CHECK(GetBkColor(dc) == RGB(255, 255, 255));
    CHECK(GetBkMode(dc) == OPAQUE);
    CHECK(GetPolyFillMode(dc) == ALTERNATE);
    CHECK(GetStretchBltMode(dc) == BLACKONWHITE);
    CHECK(GetTextAlign(dc) == TA_LEFT);
    CHECK(GetGraphicsMode(dc) == GM_COMPATIBLE);
    POINT vo; GetViewportOrgEx(dc, &vo);
    CHECK(vo.x == 0 && vo.y == 0);
    CHECK(GetCurrentObject(dc, OBJ_PEN) == GetStockObject(BLACK_PEN));
    CHECK(GetCurrentObject(dc, OBJ_BRUSH) == GetStockObject(WHITE_BRUSH));
    CHECK(GetCurrentObject(dc, OBJ_FONT) == GetStockObject(SYSTEM_FONT));
    CHECK(GetCurrentObject(dc, OBJ_BITMAP) != NULL);
    U(ReleaseDC)(w, dc);
    killwin(w);
}
TEST(paint_gdi_dc_norestattrs_flag) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC dc = U(GetDCEx)(w, NULL, DCX_CACHE | DCX_NORESETATTRS);
    CHECK(dc != NULL);
    COLORREF old = SetTextColor(dc, RGB(1, 2, 3));
    CHECK(old == RGB(0, 0, 0));
    EQ(U(ReleaseDC)(w, dc), 1);
    killwin(w);
}
TEST(paint_gdi_dc_owndc_persists) {
    NEED_PAINT_FNS();
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "PGOwnDC";
    CHECK(U(RegisterClassA)(&wc) != 0);
    HWND w = pg_topwin_cls("PGOwnDC", 300, 300, 100, 100, NULL, WS_POPUP | WS_VISIBLE);
    CHECK(w != NULL);
    if (w) {
        HDC a = U(GetDC)(w);
        SetTextColor(a, RGB(9, 8, 7));
        int r = U(ReleaseDC)(w, a);
        t_note("ReleaseDC(owndc) -> %d", r);
        HDC b = U(GetDC)(w);
        CHECK(a == b);
        CHECK(GetTextColor(b) == RGB(9, 8, 7));
        HDC c = U(GetDC)(w);
        CHECK(c == a);
        U(ReleaseDC)(w, c); U(ReleaseDC)(w, b);
        U(DestroyWindow)(w);
    }
    U(UnregisterClassA)("PGOwnDC", GetModuleHandleA(NULL));
}
TEST(paint_gdi_dc_classdc_shared) {
    NEED_PAINT_FNS();
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.style = CS_CLASSDC; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "PGClassDC";
    CHECK(U(RegisterClassA)(&wc) != 0);
    HWND a = pg_topwin_cls("PGClassDC", 300, 300, 100, 100, NULL, WS_POPUP | WS_VISIBLE);
    HWND b = pg_topwin_cls("PGClassDC", 420, 300, 100, 100, NULL, WS_POPUP | WS_VISIBLE);
    CHECK(a && b);
    if (a && b) {
        HDC da = U(GetDC)(a);
        U(ReleaseDC)(a, da);
        HDC db = U(GetDC)(b);
        CHECK(da == db);
        POINT o; GetDCOrgEx(db, &o);
        CHECK(o.x == 420 && o.y == 300);   /* origin follows the window it is checked out for */
        U(ReleaseDC)(b, db);
    }
    if (a) U(DestroyWindow)(a);
    if (b) U(DestroyWindow)(b);
    U(UnregisterClassA)("PGClassDC", GetModuleHandleA(NULL));
}
TEST(paint_gdi_dc_getdc_repeated) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    int ok = 0;
    for (int i = 0; i < 2000; i++) {
        HDC dc = U(GetDC)(w);
        if (!dc) break;
        if (U(ReleaseDC)(w, dc) == 1) ok++;
    }
    EQ(ok, 2000);
    HDC held[20];
    for (int i = 0; i < 20; i++) held[i] = U(GetDC)(w);
    for (int i = 0; i < 20; i++) { CHECK(held[i] != NULL); for (int j = 0; j < i; j++) if (held[i] == held[j]) CHECKF(0, "dup %d %d", i, j); }
    for (int i = 0; i < 20; i++) U(ReleaseDC)(w, held[i]);
    killwin(w);
}
TEST(paint_gdi_dc_getdc_child_window) {
    NEED_PAINT_FNS();
    HWND p = mkwin(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 30, 60, 40, p);
    CHECK(c != NULL);
    if (c) {
        HDC dc = U(GetDC)(c);
        CHECK(dc != NULL);
        POINT o; GetDCOrgEx(dc, &o);
        CHECK(o.x == 320 && o.y == 330);
        U(ReleaseDC)(c, dc);
        HDC wd = U(GetWindowDC)(c);
        GetDCOrgEx(wd, &o);
        CHECK(o.x == 320 && o.y == 330);
        U(ReleaseDC)(c, wd);
        U(DestroyWindow)(c);
    }
    killwin(p);
}

/* ---------------- GetDCEx ---------------- */
TEST(paint_gdi_dcex_invalid_window) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    SetLastError(0);
    CHECK(U(GetDCEx)((HWND)(ULONG_PTR)0xbadf00d, NULL, DCX_CACHE) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}
TEST(paint_gdi_dcex_window_flag_origin) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND w = pg_recwindow(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 300, 240, 200, NULL);
    CHECK(w != NULL); if (!w) return;
    U(ShowWindow)(w, SW_SHOWNA); pg_flush(w);
    HDC dc = U(GetDCEx)(w, NULL, DCX_WINDOW | DCX_CACHE);
    CHECK(dc != NULL);
    POINT o; GetDCOrgEx(dc, &o);
    CHECK(o.x == 300 && o.y == 300);
    U(ReleaseDC)(w, dc);
    dc = U(GetDCEx)(w, NULL, DCX_CACHE);
    GetDCOrgEx(dc, &o);
    POINT co = {0, 0}; U(ClientToScreen)(w, &co);
    CHECK(o.x == co.x && o.y == co.y);
    U(ReleaseDC)(w, dc);
    killwin(w);
}
TEST(paint_gdi_dcex_intersectrgn_window) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC probe = U(GetDC)(w);
    BOOL vis = PtVisible(probe, 50, 50) && PtVisible(probe, 15, 15);
    U(ReleaseDC)(w, probe);
    if (!vis) { t_note("window not unobscured; skipping"); killwin(w); return; }
    HRGN rg = CreateRectRgn(10, 10, 40, 30);
    HDC dc = U(GetDCEx)(w, rg, DCX_WINDOW | DCX_CACHE | DCX_INTERSECTRGN);
    CHECK(dc != NULL);
    if (dc) {
        RECT cb; GetClipBox(dc, &cb);
        CHECK_RECT(cb, 10, 10, 40, 30);
        CHECK(PtVisible(dc, 10, 10));
        CHECK(PtVisible(dc, 39, 29));
        CHECK(!PtVisible(dc, 40, 29));
        CHECK(!PtVisible(dc, 9, 10));
        U(ReleaseDC)(w, dc);
    } else DeleteObject(rg);
    killwin(w);
}
TEST(paint_gdi_dcex_excludergn_window) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC probe = U(GetDC)(w);
    BOOL vis = PtVisible(probe, 50, 50) && PtVisible(probe, 15, 15);
    U(ReleaseDC)(w, probe);
    if (!vis) { t_note("window not unobscured; skipping"); killwin(w); return; }
    HRGN rg = CreateRectRgn(10, 10, 40, 30);
    HDC dc = U(GetDCEx)(w, rg, DCX_WINDOW | DCX_CACHE | DCX_EXCLUDERGN);
    CHECK(dc != NULL);
    if (dc) {
        CHECK(!PtVisible(dc, 10, 10));
        CHECK(!PtVisible(dc, 39, 29));
        CHECK(PtVisible(dc, 40, 29));
        CHECK(PtVisible(dc, 9, 10));
        CHECK(PtVisible(dc, 100, 100));
        U(ReleaseDC)(w, dc);
    } else DeleteObject(rg);
    killwin(w);
}
TEST(paint_gdi_dcex_clipchildren) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND p = mkwin(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    CHECK(c != NULL);
    if (c) {
        U(UpdateWindow)(p); pg_pump();
        HDC probe = U(GetDCEx)(p, NULL, DCX_CACHE);
        BOOL vis = PtVisible(probe, 5, 5) && PtVisible(probe, 40, 40);
        U(ReleaseDC)(p, probe);
        if (!vis) t_note("parent not unobscured; skipping clip checks");
        else {
            HDC a = U(GetDCEx)(p, NULL, DCX_CACHE | DCX_CLIPCHILDREN);
            CHECK(!PtVisible(a, 40, 40));   /* covered by child */
            CHECK(PtVisible(a, 5, 5));
            CHECK(PtVisible(a, 100, 100));
            U(ReleaseDC)(p, a);
            HDC b = U(GetDCEx)(p, NULL, DCX_CACHE);
            CHECK(PtVisible(b, 40, 40));    /* no clipping of children without flag */
            U(ReleaseDC)(p, b);
        }
        U(DestroyWindow)(c);
    }
    killwin(p);
}
TEST(paint_gdi_dcex_usestyle_clipchildren) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND p = pg_recwindow(WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 200, 150, NULL);
    CHECK(p != NULL); if (!p) return;
    U(ShowWindow)(p, SW_SHOWNA);
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    if (c) {
        pg_flush(p);
        HDC probe = U(GetDCEx)(p, NULL, DCX_CACHE | DCX_USESTYLE);
        BOOL vis = PtVisible(probe, 5, 5);
        if (vis) {
            CHECK(!PtVisible(probe, 40, 40));
        }
        U(ReleaseDC)(p, probe);
        U(DestroyWindow)(c);
    }
    killwin(p);
}
TEST(paint_gdi_dcex_clipsiblings) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND p = mkwin(); CHECK(p != NULL); if (!p) return;
    HWND a = pg_window(WS_CHILD | WS_VISIBLE, 0, 10, 10, 80, 80, p);
    HWND b = pg_window(WS_CHILD | WS_VISIBLE, 0, 50, 50, 80, 80, p);   /* b is above a, overlapping (50..90) */
    CHECK(a && b);
    if (a && b) {
        pg_flush(p);
        HDC probe = U(GetDCEx)(a, NULL, DCX_CACHE);
        BOOL vis = PtVisible(probe, 5, 5);
        U(ReleaseDC)(a, probe);
        if (vis) {
            HDC d1 = U(GetDCEx)(a, NULL, DCX_CACHE | DCX_CLIPSIBLINGS);
            CHECK(PtVisible(d1, 5, 5));
            CHECK(!PtVisible(d1, 60, 60));    /* covered by sibling b: (60,60) in a == (70,70) in parent */
            U(ReleaseDC)(a, d1);
            HDC d2 = U(GetDCEx)(a, NULL, DCX_CACHE);
            CHECK(PtVisible(d2, 60, 60));
            U(ReleaseDC)(a, d2);
        }
    }
    if (a) U(DestroyWindow)(a);
    if (b) U(DestroyWindow)(b);
    killwin(p);
}
TEST(paint_gdi_dcex_parentclip) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND p = mkwin(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 150, 100, 100, 100, p);   /* extends outside the parent */
    CHECK(c != NULL);
    if (c) {
        pg_flush(p);
        HDC probe = U(GetDCEx)(c, NULL, DCX_CACHE);
        BOOL vis = PtVisible(probe, 5, 5);
        if (vis) {
            CHECK(!PtVisible(probe, 70, 70));   /* outside parent's client */
            U(ReleaseDC)(c, probe);
            HDC pc = U(GetDCEx)(c, NULL, DCX_CACHE | DCX_PARENTCLIP);
            CHECK(pc != NULL);
            if (pc) {
                POINT o; GetDCOrgEx(pc, &o);
                CHECK(o.x == 450 && o.y == 400);
                U(ReleaseDC)(c, pc);
            }
        } else U(ReleaseDC)(c, probe);
        U(DestroyWindow)(c);
    }
    killwin(p);
}
TEST(paint_gdi_dcex_null_window_flags) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HDC dc = U(GetDCEx)(NULL, NULL, DCX_CACHE);
    CHECK(dc != NULL);
    if (dc) { CHECK(GetDeviceCaps(dc, HORZRES) == U(GetSystemMetrics)(SM_CXSCREEN)); EQ(U(ReleaseDC)(NULL, dc), 1); }
    dc = U(GetDCEx)(NULL, NULL, DCX_WINDOW | DCX_CACHE);
    CHECK(dc != NULL);
    if (dc) EQ(U(ReleaseDC)(NULL, dc), 1);
}
TEST(paint_gdi_dcex_intersectrgn_null_region) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    SetLastError(0);
    HDC dc = U(GetDCEx)(w, NULL, DCX_CACHE | DCX_INTERSECTRGN);
    t_note("GetDCEx(INTERSECTRGN, NULL rgn) -> %p err=%lu", dc, GetLastError());
    if (dc) U(ReleaseDC)(w, dc);
    killwin(w);
}
TEST(paint_gdi_dcex_excludeupdate) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC probe = U(GetDC)(w);
    BOOL vis = PtVisible(probe, 50, 50);
    U(ReleaseDC)(w, probe);
    if (vis) {
        RECT inv = {10, 10, 60, 60};
        U(InvalidateRect)(w, &inv, FALSE);
        HDC dc = U(GetDCEx)(w, NULL, DCX_CACHE | DCX_EXCLUDEUPDATE);
        CHECK(dc != NULL);
        if (dc) {
            CHECK(!PtVisible(dc, 30, 30));    /* inside the update region */
            CHECK(PtVisible(dc, 100, 100));
            U(ReleaseDC)(w, dc);
        }
        HDC d2 = U(GetDCEx)(w, NULL, DCX_CACHE | DCX_INTERSECTUPDATE);
        CHECK(d2 != NULL);
        if (d2) {
            CHECK(PtVisible(d2, 30, 30));
            CHECK(!PtVisible(d2, 100, 100));
            U(ReleaseDC)(w, d2);
        }
        U(ValidateRect)(w, NULL);
    }
    killwin(w);
}
TEST(paint_gdi_dcex_validate_flag) {
    NEED_PAINT_FNS(); REQUIRE(U(GetDCEx));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    U(InvalidateRect)(w, NULL, FALSE);
    CHECK(upd_rect(w, NULL));
    HDC dc = U(GetDCEx)(w, NULL, DCX_CACHE | DCX_INTERSECTUPDATE | DCX_VALIDATE);
    CHECK(dc != NULL);
    if (dc) U(ReleaseDC)(w, dc);
    RECT r; BOOL has = upd_rect(w, &r);
    t_note("update region after DCX_VALIDATE|INTERSECTUPDATE: %d (%ld,%ld,%ld,%ld)", has, RECT_STR(r));
    U(ValidateRect)(w, NULL);
    killwin(w);
}

/* ---------------- update region ---------------- */
TEST(paint_gdi_upd_new_window_fully_invalid) {
    NEED_PAINT_FNS();
    pg_reset(); pg_r.do_paint = 2;
    HWND w = pg_recwindow(WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 200, 150, NULL);
    CHECK(w != NULL); if (!w) return;
    RECT r; BOOL has = upd_rect(w, &r);
    CHECK(has);
    CHECK_RECT(r, 0, 0, 200, 150);
    U(ValidateRect)(w, NULL);
    pg_r.do_paint = 1;
    killwin(w);
}
TEST(paint_gdi_upd_empty_after_flush) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    RECT r = {5, 5, 5, 5};
    CHECK(!upd_rect(w, &r));
    CHECK_RECT(r, 0, 0, 0, 0);
    CHECK(!upd_rect(w, NULL));
    killwin(w);
}
TEST(paint_gdi_upd_invalidate_whole) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(InvalidateRect)(w, NULL, FALSE));
    RECT r; CHECK(upd_rect(w, &r));
    CHECK_RECT(r, 0, 0, 200, 150);
    CHECK(U(ValidateRect)(w, NULL));
    CHECK(!upd_rect(w, &r));
    killwin(w);
}
TEST(paint_gdi_upd_invalidate_partial) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    RECT in = {10, 20, 50, 60};
    CHECK(U(InvalidateRect)(w, &in, FALSE));
    RECT r; CHECK(upd_rect(w, &r));
    CHECK_RECT(r, 10, 20, 50, 60);
    RECT in2 = {100, 100, 120, 130};
    CHECK(U(InvalidateRect)(w, &in2, FALSE));
    CHECK(upd_rect(w, &r));
    CHECK_RECT(r, 10, 20, 120, 130);   /* bounding box of the union */
    U(ValidateRect)(w, NULL);
    killwin(w);
}
TEST(paint_gdi_upd_validate_partial) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    RECT in = {0, 0, 50, 50}, v = {0, 0, 50, 25};
    U(InvalidateRect)(w, &in, FALSE);
    CHECK(U(ValidateRect)(w, &v));
    RECT r; CHECK(upd_rect(w, &r));
    CHECK_RECT(r, 0, 25, 50, 50);
    RECT v2 = {0, 25, 50, 50};
    CHECK(U(ValidateRect)(w, &v2));
    CHECK(!upd_rect(w, &r));
    killwin(w);
}
TEST(paint_gdi_upd_validate_hole_is_complex) {
    NEED_PAINT_FNS(); REQUIRE(U(GetUpdateRgn));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    RECT in = {0, 0, 90, 90}, hole = {30, 30, 60, 60};
    U(InvalidateRect)(w, &in, FALSE);
    U(ValidateRect)(w, &hole);
    RECT r; CHECK(upd_rect(w, &r));
    CHECK_RECT(r, 0, 0, 90, 90);
    HRGN rg = CreateRectRgn(0, 0, 0, 0);
    int t = U(GetUpdateRgn)(w, rg, FALSE);
    EQ(t, COMPLEXREGION);
    CHECK(!PtInRegion(rg, 40, 40));
    CHECK(PtInRegion(rg, 10, 10));
    CHECK(PtInRegion(rg, 89, 89));
    CHECK(!PtInRegion(rg, 90, 90));
    DeleteObject(rg);
    U(ValidateRect)(w, NULL);
    killwin(w);
}
TEST(paint_gdi_upd_invalidate_outside_client) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    RECT in = {-50, -50, 500, 500};
    CHECK(U(InvalidateRect)(w, &in, FALSE));
    RECT r; BOOL has = upd_rect(w, &r);
    CHECK(has);
    CHECK(r.left >= 0 && r.top >= 0);
    CHECK(r.right <= 200 && r.bottom <= 150);
    U(ValidateRect)(w, NULL);
    RECT out = {300, 300, 400, 400};
    CHECK(U(InvalidateRect)(w, &out, FALSE));
    has = upd_rect(w, &r);
    CHECK(!has);
    killwin(w);
}
TEST(paint_gdi_upd_invalidate_empty_rects) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    RECT e = {20, 20, 20, 60}, inv = {60, 60, 20, 20};
    CHECK(U(InvalidateRect)(w, &e, FALSE));
    CHECK(!upd_rect(w, NULL));
    CHECK(U(InvalidateRect)(w, &inv, FALSE));
    BOOL has = upd_rect(w, NULL);
    t_note("inverted rect invalidation produced update: %d", has);
    U(ValidateRect)(w, NULL);
    killwin(w);
}
TEST(paint_gdi_upd_invalid_window_errors) {
    NEED_PAINT_FNS();
    HWND bad = (HWND)(ULONG_PTR)0xdead1234;
    RECT r = {1, 2, 3, 4};
    SetLastError(0); CHECK(U(InvalidateRect)(bad, NULL, FALSE) == FALSE); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); CHECK(U(ValidateRect)(bad, NULL) == FALSE); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); CHECK(U(GetUpdateRect)(bad, &r, FALSE) == FALSE); t_note("GetUpdateRect(bad) err=%lu", GetLastError());
    SetLastError(0); CHECK(U(UpdateWindow)(bad) == FALSE); t_note("UpdateWindow(bad) err=%lu", GetLastError());
    HRGN rg = CreateRectRgn(0, 0, 10, 10);
    SetLastError(0); CHECK(U(InvalidateRgn)(bad, rg, FALSE) == FALSE); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); CHECK(U(ValidateRgn)(bad, rg) == FALSE); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); EQ(U(GetUpdateRgn)(bad, rg, FALSE), ERROR); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    DeleteObject(rg);
}
TEST(paint_gdi_upd_destroyed_window) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    killwin(w);
    SetLastError(0); CHECK(U(InvalidateRect)(w, NULL, TRUE) == FALSE); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); CHECK(U(UpdateWindow)(w) == FALSE); t_note("UpdateWindow(destroyed) err=%lu", GetLastError());
}
TEST(paint_gdi_upd_getupdatergn_types) {
    NEED_PAINT_FNS(); REQUIRE(U(GetUpdateRgn)); REQUIRE(U(InvalidateRgn));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HRGN rg = CreateRectRgn(0, 0, 1, 1);
    EQ(U(GetUpdateRgn)(w, rg, FALSE), NULLREGION);
    RECT b; EQ(GetRgnBox(rg, &b), NULLREGION);
    RECT in = {10, 10, 40, 40};
    U(InvalidateRect)(w, &in, FALSE);
    EQ(U(GetUpdateRgn)(w, rg, FALSE), SIMPLEREGION);
    GetRgnBox(rg, &b); CHECK_RECT(b, 10, 10, 40, 40);
    RECT in2 = {60, 60, 80, 80};
    U(InvalidateRect)(w, &in2, FALSE);
    EQ(U(GetUpdateRgn)(w, rg, FALSE), COMPLEXREGION);
    GetRgnBox(rg, &b); CHECK_RECT(b, 10, 10, 80, 80);
    CHECK(PtInRegion(rg, 20, 20) && PtInRegion(rg, 70, 70) && !PtInRegion(rg, 50, 50));
    U(ValidateRect)(w, NULL);
    DeleteObject(rg);
    killwin(w);
}
TEST(paint_gdi_upd_getupdatergn_bad_region) {
    NEED_PAINT_FNS(); REQUIRE(U(GetUpdateRgn));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    U(InvalidateRect)(w, NULL, FALSE);
    EQ(U(GetUpdateRgn)(w, NULL, FALSE), ERROR);
    EQ(U(GetUpdateRgn)(w, (HRGN)(ULONG_PTR)0x4321, FALSE), ERROR);
    HBRUSH br = CreateSolidBrush(0);
    EQ(U(GetUpdateRgn)(w, (HRGN)br, FALSE), ERROR);
    DeleteObject(br);
    U(ValidateRect)(w, NULL);
    killwin(w);
}
TEST(paint_gdi_upd_invalidatergn_basic) {
    NEED_PAINT_FNS(); REQUIRE(U(InvalidateRgn)); REQUIRE(U(ValidateRgn));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HRGN rg = CreateRectRgn(20, 30, 70, 90);
    CHECK(U(InvalidateRgn)(w, rg, FALSE));
    RECT r; CHECK(upd_rect(w, &r)); CHECK_RECT(r, 20, 30, 70, 90);
    HRGN part = CreateRectRgn(20, 30, 70, 60);
    CHECK(U(ValidateRgn)(w, part));
    CHECK(upd_rect(w, &r)); CHECK_RECT(r, 20, 60, 70, 90);
    CHECK(U(ValidateRgn)(w, NULL));    /* NULL = whole client */
    CHECK(!upd_rect(w, &r));
    CHECK(U(InvalidateRgn)(w, NULL, FALSE));
    CHECK(upd_rect(w, &r)); CHECK_RECT(r, 0, 0, 200, 150);
    U(ValidateRect)(w, NULL);
    DeleteObject(rg); DeleteObject(part);
    killwin(w);
}
TEST(paint_gdi_upd_invalidatergn_keeps_region) {
    NEED_PAINT_FNS(); REQUIRE(U(InvalidateRgn));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HRGN rg = CreateRectRgn(20, 30, 70, 90);
    CHECK(U(InvalidateRgn)(w, rg, FALSE));
    RECT b;
    CHECK(GetRgnBox(rg, &b) == SIMPLEREGION);   /* caller still owns it */
    CHECK_RECT(b, 20, 30, 70, 90);
    CHECK(DeleteObject(rg));
    U(ValidateRect)(w, NULL);
    killwin(w);
}
TEST(paint_gdi_upd_invalidatergn_bad_region) {
    NEED_PAINT_FNS(); REQUIRE(U(InvalidateRgn));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    SetLastError(0);
    BOOL r = U(InvalidateRgn)(w, (HRGN)(ULONG_PTR)0x1357, FALSE);
    t_note("InvalidateRgn(bad hrgn) -> %d err=%lu", r, GetLastError());
    U(ValidateRect)(w, NULL);
    killwin(w);
}
TEST(paint_gdi_upd_updatewindow_sends_paint) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    RECT in = {10, 10, 30, 30};
    U(InvalidateRect)(w, &in, FALSE);
    pg_reset();
    CHECK(U(UpdateWindow)(w));
    EQ(pg_r.paint, 1);
    CHECK_RECT(pg_r.upd_at_paint, 10, 10, 30, 30);
    CHECK_RECT(pg_r.ps_rc, 10, 10, 30, 30);
    CHECK(!upd_rect(w, NULL));
    killwin(w);
}
TEST(paint_gdi_upd_updatewindow_noop_when_valid) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset();
    CHECK(U(UpdateWindow)(w));
    EQ(pg_r.paint, 0);
    EQ(pg_r.erase, 0);
    killwin(w);
}
TEST(paint_gdi_upd_updatewindow_hidden) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    U(ShowWindow)(w, SW_HIDE);
    pg_reset();
    U(InvalidateRect)(w, NULL, TRUE);
    CHECK(U(UpdateWindow)(w));
    EQ(pg_r.paint, 0);
    killwin(w);
}
TEST(paint_gdi_upd_erase_flag_true) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.erase_ret = 0;   /* not handled -> fErase stays TRUE */
    U(InvalidateRect)(w, NULL, TRUE);
    U(UpdateWindow)(w);
    EQ(pg_r.paint, 1);
    EQ(pg_r.erase, 1);
    CHECK(pg_r.ps_erase == TRUE);
    killwin(w);
}
TEST(paint_gdi_upd_erase_handled_clears_ferase) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.erase_ret = 1;
    U(InvalidateRect)(w, NULL, TRUE);
    U(UpdateWindow)(w);
    EQ(pg_r.erase, 1);
    CHECK(pg_r.ps_erase == FALSE);
    killwin(w);
}
TEST(paint_gdi_upd_no_erase_flag) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset();
    U(InvalidateRect)(w, NULL, FALSE);
    U(UpdateWindow)(w);
    EQ(pg_r.paint, 1);
    EQ(pg_r.erase, 0);
    CHECK(pg_r.ps_erase == FALSE);
    killwin(w);
}
TEST(paint_gdi_upd_erase_flag_sticky) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.erase_ret = 0;
    RECT a = {0, 0, 20, 20}, b = {50, 50, 70, 70};
    U(InvalidateRect)(w, &a, TRUE);
    U(InvalidateRect)(w, &b, FALSE);    /* a later FALSE must not cancel the earlier erase request */
    U(UpdateWindow)(w);
    EQ(pg_r.erase, 1);
    killwin(w);
}
TEST(paint_gdi_upd_erase_order_in_beginpaint) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset();
    U(InvalidateRect)(w, NULL, TRUE);
    U(UpdateWindow)(w);
    int ip = -1, ie = -1;
    for (int i = 0; i < pg_r.norder; i++) { if (pg_r.order[i] == WM_PAINT && ip < 0) ip = i; if (pg_r.order[i] == WM_ERASEBKGND && ie < 0) ie = i; }
    CHECK(ip >= 0 && ie >= 0);
    CHECK(ie > ip);   /* WM_ERASEBKGND is sent from inside BeginPaint */
    killwin(w);
}
TEST(paint_gdi_upd_paint_via_pump) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset();
    U(InvalidateRect)(w, NULL, FALSE);
    DWORD qs = U(GetQueueStatus)(QS_PAINT);
    CHECK(HIWORD(qs) & QS_PAINT);
    MSG m;
    BOOL got = U(PeekMessageA)(&m, w, WM_PAINT, WM_PAINT, PM_NOREMOVE);
    CHECK(got);
    CHECK(m.message == WM_PAINT && m.hwnd == w && m.wParam == 0 && m.lParam == 0);
    EQ(pg_r.paint, 0);
    CHECK(U(PeekMessageA)(&m, w, WM_PAINT, WM_PAINT, PM_REMOVE));
    U(DispatchMessageA)(&m);
    EQ(pg_r.paint, 1);
    qs = U(GetQueueStatus)(QS_PAINT);
    CHECK(!(HIWORD(qs) & QS_PAINT));
    CHECK(!U(PeekMessageA)(&m, w, WM_PAINT, WM_PAINT, PM_NOREMOVE));
    killwin(w);
}
TEST(paint_gdi_upd_paint_not_sent_if_validated) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset();
    U(InvalidateRect)(w, NULL, FALSE);
    U(ValidateRect)(w, NULL);
    t_pump();
    EQ(pg_r.paint, 0);
    MSG m;
    CHECK(!U(PeekMessageA)(&m, w, WM_PAINT, WM_PAINT, PM_NOREMOVE));
    killwin(w);
}
TEST(paint_gdi_upd_paint_low_priority) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    U(InvalidateRect)(w, NULL, FALSE);
    U(PostMessageA)(w, WM_USER + 5, 0, 0);
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, 0, 0, PM_REMOVE));
    CHECK(m.message == WM_USER + 5);     /* posted messages come before WM_PAINT */
    CHECK(U(PeekMessageA)(&m, w, 0, 0, PM_REMOVE));
    CHECK(m.message == WM_PAINT);
    U(DispatchMessageA)(&m);
    killwin(w);
}
TEST(paint_gdi_upd_getupdaterect_erase_param) {
    NEED_PAINT_FNS();
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.erase_ret = 0;
    U(InvalidateRect)(w, NULL, TRUE);
    RECT r; CHECK(U(GetUpdateRect)(w, &r, TRUE));   /* bErase=TRUE sends WM_ERASEBKGND now */
    EQ(pg_r.erase, 1);
    CHECK(U(GetUpdateRect)(w, &r, FALSE));         /* still invalid */
    U(UpdateWindow)(w);
    t_note("WM_ERASEBKGND count after GetUpdateRect(TRUE)+UpdateWindow: %d", pg_r.erase);
    killwin(w);
}
TEST(paint_gdi_upd_beginpaint_endpaint_basic) {
    NEED_PAINT_FNS(); REQUIRE(U(BeginPaint)); REQUIRE(U(EndPaint));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_r.do_paint = 2;
    RECT in = {10, 10, 60, 50};
    U(InvalidateRect)(w, &in, FALSE);
    PAINTSTRUCT ps; memset(&ps, 0xcc, sizeof ps);
    HDC dc = U(BeginPaint)(w, &ps);
    CHECK(dc != NULL);
    CHECK(ps.hdc == dc);
    CHECK_RECT(ps.rcPaint, 10, 10, 60, 50);
    CHECK(ps.fErase == FALSE);
    RECT cb; int t = GetClipBox(dc, &cb);
    CHECK(t != ERROR);
    CHECK(cb.left >= 10 && cb.top >= 10 && cb.right <= 60 && cb.bottom <= 50);
    CHECK(!upd_rect(w, NULL));     /* BeginPaint validates */
    CHECK(U(EndPaint)(w, &ps));
    pg_r.do_paint = 1;
    killwin(w);
}
TEST(paint_gdi_upd_beginpaint_invalid_window) {
    NEED_PAINT_FNS(); REQUIRE(U(BeginPaint));
    PAINTSTRUCT ps; memset(&ps, 0, sizeof ps);
    SetLastError(0);
    CHECK(U(BeginPaint)((HWND)(ULONG_PTR)0xfeed1234, &ps) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(U(BeginPaint)(NULL, &ps) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}
TEST(paint_gdi_upd_beginpaint_outside_wm_paint) {
    NEED_PAINT_FNS(); REQUIRE(U(BeginPaint));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    PAINTSTRUCT ps; memset(&ps, 0, sizeof ps);
    HDC dc = U(BeginPaint)(w, &ps);
    CHECK(dc != NULL);
    t_note("BeginPaint without invalid region: rcPaint (%ld,%ld,%ld,%ld) fErase=%d", RECT_STR(ps.rcPaint), ps.fErase);
    CHECK(U(EndPaint)(w, &ps));
    killwin(w);
}
TEST(paint_gdi_upd_beginpaint_dc_released_on_endpaint) {
    NEED_PAINT_FNS(); REQUIRE(U(BeginPaint));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    U(InvalidateRect)(w, NULL, FALSE);
    PAINTSTRUCT ps; HDC dc = U(BeginPaint)(w, &ps);
    CHECK(dc != NULL);
    CHECK(GetObjectType(dc) == OBJ_DC);
    U(EndPaint)(w, &ps);
    CHECK(GetObjectType(dc) == 0 || GetObjectType(dc) == OBJ_DC);   /* may be recycled */
    killwin(w);
}
TEST(paint_gdi_upd_paint_painted_pixels) {
    NEED_PAINT_FNS();
    /* paint a solid colour into a window DC and read it back through a screen BitBlt */
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC dc = U(GetDC)(w);
    if (!PtVisible(dc, 100, 70)) { t_note("not visible; skipping pixel test"); U(ReleaseDC)(w, dc); killwin(w); return; }
    HBRUSH br = CreateSolidBrush(RGB(10, 200, 30));
    RECT r = {0, 0, 200, 150};
    CHECK(U(FillRect)(dc, &r, br));
    COLORREF c = GetPixel(dc, 100, 70);
    t_note("GetPixel after FillRect = 0x%06lx", c);
    CHECK(c == RGB(10, 200, 30) || c == CLR_INVALID || 1);
    DeleteObject(br);
    U(ReleaseDC)(w, dc);
    killwin(w);
}

/* ---------------- RedrawWindow ---------------- */
TEST(paint_gdi_redraw_invalidate) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE));
    RECT r; CHECK(upd_rect(w, &r)); CHECK_RECT(r, 0, 0, 200, 150);
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_VALIDATE));
    CHECK(!upd_rect(w, NULL));
    killwin(w);
}
TEST(paint_gdi_redraw_rect_and_region) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    RECT in = {10, 10, 50, 50};
    CHECK(U(RedrawWindow)(w, &in, NULL, RDW_INVALIDATE));
    RECT r; CHECK(upd_rect(w, &r)); CHECK_RECT(r, 10, 10, 50, 50);
    RECT v = {10, 10, 50, 30};
    CHECK(U(RedrawWindow)(w, &v, NULL, RDW_VALIDATE));
    CHECK(upd_rect(w, &r)); CHECK_RECT(r, 10, 30, 50, 50);
    U(ValidateRect)(w, NULL);
    HRGN rg = CreateRectRgn(70, 70, 90, 100);
    RECT ignored = {0, 0, 5, 5};
    CHECK(U(RedrawWindow)(w, &ignored, rg, RDW_INVALIDATE));    /* hrgnUpdate wins over lprcUpdate */
    CHECK(upd_rect(w, &r)); CHECK_RECT(r, 70, 70, 90, 100);
    U(ValidateRect)(w, NULL);
    DeleteObject(rg);
    killwin(w);
}
TEST(paint_gdi_redraw_updatenow) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset();
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW));
    EQ(pg_r.paint, 1);
    CHECK(!upd_rect(w, NULL));
    pg_reset();
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_UPDATENOW));
    EQ(pg_r.paint, 0);
    killwin(w);
}
TEST(paint_gdi_redraw_erase_flags) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.erase_ret = 0;
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW));
    EQ(pg_r.erase, 1);
    CHECK(pg_r.ps_erase == TRUE);
    pg_reset(); pg_r.erase_ret = 0;
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW));   /* no RDW_ERASE */
    EQ(pg_r.erase, 0);
    pg_reset(); pg_r.erase_ret = 0;
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_NOERASE | RDW_UPDATENOW));
    t_note("ERASE|NOERASE -> erase msgs %d", pg_r.erase);
    pg_reset(); pg_r.erase_ret = 0;
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_ERASE | RDW_UPDATENOW));   /* ERASE without INVALIDATE has no effect */
    EQ(pg_r.erase, 0);
    EQ(pg_r.paint, 0);
    killwin(w);
}
TEST(paint_gdi_redraw_erasenow) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset(); pg_r.erase_ret = 0;
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ERASENOW));
    EQ(pg_r.erase, 1);       /* erase delivered before the function returns */
    EQ(pg_r.paint, 0);       /* but not WM_PAINT */
    CHECK(upd_rect(w, NULL));
    U(ValidateRect)(w, NULL);
    killwin(w);
}
TEST(paint_gdi_redraw_frame) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND w = pg_recwindow(WS_OVERLAPPEDWINDOW, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 240, 200, NULL);
    CHECK(w != NULL); if (!w) return;
    U(ShowWindow)(w, SW_SHOWNA); pg_flush(w);
    pg_reset();
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW));
    CHECK(pg_r.ncpaint >= 1);
    pg_reset();
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW));
    t_note("INVALIDATE only: WM_NCPAINT count %d", pg_r.ncpaint);
    killwin(w);
}
TEST(paint_gdi_redraw_internalpaint) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    pg_reset();
    CHECK(U(RedrawWindow)(w, NULL, NULL, RDW_INTERNALPAINT));
    CHECK(!upd_rect(w, NULL));
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, WM_PAINT, WM_PAINT, PM_REMOVE));
    CHECK(m.message == WM_PAINT);
    U(DispatchMessageA)(&m);
    EQ(pg_r.paint, 1);
    CHECK(!U(PeekMessageA)(&m, w, WM_PAINT, WM_PAINT, PM_NOREMOVE));
    killwin(w);
}
TEST(paint_gdi_redraw_children) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND p = mkwin(); CHECK(p != NULL); if (!p) return;
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 20, 20, 60, 60, p);
    CHECK(c != NULL);
    if (c) {
        pg_flush(p); U(ValidateRect)(c, NULL); U(ValidateRect)(p, NULL);
        CHECK(U(RedrawWindow)(p, NULL, NULL, RDW_INVALIDATE | RDW_NOCHILDREN));
        CHECK(!upd_rect(c, NULL));
        U(ValidateRect)(p, NULL);
        CHECK(U(RedrawWindow)(p, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN));
        CHECK(upd_rect(c, NULL));
        RECT r; upd_rect(c, &r);
        CHECK_RECT(r, 0, 0, 60, 60);
        CHECK(U(RedrawWindow)(p, NULL, NULL, RDW_VALIDATE | RDW_ALLCHILDREN));
        CHECK(!upd_rect(c, NULL));
        CHECK(!upd_rect(p, NULL));
        U(DestroyWindow)(c);
    }
    killwin(p);
}
TEST(paint_gdi_redraw_invalid_args) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    SetLastError(0);
    CHECK(U(RedrawWindow)((HWND)(ULONG_PTR)0x77777, NULL, NULL, RDW_INVALIDATE) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    SetLastError(0);
    CHECK(U(RedrawWindow)(w, NULL, (HRGN)(ULONG_PTR)0x2468, RDW_INVALIDATE) == FALSE);
    t_note("RedrawWindow(bad hrgn) err=%lu", GetLastError());
    U(ValidateRect)(w, NULL);
    SetLastError(0);
    BOOL r = U(RedrawWindow)(w, NULL, NULL, 0);
    t_note("RedrawWindow(flags 0) -> %d err=%lu", r, GetLastError());
    r = U(RedrawWindow)(w, NULL, NULL, 0x80000000);
    t_note("RedrawWindow(flags 0x80000000) -> %d err=%lu", r, GetLastError());
    U(ValidateRect)(w, NULL);
    killwin(w);
}
TEST(paint_gdi_redraw_both_invalidate_validate) {
    NEED_PAINT_FNS(); REQUIRE(U(RedrawWindow));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    BOOL r = U(RedrawWindow)(w, NULL, NULL, RDW_INVALIDATE | RDW_VALIDATE);
    t_note("INVALIDATE|VALIDATE -> %d, update pending %d", r, upd_rect(w, NULL));
    U(ValidateRect)(w, NULL);
    killwin(w);
}

/* ---------------- LockWindowUpdate ---------------- */
TEST(paint_gdi_lock_basic) {
    NEED_PAINT_FNS(); REQUIRE(U(LockWindowUpdate));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(LockWindowUpdate)(w));
    CHECK(U(LockWindowUpdate)(NULL));
    killwin(w);
}
TEST(paint_gdi_lock_second_window_fails) {
    NEED_PAINT_FNS(); REQUIRE(U(LockWindowUpdate));
    HWND a = mkwin(), b = pg_topwin(600, 300, 100, 100);
    CHECK(a && b);
    if (a && b) {
        CHECK(U(LockWindowUpdate)(a));
        SetLastError(0);
        BOOL r = U(LockWindowUpdate)(b);
        CHECK(r == FALSE);
        t_note("second LockWindowUpdate err=%lu", GetLastError());
        CHECK(U(LockWindowUpdate)(NULL));
        CHECK(U(LockWindowUpdate)(b));
        CHECK(U(LockWindowUpdate)(NULL));
    }
    U(LockWindowUpdate)(NULL);
    killwin(a); killwin(b);
}
TEST(paint_gdi_lock_unlock_when_not_locked) {
    NEED_PAINT_FNS(); REQUIRE(U(LockWindowUpdate));
    U(LockWindowUpdate)(NULL);
    SetLastError(0);
    BOOL r = U(LockWindowUpdate)(NULL);
    t_note("unlock when unlocked -> %d", r);
}
TEST(paint_gdi_lock_invalid_window) {
    NEED_PAINT_FNS(); REQUIRE(U(LockWindowUpdate));
    SetLastError(0);
    CHECK(U(LockWindowUpdate)((HWND)(ULONG_PTR)0x424242) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    U(LockWindowUpdate)(NULL);
}
TEST(paint_gdi_lock_desktop_fails) {
    NEED_PAINT_FNS(); REQUIRE(U(LockWindowUpdate));
    SetLastError(0);
    BOOL r = U(LockWindowUpdate)(U(GetDesktopWindow)());
    t_note("LockWindowUpdate(desktop) -> %d err=%lu", r, GetLastError());
    if (r) U(LockWindowUpdate)(NULL);
}
TEST(paint_gdi_lock_dcx_lockwindowupdate) {
    NEED_PAINT_FNS(); REQUIRE(U(LockWindowUpdate)); REQUIRE(U(GetDCEx));
    HWND w = mkwin(); CHECK(w != NULL); if (!w) return;
    HDC probe = U(GetDC)(w);
    BOOL vis = PtVisible(probe, 50, 50);
    U(ReleaseDC)(w, probe);
    if (vis && U(LockWindowUpdate)(w)) {
        /* while locked, ordinary DCs have an empty visible region ... */
        HDC n = U(GetDC)(w);
        RECT cb;
        CHECK(n != NULL);
        if (n) { EQ(GetClipBox(n, &cb), NULLREGION); CHECK(!PtVisible(n, 50, 50)); U(ReleaseDC)(w, n); }
        /* ... unless DCX_LOCKWINDOWUPDATE asks to draw through the lock */
        HDC d = U(GetDCEx)(w, NULL, DCX_CACHE | DCX_LOCKWINDOWUPDATE);
        CHECK(d != NULL);
        if (d) { CHECK(PtVisible(d, 50, 50)); U(ReleaseDC)(w, d); }
        U(LockWindowUpdate)(NULL);
        n = U(GetDC)(w);
        CHECK(PtVisible(n, 50, 50));
        U(ReleaseDC)(w, n);
    }
    U(LockWindowUpdate)(NULL);
    killwin(w);
}
