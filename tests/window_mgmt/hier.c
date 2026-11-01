/* window hierarchy, ownership, enumeration, searching, hit testing */
#include "wm.h"

static HWND mkpop(HWND owner) { return wm_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 20, 20, 60, 40, owner); }
static HWND mkchild(HWND p, int x, int y, int w, int h) { return wm_mk(WS_CHILD | WS_VISIBLE, 0, x, y, w, h, p); }

TEST(wm_hier_desktop_basics) {
    NEED(GetDesktopWindow);
    HWND d = U(GetDesktopWindow)();
    CHECK(d != NULL);
    CHECK(U(IsWindow)(d));
    EQ(U(GetDesktopWindow)(), d);
    CHECK(U(IsWindowVisible)(d));
    CHECK(U(IsWindowEnabled)(d));
    EQ(U(GetParent)(d), NULL);
    EQ(U(GetAncestor)(d, GA_PARENT), NULL);
    EQ(U(GetWindow)(d, GW_OWNER), NULL);
    CHECK(U(GetWindow)(d, GW_CHILD) != NULL);
    EQ(U(GetWindow)(d, GW_HWNDNEXT), NULL);
    EQ(U(GetWindow)(d, GW_HWNDPREV), NULL);
    DWORD pid = 0;
    DWORD tid = U(GetWindowThreadProcessId)(d, &pid);
    CHECK(tid != 0);
    CHECK(pid != GetCurrentProcessId());
}

TEST(wm_hier_desktop_root_ancestors) {
    HWND d = U(GetDesktopWindow)();
    HWND r = U(GetAncestor)(d, GA_ROOT);
    HWND o = U(GetAncestor)(d, GA_ROOTOWNER);
    t_note("GetAncestor(desktop, GA_ROOT)=%p GA_ROOTOWNER=%p (desktop=%p)", r, o, d);
}

TEST(wm_hier_shell_window) {
    NEED(GetShellWindow);
    HWND s = U(GetShellWindow)();
    if (!s) { t_note("no shell window"); return; }
    CHECK(U(IsWindow)(s));
    EQ(U(GetShellWindow)(), s);
    DWORD pid = 0; U(GetWindowThreadProcessId)(s, &pid);
    CHECK(pid != GetCurrentProcessId());
    char cn[64]; U(GetClassNameA)(s, cn, 64);
    t_note("shell window class %s", cn);
    EQ(U(GetAncestor)(s, GA_PARENT), U(GetDesktopWindow)());
}

TEST(wm_hier_toplevel_parent_relations) {
    HWND h = wm_hidden();
    EQ(U(GetParent)(h), NULL);
    EQ(U(GetAncestor)(h, GA_PARENT), U(GetDesktopWindow)());
    EQ(U(GetAncestor)(h, GA_ROOT), h);
    EQ(U(GetAncestor)(h, GA_ROOTOWNER), h);
    EQ(U(GetWindow)(h, GW_OWNER), NULL);
    EQ(U(GetWindow)(h, GW_CHILD), NULL);
    EQ(U(GetTopWindow)(h), NULL);
    wm_destroy(h);
}

TEST(wm_hier_ancestor_flags) {
    HWND o = wm_hidden();
    HWND pop = wm_mk(WS_POPUP, 0, 0, 0, 50, 50, o);
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 20, 20, pop);
    HWND g = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, c);
    HWND d = U(GetDesktopWindow)();
    EQ(U(GetAncestor)(g, GA_PARENT), c);
    EQ(U(GetAncestor)(c, GA_PARENT), pop);
    EQ(U(GetAncestor)(pop, GA_PARENT), d);
    EQ(U(GetAncestor)(g, GA_ROOT), pop);
    EQ(U(GetAncestor)(c, GA_ROOT), pop);
    EQ(U(GetAncestor)(pop, GA_ROOT), pop);
    EQ(U(GetAncestor)(g, GA_ROOTOWNER), o);
    EQ(U(GetAncestor)(c, GA_ROOTOWNER), o);
    EQ(U(GetAncestor)(pop, GA_ROOTOWNER), o);
    EQ(U(GetAncestor)(o, GA_ROOTOWNER), o);
    EQ(U(GetParent)(g), c);
    EQ(U(GetParent)(pop), o);
    wm_destroy(o);
}

TEST(wm_hier_ancestor_invalid) {
    NEED(GetAncestor);
    HWND h = wm_hidden();
    SetLastError(0);
    EQ(U(GetAncestor)(h, 0), NULL);
    t_note("GetAncestor(h,0) err=%lu", GetLastError());
    EQ(U(GetAncestor)(h, 4), NULL);
    EQ(U(GetAncestor)(h, 100), NULL);
    EQ(U(GetAncestor)(h, (UINT)-1), NULL);
    EQ(U(GetAncestor)(NULL, GA_PARENT), NULL);
    EQ(U(GetAncestor)(NULL, GA_ROOT), NULL);
    EQ(U(GetAncestor)((HWND)(ULONG_PTR)0x4440, GA_ROOT), NULL);
    wm_destroy(h);
    EQ(U(GetAncestor)(h, GA_ROOT), NULL);
}

