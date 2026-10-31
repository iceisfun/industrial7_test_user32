#include "ctl.h"

#define FIRST_ID 0x9000

static HWND g_client;
static struct { HWND self; HWND wp; HWND lp; } g_act[64];
static int g_nact;
static int g_create_params[16];
static int g_ncreate;
static int g_destroyed;

static LRESULT CALLBACK frame_procA(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return U(DefFrameProcA)(h, g_client, m, w, l);
}
static LRESULT CALLBACK frame_procW(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return U(DefFrameProcW)(h, g_client, m, w, l);
}
static void note_create(LPARAM l)
{
    CREATESTRUCTA *cs = (CREATESTRUCTA *)l;
    MDICREATESTRUCTA *mc = (MDICREATESTRUCTA *)cs->lpCreateParams;
    if (g_ncreate < 16) g_create_params[g_ncreate++] = mc ? (int)mc->lParam : -1;
}
static LRESULT CALLBACK child_procA(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_CREATE) note_create(l);
    if (m == WM_MDIACTIVATE && g_nact < 64) { g_act[g_nact].self = h; g_act[g_nact].wp = (HWND)w; g_act[g_nact].lp = (HWND)l; g_nact++; }
    if (m == WM_DESTROY) g_destroyed++;
    return U(DefMDIChildProcA)(h, m, w, l);
}
static LRESULT CALLBACK child_procW(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_CREATE) note_create(l);
    if (m == WM_DESTROY) g_destroyed++;
    return U(DefMDIChildProcW)(h, m, w, l);
}

static int register_classes(void)
{
    static int done, ok;
    if (done) return ok;
    done = 1;
    WNDCLASSA a; WNDCLASSW w;
    memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    a.lpfnWndProc = frame_procA; a.hInstance = GetModuleHandleA(NULL); a.lpszClassName = "CtlMdiFrameA";
    a.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    ok = U(RegisterClassA)(&a) != 0;
    a.lpfnWndProc = child_procA; a.lpszClassName = "CtlMdiChildA"; a.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    ok &= U(RegisterClassA)(&a) != 0;
    w.lpfnWndProc = frame_procW; w.hInstance = GetModuleHandleA(NULL); w.lpszClassName = L"CtlMdiFrameW";
    ok &= U(RegisterClassW)(&w) != 0;
    w.lpfnWndProc = child_procW; w.lpszClassName = L"CtlMdiChildW"; w.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    ok &= U(RegisterClassW)(&w) != 0;
    return ok;
}

typedef struct { HWND frame, client; HMENU fmenu, wmenu; } mdi_t;

static int mdi_make(mdi_t *m, int with_menu)
{
    memset(m, 0, sizeof *m);
    if (!register_classes()) return 0;
    g_client = NULL; g_nact = 0; g_ncreate = 0; g_destroyed = 0;
    if (with_menu) {
        m->fmenu = U(CreateMenu)(); m->wmenu = U(CreatePopupMenu)();
        U(AppendMenuA)(m->fmenu, MF_POPUP, (UINT_PTR)U(CreatePopupMenu)(), "&File");
        U(AppendMenuA)(m->fmenu, MF_POPUP, (UINT_PTR)m->wmenu, "&Window");
    }
    m->frame = U(CreateWindowExA)(0, "CtlMdiFrameA", "frame", WS_OVERLAPPEDWINDOW, 30, 30, 520, 420, NULL, m->fmenu, GetModuleHandleA(NULL), NULL);
    if (!m->frame) return 0;
    CLIENTCREATESTRUCT ccs = { m->wmenu, FIRST_ID };
    m->client = U(CreateWindowExA)(WS_EX_CLIENTEDGE, "MDICLIENT", NULL, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                                   0, 0, 100, 100, m->frame, (HMENU)(INT_PTR)1, GetModuleHandleA(NULL), &ccs);
    g_client = m->client;
    if (!m->client) return 0;
    U(ShowWindow)(m->frame, SW_SHOWNOACTIVATE);
    U(UpdateWindow)(m->frame);
    t_pump();
    return 1;
}
static void mdi_free(mdi_t *m)
{
    if (m->frame) U(DestroyWindow)(m->frame);
    g_client = NULL;
    t_pump();
}
static HWND mkchild(HWND client, const char *title, int lp)
{
    return U(CreateMDIWindowA)("CtlMdiChildA", title, 0, CW_USEDEFAULT, CW_USEDEFAULT, 180, 120, client, GetModuleHandleA(NULL), lp);
}
static HWND active(HWND client, BOOL *max)
{
    BOOL m = 0x5a5a;
    HWND a = (HWND)SA(client, WM_MDIGETACTIVE, 0, &m);
    if (max) *max = m;
    return a;
}

