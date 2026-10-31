#include "ctl.h"

#ifndef SBM_GETSCROLLBARINFO
#define SBM_GETSCROLLBARINFO 0x00EB
#endif

/* top-level window carrying standard scroll bars (created hidden unless shown is set) */
static HWND mkwin(DWORD style, int shown)
{
    HWND h = U(CreateWindowExA)(0, c_parent_cls(), "sb", WS_OVERLAPPEDWINDOW | style, 20, 20, 300, 250,
                                NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (h && shown) { U(ShowWindow)(h, SW_SHOWNOACTIVATE); U(UpdateWindow)(h); t_pump(); }
    return h;
}
static HWND mksb(HWND p, DWORD style, int id)
{
    return c_ctl(p, "ScrollBar", "", style, id, 10, 10, (style & SBS_VERT) ? 17 : 150, (style & SBS_VERT) ? 150 : 17);
}

TEST(scroll_constants)
{
    EXPECT_EQ(SB_HORZ, 0); EXPECT_EQ(SB_VERT, 1); EXPECT_EQ(SB_CTL, 2); EXPECT_EQ(SB_BOTH, 3);
    EXPECT_EQ(SB_LINEUP, 0); EXPECT_EQ(SB_LINEDOWN, 1); EXPECT_EQ(SB_PAGEUP, 2); EXPECT_EQ(SB_PAGEDOWN, 3);
    EXPECT_EQ(SB_THUMBPOSITION, 4); EXPECT_EQ(SB_THUMBTRACK, 5); EXPECT_EQ(SB_TOP, 6); EXPECT_EQ(SB_BOTTOM, 7);
    EXPECT_EQ(SB_ENDSCROLL, 8); EXPECT_EQ(SB_LEFT, 6); EXPECT_EQ(SB_RIGHT, 7);
    EXPECT_EQ(SIF_RANGE, 1); EXPECT_EQ(SIF_PAGE, 2); EXPECT_EQ(SIF_POS, 4); EXPECT_EQ(SIF_DISABLENOSCROLL, 8);
    EXPECT_EQ(SIF_TRACKPOS, 0x10); EXPECT_EQ(SIF_ALL, 0x17);
    EXPECT_EQ(ESB_ENABLE_BOTH, 0); EXPECT_EQ(ESB_DISABLE_BOTH, 3); EXPECT_EQ(ESB_DISABLE_LTUP, 1);
    EXPECT_EQ(ESB_DISABLE_RTDN, 2); EXPECT_EQ(ESB_DISABLE_LEFT, 1); EXPECT_EQ(ESB_DISABLE_RIGHT, 2);
    EXPECT_EQ(ESB_DISABLE_UP, 1); EXPECT_EQ(ESB_DISABLE_DOWN, 2);
    EXPECT_EQ(SBM_SETPOS, 0xE0); EXPECT_EQ(SBM_GETPOS, 0xE1); EXPECT_EQ(SBM_SETRANGE, 0xE2);
    EXPECT_EQ(SBM_SETRANGEREDRAW, 0xE6); EXPECT_EQ(SBM_GETRANGE, 0xE3); EXPECT_EQ(SBM_ENABLE_ARROWS, 0xE4);
    EXPECT_EQ(SBM_SETSCROLLINFO, 0xE9); EXPECT_EQ(SBM_GETSCROLLINFO, 0xEA);
    EXPECT_EQ(SBS_HORZ, 0); EXPECT_EQ(SBS_VERT, 1); EXPECT_EQ(SBS_TOPALIGN, 2); EXPECT_EQ(SBS_LEFTALIGN, 2);
    EXPECT_EQ(SBS_BOTTOMALIGN, 4); EXPECT_EQ(SBS_RIGHTALIGN, 4); EXPECT_EQ(SBS_SIZEBOX, 8); EXPECT_EQ(SBS_SIZEGRIP, 0x10);
    EXPECT_EQ(sizeof(SCROLLINFO), 28); EXPECT_EQ(sizeof(SCROLLBARINFO), 60);
    EXPECT_EQ(OBJID_HSCROLL, -6); EXPECT_EQ(OBJID_VSCROLL, -5); EXPECT_EQ(OBJID_CLIENT, -4);
}

/* ---- window scroll bars: SetScrollInfo / GetScrollInfo math ---- */
static const struct { int min, max; UINT page; int pos; int emin, emax; UINT epage; int epos; } k_math[] = {
    { 0, 100, 0, 50, 0, 100, 0, 50 },
    { 0, 100, 0, 0, 0, 100, 0, 0 },
    { 0, 100, 0, 100, 0, 100, 0, 100 },
    { 0, 100, 0, 101, 0, 100, 0, 100 },
    { 0, 100, 0, -5, 0, 100, 0, 0 },
    { 0, 100, 10, 95, 0, 100, 10, 91 },
    { 0, 100, 10, 91, 0, 100, 10, 91 },
    { 0, 100, 10, 92, 0, 100, 10, 91 },
    { 0, 100, 10, -5, 0, 100, 10, 0 },
    { 0, 100, 101, 50, 0, 100, 101, 0 },
    { 0, 100, 500, 50, 0, 100, 101, 0 },
    { 0, 100, 1, 100, 0, 100, 1, 100 },
    { 0, 100, 2, 100, 0, 100, 2, 99 },
    { 10, 20, 3, 18, 10, 20, 3, 18 },
    { 10, 20, 3, 19, 10, 20, 3, 18 },
    { 10, 20, 3, 5, 10, 20, 3, 10 },
    { -50, 50, 20, 40, -50, 50, 20, 31 },
    { -50, 50, 20, -100, -50, 50, 20, -50 },
    { -50, -10, 5, -9, -50, -10, 5, -14 },
    { 0, 0, 0, 5, 0, 0, 0, 0 },
    { 5, 5, 1, 5, 5, 5, 1, 5 },
    { 0, 1000000, 1000, 999999, 0, 1000000, 1000, 999001 },
    { 0, 32767, 100, 40000, 0, 32767, 100, 32668 },
    { 0, 65535, 10, 70000, 0, 65535, 10, 65526 },
    { -1000000, 1000000, 500000, 1000000, -1000000, 1000000, 500000, 500001 },
    { 0x7FFFFF00, 0x7FFFFFF0, 4, 0x7FFFFFFF, 0x7FFFFF00, 0x7FFFFFF0, 4, 0x7FFFFFED },
};

TEST(scroll_window_setscrollinfo_math)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo)); REQUIRE(U(GetScrollInfo));
    HWND w = mkwin(WS_VSCROLL | WS_HSCROLL, 0);
    for (int bar = 0; bar < 2; bar++) {
        for (unsigned i = 0; i < sizeof k_math / sizeof k_math[0]; i++) {
            SCROLLINFO si; memset(&si, 0, sizeof si);
            si.cbSize = sizeof si; si.fMask = SIF_ALL & ~SIF_TRACKPOS;
            si.nMin = k_math[i].min; si.nMax = k_math[i].max; si.nPage = k_math[i].page; si.nPos = k_math[i].pos;
            int r = U(SetScrollInfo)(w, bar, &si, FALSE);
            CHECKF(r == k_math[i].epos, "bar %d case %u: set returned %d want %d", bar, i, r, k_math[i].epos);
            memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL;
            CHECKF(U(GetScrollInfo)(w, bar, &si), "bar %d case %u get", bar, i);
            CHECKF(si.nMin == k_math[i].emin && si.nMax == k_math[i].emax && si.nPage == k_math[i].epage && si.nPos == k_math[i].epos,
                   "bar %d case %u: got min=%d max=%d page=%u pos=%d", bar, i, si.nMin, si.nMax, si.nPage, si.nPos);
            CHECKF(si.nTrackPos == si.nPos || si.nTrackPos == 0, "bar %d case %u trackpos %d", bar, i, si.nTrackPos);
            EXPECT_EQ(U(GetScrollPos)(w, bar), k_math[i].epos);
        }
    }
    U(DestroyWindow)(w);
}

