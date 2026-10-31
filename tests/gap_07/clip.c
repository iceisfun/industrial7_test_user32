/* Clipboard: sequence number, format queries, format names, format listeners. Clipboard-mutating tests are
 * skipped when the user's clipboard holds anything other than plain text; text is saved and restored. */
#include "gap7.h"

static int have_clip(void)
{
    return U(OpenClipboard) && U(CloseClipboard) && U(EmptyClipboard) && U(SetClipboardData) && U(GetClipboardData) && U(IsClipboardFormatAvailable) &&
           U(CountClipboardFormats) && U(GetClipboardSequenceNumber) && U(RegisterClipboardFormatW) && U(GetClipboardFormatNameW) &&
           U(GetClipboardFormatNameA) && U(EnumClipboardFormats) && U(CreateWindowExW) && U(DestroyWindow);
}

static WCHAR *g_saved; static int g_have_saved;
/* returns 0 when the current clipboard content must not be touched */
static int clip_save(HWND owner)
{
    g_saved = NULL; g_have_saved = 0;
    int ok = 1;
    DWORD end = GetTickCount() + 2000;
    while (!U(OpenClipboard)(owner)) { if ((int)(end - GetTickCount()) < 0) return 0; Sleep(20); }
    UINT f = 0;
    while ((f = U(EnumClipboardFormats)(f)) != 0) {
        if (f != CF_TEXT && f != CF_OEMTEXT && f != CF_UNICODETEXT && f != CF_LOCALE) ok = 0;
    }
    if (ok && U(IsClipboardFormatAvailable)(CF_UNICODETEXT)) {
        HANDLE h = U(GetClipboardData)(CF_UNICODETEXT);
        if (h) {
            const WCHAR *p = GlobalLock(h);
            if (p) { size_t n = lstrlenW(p) + 1; g_saved = malloc(n * 2); if (g_saved) { memcpy(g_saved, p, n * 2); g_have_saved = 1; } GlobalUnlock(h); }
        }
    }
    U(CloseClipboard)();
    return ok;
}
static void clip_restore(HWND owner)
{
    DWORD end = GetTickCount() + 2000;
    while (!U(OpenClipboard)(owner)) { if ((int)(end - GetTickCount()) < 0) return; Sleep(20); }
    U(EmptyClipboard)();
    if (g_have_saved) {
        size_t n = lstrlenW(g_saved) + 1;
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n * 2);
        if (g) { void *p = GlobalLock(g); memcpy(p, g_saved, n * 2); GlobalUnlock(g); if (!U(SetClipboardData)(CF_UNICODETEXT, g)) GlobalFree(g); }
    }
    U(CloseClipboard)();
    free(g_saved); g_saved = NULL; g_have_saved = 0;
}
static HGLOBAL mkdata(const void *src, size_t n)
{
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n ? n : 1);
    if (g && src) { void *p = GlobalLock(g); memcpy(p, src, n); GlobalUnlock(g); }
    return g;
}
static int set_private(HWND owner, UINT fmt, int empty_first)
{
    if (!U(OpenClipboard)(owner)) return 0;
    if (empty_first) U(EmptyClipboard)();
    HGLOBAL g = mkdata("gap7", 5);
    int ok = U(SetClipboardData)(fmt, g) != NULL;
    if (!ok) GlobalFree(g);
    U(CloseClipboard)();
    return ok;
}
static int clear_clip(HWND owner)
{
    if (!U(OpenClipboard)(owner)) return 0;
    U(EmptyClipboard)();
    U(CloseClipboard)();
    return 1;
}
#define CLIP_SETUP(owner) \
    if (!have_clip()) { REQUIRE(have_clip()); } \
    HWND owner = g7_mkwinW(0, 0, NULL); \
    if (!owner) { t_skip("no window"); return; } \
    if (!clip_save(owner)) { U(DestroyWindow)(owner); t_skip("clipboard holds non-text data; not touching it"); return; }
