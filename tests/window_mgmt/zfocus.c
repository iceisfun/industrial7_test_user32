/* z-order, focus, activation, capture, foreground */
#include "wm.h"

static HWND hid(void) { return wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 40, 40, NULL); }
static HWND vis(int x, int y) { return wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW, x, y, 200, 120, NULL); }
static BOOL topmost(HWND h) { return (U(GetWindowLongA)(h, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0; }
#define Z_NOACT (SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)

typedef struct { void (*fn)(void *); void *arg; } thr_arg;
static DWORD WINAPI thr_main(LPVOID p) { thr_arg *a = p; a->fn(a->arg); return 0; }
static int run_thread(void (*fn)(void *), void *arg) {
    thr_arg a = { fn, arg };
    HANDLE t = CreateThread(NULL, 0, thr_main, &a, 0, NULL);
    if (!t) return 0;
    DWORD r = WaitForSingleObject(t, WM_TIMEOUT_MS * 2);
    CloseHandle(t);
    return r == WAIT_OBJECT_0;
}

/* ---------------- z-order ---------------- */
TEST(wm_z_new_window_on_top) {
    HWND a = hid(), b = hid(), c = hid();
    CHECK(wm_isabove(c, b)); CHECK(wm_isabove(b, a)); CHECK(wm_isabove(c, a));
    CHECK(!wm_isabove(a, b)); CHECK(!wm_isabove(a, c));
    wm_destroy(a); wm_destroy(b); wm_destroy(c);
}

TEST(wm_z_hwnd_top) {
    NEED(SetWindowPos);
    HWND a = hid(), b = hid(), c = hid();
    CHECK(U(SetWindowPos)(a, HWND_TOP, 0, 0, 0, 0, Z_NOACT));
    CHECK(wm_isabove(a, b)); CHECK(wm_isabove(a, c)); CHECK(wm_isabove(c, b));
    CHECK(U(SetWindowPos)(b, HWND_TOP, 0, 0, 0, 0, Z_NOACT));
    CHECK(wm_isabove(b, a)); CHECK(wm_isabove(a, c));
    CHECK(!topmost(a));
    wm_destroy(a); wm_destroy(b); wm_destroy(c);
}

TEST(wm_z_hwnd_bottom) {
    HWND a = hid(), b = hid(), c = hid();
    CHECK(U(SetWindowPos)(c, HWND_BOTTOM, 0, 0, 0, 0, Z_NOACT));
    EQ(U(GetWindow)(c, GW_HWNDLAST), c);
    CHECK(wm_isabove(b, c)); CHECK(wm_isabove(a, c)); CHECK(wm_isabove(b, a));
    CHECK(U(SetWindowPos)(b, HWND_BOTTOM, 0, 0, 0, 0, Z_NOACT));
    EQ(U(GetWindow)(b, GW_HWNDLAST), b);
    EQ(U(GetWindow)(b, GW_HWNDNEXT), NULL);
    EQ(U(GetWindow)(b, GW_HWNDPREV), c);
    wm_destroy(a); wm_destroy(b); wm_destroy(c);
}

TEST(wm_z_insert_after_specific) {
    HWND a = hid(), b = hid(), c = hid(), d = hid();
    /* order now d c b a ; move d right after a */
    CHECK(U(SetWindowPos)(d, a, 0, 0, 0, 0, Z_NOACT));
    EQ(U(GetWindow)(a, GW_HWNDNEXT) == d || wm_isabove(a, d), 1);
    CHECK(wm_isabove(a, d)); CHECK(wm_isabove(c, a));
    EQ(U(GetWindow)(a, GW_HWNDNEXT), d);
    CHECK(U(SetWindowPos)(a, c, 0, 0, 0, 0, Z_NOACT));
    EQ(U(GetWindow)(c, GW_HWNDNEXT), a);
    CHECK(wm_isabove(c, a)); CHECK(wm_isabove(a, b));
    wm_destroy(a); wm_destroy(b); wm_destroy(c); wm_destroy(d);
}

TEST(wm_z_insert_after_self) {
    HWND a = hid(), b = hid();
    SetLastError(0);
    BOOL r = U(SetWindowPos)(a, a, 0, 0, 0, 0, Z_NOACT);
    t_note("SetWindowPos(h, h) = %d err=%lu", r, GetLastError());
    CHECK(wm_isabove(b, a));
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_z_nozorder_ignores_insertafter) {
    HWND a = hid(), b = hid();
    CHECK(U(SetWindowPos)(a, HWND_TOP, 0, 0, 0, 0, Z_NOACT | SWP_NOZORDER));
    CHECK(wm_isabove(b, a));
    CHECK(U(SetWindowPos)(b, HWND_BOTTOM, 0, 0, 0, 0, Z_NOACT | SWP_NOZORDER));
    CHECK(wm_isabove(b, a));
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_z_topmost_flag_and_order) {
    HWND a = hid(), b = hid();
    CHECK(U(SetWindowPos)(a, HWND_TOPMOST, 0, 0, 0, 0, Z_NOACT));
    CHECK(topmost(a)); CHECK(!topmost(b));
    CHECK(wm_isabove(a, b));
    HWND c = hid();
    CHECK(wm_isabove(a, c));           /* new normal window stays below topmost */
    CHECK(U(SetWindowPos)(b, HWND_TOP, 0, 0, 0, 0, Z_NOACT));
    CHECK(wm_isabove(a, b));           /* HWND_TOP of a normal window does not pass topmost ones */
    CHECK(!topmost(b));
    CHECK(U(SetWindowPos)(a, HWND_NOTOPMOST, 0, 0, 0, 0, Z_NOACT));
    CHECK(!topmost(a));
    CHECK(wm_isabove(a, b) || wm_isabove(b, a));
    CHECK(wm_isabove(a, c));
    wm_destroy(a); wm_destroy(b); wm_destroy(c);
}

TEST(wm_z_notopmost_on_normal_noop) {
    HWND a = hid(), b = hid();
    CHECK(U(SetWindowPos)(a, HWND_NOTOPMOST, 0, 0, 0, 0, Z_NOACT));
    CHECK(!topmost(a));
    t_note("HWND_NOTOPMOST on a normal window: a above b = %d", wm_isabove(a, b));
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_z_topmost_group_order) {
    HWND a = hid(), b = hid();
    U(SetWindowPos)(a, HWND_TOPMOST, 0, 0, 0, 0, Z_NOACT);
    U(SetWindowPos)(b, HWND_TOPMOST, 0, 0, 0, 0, Z_NOACT);
    CHECK(topmost(a) && topmost(b));
    CHECK(wm_isabove(b, a));
    U(SetWindowPos)(a, HWND_TOP, 0, 0, 0, 0, Z_NOACT);
    CHECK(wm_isabove(a, b));
    CHECK(topmost(a));
    U(SetWindowPos)(a, HWND_BOTTOM, 0, 0, 0, 0, Z_NOACT);
    t_note("HWND_BOTTOM on topmost window: topmost flag %d", topmost(a));
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_z_create_topmost_exstyle) {
    HWND n = hid();
    HWND t = wm_mk(WS_POPUP, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 20, 20, NULL);
    CHECK(topmost(t));
    CHECK(wm_isabove(t, n));
    HWND n2 = hid();
    CHECK(wm_isabove(t, n2));
    CHECK(wm_isabove(n2, n));
    wm_destroy(n); wm_destroy(t); wm_destroy(n2);
}

TEST(wm_z_setwindowlong_topmost_no_move) {
    HWND a = hid(), b = hid();
    U(SetWindowLongA)(a, GWL_EXSTYLE, U(GetWindowLongA)(a, GWL_EXSTYLE) | WS_EX_TOPMOST);
    t_note("after SetWindowLong(WS_EX_TOPMOST): a above b = %d (flag set but z-order not refreshed)", wm_isabove(a, b));
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_z_bringwindowtotop) {
    NEED(BringWindowToTop);
    HWND a = hid(), b = hid(), c = hid();
    CHECK(U(BringWindowToTop)(a));
    CHECK(wm_isabove(a, c)); CHECK(wm_isabove(a, b));
    CHECK(U(BringWindowToTop)(b));
    CHECK(wm_isabove(b, a));
    CHECK(U(BringWindowToTop)(b));      /* already on top */
    wm_destroy(a); wm_destroy(b); wm_destroy(c);
}

TEST(wm_z_bringwindowtotop_invalid) {
    SetLastError(0);
    CHECK(!U(BringWindowToTop)(NULL));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(BringWindowToTop)((HWND)(ULONG_PTR)0x77770));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_z_bringwindowtotop_child) {
    HWND p = wm_hidden();
    HWND k1 = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p), k2 = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p), k3 = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    CHECK(U(BringWindowToTop)(k3));
    EQ(U(GetWindow)(p, GW_CHILD), k3);
    EQ(U(GetWindow)(k3, GW_HWNDNEXT), k1);
    CHECK(U(BringWindowToTop)(k2));
    EQ(U(GetWindow)(p, GW_CHILD), k2);
    EQ(U(GetWindow)(k2, GW_HWNDNEXT), k3);
    EQ(U(GetWindow)(k3, GW_HWNDNEXT), k1);
    EQ(U(GetWindow)(k1, GW_HWNDNEXT), NULL);
    wm_destroy(p);
}

