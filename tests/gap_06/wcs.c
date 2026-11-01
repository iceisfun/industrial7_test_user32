/* gap6: WCSToMBEx (internal wide->multibyte helper). Signature verified from
 * the Win10 decompilation:
 *   int WCSToMBEx(WORD cp, LPCWSTR s, int cchW, char **out, int cbOut, BOOL alloc)
 * cp 0 or ACP uses the NLS table path, other codepages WideCharToMultiByte. */
#include "t.h"

typedef int (WINAPI *WCSToMBEx_pf)(WORD, LPCWSTR, int, char **, int, int);
static int on_wine(void) { return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL; }
#define F() WCSToMBEx_pf f = (WCSToMBEx_pf)t_proc("WCSToMBEx"); REQUIRE(f); if (on_wine()) { t_skip("wine stub aborts the process"); return; }
static const WCHAR abc[] = L"abc";

TEST(gap6_wcs_resolve) {
    F();
    CHECK(f != NULL);
}
TEST(gap6_wcs_basic_cp0) {
    F();
    char buf[16]; char *p = buf;
    memset(buf, 'Z', sizeof buf);
    int r = f(0, abc, 3, &p, 16, 0);
    CHECK(r == 3);
    CHECK(!memcmp(buf, "abc", 3));
    CHECK(buf[3] == 'Z'); /* no terminator when cch excludes it */
    CHECK(p == buf);
}
TEST(gap6_wcs_includes_nul_with_minus1) {
    F();
    char buf[16]; char *p = buf;
    memset(buf, 'Z', sizeof buf);
    int r = f(0, abc, -1, &p, 16, 0);
    CHECK(r == 4);
    CHECK(!strcmp(buf, "abc"));
}
TEST(gap6_wcs_acp_explicit) {
    F();
    char buf[16]; char *p = buf;
    int r = f((WORD)GetACP(), abc, -1, &p, 16, 0);
    CHECK(r == 4);
    CHECK(!strcmp(buf, "abc"));
}
TEST(gap6_wcs_utf8_ascii) {
    F();
    char buf[16]; char *p = buf;
    int r = f(65001, abc, -1, &p, 16, 0);
    CHECK(r == 4);
    CHECK(!strcmp(buf, "abc"));
}
TEST(gap6_wcs_codepages_ascii) {
    F();
    WORD cps[] = { 437, 850, 1252, 1250, 65001, 20127, 28591 };
    for (unsigned i = 0; i < sizeof cps / sizeof *cps; i++) {
        char buf[16] = {0}; char *p = buf;
        int r = f(cps[i], L"Hello", -1, &p, 16, 0);
        CHECKF(r == 6, "cp %u -> %d", cps[i], r);
        CHECKF(!strcmp(buf, "Hello"), "cp %u data", cps[i]);
    }
}
TEST(gap6_wcs_utf8_nonascii) {
    F();
    if (GetACP() == 65001) { t_skip("ACP is UTF-8"); return; }
    char buf[16] = {0}; char *p = buf;
    int r = f(65001, L"\x00e9", 1, &p, 16, 0);
    CHECK(r == 2);
    CHECK((unsigned char)buf[0] == 0xc3 && (unsigned char)buf[1] == 0xa9);
    char b2[16] = {0}; p = b2;
    r = f(65001, L"\x20ac", 1, &p, 16, 0);
    CHECK(r == 3);
    CHECK((unsigned char)b2[0] == 0xe2 && (unsigned char)b2[1] == 0x82 && (unsigned char)b2[2] == 0xac);
}
TEST(gap6_wcs_cp1252_nonascii) {
    F();
    char buf[16] = {0}; char *p = buf;
    int r = f(1252, L"\x00e9\x20ac", 2, &p, 16, 0);
    CHECK(r == 2);
    CHECK((unsigned char)buf[0] == 0xe9 && (unsigned char)buf[1] == 0x80);
}
TEST(gap6_wcs_zero_inputs) {
    F();
    char buf[16]; char *p = buf;
    CHECK(f(0, abc, 0, &p, 16, 0) == 0);   /* cch == 0 */
    CHECK(f(0, abc, 3, &p, 0, 0) == 0);    /* cb == 0 */
    CHECK(f(0, NULL, 3, &p, 16, 0) == 0);  /* NULL string */
    CHECK(f(0, NULL, -1, &p, 16, 0) == 0);
    CHECK(f(0, abc, 0, &p, 0, 0) == 0);
    CHECK(f(65001, abc, 0, &p, 16, 0) == 0);
    CHECK(f(65001, NULL, 3, &p, 16, 0) == 0);
    CHECK(f(65001, abc, 3, &p, 0, 0) == 0);
}
TEST(gap6_wcs_negative_lengths) {
    F();
    char buf[16]; char *p = buf;
    int cch[] = { -2, -3, -100, 0x80000000 };
    for (int i = 0; i < 4; i++) {
        CHECKF(f(0, abc, cch[i], &p, 16, 0) == 0, "cch %d", cch[i]);
        CHECKF(f(65001, abc, cch[i], &p, 16, 0) == 0, "cch %d utf8", cch[i]);
    }
}
TEST(gap6_wcs_negative_cb) {
    F();
    char buf[16]; char *p = buf;
    int cb[] = { -2, -3, -100, (int)0x80000000 };
    for (int i = 0; i < 4; i++) CHECKF(f(0, abc, 3, &p, cb[i], 0) == 0, "cb %d", cb[i]);
    /* cb == -1 means "size it for me" and requires allocation */
    CHECK(f(0, abc, 3, &p, -1, 0) == 0);
    CHECK(f(65001, abc, 3, &p, -1, 0) == 0);
}
TEST(gap6_wcs_truncation) {
    F();
    char buf[16]; char *p = buf;
    memset(buf, 'Z', sizeof buf);
    if (GetACP() == 65001) {
        int r = f(0, abc, 3, &p, 2, 0);
        CHECK(r == 2 || r == 0);
    } else {
        int r = f(0, abc, 3, &p, 2, 0); /* NLS path: overflow tolerated, bytes written returned */
        CHECK(r == 2);
        CHECK(buf[0] == 'a' && buf[1] == 'b' && buf[2] == 'Z');
        memset(buf, 'Z', sizeof buf);
        r = f(65001, abc, 3, &p, 2, 0); /* WideCharToMultiByte path: insufficient buffer fails */
        CHECK(r == 0);
    }
}
TEST(gap6_wcs_exact_and_minus_one_buffer) {
    F();
    for (int pass = 0; pass < 2; pass++) {
        WORD cp = pass ? 65001 : 0;
        char buf[8]; char *p = buf;
        memset(buf, 'Z', sizeof buf);
        int r = f(cp, abc, 3, &p, 3, 0);   /* exact */
        CHECKF(r == 3, "cp %u exact -> %d", cp, r);
        CHECK(!memcmp(buf, "abc", 3) && buf[3] == 'Z');
        memset(buf, 'Z', sizeof buf);
        r = f(cp, abc, 3, &p, 4, 0);       /* exact + 1 */
        CHECK(r == 3);
        r = f(cp, abc, -1, &p, 4, 0);      /* exact with NUL */
        CHECKF(r == 4, "cp %u nul exact -> %d", cp, r);
        CHECK(!strcmp(buf, "abc"));
        r = f(cp, abc, 1, &p, 1, 0);
        CHECK(r == 1 && buf[0] == 'a');
    }
}
TEST(gap6_wcs_invalid_codepage) {
    F();
    /* 1/2/3 are CP_OEMCP/CP_MACCP/CP_THREAD_ACP: valid pseudo code pages on Win10. */
    WORD bad[] = { 99, 4000, 12345, 0xffff, 50000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        if (bad[i] == GetACP()) continue;
        char buf[16]; char *p = buf;
        int r = f(bad[i], abc, 3, &p, 16, 0);
        CHECKF(r == 0, "cp %u -> %d", bad[i], r);
    }
}
TEST(gap6_wcs_alloc_basic) {
    F();
    char *p = NULL;
    int r = f(0, abc, 3, &p, 16, 1);
    CHECK(r == 3);
    CHECK(p != NULL);
    if (p) CHECK(!memcmp(p, "abc", 3));
    /* the block lives on user32's private heap; it is not freed here */
}
TEST(gap6_wcs_alloc_autosize) {
    F();
    char *p = NULL;
    int r = f(0, abc, -1, &p, -1, 1);   /* cb == -1: sized as cch * 2 */
    CHECK(r == 4);
    CHECK(p != NULL);
    if (p) CHECK(!strcmp(p, "abc"));
    char *q = NULL;
    r = f(65001, L"xyz", 3, &q, -1, 1);
    CHECK(r == 3);
    CHECK(q != NULL);
    if (q) CHECK(!memcmp(q, "xyz", 3));
}
TEST(gap6_wcs_alloc_failure_paths) {
    F();
    char *p = (char *)0x1234;
    CHECK(f(0, abc, 0, &p, 16, 1) == 0);
    CHECK(p == (char *)0x1234);        /* argument checks happen before allocation */
    CHECK(f(0, NULL, 3, &p, 16, 1) == 0);
    CHECK(p == (char *)0x1234);
    CHECK(f(0, abc, 3, &p, 0, 1) == 0);
    CHECK(p == (char *)0x1234);
    CHECK(f(0, abc, -5, &p, 16, 1) == 0);
    CHECK(p == (char *)0x1234);
    CHECK(f(0, abc, 3, &p, -7, 1) == 0);
    CHECK(p == (char *)0x1234);
}
TEST(gap6_wcs_alloc_bad_cp_frees) {
    F();
    char *p = NULL;
    int r = f(0xffff, abc, 3, &p, 16, 1);
    CHECK(r == 0);   /* conversion failure releases the block */
}
TEST(gap6_wcs_embedded_nul) {
    F();
    static const WCHAR s[] = { 'a', 0, 'b', 0 };
    char buf[16]; char *p = buf;
    memset(buf, 'Z', sizeof buf);
    int r = f(0, s, 3, &p, 16, 0);
    CHECK(r == 3);
    CHECK(buf[0] == 'a' && buf[1] == 0 && buf[2] == 'b' && buf[3] == 'Z');
    r = f(0, s, -1, &p, 16, 0);   /* stops at first NUL, includes it */
    CHECK(r == 2);
}
TEST(gap6_wcs_matches_widechar) {
    F();
    static const WCHAR s[] = L"The quick brown fox 0123456789";
    int n = (int)lstrlenW(s);
    char a[64] = {0}, b[64] = {0}; char *p = a;
    int r = f(65001, s, n + 1, &p, 64, 0);
    int e = WideCharToMultiByte(65001, 0, s, n + 1, b, 64, NULL, NULL);
    CHECK(r == e);
    CHECK(!strcmp(a, b));
}
TEST(gap6_wcs_long_input) {
    F();
    WCHAR *w = HeapAlloc(GetProcessHeap(), 0, 4097 * sizeof(WCHAR));
    char *c = HeapAlloc(GetProcessHeap(), 0, 4100);
    if (!w || !c) { t_skip("oom"); return; }
    for (int i = 0; i < 4096; i++) w[i] = 'a' + i % 26;
    w[4096] = 0;
    char *p = c;
    int r = f(0, w, -1, &p, 4100, 0);
    CHECK(r == 4097);
    CHECK(c[0] == 'a' && c[4095] == 'a' + 4095 % 26 && c[4096] == 0);
    HeapFree(GetProcessHeap(), 0, w); HeapFree(GetProcessHeap(), 0, c);
}
TEST(gap6_wcs_repeat) {
    F();
    for (int i = 0; i < 2000; i++) {
        char b[8]; char *p = b;
        if (f(0, abc, -1, &p, 8, 0) != 4) { CHECKF(0, "iter %d", i); return; }
    }
    CHECK(1);
}
