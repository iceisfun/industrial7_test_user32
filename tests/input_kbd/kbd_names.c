/* GetKeyNameTextA/W. */
#include "kbd.h"

#define LP(scan, ext) ((LONG)(((scan) << 16) | ((ext) ? 0x01000000 : 0)))
#define NT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void wtoa(const WCHAR *w, char *a, int n)
{
    int i;
    for (i = 0; i < n - 1 && w[i]; i++) a[i] = (char)w[i];
    a[i] = 0;
}

struct kn { LONG lp; const char *name; };

static void run_kn(const struct kn *t, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        char a[64]; WCHAR w[64]; char wa[64];
        int ra, rw;
        memset(a, 0xCC, sizeof a); memset(w, 0xCC, sizeof w);
        ra = U(GetKeyNameTextA)(t[i].lp, a, sizeof a);
        rw = U(GetKeyNameTextW)(t[i].lp, w, 64);
        CHECKF(ra == (int)strlen(t[i].name), "A lparam %08lx: len %d want %d", t[i].lp, ra, (int)strlen(t[i].name));
        CHECKF(rw == (int)strlen(t[i].name), "W lparam %08lx: len %d want %d", t[i].lp, rw, (int)strlen(t[i].name));
        if (ra > 0 && ra < 63) CHECKF(!strcmp(a, t[i].name), "A lparam %08lx: \"%s\" want \"%s\"", t[i].lp, a, t[i].name);
        if (rw > 0 && rw < 63) { wtoa(w, wa, 64); CHECKF(!strcmp(wa, t[i].name), "W lparam %08lx: \"%s\" want \"%s\"", t[i].lp, wa, t[i].name); }
    }
}
#define KNREQ REQUIRE(U(GetKeyNameTextA)); REQUIRE(U(GetKeyNameTextW))
#define US_ONLY if (!kbd_is_us()) { t_skip("names are layout specific; active layout is not US"); return; }

