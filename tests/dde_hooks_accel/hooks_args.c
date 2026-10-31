/* SetWindowsHookEx argument validation, chains, WH_GETMESSAGE, WH_MSGFILTER, WH_DEBUG, LL hooks. */
#include "dh.h"

#ifndef ERROR_INVALID_HOOK_HANDLE
#define ERROR_INVALID_HOOK_HANDLE 1404
#endif

#ifndef ERROR_HOOK_NEEDS_HMOD
#define ERROR_HOOK_NEEDS_HMOD 1428
#endif
#ifndef ERROR_GLOBAL_ONLY_HOOK
#define ERROR_GLOBAL_ONLY_HOOK 1429
#endif
#ifndef ERROR_HOTKEY_ALREADY_REGISTERED
#define ERROR_HOTKEY_ALREADY_REGISTERED 1409
#endif

static const char *HCLS = "T32HookWnd";
static LRESULT CALLBACK hwnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }
static HWND hmkwnd(void) { return dh_create_window(HCLS, hwnd_proc, WS_OVERLAPPED, 0, NULL); }
static void hkillwnd(HWND h) { dh_destroy_window(h, HCLS); }
static int hneed(void) {
    return U(SetWindowsHookExA) && U(SetWindowsHookExW) && U(UnhookWindowsHookEx) && U(CallNextHookEx) && U(PeekMessageA) &&
           U(PostMessageA) && U(RegisterClassA) && U(CreateWindowExA) && U(DestroyWindow) && U(UnregisterClassA);
}
#define HNEED() do { if (!hneed()) { t_skip("missing exports"); return; } } while (0)

static LRESULT CALLBACK pass_proc(int c, WPARAM w, LPARAM l) { return U(CallNextHookEx)(NULL, c, w, l); }

