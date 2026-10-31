/* gap_00: CreateWindowInBand(Ex), CreateWindowIndirect, AnyPopup, CascadeChildWindows,
 * CalculatePopupWindowPosition, DeferWindowPosAndBand */
#include "gap0.h"

UDECL(HWND, CreateWindowInBand, (DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID, DWORD));
UDECL(HWND, CreateWindowInBandEx, (DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID, DWORD, DWORD));
UDECL(HWND, CreateWindowIndirect, (void *));
UDECL(HDWP, DeferWindowPosAndBand, (HDWP, HWND, HWND, int, int, int, int, UINT, UINT, UINT));
UDECL(WORD, CascadeChildWindows, (HWND, UINT));

#define H() GetModuleHandleA(NULL)

typedef struct {
    DWORD cbSize; DWORD pad0;
    const void *cls; const void *name;
    DWORD style; DWORD ex;
    int x, y, cx, cy;
    HWND parent; HMENU menu; HINSTANCE inst; void *param;
    WORD dpi; WORD pad1; DWORD band; DWORD pad2;
} G0CWI;   /* mirrors the layout read by CreateWindowIndirect */

static void cwi_fill(G0CWI *d, DWORD size) {
    memset(d, 0, sizeof *d);
    d->cbSize = size;
    d->cls = g0_cls(); d->name = L"indirect";
    d->style = WS_POPUP; d->ex = 0;
    d->x = 11; d->y = 22; d->cx = 133; d->cy = 77;
    d->inst = H();
}

