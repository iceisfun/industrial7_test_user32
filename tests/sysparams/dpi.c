/* SystemParametersInfoForDpi, DPI awareness queries and SetProcessDPIAware */
#include "sp.h"

#define SFD U(SystemParametersInfoForDpi)
static const UINT fd_dpis[] = { 96, 120, 144, 168, 192, 240, 288, 384 };

static BOOL ncm_dpi(UINT dpi, NONCLIENTMETRICSW *n) {
    memset(n, 0, sizeof *n); n->cbSize = sizeof *n;
    return SFD(SPI_GETNONCLIENTMETRICS, sizeof *n, n, 0, dpi);
}

TEST(spdpi_ncm_all_dpis) {
    NONCLIENTMETRICSW n; unsigned i;
    REQUIRE(SFD);
    for (i = 0; i < sizeof fd_dpis / sizeof *fd_dpis; i++) {
        BOOL r = ncm_dpi(fd_dpis[i], &n);
        CHECKF(r, "dpi %u: err=%lu", fd_dpis[i], GetLastError());
        if (!r) continue;
        CHECKF(n.cbSize == sizeof n, "dpi %u cbSize", fd_dpis[i]);
        CHECKF(n.iScrollWidth > 0, "dpi %u scroll", fd_dpis[i]);
        CHECKF(n.iCaptionHeight > 0, "dpi %u caption", fd_dpis[i]);
        CHECKF(n.iMenuHeight > 0, "dpi %u menu", fd_dpis[i]);
        CHECKF(n.lfMessageFont.lfFaceName[0], "dpi %u message font", fd_dpis[i]);
    }
}
TEST(spdpi_ncm_monotonic) {
    NONCLIENTMETRICSW n; unsigned i; int ps = -1, pc = -1, pm = -1, ph = -1, pb = -1;
    REQUIRE(SFD);
    for (i = 0; i < sizeof fd_dpis / sizeof *fd_dpis; i++) {
        int fh;
        if (!ncm_dpi(fd_dpis[i], &n)) { CHECK(0); continue; }
        fh = n.lfMessageFont.lfHeight < 0 ? -n.lfMessageFont.lfHeight : n.lfMessageFont.lfHeight;
        CHECKF(n.iScrollWidth >= ps, "scroll width not monotonic at %u", fd_dpis[i]);
        CHECKF(n.iCaptionHeight >= pc, "caption height not monotonic at %u", fd_dpis[i]);
        CHECKF(n.iMenuHeight >= pm, "menu height not monotonic at %u", fd_dpis[i]);
        CHECKF(fh >= ph, "font height not monotonic at %u", fd_dpis[i]);
        CHECKF(n.iBorderWidth >= pb, "border not monotonic at %u", fd_dpis[i]);
        ps = n.iScrollWidth; pc = n.iCaptionHeight; pm = n.iMenuHeight; ph = fh; pb = n.iBorderWidth;
    }
}
TEST(spdpi_ncm_facenames_dpi_independent) {
    NONCLIENTMETRICSW a, b;
    REQUIRE(SFD);
    CHECK(ncm_dpi(96, &a)); CHECK(ncm_dpi(192, &b));
    CHECK(!wcscmp(a.lfCaptionFont.lfFaceName, b.lfCaptionFont.lfFaceName));
    CHECK(!wcscmp(a.lfMenuFont.lfFaceName, b.lfMenuFont.lfFaceName));
    CHECK(!wcscmp(a.lfStatusFont.lfFaceName, b.lfStatusFont.lfFaceName));
    CHECK(!wcscmp(a.lfMessageFont.lfFaceName, b.lfMessageFont.lfFaceName));
    CHECK(!wcscmp(a.lfSmCaptionFont.lfFaceName, b.lfSmCaptionFont.lfFaceName));
    CHECK(a.lfMenuFont.lfWeight == b.lfMenuFont.lfWeight);
    CHECK(a.lfMenuFont.lfCharSet == b.lfMenuFont.lfCharSet);
}
TEST(spdpi_ncm_font_height_doubles) {
    NONCLIENTMETRICSW a, b; int ha, hb;
    REQUIRE(SFD);
    CHECK(ncm_dpi(96, &a)); CHECK(ncm_dpi(192, &b));
    ha = a.lfMessageFont.lfHeight; hb = b.lfMessageFont.lfHeight;
    CHECK((ha < 0) == (hb < 0));
    ha = ha < 0 ? -ha : ha; hb = hb < 0 ? -hb : hb;
    CHECKF(hb >= 2 * ha - 2 && hb <= 2 * ha + 2, "font heights %d -> %d", ha, hb);
}
TEST(spdpi_ncm_vs_metrics_at_96) {
    NONCLIENTMETRICSW n;
    REQUIRE(SFD); REQUIRE(U(GetSystemMetricsForDpi));
    CHECK(ncm_dpi(96, &n));
    CHECK(n.iScrollWidth == gsm_dpi(SM_CXVSCROLL, 96));
    CHECK(n.iScrollHeight == gsm_dpi(SM_CYHSCROLL, 96));
}
TEST(spdpi_ncm_vs_metrics_other_dpis) {
    NONCLIENTMETRICSW n; unsigned i;
    REQUIRE(SFD); REQUIRE(U(GetSystemMetricsForDpi));
    for (i = 1; i < sizeof fd_dpis / sizeof *fd_dpis; i++) {
        if (!ncm_dpi(fd_dpis[i], &n)) continue;
        if (n.iScrollWidth != gsm_dpi(SM_CXVSCROLL, fd_dpis[i]))
            t_note("dpi %u: ncm scroll %d vs metric %d", fd_dpis[i], n.iScrollWidth, gsm_dpi(SM_CXVSCROLL, fd_dpis[i]));
        CHECKF(n.iScrollWidth == gsm_dpi(SM_CYHSCROLL, fd_dpis[i]) || n.iScrollWidth == gsm_dpi(SM_CXVSCROLL, fd_dpis[i]),
               "dpi %u: scroll %d", fd_dpis[i], n.iScrollWidth);
    }
}
TEST(spdpi_ncm_old_size) {
    NONCLIENTMETRICSW n; BOOL r;
    REQUIRE(SFD);
    memset(&n, 0, sizeof n);
    n.cbSize = offsetof(NONCLIENTMETRICSW, iPaddedBorderWidth);
    r = SFD(SPI_GETNONCLIENTMETRICS, n.cbSize, &n, 0, 96);
    t_note("ForDpi ncm pre-Vista size -> %d err=%lu", r, GetLastError());
}
TEST(spdpi_ncm_bad_size) {
    NONCLIENTMETRICSW n; BOOL r;
    UINT sz[] = { 0, 1, sizeof n - 1, sizeof n + 1 };
    unsigned i;
    REQUIRE(SFD);
    for (i = 0; i < 4; i++) {
        memset(&n, 0, sizeof n); n.cbSize = sz[i];
        SetLastError(0xdeadbeef);
        r = SFD(SPI_GETNONCLIENTMETRICS, sz[i], &n, 0, 96);
        t_note("ForDpi ncm size %u -> %d err=%lu", sz[i], r, GetLastError());
    }
}
TEST(spdpi_iconmetrics) {
    ICONMETRICSW a, b; unsigned i;
    REQUIRE(SFD);
    for (i = 0; i < sizeof fd_dpis / sizeof *fd_dpis; i++) {
        memset(&a, 0, sizeof a); a.cbSize = sizeof a;
        CHECKF(SFD(SPI_GETICONMETRICS, sizeof a, &a, 0, fd_dpis[i]), "dpi %u err=%lu", fd_dpis[i], GetLastError());
        CHECKF(a.iHorzSpacing > 0 && a.iVertSpacing > 0, "dpi %u spacing", fd_dpis[i]);
        CHECKF(a.lfFont.lfFaceName[0], "dpi %u face", fd_dpis[i]);
    }
    memset(&a, 0, sizeof a); a.cbSize = sizeof a;
    memset(&b, 0, sizeof b); b.cbSize = sizeof b;
    SFD(SPI_GETICONMETRICS, sizeof a, &a, 0, 96);
    SFD(SPI_GETICONMETRICS, sizeof b, &b, 0, 192);
    CHECK(b.iHorzSpacing >= a.iHorzSpacing);
    CHECK(b.iVertSpacing >= a.iVertSpacing);
    CHECK(!wcscmp(a.lfFont.lfFaceName, b.lfFont.lfFaceName));
}
TEST(spdpi_iconmetrics_bad_size) {
    ICONMETRICSW m; BOOL r;
    REQUIRE(SFD);
    memset(&m, 0, sizeof m); m.cbSize = 0;
    SetLastError(0xdeadbeef);
    r = SFD(SPI_GETICONMETRICS, 0, &m, 0, 96);
    t_note("ForDpi iconmetrics cbSize 0 -> %d err=%lu", r, GetLastError());
    CHECK(!r);
}
TEST(spdpi_icontitlelogfont) {
    LOGFONTW a, b; unsigned i;
    REQUIRE(SFD);
    for (i = 0; i < sizeof fd_dpis / sizeof *fd_dpis; i++) {
        memset(&a, 0, sizeof a);
        CHECKF(SFD(SPI_GETICONTITLELOGFONT, sizeof a, &a, 0, fd_dpis[i]), "dpi %u err=%lu", fd_dpis[i], GetLastError());
        CHECKF(a.lfFaceName[0], "dpi %u face", fd_dpis[i]);
        CHECKF(a.lfHeight != 0, "dpi %u height", fd_dpis[i]);
    }
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    SFD(SPI_GETICONTITLELOGFONT, sizeof a, &a, 0, 96);
    SFD(SPI_GETICONTITLELOGFONT, sizeof b, &b, 0, 192);
    CHECK(!wcscmp(a.lfFaceName, b.lfFaceName));
    CHECK(abs(b.lfHeight) >= abs(a.lfHeight));
    CHECK(abs(b.lfHeight) <= 2 * abs(a.lfHeight) + 2);
}
TEST(spdpi_icontitlelogfont_vs_ncm_icon) {
    LOGFONTW l; ICONMETRICSW m;
    REQUIRE(SFD);
    memset(&m, 0, sizeof m); m.cbSize = sizeof m;
    CHECK(SFD(SPI_GETICONTITLELOGFONT, sizeof l, &l, 0, 144));
    CHECK(SFD(SPI_GETICONMETRICS, sizeof m, &m, 0, 144));
    CHECK(l.lfHeight == m.lfFont.lfHeight);
    CHECK(!wcscmp(l.lfFaceName, m.lfFont.lfFaceName));
}
TEST(spdpi_unsupported_actions_fail) {
    UINT a; int succeeded = 0; UINT buf[64];
    REQUIRE(SFD);
    for (a = 1; a < 120; a++) {
        BOOL r;
        if (a == SPI_GETICONTITLELOGFONT || a == SPI_GETNONCLIENTMETRICS || a == SPI_GETICONMETRICS) continue;
        memset(buf, 0, sizeof buf);
        SetLastError(0xdeadbeef);
        r = SFD(a, 0, buf, 0, 144);
        if (r) { succeeded++; t_note("ForDpi action %u unexpectedly succeeded", a); continue; }
        if (GetLastError() != ERROR_INVALID_PARAMETER) t_note("ForDpi action %u err=%lu", a, GetLastError());
    }
    CHECKF(succeeded == 0, "%d unsupported actions succeeded", succeeded);
}
TEST(spdpi_simple_getters_unsupported) {
    UINT v = SENT32; BOOL r;
    REQUIRE(SFD);
    SetLastError(0xdeadbeef);
    r = SFD(SPI_GETBEEP, 0, &v, 0, 96);
    CHECK(!r);
    CHECK(v == SENT32);
    SetLastError(0xdeadbeef);
    r = SFD(SPI_GETKEYBOARDSPEED, 0, &v, 0, 96);
    CHECK(!r);
    CHECK(v == SENT32);
}
TEST(spdpi_set_actions_unsupported) {
    BOOL r;
    REQUIRE(SFD);
    SetLastError(0xdeadbeef);
    r = SFD(SPI_SETBEEP, 1, NULL, 0, 96);
    CHECK(!r);
    SetLastError(0xdeadbeef);
    r = SFD(SPI_SETNONCLIENTMETRICS, 0, NULL, 0, 96);
    CHECK(!r);
}
TEST(spdpi_invalid_action_codes) {
    static const UINT bad[] = { 0, 0xFFFF, 0x10000, 0xFFFFFFFF, 0x2000 };
    UINT v[8]; unsigned i;
    REQUIRE(SFD);
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        BOOL r;
        SetLastError(0xdeadbeef);
        r = SFD(bad[i], 0, v, 0, 96);
        CHECKF(!r, "ForDpi action 0x%x accepted", bad[i]);
    }
}
TEST(spdpi_dpi_zero) {
    NONCLIENTMETRICSW n; BOOL r;
    REQUIRE(SFD);
    memset(&n, 0, sizeof n); n.cbSize = sizeof n;
    SetLastError(0xdeadbeef);
    r = SFD(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0, 0);
    t_note("ForDpi ncm dpi 0 -> %d err=%lu scroll=%d", r, GetLastError(), n.iScrollWidth);
}
TEST(spdpi_dpi_extremes) {
    NONCLIENTMETRICSW n; BOOL r; static const UINT d[] = { 1, 24, 10000 };
    unsigned i;
    REQUIRE(SFD);
    for (i = 0; i < 3; i++) {
        memset(&n, 0, sizeof n); n.cbSize = sizeof n;
        r = SFD(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0, d[i]);
        t_note("ForDpi ncm dpi %u -> %d scroll=%d", d[i], r, n.iScrollWidth);
    }
}
TEST(spdpi_fwinini_ignored_for_get) {
    LOGFONTW a, b;
    REQUIRE(SFD);
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(SFD(SPI_GETICONTITLELOGFONT, sizeof a, &a, 0, 96));
    if (SFD(SPI_GETICONTITLELOGFONT, sizeof b, &b, SPIF_UPDATEINIFILE, 96)) CHECK(!memcmp(&a, &b, sizeof a));
    else t_note("ForDpi with fWinIni flag rejected, err=%lu", GetLastError());
}
TEST(spdpi_repeat_stable) {
    NONCLIENTMETRICSW a, b; int i, bad = 0;
    REQUIRE(SFD);
    CHECK(ncm_dpi(120, &a));
    for (i = 0; i < 500; i++) {
        if (!ncm_dpi(120, &b) || b.iCaptionHeight != a.iCaptionHeight || b.lfMenuFont.lfHeight != a.lfMenuFont.lfHeight) bad++;
    }
    CHECK(bad == 0);
}
TEST(spdpi_matches_plain_spi_at_system_dpi) {
    NONCLIENTMETRICSW a, b; UINT sd;
    REQUIRE(SFD); REQUIRE(U(GetDpiForSystem));
    sd = U(GetDpiForSystem)();
    memset(&a, 0, sizeof a); a.cbSize = sizeof a;
    CHECK(sW(SPI_GETNONCLIENTMETRICS, sizeof a, &a, 0));
    CHECK(ncm_dpi(sd, &b));
    CHECK(!wcscmp(a.lfMenuFont.lfFaceName, b.lfMenuFont.lfFaceName));
    if (a.iCaptionHeight != b.iCaptionHeight || a.lfMenuFont.lfHeight != b.lfMenuFont.lfHeight)
        t_note("plain vs ForDpi(%u): caption %d/%d menu font %ld/%ld", sd, a.iCaptionHeight, b.iCaptionHeight, a.lfMenuFont.lfHeight, b.lfMenuFont.lfHeight);
}
TEST(spdpi_from_other_thread) {
    static BOOL ok; HANDLE th; DWORD tid;
    REQUIRE(SFD);
    DWORD WINAPI thr(LPVOID p) { NONCLIENTMETRICSW n; ok = ncm_dpi(144, &n) && n.iCaptionHeight > 0; return 0; }
    ok = FALSE;
    th = CreateThread(NULL, 0, thr, NULL, 0, &tid);
    CHECK(th != NULL);
    if (!th) return;
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    CloseHandle(th);
    CHECK(ok);
}

