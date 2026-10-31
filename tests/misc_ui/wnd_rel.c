/* misc_ui: GetWindow matrix, owner/parent semantics, FindWindowEx, EnumChildWindows */
#include "mu.h"

#define NEED_REL() REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindow)); REQUIRE(U(DestroyWindow)); REQUIRE(U(IsWindow)); \
    if (!mu_register()) { t_skip("class"); return; }

static HWND mkpop(HWND owner) { return mu_mk(WS_POPUP, WS_EX_NOACTIVATE, 20, 20, 80, 60, owner); }

/* parent with three children c[0..2] created in order */
static HWND mk3(HWND c[3])
{
    HWND p = mu_mk(WS_POPUP, 0, 20, 20, 200, 200, NULL);
    for (int i = 0; i < 3; i++) c[i] = p ? mu_child(p, 0) : NULL;
    return p;
}

TEST(misc_ui_getwindow_child_matrix) {
    NEED_REL();
    HWND c[3]; HWND p = mk3(c);
    if (!p || !c[0] || !c[1] || !c[2]) { CHECK(0); mu_kill(p); return; }
    CHECK(U(GetWindow)(p, GW_CHILD) == c[0]);
    CHECK(U(GetTopWindow)(p) == c[0]);
    for (int i = 0; i < 3; i++) {
        CHECKF(U(GetWindow)(c[i], GW_HWNDFIRST) == c[0], "first i=%d", i);
        CHECKF(U(GetWindow)(c[i], GW_HWNDLAST) == c[2], "last i=%d", i);
        CHECKF(U(GetWindow)(c[i], GW_OWNER) == NULL, "owner i=%d", i);
        CHECKF(U(GetWindow)(c[i], GW_CHILD) == NULL, "child i=%d", i);
    }
    CHECK(U(GetWindow)(c[0], GW_HWNDNEXT) == c[1]);
    CHECK(U(GetWindow)(c[1], GW_HWNDNEXT) == c[2]);
    CHECK(U(GetWindow)(c[2], GW_HWNDNEXT) == NULL);
    CHECK(U(GetWindow)(c[0], GW_HWNDPREV) == NULL);
    CHECK(U(GetWindow)(c[1], GW_HWNDPREV) == c[0]);
    CHECK(U(GetWindow)(c[2], GW_HWNDPREV) == c[1]);
    mu_kill(p);
}
TEST(misc_ui_getwindow_enabledpopup_self) {
    NEED_REL();
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    t_note("GW_ENABLEDPOPUP(p) == p: %d", U(GetWindow)(p, GW_ENABLEDPOPUP) == p);
    t_note("GW_ENABLEDPOPUP(child) == child: %d", U(GetWindow)(c[0], GW_ENABLEDPOPUP) == c[0]);
    CHECK(U(GetWindow)(p, GW_ENABLEDPOPUP) == p || U(GetWindow)(p, GW_ENABLEDPOPUP) == NULL);
    mu_kill(p);
}
TEST(misc_ui_getwindow_enabledpopup_owned) {
    NEED_REL();
    HWND a = mu_top(), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); mu_kill(b); return; }
    t_note("GW_ENABLEDPOPUP(owner of hidden enabled popup)==popup: %d", U(GetWindow)(a, GW_ENABLEDPOPUP) == b);
    U(ShowWindow)(b, SW_SHOWNA);
    t_note("GW_ENABLEDPOPUP(owner, shown popup)==popup: %d", U(GetWindow)(a, GW_ENABLEDPOPUP) == b);
    U(EnableWindow)(b, FALSE);
    t_note("GW_ENABLEDPOPUP(owner, disabled popup)==owner: %d", U(GetWindow)(a, GW_ENABLEDPOPUP) == a);
    U(EnableWindow)(b, TRUE);
    mu_kill(a);
}
TEST(misc_ui_getwindow_invalid_handle) {
    NEED_REL();
    HWND bad[] = { NULL, BOGUS_HWND, (HWND)0x6FFF0123 };
    for (int i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++)
        for (UINT cmd = 0; cmd <= GW_ENABLEDPOPUP; cmd++) {
            SetLastError(0);
            HWND r = U(GetWindow)(bad[i], cmd);
            CHECKF(r == NULL, "h=%p cmd=%u r=%p", bad[i], cmd, r);
            CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "h=%p cmd=%u err=%lu", bad[i], cmd, GetLastError());
        }
}
TEST(misc_ui_getwindow_invalid_cmd) {
    NEED_REL();
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    UINT cmds[] = { 7, 8, 9, 100, 0xFFFF, 0x80000000, 0xFFFFFFFF };
    for (int i = 0; i < 7; i++) {
        SetLastError(0);
        HWND r = U(GetWindow)(h, cmds[i]);
        CHECKF(r == NULL, "cmd=%x r=%p", cmds[i], r);
        CHECKF(GetLastError() == 1443, "cmd=%x err=%lu (ERROR_INVALID_GW_COMMAND)", cmds[i], GetLastError());
    }
    mu_kill(h);
}
TEST(misc_ui_getwindow_destroyed_handle) {
    NEED_REL();
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    HWND c = mu_child(p, 0);
    mu_kill(p);
    SetLastError(0);
    CHECK(U(GetWindow)(c, GW_OWNER) == NULL);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    CHECK(U(GetParent)(c) == NULL);
    CHECK(!U(IsChild)(p, c));
}
TEST(misc_ui_getwindow_reorder_setwindowpos) {
    NEED_REL(); REQUIRE(U(SetWindowPos));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    CHECK(U(SetWindowPos)(c[2], HWND_TOP, 0, 0, 0, 0, f));
    CHECK(U(GetWindow)(p, GW_CHILD) == c[2]);
    CHECK(U(GetWindow)(c[2], GW_HWNDNEXT) == c[0]);
    CHECK(U(GetWindow)(c[0], GW_HWNDLAST) == c[1]);
    CHECK(U(SetWindowPos)(c[2], HWND_BOTTOM, 0, 0, 0, 0, f));
    CHECK(U(GetWindow)(p, GW_CHILD) == c[0]);
    CHECK(U(GetWindow)(c[0], GW_HWNDLAST) == c[2]);
    /* insert c[0] after c[1] */
    CHECK(U(SetWindowPos)(c[0], c[1], 0, 0, 0, 0, f));
    CHECK(U(GetWindow)(p, GW_CHILD) == c[1]);
    CHECK(U(GetWindow)(c[1], GW_HWNDNEXT) == c[0]);
    CHECK(U(GetWindow)(c[0], GW_HWNDNEXT) == c[2]);
    CHECK(U(BringWindowToTop)(c[2]));
    CHECK(U(GetWindow)(p, GW_CHILD) == c[2]);
    /* NOZORDER leaves it alone */
    CHECK(U(SetWindowPos)(c[0], HWND_TOP, 1, 1, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(U(GetWindow)(p, GW_CHILD) == c[2]);
    mu_kill(p);
}
TEST(misc_ui_getwindow_setwindowpos_insertafter_self) {
    NEED_REL(); REQUIRE(U(SetWindowPos));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    SetLastError(0);
    BOOL r = U(SetWindowPos)(c[1], c[1], 0, 0, 0, 0, f);
    t_note("SetWindowPos(insertAfter=self) -> %d err=%lu", r, GetLastError());
    CHECK(U(GetWindow)(p, GW_CHILD) == c[0] && U(GetWindow)(c[0], GW_HWNDNEXT) == c[1]);
    mu_kill(p);
}
TEST(misc_ui_getwindow_walk_counts_children) {
    NEED_REL();
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    enum { N = 40 };
    HWND c[N];
    for (int i = 0; i < N; i++) c[i] = mu_child(p, i & 1 ? WS_VISIBLE : 0);
    int n = 0;
    for (HWND w = U(GetWindow)(p, GW_CHILD); w && n < 1000; w = U(GetWindow)(w, GW_HWNDNEXT), n++)
        CHECKF(w == c[n], "pos %d", n);
    CHECK(n == N);
    n = 0;
    for (HWND w = U(GetWindow)(c[0], GW_HWNDLAST); w && n < 1000; w = U(GetWindow)(w, GW_HWNDPREV), n++)
        CHECKF(w == c[N - 1 - n], "rev pos %d", n);
    CHECK(n == N);
    mu_kill(p);
}
TEST(misc_ui_getwindow_grandchildren_not_siblings) {
    NEED_REL();
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    HWND g = mu_child(c[1], 0);
    CHECK(g != NULL);
    CHECK(U(GetWindow)(c[1], GW_CHILD) == g);
    CHECK(U(GetWindow)(g, GW_HWNDNEXT) == NULL);
    CHECK(U(GetWindow)(g, GW_HWNDFIRST) == g);
    CHECK(U(GetWindow)(c[1], GW_HWNDNEXT) == c[2]);
    CHECK(U(GetParent)(g) == c[1]);
    CHECK(U(GetAncestor)(g, GA_PARENT) == c[1]);
    CHECK(U(GetAncestor)(g, GA_ROOT) == p);
    CHECK(U(GetAncestor)(g, GA_ROOTOWNER) == p);
    CHECK(U(IsChild)(p, g));
    CHECK(U(IsChild)(c[1], g));
    CHECK(!U(IsChild)(c[0], g));
    CHECK(!U(IsChild)(g, p));
    CHECK(!U(IsChild)(p, p));
    CHECK(!U(IsChild)(c[0], c[1]));
    mu_kill(p);
}
TEST(misc_ui_ischild_invalid) {
    NEED_REL();
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    CHECK(!U(IsChild)(NULL, c[0]));
    CHECK(!U(IsChild)(p, NULL));
    CHECK(!U(IsChild)(BOGUS_HWND, c[0]));
    CHECK(!U(IsChild)(p, BOGUS_HWND));
    CHECK(!U(IsChild)(NULL, NULL));
    t_note("IsChild(desktop, top-level) = %d", U(IsChild)(U(GetDesktopWindow)(), p));
    mu_kill(p);
}
TEST(misc_ui_getparent_matrix) {
    NEED_REL();
    HWND a = mu_top(), b = mkpop(a), c = a ? mu_child(a, 0) : NULL;
    if (!a || !b || !c) { CHECK(0); mu_kill(a); return; }
    CHECK(U(GetParent)(a) == NULL);
    CHECK(U(GetParent)(b) == a);           /* owner for popups */
    CHECK(U(GetParent)(c) == a);
    CHECK(U(GetParent)(U(GetDesktopWindow)()) == NULL);
    CHECK(U(GetParent)(NULL) == NULL);
    SetLastError(0);
    CHECK(U(GetParent)(BOGUS_HWND) == NULL);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    CHECK(U(GetAncestor)(b, GA_PARENT) == U(GetDesktopWindow)());
    CHECK(U(GetAncestor)(b, GA_ROOT) == b);
    CHECK(U(GetAncestor)(b, GA_ROOTOWNER) == a);
    CHECK(U(GetAncestor)(a, GA_ROOT) == a);
    CHECK(U(GetAncestor)(a, GA_ROOTOWNER) == a);
    CHECK(U(GetAncestor)(c, GA_ROOT) == a);
    mu_kill(a);
}
TEST(misc_ui_getancestor_edge_cases) {
    NEED_REL(); REQUIRE(U(GetAncestor));
    HWND d = U(GetDesktopWindow)();
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    CHECK(U(GetAncestor)(d, GA_PARENT) == NULL);
    t_note("GetAncestor(desktop, ROOT)=%p ROOTOWNER=%p (desktop=%p)", U(GetAncestor)(d, GA_ROOT), U(GetAncestor)(d, GA_ROOTOWNER), d);
    CHECK(U(GetAncestor)(h, GA_PARENT) == d);
    CHECK(U(GetAncestor)(h, 0) == NULL);
    CHECK(U(GetAncestor)(h, 4) == NULL);
    CHECK(U(GetAncestor)(h, 0xFFFFFFFF) == NULL);
    CHECK(U(GetAncestor)(NULL, GA_PARENT) == NULL);
    CHECK(U(GetAncestor)(BOGUS_HWND, GA_ROOT) == NULL);
    mu_kill(h);
}
TEST(misc_ui_setparent_basic) {
    NEED_REL(); REQUIRE(U(SetParent));
    HWND p1 = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL), p2 = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c = mu_child(p1, 0);
    if (!p1 || !p2 || !c) { CHECK(0); mu_kill(p1); mu_kill(p2); return; }
    CHECK(U(SetParent)(c, p2) == p1);
    CHECK(U(GetParent)(c) == p2);
    CHECK(U(GetWindow)(p2, GW_CHILD) == c);
    CHECK(U(GetWindow)(p1, GW_CHILD) == NULL);
    CHECK(U(SetParent)(c, p2) == p2);            /* same parent, returns previous */
    CHECK(U(IsChild)(p2, c) && !U(IsChild)(p1, c));
    mu_kill(p1);
    CHECK(U(IsWindow)(c));                       /* moved away, survives old parent */
    mu_kill(p2);
    CHECK(!U(IsWindow)(c));
}
TEST(misc_ui_setparent_errors) {
    NEED_REL(); REQUIRE(U(SetParent));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    SetLastError(0);
    CHECK(U(SetParent)(BOGUS_HWND, p) == NULL);
    t_note("SetParent err=%lu", GetLastError());
    SetLastError(0);
    CHECK(U(SetParent)(c[0], BOGUS_HWND) == NULL);
    t_note("SetParent err=%lu", GetLastError());
    SetLastError(0);
    CHECK(U(SetParent)(NULL, p) == NULL);
    t_note("SetParent err=%lu", GetLastError());
    CHECK(U(GetParent)(c[0]) == p);
    SetLastError(0);
    CHECK(U(SetParent)(p, c[0]) == NULL);       /* parent into its own child */
    t_note("SetParent(parent, own child) err=%lu", GetLastError());
    CHECK(U(GetParent)(c[0]) == p);
    SetLastError(0);
    t_note("SetParent(h,h) = %p", (void *)U(SetParent)(p, p));
    t_note("SetParent(h,h) err=%lu", GetLastError());
    SetLastError(0);
    t_note("SetParent(desktop,p) = %p", (void *)U(SetParent)(U(GetDesktopWindow)(), p));
    t_note("SetParent(desktop,p) err=%lu", GetLastError());
    mu_kill(p);
}
TEST(misc_ui_setparent_null_makes_toplevel_parent_desktop) {
    NEED_REL(); REQUIRE(U(SetParent));
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c = mu_child(p, 0);
    if (!p || !c) { CHECK(0); return; }
    CHECK(U(SetParent)(c, NULL) == p);
    CHECK(U(GetAncestor)(c, GA_PARENT) == U(GetDesktopWindow)());
    CHECK(U(GetParent)(c) == NULL || U(GetParent)(c) == U(GetDesktopWindow)());
    t_note("after SetParent(child,NULL): style has WS_CHILD=%d", (mu_style(c) & WS_CHILD) != 0);
    mu_kill(c); mu_kill(p);
}
TEST(misc_ui_setparent_to_message_window) {
    NEED_REL(); REQUIRE(U(SetParent));
    HWND c = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    if (!c) { CHECK(0); return; }
    HWND prev = U(SetParent)(c, HWND_MESSAGE);
    CHECK(prev == U(GetDesktopWindow)() || prev == NULL);
    HWND anc = U(GetAncestor)(c, GA_PARENT);
    CHECK(anc != NULL && anc != U(GetDesktopWindow)());
    CHECK(!U(IsWindowVisible)(c));
    U(SetParent)(c, NULL);
    CHECK(U(GetAncestor)(c, GA_PARENT) == U(GetDesktopWindow)());
    mu_kill(c);
}

/* ---------- owner semantics ---------- */
TEST(misc_ui_owner_destroy_with_owner) {
    NEED_REL();
    HWND a = mu_top(), b = mkpop(a), c = mkpop(b);
    if (!a || !b || !c) { CHECK(0); mu_kill(a); return; }
    CHECK(U(GetWindow)(c, GW_OWNER) == b);
    CHECK(U(GetAncestor)(c, GA_ROOTOWNER) == a);
    CHECK(U(DestroyWindow)(a));
    CHECK(!U(IsWindow)(a));
    CHECK(!U(IsWindow)(b));
    CHECK(!U(IsWindow)(c));
}
TEST(misc_ui_owner_destroy_owned_keeps_owner) {
    NEED_REL();
    HWND a = mu_top(), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); return; }
    CHECK(U(DestroyWindow)(b));
    CHECK(U(IsWindow)(a));
    mu_kill(a);
}
TEST(misc_ui_owner_destroy_messages) {
    NEED_REL();
    HWND a = mu_top(), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); return; }
    mu_log_reset(); mu_logging = 1;
    U(DestroyWindow)(a);
    mu_logging = 0;
    CHECK(mu_count_h(a, WM_DESTROY) == 1 && mu_count_h(b, WM_DESTROY) == 1);
    CHECK(mu_count_h(a, WM_NCDESTROY) == 1 && mu_count_h(b, WM_NCDESTROY) == 1);
    int ia = mu_find_h(a, WM_DESTROY), ib = mu_find_h(b, WM_DESTROY);
    t_note("owner WM_DESTROY index %d, owned WM_DESTROY index %d", ia, ib);
}
TEST(misc_ui_owner_hide_owner_hides_owned) {
    NEED_REL(); REQUIRE(U(ShowWindow)); REQUIRE(U(IsWindowVisible));
    HWND a = mu_mk(WS_POPUP, WS_EX_NOACTIVATE, 30, 30, 120, 90, NULL), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); return; }
    U(ShowWindow)(a, SW_SHOWNA); U(ShowWindow)(b, SW_SHOWNA);
    CHECK(U(IsWindowVisible)(a) && U(IsWindowVisible)(b));
    mu_log_reset(); mu_logging = 1;
    U(ShowWindow)(a, SW_HIDE);
    mu_logging = 0;
    CHECK(!U(IsWindowVisible)(a));
    t_note("owned popup IsWindowVisible after owner hide: %d", U(IsWindowVisible)(b));
    int i = mu_find_h(b, WM_SHOWWINDOW);
    CHECK(i >= 0);
    if (i >= 0) { CHECK(mu_log[i].w == FALSE); CHECK(mu_log[i].l == SW_PARENTCLOSING); }
    mu_log_reset(); mu_logging = 1;
    U(ShowWindow)(a, SW_SHOWNA);
    mu_logging = 0;
    CHECK(U(IsWindowVisible)(a));
    t_note("owned popup IsWindowVisible after owner show: %d", U(IsWindowVisible)(b));
    i = mu_find_h(b, WM_SHOWWINDOW);
    CHECK(i >= 0);
    if (i >= 0) { CHECK(mu_log[i].w == TRUE); CHECK(mu_log[i].l == SW_PARENTOPENING); }
    mu_kill(a);
}
TEST(misc_ui_owner_minimize_owner_hides_owned) {
    NEED_REL(); REQUIRE(U(ShowWindow)); REQUIRE(U(IsWindowVisible));
    HWND a = mu_mk(WS_OVERLAPPEDWINDOW, WS_EX_NOACTIVATE, 30, 30, 200, 150, NULL), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); return; }
    U(ShowWindow)(a, SW_SHOWNA); U(ShowWindow)(b, SW_SHOWNA);
    CHECK(U(IsWindowVisible)(b));
    U(ShowWindow)(a, SW_SHOWMINNOACTIVE);
    CHECK(U(IsIconic)(a));
    t_note("owned popup visible while owner iconic: %d", U(IsWindowVisible)(b));
    U(ShowWindow)(a, SW_RESTORE);
    CHECK(!U(IsIconic)(a));
    t_note("owned popup visible after owner restore: %d", U(IsWindowVisible)(b));
    mu_kill(a);
}
TEST(misc_ui_owner_owned_stays_above_owner) {
    NEED_REL(); REQUIRE(U(SetWindowPos));
    HWND a = mu_mk(WS_POPUP, WS_EX_NOACTIVATE, 30, 30, 100, 80, NULL), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); return; }
    U(ShowWindow)(a, SW_SHOWNA); U(ShowWindow)(b, SW_SHOWNA);
    UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    U(SetWindowPos)(a, HWND_TOP, 0, 0, 0, 0, f);
    int above = 0, n = 0;
    for (HWND w = U(GetWindow)(a, GW_HWNDPREV); w && n < 10000; w = U(GetWindow)(w, GW_HWNDPREV), n++) if (w == b) above = 1;
    CHECK(above);
    U(SetWindowPos)(a, HWND_BOTTOM, 0, 0, 0, 0, f);
    above = 0; n = 0;
    for (HWND w = U(GetWindow)(a, GW_HWNDPREV); w && n < 10000; w = U(GetWindow)(w, GW_HWNDPREV), n++) if (w == b) above = 1;
    t_note("after HWND_BOTTOM on owner, owned still above: %d", above);
    mu_kill(a);
}
TEST(misc_ui_owner_ischild_false_for_owned) {
    NEED_REL();
    HWND a = mu_top(), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); return; }
    CHECK(!U(IsChild)(a, b));
    CHECK(!U(IsChild)(b, a));
    CHECK(U(GetWindow)(a, GW_CHILD) == NULL);
    mu_kill(a);
}
TEST(misc_ui_owner_owned_in_enumwindows) {
    NEED_REL(); REQUIRE(U(EnumWindows));
    HWND a = mu_top(), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); return; }
    struct S { HWND a, b; int fa, fb, n; } s = { a, b, 0, 0, 0 };
    BOOL CALLBACK cb(HWND h, LPARAM l) { struct S *s = (struct S *)l; s->n++; if (h == s->a) s->fa++; if (h == s->b) s->fb++; return s->n < 100000; }
    U(EnumWindows)(cb, (LPARAM)&s);
    CHECK(s.fa == 1);
    CHECK(s.fb == 1);
    mu_kill(a);
}
TEST(misc_ui_owner_set_owner_cycle_rejected) {
    NEED_REL();
    HWND a = mu_top(), b = mkpop(a);
    if (!a || !b) { CHECK(0); mu_kill(a); return; }
    SetLastError(0);
    LONG_PTR r = U(SetWindowLongPtrA)(a, GWLP_HWNDPARENT, (LONG_PTR)b);
    t_note("owner cycle attempt: ret=%p err=%lu owner(a)=%p", (void *)r, GetLastError(), (void *)U(GetWindow)(a, GW_OWNER));
    CHECK(U(GetWindow)(b, GW_OWNER) == a);
    mu_kill(a);
}
TEST(misc_ui_owner_toolwindow_owned_by_hidden) {
    NEED_REL();
    HWND a = mu_top();
    HWND b = mu_mk(WS_POPUP | WS_CAPTION, WS_EX_TOOLWINDOW, 0, 0, 80, 60, a);
    CHECK(a && b);
    if (b) { CHECK((mu_ex(b) & WS_EX_TOOLWINDOW) != 0); CHECK(U(GetWindow)(b, GW_OWNER) == a); }
    mu_kill(a);
}