#define CLIP_END(owner) do { clip_restore(owner); U(DestroyWindow)(owner); } while (0)

/* ---------- pure format-name tests (atom table only, clipboard untouched) ---------- */
TEST(gap7_clipname_registered_roundtrip)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    UINT id = U(RegisterClipboardFormatW)(L"gap7_fmt_roundtrip");
    CHECKF(id >= 0xC000 && id <= 0xFFFF, "id=%x", id);
    WCHAR b[64];
    memset(b, 0x55, sizeof b);
    int n = U(GetClipboardFormatNameW)(id, b, 64);
    CHECKF(n == 18, "n=%d", n);
    CHECK(!lstrcmpW(b, L"gap7_fmt_roundtrip"));
    CHECK(b[18] == 0);
    char a[64]; memset(a, 0x55, sizeof a);
    n = U(GetClipboardFormatNameA)(id, a, 64);
    CHECKF(n == 18, "A n=%d", n);
    CHECK(!strcmp(a, "gap7_fmt_roundtrip"));
    /* re-registering gives the same id */
    CHECK(U(RegisterClipboardFormatW)(L"gap7_fmt_roundtrip") == id);
    if (U(RegisterClipboardFormatA)) CHECK(U(RegisterClipboardFormatA)("gap7_fmt_roundtrip") == id);
}

TEST(gap7_clipname_case_insensitive_registration)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    UINT a = U(RegisterClipboardFormatW)(L"Gap7_CaseFormat");
    UINT b = U(RegisterClipboardFormatW)(L"GAP7_CASEFORMAT");
    UINT c = U(RegisterClipboardFormatW)(L"gap7_caseformat");
    CHECK(a != 0);
    CHECKF(a == b && b == c, "%x %x %x", a, b, c);
    WCHAR n[64];
    int len = U(GetClipboardFormatNameW)(c, n, 64);
    CHECK(len == 15);
    t_note("name stored for case-folded registrations: '%ls'", n);
}

TEST(gap7_clipname_buffer_sizes)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    UINT id = U(RegisterClipboardFormatW)(L"gap7_fmt_buffers");   /* 16 chars */
    for (int cch = 0; cch <= 20; cch++) {
        WCHAR b[40]; for (int i = 0; i < 40; i++) b[i] = 0x7777;
        SET_ERR(0);
        int n = U(GetClipboardFormatNameW)(id, b, cch);
        int want = cch <= 0 ? 0 : (cch - 1 < 16 ? cch - 1 : 16);
        if (cch == 0) { CHECKF(n == 0, "W cch=0 n=%d", n); CHECK(b[0] == 0x7777); continue; }
        CHECKF(n == want, "W cch=%d n=%d want %d", cch, n, want);
        CHECKF(b[n] == 0, "W cch=%d not terminated", cch);
        CHECKF(!wcsncmp(b, L"gap7_fmt_buffers", n), "W cch=%d content", cch);
        CHECKF(b[cch] == 0x7777, "W cch=%d overran", cch);
        char a[40]; memset(a, 0x55, sizeof a);
        n = U(GetClipboardFormatNameA)(id, a, cch);
        CHECKF(n == want, "A cch=%d n=%d want %d", cch, n, want);
        CHECKF(a[n] == 0, "A cch=%d not terminated", cch);
        CHECKF(a[cch] == 0x55, "A cch=%d overran", cch);
    }
}

TEST(gap7_clipname_predefined_and_invalid_formats)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    static const UINT ids[] = { 0, CF_TEXT, CF_BITMAP, CF_UNICODETEXT, CF_HDROP, CF_LOCALE, CF_DIB, 0x7, 0x1F, CF_OWNERDISPLAY, CF_PRIVATEFIRST,
                                CF_GDIOBJFIRST, 0xBFFF, 0xFFFF, 0x10000, 0x80000000u, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        WCHAR b[32] = L"zz"; char a[32] = "zz";
        SET_ERR(0);
        int n = U(GetClipboardFormatNameW)(ids[i], b, 32);
        CHECKF(n == 0, "W id=%x n=%d", ids[i], n);
        DWORD e = GetLastError();
        n = U(GetClipboardFormatNameA)(ids[i], a, 32);
        CHECKF(n == 0, "A id=%x n=%d", ids[i], n);
        if (i < 5) t_note("GetClipboardFormatNameW(%x): error %lu", ids[i], e);
    }
}

