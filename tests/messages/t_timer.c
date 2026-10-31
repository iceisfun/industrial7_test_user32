/* Timers: SetTimer / KillTimer / SetCoalescableTimer / WM_TIMER / TIMERPROC. */
#include "mh.h"

#define MU(n) (WM_USER + (n))
#define NOERR 0xdeadbeef
#define SETE() SetLastError(NOERR)

typedef struct { volatile LONG n; HWND h; UINT m; UINT_PTR id; DWORD tick; } TP;
static TP tp1, tp2;
static VOID CALLBACK tproc1(HWND h, UINT m, UINT_PTR id, DWORD t) { tp1.h = h; tp1.m = m; tp1.id = id; tp1.tick = t; InterlockedIncrement(&tp1.n); }
static VOID CALLBACK tproc2(HWND h, UINT m, UINT_PTR id, DWORD t) { tp2.h = h; tp2.m = m; tp2.id = id; tp2.tick = t; InterlockedIncrement(&tp2.n); }
static void tp_reset(void) { memset(&tp1, 0, sizeof tp1); memset(&tp2, 0, sizeof tp2); }

/* pump for up to ms milliseconds, dispatching everything, stop early when *stop >= want */
static void tpump(DWORD ms, volatile LONG *stop, LONG want) {
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < ms) {
        MSG m;
        while (PKA(&m, NULL, 0, 0, PM_REMOVE)) { if (m.message == WM_QUIT) return; U(TranslateMessage)(&m); U(DispatchMessageA)(&m); }
        if (stop && *stop >= want) return;
        Sleep(2);
    }
}

TEST(messages_timer_null_hwnd_returns_id_and_message) {
    UINT_PTR id; MSG m; DWORD t0; int got = 0;
    REQUIRE(U(SetTimer)); REQUIRE(U(KillTimer)); REQUIRE_MH();
    mh_reset();
    id = U(SetTimer)(NULL, 0, 30, NULL);
    CHECK(id != 0);
    t0 = GetTickCount();
    while (GetTickCount() - t0 < 2000 && !got) {
        if (PKA(&m, NULL, WM_TIMER, WM_TIMER, PM_REMOVE)) got = 1; else Sleep(5);
    }
    CHECK(got);
    if (got) {
        EXPECT_EQ(m.message, WM_TIMER); EXPECT_EQ(m.hwnd, NULL);
        EXPECT_EQ(m.wParam, id); EXPECT_EQ(m.lParam, 0);
    }
    CHECK(U(KillTimer)(NULL, id) == TRUE);
    mh_drain();
}

TEST(messages_timer_hwnd_timer_returns_given_id) {
    HWND h; UINT_PTR r; int k;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    r = U(SetTimer)(h, 7, 30, NULL);
    EXPECT_EQ(r, 7);
    mh_rec = 1;
    tpump(2000, &mh_nev, 1);
    mh_rec = 0;
    k = mh_find(WM_TIMER, 0);
    CHECK(k >= 0);
    if (k >= 0) { EXPECT_EQ(mh_ev[k].h, h); EXPECT_EQ(mh_ev[k].w, 7); EXPECT_EQ(mh_ev[k].l, 0); }
    CHECK(U(KillTimer)(h, 7) == TRUE);
    mh_kill(h);
}

TEST(messages_timer_large_id_roundtrip) {
    HWND h; UINT_PTR r;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    r = U(SetTimer)(h, 0xFFFF, 1000, NULL);
    EXPECT_EQ(r, 0xFFFF);
    CHECK(U(KillTimer)(h, 0xFFFF));
    r = U(SetTimer)(h, 0x7FFFFFFF, 1000, NULL);
    t_note("SetTimer id 0x7FFFFFFF returned %llx", (unsigned long long)r);
    if (r) U(KillTimer)(h, 0x7FFFFFFF);
    mh_kill(h);
}

TEST(messages_timer_replace_same_id_single_timer) {
    HWND h; UINT_PTR a, b; int n;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    a = U(SetTimer)(h, 3, 40, NULL);
    b = U(SetTimer)(h, 3, 40, NULL);
    EXPECT_EQ(a, 3); EXPECT_EQ(b, 3);
    CHECK(U(KillTimer)(h, 3) == TRUE);
    SETE();
    CHECK(U(KillTimer)(h, 3) == FALSE);    /* only one timer existed */
    mh_rec = 1; tpump(150, NULL, 0); mh_rec = 0;
    n = mh_count(WM_TIMER);
    EXPECT_EQ(n, 0);
    mh_kill(h);
}

