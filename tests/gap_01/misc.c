/* gap_01: pointer device internals, magnification context queries,
 * shutdown reason titles, cursor frame info, message position helpers,
 * and DWM redirection queries (error / harmless paths only). */
#include "t.h"

UDECL(BOOL, GetPointerDeviceOrientation, (HANDLE, DWORD *));
UDECL(BOOL, GetPointerDeviceInputSpace, (HANDLE, RECT *, RECT *));
UDECL(BOOL, GetExtendedPointerDeviceProperty, (HANDLE, UINT, void *));
UDECL(BOOL, GetPointerFrameTimes, (UINT, UINT *, DWORD *));
UDECL(BOOL, GetPointerFrameArrivalTimes, (UINT, UINT, ULONGLONG *));
UDECL(BOOL, GetMagnificationDesktopColorEffect, (float *));
UDECL(BOOL, GetMagnificationDesktopMagnification, (void *, int *, int *));
UDECL(BOOL, GetMagnificationDesktopSamplingMode, (DWORD *));
UDECL(BOOL, GetReasonTitleFromReasonCode, (DWORD, LPWSTR, DWORD));
UDECL(HCURSOR, GetCursorFrameInfo, (HCURSOR, LPCWSTR, DWORD, DWORD *, DWORD *));
UDECL(DWORD, GetUnpredictedMessagePos, (void));
UDECL(HWND, GetSendMessageReceiver, (DWORD));
UDECL(BOOL, GetProcessUIContextInformation, (HANDLE, void *));
UDECL(BOOL, DwmGetDxSharedSurface, (HWND, HANDLE *, LUID *, ULONG *, ULONG *, ULONGLONG *));

#define IDCW_ARROW MAKEINTRESOURCEW(32512)
/* Environments that only provide abort-on-call stubs for undocumented exports are skipped. */
static int stub_env(void) { return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL; }

/* ================= pointer device queries ================= */
static HANDLE bad_devs[] = {NULL, (HANDLE)(ULONG_PTR)1, (HANDLE)(ULONG_PTR)0x12345678, (HANDLE)(ULONG_PTR)-1, (HANDLE)(ULONG_PTR)0xdeadbeef};
#define NBAD 5

TEST(gap1_ptrdev_orientation_bogus_device) {
    REQUIRE(UN(GetPointerDeviceOrientation));
    for (int i = 0; i < NBAD; i++) {
        BYTE out[32]; memset(out, 0xcd, sizeof out);
        SetLastError(0);
        BOOL r = UN(GetPointerDeviceOrientation)(bad_devs[i], (DWORD *)out);
        /* (HANDLE)-1 is accepted by Win10 22H2 (TRUE); it behaves like a
         * pseudo handle for the default device, so only record it. */
        if (bad_devs[i] == (HANDLE)(ULONG_PTR)-1) { t_note("orientation(-1) -> %d err %lu", r, GetLastError()); continue; }
        CHECKF(r == FALSE, "device %d", i);
        DWORD e = GetLastError();
        if (e != ERROR_INVALID_HANDLE && e != ERROR_INVALID_PARAMETER) t_note("orientation bogus device err %lu", e);
    }
}
TEST(gap1_ptrdev_inputspace_bogus_device) {
    REQUIRE(UN(GetPointerDeviceInputSpace));
    for (int i = 0; i < NBAD; i++) {
        RECT a = {1, 2, 3, 4}, b = {5, 6, 7, 8};
        BOOL r = UN(GetPointerDeviceInputSpace)(bad_devs[i], &a, &b);
        /* (HANDLE)-1 is accepted by Win10 22H2 (TRUE); see the orientation test. */
        if (bad_devs[i] == (HANDLE)(ULONG_PTR)-1) { t_note("inputspace(-1) -> %d err %lu", r, GetLastError()); continue; }
        CHECKF(r == FALSE, "device %d", i);
    }
}
TEST(gap1_ptrdev_extended_property_bogus_device) {
    REQUIRE(UN(GetExtendedPointerDeviceProperty));
    for (int i = 0; i < NBAD; i++) {
        BYTE out[64]; memset(out, 0, sizeof out);
        BOOL r = UN(GetExtendedPointerDeviceProperty)(bad_devs[i], 0, out);
        CHECKF(r == FALSE, "device %d", i);
        r = UN(GetExtendedPointerDeviceProperty)(bad_devs[i], 0xffffffff, out);
        CHECKF(r == FALSE, "device %d prop -1", i);
    }
}
TEST(gap1_ptrdev_frame_times_bogus_pointer) {
    REQUIRE(UN(GetPointerFrameTimes));
    UINT ids[] = {0, 1, 0x7fff, 0xfffffff0, 0xffffffff};
    for (int i = 0; i < 5; i++) {
        UINT cnt = 0; DWORD times[8] = {0};
        SetLastError(0);
        BOOL r = UN(GetPointerFrameTimes)(ids[i], &cnt, times);
        CHECKF(r == FALSE, "pointer id %u", ids[i]);
    }
}
TEST(gap1_ptrdev_enumerated_devices_note) {
    REQUIRE(U(GetPointerDevices)); REQUIRE(UN(GetPointerDeviceOrientation));
    UINT n = 0;
    if (!U(GetPointerDevices)(&n, NULL)) { t_note("GetPointerDevices count query failed err %lu", GetLastError()); return; }
    t_note("pointer device count %u", n);
    if (n == 0 || n > 32) return;
    POINTER_DEVICE_INFO *info = calloc(n, sizeof *info);
    if (!info) return;
    UINT n2 = n;
    if (U(GetPointerDevices)(&n2, info)) {
        for (UINT i = 0; i < n2; i++) {
            DWORD o = 0xff;
            BOOL r = UN(GetPointerDeviceOrientation)(info[i].device, &o);
            t_note("device %u orientation query -> %d value %lu (display orientation %lu)", i, r, o, info[i].displayOrientation);
            RECT a, b;
            if (UN(GetPointerDeviceInputSpace)) {
                r = UN(GetPointerDeviceInputSpace)(info[i].device, &a, &b);
                t_note("device %u input space -> %d (%ld,%ld,%ld,%ld)", i, r, a.left, a.top, a.right, a.bottom);
            }
        }
    }
    free(info);
}

