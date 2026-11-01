/* CharUpper/CharLower/CharNext/CharPrev/IsChar* conformance. */
#include "t.h"

#define W(s) L##s

static int acp(void) { return (int)GetACP(); }

/* ---------- CharUpperA / CharLowerA single-character form ---------- */

static char up1(LPSTR (WINAPI *f)(LPSTR), unsigned c) { return (char)(ULONG_PTR)f((LPSTR)(ULONG_PTR)c); }

TEST(chars_upperA_single_ascii) {
    REQUIRE(U(CharUpperA));
    for (unsigned c = 'a'; c <= 'z'; c++) {
        ULONG_PTR r = (ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)c);
        CHECKF(r == c - 32, "CharUpperA(%c) = %llx", c, (unsigned long long)r);
    }
    for (unsigned c = 'A'; c <= 'Z'; c++) CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)c) == c);
    for (unsigned c = '0'; c <= '9'; c++) CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)c) == c);
    for (unsigned c = 1; c < 'A'; c++) CHECKF((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)c) == c, "c=%u", c);
    for (unsigned c = '['; c < 'a'; c++) CHECKF((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)c) == c, "c=%u", c);
    for (unsigned c = '{'; c < 0x80; c++) CHECKF((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)c) == c, "c=%u", c);
}

TEST(chars_lowerA_single_ascii) {
    REQUIRE(U(CharLowerA));
    for (unsigned c = 'A'; c <= 'Z'; c++) {
        ULONG_PTR r = (ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)c);
        CHECKF(r == c + 32, "CharLowerA(%c) = %llx", c, (unsigned long long)r);
    }
    for (unsigned c = 'a'; c <= 'z'; c++) CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)c) == c);
    for (unsigned c = '0'; c <= '9'; c++) CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)c) == c);
    for (unsigned c = 1; c < 'A'; c++) CHECKF((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)c) == c, "c=%u", c);
    for (unsigned c = '['; c < 'a'; c++) CHECKF((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)c) == c, "c=%u", c);
    for (unsigned c = '{'; c < 0x80; c++) CHECKF((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)c) == c, "c=%u", c);
}

TEST(chars_upperA_single_result_is_16bit) {
    REQUIRE(U(CharUpperA));
    for (INT_PTR i = 0; i < 256; i++) {
        INT_PTR out = (INT_PTR)U(CharUpperA)((LPSTR)i);
        CHECKF((out >> 16) == 0, "CharUpperA(0x%Ix) -> 0x%Ix", i, out);
        CHECKF(out < 256, "CharUpperA(0x%Ix) -> 0x%Ix not a byte", i, out);
    }
}

TEST(chars_lowerA_single_result_is_16bit) {
    REQUIRE(U(CharLowerA));
    for (INT_PTR i = 0; i < 256; i++) {
        INT_PTR out = (INT_PTR)U(CharLowerA)((LPSTR)i);
        CHECKF((out >> 16) == 0, "CharLowerA(0x%Ix) -> 0x%Ix", i, out);
        CHECKF(out < 256, "CharLowerA(0x%Ix) -> 0x%Ix", i, out);
    }
}

TEST(chars_upperA_single_zero) {
    REQUIRE(U(CharUpperA));
    CHECK(U(CharUpperA)(NULL) == NULL);
    CHECK(U(CharLowerA) == NULL || U(CharLowerA)(NULL) == NULL);
}

TEST(chars_upperA_single_highword_ignored_in_char) {
    /* a value with a non-zero HIWORD is a pointer; only 0..0xFFFF is a character. 0xFFFF is the largest character form. */
    REQUIRE(U(CharUpperA));
    ULONG_PTR r = (ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)'q');
    CHECK(r == 'Q');
    r = (ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0x0161);   /* low byte 'a' */
    t_note("CharUpperA(0x0161)=0x%llx (only low byte is a character in the A form)", (unsigned long long)r);
    CHECK((r >> 16) == 0);
}

TEST(chars_cp1252_latin1_case) {
    REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerA));
    if (acp() != 1252) { t_skip("ACP is %d, not 1252", acp()); return; }
    for (unsigned c = 0xE0; c <= 0xFE; c++) {
        if (c == 0xF7) { CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)c) == c); continue; }
        CHECKF((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)c) == c - 0x20, "upper 0x%x", c);
    }
    for (unsigned c = 0xC0; c <= 0xDE; c++) {
        if (c == 0xD7) { CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)c) == c); continue; }
        CHECKF((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)c) == c + 0x20, "lower 0x%x", c);
    }
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0xFF) == 0x9F);   /* y diaeresis -> Y diaeresis */
    CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)0x9F) == 0xFF);
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0x9C) == 0x8C);   /* oe -> OE */
    CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)0x8C) == 0x9C);
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0x9A) == 0x8A);   /* s caron */
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0x9E) == 0x8E);   /* z caron */
}

/* ---------- CharUpperA / CharLowerA string form ---------- */

TEST(chars_upperA_string) {
    REQUIRE(U(CharUpperA));
    char b[64];
    strcpy(b, "hello, World 123 _xyz");
    LPSTR r = U(CharUpperA)(b);
    CHECK(r == b);
    EXPECT_STR(b, "HELLO, WORLD 123 _XYZ");
    strcpy(b, "");
    CHECK(U(CharUpperA)(b) == b);
    EXPECT_STR(b, "");
    strcpy(b, "a");
    CHECK(U(CharUpperA)(b) == b);
    EXPECT_STR(b, "A");
    strcpy(b, "ABC");
    CHECK(U(CharUpperA)(b) == b);
    EXPECT_STR(b, "ABC");
}

