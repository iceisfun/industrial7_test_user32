/* misc_ui: window style / ex-style normalisation, GetWindowLongPtr family, handle properties */
#include "mu.h"

#define NEED_WND() REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow)); REQUIRE(U(IsWindow)); \
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrA)); if (!mu_register()) { t_skip("class"); return; }

TEST(misc_ui_style_overlapped_gets_windowedge) {
    NEED_WND();
    HWND h = mu_top();
    CHECK(h != NULL);
    if (!h) return;
    CHECK((mu_ex(h) & WS_EX_WINDOWEDGE) != 0);
    CHECK((mu_style(h) & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW);
    CHECK((mu_style(h) & WS_VISIBLE) == 0);
    mu_kill(h);
}

TEST(misc_ui_style_windowedge_rules) {
    NEED_WND();
    static const struct { DWORD style, ex; int edge; const char *what; } tab[] = {
        { WS_POPUP, 0, 0, "popup" },
        { WS_POPUP | WS_BORDER, 0, 0, "popup+border" },
        { WS_POPUP | WS_DLGFRAME, 0, 1, "popup+dlgframe" },
        { WS_POPUP | WS_THICKFRAME, 0, 1, "popup+thick" },
        { WS_POPUP | WS_CAPTION, 0, 1, "popup+caption" },
        { WS_POPUP, WS_EX_DLGMODALFRAME, 1, "popup+dlgmodalframe" },
        { WS_CAPTION, 0, 1, "caption" },
        { WS_THICKFRAME, 0, 1, "thickframe" },
    };
    for (int i = 0; i < (int)(sizeof tab / sizeof tab[0]); i++) {
        HWND h = mu_mk(tab[i].style, tab[i].ex, 10, 10, 100, 100, NULL);
        CHECKF(h != NULL, "%s", tab[i].what);
        if (!h) continue;
        int has = (mu_ex(h) & WS_EX_WINDOWEDGE) != 0;
        CHECKF(has == tab[i].edge, "%s ex=0x%llx", tab[i].what, (long long)mu_ex(h));
        mu_kill(h);
    }
}

TEST(misc_ui_style_child_edge_rules) {
    NEED_WND();
    HWND p = mu_mk(WS_POPUP, 0, 10, 10, 200, 200, NULL);
    CHECK(p != NULL);
    if (!p) return;
    HWND c1 = mu_mk(WS_CHILD, 0, 0, 0, 20, 20, p);
    HWND c2 = mu_mk(WS_CHILD | WS_THICKFRAME, 0, 0, 0, 20, 20, p);
    HWND c3 = mu_mk(WS_CHILD | WS_BORDER, 0, 0, 0, 20, 20, p);
    HWND c4 = mu_mk(WS_CHILD, WS_EX_CLIENTEDGE, 0, 0, 20, 20, p);
    CHECK(c1 && c2 && c3 && c4);
    if (c1) CHECK((mu_ex(c1) & WS_EX_WINDOWEDGE) == 0);
    if (c2) t_note("child+THICKFRAME WINDOWEDGE=%d", (mu_ex(c2) & WS_EX_WINDOWEDGE) != 0);
    if (c3) CHECK((mu_ex(c3) & WS_EX_WINDOWEDGE) == 0);
    if (c4) CHECK((mu_ex(c4) & WS_EX_CLIENTEDGE) != 0);
    mu_kill(p);
    CHECK(!c1 || !U(IsWindow)(c1));
}

TEST(misc_ui_style_exstyle_roundtrip_popup) {
    NEED_WND();
    static const struct { DWORD bit; const char *n; } tab[] = {
        { WS_EX_TOOLWINDOW, "TOOLWINDOW" }, { WS_EX_TOPMOST, "TOPMOST" }, { WS_EX_TRANSPARENT, "TRANSPARENT" },
        { WS_EX_LAYERED, "LAYERED" }, { WS_EX_NOACTIVATE, "NOACTIVATE" }, { WS_EX_ACCEPTFILES, "ACCEPTFILES" },
        { WS_EX_CONTEXTHELP, "CONTEXTHELP" }, { WS_EX_RTLREADING, "RTLREADING" }, { WS_EX_RIGHT, "RIGHT" },
        { WS_EX_LEFTSCROLLBAR, "LEFTSCROLLBAR" }, { WS_EX_LAYOUTRTL, "LAYOUTRTL" }, { WS_EX_COMPOSITED, "COMPOSITED" },
        { 0x00200000, "NOREDIRECTIONBITMAP" }, { WS_EX_APPWINDOW, "APPWINDOW" }, { WS_EX_CONTROLPARENT, "CONTROLPARENT" },
        { WS_EX_STATICEDGE, "STATICEDGE" }, { WS_EX_NOINHERITLAYOUT, "NOINHERITLAYOUT" },
        { WS_EX_NOPARENTNOTIFY, "NOPARENTNOTIFY" }, { WS_EX_CLIENTEDGE, "CLIENTEDGE" },
        { WS_EX_DLGMODALFRAME, "DLGMODALFRAME" }, { WS_EX_LTRREADING, "LTRREADING(0)" },
    };
    for (int i = 0; i < (int)(sizeof tab / sizeof tab[0]); i++) {
        HWND h = mu_mk(WS_POPUP, tab[i].bit, 10, 10, 100, 100, NULL);
        CHECKF(h != NULL, "%s", tab[i].n);
        if (!h) continue;
        CHECKF((mu_ex(h) & tab[i].bit) == tab[i].bit, "%s ex=0x%llx", tab[i].n, (long long)mu_ex(h));
        mu_kill(h);
    }
}

TEST(misc_ui_style_exstyle_set_roundtrip) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 100, 100, NULL);
    CHECK(h != NULL);
    if (!h) return;
    static const DWORD bits[] = { WS_EX_TOOLWINDOW, WS_EX_NOACTIVATE, WS_EX_APPWINDOW, WS_EX_TRANSPARENT,
        WS_EX_ACCEPTFILES, WS_EX_CONTROLPARENT, WS_EX_RTLREADING, WS_EX_RIGHT, WS_EX_LEFTSCROLLBAR, WS_EX_LAYERED,
        WS_EX_COMPOSITED, WS_EX_LAYOUTRTL };
    for (int i = 0; i < (int)(sizeof bits / sizeof bits[0]); i++) {
        LONG_PTR old = mu_ex(h);
        LONG_PTR r = U(SetWindowLongPtrA)(h, GWL_EXSTYLE, old | bits[i]);
        CHECKF(r == old, "bit 0x%lx: ret 0x%llx old 0x%llx", bits[i], (long long)r, (long long)old);
        CHECKF((mu_ex(h) & bits[i]) == bits[i], "bit 0x%lx not set", bits[i]);
        r = U(SetWindowLongPtrA)(h, GWL_EXSTYLE, old);
        CHECK((r & bits[i]) == bits[i]);
        CHECKF((mu_ex(h) & bits[i]) == (old & bits[i]), "bit 0x%lx not cleared", bits[i]);
    }
    mu_kill(h);
}

