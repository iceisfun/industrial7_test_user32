#include "ri.h"

/* Icons and cursors from resources: fixture module and in-memory data. */

static HICON ri_li(HMODULE m, int id, UINT type, int cx, int cy, UINT fl)
{
    return (HICON)U(LoadImageW)(m, MAKEINTRESOURCEW(id), type, cx, cy, fl);
}

TEST(resources_icons_res_findresource_sizes)
{
    static const struct { LPCSTR type; int id; DWORD size; } t[] = {
        {RT_ICON, RI_RT_ICON_BASE + 0, 40 + 16 * 16 * 4 + 4 * 16},
        {RT_ICON, RI_RT_ICON_BASE + 1, 40 + 32 * 32 * 4 + 4 * 32},
        {RT_ICON, RI_RT_ICON_BASE + 2, 40 + 48 * 48 * 4 + 8 * 48},
        {RT_GROUP_ICON, RI_GRP_ICON, 6 + 14 * 3},
        {RT_GROUP_ICON, RI_GRP_ICON8, 6 + 14},
        {RT_GROUP_CURSOR, RI_GRP_CUR, 6 + 14},
    };
    int i;
    REQUIRE_FIX(m);
    for (i = 0; i < 6; i++) {
        HRSRC r = FindResourceA(m, MAKEINTRESOURCEA(t[i].id), t[i].type);
        CHECKF(r != NULL, "FindResource %d", i);
        if (!r) continue;
        CHECKF(SizeofResource(m, r) == t[i].size, "size %d: %lu vs %lu", i, SizeofResource(m, r), t[i].size);
        HGLOBAL g = LoadResource(m, r);
        CHECK(g != NULL);
        CHECK(LockResource(g) != NULL);
    }
}

TEST(resources_icons_res_findresource_missing)
{
    REQUIRE_FIX(m);
    SetLastError(0);
    CHECK(FindResourceA(m, MAKEINTRESOURCEA(9999), RT_ICON) == NULL);
    EXPECT_ERR(0, ERROR_RESOURCE_NAME_NOT_FOUND);
    SetLastError(0);
    CHECK(FindResourceA(m, MAKEINTRESOURCEA(RI_GRP_ICON), RT_MENU) == NULL);
    EXPECT_ERR(0, ERROR_RESOURCE_TYPE_NOT_FOUND);
}

TEST(resources_icons_res_cursor_resource_data)
{
    REQUIRE_FIX(m);
    HRSRC r = FindResourceA(m, MAKEINTRESOURCEA(RI_RT_CUR_BASE), RT_CURSOR);
    CHECK(r != NULL);
    if (r) {
        WORD *d = LockResource(LoadResource(m, r));
        CHECK(d != NULL);
        if (d) { RI_EQ(d[0], 5); RI_EQ(d[1], 7); RI_EQ(*(DWORD *)(d + 2), 40); }
    }
}

TEST(resources_icons_res_loadimage_group_sizes)
{
    static const int sz[] = {16, 32, 48};
    int i;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    for (i = 0; i < 3; i++) {
        int w = 0, h = 0, bpp = 0; BOOL cur = TRUE;
        HICON ic = ri_li(m, RI_GRP_ICON, IMAGE_ICON, sz[i], sz[i], 0);
        CHECKF(ic != NULL, "size %d", sz[i]);
        if (!ic) continue;
        CHECK(ri_iconsize(ic, &w, &h, &bpp, &cur));
        RI_EQ(w, sz[i]); RI_EQ(h, sz[i]);
        RI_EQ(cur, FALSE);
        RI_EQ(bpp, 32);
        CHECK(U(DestroyIcon)(ic));
    }
}

TEST(resources_icons_res_loadicon_group)
{
    HICON a, b;
    REQUIRE(U(LoadIconW));
    REQUIRE(U(DestroyIcon));
    REQUIRE_FIX(m);
    a = U(LoadIconW)(m, MAKEINTRESOURCEW(RI_GRP_ICON));
    CHECK(a != NULL);
    b = U(LoadIconW)(m, MAKEINTRESOURCEW(RI_GRP_ICON));
    CHECK(b != NULL);
    /* LoadIcon returns a shared handle: repeated loads give the same handle */
    RI_EQ(a, b);
    {
        int w = 0, h = 0;
        if (a && ri_iconsize(a, &w, &h, NULL, NULL)) {
            RI_EQ(w, ri_defsize(0,0));
            RI_EQ(h, ri_defsize(0,1));
        }
    }
}