TEST(messages_timer_timerproc_called_instead_of_wndproc) {
    HWND h; UINT_PTR r; DWORD now;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    r = U(SetTimer)(h, 9, 30, tproc1);
    EXPECT_EQ(r, 9);
    mh_rec = 1;
    tpump(2000, &tp1.n, 1);
    now = GetTickCount();
    mh_rec = 0;
    CHECK(tp1.n >= 1);
    EXPECT_EQ(tp1.h, h); EXPECT_EQ(tp1.m, WM_TIMER); EXPECT_EQ(tp1.id, 9);
    CHECKF((LONG)(now - tp1.tick) >= 0 && (LONG)(now - tp1.tick) < 2500, "tick %lu now %lu", tp1.tick, now);
    EXPECT_EQ(mh_count(WM_TIMER), 0);
    U(KillTimer)(h, 9);
    mh_kill(h);
}

TEST(messages_timer_timerproc_null_hwnd) {
    UINT_PTR id;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    id = U(SetTimer)(NULL, 0, 30, tproc1);
    CHECK(id != 0);
    tpump(2000, &tp1.n, 1);
    CHECK(tp1.n >= 1);
    EXPECT_EQ(tp1.h, NULL); EXPECT_EQ(tp1.id, id); EXPECT_EQ(tp1.m, WM_TIMER);
    CHECK(U(KillTimer)(NULL, id));
    mh_drain();
}

TEST(messages_timer_message_carries_timerproc_in_lparam) {
    UINT_PTR id; MSG m; DWORD t0; int got = 0;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    id = U(SetTimer)(NULL, 0, 30, tproc1); REQUIRE(id);
    t0 = GetTickCount();
    while (GetTickCount() - t0 < 2000 && !got) { if (PKA(&m, NULL, WM_TIMER, WM_TIMER, PM_REMOVE)) got = 1; else Sleep(5); }
    CHECK(got);
    if (got) {
        EXPECT_EQ(m.lParam, (LPARAM)tproc1);
        EXPECT_EQ(m.wParam, id);
        CHECK(tp1.n == 0);                 /* retrieving alone must not call the proc */
        U(DispatchMessageA)(&m);
        CHECK(tp1.n == 1);
    }
    U(KillTimer)(NULL, id);
    mh_drain();
}

TEST(messages_timer_kill_stops_delivery) {
    HWND h; int n;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetTimer)(h, 1, 20, NULL);
    mh_rec = 1; tpump(2000, &mh_nev, 1);
    CHECK(U(KillTimer)(h, 1));
    mh_drain();
    mh_clear();
    tpump(200, NULL, 0);
    mh_rec = 0;
    n = mh_count(WM_TIMER);
    EXPECT_EQ(n, 0);
    mh_kill(h);
}

TEST(messages_timer_kill_nonexistent) {
    HWND h; BOOL r;
    REQUIRE(U(KillTimer)); REQUIRE_MH();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    SETE(); r = U(KillTimer)(h, 12345);
    CHECK(r == FALSE);
    t_note("KillTimer nonexistent id: error %lu", GetLastError());
    SETE(); r = U(KillTimer)(NULL, 0x7777);
    CHECK(r == FALSE);
    SETE(); r = U(KillTimer)(h, 0);
    CHECK(r == FALSE);
    mh_kill(h);
}

TEST(messages_timer_kill_twice) {
    UINT_PTR id;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    id = U(SetTimer)(NULL, 0, 1000, NULL); REQUIRE(id);
    CHECK(U(KillTimer)(NULL, id) == TRUE);
    CHECK(U(KillTimer)(NULL, id) == FALSE);
}

