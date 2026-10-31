/* wsprintfA / wvsprintfA conformance. */
#include "t.h"

typedef int (WINAPIV *wsA_t)(LPSTR, LPCSTR, ...);
typedef int (WINAPIV *wsW_t)(LPWSTR, LPCWSTR, ...);

/* check expected output and return value; the buffer is pre-filled with a sentinel */
#define FA(exp, fmt, ...) do { \
    char b_[2048]; memset(b_, 0x55, sizeof b_); \
    int r_ = pA(b_, fmt, __VA_ARGS__); \
    CHECKF(!strcmp(b_, exp) && r_ == (int)strlen(exp), "fmt \"%s\": got \"%s\" (%d) want \"%s\"", fmt, b_, r_, exp); } while (0)
#define FA0(exp, fmt) do { \
    char b_[2048]; memset(b_, 0x55, sizeof b_); \
    int r_ = pA(b_, fmt); \
    CHECKF(!strcmp(b_, exp) && r_ == (int)strlen(exp), "fmt \"%s\": got \"%s\" (%d) want \"%s\"", fmt, b_, r_, exp); } while (0)
/* observation only */
#define NA(fmt, ...) do { char b_[512]; int r_ = pA(b_, fmt, __VA_ARGS__); t_note("wsprintfA(\"%s\") = [%s] ret %d", fmt, b_, r_); } while (0)

static int WINAPIV call_wv(LPSTR buf, LPCSTR fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = U(wvsprintfA)(buf, fmt, ap);
    va_end(ap);
    return r;
}

TEST(wsprintfA_plain_text) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA0("", "");
    FA0("a", "a");
    FA0("hello world", "hello world");
    FA0("0123456789", "0123456789");
    FA0("tab\there", "tab\there");
    FA0("nl\nx", "nl\nx");
    FA0("  leading and trailing  ", "  leading and trailing  ");
    FA0("\\ \" ' ~", "\\ \" ' ~");
}

TEST(wsprintfA_percent_literal) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA0("%", "%%");
    FA0("100%", "100%%");
    FA0("a%b", "a%%b");
    FA0("%%", "%%%%");
    FA0("%d", "%%d");
    FA0("%%%", "%%%%%%");
    FA("5%", "%d%%", 5);
    FA("%5", "%%%d", 5);
}

TEST(wsprintfA_trailing_percent) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA0("abc", "abc%");
    FA0("", "%");
    FA0("x", "x%");
}

TEST(wsprintfA_d_basic) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("0", "%d", 0);
    FA("1", "%d", 1);
    FA("-1", "%d", -1);
    FA("42", "%d", 42);
    FA("-42", "%d", -42);
    FA("2147483647", "%d", 2147483647);
    FA("-2147483648", "%d", (int)0x80000000);
    FA("-2147483647", "%d", -2147483647);
    FA("1000000", "%d", 1000000);
    FA("7", "%i", 7);
    FA("-7", "%i", -7);
    FA("2147483647", "%i", 2147483647);
    FA("-2147483648", "%i", (int)0x80000000);
}

TEST(wsprintfA_ld_lu_lx) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("-1", "%ld", -1);
    FA("123456", "%ld", 123456);
    FA("4294967295", "%lu", 0xFFFFFFFFu);
    FA("4294967295", "%lu", -1);
    FA("ff", "%lx", 255);
    FA("FF", "%lX", 255);
    FA("-2147483648", "%ld", (int)0x80000000);
    FA("-5", "%li", -5);
}

TEST(wsprintfA_u_basic) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("0", "%u", 0);
    FA("1", "%u", 1);
    FA("4294967295", "%u", -1);
    FA("4294967295", "%u", 0xFFFFFFFFu);
    FA("2147483648", "%u", 0x80000000u);
    FA("12345", "%u", 12345);
    FA("4294967254", "%u", -42);
}

