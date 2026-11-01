#include "cbcommon.h"
typedef struct { DWORD pFiles; POINT pt; BOOL fNC; BOOL fWide; } CBDROPFILES;
/* Set/Get/Enum/Count/Avail with plain data and error paths. */

static void put_text(HWND w, const char *s) {
    OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text(s)); CLOSE();
}
TEST(clipboard_set_get_text_roundtrip) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    CHECK(OPEN(w)); CHECK(EMPTY());
    HGLOBAL g = cb_text("hello clip");
    HANDLE r = SETD(CF_TEXT, g);
    CHECK(r == g);
    HANDLE d = GETD(CF_TEXT);
    CHECK(d != NULL);
    if (d) { char *p = GlobalLock(d); CHECK(p && !strcmp(p, "hello clip")); GlobalUnlock(d); }
    CHECK(CLOSE());
    CHECK(OPEN(w));
    d = GETD(CF_TEXT);
    CHECK(d != NULL);
    if (d) { char *p = GlobalLock(d); CHECK(p && !strcmp(p, "hello clip")); GlobalUnlock(d); }
    CHECK(CLOSE());
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_get_returns_same_handle) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); HGLOBAL g = cb_text("same");
    CHECK(SETD(CF_TEXT, g) == g);
    CHECK(GETD(CF_TEXT) == g);
    CHECK(GETD(CF_TEXT) == g);
    CLOSE(); OPEN(w);
    t_note("handle after reopen: same=%d", GETD(CF_TEXT) == g);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_get_missing_format) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "t");
    OPEN(w);
    SET_ERR(0); CHECK(GETD(CF_BITMAP) == NULL);
    SET_ERR(0); CHECK(GETD(CF_HDROP) == NULL);
    SET_ERR(0); CHECK(GETD(0xC123) == NULL);
    SET_ERR(0); CHECK(GETD(CF_PRIVATEFIRST) == NULL);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_get_format_zero) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "t");
    OPEN(w);
    SET_ERR(0); CHECK(GETD(0) == NULL);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_set_format_zero) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    HGLOBAL g = cb_text("z");
    SET_ERR(0); HANDLE r = SETD(0, g);
    CHECK(r == NULL);
    if (!r) GlobalFree(g);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_set_unregistered_formats) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    UINT fmts[] = { 0xC000, 0xC7FF, 0xFFFF };
    for (int i = 0; i < 3; i++) {
        HGLOBAL g = cb_text("u");
        HANDLE r = SETD(fmts[i], g);
        t_note("SetClipboardData(0x%x) unregistered -> %p err=%lu", fmts[i], r, GetLastError());
        if (!r) GlobalFree(g);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_set_private_range) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    UINT fmts[] = { CF_PRIVATEFIRST, CF_PRIVATEFIRST + 5, CF_PRIVATELAST, CF_GDIOBJFIRST, CF_GDIOBJFIRST + 10 };
    for (int i = 0; i < 3; i++) {
        HGLOBAL g = cb_text("priv");
        CHECKF(SETD(fmts[i], g) == g, "fmt %x", fmts[i]);
        CHECKF(AVAIL(fmts[i]), "fmt %x", fmts[i]);
        CHECKF(GETD(fmts[i]) == g, "fmt %x", fmts[i]);
    }
    CHECK(COUNT() == 3);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_set_registered_format) {
    CB_REQ_BASIC; REQUIRE(U(RegisterClipboardFormatA)); cb_reset();
    UINT f = U(RegisterClipboardFormatA)("T32_clip_custom_fmt");
    CHECK(f >= 0xC000 && f <= 0xFFFF);
    HWND w = cb_win(); OPEN(w); EMPTY();
    HGLOBAL g = cb_mem("\x01\x02\x03\x04", 4);
    CHECK(SETD(f, g) == g);
    CHECK(AVAIL(f));
    CHECK(GETD(f) == g);
    CHECK(GlobalSize(g) >= 4);
    CLOSE();
    CHECK(AVAIL(f)); CHECK(COUNT() == 1);
    cb_reset(); cb_destroy(w);
    CHECK(!AVAIL(f));
}
TEST(clipboard_set_replaces_same_format) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    HGLOBAL a = cb_text("first"), b = cb_text("second");
    CHECK(SETD(CF_TEXT, a) == a);
    CHECK(SETD(CF_TEXT, b) == b);
    HANDLE d = GETD(CF_TEXT);
    CHECK(d == b);
    if (d) { char *p = GlobalLock(d); CHECK(p && !strcmp(p, "second")); GlobalUnlock(d); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_set_without_empty) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w);
    HGLOBAL g = cb_text("noempty");
    SET_ERR(0);
    HANDLE r = SETD(CF_TEXT, g);
    t_note("SetClipboardData without EmptyClipboard (owner not set) -> %p err=%lu", r, GetLastError());
    if (!r) GlobalFree(g);
    CLOSE(); cb_reset(); cb_destroy(w);
}
/* SetClipboardData does not validate an HGLOBAL: Win10 accepts a bogus handle
 * and the heap is corrupted later when the clipboard frees it (observed
 * 0xc0000374 on the following Empty/Close). The deliberately faulting
 * sequence therefore runs in a child process; the parent records the outcome. */