TEST(misc_ui_style_topmost_via_setwindowlong) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    U(SetWindowLongPtrA)(h, GWL_EXSTYLE, mu_ex(h) | WS_EX_TOPMOST);
    t_note("SetWindowLong(EXSTYLE|TOPMOST) -> ex has TOPMOST=%d", (mu_ex(h) & WS_EX_TOPMOST) != 0);
    mu_kill(h);
}

TEST(misc_ui_style_setwindowlong_returns_old_style) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 100, 100, NULL);
    CHECK(h != NULL);
    if (!h) return;
    LONG_PTR s0 = mu_style(h);
    CHECK((s0 & WS_POPUP) == (LONG_PTR)(DWORD)WS_POPUP || (DWORD)s0 == WS_POPUP);
    LONG_PTR r = U(SetWindowLongPtrA)(h, GWL_STYLE, s0 | WS_TABSTOP | WS_GROUP);
    CHECK((DWORD)r == (DWORD)s0);
    CHECK((mu_style(h) & (WS_TABSTOP | WS_GROUP)) == (WS_TABSTOP | WS_GROUP));
    r = U(SetWindowLongPtrA)(h, GWL_STYLE, s0);
    CHECK(((DWORD)r & (WS_TABSTOP | WS_GROUP)) == (WS_TABSTOP | WS_GROUP));
    CHECK(mu_style(h) == s0);
    /* style reads are zero/sign consistent: WS_POPUP is bit 31 */
    LONG v32 = U(GetWindowLongA)(h, GWL_STYLE);
    CHECK((DWORD)v32 == (DWORD)mu_style(h));
    mu_kill(h);
}

TEST(misc_ui_style_visible_sync_with_show) {
    NEED_WND(); REQUIRE(U(ShowWindow)); REQUIRE(U(IsWindowVisible));
    HWND h = mu_mk(WS_POPUP, WS_EX_NOACTIVATE, 10, 10, 100, 100, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(!U(IsWindowVisible)(h));
    CHECK(U(ShowWindow)(h, SW_SHOWNA) == 0);          /* was hidden */
    CHECK((mu_style(h) & WS_VISIBLE) != 0);
    CHECK(U(IsWindowVisible)(h));
    CHECK(U(ShowWindow)(h, SW_SHOWNA) != 0);          /* was visible */
    CHECK(U(ShowWindow)(h, SW_HIDE) != 0);
    CHECK((mu_style(h) & WS_VISIBLE) == 0);
    CHECK(!U(IsWindowVisible)(h));
    CHECK(U(ShowWindow)(h, SW_HIDE) == 0);
    mu_kill(h);
}

TEST(misc_ui_style_created_visible) {
    NEED_WND(); REQUIRE(U(IsWindowVisible));
    HWND h = mu_mk(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 10, 10, 60, 60, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK((mu_style(h) & WS_VISIBLE) != 0);
    CHECK(U(IsWindowVisible)(h));
    mu_kill(h);
}

TEST(misc_ui_style_visible_bit_via_setwindowlong) {
    NEED_WND(); REQUIRE(U(IsWindowVisible));
    HWND h = mu_mk(WS_POPUP, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 10, 10, 60, 60, NULL);
    if (!h) { CHECK(0); return; }
    U(SetWindowLongPtrA)(h, GWL_STYLE, mu_style(h) | WS_VISIBLE);
    t_note("SetWindowLong(+WS_VISIBLE): style visible=%d IsWindowVisible=%d", (mu_style(h) & WS_VISIBLE) != 0, U(IsWindowVisible)(h));
    U(SetWindowLongPtrA)(h, GWL_STYLE, mu_style(h) & ~WS_VISIBLE);
    CHECK(!U(IsWindowVisible)(h));
    mu_kill(h);
}

TEST(misc_ui_style_disabled_sync_with_enablewindow) {
    NEED_WND(); REQUIRE(U(EnableWindow)); REQUIRE(U(IsWindowEnabled));
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 60, 60, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(IsWindowEnabled)(h));
    CHECK((mu_style(h) & WS_DISABLED) == 0);
    CHECK(U(EnableWindow)(h, FALSE) == 0);      /* was enabled */
    CHECK((mu_style(h) & WS_DISABLED) != 0);
    CHECK(!U(IsWindowEnabled)(h));
    CHECK(U(EnableWindow)(h, FALSE) != 0);      /* already disabled */
    CHECK(U(EnableWindow)(h, TRUE) != 0);       /* was disabled */
    CHECK((mu_style(h) & WS_DISABLED) == 0);
    CHECK(U(IsWindowEnabled)(h));
    CHECK(U(EnableWindow)(h, TRUE) == 0);
    CHECK(U(EnableWindow)(h, 5) == 0);          /* any nonzero enables */
    CHECK(U(IsWindowEnabled)(h));
    mu_kill(h);
}

static int g_enable_msgs, g_enable_last;
static int hook_enable(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT *r)
{
    if (m == WM_ENABLE) { g_enable_msgs++; g_enable_last = (int)w; }
    return 0;
}
TEST(misc_ui_style_wm_enable_notifications) {
    NEED_WND(); REQUIRE(U(EnableWindow));
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 60, 60, NULL);
    if (!h) { CHECK(0); return; }
    mu_hook = hook_enable; g_enable_msgs = 0;
    U(EnableWindow)(h, FALSE);
    CHECK(g_enable_msgs == 1 && g_enable_last == 0);
    U(EnableWindow)(h, FALSE);
    CHECK(g_enable_msgs == 1);                  /* no change, no message */
    U(EnableWindow)(h, TRUE);
    CHECK(g_enable_msgs == 2 && g_enable_last == 1);
    U(EnableWindow)(h, TRUE);
    CHECK(g_enable_msgs == 2);
    mu_hook = NULL;
    mu_kill(h);
}