/* ---------- EnumChildWindows ---------- */
struct ecw { HWND list[256]; int n; int stop_at; HWND destroy_target; LPARAM lp; LPARAM seen_lp; int dflt; };
static BOOL CALLBACK ecw_cb(HWND h, LPARAM l)
{
    struct ecw *s = (struct ecw *)l;
    if (s->n < 256) s->list[s->n] = h;
    s->n++;
    if (s->destroy_target && h != s->destroy_target) { DestroyWindow(s->destroy_target); s->destroy_target = NULL; }
    if (s->stop_at && s->n >= s->stop_at) return FALSE;
    return TRUE;
}
TEST(misc_ui_enumchild_order_and_depth) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    HWND g0 = mu_child(c[0], 0), g1 = mu_child(c[0], 0), g2 = mu_child(c[2], 0);
    struct ecw s; memset(&s, 0, sizeof s);
    BOOL r = U(EnumChildWindows)(p, ecw_cb, (LPARAM)&s);
    CHECK(r);
    CHECK(s.n == 6);
    HWND want[6] = { c[0], g0, g1, c[1], c[2], g2 };
    int idx[6]; for (int k = 0; k < 6; k++) { idx[k] = -1; for (int i = 0; i < s.n && i < 256; i++) if (s.list[i] == want[k]) idx[k] = i; CHECKF(idx[k] >= 0, "window %d missing", k); }
    CHECK(idx[0] < idx[3] && idx[3] < idx[4]);          /* direct children in z-order */
    CHECK(idx[1] < idx[2]);
    int exact = 1;
    for (int i = 0; i < 6 && i < s.n; i++) if (s.list[i] != want[i]) exact = 0;
    t_note("EnumChildWindows pre-order exact match: %d", exact);
    mu_kill(p);
}
TEST(misc_ui_enumchild_no_children) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    struct ecw s; memset(&s, 0, sizeof s);
    BOOL r = U(EnumChildWindows)(p, ecw_cb, (LPARAM)&s);
    CHECK(!r);
    CHECK(s.n == 0);
    mu_kill(p);
}
TEST(misc_ui_enumchild_invalid_parent) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    struct ecw s; memset(&s, 0, sizeof s);
    SetLastError(0);
    BOOL r = U(EnumChildWindows)(BOGUS_HWND, ecw_cb, (LPARAM)&s);
    CHECK(!r);
    CHECK(s.n == 0);
    t_note("EnumChildWindows(bogus) err=%lu", GetLastError());
}
TEST(misc_ui_enumchild_stop_early) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    struct ecw s; memset(&s, 0, sizeof s); s.stop_at = 2;
    BOOL r = U(EnumChildWindows)(p, ecw_cb, (LPARAM)&s);
    CHECK(!r);
    CHECK(s.n == 2);
    mu_kill(p);
}
TEST(misc_ui_enumchild_destroy_pending_child) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    struct ecw s; memset(&s, 0, sizeof s); s.destroy_target = c[2];
    U(EnumChildWindows)(p, ecw_cb, (LPARAM)&s);
    CHECK(s.n == 2);
    CHECK(s.list[0] == c[0] && s.list[1] == c[1]);
    CHECK(!U(IsWindow)(c[2]));
    mu_kill(p);
}
TEST(misc_ui_enumchild_destroy_current_child) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    static HWND cur[3];
    memcpy(cur, c, sizeof cur);
    static int cnt;
    cnt = 0;
    BOOL CALLBACK cb(HWND h, LPARAM l) { cnt++; if (h == cur[1]) DestroyWindow(h); return TRUE; }
    BOOL r = U(EnumChildWindows)(p, cb, 0);
    CHECK(r);
    CHECK(cnt == 3);
    CHECK(!U(IsWindow)(c[1]) && U(IsWindow)(c[0]) && U(IsWindow)(c[2]));
    mu_kill(p);
}
TEST(misc_ui_enumchild_destroy_parent_inside) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    static HWND pp; pp = p;
    static int cnt; cnt = 0;
    BOOL CALLBACK cb(HWND h, LPARAM l) { cnt++; DestroyWindow(pp); return TRUE; }
    U(EnumChildWindows)(p, cb, 0);
    CHECK(cnt == 1);
    CHECK(!U(IsWindow)(p));
}
TEST(misc_ui_enumchild_snapshot_ignores_new_children) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    static HWND pp; pp = p;
    static int cnt; cnt = 0;
    BOOL CALLBACK cb(HWND h, LPARAM l) { cnt++; if (cnt == 1) mu_child(pp, 0); return TRUE; }
    U(EnumChildWindows)(p, cb, 0);
    CHECK(cnt == 3);
    int n = 0;
    for (HWND w = U(GetWindow)(p, GW_CHILD); w; w = U(GetWindow)(w, GW_HWNDNEXT)) n++;
    CHECK(n == 4);
    mu_kill(p);
}
TEST(misc_ui_enumchild_lparam_passthrough) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND c[3]; HWND p = mk3(c);
    if (!p) { CHECK(0); return; }
    static LPARAM got[8]; static int n;
    n = 0;
    BOOL CALLBACK cb(HWND h, LPARAM l) { if (n < 8) got[n++] = l; return TRUE; }
    U(EnumChildWindows)(p, cb, (LPARAM)0x123456789ABCDEF0LL);
    CHECK(n == 3);
    for (int i = 0; i < n; i++) CHECK(got[i] == (LPARAM)0x123456789ABCDEF0LL);
    mu_kill(p);
}
TEST(misc_ui_enumchild_hidden_and_disabled_included) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c1 = mu_child(p, WS_VISIBLE), c2 = mu_child(p, WS_DISABLED), c3 = mu_child(p, 0);
    struct ecw s; memset(&s, 0, sizeof s);
    U(EnumChildWindows)(p, ecw_cb, (LPARAM)&s);
    CHECK(s.n == 3);
    mu_kill(p);
}
TEST(misc_ui_enumchild_desktop_includes_toplevel) {
    NEED_REL(); REQUIRE(U(EnumChildWindows));
    HWND h = mu_top();
    if (!h) { CHECK(0); return; }
    HWND c = mu_child(h, 0);
    static struct { HWND h, c; int fh, fc, n; } st;
    memset(&st, 0, sizeof st); st.h = h; st.c = c;
    BOOL CALLBACK cb(HWND w, LPARAM l) { st.n++; if (w == st.h) st.fh++; if (w == st.c) st.fc++; return st.n < 200000; }
    BOOL r = U(EnumChildWindows)(NULL, cb, 0);
    CHECK(r);
    CHECK(st.fh == 1);
    t_note("EnumChildWindows(NULL) saw nested child: %d", st.fc);
    mu_kill(h);
}
TEST(misc_ui_enumthreadwindows_basic) {
    NEED_REL(); REQUIRE(U(EnumThreadWindows));
    HWND a = mu_top(), b = mkpop(a);
    HWND c = a ? mu_child(a, 0) : NULL;
    if (!a || !b || !c) { CHECK(0); mu_kill(a); return; }
    static struct { HWND a, b, c; int fa, fb, fc; } st;
    memset(&st, 0, sizeof st); st.a = a; st.b = b; st.c = c;
    BOOL CALLBACK cb(HWND w, LPARAM l) { if (w == st.a) st.fa++; if (w == st.b) st.fb++; if (w == st.c) st.fc++; return TRUE; }
    U(EnumThreadWindows)(GetCurrentThreadId(), cb, 0);
    CHECK(st.fa == 1 && st.fb == 1);
    CHECK(st.fc == 0);
    mu_kill(a);
}
TEST(misc_ui_enumthreadwindows_other_thread_and_bogus) {
    NEED_REL(); REQUIRE(U(EnumThreadWindows));
    mu_worker w;
    if (!mu_worker_start(&w, WS_POPUP, 0, "enumthr")) { CHECK(0); mu_worker_stop(&w); return; }
    static HWND found; static int n;
    found = NULL; n = 0;
    BOOL CALLBACK cb(HWND h, LPARAM l) { n++; if (h == (HWND)l) found = h; return TRUE; }
    U(EnumThreadWindows)(w.tid, cb, (LPARAM)w.hwnd);
    CHECK(found == w.hwnd);
    CHECK(n >= 1);
    n = 0;
    U(EnumThreadWindows)(0, cb, 0);
    t_note("EnumThreadWindows(0) callbacks: %d", n); n = 0;
    U(EnumThreadWindows)(0xFFFFFF00, cb, 0);
    CHECK(n == 0);
    mu_worker_stop(&w);
}
TEST(misc_ui_enumwindows_stop_and_return) {
    NEED_REL(); REQUIRE(U(EnumWindows));
    static int n;
    n = 0;
    BOOL CALLBACK cb(HWND h, LPARAM l) { n++; return FALSE; }
    BOOL r = U(EnumWindows)(cb, 0);
    CHECK(n == 1);
    t_note("EnumWindows with early stop returns %d", r);
    n = 0;
    BOOL CALLBACK cb2(HWND h, LPARAM l) { n++; return TRUE; }
    r = U(EnumWindows)(cb2, 0);
    CHECK(r);
    CHECK(n > 0);
}
TEST(misc_ui_enumwindows_excludes_children_and_message_windows) {
    NEED_REL(); REQUIRE(U(EnumWindows));
    HWND h = mu_top(), c = h ? mu_child(h, 0) : NULL;
    HWND m = mu_mk(0, 0, 0, 0, 0, 0, HWND_MESSAGE);
    static struct { HWND c, m; int fc, fm; } st;
    memset(&st, 0, sizeof st); st.c = c; st.m = m;
    BOOL CALLBACK cb(HWND w, LPARAM l) { if (w == st.c) st.fc++; if (w == st.m) st.fm++; return TRUE; }
    U(EnumWindows)(cb, 0);
    CHECK(st.fc == 0);
    CHECK(st.fm == 0);
    mu_kill(h); mu_kill(m);
}
TEST(misc_ui_enumwindows_zorder_newest_first) {
    NEED_REL(); REQUIRE(U(EnumWindows));
    HWND a = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL), b = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL), c = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    static HWND ord[3]; static int n; static HWND ha, hb, hc;
    ha = a; hb = b; hc = c; n = 0;
    BOOL CALLBACK cb(HWND w, LPARAM l) { if (w == ha || w == hb || w == hc) { if (n < 3) ord[n] = w; n++; } return TRUE; }
    U(EnumWindows)(cb, 0);
    CHECK(n == 3);
    CHECK(ord[0] == c && ord[1] == b && ord[2] == a);
    mu_kill(a); mu_kill(b); mu_kill(c);
}

