#include "cbcommon.h"
/* Additional depth: per-format table tests, content integrity, cross-thread reads, pixel checks. */

static int blk_eq(HANDLE h, const void *d, size_t n) {
    if (!h) return 0;
    if (GlobalSize(h) < n) return 0;
    unsigned char *p = GlobalLock(h);
    int ok = p && !memcmp(p, d, n);
    GlobalUnlock(h);
    return ok;
}

/* ---- every plain HGLOBAL predefined format ---- */
static const UINT g_plain[] = { CF_TEXT, CF_OEMTEXT, CF_DSPTEXT, CF_UNICODETEXT, CF_SYLK, CF_DIF, CF_TIFF,
                                CF_PENDATA, CF_RIFF, CF_WAVE, CF_HDROP, CF_LOCALE };
TEST(clipboard_table_plain_formats_set_get) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    for (unsigned i = 0; i < sizeof g_plain / sizeof g_plain[0]; i++) {
        UINT f = g_plain[i];
        OPEN(w); EMPTY();
        char data[32]; memset(data, 'A' + (int)i, sizeof data); data[30] = 0; data[31] = 0;
        if (f == CF_UNICODETEXT || f == CF_HDROP) { memset(data, 0, sizeof data); if (f == CF_HDROP) data[0] = 20; else data[0] = 'x'; }
        HGLOBAL g = cb_mem(data, sizeof data);
        CHECKF(SETD(f, g) == g, "fmt %u", f);
        CHECKF(AVAIL(f), "fmt %u", f);
        CHECKF(GETD(f) == g, "fmt %u", f);
        CLOSE();
        CHECKF(AVAIL(f), "fmt %u after close", f);
        OPEN(w);
        HANDLE d = GETD(f);
        CHECKF(d != NULL, "fmt %u reopen", f);
        if (d && f != CF_LOCALE) CHECKF(blk_eq(d, data, sizeof data), "fmt %u content", f);
        EMPTY();
        CHECKF(!AVAIL(f), "fmt %u after empty", f);
        CLOSE();
    }
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_table_formats_do_not_leak_between_sets) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("one")); CLOSE();
    OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST + 1, cb_text("two")); CLOSE();
    CHECK(!AVAIL(CF_PRIVATEFIRST)); CHECK(AVAIL(CF_PRIVATEFIRST + 1)); CHECK(COUNT() == 1);
    cb_reset(); cb_destroy(w);
}