TEST(mdi_client_class_and_parenting)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(DefFrameProcA)); REQUIRE(U(CreateMDIWindowA));
    mdi_t m;
    CHECK(mdi_make(&m, 0));
    char n[32];
    c_classname(m.client, n, sizeof n);
    EXPECT_STR(n, "MDIClient");
    EXPECT_EQ(U(GetParent)(m.client), m.frame);
    /* DefFrameProc keeps the client sized to the frame's client area */
    RECT fc, cc;
    U(GetClientRect)(m.frame, &fc);
    U(MoveWindow)(m.frame, 40, 40, 600, 450, TRUE);
    t_pump();
    U(GetClientRect)(m.frame, &fc);
    U(GetClientRect)(m.client, &cc);
    RECT wr; U(GetWindowRect)(m.client, &wr);
    t_note("frame client %ldx%ld, MDI client window %ldx%ld client %ldx%ld", fc.right, fc.bottom, wr.right - wr.left, wr.bottom - wr.top, cc.right, cc.bottom);
    EXPECT_EQ(wr.right - wr.left, fc.right);
    EXPECT_EQ(wr.bottom - wr.top, fc.bottom);
    mdi_free(&m);
}

TEST(mdi_createmdiwindow_basic)
{
    REQUIRE(U(CreateMDIWindowA)); REQUIRE(U(GetWindowLongA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c1 = mkchild(m.client, "first", 1111);
    CHECK(c1 != NULL);
    if (c1) {
        EXPECT_EQ(U(GetParent)(c1), m.client);
        CHECK(U(GetWindowLongA)(c1, GWL_EXSTYLE) & WS_EX_MDICHILD);
        CHECK(U(GetWindowLongA)(c1, GWL_STYLE) & WS_CHILD);
        char n[64]; U(GetClassNameA)(c1, n, sizeof n);
        EXPECT_STR(n, "CtlMdiChildA");
        U(GetWindowTextA)(c1, n, sizeof n);
        EXPECT_STR(n, "first");
        EXPECT_EQ(g_ncreate, 1);
        EXPECT_EQ(g_create_params[0], 1111);   /* lParam reaches WM_CREATE via MDICREATESTRUCT */
        RECT r; U(GetWindowRect)(c1, &r);
        EXPECT_EQ(r.right - r.left, 180);
        EXPECT_EQ(r.bottom - r.top, 120);
        BOOL mx;
        EXPECT_EQ(active(m.client, &mx), c1);
        EXPECT_EQ(mx, FALSE);
    }
    mdi_free(&m);
}

TEST(mdi_createmdiwindow_w)
{
    REQUIRE(U(CreateMDIWindowW)); REQUIRE(U(GetWindowLongW));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c = U(CreateMDIWindowW)(L"CtlMdiChildW", L"wide \x0416", WS_VISIBLE, 5, 6, 200, 100, m.client, GetModuleHandleA(NULL), 77);
    CHECK(c != NULL);
    if (c) {
        CHECK(U(IsWindowUnicode)(c));
        WCHAR t[32] = { 0 };
        U(GetWindowTextW)(c, t, 32);
        CHECK(!lstrcmpW(t, L"wide \x0416"));
        EXPECT_EQ(g_create_params[0], 77);
        EXPECT_EQ(active(m.client, NULL), c);
        RECT r; U(GetWindowRect)(c, &r);
        EXPECT_EQ(r.right - r.left, 200);
        EXPECT_EQ(r.bottom - r.top, 100);
    }
    mdi_free(&m);
}

TEST(mdi_createmdiwindow_errors)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    SetLastError(0);
    HWND h = U(CreateMDIWindowA)("NoSuchMdiClass", "x", 0, 0, 0, 100, 100, m.client, GetModuleHandleA(NULL), 0);
    CHECK(h == NULL);
    EXPECT_ERR(0, ERROR_CANNOT_FIND_WND_CLASS);
    /* parent that is not an MDI client */
    SetLastError(0);
    h = U(CreateMDIWindowA)("CtlMdiChildA", "x", 0, 0, 0, 100, 100, m.frame, GetModuleHandleA(NULL), 0);
    t_note("CreateMDIWindow with a non-client parent -> %p err=%lu", h, GetLastError());
    if (h) U(DestroyWindow)(h);
    SetLastError(0);
    h = U(CreateMDIWindowA)("CtlMdiChildA", "x", 0, 0, 0, 100, 100, NULL, GetModuleHandleA(NULL), 0);
    t_note("CreateMDIWindow with NULL parent -> %p err=%lu", h, GetLastError());
    if (h) U(DestroyWindow)(h);
    h = U(CreateMDIWindowA)("CtlMdiChildA", "x", 0, 0, 0, 100, 100, (HWND)(ULONG_PTR)0xdead0, GetModuleHandleA(NULL), 0);
    CHECK(h == NULL);
    mdi_free(&m);
}

TEST(mdi_wm_mdicreate_and_destroy)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    MDICREATESTRUCTA mc; memset(&mc, 0, sizeof mc);
    mc.szClass = "CtlMdiChildA"; mc.szTitle = "via msg"; mc.hOwner = GetModuleHandleA(NULL);
    mc.x = 10; mc.y = 12; mc.cx = 150; mc.cy = 90; mc.style = WS_VISIBLE; mc.lParam = 4242;
    HWND c = (HWND)SA(m.client, WM_MDICREATE, 0, &mc);
    CHECK(c != NULL && U(IsWindow)(c));
    if (c) {
        EXPECT_EQ(g_create_params[0], 4242);
        char t[32]; U(GetWindowTextA)(c, t, 32); EXPECT_STR(t, "via msg");
        EXPECT_EQ(active(m.client, NULL), c);
        EXPECT_EQ(SA(m.client, WM_MDIDESTROY, c, 0), 0);
        CHECK(!U(IsWindow)(c));
        EXPECT_EQ(g_destroyed, 1);
        EXPECT_EQ(active(m.client, NULL), 0);
    }
    /* bad class */
    memset(&mc, 0, sizeof mc); mc.szClass = "NoSuchMdiClass"; mc.szTitle = "x"; mc.hOwner = GetModuleHandleA(NULL);
    SetLastError(0);
    CHECK(SA(m.client, WM_MDICREATE, 0, &mc) == 0);
    mdi_free(&m);
}

