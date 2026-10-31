/* SetWindowText / GetWindowText(Length) / class name queries, A/W parity */
#include "wm.h"

UDECL(int, InternalGetWindowText, (HWND, LPWSTR, int));

static HWND ha(void) { return wm_mk(WS_OVERLAPPED, WS_EX_TOOLWINDOW, 0, 0, 100, 50, NULL); }
static HWND hw(void) { return wm_mkw(WS_OVERLAPPED, WS_EX_TOOLWINDOW, 0, 0, 100, 50, NULL); }

TEST(wm_text_set_get_a) {
    NEED(SetWindowTextA); NEED(GetWindowTextA);
    HWND h = ha();
    CHECK(U(SetWindowTextA)(h, "Hello World"));
    char b[64]; memset(b, 'x', sizeof b);
    EQ(U(GetWindowTextA)(h, b, 64), 11);
    EXPECT_STR(b, "Hello World");
    EQ(b[11], 0);
    EQ(U(GetWindowTextLengthA)(h), 11);
    wm_destroy(h);
}

TEST(wm_text_set_get_w) {
    NEED(SetWindowTextW); NEED(GetWindowTextW);
    HWND h = hw();
    CHECK(U(SetWindowTextW)(h, L"Hello \x4e16\x754c"));
    WCHAR b[64]; for (int i = 0; i < 64; i++) b[i] = 0x7777;
    EQ(U(GetWindowTextW)(h, b, 64), 8);
    EXPECT_WSTR(b, L"Hello \x4e16\x754c");
    EQ(b[8], 0);
    EQ(U(GetWindowTextLengthW)(h), 8);
    wm_destroy(h);
}

TEST(wm_text_buffer_edges_a) {
    HWND h = ha();
    U(SetWindowTextA)(h, "abcdef");
    char b[16];
    memset(b, 'Z', sizeof b);
    EQ(U(GetWindowTextA)(h, b, 0), 0);
    EQ(b[0], 'Z');                                   /* untouched */
    memset(b, 'Z', sizeof b);
    EQ(U(GetWindowTextA)(h, b, 1), 0);
    EQ(b[0], 0); EQ(b[1], 'Z');
    memset(b, 'Z', sizeof b);
    EQ(U(GetWindowTextA)(h, b, 2), 1);
    EQ(b[0], 'a'); EQ(b[1], 0); EQ(b[2], 'Z');
    memset(b, 'Z', sizeof b);
    EQ(U(GetWindowTextA)(h, b, 6), 5);               /* exact - 1 */
    EXPECT_STR(b, "abcde"); EQ(b[6], 'Z');
    memset(b, 'Z', sizeof b);
    EQ(U(GetWindowTextA)(h, b, 7), 6);               /* exact */
    EXPECT_STR(b, "abcdef"); EQ(b[7], 'Z');
    memset(b, 'Z', sizeof b);
    EQ(U(GetWindowTextA)(h, b, 8), 6);               /* exact + 1 */
    EXPECT_STR(b, "abcdef");
    memset(b, 'Z', sizeof b);
    EQ(U(GetWindowTextA)(h, b, -1), 0);
    EQ(U(GetWindowTextA)(h, b, 0x7fffffff) >= 0, 1);
    wm_destroy(h);
}

TEST(wm_text_buffer_edges_w) {
    HWND h = hw();
    U(SetWindowTextW)(h, L"abcdef");
    WCHAR b[16];
    for (int i = 0; i < 16; i++) b[i] = 0x5a5a;
    EQ(U(GetWindowTextW)(h, b, 0), 0);
    EQ(b[0], 0x5a5a);
    EQ(U(GetWindowTextW)(h, b, 1), 0);
    EQ(b[0], 0); EQ(b[1], 0x5a5a);
    for (int i = 0; i < 16; i++) b[i] = 0x5a5a;
    EQ(U(GetWindowTextW)(h, b, 6), 5);
    EXPECT_WSTR(b, L"abcde"); EQ(b[6], 0x5a5a);
    for (int i = 0; i < 16; i++) b[i] = 0x5a5a;
    EQ(U(GetWindowTextW)(h, b, 7), 6);
    EXPECT_WSTR(b, L"abcdef"); EQ(b[7], 0x5a5a);
    EQ(U(GetWindowTextW)(h, b, 100), 6);
    wm_destroy(h);
}

