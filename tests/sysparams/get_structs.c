/* SystemParametersInfo: struct-taking getters and cbSize validation. */
#include "sp.h"

typedef struct { UINT cbSize; BOOL Enabled; LANGID Locale; } SP_AUDIODESC;
#ifndef SPI_GETAUDIODESCRIPTION
#define SPI_GETAUDIODESCRIPTION 0x0074
#endif

static union { char raw[2048]; UINT u[512]; ULONGLONG align; } g_buf;

/* Try SPI get with explicit uiParam / cbSize on a scratch buffer. */
static BOOL try_struct(BOOL wide, UINT spi, UINT uiParam, UINT cbSize) {
    memset(&g_buf, 0xCD, sizeof g_buf);
    g_buf.u[0] = cbSize;
    return wide ? sW(spi, uiParam, &g_buf, 0) : sA(spi, uiParam, &g_buf, 0);
}

/* Generic validation: wrong cbSize (with matching uiParam) is rejected. */
static void check_bad_sizes(UINT spi, UINT good, const char *name) {
    UINT bad[8], n = 0, i;
    bad[n++] = 0; bad[n++] = 1; bad[n++] = good - 1; bad[n++] = good + 1;
    bad[n++] = good * 2; bad[n++] = 0xFFFFFFFF; bad[n++] = 0x80000000; bad[n++] = 0x10000;
    for (i = 0; i < n; i++) {
        BOOL r;
        SetLastError(0xdeadbeef);
        r = try_struct(TRUE, spi, bad[i], bad[i]);
        CHECKF(!r, "%s: cbSize/uiParam=%u accepted", name, bad[i]);
        if (!r && GetLastError() != ERROR_INVALID_PARAMETER) t_note("%s: bad size %u err=%lu", name, bad[i], GetLastError());
        SetLastError(0xdeadbeef);
        r = try_struct(FALSE, spi, bad[i], bad[i]);
        CHECKF(!r, "%s (A): cbSize/uiParam=%u accepted", name, bad[i]);
    }
}

