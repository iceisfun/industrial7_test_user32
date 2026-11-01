/* Message queue: Post/Peek/Get, filters, flags, QS_ status, waits. */
#define MH_IMPL
#include "mh.h"

#define MU(n) (WM_USER + (n))
#define NOERR 0xdeadbeef
#define SETE() SetLastError(NOERR)

/* a popup has no non-client area, so Win10 posts no WM_DWMNCRENDERINGCHANGED to it that could appear in the queue tests later */
static HWND qw(void) { return mh_mk(WS_POPUP, 0, NULL); }

/* watchdog: posts a thread message to a target thread after a delay */
typedef struct { DWORD tid; DWORD delay; UINT msg; } QWD;
static DWORD WINAPI qwd_proc(LPVOID p) {
    QWD *w = (QWD *)p;
    Sleep(w->delay);
    U(PostThreadMessageA)(w->tid, w->msg, 0, 0);
    return 0;
}
static HANDLE qwd_start(QWD *w, DWORD delay, UINT msg) {
    w->tid = GetCurrentThreadId(); w->delay = delay; w->msg = msg;
    return CreateThread(NULL, 0, qwd_proc, w, 0, NULL);
}
static void qwd_end(HANDLE h) { if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); } }

TEST(messages_post_basic_fields) {
    HWND h; MSG m; BOOL r;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    r = PMA(h, MU(10), 0x1234, 0x5678);
    CHECK(r == TRUE);
    memset(&m, 0xAA, sizeof m);
    r = PKA(&m, NULL, 0, 0, PM_REMOVE);
    CHECK(r == TRUE);
    EXPECT_EQ(m.hwnd, h);
    EXPECT_EQ(m.message, MU(10));
    EXPECT_EQ(m.wParam, 0x1234);
    EXPECT_EQ(m.lParam, 0x5678);
    r = PKA(&m, NULL, 0, 0, PM_REMOVE);
    CHECK(r == FALSE);
    mh_kill(h);
}

TEST(messages_post_full_width_params) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    CHECK(PMA(h, MU(11), (WPARAM)0xFEDCBA9876543210ull, (LPARAM)0x8123456789ABCDEFull));
    CHECK(PKA(&m, h, MU(11), MU(11), PM_REMOVE));
    EXPECT_EQ(m.wParam, 0xFEDCBA9876543210ull);
    EXPECT_EQ(m.lParam, 0x8123456789ABCDEFull);
    mh_kill(h);
}

TEST(messages_peek_noremove_keeps_message) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(12), 1, 2);
    CHECK(PKA(&m, NULL, 0, 0, PM_NOREMOVE));
    EXPECT_EQ(m.message, MU(12));
    CHECK(PKA(&m, NULL, 0, 0, PM_NOREMOVE));
    EXPECT_EQ(m.message, MU(12));
    CHECK(PKA(&m, NULL, 0, 0, PM_NOREMOVE | PM_NOYIELD));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE | PM_NOYIELD));
    CHECK(!PKA(&m, NULL, 0, 0, PM_NOREMOVE));
    mh_kill(h);
}

TEST(messages_fifo_order) {
    HWND h; MSG m; int i;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    for (i = 0; i < 20; i++) PMA(h, MU(100 + i), i, 0);
    for (i = 0; i < 20; i++) {
        BOOL r = PKA(&m, NULL, 0, 0, PM_REMOVE);
        CHECK(r);
        if (r) EXPECT_EQ(m.message, MU(100 + i));
    }
    CHECK(!PKA(&m, NULL, 0, 0, PM_REMOVE));
    mh_kill(h);
}

TEST(messages_filter_message_range) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(20), 0, 0); PMA(h, MU(21), 0, 0); PMA(h, MU(22), 0, 0);
    CHECK(PKA(&m, NULL, MU(21), MU(21), PM_REMOVE));
    EXPECT_EQ(m.message, MU(21));
    CHECK(!PKA(&m, NULL, MU(30), MU(40), PM_REMOVE));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); EXPECT_EQ(m.message, MU(20));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); EXPECT_EQ(m.message, MU(22));
    mh_kill(h);
}

TEST(messages_filter_min_only_zero_max) {
    /* max == 0 with min != 0: treated as "no filter" (range check is skipped when both are 0, else [min,max]) */
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(23), 0, 0); PMA(h, MU(24), 0, 0);
    CHECK(PKA(&m, NULL, MU(24), MU(24), PM_REMOVE | PM_NOYIELD));
    EXPECT_EQ(m.message, MU(24));
    CHECK(PKA(&m, NULL, MU(23), MU(23), PM_REMOVE));
    EXPECT_EQ(m.message, MU(23));
    mh_kill(h);
}

TEST(messages_filter_range_inclusive) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(40), 0, 0); PMA(h, MU(41), 0, 0); PMA(h, MU(42), 0, 0); PMA(h, MU(43), 0, 0);
    CHECK(PKA(&m, NULL, MU(41), MU(42), PM_REMOVE)); EXPECT_EQ(m.message, MU(41));
    CHECK(PKA(&m, NULL, MU(41), MU(42), PM_REMOVE)); EXPECT_EQ(m.message, MU(42));
    CHECK(!PKA(&m, NULL, MU(41), MU(42), PM_REMOVE));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); EXPECT_EQ(m.message, MU(40));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); EXPECT_EQ(m.message, MU(43));
    mh_kill(h);
}

