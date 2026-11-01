/* WH_CBT codes and veto behaviour, WH_CALLWNDPROC / WH_CALLWNDPROCRET, modal-loop WH_MSGFILTER. */
#include "dh.h"

static const char *KCLS = "T32CbtWnd";
static volatile LONG kproc_calls;
static volatile UINT k_last_msg; static volatile WPARAM k_last_wp; static volatile LPARAM k_last_lp;
static volatile LONG k_seq, k_proc_seq, k_hook_seq;
static LRESULT CALLBACK kwnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 1) { k_proc_seq = ++k_seq; k_last_wp = w; k_last_lp = l; return 0x1234; }
    if (m == WM_USER + 2) { k_proc_seq = ++k_seq; return 7; }
    return DefWindowProcA(h, m, w, l);
}
static void kreg(void) {
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = kwnd_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = KCLS;
    U(RegisterClassA)(&wc);
}
static void kunreg(void) { U(UnregisterClassA)(KCLS, GetModuleHandleA(NULL)); }
static HWND kmk(DWORD style, int x, int y, int w, int h, HWND parent) {
    return U(CreateWindowExA)(0, KCLS, "t32cbt", style, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
static int kneed(void) {
    return U(SetWindowsHookExA) && U(SetWindowsHookExW) && U(UnhookWindowsHookEx) && U(CallNextHookEx) && U(RegisterClassA) &&
           U(CreateWindowExA) && U(DestroyWindow) && U(UnregisterClassA) && U(ShowWindow) && U(SendMessageA);
}
#define KNEED() do { if (!kneed()) { t_skip("missing exports"); return; } kreg(); } while (0)
#define KDONE() kunreg()

#define MAXC 128
static struct { int n; int code[MAXC]; WPARAM wp[MAXC]; LPARAM lp[MAXC]; } cbt;
static int cbt_veto_code, cbt_veto_all_zero;
static HWND cbt_veto_hwnd;
typedef struct { HWND h; CREATESTRUCTA cs; char name[64]; HWND after; } CREC;
static CREC crec[16]; static int ncrec;
static char cbt_veto_name[32];            /* veto HCBT_CREATEWND only for windows with this title */
static int cbt_modify;
static void cbt_reset(void) { memset(&cbt, 0, sizeof cbt); cbt_veto_code = -1; cbt_veto_hwnd = NULL; cbt_modify = 0; memset(crec, 0, sizeof crec); ncrec = 0; cbt_veto_name[0] = 0; }
static CREC *crec_for(HWND w) { for (int i = 0; i < ncrec; i++) if (crec[i].h == w) return &crec[i]; return NULL; }
static int cdestroyed(HWND w) { int n = 0; for (int i = 0; i < cbt.n; i++) if (cbt.code[i] == HCBT_DESTROYWND && cbt.wp[i] == (WPARAM)w) n++; return n; }
static int ccount(int code) { int n = 0; for (int i = 0; i < cbt.n; i++) if (cbt.code[i] == code) n++; return n; }
static int cfind(int code, WPARAM w) { for (int i = 0; i < cbt.n; i++) if (cbt.code[i] == code && cbt.wp[i] == w) return i; return -1; }
static LRESULT CALLBACK cbt_proc(int c, WPARAM w, LPARAM l) {
    if (c >= 0 && cbt.n < MAXC) { cbt.code[cbt.n] = c; cbt.wp[cbt.n] = w; cbt.lp[cbt.n] = l; cbt.n++; }
    if (c == HCBT_CREATEWND) {
        CBT_CREATEWNDA *cw = (CBT_CREATEWNDA *)l;
        char nm[64] = "";
        if (!IS_INTRESOURCE(cw->lpcs->lpszName) && cw->lpcs->lpszName) lstrcpynA(nm, cw->lpcs->lpszName, sizeof nm);
        if (ncrec < 16) { CREC *r = &crec[ncrec++]; r->h = (HWND)w; r->cs = *cw->lpcs; r->after = cw->hwndInsertAfter; lstrcpyA(r->name, nm); }
        if (cbt_modify && !lstrcmpA(nm, "t32cbt")) { cw->lpcs->x = 301; cw->lpcs->y = 202; cw->lpcs->cx = 111; cw->lpcs->cy = 99; }
        if (c == cbt_veto_code && cbt_veto_name[0] && !lstrcmpA(nm, cbt_veto_name)) return 1;
        return U(CallNextHookEx)(NULL, c, w, l);
    }
    if (c == cbt_veto_code && c != HCBT_CREATEWND && (!cbt_veto_hwnd || (HWND)w == cbt_veto_hwnd)) return 1;
    return U(CallNextHookEx)(NULL, c, w, l);
}
static HHOOK cbt_install(void) { cbt_reset(); return U(SetWindowsHookExA)(WH_CBT, cbt_proc, NULL, GetCurrentThreadId()); }

TEST(cbt_createwnd_reports_fields) {
    KNEED();
    HHOOK h = cbt_install();
    CHECK(h != NULL);
    HWND w = U(CreateWindowExA)(WS_EX_TOOLWINDOW, KCLS, "t32cbtname", WS_POPUP, 11, 22, 133, 144, NULL, NULL, GetModuleHandleA(NULL), (LPVOID)0x5151);
    U(UnhookWindowsHookEx)(h);
    CHECK(w != NULL);
    CREC *r = crec_for(w);
    CHECKF(r != NULL, "hook saw the new window handle");
    if (r) {
        EXPECT_STR(r->name, "t32cbtname");
        EQ(r->cs.x, 11); EQ(r->cs.y, 22); EQ(r->cs.cx, 133); EQ(r->cs.cy, 144);
        CHECK((r->cs.style & WS_POPUP) != 0);
        EQ(r->cs.dwExStyle & WS_EX_TOOLWINDOW, WS_EX_TOOLWINDOW);
        EQ(r->cs.hInstance, GetModuleHandleA(NULL));
        EQ(r->cs.lpCreateParams, (LPVOID)0x5151);
        EQ(r->cs.hwndParent, NULL);
        t_note("hwndInsertAfter = %p", r->after);
    }
    t_note("%d window creations observed for one CreateWindowEx", ncrec);
    if (w) U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_createwnd_child_fields) {
    KNEED();
    HWND parent = kmk(WS_POPUP, 0, 0, 50, 50, NULL);
    HHOOK h = cbt_install();
    HWND c = U(CreateWindowExA)(0, KCLS, "child", WS_CHILD, 3, 4, 20, 21, parent, (HMENU)(ULONG_PTR)77, GetModuleHandleA(NULL), NULL);
    U(UnhookWindowsHookEx)(h);
    CHECK(c != NULL);
    CREC *r = crec_for(c);
    CHECK(r != NULL);
    if (r) {
        EQ(r->cs.hwndParent, parent);
        CHECK((r->cs.style & WS_CHILD) != 0);
        EQ(r->cs.hMenu, (HMENU)(ULONG_PTR)77);
        EQ(r->cs.x, 3); EQ(r->cs.y, 4); EQ(r->cs.cx, 20); EQ(r->cs.cy, 21);
    }
    if (c) U(DestroyWindow)(c);
    U(DestroyWindow)(parent);
    KDONE();
}
TEST(cbt_createwnd_veto) {
    KNEED();
    HHOOK h = cbt_install();
    cbt_veto_code = HCBT_CREATEWND;
    lstrcpyA(cbt_veto_name, "t32cbt");
    SetLastError(0);
    HWND w = kmk(WS_POPUP, 0, 0, 40, 40, NULL);
    DWORD err = GetLastError();
    EQ(w, NULL);
    t_note("CreateWindowEx vetoed by CBT: err=%lu", err);
    CHECK(ccount(HCBT_CREATEWND) >= 1);
    U(UnhookWindowsHookEx)(h);
    /* creation works again afterwards */
    w = kmk(WS_POPUP, 0, 0, 40, 40, NULL);
    CHECK(w != NULL);
    if (w) U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_createwnd_veto_child_only) {
    KNEED();
    HWND parent = kmk(WS_POPUP, 0, 0, 50, 50, NULL);
    HHOOK h = cbt_install();
    cbt_veto_code = HCBT_CREATEWND;
    lstrcpyA(cbt_veto_name, "child");
    HWND c = U(CreateWindowExA)(0, KCLS, "child", WS_CHILD, 0, 0, 10, 10, parent, NULL, GetModuleHandleA(NULL), NULL);
    EQ(c, NULL);
    U(UnhookWindowsHookEx)(h);
    CHECK(IsWindow(parent));
    U(DestroyWindow)(parent);
    KDONE();
}
TEST(cbt_createwnd_modify_observed) {
    KNEED();
    HHOOK h = cbt_install();
    cbt_modify = 1;
    HWND w = kmk(WS_POPUP, 10, 10, 50, 50, NULL);
    U(UnhookWindowsHookEx)(h);
    CHECK(w != NULL);
    if (w) {
        RECT r; GetWindowRect(w, &r);
        t_note("rect after CBT modification: %ld,%ld %ldx%ld (hook asked for 301,202 111x99)", r.left, r.top, r.right - r.left, r.bottom - r.top);
        U(DestroyWindow)(w);
    }
    KDONE();
}
TEST(cbt_destroywnd_reports) {
    KNEED();
    HWND w = kmk(WS_POPUP, 0, 0, 30, 30, NULL);
    HHOOK h = cbt_install();
    EQ(U(DestroyWindow)(w), TRUE);
    U(UnhookWindowsHookEx)(h);
    EQ(cdestroyed(w), 1);
    CHECK(!IsWindow(w));
    KDONE();
}
TEST(cbt_destroywnd_veto) {
    KNEED();
    HWND w = kmk(WS_POPUP, 0, 0, 30, 30, NULL);
    HHOOK h = cbt_install();
    cbt_veto_code = HCBT_DESTROYWND; cbt_veto_hwnd = w;
    BOOL r = U(DestroyWindow)(w);
    t_note("DestroyWindow vetoed -> %d", r);
    EQ(r, FALSE);
    CHECK(IsWindow(w));
    EQ(cdestroyed(w), 1);
    U(UnhookWindowsHookEx)(h);
    EQ(U(DestroyWindow)(w), TRUE);
    CHECK(!IsWindow(w));
    KDONE();
}
TEST(cbt_destroywnd_child_and_parent_order) {
    KNEED();
    HWND p = kmk(WS_POPUP, 0, 0, 40, 40, NULL);
    HWND c = U(CreateWindowExA)(0, KCLS, "child", WS_CHILD, 0, 0, 10, 10, p, NULL, GetModuleHandleA(NULL), NULL);
    HHOOK h = cbt_install();
    U(DestroyWindow)(p);
    U(UnhookWindowsHookEx)(h);
    EQ(cdestroyed(p), 1);
    int ip = cfind(HCBT_DESTROYWND, (WPARAM)p), ic = cfind(HCBT_DESTROYWND, (WPARAM)c);
    t_note("child HCBT_DESTROYWND count %d (positions parent %d child %d)", cdestroyed(c), ip, ic);
    if (ic >= 0) CHECK(ip < ic);
    KDONE();
}
TEST(cbt_hook_is_thread_local) {
    KNEED();
    static volatile LONG done;
    DWORD WINAPI th(LPVOID p) {
        HWND w = U(CreateWindowExA)(0, KCLS, "other", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
        if (w) U(DestroyWindow)(w);
        done = 1;
        return 0;
    }
    HHOOK h = cbt_install();
    done = 0;
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    U(UnhookWindowsHookEx)(h);
    CHECK(done);
    EQ(cbt.n, 0);
    KDONE();
}
TEST(cbt_veto_nonzero_values) {
    KNEED();
    static volatile LRESULT ret;
    LRESULT CALLBACK p(int c, WPARAM w, LPARAM l) {
        if (c == HCBT_CREATEWND) {
            CBT_CREATEWNDA *cw = (CBT_CREATEWNDA *)l;
            if (cw->lpcs->lpszName && !IS_INTRESOURCE(cw->lpcs->lpszName) && !lstrcmpA(cw->lpcs->lpszName, "t32cbt")) return ret;
        }
        return U(CallNextHookEx)(NULL, c, w, l);
    }
    HHOOK h = U(SetWindowsHookExA)(WH_CBT, p, NULL, GetCurrentThreadId());
    static const LRESULT rets[] = {0, 1, -1, 2, 0x100, 0x7FFFFFFF, (LRESULT)0x100000000ll};
    for (unsigned i = 0; i < sizeof rets / sizeof rets[0]; i++) {
        ret = rets[i];
        HWND w = kmk(WS_POPUP, 0, 0, 10, 10, NULL);
        t_note("CBT_CREATEWND hook returned 0x%llx -> window %s", (unsigned long long)rets[i], w ? "created" : "refused");
        if (rets[i] == 0) CHECK(w != NULL);
        if (rets[i] == 1 || rets[i] == -1) CHECKF(w == NULL, "return %lld", (long long)rets[i]);
        if (w) U(DestroyWindow)(w);
    }
    U(UnhookWindowsHookEx)(h);
    KDONE();
}
TEST(cbt_activate_and_setfocus_observed) {
    KNEED();
    HHOOK h = cbt_install();
    HWND w = kmk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, -30000, -30000, 80, 60, NULL);
    CHECK(w != NULL);
    HWND prev = SetActiveWindow(w);
    SetFocus(w);
    U(UnhookWindowsHookEx)(h);
    t_note("HCBT_ACTIVATE x%d, HCBT_SETFOCUS x%d, previous active %p, now active %p", ccount(HCBT_ACTIVATE), ccount(HCBT_SETFOCUS), prev, GetActiveWindow());
    int i = cfind(HCBT_ACTIVATE, (WPARAM)w);
    if (i >= 0) {
        CBTACTIVATESTRUCT *a = (CBTACTIVATESTRUCT *)cbt.lp[i];
        (void)a;
    }
    if (w) U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_activate_veto_observed) {
    KNEED();
    HWND w = kmk(WS_OVERLAPPEDWINDOW, -30000, -30000, 80, 60, NULL);
    HHOOK h = cbt_install();
    cbt_veto_code = HCBT_ACTIVATE; cbt_veto_hwnd = w;
    U(ShowWindow)(w, SW_SHOWNORMAL);
    SetActiveWindow(w);
    HWND act = GetActiveWindow();
    U(UnhookWindowsHookEx)(h);
    t_note("with HCBT_ACTIVATE veto: %d activation hooks, active window %s", ccount(HCBT_ACTIVATE), act == w ? "is ours" : "is not ours");
    if (ccount(HCBT_ACTIVATE) > 0) CHECK(act != w);
    U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_setfocus_veto_observed) {
    KNEED();
    HWND p = kmk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, -30000, -30000, 80, 60, NULL);
    HWND c1 = U(CreateWindowExA)(0, KCLS, "c1", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 10, 10, p, NULL, GetModuleHandleA(NULL), NULL);
    HWND c2 = U(CreateWindowExA)(0, KCLS, "c2", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 20, 0, 10, 10, p, NULL, GetModuleHandleA(NULL), NULL);
    SetActiveWindow(p);
    SetFocus(c1);
    HWND before = GetFocus();
    HHOOK h = cbt_install();
    cbt_veto_code = HCBT_SETFOCUS; cbt_veto_hwnd = c2;
    SetFocus(c2);
    HWND after = GetFocus();
    U(UnhookWindowsHookEx)(h);
    t_note("focus before=%p (c1=%p) after veto=%p (c2=%p); HCBT_SETFOCUS x%d", before, c1, after, c2, ccount(HCBT_SETFOCUS));
    if (before == c1 && ccount(HCBT_SETFOCUS) > 0) CHECK(after != c2);
    U(DestroyWindow)(p);
    KDONE();
}
TEST(cbt_syscommand_veto) {
    KNEED();
    HWND w = kmk(WS_OVERLAPPEDWINDOW, -30000, -30000, 120, 90, NULL);
    U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    HHOOK h = cbt_install();
    cbt_veto_code = HCBT_SYSCOMMAND; cbt_veto_hwnd = w;
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    U(UnhookWindowsHookEx)(h);
    CHECK(ccount(HCBT_SYSCOMMAND) >= 1);
    int i = cfind(HCBT_SYSCOMMAND, SC_MINIMIZE);
    CHECKF(i >= 0, "hook saw SC_MINIMIZE");
    /* Win10 22H2 oracle: a non-zero HCBT_SYSCOMMAND result does not stop a
     * WM_SYSCOMMAND that is sent directly to the window; the window is
     * minimized anyway. The outcome is recorded, not asserted. */
    t_note("HCBT_SYSCOMMAND veto on sent SC_MINIMIZE: iconic=%d", IsIconic(w) ? 1 : 0);
    U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_syscommand_allowed) {
    KNEED();
    HWND w = kmk(WS_OVERLAPPEDWINDOW, -30000, -30000, 120, 90, NULL);
    U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    HHOOK h = cbt_install();
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    EQ(IsIconic(w), TRUE);
    int n = ccount(HCBT_SYSCOMMAND);
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_RESTORE, 0);
    U(UnhookWindowsHookEx)(h);
    CHECK(n >= 1);
    CHECK(ccount(HCBT_SYSCOMMAND) >= 2);
    CHECK(cfind(HCBT_SYSCOMMAND, SC_RESTORE) >= 0);
    CHECK(!IsIconic(w));
    U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_syscommand_low_bits_masked) {
    KNEED();
    HWND w = kmk(WS_OVERLAPPEDWINDOW, -30000, -30000, 120, 90, NULL);
    U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    HHOOK h = cbt_install();
    cbt_veto_code = HCBT_SYSCOMMAND; cbt_veto_hwnd = w;
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_MAXIMIZE | 0x2, 0);
    U(UnhookWindowsHookEx)(h);
    int i = -1;
    for (int k = 0; k < cbt.n; k++) if (cbt.code[k] == HCBT_SYSCOMMAND) i = k;
    CHECK(i >= 0);
    if (i >= 0) t_note("HCBT_SYSCOMMAND wParam for SC_MAXIMIZE|2 = 0x%llx", (unsigned long long)cbt.wp[i]);
    /* same oracle behaviour as cbt_syscommand_veto: the veto is not honoured
     * for a directly sent WM_SYSCOMMAND; recorded, not asserted */
    t_note("HCBT_SYSCOMMAND veto on sent SC_MAXIMIZE|2: zoomed=%d", IsZoomed(w) ? 1 : 0);
    U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_minmax_observed) {
    KNEED();
    HWND w = kmk(WS_OVERLAPPEDWINDOW, -30000, -30000, 120, 90, NULL);
    U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    HHOOK h = cbt_install();
    U(ShowWindow)(w, SW_MINIMIZE);
    int minmax = ccount(HCBT_MINMAX);
    int i = cfind(HCBT_MINMAX, (WPARAM)w);
    t_note("ShowWindow(SW_MINIMIZE): HCBT_MINMAX x%d, lParam of first=0x%llx", minmax, i >= 0 ? (unsigned long long)cbt.lp[i] : 0ull);
    if (i >= 0) EQ(LOWORD(cbt.lp[i]), SW_MINIMIZE);
    U(ShowWindow)(w, SW_RESTORE);
    cbt_veto_code = HCBT_MINMAX; cbt_veto_hwnd = w;
    U(ShowWindow)(w, SW_MAXIMIZE);
    t_note("with HCBT_MINMAX veto: IsZoomed=%d", IsZoomed(w));
    U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_movesize_observed) {
    KNEED();
    HWND w = kmk(WS_OVERLAPPEDWINDOW, -30000, -30000, 120, 90, NULL);
    U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    HHOOK h = cbt_install();
    MoveWindow(w, -30000, -30000, 140, 100, TRUE);
    SetWindowPos(w, NULL, -29990, -29990, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    U(UnhookWindowsHookEx)(h);
    t_note("MoveWindow/SetWindowPos: HCBT_MOVESIZE x%d", ccount(HCBT_MOVESIZE));
    U(DestroyWindow)(w);
    KDONE();
}
TEST(cbt_codes_constants) {
    EQ(HCBT_MOVESIZE, 0); EQ(HCBT_MINMAX, 1); EQ(HCBT_QS, 2); EQ(HCBT_CREATEWND, 3); EQ(HCBT_DESTROYWND, 4);
    EQ(HCBT_ACTIVATE, 5); EQ(HCBT_CLICKSKIPPED, 6); EQ(HCBT_KEYSKIPPED, 7); EQ(HCBT_SYSCOMMAND, 8); EQ(HCBT_SETFOCUS, 9);
    EQ(WH_CBT, 5); EQ(WH_MSGFILTER, -1); EQ(WH_KEYBOARD_LL, 13); EQ(WH_MOUSE_LL, 14); EQ(WH_MAX, 14);
}
TEST(cbt_unicode_window_ansi_hook) {
    KNEED();
    REQUIRE(U(CreateWindowExW)); REQUIRE(U(RegisterClassW));
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = kwnd_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = L"T32CbtWide";
    U(RegisterClassW)(&wc);
    HHOOK h = cbt_install();
    HWND w = U(CreateWindowExW)(0, L"T32CbtWide", L"wide-name", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    U(UnhookWindowsHookEx)(h);
    CHECK(w != NULL);
    CREC *r = crec_for(w);
    CHECK(r != NULL);
    if (r) {
        t_note("ANSI CBT hook, W window: lpszName seen as \"%s\"", r->name);
        EXPECT_STR(r->name, "wide-name");
    }
    if (w) U(DestroyWindow)(w);
    U(UnregisterClassW)(L"T32CbtWide", GetModuleHandleA(NULL));
    KDONE();
}

/* ---------- WH_CALLWNDPROC / WH_CALLWNDPROCRET ---------- */
static struct { int n; CWPSTRUCT cwp[32]; } cw;
static struct { int n; CWPRETSTRUCT cwp[32]; } cwr;
static volatile LONG cw_seq[32];
static LRESULT CALLBACK cw_proc(int c, WPARAM w, LPARAM l) {
    if (c == HC_ACTION) {
        CWPSTRUCT *p = (CWPSTRUCT *)l;
        if (p->message == WM_USER + 1 || p->message == WM_USER + 2) {
            if (cw.n < 32) { cw.cwp[cw.n] = *p; cw_seq[cw.n] = ++k_seq; cw.n++; }
            k_hook_seq = k_seq;
            k_last_msg = (UINT)w;      /* the sent-by-this-thread flag */
        }
    }
    return U(CallNextHookEx)(NULL, c, w, l);
}
static volatile WPARAM cw_flag[32];
static LRESULT CALLBACK cw_proc2(int c, WPARAM w, LPARAM l) {
    if (c == HC_ACTION) {
        CWPSTRUCT *p = (CWPSTRUCT *)l;
        if (p->message == WM_USER + 1 && cw.n < 32) { cw_flag[cw.n] = w; cw.cwp[cw.n] = *p; cw.n++; }
    }
    return U(CallNextHookEx)(NULL, c, w, l);
}
static LRESULT CALLBACK cwr_proc(int c, WPARAM w, LPARAM l) {
    if (c == HC_ACTION) {
        CWPRETSTRUCT *p = (CWPRETSTRUCT *)l;
        if ((p->message == WM_USER + 1 || p->message == WM_USER + 2) && cwr.n < 32) cwr.cwp[cwr.n++] = *p;
    }
    return U(CallNextHookEx)(NULL, c, w, l);
}
TEST(callwndproc_cwpstruct_contents) {
    KNEED();
    HWND w = kmk(WS_POPUP, 0, 0, 20, 20, NULL);
    memset(&cw, 0, sizeof cw); k_seq = 0;
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, cw_proc, NULL, GetCurrentThreadId());
    CHECK(h != NULL);
    LRESULT r = U(SendMessageA)(w, WM_USER + 1, 0x11, 0x22);
    U(UnhookWindowsHookEx)(h);
    EQ(r, 0x1234);
    EQ(cw.n, 1);
    if (cw.n == 1) {
        EQ(cw.cwp[0].hwnd, w);
        EQ(cw.cwp[0].message, WM_USER + 1);
        EQ(cw.cwp[0].wParam, 0x11);
        EQ(cw.cwp[0].lParam, 0x22);
    }
    /* Win10 22H2 oracle: wParam of the WH_CALLWNDPROC hook for a message sent
     * by the current thread is 0 (the documented "nonzero" flag is not
     * observed); recorded, not asserted. */
    t_note("CALLWNDPROC wParam for a same-thread send: %u", (unsigned)k_last_msg);
    CHECK(cw_seq[0] < k_proc_seq);              /* hook runs before the window procedure */
    U(DestroyWindow)(w);
    KDONE();
}
TEST(callwndproc_pointer_sized_params) {
    KNEED();
    HWND w = kmk(WS_POPUP, 0, 0, 20, 20, NULL);
    memset(&cw, 0, sizeof cw);
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, cw_proc2, NULL, GetCurrentThreadId());
    U(SendMessageA)(w, WM_USER + 1, (WPARAM)0x123456789ABCDEF0ull, (LPARAM)0x0FEDCBA987654321ll);
    U(UnhookWindowsHookEx)(h);
    EQ(cw.n, 1);
    if (cw.n == 1) { EQ(cw.cwp[0].wParam, 0x123456789ABCDEF0ull); EQ(cw.cwp[0].lParam, 0x0FEDCBA987654321ll); }
    EQ(k_last_wp, 0x123456789ABCDEF0ull);
    EQ(k_last_lp, 0x0FEDCBA987654321ll);
    U(DestroyWindow)(w);
    KDONE();
}
TEST(callwndproc_not_for_posted_until_dispatch) {
    KNEED();
    HWND w = kmk(WS_POPUP, 0, 0, 20, 20, NULL);
    memset(&cw, 0, sizeof cw);
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, cw_proc2, NULL, GetCurrentThreadId());
    PostMessageA(w, WM_USER + 1, 5, 6);
    EQ(cw.n, 0);                                 /* posting alone never calls the hook */
    MSG m;
    PeekMessageA(&m, w, WM_USER, WM_USER + 10, PM_REMOVE);
    EQ(cw.n, 0);                                 /* neither does retrieval */
    DispatchMessageA(&m);
    t_note("CALLWNDPROC calls after DispatchMessage of a posted message: %d (flag %llu)", cw.n, cw.n ? (unsigned long long)cw_flag[0] : 0ull);
    U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
    KDONE();
}
TEST(callwndproc_cross_thread_send_flag) {
    KNEED();
    static HWND tw; static volatile LONG fin; static volatile LRESULT sres;
    DWORD WINAPI th(LPVOID p) {
        DWORD_PTR res = 0;
        sres = U(SendMessageTimeoutA)(tw, WM_USER + 1, 1, 2, SMTO_NORMAL, 4000, &res);
        k_last_wp = (WPARAM)res;
        fin = 1;
        return 0;
    }
    REQUIRE(U(SendMessageTimeoutA));
    tw = kmk(WS_POPUP, 0, 0, 20, 20, NULL);
    memset(&cw, 0, sizeof cw); fin = 0; sres = 0;
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, cw_proc2, NULL, GetCurrentThreadId());
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    CHECK(dh_pump_until(&fin, 5000));
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    U(UnhookWindowsHookEx)(h);
    EQ(sres, 1);
    EQ(cw.n, 1);
    if (cw.n == 1) { t_note("CALLWNDPROC wParam for a cross-thread send: %llu (Win10 22H2 oracle: 1)", (unsigned long long)cw_flag[0]); EQ(cw.cwp[0].hwnd, tw); EQ(cw.cwp[0].wParam, 1); EQ(cw.cwp[0].lParam, 2); }
    U(DestroyWindow)(tw);
    KDONE();
}
TEST(callwndprocret_result) {
    KNEED();
    HWND w = kmk(WS_POPUP, 0, 0, 20, 20, NULL);
    memset(&cwr, 0, sizeof cwr);
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROCRET, cwr_proc, NULL, GetCurrentThreadId());
    CHECK(h != NULL);
    LRESULT r1 = U(SendMessageA)(w, WM_USER + 1, 0x31, 0x32);
    LRESULT r2 = U(SendMessageA)(w, WM_USER + 2, 0x41, 0x42);
    U(UnhookWindowsHookEx)(h);
    EQ(r1, 0x1234); EQ(r2, 7);
    EQ(cwr.n, 2);
    if (cwr.n == 2) {
        EQ(cwr.cwp[0].lResult, 0x1234); EQ(cwr.cwp[0].message, WM_USER + 1); EQ(cwr.cwp[0].wParam, 0x31); EQ(cwr.cwp[0].lParam, 0x32); EQ(cwr.cwp[0].hwnd, w);
        EQ(cwr.cwp[1].lResult, 7); EQ(cwr.cwp[1].message, WM_USER + 2); EQ(cwr.cwp[1].wParam, 0x41); EQ(cwr.cwp[1].lParam, 0x42);
    }
    U(DestroyWindow)(w);
    KDONE();
}
TEST(callwndproc_and_ret_ordering) {
    KNEED();
    HWND w = kmk(WS_POPUP, 0, 0, 20, 20, NULL);
    static int ord[3]; static int no;
    LRESULT CALLBACK a(int c, WPARAM wp, LPARAM l) { if (c == HC_ACTION && ((CWPSTRUCT *)l)->message == WM_USER + 2) ord[no++] = 1; return U(CallNextHookEx)(NULL, c, wp, l); }
    LRESULT CALLBACK b(int c, WPARAM wp, LPARAM l) { if (c == HC_ACTION && ((CWPRETSTRUCT *)l)->message == WM_USER + 2) ord[no++] = 3; return U(CallNextHookEx)(NULL, c, wp, l); }
    no = 0; k_seq = 0; k_proc_seq = 0;
    HHOOK h1 = U(SetWindowsHookExA)(WH_CALLWNDPROC, a, NULL, GetCurrentThreadId());
    HHOOK h2 = U(SetWindowsHookExA)(WH_CALLWNDPROCRET, b, NULL, GetCurrentThreadId());
    U(SendMessageA)(w, WM_USER + 2, 0, 0);
    U(UnhookWindowsHookEx)(h2); U(UnhookWindowsHookEx)(h1);
    EQ(no, 2);
    if (no == 2) { EQ(ord[0], 1); EQ(ord[1], 3); }
    U(DestroyWindow)(w);
    KDONE();
}
TEST(callwndproc_unicode_text_string) {
    KNEED();
    REQUIRE(U(SendMessageW));
    HWND w = kmk(WS_POPUP, 0, 0, 20, 20, NULL);
    static char seen[16]; static volatile int kind;
    LRESULT CALLBACK pa(int c, WPARAM wp, LPARAM l) {
        CWPSTRUCT *p = (CWPSTRUCT *)l;
        if (c == HC_ACTION && p->message == WM_SETTEXT) { memcpy(seen, (void *)p->lParam, 8); seen[8] = 0; kind = 1; }
        return U(CallNextHookEx)(NULL, c, wp, l);
    }
    memset(seen, 0, sizeof seen); kind = 0;
    HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, pa, NULL, GetCurrentThreadId());
    U(SendMessageW)(w, WM_SETTEXT, 0, (LPARAM)L"abcd");
    U(UnhookWindowsHookEx)(h);
    CHECK(kind == 1);
    t_note("ANSI WH_CALLWNDPROC saw WM_SETTEXT bytes: %02x %02x %02x %02x", (unsigned char)seen[0], (unsigned char)seen[1], (unsigned char)seen[2], (unsigned char)seen[3]);
    U(DestroyWindow)(w);
    KDONE();
}