TEST(scroll_window_partial_masks)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo)); REQUIRE(U(GetScrollInfo));
    HWND w = mkwin(WS_VSCROLL, 0);
    SCROLLINFO si;
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL & ~SIF_TRACKPOS;
    si.nMin = 0; si.nMax = 100; si.nPage = 10; si.nPos = 50;
    U(SetScrollInfo)(w, SB_VERT, &si, FALSE);
    /* position only: clamped to current range and page */
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_POS; si.nPos = 500;
    EXPECT_EQ(U(SetScrollInfo)(w, SB_VERT, &si, FALSE), 91);
    si.nPos = 20;
    EXPECT_EQ(U(SetScrollInfo)(w, SB_VERT, &si, FALSE), 20);
    /* page only: re-clamps the position */
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_PAGE; si.nPage = 90;
    EXPECT_EQ(U(SetScrollInfo)(w, SB_VERT, &si, FALSE), 11);   /* max pos = 100 - 90 + 1 */
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL;
    U(GetScrollInfo)(w, SB_VERT, &si);
    EXPECT_EQ(si.nPage, 90); EXPECT_EQ(si.nPos, 11); EXPECT_EQ(si.nMax, 100); EXPECT_EQ(si.nMin, 0);
    /* range only: shrinking the range clamps pos */
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_PAGE; si.nPage = 0;
    U(SetScrollInfo)(w, SB_VERT, &si, FALSE);
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_POS; si.nPos = 80;
    U(SetScrollInfo)(w, SB_VERT, &si, FALSE);
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_RANGE; si.nMin = 0; si.nMax = 30;
    U(SetScrollInfo)(w, SB_VERT, &si, FALSE);
    EXPECT_EQ(U(GetScrollPos)(w, SB_VERT), 30);
    /* partial GetScrollInfo leaves other members alone */
    memset(&si, 0x5A, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_POS;
    CHECK(U(GetScrollInfo)(w, SB_VERT, &si));
    EXPECT_EQ(si.nPos, 30);
    EXPECT_EQ(si.nMin, 0x5A5A5A5A); EXPECT_EQ(si.nMax, 0x5A5A5A5A); EXPECT_EQ(si.nPage, 0x5A5A5A5A);
    memset(&si, 0x5A, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_RANGE;
    CHECK(U(GetScrollInfo)(w, SB_VERT, &si));
    EXPECT_EQ(si.nMin, 0); EXPECT_EQ(si.nMax, 30);
    EXPECT_EQ(si.nPos, 0x5A5A5A5A); EXPECT_EQ(si.nPage, 0x5A5A5A5A);
    memset(&si, 0x5A, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_PAGE;
    CHECK(U(GetScrollInfo)(w, SB_VERT, &si));
    EXPECT_EQ(si.nPage, 0); EXPECT_EQ(si.nMin, 0x5A5A5A5A);
    U(DestroyWindow)(w);
}

TEST(scroll_window_independent_bars)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo)); REQUIRE(U(GetScrollInfo));
    HWND w = mkwin(WS_VSCROLL | WS_HSCROLL, 0);
    SCROLLINFO si;
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_RANGE | SIF_POS;
    si.nMin = 0; si.nMax = 200; si.nPos = 150;
    U(SetScrollInfo)(w, SB_HORZ, &si, FALSE);
    si.nMin = 10; si.nMax = 20; si.nPos = 12;
    U(SetScrollInfo)(w, SB_VERT, &si, FALSE);
    EXPECT_EQ(U(GetScrollPos)(w, SB_HORZ), 150);
    EXPECT_EQ(U(GetScrollPos)(w, SB_VERT), 12);
    int mn, mx;
    CHECK(U(GetScrollRange)(w, SB_HORZ, &mn, &mx)); EXPECT_EQ(mn, 0); EXPECT_EQ(mx, 200);
    CHECK(U(GetScrollRange)(w, SB_VERT, &mn, &mx)); EXPECT_EQ(mn, 10); EXPECT_EQ(mx, 20);
    U(DestroyWindow)(w);
}

