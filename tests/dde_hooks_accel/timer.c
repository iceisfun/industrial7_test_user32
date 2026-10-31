/* SetTimer / KillTimer / SetCoalescableTimer and WM_TIMER delivery. */
#include "dh.h"

static volatile LONG g_n;               /* TIMERPROC call count */
static volatile UINT_PTR g_id;
static volatile HWND g_h;
static volatile UINT g_msg;
static volatile DWORD g_tick;
static volatile LONG g_n2;
static const char *TCLS = "T32TimerWnd";
static volatile LONG g_wm_timer;        /* WM_TIMER count seen by the window procedure */
static volatile UINT_PTR g_wm_id;
static volatile LPARAM g_wm_lp;
static volatile UINT_PTR g_kill_id;     /* when nonzero the window proc kills this timer on the first WM_TIMER */
static volatile HWND g_kill_hwnd;
static volatile BOOL g_kill_ret;

static void CALLBACK tproc(HWND h, UINT m, UINT_PTR id, DWORD t) {
    InterlockedIncrement(&g_n); g_h = h; g_msg = m; g_id = id; g_tick = t;
}
static void CALLBACK tproc2(HWND h, UINT m, UINT_PTR id, DWORD t) { InterlockedIncrement(&g_n2); }
static void CALLBACK tproc_selfkill(HWND h, UINT m, UINT_PTR id, DWORD t) {
    InterlockedIncrement(&g_n);
    g_kill_ret = U(KillTimer)(h, id);
}
static LRESULT CALLBACK twnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TIMER) {
        InterlockedIncrement(&g_wm_timer); g_wm_id = w; g_wm_lp = l;
        if (g_kill_id && w == g_kill_id) { g_kill_ret = U(KillTimer)(h, g_kill_id); g_kill_id = 0; }
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}
static HWND tmkwnd(void) {
    g_wm_timer = 0; g_wm_id = 0; g_wm_lp = 0; g_kill_id = 0;
    return dh_create_window(TCLS, twnd_proc, WS_OVERLAPPED, 0, NULL);
}
static void tkillwnd(HWND h) { dh_destroy_window(h, TCLS); }
static int wait_n(volatile LONG *v, LONG n, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    while (*v < n && (LONG)(GetTickCount() - end) < 0) dh_sleep_pump(5);
    return *v >= n;
}
static void reset(void) { g_n = g_n2 = 0; g_id = 0; g_h = NULL; g_msg = 0; g_tick = 0; g_wm_timer = 0; }
static int need(void) { return U(SetTimer) && U(KillTimer) && U(RegisterClassA) && U(CreateWindowExA) && U(DestroyWindow) && U(UnregisterClassA); }
#define NEED() do { if (!need()) { t_skip("missing exports"); return; } } while (0)

