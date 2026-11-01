/* Extra message tests: hooks on the message path, ordering, thread messages, creation failures, paint status. */
#include "mh.h"

#define MU(n) (WM_USER + (n))
#define NOERR 0xdeadbeef
#define SETE() SetLastError(NOERR)

/* ---- GetMessage filter acceptance (analysed validation: high bits only matter unless max == 0xFFFFFFFF) ---- */
TEST(messages_x_getmessage_max_all_ones_accepted) {
    HWND h; MSG m; BOOL r;
    REQUIRE(GMA); REQUIRE(GMW); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    PMA(h, MU(200), 1, 2);
    SETE(); r = GMW(&m, NULL, 0, 0xFFFFFFFFu);
    CHECK(r == TRUE);
    EXPECT_EQ(m.message, MU(200));
    PMA(h, MU(201), 3, 4);
    r = GMA(&m, NULL, 0, 0xFFFFFFFFu);
    CHECK(r == TRUE);
    EXPECT_EQ(m.message, MU(201));
    PMA(h, MU(202), 5, 6);
    r = GMA(&m, h, MU(202), MU(202));
    CHECK(r == TRUE);
    EXPECT_EQ(m.wParam, 5);
    mh_kill(h);
}

TEST(messages_x_getmessage_wm_char_wparam_masked_w) {
    HWND h; MSG m; BOOL r;
    REQUIRE(GMW); REQUIRE(PMW); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    CHECK(PMW(h, WM_CHAR, 0x12345678, 0));
    r = GMW(&m, h, WM_CHAR, WM_CHAR);
    CHECK(r == TRUE);
    EXPECT_EQ(m.message, WM_CHAR);
    EXPECT_EQ(m.wParam, 0x5678);       /* GetMessageW keeps only the low 16 bits of a WM_CHAR wParam */
    mh_kill(h);
}

/* ---- sent versus posted ordering ---- */
typedef struct { HWND target; int mode; HANDLE done; } XORD;
static DWORD WINAPI xord_thread(LPVOID p) {
    XORD *o = (XORD *)p;
    DWORD_PTR r = 0;
    U(PostMessageA)(o->target, MU(210), 0, 0);
    U(SendMessageTimeoutA)(o->target, MU(211), 0, 0, SMTO_NORMAL, 4000, &r);
    SetEvent(o->done);
    return 0;
}
TEST(messages_x_sent_delivered_before_posted_from_other_thread) {
    HWND h; XORD o; HANDLE th; MSG m; int k211, k210, i, both = 0, seen_at_peek;
    REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    o.target = h; o.mode = 0; o.done = CreateEventA(NULL, TRUE, FALSE, NULL);
    mh_rec = 1;
    th = CreateThread(NULL, 0, xord_thread, &o, 0, NULL); REQUIRE(th);
    /* wait (without retrieving) until the post is in the queue and the send is pending */
    for (i = 0; i < 300; i++) {
        DWORD s = U(GetQueueStatus)(QS_SENDMESSAGE | QS_POSTMESSAGE);
        if ((s >> 16 & QS_SENDMESSAGE) && (s >> 16 & QS_POSTMESSAGE)) { both = 1; break; }
        Sleep(10);
    }
    /* one peek: the sent message is processed inside it, the posted one is returned */
    CHECK(PKA(&m, h, MU(210), MU(210), PM_REMOVE));
    EXPECT_EQ(m.message, MU(210));
    seen_at_peek = mh_find(MU(211), 0) >= 0;
    if (both) CHECK(seen_at_peek);       /* the pending send was processed inside the peek */
    WaitForSingleObject(o.done, 5000);
    mh_rec = 0;
    k211 = mh_find(MU(211), 0);
    CHECK(k211 >= 0);
    k210 = mh_find(MU(210), 0);
    CHECK(k210 < 0);                      /* posted message is not dispatched by PeekMessage itself */
    WaitForSingleObject(th, 5000); CloseHandle(th); CloseHandle(o.done);
    mh_kill(h);
}