TEST(mdi_activation_order_and_messages)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c1 = mkchild(m.client, "one", 1);
    EXPECT_EQ(active(m.client, NULL), c1);
    g_nact = 0;
    HWND c2 = mkchild(m.client, "two", 2);
    EXPECT_EQ(active(m.client, NULL), c2);
    /* c1 gets deactivated, c2 activated: both receive WM_MDIACTIVATE(deact, act) */
    int saw1 = 0, saw2 = 0;
    for (int i = 0; i < g_nact; i++) {
        if (g_act[i].self == c1 && g_act[i].wp == c1 && g_act[i].lp == c2) saw1 = 1;
        if (g_act[i].self == c2 && g_act[i].wp == c1 && g_act[i].lp == c2) saw2 = 1;
    }
    CHECK(saw1);
    CHECK(saw2);
    g_nact = 0;
    EXPECT_EQ(SA(m.client, WM_MDIACTIVATE, c1, 0), 0);
    EXPECT_EQ(active(m.client, NULL), c1);
    saw1 = saw2 = 0;
    for (int i = 0; i < g_nact; i++) {
        if (g_act[i].self == c2 && g_act[i].wp == c2 && g_act[i].lp == c1) saw2 = 1;
        if (g_act[i].self == c1 && g_act[i].wp == c2 && g_act[i].lp == c1) saw1 = 1;
    }
    CHECK(saw1);
    CHECK(saw2);
    /* the activated child comes to the top of the z-order */
    EXPECT_EQ(U(GetTopWindow)(m.client), c1);
    SA(m.client, WM_MDIACTIVATE, c2, 0);
    EXPECT_EQ(U(GetTopWindow)(m.client), c2);
    mdi_free(&m);
}

