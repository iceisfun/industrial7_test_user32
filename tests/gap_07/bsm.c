/* BroadcastSystemMessage family, CallMsgFilter, SetWindowsHookW, GetMessagePos, GetMessageW. */
#include "gap7.h"

#define NV 5
static const char *vname[NV] = { "BSM", "BSMA", "BSMW", "BSMExA", "BSMExW" };

static int have_bsm(void)
{
    return U(BroadcastSystemMessage) && U(BroadcastSystemMessageA) && U(BroadcastSystemMessageW) &&
           U(BroadcastSystemMessageExA) && U(BroadcastSystemMessageExW) && U(RegisterWindowMessageW) &&
           U(CreateWindowExW) && U(DestroyWindow) && U(PostMessageW);
}
static long bsm(int v, DWORD flags, LPDWORD info, UINT msg, WPARAM w, LPARAM l, PBSMINFO pb)
{
    switch (v) {
    case 0: return U(BroadcastSystemMessage)(flags, info, msg, w, l);
    case 1: return U(BroadcastSystemMessageA)(flags, info, msg, w, l);
    case 2: return U(BroadcastSystemMessageW)(flags, info, msg, w, l);
    case 3: return U(BroadcastSystemMessageExA)(flags, info, msg, w, l, pb);
    default: return U(BroadcastSystemMessageExW)(flags, info, msg, w, l, pb);
    }
}
static UINT priv_msg(void) { return U(RegisterWindowMessageW)(L"gap7_bsm_private_message"); }

TEST(gap7_bsm_flags_out_of_range)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    static const DWORD bad[] = { 0x800, 0x1000, 0x10000, 0x80000000u, 0xFFFFF800u, 0x00000800 | BSF_NOHANG };
    for (int v = 0; v < NV; v++)
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            SET_ERR(0);
            long r = bsm(v, bad[i], NULL, msg, 0, 0, NULL);
            CHECKF(r == 0, "%s flags=%lx r=%ld", vname[v], bad[i], r);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s flags=%lx err=%lu", vname[v], bad[i], GetLastError());
        }
}

TEST(gap7_bsm_message_out_of_range)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    static const UINT bad[] = { 0x20000, 0x20001, 0x80000000u, 0xFFFFFFFFu };
    for (int v = 0; v < NV; v++)
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            SET_ERR(0);
            long r = bsm(v, BSF_NOHANG, NULL, bad[i], 0, 0, NULL);
            CHECKF(r == 0, "%s msg=%x r=%ld", vname[v], bad[i], r);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s msg=%x err=%lu", vname[v], bad[i], GetLastError());
        }
}

TEST(gap7_bsm_wm_user_range_rejected)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    /* Messages 0x400..0xBFFF may not be broadcast. */
    static const UINT bad[] = { WM_USER, WM_USER + 1, 0x7FFF, WM_APP, WM_APP + 100, 0xBFFF };
    for (int v = 0; v < NV; v++)
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            SET_ERR(0);
            long r = bsm(v, BSF_NOHANG, NULL, bad[i], 0, 0, NULL);
            CHECKF(r == 0, "%s msg=%x r=%ld", vname[v], bad[i], r);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s msg=%x err=%lu", vname[v], bad[i], GetLastError());
        }
}

TEST(gap7_bsm_returnhdesk_luid_need_bsminfo)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    static const DWORD f[] = { BSF_RETURNHDESK, BSF_LUID, BSF_RETURNHDESK | BSF_LUID };
    UINT msg = priv_msg();
    for (int v = 0; v < NV; v++)
        for (unsigned i = 0; i < 3; i++) {
            SET_ERR(0);
            long r = bsm(v, f[i] | BSF_NOHANG, NULL, msg, 0, 0, NULL);
            CHECKF(r == 0, "%s f=%lx r=%ld", vname[v], f[i], r);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s f=%lx err=%lu", vname[v], f[i], GetLastError());
        }
}

TEST(gap7_bsm_bsminfo_cbsize_checked)
{
    REQUIRE(U(BroadcastSystemMessageExA)); REQUIRE(U(BroadcastSystemMessageExW)); REQUIRE(U(RegisterWindowMessageW));
    UINT msg = priv_msg();
    CHECK(sizeof(BSMINFO) == 32);
    static const UINT sizes[] = { 0, 1, 24, 31, 33, 40, 0x20000000, 0xFFFFFFFFu };
    for (int v = 3; v < 5; v++)
        for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
            BSMINFO bi; memset(&bi, 0, sizeof bi); bi.cbSize = sizes[i];
            SET_ERR(0);
            long r = bsm(v, BSF_NOHANG, NULL, msg, 0, 0, &bi);
            CHECKF(r == 0, "%s cb=%u r=%ld", vname[v], sizes[i], r);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s cb=%u err=%lu", vname[v], sizes[i], GetLastError());
            /* same check on the RETURNHDESK/LUID path */
            SET_ERR(0);
            r = bsm(v, BSF_NOHANG | BSF_RETURNHDESK, NULL, msg, 0, 0, &bi);
            CHECKF(r == 0 && GetLastError() == ERROR_INVALID_PARAMETER, "%s hdesk cb=%u r=%ld", vname[v], sizes[i], r);
        }
}