TEST(messages_timer_invalid_hwnd) {
    UINT_PTR r; BOOL k;
    REQUIRE(U(SetTimer)); REQUIRE(U(KillTimer));
    SETE(); r = U(SetTimer)((HWND)0xDEADBEEF, 1, 100, NULL);
    CHECK(r == 0); t_note("SetTimer-family invalid hwnd error %lu", GetLastError());
    SETE(); k = U(KillTimer)((HWND)0xDEADBEEF, 1);
    CHECK(k == FALSE);
    t_note("KillTimer invalid hwnd error %lu", GetLastError());
}

TEST(messages_timer_destroyed_hwnd) {
    HWND h; UINT_PTR r;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    DESTROYW(h);
    SETE(); r = U(SetTimer)(h, 1, 100, NULL);
    CHECK(r == 0); t_note("SetTimer-family invalid hwnd error %lu", GetLastError());
}

TEST(messages_timer_other_thread_window) {
    MH_TH t; UINT_PTR r;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    SETE(); r = U(SetTimer)(t.hwnd, 1, 100, NULL);
    CHECK(r == 0);
    t_note("SetTimer on other thread's window error %lu", GetLastError());
    if (r) U(KillTimer)(t.hwnd, 1);
    mh_th_stop(&t);
}

TEST(messages_timer_destroy_window_kills_timers) {
    HWND h;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetTimer)(h, 1, 20, tproc1);
    U(SetTimer)(h, 2, 20, tproc1);
    DESTROYW(h);
    tpump(200, NULL, 0);
    EXPECT_EQ(tp1.n, 0);
}

TEST(messages_timer_zero_elapse_is_clamped) {
    HWND h; int n;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    EXPECT_EQ(U(SetTimer)(h, 1, 0, tproc1), 1);
    tpump(300, NULL, 0);
    U(KillTimer)(h, 1);
    n = tp1.n;
    CHECK(n >= 1);
    CHECKF(n <= 40, "count %d in 300 ms", n);
    mh_kill(h);
}

TEST(messages_timer_small_elapse_values_accepted) {
    HWND h; UINT_PTR r; UINT e[] = { 0, 1, 5, 9, 10, 11 }; int i;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    for (i = 0; i < 6; i++) {
        r = U(SetTimer)(h, 100 + i, e[i], NULL);
        CHECKF(r == (UINT_PTR)(100 + i), "elapse %u -> %llx", e[i], (unsigned long long)r);
    }
    for (i = 0; i < 6; i++) CHECK(U(KillTimer)(h, 100 + i));
    mh_kill(h);
}

TEST(messages_timer_huge_elapse_clamped) {
    HWND h; UINT_PTR r;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    r = U(SetTimer)(h, 1, 0xFFFFFFFFu, NULL);
    EXPECT_EQ(r, 1);
    CHECK(U(KillTimer)(h, 1));
    r = U(SetTimer)(h, 1, 0x80000000u, NULL);
    EXPECT_EQ(r, 1);
    CHECK(U(KillTimer)(h, 1));
    r = U(SetTimer)(h, 1, USER_TIMER_MAXIMUM, NULL);
    EXPECT_EQ(r, 1);
    CHECK(U(KillTimer)(h, 1));
    mh_kill(h);
}

TEST(messages_timer_does_not_fire_early) {
    HWND h; DWORD t0, dt;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    t0 = GetTickCount();
    U(SetTimer)(h, 1, 120, tproc1);
    tpump(3000, &tp1.n, 1);
    dt = GetTickCount() - t0;
    U(KillTimer)(h, 1);
    CHECK(tp1.n >= 1);
    CHECKF(dt >= 90, "first fire after %lu ms", dt);
    CHECKF(dt < 2500, "first fire after %lu ms", dt);
    mh_kill(h);
}

TEST(messages_timer_periodic_repeats) {
    HWND h; DWORD t0, dt;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    t0 = GetTickCount();
    U(SetTimer)(h, 1, 50, tproc1);
    tpump(5000, &tp1.n, 4);
    dt = GetTickCount() - t0;
    U(KillTimer)(h, 1);
    CHECK(tp1.n >= 4);
    CHECKF(dt >= 150, "four fires after %lu ms", dt);
    mh_kill(h);
}