TEST(chars_lowerA_string) {
    REQUIRE(U(CharLowerA));
    char b[64];
    strcpy(b, "HELLO, World 123 _XYZ");
    LPSTR r = U(CharLowerA)(b);
    CHECK(r == b);
    EXPECT_STR(b, "hello, world 123 _xyz");
    strcpy(b, "");
    CHECK(U(CharLowerA)(b) == b);
    EXPECT_STR(b, "");
    strcpy(b, "z");
    CHECK(U(CharLowerA)(b) == b);
    EXPECT_STR(b, "z");
}

TEST(chars_string_form_does_not_touch_past_nul) {
    REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerA));
    char b[16];
    memcpy(b, "ab\0cd\0\0\0\0\0\0\0\0\0\0\0", 16);
    U(CharUpperA)(b);
    CHECK(!memcmp(b, "AB\0cd", 5));
    memcpy(b, "AB\0CD\0\0\0\0\0\0\0\0\0\0\0", 16);
    U(CharLowerA)(b);
    CHECK(!memcmp(b, "ab\0CD", 5));
}

TEST(chars_upperA_string_all_ascii_bytes) {
    REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerA));
    char b[130], c[130];
    for (int i = 1; i < 128; i++) b[i - 1] = (char)i;
    b[127] = 0;
    memcpy(c, b, 128);
    U(CharUpperA)(b);
    for (int i = 1; i < 128; i++) {
        int want = (i >= 'a' && i <= 'z') ? i - 32 : i;
        CHECKF((unsigned char)b[i - 1] == want, "upper byte %d -> %d", i, (unsigned char)b[i - 1]);
    }
    U(CharLowerA)(c);
    for (int i = 1; i < 128; i++) {
        int want = (i >= 'A' && i <= 'Z') ? i + 32 : i;
        CHECKF((unsigned char)c[i - 1] == want, "lower byte %d -> %d", i, (unsigned char)c[i - 1]);
    }
}

/* ---------- W forms ---------- */

TEST(chars_upperW_single_ascii) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    for (unsigned c = 0; c < 0x80; c++) {
        unsigned wantU = (c >= 'a' && c <= 'z') ? c - 32 : c;
        unsigned wantL = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        CHECKF((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == wantU, "upper %u", c);
        CHECKF((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c) == wantL, "lower %u", c);
    }
}

TEST(chars_upperW_single_result_16bit_all_chars) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    unsigned bad = 0;
    for (unsigned c = 0; c < 0x10000; c++) {
        ULONG_PTR u = (ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c), l = (ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c);
        if (u > 0xFFFF || l > 0xFFFF) bad++;
    }
    CHECK(bad == 0);
}

TEST(chars_upperW_lower_idempotent_all_chars) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    unsigned badu = 0, badl = 0;
    for (unsigned c = 0; c < 0x10000; c++) {
        ULONG_PTR u = (ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c);
        ULONG_PTR l = (ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c);
        if ((ULONG_PTR)U(CharUpperW)((LPWSTR)u) != u) badu++;
        if ((ULONG_PTR)U(CharLowerW)((LPWSTR)l) != l) badl++;
    }
    t_note("non-idempotent upper=%u lower=%u", badu, badl);
    CHECK(badu < 20);
    CHECK(badl < 20);
}

TEST(chars_upperW_latin1_supplement) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    for (unsigned c = 0xE0; c <= 0xFE; c++) {
        if (c == 0xF7) { CHECK((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == c); continue; }
        CHECKF((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == c - 0x20, "upper U+%04X", c);
        CHECKF((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)(c - 0x20)) == c, "lower U+%04X", c - 0x20);
    }
    CHECK((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)0xFF) == 0x178);
    CHECK((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)0x178) == 0xFF);
    CHECK((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)0xD7) == 0xD7);
    CHECK((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)0xD7) == 0xD7);
    CHECK((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)0xB5) == 0x39C || (ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)0xB5) == 0xB5);
    t_note("CharUpperW(U+00DF)=U+%04llX CharUpperW(U+00B5)=U+%04llX", (unsigned long long)(ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)0xDF),
           (unsigned long long)(ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)0xB5));
}

TEST(chars_upperW_greek_cyrillic) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    for (unsigned c = 0x3B1; c <= 0x3C9; c++) {
        if (c == 0x3C2) continue;   /* final sigma */
        CHECKF((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == c - 0x20, "greek upper U+%04X", c);
        CHECKF((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)(c - 0x20)) == c, "greek lower U+%04X", c - 0x20);
    }
    for (unsigned c = 0x430; c <= 0x44F; c++) {
        CHECKF((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == c - 0x20, "cyr upper U+%04X", c);
        CHECKF((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)(c - 0x20)) == c, "cyr lower U+%04X", c - 0x20);
    }
    for (unsigned c = 0x450; c <= 0x45F; c++) CHECKF((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == c - 0x50, "cyr ext upper U+%04X", c);
    t_note("CharUpperW(U+03C2 final sigma)=U+%04llX", (unsigned long long)(ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)0x3C2));
}