typedef struct { HWND target; int n; } XFIFO;
static DWORD WINAPI xfifo_thread(LPVOID p) {
    XFIFO *f = (XFIFO *)p; int i;
    for (i = 0; i < f->n; i++) U(PostMessageA)(f->target, MU(220), i, 0);
    return 0;
}
TEST(messages_x_cross_thread_post_fifo) {
    HWND h; XFIFO f; HANDLE th; MSG m; int i, got = 0, inorder = 1; DWORD t0;
    REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    f.target = h; f.n = 200;
    th = CreateThread(NULL, 0, xfifo_thread, &f, 0, NULL); REQUIRE(th);
    t0 = GetTickCount();
    while (got < 200 && GetTickCount() - t0 < 5000) {
        if (PKA(&m, h, MU(220), MU(220), PM_REMOVE)) { if ((int)m.wParam != got) inorder = 0; got++; }
        else Sleep(1);
    }
    WaitForSingleObject(th, 5000); CloseHandle(th);
    EXPECT_EQ(got, 200);
    CHECK(inorder);
    (void)i;
    mh_kill(h);
}

/* ---- thread messages to a worker thread with a message loop ---- */
typedef struct { HANDLE ready; DWORD tid; MSG first; int firstok; int quit_ret; WPARAM quit_w; volatile LONG nmsgs; } XTHR;
static DWORD WINAPI xthr_proc(LPVOID p) {
    XTHR *t = (XTHR *)p; MSG m; BOOL r; DWORD t0 = GetTickCount();
    PKA(&m, NULL, 0, 0, PM_NOREMOVE);
    t->tid = GetCurrentThreadId();
    SetEvent(t->ready);
    for (;;) {
        if (GetTickCount() - t0 > 8000) break;
        if (U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 200, QS_ALLINPUT) == WAIT_TIMEOUT) continue;
        while (PKA(&m, NULL, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) { t->quit_ret = 0; t->quit_w = m.wParam; return 0; }
            if (!t->firstok) { t->first = m; t->firstok = 1; }
            InterlockedIncrement(&t->nmsgs);
        }
    }
    (void)r;
    t->quit_ret = -1;
    return 0;
}
TEST(messages_x_postthreadmessage_to_worker_and_quit) {
    XTHR t; HANDLE th; int i;
    REQUIRE(U(PostThreadMessageA)); REQUIRE(PKA); REQUIRE_MH();
    memset(&t, 0, sizeof t);
    t.ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, xthr_proc, &t, 0, NULL); REQUIRE(th);
    CHECK(WaitForSingleObject(t.ready, 5000) == WAIT_OBJECT_0);
    CHECK(U(PostThreadMessageA)(t.tid, MU(230), 0x1111, 0x2222) == TRUE);
    for (i = 0; i < 200 && !t.nmsgs; i++) Sleep(10);
    CHECK(t.firstok);
    EXPECT_EQ(t.first.hwnd, NULL);
    EXPECT_EQ(t.first.message, MU(230));
    EXPECT_EQ(t.first.wParam, 0x1111);
    EXPECT_EQ(t.first.lParam, 0x2222);
    CHECK(U(PostThreadMessageW)(t.tid, WM_QUIT, 77, 0) == TRUE);
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    EXPECT_EQ(t.quit_ret, 0);
    EXPECT_EQ(t.quit_w, 77);
    CloseHandle(th); CloseHandle(t.ready);
}

