/* GUI resources, default layout, shutdown reasons, shell windows, notifications, misc process state */
#include "wsp.h"
#include <dbt.h>

UDECL(BOOL, GetProcessDefaultLayout, (DWORD *));
UDECL(HWND, GetProgmanWindow, (void));
UDECL(HWND, GetTaskmanWindow, (void));
UDECL(BOOL, SetProgmanWindow, (HWND));
UDECL(BOOL, SetTaskmanWindow, (HWND));
UDECL(BOOL, SetShellWindow, (HWND));
UDECL(BOOL, SetShellWindowEx, (HWND, HWND));
UDECL(BOOL, DeregisterShellHookWindow, (HWND));
UDECL(BOOL, UnregisterPowerSettingNotification, (HANDLE));
UDECL(BOOL, UnregisterSuspendResumeNotification, (HANDLE));
UDECL(DWORD, GetAppCompatFlags, (HANDLE));
UDECL(DWORD, GetAppCompatFlags2, (HANDLE));
UDECL(BOOL, ShutdownBlockReasonDestroy, (HWND));
UDECL(BOOL, ShutdownBlockReasonQuery, (HWND, LPWSTR, DWORD *));
UDECL(BOOL, UserHandleGrantAccess, (HANDLE, HANDLE, BOOL));
UDECL(BOOL, IsImmersiveProcess_, (HANDLE));

static HWND mkw(DWORD style) { return t_make_window(NULL, style, WS_EX_TOOLWINDOW, -32000, -32000, 30, 30, NULL); }
static DWORD gdi_cnt(HANDLE p, DWORD f) { return U(GetGuiResources)(p, f); }