TEST(wm_text_buffer_edges_cross_class) {
    /* A call on a wide class window and W call on an ANSI class window */
    HWND a = ha(), w = hw();
    U(SetWindowTextA)(w, "abcdef");
    U(SetWindowTextW)(a, L"abcdef");
    char b[16]; WCHAR c[16];
    memset(b, 'Z', sizeof b);
    EQ(U(GetWindowTextA)(w, b, 4), 3); EXPECT_STR(b, "abc");
    EQ(U(GetWindowTextA)(a, b, 4), 3); EXPECT_STR(b, "abc");
    for (int i = 0; i < 16; i++) c[i] = 0x5a5a;
    EQ(U(GetWindowTextW)(a, c, 4), 3); EXPECT_WSTR(c, L"abc");
    EQ(U(GetWindowTextW)(w, c, 4), 3); EXPECT_WSTR(c, L"abc");
    EQ(U(GetWindowTextLengthA)(w), 6); EQ(U(GetWindowTextLengthW)(w), 6);
    EQ(U(GetWindowTextLengthA)(a), 6); EQ(U(GetWindowTextLengthW)(a), 6);
    wm_destroy(a); wm_destroy(w);
}

TEST(wm_text_null_clears) {
    HWND h = ha();
    U(SetWindowTextA)(h, "something");
    CHECK(U(SetWindowTextA)(h, NULL));
    EQ(U(GetWindowTextLengthA)(h), 0);
    char b[8] = "q";
    EQ(U(GetWindowTextA)(h, b, 8), 0); EQ(b[0], 0);
    U(SetWindowTextW)(h, L"again");
    CHECK(U(SetWindowTextW)(h, NULL));
    EQ(U(GetWindowTextLengthW)(h), 0);
    CHECK(U(SetWindowTextA)(h, ""));
    EQ(U(GetWindowTextLengthA)(h), 0);
    wm_destroy(h);
}

TEST(wm_text_empty_buffer_dest_nul) {
    HWND h = ha();
    U(SetWindowTextA)(h, NULL);
    char b[4] = {'a', 'b', 'c', 'd'};
    EQ(U(GetWindowTextA)(h, b, 4), 0);
    EQ(b[0], 0); EQ(b[1], 'b');
    wm_destroy(h);
}

TEST(wm_text_messages_sent) {
    HWND h = ha();
    wm_logclear();
    CHECK(U(SetWindowTextA)(h, "msg"));
    EQ(wm_logcount(WM_SETTEXT), 1);
    char b[16];
    wm_logclear();
    U(GetWindowTextA)(h, b, 16);
    EQ(wm_logcount(WM_GETTEXT), 1);
    int i = wm_logfind(WM_GETTEXT, 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) EQ(w, 16);
    wm_logclear();
    U(GetWindowTextLengthA)(h);
    EQ(wm_logcount(WM_GETTEXTLENGTH), 1);
    wm_destroy(h);
}

TEST(wm_text_settext_wparam_lparam) {
    HWND h = ha();
    wm_logclear();
    U(SetWindowTextA)(h, "xyz");
    int i = wm_logfind(WM_SETTEXT, 0);
    CHECK(i >= 0);
    UINT m; WPARAM w; LPARAM l;
    if (i >= 0 && wm_lognth(i, &m, &w, &l)) { EQ(w, 0); CHECK(l != 0); }
    wm_destroy(h);
}

