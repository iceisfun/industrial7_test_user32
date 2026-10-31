/* additional window-management behaviours: owned popups, minimize/restore helpers, WM_WINDOWPOSCHANGING/NCCALCSIZE
   overrides, handle lifetime */
#include "wm.h"

static HWND hid(void) { return wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 40, 40, NULL); }
static HWND tw(void) { return wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 220, 160, NULL); }

TEST(wm_extra_closewindow_minimizes) {
    NEED(CloseWindow);
    HWND h = tw();
    U(ShowWindow)(h, SW_SHOWNA);
    CHECK(U(CloseWindow)(h));
    CHECK(U(IsIconic)(h));
    CHECK(U(IsWindow)(h));          /* CloseWindow does not destroy */
    wm_destroy(h);
}

TEST(wm_extra_openicon_restores) {
    NEED(OpenIcon);
    HWND h = tw();
    U(ShowWindow)(h, SW_SHOWNA);
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    CHECK(U(IsIconic)(h));
    CHECK(U(OpenIcon)(h));
    CHECK(!U(IsIconic)(h));
    CHECK(U(IsWindowVisible)(h));
    BOOL r = U(OpenIcon)(h);
    t_note("OpenIcon on non-minimized window = %d", r);
    wm_destroy(h);
}

TEST(wm_extra_openicon_closewindow_invalid) {
    SetLastError(0);
    CHECK(!U(OpenIcon)(NULL));
    t_note("CloseWindow(NULL)=%d", U(CloseWindow)(NULL));
    CHECK(!U(OpenIcon)((HWND)(ULONG_PTR)0x6660));
    t_note("CloseWindow(bogus)=%d", U(CloseWindow)((HWND)(ULONG_PTR)0x6660));
}

TEST(wm_extra_closewindow_child) {
    HWND p = tw();
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 30, 30, p);
    BOOL r = U(CloseWindow)(c);
    t_note("CloseWindow(child) = %d iconic=%d", r, U(IsIconic)(c));
    wm_destroy(p);
}

TEST(wm_extra_showownedpopups) {
    NEED(ShowOwnedPopups);
    HWND o = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 10, 10, 100, 80, NULL);
    HWND p1 = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 20, 20, 40, 30, o);
    HWND p2 = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 20, 20, 40, 30, o);   /* hidden */
    CHECK(U(ShowOwnedPopups)(o, FALSE));
    CHECK(!U(IsWindowVisible)(p1));
    CHECK(!U(IsWindowVisible)(p2));
    CHECK(U(ShowOwnedPopups)(o, TRUE));
    CHECK(U(IsWindowVisible)(p1));
    CHECK(!U(IsWindowVisible)(p2));        /* only popups hidden by the call are restored */
    wm_destroy(o);
}

TEST(wm_extra_showownedpopups_invalid) {
    SetLastError(0);
    t_note("ShowOwnedPopups(bogus) = %d", U(ShowOwnedPopups)((HWND)(ULONG_PTR)0x7770, TRUE));
    HWND h = hid(); wm_destroy(h);
    t_note("ShowOwnedPopups(dead) = %d", U(ShowOwnedPopups)(h, FALSE));
    t_note("ShowOwnedPopups(NULL) = %d", U(ShowOwnedPopups)(NULL, TRUE));
}

TEST(wm_extra_owner_minimize_hides_popup) {
    HWND o = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 10, 10, 100, 80, NULL);
    HWND p = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 20, 20, 40, 30, o);
    wm_logclear();
    U(ShowWindow)(o, SW_SHOWMINNOACTIVE);
    CHECK(!U(IsWindowVisible)(p));
    int i = wm_logfind_h(WM_SHOWWINDOW, p, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(w, FALSE); EQ(l, SW_PARENTCLOSING); }
    wm_logclear();
    U(ShowWindow)(o, SW_RESTORE);
    CHECK(U(IsWindowVisible)(p));
    i = wm_logfind_h(WM_SHOWWINDOW, p, 0);
    CHECK(i >= 0);
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(w, TRUE); EQ(l, SW_PARENTOPENING); }
    wm_destroy(o);
}