TEST(resources_icons_res_loadimage_actual_size)
{
    /* cx=cy=0 without LR_DEFAULTSIZE: use the size of the resource itself */
    int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    ic = ri_li(m, RI_GRP_ICON24, IMAGE_ICON, 0, 0, 0);
    CHECK(ic != NULL);
    if (ic) { CHECK(ri_iconsize(ic, &w, &h, NULL, NULL)); RI_EQ(w, 24); RI_EQ(h, 24); U(DestroyIcon)(ic); }
}

TEST(resources_icons_res_loadimage_defaultsize)
{
    int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    ic = ri_li(m, RI_GRP_ICON24, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(ri_iconsize(ic, &w, &h, NULL, NULL));
        RI_EQ(w, ri_defsize(0,0));
        RI_EQ(h, ri_defsize(0,1));
        U(DestroyIcon)(ic);
    }
    /* explicit cx/cy override LR_DEFAULTSIZE */
    ic = ri_li(m, RI_GRP_ICON24, IMAGE_ICON, 40, 40, LR_DEFAULTSIZE);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(ri_iconsize(ic, &w, &h, NULL, NULL));
        t_note("cx=40 + LR_DEFAULTSIZE -> %dx%d", w, h);
        U(DestroyIcon)(ic);
    }
}

TEST(resources_icons_res_loadimage_scaled)
{
    static const int sz[] = {8, 20, 64, 100};
    int i;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    for (i = 0; i < 4; i++) {
        int w = 0, h = 0;
        HICON ic = ri_li(m, RI_GRP_ICON24, IMAGE_ICON, sz[i], sz[i], 0);
        CHECKF(ic != NULL, "sz %d", sz[i]);
        if (!ic) continue;
        CHECK(ri_iconsize(ic, &w, &h, NULL, NULL));
        RI_EQ(w, sz[i]); RI_EQ(h, sz[i]);
        U(DestroyIcon)(ic);
    }
}

TEST(resources_icons_res_loadimage_nonsquare)
{
    int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    ic = ri_li(m, RI_GRP_ICON24, IMAGE_ICON, 20, 40, 0);
    CHECK(ic != NULL);
    if (ic) { CHECK(ri_iconsize(ic, &w, &h, NULL, NULL)); RI_EQ(w, 20); RI_EQ(h, 40); U(DestroyIcon)(ic); }
}

TEST(resources_icons_res_loadimage_depths)
{
    static const int ids[] = {RI_GRP_ICON8, RI_GRP_ICON4, RI_GRP_ICON1, RI_GRP_ICON24};
    static const int exp[] = {32, 32, 32, 24};
    int i;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    for (i = 0; i < 4; i++) {
        int w = 0, h = 0, bpp = 0;
        HICON ic = ri_li(m, ids[i], IMAGE_ICON, 0, 0, 0);
        CHECKF(ic != NULL, "group %d", ids[i]);
        if (!ic) continue;
        CHECK(ri_iconsize(ic, &w, &h, &bpp, NULL));
        RI_EQ(w, exp[i]); RI_EQ(h, exp[i]);
        t_note("group %d colour bitmap bpp=%d", ids[i], bpp);
        U(DestroyIcon)(ic);
    }
}

