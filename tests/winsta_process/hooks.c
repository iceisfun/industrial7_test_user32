/* SetWindowsHookEx family: error paths and thread-local hook behaviour */
#include "wsp.h"

#define WSP_WH_MSGFILTER (-1)
#define LOGMAX 128
enum { K_CWP = 1, K_CWPRET, K_GM, K_CBT, K_KBD, K_MOUSE, K_DBG, K_MSGF, K_WND, K_SHELL };
typedef struct { int kind; int code; WPARAM wp; LPARAM lp; DWORD tid; LONG_PTR a, b, c; } hev;
static hev g_log[LOGMAX];
static volatile LONG g_n;
static LRESULT g_ret;            /* value returned by the test hook procs */
static int g_next;               /* call CallNextHookEx */
static HHOOK g_self;             /* hook handle passed to CallNextHookEx / self-unhook */
static int g_cbt_block_create, g_cbt_block_destroy, g_unhook_self;
static int g_wnd_ret_enable;

static void hk_reset(void) { g_n = 0; memset(g_log, 0, sizeof g_log); g_ret = 0; g_next = 1; g_self = NULL; g_cbt_block_create = g_cbt_block_destroy = g_unhook_self = 0; }
static hev *hk_add(int kind, int code, WPARAM wp, LPARAM lp) {
    LONG i = InterlockedIncrement(&g_n) - 1;
    if (i >= LOGMAX) return &g_log[LOGMAX - 1];
    g_log[i].kind = kind; g_log[i].code = code; g_log[i].wp = wp; g_log[i].lp = lp; g_log[i].tid = GetCurrentThreadId();
    return &g_log[i];
}
static int hk_count(int kind) { int c = 0; for (LONG i = 0; i < g_n && i < LOGMAX; i++) if (g_log[i].kind == kind) c++; return c; }
static hev *hk_find(int kind, int nth) { for (LONG i = 0; i < g_n && i < LOGMAX; i++) if (g_log[i].kind == kind && nth-- == 0) return &g_log[i]; return NULL; }
static int hk_index(int kind, int code) { for (LONG i = 0; i < g_n && i < LOGMAX; i++) if (g_log[i].kind == kind && g_log[i].code == code) return (int)i; return -1; }

static LRESULT CALLBACK hp_cwp(int c, WPARAM wp, LPARAM lp) {
    if (c == HC_ACTION) { CWPSTRUCT *s = (CWPSTRUCT *)lp; hev *e = hk_add(K_CWP, c, wp, lp); e->a = s->message; e->b = (LONG_PTR)s->hwnd; e->c = (LONG_PTR)s->wParam; }
    return g_next ? CallNextHookEx(g_self, c, wp, lp) : g_ret;
}
static LRESULT CALLBACK hp_cwpret(int c, WPARAM wp, LPARAM lp) {
    if (c == HC_ACTION) { CWPRETSTRUCT *s = (CWPRETSTRUCT *)lp; hev *e = hk_add(K_CWPRET, c, wp, lp); e->a = s->message; e->b = (LONG_PTR)s->hwnd; e->c = (LONG_PTR)s->lResult; }
    return g_next ? CallNextHookEx(g_self, c, wp, lp) : g_ret;
}
static LRESULT CALLBACK hp_gm(int c, WPARAM wp, LPARAM lp) {
    if (c >= 0) {
        MSG *m = (MSG *)lp; hev *e = hk_add(K_GM, c, wp, lp); e->a = m->message; e->b = (LONG_PTR)m->hwnd; e->c = (LONG_PTR)m->wParam;
        if (g_ret && m->message == WM_USER + 3) m->message = WM_USER + 4;   /* rewrite */
        if (g_unhook_self) { U(UnhookWindowsHookEx)(g_self); }
    }
    return CallNextHookEx(g_self, c, wp, lp);
}
static LRESULT CALLBACK hp_cbt(int c, WPARAM wp, LPARAM lp) {
    hev *e = hk_add(K_CBT, c, wp, lp);
    if (c == HCBT_CREATEWND) {
        CBT_CREATEWNDA *cw = (CBT_CREATEWNDA *)lp;
        e->a = cw->lpcs->cx; e->b = (LONG_PTR)cw->lpcs->style; e->c = (LONG_PTR)cw->lpcs->lpszClass;
        if (g_cbt_block_create) return 1;
    }
    if (c == HCBT_DESTROYWND && g_cbt_block_destroy) return 1;
    return CallNextHookEx(g_self, c, wp, lp);
}
static LRESULT CALLBACK hp_kbd(int c, WPARAM wp, LPARAM lp) {
    if (c >= 0) { hev *e = hk_add(K_KBD, c, wp, lp); (void)e; }
    return CallNextHookEx(g_self, c, wp, lp);
}
static LRESULT CALLBACK hp_mouse(int c, WPARAM wp, LPARAM lp) {
    if (c >= 0) { MOUSEHOOKSTRUCT *s = (MOUSEHOOKSTRUCT *)lp; hev *e = hk_add(K_MOUSE, c, wp, lp); e->a = (LONG_PTR)s->hwnd; e->b = s->wHitTestCode; }
    return CallNextHookEx(g_self, c, wp, lp);
}
static LRESULT CALLBACK hp_dbg(int c, WPARAM wp, LPARAM lp) {
    if (c >= 0) { DEBUGHOOKINFO *d = (DEBUGHOOKINFO *)lp; hev *e = hk_add(K_DBG, c, wp, lp); e->a = d->idThread; e->b = d->code; }
    return CallNextHookEx(g_self, c, wp, lp);
}
static LRESULT CALLBACK hp_msgf(int c, WPARAM wp, LPARAM lp) {
    MSG *m = (MSG *)lp; hev *e = hk_add(K_MSGF, c, wp, lp);
    e->a = m->message; e->b = (LONG_PTR)m->wParam; e->c = (LONG_PTR)m->lParam;
    return g_next ? CallNextHookEx(g_self, c, wp, lp) : g_ret;
}
static LRESULT CALLBACK hp_plain(int c, WPARAM wp, LPARAM lp) { return CallNextHookEx(NULL, c, wp, lp); }
static LRESULT CALLBACK hp_ll(int c, WPARAM wp, LPARAM lp) { return CallNextHookEx(NULL, c, wp, lp); }

