/* gap_05: SetLastErrorEx, SetMessageQueue, TranslateMessageEx, pointer-input
 * targets, DPI internals, SetSysColorsTemp, IMM / WOW entry points. */
#include "g5.h"

UDECL(BOOL, TranslateMessageEx, (const MSG *, UINT));
UDECL(BOOL, UnregisterPointerInputTargetEx, (HWND, DWORD));
UDECL(BOOL, RegisterPointerInputTargetEx, (HWND, DWORD, BOOL));
UDECL(BOOL, GetPointerFrameArrivalTimes, (UINT, UINT, UINT64 *));
UDECL(BOOL, SetProcessDpiAwarenessInternal, (int));
UDECL(ULONG_PTR, SetSysColorsTemp, (const COLORREF *, const HBRUSH *, UINT_PTR));
UDECL(ULONG_PTR, User32InitializeImmEntryTable, (DWORD, DWORD));
UDECL(ULONG_PTR, UserRegisterWowHandlers, (void *, void *));
UDECL(BOOL, SetMessageQueue_, (int));
UDECL(BOOL, DdeSetQualityOfService, (HWND, const void *, void *));
UDECL(BOOL, DdeGetQualityOfService, (HWND, const void *, void *));

/* ---------------- SetLastErrorEx ---------------- */
static const DWORD g_errs[] = { 0, 1, 2, 5, 6, 87, 122, 183, 997, 1400, 1404, 1813, 0x7fff, 0xffff, 0x10000,
    0x12345678, 0x7fffffff, 0x80000000, 0xc0000005, 0xdeadbeef, 0xfffffffe, 0xffffffff };
#define NERRS (sizeof g_errs / sizeof g_errs[0])

TEST(gap5_sle_basic_types) {
    REQUIRE(U(SetLastErrorEx));
    for (DWORD t = 0; t < 8; t++) {
        SetLastError(0);
        U(SetLastErrorEx)(1234 + t, t);
        EXPECT_EQ(GetLastError(), 1234 + t);
    }
}
TEST(gap5_sle_all_values_type0) {
    REQUIRE(U(SetLastErrorEx));
    for (unsigned i = 0; i < NERRS; i++) {
        SetLastError(0x55);
        U(SetLastErrorEx)(g_errs[i], 0);
        DWORD e = GetLastError();
        CHECKF(e == g_errs[i], "idx %u got 0x%lx want 0x%lx", i, e, g_errs[i]);
    }
}
TEST(gap5_sle_all_values_slerror) {
    REQUIRE(U(SetLastErrorEx));
    for (unsigned i = 0; i < NERRS; i++) {
        SetLastError(0x55);
        U(SetLastErrorEx)(g_errs[i], 1); /* SLE_ERROR */
        CHECKF(GetLastError() == g_errs[i], "idx %u", i);
    }
}
TEST(gap5_sle_all_values_minor) {
    REQUIRE(U(SetLastErrorEx));
    for (unsigned i = 0; i < NERRS; i++) {
        SetLastError(0x55);
        U(SetLastErrorEx)(g_errs[i], 2); /* SLE_MINORERROR */
        CHECKF(GetLastError() == g_errs[i], "idx %u", i);
    }
}
TEST(gap5_sle_all_values_warning) {
    REQUIRE(U(SetLastErrorEx));
    for (unsigned i = 0; i < NERRS; i++) {
        SetLastError(0x55);
        U(SetLastErrorEx)(g_errs[i], 3); /* SLE_WARNING */
        CHECKF(GetLastError() == g_errs[i], "idx %u", i);
    }
}
TEST(gap5_sle_type_ignored_wild) {
    REQUIRE(U(SetLastErrorEx));
    static const DWORD types[] = { 4, 0x10, 0xff, 0x100, 0x7fffffff, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof types / sizeof types[0]; i++) {
        SetLastError(0);
        U(SetLastErrorEx)(4321, types[i]);
        CHECKF(GetLastError() == 4321, "type 0x%lx", types[i]);
    }
}
TEST(gap5_sle_zero_clears) {
    REQUIRE(U(SetLastErrorEx));
    SetLastError(999);
    U(SetLastErrorEx)(0, 1);
    EXPECT_EQ(GetLastError(), 0);
}
TEST(gap5_sle_matches_teb) {
    REQUIRE(U(SetLastErrorEx));
    U(SetLastErrorEx)(0x1357, 1);
    EXPECT_EQ(GetLastError(), 0x1357);
}
TEST(gap5_sle_repeated) {
    REQUIRE(U(SetLastErrorEx));
    for (DWORD i = 0; i < 5000; i++) {
        U(SetLastErrorEx)(i * 7 + 1, i & 3);
        if (GetLastError() != i * 7 + 1) { CHECKF(0, "iter %lu", i); break; }
    }
    CHECK(1);
}
static void sle_thread(void *p) {
    U(SetLastErrorEx)(777, 1);
    *(DWORD *)p = GetLastError();
}
TEST(gap5_sle_per_thread) {
    REQUIRE(U(SetLastErrorEx));
    DWORD other = 0;
    U(SetLastErrorEx)(555, 1);
    CHECK(g5_run_thread(sle_thread, &other));
    EXPECT_EQ(other, 777);
    EXPECT_EQ(GetLastError(), 555);
}
TEST(gap5_sle_vs_setlasterror_parity) {
    REQUIRE(U(SetLastErrorEx));
    for (unsigned i = 0; i < NERRS; i++) {
        U(SetLastErrorEx)(g_errs[i], 1);
        DWORD a = GetLastError();
        SetLastError(g_errs[i]);
        DWORD b = GetLastError();
        CHECKF(a == b, "idx %u", i);
    }
}
TEST(gap5_sle_preserved_by_getlasterror) {
    REQUIRE(U(SetLastErrorEx));
    U(SetLastErrorEx)(31337, 1);
    for (int i = 0; i < 10; i++) EXPECT_EQ(GetLastError(), 31337);
}

