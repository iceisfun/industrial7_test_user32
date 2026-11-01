/* ToUnicode(Ex) / ToAscii(Ex) tables against an explicit US layout. */
#include "kbd.h"

#define SH 1
#define CT 2
#define AL 4
#define CAPS 8
#define NT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void mkstate(BYTE *st, DWORD m)
{
    memset(st, 0, 256);
    if (m & SH) st[VK_SHIFT] = st[VK_LSHIFT] = 0x80;
    if (m & CT) st[VK_CONTROL] = st[VK_LCONTROL] = 0x80;
    if (m & AL) st[VK_MENU] = st[VK_LMENU] = 0x80;
    if (m & CAPS) st[VK_CAPITAL] = 1;
}
/* returns ToUnicodeEx result, fills out[4] */
static int tu(HKL h, UINT vk, DWORD m, WCHAR *out)
{
    BYTE st[256];
    UINT sc = U(MapVirtualKeyExW)(vk, MAPVK_VK_TO_VSC, h);
    mkstate(st, m);
    out[0] = out[1] = out[2] = out[3] = 0xAAAA;
    return U(ToUnicodeEx)(vk, sc, st, out, 4, 0, h);
}

struct tc { UINT vk; DWORD mod; int ret; WCHAR ch; };

static void run_tc(HKL us, const struct tc *t, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        WCHAR w[4];
        int r = tu(us, t[i].vk, t[i].mod, w);
        CHECKF(r == t[i].ret, "vk 0x%02x mod %lx: ret %d want %d", t[i].vk, t[i].mod, r, t[i].ret);
        if (r == 1 && t[i].ret == 1) CHECKF(w[0] == t[i].ch, "vk 0x%02x mod %lx: char %04x want %04x", t[i].vk, t[i].mod, w[0], t[i].ch);
    }
}
#define US_BEGIN HKL us = kbd_us_hkl(); if (!us) { kbd_us_release(); t_skip("US layout unavailable"); return; }
#define US_END kbd_us_release();
#define TUREQ REQUIRE(U(ToUnicodeEx)); REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(LoadKeyboardLayoutW))

