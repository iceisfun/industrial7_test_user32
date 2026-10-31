#include "ri.h"

/* Icon / cursor objects created from bitmaps: CreateIcon*, CopyIcon, CopyImage, Destroy*, GetIconInfo*, DrawIconEx */

static HBITMAP ri_mkmask(int w, int h, int bit)
{
    BYTE bits[16 * 256];
    int stride = ((w + 15) / 16) * 2;
    memset(bits, bit ? 0xFF : 0x00, stride * h);
    return CreateBitmap(w, h, 1, 1, bits);
}

static HBITMAP ri_mkcolor24(int w, int h, DWORD rgb, void **pbits)
{
    BITMAPINFO bi; void *bits = NULL; HBITMAP b; int x, y;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    b = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (b) {
        int stride = ((w * 3 + 3) / 4) * 4;
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            BYTE *p = (BYTE *)bits + y * stride + x * 3;
            p[0] = rgb & 255; p[1] = (rgb >> 8) & 255; p[2] = (rgb >> 16) & 255;
        }
    }
    if (pbits) *pbits = bits;
    return b;
}

/* rgb here is 0xRRGGBB; alpha is the premultiplied 32bpp value 0xAARRGGBB */
static HBITMAP ri_mkcolor32(int w, int h, DWORD argb)
{
    BITMAPINFO bi; void *bits = NULL; HBITMAP b; int i;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    b = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (b) for (i = 0; i < w * h; i++) ((DWORD *)bits)[i] = argb;
    return b;
}

static HICON ri_icon(int w, int h, DWORD rgb, int maskbit, BOOL fIcon, int hx, int hy)
{
    ICONINFO ii; HICON ic;
    ii.fIcon = fIcon; ii.xHotspot = hx; ii.yHotspot = hy;
    ii.hbmMask = ri_mkmask(w, h, maskbit);
    ii.hbmColor = ri_mkcolor24(w, h, rgb, NULL);
    ic = U(CreateIconIndirect)(&ii);
    DeleteObject(ii.hbmMask); DeleteObject(ii.hbmColor);
    return ic;
}

static HDC ri_dc(int w, int h, DWORD fill, DWORD **pbits, HBITMAP *old, HBITMAP *bmp)
{
    BITMAPINFO bi; void *bits = NULL; HDC dc = CreateCompatibleDC(NULL); int i;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    *bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    *old = SelectObject(dc, *bmp);
    for (i = 0; i < w * h; i++) ((DWORD *)bits)[i] = fill;
    *pbits = bits;
    return dc;
}
static void ri_dc_free(HDC dc, HBITMAP old, HBITMAP bmp) { SelectObject(dc, old); DeleteObject(bmp); DeleteDC(dc); }
#define PX(bits, w, x, y) ((bits)[(y) * (w) + (x)] & 0xFFFFFF)

/* ---- CreateIconIndirect ------------------------------------------------ */

TEST(resources_icons_obj_cii_color_roundtrip)
{
    static const int sz[] = {1, 8, 16, 32, 48, 64, 128};
    int i;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetIconInfo)); REQUIRE(U(DestroyIcon));
    for (i = 0; i < 7; i++) {
        int w = 0, h = 0; BOOL cur = TRUE;
        HICON ic = ri_icon(sz[i], sz[i], 0x336699, 0, TRUE, 0, 0);
        CHECKF(ic != NULL, "size %d", sz[i]);
        if (!ic) continue;
        CHECK(ri_iconsize(ic, &w, &h, NULL, &cur));
        RI_EQ(w, sz[i]); RI_EQ(h, sz[i]); RI_EQ(cur, FALSE);
        CHECK(U(DestroyIcon)(ic));
    }
}

TEST(resources_icons_obj_cii_nonsquare)
{
    int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetIconInfo));
    ic = ri_icon(24, 40, 0x00FF00, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (ic) { CHECK(ri_iconsize(ic, &w, &h, NULL, NULL)); RI_EQ(w, 24); RI_EQ(h, 40); U(DestroyIcon)(ic); }
}

TEST(resources_icons_obj_cii_cursor_hotspot)
{
    static const int hot[][2] = {{0, 0}, {5, 7}, {31, 31}, {16, 0}, {0, 31}};
    int i;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetIconInfo));
    for (i = 0; i < 5; i++) {
        ICONINFO ii;
        HICON c = ri_icon(32, 32, 0x0000FF, 0, FALSE, hot[i][0], hot[i][1]);
        CHECK(c != NULL);
        if (!c) continue;
        CHECK(U(GetIconInfo)(c, &ii));
        RI_EQ(ii.fIcon, FALSE);
        RI_EQ(ii.xHotspot, hot[i][0]);
        RI_EQ(ii.yHotspot, hot[i][1]);
        DeleteObject(ii.hbmMask); if (ii.hbmColor) DeleteObject(ii.hbmColor);
        CHECK(U(DestroyCursor)(c));
    }
}

TEST(resources_icons_obj_cii_icon_ignores_hotspot_note)
{
    ICONINFO ii;
    HICON ic = ri_icon(32, 32, 0x0000FF, 0, TRUE, 3, 4);
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetIconInfo));
    ic = ri_icon(32, 32, 0x0000FF, 0, TRUE, 3, 4);
    CHECK(ic != NULL);
    if (!ic) return;
    CHECK(U(GetIconInfo)(ic, &ii));
    RI_EQ(ii.fIcon, TRUE);
    t_note("icon created with hotspot 3,4 reports %lu,%lu", ii.xHotspot, ii.yHotspot);
    DeleteObject(ii.hbmMask); if (ii.hbmColor) DeleteObject(ii.hbmColor);
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_cii_null_and_bogus_bitmaps)
{
    ICONINFO ii;
    HBITMAP good;
    REQUIRE(U(CreateIconIndirect));
    good = ri_mkmask(32, 32, 0);
    ii.fIcon = TRUE; ii.xHotspot = ii.yHotspot = 0;
    ii.hbmMask = NULL; ii.hbmColor = NULL;
    RI_EQ(U(CreateIconIndirect)(&ii), NULL);
    ii.hbmMask = (HBITMAP)(ULONG_PTR)0x1234; ii.hbmColor = NULL;
    RI_EQ(U(CreateIconIndirect)(&ii), NULL);
    ii.hbmMask = NULL; ii.hbmColor = good;
    RI_EQ(U(CreateIconIndirect)(&ii), NULL);
    ii.hbmMask = good; ii.hbmColor = (HBITMAP)(ULONG_PTR)0x4321;
    RI_EQ(U(CreateIconIndirect)(&ii), NULL);
    ii.hbmMask = (HBITMAP)(ULONG_PTR)-1; ii.hbmColor = NULL;
    RI_EQ(U(CreateIconIndirect)(&ii), NULL);
    DeleteObject(good);
}

TEST(resources_icons_obj_cii_deleted_bitmap)
{
    ICONINFO ii;
    REQUIRE(U(CreateIconIndirect));
    ii.fIcon = TRUE; ii.xHotspot = ii.yHotspot = 0;
    ii.hbmMask = ri_mkmask(16, 16, 0);
    ii.hbmColor = ri_mkcolor24(16, 16, 0, NULL);
    DeleteObject(ii.hbmMask);
    RI_EQ(U(CreateIconIndirect)(&ii), NULL);
    DeleteObject(ii.hbmColor);
    RI_EQ(U(CreateIconIndirect)(&ii), NULL);
}

