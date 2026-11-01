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
    EXPECT_EQ(U(GetPriorityClipboardFormat)(l2, 2), (UINT)-1);   /* documented: data present, none in list -> -1 */
    cb_reset();
    EXPECT_EQ(U(GetPriorityClipboardFormat)(l1, 3), 0);          /* empty clipboard: 0 */
    cb_destroy(w);
}

TEST(clipboard_x3_gdiobj_range_object_released_on_empty) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    if (!OPEN(w)) { t_skip("clipboard busy err %lu", GetLastError()); cb_destroy(w); return; }
    EMPTY();
    HBITMAP bm = CreateBitmap(8, 8, 1, 1, NULL);
    REQUIRE(bm);
    HANDLE r = SETD(CF_GDIOBJFIRST + 3, bm);
    CHECK(r == bm);
    CHECK(AVAIL(CF_GDIOBJFIRST + 3));
    CHECK(GETD(CF_GDIOBJFIRST + 3) == bm);
    CHECK(EMPTY());
    BITMAP b;
    int alive = GetObjectW(bm, sizeof b, &b);
    t_note("GDI-object format bitmap after EmptyClipboard: GetObject=%d", alive);
    if (alive) DeleteObject(bm);
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