/* ---------------- SetMessageQueue / DdeXetQualityOfService alias ---------------- */
TEST(gap5_smq_returns_true) {
    REQUIRE(U(SetMessageQueue));
    static const int v[] = { 0, 1, 8, 50, 120, 1000, 10000, 0x7fffffff, -1, -100, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        BOOL r = U(SetMessageQueue)(v[i]);
        CHECKF(r == TRUE, "size %d ret %d", v[i], r);
    }
}
TEST(gap5_smq_preserves_lasterror) {
    REQUIRE(U(SetMessageQueue));
    SetLastError(0x4242);
    U(SetMessageQueue)(100);
    EXPECT_EQ(GetLastError(), 0x4242);
    SetLastError(0);
    U(SetMessageQueue)(-5);
    EXPECT_EQ(GetLastError(), 0);
}
TEST(gap5_smq_does_not_break_queue) {
    REQUIRE(U(SetMessageQueue)); REQUIRE(U(PostThreadMessageA)); REQUIRE(U(PeekMessageA));
    U(SetMessageQueue)(10);
    MSG m;
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE); /* make sure a queue exists */
    BOOL ok = U(PostThreadMessageA)(GetCurrentThreadId(), WM_USER + 77, 1, 2);
    CHECK(ok);
    BOOL got = U(PeekMessageA)(&m, NULL, WM_USER + 77, WM_USER + 77, PM_REMOVE);
    CHECK(got);
    if (got) { EXPECT_EQ(m.wParam, 1); EXPECT_EQ(m.lParam, 2); }
}
TEST(gap5_smq_alias_dde_qos) {
    REQUIRE(U(SetMessageQueue));
    FARPROC a = (FARPROC)U(SetMessageQueue);
    FARPROC g = t_proc("DdeGetQualityOfService"), s = t_proc("DdeSetQualityOfService");
    if (!g || !s) { t_skip("dde qos missing"); return; }
    /* In the analysed build all three exports share one code address. */
    CHECK(a == g);
    CHECK(a == s);
}
TEST(gap5_smq_dde_qos_return_true) {
    REQUIRE(UN(DdeGetQualityOfService)); REQUIRE(UN(DdeSetQualityOfService));
    SECURITY_QUALITY_OF_SERVICE q, p;
    memset(&q, 0, sizeof q); memset(&p, 0, sizeof p);
    SetLastError(0x77);
    EXPECT_EQ(UN(DdeGetQualityOfService)(BOGUS_HWND, &q, (void *)&p), TRUE);
    EXPECT_EQ(UN(DdeSetQualityOfService)(BOGUS_HWND, &q, (void *)&p), TRUE);
    EXPECT_EQ(GetLastError(), 0x77);
}

/* ---------------- TranslateMessageEx ---------------- */
static MSG mk(HWND h, UINT m, WPARAM w, LPARAM l) { MSG x; memset(&x, 0, sizeof x); x.hwnd = h; x.message = m; x.wParam = w; x.lParam = l; x.time = GetTickCount(); return x; }