TEST(resources_icons_res_iconinfo_icon)
{
    ICONINFO ii;
    BITMAP bm;
    HICON ic;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    ic = ri_li(m, RI_GRP_ICON, IMAGE_ICON, 32, 32, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    memset(&ii, 0xAA, sizeof ii);
    CHECK(U(GetIconInfo)(ic, &ii));
    RI_EQ(ii.fIcon, TRUE);
    RI_EQ(ii.xHotspot, 16);
    RI_EQ(ii.yHotspot, 16);
    CHECK(ii.hbmMask != NULL);
    CHECK(ii.hbmColor != NULL);
    CHECK(ri_bmp(ii.hbmMask, &bm));
    RI_EQ(bm.bmWidth, 32); RI_EQ(bm.bmHeight, 32); RI_EQ(bm.bmBitsPixel, 1); RI_EQ(bm.bmPlanes, 1);
    CHECK(ri_bmp(ii.hbmColor, &bm));
    RI_EQ(bm.bmWidth, 32); RI_EQ(bm.bmHeight, 32);
    RI_EQ(GetObjectType(ii.hbmMask), OBJ_BITMAP);
    RI_EQ(GetObjectType(ii.hbmColor), OBJ_BITMAP);
    /* every call returns fresh bitmap copies */
    {
        ICONINFO i2;
        CHECK(U(GetIconInfo)(ic, &i2));
        CHECK(i2.hbmMask != ii.hbmMask);
        CHECK(i2.hbmColor != ii.hbmColor);
        DeleteObject(i2.hbmMask); DeleteObject(i2.hbmColor);
    }
    DeleteObject(ii.hbmMask); DeleteObject(ii.hbmColor);
    U(DestroyIcon)(ic);
}

TEST(resources_icons_res_iconinfo_hotspot_icon_sizes)
{
    static const int sz[] = {16, 32, 48};
    int i;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    for (i = 0; i < 3; i++) {
        ICONINFO ii;
        HICON ic = ri_li(m, RI_GRP_ICON, IMAGE_ICON, sz[i], sz[i], 0);
        if (!ic) { CHECK(0); continue; }
        CHECK(U(GetIconInfo)(ic, &ii));
        RI_EQ(ii.xHotspot, sz[i] / 2);
        RI_EQ(ii.yHotspot, sz[i] / 2);
        DeleteObject(ii.hbmMask); DeleteObject(ii.hbmColor);
        U(DestroyIcon)(ic);
    }
}

TEST(resources_icons_res_cursor_loadimage)
{
    ICONINFO ii; BITMAP bm;
    HICON c;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    c = ri_li(m, RI_GRP_CUR, IMAGE_CURSOR, 0, 0, 0);
    CHECK(c != NULL);
    if (!c) return;
    CHECK(U(GetIconInfo)(c, &ii));
    RI_EQ(ii.fIcon, FALSE);
    RI_EQ(ii.xHotspot, 5);
    RI_EQ(ii.yHotspot, 7);
    CHECK(ii.hbmMask != NULL);
    CHECK(ri_bmp(ii.hbmMask, &bm));
    RI_EQ(bm.bmWidth, 32);
    t_note("mono cursor: hbmColor=%p mask height=%d", ii.hbmColor, bm.bmHeight);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    CHECK(U(DestroyCursor)(c));
}

TEST(resources_icons_res_cursor_loadcursor)
{
    HCURSOR c;
    REQUIRE(U(LoadCursorW));
    REQUIRE_FIX(m);
    c = U(LoadCursorW)(m, MAKEINTRESOURCEW(RI_GRP_CUR));
    CHECK(c != NULL);
    RI_EQ(U(LoadCursorW)(m, MAKEINTRESOURCEW(RI_GRP_CUR)), c);
}

TEST(resources_icons_res_cursor_scaled_hotspot)
{
    ICONINFO ii;
    HICON c;
    REQUIRE(U(LoadImageW));
    REQUIRE(U(GetIconInfo));
    REQUIRE_FIX(m);
    c = ri_li(m, RI_GRP_CUR, IMAGE_CURSOR, 64, 64, 0);
    CHECK(c != NULL);
    if (!c) return;
    CHECK(U(GetIconInfo)(c, &ii));
    t_note("cursor scaled 32->64: hotspot %lu,%lu", ii.xHotspot, ii.yHotspot);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    U(DestroyCursor)(c);
}

TEST(resources_icons_res_loadimage_wrong_kind)
{
    HICON a, b;
    REQUIRE(U(LoadImageW));
    REQUIRE_FIX(m);
    SetLastError(0);
    a = ri_li(m, RI_GRP_CUR, IMAGE_ICON, 0, 0, 0);
    t_note("IMAGE_ICON on a cursor group: %p err=%lu", a, GetLastError());
    SetLastError(0);
    b = ri_li(m, RI_GRP_ICON, IMAGE_CURSOR, 0, 0, 0);
    t_note("IMAGE_CURSOR on an icon group: %p err=%lu", b, GetLastError());
    if (a) U(DestroyIcon)(a);
    if (b) U(DestroyIcon)(b);
}

TEST(resources_icons_res_loadimage_missing_name)
{
    REQUIRE(U(LoadImageW));
    REQUIRE_FIX(m);
    SetLastError(0);
    CHECK(ri_li(m, 12345, IMAGE_ICON, 0, 0, 0) == NULL);
    t_note("missing icon group err=%lu", GetLastError());
    SetLastError(0);
    CHECK(ri_li(m, 12345, IMAGE_CURSOR, 0, 0, 0) == NULL);
    SetLastError(0);
    CHECK(ri_li(m, 12345, IMAGE_BITMAP, 0, 0, 0) == NULL);
    SetLastError(0);
    CHECK(U(LoadIconW)(m, MAKEINTRESOURCEW(12345)) == NULL);
    CHECK(U(LoadCursorW)(m, MAKEINTRESOURCEW(12345)) == NULL);
    CHECK(U(LoadBitmapW)(m, MAKEINTRESOURCEW(12345)) == NULL);
}

TEST(resources_icons_res_loadbitmap)
{
    HBITMAP b; BITMAP bm;
    REQUIRE(U(LoadBitmapW));
    REQUIRE_FIX(m);
    b = U(LoadBitmapW)(m, MAKEINTRESOURCEW(RI_BMP_ID));
    CHECK(b != NULL);
    if (!b) return;
    CHECK(ri_bmp(b, &bm));
    RI_EQ(bm.bmWidth, 8); RI_EQ(bm.bmHeight, 4);
    RI_EQ(bm.bmType, 0);
    RI_EQ(GetObjectType(b), OBJ_BITMAP);
    /* LoadBitmap yields a DDB: GetObject(DIBSECTION size) returns only a BITMAP */
    {
        DIBSECTION ds;
        RI_EQ(GetObjectW(b, sizeof ds, &ds), sizeof(BITMAP));
    }
    CHECK(DeleteObject(b));
}

TEST(resources_icons_res_loadbitmap_a_w_parity)
{
    HBITMAP a, w; BITMAP ba, bw;
    REQUIRE(U(LoadBitmapA));
    REQUIRE(U(LoadBitmapW));
    REQUIRE_FIX(m);
    a = U(LoadBitmapA)(m, MAKEINTRESOURCEA(RI_BMP_ID));
    w = U(LoadBitmapW)(m, MAKEINTRESOURCEW(RI_BMP_ID));
    CHECK(a && w);
    if (a && w) {
        ri_bmp(a, &ba); ri_bmp(w, &bw);
        RI_EQ(ba.bmWidth, bw.bmWidth); RI_EQ(ba.bmHeight, bw.bmHeight); RI_EQ(ba.bmBitsPixel, bw.bmBitsPixel);
    }
    if (a) DeleteObject(a);
    if (w) DeleteObject(w);
}

TEST(resources_icons_res_loadimage_bitmap_dibsection)
{
    HBITMAP b; DIBSECTION ds;
    REQUIRE(U(LoadImageW));
    REQUIRE_FIX(m);
    b = U(LoadImageW)(m, MAKEINTRESOURCEW(RI_BMP_ID), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    CHECK(b != NULL);
    if (!b) return;
    memset(&ds, 0, sizeof ds);
    RI_EQ(GetObjectW(b, sizeof ds, &ds), sizeof ds);
    RI_EQ(ds.dsBm.bmWidth, 8); RI_EQ(ds.dsBm.bmHeight, 4);
    CHECK(ds.dsBm.bmBits != NULL);
    RI_EQ(ds.dsBmih.biBitCount, 24);
    RI_EQ(GetObjectType(b), OBJ_BITMAP);
    DeleteObject(b);
}

TEST(resources_icons_res_loadimage_bitmap_monochrome)
{
    HBITMAP b; BITMAP bm;
    REQUIRE(U(LoadImageW));
    REQUIRE_FIX(m);
    b = U(LoadImageW)(m, MAKEINTRESOURCEW(RI_BMP_ID), IMAGE_BITMAP, 0, 0, LR_MONOCHROME);
    CHECK(b != NULL);
    if (!b) return;
    CHECK(ri_bmp(b, &bm));
    RI_EQ(bm.bmBitsPixel, 1); RI_EQ(bm.bmPlanes, 1);
    RI_EQ(bm.bmWidth, 8); RI_EQ(bm.bmHeight, 4);
    DeleteObject(b);
}

TEST(resources_icons_res_loadimage_bitmap_resize)
{
    HBITMAP b; BITMAP bm;
    REQUIRE(U(LoadImageW));
    REQUIRE_FIX(m);
    b = U(LoadImageW)(m, MAKEINTRESOURCEW(RI_BMP_ID), IMAGE_BITMAP, 16, 12, 0);
    CHECK(b != NULL);
    if (!b) return;
    CHECK(ri_bmp(b, &bm));
    RI_EQ(bm.bmWidth, 16); RI_EQ(bm.bmHeight, 12);
    DeleteObject(b);
}

/* ---- in-memory: CreateIconFromResourceEx ---------------------------- */

TEST(resources_icons_cifr_basic_sizes)
{
    static const int sz[] = {1, 8, 16, 31, 32, 33, 48, 64};
    int i;
    REQUIRE(U(CreateIconFromResourceEx));
    REQUIRE(U(GetIconInfo));
    for (i = 0; i < 8; i++) {
        DWORD n; BYTE *d = ri_dib(sz[i], sz[i], 32, 0xFFFF0000, 0, &n);
        int w = 0, h = 0; BOOL cur = TRUE;
        HICON ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, 0);
        CHECKF(ic != NULL, "size %d", sz[i]);
        if (ic) {
            CHECK(ri_iconsize(ic, &w, &h, NULL, &cur));
            RI_EQ(w, sz[i]); RI_EQ(h, sz[i]); RI_EQ(cur, FALSE);
            CHECK(U(DestroyIcon)(ic));
        }
        free(d);
    }
}

TEST(resources_icons_cifr_depths)
{
    static const int bpp[] = {1, 4, 8, 24, 32};
    int i;
    REQUIRE(U(CreateIconFromResourceEx));
    for (i = 0; i < 5; i++) {
        DWORD n; BYTE *d = ri_dib(32, 32, bpp[i], 0xFF204060, 0, &n);
        int w = 0, h = 0, cb = 0;
        HICON ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, 0);
        CHECKF(ic != NULL, "bpp %d", bpp[i]);
        if (ic) {
            CHECK(ri_iconsize(ic, &w, &h, &cb, NULL));
            RI_EQ(w, 32); RI_EQ(h, 32);
            t_note("bpp %d icon -> colour bitmap bpp %d", bpp[i], cb);
            U(DestroyIcon)(ic);
        }
        free(d);
    }
}

