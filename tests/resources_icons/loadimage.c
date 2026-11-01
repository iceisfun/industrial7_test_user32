#include "ri.h"

/* LoadIcon / LoadCursor / LoadImage / LoadBitmap: OEM resources and files. */

#define W_(x) ((LPCWSTR)(ULONG_PTR)(x))

static const WORD ri_oem_cursors[] = {32512, 32513, 32514, 32515, 32516, 32642, 32643, 32644, 32645, 32646, 32648, 32649, 32650};
static const WORD ri_oem_icons[] = {32512, 32513, 32514, 32515, 32516, 32517};

/* ---- OEM cursors ------------------------------------------------------- */


/* Win10 22H2 oracle: LoadImage(NULL, OEM id, ...) without LR_SHARED fails (NULL) for icons and
 * cursors; only the LR_SHARED form returns a handle. Try the requested flags first, note the
 * failure, and fall back to LR_SHARED so the size/identity checks still run. *shared reports
 * whether the returned handle is a shared (not destroyable) one. */
static HANDLE ri_oem_load(UINT id, UINT type, int cx, int cy, UINT fl, int *shared)
{
    HANDLE h = U(LoadImageW)(NULL, MAKEINTRESOURCEW(id), type, cx, cy, fl);
    *shared = (fl & LR_SHARED) != 0;
    if (!h && !(fl & LR_SHARED)) {
        t_note("LoadImage(NULL, OEM %u) without LR_SHARED -> NULL err=%lu", id, GetLastError());
        h = U(LoadImageW)(NULL, MAKEINTRESOURCEW(id), type, cx, cy, fl | LR_SHARED);
        *shared = 1;
    }
    return h;
}

TEST(resources_icons_li_oem_cursors_load)
{
    int i;
    REQUIRE(U(LoadCursorW));
    for (i = 0; i < 13; i++) {
        HCURSOR c = U(LoadCursorW)(NULL, W_(ri_oem_cursors[i]));
        CHECKF(c != NULL, "OCR %u", ri_oem_cursors[i]);
    }
}

TEST(resources_icons_li_oem_cursors_shared_handle)
{
    int i;
    REQUIRE(U(LoadCursorW));
    for (i = 0; i < 13; i++) {
        HCURSOR a = U(LoadCursorW)(NULL, W_(ri_oem_cursors[i]));
        HCURSOR b = U(LoadCursorW)(NULL, W_(ri_oem_cursors[i]));
        CHECKF(a == b, "OCR %u: %p vs %p", ri_oem_cursors[i], a, b);
    }
}

TEST(resources_icons_li_oem_cursors_distinct)
{
    HCURSOR h[13]; int i, j, dup = 0;
    REQUIRE(U(LoadCursorW));
    for (i = 0; i < 13; i++) h[i] = U(LoadCursorW)(NULL, W_(ri_oem_cursors[i]));
    for (i = 0; i < 13; i++) for (j = i + 1; j < 13; j++) if (h[i] == h[j]) dup++;
    RI_EQ(dup, 0);
}

TEST(resources_icons_li_oem_cursors_a_w_parity)
{
    int i;
    REQUIRE(U(LoadCursorW)); REQUIRE(U(LoadCursorA));
    for (i = 0; i < 13; i++) {
        HCURSOR a = U(LoadCursorA)(NULL, (LPCSTR)(ULONG_PTR)ri_oem_cursors[i]);
        HCURSOR w = U(LoadCursorW)(NULL, W_(ri_oem_cursors[i]));
        CHECKF(a == w, "OCR %u", ri_oem_cursors[i]);
    }
}

TEST(resources_icons_li_oem_cursors_geticoninfo)
{
    int i;
    REQUIRE(U(LoadCursorW)); REQUIRE(U(GetIconInfo));
    for (i = 0; i < 13; i++) {
        ICONINFO ii; BITMAP bm;
        HCURSOR c = U(LoadCursorW)(NULL, W_(ri_oem_cursors[i]));
        if (!c) { CHECK(0); continue; }
        CHECKF(U(GetIconInfo)(c, &ii), "OCR %u", ri_oem_cursors[i]);
        RI_EQ(ii.fIcon, FALSE);
        if (ii.hbmMask && ri_bmp(ii.hbmMask, &bm)) {
            CHECKF(ii.xHotspot <= (DWORD)bm.bmWidth && ii.yHotspot <= (DWORD)bm.bmHeight, "OCR %u hotspot %lu,%lu", ri_oem_cursors[i], ii.xHotspot, ii.yHotspot);
            CHECKF(bm.bmWidth > 0 && bm.bmHeight > 0, "OCR %u", ri_oem_cursors[i]);
        }
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
    }
}

TEST(resources_icons_li_oem_cursor_arrow_hotspot)
{
    ICONINFO ii; HCURSOR c;
    REQUIRE(U(LoadCursorW)); REQUIRE(U(GetIconInfo));
    c = U(LoadCursorW)(NULL, (LPCWSTR)IDC_ARROW);
    CHECK(c != NULL);
    if (!c) return;
    CHECK(U(GetIconInfo)(c, &ii));
    /* the arrow points at the top-left corner */
    RI_EQ(ii.xHotspot, 0); RI_EQ(ii.yHotspot, 0);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
}

TEST(resources_icons_li_oem_cursor_size_metric)
{
    int w = 0, h = 0;
    HCURSOR c;
    REQUIRE(U(LoadCursorW)); REQUIRE(U(GetIconInfo));
    c = U(LoadCursorW)(NULL, (LPCWSTR)IDC_ARROW);
    CHECK(c != NULL);
    if (c && ri_iconsize(c, &w, &h, NULL, NULL)) {
        t_note("IDC_ARROW %dx%d, SM_CXCURSOR=%d SM_CYCURSOR=%d", w, h, GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR));
        CHECK(w > 0 && h > 0);
    }
}