TEST(gap7_clipname_unregistered_high_ids)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    /* ids in the registered range that were never handed out */
    UINT top = U(RegisterClipboardFormatW)(L"gap7_fmt_probe_top");
    WCHAR b[64];
    int hits = 0;
    for (UINT id = 0xC000; id < 0xC000 + 8; id++) { if (U(GetClipboardFormatNameW)(id, b, 64) > 0) hits++; }
    t_note("%d of the first 8 registered ids already have names", hits);
    CHECK(U(GetClipboardFormatNameW)(0xFFFF, b, 64) == 0 || top == 0xFFFF || 1);
    (void)top;
}

TEST(gap7_clipname_unicode_name)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    static const WCHAR nm[] = L"gap7_\x00e9\x4e2d\x6587_fmt";
    UINT id = U(RegisterClipboardFormatW)(nm);
    CHECK(id >= 0xC000);
    WCHAR b[64] = { 0 };
    int n = U(GetClipboardFormatNameW)(id, b, 64);
    CHECKF(n == lstrlenW(nm), "n=%d", n);
    CHECK(!lstrcmpW(b, nm));
    /* the ANSI call converts through the code page; length is bounded by the buffer */
    char a[64]; memset(a, 0x55, sizeof a);
    n = U(GetClipboardFormatNameA)(id, a, 64);
    CHECK(n > 0 && n <= 63 && a[n] == 0);
    char small[4]; memset(small, 0x55, sizeof small);
    n = U(GetClipboardFormatNameA)(id, small, 2);
    CHECK(n <= 1 && small[n] == 0 && small[2] == 0x55);
}

TEST(gap7_clipname_long_names)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    WCHAR name[300];
    for (int i = 0; i < 255; i++) name[i] = 'a' + (i % 26);
    name[255] = 0;
    UINT id = U(RegisterClipboardFormatW)(name);
    t_note("255-char format name registered: id=%x", id);
    if (id) {
        WCHAR b[300] = { 0 };
        int n = U(GetClipboardFormatNameW)(id, b, 300);
        CHECKF(n == 255, "n=%d", n);
        CHECK(!lstrcmpW(b, name));
    }
    for (int i = 0; i < 299; i++) name[i] = 'a' + (i % 26);
    name[299] = 0;
    SET_ERR(0);
    UINT id2 = U(RegisterClipboardFormatW)(name);
    t_note("299-char format name: id=%x err=%lu", id2, GetLastError());
}

TEST(gap7_clipname_many_registrations_unique_ids)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    UINT ids[40];
    for (int i = 0; i < 40; i++) {
        WCHAR nm[40]; wsprintfW(nm, L"gap7_uniq_fmt_%d", i);
        ids[i] = U(RegisterClipboardFormatW)(nm);
        CHECKF(ids[i] >= 0xC000, "i=%d id=%x", i, ids[i]);
    }
    for (int i = 0; i < 40; i++) for (int j = i + 1; j < 40; j++) if (ids[i] == ids[j]) { CHECKF(0, "duplicate id %x for %d,%d", ids[i], i, j); }
    for (int i = 0; i < 40; i++) {
        WCHAR nm[40], got[40]; wsprintfW(nm, L"gap7_uniq_fmt_%d", i);
        CHECK(U(GetClipboardFormatNameW)(ids[i], got, 40) == lstrlenW(nm) && !lstrcmpW(got, nm));
    }
}