TEST(resources_icons_obj_cii_bitmaps_remain_owned_by_caller)
{
    ICONINFO ii; BITMAP bm;
    HICON ic;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(DestroyIcon));
    ii.fIcon = TRUE; ii.xHotspot = ii.yHotspot = 0;
    ii.hbmMask = ri_mkmask(16, 16, 0);
    ii.hbmColor = ri_mkcolor24(16, 16, 0xFF, NULL);
    ic = U(CreateIconIndirect)(&ii);
    CHECK(ic != NULL);
    /* caller keeps ownership: bitmaps still valid after icon creation and destruction */
    CHECK(ri_bmp(ii.hbmMask, &bm));
    CHECK(ri_bmp(ii.hbmColor, &bm));
    if (ic) CHECK(U(DestroyIcon)(ic));
    CHECK(ri_bmp(ii.hbmMask, &bm));
    CHECK(ri_bmp(ii.hbmColor, &bm));
    CHECK(DeleteObject(ii.hbmMask));
    CHECK(DeleteObject(ii.hbmColor));
}

TEST(resources_icons_obj_cii_mono)
{
    ICONINFO ii, back;
    HICON ic;
    int w = 0, h = 0;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetIconInfo));
    ii.fIcon = TRUE; ii.xHotspot = ii.yHotspot = 0;
    ii.hbmMask = ri_mkmask(32, 64, 0);   /* AND + XOR halves */
    ii.hbmColor = NULL;
    ic = U(CreateIconIndirect)(&ii);
    CHECK(ic != NULL);
    if (ic) {
        BITMAP bm;
        CHECK(U(GetIconInfo)(ic, &back));
        CHECK(back.hbmMask != NULL);
        if (back.hbmMask && ri_bmp(back.hbmMask, &bm)) { RI_EQ(bm.bmWidth, 32); RI_EQ(bm.bmBitsPixel, 1); t_note("mono mask height %d", bm.bmHeight); }
        t_note("mono icon hbmColor=%p", back.hbmColor);
        if (back.hbmMask) DeleteObject(back.hbmMask);
        if (back.hbmColor) DeleteObject(back.hbmColor);
        ri_iconsize(ic, &w, &h, NULL, NULL);
        RI_EQ(w, 32);
        U(DestroyIcon)(ic);
    }
    DeleteObject(ii.hbmMask);
}

TEST(resources_icons_obj_cii_alpha_32bpp)
{
    ICONINFO ii; BITMAP bm;
    HICON ic;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetIconInfo));
    ii.fIcon = TRUE; ii.xHotspot = ii.yHotspot = 0;
    ii.hbmMask = ri_mkmask(32, 32, 0);
    ii.hbmColor = ri_mkcolor32(32, 32, 0x80800000);
    ic = U(CreateIconIndirect)(&ii);
    CHECK(ic != NULL);
    if (ic) {
        ICONINFO b;
        CHECK(U(GetIconInfo)(ic, &b));
        CHECK(b.hbmColor != NULL);
        if (b.hbmColor && ri_bmp(b.hbmColor, &bm)) { RI_EQ(bm.bmBitsPixel, 32); RI_EQ(bm.bmWidth, 32); }
        DeleteObject(b.hbmMask); if (b.hbmColor) DeleteObject(b.hbmColor);
        U(DestroyIcon)(ic);
    }
    DeleteObject(ii.hbmMask); DeleteObject(ii.hbmColor);
}

/* ---- CreateIcon ------------------------------------------------------- */

TEST(resources_icons_obj_createicon_mono)
{
    BYTE a[128], x[128];
    HICON ic;
    int w = 0, h = 0;
    REQUIRE(U(CreateIcon)); REQUIRE(U(GetIconInfo));
    memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
    ic = U(CreateIcon)(GetModuleHandleA(NULL), 32, 32, 1, 1, a, x);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(ri_iconsize(ic, &w, &h, NULL, NULL));
        RI_EQ(w, 32); RI_EQ(h, 32);
        CHECK(U(DestroyIcon)(ic));
    }
}

TEST(resources_icons_obj_createicon_color_depths)
{
    static const int bpp[] = {1, 4, 8, 16, 24, 32};
    int i;
    REQUIRE(U(CreateIcon)); REQUIRE(U(GetIconInfo));
    for (i = 0; i < 6; i++) {
        BYTE a[128]; BYTE *x = calloc(1, 32 * 32 * 4);
        HICON ic;
        int w = 0, h = 0;
        memset(a, 0, sizeof a);
        ic = U(CreateIcon)(NULL, 32, 32, 1, bpp[i], a, x);
        CHECKF(ic != NULL, "bpp %d", bpp[i]);
        if (ic) {
            CHECK(ri_iconsize(ic, &w, &h, NULL, NULL));
            RI_EQ(w, 32); RI_EQ(h, 32);
            U(DestroyIcon)(ic);
        }
        free(x);
    }
}

TEST(resources_icons_obj_createicon_hinstance_variants)
{
    BYTE a[128], x[128];
    HICON ic;
    REQUIRE(U(CreateIcon));
    memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
    ic = U(CreateIcon)(NULL, 32, 32, 1, 1, a, x);
    CHECK(ic != NULL); if (ic) U(DestroyIcon)(ic);
    ic = U(CreateIcon)(GetModuleHandleA("kernel32.dll"), 32, 32, 1, 1, a, x);
    CHECK(ic != NULL); if (ic) U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_createicon_16x16_and_odd)
{
    static const int sz[][2] = {{16, 16}, {16, 8}, {32, 16}};
    int i;
    REQUIRE(U(CreateIcon)); REQUIRE(U(GetIconInfo));
    for (i = 0; i < 3; i++) {
        BYTE a[512], x[512]; int w = 0, h = 0;
        HICON ic;
        memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
        ic = U(CreateIcon)(NULL, sz[i][0], sz[i][1], 1, 1, a, x);
        CHECKF(ic != NULL, "%dx%d", sz[i][0], sz[i][1]);
        if (ic) { ri_iconsize(ic, &w, &h, NULL, NULL); RI_EQ(w, sz[i][0]); RI_EQ(h, sz[i][1]); U(DestroyIcon)(ic); }
    }
}

TEST(resources_icons_obj_createicon_zero_size_note)
{
    BYTE a[16] = {0}, x[16] = {0};
    HICON ic;
    REQUIRE(U(CreateIcon));
    SetLastError(0);
    ic = U(CreateIcon)(NULL, 0, 0, 1, 1, a, x);
    t_note("CreateIcon 0x0 -> %p err=%lu", ic, GetLastError());
    if (ic) U(DestroyIcon)(ic);
    ic = U(CreateIcon)(NULL, -1, -1, 1, 1, a, x);
    t_note("CreateIcon -1x-1 -> %p", ic);
    if (ic) U(DestroyIcon)(ic);
}

/* ---- CreateCursor ------------------------------------------------------ */

TEST(resources_icons_obj_createcursor_basic)
{
    BYTE a[128], x[128]; ICONINFO ii; HCURSOR c;
    REQUIRE(U(CreateCursor)); REQUIRE(U(GetIconInfo));
    memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
    c = U(CreateCursor)(GetModuleHandleA(NULL), 4, 6, 32, 32, a, x);
    CHECK(c != NULL);
    if (!c) return;
    CHECK(U(GetIconInfo)(c, &ii));
    RI_EQ(ii.fIcon, FALSE); RI_EQ(ii.xHotspot, 4); RI_EQ(ii.yHotspot, 6);
    if (ii.hbmMask) { BITMAP bm; ri_bmp(ii.hbmMask, &bm); RI_EQ(bm.bmWidth, 32); RI_EQ(bm.bmHeight, 64); DeleteObject(ii.hbmMask); }
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    CHECK(U(DestroyCursor)(c));
}

