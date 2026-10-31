/* OemToChar / CharToOem family and GetWindowModuleFileName. */
#include "t.h"

static int is_cp(UINT acp, UINT oem) { return GetACP() == acp && GetOEMCP() == oem; }

TEST(oem_ascii_roundtrip_A) {
    REQUIRE(U(OemToCharA)); REQUIRE(U(CharToOemA));
    char src[128], dst[128], back[128];
    int n = 0;
    for (int c = 0x20; c < 0x7F; c++) src[n++] = (char)c;
    src[n] = 0;
    memset(dst, 0x55, sizeof dst);
    CHECK(U(CharToOemA)(src, dst));
    EXPECT_STR(dst, src);
    CHECK(U(OemToCharA)(dst, back));
    EXPECT_STR(back, src);
    memset(dst, 0x55, sizeof dst);
    CHECK(U(OemToCharA)(src, dst));
    EXPECT_STR(dst, src);
}

TEST(oem_empty_string_A) {
    REQUIRE(U(OemToCharA)); REQUIRE(U(CharToOemA));
    char dst[8];
    memset(dst, 'x', sizeof dst);
    CHECK(U(CharToOemA)("", dst));
    CHECK(dst[0] == 0); CHECK(dst[1] == 'x');
    memset(dst, 'x', sizeof dst);
    CHECK(U(OemToCharA)("", dst));
    CHECK(dst[0] == 0); CHECK(dst[1] == 'x');
}

TEST(oem_terminator_written_A) {
    REQUIRE(U(OemToCharA)); REQUIRE(U(CharToOemA));
    char dst[8];
    memset(dst, 'x', sizeof dst);
    CHECK(U(CharToOemA)("abc", dst));
    CHECK(!memcmp(dst, "abc\0x", 5));
    memset(dst, 'x', sizeof dst);
    CHECK(U(OemToCharA)("abc", dst));
    CHECK(!memcmp(dst, "abc\0x", 5));
}

TEST(oem_cp437_1252_mapping_A) {
    REQUIRE(U(OemToCharA)); REQUIRE(U(CharToOemA));
    if (!is_cp(1252, 437)) { t_skip("needs ACP 1252 / OEM 437 (have %u/%u)", GetACP(), GetOEMCP()); return; }
    static const struct { unsigned char ansi, oem; } t[] = {
        { 0xC7, 0x80 }, { 0xFC, 0x81 }, { 0xE9, 0x82 }, { 0xE2, 0x83 }, { 0xE4, 0x84 }, { 0xE0, 0x85 }, { 0xE5, 0x86 }, { 0xE7, 0x87 },
        { 0xEA, 0x88 }, { 0xEB, 0x89 }, { 0xE8, 0x8A }, { 0xEF, 0x8B }, { 0xEE, 0x8C }, { 0xEC, 0x8D }, { 0xC4, 0x8E }, { 0xC5, 0x8F },
        { 0xC9, 0x90 }, { 0xE6, 0x91 }, { 0xC6, 0x92 }, { 0xF4, 0x93 }, { 0xF6, 0x94 }, { 0xF2, 0x95 }, { 0xFB, 0x96 }, { 0xF9, 0x97 },
        { 0xFF, 0x98 }, { 0xD6, 0x99 }, { 0xDC, 0x9A }, { 0xA2, 0x9B }, { 0xA3, 0x9C }, { 0xA5, 0x9D }, { 0xE1, 0xA0 }, { 0xED, 0xA1 },
        { 0xF3, 0xA2 }, { 0xFA, 0xA3 }, { 0xF1, 0xA4 }, { 0xD1, 0xA5 }, { 0xAA, 0xA6 }, { 0xBA, 0xA7 }, { 0xBF, 0xA8 }, { 0xAC, 0xAA },
        { 0xBD, 0xAB }, { 0xBC, 0xAC }, { 0xA1, 0xAD }, { 0xAB, 0xAE }, { 0xBB, 0xAF }, { 0xDF, 0xE1 }, { 0xB5, 0xE6 }, { 0xB1, 0xF1 },
        { 0xB0, 0xF8 }, { 0xB7, 0xFA }, { 0xB2, 0xFD },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        char s[2] = { (char)t[i].ansi, 0 }, d[2] = { 0, 0 };
        CHECKF(U(CharToOemA)(s, d), "CharToOemA");
        CHECKF((unsigned char)d[0] == t[i].oem, "ANSI 0x%02x -> OEM 0x%02x, want 0x%02x", t[i].ansi, (unsigned char)d[0], t[i].oem);
        s[0] = (char)t[i].oem; d[0] = 0;
        CHECKF(U(OemToCharA)(s, d), "OemToCharA");
        CHECKF((unsigned char)d[0] == t[i].ansi, "OEM 0x%02x -> ANSI 0x%02x, want 0x%02x", t[i].oem, (unsigned char)d[0], t[i].ansi);
    }
}