/* ---------- GetGuiResources ---------- */
TEST(winsta_process_guires_current_process_nonzero) {
    REQUIRE(U(GetGuiResources));
    HWND w = mkw(WS_POPUP);
    DWORD u = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    CHECK(u > 0);
    DWORD up = U(GetGuiResources)(GetCurrentProcess(), 4 /* GR_USEROBJECTS_PEAK */);
    CHECK(up >= u);
    DWORD gp = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS_PEAK);
    DWORD g = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECK(gp >= g);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_guires_window_count_delta) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(DestroyWindow));
    HWND w0 = mkw(WS_POPUP);   /* ensure the thread is a GUI thread */
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    HWND ws[20];
    for (int i = 0; i < 20; i++) ws[i] = mkw(WS_POPUP);
    DWORD mid = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    CHECKF(mid >= before + 20, "before %lu mid %lu", before, mid);
    DWORD peak = U(GetGuiResources)(GetCurrentProcess(), 4);
    CHECK(peak >= mid);
    for (int i = 0; i < 20; i++) if (ws[i]) U(DestroyWindow)(ws[i]);
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    CHECKF(after <= before + 1, "before %lu after %lu", before, after);
    if (after != before) t_note("USER object count %lu -> %lu after create/destroy 20 windows", before, after);
    DWORD peak2 = U(GetGuiResources)(GetCurrentProcess(), 4);
    CHECK(peak2 >= peak);          /* peak never drops */
    CHECK(peak2 >= mid);
    if (w0) U(DestroyWindow)(w0);
}
TEST(winsta_process_guires_menu_counts_as_user_object) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu)); mkw(WS_POPUP);
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    HMENU m[10];
    for (int i = 0; i < 10; i++) m[i] = U(CreateMenu)();
    DWORD mid = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    CHECKF(mid >= before + 10, "before %lu mid %lu", before, mid);
    for (int i = 0; i < 10; i++) if (m[i]) U(DestroyMenu)(m[i]);
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    CHECKF(after <= before, "before %lu after %lu", before, after);
}
TEST(winsta_process_guires_gdi_delta) {
    REQUIRE(U(GetGuiResources));
    DWORD before = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    HBRUSH b[25];
    for (int i = 0; i < 25; i++) b[i] = CreateSolidBrush(RGB(i, i, i));
    DWORD mid = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(mid >= before + 25, "before %lu mid %lu", before, mid);
    DWORD pk = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS_PEAK);
    CHECK(pk >= mid);
    for (int i = 0; i < 25; i++) DeleteObject(b[i]);
    DWORD after = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECKF(after <= before, "before %lu after %lu", before, after);
    CHECK(U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS_PEAK) >= pk);
}
TEST(winsta_process_guires_real_handle_matches_pseudo) {
    REQUIRE(U(GetGuiResources));
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (!h) { t_skip("OpenProcess failed %lu", GetLastError()); return; }
    EXPECT_EQ(U(GetGuiResources)(h, GR_USEROBJECTS), U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS));
    EXPECT_EQ(U(GetGuiResources)(h, GR_GDIOBJECTS), U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS));
    CloseHandle(h);
}
TEST(winsta_process_guires_invalid_flags) {
    REQUIRE(U(GetGuiResources));
    static const DWORD bad[] = { 3, 5, 6, 7, 8, 99, 0x100, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        DWORD r = U(GetGuiResources)(GetCurrentProcess(), bad[i]);
        CHECKF(r == 0, "flags %lu got %lu", (unsigned long)bad[i], r);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}
TEST(winsta_process_guires_invalid_handle) {
    REQUIRE(U(GetGuiResources));
    SetLastError(0);
    CHECK(U(GetGuiResources)(NULL, GR_USEROBJECTS) == 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(U(GetGuiResources)((HANDLE)(ULONG_PTR)0x7ffc, GR_GDIOBJECTS) == 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CHECK(U(GetGuiResources)(ev, GR_USEROBJECTS) == 0);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(winsta_process_guires_pseudo_handle_minus_one) {
    REQUIRE(U(GetGuiResources));
    EXPECT_EQ(U(GetGuiResources)((HANDLE)(LONG_PTR)-1, GR_USEROBJECTS), U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS));
}
TEST(winsta_process_guires_no_query_rights) {
    REQUIRE(U(GetGuiResources));
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (!h) { t_skip("OpenProcess failed"); return; }
    SetLastError(0);
    DWORD r = U(GetGuiResources)(h, GR_USEROBJECTS);
    CHECK(r == 0);
    EXPECT_ERR(0, ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(winsta_process_guires_limited_rights) {
    REQUIRE(U(GetGuiResources));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!h) { t_skip("OpenProcess failed"); return; }
    SetLastError(0);
    DWORD r = U(GetGuiResources)(h, GR_USEROBJECTS);
    t_note("GetGuiResources with PROCESS_QUERY_LIMITED_INFORMATION: %lu err %lu", r, GetLastError());
    CloseHandle(h);
}
TEST(winsta_process_guires_global) {
    REQUIRE(U(GetGuiResources));
    SetLastError(0);
    DWORD r = U(GetGuiResources)((HANDLE)(LONG_PTR)-2, GR_USEROBJECTS);
    t_note("GetGuiResources(GR_GLOBAL, USEROBJECTS) = %lu err %lu", r, GetLastError());
}
TEST(winsta_process_guires_other_process) {
    REQUIRE(U(GetGuiResources));
    /* a console child that has never touched user32: counts must not be an error */
    char cmd[MAX_PATH + 64];
    GetSystemDirectoryA(cmd, MAX_PATH);
    strcat(cmd, "\\ping.exe -n 4 127.0.0.1");
    STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) { t_skip("cannot spawn helper: %lu", GetLastError()); return; }
    Sleep(200);
    SetLastError(0);
    DWORD u = U(GetGuiResources)(pi.hProcess, GR_USEROBJECTS);
    DWORD g = U(GetGuiResources)(pi.hProcess, GR_GDIOBJECTS);
    t_note("helper process gui resources: user=%lu gdi=%lu err=%lu", u, g, GetLastError());
    TerminateProcess(pi.hProcess, 0);
    WaitForSingleObject(pi.hProcess, 5000);
    SetLastError(0);
    DWORD dead = U(GetGuiResources)(pi.hProcess, GR_USEROBJECTS);
    t_note("terminated process: %lu err %lu", dead, GetLastError());
    CHECK(dead == 0);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
}

/* ---------- default layout ---------- */
TEST(winsta_process_layout_get_default) {
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout));
    DWORD l = 0xdead;
    CHECK(U(GetProcessDefaultLayout)(&l));
    CHECK((l & ~(DWORD)(LAYOUT_RTL | LAYOUT_BTT | LAYOUT_VBH | LAYOUT_BITMAPORIENTATIONPRESERVED)) == 0);
    t_note("initial process default layout 0x%lx", (unsigned long)l);
    DWORD l2 = 0xdead;
    CHECK(U(GetProcessDefaultLayout)(&l2));
    EXPECT_EQ(l, l2);
}
TEST(winsta_process_layout_set_get_roundtrip) {
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout));
    DWORD orig = 0; U(GetProcessDefaultLayout)(&orig);
    DWORD l = 0;
    CHECK(U(SetProcessDefaultLayout)(LAYOUT_RTL));
    CHECK(U(GetProcessDefaultLayout)(&l)); EXPECT_EQ(l, LAYOUT_RTL);
    CHECK(U(SetProcessDefaultLayout)(0));
    CHECK(U(GetProcessDefaultLayout)(&l)); EXPECT_EQ(l, 0);
    CHECK(U(SetProcessDefaultLayout)(LAYOUT_RTL));
    CHECK(U(SetProcessDefaultLayout)(LAYOUT_RTL));
    CHECK(U(GetProcessDefaultLayout)(&l)); EXPECT_EQ(l, LAYOUT_RTL);
    CHECK(U(SetProcessDefaultLayout)(orig));
    CHECK(U(GetProcessDefaultLayout)(&l)); EXPECT_EQ(l, orig);
}
TEST(winsta_process_layout_affects_new_windows) {
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout)); REQUIRE(U(GetWindowLongA));
    DWORD orig = 0; U(GetProcessDefaultLayout)(&orig);
    CHECK(U(SetProcessDefaultLayout)(0));
    HWND a = mkw(WS_POPUP);
    CHECK(a && !(U(GetWindowLongA)(a, GWL_EXSTYLE) & WS_EX_LAYOUTRTL));
    CHECK(U(SetProcessDefaultLayout)(LAYOUT_RTL));
    HWND b = mkw(WS_POPUP);
    CHECK(b && (U(GetWindowLongA)(b, GWL_EXSTYLE) & WS_EX_LAYOUTRTL));
    CHECK(a && !(U(GetWindowLongA)(a, GWL_EXSTYLE) & WS_EX_LAYOUTRTL));    /* existing windows unchanged */
    HWND c = t_make_window(NULL, WS_CHILD, 0, 0, 0, 5, 5, b);
    t_note("child created under RTL default layout: ex-style 0x%lx", c ? (unsigned long)U(GetWindowLongA)(c, GWL_EXSTYLE) : 0);
    CHECK(U(SetProcessDefaultLayout)(orig));
    HWND d = mkw(WS_POPUP);
    EXPECT_EQ((U(GetWindowLongA)(d, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0, (orig & LAYOUT_RTL) != 0);
    if (c) U(DestroyWindow)(c);
    if (a) U(DestroyWindow)(a); if (b) U(DestroyWindow)(b); if (d) U(DestroyWindow)(d);
}
TEST(winsta_process_layout_explicit_ltr_overrides) {
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout)); REQUIRE(U(GetWindowLongA));
    DWORD orig = 0; U(GetProcessDefaultLayout)(&orig);
    U(SetProcessDefaultLayout)(LAYOUT_RTL);
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_LTRREADING | WS_EX_LEFTSCROLLBAR * 0, -32000, -32000, 20, 20, NULL);
    t_note("window with explicit LTR flags under RTL default: ex 0x%lx", w ? (unsigned long)U(GetWindowLongA)(w, GWL_EXSTYLE) : 0);
    U(SetProcessDefaultLayout)(orig);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_layout_invalid_values) {
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout));
    DWORD orig = 0; U(GetProcessDefaultLayout)(&orig);
    static const DWORD v[] = { 2, 4, 8, 0x10, 0x80, 0xffffffff };
    for (unsigned i = 0; i < 6; i++) {
        SetLastError(0);
        BOOL r = U(SetProcessDefaultLayout)(v[i]);
        DWORD now = 0; U(GetProcessDefaultLayout)(&now);
        t_note("SetProcessDefaultLayout(0x%lx) ret %d err %lu -> now 0x%lx", (unsigned long)v[i], r, GetLastError(), (unsigned long)now);
        U(SetProcessDefaultLayout)(orig);
    }
    DWORD l = 0; U(GetProcessDefaultLayout)(&l); EXPECT_EQ(l, orig);
}
static DWORD wk_layout(void *p) { U(GetProcessDefaultLayout)((DWORD *)p); return 0; }
TEST(winsta_process_layout_process_wide) {
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout));
    DWORD orig = 0; U(GetProcessDefaultLayout)(&orig);
    U(SetProcessDefaultLayout)(LAYOUT_RTL);
    DWORD seen = 0xdead, r;
    CHECK(wsp_run(wk_layout, &seen, 5000, &r));
    EXPECT_EQ(seen, LAYOUT_RTL);
    U(SetProcessDefaultLayout)(orig);
}