TEST(input_kbd_keyname_us_editing_keys) {
    static const struct kn t[] = {
        { LP(0x01,0), "Esc" }, { LP(0x0E,0), "Backspace" }, { LP(0x0F,0), "Tab" }, { LP(0x1C,0), "Enter" },
        { LP(0x39,0), "Space" }, { LP(0x3A,0), "Caps Lock" },
    };
    KNREQ; US_ONLY
    run_kn(t, NT(t));
}
TEST(input_kbd_keyname_us_modifiers) {
    static const struct kn t[] = {
        { LP(0x2A,0), "Shift" }, { LP(0x36,0), "Right Shift" }, { LP(0x1D,0), "Ctrl" }, { LP(0x1D,1), "Right Ctrl" },
        { LP(0x38,0), "Alt" }, { LP(0x38,1), "Right Alt" },
    };
    KNREQ; US_ONLY
    run_kn(t, NT(t));
}
TEST(input_kbd_keyname_us_function_keys) {
    static const struct kn t[] = {
        { LP(0x3B,0), "F1" }, { LP(0x3C,0), "F2" }, { LP(0x3D,0), "F3" }, { LP(0x3E,0), "F4" }, { LP(0x3F,0), "F5" },
        { LP(0x40,0), "F6" }, { LP(0x41,0), "F7" }, { LP(0x42,0), "F8" }, { LP(0x43,0), "F9" }, { LP(0x44,0), "F10" },
        { LP(0x57,0), "F11" }, { LP(0x58,0), "F12" },
    };
    KNREQ; US_ONLY
    run_kn(t, NT(t));
}
TEST(input_kbd_keyname_us_letters) {
    static const struct { int sc; const char *n; } t[] = {
        {0x10,"Q"},{0x11,"W"},{0x12,"E"},{0x13,"R"},{0x14,"T"},{0x15,"Y"},{0x16,"U"},{0x17,"I"},{0x18,"O"},{0x19,"P"},
        {0x1E,"A"},{0x1F,"S"},{0x20,"D"},{0x21,"F"},{0x22,"G"},{0x23,"H"},{0x24,"J"},{0x25,"K"},{0x26,"L"},
        {0x2C,"Z"},{0x2D,"X"},{0x2E,"C"},{0x2F,"V"},{0x30,"B"},{0x31,"N"},{0x32,"M"},
    };
    int i;
    KNREQ; US_ONLY
    for (i = 0; i < NT(t); i++) { struct kn k = { LP(t[i].sc, 0), t[i].n }; run_kn(&k, 1); }
}
TEST(input_kbd_keyname_us_digits_and_punctuation) {
    static const struct kn t[] = {
        { LP(0x02,0), "1" }, { LP(0x03,0), "2" }, { LP(0x04,0), "3" }, { LP(0x05,0), "4" }, { LP(0x06,0), "5" },
        { LP(0x07,0), "6" }, { LP(0x08,0), "7" }, { LP(0x09,0), "8" }, { LP(0x0A,0), "9" }, { LP(0x0B,0), "0" },
        { LP(0x0C,0), "-" }, { LP(0x0D,0), "=" }, { LP(0x1A,0), "[" }, { LP(0x1B,0), "]" }, { LP(0x27,0), ";" },
        { LP(0x28,0), "'" }, { LP(0x29,0), "`" }, { LP(0x2B,0), "\\" }, { LP(0x33,0), "," }, { LP(0x34,0), "." },
        { LP(0x35,0), "/" },
    };
    KNREQ; US_ONLY
    run_kn(t, NT(t));
}
TEST(input_kbd_keyname_us_extended_bit_semantics) {
    static const struct kn t[] = {
        { 0x00370000, "Num *" }, { 0x01370000, "Prnt Scrn" },
        { 0x00450000, "Pause" }, { 0x01450000, "Num Lock" },
        { 0x00460000, "Scroll Lock" }, { 0x01460000, "Break" },
        { 0x01480000, "Up" }, { 0x011d0000, "Right Ctrl" }, { 0x001d0000, "Ctrl" },
    };
    KNREQ; US_ONLY
    run_kn(t, NT(t));
}
TEST(input_kbd_keyname_us_dont_care_bit25) {
    /* bit 25 asks for a name that does not distinguish left and right */
    static const struct kn t[] = {
        { 0x021d0000, "Ctrl" }, { 0x022A0000, "Shift" }, { 0x02380000, "Alt" },
    };
    KNREQ; US_ONLY
    run_kn(t, NT(t));
}
TEST(input_kbd_keyname_us_dont_care_bit25_observed) {
    static const LONG l[] = { 0x021d0000 | 0x01000000, 0x02360000, 0x02380000 | 0x01000000 };
    int i;
    KNREQ;
    for (i = 0; i < NT(l); i++) {
        char a[64] = "";
        int r = U(GetKeyNameTextA)(l[i], a, sizeof a);
        t_note("GetKeyNameText(%08lx) = %d \"%s\"", l[i], r, a);
    }
    CHECK(1);
}
TEST(input_kbd_keyname_us_navigation_extended) {
    static const struct kn t[] = {
        { LP(0x47,1), "Home" }, { LP(0x48,1), "Up" }, { LP(0x49,1), "Page Up" }, { LP(0x4B,1), "Left" },
        { LP(0x4D,1), "Right" }, { LP(0x4F,1), "End" }, { LP(0x50,1), "Down" }, { LP(0x51,1), "Page Down" },
        { LP(0x52,1), "Insert" }, { LP(0x53,1), "Delete" },
    };
    KNREQ; US_ONLY
    run_kn(t, NT(t));
}
TEST(input_kbd_keyname_us_numpad) {
    static const struct kn t[] = {
        { LP(0x4A,0), "Num -" }, { LP(0x4E,0), "Num +" }, { LP(0x35,1), "Num /" }, { LP(0x1C,1), "Num Enter" },
    };
    KNREQ; US_ONLY
    run_kn(t, NT(t));
}
TEST(input_kbd_keyname_generic_letter_key_has_name) {
    char a[64]; WCHAR w[64];
    int ra, rw;
    KNREQ;
    memset(a, 0xCC, sizeof a); memset(w, 0xCC, sizeof w);
    ra = U(GetKeyNameTextA)(0x1E << 16, a, sizeof a);
    rw = U(GetKeyNameTextW)(0x1E << 16, w, 64);
    CHECK(ra > 0); CHECK(rw > 0);
    CHECK(ra == (int)strlen(a));
    CHECK(rw == lstrlenW(w));
}
TEST(input_kbd_keyname_generic_modifier_names_nonempty) {
    static const int sc[] = { 0x2A, 0x36, 0x1D, 0x38, 0x01, 0x0E, 0x0F, 0x1C, 0x39, 0x3A, 0x3B, 0x44, 0x57, 0x58 };
    int i;
    KNREQ;
    for (i = 0; i < NT(sc); i++) {
        char a[64];
        int r = U(GetKeyNameTextA)(sc[i] << 16, a, sizeof a);
        CHECKF(r > 0 && r == (int)strlen(a), "scan %02x ret %d", sc[i], r);
    }
}
TEST(input_kbd_keyname_a_w_parity_all_scancodes) {
    int sc, ext, bad = 0;
    KNREQ;
    for (ext = 0; ext < 2; ext++)
        for (sc = 0; sc < 256; sc++) {
            char a[128], wa[128]; WCHAR w[128];
            char conv[128];
            int ra, rw;
            LONG lp = LP(sc, ext);
            memset(a, 0, sizeof a); memset(w, 0, sizeof w);
            ra = U(GetKeyNameTextA)(lp, a, sizeof a);
            rw = U(GetKeyNameTextW)(lp, w, 128);
            if ((ra == 0) != (rw == 0)) { bad++; continue; }
            if (ra == 0) continue;
            if (rw != lstrlenW(w) || ra != (int)strlen(a)) { bad++; continue; }
            WideCharToMultiByte(CP_ACP, 0, w, -1, conv, sizeof conv, NULL, NULL);
            (void)wa;
            if (strcmp(conv, a)) bad++;
        }
    CHECKF(bad == 0, "%d mismatches", bad);
}
TEST(input_kbd_keyname_len_matches_strlen_all) {
    int i, bad = 0;
    KNREQ;
    for (i = 0; i < 512; i++) {
        char a[128]; WCHAR w[128];
        int ra, rw;
        strcpy(a, "----"); w[0] = 0x2D;
        ra = U(GetKeyNameTextA)(i << 16, a, sizeof a);
        rw = U(GetKeyNameTextW)(i << 16, w, 128);
        if (ra != (int)strlen(a) && ra != 0) bad++;
        if (ra == 0 && a[0] != 0) bad++;    /* A variant clears the buffer on failure */
        if (rw != 0 && rw != lstrlenW(w)) bad++;
    }
    CHECKF(bad == 0, "%d problems", bad);
}
TEST(input_kbd_keyname_a_buffer_zero) {
    char a[8];
    int r;
    KNREQ;
    memset(a, 0xCC, sizeof a);
    r = U(GetKeyNameTextA)(0x1D << 16, a, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(a[0], 0);
}
TEST(input_kbd_keyname_w_buffer_zero) {
    WCHAR w[8];
    int r;
    KNREQ;
    w[0] = 0xCCCC;
    r = U(GetKeyNameTextW)(0x1D << 16, w, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(w[0], 0xCCCC);
}
TEST(input_kbd_keyname_a_truncation_exact_and_minus_one) {
    char full[64], a[64];
    int len, r, n;
    KNREQ;
    len = U(GetKeyNameTextA)(0x1D << 16, full, sizeof full);
    CHECK(len > 0);
    if (len <= 0) return;
    /* exact: room for the name and NUL */
    memset(a, 0xCC, sizeof a);
    r = U(GetKeyNameTextA)(0x1D << 16, a, len + 1);
    EXPECT_EQ(r, len);
    CHECK(!strcmp(a, full));
    /* exact - 1: name truncated by one */
    memset(a, 0xCC, sizeof a);
    r = U(GetKeyNameTextA)(0x1D << 16, a, len);
    EXPECT_EQ(r, len - 1);
    EXPECT_EQ(strlen(a), len - 1);
    CHECK(!strncmp(a, full, len - 1));
    /* every smaller size truncates consistently */
    for (n = 1; n <= len + 1; n++) {
        memset(a, 0xCC, sizeof a);
        r = U(GetKeyNameTextA)(0x1D << 16, a, n);
        CHECKF(r == (n - 1 < len ? n - 1 : len), "cch %d -> %d", n, r);
        CHECKF(strlen(a) == (size_t)r, "cch %d strlen %d", n, (int)strlen(a));
        CHECKF(a[r] == 0, "cch %d not terminated", n);
        CHECKF(!strncmp(a, full, r), "cch %d prefix", n);
    }
}
TEST(input_kbd_keyname_w_truncation_exact_and_minus_one) {
    WCHAR full[64], w[64];
    int len, r, n;
    KNREQ;
    len = U(GetKeyNameTextW)(0x1D << 16, full, 64);
    CHECK(len > 0);
    if (len <= 0) return;
    for (n = 1; n <= len + 1; n++) {
        int i;
        for (i = 0; i < 64; i++) w[i] = 0xCCCC;
        r = U(GetKeyNameTextW)(0x1D << 16, w, n);
        CHECKF(r == (n - 1 < len ? n - 1 : len), "cch %d -> %d", n, r);
        CHECKF(w[r] == 0, "cch %d not terminated", n);
        CHECKF(!memcmp(w, full, r * sizeof(WCHAR)), "cch %d prefix", n);
        CHECKF(w[n] == 0xCCCC || n >= 63, "cch %d overrun", n);
    }
}
TEST(input_kbd_keyname_w_cch_one) {
    WCHAR w[4] = { 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC };
    int r;
    KNREQ;
    r = U(GetKeyNameTextW)(0x1D << 16, w, 1);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(w[1], 0xCCCC);
}
TEST(input_kbd_keyname_a_cch_one) {
    char a[4];
    int r;
    KNREQ;
    memset(a, 0xCC, sizeof a);
    r = U(GetKeyNameTextA)(0x1D << 16, a, 1);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(a[0], 0);
    EXPECT_EQ((unsigned char)a[1], 0xCC);
}
TEST(input_kbd_keyname_unnamed_scancodes_clear_ansi_buffer) {
    int sc, bad = 0, named = 0;
    KNREQ;
    for (sc = 0; sc < 0x200; sc++) {
        char a[64];
        int r;
        memset(a, 'x', sizeof a); a[63] = 0;
        r = U(GetKeyNameTextA)(sc << 16, a, sizeof a);
        if (r == 0) { if (a[0] != 0) bad++; } else named++;
    }
    CHECK(bad == 0);
    CHECK(named > 50);
}
TEST(input_kbd_keyname_high_lparam_bits_ignored_us) {
    char a[64], b[64];
    int r1, r2;
    KNREQ; US_ONLY
    r1 = U(GetKeyNameTextA)(0x1E << 16, a, sizeof a);
    r2 = U(GetKeyNameTextA)((0x1E << 16) | 0xFFFF | 0x80000000, b, sizeof b);
    EXPECT_EQ(r1, r2);
    EXPECT_STR(a, b);
    EXPECT_STR(a, "A");
}
TEST(input_kbd_keyname_repeat_deterministic) {
    char a[64], b[64];
    int i;
    KNREQ;
    U(GetKeyNameTextA)(0x2A << 16, a, sizeof a);
    for (i = 0; i < 200; i++) {
        U(GetKeyNameTextA)(0x2A << 16, b, sizeof b);
        if (strcmp(a, b)) { CHECKF(0, "iteration %d changed", i); return; }
    }
    CHECK(1);
}
TEST(input_kbd_keyname_lasterror_untouched_on_success) {
    char a[64];
    KNREQ;
    SET_ERR(0x4242);
    CHECK(U(GetKeyNameTextA)(0x1D << 16, a, sizeof a) > 0);
    t_note("GetLastError after GetKeyNameTextA success: %lu", GetLastError());
    CHECK(1);
}