static const UINT g_nonkey[] = { WM_NULL, WM_CREATE, WM_MOUSEMOVE, WM_LBUTTONDOWN, 0xff, WM_CHAR, WM_DEADCHAR,
    WM_SYSCHAR, WM_SYSDEADCHAR, 0x108, 0x109, 0x10f, 0x110, WM_USER, WM_USER + 100, WM_APP, 0xc000, 0xffff,
    0x10000, 0x1ffff };
#define NNONKEY (sizeof g_nonkey / sizeof g_nonkey[0])

TEST(gap5_tmex_nonkey_returns_zero) {
    REQUIRE(UN(TranslateMessageEx));
    for (unsigned i = 0; i < NNONKEY; i++) {
        MSG m = mk(NULL, g_nonkey[i], 'A', 0);
        BOOL r = UN(TranslateMessageEx)(&m, 0);
        CHECKF(r == 0, "msg 0x%x ret %d", g_nonkey[i], r);
    }
}
TEST(gap5_tmex_nonkey_no_error) {
    REQUIRE(UN(TranslateMessageEx));
    for (unsigned i = 0; i < NNONKEY; i++) {
        MSG m = mk(NULL, g_nonkey[i], 'A', 0);
        SetLastError(0x1234);
        UN(TranslateMessageEx)(&m, 0);
        CHECKF(GetLastError() == 0x1234, "msg 0x%x err %lu", g_nonkey[i], GetLastError());
    }
}
TEST(gap5_tmex_high_message_invalid_param) {
    REQUIRE(UN(TranslateMessageEx));
    static const UINT v[] = { 0x20000, 0x20001, 0x40000, 0x80000000, 0x12345678, 0xfffe0000, 0xffffffff };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        MSG m = mk(NULL, v[i], 0, 0);
        SetLastError(0);
        BOOL r = UN(TranslateMessageEx)(&m, 0);
        CHECKF(r == 0, "msg 0x%x", v[i]);
        CHECKF(GetLastError() == ERR_INVALID_PARAMETER, "msg 0x%x err %lu", v[i], GetLastError());
    }
}
TEST(gap5_tmex_high_message_flags_ignored) {
    REQUIRE(UN(TranslateMessageEx));
    static const UINT f[] = { 0, 1, 2, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        MSG m = mk(NULL, 0x30000, 0, 0);
        SetLastError(0);
        BOOL r = UN(TranslateMessageEx)(&m, f[i]);
        CHECKF(r == 0, "flags 0x%x", f[i]);
        CHECKF(GetLastError() == ERR_INVALID_PARAMETER, "flags 0x%x", f[i]);
    }
}
TEST(gap5_tmex_boundary_0xff_and_0x106) {
    REQUIRE(UN(TranslateMessageEx));
    for (UINT msg = 0xf8; msg <= 0x10f; msg++) {
        MSG m = mk(NULL, msg, VK_F5, 0);
        BOOL r = UN(TranslateMessageEx)(&m, 0);
        if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
            CHECKF(r != 0, "msg 0x%x should translate", msg);
        else
            CHECKF(r == 0, "msg 0x%x should not translate", msg);
    }
}
TEST(gap5_tmex_boundary_2_17_bits) {
    REQUIRE(UN(TranslateMessageEx));
    SetLastError(0);
    MSG a = mk(NULL, 0x1ffff, 0, 0);
    UN(TranslateMessageEx)(&a, 0);
    EXPECT_EQ(GetLastError(), 0);
    MSG b = mk(NULL, 0x20000, 0, 0);
    UN(TranslateMessageEx)(&b, 0);
    EXPECT_EQ(GetLastError(), ERR_INVALID_PARAMETER);
}
TEST(gap5_tmex_keydown_nonchar_true) {
    REQUIRE(UN(TranslateMessageEx)); REQUIRE(U(PeekMessageA));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0);
    CHECK(h != NULL);
    static const UINT km[] = { WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP };
    static const WORD vks[] = { VK_F5, VK_F6, VK_F11, VK_LEFT, VK_HOME, VK_INSERT };
    for (unsigned i = 0; i < 4; i++)
        for (unsigned j = 0; j < sizeof vks / sizeof vks[0]; j++) {
            MSG m = mk(h, km[i], vks[j], 1);
            BOOL r = UN(TranslateMessageEx)(&m, 0);
            CHECKF(r != 0, "msg 0x%x vk 0x%x", km[i], vks[j]);
        }
    MSG q;
    while (U(PeekMessageA)(&q, h, 0, 0, PM_REMOVE)) {}
    g5_destroy(h);
}
TEST(gap5_tmex_matches_translatemessage) {
    REQUIRE(UN(TranslateMessageEx)); REQUIRE(U(TranslateMessage)); REQUIRE(U(PeekMessageA));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0);
    CHECK(h != NULL);
    for (UINT msg = 0x100; msg <= 0x108; msg++) {
        MSG m = mk(h, msg, VK_F7, 1);
        BOOL a = UN(TranslateMessageEx)(&m, 0), b = U(TranslateMessage)(&m);
        CHECKF((a != 0) == (b != 0), "msg 0x%x ex=%d plain=%d", msg, a, b);
    }
    for (unsigned i = 0; i < NNONKEY; i++) {
        MSG m = mk(h, g_nonkey[i], 'A', 0);
        BOOL a = UN(TranslateMessageEx)(&m, 0), b = U(TranslateMessage)(&m);
        CHECKF((a != 0) == (b != 0), "msg 0x%x", g_nonkey[i]);
    }
    MSG q;
    while (U(PeekMessageA)(&q, h, 0, 0, PM_REMOVE)) {}
    g5_destroy(h);
}
TEST(gap5_tmex_char_posted_for_letter) {
    REQUIRE(UN(TranslateMessageEx)); REQUIRE(U(PeekMessageA));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0);
    CHECK(h != NULL);
    MSG m = mk(h, WM_KEYDOWN, 'A', 1 | (0x1e << 16));
    BOOL r = UN(TranslateMessageEx)(&m, 0);
    CHECK(r != 0);
    MSG q;
    BOOL got = U(PeekMessageA)(&q, h, WM_CHAR, WM_CHAR, PM_REMOVE);
    /* Depends on the keyboard layout and on async key state: observe only. */
    t_note("WM_CHAR posted for synthetic 'A' keydown: %d (char 0x%x)", got, got ? (unsigned)q.wParam : 0);
    while (U(PeekMessageA)(&q, h, 0, 0, PM_REMOVE)) {}
    g5_destroy(h);
}
TEST(gap5_tmex_repeated_calls_stable) {
    REQUIRE(UN(TranslateMessageEx));
    MSG m = mk(NULL, WM_NULL, 0, 0), k = mk(NULL, 0x30000, 0, 0);
    for (int i = 0; i < 2000; i++) {
        if (UN(TranslateMessageEx)(&m, 0) != 0) { CHECKF(0, "iter %d", i); return; }
        SetLastError(0);
        if (UN(TranslateMessageEx)(&k, 0) != 0 || GetLastError() != ERR_INVALID_PARAMETER) { CHECKF(0, "iter %d", i); return; }
    }
    CHECK(1);
}

