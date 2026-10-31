/* MapVirtualKey*, VkKeyScan*, OemKeyScan. */
#include "kbd.h"

struct mv { UINT in, type, want; };

/* Check W, A, ExW and ExA (explicit HKL) agree on the expected value. */
static void run_mv(HKL hkl, const struct mv *t, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        UINT w = U(MapVirtualKeyExW)(t[i].in, t[i].type, hkl);
        UINT a = U(MapVirtualKeyExA)(t[i].in, t[i].type, hkl);
        CHECKF(w == t[i].want, "MapVirtualKeyExW(0x%x, %u) = 0x%x want 0x%x", t[i].in, t[i].type, w, t[i].want);
        CHECKF(a == t[i].want, "MapVirtualKeyExA(0x%x, %u) = 0x%x want 0x%x", t[i].in, t[i].type, a, t[i].want);
    }
}
#define NT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* --- layout independent hardware keys (scan codes are fixed by the keyboard) --- */
TEST(input_kbd_mapvk_vk_to_vsc_editing) {
    static const struct mv t[] = {
        { VK_ESCAPE, 0, 0x01 }, { VK_BACK, 0, 0x0E }, { VK_TAB, 0, 0x0F }, { VK_RETURN, 0, 0x1C }, { VK_SPACE, 0, 0x39 },
        { VK_CAPITAL, 0, 0x3A },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_vk_to_vsc_modifiers) {
    static const struct mv t[] = {
        { VK_LSHIFT, 0, 0x2A }, { VK_RSHIFT, 0, 0x36 }, { VK_LCONTROL, 0, 0x1D }, { VK_LMENU, 0, 0x38 },
        { VK_SHIFT, 0, 0x2A }, { VK_CONTROL, 0, 0x1D }, { VK_MENU, 0, 0x38 },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_vk_to_vsc_right_variants_have_no_prefix) {
    UINT s;
    REQUIRE(U(MapVirtualKeyW));
    s = U(MapVirtualKeyW)(VK_RCONTROL, MAPVK_VK_TO_VSC);
    EXPECT_EQ(s >> 8, 0);
    s = U(MapVirtualKeyW)(VK_RMENU, MAPVK_VK_TO_VSC);
    EXPECT_EQ(s >> 8, 0);
    s = U(MapVirtualKeyW)(VK_RSHIFT, MAPVK_VK_TO_VSC);
    EXPECT_EQ(s, 0x36);
}
TEST(input_kbd_mapvk_vk_to_vsc_ex_prefix) {
    REQUIRE(U(MapVirtualKeyW));
    EXPECT_EQ(U(MapVirtualKeyW)(VK_RCONTROL, MAPVK_VK_TO_VSC_EX) >> 8, 0xE0);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_RMENU, MAPVK_VK_TO_VSC_EX) >> 8, 0xE0);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_RSHIFT, MAPVK_VK_TO_VSC_EX), 0x36);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_LCONTROL, MAPVK_VK_TO_VSC_EX), 0x1D);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_LSHIFT, MAPVK_VK_TO_VSC_EX), 0x2A);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_LMENU, MAPVK_VK_TO_VSC_EX), 0x38);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_MULTIPLY, MAPVK_VK_TO_VSC_EX), 0x37);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_NUMLOCK, MAPVK_VK_TO_VSC_EX), 0x45);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_SCROLL, MAPVK_VK_TO_VSC_EX), 0x46);
}
TEST(input_kbd_mapvk_vk_to_vsc_function_keys) {
    static const struct mv t[] = {
        { VK_F1, 0, 0x3B }, { VK_F2, 0, 0x3C }, { VK_F3, 0, 0x3D }, { VK_F4, 0, 0x3E }, { VK_F5, 0, 0x3F },
        { VK_F6, 0, 0x40 }, { VK_F7, 0, 0x41 }, { VK_F8, 0, 0x42 }, { VK_F9, 0, 0x43 }, { VK_F10, 0, 0x44 },
        { VK_F11, 0, 0x57 }, { VK_F12, 0, 0x58 }, { VK_NUMLOCK, 0, 0x45 }, { VK_SCROLL, 0, 0x46 },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_vk_to_vsc_numpad) {
    static const struct mv t[] = {
        { VK_NUMPAD7, 0, 0x47 }, { VK_NUMPAD8, 0, 0x48 }, { VK_NUMPAD9, 0, 0x49 }, { VK_SUBTRACT, 0, 0x4A },
        { VK_NUMPAD4, 0, 0x4B }, { VK_NUMPAD5, 0, 0x4C }, { VK_NUMPAD6, 0, 0x4D }, { VK_ADD, 0, 0x4E },
        { VK_NUMPAD1, 0, 0x4F }, { VK_NUMPAD2, 0, 0x50 }, { VK_NUMPAD3, 0, 0x51 }, { VK_NUMPAD0, 0, 0x52 },
        { VK_DECIMAL, 0, 0x53 }, { VK_MULTIPLY, 0, 0x37 },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_vk_to_vsc_digit_row) {
    UINT vk;
    REQUIRE(U(MapVirtualKeyW));
    for (vk = '1'; vk <= '9'; vk++) EXPECT_EQ(U(MapVirtualKeyW)(vk, MAPVK_VK_TO_VSC), vk - '1' + 2);
    EXPECT_EQ(U(MapVirtualKeyW)('0', MAPVK_VK_TO_VSC), 0x0B);
}
TEST(input_kbd_mapvk_vsc_to_vk_basic) {
    static const struct mv t[] = {
        { 0x01, 1, VK_ESCAPE }, { 0x0E, 1, VK_BACK }, { 0x0F, 1, VK_TAB }, { 0x1C, 1, VK_RETURN }, { 0x39, 1, VK_SPACE },
        { 0x3A, 1, VK_CAPITAL }, { 0x45, 1, VK_NUMLOCK }, { 0x46, 1, VK_SCROLL },
        { 0x3B, 1, VK_F1 }, { 0x3C, 1, VK_F2 }, { 0x3D, 1, VK_F3 }, { 0x3E, 1, VK_F4 }, { 0x3F, 1, VK_F5 }, { 0x40, 1, VK_F6 },
        { 0x41, 1, VK_F7 }, { 0x42, 1, VK_F8 }, { 0x43, 1, VK_F9 }, { 0x44, 1, VK_F10 }, { 0x57, 1, VK_F11 }, { 0x58, 1, VK_F12 },
        { 0x37, 1, VK_MULTIPLY }, { 0x4A, 1, VK_SUBTRACT }, { 0x4E, 1, VK_ADD },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_vsc_to_vk_generic_modifiers) {
    static const struct mv t[] = {
        { 0x2A, 1, VK_SHIFT }, { 0x36, 1, VK_SHIFT }, { 0x1D, 1, VK_CONTROL }, { 0x38, 1, VK_MENU },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_vsc_to_vk_ex_sided_modifiers) {
    static const struct mv t[] = {
        { 0x2A, 3, VK_LSHIFT }, { 0x36, 3, VK_RSHIFT }, { 0x1D, 3, VK_LCONTROL }, { 0x38, 3, VK_LMENU },
        { 0x01, 3, VK_ESCAPE }, { 0x1C, 3, VK_RETURN }, { 0x39, 3, VK_SPACE }, { 0x3B, 3, VK_F1 },
        { 0x45, 3, VK_NUMLOCK }, { 0x46, 3, VK_SCROLL }, { 0x37, 3, VK_MULTIPLY },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_vsc_to_vk_ex_prefixed) {
    static const struct mv t[] = {
        { 0xE037, 3, VK_SNAPSHOT }, { 0xE01D, 3, VK_RCONTROL }, { 0xE11D, 3, VK_PAUSE }, { 0xE046, 3, VK_CANCEL },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_vsc_to_vk_ex_invalid_scans) {
    static const struct mv t[] = {
        { 0x136, 3, 0 }, { 0xE036, 3, 0 }, { 0xE136, 3, 0 }, { 0x137, 3, 0 }, { 0x237, 3, 0 }, { 0xE137, 3, 0 },
        { 0x145, 3, 0 }, { 0xE045, 3, 0 }, { 0xE145, 3, 0 }, { 0x011D, 3, 0 }, { 0x021D, 3, 0 },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(MapVirtualKeyExA));
    run_mv(U(GetKeyboardLayout)(0), t, NT(t));
}
TEST(input_kbd_mapvk_zero_inputs) {
    UINT ty;
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(MapVirtualKeyA));
    for (ty = 0; ty <= 4; ty++) {
        EXPECT_EQ(U(MapVirtualKeyW)(0, ty), 0);
        EXPECT_EQ(U(MapVirtualKeyA)(0, ty), 0);
    }
}
TEST(input_kbd_mapvk_invalid_map_types) {
    static const UINT ty[] = { 5, 6, 7, 8, 0x10, 0x100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    int i;
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(MapVirtualKeyA)); REQUIRE(U(MapVirtualKeyExW));
    for (i = 0; i < NT(ty); i++) {
        CHECKF(U(MapVirtualKeyW)(VK_RETURN, ty[i]) == 0, "type 0x%x", ty[i]);
        CHECKF(U(MapVirtualKeyA)(0x1C, ty[i]) == 0, "type 0x%x", ty[i]);
        CHECKF(U(MapVirtualKeyExW)(VK_SPACE, ty[i], U(GetKeyboardLayout)(0)) == 0, "type 0x%x", ty[i]);
    }
}
TEST(input_kbd_mapvk_function_keys_have_no_char) {
    UINT vk;
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(MapVirtualKeyA));
    for (vk = VK_F1; vk <= VK_F24; vk++) {
        EXPECT_EQ(U(MapVirtualKeyW)(vk, MAPVK_VK_TO_CHAR), 0);
        EXPECT_EQ(U(MapVirtualKeyA)(vk, MAPVK_VK_TO_CHAR), 0);
    }
    EXPECT_EQ(U(MapVirtualKeyW)(VK_SHIFT, MAPVK_VK_TO_CHAR), 0);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_CONTROL, MAPVK_VK_TO_CHAR), 0);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_MENU, MAPVK_VK_TO_CHAR), 0);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_LEFT, MAPVK_VK_TO_CHAR), 0);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_UP, MAPVK_VK_TO_CHAR), 0);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_CAPITAL, MAPVK_VK_TO_CHAR), 0);
    EXPECT_EQ(U(MapVirtualKeyW)(VK_LWIN, MAPVK_VK_TO_CHAR), 0);
}
TEST(input_kbd_mapvk_to_char_space_and_numpad_ops) {
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(MapVirtualKeyA));
    EXPECT_EQ(U(MapVirtualKeyW)(VK_SPACE, MAPVK_VK_TO_CHAR), ' ');
    EXPECT_EQ(U(MapVirtualKeyA)(VK_SPACE, MAPVK_VK_TO_CHAR), ' ');
    EXPECT_EQ(U(MapVirtualKeyW)(VK_ADD, MAPVK_VK_TO_CHAR), '+');
    EXPECT_EQ(U(MapVirtualKeyW)(VK_SUBTRACT, MAPVK_VK_TO_CHAR), '-');
    EXPECT_EQ(U(MapVirtualKeyW)(VK_MULTIPLY, MAPVK_VK_TO_CHAR), '*');
    EXPECT_EQ(U(MapVirtualKeyW)(VK_DIVIDE, MAPVK_VK_TO_CHAR), '/');
}
TEST(input_kbd_mapvk_to_char_control_keys_observed) {
    REQUIRE(U(MapVirtualKeyW));
    t_note("MAPVK_VK_TO_CHAR RETURN=%x TAB=%x BACK=%x ESC=%x NUMPAD0=%x DECIMAL=%x",
           U(MapVirtualKeyW)(VK_RETURN, 2), U(MapVirtualKeyW)(VK_TAB, 2), U(MapVirtualKeyW)(VK_BACK, 2),
           U(MapVirtualKeyW)(VK_ESCAPE, 2), U(MapVirtualKeyW)(VK_NUMPAD0, 2), U(MapVirtualKeyW)(VK_DECIMAL, 2));
    CHECK(1);
}
TEST(input_kbd_mapvk_to_char_letters_english) {
    UINT vk;
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(MapVirtualKeyA));
    if (!kbd_is_english()) { t_skip("non-English layout"); return; }
    for (vk = 'A'; vk <= 'Z'; vk++) {
        EXPECT_EQ(U(MapVirtualKeyW)(vk, MAPVK_VK_TO_CHAR), vk);
        EXPECT_EQ(U(MapVirtualKeyA)(vk, MAPVK_VK_TO_CHAR), vk);
    }
}
TEST(input_kbd_mapvk_a_w_parity_all_vk_all_types) {
    UINT vk, ty;
    int bad = 0;
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(MapVirtualKeyA));
    for (ty = 0; ty <= 4; ty++)
        for (vk = 0; vk < 256; vk++) {
            UINT w = U(MapVirtualKeyW)(vk, ty), a = U(MapVirtualKeyA)(vk, ty);
            if (ty == 2) { if ((w & 0x7FFFFFFF) < 0x80 ? a != w : 0) bad++; }
            else if (a != w) bad++;
        }
    CHECKF(bad == 0, "%d A/W differences", bad);
}
TEST(input_kbd_mapvk_ex_current_layout_matches_plain) {
    UINT vk, ty;
    int bad = 0;
    HKL h;
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(GetKeyboardLayout));
    h = U(GetKeyboardLayout)(0);
    for (ty = 0; ty <= 4; ty++)
        for (vk = 0; vk < 256; vk++)
            if (U(MapVirtualKeyW)(vk, ty) != U(MapVirtualKeyExW)(vk, ty, h)) bad++;
    CHECKF(bad == 0, "%d plain/Ex differences", bad);
}
TEST(input_kbd_mapvk_ex_null_hkl_observed) {
    UINT vk;
    int diff = 0;
    REQUIRE(U(MapVirtualKeyW)); REQUIRE(U(MapVirtualKeyExW));
    for (vk = 0; vk < 256; vk++) if (U(MapVirtualKeyW)(vk, 0) != U(MapVirtualKeyExW)(vk, 0, NULL)) diff++;
    if (diff) t_note("MapVirtualKeyExW with NULL hkl differs from current layout for %d keys", diff);
    CHECK(1);
}
TEST(input_kbd_mapvk_ex_bogus_hkl_observed) {
    REQUIRE(U(MapVirtualKeyExW));
    SET_ERR(0);
    UINT r = U(MapVirtualKeyExW)(VK_RETURN, 0, (HKL)(ULONG_PTR)0xDEADBEEF);
    t_note("MapVirtualKeyExW(RETURN, bogus hkl) = 0x%x, err %lu", r, GetLastError());
    CHECK(1);
}
TEST(input_kbd_mapvk_oversized_inputs_observed) {
    REQUIRE(U(MapVirtualKeyW));
    t_note("VK_TO_VSC(0x10D)=%x (0x100|RETURN) VK_TO_VSC(0xFFFFFFFF)=%x VSC_TO_VK(0x101C)=%x",
           U(MapVirtualKeyW)(0x10D, 0), U(MapVirtualKeyW)(0xFFFFFFFF, 0), U(MapVirtualKeyW)(0x101C, 1));
    CHECK(1);
}
TEST(input_kbd_mapvk_round_trip_letters_digits) {
    UINT vk;
    REQUIRE(U(MapVirtualKeyW));
    for (vk = '0'; vk <= '9'; vk++) {
        UINT sc = U(MapVirtualKeyW)(vk, 0);
        CHECKF(sc != 0, "vk %x", vk);
        EXPECT_EQ(U(MapVirtualKeyW)(sc, 1), vk);
    }
    for (vk = 'A'; vk <= 'Z'; vk++) {
        UINT sc = U(MapVirtualKeyW)(vk, 0);
        CHECKF(sc != 0, "vk %x", vk);
        EXPECT_EQ(U(MapVirtualKeyW)(sc, 1), vk);
        EXPECT_EQ(U(MapVirtualKeyW)(sc, 3), vk);
    }
}
TEST(input_kbd_mapvk_letter_scancodes_unique) {
    UINT vk, seen[256];
    int i, n = 0;
    REQUIRE(U(MapVirtualKeyW));
    for (vk = 'A'; vk <= 'Z'; vk++) {
        UINT sc = U(MapVirtualKeyW)(vk, 0);
        for (i = 0; i < n; i++) CHECKF(seen[i] != sc, "duplicate scan %x", sc);
        seen[n++] = sc;
        CHECK(sc >= 0x10 && sc <= 0x32);
    }
}

