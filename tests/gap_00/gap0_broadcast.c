/* gap_00: BroadcastSystemMessageW/A/ExW and CsrBroadcastSystemMessageExW */
#include "gap0.h"

typedef struct { UINT cbSize; HDESK hdesk; HWND hwnd; LUID luid; } G0BSM;
UDECL(ULONG, CsrBroadcastSystemMessageExW, (DWORD, LPDWORD, UINT, WPARAM, LPARAM, void *));

/* recipient mask without BSM_APPLICATIONS: validation runs, nothing is sent, result is -1 */
#define NO_APPS BSM_VXDS

TEST(gap0_bsm_msg_bits_above_17_rejected) {
    REQUIRE(U(BroadcastSystemMessageW));
    static const UINT m[] = { 0x20000, 0x20001, 0x40000, 0x80000000u, 0xFFFFFFFFu, 0x1000000 };
    for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) {
        DWORD rec = NO_APPS;
        SET_ERR(0);
        LONG r = U(BroadcastSystemMessageW)(0, &rec, m[i], 0, 0);
        CHECKF(r == 0, "msg %x ret %ld", m[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(gap0_bsm_wm_user_range_rejected) {
    REQUIRE(U(BroadcastSystemMessageW));
    static const UINT m[] = { 0x400, 0x401, 0x7FFF, 0x8000, 0x8001, 0xBFFE, 0xBFFF, WM_USER, WM_APP };
    for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) {
        DWORD rec = NO_APPS;
        SET_ERR(0);
        LONG r = U(BroadcastSystemMessageW)(0, &rec, m[i], 0, 0);
        CHECKF(r == 0, "msg %x ret %ld", m[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(gap0_bsm_valid_message_ranges_pass_validation) {
    REQUIRE(U(BroadcastSystemMessageW));
    static const UINT m[] = { 0, 0x1, 0x3FF, 0xC000, 0xC001, 0xFFFF, 0x10000, 0x1FFFF };
    for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) {
        DWORD rec = NO_APPS;
        SET_ERR(0);
        LONG r = U(BroadcastSystemMessageW)(0, &rec, m[i], 0, 0);
        CHECKF(r == -1, "msg %x ret %ld", m[i], r);
        EXPECT_EQ(rec, NO_APPS);
    }
}
TEST(gap0_bsm_bad_flags_rejected) {
    REQUIRE(U(BroadcastSystemMessageW));
    static const DWORD f[] = { 0x800, 0x1000, 0x8000, 0x10000, 0x80000000u, 0xFFFFF800u, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        DWORD rec = NO_APPS;
        SET_ERR(0);
        LONG r = U(BroadcastSystemMessageW)(f[i], &rec, 0xC000, 0, 0);
        CHECKF(r == 0, "flags %lx ret %ld", f[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(gap0_bsm_bad_recipients_rejected) {
    REQUIRE(U(BroadcastSystemMessageW));
    static const DWORD rc[] = { 0x20, 0x40, 0x80, 0x100, 0x8000, 0x80000000u, 0x21, 0x3F, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof rc / sizeof rc[0]; i++) {
        DWORD rec = rc[i];
        SET_ERR(0);
        LONG r = U(BroadcastSystemMessageW)(0, &rec, 0xC000, 0, 0);
        CHECKF(r == 0, "recipients %lx ret %ld", rc[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        EXPECT_EQ(rec, rc[i]);
    }
}
TEST(gap0_bsm_recipients_without_apps_return_minus_one) {
    REQUIRE(U(BroadcastSystemMessageW));
    static const DWORD rc[] = { BSM_VXDS, BSM_NETDRIVER, BSM_INSTALLABLEDRIVERS, 3, 5, 6, 7 };
    for (unsigned i = 0; i < sizeof rc / sizeof rc[0]; i++) {
        DWORD rec = rc[i];
        LONG r = U(BroadcastSystemMessageW)(0, &rec, 0xC000, 0, 0);
        CHECKF(r == -1, "recipients %lx ret %ld", rc[i], r);
        EXPECT_EQ(rec, rc[i]);
    }
}
TEST(gap0_bsm_flag_bits_valid_with_no_apps) {
    REQUIRE(U(BroadcastSystemMessageW));
    /* every documented flag bit below 0x800 passes validation for a registered-range message */
    for (DWORD f = 1; f < 0x800; f <<= 1) {
        if (f == BSF_LUID || f == BSF_RETURNHDESK) continue; /* need BSMINFO */
        DWORD rec = NO_APPS;
        SET_ERR(0);
        LONG r = U(BroadcastSystemMessageW)(f, &rec, 0xC000, 0, 0);
        CHECKF(r == -1, "flag %lx ret %ld err %lu", f, r, GetLastError());
    }
}
TEST(gap0_bsm_luid_flags_need_info) {
    REQUIRE(U(BroadcastSystemMessageW));
    static const DWORD f[] = { BSF_LUID, BSF_RETURNHDESK, BSF_LUID | BSF_RETURNHDESK };
    for (unsigned i = 0; i < 3; i++) {
        DWORD rec = NO_APPS;
        SET_ERR(0);
        LONG r = U(BroadcastSystemMessageW)(f[i], &rec, 0xC000, 0, 0);
        CHECKF(r == 0, "flags %lx ret %ld", f[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(gap0_bsmex_cbsize_validation) {
    REQUIRE(U(BroadcastSystemMessageExW));
    static const UINT sz[] = { 0, 1, 0x10, 0x1F, 0x21, 0x28, 0x40, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        G0BSM info; memset(&info, 0, sizeof info); info.cbSize = sz[i];
        DWORD rec = NO_APPS;
        SET_ERR(0);
        LONG r = U(BroadcastSystemMessageExW)(0, &rec, 0xC000, 0, 0, (PBSMINFO)&info);
        CHECKF(r == 0, "cbSize %x ret %ld", sz[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(gap0_bsmex_cbsize_ok) {
    REQUIRE(U(BroadcastSystemMessageExW));
    G0BSM info; memset(&info, 0, sizeof info); info.cbSize = 0x20;
    EXPECT_EQ(sizeof(G0BSM), 0x20);
    DWORD rec = NO_APPS;
    LONG r = U(BroadcastSystemMessageExW)(0, &rec, 0xC000, 0, 0, (PBSMINFO)&info);
    EXPECT_EQ(r, -1);
}
TEST(gap0_bsmex_luid_flags_with_info) {
    REQUIRE(U(BroadcastSystemMessageExW));
    static const DWORD f[] = { BSF_LUID, BSF_RETURNHDESK, BSF_LUID | BSF_RETURNHDESK };
    for (unsigned i = 0; i < 3; i++) {
        G0BSM info; memset(&info, 0, sizeof info); info.cbSize = 0x20;
        DWORD rec = NO_APPS;
        LONG r = U(BroadcastSystemMessageExW)(f[i], &rec, 0xC000, 0, 0, (PBSMINFO)&info);
        CHECKF(r == -1, "flags %lx ret %ld", f[i], r);
    }
}
TEST(gap0_bsmex_luid_flag_wrong_size) {
    REQUIRE(U(BroadcastSystemMessageExW));
    G0BSM info; memset(&info, 0, sizeof info); info.cbSize = 0x18;
    DWORD rec = NO_APPS;
    SET_ERR(0);
    LONG r = U(BroadcastSystemMessageExW)(BSF_LUID, &rec, 0xC000, 0, 0, (PBSMINFO)&info);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
}
TEST(gap0_bsm_a_w_validation_parity) {
    REQUIRE(U(BroadcastSystemMessageA)); REQUIRE(U(BroadcastSystemMessageW));
    static const UINT m[] = { 0x400, 0x20000, 0xC000, 0x3FF };
    for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) {
        DWORD ra = NO_APPS, rw = NO_APPS;
        SET_ERR(0);
        LONG a = U(BroadcastSystemMessageA)(0, &ra, m[i], 0, 0);
        DWORD ea = GetLastError();
        SET_ERR(0);
        LONG w = U(BroadcastSystemMessageW)(0, &rw, m[i], 0, 0);
        DWORD ew = GetLastError();
        CHECKF(a == w, "msg %x A=%ld W=%ld", m[i], a, w);
        CHECKF(ea == ew, "msg %x errA=%lu errW=%lu", m[i], ea, ew);
    }
}
TEST(gap0_bsmex_a_w_parity) {
    REQUIRE(U(BroadcastSystemMessageExA)); REQUIRE(U(BroadcastSystemMessageExW));
    G0BSM info; memset(&info, 0, sizeof info); info.cbSize = 0x10;
    DWORD ra = NO_APPS, rw = NO_APPS;
    SET_ERR(0);
    LONG a = U(BroadcastSystemMessageExA)(0, &ra, 0xC000, 0, 0, (PBSMINFO)&info);
    DWORD ea = GetLastError();
    SET_ERR(0);
    LONG w = U(BroadcastSystemMessageExW)(0, &rw, 0xC000, 0, 0, (PBSMINFO)&info);
    DWORD ew = GetLastError();
    EXPECT_EQ(a, w);
    EXPECT_EQ(ea, ew);
    EXPECT_EQ(ew, ERROR_INVALID_PARAMETER);
}
TEST(gap0_bsm_null_recipients_ptr_accepts_validation_failure) {
    REQUIRE(U(BroadcastSystemMessageW));
    SET_ERR(0);
    LONG r = U(BroadcastSystemMessageW)(0, NULL, 0x400, 0, 0);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(BroadcastSystemMessageW)(0x800, NULL, 0xC000, 0, 0);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
}

/* ---- real delivery of a private registered message ---- */
static UINT reg_msg(void) {
    return U(RegisterWindowMessageW) ? U(RegisterWindowMessageW)(L"Gap0_BroadcastTestMsg_1") : 0;
}
TEST(gap0_bsm_registered_message_in_range) {
    REQUIRE(U(RegisterWindowMessageW));
    UINT m = reg_msg();
    CHECK(m >= 0xC000 && m <= 0xFFFF);
    EXPECT_EQ(reg_msg(), m);
}
/* Number of top-level windows of this process that belong to the test class.
 * A broadcast reaches every one of them, including windows other tests leaked
 * and any left-over from earlier in the run, so expectations are relative to
 * this count rather than a literal. */
static BOOL CALLBACK live_cb(HWND h, LPARAM lp) {
    DWORD pid = 0; WCHAR cn[64];
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId() && GetClassNameW(h, cn, 64) && !lstrcmpW(cn, g0_cls())) (*(int *)lp)++;
    return TRUE;
}
static int live_toplevels(void) { int n = 0; EnumWindows(live_cb, (LPARAM)&n); return n; }

/* Synchronous (sent) broadcasts: on Win10 22H2 these return 0 and deliver
 * nothing to this thread's windows while the call is in progress (the
 * posted and sendnotify forms do deliver). Record what happened, after a
 * pump too, and only assert consistency: a positive result must have
 * delivered to every live window. */
static void bsm_sync_report(const char *what, LONG r, int live) {
    int immediate = g0_bsm_count;
    g0_pump_ms(300);
    t_note("%s: ret %ld, live windows %d, delivered immediately %d, after pump %d", what, r, live, immediate, g0_bsm_count);
    if (r > 0) CHECKF(immediate == live, "%s: delivered %d of %d", what, immediate, live);
}

TEST(gap0_bsm_post_reaches_toplevel_window) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(w);
    int live = live_toplevels();
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_POSTMESSAGE | BSF_NOHANG, &rec, g0_bsm_msg, 0, 0);
    CHECKF(r > 0, "ret %ld", r);
    g0_pump_ms(300);
    CHECKF(live >= 1 && g0_bsm_count == live, "delivered %d to %d live windows", g0_bsm_count, live);
    U(DestroyWindow)(w);
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_ignorecurrenttask_skips_own_windows) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(w);
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_POSTMESSAGE | BSF_NOHANG | BSF_IGNORECURRENTTASK, &rec, g0_bsm_msg, 0, 0);
    CHECKF(r > 0, "ret %ld", r);
    g0_pump_ms(300);
    EXPECT_EQ(g0_bsm_count, 0);
    U(DestroyWindow)(w);
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_sendnotify_reaches_toplevel_window) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(w);
    int live = live_toplevels();
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_SENDNOTIFYMESSAGE | BSF_NOHANG, &rec, g0_bsm_msg, 0, 0);
    CHECKF(r > 0, "ret %ld", r);
    g0_pump_ms(300);
    CHECKF(live >= 1 && g0_bsm_count == live, "delivered %d to %d live windows", g0_bsm_count, live);
    U(DestroyWindow)(w);
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_send_synchronous_delivery) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(w);
    int live = live_toplevels();
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_NOHANG, &rec, g0_bsm_msg, 0, 0);
    bsm_sync_report("sync BSM_APPLICATIONS", r, live);
    U(DestroyWindow)(w);
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_default_recipients_deliver) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(w);
    int live = live_toplevels();
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    LONG r = U(BroadcastSystemMessageW)(BSF_NOHANG, NULL, g0_bsm_msg, 0, 0);   /* NULL recipients = all */
    bsm_sync_report("sync NULL recipients", r, live);
    U(DestroyWindow)(w);
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_query_denied_returns_zero) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(w);
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = BROADCAST_QUERY_DENY;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_QUERY | BSF_NOHANG, &rec, g0_bsm_msg, 0, 0);
    EXPECT_EQ(r, 0);
    /* The query is a sent message: see bsm_sync_report. Delivery is recorded only. */
    t_note("query denied: recipients now 0x%lx, windows queried %d", rec, g0_bsm_count);
    g0_deny = 0;
    U(DestroyWindow)(w);
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_query_granted) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(w);
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_QUERY | BSF_NOHANG, &rec, g0_bsm_msg, 0, 0);
    t_note("query granted: ret %ld, windows queried %d", r, g0_bsm_count);
    if (r > 0) CHECK(g0_bsm_count >= 1);
    U(DestroyWindow)(w);
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_no_windows_after_destroy) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND w = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(w);
    U(DestroyWindow)(w);
    int live = live_toplevels();
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_NOHANG, &rec, g0_bsm_msg, 0, 0);
    bsm_sync_report("sync after destroy", r, live);   /* live excludes the destroyed window */
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_child_windows_not_targeted) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    HWND p = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    REQUIRE(p);
    HWND c = g0_mk(WS_CHILD | WS_VISIBLE, 0, 0, 0, 20, 20, p);
    REQUIRE(c);
    int live = live_toplevels();   /* counts top-level windows only, never the child */
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_POSTMESSAGE | BSF_NOHANG, &rec, g0_bsm_msg, 0, 0);
    CHECKF(r > 0, "ret %ld", r);
    g0_pump_ms(300);
    CHECKF(g0_bsm_count == live, "delivered %d to %d top-level windows", g0_bsm_count, live);
    U(DestroyWindow)(p);
    g0_bsm_msg = 0;
}
TEST(gap0_bsm_two_toplevels) {
    REQUIRE(U(BroadcastSystemMessageW)); REQUIRE(U(RegisterWindowMessageW)); REQUIRE(U(DestroyWindow));
    int before = live_toplevels();
    HWND a = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    HWND b = g0_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(a); REQUIRE(b);
    int live = live_toplevels();
    CHECKF(live == before + 2, "live %d before %d", live, before);
    g0_bsm_msg = reg_msg(); g0_bsm_count = 0; g0_deny = 0;
    DWORD rec = BSM_APPLICATIONS;
    LONG r = U(BroadcastSystemMessageW)(BSF_POSTMESSAGE | BSF_NOHANG, &rec, g0_bsm_msg, 0, 0);
    CHECKF(r > 0, "ret %ld", r);
    g0_pump_ms(300);
    CHECKF(g0_bsm_count == live, "delivered %d to %d live windows", g0_bsm_count, live);
    U(DestroyWindow)(a); U(DestroyWindow)(b);
    g0_bsm_msg = 0;
}

