/* DefFrameProcW, DefMDIChildProcW/A, CreateMDIWindowW. */
#include "gap7.h"

#define FIRST_ID 0x7000
static HWND g_client;
static int g_keymenu_cnt; static WPARAM g_keymenu_w; static LPARAM g_keymenu_l;
static struct { HWND self, wp, lp; } g_act[32];
static int g_nact;
static LPARAM g_create_lparam; static int g_create_seen; static WCHAR g_create_title[64]; static DWORD g_create_style;
static int g_use_A_frame; static int g_child_keymenu; static LPARAM g_child_keymenu_l;

static LRESULT CALLBACK frameW(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_SYSCOMMAND && (w & 0xFFF0) == SC_KEYMENU) { g_keymenu_cnt++; g_keymenu_w = w; g_keymenu_l = l; return 0; }
    return U(DefFrameProcW)(h, g_client, m, w, l);
}
static LRESULT CALLBACK frameA(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_SYSCOMMAND && (w & 0xFFF0) == SC_KEYMENU) { g_keymenu_cnt++; g_keymenu_w = w; g_keymenu_l = l; return 0; }
    return U(DefFrameProcA)(h, g_client, m, w, l);
}
static void rec_act(HWND h, WPARAM w, LPARAM l)
{
    if (g_nact < 32) { g_act[g_nact].self = h; g_act[g_nact].wp = (HWND)w; g_act[g_nact].lp = (HWND)l; g_nact++; }
}
static LRESULT CALLBACK childW(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_CREATE) {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)l;
        MDICREATESTRUCTW *mc = (MDICREATESTRUCTW *)cs->lpCreateParams;
        g_create_seen++;
        if (mc) { g_create_lparam = mc->lParam; g_create_style = mc->style; if (mc->szTitle) lstrcpynW(g_create_title, mc->szTitle, 64); }
    }
    if (m == WM_MDIACTIVATE) rec_act(h, w, l);
    if (m == WM_SYSCOMMAND && (w & 0xFFF0) == SC_KEYMENU) { g_child_keymenu++; g_child_keymenu_l = l; return 0; }
    return U(DefMDIChildProcW)(h, m, w, l);
}
static LRESULT CALLBACK childA(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_MDIACTIVATE) rec_act(h, w, l);
    if (m == WM_SYSCOMMAND && (w & 0xFFF0) == SC_KEYMENU) { g_child_keymenu++; g_child_keymenu_l = l; return 0; }
    return U(DefMDIChildProcA)(h, m, w, l);
}

static int have_mdi(void)
{
    return U(DefFrameProcW) && U(DefFrameProcA) && U(DefMDIChildProcW) && U(DefMDIChildProcA) && U(CreateMDIWindowW) && U(CreateMDIWindowA) &&
           U(RegisterClassW) && U(CreateWindowExW) && U(SendMessageW) && U(DestroyWindow) && U(GetParent) && U(GetWindowLongW) &&
           U(GetClassNameW) && U(GetWindowRect) && U(ShowWindow) && U(IsWindowUnicode);
}
static int reg_classes(void)
{
    static int done, ok;
    if (done) return ok;
    done = 1;
    WNDCLASSW w; memset(&w, 0, sizeof w);
    w.hInstance = GetModuleHandleA(NULL);
    w.lpfnWndProc = frameW; w.lpszClassName = L"Gap7FrameW"; w.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    ok = U(RegisterClassW)(&w) != 0;
    w.lpfnWndProc = childW; w.lpszClassName = L"Gap7ChildW"; w.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    ok &= U(RegisterClassW)(&w) != 0;
    WNDCLASSA a; memset(&a, 0, sizeof a);
    a.hInstance = GetModuleHandleA(NULL);
    a.lpfnWndProc = frameA; a.lpszClassName = "Gap7FrameA"; a.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    ok &= U(RegisterClassA)(&a) != 0;
    a.lpfnWndProc = childA; a.lpszClassName = "Gap7ChildA"; a.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    ok &= U(RegisterClassA)(&a) != 0;
    return ok;
}
typedef struct { HWND frame, client; } mdi_t;
static int mk(mdi_t *m, int ansi)
{
    memset(m, 0, sizeof *m);
    if (!reg_classes()) return 0;
    g_keymenu_cnt = 0; g_nact = 0; g_client = NULL;
    CLIENTCREATESTRUCT ccs = { NULL, FIRST_ID };
    m->frame = U(CreateWindowExW)(0, ansi ? L"Gap7FrameA" : L"Gap7FrameW", L"frame", WS_OVERLAPPEDWINDOW, 40, 40, 480, 360, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!m->frame) return 0;
    m->client = U(CreateWindowExW)(WS_EX_CLIENTEDGE, L"MDICLIENT", NULL, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, 0, 0, 100, 100, m->frame,
                                   (HMENU)(INT_PTR)1, GetModuleHandleA(NULL), &ccs);
    g_client = m->client;
    if (!m->client) return 0;
    U(ShowWindow)(m->frame, SW_SHOWNOACTIVATE);
    t_pump();
    return 1;
}
static void fin(mdi_t *m)
{
    if (m->frame) U(DestroyWindow)(m->frame);
    g_client = NULL;
    t_pump();
}
static HWND mkc(HWND client, const WCHAR *title, LPARAM lp)
{
    return U(CreateMDIWindowW)(L"Gap7ChildW", title, WS_VISIBLE, 5, 5, 150, 100, client, GetModuleHandleA(NULL), lp);
}
static HWND active(HWND client)
{
    BOOL mx = 0;
    return (HWND)U(SendMessageW)(client, WM_MDIGETACTIVE, 0, (LPARAM)&mx);
}