/* ---------- CreateWindowInBand ---------- */
TEST(gap0_inband_default_band_creates) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    g0_cls();
    HWND w = UN(CreateWindowInBand)(0, g0_cls(), L"banded", WS_POPUP, 10, 20, 130, 70, NULL, NULL, H(), NULL, 0);
    CHECK(w != NULL);
    if (!w) return;
    CHECK(U(IsWindow)(w));
    WCHAR b[64]; U(GetClassNameW)(w, b, 64);
    EXPECT_WSTR(b, g0_cls());
    U(GetWindowTextW)(w, b, 64);
    EXPECT_WSTR(b, L"banded");
    RECT r; U(GetWindowRect)(w, &r);
    CHECK(r.left == 10 && r.top == 20 && r.right == 140 && r.bottom == 90);
    CHECK(U(IsWindowUnicode)(w));
    CHECK(U(DestroyWindow)(w));
    CHECK(!U(IsWindow)(w));
}
TEST(gap0_inband_matches_createwindowex) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    HWND a = UN(CreateWindowInBand)(WS_EX_TOOLWINDOW, g0_cls(), L"cmp", WS_POPUP | WS_BORDER, 5, 6, 77, 88, NULL, NULL, H(), NULL, 0);
    HWND b = U(CreateWindowExW)(WS_EX_TOOLWINDOW, g0_cls(), L"cmp", WS_POPUP | WS_BORDER, 5, 6, 77, 88, NULL, NULL, H(), NULL);
    REQUIRE(a); REQUIRE(b);
    RECT ra, rb, ca, cb;
    U(GetWindowRect)(a, &ra); U(GetWindowRect)(b, &rb);
    U(GetClientRect)(a, &ca); U(GetClientRect)(b, &cb);
    CHECK(!memcmp(&ra, &rb, sizeof ra));
    CHECK(!memcmp(&ca, &cb, sizeof ca));
    EXPECT_EQ(U(GetWindowLongW)(a, GWL_STYLE), U(GetWindowLongW)(b, GWL_STYLE));
    EXPECT_EQ(U(GetWindowLongW)(a, GWL_EXSTYLE), U(GetWindowLongW)(b, GWL_EXSTYLE));
    EXPECT_EQ(U(GetParent)(a), U(GetParent)(b));
    U(DestroyWindow)(a); U(DestroyWindow)(b);
}
TEST(gap0_inband_createparams_delivered) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    int token = 0;
    g0_create_count = 0;
    HWND w = UN(CreateWindowInBand)(0, g0_cls(), L"cp", WS_POPUP, 3, 4, 50, 60, NULL, NULL, H(), &token, 0);
    REQUIRE(w);
    EXPECT_EQ(g0_create_count, 1);
    CHECK(g0_last_cs.lpCreateParams == &token);
    CHECK(g0_last_cs.x == 3 && g0_last_cs.y == 4 && g0_last_cs.cx == 50 && g0_last_cs.cy == 60);
    CHECK(g0_last_cs.hInstance == H());
    CHECK((g0_last_cs.style & WS_POPUP) != 0);
    U(DestroyWindow)(w);
}
TEST(gap0_inband_unknown_class) {
    REQUIRE(UN(CreateWindowInBand));
    SET_ERR(0);
    HWND w = UN(CreateWindowInBand)(0, L"Gap0NoSuchClass", L"x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, H(), NULL, 0);
    CHECK(w == NULL);
    EXPECT_ERR(w, ERROR_CANNOT_FIND_WND_CLASS);
}
TEST(gap0_inband_builtin_class) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    HWND w = UN(CreateWindowInBand)(0, L"STATIC", L"lbl", WS_POPUP, 0, 0, 40, 20, NULL, NULL, H(), NULL, 0);
    REQUIRE(w);
    WCHAR b[32]; U(GetClassNameW)(w, b, 32);
    CHECK(!lstrcmpiW(b, L"static"));
    U(DestroyWindow)(w);
}
TEST(gap0_inband_unicode_title_roundtrip) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    static const WCHAR t[] = { 0x00E9, 0x4E2D, 0x6587, 0xD83D, 0xDE00, 'x', 0 };
    HWND w = UN(CreateWindowInBand)(0, g0_cls(), t, WS_POPUP, 0, 0, 40, 20, NULL, NULL, H(), NULL, 0);
    REQUIRE(w);
    WCHAR b[16]; int n = U(GetWindowTextW)(w, b, 16);
    EXPECT_EQ(n, 6);
    EXPECT_WSTR(b, t);
    U(DestroyWindow)(w);
}
TEST(gap0_inband_null_title) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    HWND w = UN(CreateWindowInBand)(0, g0_cls(), NULL, WS_POPUP, 0, 0, 40, 20, NULL, NULL, H(), NULL, 0);
    REQUIRE(w);
    WCHAR b[8] = { 'z', 0 };
    EXPECT_EQ(U(GetWindowTextW)(w, b, 8), 0);
    EXPECT_EQ(b[0], 0);
    EXPECT_EQ(U(GetWindowTextLengthW)(w), 0);
    U(DestroyWindow)(w);
}
TEST(gap0_inband_child_with_parent) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    HWND p = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    REQUIRE(p);
    HWND c = UN(CreateWindowInBand)(0, g0_cls(), L"c", WS_CHILD, 5, 5, 20, 20, p, (HMENU)(ULONG_PTR)77, H(), NULL, 0);
    REQUIRE(c);
    EXPECT_EQ(U(GetParent)(c), p);
    EXPECT_EQ(U(GetDlgCtrlID)(c), 77);
    U(DestroyWindow)(p);
    CHECK(!U(IsWindow)(c));
}
TEST(gap0_inband_child_without_parent_fails) {
    REQUIRE(UN(CreateWindowInBand));
    SET_ERR(0);
    HWND w = UN(CreateWindowInBand)(0, g0_cls(), L"c", WS_CHILD, 0, 0, 20, 20, NULL, NULL, H(), NULL, 0);
    CHECK(w == NULL);
    EXPECT_ERR(w, ERROR_TLW_WITH_WSCHILD);
}
TEST(gap0_inband_bogus_parent) {
    REQUIRE(UN(CreateWindowInBand));
    SET_ERR(0);
    HWND w = UN(CreateWindowInBand)(0, g0_cls(), L"c", WS_CHILD, 0, 0, 20, 20, (HWND)(ULONG_PTR)0xDEAD0, NULL, H(), NULL, 0);
    CHECK(w == NULL);
    EXPECT_ERR(w, ERROR_INVALID_WINDOW_HANDLE);
}
TEST(gap0_inband_nonzero_bands_observed) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    /* Bands other than ZBID_DEFAULT are privileged or invalid; only observe */
    static const DWORD z[] = { 1, 2, 3, 4, 5, 16, 0xFF, 0x7FFFFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof z / sizeof z[0]; i++) {
        SET_ERR(0);
        HWND w = UN(CreateWindowInBand)(0, g0_cls(), L"zb", WS_POPUP, 0, 0, 10, 10, NULL, NULL, H(), NULL, z[i]);
        t_note("CreateWindowInBand band %lu -> %p err %lu", z[i], (void *)w, w ? 0 : GetLastError());
        if (w) U(DestroyWindow)(w);
    }
}
TEST(gap0_inband_repeat_create_destroy) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    int bad = 0;
    for (int i = 0; i < 300; i++) {
        HWND w = UN(CreateWindowInBand)(0, g0_cls(), L"r", WS_POPUP, 0, 0, 10, 10, NULL, NULL, H(), NULL, 0);
        if (!w || !U(DestroyWindow)(w) || U(IsWindow)(w)) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap0_inband_extended_style_applied) {
    REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    HWND w = UN(CreateWindowInBand)(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, g0_cls(), L"ex", WS_POPUP, 0, 0, 10, 10, NULL, NULL, H(), NULL, 0);
    REQUIRE(w);
    LONG ex = U(GetWindowLongW)(w, GWL_EXSTYLE);
    CHECK(ex & WS_EX_TOOLWINDOW);
    CHECK(ex & WS_EX_TOPMOST);
    U(DestroyWindow)(w);
}