#define SIZETEST(tn, spi, type) \
TEST(spstruct_badsize_##tn) { REQUIRE(U(SystemParametersInfoW)); REQUIRE(U(SystemParametersInfoA)); \
    CHECKF(try_struct(TRUE, spi, sizeof(type), sizeof(type)), "good size rejected err=%lu", GetLastError()); \
    CHECKF(try_struct(FALSE, spi, sizeof(type), sizeof(type)), "good size rejected (A) err=%lu", GetLastError()); \
    check_bad_sizes(spi, sizeof(type), #tn); }

SIZETEST(filterkeys, SPI_GETFILTERKEYS, FILTERKEYS)
SIZETEST(stickykeys, SPI_GETSTICKYKEYS, STICKYKEYS)
SIZETEST(mousekeys, SPI_GETMOUSEKEYS, MOUSEKEYS)
SIZETEST(togglekeys, SPI_GETTOGGLEKEYS, TOGGLEKEYS)
SIZETEST(accesstimeout, SPI_GETACCESSTIMEOUT, ACCESSTIMEOUT)
SIZETEST(minimizedmetrics, SPI_GETMINIMIZEDMETRICS, MINIMIZEDMETRICS)
SIZETEST(animation, SPI_GETANIMATION, ANIMATIONINFO)

/* uiParam may be 0 with a correct cbSize for several of these. */
#define PARAM0TEST(tn, spi, type) \
TEST(spstruct_uiparam0_##tn) { REQUIRE(U(SystemParametersInfoW)); \
    BOOL r = try_struct(TRUE, spi, 0, sizeof(type)); \
    t_note(#tn ": uiParam=0 with valid cbSize -> %d err=%lu", r, GetLastError()); }
PARAM0TEST(filterkeys, SPI_GETFILTERKEYS, FILTERKEYS)
PARAM0TEST(stickykeys, SPI_GETSTICKYKEYS, STICKYKEYS)
PARAM0TEST(mousekeys, SPI_GETMOUSEKEYS, MOUSEKEYS)
PARAM0TEST(togglekeys, SPI_GETTOGGLEKEYS, TOGGLEKEYS)
PARAM0TEST(accesstimeout, SPI_GETACCESSTIMEOUT, ACCESSTIMEOUT)
PARAM0TEST(minimizedmetrics, SPI_GETMINIMIZEDMETRICS, MINIMIZEDMETRICS)
PARAM0TEST(animation, SPI_GETANIMATION, ANIMATIONINFO)
PARAM0TEST(ncm, SPI_GETNONCLIENTMETRICS, NONCLIENTMETRICSW)
PARAM0TEST(iconmetrics, SPI_GETICONMETRICS, ICONMETRICSW)

/* ---- NONCLIENTMETRICS ---- */
#define NCM_OLD_W ((UINT)offsetof(NONCLIENTMETRICSW, iPaddedBorderWidth))
#define NCM_OLD_A ((UINT)offsetof(NONCLIENTMETRICSA, iPaddedBorderWidth))

TEST(spstruct_ncm_sizes) {
    CHECK(NCM_OLD_W == 500);
    CHECK(sizeof(NONCLIENTMETRICSW) == NCM_OLD_W + 4); /* iPaddedBorderWidth */
    CHECK(sizeof(NONCLIENTMETRICSA) == NCM_OLD_A + 4);
    CHECK(sizeof(LOGFONTW) == 92);
    CHECK(sizeof(LOGFONTA) == 60);
}
TEST(spstruct_ncm_get_w) {
    NONCLIENTMETRICSW n; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&n, 0, sizeof n); n.cbSize = sizeof n;
    r = sW(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0);
    CHECKF(r, "err=%lu", GetLastError());
    if (!r) return;
    CHECK(n.cbSize == sizeof n);
    CHECK(n.iBorderWidth >= 1 && n.iBorderWidth < 50);
    CHECK(n.iScrollWidth > 0 && n.iScrollWidth < 200);
    CHECK(n.iScrollHeight > 0 && n.iScrollHeight < 200);
    CHECK(n.iCaptionWidth > 0 && n.iCaptionWidth < 400);
    CHECK(n.iCaptionHeight > 0 && n.iCaptionHeight < 200);
    CHECK(n.iSmCaptionWidth > 0 && n.iSmCaptionWidth < 400);
    CHECK(n.iSmCaptionHeight > 0 && n.iSmCaptionHeight < 200);
    CHECK(n.iMenuWidth > 0 && n.iMenuWidth < 400);
    CHECK(n.iMenuHeight > 0 && n.iMenuHeight < 200);
    CHECK(n.lfCaptionFont.lfFaceName[0] != 0);
    CHECK(n.lfSmCaptionFont.lfFaceName[0] != 0);
    CHECK(n.lfMenuFont.lfFaceName[0] != 0);
    CHECK(n.lfStatusFont.lfFaceName[0] != 0);
    CHECK(n.lfMessageFont.lfFaceName[0] != 0);
    CHECK(n.lfCaptionFont.lfHeight != 0);
    CHECK(n.lfMenuFont.lfHeight != 0);
    CHECK(n.lfMessageFont.lfHeight != 0);
    CHECK(n.lfCaptionFont.lfWeight >= FW_DONTCARE && n.lfCaptionFont.lfWeight <= FW_HEAVY);
    CHECK(n.lfMenuFont.lfWeight >= FW_DONTCARE && n.lfMenuFont.lfWeight <= FW_HEAVY);
    CHECK(wcslen(n.lfCaptionFont.lfFaceName) < LF_FACESIZE);
    t_note("iPaddedBorderWidth=%d SM_CXPADDEDBORDER=%d", n.iPaddedBorderWidth, gsm(SM_CXPADDEDBORDER));
    t_note("caption font '%ls' h=%ld", n.lfCaptionFont.lfFaceName, n.lfCaptionFont.lfHeight);
}
TEST(spstruct_ncm_metrics_vs_system_metrics) {
    NONCLIENTMETRICSW n;
    REQUIRE(U(SystemParametersInfoW));
    memset(&n, 0, sizeof n); n.cbSize = sizeof n;
    if (!sW(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0)) { CHECK(0); return; }
    CHECK(n.iScrollWidth == gsm(SM_CXVSCROLL));
    CHECK(n.iScrollHeight == gsm(SM_CYHSCROLL));
    CHECK(n.iSmCaptionHeight + 1 == gsm(SM_CYSMCAPTION) || n.iSmCaptionHeight == gsm(SM_CYSMCAPTION));
    CHECK(n.iCaptionHeight + 1 == gsm(SM_CYCAPTION) || n.iCaptionHeight == gsm(SM_CYCAPTION));
    t_note("iMenuHeight=%d SM_CYMENUSIZE=%d SM_CYMENU=%d iBorderWidth=%d", n.iMenuHeight, gsm(SM_CYMENUSIZE), gsm(SM_CYMENU), n.iBorderWidth);
}
TEST(spstruct_ncm_get_a_w_parity) {
    NONCLIENTMETRICSW w; NONCLIENTMETRICSA a;
    char conv[LF_FACESIZE + 4];
    REQUIRE(U(SystemParametersInfoW)); REQUIRE(U(SystemParametersInfoA));
    memset(&w, 0, sizeof w); w.cbSize = sizeof w;
    memset(&a, 0, sizeof a); a.cbSize = sizeof a;
    CHECK(sW(SPI_GETNONCLIENTMETRICS, sizeof w, &w, 0));
    CHECKF(sA(SPI_GETNONCLIENTMETRICS, sizeof a, &a, 0), "A err=%lu", GetLastError());
    CHECK(a.iBorderWidth == w.iBorderWidth);
    CHECK(a.iScrollWidth == w.iScrollWidth);
    CHECK(a.iScrollHeight == w.iScrollHeight);
    CHECK(a.iCaptionWidth == w.iCaptionWidth);
    CHECK(a.iCaptionHeight == w.iCaptionHeight);
    CHECK(a.iSmCaptionWidth == w.iSmCaptionWidth);
    CHECK(a.iSmCaptionHeight == w.iSmCaptionHeight);
    CHECK(a.iMenuWidth == w.iMenuWidth);
    CHECK(a.iMenuHeight == w.iMenuHeight);
    CHECK(a.lfCaptionFont.lfHeight == w.lfCaptionFont.lfHeight);
    CHECK(a.lfMenuFont.lfWeight == w.lfMenuFont.lfWeight);
    memset(conv, 0, sizeof conv);
    WideCharToMultiByte(CP_ACP, 0, w.lfCaptionFont.lfFaceName, -1, conv, sizeof conv, NULL, NULL);
    CHECK(!strcmp(conv, a.lfCaptionFont.lfFaceName));
    memset(conv, 0, sizeof conv);
    WideCharToMultiByte(CP_ACP, 0, w.lfMenuFont.lfFaceName, -1, conv, sizeof conv, NULL, NULL);
    CHECK(!strcmp(conv, a.lfMenuFont.lfFaceName));
    memset(conv, 0, sizeof conv);
    WideCharToMultiByte(CP_ACP, 0, w.lfStatusFont.lfFaceName, -1, conv, sizeof conv, NULL, NULL);
    CHECK(!strcmp(conv, a.lfStatusFont.lfFaceName));
}
TEST(spstruct_ncm_old_size_accepted) {
    NONCLIENTMETRICSW n;
    BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&n, 0xCD, sizeof n); n.cbSize = NCM_OLD_W;
    r = sW(SPI_GETNONCLIENTMETRICS, NCM_OLD_W, &n, 0);
    CHECKF(r, "pre-Vista sized NONCLIENTMETRICSW rejected err=%lu", GetLastError());
    if (r) {
        CHECK(n.lfMessageFont.lfFaceName[0] != 0 && n.lfMessageFont.lfFaceName[0] != 0xCDCD);
        t_note("padded border after old-size call: 0x%x", n.iPaddedBorderWidth);
    }
}
TEST(spstruct_ncm_old_size_accepted_a) {
    NONCLIENTMETRICSA n;
    BOOL r;
    REQUIRE(U(SystemParametersInfoA));
    memset(&n, 0xCD, sizeof n); n.cbSize = NCM_OLD_A;
    r = sA(SPI_GETNONCLIENTMETRICS, NCM_OLD_A, &n, 0);
    CHECKF(r, "pre-Vista sized NONCLIENTMETRICSA rejected err=%lu", GetLastError());
}
TEST(spstruct_ncm_off_by_one_sizes) {
    NONCLIENTMETRICSW n; UINT sz[] = { NCM_OLD_W - 1, NCM_OLD_W + 1, NCM_OLD_W + 2, NCM_OLD_W + 3, NCM_OLD_W + 4, sizeof n - 1, sizeof n + 1, 0, 1, 4, 0xFFFFFFFF };
    unsigned i;
    REQUIRE(U(SystemParametersInfoW));
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        BOOL r;
        memset(&g_buf, 0, sizeof g_buf); g_buf.u[0] = sz[i];
        SetLastError(0xdeadbeef);
        r = sW(SPI_GETNONCLIENTMETRICS, 0, &g_buf, 0);
        if (sz[i] == NCM_OLD_W + 4) { t_note("ncm size %u -> %d", sz[i], r); continue; }
        if (i >= 7 && i <= 10) { t_note("ncm cbSize %u -> %d err=%lu", sz[i], r, GetLastError()); continue; }
        t_note("ncm cbSize %u -> %d err=%lu", sz[i], r, GetLastError());
    }
}
TEST(spstruct_ncm_off_by_one_sizes_a) {
    UINT sz[] = { NCM_OLD_A - 1, NCM_OLD_A + 1, NCM_OLD_A + 2, 0, 1, 0xFFFFFFFF };
    unsigned i;
    REQUIRE(U(SystemParametersInfoA));
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        BOOL r;
        memset(&g_buf, 0, sizeof g_buf); g_buf.u[0] = sz[i];
        r = sA(SPI_GETNONCLIENTMETRICS, 0, &g_buf, 0);
        t_note("ncmA cbSize %u -> %d", sz[i], r);
    }
}
TEST(spstruct_ncm_uiparam_mismatch) {
    NONCLIENTMETRICSW n; BOOL r1, r2, r3;
    REQUIRE(U(SystemParametersInfoW));
    memset(&n, 0, sizeof n); n.cbSize = sizeof n;
    r1 = sW(SPI_GETNONCLIENTMETRICS, 0, &n, 0);
    n.cbSize = sizeof n;
    r2 = sW(SPI_GETNONCLIENTMETRICS, sizeof n + 8, &n, 0);
    n.cbSize = sizeof n;
    r3 = sW(SPI_GETNONCLIENTMETRICS, 1, &n, 0);
    t_note("ncm uiParam=0 -> %d, uiParam=size+8 -> %d, uiParam=1 -> %d", r1, r2, r3);
}
TEST(spstruct_ncm_repeat_identical) {
    NONCLIENTMETRICSW a, b;
    REQUIRE(U(SystemParametersInfoW));
    memset(&a, 0, sizeof a); memset(&b, 0xFF, sizeof b);
    a.cbSize = b.cbSize = sizeof a;
    CHECK(sW(SPI_GETNONCLIENTMETRICS, sizeof a, &a, 0));
    CHECK(sW(SPI_GETNONCLIENTMETRICS, sizeof b, &b, 0));
    CHECK(!memcmp(&a.iBorderWidth, &b.iBorderWidth, 9 * sizeof(int)));
    CHECK(a.lfMenuFont.lfHeight == b.lfMenuFont.lfHeight);
    CHECK(!wcscmp(a.lfMenuFont.lfFaceName, b.lfMenuFont.lfFaceName));
}
TEST(spstruct_ncm_font_charsets) {
    NONCLIENTMETRICSW n;
    REQUIRE(U(SystemParametersInfoW));
    memset(&n, 0, sizeof n); n.cbSize = sizeof n;
    CHECK(sW(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0));
    CHECK(n.lfCaptionFont.lfQuality <= 6);
    CHECK(n.lfMenuFont.lfQuality <= 6);
    CHECK(n.lfCaptionFont.lfItalic <= 1);
    CHECK(n.lfCaptionFont.lfUnderline <= 1);
    CHECK(n.lfCaptionFont.lfStrikeOut <= 1);
    CHECK(n.lfCaptionFont.lfEscapement == 0);
    CHECK(n.lfCaptionFont.lfOrientation == 0);
}