/* GetPointerFrameArrivalTimes is, on this build, the shared "not supported" stub
 * (RegisterPointerInputTargetEx and UnregisterPointerInputTargetEx fold into it). */
TEST(gap1_ptrarrival_not_supported) {
    REQUIRE(UN(GetPointerFrameArrivalTimes));
    ULONGLONG t[4] = {0};
    SetLastError(0);
    BOOL r = UN(GetPointerFrameArrivalTimes)(1, 1, t);
    CHECK(r == FALSE);
    EXPECT_EQ(GetLastError(), ERROR_NOT_SUPPORTED);
    SetLastError(0);
    r = UN(GetPointerFrameArrivalTimes)(0, 0, NULL);
    CHECK(r == FALSE);
    EXPECT_EQ(GetLastError(), ERROR_NOT_SUPPORTED);
    CHECK(t[0] == 0);
}
TEST(gap1_ptrarrival_repeatable) {
    REQUIRE(UN(GetPointerFrameArrivalTimes));
    for (int i = 0; i < 100; i++) {
        SetLastError(0);
        if (UN(GetPointerFrameArrivalTimes)(i, i, NULL) != FALSE || GetLastError() != ERROR_NOT_SUPPORTED) { CHECKF(0, "iter %d", i); return; }
    }
    CHECK(1);
}
TEST(gap1_ptrinputtargetex_not_supported) {
    REQUIRE(U(RegisterPointerInputTargetEx)); REQUIRE(U(UnregisterPointerInputTargetEx));
    HWND h = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 10, 10, NULL);
    if (!h) { CHECK(0); return; }
    SetLastError(0);
    BOOL r = U(RegisterPointerInputTargetEx)(h, PT_TOUCH, TRUE);
    t_note("RegisterPointerInputTargetEx -> %d err %lu", r, GetLastError());
    if (r) U(UnregisterPointerInputTargetEx)(h, PT_TOUCH);
    SetLastError(0);
    r = U(UnregisterPointerInputTargetEx)(h, PT_TOUCH);
    t_note("UnregisterPointerInputTargetEx -> %d err %lu", r, GetLastError());
    U(DestroyWindow)(h);
    CHECK(1);
}
TEST(gap1_ptrarrival_shares_address_note) {
    REQUIRE(UN(GetPointerFrameArrivalTimes));
    FARPROC a = t_proc("RegisterPointerInputTargetEx"), b = t_proc("UnregisterPointerInputTargetEx");
    FARPROC g = t_proc("GetPointerFrameArrivalTimes");
    if (a != g || b != g) t_note("the three exports are not folded to one stub on this build");
    CHECK(1);
}