/* ---------------- UnregisterPointerInputTarget(Ex) ---------------- */
TEST(gap5_upit_ex_always_not_supported) {
    REQUIRE(UN(UnregisterPointerInputTargetEx));
    static const DWORD t[] = { 0, 1, 2, 3, 4, 5, 6, 0x7fffffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SetLastError(0);
        BOOL r = UN(UnregisterPointerInputTargetEx)(NULL, t[i]);
        CHECKF(r == 0, "type %lu", t[i]);
        CHECKF(GetLastError() == ERR_NOT_SUPPORTED, "type %lu err %lu", t[i], GetLastError());
    }
}
TEST(gap5_upit_ex_ignores_hwnd) {
    REQUIRE(UN(UnregisterPointerInputTargetEx));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0);
    HWND hs[] = { NULL, BOGUS_HWND, h, HWND_BROADCAST, (HWND)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 5; i++) {
        SetLastError(0);
        BOOL r = UN(UnregisterPointerInputTargetEx)(hs[i], PT_TOUCH);
        CHECKF(r == 0, "hwnd idx %u", i);
        CHECKF(GetLastError() == ERR_NOT_SUPPORTED, "hwnd idx %u", i);
    }
    g5_destroy(h);
}
TEST(gap5_upit_ex_shares_stub_with_siblings) {
    REQUIRE(UN(UnregisterPointerInputTargetEx));
    FARPROC a = t_proc("UnregisterPointerInputTargetEx");
    FARPROC b = t_proc("RegisterPointerInputTargetEx"), c = t_proc("GetPointerFrameArrivalTimes");
    if (!b || !c) { t_skip("siblings missing"); return; }
    CHECK(a == b);
    CHECK(a == c);
}
TEST(gap5_upit_ex_sibling_register_not_supported) {
    REQUIRE(UN(RegisterPointerInputTargetEx));
    SetLastError(0);
    BOOL r = UN(RegisterPointerInputTargetEx)(BOGUS_HWND, PT_TOUCH, TRUE);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(GetLastError(), ERR_NOT_SUPPORTED);
}
TEST(gap5_upit_ex_sibling_arrival_times_not_supported) {
    REQUIRE(UN(GetPointerFrameArrivalTimes));
    UINT64 buf[4] = {0};
    SetLastError(0);
    BOOL r = UN(GetPointerFrameArrivalTimes)(1, 4, buf);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(GetLastError(), ERR_NOT_SUPPORTED);
    EXPECT_EQ(buf[0], 0);
}
TEST(gap5_upit_bogus_hwnd_fails) {
    REQUIRE(U(UnregisterPointerInputTarget));
    HWND hs[] = { NULL, BOGUS_HWND, (HWND)(LONG_PTR)-1, (HWND)(ULONG_PTR)1 };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0);
        BOOL r = U(UnregisterPointerInputTarget)(hs[i], PT_TOUCH);
        CHECKF(r == 0, "hwnd idx %u ret %d", i, r);
        t_note("UnregisterPointerInputTarget bogus hwnd %u err %lu", i, GetLastError());
    }
}
TEST(gap5_upit_invalid_type_fails) {
    REQUIRE(U(UnregisterPointerInputTarget));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0);
    CHECK(h != NULL);
    static const int t[] = { 0, 6, 7, 100, -1, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SetLastError(0);
        BOOL r = U(UnregisterPointerInputTarget)(h, (POINTER_INPUT_TYPE)t[i]);
        CHECKF(r == 0, "type %d ret %d", t[i], r);
        t_note("UnregisterPointerInputTarget type %d err %lu", t[i], GetLastError());
    }
    g5_destroy(h);
}
TEST(gap5_upit_not_registered_fails) {
    REQUIRE(U(UnregisterPointerInputTarget));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0);
    CHECK(h != NULL);
    for (int t = PT_POINTER; t <= PT_TOUCHPAD; t++) {
        SetLastError(0);
        BOOL r = U(UnregisterPointerInputTarget)(h, (POINTER_INPUT_TYPE)t);
        t_note("UnregisterPointerInputTarget unregistered type %d -> %d err %lu", t, r, GetLastError());
    }
    g5_destroy(h);
}
TEST(gap5_upit_destroyed_hwnd) {
    REQUIRE(U(UnregisterPointerInputTarget));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0);
    CHECK(h != NULL);
    g5_destroy(h);
    BOOL r = U(UnregisterPointerInputTarget)(h, PT_TOUCH);
    EXPECT_EQ(r, 0);
}