TEST(misc_ui_style_created_disabled) {
    NEED_WND(); REQUIRE(U(IsWindowEnabled));
    HWND h = mu_mk(WS_POPUP | WS_DISABLED, 0, 10, 10, 60, 60, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(!U(IsWindowEnabled)(h));
    CHECK(U(EnableWindow)(h, TRUE) != 0);
    mu_kill(h);
}

TEST(misc_ui_style_minimize_maximize_bits) {
    NEED_WND(); REQUIRE(U(ShowWindow)); REQUIRE(U(IsIconic)); REQUIRE(U(IsZoomed));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, WS_EX_NOACTIVATE, 50, 50, 300, 200, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(!U(IsIconic)(h) && !U(IsZoomed)(h));
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    CHECK((mu_style(h) & WS_MINIMIZE) != 0);
    CHECK((mu_style(h) & WS_VISIBLE) != 0);
    CHECK(U(IsIconic)(h));
    CHECK(!U(IsZoomed)(h));
    U(ShowWindow)(h, SW_RESTORE);
    CHECK((mu_style(h) & (WS_MINIMIZE | WS_MAXIMIZE)) == 0);
    CHECK(!U(IsIconic)(h));
    U(ShowWindow)(h, SW_SHOWMAXIMIZED);
    CHECK((mu_style(h) & WS_MAXIMIZE) != 0);
    CHECK(U(IsZoomed)(h));
    CHECK(!U(IsIconic)(h));
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    CHECK(U(IsIconic)(h));
    t_note("maximized then minimized: IsZoomed=%d (restore-to-maximized)", U(IsZoomed)(h));
    U(ShowWindow)(h, SW_RESTORE);
    CHECK((mu_style(h) & WS_MINIMIZE) == 0);
    U(ShowWindow)(h, SW_RESTORE);
    CHECK((mu_style(h) & (WS_MINIMIZE | WS_MAXIMIZE)) == 0);
    mu_kill(h);
}

TEST(misc_ui_style_created_minimized_flag) {
    NEED_WND(); REQUIRE(U(IsIconic));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_MINIMIZE, 0, 50, 50, 300, 200, NULL);
    CHECK(h != NULL);
    if (!h) return;
    t_note("created with WS_MINIMIZE: style bit=%d IsIconic=%d", (mu_style(h) & WS_MINIMIZE) != 0, U(IsIconic)(h));
    CHECK(U(IsIconic)(h) == ((mu_style(h) & WS_MINIMIZE) != 0));
    mu_kill(h);
}

TEST(misc_ui_style_created_maximized_flag) {
    NEED_WND(); REQUIRE(U(IsZoomed));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_MAXIMIZE, 0, 50, 50, 300, 200, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(IsZoomed)(h) == ((mu_style(h) & WS_MAXIMIZE) != 0));
    t_note("created with WS_MAXIMIZE: style bit=%d", (mu_style(h) & WS_MAXIMIZE) != 0);
    mu_kill(h);
}

TEST(misc_ui_style_minimize_maximize_boxes_buttons) {
    NEED_WND();
    HWND h = mu_mk(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0, 50, 50, 300, 200, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK((mu_style(h) & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) == 0);
    U(SetWindowLongPtrA)(h, GWL_STYLE, mu_style(h) | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    CHECK((mu_style(h) & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) == (WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
    mu_kill(h);
}

/* ---------- WM_STYLECHANGING / CHANGED ---------- */
static struct { int n_ing, n_ed; WPARAM w_ing, w_ed; DWORD old_ing, new_ing, old_ed, new_ed; DWORD force_clear; } sc;
static int hook_style(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT *r)
{
    STYLESTRUCT *ss = (STYLESTRUCT *)l;
    if (m == WM_STYLECHANGING) {
        sc.n_ing++; sc.w_ing = w; sc.old_ing = ss->styleOld; sc.new_ing = ss->styleNew;
        ss->styleNew &= ~sc.force_clear;
        *r = 0; return 1;
    }
    if (m == WM_STYLECHANGED) {
        sc.n_ed++; sc.w_ed = w; sc.old_ed = ss->styleOld; sc.new_ed = ss->styleNew;
        *r = 0; return 1;
    }
    return 0;
}
TEST(misc_ui_style_wm_stylechanging_data) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 60, 60, NULL);
    if (!h) { CHECK(0); return; }
    memset(&sc, 0, sizeof sc);
    mu_hook = hook_style;
    DWORD old = (DWORD)mu_style(h);
    U(SetWindowLongPtrA)(h, GWL_STYLE, (LONG_PTR)(DWORD)(old | WS_TABSTOP));
    CHECK(sc.n_ing == 1 && sc.n_ed == 1);
    CHECK(sc.w_ing == (WPARAM)GWL_STYLE && sc.w_ed == (WPARAM)GWL_STYLE);
    CHECK(sc.old_ing == old && sc.new_ing == (old | WS_TABSTOP));
    CHECK(sc.old_ed == old && sc.new_ed == (old | WS_TABSTOP));
    mu_hook = NULL;
    mu_kill(h);
}
TEST(misc_ui_style_wm_stylechanging_exstyle_data) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 60, 60, NULL);
    if (!h) { CHECK(0); return; }
    memset(&sc, 0, sizeof sc);
    mu_hook = hook_style;
    DWORD old = (DWORD)mu_ex(h);
    U(SetWindowLongPtrA)(h, GWL_EXSTYLE, (LONG_PTR)(old | WS_EX_TOOLWINDOW));
    CHECK(sc.n_ing == 1 && sc.n_ed == 1);
    CHECK(sc.w_ing == (WPARAM)(ULONG_PTR)(LONG_PTR)GWL_EXSTYLE);
    CHECK(sc.w_ed == (WPARAM)(ULONG_PTR)(LONG_PTR)GWL_EXSTYLE);
    CHECK(sc.old_ing == old);
    CHECK((sc.new_ing & WS_EX_TOOLWINDOW) != 0);
    CHECK((sc.new_ed & WS_EX_TOOLWINDOW) != 0);
    mu_hook = NULL;
    mu_kill(h);
}
TEST(misc_ui_style_wm_stylechanging_can_veto_bits) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 60, 60, NULL);
    if (!h) { CHECK(0); return; }
    memset(&sc, 0, sizeof sc);
    sc.force_clear = WS_TABSTOP;
    mu_hook = hook_style;
    DWORD old = (DWORD)mu_style(h);
    U(SetWindowLongPtrA)(h, GWL_STYLE, (LONG_PTR)(DWORD)(old | WS_TABSTOP | WS_GROUP));
    CHECK((mu_style(h) & WS_TABSTOP) == 0);
    CHECK((mu_style(h) & WS_GROUP) != 0);
    CHECK((sc.new_ed & WS_TABSTOP) == 0);
    mu_hook = NULL;
    mu_kill(h);
}
TEST(misc_ui_style_wm_stylechanged_same_value) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 60, 60, NULL);
    if (!h) { CHECK(0); return; }
    memset(&sc, 0, sizeof sc);
    mu_hook = hook_style;
    U(SetWindowLongPtrA)(h, GWL_STYLE, mu_style(h));
    t_note("same-value SetWindowLong: STYLECHANGING=%d STYLECHANGED=%d", sc.n_ing, sc.n_ed);
    mu_hook = NULL;
    mu_kill(h);
}