TEST(messages_timer_two_timers_independent) {
    HWND h;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetTimer)(h, 1, 30, tproc1);
    U(SetTimer)(h, 2, 30, tproc2);
    tpump(3000, &tp1.n, 2);
    tpump(3000, &tp2.n, 2);
    CHECK(tp1.n >= 2 && tp2.n >= 2);
    EXPECT_EQ(tp1.id, 1); EXPECT_EQ(tp2.id, 2);
    CHECK(U(KillTimer)(h, 1)); CHECK(U(KillTimer)(h, 2));
    mh_kill(h);
}

TEST(messages_timer_same_id_different_windows) {
    HWND a, b;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    a = mh_mk(0, 0, NULL); b = mh_mk(0, 0, NULL); REQUIRE(a && b);
    EXPECT_EQ(U(SetTimer)(a, 1, 30, tproc1), 1);
    EXPECT_EQ(U(SetTimer)(b, 1, 30, tproc2), 1);
    tpump(3000, &tp1.n, 1);
    tpump(3000, &tp2.n, 1);
    EXPECT_EQ(tp1.h, a); EXPECT_EQ(tp2.h, b);
    CHECK(U(KillTimer)(a, 1));
    CHECK(U(KillTimer)(b, 1));
    mh_kill(a); mh_kill(b);
}

TEST(messages_timer_null_hwnd_ids_distinct) {
    UINT_PTR a, b, c;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    a = U(SetTimer)(NULL, 0, 5000, NULL);
    b = U(SetTimer)(NULL, 0, 5000, NULL);
    c = U(SetTimer)(NULL, 0, 5000, NULL);
    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
    CHECK(U(KillTimer)(NULL, a)); CHECK(U(KillTimer)(NULL, b)); CHECK(U(KillTimer)(NULL, c));
}

TEST(messages_timer_null_hwnd_with_existing_id_replaces) {
    UINT_PTR a, b;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    a = U(SetTimer)(NULL, 0, 5000, NULL); REQUIRE(a);
    b = U(SetTimer)(NULL, a, 6000, NULL);
    EXPECT_EQ(b, a);
    CHECK(U(KillTimer)(NULL, a));
    CHECK(!U(KillTimer)(NULL, a));
}

TEST(messages_timer_posted_message_has_priority_over_timer) {
    HWND h; MSG m;
    REQUIRE(U(SetTimer)); REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetTimer)(h, 1, 15, NULL);
    Sleep(80);
    PMA(h, MU(70), 0, 0);
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, MU(70));
    CHECK(PKA(&m, NULL, 0, 0, PM_REMOVE));
    EXPECT_EQ(m.message, WM_TIMER);
    U(KillTimer)(h, 1);
    mh_kill(h);
}

TEST(messages_timer_queue_status_bit) {
    HWND h; DWORD s;
    REQUIRE(U(SetTimer)); REQUIRE(U(GetQueueStatus)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    s = U(GetQueueStatus)(QS_TIMER);
    CHECK((s >> 16 & QS_TIMER) == 0);
    U(SetTimer)(h, 1, 20, NULL);
    Sleep(100);
    s = U(GetQueueStatus)(QS_TIMER);
    CHECKF((s >> 16 & QS_TIMER) != 0, "status %lx", s);
    U(KillTimer)(h, 1);
    mh_drain();
    mh_kill(h);
}

TEST(messages_timer_pending_timers_coalesce) {
    HWND h; MSG m; int n = 0, guard = 0;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetTimer)(h, 1, 20, NULL);
    Sleep(250);
    while (guard++ < 100 && PKA(&m, NULL, WM_TIMER, WM_TIMER, PM_REMOVE)) n++;
    U(KillTimer)(h, 1);
    CHECKF(n >= 1 && n <= 3, "retrieved %d WM_TIMER for ~12 missed intervals", n);
    mh_kill(h);
}

TEST(messages_timer_peek_noremove_keeps_timer) {
    HWND h; MSG m; int ok;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetTimer)(h, 1, 20, NULL);
    Sleep(80);
    ok = PKA(&m, NULL, WM_TIMER, WM_TIMER, PM_NOREMOVE);
    CHECK(ok);
    if (!PKA(&m, NULL, WM_TIMER, WM_TIMER, PM_NOREMOVE)) t_note("second PM_NOREMOVE peek of WM_TIMER found nothing");
    if (PKA(&m, NULL, WM_TIMER, WM_TIMER, PM_REMOVE)) EXPECT_EQ(m.wParam, 1);
    else t_note("PM_REMOVE after PM_NOREMOVE peek of WM_TIMER found nothing");
    U(KillTimer)(h, 1);
    mh_kill(h);
}