TEST(oem_cp437_to_unicode_W) {
    REQUIRE(U(OemToCharW));
    if (GetOEMCP() != 437) { t_skip("OEM CP %u", GetOEMCP()); return; }
    static const struct { unsigned char oem; WCHAR wc; } t[] = {
        { 0x80, 0x00C7 }, { 0x81, 0x00FC }, { 0x82, 0x00E9 }, { 0x9B, 0x00A2 }, { 0xA4, 0x00F1 }, { 0xB0, 0x2591 }, { 0xB1, 0x2592 }, { 0xB2, 0x2593 },
        { 0xB3, 0x2502 }, { 0xBA, 0x2551 }, { 0xC4, 0x2500 }, { 0xCD, 0x2550 }, { 0xDB, 0x2588 }, { 0xE1, 0x00DF }, { 0xE3, 0x03C0 }, { 0xE4, 0x03A3 },
        { 0xE9, 0x0398 }, { 0xEB, 0x03B4 }, { 0xEC, 0x221E }, { 0xF1, 0x00B1 }, { 0xF8, 0x00B0 }, { 0xFB, 0x221A }, { 0xFE, 0x25A0 }, { 0xFF, 0x00A0 },
        { 0x41, 'A' }, { 0x7A, 'z' }, { 0x20, ' ' },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        char s[2] = { (char)t[i].oem, 0 }; WCHAR d[2] = { 0x5555, 0x5555 };
        CHECKF(U(OemToCharW)(s, d), "OemToCharW");
        CHECKF(d[0] == t[i].wc, "OEM 0x%02x -> U+%04x, want U+%04x", t[i].oem, d[0], t[i].wc);
        CHECKF(d[1] == 0, "terminator");
    }
}

TEST(oem_charToOemW_ascii_and_cp437) {
    REQUIRE(U(CharToOemW));
    char d[16];
    memset(d, 0x55, sizeof d);
    CHECK(U(CharToOemW)(L"Hello, World 123", d));
    EXPECT_STR(d, "Hello, World 123");
    memset(d, 0x55, sizeof d);
    CHECK(U(CharToOemW)(L"", d));
    CHECK(d[0] == 0);
    if (GetOEMCP() == 437) {
        CHECK(U(CharToOemW)(L"éüß±", d));
        CHECK((unsigned char)d[0] == 0x82 && (unsigned char)d[1] == 0x81 && (unsigned char)d[2] == 0xE1 && (unsigned char)d[3] == 0xF1 && d[4] == 0);
        CHECK(U(CharToOemW)(L"π≤", d));
        CHECK((unsigned char)d[0] == 0xE3);
        t_note("CharToOemW(U+2264) -> 0x%02x", (unsigned char)d[1]);
    }
}

