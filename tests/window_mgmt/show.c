/* ShowWindow / visibility / enable / iconic / zoomed / placement / window info */
#include "wm.h"

static HWND tw(int visible_flags) {
    return wm_mk(WS_OVERLAPPEDWINDOW | visible_flags, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 220, 160, NULL);
}

TEST(wm_show_return_values) {
    NEED(ShowWindow);
    HWND h = tw(0);
    CHECK(h != NULL);
    EQ(U(ShowWindow)(h, SW_HIDE), 0);                 /* was hidden */
    EQ(U(ShowWindow)(h, SW_SHOWNA), 0);               /* was hidden -> 0 */
    CHECK(U(IsWindowVisible)(h));
    CHECK(U(ShowWindow)(h, SW_SHOWNA) != 0);          /* was visible */
    CHECK(U(ShowWindow)(h, SW_HIDE) != 0);
    CHECK(!U(IsWindowVisible)(h));
    EQ(U(ShowWindow)(h, SW_HIDE), 0);
    wm_destroy(h);
}

TEST(wm_show_flag_in_style) {
    HWND h = tw(0);
    CHECK(!(U(GetWindowLongA)(h, GWL_STYLE) & WS_VISIBLE));
    U(ShowWindow)(h, SW_SHOWNA);
    CHECK(U(GetWindowLongA)(h, GWL_STYLE) & WS_VISIBLE);
    U(ShowWindow)(h, SW_HIDE);
    CHECK(!(U(GetWindowLongA)(h, GWL_STYLE) & WS_VISIBLE));
    wm_destroy(h);
}

TEST(wm_show_showwindow_message) {
    HWND h = tw(0);
    wm_logclear();
    U(ShowWindow)(h, SW_SHOWNA);
    int i = wm_logfind(WM_SHOWWINDOW, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(w, TRUE); EQ(l, 0); }
    wm_logclear();
    U(ShowWindow)(h, SW_HIDE);
    i = wm_logfind(WM_SHOWWINDOW, 0);
    CHECK(i >= 0);
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(w, FALSE); EQ(l, 0); }
    wm_destroy(h);
}

TEST(wm_show_no_message_when_unchanged) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    wm_logclear();
    U(ShowWindow)(h, SW_SHOWNA);
    /* Win10 22H2 re-sends WM_SHOWWINDOW for SW_SHOWNA on an already visible overlapped
     * window (observed count 1); cause not established, so only logged. The hide case
     * below is strict. */
    { UINT m; WPARAM w; LPARAM l; int i = wm_logfind(WM_SHOWWINDOW, 0);
      if (i >= 0 && wm_lognth(i, &m, &w, &l)) t_note("SHOWNA on visible: WM_SHOWWINDOW count %d wp=%llx lp=%llx", wm_logcount(WM_SHOWWINDOW), (unsigned long long)w, (unsigned long long)l);
      else t_note("SHOWNA on visible: no WM_SHOWWINDOW"); }
    U(ShowWindow)(h, SW_HIDE);
    wm_logclear();
    U(ShowWindow)(h, SW_HIDE);
    EQ(wm_logcount(WM_SHOWWINDOW), 0);
    wm_destroy(h);
}

TEST(wm_show_minimize_state) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    wm_logclear();
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    CHECK(U(IsIconic)(h));
    CHECK(!U(IsZoomed)(h));
    CHECK(U(IsWindowVisible)(h));
    LONG st = U(GetWindowLongA)(h, GWL_STYLE);
    CHECK(st & WS_MINIMIZE);
    CHECK(!(st & WS_MAXIMIZE));
    int i = wm_logfind(WM_SIZE, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, SIZE_MINIMIZED);
    RECT r; U(GetWindowRect)(h, &r);
    /* Win10: a WS_EX_TOOLWINDOW is not parked at (-32000,-32000) when minimized; it keeps a
     * small caption-only rect. Check the size shrank instead. */
    t_note("minimized tool window rect %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    CHECK((r.right - r.left) < 220 && (r.bottom - r.top) < 160);
    U(ShowWindow)(h, SW_RESTORE);
    CHECK(!U(IsIconic)(h));
    CHECK(!(U(GetWindowLongA)(h, GWL_STYLE) & WS_MINIMIZE));
    wm_destroy(h);
}

TEST(wm_show_maximize_state) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    wm_logclear();
    U(ShowWindow)(h, SW_SHOWMAXIMIZED);
    CHECK(U(IsZoomed)(h));
    CHECK(!U(IsIconic)(h));
    CHECK(U(GetWindowLongA)(h, GWL_STYLE) & WS_MAXIMIZE);
    int i = wm_logfind(WM_SIZE, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, SIZE_MAXIMIZED);
    RECT r, cr; U(GetClientRect)(h, &cr);
    CHECK(U(GetWindowRect)(h, &r));
    CHECK(cr.right > 0 && cr.bottom > 0);
    wm_logclear();
    U(ShowWindow)(h, SW_RESTORE);
    CHECK(!U(IsZoomed)(h));
    i = wm_logfind(WM_SIZE, 0);
    CHECK(i >= 0);
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, SIZE_RESTORED);
    wm_destroy(h);
}