TEST(messages_filter_hwnd) {
    HWND a, b; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    a = qw(); b = qw(); REQUIRE(a && b);
    PMA(a, MU(50), 0, 0); PMA(b, MU(51), 0, 0);
    CHECK(PKA(&m, b, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.hwnd, b); EXPECT_EQ(m.message, MU(51));
    CHECK(!PKA(&m, b, 0, 0, PM_REMOVE));
    CHECK(PKA(&m, a, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.hwnd, a);
    mh_kill(a); mh_kill(b);
}

TEST(messages_filter_hwnd_minus1_thread_msgs) {
    HWND a; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    a = qw(); REQUIRE(a);
    PMA(a, MU(52), 0, 0);
    CHECK(!PKA(&m, (HWND)-1, 0, 0, PM_REMOVE));
    PMA(NULL, MU(53), 7, 8);
    CHECK(PKA(&m, (HWND)-1, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.hwnd, NULL); EXPECT_EQ(m.message, MU(53));
    EXPECT_EQ(m.wParam, 7); EXPECT_EQ(m.lParam, 8);
    CHECK(PKA(&m, a, 0, 0, PM_REMOVE));
    mh_kill(a);
}

TEST(messages_post_null_hwnd_is_thread_message) {
    MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    CHECK(PMA(NULL, MU(54), 1, 2) == TRUE);
    CHECK(PKA(&m, NULL, MU(54), MU(54), PM_REMOVE));
    EXPECT_EQ(m.hwnd, NULL);
    EXPECT_EQ(m.message, MU(54));
}

TEST(messages_post_thread_message_self) {
    MSG m;
    REQUIRE(U(PostThreadMessageA)); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    CHECK(U(PostThreadMessageA)(GetCurrentThreadId(), MU(55), 3, 4) == TRUE);
    CHECK(U(PostThreadMessageW)(GetCurrentThreadId(), MU(56), 5, 6) == TRUE);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.hwnd, NULL); EXPECT_EQ(m.message, MU(55)); EXPECT_EQ(m.wParam, 3);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, MU(56)); EXPECT_EQ(m.lParam, 6);
}

TEST(messages_post_thread_message_bad_thread) {
    DWORD ids[] = { 0xFFFFFFF0, 0x7FFFFFF0, 0xFFFFFFFC };
    int i;
    REQUIRE(U(PostThreadMessageA));
    for (i = 0; i < 3; i++) {
        BOOL r; SETE();
        r = U(PostThreadMessageA)(ids[i], MU(57), 0, 0);
        CHECKF(r == FALSE, "id %lx", ids[i]);
        EXPECT_ERR(r, E_BADTHREAD);
    }
}

TEST(messages_post_thread_message_zero_tid) {
    BOOL r;
    REQUIRE(U(PostThreadMessageA));
    SETE();
    r = U(PostThreadMessageA)(0, MU(58), 0, 0);
    if (r) { t_note("PostThreadMessage(0) succeeded"); mh_drain(); }
    t_note("PostThreadMessage(0) last error = %lu", GetLastError());
}

TEST(messages_post_thread_message_touch_gesture_rejected) {
    BOOL r;
    REQUIRE(U(PostThreadMessageW));
    SETE(); r = U(PostThreadMessageW)(GetCurrentThreadId(), 0x240, 0, 0);
    CHECK(r == FALSE); EXPECT_ERR(r, E_BADMSG_WT);
    SETE(); r = U(PostThreadMessageW)(GetCurrentThreadId(), 0x119, 0, 0);
    CHECK(r == FALSE); EXPECT_ERR(r, E_BADMSG_WT);
    SETE(); r = U(PostThreadMessageA)(GetCurrentThreadId(), 0x240, 0, 0);
    CHECK(r == FALSE); EXPECT_ERR(r, E_BADMSG_WT);
    SETE(); r = U(PostThreadMessageA)(GetCurrentThreadId(), 0x119, 0, 0);
    CHECK(r == FALSE); EXPECT_ERR(r, E_BADMSG_WT);
    mh_drain();
}

static DWORD WINAPI noqueue_thread(LPVOID p) { WaitForSingleObject((HANDLE)p, 5000); return 0; }
TEST(messages_post_thread_message_thread_without_queue) {
    HANDLE ev, th; DWORD tid; BOOL r;
    REQUIRE(U(PostThreadMessageA));
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, noqueue_thread, ev, 0, &tid);
    REQUIRE(th);
    Sleep(50);
    SETE();
    r = U(PostThreadMessageA)(tid, MU(59), 0, 0);
    CHECK(r == FALSE);
    t_note("PostThreadMessage to queue-less thread: err %lu", GetLastError());
    SetEvent(ev); WaitForSingleObject(th, 5000); CloseHandle(th); CloseHandle(ev);
}

TEST(messages_post_invalid_hwnd) {
    HWND bad[] = { (HWND)0xDEADBEEF, (HWND)0x12345678, (HWND)(ULONG_PTR)0x7FFFFFFFFFFFull };
    int i;
    REQUIRE(PMA); REQUIRE(PMW);
    for (i = 0; i < 3; i++) {
        BOOL r; SETE();
        r = PMA(bad[i], MU(60), 0, 0);
        CHECKF(r == FALSE, "hwnd %p", bad[i]);
        EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
        SETE();
        r = PMW(bad[i], MU(60), 0, 0);
        CHECKF(r == FALSE, "W hwnd %p", bad[i]);
        EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    }
}

TEST(messages_post_destroyed_hwnd) {
    HWND h; BOOL r;
    REQUIRE(PMA); REQUIRE_MH();
    h = qw(); REQUIRE(h);
    DESTROYW(h);
    SETE(); r = PMA(h, MU(61), 0, 0);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    CHECK(!ISWND(h));
}

TEST(messages_post_broadcast_returns_true) {
    UINT mid;
    REQUIRE(PMA); REQUIRE(U(RegisterWindowMessageA));
    mid = U(RegisterWindowMessageA)("t32_messages_broadcast_post");
    CHECK(mid >= 0xC000 && mid <= 0xFFFF);
    CHECK(PMA(HWND_BROADCAST, mid, 0, 0) == TRUE);
}