TEST(resources_icons_cifr_scaled)
{
    static const int want[] = {16, 24, 48, 100};
    int i;
    REQUIRE(U(CreateIconFromResourceEx));
    for (i = 0; i < 4; i++) {
        DWORD n; BYTE *d = ri_dib(32, 32, 32, 0xFF00FF00, 0, &n);
        int w = 0, h = 0;
        HICON ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, want[i], want[i], 0);
        CHECKF(ic != NULL, "want %d", want[i]);
        if (ic) { ri_iconsize(ic, &w, &h, NULL, NULL); RI_EQ(w, want[i]); RI_EQ(h, want[i]); U(DestroyIcon)(ic); }
        free(d);
    }
}

TEST(resources_icons_cifr_defaultsize_icon)
{
    DWORD n; BYTE *d = ri_dib(20, 20, 32, 0xFF0000FF, 0, &n);
    int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(CreateIconFromResourceEx));
    ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, LR_DEFAULTSIZE);
    CHECK(ic != NULL);
    if (ic) {
        ri_iconsize(ic, &w, &h, NULL, NULL);
        RI_EQ(w, ri_defsize(0,0));
        RI_EQ(h, ri_defsize(0,1));
        U(DestroyIcon)(ic);
    }
    free(d);
}

TEST(resources_icons_cifr_defaultsize_cursor)
{
    DWORD n; BYTE *d = ri_curdata(20, 20, 1, 0xFFFFFFFF, 3, 4, &n);
    int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(CreateIconFromResourceEx));
    ic = U(CreateIconFromResourceEx)(d, n, FALSE, 0x30000, 0, 0, LR_DEFAULTSIZE);
    CHECK(ic != NULL);
    if (ic) {
        ri_iconsize(ic, &w, &h, NULL, NULL);
        RI_EQ(w, ri_defsize(1,0));
        RI_EQ(h, ri_defsize(1,1));
        U(DestroyCursor)(ic);
    }
    free(d);
}