TEST(resources_icons_obj_createcursor_hotspot_bounds)
{
    /* decomp: xHot<0, xHot>width, yHot<0, yHot>height are rejected; == width/height accepted */
    BYTE a[128], x[128];
    HCURSOR c;
    static const int ok[][2] = {{0, 0}, {31, 31}, {32, 32}, {32, 0}, {0, 32}};
    static const int bad[][2] = {{-1, 0}, {0, -1}, {33, 0}, {0, 33}, {-1, -1}, {1000, 1000}, {0x7FFFFFFF, 0}};
    int i;
    REQUIRE(U(CreateCursor));
    memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
    for (i = 0; i < 5; i++) {
        c = U(CreateCursor)(NULL, ok[i][0], ok[i][1], 32, 32, a, x);
        CHECKF(c != NULL, "hot %d,%d", ok[i][0], ok[i][1]);
        if (c) U(DestroyCursor)(c);
    }
    for (i = 0; i < 7; i++) {
        c = U(CreateCursor)(NULL, bad[i][0], bad[i][1], 32, 32, a, x);
        CHECKF(c == NULL, "hot %d,%d", bad[i][0], bad[i][1]);
        if (c) U(DestroyCursor)(c);
    }
}

TEST(resources_icons_obj_createcursor_sizes)
{
    static const int sz[] = {8, 16, 24, 32, 48, 64};
    int i;
    REQUIRE(U(CreateCursor)); REQUIRE(U(GetIconInfo));
    for (i = 0; i < 6; i++) {
        BYTE a[8 * 64], x[8 * 64]; int w = 0, h = 0; BOOL cur = FALSE;
        HCURSOR c;
        memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
        c = U(CreateCursor)(NULL, 0, 0, sz[i], sz[i], a, x);
        CHECKF(c != NULL, "size %d", sz[i]);
        if (c) { ri_iconsize(c, &w, &h, NULL, &cur); RI_EQ(w, sz[i]); RI_EQ(h, sz[i]); RI_EQ(cur, TRUE); U(DestroyCursor)(c); }
    }
}

/* ---- Destroy / validity ------------------------------------------------ */

TEST(resources_icons_obj_destroy_invalid)
{
    REQUIRE(U(DestroyIcon)); REQUIRE(U(DestroyCursor));
    SetLastError(0);
    RI_EQ(U(DestroyIcon)(NULL), FALSE);
    t_note("DestroyIcon(NULL) err=%lu", GetLastError());
    RI_EQ(U(DestroyIcon)((HICON)(ULONG_PTR)0x12345), FALSE);
    RI_EQ(U(DestroyIcon)((HICON)(ULONG_PTR)1), FALSE);
    RI_EQ(U(DestroyIcon)((HICON)(ULONG_PTR)-1), FALSE);
    RI_EQ(U(DestroyCursor)(NULL), FALSE);
    RI_EQ(U(DestroyCursor)((HCURSOR)(ULONG_PTR)0x12345), FALSE);
    RI_EQ(U(DestroyCursor)((HCURSOR)(ULONG_PTR)-1), FALSE);
}

TEST(resources_icons_obj_destroy_wrong_object_type)
{
    HBITMAP b;
    REQUIRE(U(DestroyIcon));
    b = ri_mkmask(8, 8, 0);
    RI_EQ(U(DestroyIcon)((HICON)b), FALSE);
    RI_EQ(U(DestroyCursor)((HCURSOR)b), FALSE);
    {
        BITMAP bm;
        CHECK(ri_bmp(b, &bm)); /* bitmap untouched */
    }
    DeleteObject(b);
    {
        HBRUSH br = CreateSolidBrush(0);
        RI_EQ(U(DestroyIcon)((HICON)br), FALSE);
        CHECK(DeleteObject(br));
    }
}

TEST(resources_icons_obj_destroy_twice)
{
    HICON ic;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(DestroyIcon));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    RI_EQ(U(DestroyIcon)(ic), TRUE);
    SetLastError(0);
    RI_EQ(U(DestroyIcon)(ic), FALSE);
    t_note("second DestroyIcon err=%lu", GetLastError());
    RI_EQ(U(DestroyCursor)(ic), FALSE);
}

TEST(resources_icons_obj_destroy_cursor_icon_interchangeable)
{
    HICON ic, c;
    REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    c = ri_icon(16, 16, 0xFF, 0, FALSE, 1, 1);
    CHECK(ic && c);
    if (ic) RI_EQ(U(DestroyCursor)((HCURSOR)ic), TRUE);
    if (c) RI_EQ(U(DestroyIcon)(c), TRUE);
}

TEST(resources_icons_obj_geticoninfo_invalid)
{
    ICONINFO ii;
    HICON ic;
    REQUIRE(U(GetIconInfo)); REQUIRE(U(CreateIconIndirect));
    memset(&ii, 0, sizeof ii);
    SetLastError(0);
    RI_EQ(U(GetIconInfo)(NULL, &ii), FALSE);
    t_note("GetIconInfo(NULL) err=%lu", GetLastError());
    RI_EQ(U(GetIconInfo)((HICON)(ULONG_PTR)0x1234, &ii), FALSE);
    RI_EQ(U(GetIconInfo)((HICON)(ULONG_PTR)-1, &ii), FALSE);
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    if (ic) {
        U(DestroyIcon)(ic);
        SetLastError(0);
        RI_EQ(U(GetIconInfo)(ic, &ii), FALSE);
        t_note("GetIconInfo(destroyed) err=%lu", GetLastError());
    }
}

TEST(resources_icons_obj_geticoninfo_failure_leaves_struct)
{
    ICONINFO ii;
    REQUIRE(U(GetIconInfo));
    memset(&ii, 0x5A, sizeof ii);
    RI_EQ(U(GetIconInfo)(NULL, &ii), FALSE);
    RI_EQ(ii.hbmMask, (HBITMAP)0x5A5A5A5A5A5A5A5AULL);
    RI_EQ(ii.hbmColor, (HBITMAP)0x5A5A5A5A5A5A5A5AULL);
}