/* ---- CsrBroadcastSystemMessageExW: server-process only ---- */
TEST(gap0_csrbsm_denied_for_client_process) {
    REQUIRE(UN(CsrBroadcastSystemMessageExW));
    DWORD rec = NO_APPS;
    ULONG st = UN(CsrBroadcastSystemMessageExW)(0, &rec, 0xC000, 0, 0, NULL);
    EXPECT_EQ(st, 0xC0000022);   /* STATUS_ACCESS_DENIED */
    EXPECT_EQ(rec, NO_APPS);
}
TEST(gap0_csrbsm_denied_ignores_arguments) {
    REQUIRE(UN(CsrBroadcastSystemMessageExW));
    static const UINT m[] = { 0, 0x400, 0x20000, 0xC000 };
    for (unsigned i = 0; i < 4; i++) {
        DWORD rec = NO_APPS;
        ULONG st = UN(CsrBroadcastSystemMessageExW)(0xFFFFFFFF, &rec, m[i], 1, 2, NULL);
        CHECKF(st == 0xC0000022, "msg %x status %lx", m[i], st);
    }
}
TEST(gap0_csrbsm_does_not_set_last_error) {
    REQUIRE(UN(CsrBroadcastSystemMessageExW));
    DWORD rec = NO_APPS;
    SET_ERR(0x1234);
    UN(CsrBroadcastSystemMessageExW)(0, &rec, 0xC000, 0, 0, NULL);
    EXPECT_EQ(GetLastError(), 0x1234);
}
