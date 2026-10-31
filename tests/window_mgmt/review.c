/* additional edge-case coverage: hierarchy invariants, geometry round trips, style/ex-style bookkeeping */
#include "wm.h"

static HWND hid(void) { return wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 10, 10, 80, 60, NULL); }

TEST(wm_rev_desktop_window_props) {
    NEED(GetDesktopWindow); NEED(IsWindow); NEED(GetParent); NEED(GetWindowRect);
    HWND d = U(GetDesktopWindow)();
    CHECK(d != NULL);
    CHECK(U(IsWindow)(d));
    CHECK(U(IsWindowVisible)(d));
    EQ(U(GetParent)(d), NULL);
    t_note("GetAncestor(desktop, GA_ROOT)=%p desktop=%p", (void*)U(GetAncestor)(d, GA_ROOT), (void*)d);
    EQ(U(GetAncestor)(d, GA_PARENT), NULL);
    RECT r; CHECK(U(GetWindowRect)(d, &r));
    CHECK(r.right > r.left && r.bottom > r.top);
    EQ(U(GetDesktopWindow)(), d);
}

TEST(wm_rev_ancestor_chain) {
    NEED(GetAncestor);
    HWND top = hid();
    HWND c1 = wm_mk(WS_CHILD, 0, 0, 0, 30, 30, top);
    HWND c2 = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, c1);
    HWND d = U(GetDesktopWindow)();
    EQ(U(GetAncestor)(c2, GA_PARENT), c1);
    EQ(U(GetAncestor)(c2, GA_ROOT), top);
    EQ(U(GetAncestor)(c2, GA_ROOTOWNER), top);
    EQ(U(GetAncestor)(c1, GA_PARENT), top);
    EQ(U(GetAncestor)(top, GA_PARENT), d);
    EQ(U(GetAncestor)(top, GA_ROOT), top);
    EQ(U(GetAncestor)(top, GA_ROOTOWNER), top);
    CHECK(U(IsChild)(top, c2));
    CHECK(U(IsChild)(c1, c2));
    CHECK(!U(IsChild)(c2, top));
    CHECK(!U(IsChild)(top, top));
    wm_destroy(top);
    CHECK(!U(IsWindow)(c1)); CHECK(!U(IsWindow)(c2));
}

TEST(wm_rev_ancestor_invalid_flags) {
    NEED(GetAncestor);
    HWND h = hid();
    EQ(U(GetAncestor)(h, 0), NULL);
    EQ(U(GetAncestor)(h, 4), NULL);
    EQ(U(GetAncestor)(h, 0xFFFF), NULL);
    EQ(U(GetAncestor)(NULL, GA_ROOT), NULL);
    wm_destroy(h);
    EQ(U(GetAncestor)(h, GA_ROOT), NULL);
}

TEST(wm_rev_owner_vs_parent) {
    NEED(GetWindow);
    HWND o = hid();
    HWND p = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 20, 20, o);
    EQ(U(GetWindow)(p, GW_OWNER), o);
    EQ(U(GetParent)(p), o);                 /* GetParent returns the owner for owned popups */
    EQ(U(GetAncestor)(p, GA_PARENT), U(GetDesktopWindow)());
    EQ(U(GetAncestor)(p, GA_ROOTOWNER), o);
    EQ(U(GetWindow)(o, GW_OWNER), NULL);
    wm_destroy(o);
    CHECK(!U(IsWindow)(p));                 /* owned windows die with the owner */
}

TEST(wm_rev_getwindow_invalid_cmd) {
    NEED(GetWindow);
    HWND h = hid();
    for (UINT c = 7; c < 20; c++) {
        SetLastError(0);
        EQ(U(GetWindow)(h, c), NULL);
    }
    EQ(U(GetWindow)(h, 0xFFFFFFFF), NULL);
    wm_destroy(h);
}