TEST(resources_icons_obj_geticoninfo_new_bitmaps_each_call)
{
    ICONINFO a, b;
    HICON ic;
    REQUIRE(U(GetIconInfo)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    CHECK(U(GetIconInfo)(ic, &a)); CHECK(U(GetIconInfo)(ic, &b));
    CHECK(a.hbmColor != b.hbmColor); CHECK(a.hbmMask != b.hbmMask);
    CHECK(a.hbmMask != a.hbmColor);
    DeleteObject(a.hbmMask); DeleteObject(a.hbmColor);
    /* b still valid after a is deleted */
    { BITMAP bm; CHECK(ri_bmp(b.hbmColor, &bm)); CHECK(ri_bmp(b.hbmMask, &bm)); }
    DeleteObject(b.hbmMask); DeleteObject(b.hbmColor);
    U(DestroyIcon)(ic);
}

/* ---- GetIconInfoEx ----------------------------------------------------- */

TEST(resources_icons_obj_iconinfoex_struct_sizes)
{
    RI_EQ(sizeof(ICONINFOEXW), 0x438);
    RI_EQ(sizeof(ICONINFOEXA), 0x230);
}

TEST(resources_icons_obj_iconinfoexw_basic)
{
    ICONINFOEXW e; HICON ic;
    REQUIRE(U(GetIconInfoExW)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(32, 32, 0xFF, 0, FALSE, 3, 5);
    CHECK(ic != NULL);
    if (!ic) return;
    memset(&e, 0xCC, sizeof e);
    e.cbSize = sizeof e;
    CHECK(U(GetIconInfoExW)(ic, &e));
    RI_EQ(e.fIcon, FALSE); RI_EQ(e.xHotspot, 3); RI_EQ(e.yHotspot, 5);
    CHECK(e.hbmMask != NULL); CHECK(e.hbmColor != NULL);
    RI_EQ(e.wResID, 0);
    RI_EQ(e.szModName[0], 0);
    RI_EQ(e.szResName[0], 0);
    if (e.hbmMask) DeleteObject(e.hbmMask);
    if (e.hbmColor) DeleteObject(e.hbmColor);
    U(DestroyCursor)(ic);
}

TEST(resources_icons_obj_iconinfoexw_bad_cbsize)
{
    static const DWORD sizes[] = {0, 1, 0x437, 0x439, 0x230, sizeof(ICONINFO), 0xFFFFFFFF};
    ICONINFOEXW e; HICON ic; int i;
    REQUIRE(U(GetIconInfoExW)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    for (i = 0; i < 7; i++) {
        memset(&e, 0, sizeof e);
        e.cbSize = sizes[i];
        SetLastError(0);
        CHECKF(U(GetIconInfoExW)(ic, &e) == FALSE, "cbSize 0x%lx", sizes[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "cbSize 0x%lx err=%lu", sizes[i], GetLastError());
    }
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_iconinfoexa_bad_cbsize)
{
    static const DWORD sizes[] = {0, 1, 0x22F, 0x231, 0x438, sizeof(ICONINFO), 0xFFFFFFFF};
    ICONINFOEXA e; HICON ic; int i;
    REQUIRE(U(GetIconInfoExA)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    for (i = 0; i < 7; i++) {
        memset(&e, 0, sizeof e);
        e.cbSize = sizes[i];
        SetLastError(0);
        CHECKF(U(GetIconInfoExA)(ic, &e) == FALSE, "cbSize 0x%lx", sizes[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "cbSize 0x%lx err=%lu", sizes[i], GetLastError());
    }
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_iconinfoex_a_w_parity)
{
    ICONINFOEXA a; ICONINFOEXW w; HICON ic;
    REQUIRE(U(GetIconInfoExA)); REQUIRE(U(GetIconInfoExW)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(32, 32, 0xFF, 0, FALSE, 2, 3);
    CHECK(ic != NULL);
    if (!ic) return;
    memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    a.cbSize = sizeof a; w.cbSize = sizeof w;
    CHECK(U(GetIconInfoExA)(ic, &a));
    CHECK(U(GetIconInfoExW)(ic, &w));
    RI_EQ(a.fIcon, w.fIcon); RI_EQ(a.xHotspot, w.xHotspot); RI_EQ(a.yHotspot, w.yHotspot);
    RI_EQ(a.wResID, w.wResID);
    RI_EQ(a.szModName[0], 0); RI_EQ(a.szResName[0], 0);
    if (a.hbmMask) DeleteObject(a.hbmMask);
    if (a.hbmColor) DeleteObject(a.hbmColor);
    if (w.hbmMask) DeleteObject(w.hbmMask);
    if (w.hbmColor) DeleteObject(w.hbmColor);
    U(DestroyCursor)(ic);
}

TEST(resources_icons_obj_iconinfoex_invalid_handle)
{
    ICONINFOEXW e;
    REQUIRE(U(GetIconInfoExW));
    memset(&e, 0, sizeof e); e.cbSize = sizeof e;
    RI_EQ(U(GetIconInfoExW)(NULL, &e), FALSE);
    RI_EQ(U(GetIconInfoExW)((HICON)(ULONG_PTR)0x1234, &e), FALSE);
}

TEST(resources_icons_obj_iconinfoex_fixture_resource)
{
    ICONINFOEXW e; HICON ic;
    REQUIRE(U(GetIconInfoExW)); REQUIRE(U(LoadImageW));
    REQUIRE_FIX(m);
    ic = U(LoadImageW)(m, MAKEINTRESOURCEW(RI_GRP_ICON), IMAGE_ICON, 32, 32, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    memset(&e, 0, sizeof e); e.cbSize = sizeof e;
    CHECK(U(GetIconInfoExW)(ic, &e));
    t_note("fixture icon: wResID=%u mod='%ls' res='%ls'", e.wResID, e.szModName, e.szResName);
    if (e.hbmMask) DeleteObject(e.hbmMask);
    if (e.hbmColor) DeleteObject(e.hbmColor);
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_iconinfoex_oem_icon)
{
    ICONINFOEXW e; HICON ic;
    REQUIRE(U(GetIconInfoExW)); REQUIRE(U(LoadIconW));
    ic = U(LoadIconW)(NULL, (LPCWSTR)IDI_APPLICATION);
    CHECK(ic != NULL);
    if (!ic) return;
    memset(&e, 0, sizeof e); e.cbSize = sizeof e;
    CHECK(U(GetIconInfoExW)(ic, &e));
    RI_EQ(e.fIcon, TRUE);
    t_note("IDI_APPLICATION: wResID=%u mod='%ls'", e.wResID, e.szModName);
    if (e.hbmMask) DeleteObject(e.hbmMask);
    if (e.hbmColor) DeleteObject(e.hbmColor);
}

/* ---- CopyIcon ---------------------------------------------------------- */

TEST(resources_icons_obj_copyicon_basic)
{
    HICON ic, cp; int w = 0, h = 0; BOOL cur = TRUE;
    REQUIRE(U(CopyIcon)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(24, 24, 0xFF00FF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    cp = U(CopyIcon)(ic);
    CHECK(cp != NULL);
    CHECK(cp != ic);
    if (cp) {
        CHECK(ri_iconsize(cp, &w, &h, NULL, &cur));
        RI_EQ(w, 24); RI_EQ(h, 24); RI_EQ(cur, FALSE);
        /* the copy is independent of the original */
        CHECK(U(DestroyIcon)(ic));
        ic = NULL;
        CHECK(ri_iconsize(cp, &w, &h, NULL, NULL));
        CHECK(U(DestroyIcon)(cp));
    }
    if (ic) U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_copyicon_cursor_hotspot)
{
    HICON c, cp; ICONINFO ii;
    REQUIRE(U(CopyIcon)); REQUIRE(U(CreateIconIndirect));
    c = ri_icon(32, 32, 0xFF, 0, FALSE, 7, 9);
    CHECK(c != NULL);
    if (!c) return;
    cp = U(CopyIcon)(c);
    CHECK(cp != NULL);
    if (cp) {
        CHECK(U(GetIconInfo)(cp, &ii));
        RI_EQ(ii.fIcon, FALSE); RI_EQ(ii.xHotspot, 7); RI_EQ(ii.yHotspot, 9);
        DeleteObject(ii.hbmMask); if (ii.hbmColor) DeleteObject(ii.hbmColor);
        U(DestroyCursor)(cp);
    }
    U(DestroyCursor)(c);
}

TEST(resources_icons_obj_copyicon_invalid)
{
    HICON ic;
    REQUIRE(U(CopyIcon)); REQUIRE(U(CreateIconIndirect));
    RI_EQ(U(CopyIcon)(NULL), NULL);
    RI_EQ(U(CopyIcon)((HICON)(ULONG_PTR)0x1234), NULL);
    RI_EQ(U(CopyIcon)((HICON)(ULONG_PTR)-1), NULL);
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    if (ic) { U(DestroyIcon)(ic); RI_EQ(U(CopyIcon)(ic), NULL); }
}

TEST(resources_icons_obj_copyicon_oem_icon)
{
    HICON ic, cp;
    REQUIRE(U(CopyIcon)); REQUIRE(U(LoadIconW));
    ic = U(LoadIconW)(NULL, (LPCWSTR)IDI_APPLICATION);
    CHECK(ic != NULL);
    cp = ic ? U(CopyIcon)(ic) : NULL;
    CHECK(cp != NULL);
    CHECK(cp != ic);
    if (cp) CHECK(U(DestroyIcon)(cp));
}

TEST(resources_icons_obj_copyicon_chain)
{
    HICON a, b, c;
    REQUIRE(U(CopyIcon)); REQUIRE(U(CreateIconIndirect));
    a = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    b = a ? U(CopyIcon)(a) : NULL;
    c = b ? U(CopyIcon)(b) : NULL;
    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
    if (c) CHECK(U(DestroyIcon)(c));
    if (b) CHECK(U(DestroyIcon)(b));
    if (a) CHECK(U(DestroyIcon)(a));
}

/* ---- CopyImage --------------------------------------------------------- */

TEST(resources_icons_obj_copyimage_invalid_flags)
{
    static const UINT bad[] = {0x100, 0x200, 0x400, 0x300, 0x20000, 0x40000, 0x80000000, 0xFFFE0000, 0xFFFFFFFF};
    HICON ic; int i;
    REQUIRE(U(CopyImage)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    for (i = 0; i < 9; i++) {
        SetLastError(0);
        CHECKF(U(CopyImage)(ic, IMAGE_ICON, 0, 0, bad[i]) == NULL, "flags 0x%x", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "flags 0x%x err=%lu", bad[i], GetLastError());
    }
    /* invalid flags are rejected before the handle is examined */
    for (i = 0; i < 9; i++) {
        SetLastError(0);
        CHECKF(U(CopyImage)(NULL, IMAGE_BITMAP, 0, 0, bad[i]) == NULL, "null flags 0x%x", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "null flags 0x%x err=%lu", bad[i], GetLastError());
    }
    if (ic) U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_copyimage_invalid_handle_type)
{
    REQUIRE(U(CopyImage));
    SetLastError(0);
    RI_EQ(U(CopyImage)(NULL, IMAGE_ICON, 0, 0, 0), NULL);
    RI_EQ(U(CopyImage)(NULL, IMAGE_CURSOR, 0, 0, 0), NULL);
    RI_EQ(U(CopyImage)(NULL, IMAGE_BITMAP, 0, 0, 0), NULL);
    RI_EQ(U(CopyImage)((HANDLE)(ULONG_PTR)0x1234, IMAGE_ICON, 0, 0, 0), NULL);
    RI_EQ(U(CopyImage)((HANDLE)(ULONG_PTR)0x1234, IMAGE_BITMAP, 0, 0, 0), NULL);
    RI_EQ(U(CopyImage)((HANDLE)(ULONG_PTR)0x1234, 3, 0, 0, 0), NULL);
    RI_EQ(U(CopyImage)((HANDLE)(ULONG_PTR)0x1234, 0xFFFF, 0, 0, 0), NULL);
}

TEST(resources_icons_obj_copyimage_bad_type_real_handle)
{
    HBITMAP b;
    REQUIRE(U(CopyImage));
    b = ri_mkmask(8, 8, 0);
    SetLastError(0);
    RI_EQ(U(CopyImage)(b, 3, 0, 0, 0), NULL);
    RI_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    RI_EQ(U(CopyImage)(b, 0x1234, 0, 0, 0), NULL);
    DeleteObject(b);
}

TEST(resources_icons_obj_copyimage_icon_same_size)
{
    HICON ic, cp; int w = 0, h = 0;
    REQUIRE(U(CopyImage)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(24, 24, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    cp = U(CopyImage)(ic, IMAGE_ICON, 0, 0, 0);
    CHECK(cp != NULL); CHECK(cp != ic);
    if (cp) { ri_iconsize(cp, &w, &h, NULL, NULL); RI_EQ(w, 24); RI_EQ(h, 24); U(DestroyIcon)(cp); }
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_copyimage_icon_resize)
{
    static const int sz[] = {8, 16, 32, 48, 100};
    HICON ic; int i;
    REQUIRE(U(CopyImage)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(32, 32, 0x123456, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    for (i = 0; i < 5; i++) {
        int w = 0, h = 0;
        HICON cp = U(CopyImage)(ic, IMAGE_ICON, sz[i], sz[i], 0);
        CHECKF(cp != NULL, "sz %d", sz[i]);
        if (cp) { ri_iconsize(cp, &w, &h, NULL, NULL); RI_EQ(w, sz[i]); RI_EQ(h, sz[i]); U(DestroyIcon)(cp); }
    }
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_copyimage_icon_defaultsize)
{
    HICON ic, cp; int w = 0, h = 0;
    REQUIRE(U(CopyImage)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(20, 20, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    cp = U(CopyImage)(ic, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    CHECK(cp != NULL);
    if (cp) { ri_iconsize(cp, &w, &h, NULL, NULL); RI_EQ(w, ri_defsize(0, 0)); RI_EQ(h, ri_defsize(0, 1)); U(DestroyIcon)(cp); }
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_copyimage_copydeleteorg_icon)
{
    HICON ic, cp; ICONINFO ii;
    REQUIRE(U(CopyImage)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    cp = U(CopyImage)(ic, IMAGE_ICON, 0, 0, LR_COPYDELETEORG);
    CHECK(cp != NULL);
    RI_EQ(U(GetIconInfo)(ic, &ii), FALSE);
    if (cp) { int w, h; CHECK(ri_iconsize(cp, &w, &h, NULL, NULL)); U(DestroyIcon)(cp); }
}

TEST(resources_icons_obj_copyimage_returnorg_icon)
{
    HICON ic, cp, cp2;
    REQUIRE(U(CopyImage)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    cp = U(CopyImage)(ic, IMAGE_ICON, 16, 16, LR_COPYRETURNORG);
    RI_EQ(cp, ic);
    cp = U(CopyImage)(ic, IMAGE_ICON, 0, 0, LR_COPYRETURNORG);
    RI_EQ(cp, ic);
    cp2 = U(CopyImage)(ic, IMAGE_ICON, 32, 32, LR_COPYRETURNORG);
    CHECK(cp2 != NULL); CHECK(cp2 != ic);
    if (cp2 && cp2 != ic) U(DestroyIcon)(cp2);
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_copyimage_cursor)
{
    HICON c, cp; ICONINFO ii;
    REQUIRE(U(CopyImage)); REQUIRE(U(CreateIconIndirect));
    c = ri_icon(32, 32, 0xFF, 0, FALSE, 8, 12);
    CHECK(c != NULL);
    if (!c) return;
    cp = U(CopyImage)(c, IMAGE_CURSOR, 0, 0, 0);
    CHECK(cp != NULL);
    if (cp) {
        CHECK(U(GetIconInfo)(cp, &ii));
        RI_EQ(ii.fIcon, FALSE); RI_EQ(ii.xHotspot, 8); RI_EQ(ii.yHotspot, 12);
        DeleteObject(ii.hbmMask); if (ii.hbmColor) DeleteObject(ii.hbmColor);
        U(DestroyCursor)(cp);
    }
    U(DestroyCursor)(c);
}

TEST(resources_icons_obj_copyimage_bitmap_basic)
{
    HBITMAP b, cp; BITMAP bm;
    REQUIRE(U(CopyImage));
    b = ri_mkcolor24(20, 10, 0x102030, NULL);
    cp = U(CopyImage)(b, IMAGE_BITMAP, 0, 0, 0);
    CHECK(cp != NULL); CHECK(cp != b);
    if (cp) {
        CHECK(ri_bmp(cp, &bm));
        RI_EQ(bm.bmWidth, 20); RI_EQ(bm.bmHeight, 10);
        DeleteObject(cp);
    }
    CHECK(ri_bmp(b, &bm));
    DeleteObject(b);
}

TEST(resources_icons_obj_copyimage_bitmap_resize)
{
    static const int sz[][2] = {{40, 20}, {10, 5}, {1, 1}, {100, 3}};
    HBITMAP b; int i;
    REQUIRE(U(CopyImage));
    b = ri_mkcolor24(20, 10, 0x102030, NULL);
    for (i = 0; i < 4; i++) {
        BITMAP bm;
        HBITMAP cp = U(CopyImage)(b, IMAGE_BITMAP, sz[i][0], sz[i][1], 0);
        CHECKF(cp != NULL, "%dx%d", sz[i][0], sz[i][1]);
        if (cp) { CHECK(ri_bmp(cp, &bm)); RI_EQ(bm.bmWidth, sz[i][0]); RI_EQ(bm.bmHeight, sz[i][1]); DeleteObject(cp); }
    }
    DeleteObject(b);
}

TEST(resources_icons_obj_copyimage_bitmap_monochrome)
{
    HBITMAP b, cp; BITMAP bm;
    REQUIRE(U(CopyImage));
    b = ri_mkcolor24(16, 16, 0xFFFFFF, NULL);
    cp = U(CopyImage)(b, IMAGE_BITMAP, 0, 0, LR_MONOCHROME);
    CHECK(cp != NULL);
    if (cp) { CHECK(ri_bmp(cp, &bm)); RI_EQ(bm.bmBitsPixel, 1); RI_EQ(bm.bmPlanes, 1); DeleteObject(cp); }
    DeleteObject(b);
}

TEST(resources_icons_obj_copyimage_bitmap_dibsection)
{
    HBITMAP b, cp; DIBSECTION ds;
    REQUIRE(U(CopyImage));
    b = ri_mkcolor24(16, 8, 0x808080, NULL);
    cp = U(CopyImage)(b, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    CHECK(cp != NULL);
    if (cp) {
        memset(&ds, 0, sizeof ds);
        RI_EQ(GetObjectW(cp, sizeof ds, &ds), sizeof ds);
        RI_EQ(ds.dsBm.bmWidth, 16); RI_EQ(ds.dsBm.bmHeight, 8);
        CHECK(ds.dsBm.bmBits != NULL);
        DeleteObject(cp);
    }
    /* without the flag a DDB comes back, even from a DIB section */
    cp = U(CopyImage)(b, IMAGE_BITMAP, 0, 0, 0);
    CHECK(cp != NULL);
    if (cp) { RI_EQ(GetObjectW(cp, sizeof ds, &ds), sizeof(BITMAP)); DeleteObject(cp); }
    DeleteObject(b);
}

TEST(resources_icons_obj_copyimage_bitmap_copydeleteorg)
{
    HBITMAP b, cp; BITMAP bm;
    REQUIRE(U(CopyImage));
    b = ri_mkcolor24(16, 8, 0x808080, NULL);
    cp = U(CopyImage)(b, IMAGE_BITMAP, 0, 0, LR_COPYDELETEORG);
    CHECK(cp != NULL);
    RI_EQ(ri_bmp(b, &bm), FALSE);
    if (cp) { CHECK(ri_bmp(cp, &bm)); DeleteObject(cp); }
}

TEST(resources_icons_obj_copyimage_bitmap_returnorg)
{
    HBITMAP b, cp;
    REQUIRE(U(CopyImage));
    b = ri_mkcolor24(16, 8, 0x808080, NULL);
    cp = U(CopyImage)(b, IMAGE_BITMAP, 16, 8, LR_COPYRETURNORG);
    RI_EQ(cp, b);
    cp = U(CopyImage)(b, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
    RI_EQ(cp, b);
    cp = U(CopyImage)(b, IMAGE_BITMAP, 32, 16, LR_COPYRETURNORG);
    CHECK(cp != NULL); CHECK(cp != b);
    if (cp && cp != b) DeleteObject(cp);
    DeleteObject(b);
}

TEST(resources_icons_obj_copyimage_bitmap_pixels_preserved)
{
    HBITMAP b, cp; DIBSECTION ds; DWORD px = 0;
    REQUIRE(U(CopyImage));
    b = ri_mkcolor24(8, 8, 0x4080C0, NULL);
    cp = U(CopyImage)(b, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    CHECK(cp != NULL);
    if (cp) {
        memset(&ds, 0, sizeof ds);
        if (GetObjectW(cp, sizeof ds, &ds) == sizeof ds && ds.dsBm.bmBits) {
            int bpp = ds.dsBm.bmBitsPixel;
            BYTE *p = ds.dsBm.bmBits;
            if (bpp == 24 || bpp == 32) px = (p[2] << 16) | (p[1] << 8) | p[0];
            RI_EQ(px, 0x4080C0);
        }
        DeleteObject(cp);
    }
    DeleteObject(b);
}

/* ---- DrawIconEx ------------------------------------------------------- */

TEST(resources_icons_obj_draw_opaque)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(48, 48, 0x00808080, &bits, &old, &bmp);
    ic = ri_icon(32, 32, 0xFF0000, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, 4, 4, ic, 32, 32, 0, NULL, DI_NORMAL));
        GdiFlush();
        RI_EQ(PX(bits, 48, 4, 4), 0xFF0000);
        RI_EQ(PX(bits, 48, 35, 35), 0xFF0000);
        RI_EQ(PX(bits, 48, 20, 20), 0xFF0000);
        RI_EQ(PX(bits, 48, 3, 4), 0x808080);
        RI_EQ(PX(bits, 48, 4, 3), 0x808080);
        RI_EQ(PX(bits, 48, 36, 20), 0x808080);
        RI_EQ(PX(bits, 48, 20, 36), 0x808080);
        U(DestroyIcon)(ic);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_transparent)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic; int x, y, bad = 0;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(32, 32, 0x00406080, &bits, &old, &bmp);
    ic = ri_icon(32, 32, 0x000000, 1, TRUE, 0, 0); /* AND=1, XOR=0: leaves background */
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, 0, 0, ic, 32, 32, 0, NULL, DI_NORMAL));
        GdiFlush();
        for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) if (PX(bits, 32, x, y) != 0x406080) bad++;
        RI_EQ(bad, 0);
        U(DestroyIcon)(ic);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_mask_only)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(32, 32, 0x00FFFFFF, &bits, &old, &bmp);
    ic = ri_icon(32, 32, 0x00FF00, 0, TRUE, 0, 0); /* mask all 0 -> black when mask is drawn */
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, 0, 0, ic, 32, 32, 0, NULL, DI_MASK));
        GdiFlush();
        RI_EQ(PX(bits, 32, 16, 16), 0x000000);
        RI_EQ(PX(bits, 32, 0, 0), 0x000000);
        U(DestroyIcon)(ic);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_image_only_opaque)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(32, 32, 0x00000000, &bits, &old, &bmp);
    ic = ri_icon(32, 32, 0x00FF00, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, 0, 0, ic, 32, 32, 0, NULL, DI_IMAGE));
        GdiFlush();
        t_note("DI_IMAGE on black: %06x", PX(bits, 32, 16, 16));
        U(DestroyIcon)(ic);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_scaled_down)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(48, 48, 0x00808080, &bits, &old, &bmp);
    ic = ri_icon(32, 32, 0x0000FF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, 0, 0, ic, 16, 16, 0, NULL, DI_NORMAL));
        GdiFlush();
        RI_EQ(PX(bits, 48, 0, 0), 0x0000FF);
        RI_EQ(PX(bits, 48, 8, 8), 0x0000FF);
        RI_EQ(PX(bits, 48, 15, 15), 0x0000FF);
        RI_EQ(PX(bits, 48, 16, 16), 0x808080);
        RI_EQ(PX(bits, 48, 16, 0), 0x808080);
        U(DestroyIcon)(ic);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_scaled_up)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(64, 64, 0x00808080, &bits, &old, &bmp);
    ic = ri_icon(16, 16, 0x0000FF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, 0, 0, ic, 48, 48, 0, NULL, DI_NORMAL));
        GdiFlush();
        RI_EQ(PX(bits, 64, 24, 24), 0x0000FF);
        RI_EQ(PX(bits, 64, 47, 47), 0x0000FF);
        RI_EQ(PX(bits, 64, 48, 48), 0x808080);
        U(DestroyIcon)(ic);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_native_size_zero)
{
    /* cxWidth/cyWidth 0 without DI_DEFAULTSIZE: draw at the icon's own size */
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(48, 48, 0x00808080, &bits, &old, &bmp);
    ic = ri_icon(20, 20, 0x00FF00, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, 0, 0, ic, 0, 0, 0, NULL, DI_NORMAL));
        GdiFlush();
        RI_EQ(PX(bits, 48, 19, 19), 0x00FF00);
        RI_EQ(PX(bits, 48, 20, 20), 0x808080);
        U(DestroyIcon)(ic);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_clipped_offscreen)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(16, 16, 0x00808080, &bits, &old, &bmp);
    ic = ri_icon(32, 32, 0x00FF00, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, -16, -16, ic, 32, 32, 0, NULL, DI_NORMAL));
        CHECK(U(DrawIconEx)(dc, 8, 8, ic, 32, 32, 0, NULL, DI_NORMAL));
        CHECK(U(DrawIconEx)(dc, 1000, 1000, ic, 32, 32, 0, NULL, DI_NORMAL));
        GdiFlush();
        RI_EQ(PX(bits, 16, 0, 0), 0x00FF00);
        RI_EQ(PX(bits, 16, 15, 15), 0x00FF00);
        U(DestroyIcon)(ic);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_cursor_and_drawicon)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON c;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(DrawIcon)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(40, 40, 0x00808080, &bits, &old, &bmp);
    c = ri_icon(32, 32, 0xFF00FF, 0, FALSE, 2, 2);
    CHECK(c != NULL);
    if (c) {
        CHECK(U(DrawIconEx)(dc, 0, 0, c, 32, 32, 0, NULL, DI_NORMAL));
        GdiFlush();
        RI_EQ(PX(bits, 40, 10, 10), 0xFF00FF);
        { int i; for (i = 0; i < 1600; i++) bits[i] = 0x808080; }
        CHECK(U(DrawIcon)(dc, 4, 4, c));
        GdiFlush();
        RI_EQ(PX(bits, 40, 4, 4), 0xFF00FF);
        RI_EQ(PX(bits, 40, 35, 35), 0xFF00FF);
        RI_EQ(PX(bits, 40, 3, 3), 0x808080);
        U(DestroyCursor)(c);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_alpha_opaque_and_half)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    ICONINFO ii; HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(32, 32, 0x00000000, &bits, &old, &bmp);
    ii.fIcon = TRUE; ii.xHotspot = ii.yHotspot = 0;
    ii.hbmMask = ri_mkmask(32, 32, 0);
    ii.hbmColor = ri_mkcolor32(32, 32, 0xFFFF0000);
    ic = U(CreateIconIndirect)(&ii);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(U(DrawIconEx)(dc, 0, 0, ic, 32, 32, 0, NULL, DI_NORMAL));
        GdiFlush();
        RI_EQ(PX(bits, 32, 16, 16), 0xFF0000);
        U(DestroyIcon)(ic);
    }
    DeleteObject(ii.hbmColor);
    /* premultiplied half-alpha red over black */
    ii.hbmColor = ri_mkcolor32(32, 32, 0x80800000);
    ic = U(CreateIconIndirect)(&ii);
    CHECK(ic != NULL);
    if (ic) {
        DWORD v;
        int i; for (i = 0; i < 1024; i++) bits[i] = 0;
        CHECK(U(DrawIconEx)(dc, 0, 0, ic, 32, 32, 0, NULL, DI_NORMAL));
        GdiFlush();
        v = PX(bits, 32, 16, 16);
        CHECK((v & 0xFFFF) == 0);
        t_note("half-alpha red (premultiplied 0x80 source) over black -> %06x", v);
        U(DestroyIcon)(ic);
    }
    DeleteObject(ii.hbmColor); DeleteObject(ii.hbmMask);
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_invalid_args)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(16, 16, 0x00808080, &bits, &old, &bmp);
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    SetLastError(0);
    RI_EQ(U(DrawIconEx)(NULL, 0, 0, ic, 16, 16, 0, NULL, DI_NORMAL), FALSE);
    t_note("DrawIconEx(NULL dc) err=%lu", GetLastError());
    SetLastError(0);
    RI_EQ(U(DrawIconEx)(dc, 0, 0, NULL, 16, 16, 0, NULL, DI_NORMAL), FALSE);
    t_note("DrawIconEx(NULL icon) err=%lu", GetLastError());
    RI_EQ(U(DrawIconEx)(dc, 0, 0, (HICON)(ULONG_PTR)0x1234, 16, 16, 0, NULL, DI_NORMAL), FALSE);
    RI_EQ(U(DrawIconEx)((HDC)(ULONG_PTR)0x1234, 0, 0, ic, 16, 16, 0, NULL, DI_NORMAL), FALSE);
    RI_EQ(U(DrawIconEx)(dc, 0, 0, (HICON)(ULONG_PTR)-1, 16, 16, 0, NULL, DI_NORMAL), FALSE);
    if (ic) {
        U(DestroyIcon)(ic);
        RI_EQ(U(DrawIconEx)(dc, 0, 0, ic, 16, 16, 0, NULL, DI_NORMAL), FALSE);
    }
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_invalid_flags)
{
    /* decomp: diFlags & ~0x1F => ERROR_INVALID_PARAMETER before anything else is examined */
    static const UINT bad[] = {0x20, 0x40, 0x80, 0x100, 0x10000, 0x80000000, 0xFFFFFFE0, 0xFFFFFFFF, 0x23};
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    HICON ic; int i;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    dc = ri_dc(16, 16, 0x00808080, &bits, &old, &bmp);
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    for (i = 0; i < 9; i++) {
        SetLastError(0);
        CHECKF(U(DrawIconEx)(dc, 0, 0, ic, 16, 16, 0, NULL, bad[i]) == FALSE, "flags 0x%x", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "flags 0x%x err=%lu", bad[i], GetLastError());
        SetLastError(0);
        CHECKF(U(DrawIconEx)(NULL, 0, 0, NULL, 16, 16, 0, NULL, bad[i]) == FALSE, "null flags 0x%x", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "null flags 0x%x err=%lu", bad[i], GetLastError());
    }
    GdiFlush();
    RI_EQ(PX(bits, 16, 8, 8), 0x808080);
    /* every defined flag bit is accepted */
    {
        static const UINT ok[] = {DI_MASK, DI_IMAGE, DI_NORMAL, DI_COMPAT, DI_DEFAULTSIZE, DI_NOMIRROR, 0x1F};
        for (i = 0; i < 7; i++) {
            SetLastError(0);
            U(DrawIconEx)(dc, 0, 0, ic, 16, 16, 0, NULL, ok[i]);
            CHECKF(GetLastError() != ERROR_INVALID_PARAMETER, "flags 0x%x rejected", ok[i]);
        }
    }
    if (ic) U(DestroyIcon)(ic);
    ri_dc_free(dc, old, bmp);
}

TEST(resources_icons_obj_draw_deleted_dc)
{
    HDC dc; HICON ic;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    dc = CreateCompatibleDC(NULL);
    DeleteDC(dc);
    RI_EQ(U(DrawIconEx)(dc, 0, 0, ic, 16, 16, 0, NULL, DI_NORMAL), FALSE);
    if (ic) U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_draw_oem_icons)
{
    HBITMAP old, bmp; DWORD *bits; HDC dc;
    static const LPCSTR ids[] = {IDI_APPLICATION, IDI_ERROR, IDI_QUESTION, IDI_WARNING, IDI_INFORMATION, IDI_WINLOGO, IDI_SHIELD};
    int i;
    REQUIRE(U(DrawIconEx)); REQUIRE(U(LoadIconW));
    dc = ri_dc(64, 64, 0x00FFFFFF, &bits, &old, &bmp);
    for (i = 0; i < 7; i++) {
        HICON ic = U(LoadIconW)(NULL, (LPCWSTR)ids[i]);
        CHECKF(ic != NULL, "oem %d", i);
        if (ic) CHECKF(U(DrawIconEx)(dc, 0, 0, ic, 32, 32, 0, NULL, DI_NORMAL), "draw %d", i);
    }
    ri_dc_free(dc, old, bmp);
}

/* ---- leaks ------------------------------------------------------------ */

#define LEAKN 200

TEST(resources_icons_obj_leak_createiconindirect)
{
    DWORD u0, g0, u1, g1; int i;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetGuiResources));
    u0 = ri_gui(GR_USEROBJECTS); g0 = ri_gui(GR_GDIOBJECTS);
    for (i = 0; i < LEAKN; i++) {
        HICON ic = ri_icon(32, 32, 0xFF, 0, TRUE, 0, 0);
        if (!ic) { CHECK(0); break; }
        if (i == 0) CHECK(ri_gui(GR_USEROBJECTS) > u0);
        U(DestroyIcon)(ic);
    }
    u1 = ri_gui(GR_USEROBJECTS); g1 = ri_gui(GR_GDIOBJECTS);
    RI_EQ(u1, u0);
    RI_EQ(g1, g0);
}