/* ---- ICONMETRICS / ICONTITLELOGFONT ---- */
TEST(spstruct_iconmetrics_get_w) {
    ICONMETRICSW im; BOOL r; UINT wrap;
    REQUIRE(U(SystemParametersInfoW));
    memset(&im, 0, sizeof im); im.cbSize = sizeof im;
    r = sW(SPI_GETICONMETRICS, sizeof im, &im, 0);
    CHECKF(r, "err=%lu", GetLastError());
    if (!r) return;
    CHECK(im.cbSize == sizeof im);
    CHECK(im.iHorzSpacing == gsm(SM_CXICONSPACING));
    CHECK(im.iVertSpacing == gsm(SM_CYICONSPACING));
    CHECK(im.iHorzSpacing >= 32 && im.iVertSpacing >= 32);
    CHECK(sp_get_u32(SPI_GETICONTITLEWRAP, &wrap));
    CHECK((UINT)im.iTitleWrap == wrap);
    CHECK(im.lfFont.lfFaceName[0] != 0);
}
TEST(spstruct_iconmetrics_vs_titlelogfont) {
    ICONMETRICSW im; LOGFONTW lf;
    REQUIRE(U(SystemParametersInfoW));
    memset(&im, 0, sizeof im); im.cbSize = sizeof im;
    memset(&lf, 0xCD, sizeof lf);
    CHECK(sW(SPI_GETICONMETRICS, sizeof im, &im, 0));
    CHECK(sW(SPI_GETICONTITLELOGFONT, sizeof lf, &lf, 0));
    CHECK(lf.lfHeight == im.lfFont.lfHeight);
    CHECK(lf.lfWeight == im.lfFont.lfWeight);
    CHECK(lf.lfCharSet == im.lfFont.lfCharSet);
    CHECK(!wcscmp(lf.lfFaceName, im.lfFont.lfFaceName));
}
TEST(spstruct_iconmetrics_get_a) {
    ICONMETRICSA im; BOOL r;
    REQUIRE(U(SystemParametersInfoA));
    memset(&im, 0, sizeof im); im.cbSize = sizeof im;
    r = sA(SPI_GETICONMETRICS, sizeof im, &im, 0);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) { CHECK(im.iHorzSpacing == gsm(SM_CXICONSPACING)); CHECK(im.lfFont.lfFaceName[0]); }
}
TEST(spstruct_iconmetrics_badsize) {
    UINT sz[] = { 0, 1, sizeof(ICONMETRICSW) - 1, sizeof(ICONMETRICSW) + 1, sizeof(ICONMETRICSW) + 4, 0xFFFFFFFF };
    unsigned i;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(try_struct(TRUE, SPI_GETICONMETRICS, sizeof(ICONMETRICSW), sizeof(ICONMETRICSW)));
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        BOOL r;
        SetLastError(0xdeadbeef);
        r = try_struct(TRUE, SPI_GETICONMETRICS, sz[i], sz[i]);
        CHECKF(!r, "iconmetrics size %u accepted", sz[i]);
        if (!r) t_note("iconmetrics size %u err=%lu", sz[i], GetLastError());
    }
}
TEST(spstruct_icontitlelogfont_w) {
    LOGFONTW lf;
    REQUIRE(U(SystemParametersInfoW));
    memset(&lf, 0xCD, sizeof lf);
    CHECK(sW(SPI_GETICONTITLELOGFONT, sizeof lf, &lf, 0));
    CHECK(lf.lfFaceName[0] != 0 && lf.lfFaceName[0] != 0xCDCD);
    CHECK(wcsnlen(lf.lfFaceName, LF_FACESIZE) < LF_FACESIZE);
    CHECK(lf.lfHeight != 0);
    CHECK(lf.lfWeight >= 100 && lf.lfWeight <= 900);
}
TEST(spstruct_icontitlelogfont_a_parity) {
    LOGFONTW w; LOGFONTA a; char conv[LF_FACESIZE + 4];
    REQUIRE(U(SystemParametersInfoA)); REQUIRE(U(SystemParametersInfoW));
    memset(&w, 0, sizeof w); memset(&a, 0, sizeof a);
    CHECK(sW(SPI_GETICONTITLELOGFONT, sizeof w, &w, 0));
    CHECK(sA(SPI_GETICONTITLELOGFONT, sizeof a, &a, 0));
    CHECK(a.lfHeight == w.lfHeight);
    CHECK(a.lfWeight == w.lfWeight);
    CHECK(a.lfCharSet == w.lfCharSet);
    memset(conv, 0, sizeof conv);
    WideCharToMultiByte(CP_ACP, 0, w.lfFaceName, -1, conv, sizeof conv, NULL, NULL);
    CHECK(!strcmp(conv, a.lfFaceName));
}
TEST(spstruct_icontitlelogfont_uiparam_sizes) {
    LOGFONTW lf; BOOL r0, r1, r2;
    REQUIRE(U(SystemParametersInfoW));
    r0 = sW(SPI_GETICONTITLELOGFONT, sizeof lf, &lf, 0);
    r1 = sW(SPI_GETICONTITLELOGFONT, 0, &lf, 0);
    r2 = sW(SPI_GETICONTITLELOGFONT, 1, &lf, 0);
    CHECK(r0);
    t_note("ICONTITLELOGFONT uiParam 0 -> %d, 1 -> %d", r1, r2);
}
TEST(spstruct_icontitlelogfont_wrong_size_a_vs_w) {
    LOGFONTA a; BOOL r;
    REQUIRE(U(SystemParametersInfoA));
    memset(&a, 0, sizeof a);
    SetLastError(0xdeadbeef);
    r = sA(SPI_GETICONTITLELOGFONT, sizeof(LOGFONTW), &a, 0);
    t_note("ICONTITLELOGFONT A with W-size uiParam -> %d err=%lu", r, GetLastError());
}

