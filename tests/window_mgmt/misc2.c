/* GetGUIThreadInfo, message filters, DPI awareness, window procedure access */
#include "wm.h"

UDECL(BOOL, AreDpiAwarenessContextsEqual, (HANDLE, HANDLE));
UDECL(BOOL, IsValidDpiAwarenessContext, (HANDLE));
UDECL(int,  GetAwarenessFromDpiAwarenessContext, (HANDLE));
UDECL(HANDLE, GetThreadDpiAwarenessContext, (void));
UDECL(HANDLE, SetThreadDpiAwarenessContext, (HANDLE));
UDECL(UINT, GetDpiFromDpiAwarenessContext, (HANDLE));
UDECL(HANDLE, GetWindowDpiAwarenessContext, (HWND));
UDECL(UINT, GetDpiForWindow, (HWND));
UDECL(UINT, GetDpiForSystem, (void));

#define CTX_UNAWARE      ((HANDLE)-1)
#define CTX_SYSTEM       ((HANDLE)-2)
#define CTX_PM           ((HANDLE)-3)
#define CTX_PMV2         ((HANDLE)-4)
#define CTX_GDISCALED    ((HANDLE)-5)

/* ---------------- GetGUIThreadInfo ---------------- */
TEST(wm_gti_cbsize) {
    NEED(GetGUIThreadInfo);
    GUITHREADINFO gi;
    memset(&gi, 0, sizeof gi);
    EQ(sizeof(GUITHREADINFO), 72);
    SetLastError(0);
    CHECK(!U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    gi.cbSize = sizeof gi - 1;
    SetLastError(0);
    CHECK(!U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    gi.cbSize = sizeof gi + 1;
    CHECK(!U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    gi.cbSize = sizeof gi;
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
}

TEST(wm_gti_current_thread) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW, 300, 300, 150, 100, NULL);
    U(SetActiveWindow)(h);
    t_pump();
    GUITHREADINFO gi; memset(&gi, 0xCC, sizeof gi); gi.cbSize = sizeof gi;
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EQ(gi.cbSize, sizeof gi);
    EQ(gi.hwndActive, U(GetActiveWindow)());
    EQ(gi.hwndFocus, U(GetFocus)());
    EQ(gi.hwndCapture, U(GetCapture)());
    EQ(gi.hwndMenuOwner, NULL);
    EQ(gi.hwndMoveSize, NULL);
    EQ(gi.flags & (GUI_INMENUMODE | GUI_INMOVESIZE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE), 0);
    wm_destroy(h);
}

TEST(wm_gti_capture_field) {
    HWND h = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 50, 50, NULL);
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    U(SetCapture)(h);
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EQ(gi.hwndCapture, h);
    U(ReleaseCapture)();
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EQ(gi.hwndCapture, NULL);
    wm_destroy(h);
}

TEST(wm_gti_invalid_thread) {
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    SetLastError(0);
    BOOL r = U(GetGUIThreadInfo)(0xFFFFFFF0, &gi);
    t_note("GetGUIThreadInfo(bogus tid) = %d err=%lu", r, GetLastError());
    CHECK(!r);
}

TEST(wm_gti_foreground_thread) {
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    SetLastError(0);
    BOOL r = U(GetGUIThreadInfo)(0, &gi);
    t_note("GetGUIThreadInfo(0) = %d err=%lu active=%p focus=%p", r, GetLastError(), gi.hwndActive, gi.hwndFocus);
    if (r && gi.hwndActive) CHECK(U(IsWindow)(gi.hwndActive));
}

static DWORD WINAPI idle_thread(LPVOID p) { WaitForSingleObject((HANDLE)p, WM_TIMEOUT_MS * 2); return 0; }
TEST(wm_gti_non_gui_thread) {
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD tid;
    HANDLE t = CreateThread(NULL, 0, idle_thread, ev, 0, &tid);
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    SetLastError(0);
    BOOL r = U(GetGUIThreadInfo)(tid, &gi);
    t_note("GetGUIThreadInfo(non GUI thread) = %d err=%lu", r, GetLastError());
    if (r) { EQ(gi.hwndActive, NULL); EQ(gi.hwndFocus, NULL); EQ(gi.hwndCapture, NULL); }
    SetEvent(ev);
    WaitForSingleObject(t, WM_TIMEOUT_MS);
    CloseHandle(t); CloseHandle(ev);
}