/* ---- window procedure observation through hooks ---- */
static volatile LONG gm_n, gm_remove_seen, gm_noremove_seen; static HHOOK gm_hook; static volatile UINT gm_modify;
static LRESULT CALLBACK gm_proc(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION) {
        MSG *m = (MSG *)l;
        if (m->message == MU(240)) {
            InterlockedIncrement(&gm_n);
            if (w == PM_REMOVE) InterlockedIncrement(&gm_remove_seen);
            if (w == PM_NOREMOVE) InterlockedIncrement(&gm_noremove_seen);
            if (gm_modify) m->wParam = gm_modify;
        }
    }
    return U(CallNextHookEx)(NULL, code, w, l);
}
TEST(messages_x_getmsg_hook_sees_peek_flags) {
    HWND h; MSG m;
    REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(U(CallNextHookEx)); REQUIRE_MH();
    mh_reset(); gm_n = gm_remove_seen = gm_noremove_seen = 0; gm_modify = 0;
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    gm_hook = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    if (!gm_hook) { t_note("cannot install WH_GETMESSAGE hook (err %lu)", GetLastError()); mh_kill(h); return; }
    PMA(h, MU(240), 5, 6);
    CHECK(PKA(&m, h, MU(240), MU(240), PM_NOREMOVE));
    EXPECT_EQ(gm_noremove_seen, 1);
    EXPECT_EQ(gm_remove_seen, 0);
    CHECK(PKA(&m, h, MU(240), MU(240), PM_REMOVE));
    EXPECT_EQ(gm_remove_seen, 1);
    EXPECT_EQ(m.wParam, 5);
    PMA(h, MU(240), 7, 8);
    gm_modify = 99;
    CHECK(PKA(&m, h, MU(240), MU(240), PM_REMOVE));
    if (m.wParam != 99) t_note("WH_GETMESSAGE modification of wParam not visible to PeekMessage caller (%llu)", (unsigned long long)m.wParam);
    gm_modify = 0;
    CHECK(U(UnhookWindowsHookEx)(gm_hook));
    PMA(h, MU(240), 9, 10);
    {
        LONG before = gm_n;
        CHECK(PKA(&m, h, MU(240), MU(240), PM_REMOVE));
        EXPECT_EQ(gm_n, before);          /* hook is gone */
    }
    mh_kill(h);
}

static volatile LONG cw_n, cw_sent_flag; static volatile UINT cw_msg; static HHOOK cw_hook;
static LRESULT CALLBACK cw_proc(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION) {
        CWPSTRUCT *c = (CWPSTRUCT *)l;
        if (c->message == MU(241)) { InterlockedIncrement(&cw_n); cw_sent_flag = (LONG)(w != 0); }
    }
    return U(CallNextHookEx)(NULL, code, w, l);
}
static volatile LONG cwr_n; static volatile LRESULT cwr_res; static HHOOK cwr_hook;
static LRESULT CALLBACK cwr_proc(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION) {
        CWPRETSTRUCT *c = (CWPRETSTRUCT *)l;
        if (c->message == WM_REC_A) { InterlockedIncrement(&cwr_n); cwr_res = c->lResult; }
    }
    return U(CallNextHookEx)(NULL, code, w, l);
}
TEST(messages_x_callwndproc_hooks_see_sent_messages) {
    HWND h;
    REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(SMA); REQUIRE_MH();
    mh_reset(); cw_n = cwr_n = 0; cw_sent_flag = -1; cwr_res = -1;
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    cw_hook = U(SetWindowsHookExA)(WH_CALLWNDPROC, cw_proc, NULL, GetCurrentThreadId());
    cwr_hook = U(SetWindowsHookExA)(WH_CALLWNDPROCRET, cwr_proc, NULL, GetCurrentThreadId());
    if (!cw_hook || !cwr_hook) {
        t_note("cannot install CALLWNDPROC hooks");
        if (cw_hook) U(UnhookWindowsHookEx)(cw_hook);
        if (cwr_hook) U(UnhookWindowsHookEx)(cwr_hook);
        mh_kill(h); return;
    }
    SMA(h, MU(241), 0, 0);
    EXPECT_EQ(cw_n, 1);
    /* Documented as nonzero for a message sent by the current thread; Win10 22H2 reports 0 for a
     * same-thread SendMessage (the oracle run), so record it instead of asserting it. */
    t_note("WH_CALLWNDPROC wParam for a same-thread send: %ld", (long)cw_sent_flag);
    CHECK(cw_sent_flag == 0 || cw_sent_flag == 1);
    EXPECT_EQ(SMA(h, WM_REC_A, 20, 22), 42);
    EXPECT_EQ(cwr_n, 1);
    EXPECT_EQ(cwr_res, 42);
    PMA(h, MU(241), 0, 0);
    EXPECT_EQ(cw_n, 1);                         /* a posted message is not reported before it is dispatched */
    mh_drain();
    t_note("WH_CALLWNDPROC count after dispatching a posted message: %ld (1 = only sent messages are hooked)", (long)cw_n);
    CHECK(U(UnhookWindowsHookEx)(cw_hook));
    CHECK(U(UnhookWindowsHookEx)(cwr_hook));
    { LONG before = cw_n; SMA(h, MU(241), 0, 0); EXPECT_EQ(cw_n, before); }
    mh_kill(h);
}

