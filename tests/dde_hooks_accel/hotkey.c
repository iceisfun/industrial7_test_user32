/* RegisterHotKey / UnregisterHotKey. Only obscure key combinations are used and everything is
 * unregistered again; no input is generated. */
#include "dh.h"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
#ifndef ERROR_HOTKEY_ALREADY_REGISTERED
#define ERROR_HOTKEY_ALREADY_REGISTERED 1409
#endif
#ifndef ERROR_HOTKEY_NOT_REGISTERED
#define ERROR_HOTKEY_NOT_REGISTERED 1419
#endif
#define MODS3 (MOD_CONTROL | MOD_ALT | MOD_SHIFT)

static const char *HKCLS = "T32HotkeyWnd";
static volatile LONG hk_count; static volatile WPARAM hk_wp; static volatile LPARAM hk_lp;
static LRESULT CALLBACK hk_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_HOTKEY) { hk_count++; hk_wp = w; hk_lp = l; return 0; }
    return DefWindowProcA(h, m, w, l);
}
static HWND hkw(void) { hk_count = 0; return dh_create_window(HKCLS, hk_proc, WS_OVERLAPPED, 0, NULL); }
static void hkkill(HWND h) { dh_destroy_window(h, HKCLS); }
static int hkneed(void) { return U(RegisterHotKey) && U(UnregisterHotKey) && U(RegisterClassA) && U(CreateWindowExA) && U(DestroyWindow) && U(UnregisterClassA); }
#define HKNEED() do { if (!hkneed()) { t_skip("missing exports"); return; } } while (0)