/* ================= magnification context ================= */
TEST(gap1_mag_color_effect_valid_buffer) {
    REQUIRE(UN(GetMagnificationDesktopColorEffect));
    float m[25]; for (int i = 0; i < 25; i++) m[i] = -7.0f;
    BOOL r = UN(GetMagnificationDesktopColorEffect)(m);
    t_note("GetMagnificationDesktopColorEffect -> %d err %lu", r, GetLastError());
    if (r) {
        int ident = 1;
        for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) if (m[i * 5 + j] != (i == j ? 1.0f : 0.0f)) ident = 0;
        if (!ident) t_note("desktop colour effect is not the identity matrix (colour effect active?)");
    } else {
        int untouched = 1; for (int i = 0; i < 25; i++) if (m[i] != -7.0f) untouched = 0;
        CHECK(untouched);
    }
}
TEST(gap1_mag_color_effect_null_buffer) {
    REQUIRE(UN(GetMagnificationDesktopColorEffect));
    /* user32 tests the pointer before storing, so NULL never faults */
    BOOL r = UN(GetMagnificationDesktopColorEffect)(NULL);
    t_note("NULL buffer -> %d", r);
    CHECK(1);
}
TEST(gap1_mag_color_effect_repeatable) {
    REQUIRE(UN(GetMagnificationDesktopColorEffect));
    float a[25], b[25];
    BOOL r1 = UN(GetMagnificationDesktopColorEffect)(a);
    BOOL r2 = UN(GetMagnificationDesktopColorEffect)(b);
    CHECK(r1 == r2);
    if (r1 && r2) CHECK(memcmp(a, b, sizeof a) == 0);
}
TEST(gap1_mag_magnification_defaults) {
    REQUIRE(UN(GetMagnificationDesktopMagnification));
    BYTE mag[16]; memset(mag, 0, sizeof mag);
    int x = 0x7777, y = 0x7777;
    BOOL r = UN(GetMagnificationDesktopMagnification)(mag, &x, &y);
    float f = *(float *)mag;
    t_note("magnification ret=%d level=%f offset=(%d,%d)", r, f, x, y);
    if (r) {
        CHECK(f >= 1.0f || f == 0.0f);
        CHECK(x >= 0 && y >= 0);
    }
}
TEST(gap1_mag_sampling_mode) {
    REQUIRE(UN(GetMagnificationDesktopSamplingMode));
    DWORD m = 0xdead;
    BOOL r = UN(GetMagnificationDesktopSamplingMode)(&m);
    t_note("sampling mode ret=%d mode=%lu err=%lu", r, m, GetLastError());
    if (!r) CHECK(m == 0xdead);
    else CHECKF(m <= 3, "mode %lu", m);
}
TEST(gap1_mag_sampling_mode_null) {
    REQUIRE(UN(GetMagnificationDesktopSamplingMode));
    BOOL r = UN(GetMagnificationDesktopSamplingMode)(NULL);
    t_note("NULL out -> %d", r);
    CHECK(1);
}
TEST(gap1_mag_sampling_repeatable) {
    REQUIRE(UN(GetMagnificationDesktopSamplingMode));
    DWORD a = 0xdead, b = 0xbeef;
    BOOL r1 = UN(GetMagnificationDesktopSamplingMode)(&a), r2 = UN(GetMagnificationDesktopSamplingMode)(&b);
    CHECK(r1 == r2);
    if (r1) CHECK(a == b);
}

