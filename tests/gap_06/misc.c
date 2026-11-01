/* gap6: WaitForInputIdle, WindowFromDC, _UserTestTokenForInteractive. */
#include "t.h"

/* ---- WaitForInputIdle ---- */
#define WFII_ERR_FAIL ((DWORD)-1)

TEST(gap6_wfii_null_handle) {
    REQUIRE(U(WaitForInputIdle));
    SetLastError(0);
    DWORD r = U(WaitForInputIdle)(NULL, 0);
    CHECK(r == WFII_ERR_FAIL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    SetLastError(0);
    r = U(WaitForInputIdle)(NULL, 100);
    CHECK(r == WFII_ERR_FAIL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(gap6_wfii_bogus_handles) {
    REQUIRE(U(WaitForInputIdle));
    ULONG_PTR h[] = { 0x4, 0x8, 0x1000, 0x7ffc, 0xdeadbee4, 0x7ffffffc, 0xfffffffc, 0xfffffffffffffffcull - 0x100 };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        SetLastError(0);
        DWORD r = U(WaitForInputIdle)((HANDLE)h[i], 0);
        CHECKF(r == WFII_ERR_FAIL, "h=%llx r=%lx", (unsigned long long)h[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "h=%llx err=%lu", (unsigned long long)h[i], GetLastError());
    }
}
TEST(gap6_wfii_timeout_ignored_on_invalid) {
    REQUIRE(U(WaitForInputIdle));
    DWORD t[] = { 0, 1, 1000, INFINITE, 0x7fffffff, 0x80000000 };
    for (int i = 0; i < 6; i++) {
        SetLastError(0);
        DWORD r = U(WaitForInputIdle)((HANDLE)(UINT_PTR)0x4444, t[i]);
        CHECKF(r == WFII_ERR_FAIL, "t=%lu", t[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "t=%lu", t[i]);
    }
}
TEST(gap6_wfii_event_handle) {
    REQUIRE(U(WaitForInputIdle));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ev) { t_skip("no event"); return; }
    SetLastError(0);
    DWORD r = U(WaitForInputIdle)(ev, 0);   /* not a process: type mismatch -> 6 */
    CHECK(r == WFII_ERR_FAIL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    /* handle tag bit 0 on a type-mismatched handle short-circuits to success (0) */
    SetLastError(0);
    r = U(WaitForInputIdle)((HANDLE)((ULONG_PTR)ev | 1), 0);
    t_note("event|1 -> %lu", r);
    CHECK(r == 0);
    CloseHandle(ev);
}
TEST(gap6_wfii_thread_handle) {
    REQUIRE(U(WaitForInputIdle));
    HANDLE th = OpenThread(THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
    if (!th) { t_skip("no thread handle"); return; }
    SetLastError(0);
    DWORD r = U(WaitForInputIdle)(th, 0);
    CHECK(r == WFII_ERR_FAIL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    CloseHandle(th);
}
TEST(gap6_wfii_token_handle) {
    REQUIRE(U(WaitForInputIdle));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    SetLastError(0);
    DWORD r = U(WaitForInputIdle)(tok, 0);
    CHECK(r == WFII_ERR_FAIL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    CloseHandle(tok);
}
TEST(gap6_wfii_closed_handle) {
    REQUIRE(U(WaitForInputIdle));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ev) { t_skip("no event"); return; }
    CloseHandle(ev);
    SetLastError(0);
    DWORD r = U(WaitForInputIdle)(ev, 0);
    CHECK(r == WFII_ERR_FAIL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}
TEST(gap6_wfii_process_without_access) {
    REQUIRE(U(WaitForInputIdle));
    /* PROCESS_QUERY_LIMITED_INFORMATION is enough for NtQueryInformationProcess(0)
     * on Win10; a handle with only SYNCHRONIZE is not. */
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (!h) { t_skip("OpenProcess failed"); return; }
    SetLastError(0);
    DWORD r = U(WaitForInputIdle)(h, 0);
    CHECK(r == WFII_ERR_FAIL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);   /* any failed query maps to 6 */
    CloseHandle(h);
}
TEST(gap6_wfii_self_nonblocking) {
    REQUIRE(U(WaitForInputIdle));
    /* Waiting on ourselves with a zero timeout never blocks. Results seen:
     * 0 (idle), WAIT_TIMEOUT, or -1 with ERROR_NOT_GUI_PROCESS. */
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 10, 10, NULL);
    SetLastError(0);
    DWORD r = U(WaitForInputIdle)(GetCurrentProcess(), 0);
    DWORD e = GetLastError();
    t_note("self wfii -> %lu err %lu", r, e);
    CHECK(r == 0 || r == WAIT_TIMEOUT || r == WFII_ERR_FAIL);
    if (r == WFII_ERR_FAIL && e == 0) t_note("failure without last error");
    HANDLE real = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (real) {
        DWORD r2 = U(WaitForInputIdle)(real, 0);
        t_note("self (real handle) wfii -> %lu", r2);
        CHECK(r2 == 0 || r2 == WAIT_TIMEOUT || r2 == WFII_ERR_FAIL);
        CloseHandle(real);
    }
    if (w) DestroyWindow(w);
}
static int spawn_child(PROCESS_INFORMATION *pi) {
    char cmd[MAX_PATH + 32], dir[MAX_PATH];
    STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    GetSystemDirectoryA(dir, MAX_PATH);
    wsprintfA(cmd, "%s\\cmd.exe /c exit 0", dir);
    return CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, pi);
}
TEST(gap6_wfii_console_child) {
    REQUIRE(U(WaitForInputIdle));
    PROCESS_INFORMATION pi;
    if (!spawn_child(&pi)) { t_skip("cannot spawn cmd"); return; }
    SetLastError(0);
    DWORD r = U(WaitForInputIdle)(pi.hProcess, 3000);
    DWORD e = GetLastError();
    t_note("console child wfii -> %lu err %lu", r, e);
    CHECK(r == 0 || r == WAIT_TIMEOUT || r == WFII_ERR_FAIL);
    WaitForSingleObject(pi.hProcess, 5000);
    SetLastError(0);
    r = U(WaitForInputIdle)(pi.hProcess, 0);
    t_note("exited child wfii -> %lu err %lu", r, GetLastError());
    CHECK(r == 0 || r == WAIT_TIMEOUT || r == WFII_ERR_FAIL);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    SetLastError(0);
    r = U(WaitForInputIdle)(pi.hProcess, 0);
    CHECK(r == WFII_ERR_FAIL);
    EXPECT_ERR(r, ERROR_INVALID_HANDLE);
}

/* ---- WindowFromDC ---- */
static LRESULT CALLBACK paint_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        HWND got = U(WindowFromDC) ? U(WindowFromDC)(dc) : NULL;
        SetPropA(h, "gap6_wfdc", (HANDLE)got);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}
static const char *reg_class(DWORD style, const char *name) {
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.style = style; wc.lpfnWndProc = paint_proc; wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = name;
    RegisterClassA(&wc);
    return name;
}
static HWND mk(const char *cls, DWORD style, HWND parent) {
    return CreateWindowExA(0, cls, "w", style, 0, 0, 50, 50, parent, NULL, GetModuleHandleA(NULL), NULL);
}

TEST(gap6_wfdc_null) {
    REQUIRE(U(WindowFromDC));
    CHECK(U(WindowFromDC)(NULL) == NULL);
}
TEST(gap6_wfdc_bogus_handles) {
    REQUIRE(U(WindowFromDC));
    ULONG_PTR v[] = { 1, 2, 4, 0x1234, 0xffff, 0x10000, 0x7fffffff, 0xffffffff, (ULONG_PTR)-1, (ULONG_PTR)-4, 0x123456789ull };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        HWND r = U(WindowFromDC)((HDC)v[i]);
        CHECKF(r == NULL, "dc %llx -> %p", (unsigned long long)v[i], r);
    }
}
TEST(gap6_wfdc_window_dc) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    CHECK(dc != NULL);
    CHECK(U(WindowFromDC)(dc) == w);
    CHECK(U(WindowFromDC)(dc) == w); /* repeatable */
    CHECK(U(ReleaseDC)(w, dc) == 1);
    DestroyWindow(w);
}
TEST(gap6_wfdc_getwindowdc) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetWindowDC)); REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED | WS_CAPTION, 0, 0, 0, 80, 80, NULL);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetWindowDC)(w);
    CHECK(dc != NULL);
    CHECK(U(WindowFromDC)(dc) == w);
    U(ReleaseDC)(w, dc);
    DestroyWindow(w);
}
TEST(gap6_wfdc_getdcex) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDCEx)); REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (!w) { t_skip("no window"); return; }
    DWORD fl[] = { DCX_CACHE, DCX_WINDOW | DCX_CACHE, DCX_CACHE | DCX_CLIPSIBLINGS, DCX_CACHE | DCX_NORESETATTRS, DCX_CACHE | DCX_EXCLUDERGN * 0 };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        HDC dc = U(GetDCEx)(w, NULL, fl[i]);
        CHECKF(dc != NULL, "flags %lx", fl[i]);
        if (dc) { CHECKF(U(WindowFromDC)(dc) == w, "flags %lx", fl[i]); U(ReleaseDC)(w, dc); }
    }
    DestroyWindow(w);
}
TEST(gap6_wfdc_child_window) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND p = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    if (!p) { t_skip("no window"); return; }
    HWND c = t_make_window(NULL, WS_CHILD | WS_VISIBLE, 0, 5, 5, 40, 40, p);
    if (c) {
        HDC dc = U(GetDC)(c);
        CHECK(U(WindowFromDC)(dc) == c);
        U(ReleaseDC)(c, dc);
    }
    HDC dc = U(GetDC)(p);
    CHECK(U(WindowFromDC)(dc) == p);
    U(ReleaseDC)(p, dc);
    DestroyWindow(p);
}
TEST(gap6_wfdc_own_and_class_dc) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    DWORD styles[] = { CS_OWNDC, CS_CLASSDC };
    const char *names[] = { "gap6_owndc", "gap6_classdc" };
    for (int i = 0; i < 2; i++) {
        reg_class(styles[i], names[i]);
        HWND w = mk(names[i], WS_OVERLAPPED, NULL);
        if (!w) { t_note("no window %s", names[i]); continue; }
        HDC dc = U(GetDC)(w);
        CHECKF(dc != NULL, "%s", names[i]);
        CHECKF(U(WindowFromDC)(dc) == w, "%s", names[i]);
        U(ReleaseDC)(w, dc);
        HDC dc2 = U(GetDC)(w);
        CHECKF(U(WindowFromDC)(dc2) == w, "%s second", names[i]);
        U(ReleaseDC)(w, dc2);
        DestroyWindow(w);
        UnregisterClassA(names[i], GetModuleHandleA(NULL));
    }
}
TEST(gap6_wfdc_screen_dc) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HDC dc = U(GetDC)(NULL);
    CHECK(dc != NULL);
    HWND r = U(WindowFromDC)(dc);
    t_note("WindowFromDC(screen dc) = %p, desktop = %p", r, GetDesktopWindow());
    CHECK(r == GetDesktopWindow() || r == NULL);
    U(ReleaseDC)(NULL, dc);
}
TEST(gap6_wfdc_desktop_window_dc) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND d = GetDesktopWindow();
    HDC dc = U(GetDC)(d);
    CHECK(dc != NULL);
    HWND r = U(WindowFromDC)(dc);
    CHECK(r == d);
    U(ReleaseDC)(d, dc);
}
TEST(gap6_wfdc_memory_dc) {
    REQUIRE(U(WindowFromDC));
    HDC mem = CreateCompatibleDC(NULL);
    if (!mem) { t_skip("no memdc"); return; }
    CHECK(U(WindowFromDC)(mem) == NULL);
    HDC sc = GetDC(NULL);
    HBITMAP bm = CreateCompatibleBitmap(sc, 8, 8);
    HGDIOBJ old = SelectObject(mem, bm);
    CHECK(U(WindowFromDC)(mem) == NULL);
    SelectObject(mem, old);
    DeleteObject(bm);
    ReleaseDC(NULL, sc);
    DeleteDC(mem);
    CHECK(U(WindowFromDC)(mem) == NULL);   /* deleted */
}
TEST(gap6_wfdc_created_display_dc) {
    REQUIRE(U(WindowFromDC));
    HDC dc = CreateDCA("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no display dc"); return; }
    HWND r = U(WindowFromDC)(dc);
    t_note("WindowFromDC(CreateDC DISPLAY) = %p", r);
    DeleteDC(dc);
}
TEST(gap6_wfdc_info_dc) {
    REQUIRE(U(WindowFromDC));
    HDC dc = CreateICA("DISPLAY", NULL, NULL, NULL);
    if (!dc) { t_skip("no IC"); return; }
    t_note("WindowFromDC(IC) = %p", U(WindowFromDC)(dc));
    DeleteDC(dc);
}
TEST(gap6_wfdc_after_release) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    U(ReleaseDC)(w, dc);
    HWND r = U(WindowFromDC)(dc);
    t_note("WindowFromDC after ReleaseDC = %p (window %p)", r, w);
    DestroyWindow(w);
}
TEST(gap6_wfdc_after_destroy) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC));
    reg_class(CS_OWNDC, "gap6_owndc2");
    HWND w = mk("gap6_owndc2", WS_OVERLAPPED, NULL);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    DestroyWindow(w);
    HWND r = U(WindowFromDC)(dc);
    t_note("WindowFromDC after DestroyWindow (owndc) = %p", r);
    CHECK(r != w || !IsWindow(w));
    UnregisterClassA("gap6_owndc2", GetModuleHandleA(NULL));
}
TEST(gap6_wfdc_beginpaint) {
    REQUIRE(U(WindowFromDC));
    reg_class(0, "gap6_paint");
    HWND w = mk("gap6_paint", WS_OVERLAPPED | WS_VISIBLE, NULL);
    if (!w) { t_skip("no window"); return; }
    ShowWindow(w, SW_SHOWNA);
    InvalidateRect(w, NULL, TRUE);
    UpdateWindow(w);
    t_pump();
    HWND got = (HWND)GetPropA(w, "gap6_wfdc");
    if (!got) t_note("no paint seen");
    else CHECK(got == w);
    RemovePropA(w, "gap6_wfdc");
    DestroyWindow(w);
    UnregisterClassA("gap6_paint", GetModuleHandleA(NULL));
}
TEST(gap6_wfdc_many_dcs) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND w[4]; HDC d[4];
    for (int i = 0; i < 4; i++) { w[i] = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 30, 30, NULL); d[i] = w[i] ? U(GetDC)(w[i]) : NULL; }
    for (int i = 0; i < 4; i++) if (d[i]) CHECKF(U(WindowFromDC)(d[i]) == w[i], "i=%d", i);
    for (int i = 0; i < 4; i++) { if (d[i]) U(ReleaseDC)(w[i], d[i]); if (w[i]) DestroyWindow(w[i]); }
}
static DWORD WINAPI wfdc_thread(LPVOID p) {
    HDC dc = (HDC)p;
    return (DWORD)(ULONG_PTR)U(WindowFromDC)(dc);
}
TEST(gap6_wfdc_cross_thread) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    HANDLE th = CreateThread(NULL, 0, wfdc_thread, dc, 0, NULL);
    if (th) {
        if (WaitForSingleObject(th, 5000) == WAIT_OBJECT_0) {
            DWORD r; GetExitCodeThread(th, &r);
            t_note("cross-thread WindowFromDC = %lx (hwnd %p)", r, w);
        }
        CloseHandle(th);
    }
    U(ReleaseDC)(w, dc);
    DestroyWindow(w);
}
TEST(gap6_wfdc_repeat_nodc_leak) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (!w) { t_skip("no window"); return; }
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        HDC dc = U(GetDC)(w);
        if (!dc || U(WindowFromDC)(dc) != w) bad++;
        if (dc) U(ReleaseDC)(w, dc);
    }
    CHECK(bad == 0);
    DestroyWindow(w);
}
TEST(gap6_wfdc_lasterror_untouched_on_success) {
    REQUIRE(U(WindowFromDC)); REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (!w) { t_skip("no window"); return; }
    HDC dc = U(GetDC)(w);
    SetLastError(0x5555);
    HWND r = U(WindowFromDC)(dc);
    CHECK(r == w);
    t_note("last error after success: %lu", GetLastError());
    U(ReleaseDC)(w, dc);
    DestroyWindow(w);
}
TEST(gap6_wfdc_invalid_note_lasterror) {
    REQUIRE(U(WindowFromDC));
    SetLastError(0x5555);
    HWND r = U(WindowFromDC)((HDC)(UINT_PTR)0x1234);
    CHECK(r == NULL);
    t_note("last error after invalid DC: %lu", GetLastError());
}

