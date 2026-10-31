/* RegisterHotKey / UnregisterHotKey. Uses Ctrl+Shift+F13..F24 style combinations. */
#include "kbd.h"

#define NT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define HKREQ REQUIRE(U(RegisterHotKey)); REQUIRE(U(UnregisterHotKey))
#define MODS (MOD_CONTROL | MOD_ALT | MOD_SHIFT)

static HWND hk_window(void) { return kw_plain(); }

static BOOL reg(HWND w, int id, UINT mod, UINT vk)
{
    SET_ERR(0);
    return U(RegisterHotKey)(w, id, mod, vk);
}

TEST(input_kbd_hotkey_register_unregister_basic) {
    HWND w;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    if (!reg(w, 1, MODS, VK_F24)) { t_skip("cannot register (err %lu)", GetLastError()); U(DestroyWindow)(w); return; }
    CHECK(U(UnregisterHotKey)(w, 1));
    SET_ERR(0);
    CHECK(!U(UnregisterHotKey)(w, 1));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_unregister_unknown) {
    HWND w;
    HKREQ;
    SET_ERR(0xdeadbeef);
    CHECK(!U(UnregisterHotKey)(NULL, 0));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    SET_ERR(0xdeadbeef);
    CHECK(!U(UnregisterHotKey)(w, 0));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    SET_ERR(0xdeadbeef);
    CHECK(!U(UnregisterHotKey)(w, 12345));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    SET_ERR(0xdeadbeef);
    CHECK(!U(UnregisterHotKey)(NULL, 12345));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_duplicate_registration) {
    HWND w;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    if (!reg(w, 5, MODS, VK_F24)) { t_skip("cannot register (err %lu)", GetLastError()); U(DestroyWindow)(w); return; }
    /* same key, different id */
    CHECK(!reg(w, 4, MODS, VK_F24));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
    /* same key, thread hotkey */
    CHECK(!reg(NULL, 5, MODS, VK_F24));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
    /* same everything */
    CHECK(!reg(w, 5, MODS, VK_F24));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_ALREADY_REGISTERED);
    CHECK(U(UnregisterHotKey)(w, 5));
    /* now it is free again */
    CHECK(reg(w, 4, MODS, VK_F24));
    CHECK(U(UnregisterHotKey)(w, 4));
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_same_id_different_key_observed) {
    HWND w;
    BOOL r;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    if (!reg(w, 9, MODS, VK_F24)) { t_skip("cannot register"); U(DestroyWindow)(w); return; }
    r = reg(w, 9, MODS, VK_F23);
    t_note("registering the same id for a different key: %d (err %lu)", r, GetLastError());
    if (r) {
        BOOL a = U(UnregisterHotKey)(w, 9), b;
        SET_ERR(0);
        b = U(UnregisterHotKey)(w, 9);
        t_note("unregister id 9: first %d, second %d", a, b);
        if (b) U(UnregisterHotKey)(w, 9);
    } else U(UnregisterHotKey)(w, 9);
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_thread_hotkey_null_window) {
    HKREQ;
    if (!reg(NULL, 77, MODS, VK_F23)) { t_skip("cannot register (err %lu)", GetLastError()); return; }
    CHECK(U(UnregisterHotKey)(NULL, 77));
    SET_ERR(0);
    CHECK(!U(UnregisterHotKey)(NULL, 77));
    EXPECT_EQ(GetLastError(), ERROR_HOTKEY_NOT_REGISTERED);
}
TEST(input_kbd_hotkey_window_vs_thread_id_namespaces) {
    HWND w;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    if (!reg(NULL, 31, MODS, VK_F22)) { t_skip("cannot register"); U(DestroyWindow)(w); return; }
    /* same id on a window is a different hotkey */
    if (reg(w, 31, MODS, VK_F21)) {
        CHECK(U(UnregisterHotKey)(NULL, 31));
        CHECK(U(UnregisterHotKey)(w, 31));
    } else {
        t_note("same id for thread and window hotkey rejected: %lu", GetLastError());
        U(UnregisterHotKey)(NULL, 31);
    }
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_invalid_window_handle) {
    static const ULONG_PTR h[] = { 0x1234, 0xDEADBEEF, 0xFFFFFFFF };
    int i;
    HKREQ;
    for (i = 0; i < NT(h); i++) {
        CHECKF(!reg((HWND)h[i], 1, MODS, VK_F24), "bogus %p accepted", (void *)h[i]);
        EXPECT_EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
        SET_ERR(0);
        CHECK(!U(UnregisterHotKey)((HWND)h[i], 1));
        t_note("UnregisterHotKey(bogus %p) error %lu", (void *)h[i], GetLastError());
    }
}
TEST(input_kbd_hotkey_destroyed_window_handle) {
    HWND w;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    U(DestroyWindow)(w);
    CHECK(!reg(w, 1, MODS, VK_F24));
    EXPECT_EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
}
static HWND g_other_wnd;
static HANDLE g_ev_up, g_ev_down;
static DWORD WINAPI other_thread(void *p) {
    MSG m;
    g_other_wnd = kw_plain();
    SetEvent(g_ev_up);
    WaitForSingleObject(g_ev_down, 5000);
    if (g_other_wnd) U(DestroyWindow)(g_other_wnd);
    while (U(PeekMessageA)(&m, NULL, 0, 0, PM_REMOVE)) U(DispatchMessageA)(&m);
    return 0;
}
TEST(input_kbd_hotkey_window_of_other_thread) {
    HANDLE th;
    BOOL r;
    HKREQ;
    g_ev_up = CreateEventA(NULL, TRUE, FALSE, NULL); g_ev_down = CreateEventA(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, other_thread, NULL, 0, NULL);
    CHECK(th != NULL);
    if (th) {
        WaitForSingleObject(g_ev_up, 5000);
        CHECK(g_other_wnd != NULL);
        if (g_other_wnd) {
            r = reg(g_other_wnd, 3, MODS, VK_F24);
            CHECK(!r);
            if (r) U(UnregisterHotKey)(g_other_wnd, 3);
            else EXPECT_EQ(GetLastError(), ERROR_WINDOW_OF_OTHER_THREAD);
        }
        SetEvent(g_ev_down);
        WaitForSingleObject(th, 5000);
        CloseHandle(th);
    }
    CloseHandle(g_ev_up); CloseHandle(g_ev_down);
}
TEST(input_kbd_hotkey_unregister_window_of_other_thread_observed) {
    HANDLE th;
    HKREQ;
    g_ev_up = CreateEventA(NULL, TRUE, FALSE, NULL); g_ev_down = CreateEventA(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, other_thread, NULL, 0, NULL);
    CHECK(th != NULL);
    if (th) {
        WaitForSingleObject(g_ev_up, 5000);
        if (g_other_wnd) {
            SET_ERR(0);
            CHECK(!U(UnregisterHotKey)(g_other_wnd, 3));
            t_note("UnregisterHotKey(window of other thread) error %lu", GetLastError());
        }
        SetEvent(g_ev_down);
        WaitForSingleObject(th, 5000);
        CloseHandle(th);
    }
    CloseHandle(g_ev_up); CloseHandle(g_ev_down);
}
TEST(input_kbd_hotkey_id_range_edges) {
    static const int ids[] = { 0, 1, 0x7FFF, 0xBFFF };
    HWND w;
    int i;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    for (i = 0; i < NT(ids); i++) {
        if (!reg(w, ids[i], MODS, VK_F24)) { t_note("id 0x%x rejected: %lu", ids[i], GetLastError()); continue; }
        CHECKF(U(UnregisterHotKey)(w, ids[i]), "unregister 0x%x", ids[i]);
    }
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_id_out_of_range_observed) {
    static const int ids[] = { 0xC000, 0xFFFF, 0x10000, -1, 0x7FFFFFFF };
    HWND w;
    int i;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    for (i = 0; i < NT(ids); i++) {
        BOOL r = reg(w, ids[i], MODS, VK_F24);
        t_note("RegisterHotKey(window, id 0x%x) = %d err %lu", ids[i], r, GetLastError());
        if (r) U(UnregisterHotKey)(w, ids[i]);
    }
    CHECK(1);
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_norepeat_flag_accepted) {
    HWND w;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    if (reg(w, 2, MOD_NOREPEAT | MODS, VK_F24)) {
        BOOL r = reg(w, 3, MODS, VK_F24);
        t_note("same key without NOREPEAT: %d err %lu", r, GetLastError());
        if (r) U(UnregisterHotKey)(w, 3);
        CHECK(U(UnregisterHotKey)(w, 2));
    } else t_skip("cannot register");
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_all_modifier_combinations) {
    HWND w;
    UINT mod;
    int ok = 0;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    for (mod = 1; mod < 16; mod++) {
        /* MOD_WIN combos may already belong to the shell; only judge the ones we obtained */
        if (reg(w, 100 + mod, mod, VK_F24)) {
            ok++;
            CHECKF(U(UnregisterHotKey)(w, 100 + mod), "unregister mod %u", mod);
        } else t_note("modifiers %u not registrable: %lu", mod, GetLastError());
    }
    CHECK(ok >= 7);
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_no_modifier_hotkey_observed) {
    HWND w;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    if (reg(w, 6, 0, VK_F24)) {
        CHECK(U(UnregisterHotKey)(w, 6));
    } else t_note("hotkey without modifiers rejected: %lu", GetLastError());
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_invalid_vk_observed) {
    static const UINT vks[] = { 0, 0x100, 0xFFFF, 0xFF, 0x7FFFFFFF };
    HWND w;
    int i;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    for (i = 0; i < NT(vks); i++) {
        BOOL r = reg(w, 7, MODS, vks[i]);
        t_note("RegisterHotKey(vk=0x%x) = %d err %lu", vks[i], r, GetLastError());
        if (r) U(UnregisterHotKey)(w, 7);
    }
    CHECK(1);
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_invalid_modifiers_observed) {
    static const UINT mods[] = { 0x10, 0x20, 0x100, 0x8000, 0x10000, 0xFFFF0000, 0xFFFFFFFF };
    HWND w;
    int i;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    for (i = 0; i < NT(mods); i++) {
        BOOL r = reg(w, 8, mods[i], VK_F24);
        t_note("RegisterHotKey(mod=0x%x) = %d err %lu", mods[i], r, GetLastError());
        if (r) U(UnregisterHotKey)(w, 8);
    }
    CHECK(1);
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_window_destroy_releases_key) {
    HWND w;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    if (!reg(w, 1, MODS, VK_F24)) { t_skip("cannot register"); U(DestroyWindow)(w); return; }
    U(DestroyWindow)(w);
    CHECK(reg(NULL, 1, MODS, VK_F24));
    CHECK(U(UnregisterHotKey)(NULL, 1));
}
static DWORD WINAPI hk_thread(void *p) {
    MSG m;
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);
    *(BOOL *)p = U(RegisterHotKey)(NULL, 1, MODS, VK_F24);
    return 0;
}
TEST(input_kbd_hotkey_thread_exit_releases_key) {
    HANDLE th;
    BOOL registered = FALSE;
    HKREQ;
    th = CreateThread(NULL, 0, hk_thread, &registered, 0, NULL);
    CHECK(th != NULL);
    if (!th) return;
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    if (!registered) { t_skip("worker could not register"); return; }
    Sleep(50);
    CHECK(reg(NULL, 1, MODS, VK_F24));
    U(UnregisterHotKey)(NULL, 1);
}
TEST(input_kbd_hotkey_many_registrations) {
    int i, n = 0, bad = 0;
    HWND w;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    for (i = 0; i < 12 * 4; i++) {
        UINT vk = VK_F13 + (i % 12), mod = (i / 12 == 0) ? (MOD_CONTROL | MOD_ALT) : (i / 12 == 1) ? (MOD_CONTROL | MOD_SHIFT | MOD_ALT) : (i / 12 == 2) ? (MOD_ALT | MOD_SHIFT) : (MOD_CONTROL | MOD_SHIFT);
        if (reg(w, 1000 + i, mod, vk)) n++; else bad++;
    }
    t_note("registered %d of %d hotkeys", n, 48);
    CHECK(n >= 40);
    for (i = 0; i < 48; i++) U(UnregisterHotKey)(w, 1000 + i);
    for (i = 0; i < 48; i++) { SET_ERR(0); CHECK(!U(UnregisterHotKey)(w, 1000 + i)); }
    U(DestroyWindow)(w);
}
TEST(input_kbd_hotkey_register_unregister_cycles) {
    HWND w;
    int i, bad = 0;
    HKREQ;
    w = hk_window();
    CHECK(w != NULL);
    if (!w) return;
    if (!reg(w, 1, MODS, VK_F24)) { t_skip("cannot register"); U(DestroyWindow)(w); return; }
    U(UnregisterHotKey)(w, 1);
    for (i = 0; i < 500; i++) {
        if (!U(RegisterHotKey)(w, 1, MODS, VK_F24)) bad++;
        if (!U(UnregisterHotKey)(w, 1)) bad++;
    }
    CHECK(bad == 0);
    U(DestroyWindow)(w);
}