TEST(oem_buff_zero_length) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA)); REQUIRE(U(OemToCharBuffW)); REQUIRE(U(CharToOemBuffW));
    char d[8]; WCHAR w[8];
    memset(d, 0x55, sizeof d);
    CHECK(U(OemToCharBuffA)("abc", d, 0));
    CHECK((unsigned char)d[0] == 0x55);
    CHECK(U(CharToOemBuffA)("abc", d, 0));
    CHECK((unsigned char)d[0] == 0x55);
    for (int i = 0; i < 8; i++) w[i] = 0x5555;
    CHECK(U(OemToCharBuffW)("abc", w, 0));
    CHECK(w[0] == 0x5555);
    memset(d, 0x55, sizeof d);
    CHECK(U(CharToOemBuffW)(L"abc", d, 0));
    CHECK((unsigned char)d[0] == 0x55);
}

TEST(oem_buff_exact_length_no_terminator) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA)); REQUIRE(U(OemToCharBuffW)); REQUIRE(U(CharToOemBuffW));
    char d[8]; WCHAR w[8];
    memset(d, 0x55, sizeof d);
    CHECK(U(CharToOemBuffA)("abcdef", d, 3));
    CHECK(!memcmp(d, "abc", 3)); CHECK((unsigned char)d[3] == 0x55);
    memset(d, 0x55, sizeof d);
    CHECK(U(OemToCharBuffA)("abcdef", d, 4));
    CHECK(!memcmp(d, "abcd", 4)); CHECK((unsigned char)d[4] == 0x55);
    for (int i = 0; i < 8; i++) w[i] = 0x5555;
    CHECK(U(OemToCharBuffW)("abcdef", w, 2));
    CHECK(w[0] == 'a' && w[1] == 'b' && w[2] == 0x5555);
    memset(d, 0x55, sizeof d);
    CHECK(U(CharToOemBuffW)(L"abcdef", d, 5));
    CHECK(!memcmp(d, "abcde", 5)); CHECK((unsigned char)d[5] == 0x55);
}

TEST(oem_buff_length_one) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA));
    char d[4];
    memset(d, 0x55, sizeof d);
    CHECK(U(CharToOemBuffA)("Q", d, 1));
    CHECK(d[0] == 'Q'); CHECK((unsigned char)d[1] == 0x55);
    memset(d, 0x55, sizeof d);
    CHECK(U(OemToCharBuffA)("Q", d, 1));
    CHECK(d[0] == 'Q'); CHECK((unsigned char)d[1] == 0x55);
}

TEST(oem_buff_embedded_nul_converted) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA)); REQUIRE(U(OemToCharBuffW)); REQUIRE(U(CharToOemBuffW));
    char d[8]; WCHAR w[8];
    static const char s[] = { 'a', 0, 'b', 0, 'c' };
    memset(d, 0x55, sizeof d);
    CHECK(U(CharToOemBuffA)(s, d, 5));
    CHECK(!memcmp(d, s, 5));
    memset(d, 0x55, sizeof d);
    CHECK(U(OemToCharBuffA)(s, d, 5));
    CHECK(!memcmp(d, s, 5));
    for (int i = 0; i < 8; i++) w[i] = 0x5555;
    CHECK(U(OemToCharBuffW)(s, w, 5));
    CHECK(w[0] == 'a' && w[1] == 0 && w[2] == 'b' && w[3] == 0 && w[4] == 'c' && w[5] == 0x5555);
    static const WCHAR ws[] = { 'a', 0, 'b' };
    memset(d, 0x55, sizeof d);
    CHECK(U(CharToOemBuffW)(ws, d, 3));
    CHECK(d[0] == 'a' && d[1] == 0 && d[2] == 'b' && (unsigned char)d[3] == 0x55);
}

TEST(oem_buff_in_place) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA));
    char b[16];
    strcpy(b, "in place 123");
    CHECK(U(CharToOemBuffA)(b, b, 12));
    EXPECT_STR(b, "in place 123");
    CHECK(U(OemToCharBuffA)(b, b, 12));
    EXPECT_STR(b, "in place 123");
}