/* ---- DPI awareness contexts ---- */
UDECL(DPI_AWARENESS_CONTEXT, GetDpiAwarenessContextForProcess, (HANDLE));
#define CTX(n) ((DPI_AWARENESS_CONTEXT)(LONG_PTR)(n))
static const LONG_PTR ctxs[] = { -1, -2, -3, -4, -5 };

TEST(dpictx_is_valid) {
    unsigned i;
    REQUIRE(U(IsValidDpiAwarenessContext));
    for (i = 0; i < 5; i++) CHECKF(U(IsValidDpiAwarenessContext)(CTX(ctxs[i])), "ctx %ld invalid", (long)ctxs[i]);
    CHECK(!U(IsValidDpiAwarenessContext)(NULL));
    CHECK(!U(IsValidDpiAwarenessContext)(CTX(-6)));
    CHECK(!U(IsValidDpiAwarenessContext)(CTX(-100)));
    CHECK(!U(IsValidDpiAwarenessContext)(CTX(1)));
    CHECK(!U(IsValidDpiAwarenessContext)(CTX(0x1234)));
}
TEST(dpictx_equal_self) {
    unsigned i;
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    for (i = 0; i < 5; i++) CHECKF(U(AreDpiAwarenessContextsEqual)(CTX(ctxs[i]), CTX(ctxs[i])), "ctx %ld", (long)ctxs[i]);
}
TEST(dpictx_equal_distinct) {
    unsigned i, j;
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    for (i = 0; i < 5; i++) for (j = 0; j < 5; j++) {
        if (i == j) continue;
        CHECKF(!U(AreDpiAwarenessContextsEqual)(CTX(ctxs[i]), CTX(ctxs[j])), "ctx %ld == %ld", (long)ctxs[i], (long)ctxs[j]);
    }
}
TEST(dpictx_equal_invalid) {
    BOOL r;
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    SetLastError(0xdeadbeef);
    r = U(AreDpiAwarenessContextsEqual)(NULL, NULL);
    t_note("AreEqual(NULL,NULL) = %d err=%lu", r, GetLastError());
    SetLastError(0xdeadbeef);
    r = U(AreDpiAwarenessContextsEqual)(CTX(-1), CTX(0x7777));
    CHECK(!r);
    r = U(AreDpiAwarenessContextsEqual)(CTX(0x7777), CTX(0x7777));
    t_note("AreEqual(invalid,invalid same) = %d", r);
}
TEST(dpictx_awareness_from_context) {
    REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    CHECK(U(GetAwarenessFromDpiAwarenessContext)(CTX(-1)) == DPI_AWARENESS_UNAWARE);
    CHECK(U(GetAwarenessFromDpiAwarenessContext)(CTX(-2)) == DPI_AWARENESS_SYSTEM_AWARE);
    CHECK(U(GetAwarenessFromDpiAwarenessContext)(CTX(-3)) == DPI_AWARENESS_PER_MONITOR_AWARE);
    CHECK(U(GetAwarenessFromDpiAwarenessContext)(CTX(-4)) == DPI_AWARENESS_PER_MONITOR_AWARE);
    CHECK(U(GetAwarenessFromDpiAwarenessContext)(CTX(-5)) == DPI_AWARENESS_UNAWARE);
}
TEST(dpictx_awareness_from_invalid) {
    DPI_AWARENESS a;
    REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    a = U(GetAwarenessFromDpiAwarenessContext)(NULL);
    CHECK(a == DPI_AWARENESS_INVALID);
    a = U(GetAwarenessFromDpiAwarenessContext)(CTX(-77));
    CHECK(a == DPI_AWARENESS_INVALID);
    a = U(GetAwarenessFromDpiAwarenessContext)(CTX(0x1234));
    CHECK(a == DPI_AWARENESS_INVALID);
}
TEST(dpictx_dpi_from_context) {
    UINT d;
    REQUIRE(U(GetDpiFromDpiAwarenessContext));
    d = U(GetDpiFromDpiAwarenessContext)(CTX(-1));
    CHECK(d == 96);
    d = U(GetDpiFromDpiAwarenessContext)(CTX(-5));
    CHECK(d == 96);
    d = U(GetDpiFromDpiAwarenessContext)(CTX(-2));
    CHECK(d >= 96 || d == 0);
    t_note("DPI of PER_MONITOR context = %u, SYSTEM = %u", U(GetDpiFromDpiAwarenessContext)(CTX(-3)), d);
    CHECK(U(GetDpiFromDpiAwarenessContext)(NULL) == 0);
    t_note("DPI of invalid ctx = %u", U(GetDpiFromDpiAwarenessContext)(CTX(0x1234)));
}
TEST(dpictx_thread_get_set_restore) {
    DPI_AWARENESS_CONTEXT orig, old;
    unsigned i;
    REQUIRE(U(GetThreadDpiAwarenessContext)); REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(AreDpiAwarenessContextsEqual));
    orig = U(GetThreadDpiAwarenessContext)();
    CHECK(orig != NULL);
    CHECK(U(IsValidDpiAwarenessContext)(orig));
    for (i = 0; i < 5; i++) {
        DPI_AWARENESS_CONTEXT before = U(GetThreadDpiAwarenessContext)();
        old = U(SetThreadDpiAwarenessContext)(CTX(ctxs[i]));
        CHECKF(old != NULL, "set ctx %ld failed err=%lu", (long)ctxs[i], GetLastError());
        CHECKF(U(AreDpiAwarenessContextsEqual)(old, before), "old ctx mismatch for %ld", (long)ctxs[i]);
        CHECKF(U(AreDpiAwarenessContextsEqual)(U(GetThreadDpiAwarenessContext)(), CTX(ctxs[i])), "get after set %ld", (long)ctxs[i]);
    }
    old = U(SetThreadDpiAwarenessContext)(orig);
    CHECK(old != NULL);
    CHECK(U(AreDpiAwarenessContextsEqual)(U(GetThreadDpiAwarenessContext)(), orig));
}
TEST(dpictx_thread_set_invalid) {
    DPI_AWARENESS_CONTEXT orig, r;
    static const LONG_PTR bad[] = { 0, 1, 0x11, -6, -100, 0x1234, (LONG_PTR)0x80000000 };
    unsigned i;
    REQUIRE(U(GetThreadDpiAwarenessContext)); REQUIRE(U(SetThreadDpiAwarenessContext));
    orig = U(GetThreadDpiAwarenessContext)();
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0xdeadbeef);
        r = U(SetThreadDpiAwarenessContext)(CTX(bad[i]));
        CHECKF(r == NULL, "set invalid ctx %ld accepted", (long)bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "set invalid ctx %ld err=%lu", (long)bad[i], GetLastError());
        CHECKF(U(AreDpiAwarenessContextsEqual)(U(GetThreadDpiAwarenessContext)(), orig), "thread ctx changed by invalid set");
    }
}
TEST(dpictx_thread_isolated_between_threads) {
    static DPI_AWARENESS_CONTEXT seen; static int ok;
    DPI_AWARENESS_CONTEXT orig; HANDLE th; DWORD tid;
    REQUIRE(U(GetThreadDpiAwarenessContext)); REQUIRE(U(SetThreadDpiAwarenessContext));
    DWORD WINAPI thr(LPVOID p) { seen = U(GetThreadDpiAwarenessContext)(); ok = 1; return 0; }
    orig = U(GetThreadDpiAwarenessContext)();
    U(SetThreadDpiAwarenessContext)(CTX(-2));
    seen = NULL; ok = 0;
    th = CreateThread(NULL, 0, thr, NULL, 0, &tid);
    CHECK(th != NULL);
    if (th) { CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0); CloseHandle(th); }
    U(SetThreadDpiAwarenessContext)(orig);
    CHECK(ok);
    CHECK(seen != NULL);
    /* a new thread starts with the process default, not the caller's per-thread override */
    t_note("new thread ctx %p, caller restored ctx %p", seen, orig);
}
TEST(dpictx_process_context) {
    DPI_AWARENESS_CONTEXT c;
    REQUIRE(UN(GetDpiAwarenessContextForProcess));
    c = UN(GetDpiAwarenessContextForProcess)(GetCurrentProcess());
    CHECK(c != NULL);
    CHECK(U(IsValidDpiAwarenessContext)(c));
    c = UN(GetDpiAwarenessContextForProcess)(NULL);
    CHECK(c != NULL);
    SetLastError(0xdeadbeef);
    c = UN(GetDpiAwarenessContextForProcess)((HANDLE)(ULONG_PTR)0x12345678);
    t_note("GetDpiAwarenessContextForProcess(bogus) = %p err=%lu", c, GetLastError());
}
TEST(dpictx_system_dpi) {
    UINT d;
    REQUIRE(U(GetDpiForSystem));
    d = U(GetDpiForSystem)();
    CHECK(d >= 96 && d <= 960);
    CHECK(U(GetDpiForSystem)() == d);
}
TEST(dpictx_set_process_invalid_contexts) {
    static const LONG_PTR bad[] = { 0, 0x11, 0x21, 0x32, 0x6012, 0x6022, 0x7810, -6, -100 };
    unsigned i;
    REQUIRE(U(SetProcessDpiAwarenessContext));
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        BOOL r;
        SetLastError(0xdeadbeef);
        r = U(SetProcessDpiAwarenessContext)(CTX(bad[i]));
        CHECKF(!r, "SetProcessDpiAwarenessContext(%ld) accepted", (long)bad[i]);
        /* invalid -> ERROR_INVALID_PARAMETER; once awareness is locked in -> ERROR_ACCESS_DENIED */
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() == ERROR_ACCESS_DENIED, "ctx %ld err=%lu", (long)bad[i], GetLastError());
    }
}
TEST(dpictx_setprocessdpiaware_semantics) {
    BOOL r1, r2, r3; DWORD e;
    REQUIRE(U(SetProcessDPIAware)); REQUIRE(U(SetProcessDpiAwarenessContext)); REQUIRE(U(IsProcessDPIAware));
    /* first call may either lock in system awareness or find awareness already decided; both succeed */
    SetLastError(0xdeadbeef);
    r1 = U(SetProcessDPIAware)();
    CHECKF(r1, "SetProcessDPIAware failed err=%lu", GetLastError());
    /* repeating is a no-op that still succeeds */
    r2 = U(SetProcessDPIAware)();
    CHECK(r2);
    r3 = U(SetProcessDPIAware)();
    CHECK(r3);
    CHECK(U(IsProcessDPIAware)());
    /* awareness is now fixed: attempts to change it fail with access denied */
    SetLastError(0xdeadbeef);
    r1 = U(SetProcessDpiAwarenessContext)(CTX(-4));
    e = GetLastError();
    CHECK(!r1);
    CHECK(e == ERROR_ACCESS_DENIED);
    SetLastError(0xdeadbeef);
    r1 = U(SetProcessDpiAwarenessContext)(CTX(-2));
    CHECK(!r1);
    CHECK(GetLastError() == ERROR_ACCESS_DENIED);
    SetLastError(0xdeadbeef);
    r1 = U(SetProcessDpiAwarenessContext)(NULL);
    CHECK(!r1);
    CHECK(GetLastError() == ERROR_ACCESS_DENIED);
}
TEST(dpictx_isprocessdpiaware_after) {
    REQUIRE(U(IsProcessDPIAware));
    t_note("IsProcessDPIAware = %d", U(IsProcessDPIAware)());
}
TEST(dpictx_window_context_of_desktop) {
    HWND d;
    DPI_AWARENESS_CONTEXT c;
    REQUIRE(U(GetDesktopWindow)); REQUIRE(U(GetWindowDpiAwarenessContext));
    d = U(GetDesktopWindow)();
    c = U(GetWindowDpiAwarenessContext)(d);
    t_note("desktop window dpi ctx %p", c);
    SetLastError(0xdeadbeef);
    c = U(GetWindowDpiAwarenessContext)(NULL);
    CHECK(c == NULL);
    SetLastError(0xdeadbeef);
    c = U(GetWindowDpiAwarenessContext)((HWND)(ULONG_PTR)0x12345678);
    CHECK(c == NULL);
}
TEST(dpictx_dpi_for_window) {
    HWND w; UINT d;
    REQUIRE(U(GetDpiForWindow));
    w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 50, 50, NULL);
    CHECK(w != NULL);
    if (w) {
        d = U(GetDpiForWindow)(w);
        CHECK(d >= 96 && d <= 960);
        U(DestroyWindow)(w);
        SetLastError(0xdeadbeef);
        CHECK(U(GetDpiForWindow)(w) == 0);
    }
    SetLastError(0xdeadbeef);
    CHECK(U(GetDpiForWindow)(NULL) == 0);
    SetLastError(0xdeadbeef);
    CHECK(U(GetDpiForWindow)((HWND)(ULONG_PTR)0x12345678) == 0);
}