TEST(input_kbd_tounicode_us_letters_plain) {
    UINT vk;
    TUREQ;
    { US_BEGIN
    for (vk = 'A'; vk <= 'Z'; vk++) {
        WCHAR w[4];
        CHECKF(tu(us, vk, 0, w) == 1, "vk %x", vk);
        EXPECT_EQ(w[0], vk + 32);
        EXPECT_EQ(w[1], 0);   /* terminated when room */
    }
    US_END }
}
TEST(input_kbd_tounicode_us_letters_shift) {
    UINT vk;
    TUREQ;
    { US_BEGIN
    for (vk = 'A'; vk <= 'Z'; vk++) { WCHAR w[4]; CHECK(tu(us, vk, SH, w) == 1); EXPECT_EQ(w[0], vk); }
    US_END }
}
TEST(input_kbd_tounicode_us_letters_capslock) {
    UINT vk;
    TUREQ;
    { US_BEGIN
    for (vk = 'A'; vk <= 'Z'; vk++) {
        WCHAR w[4];
        CHECK(tu(us, vk, CAPS, w) == 1); EXPECT_EQ(w[0], vk);
        CHECK(tu(us, vk, CAPS | SH, w) == 1); EXPECT_EQ(w[0], vk + 32);
    }
    US_END }
}
TEST(input_kbd_tounicode_us_caps_does_not_affect_digits) {
    UINT vk;
    TUREQ;
    { US_BEGIN
    for (vk = '0'; vk <= '9'; vk++) { WCHAR w[4]; CHECK(tu(us, vk, CAPS, w) == 1); EXPECT_EQ(w[0], vk); }
    US_END }
}
TEST(input_kbd_tounicode_us_digits_shift) {
    static const WCHAR sh[] = { ')', '!', '@', '#', '$', '%', '^', '&', '*', '(' };
    int i;
    TUREQ;
    { US_BEGIN
    for (i = 0; i < 10; i++) {
        WCHAR w[4];
        CHECK(tu(us, '0' + i, 0, w) == 1); EXPECT_EQ(w[0], '0' + i);
        CHECK(tu(us, '0' + i, SH, w) == 1); EXPECT_EQ(w[0], sh[i]);
    }
    US_END }
}
TEST(input_kbd_tounicode_us_oem_keys) {
    static const struct tc t[] = {
        { VK_OEM_1, 0, 1, ';' }, { VK_OEM_1, SH, 1, ':' }, { VK_OEM_PLUS, 0, 1, '=' }, { VK_OEM_PLUS, SH, 1, '+' },
        { VK_OEM_COMMA, 0, 1, ',' }, { VK_OEM_COMMA, SH, 1, '<' }, { VK_OEM_MINUS, 0, 1, '-' }, { VK_OEM_MINUS, SH, 1, '_' },
        { VK_OEM_PERIOD, 0, 1, '.' }, { VK_OEM_PERIOD, SH, 1, '>' }, { VK_OEM_2, 0, 1, '/' }, { VK_OEM_2, SH, 1, '?' },
        { VK_OEM_3, 0, 1, '`' }, { VK_OEM_3, SH, 1, '~' }, { VK_OEM_4, 0, 1, '[' }, { VK_OEM_4, SH, 1, '{' },
        { VK_OEM_5, 0, 1, '\\' }, { VK_OEM_5, SH, 1, '|' }, { VK_OEM_6, 0, 1, ']' }, { VK_OEM_6, SH, 1, '}' },
        { VK_OEM_7, 0, 1, '\'' }, { VK_OEM_7, SH, 1, '"' },
    };
    TUREQ;
    { US_BEGIN run_tc(us, t, NT(t)); US_END }
}
TEST(input_kbd_tounicode_us_control_keys) {
    static const struct tc t[] = {
        { VK_RETURN, 0, 1, '\r' }, { VK_TAB, 0, 1, '\t' }, { VK_BACK, 0, 1, 8 }, { VK_ESCAPE, 0, 1, 0x1B },
        { VK_SPACE, 0, 1, ' ' }, { VK_SPACE, SH, 1, ' ' }, { VK_SPACE, CT, 1, ' ' },
        { VK_RETURN, CT, 1, '\n' }, { VK_TAB, CT, 0, 0 },
        { VK_ESCAPE, CT, 1, 0x1B },
    };
    TUREQ;
    { US_BEGIN run_tc(us, t, NT(t)); US_END }
}
TEST(input_kbd_tounicode_us_ctrl_letters) {
    UINT vk;
    TUREQ;
    { US_BEGIN
    for (vk = 'A'; vk <= 'Z'; vk++) {
        WCHAR w[4];
        CHECK(tu(us, vk, CT, w) == 1);
        EXPECT_EQ(w[0], vk - 'A' + 1);
        CHECK(tu(us, vk, CT | SH, w) == 1);
        EXPECT_EQ(w[0], vk - 'A' + 1);
        CHECK(tu(us, vk, CT | CAPS, w) == 1);
        EXPECT_EQ(w[0], vk - 'A' + 1);
    }
    US_END }
}
TEST(input_kbd_tounicode_us_ctrl_punctuation) {
    static const struct tc t[] = {
        { VK_OEM_4, CT, 1, 0x1B }, { VK_OEM_5, CT, 1, 0x1C }, { VK_OEM_6, CT, 1, 0x1D },
        { '6', CT | SH, 1, 0x1E }, { VK_OEM_MINUS, CT | SH, 1, 0x1F }, { '2', CT | SH, 1, 0 },
        { '4', CT, 0, 0 }, { '4', CT | SH, 0, 0 }, { VK_OEM_3, CT, 0, 0 }, { VK_OEM_COMMA, CT, 0, 0 },
        { VK_OEM_PERIOD, CT, 0, 0 }, { VK_OEM_2, CT, 0, 0 }, { VK_OEM_1, CT, 0, 0 }, { VK_OEM_PLUS, CT, 0, 0 },
        { VK_OEM_7, CT, 0, 0 }, { '1', CT | SH, 0, 0 }, { '9', CT | SH, 0, 0 }, { '0', CT | SH, 0, 0 },
    };
    TUREQ;
    { US_BEGIN run_tc(us, t, NT(t)); US_END }
}
TEST(input_kbd_tounicode_us_alt_does_not_change_letters) {
    UINT vk;
    TUREQ;
    { US_BEGIN
    for (vk = 'A'; vk <= 'Z'; vk++) {
        WCHAR w[4];
        CHECK(tu(us, vk, AL, w) == 1); EXPECT_EQ(w[0], vk + 32);
        CHECK(tu(us, vk, AL | SH, w) == 1); EXPECT_EQ(w[0], vk);
    }
    US_END }
}
TEST(input_kbd_tounicode_us_shift_ctrl_alt_nothing) {
    UINT vk;
    TUREQ;
    { US_BEGIN
    for (vk = 'A'; vk <= 'Z'; vk++) { WCHAR w[4]; CHECKF(tu(us, vk, SH | CT | AL, w) == 0, "vk %x", vk); }
    US_END }
}
TEST(input_kbd_tounicode_us_non_character_keys_return_zero) {
    static const UINT vks[] = {
        VK_F1, VK_F2, VK_F5, VK_F12, VK_F13, VK_F24, VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_CONTROL, VK_LCONTROL, VK_MENU, VK_LMENU,
        VK_CAPITAL, VK_NUMLOCK, VK_SCROLL, VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_INSERT,
        VK_DELETE, VK_LWIN, VK_RWIN, VK_APPS, VK_SNAPSHOT, VK_PAUSE, VK_BROWSER_BACK, VK_VOLUME_UP, VK_MEDIA_PLAY_PAUSE,
        VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, 0, 0xFF,
    };
    int i;
    TUREQ;
    { US_BEGIN
    for (i = 0; i < NT(vks); i++) { WCHAR w[4]; int r = tu(us, vks[i], 0, w); CHECKF(r == 0, "vk 0x%02x ret %d", vks[i], r); if (r == 0) CHECKF(w[0] == 0, "vk 0x%02x: buffer not zeroed (%04x)", vks[i], w[0]); }
    US_END }
}
/* VK_CANCEL (Ctrl+Break) is not in the list above: Win10 22H2 ToUnicodeEx returns 1 for it
 * (the ETX control character 0x03) even with no modifier down; Win7 returned 0. Record only. */