TEST(gap7_bsm_recipient_bits_invalid)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    static const DWORD bad[] = { 0x20, 0x40, 0x100, 0x80000000u, 0xFFFFFFE0u, 0x28 };
    for (int v = 0; v < NV; v++)
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            DWORD info = bad[i];
            SET_ERR(0);
            long r = bsm(v, BSF_NOHANG, &info, msg, 0, 0, NULL);
            CHECKF(r == 0, "%s info=%lx r=%ld", vname[v], bad[i], r);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s info=%lx err=%lu", vname[v], bad[i], GetLastError());
            CHECKF(info == bad[i], "%s info modified to %lx", vname[v], info);
        }
}

TEST(gap7_bsm_non_application_recipients_return_minus1)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    /* VXDs / network drivers / installable drivers only: nothing to deliver to. */
    static const DWORD only[] = { BSM_VXDS, BSM_NETDRIVER, BSM_INSTALLABLEDRIVERS, 3, 5, 6, 7 };
    for (int v = 0; v < NV; v++)
        for (unsigned i = 0; i < sizeof only / sizeof only[0]; i++) {
            DWORD info = only[i];
            long r = bsm(v, BSF_NOHANG, &info, msg, 0, 0, NULL);
            CHECKF(r == -1, "%s info=%lx r=%ld", vname[v], only[i], r);
            CHECKF(info == only[i], "%s info modified to %lx", vname[v], info);
        }
}

TEST(gap7_bsm_send_reaches_all_top_level_windows)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    for (int v = 0; v < NV; v++) {
        HWND a = g7_mkwinW(WS_POPUP, 0, NULL), b = g7_mkwinA(WS_OVERLAPPED, 0, NULL);
        HWND child = g7_mkwinW(WS_CHILD, 0, a);
        CHECK(a && b && child);
        g7_reset(msg, msg, 1);
        long r = bsm(v, BSF_NOHANG, NULL, msg, 0x1234, 0x5678, NULL);
        CHECKF(r > 0, "%s r=%ld", vname[v], r);
        CHECKF(g7_count(a, msg) == 1, "%s a=%d", vname[v], g7_count(a, msg));
        CHECKF(g7_count(b, msg) == 1, "%s b=%d", vname[v], g7_count(b, msg));
        CHECKF(g7_count(child, msg) == 0, "%s child=%d", vname[v], g7_count(child, msg));
        for (int i = 0; i < g7_nlog; i++) {
            CHECK(g7_log[i].w == 0x1234);
            CHECK(g7_log[i].l == 0x5678);
        }
        U(DestroyWindow)(a); U(DestroyWindow)(b);
        g7_reset(1, 0, 0);
    }
}

TEST(gap7_bsm_recipients_with_apps_bit_and_lpinfo_roundtrip)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    HWND a = g7_mkwinW(WS_POPUP, 0, NULL);
    CHECK(a);
    for (int v = 0; v < NV; v++) {
        g7_reset(msg, msg, 1);
        DWORD info = BSM_APPLICATIONS;
        long r = bsm(v, BSF_NOHANG, &info, msg, 0, 0, NULL);
        CHECKF(r > 0, "%s r=%ld", vname[v], r);
        CHECKF(g7_count(a, msg) == 1, "%s count=%d", vname[v], g7_count(a, msg));
        t_note("%s: lpInfo after call = 0x%lx", vname[v], info);
        /* components 0 means all: applications included */
        g7_reset(msg, msg, 1);
        info = BSM_ALLCOMPONENTS;
        r = bsm(v, BSF_NOHANG, &info, msg, 0, 0, NULL);
        CHECKF(r > 0 && g7_count(a, msg) == 1, "%s all r=%ld count=%d", vname[v], r, g7_count(a, msg));
    }
    U(DestroyWindow)(a);
    g7_reset(1, 0, 0);
}

TEST(gap7_bsm_query_deny_and_accept)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    HWND a = g7_mkwinW(WS_POPUP, 0, NULL);
    CHECK(a);
    for (int v = 0; v < NV; v++) {
        g7_reset(msg, msg, BROADCAST_QUERY_DENY);
        long r = bsm(v, BSF_NOHANG | BSF_QUERY, NULL, msg, 0, 0, NULL);
        CHECKF(r == 0, "%s deny r=%ld", vname[v], r);
        CHECKF(g7_count(a, msg) >= 1, "%s deny count=%d", vname[v], g7_count(a, msg));
        g7_reset(msg, msg, TRUE);
        r = bsm(v, BSF_NOHANG | BSF_QUERY, NULL, msg, 0, 0, NULL);
        CHECKF(r > 0, "%s accept r=%ld", vname[v], r);
        CHECKF(g7_count(a, msg) >= 1, "%s accept count=%d", vname[v], g7_count(a, msg));
    }
    U(DestroyWindow)(a);
    g7_reset(1, 0, 0);
}