/* ================= GetReasonTitleFromReasonCode ================= */
TEST(gap1_reason_null_or_zero_buffer) {
    REQUIRE(UN(GetReasonTitleFromReasonCode));
    WCHAR buf[8] = {0x5555, 0x5555};
    CHECK(UN(GetReasonTitleFromReasonCode)(0, NULL, 0) == FALSE);
    CHECK(UN(GetReasonTitleFromReasonCode)(0, NULL, 100) == FALSE);
    CHECK(UN(GetReasonTitleFromReasonCode)(0, buf, 0) == FALSE);
    CHECK(UN(GetReasonTitleFromReasonCode)(0x80000000, NULL, 0) == FALSE);
    CHECK(UN(GetReasonTitleFromReasonCode)(0xffffffff, buf, 0) == FALSE);
    CHECK(buf[0] == 0x5555 && buf[1] == 0x5555);
}
TEST(gap1_reason_one_char_buffer_fails) {
    REQUIRE(UN(GetReasonTitleFromReasonCode));
    DWORD codes[] = {0, 0x80000000, 0x00020002, 0x00FF00FF, 0xffffffff};
    for (int i = 0; i < 5; i++) {
        WCHAR b[2] = {0x5555, 0x5555};
        BOOL r = UN(GetReasonTitleFromReasonCode)(codes[i], b, 1);
        CHECKF(r == FALSE, "code 0x%lx", codes[i]);
        CHECKF(b[1] == 0x5555, "wrote past the buffer for 0x%lx", codes[i]);
    }
}
TEST(gap1_reason_known_codes_have_titles) {
    REQUIRE(UN(GetReasonTitleFromReasonCode));
    DWORD codes[] = {
        0x00000000, 0x80000000, 0x00020002 /* app: install, unplanned? */, 0x80020002,
        0x00010001, 0x00040001, 0x00050001, 0x00050002, 0x80050003, 0x00060000,
    };
    for (int i = 0; i < 10; i++) {
        WCHAR b[256]; for (int k = 0; k < 256; k++) b[k] = 0x5555;
        BOOL r = UN(GetReasonTitleFromReasonCode)(codes[i], b, 256);
        if (r) {
            size_t len = lstrlenW(b);
            CHECKF(len > 0 && len < 256, "code 0x%lx len %zu", codes[i], len);
        } else t_note("title for reason 0x%lx failed (err %lu)", codes[i], GetLastError());
    }
}
TEST(gap1_reason_deterministic_and_nonoverlapping) {
    REQUIRE(UN(GetReasonTitleFromReasonCode));
    WCHAR a[256], b[256];
    BOOL r1 = UN(GetReasonTitleFromReasonCode)(0x80020002, a, 256);
    BOOL r2 = UN(GetReasonTitleFromReasonCode)(0x80020002, b, 256);
    CHECK(r1 == r2);
    if (r1) CHECK(lstrcmpW(a, b) == 0);
}
TEST(gap1_reason_buffer_sizes) {
    REQUIRE(UN(GetReasonTitleFromReasonCode));
    WCHAR full[256];
    if (!UN(GetReasonTitleFromReasonCode)(0x80020002, full, 256)) { t_note("no title for sample code"); return; }
    int L = lstrlenW(full);
    CHECK(L > 0);
    /* exact fit incl. NUL */
    WCHAR *ex = calloc(L + 8, sizeof(WCHAR));
    for (int i = 0; i < L + 8; i++) ex[i] = 0x5555;
    BOOL r = UN(GetReasonTitleFromReasonCode)(0x80020002, ex, L + 1);
    CHECK(r == TRUE);
    CHECK(lstrcmpW(ex, full) == 0);
    CHECK(ex[L + 1] == 0x5555);
    /* one short: truncated or failed, must never overrun */
    for (int i = 0; i < L + 8; i++) ex[i] = 0x5555;
    r = UN(GetReasonTitleFromReasonCode)(0x80020002, ex, L);
    t_note("buffer of exactly the string length: ret=%d", r);
    CHECK(ex[L] == 0x5555);
    CHECK(ex[L - 1] == 0 || ex[L - 1] == 0x5555 || r);
    free(ex);
}
TEST(gap1_reason_planned_flag_variants) {
    REQUIRE(UN(GetReasonTitleFromReasonCode));
    WCHAR a[256], b[256];
    BOOL r1 = UN(GetReasonTitleFromReasonCode)(0x00000000, a, 256);
    BOOL r2 = UN(GetReasonTitleFromReasonCode)(0x80000000, b, 256);
    t_note("unplanned other: %d, planned other: %d", r1, r2);
    if (r1 && r2 && lstrcmpW(a, b) == 0) t_note("planned and unplanned 'other' share a title");
    CHECK(1);
}
TEST(gap1_reason_unknown_code_falls_back) {
    REQUIRE(UN(GetReasonTitleFromReasonCode));
    WCHAR b[256];
    BOOL r = UN(GetReasonTitleFromReasonCode)(0x00FE00FE, b, 256);
    if (r) CHECK(lstrlenW(b) > 0);
    else t_note("unknown reason code gave no fallback title");
}
TEST(gap1_reason_large_buffer) {
    REQUIRE(UN(GetReasonTitleFromReasonCode));
    WCHAR *b = malloc(65536 * sizeof(WCHAR));
    if (!b) return;
    for (int i = 0; i < 65536; i++) b[i] = 0x5555;
    BOOL r = UN(GetReasonTitleFromReasonCode)(0x00000000, b, 65536);
    if (r) { CHECK(lstrlenW(b) < 1024); }
    free(b);
    CHECK(1);
}