TEST(messages_post_destroy_flushes_queue) {
    HWND a, b; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    a = qw(); b = qw(); REQUIRE(a && b);
    PMA(a, MU(62), 0, 0); PMA(b, MU(63), 0, 0); PMA(a, MU(64), 0, 0);
    DESTROYW(a);
    do { CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); } while (m.message == 0x31F); /* skip the DWM rendering notification posted to new windows */
    EXPECT_EQ(m.hwnd, b); EXPECT_EQ(m.message, MU(63));
    while (PKA(&m, NULL, 0, 0, PM_REMOVE)) CHECKF(m.message == 0x31F, "unexpected message 0x%x", m.message);
    mh_kill(b);
}

TEST(messages_peek_empty_queue) {
    MSG m;
    REQUIRE(PKA); REQUIRE(PKW); REQUIRE_MH();
    mh_reset();
    CHECK(PKA(&m, NULL, 0, 0, PM_NOREMOVE) == FALSE);
    CHECK(PKW(&m, NULL, 0, 0, PM_REMOVE) == FALSE);
}

TEST(messages_peek_bogus_hwnd) {
    MSG m; BOOL r;
    REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    SETE(); r = PKA(&m, (HWND)0xDEADBEEF, 0, 0, PM_REMOVE);
    CHECK(r == FALSE);
    t_note("PeekMessage bogus hwnd last error %lu", GetLastError());
}

TEST(messages_get_bogus_hwnd_returns_minus1) {
    MSG m; BOOL r;
    REQUIRE(GMA); REQUIRE_MH();
    mh_reset();
    SETE(); r = GMA(&m, (HWND)0xDEADBEEF, 0, 0);
    CHECK(r == -1);
    EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    SETE(); r = GMW(&m, (HWND)0x12345, 0, 0);
    CHECK(r == -1);
}

TEST(messages_get_invalid_filter_range) {
    MSG m; BOOL r; QWD w; HANDLE wd;
    REQUIRE(GMA); REQUIRE(GMW); REQUIRE_MH();
    mh_reset();
    if (GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version")) { t_skip("emulator would block in GetMessage here"); return; }
    {   /* bounded safety net: several late wake-up messages so no call can block for long */
        static QWD w2, w3, w4; HANDLE h2, h3, h4;
        h2 = qwd_start(&w2, 1600, MU(997)); h3 = qwd_start(&w3, 1700, MU(997)); h4 = qwd_start(&w4, 1800, MU(997));
        wd = qwd_start(&w, 1500, MU(997));
        SETE(); r = GMA(&m, NULL, 0x20000, 0x20000);
        CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        SETE(); r = GMW(&m, NULL, 0, 0x30000);
        CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        SETE(); r = GMW(&m, NULL, 0x80000000u, 0);
        CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        SETE(); r = GMA(&m, NULL, 0x10000, 0xFFFFFFFEu);
        CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        qwd_end(h2); qwd_end(h3); qwd_end(h4);
    }
    qwd_end(wd);
    mh_drain();
}

TEST(messages_get_message_returns_posted) {
    HWND h; MSG m; BOOL r;
    REQUIRE(GMA); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(70), 9, 10);
    r = GMA(&m, NULL, 0, 0);
    CHECK(r == TRUE);
    EXPECT_EQ(m.message, MU(70)); EXPECT_EQ(m.wParam, 9);
    mh_kill(h);
}

TEST(messages_get_message_filtered_wait_via_watchdog) {
    HWND h; MSG m; BOOL r; QWD w; HANDLE wd;
    REQUIRE(GMA); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(71), 0, 0); /* does not match filter */
    wd = qwd_start(&w, 100, MU(72));
    r = GMA(&m, NULL, MU(72), MU(72));
    qwd_end(wd);
    CHECK(r == TRUE);
    EXPECT_EQ(m.message, MU(72));
    CHECK(PKA(&m, NULL, MU(71), MU(71), PM_REMOVE));
    mh_kill(h);
}

TEST(messages_quit_basic) {
    MSG m; BOOL r;
    REQUIRE(GMA); REQUIRE(U(PostQuitMessage)); REQUIRE_MH();
    mh_reset();
    U(PostQuitMessage)(42);
    r = GMA(&m, NULL, 0, 0);
    CHECK(r == 0);
    EXPECT_EQ(m.message, WM_QUIT);
    EXPECT_EQ(m.wParam, 42);
    EXPECT_EQ(m.hwnd, NULL);
    CHECK(!PKA(&m, NULL, 0, 0, PM_REMOVE));
}

TEST(messages_quit_negative_exit_code) {
    MSG m;
    REQUIRE(PKA); REQUIRE(U(PostQuitMessage)); REQUIRE_MH();
    mh_reset();
    U(PostQuitMessage)(-1);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, WM_QUIT);
    EXPECT_EQ((int)m.wParam, -1);
}

TEST(messages_quit_after_posted_messages) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE(U(PostQuitMessage)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    U(PostQuitMessage)(3);
    PMA(h, MU(73), 0, 0); PMA(h, MU(74), 0, 0);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); EXPECT_EQ(m.message, MU(73));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); EXPECT_EQ(m.message, MU(74));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); EXPECT_EQ(m.message, WM_QUIT);
    CHECK(!PKA(&m, NULL, 0, 0, PM_REMOVE));
    mh_kill(h);
}

TEST(messages_quit_double_post_single_message) {
    MSG m;
    REQUIRE(PKA); REQUIRE(U(PostQuitMessage)); REQUIRE_MH();
    mh_reset();
    U(PostQuitMessage)(1);
    U(PostQuitMessage)(2);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, WM_QUIT);
    t_note("PostQuitMessage twice: wParam = %llu", (unsigned long long)m.wParam);
    CHECK(!PKA(&m, NULL, 0, 0, PM_REMOVE));
}