TEST(clipboard_set_bad_handle) {
    CB_REQ_BASIC; cb_reset();
    DWORD rc = 0;
    int r = cb_run_child("clipboard_childonly_bad_handle", 60000, &rc);
    t_note("bad-handle child: completed=%d exit=0x%lx", r, rc);
    CHECK(r != 0);
    cb_reset();
}
TEST(clipboard_childonly_bad_handle) {
    char v[8];
    if (!GetEnvironmentVariableA("U32_CB_CHILD", v, sizeof v)) return; /* only runs in the child */
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    SET_ERR(0);
    HANDLE r = SETD(CF_TEXT, (HANDLE)(ULONG_PTR)0x1234);
    t_note("SetClipboardData bogus hglobal -> %p err=%lu", r, GetLastError());
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_set_gdi_formats_invalid_handle) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    SET_ERR(0);
    HANDLE r = SETD(CF_BITMAP, (HANDLE)(ULONG_PTR)0x1234);
    t_note("SetClipboardData(CF_BITMAP, bogus) -> %p err=%lu", r, GetLastError());
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_avail_no_open_needed) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "avail");
    CHECK(AVAIL(CF_TEXT));
    CHECK(AVAIL(CF_UNICODETEXT));
    CHECK(AVAIL(CF_OEMTEXT));
    CHECK(AVAIL(CF_LOCALE));
    CHECK(!AVAIL(CF_BITMAP));
    CHECK(!AVAIL(CF_HDROP));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_avail_invalid_formats) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "avail");
    UINT bad[] = { 0, 0xFFFF, 0xC000, 0xFFFFFFFF, 0x80000000, 0x10000, 0xBFFF };
    for (int i = 0; i < 7; i++) { SET_ERR(0); CHECKF(!AVAIL(bad[i]), "fmt %x", bad[i]); }
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_avail_empty) {
    CB_REQ_BASIC; cb_reset();
    for (UINT f = 1; f <= 20; f++) CHECKF(!AVAIL(f), "fmt %u", f);
    CHECK(COUNT() == 0);
    CHECK(ENUM(0) == 0);
}
TEST(clipboard_count_values) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    CHECK(COUNT() == 0);
    SETD(CF_PRIVATEFIRST, cb_text("1")); CHECK(COUNT() == 1);
    SETD(CF_PRIVATEFIRST + 1, cb_text("2")); CHECK(COUNT() == 2);
    SETD(CF_PRIVATEFIRST + 2, cb_text("3")); CHECK(COUNT() == 3);
    SETD(CF_PRIVATEFIRST + 1, cb_text("2b")); CHECK(COUNT() == 3);
    CLOSE(); CHECK(COUNT() == 3);
    OPEN(w); EMPTY(); CHECK(COUNT() == 0); CLOSE();
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_count_not_open_error) {
    CB_REQ_BASIC; cb_reset();
    SET_ERR(0); int n = COUNT();
    CHECK(n == 0);
}
TEST(clipboard_enum_order_and_terminator) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    SETD(CF_PRIVATEFIRST + 3, cb_text("a"));
    SETD(CF_PRIVATEFIRST + 1, cb_text("b"));
    SETD(CF_PRIVATEFIRST + 2, cb_text("c"));
    UINT seq[8]; int n = 0; UINT f = 0;
    while (n < 8 && (f = ENUM(f))) seq[n++] = f;
    CHECK(n == 3);
    CHECK(f == 0);
    if (n == 3) {
        t_note("enum order: %x %x %x", seq[0], seq[1], seq[2]);
        int ok = 0;
        for (int i = 0; i < 3; i++) if (seq[i] == CF_PRIVATEFIRST + 3) ok++;
        CHECK(ok == 1);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_enum_matches_count) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "enumcount");
    OPEN(w);
    int n = 0; UINT f = 0;
    while (n < 100 && (f = ENUM(f))) n++;
    CHECK(n == COUNT()); CHECK(n >= 3);
    CLOSE();
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_enum_empty_returns_zero) {
    CB_REQ_BASIC; cb_reset();
    SET_ERR(0); CHECK(ENUM(0) == 0);
    t_note("EnumClipboardFormats(0) on empty: err=%lu", GetLastError());
}
TEST(clipboard_enum_unknown_start) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "e");
    SET_ERR(0); UINT r = ENUM(0xC999);
    t_note("EnumClipboardFormats(unlisted) -> %u err=%lu", r, GetLastError());
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_enum_last_returns_zero_noerror) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("x"));
    UINT f = ENUM(0); CHECK(f == CF_PRIVATEFIRST);
    SET_ERR(0x77); CHECK(ENUM(f) == 0);
    EXPECT_ERR(0, ERROR_SUCCESS);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_empty_removes_formats) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "gone");
    CHECK(AVAIL(CF_TEXT));
    OPEN(w); EMPTY(); CLOSE();
    CHECK(!AVAIL(CF_TEXT)); CHECK(!AVAIL(CF_UNICODETEXT)); CHECK(COUNT() == 0);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_empty_frees_data) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    HGLOBAL g = cb_text("free me");
    SETD(CF_PRIVATEFIRST, g);
    EMPTY();
    /* ownership was transferred; the handle should be invalid now */
    CHECK(COUNT() == 0);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_data_persists_after_owner_destroyed) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "persist");
    cb_destroy(w);
    CHECK(AVAIL(CF_TEXT));
    OPEN(NULL);
    HANDLE d = GETD(CF_TEXT);
    CHECK(d != NULL);
    if (d) { char *p = GlobalLock(d); CHECK(p && !strcmp(p, "persist")); GlobalUnlock(d); }
    CLOSE(); cb_reset();
}
TEST(clipboard_get_while_other_window_open) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); put_text(w, "x");
    HWND o = cb_win();
    OPEN(o);
    CHECK(GETD(CF_TEXT) != NULL);
    CLOSE(); cb_reset(); cb_destroy(w); cb_destroy(o);
}
TEST(clipboard_many_private_formats) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    int ok = 0;
    for (UINT f = CF_PRIVATEFIRST; f <= CF_PRIVATELAST; f++) if (SETD(f, cb_text("p"))) ok++;
    CHECK(ok == 0x100);
    CHECK(COUNT() == 0x100);
    int n = 0; UINT f = 0; while (n < 1000 && (f = ENUM(f))) n++;
    CHECK(n == 0x100);
    EMPTY(); CHECK(COUNT() == 0);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_set_zero_size_block) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 0);
    HANDLE r = SETD(CF_PRIVATEFIRST, g);
    t_note("zero size hglobal -> %p", r);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_large_private_data) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    size_t n = 8 * 1024 * 1024;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n);
    CHECK(g);
    if (g) {
        unsigned char *p = GlobalLock(g); memset(p, 0xA5, n); p[n - 1] = 0x5A; GlobalUnlock(g);
        CHECK(SETD(CF_PRIVATEFIRST, g) == g);
        HANDLE d = GETD(CF_PRIVATEFIRST);
        CHECK(d != NULL);
        if (d) { CHECK(GlobalSize(d) >= n); p = GlobalLock(d); CHECK(p && p[0] == 0xA5 && p[n - 1] == 0x5A); GlobalUnlock(d); }
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_large_text_16mb) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    size_t n = 16 * 1024 * 1024;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n);
    if (g) {
        char *p = GlobalLock(g); memset(p, 'a', n - 1); p[n - 1] = 0; GlobalUnlock(g);
        CHECK(SETD(CF_TEXT, g) == g);
        HANDLE d = GETD(CF_TEXT);
        CHECK(d == g);
        CHECK(AVAIL(CF_TEXT));
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_text_with_embedded_data_after_nul) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    HGLOBAL g = cb_mem("ab\0cd\0", 6);
    CHECK(SETD(CF_TEXT, g) == g);
    HANDLE d = GETD(CF_TEXT);
    CHECK(d == g);
    if (d) CHECK(GlobalSize(d) >= 6);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_set_hdrop_like_data) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(CBDROPFILES) + 8);
    CBDROPFILES *df = GlobalLock(g); df->pFiles = sizeof(CBDROPFILES); df->fWide = TRUE; GlobalUnlock(g);
    CHECK(SETD(CF_HDROP, g) == g);
    CHECK(AVAIL(CF_HDROP));
    CHECK(GETD(CF_HDROP) == g);
    CLOSE(); cb_reset(); cb_destroy(w);
}