TEST(scroll_window_default_range)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetScrollRange)); REQUIRE(U(GetScrollInfo));
    DWORD sets[] = { WS_VSCROLL, WS_HSCROLL, WS_VSCROLL | WS_HSCROLL, 0 };
    for (int i = 0; i < 4; i++) {
        HWND w = mkwin(sets[i], 0);
        int mn = -7, mx = -7;
        CHECK(U(GetScrollRange)(w, SB_VERT, &mn, &mx));
        if (sets[i] == 0) { EXPECT_EQ(mn, 0); EXPECT_EQ(mx, 0); }
        else { EXPECT_EQ(mn, 0); EXPECT_EQ(mx, 100); }
        EXPECT_EQ(U(GetScrollPos)(w, SB_VERT), 0);
        SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL;
        BOOL r = U(GetScrollInfo)(w, SB_VERT, &si);
        if (sets[i] == 0) CHECK(!r);
        else CHECK(r);
        U(DestroyWindow)(w);
    }
}

TEST(scroll_window_setscrollrange_pos)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollRange)); REQUIRE(U(SetScrollPos));
    HWND w = mkwin(WS_VSCROLL, 0);
    CHECK(U(SetScrollRange)(w, SB_VERT, 10, 60, FALSE));
    int mn, mx;
    CHECK(U(GetScrollRange)(w, SB_VERT, &mn, &mx));
    EXPECT_EQ(mn, 10); EXPECT_EQ(mx, 60);
    /* SetScrollPos returns the previous position */
    int prev = U(GetScrollPos)(w, SB_VERT);
    EXPECT_EQ(U(SetScrollPos)(w, SB_VERT, 30, FALSE), prev);
    EXPECT_EQ(U(GetScrollPos)(w, SB_VERT), 30);
    EXPECT_EQ(U(SetScrollPos)(w, SB_VERT, 45, FALSE), 30);
    EXPECT_EQ(U(SetScrollPos)(w, SB_VERT, 1000, FALSE), 45);
    EXPECT_EQ(U(GetScrollPos)(w, SB_VERT), 60);
    EXPECT_EQ(U(SetScrollPos)(w, SB_VERT, -1000, FALSE), 60);
    EXPECT_EQ(U(GetScrollPos)(w, SB_VERT), 10);
    /* shrinking the range clamps the position */
    U(SetScrollPos)(w, SB_VERT, 55, FALSE);
    CHECK(U(SetScrollRange)(w, SB_VERT, 0, 20, FALSE));
    EXPECT_EQ(U(GetScrollPos)(w, SB_VERT), 20);
    /* identical min and max */
    CHECK(U(SetScrollRange)(w, SB_VERT, 7, 7, FALSE));
    EXPECT_EQ(U(GetScrollPos)(w, SB_VERT), 7);
    U(DestroyWindow)(w);
}

TEST(scroll_window_setscrollrange_limits)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollRange));
    HWND w = mkwin(WS_VSCROLL, 0);
    SetLastError(0);
    BOOL r = U(SetScrollRange)(w, SB_VERT, -1, 0x7FFFFFFF, FALSE);   /* span exceeds MAXLONG */
    t_note("SetScrollRange(span > MAXLONG) -> %d: err=%lu (ERROR_INVALID_SCROLLBAR_RANGE=1448)", r, GetLastError());
    SetLastError(0);
    r = U(SetScrollRange)(w, SB_VERT, 0, 0x7FFFFFFF, FALSE);        /* exactly MAXLONG */
    CHECK(r);
    int mn, mx;
    U(GetScrollRange)(w, SB_VERT, &mn, &mx);
    EXPECT_EQ(mn, 0); EXPECT_EQ(mx, 0x7FFFFFFF);
    r = U(SetScrollRange)(w, SB_VERT, -0x40000000, 0x3FFFFFFF, FALSE);
    CHECK(r);
    U(GetScrollRange)(w, SB_VERT, &mn, &mx);
    EXPECT_EQ(mn, -0x40000000); EXPECT_EQ(mx, 0x3FFFFFFF);
    /* inverted range */
    r = U(SetScrollRange)(w, SB_VERT, 50, 10, FALSE);
    U(GetScrollRange)(w, SB_VERT, &mn, &mx);
    t_note("SetScrollRange(50,10) -> ret %d, range %d..%d", r, mn, mx);
    U(DestroyWindow)(w);
}