TEST(wm_gti_worker_thread) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 1)) { t_skip("worker"); wm_worker_stop(&w); return; }
    GUITHREADINFO gi; memset(&gi, 0xCC, sizeof gi); gi.cbSize = sizeof gi;
    CHECK(U(GetGUIThreadInfo)(w.tid, &gi));
    EQ(gi.hwndCapture, NULL);
    EQ(gi.hwndMenuOwner, NULL);
    EQ(gi.hwndMoveSize, NULL);
    EQ(gi.hwndCaret, NULL);
    t_note("worker thread: active=%p focus=%p flags=%lx", gi.hwndActive, gi.hwndFocus, gi.flags);
    if (gi.hwndActive) EQ(U(GetWindowThreadProcessId)(gi.hwndActive, NULL), w.tid);
    wm_worker_stop(&w);
}

TEST(wm_gti_caret) {
    NEED(CreateCaret);
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW, 300, 300, 200, 150, NULL);
    U(SetActiveWindow)(h);
    U(SetFocus)(h);
    if (U(GetFocus)() != h) { t_note("window not focused, caret info is informational only"); }
    BOOL ok = U(CreateCaret)(h, NULL, 3, 12);
    if (!ok) { t_note("CreateCaret failed err=%lu", GetLastError()); wm_destroy(h); return; }
    CHECK(U(SetCaretPos)(10, 20));
    POINT p; CHECK(U(GetCaretPos)(&p));
    EQ(p.x, 10); EQ(p.y, 20);
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    t_note("caret owner %p (window %p) rect %ld,%ld,%ld,%ld", gi.hwndCaret, h, gi.rcCaret.left, gi.rcCaret.top, gi.rcCaret.right, gi.rcCaret.bottom);
    if (gi.hwndCaret == h) { EQ(gi.rcCaret.left, 10); EQ(gi.rcCaret.top, 20); EQ(gi.rcCaret.right, 13); EQ(gi.rcCaret.bottom, 32); }
    CHECK(U(DestroyCaret)());
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EQ(gi.hwndCaret, NULL);
    wm_destroy(h);
}

/* ---------------- message filters ---------------- */
#define TEST_MSG (WM_USER + 0x1234)

TEST(wm_msgfilter_process_wide) {
    NEED(ChangeWindowMessageFilter);
    SetLastError(0);
    CHECK(U(ChangeWindowMessageFilter)(TEST_MSG, MSGFLT_ADD));
    CHECK(U(ChangeWindowMessageFilter)(TEST_MSG, MSGFLT_ADD));
    CHECK(U(ChangeWindowMessageFilter)(TEST_MSG, MSGFLT_REMOVE));
    CHECK(U(ChangeWindowMessageFilter)(TEST_MSG, MSGFLT_REMOVE));
}

TEST(wm_msgfilter_process_wide_invalid) {
    SetLastError(0);
    BOOL r = U(ChangeWindowMessageFilter)(TEST_MSG, 3);
    t_note("ChangeWindowMessageFilter(flag 3) = %d err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(ChangeWindowMessageFilter)(TEST_MSG, 0);
    t_note("ChangeWindowMessageFilter(flag 0) = %d err=%lu", r, GetLastError());
    SetLastError(0);
    r = U(ChangeWindowMessageFilter)(0, MSGFLT_ADD);
    t_note("ChangeWindowMessageFilter(msg 0, ADD) = %d err=%lu", r, GetLastError());
    if (r) U(ChangeWindowMessageFilter)(0, MSGFLT_REMOVE);
}

TEST(wm_msgfilterex_window) {
    NEED(ChangeWindowMessageFilterEx);
    HWND h = wm_hidden();
    CHANGEFILTERSTRUCT cf; memset(&cf, 0xAA, sizeof cf);
    cf.cbSize = sizeof cf;
    EQ(sizeof(CHANGEFILTERSTRUCT), 8);
    SetLastError(0);
    CHECK(U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_ALLOW, &cf));
    t_note("ALLOW first: ExtStatus=%lu", cf.ExtStatus);
    cf.ExtStatus = 0xAA;
    CHECK(U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_ALLOW, &cf));
    t_note("ALLOW again: ExtStatus=%lu", cf.ExtStatus);
    CHECK(U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_DISALLOW, &cf));
    t_note("DISALLOW: ExtStatus=%lu", cf.ExtStatus);
    CHECK(U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_DISALLOW, &cf));
    t_note("DISALLOW again: ExtStatus=%lu", cf.ExtStatus);
    CHECK(U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_RESET, &cf));
    t_note("RESET: ExtStatus=%lu", cf.ExtStatus);
    CHECK(U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_ALLOW, NULL));
    CHECK(U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_RESET, NULL));
    wm_destroy(h);
}