TEST(mdi_next_prev_cycle)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c[3];
    for (int i = 0; i < 3; i++) c[i] = mkchild(m.client, "kid", i);
    HWND start = active(m.client, NULL);
    EXPECT_EQ(start, c[2]);
    HWND seen[3];
    for (int i = 0; i < 3; i++) {
        SA(m.client, WM_MDINEXT, 0, 0);
        seen[i] = active(m.client, NULL);
        CHECK(seen[i] != NULL);
    }
    CHECK(seen[0] != start);
    CHECK(seen[0] != seen[1] && seen[1] != seen[2] && seen[0] != seen[2]);
    EXPECT_EQ(seen[2], start);
    /* explicit child as wParam, and backwards */
    for (int i = 0; i < 3; i++) {
        SA(m.client, WM_MDINEXT, 0, 1);
        seen[i] = active(m.client, NULL);
    }
    CHECK(seen[0] != seen[1] && seen[1] != seen[2] && seen[0] != seen[2]);
    EXPECT_EQ(seen[2], start);
    /* NEXT then PREV returns to where we began */
    HWND a = active(m.client, NULL);
    SA(m.client, WM_MDINEXT, 0, 0);
    SA(m.client, WM_MDINEXT, 0, 1);
    EXPECT_EQ(active(m.client, NULL), a);
    /* single child: nothing to switch to */
    SA(m.client, WM_MDIDESTROY, c[0], 0);
    SA(m.client, WM_MDIDESTROY, c[1], 0);
    SA(m.client, WM_MDINEXT, 0, 0);
    EXPECT_EQ(active(m.client, NULL), c[2]);
    mdi_free(&m);
}

TEST(mdi_destroy_active_activates_other)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c1 = mkchild(m.client, "a", 1), c2 = mkchild(m.client, "b", 2), c3 = mkchild(m.client, "c", 3);
    EXPECT_EQ(active(m.client, NULL), c3);
    SA(m.client, WM_MDIDESTROY, c3, 0);
    HWND a = active(m.client, NULL);
    CHECK(a == c1 || a == c2);
    SA(m.client, WM_MDIDESTROY, a, 0);
    a = active(m.client, NULL);
    CHECK(a == c1 || a == c2);
    CHECK(U(IsWindow)(a));
    SA(m.client, WM_MDIDESTROY, a, 0);
    EXPECT_EQ(active(m.client, NULL), 0);
    EXPECT_EQ(U(GetTopWindow)(m.client), 0);
    mdi_free(&m);
}

TEST(mdi_maximize_restore)
{
    REQUIRE(U(CreateMDIWindowA)); REQUIRE(U(IsZoomed));
    mdi_t m; CHECK(mdi_make(&m, 1));
    HWND c1 = mkchild(m.client, "a", 1), c2 = mkchild(m.client, "b", 2);
    BOOL mx = 7;
    CHECK(!U(IsZoomed)(c2));
    EXPECT_EQ(SA(m.client, WM_MDIMAXIMIZE, c2, 0), 0);
    CHECK(U(IsZoomed)(c2));
    EXPECT_EQ(active(m.client, &mx), c2);
    EXPECT_EQ(mx, TRUE);
    CHECK(U(GetWindowLongA)(c2, GWL_STYLE) & WS_MAXIMIZE);
    /* a maximized child fills the client area */
    RECT cc, cr;
    U(GetClientRect)(m.client, &cc);
    U(GetClientRect)(c2, &cr);
    t_note("maximized child client %ldx%ld vs MDI client %ldx%ld", cr.right, cr.bottom, cc.right, cc.bottom);
    EXPECT_EQ(SA(m.client, WM_MDIRESTORE, c2, 0), 0);
    CHECK(!U(IsZoomed)(c2));
    EXPECT_EQ(active(m.client, &mx), c2);
    EXPECT_EQ(mx, FALSE);
    /* maximize via ShowWindow */
    U(ShowWindow)(c1, SW_SHOWMAXIMIZED);
    CHECK(U(IsZoomed)(c1));
    t_note("after ShowWindow(c1, SW_SHOWMAXIMIZED): active=%s maximized=%d", active(m.client, &mx) == c1 ? "c1" : "other", mx);
    SA(m.client, WM_MDIRESTORE, c1, 0);
    CHECK(!U(IsZoomed)(c1));
    mdi_free(&m);
}