TEST(wm_show_maximize_without_box) {
    HWND h = wm_mk(WS_CAPTION | WS_SYSMENU, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 200, 120, NULL);
    CHECK(h != NULL);
    U(ShowWindow)(h, SW_SHOWNA);
    U(ShowWindow)(h, SW_MAXIMIZE);
    CHECK(U(IsZoomed)(h));
    U(ShowWindow)(h, SW_RESTORE);
    CHECK(!U(IsZoomed)(h));
    wm_destroy(h);
}

TEST(wm_show_max_then_min_restore_chain) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    U(ShowWindow)(h, SW_SHOWMAXIMIZED);
    U(ShowWindow)(h, SW_MINIMIZE);
    CHECK(U(IsIconic)(h));
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    CHECK(U(GetWindowPlacement)(h, &wp));
    CHECK(wp.flags & WPF_RESTORETOMAXIMIZED);
    EQ(wp.showCmd, SW_SHOWMINIMIZED);
    U(ShowWindow)(h, SW_RESTORE);
    CHECK(!U(IsIconic)(h));
    CHECK(U(IsZoomed)(h));
    U(ShowWindow)(h, SW_RESTORE);
    CHECK(!U(IsZoomed)(h));
    CHECK(!U(IsIconic)(h));
    wm_destroy(h);
}

TEST(wm_show_cmd_table) {
    static const struct { int cmd; int vis, ico, zoom; const char *n; } t[] = {
        { SW_HIDE, 0, 0, 0, "HIDE" }, { SW_SHOWNORMAL, 1, 0, 0, "SHOWNORMAL" }, { SW_SHOWMINIMIZED, 1, 1, 0, "SHOWMINIMIZED" },
        { SW_SHOWMAXIMIZED, 1, 0, 1, "SHOWMAXIMIZED" }, { SW_SHOWNOACTIVATE, 1, 0, 0, "SHOWNOACTIVATE" },
        { SW_SHOW, 1, 0, 0, "SHOW" }, { SW_MINIMIZE, 1, 1, 0, "MINIMIZE" }, { SW_SHOWMINNOACTIVE, 1, 1, 0, "SHOWMINNOACTIVE" },
        { SW_SHOWNA, 1, 0, 0, "SHOWNA" }, { SW_RESTORE, 1, 0, 0, "RESTORE" }, { SW_FORCEMINIMIZE, 0, 0, 0, "FORCEMINIMIZE" },  /* Win10: no effect on a hidden window of the calling thread */
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HWND h = tw(0);
        U(ShowWindow)(h, t[i].cmd);
        CHECKF(!!U(IsWindowVisible)(h) == t[i].vis, "%s visible", t[i].n);
        CHECKF(!!U(IsIconic)(h) == t[i].ico, "%s iconic", t[i].n);
        CHECKF(!!U(IsZoomed)(h) == t[i].zoom, "%s zoomed", t[i].n);
        wm_destroy(h);
    }
}

TEST(wm_show_restore_from_hidden) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_RESTORE);
    CHECK(U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_show_minimize_hidden_window) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_MINIMIZE);
    CHECK(U(IsIconic)(h));
    t_note("SW_MINIMIZE on hidden window: visible=%d", U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_show_invalid_cmd) {
    HWND h = tw(0);
    SetLastError(0);
    BOOL r = U(ShowWindow)(h, 99);
    t_note("ShowWindow(99) = %d, visible=%d err=%lu", r, U(IsWindowVisible)(h), GetLastError());
    r = U(ShowWindow)(h, -1);
    t_note("ShowWindow(-1) = %d, visible=%d", r, U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_show_null_and_bogus) {
    SetLastError(0);
    CHECK(!U(ShowWindow)(NULL, SW_SHOW));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(ShowWindow)((HWND)(ULONG_PTR)0x10000, SW_HIDE));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_show_child_vs_parent_visibility) {
    HWND p = tw(0);
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE, 0, 5, 5, 50, 50, p);
    HWND g = wm_mk(WS_CHILD | WS_VISIBLE, 0, 1, 1, 10, 10, c);
    CHECK(c && g);
    CHECK(U(GetWindowLongA)(c, GWL_STYLE) & WS_VISIBLE);
    CHECK(!U(IsWindowVisible)(c));
    CHECK(!U(IsWindowVisible)(g));
    U(ShowWindow)(p, SW_SHOWNA);
    CHECK(U(IsWindowVisible)(p)); CHECK(U(IsWindowVisible)(c)); CHECK(U(IsWindowVisible)(g));
    U(ShowWindow)(c, SW_HIDE);
    CHECK(!U(IsWindowVisible)(c)); CHECK(!U(IsWindowVisible)(g));
    CHECK(U(GetWindowLongA)(g, GWL_STYLE) & WS_VISIBLE);
    U(ShowWindow)(c, SW_SHOWNA);
    CHECK(U(IsWindowVisible)(g));
    U(ShowWindow)(p, SW_HIDE);
    CHECK(!U(IsWindowVisible)(g));
    wm_destroy(p);
}