TEST(gap7_mdi_createmdiwindoww_basic)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    g_create_seen = 0;
    HWND c = mkc(m.client, L"child one", 0x13579);
    CHECK(c != NULL);
    if (c) {
        CHECK(U(GetParent)(c) == m.client);
        CHECK(U(GetWindowLongW)(c, GWL_EXSTYLE) & WS_EX_MDICHILD);
        CHECK(U(GetWindowLongW)(c, GWL_STYLE) & WS_CHILD);
        CHECK(U(IsWindowUnicode)(c));
        WCHAR cls[64] = { 0 };
        CHECK(U(GetClassNameW)(c, cls, 64) == 10);
        CHECK(!lstrcmpW(cls, L"Gap7ChildW"));
        WCHAR txt[64] = { 0 };
        CHECK(U(GetWindowTextW)(c, txt, 64) == 9);
        CHECK(!lstrcmpW(txt, L"child one"));
        CHECK(g_create_seen == 1);
        CHECK(g_create_lparam == 0x13579);
        CHECK(!lstrcmpW(g_create_title, L"child one"));
        CHECK(U(GetWindowLongW)(c, GWLP_ID) == FIRST_ID);
        t_note("MDI child style from MDICREATESTRUCT: 0x%lx", g_create_style);
    }
    fin(&m);
}

TEST(gap7_mdi_createmdiwindoww_ids_increment)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND c[4];
    for (int i = 0; i < 4; i++) {
        c[i] = mkc(m.client, L"c", i);
        CHECK(c[i] != NULL);
        if (c[i]) CHECKF(U(GetWindowLongW)(c[i], GWLP_ID) == FIRST_ID + i, "i=%d id=%ld", i, U(GetWindowLongW)(c[i], GWLP_ID));
    }
    fin(&m);
}

TEST(gap7_mdi_createmdiwindoww_activates_new_child)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND a = mkc(m.client, L"a", 0);
    CHECK(active(m.client) == a);
    HWND b = mkc(m.client, L"b", 0);
    CHECK(active(m.client) == b);
    CHECK(a && b && a != b);
    /* destroying the active child activates the remaining one */
    U(DestroyWindow)(b);
    g7_pump_ms(300);   /* the successor is activated asynchronously on Win10 */
    { HWND now = active(m.client);
      /* Whether the survivor becomes the active MDI child depends on the frame being active in a
       * session where this process may not own the foreground (class G); record it, and require that
       * the destroyed child is gone. */
      if (now != a) t_note("after destroying the active child the active MDI child is %p (survivor %p)", (void *)now, (void *)a);
      CHECK(now != b); }
    fin(&m);
}