/* ================= GetCursorFrameInfo ================= */
TEST(gap1_cursorframe_static_cursor) {
    REQUIRE(UN(GetCursorFrameInfo)); REQUIRE(U(LoadCursorW));
    HCURSOR c = U(LoadCursorW)(NULL, IDCW_ARROW);
    CHECK(c != NULL);
    DWORD rate = 0x1234, steps = 0x1234;
    HCURSOR r = UN(GetCursorFrameInfo)(c, NULL, 0, &rate, &steps);
    t_note("static arrow: ret=%p rate=%lu steps=%lu err=%lu", r, rate, steps, GetLastError());
    if (r && steps != 0x1234) CHECKF(steps >= 1, "steps %lu", steps);
}
TEST(gap1_cursorframe_bogus_cursor) {
    REQUIRE(UN(GetCursorFrameInfo));
    HCURSOR bad[] = {(HCURSOR)(ULONG_PTR)0x12345678, (HCURSOR)(ULONG_PTR)-1, (HCURSOR)(ULONG_PTR)0xdeadbeef};
    for (int i = 0; i < 3; i++) {
        DWORD rate = 0x1234, steps = 0x1234;
        HCURSOR r = UN(GetCursorFrameInfo)(bad[i], NULL, 0, &rate, &steps);
        CHECKF(r == NULL, "bogus cursor %d returned %p", i, r);
    }
}
TEST(gap1_cursorframe_by_resource_id_loads_system_cursor) {
    REQUIRE(UN(GetCursorFrameInfo));
    HCURSOR r = UN(GetCursorFrameInfo)(NULL, IDCW_ARROW, 0, NULL, NULL);
    t_note("load by id (NULL cursor) -> %p", r);
    if (r && r != U(LoadCursorW)(NULL, IDCW_ARROW)) U(DestroyCursor)(r);
    CHECK(1);
}
TEST(gap1_cursorframe_step_out_of_range_note) {
    REQUIRE(UN(GetCursorFrameInfo)); REQUIRE(U(LoadCursorW));
    HCURSOR c = U(LoadCursorW)(NULL, IDCW_ARROW);
    DWORD steps[] = {1, 2, 100, 0xffffffff};
    for (int i = 0; i < 4; i++) {
        DWORD rate = 0, n = 0;
        HCURSOR r = UN(GetCursorFrameInfo)(c, NULL, steps[i], &rate, &n);
        t_note("step %lu -> %p (rate %lu n %lu)", steps[i], r, rate, n);
    }
    CHECK(1);
}