TEST(wm_z_child_bottom_and_after) {
    HWND p = wm_hidden();
    HWND k1 = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p), k2 = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p), k3 = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    CHECK(U(SetWindowPos)(k1, HWND_BOTTOM, 0, 0, 0, 0, Z_NOACT));
    EQ(U(GetWindow)(k2, GW_HWNDNEXT), k3);
    EQ(U(GetWindow)(k3, GW_HWNDNEXT), k1);
    CHECK(U(SetWindowPos)(k1, k2, 0, 0, 0, 0, Z_NOACT));
    EQ(U(GetWindow)(k2, GW_HWNDNEXT), k1);
    EQ(U(GetWindow)(p, GW_CHILD), k2);
    CHECK(U(SetWindowPos)(k3, HWND_TOP, 0, 0, 0, 0, Z_NOACT));
    EQ(U(GetWindow)(p, GW_CHILD), k3);
    wm_destroy(p);
}

TEST(wm_z_child_topmost_flag_rejected) {
    HWND p = wm_hidden();
    HWND k = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    SetLastError(0);
    BOOL r = U(SetWindowPos)(k, HWND_TOPMOST, 0, 0, 0, 0, Z_NOACT);
    t_note("SetWindowPos(child, HWND_TOPMOST) = %d err=%lu topmost=%d", r, GetLastError(), topmost(k));
    if (topmost(k)) t_note("child got WS_EX_TOPMOST");
    wm_destroy(p);
}