/* ---------- FRAMECHANGED ---------- */
TEST(misc_ui_style_framechanged_recalc_client) {
    NEED_WND(); REQUIRE(U(GetClientRect)); REQUIRE(U(SetWindowPos)); REQUIRE(U(GetSystemMetrics));
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 200, 150, NULL);
    if (!h) { CHECK(0); return; }
    RECT c0, c1, c2;
    U(GetClientRect)(h, &c0);
    CHECK(c0.right == 200 && c0.bottom == 150);
    U(SetWindowLongPtrA)(h, GWL_STYLE, mu_style(h) | WS_BORDER);
    U(GetClientRect)(h, &c1);
    CHECK(c1.right == c0.right && c1.bottom == c0.bottom);   /* not recalculated yet */
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED));
    U(GetClientRect)(h, &c2);
    CHECK(c2.right < c0.right && c2.bottom < c0.bottom);
    int bx = U(GetSystemMetrics)(SM_CXBORDER), by = U(GetSystemMetrics)(SM_CYBORDER);
    if (c2.right != c0.right - 2 * bx || c2.bottom != c0.bottom - 2 * by)
        t_note("client after border: %ldx%ld (borders %d,%d)", c2.right, c2.bottom, bx, by);
    mu_kill(h);
}
TEST(misc_ui_style_framechanged_sends_nccalcsize) {
    NEED_WND(); REQUIRE(U(SetWindowPos));
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 200, 150, NULL);
    if (!h) { CHECK(0); return; }
    mu_log_reset(); mu_logging = 1;
    U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    int a = mu_count(WM_NCCALCSIZE);
    mu_log_reset();
    U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    int b = mu_count(WM_NCCALCSIZE);
    CHECK(b >= 1);
    t_note("NCCALCSIZE without FRAMECHANGED: %d", a);
    int i = mu_find(WM_NCCALCSIZE);
    if (i >= 0) CHECK(mu_log[i].w == TRUE);
    int j = mu_find(WM_WINDOWPOSCHANGING);
    if (j >= 0) {
        WINDOWPOS *wp = (WINDOWPOS *)mu_log[j].l;   /* pointer no longer valid after return; only check existence */
        (void)wp;
    }
    CHECK(mu_count(WM_WINDOWPOSCHANGING) >= 1 && mu_count(WM_WINDOWPOSCHANGED) >= 1);
    mu_logging = 0;
    mu_kill(h);
}

