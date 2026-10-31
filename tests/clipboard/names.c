#include "cbcommon.h"
/* RegisterClipboardFormat / GetClipboardFormatName */

#define RA U(RegisterClipboardFormatA)
#define RW U(RegisterClipboardFormatW)
#define GNA U(GetClipboardFormatNameA)
#define GNW U(GetClipboardFormatNameW)

TEST(clipboard_register_a_range) {
    REQUIRE(RA);
    UINT f = RA("T32_names_range_A");
    CHECK(f >= 0xC000 && f <= 0xFFFF);
}
TEST(clipboard_register_w_range) {
    REQUIRE(RW);
    UINT f = RW(L"T32_names_range_W");
    CHECK(f >= 0xC000 && f <= 0xFFFF);
}
TEST(clipboard_register_same_name_same_id) {
    REQUIRE(RA);
    UINT a = RA("T32_names_same"), b = RA("T32_names_same");
    CHECK(a != 0); EXPECT_EQ(a, b);
}
TEST(clipboard_register_case_insensitive) {
    REQUIRE(RA);
    UINT a = RA("T32_Names_Case"), b = RA("t32_names_case"), c = RA("T32_NAMES_CASE");
    CHECK(a != 0); EXPECT_EQ(a, b); EXPECT_EQ(a, c);
}
TEST(clipboard_register_distinct_names_distinct_ids) {
    REQUIRE(RA);
    UINT a = RA("T32_names_d1"), b = RA("T32_names_d2"), c = RA("T32_names_d3");
    CHECK(a != b && b != c && a != c);
}
TEST(clipboard_register_ansi_wide_parity) {
    REQUIRE(RA); REQUIRE(RW);
    UINT a = RA("T32_names_parity"), w = RW(L"T32_names_parity");
    CHECK(a != 0); EXPECT_EQ(a, w);
    UINT w2 = RW(L"T32_NAMES_PARITY"); EXPECT_EQ(a, w2);
}
TEST(clipboard_register_wide_first_then_ansi) {
    REQUIRE(RA); REQUIRE(RW);
    UINT w = RW(L"T32_names_wfirst"), a = RA("T32_names_wfirst");
    CHECK(w != 0); EXPECT_EQ(w, a);
}
TEST(clipboard_register_null_a) {
    REQUIRE(RA);
    SET_ERR(0); CHECK(RA(NULL) == 0); t_note("Register(NULL) err=%lu", GetLastError());
}
TEST(clipboard_register_null_w) {
    REQUIRE(RW);
    SET_ERR(0); CHECK(RW(NULL) == 0); t_note("RegisterW(NULL) err=%lu", GetLastError());
}
TEST(clipboard_register_empty_a) {
    REQUIRE(RA);
    SET_ERR(0); CHECK(RA("") == 0); t_note("Register(\"\") err=%lu", GetLastError());
}
TEST(clipboard_register_empty_w) {
    REQUIRE(RW);
    SET_ERR(0); CHECK(RW(L"") == 0); t_note("RegisterW(\"\") err=%lu", GetLastError());
}
TEST(clipboard_register_length_ladder) {
    REQUIRE(RA);
    char b[600];
    int lens[] = { 1, 2, 10, 63, 64, 100, 200 };
    for (int i = 0; i < 7; i++) {
        memset(b, 'k', lens[i]); b[lens[i]] = 0;
        b[0] = 'L'; if (lens[i] > 2) { b[1] = '0' + lens[i] / 100 % 10; b[2] = '0' + lens[i] / 10 % 10; }
        UINT f = RA(b), g = RA(b);
        CHECKF(f >= 0xC000 && f == g, "len %d -> %x/%x", lens[i], f, g);
    }
}
TEST(clipboard_register_long_names_observed) {
    REQUIRE(RA);
    char b[600];
    int lens[] = { 255, 256, 257, 300, 500 };
    for (int i = 0; i < 5; i++) {
        memset(b, 'm', lens[i]); b[lens[i]] = 0;
        SET_ERR(0);
        UINT f = RA(b);
        t_note("RegisterClipboardFormatA(len %d) -> 0x%x err=%lu", lens[i], f, GetLastError());
    }
}
TEST(clipboard_register_stable_over_many_calls) {
    REQUIRE(RA);
    UINT f = RA("T32_names_stable");
    int bad = 0;
    for (int i = 0; i < 2000; i++) if (RA("T32_names_stable") != f) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(clipboard_register_many_distinct) {
    REQUIRE(RA);
    char b[64]; UINT first = 0; int ok = 0;
    for (int i = 0; i < 200; i++) {
        sprintf(b, "T32_names_many_%d", i);
        UINT f = RA(b);
        if (f >= 0xC000 && f <= 0xFFFF) ok++;
        if (!first) first = f;
        if (RA(b) != f) ok--;
    }
    EXPECT_EQ(ok, 200);
}
TEST(clipboard_register_special_chars) {
    REQUIRE(RA); REQUIRE(GNA);
    const char *n[] = { "T32 names with spaces", "T32.names/slash\\back", "T32_names_!@#$%", "T32_names_tab\there" };
    for (int i = 0; i < 4; i++) {
        UINT f = RA(n[i]);
        char out[128] = {0};
        int r = GNA(f, out, sizeof out);
        CHECKF(f != 0, "%s", n[i]);
        CHECKF(r == (int)strlen(n[i]) && !strcmp(out, n[i]), "%s -> %s", n[i], out);
    }
}
TEST(clipboard_register_same_as_window_message) {
    REQUIRE(RA); REQUIRE(U(RegisterWindowMessageA));
    UINT a = RA("T32_names_wm_shared"), m = U(RegisterWindowMessageA)("T32_names_wm_shared");
    EXPECT_EQ(a, m);
}
TEST(clipboard_register_unicode_name_roundtrip) {
    REQUIRE(RW); REQUIRE(GNW);
    WCHAR n[] = { 'T','3','2','_', 0x00E9, 0x03A9, 0x4E2D, 0 };
    UINT f = RW(n);
    CHECK(f >= 0xC000);
    WCHAR o[32] = {0};
    int r = GNW(f, o, 32);
    CHECK(r == 7);
    CHECK(!lstrcmpW(o, n));
    UINT g = RW(n); EXPECT_EQ(f, g);
}

/* GetClipboardFormatName */
TEST(clipboard_name_a_roundtrip) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_Names_Round");
    char b[64]; memset(b, 'z', sizeof b);
    int r = GNA(f, b, sizeof b);
    EXPECT_EQ(r, 15); EXPECT_STR(b, "T32_Names_Round");
}
TEST(clipboard_name_w_roundtrip) {
    REQUIRE(RW); REQUIRE(GNW);
    UINT f = RW(L"T32_Names_RoundW");
    WCHAR b[64]; memset(b, 0x7a, sizeof b);
    int r = GNW(f, b, 64);
    EXPECT_EQ(r, 16); EXPECT_WSTR(b, L"T32_Names_RoundW");
}
TEST(clipboard_name_preserves_original_case) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_CaSe_Keep");
    UINT g = RA("t32_case_keep");
    char b[64];
    GNA(g, b, 64); EXPECT_STR(b, "T32_CaSe_Keep");
    EXPECT_EQ(f, g);
}
TEST(clipboard_name_buffer_exact) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_Names_Exact"); /* 15 chars */
    char b[32]; memset(b, 'q', sizeof b);
    int r = GNA(f, b, 16);
    EXPECT_EQ(r, 15); EXPECT_STR(b, "T32_Names_Exact"); CHECK(b[16] == 'q');
}
TEST(clipboard_name_buffer_exact_minus_one) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_Names_Exact");
    char b[32]; memset(b, 'q', sizeof b);
    int r = GNA(f, b, 15);
    t_note("cch=15 for 15 char name -> %d", r); CHECK(b[15] == 'q');
}
TEST(clipboard_name_buffer_one) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_Names_Exact");
    char b[4]; memset(b, 'q', sizeof b);
    int r = GNA(f, b, 1);
    EXPECT_EQ(r, 0); CHECK(b[0] == 0); CHECK(b[1] == 'q');
}
TEST(clipboard_name_buffer_two) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_Names_Exact");
    char b[4]; memset(b, 'q', sizeof b);
    int r = GNA(f, b, 2);
    t_note("cch=2 -> %d [%d,%d]", r, b[0], b[1]);
}
TEST(clipboard_name_buffer_zero) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_Names_Exact");
    char b[4]; memset(b, 'q', sizeof b);
    SET_ERR(0);
    int r = GNA(f, b, 0);
    EXPECT_EQ(r, 0);
    CHECK(b[0] == 'q');
    t_note("GetClipboardFormatNameA(cch 0) err=%lu", GetLastError());
}
TEST(clipboard_name_buffer_negative) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_Names_Exact");
    char b[4]; memset(b, 'q', sizeof b);
    SET_ERR(0);
    int r = GNA(f, b, -1);
    CHECK(r == 0);
    t_note("GetClipboardFormatNameA(cch -1) err=%lu b0=%d", GetLastError(), b[0]);
}
TEST(clipboard_name_w_buffer_edges) {
    REQUIRE(RW); REQUIRE(GNW);
    UINT f = RW(L"T32_Names_ExactW"); /* 16 */
    WCHAR b[40];
    for (int i = 0; i < 40; i++) b[i] = 0x7171;
    EXPECT_EQ(GNW(f, b, 17), 16); EXPECT_WSTR(b, L"T32_Names_ExactW");
    for (int i = 0; i < 40; i++) b[i] = 0x7171;
    t_note("W cch=16 -> %d", GNW(f, b, 16)); CHECK(b[16] == 0x7171);
    for (int i = 0; i < 40; i++) b[i] = 0x7171;
    t_note("W cch=1 -> %d", GNW(f, b, 1)); CHECK(b[1] == 0x7171);
    for (int i = 0; i < 40; i++) b[i] = 0x7171;
    t_note("W cch=2 -> %d", GNW(f, b, 2));
}
TEST(clipboard_name_predefined_formats_fail) {
    REQUIRE(GNA); REQUIRE(GNW);
    UINT fmts[] = { CF_TEXT, CF_BITMAP, CF_METAFILEPICT, CF_UNICODETEXT, CF_ENHMETAFILE, CF_HDROP, CF_LOCALE, CF_DIB, CF_DIBV5, CF_OEMTEXT };
    for (int i = 0; i < 10; i++) {
        char b[64]; memset(b, 'x', sizeof b);
        SET_ERR(0);
        int r = GNA(fmts[i], b, 64);
        CHECKF(r == 0, "fmt %u r=%d", fmts[i], r);
        WCHAR w[64]; SET_ERR(0);
        CHECKF(GNW(fmts[i], w, 64) == 0, "fmt %u", fmts[i]);
    }
}
TEST(clipboard_name_predefined_error_observed) {
    REQUIRE(GNA);
    char b[64]; SET_ERR(0); GNA(CF_TEXT, b, 64);
    t_note("GetClipboardFormatNameA(CF_TEXT) err=%lu", GetLastError());
}
TEST(clipboard_name_unregistered_ids) {
    REQUIRE(GNA); REQUIRE(RA);
    char b[64];
    UINT bad[] = { 0, 0xFFFF, 0xFFFFFFFF, 0x10000, 0x1, 0xBFFF };
    for (int i = 0; i < 6; i++) { SET_ERR(0); CHECKF(GNA(bad[i], b, 64) == 0, "fmt %x", bad[i]); }
}
TEST(clipboard_name_a_null_buffer) {
    REQUIRE(GNA);
    SET_ERR(0);
    int r = GNA(0xC000, NULL, 0);
    CHECK(r == 0);
}
TEST(clipboard_name_private_range_fail) {
    REQUIRE(GNA);
    char b[16];
    CHECK(GNA(CF_PRIVATEFIRST, b, 16) == 0);
    CHECK(GNA(CF_GDIOBJFIRST, b, 16) == 0);
}
TEST(clipboard_name_ansi_wide_parity) {
    REQUIRE(RA); REQUIRE(GNA); REQUIRE(GNW);
    UINT f = RA("T32_Names_AWpar");
    char a[64]; WCHAR w[64];
    int ra = GNA(f, a, 64), rw = GNW(f, w, 64);
    EXPECT_EQ(ra, rw);
    int ok = 1; for (int i = 0; i <= ra; i++) if ((WCHAR)(unsigned char)a[i] != w[i]) ok = 0;
    CHECK(ok);
}
TEST(clipboard_name_repeat) {
    REQUIRE(RA); REQUIRE(GNA);
    UINT f = RA("T32_Names_Repeat");
    char b[64]; int bad = 0;
    for (int i = 0; i < 1000; i++) { memset(b, 0, sizeof b); if (GNA(f, b, 64) != 16 || strcmp(b, "T32_Names_Repeat")) bad++; }
    EXPECT_EQ(bad, 0);
}
TEST(clipboard_name_after_clipboard_activity) {
    CB_REQ_BASIC; REQUIRE(RA); REQUIRE(GNA); cb_reset();
    UINT f = RA("T32_Names_Activity");
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(f, cb_text("a")); CLOSE();
    char b[64]; EXPECT_EQ(GNA(f, b, 64), 18);
    cb_reset(); cb_destroy(w);
    EXPECT_EQ(GNA(f, b, 64), 18); /* registration survives empty */
}
