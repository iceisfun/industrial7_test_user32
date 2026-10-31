/* SetWinEventHook / UnhookWinEvent / NotifyWinEvent / IsWinEventHookInstalled */
#include "wsp.h"

#define EVLOG 128
typedef struct { HWINEVENTHOOK h; DWORD ev; HWND hwnd; LONG obj, child; DWORD thr, tid; } wev;
static wev g_ev[EVLOG];
static volatile LONG g_nev;
static void ev_reset(void) { g_nev = 0; memset(g_ev, 0, sizeof g_ev); }
static void CALLBACK ev_proc(HWINEVENTHOOK h, DWORD ev, HWND hwnd, LONG obj, LONG child, DWORD thr, DWORD time) {
    LONG i = InterlockedIncrement(&g_nev) - 1;
    if (i >= EVLOG) return;
    g_ev[i].h = h; g_ev[i].ev = ev; g_ev[i].hwnd = hwnd; g_ev[i].obj = obj; g_ev[i].child = child; g_ev[i].thr = thr; g_ev[i].tid = GetCurrentThreadId();
}
static int ev_count(DWORD ev, HWND hwnd) {
    int c = 0;
    for (LONG i = 0; i < g_nev && i < EVLOG; i++) if (g_ev[i].ev == ev && (!hwnd || g_ev[i].hwnd == hwnd)) c++;
    return c;
}
static wev *ev_find(DWORD ev, HWND hwnd, int nth) {
    for (LONG i = 0; i < g_nev && i < EVLOG; i++) if (g_ev[i].ev == ev && (!hwnd || g_ev[i].hwnd == hwnd) && nth-- == 0) return &g_ev[i];
    return NULL;
}
/* pump until at least n events logged or timeout */
static void ev_wait(LONG n, DWORD ms) {
    DWORD t0 = GetTickCount();
    for (;;) {
        t_pump();
        if (g_nev >= n || GetTickCount() - t0 >= ms) return;
        Sleep(5);
    }
}
static HWINEVENTHOOK hook_self(DWORD lo, DWORD hi, DWORD flags) {
    return U(SetWinEventHook)(lo, hi, NULL, ev_proc, GetCurrentProcessId(), 0, flags);
}
#define NEED_WE() do { REQUIRE(U(SetWinEventHook)); REQUIRE(U(UnhookWinEvent)); } while (0)