/* ---------- CreateWindowInBandEx ---------- */
TEST(gap0_inbandex_default) {
    REQUIRE(UN(CreateWindowInBandEx)); REQUIRE(U(DestroyWindow));
    HWND w = UN(CreateWindowInBandEx)(0, g0_cls(), L"bandex", WS_POPUP, 12, 13, 90, 40, NULL, NULL, H(), NULL, 0, 0);
    REQUIRE(w);
    RECT r; U(GetWindowRect)(w, &r);
    CHECK(r.left == 12 && r.top == 13 && r.right == 102 && r.bottom == 53);
    WCHAR b[32]; U(GetWindowTextW)(w, b, 32);
    EXPECT_WSTR(b, L"bandex");
    U(DestroyWindow)(w);
}
TEST(gap0_inbandex_unknown_class) {
    REQUIRE(UN(CreateWindowInBandEx));
    SET_ERR(0);
    HWND w = UN(CreateWindowInBandEx)(0, L"Gap0NoSuchClass2", L"x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, H(), NULL, 0, 0);
    CHECK(w == NULL);
    EXPECT_ERR(w, ERROR_CANNOT_FIND_WND_CLASS);
}
TEST(gap0_inbandex_matches_inband) {
    REQUIRE(UN(CreateWindowInBandEx)); REQUIRE(UN(CreateWindowInBand)); REQUIRE(U(DestroyWindow));
    HWND a = UN(CreateWindowInBand)(0, g0_cls(), L"m", WS_POPUP | WS_CAPTION, 1, 2, 160, 90, NULL, NULL, H(), NULL, 0);
    HWND b = UN(CreateWindowInBandEx)(0, g0_cls(), L"m", WS_POPUP | WS_CAPTION, 1, 2, 160, 90, NULL, NULL, H(), NULL, 0, 0);
    REQUIRE(a); REQUIRE(b);
    RECT ra, rb, ca, cb;
    U(GetWindowRect)(a, &ra); U(GetWindowRect)(b, &rb);
    U(GetClientRect)(a, &ca); U(GetClientRect)(b, &cb);
    CHECK(!memcmp(&ra, &rb, sizeof ra));
    CHECK(!memcmp(&ca, &cb, sizeof ca));
    U(DestroyWindow)(a); U(DestroyWindow)(b);
}
TEST(gap0_inbandex_createparams) {
    REQUIRE(UN(CreateWindowInBandEx)); REQUIRE(U(DestroyWindow));
    int token;
    g0_create_count = 0;
    HWND w = UN(CreateWindowInBandEx)(0, g0_cls(), L"cp", WS_POPUP, 0, 0, 10, 10, NULL, NULL, H(), &token, 0, 0);
    REQUIRE(w);
    CHECK(g0_last_cs.lpCreateParams == &token);
    U(DestroyWindow)(w);
}
TEST(gap0_inbandex_bogus_parent) {
    REQUIRE(UN(CreateWindowInBandEx));
    SET_ERR(0);
    HWND w = UN(CreateWindowInBandEx)(0, g0_cls(), L"c", WS_CHILD, 0, 0, 20, 20, (HWND)(ULONG_PTR)0xBEEF0, NULL, H(), NULL, 0, 0);
    CHECK(w == NULL);
    EXPECT_ERR(w, ERROR_INVALID_WINDOW_HANDLE);
}
TEST(gap0_inbandex_repeat) {
    REQUIRE(UN(CreateWindowInBandEx)); REQUIRE(U(DestroyWindow));
    int bad = 0;
    for (int i = 0; i < 200; i++) {
        HWND w = UN(CreateWindowInBandEx)(0, g0_cls(), L"r", WS_POPUP, 0, 0, 10, 10, NULL, NULL, H(), NULL, 0, 0);
        if (!w || !U(DestroyWindow)(w)) bad++;
    }
    EXPECT_EQ(bad, 0);
}

/* ---------- CreateWindowIndirect ---------- */
TEST(gap0_cwi_basic) {
    REQUIRE(UN(CreateWindowIndirect)); REQUIRE(U(DestroyWindow));
    G0CWI d; cwi_fill(&d, sizeof d);
    HWND w = UN(CreateWindowIndirect)(&d);
    REQUIRE(w);
    WCHAR b[64]; U(GetClassNameW)(w, b, 64);
    EXPECT_WSTR(b, g0_cls());
    U(GetWindowTextW)(w, b, 64);
    EXPECT_WSTR(b, L"indirect");
    RECT r; U(GetWindowRect)(w, &r);
    CHECK(r.left == 11 && r.top == 22 && r.right == 144 && r.bottom == 99);
    U(DestroyWindow)(w);
}
TEST(gap0_cwi_struct_layout_size) {
    CHECK(sizeof(G0CWI) >= 0x58);
    EXPECT_EQ(offsetof(G0CWI, cls), 8);
    EXPECT_EQ(offsetof(G0CWI, style), 0x18);
    EXPECT_EQ(offsetof(G0CWI, x), 0x20);
    EXPECT_EQ(offsetof(G0CWI, parent), 0x30);
    EXPECT_EQ(offsetof(G0CWI, inst), 0x40);
    EXPECT_EQ(offsetof(G0CWI, param), 0x48);
    EXPECT_EQ(offsetof(G0CWI, band), 0x54);
    EXPECT_EQ(offsetof(G0CWI, dpi), 0x50);
}
TEST(gap0_cwi_small_size_rejected) {
    REQUIRE(UN(CreateWindowIndirect));
    static const DWORD s[] = { 0, 1, 4, 0x20, 0x3B };
    for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++) {
        G0CWI d; cwi_fill(&d, s[i]);
        SET_ERR(0);
        HWND w = UN(CreateWindowIndirect)(&d);
        CHECKF(w == NULL, "size %lx made %p", s[i], (void *)w);
        EXPECT_ERR(w, ERROR_INVALID_PARAMETER);
        if (w) U(DestroyWindow)(w);
    }
}
TEST(gap0_cwi_min_size_accepted) {
    REQUIRE(UN(CreateWindowIndirect)); REQUIRE(U(DestroyWindow));
    static const DWORD s[] = { 0x3C, 0x40, 0x58, 0x100, 0x7FFFFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++) {
        G0CWI d; cwi_fill(&d, s[i]);
        HWND w = UN(CreateWindowIndirect)(&d);
        CHECKF(w != NULL, "size %lx err %lu", s[i], GetLastError());
        if (w) U(DestroyWindow)(w);
    }
}
TEST(gap0_cwi_unknown_class) {
    REQUIRE(UN(CreateWindowIndirect));
    G0CWI d; cwi_fill(&d, sizeof d);
    d.cls = L"Gap0NoSuchClass3";
    SET_ERR(0);
    HWND w = UN(CreateWindowIndirect)(&d);
    CHECK(w == NULL);
    EXPECT_ERR(w, ERROR_CANNOT_FIND_WND_CLASS);
}
TEST(gap0_cwi_createparams_and_instance) {
    REQUIRE(UN(CreateWindowIndirect)); REQUIRE(U(DestroyWindow));
    G0CWI d; cwi_fill(&d, sizeof d);
    int token;
    d.param = &token;
    g0_create_count = 0;
    HWND w = UN(CreateWindowIndirect)(&d);
    REQUIRE(w);
    EXPECT_EQ(g0_create_count, 1);
    CHECK(g0_last_cs.lpCreateParams == &token);
    CHECK(g0_last_cs.hInstance == H());
    CHECK(g0_last_cs.cx == 133 && g0_last_cs.cy == 77);
    U(DestroyWindow)(w);
}
TEST(gap0_cwi_child_parent_menu_id) {
    REQUIRE(UN(CreateWindowIndirect)); REQUIRE(U(DestroyWindow));
    HWND p = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    REQUIRE(p);
    G0CWI d; cwi_fill(&d, sizeof d);
    d.style = WS_CHILD; d.parent = p; d.menu = (HMENU)(ULONG_PTR)4242;
    HWND c = UN(CreateWindowIndirect)(&d);
    REQUIRE(c);
    EXPECT_EQ(U(GetParent)(c), p);
    EXPECT_EQ(U(GetDlgCtrlID)(c), 4242);
    U(DestroyWindow)(p);
}
TEST(gap0_cwi_child_no_parent) {
    REQUIRE(UN(CreateWindowIndirect));
    G0CWI d; cwi_fill(&d, sizeof d);
    d.style = WS_CHILD;
    SET_ERR(0);
    HWND w = UN(CreateWindowIndirect)(&d);
    CHECK(w == NULL);
    EXPECT_ERR(w, ERROR_TLW_WITH_WSCHILD);
}
TEST(gap0_cwi_exstyle_and_style) {
    REQUIRE(UN(CreateWindowIndirect)); REQUIRE(U(DestroyWindow));
    G0CWI d; cwi_fill(&d, sizeof d);
    d.style = WS_POPUP | WS_BORDER | WS_DISABLED; d.ex = WS_EX_TOOLWINDOW;
    HWND w = UN(CreateWindowIndirect)(&d);
    REQUIRE(w);
    LONG s = U(GetWindowLongW)(w, GWL_STYLE);
    CHECK(s & WS_BORDER); CHECK(s & WS_DISABLED); CHECK(!(s & WS_VISIBLE));
    CHECK(U(GetWindowLongW)(w, GWL_EXSTYLE) & WS_EX_TOOLWINDOW);
    CHECK(!U(IsWindowEnabled)(w));
    U(DestroyWindow)(w);
}
TEST(gap0_cwi_default_band_zero) {
    REQUIRE(UN(CreateWindowIndirect)); REQUIRE(U(DestroyWindow));
    G0CWI d; cwi_fill(&d, sizeof d);
    d.band = 0;
    HWND w = UN(CreateWindowIndirect)(&d);
    CHECK(w != NULL);
    if (w) U(DestroyWindow)(w);
}
TEST(gap0_cwi_matches_createwindowex) {
    REQUIRE(UN(CreateWindowIndirect)); REQUIRE(U(DestroyWindow));
    G0CWI d; cwi_fill(&d, sizeof d);
    d.style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    HWND a = UN(CreateWindowIndirect)(&d);
    HWND b = U(CreateWindowExW)(0, g0_cls(), L"indirect", d.style, 11, 22, 133, 77, NULL, NULL, H(), NULL);
    REQUIRE(a); REQUIRE(b);
    RECT ra, rb, ca, cb;
    U(GetWindowRect)(a, &ra); U(GetWindowRect)(b, &rb);
    U(GetClientRect)(a, &ca); U(GetClientRect)(b, &cb);
    CHECK(!memcmp(&ra, &rb, sizeof ra));
    CHECK(!memcmp(&ca, &cb, sizeof ca));
    U(DestroyWindow)(a); U(DestroyWindow)(b);
}
TEST(gap0_cwi_repeat) {
    REQUIRE(UN(CreateWindowIndirect)); REQUIRE(U(DestroyWindow));
    int bad = 0;
    for (int i = 0; i < 200; i++) {
        G0CWI d; cwi_fill(&d, sizeof d);
        HWND w = UN(CreateWindowIndirect)(&d);
        if (!w || !U(DestroyWindow)(w)) bad++;
    }
    EXPECT_EQ(bad, 0);
}