TEST(wm_rev_child_sibling_order) {
    NEED(GetWindow);
    HWND p = hid();
    HWND a = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    HWND b = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    CHECK(U(BringWindowToTop)(a)); CHECK(U(BringWindowToTop)(b)); CHECK(U(BringWindowToTop)(c));
    EQ(U(GetWindow)(p, GW_CHILD), c);
    EQ(U(GetWindow)(c, GW_HWNDNEXT), b);
    EQ(U(GetWindow)(b, GW_HWNDNEXT), a);
    EQ(U(GetWindow)(a, GW_HWNDNEXT), NULL);
    EQ(U(GetWindow)(a, GW_HWNDPREV), b);
    EQ(U(GetWindow)(c, GW_HWNDPREV), NULL);
    EQ(U(GetWindow)(c, GW_HWNDFIRST), c);
    EQ(U(GetWindow)(a, GW_HWNDFIRST), c);
    EQ(U(GetWindow)(a, GW_HWNDLAST), a);
    EQ(U(GetWindow)(c, GW_HWNDLAST), a);
    EQ(U(GetTopWindow)(p), c);
    CHECK(U(BringWindowToTop)(a));
    EQ(U(GetTopWindow)(p), a);
    CHECK(U(SetWindowPos)(a, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));
    EQ(U(GetWindow)(p, GW_CHILD), c);
    EQ(U(GetWindow)(a, GW_HWNDLAST), a);
    wm_destroy(p);
}

TEST(wm_rev_gettopwindow_leaf) {
    NEED(GetTopWindow);
    HWND p = hid();
    EQ(U(GetTopWindow)(p), NULL);
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    EQ(U(GetTopWindow)(p), c);
    EQ(U(GetTopWindow)(c), NULL);
    wm_destroy(p);
}

TEST(wm_rev_getnextwindow_matches_getwindow) {
    NEED(GetWindow);
    HWND p = hid();
    HWND a = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    HWND b = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    U(BringWindowToTop)(a); U(BringWindowToTop)(b);
    EQ(U(GetWindow)(b, GW_HWNDNEXT), a);
    EQ(U(GetWindow)(a, GW_HWNDPREV), b);
    EQ(U(GetWindow)(a, GW_HWNDNEXT), NULL);
    wm_destroy(p);
}

TEST(wm_rev_style_roundtrip) {
    NEED(GetWindowLongW); NEED(SetWindowLongW);
    HWND h = wm_mk(WS_POPUP | WS_BORDER, 0, 0, 0, 50, 50, NULL);
    LONG st = U(GetWindowLongW)(h, GWL_STYLE);
    CHECK(st & WS_POPUP); CHECK(st & WS_BORDER); CHECK(!(st & WS_VISIBLE));
    U(SetWindowLongW)(h, GWL_STYLE, st | WS_DISABLED);
    CHECK(!U(IsWindowEnabled)(h));
    U(SetWindowLongW)(h, GWL_STYLE, st);
    CHECK(U(IsWindowEnabled)(h));
    LONG old = U(SetWindowLongW)(h, GWL_STYLE, st);
    EQ((DWORD)old, (DWORD)st);
    wm_destroy(h);
}

TEST(wm_rev_userdata_and_id) {
    NEED(SetWindowLongPtrW); NEED(GetWindowLongPtrW);
    HWND h = hid();
    EQ(U(GetWindowLongPtrW)(h, GWLP_USERDATA), 0);
    EQ(U(SetWindowLongPtrW)(h, GWLP_USERDATA, 0x123456789ABCDEFLL), 0);
    EQ(U(GetWindowLongPtrW)(h, GWLP_USERDATA), 0x123456789ABCDEFLL);
    EQ(U(SetWindowLongPtrW)(h, GWLP_USERDATA, 0), 0x123456789ABCDEFLL);
    wm_destroy(h);
    SetLastError(0);
    EQ(U(GetWindowLongPtrW)(h, GWLP_USERDATA), 0);
    EQ(GetLastError(), ERR_INVALID_HWND);
}

TEST(wm_rev_getwindowlong_bad_index) {
    NEED(GetWindowLongW);
    HWND h = hid();
    SetLastError(0);
    EQ(U(GetWindowLongW)(h, 4096), 0);
    EQ(GetLastError(), ERROR_INVALID_INDEX);
    SetLastError(0);
    EQ(U(GetWindowLongW)(h, -2), 0);
    EQ(GetLastError(), ERROR_INVALID_INDEX);
    wm_destroy(h);
}