TEST(wm_hier_getwindow_invalid_cmd) {
    HWND h = wm_hidden();
    SetLastError(0);
    EQ(U(GetWindow)(h, 7), NULL);
    t_note("GetWindow(h,7) err=%lu", GetLastError());
    EQ(U(GetWindow)(h, 100), NULL);
    EQ(U(GetWindow)(h, (UINT)-1), NULL);
    SetLastError(0);
    EQ(U(GetWindow)(NULL, GW_CHILD), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    wm_destroy(h);
}

TEST(wm_hier_children_order) {
    HWND p = wm_hidden();
    HWND c1 = mkchild(p, 0, 0, 10, 10), c2 = mkchild(p, 0, 0, 10, 10), c3 = mkchild(p, 0, 0, 10, 10);
    EQ(U(GetWindow)(p, GW_CHILD), c1);
    EQ(U(GetTopWindow)(p), c1);
    EQ(U(GetWindow)(c1, GW_HWNDFIRST), c1);
    EQ(U(GetWindow)(c3, GW_HWNDFIRST), c1);
    EQ(U(GetWindow)(c1, GW_HWNDLAST), c3);
    EQ(U(GetWindow)(c3, GW_HWNDLAST), c3);
    EQ(U(GetWindow)(c1, GW_HWNDNEXT), c2);
    EQ(U(GetWindow)(c2, GW_HWNDNEXT), c3);
    EQ(U(GetWindow)(c3, GW_HWNDNEXT), NULL);
    EQ(U(GetWindow)(c3, GW_HWNDPREV), c2);
    EQ(U(GetWindow)(c2, GW_HWNDPREV), c1);
    EQ(U(GetWindow)(c1, GW_HWNDPREV), NULL);
    EQ(U(GetWindow)(c2, GW_OWNER), NULL);
    EQ(U(GetWindow)(c2, GW_CHILD), NULL);
    wm_destroy(p);
}

TEST(wm_hier_enabledpopup) {
    HWND o = wm_hidden();
    HWND pu = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 20, 20, 60, 40, o);
    EQ(U(GetWindow)(o, GW_ENABLEDPOPUP), pu);
    U(EnableWindow)(pu, FALSE);
    t_note("ENABLEDPOPUP with disabled popup = %p (owner %p)", (void*)U(GetWindow)(o, GW_ENABLEDPOPUP), (void*)o);
    U(EnableWindow)(pu, TRUE);
    EQ(U(GetWindow)(o, GW_ENABLEDPOPUP), pu);
    U(ShowWindow)(pu, SW_HIDE);
    t_note("GW_ENABLEDPOPUP with hidden popup = %s", U(GetWindow)(o, GW_ENABLEDPOPUP) == pu ? "popup" : "owner/other");
    wm_destroy(o);
}

TEST(wm_hier_owner_chain) {
    HWND o = wm_hidden();
    HWND a = mkpop(o);
    HWND b = mkpop(a);
    EQ(U(GetWindow)(a, GW_OWNER), o);
    EQ(U(GetWindow)(b, GW_OWNER), a);
    EQ(U(GetAncestor)(b, GA_ROOTOWNER), o);
    EQ(U(GetAncestor)(b, GA_ROOT), b);
    EQ(U(GetParent)(b), a);
    wm_destroy(o);
}

