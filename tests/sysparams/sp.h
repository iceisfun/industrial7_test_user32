/* Shared helpers for the sysparams area (header-only, static). */
#ifndef SP_H
#define SP_H
#include "t.h"

#ifndef SPI_GETCARETTIMEOUT
#define SPI_GETCARETTIMEOUT 0x2022
#define SPI_SETCARETTIMEOUT 0x2023
#endif
#ifndef SPI_GETHANDEDNESS
#define SPI_GETHANDEDNESS 0x2024
#define SPI_SETHANDEDNESS 0x2025
#endif
#define SENT32 0xCDCDCDCDu

static BOOL sW(UINT a, UINT p, PVOID v, UINT f) {
    BOOL (WINAPI *fn)(UINT, UINT, PVOID, UINT) = (void *)t_proc("SystemParametersInfoW");
    return fn ? fn(a, p, v, f) : FALSE;
}
static BOOL sA(UINT a, UINT p, PVOID v, UINT f) {
    BOOL (WINAPI *fn)(UINT, UINT, PVOID, UINT) = (void *)t_proc("SystemParametersInfoA");
    return fn ? fn(a, p, v, f) : FALSE;
}
static int gsm(int i) {
    int (WINAPI *fn)(int) = (void *)t_proc("GetSystemMetrics");
    return fn ? fn(i) : 0;
}
static int gsm_dpi(int i, UINT dpi) {
    int (WINAPI *fn)(int, UINT) = (void *)t_proc("GetSystemMetricsForDpi");
    return fn ? fn(i, dpi) : 0;
}

/* Read a BOOL/UINT style value via the W entry point. Returns FALSE on failure. */
static BOOL sp_get_u32(UINT spi, UINT *out) {
    UINT v = SENT32;
    BOOL r = sW(spi, 0, &v, 0);
    *out = v;
    return r;
}

/* Common check for a getter that fills a single 32-bit value. */
static void check_get_u32(UINT spi, const char *name, UINT lo, UINT hi, int must) {
    UINT w = SENT32, a = SENT32, w2 = SENT32;
    BOOL rw, ra, rw2;
    SetLastError(0xdeadbeef);
    rw = sW(spi, 0, &w, 0);
    if (!rw) {
        if (must) CHECKF(0, "%s: SPI get failed, err=%lu", name, GetLastError());
        else t_note("%s: get unsupported (err=%lu)", name, GetLastError());
        return;
    }
    CHECKF(w != SENT32, "%s: value not written", name);
    CHECKF(w >= lo && w <= hi, "%s: value %u outside [%u,%u]", name, w, lo, hi);
    ra = sA(spi, 0, &a, 0);
    CHECKF(ra, "%s: ANSI get failed", name);
    CHECKF(a == w, "%s: ANSI %u != Unicode %u", name, a, w);
    rw2 = sW(spi, 0, &w2, 0);
    CHECKF(rw2 && w2 == w, "%s: unstable across calls (%u vs %u)", name, w, w2);
}
static void check_get_bool(UINT spi, const char *name, int must) {
    check_get_u32(spi, name, 0, 1, must);
}
#define GETB(tn, spi, must) TEST(spget_bool_##tn) { REQUIRE(U(SystemParametersInfoW)); REQUIRE(U(SystemParametersInfoA)); check_get_bool(spi, #tn, must); }
#define GETU(tn, spi, lo, hi, must) TEST(spget_u32_##tn) { REQUIRE(U(SystemParametersInfoW)); REQUIRE(U(SystemParametersInfoA)); check_get_u32(spi, #tn, lo, hi, must); }

/* Set (uiParam style) with fWinIni=0, verify, restore. */
static void roundtrip_u32(UINT getspi, UINT setspi, BOOL via_param, UINT newval, const char *name, int must_set) {
    UINT orig, cur;
    BOOL r;
    if (!sp_get_u32(getspi, &orig)) { t_note("%s: get failed, skipping", name); return; }
    SetLastError(0xdeadbeef);
    r = via_param ? sW(setspi, newval, NULL, 0) : sW(setspi, 0, (PVOID)(ULONG_PTR)newval, 0);
    if (!r) {
        if (must_set) CHECKF(0, "%s: set(%u) failed err=%lu", name, newval, GetLastError());
        else t_note("%s: set(%u) failed err=%lu", name, newval, GetLastError());
    } else {
        cur = SENT32;
        CHECKF(sp_get_u32(getspi, &cur), "%s: get after set failed", name);
        CHECKF(cur == newval, "%s: after set got %u want %u", name, cur, newval);
    }
    /* restore */
    r = via_param ? sW(setspi, orig, NULL, 0) : sW(setspi, 0, (PVOID)(ULONG_PTR)orig, 0);
    CHECKF(r, "%s: restore failed err=%lu", name, GetLastError());
    cur = SENT32;
    sp_get_u32(getspi, &cur);
    CHECKF(cur == orig, "%s: not restored: %u vs %u", name, cur, orig);
}
#endif