TEST(scroll_window_invalid_arguments)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo)); REQUIRE(U(GetScrollInfo));
    HWND w = mkwin(WS_VSCROLL | WS_HSCROLL, 0);
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL;
    si.nMax = 10;
    /* bogus window */
    SetLastError(0);
    EXPECT_EQ(U(SetScrollInfo)(NULL, SB_VERT, &si, FALSE), 0);
    t_note("SetScrollInfo(NULL) err=%lu", GetLastError());
    EXPECT_EQ(U(SetScrollInfo)((HWND)(ULONG_PTR)0xdead0, SB_VERT, &si, FALSE), 0);
    SetLastError(0);
    CHECK(!U(GetScrollInfo)(NULL, SB_VERT, &si));
    t_note("GetScrollInfo(NULL) err=%lu", GetLastError());
    CHECK(!U(GetScrollInfo)((HWND)(ULONG_PTR)0xdead0, SB_VERT, &si));
    /* GetScrollInfo does not accept SB_BOTH or garbage bars */
    CHECK(!U(GetScrollInfo)(w, SB_BOTH, &si));
    CHECK(!U(GetScrollInfo)(w, 4, &si));
    CHECK(!U(GetScrollInfo)(w, -1, &si));
    CHECK(!U(GetScrollInfo)(w, 0x100, &si));
    /* wrong structure size */
    si.cbSize = 0;
    SetLastError(0);
    BOOL r = U(GetScrollInfo)(w, SB_VERT, &si);
    t_note("GetScrollInfo(cbSize=0) -> %d err=%lu", r, GetLastError());
    CHECK(!r);
    si.cbSize = sizeof si - 4;
    r = U(GetScrollInfo)(w, SB_VERT, &si);
    t_note("GetScrollInfo(cbSize-4) -> %d", r);
    si.cbSize = sizeof si + 4;
    r = U(GetScrollInfo)(w, SB_VERT, &si);
    t_note("GetScrollInfo(cbSize+4) -> %d", r);
    si.cbSize = 0;
    SetLastError(0);
    int sr = U(SetScrollInfo)(w, SB_VERT, &si, FALSE);
    t_note("SetScrollInfo(cbSize=0) -> %d err=%lu", sr, GetLastError());
    /* invalid fMask bits */
    si.cbSize = sizeof si; si.fMask = 0x80;
    r = U(GetScrollInfo)(w, SB_VERT, &si);
    t_note("GetScrollInfo(fMask=0x80) -> %d", r);
    U(DestroyWindow)(w);
}

TEST(scroll_window_sb_both_set)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo));
    HWND w = mkwin(WS_VSCROLL | WS_HSCROLL, 0);
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_RANGE | SIF_POS;
    si.nMin = 0; si.nMax = 40; si.nPos = 25;
    int r = U(SetScrollInfo)(w, SB_BOTH, &si, FALSE);
    t_note("SetScrollInfo(SB_BOTH) -> %d, horz pos %d vert pos %d", r, U(GetScrollPos)(w, SB_HORZ), U(GetScrollPos)(w, SB_VERT));
    U(DestroyWindow)(w);
}

TEST(scroll_window_trackpos_not_tracking)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo)); REQUIRE(U(GetScrollInfo));
    HWND w = mkwin(WS_VSCROLL, 0);
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_RANGE | SIF_POS;
    si.nMin = 0; si.nMax = 100; si.nPos = 33;
    U(SetScrollInfo)(w, SB_VERT, &si, FALSE);
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_TRACKPOS;
    CHECK(U(GetScrollInfo)(w, SB_VERT, &si));
    t_note("nTrackPos when not tracking = %d (pos 33)", si.nTrackPos);
    CHECK(si.nTrackPos == 0 || si.nTrackPos == 33);
    U(DestroyWindow)(w);
}

TEST(scroll_window_style_bits_and_showscrollbar)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(ShowScrollBar)); REQUIRE(U(GetWindowLongA));
    HWND w = mkwin(0, 0);
    LONG st = U(GetWindowLongA)(w, GWL_STYLE);
    CHECK(!(st & (WS_VSCROLL | WS_HSCROLL)));
    CHECK(U(ShowScrollBar)(w, SB_VERT, TRUE));
    st = U(GetWindowLongA)(w, GWL_STYLE);
    CHECK(st & WS_VSCROLL); CHECK(!(st & WS_HSCROLL));
    CHECK(U(ShowScrollBar)(w, SB_HORZ, TRUE));
    CHECK(U(GetWindowLongA)(w, GWL_STYLE) & WS_HSCROLL);
    CHECK(U(ShowScrollBar)(w, SB_VERT, FALSE));
    st = U(GetWindowLongA)(w, GWL_STYLE);
    CHECK(!(st & WS_VSCROLL)); CHECK(st & WS_HSCROLL);
    CHECK(U(ShowScrollBar)(w, SB_BOTH, FALSE));
    CHECK(!(U(GetWindowLongA)(w, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL)));
    CHECK(U(ShowScrollBar)(w, SB_BOTH, TRUE));
    st = U(GetWindowLongA)(w, GWL_STYLE);
    CHECK((st & WS_VSCROLL) && (st & WS_HSCROLL));
    /* idempotent */
    CHECK(U(ShowScrollBar)(w, SB_BOTH, TRUE));
    CHECK(!U(ShowScrollBar)(NULL, SB_VERT, TRUE));
    CHECK(!U(ShowScrollBar)((HWND)(ULONG_PTR)0xdead0, SB_VERT, TRUE));
    U(DestroyWindow)(w);
}

