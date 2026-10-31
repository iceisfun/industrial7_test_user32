/* gap_01: DPI-related internal exports. */
#include "t.h"

UDECL(BOOL, GetDpiForMonitorInternal, (HMONITOR, UINT, UINT *, UINT *));
UDECL(BOOL, GetProcessDpiAwarenessInternal, (HANDLE, int *));
UDECL(DPI_AWARENESS_CONTEXT, GetDpiAwarenessContextForProcess, (HANDLE));

#define CUR_PROC ((HANDLE)(LONG_PTR)-1)

static HMONITOR primary(void) {
    POINT pt = {0, 0};
    return U(MonitorFromPoint) ? U(MonitorFromPoint)(pt, MONITOR_DEFAULTTOPRIMARY) : NULL;
}

/* ---- GetDpiForMonitorInternal ---- */
TEST(gap1_dpimon_type_out_of_range) {
    REQUIRE(UN(GetDpiForMonitorInternal));
    UINT types[] = {3, 4, 5, 0x10, 0xff, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff};
    HMONITOR mons[2] = { primary(), NULL };
    for (int m = 0; m < 2; m++)
        for (int i = 0; i < 9; i++) {
            UINT x = 0x1111, y = 0x2222;
            SetLastError(0);
            BOOL r = UN(GetDpiForMonitorInternal)(mons[m], types[i], &x, &y);
            CHECKF(r == FALSE, "type %u mon %d", types[i], m);
            EXPECT_EQ(GetLastError(), ERROR_BAD_ARGUMENTS);
            EXPECT_EQ(x, 0x1111);
            EXPECT_EQ(y, 0x2222);
        }
}
TEST(gap1_dpimon_type_error_checked_before_monitor) {
    REQUIRE(UN(GetDpiForMonitorInternal));
    /* A bogus monitor with a bad type still reports the bad-argument error, not a monitor error. */
    SetLastError(0);
    BOOL r = UN(GetDpiForMonitorInternal)((HMONITOR)(ULONG_PTR)0xdeadbeef, 3, NULL, NULL);
    CHECK(r == FALSE);
    EXPECT_EQ(GetLastError(), ERROR_BAD_ARGUMENTS);
}
TEST(gap1_dpimon_valid_types_primary) {
    REQUIRE(UN(GetDpiForMonitorInternal));
    HMONITOR m = primary(); CHECK(m != NULL);
    for (UINT t = 0; t < 3; t++) {
        UINT x = 0, y = 0;
        BOOL r = UN(GetDpiForMonitorInternal)(m, t, &x, &y);
        CHECKF(r, "type %u", t);
        if (r) {
            CHECKF(x >= 48 && x <= 1000, "x=%u", x);
            CHECKF(y >= 48 && y <= 1000, "y=%u", y);
        }
    }
}
TEST(gap1_dpimon_effective_square) {
    REQUIRE(UN(GetDpiForMonitorInternal));
    UINT x = 0, y = 0;
    if (!UN(GetDpiForMonitorInternal)(primary(), 0, &x, &y)) { CHECK(0); return; }
    EXPECT_EQ(x, y);
}
TEST(gap1_dpimon_repeatable) {
    REQUIRE(UN(GetDpiForMonitorInternal));
    UINT x0 = 0, y0 = 0;
    BOOL r0 = UN(GetDpiForMonitorInternal)(primary(), 0, &x0, &y0);
    for (int i = 0; i < 200; i++) {
        UINT x = 0, y = 0;
        BOOL r = UN(GetDpiForMonitorInternal)(primary(), 0, &x, &y);
        if (r != r0 || x != x0 || y != y0) { CHECK(0); return; }
    }
    CHECK(1);
}
TEST(gap1_dpimon_bogus_monitor_fails) {
    REQUIRE(UN(GetDpiForMonitorInternal));
    HMONITOR bad[] = {NULL, (HMONITOR)(ULONG_PTR)1, (HMONITOR)(ULONG_PTR)0xdeadbeef, (HMONITOR)(ULONG_PTR)-1};
    for (int i = 0; i < 4; i++) {
        UINT x = 7, y = 9;
        SetLastError(0);
        BOOL r = UN(GetDpiForMonitorInternal)(bad[i], 0, &x, &y);
        CHECKF(r == FALSE, "bad monitor %d", i);
        DWORD e = GetLastError();
        if (e != ERROR_INVALID_PARAMETER && e != ERROR_INVALID_HANDLE) t_note("bogus monitor last error %lu", e);
    }
}
TEST(gap1_dpimon_matches_monitorinfo_scale_note) {
    REQUIRE(UN(GetDpiForMonitorInternal));
    HMONITOR m = primary();
    UINT x = 0, y = 0, ax = 0, ay = 0, rx = 0, ry = 0;
    UN(GetDpiForMonitorInternal)(m, 0, &x, &y);
    UN(GetDpiForMonitorInternal)(m, 1, &ax, &ay);
    UN(GetDpiForMonitorInternal)(m, 2, &rx, &ry);
    t_note("primary monitor dpi effective=%u/%u angular=%u/%u raw=%u/%u", x, y, ax, ay, rx, ry);
    if (U(GetDpiForSystem)) t_note("GetDpiForSystem=%u", U(GetDpiForSystem)());
}
TEST(gap1_dpimon_enum_monitors_all_valid) {
    REQUIRE(UN(GetDpiForMonitorInternal)); REQUIRE(U(EnumDisplayMonitors));
    HMONITOR mons[16]; int n = 0;
    BOOL CALLBACK cb(HMONITOR h, HDC dc, LPRECT rc, LPARAM lp) { if (n < 16) mons[n++] = h; return TRUE; }
    U(EnumDisplayMonitors)(NULL, NULL, cb, 0);
    CHECK(n >= 1);
    for (int i = 0; i < n; i++) {
        UINT x = 0, y = 0;
        CHECKF(UN(GetDpiForMonitorInternal)(mons[i], 0, &x, &y), "monitor %d", i);
        CHECK(x > 0 && y > 0);
    }
}