/* ---------- error paths ---------- */
TEST(winsta_process_winevent_null_proc) {
    NEED_WE();
    SetLastError(0);
    HWINEVENTHOOK h = U(SetWinEventHook)(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, NULL, NULL, 0, 0, WINEVENT_OUTOFCONTEXT);
    CHECK(h == NULL);
    t_note("SetWinEventHook(NULL proc) err %lu", GetLastError());
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    if (h) U(UnhookWinEvent)(h);
}
TEST(winsta_process_winevent_min_greater_than_max) {
    NEED_WE();
    SetLastError(0);
    HWINEVENTHOOK h = U(SetWinEventHook)(EVENT_OBJECT_DESTROY, EVENT_OBJECT_CREATE, NULL, ev_proc, GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
    CHECK(h == NULL);
    t_note("SetWinEventHook(min>max) err %lu", GetLastError());
    if (h) U(UnhookWinEvent)(h);
}
TEST(winsta_process_winevent_invalid_flags) {
    NEED_WE();
    static const DWORD bad[] = { 0x8, 0x10, 0x80, 0x100, 0x10000, 0x80000000u };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        HWINEVENTHOOK h = U(SetWinEventHook)(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, NULL, ev_proc, GetCurrentProcessId(), 0, bad[i]);
        t_note("flags 0x%lx -> %p", (unsigned long)bad[i], (void*)h);
        t_note("SetWinEventHook flags 0x%lx err %lu", (unsigned long)bad[i], GetLastError());
        if (h) U(UnhookWinEvent)(h);
    }
}
TEST(winsta_process_winevent_incontext_without_module) {
    NEED_WE();
    SetLastError(0);
    HWINEVENTHOOK h = U(SetWinEventHook)(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, NULL, ev_proc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);
    t_note("INCONTEXT without hmod -> %p err %lu", h, GetLastError());
    CHECK(h == NULL);
    if (h) U(UnhookWinEvent)(h);
}
TEST(winsta_process_winevent_incontext_bad_module) {
    NEED_WE();
    SetLastError(0);
    HWINEVENTHOOK h = U(SetWinEventHook)(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, (HMODULE)(ULONG_PTR)0x10, ev_proc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);
    CHECK(h == NULL);   /* GetModuleFileNameW on a bogus module fails first */
    t_note("INCONTEXT bogus hmod err %lu", GetLastError());
    if (h) U(UnhookWinEvent)(h);
}
TEST(winsta_process_winevent_incontext_skip_own_process) {
    NEED_WE();
    SetLastError(0);
    HWINEVENTHOOK h = U(SetWinEventHook)(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, GetModuleHandleA(NULL), ev_proc, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT | WINEVENT_SKIPOWNPROCESS);
    t_note("INCONTEXT|SKIPOWNPROCESS with exe module -> %p err %lu", h, GetLastError());
    if (h) CHECK(U(UnhookWinEvent)(h));
}
TEST(winsta_process_unhookwinevent_invalid) {
    NEED_WE();
    SetLastError(0);
    CHECK(!U(UnhookWinEvent)(NULL));
    t_note("UnhookWinEvent(NULL) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(UnhookWinEvent)((HWINEVENTHOOK)(ULONG_PTR)0x1234));
    t_note("UnhookWinEvent(0x1234) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(UnhookWinEvent)((HWINEVENTHOOK)INVALID_HANDLE_VALUE));
    CHECK(!U(UnhookWinEvent)((HWINEVENTHOOK)(ULONG_PTR)0xdeadbeefdeadull));
}
TEST(winsta_process_unhookwinevent_twice) {
    NEED_WE();
    HWINEVENTHOOK h = hook_self(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, WINEVENT_OUTOFCONTEXT);
    CHECK(h != NULL);
    CHECK(U(UnhookWinEvent)(h));
    SetLastError(0);
    CHECK(!U(UnhookWinEvent)(h));
    t_note("second UnhookWinEvent err %lu", GetLastError());
}
TEST(winsta_process_unhookwinevent_hook_handle_not_hhook) {
    NEED_WE(); REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(UnhookWindowsHookEx));
    HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, (HOOKPROC)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetTickCount"), NULL, GetCurrentThreadId());
    if (!h) { t_skip("no hook"); return; }
    t_note("UnhookWinEvent(plain hook) -> %d", U(UnhookWinEvent)((HWINEVENTHOOK)h));
    U(UnhookWindowsHookEx)(h);
    HWINEVENTHOOK e = hook_self(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, WINEVENT_OUTOFCONTEXT);
    SetLastError(0);
    t_note("UnhookWindowsHookEx(winevent hook) -> %d err %lu", U(UnhookWindowsHookEx)((HHOOK)e), GetLastError());
    if (e) U(UnhookWinEvent)(e);
}

/* ---------- install / ranges ---------- */
TEST(winsta_process_winevent_install_ranges) {
    NEED_WE();
    static const DWORD r[][2] = {
        { EVENT_MIN, EVENT_MAX }, { EVENT_SYSTEM_SOUND, EVENT_SYSTEM_ALERT }, { EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE },
        { EVENT_OBJECT_CREATE, EVENT_OBJECT_END }, { EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND },
        { EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE }, { 0xA000, 0xAFFF }, { EVENT_OEM_DEFINED_START, EVENT_OEM_DEFINED_END },
        { EVENT_UIA_EVENTID_START, EVENT_UIA_EVENTID_END }, { EVENT_UIA_PROPID_START, EVENT_UIA_PROPID_END },
    };
    for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) {
        SetLastError(0);
        HWINEVENTHOOK h = hook_self(r[i][0], r[i][1], WINEVENT_OUTOFCONTEXT);
        CHECKF(h != NULL, "range [0x%lx,0x%lx] err %lu", (unsigned long)r[i][0], (unsigned long)r[i][1], GetLastError());
        if (h) CHECK(U(UnhookWinEvent)(h));
    }
}
TEST(winsta_process_winevent_install_zero_range) {
    NEED_WE();
    SetLastError(0);
    HWINEVENTHOOK h = hook_self(0, 0, WINEVENT_OUTOFCONTEXT);
    t_note("SetWinEventHook(0,0) -> %p err %lu", h, GetLastError());
    if (h) U(UnhookWinEvent)(h);
    h = hook_self(0, 0x7fffffff, WINEVENT_OUTOFCONTEXT);
    t_note("SetWinEventHook(0,0x7fffffff) -> %p err %lu", h, GetLastError());
    if (h) U(UnhookWinEvent)(h);
    h = hook_self(0x80000000u, 0xffffffffu, WINEVENT_OUTOFCONTEXT);
    t_note("SetWinEventHook(0x80000000,0xffffffff) -> %p err %lu", h, GetLastError());
    if (h) U(UnhookWinEvent)(h);
}
TEST(winsta_process_winevent_flag_combinations) {
    NEED_WE();
    static const DWORD f[] = { 0, WINEVENT_SKIPOWNTHREAD, WINEVENT_SKIPOWNPROCESS, WINEVENT_SKIPOWNTHREAD | WINEVENT_SKIPOWNPROCESS };
    for (unsigned i = 0; i < 4; i++) {
        HWINEVENTHOOK h = hook_self(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, f[i]);
        CHECKF(h != NULL, "flags 0x%lx", (unsigned long)f[i]);
        if (h) CHECK(U(UnhookWinEvent)(h));
    }
}
TEST(winsta_process_winevent_handles_distinct) {
    NEED_WE();
    HWINEVENTHOOK hs[40]; int n = 0;
    for (int i = 0; i < 40; i++) { hs[i] = hook_self(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, WINEVENT_OUTOFCONTEXT); if (hs[i]) n++; }
    EXPECT_EQ(n, 40);
    for (int i = 0; i < 40; i++) for (int j = i + 1; j < 40; j++) if (hs[i] && hs[i] == hs[j]) CHECK(0);
    for (int i = 0; i < 40; i++) if (hs[i]) CHECK(U(UnhookWinEvent)(hs[i]));
}
TEST(winsta_process_winevent_idthread_bogus) {
    NEED_WE();
    SetLastError(0);
    HWINEVENTHOOK h = U(SetWinEventHook)(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, NULL, ev_proc, 0, 0xfffffff0, WINEVENT_OUTOFCONTEXT);
    t_note("SetWinEventHook(bogus thread) -> %p err %lu", h, GetLastError());
    if (h) U(UnhookWinEvent)(h);
    h = U(SetWinEventHook)(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, NULL, ev_proc, 0xfffffff0, 0, WINEVENT_OUTOFCONTEXT);
    t_note("SetWinEventHook(bogus pid) -> %p err %lu", h, GetLastError());
    if (h) U(UnhookWinEvent)(h);
}
TEST(winsta_process_iswineventhookinstalled_after_install) {
    NEED_WE(); REQUIRE(U(IsWinEventHookInstalled)); IsGUIThread(TRUE);
    static const DWORD evs[] = { EVENT_OBJECT_CREATE, EVENT_OBJECT_NAMECHANGE, EVENT_SYSTEM_FOREGROUND, 0xA001 };
    for (unsigned i = 0; i < 4; i++) {
        BOOL before = U(IsWinEventHookInstalled)(evs[i]);
        HWINEVENTHOOK h = hook_self(evs[i], evs[i], WINEVENT_OUTOFCONTEXT);
        CHECK(h != NULL);
        CHECKF(U(IsWinEventHookInstalled)(evs[i]), "event 0x%lx", (unsigned long)evs[i]);
        t_note("IsWinEventHookInstalled(0x%lx) before=%d", (unsigned long)evs[i], before);
        if (h) U(UnhookWinEvent)(h);
    }
}
TEST(winsta_process_iswineventhookinstalled_range_edges) {
    NEED_WE(); REQUIRE(U(IsWinEventHookInstalled)); IsGUIThread(TRUE);
    HWINEVENTHOOK h = hook_self(EVENT_MIN, EVENT_MAX, WINEVENT_OUTOFCONTEXT);
    CHECK(h != NULL);
    CHECK(U(IsWinEventHookInstalled)(EVENT_MIN));
    CHECK(U(IsWinEventHookInstalled)(EVENT_SYSTEM_MENUSTART));
    CHECK(U(IsWinEventHookInstalled)(EVENT_OBJECT_SHOW));
    CHECK(U(IsWinEventHookInstalled)(EVENT_OBJECT_LOCATIONCHANGE));
    if (h) U(UnhookWinEvent)(h);
}