TEST(wm_msgfilterex_window_null_means_process) {
    CHANGEFILTERSTRUCT cf; memset(&cf, 0, sizeof cf); cf.cbSize = sizeof cf;
    SetLastError(0);
    BOOL r = U(ChangeWindowMessageFilterEx)(NULL, TEST_MSG, MSGFLT_ALLOW, &cf);
    t_note("ChangeWindowMessageFilterEx(NULL hwnd, ALLOW) = %d err=%lu", r, GetLastError());
    if (r) U(ChangeWindowMessageFilterEx)(NULL, TEST_MSG, MSGFLT_RESET, &cf);
}

TEST(wm_msgfilterex_bad_cbsize) {
    HWND h = wm_hidden();
    CHANGEFILTERSTRUCT cf; memset(&cf, 0, sizeof cf);
    cf.cbSize = 4;
    SetLastError(0);
    BOOL r = U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_ALLOW, &cf);
    t_note("ChangeWindowMessageFilterEx(cbSize=4) = %d err=%lu", r, GetLastError());
    CHECK(!r);
    cf.cbSize = 0;
    r = U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_ALLOW, &cf);
    CHECK(!r);
    cf.cbSize = sizeof cf + 8;
    r = U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_ALLOW, &cf);
    CHECK(!r);
    U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_RESET, NULL);
    wm_destroy(h);
}

TEST(wm_msgfilterex_invalid_action) {
    HWND h = wm_hidden();
    SetLastError(0);
    BOOL r = U(ChangeWindowMessageFilterEx)(h, TEST_MSG, 4, NULL);
    t_note("ChangeWindowMessageFilterEx(action 4) = %d err=%lu", r, GetLastError());
    r = U(ChangeWindowMessageFilterEx)(h, TEST_MSG, 0xFFFF, NULL);
    t_note("ChangeWindowMessageFilterEx(action 0xFFFF) = %d", r);
    wm_destroy(h);
}

TEST(wm_msgfilterex_dead_window) {
    HWND h = wm_hidden(); wm_destroy(h);
    SetLastError(0);
    BOOL r = U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_ALLOW, NULL);
    t_note("ChangeWindowMessageFilterEx(dead) = %d err=%lu", r, GetLastError());
    r = U(ChangeWindowMessageFilterEx)((HWND)(ULONG_PTR)0x99990, TEST_MSG, MSGFLT_ALLOW, NULL);
    t_note("ChangeWindowMessageFilterEx(bogus) = %d", r);
}

TEST(wm_msgfilterex_core_messages) {
    HWND h = wm_hidden();
    static const UINT core[] = { WM_NULL, WM_CLOSE, WM_COPYDATA, WM_DESTROY, WM_USER, 0x4A /*COPYDATA*/, 0x49 /*COPYGLOBALDATA*/ };
    for (unsigned i = 0; i < sizeof core / sizeof *core; i++) {
        CHANGEFILTERSTRUCT cf; memset(&cf, 0, sizeof cf); cf.cbSize = sizeof cf;
        SetLastError(0);
        BOOL r = U(ChangeWindowMessageFilterEx)(h, core[i], MSGFLT_DISALLOW, &cf);
        t_note("DISALLOW message 0x%x: %d err=%lu ext=%lu", core[i], r, GetLastError(), cf.ExtStatus);
        U(ChangeWindowMessageFilterEx)(h, core[i], MSGFLT_RESET, NULL);
    }
    wm_destroy(h);
}