TEST(oem_buff_ascii_all_printable_A_W) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA)); REQUIRE(U(OemToCharBuffW)); REQUIRE(U(CharToOemBuffW));
    char s[96], d[96]; WCHAR w[96], ws[96];
    for (int i = 0; i < 95; i++) { s[i] = (char)(0x20 + i); ws[i] = (WCHAR)(0x20 + i); }
    CHECK(U(CharToOemBuffA)(s, d, 95));
    CHECK(!memcmp(d, s, 95));
    CHECK(U(OemToCharBuffA)(s, d, 95));
    CHECK(!memcmp(d, s, 95));
    CHECK(U(OemToCharBuffW)(s, w, 95));
    CHECK(!memcmp(w, ws, 95 * sizeof(WCHAR)));
    CHECK(U(CharToOemBuffW)(ws, d, 95));
    CHECK(!memcmp(d, s, 95));
}

TEST(oem_controls_pass_through) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA));
    char s[32], d[32];
    for (int i = 0; i < 32; i++) s[i] = (char)(i + 1);
    CHECK(U(CharToOemBuffA)(s, d, 31));
    for (int i = 0; i < 31; i++) CHECKF((unsigned char)d[i] == i + 1, "ctrl %d -> %d (ANSI->OEM)", i + 1, (unsigned char)d[i]);
    CHECK(U(OemToCharBuffA)(s, d, 31));
    for (int i = 0; i < 31; i++) CHECKF((unsigned char)d[i] == i + 1, "ctrl %d -> %d (OEM->ANSI)", i + 1, (unsigned char)d[i]);
}

TEST(oem_roundtrip_all_bytes_note) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA));
    unsigned char s[256], m[256], r[256];
    for (int i = 0; i < 256; i++) s[i] = (unsigned char)i;
    CHECK(U(CharToOemBuffA)((char *)s, (char *)m, 256));
    CHECK(U(OemToCharBuffA)((char *)m, (char *)r, 256));
    unsigned lossy = 0;
    for (int i = 0; i < 256; i++) if (r[i] != s[i]) lossy++;
    t_note("ANSI->OEM->ANSI changes %u of 256 byte values (ACP %u OEM %u)", lossy, GetACP(), GetOEMCP());
    for (int i = 0; i < 0x80; i++) CHECKF(r[i] == s[i], "ASCII byte %d changed to %d", i, r[i]);
}

TEST(oem_buffW_all_oem_bytes_nonzero) {
    REQUIRE(U(OemToCharBuffW));
    unsigned char s[255]; WCHAR w[255];
    for (int i = 0; i < 255; i++) s[i] = (unsigned char)(i + 1);
    CHECK(U(OemToCharBuffW)((char *)s, w, 255));
    unsigned zero = 0;
    for (int i = 0; i < 255; i++) if (w[i] == 0) zero++;
    CHECK(zero == 0);
    for (int i = 0x1F; i < 0x7E; i++) CHECKF(w[i] == (WCHAR)(i + 1), "ASCII %d -> U+%04x", i + 1, w[i]);
}

TEST(oem_return_value_is_true) {
    REQUIRE(U(OemToCharA)); REQUIRE(U(CharToOemA)); REQUIRE(U(OemToCharW)); REQUIRE(U(CharToOemW));
    char d[64]; WCHAR w[64];
    CHECK(U(OemToCharA)("x", d) == TRUE);
    CHECK(U(CharToOemA)("x", d) == TRUE);
    CHECK(U(OemToCharW)("x", w) == TRUE);
    CHECK(U(CharToOemW)(L"x", d) == TRUE);
}

TEST(oem_long_string) {
    REQUIRE(U(OemToCharBuffA)); REQUIRE(U(CharToOemBuffA));
    char *s = (char *)malloc(300001), *d = (char *)malloc(300001);
    REQUIRE(s && d);
    for (int i = 0; i < 300000; i++) s[i] = (char)('a' + (i % 26));
    s[300000] = 0;
    CHECK(U(CharToOemBuffA)(s, d, 300000));
    CHECK(!memcmp(s, d, 300000));
    CHECK(U(OemToCharBuffA)(s, d, 300000));
    CHECK(!memcmp(s, d, 300000));
    free(s); free(d);
}