/* ---------------- SetProcessDpiAwarenessInternal ---------------- */
/* Valid values (0,1,2) would change this process' DPI awareness permanently
 * and are therefore never passed; only the parameter-validation path is run. */
TEST(gap5_spdai_invalid_values) {
    REQUIRE(UN(SetProcessDpiAwarenessInternal));
    static const int v[] = { 3, 4, 5, 7, 8, 16, 100, 255, 0x100, 0x7fffffff, -1, -2, -100, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        SetLastError(0);
        BOOL r = UN(SetProcessDpiAwarenessInternal)(v[i]);
        CHECKF(r == 0, "value %d ret %d", v[i], r);
        CHECKF(GetLastError() == ERR_INVALID_PARAMETER, "value %d err %lu", v[i], GetLastError());
    }
}
TEST(gap5_spdai_invalid_does_not_change_awareness) {
    REQUIRE(UN(SetProcessDpiAwarenessInternal)); REQUIRE(U(GetThreadDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT before = U(GetThreadDpiAwarenessContext)();
    UN(SetProcessDpiAwarenessInternal)(3);
    UN(SetProcessDpiAwarenessInternal)(-1);
    DPI_AWARENESS_CONTEXT after = U(GetThreadDpiAwarenessContext)();
    CHECK(before == after);
}
TEST(gap5_spdai_invalid_lasterror_overwritten) {
    REQUIRE(UN(SetProcessDpiAwarenessInternal));
    SetLastError(ERROR_SUCCESS);
    UN(SetProcessDpiAwarenessInternal)(9);
    EXPECT_EQ(GetLastError(), ERR_INVALID_PARAMETER);
    SetLastError(0x9999);
    UN(SetProcessDpiAwarenessInternal)(9);
    EXPECT_EQ(GetLastError(), ERR_INVALID_PARAMETER);
}

/* ---------------- SetThreadDpiHostingBehavior ---------------- */
static void dpih_main(void *p) {
    int *res = p; /* res[0]: failures */
    DPI_HOSTING_BEHAVIOR (WINAPI *set)(DPI_HOSTING_BEHAVIOR) = (void *)t_proc("SetThreadDpiHostingBehavior");
    DPI_HOSTING_BEHAVIOR (WINAPI *get)(void) = (void *)t_proc("GetThreadDpiHostingBehavior");
    BOOL (WINAPI *gui)(BOOL) = (void *)t_proc("IsGUIThread");
    if (!set || !get || !gui) { res[1] = 1; return; }
    gui(TRUE);
    res[2] = get();                                 /* default */
    res[3] = set(DPI_HOSTING_BEHAVIOR_MIXED);       /* returns previous */
    res[4] = get();
    res[5] = set(DPI_HOSTING_BEHAVIOR_MIXED);       /* same value: returns previous == MIXED */
    res[6] = set(DPI_HOSTING_BEHAVIOR_DEFAULT);
    res[7] = get();
    SetLastError(0);
    res[8] = set((DPI_HOSTING_BEHAVIOR)2);
    res[9] = GetLastError();
    res[10] = get();
    SetLastError(0);
    res[11] = set((DPI_HOSTING_BEHAVIOR)0x7fffffff);
    res[12] = GetLastError();
    SetLastError(0);
    res[13] = set(DPI_HOSTING_BEHAVIOR_INVALID);    /* -1 */
    res[14] = GetLastError();
    res[15] = get();
}
TEST(gap5_stdhb_state_machine_in_thread) {
    REQUIRE(U(SetThreadDpiHostingBehavior)); REQUIRE(U(GetThreadDpiHostingBehavior)); REQUIRE(U(IsGUIThread));
    int r[16] = {0};
    CHECK(g5_run_thread(dpih_main, r));
    if (r[1]) { t_skip("missing"); return; }
    EXPECT_EQ(r[2], 0);
    EXPECT_EQ(r[3], 0);
    EXPECT_EQ(r[4], 1);
    EXPECT_EQ(r[5], 1);
    EXPECT_EQ(r[6], 1);
    EXPECT_EQ(r[7], 0);
    EXPECT_EQ((DWORD)r[8], 0xffffffffu);
    EXPECT_EQ(r[9], ERR_INVALID_PARAMETER);
    EXPECT_EQ(r[10], 0);
    EXPECT_EQ((DWORD)r[11], 0xffffffffu);
    EXPECT_EQ(r[12], ERR_INVALID_PARAMETER);
    EXPECT_EQ((DWORD)r[13], 0xffffffffu);
    EXPECT_EQ(r[14], ERR_INVALID_PARAMETER);
    EXPECT_EQ(r[15], 0);
}
static void dpih_nongui(void *p) {
    int *res = p;
    DPI_HOSTING_BEHAVIOR (WINAPI *get)(void) = (void *)t_proc("GetThreadDpiHostingBehavior");
    DPI_HOSTING_BEHAVIOR (WINAPI *set)(DPI_HOSTING_BEHAVIOR) = (void *)t_proc("SetThreadDpiHostingBehavior");
    SetLastError(0);
    res[0] = get();
    res[1] = set(DPI_HOSTING_BEHAVIOR_MIXED);
    res[2] = GetLastError();
}
TEST(gap5_stdhb_nongui_thread) {
    REQUIRE(U(SetThreadDpiHostingBehavior)); REQUIRE(U(GetThreadDpiHostingBehavior));
    int r[3] = {0};
    CHECK(g5_run_thread(dpih_nongui, r));
    /* A thread that never touched win32k: observed only (queue is created lazily). */
    t_note("non-GUI thread: get=%d set=%d err=%d", r[0], r[1], r[2]);
}
static void dpih_invalid_first(void *p) {
    int *res = p;
    DPI_HOSTING_BEHAVIOR (WINAPI *set)(DPI_HOSTING_BEHAVIOR) = (void *)t_proc("SetThreadDpiHostingBehavior");
    BOOL (WINAPI *gui)(BOOL) = (void *)t_proc("IsGUIThread");
    gui(TRUE);
    static const int v[] = { 2, 3, 10, 0x100, 0x7fffffff, -1, -2, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        SetLastError(0);
        int rr = set((DPI_HOSTING_BEHAVIOR)v[i]);
        if (rr != -1 || GetLastError() != 87) res[0]++;
    }
}
TEST(gap5_stdhb_invalid_values_in_thread) {
    REQUIRE(U(SetThreadDpiHostingBehavior)); REQUIRE(U(IsGUIThread));
    int r[2] = {0};
    CHECK(g5_run_thread(dpih_invalid_first, r));
    EXPECT_EQ(r[0], 0);
}
static void dpih_isolated(void *p) {
    int *res = p;
    DPI_HOSTING_BEHAVIOR (WINAPI *set)(DPI_HOSTING_BEHAVIOR) = (void *)t_proc("SetThreadDpiHostingBehavior");
    BOOL (WINAPI *gui)(BOOL) = (void *)t_proc("IsGUIThread");
    gui(TRUE);
    set(DPI_HOSTING_BEHAVIOR_MIXED);
    *res = 1;
    Sleep(50);
}
TEST(gap5_stdhb_per_thread_isolated) {
    REQUIRE(U(SetThreadDpiHostingBehavior)); REQUIRE(U(GetThreadDpiHostingBehavior));
    DPI_HOSTING_BEHAVIOR here = U(GetThreadDpiHostingBehavior)();
    int r = 0;
    CHECK(g5_run_thread(dpih_isolated, &r));
    EXPECT_EQ(r, 1);
    CHECK(U(GetThreadDpiHostingBehavior)() == here);
}
TEST(gap5_stdhb_current_thread_roundtrip) {
    REQUIRE(U(SetThreadDpiHostingBehavior)); REQUIRE(U(GetThreadDpiHostingBehavior)); REQUIRE(U(IsGUIThread));
    U(IsGUIThread)(TRUE);
    DPI_HOSTING_BEHAVIOR orig = U(GetThreadDpiHostingBehavior)();
    DPI_HOSTING_BEHAVIOR other = orig == DPI_HOSTING_BEHAVIOR_MIXED ? DPI_HOSTING_BEHAVIOR_DEFAULT : DPI_HOSTING_BEHAVIOR_MIXED;
    DPI_HOSTING_BEHAVIOR prev = U(SetThreadDpiHostingBehavior)(other);
    CHECK(prev == orig);
    CHECK(U(GetThreadDpiHostingBehavior)() == other);
    prev = U(SetThreadDpiHostingBehavior)(orig);
    CHECK(prev == other);
    CHECK(U(GetThreadDpiHostingBehavior)() == orig);
}

/* ---------------- SetSysColorsTemp ---------------- */
TEST(gap5_sct_count_too_large) {
    REQUIRE(UN(SetSysColorsTemp)); REQUIRE(U(GetSysColor));
    COLORREF c[40]; for (int i = 0; i < 40; i++) c[i] = U(GetSysColor)(i % 31);
    static const UINT_PTR n[] = { 0x20, 0x21, 0x40, 0x100, 0xffff, 0x7fffffff, 0xffffffffULL, 0x100000000ULL };
    for (unsigned i = 0; i < sizeof n / sizeof n[0]; i++) {
        SetLastError(0);
        ULONG_PTR r = UN(SetSysColorsTemp)(c, NULL, n[i]);
        CHECKF(r == 0, "n=0x%llx", (unsigned long long)n[i]);
        CHECKF(GetLastError() == ERR_INVALID_PARAMETER, "n=0x%llx err %lu", (unsigned long long)n[i], GetLastError());
    }
}
TEST(gap5_sct_restore_without_temp) {
    REQUIRE(UN(SetSysColorsTemp));
    SetLastError(0);
    ULONG_PTR r = UN(SetSysColorsTemp)(NULL, NULL, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(GetLastError(), ERR_INVALID_PARAMETER);
    SetLastError(0);
    r = UN(SetSysColorsTemp)(NULL, NULL, 5);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(GetLastError(), ERR_INVALID_PARAMETER);
}
TEST(gap5_sct_temp_cycle_with_identical_colors) {
    REQUIRE(UN(SetSysColorsTemp)); REQUIRE(U(GetSysColor));
    COLORREF cur[8], snap[31];
    for (int i = 0; i < 31; i++) snap[i] = U(GetSysColor)(i);
    for (int i = 0; i < 8; i++) cur[i] = snap[i];
    SetLastError(0);
    ULONG_PTR r = UN(SetSysColorsTemp)(cur, NULL, 8);
    if (!r) { t_note("SetSysColorsTemp refused (err %lu): skipping cycle", GetLastError()); return; }
    const COLORREF *saved = (const COLORREF *)r;
    for (int i = 0; i < 8; i++) CHECKF(saved[i] == snap[i], "saved[%d]=%08lx want %08lx", i, saved[i], snap[i]);
    /* A second temp set while one is outstanding is refused. */
    SetLastError(0);
    ULONG_PTR r2 = UN(SetSysColorsTemp)(cur, NULL, 8);
    EXPECT_EQ(r2, 0);
    EXPECT_EQ(GetLastError(), ERR_ACCESS_DENIED);
    /* Count validation still happens first. */
    SetLastError(0);
    r2 = UN(SetSysColorsTemp)(cur, NULL, 0x20);
    EXPECT_EQ(r2, 0);
    EXPECT_EQ(GetLastError(), ERR_INVALID_PARAMETER);
    /* Restore. */
    SetLastError(0);
    r2 = UN(SetSysColorsTemp)(NULL, NULL, 0);
    EXPECT_EQ(r2, 1);
    for (int i = 0; i < 31; i++) CHECKF(U(GetSysColor)(i) == snap[i], "color %d changed", i);
    /* Restoring again fails. */
    SetLastError(0);
    r2 = UN(SetSysColorsTemp)(NULL, NULL, 0);
    EXPECT_EQ(r2, 0);
    EXPECT_EQ(GetLastError(), ERR_INVALID_PARAMETER);
}
TEST(gap5_sct_temp_cycle_twice) {
    REQUIRE(UN(SetSysColorsTemp)); REQUIRE(U(GetSysColor));
    COLORREF cur[3];
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < 3; i++) cur[i] = U(GetSysColor)(i);
        ULONG_PTR r = UN(SetSysColorsTemp)(cur, NULL, 3);
        if (!r) { t_note("refused round %d err %lu", k, GetLastError()); return; }
        CHECK(((COLORREF *)r)[0] == cur[0]);
        EXPECT_EQ(UN(SetSysColorsTemp)(NULL, NULL, 0), 1);
    }
}

/* ---------------- User32InitializeImmEntryTable ---------------- */
TEST(gap5_imm_table_wrong_magic) {
    REQUIRE(UN(User32InitializeImmEntryTable));
    static const DWORD v[] = { 0, 1, 0x19650411, 0x19650413, 0x12040565, 0x7fffffff, 0x80000000, 0xffffffff, 0x19650412 ^ 0x100 };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        ULONG_PTR r = UN(User32InitializeImmEntryTable)(v[i], 0);
        CHECKF(r == 0, "magic 0x%lx ret %llx", v[i], (unsigned long long)r);
    }
}
TEST(gap5_imm_table_wrong_magic_second_arg) {
    REQUIRE(UN(User32InitializeImmEntryTable));
    for (DWORD a2 = 0; a2 < 4; a2++) {
        ULONG_PTR r = UN(User32InitializeImmEntryTable)(0xdead, a2);
        CHECK(r == 0);
    }
}
TEST(gap5_imm_table_no_lasterror) {
    REQUIRE(UN(User32InitializeImmEntryTable));
    SetLastError(0x3131);
    UN(User32InitializeImmEntryTable)(5, 0);
    EXPECT_EQ(GetLastError(), 0x3131);
}
/* The valid magic 0x19650412 is deliberately never passed: it re-runs IMM setup. */

