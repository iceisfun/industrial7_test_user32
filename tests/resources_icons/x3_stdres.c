/* resources_icons: stock OEM bitmaps / cursors / icons that no other test touches. */
#include "ri.h"

#define OIC_SAMPLE 32512
#define OIC_HAND 32513
#define OIC_QUES 32514
#define OIC_BANG 32515
#define OIC_NOTE 32516
#define OIC_WINLOGO 32517
#define OIC_ERROR OIC_HAND
#define OIC_WARNING OIC_BANG
#define OIC_INFORMATION OIC_NOTE

static const struct { const char *n; WORD id; } x3_obm[] = {
    {"REDUCE", 32749}, {"ZOOM", 32748}, {"RESTORE", 32747}, {"REDUCED", 32746}, {"ZOOMD", 32745},
    {"RESTORED", 32744}, {"UPARROWD", 32743}, {"DNARROWD", 32742}, {"RGARROWD", 32741}, {"LFARROWD", 32740},
    {"MNARROW", 32739}, {"UPARROWI", 32737}, {"DNARROWI", 32736}, {"RGARROWI", 32735}, {"LFARROWI", 32734},
    {"SIZE", 32766}, {"BTSIZE", 32761}, {"BTNCORNERS", 32758},
};
static const struct { const char *n; WORD id; } x3_obm_old[] = {
    {"OLD_CLOSE", 32767}, {"OLD_UPARROW", 32765}, {"OLD_DNARROW", 32764}, {"OLD_RGARROW", 32763},
    {"OLD_LFARROW", 32762}, {"OLD_REDUCE", 32757}, {"OLD_ZOOM", 32756}, {"OLD_RESTORE", 32755},
};
#define NOBM ((int)(sizeof x3_obm / sizeof x3_obm[0]))
#define NOBMOLD ((int)(sizeof x3_obm_old / sizeof x3_obm_old[0]))

TEST(resources_x3_obm_remaining_ids_load_as_bitmaps) {
    REQUIRE(U(LoadBitmapW));
    for (int i = 0; i < NOBM; i++) {
        HBITMAP b = U(LoadBitmapW)(NULL, MAKEINTRESOURCEW(x3_obm[i].id));
        CHECKF(b != NULL, "OBM_%s err %lu", x3_obm[i].n, GetLastError());
        if (!b) continue;
        BITMAP bm; memset(&bm, 0, sizeof bm);
        CHECKF(GetObjectW(b, sizeof bm, &bm) == sizeof bm, "OBM_%s GetObject", x3_obm[i].n);
        CHECKF(bm.bmWidth > 0 && bm.bmHeight > 0, "OBM_%s size %ldx%ld", x3_obm[i].n, bm.bmWidth, bm.bmHeight);
        DeleteObject(b);
    }
}

TEST(resources_x3_obm_old_ids_note) {
    REQUIRE(U(LoadBitmapW));
    for (int i = 0; i < NOBMOLD; i++) {
        SetLastError(0);
        HBITMAP b = U(LoadBitmapW)(NULL, MAKEINTRESOURCEW(x3_obm_old[i].id));
        t_note("OBM_%s -> %p err %lu", x3_obm_old[i].n, b, GetLastError());
        if (b) DeleteObject(b);
    }
}

static const WORD x3_ocr[] = { 32512, 32513, 32514, 32515, 32516, 32642, 32643, 32644, 32645, 32646, 32648, 32649, 32650 };
/* NORMAL IBEAM WAIT CROSS UP SIZENWSE SIZENESW SIZEWE SIZENS SIZEALL NO HAND APPSTARTING */

TEST(resources_x3_ocr_cursors_load_shared) {
    REQUIRE(U(LoadImageW)); REQUIRE(U(GetIconInfo));
    for (unsigned i = 0; i < sizeof x3_ocr / sizeof x3_ocr[0]; i++) {
        HANDLE a = U(LoadImageW)(NULL, MAKEINTRESOURCEW(x3_ocr[i]), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
        HANDLE b = U(LoadImageW)(NULL, MAKEINTRESOURCEW(x3_ocr[i]), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
        CHECKF(a != NULL, "OCR %u err %lu", x3_ocr[i], GetLastError());
        if (!a) continue;
        CHECKF(a == b, "LR_SHARED not shared for OCR %u (%p vs %p)", x3_ocr[i], a, b);
        ICONINFO ii; memset(&ii, 0, sizeof ii);
        CHECKF(U(GetIconInfo)((HICON)a, &ii), "GetIconInfo OCR %u", x3_ocr[i]);
        CHECKF(!ii.fIcon, "OCR %u reports fIcon", x3_ocr[i]);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
    }
}

TEST(resources_x3_ocr_obsolete_ids_note) {
    REQUIRE(U(LoadImageW));
    static const struct { const char *n; WORD id; } o[] = { {"SIZE", 32640}, {"ICON", 32641}, {"ICOCUR", 32647} };
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        HANDLE h = U(LoadImageW)(NULL, MAKEINTRESOURCEW(o[i].id), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
        t_note("OCR_%s -> %p err %lu", o[i].n, h, GetLastError());
    }
}

TEST(resources_x3_oic_icons_load_shared) {
    REQUIRE(U(LoadImageW)); REQUIRE(U(GetIconInfo));
    for (WORD id = 32512; id <= 32518; id++) {
        HANDLE a = U(LoadImageW)(NULL, MAKEINTRESOURCEW(id), IMAGE_ICON, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
        HANDLE b = U(LoadImageW)(NULL, MAKEINTRESOURCEW(id), IMAGE_ICON, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
        CHECKF(a != NULL, "OIC %u err %lu", id, GetLastError());
        if (!a) continue;
        CHECKF(a == b, "LR_SHARED not shared for OIC %u", id);
        ICONINFO ii; memset(&ii, 0, sizeof ii);
        CHECKF(U(GetIconInfo)((HICON)a, &ii) && ii.fIcon, "GetIconInfo OIC %u", id);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
    }
}

TEST(resources_x3_oic_aliases) {
    EXPECT_EQ(OIC_SAMPLE, 32512); EXPECT_EQ(OIC_HAND, 32513); EXPECT_EQ(OIC_QUES, 32514);
    EXPECT_EQ(OIC_BANG, 32515); EXPECT_EQ(OIC_NOTE, 32516); EXPECT_EQ(OIC_WINLOGO, 32517);
    EXPECT_EQ(OIC_ERROR, OIC_HAND); EXPECT_EQ(OIC_WARNING, OIC_BANG); EXPECT_EQ(OIC_INFORMATION, OIC_NOTE);
    EXPECT_EQ(IDI_ERROR, IDI_HAND); EXPECT_EQ(IDI_WARNING, IDI_EXCLAMATION); EXPECT_EQ(IDI_INFORMATION, IDI_ASTERISK);
}

TEST(resources_x3_loadimage_wrong_type_for_oem_id) {
    REQUIRE(U(LoadImageW));
    SetLastError(0);
    HANDLE h = U(LoadImageW)(NULL, MAKEINTRESOURCEW(32512), 9 /* invalid image type */, 0, 0, LR_SHARED);
    EXPECT_EQ(h, NULL);
    t_note("LoadImage(type 9) err %lu", GetLastError());
    SetLastError(0);
    h = U(LoadImageW)(NULL, MAKEINTRESOURCEW(0xFFFE), IMAGE_ICON, 0, 0, LR_SHARED);
    EXPECT_EQ(h, NULL);
    t_note("LoadImage(unknown OEM icon id) err %lu", GetLastError());
}