TEST(resources_icons_cifr_cursor_hotspot)
{
    DWORD n; BYTE *d = ri_curdata(32, 32, 1, 0xFFFFFFFF, 9, 11, &n);
    ICONINFO ii;
    HICON c;
    REQUIRE(U(CreateIconFromResourceEx));
    REQUIRE(U(GetIconInfo));
    c = U(CreateIconFromResourceEx)(d, n, FALSE, 0x30000, 0, 0, 0);
    CHECK(c != NULL);
    if (c) {
        CHECK(U(GetIconInfo)(c, &ii));
        RI_EQ(ii.fIcon, FALSE); RI_EQ(ii.xHotspot, 9); RI_EQ(ii.yHotspot, 11);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        U(DestroyCursor)(c);
    }
    free(d);
}

TEST(resources_icons_cifr_cursor_color)
{
    DWORD n; BYTE *d = ri_curdata(32, 32, 24, 0xFF336699, 1, 2, &n);
    HICON c;
    int w = 0, h = 0; BOOL cur = FALSE;
    REQUIRE(U(CreateIconFromResourceEx));
    c = U(CreateIconFromResourceEx)(d, n, FALSE, 0x30000, 0, 0, 0);
    CHECK(c != NULL);
    if (c) {
        CHECK(ri_iconsize(c, &w, &h, NULL, &cur));
        RI_EQ(cur, TRUE); RI_EQ(w, 32); RI_EQ(h, 32);
        U(DestroyCursor)(c);
    }
    free(d);
}