TEST(wsprintfA_x_basic) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("0", "%x", 0);
    FA("1", "%x", 1);
    FA("f", "%x", 15);
    FA("10", "%x", 16);
    FA("ff", "%x", 255);
    FA("FF", "%X", 255);
    FA("deadbeef", "%x", 0xdeadbeefu);
    FA("DEADBEEF", "%X", 0xdeadbeefu);
    FA("ffffffff", "%x", -1);
    FA("FFFFFFFF", "%X", -1);
    FA("80000000", "%x", 0x80000000u);
    FA("abcdef", "%x", 0xabcdef);
    FA("ABCDEF", "%X", 0xabcdef);
    FA("1234abcd", "%x", 0x1234abcd);
}

TEST(wsprintfA_d_width) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("   42", "%5d", 42);
    FA("42", "%2d", 42);
    FA("42", "%1d", 42);
    FA("12345", "%3d", 12345);
    FA("  -42", "%5d", -42);
    FA("-42", "%3d", -42);
    FA("-42", "%2d", -42);
    FA("        0", "%9d", 0);
    FA("42   ", "%-5d", 42);
    FA("-42  ", "%-5d", -42);
    FA("0    ", "%-5d", 0);
    FA("42", "%-2d", 42);
    FA("      42", "%8d", 42);
    FA("42      |", "%-8d|", 42);
}

TEST(wsprintfA_d_zero_pad) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("00042", "%05d", 42);
    FA("-0042", "%05d", -42);
    FA("-000000001", "%010ld", -1);
    FA("0000000000", "%010d", 0);
    FA("42", "%02d", 42);
    FA("000042", "%06u", 42);
    FA("00ff", "%04x", 255);
    FA("00FF", "%04X", 255);
    FA("0000beef", "%08x", 0xbeef);
    FA("7", "%01d", 7);
    FA("2147483647", "%010d", 2147483647);
    FA("4294967295", "%010u", 0xFFFFFFFFu);
    FA("-2147483648", "%011d", (int)0x80000000);
    FA("-02147483648", "%012d", (int)0x80000000);
}

TEST(wsprintfA_x_width) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("    beef", "%8x", 0xbeef);
    FA("beef    ", "%-8x", 0xbeef);
    FA("BEEF    |", "%-8X|", 0xbeef);
    FA("    BEEF", "%8X", 0xbeef);
    FA("beef", "%2x", 0xbeef);
    FA("  ff", "%4x", 255);
    FA("ff  ", "%-4x", 255);
    FA("   0", "%4x", 0);
}

TEST(wsprintfA_precision_integers) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("005", "%.3d", 5);
    FA("12345", "%.3d", 12345);
    FA("  005", "%5.3d", 5);
    FA("005  ", "%-5.3d", 5);
    FA("0000beef", "%.8x", 0xbeef);
    FA("0000BEEF", "%.8X", 0xbeef);
    FA("00000042", "%.8u", 42);
    NA("%.3d", -5);
    NA("%6.3d", -5);
}

TEST(wsprintfA_hash_flag) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("0xff", "%#x", 255);
    FA("0XFF", "%#X", 255);
    FA("0x1", "%#x", 1);
    FA("0xdeadbeef", "%#x", 0xdeadbeefu);
    FA("0XDEADBEEF", "%#X", 0xdeadbeefu);
    NA("%#x", 0);
    NA("%#10x", 255);
    NA("%#010x", 255);
    NA("%#-10x|", 255);
    NA("%#d", 12);
    NA("%#u", 12);
}

TEST(wsprintfA_unsupported_flags_are_literal) {
    /* only '-', '#' and '0' are flags; '+' and ' ' end the specification and the specification char is output literally */
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    NA("%+d", 5);
    NA("% d", 5);
    NA("%+5d", 5);
    NA("%*d", 5, 3);
    NA("%.*d", 3, 5);
    NA("%'d", 5);
}

