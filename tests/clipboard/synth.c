#include "cbcommon.h"
/* Synthesized formats, GDI handle formats, priority list, sequence number. */

static HGLOBAL dib24(int w, int h, unsigned char fill) {
    int stride = (w * 3 + 3) & ~3;
    size_t n = sizeof(BITMAPINFOHEADER) + stride * h;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, n);
    unsigned char *p = GlobalLock(g);
    BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *)p;
    bi->biSize = sizeof *bi; bi->biWidth = w; bi->biHeight = h; bi->biPlanes = 1; bi->biBitCount = 24;
    bi->biSizeImage = stride * h;
    memset(p + sizeof *bi, fill, stride * h);
    GlobalUnlock(g);
    return g;
}
static const char *gstr(HANDLE h) { return h ? (const char *)GlobalLock(h) : NULL; }

TEST(clipboard_synth_text_gives_unicode_oem_locale) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("synth text")); CLOSE();
    CHECK(AVAIL(CF_TEXT)); CHECK(AVAIL(CF_UNICODETEXT)); CHECK(AVAIL(CF_OEMTEXT)); CHECK(AVAIL(CF_LOCALE));
    CHECK(COUNT() == 4);
    CHECK(!AVAIL(CF_BITMAP)); CHECK(!AVAIL(CF_DIB));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_unicode_from_text_content) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("abc XYZ 123")); CLOSE();
    OPEN(w);
    HANDLE h = GETD(CF_UNICODETEXT);
    CHECK(h != NULL);
    if (h) { WCHAR *p = GlobalLock(h); CHECK(p && !lstrcmpW(p, L"abc XYZ 123")); CHECK(GlobalSize(h) >= 24); GlobalUnlock(h); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_text_from_unicode_content) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_UNICODETEXT, cb_wtext(L"wide only")); CLOSE();
    CHECK(AVAIL(CF_TEXT)); CHECK(AVAIL(CF_OEMTEXT)); CHECK(AVAIL(CF_UNICODETEXT)); CHECK(AVAIL(CF_LOCALE));
    OPEN(w);
    HANDLE h = GETD(CF_TEXT);
    CHECK(h != NULL);
    if (h) { const char *p = gstr(h); CHECK(p && !strcmp(p, "wide only")); GlobalUnlock(h); }
    h = GETD(CF_OEMTEXT);
    CHECK(h != NULL);
    if (h) { const char *p = gstr(h); CHECK(p && !strcmp(p, "wide only")); GlobalUnlock(h); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_from_oemtext) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_OEMTEXT, cb_text("oem text")); CLOSE();
    CHECK(AVAIL(CF_TEXT)); CHECK(AVAIL(CF_UNICODETEXT)); CHECK(AVAIL(CF_OEMTEXT)); CHECK(AVAIL(CF_LOCALE));
    OPEN(w);
    HANDLE h = GETD(CF_UNICODETEXT);
    CHECK(h != NULL);
    if (h) { WCHAR *p = GlobalLock(h); CHECK(p && !lstrcmpW(p, L"oem text")); GlobalUnlock(h); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_explicit_not_overwritten) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    HGLOBAL a = cb_text("narrow"), b = cb_wtext(L"different wide");
    SETD(CF_TEXT, a); SETD(CF_UNICODETEXT, b);
    CHECK(GETD(CF_TEXT) == a);
    CHECK(GETD(CF_UNICODETEXT) == b);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_get_handle_stable) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("stable")); CLOSE(); OPEN(w);
    HANDLE a = GETD(CF_UNICODETEXT), b = GETD(CF_UNICODETEXT);
    CHECK(a != NULL); CHECK(b != NULL);
    /* Win10 22H2 oracle: a repeated GetClipboardData of a synthesized format
     * does not return the identical handle. Handle identity is not a contract;
     * the content must be stable. */
    if (a && b) {
        WCHAR *pa = GlobalLock(a), *pb = GlobalLock(b);
        CHECK(pa && pb && !lstrcmpW(pa, pb) && !lstrcmpW(pa, L"stable"));
        if (pa) GlobalUnlock(a);
        if (pb) GlobalUnlock(b);
    }
    t_note("synthesized handle identical across two gets: %d", a == b);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_get_before_close) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("early"));
    HANDLE h = GETD(CF_UNICODETEXT);
    t_note("synthesized GetClipboardData while still open -> %p", h);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_locale_data) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("loc")); CLOSE();
    OPEN(w);
    HANDLE h = GETD(CF_LOCALE);
    CHECK(h != NULL);
    if (h) {
        CHECK(GlobalSize(h) >= sizeof(DWORD));
        DWORD *p = GlobalLock(h);
        if (p) { t_note("CF_LOCALE lcid=0x%lx user lcid=0x%lx", *p, GetUserDefaultLCID()); CHECK(*p != 0); }
        GlobalUnlock(h);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_locale_explicit) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    DWORD lc = 0x0409; HGLOBAL g = cb_mem(&lc, 4);
    SETD(CF_TEXT, cb_text("loc")); CHECK(SETD(CF_LOCALE, g) == g);
    CHECK(GETD(CF_LOCALE) == g);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_no_text_synth_for_private) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("p")); CLOSE();
    CHECK(!AVAIL(CF_TEXT)); CHECK(!AVAIL(CF_UNICODETEXT)); CHECK(!AVAIL(CF_LOCALE));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_dsptext_not_synthesized) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_DSPTEXT, cb_text("dsp")); CLOSE();
    CHECK(AVAIL(CF_DSPTEXT)); CHECK(!AVAIL(CF_TEXT)); CHECK(!AVAIL(CF_UNICODETEXT));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_text_ascii_ladder) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    int lens[] = { 0, 1, 2, 7, 255, 256, 1000, 70000 };
    for (int i = 0; i < 8; i++) {
        int n = lens[i];
        char *s = malloc(n + 1); memset(s, 'a' + i, n); s[n] = 0;
        OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text(s)); CLOSE();
        OPEN(w);
        HANDLE h = GETD(CF_UNICODETEXT);
        CHECKF(h != NULL, "len %d", n);
        if (h) { WCHAR *p = GlobalLock(h); int ok = p && lstrlenW(p) == n; for (int k = 0; ok && k < n; k++) ok = p[k] == (WCHAR)s[k]; CHECKF(ok, "len %d", n); GlobalUnlock(h); }
        CLOSE(); free(s);
    }
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_crlf_untouched) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("a\r\nb\nc\r")); CLOSE();
    OPEN(w);
    HANDLE h = GETD(CF_UNICODETEXT);
    CHECK(h != NULL);
    if (h) { WCHAR *p = GlobalLock(h); CHECK(p && !lstrcmpW(p, L"a\r\nb\nc\r")); GlobalUnlock(h); }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_latin1_roundtrip_observed) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_UNICODETEXT, cb_wtext(L"caf\x00e9 \x4e2d")); CLOSE();
    OPEN(w);
    HANDLE h = GETD(CF_TEXT);
    CHECK(h != NULL);
    if (h) { const unsigned char *p = (const unsigned char *)gstr(h); t_note("CF_TEXT from unicode: len=%zu c4=0x%x", strlen((const char *)p), p[3]); CHECK(p[0] == 'c' && p[1] == 'a' && p[2] == 'f'); GlobalUnlock(h); }
    CLOSE(); cb_reset(); cb_destroy(w);
}