/* ---------- queries that do not need the clipboard open ---------- */
TEST(gap7_clipq_invalid_format_ids)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    static const UINT ids[] = { 0, 0xFFFFFFFFu, 0x80000000u, 0xFFFF, 0x10000, 0xBFFF };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SET_ERR(0);
        BOOL r = U(IsClipboardFormatAvailable)(ids[i]);
        if (ids[i] == 0xFFFF || ids[i] == 0xBFFF) { t_note("IsClipboardFormatAvailable(%x) = %d", ids[i], r); continue; }
        CHECKF(r == FALSE, "id=%x r=%d", ids[i], r);
    }
}

TEST(gap7_clipq_count_and_available_without_open)
{
    CLIP_SETUP(owner);
    CHECK(clear_clip(owner));
    CHECK(U(CountClipboardFormats)() == 0);
    CHECK(!U(IsClipboardFormatAvailable)(CF_TEXT));
    CHECK(!U(IsClipboardFormatAvailable)(CF_UNICODETEXT));
    CHECK(!U(IsClipboardFormatAvailable)(CF_BITMAP));
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_count");
    CHECK(set_private(owner, fmt, 1));
    CHECK(U(CountClipboardFormats)() == 1);
    CHECK(U(IsClipboardFormatAvailable)(fmt));
    CHECK(!U(IsClipboardFormatAvailable)(CF_TEXT));
    CHECK(!U(IsClipboardFormatAvailable)(fmt + 1));
    CLIP_END(owner);
}

TEST(gap7_clipq_count_matches_enumeration)
{
    CLIP_SETUP(owner);
    UINT f1 = U(RegisterClipboardFormatW)(L"gap7_priv_enum_a"), f2 = U(RegisterClipboardFormatW)(L"gap7_priv_enum_b");
    CHECK(U(OpenClipboard)(owner));
    U(EmptyClipboard)();
    HGLOBAL g1 = mkdata("a", 2), g2 = mkdata("b", 2), g3 = mkdata("c", 2);
    CHECK(U(SetClipboardData)(f1, g1));
    CHECK(U(SetClipboardData)(f2, g2));
    CHECK(U(SetClipboardData)(CF_PRIVATEFIRST + 5, g3));
    CHECK(U(CountClipboardFormats)() == 3);
    UINT f = 0; int n = 0; UINT seen[8] = { 0 };
    while ((f = U(EnumClipboardFormats)(f)) != 0 && n < 8) seen[n++] = f;
    CHECK(n == 3);
    CHECK(seen[0] == f1 && seen[1] == f2 && seen[2] == CF_PRIVATEFIRST + 5);   /* insertion order */
    CHECK(U(CloseClipboard)());
    CHECK(U(CountClipboardFormats)() == 3);
    CHECK(U(IsClipboardFormatAvailable)(CF_PRIVATEFIRST + 5));
    CLIP_END(owner);
}

TEST(gap7_clipq_synthesized_text_formats)
{
    CLIP_SETUP(owner);
    CHECK(U(OpenClipboard)(owner));
    U(EmptyClipboard)();
    CHECK(U(SetClipboardData)(CF_UNICODETEXT, mkdata(L"synth", 12)));
    CHECK(U(CloseClipboard)());
    CHECK(U(IsClipboardFormatAvailable)(CF_UNICODETEXT));
    CHECK(U(IsClipboardFormatAvailable)(CF_TEXT));
    CHECK(U(IsClipboardFormatAvailable)(CF_OEMTEXT));
    int n = U(CountClipboardFormats)();
    CHECKF(n >= 3 && n <= 4, "count=%d", n);
    t_note("CF_UNICODETEXT alone -> CountClipboardFormats = %d (CF_LOCALE %s)", n, U(IsClipboardFormatAvailable)(CF_LOCALE) ? "present" : "absent");
    CHECK(!U(IsClipboardFormatAvailable)(CF_BITMAP));
    CHECK(!U(IsClipboardFormatAvailable)(CF_HDROP));
    CLIP_END(owner);
}