TEST(wsprintfA_unsupported_conversions) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    /* floating point and octal conversions are not supported: the conversion character is copied literally */
    FA("o", "%o", 8);
    FA("f", "%f", 1);
    FA("e", "%e", 1);
    FA("g", "%g", 1);
    FA("E", "%E", 1);
    FA("G", "%G", 1);
    FA("n", "%n", 1);
    FA("q", "%q", 1);
    FA("z", "%z", 1);
    FA("Z", "%Z", 1);
}

TEST(wsprintfA_unsupported_does_not_consume_arg) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("q 5", "%q %d", 5);
    FA("o,5", "%o,%d", 5);
}

TEST(wsprintfA_float_not_supported_note) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[128];
    int r = pA(b, "%f", 3.14159);
    CHECK(r == 1);
    CHECK(!strcmp(b, "f"));
    r = pA(b, "%.2f", 3.14159);
    t_note("wsprintfA(\"%%.2f\") = [%s]", b);
    CHECK(r == (int)strlen(b));
    r = pA(b, "%5.1f|", 2.5);
    t_note("wsprintfA(\"%%5.1f|\") = [%s]", b);
    CHECK(r == (int)strlen(b));
}

TEST(wsprintfA_c) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("A", "%c", 'A');
    FA("z", "%c", 'z');
    FA(" ", "%c", ' ');
    FA("%", "%c", '%');
    FA("    A", "%5c", 'A');
    FA("A    ", "%-5c", 'A');
    FA("A", "%1c", 'A');
    FA("xAy", "x%cy", 'A');
    FA("ABC", "%c%c%c", 'A', 'B', 'C');
}

TEST(wsprintfA_c_nul) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[16]; memset(b, 0x55, sizeof b);
    int r = pA(b, "a%cb", 0);
    t_note("wsprintfA(\"a%%cb\", 0) ret %d bytes %02x %02x %02x", r, b[0], b[1], b[2]);
    CHECK(b[0] == 'a');
}

TEST(wsprintfA_c_uses_low_byte) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("A", "%c", 0x141);
    FA("B", "%c", 0xFFFFFF42);
}

TEST(wsprintfA_s) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("abc", "%s", "abc");
    FA("", "%s", "");
    FA("xabcy", "x%sy", "abc");
    FA("a b", "%s %s", "a", "b");
    FA("   abc", "%6s", "abc");
    FA("abc   |", "%-6s|", "abc");
    FA("abc", "%2s", "abc");
    FA("ab", "%.2s", "abc");
    FA("", "%.0s", "abc");
    FA("abc", "%.10s", "abc");
    FA("   ab", "%5.2s", "abc");
    FA("ab   |", "%-5.2s|", "abc");
    FA("     ", "%5s", "");
    FA("hello world", "%s %s", "hello", "world");
    FA("%s", "%%s", "unused");
}

TEST(wsprintfA_s_null_pointer) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[64]; memset(b, 0x55, sizeof b);
    int r = pA(b, "[%s]", (char *)NULL);
    t_note("wsprintfA(\"[%%s]\", NULL) = %s ret %d", b, r);
    CHECK(r == (int)strlen(b));
    r = pA(b, "[%5s]", (char *)NULL);
    t_note("wsprintfA(\"[%%5s]\", NULL) = %s ret %d", b, r);
    CHECK(r == (int)strlen(b));
    r = pA(b, "[%S]", (WCHAR *)NULL);
    t_note("wsprintfA(\"[%%S]\", NULL) = %s ret %d", b, r);
    CHECK(r == (int)strlen(b));
}

TEST(wsprintfA_hs_ls_ws_S) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    /* in the ANSI function: %s/%hs narrow; %S/%ls/%ws wide */
    FA("abc", "%hs", "abc");
    FA("abc", "%ls", L"abc");
    FA("abc", "%ws", L"abc");
    FA("abc", "%S", L"abc");
    FA("abc", "%lS", L"abc");
    FA("abc", "%hS", "abc");
    FA("abc", "%wS", L"abc");
    FA("  abc", "%5ls", L"abc");
    FA("abc  |", "%-5S|", L"abc");
    FA("ab", "%.2ls", L"abc");
    FA("ab", "%.2S", L"abc");
    FA("", "%S", L"");
    FA("", "%ls", L"");
    FA("a-b-c", "%s-%S-%hs", "a", L"b", "c");
}