/* bitmaps */
TEST(clipboard_synth_dib_gives_bitmap_dibv5) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); CHECK(SETD(CF_DIB, dib24(4, 3, 0x80))); CLOSE();
    CHECK(AVAIL(CF_DIB)); CHECK(AVAIL(CF_BITMAP)); CHECK(AVAIL(CF_DIBV5));
    CHECK(!AVAIL(CF_TEXT));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_bitmap_from_dib_properties) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_DIB, dib24(5, 3, 0x40)); CLOSE();
    OPEN(w);
    HBITMAP hb = (HBITMAP)GETD(CF_BITMAP);
    CHECK(hb != NULL);
    if (hb) {
        BITMAP bm; memset(&bm, 0, sizeof bm);
        CHECK(GetObjectA(hb, sizeof bm, &bm) == sizeof bm);
        CHECK(bm.bmWidth == 5); CHECK(bm.bmHeight == 3);
        CHECK(GetObjectType(hb) == OBJ_BITMAP);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_dibv5_header) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_DIB, dib24(2, 2, 0x10)); CLOSE();
    OPEN(w);
    HANDLE h = GETD(CF_DIBV5);
    CHECK(h != NULL);
    if (h) {
        BITMAPV5HEADER *bi = GlobalLock(h);
        CHECK(bi && bi->bV5Size == sizeof(BITMAPV5HEADER));
        CHECK(bi && bi->bV5Width == 2); CHECK(bi && abs(bi->bV5Height) == 2);
        CHECK(bi && bi->bV5Planes == 1);
        GlobalUnlock(h);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_dib_from_bitmap) {
    CB_REQ_BASIC; cb_reset();
    HBITMAP hb = CreateBitmap(6, 4, 1, 32, NULL);
    HWND w = cb_win(); OPEN(w); EMPTY();
    CHECK(SETD(CF_BITMAP, hb) == hb);
    CLOSE();
    CHECK(AVAIL(CF_BITMAP)); CHECK(AVAIL(CF_DIB)); CHECK(AVAIL(CF_DIBV5));
    OPEN(w);
    HANDLE h = GETD(CF_DIB);
    CHECK(h != NULL);
    if (h) {
        BITMAPINFOHEADER *bi = GlobalLock(h);
        CHECK(bi && bi->biSize >= sizeof(BITMAPINFOHEADER));
        CHECK(bi && bi->biWidth == 6); CHECK(bi && abs(bi->biHeight) == 4);
        CHECK(bi && bi->biPlanes == 1);
        GlobalUnlock(h);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_bitmap_set_returns_handle) {
    CB_REQ_BASIC; cb_reset();
    HBITMAP hb = CreateBitmap(2, 2, 1, 1, NULL);
    HWND w = cb_win(); OPEN(w); EMPTY();
    CHECK(SETD(CF_BITMAP, hb) == hb);
    CHECK(GETD(CF_BITMAP) == hb);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_dibv5_gives_dib_bitmap) {
    CB_REQ_BASIC; cb_reset();
    int w_ = 3, h_ = 2, stride = 12;
    size_t n = sizeof(BITMAPV5HEADER) + stride * h_;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, n);
    BITMAPV5HEADER *bi = GlobalLock(g);
    bi->bV5Size = sizeof *bi; bi->bV5Width = w_; bi->bV5Height = h_; bi->bV5Planes = 1; bi->bV5BitCount = 24;
    bi->bV5Compression = BI_RGB; bi->bV5SizeImage = stride * h_;
    GlobalUnlock(g);
    HWND w = cb_win(); OPEN(w); EMPTY(); CHECK(SETD(CF_DIBV5, g)); CLOSE();
    CHECK(AVAIL(CF_DIBV5)); CHECK(AVAIL(CF_DIB)); CHECK(AVAIL(CF_BITMAP));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_dib_8bit_gives_palette) {
    CB_REQ_BASIC; cb_reset();
    size_t n = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD) + 4 * 2;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, n);
    BITMAPINFOHEADER *bi = GlobalLock(g);
    bi->biSize = sizeof *bi; bi->biWidth = 4; bi->biHeight = 2; bi->biPlanes = 1; bi->biBitCount = 8; bi->biClrUsed = 256;
    RGBQUAD *q = (RGBQUAD *)(bi + 1); for (int i = 0; i < 256; i++) q[i].rgbRed = i;
    GlobalUnlock(g);
    HWND w = cb_win(); OPEN(w); EMPTY(); CHECK(SETD(CF_DIB, g)); CLOSE();
    CHECK(AVAIL(CF_DIB)); CHECK(AVAIL(CF_BITMAP));
    t_note("palette synthesized for 8bpp DIB: %d", AVAIL(CF_PALETTE));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_palette_roundtrip) {
    CB_REQ_BASIC; cb_reset();
    LOGPALETTE *lp = calloc(1, sizeof(LOGPALETTE) + 3 * sizeof(PALETTEENTRY));
    lp->palVersion = 0x300; lp->palNumEntries = 4;
    HPALETTE hp = CreatePalette(lp); free(lp);
    CHECK(hp);
    HWND w = cb_win(); OPEN(w); EMPTY();
    CHECK(SETD(CF_PALETTE, hp) == hp);
    CHECK(AVAIL(CF_PALETTE));
    CHECK(GETD(CF_PALETTE) == hp);
    CLOSE(); cb_reset(); cb_destroy(w);
}