/* ---------- delivery ---------- */
TEST(winsta_process_winevent_window_create_destroy) {
    NEED_WE(); REQUIRE(U(DestroyWindow));
    ev_reset();
    HWINEVENTHOOK h = hook_self(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, WINEVENT_OUTOFCONTEXT);
    CHECK(h != NULL);
    HWND w = t_make_window(NULL, WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, -32000, -32000, 20, 20, NULL);
    CHECK(w != NULL);
    for (int i = 0; i < 400 && !ev_count(EVENT_OBJECT_CREATE, w); i++) { t_pump(); Sleep(5); }
    EXPECT_EQ(ev_count(EVENT_OBJECT_CREATE, w), 1);
    wev *e = ev_find(EVENT_OBJECT_CREATE, w, 0);
    if (e) { EXPECT_EQ(e->obj, OBJID_WINDOW); EXPECT_EQ(e->child, CHILDID_SELF); EXPECT_EQ(e->h, h); EXPECT_EQ(e->tid, GetCurrentThreadId()); EXPECT_EQ(e->thr, GetCurrentThreadId()); }
    if (ev_count(EVENT_OBJECT_SHOW, w) == 0) {}
    U(DestroyWindow)(w);
    for (int i = 0; i < 400 && !ev_count(EVENT_OBJECT_DESTROY, w); i++) { t_pump(); Sleep(5); }
    EXPECT_EQ(ev_count(EVENT_OBJECT_DESTROY, w), 1);
    e = ev_find(EVENT_OBJECT_DESTROY, w, 0);
    if (e) { EXPECT_EQ(e->obj, OBJID_WINDOW); EXPECT_EQ(e->child, CHILDID_SELF); }
    /* create must precede destroy */
    int ic = -1, id = -1;
    for (LONG i = 0; i < g_nev; i++) { if (g_ev[i].ev == EVENT_OBJECT_CREATE && g_ev[i].hwnd == w) ic = (int)i; if (g_ev[i].ev == EVENT_OBJECT_DESTROY && g_ev[i].hwnd == w) id = (int)i; }
    CHECK(ic >= 0 && id > ic);
    CHECK(U(UnhookWinEvent)(h));
}
TEST(winsta_process_winevent_show_hide) {
    NEED_WE(); REQUIRE(U(ShowWindow));
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, -32000, -32000, 20, 20, NULL);
    CHECK(w != NULL);
    ev_reset();
    HWINEVENTHOOK h = hook_self(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE, WINEVENT_OUTOFCONTEXT);
    U(ShowWindow)(w, SW_SHOWNA);
    for (int i = 0; i < 400 && !ev_count(EVENT_OBJECT_SHOW, w); i++) { t_pump(); Sleep(5); }
    EXPECT_EQ(ev_count(EVENT_OBJECT_SHOW, w), 1);
    U(ShowWindow)(w, SW_HIDE);
    for (int i = 0; i < 400 && !ev_count(EVENT_OBJECT_HIDE, w); i++) { t_pump(); Sleep(5); }
    EXPECT_EQ(ev_count(EVENT_OBJECT_HIDE, w), 1);
    wev *e = ev_find(EVENT_OBJECT_SHOW, w, 0);
    if (e) { EXPECT_EQ(e->obj, OBJID_WINDOW); EXPECT_EQ(e->child, CHILDID_SELF); }
    if (h) CHECK(U(UnhookWinEvent)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_notify_custom) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, -32000, -32000, 20, 20, NULL);
    CHECK(w != NULL);
    ev_reset();
    HWINEVENTHOOK h = hook_self(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE, WINEVENT_OUTOFCONTEXT);
    CHECK(h != NULL);
    U(NotifyWinEvent)(EVENT_OBJECT_NAMECHANGE, w, OBJID_CLIENT, 3);
    ev_wait(1, 2000);
    EXPECT_EQ(ev_count(EVENT_OBJECT_NAMECHANGE, w), 1);
    wev *e = ev_find(EVENT_OBJECT_NAMECHANGE, w, 0);
    if (e) { EXPECT_EQ(e->obj, OBJID_CLIENT); EXPECT_EQ(e->child, 3); EXPECT_EQ(e->tid, GetCurrentThreadId()); EXPECT_EQ(e->h, h); }
    /* event outside the subscribed range is not delivered */
    U(NotifyWinEvent)(EVENT_OBJECT_VALUECHANGE, w, OBJID_CLIENT, 4);
    wsp_pump_until(NULL, 150);
    EXPECT_EQ(ev_count(EVENT_OBJECT_VALUECHANGE, NULL), 0);
    EXPECT_EQ(g_nev, 1);
    if (h) CHECK(U(UnhookWinEvent)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_notify_no_hook_nothing) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWND w = t_make_window(NULL, WS_POPUP, 0, -32000, -32000, 20, 20, NULL);
    ev_reset();
    HWINEVENTHOOK h = hook_self(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE, WINEVENT_OUTOFCONTEXT);
    U(UnhookWinEvent)(h);
    U(NotifyWinEvent)(EVENT_OBJECT_NAMECHANGE, w, OBJID_CLIENT, 1);
    wsp_pump_until(NULL, 150);
    EXPECT_EQ(g_nev, 0);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_notify_order_preserved) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW, -32000, -32000, 20, 20, NULL);
    ev_reset();
    HWINEVENTHOOK h = hook_self(0xA000, 0xAFFF, WINEVENT_OUTOFCONTEXT);
    CHECK(h != NULL);
    for (int i = 1; i <= 8; i++) U(NotifyWinEvent)(0xA000 + i, w, OBJID_CLIENT, i);
    ev_wait(8, 3000);
    EXPECT_EQ(g_nev, 8);
    for (int i = 0; i < 8 && i < g_nev; i++) { EXPECT_EQ(g_ev[i].ev, 0xA001 + i); EXPECT_EQ(g_ev[i].child, i + 1); EXPECT_EQ(g_ev[i].hwnd, w); }
    if (h) U(UnhookWinEvent)(h);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_two_hooks_both_called) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW, -32000, -32000, 20, 20, NULL);
    ev_reset();
    HWINEVENTHOOK a = hook_self(0xA000, 0xAFFF, WINEVENT_OUTOFCONTEXT), b = hook_self(0xA010, 0xA020, WINEVENT_OUTOFCONTEXT);
    U(NotifyWinEvent)(0xA011, w, OBJID_CLIENT, 0);
    U(NotifyWinEvent)(0xA001, w, OBJID_CLIENT, 0);
    ev_wait(3, 2000);
    EXPECT_EQ(g_nev, 3);
    int na = 0, nb = 0;
    for (LONG i = 0; i < g_nev && i < EVLOG; i++) { if (g_ev[i].h == a) na++; if (g_ev[i].h == b) nb++; }
    EXPECT_EQ(na, 2); EXPECT_EQ(nb, 1);
    if (a) CHECK(U(UnhookWinEvent)(a));
    if (b) CHECK(U(UnhookWinEvent)(b));
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_skipownthread) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW, -32000, -32000, 20, 20, NULL);
    ev_reset();
    HWINEVENTHOOK h = hook_self(0xA000, 0xAFFF, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNTHREAD);
    CHECK(h != NULL);
    U(NotifyWinEvent)(0xA005, w, OBJID_CLIENT, 0);
    wsp_pump_until(NULL, 200);
    EXPECT_EQ(g_nev, 0);
    if (h) U(UnhookWinEvent)(h);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_skipownprocess) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW, -32000, -32000, 20, 20, NULL);
    ev_reset();
    HWINEVENTHOOK h = hook_self(0xA000, 0xAFFF, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    CHECK(h != NULL);
    U(NotifyWinEvent)(0xA005, w, OBJID_CLIENT, 0);
    wsp_pump_until(NULL, 200);
    EXPECT_EQ(g_nev, 0);
    if (h) U(UnhookWinEvent)(h);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_thread_filter_excludes) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW, -32000, -32000, 20, 20, NULL);
    ev_reset();
    HWINEVENTHOOK h = U(SetWinEventHook)(0xA000, 0xAFFF, NULL, ev_proc, GetCurrentProcessId(), GetCurrentThreadId() + 4, WINEVENT_OUTOFCONTEXT);
    t_note("SetWinEventHook with idThread = other id -> %p err %lu", h, GetLastError());
    if (h) {
        U(NotifyWinEvent)(0xA005, w, OBJID_CLIENT, 0);
        wsp_pump_until(NULL, 200);
        EXPECT_EQ(g_nev, 0);
        U(UnhookWinEvent)(h);
    }
    h = U(SetWinEventHook)(0xA000, 0xAFFF, NULL, ev_proc, GetCurrentProcessId(), GetCurrentThreadId(), WINEVENT_OUTOFCONTEXT);
    CHECK(h != NULL);
    U(NotifyWinEvent)(0xA005, w, OBJID_CLIENT, 0);
    ev_wait(1, 2000);
    EXPECT_EQ(g_nev, 1);
    if (h) U(UnhookWinEvent)(h);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_not_delivered_after_unhook) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW, -32000, -32000, 20, 20, NULL);
    ev_reset();
    HWINEVENTHOOK h = hook_self(0xA000, 0xAFFF, WINEVENT_OUTOFCONTEXT);
    U(NotifyWinEvent)(0xA005, w, OBJID_CLIENT, 0);
    /* unhook before the queued event is dispatched */
    CHECK(U(UnhookWinEvent)(h));
    wsp_pump_until(NULL, 200);
    t_note("events delivered after immediate unhook: %ld", (long)g_nev);
    U(NotifyWinEvent)(0xA006, w, OBJID_CLIENT, 0);
    wsp_pump_until(NULL, 150);
    CHECK(ev_count(0xA006, NULL) == 0);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_winevent_notify_bad_args_no_crash) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    HWINEVENTHOOK h = hook_self(0xA000, 0xAFFF, WINEVENT_OUTOFCONTEXT);
    ev_reset();
    U(NotifyWinEvent)(0xA001, NULL, 0, 0);
    U(NotifyWinEvent)(0xA002, (HWND)(ULONG_PTR)0x12345678, OBJID_WINDOW, 0);
    U(NotifyWinEvent)(0xA003, NULL, 0x7fffffff, 0x7fffffff);
    U(NotifyWinEvent)(0xA004, NULL, (LONG)0x80000000, (LONG)0x80000000);
    U(NotifyWinEvent)(0x80000001u, NULL, 0, 0);
    U(NotifyWinEvent)(0, NULL, 0, 0);
    wsp_pump_until(NULL, 200);
    t_note("bad-arg NotifyWinEvent delivered %ld events", (long)g_nev);
    if (h) U(UnhookWinEvent)(h);
}
TEST(winsta_process_winevent_notify_hwnd_null_delivery) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent));
    ev_reset();
    HWINEVENTHOOK h = hook_self(0xA000, 0xAFFF, WINEVENT_OUTOFCONTEXT);
    U(NotifyWinEvent)(0xA001, NULL, OBJID_CURSOR, 7);
    ev_wait(1, 1000);
    t_note("NotifyWinEvent(hwnd NULL) delivered %ld events", (long)g_nev);
    wev *e = ev_find(0xA001, NULL, 0);
    if (e) { EXPECT_EQ(e->hwnd, NULL); EXPECT_EQ(e->obj, OBJID_CURSOR); EXPECT_EQ(e->child, 7); }
    if (h) U(UnhookWinEvent)(h);
}