TEST(messages_timer_synthetic_wm_timer_to_wndproc) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    SMA(h, WM_TIMER, 55, 0);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_TIMER), 1);
    { int k = mh_find(WM_TIMER, 0); CHECK(k >= 0 && mh_ev[k].w == 55 && mh_ev[k].l == 0); }
    mh_kill(h);
}

TEST(messages_timer_forged_wm_timer_with_callback) {
    HWND h; MSG m;
    REQUIRE(U(DispatchMessageA)); REQUIRE_MH();
    mh_reset(); tp_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    memset(&m, 0, sizeof m);
    m.hwnd = h; m.message = WM_TIMER; m.wParam = 123; m.lParam = (LPARAM)tproc1;
    U(DispatchMessageA)(&m);
    t_note("forged WM_TIMER with TIMERPROC lParam: proc called %ld times", (long)tp1.n);
    SMA(h, WM_TIMER, 124, (LPARAM)tproc1);
    t_note("SendMessage(WM_TIMER,lParam=proc): proc called %ld times", (long)tp1.n);
    mh_kill(h);
}

TEST(messages_timer_many_timers_one_window) {
    HWND h; int i, ok = 0, killed = 0;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    for (i = 1; i <= 200; i++) if (U(SetTimer)(h, i, 60000, NULL) == (UINT_PTR)i) ok++;
    EXPECT_EQ(ok, 200);
    for (i = 1; i <= 200; i++) if (U(KillTimer)(h, i)) killed++;
    EXPECT_EQ(killed, 200);
    mh_kill(h);
}

TEST(messages_timer_on_child_and_message_only_windows) {
    HWND p, c, mo;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    c = mh_mk(WS_CHILD, 0, p); mo = mh_mk(0, 0, HWND_MESSAGE);
    REQUIRE(c && mo);
    EXPECT_EQ(U(SetTimer)(c, 4, 30, tproc1), 4);
    EXPECT_EQ(U(SetTimer)(mo, 5, 30, tproc2), 5);
    tpump(3000, &tp1.n, 1);
    tpump(3000, &tp2.n, 1);
    CHECK(tp1.n >= 1 && tp2.n >= 1);
    EXPECT_EQ(tp1.h, c); EXPECT_EQ(tp2.h, mo);
    U(KillTimer)(c, 4); U(KillTimer)(mo, 5);
    mh_kill(p); mh_kill(mo);
}

static volatile LONG killer_n; static volatile LONG killer_ret;
static VOID CALLBACK tproc_kill(HWND h, UINT m, UINT_PTR id, DWORD t) {
    InterlockedIncrement(&killer_n);
    killer_ret = U(KillTimer)(h, id) ? 1 : 2;
}
TEST(messages_timer_proc_can_kill_own_timer) {
    HWND h;
    REQUIRE(U(SetTimer)); REQUIRE_MH();
    mh_reset(); killer_n = 0; killer_ret = 0;
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    EXPECT_EQ(U(SetTimer)(h, 1, 20, tproc_kill), 1);
    tpump(2000, &killer_n, 1);
    tpump(150, NULL, 0);
    EXPECT_EQ(killer_n, 1);
    EXPECT_EQ(killer_ret, 1);
    SETE();
    CHECK(U(KillTimer)(h, 1) == FALSE);
    mh_kill(h);
}