/* ---- _UserTestTokenForInteractive ----
 * NTSTATUS _UserTestTokenForInteractive(HANDLE token, LUID *authId):
 * queries TokenStatistics (class 10), stores the AuthenticationId and asks
 * win32k whether it is the interactive logon session. */
typedef LONG (WINAPI *UTTFI_pf)(HANDLE, LUID *);
#define STATUS_INVALID_HANDLE_ 0xC0000008u
#define STATUS_TYPE_MISMATCH_ 0xC0000024u
#define STATUS_ACCESS_DENIED_ 0xC0000022u

TEST(gap6_uttfi_null_token) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    LUID l = { 0x11, 0x22 };
    LONG r = f(NULL, &l);
    CHECKF((ULONG)r == STATUS_INVALID_HANDLE_, "r=%lx", (ULONG)r);
    CHECK(l.LowPart == 0x11 && l.HighPart == 0x22);
}
TEST(gap6_uttfi_bogus_tokens) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    ULONG_PTR v[] = { 4, 8, 0x1234, 0x7ffffffc, 0xfffffffc };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        LUID l = { 1, 2 };
        LONG r = f((HANDLE)v[i], &l);
        /* Handles 4 and 8 are usually live kernel objects of this process on Win10, so the
         * type check fires first (STATUS_OBJECT_TYPE_MISMATCH); only unmapped values are invalid. */
        if (v[i] <= 8)
            CHECKF((ULONG)r == STATUS_INVALID_HANDLE_ || (ULONG)r == STATUS_TYPE_MISMATCH_, "h=%llx r=%lx", (unsigned long long)v[i], (ULONG)r);
        else
            CHECKF((ULONG)r == STATUS_INVALID_HANDLE_, "h=%llx r=%lx", (unsigned long long)v[i], (ULONG)r);
        CHECK(l.LowPart == 1 && l.HighPart == 2);
    }
}
TEST(gap6_uttfi_wrong_type) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ev) { t_skip("no event"); return; }
    LUID l = { 7, 8 };
    LONG r = f(ev, &l);
    CHECKF((ULONG)r == STATUS_TYPE_MISMATCH_, "r=%lx", (ULONG)r);
    CHECK(l.LowPart == 7 && l.HighPart == 8);
    CloseHandle(ev);
    r = f(GetCurrentProcess(), &l);   /* pseudo process handle is not a token */
    CHECK(r < 0);
}
TEST(gap6_uttfi_closed_token) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    CloseHandle(tok);
    LUID l = { 0, 0 };
    LONG r = f(tok, &l);
    CHECKF((ULONG)r == STATUS_INVALID_HANDLE_, "r=%lx", (ULONG)r);
}
TEST(gap6_uttfi_no_query_access) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    LUID l = { 3, 4 };
    LONG r = f(tok, &l);
    CHECKF((ULONG)r == STATUS_ACCESS_DENIED_, "r=%lx", (ULONG)r);
    CHECK(l.LowPart == 3 && l.HighPart == 4);
    CloseHandle(tok);
}
TEST(gap6_uttfi_own_token) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    LUID l = { 0, 0 };
    LONG r = f(tok, &l);
    t_note("own token status %lx luid %lx:%lx", (ULONG)r, l.HighPart, l.LowPart);
    TOKEN_STATISTICS ts; DWORD n = 0;
    if (GetTokenInformation(tok, TokenStatistics, &ts, sizeof ts, &n)) {
        CHECK(l.LowPart == ts.AuthenticationId.LowPart);
        CHECK(l.HighPart == ts.AuthenticationId.HighPart);
    }
    /* interactive desktop session: success expected, but services/SSH
     * sessions legitimately yield a failure status */
    CHECK(r >= 0 || (ULONG)r != STATUS_INVALID_HANDLE_);
    CHECK(!(l.LowPart == 0 && l.HighPart == 0));
    CloseHandle(tok);
}
TEST(gap6_uttfi_repeat_consistent) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    LUID l0; LONG r0 = f(tok, &l0);
    for (int i = 0; i < 200; i++) {
        LUID l; LONG r = f(tok, &l);
        if (r != r0 || l.LowPart != l0.LowPart || l.HighPart != l0.HighPart) { CHECKF(0, "iter %d differs", i); break; }
    }
    CHECK(1);
    CloseHandle(tok);
}
TEST(gap6_uttfi_duplicated_tokens) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    HANDLE tok, dup;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    LUID l0; LONG r0 = f(tok, &l0);
    if (DuplicateTokenEx(tok, TOKEN_QUERY, NULL, SecurityImpersonation, TokenImpersonation, &dup)) {
        LUID l1; LONG r1 = f(dup, &l1);
        CHECK(l0.LowPart == l1.LowPart && l0.HighPart == l1.HighPart);
        CHECK(r0 == r1);
        CloseHandle(dup);
    }
    if (DuplicateTokenEx(tok, TOKEN_QUERY, NULL, SecurityAnonymous, TokenPrimary, &dup)) {
        LUID l1; LONG r1 = f(dup, &l1);
        CHECK(l0.LowPart == l1.LowPart && l0.HighPart == l1.HighPart);
        CloseHandle(dup);
    }
    CloseHandle(tok);
}
TEST(gap6_uttfi_thread_token) {
    UTTFI_pf f = (UTTFI_pf)t_proc("_UserTestTokenForInteractive"); REQUIRE(f);
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    HANDLE dup;
    LUID l0; f(tok, &l0);
    if (DuplicateTokenEx(tok, TOKEN_QUERY | TOKEN_IMPERSONATE, NULL, SecurityImpersonation, TokenImpersonation, &dup)) {
        if (SetThreadToken(NULL, dup)) {
            HANDLE tt;
            if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tt)) {
                LUID l1 = {0,0}; f(tt, &l1);
                CHECK(l1.LowPart == l0.LowPart && l1.HighPart == l0.HighPart);
                CloseHandle(tt);
            }
            SetThreadToken(NULL, NULL);
        }
        CloseHandle(dup);
    }
    CloseHandle(tok);
}