/* ---- GetProcessDpiAwarenessInternal ---- */
TEST(gap1_procdpi_current_process) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal));
    int a = 0x77;
    BOOL r = UN(GetProcessDpiAwarenessInternal)(CUR_PROC, &a);
    CHECK(r);
    CHECKF(a >= 0 && a <= 2, "a=%d", a);
}
TEST(gap1_procdpi_real_process_handle_same_as_pseudo) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!h) { t_note("OpenProcess failed"); return; }
    int a = 0x77, b = 0x78;
    BOOL r1 = UN(GetProcessDpiAwarenessInternal)(h, &a);
    BOOL r2 = UN(GetProcessDpiAwarenessInternal)(CUR_PROC, &b);
    CHECK(r1 == r2);
    CHECK(a == b);
    CloseHandle(h);
}
TEST(gap1_procdpi_matches_awareness_context) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal));
    REQUIRE(UN(GetDpiAwarenessContextForProcess)); REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    int a = 0x77;
    if (!UN(GetProcessDpiAwarenessInternal)(CUR_PROC, &a)) { CHECK(0); return; }
    DPI_AWARENESS_CONTEXT c = UN(GetDpiAwarenessContextForProcess)(CUR_PROC);
    DPI_AWARENESS aw = U(GetAwarenessFromDpiAwarenessContext)(c);
    CHECKF((int)aw == a, "internal %d, ctx-derived %d", a, (int)aw);
}
TEST(gap1_procdpi_vs_isprocessdpiaware) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal)); REQUIRE(U(IsProcessDPIAware));
    int a = -5;
    if (!UN(GetProcessDpiAwarenessInternal)(CUR_PROC, &a)) { CHECK(0); return; }
    BOOL aware = U(IsProcessDPIAware)();
    if ((a != 0) != (aware != 0)) t_note("awareness %d but IsProcessDPIAware=%d", a, aware);
    CHECK(1);
}
TEST(gap1_procdpi_bogus_handle_writes_minus_one) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal));
    HANDLE bad[] = {(HANDLE)(ULONG_PTR)0x7ffffff0, (HANDLE)(ULONG_PTR)0xdeadbee0, (HANDLE)(ULONG_PTR)0x44444};
    for (int i = 0; i < 3; i++) {
        int a = 0x77;
        BOOL r = UN(GetProcessDpiAwarenessInternal)(bad[i], &a);
        CHECKF(r == FALSE, "bad handle %d", i);
        /* user32 always stores -1 into the out value when the query fails */
        CHECKF(a == -1, "a=%d", a);
    }
}
TEST(gap1_procdpi_closed_handle) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!h) return;
    CloseHandle(h);
    int a = 0x77;
    BOOL r = UN(GetProcessDpiAwarenessInternal)(h, &a);
    CHECK(r == FALSE);
    CHECK(a == -1);
}
TEST(gap1_procdpi_null_handle_note) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal));
    int a = 0x77;
    BOOL r = UN(GetProcessDpiAwarenessInternal)(NULL, &a);
    t_note("NULL process handle: ret=%d awareness=%d err=%lu", r, a, GetLastError());
    if (!r) CHECK(a == -1);
}
TEST(gap1_procdpi_repeatable) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal));
    int a0 = 0; BOOL r0 = UN(GetProcessDpiAwarenessInternal)(CUR_PROC, &a0);
    for (int i = 0; i < 500; i++) {
        int a = 0x99; BOOL r = UN(GetProcessDpiAwarenessInternal)(CUR_PROC, &a);
        if (r != r0 || a != a0) { CHECK(0); return; }
    }
    CHECK(1);
}
static DWORD WINAPI procdpi_thread(void *p) {
    int a = 0x77;
    BOOL r = UN(GetProcessDpiAwarenessInternal)(CUR_PROC, &a);
    *(int *)p = r ? a : -100;
    return 0;
}
TEST(gap1_procdpi_other_thread_same_value) {
    REQUIRE(UN(GetProcessDpiAwarenessInternal));
    int a = 0x77;
    UN(GetProcessDpiAwarenessInternal)(CUR_PROC, &a);
    int b = 0x55;
    HANDLE t = CreateThread(NULL, 0, procdpi_thread, &b, 0, NULL);
    CHECK(t != NULL);
    if (t) { CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0); CloseHandle(t); }
    CHECK(a == b);
}