/* ---- MINIMIZEDMETRICS ---- */
TEST(spstruct_minimizedmetrics) {
    MINIMIZEDMETRICS m; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&m, 0xCD, sizeof m); m.cbSize = sizeof m;
    r = sW(SPI_GETMINIMIZEDMETRICS, sizeof m, &m, 0);
    CHECKF(r, "err=%lu", GetLastError());
    if (!r) return;
    CHECK(sizeof m == 20);
    CHECK(m.iWidth > 0 && m.iWidth < 2000);
    CHECK(m.iHorzGap >= 0 && m.iHorzGap < 500);
    CHECK(m.iVertGap >= 0 && m.iVertGap < 500);
    CHECK((m.iArrange & ~(ARW_STARTMASK | 0x000Fu | ARW_HIDE)) == 0);
    CHECK((int)m.iArrange == gsm(SM_ARRANGE));
    t_note("minimized: w=%d hgap=%d vgap=%d arrange=0x%x SM_CXMINSPACING=%d SM_CXMINIMIZED=%d", m.iWidth, m.iHorzGap, m.iVertGap, m.iArrange, gsm(SM_CXMINSPACING), gsm(SM_CXMINIMIZED));
}
TEST(spstruct_minimizedmetrics_spacing) {
    MINIMIZEDMETRICS m;
    REQUIRE(U(SystemParametersInfoW));
    memset(&m, 0, sizeof m); m.cbSize = sizeof m;
    CHECK(sW(SPI_GETMINIMIZEDMETRICS, sizeof m, &m, 0));
    CHECK(gsm(SM_CXMINSPACING) >= m.iWidth);
    CHECK(gsm(SM_CXMINSPACING) == gsm(SM_CXMINIMIZED) + m.iHorzGap);
    CHECK(gsm(SM_CYMINSPACING) == gsm(SM_CYMINIMIZED) + m.iVertGap);
}
TEST(spstruct_minimizedmetrics_a_w) {
    MINIMIZEDMETRICS a, b;
    REQUIRE(U(SystemParametersInfoW)); REQUIRE(U(SystemParametersInfoA));
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); a.cbSize = b.cbSize = sizeof a;
    CHECK(sA(SPI_GETMINIMIZEDMETRICS, sizeof a, &a, 0));
    CHECK(sW(SPI_GETMINIMIZEDMETRICS, sizeof b, &b, 0));
    CHECK(!memcmp(&a, &b, sizeof a));
}