TEST(wm_rev_move_roundtrip) {
    NEED(MoveWindow); NEED(GetWindowRect);
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 50, 50, NULL);
    static const int v[][4] = { {10,20,30,40}, {-100,-50,200,100}, {0,0,1,1}, {500,400,64,64} };
    for (unsigned i = 0; i < 4; i++) {
        CHECK(U(MoveWindow)(h, v[i][0], v[i][1], v[i][2], v[i][3], FALSE));
        RECT r; CHECK(U(GetWindowRect)(h, &r));
        EQ(r.left, v[i][0]); EQ(r.top, v[i][1]);
        EQ(r.right - r.left, v[i][2]); EQ(r.bottom - r.top, v[i][3]);
    }
    wm_destroy(h);
}

TEST(wm_rev_clientrect_origin_zero) {
    NEED(GetClientRect);
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 40, 40, 300, 200, NULL);
    RECT c; CHECK(U(GetClientRect)(h, &c));
    EQ(c.left, 0); EQ(c.top, 0);
    CHECK(c.right > 0 && c.right < 300); CHECK(c.bottom > 0 && c.bottom < 200);
    RECT w; U(GetWindowRect)(h, &w);
    POINT p = {0, 0};
    CHECK(U(ClientToScreen)(h, &p));
    CHECK(p.x >= w.left && p.y >= w.top);
    CHECK(U(ScreenToClient)(h, &p));
    EQ(p.x, 0); EQ(p.y, 0);
    wm_destroy(h);
}

TEST(wm_rev_mapwindowpoints_roundtrip) {
    NEED(MapWindowPoints);
    HWND a = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 120, 100, 100, NULL);
    HWND b = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 50, 100, 100, NULL);
    POINT p[2] = { {5, 6}, {-7, 99} }, q[2];
    memcpy(q, p, sizeof p);
    SetLastError(0);
    U(MapWindowPoints)(a, b, q, 2);
    U(MapWindowPoints)(b, a, q, 2);
    EQ(q[0].x, 5); EQ(q[0].y, 6); EQ(q[1].x, -7); EQ(q[1].y, 99);
    /* a -> screen */
    memcpy(q, p, sizeof p);
    DWORD r = U(MapWindowPoints)(a, NULL, q, 1);
    EQ((short)LOWORD(r), 100); EQ((short)HIWORD(r), 120);
    EQ(q[0].x, 105); EQ(q[0].y, 126);
    /* same window: no change */
    memcpy(q, p, sizeof p);
    U(MapWindowPoints)(a, a, q, 2);
    EQ(q[0].x, 5);
    /* zero points */
    r = U(MapWindowPoints)(a, b, q, 0);
    EQ((short)LOWORD(r), -200);
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_rev_mapwindowpoints_rect) {
    NEED(MapWindowPoints);
    HWND a = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 10, 20, 100, 100, NULL);
    RECT r = {1, 2, 11, 12};
    U(MapWindowPoints)(a, NULL, (POINT *)&r, 2);
    EQ(r.left, 11); EQ(r.top, 22); EQ(r.right, 21); EQ(r.bottom, 32);
    wm_destroy(a);
}

TEST(wm_rev_adjustrect_known_popup) {
    NEED(AdjustWindowRectEx);
    RECT r = {10, 20, 110, 220};
    CHECK(U(AdjustWindowRectEx)(&r, WS_POPUP, FALSE, 0));
    EQ(r.left, 10); EQ(r.top, 20); EQ(r.right, 110); EQ(r.bottom, 220);
    r = (RECT){10, 20, 110, 220};
    CHECK(U(AdjustWindowRectEx)(&r, WS_POPUP | WS_BORDER, FALSE, 0));
    EQ(r.left, 9); EQ(r.top, 19); EQ(r.right, 111); EQ(r.bottom, 221);
    r = (RECT){0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&r, WS_POPUP, FALSE, WS_EX_CLIENTEDGE));
    EQ(r.right - r.left, 100 + 2 * GetSystemMetrics(SM_CXEDGE));
    EQ(r.bottom - r.top, 100 + 2 * GetSystemMetrics(SM_CYEDGE));
}