TEST(chars_upperW_latin_extended_a_pairs) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    /* U+0100..U+017F alternates upper(even)/lower(odd) for 0100-0137 and 014A-0177 */
    for (unsigned c = 0x100; c < 0x138; c += 2) {
        if (c == 0x130) continue;
        CHECKF((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c) == c + 1, "lower U+%04X", c);
        CHECKF((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)(c + 1)) == c || c == 0x130, "upper U+%04X", c + 1);
    }
    for (unsigned c = 0x14A; c < 0x178; c += 2) {
        CHECKF((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c) == c + 1, "lower U+%04X", c);
        CHECKF((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)(c + 1)) == c, "upper U+%04X", c + 1);
    }
}

TEST(chars_upperW_fullwidth_and_math_unchanged) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    for (unsigned c = 0xFF41; c <= 0xFF5A; c++) CHECKF((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == c - 0x20, "fullwidth upper U+%04X", c);
    for (unsigned c = 0xFF21; c <= 0xFF3A; c++) CHECKF((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c) == c + 0x20, "fullwidth lower U+%04X", c);
    for (unsigned c = 0x4E00; c < 0x4E40; c++) { CHECK((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == c); CHECK((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c) == c); }
    for (unsigned c = 0x2200; c < 0x2220; c++) CHECK((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) == c);
}

TEST(chars_upperW_string) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    WCHAR b[64];
    lstrcpyW(b, L"hello wörld αβγ да 123");
    LPWSTR r = U(CharUpperW)(b);
    CHECK(r == b);
    EXPECT_WSTR(b, L"HELLO WÖRLD ΑΒΓ ДА 123");
    r = U(CharLowerW)(b);
    CHECK(r == b);
    EXPECT_WSTR(b, L"hello wörld αβγ да 123");
    b[0] = 0;
    CHECK(U(CharUpperW)(b) == b);
    CHECK(U(CharLowerW)(b) == b);
    CHECK(b[0] == 0);
}

TEST(chars_upperW_string_surrogates_unchanged) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    WCHAR b[8] = { 0xD801, 0xDC28, 'a', 0 };   /* U+10428 DESERET SMALL LETTER; surrogates are not touched, 'a' is */
    U(CharUpperW)(b);
    CHECK(b[0] == 0xD801);
    CHECK(b[1] == 0xDC28 || b[1] == 0xDC00);
    CHECK(b[2] == 'A');
    WCHAR c[4] = { 0xD801, 0xDC00, 'B', 0 };
    U(CharLowerW)(c);
    CHECK(c[0] == 0xD801);
    CHECK(c[2] == 'b');
}

TEST(chars_upperW_string_stops_at_nul) {
    REQUIRE(U(CharUpperW));
    WCHAR b[8] = { 'a', 'b', 0, 'c', 'd', 0, 0, 0 };
    U(CharUpperW)(b);
    CHECK(b[0] == 'A' && b[1] == 'B' && b[2] == 0 && b[3] == 'c' && b[4] == 'd');
}

TEST(chars_AW_parity_ascii_strings) {
    REQUIRE(U(CharUpperA)); REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerA)); REQUIRE(U(CharLowerW));
    char a[128]; WCHAR w[128];
    for (int i = 1; i < 128; i++) { a[i - 1] = (char)i; w[i - 1] = (WCHAR)i; }
    a[127] = 0; w[127] = 0;
    U(CharUpperA)(a); U(CharUpperW)(w);
    for (int i = 0; i < 127; i++) CHECKF((unsigned char)a[i] == w[i], "upper idx %d: %d vs %d", i, (unsigned char)a[i], w[i]);
    for (int i = 1; i < 128; i++) { a[i - 1] = (char)i; w[i - 1] = (WCHAR)i; }
    U(CharLowerA)(a); U(CharLowerW)(w);
    for (int i = 0; i < 127; i++) CHECKF((unsigned char)a[i] == w[i], "lower idx %d: %d vs %d", i, (unsigned char)a[i], w[i]);
}

/* ---------- CharUpperBuff / CharLowerBuff ---------- */

TEST(chars_buffA_basic) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA));
    char b[32];
    strcpy(b, "abcdef");
    CHECK(U(CharUpperBuffA)(b, 3) == 3);
    EXPECT_STR(b, "ABCdef");
    CHECK(U(CharUpperBuffA)(b, 6) == 6);
    EXPECT_STR(b, "ABCDEF");
    CHECK(U(CharLowerBuffA)(b, 2) == 2);
    EXPECT_STR(b, "abCDEF");
    CHECK(U(CharLowerBuffA)(b, 6) == 6);
    EXPECT_STR(b, "abcdef");
}

TEST(chars_buffA_zero_length) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA));
    char b[8] = "abc";
    CHECK(U(CharUpperBuffA)(b, 0) == 0);
    EXPECT_STR(b, "abc");
    strcpy(b, "ABC");
    CHECK(U(CharLowerBuffA)(b, 0) == 0);
    EXPECT_STR(b, "ABC");
}

TEST(chars_buffA_one_char) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA));
    char b[8] = "zz";
    CHECK(U(CharUpperBuffA)(b, 1) == 1);
    EXPECT_STR(b, "Zz");
    CHECK(U(CharLowerBuffA)(b, 1) == 1);
    EXPECT_STR(b, "zz");
}