/* ---- content integrity ---- */
TEST(clipboard_content_size_ladder) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    size_t sizes[] = { 1, 2, 3, 7, 8, 4095, 4096, 4097, 65536, 65537, 1000000 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        size_t n = sizes[i];
        unsigned char *buf = malloc(n);
        for (size_t k = 0; k < n; k++) buf[k] = (unsigned char)(k * 31 + i);
        OPEN(w); EMPTY();
        HGLOBAL g = cb_mem(buf, n);
        CHECKF(SETD(CF_PRIVATEFIRST + 7, g) == g, "size %zu", n);
        CLOSE();
        OPEN(NULL);
        HANDLE d = GETD(CF_PRIVATEFIRST + 7);
        CHECKF(d != NULL && blk_eq(d, buf, n), "size %zu content", n);
        CLOSE();
        free(buf);
    }
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_content_registered_roundtrip) {
    CB_REQ_BASIC; REQUIRE(U(RegisterClipboardFormatA)); cb_reset();
    UINT f = U(RegisterClipboardFormatA)("T32_extra_content_fmt");
    HWND w = cb_win();
    unsigned char data[300]; for (int i = 0; i < 300; i++) data[i] = (unsigned char)(i ^ 0x5a);
    OPEN(w); EMPTY(); CHECK(SETD(f, cb_mem(data, sizeof data))); CLOSE();
    OPEN(NULL); HANDLE d = GETD(f); CHECK(d && blk_eq(d, data, sizeof data)); CLOSE();
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_content_survives_other_reads) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("integrity")); CLOSE();
    for (int i = 0; i < 20; i++) {
        OPEN(NULL);
        HANDLE d = GETD(CF_TEXT);
        CHECKF(d && blk_eq(d, "integrity", 10), "iter %d", i);
        GETD(CF_UNICODETEXT); GETD(CF_OEMTEXT);
        CLOSE();
    }
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_content_text_empty_string) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("")); CLOSE();
    CHECK(AVAIL(CF_TEXT)); CHECK(AVAIL(CF_UNICODETEXT));
    OPEN(w); HANDLE h = GETD(CF_UNICODETEXT); CHECK(h != NULL);
    if (h) { WCHAR *p = GlobalLock(h); CHECK(p && p[0] == 0); GlobalUnlock(h); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_content_unicode_empty_to_text) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_UNICODETEXT, cb_wtext(L"")); CLOSE();
    OPEN(w); HANDLE h = GETD(CF_TEXT); CHECK(h != NULL);
    if (h) { char *p = GlobalLock(h); CHECK(p && p[0] == 0); GlobalUnlock(h); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_content_ansi_high_char_observed) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("a\xe9z")); CLOSE();
    OPEN(w); HANDLE h = GETD(CF_UNICODETEXT); CHECK(h != NULL);
    if (h) {
        WCHAR *p = GlobalLock(h);
        CHECK(p && p[0] == 'a' && p[2] == 'z' && p[3] == 0);
        WCHAR want[4]; MultiByteToWideChar(CP_ACP, 0, "a\xe9z", 4, want, 4);
        CHECKF(p && p[1] == want[1], "acp conversion %x vs %x", p ? p[1] : 0, want[1]);
        GlobalUnlock(h);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_content_unicode_to_text_matches_acp) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_UNICODETEXT, cb_wtext(L"b\x00e9y")); CLOSE();
    OPEN(w); HANDLE h = GETD(CF_TEXT); CHECK(h != NULL);
    if (h) {
        unsigned char *p = GlobalLock(h);
        char want[8] = {0}; WideCharToMultiByte(CP_ACP, 0, L"b\x00e9y", 4, want, 8, NULL, NULL);
        CHECKF(p && !strcmp((char *)p, want), "acp narrow conversion");
        GlobalUnlock(h);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_content_unicode_surrogates) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    WCHAR s[] = { 'x', 0xD83D, 0xDE00, 'y', 0 };
    OPEN(w); EMPTY(); SETD(CF_UNICODETEXT, cb_wtext(s)); CLOSE();
    OPEN(w); HANDLE h = GETD(CF_UNICODETEXT); CHECK(h && blk_eq(h, s, sizeof s)); CLOSE();
    cb_reset(); cb_destroy(w);
}

/* ---- enumeration ---- */
TEST(clipboard_enum_repeatable) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("r")); SETD(CF_PRIVATEFIRST, cb_text("p")); SETD(CF_PRIVATEFIRST + 9, cb_text("q"));
    UINT a[32], b[32]; int na = 0, nb = 0; UINT f = 0;
    while (na < 32 && (f = ENUM(f))) a[na++] = f;
    f = 0; while (nb < 32 && (f = ENUM(f))) b[nb++] = f;
    CHECK(na == nb); CHECK(na >= 3);
    for (int i = 0; i < na && i < nb; i++) CHECKF(a[i] == b[i], "idx %d", i);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_enum_no_duplicates) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("d")); SETD(CF_UNICODETEXT, cb_wtext(L"d"));
    CLOSE();
    OPEN(w);
    UINT a[32]; int n = 0; UINT f = 0;
    while (n < 32 && (f = ENUM(f))) a[n++] = f;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) CHECKF(a[i] != a[j], "dup %u", a[i]);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_enum_each_member_is_available) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("m")); CLOSE();
    OPEN(w);
    UINT f = 0; int n = 0;
    while (n++ < 32 && (f = ENUM(f))) CHECKF(AVAIL(f), "fmt %u", f);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_enum_after_empty_is_zero) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("p"));
    CHECK(ENUM(0) == CF_PRIVATEFIRST);
    EMPTY();
    CHECK(ENUM(0) == 0);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_enum_during_other_thread_open) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("p")); CLOSE();
    CHECK(COUNT() == 1);
    OPEN(w);
    CHECK(COUNT() == 1);
    CLOSE(); cb_reset(); cb_destroy(w);
}