/* ---------- GetWindowModuleFileName ---------- */

static LRESULT CALLBACK sm_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }
static const char *SMCLS = "SMOemTestClass";

static HWND mk(HINSTANCE inst) {
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = sm_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = SMCLS;
    U(RegisterClassA)(&wc);
    return U(CreateWindowExA)(0, SMCLS, "t", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, inst, NULL);
}
static void rm(HWND h) {
    if (h) U(DestroyWindow)(h);
    U(UnregisterClassA)(SMCLS, GetModuleHandleA(NULL));
}

TEST(winmodfile_own_window_A) {
    REQUIRE(U(GetWindowModuleFileNameA)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(RegisterClassA));
    HWND h = mk(GetModuleHandleA(NULL));
    REQUIRE(h);
    char a[MAX_PATH], b[MAX_PATH];
    UINT r = U(GetWindowModuleFileNameA)(h, a, MAX_PATH);
    DWORD e = GetModuleFileNameA(GetModuleHandleA(NULL), b, MAX_PATH);
    CHECK(r == e);
    CHECK(r > 0);
    EXPECT_STR(a, b);
    rm(h);
}

TEST(winmodfile_own_window_W) {
    REQUIRE(U(GetWindowModuleFileNameW)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(RegisterClassA));
    HWND h = mk(GetModuleHandleA(NULL));
    REQUIRE(h);
    WCHAR a[MAX_PATH], b[MAX_PATH];
    UINT r = U(GetWindowModuleFileNameW)(h, a, MAX_PATH);
    DWORD e = GetModuleFileNameW(GetModuleHandleA(NULL), b, MAX_PATH);
    CHECK(r == e); CHECK(r > 0);
    EXPECT_WSTR(a, b);
    rm(h);
}

TEST(winmodfile_AW_parity) {
    REQUIRE(U(GetWindowModuleFileNameA)); REQUIRE(U(GetWindowModuleFileNameW)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(RegisterClassA));
    HWND h = mk(GetModuleHandleA(NULL));
    REQUIRE(h);
    char a[MAX_PATH], c[MAX_PATH]; WCHAR w[MAX_PATH];
    UINT ra = U(GetWindowModuleFileNameA)(h, a, MAX_PATH), rw = U(GetWindowModuleFileNameW)(h, w, MAX_PATH);
    WideCharToMultiByte(CP_ACP, 0, w, -1, c, MAX_PATH, NULL, NULL);
    CHECK(ra == rw); CHECK(!strcmp(a, c));
    rm(h);
}

TEST(winmodfile_other_instance_handle) {
    REQUIRE(U(GetWindowModuleFileNameA)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(RegisterClassA));
    HWND h = mk((HINSTANCE)t_user32());
    REQUIRE(h);
    char a[MAX_PATH], b[MAX_PATH];
    UINT r = U(GetWindowModuleFileNameA)(h, a, MAX_PATH);
    DWORD e = GetModuleFileNameA(t_user32(), b, MAX_PATH);
    t_note("window created with hInstance=user32: GetWindowModuleFileName=%u [%s]", r, a);
    CHECK(r == e);
    EXPECT_STR(a, b);
    rm(h);
}