TEST(messages_x_unhook_twice_and_bad_handle) {
    HHOOK hk; BOOL r;
    REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(UnhookWindowsHookEx));
    hk = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, GetCurrentThreadId());
    if (!hk) { t_note("cannot install hook"); return; }
    CHECK(U(UnhookWindowsHookEx)(hk) == TRUE);
    SETE(); r = U(UnhookWindowsHookEx)(hk);
    CHECK(r == FALSE);
    t_note("UnhookWindowsHookEx(stale) error %lu", GetLastError());
    SETE(); r = U(UnhookWindowsHookEx)((HHOOK)0xDEADBEEF);
    CHECK(r == FALSE);
}

TEST(messages_x_sethookex_invalid_params) {
    HHOOK hk;
    REQUIRE(U(SetWindowsHookExA));
    SETE(); hk = U(SetWindowsHookExA)(WH_GETMESSAGE, NULL, NULL, GetCurrentThreadId());
    CHECK(hk == NULL); t_note("SetWindowsHookEx(NULL proc) error %lu", GetLastError());
    SETE(); hk = U(SetWindowsHookExA)(9999, gm_proc, NULL, GetCurrentThreadId());
    CHECK(hk == NULL); t_note("SetWindowsHookEx(bad id) error %lu", GetLastError());
    SETE(); hk = U(SetWindowsHookExA)(WH_GETMESSAGE, gm_proc, NULL, 0xFFFFFFF0u);
    CHECK(hk == NULL); t_note("SetWindowsHookEx(bad thread) error %lu", GetLastError());
}

/* ---- creation failures ---- */
TEST(messages_x_create_toplevel_child_fails) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    SETE(); h = mh_mk(WS_CHILD, 0, NULL);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), 1406);          /* ERROR_TLW_WITH_WSCHILD */
    if (h) mh_kill(h);
}