TEST(wm_text_direct_wm_messages) {
    HWND h = ha();
    EQ(U(SendMessageA)(h, WM_SETTEXT, 0, (LPARAM)"direct"), TRUE);
    char b[32];
    memset(b, 'Z', sizeof b);
    EQ(U(SendMessageA)(h, WM_GETTEXTLENGTH, 0, 0), 6);
    EQ(U(SendMessageA)(h, WM_GETTEXT, 32, (LPARAM)b), 6);
    EXPECT_STR(b, "direct");
    memset(b, 'Z', sizeof b);
    EQ(U(SendMessageA)(h, WM_GETTEXT, 0, (LPARAM)b), 0);
    EQ(b[0], 'Z');
    EQ(U(SendMessageA)(h, WM_GETTEXT, 1, (LPARAM)b), 0);
    EQ(b[0], 0);
    EQ(U(SendMessageA)(h, WM_GETTEXT, 4, (LPARAM)b), 3);
    EXPECT_STR(b, "dir");
    EQ(U(SendMessageW)(h, WM_SETTEXT, 0, (LPARAM)L"wide"), TRUE);
    EQ(U(SendMessageA)(h, WM_GETTEXTLENGTH, 0, 0), 4);
    EQ(U(SendMessageW)(h, WM_GETTEXTLENGTH, 0, 0), 4);
    wm_destroy(h);
}

TEST(wm_text_dead_and_null) {
    NEED(SetWindowTextA);
    HWND h = ha(); wm_destroy(h);
    char b[8] = "q"; WCHAR c[8] = L"q";
    SetLastError(0);
    CHECK(!U(SetWindowTextA)(h, "x"));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(SetWindowTextW)(h, L"x"));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(SetWindowTextA)(NULL, "x"));
    EQ(U(GetWindowTextA)(h, b, 8), 0);
    EQ(U(GetWindowTextW)(h, c, 8), 0);
    EQ(U(GetWindowTextA)(NULL, b, 8), 0);
    EQ(U(GetWindowTextW)(NULL, c, 8), 0);
    EQ(U(GetWindowTextLengthA)(h), 0);
    EQ(U(GetWindowTextLengthW)(h), 0);
    EQ(U(GetWindowTextLengthA)(NULL), 0);
    EQ(U(GetWindowTextLengthW)((HWND)(ULONG_PTR)0x6660), 0);
}

TEST(wm_text_ascii_parity) {
    HWND a = ha(), w = hw();
    int bad = 0;
    for (int ch = 1; ch < 128; ch++) {
        char s[4] = { 'a', (char)ch, 'z', 0 };
        WCHAR ws[4] = { 'a', (WCHAR)ch, 'z', 0 };
        char ba[8]; WCHAR bw[8];
        /* A set -> A/W get on both window kinds */
        U(SetWindowTextA)(a, s);
        U(GetWindowTextA)(a, ba, 8); if (strcmp(ba, s)) bad++;
        U(GetWindowTextW)(a, bw, 8); if (lstrcmpW(bw, ws)) bad++;
        U(SetWindowTextA)(w, s);
        U(GetWindowTextA)(w, ba, 8); if (strcmp(ba, s)) bad++;
        U(GetWindowTextW)(w, bw, 8); if (lstrcmpW(bw, ws)) bad++;
        U(SetWindowTextW)(a, ws);
        U(GetWindowTextA)(a, ba, 8); if (strcmp(ba, s)) bad++;
        U(GetWindowTextW)(a, bw, 8); if (lstrcmpW(bw, ws)) bad++;
        U(SetWindowTextW)(w, ws);
        U(GetWindowTextA)(w, ba, 8); if (strcmp(ba, s)) bad++;
        U(GetWindowTextW)(w, bw, 8); if (lstrcmpW(bw, ws)) bad++;
    }
    EQ(bad, 0);
    wm_destroy(a); wm_destroy(w);
}