TEST(resources_icons_obj_leak_copyicon_and_geticoninfo)
{
    DWORD u0, g0, u1, g1; int i;
    HICON src;
    REQUIRE(U(CopyIcon)); REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateIconIndirect));
    src = ri_icon(32, 32, 0xFF, 0, TRUE, 0, 0);
    CHECK(src != NULL);
    u0 = ri_gui(GR_USEROBJECTS); g0 = ri_gui(GR_GDIOBJECTS);
    for (i = 0; i < LEAKN; i++) {
        HICON cp = U(CopyIcon)(src);
        ICONINFO ii;
        if (!cp) { CHECK(0); break; }
        if (U(GetIconInfo)(cp, &ii)) { DeleteObject(ii.hbmMask); if (ii.hbmColor) DeleteObject(ii.hbmColor); }
        U(DestroyIcon)(cp);
    }
    u1 = ri_gui(GR_USEROBJECTS); g1 = ri_gui(GR_GDIOBJECTS);
    RI_EQ(u1, u0);
    RI_EQ(g1, g0);
    if (src) U(DestroyIcon)(src);
}

TEST(resources_icons_obj_leak_geticoninfo_without_delete)
{
    DWORD g0, g1, g2; ICONINFO ii; HICON ic;
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateIconIndirect));
    ic = ri_icon(32, 32, 0xFF, 0, TRUE, 0, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    g0 = ri_gui(GR_GDIOBJECTS);
    CHECK(U(GetIconInfo)(ic, &ii));
    g1 = ri_gui(GR_GDIOBJECTS);
    CHECKF(g1 >= g0 + 2, "gdi objects %lu -> %lu after GetIconInfo", g0, g1);
    DeleteObject(ii.hbmMask); DeleteObject(ii.hbmColor);
    g2 = ri_gui(GR_GDIOBJECTS);
    RI_EQ(g2, g0);
    U(DestroyIcon)(ic);
}