TEST(messages_x_create_unknown_class_fails) {
    HWND h;
    REQUIRE(CWEA);
    SETE(); h = CWEA(0, "t32_messages_no_such_class", "x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), 1407);          /* ERROR_CANNOT_FIND_WND_CLASS */
    SETE(); h = CWEW(0, L"t32_messages_no_such_class_w", L"x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h == NULL);
    EXPECT_EQ(GetLastError(), 1407);
    if (h) DESTROYW(h);
}

TEST(messages_x_create_invalid_parent_fails) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    SETE(); h = mh_mk(WS_CHILD, 0, (HWND)0xDEADBEEF);
    CHECK(h == NULL);
    t_note("CreateWindowEx(bad parent) error %lu", GetLastError());
    if (h) mh_kill(h);
}

TEST(messages_x_create_failure_sends_no_messages_for_unknown_class) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1;
    h = CWEA(0, "t32_messages_no_such_class2", "x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    mh_rec = 0;
    CHECK(h == NULL);
    EXPECT_EQ(mh_nev, 0);
}

/* ---- Unicode window text roundtrip ---- */
TEST(messages_x_wide_window_text_unicode_roundtrip) {
    HWND h; static const WCHAR txt[] = { 'A', 0x00E9, 0x20AC, 0x4E2D, 'Z', 0 }; WCHAR out[16]; LRESULT r; int i;
    REQUIRE(SMW); REQUIRE_MH();
    mh_reset();
    h = mh_mkw(0, 0, NULL); REQUIRE(h);
    EXPECT_EQ(SMW(h, WM_SETTEXT, 0, (LPARAM)txt), TRUE);
    EXPECT_EQ(SMW(h, WM_GETTEXTLENGTH, 0, 0), 5);
    for (i = 0; i < 16; i++) out[i] = 0x7A7A;
    r = SMW(h, WM_GETTEXT, 16, (LPARAM)out);
    EXPECT_EQ(r, 5);
    EXPECT_WSTR(out, txt);
    EXPECT_EQ(out[6], 0x7A7A);
    for (i = 0; i < 16; i++) out[i] = 0x7A7A;
    r = SMW(h, WM_GETTEXT, 3, (LPARAM)out);
    EXPECT_EQ(r, 2);
    EXPECT_EQ(out[0], 'A'); EXPECT_EQ(out[1], 0x00E9); EXPECT_EQ(out[2], 0);
    mh_kill(h);
}

TEST(messages_x_settext_null_clears_title) {
    HWND h;
    REQUIRE(U(SetWindowTextA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    CHECK(U(SetWindowTextA)(h, "something"));
    SETE();
    if (U(SetWindowTextA)(h, NULL)) {
        EXPECT_EQ(U(GetWindowTextLengthA)(h), 0);
    } else t_note("SetWindowTextA(h, NULL) failed, err %lu", GetLastError());
    mh_kill(h);
}

/* ---- InSendMessageEx inside nested sends ---- */
TEST(messages_x_insend_flag_in_nested_send_back) {
    MH_TH t; DWORD_PTR r = 0; int k;
    REQUIRE(U(InSendMessageEx)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    mh_mainwnd = mh_mk(0, 0, NULL); REQUIRE(mh_mainwnd);
    mh_flag[1] = 0;
    mh_rec = 1;
    CHECK(mh_xsend(t.hwnd, WM_REC_D, 0, 0, &r));
    mh_rec = 0;
    k = mh_find(WM_REC_E, 0);
    CHECK(k >= 0);
    if (k >= 0) {
        CHECK((mh_ev[k].ins & ISMEX_SEND) != 0);
        EXPECT_EQ(mh_ev[k].tid, GetCurrentThreadId());
    }
    EXPECT_EQ(U(InSendMessageEx)(NULL), ISMEX_NOSEND);
    mh_kill(mh_mainwnd); mh_mainwnd = NULL;
    mh_th_stop(&t);
}

/* ---- paint status ---- */
TEST(messages_x_paint_status_and_wm_paint) {
    HWND h; DWORD s; MSG m;
    REQUIRE(U(InvalidateRect)); REQUIRE(U(ShowWindow)); REQUIRE(U(GetQueueStatus)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, NULL); REQUIRE(h);
    U(ShowWindow)(h, SW_SHOWNA);
    mh_drain();
    s = U(GetQueueStatus)(QS_PAINT);
    CHECK((s >> 16 & QS_PAINT) == 0);
    CHECK(U(InvalidateRect)(h, NULL, FALSE));
    s = U(GetQueueStatus)(QS_PAINT);
    CHECKF((s >> 16 & QS_PAINT) != 0, "status %lx", s);
    CHECK(PKA(&m, h, WM_PAINT, WM_PAINT, PM_NOREMOVE));
    EXPECT_EQ(m.message, WM_PAINT);
    EXPECT_EQ(m.hwnd, h);
    EXPECT_EQ(m.wParam, 0);
    mh_drain();
    s = U(GetQueueStatus)(QS_PAINT);
    CHECKF((s >> 16 & QS_PAINT) == 0, "status %lx after paint", s);
    CHECK(!PKA(&m, h, WM_PAINT, WM_PAINT, PM_NOREMOVE));
    mh_kill(h);
}

TEST(messages_x_paint_lowest_priority) {
    HWND h; MSG m;
    REQUIRE(U(InvalidateRect)); REQUIRE(U(ShowWindow)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, NULL); REQUIRE(h);
    U(ShowWindow)(h, SW_SHOWNA);
    mh_drain();
    U(InvalidateRect)(h, NULL, FALSE);
    PMA(h, MU(250), 0, 0);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, MU(250));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, WM_PAINT);
    U(DispatchMessageA)(&m);
    mh_drain();
    mh_kill(h);
}

/* ---- WM_GETMINMAXINFO ---- */
static LRESULT hk_minmax(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_GETMINMAXINFO) { MINMAXINFO *p = (MINMAXINFO *)l; p->ptMinTrackSize.x = 321; p->ptMinTrackSize.y = 123; *hd = 1; return 0; }
    return 0;
}
TEST(messages_x_getminmaxinfo_default_and_sent) {
    HWND h; MINMAXINFO mm; LRESULT r;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_OVERLAPPEDWINDOW, 0, NULL); REQUIRE(h);
    memset(&mm, 0, sizeof mm);
    r = SMA(h, WM_GETMINMAXINFO, 0, (LPARAM)&mm);
    EXPECT_EQ(r, 0);
    t_note("DefWindowProc WM_GETMINMAXINFO: max %ldx%ld track min %ldx%ld max %ldx%ld", mm.ptMaxSize.x, mm.ptMaxSize.y,
           mm.ptMinTrackSize.x, mm.ptMinTrackSize.y, mm.ptMaxTrackSize.x, mm.ptMaxTrackSize.y);
    mh_kill(h);
}
TEST(messages_x_getminmaxinfo_handler_values_returned) {
    HWND h; MINMAXINFO mm;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_OVERLAPPEDWINDOW, 0, NULL); REQUIRE(h);
    mh_hook = hk_minmax;
    memset(&mm, 0, sizeof mm);
    SMA(h, WM_GETMINMAXINFO, 0, (LPARAM)&mm);
    mh_hook = NULL;
    EXPECT_EQ(mm.ptMinTrackSize.x, 321);
    EXPECT_EQ(mm.ptMinTrackSize.y, 123);
    mh_kill(h);
}