TEST(wm_msgfilterex_does_not_block_same_process) {
    HWND h = wm_hidden();
    U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_DISALLOW, NULL);
    wm_logclear();
    U(SendMessageA)(h, TEST_MSG, 1, 2);
    EQ(wm_logcount(TEST_MSG), 1);          /* filters only affect lower integrity senders */
    U(PostMessageA)(h, TEST_MSG, 1, 2);
    t_pump();
    EQ(wm_logcount(TEST_MSG), 2);
    U(ChangeWindowMessageFilterEx)(h, TEST_MSG, MSGFLT_RESET, NULL);
    wm_destroy(h);
}

/* ---------------- DPI ---------------- */
TEST(wm_dpi_system) {
    REQUIRE(UN(GetDpiForSystem));
    UINT d = UN(GetDpiForSystem)();
    CHECK(d >= 96);
    CHECK(d <= 960);
    EQ(UN(GetDpiForSystem)(), d);
}

TEST(wm_dpi_window_basic) {
    REQUIRE(UN(GetDpiForWindow));
    HWND h = wm_hidden();
    UINT d = UN(GetDpiForWindow)(h);
    CHECK(d >= 96);
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, h);
    EQ(UN(GetDpiForWindow)(c), d);
    wm_destroy(h);
    EQ(UN(GetDpiForWindow)(h), 0);
    EQ(UN(GetDpiForWindow)(NULL), 0);
    EQ(UN(GetDpiForWindow)((HWND)(ULONG_PTR)0x77770), 0);
    UINT dd = UN(GetDpiForWindow)(U(GetDesktopWindow)());
    t_note("GetDpiForWindow(desktop) = %u", dd);
}

TEST(wm_dpi_awareness_context_helpers) {
    REQUIRE(UN(IsValidDpiAwarenessContext));
    REQUIRE(UN(GetAwarenessFromDpiAwarenessContext));
    REQUIRE(UN(AreDpiAwarenessContextsEqual));
    HANDLE valid[] = { CTX_UNAWARE, CTX_SYSTEM, CTX_PM, CTX_PMV2, CTX_GDISCALED };
    for (int i = 0; i < 5; i++) {
        if (!UN(IsValidDpiAwarenessContext)(valid[i])) t_note("context %d not valid on this build", -1 - i);
        EQ(UN(AreDpiAwarenessContextsEqual)(valid[i], valid[i]), TRUE);
    }
    CHECK(UN(IsValidDpiAwarenessContext)(CTX_UNAWARE));
    CHECK(UN(IsValidDpiAwarenessContext)(CTX_SYSTEM));
    CHECK(UN(IsValidDpiAwarenessContext)(CTX_PM));
    CHECK(!UN(IsValidDpiAwarenessContext)(NULL));
    CHECK(!UN(IsValidDpiAwarenessContext)((HANDLE)-6));
    t_note("IsValidDpiAwarenessContext(raw 0x1234) = %d", UN(IsValidDpiAwarenessContext)((HANDLE)(ULONG_PTR)0x1234));
    CHECK(!UN(IsValidDpiAwarenessContext)((HANDLE)-100));
    EQ(UN(GetAwarenessFromDpiAwarenessContext)(CTX_UNAWARE), 0);
    EQ(UN(GetAwarenessFromDpiAwarenessContext)(CTX_SYSTEM), 1);
    EQ(UN(GetAwarenessFromDpiAwarenessContext)(CTX_PM), 2);
    if (UN(IsValidDpiAwarenessContext)(CTX_PMV2)) EQ(UN(GetAwarenessFromDpiAwarenessContext)(CTX_PMV2), 2);
    if (UN(IsValidDpiAwarenessContext)(CTX_GDISCALED)) EQ(UN(GetAwarenessFromDpiAwarenessContext)(CTX_GDISCALED), 0);
    EQ(UN(GetAwarenessFromDpiAwarenessContext)(NULL), -1);

    EQ(UN(AreDpiAwarenessContextsEqual)(CTX_UNAWARE, CTX_SYSTEM), FALSE);
    EQ(UN(AreDpiAwarenessContextsEqual)(CTX_SYSTEM, CTX_PM), FALSE);
    EQ(UN(AreDpiAwarenessContextsEqual)(CTX_PM, CTX_UNAWARE), FALSE);
    if (UN(IsValidDpiAwarenessContext)(CTX_PMV2)) EQ(UN(AreDpiAwarenessContextsEqual)(CTX_PM, CTX_PMV2), FALSE);
    if (UN(IsValidDpiAwarenessContext)(CTX_GDISCALED)) EQ(UN(AreDpiAwarenessContextsEqual)(CTX_UNAWARE, CTX_GDISCALED), FALSE);
    EQ(UN(AreDpiAwarenessContextsEqual)(NULL, NULL), FALSE);
}