TEST(resources_icons_li_oem_cursor_loadimage_shared)
{
    HANDLE a, b;
    REQUIRE(U(LoadImageW));
    a = U(LoadImageW)(NULL, W_(32512), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
    b = U(LoadImageW)(NULL, W_(32512), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE);
    CHECK(a != NULL);
    RI_EQ(a, b);
    if (a && U(LoadCursorW)) t_note("LoadImage shared == LoadCursor handle: %d", a == (HANDLE)U(LoadCursorW)(NULL, W_(32512)));
}

TEST(resources_icons_li_oem_cursor_loadimage_unshared)
{
    HANDLE a, b; int sa, sb;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyCursor));
    a = ri_oem_load(32513, IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE, &sa);
    b = ri_oem_load(32513, IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE, &sb);
    t_note("unshared OEM cursor loads: %p %p", a, b);
    CHECK(a != NULL);
    if (a && b && a != b && !sa && !sb) { CHECK(U(DestroyCursor)(a)); CHECK(U(DestroyCursor)(b)); }
}

TEST(resources_icons_li_cursor_bogus_ids)
{
    REQUIRE(U(LoadCursorW)); REQUIRE(U(LoadIconW));
    SetLastError(0);
    CHECK(U(LoadCursorW)(NULL, W_(5000)) == NULL);
    t_note("LoadCursor(NULL, 5000) err=%lu", GetLastError());
    CHECK(U(LoadCursorW)(NULL, W_(0xFFFF)) == NULL);
    CHECK(U(LoadCursorW)(NULL, W_(32511)) == NULL);
    CHECK(U(LoadCursorW)(NULL, W_(32767)) == NULL);
    CHECK(U(LoadIconW)(NULL, W_(5000)) == NULL);
    CHECK(U(LoadIconW)(NULL, W_(32511)) == NULL);
    CHECK(U(LoadIconW)(NULL, W_(0xFFFF)) == NULL);
}

TEST(resources_icons_li_cursor_name_missing_in_exe)
{
    HINSTANCE self = GetModuleHandleA(NULL);
    REQUIRE(U(LoadCursorW)); REQUIRE(U(LoadCursorA)); REQUIRE(U(LoadIconA)); REQUIRE(U(LoadIconW));
    CHECK(U(LoadCursorW)(self, L"NO_SUCH_CURSOR") == NULL);
    CHECK(U(LoadCursorA)(self, "NO_SUCH_CURSOR") == NULL);
    CHECK(U(LoadIconW)(self, L"NO_SUCH_ICON") == NULL);
    CHECK(U(LoadIconA)(self, "NO_SUCH_ICON") == NULL);
    CHECK(U(LoadCursorW)(self, W_(1)) == NULL);
    CHECK(U(LoadIconW)(self, W_(1)) == NULL);
}

/* ---- OEM icons -------------------------------------------------------- */

TEST(resources_icons_li_oem_icons_load_and_share)
{
    int i;
    REQUIRE(U(LoadIconW));
    for (i = 0; i < 6; i++) {
        HICON a = U(LoadIconW)(NULL, W_(ri_oem_icons[i]));
        HICON b = U(LoadIconW)(NULL, W_(ri_oem_icons[i]));
        CHECKF(a != NULL, "OIC %u", ri_oem_icons[i]);
        CHECKF(a == b, "OIC %u shared", ri_oem_icons[i]);
    }
}

TEST(resources_icons_li_oem_icons_a_w_parity)
{
    int i;
    REQUIRE(U(LoadIconW)); REQUIRE(U(LoadIconA));
    for (i = 0; i < 6; i++) {
        HICON a = U(LoadIconA)(NULL, (LPCSTR)(ULONG_PTR)ri_oem_icons[i]);
        HICON w = U(LoadIconW)(NULL, W_(ri_oem_icons[i]));
        CHECKF(a == w, "OIC %u", ri_oem_icons[i]);
    }
}

TEST(resources_icons_li_oem_icons_distinct)
{
    HICON h[6]; int i, j, dup = 0;
    REQUIRE(U(LoadIconW));
    for (i = 0; i < 6; i++) h[i] = U(LoadIconW)(NULL, W_(ri_oem_icons[i]));
    for (i = 0; i < 6; i++) for (j = i + 1; j < 6; j++) if (h[i] == h[j]) dup++;
    t_note("duplicate OEM icon handles: %d", dup);
    CHECK(h[1] != h[2]);
}

TEST(resources_icons_li_oem_icon_aliases)
{
    /* IDI_ERROR==IDI_HAND, IDI_WARNING==IDI_EXCLAMATION, IDI_INFORMATION==IDI_ASTERISK are the same ids */
    HICON a, b;
    REQUIRE(U(LoadIconW));
    a = U(LoadIconW)(NULL, (LPCWSTR)IDI_ERROR); b = U(LoadIconW)(NULL, (LPCWSTR)IDI_HAND);
    CHECK(a != NULL); RI_EQ(a, b);
    a = U(LoadIconW)(NULL, (LPCWSTR)IDI_WARNING); b = U(LoadIconW)(NULL, (LPCWSTR)IDI_EXCLAMATION);
    CHECK(a != NULL); RI_EQ(a, b);
    a = U(LoadIconW)(NULL, (LPCWSTR)IDI_INFORMATION); b = U(LoadIconW)(NULL, (LPCWSTR)IDI_ASTERISK);
    CHECK(a != NULL); RI_EQ(a, b);
}