/* ---------- argument validation ---------- */
TEST(hook_set_null_proc) {
    HNEED();
    DWORD tid = GetCurrentThreadId();
    SetLastError(0);
    EQ(U(SetWindowsHookExA)(WH_GETMESSAGE, NULL, NULL, tid), NULL);
    EQ(GetLastError(), ERROR_INVALID_FILTER_PROC);
    SetLastError(0);
    EQ(U(SetWindowsHookExW)(WH_CBT, NULL, GetModuleHandleA(NULL), tid), NULL);
    EQ(GetLastError(), ERROR_INVALID_FILTER_PROC);
}
TEST(hook_set_invalid_id) {
    HNEED();
    static const int ids[] = {-2, -100, 15, 16, 100, 0x7FFFFFFF, (int)0x80000000, -0x7FFFFFFF};
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(ids[i], pass_proc, NULL, GetCurrentThreadId());
        CHECKF(h == NULL, "idHook %d", ids[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HOOK_FILTER, "idHook %d err %lu", ids[i], GetLastError());
        if (h) U(UnhookWindowsHookEx)(h);
        SetLastError(0);
        h = U(SetWindowsHookExW)(ids[i], pass_proc, NULL, GetCurrentThreadId());
        CHECKF(h == NULL, "W idHook %d", ids[i]);
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(hook_set_global_needs_module) {
    HNEED();
    static const int ids[] = {WH_MSGFILTER, WH_GETMESSAGE, WH_CALLWNDPROC, WH_CBT, WH_KEYBOARD, WH_MOUSE, WH_DEBUG, WH_CALLWNDPROCRET, WH_FOREGROUNDIDLE, WH_SHELL};
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(ids[i], pass_proc, NULL, 0);
        CHECKF(h == NULL, "idHook %d", ids[i]);
        CHECKF(GetLastError() == ERROR_HOOK_NEEDS_HMOD, "idHook %d err %lu", ids[i], GetLastError());
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(hook_set_nonexistent_thread) {
    HNEED();
    static const DWORD tids[] = {1, 3, 0xFFFFFFFF, 0xFFFFFFF1, 0x7FFFFFFF, 0x12345679};
    for (unsigned i = 0; i < sizeof tids / sizeof tids[0]; i++) {
        SetLastError(0);
        HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, pass_proc, GetModuleHandleA(NULL), tids[i]);
        CHECKF(h == NULL, "tid 0x%lx", tids[i]);
        t_note("tid 0x%lx -> err %lu", tids[i], GetLastError());
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "tid 0x%lx err %lu", tids[i], GetLastError());
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(hook_set_bad_module_handle) {
    HNEED();
    static const ULONG_PTR mods[] = {1, 2, 0x1000, 0xDEADBEEF};
    for (unsigned i = 0; i < sizeof mods / sizeof mods[0]; i++) {
        HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, pass_proc, (HINSTANCE)mods[i], GetCurrentThreadId());
        CHECKF(h == NULL, "hMod 0x%llx", (unsigned long long)mods[i]);
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(hook_set_local_types) {
    HNEED();
    static const int ids[] = {WH_MSGFILTER, WH_GETMESSAGE, WH_CALLWNDPROC, WH_CBT, WH_KEYBOARD, WH_MOUSE, WH_DEBUG, WH_CALLWNDPROCRET, WH_FOREGROUNDIDLE};
    DWORD tid = GetCurrentThreadId();
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        HHOOK h = U(SetWindowsHookExA)(ids[i], pass_proc, NULL, tid);
        CHECKF(h != NULL, "A idHook %d err %lu", ids[i], GetLastError());
        if (h) EQ(U(UnhookWindowsHookEx)(h), TRUE);
        h = U(SetWindowsHookExW)(ids[i], pass_proc, GetModuleHandleA(NULL), tid);
        CHECKF(h != NULL, "W idHook %d err %lu", ids[i], GetLastError());
        if (h) EQ(U(UnhookWindowsHookEx)(h), TRUE);
    }
}
TEST(hook_set_shell_local_observed) {
    HNEED();
    SetLastError(0);
    HHOOK h = U(SetWindowsHookExA)(WH_SHELL, pass_proc, NULL, GetCurrentThreadId());
    t_note("WH_SHELL thread hook -> %p err=%lu", h, GetLastError());
    if (h) U(UnhookWindowsHookEx)(h);
}
TEST(hook_set_sysmsgfilter_thread_rejected) {
    HNEED();
    SetLastError(0);
    HHOOK h = U(SetWindowsHookExA)(WH_SYSMSGFILTER, pass_proc, GetModuleHandleA(NULL), GetCurrentThreadId());
    EQ(h, NULL);
    t_note("WH_SYSMSGFILTER with thread id err=%lu", GetLastError());
    CHECK(GetLastError() == ERROR_GLOBAL_ONLY_HOOK);
    if (h) U(UnhookWindowsHookEx)(h);
}
TEST(hook_set_ll_with_thread_rejected) {
    HNEED();
    SetLastError(0);
    HHOOK h = U(SetWindowsHookExA)(WH_KEYBOARD_LL, pass_proc, GetModuleHandleA(NULL), GetCurrentThreadId());
    EQ(h, NULL);
    t_note("WH_KEYBOARD_LL with thread id err=%lu", GetLastError());
    if (h) U(UnhookWindowsHookEx)(h);
    SetLastError(0);
    h = U(SetWindowsHookExW)(WH_MOUSE_LL, pass_proc, GetModuleHandleA(NULL), GetCurrentThreadId());
    EQ(h, NULL);
    t_note("WH_MOUSE_LL with thread id err=%lu", GetLastError());
    if (h) U(UnhookWindowsHookEx)(h);
}
TEST(hook_ll_install_uninstall) {
    HNEED();
    HMODULE me = GetModuleHandleA(NULL);
    HHOOK k = U(SetWindowsHookExA)(WH_KEYBOARD_LL, pass_proc, me, 0);
    CHECKF(k != NULL, "WH_KEYBOARD_LL err %lu", GetLastError());
    HHOOK m = U(SetWindowsHookExW)(WH_MOUSE_LL, pass_proc, me, 0);
    CHECKF(m != NULL, "WH_MOUSE_LL err %lu", GetLastError());
    if (k && m) CHECK(k != m);
    if (m) EQ(U(UnhookWindowsHookEx)(m), TRUE);
    if (k) EQ(U(UnhookWindowsHookEx)(k), TRUE);
    if (k) { SetLastError(0); EQ(U(UnhookWindowsHookEx)(k), FALSE); EQ(GetLastError(), ERROR_INVALID_HOOK_HANDLE); }
}
TEST(hook_ll_null_module_observed) {
    HNEED();
    SetLastError(0);
    HHOOK k = U(SetWindowsHookExA)(WH_KEYBOARD_LL, pass_proc, NULL, 0);
    t_note("WH_KEYBOARD_LL with NULL hMod -> %p err=%lu", k, GetLastError());
    if (k) U(UnhookWindowsHookEx)(k);
}
TEST(hook_ll_repeated_install) {
    HNEED();
    HMODULE me = GetModuleHandleA(NULL);
    HHOOK hs[8]; int n = 0;
    for (int i = 0; i < 8; i++) { hs[i] = U(SetWindowsHookExA)(WH_MOUSE_LL, pass_proc, me, 0); if (hs[i]) n++; }
    EQ(n, 8);
    for (int i = 0; i < 8; i++) if (hs[i]) CHECK(U(UnhookWindowsHookEx)(hs[i]));
}
TEST(hook_unhook_invalid_handles) {
    HNEED();
    static const ULONG_PTR bad[] = {0, 1, 2, 0x1234, 0xFFFF, 0x10000, 0xDEADBEEF, ~(ULONG_PTR)0};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(U(UnhookWindowsHookEx)((HHOOK)bad[i]) == FALSE, "h=0x%llx", (unsigned long long)bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HOOK_HANDLE, "h=0x%llx err %lu", (unsigned long long)bad[i], GetLastError());
    }
}
TEST(hook_unhook_twice) {
    HNEED();
    HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, pass_proc, NULL, GetCurrentThreadId());
    CHECK(h != NULL);
    EQ(U(UnhookWindowsHookEx)(h), TRUE);
    SetLastError(0);
    EQ(U(UnhookWindowsHookEx)(h), FALSE);
    EQ(GetLastError(), ERROR_INVALID_HOOK_HANDLE);
}
TEST(hook_unhook_wrong_object) {
    HNEED();
    HWND w = hmkwnd();
    SetLastError(0);
    EQ(U(UnhookWindowsHookEx)((HHOOK)w), FALSE);
    EQ(GetLastError(), ERROR_INVALID_HOOK_HANDLE);
    CHECK(IsWindow(w));
    hkillwnd(w);
}
TEST(hook_handles_distinct) {
    HNEED();
    HHOOK a = U(SetWindowsHookExA)(WH_GETMESSAGE, pass_proc, NULL, GetCurrentThreadId());
    HHOOK b = U(SetWindowsHookExA)(WH_GETMESSAGE, pass_proc, NULL, GetCurrentThreadId());
    HHOOK c = U(SetWindowsHookExW)(WH_CBT, pass_proc, NULL, GetCurrentThreadId());
    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
    U(UnhookWindowsHookEx)(b); U(UnhookWindowsHookEx)(a); U(UnhookWindowsHookEx)(c);
}
TEST(hook_callnext_outside_hook) {
    HNEED();
    EQ(U(CallNextHookEx)(NULL, 0, 0, 0), 0);
    EQ(U(CallNextHookEx)(NULL, HC_ACTION, 5, 6), 0);
    EQ(U(CallNextHookEx)(NULL, -1, 0, 0), 0);
    EQ(U(CallNextHookEx)((HHOOK)0x1234, WH_GETMESSAGE, 1, 2), 0);
}
TEST(hook_many_install_remove) {
    HNEED();
    enum { N = 60 };
    HHOOK hs[N]; int ok = 0;
    for (int i = 0; i < N; i++) { hs[i] = U(SetWindowsHookExA)(WH_GETMESSAGE, pass_proc, NULL, GetCurrentThreadId()); if (hs[i]) ok++; }
    EQ(ok, N);
    for (int i = 0; i < N; i += 2) if (hs[i]) CHECK(U(UnhookWindowsHookEx)(hs[i]));
    for (int i = 1; i < N; i += 2) if (hs[i]) CHECK(U(UnhookWindowsHookEx)(hs[i]));
}
TEST(hook_install_remove_loop) {
    HNEED();
    int bad = 0;
    for (int i = 0; i < 3000; i++) {
        HHOOK h = U(SetWindowsHookExA)(WH_CALLWNDPROC, pass_proc, NULL, GetCurrentThreadId());
        if (!h || !U(UnhookWindowsHookEx)(h)) { bad++; break; }
    }
    EQ(bad, 0);
}
TEST(hook_obsolete_sethook_observed) {
    REQUIRE(U(SetWindowsHookA)); REQUIRE(U(UnhookWindowsHook));
    SetLastError(0);
    HHOOK h = U(SetWindowsHookA)(WH_GETMESSAGE, (HOOKPROC)pass_proc);
    t_note("SetWindowsHookA(WH_GETMESSAGE) -> %p err=%lu", h, GetLastError());
    if (h) {
        BOOL r = U(UnhookWindowsHook)(WH_GETMESSAGE, (HOOKPROC)pass_proc);
        t_note("UnhookWindowsHook -> %d", r);
        U(UnhookWindowsHookEx)(h);
    }
}

/* ---------- WH_GETMESSAGE ---------- */
static volatile LONG gm_calls, gm_remove, gm_noremove;
static volatile UINT gm_last_msg;
static volatile WPARAM gm_last_wp;
static volatile int gm_rewrite;
static volatile int gm_remove_self;
static HHOOK gm_hook;
static DWORD gm_tid;
static LRESULT CALLBACK gm_proc(int c, WPARAM w, LPARAM l) {
    if (c == HC_ACTION || c == HC_NOREMOVE) {
        MSG *m = (MSG *)l;
        InterlockedIncrement(&gm_calls);
        if (w == PM_REMOVE) InterlockedIncrement(&gm_remove); else InterlockedIncrement(&gm_noremove);
        gm_last_msg = m->message; gm_last_wp = w;
        gm_tid = GetCurrentThreadId();
        if (m->message == WM_USER + 5 && gm_rewrite) { m->message = WM_USER + 6; m->wParam = 0x77; m->lParam = 0x88; }
        if (gm_remove_self) { U(UnhookWindowsHookEx)(gm_hook); gm_remove_self = 0; }
    }
    return U(CallNextHookEx)(gm_hook, c, w, l);
}
static void gm_reset(void) { gm_calls = gm_remove = gm_noremove = 0; gm_last_msg = 0; gm_last_wp = 0; gm_rewrite = 0; gm_remove_self = 0; gm_tid = 0; }

TEST(hook_getmessage_basic) {
    HNEED();
    gm_reset();
    HWND w = hmkwnd();
    gm_hook = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    CHECK(gm_hook != NULL);
    PostMessageA(w, WM_USER + 5, 1, 2);
    MSG m;
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    EQ(m.message, WM_USER + 5);
    EQ(gm_last_msg, WM_USER + 5);
    EQ(gm_last_wp, PM_REMOVE);
    CHECK(gm_remove >= 1);
    EQ(gm_tid, GetCurrentThreadId());
    U(UnhookWindowsHookEx)(gm_hook);
    hkillwnd(w);
}
TEST(hook_getmessage_noremove_flag) {
    HNEED();
    gm_reset();
    HWND w = hmkwnd();
    gm_hook = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    PostMessageA(w, WM_USER + 5, 1, 2);
    MSG m;
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_NOREMOVE));
    CHECK(gm_noremove >= 1);
    EQ(gm_remove, 0);
    EQ(gm_last_wp, PM_NOREMOVE);
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    CHECK(gm_remove >= 1);
    U(UnhookWindowsHookEx)(gm_hook);
    hkillwnd(w);
}
TEST(hook_getmessage_no_message_no_call) {
    HNEED();
    gm_reset();
    HWND w = hmkwnd();
    gm_hook = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    MSG m;
    t_pump();
    gm_calls = 0;
    CHECK(!PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    EQ(gm_calls, 0);
    U(UnhookWindowsHookEx)(gm_hook);
    hkillwnd(w);
}
TEST(hook_getmessage_modifies_message) {
    HNEED();
    gm_reset();
    HWND w = hmkwnd();
    gm_hook = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    gm_rewrite = 1;
    PostMessageA(w, WM_USER + 5, 1, 2);
    MSG m; memset(&m, 0, sizeof m);
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    EQ(m.message, WM_USER + 6);
    EQ(m.wParam, 0x77);
    EQ(m.lParam, 0x88);
    EQ(m.hwnd, w);
    U(UnhookWindowsHookEx)(gm_hook);
    hkillwnd(w);
}
TEST(hook_getmessage_modify_with_noremove_then_remove) {
    HNEED();
    gm_reset();
    HWND w = hmkwnd();
    gm_hook = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    gm_rewrite = 1;
    PostMessageA(w, WM_USER + 5, 1, 2);
    MSG m;
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_NOREMOVE));
    t_note("modified message through PM_NOREMOVE: 0x%x", m.message);
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    U(UnhookWindowsHookEx)(gm_hook);
    hkillwnd(w);
}
TEST(hook_getmessage_removed_inside_proc) {
    HNEED();
    gm_reset();
    HWND w = hmkwnd();
    gm_hook = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    gm_remove_self = 1;
    PostMessageA(w, WM_USER + 1, 0, 0);
    MSG m;
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    EQ(gm_calls, 1);
    PostMessageA(w, WM_USER + 1, 0, 0);
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    EQ(gm_calls, 1);                         /* the hook is gone */
    SetLastError(0);
    EQ(U(UnhookWindowsHookEx)(gm_hook), FALSE);
    EQ(GetLastError(), ERROR_INVALID_HOOK_HANDLE);
    hkillwnd(w);
}
TEST(hook_getmessage_not_called_after_unhook) {
    HNEED();
    gm_reset();
    HWND w = hmkwnd();
    gm_hook = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    EQ(U(UnhookWindowsHookEx)(gm_hook), TRUE);
    PostMessageA(w, WM_USER + 1, 0, 0);
    MSG m;
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    EQ(gm_calls, 0);
    hkillwnd(w);
}
TEST(hook_getmessage_wide_variant) {
    HNEED();
    gm_reset();
    HWND w = hmkwnd();
    gm_hook = U(SetWindowsHookExW)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    CHECK(gm_hook != NULL);
    PostMessageA(w, WM_USER + 1, 3, 4);
    MSG m;
    CHECK(U(PeekMessageW) && U(PeekMessageW)(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    CHECK(gm_remove >= 1);
    U(UnhookWindowsHookEx)(gm_hook);
    hkillwnd(w);
}
TEST(hook_getmessage_char_ansi_wide_parity) {
    HNEED();
    REQUIRE(U(PeekMessageW));
    static volatile WPARAM seen_a, seen_w;
    LRESULT CALLBACK pa(int c, WPARAM w, LPARAM l) { if (c == HC_ACTION && ((MSG *)l)->message == WM_CHAR) seen_a = ((MSG *)l)->wParam; return U(CallNextHookEx)(NULL, c, w, l); }
    LRESULT CALLBACK pw(int c, WPARAM w, LPARAM l) { if (c == HC_ACTION && ((MSG *)l)->message == WM_CHAR) seen_w = ((MSG *)l)->wParam; return U(CallNextHookEx)(NULL, c, w, l); }
    HWND w = hmkwnd();
    HHOOK ha = U(SetWindowsHookExA)(WH_GETMESSAGE, pa, NULL, GetCurrentThreadId());
    PostMessageW(w, WM_CHAR, 'Q', 1);
    MSG m;
    seen_a = 0;
    CHECK(PeekMessageW(&m, w, WM_CHAR, WM_CHAR, PM_REMOVE));
    EQ(seen_a, 'Q'); EQ(m.wParam, 'Q');
    PostMessageW(w, WM_CHAR, 0x20AC, 1);
    seen_a = 0;
    CHECK(PeekMessageW(&m, w, WM_CHAR, WM_CHAR, PM_REMOVE));
    t_note("A hook on W retrieval of U+20AC: hook saw 0x%llx, app saw 0x%llx", (unsigned long long)seen_a, (unsigned long long)m.wParam);
    U(UnhookWindowsHookEx)(ha);
    HHOOK hw = U(SetWindowsHookExW)(WH_GETMESSAGE, pw, NULL, GetCurrentThreadId());
    PostMessageW(w, WM_CHAR, 0x20AC, 1);
    seen_w = 0;
    CHECK(PeekMessageW(&m, w, WM_CHAR, WM_CHAR, PM_REMOVE));
    EQ(seen_w, 0x20AC);
    EQ(m.wParam, 0x20AC);
    U(UnhookWindowsHookEx)(hw);
    hkillwnd(w);
}

/* ---------- chaining ---------- */
static int order[16]; static int norder;
static HHOOK chain_h[4];
static int chain_stop_at = -1;
#define CHAINPROC(N) static LRESULT CALLBACK chain_proc##N(int c, WPARAM w, LPARAM l) { \
    if (c == HC_ACTION && ((MSG *)l)->message == WM_USER + 9 && norder < 16) order[norder++] = N; \
    if (chain_stop_at == N) return 0; \
    return U(CallNextHookEx)(chain_h[N], c, w, l); }
CHAINPROC(0) CHAINPROC(1) CHAINPROC(2) CHAINPROC(3)
static HOOKPROC chain_procs[4] = {chain_proc0, chain_proc1, chain_proc2, chain_proc3};

static void chain_pump(HWND w) {
    MSG m;
    PostMessageA(w, WM_USER + 9, 0, 0);
    norder = 0;
    PeekMessageA(&m, w, WM_USER + 9, WM_USER + 9, PM_REMOVE);
}
TEST(hook_chain_lifo_order) {
    HNEED();
    HWND w = hmkwnd();
    chain_stop_at = -1;
    for (int i = 0; i < 4; i++) { chain_h[i] = U(SetWindowsHookExA)(WH_GETMESSAGE, chain_procs[i], NULL, GetCurrentThreadId()); CHECK(chain_h[i] != NULL); }
    chain_pump(w);
    EQ(norder, 4);
    for (int i = 0; i < norder; i++) CHECKF(order[i] == 3 - i, "position %d got %d", i, order[i]);
    for (int i = 3; i >= 0; i--) if (chain_h[i]) U(UnhookWindowsHookEx)(chain_h[i]);
    hkillwnd(w);
}
TEST(hook_chain_stop_without_next) {
    HNEED();
    HWND w = hmkwnd();
    for (int i = 0; i < 4; i++) chain_h[i] = U(SetWindowsHookExA)(WH_GETMESSAGE, chain_procs[i], NULL, GetCurrentThreadId());
    chain_stop_at = 2;
    chain_pump(w);
    EQ(norder, 2);                       /* 3 then 2; 1 and 0 never run */
    if (norder == 2) { EQ(order[0], 3); EQ(order[1], 2); }
    chain_stop_at = -1;
    for (int i = 3; i >= 0; i--) if (chain_h[i]) U(UnhookWindowsHookEx)(chain_h[i]);
    hkillwnd(w);
}
TEST(hook_chain_remove_middle) {
    HNEED();
    HWND w = hmkwnd();
    chain_stop_at = -1;
    for (int i = 0; i < 4; i++) chain_h[i] = U(SetWindowsHookExA)(WH_GETMESSAGE, chain_procs[i], NULL, GetCurrentThreadId());
    EQ(U(UnhookWindowsHookEx)(chain_h[2]), TRUE);
    chain_pump(w);
    EQ(norder, 3);
    if (norder == 3) { EQ(order[0], 3); EQ(order[1], 1); EQ(order[2], 0); }
    EQ(U(UnhookWindowsHookEx)(chain_h[3]), TRUE);
    chain_pump(w);
    EQ(norder, 2);
    if (norder == 2) { EQ(order[0], 1); EQ(order[1], 0); }
    U(UnhookWindowsHookEx)(chain_h[1]); U(UnhookWindowsHookEx)(chain_h[0]);
    chain_pump(w);
    EQ(norder, 0);
    hkillwnd(w);
}
TEST(hook_chain_reinstall_goes_first) {
    HNEED();
    HWND w = hmkwnd();
    chain_stop_at = -1;
    chain_h[0] = U(SetWindowsHookExA)(WH_GETMESSAGE, chain_procs[0], NULL, GetCurrentThreadId());
    chain_h[1] = U(SetWindowsHookExA)(WH_GETMESSAGE, chain_procs[1], NULL, GetCurrentThreadId());
    U(UnhookWindowsHookEx)(chain_h[0]);
    chain_h[0] = U(SetWindowsHookExA)(WH_GETMESSAGE, chain_procs[0], NULL, GetCurrentThreadId());
    chain_pump(w);
    EQ(norder, 2);
    if (norder == 2) { EQ(order[0], 0); EQ(order[1], 1); }
    U(UnhookWindowsHookEx)(chain_h[0]); U(UnhookWindowsHookEx)(chain_h[1]);
    hkillwnd(w);
}
TEST(hook_chain_different_types_independent) {
    HNEED();
    HWND w = hmkwnd();
    chain_stop_at = -1;
    chain_h[0] = U(SetWindowsHookExA)(WH_GETMESSAGE, chain_procs[0], NULL, GetCurrentThreadId());
    chain_h[1] = U(SetWindowsHookExA)(WH_KEYBOARD, pass_proc, NULL, GetCurrentThreadId());
    chain_h[2] = U(SetWindowsHookExA)(WH_CALLWNDPROC, pass_proc, NULL, GetCurrentThreadId());
    chain_pump(w);
    EQ(norder, 1);
    for (int i = 2; i >= 0; i--) U(UnhookWindowsHookEx)(chain_h[i]);
    hkillwnd(w);
}

/* ---------- hook thread binding ---------- */
static volatile LONG thr_ready, thr_done, thr_calls;
static volatile DWORD thr_hook_tid, thr_id;
static LRESULT CALLBACK thr_proc(int c, WPARAM w, LPARAM l) {
    if (c == HC_ACTION && ((MSG *)l)->message == WM_USER + 21) { InterlockedIncrement(&thr_calls); thr_hook_tid = GetCurrentThreadId(); }
    return U(CallNextHookEx)(NULL, c, w, l);
}
static DWORD WINAPI thr_main(LPVOID p) {
    MSG m;
    thr_id = GetCurrentThreadId();
    PeekMessageA(&m, NULL, 0, 0, PM_NOREMOVE);        /* create the queue */
    thr_ready = 1;
    while (!thr_done) {
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
    }
    return 0;
}
TEST(hook_thread_specific_other_thread) {
    HNEED();
    thr_ready = thr_done = 0; thr_calls = 0; thr_hook_tid = 0;
    HANDLE t = CreateThread(NULL, 0, thr_main, NULL, 0, NULL);
    DWORD end = GetTickCount() + 5000;
    while (!thr_ready && GetTickCount() < end) Sleep(5);
    CHECK(thr_ready);
    if (thr_ready) {
        HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, thr_proc, GetModuleHandleA(NULL), thr_id);
        CHECKF(h != NULL, "err %lu", GetLastError());
        /* our own thread's retrievals are not hooked */
        PostThreadMessageA(GetCurrentThreadId(), WM_USER + 21, 0, 0);
        MSG m;
        CHECK(PeekMessageA(&m, NULL, WM_USER + 21, WM_USER + 21, PM_REMOVE));
        EQ(thr_calls, 0);
        /* the target thread's are */
        CHECK(PostThreadMessageA(thr_id, WM_USER + 21, 0, 0));
        end = GetTickCount() + 3000;
        while (thr_calls == 0 && GetTickCount() < end) Sleep(5);
        EQ(thr_calls, 1);
        EQ(thr_hook_tid, thr_id);
        if (h) EQ(U(UnhookWindowsHookEx)(h), TRUE);
    }
    thr_done = 1;
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
}
TEST(hook_thread_unhook_from_other_thread) {
    HNEED();
    static HHOOK uh; static volatile LONG r = -1;
    DWORD WINAPI th(LPVOID p) { r = U(UnhookWindowsHookEx)(uh); return 0; }
    uh = U(SetWindowsHookExA)(WH_GETMESSAGE, pass_proc, NULL, GetCurrentThreadId());
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    t_note("UnhookWindowsHookEx from a different thread -> %ld", r);
    if (r != 1) U(UnhookWindowsHookEx)(uh);
}