TEST(chars_buffA_ignores_nul_terminator) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA));
    char b[8];
    memcpy(b, "ab\0cd\0\0\0", 8);
    CHECK(U(CharUpperBuffA)(b, 5) == 5);
    CHECK(!memcmp(b, "AB\0CD", 5));
    CHECK(U(CharLowerBuffA)(b, 5) == 5);
    CHECK(!memcmp(b, "ab\0cd", 5));
}

TEST(chars_buffA_does_not_write_past_length) {
    REQUIRE(U(CharUpperBuffA));
    char b[16]; memset(b, 'q', sizeof b);
    CHECK(U(CharUpperBuffA)(b, 7) == 7);
    for (int i = 0; i < 7; i++) CHECK(b[i] == 'Q');
    for (int i = 7; i < 16; i++) CHECK(b[i] == 'q');
}

TEST(chars_buffW_basic) {
    REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharLowerBuffW));
    WCHAR b[32];
    lstrcpyW(b, L"abcéαд");
    CHECK(U(CharUpperBuffW)(b, 6) == 6);
    EXPECT_WSTR(b, L"ABCÉΑД");
    CHECK(U(CharLowerBuffW)(b, 3) == 3);
    EXPECT_WSTR(b, L"abcÉΑД");
    CHECK(U(CharLowerBuffW)(b, 0) == 0);
    EXPECT_WSTR(b, L"abcÉΑД");
}

TEST(chars_buffW_ignores_nul_and_bounds) {
    REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharLowerBuffW));
    WCHAR b[10] = { 'a', 0, 'b', 'c', 'x', 'x', 'x', 'x', 'x', 'x' };
    CHECK(U(CharUpperBuffW)(b, 4) == 4);
    CHECK(b[0] == 'A' && b[1] == 0 && b[2] == 'B' && b[3] == 'C' && b[4] == 'x');
    CHECK(U(CharLowerBuffW)(b, 4) == 4);
    CHECK(b[0] == 'a' && b[2] == 'b' && b[3] == 'c' && b[4] == 'x');
}

TEST(chars_buffW_matches_single_form_all_chars) {
    REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharLowerBuffW)); REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    WCHAR *u = (WCHAR *)malloc(0x10000 * sizeof(WCHAR)), *l = (WCHAR *)malloc(0x10000 * sizeof(WCHAR));
    REQUIRE(u && l);
    for (unsigned c = 0; c < 0x10000; c++) u[c] = l[c] = (WCHAR)c;
    CHECK(U(CharUpperBuffW)(u, 0x10000) == 0x10000);
    CHECK(U(CharLowerBuffW)(l, 0x10000) == 0x10000);
    unsigned du = 0, dl = 0;
    for (unsigned c = 0; c < 0x10000; c++) {
        if ((ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c) != u[c]) du++;
        if ((ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c) != l[c]) dl++;
    }
    t_note("buff vs single differences: upper=%u lower=%u", du, dl);
    CHECK(du < 50 && dl < 50);
    free(u); free(l);
}

TEST(chars_buffA_matches_string_form) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA)); REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerA));
    char a[256], b[256];
    for (int i = 0; i < 255; i++) a[i] = b[i] = (char)(i + 1);
    a[255] = b[255] = 0;
    U(CharUpperBuffA)(a, 255); U(CharUpperA)(b);
    CHECK(!memcmp(a, b, 256));
    for (int i = 0; i < 255; i++) a[i] = b[i] = (char)(i + 1);
    U(CharLowerBuffA)(a, 255); U(CharLowerA)(b);
    CHECK(!memcmp(a, b, 256));
}

/* ---------- CharNext / CharPrev ---------- */

TEST(chars_nextA_basic) {
    REQUIRE(U(CharNextA));
    static const char s[] = "abc";
    CHECK(U(CharNextA)(s) == s + 1);
    CHECK(U(CharNextA)(s + 1) == s + 2);
    CHECK(U(CharNextA)(s + 2) == s + 3);
    CHECK(U(CharNextA)(s + 3) == s + 3);   /* at NUL it does not advance */
}

TEST(chars_nextA_empty_and_single) {
    REQUIRE(U(CharNextA));
    static const char e[] = "";
    CHECK(U(CharNextA)(e) == e);
    static const char o[] = "x";
    CHECK(U(CharNextA)(o) == o + 1);
    CHECK(U(CharNextA)(o + 1) == o + 1);
}

TEST(chars_nextA_walk_whole_string) {
    REQUIRE(U(CharNextA));
    static const char s[] = "The quick brown fox jumps over the lazy dog 0123456789";
    const char *p = s; int n = 0;
    while (*p && n < 1000) { const char *q = U(CharNextA)(p); CHECK(q == p + 1); p = q; n++; }
    CHECK(n == (int)strlen(s));
    CHECK(*p == 0);
}

TEST(chars_nextW_basic) {
    REQUIRE(U(CharNextW));
    static const WCHAR s[] = L"abé中";
    CHECK(U(CharNextW)(s) == s + 1);
    CHECK(U(CharNextW)(s + 1) == s + 2);
    CHECK(U(CharNextW)(s + 2) == s + 3);
    CHECK(U(CharNextW)(s + 3) == s + 4);
    CHECK(U(CharNextW)(s + 4) == s + 4);
}

