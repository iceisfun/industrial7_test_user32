/* wsprintfW / wvsprintfW conformance and A/W parity. */
#include "t.h"

typedef int (WINAPIV *wsA_t)(LPSTR, LPCSTR, ...);
typedef int (WINAPIV *wsW_t)(LPWSTR, LPCWSTR, ...);

#define FW(exp, fmt, ...) do { \
    WCHAR b_[2048]; for (int i_ = 0; i_ < 2048; i_++) b_[i_] = 0x5555; \
    int r_ = pW(b_, fmt, __VA_ARGS__); \
    CHECKF(!lstrcmpW(b_, exp) && r_ == lstrlenW(exp), "fmt \"%ls\": got \"%ls\" (%d) want \"%ls\"", fmt, b_, r_, exp); } while (0)
#define FW0(exp, fmt) do { \
    WCHAR b_[2048]; for (int i_ = 0; i_ < 2048; i_++) b_[i_] = 0x5555; \
    int r_ = pW(b_, fmt); \
    CHECKF(!lstrcmpW(b_, exp) && r_ == lstrlenW(exp), "fmt \"%ls\": got \"%ls\" (%d) want \"%ls\"", fmt, b_, r_, exp); } while (0)
#define NW(fmt, ...) do { WCHAR b_[512]; int r_ = pW(b_, fmt, __VA_ARGS__); t_note("wsprintfW(\"%ls\") = [%ls] ret %d", fmt, b_, r_); } while (0)

static int WINAPIV call_wvw(LPWSTR buf, LPCWSTR fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = U(wvsprintfW)(buf, fmt, ap);
    va_end(ap);
    return r;
}

TEST(wsprintfW_plain_and_percent) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW0(L"", L"");
    FW0(L"a", L"a");
    FW0(L"hello world", L"hello world");
    FW0(L"éα中", L"éα中");
    FW0(L"%", L"%%");
    FW0(L"100%", L"100%%");
    FW0(L"abc", L"abc%");
    FW0(L"", L"%");
    FW(L"5%", L"%d%%", 5);
}

TEST(wsprintfW_integers) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"0", L"%d", 0);
    FW(L"-1", L"%d", -1);
    FW(L"42", L"%i", 42);
    FW(L"2147483647", L"%d", 2147483647);
    FW(L"-2147483648", L"%d", (int)0x80000000);
    FW(L"4294967295", L"%u", -1);
    FW(L"4294967295", L"%lu", 0xFFFFFFFFu);
    FW(L"-123", L"%ld", -123);
    FW(L"ff", L"%x", 255);
    FW(L"FF", L"%X", 255);
    FW(L"deadbeef", L"%x", 0xdeadbeefu);
    FW(L"DEADBEEF", L"%lX", 0xdeadbeefu);
    FW(L"ffffffff", L"%x", -1);
}

TEST(wsprintfW_width_flags) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"   42", L"%5d", 42);
    FW(L"42   ", L"%-5d", 42);
    FW(L"00042", L"%05d", 42);
    FW(L"-0042", L"%05d", -42);
    FW(L"-000000001", L"%010ld", -1);
    FW(L"  -42", L"%5d", -42);
    FW(L"-42  ", L"%-5d", -42);
    FW(L"0000beef", L"%08x", 0xbeef);
    FW(L"0000BEEF", L"%08X", 0xbeef);
    FW(L"    beef", L"%8x", 0xbeef);
    FW(L"beef    |", L"%-8x|", 0xbeef);
    FW(L"12345", L"%3d", 12345);
    FW(L"005", L"%.3d", 5);
    FW(L"  005", L"%5.3d", 5);
    FW(L"005  ", L"%-5.3d", 5);
    FW(L"0xff", L"%#x", 255);
    FW(L"0XFF", L"%#X", 255);
    FW(L"42   |", L"%-05d|", 42);
}

TEST(wsprintfW_unsupported_conversions) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"o", L"%o", 8);
    FW(L"f", L"%f", 1);
    FW(L"e", L"%e", 1);
    FW(L"g", L"%g", 1);
    FW(L"n", L"%n", 1);
    FW(L"q 5", L"%q %d", 5);
    NW(L"%+d", 5);
    NW(L"% d", 5);
    NW(L"%*d", 5, 3);
}

TEST(wsprintfW_float_not_supported) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    WCHAR b[64];
    int r = pW(b, L"%f", 3.5);
    CHECK(r == 1);
    CHECK(!lstrcmpW(b, L"f"));
}