/* ---------- AnyPopup ---------- */
TEST(gap0_anypopup_boolean) {
    REQUIRE(U(AnyPopup));
    BOOL b = U(AnyPopup)();
    CHECKF(b == TRUE || b == FALSE, "got %d", b);
}
TEST(gap0_anypopup_owned_visible_popup) {
    REQUIRE(U(AnyPopup)); REQUIRE(U(DestroyWindow)); REQUIRE(U(ShowWindow));
    HWND owner = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    REQUIRE(owner);
    HWND pop = g0_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 60, 40, owner);
    REQUIRE(pop);
    CHECK(U(IsWindowVisible)(pop));
    EXPECT_EQ(U(GetWindow)(pop, GW_OWNER), owner);
    EXPECT_EQ(U(AnyPopup)(), TRUE);
    U(DestroyWindow)(pop);
    U(DestroyWindow)(owner);
}
TEST(gap0_anypopup_hidden_owned_not_counted) {
    REQUIRE(U(AnyPopup)); REQUIRE(U(DestroyWindow));
    BOOL before = U(AnyPopup)();
    HWND owner = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND pop = g0_mk(WS_POPUP, WS_EX_TOOLWINDOW, 0, 0, 60, 40, owner);   /* not visible */
    REQUIRE(owner); REQUIRE(pop);
    BOOL after = U(AnyPopup)();
    if (!before) EXPECT_EQ(after, FALSE);
    else t_note("other owned visible windows already exist");
    U(DestroyWindow)(pop); U(DestroyWindow)(owner);
}
TEST(gap0_anypopup_unowned_visible_not_counted) {
    REQUIRE(U(AnyPopup)); REQUIRE(U(DestroyWindow));
    BOOL before = U(AnyPopup)();
    HWND w = g0_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 60, 40, NULL);
    REQUIRE(w);
    if (!before) EXPECT_EQ(U(AnyPopup)(), FALSE);
    else t_note("other owned visible windows already exist");
    U(DestroyWindow)(w);
}
TEST(gap0_anypopup_hide_owned) {
    REQUIRE(U(AnyPopup)); REQUIRE(U(DestroyWindow)); REQUIRE(U(ShowWindow));
    BOOL before = U(AnyPopup)();
    HWND owner = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND pop = g0_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 60, 40, owner);
    REQUIRE(owner); REQUIRE(pop);
    EXPECT_EQ(U(AnyPopup)(), TRUE);
    U(ShowWindow)(pop, SW_HIDE);
    if (!before) EXPECT_EQ(U(AnyPopup)(), FALSE);
    U(ShowWindow)(pop, SW_SHOWNOACTIVATE);
    EXPECT_EQ(U(AnyPopup)(), TRUE);
    U(DestroyWindow)(owner);
}
TEST(gap0_anypopup_child_not_counted) {
    REQUIRE(U(AnyPopup)); REQUIRE(U(DestroyWindow));
    BOOL before = U(AnyPopup)();
    HWND p = g0_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 100, 100, NULL);
    REQUIRE(p);
    HWND c = g0_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 20, 20, p);
    REQUIRE(c);
    if (!before) EXPECT_EQ(U(AnyPopup)(), FALSE);
    U(DestroyWindow)(p);
}
TEST(gap0_anypopup_destroyed_owner_removes_popup) {
    REQUIRE(U(AnyPopup)); REQUIRE(U(DestroyWindow));
    BOOL before = U(AnyPopup)();
    HWND owner = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND pop = g0_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 60, 40, owner);
    REQUIRE(owner); REQUIRE(pop);
    U(DestroyWindow)(owner);
    CHECK(!U(IsWindow)(pop));   /* owned windows die with their owner */
    if (!before) EXPECT_EQ(U(AnyPopup)(), FALSE);
}
TEST(gap0_anypopup_repeat) {
    REQUIRE(U(AnyPopup));
    BOOL f = U(AnyPopup)();
    int diff = 0;
    for (int i = 0; i < 500; i++) if (U(AnyPopup)() != f) diff++;
    if (diff) t_note("AnyPopup changed %d times (desktop activity)", diff);
    CHECK(1);
}

