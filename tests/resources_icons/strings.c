#include "ri.h"

/* LoadStringA/W against a string table added to a private copy of this exe. */

#define SID(n) (RI_STR_BASE + (n))

TEST(resources_icons_ls_w_basic)
{
    WCHAR buf[64];
    int n;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    wmemset(buf, 0x7777, 64);
    n = U(LoadStringW)(m, SID(0), buf, 64);
    RI_EQ(n, 5);
    EXPECT_WSTR(buf, L"Hello");
    RI_EQ(buf[5], 0);
}

TEST(resources_icons_ls_w_unicode)
{
    WCHAR buf[64];
    int n;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    n = U(LoadStringW)(m, SID(2), buf, 64);
    RI_EQ(n, 7);
    RI_EQ(buf[3], 0xE9);
    RI_EQ(buf[4], 0x4E2D);
    RI_EQ(buf[5], 0x6587);
    RI_EQ(buf[6], L'!');
    RI_EQ(buf[7], 0);
}

TEST(resources_icons_ls_w_single_char)
{
    WCHAR buf[8];
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    RI_EQ(U(LoadStringW)(m, SID(3), buf, 8), 1);
    RI_EQ(buf[0], L'A');
    RI_EQ(buf[1], 0);
}

TEST(resources_icons_ls_w_other_block)
{
    WCHAR buf[8];
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    RI_EQ(U(LoadStringW)(m, SID(8), buf, 8), 1);
    RI_EQ(buf[0], L'Z');
}

TEST(resources_icons_ls_w_long)
{
    WCHAR buf[400];
    int n, i, bad = 0;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    n = U(LoadStringW)(m, SID(4), buf, 400);
    RI_EQ(n, 300);
    for (i = 0; i < 300; i++) if (buf[i] != L'a' + (i % 26)) bad++;
    RI_EQ(bad, 0);
    RI_EQ(buf[300], 0);
}

TEST(resources_icons_ls_w_buffer_edges)
{
    /* resource "Hello" (5 chars) with several buffer sizes */
    static const int sizes[] = {1, 2, 3, 5, 6, 7};
    static const int want[]  = {0, 1, 2, 4, 5, 5};
    WCHAR buf[16];
    int i;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    for (i = 0; i < 6; i++) {
        wmemset(buf, 0x7777, 16);
        int n = U(LoadStringW)(m, SID(0), buf, sizes[i]);
        CHECKF(n == want[i], "cch=%d got %d want %d", sizes[i], n, want[i]);
        CHECKF(buf[n] == 0, "cch=%d not NUL terminated at %d", sizes[i], n);
        if (n > 0) CHECKF(!memcmp(buf, L"Hello", n * 2), "cch=%d content", sizes[i]);
        CHECKF(buf[sizes[i]] == 0x7777, "cch=%d overran buffer", sizes[i]);
    }
}

TEST(resources_icons_ls_w_cch_zero_pointer)
{
    /* cchBufferMax == 0: lpBuffer receives a pointer to the read-only resource data */
    LPCWSTR ptr = NULL;
    int n;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    n = U(LoadStringW)(m, SID(0), (LPWSTR)&ptr, 0);
    RI_EQ(n, 5);
    CHECK(ptr != NULL);
    if (ptr && n == 5) {
        CHECK(!memcmp(ptr, L"Hello", 10));
    }
    ptr = NULL;
    n = U(LoadStringW)(m, SID(2), (LPWSTR)&ptr, 0);
    RI_EQ(n, 7);
    if (ptr && n == 7) RI_EQ(ptr[4], 0x4E2D);
    /* pointer trick does not NUL-terminate: next WORD is the next entry's length (1003 -> 1) */
    ptr = NULL;
    n = U(LoadStringW)(m, SID(3), (LPWSTR)&ptr, 0);
    RI_EQ(n, 1);
    if (ptr && n == 1) RI_EQ(ptr[0], L'A');
}

TEST(resources_icons_ls_w_cch_zero_pointer_stable)
{
    LPCWSTR a = NULL, b = NULL;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    U(LoadStringW)(m, SID(0), (LPWSTR)&a, 0);
    U(LoadStringW)(m, SID(0), (LPWSTR)&b, 0);
    CHECK(a != NULL);
    RI_EQ(a, b);
}

TEST(resources_icons_ls_w_embedded_nul)
{
    WCHAR buf[16];
    LPCWSTR ptr = NULL;
    int n;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    /* the length stored in the table is authoritative: embedded NUL is counted */
    n = U(LoadStringW)(m, SID(5), (LPWSTR)&ptr, 0);
    RI_EQ(n, 5);
    if (ptr && n == 5) { RI_EQ(ptr[2], 0); RI_EQ(ptr[3], L'c'); RI_EQ(ptr[4], L'd'); }
    n = U(LoadStringW)(m, SID(5), buf, 16);
    RI_EQ(n, 5);
    RI_EQ(buf[2], 0);
    RI_EQ(buf[3], L'c');
    RI_EQ(buf[5], 0);
}