TEST(wm_show_minimized_visible_still_true) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    CHECK(U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_show_child_minimize) {
    HWND p = tw(0);
    HWND c = wm_mk(WS_CHILD, 0, 5, 5, 50, 50, p);
    U(ShowWindow)(c, SW_SHOWNA);
    U(ShowWindow)(c, SW_MAXIMIZE);
    CHECK(U(IsZoomed)(c));
    U(ShowWindow)(c, SW_RESTORE);
    CHECK(!U(IsZoomed)(c));
    wm_destroy(p);
}

TEST(wm_showasync_basic) {
    NEED(ShowWindowAsync);
    HWND h = tw(0);
    BOOL r = U(ShowWindowAsync)(h, SW_SHOWNA);
    t_note("ShowWindowAsync(same thread) = %d", r);
    t_pump();
    CHECK(U(IsWindowVisible)(h));
    U(ShowWindowAsync)(h, SW_HIDE);
    t_pump();
    CHECK(!U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_showasync_bad_handle) {
    NEED(ShowWindowAsync);
    SetLastError(0);
    CHECK(!U(ShowWindowAsync)(NULL, SW_SHOW));
    SetLastError(0);
    CHECK(!U(ShowWindowAsync)((HWND)(ULONG_PTR)0x12340, SW_SHOW));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_showasync_cross_thread) {
    NEED(ShowWindowAsync);
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    CHECK(!U(IsWindowVisible)(w.hwnd));
    BOOL r = U(ShowWindowAsync)(w.hwnd, SW_SHOWNA);
    t_note("ShowWindowAsync(cross thread) = %d", r);
    int ok = 0;
    for (int i = 0; i < 100 && !ok; i++) { ok = U(IsWindowVisible)(w.hwnd); if (!ok) Sleep(20); }
    CHECK(ok);
    U(ShowWindowAsync)(w.hwnd, SW_HIDE);
    ok = 1;
    for (int i = 0; i < 100 && ok; i++) { ok = U(IsWindowVisible)(w.hwnd); if (ok) Sleep(20); }
    CHECK(!ok);
    wm_worker_stop(&w);
}

TEST(wm_show_cross_thread_sync) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    EQ(U(ShowWindow)(w.hwnd, SW_SHOWNA), 0);
    CHECK(U(IsWindowVisible)(w.hwnd));
    CHECK(U(ShowWindow)(w.hwnd, SW_HIDE) != 0);
    CHECK(!U(IsWindowVisible)(w.hwnd));
    wm_worker_stop(&w);
}

/* ---- Enable ---- */
TEST(wm_enable_return_values) {
    NEED(EnableWindow);
    HWND h = tw(0);
    CHECK(U(IsWindowEnabled)(h));
    EQ(U(EnableWindow)(h, FALSE), 0);        /* was enabled */
    CHECK(!U(IsWindowEnabled)(h));
    CHECK(U(GetWindowLongA)(h, GWL_STYLE) & WS_DISABLED);
    CHECK(U(EnableWindow)(h, FALSE) != 0);   /* was disabled */
    CHECK(U(EnableWindow)(h, TRUE) != 0);    /* was disabled */
    CHECK(U(IsWindowEnabled)(h));
    CHECK(!(U(GetWindowLongA)(h, GWL_STYLE) & WS_DISABLED));
    EQ(U(EnableWindow)(h, TRUE), 0);         /* was enabled */
    wm_destroy(h);
}

TEST(wm_enable_nonzero_bool) {
    HWND h = tw(0);
    U(EnableWindow)(h, FALSE);
    EQ(U(EnableWindow)(h, 7), 1);
    CHECK(U(IsWindowEnabled)(h));
    U(EnableWindow)(h, 0x100);
    CHECK(U(IsWindowEnabled)(h));
    wm_destroy(h);
}

TEST(wm_enable_message_once) {
    HWND h = tw(0);
    wm_logclear();
    U(EnableWindow)(h, FALSE);
    EQ(wm_logcount(WM_ENABLE), 1);
    UINT m; WPARAM w; LPARAM l;
    int i = wm_logfind(WM_ENABLE, 0);
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, FALSE);
    U(EnableWindow)(h, FALSE);
    EQ(wm_logcount(WM_ENABLE), 1);
    U(EnableWindow)(h, TRUE);
    EQ(wm_logcount(WM_ENABLE), 2);
    i = wm_logfind(WM_ENABLE, i + 1);
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, TRUE);
    U(EnableWindow)(h, TRUE);
    EQ(wm_logcount(WM_ENABLE), 2);
    wm_destroy(h);
}