/* --- explicit US layout tables --- */
#define US_BEGIN HKL us = kbd_us_hkl(); if (!us) { kbd_us_release(); t_skip("US layout unavailable"); return; }
#define US_END kbd_us_release();

TEST(input_kbd_mapvk_us_letter_scancodes) {
    static const struct { char c; UINT sc; } t[] = {
        {'Q',0x10},{'W',0x11},{'E',0x12},{'R',0x13},{'T',0x14},{'Y',0x15},{'U',0x16},{'I',0x17},{'O',0x18},{'P',0x19},
        {'A',0x1E},{'S',0x1F},{'D',0x20},{'F',0x21},{'G',0x22},{'H',0x23},{'J',0x24},{'K',0x25},{'L',0x26},
        {'Z',0x2C},{'X',0x2D},{'C',0x2E},{'V',0x2F},{'B',0x30},{'N',0x31},{'M',0x32},
    };
    int i;
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (i = 0; i < NT(t); i++) {
        struct mv m[2] = { { (UINT)t[i].c, 0, t[i].sc }, { t[i].sc, 1, (UINT)t[i].c } };
        run_mv(us, m, 2);
        EXPECT_EQ(U(MapVirtualKeyExW)(t[i].sc, MAPVK_VSC_TO_VK_EX, us), (UINT)t[i].c);
        EXPECT_EQ(U(MapVirtualKeyExW)(t[i].c, MAPVK_VK_TO_CHAR, us), (UINT)t[i].c);
    }
    US_END }
}
TEST(input_kbd_mapvk_us_oem_scancodes) {
    static const struct mv t[] = {
        { VK_OEM_1, 0, 0x27 }, { VK_OEM_7, 0, 0x28 }, { VK_OEM_3, 0, 0x29 }, { VK_OEM_5, 0, 0x2B },
        { VK_OEM_COMMA, 0, 0x33 }, { VK_OEM_PERIOD, 0, 0x34 }, { VK_OEM_2, 0, 0x35 }, { VK_OEM_MINUS, 0, 0x0C },
        { VK_OEM_PLUS, 0, 0x0D }, { VK_OEM_4, 0, 0x1A }, { VK_OEM_6, 0, 0x1B },
        { 0x27, 1, VK_OEM_1 }, { 0x28, 1, VK_OEM_7 }, { 0x29, 1, VK_OEM_3 }, { 0x2B, 1, VK_OEM_5 }, { 0x33, 1, VK_OEM_COMMA },
        { 0x34, 1, VK_OEM_PERIOD }, { 0x35, 1, VK_OEM_2 }, { 0x0C, 1, VK_OEM_MINUS }, { 0x0D, 1, VK_OEM_PLUS },
        { 0x1A, 1, VK_OEM_4 }, { 0x1B, 1, VK_OEM_6 },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN run_mv(us, t, NT(t)); US_END }
}
TEST(input_kbd_mapvk_us_to_char_oem) {
    static const struct mv t[] = {
        { VK_OEM_1, 2, ';' }, { VK_OEM_PLUS, 2, '=' }, { VK_OEM_COMMA, 2, ',' }, { VK_OEM_MINUS, 2, '-' },
        { VK_OEM_PERIOD, 2, '.' }, { VK_OEM_2, 2, '/' }, { VK_OEM_3, 2, '`' }, { VK_OEM_4, 2, '[' },
        { VK_OEM_5, 2, '\\' }, { VK_OEM_6, 2, ']' }, { VK_OEM_7, 2, '\'' },
        { '0', 2, '0' }, { '1', 2, '1' }, { '5', 2, '5' }, { '9', 2, '9' }, { VK_SPACE, 2, ' ' },
    };
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN run_mv(us, t, NT(t)); US_END }
}
TEST(input_kbd_mapvk_us_digit_row) {
    int i;
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (i = 0; i < 10; i++) {
        UINT vk = '0' + i, sc = i == 0 ? 0x0B : 0x01 + i;
        EXPECT_EQ(U(MapVirtualKeyExW)(vk, 0, us), sc);
        EXPECT_EQ(U(MapVirtualKeyExW)(sc, 1, us), vk);
    }
    US_END }
}
TEST(input_kbd_mapvk_us_oem102) {
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    EXPECT_EQ(U(MapVirtualKeyExW)(VK_OEM_102, 0, us), 0x56);
    EXPECT_EQ(U(MapVirtualKeyExW)(0x56, 1, us), VK_OEM_102);
    US_END }
}
TEST(input_kbd_mapvk_us_numpad_collisions_prefer_nav) {
    /* Where VK_NUMPADx and a navigation key share a scan code, VSC->VK returns the nav key. */
    static const UINT pairs[][2] = { {VK_NUMPAD0,VK_INSERT},{VK_NUMPAD1,VK_END},{VK_NUMPAD2,VK_DOWN},{VK_NUMPAD3,VK_NEXT},
        {VK_NUMPAD4,VK_LEFT},{VK_NUMPAD6,VK_RIGHT},{VK_NUMPAD7,VK_HOME},{VK_NUMPAD8,VK_UP},{VK_NUMPAD9,VK_PRIOR} };
    int i;
    REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (i = 0; i < NT(pairs); i++) {
        UINT a = U(MapVirtualKeyExW)(pairs[i][0], 0, us), b = U(MapVirtualKeyExW)(pairs[i][1], 0, us);
        if (a && a == b) CHECKF(U(MapVirtualKeyExW)(a, 1, us) != pairs[i][0], "scan %x returned the numpad key", a);
        CHECK(a != 0);
    }
    US_END }
}
/* ---- VkKeyScan ---- */
TEST(input_kbd_vkkeyscan_us_lowercase_letters) {
    int c;
    REQUIRE(U(VkKeyScanExW)); REQUIRE(U(VkKeyScanExA)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (c = 'a'; c <= 'z'; c++) {
        EXPECT_EQ((unsigned short)U(VkKeyScanExW)(c, us), c - 'a' + 'A');
        EXPECT_EQ((unsigned short)U(VkKeyScanExA)(c, us), c - 'a' + 'A');
    }
    US_END }
}
TEST(input_kbd_vkkeyscan_us_uppercase_letters) {
    int c;
    REQUIRE(U(VkKeyScanExW)); REQUIRE(U(VkKeyScanExA)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (c = 'A'; c <= 'Z'; c++) {
        EXPECT_EQ((unsigned short)U(VkKeyScanExW)(c, us), 0x100 | c);
        EXPECT_EQ((unsigned short)U(VkKeyScanExA)(c, us), 0x100 | c);
    }
    US_END }
}
TEST(input_kbd_vkkeyscan_us_digits_and_shifted_digits) {
    static const char sh[] = ")!@#$%^&*(";
    int i;
    REQUIRE(U(VkKeyScanExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (i = 0; i < 10; i++) {
        EXPECT_EQ((unsigned short)U(VkKeyScanExW)('0' + i, us), '0' + i);
        EXPECT_EQ((unsigned short)U(VkKeyScanExW)(sh[i], us), 0x100 | ('0' + i));
    }
    US_END }
}
TEST(input_kbd_vkkeyscan_us_punctuation) {
    static const struct { WCHAR c; unsigned short r; } t[] = {
        {' ',0x20},{';',0xBA},{':',0x1BA},{'=',0xBB},{'+',0x1BB},{',',0xBC},{'<',0x1BC},{'-',0xBD},{'_',0x1BD},
        {'.',0xBE},{'>',0x1BE},{'/',0xBF},{'?',0x1BF},{'`',0xC0},{'~',0x1C0},{'[',0xDB},{'{',0x1DB},{'\\',0xDC},
        {'|',0x1DC},{']',0xDD},{'}',0x1DD},{'\'',0xDE},{'"',0x1DE},
    };
    int i;
    REQUIRE(U(VkKeyScanExW)); REQUIRE(U(VkKeyScanExA)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (i = 0; i < NT(t); i++) {
        unsigned short w = U(VkKeyScanExW)(t[i].c, us), a = U(VkKeyScanExA)((CHAR)t[i].c, us);
        CHECKF(w == t[i].r, "VkKeyScanExW(U+%04x) = %04x want %04x", t[i].c, w, t[i].r);
        CHECKF(a == t[i].r, "VkKeyScanExA(U+%04x) = %04x want %04x", t[i].c, a, t[i].r);
    }
    US_END }
}
TEST(input_kbd_vkkeyscan_us_control_chars) {
    REQUIRE(U(VkKeyScanExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    /* Ctrl+letter characters: Ctrl bit (0x200) with the letter VK */
    EXPECT_EQ((unsigned short)U(VkKeyScanExW)(0x01, us), 0x241);
    EXPECT_EQ((unsigned short)U(VkKeyScanExW)(0x1A, us), 0x25A);
    t_note("VkKeyScan US: BS=%04x TAB=%04x ESC=%04x CR=%04x LF=%04x", (unsigned short)U(VkKeyScanExW)(0x08, us),
           (unsigned short)U(VkKeyScanExW)(0x09, us), (unsigned short)U(VkKeyScanExW)(0x1B, us),
           (unsigned short)U(VkKeyScanExW)(0x0D, us), (unsigned short)U(VkKeyScanExW)(0x0A, us));
    US_END }
}
TEST(input_kbd_vkkeyscan_us_unmappable) {
    static const WCHAR t[] = { 0x4E2D, 0x00E9, 0x20AC, 0x0416, 0xFFFF, 0xD800, 0x2603, 0x100 };
    int i;
    REQUIRE(U(VkKeyScanExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (i = 0; i < NT(t); i++)
        CHECKF(U(VkKeyScanExW)(t[i], us) == -1, "U+%04x -> %04x", t[i], (unsigned short)U(VkKeyScanExW)(t[i], us));
    US_END }
}
TEST(input_kbd_vkkeyscan_us_ex_matches_plain_on_current) {
    int c;
    HKL h;
    REQUIRE(U(VkKeyScanW)); REQUIRE(U(VkKeyScanExW)); REQUIRE(U(VkKeyScanA)); REQUIRE(U(VkKeyScanExA));
    h = U(GetKeyboardLayout)(0);
    for (c = 0; c < 128; c++) {
        CHECKF(U(VkKeyScanW)(c) == U(VkKeyScanExW)(c, h), "c=%d", c);
        CHECKF(U(VkKeyScanA)((CHAR)c) == U(VkKeyScanExA)((CHAR)c, h), "c=%d", c);
        CHECKF(U(VkKeyScanA)((CHAR)c) == U(VkKeyScanW)(c), "A/W c=%d", c);
    }
}
TEST(input_kbd_vkkeyscan_letters_english_current) {
    int c;
    REQUIRE(U(VkKeyScanW));
    if (!kbd_is_english()) { t_skip("non-English layout"); return; }
    for (c = 'a'; c <= 'z'; c++) EXPECT_EQ((unsigned short)U(VkKeyScanW)(c), c - 32);
    for (c = 'A'; c <= 'Z'; c++) EXPECT_EQ((unsigned short)U(VkKeyScanW)(c), 0x100 | c);
}
TEST(input_kbd_vkkeyscan_result_shape_all_ascii) {
    int c, bad = 0;
    REQUIRE(U(VkKeyScanW));
    for (c = 0; c < 256; c++) {
        SHORT r = U(VkKeyScanW)(c);
        if (r == -1) continue;
        /* only shift/ctrl/alt/hankaku/reserved bits 8..13 may be set in the high byte */
        if ((r & 0xFF) == 0xFF || (r & 0xC000)) bad++;
    }
    CHECK(bad == 0);
}
TEST(input_kbd_vkkeyscan_roundtrip_through_toascii_us) {
    int c, bad = 0, tested = 0;
    REQUIRE(U(VkKeyScanExW)); REQUIRE(U(ToUnicodeEx)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (c = 0x20; c < 0x7F; c++) {
        SHORT r = U(VkKeyScanExW)(c, us);
        BYTE st[256] = {0};
        WCHAR w[4] = {0};
        int n;
        if (r == -1) { bad++; continue; }
        if (r & 0x100) st[VK_SHIFT] = st[VK_LSHIFT] = 0x80;
        if (r & 0x200) st[VK_CONTROL] = st[VK_LCONTROL] = 0x80;
        if (r & 0x400) st[VK_MENU] = st[VK_LMENU] = 0x80;
        n = U(ToUnicodeEx)(r & 0xFF, U(MapVirtualKeyExW)(r & 0xFF, 0, us), st, w, 4, 0, us);
        tested++;
        if (n != 1 || w[0] != c) { bad++; t_note("char %c -> vk %04x -> n=%d w=%x", c, r, n, w[0]); }
    }
    US_END }
    CHECK(bad == 0);
    CHECK(tested == 95);
}

/* ---- OemKeyScan ---- */
TEST(input_kbd_oemkeyscan_ascii_us) {
    int c;
    REQUIRE(U(OemKeyScan)); REQUIRE(U(VkKeyScanW)); REQUIRE(U(MapVirtualKeyW));
    if (!kbd_is_us()) { t_skip("needs the active layout to be US"); return; }
    for (c = 0x20; c < 0x7F; c++) {
        DWORD r = U(OemKeyScan)(c);
        SHORT v = U(VkKeyScanW)(c);
        DWORD sc = U(MapVirtualKeyW)(v & 0xFF, 0);
        DWORD want = ((v & 0xFF00) << 8) | sc;
        CHECKF(r == want, "OemKeyScan(%c) = %08lx want %08lx", c, r, want);
    }
}
TEST(input_kbd_oemkeyscan_known_values_us) {
    REQUIRE(U(OemKeyScan));
    if (!kbd_is_us()) { t_skip("needs the active layout to be US"); return; }
    EXPECT_EQ(U(OemKeyScan)('a'), 0x1E);
    EXPECT_EQ(U(OemKeyScan)('A'), 0x1E | (1 << 16));
    EXPECT_EQ(U(OemKeyScan)(' '), 0x39);
    EXPECT_EQ(U(OemKeyScan)('1'), 0x02);
    EXPECT_EQ(U(OemKeyScan)('!'), 0x02 | (1 << 16));
    EXPECT_EQ(U(OemKeyScan)('0'), 0x0B);
    EXPECT_EQ(U(OemKeyScan)(0x01), 0x1E | (2 << 16));
}
TEST(input_kbd_oemkeyscan_unmappable_is_minus_one) {
    REQUIRE(U(OemKeyScan));
    /* Characters with no key on the keyboard yield 0xFFFFFFFF. 0x1F & 0xFF00 junk is ignored for DBCS lead only. */
    if (!kbd_is_us()) { t_skip("needs the active layout to be US"); return; }
    EXPECT_EQ(U(OemKeyScan)(0xFF), 0xFFFFFFFF);
    EXPECT_EQ(U(OemKeyScan)(0xF0), 0xFFFFFFFF);
}
TEST(input_kbd_oemkeyscan_deterministic) {
    WORD c;
    DWORD a[256];
    REQUIRE(U(OemKeyScan));
    for (c = 0; c < 256; c++) a[c] = U(OemKeyScan)(c);
    for (c = 0; c < 256; c++) EXPECT_EQ(U(OemKeyScan)(c), a[c]);
}
TEST(input_kbd_oemkeyscan_result_shape) {
    WORD c;
    int bad = 0;
    REQUIRE(U(OemKeyScan));
    for (c = 0; c < 256; c++) {
        DWORD r = U(OemKeyScan)(c);
        if (r == 0xFFFFFFFF) continue;
        if (r & 0xFF000000) bad++;       /* only the VkKeyScan shift byte at bits 16..23 */
        if ((r & 0xFFFF) == 0) bad++;    /* scan code must be nonzero */
    }
    CHECK(bad == 0);
}