/* ---------------- UserRegisterWowHandlers ---------------- */
static ULONG_PTR wow_in[64], wow_out[64]; /* zeroed scratch tables, never read on x64 */
TEST(gap5_wow_handlers_unsupported_on_x64) {
    REQUIRE(UN(UserRegisterWowHandlers));
    /* NtVdmControl(0xe) fails on 64-bit Windows, so the tables are never touched. */
    ULONG_PTR r = UN(UserRegisterWowHandlers)(wow_in, wow_out);
    EXPECT_EQ(r, (ULONG_PTR)0xffffffffc0000022ULL);
}
TEST(gap5_wow_handlers_repeatable) {
    REQUIRE(UN(UserRegisterWowHandlers));
    for (int i = 0; i < 20; i++) {
        ULONG_PTR r = UN(UserRegisterWowHandlers)(wow_in, wow_out);
        if (r != (ULONG_PTR)0xffffffffc0000022ULL) { CHECKF(0, "iter %d ret %llx", i, (unsigned long long)r); return; }
    }
    CHECK(1);
}
TEST(gap5_wow_handlers_not_a_shared_info_ptr) {
    REQUIRE(UN(UserRegisterWowHandlers));
    ULONG_PTR r = UN(UserRegisterWowHandlers)(wow_in, wow_out);
    CHECK(r != 0);
    CHECK((r >> 32) == 0xffffffffu);
}