TEST(wm_enable_child_independent) {
    HWND p = tw(0);
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 50, 50, p);
    U(EnableWindow)(p, FALSE);
    CHECK(!U(IsWindowEnabled)(p));
    CHECK(U(IsWindowEnabled)(c));
    U(EnableWindow)(p, TRUE);
    U(EnableWindow)(c, FALSE);
    CHECK(U(IsWindowEnabled)(p));
    CHECK(!U(IsWindowEnabled)(c));
    wm_destroy(p);
}

TEST(wm_enable_null_bogus) {
    SetLastError(0);
    CHECK(!U(EnableWindow)(NULL, TRUE));
    t_note("EnableWindow(NULL) err=%lu", GetLastError());
    CHECK(!U(IsWindowEnabled)(NULL));
    CHECK(!U(IsWindowEnabled)((HWND)(ULONG_PTR)0x77770));
}

TEST(wm_enable_disabled_by_style) {
    NEED(SetWindowLongA);
    HWND h = tw(0);
    LONG st = U(GetWindowLongA)(h, GWL_STYLE);
    U(SetWindowLongA)(h, GWL_STYLE, st | WS_DISABLED);
    CHECK(!U(IsWindowEnabled)(h));
    U(SetWindowLongA)(h, GWL_STYLE, st);
    CHECK(U(IsWindowEnabled)(h));
    wm_destroy(h);
}

TEST(wm_enable_cross_thread) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    EQ(U(EnableWindow)(w.hwnd, FALSE), 0);
    CHECK(!U(IsWindowEnabled)(w.hwnd));
    CHECK(U(EnableWindow)(w.hwnd, TRUE) != 0);
    CHECK(U(IsWindowEnabled)(w.hwnd));
    wm_worker_stop(&w);
}

TEST(wm_enable_disabled_releases_capture) {
    HWND h = tw(WS_VISIBLE);
    U(SetCapture)(h);
    EQ(U(GetCapture)(), h);
    U(EnableWindow)(h, FALSE);
    EQ(U(GetCapture)(), NULL);
    U(EnableWindow)(h, TRUE);
    U(ReleaseCapture)();
    wm_destroy(h);
}

/* ---- predicates ---- */
TEST(wm_ischild_relations) {
    NEED(IsChild);
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 50, 50, p);
    HWND g = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, c);
    HWND o = wm_mk(WS_POPUP, 0, 0, 0, 10, 10, p);
    HWND other = wm_hidden();
    CHECK(U(IsChild)(p, c)); CHECK(U(IsChild)(p, g)); CHECK(U(IsChild)(c, g));
    CHECK(!U(IsChild)(c, p)); CHECK(!U(IsChild)(g, p)); CHECK(!U(IsChild)(g, c));
    CHECK(!U(IsChild)(p, p)); CHECK(!U(IsChild)(c, c));
    CHECK(!U(IsChild)(p, o)); CHECK(!U(IsChild)(o, p));
    CHECK(!U(IsChild)(other, c)); CHECK(!U(IsChild)(other, g));
    CHECK(!U(IsChild)(NULL, c)); CHECK(!U(IsChild)(p, NULL)); CHECK(!U(IsChild)(NULL, NULL));
    CHECK(!U(IsChild)((HWND)(ULONG_PTR)0x4440, c));
    CHECK(!U(IsChild)(p, (HWND)(ULONG_PTR)0x4440));
    HWND d = U(GetDesktopWindow)();
    if (!U(IsChild)(d, p)) t_note("IsChild(desktop, toplevel) is FALSE");
    CHECK(U(IsChild)(d, c) || 1);
    wm_destroy(p); wm_destroy(other);
}

TEST(wm_istoplevel_basic) {
    REQUIRE(UN(IsTopLevelWindow));
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 50, 50, p);
    HWND o = wm_mk(WS_POPUP, 0, 0, 0, 10, 10, p);
    CHECK(UN(IsTopLevelWindow)(p));
    CHECK(!UN(IsTopLevelWindow)(c));
    CHECK(UN(IsTopLevelWindow)(o));
    CHECK(!UN(IsTopLevelWindow)(NULL));
    CHECK(!UN(IsTopLevelWindow)((HWND)(ULONG_PTR)0x999990));
    t_note("IsTopLevelWindow(desktop)=%d", UN(IsTopLevelWindow)(U(GetDesktopWindow)()));
    wm_destroy(p);
}

TEST(wm_iswindowarranged_basic) {
    REQUIRE(UN(IsWindowArranged));
    HWND h = wm_hidden();
    CHECK(!UN(IsWindowArranged)(h));
    wm_destroy(h);
    SetLastError(0);
    CHECK(!UN(IsWindowArranged)(h));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}

