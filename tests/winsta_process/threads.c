/* threads: GUI thread state, cross-thread behaviour, per-thread desktops */
#include "wsp.h"

typedef struct {
    HANDLE th, ready, go;
    DWORD tid;
    HWND hwnd, hwnd2;
    HDESK desk;
    HDESK orig;
    LONG r[16];
    char name[96];
    int mode;
} wk;

static wk *wk_new(void) {
    wk *w = (wk *)calloc(1, sizeof *w);
    w->ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    w->go = CreateEventA(NULL, TRUE, FALSE, NULL);
    return w;
}
static int wk_wait_ready(wk *w) { return WaitForSingleObject(w->ready, 8000) == WAIT_OBJECT_0; }
static void wk_free(wk *w) {
    SetEvent(w->go);
    if (w->th) { if (WaitForSingleObject(w->th, 8000) != WAIT_OBJECT_0) TerminateThread(w->th, 0); CloseHandle(w->th); }
    CloseHandle(w->ready); CloseHandle(w->go); free(w);
}
static wk *wk_start(LPTHREAD_START_ROUTINE fn) {
    wk *w = wk_new();
    w->th = CreateThread(NULL, 0, fn, w, 0, &w->tid);
    return w;
}
static void wk_signal_ready_and_wait(wk *w) { SetEvent(w->ready); WaitForSingleObject(w->go, 8000); }

static HWND mkwin(DWORD style) {
    return t_make_window(NULL, style, 0, -32000, -32000, 40, 40, NULL);
}
static void make_gui(void) { U(IsGUIThread)(TRUE); }

/* ---------- IsGUIThread ---------- */
static DWORD wk_isgui_fresh(LPVOID p) {
    wk *w = p;
    w->r[0] = U(IsGUIThread)(FALSE);
    w->r[1] = U(IsGUIThread)(FALSE);
    w->r[2] = U(IsGUIThread)(TRUE);
    w->r[3] = U(IsGUIThread)(FALSE);
    w->r[4] = U(IsGUIThread)(TRUE);
    SetEvent(w->ready);
    return 0;
}
TEST(winsta_process_isguithread_fresh_and_convert) {
    REQUIRE(U(IsGUIThread));
    wk *w = wk_start(wk_isgui_fresh);
    CHECK(wk_wait_ready(w));
    EXPECT_EQ(w->r[0], 0); EXPECT_EQ(w->r[1], 0);
    EXPECT_EQ(w->r[2], 1); EXPECT_EQ(w->r[3], 1); EXPECT_EQ(w->r[4], 1);
    wk_free(w);
}
static DWORD wk_isgui_window(LPVOID p) {
    wk *w = p;
    w->r[0] = U(IsGUIThread)(FALSE);
    HWND h = mkwin(WS_POPUP);
    w->r[1] = U(IsGUIThread)(FALSE);
    if (h) U(DestroyWindow)(h);
    w->r[2] = U(IsGUIThread)(FALSE);    /* stays a GUI thread after the window is gone */
    SetEvent(w->ready);
    return 0;
}
TEST(winsta_process_isguithread_after_window) {
    REQUIRE(U(IsGUIThread)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow));
    wk *w = wk_start(wk_isgui_window);
    CHECK(wk_wait_ready(w));
    EXPECT_EQ(w->r[0], 0); EXPECT_EQ(w->r[1], 1); EXPECT_EQ(w->r[2], 1);
    wk_free(w);
}
static DWORD wk_isgui_peek(LPVOID p) {
    wk *w = p; MSG m;
    w->r[0] = U(IsGUIThread)(FALSE);
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);
    w->r[1] = U(IsGUIThread)(FALSE);
    SetEvent(w->ready);
    return 0;
}
TEST(winsta_process_isguithread_after_peekmessage) {
    REQUIRE(U(IsGUIThread)); REQUIRE(U(PeekMessageA));
    wk *w = wk_start(wk_isgui_peek);
    CHECK(wk_wait_ready(w));
    EXPECT_EQ(w->r[0], 0);
    t_note("IsGUIThread(FALSE) after PeekMessage on fresh thread: %ld", w->r[1]);
    wk_free(w);
}
static DWORD wk_isgui_gdi(LPVOID p) {
    wk *w = p;
    w->r[0] = U(GetSystemMetrics)(SM_CXSCREEN) > 0;
    w->r[1] = U(IsGUIThread)(FALSE);
    SetEvent(w->ready);
    return 0;
}
TEST(winsta_process_isguithread_after_getsystemmetrics) {
    REQUIRE(U(IsGUIThread)); REQUIRE(U(GetSystemMetrics));
    wk *w = wk_start(wk_isgui_gdi);
    CHECK(wk_wait_ready(w));
    EXPECT_EQ(w->r[0], 1);
    t_note("IsGUIThread(FALSE) after GetSystemMetrics on fresh thread: %ld", w->r[1]);
    wk_free(w);
}
TEST(winsta_process_isguithread_repeat) {
    REQUIRE(U(IsGUIThread));
    CHECK(U(IsGUIThread)(TRUE));
    for (int i = 0; i < 1000; i++) if (!U(IsGUIThread)(FALSE) || !U(IsGUIThread)(TRUE)) { CHECK(0); break; }
}
TEST(winsta_process_isguithread_nonzero_bconvert) {
    REQUIRE(U(IsGUIThread));
    make_gui();
    CHECK(U(IsGUIThread)(0x100) != 0);
    CHECK(U(IsGUIThread)(-1) != 0);
}