/* ---------- GetWindowLongPtr family ---------- */
TEST(misc_ui_gwl_invalid_handle_errors) {
    NEED_WND();
    HWND bad[] = { NULL, BOGUS_HWND, (HWND)(ULONG_PTR)0x6FFF0123 };
    int idx[] = { GWL_STYLE, GWL_EXSTYLE, GWLP_USERDATA, GWLP_ID, GWLP_HINSTANCE, GWLP_WNDPROC, 0 };
    for (int i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++)
        for (int j = 0; j < (int)(sizeof idx / sizeof idx[0]); j++) {
            SetLastError(0xdeadbeef);
            LONG_PTR r = U(GetWindowLongPtrA)(bad[i], idx[j]);
            CHECKF(r == 0, "h=%p idx=%d r=%llx", bad[i], idx[j], (long long)r);
            CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "h=%p idx=%d err=%lu", bad[i], idx[j], GetLastError());
        }
}
TEST(misc_ui_gwl_set_invalid_handle_errors) {
    NEED_WND();
    HWND bad[] = { NULL, BOGUS_HWND, (HWND)(ULONG_PTR)0x6FFF0123 };
    int idx[] = { GWL_STYLE, GWLP_USERDATA, GWL_EXSTYLE, 0 };
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++) {
            SetLastError(0xdeadbeef);
            LONG_PTR r = U(SetWindowLongPtrA)(bad[i], idx[j], 5);
            CHECKF(r == 0, "h=%p idx=%d r=%llx", bad[i], idx[j], (long long)r);
            CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "h=%p idx=%d err=%lu", bad[i], idx[j], GetLastError());
        }
}
TEST(misc_ui_gwl_destroyed_handle) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 10, 10, 60, 60, NULL);
    CHECK(h != NULL); if (!h) return;
    U(SetWindowLongPtrA)(h, GWLP_USERDATA, 77);
    CHECK(U(DestroyWindow)(h));
    CHECK(!U(IsWindow)(h));
    SetLastError(0);
    CHECK(U(GetWindowLongPtrA)(h, GWLP_USERDATA) == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(U(SetWindowLongPtrA)(h, GWLP_USERDATA, 5) == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(DestroyWindow)(h));
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
}
TEST(misc_ui_gwl_bad_index) {
    NEED_WND();
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    int idx[] = { 16, 17, 20, 24, 100, 5000, -1, -2, -3, -5, -7, -9, -10, -11, -13, -14, -15, -17, -19, -22, -100, 0x7fffffff, (int)0x80000000 };
    for (int i = 0; i < (int)(sizeof idx / sizeof idx[0]); i++) {
        SetLastError(0);
        LONG_PTR r = U(GetWindowLongPtrA)(h, idx[i]);
        CHECKF(r == 0, "idx=%d r=%llx", idx[i], (long long)r);
        CHECKF(GetLastError() == ERROR_INVALID_INDEX, "idx=%d err=%lu", idx[i], GetLastError());
        SetLastError(0);
        r = U(SetWindowLongPtrA)(h, idx[i], 3);
        CHECKF(r == 0, "set idx=%d r=%llx", idx[i], (long long)r);
        CHECKF(GetLastError() == ERROR_INVALID_INDEX, "set idx=%d err=%lu", idx[i], GetLastError());
    }
    mu_kill(h);
}
TEST(misc_ui_gwl_extra_bytes_ptr) {
    NEED_WND();
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    CHECK(U(GetWindowLongPtrA)(h, 0) == 0);
    CHECK(U(GetWindowLongPtrA)(h, 8) == 0);
    SetLastError(0);
    CHECK(U(SetWindowLongPtrA)(h, 0, (LONG_PTR)0x1122334455667788LL) == 0);
    CHECK(U(SetWindowLongPtrA)(h, 8, (LONG_PTR)-2) == 0);
    CHECK(U(GetWindowLongPtrA)(h, 0) == (LONG_PTR)0x1122334455667788LL);
    CHECK(U(GetWindowLongPtrA)(h, 8) == (LONG_PTR)-2);
    CHECK(U(SetWindowLongPtrA)(h, 0, 9) == (LONG_PTR)0x1122334455667788LL);
    /* 32-bit halves, little endian */
    U(SetWindowLongPtrA)(h, 8, (LONG_PTR)0x1122334455667788LL);
    CHECK((DWORD)U(GetWindowLongA)(h, 8) == 0x55667788);
    CHECK((DWORD)U(GetWindowLongA)(h, 12) == 0x11223344);
    SetLastError(0);
    CHECK(U(GetWindowLongPtrA)(h, 16) == 0);
    CHECK(GetLastError() == ERROR_INVALID_INDEX);
    /* a pointer read straddling the end fails */
    SetLastError(0);
    LONG_PTR r = U(GetWindowLongPtrA)(h, 12);
    t_note("GetWindowLongPtr(h,12) straddling end: r=%llx err=%lu", (long long)r, GetLastError());
    SetLastError(0);
    CHECK(U(GetWindowLongA)(h, 16) == 0 && GetLastError() == ERROR_INVALID_INDEX);
    SetLastError(0);
    t_note("GetWindowLongA(h,13) = %lx err=%lu", (unsigned long)U(GetWindowLongA)(h, 13), GetLastError());
    mu_kill(h);
}
TEST(misc_ui_gwl_extra_bytes_long_halves) {
    NEED_WND();
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    for (int off = 0; off <= 12; off += 4) {
        CHECK(U(SetWindowLongA)(h, off, 0x10000 + off) == 0);
        CHECK(U(GetWindowLongA)(h, off) == 0x10000 + off);
    }
    for (int off = 0; off <= 12; off += 4)
        CHECK(U(GetWindowLongA)(h, off) == 0x10000 + off);
    CHECK(U(SetWindowLongA)(h, 4, (LONG)0x80000000) == 0x10004);
    CHECK((DWORD)U(GetWindowLongA)(h, 4) == 0x80000000);
    mu_kill(h);
}
TEST(misc_ui_gwl_extra_bytes_word_access) {
    NEED_WND(); REQUIRE(U(GetWindowWord));
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    U(SetWindowLongA)(h, 0, 0x12345678);
    /* GetWindowWord only supports GWW_HINSTANCE / HWNDPARENT / ID or in-range extra bytes */
    WORD w0 = U(GetWindowWord)(h, 0);
    t_note("GetWindowWord(h,0) after Long=0x12345678: 0x%x", w0);
    mu_kill(h);
}
TEST(misc_ui_gwl_userdata_full_width) {
    NEED_WND();
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    LONG_PTR v[] = { 0x1122334455667788LL, -1, 0, 1, (LONG_PTR)0x8000000000000000ULL, 0x7FFFFFFF, (LONG_PTR)0xFFFFFFFFULL, (LONG_PTR)0x100000000ULL };
    LONG_PTR prev = 0;
    CHECK(U(GetWindowLongPtrA)(h, GWLP_USERDATA) == 0);
    for (int i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        CHECKF(U(SetWindowLongPtrA)(h, GWLP_USERDATA, v[i]) == prev, "i=%d", i);
        CHECKF(U(GetWindowLongPtrA)(h, GWLP_USERDATA) == v[i], "i=%d", i);
        CHECKF(U(GetWindowLongPtrW)(h, GWLP_USERDATA) == v[i], "W i=%d", i);
        prev = v[i];
    }
    U(SetWindowLongPtrA)(h, GWLP_USERDATA, (LONG_PTR)0x1122334455667788LL);
    t_note("GetWindowLong(GWL_USERDATA) low 32 bits match: %d", (DWORD)U(GetWindowLongA)(h, GWL_USERDATA) == 0x55667788);
    mu_kill(h);
}
TEST(misc_ui_gwl_userdata_setwindowlong32_upper_bits) {
    NEED_WND();
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    U(SetWindowLongPtrA)(h, GWLP_USERDATA, (LONG_PTR)0x1122334455667788LL);
    LONG old = U(SetWindowLongA)(h, GWL_USERDATA, (LONG)0xAABBCCDD);
    LONG_PTR now = U(GetWindowLongPtrA)(h, GWLP_USERDATA);
    t_note("SetWindowLong(GWL_USERDATA) returned 0x%lx, low half now 0x%lx", (DWORD)old, (DWORD)now);
    t_note("SetWindowLong32(USERDATA) on 64-bit: full value now 0x%llx", (long long)now);
    mu_kill(h);
}
TEST(misc_ui_gwl_userdata_per_window) {
    NEED_WND();
    HWND a = mu_top(), b = mu_top();
    if (!a || !b) { CHECK(0); mu_kill(a); mu_kill(b); return; }
    U(SetWindowLongPtrA)(a, GWLP_USERDATA, 1);
    U(SetWindowLongPtrA)(b, GWLP_USERDATA, 2);
    CHECK(U(GetWindowLongPtrA)(a, GWLP_USERDATA) == 1 && U(GetWindowLongPtrA)(b, GWLP_USERDATA) == 2);
    mu_kill(a); mu_kill(b);
}
TEST(misc_ui_gwl_hinstance) {
    NEED_WND();
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    CHECK((HINSTANCE)U(GetWindowLongPtrA)(h, GWLP_HINSTANCE) == GetModuleHandleA(NULL));
    CHECK(U(GetWindowLongPtrW)(h, GWLP_HINSTANCE) == U(GetWindowLongPtrA)(h, GWLP_HINSTANCE));
    LONG_PTR old = U(SetWindowLongPtrA)(h, GWLP_HINSTANCE, (LONG_PTR)0x400000);
    CHECK((HINSTANCE)old == GetModuleHandleA(NULL));
    CHECK(U(GetWindowLongPtrA)(h, GWLP_HINSTANCE) == 0x400000);
    U(SetWindowLongPtrA)(h, GWLP_HINSTANCE, old);
    CHECK((HINSTANCE)U(GetWindowLongPtrA)(h, GWLP_HINSTANCE) == GetModuleHandleA(NULL));
    mu_kill(h);
}
TEST(misc_ui_gwl_hinstance_null_at_create) {
    NEED_WND();
    HWND h = U(CreateWindowExA)(0, MU_CLS, "t", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    CHECK(h != NULL); if (!h) return;
    t_note("CreateWindowEx(hInstance=NULL): GWLP_HINSTANCE=%p", (void *)U(GetWindowLongPtrA)(h, GWLP_HINSTANCE));
    mu_kill(h);
}
TEST(misc_ui_gwl_hwndparent_toplevel_and_owner) {
    NEED_WND(); REQUIRE(U(GetWindow)); REQUIRE(U(GetParent));
    HWND a = mu_top();
    HWND b = mu_mk(WS_POPUP, 0, 10, 10, 50, 50, a);        /* owned popup */
    HWND c = mu_top();
    if (!a || !b || !c) { CHECK(0); mu_kill(a); mu_kill(b); mu_kill(c); return; }
    CHECK(U(GetWindowLongPtrA)(a, GWLP_HWNDPARENT) == 0);
    CHECK((HWND)U(GetWindowLongPtrA)(b, GWLP_HWNDPARENT) == a);
    CHECK(U(GetWindow)(b, GW_OWNER) == a);
    CHECK(U(GetParent)(b) == a);
    /* changing the owner */
    LONG_PTR old = U(SetWindowLongPtrA)(b, GWLP_HWNDPARENT, (LONG_PTR)c);
    CHECK((HWND)old == a);
    CHECK(U(GetWindow)(b, GW_OWNER) == c);
    CHECK((HWND)U(GetWindowLongPtrA)(b, GWLP_HWNDPARENT) == c);
    /* removing it */
    old = U(SetWindowLongPtrA)(b, GWLP_HWNDPARENT, 0);
    t_note("clearing owner: ret==prev %d, owner now %p", (HWND)old == c, (void *)U(GetWindow)(b, GW_OWNER));
    mu_kill(a); mu_kill(b); mu_kill(c);
}
TEST(misc_ui_gwl_hwndparent_child) {
    NEED_WND(); REQUIRE(U(GetParent));
    HWND p1 = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL), p2 = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c = p1 ? mu_child(p1, 0) : NULL;
    if (!p1 || !p2 || !c) { CHECK(0); mu_kill(p1); mu_kill(p2); return; }
    CHECK((HWND)U(GetWindowLongPtrA)(c, GWLP_HWNDPARENT) == p1);
    CHECK(U(GetParent)(c) == p1);
    LONG_PTR old = U(SetWindowLongPtrA)(c, GWLP_HWNDPARENT, (LONG_PTR)p2);
    t_note("SetWindowLongPtr(child, HWNDPARENT): ret==oldparent %d, GetParent==newparent %d", (HWND)old == p1, U(GetParent)(c) == p2);
    mu_kill(p1); mu_kill(p2);
}
TEST(misc_ui_gwl_id_child) {
    NEED_WND(); REQUIRE(U(GetDlgCtrlID)); REQUIRE(U(GetDlgItem));
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    CHECK(p != NULL); if (!p) return;
    HWND (WINAPI *cw)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) = U(CreateWindowExA);
    HWND c = cw(0, MU_CLS, "c", WS_CHILD, 0, 0, 10, 10, p, (HMENU)(UINT_PTR)0x1234, GetModuleHandleA(NULL), NULL);
    CHECK(c != NULL);
    if (c) {
        CHECK(U(GetWindowLongPtrA)(c, GWLP_ID) == 0x1234);
        CHECK(U(GetWindowLongA)(c, GWL_ID) == 0x1234);
        CHECK(U(GetDlgCtrlID)(c) == 0x1234);
        CHECK(U(GetDlgItem)(p, 0x1234) == c);
        LONG_PTR vals[] = { 0, 1, 0x7FFFFFFF, 0xFFFF, 0x10000, 42 };
        for (int i = 0; i < 6; i++) {
            U(SetWindowLongPtrA)(c, GWLP_ID, vals[i]);
            CHECKF(U(GetWindowLongPtrA)(c, GWLP_ID) == vals[i], "i=%d got %llx", i, (long long)U(GetWindowLongPtrA)(c, GWLP_ID));
            CHECKF(U(GetDlgCtrlID)(c) == (int)vals[i], "dlgctrlid i=%d", i);
            if (vals[i]) CHECKF(U(GetDlgItem)(p, (int)vals[i]) == c, "getdlgitem i=%d", i);
        }
        U(SetWindowLongPtrA)(c, GWLP_ID, -1);
        t_note("GWLP_ID set to -1: Ptr=0x%llx Long=%ld DlgCtrlID=%d", (long long)U(GetWindowLongPtrA)(c, GWLP_ID), U(GetWindowLongA)(c, GWL_ID), U(GetDlgCtrlID)(c));
        U(SetWindowLongPtrA)(c, GWLP_ID, (LONG_PTR)0x123456789LL);
        t_note("GWLP_ID set to 0x123456789: Ptr=0x%llx", (long long)U(GetWindowLongPtrA)(c, GWLP_ID));
    }
    mu_kill(p);
}
TEST(misc_ui_gwl_id_toplevel_is_menu) {
    NEED_WND(); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu)); REQUIRE(U(GetMenu)); REQUIRE(U(SetMenu));
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    HMENU m = U(CreateMenu)();
    CHECK(m != NULL);
    CHECK(U(GetWindowLongPtrA)(h, GWLP_ID) == 0);
    CHECK(U(GetMenu)(h) == NULL);
    CHECK(U(SetMenu)(h, m));
    CHECK((HMENU)U(GetWindowLongPtrA)(h, GWLP_ID) == m);
    CHECK(U(GetMenu)(h) == m);
    CHECK(U(SetMenu)(h, NULL));
    CHECK(U(GetWindowLongPtrA)(h, GWLP_ID) == 0);
    U(DestroyMenu)(m);
    mu_kill(h);
}
static LRESULT CALLBACK alt_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }
static int g_alt_hits;
static LRESULT CALLBACK alt_proc2(HWND h, UINT m, WPARAM w, LPARAM l) { if (m == WM_NULL) g_alt_hits++; return DefWindowProcA(h, m, w, l); }
TEST(misc_ui_gwl_wndproc_get_set) {
    NEED_WND(); REQUIRE(U(SendMessageA));
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    LONG_PTR a = U(GetWindowLongPtrA)(h, GWLP_WNDPROC);
    CHECK((WNDPROC)a == mu_proc);
    LONG_PTR w = U(GetWindowLongPtrW)(h, GWLP_WNDPROC);
    CHECK(w != 0);
    t_note("GWLP_WNDPROC A-class: via A==proc, via W %s proc", w == a ? "==" : "!= (thunk)");
    LONG_PTR old = U(SetWindowLongPtrA)(h, GWLP_WNDPROC, (LONG_PTR)alt_proc2);
    CHECK(old == a);
    CHECK(U(GetWindowLongPtrA)(h, GWLP_WNDPROC) == (LONG_PTR)alt_proc2);
    g_alt_hits = 0;
    U(SendMessageA)(h, WM_NULL, 0, 0);
    CHECK(g_alt_hits == 1);
    old = U(SetWindowLongPtrA)(h, GWLP_WNDPROC, a);
    CHECK(old == (LONG_PTR)alt_proc2);
    g_alt_hits = 0;
    U(SendMessageA)(h, WM_NULL, 0, 0);
    CHECK(g_alt_hits == 0);
    mu_kill(h);
}
TEST(misc_ui_gwl_wndproc_wide_set_converts) {
    NEED_WND(); REQUIRE(U(SendMessageA)); REQUIRE(U(CallWindowProcA));
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    LONG_PTR a = U(GetWindowLongPtrA)(h, GWLP_WNDPROC);
    /* installing a proc through the W entry point marks the window as Unicode */
    LONG_PTR old = U(SetWindowLongPtrW)(h, GWLP_WNDPROC, (LONG_PTR)alt_proc2);
    CHECK(old != 0);
    CHECK(U(IsWindowUnicode)(h));
    g_alt_hits = 0;
    U(SendMessageA)(h, WM_NULL, 0, 0);
    CHECK(g_alt_hits == 1);
    CHECK(U(GetWindowLongPtrW)(h, GWLP_WNDPROC) == (LONG_PTR)alt_proc2);
    t_note("A-view of wide-installed proc %s raw proc", U(GetWindowLongPtrA)(h, GWLP_WNDPROC) == (LONG_PTR)alt_proc2 ? "==" : "!= (thunk)");
    U(SetWindowLongPtrA)(h, GWLP_WNDPROC, a);
    CHECK(!U(IsWindowUnicode)(h));
    mu_kill(h);
}
TEST(misc_ui_gwl_isunicode_for_class_kind) {
    NEED_WND(); REQUIRE(U(IsWindowUnicode));
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    CHECK(!U(IsWindowUnicode)(h));
    mu_kill(h);
    SetLastError(0);
    CHECK(!U(IsWindowUnicode)(BOGUS_HWND));
    CHECK(!U(IsWindowUnicode)(NULL));
}

