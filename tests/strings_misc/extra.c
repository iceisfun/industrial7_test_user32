#include "t.h"
#include <windows.h>
#include <string.h>

typedef int (WINAPI *pwsprintfA_t)(LPSTR, LPCSTR, ...);
typedef int (WINAPI *pwsprintfW_t)(LPWSTR, LPCWSTR, ...);

TEST(smx_wsprintfA_empty_format) {
    REQUIRE(U(wsprintfA));
    char b[16]; memset(b, 'x', sizeof b);
    int r = ((pwsprintfA_t)U(wsprintfA))(b, "");
    CHECK(r == 0);
    CHECK(b[0] == 0);
}

TEST(smx_wsprintfA_literal_and_pct) {
    REQUIRE(U(wsprintfA));
    char b[32];
    int r = ((pwsprintfA_t)U(wsprintfA))(b, "100%%");
    CHECK(r == 4);
    CHECK(!strcmp(b, "100%"));
}

TEST(smx_wsprintfA_ints_table) {
    REQUIRE(U(wsprintfA));
    static const struct { const char *f; int v; const char *want; } t[] = {
        {"%d", 0, "0"}, {"%d", -1, "-1"}, {"%u", -1, "4294967295"}, {"%x", 255, "ff"},
        {"%X", 255, "FF"}, {"%5d", 42, "   42"}, {"%-5d|", 42, "42   |"}, {"%05d", 42, "00042"},
        {"%i", 7, "7"}, {"%c", 'A', "A"}, {"%d", 2147483647, "2147483647"}, {"%d", (int)0x80000000, "-2147483648"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        char b[64];
        int r = ((pwsprintfA_t)U(wsprintfA))(b, t[i].f, t[i].v);
        CHECKF(!strcmp(b, t[i].want) && r == (int)strlen(t[i].want), "fmt %s got '%s' (%d)", t[i].f, b, r);
    }
}

TEST(smx_wsprintfW_ints_table) {
    REQUIRE(U(wsprintfW));
    static const struct { const WCHAR *f; int v; const WCHAR *want; } t[] = {
        {L"%d", 0, L"0"}, {L"%d", -1, L"-1"}, {L"%u", -1, L"4294967295"}, {L"%x", 255, L"ff"},
        {L"%X", 255, L"FF"}, {L"%5d", 42, L"   42"}, {L"%05d", 42, L"00042"}, {L"%c", 'A', L"A"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR b[64];
        int r = ((pwsprintfW_t)U(wsprintfW))(b, t[i].f, t[i].v);
        CHECKF(!lstrcmpW(b, t[i].want) && r == lstrlenW(t[i].want), "idx %u r=%d", i, r);
    }
}

TEST(smx_charupper_roundtrip_ascii) {
    REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerA));
    for (int c = 'a'; c <= 'z'; c++) {
        CHECKF((UINT_PTR)U(CharUpperA)((LPSTR)(UINT_PTR)c) == (UINT_PTR)(c - 32), "up %d", c);
        CHECKF((UINT_PTR)U(CharLowerA)((LPSTR)(UINT_PTR)(c - 32)) == (UINT_PTR)c, "lo %d", c);
    }
}

TEST(smx_charnextW_stops_at_nul) {
    REQUIRE(U(CharNextW)); REQUIRE(U(CharPrevW));
    static const WCHAR s[] = L"ab";
    CHECK(U(CharNextW)(s) == s + 1);
    CHECK(U(CharNextW)(s + 2) == s + 2);
    CHECK(U(CharNextA)("") != NULL);
}