/* ---- broadcast receipt ---- */
TEST(messages_x_post_broadcast_reaches_toplevel) {
    UINT mid; HWND t1, child; int i, got = 0, gotc = 0;
    REQUIRE(PMA); REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    mid = U(RegisterWindowMessageA)("t32_messages_broadcast_post_rx");
    t1 = mh_mk(0, 0, NULL); REQUIRE(t1);
    child = mh_mk(WS_CHILD, 0, t1); REQUIRE(child);
    mh_rec = 1;
    CHECK(PMA(HWND_BROADCAST, mid, 3, 4));
    mh_pump_until(&mh_flag[4], 300);
    mh_drain();
    mh_rec = 0;
    for (i = 0; i < mh_nev; i++) if (mh_ev[i].m == mid) { if (mh_ev[i].h == t1) got++; if (mh_ev[i].h == child) gotc++; }
    if (got != 1) t_note("broadcast post: top-level window received %d", got);
    EXPECT_EQ(gotc, 0);
    mh_kill(t1);
}

/* ---- destroyed-window handle lifetime with messages in flight ---- */
TEST(messages_x_sendmessage_after_destroy_in_wm_destroy_handler) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    DESTROYW(h);
    mh_rec = 0;
    SETE();
    EXPECT_EQ(SMA(h, WM_REC_A, 1, 2), 0);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
    CHECK(!PMA(h, WM_NULL, 0, 0));
    CHECK(!U(SendNotifyMessageA)(h, WM_NULL, 0, 0));
}

TEST(messages_x_sendmessage_callback_to_destroyed_window) {
    HWND h; BOOL r;
    REQUIRE(U(SendMessageCallbackA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    DESTROYW(h);
    SETE(); r = U(SendMessageCallbackA)(h, WM_NULL, 0, 0, NULL, 0);
    CHECK(r == FALSE);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
}