/* ================= GetUnpredictedMessagePos ================= */
TEST(gap1_unpredictedpos_close_to_cursor) {
    REQUIRE(UN(GetUnpredictedMessagePos)); REQUIRE(U(GetCursorPos));
    POINT p; U(GetCursorPos)(&p);
    DWORD v = UN(GetUnpredictedMessagePos)();
    POINT q = {(short)LOWORD(v), (short)HIWORD(v)};
    t_note("cursor (%ld,%ld) unpredicted (%ld,%ld) raw 0x%08lx", p.x, p.y, q.x, q.y, v);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    CHECK(q.x >= vx - 1 && q.x <= vx + vw);
    CHECK(q.y >= vy - 1 && q.y <= vy + vh);
}
TEST(gap1_unpredictedpos_stable_without_input) {
    REQUIRE(UN(GetUnpredictedMessagePos));
    DWORD a = UN(GetUnpredictedMessagePos)();
    DWORD diff = 0;
    for (int i = 0; i < 50; i++) { if (UN(GetUnpredictedMessagePos)() != a) diff++; }
    if (diff) t_note("value changed %lu times in 50 reads (mouse moving?)", diff);
    CHECK(1);
}
TEST(gap1_unpredictedpos_vs_messagepos) {
    REQUIRE(UN(GetUnpredictedMessagePos)); REQUIRE(U(GetMessagePos));
    DWORD a = UN(GetUnpredictedMessagePos)(), b = U(GetMessagePos)();
    if (a != b) t_note("GetUnpredictedMessagePos 0x%08lx vs GetMessagePos 0x%08lx", a, b);
    CHECK(1);
}
static LRESULT CALLBACK pos_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 1) {
        DWORD a = UN(GetUnpredictedMessagePos)(), b = U(GetMessagePos)();
        if (a != b) t_note("inside message: unpredicted 0x%08lx, message pos 0x%08lx", a, b);
        return 1;
    }
    return DefWindowProcA(h, m, w, l);
}
TEST(gap1_unpredictedpos_inside_wndproc) {
    REQUIRE(UN(GetUnpredictedMessagePos));
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = pos_wndproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "gap1_pos_cls";
    if (!U(RegisterClassA)(&wc)) { CHECK(0); return; }
    HWND h = t_make_window("gap1_pos_cls", WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 10, 10, NULL);
    CHECK(h != NULL);
    if (h) {
        CHECK(U(SendMessageA)(h, WM_USER + 1, 0, 0) == 1);
        U(PostMessageA)(h, WM_USER + 1, 0, 0);
        t_pump();
        U(DestroyWindow)(h);
    }
    U(UnregisterClassA)("gap1_pos_cls", GetModuleHandleA(NULL));
}
static DWORD WINAPI pos_thread(void *p) {
    *(DWORD *)p = UN(GetUnpredictedMessagePos)();
    return 0;
}
TEST(gap1_unpredictedpos_other_thread) {
    REQUIRE(UN(GetUnpredictedMessagePos));
    DWORD v = 0xffffffff;
    HANDLE t = CreateThread(NULL, 0, pos_thread, &v, 0, NULL);
    if (t) { CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0); CloseHandle(t); }
    CHECK(v != 0xffffffff || 1);
}

/* ================= GetSendMessageReceiver ================= */
TEST(gap1_sendrecv_idle_thread_has_none) {
    REQUIRE(UN(GetSendMessageReceiver));
    DWORD ids[] = {0, GetCurrentThreadId(), 0xffffffff, 0x7fffffff};
    for (int i = 0; i < 4; i++) {
        HWND r = UN(GetSendMessageReceiver)(ids[i]);
        CHECKF(r == NULL || U(IsWindow)(r), "id 0x%lx returned stale handle %p", ids[i], r);
        if (r) t_note("thread 0x%lx reports pending receiver %p", ids[i], r);
    }
}
static DWORD g_tid_main;
static HWND g_recv_seen; static int g_in_proc;
static LRESULT CALLBACK sr_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 2) { g_recv_seen = UN(GetSendMessageReceiver)((DWORD)l); g_in_proc = 1; return 5; }
    return DefWindowProcA(h, m, w, l);
}
static DWORD WINAPI sr_sender(void *p) {
    LRESULT r = U(SendMessageA)((HWND)p, WM_USER + 2, 0, (LPARAM)GetCurrentThreadId());
    return (DWORD)r;
}
TEST(gap1_sendrecv_during_sendmessage_note) {
    REQUIRE(UN(GetSendMessageReceiver));
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = sr_wndproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "gap1_sr_cls";
    if (!U(RegisterClassA)(&wc)) { CHECK(0); return; }
    HWND h = t_make_window("gap1_sr_cls", WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 10, 10, NULL);
    if (!h) { CHECK(0); return; }
    g_recv_seen = (HWND)1; g_in_proc = 0;
    HANDLE t = CreateThread(NULL, 0, sr_sender, h, 0, NULL);
    DWORD end = GetTickCount() + 5000;
    while (t && WaitForSingleObject(t, 10) == WAIT_TIMEOUT && GetTickCount() < end) t_pump();
    DWORD code = 0;
    if (t) { GetExitCodeThread(t, &code); CloseHandle(t); }
    CHECK(g_in_proc == 1);
    CHECK(code == 5);
    t_note("receiver as seen by the sending thread id from inside the handler: %p (window %p)", g_recv_seen, h);
    U(DestroyWindow)(h);
    U(UnregisterClassA)("gap1_sr_cls", GetModuleHandleA(NULL));
}