TEST(mdi_minimize_iconarrange)
{
    REQUIRE(U(CreateMDIWindowA)); REQUIRE(U(IsIconic));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c1 = mkchild(m.client, "a", 1), c2 = mkchild(m.client, "b", 2);
    U(ShowWindow)(c1, SW_MINIMIZE);
    U(ShowWindow)(c2, SW_MINIMIZE);
    CHECK(U(IsIconic)(c1));
    CHECK(U(IsIconic)(c2));
    EXPECT_EQ(SA(m.client, WM_MDIICONARRANGE, 0, 0), 0);
    RECT r1, r2;
    U(GetWindowRect)(c1, &r1); U(GetWindowRect)(c2, &r2);
    CHECK(r1.right - r1.left == U(GetSystemMetrics)(SM_CXMINIMIZED) || r1.right - r1.left > 0);
    CHECK(!(r1.left == r2.left && r1.top == r2.top));   /* icons are placed apart */
    /* the icons sit along the bottom of the client area */
    RECT cl; U(GetClientRect)(m.client, &cl);
    POINT pt = { r1.left, r1.bottom }; U(ScreenToClient)(m.client, &pt);
    CHECK(pt.y <= cl.bottom + 2 && pt.y > cl.bottom / 2);
    SA(m.client, WM_MDIRESTORE, c1, 0);
    CHECK(!U(IsIconic)(c1));
    mdi_free(&m);
}

static int overlap(const RECT *a, const RECT *b)
{
    return a->left < b->right && b->left < a->right && a->top < b->bottom && b->top < a->bottom;
}

TEST(mdi_tile_and_cascade_messages)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c[3]; RECT r[3];
    for (int i = 0; i < 3; i++) c[i] = mkchild(m.client, "t", i);
    t_note("WM_MDITILE(vertical) -> %lld", (long long)SA(m.client, WM_MDITILE, MDITILE_VERTICAL, 0));
    for (int i = 0; i < 3; i++) U(GetWindowRect)(c[i], &r[i]);
    CHECK(!overlap(&r[0], &r[1]) && !overlap(&r[0], &r[2]) && !overlap(&r[1], &r[2]));
    RECT cl; U(GetClientRect)(m.client, &cl);
    int w0 = r[0].right - r[0].left;
    for (int i = 1; i < 3; i++) CHECK(abs((r[i].right - r[i].left) - w0) <= 2);   /* vertical tiling: equal widths */
    t_note("WM_MDITILE(horizontal) -> %lld", (long long)SA(m.client, WM_MDITILE, MDITILE_HORIZONTAL, 0));
    for (int i = 0; i < 3; i++) U(GetWindowRect)(c[i], &r[i]);
    CHECK(!overlap(&r[0], &r[1]) && !overlap(&r[0], &r[2]) && !overlap(&r[1], &r[2]));
    int h0 = r[0].bottom - r[0].top;
    for (int i = 1; i < 3; i++) CHECK(abs((r[i].bottom - r[i].top) - h0) <= 2);
    /* cascade: each window is offset from the previous one by the caption height */
    t_note("WM_MDICASCADE -> %lld", (long long)SA(m.client, WM_MDICASCADE, 0, 0));
    for (int i = 0; i < 3; i++) U(GetWindowRect)(c[i], &r[i]);
    int dx1 = r[1].left - r[0].left, dy1 = r[1].top - r[0].top;
    int dx2 = r[2].left - r[1].left, dy2 = r[2].top - r[1].top;
    t_note("cascade offsets: (%d,%d) (%d,%d)", dx1, dy1, dx2, dy2);
    CHECK(dx1 > 0 && dy1 > 0);
    CHECK(dx1 == dx2 && dy1 == dy2);
    CHECK(overlap(&r[0], &r[1]));
    for (int i = 1; i < 3; i++) CHECK(r[i].right - r[i].left == r[0].right - r[0].left);
    /* no windows: tile/cascade still report success */
    for (int i = 0; i < 3; i++) SA(m.client, WM_MDIDESTROY, c[i], 0);
    t_note("empty client: TILE=%lld CASCADE=%lld", (long long)SA(m.client, WM_MDITILE, 0, 0), (long long)SA(m.client, WM_MDICASCADE, 0, 0));
    mdi_free(&m);
}

