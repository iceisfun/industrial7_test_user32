/* Additional checks grounded in the user32 client-side code paths. */
#include "kbd.h"
#include <ctype.h>

#define NT(a) ((int)(sizeof(a) / sizeof((a)[0])))
static ACCEL mka(BYTE f, WORD key, WORD cmd) { ACCEL a; memset(&a, 0, sizeof a); a.fVirt = f; a.key = key; a.cmd = cmd; return a; }
static MSG mkm(HWND w, UINT m, WPARAM wp, LPARAM lp) { MSG g; memset(&g, 0, sizeof g); g.hwnd = w; g.message = m; g.wParam = wp; g.lParam = lp; g.time = GetTickCount(); return g; }

/* ---- TranslateMessage: client-side filtering ---- */
TEST(input_kbd_x_translatemessage_high_bits_invalid_param) {
    static const UINT msgs[] = { 0x20000, 0x40000, 0x80000000, 0xFFFE0000, 0xFFFFFFFF, 0x7FFE0000 };
    int i;
    MSG m;
    REQUIRE(U(TranslateMessage));
    for (i = 0; i < NT(msgs); i++) {
        m = mkm(NULL, msgs[i], 'A', 1);
        SET_ERR(0x1234);
        CHECKF(U(TranslateMessage)(&m) == 0, "message %08x translated", msgs[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "message %08x: last error %lu", msgs[i], GetLastError());
    }
}
TEST(input_kbd_x_translatemessage_char_messages_leave_error) {
    static const UINT msgs[] = { WM_CHAR, WM_DEADCHAR, WM_SYSCHAR, WM_SYSDEADCHAR, WM_MOUSEMOVE, WM_USER, WM_NULL, WM_TIMER };
    int i;
    MSG m;
    REQUIRE(U(TranslateMessage));
    for (i = 0; i < NT(msgs); i++) {
        m = mkm(NULL, msgs[i], 'A', 1);
        SET_ERR(0x1234);
        CHECKF(U(TranslateMessage)(&m) == 0, "message %x translated", msgs[i]);
        CHECKF(GetLastError() == 0x1234, "message %x changed last error to %lu", msgs[i], GetLastError());
    }
}
TEST(input_kbd_x_translatemessage_does_not_modify_msg) {
    HWND w;
    MSG m, c;
    REQUIRE(U(TranslateMessage));
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    m = mkm(w, WM_KEYDOWN, VK_F5, 0x003F0001);
    c = m;
    U(TranslateMessage)(&m);
    CHECK(!memcmp(&m, &c, sizeof m));
    m = mkm(w, WM_KEYUP, VK_F5, 0xC03F0001);
    c = m;
    U(TranslateMessage)(&m);
    CHECK(!memcmp(&m, &c, sizeof m));
    kw_pump(100, 1000, 0);
    U(DestroyWindow)(w);
    kw_clear();
}

/* ---- ToUnicode / ToAscii zero results ---- */
TEST(input_kbd_x_tounicode_zero_result_clears_first_char) {
    static const UINT vks[] = { VK_F1, VK_LEFT, VK_SHIFT, VK_CAPITAL, VK_LWIN, VK_F24, VK_NUMLOCK, VK_DELETE };
    int i;
    REQUIRE(U(ToUnicode)); REQUIRE(U(ToUnicodeEx)); REQUIRE(U(GetKeyboardLayout));
    for (i = 0; i < NT(vks); i++) {
        BYTE st[256] = {0};
        WCHAR w[4] = { 0x5555, 0x5555, 0x5555, 0x5555 };
        int r = U(ToUnicode)(vks[i], 0, st, w, 4, 0);
        if (r == 0) { CHECKF(w[0] == 0, "ToUnicode vk %x: buf[0]=%04x", vks[i], w[0]); }
        w[0] = w[1] = 0x5555;
        r = U(ToUnicodeEx)(vks[i], 0, st, w, 4, 0, U(GetKeyboardLayout)(0));
        if (r == 0) { CHECKF(w[0] == 0, "ToUnicodeEx vk %x: buf[0]=%04x", vks[i], w[0]); }
    }
}
TEST(input_kbd_x_toascii_zero_result_leaves_output) {
    static const UINT vks[] = { VK_F1, VK_LEFT, VK_SHIFT, VK_F24 };
    int i;
    REQUIRE(U(ToAscii)); REQUIRE(U(ToAsciiEx)); REQUIRE(U(GetKeyboardLayout));
    for (i = 0; i < NT(vks); i++) {
        BYTE st[256] = {0};
        WORD c = 0x5A5A;
        int r = U(ToAscii)(vks[i], 0, st, &c, 0);
        if (r == 0) EXPECT_EQ(c, 0x5A5A);
        c = 0x5A5A;
        r = U(ToAsciiEx)(vks[i], 0, st, &c, 0, U(GetKeyboardLayout)(0));
        if (r == 0) EXPECT_EQ(c, 0x5A5A);
    }
}
TEST(input_kbd_x_toascii_result_byte_only) {
    BYTE st[256] = {0};
    WORD c;
    REQUIRE(U(ToAscii)); REQUIRE(U(GetKeyboardLayout));
    if (!kbd_is_english()) { t_skip("non-English layout"); return; }
    c = 0xFFFF;
    EXPECT_EQ(U(ToAscii)(VK_SPACE, 0x39, st, &c, 0), 1);
    EXPECT_EQ(c & 0xFF, ' ');
    c = 0xFFFF;
    EXPECT_EQ(U(ToAscii)(VK_RETURN, 0x1C, st, &c, 0), 1);
    EXPECT_EQ(c & 0xFF, '\r');
}
TEST(input_kbd_x_tounicode_leaves_keystate_untouched_us) {
    HKL us = kbd_us_hkl();
    int vk;
    REQUIRE(U(ToUnicodeEx)); REQUIRE(U(MapVirtualKeyExW));
    if (!us) { kbd_us_release(); t_skip("US layout unavailable"); return; }
    for (vk = 8; vk < 0xFF; vk++) {
        BYTE st[256], copy[256];
        WCHAR w[4];
        memset(st, 0, sizeof st);
        st[VK_SHIFT] = 0x80; st[VK_CAPITAL] = 1; st[VK_NUMLOCK] = 1;
        memcpy(copy, st, sizeof st);
        U(ToUnicodeEx)(vk, U(MapVirtualKeyExW)(vk, 0, us), st, w, 4, 0, us);
        if (memcmp(st, copy, sizeof st)) { CHECKF(0, "vk %x changed the state array", vk); break; }
    }
    CHECK(1);
    kbd_us_release();
}

/* ---- MapVirtualKey invariants ---- */
TEST(input_kbd_x_mapvk_vsc_ex_prefix_bytes_valid) {
    UINT vk;
    int bad = 0;
    REQUIRE(U(MapVirtualKeyW));
    for (vk = 0; vk < 256; vk++) {
        UINT r = U(MapVirtualKeyW)(vk, MAPVK_VK_TO_VSC_EX);
        UINT hi = r >> 8;
        if (r > 0xFFFF || (hi != 0 && hi != 0xE0 && hi != 0xE1)) bad++;
    }
    CHECK(bad == 0);
}
TEST(input_kbd_x_mapvk_plain_vs_ex_vk_relationship) {
    UINT sc;
    int bad = 0;
    REQUIRE(U(MapVirtualKeyW));
    for (sc = 1; sc < 0x80; sc++) {
        UINT p = U(MapVirtualKeyW)(sc, MAPVK_VSC_TO_VK), e = U(MapVirtualKeyW)(sc, MAPVK_VSC_TO_VK_EX);
        UINT g = e;
        if (e == VK_LSHIFT || e == VK_RSHIFT) g = VK_SHIFT;
        if (e == VK_LCONTROL || e == VK_RCONTROL) g = VK_CONTROL;
        if (e == VK_LMENU || e == VK_RMENU) g = VK_MENU;
        if (p != e && p != g) { bad++; t_note("scan %02x: plain %x ex %x", sc, p, e); }
    }
    CHECK(bad == 0);
}
TEST(input_kbd_x_mapvk_vk_to_vsc_ex_low_byte_matches_plain) {
    UINT vk;
    int bad = 0;
    REQUIRE(U(MapVirtualKeyW));
    for (vk = 1; vk < 256; vk++) {
        UINT p = U(MapVirtualKeyW)(vk, MAPVK_VK_TO_VSC), e = U(MapVirtualKeyW)(vk, MAPVK_VK_TO_VSC_EX);
        int simple = (vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z') || (vk >= VK_F1 && vk <= VK_F12);
        if (p != 0 && (e & 0xFF) != p) { if (simple) bad++; t_note("vk %02x: plain %x ex %x", vk, p, e); }
    }
    CHECK(bad == 0);
}
TEST(input_kbd_x_mapvk_to_char_matches_vkkeyscan_unshifted_english) {
    int c, tested = 0;
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(VkKeyScanW));
    if (!kbd_is_english()) { t_skip("non-English layout"); return; }
    for (c = 'a'; c <= 'z'; c++) {
        SHORT r = U(VkKeyScanW)(c);
        UINT ch;
        if (r == -1 || (r & 0xFF00)) continue;
        ch = U(MapVirtualKeyW)(r & 0xFF, MAPVK_VK_TO_CHAR);
        CHECKF((ch & 0xFFFF) == (UINT)toupper(c), "char %c: vk %x -> %x", c, r & 0xFF, ch);
        tested++;
    }
    CHECK(tested > 0);
}

/* ---- accelerator semantics that do not need key state ---- */
static HWND xw(void)
{
    HWND w = kw_plain();
    if (w) U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    kw_clear();
    return w;
}
#define XAREQ REQUIRE(U(CreateAcceleratorTableW)); REQUIRE(U(DestroyAcceleratorTable)); REQUIRE(U(TranslateAcceleratorW)); REQUIRE(U(ShowWindow))

TEST(input_kbd_x_ta_ascii_entry_ignores_keydown) {
    ACCEL in AL8 = mka(0, 'x', 301);
    HACCEL h;
    HWND w;
    MSG m;
    XAREQ;
    w = xw(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        m = mkm(w, WM_KEYDOWN, 'X', 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        m = mkm(w, WM_KEYDOWN, 'x', 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    if (w) U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_x_ta_virtkey_entry_ignores_wm_char) {
    ACCEL in AL8 = mka(FVIRTKEY, 'A', 302);
    HACCEL h;
    HWND w;
    MSG m;
    XAREQ;
    w = xw(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        m = mkm(w, WM_CHAR, 'A', 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        m = mkm(w, WM_CHAR, 'a', 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        m = mkm(w, WM_SYSCHAR, 'a', 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    if (w) U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_x_ta_null_window_even_with_match) {
    ACCEL in AL8 = mka(FVIRTKEY, VK_F9, 303);
    HACCEL h;
    MSG m;
    XAREQ;
    h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(h != NULL);
    if (h) {
        UINT i;
        static const UINT msgs[] = { WM_KEYDOWN, WM_SYSKEYDOWN, WM_CHAR, WM_SYSCHAR };
        for (i = 0; i < 4; i++) {
            m = mkm(NULL, msgs[i], VK_F9, 1);
            EXPECT_EQ(U(TranslateAcceleratorW)(NULL, h, &m), 0);
        }
        U(DestroyAcceleratorTable)(h);
    }
}
TEST(input_kbd_x_ta_mouse_and_paint_messages_never_match) {
    ACCEL in AL8 = mka(0, 0, 304);
    HACCEL h;
    HWND w;
    MSG m;
    int i;
    static const UINT msgs[] = { WM_PAINT, WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_KEYUP, WM_SYSKEYUP, WM_COMMAND, WM_CLOSE, WM_UNICHAR, WM_IME_CHAR };
    XAREQ;
    w = xw(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        for (i = 0; i < NT(msgs); i++) {
            m = mkm(w, msgs[i], 0, 1);
            CHECKF(U(TranslateAcceleratorW)(w, h, &m) == 0, "message %x", msgs[i]);
        }
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    if (w) U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_x_accel_copy_after_destroy_of_other_table) {
    ACCEL a AL8 = mka(FVIRTKEY, VK_F7, 11), b AL8 = mka(FVIRTKEY | FCONTROL, 'Q', 12), out[2] AL8;
    HACCEL ha, hb;
    XAREQ; REQUIRE(U(CopyAcceleratorTableW));
    ha = U(CreateAcceleratorTableW)(&a, 1);
    hb = U(CreateAcceleratorTableW)(&b, 1);
    CHECK(ha && hb);
    if (ha && hb) {
        CHECK(U(DestroyAcceleratorTable)(ha));
        memset(out, 0, sizeof out);
        EXPECT_EQ(U(CopyAcceleratorTableW)(hb, out, 2), 1);
        EXPECT_EQ(out[0].key, 'Q');
        EXPECT_EQ(out[0].cmd, 12);
        EXPECT_EQ(out[0].fVirt & 0x1F, FVIRTKEY | FCONTROL);
        CHECK(U(DestroyAcceleratorTable)(hb));
    } else {
        if (ha) U(DestroyAcceleratorTable)(ha);
        if (hb) U(DestroyAcceleratorTable)(hb);
    }
}
TEST(input_kbd_x_accel_command_ids_full_range) {
    static const WORD cmds[] = { 0, 1, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF };
    ACCEL in[6] AL8, out[6] AL8;
    HACCEL h;
    int i;
    XAREQ; REQUIRE(U(CopyAcceleratorTableW));
    for (i = 0; i < NT(cmds); i++) in[i] = mka(FVIRTKEY, VK_F1 + i, cmds[i]);
    h = U(CreateAcceleratorTableW)(in, 6);
    CHECK(h != NULL);
    if (!h) return;
    memset(out, 0, sizeof out);
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, out, 6), 6);
    for (i = 0; i < 6; i++) EXPECT_EQ(out[i].cmd, cmds[i]);
    U(DestroyAcceleratorTable)(h);
}
TEST(input_kbd_x_accel_load_missing_error_observed) {
    HACCEL h;
    REQUIRE(U(LoadAcceleratorsW));
    SET_ERR(0);
    h = U(LoadAcceleratorsW)(GetModuleHandleA(NULL), L"MISSING_ACCEL_TABLE");
    CHECK(h == NULL);
    t_note("LoadAcceleratorsW(missing) -> error %lu (expected one of 1812/1813/1814)", GetLastError());
}

/* ---- key name / layout ---- */
TEST(input_kbd_x_keyname_letters_english) {
    UINT vk;
    REQUIRE(U(GetKeyNameTextA)); REQUIRE(U(MapVirtualKeyW));
    if (!kbd_is_english()) { t_skip("non-English layout"); return; }
    for (vk = 'A'; vk <= 'Z'; vk++) {
        char a[16];
        UINT sc = U(MapVirtualKeyW)(vk, MAPVK_VK_TO_VSC);
        int r = U(GetKeyNameTextA)(sc << 16, a, sizeof a);
        CHECKF(r == 1, "vk %x scan %x: len %d", vk, sc, r);
        if (r == 1) CHECKF(a[0] == (char)vk, "vk %x name \"%s\"", vk, a);
    }
}
TEST(input_kbd_x_keyname_function_keys_observed) {
    int i;
    REQUIRE(U(GetKeyNameTextA)); REQUIRE(U(MapVirtualKeyW));
    for (i = 13; i <= 24; i++) {
        char a[32] = "";
        UINT sc = U(MapVirtualKeyW)(VK_F1 + i - 1, MAPVK_VK_TO_VSC);
        int r = U(GetKeyNameTextA)(sc << 16, a, sizeof a);
        t_note("F%d scan %02x name \"%s\" (%d)", i, sc, a, r);
    }
    CHECK(1);
}
TEST(input_kbd_x_layout_hkl_low_word_is_language) {
    HKL buf[64];
    int n, i;
    REQUIRE(U(GetKeyboardLayoutList));
    n = U(GetKeyboardLayoutList)(64, buf);
    CHECK(n >= 1);
    for (i = 0; i < n; i++) CHECKF(LOWORD((ULONG_PTR)buf[i]) != 0, "entry %d = %p", i, buf[i]);
}
TEST(input_kbd_x_layoutlist_big_buffer_count_equals_query) {
    HKL buf[64];
    int n, r;
    REQUIRE(U(GetKeyboardLayoutList));
    n = U(GetKeyboardLayoutList)(0, NULL);
    r = U(GetKeyboardLayoutList)(64, buf);
    EXPECT_EQ(r, n);
}
TEST(input_kbd_x_kbcodepage_valid_codepage) {
    UINT cp;
    REQUIRE(U(GetKBCodePage));
    cp = U(GetKBCodePage)();
    CHECK(IsValidCodePage(cp));
}
TEST(input_kbd_x_lastinputinfo_second_field_untouched) {
    struct { LASTINPUTINFO li; DWORD guard; } s;
    REQUIRE(U(GetLastInputInfo));
    s.guard = 0xA5A5A5A5;
    s.li.cbSize = sizeof(LASTINPUTINFO);
    CHECK(U(GetLastInputInfo)(&s.li));
    EXPECT_EQ(s.guard, 0xA5A5A5A5);
    EXPECT_EQ(s.li.cbSize, sizeof(LASTINPUTINFO));
}