/* ---------- GetGUIThreadInfo ---------- */
TEST(winsta_process_guiinfo_bad_cbsize) {
    REQUIRE(U(GetGUIThreadInfo)); make_gui();
    static const DWORD cb[] = { 0, 1, 8, 16, 71, 73, 1000 };
    for (unsigned i = 0; i < 7; i++) {
        GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = cb[i];
        SetLastError(0);
        BOOL r = U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi);
        if (cb[i] <= 16) CHECKF(!r, "cbSize %lu", (unsigned long)cb[i]);
        else t_note("GetGUIThreadInfo cbSize %lu -> %d err %lu", (unsigned long)cb[i], r, GetLastError());
    }
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = 0;
    SetLastError(0);
    CHECK(!U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    t_note("GetGUIThreadInfo(cbSize=0) err %lu", GetLastError());
}
TEST(winsta_process_guiinfo_struct_size) {
    EXPECT_EQ(sizeof(GUITHREADINFO), 72);
}
TEST(winsta_process_guiinfo_own_thread_basic) {
    REQUIRE(U(GetGUIThreadInfo)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow));
    HWND h = mkwin(WS_POPUP); CHECK(h != NULL);
    GUITHREADINFO gi; memset(&gi, 0xcc, sizeof gi); gi.cbSize = sizeof gi;
    SetLastError(0);
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EXPECT_EQ(gi.cbSize, sizeof gi);
    EXPECT_EQ(gi.hwndCapture, NULL);
    EXPECT_EQ(gi.hwndMenuOwner, NULL);
    EXPECT_EQ(gi.hwndMoveSize, NULL);
    CHECK(!(gi.flags & (GUI_INMENUMODE | GUI_INMOVESIZE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE)));
    if (h) U(DestroyWindow)(h);
}
TEST(winsta_process_guiinfo_matches_getters) {
    REQUIRE(U(GetGUIThreadInfo)); REQUIRE(U(GetActiveWindow)); REQUIRE(U(GetFocus)); REQUIRE(U(GetCapture));
    HWND h = mkwin(WS_POPUP | WS_VISIBLE); CHECK(h != NULL);
    U(SetActiveWindow)(h);
    U(SetFocus)(h);
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EXPECT_EQ(gi.hwndActive, U(GetActiveWindow)());
    EXPECT_EQ(gi.hwndFocus, U(GetFocus)());
    EXPECT_EQ(gi.hwndCapture, U(GetCapture)());
    t_note("active=%p focus=%p (window %p) flags=0x%lx", gi.hwndActive, gi.hwndFocus, h, (unsigned long)gi.flags);
    if (h) U(DestroyWindow)(h);
}
TEST(winsta_process_guiinfo_capture) {
    REQUIRE(U(GetGUIThreadInfo)); REQUIRE(U(SetCapture)); REQUIRE(U(ReleaseCapture)); REQUIRE(U(GetCapture));
    HWND h = mkwin(WS_POPUP | WS_VISIBLE); CHECK(h != NULL);
    U(SetCapture)(h);
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EXPECT_EQ(U(GetCapture)(), h);
    EXPECT_EQ(gi.hwndCapture, h);
    CHECK(U(ReleaseCapture)());
    memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
    EXPECT_EQ(gi.hwndCapture, NULL);
    EXPECT_EQ(U(GetCapture)(), NULL);
    if (h) U(DestroyWindow)(h);
}
TEST(winsta_process_guiinfo_caret) {
    REQUIRE(U(GetGUIThreadInfo)); REQUIRE(U(CreateCaret)); REQUIRE(U(DestroyCaret)); REQUIRE(U(SetCaretPos));
    HWND h = mkwin(WS_POPUP | WS_VISIBLE); CHECK(h != NULL);
    BOOL c = U(CreateCaret)(h, NULL, 3, 11);
    CHECK(c);
    if (c) {
        CHECK(U(SetCaretPos)(5, 6));
        GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
        CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
        EXPECT_EQ(gi.hwndCaret, h);
        EXPECT_EQ(gi.rcCaret.left, 5); EXPECT_EQ(gi.rcCaret.top, 6);
        EXPECT_EQ(gi.rcCaret.right, 8); EXPECT_EQ(gi.rcCaret.bottom, 17);
        CHECK(U(DestroyCaret)());
        memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
        CHECK(U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi));
        EXPECT_EQ(gi.hwndCaret, NULL);
    }
    if (h) U(DestroyWindow)(h);
}
static DWORD wk_guiinfo_target(LPVOID p) {
    wk *w = p;
    w->hwnd = mkwin(WS_POPUP | WS_VISIBLE);
    if (w->hwnd) { U(SetCapture)(w->hwnd); U(CreateCaret)(w->hwnd, NULL, 4, 9); U(SetCaretPos)(2, 3); }
    wk_signal_ready_and_wait(w);
    if (w->hwnd) { U(DestroyCaret)(); U(ReleaseCapture)(); U(DestroyWindow)(w->hwnd); }
    return 0;
}
TEST(winsta_process_guiinfo_other_thread) {
    REQUIRE(U(GetGUIThreadInfo)); REQUIRE(U(SetCapture)); REQUIRE(U(CreateCaret));
    wk *w = wk_start(wk_guiinfo_target);
    if (!wk_wait_ready(w)) { CHECK(0); wk_free(w); return; }
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    SetLastError(0);
    CHECK(U(GetGUIThreadInfo)(w->tid, &gi));
    EXPECT_EQ(gi.hwndCapture, w->hwnd);
    EXPECT_EQ(gi.hwndCaret, w->hwnd);
    EXPECT_EQ(gi.rcCaret.left, 2); EXPECT_EQ(gi.rcCaret.top, 3);
    EXPECT_EQ(gi.rcCaret.right, 6); EXPECT_EQ(gi.rcCaret.bottom, 12);
    /* the calling thread's own capture is unaffected */
    CHECK(U(GetCapture)() == NULL);
    wk_free(w);
}
TEST(winsta_process_guiinfo_bogus_thread) {
    REQUIRE(U(GetGUIThreadInfo));
    static const DWORD tids[] = { 0xfffffff0, 0xffffffff, 0x7fffffff };
    for (unsigned i = 0; i < 3; i++) {
        GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
        SetLastError(0);
        BOOL r = U(GetGUIThreadInfo)(tids[i], &gi);
        CHECKF(!r, "tid 0x%lx", (unsigned long)tids[i]);
        t_note("GetGUIThreadInfo(0x%lx) err %lu", (unsigned long)tids[i], GetLastError());
    }
}
TEST(winsta_process_guiinfo_tid_zero_foreground) {
    REQUIRE(U(GetGUIThreadInfo));
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    SetLastError(0);
    BOOL r = U(GetGUIThreadInfo)(0, &gi);
    t_note("GetGUIThreadInfo(0) ret %d err %lu active=%p", r, GetLastError(), gi.hwndActive);
}
static DWORD wk_idle(LPVOID p) { wk *w = p; wk_signal_ready_and_wait(w); return 0; }
TEST(winsta_process_guiinfo_non_gui_thread) {
    REQUIRE(U(GetGUIThreadInfo));
    wk *w = wk_start(wk_idle);
    CHECK(wk_wait_ready(w));
    GUITHREADINFO gi; memset(&gi, 0xcc, sizeof gi); gi.cbSize = sizeof gi;
    SetLastError(0);
    BOOL r = U(GetGUIThreadInfo)(w->tid, &gi);
    t_note("GetGUIThreadInfo(non-GUI thread) ret %d err %lu", r, GetLastError());
    if (r) { EXPECT_EQ(gi.hwndCapture, NULL); EXPECT_EQ(gi.hwndCaret, NULL); }
    wk_free(w);
}