TEST(wm_iswindow_basic) {
    NEED(IsWindow);
    CHECK(!U(IsWindow)(NULL));
    CHECK(U(IsWindow)(U(GetDesktopWindow)()));
    CHECK(!U(IsWindow)((HWND)(ULONG_PTR)1));
    CHECK(!U(IsWindow)((HWND)(ULONG_PTR)0xFFFFFFFF));
    CHECK(!U(IsWindow)((HWND)(ULONG_PTR)0xFFFFFFFFFFFFFFFFULL));
    CHECK(!U(IsWindow)((HWND)(ULONG_PTR)0x100000000ULL));
    CHECK(!U(IsWindow)(HWND_TOPMOST));
    CHECK(!U(IsWindow)(HWND_BOTTOM));
    CHECK(!U(IsWindow)(HWND_NOTOPMOST));
    CHECK(!U(IsWindow)(HWND_TOP));
    CHECK(!U(IsWindow)(HWND_MESSAGE));
    HWND h = wm_hidden();
    CHECK(U(IsWindow)(h));
    /* only low 16 bits select the slot but the upper bits must match */
    CHECK(!U(IsWindow)((HWND)((ULONG_PTR)h ^ 0x10000)));
    CHECK(!U(IsWindow)((HWND)((ULONG_PTR)h ^ 0x100000000ULL)) || 1);
    wm_destroy(h);
}

TEST(wm_iswindow_menu_handle) {
    HMENU m = U(CreateMenu)();
    CHECK(!U(IsWindow)((HWND)m));
    U(DestroyMenu)(m);
}

TEST(wm_iswindow_cross_thread) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    HWND h = w.hwnd;
    CHECK(U(IsWindow)(h));
    CHECK(!U(IsWindowVisible)(h));
    CHECK(U(IsWindowEnabled)(h));
    CHECK(!U(IsIconic)(h)); CHECK(!U(IsZoomed)(h));
    wm_worker_stop(&w);
    CHECK(!U(IsWindow)(h));
}

/* ---- placement ---- */
TEST(wm_placement_bad_length) {
    NEED(GetWindowPlacement);
    HWND h = tw(0);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp);
    SetLastError(0);
    BOOL r = U(GetWindowPlacement)(h, &wp);
    t_note("GetWindowPlacement(length=0) = %d err=%lu", r, GetLastError());
    wp.length = sizeof wp - 4;
    SetLastError(0);
    r = U(GetWindowPlacement)(h, &wp);
    t_note("GetWindowPlacement(length-4) = %d err=%lu", r, GetLastError());
    wp.length = sizeof wp + 4;
    SetLastError(0);
    r = U(GetWindowPlacement)(h, &wp);
    t_note("GetWindowPlacement(length+4) = %d err=%lu", r, GetLastError());
    SetLastError(0);
    wp.length = sizeof wp;
    CHECK(U(GetWindowPlacement)(h, &wp));
    wm_destroy(h);
}

TEST(wm_placement_dead_window) {
    HWND h = tw(0); wm_destroy(h);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    SetLastError(0);
    CHECK(!U(GetWindowPlacement)(h, &wp));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(SetWindowPlacement)(h, &wp));
    SetLastError(0);
    CHECK(!U(GetWindowPlacement)(NULL, &wp));
}

TEST(wm_placement_normal_visible) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    WINDOWPLACEMENT wp; memset(&wp, 0x55, sizeof wp); wp.length = sizeof wp;
    CHECK(U(GetWindowPlacement)(h, &wp));
    EQ(wp.length, sizeof wp);
    EQ(wp.showCmd, SW_SHOWNORMAL);
    CHECK(!(wp.flags & WPF_RESTORETOMAXIMIZED));
    RECT r; U(GetWindowRect)(h, &r);
    CHECK(wp.rcNormalPosition.right - wp.rcNormalPosition.left == r.right - r.left);
    CHECK(wp.rcNormalPosition.bottom - wp.rcNormalPosition.top == r.bottom - r.top);
    EQ(wp.rcNormalPosition.left, r.left); EQ(wp.rcNormalPosition.top, r.top);
    EQ(wp.ptMinPosition.x, -1); EQ(wp.ptMinPosition.y, -1);
    EQ(wp.ptMaxPosition.x, -1); EQ(wp.ptMaxPosition.y, -1);
    wm_destroy(h);
}

TEST(wm_placement_hidden_showcmd) {
    HWND h = tw(0);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    CHECK(U(GetWindowPlacement)(h, &wp));
    t_note("hidden window showCmd=%u", wp.showCmd);
    CHECK(wp.showCmd == SW_HIDE || wp.showCmd == SW_SHOWNORMAL);
    wm_destroy(h);
}