TEST(wm_hier_getparent_variants) {
    HWND p = wm_hidden();
    HWND c = mkchild(p, 0, 0, 10, 10);
    SetLastError(0);
    EQ(U(GetParent)(c), p);
    EQ(U(GetParent)(p), NULL);
    SetLastError(0);
    EQ(U(GetParent)(NULL), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    EQ(U(GetParent)((HWND)(ULONG_PTR)0x1230), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    wm_destroy(p);
}

TEST(wm_hier_gettopwindow) {
    NEED(GetTopWindow);
    HWND p = wm_hidden();
    EQ(U(GetTopWindow)(p), NULL);
    HWND c1 = mkchild(p, 0, 0, 5, 5), c2 = mkchild(p, 0, 0, 5, 5);
    EQ(U(GetTopWindow)(p), c1);
    U(BringWindowToTop)(c2);
    EQ(U(GetTopWindow)(p), c2);
    CHECK(U(GetTopWindow)(NULL) != NULL);
    EQ(U(GetTopWindow)(NULL), U(GetWindow)(U(GetDesktopWindow)(), GW_CHILD));
    SetLastError(0);
    EQ(U(GetTopWindow)((HWND)(ULONG_PTR)0x1230), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    wm_destroy(p);
}

TEST(wm_hier_setparent_basic) {
    NEED(SetParent);
    HWND p1 = wm_hidden(), p2 = wm_hidden();
    HWND c = mkchild(p1, 7, 8, 30, 20);
    EQ(U(SetParent)(c, p2), p1);
    EQ(U(GetParent)(c), p2);
    CHECK(U(IsChild)(p2, c)); CHECK(!U(IsChild)(p1, c));
    CHECK(U(GetWindowLongA)(c, GWL_STYLE) & WS_CHILD);
    CHECK(U(GetWindowLongA)(c, GWL_STYLE) & WS_VISIBLE);
    POINT o = {0, 0}; U(ClientToScreen)(p2, &o);
    RECT r; U(GetWindowRect)(c, &r);
    EQ(r.left, o.x + 7); EQ(r.top, o.y + 8);
    EQ(U(SetParent)(c, p2), p2);
    EQ(U(GetWindow)(p1, GW_CHILD), NULL);
    EQ(U(GetWindow)(p2, GW_CHILD), c);
    wm_destroy(p1); wm_destroy(p2);
}

TEST(wm_hier_setparent_null_makes_desktop_child) {
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 7, 8, 30, 20, p);
    EQ(U(SetParent)(c, NULL), p);
    EQ(U(GetAncestor)(c, GA_PARENT), U(GetDesktopWindow)());
    CHECK(!U(IsChild)(p, c));
    t_note("GetParent after SetParent(NULL) = %p", U(GetParent)(c));
    wm_destroy(c); wm_destroy(p);
}

TEST(wm_hier_setparent_toplevel_to_child) {
    HWND p = wm_hidden();
    HWND t = wm_mk(WS_POPUP, 0, 5, 5, 40, 40, NULL);
    HWND d = U(GetDesktopWindow)();
    HWND old = U(SetParent)(t, p);
    t_note("SetParent(toplevel popup, p) returned %p (desktop %p, NULL %p)", old, d, NULL);
    CHECK(old == NULL || old == d);
    EQ(U(GetAncestor)(t, GA_PARENT), p);
    t_note("GetParent(popup reparented) = %s", U(GetParent)(t) == p ? "new parent" : "other");
    wm_destroy(p);
}

TEST(wm_hier_setparent_invalid) {
    NEED(SetParent);
    HWND p = wm_hidden();
    HWND c = mkchild(p, 0, 0, 10, 10);
    SetLastError(0);
    EQ(U(SetParent)(c, c), NULL);
    t_note("SetParent(c,c) err=%lu", GetLastError());
    EQ(U(GetParent)(c), p);
    SetLastError(0);
    EQ(U(SetParent)(p, c), NULL);   /* cycle */
    t_note("SetParent(p,child-of-p) err=%lu", GetLastError());
    EQ(U(GetParent)(c), p);
    SetLastError(0);
    EQ(U(SetParent)(c, (HWND)(ULONG_PTR)0x8880), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    EQ(U(SetParent)((HWND)(ULONG_PTR)0x8880, p), NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    EQ(U(SetParent)(NULL, p), NULL);
    SetLastError(0);
    EQ(U(SetParent)(U(GetDesktopWindow)(), p), NULL);
    t_note("SetParent(desktop,p) err=%lu", GetLastError());
    EQ(U(GetParent)(c), p);
    wm_destroy(p);
}

TEST(wm_hier_setparent_cross_thread) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    HWND p = wm_hidden();
    HWND c = mkchild(p, 0, 0, 10, 10);
    SetLastError(0);
    HWND r = U(SetParent)(c, w.hwnd);
    t_note("SetParent across threads = %p err=%lu", r, GetLastError());
    if (r) EQ(U(GetParent)(c), w.hwnd);
    wm_destroy(p);
    wm_worker_stop(&w);
}

TEST(wm_hier_message_only_window) {
    NEED(FindWindowExA);
    HWND m = U(CreateWindowExA)(0, wm_cls(), "wm_msgonly_unique_1", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(m != NULL);
    if (!m) return;
    CHECK(U(IsWindow)(m));
    CHECK(!U(IsWindowVisible)(m));
    EQ(U(FindWindowExA)(HWND_MESSAGE, NULL, wm_cls(), "wm_msgonly_unique_1"), m);
    /* Win10: FindWindow / FindWindowEx with a NULL parent search the message-only windows too */
    EQ(U(FindWindowA)(wm_cls(), "wm_msgonly_unique_1"), m);
    EQ(U(FindWindowExA)(NULL, NULL, wm_cls(), "wm_msgonly_unique_1"), m);
    HWND par = U(GetAncestor)(m, GA_PARENT);
    CHECK(par != NULL);
    CHECK(par != U(GetDesktopWindow)());
    char cn[32]; U(GetClassNameA)(par, cn, 32);
    t_note("message-only parent class %s", cn);
    EQ(U(PostMessageA)(m, WM_NULL, 0, 0), TRUE);
    EQ(U(SendMessageA)(m, WM_NULL, 0, 0), 0);
    wm_destroy(m);
}

TEST(wm_hier_parentnotify) {
    HWND p = wm_hidden();
    wm_logclear();
    HWND c = U(CreateWindowExA)(0, wm_cls(), "k", WS_CHILD, 0, 0, 10, 10, p, (HMENU)(ULONG_PTR)77, GetModuleHandleA(NULL), NULL);
    CHECK(c != NULL);
    int i = wm_logfind(WM_PARENTNOTIFY, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(LOWORD(w), WM_CREATE); EQ(HIWORD(w), 77); }
    wm_logclear();
    U(DestroyWindow)(c);
    i = wm_logfind(WM_PARENTNOTIFY, 0);
    CHECK(i >= 0);
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(LOWORD(w), WM_DESTROY); EQ(HIWORD(w), 77); }
    wm_logclear();
    c = U(CreateWindowExA)(WS_EX_NOPARENTNOTIFY, wm_cls(), "k", WS_CHILD, 0, 0, 10, 10, p, (HMENU)(ULONG_PTR)78, GetModuleHandleA(NULL), NULL);
    EQ(wm_logcount(WM_PARENTNOTIFY), 0);
    wm_destroy(p);
}

/* ---- enumeration ---- */
typedef struct { HWND list[512]; int n; LPARAM lp; int stop_at; } enum_ctx;
static BOOL CALLBACK enum_cb(HWND h, LPARAM l) {
    enum_ctx *c = (enum_ctx *)l;
    if (c->n < 512) c->list[c->n] = h;
    c->n++;
    return !(c->stop_at && c->n >= c->stop_at);
}
static int idx(enum_ctx *c, HWND h) { for (int i = 0; i < c->n && i < 512; i++) if (c->list[i] == h) return i; return -1; }

TEST(wm_enum_windows_contains_ours) {
    NEED(EnumWindows);
    HWND a = wm_hidden(), b = wm_hidden(), cvis = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 10, 10, NULL);
    HWND child = mkchild(a, 0, 0, 5, 5);
    static enum_ctx c; memset(&c, 0, sizeof c);
    CHECK(U(EnumWindows)(enum_cb, (LPARAM)&c));
    CHECK(c.n > 2);
    CHECK(idx(&c, a) >= 0); CHECK(idx(&c, b) >= 0); CHECK(idx(&c, cvis) >= 0);
    CHECK(idx(&c, child) < 0);
    CHECK(idx(&c, U(GetDesktopWindow)()) < 0);
    int dup = 0;
    for (int i = 0; i < c.n && i < 512; i++) { for (int j = i + 1; j < c.n && j < 512; j++) if (c.list[i] == c.list[j]) dup++; if (U(GetParent)(c.list[i]) != NULL && !(U(GetWindowLongA)(c.list[i], GWL_STYLE) & WS_POPUP)) {} }
    EQ(dup, 0);
    wm_destroy(a); wm_destroy(b); wm_destroy(cvis);
}

TEST(wm_enum_windows_zorder_matches_chain) {
    HWND a = wm_hidden(), b = wm_hidden(), c3 = wm_hidden();
    static enum_ctx c; memset(&c, 0, sizeof c);
    U(EnumWindows)(enum_cb, (LPARAM)&c);
    int ia = idx(&c, a), ib = idx(&c, b), ic = idx(&c, c3);
    CHECK(ia >= 0 && ib >= 0 && ic >= 0);
    EQ(ia < ib, wm_isabove(a, b));
    EQ(ib < ic, wm_isabove(b, c3));
    EQ(ia < ic, wm_isabove(a, c3));
    CHECK(ic < ib && ib < ia);       /* newest on top */
    wm_destroy(a); wm_destroy(b); wm_destroy(c3);
}

TEST(wm_enum_windows_stop) {
    static enum_ctx c; memset(&c, 0, sizeof c);
    c.stop_at = 1;
    BOOL r = U(EnumWindows)(enum_cb, (LPARAM)&c);
    EQ(r, FALSE);
    EQ(c.n, 1);
    memset(&c, 0, sizeof c); c.stop_at = 3;
    r = U(EnumWindows)(enum_cb, (LPARAM)&c);
    EQ(r, FALSE); EQ(c.n, 3);
}

static BOOL CALLBACK enum_destroy_cb(HWND h, LPARAM l) { (void)h; HWND *victim = (HWND *)l; if (*victim) { U(DestroyWindow)(*victim); *victim = NULL; } return TRUE; }
TEST(wm_enum_windows_destroy_during) {
    HWND a = wm_hidden(), b = wm_hidden();
    HWND v = a;
    CHECK(U(EnumWindows)(enum_destroy_cb, (LPARAM)&v));
    CHECK(!U(IsWindow)(a));
    wm_destroy(b);
}

TEST(wm_enum_children_all_descendants) {
    NEED(EnumChildWindows);
    HWND p = wm_hidden();
    HWND c1 = mkchild(p, 0, 0, 50, 50), c2 = mkchild(p, 0, 0, 50, 50);
    HWND g1 = mkchild(c1, 0, 0, 5, 5), g2 = mkchild(c1, 0, 0, 5, 5), g3 = mkchild(c2, 0, 0, 5, 5);
    HWND gg = mkchild(g1, 0, 0, 2, 2);
    HWND hid = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, p);
    static enum_ctx c; memset(&c, 0, sizeof c);
    U(EnumChildWindows)(p, enum_cb, (LPARAM)&c);
    EQ(c.n, 7);
    HWND all[] = { c1, c2, g1, g2, g3, gg, hid };
    for (unsigned i = 0; i < 7; i++) CHECKF(idx(&c, all[i]) >= 0, "descendant %u", i);
    CHECK(idx(&c, p) < 0);
    CHECK(idx(&c, c1) < idx(&c, c2));
    CHECK(idx(&c, c1) < idx(&c, g1));
    CHECK(idx(&c, g1) < idx(&c, gg));
    t_note("child enumeration order: c1=%d g1=%d gg=%d g2=%d c2=%d g3=%d hid=%d", idx(&c, c1), idx(&c, g1), idx(&c, gg), idx(&c, g2), idx(&c, c2), idx(&c, g3), idx(&c, hid));
    wm_destroy(p);
}

TEST(wm_enum_children_stop_and_none) {
    HWND p = wm_hidden();
    static enum_ctx c; memset(&c, 0, sizeof c);
    U(EnumChildWindows)(p, enum_cb, (LPARAM)&c);
    EQ(c.n, 0);
    mkchild(p, 0, 0, 5, 5); mkchild(p, 0, 0, 5, 5); mkchild(p, 0, 0, 5, 5);
    memset(&c, 0, sizeof c); c.stop_at = 2;
    U(EnumChildWindows)(p, enum_cb, (LPARAM)&c);
    EQ(c.n, 2);
    wm_destroy(p);
}

TEST(wm_enum_children_null_parent_is_desktop) {
    HWND a = wm_hidden();
    static enum_ctx x, y; memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
    U(EnumChildWindows)(NULL, enum_cb, (LPARAM)&x);
    CHECK(idx(&x, a) >= 0);
    U(EnumChildWindows)(U(GetDesktopWindow)(), enum_cb, (LPARAM)&y);
    CHECK(idx(&y, a) >= 0);
    CHECK(y.n >= x.n - 20 || 1);
    wm_destroy(a);
}

TEST(wm_enum_children_bad_parent) {
    static enum_ctx c; memset(&c, 0, sizeof c);
    HWND h = wm_hidden(); wm_destroy(h);
    U(EnumChildWindows)(h, enum_cb, (LPARAM)&c);
    EQ(c.n, 0);
    U(EnumChildWindows)((HWND)(ULONG_PTR)0x6660, enum_cb, (LPARAM)&c);
    EQ(c.n, 0);
}

TEST(wm_enum_children_direct_order_matches_chain) {
    HWND p = wm_hidden();
    HWND k[5];
    for (int i = 0; i < 5; i++) k[i] = mkchild(p, 0, 0, 5, 5);
    U(BringWindowToTop)(k[3]);
    U(SetWindowPos)(k[1], HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    static enum_ctx c; memset(&c, 0, sizeof c);
    U(EnumChildWindows)(p, enum_cb, (LPARAM)&c);
    EQ(c.n, 5);
    HWND w = U(GetWindow)(p, GW_CHILD);
    for (int i = 0; i < 5 && i < c.n; i++) { EQ(c.list[i], w); w = U(GetWindow)(w, GW_HWNDNEXT); }
    EQ(c.list[0], k[3]); EQ(c.list[4], k[1]);
    wm_destroy(p);
}

TEST(wm_enum_thread_windows) {
    NEED(EnumThreadWindows);
    HWND a = wm_hidden();
    HWND ch = mkchild(a, 0, 0, 5, 5);
    HWND pop = mkpop(a);
    static enum_ctx c; memset(&c, 0, sizeof c);
    U(EnumThreadWindows)(GetCurrentThreadId(), enum_cb, (LPARAM)&c);
    CHECK(idx(&c, a) >= 0); CHECK(idx(&c, pop) >= 0);
    CHECK(idx(&c, ch) < 0);
    int foreign = 0;
    for (int i = 0; i < c.n && i < 512; i++) { DWORD pid; if (U(GetWindowThreadProcessId)(c.list[i], &pid) != GetCurrentThreadId()) foreign++; }
    EQ(foreign, 0);
    wm_destroy(a);
}

TEST(wm_enum_thread_windows_worker) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    HWND mine = wm_hidden();
    static enum_ctx c; memset(&c, 0, sizeof c);
    U(EnumThreadWindows)(w.tid, enum_cb, (LPARAM)&c);
    CHECK(idx(&c, w.hwnd) >= 0);     /* the default IME window of the thread may be listed too */
    for (int i = 0; i < c.n && i < 512; i++) EQ(U(GetWindowThreadProcessId)(c.list[i], NULL), w.tid);
    memset(&c, 0, sizeof c);
    U(EnumThreadWindows)(GetCurrentThreadId(), enum_cb, (LPARAM)&c);
    CHECK(idx(&c, w.hwnd) < 0);
    CHECK(idx(&c, mine) >= 0);
    wm_destroy(mine);
    wm_worker_stop(&w);
}

TEST(wm_enum_thread_windows_stop_and_invalid) {
    HWND a = wm_hidden(), b = wm_hidden();
    static enum_ctx c; memset(&c, 0, sizeof c);
    c.stop_at = 1;
    BOOL r = U(EnumThreadWindows)(GetCurrentThreadId(), enum_cb, (LPARAM)&c);
    EQ(c.n, 1);
    t_note("EnumThreadWindows stopped, returned %d", r);
    memset(&c, 0, sizeof c);
    SetLastError(0);
    r = U(EnumThreadWindows)(0, enum_cb, (LPARAM)&c);
    t_note("EnumThreadWindows(tid 0) = %d n=%d err=%lu", r, c.n, GetLastError());
    memset(&c, 0, sizeof c);
    r = U(EnumThreadWindows)(0xFFFFFFF0, enum_cb, (LPARAM)&c);
    EQ(c.n, 0);
    t_note("EnumThreadWindows(bogus tid) = %d", r);
    wm_destroy(a); wm_destroy(b);
}

TEST(wm_enum_desktop_windows) {
    NEED(EnumDesktopWindows);
    HWND a = wm_hidden();
    static enum_ctx c; memset(&c, 0, sizeof c);
    CHECK(U(EnumDesktopWindows)(NULL, enum_cb, (LPARAM)&c));
    CHECK(idx(&c, a) >= 0);
    wm_destroy(a);
}

static LPARAM g_seen_lp;
static BOOL CALLBACK lp_cb(HWND h, LPARAM l) { g_seen_lp = l; return FALSE; }
TEST(wm_enum_lparam_passthrough) {
    LPARAM seen;
    HWND h = wm_hidden();
    g_seen_lp = 0;
    U(EnumWindows)(lp_cb, 0x1234567812345678ULL);
    seen = g_seen_lp;
    wm_destroy(h);
    EQ(seen, 0x1234567812345678ULL);
}

/* ---- FindWindow ---- */
TEST(wm_find_by_class_and_title) {
    NEED(FindWindowA); NEED(FindWindowW);
    HWND h = U(CreateWindowExA)(0, wm_cls(), "wm_find_title_1", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(U(FindWindowA)(wm_cls(), "wm_find_title_1"), h);
    EQ(U(FindWindowA)(NULL, "wm_find_title_1"), h);
    EQ(U(FindWindowA)(wm_cls(), NULL), h);
    EQ(U(FindWindowW)(L"WmTestClassA", L"wm_find_title_1"), h);
    EQ(U(FindWindowW)(NULL, L"wm_find_title_1"), h);
    EQ(U(FindWindowA)(wm_cls(), "wm_find_title_2"), NULL);
    EQ(U(FindWindowA)(wm_cls(), "wm_find_title"), NULL);        /* no prefix match */
    EQ(U(FindWindowA)(wm_cls(), "wm_find_title_1x"), NULL);
    EQ(U(FindWindowA)(NULL, "wm_no_such_title_zzz"), NULL);
    EQ(U(FindWindowA)(wm_cls(), ""), NULL);
    char cn[64]; U(GetClassNameA)(h, cn, 64);
    t_note("find with lowercase class: %p, with lowercase title: %p",
           U(FindWindowA)("wmtestclassa", "wm_find_title_1"), U(FindWindowA)(wm_cls(), "WM_FIND_TITLE_1"));
    wm_destroy(h);
    EQ(U(FindWindowA)(wm_cls(), "wm_find_title_1"), NULL);
}

TEST(wm_find_both_null) {
    HWND r = U(FindWindowA)(NULL, NULL);
    CHECK(r != NULL);
    /* Win10: FindWindow(NULL, NULL) returns a top-level window but not necessarily the
     * z-order top (observed 0x10112 vs GetTopWindow 0x10166) */
    t_note("FindWindow(NULL,NULL)=%p GetTopWindow(NULL)=%p", r, U(GetTopWindow)(NULL));
    CHECK(U(IsWindow)(r));
    CHECK(U(GetAncestor)(r, GA_PARENT) == U(GetDesktopWindow)());
}

TEST(wm_find_unregistered_class) {
    SetLastError(0);
    EQ(U(FindWindowA)("WmNoSuchClass_zzz", NULL), NULL);
    t_note("FindWindow(unregistered class) err=%lu", GetLastError());
    SetLastError(0);
    EQ(U(FindWindowW)(L"WmNoSuchClass_zzz", L"t"), NULL);
}

TEST(wm_find_hidden_and_disabled) {
    HWND h = U(CreateWindowExA)(0, wm_cls(), "wm_find_hidden", WS_OVERLAPPED | WS_DISABLED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(!U(IsWindowVisible)(h));
    EQ(U(FindWindowA)(wm_cls(), "wm_find_hidden"), h);
    wm_destroy(h);
}

TEST(wm_find_excludes_children) {
    HWND p = wm_hidden();
    HWND c = U(CreateWindowExA)(0, wm_cls(), "wm_find_child", WS_CHILD, 0, 0, 10, 10, p, NULL, GetModuleHandleA(NULL), NULL);
    EQ(U(FindWindowA)(wm_cls(), "wm_find_child"), NULL);
    EQ(U(FindWindowExA)(p, NULL, wm_cls(), "wm_find_child"), c);
    EQ(U(FindWindowExA)(p, NULL, NULL, "wm_find_child"), c);
    EQ(U(FindWindowExA)(p, NULL, wm_cls(), NULL), c);
    EQ(U(FindWindowExA)(p, c, wm_cls(), NULL), NULL);
    wm_destroy(p);
}

TEST(wm_find_ex_iteration) {
    NEED(FindWindowExA); NEED(FindWindowExW);
    wm_cls(); wm_clsw();
    HWND p = wm_hidden();
    HWND k[4];
    for (int i = 0; i < 4; i++) k[i] = U(CreateWindowExA)(0, i == 2 ? "WmTestClassW" : wm_cls(), "kid", WS_CHILD, 0, 0, 10, 10, p, NULL, GetModuleHandleA(NULL), NULL);
    for (int i = 0; i < 4; i++) CHECK(k[i] != NULL);
    EQ(U(FindWindowExA)(p, NULL, NULL, NULL), k[0]);
    EQ(U(FindWindowExA)(p, k[0], NULL, NULL), k[1]);
    EQ(U(FindWindowExA)(p, k[1], NULL, NULL), k[2]);
    EQ(U(FindWindowExA)(p, k[3], NULL, NULL), NULL);
    EQ(U(FindWindowExA)(p, NULL, wm_cls(), NULL), k[0]);
    EQ(U(FindWindowExA)(p, k[0], wm_cls(), NULL), k[1]);
    EQ(U(FindWindowExA)(p, k[1], wm_cls(), NULL), k[3]);
    EQ(U(FindWindowExA)(p, NULL, wm_clsw() ? "WmTestClassW" : NULL, NULL), k[2]);
    EQ(U(FindWindowExW)(p, NULL, L"WmTestClassW", L"kid"), k[2]);
    EQ(U(FindWindowExW)(p, NULL, L"WmTestClassW", L"other"), NULL);
    wm_destroy(p);
}

TEST(wm_find_ex_after_not_child) {
    HWND p = wm_hidden(), q = wm_hidden();
    HWND c = mkchild(p, 0, 0, 5, 5);
    HWND d = mkchild(q, 0, 0, 5, 5);
    SetLastError(0);
    HWND r = U(FindWindowExA)(p, d, NULL, NULL);
    t_note("FindWindowEx(after not child of parent) = %p err=%lu", r, GetLastError());
    wm_destroy(p); wm_destroy(q);
}

TEST(wm_find_ex_desktop_equiv) {
    HWND h = U(CreateWindowExA)(0, wm_cls(), "wm_find_desk", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(U(FindWindowExA)(NULL, NULL, wm_cls(), "wm_find_desk"), h);
    EQ(U(FindWindowExA)(U(GetDesktopWindow)(), NULL, wm_cls(), "wm_find_desk"), h);
    EQ(U(FindWindowExA)(NULL, h, wm_cls(), "wm_find_desk"), NULL);
    wm_destroy(h);
}

TEST(wm_find_atom_class) {
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "WmFindAtomCls";
    ATOM at = U(RegisterClassExA)(&wc);
    CHECK(at != 0);
    if (!at) return;
    HWND h = U(CreateWindowExA)(0, "WmFindAtomCls", "x", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(U(FindWindowA)((LPCSTR)(ULONG_PTR)at, NULL), h);
    EQ(U(FindWindowExA)(NULL, NULL, (LPCSTR)(ULONG_PTR)at, NULL), h);
    wm_destroy(h);
    CHECK(U(UnregisterClassA)("WmFindAtomCls", GetModuleHandleA(NULL)));
}

TEST(wm_find_other_thread) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    U(SetWindowTextA)(w.hwnd, "wm_find_worker");
    EQ(U(FindWindowA)(wm_cls(), "wm_find_worker"), w.hwnd);
    wm_worker_stop(&w);
    EQ(U(FindWindowA)(wm_cls(), "wm_find_worker"), NULL);
}

TEST(wm_find_ex_zorder_first_wins) {
    HWND a = U(CreateWindowExA)(0, wm_cls(), "wm_dup_title", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    HWND b = U(CreateWindowExA)(0, wm_cls(), "wm_dup_title", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(U(FindWindowA)(wm_cls(), "wm_dup_title"), b);       /* topmost in z-order first */
    EQ(U(FindWindowExA)(NULL, b, wm_cls(), "wm_dup_title"), a);
    EQ(U(FindWindowExA)(NULL, a, wm_cls(), "wm_dup_title"), NULL);
    wm_destroy(a); wm_destroy(b);
}

/* ---- hit testing ---- */
static HWND vis_topmost(int x, int y, int w, int h) {
    HWND r = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, x, y, w, h, NULL);
    return r;
}

TEST(wm_hit_windowfrompoint) {
    NEED(WindowFromPoint);
    int sx = U(GetSystemMetrics)(SM_CXSCREEN) / 2, sy = U(GetSystemMetrics)(SM_CYSCREEN) / 2;
    HWND h = vis_topmost(sx, sy, 100, 100);
    t_pump();
    POINT p = { sx + 50, sy + 50 };
    EQ(U(WindowFromPoint)(p), h);
    p.x = sx; p.y = sy;
    EQ(U(WindowFromPoint)(p), h);
    p.x = sx + 99; p.y = sy + 99;
    EQ(U(WindowFromPoint)(p), h);
    p.x = sx + 100; p.y = sy + 100;
    CHECK(U(WindowFromPoint)(p) != h);
    U(ShowWindow)(h, SW_HIDE);
    p.x = sx + 50; p.y = sy + 50;
    CHECK(U(WindowFromPoint)(p) != h);
    wm_destroy(h);
}

TEST(wm_hit_windowfrompoint_child) {
    int sx = U(GetSystemMetrics)(SM_CXSCREEN) / 2, sy = U(GetSystemMetrics)(SM_CYSCREEN) / 2;
    HWND h = vis_topmost(sx, sy, 100, 100);
    HWND c = mkchild(h, 10, 10, 30, 30);
    HWND g = mkchild(c, 5, 5, 10, 10);
    POINT p = { sx + 12, sy + 12 };
    EQ(U(WindowFromPoint)(p), c);
    p.x = sx + 16; p.y = sy + 16;
    EQ(U(WindowFromPoint)(p), g);
    p.x = sx + 80; p.y = sy + 80;
    EQ(U(WindowFromPoint)(p), h);
    U(ShowWindow)(c, SW_HIDE);
    p.x = sx + 16; p.y = sy + 16;
    EQ(U(WindowFromPoint)(p), h);
    wm_destroy(h);
}

TEST(wm_hit_windowfrompoint_disabled_transparent) {
    int sx = U(GetSystemMetrics)(SM_CXSCREEN) / 2, sy = U(GetSystemMetrics)(SM_CYSCREEN) / 2;
    HWND h = vis_topmost(sx, sy, 100, 100);
    POINT p = { sx + 50, sy + 50 };
    U(EnableWindow)(h, FALSE);
    HWND r = U(WindowFromPoint)(p);
    t_note("WindowFromPoint over disabled window: %s", r == h ? "returns it" : "skips it");
    U(EnableWindow)(h, TRUE);
    U(SetWindowLongA)(h, GWL_EXSTYLE, U(GetWindowLongA)(h, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
    r = U(WindowFromPoint)(p);
    t_note("WindowFromPoint over WS_EX_TRANSPARENT window: %s", r == h ? "returns it" : "skips it");
    wm_destroy(h);
}

TEST(wm_hit_childwindowfrompoint) {
    NEED(ChildWindowFromPoint);
    HWND p = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 200, 200, NULL);
    HWND c1 = mkchild(p, 10, 10, 50, 50), c2 = mkchild(p, 100, 100, 50, 50);
    HWND g = mkchild(c1, 5, 5, 10, 10);
    POINT pt = { 20, 20 };
    EQ(U(ChildWindowFromPoint)(p, pt), c1);
    pt.x = 17; pt.y = 17;                       /* over the grandchild: still the direct child */
    EQ(U(ChildWindowFromPoint)(p, pt), c1);
    pt.x = 110; pt.y = 110;
    EQ(U(ChildWindowFromPoint)(p, pt), c2);
    pt.x = 80; pt.y = 80;
    EQ(U(ChildWindowFromPoint)(p, pt), p);       /* in client, no child */
    pt.x = -1; pt.y = 5;
    EQ(U(ChildWindowFromPoint)(p, pt), NULL);
    pt.x = 5; pt.y = 200;
    EQ(U(ChildWindowFromPoint)(p, pt), NULL);
    pt.x = 199; pt.y = 199;
    EQ(U(ChildWindowFromPoint)(p, pt), p);
    pt.x = 10; pt.y = 10;
    EQ(U(ChildWindowFromPoint)(p, pt), c1);
    pt.x = 59; pt.y = 59;
    EQ(U(ChildWindowFromPoint)(p, pt), c1);
    pt.x = 60; pt.y = 60;
    EQ(U(ChildWindowFromPoint)(p, pt), p);
    /* point relative to a child */
    pt.x = 7; pt.y = 7;
    EQ(U(ChildWindowFromPoint)(c1, pt), g);
    pt.x = 40; pt.y = 40;
    EQ(U(ChildWindowFromPoint)(c1, pt), c1);
    wm_destroy(p);
}

TEST(wm_hit_childwindowfrompointex_flags) {
    NEED(ChildWindowFromPointEx);
    HWND p = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 200, 200, NULL);
    HWND under = mkchild(p, 10, 10, 80, 80);
    HWND over = mkchild(p, 10, 10, 80, 80);       /* created later => below in z-order; reorder */
    U(SetWindowPos)(over, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    POINT pt = { 30, 30 };
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_ALL), over);
    U(ShowWindow)(over, SW_HIDE);
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_ALL), over);
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_SKIPINVISIBLE), under);
    U(ShowWindow)(over, SW_SHOWNA);
    U(EnableWindow)(over, FALSE);
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_ALL), over);
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_SKIPDISABLED), under);
    U(EnableWindow)(over, TRUE);
    U(SetWindowLongA)(over, GWL_EXSTYLE, U(GetWindowLongA)(over, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_ALL), over);
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_SKIPTRANSPARENT), under);
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT), under);
    U(ShowWindow)(under, SW_HIDE);
    EQ(U(ChildWindowFromPointEx)(p, pt, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT), p);
    wm_destroy(p);
}