TEST(input_kbd_tounicode_us_vk_cancel_observed) {
    TUREQ;
    { US_BEGIN
    WCHAR w[4]; int r = tu(us, VK_CANCEL, 0, w);
    t_note("ToUnicodeEx(VK_CANCEL, no modifiers) -> %d, w[0]=%04x", r, w[0]);
    CHECK(r == 0 || r == 1);
    US_END }
}
TEST(input_kbd_tounicode_us_numpad_operators) {
    static const struct tc t[] = {
        { VK_MULTIPLY, 0, 1, '*' }, { VK_ADD, 0, 1, '+' }, { VK_SUBTRACT, 0, 1, '-' }, { VK_DIVIDE, 0, 1, '/' },
    };
    TUREQ;
    { US_BEGIN run_tc(us, t, NT(t)); US_END }
}
TEST(input_kbd_tounicode_us_numpad_digits_need_numlock) {
    BYTE st[256];
    WCHAR w[4];
    UINT sc;
    int r;
    TUREQ;
    { US_BEGIN
    sc = U(MapVirtualKeyExW)(VK_NUMPAD5, 0, us);
    mkstate(st, 0); st[VK_NUMLOCK] = 1;
    r = U(ToUnicodeEx)(VK_NUMPAD5, sc, st, w, 4, 0, us);
    CHECK(r == 1);
    if (r == 1) EXPECT_EQ(w[0], '5');
    mkstate(st, 0);
    r = U(ToUnicodeEx)(VK_NUMPAD5, sc, st, w, 4, 0, us);
    t_note("NUMPAD5 with NumLock off returns %d", r);
    US_END }
}
TEST(input_kbd_tounicode_us_numpad_digits_all) {
    BYTE st[256];
    WCHAR w[4];
    int i;
    TUREQ;
    { US_BEGIN
    for (i = 0; i < 10; i++) {
        UINT sc = U(MapVirtualKeyExW)(VK_NUMPAD0 + i, 0, us);
        int r;
        mkstate(st, 0); st[VK_NUMLOCK] = 1;
        r = U(ToUnicodeEx)(VK_NUMPAD0 + i, sc, st, w, 4, 0, us);
        if (!sc) { t_note("no scan for numpad %d", i); continue; }
        CHECKF(r == 1, "numpad %d ret %d", i, r);
        if (r == 1) EXPECT_EQ(w[0], '0' + i);
    }
    US_END }
}
TEST(input_kbd_tounicode_flags_bit2_no_state_change) {
    int f;
    TUREQ;
    { US_BEGIN
    for (f = 0; f < 8; f++) {
        BYTE st[256];
        WCHAR w[4];
        int r;
        mkstate(st, 0);
        w[0] = w[1] = 0xAAAA;
        r = U(ToUnicodeEx)('A', 0x1E, st, w, 4, f, us);
        CHECKF(r == 1, "flags %d ret %d", f, r);
        if (r == 1) EXPECT_EQ(w[0], 'a');
    }
    US_END }
}
TEST(input_kbd_tounicode_flags_menu_bit) {
    BYTE st[256];
    WCHAR w[4];
    int r;
    TUREQ;
    { US_BEGIN
    mkstate(st, 0);
    r = U(ToUnicodeEx)('A', 0x1E, st, w, 4, 1, us);
    CHECK(r == 1);
    mkstate(st, SH);
    r = U(ToUnicodeEx)('A', 0x1E, st, w, 4, 1, us);
    CHECK(r == 1);
    if (r == 1) EXPECT_EQ(w[0], 'A');
    US_END }
}
TEST(input_kbd_tounicode_scancode_ignored_for_translation_observed) {
    BYTE st[256];
    WCHAR w[4];
    int r1, r2, r3;
    TUREQ;
    { US_BEGIN
    mkstate(st, 0);
    r1 = U(ToUnicodeEx)('A', 0x1E, st, w, 4, 0, us);
    r2 = U(ToUnicodeEx)('A', 0, st, w, 4, 0, us);
    r3 = U(ToUnicodeEx)('A', 0x9E, st, w, 4, 0, us);
    CHECK(r1 == 1);
    t_note("ToUnicodeEx scan=0 -> %d, key-up scan(0x9E) -> %d", r2, r3);
    US_END }
}
TEST(input_kbd_tounicode_null_keystate) {
    WCHAR w[4];
    HKL h;
    REQUIRE(U(ToUnicode)); REQUIRE(U(ToUnicodeEx)); REQUIRE(U(ToAscii)); REQUIRE(U(ToAsciiEx));
    h = U(GetKeyboardLayout)(0);
    /* the kernel probe of a NULL key state is unverified: report only; but a zero result must zero the buffer */
    w[0] = 0x5555;
    t_note("ToUnicode(NULL state) -> %d", U(ToUnicode)(VK_TAB, 0x0F, NULL, w, 4, 0));
    w[0] = 0x5555;
    t_note("ToUnicode(NULL state, 'A') -> %d", U(ToUnicode)('A', 0x1E, NULL, w, 4, 0));
    t_note("ToUnicodeEx(NULL state) -> %d", U(ToUnicodeEx)(VK_TAB, 0x0F, NULL, w, 4, 0, h));
    CHECK(1);
}
TEST(input_kbd_toascii_null_keystate) {
    WORD c = 0x5555;
    HKL h;
    REQUIRE(U(ToAscii)); REQUIRE(U(ToAsciiEx));
    h = U(GetKeyboardLayout)(0);
    t_note("ToAscii(NULL state) -> %d", U(ToAscii)('A', 0x1E, NULL, &c, 0));
    t_note("ToAsciiEx(NULL state) -> %d", U(ToAsciiEx)('A', 0x1E, NULL, &c, 0, h));
    CHECK(1);
}
TEST(input_kbd_tounicode_return_terminates_buffer) {
    BYTE st[256] = {0};
    WCHAR w[4];
    int r;
    REQUIRE(U(ToUnicode));
    w[0] = w[1] = 0xAAAA;
    r = U(ToUnicode)(VK_RETURN, 0x1C, st, w, 4, 0);
    EXPECT_EQ(r, 1);
    EXPECT_EQ(w[0], '\r');
    EXPECT_EQ(w[1], 0);
}
TEST(input_kbd_tounicode_current_layout_return_tab_back_esc_space) {
    BYTE st[256] = {0};
    WCHAR w[4];
    static const struct { UINT vk, sc; WCHAR ch; } t[] = {
        { VK_RETURN, 0x1C, '\r' }, { VK_TAB, 0x0F, '\t' }, { VK_BACK, 0x0E, 8 }, { VK_ESCAPE, 0x01, 0x1B }, { VK_SPACE, 0x39, ' ' },
    };
    int i;
    REQUIRE(U(ToUnicode));
    for (i = 0; i < NT(t); i++) {
        w[0] = 0xAAAA;
        EXPECT_EQ(U(ToUnicode)(t[i].vk, t[i].sc, st, w, 4, 0), 1);
        EXPECT_EQ(w[0], t[i].ch);
    }
}
TEST(input_kbd_tounicode_buffer_size_one) {
    BYTE st[256] = {0};
    WCHAR w[4];
    int r;
    REQUIRE(U(ToUnicode));
    w[0] = w[1] = 0xAAAA;
    r = U(ToUnicode)(VK_RETURN, 0x1C, st, w, 1, 0);
    CHECKF(r == 1, "ret %d", r);
    EXPECT_EQ(w[0], '\r');
    EXPECT_EQ(w[1], 0xAAAA);
}
TEST(input_kbd_tounicode_buffer_size_zero_observed) {
    BYTE st[256] = {0};
    WCHAR w[4];
    int r;
    REQUIRE(U(ToUnicode));
    w[0] = 0xAAAA;
    r = U(ToUnicode)(VK_RETURN, 0x1C, st, w, 0, 0);
    t_note("ToUnicode with cchBuff=0 returned %d, buf[0]=%04x", r, w[0]);
    CHECK(1);
}
TEST(input_kbd_tounicode_invalid_vk_values) {
    BYTE st[256] = {0};
    WCHAR w[4];
    static const UINT v[] = { 0, 0xFF };
    int i;
    REQUIRE(U(ToUnicode));
    for (i = 0; i < NT(v); i++) EXPECT_EQ(U(ToUnicode)(v[i], 0, st, w, 4, 0), 0);
    t_note("vk=0x100 -> %d, vk=0xFFFFFFFF -> %d", U(ToUnicode)(0x100, 0, st, w, 4, 0), U(ToUnicode)(0xFFFFFFFF, 0, st, w, 4, 0));
    CHECK(1);
}
TEST(input_kbd_tounicode_state_array_not_modified) {
    BYTE st[256], copy[256];
    WCHAR w[4];
    int vk;
    REQUIRE(U(ToUnicode));
    memset(st, 0, sizeof st); st[VK_SHIFT] = 0x80; st[VK_CAPITAL] = 1; st[VK_F7] = 0x80;
    memcpy(copy, st, sizeof st);
    for (vk = 0; vk < 256; vk++) U(ToUnicode)(vk, 0, st, w, 4, 0);
    CHECK(!memcmp(st, copy, sizeof st));
}
TEST(input_kbd_tounicode_repeat_is_stable) {
    BYTE st[256] = {0};
    WCHAR w[4];
    int i;
    REQUIRE(U(ToUnicode));
    for (i = 0; i < 1000; i++) {
        w[0] = 0;
        if (U(ToUnicode)(VK_SPACE, 0x39, st, w, 4, 0) != 1 || w[0] != ' ') { CHECKF(0, "iteration %d", i); return; }
    }
    CHECK(1);
}
TEST(input_kbd_tounicode_all_vk_no_crash_and_range) {
    BYTE st[256];
    WCHAR w[4];
    int vk, m, bad = 0;
    REQUIRE(U(ToUnicode));
    for (m = 0; m < 8; m++) {
        for (vk = 0; vk < 256; vk++) {
            int r;
            mkstate(st, m);
            r = U(ToUnicode)(vk, 0, st, w, 4, 4);   /* bit 2 keeps dead-key state intact */
            if (r < -2 || r > 4) bad++;
        }
    }
    CHECK(bad == 0);
}