/* ---- cross-thread data access ---- */
static volatile LONG g_xs; static int g_xok; static UINT g_xcount; static BOOL g_xavail;
static DWORD WINAPI reader_thread(LPVOID p) {
    g_xok = 0;
    for (int t = 0; t < 100; t++) {
        if (OPEN(NULL)) {
            HANDLE d = GETD(CF_PRIVATEFIRST + 3);
            g_xok = d && blk_eq(d, "xthread", 8);
            CLOSE();
            break;
        }
        Sleep(20);
    }
    g_xcount = COUNT(); g_xavail = AVAIL(CF_PRIVATEFIRST + 3);
    return 0;
}
TEST(clipboard_xthread_read_data_set_by_main) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST + 3, cb_text("xthread")); CLOSE();
    HANDLE h = CreateThread(NULL, 0, reader_thread, NULL, 0, NULL);
    CHECK(h);
    if (h) { CHECK(WaitForSingleObject(h, 8000) == WAIT_OBJECT_0); CloseHandle(h); }
    CHECK(g_xok); CHECK(g_xcount == 1); CHECK(g_xavail);
    cb_reset(); cb_destroy(w);
}
static DWORD WINAPI setter_thread(LPVOID p) {
    for (int t = 0; t < 100; t++) {
        if (OPEN(NULL)) { EMPTY(); g_xok = 0; CLOSE(); break; }
        Sleep(20);
    }
    return 0;
}
TEST(clipboard_xthread_empty_by_other_thread) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST + 3, cb_text("gone")); CLOSE();
    CHECK(AVAIL(CF_PRIVATEFIRST + 3));
    HANDLE h = CreateThread(NULL, 0, setter_thread, NULL, 0, NULL);
    if (h) { WaitForSingleObject(h, 8000); CloseHandle(h); }
    CHECK(!AVAIL(CF_PRIVATEFIRST + 3)); CHECK(COUNT() == 0);
    CHECK(U(GetClipboardOwner) ? U(GetClipboardOwner)() == NULL : 1);
    cb_reset(); cb_destroy(w);
}
static HWND g_tw2; static volatile LONG g_tstate;
static DWORD WINAPI holder_thread(LPVOID p) {
    g_tw2 = cb_win();
    BOOL ok = OPEN(g_tw2);
    g_tstate = ok ? 1 : -1;
    DWORD t = GetTickCount();
    while (g_tstate != 2 && GetTickCount() - t < 5000) Sleep(5);
    if (ok) CLOSE();
    cb_destroy(g_tw2);
    return 0;
}
TEST(clipboard_getopenwindow_other_thread) {
    CB_REQ_BASIC; REQUIRE(U(GetOpenClipboardWindow)); cb_reset();
    g_tstate = 0; g_tw2 = NULL;
    HANDLE h = CreateThread(NULL, 0, holder_thread, NULL, 0, NULL);
    DWORD t = GetTickCount();
    while (g_tstate == 0 && GetTickCount() - t < 5000) Sleep(5);
    if (g_tstate == 1) CHECK(U(GetOpenClipboardWindow)() == g_tw2);
    else t_skip("holder thread could not open clipboard");
    g_tstate = 2;
    if (h) { WaitForSingleObject(h, 8000); CloseHandle(h); }
    CHECK(U(GetOpenClipboardWindow)() == NULL);
}
TEST(clipboard_getopenwindow_open_by_null) {
    CB_REQ_BASIC; REQUIRE(U(GetOpenClipboardWindow)); cb_reset();
    CHECK(OPEN(NULL));
    CHECK(U(GetOpenClipboardWindow)() == NULL);
    CHECK(CLOSE());
}
TEST(clipboard_other_thread_cannot_close) {
    CB_REQ_BASIC; cb_reset();
    g_tstate = 0;
    HANDLE h = CreateThread(NULL, 0, holder_thread, NULL, 0, NULL);
    DWORD t = GetTickCount();
    while (g_tstate == 0 && GetTickCount() - t < 5000) Sleep(5);
    if (g_tstate == 1) {
        SET_ERR(0); CHECK(!CLOSE()); EXPECT_ERR(0, ERR_NOT_OPEN);
        CHECK(U(GetOpenClipboardWindow) ? U(GetOpenClipboardWindow)() == g_tw2 : 1);
    } else t_skip("holder thread could not open clipboard");
    g_tstate = 2;
    if (h) { WaitForSingleObject(h, 8000); CloseHandle(h); }
    CHECK(OPEN(NULL)); CHECK(CLOSE());
}