TEST(resources_icons_cifr_invalid_args)
{
    DWORD n; BYTE *d = ri_dib(32, 32, 32, 0xFF000000, 0, &n);
    REQUIRE(U(CreateIconFromResourceEx));
    RI_EQ(U(CreateIconFromResourceEx)(NULL, n, TRUE, 0x30000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(NULL, 0, TRUE, 0x30000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, 0, TRUE, 0x30000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, 0x27, TRUE, 0x30000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, 1, TRUE, 0x30000, 0, 0, 0), NULL);
    /* version must be within 0x20000..0x30000 */
    RI_EQ(U(CreateIconFromResourceEx)(d, n, TRUE, 0, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, n, TRUE, 0x10000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, n, TRUE, 0x1FFFF, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, n, TRUE, 0x30001, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, n, TRUE, 0x40000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, n, TRUE, 0xFFFFFFFF, 0, 0, 0), NULL);
    free(d);
}

TEST(resources_icons_cifr_valid_versions)
{
    DWORD n; BYTE *d = ri_dib(32, 32, 32, 0xFF000000, 0, &n);
    static const DWORD v[] = {0x20000, 0x20001, 0x2FFFF, 0x30000};
    int i;
    REQUIRE(U(CreateIconFromResourceEx));
    for (i = 0; i < 4; i++) {
        HICON ic = U(CreateIconFromResourceEx)(d, n, TRUE, v[i], 0, 0, 0);
        CHECKF(ic != NULL, "ver 0x%lx", v[i]);
        if (ic) U(DestroyIcon)(ic);
    }
    free(d);
}

TEST(resources_icons_cifr_bad_header)
{
    DWORD n; BYTE *d = ri_dib(32, 32, 32, 0xFF000000, 0, &n);
    BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *)d;
    HICON ic;
    REQUIRE(U(CreateIconFromResourceEx));
    bi->biSize = 0;
    ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, 0);
    t_note("biSize=0: %p", ic); if (ic) U(DestroyIcon)(ic);
    bi->biSize = sizeof *bi; bi->biBitCount = 7;
    ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, 0);
    t_note("biBitCount=7: %p", ic); if (ic) U(DestroyIcon)(ic);
    bi->biBitCount = 32; bi->biCompression = 99;
    ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, 0);
    t_note("biCompression=99: %p", ic); if (ic) U(DestroyIcon)(ic);
    bi->biCompression = 0; bi->biWidth = 0;
    ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, 0);
    t_note("biWidth=0: %p", ic); if (ic) U(DestroyIcon)(ic);
    free(d);
}

TEST(resources_icons_cifr_icon_flag_hotspot)
{
    DWORD n; BYTE *d = ri_dib(32, 32, 32, 0xFF808080, 0, &n);
    ICONINFO ii;
    HICON ic;
    REQUIRE(U(CreateIconFromResourceEx));
    ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, 0);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(GetIconInfo)(ic, &ii));
        RI_EQ(ii.fIcon, TRUE); RI_EQ(ii.xHotspot, 16); RI_EQ(ii.yHotspot, 16);
        DeleteObject(ii.hbmMask); if (ii.hbmColor) DeleteObject(ii.hbmColor);
        U(DestroyIcon)(ic);
    }
    free(d);
}