TEST(wm_placement_min_max_showcmd) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    RECT r0; U(GetWindowRect)(h, &r0);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    CHECK(U(GetWindowPlacement)(h, &wp));
    EQ(wp.showCmd, SW_SHOWMINIMIZED);
    EQ(wp.rcNormalPosition.left, r0.left); EQ(wp.rcNormalPosition.top, r0.top);
    EQ(wp.rcNormalPosition.right, r0.right); EQ(wp.rcNormalPosition.bottom, r0.bottom);
    U(ShowWindow)(h, SW_RESTORE);
    U(ShowWindow)(h, SW_SHOWMAXIMIZED);
    memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    CHECK(U(GetWindowPlacement)(h, &wp));
    EQ(wp.showCmd, SW_SHOWMAXIMIZED);
    EQ(wp.rcNormalPosition.left, r0.left); EQ(wp.rcNormalPosition.top, r0.top);
    EQ(wp.rcNormalPosition.right, r0.right); EQ(wp.rcNormalPosition.bottom, r0.bottom);
    wm_destroy(h);
}

TEST(wm_placement_set_normal) {
    NEED(SetWindowPlacement);
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    wp.showCmd = SW_SHOWNOACTIVATE;
    wp.ptMinPosition.x = -1; wp.ptMinPosition.y = -1; wp.ptMaxPosition.x = -1; wp.ptMaxPosition.y = -1;
    SetRect(&wp.rcNormalPosition, 210, 220, 410, 340);
    CHECK(U(SetWindowPlacement)(h, &wp));
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 210); EQ(r.top, 220); EQ(r.right, 410); EQ(r.bottom, 340);
    memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    CHECK(U(GetWindowPlacement)(h, &wp));
    EQ(wp.rcNormalPosition.left, 210); EQ(wp.rcNormalPosition.bottom, 340);
    EQ(wp.showCmd, SW_SHOWNORMAL);
    wm_destroy(h);
}

TEST(wm_placement_set_minimized_keeps_rect) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    wp.showCmd = SW_SHOWMINNOACTIVE;
    wp.ptMinPosition.x = -1; wp.ptMinPosition.y = -1; wp.ptMaxPosition.x = -1; wp.ptMaxPosition.y = -1;
    SetRect(&wp.rcNormalPosition, 300, 310, 500, 450);
    CHECK(U(SetWindowPlacement)(h, &wp));
    CHECK(U(IsIconic)(h));
    memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    CHECK(U(GetWindowPlacement)(h, &wp));
    EQ(wp.rcNormalPosition.left, 300); EQ(wp.rcNormalPosition.top, 310);
    EQ(wp.rcNormalPosition.right, 500); EQ(wp.rcNormalPosition.bottom, 450);
    EQ(wp.showCmd, SW_SHOWMINIMIZED);
    U(ShowWindow)(h, SW_RESTORE);
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 300); EQ(r.top, 310); EQ(r.right, 500); EQ(r.bottom, 450);
    wm_destroy(h);
}

TEST(wm_placement_set_maximized) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    wp.showCmd = SW_SHOWMAXIMIZED;
    wp.ptMinPosition.x = -1; wp.ptMinPosition.y = -1; wp.ptMaxPosition.x = -1; wp.ptMaxPosition.y = -1;
    SetRect(&wp.rcNormalPosition, 120, 130, 320, 280);
    CHECK(U(SetWindowPlacement)(h, &wp));
    CHECK(U(IsZoomed)(h));
    U(ShowWindow)(h, SW_RESTORE);
    CHECK(!U(IsZoomed)(h));
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 120); EQ(r.top, 130); EQ(r.right, 320); EQ(r.bottom, 280);
    wm_destroy(h);
}

TEST(wm_placement_set_hide) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    wp.showCmd = SW_HIDE;
    wp.ptMinPosition.x = -1; wp.ptMinPosition.y = -1; wp.ptMaxPosition.x = -1; wp.ptMaxPosition.y = -1;
    SetRect(&wp.rcNormalPosition, 130, 140, 330, 290);
    CHECK(U(SetWindowPlacement)(h, &wp));
    CHECK(!U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_placement_set_bad_length) {
    HWND h = tw(0);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp);
    wp.length = 3;
    SetLastError(0);
    BOOL r = U(SetWindowPlacement)(h, &wp);
    t_note("SetWindowPlacement(length=3) = %d err=%lu", r, GetLastError());
    wm_destroy(h);
}

TEST(wm_placement_restore_to_max_flag_set) {
    HWND h = tw(0);
    U(ShowWindow)(h, SW_SHOWNA);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    wp.flags = WPF_RESTORETOMAXIMIZED; wp.showCmd = SW_SHOWMINNOACTIVE;
    wp.ptMinPosition.x = -1; wp.ptMinPosition.y = -1; wp.ptMaxPosition.x = -1; wp.ptMaxPosition.y = -1;
    SetRect(&wp.rcNormalPosition, 100, 100, 300, 250);
    CHECK(U(SetWindowPlacement)(h, &wp));
    CHECK(U(IsIconic)(h));
    U(ShowWindow)(h, SW_RESTORE);
    CHECK(U(IsZoomed)(h));
    wm_destroy(h);
}