TEST(timer_null_hwnd_returns_id) {
    NEED();
    UINT_PTR id = U(SetTimer)(NULL, 0, 1000, NULL);
    CHECK(id != 0);
    EQ(U(KillTimer)(NULL, id), TRUE);
}
TEST(timer_null_hwnd_ids_distinct) {
    NEED();
    UINT_PTR ids[50];
    for (int i = 0; i < 50; i++) {
        ids[i] = U(SetTimer)(NULL, 0, 5000, NULL);
        CHECKF(ids[i] != 0, "timer %d", i);
        for (int j = 0; j < i; j++) CHECKF(ids[i] != ids[j], "timers %d and %d share id %llu", i, j, (unsigned long long)ids[i]);
    }
    for (int i = 0; i < 50; i++) CHECK(U(KillTimer)(NULL, ids[i]));
}
TEST(timer_null_hwnd_ignores_unknown_id) {
    NEED();
    UINT_PTR id = U(SetTimer)(NULL, 0x7A57, 5000, NULL);
    CHECK(id != 0);
    t_note("SetTimer(NULL, 0x7A57) -> 0x%llx", (unsigned long long)id);
    CHECK(U(KillTimer)(NULL, id));
}
TEST(timer_null_hwnd_replace_existing) {
    NEED();
    reset();
    UINT_PTR id = U(SetTimer)(NULL, 0, 5000, tproc);
    CHECK(id != 0);
    UINT_PTR id2 = U(SetTimer)(NULL, id, 20, tproc2);
    EQ(id2, id);
    CHECK(wait_n(&g_n2, 1, 3000));
    EQ(g_n, 0);
    EQ(U(KillTimer)(NULL, id), TRUE);
    EQ(U(KillTimer)(NULL, id), FALSE);
}
TEST(timer_proc_called_with_args) {
    NEED();
    reset();
    UINT_PTR id = U(SetTimer)(NULL, 0, 30, tproc);
    CHECK(id != 0);
    CHECK(wait_n(&g_n, 1, 3000));
    U(KillTimer)(NULL, id);
    EQ(g_h, NULL);
    EQ(g_msg, WM_TIMER);
    EQ(g_id, id);
    DWORD now = GetTickCount();
    CHECK((LONG)(now - g_tick) >= 0 && (LONG)(now - g_tick) < 5000);
}
TEST(timer_proc_hwnd_variant) {
    NEED();
    reset();
    HWND w = tmkwnd();
    UINT_PTR id = U(SetTimer)(w, 42, 30, tproc);
    EQ(id, 42);
    CHECK(wait_n(&g_n, 1, 3000));
    EQ(g_h, w);
    EQ(g_id, 42);
    EQ(g_msg, WM_TIMER);
    EQ(g_wm_timer, 0);                       /* the window procedure is bypassed */
    U(KillTimer)(w, 42);
    tkillwnd(w);
}
TEST(timer_wm_timer_to_wndproc) {
    NEED();
    HWND w = tmkwnd();
    UINT_PTR id = U(SetTimer)(w, 7, 30, NULL);
    EQ(id, 7);
    CHECK(wait_n(&g_wm_timer, 1, 3000));
    EQ(g_wm_id, 7);
    EQ(g_wm_lp, 0);
    U(KillTimer)(w, 7);
    tkillwnd(w);
}
TEST(timer_wm_timer_in_thread_queue) {
    NEED();
    UINT_PTR id = U(SetTimer)(NULL, 0, 20, NULL);
    CHECK(id != 0);
    MSG m; int got = 0;
    DWORD end = GetTickCount() + 3000;
    while (!got && (LONG)(GetTickCount() - end) < 0) {
        if (PeekMessageA(&m, NULL, WM_TIMER, WM_TIMER, PM_REMOVE)) got = 1; else Sleep(5);
    }
    CHECK(got);
    if (got) {
        EQ(m.hwnd, NULL);
        EQ(m.message, WM_TIMER);
        EQ(m.wParam, id);
        EQ(m.lParam, 0);
    }
    U(KillTimer)(NULL, id);
}
TEST(timer_wm_timer_carries_proc_in_lparam) {
    NEED();
    UINT_PTR id = U(SetTimer)(NULL, 0, 20, tproc2);
    MSG m; int got = 0;
    DWORD end = GetTickCount() + 3000;
    while (!got && (LONG)(GetTickCount() - end) < 0) {
        if (PeekMessageA(&m, NULL, WM_TIMER, WM_TIMER, PM_REMOVE)) got = 1; else Sleep(5);
    }
    CHECK(got);
    if (got) { EQ(m.wParam, id); EQ(m.lParam, (LPARAM)tproc2); }
    U(KillTimer)(NULL, id);
}
TEST(timer_timing_lower_bound) {
    NEED();
    reset();
    DWORD t0 = GetTickCount();
    UINT_PTR id = U(SetTimer)(NULL, 0, 150, tproc);
    CHECK(wait_n(&g_n, 1, 3000));
    DWORD dt = GetTickCount() - t0;
    U(KillTimer)(NULL, id);
    CHECK(dt >= 120);
    CHECK(dt < 3000);
    t_note("150ms timer fired after %lu ms", dt);
}
TEST(timer_repeats) {
    NEED();
    reset();
    UINT_PTR id = U(SetTimer)(NULL, 0, 30, tproc);
    CHECK(wait_n(&g_n, 5, 5000));
    U(KillTimer)(NULL, id);
    LONG n = g_n;
    dh_sleep_pump(150);
    EQ(g_n, n);                               /* nothing after KillTimer */
}
TEST(timer_elapse_zero_clamped) {
    NEED();
    reset();
    UINT_PTR id = U(SetTimer)(NULL, 0, 0, tproc);
    CHECK(id != 0);
    dh_sleep_pump(300);
    LONG n = g_n;
    U(KillTimer)(NULL, id);
    t_note("elapse 0 -> %ld callbacks in 300 ms", n);
    CHECK(n >= 1);
    CHECK(n <= 40);         /* clamped to USER_TIMER_MINIMUM (10 ms): at most ~30 ticks */
}
TEST(timer_elapse_boundaries_accepted) {
    NEED();
    static const UINT el[] = {0, 1, 9, 10, 11, 15, 16, 100, 1000, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof el / sizeof el[0]; i++) {
        UINT_PTR id = U(SetTimer)(NULL, 0, el[i], tproc2);
        CHECKF(id != 0, "elapse 0x%x", el[i]);
        if (id) CHECKF(U(KillTimer)(NULL, id), "kill elapse 0x%x", el[i]);
    }
    dh_sleep_pump(20);
}
TEST(timer_huge_elapse_does_not_fire) {
    NEED();
    reset();
    UINT_PTR id = U(SetTimer)(NULL, 0, 0xFFFFFFFF, tproc);
    CHECK(id != 0);
    dh_sleep_pump(200);
    EQ(g_n, 0);
    U(KillTimer)(NULL, id);
}
TEST(timer_hwnd_same_id_replaces) {
    NEED();
    reset();
    HWND w = tmkwnd();
    EQ(U(SetTimer)(w, 3, 5000, tproc), 3);
    EQ(U(SetTimer)(w, 3, 20, tproc2), 3);
    CHECK(wait_n(&g_n2, 1, 3000));
    EQ(g_n, 0);
    EQ(U(KillTimer)(w, 3), TRUE);
    EQ(U(KillTimer)(w, 3), FALSE);
    tkillwnd(w);
}
TEST(timer_hwnd_ids_independent_per_window) {
    NEED();
    HWND a = tmkwnd();
    HWND b = U(CreateWindowExA)(0, TCLS, "b", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(U(SetTimer)(a, 1, 5000, NULL), 1);
    EQ(U(SetTimer)(b, 1, 5000, NULL), 1);
    EQ(U(KillTimer)(a, 1), TRUE);
    EQ(U(KillTimer)(a, 1), FALSE);
    EQ(U(KillTimer)(b, 1), TRUE);
    U(DestroyWindow)(b);
    tkillwnd(a);
}
TEST(timer_kill_wrong_owner) {
    NEED();
    HWND w = tmkwnd();
    UINT_PTR nid = U(SetTimer)(NULL, 0, 5000, NULL);
    EQ(U(SetTimer)(w, 9, 5000, NULL), 9);
    EQ(U(KillTimer)(NULL, 9), FALSE);         /* belongs to the window */
    EQ(U(KillTimer)(w, nid), FALSE);          /* belongs to the thread queue */
    EQ(U(KillTimer)(w, 9), TRUE);
    EQ(U(KillTimer)(NULL, nid), TRUE);
    tkillwnd(w);
}
TEST(timer_kill_nonexistent) {
    NEED();
    SetLastError(0);
    EQ(U(KillTimer)(NULL, 0x5A5A5A), FALSE);
    t_note("KillTimer(NULL, unknown) err=%lu", GetLastError());
    EQ(U(KillTimer)(NULL, 0), FALSE);
    HWND w = tmkwnd();
    SetLastError(0);
    EQ(U(KillTimer)(w, 1234), FALSE);
    t_note("KillTimer(hwnd, unknown) err=%lu", GetLastError());
    tkillwnd(w);
}
TEST(timer_kill_invalid_hwnd) {
    NEED();
    SetLastError(0);
    EQ(U(KillTimer)((HWND)0x12345678, 1), FALSE);
    EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    EQ(U(KillTimer)((HWND)(ULONG_PTR)0xFFFFFFFF, 1), FALSE);
    t_note("KillTimer(0xFFFFFFFF, 1) err=%lu", GetLastError());
}
TEST(timer_set_invalid_hwnd) {
    NEED();
    SetLastError(0);
    EQ(U(SetTimer)((HWND)0x12345678, 1, 100, NULL), 0);
    EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    EQ(U(SetTimer)((HWND)(ULONG_PTR)0xDEAD, 1, 100, tproc), 0);
    EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
}
TEST(timer_set_on_destroyed_window) {
    NEED();
    HWND w = tmkwnd();
    U(DestroyWindow)(w);
    SetLastError(0);
    EQ(U(SetTimer)(w, 1, 100, NULL), 0);
    EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    EQ(U(KillTimer)(w, 1), FALSE);
    EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
    dh_destroy_window(NULL, TCLS);
}
TEST(timer_destroy_window_kills_timers) {
    NEED();
    reset();
    HWND w = tmkwnd();
    EQ(U(SetTimer)(w, 1, 20, tproc), 1);
    EQ(U(SetTimer)(w, 2, 20, tproc), 2);
    tkillwnd(w);
    dh_sleep_pump(150);
    EQ(g_n, 0);
}
TEST(timer_other_thread_window) {
    NEED();
    static HWND tw; static volatile LONG ready, done;
    static DWORD tid;
    DWORD WINAPI th(LPVOID p) {
        tw = U(CreateWindowExA)(0, TCLS, "t", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
        tid = GetCurrentThreadId();
        ready = 1;
        MSG m;
        while (!done) {
            if (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
            else MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
        }
        U(DestroyWindow)(tw);
        return 0;
    }
    HWND mine = tmkwnd();          /* registers the class */
    ready = done = 0;
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    DWORD end = GetTickCount() + 5000;
    while (!ready && GetTickCount() < end) Sleep(5);
    CHECK(ready);
    if (ready) {
        SetLastError(0);
        EQ(U(SetTimer)(tw, 1, 100, NULL), 0);
        t_note("SetTimer on another thread's window err=%lu", GetLastError());
        SetLastError(0);
        EQ(U(KillTimer)(tw, 1), FALSE);
    }
    done = 1;
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    tkillwnd(mine);
}
TEST(timer_kill_other_threads_queue_timer) {
    NEED();
    static volatile UINT_PTR tid_timer; static volatile LONG ready, done, killres = -1;
    DWORD WINAPI th(LPVOID p) {
        tid_timer = U(SetTimer)(NULL, 0, 5000, NULL);
        ready = 1;
        while (!done) Sleep(5);
        U(KillTimer)(NULL, tid_timer);
        return 0;
    }
    ready = done = 0; tid_timer = 0;
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    DWORD end = GetTickCount() + 5000;
    while (!ready && GetTickCount() < end) Sleep(5);
    CHECK(tid_timer != 0);
    EQ(U(KillTimer)(NULL, tid_timer), FALSE);   /* thread timers are private to the creating thread */
    done = 1;
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
}
TEST(timer_kill_inside_proc) {
    NEED();
    reset();
    UINT_PTR id = U(SetTimer)(NULL, 0, 20, tproc_selfkill);
    CHECK(wait_n(&g_n, 1, 3000));
    EQ(g_kill_ret, TRUE);
    dh_sleep_pump(150);
    EQ(g_n, 1);
    EQ(U(KillTimer)(NULL, id), FALSE);
}
TEST(timer_kill_inside_wndproc) {
    NEED();
    HWND w = tmkwnd();
    g_kill_id = 11; g_kill_ret = FALSE;
    EQ(U(SetTimer)(w, 11, 20, NULL), 11);
    CHECK(wait_n(&g_wm_timer, 1, 3000));
    EQ(g_kill_ret, TRUE);
    LONG n = g_wm_timer;
    dh_sleep_pump(150);
    EQ(g_wm_timer, n);
    tkillwnd(w);
}
TEST(timer_wm_timer_coalesced_while_idle) {
    NEED();
    HWND w = tmkwnd();
    EQ(U(SetTimer)(w, 5, 10, NULL), 5);
    Sleep(250);                                /* ~25 intervals elapse while we do not pump */
    MSG m; int n = 0;
    while (PeekMessageA(&m, w, WM_TIMER, WM_TIMER, PM_REMOVE)) { n++; if (n > 100) break; DispatchMessageA(&m); }
    t_note("WM_TIMER messages drained after idle: %d", n);
    CHECK(n >= 1);
    CHECK(n <= 3);
    U(KillTimer)(w, 5);
    tkillwnd(w);
}
TEST(timer_peek_noremove_keeps_message) {
    NEED();
    HWND w = tmkwnd();
    EQ(U(SetTimer)(w, 6, 10, NULL), 6);
    Sleep(60);
    MSG m;
    CHECK(PeekMessageA(&m, w, WM_TIMER, WM_TIMER, PM_NOREMOVE));
    EQ(m.wParam, 6);
    CHECK(PeekMessageA(&m, w, WM_TIMER, WM_TIMER, PM_NOREMOVE));
    CHECK(PeekMessageA(&m, w, WM_TIMER, WM_TIMER, PM_REMOVE));
    U(KillTimer)(w, 6);
    tkillwnd(w);
}
TEST(timer_wm_timer_lowest_priority) {
    NEED();
    HWND w = tmkwnd();
    EQ(U(SetTimer)(w, 8, 10, NULL), 8);
    Sleep(50);
    PostMessageA(w, WM_USER + 1, 0, 0);
    MSG m;
    CHECK(PeekMessageA(&m, w, 0, 0, PM_REMOVE));
    EQ(m.message, WM_USER + 1);                /* posted messages come before WM_TIMER */
    U(KillTimer)(w, 8);
    tkillwnd(w);
}
TEST(timer_two_timers_independent) {
    NEED();
    reset();
    UINT_PTR a = U(SetTimer)(NULL, 0, 20, tproc), b = U(SetTimer)(NULL, 0, 20, tproc2);
    CHECK(a && b && a != b);
    CHECK(wait_n(&g_n, 2, 3000));
    CHECK(wait_n(&g_n2, 2, 3000));
    U(KillTimer)(NULL, a);
    LONG n = g_n, n2 = g_n2;
    dh_sleep_pump(100);
    EQ(g_n, n);
    CHECK(g_n2 > n2);
    U(KillTimer)(NULL, b);
}
TEST(timer_many_timers) {
    NEED();
    enum { N = 300 };
    static UINT_PTR ids[N];
    int ok = 0;
    for (int i = 0; i < N; i++) { ids[i] = U(SetTimer)(NULL, 0, 60000, NULL); if (ids[i]) ok++; }
    CHECK(ok >= 250);
    for (int i = 0; i < N; i++) if (ids[i]) CHECKF(U(KillTimer)(NULL, ids[i]), "kill %d", i);
}
TEST(timer_id_reuse_observed) {
    NEED();
    UINT_PTR a = U(SetTimer)(NULL, 0, 60000, NULL);
    U(KillTimer)(NULL, a);
    UINT_PTR b = U(SetTimer)(NULL, 0, 60000, NULL);
    t_note("first id %llu, id after kill %llu", (unsigned long long)a, (unsigned long long)b);
    CHECK(b != 0);
    U(KillTimer)(NULL, b);
}
TEST(timer_hwnd_zero_id) {
    NEED();
    HWND w = tmkwnd();
    UINT_PTR id = U(SetTimer)(w, 0, 5000, NULL);
    t_note("SetTimer(hwnd, 0) -> %llu", (unsigned long long)id);
    BOOL k = U(KillTimer)(w, 0);
    t_note("KillTimer(hwnd, 0) -> %d", k);
    tkillwnd(w);
}
TEST(timer_hwnd_large_ids) {
    NEED();
    HWND w = tmkwnd();
    static const UINT_PTR ids[] = {0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF, 0x100000001ull, ~(UINT_PTR)0};
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        UINT_PTR r = U(SetTimer)(w, ids[i], 5000, NULL);
        t_note("SetTimer(hwnd, 0x%llx) -> 0x%llx", (unsigned long long)ids[i], (unsigned long long)r);
        CHECK(r != 0);
        if (r) U(KillTimer)(w, ids[i]);
    }
    tkillwnd(w);
}
TEST(timer_proc_on_hwnd_timer_receives_wndproc_hwnd) {
    NEED();
    reset();
    HWND w = tmkwnd();
    HWND w2 = U(CreateWindowExA)(0, TCLS, "w2", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(U(SetTimer)(w2, 99, 20, tproc), 99);
    CHECK(wait_n(&g_n, 1, 3000));
    EQ(g_h, w2);
    U(KillTimer)(w2, 99);
    U(DestroyWindow)(w2);
    tkillwnd(w);
}
TEST(timer_hidden_and_disabled_window_still_ticks) {
    NEED();
    HWND w = tmkwnd();
    EnableWindow(w, FALSE);
    EQ(U(SetTimer)(w, 1, 20, NULL), 1);
    CHECK(wait_n(&g_wm_timer, 1, 3000));
    U(KillTimer)(w, 1);
    tkillwnd(w);
}
TEST(timer_setcoalescable_basic) {
    UDECL(UINT_PTR, SetCoalescableTimerX, (HWND, UINT_PTR, UINT, TIMERPROC, ULONG));
    NEED();
    REQUIRE(U(SetCoalescableTimer));
    reset();
    UINT_PTR id = U(SetCoalescableTimer)(NULL, 0, 30, tproc, 0);
    CHECK(id != 0);
    CHECK(wait_n(&g_n, 1, 3000));
    EQ(g_id, id);
    EQ(U(KillTimer)(NULL, id), TRUE);
}
TEST(timer_setcoalescable_tolerances) {
    NEED();
    REQUIRE(U(SetCoalescableTimer));
    static const ULONG tol[] = {TIMERV_DEFAULT_COALESCING, 1, 10, 100, 1000, 100000, TIMERV_NO_COALESCING};
    for (unsigned i = 0; i < sizeof tol / sizeof tol[0]; i++) {
        UINT_PTR id = U(SetCoalescableTimer)(NULL, 0, 1000, tproc2, tol[i]);
        CHECKF(id != 0, "tolerance %lu", tol[i]);
        if (id) CHECKF(U(KillTimer)(NULL, id), "kill tolerance %lu", tol[i]);
    }
}
TEST(timer_setcoalescable_fires_within_window) {
    NEED();
    REQUIRE(U(SetCoalescableTimer));
    reset();
    DWORD t0 = GetTickCount();
    UINT_PTR id = U(SetCoalescableTimer)(NULL, 0, 100, tproc, 100);
    CHECK(id != 0);
    CHECK(wait_n(&g_n, 1, 4000));
    DWORD dt = GetTickCount() - t0;
    t_note("coalescable 100ms+100ms tolerance fired after %lu ms", dt);
    CHECK(dt >= 70);
    U(KillTimer)(NULL, id);
}
TEST(timer_setcoalescable_no_coalescing_fires) {
    NEED();
    REQUIRE(U(SetCoalescableTimer));
    reset();
    UINT_PTR id = U(SetCoalescableTimer)(NULL, 0, 30, tproc, TIMERV_NO_COALESCING);
    CHECK(id != 0);
    CHECK(wait_n(&g_n, 3, 4000));
    U(KillTimer)(NULL, id);
}
TEST(timer_setcoalescable_replace_with_hwnd) {
    NEED();
    REQUIRE(U(SetCoalescableTimer));
    HWND w = tmkwnd();
    EQ(U(SetCoalescableTimer)(w, 4, 5000, NULL, 50), 4);
    EQ(U(SetCoalescableTimer)(w, 4, 20, NULL, 0), 4);
    CHECK(wait_n(&g_wm_timer, 1, 3000));
    EQ(U(KillTimer)(w, 4), TRUE);
    tkillwnd(w);
}
TEST(timer_setcoalescable_invalid) {
    NEED();
    REQUIRE(U(SetCoalescableTimer));
    SetLastError(0);
    EQ(U(SetCoalescableTimer)((HWND)0x12345678, 1, 100, NULL, 0), 0);
    EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
    /* tolerance just beyond TIMERV_MAX_COALESCING with a small elapse */
    SetLastError(0);
    UINT_PTR id = U(SetCoalescableTimer)(NULL, 0, 100, NULL, TIMERV_MAX_COALESCING + 1);
    t_note("SetCoalescableTimer(tolerance MAX+1) -> %llu err=%lu", (unsigned long long)id, GetLastError());
    if (id) U(KillTimer)(NULL, id);
}
TEST(timer_setcoalescable_max_tolerance) {
    NEED();
    REQUIRE(U(SetCoalescableTimer));
    SetLastError(0);
    UINT_PTR id = U(SetCoalescableTimer)(NULL, 0, 100, NULL, TIMERV_MAX_COALESCING);
    t_note("SetCoalescableTimer(tolerance MAX) -> %llu err=%lu", (unsigned long long)id, GetLastError());
    if (id) U(KillTimer)(NULL, id);
}
TEST(timer_constants) {
    EQ(USER_TIMER_MINIMUM, 0x0A);
    EQ(USER_TIMER_MAXIMUM, 0x7FFFFFFF);
    EQ(TIMERV_DEFAULT_COALESCING, 0);
    EQ(TIMERV_NO_COALESCING, 0xFFFFFFFF);
    EQ(TIMERV_MAX_COALESCING, 0x7FFFFFF5);
    EQ(WM_TIMER, 0x113);
}
TEST(timer_survive_get_message_quit) {
    NEED();
    reset();
    UINT_PTR id = U(SetTimer)(NULL, 0, 20, tproc);
    PostQuitMessage(0);
    MSG m; int quit = 0;
    DWORD end = GetTickCount() + 3000;
    while (!quit && GetTickCount() < end) {
        if (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { if (m.message == WM_QUIT) quit = 1; else DispatchMessageA(&m); }
        else Sleep(2);
    }
    CHECK(quit);
    U(KillTimer)(NULL, id);
}