TEST(resources_icons_li_oem_icon_info)
{
    int i;
    REQUIRE(U(LoadIconW)); REQUIRE(U(GetIconInfo));
    for (i = 0; i < 6; i++) {
        ICONINFO ii; BITMAP bm;
        HICON ic = U(LoadIconW)(NULL, W_(ri_oem_icons[i]));
        if (!ic) { CHECK(0); continue; }
        CHECK(U(GetIconInfo)(ic, &ii));
        RI_EQ(ii.fIcon, TRUE);
        CHECK(ii.hbmMask != NULL);
        if (ii.hbmColor && ri_bmp(ii.hbmColor, &bm)) {
            CHECKF(bm.bmWidth == GetSystemMetrics(SM_CXICON) || bm.bmWidth > 0, "OIC %u width %d", ri_oem_icons[i], bm.bmWidth);
            RI_EQ(ii.xHotspot, bm.bmWidth / 2);
            RI_EQ(ii.yHotspot, bm.bmHeight / 2);
        }
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
    }
}

TEST(resources_icons_li_oem_icon_loadimage_sizes)
{
    static const int sz[] = {16, 32, 48};
    int i;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    for (i = 0; i < 3; i++) {
        int w = 0, h = 0;
        int sh;
        HICON ic = ri_oem_load(32512, IMAGE_ICON, sz[i], sz[i], 0, &sh);
        CHECKF(ic != NULL, "size %d", sz[i]);
        if (!ic) continue;
        ri_iconsize(ic, &w, &h, NULL, NULL);
        /* Win10 22H2: the shared OEM icons come back at the system icon size (SM_CXICON) whatever size is asked. */
        if (sh) { RI_EQ(w, GetSystemMetrics(SM_CXICON)); RI_EQ(h, GetSystemMetrics(SM_CYICON)); }
        else { RI_EQ(w, sz[i]); RI_EQ(h, sz[i]); }
        if (!sh) CHECK(U(DestroyIcon)(ic));
    }
}

TEST(resources_icons_li_oem_icon_loadimage_defaultsize)
{
    int w = 0, h = 0;
    HICON ic; int sh;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    ic = ri_oem_load(32514, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE, &sh);
    CHECK(ic != NULL);
    if (ic) {
        ri_iconsize(ic, &w, &h, NULL, NULL);
        RI_EQ(w, ri_defsize(0, 0)); RI_EQ(h, ri_defsize(0, 1));
        if (!sh) U(DestroyIcon)(ic);
    }
}

TEST(resources_icons_li_oem_icon_loadimage_shared_vs_not)
{
    HICON a, b, c;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    a = U(LoadImageW)(NULL, W_(32512), IMAGE_ICON, 32, 32, LR_SHARED);
    b = U(LoadImageW)(NULL, W_(32512), IMAGE_ICON, 32, 32, LR_SHARED);
    CHECK(a != NULL);
    RI_EQ(a, b);
    { int sh;
      c = ri_oem_load(32512, IMAGE_ICON, 32, 32, 0, &sh);
      CHECK(c != NULL);
      if (!sh) { CHECK(c != a); if (c) U(DestroyIcon)(c); }
      else t_note("unshared OEM load not available; shared handle %s the first one", c == a ? "equals" : "differs from"); }
}

TEST(resources_icons_li_oem_icon_size_matrix)
{
    static const int sizes[][2] = {{16, 16}, {24, 24}, {32, 32}, {40, 40}, {64, 64}, {16, 32}, {1, 1}, {256, 256}};
    int i;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    for (i = 0; i < 8; i++) {
        int w = 0, h = 0;
        int sh;
        HICON ic = ri_oem_load(32515, IMAGE_ICON, sizes[i][0], sizes[i][1], 0, &sh);
        CHECKF(ic != NULL, "%dx%d", sizes[i][0], sizes[i][1]);
        if (!ic) continue;
        ri_iconsize(ic, &w, &h, NULL, NULL);
        /* shared OEM icons are always the system icon size on Win10 22H2 */
        if (sh) { RI_EQ(w, GetSystemMetrics(SM_CXICON)); RI_EQ(h, GetSystemMetrics(SM_CYICON)); }
        else { RI_EQ(w, sizes[i][0]); RI_EQ(h, sizes[i][1]); }
        if (!sh) U(DestroyIcon)(ic);
    }
}

TEST(resources_icons_li_loadimage_invalid_type)
{
    REQUIRE(U(LoadImageW));
    SetLastError(0);
    CHECK(U(LoadImageW)(NULL, W_(32512), 3, 0, 0, 0) == NULL);
    CHECK(U(LoadImageW)(NULL, W_(32512), 4, 0, 0, 0) == NULL);
    CHECK(U(LoadImageW)(NULL, W_(32512), 0xFFFF, 0, 0, 0) == NULL);
    CHECK(U(LoadImageW)(NULL, W_(32512), 0xFFFFFFFF, 0, 0, 0) == NULL);
    CHECK(U(LoadImageA)(NULL, (LPCSTR)32512, 3, 0, 0, 0) == NULL);
}

TEST(resources_icons_li_loadimage_flag_bit17_ignored)
{
    /* LoadImageW masks off 0x20000 before use */
    HICON a, b;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    a = U(LoadImageW)(NULL, W_(32512), IMAGE_ICON, 32, 32, LR_SHARED);
    b = U(LoadImageW)(NULL, W_(32512), IMAGE_ICON, 32, 32, LR_SHARED | 0x20000);
    CHECK(a != NULL);
    RI_EQ(a, b);
}

TEST(resources_icons_li_loadimage_a_w_names)
{
    HICON a, w;
    REQUIRE(U(LoadImageA)); REQUIRE(U(LoadImageW));
    a = U(LoadImageA)(NULL, (LPCSTR)32513, IMAGE_ICON, 32, 32, LR_SHARED);
    w = U(LoadImageW)(NULL, W_(32513), IMAGE_ICON, 32, 32, LR_SHARED);
    CHECK(a != NULL);
    RI_EQ(a, w);
}