/* ---------- CascadeChildWindows ---------- */
static HWND cas_parent(void) { return g0_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 40, 40, 500, 400, NULL); }
static HWND cas_child(HWND p, int x, int y, DWORD extra) {
    return g0_mk(WS_CHILD | WS_VISIBLE | WS_CAPTION | WS_THICKFRAME | extra, 0, x, y, 120, 90, p);
}
TEST(gap0_cascade_invalid_window) {
    REQUIRE(UN(CascadeChildWindows));
    SET_ERR(0);
    EXPECT_EQ(UN(CascadeChildWindows)((HWND)(ULONG_PTR)0xDEAD0, 0), 0);
}
TEST(gap0_cascade_destroyed_window) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    U(DestroyWindow)(p);
    EXPECT_EQ(UN(CascadeChildWindows)(p, 0), 0);
}
TEST(gap0_cascade_no_children) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    EXPECT_EQ(UN(CascadeChildWindows)(p, 0), 0);
    U(DestroyWindow)(p);
}
TEST(gap0_cascade_three_children_count) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    HWND c[3];
    for (int i = 0; i < 3; i++) { c[i] = cas_child(p, 0, 0, 0); REQUIRE(c[i]); }
    EXPECT_EQ(UN(CascadeChildWindows)(p, 0), 3);
    U(DestroyWindow)(p);
}
TEST(gap0_cascade_positions_distinct) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    HWND c[3]; POINT pt[3];
    for (int i = 0; i < 3; i++) { c[i] = cas_child(p, 0, 0, 0); REQUIRE(c[i]); }
    UN(CascadeChildWindows)(p, 0);
    for (int i = 0; i < 3; i++) {
        RECT r; U(GetWindowRect)(c[i], &r);
        pt[i].x = r.left; pt[i].y = r.top;
        U(ScreenToClient)(p, &pt[i]);
    }
    for (int i = 0; i < 3; i++) for (int j = i + 1; j < 3; j++)
        CHECKF(pt[i].x != pt[j].x || pt[i].y != pt[j].y, "children %d,%d share position", i, j);
    U(DestroyWindow)(p);
}
TEST(gap0_cascade_positions_inside_parent) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    HWND c[3];
    for (int i = 0; i < 3; i++) { c[i] = cas_child(p, 300, 300, 0); REQUIRE(c[i]); }
    UN(CascadeChildWindows)(p, 0);
    RECT pc; U(GetClientRect)(p, &pc);
    for (int i = 0; i < 3; i++) {
        RECT r; U(GetWindowRect)(c[i], &r);
        POINT tl = { r.left, r.top }; U(ScreenToClient)(p, &tl);
        CHECKF(tl.x >= 0 && tl.y >= 0 && tl.x < pc.right && tl.y < pc.bottom, "child %d at %ld,%ld", i, tl.x, tl.y);
    }
    U(DestroyWindow)(p);
}
TEST(gap0_cascade_hidden_child_skipped) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    HWND a = cas_child(p, 0, 0, 0), b = g0_mk(WS_CHILD | WS_CAPTION | WS_THICKFRAME, 0, 0, 0, 120, 90, p), c = cas_child(p, 0, 0, 0);
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    EXPECT_EQ(UN(CascadeChildWindows)(p, 0), 2);
    U(DestroyWindow)(p);
}
TEST(gap0_cascade_skipdisabled) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    HWND a = cas_child(p, 0, 0, 0), b = cas_child(p, 0, 0, WS_DISABLED), c = cas_child(p, 0, 0, 0);
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    EXPECT_EQ(UN(CascadeChildWindows)(p, MDITILE_SKIPDISABLED), 2);
    EXPECT_EQ(UN(CascadeChildWindows)(p, 0), 3);
    U(DestroyWindow)(p);
}
TEST(gap0_cascade_matches_cascadewindows) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(CascadeWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    HWND c[3];
    for (int i = 0; i < 3; i++) { c[i] = cas_child(p, 7 * i, 5 * i, 0); REQUIRE(c[i]); }
    WORD a = UN(CascadeChildWindows)(p, 0);
    RECT ra[3]; for (int i = 0; i < 3; i++) U(GetWindowRect)(c[i], &ra[i]);
    WORD b = U(CascadeWindows)(p, 0, NULL, 0, NULL);
    RECT rb[3]; for (int i = 0; i < 3; i++) U(GetWindowRect)(c[i], &rb[i]);
    EXPECT_EQ(a, b);
    CHECK(!memcmp(ra, rb, sizeof ra));
    U(DestroyWindow)(p);
}
TEST(gap0_cascade_repeat_idempotent) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    HWND c[2];
    for (int i = 0; i < 2; i++) { c[i] = cas_child(p, 0, 0, 0); REQUIRE(c[i]); }
    UN(CascadeChildWindows)(p, 0);
    RECT r1[2], r2[2];
    for (int i = 0; i < 2; i++) U(GetWindowRect)(c[i], &r1[i]);
    UN(CascadeChildWindows)(p, 0);
    for (int i = 0; i < 2; i++) U(GetWindowRect)(c[i], &r2[i]);
    CHECK(!memcmp(r1, r2, sizeof r1));
    U(DestroyWindow)(p);
}
TEST(gap0_cascade_minimized_child_skipped) {
    REQUIRE(UN(CascadeChildWindows)); REQUIRE(U(DestroyWindow)); REQUIRE(U(ShowWindow));
    HWND p = cas_parent();
    REQUIRE(p);
    HWND a = cas_child(p, 0, 0, 0), b = cas_child(p, 0, 0, 0);
    REQUIRE(a); REQUIRE(b);
    U(ShowWindow)(b, SW_SHOWMINNOACTIVE);
    EXPECT_EQ(UN(CascadeChildWindows)(p, 0), 1);
    U(DestroyWindow)(p);
}