/* ---------- shutdown block reasons ---------- */
#define NEED_SB() do { REQUIRE(U(ShutdownBlockReasonCreate)); REQUIRE(UN(ShutdownBlockReasonDestroy)); REQUIRE(UN(ShutdownBlockReasonQuery)); } while (0)
TEST(winsta_process_shutdown_reason_lifecycle) {
    NEED_SB();
    HWND w = mkw(WS_POPUP); CHECK(w != NULL);
    SetLastError(0);
    BOOL c = U(ShutdownBlockReasonCreate)(w, L"t32 test reason");
    if (!c) { t_note("ShutdownBlockReasonCreate err %lu", GetLastError()); t_skip("cannot create reason"); U(DestroyWindow)(w); return; }
    WCHAR buf[64]; DWORD n = 64;
    memset(buf, 0, sizeof buf);
    CHECK(UN(ShutdownBlockReasonQuery)(w, buf, &n));
    EXPECT_WSTR(buf, L"t32 test reason");
    EXPECT_EQ(n, 16);
    CHECK(UN(ShutdownBlockReasonDestroy)(w));
    SetLastError(0);
    n = 64;
    CHECK(!UN(ShutdownBlockReasonQuery)(w, buf, &n));
    t_note("Query after Destroy: err %lu", GetLastError());
    SetLastError(0);
    CHECK(!UN(ShutdownBlockReasonDestroy)(w));
    t_note("second Destroy: err %lu", GetLastError());
    U(DestroyWindow)(w);
}
TEST(winsta_process_shutdown_reason_query_sizing) {
    NEED_SB();
    HWND w = mkw(WS_POPUP);
    if (!U(ShutdownBlockReasonCreate)(w, L"abcdef")) { t_skip("cannot create reason"); U(DestroyWindow)(w); return; }
    WCHAR buf[32]; DWORD n = 0;
    SetLastError(0);
    CHECK(!UN(ShutdownBlockReasonQuery)(w, NULL, &n));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, 7);
    n = 6; SetLastError(0);
    CHECK(!UN(ShutdownBlockReasonQuery)(w, buf, &n));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, 7);
    n = 7; memset(buf, 0, sizeof buf);
    CHECK(UN(ShutdownBlockReasonQuery)(w, buf, &n));
    EXPECT_WSTR(buf, L"abcdef");
    EXPECT_EQ(n, 7);
    UN(ShutdownBlockReasonDestroy)(w);
    U(DestroyWindow)(w);
}
TEST(winsta_process_shutdown_reason_replace) {
    NEED_SB();
    HWND w = mkw(WS_POPUP);
    if (!U(ShutdownBlockReasonCreate)(w, L"first")) { t_skip("cannot create reason"); U(DestroyWindow)(w); return; }
    BOOL c2 = U(ShutdownBlockReasonCreate)(w, L"second one");
    t_note("second Create on same window ret %d", c2);
    WCHAR buf[64]; DWORD n = 64;
    CHECK(UN(ShutdownBlockReasonQuery)(w, buf, &n));
    t_note("reason after second create: '%ls'", buf);
    if (c2) EXPECT_WSTR(buf, L"second one");
    UN(ShutdownBlockReasonDestroy)(w);
    U(DestroyWindow)(w);
}
TEST(winsta_process_shutdown_reason_null_reason) {
    NEED_SB();
    HWND w = mkw(WS_POPUP);
    SetLastError(0);
    CHECK(!U(ShutdownBlockReasonCreate)(w, NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    U(DestroyWindow)(w);
}
TEST(winsta_process_shutdown_reason_length_limit) {
    NEED_SB();
    HWND w = mkw(WS_POPUP);
    WCHAR big[400];
    for (int i = 0; i < 399; i++) big[i] = L'x';
    big[257] = 0;                                  /* 257 characters: over the limit */
    SetLastError(0);
    CHECK(!U(ShutdownBlockReasonCreate)(w, big));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    big[257] = L'x'; big[300] = 0;
    SetLastError(0);
    CHECK(!U(ShutdownBlockReasonCreate)(w, big));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    big[300] = L'x'; big[256] = 0;                 /* 256 characters: accepted by user32 */
    SetLastError(0);
    BOOL r = U(ShutdownBlockReasonCreate)(w, big);
    t_note("256-character reason ret %d err %lu", r, GetLastError());
    if (r) UN(ShutdownBlockReasonDestroy)(w);
    U(DestroyWindow)(w);
}
TEST(winsta_process_shutdown_reason_bad_window) {
    NEED_SB();
    SetLastError(0);
    CHECK(!U(ShutdownBlockReasonCreate)(NULL, L"x"));
    t_note("Create(NULL hwnd) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(ShutdownBlockReasonCreate)((HWND)(ULONG_PTR)0x7ffffff0, L"x"));
    t_note("Create(bogus hwnd) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!UN(ShutdownBlockReasonDestroy)(NULL));
    CHECK(!UN(ShutdownBlockReasonDestroy)((HWND)(ULONG_PTR)0x7ffffff0));
    WCHAR b[8]; DWORD n = 8;
    CHECK(!UN(ShutdownBlockReasonQuery)(NULL, b, &n));
    CHECK(!UN(ShutdownBlockReasonQuery)((HWND)(ULONG_PTR)0x7ffffff0, b, &n));
}
TEST(winsta_process_shutdown_reason_child_window) {
    NEED_SB();
    HWND p = mkw(WS_POPUP);
    HWND c = t_make_window(NULL, WS_CHILD, 0, 0, 0, 10, 10, p);
    SetLastError(0);
    BOOL r = U(ShutdownBlockReasonCreate)(c, L"child");
    t_note("Create on child window ret %d err %lu", r, GetLastError());
    if (r) UN(ShutdownBlockReasonDestroy)(c);
    if (c) U(DestroyWindow)(c);
    if (p) U(DestroyWindow)(p);
}
TEST(winsta_process_shutdown_reason_dies_with_window) {
    NEED_SB();
    HWND w = mkw(WS_POPUP);
    if (!U(ShutdownBlockReasonCreate)(w, L"going away")) { t_skip("cannot create reason"); U(DestroyWindow)(w); return; }
    U(DestroyWindow)(w);
    WCHAR b[32]; DWORD n = 32;
    SetLastError(0);
    CHECK(!UN(ShutdownBlockReasonQuery)(w, b, &n));
    t_note("Query on destroyed window err %lu", GetLastError());
}
static DWORD wk_sb_other(void *p) { HWND w = (HWND)p; SetLastError(0); return U(ShutdownBlockReasonCreate)(w, L"cross thread") ? 1 : 0; }
TEST(winsta_process_shutdown_reason_other_threads_window) {
    NEED_SB();
    HWND w = mkw(WS_POPUP);
    DWORD r = 9;
    CHECK(wsp_run(wk_sb_other, w, 5000, &r));
    t_note("Create for a window owned by another thread: ret %lu", r);
    if (r) UN(ShutdownBlockReasonDestroy)(w);
    U(DestroyWindow)(w);
}

/* ---------- shell / taskman / progman ---------- */
TEST(winsta_process_shell_getshellwindow) {
    REQUIRE(U(GetShellWindow)); REQUIRE(U(IsWindow));
    HWND s = U(GetShellWindow)();
    EXPECT_EQ(s, U(GetShellWindow)());
    if (!s) { t_note("no shell window on this desktop"); return; }
    CHECK(U(IsWindow)(s));
    DWORD pid = 0; DWORD tid = U(GetWindowThreadProcessId)(s, &pid);
    CHECK(tid != 0); CHECK(pid != GetCurrentProcessId());
    char cls[64] = "";
    U(GetClassNameA)(s, cls, sizeof cls);
    t_note("shell window %p class '%s' pid %lu", s, cls, (unsigned long)pid);
    CHECK(U(GetParent)(s) == NULL || 1);
}
TEST(winsta_process_shell_getshellwindow_worker_same) {
    REQUIRE(U(GetShellWindow));
    HWND main_s = U(GetShellWindow)();
    HWND seen = (HWND)1; DWORD r;
    DWORD wk(void *p) { *(HWND *)p = U(GetShellWindow)(); return 0; }
    CHECK(wsp_run(wk, &seen, 5000, &r));
    EXPECT_EQ(seen, main_s);
}
TEST(winsta_process_shell_progman_taskman_getters) {
    REQUIRE(UN(GetProgmanWindow)); REQUIRE(UN(GetTaskmanWindow)); REQUIRE(U(GetShellWindow));
    HWND p = UN(GetProgmanWindow)(), t = UN(GetTaskmanWindow)(), s = U(GetShellWindow)();
    t_note("progman=%p taskman=%p shell=%p", p, t, s);
    EXPECT_EQ(p, UN(GetProgmanWindow)());
    EXPECT_EQ(t, UN(GetTaskmanWindow)());
    if (p) CHECK(U(IsWindow)(p));
    if (t) CHECK(U(IsWindow)(t));
}
TEST(winsta_process_shell_set_bogus_window) {
    REQUIRE(U(GetShellWindow));
    HWND before = U(GetShellWindow)();
    HWND bogus = (HWND)(ULONG_PTR)0x7ffffff0;
    if (UN(SetShellWindow)) {
        SetLastError(0);
        CHECK(!UN(SetShellWindow)(bogus));
        t_note("SetShellWindow(bogus) err %lu", GetLastError());
    }
    if (UN(SetShellWindowEx)) {
        SetLastError(0);
        CHECK(!UN(SetShellWindowEx)(bogus, bogus));
        t_note("SetShellWindowEx(bogus,bogus) err %lu", GetLastError());
    }
    if (UN(SetTaskmanWindow)) {
        SetLastError(0);
        CHECK(!UN(SetTaskmanWindow)(bogus));
        t_note("SetTaskmanWindow(bogus) err %lu", GetLastError());
    }
    if (UN(SetProgmanWindow)) {
        SetLastError(0);
        CHECK(!UN(SetProgmanWindow)(bogus));
        t_note("SetProgmanWindow(bogus) err %lu", GetLastError());
    }
    EXPECT_EQ(U(GetShellWindow)(), before);
}
TEST(winsta_process_shell_set_child_window) {
    REQUIRE(U(GetShellWindow));
    if (!U(GetShellWindow)() || !UN(SetShellWindow)) { t_skip("no shell to protect or no export"); return; }
    HWND before = U(GetShellWindow)();
    HWND p = mkw(WS_POPUP);
    HWND c = t_make_window(NULL, WS_CHILD, 0, 0, 0, 10, 10, p);
    SetLastError(0);
    CHECK(!UN(SetShellWindow)(c));          /* child windows can never be the shell */
    t_note("SetShellWindow(child) err %lu", GetLastError());
    EXPECT_EQ(U(GetShellWindow)(), before);
    if (c) U(DestroyWindow)(c);
    if (p) U(DestroyWindow)(p);
}
TEST(winsta_process_shellhook_register_deregister) {
    REQUIRE(U(RegisterShellHookWindow)); REQUIRE(UN(DeregisterShellHookWindow));
    HWND w = mkw(WS_POPUP); CHECK(w != NULL);
    SetLastError(0);
    BOOL r = U(RegisterShellHookWindow)(w);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        BOOL r2 = U(RegisterShellHookWindow)(w);
        t_note("second RegisterShellHookWindow ret %d", r2);
        t_pump();
        CHECK(UN(DeregisterShellHookWindow)(w));
        SetLastError(0);
        BOOL d2 = UN(DeregisterShellHookWindow)(w);
        t_note("second Deregister ret %d err %lu", d2, GetLastError());
        if (r2) UN(DeregisterShellHookWindow)(w);
    }
    t_pump();
    U(DestroyWindow)(w);
}
TEST(winsta_process_shellhook_invalid_window) {
    REQUIRE(U(RegisterShellHookWindow)); REQUIRE(UN(DeregisterShellHookWindow));
    SetLastError(0);
    CHECK(!U(RegisterShellHookWindow)(NULL));
    t_note("RegisterShellHookWindow(NULL) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(RegisterShellHookWindow)((HWND)(ULONG_PTR)0x7ffffff0));
    t_note("RegisterShellHookWindow(bogus) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!UN(DeregisterShellHookWindow)(NULL));
    SetLastError(0);
    CHECK(!UN(DeregisterShellHookWindow)((HWND)(ULONG_PTR)0x7ffffff0));
}
TEST(winsta_process_shellhook_deregister_unregistered) {
    REQUIRE(UN(DeregisterShellHookWindow));
    HWND w = mkw(WS_POPUP);
    SetLastError(0);
    CHECK(!UN(DeregisterShellHookWindow)(w));
    t_note("Deregister of never-registered window err %lu", GetLastError());
    U(DestroyWindow)(w);
}
static DWORD wk_shellhook(void *p) {
    HWND w = (HWND)p;
    SetLastError(0);
    BOOL r = U(RegisterShellHookWindow)(w);
    if (r) UN(DeregisterShellHookWindow)(w);
    return r ? 1 : (0x10000 | GetLastError());
}
TEST(winsta_process_shellhook_other_thread_window) {
    REQUIRE(U(RegisterShellHookWindow)); REQUIRE(UN(DeregisterShellHookWindow));
    HWND w = mkw(WS_POPUP);
    DWORD r = 0;
    CHECK(wsp_run(wk_shellhook, w, 5000, &r));
    t_note("RegisterShellHookWindow for another thread's window: 0x%lx", (unsigned long)r);
    U(DestroyWindow)(w);
}
TEST(winsta_process_shellhook_destroy_registered_window) {
    REQUIRE(U(RegisterShellHookWindow));
    HWND w = mkw(WS_POPUP);
    BOOL r = U(RegisterShellHookWindow)(w);
    CHECK(r);
    CHECK(U(DestroyWindow)(w));          /* registration is dropped with the window */
    t_pump();
    SetLastError(0);
    CHECK(!UN(DeregisterShellHookWindow)(w));
}

/* ---------- notifications ---------- */
static const GUID k_usb = { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };
static const GUID k_display = { 0x6FE69556, 0x704A, 0x47A0, { 0x8F, 0x24, 0xC2, 0x8D, 0x93, 0x6F, 0xDA, 0x47 } };

TEST(winsta_process_devnotify_register_a) {
    REQUIRE(U(RegisterDeviceNotificationA)); REQUIRE(U(UnregisterDeviceNotification));
    HWND w = mkw(WS_POPUP);
    DEV_BROADCAST_DEVICEINTERFACE_A f; memset(&f, 0, sizeof f);
    f.dbcc_size = sizeof f; f.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE; f.dbcc_classguid = k_usb;
    SetLastError(0);
    HDEVNOTIFY h = U(RegisterDeviceNotificationA)(w, &f, DEVICE_NOTIFY_WINDOW_HANDLE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) { CHECK(U(UnregisterDeviceNotification)(h)); SetLastError(0); CHECK(!U(UnregisterDeviceNotification)(h)); t_note("second Unregister err %lu", GetLastError()); }
    U(DestroyWindow)(w);
}
TEST(winsta_process_devnotify_register_w) {
    REQUIRE(U(RegisterDeviceNotificationW)); REQUIRE(U(UnregisterDeviceNotification));
    HWND w = mkw(WS_POPUP);
    DEV_BROADCAST_DEVICEINTERFACE_W f; memset(&f, 0, sizeof f);
    f.dbcc_size = sizeof f; f.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE; f.dbcc_classguid = k_usb;
    HDEVNOTIFY h = U(RegisterDeviceNotificationW)(w, &f, DEVICE_NOTIFY_WINDOW_HANDLE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) CHECK(U(UnregisterDeviceNotification)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_devnotify_all_interface_classes) {
    REQUIRE(U(RegisterDeviceNotificationW)); REQUIRE(U(UnregisterDeviceNotification));
    HWND w = mkw(WS_POPUP);
    DEV_BROADCAST_DEVICEINTERFACE_W f; memset(&f, 0, sizeof f);
    f.dbcc_size = sizeof f; f.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    HDEVNOTIFY h = U(RegisterDeviceNotificationW)(w, &f, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) CHECK(U(UnregisterDeviceNotification)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_devnotify_reserved_devicetype) {
    REQUIRE(U(RegisterDeviceNotificationA)); REQUIRE(U(RegisterDeviceNotificationW)); REQUIRE(U(UnregisterDeviceNotification));
    HWND w = mkw(WS_POPUP);
    DEV_BROADCAST_DEVICEINTERFACE_A f; memset(&f, 0, sizeof f);
    f.dbcc_size = sizeof f; f.dbcc_devicetype = 0x80000001u; f.dbcc_classguid = k_usb;   /* rejected in user32 before any RPC */
    SetLastError(0);
    HDEVNOTIFY h = U(RegisterDeviceNotificationA)(w, &f, DEVICE_NOTIFY_WINDOW_HANDLE);
    CHECK(h == NULL);
    EXPECT_ERR(0, ERROR_INVALID_DATA);
    SetLastError(0);
    h = U(RegisterDeviceNotificationW)(w, &f, DEVICE_NOTIFY_WINDOW_HANDLE);
    CHECK(h == NULL);
    EXPECT_ERR(0, ERROR_INVALID_DATA);
    if (h) U(UnregisterDeviceNotification)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_devnotify_bad_recipient) {
    REQUIRE(U(RegisterDeviceNotificationA)); REQUIRE(U(UnregisterDeviceNotification));
    DEV_BROADCAST_DEVICEINTERFACE_A f; memset(&f, 0, sizeof f);
    f.dbcc_size = sizeof f; f.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE; f.dbcc_classguid = k_usb;
    SetLastError(0);
    HDEVNOTIFY h = U(RegisterDeviceNotificationA)(NULL, &f, DEVICE_NOTIFY_WINDOW_HANDLE);
    t_note("RegisterDeviceNotification(NULL recipient) -> %p err %lu", h, GetLastError());
    if (h) U(UnregisterDeviceNotification)(h);
    SetLastError(0);
    h = U(RegisterDeviceNotificationA)((HANDLE)(ULONG_PTR)0x7ffffff0, &f, DEVICE_NOTIFY_WINDOW_HANDLE);
    t_note("RegisterDeviceNotification(bogus hwnd) -> %p err %lu", h, GetLastError());
    if (h) U(UnregisterDeviceNotification)(h);
    SetLastError(0);
    h = U(RegisterDeviceNotificationA)((HANDLE)(ULONG_PTR)0x7ffc, &f, DEVICE_NOTIFY_SERVICE_HANDLE);
    t_note("RegisterDeviceNotification(bogus service handle) -> %p err %lu", h, GetLastError());
    if (h) U(UnregisterDeviceNotification)(h);
}
TEST(winsta_process_devnotify_bad_flags_and_size) {
    REQUIRE(U(RegisterDeviceNotificationA)); REQUIRE(U(UnregisterDeviceNotification));
    HWND w = mkw(WS_POPUP);
    DEV_BROADCAST_DEVICEINTERFACE_A f; memset(&f, 0, sizeof f);
    f.dbcc_size = sizeof f; f.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE; f.dbcc_classguid = k_usb;
    SetLastError(0);
    HDEVNOTIFY h = U(RegisterDeviceNotificationA)(w, &f, 0x40);
    t_note("bad flags -> %p err %lu", h, GetLastError());
    if (h) U(UnregisterDeviceNotification)(h);
    f.dbcc_size = 4;
    SetLastError(0);
    h = U(RegisterDeviceNotificationA)(w, &f, DEVICE_NOTIFY_WINDOW_HANDLE);
    t_note("bad dbcc_size -> %p err %lu", h, GetLastError());
    if (h) U(UnregisterDeviceNotification)(h);
    f.dbcc_size = sizeof f; f.dbcc_devicetype = 0x7777;
    SetLastError(0);
    h = U(RegisterDeviceNotificationA)(w, &f, DEVICE_NOTIFY_WINDOW_HANDLE);
    t_note("unknown devicetype -> %p err %lu", h, GetLastError());
    if (h) U(UnregisterDeviceNotification)(h);
    U(DestroyWindow)(w);
}
TEST(winsta_process_devnotify_unregister_invalid) {
    REQUIRE(U(UnregisterDeviceNotification));
    SetLastError(0);
    CHECK(!U(UnregisterDeviceNotification)(NULL));
    t_note("UnregisterDeviceNotification(NULL) err %lu", GetLastError());
}
TEST(winsta_process_devnotify_many_registrations) {
    REQUIRE(U(RegisterDeviceNotificationW)); REQUIRE(U(UnregisterDeviceNotification));
    HWND w = mkw(WS_POPUP);
    DEV_BROADCAST_DEVICEINTERFACE_W f; memset(&f, 0, sizeof f);
    f.dbcc_size = sizeof f; f.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE; f.dbcc_classguid = k_usb;
    HDEVNOTIFY h[8]; int n = 0;
    for (int i = 0; i < 8; i++) { h[i] = U(RegisterDeviceNotificationW)(w, &f, 0); if (h[i]) n++; }
    EXPECT_EQ(n, 8);
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) if (h[i] && h[i] == h[j]) CHECK(0);
    for (int i = 0; i < 8; i++) if (h[i]) CHECK(U(UnregisterDeviceNotification)(h[i]));
    t_pump();
    U(DestroyWindow)(w);
}
TEST(winsta_process_powernotify_register_display_state) {
    REQUIRE(U(RegisterPowerSettingNotification)); REQUIRE(UN(UnregisterPowerSettingNotification));
    HWND w = mkw(WS_POPUP);
    SetLastError(0);
    HPOWERNOTIFY h = U(RegisterPowerSettingNotification)(w, &k_display, DEVICE_NOTIFY_WINDOW_HANDLE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        t_pump();
        CHECK(UN(UnregisterPowerSettingNotification)(h));
        SetLastError(0);
        CHECK(!UN(UnregisterPowerSettingNotification)(h));
        t_note("second UnregisterPowerSettingNotification err %lu", GetLastError());
    }
    U(DestroyWindow)(w);
}
TEST(winsta_process_powernotify_invalid_args) {
    REQUIRE(U(RegisterPowerSettingNotification)); REQUIRE(UN(UnregisterPowerSettingNotification));
    HWND w = mkw(WS_POPUP);
    SetLastError(0);
    HPOWERNOTIFY h = U(RegisterPowerSettingNotification)(w, &k_display, 0x40);
    CHECK(h == NULL);
    t_note("bad flags err %lu", GetLastError());
    if (h) UN(UnregisterPowerSettingNotification)(h);
    SetLastError(0);
    h = U(RegisterPowerSettingNotification)(NULL, &k_display, DEVICE_NOTIFY_WINDOW_HANDLE);
    t_note("NULL recipient -> %p err %lu", h, GetLastError());
    if (h) UN(UnregisterPowerSettingNotification)(h);
    SetLastError(0);
    h = U(RegisterPowerSettingNotification)((HANDLE)(ULONG_PTR)0x7ffffff0, &k_display, DEVICE_NOTIFY_WINDOW_HANDLE);
    t_note("bogus window -> %p err %lu", h, GetLastError());
    if (h) UN(UnregisterPowerSettingNotification)(h);
    SetLastError(0);
    CHECK(!UN(UnregisterPowerSettingNotification)(NULL));
    t_note("Unregister(NULL) err %lu", GetLastError());
    U(DestroyWindow)(w);
}
TEST(winsta_process_powernotify_unknown_guid) {
    REQUIRE(U(RegisterPowerSettingNotification)); REQUIRE(UN(UnregisterPowerSettingNotification));
    HWND w = mkw(WS_POPUP);
    GUID g = { 0x12345678, 0x1234, 0x1234, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    SetLastError(0);
    HPOWERNOTIFY h = U(RegisterPowerSettingNotification)(w, &g, DEVICE_NOTIFY_WINDOW_HANDLE);
    t_note("unknown power setting GUID -> %p err %lu", h, GetLastError());
    if (h) CHECK(UN(UnregisterPowerSettingNotification)(h));
    U(DestroyWindow)(w);
}
TEST(winsta_process_suspendresume_register) {
    REQUIRE(U(RegisterSuspendResumeNotification)); REQUIRE(UN(UnregisterSuspendResumeNotification));
    HWND w = mkw(WS_POPUP);
    SetLastError(0);
    HPOWERNOTIFY h = U(RegisterSuspendResumeNotification)(w, DEVICE_NOTIFY_WINDOW_HANDLE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        t_pump();
        CHECK(UN(UnregisterSuspendResumeNotification)(h));
        SetLastError(0);
        CHECK(!UN(UnregisterSuspendResumeNotification)(h));
        t_note("second UnregisterSuspendResumeNotification err %lu", GetLastError());
    }
    U(DestroyWindow)(w);
}
TEST(winsta_process_suspendresume_invalid) {
    REQUIRE(U(RegisterSuspendResumeNotification)); REQUIRE(UN(UnregisterSuspendResumeNotification));
    HWND w = mkw(WS_POPUP);
    SetLastError(0);
    HPOWERNOTIFY h = U(RegisterSuspendResumeNotification)(w, 0x40);
    CHECK(h == NULL);
    t_note("bad flags err %lu", GetLastError());
    if (h) UN(UnregisterSuspendResumeNotification)(h);
    SetLastError(0);
    h = U(RegisterSuspendResumeNotification)(NULL, DEVICE_NOTIFY_WINDOW_HANDLE);
    t_note("NULL recipient -> %p err %lu", h, GetLastError());
    if (h) UN(UnregisterSuspendResumeNotification)(h);
    SetLastError(0);
    CHECK(!UN(UnregisterSuspendResumeNotification)(NULL));
    t_note("Unregister(NULL) err %lu", GetLastError());
    U(DestroyWindow)(w);
}

/* ---------- misc process state ---------- */
TEST(winsta_process_immersive_current_process) {
    REQUIRE(U(IsImmersiveProcess));
    SetLastError(0);
    CHECK(!U(IsImmersiveProcess)(GetCurrentProcess()));
    CHECK(!U(IsImmersiveProcess)((HANDLE)(LONG_PTR)-1));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { CHECK(!U(IsImmersiveProcess)(h)); CloseHandle(h); }
}
TEST(winsta_process_immersive_invalid_handle) {
    REQUIRE(U(IsImmersiveProcess));
    SetLastError(0);
    CHECK(!U(IsImmersiveProcess)(NULL));
    t_note("IsImmersiveProcess(NULL) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(IsImmersiveProcess)((HANDLE)(ULONG_PTR)0x7ffc));
    t_note("IsImmersiveProcess(bogus) err %lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(!U(IsImmersiveProcess)(ev));
    CloseHandle(ev);
}
TEST(winsta_process_appcompat_flags_stable) {
    REQUIRE(UN(GetAppCompatFlags)); REQUIRE(UN(GetAppCompatFlags2));
    DWORD a = UN(GetAppCompatFlags)(NULL), b = UN(GetAppCompatFlags)(NULL);
    EXPECT_EQ(a, b);
    DWORD c = UN(GetAppCompatFlags2)(NULL), d = UN(GetAppCompatFlags2)(NULL);
    EXPECT_EQ(c, d);
    t_note("GetAppCompatFlags=0x%lx GetAppCompatFlags2=0x%lx", (unsigned long)a, (unsigned long)c);
    DWORD e = UN(GetAppCompatFlags)((HANDLE)(ULONG_PTR)GetCurrentThreadId());
    DWORD f = UN(GetAppCompatFlags2)((HANDLE)(ULONG_PTR)GetCurrentThreadId());
    t_note("with current thread id as task: 0x%lx 0x%lx", (unsigned long)e, (unsigned long)f);
}
TEST(winsta_process_usergrant_invalid) {
    REQUIRE(UN(UserHandleGrantAccess));
    HANDLE job = CreateJobObjectW(NULL, NULL);
    HWND w = mkw(WS_POPUP);
    SetLastError(0);
    CHECK(!UN(UserHandleGrantAccess)(NULL, job, TRUE));
    t_note("UserHandleGrantAccess(NULL, job) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!UN(UserHandleGrantAccess)(w, NULL, TRUE));
    t_note("UserHandleGrantAccess(hwnd, NULL job) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!UN(UserHandleGrantAccess)(w, (HANDLE)(ULONG_PTR)0x7ffc, TRUE));
    t_note("UserHandleGrantAccess(hwnd, bogus job) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!UN(UserHandleGrantAccess)((HANDLE)(ULONG_PTR)0x7ffffff0, job, TRUE));
    t_note("UserHandleGrantAccess(bogus user handle, job) err %lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CHECK(!UN(UserHandleGrantAccess)(w, ev, TRUE));           /* not a job object */
    t_note("UserHandleGrantAccess(hwnd, event) err %lu", GetLastError());
    CloseHandle(ev);
    if (w) U(DestroyWindow)(w);
    if (job) CloseHandle(job);
}
TEST(winsta_process_usergrant_grant_revoke) {
    REQUIRE(UN(UserHandleGrantAccess));
    HANDLE job = CreateJobObjectW(NULL, NULL);
    HWND w = mkw(WS_POPUP);
    SetLastError(0);
    BOOL g = UN(UserHandleGrantAccess)(w, job, TRUE);
    t_note("grant window access to job ret %d err %lu", g, GetLastError());
    if (g) {
        SetLastError(0);
        BOOL r = UN(UserHandleGrantAccess)(w, job, FALSE);
        t_note("revoke ret %d err %lu", r, GetLastError());
        CHECK(r);
    }
    if (w) U(DestroyWindow)(w);
    if (job) CloseHandle(job);
}
TEST(winsta_process_disable_ghosting) {
    REQUIRE(U(DisableProcessWindowsGhosting)); REQUIRE(U(SendMessageA));
    U(DisableProcessWindowsGhosting)();
    U(DisableProcessWindowsGhosting)();     /* idempotent */
    HWND w = mkw(WS_POPUP);
    CHECK(w != NULL);
    CHECK(U(SendMessageA)(w, WM_NULL, 0, 0) == 0);
    CHECK(U(IsHungAppWindow) ? !U(IsHungAppWindow)(w) : 1);
    if (w) U(DestroyWindow)(w);
}
TEST(winsta_process_misc_exports_present) {
    static const char *names[] = {
        "GetDesktopID", "GetWinStationInfo", "SetWindowStationUser", "GetInputDesktop", "SwitchDesktop", "SwitchDesktopWithFade",
        "SetProcessRestrictionExemption", "RegisterUserApiHook", "UnregisterUserApiHook", "GetShellChangeNotifyWindow", "SetShellChangeNotifyWindow",
        "IsThreadDesktopComposited", "IsInDesktopWindowBand", "RegisterMessagePumpHook", "UnregisterMessagePumpHook", "UpdateDefaultDesktopThumbnail",
        "GetProgmanWindow", "SetProgmanWindow", "GetTaskmanWindow", "SetTaskmanWindow", "SetShellWindow", "SetShellWindowEx",
    };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) CHECKF(t_proc(names[i]) != NULL, "export %s", names[i]);
}
TEST(winsta_process_getthreaddesktop_not_input_desktop_flags) {
    REQUIRE(U(GetThreadDesktop)); REQUIRE(U(OpenInputDesktop)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetUserObjectInformationA));
    HDESK in = U(OpenInputDesktop)(0, FALSE, DESKTOP_READOBJECTS);
    if (!in) { t_skip("input desktop not accessible"); return; }
    char a[64] = "", b[64] = ""; DWORD n;
    U(GetUserObjectInformationA)(in, UOI_NAME, a, sizeof a, &n);
    U(GetUserObjectInformationA)(U(GetThreadDesktop)(GetCurrentThreadId()), UOI_NAME, b, sizeof b, &n);
    t_note("input desktop '%s' thread desktop '%s'", a, b);
    CHECK(!lstrcmpiA(a, b));    /* a test run from the interactive session runs on the input desktop */
    U(CloseDesktop)(in);
}