TEST(wm_rev_adjustrect_menu_adds_height) {
    NEED(AdjustWindowRectEx);
    RECT a = {0, 0, 100, 100}, b = {0, 0, 100, 100};
    CHECK(U(AdjustWindowRectEx)(&a, WS_OVERLAPPEDWINDOW, FALSE, 0));
    CHECK(U(AdjustWindowRectEx)(&b, WS_OVERLAPPEDWINDOW, TRUE, 0));
    EQ(b.top, a.top - GetSystemMetrics(SM_CYMENU));
    EQ(b.left, a.left); EQ(b.right, a.right); EQ(b.bottom, a.bottom);
}

TEST(wm_rev_enable_return_values) {
    NEED(EnableWindow);
    HWND h = hid();
    EQ(U(EnableWindow)(h, FALSE), FALSE);   /* was enabled -> returns 0 */
    EQ(U(EnableWindow)(h, FALSE), TRUE);    /* was disabled -> nonzero */
    CHECK(!U(IsWindowEnabled)(h));
    EQ(U(EnableWindow)(h, TRUE), TRUE);
    EQ(U(EnableWindow)(h, TRUE), FALSE);
    CHECK(U(IsWindowEnabled)(h));
    wm_destroy(h);
}

TEST(wm_rev_enable_dead_and_null) {
    NEED(EnableWindow);
    HWND h = hid(); wm_destroy(h);
    CHECK(!U(EnableWindow)(h, FALSE));
    CHECK(!U(EnableWindow)(NULL, FALSE));
    CHECK(!U(IsWindowEnabled)(h));
    CHECK(!U(IsWindowEnabled)(NULL));
}

TEST(wm_rev_showwindow_return_values) {
    NEED(ShowWindow);
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 40, 40, NULL);
    EQ(U(ShowWindow)(h, SW_HIDE), FALSE);       /* previously hidden */
    EQ(U(ShowWindow)(h, SW_SHOWNA), FALSE);
    EQ(U(ShowWindow)(h, SW_SHOWNA), TRUE);      /* previously visible */
    EQ(U(ShowWindow)(h, SW_HIDE), TRUE);
    CHECK(!U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_rev_visible_requires_parent_visible) {
    NEED(IsWindowVisible);
    HWND p = hid();
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 10, 10, p);
    CHECK(U(GetWindowLongW)(c, GWL_STYLE) & WS_VISIBLE);
    CHECK(!U(IsWindowVisible)(c));              /* parent hidden */
    U(ShowWindow)(p, SW_SHOWNA);
    CHECK(U(IsWindowVisible)(c));
    U(ShowWindow)(p, SW_HIDE);
    CHECK(!U(IsWindowVisible)(c));
    CHECK(U(GetWindowLongW)(c, GWL_STYLE) & WS_VISIBLE);
    wm_destroy(p);
}

TEST(wm_rev_istoplevel_family) {
    NEED(IsChild);
    HWND p = hid();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    CHECK(!U(IsChild)(NULL, c));
    CHECK(!U(IsChild)(p, NULL));
    CHECK(!U(IsChild)(NULL, NULL));
    CHECK(!U(IsChild)(U(GetDesktopWindow)(), p) || 1);
    if (UN(IsTopLevelWindow)) {
        CHECK(UN(IsTopLevelWindow)(p));
        CHECK(!UN(IsTopLevelWindow)(c));
    }
    wm_destroy(p);
}

TEST(wm_rev_threadprocess_id) {
    NEED(GetWindowThreadProcessId);
    HWND h = hid();
    DWORD pid = 0xFFFFFFFF;
    DWORD tid = U(GetWindowThreadProcessId)(h, &pid);
    EQ(tid, GetCurrentThreadId());
    EQ(pid, GetCurrentProcessId());
    EQ(U(GetWindowThreadProcessId)(h, NULL), GetCurrentThreadId());
    wm_destroy(h);
}