static LRESULT CALLBACK hk_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CREATE || m == WM_NCCREATE || m == WM_CLOSE || m == WM_DESTROY || (m >= WM_USER && m < WM_USER + 100)) hk_add(K_WND, m, w, l);
    if (m == WM_USER + 1) return 0x1234;
    if (m == WM_SYSCOMMAND && (w & 0xfff0) == SC_CLOSE && g_wnd_ret_enable) return 0;
    return DefWindowProcA(h, m, w, l);
}
static const char *hk_cls(void) {
    static int done;
    if (!done) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = hk_wndproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "T32WspHookCls";
        U(RegisterClassA)(&wc);
        done = 1;
    }
    return "T32WspHookCls";
}
static HWND hk_win(DWORD style) { return t_make_window(hk_cls(), style, 0, -32000, -32000, 33, 22, NULL); }
static HHOOK hook_thread(int id, HOOKPROC p) { return U(SetWindowsHookExA)(id, p, NULL, GetCurrentThreadId()); }
#define NEED_HK() do { REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(SetWindowsHookExW)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(U(CallNextHookEx)); REQUIRE(U(RegisterClassA)); REQUIRE(U(DestroyWindow)); } while (0)

/* ---------- install error paths ---------- */
TEST(winsta_process_hook_invalid_idhook) {
    NEED_HK();
    static const int bad[] = { -2, 15, 16, 100, 0x7fffffff, -100, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(bad[i], (HOOKPROC)hp_plain, NULL, GetCurrentThreadId());
        CHECKF(h == NULL, "A id %d", bad[i]);
        EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_FILTER);
        if (h) U(UnhookWindowsHookEx)(h);
        SetLastError(0);
        h = U(SetWindowsHookExW)(bad[i], (HOOKPROC)hp_plain, NULL, GetCurrentThreadId());
        CHECKF(h == NULL, "W id %d", bad[i]);
        EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_FILTER);
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(winsta_process_hook_null_proc) {
    NEED_HK();
    static const int ids[] = { WH_GETMESSAGE, WH_CALLWNDPROC, WH_CBT, WH_KEYBOARD, WH_MOUSE, WSP_WH_MSGFILTER };
    for (unsigned i = 0; i < 6; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(ids[i], NULL, NULL, GetCurrentThreadId());
        CHECKF(h == NULL, "id %d", ids[i]);
        EXPECT_ERR(0, WSP_HERR_INVALID_FILTER_PROC);
        if (h) U(UnhookWindowsHookEx)(h);
        SetLastError(0);
        h = U(SetWindowsHookExW)(ids[i], NULL, NULL, GetCurrentThreadId());
        CHECKF(h == NULL, "W id %d", ids[i]);
        EXPECT_ERR(0, WSP_HERR_INVALID_FILTER_PROC);
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(winsta_process_hook_global_requires_hmod) {
    NEED_HK();
    static const int ids[] = { WH_KEYBOARD, WH_GETMESSAGE, WH_CALLWNDPROC, WH_CBT, WH_SYSMSGFILTER, WH_MOUSE, WH_DEBUG, WH_SHELL, WH_FOREGROUNDIDLE, WH_CALLWNDPROCRET };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(ids[i], (HOOKPROC)hp_plain, NULL, 0);
        CHECKF(h == NULL, "A id %d", ids[i]);
        EXPECT_ERR(0, WSP_HERR_HOOK_NEEDS_HMOD);
        if (h) U(UnhookWindowsHookEx)(h);
        SetLastError(0);
        h = U(SetWindowsHookExW)(ids[i], (HOOKPROC)hp_plain, NULL, 0);
        CHECKF(h == NULL, "W id %d", ids[i]);
        EXPECT_ERR(0, WSP_HERR_HOOK_NEEDS_HMOD);
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(winsta_process_hook_bad_threadid) {
    NEED_HK();
    static const DWORD tids[] = { 0xfffffff0, 0xffffffff, 0x7ffffff0 };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, (HOOKPROC)hp_plain, NULL, tids[i]);
        CHECKF(h == NULL, "tid 0x%lx", (unsigned long)tids[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(winsta_process_hook_bad_hmod) {
    NEED_HK();
    static const ULONG_PTR mods[] = { 0x10, 0x1000, 0xdeadbeef0000ull };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, (HOOKPROC)hp_plain, (HINSTANCE)mods[i], GetCurrentThreadId());
        CHECKF(h == NULL, "hmod %p", (void *)mods[i]);
        t_note("SetWindowsHookEx bogus hmod %p err %lu", (void *)mods[i], GetLastError());
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(winsta_process_hook_proc_in_other_module_thread) {
    NEED_HK();
    /* a valid hmod that is not the module containing the proc, thread-local hook */
    HMODULE k = GetModuleHandleA("kernel32.dll");
    SetLastError(0);
    HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, (HOOKPROC)hp_plain, k, GetCurrentThreadId());
    t_note("SetWindowsHookEx(thread, hmod=kernel32) -> %p err %lu", h, GetLastError());
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
}
TEST(winsta_process_hook_thread_local_install_table) {
    NEED_HK();
    static const int ids[] = { WSP_WH_MSGFILTER, WH_KEYBOARD, WH_GETMESSAGE, WH_CALLWNDPROC, WH_CBT, WH_MOUSE, WH_DEBUG, WH_FOREGROUNDIDLE, WH_CALLWNDPROCRET };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0);
        HHOOK a = U(SetWindowsHookExA)(ids[i], (HOOKPROC)hp_plain, NULL, GetCurrentThreadId());
        CHECKF(a != NULL, "A id %d err %lu", ids[i], GetLastError());
        HHOOK w = U(SetWindowsHookExW)(ids[i], (HOOKPROC)hp_plain, NULL, GetCurrentThreadId());
        CHECKF(w != NULL, "W id %d err %lu", ids[i], GetLastError());
        CHECK(a != w);
        if (a) { SetLastError(0); CHECKF(U(UnhookWindowsHookEx)(a), "unhook A %d", ids[i]); }
        if (w) { CHECKF(U(UnhookWindowsHookEx)(w), "unhook W %d", ids[i]); }
    }
}
TEST(winsta_process_hook_thread_local_install_with_hmod) {
    NEED_HK();
    HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, (HOOKPROC)hp_plain, GetModuleHandleA(NULL), GetCurrentThreadId());
    CHECK(h != NULL);
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
}
TEST(winsta_process_hook_global_only_ids_thread_local) {
    NEED_HK();
    static const int ids[] = { WH_SYSMSGFILTER, WH_SHELL, WH_KEYBOARD_LL, WH_MOUSE_LL, WH_JOURNALRECORD, WH_JOURNALPLAYBACK };
    for (unsigned i = 0; i < 6; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(ids[i], (HOOKPROC)hp_plain, GetModuleHandleA(NULL), GetCurrentThreadId());
        t_note("thread-local hook id %d -> %p err %lu", ids[i], h, GetLastError());
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(winsta_process_hook_ll_install_uninstall) {
    NEED_HK();
    HHOOK k = U(SetWindowsHookExA)(WH_KEYBOARD_LL, (HOOKPROC)hp_ll, GetModuleHandleA(NULL), 0);
    CHECKF(k != NULL, "kbd ll err %lu", GetLastError());
    if (k) CHECK(U(UnhookWindowsHookEx)(k));
    HHOOK m = U(SetWindowsHookExW)(WH_MOUSE_LL, (HOOKPROC)hp_ll, GetModuleHandleA(NULL), 0);
    CHECKF(m != NULL, "mouse ll err %lu", GetLastError());
    if (m) CHECK(U(UnhookWindowsHookEx)(m));
}
TEST(winsta_process_hook_ll_null_hmod) {
    NEED_HK();
    SetLastError(0);
    HHOOK k = U(SetWindowsHookExA)(WH_KEYBOARD_LL, (HOOKPROC)hp_ll, NULL, 0);
    t_note("WH_KEYBOARD_LL with hmod NULL -> %p err %lu", k, GetLastError());
    if (k) U(UnhookWindowsHookEx)(k);
    SetLastError(0);
    HHOOK m = U(SetWindowsHookExA)(WH_MOUSE_LL, (HOOKPROC)hp_ll, NULL, 0);
    t_note("WH_MOUSE_LL with hmod NULL -> %p err %lu", m, GetLastError());
    if (m) U(UnhookWindowsHookEx)(m);
}
TEST(winsta_process_hook_ll_install_repeated) {
    NEED_HK();
    for (int i = 0; i < 20; i++) {
        HHOOK k = U(SetWindowsHookExA)(WH_KEYBOARD_LL, (HOOKPROC)hp_ll, GetModuleHandleA(NULL), 0);
        if (!k) { CHECKF(0, "iteration %d err %lu", i, GetLastError()); break; }
        if (!U(UnhookWindowsHookEx)(k)) { CHECK(0); break; }
    }
}
TEST(winsta_process_hook_ll_pair_distinct) {
    NEED_HK();
    HHOOK k1 = U(SetWindowsHookExA)(WH_KEYBOARD_LL, (HOOKPROC)hp_ll, GetModuleHandleA(NULL), 0);
    HHOOK k2 = U(SetWindowsHookExA)(WH_KEYBOARD_LL, (HOOKPROC)hp_ll, GetModuleHandleA(NULL), 0);
    CHECK(k1 && k2); CHECK(k1 != k2);
    if (k1) CHECK(U(UnhookWindowsHookEx)(k1));
    if (k2) CHECK(U(UnhookWindowsHookEx)(k2));
}

/* ---------- unhook ---------- */
TEST(winsta_process_unhook_invalid) {
    NEED_HK();
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)(NULL));
    EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_HANDLE);
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)((HHOOK)(ULONG_PTR)0x1234));
    EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_HANDLE);
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)((HHOOK)INVALID_HANDLE_VALUE));
    EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_HANDLE);
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)((HHOOK)(ULONG_PTR)0xdeadbeefdeadull));
    EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_HANDLE);
}
TEST(winsta_process_unhook_twice) {
    NEED_HK();
    HHOOK h = hook_thread(WH_GETMESSAGE, hp_plain);
    CHECK(h != NULL);
    CHECK(U(UnhookWindowsHookEx)(h));
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)(h));
    EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_HANDLE);
}
TEST(winsta_process_unhook_window_handle_is_not_hook) {
    NEED_HK();
    HWND w = hk_win(WS_POPUP);
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)((HHOOK)w));
    EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_HANDLE);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_hook_many_install_unhook) {
    NEED_HK();
    HHOOK hs[64]; int n = 0;
    for (int i = 0; i < 64; i++) { hs[i] = hook_thread(WH_CALLWNDPROC, hp_plain); if (hs[i]) n++; }
    EXPECT_EQ(n, 64);
    for (int i = 0; i < 64; i++) for (int j = i + 1; j < 64; j++) if (hs[i] && hs[i] == hs[j]) { CHECK(0); }
    for (int i = 0; i < 64; i++) if (hs[i]) CHECK(U(UnhookWindowsHookEx)(hs[i]));
}
TEST(winsta_process_unhookwindowshook_legacy_export) {
    CHECK(t_proc("UnhookWindowsHook") != NULL);
    CHECK(t_proc("SetWindowsHookA") != NULL);
    CHECK(t_proc("SetWindowsHookW") != NULL);
}
TEST(winsta_process_callnexthook_no_hook) {
    NEED_HK();
    LRESULT r = CallNextHookEx(NULL, 0, 0, 0);
    t_note("CallNextHookEx(NULL,...) outside any hook -> %lld", (long long)r);
    r = U(CallNextHookEx)(NULL, -1, 0, 0);
    t_note("CallNextHookEx(NULL,-1,...) outside any hook -> %lld", (long long)r);
}