/* ---- GetSystemDpiForProcess ---- */
TEST(gap1_sysdpi_current_process) {
    REQUIRE(U(GetSystemDpiForProcess));
    UINT d = U(GetSystemDpiForProcess)(CUR_PROC);
    CHECKF(d >= 48 && d <= 1000, "dpi %u", d);
    if (U(GetDpiForSystem) && U(GetDpiForSystem)() != d) t_note("GetSystemDpiForProcess=%u GetDpiForSystem=%u (differs for unaware processes)", d, U(GetDpiForSystem)());
}
TEST(gap1_sysdpi_real_handle) {
    REQUIRE(U(GetSystemDpiForProcess));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!h) return;
    CHECK(U(GetSystemDpiForProcess)(h) == U(GetSystemDpiForProcess)(CUR_PROC));
    CloseHandle(h);
}
TEST(gap1_sysdpi_bogus_handles) {
    REQUIRE(U(GetSystemDpiForProcess));
    HANDLE bad[] = {NULL, (HANDLE)(ULONG_PTR)0x44444, (HANDLE)(ULONG_PTR)0xdeadbee0, INVALID_HANDLE_VALUE == (HANDLE)-1 ? (HANDLE)(ULONG_PTR)0x7ffffff8 : NULL};
    for (int i = 0; i < 4; i++) {
        SetLastError(0);
        UINT d = U(GetSystemDpiForProcess)(bad[i]);
        if (d != 0) t_note("handle %p returned %u (err %lu)", bad[i], d, GetLastError());
        else CHECK(d == 0);
    }
}
TEST(gap1_sysdpi_non_process_handle) {
    REQUIRE(U(GetSystemDpiForProcess));
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    UINT d = U(GetSystemDpiForProcess)(ev);
    CHECKF(d == 0, "event handle returned %u", d);
    CloseHandle(ev);
}
TEST(gap1_sysdpi_repeatable) {
    REQUIRE(U(GetSystemDpiForProcess));
    UINT d0 = U(GetSystemDpiForProcess)(CUR_PROC);
    for (int i = 0; i < 300; i++) if (U(GetSystemDpiForProcess)(CUR_PROC) != d0) { CHECK(0); return; }
    CHECK(1);
}
TEST(gap1_sysdpi_vs_aware_thread_context) {
    REQUIRE(U(GetSystemDpiForProcess)); REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetDpiForSystem));
    DPI_AWARENESS_CONTEXT old = U(SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    if (!old) { t_note("SetThreadDpiAwarenessContext failed"); return; }
    UINT sysd = U(GetDpiForSystem)();
    UINT proc = U(GetSystemDpiForProcess)(CUR_PROC);
    U(SetThreadDpiAwarenessContext)(old);
    CHECKF(sysd == proc, "system-aware thread GetDpiForSystem=%u process=%u", sysd, proc);
}