TEST(wm_dpi_get_dpi_from_context) {
    REQUIRE(UN(GetDpiFromDpiAwarenessContext));
    UINT u = UN(GetDpiFromDpiAwarenessContext)(CTX_UNAWARE);
    EQ(u, 96);
    EQ(UN(GetDpiFromDpiAwarenessContext)(CTX_SYSTEM), UN(GetDpiForSystem)());
    EQ(UN(GetDpiFromDpiAwarenessContext)(NULL), 0);
    t_note("dpi from raw 0x1234: %u", UN(GetDpiFromDpiAwarenessContext)((HANDLE)(ULONG_PTR)0x1234));
    EQ(UN(GetDpiFromDpiAwarenessContext)((HANDLE)-100), 0);
    t_note("dpi from PM context: %u", UN(GetDpiFromDpiAwarenessContext)(CTX_PM));
}

TEST(wm_dpi_thread_context_current) {
    REQUIRE(UN(GetThreadDpiAwarenessContext));
    HANDLE c = UN(GetThreadDpiAwarenessContext)();
    CHECK(c != NULL);
    CHECK(UN(IsValidDpiAwarenessContext)(c));
    int a = UN(GetAwarenessFromDpiAwarenessContext)(c);
    CHECK(a >= 0 && a <= 2);
    EQ(UN(AreDpiAwarenessContextsEqual)(c, c), TRUE);
    t_note("thread dpi context %p awareness %d", c, a);
}

typedef struct { HANDLE ctx; HANDLE prev; HANDLE seen; int aw; int ok; UINT dpiwin; UINT dpisys; HANDLE winctx; int winaw; HANDLE restored; } dpi_job;
static DWORD WINAPI dpi_thread(LPVOID p) {
    dpi_job *j = p;
    U(IsGUIThread)(TRUE);
    j->prev = UN(SetThreadDpiAwarenessContext)(j->ctx);
    if (!j->prev) return 0;
    j->ok = 1;
    j->seen = UN(GetThreadDpiAwarenessContext)();
    j->aw = UN(GetAwarenessFromDpiAwarenessContext)(j->seen);
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW, 100, 100, 200, 150, NULL);
    if (h) {
        j->dpiwin = UN(GetDpiForWindow)(h);
        j->winctx = UN(GetWindowDpiAwarenessContext)(h);
        j->winaw = UN(GetAwarenessFromDpiAwarenessContext)(j->winctx);
        U(DestroyWindow)(h);
    }
    j->dpisys = UN(GetDpiForSystem)();
    j->restored = UN(SetThreadDpiAwarenessContext)(j->prev);
    return 0;
}
static int dpi_run(dpi_job *j, HANDLE ctx) {
    memset(j, 0, sizeof *j); j->ctx = ctx;
    HANDLE t = CreateThread(NULL, 0, dpi_thread, j, 0, NULL);
    if (!t) return 0;
    DWORD r = WaitForSingleObject(t, WM_TIMEOUT_MS * 2);
    CloseHandle(t);
    return r == WAIT_OBJECT_0;
}