/* ---------- FindWindowEx ---------- */
static ATOM reg_unique(const char *name)
{
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = mu_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = name;
    return U(RegisterClassExA)(&wc);
}
TEST(misc_ui_findwindow_by_class_and_title) {
    NEED_REL(); REQUIRE(U(FindWindowExA)); REQUIRE(U(FindWindowA));
    HWND a = mu_mk_cls(MU_CLS, "MU_find_alpha", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    HWND b = mu_mk_cls(MU_CLS, "MU_find_beta", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    if (!a || !b) { CHECK(0); mu_kill(a); mu_kill(b); return; }
    CHECK(U(FindWindowA)(MU_CLS, "MU_find_alpha") == a);
    CHECK(U(FindWindowA)(MU_CLS, "MU_find_beta") == b);
    CHECK(U(FindWindowA)(NULL, "MU_find_alpha") == a);
    CHECK(U(FindWindowA)(MU_CLS, "MU_find_gamma") == NULL);
    CHECK(U(FindWindowA)("MU_NoSuchClass_xyz", NULL) == NULL);
    CHECK(U(FindWindowA)("MU_NoSuchClass_xyz", "MU_find_alpha") == NULL);
    CHECK(U(FindWindowExA)(NULL, NULL, MU_CLS, "MU_find_alpha") == a);
    t_note("title case-insensitive match: %d", U(FindWindowA)(MU_CLS, "mu_find_ALPHA") == a);
    t_note("class case-insensitive match: %d", U(FindWindowA)("mu_testcls", "MU_find_alpha") == a);
    CHECK(U(FindWindowA)(MU_CLS, "MU_find_alph") == NULL);       /* no prefix match */
    CHECK(U(FindWindowA)(MU_CLS, "MU_find_alpha ") == NULL);
    mu_kill(a); mu_kill(b);
    CHECK(U(FindWindowA)(MU_CLS, "MU_find_alpha") == NULL);
}
TEST(misc_ui_findwindow_wide_matches_ansi_window) {
    NEED_REL(); REQUIRE(U(FindWindowExW)); REQUIRE(U(FindWindowW));
    HWND a = mu_mk_cls(MU_CLS, "MU_find_wide", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    if (!a) { CHECK(0); return; }
    CHECK(U(FindWindowW)(L"MU_TestCls", L"MU_find_wide") == a);
    CHECK(U(FindWindowExW)(NULL, NULL, L"MU_TestCls", L"MU_find_wide") == a);
    CHECK(U(FindWindowW)(NULL, L"MU_find_wide") == a);
    mu_kill(a);
}
TEST(misc_ui_findwindow_unicode_title) {
    NEED_REL(); REQUIRE(U(FindWindowExW)); REQUIRE(U(CreateWindowExW)); REQUIRE(U(RegisterClassExW));
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = L"MU_UniCls\x00e9\x4e2d";
    ATOM at = U(RegisterClassExW)(&wc);
    CHECK(at != 0);
    HWND h = U(CreateWindowExW)(0, wc.lpszClassName, L"T\x00fc\x4e2d\x6587 title", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    CHECK(h != NULL);
    if (h) {
        CHECK(U(FindWindowExW)(NULL, NULL, wc.lpszClassName, NULL) == h);
        CHECK(U(FindWindowExW)(NULL, NULL, NULL, L"T\x00fc\x4e2d\x6587 title") == h);
        CHECK(U(FindWindowExW)(NULL, NULL, wc.lpszClassName, L"T\x00fc\x4e2d\x6587 titlf") == NULL);
        WCHAR cls[64] = L"";
        CHECK(U(GetClassNameW)(h, cls, 64) == lstrlenW(wc.lpszClassName));
        CHECK(!lstrcmpW(cls, wc.lpszClassName));
        U(DestroyWindow)(h);
    }
    U(UnregisterClassW)(wc.lpszClassName, wc.hInstance);
}
TEST(misc_ui_findwindow_by_class_atom) {
    NEED_REL(); REQUIRE(U(FindWindowExA)); REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    ATOM at = reg_unique("MU_AtomCls_1");
    CHECK(at != 0);
    HWND h = U(CreateWindowExA)(0, "MU_AtomCls_1", "atomwin", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    if (h) {
        CHECK(U(FindWindowExA)(NULL, NULL, MAKEINTATOM(at), NULL) == h);
        CHECK(U(FindWindowExA)(NULL, NULL, MAKEINTATOM(at), "atomwin") == h);
        CHECK(U(FindWindowExA)(NULL, NULL, MAKEINTATOM(at), "nope") == NULL);
        CHECK(U(FindWindowA)(MAKEINTATOM(at), NULL) == h);
        CHECK(U(FindWindowExW)(NULL, NULL, (LPCWSTR)MAKEINTATOM(at), NULL) == h);
        CHECK(U(FindWindowExA)(NULL, NULL, MAKEINTATOM(at ^ 0x1), NULL) != h);
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassA)("MU_AtomCls_1", GetModuleHandleA(NULL)));
}
TEST(misc_ui_findwindow_null_and_empty_strings) {
    NEED_REL(); REQUIRE(U(FindWindowExA));
    HWND a = mu_mk_cls(MU_CLS, "", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    HWND b = mu_mk_cls(MU_CLS, "MU_nonempty", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    if (!a || !b) { CHECK(0); mu_kill(a); mu_kill(b); return; }
    CHECK(U(FindWindowExA)(NULL, NULL, NULL, NULL) != NULL);
    HWND first = U(FindWindowExA)(NULL, NULL, MU_CLS, NULL);
    CHECK(first == b);                                  /* newest, topmost */
    CHECK(U(FindWindowExA)(NULL, b, MU_CLS, NULL) == a);
    CHECK(U(FindWindowExA)(NULL, a, MU_CLS, NULL) == NULL);
    HWND e = U(FindWindowExA)(NULL, NULL, MU_CLS, "");
    t_note("FindWindowEx(title=\"\") found empty-titled window: %d (found %p, a=%p, b=%p)", e == a, e, a, b);

    mu_kill(a); mu_kill(b);
}
TEST(misc_ui_findwindow_iterate_all_same_class) {
    NEED_REL(); REQUIRE(U(FindWindowExA));
    enum { N = 6 };
    HWND h[N];
    for (int i = 0; i < N; i++) h[i] = mu_mk_cls(MU_CLS, "MU_iter", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    HWND w = NULL; int n = 0;
    while ((w = U(FindWindowExA)(NULL, w, MU_CLS, "MU_iter")) != NULL && n < 100) {
        CHECKF(w == h[N - 1 - n], "iter %d", n);
        n++;
    }
    CHECK(n == N);
    for (int i = 0; i < N; i++) mu_kill(h[i]);
}
TEST(misc_ui_findwindow_hidden_windows_found) {
    NEED_REL(); REQUIRE(U(FindWindowExA));
    HWND h = mu_mk_cls(MU_CLS, "MU_hidden_find", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    CHECK(h && !U(IsWindowVisible)(h));
    CHECK(U(FindWindowA)(MU_CLS, "MU_hidden_find") == h);
    U(EnableWindow)(h, FALSE);
    CHECK(U(FindWindowA)(MU_CLS, "MU_hidden_find") == h);
    mu_kill(h);
}
TEST(misc_ui_findwindow_children_only_direct) {
    NEED_REL(); REQUIRE(U(FindWindowExA));
    HWND p = mu_mk_cls(MU_CLS, "MU_fparent", WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c1 = mu_mk_cls(MU_CLS, "MU_fchild", WS_CHILD, 0, 0, 0, 20, 20, p);
    HWND c2 = mu_mk_cls(MU_CLS, "MU_fchild", WS_CHILD, 0, 0, 0, 20, 20, p);
    HWND g = c1 ? mu_mk_cls(MU_CLS, "MU_fgrand", WS_CHILD, 0, 0, 0, 5, 5, c1) : NULL;
    if (!p || !c1 || !c2 || !g) { CHECK(0); mu_kill(p); return; }
    CHECK(U(FindWindowExA)(p, NULL, MU_CLS, "MU_fchild") == c1);
    CHECK(U(FindWindowExA)(p, c1, MU_CLS, "MU_fchild") == c2);
    CHECK(U(FindWindowExA)(p, c2, MU_CLS, "MU_fchild") == NULL);
    CHECK(U(FindWindowExA)(p, NULL, MU_CLS, "MU_fgrand") == NULL);       /* not recursive */
    CHECK(U(FindWindowExA)(c1, NULL, MU_CLS, "MU_fgrand") == g);
    CHECK(U(FindWindowExA)(NULL, NULL, MU_CLS, "MU_fchild") == NULL);     /* children are not top-level */
    CHECK(U(FindWindowA)(MU_CLS, "MU_fchild") == NULL);
    CHECK(U(FindWindowExA)(p, NULL, NULL, NULL) == c1);
    mu_kill(p);
}
TEST(misc_ui_findwindow_childafter_not_a_child) {
    NEED_REL(); REQUIRE(U(FindWindowExA));
    HWND p = mu_mk_cls(MU_CLS, "MU_fp2", WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c = mu_mk_cls(MU_CLS, "MU_fc2", WS_CHILD, 0, 0, 0, 20, 20, p);
    HWND other = mu_mk_cls(MU_CLS, "MU_fo2", WS_POPUP, 0, 0, 0, 20, 20, NULL);
    if (!p || !c || !other) { CHECK(0); mu_kill(p); mu_kill(other); return; }
    SetLastError(0);
    HWND r = U(FindWindowExA)(p, other, MU_CLS, NULL);
    t_note("FindWindowEx(parent, non-child after) = %p err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(FindWindowExA)(p, BOGUS_HWND, MU_CLS, NULL);
    t_note("FindWindowEx(parent, bogus after) = %p err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(FindWindowExA)(BOGUS_HWND, NULL, MU_CLS, NULL);
    CHECK(r == NULL);
    t_note("FindWindowEx(bogus parent) err=%lu", GetLastError());
    mu_kill(p); mu_kill(other);
}
TEST(misc_ui_findwindow_message_only) {
    NEED_REL(); REQUIRE(U(FindWindowExA));
    HWND m = mu_mk_cls(MU_CLS, "MU_msgonly", 0, 0, 0, 0, 0, 0, HWND_MESSAGE);
    CHECK(m != NULL);
    if (!m) return;
    CHECK(U(FindWindowExA)(HWND_MESSAGE, NULL, MU_CLS, "MU_msgonly") == m);
    CHECK(U(FindWindowExA)(HWND_MESSAGE, NULL, NULL, "MU_msgonly") == m);
    CHECK(U(FindWindowExA)(NULL, NULL, MU_CLS, "MU_msgonly") == NULL);
    CHECK(U(FindWindowA)(MU_CLS, "MU_msgonly") == NULL);
    HWND mp = U(GetAncestor)(m, GA_PARENT);
    CHECK(U(FindWindowExA)(mp, NULL, MU_CLS, "MU_msgonly") == m);
    HWND top = mu_mk_cls(MU_CLS, "MU_msgonly", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    CHECK(U(FindWindowExA)(HWND_MESSAGE, NULL, MU_CLS, "MU_msgonly") == m);
    CHECK(U(FindWindowA)(MU_CLS, "MU_msgonly") == top);
    mu_kill(top); mu_kill(m);
}
TEST(misc_ui_findwindow_long_class_name) {
    NEED_REL(); REQUIRE(U(FindWindowExA));
    char buf[1024];
    memset(buf, 'a', sizeof buf); buf[300] = 0;
    CHECK(U(FindWindowExA)(NULL, NULL, buf, NULL) == NULL);
    memset(buf, 'a', 256); buf[256] = 0;
    CHECK(U(FindWindowExA)(NULL, NULL, buf, NULL) == NULL);
    memset(buf, 'b', 1000); buf[1000] = 0;
    CHECK(U(FindWindowExA)(NULL, NULL, MU_CLS, buf) == NULL);
}
TEST(misc_ui_findwindow_system_class) {
    NEED_REL(); REQUIRE(U(FindWindowExA));
    HWND d = U(FindWindowExA)(NULL, NULL, "#32769", NULL);
    t_note("FindWindow(\"#32769\") = %p desktop=%p", d, U(GetDesktopWindow)());
    HWND tray = U(FindWindowExA)(NULL, NULL, "Shell_TrayWnd", NULL);
    t_note("Shell_TrayWnd=%p", tray);
    if (tray) { char c[64] = ""; U(GetClassNameA)(tray, c, 64); CHECK(!strcmp(c, "Shell_TrayWnd")); }
}