/* ---------- WH_MSGFILTER via CallMsgFilter ---------- */
static volatile int mf_code, mf_calls; static volatile int mf_ret;
static MSG mf_seen;
static LRESULT CALLBACK mf_proc(int c, WPARAM w, LPARAM l) {
    mf_calls++; mf_code = c;
    if (l) mf_seen = *(MSG *)l;
    if (mf_ret) return mf_ret;
    return U(CallNextHookEx)(NULL, c, w, l);
}
TEST(hook_msgfilter_callmsgfilter) {
    HNEED();
    REQUIRE(U(CallMsgFilterA)); REQUIRE(U(CallMsgFilterW));
    MSG m; memset(&m, 0, sizeof m);
    m.hwnd = (HWND)(ULONG_PTR)0x1234; m.message = WM_USER + 3; m.wParam = 0xAA; m.lParam = 0xBB; m.time = 77; m.pt.x = 5; m.pt.y = 6;
    EQ(U(CallMsgFilterA)(&m, MSGF_USER), FALSE);          /* no hook installed */
    mf_calls = 0; mf_ret = 0;
    HHOOK h = U(SetWindowsHookExA)(WH_MSGFILTER, mf_proc, NULL, GetCurrentThreadId());
    CHECK(h != NULL);
    EQ(U(CallMsgFilterA)(&m, MSGF_USER + 5), FALSE);
    EQ(mf_calls, 1);
    EQ(mf_code, MSGF_USER + 5);
    EQ(mf_seen.message, WM_USER + 3); EQ(mf_seen.wParam, 0xAA); EQ(mf_seen.lParam, 0xBB);
    EQ(mf_seen.hwnd, (HWND)(ULONG_PTR)0x1234); EQ(mf_seen.time, 77); EQ(mf_seen.pt.x, 5); EQ(mf_seen.pt.y, 6);
    mf_ret = 1;
    EQ(U(CallMsgFilterA)(&m, MSGF_USER), TRUE);
    EQ(U(CallMsgFilterW)(&m, MSGF_USER), TRUE);
    EQ(mf_calls, 3);
    mf_ret = 0x55;
    LRESULT r = U(CallMsgFilterA)(&m, MSGF_USER);
    t_note("CallMsgFilter with hook result 0x55 -> %lld", (long long)r);
    CHECK(r != 0);
    U(UnhookWindowsHookEx)(h);
    mf_ret = 0;
    EQ(U(CallMsgFilterA)(&m, MSGF_USER), FALSE);
}
TEST(hook_msgfilter_codes_passed) {
    HNEED();
    REQUIRE(U(CallMsgFilterA));
    mf_calls = 0; mf_ret = 0;
    HHOOK h = U(SetWindowsHookExA)(WH_MSGFILTER, mf_proc, NULL, GetCurrentThreadId());
    MSG m; memset(&m, 0, sizeof m); m.message = WM_USER;
    static const int codes[] = {MSGF_DIALOGBOX, MSGF_MESSAGEBOX, MSGF_MENU, 3, 4, MSGF_SCROLLBAR, MSGF_NEXTWINDOW, 7, MSGF_MAX, MSGF_USER, MSGF_USER + 1, MSGF_USER + 0x7000, 0x7FFFFFFF};
    for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        mf_calls = 0; mf_code = -77;
        U(CallMsgFilterA)(&m, codes[i]);
        CHECKF(mf_calls == 1 && mf_code == codes[i], "code %d calls %d seen %d", codes[i], mf_calls, mf_code);
    }
    U(UnhookWindowsHookEx)(h);
}
TEST(hook_msgfilter_skips_touch_and_gesture) {
    HNEED();
    REQUIRE(U(CallMsgFilterA));
    mf_calls = 0; mf_ret = 1;
    HHOOK h = U(SetWindowsHookExA)(WH_MSGFILTER, mf_proc, NULL, GetCurrentThreadId());
    MSG m; memset(&m, 0, sizeof m);
    static const UINT skipped[] = {0x240, 0x119, 0x11B};
    for (unsigned i = 0; i < sizeof skipped / sizeof skipped[0]; i++) {
        m.message = skipped[i];
        mf_calls = 0;
        CHECKF(U(CallMsgFilterA)(&m, MSGF_USER) == FALSE && mf_calls == 0, "message 0x%x", skipped[i]);
    }
    m.message = 0x11A;
    mf_calls = 0;
    CHECK(U(CallMsgFilterA)(&m, MSGF_USER) != FALSE);
    mf_ret = 0;
    U(UnhookWindowsHookEx)(h);
}
TEST(hook_msgfilter_chain) {
    HNEED();
    REQUIRE(U(CallMsgFilterA));
    chain_stop_at = -1;
    static int seq[4]; static int ns;
    LRESULT CALLBACK f1(int c, WPARAM w, LPARAM l) { seq[ns++] = 1; return U(CallNextHookEx)(NULL, c, w, l); }
    LRESULT CALLBACK f2(int c, WPARAM w, LPARAM l) { seq[ns++] = 2; return U(CallNextHookEx)(NULL, c, w, l); }
    HHOOK h1 = U(SetWindowsHookExA)(WH_MSGFILTER, f1, NULL, GetCurrentThreadId());
    HHOOK h2 = U(SetWindowsHookExA)(WH_MSGFILTER, f2, NULL, GetCurrentThreadId());
    MSG m; memset(&m, 0, sizeof m); m.message = WM_USER;
    ns = 0;
    EQ(U(CallMsgFilterA)(&m, MSGF_USER), FALSE);
    EQ(ns, 2);
    if (ns == 2) { EQ(seq[0], 2); EQ(seq[1], 1); }
    U(UnhookWindowsHookEx)(h2); U(UnhookWindowsHookEx)(h1);
}
TEST(hook_callmsgfilter_no_hook) {
    REQUIRE(U(CallMsgFilterA)); REQUIRE(U(CallMsgFilterW)); REQUIRE(U(CallMsgFilter));
    MSG m; memset(&m, 0, sizeof m); m.message = WM_USER;
    EQ(U(CallMsgFilterA)(&m, MSGF_USER), FALSE);
    EQ(U(CallMsgFilterW)(&m, 0), FALSE);
    EQ(U(CallMsgFilterA)(&m, -1), FALSE);
}