/* ---- pixel content of synthesized bitmaps ---- */
static HGLOBAL dib24f(int w, int h, unsigned char fill) {
    int stride = (w * 3 + 3) & ~3;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(BITMAPINFOHEADER) + stride * h);
    unsigned char *p = GlobalLock(g);
    BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *)p;
    bi->biSize = sizeof *bi; bi->biWidth = w; bi->biHeight = h; bi->biPlanes = 1; bi->biBitCount = 24; bi->biSizeImage = stride * h;
    memset(p + sizeof *bi, fill, stride * h);
    GlobalUnlock(g);
    return g;
}
static int bitmap_pixel(HBITMAP hb, int x, int y, COLORREF *out) {
    HDC dc = GetDC(NULL); HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, hb);
    COLORREF c = GetPixel(mem, x, y);
    SelectObject(mem, old); DeleteDC(mem); ReleaseDC(NULL, dc);
    if (c == CLR_INVALID) return 0;
    *out = c; return 1;
}
TEST(clipboard_pixels_white_dib_to_bitmap) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_DIB, dib24f(4, 4, 0xFF)); CLOSE();
    OPEN(w); HBITMAP hb = (HBITMAP)GETD(CF_BITMAP); CHECK(hb);
    COLORREF c = 0;
    if (hb) { CHECK(bitmap_pixel(hb, 1, 1, &c)); CHECK(c == RGB(255, 255, 255)); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_pixels_black_dib_to_bitmap) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_DIB, dib24f(4, 4, 0x00)); CLOSE();
    OPEN(w); HBITMAP hb = (HBITMAP)GETD(CF_BITMAP); CHECK(hb);
    COLORREF c = 1;
    if (hb) { CHECK(bitmap_pixel(hb, 2, 2, &c)); CHECK(c == RGB(0, 0, 0)); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_dib_to_dibv5_data_size) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_DIB, dib24f(8, 8, 0x33)); CLOSE();
    OPEN(w); HANDLE h = GETD(CF_DIBV5); CHECK(h);
    if (h) {
        BITMAPV5HEADER *bi = GlobalLock(h);
        CHECK(bi && bi->bV5BitCount >= 24);
        CHECK(GlobalSize(h) >= sizeof(BITMAPV5HEADER) + 8 * 8 * 3);
        GlobalUnlock(h);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_dib_dimensions_preserved) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    int dims[][2] = { {1, 1}, {3, 7}, {16, 2}, {33, 5} };
    for (int i = 0; i < 4; i++) {
        OPEN(w); EMPTY(); SETD(CF_DIB, dib24f(dims[i][0], dims[i][1], 0x20)); CLOSE();
        OPEN(w); HBITMAP hb = (HBITMAP)GETD(CF_BITMAP); CHECKF(hb, "dim %d", i);
        if (hb) { BITMAP bm; GetObjectA(hb, sizeof bm, &bm); CHECKF(bm.bmWidth == dims[i][0] && bm.bmHeight == dims[i][1], "dim %d", i); }
        CLOSE();
    }
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_bitmap_set_then_empty_deletes_nothing_visible) {
    CB_REQ_BASIC; cb_reset();
    HBITMAP hb = CreateBitmap(3, 3, 1, 1, NULL);
    HWND w = cb_win();
    OPEN(w); EMPTY(); CHECK(SETD(CF_BITMAP, hb) == hb); CHECK(EMPTY()); CHECK(!AVAIL(CF_BITMAP)); CLOSE();
    cb_reset(); cb_destroy(w);
}