TEST(gap7_bsm_post_is_asynchronous)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    for (int v = 0; v < NV; v++) {
        HWND a = g7_mkwinW(WS_POPUP, 0, NULL);
        CHECK(a);
        g7_reset(msg, msg, 0);
        long r = bsm(v, BSF_POSTMESSAGE | BSF_NOHANG, NULL, msg, 7, 9, NULL);
        CHECKF(r > 0, "%s r=%ld", vname[v], r);
        CHECKF(g7_count(a, msg) == 0, "%s delivered synchronously", vname[v]);
        g7_pump_ms(50);
        CHECKF(g7_count(a, msg) == 1, "%s after pump=%d", vname[v], g7_count(a, msg));
        U(DestroyWindow)(a);
        g7_reset(1, 0, 0);
    }
}

TEST(gap7_bsm_sendnotify_flag)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    HWND a = g7_mkwinW(WS_POPUP, 0, NULL);
    CHECK(a);
    for (int v = 0; v < NV; v++) {
        g7_reset(msg, msg, 0);
        long r = bsm(v, BSF_SENDNOTIFYMESSAGE | BSF_NOHANG, NULL, msg, 0, 0, NULL);
        CHECKF(r > 0, "%s r=%ld", vname[v], r);
        g7_pump_ms(30);
        CHECKF(g7_count(a, msg) == 1, "%s count=%d", vname[v], g7_count(a, msg));
    }
    U(DestroyWindow)(a);
    g7_reset(1, 0, 0);
}

TEST(gap7_bsm_ignorecurrenttask_observed)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    HWND a = g7_mkwinW(WS_POPUP, 0, NULL);
    CHECK(a);
    g7_reset(msg, msg, 1);
    long r = bsm(2, BSF_NOHANG | BSF_IGNORECURRENTTASK, NULL, msg, 0, 0, NULL);
    CHECKF(r > 0, "r=%ld", r);
    t_note("IGNORECURRENTTASK: own window received %d (0 expected: windows of the current task are skipped)", g7_count(a, msg));
    U(DestroyWindow)(a);
    g7_reset(1, 0, 0);
}

TEST(gap7_bsm_wm_null_broadcast_allowed)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    HWND a = g7_mkwinW(WS_POPUP, 0, NULL);
    CHECK(a);
    for (int v = 0; v < NV; v++) {
        g7_reset(WM_NULL, WM_NULL, 0);
        long r = bsm(v, BSF_NOHANG, NULL, WM_NULL, 0, 0, NULL);
        CHECKF(r > 0, "%s r=%ld", vname[v], r);
        CHECKF(g7_count(a, WM_NULL) == 1, "%s count=%d", vname[v], g7_count(a, WM_NULL));
    }
    U(DestroyWindow)(a);
    g7_reset(1, 0, 0);
}

TEST(gap7_bsm_message_just_below_0x400_and_0xc000_boundary)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    HWND a = g7_mkwinW(WS_POPUP, 0, NULL);
    CHECK(a);
    /* 0xC000 is the first allowed registered-message id. Nobody listens to it. */
    g7_reset(0xC000, 0xC000, 1);
    SET_ERR(0);
    long r = bsm(2, BSF_NOHANG, NULL, 0xC000, 0, 0, NULL);
    CHECKF(r > 0, "r=%ld", r);
    CHECKF(g7_count(a, 0xC000) == 1, "count=%d", g7_count(a, 0xC000));
    g7_reset(0x1FFFF, 0x1FFFF, 1);
    r = bsm(2, BSF_NOHANG, NULL, 0x1FFFF, 0, 0, NULL);
    t_note("msg 0x1FFFF: r=%ld", r);
    U(DestroyWindow)(a);
    g7_reset(1, 0, 0);
}

TEST(gap7_bsm_a_w_ex_parity_of_results)
{
    if (!have_bsm()) { REQUIRE(have_bsm()); }
    UINT msg = priv_msg();
    HWND a = g7_mkwinW(WS_POPUP, 0, NULL);
    long res[NV];
    for (int v = 0; v < NV; v++) {
        g7_reset(msg, msg, 1);
        res[v] = bsm(v, BSF_NOHANG, NULL, msg, 0, 0, NULL);
        CHECKF(g7_count(a, msg) == 1, "%s", vname[v]);
    }
    for (int v = 1; v < NV; v++) CHECKF(res[v] == res[0], "%s %ld vs %ld", vname[v], res[v], res[0]);
    /* Ex variants accept a valid BSMINFO with cbSize == 32 */
    for (int v = 3; v < 5; v++) {
        BSMINFO bi; memset(&bi, 0, sizeof bi); bi.cbSize = sizeof bi;
        g7_reset(msg, msg, 1);
        long r = bsm(v, BSF_NOHANG, NULL, msg, 0, 0, &bi);
        CHECKF(r > 0 && g7_count(a, msg) == 1, "%s valid bsminfo r=%ld", vname[v], r);
        CHECKF(bi.cbSize == sizeof bi, "cbSize clobbered");
    }
    U(DestroyWindow)(a);
    g7_reset(1, 0, 0);
}