/* metafiles */
static HENHMETAFILE mk_emf(void) {
    HDC dc = CreateEnhMetaFileA(NULL, NULL, NULL, NULL);
    if (!dc) return NULL;
    Rectangle(dc, 0, 0, 20, 20);
    return CloseEnhMetaFile(dc);
}
TEST(clipboard_synth_emf_gives_metafilepict) {
    CB_REQ_BASIC; cb_reset();
    HENHMETAFILE e = mk_emf(); CHECK(e);
    HWND w = cb_win(); OPEN(w); EMPTY();
    CHECK(SETD(CF_ENHMETAFILE, e) == e);
    CLOSE();
    CHECK(AVAIL(CF_ENHMETAFILE)); CHECK(AVAIL(CF_METAFILEPICT));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_emf_to_wmf_data) {
    CB_REQ_BASIC; cb_reset();
    HENHMETAFILE e = mk_emf();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_ENHMETAFILE, e); CLOSE();
    OPEN(w);
    HANDLE h = GETD(CF_METAFILEPICT);
    CHECK(h != NULL);
    if (h) {
        METAFILEPICT *mp = GlobalLock(h);
        CHECK(mp && mp->hMF != NULL);
        CHECK(mp && mp->mm > 0);
        if (mp && mp->hMF) CHECK(GetObjectType(mp->hMF) == OBJ_METAFILE);
        GlobalUnlock(h);
    }
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_synth_wmf_gives_emf) {
    CB_REQ_BASIC; cb_reset();
    HDC dc = CreateMetaFileA(NULL);
    Rectangle(dc, 0, 0, 10, 10);
    HMETAFILE mf = CloseMetaFile(dc);
    CHECK(mf);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
    METAFILEPICT *mp = GlobalLock(g); mp->mm = MM_ANISOTROPIC; mp->xExt = 100; mp->yExt = 100; mp->hMF = mf; GlobalUnlock(g);
    HWND w = cb_win(); OPEN(w); EMPTY();
    CHECK(SETD(CF_METAFILEPICT, g) == g);
    CLOSE();
    CHECK(AVAIL(CF_METAFILEPICT)); CHECK(AVAIL(CF_ENHMETAFILE));
    OPEN(w);
    HENHMETAFILE e = (HENHMETAFILE)GETD(CF_ENHMETAFILE);
    CHECK(e != NULL);
    if (e) CHECK(GetObjectType(e) == OBJ_ENHMETAFILE);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_emf_handle_returned_by_set_and_get) {
    CB_REQ_BASIC; cb_reset();
    HENHMETAFILE e = mk_emf();
    HWND w = cb_win(); OPEN(w); EMPTY();
    SETD(CF_ENHMETAFILE, e);
    CHECK(GETD(CF_ENHMETAFILE) == e);
    CLOSE(); cb_reset(); cb_destroy(w);
}

