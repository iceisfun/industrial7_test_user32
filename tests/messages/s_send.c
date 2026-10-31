/* SendMessage family: A/W, Timeout, Notify, Callback, ReplyMessage, InSendMessage, cross-thread. */
#include "mh.h"

#define MU(n) (WM_USER + (n))
#define NOERR 0xdeadbeef
#define SETE() SetLastError(NOERR)

static HWND sw(void) { return mh_mk(0, 0, NULL); }
static int wine_emul(void) { return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL; }

TEST(messages_send_same_thread_result) {
    HWND h;
    REQUIRE(SMA); REQUIRE(SMW); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    EXPECT_EQ(SMA(h, WM_REC_A, 3, 4), 7);
    EXPECT_EQ(SMW(h, WM_REC_A, 10, 20), 30);
    EXPECT_EQ(SMA(h, WM_REC_A, 0, 0), 0);
    mh_kill(h);
}

TEST(messages_send_64bit_params_and_result) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    EXPECT_EQ(SMA(h, WM_REC_A, (WPARAM)0x100000000ull, (LPARAM)0x23), 0x100000023ull);
    EXPECT_EQ(SMA(h, WM_USER + 9, 0, 0), 0x1122334455667788ull);
    EXPECT_EQ(SMW(h, WM_USER + 9, 0, 0), 0x1122334455667788ull);
    mh_kill(h);
}

TEST(messages_send_wm_null) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    SETE();
    EXPECT_EQ(SMA(h, WM_NULL, 0, 0), 0);
    EXPECT_EQ(SMW(h, WM_NULL, 123, 456), 0);
    mh_kill(h);
}

TEST(messages_send_invalid_hwnd) {
    HWND bad[] = { (HWND)0xDEADBEEF, (HWND)0x12345678, (HWND)0x3, (HWND)(ULONG_PTR)0x7FFFFFFFFFFFull };
    int i;
    REQUIRE(SMA); REQUIRE(SMW);
    for (i = 0; i < 4; i++) {
        LRESULT r;
        SETE(); r = SMA(bad[i], WM_NULL, 0, 0);
        CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
        SETE(); r = SMW(bad[i], WM_GETTEXTLENGTH, 0, 0);
        CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    }
}

TEST(messages_send_null_hwnd) {
    LRESULT r;
    REQUIRE(SMA);
    SETE(); r = SMA(NULL, WM_NULL, 0, 0);
    CHECK(r == 0);
    t_note("SendMessage(NULL) last error %lu", GetLastError());
}