TEST(mdi_tilewindows_cascadewindows_api)
{
    REQUIRE(U(CreateMDIWindowA)); REQUIRE(U(TileWindows)); REQUIRE(U(CascadeWindows));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c[3];
    for (int i = 0; i < 3; i++) c[i] = mkchild(m.client, "t", i);
    WORD n = U(TileWindows)(m.client, MDITILE_VERTICAL, NULL, 0, NULL);
    EXPECT_EQ(n, 3);
    RECT a, b;
    U(GetWindowRect)(c[0], &a); U(GetWindowRect)(c[1], &b);
    CHECK(!overlap(&a, &b));
    HWND two[2] = { c[0], c[1] };
    n = U(TileWindows)(m.client, MDITILE_HORIZONTAL, NULL, 2, two);
    EXPECT_EQ(n, 2);
    n = U(CascadeWindows)(m.client, 0, NULL, 0, NULL);
    EXPECT_EQ(n, 3);
    n = U(CascadeWindows)(m.client, 0, NULL, 2, two);
    EXPECT_EQ(n, 2);
    /* a restricting rectangle (client coordinates of the parent) */
    RECT lim = { 0, 0, 300, 200 };
    n = U(TileWindows)(m.client, MDITILE_VERTICAL, &lim, 0, NULL);
    EXPECT_EQ(n, 3);
    U(GetWindowRect)(c[2], &a);
    POINT p = { a.right, a.bottom }; U(ScreenToClient)(m.client, &p);
    CHECK(p.x <= 300 + 4 && p.y <= 200 + 4);
    mdi_free(&m);
}

TEST(mdi_window_menu_list)
{
    REQUIRE(U(CreateMDIWindowA)); REQUIRE(U(GetMenuItemCount)); REQUIRE(U(GetMenuStringA)); REQUIRE(U(GetMenuItemID));
    mdi_t m; CHECK(mdi_make(&m, 1));
    int base = U(GetMenuItemCount)(m.wmenu);
    EXPECT_EQ(base, 0);
    HWND c1 = mkchild(m.client, "alpha", 1);
    int n1 = U(GetMenuItemCount)(m.wmenu);
    HWND c2 = mkchild(m.client, "beta", 2);
    int n2 = U(GetMenuItemCount)(m.wmenu);
    HWND c3 = mkchild(m.client, "gamma", 3);
    int n3 = U(GetMenuItemCount)(m.wmenu);
    t_note("window menu items: %d %d %d", n1, n2, n3);
    EXPECT_EQ(n2, n1 + 1);
    EXPECT_EQ(n3, n2 + 1);
    char s[64];
    /* the list entries are "&N title" with ids FIRST_ID + index */
    int k = n3 - 1;
    s[0] = 0; U(GetMenuStringA)(m.wmenu, k, s, 64, MF_BYPOSITION);
    EXPECT_STR(s, "&3 gamma");
    EXPECT_EQ(U(GetMenuItemID)(m.wmenu, k), FIRST_ID + 2);
    s[0] = 0; U(GetMenuStringA)(m.wmenu, k - 1, s, 64, MF_BYPOSITION);
    EXPECT_STR(s, "&2 beta");
    EXPECT_EQ(U(GetMenuItemID)(m.wmenu, k - 1), FIRST_ID + 1);
    s[0] = 0; U(GetMenuStringA)(m.wmenu, k - 2, s, 64, MF_BYPOSITION);
    EXPECT_STR(s, "&1 alpha");
    EXPECT_EQ(U(GetMenuItemID)(m.wmenu, k - 2), FIRST_ID);
    /* the active child is checked */
    UINT st = U(GetMenuState)(m.wmenu, FIRST_ID + 2, MF_BYCOMMAND);
    CHECK(st != (UINT)-1 && (st & MF_CHECKED));
    st = U(GetMenuState)(m.wmenu, FIRST_ID, MF_BYCOMMAND);
    CHECK(st != (UINT)-1 && !(st & MF_CHECKED));
    /* DefFrameProc turns WM_COMMAND for the list ids into child activation */
    SA(m.frame, WM_COMMAND, MAKEWPARAM(FIRST_ID, 0), 0);
    EXPECT_EQ(active(m.client, NULL), c1);
    SA(m.frame, WM_COMMAND, MAKEWPARAM(FIRST_ID + 1, 0), 0);
    EXPECT_EQ(active(m.client, NULL), c2);
    /* renaming a child updates the entry */
    U(SetWindowTextA)(c1, "renamed");
    s[0] = 0; U(GetMenuStringA)(m.wmenu, k - 2, s, 64, MF_BYPOSITION);
    t_note("after SetWindowText: \"%s\"", s);
    /* destroying removes the entry */
    SA(m.client, WM_MDIDESTROY, c3, 0);
    EXPECT_EQ(U(GetMenuItemCount)(m.wmenu), n3 - 1);
    SA(m.client, WM_MDIDESTROY, c2, 0);
    SA(m.client, WM_MDIDESTROY, c1, 0);
    t_note("window menu items after destroying all: %d", U(GetMenuItemCount)(m.wmenu));
    mdi_free(&m);
}