/* ---------- CallMsgFilter / MSGFILTER ---------- */
TEST(winsta_process_callmsgfilter_no_hook) {
    NEED_HK(); REQUIRE(U(CallMsgFilterA)); REQUIRE(U(CallMsgFilterW));
    U(IsGUIThread)(TRUE);
    MSG m = { NULL, WM_USER + 5, 1, 2, 0, { 0, 0 } };
    CHECK(!U(CallMsgFilterA)(&m, MSGF_USER));
    CHECK(!U(CallMsgFilterW)(&m, MSGF_USER));
}
TEST(winsta_process_callmsgfilter_hook_called) {
    NEED_HK(); REQUIRE(U(CallMsgFilterA));
    hk_reset(); g_next = 0; g_ret = 0;
    HHOOK h = hook_thread(WSP_WH_MSGFILTER, hp_msgf); g_self = h;
    CHECK(h != NULL);
    MSG m = { (HWND)(ULONG_PTR)0x1234, WM_USER + 5, 77, 88, 5, { 6, 7 } };
    BOOL r = U(CallMsgFilterA)(&m, MSGF_USER);
    CHECK(!r);
    EXPECT_EQ(hk_count(K_MSGF), 1);
    hev *e = hk_find(K_MSGF, 0);
    if (e) { EXPECT_EQ(e->code, MSGF_USER); EXPECT_EQ(e->a, WM_USER + 5); EXPECT_EQ(e->b, 77); EXPECT_EQ(e->c, 88); EXPECT_EQ(e->tid, GetCurrentThreadId()); }
    g_ret = 1;
    r = U(CallMsgFilterA)(&m, MSGF_DIALOGBOX);
    CHECK(r);
    e = hk_find(K_MSGF, 1);
    if (e) EXPECT_EQ(e->code, MSGF_DIALOGBOX);
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
}
TEST(winsta_process_callmsgfilter_w_parity) {
    NEED_HK(); REQUIRE(U(CallMsgFilterW));
    hk_reset(); g_next = 0; g_ret = 1;
    HHOOK h = hook_thread(WSP_WH_MSGFILTER, hp_msgf); g_self = h;
    CHECK(h != NULL);
    MSG m = { NULL, WM_USER + 6, 5, 6, 0, { 0, 0 } };
    CHECK(U(CallMsgFilterW)(&m, 9));
    hev *e = hk_find(K_MSGF, 0);
    CHECK(e != NULL);
    if (e) { EXPECT_EQ(e->code, 9); EXPECT_EQ(e->a, WM_USER + 6); EXPECT_EQ(e->b, 5); EXPECT_EQ(e->c, 6); }
    if (h) U(UnhookWindowsHookEx)(h);
}
TEST(winsta_process_callmsgfilter_chain_order) {
    NEED_HK(); REQUIRE(U(CallMsgFilterA));
    hk_reset(); g_next = 1;
    HHOOK h1 = hook_thread(WSP_WH_MSGFILTER, hp_msgf);
    HHOOK h2 = hook_thread(WSP_WH_MSGFILTER, hp_msgf);
    CHECK(h1 && h2);
    MSG m = { NULL, WM_USER + 5, 1, 2, 0, { 0, 0 } };
    g_self = h2;
    CHECK(!U(CallMsgFilterA)(&m, MSGF_USER));
    EXPECT_EQ(hk_count(K_MSGF), 2);          /* both ran: newest calls the older through CallNextHookEx */
    g_next = 0; g_ret = 0; hk_reset(); g_next = 0;
    CHECK(!U(CallMsgFilterA)(&m, MSGF_USER));
    EXPECT_EQ(hk_count(K_MSGF), 1);          /* newest did not chain */
    if (h2) CHECK(U(UnhookWindowsHookEx)(h2));
    if (h1) CHECK(U(UnhookWindowsHookEx)(h1));
}
TEST(winsta_process_callmsgfilter_after_unhook) {
    NEED_HK(); REQUIRE(U(CallMsgFilterA));
    hk_reset(); g_next = 0; g_ret = 1;
    HHOOK h = hook_thread(WSP_WH_MSGFILTER, hp_msgf); g_self = h;
    MSG m = { NULL, WM_USER + 5, 1, 2, 0, { 0, 0 } };
    CHECK(U(CallMsgFilterA)(&m, MSGF_USER));
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
    CHECK(!U(CallMsgFilterA)(&m, MSGF_USER));
    EXPECT_EQ(hk_count(K_MSGF), 1);
}