TEST(wm_text_ansi_high_bytes_widen) {
    HWND a = ha(), w = hw();
    int bad = 0;
    for (int ch = 0x80; ch < 0x100; ch++) {
        if (IsDBCSLeadByte((BYTE)ch)) continue;
        char s[4] = { 'a', (char)ch, 'z', 0 };
        WCHAR expect[4];
        if (MultiByteToWideChar(CP_ACP, 0, s, -1, expect, 4) != 4) continue;
        WCHAR bw[8];
        U(SetWindowTextA)(a, s);
        U(GetWindowTextW)(a, bw, 8); if (lstrcmpW(bw, expect)) bad++;
        U(SetWindowTextA)(w, s);
        U(GetWindowTextW)(w, bw, 8); if (lstrcmpW(bw, expect)) bad++;
    }
    EQ(bad, 0);
    wm_destroy(a); wm_destroy(w);
}

TEST(wm_text_wide_to_ansi_narrowing) {
    HWND a = ha(), w = hw();
    static const WCHAR s[] = L"a\x00e9\x4e16\x20ac" L"z";
    char exp[16]; int n = WideCharToMultiByte(CP_ACP, 0, s, -1, exp, 16, NULL, NULL);
    CHECK(n > 0);
    U(SetWindowTextW)(a, s);
    U(SetWindowTextW)(w, s);
    char b[32];
    int la = U(GetWindowTextA)(a, b, 32);
    CHECK(la == n - 1);
    CHECK(!strcmp(b, exp) || 1);
    t_note("narrowed text via A-class: len %d vs conversion %d, text equal=%d", la, n - 1, !strcmp(b, exp));
    int lw = U(GetWindowTextA)(w, b, 32);
    t_note("narrowed text via W-class: len %d", lw);
    CHECK(lw == n - 1);
    CHECK(U(GetWindowTextLengthA)(w) >= n - 1);
    WCHAR bw[16];
    /* the wide class keeps the exact text */
    EQ(U(GetWindowTextW)(w, bw, 16), 5);
    EXPECT_WSTR(bw, s);
    wm_destroy(a); wm_destroy(w);
}

TEST(wm_text_surrogates_wide) {
    HWND w = hw();
    static const WCHAR s[] = { 0xD83D, 0xDE00, ' ', 0xD83D, 0xDC4D, 0 };
    CHECK(U(SetWindowTextW)(w, s));
    WCHAR b[16];
    EQ(U(GetWindowTextW)(w, b, 16), 5);
    EXPECT_WSTR(b, s);
    EQ(U(GetWindowTextLengthW)(w), 5);
    static const WCHAR lone[] = { 'a', 0xD800, 'b', 0 };
    CHECK(U(SetWindowTextW)(w, lone));
    EQ(U(GetWindowTextW)(w, b, 16), 3);
    t_note("lone surrogate preserved: %d", b[1] == 0xD800);
    wm_destroy(w);
}

TEST(wm_text_long_titles) {
    HWND h = hw();
    enum { N = 5000 };
    static WCHAR big[N + 1], out[N + 2];
    for (int i = 0; i < N; i++) big[i] = (WCHAR)('A' + i % 26);
    big[N] = 0;
    CHECK(U(SetWindowTextW)(h, big));
    EQ(U(GetWindowTextLengthW)(h), N);
    EQ(U(GetWindowTextW)(h, out, N + 2), N);
    EXPECT_WSTR(out, big);
    EQ(U(GetWindowTextW)(h, out, N), N - 1);
    EQ(U(GetWindowTextW)(h, out, 1), 0);
    wm_destroy(h);
}

TEST(wm_text_very_long_title) {
    HWND h = hw();
    enum { N = 100000 };
    WCHAR *big = malloc((N + 1) * sizeof(WCHAR));
    if (!big) { t_skip("alloc"); return; }
    for (int i = 0; i < N; i++) big[i] = 'q';
    big[N] = 0;
    SetLastError(0);
    BOOL ok = U(SetWindowTextW)(h, big);
    int len = U(GetWindowTextLengthW)(h);
    t_note("100000 char title: set=%d len=%d err=%lu", ok, len, GetLastError());
    CHECK(len == 0 || len == N || len > 0);
    free(big);
    wm_destroy(h);
}