/* ---- misc API semantics ---- */
TEST(clipboard_misc_open_empty_close_cycle_counts) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    for (int i = 0; i < 50; i++) {
        CHECKF(OPEN(w), "open %d", i); CHECK(EMPTY());
        SETD(CF_PRIVATEFIRST + (i & 7), cb_text("cyc"));
        CHECKF(COUNT() == 1, "count %d", i);
        CHECK(CLOSE());
    }
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_misc_setdata_after_close_fails) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); CLOSE();
    HGLOBAL g = cb_text("late");
    SET_ERR(0); CHECK(SETD(CF_TEXT, g) == NULL); EXPECT_ERR(0, ERR_NOT_OPEN);
    GlobalFree(g);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_misc_owner_after_open_null_only) {
    CB_REQ_BASIC; REQUIRE(U(GetClipboardOwner)); cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); CLOSE();
    OPEN(NULL); CLOSE();
    CHECK(U(GetClipboardOwner)() == w);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_misc_seq_bumps_on_each_empty_with_data) {
    CB_REQ_BASIC; REQUIRE(SEQ); cb_reset();
    HWND w = cb_win();
    for (int i = 0; i < 5; i++) {
        OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("s")); CLOSE();
        DWORD a = SEQ();
        OPEN(w); EMPTY(); CLOSE();
        CHECKF(SEQ() != a, "iter %d", i);
    }
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_misc_priority_list_ordering_matters) {
    CB_REQ_BASIC; REQUIRE(U(GetPriorityClipboardFormat)); cb_reset();
    HWND w = cb_win();
    OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("a")); SETD(CF_PRIVATEFIRST + 1, cb_text("b")); CLOSE();
    UINT a[2] = { CF_PRIVATEFIRST, CF_PRIVATEFIRST + 1 }, b[2] = { CF_PRIVATEFIRST + 1, CF_PRIVATEFIRST };
    CHECK(U(GetPriorityClipboardFormat)(a, 2) == CF_PRIVATEFIRST);
    CHECK(U(GetPriorityClipboardFormat)(b, 2) == CF_PRIVATEFIRST + 1);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_misc_registered_name_is_not_predefined) {
    REQUIRE(U(RegisterClipboardFormatA)); REQUIRE(U(GetClipboardFormatNameA));
    UINT f = U(RegisterClipboardFormatA)("T32_extra_name");
    CHECK(f > CF_DIBV5 && f >= 0xC000);
    char b[64]; CHECK(U(GetClipboardFormatNameA)(f, b, 64) == 14); CHECK(!strcmp(b, "T32_extra_name"));
}
TEST(clipboard_misc_wellknown_registered_names) {
    REQUIRE(U(RegisterClipboardFormatA));
    UINT a = U(RegisterClipboardFormatA)("HTML Format"), b = U(RegisterClipboardFormatA)("Rich Text Format");
    UINT c = U(RegisterClipboardFormatA)("FileNameW"), d = U(RegisterClipboardFormatA)("Shell IDList Array");
    CHECK(a >= 0xC000); CHECK(b >= 0xC000); CHECK(c >= 0xC000); CHECK(d >= 0xC000);
    CHECK(a != b && b != c && c != d && a != d);
    CHECK(U(RegisterClipboardFormatA)("html format") == a);
}
TEST(clipboard_misc_ids_in_atom_range_many) {
    REQUIRE(U(RegisterClipboardFormatW));
    WCHAR n[40]; int bad = 0;
    for (int i = 0; i < 100; i++) {
        wsprintfW(n, L"T32_extra_atom_%d", i);
        UINT f = U(RegisterClipboardFormatW)(n);
        if (f < 0xC000 || f > 0xFFFF) bad++;
    }
    CHECK(bad == 0);
}