TEST(wsprintfW_c_wide_char) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"A", L"%c", 'A');
    FW(L"é", L"%c", 0xe9);
    FW(L"★", L"%c", 0x2605);
    FW(L"中", L"%lc", 0x4e2d);
    FW(L"中", L"%wc", 0x4e2d);
    FW(L" *", L"%2c", L'*');
    FW(L"*  |", L"%-3c|", L'*');
    FW(L"xAy", L"x%cy", 'A');
    FW(L"Ł", L"%c", 0x141);
}

TEST(wsprintfW_c_wide_char_high_values) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"\xffff", L"%c", 0xffff);
    FW(L"\x0141", L"%c", 0x10141);   /* only the low 16 bits are used */
}

TEST(wsprintfW_C_narrow_char) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"A", L"%C", 'A');
    FW(L"A", L"%hc", 'A');
    FW(L"A", L"%hC", 'A');
    FW(L"z", L"%C", 'z');
    FW(L"    A", L"%5C", 'A');
    FW(L"A    ", L"%-5C", 'A');
    WCHAR b[8]; wsW_t p = pW;
    if (GetACP() == 1252) {
        int r = p(b, L"%C", 0xe9);
        CHECK(r == 1); CHECK(b[0] == 0xe9);
        r = p(b, L"%C", 0x80);
        CHECK(r == 1); CHECK(b[0] == 0x20ac);
    }
}

TEST(wsprintfW_s_wide_string) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"abc", L"%s", L"abc");
    FW(L"", L"%s", L"");
    FW(L"abc", L"%ls", L"abc");
    FW(L"abc", L"%ws", L"abc");
    FW(L"abc", L"%lS", L"abc");
    FW(L"xabcy", L"x%sy", L"abc");
    FW(L"   abc", L"%6s", L"abc");
    FW(L"abc   |", L"%-6s|", L"abc");
    FW(L"ab", L"%.2s", L"abc");
    FW(L"", L"%.0s", L"abc");
    FW(L"   ab", L"%5.2s", L"abc");
    FW(L"wörld 中", L"%s", L"wörld 中");
    FW(L"a b", L"%s %s", L"a", L"b");
}

TEST(wsprintfW_S_narrow_string) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"abc", L"%S", "abc");
    FW(L"abc", L"%hs", "abc");
    FW(L"abc", L"%hS", "abc");
    FW(L"", L"%S", "");
    FW(L"  abc", L"%5S", "abc");
    FW(L"abc  |", L"%-5S|", "abc");
    FW(L"ab", L"%.2S", "abc");
    FW(L"ab", L"%.2hs", "abc");
    FW(L"x-y-z", L"%s-%S-%hs", L"x", "y", "z");
    if (GetACP() == 1252) FW(L"café", L"%S", "caf\xe9");
}

TEST(wsprintfW_s_null_pointer) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    WCHAR b[64];
    int r = pW(b, L"[%s]", (WCHAR *)NULL);
    t_note("wsprintfW(\"[%%s]\", NULL) = [%ls] ret %d", b, r);
    CHECK(r == lstrlenW(b));
    r = pW(b, L"[%S]", (char *)NULL);
    t_note("wsprintfW(\"[%%S]\", NULL) = [%ls] ret %d", b, r);
    CHECK(r == lstrlenW(b));
}

TEST(wsprintfW_p) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"0000000000000000", L"%p", (void *)0);
    FW(L"00000000DEADBEEF", L"%p", (void *)0xdeadbeef);
    FW(L"FFFFFFFFFFFFFFFF", L"%p", (void *)(ULONG_PTR)-1);
    FW(L"FFFFFFFFFFFEDCBB", L"%p", (void *)(ULONG_PTR)-0x12345);
    FW(L"<0000000000000042>", L"<%p>", (void *)0x42);
}

TEST(wsprintfW_i64) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    WCHAR b[64];
    if (pW(b, L"%I64x", (ULONGLONG)0) == 4) { t_skip("I64 unsupported"); return; }
    FW(L"123450067890A", L"%I64X", ((ULONGLONG)0x12345 << 32) | 0x67890a);
    FW(L"67890A", L"%I32X", ((ULONGLONG)0x12345 << 32) | 0x67890a);
    FW(L"543210000000", L"%I64d", (ULONGLONG)543210 * 1000000);
    FW(L"FFFFFFFFFFFEDCBB", L"%I64X", (LONGLONG)-0x12345);
    FW(L"fffedcbb", L"%I32x", (LONGLONG)-0x12345);
    FW(L"18446744073709551493", L"%I64u", (LONGLONG)-123);
    FW(L"-12345", L"%Id", (LONGLONG)-12345);
    FW(L"123450067890a", L"%Ix", ((ULONGLONG)0x12345 << 32) | 0x67890a);
    FW(L"fffffffffffedcbb", L"%Ix", (LONGLONG)-0x12345);
    FW(L"-9223372036854775808", L"%I64d", (LONGLONG)0x8000000000000000ULL);
    FW(L"18446744073709551615", L"%I64u", (ULONGLONG)-1);
}