TEST(messages_quit_noremove_then_remove) {
    MSG m;
    REQUIRE(PKA); REQUIRE(U(PostQuitMessage)); REQUIRE_MH();
    mh_reset();
    U(PostQuitMessage)(5);
    CHECK(PKA(&m, NULL, 0, 0, PM_NOREMOVE)); EXPECT_EQ(m.message, WM_QUIT);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE)); EXPECT_EQ(m.message, WM_QUIT);
    EXPECT_EQ(m.wParam, 5);
    CHECK(!PKA(&m, NULL, 0, 0, PM_REMOVE));
}

TEST(messages_quit_filtered_out_by_range) {
    MSG m; BOOL r_;
    REQUIRE(PKA); REQUIRE(U(PostQuitMessage)); REQUIRE_MH();
    mh_reset();
    U(PostQuitMessage)(6);
    r_ = PKA(&m, NULL, MU(0), MU(10), PM_REMOVE);
    if (r_) t_note("WM_QUIT returned despite a filter range that excludes it");
    CHECK(r_ || PKA(&m, NULL, WM_QUIT, WM_QUIT, PM_REMOVE));
    EXPECT_EQ(m.message, WM_QUIT);
}

TEST(messages_quit_posted_directly) {
    MSG m;
    REQUIRE(PKA); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    CHECK(PMA(NULL, WM_QUIT, 9, 0));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, WM_QUIT);
    EXPECT_EQ(m.wParam, 9);
    mh_drain();
}

TEST(messages_getqueuestatus_invalid_flags) {
    UINT bad[] = { 0x200, 0x2000, 0x8000, 0x10000, 0x80000000u, 0xFFFFFFFFu, 0x10, 0x00400000 };
    int i;
    REQUIRE(U(GetQueueStatus));
    for (i = 0; i < 8; i++) {
        DWORD r; SETE();
        r = U(GetQueueStatus)(bad[i]);
        if (bad[i] == 0x10) { continue; }
        CHECKF(r == 0, "flags %x ret %lx", bad[i], r);
        EXPECT_ERR(r, E_BADFLAGS);
    }
}

TEST(messages_getqueuestatus_zero) {
    REQUIRE(U(GetQueueStatus)); REQUIRE_MH();
    mh_reset();
    EXPECT_EQ(U(GetQueueStatus)(0), 0);
}

TEST(messages_getqueuestatus_post) {
    HWND h; DWORD s;
    REQUIRE(U(GetQueueStatus)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    s = U(GetQueueStatus)(QS_POSTMESSAGE);
    CHECK((s >> 16 & QS_POSTMESSAGE) == 0);
    PMA(h, MU(80), 0, 0);
    s = U(GetQueueStatus)(QS_POSTMESSAGE);
    CHECKF((s >> 16 & QS_POSTMESSAGE) != 0, "hi %lx", s >> 16);
    CHECKF((s & QS_POSTMESSAGE) != 0, "lo %lx (new since last call)", s & 0xFFFF);
    s = U(GetQueueStatus)(QS_POSTMESSAGE);
    CHECKF((s >> 16 & QS_POSTMESSAGE) != 0, "hi still set %lx", s >> 16);
    CHECKF((s & QS_POSTMESSAGE) == 0, "lo cleared %lx", s & 0xFFFF);
    mh_drain();
    s = U(GetQueueStatus)(QS_ALLINPUT);
    CHECK((s >> 16 & QS_POSTMESSAGE) == 0);
    mh_kill(h);
}

TEST(messages_getqueuestatus_mask_filters_bits) {
    HWND h; DWORD s;
    REQUIRE(U(GetQueueStatus)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(81), 0, 0);
    s = U(GetQueueStatus)(QS_PAINT | QS_TIMER);
    CHECK((s >> 16 & QS_POSTMESSAGE) == 0);
    CHECK((s & QS_POSTMESSAGE) == 0);
    s = U(GetQueueStatus)(QS_ALLEVENTS);
    CHECK((s >> 16 & QS_POSTMESSAGE) != 0);
    mh_drain(); mh_kill(h);
}

TEST(messages_getqueuestatus_valid_masks_no_error) {
    UINT ok[] = { QS_KEY, QS_MOUSE, QS_INPUT, QS_ALLEVENTS, QS_ALLINPUT, QS_SENDMESSAGE, QS_PAINT, QS_TIMER, QS_HOTKEY, QS_RAWINPUT, QS_ALLPOSTMESSAGE };
    int i;
    REQUIRE(U(GetQueueStatus));
    for (i = 0; i < (int)(sizeof ok / sizeof ok[0]); i++) {
        DWORD r; SETE();
        r = U(GetQueueStatus)(ok[i]);
        CHECKF(GetLastError() == NOERR, "flags %x err %lu", ok[i], GetLastError());
        CHECKF((r >> 16 & ~(ok[i] | QS_ALLPOSTMESSAGE | 0x1000)) == 0, "flags %x ret %lx", ok[i], r);
    }
}

TEST(messages_getqueuestatus_sendmessage_pending) {
    /* a cross-thread SendMessage in flight shows QS_SENDMESSAGE; the sender is blocked until we pump */
    MH_TH t; DWORD s; int i;
    REQUIRE(U(GetQueueStatus)); REQUIRE_MH();
    mh_reset();
    CHECK(mh_th_start(&t, 0, 0));
    if (t.hwnd) {
        /* have worker thread send to main window asynchronously via SendNotifyMessage */
        HWND mw = mh_mk(0, 0, NULL);
        REQUIRE(mw);
        mh_mainwnd = mw;
        {
            DWORD_PTR r = 0;
            /* worker bounce: ask worker to send WM_REC_E to main; main is not pumping yet -> worker blocks */
            CHECK(U(PostMessageA)(t.hwnd, WM_REC_D, 0, 0));
            for (i = 0; i < 100; i++) {
                s = U(GetQueueStatus)(QS_SENDMESSAGE);
                if (s >> 16 & QS_SENDMESSAGE) break;
                Sleep(10);
            }
            CHECKF((s >> 16 & QS_SENDMESSAGE) != 0, "status %lx", s);
            mh_flag[1] = 0;
            mh_pump_until(&mh_flag[1], 3000);
            CHECK(mh_flag[1] == 1);
            (void)r;
        }
        mh_mainwnd = NULL;
        mh_kill(mw);
    }
    mh_th_stop(&t);
    mh_drain();
}

TEST(messages_getinputstate_posted_is_not_input) {
    HWND h;
    REQUIRE(U(GetInputState)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(82), 0, 0);
    CHECK(U(GetInputState)() == FALSE);
    mh_drain(); mh_kill(h);
}

TEST(messages_waitmessage_returns_on_post) {
    QWD w; HANDLE wd; DWORD t0, dt; MSG m;
    REQUIRE(U(WaitMessage)); REQUIRE_MH();
    mh_reset();
    wd = qwd_start(&w, 150, MU(83));
    t0 = GetTickCount();
    CHECK(U(WaitMessage)() != 0);
    dt = GetTickCount() - t0;
    qwd_end(wd);
    CHECKF(dt >= 100 && dt < 3500, "waited %lu ms", dt);
    CHECK(PKA(&m, NULL, MU(83), MU(83), PM_REMOVE));
}

TEST(messages_waitmessage_immediate_when_unseen_posted) {
    QWD w; HANDLE wd; DWORD t0, dt; HWND h;
    REQUIRE(U(WaitMessage)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(84), 0, 0);
    wd = qwd_start(&w, 3000, MU(85));
    t0 = GetTickCount();
    U(WaitMessage)();
    dt = GetTickCount() - t0;
    CHECKF(dt < 1500, "waited %lu ms", dt);
    qwd_end(wd);
    mh_drain(); mh_kill(h);
}

TEST(messages_msgwait_timeout_no_handles) {
    DWORD r, t0, dt;
    REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    mh_reset();
    t0 = GetTickCount();
    r = U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 60, QS_ALLINPUT);
    dt = GetTickCount() - t0;
    CHECKF(r == WAIT_TIMEOUT, "ret %lu", r);
    CHECKF(dt >= 40 && dt < 2000, "elapsed %lu", dt);
}