TEST(resources_icons_ls_w_missing_id)
{
    WCHAR buf[16];
    int ids[] = {0, 1, 9999, 65535, SID(100)};
    int i;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    for (i = 0; i < 5; i++) {
        wmemset(buf, 0x7777, 16);
        SetLastError(0);
        CHECKF(U(LoadStringW)(m, ids[i], buf, 16) == 0, "id %d", ids[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_RESOURCE_NAME_NOT_FOUND, "id %d err %lu", ids[i], e);
    }
}

TEST(resources_icons_ls_w_missing_id_block_exists)
{
    /* ids in an existing block but with no string: 999 (block 63, empty), 1001, 1006, 1009 */
    static const int ids[] = {999, 1001, 1006, 1007, 1009, 1023};
    WCHAR buf[16];
    int i;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    for (i = 0; i < 6; i++) {
        wmemset(buf, 0x7777, 16);
        SetLastError(0);
        CHECKF(U(LoadStringW)(m, ids[i], buf, 16) == 0, "id %d", ids[i]);
    }
    /* the pointer variant: zero length, and the same id can be retried */
    {
        LPCWSTR p = (LPCWSTR)(ULONG_PTR)1;
        int n = U(LoadStringW)(m, 1001, (LPWSTR)&p, 0);
        RI_EQ(n, 0);
    }
}

TEST(resources_icons_ls_w_id_above_65535)
{
    /* uID is 32-bit but string ids are 16-bit; high bits do not alias in-range ids on current builds */
    WCHAR buf[16];
    int n;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    n = U(LoadStringW)(m, 0x10000 + SID(0), buf, 16);
    t_note("LoadStringW(id 0x1%04x) = %d", SID(0), n);
}

TEST(resources_icons_ls_w_bad_module)
{
    WCHAR buf[16];
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    SetLastError(0);
    /* A bogus (unmapped) HINSTANCE is not validated by Win10 22H2 LoadStringW: it faults in the resource
     * lookup (0xc0000005), so that probe is not part of the corpus. */
    /* a module with no string table of that id */
    RI_EQ(U(LoadStringW)(t_user32(), 0x7FFF, buf, 16), 0);
    EXPECT_ERR(0, ERROR_RESOURCE_NAME_NOT_FOUND);
}

TEST(resources_icons_ls_w_null_hinstance_note)
{
    WCHAR buf[16];
    int n;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    n = U(LoadStringW)(NULL, SID(0), buf, 16);
    /* NULL hInstance resolves to the process exe, which has no such string */
    RI_EQ(n, 0);
}

TEST(resources_icons_ls_w_repeat)
{
    WCHAR buf[16];
    int i, bad = 0;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    for (i = 0; i < 2000; i++) {
        if (U(LoadStringW)(m, SID(0), buf, 16) != 5 || lstrcmpW(buf, L"Hello")) bad++;
    }
    RI_EQ(bad, 0);
}

TEST(resources_icons_ls_a_basic)
{
    char buf[64];
    int n;
    REQUIRE(U(LoadStringA));
    REQUIRE_FIX(m);
    memset(buf, 0x77, sizeof buf);
    n = U(LoadStringA)(m, SID(0), buf, 64);
    RI_EQ(n, 5);
    EXPECT_STR(buf, "Hello");
    RI_EQ(buf[5], 0);
}

TEST(resources_icons_ls_a_buffer_edges)
{
    static const int sizes[] = {1, 2, 3, 5, 6, 7};
    static const int want[]  = {0, 1, 2, 4, 5, 5};
    char buf[16];
    int i;
    REQUIRE(U(LoadStringA));
    REQUIRE_FIX(m);
    for (i = 0; i < 6; i++) {
        memset(buf, 0x77, sizeof buf);
        int n = U(LoadStringA)(m, SID(0), buf, sizes[i]);
        CHECKF(n == want[i], "cch=%d got %d want %d", sizes[i], n, want[i]);
        CHECKF(buf[n] == 0, "cch=%d not NUL terminated", sizes[i]);
        CHECKF(!strncmp(buf, "Hello", n), "cch=%d content", sizes[i]);
        CHECKF(buf[sizes[i]] == 0x77, "cch=%d overran", sizes[i]);
    }
}

TEST(resources_icons_ls_a_missing_id)
{
    char buf[16];
    REQUIRE(U(LoadStringA));
    REQUIRE_FIX(m);
    memset(buf, 0x77, sizeof buf);
    SetLastError(0);
    RI_EQ(U(LoadStringA)(m, 9999, buf, 16), 0);
    EXPECT_ERR(0, ERROR_RESOURCE_NAME_NOT_FOUND);
    RI_EQ(buf[0], 0); /* LoadStringA always terminates the buffer */
}

TEST(resources_icons_ls_a_empty_string)
{
    char buf[16];
    REQUIRE(U(LoadStringA));
    REQUIRE_FIX(m);
    memset(buf, 0x77, sizeof buf);
    RI_EQ(U(LoadStringA)(m, SID(1), buf, 16), 0);
    RI_EQ(buf[0], 0);
}

TEST(resources_icons_ls_a_unicode_conversion)
{
    /* L"Uni\xe9\x4e2d\x6587!" converted with the ACP: length depends on code page */
    char buf[64];
    char want[64];
    WCHAR w[16];
    int n, wn;
    REQUIRE(U(LoadStringA));
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    n = U(LoadStringA)(m, SID(2), buf, 64);
    wn = U(LoadStringW)(m, SID(2), w, 16);
    RI_EQ(wn, 7);
    CHECK(n >= 4);
    CHECK(!strncmp(buf, "Uni", 3));
    {
        int c = WideCharToMultiByte(CP_ACP, 0, w, wn, want, sizeof want, NULL, NULL);
        RI_EQ(n, c);
        CHECK(!memcmp(buf, want, c));
    }
    RI_EQ(buf[n], 0);
}

TEST(resources_icons_ls_a_ascii_parity)
{
    char a[64]; WCHAR w[64]; char conv[64];
    int ids[] = {SID(0), SID(3), SID(4), SID(5), SID(8)};
    int i;
    REQUIRE(U(LoadStringA));
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    for (i = 0; i < 5; i++) {
        int na, nw;
        if (ids[i] == SID(4)) continue; /* 300 chars do not fit */
        na = U(LoadStringA)(m, ids[i], a, 64);
        nw = U(LoadStringW)(m, ids[i], w, 64);
        CHECKF(na == nw, "id %d: A=%d W=%d", ids[i], na, nw);
        WideCharToMultiByte(CP_ACP, 0, w, nw, conv, 64, NULL, NULL);
        CHECKF(!memcmp(a, conv, nw), "id %d content", ids[i]);
    }
}

TEST(resources_icons_ls_a_long_truncate)
{
    char buf[100];
    int n;
    REQUIRE(U(LoadStringA));
    REQUIRE_FIX(m);
    memset(buf, 0x77, sizeof buf);
    n = U(LoadStringA)(m, SID(4), buf, 50);
    RI_EQ(n, 49);
    RI_EQ(buf[49], 0);
    RI_EQ(buf[50], 0x77);
    RI_EQ(buf[0], 'a');
    RI_EQ(buf[48], 'a' + (48 % 26));
}

TEST(resources_icons_ls_a_embedded_nul)
{
    char buf[16];
    int n;
    REQUIRE(U(LoadStringA));
    REQUIRE_FIX(m);
    n = U(LoadStringA)(m, SID(5), buf, 16);
    RI_EQ(n, 5);
    RI_EQ(buf[2], 0);
    RI_EQ(buf[3], 'c');
}

TEST(resources_icons_ls_w_empty_string)
{
    WCHAR buf[8];
    int n;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    wmemset(buf, 0x7777, 8);
    SetLastError(0);
    n = U(LoadStringW)(m, SID(1), buf, 8);
    RI_EQ(n, 0);
    t_note("empty string: buf[0]=0x%x err=%lu", buf[0], GetLastError());
}

TEST(resources_icons_ls_w_failure_buffer_note)
{
    WCHAR buf[8];
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    wmemset(buf, 0x7777, 8);
    RI_EQ(U(LoadStringW)(m, 9999, buf, 8), 0);
    t_note("LoadStringW failure leaves buf[0]=0x%x", buf[0]);
}

TEST(resources_icons_ls_system_module)
{
    /* user32 ships many messages; scan a small range through both APIs for A/W parity */
    int id, found = 0, mismatch = 0;
    HMODULE u = t_user32();
    REQUIRE(U(LoadStringA));
    REQUIRE(U(LoadStringW));
    for (id = 0; id < 1200; id++) {
        char a[128]; WCHAR w[128]; char conv[128];
        int nw = U(LoadStringW)(u, id, w, 128);
        int na = U(LoadStringA)(u, id, a, 128);
        if (nw) {
            found++;
            if (nw < 100) {
                int c = WideCharToMultiByte(CP_ACP, 0, w, nw, conv, 128, NULL, NULL);
                if (na != c || memcmp(a, conv, c)) mismatch++;
            }
        } else if (na) mismatch++;
    }
    t_note("user32 string ids found in [0,1200): %d", found);
    RI_EQ(mismatch, 0);
}

TEST(resources_icons_ls_w_pointer_zero_missing)
{
    LPCWSTR p = (LPCWSTR)(ULONG_PTR)0x1234;
    REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    SetLastError(0);
    RI_EQ(U(LoadStringW)(m, 5000, (LPWSTR)&p, 0), 0);
    EXPECT_ERR(0, ERROR_RESOURCE_NAME_NOT_FOUND);
}