/* priority list */
TEST(clipboard_priority_empty_clipboard) {
    CB_REQ_BASIC; REQUIRE(U(GetPriorityClipboardFormat)); cb_reset();
    UINT l[2] = { CF_TEXT, CF_UNICODETEXT };
    CHECK(U(GetPriorityClipboardFormat)(l, 2) == 0);
}
TEST(clipboard_priority_picks_first_available) {
    CB_REQ_BASIC; REQUIRE(U(GetPriorityClipboardFormat)); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST + 2, cb_text("x")); SETD(CF_PRIVATEFIRST + 4, cb_text("y")); CLOSE();
    UINT l1[3] = { CF_PRIVATEFIRST + 4, CF_PRIVATEFIRST + 2, CF_TEXT };
    UINT l2[3] = { CF_TEXT, CF_PRIVATEFIRST + 2, CF_PRIVATEFIRST + 4 };
    UINT l3[1] = { CF_PRIVATEFIRST + 3 };
    CHECK(U(GetPriorityClipboardFormat)(l1, 3) == CF_PRIVATEFIRST + 4);
    CHECK(U(GetPriorityClipboardFormat)(l2, 3) == CF_PRIVATEFIRST + 2);
    CHECK(U(GetPriorityClipboardFormat)(l3, 1) == (UINT)-1);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_priority_synthesized) {
    CB_REQ_BASIC; REQUIRE(U(GetPriorityClipboardFormat)); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("t")); CLOSE();
    UINT l[2] = { CF_UNICODETEXT, CF_TEXT };
    CHECK(U(GetPriorityClipboardFormat)(l, 2) == CF_UNICODETEXT);
    UINT l2[1] = { CF_OEMTEXT };
    CHECK(U(GetPriorityClipboardFormat)(l2, 1) == CF_OEMTEXT);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_priority_zero_count) {
    CB_REQ_BASIC; REQUIRE(U(GetPriorityClipboardFormat)); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("t")); CLOSE();
    UINT l[1] = { CF_TEXT };
    SET_ERR(0);
    UINT r = U(GetPriorityClipboardFormat)(l, 0);
    t_note("GetPriorityClipboardFormat(n=0) -> %u err=%lu", r, GetLastError());
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_priority_zero_entries) {
    CB_REQ_BASIC; REQUIRE(U(GetPriorityClipboardFormat)); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("t")); CLOSE();
    UINT l[2] = { 0, CF_TEXT };
    UINT r = U(GetPriorityClipboardFormat)(l, 2);
    t_note("priority list with 0 entry first -> %u", r);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_priority_no_open_needed) {
    CB_REQ_BASIC; REQUIRE(U(GetPriorityClipboardFormat)); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("t")); 
    UINT l[1] = { CF_TEXT };
    CHECK(U(GetPriorityClipboardFormat)(l, 1) == CF_TEXT);
    CLOSE(); cb_reset(); cb_destroy(w);
}