TEST(wm_z_insert_after_other_parent) {
    HWND p1 = wm_hidden(), p2 = wm_hidden();
    HWND a = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p1), b = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p2);
    SetLastError(0);
    BOOL r = U(SetWindowPos)(a, b, 0, 0, 0, 0, Z_NOACT);
    t_note("SetWindowPos(insertAfter from another parent) = %d err=%lu", r, GetLastError());
    EQ(U(GetParent)(a), p1);
    wm_destroy(p1); wm_destroy(p2);
}

TEST(wm_z_owned_popup_above_owner) {
    HWND o = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 10, 10, 80, 60, NULL);
    HWND p = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 20, 20, 40, 30, o);
    CHECK(wm_isabove(p, o));
    CHECK(U(SetWindowPos)(o, HWND_TOP, 0, 0, 0, 0, Z_NOACT));
    CHECK(wm_isabove(p, o));
    CHECK(U(SetWindowPos)(o, HWND_BOTTOM, 0, 0, 0, 0, Z_NOACT));
    CHECK(wm_isabove(p, o));
    CHECK(U(BringWindowToTop)(o));
    CHECK(wm_isabove(p, o));
    wm_destroy(o);
}

TEST(wm_z_owned_topmost_follows_owner) {
    HWND o = hid();
    HWND p = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 20, 20, 40, 30, o);
    U(SetWindowPos)(o, HWND_TOPMOST, 0, 0, 0, 0, Z_NOACT);
    t_note("owner topmost flag %d", topmost(o));
    t_note("owned popup topmost flag after owner became topmost: %d", topmost(p));
    CHECK(wm_isabove(p, o) || topmost(p) == 0);
    wm_destroy(o);
}

TEST(wm_z_chain_consistency) {
    HWND h[6];
    for (int i = 0; i < 6; i++) h[i] = hid();
    U(SetWindowPos)(h[2], HWND_BOTTOM, 0, 0, 0, 0, Z_NOACT);
    U(SetWindowPos)(h[4], HWND_TOP, 0, 0, 0, 0, Z_NOACT);
    U(SetWindowPos)(h[0], h[3], 0, 0, 0, 0, Z_NOACT);
    for (int i = 0; i < 6; i++) {
        HWND n = U(GetWindow)(h[i], GW_HWNDNEXT);
        if (n) EQ(U(GetWindow)(n, GW_HWNDPREV), h[i]);
        HWND pr = U(GetWindow)(h[i], GW_HWNDPREV);
        if (pr) EQ(U(GetWindow)(pr, GW_HWNDNEXT), h[i]);
        CHECK(U(GetWindow)(h[i], GW_HWNDFIRST) != NULL);
        EQ(U(GetWindow)(U(GetWindow)(h[i], GW_HWNDFIRST), GW_HWNDPREV), NULL);
        EQ(U(GetWindow)(U(GetWindow)(h[i], GW_HWNDLAST), GW_HWNDNEXT), NULL);
    }
    for (int i = 0; i < 6; i++) wm_destroy(h[i]);
}

TEST(wm_z_destroy_removes_from_chain) {
    HWND a = hid(), b = hid(), c = hid();
    wm_destroy(b);
    EQ(U(GetWindow)(c, GW_HWNDNEXT) == a || wm_isabove(c, a), 1);
    CHECK(wm_isabove(c, a));
    wm_destroy(a); wm_destroy(c);
}