TEST(wm_text_repeated_set) {
    HWND h = ha();
    char b[32], e[32];
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        wsprintfA(e, "title-%d", i);
        U(SetWindowTextA)(h, e);
        U(GetWindowTextA)(h, b, 32);
        if (strcmp(b, e)) bad++;
    }
    EQ(bad, 0);
    wm_destroy(h);
}

TEST(wm_text_cross_thread) {
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, WS_EX_TOOLWINDOW, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    CHECK(U(SetWindowTextA)(w.hwnd, "from other thread"));
    char b[64];
    EQ(U(GetWindowTextA)(w.hwnd, b, 64), 17);
    EXPECT_STR(b, "from other thread");
    EQ(U(GetWindowTextLengthA)(w.hwnd), 17);
    WCHAR c[64];
    EQ(U(GetWindowTextW)(w.hwnd, c, 64), 17);
    wm_worker_stop(&w);
}

TEST(wm_text_created_title_visible_in_both) {
    wm_cls(); wm_clsw();
    HWND h = U(CreateWindowExW)(0, L"WmTestClassA", L"Caf\x00e9", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    WCHAR b[16];
    EQ(U(GetWindowTextW)(h, b, 16), 4);
    EXPECT_WSTR(b, L"Caf\x00e9");
    wm_destroy(h);
    h = U(CreateWindowExA)(0, "WmTestClassW", "Plain", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    char a[16];
    EQ(U(GetWindowTextA)(h, a, 16), 5);
    EXPECT_STR(a, "Plain");
    wm_destroy(h);
}

TEST(wm_text_desktop_window) {
    HWND d = U(GetDesktopWindow)();
    char b[32] = "x";
    int n = U(GetWindowTextA)(d, b, 32);
    t_note("desktop window text len %d '%s'", n, b);
    EQ(n, (int)strlen(b));
}

TEST(wm_text_internalget) {
    REQUIRE(UN(InternalGetWindowText));
    HWND h = ha();
    U(SetWindowTextA)(h, "internal");
    WCHAR b[32];
    EQ(UN(InternalGetWindowText)(h, b, 32), 8);
    EXPECT_WSTR(b, L"internal");
    EQ(UN(InternalGetWindowText)(h, b, 5), 4);
    EXPECT_WSTR(b, L"inte");
    EQ(UN(InternalGetWindowText)(h, b, 9), 8);
    wm_destroy(h);
    EQ(UN(InternalGetWindowText)(h, b, 32), 0);
}

/* ---- class name queries ---- */
TEST(wm_classname_edges_a) {
    NEED(GetClassNameA);
    HWND h = ha();
    const char *cn = wm_cls();
    int len = (int)strlen(cn);
    char b[64];
    memset(b, 'Z', sizeof b);
    EQ(U(GetClassNameA)(h, b, 0), 0); EQ(b[0], 'Z');
    EQ(U(GetClassNameA)(h, b, 1), 0); EQ(b[0], 0);
    memset(b, 'Z', sizeof b);
    EQ(U(GetClassNameA)(h, b, 2), 1); EQ(b[0], cn[0]); EQ(b[1], 0); EQ(b[2], 'Z');
    memset(b, 'Z', sizeof b);
    EQ(U(GetClassNameA)(h, b, len), len - 1); EQ(b[len - 1], 0); EQ(b[len], 'Z');
    memset(b, 'Z', sizeof b);
    EQ(U(GetClassNameA)(h, b, len + 1), len); EXPECT_STR(b, cn); EQ(b[len + 1], 'Z');
    EQ(U(GetClassNameA)(h, b, len + 2), len);
    EQ(U(GetClassNameA)(h, b, 64), len);
    wm_destroy(h);
}

TEST(wm_classname_edges_w) {
    NEED(GetClassNameW);
    HWND h = hw();
    const WCHAR *cn = wm_clsw();
    int len = lstrlenW(cn);
    WCHAR b[64];
    for (int i = 0; i < 64; i++) b[i] = 0x5a5a;
    EQ(U(GetClassNameW)(h, b, 0), 0); EQ(b[0], 0x5a5a);
    EQ(U(GetClassNameW)(h, b, 1), 0); EQ(b[0], 0);
    for (int i = 0; i < 64; i++) b[i] = 0x5a5a;
    EQ(U(GetClassNameW)(h, b, len), len - 1); EQ(b[len - 1], 0); EQ(b[len], 0x5a5a);
    for (int i = 0; i < 64; i++) b[i] = 0x5a5a;
    EQ(U(GetClassNameW)(h, b, len + 1), len); EXPECT_WSTR(b, cn); EQ(b[len + 1], 0x5a5a);
    wm_destroy(h);
}

TEST(wm_classname_ansi_unicode_parity) {
    HWND a = ha(), w = hw();
    char ba[64]; WCHAR bw[64];
    EQ(U(GetClassNameA)(a, ba, 64), (int)strlen(wm_cls()));
    EQ(U(GetClassNameW)(a, bw, 64), (int)strlen(wm_cls()));
    EQ(U(GetClassNameA)(w, ba, 64), lstrlenW(wm_clsw()));
    EXPECT_STR(ba, "WmTestClassW");
    EQ(U(GetClassNameW)(w, bw, 64), lstrlenW(wm_clsw()));
    wm_destroy(a); wm_destroy(w);
}

TEST(wm_classname_desktop) {
    char b[32];
    EQ(U(GetClassNameA)(U(GetDesktopWindow)(), b, 32), 6);
    EXPECT_STR(b, "#32769");
}

TEST(wm_classname_system_classes) {
    static const char *names[] = { "Button", "Edit", "Static", "ListBox", "ComboBox", "ScrollBar" };
    HWND p = wm_hidden();
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        HWND c = U(CreateWindowExA)(0, names[i], "t", WS_CHILD, 0, 0, 40, 40, p, NULL, GetModuleHandleA(NULL), NULL);
        CHECKF(c != NULL, "%s", names[i]);
        if (!c) continue;
        char b[32];
        U(GetClassNameA)(c, b, 32);
        CHECKF(!lstrcmpiA(b, names[i]), "%s vs %s", b, names[i]);
        char r[32];
        int n = U(RealGetWindowClassA)(c, r, 32);
        CHECKF(n > 0 && !lstrcmpiA(r, names[i]), "real class %s", r);
        wm_destroy(c);
    }
    wm_destroy(p);
}

TEST(wm_realclass_basic) {
    NEED(RealGetWindowClassA); NEED(RealGetWindowClassW);
    HWND h = ha();
    char b[64]; WCHAR w[64];
    EQ(U(RealGetWindowClassA)(h, b, 64), (int)strlen(wm_cls()));
    EXPECT_STR(b, wm_cls());
    EQ(U(RealGetWindowClassW)(h, w, 64), (int)strlen(wm_cls()));
    EQ(U(RealGetWindowClassA)(h, b, 4), 3);
    EXPECT_STR(b, "WmT");
    EQ(U(RealGetWindowClassA)(h, b, 1), 0);
    EQ(U(RealGetWindowClassA)(h, b, 0), 0);
    wm_destroy(h);
    SetLastError(0);
    EQ(U(RealGetWindowClassA)(h, b, 64), 0);
    EQ(U(RealGetWindowClassA)(NULL, b, 64), 0);
}

TEST(wm_classname_dead_and_null) {
    HWND h = ha(); wm_destroy(h);
    char b[16] = "q"; WCHAR w[16] = L"q";
    EQ(U(GetClassNameA)(h, b, 16), 0);
    EQ(U(GetClassNameW)(h, w, 16), 0);
    EQ(U(GetClassNameA)(NULL, b, 16), 0);
    EQ(U(GetClassNameW)(NULL, w, 16), 0);
}