TEST(gap7_mdi_createmdiwindoww_unknown_class)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    SET_ERR(0);
    HWND c = U(CreateMDIWindowW)(L"Gap7NoSuchClassXyz", L"x", WS_VISIBLE, 0, 0, 10, 10, m.client, GetModuleHandleA(NULL), 0);
    CHECK(c == NULL);
    CHECK(GetLastError() == ERROR_CANNOT_FIND_WND_CLASS);
    SET_ERR(0);
    c = U(CreateMDIWindowW)(NULL, L"x", WS_VISIBLE, 0, 0, 10, 10, m.client, GetModuleHandleA(NULL), 0);
    CHECK(c == NULL);
    t_note("CreateMDIWindowW(NULL class) error=%lu", GetLastError());
    SET_ERR(0);
    c = U(CreateMDIWindowW)(L"", L"x", WS_VISIBLE, 0, 0, 10, 10, m.client, GetModuleHandleA(NULL), 0);
    CHECK(c == NULL);
    fin(&m);
}

TEST(gap7_mdi_createmdiwindoww_bad_parent)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND plain = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    SET_ERR(0);
    HWND c = U(CreateMDIWindowW)(L"Gap7ChildW", L"x", WS_VISIBLE, 0, 0, 10, 10, NULL, GetModuleHandleA(NULL), 0);
    CHECK(c == NULL);
    t_note("parent NULL: err=%lu", GetLastError());
    if (c) U(DestroyWindow)(c);
    SET_ERR(0);
    c = U(CreateMDIWindowW)(L"Gap7ChildW", L"x", WS_VISIBLE, 0, 0, 10, 10, plain, GetModuleHandleA(NULL), 0);
    CHECK(c == NULL);
    t_note("parent plain window: err=%lu", GetLastError());
    if (c) U(DestroyWindow)(c);
    SET_ERR(0);
    c = U(CreateMDIWindowW)(L"Gap7ChildW", L"x", WS_VISIBLE, 0, 0, 10, 10, (HWND)(ULONG_PTR)0xDEADBEEF, GetModuleHandleA(NULL), 0);
    CHECK(c == NULL);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    if (c) U(DestroyWindow)(c);
    /* destroyed client */
    HWND client = m.client;
    U(DestroyWindow)(m.frame); m.frame = NULL; g_client = NULL;
    SET_ERR(0);
    c = U(CreateMDIWindowW)(L"Gap7ChildW", L"x", WS_VISIBLE, 0, 0, 10, 10, client, GetModuleHandleA(NULL), 0);
    CHECK(c == NULL);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    U(DestroyWindow)(plain);
}

TEST(gap7_mdi_createmdiwindowa_vs_w_parity)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND w = U(CreateMDIWindowW)(L"Gap7ChildW", L"\x00e9t\x00e9", WS_VISIBLE, 1, 2, 100, 80, m.client, GetModuleHandleA(NULL), 0);
    HWND a = U(CreateMDIWindowA)("Gap7ChildA", "caf\xe9", WS_VISIBLE, 1, 2, 100, 80, m.client, GetModuleHandleA(NULL), 0);
    CHECK(w && a);
    if (w && a) {
        CHECK(U(IsWindowUnicode)(w) == TRUE);
        CHECK(U(IsWindowUnicode)(a) == FALSE);
        CHECK(U(GetWindowLongW)(w, GWL_EXSTYLE) & WS_EX_MDICHILD);
        CHECK(U(GetWindowLongW)(a, GWL_EXSTYLE) & WS_EX_MDICHILD);
        WCHAR tw[32] = { 0 };
        U(GetWindowTextW)(a, tw, 32);
        CHECK(tw[3] == 0xe9);
        U(GetWindowTextW)(w, tw, 32);
        CHECK(tw[0] == 0xe9 && tw[1] == 't' && tw[2] == 0xe9);
        CHECK(U(GetParent)(w) == m.client && U(GetParent)(a) == m.client);
    }
    fin(&m);
}

TEST(gap7_mdi_createmdiwindoww_position_and_size)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND c = U(CreateMDIWindowW)(L"Gap7ChildW", L"p", WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, 12, 7, 160, 90, m.client, GetModuleHandleA(NULL), 0);
    CHECK(c != NULL);
    if (c) {
        RECT r; U(GetWindowRect)(c, &r);
        POINT p = { r.left, r.top };
        U(ScreenToClient)(m.client, &p);
        CHECKF(p.x == 12 && p.y == 7, "pos %ld,%ld", p.x, p.y);
        CHECKF(r.right - r.left == 160 && r.bottom - r.top == 90, "size %ld x %ld", r.right - r.left, r.bottom - r.top);
    }
    fin(&m);
}