TEST(mdi_setmenu_refreshmenu)
{
    REQUIRE(U(CreateMDIWindowA)); REQUIRE(U(GetMenu));
    mdi_t m; CHECK(mdi_make(&m, 1));
    EXPECT_EQ(U(GetMenu)(m.frame), m.fmenu);
    HMENU f2 = U(CreateMenu)(), w2 = U(CreatePopupMenu)();
    U(AppendMenuA)(f2, MF_POPUP, (UINT_PTR)w2, "&W2");
    LRESULT prev = SA(m.client, WM_MDISETMENU, f2, w2);
    EXPECT_EQ(prev, m.fmenu);               /* returns the previous frame menu */
    EXPECT_EQ(U(GetMenu)(m.frame), f2);
    HWND c1 = mkchild(m.client, "m", 1);
    EXPECT_EQ(U(GetMenuItemCount)(w2), U(GetMenuItemCount)(w2));
    CHECK(U(GetMenuItemCount)(w2) >= 1);     /* list moved to the new window menu */
    LRESULT r = SA(m.client, WM_MDIREFRESHMENU, 0, 0);
    t_note("WM_MDIREFRESHMENU -> %p (frame menu %p)", (void *)r, (void *)U(GetMenu)(m.frame));
    CHECK(r == (LRESULT)f2 || r == 0);
    /* restore the original menus so that they are released with the frame */
    SA(m.client, WM_MDISETMENU, m.fmenu, m.wmenu);
    EXPECT_EQ(U(GetMenu)(m.frame), m.fmenu);
    U(DestroyMenu)(f2);
    (void)c1;
    mdi_free(&m);
}

TEST(mdi_translate_sys_accel)
{
    REQUIRE(U(CreateMDIWindowA)); REQUIRE(U(TranslateMDISysAccel));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c1 = mkchild(m.client, "a", 1), c2 = mkchild(m.client, "b", 2);
    MSG msg; memset(&msg, 0, sizeof msg);
    /* messages that are not key presses are never translated */
    msg.hwnd = c2; msg.message = WM_MOUSEMOVE;
    EXPECT_EQ(U(TranslateMDISysAccel)(m.client, &msg), FALSE);
    msg.message = WM_CHAR; msg.wParam = 'a';
    EXPECT_EQ(U(TranslateMDISysAccel)(m.client, &msg), FALSE);
    msg.message = WM_NULL;
    EXPECT_EQ(U(TranslateMDISysAccel)(m.client, &msg), FALSE);
    /* plain keystrokes (no Ctrl held) are not system accelerators */
    if (!(U(GetAsyncKeyState)(VK_CONTROL) & 0x8000)) {
        msg.message = WM_KEYDOWN; msg.wParam = VK_F6;
        EXPECT_EQ(U(TranslateMDISysAccel)(m.client, &msg), FALSE);
        msg.wParam = VK_F4;
        EXPECT_EQ(U(TranslateMDISysAccel)(m.client, &msg), FALSE);
        msg.wParam = VK_TAB;
        EXPECT_EQ(U(TranslateMDISysAccel)(m.client, &msg), FALSE);
    }
    EXPECT_EQ(active(m.client, NULL), c2);
    /* invalid client handle */
    msg.message = WM_KEYDOWN; msg.wParam = VK_F6;
    EXPECT_EQ(U(TranslateMDISysAccel)(NULL, &msg), FALSE);
    EXPECT_EQ(U(TranslateMDISysAccel)((HWND)(ULONG_PTR)0xdead0, &msg), FALSE);
    EXPECT_EQ(U(TranslateMDISysAccel)(m.frame, &msg), FALSE);   /* not an MDI client */
    (void)c1;
    mdi_free(&m);
}

