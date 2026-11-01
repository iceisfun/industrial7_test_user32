/* Message loop edge cases: MsgWait bWaitAll, unmarshalled pointers across threads, chained sends,
 * sender release when the receiver thread dies, foreign-thread window filters. */
#include "mh.h"

#define MU(n) (WM_USER + (n))

/* ---- MsgWaitForMultipleObjects with bWaitAll ---- */
TEST(messages_x2_msgwait_waitall_unsignaled_handle_times_out) {
    HANDLE ev; HWND h; DWORD r;
    REQUIRE(PMA); REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(PMA(h, MU(300), 0, 0));
    /* the handle is never signaled, so waiting for all of them (plus input) cannot complete */
    r = U(MsgWaitForMultipleObjects)(1, &ev, TRUE, 150, QS_ALLINPUT);
    EXPECT_EQ(r, WAIT_TIMEOUT);
    mh_drain();
    CloseHandle(ev);
    mh_kill(h);
}

TEST(messages_x2_msgwait_waitall_signaled_and_input_completes) {
    HANDLE ev; HWND h; DWORD r; DWORD t0;
    REQUIRE(PMA); REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    ev = CreateEventA(NULL, TRUE, TRUE, NULL);
    CHECK(PMA(h, MU(301), 0, 0));
    t0 = GetTickCount();
    r = U(MsgWaitForMultipleObjects)(1, &ev, TRUE, 3000, QS_ALLINPUT);
    CHECK(r != WAIT_TIMEOUT && r != WAIT_FAILED);
    CHECK(GetTickCount() - t0 < 2500);
    t_note("bWaitAll, handle signaled + posted message -> 0x%lx (WAIT_OBJECT_0+nCount is 0x%lx)", r, (DWORD)(WAIT_OBJECT_0 + 1));
    mh_drain();
    /* handle signaled, queue empty */
    r = U(MsgWaitForMultipleObjects)(1, &ev, TRUE, 100, QS_ALLINPUT);
    t_note("bWaitAll, handle signaled + empty queue -> 0x%lx", r);
    CloseHandle(ev);
    mh_kill(h);
}

/* ---- pointers in lParam are passed as-is between threads of one process ---- */
typedef struct { DWORD tid; LONG val; ULONG_PTR echo; } X2P;
static LRESULT x2_ptr_hook(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_NOTIFY || m == MU(302)) {
        X2P *p = (X2P *)l;
        p->tid = GetCurrentThreadId(); p->val += 1; p->echo = (ULONG_PTR)l;
        *hd = 1;
        return 7;
    }
    *hd = 0;
    return 0;
}
static void x2_ptr_case(UINT msg) {
    MH_TH t; X2P p; DWORD_PTR res = 0; int ok;
    REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    memset(&p, 0, sizeof p);
    mh_hook = x2_ptr_hook;
    ok = mh_xsend(t.hwnd, msg, 5, (LPARAM)&p, &res);
    CHECK(ok);
    EXPECT_EQ(res, 7);
    EXPECT_EQ(p.val, 1);                 /* the receiver wrote into the sender's memory: no copy */
    EXPECT_EQ(p.tid, t.tid);
    EXPECT_EQ(p.echo, (ULONG_PTR)&p);
    mh_hook = NULL;
    mh_th_stop(&t);
}
TEST(messages_x2_wm_notify_pointer_passed_unmarshalled_cross_thread) { REQUIRE(SMA); x2_ptr_case(WM_NOTIFY); }
TEST(messages_x2_wm_user_pointer_passed_unmarshalled_cross_thread) { REQUIRE(SMA); x2_ptr_case(MU(302)); }