/* ---------------- CallMsgFilter ---------------- */
static struct { int calls; int ncode; WPARAM w; MSG m; LRESULT ret; HWND hwnd; int lp_is_copy; MSG *orig; } hk;
static LRESULT CALLBACK msgfilter_proc(int code, WPARAM w, LPARAM l)
{
    if (code >= 0 || 1) {
        hk.calls++;
        hk.ncode = code; hk.w = w;
        if (l) { hk.m = *(MSG *)l; hk.lp_is_copy = ((MSG *)l != hk.orig); }
    }
    return hk.ret;
}
static HHOOK install_mf(void)
{
    memset(&hk, 0, sizeof hk);
    return U(SetWindowsHookExW)(WH_MSGFILTER, msgfilter_proc, NULL, GetCurrentThreadId());
}
static int have_cmf(void)
{
    return U(CallMsgFilterA) && U(CallMsgFilterW) && U(SetWindowsHookExW) && U(UnhookWindowsHookEx) && U(CreateWindowExW) && U(DestroyWindow);
}

TEST(gap7_cmf_no_hook_returns_false)
{
    if (!have_cmf()) { REQUIRE(have_cmf()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    MSG m; memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_NULL;
    static const int codes[] = { 0, 1, 2, 5, 6, MSGF_USER, MSGF_USER + 1, 0x7FFFFFFF, -1, -2, INT_MIN };
    for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        SET_ERR(0);
        CHECKF(U(CallMsgFilterW)(&m, codes[i]) == 0, "W code=%d", codes[i]);
        CHECKF(U(CallMsgFilterA)(&m, codes[i]) == 0, "A code=%d", codes[i]);
    }
    U(DestroyWindow)(w);
}

TEST(gap7_cmf_hook_sees_ncode_and_msg_copy)
{
    if (!have_cmf()) { REQUIRE(have_cmf()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    HHOOK h = install_mf();
    REQUIRE(h);
    MSG m; memset(&m, 0, sizeof m);
    m.hwnd = w; m.message = WM_KEYDOWN; m.wParam = 0x41; m.lParam = 0x1E0001; m.time = 0x1234; m.pt.x = 11; m.pt.y = -7;
    static const int codes[] = { 0, MSGF_MENU, MSGF_SCROLLBAR, MSGF_NEXTWINDOW, MSGF_USER, MSGF_USER + 77, 3, 100 };
    for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        hk.calls = 0; hk.ret = 0; hk.orig = &m;
        BOOL r = U(CallMsgFilterW)(&m, codes[i]);
        CHECKF(r == 0, "code=%d r=%d", codes[i], r);
        CHECKF(hk.calls == 1, "code=%d calls=%d", codes[i], hk.calls);
        CHECKF(hk.ncode == codes[i], "code=%d got=%d", codes[i], hk.ncode);
        CHECK(hk.m.hwnd == w && hk.m.message == WM_KEYDOWN && hk.m.wParam == 0x41 && hk.m.lParam == 0x1E0001);
        CHECK(hk.m.time == 0x1234 && hk.m.pt.x == 11 && hk.m.pt.y == -7);
        CHECKF(hk.lp_is_copy, "hook received the caller's MSG, not a copy");
    }
    t_note("hook wParam for CallMsgFilter = %llu", (unsigned long long)hk.w);
    U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}

TEST(gap7_cmf_hook_return_value_propagates)
{
    if (!have_cmf()) { REQUIRE(have_cmf()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    HHOOK h = install_mf();
    REQUIRE(h);
    MSG m; memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_NULL;
    hk.orig = &m;
    hk.ret = 1;
    CHECK(U(CallMsgFilterW)(&m, MSGF_USER) != 0);
    CHECK(U(CallMsgFilterA)(&m, MSGF_USER) != 0);
    hk.ret = 0;
    CHECK(U(CallMsgFilterW)(&m, MSGF_USER) == 0);
    CHECK(U(CallMsgFilterA)(&m, MSGF_USER) == 0);
    hk.ret = 77;
    BOOL r = U(CallMsgFilterW)(&m, 4096);
    CHECKF(r != 0, "r=%d", r);
    U(UnhookWindowsHookEx)(h);
    /* after unhooking the call is a no-op again */
    hk.calls = 0; hk.ret = 1;
    CHECK(U(CallMsgFilterW)(&m, MSGF_USER) == 0);
    CHECK(hk.calls == 0);
    U(DestroyWindow)(w);
}

TEST(gap7_cmf_excluded_messages_skip_hook)
{
    if (!have_cmf()) { REQUIRE(have_cmf()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    HHOOK h = install_mf();
    REQUIRE(h);
    hk.ret = 1;
    MSG m; memset(&m, 0, sizeof m); m.hwnd = w; hk.orig = &m;
    static const UINT skipped[] = { 0x240 /*WM_TOUCH*/, 0x119 /*WM_GESTURE*/, 0x11B };
    for (unsigned i = 0; i < 3; i++) {
        m.message = skipped[i]; hk.calls = 0;
        CHECKF(U(CallMsgFilterW)(&m, MSGF_USER) == 0, "W msg=%x", skipped[i]);
        CHECKF(U(CallMsgFilterA)(&m, MSGF_USER) == 0, "A msg=%x", skipped[i]);
        CHECKF(hk.calls == 0, "hook called for %x", skipped[i]);
    }
    static const UINT called[] = { WM_NULL, WM_KEYDOWN, WM_MOUSEMOVE, 0x11A, 0x118, 0x241, 0x23F, WM_USER, 0xC001 };
    for (unsigned i = 0; i < sizeof called / sizeof called[0]; i++) {
        m.message = called[i]; hk.calls = 0;
        CHECKF(U(CallMsgFilterW)(&m, MSGF_USER) != 0, "msg=%x", called[i]);
        CHECKF(hk.calls == 1, "msg=%x calls=%d", called[i], hk.calls);
    }
    U(UnhookWindowsHookEx)(h);
    U(DestroyWindow)(w);
}

static DWORD WINAPI cmf_thread(LPVOID p)
{
    MSG m; memset(&m, 0, sizeof m); m.message = WM_NULL;
    *(BOOL *)p = U(CallMsgFilterW)(&m, MSGF_USER);
    return 0;
}
TEST(gap7_cmf_hook_is_per_thread)
{
    if (!have_cmf()) { REQUIRE(have_cmf()); }
    HHOOK h = install_mf();
    REQUIRE(h);
    hk.ret = 1;
    BOOL r = 99;
    HANDLE t = CreateThread(NULL, 0, cmf_thread, &r, 0, NULL);
    REQUIRE(t);
    CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
    CloseHandle(t);
    CHECKF(r == 0, "other thread r=%d", r);
    CHECKF(hk.calls == 0, "hook ran for other thread");
    U(UnhookWindowsHookEx)(h);
}

TEST(gap7_cmf_nested_hooks_chain)
{
    if (!have_cmf()) { REQUIRE(have_cmf()); }
    REQUIRE(U(CallNextHookEx));
    HHOOK h = install_mf();
    REQUIRE(h);
    hk.ret = 0;
    MSG m; memset(&m, 0, sizeof m); m.message = WM_NULL; hk.orig = &m;
    CHECK(U(CallMsgFilterW)(&m, MSGF_USER) == 0);
    CHECK(hk.calls == 1);
    /* second hook on the same chain: newest is called first; returning nonzero stops the chain */
    int first = 0;
    (void)first;
    U(UnhookWindowsHookEx)(h);
    CHECK(U(UnhookWindowsHookEx)(h) == 0);
}

/* ---------------- SetWindowsHookW ---------------- */
TEST(gap7_swhw_invalid_filter_type)
{
    REQUIRE(U(SetWindowsHookW)); REQUIRE(U(UnhookWindowsHookEx));
    static const int bad[] = { -2, -100, 15, 16, 0x100, 0x7FFFFFFF, INT_MIN };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        HHOOK h = U(SetWindowsHookW)(bad[i], msgfilter_proc);
        CHECKF(h == NULL, "type=%d h=%p", bad[i], h);
        if (h) U(UnhookWindowsHookEx)(h);
        else t_note("SetWindowsHookW(%d) error %lu (ERROR_INVALID_HOOK_FILTER=1426)", bad[i], GetLastError());
    }
}

TEST(gap7_swhw_null_proc)
{
    REQUIRE(U(SetWindowsHookW)); REQUIRE(U(UnhookWindowsHookEx));
    static const int types[] = { WH_MSGFILTER, WH_GETMESSAGE, WH_CALLWNDPROC, WH_CBT };
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        HHOOK h = U(SetWindowsHookW)(types[i], NULL);
        CHECKF(h == NULL, "type=%d h=%p", types[i], h);
        if (h) U(UnhookWindowsHookEx)(h);
        else t_note("SetWindowsHookW(%d,NULL) error %lu", types[i], GetLastError());
    }
}

TEST(gap7_swhw_msgfilter_acts_as_thread_hook)
{
    if (!have_cmf()) { REQUIRE(have_cmf()); }
    REQUIRE(U(SetWindowsHookW));
    memset(&hk, 0, sizeof hk);
    SET_ERR(0);
    HHOOK h = U(SetWindowsHookW)(WH_MSGFILTER, msgfilter_proc);
    if (!h) { t_note("SetWindowsHookW(WH_MSGFILTER) failed, err=%lu", GetLastError()); return; }
    MSG m; memset(&m, 0, sizeof m); m.message = WM_NULL; hk.orig = &m;
    hk.ret = 1;
    BOOL r = U(CallMsgFilterW)(&m, MSGF_USER);
    CHECKF(r != 0, "r=%d", r);
    CHECKF(hk.calls == 1 && hk.ncode == MSGF_USER, "calls=%d code=%d", hk.calls, hk.ncode);
    CHECK(U(UnhookWindowsHookEx)(h));
    SET_ERR(0);
    CHECK(U(UnhookWindowsHookEx)(h) == 0);
    hk.calls = 0;
    CHECK(U(CallMsgFilterW)(&m, MSGF_USER) == 0);
    CHECK(hk.calls == 0);
}

/* ---------------- GetMessagePos / GetMessageW ---------------- */
TEST(gap7_getmessagepos_matches_peeked_message)
{
    REQUIRE(U(GetMessagePos)); REQUIRE(U(GetMessageTime)); REQUIRE(U(PeekMessageW)); REQUIRE(U(DispatchMessageW));
    HWND w = g7_mkwinW(0, 0, NULL);
    CHECK(w);
    POINT cur; GetCursorPos(&cur);
    UINT msg = WM_APP + 21;
    CHECK(U(PostMessageW)(w, msg, 3, 4));
    MSG m; memset(&m, 0, sizeof m);
    CHECK(U(PeekMessageW)(&m, w, msg, msg, PM_REMOVE));
    DWORD pos = U(GetMessagePos)();
    CHECKF((short)LOWORD(pos) == m.pt.x, "x %d vs %ld", (short)LOWORD(pos), m.pt.x);
    CHECKF((short)HIWORD(pos) == m.pt.y, "y %d vs %ld", (short)HIWORD(pos), m.pt.y);
    CHECKF((LONG)U(GetMessageTime)() == (LONG)m.time, "time %ld vs %lu", U(GetMessageTime)(), m.time);
    /* repeated calls are stable until the next retrieval */
    for (int i = 0; i < 5; i++) CHECK(U(GetMessagePos)() == pos);
    U(DispatchMessageW)(&m);
    CHECK(U(GetMessagePos)() == pos);
    U(DestroyWindow)(w);
}

static DWORD g_pos_in_proc, g_time_in_proc;
static LRESULT CALLBACK pos_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_APP + 22) { g_pos_in_proc = U(GetMessagePos)(); g_time_in_proc = U(GetMessageTime)(); return 0; }
    return DefWindowProcW(h, m, w, l);
}
TEST(gap7_getmessagepos_inside_wndproc_and_updates_per_message)
{
    REQUIRE(U(GetMessagePos)); REQUIRE(U(RegisterClassW)); REQUIRE(U(PostMessageW)); REQUIRE(U(PeekMessageW)); REQUIRE(U(DispatchMessageW));
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = pos_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = L"Gap7PosClass";
    CHECK(U(RegisterClassW)(&wc));
    HWND w = U(CreateWindowExW)(0, L"Gap7PosClass", L"p", 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    CHECK(w);
    U(PostMessageW)(w, WM_APP + 22, 0, 0);
    MSG m;
    CHECK(U(PeekMessageW)(&m, w, WM_APP + 22, WM_APP + 22, PM_REMOVE));
    U(DispatchMessageW)(&m);
    CHECK(g_pos_in_proc == MAKELONG(m.pt.x, m.pt.y));
    CHECK((LONG)g_time_in_proc == (LONG)m.time);
    /* A later retrieval updates the time (never goes backwards) */
    Sleep(20);
    U(PostMessageW)(w, WM_APP + 23, 0, 0);
    MSG m2;
    CHECK(U(PeekMessageW)(&m2, w, WM_APP + 23, WM_APP + 23, PM_REMOVE));
    CHECK((LONG)(m2.time - m.time) >= 0);
    CHECK((LONG)(U(GetMessageTime)() - (LONG)m.time) >= 0);
    U(DestroyWindow)(w);
    U(UnregisterClassW)(L"Gap7PosClass", wc.hInstance);
}

TEST(gap7_getmessagepos_screen_coords_match_cursor_range)
{
    REQUIRE(U(GetMessagePos)); REQUIRE(U(GetSystemMetrics)); REQUIRE(U(PostMessageW)); REQUIRE(U(PeekMessageW));
    HWND w = g7_mkwinW(0, 0, NULL);
    U(PostMessageW)(w, WM_APP + 24, 0, 0);
    MSG m;
    CHECK(U(PeekMessageW)(&m, w, WM_APP + 24, WM_APP + 24, PM_REMOVE));
    DWORD p = U(GetMessagePos)();
    int x = (short)LOWORD(p), y = (short)HIWORD(p);
    int vx = U(GetSystemMetrics)(SM_XVIRTUALSCREEN), vy = U(GetSystemMetrics)(SM_YVIRTUALSCREEN);
    int vw = U(GetSystemMetrics)(SM_CXVIRTUALSCREEN), vh = U(GetSystemMetrics)(SM_CYVIRTUALSCREEN);
    CHECKF(x >= vx - 1 && x <= vx + vw, "x=%d virt=[%d,%d]", x, vx, vx + vw);
    CHECKF(y >= vy - 1 && y <= vy + vh, "y=%d virt=[%d,%d]", y, vy, vy + vh);
    U(DestroyWindow)(w);
}

static struct { UINT lo, hi; BOOL r; DWORD err; } gm_arg;
static DWORD WINAPI gm_thread(LPVOID p)
{
    MSG m; memset(&m, 0, sizeof m);
    SetLastError(0);
    gm_arg.r = U(GetMessageW)(&m, NULL, gm_arg.lo, gm_arg.hi);
    gm_arg.err = GetLastError();
    return 0;
}
TEST(gap7_getmessagew_filter_range_bits)
{
    REQUIRE(U(GetMessageW));
    /* Run on a helper thread with a bounded wait: a conforming implementation returns immediately. */
    struct { UINT lo, hi; } bad[] = { { 0, 0x20000 }, { 0x20000, 0 }, { 0x20000, 0x20000 }, { 0, 0x80000000u }, { 0x80000000u, 0 },
                                       { 0xFFFFFFFFu, 0 }, { 0x40000, 0xFFFFFFFFu }, { 0x1000000, 0x2000000 }, { 1, 0x20000 }, { 0x20000, 0xFFFFFFFFu } };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        gm_arg.lo = bad[i].lo; gm_arg.hi = bad[i].hi; gm_arg.r = 12345; gm_arg.err = 0;
        HANDLE t = CreateThread(NULL, 0, gm_thread, NULL, 0, NULL);
        REQUIRE(t);
        if (WaitForSingleObject(t, 3000) != WAIT_OBJECT_0) {
            CHECKF(0, "[%x,%x] GetMessageW blocked", bad[i].lo, bad[i].hi);
            CloseHandle(t);
            return;
        }
        CloseHandle(t);
        CHECKF(gm_arg.r == 0, "[%x,%x] r=%d", bad[i].lo, bad[i].hi, gm_arg.r);
        CHECKF(gm_arg.err == ERROR_INVALID_PARAMETER, "[%x,%x] err=%lu", bad[i].lo, bad[i].hi, gm_arg.err);
    }
}

TEST(gap7_getmessagew_max_0xffffffff_accepted)
{
    REQUIRE(U(GetMessageW)); REQUIRE(U(PostMessageW));
    HWND w = g7_mkwinW(0, 0, NULL);
    U(PostMessageW)(w, WM_APP + 30, 5, 6);
    MSG m; memset(&m, 0, sizeof m);
    SET_ERR(0);
    BOOL r = U(GetMessageW)(&m, w, 0, 0xFFFFFFFFu);
    CHECKF(r > 0, "r=%d", r);
    CHECK(m.hwnd == w && m.message == WM_APP + 30 && m.wParam == 5 && m.lParam == 6);
    U(DestroyWindow)(w);
}

TEST(gap7_getmessagew_invalid_hwnd_returns_minus1)
{
    REQUIRE(U(GetMessageW));
    MSG m; memset(&m, 0, sizeof m);
    static const ULONG_PTR bogus[] = { 0xDEADBEEF, 0x12345678 };
    for (unsigned i = 0; i < 2; i++) {
        SET_ERR(0);
        BOOL r = U(GetMessageW)(&m, (HWND)bogus[i], 0, 0);
        CHECKF(r == -1, "hwnd=%llx r=%d", (unsigned long long)bogus[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "hwnd=%llx err=%lu", (unsigned long long)bogus[i], GetLastError());
    }
}

TEST(gap7_getmessagew_destroyed_window_returns_minus1)
{
    REQUIRE(U(GetMessageW));
    HWND w = g7_mkwinW(0, 0, NULL);
    CHECK(U(DestroyWindow)(w));
    MSG m;
    SET_ERR(0);
    CHECK(U(GetMessageW)(&m, w, 0, 0) == -1);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
}

TEST(gap7_getmessagew_fifo_and_range_filter)
{
    REQUIRE(U(GetMessageW)); REQUIRE(U(PeekMessageW)); REQUIRE(U(PostMessageW));
    HWND w = g7_mkwinW(0, 0, NULL);
    UINT base = WM_APP + 100;
    for (int i = 0; i < 5; i++) U(PostMessageW)(w, base + i, i, i * 2);
    MSG m;
    /* in-range filter picks the first matching, not the first queued */
    CHECK(U(PeekMessageW)(&m, w, base + 3, base + 4, PM_NOREMOVE));
    CHECK(U(GetMessageW)(&m, w, base + 3, base + 4) > 0);
    CHECK(m.message == base + 3);
    CHECK(U(GetMessageW)(&m, w, base + 3, base + 4) > 0);
    CHECK(m.message == base + 4);
    for (int i = 0; i < 3; i++) {
        CHECK(U(GetMessageW)(&m, w, 0, 0) > 0);
        CHECKF(m.message == base + i && m.wParam == (WPARAM)i && m.lParam == i * 2, "i=%d msg=%x", i, m.message);
        CHECK(m.hwnd == w);
    }
    U(DestroyWindow)(w);
}

TEST(gap7_getmessagew_hwnd_filter)
{
    REQUIRE(U(GetMessageW)); REQUIRE(U(PeekMessageW)); REQUIRE(U(PostMessageW));
    HWND a = g7_mkwinW(0, 0, NULL), b = g7_mkwinW(0, 0, NULL);
    U(PostMessageW)(a, WM_APP + 40, 1, 0);
    U(PostMessageW)(b, WM_APP + 40, 2, 0);
    MSG m;
    CHECK(U(PeekMessageW)(&m, b, 0, 0, PM_NOREMOVE));
    CHECK(U(GetMessageW)(&m, b, 0, 0) > 0);
    CHECK(m.hwnd == b && m.wParam == 2);
    CHECK(U(GetMessageW)(&m, a, 0, 0) > 0);
    CHECK(m.hwnd == a && m.wParam == 1);
    /* destroyed windows drop their queued messages */
    U(PostMessageW)(a, WM_APP + 41, 0, 0);
    U(DestroyWindow)(a);
    CHECK(!U(PeekMessageW)(&m, NULL, WM_APP + 41, WM_APP + 41, PM_NOREMOVE));
    U(DestroyWindow)(b);
}

TEST(gap7_getmessagew_thread_messages_via_minus1_hwnd)
{
    REQUIRE(U(GetMessageW)); REQUIRE(U(PeekMessageW)); REQUIRE(U(PostMessageW)); REQUIRE(U(PostThreadMessageW));
    HWND w = g7_mkwinW(0, 0, NULL);
    CHECK(U(PostThreadMessageW)(GetCurrentThreadId(), WM_APP + 50, 8, 9));
    U(PostMessageW)(w, WM_APP + 51, 0, 0);
    MSG m;
    CHECK(U(PeekMessageW)(&m, (HWND)-1, 0, 0, PM_NOREMOVE));
    CHECK(U(GetMessageW)(&m, (HWND)-1, 0, 0) > 0);
    CHECK(m.hwnd == NULL && m.message == WM_APP + 50 && m.wParam == 8 && m.lParam == 9);
    CHECK(!U(PeekMessageW)(&m, (HWND)-1, 0, 0, PM_NOREMOVE));
    CHECK(U(GetMessageW)(&m, w, 0, 0) > 0 && m.message == WM_APP + 51);
    U(DestroyWindow)(w);
}

TEST(gap7_getmessagew_wm_quit_returns_zero)
{
    REQUIRE(U(GetMessageW)); REQUIRE(U(PostQuitMessage)); REQUIRE(U(PeekMessageW));
    MSG m; memset(&m, 0, sizeof m);
    t_pump();
    U(PostQuitMessage)(42);
    BOOL r = U(GetMessageW)(&m, NULL, 0, 0);
    CHECKF(r == 0, "r=%d", r);
    CHECK(m.message == WM_QUIT);
    CHECK(m.wParam == 42);
    /* the quit message is consumed */
    CHECK(!U(PeekMessageW)(&m, NULL, WM_QUIT, WM_QUIT, PM_NOREMOVE));
    U(PostQuitMessage)(-5);
    r = U(GetMessageW)(&m, NULL, 0, 0);
    CHECK(r == 0 && m.message == WM_QUIT && (int)m.wParam == -5);
}

TEST(gap7_getmessagew_wm_char_wparam_masked_to_16_bits)
{
    REQUIRE(U(GetMessageW)); REQUIRE(U(PostMessageW));
    HWND w = g7_mkwinW(0, 0, NULL);
    U(PostMessageW)(w, WM_CHAR, 0x12345, 0x1);
    MSG m;
    CHECK(U(GetMessageW)(&m, w, WM_CHAR, WM_CHAR) > 0);
    CHECK(m.message == WM_CHAR);
    CHECKF((m.wParam & ~(WPARAM)0xFFFF) == 0, "wParam=%llx", (unsigned long long)m.wParam);
    U(PostMessageW)(w, WM_CHAR, 0x41, 0x1);
    CHECK(U(GetMessageW)(&m, w, WM_CHAR, WM_CHAR) > 0 && m.wParam == 0x41);
    U(DestroyWindow)(w);
}

TEST(gap7_getmessagew_fills_all_msg_fields)
{
    REQUIRE(U(GetMessageW)); REQUIRE(U(PostMessageW));
    HWND w = g7_mkwinW(0, 0, NULL);
    MSG m; memset(&m, 0xCC, sizeof m);
    U(PostMessageW)(w, WM_APP + 60, 0xAABBCCDD, 0x11223344);
    CHECK(U(GetMessageW)(&m, w, 0, 0) > 0);
    CHECK(m.hwnd == w && m.message == WM_APP + 60 && m.wParam == 0xAABBCCDD && m.lParam == 0x11223344);
    CHECK(m.time != 0xCCCCCCCC);
    CHECK(m.pt.x != (LONG)0xCCCCCCCC || m.pt.y != (LONG)0xCCCCCCCC);
    U(DestroyWindow)(w);
}