/* ---------- cross-thread ---------- */
TEST(misc_ui_gwl_cross_thread_set_get) {
    NEED_WND();
    mu_worker w;
    if (!mu_worker_start(&w, WS_POPUP, 0, "xthread")) { CHECK(0); mu_worker_stop(&w); return; }
    CHECK(U(SetWindowLongPtrA)(w.hwnd, GWLP_USERDATA, 0x55) == 0);
    CHECK(U(GetWindowLongPtrA)(w.hwnd, GWLP_USERDATA) == 0x55);
    LONG_PTR s = mu_style(w.hwnd);
    CHECK((DWORD)U(SetWindowLongPtrA)(w.hwnd, GWL_STYLE, s | WS_TABSTOP) == (DWORD)s);
    CHECK((mu_style(w.hwnd) & WS_TABSTOP) != 0);
    CHECK(U(GetWindowLongPtrA)(w.hwnd, GWLP_WNDPROC) == (LONG_PTR)mu_proc);
    CHECK(U(EnableWindow)(w.hwnd, FALSE) == 0);
    CHECK((mu_style(w.hwnd) & WS_DISABLED) != 0);
    mu_worker_stop(&w);
}
TEST(misc_ui_gwl_cross_thread_destroy_denied) {
    NEED_WND();
    mu_worker w;
    if (!mu_worker_start(&w, WS_POPUP, 0, "xthread")) { CHECK(0); mu_worker_stop(&w); return; }
    SetLastError(0);
    BOOL r = U(DestroyWindow)(w.hwnd);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_ACCESS_DENIED);
    CHECK(U(IsWindow)(w.hwnd));
    /* a message to the owning thread works though */
    CHECK(U(SendMessageA)(w.hwnd, WM_CLOSE, 0, 0) == 0);
    mu_sleep_pump(50);
    CHECK(!U(IsWindow)(w.hwnd));
    mu_worker_stop(&w);
}
TEST(misc_ui_gwl_threadprocessid) {
    NEED_WND(); REQUIRE(U(GetWindowThreadProcessId));
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    DWORD pid = 0;
    DWORD tid = U(GetWindowThreadProcessId)(h, &pid);
    CHECK(tid == GetCurrentThreadId());
    CHECK(pid == GetCurrentProcessId());
    pid = 0xdeadbeef;
    CHECK(U(GetWindowThreadProcessId)(h, NULL) == GetCurrentThreadId());
    mu_worker w;
    if (mu_worker_start(&w, WS_POPUP, 0, "tp")) {
        pid = 0;
        tid = U(GetWindowThreadProcessId)(w.hwnd, &pid);
        CHECK(tid == w.tid);
        CHECK(pid == GetCurrentProcessId());
    } else CHECK(0);
    mu_worker_stop(&w);
    pid = 0xdeadbeef;
    SetLastError(0);
    CHECK(U(GetWindowThreadProcessId)(BOGUS_HWND, &pid) == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    CHECK(pid == 0 || pid == 0xdeadbeef);
    SetLastError(0);
    CHECK(U(GetWindowThreadProcessId)(NULL, NULL) == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    mu_kill(h);
}
TEST(misc_ui_gwl_threadprocessid_desktop) {
    NEED_WND(); REQUIRE(U(GetWindowThreadProcessId)); REQUIRE(U(GetDesktopWindow));
    DWORD pid = 0;
    DWORD tid = U(GetWindowThreadProcessId)(U(GetDesktopWindow)(), &pid);
    CHECK(tid != 0);
    CHECK(pid != 0);
    CHECK(tid != GetCurrentThreadId());
}

/* ---------- handle value properties ---------- */
TEST(misc_ui_handle_fits_32_bits) {
    NEED_WND();
    HWND h[8] = {0};
    for (int i = 0; i < 8; i++) {
        h[i] = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
        CHECK(h[i] != NULL);
        if (!h[i]) continue;
        CHECKF(((ULONG_PTR)h[i] >> 32) == 0 || ((ULONG_PTR)h[i] >> 32) == 0xFFFFFFFF, "h=%p", h[i]);
        CHECKF(((ULONG_PTR)h[i] & 0xFFFF) != 0, "zero index h=%p", h[i]);
        CHECKF((HWND)(LONG_PTR)(LONG)(ULONG_PTR)h[i] == h[i] || ((ULONG_PTR)h[i] & 0x80000000), "signext h=%p", h[i]);
        CHECK(U(IsWindow)((HWND)(ULONG_PTR)(DWORD)(ULONG_PTR)h[i]));
        CHECK(U(IsWindow)((HWND)(LONG_PTR)(LONG)(ULONG_PTR)h[i]));
    }
    for (int i = 0; i < 8; i++)
        for (int j = i + 1; j < 8; j++) CHECK(h[i] != h[j]);
    for (int i = 0; i < 8; i++) mu_kill(h[i]);
}
TEST(misc_ui_handle_alias_high_bits) {
    NEED_WND();
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    CHECK(h != NULL); if (!h) return;
    HWND alias = (HWND)((ULONG_PTR)h | 0x100000000ULL);
    t_note("IsWindow(handle|1<<32) = %d, IsWindow(sign-extended)=%d", U(IsWindow)(alias), U(IsWindow)((HWND)(LONG_PTR)(LONG)(ULONG_PTR)h));
    mu_kill(h);
}
TEST(misc_ui_handle_loword_reused_hiword_changes) {
    NEED_WND();
    HWND a = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    CHECK(a != NULL); if (!a) return;
    mu_kill(a);
    CHECK(!U(IsWindow)(a));
    HWND b = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    CHECK(b != NULL);
    if (b) {
        if (a == b) t_note("handle value reused immediately");
        CHECK(!(a == b && !U(IsWindow)(a)));
        if ((ULONG_PTR)a != (ULONG_PTR)b && ((ULONG_PTR)a & 0xFFFF) == ((ULONG_PTR)b & 0xFFFF))
            t_note("slot reused with new uniqueness 0x%x -> 0x%x", (unsigned)((ULONG_PTR)a >> 16), (unsigned)((ULONG_PTR)b >> 16));
        if (a != b) CHECK(!U(IsWindow)(a));
    }
    mu_kill(b);
}
TEST(misc_ui_handle_pseudo_handles_not_windows) {
    NEED_WND();
    HWND ps[] = { HWND_TOP, HWND_BOTTOM, HWND_TOPMOST, HWND_NOTOPMOST, HWND_BROADCAST, HWND_MESSAGE, HWND_DESKTOP };
    for (int i = 0; i < (int)(sizeof ps / sizeof ps[0]); i++)
        CHECKF(!U(IsWindow)(ps[i]), "pseudo-handle %p is a window", ps[i]);
    CHECK((ULONG_PTR)HWND_BROADCAST == 0xFFFF);
    CHECK((LONG_PTR)HWND_TOPMOST == -1 && (LONG_PTR)HWND_NOTOPMOST == -2 && (LONG_PTR)HWND_MESSAGE == -3);
    CHECK((LONG_PTR)HWND_BOTTOM == 1 && HWND_TOP == NULL);
}
TEST(misc_ui_handle_desktop_and_message_window) {
    NEED_WND(); REQUIRE(U(GetDesktopWindow)); REQUIRE(U(GetParent)); REQUIRE(U(GetAncestor));
    HWND d = U(GetDesktopWindow)();
    CHECK(d != NULL && U(IsWindow)(d));
    CHECK(((ULONG_PTR)d >> 32) == 0);
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    CHECK(U(GetAncestor)(h, GA_PARENT) == d);
    CHECK(U(GetParent)(h) == NULL);
    HWND m = mu_mk(0, 0, 0, 0, 0, 0, HWND_MESSAGE);
    CHECK(m != NULL);
    if (m) {
        HWND mp = U(GetAncestor)(m, GA_PARENT);
        CHECK(mp != NULL && mp != d);
        CHECK(U(IsWindow)(mp));
        char cls[64] = "";
        U(GetClassNameA)(mp, cls, 64);
        t_note("message-window parent class: %s", cls);
        CHECK(U(GetParent)(m) == NULL || U(GetParent)(m) == mp);
        CHECK(!U(IsWindowVisible)(m));
    }
    mu_kill(h); mu_kill(m);
}
TEST(misc_ui_handle_guiresources_count) {
    NEED_WND(); REQUIRE(U(GetGuiResources));
    HANDLE me = GetCurrentProcess();
    { HWND warm = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL); mu_kill(warm); }   /* thread's implicit IME window */
    DWORD n0 = U(GetGuiResources)(me, GR_USEROBJECTS);
    DWORD g0 = U(GetGuiResources)(me, GR_GDIOBJECTS);
    if (n0 == 0) { t_skip("GetGuiResources reports no objects"); return; }
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    DWORD n1 = U(GetGuiResources)(me, GR_USEROBJECTS);
    CHECKF(n1 > n0, "n0=%lu n1=%lu", n0, n1);
    mu_kill(h);
    DWORD n2 = U(GetGuiResources)(me, GR_USEROBJECTS);
    CHECKF(n2 < n1, "n1=%lu n2=%lu", n1, n2);
    t_note("GDI objects before/after window create+destroy: %lu / %lu", g0, U(GetGuiResources)(me, GR_GDIOBJECTS));
    DWORD peak = U(GetGuiResources)(me, 4);
    CHECK(peak >= n1);
    DWORD gpeak = U(GetGuiResources)(me, 2);
    CHECK(gpeak >= g0);
}
TEST(misc_ui_handle_guiresources_errors) {
    NEED_WND(); REQUIRE(U(GetGuiResources));
    SetLastError(0);
    DWORD r = U(GetGuiResources)(NULL, GR_USEROBJECTS);
    CHECK(r == 0);
    t_note("GetGuiResources(NULL): err=%lu", GetLastError());
    SetLastError(0);
    r = U(GetGuiResources)(GetCurrentProcess(), 100);
    CHECK(r == 0);
    t_note("GetGuiResources(flags=100): err=%lu", GetLastError());
    r = U(GetGuiResources)(INVALID_HANDLE_VALUE, GR_USEROBJECTS);
    t_note("GetGuiResources(INVALID_HANDLE_VALUE) = %lu", r);
}
TEST(misc_ui_handle_many_windows_distinct) {
    NEED_WND();
    enum { N = 200 };
    static HWND hs[N];
    int made = 0;
    for (int i = 0; i < N; i++) { hs[i] = mu_mk(WS_POPUP, 0, 0, 0, 5, 5, NULL); if (hs[i]) made++; }
    CHECK(made == N);
    int dup = 0;
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (hs[i] && hs[i] == hs[j]) dup++;
    CHECK(dup == 0);
    for (int i = 0; i < N; i++) mu_kill(hs[i]);
    for (int i = 0; i < N; i++) if (hs[i]) CHECK(!U(IsWindow)(hs[i]));
}
TEST(misc_ui_handle_getwindowmodulefilename) {
    NEED_WND(); REQUIRE(U(GetWindowModuleFileNameA)); REQUIRE(U(GetWindowModuleFileNameW));
    HWND h = mu_top();
    CHECK(h != NULL); if (!h) return;
    char a[MAX_PATH], ref[MAX_PATH]; WCHAR w[MAX_PATH], rw[MAX_PATH];
    UINT na = U(GetWindowModuleFileNameA)(h, a, MAX_PATH);
    UINT nr = GetModuleFileNameA(GetModuleHandleA(NULL), ref, MAX_PATH);
    CHECK(na == nr && na > 0);
    CHECK(!strcmp(a, ref));
    UINT nw = U(GetWindowModuleFileNameW)(h, w, MAX_PATH);
    GetModuleFileNameW(GetModuleHandleA(NULL), rw, MAX_PATH);
    CHECK(nw == nr);
    CHECK(!lstrcmpW(w, rw));
    /* truncation */
    memset(a, 'x', sizeof a);
    UINT nt = U(GetWindowModuleFileNameA)(h, a, 4);
    CHECK(nt == 4);
    CHECK(a[3] == 0);
    SetLastError(0);
    memset(a, 'x', sizeof a);
    CHECK(U(GetWindowModuleFileNameA)(h, a, 0) == 0);
    /* bogus */
    a[0] = 'x';
    CHECK(U(GetWindowModuleFileNameA)(BOGUS_HWND, a, MAX_PATH) == 0);
    CHECK(U(GetWindowModuleFileNameW)(NULL, w, MAX_PATH) == 0);
    /* hInstance switched to user32 */
    LONG_PTR old = U(SetWindowLongPtrA)(h, GWLP_HINSTANCE, (LONG_PTR)t_user32());
    UINT nu = U(GetWindowModuleFileNameA)(h, a, MAX_PATH);
    CHECK(nu > 0);
    CHECK(strstr(a, "user32") || strstr(a, "USER32"));
    U(SetWindowLongPtrA)(h, GWLP_HINSTANCE, old);
    mu_kill(h);
}