/* ---------- AttachThreadInput ---------- */
TEST(winsta_process_attach_same_thread) {
    REQUIRE(U(AttachThreadInput)); make_gui();
    DWORD me = GetCurrentThreadId();
    SetLastError(0);
    CHECK(!U(AttachThreadInput)(me, me, TRUE));
    t_note("AttachThreadInput(self,self,TRUE) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(AttachThreadInput)(me, me, FALSE));
}
TEST(winsta_process_attach_bogus_threads) {
    REQUIRE(U(AttachThreadInput)); make_gui();
    DWORD me = GetCurrentThreadId();
    SetLastError(0); CHECK(!U(AttachThreadInput)(0xfffffff0, me, TRUE));
    t_note("Attach(bogus, me) err %lu", GetLastError());
    SetLastError(0); CHECK(!U(AttachThreadInput)(me, 0xfffffff0, TRUE));
    SetLastError(0); CHECK(!U(AttachThreadInput)(0, me, TRUE));
    SetLastError(0); CHECK(!U(AttachThreadInput)(me, 0, TRUE));
    SetLastError(0); CHECK(!U(AttachThreadInput)(0, 0, TRUE));
    SetLastError(0); CHECK(!U(AttachThreadInput)(0xfffffff0, 0xfffffff4, FALSE));
}
TEST(winsta_process_attach_detach_not_attached) {
    REQUIRE(U(AttachThreadInput)); make_gui();
    wk *w = wk_start(wk_guiinfo_target);
    if (!wk_wait_ready(w)) { CHECK(0); wk_free(w); return; }
    SetLastError(0);
    BOOL r = U(AttachThreadInput)(GetCurrentThreadId(), w->tid, FALSE);
    t_note("detach of never-attached threads: ret %d err %lu", r, GetLastError());
    CHECK(!r);
    wk_free(w);
}
static DWORD wk_gui_window(LPVOID p) {
    wk *w = p; MSG m;
    w->hwnd = mkwin(WS_POPUP);
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);
    wk_signal_ready_and_wait(w);
    if (w->hwnd) U(DestroyWindow)(w->hwnd);
    return 0;
}
TEST(winsta_process_attach_roundtrip) {
    REQUIRE(U(AttachThreadInput)); make_gui();
    wk *w = wk_start(wk_gui_window);
    if (!wk_wait_ready(w)) { CHECK(0); wk_free(w); return; }
    DWORD me = GetCurrentThreadId();
    SetLastError(0);
    BOOL a = U(AttachThreadInput)(me, w->tid, TRUE);
    CHECK(a);
    if (a) {
        BOOL a2 = U(AttachThreadInput)(me, w->tid, TRUE);
        t_note("second attach ret %d err %lu", a2, GetLastError());
        CHECK(U(AttachThreadInput)(me, w->tid, FALSE));
        BOOL d2 = U(AttachThreadInput)(me, w->tid, FALSE);
        t_note("second detach ret %d err %lu", d2, GetLastError());
    }
    /* reversed argument order */
    a = U(AttachThreadInput)(w->tid, me, TRUE);
    CHECK(a);
    if (a) CHECK(U(AttachThreadInput)(w->tid, me, FALSE));
    wk_free(w);
}
TEST(winsta_process_attach_thread_exits_while_attached) {
    REQUIRE(U(AttachThreadInput)); make_gui();
    wk *w = wk_start(wk_gui_window);
    if (!wk_wait_ready(w)) { CHECK(0); wk_free(w); return; }
    DWORD me = GetCurrentThreadId();
    BOOL a = U(AttachThreadInput)(me, w->tid, TRUE);
    CHECK(a);
    SetEvent(w->go);
    WaitForSingleObject(w->th, 8000);
    SetLastError(0);
    BOOL d = U(AttachThreadInput)(me, w->tid, FALSE);
    CHECK(!d);   /* thread no longer exists */
    t_pump();
    wk_free(w);
}
/* ---------- thread desktop ---------- */
static DWORD wk_setdesk_basic(LPVOID p) {
    wk *w = p;
    HDESK me = U(GetThreadDesktop)(GetCurrentThreadId());
    w->r[0] = (me == w->orig);
    w->r[1] = U(SetThreadDesktop)(me);
    SetLastError(0);
    w->r[2] = U(SetThreadDesktop)(NULL);
    w->r[3] = GetLastError();
    SetLastError(0);
    w->r[4] = U(SetThreadDesktop)((HDESK)(ULONG_PTR)0x7ffc);
    w->r[5] = GetLastError();
    SetLastError(0);
    w->r[6] = U(SetThreadDesktop)((HDESK)U(GetProcessWindowStation)());
    w->r[7] = GetLastError();
    w->r[8] = (U(GetThreadDesktop)(GetCurrentThreadId()) == w->orig);   /* unchanged after failures */
    SetEvent(w->ready);
    return 0;
}
TEST(winsta_process_setthreaddesktop_basic_errors) {
    REQUIRE(U(SetThreadDesktop)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetProcessWindowStation));
    wk *w = wk_new(); w->orig = U(GetThreadDesktop)(GetCurrentThreadId());
    w->th = CreateThread(NULL, 0, wk_setdesk_basic, w, 0, &w->tid);
    CHECK(wk_wait_ready(w));
    EXPECT_EQ(w->r[0], 1);
    EXPECT_EQ(w->r[1], 1);
    EXPECT_EQ(w->r[2], 0);
    t_note("SetThreadDesktop(NULL) err %ld", w->r[3]);
    EXPECT_EQ(w->r[3], ERROR_INVALID_HANDLE);
    EXPECT_EQ(w->r[4], 0); EXPECT_EQ(w->r[5], ERROR_INVALID_HANDLE);
    EXPECT_EQ(w->r[6], 0); EXPECT_EQ(w->r[7], ERROR_INVALID_HANDLE);
    EXPECT_EQ(w->r[8], 1);
    wk_free(w);
}
static DWORD wk_setdesk_window(LPVOID p) {
    wk *w = p;
    w->hwnd = mkwin(WS_POPUP);
    w->r[0] = w->hwnd != NULL;
    SetLastError(0);
    w->r[1] = U(SetThreadDesktop)(w->desk);          /* thread owns a window: must fail */
    w->r[2] = GetLastError();
    w->r[3] = (U(GetThreadDesktop)(GetCurrentThreadId()) == w->orig);
    if (w->hwnd) U(DestroyWindow)(w->hwnd);
    SetLastError(0);
    w->r[4] = U(SetThreadDesktop)(w->desk);
    w->r[5] = GetLastError();
    w->r[6] = (U(GetThreadDesktop)(GetCurrentThreadId()) == w->desk);
    SetEvent(w->ready);
    return 0;
}
TEST(winsta_process_setthreaddesktop_busy_with_window) {
    REQUIRE(U(SetThreadDesktop)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop)); REQUIRE(U(CreateWindowExA));
    wk *w = wk_new(); wsp_name(w->name, sizeof w->name, "tdB");
    w->desk = U(CreateDesktopA)(w->name, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
    if (!w->desk) { t_skip("cannot create desktop"); wk_free(w); return; }
    w->orig = U(GetThreadDesktop)(GetCurrentThreadId());
    w->th = CreateThread(NULL, 0, wk_setdesk_window, w, 0, &w->tid);
    CHECK(wk_wait_ready(w));
    EXPECT_EQ(w->r[0], 1);
    EXPECT_EQ(w->r[1], 0);
    EXPECT_EQ(w->r[2], ERROR_BUSY);
    EXPECT_EQ(w->r[3], 1);
    t_note("SetThreadDesktop after DestroyWindow: ret %ld err %ld cur==new %ld", w->r[4], w->r[5], w->r[6]);
    WaitForSingleObject(w->th, 8000);
    CHECK(U(CloseDesktop)(w->desk));
    wk_free(w);
}
static DWORD CALLBACK hk_dummy(int c, WPARAM wp, LPARAM lp) { return CallNextHookEx(NULL, c, wp, lp); }
static DWORD wk_setdesk_hook(LPVOID p) {
    wk *w = p;
    HHOOK h = U(SetWindowsHookExA)(WH_GETMESSAGE, (HOOKPROC)hk_dummy, NULL, GetCurrentThreadId());
    w->r[0] = h != NULL;
    SetLastError(0);
    w->r[1] = U(SetThreadDesktop)(w->desk);
    w->r[2] = GetLastError();
    if (h) U(UnhookWindowsHookEx)(h);
    SetLastError(0);
    w->r[3] = U(SetThreadDesktop)(w->desk);
    w->r[4] = GetLastError();
    SetEvent(w->ready);
    return 0;
}
TEST(winsta_process_setthreaddesktop_busy_with_hook) {
    REQUIRE(U(SetThreadDesktop)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop)); REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(UnhookWindowsHookEx));
    wk *w = wk_new(); wsp_name(w->name, sizeof w->name, "tdH");
    w->desk = U(CreateDesktopA)(w->name, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
    if (!w->desk) { t_skip("cannot create desktop"); wk_free(w); return; }
    w->th = CreateThread(NULL, 0, wk_setdesk_hook, w, 0, &w->tid);
    CHECK(wk_wait_ready(w));
    EXPECT_EQ(w->r[0], 1);
    EXPECT_EQ(w->r[1], 0);
    EXPECT_EQ(w->r[2], ERROR_BUSY);
    EXPECT_EQ(w->r[3], 1);
    WaitForSingleObject(w->th, 8000);
    CHECK(U(CloseDesktop)(w->desk));
    wk_free(w);
}