TEST(wsprintfA_C_lc_hc) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("A", "%C", (WCHAR)'A');
    FA("A", "%lc", (WCHAR)'A');
    FA("A", "%wc", (WCHAR)'A');
    FA("A", "%hc", 'A');
    FA("A", "%hC", 'A');
    FA("    A", "%5C", (WCHAR)'A');
    FA("A    ", "%-5C", (WCHAR)'A');
    FA("xyz", "%C%C%C", (WCHAR)'x', (WCHAR)'y', (WCHAR)'z');
}

TEST(wsprintfA_wide_to_ansi_conversion) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[64]; int r;
    if (GetACP() == 1252) {
        r = pA(b, "%ls", L"café");
        CHECK(r == 4); CHECK(!strcmp(b, "caf\xe9"));
        r = pA(b, "%C", (WCHAR)0xe9);
        CHECK(r == 1); CHECK((unsigned char)b[0] == 0xe9);
        r = pA(b, "%ls", L"€");
        CHECK(r == 1); CHECK((unsigned char)b[0] == 0x80);
    } else t_note("ACP %u: skipping Latin-1 conversion checks", GetACP());
    r = pA(b, "%ls", L"中");
    t_note("wsprintfA(\"%%ls\", U+4E2D) ret %d first byte 0x%02x", r, (unsigned char)b[0]);
    CHECK(r == (int)strlen(b));
}

TEST(wsprintfA_p) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    /* %p prints 16 uppercase hex digits on x64 */
    FA("0000000000000000", "%p", (void *)0);
    FA("0000000000001234", "%p", (void *)0x1234);
    FA("00000000DEADBEEF", "%p", (void *)0xdeadbeef);
    FA("0001234500067890", "%p", (void *)0x1234500067890ULL);
    FA("FFFFFFFFFFFFFFFF", "%p", (void *)(ULONG_PTR)-1);
    FA("FFFFFFFFFFFEDCBB", "%p", (void *)(ULONG_PTR)-0x12345);
    FA("[0000000000000001]", "[%p]", (void *)1);
}

TEST(wsprintfA_p_width) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("0000000000000010", "%4p", (void *)0x10);
    NA("%20p", (void *)0x10);
    NA("%-20p|", (void *)0x10);
    NA("%8p", (void *)0x10);
    NA("%.4p", (void *)0x10);
    NA("%#p", (void *)0x10);
}

TEST(wsprintfA_h_prefix) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("4464", "%hd", 70000);
    FA("-1", "%hd", 0xFFFF);
    FA("-1", "%hd", -1);
    FA("32767", "%hd", 32767);
    FA("-32768", "%hd", 32768);
    FA("123", "%hd", 123);
    NA("%hu", 70000);
    NA("%hu", -1);
    NA("%hx", 0x12345);
    NA("%hX", 0xFFFFF);
}

TEST(wsprintfA_l_prefix) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("70000", "%ld", 70000);
    FA("-70000", "%ld", -70000);
    FA("123456789", "%lu", 123456789u);
    FA("75bcd15", "%lx", 123456789);
    FA("75BCD15", "%lX", 123456789);
    NA("%lld", 5LL);
    NA("%llu", 5ULL);
    NA("%llx", 0x1234567890ULL);
    NA("%lli", 5LL);
}