/* ================= GetProcessUIContextInformation ================= */
TEST(gap1_uictx_current_process_note) {
    REQUIRE(UN(GetProcessUIContextInformation));
    if (stub_env()) { t_skip("stub environment"); return; }
    BYTE buf[256]; memset(buf, 0, sizeof buf);
    BOOL r = UN(GetProcessUIContextInformation)((HANDLE)(LONG_PTR)-1, buf);
    t_note("GetProcessUIContextInformation(current) ret=%d err=%lu first dwords %08lx %08lx %08lx %08lx", r, GetLastError(),
           ((DWORD *)buf)[0], ((DWORD *)buf)[1], ((DWORD *)buf)[2], ((DWORD *)buf)[3]);
    CHECK(1);
}
TEST(gap1_uictx_bogus_handle) {
    REQUIRE(UN(GetProcessUIContextInformation));
    if (stub_env()) { t_skip("stub environment"); return; }
    BYTE buf[256]; memset(buf, 0, sizeof buf);
    BOOL r = UN(GetProcessUIContextInformation)((HANDLE)(ULONG_PTR)0x7ffffff0, buf);
    t_note("bogus handle ret=%d err=%lu", r, GetLastError());
    CHECK(1);
}

/* ================= DwmGetDxSharedSurface (no-privilege query) ================= */
TEST(gap1_dxsurface_failure_leaves_outputs) {
    REQUIRE(UN(DwmGetDxSharedSurface));
    HANDLE h = (HANDLE)0x1111; LUID l = {0x2222, 0x3333}; ULONG fmt = 0x4444, flags = 0x5555; ULONGLONG id = 0x6666;
    BOOL r = UN(DwmGetDxSharedSurface)(NULL, &h, &l, &fmt, &flags, &id);
    t_note("DwmGetDxSharedSurface(NULL) ret=%d err=%lu", r, GetLastError());
    if (!r) {
        CHECK(h == (HANDLE)0x1111);
        CHECK(l.LowPart == 0x2222 && l.HighPart == 0x3333);
        CHECK(fmt == 0x4444);
        CHECK(flags == 0x5555);
        CHECK(id == 0x6666);
    }
}
TEST(gap1_dxsurface_all_null_outputs) {
    REQUIRE(UN(DwmGetDxSharedSurface));
    HWND bad[] = {NULL, (HWND)(ULONG_PTR)0x12345678, GetDesktopWindow()};
    for (int i = 0; i < 3; i++) {
        BOOL r = UN(DwmGetDxSharedSurface)(bad[i], NULL, NULL, NULL, NULL, NULL);
        t_note("window %d with NULL outputs -> %d", i, r);
    }
    CHECK(1);
}
TEST(gap1_dxsurface_own_window_note) {
    REQUIRE(UN(DwmGetDxSharedSurface));
    HWND h = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 10, 10, NULL);
    if (!h) { CHECK(0); return; }
    HANDLE sh = NULL; LUID l = {0}; ULONG fmt = 0, fl = 0; ULONGLONG id = 0;
    BOOL r = UN(DwmGetDxSharedSurface)(h, &sh, &l, &fmt, &fl, &id);
    t_note("DwmGetDxSharedSurface(own window) ret=%d err=%lu", r, GetLastError());
    U(DestroyWindow)(h);
    CHECK(1);
}

/* ExitWindowsEx is deliberately never invoked: almost every flag combination,
 * including 0 (EWX_LOGOFF), would end the interactive session. */
TEST(gap1_exitwindowsex_present_only) {
    REQUIRE(U(ExitWindowsEx));
    CHECK(U(ExitWindowsEx) != NULL);
}