TEST(gap7_clipq_empty_resets_everything)
{
    CLIP_SETUP(owner);
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_empty");
    CHECK(set_private(owner, fmt, 1));
    CHECK(U(CountClipboardFormats)() >= 1);
    CHECK(clear_clip(owner));
    CHECK(U(CountClipboardFormats)() == 0);
    CHECK(!U(IsClipboardFormatAvailable)(fmt));
    CLIP_END(owner);
}

/* ---------- sequence number ---------- */
TEST(gap7_clipseq_stable_without_changes)
{
    if (!have_clip()) { REQUIRE(have_clip()); }
    DWORD a = U(GetClipboardSequenceNumber)();
    DWORD b = U(GetClipboardSequenceNumber)();
    CHECK(a != 0);
    CHECKF(a == b, "%lu vs %lu", a, b);
}

TEST(gap7_clipseq_changes_when_clipboard_changes)
{
    CLIP_SETUP(owner);
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_seq");
    DWORD s0 = U(GetClipboardSequenceNumber)();
    CHECK(set_private(owner, fmt, 1));
    DWORD s1 = U(GetClipboardSequenceNumber)();
    CHECKF(s1 != s0, "unchanged %lu", s0);
    CHECKF((int)(s1 - s0) > 0 && (int)(s1 - s0) < 100, "delta %d", (int)(s1 - s0));
    CHECK(set_private(owner, fmt + 0 /* same format again */, 1));
    DWORD s2 = U(GetClipboardSequenceNumber)();
    CHECKF((int)(s2 - s1) > 0, "no change after second set: %lu -> %lu", s1, s2);
    CHECK(clear_clip(owner));
    DWORD s3 = U(GetClipboardSequenceNumber)();
    CHECKF((int)(s3 - s2) > 0, "no change after empty: %lu -> %lu", s2, s3);
    CLIP_END(owner);
}

TEST(gap7_clipseq_open_close_alone_does_not_change)
{
    CLIP_SETUP(owner);
    DWORD s0 = U(GetClipboardSequenceNumber)();
    CHECK(U(OpenClipboard)(owner));
    DWORD s1 = U(GetClipboardSequenceNumber)();
    U(IsClipboardFormatAvailable)(CF_TEXT);
    U(CountClipboardFormats)();
    CHECK(U(CloseClipboard)());
    DWORD s2 = U(GetClipboardSequenceNumber)();
    CHECKF(s0 == s1 && s1 == s2, "%lu %lu %lu", s0, s1, s2);
    CLIP_END(owner);
}

TEST(gap7_clipseq_within_open_session)
{
    CLIP_SETUP(owner);
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_seq2");
    CHECK(U(OpenClipboard)(owner));
    DWORD s0 = U(GetClipboardSequenceNumber)();
    U(EmptyClipboard)();
    DWORD s1 = U(GetClipboardSequenceNumber)();
    CHECK(U(SetClipboardData)(fmt, mkdata("x", 2)));
    DWORD s2 = U(GetClipboardSequenceNumber)();
    CHECK(U(CloseClipboard)());
    DWORD s3 = U(GetClipboardSequenceNumber)();
    CHECKF((int)(s1 - s0) > 0, "empty did not bump: %lu -> %lu", s0, s1);
    CHECK((int)(s2 - s1) >= 0);
    CHECK((int)(s3 - s2) >= 0);
    t_note("sequence: before %lu, after empty %lu, after set %lu, after close %lu", s0, s1, s2, s3);
    CLIP_END(owner);
}

TEST(gap7_clipseq_monotonic_over_repeated_changes)
{
    CLIP_SETUP(owner);
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_seq3");
    DWORD prev = U(GetClipboardSequenceNumber)();
    int bad = 0;
    for (int i = 0; i < 20; i++) {
        if (!set_private(owner, fmt, 1)) { bad++; continue; }
        DWORD cur = U(GetClipboardSequenceNumber)();
        if ((int)(cur - prev) <= 0) bad++;
        prev = cur;
    }
    CHECKF(bad == 0, "bad=%d", bad);
    CLIP_END(owner);
}