TEST(winmodfile_small_buffers) {
    REQUIRE(U(GetWindowModuleFileNameA)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(RegisterClassA));
    HWND h = mk(GetModuleHandleA(NULL));
    REQUIRE(h);
    char full[MAX_PATH], a[MAX_PATH];
    UINT len = U(GetWindowModuleFileNameA)(h, full, MAX_PATH);
    CHECK(len > 3);
    memset(a, 'x', sizeof a);
    UINT r = U(GetWindowModuleFileNameA)(h, a, 0);
    CHECK(r == 0);
    CHECK(a[0] == 'x');
    memset(a, 'x', sizeof a);
    r = U(GetWindowModuleFileNameA)(h, a, 1);
    t_note("cch=1 -> ret %u a[0]=%d", r, a[0]);
    CHECK(r <= 1);
    memset(a, 'x', sizeof a);
    r = U(GetWindowModuleFileNameA)(h, a, len);
    t_note("cch=len -> ret %u (len %u)", r, len);
    CHECK(r == len || r == len - 1);
    CHECK(!strncmp(a, full, len - 1));
    memset(a, 'x', sizeof a);
    r = U(GetWindowModuleFileNameA)(h, a, len + 1);
    CHECK(r == len);
    CHECK(!strcmp(a, full));
    rm(h);
}

TEST(winmodfile_invalid_window_handles) {
    REQUIRE(U(GetWindowModuleFileNameA)); REQUIRE(U(GetWindowModuleFileNameW));
    char a[MAX_PATH]; WCHAR w[MAX_PATH];
    HWND bad[] = { NULL, (HWND)(ULONG_PTR)1, (HWND)(ULONG_PTR)0xDEAD, (HWND)(ULONG_PTR)0xFFFFFFFF, (HWND)(ULONG_PTR)0x7FFFFFFF, (HWND)(ULONG_PTR)0x12345678 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        a[0] = 'x'; SetLastError(0);
        UINT r = U(GetWindowModuleFileNameA)(bad[i], a, MAX_PATH);
        DWORD e = GetLastError();
        CHECKF(r == 0, "A hwnd %p -> %u", bad[i], r);
        CHECKF(e == ERROR_INVALID_WINDOW_HANDLE, "A hwnd %p err %lu", bad[i], e);
        w[0] = 'x'; SetLastError(0);
        r = U(GetWindowModuleFileNameW)(bad[i], w, MAX_PATH);
        e = GetLastError();
        CHECKF(r == 0, "W hwnd %p -> %u", bad[i], r);
        CHECKF(e == ERROR_INVALID_WINDOW_HANDLE, "W hwnd %p err %lu", bad[i], e);
    }
}

TEST(winmodfile_destroyed_window) {
    REQUIRE(U(GetWindowModuleFileNameA)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(RegisterClassA));
    HWND h = mk(GetModuleHandleA(NULL));
    REQUIRE(h);
    char a[MAX_PATH];
    CHECK(U(GetWindowModuleFileNameA)(h, a, MAX_PATH) > 0);
    U(DestroyWindow)(h);
    SetLastError(0);
    UINT r = U(GetWindowModuleFileNameA)(h, a, MAX_PATH);
    CHECK(r == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    rm(NULL);
}

TEST(winmodfile_desktop_and_shell_note) {
    REQUIRE(U(GetWindowModuleFileNameA)); REQUIRE(U(GetDesktopWindow));
    char a[MAX_PATH];
    SetLastError(0);
    UINT r = U(GetWindowModuleFileNameA)(U(GetDesktopWindow)(), a, MAX_PATH);
    t_note("GetWindowModuleFileNameA(desktop) = %u [%s] err %lu", r, r ? a : "", GetLastError());
    if (U(GetShellWindow) && U(GetShellWindow)()) {
        SetLastError(0);
        r = U(GetWindowModuleFileNameA)(U(GetShellWindow)(), a, MAX_PATH);
        t_note("GetWindowModuleFileNameA(shell) = %u [%s] err %lu", r, r ? a : "", GetLastError());
    }
}

TEST(winmodfile_export_aliases) {
    /* both the unsuffixed ordinal-style alias and the A name exist in the decompiled module */
    CHECK(t_proc("GetWindowModuleFileNameA") != NULL);
    CHECK(t_proc("GetWindowModuleFileNameW") != NULL);
    t_note("GetWindowModuleFileName (unsuffixed) export present: %d", t_proc("GetWindowModuleFileName") != NULL);
}
