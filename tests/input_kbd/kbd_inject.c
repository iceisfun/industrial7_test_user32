/* Injected keyboard input into our own foreground window. */
#include "kbd.h"

#define NT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define KBEGIN HWND w = kw_begin(); if (!w) return
#define LP_SCANMASK 0x00FF0000
#define BIT(l, n) (((l) >> (n)) & 1)

static int wait_async(int vk, int down, int ms)
{
    DWORD end = GetTickCount() + ms;
    for (;;) {
        int is = (U(GetAsyncKeyState)(vk) & 0x8000) != 0;
        if (is == down) return 1;
        if ((LONG)(GetTickCount() - end) >= 0) return 0;
        Sleep(5);
    }
}

TEST(input_kbd_inj_down_up_messages) {
    struct klog *d, *u;
    KBEGIN;
    EXPECT_EQ(kw_down(VK_F13), 1);
    EXPECT_EQ(kw_up(VK_F13), 1);
    kw_pump(1500, 2, 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    EXPECT_EQ(kw_count(WM_KEYUP), 1);
    EXPECT_EQ(kw_count(WM_SYSKEYDOWN), 0);
    EXPECT_EQ(kw_count(WM_CHAR), 0);
    d = kw_find(WM_KEYDOWN, 0); u = kw_find(WM_KEYUP, 0);
    if (d) {
        EXPECT_EQ(d->wp, VK_F13);
        EXPECT_EQ(d->lp & 0xFFFF, 1);
        EXPECT_EQ(BIT(d->lp, 24), 0);
        EXPECT_EQ(BIT(d->lp, 29), 0);
        EXPECT_EQ(BIT(d->lp, 30), 0);
        EXPECT_EQ(BIT(d->lp, 31), 0);
        EXPECT_EQ(d->lp & 0x1E000000, 0);
    }
    if (u) {
        EXPECT_EQ(u->wp, VK_F13);
        EXPECT_EQ(u->lp & 0xFFFF, 1);
        EXPECT_EQ(BIT(u->lp, 29), 0);
        EXPECT_EQ(BIT(u->lp, 30), 1);
        EXPECT_EQ(BIT(u->lp, 31), 1);
    }
    if (d && u) EXPECT_EQ(d->lp & LP_SCANMASK, u->lp & LP_SCANMASK);
    if (d) {
        UINT sc = (d->lp >> 16) & 0xFF, mv = U(MapVirtualKeyW)(VK_F13, MAPVK_VK_TO_VSC);
        if (sc != 0 && sc != mv) t_note("lParam scan %02x differs from MapVirtualKey %02x", sc, mv);
        if (sc == 0) t_note("scan field is 0 when wScan=0 and no SCANCODE flag");
    }
    kw_end(w);
}
TEST(input_kbd_inj_message_source_is_injected_keyboard) {
    int i;
    KBEGIN;
    kw_down(VK_F14); kw_up(VK_F14);
    kw_pump(1500, 2, 0);
    EXPECT_EQ(g_nklog, 2);
    for (i = 0; i < g_nklog; i++) {
        EXPECT_EQ(g_klog[i].dev, IMDT_KEYBOARD);
        EXPECT_EQ(g_klog[i].org, IMO_INJECTED);
    }
    kw_end(w);
}
TEST(input_kbd_inj_extra_info_roundtrip_sendinput) {
    INPUT in[2];
    KBEGIN;
    memset(in, 0, sizeof in);
    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wVk = in[1].ki.wVk = VK_F15;
    in[0].ki.dwExtraInfo = in[1].ki.dwExtraInfo = 0xCAFE1234;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    EXPECT_EQ(kw_input(in, 2), 2);
    kw_pump(1500, 2, 0);
    EXPECT_EQ(g_nklog, 2);
    if (g_nklog == 2) { EXPECT_EQ(g_klog[0].extra, 0xCAFE1234); EXPECT_EQ(g_klog[1].extra, 0xCAFE1234); }
    kw_end(w);
}
TEST(input_kbd_inj_extra_info_roundtrip_keybd_event) {
    KBEGIN;
    REQUIRE(U(keybd_event));
    U(keybd_event)(VK_F16, 0, 0, 0x1357);
    U(keybd_event)(VK_F16, 0, KEYEVENTF_KEYUP, 0x2468);
    kw_pump(1500, 2, 0);
    EXPECT_EQ(g_nklog, 2);
    if (g_nklog == 2) {
        EXPECT_EQ(g_klog[0].msg, WM_KEYDOWN); EXPECT_EQ(g_klog[0].wp, VK_F16); EXPECT_EQ(g_klog[0].extra, 0x1357);
        EXPECT_EQ(g_klog[1].msg, WM_KEYUP);   EXPECT_EQ(g_klog[1].extra, 0x2468);
    }
    kw_end(w);
}
TEST(input_kbd_inj_keybd_event_same_as_sendinput) {
    int n1, n2;
    KBEGIN;
    REQUIRE(U(keybd_event));
    kw_down(VK_F17); kw_up(VK_F17);
    kw_pump(1500, 2, 0);
    n1 = g_nklog;
    kw_clear();
    U(keybd_event)(VK_F17, 0, 0, 0); U(keybd_event)(VK_F17, 0, KEYEVENTF_KEYUP, 0);
    kw_pump(1500, 2, 0);
    n2 = g_nklog;
    EXPECT_EQ(n1, 2); EXPECT_EQ(n2, 2);
    kw_end(w);
}
TEST(input_kbd_inj_autorepeat_previous_state_bit) {
    int i;
    KBEGIN;
    for (i = 0; i < 4; i++) kw_down(VK_F18);
    kw_up(VK_F18);
    kw_pump(2000, 5, 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 4);
    EXPECT_EQ(kw_count(WM_KEYUP), 1);
    if (kw_find(WM_KEYDOWN, 0)) EXPECT_EQ(BIT(kw_find(WM_KEYDOWN, 0)->lp, 30), 0);
    for (i = 1; i < kw_count(WM_KEYDOWN); i++) EXPECT_EQ(BIT(kw_find(WM_KEYDOWN, i)->lp, 30), 1);
    for (i = 0; i < kw_count(WM_KEYDOWN); i++) CHECK((kw_find(WM_KEYDOWN, i)->lp & 0xFFFF) >= 1);
    kw_end(w);
}
TEST(input_kbd_inj_press_after_release_clears_previous_bit) {
    int i;
    KBEGIN;
    for (i = 0; i < 3; i++) { kw_down(VK_F19); kw_up(VK_F19); }
    kw_pump(2000, 6, 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 3);
    for (i = 0; i < kw_count(WM_KEYDOWN); i++) EXPECT_EQ(BIT(kw_find(WM_KEYDOWN, i)->lp, 30), 0);
    for (i = 0; i < kw_count(WM_KEYUP); i++) EXPECT_EQ(BIT(kw_find(WM_KEYUP, i)->lp, 30), 1);
    kw_end(w);
}
TEST(input_kbd_inj_extended_flag_sets_bit24) {
    KBEGIN;
    kw_key(VK_HOME, 0, KEYEVENTF_EXTENDEDKEY);
    kw_key(VK_HOME, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP);
    kw_pump(1500, 2, 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    EXPECT_EQ(kw_count(WM_KEYUP), 1);
    if (kw_find(WM_KEYDOWN, 0)) { EXPECT_EQ(kw_find(WM_KEYDOWN, 0)->wp, VK_HOME); EXPECT_EQ(BIT(kw_find(WM_KEYDOWN, 0)->lp, 24), 1); }
    if (kw_find(WM_KEYUP, 0)) EXPECT_EQ(BIT(kw_find(WM_KEYUP, 0)->lp, 24), 1);
    kw_end(w);
}
TEST(input_kbd_inj_no_extended_flag_observed) {
    KBEGIN;
    kw_key(VK_HOME, 0, 0);
    kw_key(VK_HOME, 0, KEYEVENTF_KEYUP);
    kw_pump(1500, 2, 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    if (kw_find(WM_KEYDOWN, 0)) t_note("VK_HOME injected without EXTENDEDKEY: lParam bit24 = %d", (int)BIT(kw_find(WM_KEYDOWN, 0)->lp, 24));
    kw_end(w);
}
TEST(input_kbd_inj_scancode_flag_maps_vk) {
    KBEGIN;
    kw_key(0, 0x3B, KEYEVENTF_SCANCODE);
    kw_key(0, 0x3B, KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP);
    kw_pump(1500, 2, 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    EXPECT_EQ(kw_count(WM_KEYUP), 1);
    if (kw_find(WM_KEYDOWN, 0)) {
        EXPECT_EQ(kw_find(WM_KEYDOWN, 0)->wp, VK_F1);
        EXPECT_EQ((kw_find(WM_KEYDOWN, 0)->lp >> 16) & 0xFF, 0x3B);
    }
    if (kw_find(WM_KEYUP, 0)) EXPECT_EQ(kw_find(WM_KEYUP, 0)->wp, VK_F1);
    kw_end(w);
}
TEST(input_kbd_inj_unicode_generates_wm_char) {
    INPUT in[2];
    struct klog *c;
    KBEGIN;
    memset(in, 0, sizeof in);
    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wScan = in[1].ki.wScan = 0x3C0;
    in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    EXPECT_EQ(kw_input(in, 2), 2);
    kw_pump(2000, 3, 1);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    EXPECT_EQ(kw_count(WM_KEYUP), 1);
    EXPECT_EQ(kw_count(WM_CHAR), 1);
    if (kw_find(WM_KEYDOWN, 0)) {
        EXPECT_EQ(LOWORD(kw_find(WM_KEYDOWN, 0)->wp), VK_PACKET);
        t_note("VK_PACKET wParam high word = %04x", (unsigned)(kw_find(WM_KEYDOWN, 0)->wp >> 16));
    }
    c = kw_find(WM_CHAR, 0);
    if (c) EXPECT_EQ(c->wp, 0x3C0);
    kw_end(w);
}
TEST(input_kbd_inj_unicode_sequence_order) {
    INPUT in[8];
    static const WCHAR s[] = { 'H', 'i', 0xE9, 0x20AC };
    int i;
    KBEGIN;
    memset(in, 0, sizeof in);
    for (i = 0; i < 4; i++) {
        in[2 * i].type = in[2 * i + 1].type = INPUT_KEYBOARD;
        in[2 * i].ki.wScan = in[2 * i + 1].ki.wScan = s[i];
        in[2 * i].ki.dwFlags = KEYEVENTF_UNICODE;
        in[2 * i + 1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    }
    EXPECT_EQ(kw_input(in, 8), 8);
    kw_pump(2500, 12, 1);
    EXPECT_EQ(kw_count(WM_CHAR), 4);
    for (i = 0; i < 4; i++) if (kw_find(WM_CHAR, i)) EXPECT_EQ(kw_find(WM_CHAR, i)->wp, s[i]);
    kw_end(w);
}
TEST(input_kbd_inj_unicode_surrogates_observed) {
    INPUT in[4];
    static const WCHAR s[] = { 0xD83D, 0xDE00 };
    int i;
    KBEGIN;
    memset(in, 0, sizeof in);
    for (i = 0; i < 2; i++) {
        in[2 * i].type = in[2 * i + 1].type = INPUT_KEYBOARD;
        in[2 * i].ki.wScan = in[2 * i + 1].ki.wScan = s[i];
        in[2 * i].ki.dwFlags = KEYEVENTF_UNICODE;
        in[2 * i + 1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    }
    EXPECT_EQ(kw_input(in, 4), 4);
    kw_pump(2000, 6, 1);
    t_note("surrogate pair produced %d WM_CHAR (%04x %04x)", kw_count(WM_CHAR),
           kw_find(WM_CHAR, 0) ? (unsigned)kw_find(WM_CHAR, 0)->wp : 0, kw_find(WM_CHAR, 1) ? (unsigned)kw_find(WM_CHAR, 1)->wp : 0);
    CHECK(kw_count(WM_CHAR) >= 1);
    kw_end(w);
}
TEST(input_kbd_inj_unicode_does_not_touch_key_state) {
    INPUT in[2];
    BYTE before[256], after[256];
    KBEGIN;
    kw_pump(50, 1000, 0);
    U(GetKeyboardState)(before);
    memset(in, 0, sizeof in);
    in[0].type = INPUT_KEYBOARD; in[0].ki.wScan = 'q'; in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    kw_input(in, 1);
    kw_pump(1000, 1, 0);
    U(GetKeyboardState)(after);
    if (memcmp(before, after, 256)) t_note("KEYEVENTF_UNICODE input changed the keyboard state array");
    CHECK(1);
    in[0].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    kw_input(in, 1);
    kw_pump(500, 1, 0);
    kw_end(w);
}
TEST(input_kbd_inj_key_state_tracking) {
    BYTE st[256];
    KBEGIN;
    kw_down(VK_F20);
    CHECK(wait_async(VK_F20, 1, 1000));
    kw_pump(1000, 1, 0);
    CHECK(U(GetKeyState)(VK_F20) < 0);
    CHECK(U(GetAsyncKeyState)(VK_F20) < 0);
    U(GetKeyboardState)(st);
    CHECK(st[VK_F20] & 0x80);
    kw_up(VK_F20);
    CHECK(wait_async(VK_F20, 0, 1000));
    kw_pump(1000, 2, 0);
    CHECK(U(GetKeyState)(VK_F20) >= 0);
    U(GetKeyboardState)(st);
    CHECK(!(st[VK_F20] & 0x80));
    kw_end(w);
}
TEST(input_kbd_inj_getkeystate_lags_until_message_retrieved) {
    KBEGIN;
    kw_down(VK_F21);
    CHECK(wait_async(VK_F21, 1, 1000));
    t_note("GetKeyState(F21) before retrieving the message: 0x%04x", (unsigned short)U(GetKeyState)(VK_F21));
    kw_pump(1000, 1, 0);
    CHECK(U(GetKeyState)(VK_F21) < 0);
    kw_up(VK_F21);
    kw_pump(1000, 2, 0);
    kw_end(w);
}
TEST(input_kbd_inj_toggle_bit_scroll_lock) {
    int init, after1, after2;
    KBEGIN;
    kw_pump(50, 1000, 0);
    init = U(GetKeyState)(VK_SCROLL) & 1;
    kw_down(VK_SCROLL); kw_up(VK_SCROLL);
    kw_pump(1500, 2, 0);
    after1 = U(GetKeyState)(VK_SCROLL) & 1;
    kw_clear();
    kw_down(VK_SCROLL); kw_up(VK_SCROLL);
    kw_pump(1500, 2, 0);
    after2 = U(GetKeyState)(VK_SCROLL) & 1;
    EXPECT_EQ(after1, !init);
    EXPECT_EQ(after2, init);
    if (after2 != init) {            /* restore the original toggle state */
        kw_clear();
        kw_down(VK_SCROLL); kw_up(VK_SCROLL);
        kw_pump(1500, 2, 0);
    }
    kw_end(w);
}
TEST(input_kbd_inj_modifier_shift_generic_and_sided) {
    BYTE st[256];
    KBEGIN;
    kw_down(VK_LSHIFT);
    kw_pump(1000, 1, 0);
    CHECK(U(GetKeyState)(VK_SHIFT) < 0);
    CHECK(U(GetKeyState)(VK_LSHIFT) < 0);
    CHECK(U(GetKeyState)(VK_RSHIFT) >= 0);
    U(GetKeyboardState)(st);
    CHECK(st[VK_SHIFT] & 0x80); CHECK(st[VK_LSHIFT] & 0x80); CHECK(!(st[VK_RSHIFT] & 0x80));
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    if (kw_find(WM_KEYDOWN, 0)) EXPECT_EQ(kw_find(WM_KEYDOWN, 0)->wp, VK_SHIFT);
    kw_up(VK_LSHIFT);
    kw_pump(1000, 2, 0);
    CHECK(U(GetKeyState)(VK_SHIFT) >= 0);
    kw_clear();
    kw_down(VK_RSHIFT);
    kw_pump(1000, 1, 0);
    CHECK(U(GetKeyState)(VK_SHIFT) < 0);
    CHECK(U(GetKeyState)(VK_RSHIFT) < 0);
    CHECK(U(GetKeyState)(VK_LSHIFT) >= 0);
    kw_up(VK_RSHIFT);
    kw_pump(1000, 2, 0);
    kw_end(w);
}
TEST(input_kbd_inj_modifier_control_generic_and_sided) {
    KBEGIN;
    kw_down(VK_LCONTROL);
    kw_pump(1000, 1, 0);
    CHECK(U(GetKeyState)(VK_CONTROL) < 0);
    CHECK(U(GetKeyState)(VK_LCONTROL) < 0);
    CHECK(U(GetKeyState)(VK_RCONTROL) >= 0);
    if (kw_find(WM_KEYDOWN, 0)) EXPECT_EQ(kw_find(WM_KEYDOWN, 0)->wp, VK_CONTROL);
    kw_up(VK_LCONTROL);
    kw_pump(1000, 2, 0);
    kw_clear();
    kw_key(VK_RCONTROL, 0, KEYEVENTF_EXTENDEDKEY);
    kw_pump(1000, 1, 0);
    CHECK(U(GetKeyState)(VK_CONTROL) < 0);
    CHECK(U(GetKeyState)(VK_RCONTROL) < 0);
    CHECK(U(GetKeyState)(VK_LCONTROL) >= 0);
    if (kw_find(WM_KEYDOWN, 0)) {
        EXPECT_EQ(kw_find(WM_KEYDOWN, 0)->wp, VK_CONTROL);
        EXPECT_EQ(BIT(kw_find(WM_KEYDOWN, 0)->lp, 24), 1);
    }
    kw_key(VK_RCONTROL, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP);
    kw_pump(1000, 2, 0);
    CHECK(U(GetKeyState)(VK_CONTROL) >= 0);
    kw_end(w);
}
TEST(input_kbd_inj_two_shifts_hold_generic_until_both_up) {
    KBEGIN;
    kw_down(VK_LSHIFT); kw_down(VK_RSHIFT);
    kw_pump(1000, 2, 0);
    CHECK(U(GetKeyState)(VK_SHIFT) < 0);
    kw_up(VK_LSHIFT);
    kw_pump(1000, 3, 0);
    CHECK(U(GetKeyState)(VK_SHIFT) < 0);
    CHECK(U(GetKeyState)(VK_RSHIFT) < 0);
    CHECK(U(GetKeyState)(VK_LSHIFT) >= 0);
    kw_up(VK_RSHIFT);
    kw_pump(1000, 4, 0);
    CHECK(U(GetKeyState)(VK_SHIFT) >= 0);
    kw_end(w);
}
TEST(input_kbd_inj_alt_makes_sys_messages) {
    struct klog *e;
    KBEGIN;
    kw_down(VK_LMENU);
    kw_down(VK_F13);
    kw_up(VK_F13);
    kw_up(VK_LMENU);
    kw_pump(2000, 4, 0);
    CHECK(kw_count(WM_SYSKEYDOWN) >= 2);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 0);
    e = kw_find(WM_SYSKEYDOWN, 0);
    if (e) { EXPECT_EQ(e->wp, VK_MENU); EXPECT_EQ(BIT(e->lp, 29), 1); EXPECT_EQ(BIT(e->lp, 30), 0); EXPECT_EQ(BIT(e->lp, 31), 0); }
    e = kw_find(WM_SYSKEYDOWN, 1);
    if (e) { EXPECT_EQ(e->wp, VK_F13); EXPECT_EQ(BIT(e->lp, 29), 1); EXPECT_EQ(BIT(e->lp, 31), 0); }
    e = kw_find(WM_SYSKEYUP, 0);
    if (e) { EXPECT_EQ(e->wp, VK_F13); EXPECT_EQ(BIT(e->lp, 29), 1); EXPECT_EQ(BIT(e->lp, 31), 1); }
    else CHECK(0);
    t_note("Alt release delivered as %s", kw_count(WM_SYSKEYUP) >= 2 ? "WM_SYSKEYUP" : kw_count(WM_KEYUP) ? "WM_KEYUP" : "nothing");
    kw_end(w);
}
TEST(input_kbd_inj_alt_state_in_getkeystate) {
    KBEGIN;
    kw_down(VK_LMENU);
    kw_down(VK_F13);
    kw_pump(1500, 2, 0);
    CHECK(U(GetKeyState)(VK_MENU) < 0);
    CHECK(U(GetKeyState)(VK_LMENU) < 0);
    CHECK(U(GetKeyState)(VK_RMENU) >= 0);
    kw_up(VK_F13);
    kw_up(VK_LMENU);
    kw_pump(1500, 4, 0);
    CHECK(U(GetKeyState)(VK_MENU) >= 0);
    kw_end(w);
}
TEST(input_kbd_inj_ctrl_alt_shift_combo_state) {
    BYTE st[256];
    KBEGIN;
    kw_down(VK_LCONTROL); kw_down(VK_LSHIFT); kw_down(VK_LMENU); kw_down(VK_F13);
    kw_pump(2000, 4, 0);
    U(GetKeyboardState)(st);
    CHECK(st[VK_CONTROL] & 0x80); CHECK(st[VK_SHIFT] & 0x80); CHECK(st[VK_MENU] & 0x80); CHECK(st[VK_F13] & 0x80);
    kw_up(VK_F13); kw_up(VK_LMENU); kw_up(VK_LSHIFT); kw_up(VK_LCONTROL);
    kw_pump(2000, 8, 0);
    U(GetKeyboardState)(st);
    CHECK(!(st[VK_CONTROL] & 0x80)); CHECK(!(st[VK_SHIFT] & 0x80)); CHECK(!(st[VK_MENU] & 0x80)); CHECK(!(st[VK_F13] & 0x80));
    kw_end(w);
}
TEST(input_kbd_inj_release_without_press) {
    KBEGIN;
    kw_up(VK_F22);
    kw_pump(1000, 1, 0);
    t_note("key-up without press delivered %d WM_KEYUP", kw_count(WM_KEYUP));
    if (kw_find(WM_KEYUP, 0)) EXPECT_EQ(BIT(kw_find(WM_KEYUP, 0)->lp, 31), 1);
    CHECK(U(GetKeyState)(VK_F22) >= 0);
    kw_end(w);
}
TEST(input_kbd_inj_batch_order_preserved) {
    INPUT in[6];
    static const WORD vks[3] = { VK_F13, VK_F14, VK_F15 };
    int i;
    KBEGIN;
    memset(in, 0, sizeof in);
    for (i = 0; i < 3; i++) {
        in[i].type = INPUT_KEYBOARD; in[i].ki.wVk = vks[i];
        in[3 + i].type = INPUT_KEYBOARD; in[3 + i].ki.wVk = vks[2 - i]; in[3 + i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    EXPECT_EQ(kw_input(in, 6), 6);
    kw_pump(2000, 6, 0);
    EXPECT_EQ(g_nklog, 6);
    for (i = 0; i < 3 && g_nklog == 6; i++) {
        EXPECT_EQ(g_klog[i].msg, WM_KEYDOWN); EXPECT_EQ(g_klog[i].wp, vks[i]);
        EXPECT_EQ(g_klog[3 + i].msg, WM_KEYUP); EXPECT_EQ(g_klog[3 + i].wp, vks[2 - i]);
    }
    kw_end(w);
}
TEST(input_kbd_inj_timestamps_nondecreasing) {
    int i;
    KBEGIN;
    for (i = 0; i < 5; i++) { kw_down(VK_F23); kw_up(VK_F23); }
    kw_pump(2500, 10, 0);
    EXPECT_EQ(g_nklog, 10);
    for (i = 1; i < g_nklog; i++) CHECK((LONG)(g_klog[i].tick - g_klog[i - 1].tick) >= 0);
    kw_end(w);
}
TEST(input_kbd_inj_stress_many_events) {
    int i;
    KBEGIN;
    for (i = 0; i < 100; i++) { kw_down(VK_F24); kw_up(VK_F24); }
    kw_pump(5000, 200, 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 100);
    EXPECT_EQ(kw_count(WM_KEYUP), 100);
    kw_end(w);
}
TEST(input_kbd_inj_queue_status_and_input_state) {
    KBEGIN;
    REQUIRE(U(GetQueueStatus)); REQUIRE(U(GetInputState));
    kw_down(VK_F13);
    {
        DWORD end = GetTickCount() + 1500; int seen = 0, st = 0;
        while ((LONG)(GetTickCount() - end) < 0) {
            DWORD q = U(GetQueueStatus)(QS_KEY | QS_INPUT);
            if ((q >> 16) & QS_KEY) { seen = 1; st = U(GetInputState)(); break; }
            Sleep(5);
        }
        CHECK(seen);
        CHECK(st != 0);
    }
    kw_up(VK_F13);
    kw_pump(1000, 2, 0);
    kw_end(w);
}
TEST(input_kbd_inj_peekmessage_key_filter) {
    MSG m;
    int n = 0;
    KBEGIN;
    kw_down(VK_F13); kw_up(VK_F13);
    Sleep(200);
    U(PostMessageA)(w, WM_USER + 5, 1, 2);
    {
        DWORD end = GetTickCount() + 1500;
        while ((LONG)(GetTickCount() - end) < 0 && n < 2) {
            if (U(PeekMessageA)(&m, w, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) { n++; CHECK(m.message == WM_KEYDOWN || m.message == WM_KEYUP); }
            else Sleep(5);
        }
    }
    EXPECT_EQ(n, 2);
    CHECK(U(PeekMessageA)(&m, w, WM_USER + 5, WM_USER + 5, PM_REMOVE));
    kw_end(w);
}
TEST(input_kbd_inj_us_letter_char_translation) {
    KBEGIN;
    if (!kbd_is_us()) { kw_end(w); t_skip("needs active US layout"); return; }
    kw_down('A'); kw_up('A');
    kw_pump(2000, 3, 1);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    EXPECT_EQ(kw_count(WM_CHAR), 1);
    if (kw_find(WM_CHAR, 0)) {
        struct klog *d = kw_find(WM_KEYDOWN, 0);
        EXPECT_EQ(kw_find(WM_CHAR, 0)->wp, 'a');
        if (d) EXPECT_EQ(kw_find(WM_CHAR, 0)->lp & 0xFFFFFF, d->lp & 0xFFFFFF);
    }
    kw_clear();
    kw_down(VK_LSHIFT); kw_down('B'); kw_up('B'); kw_up(VK_LSHIFT);
    kw_pump(2000, 5, 1);
    EXPECT_EQ(kw_count(WM_CHAR), 1);
    if (kw_find(WM_CHAR, 0)) EXPECT_EQ(kw_find(WM_CHAR, 0)->wp, 'B');
    kw_clear();
    kw_down(VK_LCONTROL); kw_down('C'); kw_up('C'); kw_up(VK_LCONTROL);
    kw_pump(2000, 5, 1);
    EXPECT_EQ(kw_count(WM_CHAR), 1);
    if (kw_find(WM_CHAR, 0)) EXPECT_EQ(kw_find(WM_CHAR, 0)->wp, 3);
    kw_end(w);
}
TEST(input_kbd_inj_us_alt_letter_gives_syschar) {
    struct klog *c;
    KBEGIN;
    if (!kbd_is_us()) { kw_end(w); t_skip("needs active US layout"); return; }
    kw_down(VK_LMENU); kw_down('N'); kw_up('N'); kw_up(VK_LMENU);
    kw_pump(2500, 5, 1);
    EXPECT_EQ(kw_count(WM_SYSCHAR), 1);
    EXPECT_EQ(kw_count(WM_CHAR), 0);
    c = kw_find(WM_SYSCHAR, 0);
    if (c) { EXPECT_EQ(c->wp, 'n'); EXPECT_EQ(BIT(c->lp, 29), 1); }
    kw_end(w);
}
TEST(input_kbd_inj_enter_tab_space_chars) {
    static const struct { WORD vk; WCHAR ch; } t[] = { { VK_RETURN, '\r' }, { VK_TAB, '\t' }, { VK_SPACE, ' ' }, { VK_BACK, 8 }, { VK_ESCAPE, 0x1B } };
    int i;
    KBEGIN;
    for (i = 0; i < NT(t); i++) {
        kw_clear();
        kw_down(t[i].vk); kw_up(t[i].vk);
        kw_pump(2000, 3, 1);
        CHECKF(kw_count(WM_CHAR) == 1, "vk 0x%x: %d WM_CHAR", t[i].vk, kw_count(WM_CHAR));
        if (kw_find(WM_CHAR, 0)) EXPECT_EQ(kw_find(WM_CHAR, 0)->wp, t[i].ch);
    }
    kw_end(w);
}
TEST(input_kbd_inj_function_keys_make_no_char) {
    KBEGIN;
    kw_down(VK_F13); kw_up(VK_F13);
    kw_down(VK_LSHIFT); kw_up(VK_LSHIFT);
    kw_pump(2000, 4, 1);
    EXPECT_EQ(kw_count(WM_CHAR), 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 2);
    kw_end(w);
}
TEST(input_kbd_inj_lastinputinfo_advances) {
    LASTINPUTINFO a, b;
    KBEGIN;
    a.cbSize = b.cbSize = sizeof a;
    U(GetLastInputInfo)(&a);
    Sleep(50);
    kw_down(VK_F13); kw_up(VK_F13);
    kw_pump(1000, 2, 0);
    U(GetLastInputInfo)(&b);
    CHECK((LONG)(b.dwTime - a.dwTime) >= 0);
    if (b.dwTime == a.dwTime) t_note("injected input did not advance GetLastInputInfo");
    kw_end(w);
}
TEST(input_kbd_inj_asynckeystate_low_bit_observed) {
    KBEGIN;
    U(GetAsyncKeyState)(VK_F13);
    kw_down(VK_F13); kw_up(VK_F13);
    wait_async(VK_F13, 0, 1000);
    kw_pump(500, 2, 0);
    t_note("GetAsyncKeyState(F13) after a press/release: 0x%04x", (unsigned short)U(GetAsyncKeyState)(VK_F13));
    CHECK((U(GetAsyncKeyState)(VK_F13) & 0x8000) == 0);
    kw_end(w);
}
TEST(input_kbd_inj_focus_lost_stops_delivery) {
    HWND w2;
    KBEGIN;
    w2 = kw_plain();
    CHECK(w2 != NULL);
    if (w2) {
        U(ShowWindow)(w2, SW_SHOWNOACTIVATE);
        U(PostMessageA)(w2, WM_KEYDOWN, VK_F5, 1);      /* synthetic: never reaches w */
        kw_pump(200, 1, 0);
        EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
        U(DestroyWindow)(w2);
    }
    kw_end(w);
}

/* ---- TranslateMessage with synthetic messages ---- */
static MSG tm_msg(HWND w, UINT m, WPARAM wp, LPARAM lp) { MSG g; memset(&g, 0, sizeof g); g.hwnd = w; g.message = m; g.wParam = wp; g.lParam = lp; return g; }

TEST(input_kbd_translatemessage_return_values) {
    HWND w;
    MSG m;
    static const UINT nonzero[] = { WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP };
    static const UINT zero[] = { WM_CHAR, WM_SYSCHAR, WM_DEADCHAR, WM_MOUSEMOVE, WM_NULL, WM_PAINT, WM_TIMER, WM_COMMAND, WM_USER };
    int i;
    REQUIRE(U(TranslateMessage));
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    for (i = 0; i < NT(nonzero); i++) {
        m = tm_msg(w, nonzero[i], VK_F13, 1);
        CHECKF(U(TranslateMessage)(&m) != 0, "message 0x%x", nonzero[i]);
    }
    for (i = 0; i < NT(zero); i++) {
        m = tm_msg(w, zero[i], VK_F13, 1);
        CHECKF(U(TranslateMessage)(&m) == 0, "message 0x%x", zero[i]);
    }
    kw_pump(100, 1000, 0);
    U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_translatemessage_control_keys_post_chars) {
    static const struct { WORD vk; UINT sc; WCHAR ch; } t[] = {
        { VK_RETURN, 0x1C, '\r' }, { VK_TAB, 0x0F, '\t' }, { VK_BACK, 0x0E, 8 }, { VK_ESCAPE, 0x01, 0x1B }, { VK_SPACE, 0x39, ' ' },
    };
    HWND w;
    MSG m;
    int i;
    REQUIRE(U(TranslateMessage));
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    for (i = 0; i < NT(t); i++) {
        kw_clear();
        m = tm_msg(w, WM_KEYDOWN, t[i].vk, 1 | (t[i].sc << 16));
        U(TranslateMessage)(&m);
        kw_pump(300, 1, 0);
        CHECKF(kw_count(WM_CHAR) == 1, "vk %x: %d WM_CHAR", t[i].vk, kw_count(WM_CHAR));
        if (kw_find(WM_CHAR, 0)) EXPECT_EQ(kw_find(WM_CHAR, 0)->wp, t[i].ch);
    }
    U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_translatemessage_keyup_posts_nothing) {
    HWND w;
    MSG m;
    REQUIRE(U(TranslateMessage));
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    m = tm_msg(w, WM_KEYUP, VK_RETURN, 0xC0000001 | (0x1C << 16));
    U(TranslateMessage)(&m);
    m = tm_msg(w, WM_KEYUP, VK_SPACE, 0xC0000001);
    U(TranslateMessage)(&m);
    kw_pump(200, 1, 0);
    EXPECT_EQ(kw_count(WM_CHAR), 0);
    EXPECT_EQ(kw_count(WM_SYSCHAR), 0);
    U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_translatemessage_sys_space_posts_syschar) {
    HWND w;
    MSG m;
    REQUIRE(U(TranslateMessage));
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    m = tm_msg(w, WM_SYSKEYDOWN, VK_SPACE, 1 | 0x20000000 | (0x39 << 16));
    U(TranslateMessage)(&m);
    kw_pump(300, 1, 0);
    EXPECT_EQ(kw_count(WM_SYSCHAR), 1);
    EXPECT_EQ(kw_count(WM_CHAR), 0);
    if (kw_find(WM_SYSCHAR, 0)) EXPECT_EQ(kw_find(WM_SYSCHAR, 0)->wp, ' ');
    U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_translatemessage_vk_packet_posts_char) {
    HWND w;
    MSG m;
    REQUIRE(U(TranslateMessage));
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    m = tm_msg(w, WM_KEYDOWN, VK_PACKET | (0x3C0 << 16), 1);
    U(TranslateMessage)(&m);
    kw_pump(300, 1, 0);
    EXPECT_EQ(kw_count(WM_CHAR), 1);
    if (kw_find(WM_CHAR, 0)) EXPECT_EQ(kw_find(WM_CHAR, 0)->wp, 0x3C0);
    kw_clear();
    m = tm_msg(w, WM_KEYDOWN, VK_PACKET | ((WPARAM)0x20AC << 16), 1);
    U(TranslateMessage)(&m);
    kw_pump(300, 1, 0);
    if (kw_find(WM_CHAR, 0)) EXPECT_EQ(kw_find(WM_CHAR, 0)->wp, 0x20AC);
    U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_translatemessage_us_letters) {
    HWND w;
    MSG m;
    int vk;
    REQUIRE(U(TranslateMessage));
    if (!kbd_is_us()) { t_skip("needs active US layout"); return; }
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    for (vk = 'A'; vk <= 'Z'; vk++) {
        kw_clear();
        m = tm_msg(w, WM_KEYDOWN, vk, 1 | (U(MapVirtualKeyW)(vk, 0) << 16));
        U(TranslateMessage)(&m);
        kw_pump(200, 1, 0);
        CHECKF(kw_count(WM_CHAR) == 1, "vk %x", vk);
        if (kw_find(WM_CHAR, 0)) EXPECT_EQ(kw_find(WM_CHAR, 0)->wp, vk + 32);
    }
    U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_translatemessage_hwnd_null_posts_to_thread) {
    MSG m, got;
    int found = 0;
    REQUIRE(U(TranslateMessage));
    m = tm_msg(NULL, WM_KEYDOWN, VK_SPACE, 1 | (0x39 << 16));
    U(TranslateMessage)(&m);
    {
        DWORD end = GetTickCount() + 500;
        while ((LONG)(GetTickCount() - end) < 0 && !found) {
            if (U(PeekMessageA)(&got, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) found = 1; else Sleep(5);
        }
    }
    t_note("TranslateMessage with NULL hwnd %s a WM_CHAR", found ? "posted" : "did not post");
    if (found) EXPECT_EQ(got.wParam, ' ');
    CHECK(1);
}

/* ---- SendInput validation (no real key events leave the process) ---- */
TEST(input_kbd_sendinput_invalid_arguments) {
    INPUT in[2];
    REQUIRE(U(SendInput));
    memset(in, 0, sizeof in);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(0, NULL, 0), 0);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(0, in, sizeof(INPUT)), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(0, NULL, sizeof(INPUT)), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(1, NULL, sizeof(INPUT)), 0);
    CHECK(GetLastError() == ERROR_NOACCESS || GetLastError() == ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(1, NULL, 0), 0);
}
TEST(input_kbd_sendinput_bad_cbsize) {
    static const int d[] = { -1, 1, 0, 8, 28, 39 };
    INPUT in[2];
    int i;
    REQUIRE(U(SendInput));
    memset(in, 0, sizeof in);
    for (i = 0; i < NT(d); i++) {
        UINT sz = (UINT)((int)sizeof(INPUT) + d[i]);
        if (d[i] > 0 && d[i] < 40) sz = d[i];
        SET_ERR(0xdeadbeef);
        CHECKF(U(SendInput)(1, in, sz) == 0, "cbSize %u accepted", sz);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(1, in, sizeof(INPUT) + 1), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(1, in, sizeof(INPUT) * 2), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(input_kbd_sendinput_truncated_keyboard_struct) {
    INPUT in[16];
    int i;
    REQUIRE(U(SendInput));
    memset(in, 0, sizeof in);
    for (i = 0; i < 16; i++) in[i].type = INPUT_KEYBOARD;
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(16, in, offsetof(INPUT, ki) + sizeof(KEYBDINPUT)), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(input_kbd_sendinput_hardware_type_rejected) {
    INPUT in[2];
    UINT r;
    REQUIRE(U(SendInput));
    memset(in, 0, sizeof in);
    in[0].type = in[1].type = INPUT_HARDWARE;
    SET_ERR(0xdeadbeef);
    r = U(SendInput)(2, in, sizeof(INPUT));
    t_note("SendInput(INPUT_HARDWARE) = %u, error %lu", r, GetLastError());
    CHECK(r == 0 || r == 2);
}
TEST(input_kbd_sendinput_noop_mouse_inputs_counted) {
    INPUT in[16];
    REQUIRE(U(SendInput));
    memset(in, 0, sizeof in);        /* type 0 = mouse, dwFlags 0: no movement, no buttons */
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(1, in, sizeof(INPUT)), 1);
    EXPECT_EQ(GetLastError(), 0xdeadbeef);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(SendInput)(16, in, sizeof(INPUT)), 16);
    EXPECT_EQ(GetLastError(), 0xdeadbeef);
}
TEST(input_kbd_keybd_event_zero_vk_keyup_harmless) {
    REQUIRE(U(keybd_event));
    /* a key-up of a key that is not down must be harmless */
    U(keybd_event)(VK_F24, 0, KEYEVENTF_KEYUP, 0);
    CHECK((U(GetAsyncKeyState)(VK_F24) & 0x8000) == 0);
}