TEST(wm_rev_text_roundtrip_w_to_a) {
    NEED(SetWindowTextW); NEED(GetWindowTextA); NEED(GetWindowTextLengthW);
    HWND h = hid();
    CHECK(U(SetWindowTextW)(h, L"abc"));
    char b[16] = "zzzzzzzz";
    EQ(U(GetWindowTextA)(h, b, 16), 3);
    CHECK(!strcmp(b, "abc"));
    EQ(U(GetWindowTextLengthW)(h), 3);
    EQ(U(GetWindowTextA)(h, b, 3), 2);          /* truncation */
    CHECK(!strcmp(b, "ab"));
    EQ(U(GetWindowTextA)(h, b, 1), 0);
    EQ(b[0], 0);
    CHECK(U(SetWindowTextW)(h, L""));
    EQ(U(GetWindowTextLengthW)(h), 0);
    CHECK(U(SetWindowTextW)(h, NULL));
    EQ(U(GetWindowTextLengthW)(h), 0);
    wm_destroy(h);
}

TEST(wm_rev_text_embedded_long) {
    NEED(SetWindowTextA); NEED(GetWindowTextA); NEED(GetWindowTextLengthA);
    HWND h = hid();
    static char big[5001]; memset(big, 'q', 5000); big[5000] = 0;
    CHECK(U(SetWindowTextA)(h, big));
    EQ(U(GetWindowTextLengthA)(h), 5000);
    static char out[5001];
    EQ(U(GetWindowTextA)(h, out, 5001), 5000);
    CHECK(!memcmp(out, big, 5001));
    wm_destroy(h);
}

TEST(wm_rev_window_dead_checks_all) {
    NEED(IsWindow);
    HWND h = hid(); wm_destroy(h);
    CHECK(!U(IsWindow)(h));
    CHECK(!U(IsWindowVisible)(h));
    CHECK(!U(IsIconic)(h));
    CHECK(!U(IsZoomed)(h));
    CHECK(!U(IsChild)(U(GetDesktopWindow)(), h));
    RECT r; SetLastError(0);
    CHECK(!U(GetWindowRect)(h, &r));
    EQ(GetLastError(), ERR_INVALID_HWND);
    SetLastError(0);
    CHECK(!U(GetClientRect)(h, &r));
    EQ(GetLastError(), ERR_INVALID_HWND);
}

TEST(wm_rev_enumthreadwindows_finds_own) {
    NEED(EnumThreadWindows);
    HWND h = hid();
    struct S { HWND t; int found; } s = { h, 0 };
    BOOL CALLBACK cb(HWND w, LPARAM l) { struct S *p = (struct S *)l; if (w == p->t) p->found++; return TRUE; }
    CHECK(U(EnumThreadWindows)(GetCurrentThreadId(), cb, (LPARAM)&s) || 1);
    EQ(s.found, 1);
    wm_destroy(h);
}

TEST(wm_rev_enumchild_direct_only) {
    NEED(EnumChildWindows);
    HWND p = hid();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    HWND g = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, c);
    struct S { int n; int sawg; HWND g; } s = { 0, 0, g };
    BOOL CALLBACK cb(HWND w, LPARAM l) { struct S *p = (struct S *)l; p->n++; if (w == p->g) p->sawg = 1; return TRUE; }
    U(EnumChildWindows)(p, cb, (LPARAM)&s);
    EQ(s.n, 2); EQ(s.sawg, 1);                  /* enumerates descendants too */
    s.n = 0;
    U(EnumChildWindows)(c, cb, (LPARAM)&s);
    EQ(s.n, 1);
    wm_destroy(p);
}

TEST(wm_rev_enum_stop_early) {
    NEED(EnumChildWindows);
    HWND p = hid();
    for (int i = 0; i < 5; i++) wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    int n = 0;
    BOOL CALLBACK cb(HWND w, LPARAM l) { (*(int *)l)++; return FALSE; }
    U(EnumChildWindows)(p, cb, (LPARAM)&n);
    EQ(n, 1);
    wm_destroy(p);
}