typedef struct { HANDLE ready, go, th; DWORD tid; HWINEVENTHOOK h; HWND w; } wew;
static DWORD WINAPI wew_thread(LPVOID p) {
    wew *x = p; MSG m;
    x->w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW, -32000, -32000, 20, 20, NULL);
    x->h = U(SetWinEventHook)(0xA000, 0xAFFF, NULL, ev_proc, GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
    SetEvent(x->ready);
    while (WaitForSingleObject(x->go, 0) != WAIT_OBJECT_0) {
        while (U(PeekMessageA)(&m, NULL, 0, 0, PM_REMOVE)) U(DispatchMessageA)(&m);
        Sleep(1);
    }
    if (x->h) U(UnhookWinEvent)(x->h);
    if (x->w) U(DestroyWindow)(x->w);
    return 0;
}
TEST(winsta_process_winevent_delivered_to_installing_thread) {
    NEED_WE(); REQUIRE(U(NotifyWinEvent)); REQUIRE(U(DispatchMessageA));
    wew x; memset(&x, 0, sizeof x);
    x.ready = CreateEventA(NULL, TRUE, FALSE, NULL); x.go = CreateEventA(NULL, TRUE, FALSE, NULL);
    ev_reset();
    x.th = CreateThread(NULL, 0, wew_thread, &x, 0, &x.tid);
    CHECK(WaitForSingleObject(x.ready, 8000) == WAIT_OBJECT_0);
    CHECK(x.h != NULL);
    U(NotifyWinEvent)(0xA001, x.w, OBJID_CLIENT, 9);
    for (int i = 0; i < 400 && !g_nev; i++) Sleep(5);
    EXPECT_EQ(g_nev, 1);
    if (g_nev > 0) { EXPECT_EQ(g_ev[0].tid, x.tid); EXPECT_EQ(g_ev[0].child, 9); EXPECT_EQ(g_ev[0].thr, GetCurrentThreadId()); }
    SetEvent(x.go);
    WaitForSingleObject(x.th, 8000);
    CloseHandle(x.th); CloseHandle(x.ready); CloseHandle(x.go);
}
TEST(winsta_process_winevent_hook_dies_with_thread) {
    NEED_WE();
    wew x; memset(&x, 0, sizeof x);
    x.ready = CreateEventA(NULL, TRUE, FALSE, NULL); x.go = CreateEventA(NULL, TRUE, FALSE, NULL);
    x.th = CreateThread(NULL, 0, wew_thread, &x, 0, &x.tid);
    CHECK(WaitForSingleObject(x.ready, 8000) == WAIT_OBJECT_0);
    HWINEVENTHOOK h = x.h;
    CHECK(h != NULL);
    SetEvent(x.go);
    WaitForSingleObject(x.th, 8000);
    SetLastError(0);
    BOOL r = U(UnhookWinEvent)(h);
    t_note("UnhookWinEvent after installing thread exited: ret %d err %lu", r, GetLastError());
    CloseHandle(x.th); CloseHandle(x.ready); CloseHandle(x.go);
}
static DWORD WINAPI unhook_other_thr(LPVOID p) { *(BOOL *)p = U(UnhookWinEvent)(*((HWINEVENTHOOK *)p + 1)); return 0; }
TEST(winsta_process_winevent_unhook_from_other_thread) {
    NEED_WE();
    struct { BOOL r; HWINEVENTHOOK h; } a;
    a.h = hook_self(0xA000, 0xAFFF, WINEVENT_OUTOFCONTEXT);
    CHECK(a.h != NULL);
    DWORD tid;
    HANDLE th = CreateThread(NULL, 0, unhook_other_thr, &a, 0, &tid);
    WaitForSingleObject(th, 8000); CloseHandle(th);
    t_note("UnhookWinEvent from a different thread ret %d", a.r);
    if (!a.r) CHECK(U(UnhookWinEvent)(a.h));
}