/* ---------- desktop heap isolation ---------- */
typedef struct { HWND want; int found, n; } ewctx;
static BOOL CALLBACK ew_cb(HWND h, LPARAM lp) { ewctx *c = (ewctx *)lp; c->n++; if (h == c->want) c->found++; return TRUE; }

static DWORD wk_other_desk(LPVOID p) {
    wk *w = p;
    w->r[0] = U(SetThreadDesktop)(w->desk);
    w->r[1] = (U(GetThreadDesktop)(GetCurrentThreadId()) == w->desk);
    w->hwnd = mkwin(WS_POPUP);
    w->hwnd2 = U(GetDesktopWindow)();
    w->r[2] = w->hwnd != NULL;
    if (w->hwnd) {
        w->r[3] = (U(GetWindowLongA)(w->hwnd, GWL_STYLE) & WS_POPUP) != 0;
        w->r[4] = (U(GetParent)(w->hwnd) == NULL);
        w->r[5] = (U(GetAncestor)(w->hwnd, GA_PARENT) == w->hwnd2);
    }
    char nm[128] = ""; DWORD nd;
    U(GetUserObjectInformationA)(U(GetThreadDesktop)(GetCurrentThreadId()), UOI_NAME, nm, sizeof nm, &nd);
    w->r[6] = !strcmp(nm, w->name);
    wk_signal_ready_and_wait(w);
    if (w->hwnd) U(DestroyWindow)(w->hwnd);
    return 0;
}
TEST(winsta_process_desktop_isolation_enumwindows) {
    REQUIRE(U(SetThreadDesktop)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop)); REQUIRE(U(EnumDesktopWindows)); REQUIRE(U(EnumWindows)); REQUIRE(U(FindWindowA));
    wk *w = wk_new(); wsp_name(w->name, sizeof w->name, "iso");
    w->desk = U(CreateDesktopA)(w->name, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
    if (!w->desk) { t_skip("cannot create desktop"); wk_free(w); return; }
    w->th = CreateThread(NULL, 0, wk_other_desk, w, 0, &w->tid);
    if (!wk_wait_ready(w)) { CHECK(0); wk_free(w); return; }
    EXPECT_EQ(w->r[0], 1); EXPECT_EQ(w->r[1], 1); EXPECT_EQ(w->r[2], 1);
    EXPECT_EQ(w->r[3], 1); EXPECT_EQ(w->r[4], 1); EXPECT_EQ(w->r[5], 1); EXPECT_EQ(w->r[6], 1);
    CHECK(w->hwnd != NULL);
    ewctx c = { w->hwnd, 0, 0 };
    CHECK(U(EnumDesktopWindows)(w->desk, ew_cb, (LPARAM)&c));
    EXPECT_EQ(c.found, 1);
    c.found = 0; c.n = 0;
    CHECK(U(EnumWindows)(ew_cb, (LPARAM)&c));
    EXPECT_EQ(c.found, 0);
    c.found = 0;
    U(EnumDesktopWindows)(NULL, ew_cb, (LPARAM)&c);
    EXPECT_EQ(c.found, 0);
    CHECK(U(FindWindowA)(t_cls(), NULL) == NULL);
    /* each desktop has its own desktop window */
    CHECK(w->hwnd2 != NULL);
    EXPECT_NE(w->hwnd2, U(GetDesktopWindow)());
    t_note("IsWindow(window on other desktop) from default desktop thread: %d", U(IsWindow)(w->hwnd));
    SetEvent(w->go);
    WaitForSingleObject(w->th, 8000);
    c.found = 0;
    SetLastError(0);
    U(EnumDesktopWindows)(w->desk, ew_cb, (LPARAM)&c);
    EXPECT_EQ(c.found, 0);                /* window died with its thread */
    CHECK(U(CloseDesktop)(w->desk));
    wk_free(w);
}
TEST(winsta_process_desktop_isolation_desktop_window_differs) {
    REQUIRE(U(GetDesktopWindow)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop));
    wk *w = wk_new(); wsp_name(w->name, sizeof w->name, "iso2");
    w->desk = U(CreateDesktopA)(w->name, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
    if (!w->desk) { t_skip("cannot create desktop"); wk_free(w); return; }
    w->th = CreateThread(NULL, 0, wk_other_desk, w, 0, &w->tid);
    CHECK(wk_wait_ready(w));
    CHECK(w->hwnd2 != NULL && w->hwnd2 != U(GetDesktopWindow)());
    SetEvent(w->go);
    WaitForSingleObject(w->th, 8000);
    CHECK(U(CloseDesktop)(w->desk));
    wk_free(w);
}
static BOOL CALLBACK ew_stop(HWND h, LPARAM lp) { (*(int *)lp)++; return FALSE; }
TEST(winsta_process_enumdesktopwindows_stop_and_lparam) {
    REQUIRE(U(EnumDesktopWindows)); make_gui();
    HWND h = mkwin(WS_POPUP); CHECK(h != NULL);
    int n = 0;
    U(EnumDesktopWindows)(NULL, ew_stop, (LPARAM)&n);
    EXPECT_EQ(n, 1);
    ewctx c = { h, 0, 0 };
    CHECK(U(EnumDesktopWindows)(U(GetThreadDesktop)(GetCurrentThreadId()), ew_cb, (LPARAM)&c));
    EXPECT_EQ(c.found, 1);
    c.found = 0;
    CHECK(U(EnumDesktopWindows)(NULL, ew_cb, (LPARAM)&c));
    EXPECT_EQ(c.found, 1);
    /* top-level enumeration via EnumWindows agrees for a top level window */
    c.found = 0;
    CHECK(U(EnumWindows)(ew_cb, (LPARAM)&c));
    EXPECT_EQ(c.found, 1);
    if (h) U(DestroyWindow)(h);
}
TEST(winsta_process_enumdesktopwindows_invalid_desktop) {
    REQUIRE(U(EnumDesktopWindows));
    ewctx c = { NULL, 0, 0 };
    SetLastError(0);
    CHECK(!U(EnumDesktopWindows)((HDESK)(ULONG_PTR)0x7ffc, ew_cb, (LPARAM)&c));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    EXPECT_EQ(c.n, 0);
    SetLastError(0);
    CHECK(!U(EnumDesktopWindows)((HDESK)INVALID_HANDLE_VALUE, ew_cb, (LPARAM)&c));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(EnumDesktopWindows)((HDESK)U(GetProcessWindowStation)(), ew_cb, (LPARAM)&c));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_enumdesktopwindows_empty_desktop) {
    REQUIRE(U(EnumDesktopWindows)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop));
    char n[96]; wsp_name(n, sizeof n, "ewE");
    HDESK d = U(CreateDesktopA)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
    if (!d) { t_skip("cannot create desktop"); return; }
    ewctx c = { NULL, 0, 0 };
    SetLastError(0);
    BOOL r = U(EnumDesktopWindows)(d, ew_cb, (LPARAM)&c);
    t_note("EnumDesktopWindows(new empty desktop) ret %d count %d err %lu", r, c.n, GetLastError());
    U(CloseDesktop)(d);
}