TEST(wm_extra_setwindowlong_visible_style) {
    HWND h = hid();
    CHECK(!U(IsWindowVisible)(h));
    wm_logclear();
    U(SetWindowLongA)(h, GWL_STYLE, U(GetWindowLongA)(h, GWL_STYLE) | WS_VISIBLE);
    CHECK(U(IsWindowVisible)(h));
    EQ(wm_logcount(WM_SHOWWINDOW), 0);
    U(SetWindowLongA)(h, GWL_STYLE, U(GetWindowLongA)(h, GWL_STYLE) & ~WS_VISIBLE);
    CHECK(!U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_extra_setwindowlong_returns_previous_and_stylechange_msgs) {
    HWND h = hid();
    LONG st = U(GetWindowLongA)(h, GWL_STYLE);
    wm_logclear();
    LONG prev = U(SetWindowLongA)(h, GWL_STYLE, st | WS_BORDER);
    EQ(prev, st);
    EQ(wm_logcount(WM_STYLECHANGING), 1);
    EQ(wm_logcount(WM_STYLECHANGED), 1);
    int i = wm_logfind(WM_STYLECHANGED, 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, GWL_STYLE);
    wm_logclear();
    U(SetWindowLongA)(h, GWL_EXSTYLE, U(GetWindowLongA)(h, GWL_EXSTYLE) | WS_EX_TOPMOST);
    i = wm_logfind(WM_STYLECHANGED, 0);
    CHECK(i >= 0);
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, GWL_EXSTYLE);
    wm_destroy(h);
}

TEST(wm_extra_wpc_override_size) {
    HWND h = hid();
    wm_wpc_mode = 1;
    CHECK(U(SetWindowPos)(h, NULL, 10, 10, 200, 200, SWP_NOZORDER | SWP_NOACTIVATE));
    wm_wpc_mode = 0;
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.right - r.left, 77); EQ(r.bottom - r.top, 66);
    EQ(r.left, 10);
    wm_destroy(h);
}

TEST(wm_extra_wpc_override_nomove) {
    HWND h = hid();
    U(SetWindowPos)(h, NULL, 10, 20, 40, 40, SWP_NOZORDER | SWP_NOACTIVATE);
    wm_wpc_mode = 2;
    U(SetWindowPos)(h, NULL, 300, 400, 90, 91, SWP_NOZORDER | SWP_NOACTIVATE);
    wm_wpc_mode = 0;
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 10); EQ(r.top, 20);
    EQ(r.right - r.left, 90); EQ(r.bottom - r.top, 91);
    wm_destroy(h);
}

TEST(wm_extra_wpc_override_nosize) {
    HWND h = hid();
    wm_wpc_mode = 3;
    U(SetWindowPos)(h, NULL, 300, 400, 90, 91, SWP_NOZORDER | SWP_NOACTIVATE);
    wm_wpc_mode = 0;
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 300); EQ(r.top, 400);
    EQ(r.right - r.left, 40); EQ(r.bottom - r.top, 40);
    wm_destroy(h);
}

TEST(wm_extra_wpc_swallow_changed_no_size_move) {
    HWND h = hid();
    wm_wpc_mode = 4;
    wm_logclear();
    U(SetWindowPos)(h, NULL, 55, 66, 90, 91, SWP_NOZORDER | SWP_NOACTIVATE);
    wm_wpc_mode = 0;
    EQ(wm_logcount(WM_WINDOWPOSCHANGED), 1);
    EQ(wm_logcount(WM_MOVE), 0);
    EQ(wm_logcount(WM_SIZE), 0);
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.left, 55); EQ(r.right - r.left, 90);
    wm_destroy(h);
}