TEST(scroll_window_disablenoscroll_style_effects)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo)); REQUIRE(U(GetWindowLongA));
    HWND w = mkwin(WS_VSCROLL, 1);
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0; si.nMax = 100; si.nPage = 10;
    U(SetScrollInfo)(w, SB_VERT, &si, TRUE);
    CHECK(U(GetWindowLongA)(w, GWL_STYLE) & WS_VSCROLL);
    /* page covers the whole range: bar not needed */
    si.nPage = 101;
    U(SetScrollInfo)(w, SB_VERT, &si, TRUE);
    t_note("WS_VSCROLL after page>=range without DISABLENOSCROLL: %d", (U(GetWindowLongA)(w, GWL_STYLE) & WS_VSCROLL) != 0);
    si.nPage = 10;
    U(SetScrollInfo)(w, SB_VERT, &si, TRUE);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL; si.nPage = 101;
    U(SetScrollInfo)(w, SB_VERT, &si, TRUE);
    int kept = (U(GetWindowLongA)(w, GWL_STYLE) & WS_VSCROLL) != 0;
    t_note("WS_VSCROLL after page>=range with DISABLENOSCROLL: %d", kept);
    CHECK(kept);
    U(DestroyWindow)(w);
}

TEST(scroll_window_enablescrollbar)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(EnableScrollBar));
    HWND w = mkwin(WS_VSCROLL | WS_HSCROLL, 1);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_VERT, ESB_DISABLE_BOTH), TRUE);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_VERT, ESB_DISABLE_BOTH), FALSE);   /* already in that state */
    EXPECT_EQ(U(EnableScrollBar)(w, SB_VERT, ESB_ENABLE_BOTH), TRUE);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_VERT, ESB_ENABLE_BOTH), FALSE);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_HORZ, ESB_DISABLE_LTUP), TRUE);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_HORZ, ESB_DISABLE_LTUP), FALSE);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_HORZ, ESB_DISABLE_RTDN), TRUE);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_HORZ, ESB_ENABLE_BOTH), TRUE);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_BOTH, ESB_DISABLE_BOTH), TRUE);
    EXPECT_EQ(U(EnableScrollBar)(w, SB_BOTH, ESB_ENABLE_BOTH), TRUE);
    /* window itself is not disabled */
    CHECK(U(IsWindowEnabled)(w));
    EXPECT_EQ(U(EnableScrollBar)(NULL, SB_VERT, ESB_DISABLE_BOTH), FALSE);
    EXPECT_EQ(U(EnableScrollBar)((HWND)(ULONG_PTR)0xdead0, SB_VERT, ESB_DISABLE_BOTH), FALSE);
    U(DestroyWindow)(w);
}

TEST(scroll_window_getscrollbarinfo)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetScrollBarInfo)); REQUIRE(U(GetClientRect));
    HWND w = mkwin(WS_VSCROLL | WS_HSCROLL, 1);
    SCROLLBARINFO sb;
    RECT cl;
    memset(&sb, 0, sizeof sb); sb.cbSize = sizeof sb;
    CHECK(U(GetScrollBarInfo)(w, OBJID_VSCROLL, &sb));
    U(GetClientRect)(w, &cl);
    EXPECT_EQ(sb.rcScrollBar.bottom - sb.rcScrollBar.top, cl.bottom - cl.top);
    EXPECT_EQ(sb.rcScrollBar.right - sb.rcScrollBar.left, U(GetSystemMetrics)(SM_CXVSCROLL));
    CHECK(!(sb.rgstate[0] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN)));
    CHECK(sb.dxyLineButton > 0);
    CHECK(sb.xyThumbTop >= sb.dxyLineButton);
    CHECK(sb.xyThumbBottom > sb.xyThumbTop);
    memset(&sb, 0, sizeof sb); sb.cbSize = sizeof sb;
    CHECK(U(GetScrollBarInfo)(w, OBJID_HSCROLL, &sb));
    EXPECT_EQ(sb.rcScrollBar.right - sb.rcScrollBar.left, cl.right - cl.left);
    EXPECT_EQ(sb.rcScrollBar.bottom - sb.rcScrollBar.top, U(GetSystemMetrics)(SM_CYHSCROLL));
    /* moving the window moves the reported rectangle (screen coordinates) */
    RECT before = sb.rcScrollBar, wr;
    U(GetWindowRect)(w, &wr);
    U(MoveWindow)(w, wr.left + 7, wr.top + 9, wr.right - wr.left, wr.bottom - wr.top, TRUE);
    memset(&sb, 0, sizeof sb); sb.cbSize = sizeof sb;
    CHECK(U(GetScrollBarInfo)(w, OBJID_HSCROLL, &sb));
    EXPECT_EQ(sb.rcScrollBar.left - before.left, 7);
    EXPECT_EQ(sb.rcScrollBar.top - before.top, 9);
    /* invalid usage */
    memset(&sb, 0, sizeof sb); sb.cbSize = sizeof sb - 4;
    CHECK(!U(GetScrollBarInfo)(w, OBJID_VSCROLL, &sb));
    sb.cbSize = 0;
    CHECK(!U(GetScrollBarInfo)(w, OBJID_VSCROLL, &sb));
    sb.cbSize = sizeof sb;
    CHECK(!U(GetScrollBarInfo)(NULL, OBJID_VSCROLL, &sb));
    CHECK(!U(GetScrollBarInfo)(w, OBJID_WINDOW, &sb));
    CHECK(!U(GetScrollBarInfo)(w, 12345, &sb));
    U(DestroyWindow)(w);
}