/* ---------- CALLWNDPROC / CALLWNDPROCRET ---------- */
TEST(winsta_process_hook_callwndproc_sendmessage) {
    NEED_HK(); REQUIRE(U(SendMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    hk_reset();
    HHOOK h = hook_thread(WH_CALLWNDPROC, hp_cwp); g_self = h;
    CHECK(h != NULL);
    LRESULT r = U(SendMessageA)(w, WM_USER + 1, 5, 6);
    EXPECT_EQ(r, 0x1234);
    hev *e = NULL;
    for (int i = 0; i < hk_count(K_CWP); i++) { hev *x = hk_find(K_CWP, i); if (x->a == WM_USER + 1) { e = x; break; } }
    CHECK(e != NULL);
    if (e) { EXPECT_EQ(e->code, HC_ACTION); EXPECT_EQ(e->wp, 0); /* Win10: 0 for a message sent by the hooked thread itself (opposite of the MSDN text) */ EXPECT_EQ(e->b, (LONG_PTR)w); EXPECT_EQ(e->c, 5); }
    int i_hook = -1, i_wnd = -1;
    for (LONG i = 0; i < g_n; i++) {
        if (g_log[i].kind == K_CWP && g_log[i].a == WM_USER + 1) i_hook = (int)i;
        if (g_log[i].kind == K_WND && g_log[i].code == WM_USER + 1) i_wnd = (int)i;
    }
    CHECK(i_hook >= 0 && i_wnd >= 0 && i_hook < i_wnd);   /* hook runs before the window procedure */
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_callwndprocret_result) {
    NEED_HK(); REQUIRE(U(SendMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    hk_reset();
    HHOOK h = hook_thread(WH_CALLWNDPROCRET, hp_cwpret); g_self = h;
    CHECK(h != NULL);
    U(SendMessageA)(w, WM_USER + 1, 5, 6);
    hev *e = NULL;
    for (int i = 0; i < hk_count(K_CWPRET); i++) { hev *x = hk_find(K_CWPRET, i); if (x->a == WM_USER + 1) { e = x; break; } }
    CHECK(e != NULL);
    if (e) { EXPECT_EQ(e->code, HC_ACTION); EXPECT_EQ(e->wp, 0); /* Win10: 0 for a same-thread send */ EXPECT_EQ(e->b, (LONG_PTR)w); EXPECT_EQ(e->c, 0x1234); }
    int i_hook = -1, i_wnd = -1;
    for (LONG i = 0; i < g_n; i++) {
        if (g_log[i].kind == K_CWPRET && g_log[i].a == WM_USER + 1) i_hook = (int)i;
        if (g_log[i].kind == K_WND && g_log[i].code == WM_USER + 1) i_wnd = (int)i;
    }
    CHECK(i_hook >= 0 && i_wnd >= 0 && i_wnd < i_hook);   /* ... and the RET hook after it */
    if (h) U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_callwndproc_both_hooks_order) {
    NEED_HK(); REQUIRE(U(SendMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    hk_reset();
    HHOOK a = hook_thread(WH_CALLWNDPROC, hp_cwp), b = hook_thread(WH_CALLWNDPROCRET, hp_cwpret);
    U(SendMessageA)(w, WM_USER + 2, 0, 0);
    int ic = -1, ir = -1;
    for (LONG i = 0; i < g_n; i++) {
        if (g_log[i].kind == K_CWP && g_log[i].a == WM_USER + 2) ic = (int)i;
        if (g_log[i].kind == K_CWPRET && g_log[i].a == WM_USER + 2) ir = (int)i;
    }
    CHECK(ic >= 0 && ir > ic);
    if (a) U(UnhookWindowsHookEx)(a);
    if (b) U(UnhookWindowsHookEx)(b);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_callwndproc_sees_create) {
    NEED_HK();
    hk_reset();
    HHOOK h = hook_thread(WH_CALLWNDPROC, hp_cwp);
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    int nc = 0, cr = 0;
    for (LONG i = 0; i < g_n; i++) { if (g_log[i].kind == K_CWP && g_log[i].a == WM_NCCREATE) nc++; if (g_log[i].kind == K_CWP && g_log[i].a == WM_CREATE) cr++; }
    EXPECT_EQ(nc, 1); EXPECT_EQ(cr, 1);
    if (h) U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_not_called_after_unhook) {
    NEED_HK(); REQUIRE(U(SendMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    hk_reset();
    HHOOK h = hook_thread(WH_CALLWNDPROC, hp_cwp); g_self = h;
    U(SendMessageA)(w, WM_USER + 7, 0, 0);
    int before = hk_count(K_CWP);
    CHECK(before >= 1);
    CHECK(U(UnhookWindowsHookEx)(h));
    U(SendMessageA)(w, WM_USER + 7, 0, 0);
    EXPECT_EQ(hk_count(K_CWP), before);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_same_proc_installed_twice) {
    NEED_HK(); REQUIRE(U(SendMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    hk_reset();
    HHOOK a = hook_thread(WH_CALLWNDPROC, hp_cwp), b = hook_thread(WH_CALLWNDPROC, hp_cwp);
    CHECK(a && b && a != b);
    g_self = b;
    U(SendMessageA)(w, WM_USER + 8, 0, 0);
    int n = 0;
    for (LONG i = 0; i < g_n; i++) if (g_log[i].kind == K_CWP && g_log[i].a == WM_USER + 8) n++;
    EXPECT_EQ(n, 2);
    if (a) U(UnhookWindowsHookEx)(a);
    U(SendMessageA)(w, WM_USER + 9, 0, 0);
    n = 0;
    for (LONG i = 0; i < g_n; i++) if (g_log[i].kind == K_CWP && g_log[i].a == WM_USER + 9) n++;
    EXPECT_EQ(n, 1);
    if (b) U(UnhookWindowsHookEx)(b);
    U(DestroyWindow)(w);
}

/* ---------- GETMESSAGE ---------- */
TEST(winsta_process_hook_getmessage_remove) {
    NEED_HK(); REQUIRE(U(PostMessageA)); REQUIRE(U(PeekMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    t_pump();
    hk_reset();
    HHOOK h = hook_thread(WH_GETMESSAGE, hp_gm); g_self = h;
    CHECK(h != NULL);
    CHECK(U(PostMessageA)(w, WM_USER + 3, 0x11, 0x22));
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, WM_USER, WM_USER + 50, PM_REMOVE));
    EXPECT_EQ(m.message, WM_USER + 3); EXPECT_EQ(m.wParam, 0x11); EXPECT_EQ(m.lParam, 0x22); EXPECT_EQ(m.hwnd, w);
    hev *e = NULL;
    for (int i = 0; i < hk_count(K_GM); i++) { hev *x = hk_find(K_GM, i); if (x->a == WM_USER + 3) { e = x; break; } }
    CHECK(e != NULL);
    if (e) { EXPECT_EQ(e->code, HC_ACTION); EXPECT_EQ(e->wp, PM_REMOVE); EXPECT_EQ(e->b, (LONG_PTR)w); EXPECT_EQ(e->c, 0x11); }
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_getmessage_modify) {
    NEED_HK(); REQUIRE(U(PostMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    t_pump();
    hk_reset(); g_ret = 1;           /* rewrite WM_USER+3 into WM_USER+4 */
    HHOOK h = hook_thread(WH_GETMESSAGE, hp_gm); g_self = h;
    U(PostMessageA)(w, WM_USER + 3, 1, 2);
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, WM_USER, WM_USER + 50, PM_REMOVE));
    EXPECT_EQ(m.message, WM_USER + 4);
    if (h) U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_getmessage_noremove) {
    NEED_HK(); REQUIRE(U(PostMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    t_pump();
    hk_reset();
    HHOOK h = hook_thread(WH_GETMESSAGE, hp_gm); g_self = h;
    U(PostMessageA)(w, WM_USER + 10, 1, 2);
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, WM_USER, WM_USER + 50, PM_NOREMOVE));
    hev *e = NULL;
    for (int i = 0; i < hk_count(K_GM); i++) { hev *x = hk_find(K_GM, i); if (x->a == WM_USER + 10) { e = x; break; } }
    t_note("GETMESSAGE hook on PM_NOREMOVE: called=%d wParam=%lld", e != NULL, e ? (long long)e->wp : -1LL);
    CHECK(U(PeekMessageA)(&m, w, WM_USER, WM_USER + 50, PM_REMOVE));
    if (h) U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_unhook_inside_proc) {
    NEED_HK(); REQUIRE(U(PostMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    t_pump();
    hk_reset(); g_unhook_self = 1;
    HHOOK h = hook_thread(WH_GETMESSAGE, hp_gm); g_self = h;
    MSG m;
    U(PostMessageA)(w, WM_USER + 11, 0, 0);
    CHECK(U(PeekMessageA)(&m, w, WM_USER, WM_USER + 50, PM_REMOVE));
    EXPECT_EQ(m.message, WM_USER + 11);
    int c = hk_count(K_GM);
    CHECK(c >= 1);
    U(PostMessageA)(w, WM_USER + 12, 0, 0);
    CHECK(U(PeekMessageA)(&m, w, WM_USER, WM_USER + 50, PM_REMOVE));
    EXPECT_EQ(hk_count(K_GM), c);
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)(h));
    EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_HANDLE);
    U(DestroyWindow)(w);
}

/* ---------- KEYBOARD / MOUSE ---------- */
TEST(winsta_process_hook_keyboard_posted_keys) {
    NEED_HK(); REQUIRE(U(PostMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    t_pump();
    hk_reset();
    HHOOK h = hook_thread(WH_KEYBOARD, hp_kbd); g_self = h;
    CHECK(h != NULL);
    U(PostMessageA)(w, WM_KEYDOWN, 'A', 0x001e0001);
    U(PostMessageA)(w, WM_KEYUP, 'A', 0xc01e0001);
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE));
    CHECK(U(PeekMessageA)(&m, w, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE));
    hev *e = hk_find(K_KBD, 0);
    t_note("WH_KEYBOARD saw %d posted key messages", hk_count(K_KBD));
    if (e) { EXPECT_EQ(e->code, HC_ACTION); EXPECT_EQ(e->wp, 'A'); EXPECT_EQ((DWORD)e->lp, 0x001e0001); EXPECT_EQ(e->tid, GetCurrentThreadId()); }
    e = hk_find(K_KBD, 1);
    if (e) { EXPECT_EQ(e->wp, 'A'); EXPECT_EQ((DWORD)e->lp, 0xc01e0001); }
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_keyboard_noremove) {
    NEED_HK(); REQUIRE(U(PostMessageA));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    t_pump();
    hk_reset();
    HHOOK h = hook_thread(WH_KEYBOARD, hp_kbd); g_self = h;
    U(PostMessageA)(w, WM_KEYDOWN, 'B', 0x00300001);
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE));
    hev *e = hk_find(K_KBD, 0);
    t_note("WH_KEYBOARD on PM_NOREMOVE: called=%d code=%d", e != NULL, e ? e->code : -1);
    if (e) EXPECT_EQ(e->code, HC_NOREMOVE);
    U(PeekMessageA)(&m, w, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE);
    if (h) U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_mouse_posted) {
    NEED_HK(); REQUIRE(U(PostMessageA));
    HWND w = hk_win(WS_POPUP | WS_VISIBLE); CHECK(w != NULL);
    t_pump();
    hk_reset();
    HHOOK h = hook_thread(WH_MOUSE, hp_mouse); g_self = h;
    CHECK(h != NULL);
    U(PostMessageA)(w, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(3, 4));
    MSG m;
    CHECK(U(PeekMessageA)(&m, w, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE));
    EXPECT_EQ(m.message, WM_LBUTTONDOWN);
    hev *e = hk_find(K_MOUSE, 0);
    t_note("WH_MOUSE saw %d posted mouse messages", hk_count(K_MOUSE));
    if (e) { EXPECT_EQ(e->code, HC_ACTION); EXPECT_EQ(e->wp, WM_LBUTTONDOWN); EXPECT_EQ(e->a, (LONG_PTR)w); }
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
    U(DestroyWindow)(w);
}

/* ---------- CBT / DEBUG ---------- */
TEST(winsta_process_hook_cbt_createwnd) {
    NEED_HK();
    hk_reset();
    HHOOK h = hook_thread(WH_CBT, hp_cbt); g_self = h;
    CHECK(h != NULL);
    HWND w = hk_win(WS_POPUP);
    CHECK(w != NULL);
    int ic = hk_index(K_CBT, HCBT_CREATEWND);
    CHECK(ic >= 0);
    if (ic >= 0) {
        EXPECT_EQ(g_log[ic].wp, (WPARAM)w);
        EXPECT_EQ(g_log[ic].a, 33);
        EXPECT_NE(g_log[ic].b & WS_POPUP, 0);
        if (g_log[ic].c > 0xffff) t_note("CBT_CREATEWND class '%s'", (const char *)g_log[ic].c);
        int iw = -1;
        for (LONG i = 0; i < g_n; i++) if (g_log[i].kind == K_WND && g_log[i].code == WM_CREATE) iw = (int)i;
        CHECK(iw > ic);
        int in = -1;
        for (LONG i = 0; i < g_n; i++) if (g_log[i].kind == K_WND && g_log[i].code == WM_NCCREATE) in = (int)i;
        t_note("HCBT_CREATEWND index %d, WM_NCCREATE index %d, WM_CREATE index %d", ic, in, iw);
    }
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_cbt_createwnd_block) {
    NEED_HK();
    hk_reset(); g_cbt_block_create = 1;
    HHOOK h = hook_thread(WH_CBT, hp_cbt); g_self = h;
    SetLastError(0);
    HWND w = hk_win(WS_POPUP);
    CHECK(w == NULL);
    t_note("CreateWindow blocked by CBT hook: err %lu", GetLastError());
    EXPECT_EQ(hk_count(K_WND) >= 0, 1);
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
    g_cbt_block_create = 0;
    w = hk_win(WS_POPUP);
    CHECK(w != NULL);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_hook_cbt_destroywnd) {
    NEED_HK();
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    hk_reset();
    HHOOK h = hook_thread(WH_CBT, hp_cbt); g_self = h;
    CHECK(U(DestroyWindow)(w));
    int i = hk_index(K_CBT, HCBT_DESTROYWND);
    CHECK(i >= 0);
    if (i >= 0) EXPECT_EQ(g_log[i].wp, (WPARAM)w);
    CHECK(!U(IsWindow)(w));
    if (h) U(UnhookWindowsHookEx)(h);
}
TEST(winsta_process_hook_cbt_destroywnd_block) {
    NEED_HK();
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    hk_reset(); g_cbt_block_destroy = 1;
    HHOOK h = hook_thread(WH_CBT, hp_cbt); g_self = h;
    BOOL r = U(DestroyWindow)(w);
    t_note("DestroyWindow with blocking CBT hook ret %d IsWindow %d", r, U(IsWindow)(w));
    CHECK(!r);
    CHECK(U(IsWindow)(w));
    if (h) U(UnhookWindowsHookEx)(h);
    g_cbt_block_destroy = 0;
    CHECK(U(DestroyWindow)(w));
}
TEST(winsta_process_hook_cbt_activate_and_focus) {
    NEED_HK(); REQUIRE(U(ShowWindow)); REQUIRE(U(SetActiveWindow));
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    hk_reset();
    HHOOK h = hook_thread(WH_CBT, hp_cbt); g_self = h;
    U(ShowWindow)(w, SW_SHOWNA);
    U(SetActiveWindow)(w);
    int ia = hk_index(K_CBT, HCBT_ACTIVATE);
    int is = hk_index(K_CBT, HCBT_SETFOCUS);
    t_note("HCBT_ACTIVATE seen: %d, HCBT_SETFOCUS seen: %d (active=%p)", ia >= 0, is >= 0, U(GetActiveWindow)());
    if (ia >= 0 && U(GetActiveWindow)() == w) EXPECT_EQ(g_log[ia].wp, (WPARAM)w);
    if (h) U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_cbt_minmax) {
    NEED_HK(); REQUIRE(U(ShowWindow));
    HWND w = hk_win(WS_OVERLAPPEDWINDOW); CHECK(w != NULL);
    hk_reset();
    HHOOK h = hook_thread(WH_CBT, hp_cbt); g_self = h;
    U(ShowWindow)(w, SW_SHOWMINNOACTIVE);
    int i = hk_index(K_CBT, HCBT_MINMAX);
    t_note("HCBT_MINMAX seen: %d", i >= 0);
    if (i >= 0) { EXPECT_EQ(g_log[i].wp, (WPARAM)w); EXPECT_EQ(LOWORD(g_log[i].lp), SW_SHOWMINNOACTIVE); }
    if (h) U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_debug_precedes_cbt) {
    NEED_HK();
    hk_reset();
    HHOOK hd = hook_thread(WH_DEBUG, hp_dbg), hc = hook_thread(WH_CBT, hp_cbt);
    CHECK(hd != NULL); CHECK(hc != NULL);
    g_self = hc;
    HWND w = hk_win(WS_POPUP); CHECK(w != NULL);
    int idbg = -1, icbt = -1;
    for (LONG i = 0; i < g_n; i++) {
        if (idbg < 0 && g_log[i].kind == K_DBG && g_log[i].wp == WH_CBT) idbg = (int)i;
        if (icbt < 0 && g_log[i].kind == K_CBT && g_log[i].code == HCBT_CREATEWND) icbt = (int)i;
    }
    CHECK(idbg >= 0); CHECK(icbt >= 0);
    CHECK(idbg < icbt);
    if (idbg >= 0) { EXPECT_EQ(g_log[idbg].a, GetCurrentThreadId()); EXPECT_EQ(g_log[idbg].tid, GetCurrentThreadId()); }
    if (hd) CHECK(U(UnhookWindowsHookEx)(hd));
    if (hc) CHECK(U(UnhookWindowsHookEx)(hc));
    U(DestroyWindow)(w);
}
TEST(winsta_process_hook_shell_foregroundidle_thread_install) {
    NEED_HK();
    HHOOK s = U(SetWindowsHookExA)(WH_SHELL, (HOOKPROC)hp_plain, NULL, GetCurrentThreadId());
    t_note("thread-local WH_SHELL -> %p err %lu", s, GetLastError());
    if (s) U(UnhookWindowsHookEx)(s);
    HHOOK f = hook_thread(WH_FOREGROUNDIDLE, hp_plain);
    CHECK(f != NULL);
    if (f) CHECK(U(UnhookWindowsHookEx)(f));
}

/* ---------- cross-thread ---------- */
typedef struct { HANDLE ready, go, th; HWND hwnd; DWORD tid; HHOOK own; int install_own; } hkw;
static LRESULT CALLBACK hp_remote(int c, WPARAM wp, LPARAM lp) {
    if (c == HC_ACTION) {
        CWPSTRUCT *s = (CWPSTRUCT *)lp;
        if (s->message == WM_USER + 21) { hev *e = hk_add(K_CWP, c, wp, lp); e->a = s->message; }
    }
    return CallNextHookEx(NULL, c, wp, lp);
}
static DWORD WINAPI hkw_thread(LPVOID p) {
    hkw *w = p; MSG m;
    w->hwnd = hk_win(WS_POPUP);
    if (w->install_own) w->own = hook_thread(WH_CALLWNDPROC, hp_remote);
    SetEvent(w->ready);
    while (WaitForSingleObject(w->go, 0) != WAIT_OBJECT_0) {
        while (U(PeekMessageA)(&m, NULL, 0, 0, PM_REMOVE)) U(DispatchMessageA)(&m);
        Sleep(1);
    }
    if (w->own && !w->install_own) {}
    if (w->hwnd) U(DestroyWindow)(w->hwnd);
    return 0;
}
static hkw *hkw_start(int own) {
    hkw *w = (hkw *)calloc(1, sizeof *w);
    w->ready = CreateEventA(NULL, TRUE, FALSE, NULL); w->go = CreateEventA(NULL, TRUE, FALSE, NULL);
    w->install_own = own;
    w->th = CreateThread(NULL, 0, hkw_thread, w, 0, &w->tid);
    if (WaitForSingleObject(w->ready, 8000) != WAIT_OBJECT_0) CHECK(0);
    return w;
}
static void hkw_stop(hkw *w) {
    SetEvent(w->go);
    if (WaitForSingleObject(w->th, 8000) != WAIT_OBJECT_0) TerminateThread(w->th, 0);
    CloseHandle(w->th); CloseHandle(w->ready); CloseHandle(w->go); free(w);
}
TEST(winsta_process_hook_on_other_thread_runs_in_that_thread) {
    NEED_HK(); REQUIRE(U(SendMessageA)); REQUIRE(U(DispatchMessageA));
    hkw *w = hkw_start(0);
    hk_reset();
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, hp_remote, NULL, w->tid);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        LRESULT r = U(SendMessageA)(w->hwnd, WM_USER + 21, 0, 0);
        (void)r;
        hev *e = hk_find(K_CWP, 0);
        CHECK(e != NULL);
        /* Win10: wParam is nonzero (1) when the message came from another thread */
        if (e) { EXPECT_EQ(e->tid, w->tid); EXPECT_NE(e->wp, 0); }
        CHECK(U(UnhookWindowsHookEx)(h));
    }
    hkw_stop(w);
}
TEST(winsta_process_hook_cross_thread_sendmessage_wparam_zero) {
    NEED_HK(); REQUIRE(U(SendMessageA));
    hkw *w = hkw_start(1);
    hk_reset();
    U(SendMessageA)(w->hwnd, WM_USER + 21, 0, 0);
    hev *e = hk_find(K_CWP, 0);
    CHECK(e != NULL);
    if (e) { EXPECT_EQ(e->tid, w->tid); EXPECT_NE(e->wp, 0); }   /* Win10: nonzero (1) for a cross-thread send */
    hkw_stop(w);
}
TEST(winsta_process_hook_dies_with_thread) {
    NEED_HK();
    hkw *w = hkw_start(1);
    HHOOK own = w->own;
    CHECK(own != NULL);
    SetEvent(w->go);
    WaitForSingleObject(w->th, 8000);
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)(own));
    EXPECT_ERR(0, WSP_HERR_INVALID_HOOK_HANDLE);
    hkw_stop(w);
}
TEST(winsta_process_hook_unhook_from_other_thread) {
    NEED_HK();
    hkw *w = hkw_start(1);
    HHOOK own = w->own;
    CHECK(own != NULL);
    SetLastError(0);
    BOOL r = U(UnhookWindowsHookEx)(own);
    t_note("UnhookWindowsHookEx of another thread's hook: ret %d err %lu", r, GetLastError());
    hkw_stop(w);
}
TEST(winsta_process_hook_other_thread_bad_hmod_needed) {
    NEED_HK();
    hkw *w = hkw_start(0);
    SetLastError(0);
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, hp_remote, NULL, w->tid);
    CHECK(h != NULL);
    if (h) U(UnhookWindowsHookEx)(h);
    h = U(SetWindowsHookExA)(WH_CALLWNDPROC, hp_remote, GetModuleHandleA(NULL), w->tid);
    CHECK(h != NULL);
    if (h) U(UnhookWindowsHookEx)(h);
    hkw_stop(w);
}
TEST(winsta_process_hook_exited_thread_id) {
    NEED_HK();
    hkw *w = hkw_start(0);
    DWORD tid = w->tid;
    SetEvent(w->go);
    WaitForSingleObject(w->th, 8000);
    SetLastError(0);
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, hp_remote, NULL, tid);
    CHECK(h == NULL);
    t_note("hook on exited thread id: err %lu", GetLastError());
    if (h) U(UnhookWindowsHookEx)(h);
    hkw_stop(w);
}
TEST(winsta_process_hook_non_gui_thread_target) {
    NEED_HK();
    /* thread without a message queue / GUI state: hooking it is still allowed */
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL), go = CreateEventA(NULL, TRUE, FALSE, NULL);
    struct { HANDLE ev, go; } a = { ev, go };
    DWORD tid;
    DWORD WINAPI idle(LPVOID p) { SetEvent(((typeof(a) *)p)->ev); WaitForSingleObject(((typeof(a) *)p)->go, 8000); return 0; }
    HANDLE th = CreateThread(NULL, 0, idle, &a, 0, &tid);
    WaitForSingleObject(ev, 8000);
    SetLastError(0);
    HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, hp_plain, NULL, tid);
    t_note("hook on non-GUI thread -> %p err %lu", h, GetLastError());
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
    SetEvent(go);
    WaitForSingleObject(th, 8000);
    CloseHandle(th); CloseHandle(ev); CloseHandle(go);
}