TEST(wsprintfA_i64) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[64];
    int r = pA(b, "%I64X", (ULONGLONG)0);
    if (r == 4 && !strcmp(b, "I64X")) { t_skip("I64 formats unsupported"); return; }
    FA("0", "%I64X", (ULONGLONG)0);
    FA("123450067890A", "%I64X", ((ULONGLONG)0x12345 << 32) | 0x67890a);
    FA("67890A", "%I32X", ((ULONGLONG)0x12345 << 32) | 0x67890a);
    FA("543210000000", "%I64d", (ULONGLONG)543210 * 1000000);
    FA("FFFFFFFFFFFEDCBB", "%I64X", (LONGLONG)-0x12345);
    FA("fffedcbb", "%I32x", (LONGLONG)-0x12345);
    FA("18446744073709551493", "%I64u", (LONGLONG)-123);
    FA("-12345", "%Id", (LONGLONG)-12345);
    FA("123450067890a", "%Ix", ((ULONGLONG)0x12345 << 32) | 0x67890a);
    FA("fffffffffffedcbb", "%Ix", (LONGLONG)-0x12345);
    FA("9223372036854775807", "%I64d", (LONGLONG)0x7FFFFFFFFFFFFFFFLL);
    FA("-9223372036854775808", "%I64d", (LONGLONG)0x8000000000000000ULL);
    FA("18446744073709551615", "%I64u", (ULONGLONG)-1);
    FA("ffffffffffffffff", "%I64x", (ULONGLONG)-1);
    FA("-1", "%I32d", -1);
    FA("4294967295", "%I32u", (ULONGLONG)0xFFFFFFFFu);
}

TEST(wsprintfA_i64_width) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[64];
    if (pA(b, "%I64X", (ULONGLONG)0) == 4) { t_skip("I64 unsupported"); return; }
    FA("          12345", "%15I64d", (LONGLONG)12345);
    FA("12345          |", "%-15I64d|", (LONGLONG)12345);
    FA("000000000012345", "%015I64d", (LONGLONG)12345);
    FA("0000000000FFFFFF", "%016I64X", (ULONGLONG)0xFFFFFF);
    FA("-00000000012345", "%015I64d", (LONGLONG)-12345);
}

TEST(wsprintfA_multiple_args_stack) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("1 2 3 4 5 6 7 8", "%d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8);
    FA("a1b2c3", "%c%d%c%d%c%d", 'a', 1, 'b', 2, 'c', 3);
    FA("x=10 y=-20 s=str h=ff", "x=%d y=%d s=%s h=%x", 10, -20, "str", 255);
    FA("1,2,3,4,5,6,7,8,9,10", "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    FA("s1 s2 s3 s4 s5 s6", "%s %s %s %s %s %s", "s1", "s2", "s3", "s4", "s5", "s6");
    FA("0000000000000001 2 3 4", "%p %d %d %d", (void *)1, 2, 3, 4);
}

TEST(wsprintfA_return_values) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[64];
    CHECK(pA(b, "") == 0);
    CHECK(pA(b, "a") == 1);
    CHECK(pA(b, "%d", 12345) == 5);
    CHECK(pA(b, "%10d", 1) == 10);
    CHECK(pA(b, "%s", "") == 0);
    CHECK(pA(b, "%%") == 1);
    CHECK(pA(b, "abc%") == 3);
    CHECK(pA(b, "%I64d", (LONGLONG)1) == 1);
}

TEST(wsprintfA_nul_terminated_exactly) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[16]; memset(b, 'Z', sizeof b);
    int r = pA(b, "abc");
    CHECK(r == 3); CHECK(b[3] == 0); CHECK(b[4] == 'Z');
    memset(b, 'Z', sizeof b);
    r = pA(b, "%d", 42);
    CHECK(r == 2); CHECK(b[2] == 0); CHECK(b[3] == 'Z');
    memset(b, 'Z', sizeof b);
    r = pA(b, "%5d", 42);
    CHECK(r == 5); CHECK(b[5] == 0); CHECK(b[6] == 'Z');
    memset(b, 'Z', sizeof b);
    r = pA(b, "%s", "");
    CHECK(r == 0); CHECK(b[0] == 0); CHECK(b[1] == 'Z');
}

static void build_fmt(char *out, int n, char ch) { memset(out, ch, n); out[n] = 0; }

/* Capacity: output is capped near 1024 characters. The decompiled Win10 code returns 1024 and terminates at index 1024;
 * the check below accepts either 1023 or 1024 so only the cap itself, termination and no overrun are hard requirements. */