/* ---- Thread DPI hosting behaviour ---- */
TEST(gap1_hosting_default_zero) {
    REQUIRE(U(GetThreadDpiHostingBehavior));
    EXPECT_EQ(U(GetThreadDpiHostingBehavior)(), DPI_HOSTING_BEHAVIOR_DEFAULT);
}
TEST(gap1_hosting_set_returns_previous) {
    REQUIRE(U(GetThreadDpiHostingBehavior)); REQUIRE(U(SetThreadDpiHostingBehavior));
    DPI_HOSTING_BEHAVIOR orig = U(GetThreadDpiHostingBehavior)();
    DPI_HOSTING_BEHAVIOR r = U(SetThreadDpiHostingBehavior)(DPI_HOSTING_BEHAVIOR_MIXED);
    EXPECT_EQ(r, orig);
    EXPECT_EQ(U(GetThreadDpiHostingBehavior)(), DPI_HOSTING_BEHAVIOR_MIXED);
    r = U(SetThreadDpiHostingBehavior)(DPI_HOSTING_BEHAVIOR_MIXED);
    EXPECT_EQ(r, DPI_HOSTING_BEHAVIOR_MIXED);
    r = U(SetThreadDpiHostingBehavior)(DPI_HOSTING_BEHAVIOR_DEFAULT);
    EXPECT_EQ(r, DPI_HOSTING_BEHAVIOR_MIXED);
    EXPECT_EQ(U(GetThreadDpiHostingBehavior)(), DPI_HOSTING_BEHAVIOR_DEFAULT);
    U(SetThreadDpiHostingBehavior)(orig);
}
TEST(gap1_hosting_invalid_values) {
    REQUIRE(U(GetThreadDpiHostingBehavior)); REQUIRE(U(SetThreadDpiHostingBehavior));
    DPI_HOSTING_BEHAVIOR orig = U(GetThreadDpiHostingBehavior)();
    int bad[] = {2, 3, 4, 0x10, 0x100, 0x7fffffff, (int)0x80000000, -1, -2};
    for (int i = 0; i < 9; i++) {
        SetLastError(0);
        int r = (int)U(SetThreadDpiHostingBehavior)((DPI_HOSTING_BEHAVIOR)bad[i]);
        CHECKF(r == -1, "value %d returned %d", bad[i], r);
        EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(U(GetThreadDpiHostingBehavior)(), orig);
    }
}
TEST(gap1_hosting_invalid_does_not_clobber_error_on_success) {
    REQUIRE(U(SetThreadDpiHostingBehavior)); REQUIRE(U(GetThreadDpiHostingBehavior));
    DPI_HOSTING_BEHAVIOR orig = U(GetThreadDpiHostingBehavior)();
    SetLastError(0xabcd);
    U(SetThreadDpiHostingBehavior)(orig);
    EXPECT_EQ(GetLastError(), 0xabcd);
    U(SetThreadDpiHostingBehavior)(orig);
}
static DWORD WINAPI hosting_thread(void *p) {
    int *o = p;
    o[0] = (int)U(GetThreadDpiHostingBehavior)();
    o[1] = (int)U(SetThreadDpiHostingBehavior)(DPI_HOSTING_BEHAVIOR_MIXED);
    o[2] = (int)U(GetThreadDpiHostingBehavior)();
    return 0;
}
TEST(gap1_hosting_is_per_thread) {
    REQUIRE(U(GetThreadDpiHostingBehavior)); REQUIRE(U(SetThreadDpiHostingBehavior));
    DPI_HOSTING_BEHAVIOR orig = U(GetThreadDpiHostingBehavior)();
    U(SetThreadDpiHostingBehavior)(DPI_HOSTING_BEHAVIOR_DEFAULT);
    int o[3] = {-9, -9, -9};
    HANDLE t = CreateThread(NULL, 0, hosting_thread, o, 0, NULL);
    CHECK(t != NULL);
    if (t) { CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0); CloseHandle(t); }
    EXPECT_EQ(o[0], 0);
    EXPECT_EQ(o[1], 0);
    EXPECT_EQ(o[2], 1);
    EXPECT_EQ(U(GetThreadDpiHostingBehavior)(), DPI_HOSTING_BEHAVIOR_DEFAULT);
    U(SetThreadDpiHostingBehavior)(orig);
}
TEST(gap1_hosting_window_inherits_note) {
    REQUIRE(U(SetThreadDpiHostingBehavior)); REQUIRE(U(GetThreadDpiHostingBehavior)); REQUIRE(U(GetWindowDpiHostingBehavior));
    DPI_HOSTING_BEHAVIOR orig = U(GetThreadDpiHostingBehavior)();
    U(SetThreadDpiHostingBehavior)(DPI_HOSTING_BEHAVIOR_MIXED);
    HWND h = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 20, 20, NULL);
    U(SetThreadDpiHostingBehavior)(DPI_HOSTING_BEHAVIOR_DEFAULT);
    HWND h2 = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 20, 20, NULL);
    if (h) {
        int b = (int)U(GetWindowDpiHostingBehavior)(h);
        if (b != 1) t_note("window created in MIXED thread reports hosting %d", b);
        U(DestroyWindow)(h);
    }
    if (h2) {
        int b = (int)U(GetWindowDpiHostingBehavior)(h2);
        CHECKF(b == 0, "default hosting window reports %d", b);
        U(DestroyWindow)(h2);
    }
    U(SetThreadDpiHostingBehavior)(orig);
}
TEST(gap1_hosting_stress_toggle) {
    REQUIRE(U(SetThreadDpiHostingBehavior)); REQUIRE(U(GetThreadDpiHostingBehavior));
    DPI_HOSTING_BEHAVIOR orig = U(GetThreadDpiHostingBehavior)();
    for (int i = 0; i < 1000; i++) {
        DPI_HOSTING_BEHAVIOR v = (DPI_HOSTING_BEHAVIOR)(i & 1);
        U(SetThreadDpiHostingBehavior)(v);
        if (U(GetThreadDpiHostingBehavior)() != v) { CHECK(0); break; }
    }
    U(SetThreadDpiHostingBehavior)(orig);
    CHECK(1);
}