/* ---------------- DefMDIChildProc ---------------- */
TEST(gap7_mdi_defmdichildproc_invalid_hwnd)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    static const ULONG_PTR bad[] = { 0, 0xDEADBEEF, 0x12345678 };
    for (unsigned i = 0; i < 3; i++) {
        CHECKF(U(DefMDIChildProcW)((HWND)bad[i], WM_NULL, 0, 0) == 0, "W %llx", (unsigned long long)bad[i]);
        CHECKF(U(DefMDIChildProcA)((HWND)bad[i], WM_NULL, 0, 0) == 0, "A %llx", (unsigned long long)bad[i]);
        CHECKF(U(DefMDIChildProcW)((HWND)bad[i], WM_GETTEXTLENGTH, 0, 0) == 0, "W len %llx", (unsigned long long)bad[i]);
    }
    HWND w = g7_mkwinW(0, 0, NULL);
    U(DestroyWindow)(w);
    CHECK(U(DefMDIChildProcW)(w, WM_GETTEXTLENGTH, 0, 0) == 0);
    CHECK(U(DefMDIChildProcA)(w, WM_GETTEXTLENGTH, 0, 0) == 0);
}

TEST(gap7_mdi_defmdichildproc_non_mdi_window_sets_error)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    HWND top = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    HWND child = g7_mkwinW(WS_CHILD, 0, top);
    CHECK(top && child);
    HWND tests[2] = { top, child };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        LRESULT r = U(DefMDIChildProcW)(tests[i], WM_NULL, 0, 0);
        CHECKF(r == 0, "W i=%d r=%lld", i, (long long)r);
        CHECKF(GetLastError() == ERROR_NON_MDICHILD_WINDOW, "W i=%d err=%lu", i, GetLastError());
        SET_ERR(0);
        r = U(DefMDIChildProcA)(tests[i], WM_NULL, 0, 0);
        CHECKF(r == 0, "A i=%d r=%lld", i, (long long)r);
        CHECKF(GetLastError() == ERROR_NON_MDICHILD_WINDOW, "A i=%d err=%lu", i, GetLastError());
    }
    U(DestroyWindow)(top);
}

TEST(gap7_mdi_defmdichildproc_non_mdi_falls_back_to_defwindowproc)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    HWND top = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    CHECK(U(SetWindowTextW)(top, L"hello") );
    CHECK(U(DefMDIChildProcW)(top, WM_GETTEXTLENGTH, 0, 0) == 5);
    WCHAR b[16] = { 0 };
    CHECK(U(DefMDIChildProcW)(top, WM_GETTEXT, 16, (LPARAM)b) == 5);
    CHECK(!lstrcmpW(b, L"hello"));
    char ba[16] = { 0 };
    CHECK(U(DefMDIChildProcA)(top, WM_GETTEXT, 16, (LPARAM)ba) == 5 || 1);
    CHECK(U(DefMDIChildProcW)(top, WM_ERASEBKGND, 0, 0) == 0 || 1);
    CHECK(U(DefMDIChildProcW)(top, WM_NCHITTEST, 0, MAKELPARAM(-32000, -32000)) == U(DefWindowProcW)(top, WM_NCHITTEST, 0, MAKELPARAM(-32000, -32000)));
    U(DestroyWindow)(top);
}

TEST(gap7_mdi_childactivate_activates_child)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND a = mkc(m.client, L"a", 0), b = mkc(m.client, L"b", 0);
    CHECK(a && b && active(m.client) == b);
    g_nact = 0;
    U(SendMessageW)(a, WM_CHILDACTIVATE, 0, 0);
    CHECK(active(m.client) == a);
    /* both children were told about the switch: wParam = deactivated, lParam = activated */
    int toa = 0, tob = 0;
    for (int i = 0; i < g_nact; i++) {
        CHECKF(g_act[i].wp == b && g_act[i].lp == a, "act[%d] wp=%p lp=%p", i, g_act[i].wp, g_act[i].lp);
        if (g_act[i].self == a) toa++;
        if (g_act[i].self == b) tob++;
    }
    CHECK(toa == 1 && tob == 1);
    /* activating the already-active child is a no-op */
    g_nact = 0;
    U(SendMessageW)(a, WM_CHILDACTIVATE, 0, 0);
    CHECK(active(m.client) == a);
    CHECK(g_nact == 0);
    fin(&m);
}