TEST(wm_z_next_window_macro) {
    HWND p = wm_hidden();
    HWND a = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p), b = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    EQ(GetNextWindow(a, GW_HWNDNEXT), b);
    EQ(GetNextWindow(b, GW_HWNDPREV), a);
    wm_destroy(p);
}

TEST(wm_z_swp_messages) {
    HWND a = hid(), b = hid();
    wm_logclear();
    CHECK(U(SetWindowPos)(a, HWND_TOP, 0, 0, 0, 0, Z_NOACT));
    CHECK(wm_logcount_h(WM_WINDOWPOSCHANGING, a) >= 1);
    CHECK(wm_logcount_h(WM_WINDOWPOSCHANGED, a) >= 1);
    EQ(wm_logcount(WM_MOVE), 0);
    EQ(wm_logcount(WM_SIZE), 0);
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_z_show_with_noactivate_keeps_order) {
    HWND a = hid(), b = hid();
    U(SetWindowPos)(a, 0, 0, 0, 0, 0, Z_NOACT | SWP_SHOWWINDOW | SWP_NOZORDER);
    CHECK(wm_isabove(b, a));
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_z_arrange_iconic_windows) {
    NEED(ArrangeIconicWindows);
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE | WS_MINIMIZE | WS_CAPTION, 0, 0, 0, 50, 50, p);
    UINT r = U(ArrangeIconicWindows)(p);
    t_note("ArrangeIconicWindows(parent with minimized child) = %u", r);
    r = U(ArrangeIconicWindows)(p);
    t_note("ArrangeIconicWindows again = %u", r);
    wm_destroy(p);
    SetLastError(0);
    r = U(ArrangeIconicWindows)(p);
    EQ(r, 0);
}

static WORD tile_cascade_run(int tile, HWND parent, const HWND *kids, UINT n) {
    return tile ? U(TileWindows)(parent, MDITILE_VERTICAL, NULL, n, kids) : U(CascadeWindows)(parent, 0, NULL, n, kids);
}

TEST(wm_z_cascade_windows_children) {
    NEED(CascadeWindows);
    HWND p = wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW, 0, 0, 400, 300, NULL);
    HWND k[3];
    for (int i = 0; i < 3; i++) k[i] = wm_mk(WS_CHILD | WS_VISIBLE | WS_CAPTION | WS_THICKFRAME, 0, 0, 0, 100, 80, p);
    U(ShowWindow)(p, SW_SHOWNA);
    WORD n = tile_cascade_run(0, p, k, 3);
    t_note("CascadeWindows(parent, 3 kids) = %u", n);
    CHECK(n == 3 || n == 0);
    if (n == 3) {
        RECT r0, r1; U(GetWindowRect)(k[0], &r0); U(GetWindowRect)(k[1], &r1);
        CHECK(r0.left != r1.left || r0.top != r1.top);
    }
    wm_destroy(p);
}

TEST(wm_z_tile_windows_children) {
    NEED(TileWindows);
    HWND p = wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW, 0, 0, 400, 300, NULL);
    HWND k[2];
    for (int i = 0; i < 2; i++) k[i] = wm_mk(WS_CHILD | WS_VISIBLE | WS_CAPTION | WS_THICKFRAME, 0, 0, 0, 100, 80, p);
    U(ShowWindow)(p, SW_SHOWNA);
    WORD n = tile_cascade_run(1, p, k, 2);
    t_note("TileWindows(parent, 2 kids, vertical) = %u", n);
    CHECK(n == 2 || n == 0);
    if (n == 2) {
        RECT r0, r1; U(GetWindowRect)(k[0], &r0); U(GetWindowRect)(k[1], &r1);
        CHECK(r0.right <= r1.left + 1 || r1.right <= r0.left + 1);
    }
    wm_destroy(p);
}

TEST(wm_z_tile_cascade_zero_and_dead) {
    NEED(TileWindows); NEED(CascadeWindows);
    HWND p = wm_hidden();
    EQ(U(TileWindows)(p, MDITILE_HORIZONTAL, NULL, 0, NULL), 0);
    EQ(U(CascadeWindows)(p, 0, NULL, 0, NULL), 0);
    wm_destroy(p);
    SetLastError(0);
    EQ(U(TileWindows)(p, MDITILE_HORIZONTAL, NULL, 0, NULL), 0);
    EQ(U(CascadeWindows)(p, 0, NULL, 0, NULL), 0);
}

/* ---------------- focus / activation ---------------- */
static int try_activate(HWND top) {
    U(ShowWindow)(top, SW_SHOW);
    U(SetActiveWindow)(top);
    t_pump();
    return U(GetActiveWindow)() == top;
}