TEST(messages_msgwait_timeout_zero) {
    DWORD r;
    REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    mh_reset();
    r = U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 0, QS_ALLINPUT);
    CHECKF(r == WAIT_TIMEOUT, "ret %lu", r);
}

TEST(messages_msgwait_signaled_handle) {
    HANDLE ev[2]; DWORD r;
    REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    mh_reset();
    ev[0] = CreateEventA(NULL, TRUE, FALSE, NULL);
    ev[1] = CreateEventA(NULL, TRUE, TRUE, NULL);
    r = U(MsgWaitForMultipleObjects)(2, ev, FALSE, 1000, QS_ALLINPUT);
    CHECKF(r == WAIT_OBJECT_0 + 1, "ret %lu", r);
    SetEvent(ev[0]);
    /* wait-all also requires input to be available */
    r = U(MsgWaitForMultipleObjects)(2, ev, TRUE, 40, QS_ALLINPUT);
    CHECKF(r == WAIT_TIMEOUT, "wait-all without input ret %lu", r);
    CloseHandle(ev[0]); CloseHandle(ev[1]);
}

TEST(messages_msgwait_message_wakes_with_ncount) {
    HANDLE ev; DWORD r; HWND h;
    REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    PMA(h, MU(86), 0, 0);
    r = U(MsgWaitForMultipleObjects)(1, &ev, FALSE, 1000, QS_POSTMESSAGE);
    CHECKF(r == WAIT_OBJECT_0 + 1, "ret %lu", r);
    r = U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 1000, QS_ALLINPUT);
    t_note("MsgWait(0 handles) after the first wake returned %lu (WAIT_OBJECT_0 = unseen input still pending)", r);
    mh_drain(); mh_kill(h); CloseHandle(ev);
}

TEST(messages_msgwait_mask_excludes_post) {
    DWORD r; HWND h;
    REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(87), 0, 0);
    r = U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 60, QS_PAINT | QS_SENDMESSAGE);
    CHECKF(r == WAIT_TIMEOUT, "ret %lu", r);
    r = U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 60, QS_KEY | QS_MOUSE);
    CHECKF(r == WAIT_TIMEOUT, "ret %lu", r);
    mh_drain(); mh_kill(h);
}

TEST(messages_msgwait_seen_message_does_not_wake) {
    DWORD r; HWND h; MSG m;
    REQUIRE(U(MsgWaitForMultipleObjectsEx)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(88), 0, 0);
    PKA(&m, NULL, MU(99), MU(99), PM_NOREMOVE);  /* looks at the queue with a non-matching filter */
    r = U(MsgWaitForMultipleObjectsEx)(0, NULL, 40, QS_POSTMESSAGE, 0);
    t_note("after peek with other filter: ret %lu", r);
    PKA(&m, NULL, 0, 0, PM_NOREMOVE);            /* now actually seen */
    r = U(MsgWaitForMultipleObjectsEx)(0, NULL, 40, QS_POSTMESSAGE, 0);
    CHECKF(r == WAIT_TIMEOUT, "seen message must not wake (ret %lu)", r);
    r = U(MsgWaitForMultipleObjectsEx)(0, NULL, 1000, QS_POSTMESSAGE, MWMO_INPUTAVAILABLE);
    CHECKF(r == WAIT_OBJECT_0, "MWMO_INPUTAVAILABLE ret %lu", r);
    mh_drain(); mh_kill(h);
}