TEST(resources_icons_obj_leak_createcursor_and_fromres)
{
    DWORD u0, u1; int i;
    REQUIRE(U(CreateCursor)); REQUIRE(U(CreateIconFromResourceEx)); REQUIRE(U(GetGuiResources));
    u0 = ri_gui(GR_USEROBJECTS);
    for (i = 0; i < 100; i++) {
        BYTE a[128], x[128]; DWORD n; BYTE *d;
        HCURSOR c;
        HICON ic;
        memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
        c = U(CreateCursor)(NULL, 1, 1, 32, 32, a, x);
        if (c) U(DestroyCursor)(c); else CHECK(0);
        d = ri_dib(16, 16, 8, 0xFF112233, 0, &n);
        ic = U(CreateIconFromResourceEx)(d, n, TRUE, 0x30000, 0, 0, 0);
        if (ic) U(DestroyIcon)(ic); else CHECK(0);
        free(d);
    }
    u1 = ri_gui(GR_USEROBJECTS);
    RI_EQ(u1, u0);
}

TEST(resources_icons_obj_leak_user_count_delta)
{
    DWORD u0, u1, u2; HICON ic[10]; int i;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetGuiResources));
    u0 = ri_gui(GR_USEROBJECTS);
    for (i = 0; i < 10; i++) ic[i] = ri_icon(16, 16, 0xFF, 0, TRUE, 0, 0);
    u1 = ri_gui(GR_USEROBJECTS);
    RI_EQ(u1, u0 + 10);
    for (i = 0; i < 10; i++) if (ic[i]) U(DestroyIcon)(ic[i]);
    u2 = ri_gui(GR_USEROBJECTS);
    RI_EQ(u2, u0);
}