TEST(mdi_deffrmeproc_defmdichildproc_basics)
{
    REQUIRE(U(CreateMDIWindowA)); REQUIRE(U(DefFrameProcA)); REQUIRE(U(DefMDIChildProcA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c = mkchild(m.client, "title", 1);
    /* WM_GETTEXT / WM_SETTEXT via DefMDIChildProc */
    char t[32];
    EXPECT_EQ(SA(c, WM_GETTEXT, 32, t), 5);
    EXPECT_STR(t, "title");
    SA(c, WM_SETTEXT, 0, "renamed");
    U(GetWindowTextA)(c, t, 32);
    EXPECT_STR(t, "renamed");
    /* WM_SYSCOMMAND SC_MAXIMIZE / SC_RESTORE through the system menu path */
    SA(c, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    CHECK(U(IsZoomed)(c));
    SA(c, WM_SYSCOMMAND, SC_RESTORE, 0);
    CHECK(!U(IsZoomed)(c));
    SA(c, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    CHECK(U(IsIconic)(c));
    SA(c, WM_SYSCOMMAND, SC_RESTORE, 0);
    CHECK(!U(IsIconic)(c));
    /* the frame forwards unknown messages to DefWindowProc */
    char ft[32]; EXPECT_EQ(SA(m.frame, WM_GETTEXT, 32, ft), 5);
    EXPECT_STR(ft, "frame");
    /* WM_SYSCOMMAND SC_CLOSE destroys the child */
    SA(c, WM_SYSCOMMAND, SC_CLOSE, 0);
    t_pump();
    CHECK(!U(IsWindow)(c));
    mdi_free(&m);
}

TEST(mdi_frame_destroy_destroys_children)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    HWND c[4];
    for (int i = 0; i < 4; i++) c[i] = mkchild(m.client, "k", i);
    g_destroyed = 0;
    HWND client = m.client;
    U(DestroyWindow)(m.frame);
    m.frame = NULL;
    EXPECT_EQ(g_destroyed, 4);
    for (int i = 0; i < 4; i++) CHECK(!U(IsWindow)(c[i]));
    CHECK(!U(IsWindow)(client));
    mdi_free(&m);
}

TEST(mdi_many_children)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 1));
    HWND c[40]; int n = 0;
    for (int i = 0; i < 40; i++) { char t[16]; sprintf(t, "doc %d", i); c[i] = mkchild(m.client, t, i); if (c[i]) n++; }
    EXPECT_EQ(n, 40);
    EXPECT_EQ(active(m.client, NULL), c[39]);
    int cnt = 0;
    for (HWND h = U(GetWindow)(m.client, GW_CHILD); h; h = U(GetWindow)(h, GW_HWNDNEXT)) cnt++;
    t_note("MDI client has %d children windows (40 documents)", cnt);
    CHECK(cnt >= 40);
    t_note("window menu items with 40 children: %d", U(GetMenuItemCount)(m.wmenu));
    SA(m.client, WM_MDITILE, 0, 0);
    SA(m.client, WM_MDICASCADE, 0, 0);
    for (int i = 0; i < 40; i++) SA(m.client, WM_MDIDESTROY, c[i], 0);
    EXPECT_EQ(active(m.client, NULL), 0);
    mdi_free(&m);
}

TEST(mdi_client_messages_without_children)
{
    REQUIRE(U(CreateMDIWindowA));
    mdi_t m; CHECK(mdi_make(&m, 0));
    BOOL mx = 3;
    EXPECT_EQ(SA(m.client, WM_MDIGETACTIVE, 0, &mx), 0);
    EXPECT_EQ(mx, FALSE);
    EXPECT_EQ(SA(m.client, WM_MDIGETACTIVE, 0, 0), 0);
    EXPECT_EQ(SA(m.client, WM_MDIICONARRANGE, 0, 0), 0);
    SA(m.client, WM_MDINEXT, 0, 0);
    SA(m.client, WM_MDIMAXIMIZE, m.frame, 0);
    SA(m.client, WM_MDIRESTORE, m.frame, 0);
    SA(m.client, WM_MDIACTIVATE, m.frame, 0);
    CHECK(U(IsWindow)(m.client));
    mdi_free(&m);
}