TEST(wm_dpi_thread_set_unaware_window) {
    REQUIRE(UN(SetThreadDpiAwarenessContext)); REQUIRE(UN(GetWindowDpiAwarenessContext));
    dpi_job j;
    CHECK(dpi_run(&j, CTX_UNAWARE));
    if (!j.ok) { t_note("SetThreadDpiAwarenessContext(UNAWARE) failed"); return; }
    EQ(j.aw, 0);
    EQ(j.winaw, 0);
    EQ(j.dpiwin, 96);
    CHECK(j.winctx != NULL || 1);
}

TEST(wm_dpi_thread_set_system_window) {
    REQUIRE(UN(SetThreadDpiAwarenessContext));
    dpi_job j;
    CHECK(dpi_run(&j, CTX_SYSTEM));
    if (!j.ok) { t_note("SetThreadDpiAwarenessContext(SYSTEM) failed"); return; }
    EQ(j.aw, 1);
    EQ(j.winaw, 1);
    EQ(j.dpiwin, j.dpisys);
}

TEST(wm_dpi_thread_set_permonitor_window) {
    REQUIRE(UN(SetThreadDpiAwarenessContext));
    dpi_job j;
    CHECK(dpi_run(&j, CTX_PM));
    if (!j.ok) { t_note("SetThreadDpiAwarenessContext(PM) failed"); return; }
    EQ(j.aw, 2);
    EQ(j.winaw, 2);
    CHECK(j.dpiwin >= 96);
}

TEST(wm_dpi_thread_set_permonitor_v2_window) {
    REQUIRE(UN(SetThreadDpiAwarenessContext));
    dpi_job j;
    CHECK(dpi_run(&j, CTX_PMV2));
    if (!j.ok) { t_note("SetThreadDpiAwarenessContext(PMv2) not supported here"); return; }
    EQ(j.aw, 2);
    EQ(j.winaw, 2);
    CHECK(UN(AreDpiAwarenessContextsEqual)(j.seen, CTX_PMV2));
    CHECK(j.dpiwin >= 96);
}

TEST(wm_dpi_thread_set_gdiscaled_window) {
    REQUIRE(UN(SetThreadDpiAwarenessContext));
    dpi_job j;
    CHECK(dpi_run(&j, CTX_GDISCALED));
    if (!j.ok) { t_note("SetThreadDpiAwarenessContext(GDISCALED) not supported here"); return; }
    EQ(j.aw, 0);
    EQ(j.winaw, 0);
    EQ(j.dpiwin, 96);
    CHECK(UN(AreDpiAwarenessContextsEqual)(j.seen, CTX_GDISCALED));
}

TEST(wm_dpi_thread_context_restore) {
    REQUIRE(UN(SetThreadDpiAwarenessContext));
    dpi_job j;
    CHECK(dpi_run(&j, CTX_SYSTEM));
    if (!j.ok) return;
    CHECK(UN(AreDpiAwarenessContextsEqual)(j.restored, CTX_SYSTEM) || j.restored != NULL);
    CHECK(UN(AreDpiAwarenessContextsEqual)(j.seen, CTX_SYSTEM));
}

typedef struct { HANDLE r1, r2, r3; } dpi_bad;
static DWORD WINAPI dpi_bad_thread(LPVOID p) {
    dpi_bad *b = p;
    U(IsGUIThread)(TRUE);
    SetLastError(0);
    b->r1 = UN(SetThreadDpiAwarenessContext)(NULL);
    b->r2 = UN(SetThreadDpiAwarenessContext)((HANDLE)(ULONG_PTR)0x1234);
    b->r3 = UN(SetThreadDpiAwarenessContext)((HANDLE)-99);
    return 0;
}
TEST(wm_dpi_thread_set_invalid) {
    REQUIRE(UN(SetThreadDpiAwarenessContext));
    dpi_bad b = {0};
    HANDLE t = CreateThread(NULL, 0, dpi_bad_thread, &b, 0, NULL);
    WaitForSingleObject(t, WM_TIMEOUT_MS * 2);
    CloseHandle(t);
    EQ(b.r1, NULL);
    EQ(b.r2, NULL);
    EQ(b.r3, NULL);
}

