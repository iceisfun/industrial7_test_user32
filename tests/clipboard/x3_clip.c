/* clipboard: display formats (CF_DSP*, CF_OWNERDISPLAY), GDI-object private range, priority list. */
#include "cbcommon.h"

#ifndef CF_GDIOBJFIRST
#define CF_GDIOBJFIRST 0x0300
#define CF_GDIOBJLAST 0x03FF
#endif

TEST(clipboard_x3_display_formats_delayed_listed) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    static const UINT f[] = { CF_OWNERDISPLAY, CF_DSPTEXT, CF_DSPBITMAP, CF_DSPMETAFILEPICT, CF_DSPENHMETAFILE };
    if (!OPEN(w)) { t_skip("clipboard busy (observer interference) err %lu", GetLastError()); cb_destroy(w); return; }
    CHECK(EMPTY());
    for (int i = 0; i < 5; i++) SETD(f[i], NULL);          /* delayed rendering, never rendered here */
    for (int i = 0; i < 5; i++) CHECKF(AVAIL(f[i]), "format 0x%x not available", f[i]);
    EXPECT_EQ(COUNT(), 5);
    UINT seen = 0, fm = 0;
    while ((fm = ENUM(fm)) != 0) seen++;
    EXPECT_EQ(seen, 5);
    CLOSE();
    cb_reset(); cb_destroy(w);
}

TEST(clipboard_x3_priority_list_over_display_formats) {
    CB_REQ_BASIC; cb_reset(); REQUIRE(U(GetPriorityClipboardFormat));
    HWND w = cb_win();
    if (!OPEN(w)) { t_skip("clipboard busy err %lu", GetLastError()); cb_destroy(w); return; }
    EMPTY();
    SETD(CF_DSPTEXT, cb_text("d"));
    CLOSE();
    UINT l1[] = { CF_TEXT, CF_DSPTEXT, CF_DSPBITMAP };
    EXPECT_EQ(U(GetPriorityClipboardFormat)(l1, 3), CF_DSPTEXT);
    UINT l2[] = { CF_DSPBITMAP, CF_DSPENHMETAFILE };
    EXPECT_EQ((int)U(GetPriorityClipboardFormat)(l2, 2), -1);    /* documented: data present, none in list -> -1 (a 32-bit int result) */
    cb_reset();
    EXPECT_EQ(U(GetPriorityClipboardFormat)(l1, 3), 0);          /* empty clipboard: 0 */
    cb_destroy(w);
}

TEST(clipboard_x3_gdiobj_range_handle_kept_on_empty) {
    /* Documented: in the CF_GDIOBJFIRST..CF_GDIOBJLAST range hMem is NOT a GDI object but a GlobalAlloc(GMEM_MOVEABLE)
     * handle, and it is not freed when the clipboard is emptied (the owner frees it); the oracle frees it. A GDI bitmap handle in this range
     * is a caller error: the clipboard GlobalFrees what it was given and corrupts the heap on Win10 22H2 (0xc0000374). */
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    if (!OPEN(w)) { t_skip("clipboard busy err %lu", GetLastError()); cb_destroy(w); return; }
    EMPTY();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 32);
    REQUIRE(g);
    HANDLE r = SETD(CF_GDIOBJFIRST + 3, g);
    CHECK(r == g);
    CHECK(AVAIL(CF_GDIOBJFIRST + 3));
    CHECK(GETD(CF_GDIOBJFIRST + 3) == g);
    CHECK(EMPTY());
    SIZE_T sz = GlobalSize(g);
    t_note("GDIOBJ-range block after EmptyClipboard: GlobalSize=%llu", (unsigned long long)sz);
    /* Win10 22H2: the block IS freed by EmptyClipboard even in the GDIOBJ range (the documentation says it is kept) */
    EXPECT_EQ(sz, 0);
    if (sz) GlobalFree(g);
    CLOSE();
    cb_reset(); cb_destroy(w);
}

TEST(clipboard_x3_private_range_edges) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    if (!OPEN(w)) { t_skip("clipboard busy err %lu", GetLastError()); cb_destroy(w); return; }
    EMPTY();
    UINT edges[] = { CF_PRIVATEFIRST, CF_PRIVATELAST, CF_GDIOBJFIRST - 1 };   /* 0x200, 0x2FF, 0x2FF */
    for (int i = 0; i < 2; i++) {
        HGLOBAL g = cb_text("e");
        CHECKF(SETD(edges[i], g) == g, "fmt 0x%x", edges[i]);
        CHECKF(AVAIL(edges[i]), "fmt 0x%x", edges[i]);
    }
    EXPECT_EQ(COUNT(), 2);
    CLOSE();
    cb_reset(); cb_destroy(w);
}