TEST(wsprintfW_h_prefix) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    FW(L"4464", L"%hd", 70000);
    FW(L"-1", L"%hd", 0xFFFF);
    FW(L"123", L"%hd", 123);
}

TEST(wsprintfW_return_values) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    WCHAR b[64];
    CHECK(pW(b, L"") == 0);
    CHECK(pW(b, L"abc") == 3);
    CHECK(pW(b, L"%d", 12345) == 5);
    CHECK(pW(b, L"%10d", 1) == 10);
    CHECK(pW(b, L"%s", L"中文") == 2);
    CHECK(pW(b, L"%S", "abcd") == 4);
}

TEST(wsprintfW_nul_terminated_exactly) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    WCHAR b[16];
    for (int i = 0; i < 16; i++) b[i] = 'Z';
    int r = pW(b, L"abc");
    CHECK(r == 3); CHECK(b[3] == 0); CHECK(b[4] == 'Z');
    for (int i = 0; i < 16; i++) b[i] = 'Z';
    r = pW(b, L"%5d", 42);
    CHECK(r == 5); CHECK(b[5] == 0); CHECK(b[6] == 'Z');
}

TEST(wsprintfW_limit_1024) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    static const int sizes[] = { 1000, 1022, 1023, 1024, 1025, 1500, 2000 };
    WCHAR *fmt = (WCHAR *)malloc(4100 * sizeof(WCHAR)), *out = (WCHAR *)malloc(8192 * sizeof(WCHAR));
    REQUIRE(fmt && out);
    for (unsigned k = 0; k < sizeof sizes / sizeof *sizes; k++) {
        int n = sizes[k];
        for (int i = 0; i < n; i++) fmt[i] = L'x';
        fmt[n] = 0;
        for (int i = 0; i < 8192; i++) out[i] = 0x5555;
        int r = pW(out, fmt);
        int len = lstrlenW(out);
        t_note("W literal n=%d -> ret %d len %d", n, r, len);
        if (n <= 1022) CHECKF(r == n && len == n, "n=%d ret %d len %d", n, r, len);
        else {
            CHECKF(r >= 1023 && r <= 1024, "n=%d ret %d", n, r);
            CHECKF(len == r || len == r - 1, "n=%d len %d ret %d", n, len, r);
            CHECKF(out[1026] == 0x5555 && out[1500] == 0x5555, "n=%d overflow", n);
        }
    }
    for (int i = 0; i < 4000; i++) fmt[i] = L's';
    fmt[4000] = 0;
    for (int i = 0; i < 8192; i++) out[i] = 0x5555;
    int r = pW(out, L"%s", fmt);
    CHECKF(r >= 1023 && r <= 1024, "%%s 4000 ret %d", r);
    CHECK(out[1026] == 0x5555);
    free(fmt); free(out);
}

TEST(wvsprintfW_basic) {
    REQUIRE(U(wvsprintfW));
    WCHAR b[256];
    int r = call_wvw(b, L"%d|%s|%x|%c", -5, L"str", 0xBEEF, L'q');
    CHECK(r == lstrlenW(b));
    EXPECT_WSTR(b, L"-5|str|beef|q");
    r = call_wvw(b, L"");
    CHECK(r == 0);
    r = call_wvw(b, L"%S|%hs", "ansi", "ansi2");
    CHECK(r == 10); EXPECT_WSTR(b, L"ansi|ansi2");
    r = call_wvw(b, L"%08X", 0x1234abcd);
    CHECK(r == 8); EXPECT_WSTR(b, L"1234ABCD");
    r = call_wvw(b, L"%p", (void *)0x77);
    CHECK(r == 16); EXPECT_WSTR(b, L"0000000000000077");
    r = call_wvw(b, L"%I64u", (ULONGLONG)18446744073709551615ULL);
    CHECK(r == 20);
}