TEST(chars_nextW_empty) {
    REQUIRE(U(CharNextW));
    static const WCHAR e[] = L"";
    CHECK(U(CharNextW)(e) == e);
}

TEST(chars_nextW_combining_marks_note) {
    REQUIRE(U(CharNextW));
    static const WCHAR s[] = { 'e', 0x0301, 'x', 0 };
    const WCHAR *n = U(CharNextW)(s);
    t_note("CharNextW(e + U+0301) advances %d code unit(s)", (int)(n - s));
    CHECK(n == s + 1 || n == s + 2);
    static const WCHAR t[] = { 0xD801, 0xDC00, 'x', 0 };
    n = U(CharNextW)(t);
    t_note("CharNextW(surrogate pair) advances %d code unit(s)", (int)(n - t));
    CHECK(n == t + 1 || n == t + 2);
}

TEST(chars_prevA_basic) {
    REQUIRE(U(CharPrevA));
    static const char s[] = "foobar\0r";
    CHECK(U(CharPrevA)(s, s + 5) == s + 4);
    CHECK(U(CharPrevA)(s, s + 6) == s + 5);   /* at terminator */
    CHECK(U(CharPrevA)(s, s + 7) == s + 6);   /* beyond terminator: no NUL stop */
    CHECK(U(CharPrevA)(s, s) == s);
    CHECK(U(CharPrevA)(s, s + 1) == s);
    t_note("CharPrevA(start>current) -> %s", U(CharPrevA)(s + 1, s) == s ? "start" : "other");
}

TEST(chars_prevA_sweep) {
    REQUIRE(U(CharPrevA));
    static const char s[] = "0123456789abcdef";
    for (int i = 0; i <= 16; i++) {
        const char *p = U(CharPrevA)(s, s + i);
        CHECKF(p == (i ? s + i - 1 : s), "i=%d", i);
    }
}

TEST(chars_prevW_basic) {
    REQUIRE(U(CharPrevW));
    static const WCHAR s[] = L"foobar\0r";
    CHECK(U(CharPrevW)(s, s + 5) == s + 4);
    CHECK(U(CharPrevW)(s, s + 6) == s + 5);
    CHECK(U(CharPrevW)(s, s + 7) == s + 6);
    CHECK(U(CharPrevW)(s, s) == s);
    CHECK(U(CharPrevW)(s, s + 1) == s);
    CHECK(U(CharPrevW)(s + 1, s) == s);
}

TEST(chars_prevW_sweep) {
    REQUIRE(U(CharPrevW));
    static const WCHAR s[] = L"0123456789éα中xyz";
    int n = lstrlenW(s);
    for (int i = 0; i <= n; i++) {
        const WCHAR *p = U(CharPrevW)(s, s + i);
        CHECKF(p == (i ? s + i - 1 : s), "i=%d", i);
    }
}

TEST(chars_nextExA_sbcs) {
    REQUIRE(U(CharNextExA));
    static const char s[] = "abc";
    CHECK(U(CharNextExA)(CP_ACP, s, 0) == s + 1);
    CHECK(U(CharNextExA)(CP_ACP, s + 2, 0) == s + 3);
    CHECK(U(CharNextExA)(CP_ACP, s + 3, 0) == s + 3);
    CHECK(U(CharNextExA)(1252, s, 0) == s + 1);
    CHECK(U(CharNextExA)(437, s, 0) == s + 1);
    CHECK(U(CharNextExA)(CP_OEMCP, s, 0) == s + 1);
    static const char hi[] = "\xe9\xe8";
    CHECK(U(CharNextExA)(1252, hi, 0) == hi + 1);
    CHECK(U(CharNextExA)(437, hi, 0) == hi + 1);
}

TEST(chars_nextExA_dbcs_936) {
    REQUIRE(U(CharNextExA));
    static const char s[] = { (char)0xa1, 'A', (char)0xa1, 'B', 'z', 0 };
    const char *r0 = U(CharNextExA)(936, s, 0);
    t_note("CharNextExA(936, A1 41) advances %d", (int)(r0 - s));
    CHECK(r0 == s + 2);
    CHECK(U(CharNextExA)(936, s + 2, 0) == s + 4);
    CHECK(U(CharNextExA)(936, s + 4, 0) == s + 5);
    static const char tail[] = { (char)0xa1, 0, 'x' };   /* lead byte followed by NUL: do not skip the terminator */
    const char *r1 = U(CharNextExA)(936, tail, 0);
    CHECK(r1 == tail + 1);
}

TEST(chars_prevExA_dbcs_table) {
    REQUIRE(U(CharPrevExA));
    static const char d1[] = { (char)0xa1, 'A', (char)0xa1, 'B', (char)0xa1, 0, 'C' };
    static const struct { int cur, want; } t[] = { {1, 0}, {2, 0}, {3, 2}, {6, 4} };
    for (unsigned i = 0; i < 3; i++) {
        const char *p = U(CharPrevExA)(936, d1, d1 + t[i].cur, 0);
        CHECKF(p == d1 + t[i].want, "cur=%d got %d want %d", t[i].cur, (int)(p - d1), t[i].want);
    }
    t_note("CharPrevExA(936, lead+NUL, cur=6) -> offset %d", (int)(U(CharPrevExA)(936, d1, d1 + 6, 0) - d1));
}