/* ---- scroll bar controls ---- */
TEST(scrollctl_classname_style)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 300);
    HWND h = mksb(p, SBS_HORZ, 1), v = mksb(p, SBS_VERT, 2);
    char n[32];
    c_classname(h, n, sizeof n);
    EXPECT_STR(n, "ScrollBar");
    CHECK(!(U(GetWindowLongA)(h, GWL_STYLE) & SBS_VERT));
    CHECK(U(GetWindowLongA)(v, GWL_STYLE) & SBS_VERT);
    t_note("scroll bar WM_GETDLGCODE = %llx", (long long)SA(h, WM_GETDLGCODE, 0, 0));
    U(DestroyWindow)(p);
}

TEST(scrollctl_sbm_pos_range)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND s = mksb(p, SBS_HORZ, 1);
    t_note("default control range/pos: pos=%lld", (long long)SA(s, SBM_GETPOS, 0, 0));
    EXPECT_EQ(SA(s, SBM_SETRANGE, 0, 100), 0);
    int mn = -1, mx = -1;
    SA(s, SBM_GETRANGE, &mn, &mx);
    EXPECT_EQ(mn, 0); EXPECT_EQ(mx, 100);
    EXPECT_EQ(SA(s, SBM_SETPOS, 40, FALSE), 0);        /* returns previous position */
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 40);
    EXPECT_EQ(SA(s, SBM_SETPOS, 60, TRUE), 40);
    EXPECT_EQ(SA(s, SBM_SETPOS, 500, FALSE), 60);
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 100);           /* clamped to max */
    EXPECT_EQ(SA(s, SBM_SETPOS, -20, FALSE), 100);
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 0);             /* clamped to min */
    /* SBM_SETRANGE clamps the position */
    SA(s, SBM_SETPOS, 90, FALSE);
    SA(s, SBM_SETRANGE, 10, 50);
    SA(s, SBM_GETRANGE, &mn, &mx);
    EXPECT_EQ(mn, 10); EXPECT_EQ(mx, 50);
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 50);
    SA(s, SBM_SETRANGEREDRAW, -30, 30);
    SA(s, SBM_GETRANGE, &mn, &mx);
    EXPECT_EQ(mn, -30); EXPECT_EQ(mx, 30);
    /* the API functions see the same state */
    int a, b;
    CHECK(U(GetScrollRange)(s, SB_CTL, &a, &b));
    EXPECT_EQ(a, -30); EXPECT_EQ(b, 30);
    EXPECT_EQ(U(GetScrollPos)(s, SB_CTL), SA(s, SBM_GETPOS, 0, 0));
    EXPECT_EQ(U(SetScrollPos)(s, SB_CTL, 11, FALSE), 30);
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 11);
    U(DestroyWindow)(p);
}

TEST(scrollctl_sbm_scrollinfo)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND s = mksb(p, SBS_VERT, 1);
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL & ~SIF_TRACKPOS;
    si.nMin = 0; si.nMax = 200; si.nPage = 20; si.nPos = 300;
    EXPECT_EQ(SA(s, SBM_SETSCROLLINFO, FALSE, &si), 181);   /* 200 - 20 + 1 */
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL;
    EXPECT_EQ(SA(s, SBM_GETSCROLLINFO, 0, &si), TRUE);
    EXPECT_EQ(si.nMin, 0); EXPECT_EQ(si.nMax, 200); EXPECT_EQ(si.nPage, 20); EXPECT_EQ(si.nPos, 181);
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 181);
    int mn, mx; SA(s, SBM_GETRANGE, &mn, &mx);
    EXPECT_EQ(mn, 0); EXPECT_EQ(mx, 200);
    /* API and message agree */
    SCROLLINFO s2; memset(&s2, 0, sizeof s2); s2.cbSize = sizeof s2; s2.fMask = SIF_ALL;
    CHECK(U(GetScrollInfo)(s, SB_CTL, &s2));
    CHECK(s2.nMin == si.nMin && s2.nMax == si.nMax && s2.nPage == si.nPage && s2.nPos == si.nPos);
    s2.fMask = SIF_POS; s2.nPos = 7;
    EXPECT_EQ(U(SetScrollInfo)(s, SB_CTL, &s2, FALSE), 7);
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 7);
    U(DestroyWindow)(p);
}