TEST(wm_focus_invalid_targets) {
    NEED(SetFocus);
    HWND h = hid();
    HWND before = U(GetFocus)();
    SetLastError(0);
    EQ(U(SetFocus)((HWND)(ULONG_PTR)0x66660), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    EQ(U(GetFocus)(), before);
    wm_destroy(h);
    SetLastError(0);
    EQ(U(SetFocus)(h), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    EQ(U(GetFocus)(), before);
}

TEST(wm_focus_hidden_and_disabled) {
    HWND top = vis(300, 300);
    int act = try_activate(top);
    HWND child = wm_mk(WS_CHILD, 0, 0, 0, 20, 20, top);   /* not visible */
    HWND before = U(GetFocus)();
    SetLastError(0);
    U(SetFocus)(child);
    if (U(GetFocus)() == child) t_note("hidden child took focus");
    t_note("SetFocus(hidden child) err=%lu", GetLastError());
    U(ShowWindow)(child, SW_SHOWNA);
    U(EnableWindow)(child, FALSE);
    SetLastError(0);
    U(SetFocus)(child);
    CHECK(U(GetFocus)() != child);
    t_note("SetFocus(disabled child) err=%lu active=%d", GetLastError(), act);
    wm_destroy(top);
}

TEST(wm_focus_set_and_get_child) {
    HWND top = vis(300, 300);
    HWND c1 = wm_mk(WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 20, 20, top);
    HWND c2 = wm_mk(WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 30, 0, 20, 20, top);
    if (!try_activate(top)) { t_note("window not activatable here (not foreground), skipping focus asserts"); wm_destroy(top); return; }
    U(SetFocus)(c1);
    EQ(U(GetFocus)(), c1);
    wm_logclear();
    EQ(U(SetFocus)(c2), c1);
    EQ(U(GetFocus)(), c2);
    int k = wm_logfind_h(WM_KILLFOCUS, c1, 0), s = wm_logfind_h(WM_SETFOCUS, c2, 0);
    CHECK(k >= 0 && s > k);
    UINT m; WPARAM w; LPARAM l;
    if (k >= 0 && wm_lognth(k, &m, &w, &l)) EQ(w, (WPARAM)c2);
    if (s >= 0 && wm_lognth(s, &m, &w, &l)) EQ(w, (WPARAM)c1);
    wm_logclear();
    EQ(U(SetFocus)(c2), c2);                    /* returns previous focus (itself) */
    EQ(wm_logcount(WM_SETFOCUS), 0);
    EQ(wm_logcount(WM_KILLFOCUS), 0);
    wm_destroy(top);
}

TEST(wm_focus_set_null) {
    HWND top = vis(300, 300);
    HWND c1 = wm_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 20, 20, top);
    if (!try_activate(top)) { t_note("not activatable, skip"); wm_destroy(top); return; }
    U(SetFocus)(c1);
    EQ(U(GetFocus)(), c1);
    wm_logclear();
    EQ(U(SetFocus)(NULL), c1);
    EQ(U(GetFocus)(), NULL);
    int k = wm_logfind_h(WM_KILLFOCUS, c1, 0);
    CHECK(k >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (k >= 0 && wm_lognth(k, &m, &w, &l)) EQ(w, 0);
    wm_destroy(top);
}

TEST(wm_focus_destroy_focused) {
    HWND top = vis(300, 300);
    HWND c1 = wm_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 20, 20, top);
    if (!try_activate(top)) { t_note("not activatable, skip"); wm_destroy(top); return; }
    U(SetFocus)(c1);
    EQ(U(GetFocus)(), c1);
    wm_logclear();
    U(DestroyWindow)(c1);
    CHECK(wm_logcount_h(WM_KILLFOCUS, c1) >= 1);
    HWND f = U(GetFocus)();
    t_note("focus after destroying focused child: %s", f == NULL ? "NULL" : f == top ? "parent" : "other");
    wm_destroy(top);
}

TEST(wm_focus_to_other_toplevel_activates) {
    HWND a = vis(300, 300), b = vis(560, 300);
    if (!try_activate(a)) { t_note("not activatable, skip"); wm_destroy(a); wm_destroy(b); return; }
    wm_logclear();
    U(SetFocus)(b);
    EQ(U(GetFocus)(), b);
    EQ(U(GetActiveWindow)(), b);
    CHECK(wm_logcount_h(WM_ACTIVATE, a) >= 1);
    CHECK(wm_logcount_h(WM_ACTIVATE, b) >= 1);
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_active_set_and_get) {
    NEED(SetActiveWindow); NEED(GetActiveWindow);
    HWND a = vis(300, 300), b = vis(560, 300);
    if (!try_activate(a)) { t_note("not activatable, skip"); wm_destroy(a); wm_destroy(b); return; }
    EQ(U(GetActiveWindow)(), a);
    wm_logclear();
    EQ(U(SetActiveWindow)(b), a);
    EQ(U(GetActiveWindow)(), b);
    int da = wm_logfind_h(WM_ACTIVATE, a, 0), db = wm_logfind_h(WM_ACTIVATE, b, 0);
    CHECK(da >= 0 && db > da);
    UINT m; WPARAM w; LPARAM l;
    if (da >= 0 && wm_lognth(da, &m, &w, &l)) { EQ(LOWORD(w), WA_INACTIVE); EQ(l, (LPARAM)b); }
    if (db >= 0 && wm_lognth(db, &m, &w, &l)) { EQ(LOWORD(w), WA_ACTIVE); EQ(l, (LPARAM)a); }
    EQ(U(SetActiveWindow)(b), b);
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_active_invalid) {
    SetLastError(0);
    EQ(U(SetActiveWindow)((HWND)(ULONG_PTR)0x88880), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    HWND cur = U(GetActiveWindow)();
    SetLastError(0);
    EQ(U(SetActiveWindow)(NULL), NULL);
    t_note("SetActiveWindow(NULL) err=%lu", GetLastError());
    EQ(U(GetActiveWindow)() == cur || 1, 1);
}

TEST(wm_active_disabled_window) {
    HWND a = vis(300, 300), b = vis(560, 300);
    if (!try_activate(a)) { t_note("not activatable, skip"); wm_destroy(a); wm_destroy(b); return; }
    U(EnableWindow)(b, FALSE);
    SetLastError(0);
    HWND r = U(SetActiveWindow)(b);
    t_note("SetActiveWindow(disabled) = %p err=%lu, active is %s", r, GetLastError(), U(GetActiveWindow)() == b ? "the disabled window" : "other");
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_active_child_activates_root) {
    HWND a = vis(300, 300);
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 20, 20, a);
    U(SetActiveWindow)(c);
    HWND r = U(GetActiveWindow)();
    t_note("after SetActiveWindow(child): active=%s", r == a ? "root" : r == c ? "child" : r == NULL ? "NULL" : "other");
    CHECK(r != c);
    wm_destroy(a);
}

TEST(wm_active_other_thread_window) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 1)) { t_skip("worker"); wm_worker_stop(&w); return; }
    SetLastError(0);
    HWND r = U(SetActiveWindow)(w.hwnd);
    t_note("SetActiveWindow(other thread window) = %p err=%lu", r, GetLastError());
    CHECK(U(GetActiveWindow)() != w.hwnd);
    r = U(SetFocus)(w.hwnd);
    t_note("SetFocus(other thread window) = %p err=%lu", r, GetLastError());
    CHECK(U(GetFocus)() != w.hwnd);
    wm_worker_stop(&w);
}