static void check_capped(const char *what, const char *out, int r, int expect_min) {
    int len = (int)strlen(out);
    CHECKF(r >= expect_min && r <= 1024, "%s: ret %d", what, r);
    CHECKF(len == r || len == r - 1, "%s: strlen %d ret %d", what, len, r);
    CHECKF((unsigned char)out[1026] == 0x55 && (unsigned char)out[1500] == 0x55, "%s: wrote past the cap", what);
}

TEST(wsprintfA_limit_1024_literal) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    static const int sizes[] = { 1000, 1022, 1023, 1024, 1025, 1026, 1100, 1500, 2000 };
    for (unsigned k = 0; k < sizeof sizes / sizeof *sizes; k++) {
        int n = sizes[k];
        char *fmt = (char *)malloc(n + 1), *out = (char *)malloc(4096);
        REQUIRE(fmt && out);
        build_fmt(fmt, n, 'x');
        memset(out, 0x55, 4096);
        int r = pA(out, fmt);
        char what[32]; sprintf(what, "n=%d", n);
        if (n <= 1022) { CHECKF(r == n && (int)strlen(out) == n, "n=%d ret %d", n, r); }
        else check_capped(what, out, r, 1023);
        t_note("literal n=%d -> ret %d strlen %d", n, r, (int)strlen(out));
        free(fmt); free(out);
    }
}

TEST(wsprintfA_limit_1024_string_arg) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char *src = (char *)malloc(5001), *out = (char *)malloc(8192);
    REQUIRE(src && out);
    build_fmt(src, 5000, 's');
    memset(out, 0x55, 8192);
    int r = pA(out, "%s", src);
    check_capped("%s 5000", out, r, 1023);
    memset(out, 0x55, 8192);
    r = pA(out, "ab%scd", src);
    check_capped("ab%scd", out, r, 1023);
    CHECK(out[0] == 'a' && out[1] == 'b' && out[2] == 's');
    memset(out, 0x55, 8192);
    r = pA(out, "%s", src + 4000);   /* 1000 chars: fits */
    CHECK(r == 1000);
    CHECK((int)strlen(out) == 1000);
    free(src); free(out);
}

TEST(wsprintfA_limit_1024_number_near_end) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char *fmt = (char *)malloc(1100), *out = (char *)malloc(4096);
    REQUIRE(fmt && out);
    for (int pre = 1000; pre <= 1030; pre++) {
        memset(fmt, 'p', pre);
        strcpy(fmt + pre, "%d");
        memset(out, 0x55, 4096);
        int r = pA(out, fmt, 1234567890);
        int len = (int)strlen(out);
        CHECKF(r <= 1024 && r >= (pre < 1023 ? pre : 1023), "pre=%d ret %d", pre, r);
        CHECKF(len == r || len == r - 1, "pre=%d strlen %d ret %d", pre, len, r);
        CHECKF((unsigned char)out[1026] == 0x55, "pre=%d overflow", pre);
        if (pre + 10 <= 1022) CHECKF(r == pre + 10 && !strcmp(out + pre, "1234567890"), "pre=%d full number [%s]", pre, out + pre);
    }
    free(fmt); free(out);
}

TEST(wsprintfA_limit_1024_width) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char *out = (char *)malloc(4096);
    REQUIRE(out);
    memset(out, 0x55, 4096);
    int r = pA(out, "%2000d", 5);
    t_note("wsprintfA(\"%%2000d\") ret %d strlen %d", r, (int)strlen(out));
    CHECK(r <= 1024);
    CHECK(strlen(out) <= 1024);
    CHECK((unsigned char)out[1026] == 0x55);
    memset(out, 0x55, 4096);
    r = pA(out, "%-1500s|", "ab");
    CHECK(r <= 1024);
    CHECK(strlen(out) <= 1024);
    CHECK((unsigned char)out[1026] == 0x55);
    free(out);
}

