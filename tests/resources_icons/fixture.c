#include "ri.h"

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

HMODULE ri_fixture(void)
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