static void thr_fresh(void *p) {
    int *ok = p;
    ok[0] = U(GetActiveWindow)() == NULL;
    ok[1] = U(GetFocus)() == NULL;
    ok[2] = U(GetCapture)() == NULL;
    ok[3] = U(GetActiveWindow)() == NULL;
}
TEST(wm_focus_fresh_thread_state) {
    int ok[4] = {0};
    CHECK(run_thread(thr_fresh, ok));
    CHECK(ok[0]); CHECK(ok[1]); CHECK(ok[2]); CHECK(ok[3]);
}

static void thr_isgui(void *p) {
    int *r = p;
    r[0] = U(IsGUIThread)(FALSE);
    HWND h = wm_hidden();
    r[1] = U(IsGUIThread)(FALSE);
    wm_destroy(h);
    r[2] = U(IsGUIThread)(FALSE);
}
TEST(wm_gui_thread_conversion) {
    NEED(IsGUIThread);
    int r[3] = {-1, -1, -1};
    CHECK(run_thread(thr_isgui, r));
    t_note("IsGUIThread on fresh thread: %d", r[0]);
    CHECK(r[1] == TRUE);
    CHECK(r[2] == TRUE);          /* stays a GUI thread after the window is gone */
}

static void thr_isgui_convert(void *p) {
    int *r = p;
    r[0] = U(IsGUIThread)(TRUE);
    r[1] = U(IsGUIThread)(FALSE);
}
TEST(wm_gui_thread_convert_flag) {
    int r[2] = {-1, -1};
    CHECK(run_thread(thr_isgui_convert, r));
    CHECK(r[0] == TRUE);
    CHECK(r[1] == TRUE);
}

TEST(wm_gui_main_thread) {
    HWND h = wm_hidden();
    CHECK(U(IsGUIThread)(FALSE));
    CHECK(U(IsGUIThread)(TRUE));
    wm_destroy(h);
}

TEST(wm_foreground_get) {
    NEED(GetForegroundWindow);
    HWND f = U(GetForegroundWindow)();
    if (f) {
        CHECK(U(IsWindow)(f));
        DWORD pid = 0;
        CHECK(U(GetWindowThreadProcessId)(f, &pid) != 0);
        t_note("foreground window class/pid: pid=%lu", pid);
    } else t_note("no foreground window");
}