TEST(wsprintfA_width_precision_digits_parsing) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("          7", "%11d", 7);
    FA("00000000007", "%011d", 7);
    FA("7          |", "%-11d|", 7);
    FA("   7", "%4d", 7);
    FA("0007", "%04d", 7);
    FA("0000000007", "%010d", 7);
    FA("   7", "%4.1d", 7);
}

TEST(wsprintfA_zero_flag_with_left_justify) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("42   |", "%-05d|", 42);
    FA("ff   |", "%-05x|", 255);
}

TEST(wsprintfA_zero_pad_strings_note) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    NA("%05s", "ab");
    NA("%05c", 'a');
    NA("%-05s|", "ab");
}

TEST(wsprintfA_zero_precision_zero_value_note) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    NA("[%.0d]", 0);
    NA("[%.0x]", 0);
    NA("[%.0u]", 5);
}

TEST(wsprintfA_numeric_boundaries_table) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    static const struct { int v; const char *d; const char *x; const char *u; } t[] = {
        { 0, "0", "0", "0" }, { 1, "1", "1", "1" }, { -1, "-1", "ffffffff", "4294967295" }, { 9, "9", "9", "9" }, { 10, "10", "a", "10" },
        { 99, "99", "63", "99" }, { 100, "100", "64", "100" }, { 255, "255", "ff", "255" }, { 256, "256", "100", "256" },
        { 32767, "32767", "7fff", "32767" }, { 32768, "32768", "8000", "32768" }, { 65535, "65535", "ffff", "65535" }, { 65536, "65536", "10000", "65536" },
        { 2147483647, "2147483647", "7fffffff", "2147483647" }, { (int)0x80000000, "-2147483648", "80000000", "2147483648" },
        { (int)0x80000001, "-2147483647", "80000001", "2147483649" }, { -256, "-256", "ffffff00", "4294967040" },
        { 1000000000, "1000000000", "3b9aca00", "1000000000" }, { -1000000000, "-1000000000", "c4653600", "3294967296" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        char b[64];
        pA(b, "%d", t[i].v); CHECKF(!strcmp(b, t[i].d), "%%d %d -> %s", t[i].v, b);
        pA(b, "%x", t[i].v); CHECKF(!strcmp(b, t[i].x), "%%x %d -> %s", t[i].v, b);
        pA(b, "%u", t[i].v); CHECKF(!strcmp(b, t[i].u), "%%u %d -> %s", t[i].v, b);
        pA(b, "%i", t[i].v); CHECKF(!strcmp(b, t[i].d), "%%i %d -> %s", t[i].v, b);
        pA(b, "%ld", t[i].v); CHECKF(!strcmp(b, t[i].d), "%%ld %d -> %s", t[i].v, b);
        pA(b, "%lu", t[i].v); CHECKF(!strcmp(b, t[i].u), "%%lu %d -> %s", t[i].v, b);
    }
}

TEST(wsprintfA_roundtrip_with_strtol) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    unsigned x = 12345u;
    for (int i = 0; i < 3000; i++) {
        x = x * 1664525u + 1013904223u;
        char b[64];
        pA(b, "%d", (int)x);
        CHECKF(strtol(b, NULL, 10) == (int)x, "%%d round trip %d [%s]", (int)x, b);
        pA(b, "%u", x);
        CHECKF(strtoul(b, NULL, 10) == x, "%%u round trip %u [%s]", x, b);
        pA(b, "%x", x);
        CHECKF(strtoul(b, NULL, 16) == x, "%%x round trip %x [%s]", x, b);
        pA(b, "%08X", x);
        CHECKF(strlen(b) == 8 && strtoul(b, NULL, 16) == x, "%%08X %x [%s]", x, b);
    }
}

TEST(wsprintfA_repeated_calls_stable) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[64];
    for (int i = 0; i < 20000; i++) {
        int r = pA(b, "%d:%s:%x", i, "v", i);
        char e[64]; sprintf(e, "%d:v:%x", i, i);
        if (strcmp(b, e) || r != (int)strlen(e)) { CHECKF(0, "iter %d got [%s] want [%s]", i, b, e); break; }
    }
    CHECK(1);
}