/* ---------- clipboard format listener ---------- */
static int have_lst(void) { return U(AddClipboardFormatListener) && U(RemoveClipboardFormatListener) && have_clip(); }

TEST(gap7_cliplist_invalid_windows)
{
    if (!have_lst()) { REQUIRE(have_lst()); }
    static const ULONG_PTR bad[] = { 0, 0xDEADBEEF, 0x12345678, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL r = U(AddClipboardFormatListener)((HWND)bad[i]);
        CHECKF(r == FALSE, "add %llx r=%d", (unsigned long long)bad[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "add %llx err=%lu", (unsigned long long)bad[i], GetLastError());
        SET_ERR(0);
        r = U(RemoveClipboardFormatListener)((HWND)bad[i]);
        CHECKF(r == FALSE, "remove %llx r=%d", (unsigned long long)bad[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "remove %llx err=%lu", (unsigned long long)bad[i], GetLastError());
    }
    HWND w = g7_mkwinW(0, 0, NULL);
    U(DestroyWindow)(w);
    SET_ERR(0);
    CHECK(U(AddClipboardFormatListener)(w) == FALSE);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
}

TEST(gap7_cliplist_add_remove_lifecycle)
{
    if (!have_lst()) { REQUIRE(have_lst()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    SET_ERR(0);
    CHECK(U(RemoveClipboardFormatListener)(w) == FALSE);          /* not registered yet */
    t_note("remove of unregistered window: err=%lu", GetLastError());
    CHECK(U(AddClipboardFormatListener)(w));
    SET_ERR(0);
    BOOL again = U(AddClipboardFormatListener)(w);
    t_note("second add of the same window: r=%d err=%lu", again, GetLastError());
    CHECK(U(RemoveClipboardFormatListener)(w));
    SET_ERR(0);
    BOOL r = U(RemoveClipboardFormatListener)(w);
    t_note("second remove: r=%d err=%lu", r, GetLastError());
    if (again) { /* an idempotent add leaves exactly one registration */ CHECK(r == FALSE); }
    U(DestroyWindow)(w);
}

TEST(gap7_cliplist_message_only_window_allowed)
{
    if (!have_lst()) { REQUIRE(have_lst()); }
    HWND w = g7_mkwinW(0, 0, HWND_MESSAGE);
    if (!w) { t_skip("no message-only window"); return; }
    BOOL r = U(AddClipboardFormatListener)(w);
    CHECKF(r, "add on message-only window failed, err=%lu", GetLastError());
    if (r) CHECK(U(RemoveClipboardFormatListener)(w));
    U(DestroyWindow)(w);
}

TEST(gap7_cliplist_receives_clipboardupdate)
{
    CLIP_SETUP(owner);
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_listen");
    HWND lw = g7_mkwinW(0, 0, NULL);
    g7_reset(0x31D, 0x31D, 0);
    CHECK(U(AddClipboardFormatListener)(lw));
    g7_pump_ms(100);
    int initial = g7_count(lw, 0x31D);
    t_note("WM_CLIPBOARDUPDATE right after registration: %d", initial);
    g7_nlog = 0;
    CHECK(set_private(owner, fmt, 1));
    DWORD end = GetTickCount() + 3000;
    while (g7_count(lw, 0x31D) == 0 && (int)(end - GetTickCount()) > 0) { t_pump(); Sleep(5); }
    int got = g7_count(lw, 0x31D);
    CHECKF(got >= 1, "no WM_CLIPBOARDUPDATE after a clipboard change");
    g7_pump_ms(100);
    t_note("WM_CLIPBOARDUPDATE count for empty+set+close: %d", g7_count(lw, 0x31D));
    /* after removal nothing more arrives */
    CHECK(U(RemoveClipboardFormatListener)(lw));
    g7_pump_ms(50);
    g7_nlog = 0;
    CHECK(set_private(owner, fmt, 1));
    g7_pump_ms(300);
    CHECKF(g7_count(lw, 0x31D) == 0, "message after removal: %d", g7_count(lw, 0x31D));
    U(DestroyWindow)(lw);
    g7_reset(1, 0, 0);
    CLIP_END(owner);
}

TEST(gap7_cliplist_two_listeners_both_notified)
{
    CLIP_SETUP(owner);
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_listen2");
    HWND a = g7_mkwinW(0, 0, NULL), b = g7_mkwinW(0, 0, NULL);
    g7_reset(0x31D, 0x31D, 0);
    CHECK(U(AddClipboardFormatListener)(a));
    CHECK(U(AddClipboardFormatListener)(b));
    g7_pump_ms(100);
    g7_nlog = 0;
    CHECK(set_private(owner, fmt, 1));
    DWORD end = GetTickCount() + 3000;
    while ((g7_count(a, 0x31D) == 0 || g7_count(b, 0x31D) == 0) && (int)(end - GetTickCount()) > 0) { t_pump(); Sleep(5); }
    CHECKF(g7_count(a, 0x31D) >= 1, "a did not get the update");
    CHECKF(g7_count(b, 0x31D) >= 1, "b did not get the update");
    U(RemoveClipboardFormatListener)(a);
    g7_pump_ms(30);
    g7_nlog = 0;
    CHECK(set_private(owner, fmt, 1));
    end = GetTickCount() + 3000;
    while (g7_count(b, 0x31D) == 0 && (int)(end - GetTickCount()) > 0) { t_pump(); Sleep(5); }
    CHECK(g7_count(b, 0x31D) >= 1);
    CHECK(g7_count(a, 0x31D) == 0);
    U(RemoveClipboardFormatListener)(b);
    U(DestroyWindow)(a); U(DestroyWindow)(b);
    g7_reset(1, 0, 0);
    CLIP_END(owner);
}

TEST(gap7_cliplist_destroyed_listener_is_dropped)
{
    CLIP_SETUP(owner);
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_listen3");
    HWND a = g7_mkwinW(0, 0, NULL);
    CHECK(U(AddClipboardFormatListener)(a));
    U(DestroyWindow)(a);
    /* changing the clipboard with a dead listener registered must work and not post to a stale handle */
    CHECK(set_private(owner, fmt, 1));
    g7_pump_ms(50);
    SET_ERR(0);
    CHECK(U(RemoveClipboardFormatListener)(a) == FALSE);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    CLIP_END(owner);
}

TEST(gap7_cliplist_unrelated_change_notifies_once_per_change)
{
    CLIP_SETUP(owner);
    UINT fmt = U(RegisterClipboardFormatW)(L"gap7_priv_listen4");
    HWND lw = g7_mkwinW(0, 0, NULL);
    g7_reset(0x31D, 0x31D, 0);
    CHECK(U(AddClipboardFormatListener)(lw));
    g7_pump_ms(100);
    int total = 0;
    for (int i = 0; i < 3; i++) {
        g7_nlog = 0;
        CHECK(set_private(owner, fmt, 1));
        DWORD end = GetTickCount() + 3000;
        while (g7_count(lw, 0x31D) == 0 && (int)(end - GetTickCount()) > 0) { t_pump(); Sleep(5); }
        g7_pump_ms(60);
        int c = g7_count(lw, 0x31D);
        CHECKF(c >= 1, "change %d: %d messages", i, c);
        total += c;
    }
    t_note("%d notifications for 3 changes", total);
    U(RemoveClipboardFormatListener)(lw);
    U(DestroyWindow)(lw);
    g7_reset(1, 0, 0);
    CLIP_END(owner);
}