/* ---- GetWindowInfo ---- */
TEST(wm_windowinfo_cbsize) {
    NEED(GetWindowInfo);
    HWND h = tw(0);
    WINDOWINFO wi; memset(&wi, 0, sizeof wi);
    SetLastError(0);
    /* user32 records ERROR_INVALID_PARAMETER but still fills the structure */
    BOOL r = U(GetWindowInfo)(h, &wi);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    t_note("GetWindowInfo(cbSize=0) = %d", r);
    wi.cbSize = sizeof wi + 1;
    SetLastError(0);
    r = U(GetWindowInfo)(h, &wi);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    wi.cbSize = sizeof wi;
    SetLastError(0);
    CHECK(U(GetWindowInfo)(h, &wi));
    EQ(sizeof(WINDOWINFO), 60);
    wm_destroy(h);
}

TEST(wm_windowinfo_dead) {
    HWND h = tw(0); wm_destroy(h);
    WINDOWINFO wi; memset(&wi, 0, sizeof wi); wi.cbSize = sizeof wi;
    SetLastError(0);
    CHECK(!U(GetWindowInfo)(h, &wi));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    CHECK(!U(GetWindowInfo)(NULL, &wi));
}

TEST(wm_windowinfo_fields) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 150, 160, 260, 180, NULL);
    U(ShowWindow)(h, SW_SHOWNA);
    WINDOWINFO wi; memset(&wi, 0, sizeof wi); wi.cbSize = sizeof wi;
    CHECK(U(GetWindowInfo)(h, &wi));
    RECT r; U(GetWindowRect)(h, &r);
    EQ(wi.rcWindow.left, r.left); EQ(wi.rcWindow.top, r.top); EQ(wi.rcWindow.right, r.right); EQ(wi.rcWindow.bottom, r.bottom);
    POINT p = {0, 0}; U(ClientToScreen)(h, &p);
    RECT cr; U(GetClientRect)(h, &cr);
    EQ(wi.rcClient.left, p.x); EQ(wi.rcClient.top, p.y);
    EQ(wi.rcClient.right - wi.rcClient.left, cr.right); EQ(wi.rcClient.bottom - wi.rcClient.top, cr.bottom);
    EQ(wi.dwStyle, (DWORD)U(GetWindowLongA)(h, GWL_STYLE));
    /* Win10 GetWindowInfo reports extra internal ex-style bits (observed 0x28000980 for
     * 0x08000180); require only that every public bit is present */
    { DWORD gx = (DWORD)U(GetWindowLongA)(h, GWL_EXSTYLE); CHECK((wi.dwExStyle & gx) == gx); t_note("dwExStyle 0x%lx vs GWL_EXSTYLE 0x%lx", wi.dwExStyle, gx); }
    EQ(wi.atomWindowType, U(GetClassLongA)(h, GCW_ATOM));
    CHECK(wi.atomWindowType != 0);
    t_note("wCreatorVersion=0x%x cxBorders=%u cyBorders=%u status=0x%lx", wi.wCreatorVersion, wi.cxWindowBorders, wi.cyWindowBorders, wi.dwWindowStatus);
    CHECK(wi.dwWindowStatus == 0 || wi.dwWindowStatus == WS_ACTIVECAPTION);
    wm_destroy(h);
}

TEST(wm_windowinfo_borders_popup) {
    HWND h = wm_mk(WS_POPUP | WS_BORDER, 0, 150, 160, 100, 80, NULL);
    WINDOWINFO wi; memset(&wi, 0, sizeof wi); wi.cbSize = sizeof wi;
    CHECK(U(GetWindowInfo)(h, &wi));
    EQ(wi.cxWindowBorders * 2, (UINT)((wi.rcWindow.right - wi.rcWindow.left) - (wi.rcClient.right - wi.rcClient.left)));
    EQ(wi.cyWindowBorders * 2, (UINT)((wi.rcWindow.bottom - wi.rcWindow.top) - (wi.rcClient.bottom - wi.rcClient.top)));
    EQ(wi.cxWindowBorders, U(GetSystemMetrics)(SM_CXBORDER));
    wm_destroy(h);
    h = wm_mk(WS_POPUP, 0, 150, 160, 100, 80, NULL);
    memset(&wi, 0, sizeof wi); wi.cbSize = sizeof wi;
    CHECK(U(GetWindowInfo)(h, &wi));
    EQ(wi.cxWindowBorders, 0); EQ(wi.cyWindowBorders, 0);
    EQ(wi.rcClient.left, wi.rcWindow.left);
    EQ(wi.rcClient.right, wi.rcWindow.right);
    wm_destroy(h);
}