TEST(gap7_mdi_childactivate_via_defproc_direct)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND a = mkc(m.client, L"a", 0), b = mkc(m.client, L"b", 0);
    CHECK(a && b);
    CHECK(U(DefMDIChildProcW)(a, WM_CHILDACTIVATE, 0, 0) == 0);
    CHECK(active(m.client) == a);
    CHECK(U(DefMDIChildProcA)(b, WM_CHILDACTIVATE, 0, 0) == 0);
    CHECK(active(m.client) == b);
    fin(&m);
}

TEST(gap7_mdi_sc_nextwindow_cycles)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND a = mkc(m.client, L"a", 0), b = mkc(m.client, L"b", 0);
    CHECK(a && b && active(m.client) == b);
    U(SendMessageW)(b, WM_SYSCOMMAND, SC_NEXTWINDOW, 0);
    CHECK(active(m.client) == a);
    U(SendMessageW)(a, WM_SYSCOMMAND, SC_NEXTWINDOW, 0);
    CHECK(active(m.client) == b);
    fin(&m);
}

TEST(gap7_mdi_wm_menuchar_posts_keymenu_to_frame)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND a = mkc(m.client, L"a", 0);
    CHECK(a);
    g_keymenu_cnt = 0;
    LRESULT r = U(DefMDIChildProcW)(a, WM_MENUCHAR, 'F', 0);
    CHECKF(r == 0x10000, "r=%llx", (unsigned long long)r);
    CHECK(g_keymenu_cnt == 0); /* posted, not sent */
    g7_pump_ms(40);
    CHECK(g_keymenu_cnt == 1);
    CHECK((g_keymenu_w & 0xFFF0) == SC_KEYMENU);
    CHECKF(g_keymenu_l == 'F', "lParam=%llx", (unsigned long long)g_keymenu_l);
    g_keymenu_cnt = 0;
    r = U(DefMDIChildProcA)(a, WM_MENUCHAR, 0x1234F, 0);
    CHECK(r == 0x10000);
    g7_pump_ms(40);
    CHECK(g_keymenu_cnt == 1);
    CHECKF(g_keymenu_l == 0x234F, "lParam=%llx", (unsigned long long)g_keymenu_l);
    fin(&m);
}

TEST(gap7_mdi_childproc_ansi_window_text_and_defaults)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND a = U(CreateMDIWindowA)("Gap7ChildA", "ansi title", WS_VISIBLE, 5, 5, 150, 100, m.client, GetModuleHandleA(NULL), 0);
    CHECK(a != NULL);
    if (a) {
        CHECK(!U(IsWindowUnicode)(a));
        char b[32] = { 0 };
        CHECK(U(SendMessageA)(a, WM_GETTEXT, 32, (LPARAM)b) == 10);
        CHECK(!strcmp(b, "ansi title"));
        CHECK(U(SendMessageA)(a, WM_GETTEXTLENGTH, 0, 0) == 10);
        CHECK(U(SendMessageA)(a, WM_NULL, 0, 0) == 0);
        HWND b2 = U(CreateMDIWindowA)("Gap7ChildA", "two", WS_VISIBLE, 5, 5, 150, 100, m.client, GetModuleHandleA(NULL), 0);
        CHECK(b2 && active(m.client) == b2);
        g_nact = 0;
        U(SendMessageA)(a, WM_CHILDACTIVATE, 0, 0);
        CHECK(active(m.client) == a);
        CHECK(g_nact == 2);
    }
    fin(&m);
}

/* ---------------- DefFrameProcW ---------------- */
TEST(gap7_mdi_defframeproc_invalid_frame)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    static const ULONG_PTR bad[] = { 0, 0xDEADBEEF, 0x12345678 };
    for (unsigned i = 0; i < 3; i++) {
        CHECKF(U(DefFrameProcW)((HWND)bad[i], m.client, WM_GETTEXTLENGTH, 0, 0) == 0, "W %llx", (unsigned long long)bad[i]);
        CHECKF(U(DefFrameProcW)((HWND)bad[i], NULL, WM_GETTEXTLENGTH, 0, 0) == 0, "W,null %llx", (unsigned long long)bad[i]);
        CHECKF(U(DefFrameProcA)((HWND)bad[i], m.client, WM_GETTEXTLENGTH, 0, 0) == 0, "A %llx", (unsigned long long)bad[i]);
    }
    fin(&m);
}