TEST(messages_send_destroyed_window) {
    HWND h; LRESULT r;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    DESTROYW(h);
    SETE(); r = SMA(h, WM_REC_A, 1, 1);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    SETE(); r = SMW(h, WM_REC_A, 1, 1);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(messages_send_msg_high_bits_invalid) {
    HWND h; UINT bad[] = { 0x20000, 0x40000, 0x80000000u, 0xFFFFFFFFu, 0x7FFE0000 };
    int i;
    REQUIRE(SMA); REQUIRE(SMW); REQUIRE_MH();
    h = sw(); REQUIRE(h);
    for (i = 0; i < 5; i++) {
        LRESULT r;
        SETE(); r = SMA(h, bad[i], 1, 1);
        CHECKF(r == 0, "msg %x", bad[i]); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        SETE(); r = SMW(h, bad[i], 1, 1);
        CHECKF(r == 0, "W msg %x", bad[i]); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    mh_kill(h);
}

TEST(messages_send_msg_0x1ffff_passes) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    mh_rec = 0;
    SETE();
    EXPECT_EQ(SMA(h, 0x1FFFF, 1, 1), 0);
    CHECK(GetLastError() != ERROR_INVALID_PARAMETER);
    SETE();
    EXPECT_EQ(SMA(h, 0xC123, 1, 1), 0);
    mh_kill(h);
}

TEST(messages_sendtimeout_same_thread) {
    HWND h; DWORD_PTR res = 0xAAAA; LRESULT r;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE(U(SendMessageTimeoutW)); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    r = U(SendMessageTimeoutA)(h, WM_REC_A, 5, 6, SMTO_NORMAL, 1000, &res);
    CHECK(r != 0); EXPECT_EQ(res, 11);
    res = 0xAAAA;
    r = U(SendMessageTimeoutW)(h, WM_REC_A, 7, 8, SMTO_NORMAL, 1000, &res);
    CHECK(r != 0); EXPECT_EQ(res, 15);
    res = 0xAAAA;
    r = U(SendMessageTimeoutA)(h, WM_NULL, 0, 0, SMTO_NORMAL, 1000, &res);
    CHECK(r != 0); EXPECT_EQ(res, 0);
    mh_kill(h);
}

TEST(messages_sendtimeout_null_result_pointer) {
    HWND h; LRESULT r;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    r = U(SendMessageTimeoutA)(h, WM_REC_A, 1, 2, SMTO_NORMAL, 1000, NULL);
    CHECK(r != 0);
    mh_kill(h);
}

TEST(messages_sendtimeout_valid_flag_combinations) {
    HWND h; DWORD_PTR res; UINT fl[] = { 0, SMTO_BLOCK, SMTO_ABORTIFHUNG, SMTO_NOTIMEOUTIFNOTHUNG, SMTO_ERRORONEXIT,
        SMTO_BLOCK | SMTO_ABORTIFHUNG, SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, 0x4 };
    int i;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    for (i = 0; i < 8; i++) {
        LRESULT r; res = 0;
        r = U(SendMessageTimeoutA)(h, WM_REC_A, 20, 22, fl[i], 1000, &res);
        CHECKF(r != 0, "flags %x", fl[i]);
        CHECKF(res == 42, "flags %x res %llu", fl[i], (unsigned long long)res);
    }
    mh_kill(h);
}

TEST(messages_sendtimeout_invalid_flags) {
    HWND h; DWORD_PTR res = 0x1234; UINT bad[] = { 0x80, 0x100, 0x1000, 0xFFFF, 0x80000000u, 0xFFFFFF80u };
    int i;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    for (i = 0; i < 6; i++) {
        LRESULT r; res = 0x1234; SETE();
        r = U(SendMessageTimeoutA)(h, WM_REC_A, 1, 1, bad[i], 1000, &res);
        CHECKF(r == 0, "flags %x", bad[i]); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        CHECKF(res == 0x1234, "result untouched on bad flags (%llx)", (unsigned long long)res);
        SETE();
        r = U(SendMessageTimeoutW)(h, WM_REC_A, 1, 1, bad[i], 1000, &res);
        CHECKF(r == 0, "W flags %x", bad[i]); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    mh_kill(h);
}

TEST(messages_sendtimeout_invalid_message) {
    HWND h; DWORD_PTR res = 0x1234; LRESULT r;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    h = sw(); REQUIRE(h);
    SETE(); r = U(SendMessageTimeoutA)(h, 0x20000, 1, 1, SMTO_NORMAL, 1000, &res);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(res, 0x1234);
    mh_kill(h);
}

TEST(messages_sendtimeout_invalid_hwnd) {
    DWORD_PTR res = 0x1234; LRESULT r;
    REQUIRE(U(SendMessageTimeoutA));
    SETE(); r = U(SendMessageTimeoutA)((HWND)0xDEADBEEF, WM_NULL, 0, 0, SMTO_NORMAL, 1000, &res);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    EXPECT_EQ(res, 0);
}

TEST(messages_sendtimeout_cross_thread_success) {
    MH_TH t; DWORD_PTR res = 0; LRESULT r;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    r = U(SendMessageTimeoutA)(t.hwnd, WM_REC_A, 100, 23, SMTO_NORMAL, 3000, &res);
    CHECK(r != 0); EXPECT_EQ(res, 123);
    r = U(SendMessageTimeoutW)(t.hwnd, WM_REC_C, 0, 0, SMTO_ABORTIFHUNG, 3000, &res);
    CHECK(r != 0); EXPECT_EQ(res, t.tid);
    mh_th_stop(&t);
}

TEST(messages_sendtimeout_times_out_on_busy_thread) {
    MH_TH t; DWORD_PTR res = 0x55; LRESULT r; DWORD t0, dt;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    PMA(t.hwnd, WM_USER + 7, 700, 0);   /* worker sleeps 700 ms inside its window procedure */
    Sleep(100);
    t0 = GetTickCount();
    SETE();
    r = U(SendMessageTimeoutA)(t.hwnd, WM_REC_A, 1, 1, SMTO_NORMAL, 150, &res);
    dt = GetTickCount() - t0;
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_TIMEOUT);
    EXPECT_EQ(res, 0);
    CHECKF(dt >= 100 && dt < 650, "elapsed %lu", dt);
    mh_th_stop(&t);
}

TEST(messages_sendtimeout_abortifhung_busy_not_hung) {
    MH_TH t; DWORD_PTR res = 0; LRESULT r;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    PMA(t.hwnd, WM_USER + 7, 400, 0);
    Sleep(100);
    SETE();
    r = U(SendMessageTimeoutA)(t.hwnd, WM_REC_A, 1, 1, SMTO_ABORTIFHUNG, 100, &res);
    CHECK(r == 0);
    EXPECT_ERR(r, ERROR_TIMEOUT);
    mh_th_stop(&t);
}

TEST(messages_sendnotify_same_thread_is_synchronous) {
    HWND h;
    REQUIRE(U(SendNotifyMessageA)); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    mh_rec = 1;
    CHECK(U(SendNotifyMessageA)(h, MU(30), 1, 2) == TRUE);
    EXPECT_EQ(mh_count(MU(30)), 1);
    CHECK(U(SendNotifyMessageW)(h, MU(31), 1, 2) == TRUE);
    EXPECT_EQ(mh_count(MU(31)), 1);
    mh_rec = 0; mh_kill(h);
}

TEST(messages_sendnotify_invalid_hwnd) {
    BOOL r;
    REQUIRE(U(SendNotifyMessageA)); REQUIRE(U(SendNotifyMessageW));
    SETE(); r = U(SendNotifyMessageA)((HWND)0xDEADBEEF, WM_NULL, 0, 0);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    SETE(); r = U(SendNotifyMessageW)((HWND)0x12345678, MU(32), 0, 0);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(messages_sendnotify_cross_thread_does_not_wait) {
    MH_TH t; DWORD t0, dt; BOOL r;
    REQUIRE(U(SendNotifyMessageA)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    t0 = GetTickCount();
    r = U(SendNotifyMessageA)(t.hwnd, WM_USER + 7, 500, 0);
    dt = GetTickCount() - t0;
    CHECK(r == TRUE);
    CHECKF(dt < 300, "SendNotifyMessage waited %lu ms", dt);
    mh_th_stop(&t);
}

TEST(messages_sendnotify_cross_thread_flags) {
    MH_TH t; int i;
    REQUIRE(U(SendNotifyMessageA)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_ins_a = -1;
    CHECK(U(SendNotifyMessageA)(t.hwnd, WM_REC_F, 0, 0));
    for (i = 0; i < 300 && mh_ins_a == -1; i++) Sleep(10);
    CHECKF((mh_ins_a & ISMEX_NOTIFY) != 0, "ins %lx", (long)mh_ins_a);
    CHECKF((mh_ins_a & ISMEX_SEND) == 0, "ins %lx", (long)mh_ins_a);
    mh_th_stop(&t);
}

static volatile LONG cb_n; static HWND cb_h; static UINT cb_m; static ULONG_PTR cb_d; static LRESULT cb_r; static DWORD cb_tid;
static void CALLBACK cb_fn(HWND h, UINT m, ULONG_PTR d, LRESULT r) { cb_h = h; cb_m = m; cb_d = d; cb_r = r; cb_tid = GetCurrentThreadId(); InterlockedIncrement(&cb_n); }

TEST(messages_sendcallback_same_thread) {
    HWND h; BOOL r;
    REQUIRE(U(SendMessageCallbackA)); REQUIRE_MH();
    mh_reset(); cb_n = 0;
    h = sw(); REQUIRE(h);
    r = U(SendMessageCallbackA)(h, WM_REC_A, 40, 2, cb_fn, 0xBEEF);
    CHECK(r == TRUE);
    if (cb_n == 0) t_note("callback not invoked synchronously for same-thread target");
    mh_pump_until(&cb_n, 2000);
    EXPECT_EQ(cb_n, 1);
    EXPECT_EQ(cb_h, h); EXPECT_EQ(cb_m, WM_REC_A); EXPECT_EQ(cb_d, 0xBEEF); EXPECT_EQ(cb_r, 42);
    mh_kill(h);
}

TEST(messages_sendcallback_wide_same_thread) {
    HWND h; BOOL r;
    REQUIRE(U(SendMessageCallbackW)); REQUIRE_MH();
    mh_reset(); cb_n = 0;
    h = sw(); REQUIRE(h);
    r = U(SendMessageCallbackW)(h, WM_REC_A, 1, 2, cb_fn, 77);
    CHECK(r == TRUE);
    mh_pump_until(&cb_n, 2000);
    EXPECT_EQ(cb_n, 1); EXPECT_EQ(cb_d, 77); EXPECT_EQ(cb_r, 3);
    mh_kill(h);
}

TEST(messages_sendcallback_cross_thread_delivered_on_pump) {
    MH_TH t; BOOL r;
    REQUIRE(U(SendMessageCallbackA)); REQUIRE_MH();
    mh_reset(); cb_n = 0;
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    r = U(SendMessageCallbackA)(t.hwnd, WM_REC_A, 5, 6, cb_fn, 0x1111);
    CHECK(r == TRUE);
    mh_pump_until(&cb_n, 3000);
    EXPECT_EQ(cb_n, 1);
    EXPECT_EQ(cb_tid, GetCurrentThreadId());
    EXPECT_EQ(cb_r, 11); EXPECT_EQ(cb_d, 0x1111); EXPECT_EQ(cb_h, t.hwnd);
    mh_th_stop(&t);
}

TEST(messages_sendcallback_cross_thread_ins_callback_flag) {
    MH_TH t; int i;
    REQUIRE(U(SendMessageCallbackA)); REQUIRE_MH();
    mh_reset(); cb_n = 0;
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_ins_a = -1;
    CHECK(U(SendMessageCallbackA)(t.hwnd, WM_REC_F, 0, 0, cb_fn, 1));
    for (i = 0; i < 300 && mh_ins_a == -1; i++) Sleep(10);
    CHECKF((mh_ins_a & ISMEX_CALLBACK) != 0, "ins %lx", (long)mh_ins_a);
    mh_pump_until(&cb_n, 3000);
    mh_th_stop(&t);
}

TEST(messages_sendcallback_invalid_hwnd) {
    BOOL r;
    REQUIRE(U(SendMessageCallbackA));
    SETE(); r = U(SendMessageCallbackA)((HWND)0xDEADBEEF, WM_NULL, 0, 0, cb_fn, 0);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(messages_insendmessage_false_outside) {
    HWND h;
    REQUIRE(U(InSendMessage)); REQUIRE(U(InSendMessageEx)); REQUIRE_MH();
    mh_reset();
    CHECK(U(InSendMessage)() == FALSE);
    EXPECT_EQ(U(InSendMessageEx)(NULL), ISMEX_NOSEND);
    h = sw(); REQUIRE(h);
    mh_ins_a = -1;
    SMA(h, WM_REC_F, 0, 0);
    EXPECT_EQ(mh_ins_a, ISMEX_NOSEND);   /* same-thread send is not "in send" */
    EXPECT_EQ(SMA(h, WM_REC_F, 0, 0), 0);
    mh_kill(h);
}

TEST(messages_insendmessage_cross_thread) {
    MH_TH t; DWORD_PTR r = 0;
    REQUIRE(U(InSendMessageEx)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_ins_a = -1;
    CHECK(mh_xsend(t.hwnd, WM_REC_F, 0, 0, &r));
    EXPECT_EQ(mh_ins_a, ISMEX_SEND);
    EXPECT_EQ(r, 1);                    /* InSendMessage() true inside */
    mh_th_stop(&t);
}

TEST(messages_insendmessage_ex_with_timeout_send) {
    MH_TH t; DWORD_PTR r = 0;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_ins_a = -1;
    CHECK(U(SendMessageTimeoutA)(t.hwnd, WM_REC_F, 0, 0, SMTO_BLOCK, 3000, &r));
    EXPECT_EQ(mh_ins_a, ISMEX_SEND);
    mh_th_stop(&t);
}

TEST(messages_reply_message_outside_send) {
    BOOL r;
    REQUIRE(U(ReplyMessage));
    SETE(); r = U(ReplyMessage)(5);
    CHECK(r == FALSE);
}

TEST(messages_reply_message_same_thread_send) {
    HWND h; BOOL r;
    REQUIRE(U(ReplyMessage)); REQUIRE_MH();
    r = U(ReplyMessage)(1);
    CHECK(r == FALSE);
    h = sw(); REQUIRE(h);
    mh_flag[2] = 0;
    /* same-thread send: handler's ReplyMessage must fail, final result is the handler's return */
    EXPECT_EQ(SMA(h, WM_REC_B, 0, 0), 0x66);
    EXPECT_EQ(mh_flag[2], 2);
    mh_kill(h);
}

TEST(messages_reply_message_cross_thread) {
    MH_TH t; DWORD_PTR r = 0;
    REQUIRE(U(ReplyMessage)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_flag[0] = 0; mh_flag[2] = 0; mh_ins_a = mh_ins_b = -1;
    CHECK(mh_xsend(t.hwnd, WM_REC_B, 0, 0, &r));
    EXPECT_EQ(r, 0x55);                 /* sender sees the replied value, not the return */
    EXPECT_EQ(mh_ins_a, ISMEX_SEND);
    EXPECT_EQ(mh_ins_b, (ISMEX_SEND | ISMEX_REPLIED));
    for (int i = 0; i < 200 && !mh_flag[0]; i++) Sleep(10);
    CHECK(mh_flag[0] == 1);
    if (mh_flag[2] != 2) t_note("second ReplyMessage returned TRUE");
    mh_th_stop(&t);
}

TEST(messages_reply_message_sender_released_early) {
    MH_TH t; DWORD_PTR r = 0; DWORD t0, dt;
    REQUIRE(U(ReplyMessage)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_flag[0] = 0;
    t0 = GetTickCount();
    mh_xsend(t.hwnd, WM_REC_B, 0, 0, &r);
    dt = GetTickCount() - t0;
    CHECKF(dt < 2500, "send took %lu", dt);
    for (int i = 0; i < 200 && !mh_flag[0]; i++) Sleep(10);
    mh_th_stop(&t);
}

TEST(messages_cross_thread_handler_runs_on_owner_thread) {
    MH_TH t; DWORD_PTR r = 0;
    REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    CHECK(mh_xsend(t.hwnd, WM_REC_C, 0, 0, &r));
    EXPECT_EQ(r, t.tid);
    CHECK(r != GetCurrentThreadId());
    CHECK(U(GetWindowThreadProcessId)(t.hwnd, NULL) == t.tid);
    mh_th_stop(&t);
}

TEST(messages_cross_thread_bounce_back) {
    MH_TH t; DWORD_PTR r = 0;
    REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_mainwnd = sw(); REQUIRE(mh_mainwnd);
    mh_flag[1] = 0;
    CHECK(mh_xsend(t.hwnd, WM_REC_D, 0, 0, &r));
    EXPECT_EQ(mh_flag[1], 1);
    EXPECT_EQ(r, 709);                  /* WM_REC_E: 7*100 + 9 returned through the nested send */
    mh_kill(mh_mainwnd); mh_mainwnd = NULL;
    mh_th_stop(&t);
}

TEST(messages_cross_thread_destroyed_window) {
    MH_TH t; HWND h; LRESULT r;
    REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    h = t.hwnd;
    mh_th_stop(&t);
    CHECK(!ISWND(h));
    SETE(); r = SMA(h, WM_REC_A, 1, 1);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(messages_cross_thread_gettext_marshals_buffer) {
    MH_TH t; char buf[64]; LRESULT r; WCHAR wb[64];
    REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    memset(buf, 'x', sizeof buf);
    r = SMA(t.hwnd, WM_GETTEXT, sizeof buf, (LPARAM)buf);
    EXPECT_EQ(r, 2);
    EXPECT_STR(buf, "mh");
    r = SMW(t.hwnd, WM_GETTEXT, 64, (LPARAM)wb);
    EXPECT_EQ(r, 2);
    EXPECT_WSTR(wb, L"mh");
    EXPECT_EQ(SMA(t.hwnd, WM_GETTEXTLENGTH, 0, 0), 2);
    mh_th_stop(&t);
}

TEST(messages_cross_thread_settext_marshals_string) {
    MH_TH t; char buf[64];
    REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    EXPECT_EQ(SMA(t.hwnd, WM_SETTEXT, 0, (LPARAM)"cross-thread"), TRUE);
    memset(buf, 0, sizeof buf);
    SMA(t.hwnd, WM_GETTEXT, sizeof buf, (LPARAM)buf);
    EXPECT_STR(buf, "cross-thread");
    mh_th_stop(&t);
}

TEST(messages_send_recursion_same_thread) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    EXPECT_EQ(SMA(h, WM_USER + 8, 0, 0), 0);
    EXPECT_EQ(SMA(h, WM_USER + 8, 1, 0), 1);
    EXPECT_EQ(SMA(h, WM_USER + 8, 50, 0), 50);
    EXPECT_EQ(SMW(h, WM_USER + 8, 60, 0), 60);
    mh_kill(h);
}

TEST(messages_send_recursion_cross_thread) {
    MH_TH t; DWORD_PTR r = 0;
    REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    CHECK(mh_xsend(t.hwnd, WM_USER + 8, 30, 0, &r));
    EXPECT_EQ(r, 30);
    mh_th_stop(&t);
}

static LRESULT h_destroy_self(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == MU(40)) { DESTROYW(h); *hd = 1; return 5; }
    return 0;
}
TEST(messages_send_window_destroyed_inside_handler) {
    HWND h; LRESULT r;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    mh_hook = h_destroy_self;
    r = SMA(h, MU(40), 0, 0);
    EXPECT_EQ(r, 5);
    CHECK(!ISWND(h));
    mh_hook = NULL;
}

TEST(messages_send_ansi_to_wide_window_settext) {
    HWND h; const MH_EV *e; int k;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mkw(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    EXPECT_EQ(SMA(h, WM_SETTEXT, 0, (LPARAM)"abc"), TRUE);
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0);
    if (k >= 0) {
        e = &mh_ev[k];
        CHECK(!memcmp(e->raw, L"abc", 8));   /* wide window proc sees UTF-16 */
    }
    mh_rec = 0; mh_kill(h);
}

TEST(messages_send_wide_to_ansi_window_settext) {
    HWND h; int k;
    REQUIRE(SMW); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    EXPECT_EQ(SMW(h, WM_SETTEXT, 0, (LPARAM)L"xyz"), TRUE);
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0);
    if (k >= 0) CHECK(!memcmp(mh_ev[k].raw, "xyz", 4));
    mh_rec = 0; mh_kill(h);
}

TEST(messages_send_a_w_roundtrip_text) {
    HWND a, w; char ab[32]; WCHAR wb[32];
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    a = mh_mk(0, 0, NULL); w = mh_mkw(0, 0, NULL); REQUIRE(a && w);
    SMA(a, WM_SETTEXT, 0, (LPARAM)"plain");
    SMA(w, WM_SETTEXT, 0, (LPARAM)"plain");
    memset(ab, 0, sizeof ab); memset(wb, 0, sizeof wb);
    EXPECT_EQ(SMW(a, WM_GETTEXT, 32, (LPARAM)wb), 5);
    EXPECT_WSTR(wb, L"plain");
    EXPECT_EQ(SMA(w, WM_GETTEXT, 32, (LPARAM)ab), 5);
    EXPECT_STR(ab, "plain");
    EXPECT_EQ(SMW(a, WM_GETTEXTLENGTH, 0, 0), 5);
    EXPECT_EQ(SMA(w, WM_GETTEXTLENGTH, 0, 0), 5);
    mh_kill(a); mh_kill(w);
}

TEST(messages_send_wm_char_a_to_w_ascii) {
    HWND a, w; int k;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    a = mh_mk(0, 0, NULL); w = mh_mkw(0, 0, NULL); REQUIRE(a && w);
    mh_rec = 1;
    SMA(w, WM_CHAR, 'Q', 0);
    SMW(a, WM_CHAR, 'Q', 0);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_CHAR), 2);
    k = mh_find(WM_CHAR, 0);
    CHECK(k >= 0 && mh_ev[k].w == 'Q');
    k = mh_find(WM_CHAR, k + 1);
    CHECK(k >= 0 && mh_ev[k].w == 'Q');
    mh_kill(a); mh_kill(w);
}

TEST(messages_isw_unicode_flags_follow_class) {
    HWND a, w;
    REQUIRE(U(IsWindowUnicode)); REQUIRE_MH();
    a = mh_mk(0, 0, NULL); w = mh_mkw(0, 0, NULL); REQUIRE(a && w);
    CHECK(U(IsWindowUnicode)(a) == FALSE);
    CHECK(U(IsWindowUnicode)(w) == TRUE);
    SETE();
    CHECK(U(IsWindowUnicode)((HWND)0xDEADBEEF) == FALSE);
    mh_kill(a); mh_kill(w);
}

TEST(messages_sendmessage_gettext_buffer_edges_a) {
    HWND h; char b[16]; LRESULT r;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    SMA(h, WM_SETTEXT, 0, (LPARAM)"hello");
    memset(b, 'z', sizeof b);
    r = SMA(h, WM_GETTEXT, 0, (LPARAM)b);
    EXPECT_EQ(r, 0); EXPECT_EQ(b[0], 'z');
    memset(b, 'z', sizeof b);
    r = SMA(h, WM_GETTEXT, 1, (LPARAM)b);
    EXPECT_EQ(r, 0); EXPECT_EQ(b[0], 0);
    memset(b, 'z', sizeof b);
    r = SMA(h, WM_GETTEXT, 5, (LPARAM)b);
    EXPECT_EQ(r, 4); EXPECT_STR(b, "hell");
    memset(b, 'z', sizeof b);
    r = SMA(h, WM_GETTEXT, 6, (LPARAM)b);
    EXPECT_EQ(r, 5); EXPECT_STR(b, "hello");
    memset(b, 'z', sizeof b);
    r = SMA(h, WM_GETTEXT, 16, (LPARAM)b);
    EXPECT_EQ(r, 5); EXPECT_STR(b, "hello");
    EXPECT_EQ(b[6], 'z');
    mh_kill(h);
}

TEST(messages_sendmessage_gettext_buffer_edges_w) {
    HWND h; WCHAR b[16]; LRESULT r; int i;
    REQUIRE(SMW); REQUIRE_MH();
    mh_reset();
    h = mh_mkw(0, 0, NULL); REQUIRE(h);
    SMW(h, WM_SETTEXT, 0, (LPARAM)L"hello");
    for (i = 0; i < 16; i++) b[i] = 0x7A7A;
    r = SMW(h, WM_GETTEXT, 0, (LPARAM)b);
    EXPECT_EQ(r, 0); EXPECT_EQ(b[0], 0x7A7A);
    r = SMW(h, WM_GETTEXT, 1, (LPARAM)b);
    EXPECT_EQ(r, 0); EXPECT_EQ(b[0], 0);
    for (i = 0; i < 16; i++) b[i] = 0x7A7A;
    r = SMW(h, WM_GETTEXT, 5, (LPARAM)b);
    EXPECT_EQ(r, 4); EXPECT_WSTR(b, L"hell");
    r = SMW(h, WM_GETTEXT, 6, (LPARAM)b);
    EXPECT_EQ(r, 5); EXPECT_WSTR(b, L"hello");
    mh_kill(h);
}

TEST(messages_send_to_message_only_window) {
    HWND h; 
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, HWND_MESSAGE);
    REQUIRE(h);
    EXPECT_EQ(SMA(h, WM_REC_A, 1, 2), 3);
    CHECK(PMA(h, MU(50), 0, 0));
    mh_rec = 1; mh_drain();
    EXPECT_EQ(mh_count(MU(50)), 1);
    mh_rec = 0; mh_kill(h);
}

TEST(messages_sendmessage_timeout_zero_timeout_same_thread) {
    HWND h; DWORD_PTR res = 0; LRESULT r;
    REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    h = sw(); REQUIRE(h);
    r = U(SendMessageTimeoutA)(h, WM_REC_A, 1, 2, SMTO_NORMAL, 0, &res);
    CHECK(r != 0); EXPECT_EQ(res, 3);
    mh_kill(h);
}

TEST(messages_sendmessage_broadcast_registered_message) {
    UINT mid; HWND t1; DWORD_PTR res = 0; LRESULT r; int got;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    mid = U(RegisterWindowMessageA)("t32_messages_broadcast_send");
    CHECK(mid != 0);
    t1 = mh_mk(0, 0, NULL); REQUIRE(t1);
    mh_rec = 1;
    r = U(SendMessageTimeoutA)(HWND_BROADCAST, mid, 0, 0, SMTO_ABORTIFHUNG, 300, &res);
    got = 0;
    { int k; for (k = 0; k < mh_nev; k++) if (mh_ev[k].m == mid && mh_ev[k].h == t1) got++; }
    mh_rec = 0;
    t_note("broadcast ret %lld", (long long)r);
    CHECK(got == 1);
    mh_kill(t1);
}

TEST(messages_sendmessage_broadcast_skips_message_only_and_children) {
    UINT mid; HWND top, mo, child; DWORD_PTR res = 0; int k, gt = 0, gm = 0, gc = 0;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE(U(SendMessageTimeoutA)); REQUIRE_MH();
    mh_reset();
    mid = U(RegisterWindowMessageA)("t32_messages_broadcast_scope");
    top = mh_mk(0, 0, NULL); REQUIRE(top);
    child = mh_mk(WS_CHILD, 0, top);
    mo = mh_mk(0, 0, HWND_MESSAGE);
    REQUIRE(child && mo);
    mh_rec = 1;
    U(SendMessageTimeoutA)(HWND_BROADCAST, mid, 0, 0, SMTO_ABORTIFHUNG, 300, &res);
    mh_rec = 0;
    for (k = 0; k < mh_nev; k++) if (mh_ev[k].m == mid) {
        if (mh_ev[k].h == top) gt++; else if (mh_ev[k].h == child) gc++; else if (mh_ev[k].h == mo) gm++;
    }
    CHECK(gt == 1);
    CHECK(gc == 0);
    if (gm) t_note("broadcast reached message-only window");
    mh_kill(top); mh_kill(mo);
}