TEST(resources_icons_cifr_legacy_wrapper)
{
    /* CreateIconFromResource(presbits,size,fIcon,ver) == Ex with zero size / flags */
    DWORD n; BYTE *d = ri_dib(24, 24, 24, 0xFF808080, 0, &n);
    int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(CreateIconFromResource));
    ic = U(CreateIconFromResource)(d, n, TRUE, 0x30000);
    CHECK(ic != NULL);
    /* the wrapper passes LR_SHARED|LR_DEFAULTSIZE */
    if (ic) { ri_iconsize(ic, &w, &h, NULL, NULL); RI_EQ(w, ri_defsize(0, 0)); RI_EQ(h, ri_defsize(0, 1)); }
    RI_EQ(U(CreateIconFromResource)(NULL, n, TRUE, 0x30000), NULL);
    RI_EQ(U(CreateIconFromResource)(d, 0x10, TRUE, 0x30000), NULL);
    RI_EQ(U(CreateIconFromResource)(d, n, TRUE, 0x10000), NULL);
    free(d);
}

/* ---- directory lookup ------------------------------------------------- */

static BYTE *ri_grpdir(int count, const int *sizes, const int *bpps, const int *ids, DWORD *outsz, int cursor)
{
    int i;
    BYTE *g = calloc(1, 6 + 14 * count), *p;
    *(WORD *)(g + 2) = cursor ? 2 : 1; *(WORD *)(g + 4) = count;
    p = g + 6;
    for (i = 0; i < count; i++) {
        if (cursor) { *(WORD *)p = sizes[i]; *(WORD *)(p + 2) = sizes[i] * 2; }
        else { p[0] = sizes[i]; p[1] = sizes[i]; }
        *(WORD *)(p + 4) = 1; *(WORD *)(p + 6) = bpps[i];
        *(DWORD *)(p + 8) = 100; *(WORD *)(p + 12) = ids[i];
        p += 14;
    }
    *outsz = 6 + 14 * count;
    return g;
}

TEST(resources_icons_lookup_exact_sizes)
{
    int sz[] = {16, 32, 48}, bpp[] = {32, 32, 32}, ids[] = {10, 11, 12};
    DWORD n; BYTE *g = ri_grpdir(3, sz, bpp, ids, &n, 0);
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    RI_EQ(U(LookupIconIdFromDirectoryEx)(g, TRUE, 16, 16, 0), 10);
    RI_EQ(U(LookupIconIdFromDirectoryEx)(g, TRUE, 32, 32, 0), 11);
    RI_EQ(U(LookupIconIdFromDirectoryEx)(g, TRUE, 48, 48, 0), 12);
    free(g);
}

TEST(resources_icons_lookup_closest_and_extremes)
{
    int sz[] = {16, 32, 48}, bpp[] = {32, 32, 32}, ids[] = {10, 11, 12};
    DWORD n; BYTE *g = ri_grpdir(3, sz, bpp, ids, &n, 0);
    int r;
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    r = U(LookupIconIdFromDirectoryEx)(g, TRUE, 1, 1, 0);
    CHECK(r == 10 || r == 11 || r == 12);
    RI_EQ(r, 10);
    r = U(LookupIconIdFromDirectoryEx)(g, TRUE, 256, 256, 0);
    RI_EQ(r, 12);
    r = U(LookupIconIdFromDirectoryEx)(g, TRUE, 17, 17, 0);
    t_note("lookup 17 -> id %d", r);
    r = U(LookupIconIdFromDirectoryEx)(g, TRUE, 40, 40, 0);
    t_note("lookup 40 -> id %d", r);
    free(g);
}