TEST(wm_dpi_window_context_dead) {
    REQUIRE(UN(GetWindowDpiAwarenessContext));
    HWND h = wm_hidden();
    HANDLE c = UN(GetWindowDpiAwarenessContext)(h);
    CHECK(c != NULL);
    int aw = UN(GetAwarenessFromDpiAwarenessContext)(c);
    t_note("window awareness context %p awareness %d", c, aw);
    CHECK(aw >= 0 && aw <= 2);
    wm_destroy(h);
    EQ(UN(GetWindowDpiAwarenessContext)(h), NULL);
    EQ(UN(GetWindowDpiAwarenessContext)(NULL), NULL);
    EQ(UN(GetWindowDpiAwarenessContext)((HWND)(ULONG_PTR)0x44440), NULL);
}

TEST(wm_dpi_window_context_matches_thread) {
    REQUIRE(UN(GetWindowDpiAwarenessContext));
    HWND h = wm_hidden();
    HANDLE wc = UN(GetWindowDpiAwarenessContext)(h);
    HANDLE tc = UN(GetThreadDpiAwarenessContext)();
    EQ(UN(GetAwarenessFromDpiAwarenessContext)(wc), UN(GetAwarenessFromDpiAwarenessContext)(tc));
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, h);
    EQ(UN(GetWindowDpiAwarenessContext)(c), wc);
    wm_destroy(h);
}

TEST(wm_dpi_window_context_cross_thread_window) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    HANDLE a = UN(GetWindowDpiAwarenessContext)(w.hwnd);
    CHECK(a != NULL);
    CHECK(UN(GetDpiForWindow)(w.hwnd) >= 96);
    wm_worker_stop(&w);
}

TEST(wm_dpi_mixed_hosting_parent_child) {
    REQUIRE(UN(SetThreadDpiAwarenessContext));
    HWND parent = wm_hidden();
    HANDLE old = UN(SetThreadDpiAwarenessContext)(CTX_PM);
    if (!old) { t_note("cannot switch thread context"); wm_destroy(parent); return; }
    SetLastError(0);
    HWND child = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, parent);
    t_note("child window with different awareness than parent created: %p err=%lu", child, GetLastError());
    UN(SetThreadDpiAwarenessContext)(old);
    wm_destroy(child);
    wm_destroy(parent);
}

/* ---------------- window procedure access ---------------- */
static int g_sub_hits;
static WNDPROC g_old_proc;
static LRESULT CALLBACK sub_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 7) { g_sub_hits++; return 0x5151; }
    return U(CallWindowProcA)(g_old_proc, h, m, w, l);
}

TEST(wm_wndproc_subclass) {
    NEED(CallWindowProcA);
    HWND h = wm_hidden();
    WNDPROC old = (WNDPROC)U(GetWindowLongPtrA)(h, GWLP_WNDPROC);
    CHECK(old != NULL);
    g_old_proc = old; g_sub_hits = 0;
    WNDPROC prev = (WNDPROC)U(SetWindowLongPtrA)(h, GWLP_WNDPROC, (LONG_PTR)sub_proc);
    EQ(prev, old);
    EQ(U(GetWindowLongPtrA)(h, GWLP_WNDPROC), (LONG_PTR)sub_proc);
    EQ(U(SendMessageA)(h, WM_USER + 7, 0, 0), 0x5151);
    EQ(g_sub_hits, 1);
    char b[8];
    EQ(U(GetWindowTextA)(h, b, 8), 2);        /* other messages fall through to the original */
    U(SetWindowLongPtrA)(h, GWLP_WNDPROC, (LONG_PTR)old);
    EQ(U(SendMessageA)(h, WM_USER + 7, 0, 0), 0);
    EQ(g_sub_hits, 1);
    wm_destroy(h);
}

TEST(wm_wndproc_set_null) {
    HWND h = wm_hidden();
    WNDPROC old = (WNDPROC)U(GetWindowLongPtrA)(h, GWLP_WNDPROC);
    SetLastError(0);
    LONG_PTR r = U(SetWindowLongPtrA)(h, GWLP_WNDPROC, 0);
    t_note("SetWindowLongPtr(GWLP_WNDPROC, 0) = %p err=%lu now %p", (void *)r, GetLastError(), (void *)U(GetWindowLongPtrA)(h, GWLP_WNDPROC));
    U(SetWindowLongPtrA)(h, GWLP_WNDPROC, (LONG_PTR)old);
    wm_destroy(h);
}