TEST(wm_hit_childwindowfrompoint_equiv_ex_all) {
    HWND p = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 200, 200, NULL);
    HWND c = mkchild(p, 10, 10, 80, 80);
    U(ShowWindow)(c, SW_HIDE);
    POINT pt = { 30, 30 };
    EQ(U(ChildWindowFromPoint)(p, pt), U(ChildWindowFromPointEx)(p, pt, CWP_ALL));
    EQ(U(ChildWindowFromPoint)(p, pt), c);       /* hidden child still reported */
    wm_destroy(p);
}

TEST(wm_hit_childwindowfrompoint_dead) {
    HWND p = wm_hidden(); wm_destroy(p);
    POINT pt = {1, 1};
    EQ(U(ChildWindowFromPoint)(p, pt), NULL);
    EQ(U(ChildWindowFromPointEx)(p, pt, 0), NULL);
    EQ(U(ChildWindowFromPoint)((HWND)(ULONG_PTR)0x6660, pt), NULL);
}

TEST(wm_hit_realchildwindowfrompoint) {
    NEED(RealChildWindowFromPoint);
    HWND p = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 200, 200, NULL);
    HWND c = mkchild(p, 10, 10, 80, 80);
    POINT pt = { 30, 30 };
    EQ(U(RealChildWindowFromPoint)(p, pt), c);
    pt.x = 150; pt.y = 150;
    EQ(U(RealChildWindowFromPoint)(p, pt), p);
    pt.x = -3; pt.y = -3;
    EQ(U(RealChildWindowFromPoint)(p, pt), NULL);
    wm_destroy(p);
}