TEST(chars_prevExA_sbcs) {
    REQUIRE(U(CharPrevExA));
    static const char s[] = "foobar\0r";
    static const int cps[] = { 0, 1252, 437, 850, 1251, 28591 };
    for (unsigned k = 0; k < sizeof cps / sizeof *cps; k++) {
        CHECK(U(CharPrevExA)(cps[k], s, s + 5, 0) == s + 4);
        CHECK(U(CharPrevExA)(cps[k], s, s + 6, 0) == s + 5);
        CHECK(U(CharPrevExA)(cps[k], s, s + 7, 0) == s + 6);
        CHECK(U(CharPrevExA)(cps[k], s, s, 0) == s);
        CHECK(U(CharPrevExA)(cps[k], s, s + 1, 0) == s);
        t_note("CharPrevExA cp=%d start>current %s", cps[k], U(CharPrevExA)(cps[k], s + 1, s, 0) == s ? "start" : "other");
    }
}

TEST(chars_nextExA_null_current) {
    REQUIRE(U(CharNextExA)); REQUIRE(U(CharPrevExA));
    static const char s[] = "foo";
    t_note("CharPrevExA(current NULL) == NULL: %d", U(CharPrevExA)(CP_ACP, s, NULL, 0) == NULL);
}

/* ---------- IsChar* ---------- */

#define IA(fn, c) (U(fn)((c)) != 0)

TEST(chars_isalphaA_ascii) {
    REQUIRE(U(IsCharAlphaA)); REQUIRE(U(IsCharAlphaNumericA)); REQUIRE(U(IsCharUpperA)); REQUIRE(U(IsCharLowerA));
    for (int c = 1; c < 128; c++) {
        int al = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        int dg = c >= '0' && c <= '9';
        CHECKF(IA(IsCharAlphaA, (CHAR)c) == al, "IsCharAlphaA(%d)", c);
        CHECKF(IA(IsCharAlphaNumericA, (CHAR)c) == (al || dg), "IsCharAlphaNumericA(%d)", c);
        CHECKF(IA(IsCharUpperA, (CHAR)c) == (c >= 'A' && c <= 'Z'), "IsCharUpperA(%d)", c);
        CHECKF(IA(IsCharLowerA, (CHAR)c) == (c >= 'a' && c <= 'z'), "IsCharLowerA(%d)", c);
    }
}

TEST(chars_isalphaW_ascii) {
    REQUIRE(U(IsCharAlphaW)); REQUIRE(U(IsCharAlphaNumericW)); REQUIRE(U(IsCharUpperW)); REQUIRE(U(IsCharLowerW));
    for (int c = 1; c < 128; c++) {
        int al = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        int dg = c >= '0' && c <= '9';
        CHECKF(IA(IsCharAlphaW, (WCHAR)c) == al, "IsCharAlphaW(%d)", c);
        CHECKF(IA(IsCharAlphaNumericW, (WCHAR)c) == (al || dg), "IsCharAlphaNumericW(%d)", c);
        CHECKF(IA(IsCharUpperW, (WCHAR)c) == (c >= 'A' && c <= 'Z'), "IsCharUpperW(%d)", c);
        CHECKF(IA(IsCharLowerW, (WCHAR)c) == (c >= 'a' && c <= 'z'), "IsCharLowerW(%d)", c);
    }
}

TEST(chars_isalpha_nul) {
    REQUIRE(U(IsCharAlphaA)); REQUIRE(U(IsCharAlphaW)); REQUIRE(U(IsCharAlphaNumericA)); REQUIRE(U(IsCharAlphaNumericW));
    CHECK(!IA(IsCharAlphaA, 0)); CHECK(!IA(IsCharAlphaW, 0));
    CHECK(!IA(IsCharAlphaNumericA, 0)); CHECK(!IA(IsCharAlphaNumericW, 0));
    CHECK(!IA(IsCharUpperA, 0)); CHECK(!IA(IsCharUpperW, 0)); CHECK(!IA(IsCharLowerA, 0)); CHECK(!IA(IsCharLowerW, 0));
}

TEST(chars_isalpha_return_is_bool) {
    REQUIRE(U(IsCharAlphaW)); REQUIRE(U(IsCharUpperW));
    unsigned oddvals = 0;
    for (unsigned c = 0; c < 0x10000; c++) {
        BOOL a = U(IsCharAlphaW)((WCHAR)c), u = U(IsCharUpperW)((WCHAR)c);
        if (a != 0 && a != 1) oddvals++;
        if (u != 0 && u != 1) oddvals++;
    }
    t_note("IsCharAlphaW/IsCharUpperW returned values other than 0/1: %u", oddvals);
}

TEST(chars_isupper_islower_exclusive_all_wide) {
    REQUIRE(U(IsCharUpperW)); REQUIRE(U(IsCharLowerW));
    unsigned both = 0;
    for (unsigned c = 0; c < 0x10000; c++) if (U(IsCharUpperW)((WCHAR)c) && U(IsCharLowerW)((WCHAR)c)) both++;
    t_note("code units both upper and lower (title-case letters etc.): %u", both);
    CHECK(both < 100);
}

