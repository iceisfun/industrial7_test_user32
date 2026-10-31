/* GetSysColor / GetSysColorBrush / SetSysColors */
#include "sp.h"

#define NCOL 31
static DWORD gc(int i) { return U(GetSysColor)(i); }

static void snapshot(COLORREF *c) { int i; for (i = 0; i < NCOL; i++) c[i] = gc(i); }
/* Restore every colour that differs from the snapshot. */
static void restore(const COLORREF *saved) {
    INT idx[NCOL]; COLORREF val[NCOL]; int n = 0, i;
    for (i = 0; i < NCOL; i++) if (gc(i) != saved[i]) { idx[n] = i; val[n] = saved[i]; n++; }
    if (n) U(SetSysColors)(n, idx, val);
}

static void check_color(int idx, const char *name) {
    DWORD c; HBRUSH b, b2; LOGBRUSH lb;
    REQUIRE(U(GetSysColor)); REQUIRE(U(GetSysColorBrush));
    SetLastError(0xdeadbeef);
    c = gc(idx);
    CHECKF((c & 0xFF000000) == 0, "%s = 0x%08lx has high byte", name, c);
    CHECKF(c == gc(idx), "%s unstable", name);
    CHECKF(GetLastError() == 0xdeadbeef, "%s changed last error to %lu", name, GetLastError());
    b = U(GetSysColorBrush)(idx);
    CHECKF(b != NULL, "%s brush NULL", name);
    if (!b) return;
    b2 = U(GetSysColorBrush)(idx);
    CHECKF(b == b2, "%s brush handle unstable %p %p", name, b, b2);
    CHECKF(GetObjectType(b) == OBJ_BRUSH, "%s brush type %lu", name, GetObjectType(b));
    memset(&lb, 0xCD, sizeof lb);
    CHECKF(GetObjectA(b, sizeof lb, &lb) == sizeof lb, "%s GetObject", name);
    CHECKF(lb.lbStyle == BS_SOLID, "%s style %u", name, lb.lbStyle);
    CHECKF(lb.lbColor == c, "%s brush colour 0x%lx vs 0x%lx", name, lb.lbColor, c);
}
#define COLT(n) TEST(color_##n) { check_color(COLOR_##n, "COLOR_" #n); }
COLT(SCROLLBAR) COLT(BACKGROUND) COLT(ACTIVECAPTION) COLT(INACTIVECAPTION) COLT(MENU)
COLT(WINDOW) COLT(WINDOWFRAME) COLT(MENUTEXT) COLT(WINDOWTEXT) COLT(CAPTIONTEXT)
COLT(ACTIVEBORDER) COLT(INACTIVEBORDER) COLT(APPWORKSPACE) COLT(HIGHLIGHT) COLT(HIGHLIGHTTEXT)
COLT(BTNFACE) COLT(BTNSHADOW) COLT(GRAYTEXT) COLT(BTNTEXT) COLT(INACTIVECAPTIONTEXT)
COLT(BTNHIGHLIGHT) COLT(3DDKSHADOW) COLT(3DLIGHT) COLT(INFOTEXT) COLT(INFOBK)
COLT(HOTLIGHT) COLT(GRADIENTACTIVECAPTION) COLT(GRADIENTINACTIVECAPTION) COLT(MENUHILIGHT) COLT(MENUBAR)