TEST(scrollctl_enable_arrows_and_window_state)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(EnableScrollBar)); REQUIRE(U(IsWindowEnabled));
    HWND p = c_parent(300, 300);
    HWND s = mksb(p, SBS_HORZ, 1);
    EXPECT_EQ(U(EnableScrollBar)(s, SB_CTL, ESB_DISABLE_BOTH), TRUE);
    CHECK(!U(IsWindowEnabled)(s));
    EXPECT_EQ(U(EnableScrollBar)(s, SB_CTL, ESB_ENABLE_BOTH), TRUE);
    CHECK(U(IsWindowEnabled)(s));
    EXPECT_EQ(U(EnableScrollBar)(s, SB_CTL, ESB_DISABLE_LTUP), TRUE);
    CHECK(U(IsWindowEnabled)(s));
    EXPECT_EQ(U(EnableScrollBar)(s, SB_CTL, ESB_ENABLE_BOTH), TRUE);
    EXPECT_EQ(U(EnableScrollBar)(s, SB_CTL, ESB_DISABLE_RTDN), TRUE);
    CHECK(U(IsWindowEnabled)(s));
    EXPECT_EQ(U(EnableScrollBar)(s, SB_CTL, ESB_ENABLE_BOTH), TRUE);
    /* message form */
    LRESULT r = SA(s, SBM_ENABLE_ARROWS, ESB_DISABLE_BOTH, 0);
    EXPECT_EQ(r, TRUE);
    r = SA(s, SBM_ENABLE_ARROWS, ESB_DISABLE_BOTH, 0);
    EXPECT_EQ(r, FALSE);
    EXPECT_EQ(SA(s, SBM_ENABLE_ARROWS, ESB_ENABLE_BOTH, 0), TRUE);
    /* disabled control: SetScrollPos still stores the position once a range exists */
    U(EnableWindow)(s, FALSE);
    SA(s, SBM_SETRANGE, 0, 100);
    t_note("SBM_SETPOS on disabled control -> %lld", (long long)SA(s, SBM_SETPOS, 30, FALSE));
    t_note("pos = %lld", (long long)SA(s, SBM_GETPOS, 0, 0));
    U(EnableWindow)(s, TRUE);
    SA(s, SBM_SETPOS, 30, FALSE);
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 30);
    U(DestroyWindow)(p);
}

TEST(scrollctl_showscrollbar)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(ShowScrollBar)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 300);
    HWND s = mksb(p, SBS_HORZ, 1);
    CHECK(U(GetWindowLongA)(s, GWL_STYLE) & WS_VISIBLE);
    CHECK(U(ShowScrollBar)(s, SB_CTL, FALSE));
    CHECK(!(U(GetWindowLongA)(s, GWL_STYLE) & WS_VISIBLE));
    CHECK(U(ShowScrollBar)(s, SB_CTL, TRUE));
    CHECK(U(GetWindowLongA)(s, GWL_STYLE) & WS_VISIBLE);
    CHECK(U(ShowScrollBar)(p, SB_CTL, TRUE));   /* non-control window with SB_CTL: succeeds */
    U(DestroyWindow)(p);
}

TEST(scrollctl_getscrollbarinfo)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetScrollBarInfo)); REQUIRE(U(GetWindowRect));
    HWND p = c_parent_shown(300, 300);
    HWND s = mksb(p, SBS_VERT, 1);
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL & ~SIF_TRACKPOS;
    si.nMax = 100; si.nPage = 10; si.nPos = 50;
    SA(s, SBM_SETSCROLLINFO, TRUE, &si);
    SCROLLBARINFO sb; memset(&sb, 0, sizeof sb); sb.cbSize = sizeof sb;
    CHECK(U(GetScrollBarInfo)(s, OBJID_CLIENT, &sb));
    RECT wr; U(GetWindowRect)(s, &wr);
    CHECK(EqualRect(&wr, &sb.rcScrollBar));
    CHECK(sb.dxyLineButton > 0);
    CHECK(sb.xyThumbTop > sb.dxyLineButton && sb.xyThumbBottom > sb.xyThumbTop);
    CHECK(sb.xyThumbBottom <= (wr.bottom - wr.top) - sb.dxyLineButton);
    int mid = (sb.xyThumbTop + sb.xyThumbBottom) / 2;
    CHECK(mid > sb.dxyLineButton && mid < (wr.bottom - wr.top) - sb.dxyLineButton);
    /* message form gives the same data */
    SCROLLBARINFO m; memset(&m, 0, sizeof m); m.cbSize = sizeof m;
    LRESULT r = SA(s, SBM_GETSCROLLBARINFO, 0, &m);
    t_note("SBM_GETSCROLLBARINFO -> %lld", (long long)r);
    if (r) { CHECK(m.xyThumbTop == sb.xyThumbTop && m.xyThumbBottom == sb.xyThumbBottom); }
    /* thumb moves with position */
    si.fMask = SIF_POS; si.nPos = 0;
    SA(s, SBM_SETSCROLLINFO, TRUE, &si);
    SCROLLBARINFO sb0; memset(&sb0, 0, sizeof sb0); sb0.cbSize = sizeof sb0;
    U(GetScrollBarInfo)(s, OBJID_CLIENT, &sb0);
    CHECK(sb0.xyThumbTop < sb.xyThumbTop);
    si.nPos = 91;
    SA(s, SBM_SETSCROLLINFO, TRUE, &si);
    U(GetScrollBarInfo)(s, OBJID_CLIENT, &sb0);
    CHECK(sb0.xyThumbBottom > sb.xyThumbBottom);
    /* a disabled control reports unavailable state */
    U(EnableWindow)(s, FALSE);
    memset(&sb0, 0, sizeof sb0); sb0.cbSize = sizeof sb0;
    U(GetScrollBarInfo)(s, OBJID_CLIENT, &sb0);
    t_note("disabled scrollbar rgstate[0]=0x%lx", sb0.rgstate[0]);
    U(DestroyWindow)(p);
}