/* ---- ANIMATIONINFO ---- */
TEST(spstruct_animationinfo) {
    ANIMATIONINFO a; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&a, 0xCD, sizeof a); a.cbSize = sizeof a;
    r = sW(SPI_GETANIMATION, sizeof a, &a, 0);
    CHECKF(r, "err=%lu", GetLastError());
    CHECK(sizeof a == 8);
    if (r) { CHECK(a.cbSize == sizeof a); CHECK(a.iMinAnimate == 0 || a.iMinAnimate == 1); }
}
TEST(spstruct_animationinfo_a_w) {
    ANIMATIONINFO a, b;
    REQUIRE(U(SystemParametersInfoW)); REQUIRE(U(SystemParametersInfoA));
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); a.cbSize = b.cbSize = sizeof a;
    CHECK(sA(SPI_GETANIMATION, sizeof a, &a, 0));
    CHECK(sW(SPI_GETANIMATION, sizeof b, &b, 0));
    CHECK(a.iMinAnimate == b.iMinAnimate);
}

/* ---- Accessibility structures ---- */
TEST(spstruct_filterkeys) {
    FILTERKEYS f; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&f, 0xCD, sizeof f); f.cbSize = sizeof f;
    r = sW(SPI_GETFILTERKEYS, sizeof f, &f, 0);
    CHECKF(r, "err=%lu", GetLastError());
    CHECK(sizeof f == 24);
    if (r) {
        CHECK(f.cbSize == sizeof f);
        CHECK((f.dwFlags & ~(FKF_FILTERKEYSON | FKF_AVAILABLE | FKF_HOTKEYACTIVE | FKF_CONFIRMHOTKEY | FKF_HOTKEYSOUND | FKF_INDICATOR | FKF_CLICKON)) == 0);
        t_note("filterkeys flags=0x%lx wait=%lu delay=%lu repeat=%lu bounce=%lu", f.dwFlags, f.iWaitMSec, f.iDelayMSec, f.iRepeatMSec, f.iBounceMSec);
    }
}
TEST(spstruct_stickykeys) {
    STICKYKEYS s; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&s, 0xCD, sizeof s); s.cbSize = sizeof s;
    r = sW(SPI_GETSTICKYKEYS, sizeof s, &s, 0);
    CHECKF(r, "err=%lu", GetLastError());
    CHECK(sizeof s == 8);
    if (r) {
        CHECK(s.cbSize == sizeof s);
        t_note("stickykeys flags=0x%lx", s.dwFlags);
    }
}
TEST(spstruct_mousekeys) {
    MOUSEKEYS m; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&m, 0xCD, sizeof m); m.cbSize = sizeof m;
    r = sW(SPI_GETMOUSEKEYS, sizeof m, &m, 0);
    CHECKF(r, "err=%lu", GetLastError());
    CHECK(sizeof m == 28);
    if (r) {
        CHECK(m.cbSize == sizeof m);
        CHECK(m.iMaxSpeed > 0 && m.iMaxSpeed < 10000);
        CHECK(m.iTimeToMaxSpeed < 100000);
        CHECK(m.iCtrlSpeed < 10000);
        t_note("mousekeys flags=0x%lx maxspeed=%lu ttm=%lu ctrl=%lu", m.dwFlags, m.iMaxSpeed, m.iTimeToMaxSpeed, m.iCtrlSpeed);
    }
}
TEST(spstruct_togglekeys) {
    TOGGLEKEYS t; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&t, 0xCD, sizeof t); t.cbSize = sizeof t;
    r = sW(SPI_GETTOGGLEKEYS, sizeof t, &t, 0);
    CHECKF(r, "err=%lu", GetLastError());
    CHECK(sizeof t == 8);
    if (r) { CHECK(t.cbSize == sizeof t); t_note("togglekeys flags=0x%lx", t.dwFlags); }
}
TEST(spstruct_accesstimeout) {
    ACCESSTIMEOUT a; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&a, 0xCD, sizeof a); a.cbSize = sizeof a;
    r = sW(SPI_GETACCESSTIMEOUT, sizeof a, &a, 0);
    CHECKF(r, "err=%lu", GetLastError());
    CHECK(sizeof a == 12);
    if (r) { CHECK(a.cbSize == sizeof a); t_note("accesstimeout flags=0x%lx timeout=%lu", a.dwFlags, a.iTimeOutMSec); }
}
TEST(spstruct_soundsentry_w) {
    SOUNDSENTRYW s; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&s, 0, sizeof s); s.cbSize = sizeof s;
    r = sW(SPI_GETSOUNDSENTRY, sizeof s, &s, 0);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        CHECK(s.cbSize == sizeof s);
        t_note("soundsentry flags=0x%lx text=%lu gfx=%lu win=%lu", s.dwFlags, s.iFSTextEffect, s.iFSGrafEffect, s.iWindowsEffect);
    }
}
TEST(spstruct_soundsentry_a) {
    SOUNDSENTRYA s; BOOL r;
    REQUIRE(U(SystemParametersInfoA));
    memset(&s, 0, sizeof s); s.cbSize = sizeof s;
    r = sA(SPI_GETSOUNDSENTRY, sizeof s, &s, 0);
    CHECKF(r, "err=%lu", GetLastError());
}
TEST(spstruct_soundsentry_badsize) {
    UINT good = sizeof(SOUNDSENTRYW);
    check_bad_sizes(SPI_GETSOUNDSENTRY, good, "soundsentry");
}
TEST(spstruct_serialkeys_get) {
    SERIALKEYSW s; BOOL r;
    WCHAR port[64], dll[1];
    REQUIRE(U(SystemParametersInfoW));
    memset(&s, 0, sizeof s); s.cbSize = sizeof s;
    s.lpszActivePort = port; s.lpszPort = port;
    port[0] = 0; dll[0] = 0;
    SetLastError(0xdeadbeef);
    r = sW(SPI_GETSERIALKEYS, sizeof s, &s, 0);
    t_note("GETSERIALKEYS -> %d err=%lu flags=0x%lx", r, GetLastError(), s.dwFlags);
}
TEST(spstruct_serialkeys_badsize) {
    SERIALKEYSW s; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&s, 0, sizeof s); s.cbSize = 3;
    SetLastError(0xdeadbeef);
    r = sW(SPI_GETSERIALKEYS, 3, &s, 0);
    t_note("GETSERIALKEYS bad size -> %d err=%lu", r, GetLastError());
    CHECK(!r);
}
TEST(spstruct_audiodescription) {
    SP_AUDIODESC ad; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&ad, 0xCD, sizeof ad); ad.cbSize = sizeof ad;
    r = sW(SPI_GETAUDIODESCRIPTION, sizeof ad, &ad, 0);
    t_note("GETAUDIODESCRIPTION -> %d err=%lu enabled=%d locale=0x%x", r, GetLastError(), ad.Enabled, ad.Locale);
    if (r) CHECK(ad.Enabled == 0 || ad.Enabled == 1);
}
TEST(spstruct_highcontrast_w) {
    HIGHCONTRASTW hc; WCHAR scheme[512]; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(&hc, 0, sizeof hc); hc.cbSize = sizeof hc;
    scheme[0] = 0; hc.lpszDefaultScheme = scheme;
    r = sW(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        CHECK(hc.cbSize == sizeof hc);
        CHECK((hc.dwFlags & ~(HCF_HIGHCONTRASTON | HCF_AVAILABLE | HCF_HOTKEYACTIVE | HCF_CONFIRMHOTKEY | HCF_HOTKEYSOUND | HCF_INDICATOR | HCF_HOTKEYAVAILABLE | 0x400u)) == 0);
        t_note("highcontrast flags=0x%lx scheme='%ls'", hc.dwFlags, hc.lpszDefaultScheme ? hc.lpszDefaultScheme : L"(null)");
    }
}
TEST(spstruct_highcontrast_a) {
    HIGHCONTRASTA hc; char scheme[512]; BOOL r;
    REQUIRE(U(SystemParametersInfoA));
    memset(&hc, 0, sizeof hc); hc.cbSize = sizeof hc;
    scheme[0] = 0; hc.lpszDefaultScheme = scheme;
    r = sA(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0);
    CHECKF(r, "err=%lu", GetLastError());
}
TEST(spstruct_highcontrast_badsize) {
    HIGHCONTRASTW hc; WCHAR scheme[512];
    UINT sz[] = { 0, 1, sizeof hc - 1, sizeof hc + 1, 0xFFFFFFFF };
    unsigned i;
    REQUIRE(U(SystemParametersInfoW));
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        BOOL r;
        memset(&hc, 0, sizeof hc); hc.cbSize = sz[i]; hc.lpszDefaultScheme = scheme;
        SetLastError(0xdeadbeef);
        r = sW(SPI_GETHIGHCONTRAST, sz[i], &hc, 0);
        CHECKF(!r, "highcontrast size %u accepted", sz[i]);
        if (!r) t_note("highcontrast size %u err=%lu", sz[i], GetLastError());
    }
}
TEST(spstruct_workarea_basic) {
    RECT r = { 1, 2, 3, 4 }; RECT a;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETWORKAREA, 0, &r, 0));
    CHECK(r.right > r.left && r.bottom > r.top);
    CHECK(r.right - r.left <= gsm(SM_CXVIRTUALSCREEN));
    CHECK(sA(SPI_GETWORKAREA, 0, &a, 0));
    CHECK(!memcmp(&a, &r, sizeof r));
}
TEST(spstruct_workarea_in_screen) {
    RECT r;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETWORKAREA, 0, &r, 0));
    CHECK(r.left >= 0 && r.top >= 0);
    CHECK(r.right <= gsm(SM_CXSCREEN) && r.bottom <= gsm(SM_CYSCREEN));
    CHECK(r.right - r.left <= gsm(SM_CXFULLSCREEN) + gsm(SM_CXFRAME) * 4 + 100);
}
TEST(spstruct_workarea_matches_monitor) {
    RECT r; MONITORINFO mi; HMONITOR hm; POINT pt = { 0, 0 };
    REQUIRE(U(SystemParametersInfoW)); REQUIRE(U(MonitorFromPoint)); REQUIRE(U(GetMonitorInfoW));
    CHECK(sW(SPI_GETWORKAREA, 0, &r, 0));
    hm = U(MonitorFromPoint)(pt, MONITOR_DEFAULTTOPRIMARY);
    CHECK(hm != NULL);
    memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    CHECK(U(GetMonitorInfoW)(hm, &mi));
    CHECK(mi.dwFlags & MONITORINFOF_PRIMARY);
    CHECK(EqualRect(&r, &mi.rcWork));
    CHECK(mi.rcMonitor.right - mi.rcMonitor.left == gsm(SM_CXSCREEN));
    CHECK(mi.rcMonitor.bottom - mi.rcMonitor.top == gsm(SM_CYSCREEN));
}
TEST(spstruct_workarea_fullscreen_metrics) {
    RECT r;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETWORKAREA, 0, &r, 0));
    CHECK(gsm(SM_CXFULLSCREEN) <= r.right - r.left || gsm(SM_CXFULLSCREEN) == gsm(SM_CXSCREEN));
    t_note("SM_CYFULLSCREEN=%d work height=%ld caption=%d", gsm(SM_CYFULLSCREEN), r.bottom - r.top, gsm(SM_CYCAPTION));
}
TEST(spstruct_menurect_optional) {
    RECT r = { 7, 7, 7, 7 }; BOOL b;
    REQUIRE(U(SystemParametersInfoW));
    SetLastError(0xdeadbeef);
    b = sW(SPI_GETMENURECT, 0, &r, 0);
    t_note("GETMENURECT -> %d err=%lu", b, GetLastError());
}
TEST(spstruct_deskwallpaper_w) {
    WCHAR buf[MAX_PATH + 8]; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    memset(buf, 0xCD, sizeof buf);
    r = sW(SPI_GETDESKWALLPAPER, MAX_PATH, buf, 0);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        size_t n = wcsnlen(buf, MAX_PATH);
        CHECK(n < MAX_PATH);
        t_note("wallpaper '%ls'", buf);
    }
}
TEST(spstruct_deskwallpaper_a_w_parity) {
    WCHAR w[MAX_PATH]; char a[MAX_PATH], c[MAX_PATH];
    REQUIRE(U(SystemParametersInfoW)); REQUIRE(U(SystemParametersInfoA));
    memset(w, 0, sizeof w); memset(a, 0, sizeof a); memset(c, 0, sizeof c);
    CHECK(sW(SPI_GETDESKWALLPAPER, MAX_PATH, w, 0));
    CHECK(sA(SPI_GETDESKWALLPAPER, MAX_PATH, a, 0));
    WideCharToMultiByte(CP_ACP, 0, w, -1, c, sizeof c, NULL, NULL);
    CHECK(!strcmp(a, c));
}
TEST(spstruct_deskwallpaper_buffer_edges) {
    WCHAR buf[MAX_PATH + 8]; WCHAR full[MAX_PATH]; UINT sizes[] = { 0, 1, 2, 5 };
    unsigned i;
    REQUIRE(U(SystemParametersInfoW));
    memset(full, 0, sizeof full);
    if (!sW(SPI_GETDESKWALLPAPER, MAX_PATH, full, 0)) { CHECK(0); return; }
    for (i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        BOOL r;
        memset(buf, 0xCD, sizeof buf);
        SetLastError(0xdeadbeef);
        r = sW(SPI_GETDESKWALLPAPER, sizes[i], buf, 0);
        t_note("wallpaper uiParam=%u -> %d err=%lu first=0x%x", sizes[i], r, GetLastError(), buf[0]);
        /* never writes beyond the declared size */
        CHECK(buf[sizes[i] > MAX_PATH ? MAX_PATH : sizes[i]] == 0xCDCD || sizes[i] >= MAX_PATH);
    }
    if (wcslen(full) > 0) {
        size_t n = wcslen(full);
        memset(buf, 0xCD, sizeof buf);
        sW(SPI_GETDESKWALLPAPER, (UINT)n + 1, buf, 0);
        CHECK(buf[n + 1] == 0xCDCD);
    }
}
TEST(spstruct_deskwallpaper_exact_size) {
    WCHAR full[MAX_PATH], buf[MAX_PATH + 8]; size_t n;
    REQUIRE(U(SystemParametersInfoW));
    memset(full, 0, sizeof full);
    CHECK(sW(SPI_GETDESKWALLPAPER, MAX_PATH, full, 0));
    n = wcslen(full);
    if (!n) { t_note("no wallpaper set"); return; }
    memset(buf, 0, sizeof buf);
    CHECK(sW(SPI_GETDESKWALLPAPER, (UINT)n + 1, buf, 0));
    CHECK(!wcscmp(buf, full));
    memset(buf, 0, sizeof buf);
    {
        BOOL r = sW(SPI_GETDESKWALLPAPER, (UINT)n, buf, 0);
        t_note("wallpaper exact-1 buffer -> %d len=%zu", r, wcslen(buf));
    }
}