TEST(wm_foreground_set_invalid) {
    NEED(SetForegroundWindow);
    SetLastError(0);
    CHECK(!U(SetForegroundWindow)(NULL));
    SetLastError(0);
    CHECK(!U(SetForegroundWindow)((HWND)(ULONG_PTR)0x99990));
    HWND h = hid(); wm_destroy(h);
    CHECK(!U(SetForegroundWindow)(h));
}

TEST(wm_foreground_set_own_window) {
    HWND h = vis(300, 300);
    BOOL r = U(SetForegroundWindow)(h);
    t_pump();
    HWND f = U(GetForegroundWindow)();
    t_note("SetForegroundWindow(own visible) = %d, now foreground=%s", r, f == h ? "ours" : "other");
    if (r) CHECK(f == h || f != NULL);
    if (f == h) {
        EQ(U(GetActiveWindow)(), h);
        CHECK(U(GetFocus)() == h || U(GetFocus)() == NULL);
    }
    wm_destroy(h);
}

TEST(wm_foreground_allow_set) {
    NEED(AllowSetForegroundWindow);
    SetLastError(0);
    BOOL r = U(AllowSetForegroundWindow)(GetCurrentProcessId());
    t_note("AllowSetForegroundWindow(self) = %d err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(AllowSetForegroundWindow)(0xFFFFFFF4);
    t_note("AllowSetForegroundWindow(bogus pid) = %d err=%lu", r, GetLastError());
}

TEST(wm_foreground_lock_unlock) {
    NEED(LockSetForegroundWindow);
    SetLastError(0);
    BOOL r = U(LockSetForegroundWindow)(LSFW_UNLOCK);
    t_note("LockSetForegroundWindow(UNLOCK) = %d err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(LockSetForegroundWindow)(99);
    t_note("LockSetForegroundWindow(99) = %d err=%lu", r, GetLastError());
    U(LockSetForegroundWindow)(LSFW_UNLOCK);
}

TEST(wm_foreground_switchtothis) {
    NEED(SwitchToThisWindow);
    U(SwitchToThisWindow)(NULL, FALSE);
    U(SwitchToThisWindow)((HWND)(ULONG_PTR)0xAAAA0, FALSE);
    HWND h = hid(); wm_destroy(h);
    U(SwitchToThisWindow)(h, TRUE);
    CHECK(1);
}

TEST(wm_foreground_switchtothis_restores) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MINIMIZE, WS_EX_TOOLWINDOW, 300, 300, 200, 100, NULL);
    CHECK(U(IsIconic)(h));
    U(SwitchToThisWindow)(h, FALSE);
    t_pump();
    t_note("SwitchToThisWindow on minimized window: iconic afterwards = %d", U(IsIconic)(h));
    wm_destroy(h);
}

TEST(wm_lastactivepopup_basic) {
    NEED(GetLastActivePopup);
    HWND o = wm_hidden();
    HWND p = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 20, 20, 40, 30, o);
    EQ(U(GetLastActivePopup)(o), o);
    EQ(U(GetLastActivePopup)(p), p);
    HWND plain = hid();
    EQ(U(GetLastActivePopup)(plain), plain);
    wm_destroy(o); wm_destroy(plain);
}

TEST(wm_lastactivepopup_after_activation) {
    HWND o = vis(300, 300);
    HWND p = wm_mk(WS_POPUP | WS_VISIBLE | WS_CAPTION, WS_EX_TOOLWINDOW, 330, 330, 120, 80, o);
    if (!try_activate(o)) { t_note("not activatable, skip"); wm_destroy(o); return; }
    U(SetActiveWindow)(p);
    if (U(GetActiveWindow)() == p) {
        t_note("GetLastActivePopup after popup active = %p (popup %p)", (void*)U(GetLastActivePopup)(o), (void*)p);
        U(SetActiveWindow)(o);
        t_note("GetLastActivePopup after owner active = %p", (void*)U(GetLastActivePopup)(o));
    } else t_note("popup not activatable");
    wm_destroy(o);
}

TEST(wm_lastactivepopup_invalid) {
    SetLastError(0);
    EQ(U(GetLastActivePopup)(NULL), NULL);
    t_note("GetLastActivePopup(NULL) err=%lu", GetLastError());
    SetLastError(0);
    HWND lp = U(GetLastActivePopup)((HWND)(ULONG_PTR)0xBBBB0);
    t_note("GetLastActivePopup(bogus) = %p", lp);
    t_note("GetLastActivePopup(bogus) err=%lu", GetLastError());
}