/* ---- OEM bitmaps ------------------------------------------------------- */

TEST(resources_icons_li_oem_bitmaps_core)
{
    static const WORD ids[] = {OBM_CLOSE, OBM_UPARROW, OBM_DNARROW, OBM_RGARROW, OBM_LFARROW, OBM_CHECK, OBM_CHECKBOXES, OBM_COMBO};
    int i;
    REQUIRE(U(LoadBitmapW));
    for (i = 0; i < 8; i++) {
        HBITMAP b = U(LoadBitmapW)(NULL, W_(ids[i]));
        BITMAP bm;
        CHECKF(b != NULL, "OBM %u", ids[i]);
        if (!b) continue;
        CHECK(ri_bmp(b, &bm));
        CHECKF(bm.bmWidth > 0 && bm.bmHeight > 0, "OBM %u", ids[i]);
        RI_EQ(GetObjectType(b), OBJ_BITMAP);
        CHECK(DeleteObject(b));
    }
}

TEST(resources_icons_li_oem_bitmaps_all_note)
{
    static const WORD ids[] = {32767, 32766, 32765, 32764, 32763, 32762, 32761, 32760, 32759, 32758, 32757, 32756, 32755,
                               32754, 32753, 32752, 32751, 32750, 32749, 32748, 32747, 32746, 32745, 32744, 32743, 32742,
                               32741, 32740, 32739, 32738, 32737, 32736, 32735, 32734};
    int i, found = 0;
    REQUIRE(U(LoadBitmapW));
    for (i = 0; i < 34; i++) {
        HBITMAP b = U(LoadBitmapW)(NULL, W_(ids[i]));
        if (b) { found++; DeleteObject(b); }
        else t_note("OBM %u not loadable", ids[i]);
    }
    t_note("%d of 34 OBM ids loadable", found);
    CHECK(found > 0);
}

TEST(resources_icons_li_oem_bitmap_a_w_parity)
{
    HBITMAP a, w; BITMAP ba, bw;
    REQUIRE(U(LoadBitmapA)); REQUIRE(U(LoadBitmapW));
    a = U(LoadBitmapA)(NULL, (LPCSTR)(ULONG_PTR)OBM_CLOSE);
    w = U(LoadBitmapW)(NULL, W_(OBM_CLOSE));
    CHECK(a && w);
    if (a && w) {
        ri_bmp(a, &ba); ri_bmp(w, &bw);
        RI_EQ(ba.bmWidth, bw.bmWidth); RI_EQ(ba.bmHeight, bw.bmHeight); RI_EQ(ba.bmBitsPixel, bw.bmBitsPixel);
    }
    if (a) DeleteObject(a);
    if (w) DeleteObject(w);
}

TEST(resources_icons_li_oem_bitmap_checkboxes_layout)
{
    HBITMAP b; BITMAP bm;
    REQUIRE(U(LoadBitmapW));
    b = U(LoadBitmapW)(NULL, W_(OBM_CHECKBOXES));
    CHECK(b != NULL);
    if (!b) return;
    ri_bmp(b, &bm);
    /* a 4 x 3 grid of equal cells */
    RI_EQ(bm.bmWidth % 4, 0);
    RI_EQ(bm.bmHeight % 3, 0);
    t_note("OBM_CHECKBOXES %dx%d", bm.bmWidth, bm.bmHeight);
    DeleteObject(b);
}

TEST(resources_icons_li_loadbitmap_missing)
{
    REQUIRE(U(LoadBitmapW)); REQUIRE(U(LoadBitmapA));
    CHECK(U(LoadBitmapW)(NULL, W_(5000)) == NULL);
    CHECK(U(LoadBitmapW)(NULL, W_(0xFFFF)) == NULL);
    CHECK(U(LoadBitmapA)(NULL, "NO_SUCH_BITMAP") == NULL);
    CHECK(U(LoadBitmapW)(GetModuleHandleA(NULL), L"NO_SUCH_BITMAP") == NULL);
    CHECK(U(LoadBitmapW)(GetModuleHandleA(NULL), W_(7)) == NULL);
}

TEST(resources_icons_li_loadbitmap_new_each_time)
{
    HBITMAP a, b;
    REQUIRE(U(LoadBitmapW));
    a = U(LoadBitmapW)(NULL, W_(OBM_CLOSE));
    b = U(LoadBitmapW)(NULL, W_(OBM_CLOSE));
    CHECK(a && b);
    t_note("LoadBitmap twice: %p %p", a, b);
    if (a) CHECK(DeleteObject(a));
    if (b && b != a) CHECK(DeleteObject(b));
}

TEST(resources_icons_li_loadimage_oem_bitmap)
{
    HBITMAP b; BITMAP bm, bm2; HBITMAP lb;
    REQUIRE(U(LoadImageW)); REQUIRE(U(LoadBitmapW));
    b = U(LoadImageW)(NULL, W_(OBM_CLOSE), IMAGE_BITMAP, 0, 0, 0);
    lb = U(LoadBitmapW)(NULL, W_(OBM_CLOSE));
    CHECK(b != NULL);
    if (b && lb) {
        ri_bmp(b, &bm); ri_bmp(lb, &bm2);
        RI_EQ(bm.bmWidth, bm2.bmWidth); RI_EQ(bm.bmHeight, bm2.bmHeight);
    }
    if (b) DeleteObject(b);
    if (lb) DeleteObject(lb);
}

TEST(resources_icons_li_loadimage_oem_bitmap_resize)
{
    HBITMAP b; BITMAP bm;
    REQUIRE(U(LoadImageW));
    b = U(LoadImageW)(NULL, W_(OBM_CLOSE), IMAGE_BITMAP, 40, 30, 0);
    CHECK(b != NULL);
    if (b) { ri_bmp(b, &bm); t_note("OBM_CLOSE resized -> %dx%d", bm.bmWidth, bm.bmHeight); DeleteObject(b); }
}