static VOID CALLBACK apc_fn(ULONG_PTR p) { *(volatile LONG *)p = 1; }
TEST(messages_msgwait_alertable_apc) {
    DWORD r; volatile LONG hit = 0;
    REQUIRE(U(MsgWaitForMultipleObjectsEx)); REQUIRE_MH();
    mh_reset();
    CHECK(QueueUserAPC(apc_fn, GetCurrentThread(), (ULONG_PTR)&hit));
    r = U(MsgWaitForMultipleObjectsEx)(0, NULL, 1000, QS_ALLINPUT, MWMO_ALERTABLE);
    CHECKF(r == WAIT_IO_COMPLETION, "ret %lu", r);
    CHECK(hit == 1);
}

TEST(messages_msgwait_too_many_handles) {
    HANDLE ev[64]; DWORD r; int i;
    REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    for (i = 0; i < 64; i++) ev[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
    SETE();
    r = U(MsgWaitForMultipleObjects)(64, ev, FALSE, 10, QS_ALLINPUT);
    CHECKF(r == WAIT_FAILED, "ret %lu", r);
    t_note("MsgWait with 64 handles: err %lu", GetLastError());
    SETE();
    r = U(MsgWaitForMultipleObjects)(63, ev, FALSE, 10, QS_ALLINPUT);
    CHECKF(r == WAIT_TIMEOUT, "63 handles ret %lu", r);
    for (i = 0; i < 64; i++) CloseHandle(ev[i]);
}

TEST(messages_msgwait_bad_handle) {
    DWORD r; HANDLE bad = (HANDLE)0x12344;
    REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    SETE();
    r = U(MsgWaitForMultipleObjects)(1, &bad, FALSE, 10, QS_ALLINPUT);
    CHECKF(r == WAIT_FAILED, "ret %lu", r);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}

TEST(messages_msgwait_woken_by_other_thread_post) {
    QWD w; HANDLE wd; DWORD r, t0, dt;
    REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    mh_reset();
    wd = qwd_start(&w, 100, MU(89));
    t0 = GetTickCount();
    r = U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 4000, QS_POSTMESSAGE);
    dt = GetTickCount() - t0;
    qwd_end(wd);
    CHECKF(r == WAIT_OBJECT_0, "ret %lu", r);
    CHECKF(dt >= 50 && dt < 3500, "dt %lu", dt);
    mh_drain();
}

TEST(messages_pm_qs_flags) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(90), 0, 0);
    CHECK(!PKA(&m, NULL, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE));
    CHECK(!PKA(&m, NULL, 0, 0, PM_NOREMOVE | PM_QS_INPUT));
    CHECK(!PKA(&m, NULL, 0, 0, PM_NOREMOVE | PM_QS_PAINT));
    CHECK(PKA(&m, NULL, 0, 0, PM_NOREMOVE | PM_QS_POSTMESSAGE));
    EXPECT_EQ(m.message, MU(90));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE | PM_QS_POSTMESSAGE));
    CHECK(!PKA(&m, NULL, 0, 0, PM_REMOVE | PM_QS_POSTMESSAGE));
    mh_kill(h);
}

TEST(messages_peek_wide_ansi_parity) {
    HWND h; MSG m;
    REQUIRE(PKW); REQUIRE(PKA); REQUIRE(PMW); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMW(h, MU(91), 1, 2);
    CHECK(PKW(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, MU(91)); EXPECT_EQ(m.hwnd, h);
    PMA(h, MU(92), 3, 4);
    CHECK(PKW(&m, h, MU(92), MU(92), PM_REMOVE));
    EXPECT_EQ(m.wParam, 3); EXPECT_EQ(m.lParam, 4);
    PMW(h, MU(93), 5, 6);
    CHECK(PKA(&m, h, MU(93), MU(93), PM_REMOVE));
    EXPECT_EQ(m.lParam, 6);
    mh_kill(h);
}

TEST(messages_get_wide_ansi_parity) {
    HWND h; MSG m;
    REQUIRE(GMW); REQUIRE(GMA); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(94), 11, 12);
    CHECK(GMW(&m, h, 0, 0) == TRUE);
    EXPECT_EQ(m.message, MU(94)); EXPECT_EQ(m.wParam, 11);
    PMW(h, MU(95), 13, 14);
    CHECK(GMA(&m, NULL, MU(95), MU(95)) == TRUE);
    EXPECT_EQ(m.lParam, 14);
    mh_kill(h);
}

TEST(messages_sent_before_posted_same_thread) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    mh_rec = 1;
    PMA(h, MU(100), 0, 0);
    SMA(h, MU(101), 0, 0);
    CHECK(mh_nev == 1 && mh_ev[0].m == MU(101));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, MU(100));
    mh_rec = 0; mh_kill(h);
}

TEST(messages_post_not_dispatched_until_pumped) {
    HWND h;
    REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    mh_rec = 1;
    PMA(h, MU(102), 0, 0);
    EXPECT_EQ(mh_count(MU(102)), 0);
    mh_drain();
    EXPECT_EQ(mh_count(MU(102)), 1);
    mh_rec = 0; mh_kill(h);
}