/* ---------- WH_DEBUG ---------- */
static volatile int dbg_calls, dbg_veto; static volatile int dbg_code; static DEBUGHOOKINFO dbg_info;
static volatile int dbg_gm_calls;
static LRESULT CALLBACK dbg_proc(int c, WPARAM w, LPARAM l) {
    dbg_calls++; dbg_code = c; if (l) dbg_info = *(DEBUGHOOKINFO *)l;
    if (dbg_veto) return 1;
    return U(CallNextHookEx)(NULL, c, w, l);
}
static LRESULT CALLBACK dbg_gm_proc(int c, WPARAM w, LPARAM l) { dbg_gm_calls++; return U(CallNextHookEx)(NULL, c, w, l); }
TEST(hook_debug_sees_other_hooks) {
    HNEED();
    HWND w = hmkwnd();
    dbg_calls = 0; dbg_veto = 0; dbg_gm_calls = 0; dbg_code = -99;
    HHOOK hg = U(SetWindowsHookExA)(WH_GETMESSAGE, dbg_gm_proc, NULL, GetCurrentThreadId());
    HHOOK hd = U(SetWindowsHookExA)(WH_DEBUG, dbg_proc, NULL, GetCurrentThreadId());
    CHECK(hd != NULL);
    PostMessageA(w, WM_USER + 1, 0, 0);
    MSG m;
    PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE);
    t_note("WH_DEBUG saw code %d (%d calls), GETMESSAGE hook calls %d", dbg_code, dbg_calls, dbg_gm_calls);
    CHECK(dbg_calls >= 1);
    CHECK(dbg_gm_calls >= 1);
    if (dbg_calls) { EQ(dbg_code, WH_GETMESSAGE); EQ(dbg_info.idThread, GetCurrentThreadId()); }
    U(UnhookWindowsHookEx)(hd); U(UnhookWindowsHookEx)(hg);
    hkillwnd(w);
}
TEST(hook_debug_veto_blocks_hook) {
    HNEED();
    HWND w = hmkwnd();
    dbg_calls = 0; dbg_veto = 1; dbg_gm_calls = 0;
    HHOOK hg = U(SetWindowsHookExA)(WH_GETMESSAGE, dbg_gm_proc, NULL, GetCurrentThreadId());
    HHOOK hd = U(SetWindowsHookExA)(WH_DEBUG, dbg_proc, NULL, GetCurrentThreadId());
    PostMessageA(w, WM_USER + 1, 0, 0);
    MSG m;
    CHECK(PeekMessageA(&m, w, WM_USER, WM_USER + 100, PM_REMOVE));
    CHECK(dbg_calls >= 1);
    EQ(dbg_gm_calls, 0);
    dbg_veto = 0;
    U(UnhookWindowsHookEx)(hd); U(UnhookWindowsHookEx)(hg);
    hkillwnd(w);
}
TEST(hook_keyboard_mouse_hooks_with_synthetic_posts) {
    HNEED();
    static volatile int kb, ms;
    LRESULT CALLBACK kp(int c, WPARAM w, LPARAM l) { if (c >= 0) kb++; return U(CallNextHookEx)(NULL, c, w, l); }
    LRESULT CALLBACK mp(int c, WPARAM w, LPARAM l) { if (c >= 0) ms++; return U(CallNextHookEx)(NULL, c, w, l); }
    HWND w = hmkwnd();
    HHOOK hk = U(SetWindowsHookExA)(WH_KEYBOARD, kp, NULL, GetCurrentThreadId());
    HHOOK hm = U(SetWindowsHookExA)(WH_MOUSE, mp, NULL, GetCurrentThreadId());
    CHECK(hk && hm);
    kb = ms = 0;
    PostMessageA(w, WM_KEYDOWN, 'A', 1);
    PostMessageA(w, WM_LBUTTONDOWN, 0, MAKELPARAM(1, 1));
    MSG m;
    while (PeekMessageA(&m, w, 0, 0, PM_REMOVE)) {}
    t_note("synthetic WM_KEYDOWN -> keyboard hook calls %d; WM_LBUTTONDOWN -> mouse hook calls %d", kb, ms);
    U(UnhookWindowsHookEx)(hm); U(UnhookWindowsHookEx)(hk);
    hkillwnd(w);
}