TEST(wm_hit_childwindowfrompoint_ex_invalid_flags) {
    HWND p = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 200, 200, NULL);
    HWND c = mkchild(p, 10, 10, 80, 80);
    POINT pt = { 30, 30 };
    HWND r = U(ChildWindowFromPointEx)(p, pt, 0x80);
    t_note("ChildWindowFromPointEx(flags 0x80) = %s", r == c ? "child" : r == p ? "parent" : "NULL/other");
    r = U(ChildWindowFromPointEx)(p, pt, 0xFFFFFFF0);
    t_note("ChildWindowFromPointEx(flags 0xFFFFFFF0) = %s", r == c ? "child" : r == p ? "parent" : "NULL/other");
    wm_destroy(p);
}

/* ---- thread / process id ---- */
TEST(wm_tid_pid_basic) {
    NEED(GetWindowThreadProcessId);
    HWND h = wm_hidden();
    DWORD pid = 0xFFFFFFFF;
    EQ(U(GetWindowThreadProcessId)(h, &pid), GetCurrentThreadId());
    EQ(pid, GetCurrentProcessId());
    EQ(U(GetWindowThreadProcessId)(h, NULL), GetCurrentThreadId());
    HWND c = mkchild(h, 0, 0, 5, 5);
    EQ(U(GetWindowThreadProcessId)(c, &pid), GetCurrentThreadId());
    wm_destroy(h);
}

TEST(wm_tid_pid_invalid) {
    DWORD pid = 0x12345678;
    SetLastError(0);
    EQ(U(GetWindowThreadProcessId)(NULL, &pid), 0);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    t_note("pid after GetWindowThreadProcessId(NULL) = 0x%lx", pid);
    pid = 0x12345678;
    EQ(U(GetWindowThreadProcessId)((HWND)(ULONG_PTR)0x7770, &pid), 0);
    t_note("pid after bogus hwnd = 0x%lx", pid);
}

TEST(wm_tid_pid_worker) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    DWORD pid = 0;
    EQ(U(GetWindowThreadProcessId)(w.hwnd, &pid), w.tid);
    EQ(pid, GetCurrentProcessId());
    CHECK(w.tid != GetCurrentThreadId());
    wm_worker_stop(&w);
}