/* ---- injected hotkeys ---- */
#define KBEGIN HWND w = kw_begin(); if (!w) return

static void press_combo(void)
{
    kw_down(VK_LCONTROL); kw_down(VK_LSHIFT); kw_down(VK_F24);
    kw_up(VK_F24); kw_up(VK_LSHIFT); kw_up(VK_LCONTROL);
}

TEST(input_kbd_hotkey_inj_window_receives_wm_hotkey) {
    struct klog *h;
    KBEGIN;
    HKREQ;
    if (!reg(w, 42, MOD_CONTROL | MOD_SHIFT, VK_F24)) { kw_end(w); t_skip("cannot register (err %lu)", GetLastError()); return; }
    press_combo();
    kw_pump(2500, 4, 0);
    h = kw_find(WM_HOTKEY, 0);
    CHECK(h != NULL);
    if (h) {
        EXPECT_EQ(h->wp, 42);
        EXPECT_EQ(LOWORD(h->lp), MOD_CONTROL | MOD_SHIFT);
        EXPECT_EQ(HIWORD(h->lp), VK_F24);
    }
    EXPECT_EQ(kw_count(WM_HOTKEY), 1);
    {
        int i, f24 = 0;
        for (i = 0; i < g_nklog; i++) if (g_klog[i].msg == WM_KEYDOWN && g_klog[i].wp == VK_F24) f24++;
        EXPECT_EQ(f24, 0);       /* the hotkey key press is swallowed */
    }
    U(UnregisterHotKey)(w, 42);
    kw_end(w);
}
TEST(input_kbd_hotkey_inj_not_triggered_without_modifiers) {
    KBEGIN;
    HKREQ;
    if (!reg(w, 43, MOD_CONTROL | MOD_SHIFT, VK_F24)) { kw_end(w); t_skip("cannot register"); return; }
    kw_down(VK_F24); kw_up(VK_F24);
    kw_pump(1500, 2, 0);
    EXPECT_EQ(kw_count(WM_HOTKEY), 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    kw_clear();
    kw_down(VK_LCONTROL); kw_down(VK_F24); kw_up(VK_F24); kw_up(VK_LCONTROL);       /* shift missing */
    kw_pump(1500, 4, 0);
    EXPECT_EQ(kw_count(WM_HOTKEY), 0);
    U(UnregisterHotKey)(w, 43);
    kw_end(w);
}
TEST(input_kbd_hotkey_inj_extra_modifier_does_not_match) {
    KBEGIN;
    HKREQ;
    if (!reg(w, 44, MOD_CONTROL | MOD_SHIFT, VK_F24)) { kw_end(w); t_skip("cannot register"); return; }
    kw_down(VK_LCONTROL); kw_down(VK_LSHIFT); kw_down(VK_LMENU); kw_down(VK_F24);
    kw_up(VK_F24); kw_up(VK_LMENU); kw_up(VK_LSHIFT); kw_up(VK_LCONTROL);
    kw_pump(2500, 8, 0);
    EXPECT_EQ(kw_count(WM_HOTKEY), 0);
    U(UnregisterHotKey)(w, 44);
    kw_end(w);
}
TEST(input_kbd_hotkey_inj_unregistered_no_longer_fires) {
    KBEGIN;
    HKREQ;
    if (!reg(w, 45, MOD_CONTROL | MOD_SHIFT, VK_F24)) { kw_end(w); t_skip("cannot register"); return; }
    CHECK(U(UnregisterHotKey)(w, 45));
    press_combo();
    kw_pump(2500, 5, 0);
    EXPECT_EQ(kw_count(WM_HOTKEY), 0);
    {
        int i, f24 = 0;
        for (i = 0; i < g_nklog; i++) if (g_klog[i].msg == WM_KEYDOWN && g_klog[i].wp == VK_F24) f24++;
        EXPECT_EQ(f24, 1);
    }
    kw_end(w);
}
TEST(input_kbd_hotkey_inj_thread_hotkey_posts_to_queue) {
    MSG m;
    int got = 0;
    DWORD end;
    KBEGIN;
    HKREQ;
    if (!reg(NULL, 46, MOD_CONTROL | MOD_SHIFT, VK_F24)) { kw_end(w); t_skip("cannot register"); return; }
    press_combo();
    end = GetTickCount() + 2500;
    while ((LONG)(GetTickCount() - end) < 0 && !got) {
        if (U(PeekMessageA)(&m, NULL, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {
            got = 1;
            CHECK(m.hwnd == NULL);
            EXPECT_EQ(m.wParam, 46);
            EXPECT_EQ(m.lParam, MAKELPARAM(MOD_CONTROL | MOD_SHIFT, VK_F24));
        } else Sleep(5);
    }
    CHECK(got);
    U(UnregisterHotKey)(NULL, 46);
    kw_end(w);
}
TEST(input_kbd_hotkey_inj_queue_status_flag) {
    DWORD end, q = 0;
    MSG m;
    int seen = 0;
    KBEGIN;
    HKREQ; REQUIRE(U(GetQueueStatus));
    if (!reg(NULL, 47, MOD_CONTROL | MOD_SHIFT, VK_F24)) { kw_end(w); t_skip("cannot register"); return; }
    press_combo();
    end = GetTickCount() + 2500;
    while ((LONG)(GetTickCount() - end) < 0) {
        q = U(GetQueueStatus)(QS_HOTKEY);
        if ((q >> 16) & QS_HOTKEY) { seen = 1; break; }
        Sleep(5);
    }
    CHECK(seen);
    while (U(PeekMessageA)(&m, NULL, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {}
    q = U(GetQueueStatus)(QS_HOTKEY);
    CHECK(((q >> 16) & QS_HOTKEY) == 0);
    U(UnregisterHotKey)(NULL, 47);
    kw_end(w);
}
TEST(input_kbd_hotkey_inj_norepeat_single_message) {
    int i;
    KBEGIN;
    HKREQ;
    if (!reg(w, 48, MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, VK_F24)) { kw_end(w); t_skip("cannot register"); return; }
    kw_down(VK_LCONTROL); kw_down(VK_LSHIFT);
    for (i = 0; i < 5; i++) kw_down(VK_F24);
    kw_up(VK_F24); kw_up(VK_LSHIFT); kw_up(VK_LCONTROL);
    kw_pump(3000, 12, 0);
    EXPECT_EQ(kw_count(WM_HOTKEY), 1);
    U(UnregisterHotKey)(w, 48);
    kw_end(w);
}
TEST(input_kbd_hotkey_inj_repeat_without_norepeat_observed) {
    int i;
    KBEGIN;
    HKREQ;
    if (!reg(w, 49, MOD_CONTROL | MOD_SHIFT, VK_F24)) { kw_end(w); t_skip("cannot register"); return; }
    kw_down(VK_LCONTROL); kw_down(VK_LSHIFT);
    for (i = 0; i < 5; i++) kw_down(VK_F24);
    kw_up(VK_F24); kw_up(VK_LSHIFT); kw_up(VK_LCONTROL);
    kw_pump(3000, 12, 0);
    t_note("auto-repeat without MOD_NOREPEAT produced %d WM_HOTKEY", kw_count(WM_HOTKEY));
    CHECK(kw_count(WM_HOTKEY) >= 1);
    U(UnregisterHotKey)(w, 49);
    kw_end(w);
}