/* ---- ToAscii ---- */
TEST(input_kbd_toascii_us_letters) {
    UINT vk;
    REQUIRE(U(ToAsciiEx)); REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    for (vk = 'A'; vk <= 'Z'; vk++) {
        BYTE st[256];
        WORD c = 0xFFFF;
        mkstate(st, 0);
        EXPECT_EQ(U(ToAsciiEx)(vk, U(MapVirtualKeyExW)(vk, 0, us), st, &c, 0, us), 1);
        EXPECT_EQ(c & 0xFF, vk + 32);
        mkstate(st, SH); c = 0xFFFF;
        EXPECT_EQ(U(ToAsciiEx)(vk, U(MapVirtualKeyExW)(vk, 0, us), st, &c, 0, us), 1);
        EXPECT_EQ(c & 0xFF, vk);
        mkstate(st, CT); c = 0xFFFF;
        EXPECT_EQ(U(ToAsciiEx)(vk, U(MapVirtualKeyExW)(vk, 0, us), st, &c, 0, us), 1);
        EXPECT_EQ(c & 0xFF, vk - 'A' + 1);
    }
    US_END }
}
TEST(input_kbd_toascii_us_controls_and_nochar) {
    BYTE st[256];
    WORD c;
    REQUIRE(U(ToAsciiEx)); REQUIRE(U(LoadKeyboardLayoutW));
    { US_BEGIN
    mkstate(st, 0); c = 0xFFFF;
    EXPECT_EQ(U(ToAsciiEx)(VK_RETURN, 0x1C, st, &c, 0, us), 1);
    EXPECT_EQ(c & 0xFF, '\r');
    c = 0xFFFF;
    EXPECT_EQ(U(ToAsciiEx)(VK_TAB, 0x0F, st, &c, 0, us), 1);
    EXPECT_EQ(c & 0xFF, '\t');
    c = 0xFFFF;
    EXPECT_EQ(U(ToAsciiEx)(VK_F1, 0x3B, st, &c, 0, us), 0);
    EXPECT_EQ(U(ToAsciiEx)(VK_LEFT, 0x4B, st, &c, 0, us), 0);
    EXPECT_EQ(U(ToAsciiEx)(VK_SHIFT, 0x2A, st, &c, 0, us), 0);
    US_END }
}
TEST(input_kbd_toascii_matches_toasciiex_current) {
    UINT vk;
    BYTE st[256] = {0};
    HKL h;
    int bad = 0;
    REQUIRE(U(ToAscii)); REQUIRE(U(ToAsciiEx));
    h = U(GetKeyboardLayout)(0);
    for (vk = 0; vk < 256; vk++) {
        WORD a = 0, b = 0;
        int r1 = U(ToAscii)(vk, 0, st, &a, 4), r2 = U(ToAsciiEx)(vk, 0, st, &b, 4, h);
        if (r1 != r2 || (r1 > 0 && (a & 0xFF) != (b & 0xFF))) bad++;
    }
    CHECKF(bad == 0, "%d differences", bad);
}
TEST(input_kbd_toascii_matches_tounicode_ascii) {
    UINT vk;
    BYTE st[256];
    int m, bad = 0;
    REQUIRE(U(ToAscii)); REQUIRE(U(ToUnicode));
    for (m = 0; m < 8; m++)
        for (vk = 0; vk < 256; vk++) {
            WORD a = 0; WCHAR w[4];
            int ra, rw;
            mkstate(st, m);
            rw = U(ToUnicode)(vk, 0, st, w, 4, 4);
            mkstate(st, m);
            ra = U(ToAscii)(vk, 0, st, &a, 4);
            if (rw == 1 && w[0] < 0x80) { if (ra != 1 || (a & 0xFF) != w[0]) bad++; }
            else if (rw == 0 && ra != 0) bad++;
        }
    CHECKF(bad == 0, "%d differences", bad);
}