/* ---------- modal loops ---------- */
static volatile LONG msgf_menu, msgf_dlg, msgf_msgbox, msgf_other, msgf_scroll;
static LRESULT CALLBACK modal_filter(int c, WPARAM w, LPARAM l) {
    if (c == MSGF_MENU) msgf_menu++;
    else if (c == MSGF_DIALOGBOX) msgf_dlg++;
    else if (c == MSGF_MESSAGEBOX) msgf_msgbox++;
    else if (c == MSGF_SCROLLBAR) msgf_scroll++;
    else msgf_other++;
    return U(CallNextHookEx)(NULL, c, w, l);
}
static HWND g_menu_wnd;
static volatile LONG menu_timer_fired;
static LRESULT CALLBACK menu_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TIMER && w == 91) { menu_timer_fired++; KillTimer(h, 91); EndMenu(); return 0; }
    return DefWindowProcA(h, m, w, l);
}
static DWORD WINAPI watchdog(LPVOID p) {
    Sleep(4000);
    HWND h = (HWND)p;
    if (IsWindow(h)) { PostMessageA(h, WM_CANCELMODE, 0, 0); PostMessageA(h, WM_COMMAND, IDCANCEL, 0); PostMessageA(h, WM_CLOSE, 0, 0); }
    return 0;
}
TEST(msgfilter_menu_loop) {
    KNEED();
    REQUIRE(U(CreatePopupMenu)); REQUIRE(U(AppendMenuA)); REQUIRE(U(TrackPopupMenu)); REQUIRE(U(DestroyMenu)); REQUIRE(U(EndMenu));
    UnregisterClassA("T32MenuWnd", GetModuleHandleA(NULL));
    HWND w = dh_create_window("T32MenuWnd", menu_wnd_proc, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, NULL);
    CHECK(w != NULL);
    HMENU m = U(CreatePopupMenu)();
    U(AppendMenuA)(m, MF_STRING, 1001, "one");
    U(AppendMenuA)(m, MF_STRING, 1002, "two");
    msgf_menu = msgf_dlg = msgf_other = 0; menu_timer_fired = 0;
    HHOOK h = U(SetWindowsHookExA)(WH_MSGFILTER, modal_filter, NULL, GetCurrentThreadId());
    CHECK(h != NULL);
    HANDLE wd = CreateThread(NULL, 0, watchdog, w, 0, NULL);
    SetTimer(w, 91, 150, NULL);
    SetForegroundWindow(w);
    int cmd = U(TrackPopupMenu)(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, 0, 0, 0, w, NULL);
    U(UnhookWindowsHookEx)(h);
    KillTimer(w, 91);
    EQ(cmd, 0);
    t_note("menu loop: timer fired %ld, MSGF_MENU calls %ld, other codes %ld", menu_timer_fired, msgf_menu, msgf_other);
    if (wd) { TerminateThread(wd, 0); CloseHandle(wd); }
    U(DestroyMenu)(m);
    dh_destroy_window(w, "T32MenuWnd");
    KDONE();
}
static volatile LONG dlg_inited, dlg_timer;
static INT_PTR CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) { dlg_inited++; SetTimer(h, 92, 100, NULL); return TRUE; }
    if (m == WM_TIMER && w == 92) { dlg_timer++; KillTimer(h, 92); EndDialog(h, 123); return TRUE; }
    if (m == WM_COMMAND && LOWORD(w) == IDCANCEL) { EndDialog(h, -2); return TRUE; }
    if (m == WM_CLOSE) { EndDialog(h, -3); return TRUE; }
    return FALSE;
}
static LPDLGTEMPLATE make_dlg(void *buf) {
    DLGTEMPLATE *d = (DLGTEMPLATE *)buf;
    memset(buf, 0, 64);
    d->style = WS_POPUP | WS_CAPTION | DS_MODALFRAME;
    d->cdit = 0; d->x = 0; d->y = 0; d->cx = 80; d->cy = 40;
    /* menu, class, title: three zero words follow */
    return d;
}
TEST(msgfilter_dialog_loop) {
    KNEED();
    REQUIRE(U(DialogBoxIndirectParamA)); REQUIRE(U(EndDialog));
    BYTE buf[64];
    DLGTEMPLATE *d = make_dlg(buf);
    HWND owner = kmk(WS_OVERLAPPEDWINDOW, -30000, -30000, 100, 80, NULL);
    msgf_dlg = msgf_menu = msgf_other = 0; dlg_inited = dlg_timer = 0;
    HHOOK h = U(SetWindowsHookExA)(WH_MSGFILTER, modal_filter, NULL, GetCurrentThreadId());
    HANDLE wd = CreateThread(NULL, 0, watchdog, owner, 0, NULL);
    INT_PTR r = U(DialogBoxIndirectParamA)(GetModuleHandleA(NULL), d, owner, dlg_proc, 0);
    U(UnhookWindowsHookEx)(h);
    t_note("dialog loop: result %lld, MSGF_DIALOGBOX calls %ld, other %ld", (long long)r, msgf_dlg, msgf_other);
    EQ(dlg_inited, 1);
    EQ(r, 123);
    if (wd) { TerminateThread(wd, 0); CloseHandle(wd); }
    U(DestroyWindow)(owner);
    KDONE();
}
TEST(dialog_endialog_result_values) {
    KNEED();
    REQUIRE(U(DialogBoxIndirectParamA));
    BYTE buf[64];
    DLGTEMPLATE *d = make_dlg(buf);
    dlg_inited = dlg_timer = 0;
    HANDLE wd = NULL;
    INT_PTR r = U(DialogBoxIndirectParamA)(GetModuleHandleA(NULL), d, NULL, dlg_proc, 0);
    EQ(r, 123);
    if (wd) CloseHandle(wd);
}
static volatile HWND g_mb_found;
static void CALLBACK mb_timer(HWND h, UINT m, UINT_PTR id, DWORD t) {
    HWND mb = FindWindowA("#32770", "T32MsgBoxHookTest");
    if (mb) { g_mb_found = mb; PostMessageA(mb, WM_CLOSE, 0, 0); }
}
static DWORD WINAPI mb_watchdog(LPVOID p) {
    for (int i = 0; i < 40; i++) {
        Sleep(100);
        HWND mb = FindWindowA("#32770", "T32MsgBoxHookTest");
        if (mb) { PostMessageA(mb, WM_CLOSE, 0, 0); }
    }
    return 0;
}
TEST(msgfilter_messagebox_loop) {
    KNEED();
    REQUIRE(U(MessageBoxA));
    msgf_dlg = msgf_msgbox = msgf_menu = msgf_other = 0; g_mb_found = NULL;
    HHOOK h = U(SetWindowsHookExA)(WH_MSGFILTER, modal_filter, NULL, GetCurrentThreadId());
    UINT_PTR tm = SetTimer(NULL, 0, 100, mb_timer);
    HANDLE wd = CreateThread(NULL, 0, mb_watchdog, NULL, 0, NULL);
    int r = U(MessageBoxA)(NULL, "closing automatically", "T32MsgBoxHookTest", MB_OK);
    KillTimer(NULL, tm);
    U(UnhookWindowsHookEx)(h);
    t_note("MessageBox via WM_CLOSE returned %d; MSGF_MESSAGEBOX %ld, MSGF_DIALOGBOX %ld", r, msgf_msgbox, msgf_dlg);
    CHECK(r == IDOK || r == IDCANCEL);
    if (wd) { WaitForSingleObject(wd, 6000); CloseHandle(wd); }
    KDONE();
}