TEST(messages_timer_coalescable_basic) {
    HWND h; UINT_PTR r; ULONG tol[] = { 0, 10, 100, TIMERV_DEFAULT_COALESCING, TIMERV_NO_COALESCING, TIMERV_COALESCING_MIN, TIMERV_COALESCING_MAX };
    int i;
    REQUIRE(U(SetCoalescableTimer)); REQUIRE(U(KillTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    for (i = 0; i < 7; i++) {
        r = U(SetCoalescableTimer)(h, 1, 1000, NULL, tol[i]);
        CHECKF(r == 1, "tolerance %lx -> %llx", tol[i], (unsigned long long)r);
        CHECK(U(KillTimer)(h, 1));
    }
    mh_kill(h);
}

TEST(messages_timer_coalescable_fires) {
    HWND h;
    REQUIRE(U(SetCoalescableTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    EXPECT_EQ(U(SetCoalescableTimer)(h, 2, 40, tproc1, 20), 2);
    tpump(3000, &tp1.n, 2);
    CHECK(tp1.n >= 2);
    U(KillTimer)(h, 2);
    mh_kill(h);
}

TEST(messages_timer_coalescable_null_hwnd) {
    UINT_PTR id;
    REQUIRE(U(SetCoalescableTimer)); REQUIRE_MH();
    mh_reset(); tp_reset();
    id = U(SetCoalescableTimer)(NULL, 0, 40, tproc1, TIMERV_NO_COALESCING);
    CHECK(id != 0);
    tpump(3000, &tp1.n, 1);
    CHECK(tp1.n >= 1);
    EXPECT_EQ(tp1.id, id);
    CHECK(U(KillTimer)(NULL, id));
    mh_drain();
}

TEST(messages_timer_coalescable_bad_tolerance) {
    HWND h; UINT_PTR r;
    REQUIRE(U(SetCoalescableTimer)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    SETE();
    r = U(SetCoalescableTimer)(h, 1, 1000, NULL, 0x80000000u);
    t_note("SetCoalescableTimer tolerance 0x80000000 -> %llx err %lu", (unsigned long long)r, GetLastError());
    if (r) U(KillTimer)(h, 1);
    SETE();
    r = U(SetCoalescableTimer)(h, 1, 1000, NULL, TIMERV_COALESCING_MAX + 1);
    t_note("SetCoalescableTimer tolerance MAX+1 -> %llx err %lu", (unsigned long long)r, GetLastError());
    if (r) U(KillTimer)(h, 1);
    mh_kill(h);
}

TEST(messages_timer_coalescable_invalid_hwnd) {
    UINT_PTR r;
    REQUIRE(U(SetCoalescableTimer));
    SETE(); r = U(SetCoalescableTimer)((HWND)0xDEADBEEF, 1, 100, NULL, 0);
    CHECK(r == 0); t_note("SetTimer-family invalid hwnd error %lu", GetLastError());
}

TEST(messages_timer_msgwait_wakes_on_timer) {
    HWND h; DWORD r, t0, dt;
    REQUIRE(U(SetTimer)); REQUIRE(U(MsgWaitForMultipleObjects)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetTimer)(h, 1, 60, NULL);
    t0 = GetTickCount();
    r = U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 3000, QS_TIMER);
    dt = GetTickCount() - t0;
    CHECKF(r == WAIT_OBJECT_0, "ret %lu", r);
    CHECKF(dt >= 40 && dt < 2500, "dt %lu", dt);
    U(KillTimer)(h, 1);
    mh_drain();
    mh_kill(h);
}

TEST(messages_timer_getmessage_returns_wm_timer) {
    HWND h; MSG m; BOOL r;
    REQUIRE(U(SetTimer)); REQUIRE(GMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    REQUIRE(U(SetTimer)(h, 8, 40, NULL) == 8);
    r = GMA(&m, h, WM_TIMER, WM_TIMER);      /* a due timer guarantees this returns */
    CHECK(r == TRUE);
    EXPECT_EQ(m.message, WM_TIMER); EXPECT_EQ(m.wParam, 8); EXPECT_EQ(m.hwnd, h);
    CHECK(U(KillTimer)(h, 8));
    mh_drain();
    mh_kill(h);
}

TEST(messages_timer_getmessage_wide_returns_wm_timer) {
    HWND h; MSG m; BOOL r;
    REQUIRE(U(SetTimer)); REQUIRE(GMW); REQUIRE_MH();
    mh_reset();
    h = mh_mkw(0, 0, NULL); REQUIRE(h);
    REQUIRE(U(SetTimer)(h, 9, 40, NULL) == 9);
    r = GMW(&m, NULL, WM_TIMER, WM_TIMER);
    CHECK(r == TRUE);
    EXPECT_EQ(m.wParam, 9);
    CHECK(U(KillTimer)(h, 9));
    mh_drain();
    mh_kill(h);
}