/* ---- file based ------------------------------------------------------- */

static BYTE *ri_icofile(int count, const int *sz, const int *bpp, const DWORD *col, int cursor, const int *hot, DWORD *outsz)
{
    BYTE *imgs[8]; DWORD isz[8];
    DWORD total = 6 + 16 * count, off, i;
    BYTE *f;
    for (i = 0; i < (DWORD)count; i++) { imgs[i] = ri_dib(sz[i], sz[i], bpp[i], col[i], 0, &isz[i]); total += isz[i]; }
    f = calloc(1, total);
    *(WORD *)(f + 2) = cursor ? 2 : 1; *(WORD *)(f + 4) = count;
    off = 6 + 16 * count;
    for (i = 0; i < (DWORD)count; i++) {
        BYTE *e = f + 6 + 16 * i;
        e[0] = sz[i] >= 256 ? 0 : sz[i]; e[1] = e[0];
        if (cursor) { *(WORD *)(e + 4) = hot[i * 2]; *(WORD *)(e + 6) = hot[i * 2 + 1]; }
        else { *(WORD *)(e + 4) = 1; *(WORD *)(e + 6) = bpp[i]; }
        *(DWORD *)(e + 8) = isz[i]; *(DWORD *)(e + 12) = off;
        memcpy(f + off, imgs[i], isz[i]);
        off += isz[i]; free(imgs[i]);
    }
    *outsz = total;
    return f;
}

static BYTE *ri_bmpfile(int w, int h, int bpp, DWORD rgb, int topdown, DWORD *outsz)
{
    int ncol = bpp <= 8 ? (1 << bpp) : 0;
    int stride = ((w * bpp + 31) / 32) * 4;
    DWORD hdr = 14 + 40 + ncol * 4, total = hdr + stride * h;
    BYTE *f = calloc(1, total), *p;
    BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *)(f + 14);
    int x, y;
    f[0] = 'B'; f[1] = 'M'; *(DWORD *)(f + 2) = total; *(DWORD *)(f + 10) = hdr;
    bi->biSize = 40; bi->biWidth = w; bi->biHeight = topdown ? -h : h; bi->biPlanes = 1; bi->biBitCount = bpp; bi->biClrUsed = ncol;
    if (ncol) {
        RGBQUAD *q = (RGBQUAD *)(f + 54);
        q[ncol - 1].rgbRed = (rgb >> 16) & 255; q[ncol - 1].rgbGreen = (rgb >> 8) & 255; q[ncol - 1].rgbBlue = rgb & 255;
    }
    p = f + hdr;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        BYTE *row = p + y * stride;
        if (bpp == 32) ((DWORD *)row)[x] = rgb;
        else if (bpp == 24) { row[x * 3] = rgb & 255; row[x * 3 + 1] = (rgb >> 8) & 255; row[x * 3 + 2] = (rgb >> 16) & 255; }
        else if (bpp == 8) row[x] = 255;
        else if (bpp == 4) row[x / 2] |= (x & 1) ? 0x0F : 0xF0;
        else if (bpp == 1) row[x / 8] |= 0x80 >> (x & 7);
    }
    *outsz = total;
    return f;
}

static BOOL ri_mkfile(WCHAR *path, const WCHAR *suffix, const void *data, DWORD n)
{
    ri_tmpname(path, suffix);
    return ri_writefile(path, data, n);
}