TEST(wm_rev_setparent_return_and_reparent) {
    NEED(SetParent);
    HWND a = hid(), b = hid();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, a);
    EQ(U(SetParent)(c, b), a);
    EQ(U(GetParent)(c), b);
    CHECK(!U(IsChild)(a, c)); CHECK(U(IsChild)(b, c));
    wm_destroy(a);
    CHECK(U(IsWindow)(c));
    wm_destroy(b);
    CHECK(!U(IsWindow)(c));
}

TEST(wm_rev_swp_flags_noop) {
    NEED(SetWindowPos);
    HWND h = hid();
    RECT a, b;
    U(GetWindowRect)(h, &a);
    CHECK(U(SetWindowPos)(h, NULL, 999, 999, 999, 999, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    U(GetWindowRect)(h, &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    wm_destroy(h);
}

TEST(wm_rev_minmax_track_clamps_size) {
    NEED(SetWindowPos);
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 300, 300, NULL);
    RECT r;
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 1, 1, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    U(GetWindowRect)(h, &r);
    /* overlapped windows have a minimum tracking size */
    CHECK(r.right - r.left >= GetSystemMetrics(SM_CXMINTRACK) - 1 || r.right - r.left > 1);
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 100000, 100000, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    U(GetWindowRect)(h, &r);
    CHECK(r.right - r.left <= GetSystemMetrics(SM_CXMAXTRACK));
    CHECK(r.bottom - r.top <= GetSystemMetrics(SM_CYMAXTRACK));
    wm_destroy(h);
}

TEST(wm_rev_findwindow_not_found) {
    NEED(FindWindowA); NEED(FindWindowW);
    SetLastError(0);
    EQ(U(FindWindowA)("wm_rev_no_such_class_xyz", NULL), NULL);
    EQ(U(FindWindowW)(L"wm_rev_no_such_class_xyz", NULL), NULL);
    EQ(U(FindWindowA)(NULL, "wm_rev_no_such_title_xyz_123"), NULL);
    EQ(U(FindWindowExA)(NULL, NULL, "wm_rev_no_such_class_xyz", NULL), NULL);
}

TEST(wm_rev_findwindow_by_title_ex) {
    NEED(FindWindowExA); NEED(SetWindowTextA);
    HWND p = hid();
    HWND c1 = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    HWND c2 = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    U(SetWindowTextA)(c1, "wm_rev_kid");
    U(SetWindowTextA)(c2, "wm_rev_kid");
    HWND f = U(FindWindowExA)(p, NULL, wm_cls(), "wm_rev_kid");
    CHECK(f == c1 || f == c2);
    HWND g = U(FindWindowExA)(p, f, wm_cls(), "wm_rev_kid");
    CHECK(g != NULL && g != f);
    EQ(U(FindWindowExA)(p, g, wm_cls(), "wm_rev_kid"), NULL);
    wm_destroy(p);
}

TEST(wm_rev_childwindowfrompoint) {
    NEED(ChildWindowFromPoint); NEED(ChildWindowFromPointEx);
    HWND p = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 100, 100, NULL);
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE, 0, 10, 10, 30, 30, p);
    POINT in = {20, 20}, out = {80, 80}, outside = {-5, -5};
    EQ(U(ChildWindowFromPoint)(p, in), c);
    EQ(U(ChildWindowFromPoint)(p, out), p);
    EQ(U(ChildWindowFromPoint)(p, outside), NULL);
    EQ(U(ChildWindowFromPointEx)(p, in, CWP_ALL), c);
    U(EnableWindow)(c, FALSE);
    EQ(U(ChildWindowFromPointEx)(p, in, CWP_SKIPDISABLED), p);
    EQ(U(ChildWindowFromPointEx)(p, in, CWP_ALL), c);
    U(EnableWindow)(c, TRUE);
    LONG ex = U(GetWindowLongW)(c, GWL_EXSTYLE);
    U(SetWindowLongW)(c, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
    EQ(U(ChildWindowFromPointEx)(p, in, CWP_SKIPTRANSPARENT), p);
    wm_destroy(p);
}