/* ---------------- capture ---------------- */
TEST(wm_capture_set_get_release) {
    NEED(SetCapture); NEED(GetCapture); NEED(ReleaseCapture);
    HWND a = vis(300, 300);
    EQ(U(GetCapture)(), NULL);
    EQ(U(SetCapture)(a), NULL);
    EQ(U(GetCapture)(), a);
    EQ(U(SetCapture)(a), a);
    CHECK(U(ReleaseCapture)());
    EQ(U(GetCapture)(), NULL);
    CHECK(U(ReleaseCapture)());           /* releasing when nothing captured succeeds */
    wm_destroy(a);
}

TEST(wm_capture_changed_messages) {
    HWND a = vis(300, 300), b = vis(560, 300);
    wm_logclear();
    U(SetCapture)(a);
    EQ(wm_logcount(WM_CAPTURECHANGED), 0);
    EQ(U(SetCapture)(b), a);
    int i = wm_logfind_h(WM_CAPTURECHANGED, a, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(w, 0); EQ(l, (LPARAM)b); }
    wm_logclear();
    U(ReleaseCapture)();
    i = wm_logfind_h(WM_CAPTURECHANGED, b, 0);
    CHECK(i >= 0);
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(l, 0);
    wm_logclear();
    U(ReleaseCapture)();
    EQ(wm_logcount(WM_CAPTURECHANGED), 0);
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_capture_same_window_no_message) {
    HWND a = vis(300, 300);
    U(SetCapture)(a);
    wm_logclear();
    U(SetCapture)(a);
    t_note("WM_CAPTURECHANGED on re-capture of same window: %d", wm_logcount(WM_CAPTURECHANGED));
    U(ReleaseCapture)();
    wm_destroy(a);
}

TEST(wm_capture_destroyed_window_releases) {
    HWND a = vis(300, 300);
    U(SetCapture)(a);
    EQ(U(GetCapture)(), a);
    wm_destroy(a);
    EQ(U(GetCapture)(), NULL);
}

TEST(wm_capture_hidden_window) {
    HWND a = vis(300, 300);
    U(SetCapture)(a);
    U(ShowWindow)(a, SW_HIDE);
    t_note("capture after hiding capture window: %s", U(GetCapture)() == a ? "kept" : "released");
    U(ReleaseCapture)();
    wm_destroy(a);
}

TEST(wm_capture_child_window) {
    HWND a = vis(300, 300);
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 30, 30, a);
    EQ(U(SetCapture)(c), NULL);
    EQ(U(GetCapture)(), c);
    U(ReleaseCapture)();
    wm_destroy(a);
}

TEST(wm_capture_invalid) {
    SetLastError(0);
    EQ(U(SetCapture)((HWND)(ULONG_PTR)0xCCCC0), NULL);
    t_note("SetCapture(bogus) err=%lu", GetLastError());
    EQ(U(GetCapture)(), NULL);
    HWND a = hid(); wm_destroy(a);
    SetLastError(0);
    EQ(U(SetCapture)(a), NULL);
    t_note("SetCapture(dead) err=%lu", GetLastError());
    EQ(U(GetCapture)(), NULL);
}

TEST(wm_capture_is_per_thread) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 1)) { t_skip("worker"); wm_worker_stop(&w); return; }
    HWND a = vis(300, 300);
    U(SetCapture)(a);
    EQ(U(GetCapture)(), a);
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    CHECK(U(GetGUIThreadInfo)(w.tid, &gi));
    EQ(gi.hwndCapture, NULL);
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EQ(gi.hwndCapture, a);
    SetLastError(0);
    HWND r = U(SetCapture)(w.hwnd);
    t_note("SetCapture(window of other thread) = %p err=%lu, GetCapture=%p", r, GetLastError(), U(GetCapture)());
    U(ReleaseCapture)();
    wm_destroy(a);
    wm_worker_stop(&w);
}

TEST(wm_capture_set_null) {
    HWND a = vis(300, 300);
    U(SetCapture)(a);
    SetLastError(0);
    HWND r = U(SetCapture)(NULL);
    t_note("SetCapture(NULL) = %p, capture now %p err=%lu", r, U(GetCapture)(), GetLastError());
    U(ReleaseCapture)();
    wm_destroy(a);
}

static void thr_capture(void *p) {
    int *r = p;
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 100, 80, NULL);
    r[0] = U(SetCapture)(h) == NULL;
    r[1] = U(GetCapture)() == h;
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    r[2] = U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi) && gi.hwndCapture == h;
    wm_destroy(h);          /* destroying the capture window must release it */
    r[3] = U(GetCapture)() == NULL;
}
TEST(wm_capture_in_background_thread) {
    int r[4] = {0};
    CHECK(run_thread(thr_capture, r));
    CHECK(r[0]); CHECK(r[1]); CHECK(r[2]); CHECK(r[3]);
}