TEST(color_index_constants) {
    CHECK(COLOR_SCROLLBAR == 0 && COLOR_BACKGROUND == 1 && COLOR_MENUBAR == 30);
    CHECK(COLOR_DESKTOP == COLOR_BACKGROUND && COLOR_3DFACE == COLOR_BTNFACE);
    CHECK(COLOR_3DSHADOW == COLOR_BTNSHADOW && COLOR_3DHIGHLIGHT == COLOR_BTNHIGHLIGHT);
    CHECK(COLOR_3DHILIGHT == COLOR_BTNHIGHLIGHT && COLOR_BTNHILIGHT == COLOR_BTNHIGHLIGHT);
}
TEST(color_aliases_equal) {
    REQUIRE(U(GetSysColor));
    CHECK(gc(COLOR_DESKTOP) == gc(COLOR_BACKGROUND));
    CHECK(gc(COLOR_3DFACE) == gc(COLOR_BTNFACE));
    CHECK(gc(COLOR_3DSHADOW) == gc(COLOR_BTNSHADOW));
    CHECK(gc(COLOR_3DHIGHLIGHT) == gc(COLOR_BTNHIGHLIGHT));
    CHECK(U(GetSysColorBrush)(COLOR_3DFACE) == U(GetSysColorBrush)(COLOR_BTNFACE));
}
TEST(color_invalid_indices_getsyscolor) {
    static const int bad[] = { 31, 32, 33, 50, 100, 255, 1000, -1, -2, 0x7FFFFFFF, (int)0x80000000, 0x10000, 0xFFFF };
    unsigned i;
    REQUIRE(U(GetSysColor));
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        DWORD c;
        SetLastError(0xdeadbeef);
        c = gc(bad[i]);
        CHECKF(c == 0, "GetSysColor(%d) = 0x%lx", bad[i], c);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "GetSysColor(%d) err=%lu", bad[i], GetLastError());
    }
}
TEST(color_invalid_indices_brush) {
    static const int bad[] = { 31, 32, 100, 1000, -1, -100, 0x7FFFFFFF, (int)0x80000000, 0x10000 };
    unsigned i;
    REQUIRE(U(GetSysColorBrush));
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        HBRUSH b;
        SetLastError(0xdeadbeef);
        b = U(GetSysColorBrush)(bad[i]);
        CHECKF(b == NULL, "GetSysColorBrush(%d) = %p", bad[i], b);
        if (GetLastError() != 0xdeadbeef) t_note("GetSysColorBrush(%d) err=%lu", bad[i], GetLastError());
    }
}
TEST(color_error_cleared_on_success) {
    REQUIRE(U(GetSysColor));
    SetLastError(0);
    gc(COLOR_WINDOW);
    CHECK(GetLastError() == 0);
    SetLastError(0xdeadbeef);
    gc(COLOR_WINDOW);
    CHECK(GetLastError() == 0xdeadbeef);
}
TEST(color_distinct_brush_handles_for_distinct_colors) {
    HBRUSH w, t;
    REQUIRE(U(GetSysColorBrush));
    w = U(GetSysColorBrush)(COLOR_WINDOW); t = U(GetSysColorBrush)(COLOR_WINDOWTEXT);
    CHECK(w && t);
    if (gc(COLOR_WINDOW) != gc(COLOR_WINDOWTEXT)) CHECK(w != t);
}
TEST(color_window_vs_text_contrast) {
    DWORD w, t;
    REQUIRE(U(GetSysColor));
    w = gc(COLOR_WINDOW); t = gc(COLOR_WINDOWTEXT);
    CHECK(w != t);
    CHECK(gc(COLOR_HIGHLIGHT) != gc(COLOR_HIGHLIGHTTEXT));
    CHECK(gc(COLOR_BTNFACE) != gc(COLOR_BTNTEXT));
    CHECK(gc(COLOR_MENU) != gc(COLOR_MENUTEXT));
    CHECK(gc(COLOR_INFOBK) != gc(COLOR_INFOTEXT));
}
TEST(color_all_brushes_usable_in_dc) {
    HDC dc; int i;
    REQUIRE(U(GetSysColorBrush));
    dc = CreateCompatibleDC(NULL);
    CHECK(dc != NULL);
    if (!dc) return;
    for (i = 0; i < NCOL; i++) {
        HGDIOBJ old = SelectObject(dc, U(GetSysColorBrush)(i));
        CHECKF(old != NULL, "select brush %d", i);
        if (old) SelectObject(dc, old);
    }
    DeleteDC(dc);
}
TEST(color_brush_survives_deleteobject_attempt) {
    HBRUSH b;
    REQUIRE(U(GetSysColorBrush));
    b = U(GetSysColorBrush)(COLOR_WINDOW);
    /* system brushes cannot be deleted; the handle must stay valid */
    DeleteObject(b);
    CHECK(GetObjectType(U(GetSysColorBrush)(COLOR_WINDOW)) == OBJ_BRUSH);
    CHECK(U(GetSysColorBrush)(COLOR_WINDOW) == b);
}
TEST(color_getsyscolor_repeat_stress) {
    int i, bad = 0; DWORD first;
    REQUIRE(U(GetSysColor));
    first = gc(COLOR_BTNFACE);
    for (i = 0; i < 20000; i++) if (gc(COLOR_BTNFACE) != first) bad++;
    CHECK(bad == 0);
}