TEST(chars_alnum_superset_of_alpha_all_wide) {
    REQUIRE(U(IsCharAlphaW)); REQUIRE(U(IsCharAlphaNumericW));
    unsigned bad = 0, alpha = 0, alnum = 0;
    for (unsigned c = 0; c < 0x10000; c++) {
        int a = U(IsCharAlphaW)((WCHAR)c) != 0, n = U(IsCharAlphaNumericW)((WCHAR)c) != 0;
        alpha += a; alnum += n;
        if (a && !n) bad++;
    }
    CHECKF(bad == 0, "%u alpha-but-not-alnum code units", bad);
    t_note("alpha=%u alnum=%u of 65536", alpha, alnum);
    CHECK(alpha > 20000);
    CHECK(alnum > alpha);
}

TEST(chars_upper_lower_imply_alpha_basic_planes) {
    REQUIRE(U(IsCharAlphaW)); REQUIRE(U(IsCharUpperW)); REQUIRE(U(IsCharLowerW));
    static const unsigned ranges[][2] = { {0x41, 0x5A}, {0x61, 0x7A}, {0xC0, 0xD6}, {0xD8, 0xF6}, {0xF8, 0x24F}, {0x391, 0x3A1}, {0x3A3, 0x3A9}, {0x3B1, 0x3C9}, {0x410, 0x44F}, {0x531, 0x556}, {0x561, 0x586}, {0xFF21, 0xFF3A}, {0xFF41, 0xFF5A} };
    for (unsigned r = 0; r < sizeof ranges / sizeof *ranges; r++)
        for (unsigned c = ranges[r][0]; c <= ranges[r][1]; c++) {
            if (U(IsCharUpperW)((WCHAR)c) || U(IsCharLowerW)((WCHAR)c)) CHECKF(U(IsCharAlphaW)((WCHAR)c), "U+%04X cased but not alpha", c);
            else CHECKF(U(IsCharAlphaW)((WCHAR)c), "U+%04X in letter range but not alpha", c);
        }
}

TEST(chars_wide_case_classification_matches_mapping) {
    REQUIRE(U(IsCharUpperW)); REQUIRE(U(IsCharLowerW)); REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    unsigned mismatch_up = 0, mismatch_lo = 0;
    for (unsigned c = 0; c < 0x10000; c++) {
        WCHAR lo = (WCHAR)(ULONG_PTR)U(CharLowerW)((LPWSTR)(ULONG_PTR)c);
        WCHAR up = (WCHAR)(ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c);
        if (lo != c && !U(IsCharUpperW)((WCHAR)c)) mismatch_up++;
        if (up != c && !U(IsCharLowerW)((WCHAR)c)) mismatch_lo++;
    }
    t_note("chars with a lowercase mapping but not IsCharUpper: %u; with uppercase mapping but not IsCharLower: %u", mismatch_up, mismatch_lo);
    CHECK(mismatch_up < 300);
    CHECK(mismatch_lo < 300);
}

TEST(chars_wide_roundtrip_latin_greek_cyrillic) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW)); REQUIRE(U(IsCharUpperW)); REQUIRE(U(IsCharLowerW));
    static const unsigned ranges[][2] = { {0x61, 0x7A}, {0xE0, 0xF6}, {0xF8, 0xFE}, {0x3B1, 0x3C1}, {0x3C3, 0x3C9}, {0x430, 0x45F} };
    for (unsigned r = 0; r < sizeof ranges / sizeof *ranges; r++)
        for (unsigned c = ranges[r][0]; c <= ranges[r][1]; c++) {
            ULONG_PTR up = (ULONG_PTR)U(CharUpperW)((LPWSTR)(ULONG_PTR)c);
            CHECKF(up != c, "U+%04X has an uppercase form", c);
            CHECKF(U(IsCharLowerW)((WCHAR)c), "U+%04X IsCharLowerW", c);
            CHECKF(U(IsCharUpperW)((WCHAR)up), "U+%04lX IsCharUpperW", (unsigned long)up);
            CHECKF((ULONG_PTR)U(CharLowerW)((LPWSTR)up) == c, "round trip U+%04X", c);
        }
}

TEST(chars_isdigit_wide_alnum_not_alpha) {
    REQUIRE(U(IsCharAlphaW)); REQUIRE(U(IsCharAlphaNumericW));
    for (unsigned c = '0'; c <= '9'; c++) { CHECK(!U(IsCharAlphaW)((WCHAR)c)); CHECK(U(IsCharAlphaNumericW)((WCHAR)c)); }
    for (unsigned c = 0xFF10; c <= 0xFF19; c++) CHECKF(U(IsCharAlphaNumericW)((WCHAR)c), "fullwidth digit U+%04X", c);
    for (unsigned c = 0x660; c <= 0x669; c++) CHECKF(U(IsCharAlphaNumericW)((WCHAR)c), "arabic-indic digit U+%04X", c);
    CHECK(U(IsCharAlphaNumericW)(0x00B2) || 1);
}

TEST(chars_punctuation_and_controls_not_alnum) {
    REQUIRE(U(IsCharAlphaNumericW)); REQUIRE(U(IsCharAlphaNumericA));
    for (unsigned c = 1; c < 0x20; c++) { CHECK(!U(IsCharAlphaNumericW)((WCHAR)c)); CHECK(!U(IsCharAlphaNumericA)((CHAR)c)); }
    const char *p = " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    for (; *p; p++) { CHECKF(!U(IsCharAlphaNumericW)((WCHAR)*p), "'%c'", *p); CHECKF(!U(IsCharAlphaNumericA)(*p), "'%c'", *p); }
    for (unsigned c = 0x2000; c < 0x2040; c++) CHECKF(!U(IsCharAlphaNumericW)((WCHAR)c), "U+%04X general punctuation", c);
    for (unsigned c = 0xD800; c < 0xE000; c += 17) CHECKF(!U(IsCharAlphaNumericW)((WCHAR)c), "surrogate U+%04X", c);
}