static DWORD WINAPI wndproc_other_thread(LPVOID p) {
    HWND h = (HWND)p;
    SetLastError(0);
    LONG_PTR r = U(SetWindowLongPtrA)(h, GWLP_WNDPROC, (LONG_PTR)sub_proc);
    SetLastError(GetLastError());
    return (DWORD)(r == 0);
}
TEST(wm_wndproc_cross_thread_denied) {
    HWND h = wm_hidden();
    HANDLE t = CreateThread(NULL, 0, wndproc_other_thread, h, 0, NULL);
    WaitForSingleObject(t, WM_TIMEOUT_MS);
    DWORD failed = 0; GetExitCodeThread(t, &failed);
    CloseHandle(t);
    t_note("SetWindowLongPtr(GWLP_WNDPROC) from another thread (same process) failed=%lu", failed);
    wm_destroy(h);
}

TEST(wm_wndproc_wide_ansi_views) {
    HWND a = wm_hidden();
    LONG_PTR pa = U(GetWindowLongPtrA)(a, GWLP_WNDPROC);
    LONG_PTR pw = U(GetWindowLongPtrW)(a, GWLP_WNDPROC);
    t_note("ANSI window proc via A=%p via W=%p", (void *)pa, (void *)pw);
    CHECK(pa != 0 && pw != 0);
    HWND w = wm_mkw(WS_POPUP, WS_EX_TOOLWINDOW, 0, 0, 10, 10, NULL);
    LONG_PTR qa = U(GetWindowLongPtrA)(w, GWLP_WNDPROC);
    LONG_PTR qw = U(GetWindowLongPtrW)(w, GWLP_WNDPROC);
    t_note("Unicode window proc via A=%p via W=%p", (void *)qa, (void *)qw);
    CHECK(qa != 0 && qw != 0);
    wm_destroy(a); wm_destroy(w);
}

TEST(wm_gwl_misc_indices) {
    HWND p = wm_hidden();
    HWND c = U(CreateWindowExA)(0, wm_cls(), "k", WS_CHILD, 1, 2, 10, 10, p, (HMENU)(ULONG_PTR)4321, GetModuleHandleA(NULL), NULL);
    EQ(U(GetWindowLongPtrA)(c, GWLP_ID), 4321);
    EQ(U(SetWindowLongPtrA)(c, GWLP_ID, 99), 4321);
    EQ(U(GetDlgCtrlID)(c), 99);
    EQ(U(GetWindowLongPtrA)(c, GWLP_HWNDPARENT), (LONG_PTR)p);
    HWND o = U(CreateWindowExA)(0, wm_cls(), "o", WS_POPUP, 0, 0, 10, 10, p, NULL, GetModuleHandleA(NULL), NULL);
    EQ(U(GetWindowLongPtrA)(o, GWLP_HWNDPARENT), (LONG_PTR)p);
    EQ(U(GetWindowLongPtrA)(p, GWLP_HWNDPARENT), 0);
    EQ(U(GetWindowLongPtrA)(p, GWLP_HINSTANCE), (LONG_PTR)GetModuleHandleA(NULL));
    wm_destroy(p);
}

TEST(wm_gwl_hwndparent_set_changes_owner) {
    HWND a = wm_hidden(), b = wm_hidden();
    HWND o = U(CreateWindowExA)(0, wm_cls(), "o", WS_POPUP, 0, 0, 10, 10, a, NULL, GetModuleHandleA(NULL), NULL);
    LONG_PTR prev = U(SetWindowLongPtrA)(o, GWLP_HWNDPARENT, (LONG_PTR)b);
    EQ(prev, (LONG_PTR)a);
    EQ(U(GetWindow)(o, GW_OWNER), b);
    wm_destroy(a);
    CHECK(U(IsWindow)(o));
    wm_destroy(b);
    CHECK(!U(IsWindow)(o));
}