TEST(resources_icons_obj_leak_loadimage_unshared)
{
    DWORD u0, u1; int i;
    REQUIRE(U(LoadImageW)); REQUIRE(U(GetGuiResources));
    REQUIRE_FIX(m);
    u0 = ri_gui(GR_USEROBJECTS);
    for (i = 0; i < 100; i++) {
        HICON ic = U(LoadImageW)(m, MAKEINTRESOURCEW(RI_GRP_ICON), IMAGE_ICON, 32, 32, 0);
        if (ic) U(DestroyIcon)(ic); else { CHECK(0); break; }
    }
    u1 = ri_gui(GR_USEROBJECTS);
    RI_EQ(u1, u0);
}

TEST(resources_icons_obj_many_icons_alive)
{
    HICON ic[300]; int i, n = 0, uniq = 1;
    REQUIRE(U(CreateIconIndirect));
    for (i = 0; i < 300; i++) { ic[i] = ri_icon(16, 16, i, 0, TRUE, 0, 0); if (ic[i]) n++; }
    RI_EQ(n, 300);
    for (i = 1; i < 300; i++) if (ic[i] == ic[i - 1]) uniq = 0;
    RI_EQ(uniq, 1);
    for (i = 0; i < 300; i++) if (ic[i]) U(DestroyIcon)(ic[i]);
}
