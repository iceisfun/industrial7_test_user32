/* Shared helpers for the resources_icons area (all static). */
#ifndef RI_H
#define RI_H
#include "t.h"
#include <stdlib.h>
#include <wchar.h>

#define RI_STR_BASE 1000

/* OBM_* ids are hidden behind OEMRESOURCE in the SDK header */
#ifndef OBM_CLOSE
#define OBM_CLOSE 32754
#endif
#ifndef OCR_NORMAL
#define OCR_NORMAL 32512
#define OCR_UP 32516
#endif
#ifndef OBM_UPARROW

#define OBM_UPARROW 32753
#define OBM_DNARROW 32752
#define OBM_RGARROW 32751
#define OBM_LFARROW 32750
#define OBM_COMBO 32738
#define OBM_CHECK 32760
#define OBM_CHECKBOXES 32759
#endif

/* ---- in-memory icon / cursor DIB builders ---------------------------- */
/* Build the DIB part of an icon image (BITMAPINFOHEADER with doubled height,
 * palette, XOR (colour) bits, AND mask). Solid colour 0xAARRGGBB.
 * maskbit: 0 => opaque, 1 => fully transparent. Returns malloc'd buffer. */
static BYTE *ri_dib(int w, int h, int bpp, DWORD argb, int maskbit, DWORD *outsize)
{
    int ncol = bpp <= 8 ? (1 << bpp) : 0;
    int stride = ((w * bpp + 31) / 32) * 4;
    int mstride = ((w + 31) / 32) * 4;
    DWORD size = sizeof(BITMAPINFOHEADER) + ncol * 4 + stride * h + mstride * h;
    BYTE *b = calloc(1, size);
    BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *)b;
    BYTE *p;
    int x, y;
    bi->biSize = sizeof *bi; bi->biWidth = w; bi->biHeight = h * 2;
    bi->biPlanes = 1; bi->biBitCount = bpp; bi->biCompression = BI_RGB;
    bi->biClrUsed = ncol;
    p = b + sizeof *bi;
    if (ncol) {
        RGBQUAD *q = (RGBQUAD *)p;
        q[ncol - 1].rgbRed = (argb >> 16) & 255; q[ncol - 1].rgbGreen = (argb >> 8) & 255; q[ncol - 1].rgbBlue = argb & 255;
        p += ncol * 4;
    }
    for (y = 0; y < h; y++) {
        BYTE *row = p + y * stride;
        for (x = 0; x < w; x++) {
            if (bpp == 32) { ((DWORD *)row)[x] = argb; }
            else if (bpp == 24) { row[x * 3] = argb & 255; row[x * 3 + 1] = (argb >> 8) & 255; row[x * 3 + 2] = (argb >> 16) & 255; }
            else if (bpp == 8) row[x] = 255;
            else if (bpp == 4) { if (x & 1) row[x / 2] |= 0x0F; else row[x / 2] |= 0xF0; }
            else if (bpp == 1) row[x / 8] |= 0x80 >> (x & 7);
        }
    }
    p += stride * h;
    memset(p, maskbit ? 0xFF : 0x00, mstride * h);
    *outsize = size;
    return b;
}

/* Cursor resource data: hotspot (2 WORDs) + DIB */
static BYTE *ri_curdata(int w, int h, int bpp, DWORD argb, int hx, int hy, DWORD *outsize)
{
    DWORD ds;
    BYTE *d = ri_dib(w, h, bpp, argb, 0, &ds);
    BYTE *r = malloc(ds + 4);
    ((WORD *)r)[0] = hx; ((WORD *)r)[1] = hy;
    memcpy(r + 4, d, ds);
    free(d);
    *outsize = ds + 4;
    return r;
}

/* single-evaluation equality (the harness EXPECT_EQ evaluates its arguments twice) */
#define RI_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)

/* ---- bitmap info ------------------------------------------------------ */
static BOOL ri_bmp(HBITMAP hb, BITMAP *bm)
{
    memset(bm, 0, sizeof *bm);
    return GetObjectW(hb, sizeof *bm, bm) == sizeof *bm;
}

static BOOL ri_iconsize(HICON ic, int *w, int *h, int *colorbpp, BOOL *iscursor)
{
    ICONINFO ii; BITMAP bm;
    BOOL (WINAPI *gi)(HICON, PICONINFO) = (void *)t_proc("GetIconInfo");
    if (!gi || !gi(ic, &ii)) return FALSE;
    if (iscursor) *iscursor = !ii.fIcon;
    if (colorbpp) *colorbpp = ii.hbmColor ? 0 : 1;
    if (ii.hbmColor && ri_bmp(ii.hbmColor, &bm)) { *w = bm.bmWidth; *h = bm.bmHeight; if (colorbpp) *colorbpp = bm.bmBitsPixel; }
    else if (ri_bmp(ii.hbmMask, &bm)) { *w = bm.bmWidth; *h = bm.bmHeight / 2; }
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    return TRUE;
}

/* LR_DEFAULTSIZE metrics are taken at the system DPI */
static int ri_defsize(int cursor, int height)
{
    UINT (WINAPI *gdfs)(void) = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForSystem");
    int (WINAPI *gsm)(int, UINT) = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "GetSystemMetricsForDpi");
    int idx = cursor ? (height ? SM_CYCURSOR : SM_CXCURSOR) : (height ? SM_CYICON : SM_CXICON);
    if (gdfs && gsm) return gsm(idx, gdfs());
    return GetSystemMetrics(idx);
}

static DWORD ri_gui(DWORD flag)
{
    DWORD (WINAPI *g)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    return g ? g(GetCurrentProcess(), flag) : 0;
}

/* ---- resource fixture: a copy of this exe with resources added -------- */
/* Provides string table, icon/cursor groups and a bitmap. Loaded as datafile. */
#define RI_GRP_ICON   200   /* 16,32,48 32bpp icons (ids 1..3) */
#define RI_GRP_ICON8  201   /* single 32x32 8bpp */
#define RI_GRP_ICON4  202   /* single 32x32 4bpp */
#define RI_GRP_ICON1  203   /* single 32x32 1bpp */
#define RI_GRP_ICON24 204   /* single 24x24 24bpp */
#define RI_GRP_CUR    210   /* single 32x32 cursor hotspot 5,7 */
#define RI_BMP_ID     220   /* 8x4 24bpp bitmap */
#define RI_RT_ICON_BASE 10  /* RT_ICON ids */
#define RI_RT_CUR_BASE  30

HMODULE ri_fixture(void); /* fixture.c */

#define REQUIRE_FIX(var) HMODULE var = ri_fixture(); if (!var) { t_skip("resource fixture unavailable"); return; }

static void ri_tmpname(WCHAR *out, const WCHAR *suffix)
{
    WCHAR tmp[MAX_PATH]; static int n;
    GetTempPathW(MAX_PATH, tmp);
    wsprintfW(out, L"%sri_%lu_%d%s", tmp, GetCurrentProcessId(), n++, suffix);
}
static BOOL ri_writefile(const WCHAR *path, const void *data, DWORD sz)
{
    DWORD w; HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    BOOL ok;
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    ok = WriteFile(f, data, sz, &w, NULL) && w == sz;
    CloseHandle(f);
    return ok;
}
#endif