TEST(wvsprintfW_limit) {
    REQUIRE(U(wvsprintfW));
    WCHAR *fmt = (WCHAR *)malloc(3000 * sizeof(WCHAR)), *out = (WCHAR *)malloc(4096 * sizeof(WCHAR));
    REQUIRE(fmt && out);
    for (int i = 0; i < 2500; i++) fmt[i] = L'w';
    fmt[2500] = 0;
    for (int i = 0; i < 4096; i++) out[i] = 0x5555;
    int r = call_wvw(out, fmt);
    CHECKF(r >= 1023 && r <= 1024, "ret %d", r);
    CHECK(out[1026] == 0x5555);
    free(fmt); free(out);
}

TEST(wsprintf_AW_parity_integer_table) {
    REQUIRE(U(wsprintfA)); REQUIRE(U(wsprintfW)); wsA_t pA = (wsA_t)U(wsprintfA); wsW_t pW = (wsW_t)U(wsprintfW);
    static const char *fa[] = { "%d", "%5d", "%-5d|", "%05d", "%u", "%x", "%X", "%08x", "%#x", "%.4d", "%ld", "%lu", "%hd", "%i", "%%", "%o", "%f", "%+d", "% d", "%5.3d", "%I64d", "%p", "%I32x" };
    static const int vals[] = { 0, 1, -1, 42, -42, 255, 65535, 0x7fffffff, (int)0x80000000, 123456789 };
    for (unsigned f = 0; f < sizeof fa / sizeof *fa; f++) {
        WCHAR wf[32]; MultiByteToWideChar(CP_ACP, 0, fa[f], -1, wf, 32);
        for (unsigned v = 0; v < sizeof vals / sizeof *vals; v++) {
            char a[128]; WCHAR w[128]; char conv[128];
            int ra = pA(a, fa[f], (long long)vals[v]);
            int rw = pW(w, wf, (long long)vals[v]);
            WideCharToMultiByte(CP_ACP, 0, w, -1, conv, 128, NULL, NULL);
            CHECKF(ra == rw && !strcmp(a, conv), "fmt \"%s\" val %d: A=[%s]/%d W=[%s]/%d", fa[f], vals[v], a, ra, conv, rw);
        }
    }
}

TEST(wsprintf_AW_parity_strings) {
    REQUIRE(U(wsprintfA)); REQUIRE(U(wsprintfW)); wsA_t pA = (wsA_t)U(wsprintfA); wsW_t pW = (wsW_t)U(wsprintfW);
    char a[128]; WCHAR w[128]; char conv[128];
    int ra = pA(a, "%s|%5s|%-5s|%.2s", "abc", "de", "fg", "hijk");
    int rw = pW(w, L"%s|%5s|%-5s|%.2s", L"abc", L"de", L"fg", L"hijk");
    WideCharToMultiByte(CP_ACP, 0, w, -1, conv, 128, NULL, NULL);
    CHECK(ra == rw); CHECK(!strcmp(a, conv));
    EXPECT_STR(a, "abc|   de|fg   |hi");
    ra = pA(a, "%c%c%C", 'a', 'b', (WCHAR)'c');
    rw = pW(w, L"%c%c%C", L'a', L'b', 'c');
    WideCharToMultiByte(CP_ACP, 0, w, -1, conv, 128, NULL, NULL);
    CHECK(ra == rw); CHECK(!strcmp(a, conv)); EXPECT_STR(a, "abc");
    ra = pA(a, "%S", L"xyz");
    rw = pW(w, L"%S", "xyz");
    WideCharToMultiByte(CP_ACP, 0, w, -1, conv, 128, NULL, NULL);
    CHECK(ra == rw); CHECK(!strcmp(a, conv)); EXPECT_STR(a, "xyz");
}

TEST(wsprintfW_repeated_calls_stable) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    WCHAR b[64], e[64];
    for (int i = 0; i < 20000; i++) {
        int r = pW(b, L"%d:%s:%x", i, L"v", i);
        swprintf(e, 64, L"%d:v:%x", i, i);
        if (lstrcmpW(b, e) || r != lstrlenW(e)) { CHECKF(0, "iter %d got [%ls] want [%ls]", i, b, e); break; }
    }
}

TEST(wsprintfW_last_error_untouched) {
    REQUIRE(U(wsprintfW)); wsW_t pW = (wsW_t)U(wsprintfW);
    WCHAR b[16];
    SetLastError(0x4321);
    pW(b, L"%d", 7);
    t_note("last error after wsprintfW = 0x%lx", GetLastError());
    CHECK(GetLastError() == 0x4321);
}