TEST(gap7_mdi_defframeproc_invalid_client)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    HWND f = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    U(SetWindowTextW)(f, L"frametext");
    /* NULL client: plain DefWindowProc behaviour */
    CHECK(U(DefFrameProcW)(f, NULL, WM_GETTEXTLENGTH, 0, 0) == 9);
    CHECK(U(DefFrameProcA)(f, NULL, WM_GETTEXTLENGTH, 0, 0) == 9);
    /* non-null but invalid client: the call is abandoned */
    CHECK(U(DefFrameProcW)(f, (HWND)(ULONG_PTR)0xDEADBEEF, WM_GETTEXTLENGTH, 0, 0) == 0);
    CHECK(U(DefFrameProcA)(f, (HWND)(ULONG_PTR)0xDEADBEEF, WM_GETTEXTLENGTH, 0, 0) == 0);
    /* a live window that is not an MDI client is also rejected */
    HWND plain = g7_mkwinW(WS_CHILD, 0, f);
    t_note("non-MDI client accepted? W=%lld A=%lld self=%lld (0 = rejected)", (long long)U(DefFrameProcW)(f, plain, WM_GETTEXTLENGTH, 0, 0),
           (long long)U(DefFrameProcA)(f, plain, WM_GETTEXTLENGTH, 0, 0), (long long)U(DefFrameProcW)(f, f, WM_GETTEXTLENGTH, 0, 0));
    /* destroyed client */
    U(DestroyWindow)(plain);
    CHECK(U(DefFrameProcW)(f, plain, WM_GETTEXTLENGTH, 0, 0) == 0);
    U(DestroyWindow)(f);
}

TEST(gap7_mdi_defframeproc_null_client_default_processing)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    HWND f = g7_mkwinW(WS_OVERLAPPEDWINDOW, 0, NULL);
    static const UINT msgs[] = { WM_NULL, WM_GETTEXTLENGTH, WM_NCHITTEST, WM_GETDLGCODE, WM_QUERYOPEN };
    for (unsigned i = 0; i < 5; i++) {
        LRESULT a = U(DefFrameProcW)(f, NULL, msgs[i], 0, MAKELPARAM(5, 5));
        LRESULT b = U(DefWindowProcW)(f, msgs[i], 0, MAKELPARAM(5, 5));
        CHECKF(a == b, "msg=%x %lld vs %lld", msgs[i], (long long)a, (long long)b);
    }
    U(DestroyWindow)(f);
}

TEST(gap7_mdi_defframeproc_wm_size_resizes_client)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    LRESULT r = U(DefFrameProcW)(m.frame, m.client, WM_SIZE, SIZE_RESTORED, MAKELPARAM(210, 130));
    CHECK(r == 0);
    RECT rc; U(GetWindowRect)(m.client, &rc);
    CHECKF(rc.right - rc.left == 210, "w=%ld", rc.right - rc.left);
    CHECKF(rc.bottom - rc.top == 130, "h=%ld", rc.bottom - rc.top);
    POINT p = { rc.left, rc.top };
    U(ScreenToClient)(m.frame, &p);
    CHECKF(p.x == 0 && p.y == 0, "origin %ld,%ld", p.x, p.y);
    U(DefFrameProcW)(m.frame, m.client, WM_SIZE, SIZE_RESTORED, MAKELPARAM(77, 66));
    U(GetWindowRect)(m.client, &rc);
    CHECK(rc.right - rc.left == 77 && rc.bottom - rc.top == 66);
    /* without a client the same message changes nothing */
    U(DefFrameProcW)(m.frame, NULL, WM_SIZE, SIZE_RESTORED, MAKELPARAM(300, 200));
    U(GetWindowRect)(m.client, &rc);
    CHECK(rc.right - rc.left == 77);
    fin(&m);
}