TEST(chars_cjk_alpha_wide) {
    REQUIRE(U(IsCharAlphaW));
    for (unsigned c = 0x4E00; c < 0x4E80; c++) CHECKF(U(IsCharAlphaW)((WCHAR)c), "CJK U+%04X", c);
    /* Win10 22H2 oracle: IsCharAlphaW is FALSE for kana (no C1_ALPHA for hiragana/katakana); observed, not asserted */
    { unsigned hira = 0, kata = 0;
      for (unsigned c = 0x3041; c < 0x3090; c++) hira += U(IsCharAlphaW)((WCHAR)c) != 0;
      for (unsigned c = 0x30A1; c < 0x30F0; c++) kata += U(IsCharAlphaW)((WCHAR)c) != 0;
      t_note("IsCharAlphaW: hiragana %u/79, katakana %u/79 alpha", hira, kata); }
    for (unsigned c = 0xAC00; c < 0xAC40; c++) CHECKF(U(IsCharAlphaW)((WCHAR)c), "hangul U+%04X", c);
    for (unsigned c = 0x5D0; c <= 0x5EA; c++) CHECKF(U(IsCharAlphaW)((WCHAR)c), "hebrew U+%04X", c);
    for (unsigned c = 0x627; c <= 0x63A; c++) CHECKF(U(IsCharAlphaW)((WCHAR)c), "arabic U+%04X", c);
    CHECK(!U(IsCharUpperW)(0x4E00)); CHECK(!U(IsCharLowerW)(0x4E00));
}

TEST(chars_isalphaA_cp1252_high) {
    REQUIRE(U(IsCharAlphaA)); REQUIRE(U(IsCharUpperA)); REQUIRE(U(IsCharLowerA)); REQUIRE(U(IsCharAlphaNumericA));
    if (acp() != 1252) { t_skip("ACP %d", acp()); return; }
    for (int c = 0xC0; c <= 0xFF; c++) {
        if (c == 0xD7 || c == 0xF7) { CHECKF(!U(IsCharAlphaA)((CHAR)c), "0x%x", c); continue; }
        CHECKF(U(IsCharAlphaA)((CHAR)c), "0x%x alpha", c);
        CHECKF(U(IsCharAlphaNumericA)((CHAR)c), "0x%x alnum", c);
        if (c <= 0xDE) { CHECKF(U(IsCharUpperA)((CHAR)c), "0x%x upper", c); CHECKF(!U(IsCharLowerA)((CHAR)c), "0x%x !lower", c); }
        else if (c != 0xDF) { CHECKF(U(IsCharLowerA)((CHAR)c), "0x%x lower", c); CHECKF(!U(IsCharUpperA)((CHAR)c), "0x%x !upper", c); }
    }
    CHECK(U(IsCharLowerA)((CHAR)0xDF));
    CHECK(U(IsCharAlphaA)((CHAR)0x8A)); CHECK(U(IsCharUpperA)((CHAR)0x8A));
    CHECK(U(IsCharAlphaA)((CHAR)0x9A)); CHECK(U(IsCharLowerA)((CHAR)0x9A));
    CHECK(U(IsCharAlphaA)((CHAR)0xAA)); CHECK(U(IsCharAlphaA)((CHAR)0xBA));
    CHECK(!U(IsCharAlphaA)((CHAR)0xA0)); CHECK(!U(IsCharAlphaA)((CHAR)0xB1));
}

TEST(chars_isalphaA_vs_W_single_byte_acp) {
    REQUIRE(U(IsCharAlphaA)); REQUIRE(U(IsCharAlphaW)); REQUIRE(U(IsCharUpperA)); REQUIRE(U(IsCharUpperW));
    CPINFO ci;
    if (!GetCPInfo(CP_ACP, &ci) || ci.MaxCharSize != 1) { t_skip("ACP is not single-byte"); return; }
    unsigned da = 0, du = 0;
    for (int c = 1; c < 256; c++) {
        WCHAR w = 0; char ch = (char)c;
        if (MultiByteToWideChar(CP_ACP, 0, &ch, 1, &w, 1) != 1) continue;
        if (!U(IsCharAlphaA)((CHAR)c) != !U(IsCharAlphaW)(w)) da++;
        if (!U(IsCharUpperA)((CHAR)c) != !U(IsCharUpperW)(w)) du++;
    }
    t_note("A/W alpha mismatches=%u upper mismatches=%u", da, du);
    CHECK(da < 8);
    CHECK(du < 8);
}

TEST(chars_isalphaA_signedness) {
    /* CHAR is signed in the SDK: a negative char must be treated as the unsigned byte */
    REQUIRE(U(IsCharAlphaA)); REQUIRE(U(CharUpperA));
    if (acp() != 1252) { t_skip("ACP %d", acp()); return; }
    CHAR c = (CHAR)0xE9;
    CHECK(c < 0);
    CHECK(U(IsCharAlphaA)(c));
    CHECK(U(IsCharLowerA)(c));
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)(unsigned char)c) == 0xC9);
}