TEST(scrollctl_keyboard_sends_scroll_codes)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND v = mksb(p, SBS_VERT, 1), h = mksb(p, SBS_HORZ, 2);
    static const struct { int vk; int code; } t[] = {
        { VK_UP, SB_LINEUP }, { VK_DOWN, SB_LINEDOWN }, { VK_PRIOR, SB_PAGEUP }, { VK_NEXT, SB_PAGEDOWN },
        { VK_HOME, SB_TOP }, { VK_END, SB_BOTTOM },
    };
    SA(v, SBM_SETRANGE, 0, 100);
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        c_logclear();
        SA(v, WM_KEYDOWN, t[i].vk, 0);
        int n = c_nmsg(WM_VSCROLL);
        CHECKF(n >= 1, "vk %x: no WM_VSCROLL", t[i].vk);
        if (n >= 1) {
            int k = 0;
            while (k < c_nlogent && c_log[k].msg != WM_VSCROLL) k++;
            CHECKF(LOWORD(c_log[k].wp) == t[i].code, "vk %x code %u want %d", t[i].vk, LOWORD(c_log[k].wp), t[i].code);
            CHECK(c_log[k].lp == (LPARAM)v);
        }
        EXPECT_EQ(c_nmsg(WM_HSCROLL), 0);
        SA(v, WM_KEYUP, t[i].vk, 0);
    }
    /* horizontal control: arrows map to line up/down, sends WM_HSCROLL */
    SA(h, SBM_SETRANGE, 0, 100);
    c_logclear();
    SA(h, WM_KEYDOWN, VK_LEFT, 0);
    SA(h, WM_KEYUP, VK_LEFT, 0);
    int k = 0;
    while (k < c_nlogent && c_log[k].msg != WM_HSCROLL) k++;
    CHECK(k < c_nlogent);
    if (k < c_nlogent) { EXPECT_EQ(LOWORD(c_log[k].wp), SB_LINEUP); CHECK(c_log[k].lp == (LPARAM)h); }
    c_logclear();
    SA(h, WM_KEYDOWN, VK_RIGHT, 0);
    SA(h, WM_KEYUP, VK_RIGHT, 0);
    k = 0;
    while (k < c_nlogent && c_log[k].msg != WM_HSCROLL) k++;
    CHECK(k < c_nlogent);
    if (k < c_nlogent) EXPECT_EQ(LOWORD(c_log[k].wp), SB_LINEDOWN);
    /* the key up produces SB_ENDSCROLL */
    int end = 0;
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_HSCROLL && LOWORD(c_log[i].wp) == SB_ENDSCROLL) end++;
    t_note("SB_ENDSCROLL count after key up: %d", end);
    U(DestroyWindow)(p);
}

TEST(scrollctl_position_unchanged_by_keys)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND v = mksb(p, SBS_VERT, 1);
    SA(v, SBM_SETRANGE, 0, 100);
    SA(v, SBM_SETPOS, 30, FALSE);
    SA(v, WM_KEYDOWN, VK_DOWN, 0); SA(v, WM_KEYUP, VK_DOWN, 0);
    SA(v, WM_KEYDOWN, VK_NEXT, 0); SA(v, WM_KEYUP, VK_NEXT, 0);
    EXPECT_EQ(SA(v, SBM_GETPOS, 0, 0), 30);   /* the owner applies the change */
    U(DestroyWindow)(p);
}

TEST(scrollctl_sizebox_and_alignment_styles)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA)); REQUIRE(U(GetClientRect));
    HWND p = c_parent(300, 300);
    HWND sz = c_ctl(p, "ScrollBar", "", SBS_SIZEBOX, 1, 10, 10, 30, 30);
    CHECK(sz != NULL);
    CHECK((U(GetWindowLongA)(sz, GWL_STYLE) & SBS_SIZEBOX) == SBS_SIZEBOX);
    HWND g = c_ctl(p, "ScrollBar", "", SBS_SIZEGRIP, 2, 10, 10, 30, 30);
    CHECK(g != NULL);
    RECT r; U(GetClientRect)(sz, &r);
    t_note("sizebox client %ldx%ld", r.right, r.bottom);
    /* top/left aligned horizontal bar is sized to the system scroll bar height */
    HWND al = c_ctl(p, "ScrollBar", "", SBS_HORZ | SBS_TOPALIGN, 3, 10, 10, 100, 60);
    U(GetClientRect)(al, &r);
    t_note("SBS_TOPALIGN horizontal bar client %ldx%ld (SM_CYHSCROLL=%d)", r.right, r.bottom, U(GetSystemMetrics)(SM_CYHSCROLL));
    U(DestroyWindow)(p);
}

TEST(scrollctl_wm_enable_and_destroyed)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(IsWindow));
    HWND p = c_parent(300, 300);
    HWND s = mksb(p, SBS_HORZ, 1);
    SA(s, SBM_SETRANGE, 0, 10);
    U(DestroyWindow)(s);
    CHECK(!U(IsWindow)(s));
    EXPECT_EQ(SA(s, SBM_GETPOS, 0, 0), 0);
    EXPECT_EQ(SA(s, SBM_SETPOS, 5, 0), 0);
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL;
    CHECK(!U(GetScrollInfo)(s, SB_CTL, &si));
    EXPECT_EQ(U(GetScrollPos)(s, SB_CTL), 0);
    U(DestroyWindow)(p);
}

TEST(scrollctl_many_pos_updates)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND s = mksb(p, SBS_HORZ, 1);
    SA(s, SBM_SETRANGE, 0, 1000);
    int bad = 0;
    for (int i = 0; i < 5000; i++) {
        int want = (i * 7) % 1001;
        SA(s, SBM_SETPOS, want, FALSE);
        if (SA(s, SBM_GETPOS, 0, 0) != want) bad++;
    }
    EXPECT_EQ(bad, 0);
    U(DestroyWindow)(p);
}