/* ---- SetSysColors ---- */
TEST(setsyscolors_roundtrip_hotlight) {
    COLORREF saved[NCOL]; INT idx = COLOR_HOTLIGHT; COLORREF v;
    BOOL r;
    REQUIRE(U(SetSysColors)); REQUIRE(U(GetSysColor));
    snapshot(saved);
    v = saved[COLOR_HOTLIGHT] ^ 0x00010101;
    r = U(SetSysColors)(1, &idx, &v);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        CHECK(gc(COLOR_HOTLIGHT) == v);
        CHECK(U(SetSysColors)(1, &idx, &saved[COLOR_HOTLIGHT]));
        CHECK(gc(COLOR_HOTLIGHT) == saved[COLOR_HOTLIGHT]);
    }
    restore(saved);
    CHECK(gc(COLOR_HOTLIGHT) == saved[COLOR_HOTLIGHT]);
}
TEST(setsyscolors_brush_follows) {
    COLORREF saved[NCOL]; INT idx = COLOR_HOTLIGHT; COLORREF v; HBRUSH b; LOGBRUSH lb;
    REQUIRE(U(SetSysColors)); REQUIRE(U(GetSysColorBrush));
    snapshot(saved);
    v = saved[COLOR_HOTLIGHT] ^ 0x00020202;
    if (U(SetSysColors)(1, &idx, &v)) {
        b = U(GetSysColorBrush)(COLOR_HOTLIGHT);
        CHECK(b != NULL);
        memset(&lb, 0, sizeof lb);
        CHECK(GetObjectA(b, sizeof lb, &lb) == sizeof lb);
        CHECK(lb.lbColor == v);
    } else CHECK(0);
    restore(saved);
    b = U(GetSysColorBrush)(COLOR_HOTLIGHT);
    memset(&lb, 0, sizeof lb);
    GetObjectA(b, sizeof lb, &lb);
    CHECK(lb.lbColor == saved[COLOR_HOTLIGHT]);
}
TEST(setsyscolors_two_elements) {
    COLORREF saved[NCOL]; INT idx[2] = { COLOR_HOTLIGHT, COLOR_GRADIENTINACTIVECAPTION }; COLORREF v[2];
    REQUIRE(U(SetSysColors)); REQUIRE(U(GetSysColor));
    snapshot(saved);
    v[0] = saved[idx[0]] ^ 0x00030303; v[1] = saved[idx[1]] ^ 0x00040404;
    if (U(SetSysColors)(2, idx, v)) {
        CHECK(gc(idx[0]) == v[0]);
        CHECK(gc(idx[1]) == v[1]);
    } else CHECK(0);
    restore(saved);
    CHECK(gc(idx[0]) == saved[idx[0]] && gc(idx[1]) == saved[idx[1]]);
}
TEST(setsyscolors_same_value_ok) {
    COLORREF saved[NCOL]; INT idx = COLOR_HOTLIGHT; BOOL r;
    REQUIRE(U(SetSysColors));
    snapshot(saved);
    r = U(SetSysColors)(1, &idx, &saved[COLOR_HOTLIGHT]);
    CHECK(r);
    CHECK(gc(COLOR_HOTLIGHT) == saved[COLOR_HOTLIGHT]);
}
TEST(setsyscolors_high_byte_observed) {
    COLORREF saved[NCOL]; INT idx = COLOR_HOTLIGHT; COLORREF v; BOOL r;
    REQUIRE(U(SetSysColors));
    snapshot(saved);
    v = 0xFF000000 | (saved[COLOR_HOTLIGHT] ^ 0x00050505);
    r = U(SetSysColors)(1, &idx, &v);
    t_note("SetSysColors with high byte -> %d, stored 0x%08lx", r, gc(COLOR_HOTLIGHT));
    restore(saved);
    CHECK(gc(COLOR_HOTLIGHT) == saved[COLOR_HOTLIGHT]);
}
TEST(setsyscolors_zero_elements) {
    COLORREF saved[NCOL]; INT idx = COLOR_HOTLIGHT; COLORREF v = 0; BOOL r;
    REQUIRE(U(SetSysColors));
    snapshot(saved);
    SetLastError(0xdeadbeef);
    r = U(SetSysColors)(0, &idx, &v);
    t_note("SetSysColors(0 elements) -> %d err=%lu", r, GetLastError());
    restore(saved);
    CHECK(gc(COLOR_HOTLIGHT) == saved[COLOR_HOTLIGHT]);
}
TEST(setsyscolors_negative_count) {
    COLORREF saved[NCOL]; INT idx = COLOR_HOTLIGHT; COLORREF v; BOOL r;
    REQUIRE(U(SetSysColors));
    snapshot(saved);
    v = saved[COLOR_HOTLIGHT];
    SetLastError(0xdeadbeef);
    r = U(SetSysColors)(-1, &idx, &v);
    t_note("SetSysColors(-1 elements) -> %d err=%lu", r, GetLastError());
    restore(saved);
    CHECK(gc(COLOR_HOTLIGHT) == saved[COLOR_HOTLIGHT]);
}
TEST(setsyscolors_invalid_index) {
    COLORREF saved[NCOL]; INT idx; COLORREF v = 0x00112233; BOOL r; int i;
    static const int bad[] = { 31, 100, -1, 0x7FFFFFFF };
    REQUIRE(U(SetSysColors));
    snapshot(saved);
    for (i = 0; i < 4; i++) {
        idx = bad[i];
        SetLastError(0xdeadbeef);
        r = U(SetSysColors)(1, &idx, &v);
        t_note("SetSysColors(index %d) -> %d err=%lu", bad[i], r, GetLastError());
    }
    for (i = 0; i < NCOL; i++) CHECKF(gc(i) == saved[i], "colour %d changed by invalid-index call", i);
    restore(saved);
}
TEST(setsyscolors_mixed_valid_invalid) {
    COLORREF saved[NCOL]; INT idx[2] = { COLOR_HOTLIGHT, 50 }; COLORREF v[2]; BOOL r;
    REQUIRE(U(SetSysColors));
    snapshot(saved);
    v[0] = saved[COLOR_HOTLIGHT] ^ 0x00060606; v[1] = 0;
    SetLastError(0xdeadbeef);
    r = U(SetSysColors)(2, idx, v);
    t_note("SetSysColors(valid+invalid) -> %d err=%lu hotlight now 0x%lx", r, GetLastError(), gc(COLOR_HOTLIGHT));
    restore(saved);
    CHECK(gc(COLOR_HOTLIGHT) == saved[COLOR_HOTLIGHT]);
}
TEST(setsyscolors_all_unchanged_after_tests) {
    COLORREF a[NCOL], b[NCOL];
    REQUIRE(U(GetSysColor));
    snapshot(a);
    Sleep(1);
    snapshot(b);
    CHECK(!memcmp(a, b, sizeof a));
}
TEST(setsyscolors_all_elements_identity) {
    COLORREF saved[NCOL]; INT idx[NCOL]; int i;
    REQUIRE(U(SetSysColors));
    snapshot(saved);
    for (i = 0; i < NCOL; i++) idx[i] = i;
    CHECK(U(SetSysColors)(NCOL, idx, saved));
    for (i = 0; i < NCOL; i++) CHECKF(gc(i) == saved[i], "colour %d changed", i);
}
static volatile int g_syscolorchange;
static LRESULT CALLBACK colproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SYSCOLORCHANGE) g_syscolorchange++;
    return DefWindowProcA(h, m, w, l);
}
TEST(setsyscolors_sends_wm_syscolorchange) {
    COLORREF saved[NCOL]; INT idx = COLOR_HOTLIGHT; COLORREF v; HWND w; WNDCLASSA wc; int before, mid;
    REQUIRE(U(SetSysColors)); REQUIRE(U(RegisterClassA)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(UnregisterClassA));
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = colproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "SPColorCls";
    CHECK(U(RegisterClassA)(&wc) != 0);
    snapshot(saved);
    w = U(CreateWindowExA)(0, "SPColorCls", "c", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    CHECK(w != NULL);
    if (w) {
        t_pump();
        g_syscolorchange = 0;
        v = saved[COLOR_HOTLIGHT] ^ 0x00070707;
        U(SetSysColors)(1, &idx, &v);
        t_pump(); Sleep(50); t_pump();
        mid = g_syscolorchange;
        restore(saved);
        t_pump(); Sleep(50); t_pump();
        before = g_syscolorchange;
        CHECKF(mid >= 1, "no WM_SYSCOLORCHANGE after change (got %d)", mid);
        CHECKF(before >= 2, "no WM_SYSCOLORCHANGE after restore (got %d)", before);
        U(DestroyWindow)(w);
    } else restore(saved);
    U(UnregisterClassA)("SPColorCls", wc.hInstance);
}