TEST(wm_extra_windowpos_flags_on_show) {
    HWND h = hid();
    wm_logclear();
    U(ShowWindow)(h, SW_SHOWNA);
    int i = wm_logfind_h(WM_WINDOWPOSCHANGING, h, 0);
    CHECK(i >= 0);
    if (i >= 0) CHECK(wm_logwpflags(i) & SWP_SHOWWINDOW);
    if (i >= 0) CHECK(wm_logwpflags(i) & SWP_NOACTIVATE);
    wm_logclear();
    U(ShowWindow)(h, SW_HIDE);
    i = wm_logfind_h(WM_WINDOWPOSCHANGING, h, 0);
    CHECK(i >= 0);
    if (i >= 0) CHECK(wm_logwpflags(i) & SWP_HIDEWINDOW);
    wm_destroy(h);
}

TEST(wm_extra_windowpos_values) {
    HWND h = hid();
    wm_logclear();
    U(SetWindowPos)(h, NULL, 31, 32, 70, 71, SWP_NOZORDER | SWP_NOACTIVATE);
    int i = wm_logfind_h(WM_WINDOWPOSCHANGING, h, 0);
    CHECK(i >= 0);
    int cx = 0, cy = 0;
    if (i >= 0 && wm_logwpsize(i, &cx, &cy)) { EQ(cx, 70); EQ(cy, 71); }
    i = wm_logfind_h(WM_WINDOWPOSCHANGED, h, 0);
    CHECK(i >= 0);
    if (i >= 0 && wm_logwpsize(i, &cx, &cy)) { EQ(cx, 70); EQ(cy, 71); }
    wm_destroy(h);
}

TEST(wm_extra_nccalcsize_override) {
    wm_ncc_mode = 1;
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 200, 150, NULL);
    wm_ncc_mode = 0;
    RECT c; U(GetClientRect)(h, &c);
    EQ(c.left, 0); EQ(c.top, 0);
    EQ(c.right, 200 - 10 - 14); EQ(c.bottom, 150 - 12 - 16);
    POINT p = {0, 0}; U(ClientToScreen)(h, &p);
    EQ(p.x, 110); EQ(p.y, 112);
    WINDOWINFO wi; memset(&wi, 0, sizeof wi); wi.cbSize = sizeof wi; U(GetWindowInfo)(h, &wi);
    EQ(wi.rcClient.left, 110); EQ(wi.rcClient.bottom, 100 + 150 - 16);
    wm_destroy(h);
}