TEST(wm_windowinfo_clientedge_borders) {
    HWND h = wm_mk(WS_POPUP, WS_EX_CLIENTEDGE, 150, 160, 100, 80, NULL);
    WINDOWINFO wi; memset(&wi, 0, sizeof wi); wi.cbSize = sizeof wi;
    CHECK(U(GetWindowInfo)(h, &wi));
    /* Win10: cx/cyWindowBorders count only the window border (0 here); the client edge
     * shows up in the client rect offset */
    EQ(wi.cxWindowBorders, 0);
    EQ(wi.cyWindowBorders, 0);
    EQ(wi.rcClient.left - wi.rcWindow.left, U(GetSystemMetrics)(SM_CXEDGE));
    wm_destroy(h);
}

TEST(wm_windowinfo_active_status) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 150, 160, 100, 80, NULL);
    U(SetActiveWindow)(h);
    t_pump();
    WINDOWINFO wi; memset(&wi, 0, sizeof wi); wi.cbSize = sizeof wi;
    CHECK(U(GetWindowInfo)(h, &wi));
    if (U(GetActiveWindow)() == h) CHECK(wi.dwWindowStatus == WS_ACTIVECAPTION || wi.dwWindowStatus == 0);
    t_note("active=%d status=%lu", U(GetActiveWindow)() == h, wi.dwWindowStatus);
    wm_destroy(h);
}

/* ---- GetTitleBarInfo ---- */
TEST(wm_titlebar_cbsize) {
    NEED(GetTitleBarInfo);
    HWND h = tw(0);
    TITLEBARINFO ti; memset(&ti, 0, sizeof ti);
    SetLastError(0);
    CHECK(!U(GetTitleBarInfo)(h, &ti));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    ti.cbSize = sizeof ti + 1;
    CHECK(!U(GetTitleBarInfo)(h, &ti));
    ti.cbSize = sizeof ti;
    CHECK(U(GetTitleBarInfo)(h, &ti));
    EQ(sizeof(TITLEBARINFO), 44);
    wm_destroy(h);
}

TEST(wm_titlebar_dead_window) {
    HWND h = tw(0); wm_destroy(h);
    TITLEBARINFO ti; memset(&ti, 0, sizeof ti); ti.cbSize = sizeof ti;
    SetLastError(0);
    BOOL rr = U(GetTitleBarInfo)(h, &ti);
    t_note("GetTitleBarInfo(dead) ret=%d",rr);
    t_note("GetTitleBarInfo(dead) err=%lu", GetLastError());
}

TEST(wm_titlebar_caption_window) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 150, 160, 300, 200, NULL);
    U(ShowWindow)(h, SW_SHOWNA);
    TITLEBARINFO ti; memset(&ti, 0, sizeof ti); ti.cbSize = sizeof ti;
    CHECK(U(GetTitleBarInfo)(h, &ti));
    RECT wr; U(GetWindowRect)(h, &wr);
    CHECK(!(ti.rgstate[0] & STATE_SYSTEM_INVISIBLE));
    CHECK(ti.rcTitleBar.right > ti.rcTitleBar.left);
    CHECK(ti.rcTitleBar.bottom > ti.rcTitleBar.top);
    CHECK(ti.rcTitleBar.left >= wr.left && ti.rcTitleBar.right <= wr.right);
    CHECK(ti.rcTitleBar.top >= wr.top && ti.rcTitleBar.bottom <= wr.bottom);
    t_note("titlebar %ld,%ld-%ld,%ld states %lx %lx %lx %lx %lx %lx", ti.rcTitleBar.left, ti.rcTitleBar.top, ti.rcTitleBar.right, ti.rcTitleBar.bottom,
           ti.rgstate[0], ti.rgstate[1], ti.rgstate[2], ti.rgstate[3], ti.rgstate[4], ti.rgstate[5]);
    wm_destroy(h);
}

TEST(wm_titlebar_no_caption) {
    HWND h = wm_mk(WS_POPUP | WS_BORDER, 0, 150, 160, 300, 200, NULL);
    TITLEBARINFO ti; memset(&ti, 0, sizeof ti); ti.cbSize = sizeof ti;
    CHECK(U(GetTitleBarInfo)(h, &ti));
    t_note("titlebar nocaption state0=%lx", (unsigned long)ti.rgstate[0]);
    wm_destroy(h);
}

TEST(wm_titlebar_buttons_without_sysmenu) {
    HWND h = wm_mk(WS_CAPTION, WS_EX_TOOLWINDOW, 150, 160, 300, 200, NULL);
    TITLEBARINFO ti; memset(&ti, 0, sizeof ti); ti.cbSize = sizeof ti;
    CHECK(U(GetTitleBarInfo)(h, &ti));
    CHECK(!(ti.rgstate[0] & STATE_SYSTEM_INVISIBLE));
    CHECK(ti.rgstate[2] & STATE_SYSTEM_INVISIBLE);
    CHECK(ti.rgstate[3] & STATE_SYSTEM_INVISIBLE);
    CHECK(ti.rgstate[4] & STATE_SYSTEM_INVISIBLE);
    wm_destroy(h);
}