/* ---- GetQueueStatus after PeekMessage(PM_NOREMOVE) ---- */
TEST(messages_x2_queuestatus_allpostmessage_after_noremove_peek) {
    HWND h; MSG m; DWORD s;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE(U(GetQueueStatus)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    CHECK(PMA(h, MU(310), 0, 0));
    CHECK(PKA(&m, h, MU(310), MU(310), PM_NOREMOVE));
    s = U(GetQueueStatus)(QS_ALLPOSTMESSAGE);
    CHECK((s >> 16) & (QS_POSTMESSAGE | QS_ALLPOSTMESSAGE));
    t_note("GetQueueStatus(QS_ALLPOSTMESSAGE) after NOREMOVE peek = 0x%lx", s);
    s = U(GetQueueStatus)(QS_POSTMESSAGE);
    t_note("GetQueueStatus(QS_POSTMESSAGE) after NOREMOVE peek = 0x%lx", s);
    CHECK(PKA(&m, h, MU(310), MU(310), PM_REMOVE));
    s = U(GetQueueStatus)(QS_ALLPOSTMESSAGE);
    CHECK(((s >> 16) & (QS_POSTMESSAGE | QS_ALLPOSTMESSAGE)) == 0);
    mh_drain();
    mh_kill(h);
}

/* ---- main -> worker1 -> worker2 -> main, every hop a blocking send ---- */
static HWND x2_w2, x2_main;
static volatile LONG x2_main_tid_seen;
static LRESULT x2_chain_hook(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    DWORD_PTR r = 0;
    *hd = 1;
    switch (m) {
    case MU(320): U(SendMessageTimeoutA)(x2_w2, MU(321), 0, 0, SMTO_NORMAL, 4000, &r); return (LRESULT)(r + 1);
    case MU(321): U(SendMessageTimeoutA)(x2_main, MU(322), 0, 0, SMTO_NORMAL, 4000, &r); return (LRESULT)(r + 10);
    case MU(322): x2_main_tid_seen = (LONG)GetCurrentThreadId(); return 100;
    }
    *hd = 0;
    return 0;
}
TEST(messages_x2_send_chain_three_threads_returns_to_blocked_sender) {
    MH_TH t1, t2; DWORD_PTR res = 0; int ok;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    x2_main = mh_mk(0, 0, NULL); REQUIRE(x2_main);
    if (!mh_th_start(&t1, 0, 0)) { CHECK(0); mh_th_stop(&t1); mh_kill(x2_main); return; }
    if (!mh_th_start(&t2, 0, 0)) { CHECK(0); mh_th_stop(&t2); mh_th_stop(&t1); mh_kill(x2_main); return; }
    x2_w2 = t2.hwnd; x2_main_tid_seen = 0;
    mh_hook = x2_chain_hook;
    ok = mh_xsend(t1.hwnd, MU(320), 0, 0, &res);
    CHECK(ok);
    EXPECT_EQ(res, 111);
    /* the sent message reached us while we were blocked inside SendMessageTimeout */
    EXPECT_EQ(x2_main_tid_seen, GetCurrentThreadId());
    mh_hook = NULL;
    mh_th_stop(&t2); mh_th_stop(&t1);
    mh_kill(x2_main); x2_main = x2_w2 = NULL;
}

/* ---- the receiver thread terminates inside the handler: the blocked sender must be released ---- */
static LRESULT x2_exit_hook(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == MU(330)) ExitThread(0);
    *hd = 0;
    return 0;
}
TEST(messages_x2_sender_released_when_receiver_thread_exits) {
    MH_TH t; DWORD_PTR res = 0xabcd; LRESULT r; DWORD t0, el;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_hook = x2_exit_hook;
    t0 = GetTickCount();
    SetLastError(0);
    r = U(SendMessageTimeoutA)(t.hwnd, MU(330), 0, 0, SMTO_NORMAL, 8000, &res);
    el = GetTickCount() - t0;
    CHECK(el < 6000);                       /* released by the thread's death, not by the timeout */
    t_note("sender released after %lu ms: ret=%lld res=0x%llx err=%lu", el, (long long)r, (unsigned long long)res, GetLastError());
    mh_hook = NULL;
    WaitForSingleObject(t.th, 3000);
    CHECK(!ISWND(t.hwnd));                  /* windows die with their thread */
    mh_th_stop(&t);
}

/* ---- windows of another thread as a Peek filter / dispatch target ---- */
static volatile LONG x2_disp_tid;
static LRESULT x2_disp_hook(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == MU(340)) { x2_disp_tid = (LONG)GetCurrentThreadId(); *hd = 1; return 9; }
    *hd = 0;
    return 0;
}
TEST(messages_x2_peek_filter_hwnd_of_other_thread_returns_false) {
    MH_TH t; MSG m; BOOL r;
    REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    SetLastError(NOERROR);
    r = PKA(&m, t.hwnd, 0, 0, PM_REMOVE);
    CHECK(r == FALSE);
    t_note("PeekMessage(hwnd of another thread) err=%lu", GetLastError());
    mh_th_stop(&t);
}
TEST(messages_x2_dispatch_message_for_other_thread_window_observed) {
    MH_TH t; MSG m; LRESULT r;
    REQUIRE(U(DispatchMessageA)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_hook = x2_disp_hook; x2_disp_tid = 0;
    memset(&m, 0, sizeof m);
    m.hwnd = t.hwnd; m.message = MU(340); m.time = GetTickCount();
    r = U(DispatchMessageA)(&m);
    t_note("DispatchMessageA(foreign window) -> %lld, handler ran on %s thread", (long long)r,
           x2_disp_tid == 0 ? "no" : x2_disp_tid == (LONG)t.tid ? "the owner" : "the calling");
    mh_hook = NULL;
    mh_th_stop(&t);
}