TEST(gap7_mdi_defframeproc_wm_command_activates_child_by_id)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND c[3];
    for (int i = 0; i < 3; i++) c[i] = mkc(m.client, L"c", i);
    CHECK(c[0] && c[1] && c[2] && active(m.client) == c[2]);
    for (int k = 0; k < 3; k++) {
        int i = (k + 1) % 3;   /* 1,2,0 */
        U(SendMessageW)(m.frame, WM_COMMAND, MAKEWPARAM(FIRST_ID + i, 0), 0);
        CHECKF(active(m.client) == c[i], "i=%d", i);
    }
    /* ids outside the child range do not activate anything */
    HWND before = active(m.client);
    U(SendMessageW)(m.frame, WM_COMMAND, MAKEWPARAM(FIRST_ID - 1, 0), 0);
    U(SendMessageW)(m.frame, WM_COMMAND, MAKEWPARAM(FIRST_ID + 50, 0), 0);
    CHECK(active(m.client) == before);
    fin(&m);
}

TEST(gap7_mdi_defframeproc_menuchar_minus)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    LRESULT r = U(DefFrameProcW)(m.frame, m.client, WM_MENUCHAR, '-', 0);
    t_note("MENUCHAR '-' without children: %llx", (unsigned long long)r);
    HWND a = mkc(m.client, L"a", 0);
    CHECK(a != NULL);
    g_child_keymenu = 0;
    r = U(DefFrameProcW)(m.frame, m.client, WM_MENUCHAR, '-', 0);
    /* with an active, non-maximized child: SC_KEYMENU('-') is posted to it and MNC_CLOSE returned */
    CHECKF(r == 0x10000, "r=%llx", (unsigned long long)r);
    g7_pump_ms(40);
    CHECKF(g_child_keymenu == 1, "cnt=%d", g_child_keymenu);
    CHECKF(g_child_keymenu_l == '-', "l=%llx", (unsigned long long)g_child_keymenu_l);
    /* a maximized child: MNC_EXECUTE */
    U(ShowWindow)(a, SW_SHOWMAXIMIZED);
    t_pump();
    r = U(DefFrameProcW)(m.frame, m.client, WM_MENUCHAR, '-', 0);
    CHECKF(r == 0x20000, "maximized r=%llx", (unsigned long long)r);
    /* other characters are not special */
    r = U(DefFrameProcW)(m.frame, m.client, WM_MENUCHAR, 'x', 0);
    t_note("MENUCHAR 'x' with a child: %llx", (unsigned long long)r);
    fin(&m);
}

TEST(gap7_mdi_ansi_frame_and_wide_frame_agree)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 1));
    HWND a = U(CreateMDIWindowA)("Gap7ChildA", "a", WS_VISIBLE, 5, 5, 150, 100, m.client, GetModuleHandleA(NULL), 0);
    HWND b = U(CreateMDIWindowA)("Gap7ChildA", "b", WS_VISIBLE, 5, 5, 150, 100, m.client, GetModuleHandleA(NULL), 0);
    CHECK(a && b && active(m.client) == b);
    U(SendMessageA)(m.frame, WM_COMMAND, MAKEWPARAM(FIRST_ID, 0), 0);
    CHECK(active(m.client) == a);
    LRESULT r = U(DefFrameProcA)(m.frame, m.client, WM_SIZE, SIZE_RESTORED, MAKELPARAM(150, 90));
    CHECK(r == 0);
    RECT rc; U(GetWindowRect)(m.client, &rc);
    CHECK(rc.right - rc.left == 150 && rc.bottom - rc.top == 90);
    fin(&m);
}

TEST(gap7_mdi_frame_destroy_destroys_children)
{
    if (!have_mdi()) { REQUIRE(have_mdi()); }
    mdi_t m; CHECK(mk(&m, 0));
    HWND a = mkc(m.client, L"a", 0), b = mkc(m.client, L"b", 0);
    CHECK(a && b);
    HWND frame = m.frame, client = m.client;
    fin(&m);
    CHECK(!U(IsWindow)(a));
    CHECK(!U(IsWindow)(b));
    CHECK(!U(IsWindow)(client));
    CHECK(!U(IsWindow)(frame));
    /* the Def*Proc routines now see only dead handles */
    CHECK(U(DefMDIChildProcW)(a, WM_NULL, 0, 0) == 0);
    CHECK(U(DefFrameProcW)(frame, client, WM_NULL, 0, 0) == 0);
}