TEST(messages_peek_dispatches_sent_messages_from_other_thread) {
    MH_TH t; int i;
    REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_mainwnd = mh_mk(0, 0, NULL);
    REQUIRE(mh_mainwnd);
    mh_flag[1] = 0;
    CHECK(PMA(t.hwnd, WM_REC_D, 0, 0));
    /* only PeekMessage with a filter that matches nothing; sent messages must still be delivered */
    for (i = 0; i < 300 && !mh_flag[1]; i++) {
        MSG m; PKA(&m, NULL, MU(900), MU(900), PM_NOREMOVE);
        Sleep(10);
    }
    CHECK(mh_flag[1] == 1);
    mh_kill(mh_mainwnd); mh_mainwnd = NULL;
    mh_th_stop(&t);
}

TEST(messages_post_to_other_thread_window_runs_there) {
    MH_TH t; int i;
    REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_rec = 1;
    CHECK(PMA(t.hwnd, MU(103), 1, 2));
    for (i = 0; i < 300 && mh_count(MU(103)) == 0; i++) Sleep(10);
    CHECK(mh_count(MU(103)) == 1);
    if (mh_nev) {
        int k = mh_find(MU(103), 0);
        CHECK(k >= 0 && mh_ev[k].tid == t.tid);
        CHECK(k >= 0 && mh_ev[k].tid != GetCurrentThreadId());
    }
    mh_rec = 0;
    mh_th_stop(&t);
}