TEST(resources_icons_li_file_icon_pick_size)
{
    static const int sz[] = {16, 32, 48}, bpp[] = {32, 32, 32};
    static const DWORD col[] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF};
    DWORD n; BYTE *f = ri_icofile(3, sz, bpp, col, 0, NULL, &n);
    WCHAR path[MAX_PATH]; int i;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    CHECK(ri_mkfile(path, L".ico", f, n));
    for (i = 0; i < 3; i++) {
        int w = 0, h = 0;
        HICON ic = U(LoadImageW)(NULL, path, IMAGE_ICON, sz[i], sz[i], LR_LOADFROMFILE);
        CHECKF(ic != NULL, "size %d", sz[i]);
        if (!ic) continue;
        ri_iconsize(ic, &w, &h, NULL, NULL);
        RI_EQ(w, sz[i]); RI_EQ(h, sz[i]);
        CHECK(U(DestroyIcon)(ic));
    }
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_icon_default_and_scaled)
{
    static const int sz[] = {16, 32}, bpp[] = {24, 24};
    static const DWORD col[] = {0xFFFF0000, 0xFF00FF00};
    DWORD n; BYTE *f = ri_icofile(2, sz, bpp, col, 0, NULL, &n);
    WCHAR path[MAX_PATH]; int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    CHECK(ri_mkfile(path, L".ico", f, n));
    ic = U(LoadImageW)(NULL, path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    CHECK(ic != NULL);
    if (ic) { ri_iconsize(ic, &w, &h, NULL, NULL); RI_EQ(w, ri_defsize(0, 0)); RI_EQ(h, ri_defsize(0, 1)); U(DestroyIcon)(ic); }
    ic = U(LoadImageW)(NULL, path, IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
    CHECK(ic != NULL);
    if (ic) { ri_iconsize(ic, &w, &h, NULL, NULL); RI_EQ(w, 64); RI_EQ(h, 64); U(DestroyIcon)(ic); }
    ic = U(LoadImageW)(NULL, path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    CHECK(ic != NULL);
    if (ic) { ri_iconsize(ic, &w, &h, NULL, NULL); t_note("ico file, cx=cy=0 no flags -> %dx%d", w, h); U(DestroyIcon)(ic); }
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_icon_a_w)
{
    static const int sz[] = {32}, bpp[] = {8};
    static const DWORD col[] = {0xFF123456};
    DWORD n; BYTE *f = ri_icofile(1, sz, bpp, col, 0, NULL, &n);
    WCHAR path[MAX_PATH]; char apath[MAX_PATH];
    HICON a, w;
    REQUIRE(U(LoadImageW)); REQUIRE(U(LoadImageA)); REQUIRE(U(DestroyIcon));
    CHECK(ri_mkfile(path, L".ico", f, n));
    WideCharToMultiByte(CP_ACP, 0, path, -1, apath, MAX_PATH, NULL, NULL);
    a = U(LoadImageA)(NULL, apath, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    w = U(LoadImageW)(NULL, path, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    CHECK(a != NULL); CHECK(w != NULL);
    if (a) U(DestroyIcon)(a);
    if (w) U(DestroyIcon)(w);
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_unicode_name)
{
    static const int sz[] = {16}, bpp[] = {24};
    static const DWORD col[] = {0xFF123456};
    DWORD n; BYTE *f = ri_icofile(1, sz, bpp, col, 0, NULL, &n);
    WCHAR path[MAX_PATH], tmp[MAX_PATH];
    HICON ic;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    GetTempPathW(MAX_PATH, tmp);
    wsprintfW(path, L"%sri_\x00e9\x4e2d_%lu.ico", tmp, GetCurrentProcessId());
    if (!ri_writefile(path, f, n)) { t_skip("cannot create unicode file name"); free(f); return; }
    ic = U(LoadImageW)(NULL, path, IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
    CHECK(ic != NULL);
    if (ic) U(DestroyIcon)(ic);
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_missing_and_garbage)
{
    WCHAR path[MAX_PATH]; BYTE junk[64]; int i;
    REQUIRE(U(LoadImageW));
    ri_tmpname(path, L".ico");
    SetLastError(0);
    CHECK(U(LoadImageW)(NULL, path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE) == NULL);
    t_note("missing .ico err=%lu", GetLastError());
    CHECK(U(LoadImageW)(NULL, path, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE) == NULL);
    CHECK(U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE) == NULL);
    for (i = 0; i < 64; i++) junk[i] = (BYTE)(i * 37 + 11);
    CHECK(ri_writefile(path, junk, 64));
    CHECK(U(LoadImageW)(NULL, path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE) == NULL);
    CHECK(U(LoadImageW)(NULL, path, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE) == NULL);
    CHECK(U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE) == NULL);
    CHECK(ri_writefile(path, junk, 0));
    CHECK(U(LoadImageW)(NULL, path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE) == NULL);
    CHECK(U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE) == NULL);
    DeleteFileW(path);
}

TEST(resources_icons_li_file_truncated_ico)
{
    static const int sz[] = {32}, bpp[] = {32};
    static const DWORD col[] = {0xFF123456};
    DWORD n; BYTE *f = ri_icofile(1, sz, bpp, col, 0, NULL, &n);
    WCHAR path[MAX_PATH]; DWORD cuts[] = {1, 5, 6, 10, 22, 30, 60, 500};
    int i;
    REQUIRE(U(LoadImageW));
    for (i = 0; i < 8; i++) {
        HICON ic;
        CHECK(ri_mkfile(path, L".ico", f, cuts[i]));
        ic = U(LoadImageW)(NULL, path, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        CHECKF(ic == NULL, "truncated to %lu bytes", cuts[i]);
        if (ic) U(DestroyIcon)(ic);
        DeleteFileW(path);
    }
    free(f);
}

TEST(resources_icons_li_file_directory_path)
{
    WCHAR tmp[MAX_PATH];
    REQUIRE(U(LoadImageW));
    GetTempPathW(MAX_PATH, tmp);
    CHECK(U(LoadImageW)(NULL, tmp, IMAGE_ICON, 0, 0, LR_LOADFROMFILE) == NULL);
    CHECK(U(LoadImageW)(NULL, tmp, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE) == NULL);
}

TEST(resources_icons_li_file_wrong_type)
{
    static const int sz[] = {32}, bpp[] = {24};
    static const DWORD col[] = {0xFF123456};
    DWORD n, bn; BYTE *f = ri_icofile(1, sz, bpp, col, 0, NULL, &n);
    BYTE *bf = ri_bmpfile(8, 8, 24, 0x102030, 0, &bn);
    WCHAR pi[MAX_PATH], pb[MAX_PATH];
    HANDLE h;
    REQUIRE(U(LoadImageW));
    CHECK(ri_mkfile(pi, L".ico", f, n));
    CHECK(ri_mkfile(pb, L".bmp", bf, bn));
    h = U(LoadImageW)(NULL, pi, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    CHECK(h == NULL);
    if (h) DeleteObject(h);
    h = U(LoadImageW)(NULL, pb, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    CHECK(h == NULL);
    if (h) U(DestroyIcon)(h);
    h = U(LoadImageW)(NULL, pb, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);
    CHECK(h == NULL);
    if (h) U(DestroyIcon)(h);
    h = U(LoadImageW)(NULL, pi, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);
    t_note("ico file loaded as IMAGE_CURSOR: %p", h);
    if (h) U(DestroyIcon)(h);
    DeleteFileW(pi); DeleteFileW(pb); free(f); free(bf);
}

TEST(resources_icons_li_file_cursor_hotspot)
{
    static const int sz[] = {32}, bpp[] = {1};
    static const DWORD col[] = {0xFFFFFFFF};
    static const int hot[] = {3, 4};
    DWORD n; BYTE *f = ri_icofile(1, sz, bpp, col, 1, hot, &n);
    WCHAR path[MAX_PATH]; ICONINFO ii;
    HCURSOR c;
    REQUIRE(U(LoadImageW)); REQUIRE(U(GetIconInfo));
    CHECK(ri_mkfile(path, L".cur", f, n));
    c = U(LoadImageW)(NULL, path, IMAGE_CURSOR, 32, 32, LR_LOADFROMFILE);
    CHECK(c != NULL);
    if (c) {
        CHECK(U(GetIconInfo)(c, &ii));
        RI_EQ(ii.fIcon, FALSE); RI_EQ(ii.xHotspot, 3); RI_EQ(ii.yHotspot, 4);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        CHECK(U(DestroyCursor)(c));
    }
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_loadcursorfromfile)
{
    static const int sz[] = {32}, bpp[] = {1};
    static const DWORD col[] = {0xFFFFFFFF};
    static const int hot[] = {6, 2};
    DWORD n; BYTE *f = ri_icofile(1, sz, bpp, col, 1, hot, &n);
    WCHAR path[MAX_PATH]; char apath[MAX_PATH]; ICONINFO ii;
    HCURSOR cw, ca;
    REQUIRE(U(LoadCursorFromFileW)); REQUIRE(U(LoadCursorFromFileA)); REQUIRE(U(GetIconInfo));
    CHECK(ri_mkfile(path, L".cur", f, n));
    WideCharToMultiByte(CP_ACP, 0, path, -1, apath, MAX_PATH, NULL, NULL);
    cw = U(LoadCursorFromFileW)(path);
    ca = U(LoadCursorFromFileA)(apath);
    CHECK(cw != NULL); CHECK(ca != NULL);
    if (cw) {
        int w = 0, h = 0;
        CHECK(ri_iconsize(cw, &w, &h, NULL, NULL));
        RI_EQ(w, ri_defsize(1, 0)); RI_EQ(h, ri_defsize(1, 1));
        if (w == 32 && U(GetIconInfo)(cw, &ii)) {
            RI_EQ(ii.fIcon, FALSE); RI_EQ(ii.xHotspot, 6); RI_EQ(ii.yHotspot, 2);
            if (ii.hbmMask) DeleteObject(ii.hbmMask);
            if (ii.hbmColor) DeleteObject(ii.hbmColor);
        }
        U(DestroyCursor)(cw);
    }
    if (ca) U(DestroyCursor)(ca);
    SetLastError(0);
    DeleteFileW(path);
    CHECK(U(LoadCursorFromFileW)(path) == NULL);
    CHECK(U(LoadCursorFromFileA)(apath) == NULL);
    free(f);
}

TEST(resources_icons_li_file_bitmap_dims)
{
    static const int bpp[] = {1, 4, 8, 24, 32};
    int i;
    REQUIRE(U(LoadImageW));
    for (i = 0; i < 5; i++) {
        DWORD n; BYTE *f = ri_bmpfile(13, 7, bpp[i], 0x405060, 0, &n);
        WCHAR path[MAX_PATH]; BITMAP bm; HBITMAP b;
        CHECK(ri_mkfile(path, L".bmp", f, n));
        b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        CHECKF(b != NULL, "bpp %d", bpp[i]);
        if (b) {
            CHECK(ri_bmp(b, &bm));
            RI_EQ(bm.bmWidth, 13); RI_EQ(bm.bmHeight, 7);
            RI_EQ(GetObjectType(b), OBJ_BITMAP);
            DeleteObject(b);
        }
        DeleteFileW(path); free(f);
    }
}

TEST(resources_icons_li_file_bitmap_topdown)
{
    DWORD n; BYTE *f = ri_bmpfile(10, 6, 24, 0x405060, 1, &n);
    WCHAR path[MAX_PATH]; BITMAP bm; HBITMAP b;
    REQUIRE(U(LoadImageW));
    CHECK(ri_mkfile(path, L".bmp", f, n));
    b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    /* Win10 22H2 oracle: a top-down (negative biHeight) .bmp file is rejected by LoadImage */
    if (!b) t_note("top-down BMP file rejected, err=%lu", GetLastError());
    if (b) { ri_bmp(b, &bm); RI_EQ(bm.bmWidth, 10); RI_EQ(bm.bmHeight, 6); DeleteObject(b); }
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_bitmap_dibsection_depth)
{
    static const int bpp[] = {8, 24, 32};
    int i;
    REQUIRE(U(LoadImageW));
    for (i = 0; i < 3; i++) {
        DWORD n; BYTE *f = ri_bmpfile(9, 5, bpp[i], 0x102030, 0, &n);
        WCHAR path[MAX_PATH]; DIBSECTION ds; HBITMAP b;
        CHECK(ri_mkfile(path, L".bmp", f, n));
        b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        CHECKF(b != NULL, "bpp %d", bpp[i]);
        if (b) {
            memset(&ds, 0, sizeof ds);
            RI_EQ(GetObjectW(b, sizeof ds, &ds), sizeof ds);
            RI_EQ(ds.dsBm.bmWidth, 9); RI_EQ(ds.dsBm.bmHeight, 5);
            RI_EQ(ds.dsBmih.biBitCount, bpp[i]);
            CHECK(ds.dsBm.bmBits != NULL);
            DeleteObject(b);
        }
        DeleteFileW(path); free(f);
    }
}

TEST(resources_icons_li_file_bitmap_pixels)
{
    static const int bpp[] = {24, 32};
    int i;
    REQUIRE(U(LoadImageW));
    for (i = 0; i < 2; i++) {
        DWORD n; BYTE *f = ri_bmpfile(4, 4, bpp[i], 0x112233, 0, &n);
        WCHAR path[MAX_PATH]; DIBSECTION ds; HBITMAP b;
        CHECK(ri_mkfile(path, L".bmp", f, n));
        b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        if (b && GetObjectW(b, sizeof ds, &ds) == sizeof ds && ds.dsBm.bmBits) {
            BYTE *p = ds.dsBm.bmBits;
            DWORD px = bpp[i] == 24 ? (p[2] << 16 | p[1] << 8 | p[0]) : (((DWORD *)p)[0] & 0xFFFFFF);
            RI_EQ(px, 0x112233);
        } else CHECK(0);
        if (b) DeleteObject(b);
        DeleteFileW(path); free(f);
    }
}

TEST(resources_icons_li_file_bitmap_monochrome_flag)
{
    DWORD n; BYTE *f = ri_bmpfile(8, 8, 24, 0x808080, 0, &n);
    WCHAR path[MAX_PATH]; BITMAP bm; HBITMAP b;
    REQUIRE(U(LoadImageW));
    CHECK(ri_mkfile(path, L".bmp", f, n));
    b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_MONOCHROME);
    CHECK(b != NULL);
    if (b) { ri_bmp(b, &bm); RI_EQ(bm.bmBitsPixel, 1); RI_EQ(bm.bmPlanes, 1); DeleteObject(b); }
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_bitmap_resize)
{
    DWORD n; BYTE *f = ri_bmpfile(8, 8, 24, 0x808080, 0, &n);
    WCHAR path[MAX_PATH]; BITMAP bm; HBITMAP b;
    REQUIRE(U(LoadImageW));
    CHECK(ri_mkfile(path, L".bmp", f, n));
    b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 16, 4, LR_LOADFROMFILE);
    CHECK(b != NULL);
    if (b) { ri_bmp(b, &bm); RI_EQ(bm.bmWidth, 16); RI_EQ(bm.bmHeight, 4); DeleteObject(b); }
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_bitmap_misc_flags)
{
    static const UINT fl[] = {LR_LOADMAP3DCOLORS, LR_LOADTRANSPARENT, LR_LOADTRANSPARENT | LR_LOADMAP3DCOLORS, LR_VGACOLOR, LR_DEFAULTCOLOR};
    DWORD n; BYTE *f = ri_bmpfile(8, 8, 8, 0x808080, 0, &n);
    WCHAR path[MAX_PATH]; int i;
    REQUIRE(U(LoadImageW));
    CHECK(ri_mkfile(path, L".bmp", f, n));
    for (i = 0; i < 5; i++) {
        HBITMAP b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | fl[i]);
        CHECKF(b != NULL, "flags 0x%x", fl[i]);
        if (b) DeleteObject(b);
    }
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_bitmap_sharing_ignored)
{
    DWORD n; BYTE *f = ri_bmpfile(8, 8, 24, 0x808080, 0, &n);
    WCHAR path[MAX_PATH]; HBITMAP a, b;
    REQUIRE(U(LoadImageW));
    CHECK(ri_mkfile(path, L".bmp", f, n));
    a = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_SHARED);
    b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_SHARED);
    CHECK(a != NULL && b != NULL);
    t_note("LR_LOADFROMFILE|LR_SHARED bitmap handles %p %p", a, b);
    if (a) DeleteObject(a);
    if (b && b != a) DeleteObject(b);
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_leak_icons)
{
    static const int sz[] = {16, 32}, bpp[] = {24, 24};
    static const DWORD col[] = {0xFFFF0000, 0xFF00FF00};
    DWORD n; BYTE *f = ri_icofile(2, sz, bpp, col, 0, NULL, &n);
    WCHAR path[MAX_PATH]; DWORD u0, g0, u1, g1; int i;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon)); REQUIRE(U(GetGuiResources));
    CHECK(ri_mkfile(path, L".ico", f, n));
    u0 = ri_gui(GR_USEROBJECTS); g0 = ri_gui(GR_GDIOBJECTS);
    for (i = 0; i < 100; i++) {
        HICON ic = U(LoadImageW)(NULL, path, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        if (!ic) { CHECK(0); break; }
        U(DestroyIcon)(ic);
    }
    u1 = ri_gui(GR_USEROBJECTS); g1 = ri_gui(GR_GDIOBJECTS);
    RI_EQ(u1, u0); RI_EQ(g1, g0);
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_leak_bitmaps)
{
    DWORD n; BYTE *f = ri_bmpfile(16, 16, 24, 0x808080, 0, &n);
    WCHAR path[MAX_PATH]; DWORD g0, g1; int i;
    REQUIRE(U(LoadImageW)); REQUIRE(U(GetGuiResources));
    CHECK(ri_mkfile(path, L".bmp", f, n));
    g0 = ri_gui(GR_GDIOBJECTS);
    for (i = 0; i < 100; i++) {
        HBITMAP b = U(LoadImageW)(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        if (!b) { CHECK(0); break; }
        DeleteObject(b);
    }
    g1 = ri_gui(GR_GDIOBJECTS);
    RI_EQ(g1, g0);
    DeleteFileW(path); free(f);
}

TEST(resources_icons_li_file_alpha_icon)
{
    static const int sz[] = {32}, bpp[] = {32};
    static const DWORD col[] = {0x80FF0000};
    DWORD n; BYTE *f = ri_icofile(1, sz, bpp, col, 0, NULL, &n);
    WCHAR path[MAX_PATH]; HICON ic; int w = 0, h = 0, cb = 0;
    REQUIRE(U(LoadImageW)); REQUIRE(U(DestroyIcon));
    CHECK(ri_mkfile(path, L".ico", f, n));
    ic = U(LoadImageW)(NULL, path, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    CHECK(ic != NULL);
    if (ic) {
        CHECK(ri_iconsize(ic, &w, &h, &cb, NULL));
        RI_EQ(w, 32); RI_EQ(cb, 32);
        U(DestroyIcon)(ic);
    }
    DeleteFileW(path); free(f);
}