TEST(wsprintfA_does_not_modify_last_error_on_success) {
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    char b[64];
    SetLastError(0x1234);
    pA(b, "%d", 5);
    DWORD e = GetLastError();
    t_note("last error after wsprintfA = 0x%lx", e);
    CHECK(e == 0x1234);
}

TEST(wsprintfA_format_chars_after_percent_class) {
    /* any character that is not a recognised conversion is copied literally */
    REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    FA("a", "%a", 1);
    FA("b", "%b", 1);
    FA("k", "%k", 1);
    FA("y", "%y", 1);
    FA("A", "%A", 1);
    FA("B", "%B", 1);
    FA("D", "%D", 1);
    FA("U", "%U", 1);
    FA("~", "%~", 1);
    FA("@", "%@", 1);
    FA("!", "%!", 1);
}

/* ---------- wvsprintfA ---------- */

TEST(wvsprintfA_basic) {
    REQUIRE(U(wvsprintfA));
    char b[256];
    int r = call_wv(b, "%d|%s|%x|%c", -5, "str", 0xBEEF, 'q');
    CHECK(r == (int)strlen(b));
    EXPECT_STR(b, "-5|str|beef|q");
    r = call_wv(b, "");
    CHECK(r == 0); EXPECT_STR(b, "");
    r = call_wv(b, "%%");
    CHECK(r == 1); EXPECT_STR(b, "%");
    r = call_wv(b, "%08X", 0x1234abcd);
    CHECK(r == 8); EXPECT_STR(b, "1234ABCD");
    r = call_wv(b, "%-6d|", 12);
    CHECK(r == 7); EXPECT_STR(b, "12    |");
    r = call_wv(b, "%p", (void *)0x77);
    CHECK(r == 16); EXPECT_STR(b, "0000000000000077");
    r = call_wv(b, "%I64u", (ULONGLONG)18446744073709551615ULL);
    CHECK(r == 20); EXPECT_STR(b, "18446744073709551615");
}

TEST(wvsprintfA_limit) {
    REQUIRE(U(wvsprintfA));
    char *fmt = (char *)malloc(3000), *out = (char *)malloc(4096);
    REQUIRE(fmt && out);
    build_fmt(fmt, 2500, 'w');
    memset(out, 0x55, 4096);
    int r = call_wv(out, fmt);
    check_capped("wv 2500", out, r, 1023);
    free(fmt); free(out);
}

TEST(wvsprintfA_matches_wsprintfA_table) {
    REQUIRE(U(wvsprintfA)); REQUIRE(U(wsprintfA)); wsA_t pA = (wsA_t)U(wsprintfA);
    static const char *fmts[] = { "%d", "%5d", "%-5d|", "%05d", "%u", "%x", "%X", "%08x", "%#x", "%.4d", "%ld", "%lu", "%hd", "%i", "%c", "%3c", "%%", "%o", "%f", "%+d", "% d", "%5.3d" };
    static const int vals[] = { 0, 1, -1, 42, -42, 255, 65535, 0x7fffffff, (int)0x80000000, 123456789 };
    for (unsigned f = 0; f < sizeof fmts / sizeof *fmts; f++)
        for (unsigned v = 0; v < sizeof vals / sizeof *vals; v++) {
            char a[128], b[128];
            if (strchr(fmts[f], 'c') && (vals[v] & 0xff) == 0) continue;
            int ra = pA(a, fmts[f], vals[v]);
            int rb = call_wv(b, fmts[f], vals[v]);
            CHECKF(ra == rb && !strcmp(a, b), "fmt \"%s\" val %d: [%s]/%d vs [%s]/%d", fmts[f], vals[v], a, ra, b, rb);
            CHECKF(ra == (int)strlen(a), "fmt \"%s\" val %d ret", fmts[f], vals[v]);
        }
}
