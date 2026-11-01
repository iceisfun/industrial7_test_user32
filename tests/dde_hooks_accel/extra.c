/* Additional edge cases: DDEML string/instance corners, timer and hotkey limits, accelerator limits. */
#include "dh.h"
#include <wchar.h>

static const char *XCLS = "T32ExtraWnd";
static volatile LONG x_timer_hits;
static LRESULT CALLBACK x_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TIMER) { x_timer_hits++; return 0; }
    return DefWindowProcA(h, m, w, l);
}
static void CALLBACK x_tproc(HWND h, UINT m, UINT_PTR id, DWORD t) { x_timer_hits++; }

TEST(extra_dde_init_callback_null_observed) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    DWORD id = 0;
    UINT r = U(DdeInitializeA)(&id, NULL, APPCLASS_STANDARD, 0);
    t_note("DdeInitializeA(NULL callback) = 0x%x", r);
    if (r == 0) U(DdeUninitialize)(id);
    id = 0;
    r = U(DdeInitializeW)(&id, NULL, APPCLASS_STANDARD, 0);
    t_note("DdeInitializeW(NULL callback) = 0x%x", r);
    if (r == 0) U(DdeUninitialize)(id);
}
TEST(extra_dde_hsz_same_string_same_handle_twice) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id = 0;
    EQ(U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD | APPCMD_CLIENTONLY, 0), 0);
    HSZ a = U(DdeCreateStringHandleA)(id, "T32ExtraSame", CP_WINANSI);
    HSZ b = U(DdeCreateStringHandleA)(id, "t32extrasame", CP_WINANSI);
    CHECK(a != NULL && b != NULL);
    EQ(a, b);
    EQ(U(DdeFreeStringHandle)(id, a), TRUE);
    EQ(U(DdeFreeStringHandle)(id, b), TRUE);
    U(DdeUninitialize)(id);
}
TEST(extra_dde_uninit_frees_nothing_visible) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError));
    DWORD id = 0;
    EQ(U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD, 0), 0);
    EQ(U(DdeUninitialize)(id), TRUE);
    EQ(U(DdeUninitialize)(id), FALSE);
    /* a fresh init after uninit gives a usable instance again */
    DWORD id2 = 0;
    EQ(U(DdeInitializeA)(&id2, dh_null_cb, APPCLASS_STANDARD, 0), 0);
    CHECK(id2 != 0);
    EQ(U(DdeGetLastError)(id2), 0);
    EQ(U(DdeUninitialize)(id2), TRUE);
}
TEST(extra_dde_query_string_wide_count) {
    REQUIRE(U(DdeInitializeW)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeQueryStringW)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id = 0;
    EQ(U(DdeInitializeW)(&id, dh_null_cb, APPCLASS_STANDARD | APPCMD_CLIENTONLY, 0), 0);
    HSZ h = U(DdeCreateStringHandleW)(id, L"WideCount", CP_WINUNICODE);
    CHECK(h != NULL);
    EQ(U(DdeQueryStringW)(id, h, NULL, 0, CP_WINUNICODE), 9);
    WCHAR b[16]; wmemset(b, 0x7777, 16);
    EQ(U(DdeQueryStringW)(id, h, b, 16, CP_WINUNICODE), 9);
    EQ(b[9], 0);
    t_note("DdeQueryStringW cchMax=1 -> %lu", U(DdeQueryStringW)(id, h, b, 1, CP_WINUNICODE));
    U(DdeFreeStringHandle)(id, h);
    U(DdeUninitialize)(id);
}
TEST(extra_timer_null_hwnd_proc_called) {
    REQUIRE(U(SetTimer)); REQUIRE(U(KillTimer));
    x_timer_hits = 0;
    UINT_PTR id = U(SetTimer)(NULL, 0, 20, x_tproc);
    CHECK(id != 0);
    dh_pump_until(&x_timer_hits, 3000);
    CHECK(x_timer_hits >= 1);
    EQ(U(KillTimer)(NULL, id), TRUE);
    LONG n = x_timer_hits;
    dh_sleep_pump(100);
    EQ(x_timer_hits, n);
}
TEST(extra_timer_kill_twice) {
    REQUIRE(U(SetTimer)); REQUIRE(U(KillTimer));
    UINT_PTR id = U(SetTimer)(NULL, 0, 5000, x_tproc);
    CHECK(id != 0);
    EQ(U(KillTimer)(NULL, id), TRUE);
    EQ(U(KillTimer)(NULL, id), FALSE);
}
TEST(extra_timer_window_kill_twice) {
    REQUIRE(U(SetTimer)); REQUIRE(U(KillTimer));
    HWND w = dh_create_window(XCLS, x_proc, WS_OVERLAPPED, 0, NULL);
    CHECK(w != NULL);
    EQ(U(SetTimer)(w, 77, 5000, NULL), 77);
    EQ(U(KillTimer)(w, 77), TRUE);
    EQ(U(KillTimer)(w, 77), FALSE);
    dh_destroy_window(w, XCLS);
}
TEST(extra_timer_wm_timer_wparam_is_id) {
    REQUIRE(U(SetTimer)); REQUIRE(U(KillTimer));
    HWND w = dh_create_window(XCLS, x_proc, WS_OVERLAPPED, 0, NULL);
    EQ(U(SetTimer)(w, 4242, 20, NULL), 4242);
    MSG m; int got = 0; DWORD end = GetTickCount() + 3000;
    while (!got && GetTickCount() < end) {
        if (PeekMessageA(&m, w, WM_TIMER, WM_TIMER, PM_REMOVE)) got = 1;
        else MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
    }
    CHECK(got);
    if (got) { EQ(m.message, WM_TIMER); EQ(m.wParam, 4242); EQ(m.lParam, 0); EQ(m.hwnd, w); }
    EQ(U(KillTimer)(w, 4242), TRUE);
    dh_destroy_window(w, XCLS);
}
TEST(extra_hotkey_vk_boundaries_observed) {
    REQUIRE(U(RegisterHotKey)); REQUIRE(U(UnregisterHotKey));
    HWND w = dh_create_window(XCLS, x_proc, WS_OVERLAPPED, 0, NULL);
    static const UINT vks[] = {0, 1, 0xFE, 0xFF, 0x100, 0xFFFF, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof vks / sizeof vks[0]; i++) {
        SetLastError(0);
        BOOL r = U(RegisterHotKey)(w, 600 + i, MOD_CONTROL | MOD_ALT | MOD_SHIFT, vks[i]);
        t_note("RegisterHotKey vk=0x%x -> %d err=%lu", vks[i], r, GetLastError());
        if (r) U(UnregisterHotKey)(w, 600 + i);
    }
    dh_destroy_window(w, XCLS);
}
TEST(extra_hotkey_unregister_wrong_window) {
    REQUIRE(U(RegisterHotKey)); REQUIRE(U(UnregisterHotKey));
    HWND a = dh_create_window(XCLS, x_proc, WS_OVERLAPPED, 0, NULL);
    HWND b = U(CreateWindowExA)(0, XCLS, "b", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(U(RegisterHotKey)(a, 9, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F22));
    SetLastError(0);
    EQ(U(UnregisterHotKey)(b, 9), FALSE);
    EQ(GetLastError(), 1419);
    SetLastError(0);
    EQ(U(UnregisterHotKey)(NULL, 9), FALSE);
    EQ(U(UnregisterHotKey)(a, 9), TRUE);
    U(DestroyWindow)(b);
    dh_destroy_window(a, XCLS);
}
TEST(extra_accel_create_large_table) {
    REQUIRE(U(CreateAcceleratorTableA)); REQUIRE(U(DestroyAcceleratorTable)); REQUIRE(U(CopyAcceleratorTableA));
    enum { N = 300 };
    static ACCEL a[N];
    for (int i = 0; i < N; i++) { a[i].fVirt = FVIRTKEY; a[i].key = (WORD)(VK_F1 + (i % 24)); a[i].cmd = (WORD)(i + 1); }
    HACCEL h = U(CreateAcceleratorTableA)(a, N);
    CHECK(h != NULL);
    if (h) {
        EQ(U(CopyAcceleratorTableA)(h, NULL, 0), N);
        static ACCEL out[N];
        EQ(U(CopyAcceleratorTableA)(h, out, N), N);
        int bad = 0;
        for (int i = 0; i < N; i++) if (out[i].cmd != a[i].cmd || out[i].key != a[i].key) bad++;
        EQ(bad, 0);
        EQ(U(DestroyAcceleratorTable)(h), TRUE);
    }
}
TEST(extra_accel_create_many_tables) {
    REQUIRE(U(CreateAcceleratorTableA)); REQUIRE(U(DestroyAcceleratorTable));
    ACCEL a __attribute__((aligned(8))) = {FVIRTKEY, 'A', 1};
    HACCEL hs[50]; int ok = 0;
    DWORD first_err = 0;
    for (int i = 0; i < 50; i++) { SetLastError(0); hs[i] = U(CreateAcceleratorTableA)(&a, 1); if (hs[i]) ok++; else if (!first_err) first_err = GetLastError(); }
    /* the P0 oracle run created none of them; report the error so the next
     * run can say why */
    CHECKF(ok == 50, "created %d of 50, first error %lu", ok, first_err);
    for (int i = 0; i < 50; i++) if (hs[i]) EQ(U(DestroyAcceleratorTable)(hs[i]), TRUE);
}
TEST(extra_accel_translate_null_msg_table_survives) {
    REQUIRE(U(CreateAcceleratorTableA)); REQUIRE(U(DestroyAcceleratorTable)); REQUIRE(U(TranslateAcceleratorA));
    ACCEL a __attribute__((aligned(8))) = {FVIRTKEY, 'A', 1};
    SetLastError(0);
    HACCEL h = U(CreateAcceleratorTableA)(&a, 1);
    CHECKF(h != NULL, "CreateAcceleratorTableA failed, error %lu", GetLastError());
    if (!h) return;
    MSG m; memset(&m, 0, sizeof m); m.message = WM_KEYDOWN; m.wParam = 'A';
    EQ(U(TranslateAcceleratorA)(NULL, h, &m), 0);   /* NULL hwnd is rejected in user32 before win32k */
    EQ(U(TranslateAcceleratorW)(NULL, h, &m), 0);
    EQ(U(DestroyAcceleratorTable)(h), TRUE);
}
TEST(extra_dde_query_null_hsz_counts) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeQueryStringW));
    DWORD id = 0;
    EQ(U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD | APPCMD_CLIENTONLY, 0), 0);
    char a[4] = "zzz";
    EQ(U(DdeQueryStringA)(id, NULL, a, 4, CP_WINANSI), 1);   /* user32: NULL hsz yields the terminator count */
    EQ(a[0], 0);
    EQ(U(DdeQueryStringA)(id, NULL, NULL, 0, CP_WINANSI), 1);
    U(DdeUninitialize)(id);
}