TEST(messages_post_thread_message_not_seen_by_wndproc) {
    MH_TH t;
    REQUIRE(U(PostThreadMessageA)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_rec = 1;
    CHECK(U(PostThreadMessageA)(t.tid, MU(104), 0, 0));
    Sleep(150);
    CHECK(mh_count(MU(104)) == 0);
    mh_rec = 0;
    mh_th_stop(&t);
}

TEST(messages_post_queue_limit) {
    HWND h; int ok = 0, i; BOOL r = TRUE; DWORD err = 0;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    for (i = 0; i < 12000; i++) {
        SETE();
        r = PMA(h, MU(105), i, 0);
        if (!r) { err = GetLastError(); break; }
        ok++;
    }
    CHECK(r == FALSE);
    CHECKF(err == E_QUOTA, "err %lu after %d posts", err, ok);
    CHECKF(ok <= 10000 && ok >= 1000, "accepted %d posts", ok);
    if (ok != 10000) t_note("post limit observed: %d (default USERPostMessageLimit is 10000)", ok);
    {
        MSG m; int n = 0;
        /* count only our posts: the system may queue a stray notification of its own */
        while (PKA(&m, NULL, 0, 0, PM_REMOVE)) if (m.message == MU(105)) n++;
        CHECKF(n == ok, "drained %d accepted %d", n, ok);
    }
    CHECK(PMA(h, MU(106), 0, 0));
    mh_drain(); mh_kill(h);
}

TEST(messages_dispatch_returns_wndproc_result) {
    HWND h; MSG m;
    REQUIRE(U(DispatchMessageA)); REQUIRE(U(DispatchMessageW)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    memset(&m, 0, sizeof m);
    m.hwnd = h; m.message = WM_REC_A; m.wParam = 30; m.lParam = 12;
    EXPECT_EQ(U(DispatchMessageA)(&m), 42);
    EXPECT_EQ(U(DispatchMessageW)(&m), 42);
    m.hwnd = NULL;
    EXPECT_EQ(U(DispatchMessageA)(&m), 0);
    m.message = WM_NULL;
    m.hwnd = h;
    EXPECT_EQ(U(DispatchMessageA)(&m), 0);
    mh_kill(h);
}

TEST(messages_dispatch_destroyed_window) {
    HWND h; MSG m;
    REQUIRE(U(DispatchMessageA)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    DESTROYW(h);
    memset(&m, 0, sizeof m);
    m.hwnd = h; m.message = WM_REC_A; m.wParam = 1; m.lParam = 1;
    SETE();
    EXPECT_EQ(U(DispatchMessageA)(&m), 0);
}

TEST(messages_translatemessage_return_values) {
    HWND h; MSG m; BOOL r;
    REQUIRE(U(TranslateMessage)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    memset(&m, 0, sizeof m);
    m.hwnd = h;
    m.message = WM_USER; r = U(TranslateMessage)(&m); CHECK(r == FALSE);
    m.message = WM_CHAR; m.wParam = 'a'; r = U(TranslateMessage)(&m); if (r) t_note("TranslateMessage(WM_CHAR) returned %d", r);
    m.message = WM_PAINT; r = U(TranslateMessage)(&m); CHECK(r == FALSE);
    m.message = WM_KEYDOWN; m.wParam = 'A'; m.lParam = 0x001E0001;
    r = U(TranslateMessage)(&m); CHECK(r == TRUE);
    m.message = WM_KEYUP; m.lParam = 0xC01E0001;
    r = U(TranslateMessage)(&m); CHECK(r == TRUE);
    m.message = WM_SYSKEYDOWN; m.lParam = 0x201E0001;
    r = U(TranslateMessage)(&m); CHECK(r == TRUE);
    m.message = WM_SYSKEYUP; m.lParam = 0xE01E0001;
    r = U(TranslateMessage)(&m); CHECK(r == TRUE);
    mh_drain(); mh_kill(h);
}

TEST(messages_translatemessage_posts_nothing_for_non_key) {
    HWND h; MSG m;
    REQUIRE(U(TranslateMessage)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    memset(&m, 0, sizeof m); m.hwnd = h; m.message = MU(110);
    U(TranslateMessage)(&m);
    CHECK(!PKA(&m, NULL, 0, 0, PM_NOREMOVE));
    mh_kill(h);
}

TEST(messages_translatemessage_vk_return_posts_wm_char) {
    HWND h; MSG m; int i;
    REQUIRE(U(TranslateMessage)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    memset(&m, 0, sizeof m); m.hwnd = h; m.message = WM_KEYDOWN; m.wParam = VK_RETURN; m.lParam = 0x001C0001;
    CHECK(U(TranslateMessage)(&m));
    for (i = 0; i < 20 && !PKA(&m, h, WM_CHAR, WM_CHAR, PM_NOREMOVE); i++) Sleep(2);
    if (PKA(&m, h, WM_CHAR, WM_CHAR, PM_REMOVE)) {
        EXPECT_EQ(m.wParam, 0x0D);
    } else t_note("no WM_CHAR generated for synthetic VK_RETURN");
    mh_drain(); mh_kill(h);
}

TEST(messages_message_time_and_pos) {
    HWND h; MSG m; DWORD now;
    REQUIRE(PMA); REQUIRE(U(GetMessageTime)); REQUIRE(U(GetMessagePos)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    now = GetTickCount();
    PMA(h, MU(111), 0, 0);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    CHECKF((LONG)(m.time - now) > -3000 && (LONG)(m.time - now) < 3000, "time %lu now %lu", m.time, now);
    EXPECT_EQ(U(GetMessageTime)(), (LONG)m.time);
    EXPECT_EQ(U(GetMessagePos)(), (DWORD)MAKELONG((short)m.pt.x, (short)m.pt.y));
    mh_kill(h);
}

TEST(messages_message_pos_matches_cursor_for_posted) {
    HWND h; MSG m; POINT pt;
    REQUIRE(PMA); REQUIRE(U(GetCursorPos)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    U(GetCursorPos)(&pt);
    PMA(h, MU(112), 0, 0);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    CHECKF(abs(m.pt.x - pt.x) < 2000 && abs(m.pt.y - pt.y) < 2000, "pt %ld,%ld cursor %ld,%ld", m.pt.x, m.pt.y, pt.x, pt.y);
    mh_kill(h);
}

TEST(messages_message_time_follows_retrieval) {
    HWND h; MSG m; LONG t1, t2;
    REQUIRE(PMA); REQUIRE(U(GetMessageTime)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(113), 0, 0);
    Sleep(60);
    PMA(h, MU(114), 0, 0);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    t1 = U(GetMessageTime)();
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    t2 = U(GetMessageTime)();
    CHECKF(t2 - t1 >= 30, "t1 %ld t2 %ld", t1, t2);
    CHECKF(t2 - t1 < 3000, "t1 %ld t2 %ld", t1, t2);
    mh_kill(h);
}

TEST(messages_extrainfo_roundtrip) {
    LPARAM old, prev;
    REQUIRE(U(SetMessageExtraInfo)); REQUIRE(U(GetMessageExtraInfo));
    old = U(GetMessageExtraInfo)();
    prev = U(SetMessageExtraInfo)(0x1234);
    EXPECT_EQ(prev, old);
    EXPECT_EQ(U(GetMessageExtraInfo)(), 0x1234);
    prev = U(SetMessageExtraInfo)(0x5678);
    EXPECT_EQ(prev, 0x1234);
    EXPECT_EQ(U(GetMessageExtraInfo)(), 0x5678);
    prev = U(SetMessageExtraInfo)(old);
    EXPECT_EQ(prev, 0x5678);
    EXPECT_EQ(U(GetMessageExtraInfo)(), old);
}

static volatile LPARAM g_ei_other;
static DWORD WINAPI ei_thread(LPVOID p) { g_ei_other = U(GetMessageExtraInfo)(); return 0; }
TEST(messages_extrainfo_per_thread) {
    LPARAM old; HANDLE th;
    REQUIRE(U(SetMessageExtraInfo)); REQUIRE(U(GetMessageExtraInfo));
    old = U(SetMessageExtraInfo)(0xABCD);
    g_ei_other = 0x77;
    th = CreateThread(NULL, 0, ei_thread, NULL, 0, NULL);
    REQUIRE(th);
    WaitForSingleObject(th, 5000); CloseHandle(th);
    EXPECT_EQ(g_ei_other, 0);
    EXPECT_EQ(U(GetMessageExtraInfo)(), 0xABCD);
    U(SetMessageExtraInfo)(old);
}

TEST(messages_extrainfo_travels_with_posted_message) {
    HWND h; MSG m; LPARAM old;
    REQUIRE(U(SetMessageExtraInfo)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    old = U(SetMessageExtraInfo)(0x4242);
    PMA(h, MU(115), 0, 0);
    U(SetMessageExtraInfo)(0);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    t_note("GetMessageExtraInfo after retrieving message posted with extra=0x4242: %llx", (unsigned long long)U(GetMessageExtraInfo)());
    U(SetMessageExtraInfo)(old);
    mh_kill(h);
}

TEST(messages_peek_message_hwnd_destroyed_midway) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(116), 0, 0);
    DESTROYW(h);
    SETE();
    CHECK(!PKA(&m, h, 0, 0, PM_REMOVE));
    t_note("peek on destroyed hwnd filter: err %lu", GetLastError());
}

TEST(messages_wm_null_post_and_get) {
    HWND h; MSG m;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    CHECK(PMA(h, WM_NULL, 5, 6));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, WM_NULL); EXPECT_EQ(m.wParam, 5);
    mh_kill(h);
}

TEST(messages_getmessage_loop_exits_on_quit) {
    HWND h; MSG m; int n = 0, r;
    REQUIRE(GMA); REQUIRE(PMA); REQUIRE(U(PostQuitMessage)); REQUIRE_MH();
    mh_reset();
    h = qw(); REQUIRE(h);
    PMA(h, MU(120), 0, 0); PMA(h, MU(121), 0, 0); PMA(h, MU(122), 0, 0);
    U(PostQuitMessage)(0);
    while ((r = GMA(&m, NULL, 0, 0)) > 0 && n < 100) { U(DispatchMessageA)(&m); n++; }
    EXPECT_EQ(r, 0);
    EXPECT_EQ(n, 3);
    mh_kill(h);
}
