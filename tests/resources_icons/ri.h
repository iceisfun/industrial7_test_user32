/* Shared helpers for the resources_icons area (all static). */
#ifndef RI_H
#define RI_H
#include "t.h"
#include <stdlib.h>
#include <wchar.h>

#define RI_STR_BASE 1000

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

static HMODULE g_fix;
static WCHAR g_fixpath[MAX_PATH];
static int g_fixtried;

static void ri_fix_cleanup(void)
{
    if (g_fix) FreeLibrary(g_fix);
    g_fix = NULL;
    if (g_fixpath[0]) DeleteFileW(g_fixpath);
}


static BOOL ri_upd(HANDLE h, LPCSTR type, WORD id, void *data, DWORD sz)
{
    return UpdateResourceA(h, type, MAKEINTRESOURCEA(id), 0, data, sz);
}

static const WCHAR ri_s0[] = L"Hello";
static const WCHAR ri_s2[] = L"Uni\x00e9\x4e2d\x6587!";

static HMODULE ri_fixture(void)
{
    WCHAR tmp[MAX_PATH], exe[MAX_PATH];
    HANDLE h;
    int i;
    if (g_fixtried) return g_fix;
    g_fixtried = 1;
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    GetTempPathW(MAX_PATH, tmp);
    wsprintfW(g_fixpath, L"%sri_fix_%lu.exe", tmp, GetCurrentProcessId());
    if (!CopyFileW(exe, g_fixpath, FALSE)) { g_fixpath[0] = 0; return NULL; }
    atexit(ri_fix_cleanup);
    h = BeginUpdateResourceW(g_fixpath, FALSE);
    if (!h) return NULL;
    {
        /* block = id/16 + 1: ids 1000..1007 are in block 63, ids 1008.. in block 64. */
        WORD blk63[2048]; WORD *p = blk63; int n;
        WORD blk64[2048]; WORD *q = blk64;
        WCHAR longs[301];
        for (i = 0; i < 300; i++) longs[i] = L'a' + (i % 26);
        longs[300] = 0;
        memset(blk63, 0, sizeof blk63); memset(blk64, 0, sizeof blk64);
        /* 992..999 empty */
        for (i = 992; i < 1000; i++) *p++ = 0;
        /* 1000 Hello */
        n = 5; *p++ = n; memcpy(p, ri_s0, n * 2); p += n;
        /* 1001 empty */
        *p++ = 0;
        /* 1002 unicode */
        n = 7; *p++ = n; memcpy(p, ri_s2, n * 2); p += n;
        /* 1003 "A" */
        *p++ = 1; *p++ = L'A';
        /* 1004 300 chars */
        *p++ = 300; memcpy(p, longs, 600); p += 300;
        /* 1005 embedded NUL: "ab\0cd" length 5 */
        *p++ = 5; *p++ = L'a'; *p++ = L'b'; *p++ = 0; *p++ = L'c'; *p++ = L'd';
        /* 1006,1007 empty */
        *p++ = 0; *p++ = 0;
        ri_upd(h, RT_STRING, 63, blk63, (DWORD)((BYTE *)p - (BYTE *)blk63));
        /* block 64 (1008..1023): 1008 = "Z" */
        *q++ = 1; *q++ = L'Z';
        for (i = 1009; i < 1024; i++) *q++ = 0;
        ri_upd(h, RT_STRING, 64, blk64, (DWORD)((BYTE *)q - (BYTE *)blk64));
    }
    {
        static const int sz[3] = {16, 32, 48};
        static const DWORD col[3] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF};
        BYTE grp[6 + 14 * 3]; BYTE *g = grp;
        *(WORD *)g = 0; *(WORD *)(g + 2) = 1; *(WORD *)(g + 4) = 3; g += 6;
        for (i = 0; i < 3; i++) {
            DWORD ds; BYTE *d = ri_dib(sz[i], sz[i], 32, col[i], 0, &ds);
            ri_upd(h, RT_ICON, RI_RT_ICON_BASE + i, d, ds);
            g[0] = sz[i]; g[1] = sz[i]; g[2] = 0; g[3] = 0; *(WORD *)(g + 4) = 1; *(WORD *)(g + 6) = 32;
            *(DWORD *)(g + 8) = ds; *(WORD *)(g + 12) = RI_RT_ICON_BASE + i; g += 14;
            free(d);
        }
        ri_upd(h, RT_GROUP_ICON, RI_GRP_ICON, grp, sizeof grp);
    }
    {
        static const struct { int grp, id, w, bpp; } one[] = {
            {RI_GRP_ICON8, 20, 32, 8}, {RI_GRP_ICON4, 21, 32, 4}, {RI_GRP_ICON1, 22, 32, 1}, {RI_GRP_ICON24, 23, 24, 24} };
        for (i = 0; i < 4; i++) {
            DWORD ds; BYTE *d = ri_dib(one[i].w, one[i].w, one[i].bpp, 0xFFC08040, 0, &ds);
            BYTE grp[6 + 14]; BYTE *g = grp;
            *(WORD *)g = 0; *(WORD *)(g + 2) = 1; *(WORD *)(g + 4) = 1; g += 6;
            g[0] = one[i].w; g[1] = one[i].w; g[2] = 0; g[3] = 0; *(WORD *)(g + 4) = 1; *(WORD *)(g + 6) = one[i].bpp;
            *(DWORD *)(g + 8) = ds; *(WORD *)(g + 12) = one[i].id;
            ri_upd(h, RT_ICON, one[i].id, d, ds);
            ri_upd(h, RT_GROUP_ICON, one[i].grp, grp, sizeof grp);
            free(d);
        }
    }
    {
        DWORD ds; BYTE *d = ri_curdata(32, 32, 1, 0xFFFFFFFF, 5, 7, &ds);
        BYTE grp[6 + 14]; BYTE *g = grp;
        *(WORD *)g = 0; *(WORD *)(g + 2) = 2; *(WORD *)(g + 4) = 1; g += 6;
        *(WORD *)g = 32; *(WORD *)(g + 2) = 64; *(WORD *)(g + 4) = 1; *(WORD *)(g + 6) = 1;
        *(DWORD *)(g + 8) = ds; *(WORD *)(g + 12) = RI_RT_CUR_BASE;
        ri_upd(h, RT_CURSOR, RI_RT_CUR_BASE, d, ds);
        ri_upd(h, RT_GROUP_CURSOR, RI_GRP_CUR, grp, sizeof grp);
        free(d);
    }
    {
        BYTE bmp[sizeof(BITMAPINFOHEADER) + 12 * 4]; BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *)bmp;
        memset(bmp, 0, sizeof bmp);
        bi->biSize = sizeof *bi; bi->biWidth = 8; bi->biHeight = 4; bi->biPlanes = 1; bi->biBitCount = 24;
        for (i = 0; i < 4; i++) memset(bmp + sizeof *bi + i * 24, 0x80, 24);
        ri_upd(h, RT_BITMAP, RI_BMP_ID, bmp, sizeof(BITMAPINFOHEADER) + 4 * 24);
    }
    if (!EndUpdateResourceW(h, FALSE)) return NULL;
    g_fix = LoadLibraryExW(g_fixpath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    return g_fix;
}

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