TEST(hotkey_register_unregister) {
    HKNEED();
    HWND w = hkw();
    SetLastError(0);
    BOOL r = U(RegisterHotKey)(w, 1, MODS3, VK_F24);
    CHECKF(r, "RegisterHotKey err %lu", GetLastError());
    if (r) EQ(U(UnregisterHotKey)(w, 1), TRUE);
    hkkill(w);
}
TEST(hotkey_unregister_twice) {
    HKNEED();
    HWND w = hkw();
    CHECK(U(RegisterHotKey)(w, 2, MODS3, VK_F24));
    EQ(U(UnregisterHotKey)(w, 2), TRUE);
    SetLastError(0);
    EQ(U(UnregisterHotKey)(w, 2), FALSE);
    EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    hkkill(w);
}
TEST(hotkey_unregister_never_registered) {
    HKNEED();
    HWND w = hkw();
    static const int ids[] = {0, 1, 0x1234, 0xBFFF, 0xC000, 0xFFFF};
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0);
        CHECKF(U(UnregisterHotKey)(w, ids[i]) == FALSE, "id 0x%x", ids[i]);
        CHECKF(GetLastError() == ERROR_HOTKEY_NOT_REGISTERED, "id 0x%x err %lu", ids[i], GetLastError());
    }
    SetLastError(0);
    EQ(U(UnregisterHotKey)(NULL, 7), FALSE);
    EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    hkkill(w);
}
TEST(hotkey_unregister_invalid_window) {
    HKNEED();
    SetLastError(0);
    EQ(U(UnregisterHotKey)((HWND)0x12345678, 1), FALSE);
    t_note("UnregisterHotKey(bogus hwnd) err=%lu", GetLastError());
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE || GetLastError() == ERROR_HOTKEY_NOT_REGISTERED);
}
TEST(hotkey_register_invalid_window) {
    HKNEED();
    SetLastError(0);
    EQ(U(RegisterHotKey)((HWND)0x12345678, 1, MODS3, VK_F24), FALSE);
    EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    EQ(U(RegisterHotKey)((HWND)(ULONG_PTR)0xFFFFFFFF, 1, MODS3, VK_F24), FALSE);
    t_note("RegisterHotKey(0xFFFFFFFF) err=%lu", GetLastError());
}
TEST(hotkey_register_destroyed_window) {
    HKNEED();
    HWND w = hkw();
    U(DestroyWindow)(w);
    SetLastError(0);
    EQ(U(RegisterHotKey)(w, 1, MODS3, VK_F24), FALSE);
    EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
    dh_destroy_window(NULL, HKCLS);
}
TEST(hotkey_duplicate_same_window_same_id) {
    HKNEED();
    HWND w = hkw();
    CHECK(U(RegisterHotKey)(w, 3, MODS3, VK_F24));
    SetLastError(0);
    EQ(U(RegisterHotKey)(w, 3, MODS3, VK_F24), FALSE);
    EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
    EQ(U(UnregisterHotKey)(w, 3), TRUE);
    hkkill(w);
}
TEST(hotkey_duplicate_different_id) {
    HKNEED();
    HWND w = hkw();
    CHECK(U(RegisterHotKey)(w, 3, MODS3, VK_F24));
    SetLastError(0);
    EQ(U(RegisterHotKey)(w, 4, MODS3, VK_F24), FALSE);
    EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
    SetLastError(0);
    EQ(U(UnregisterHotKey)(w, 4), FALSE);
    EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    EQ(U(UnregisterHotKey)(w, 3), TRUE);
    hkkill(w);
}
TEST(hotkey_duplicate_across_windows_same_thread) {
    HKNEED();
    HWND a = hkw();
    HWND b = U(CreateWindowExA)(0, HKCLS, "b", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(U(RegisterHotKey)(a, 1, MODS3, VK_F24));
    SetLastError(0);
    EQ(U(RegisterHotKey)(b, 1, MODS3, VK_F24), FALSE);
    EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
    SetLastError(0);
    EQ(U(RegisterHotKey)(NULL, 1, MODS3, VK_F24), FALSE);
    EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
    EQ(U(UnregisterHotKey)(a, 1), TRUE);
    /* now it is free for the other window */
    CHECK(U(RegisterHotKey)(b, 1, MODS3, VK_F24));
    EQ(U(UnregisterHotKey)(b, 1), TRUE);
    U(DestroyWindow)(b);
    hkkill(a);
}
TEST(hotkey_same_id_different_keys_coexist) {
    HKNEED();
    HWND w = hkw();
    BOOL r1 = U(RegisterHotKey)(w, 5, MODS3, VK_F24);
    BOOL r2 = U(RegisterHotKey)(w, 5, MODS3, VK_F23);
    CHECK(r1);
    CHECKF(r2, "second key under the same id err %lu", GetLastError());
    int n = 0;
    while (U(UnregisterHotKey)(w, 5) && n < 4) n++;
    t_note("UnregisterHotKey calls that succeeded for two registrations under one id: %d", n);
    CHECK(n >= 1);
    hkkill(w);
}
TEST(hotkey_modifier_variants_distinct) {
    HKNEED();
    HWND w = hkw();
    static const UINT mods[] = {MOD_CONTROL | MOD_ALT, MOD_CONTROL | MOD_SHIFT, MOD_ALT | MOD_SHIFT, MODS3, MODS3 | MOD_WIN};
    for (unsigned i = 0; i < sizeof mods / sizeof mods[0]; i++)
        CHECKF(U(RegisterHotKey)(w, 10 + i, mods[i], VK_F23), "mods 0x%x err %lu", mods[i], GetLastError());
    for (unsigned i = 0; i < sizeof mods / sizeof mods[0]; i++)
        CHECKF(U(UnregisterHotKey)(w, 10 + i), "unregister mods 0x%x", mods[i]);
    hkkill(w);
}
TEST(hotkey_norepeat_flag) {
    HKNEED();
    HWND w = hkw();
    SetLastError(0);
    BOOL r = U(RegisterHotKey)(w, 20, MODS3 | MOD_NOREPEAT, VK_F24);
    CHECKF(r, "MOD_NOREPEAT err %lu", GetLastError());
    if (r) {
        SetLastError(0);
        BOOL d = U(RegisterHotKey)(w, 21, MODS3, VK_F24);
        t_note("same key without MOD_NOREPEAT while registered with it -> %d err=%lu", d, GetLastError());
        if (d) U(UnregisterHotKey)(w, 21);
        EQ(U(UnregisterHotKey)(w, 20), TRUE);
    }
    hkkill(w);
}
TEST(hotkey_norepeat_alone_is_invalid_or_ok_observed) {
    HKNEED();
    HWND w = hkw();
    SetLastError(0);
    BOOL r = U(RegisterHotKey)(w, 22, MOD_NOREPEAT, VK_F24);
    t_note("MOD_NOREPEAT only -> %d err=%lu", r, GetLastError());
    if (r) U(UnregisterHotKey)(w, 22);
    hkkill(w);
}
TEST(hotkey_no_modifiers_observed) {
    HKNEED();
    HWND w = hkw();
    SetLastError(0);
    BOOL r = U(RegisterHotKey)(w, 23, 0, VK_F24);
    t_note("no modifiers -> %d err=%lu", r, GetLastError());
    if (r) EQ(U(UnregisterHotKey)(w, 23), TRUE);
    hkkill(w);
}
TEST(hotkey_invalid_modifier_bits_observed) {
    HKNEED();
    HWND w = hkw();
    static const UINT bad[] = {0x10, 0x20, 0x80, 0x100, 0x1000, 0x2000, 0x8000, 0x10000, 0x80000000, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        BOOL r = U(RegisterHotKey)(w, 30, MODS3 | bad[i], VK_F24);
        t_note("modifiers 0x%x -> %d err=%lu", MODS3 | bad[i], r, GetLastError());
        if (r) U(UnregisterHotKey)(w, 30);
    }
    hkkill(w);
}
TEST(hotkey_invalid_vk_observed) {
    HKNEED();
    HWND w = hkw();
    static const UINT vks[] = {0, 1, 0xFF, 0x100, 0x1FF, 0xFFFF, 0x10000, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof vks / sizeof vks[0]; i++) {
        SetLastError(0);
        BOOL r = U(RegisterHotKey)(w, 31, MODS3, vks[i]);
        t_note("vk 0x%x -> %d err=%lu", vks[i], r, GetLastError());
        if (r) U(UnregisterHotKey)(w, 31);
    }
    hkkill(w);
}
TEST(hotkey_id_range) {
    HKNEED();
    HWND w = hkw();
    static const int ok_ids[] = {0, 1, 0x7FFF, 0xBFFF, 0xC000, 0xFFFF};
    for (unsigned i = 0; i < sizeof ok_ids / sizeof ok_ids[0]; i++) {
        SetLastError(0);
        BOOL r = U(RegisterHotKey)(w, ok_ids[i], MODS3, VK_F24);
        CHECKF(r, "id 0x%x err %lu", ok_ids[i], GetLastError());
        if (r) CHECKF(U(UnregisterHotKey)(w, ok_ids[i]), "unregister id 0x%x", ok_ids[i]);
    }
    static const int odd_ids[] = {0x10000, 0x7FFFFFFF, -1, (int)0x80000000};
    for (unsigned i = 0; i < sizeof odd_ids / sizeof odd_ids[0]; i++) {
        SetLastError(0);
        BOOL r = U(RegisterHotKey)(w, odd_ids[i], MODS3, VK_F24);
        t_note("id 0x%x -> %d err=%lu", odd_ids[i], r, GetLastError());
        if (r) U(UnregisterHotKey)(w, odd_ids[i]);
    }
    hkkill(w);
}
TEST(hotkey_null_hwnd) {
    HKNEED();
    SetLastError(0);
    BOOL r = U(RegisterHotKey)(NULL, 40, MODS3, VK_F24);
    CHECKF(r, "NULL hwnd err %lu", GetLastError());
    if (r) {
        SetLastError(0);
        EQ(U(RegisterHotKey)(NULL, 40, MODS3, VK_F24), FALSE);
        EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
        EQ(U(UnregisterHotKey)(NULL, 40), TRUE);
        SetLastError(0);
        EQ(U(UnregisterHotKey)(NULL, 40), FALSE);
        EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    }
}
TEST(hotkey_null_hwnd_posts_to_thread_queue) {
    HKNEED();
    BOOL r = U(RegisterHotKey)(NULL, 41, MODS3, VK_F24);
    CHECK(r);
    /* A synthetic WM_HOTKEY for the thread has the documented layout. */
    PostThreadMessageA(GetCurrentThreadId(), WM_HOTKEY, 41, MAKELPARAM(MODS3, VK_F24));
    MSG m; int got = 0;
    DWORD end = GetTickCount() + 2000;
    while (!got && GetTickCount() < end) { if (PeekMessageA(&m, NULL, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) got = 1; else Sleep(2); }
    CHECK(got);
    if (got) { EQ(m.hwnd, NULL); EQ(m.wParam, 41); EQ(LOWORD(m.lParam), MODS3); EQ(HIWORD(m.lParam), VK_F24); }
    if (r) U(UnregisterHotKey)(NULL, 41);
}
TEST(hotkey_wm_hotkey_layout_via_wndproc) {
    HKNEED();
    HWND w = hkw();
    SendMessageA(w, WM_HOTKEY, 0x55, MAKELPARAM(MOD_ALT | MOD_CONTROL, 0x87));
    EQ(hk_count, 1);
    EQ(hk_wp, 0x55);
    EQ(LOWORD(hk_lp), MOD_ALT | MOD_CONTROL);
    EQ(HIWORD(hk_lp), 0x87);
    hkkill(w);
}
TEST(hotkey_destroy_window_releases) {
    HKNEED();
    HWND w = hkw();
    CHECK(U(RegisterHotKey)(w, 50, MODS3, VK_F24));
    U(DestroyWindow)(w);
    SetLastError(0);
    BOOL r = U(RegisterHotKey)(NULL, 51, MODS3, VK_F24);
    CHECKF(r, "key still held after window destruction, err %lu", GetLastError());
    if (r) U(UnregisterHotKey)(NULL, 51);
    dh_destroy_window(NULL, HKCLS);
}
TEST(hotkey_register_unregister_loop) {
    HKNEED();
    HWND w = hkw();
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        if (!U(RegisterHotKey)(w, i & 0x7FFF, MODS3, VK_F24)) { bad++; break; }
        if (!U(UnregisterHotKey)(w, i & 0x7FFF)) { bad++; break; }
    }
    EQ(bad, 0);
    hkkill(w);
}
TEST(hotkey_many_registrations) {
    HKNEED();
    HWND w = hkw();
    int ok = 0, tot = 0;
    static unsigned ids[160];
    static const UINT sets[8] = {MOD_CONTROL | MOD_ALT, MOD_CONTROL | MOD_SHIFT, MOD_ALT | MOD_SHIFT, MODS3,
                                 MOD_CONTROL | MOD_ALT | MOD_WIN, MOD_CONTROL | MOD_SHIFT | MOD_WIN, MOD_ALT | MOD_SHIFT | MOD_WIN, MODS3 | MOD_WIN};
    for (UINT vk = VK_F13; vk <= VK_F24; vk++)
        for (UINT v = 0; v < 8; v++) {
            unsigned id = 100 + tot;
            if (U(RegisterHotKey)(w, id, sets[v], vk)) { ids[ok++] = id; }
            tot++;
        }
    t_note("%d of %d registrations succeeded", ok, tot);
    CHECK(ok >= tot - 4);       /* tolerate a few collisions with other applications */
    for (int i = 0; i < ok; i++) CHECKF(U(UnregisterHotKey)(w, ids[i]), "unregister %u", ids[i]);
    hkkill(w);
}
static volatile LONG t_ready, t_stop, t_reg;
static HWND t_wnd;
static DWORD WINAPI hk_thread(LPVOID p) {
    MSG m;
    t_wnd = U(CreateWindowExA)(0, HKCLS, "ot", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    t_reg = U(RegisterHotKey)(t_wnd, 60, MODS3, VK_F24);
    t_ready = 1;
    while (!t_stop) {
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
    }
    return 0;
}
TEST(hotkey_other_thread_owns_key) {
    HKNEED();
    HWND mine = hkw();
    t_ready = t_stop = t_reg = 0;
    HANDLE t = CreateThread(NULL, 0, hk_thread, NULL, 0, NULL);
    DWORD end = GetTickCount() + 5000;
    while (!t_ready && GetTickCount() < end) Sleep(5);
    CHECK(t_ready);
    CHECK(t_reg);
    if (t_reg) {
        SetLastError(0);
        EQ(U(RegisterHotKey)(mine, 1, MODS3, VK_F24), FALSE);
        EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
        /* we cannot register for, or unregister from, a window of another thread */
        SetLastError(0);
        EQ(U(RegisterHotKey)(t_wnd, 61, MODS3, VK_F23), FALSE);
        EQ(GetLastError(), ERROR_WINDOW_OF_OTHER_THREAD);
        SetLastError(0);
        EQ(U(UnregisterHotKey)(t_wnd, 60), FALSE);
        t_note("UnregisterHotKey on another thread's window err=%lu", GetLastError());
        SetLastError(0);
        EQ(U(UnregisterHotKey)(mine, 60), FALSE);
        EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    }
    t_stop = 1;
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    /* the owning thread (and its window) are gone: the key is free again */
    BOOL r = FALSE;
    for (int i = 0; i < 50 && !r; i++) { r = U(RegisterHotKey)(mine, 1, MODS3, VK_F24); if (!r) Sleep(20); }
    CHECK(r);
    if (r) U(UnregisterHotKey)(mine, 1);
    hkkill(mine);
}
TEST(hotkey_constants) {
    EQ(MOD_ALT, 1); EQ(MOD_CONTROL, 2); EQ(MOD_SHIFT, 4); EQ(MOD_WIN, 8); EQ(MOD_NOREPEAT, 0x4000);
    EQ(WM_HOTKEY, 0x312);
    EQ(ERROR_HOTKEY_ALREADY_REGISTERED, 1409); EQ(ERROR_HOTKEY_NOT_REGISTERED, 1419);
    EQ(ERROR_WINDOW_OF_OTHER_THREAD, 1408);
    EQ(VK_F24, 0x87);
}