/* sequence number */
TEST(clipboard_seq_increments_on_change) {
    CB_REQ_BASIC; REQUIRE(SEQ); cb_reset();
    HWND w = cb_win();
    DWORD s0 = SEQ();
    OPEN(w); EMPTY(); CLOSE();
    DWORD s1 = SEQ();
    t_note("seq after Empty on empty: %lu -> %lu", s0, s1);
    OPEN(w); SETD(CF_TEXT, cb_text("s")); CLOSE();
    DWORD s2 = SEQ();
    CHECK(s2 != s0);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_seq_stable_on_reads) {
    CB_REQ_BASIC; REQUIRE(SEQ); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("r")); CLOSE();
    DWORD s = SEQ();
    for (int i = 0; i < 10; i++) { OPEN(w); GETD(CF_TEXT); AVAIL(CF_TEXT); COUNT(); CLOSE(); }
    EXPECT_EQ(SEQ(), s);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_seq_nonzero) {
    CB_REQ_BASIC; REQUIRE(SEQ);
    CHECK(SEQ() != 0);
}
TEST(clipboard_seq_increases_each_set) {
    CB_REQ_BASIC; REQUIRE(SEQ); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    DWORD prev = SEQ(); int inc = 0;
    for (UINT i = 0; i < 10; i++) {
        SETD(CF_PRIVATEFIRST + i, cb_text("q"));
        DWORD s = SEQ(); if (s != prev) inc++; prev = s;
    }
    t_note("sequence changed on %d of 10 Set calls (while open)", inc);
    CLOSE();
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_seq_open_close_no_change) {
    CB_REQ_BASIC; REQUIRE(SEQ); cb_reset();
    DWORD s = SEQ();
    for (int i = 0; i < 10; i++) { OPEN(NULL); CLOSE(); }
    EXPECT_EQ(SEQ(), s);
}
static DWORD g_seq_thr;
static DWORD WINAPI seq_thread(LPVOID p) { g_seq_thr = SEQ(); return 0; }
TEST(clipboard_seq_same_across_threads) {
    CB_REQ_BASIC; REQUIRE(SEQ); cb_reset();
    HANDLE h = CreateThread(NULL, 0, seq_thread, NULL, 0, NULL);
    if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); }
    EXPECT_EQ(g_seq_thr, SEQ());
}