/* ---------- CalculatePopupWindowPosition ---------- */
static RECT prim_mon(void) { RECT r = { 0, 0, U(GetSystemMetrics)(SM_CXSCREEN), U(GetSystemMetrics)(SM_CYSCREEN) }; return r; }
TEST(gap0_calcpopup_basic_size_preserved) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(GetSystemMetrics));
    RECT m = prim_mon();
    POINT a = { m.right / 4, m.bottom / 4 }; SIZE s = { 120, 70 };
    RECT out = { -1, -1, -1, -1 };
    SET_ERR(0);
    BOOL ok = U(CalculatePopupWindowPosition)(&a, &s, TPM_LEFTALIGN | TPM_TOPALIGN, NULL, &out);
    CHECK(ok);
    CHECKF(out.right - out.left == 120 && out.bottom - out.top == 70, "got %ld,%ld,%ld,%ld", out.left, out.top, out.right, out.bottom);
}
TEST(gap0_calcpopup_left_top_aligned) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(GetSystemMetrics));
    RECT m = prim_mon();
    POINT a = { m.right / 4, m.bottom / 4 }; SIZE s = { 120, 70 };
    RECT out = { 0 };
    BOOL ok = U(CalculatePopupWindowPosition)(&a, &s, TPM_LEFTALIGN | TPM_TOPALIGN, NULL, &out);
    CHECK(ok);
    EXPECT_EQ(out.left, a.x);
    EXPECT_EQ(out.top, a.y);
}
TEST(gap0_calcpopup_default_flags_equal_left_top) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(GetSystemMetrics));
    RECT m = prim_mon();
    POINT a = { m.right / 3, m.bottom / 3 }; SIZE s = { 90, 50 };
    RECT o0 = { 0 }, o1 = { 0 };
    CHECK(U(CalculatePopupWindowPosition)(&a, &s, 0, NULL, &o0));
    CHECK(U(CalculatePopupWindowPosition)(&a, &s, TPM_LEFTALIGN | TPM_TOPALIGN, NULL, &o1));
    CHECK(!memcmp(&o0, &o1, sizeof o0));
}
TEST(gap0_calcpopup_center_right_bottom_observed) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(GetSystemMetrics));
    RECT m = prim_mon();
    POINT a = { m.right / 2, m.bottom / 2 }; SIZE s = { 100, 60 };
    static const struct { UINT f; const char *n; } t[] = {
        { TPM_CENTERALIGN | TPM_VCENTERALIGN, "center" }, { TPM_RIGHTALIGN | TPM_BOTTOMALIGN, "right-bottom" },
        { TPM_RIGHTALIGN | TPM_TOPALIGN, "right-top" }, { TPM_LEFTALIGN | TPM_BOTTOMALIGN, "left-bottom" },
        { TPM_CENTERALIGN | TPM_TOPALIGN, "hcenter-top" }, { TPM_LEFTALIGN | TPM_VCENTERALIGN, "left-vcenter" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT out = { 0 };
        BOOL ok = U(CalculatePopupWindowPosition)(&a, &s, t[i].f, NULL, &out);
        CHECKF(ok, "%s", t[i].n);
        CHECKF(out.right - out.left == 100 && out.bottom - out.top == 60, "%s size", t[i].n);
        t_note("popup %s anchor %ld,%ld -> %ld,%ld,%ld,%ld", t[i].n, a.x, a.y, out.left, out.top, out.right, out.bottom);
    }
}
TEST(gap0_calcpopup_clamped_to_virtual_screen) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(GetSystemMetrics));
    int vx = U(GetSystemMetrics)(SM_XVIRTUALSCREEN), vy = U(GetSystemMetrics)(SM_YVIRTUALSCREEN);
    int vw = U(GetSystemMetrics)(SM_CXVIRTUALSCREEN), vh = U(GetSystemMetrics)(SM_CYVIRTUALSCREEN);
    static const POINT a[] = { { 100000, 100000 }, { -100000, -100000 }, { 100000, -5 }, { -5, 100000 } };
    SIZE s = { 150, 80 };
    for (unsigned i = 0; i < 4; i++) {
        RECT out = { 0 };
        BOOL ok = U(CalculatePopupWindowPosition)(&a[i], &s, TPM_LEFTALIGN | TPM_TOPALIGN, NULL, &out);
        CHECKF(ok, "anchor %u", i);
        CHECKF(out.left >= vx && out.top >= vy && out.right <= vx + vw && out.bottom <= vy + vh,
               "anchor %u -> %ld,%ld,%ld,%ld", i, out.left, out.top, out.right, out.bottom);
        CHECKF(out.right - out.left == 150 && out.bottom - out.top == 80, "anchor %u size", i);
    }
}
TEST(gap0_calcpopup_workarea_flag) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(SystemParametersInfoW));
    RECT wa; U(SystemParametersInfoW)(SPI_GETWORKAREA, 0, &wa, 0);
    POINT a = { 100000, 100000 }; SIZE s = { 150, 80 };
    RECT out = { 0 };
    BOOL ok = U(CalculatePopupWindowPosition)(&a, &s, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_WORKAREA, NULL, &out);
    CHECK(ok);
    CHECKF(out.right <= wa.right && out.bottom <= wa.bottom && out.left >= wa.left && out.top >= wa.top,
           "work %ld,%ld,%ld,%ld out %ld,%ld,%ld,%ld", wa.left, wa.top, wa.right, wa.bottom, out.left, out.top, out.right, out.bottom);
}
TEST(gap0_calcpopup_exclude_rect_avoided) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(GetSystemMetrics));
    RECT m = prim_mon();
    POINT a = { m.right / 2, m.bottom / 2 }; SIZE s = { 100, 60 };
    RECT ex = { a.x - 20, a.y - 20, a.x + 20, a.y + 20 };
    RECT out = { 0 };
    BOOL ok = U(CalculatePopupWindowPosition)(&a, &s, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERTICAL, &ex, &out);
    CHECK(ok);
    RECT x;
    CHECKF(!U(IntersectRect)(&x, &ex, &out), "popup %ld,%ld,%ld,%ld overlaps exclude", out.left, out.top, out.right, out.bottom);
    CHECK(out.right - out.left == 100 && out.bottom - out.top == 60);
    memset(&out, 0, sizeof out);
    ok = U(CalculatePopupWindowPosition)(&a, &s, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_HORIZONTAL, &ex, &out);
    CHECK(ok);
    CHECKF(!U(IntersectRect)(&x, &ex, &out), "horizontal popup overlaps exclude");
}
TEST(gap0_calcpopup_zero_size) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(GetSystemMetrics));
    RECT m = prim_mon();
    POINT a = { m.right / 4, m.bottom / 4 }; SIZE s = { 0, 0 };
    RECT out = { 9, 9, 9, 9 };
    SET_ERR(0);
    BOOL ok = U(CalculatePopupWindowPosition)(&a, &s, 0, NULL, &out);
    t_note("zero-size popup: ret %d err %lu rect %ld,%ld,%ld,%ld", ok, ok ? 0 : GetLastError(), out.left, out.top, out.right, out.bottom);
    CHECK(1);
}
TEST(gap0_calcpopup_repeat_deterministic) {
    REQUIRE(U(CalculatePopupWindowPosition)); REQUIRE(U(GetSystemMetrics));
    RECT m = prim_mon();
    POINT a = { m.right / 3, m.bottom / 3 }; SIZE s = { 111, 55 };
    RECT first = { 0 };
    CHECK(U(CalculatePopupWindowPosition)(&a, &s, TPM_CENTERALIGN, NULL, &first));
    int diff = 0;
    for (int i = 0; i < 200; i++) {
        RECT o = { 0 };
        if (!U(CalculatePopupWindowPosition)(&a, &s, TPM_CENTERALIGN, NULL, &o) || memcmp(&o, &first, sizeof o)) diff++;
    }
    EXPECT_EQ(diff, 0);
}