TEST(wm_extra_nccalcsize_on_resize) {
    HWND h = hid();
    wm_ncc_mode = 1;
    U(SetWindowPos)(h, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_FRAMECHANGED);
    wm_ncc_mode = 0;
    RECT c; U(GetClientRect)(h, &c);
    EQ(c.right, 300 - 24); EQ(c.bottom, 200 - 28);
    wm_logclear();
    wm_ncc_mode = 1;
    U(SetWindowPos)(h, NULL, 0, 0, 310, 210, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    wm_ncc_mode = 0;
    int i = wm_logfind(WM_SIZE, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(l, MAKELPARAM(310 - 24, 210 - 28));
    wm_destroy(h);
}

TEST(wm_extra_handle_not_reused_immediately) {
    HWND a = hid();
    wm_destroy(a);
    HWND n[64];
    int same = 0;
    for (int i = 0; i < 64; i++) { n[i] = hid(); if (n[i] == a) same++; }
    EQ(same, 0);
    CHECK(!U(IsWindow)(a));
    for (int i = 0; i < 64; i++) wm_destroy(n[i]);
    CHECK(!U(IsWindow)(a));
}

TEST(wm_extra_handle_stale_after_many_cycles) {
    HWND first = hid();
    wm_destroy(first);
    int alive = 0;
    for (int i = 0; i < 500; i++) { HWND t = hid(); if (t == first) alive++; wm_destroy(t); }
    EQ(alive, 0);
    CHECK(!U(IsWindow)(first));
}

static DWORD WINAPI create_and_exit(LPVOID p) {
    HWND *out = p;
    out[0] = wm_mk(WS_OVERLAPPED, 0, 0, 0, 20, 20, NULL);
    out[1] = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, out[0]);
    return 0;
}
TEST(wm_extra_thread_exit_destroys_windows) {
    HWND out[2] = {0};
    HANDLE t = CreateThread(NULL, 0, create_and_exit, out, 0, NULL);
    WaitForSingleObject(t, WM_TIMEOUT_MS);
    CloseHandle(t);
    CHECK(out[0] != NULL && out[1] != NULL);
    CHECK(!U(IsWindow)(out[0]));
    CHECK(!U(IsWindow)(out[1]));
}

static DWORD WINAPI create_owned_exit(LPVOID p) {
    HWND *out = p;
    out[0] = wm_mk(WS_OVERLAPPED, 0, 0, 0, 20, 20, NULL);
    out[1] = wm_mk(WS_POPUP, 0, 0, 0, 20, 20, out[0]);
    return 0;
}
TEST(wm_extra_thread_exit_destroys_owned) {
    HWND out[2] = {0};
    HANDLE t = CreateThread(NULL, 0, create_owned_exit, out, 0, NULL);
    WaitForSingleObject(t, WM_TIMEOUT_MS);
    CloseHandle(t);
    CHECK(!U(IsWindow)(out[0]));
    CHECK(!U(IsWindow)(out[1]));
}

TEST(wm_extra_destroy_inside_wndproc_message) {
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    wm_logclear();
    U(SendMessageA)(c, WM_CLOSE, 0, 0);       /* DefWindowProc turns WM_CLOSE into DestroyWindow */
    CHECK(!U(IsWindow)(c));
    CHECK(U(IsWindow)(p));
    wm_destroy(p);
}

TEST(wm_extra_close_message_default) {
    HWND h = hid();
    wm_logclear();
    U(SendMessageA)(h, WM_CLOSE, 0, 0);
    CHECK(!U(IsWindow)(h));
    EQ(wm_logcount(WM_DESTROY), 1);
}

TEST(wm_extra_wm_nulls_on_dead) {
    HWND h = hid(); wm_destroy(h);
    SetLastError(0);
    EQ(U(SendMessageA)(h, WM_NULL, 0, 0), 0);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(PostMessageA)(h, WM_NULL, 0, 0));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_extra_window_extra_bytes_negative_indices) {
    HWND h = hid();
    LONG_PTR a = U(GetWindowLongPtrA)(h, GWL_STYLE);
    LONG_PTR b = U(GetWindowLongPtrA)(h, GWL_EXSTYLE);
    CHECK((DWORD)a == (DWORD)U(GetWindowLongA)(h, GWL_STYLE));
    CHECK((DWORD)b == (DWORD)U(GetWindowLongA)(h, GWL_EXSTYLE));
    EQ(U(GetWindowLongPtrA)(h, GWLP_USERDATA), 0);
    EQ(U(GetWindowLongPtrA)(h, GWLP_HINSTANCE), (LONG_PTR)GetModuleHandleA(NULL));
    wm_destroy(h);
}

TEST(wm_extra_isiconic_zoomed_children_and_dead) {
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD | WS_MINIMIZE, 0, 0, 0, 20, 20, p);
    CHECK(U(IsIconic)(c));
    LONG st = U(GetWindowLongA)(c, GWL_STYLE);
    U(SetWindowLongA)(c, GWL_STYLE, (st & ~WS_MINIMIZE) | WS_MAXIMIZE);
    CHECK(U(IsZoomed)(c)); CHECK(!U(IsIconic)(c));
    CHECK(!U(IsIconic)(NULL)); CHECK(!U(IsZoomed)(NULL));
    wm_destroy(p);
}
