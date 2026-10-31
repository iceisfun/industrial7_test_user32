#include "ri.h"

/* Additional checks driven by the user32 argument validation paths. */

TEST(resources_icons_x_cifrex_null_and_short)
{
    DWORD n; BYTE *d;
    REQUIRE(U(CreateIconFromResourceEx));
    d = ri_dib(16, 16, 8, 0xFF112233, 0, &n);
    RI_EQ(U(CreateIconFromResourceEx)(NULL, n, TRUE, 0x30000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(NULL, 0, TRUE, 0x30000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, 0, TRUE, 0x30000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, 1, FALSE, 0x30000, 0, 0, 0), NULL);
    RI_EQ(U(CreateIconFromResourceEx)(d, 0x27, FALSE, 0x30000, 0, 0, 0), NULL);
    free(d);
}

TEST(resources_icons_x_cifrex_version_bounds)
{
    /* accepted range is [0x20000, 0x30000] */
    static const DWORD bad[] = {0, 0x1FFFF, 0x30001, 0x40000, 0x80000000, 0xFFFFFFFF};
    static const DWORD good[] = {0x30000, 0x20000};
    DWORD n; BYTE *d; int i;
    REQUIRE(U(CreateIconFromResourceEx)); REQUIRE(U(DestroyIcon));
    d = ri_dib(16, 16, 4, 0xFF445566, 0, &n);
    for (i = 0; i < 6; i++) {
        HICON h = U(CreateIconFromResourceEx)(d, n, TRUE, bad[i], 0, 0, 0);
        CHECKF(h == NULL, "ver 0x%lx", bad[i]);
        if (h) U(DestroyIcon)(h);
    }
    for (i = 0; i < 2; i++) {
        HICON h = U(CreateIconFromResourceEx)(d, n, TRUE, good[i], 0, 0, 0);
        CHECKF(h != NULL, "ver 0x%lx", good[i]);
        if (h) U(DestroyIcon)(h);
    }
    free(d);
}

TEST(resources_icons_x_copyimage_flag_mask_boundaries)
{
    /* flags & 0xfffe0700 are rejected with ERROR_INVALID_PARAMETER; others are not */
    static const UINT rej[] = {0x100, 0x200, 0x400, 0x20000, 0x40000, 0x80000000};
    static const UINT acc[] = {0, 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x1000, 0x2000, 0x4000, 0x8000, 0x10000};
    HICON src; HBITMAP m, c; ICONINFO ii; int i;
    REQUIRE(U(CopyImage)); REQUIRE(U(CreateIconIndirect)); REQUIRE(U(DestroyIcon));
    m = CreateBitmap(16, 16, 1, 1, NULL); c = CreateBitmap(16, 16, 1, 32, NULL);
    ii.fIcon = TRUE; ii.xHotspot = ii.yHotspot = 0; ii.hbmMask = m; ii.hbmColor = c;
    src = U(CreateIconIndirect)(&ii);
    CHECK(src != NULL);
    if (src) {
        for (i = 0; i < 6; i++) {
            SetLastError(0);
            CHECKF(U(CopyImage)(src, IMAGE_ICON, 0, 0, rej[i]) == NULL, "rej 0x%x", rej[i]);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "rej 0x%x err=%lu", rej[i], GetLastError());
        }
        for (i = 0; i < 14; i++) {
            HICON cp;
            if (acc[i] & (LR_COPYDELETEORG | LR_COPYRETURNORG)) continue; /* covered elsewhere */
            SetLastError(0);
            cp = U(CopyImage)(src, IMAGE_ICON, 0, 0, acc[i]);
            if (cp) U(DestroyIcon)(cp);
            else CHECKF(GetLastError() != ERROR_INVALID_PARAMETER, "acc 0x%x rejected", acc[i]);
        }
        U(DestroyIcon)(src);
    }
    DeleteObject(m); DeleteObject(c);
}

TEST(resources_icons_x_loadstring_ab_parity_all)
{
    static const int ids[] = {1000, 1003, 1008};
    int i;
    REQUIRE(U(LoadStringA)); REQUIRE(U(LoadStringW));
    REQUIRE_FIX(m);
    for (i = 0; i < 3; i++) {
        char a[64]; WCHAR w[64]; int na, nw;
        na = U(LoadStringA)(m, ids[i], a, 64);
        nw = U(LoadStringW)(m, ids[i], w, 64);
        CHECKF(na == nw && na > 0, "id %d a=%d w=%d", ids[i], na, nw);
        CHECKF(a[na] == 0 && w[nw] == 0, "id %d terminators", ids[i]);
    }
}

TEST(resources_icons_x_destroyicon_cursor_roundtrip_counts)
{
    DWORD u0, u1; int i;
    BYTE a[128], x[128];
    REQUIRE(U(CreateCursor)); REQUIRE(U(DestroyCursor)); REQUIRE(U(GetGuiResources));
    memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
    u0 = ri_gui(GR_USEROBJECTS);
    for (i = 0; i < 200; i++) {
        HCURSOR c = U(CreateCursor)(NULL, 3, 4, 32, 32, a, x);
        if (!c) { CHECK(0); break; }
        CHECK(U(DestroyCursor)(c));
        SetLastError(0);
        CHECK(!U(DestroyCursor)(c));
    }
    u1 = ri_gui(GR_USEROBJECTS);
    CHECKF(u1 <= u0, "user objects %lu -> %lu", u0, u1);
}

TEST(resources_icons_x_cii_requires_valid_mask)
{
    ICONINFO ii; HICON h;
    REQUIRE(U(CreateIconIndirect));
    memset(&ii, 0, sizeof ii);
    ii.fIcon = TRUE;
    /* GetObject on hbmMask fails -> NULL */
    h = U(CreateIconIndirect)(&ii);
    CHECK(h == NULL);
    if (h) U(DestroyIcon)(h);
    ii.hbmMask = (HBITMAP)(ULONG_PTR)0x1234;   /* not a GDI object */
    h = U(CreateIconIndirect)(&ii);
    CHECK(h == NULL);
    if (h) U(DestroyIcon)(h);
}