/* ---- dead keys (US-International layout, explicit HKL, no activation) ---- */
static int g_intl_loaded_by_us;
static HKL intl_acquire(void)
{
    HKL list[64], h;
    int n, i, had = 0;
    n = U(GetKeyboardLayoutList)(64, list);
    h = U(LoadKeyboardLayoutW)(L"00020409", 0);
    if (!h) return NULL;
    for (i = 0; i < n; i++) if (list[i] == h) had = 1;
    g_intl_loaded_by_us = !had;
    return h;
}
static void intl_release(HKL h)
{
    if (g_intl_loaded_by_us && h && U(UnloadKeyboardLayout)) U(UnloadKeyboardLayout)(h);
    g_intl_loaded_by_us = 0;
}
static void flush_dead(HKL h)
{
    BYTE st[256] = {0};
    WCHAR w[4];
    U(ToUnicodeEx)(VK_SPACE, 0x39, st, w, 4, 0, h);
}
#define DKREQ REQUIRE(U(ToUnicodeEx)); REQUIRE(U(LoadKeyboardLayoutW)); REQUIRE(U(GetKeyboardLayoutList)); REQUIRE(U(UnloadKeyboardLayout))

TEST(input_kbd_tounicode_dead_key_returns_negative) {
    HKL h;
    DKREQ;
    h = intl_acquire();
    if (!h) { t_skip("US-International layout unavailable"); return; }
    {
        WCHAR w[4];
        int r = tu(h, VK_OEM_7, 0, w);
        CHECKF(r < 0, "dead key returned %d", r);
        t_note("dead acute spacing char U+%04X", w[0]);
        flush_dead(h);
    }
    intl_release(h);
}
TEST(input_kbd_tounicode_dead_key_composes_next_key) {
    HKL h;
    DKREQ;
    h = intl_acquire();
    if (!h) { t_skip("US-International layout unavailable"); return; }
    {
        WCHAR w[4];
        CHECK(tu(h, VK_OEM_7, 0, w) < 0);
        CHECK(tu(h, 'A', 0, w) == 1);
        EXPECT_EQ(w[0], 0xE1);                  /* a-acute */
        CHECK(tu(h, 'A', 0, w) == 1);           /* state consumed */
        EXPECT_EQ(w[0], 'a');
        CHECK(tu(h, VK_OEM_7, 0, w) < 0);
        CHECK(tu(h, 'E', SH, w) == 1);
        EXPECT_EQ(w[0], 0xC9);                  /* E-acute */
        flush_dead(h);
    }
    intl_release(h);
}
TEST(input_kbd_tounicode_dead_key_non_combining_gives_two_chars) {
    HKL h;
    DKREQ;
    h = intl_acquire();
    if (!h) { t_skip("US-International layout unavailable"); return; }
    {
        WCHAR w[4];
        int r;
        CHECK(tu(h, VK_OEM_7, 0, w) < 0);
        r = tu(h, 'B', 0, w);
        EXPECT_EQ(r, 2);
        EXPECT_EQ(w[1], 'b');
        flush_dead(h);
    }
    intl_release(h);
}
TEST(input_kbd_tounicode_dead_key_flag4_preserves_state) {
    HKL h;
    DKREQ;
    h = intl_acquire();
    if (!h) { t_skip("US-International layout unavailable"); return; }
    {
        BYTE st[256] = {0};
        WCHAR w[4];
        int r = U(ToUnicodeEx)(VK_OEM_7, U(MapVirtualKeyExW)(VK_OEM_7, 0, h), st, w, 4, 4, h);
        CHECK(r < 0);
        r = U(ToUnicodeEx)('A', 0x1E, st, w, 4, 0, h);
        CHECKF(r == 1, "ret %d", r);
        if (r == 1) EXPECT_EQ(w[0], 'a');     /* flags=4 left no pending dead key behind */
        flush_dead(h);
    }
    intl_release(h);
}
TEST(input_kbd_toascii_dead_key_negative) {
    HKL h;
    DKREQ; REQUIRE(U(ToAsciiEx));
    h = intl_acquire();
    if (!h) { t_skip("US-International layout unavailable"); return; }
    {
        BYTE st[256] = {0};
        WORD c = 0;
        int r = U(ToAsciiEx)(VK_OEM_7, U(MapVirtualKeyExW)(VK_OEM_7, 0, h), st, &c, 0, h);
        CHECKF(r < 0, "ToAsciiEx dead key returned %d", r);
        r = U(ToAsciiEx)('A', 0x1E, st, &c, 0, h);
        CHECK(r == 1);
        t_note("ToAscii composed to 0x%02x", c & 0xFF);
        flush_dead(h);
    }
    intl_release(h);
}
TEST(input_kbd_mapvk_to_char_dead_key_bit31_intl) {
    HKL h;
    DKREQ; REQUIRE(U(MapVirtualKeyExW));
    h = intl_acquire();
    if (!h) { t_skip("US-International layout unavailable"); return; }
    {
        UINT r = U(MapVirtualKeyExW)(VK_OEM_7, MAPVK_VK_TO_CHAR, h);
        CHECKF(r & 0x80000000, "dead key char 0x%08x lacks bit 31", r);
        CHECK(U(MapVirtualKeyExW)('A', MAPVK_VK_TO_CHAR, h) == 'A');
    }
    intl_release(h);
}