TEST(resources_icons_lookup_default_size)
{
    int sz[] = {16, 32, 48}, bpp[] = {32, 32, 32}, ids[] = {10, 11, 12};
    DWORD n; BYTE *g = ri_grpdir(3, sz, bpp, ids, &n, 0);
    int r, want = 0, cx = ri_defsize(0, 0);
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    r = U(LookupIconIdFromDirectoryEx)(g, TRUE, 0, 0, LR_DEFAULTSIZE);
    want = cx == 16 ? 10 : cx == 32 ? 11 : cx == 48 ? 12 : 0;
    if (want) RI_EQ(r, want); else t_note("SM_CXICON=%d lookup -> %d", cx, r);
    r = U(LookupIconIdFromDirectoryEx)(g, TRUE, 0, 0, 0);
    t_note("lookup 0x0 no flags -> %d", r);
    if (U(LookupIconIdFromDirectory)) {
        /* Win10 22H2 oracle: LookupIconIdFromDirectory behaves like LookupIconIdFromDirectoryEx
         * with cx=cy=0 and no flags (id 10, the 16x16 entry), not like LR_DEFAULTSIZE. */
        int plain = U(LookupIconIdFromDirectoryEx)(g, TRUE, 0, 0, 0);
        r = U(LookupIconIdFromDirectory)(g, TRUE);
        RI_EQ(r, plain);
    }
    free(g);
}

TEST(resources_icons_lookup_cursor_dir)
{
    int sz[] = {32, 48}, bpp[] = {1, 1}, ids[] = {30, 31};
    DWORD n; BYTE *g = ri_grpdir(2, sz, bpp, ids, &n, 1);
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    RI_EQ(U(LookupIconIdFromDirectoryEx)(g, FALSE, 32, 32, 0), 30);
    RI_EQ(U(LookupIconIdFromDirectoryEx)(g, FALSE, 48, 48, 0), 31);
    free(g);
}

TEST(resources_icons_lookup_single_entry)
{
    int sz[] = {24}, bpp[] = {24}, ids[] = {77};
    DWORD n; BYTE *g = ri_grpdir(1, sz, bpp, ids, &n, 0);
    int cx;
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    for (cx = 1; cx < 300; cx += 37) RI_EQ(U(LookupIconIdFromDirectoryEx)(g, TRUE, cx, cx, 0), 77);
    RI_EQ(U(LookupIconIdFromDirectoryEx)(g, TRUE, 0, 0, LR_DEFAULTSIZE), 77);
    free(g);
}

TEST(resources_icons_lookup_depth_preference)
{
    /* same size, different depths: highest depth not above the display's wins */
    int sz[] = {32, 32, 32}, bpp[] = {1, 4, 8}, ids[] = {1, 4, 8};
    DWORD n; BYTE *g = ri_grpdir(3, sz, bpp, ids, &n, 0);
    int r;
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    r = U(LookupIconIdFromDirectoryEx)(g, TRUE, 32, 32, 0);
    RI_EQ(r, 8);
    r = U(LookupIconIdFromDirectoryEx)(g, TRUE, 32, 32, LR_MONOCHROME);
    RI_EQ(r, 1);
    free(g);
}

TEST(resources_icons_lookup_empty_dir)
{
    BYTE g[6 + 14] = {0};
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    *(WORD *)(g + 2) = 1; *(WORD *)(g + 4) = 0;
    RI_EQ(U(LookupIconIdFromDirectoryEx)(g, TRUE, 32, 32, 0), 0);
    *(WORD *)(g + 2) = 7; *(WORD *)(g + 4) = 1;
    t_note("bad idType: %d", U(LookupIconIdFromDirectoryEx)(g, TRUE, 32, 32, 0));
}

TEST(resources_icons_lookup_real_group)
{
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    REQUIRE_FIX(m);
    HRSRC r = FindResourceA(m, MAKEINTRESOURCEA(RI_GRP_ICON), RT_GROUP_ICON);
    BYTE *d = r ? LockResource(LoadResource(m, r)) : NULL;
    CHECK(d != NULL);
    if (d) {
        RI_EQ(U(LookupIconIdFromDirectoryEx)(d, TRUE, 16, 16, 0), RI_RT_ICON_BASE);
        RI_EQ(U(LookupIconIdFromDirectoryEx)(d, TRUE, 32, 32, 0), RI_RT_ICON_BASE + 1);
        RI_EQ(U(LookupIconIdFromDirectoryEx)(d, TRUE, 48, 48, 0), RI_RT_ICON_BASE + 2);
    }
    r = FindResourceA(m, MAKEINTRESOURCEA(RI_GRP_CUR), RT_GROUP_CURSOR);
    d = r ? LockResource(LoadResource(m, r)) : NULL;
    CHECK(d != NULL);
    if (d) RI_EQ(U(LookupIconIdFromDirectoryEx)(d, FALSE, 32, 32, 0), RI_RT_CUR_BASE);
}