/* ---------- DeferWindowPosAndBand ---------- */
TEST(gap0_deferband_invalid_hdwp) {
    REQUIRE(UN(DeferWindowPosAndBand)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 40, 40, NULL);
    REQUIRE(w);
    SET_ERR(0);
    HDWP r = UN(DeferWindowPosAndBand)(NULL, w, NULL, 1, 2, 30, 30, SWP_NOZORDER | SWP_NOACTIVATE, 0, 0);
    CHECK(r == NULL);
    t_note("DeferWindowPosAndBand(NULL hdwp) err %lu", GetLastError());
    U(DestroyWindow)(w);
}
TEST(gap0_deferband_bogus_hdwp) {
    REQUIRE(UN(DeferWindowPosAndBand)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 40, 40, NULL);
    REQUIRE(w);
    HDWP r = UN(DeferWindowPosAndBand)((HDWP)(ULONG_PTR)0x12345670, w, NULL, 1, 2, 30, 30, SWP_NOZORDER | SWP_NOACTIVATE, 0, 0);
    CHECK(r == NULL);
    U(DestroyWindow)(w);
}
TEST(gap0_deferband_moves_window) {
    REQUIRE(UN(DeferWindowPosAndBand)); REQUIRE(U(DestroyWindow)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(EndDeferWindowPos));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 40, 40, NULL);
    REQUIRE(w);
    HDWP h = U(BeginDeferWindowPos)(1);
    REQUIRE(h);
    h = UN(DeferWindowPosAndBand)(h, w, NULL, 31, 47, 77, 66, SWP_NOZORDER | SWP_NOACTIVATE, 0, 0);
    CHECK(h != NULL);
    if (!h) { U(DestroyWindow)(w); return; }
    RECT r; U(GetWindowRect)(w, &r);
    CHECK(r.left == 0 && r.right == 40);          /* nothing applied yet */
    CHECK(U(EndDeferWindowPos)(h));
    U(GetWindowRect)(w, &r);
    CHECK(r.left == 31 && r.top == 47 && r.right == 108 && r.bottom == 113);
    U(DestroyWindow)(w);
}
TEST(gap0_deferband_two_windows_mixed_with_deferwindowpos) {
    REQUIRE(UN(DeferWindowPosAndBand)); REQUIRE(U(DestroyWindow)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND a = g0_mk(WS_POPUP, 0, 0, 0, 40, 40, NULL), b = g0_mk(WS_POPUP, 0, 0, 0, 40, 40, NULL);
    REQUIRE(a); REQUIRE(b);
    HDWP h = U(BeginDeferWindowPos)(2);
    REQUIRE(h);
    h = U(DeferWindowPos)(h, a, NULL, 10, 11, 50, 51, SWP_NOZORDER | SWP_NOACTIVATE);
    REQUIRE(h);
    h = UN(DeferWindowPosAndBand)(h, b, NULL, 20, 21, 60, 61, SWP_NOZORDER | SWP_NOACTIVATE, 0, 0);
    REQUIRE(h);
    CHECK(U(EndDeferWindowPos)(h));
    RECT ra, rb; U(GetWindowRect)(a, &ra); U(GetWindowRect)(b, &rb);
    CHECK(ra.left == 10 && ra.top == 11 && ra.right == 60 && ra.bottom == 62);
    CHECK(rb.left == 20 && rb.top == 21 && rb.right == 80 && rb.bottom == 82);
    U(DestroyWindow)(a); U(DestroyWindow)(b);
}
TEST(gap0_deferband_invalid_window) {
    REQUIRE(UN(DeferWindowPosAndBand)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(EndDeferWindowPos));
    HDWP h = U(BeginDeferWindowPos)(1);
    REQUIRE(h);
    SET_ERR(0);
    HDWP r = UN(DeferWindowPosAndBand)(h, (HWND)(ULONG_PTR)0xDEAD0, NULL, 0, 0, 10, 10, SWP_NOZORDER, 0, 0);
    CHECK(r == NULL);
    t_note("DeferWindowPosAndBand(bogus hwnd) err %lu", GetLastError());
}
TEST(gap0_deferband_nomove_nosize_flags) {
    REQUIRE(UN(DeferWindowPosAndBand)); REQUIRE(U(DestroyWindow)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(EndDeferWindowPos));
    HWND w = g0_mk(WS_POPUP, 0, 5, 6, 40, 30, NULL);
    REQUIRE(w);
    HDWP h = U(BeginDeferWindowPos)(1);
    REQUIRE(h);
    h = UN(DeferWindowPosAndBand)(h, w, NULL, 100, 100, 100, 100, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE, 0, 0);
    CHECK(h != NULL);
    if (h) CHECK(U(EndDeferWindowPos)(h));
    RECT r; U(GetWindowRect)(w, &r);
    CHECK(r.left == 5 && r.top == 6 && r.right == 45 && r.bottom == 36);
    U(DestroyWindow)(w);
}