/* ---------- window lifetime across threads ---------- */
static DWORD wk_create_exit(LPVOID p) {
    wk *w = p;
    w->hwnd = mkwin(WS_POPUP);
    w->hwnd2 = t_make_window(NULL, WS_CHILD, 0, 0, 0, 10, 10, w->hwnd);
    DWORD pid = 0;
    w->r[0] = (U(GetWindowThreadProcessId)(w->hwnd, &pid) == GetCurrentThreadId()) && pid == GetCurrentProcessId();
    SetEvent(w->ready);
    return 0;
}
TEST(winsta_process_thread_exit_destroys_windows) {
    REQUIRE(U(IsWindow)); REQUIRE(U(GetWindowThreadProcessId));
    wk *w = wk_start(wk_create_exit);
    CHECK(wk_wait_ready(w));
    WaitForSingleObject(w->th, 8000);
    EXPECT_EQ(w->r[0], 1);
    CHECK(w->hwnd != NULL && w->hwnd2 != NULL);
    CHECK(!U(IsWindow)(w->hwnd));
    CHECK(!U(IsWindow)(w->hwnd2));
    SetLastError(0);
    CHECK(U(GetWindowThreadProcessId)(w->hwnd, NULL) == 0);
    SetLastError(0);
    CHECK(!U(IsWindowVisible)(w->hwnd));
    wk_free(w);
}
TEST(winsta_process_dead_window_apis_fail) {
    REQUIRE(U(IsWindow)); REQUIRE(U(SendMessageA));
    wk *w = wk_start(wk_create_exit);
    CHECK(wk_wait_ready(w));
    WaitForSingleObject(w->th, 8000);
    SetLastError(0);
    CHECK(U(SendMessageA)(w->hwnd, WM_USER, 0, 0) == 0);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(PostMessageA)(w->hwnd, WM_USER, 0, 0));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(DestroyWindow)(w->hwnd));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(ShowWindow)(w->hwnd, SW_SHOW));
    wk_free(w);
}
TEST(winsta_process_destroywindow_cross_thread_denied) {
    REQUIRE(U(DestroyWindow)); REQUIRE(U(IsWindow));
    wk *w = wk_start(wk_gui_window);
    if (!wk_wait_ready(w)) { CHECK(0); wk_free(w); return; }
    CHECK(U(IsWindow)(w->hwnd));
    SetLastError(0);
    CHECK(!U(DestroyWindow)(w->hwnd));
    EXPECT_ERR(0, ERROR_ACCESS_DENIED);
    CHECK(U(IsWindow)(w->hwnd));
    DWORD pid = 0;
    EXPECT_EQ(U(GetWindowThreadProcessId)(w->hwnd, &pid), w->tid);
    EXPECT_EQ(pid, GetCurrentProcessId());
    wk_free(w);
}
TEST(winsta_process_postthreadmessage_no_queue) {
    REQUIRE(U(PostThreadMessageA));
    wk *w = wk_start(wk_idle);
    CHECK(wk_wait_ready(w));
    SetLastError(0);
    CHECK(!U(PostThreadMessageA)(w->tid, WM_USER, 1, 2));
    EXPECT_ERR(0, ERROR_INVALID_THREAD_ID);
    wk_free(w);
}
TEST(winsta_process_postthreadmessage_bogus_tid) {
    REQUIRE(U(PostThreadMessageA)); REQUIRE(U(PostThreadMessageW));
    SetLastError(0);
    CHECK(!U(PostThreadMessageA)(0xfffffff0, WM_USER, 0, 0));
    EXPECT_ERR(0, ERROR_INVALID_THREAD_ID);
    SetLastError(0);
    CHECK(!U(PostThreadMessageW)(0xfffffff0, WM_USER, 0, 0));
    EXPECT_ERR(0, ERROR_INVALID_THREAD_ID);
    SetLastError(0);
    BOOL r = U(PostThreadMessageA)(0, WM_USER, 0, 0);
    t_note("PostThreadMessage(tid 0) err %lu", GetLastError());
}
static DWORD wk_msgloop(LPVOID p) {
    wk *w = p; MSG m;
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);
    SetEvent(w->ready);
    w->r[0] = 0;
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < 5000) {
        if (U(PeekMessageA)(&m, NULL, WM_APP, WM_APP + 100, PM_REMOVE)) {
            w->r[0]++;
            w->r[1] = (LONG)m.message; w->r[2] = (LONG)m.wParam; w->r[3] = (LONG)m.lParam; w->r[4] = (m.hwnd == NULL);
            if (m.message == WM_APP + 9) break;
        } else Sleep(2);
    }
    return 0;
}
TEST(winsta_process_postthreadmessage_delivery) {
    REQUIRE(U(PostThreadMessageA));
    wk *w = wk_start(wk_msgloop);
    CHECK(wk_wait_ready(w));
    CHECK(U(PostThreadMessageA)(w->tid, WM_APP + 3, 0x1234, 0x5678));
    CHECK(U(PostThreadMessageW)(w->tid, WM_APP + 9, 11, 22));
    WaitForSingleObject(w->th, 8000);
    EXPECT_EQ(w->r[0], 2);
    EXPECT_EQ(w->r[1], WM_APP + 9); EXPECT_EQ(w->r[2], 11); EXPECT_EQ(w->r[3], 22); EXPECT_EQ(w->r[4], 1);
    wk_free(w);
}
TEST(winsta_process_getwindowthreadprocessid_basic) {
    REQUIRE(U(GetWindowThreadProcessId));
    HWND h = mkwin(WS_POPUP); CHECK(h != NULL);
    DWORD pid = 0;
    EXPECT_EQ(U(GetWindowThreadProcessId)(h, &pid), GetCurrentThreadId());
    EXPECT_EQ(pid, GetCurrentProcessId());
    EXPECT_EQ(U(GetWindowThreadProcessId)(h, NULL), GetCurrentThreadId());
    SetLastError(0);
    pid = 0x77;
    CHECK(U(GetWindowThreadProcessId)(NULL, &pid) == 0);
    t_note("pid after GetWindowThreadProcessId(NULL): 0x%lx", (unsigned long)pid);
    CHECK(U(GetWindowThreadProcessId)((HWND)(ULONG_PTR)0x1234567, &pid) == 0);
    if (h) U(DestroyWindow)(h);
}
TEST(winsta_process_getthreaddesktop_worker_inherits) {
    REQUIRE(U(GetThreadDesktop));
    wk *w = wk_new(); w->orig = U(GetThreadDesktop)(GetCurrentThreadId());
    w->th = CreateThread(NULL, 0, wk_setdesk_basic, w, 0, &w->tid);
    CHECK(wk_wait_ready(w));
    EXPECT_EQ(w->r[0], 1);
    wk_free(w);
}
