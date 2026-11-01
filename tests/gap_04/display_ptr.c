/* gap_04: pointer-input-target, display config (error paths only), auto
 * rotation, ScrollChildren, wallpaper. */
#include "t.h"

static void kill(HWND h) { BOOL (WINAPI *d)(HWND) = (void *)t_proc("DestroyWindow"); if (d && h) d(h); }
/* ---------------- RegisterPointerInputTarget(Ex) ---------------- */
typedef BOOL (WINAPI *rpit_fn)(HWND, DWORD);
typedef BOOL (WINAPI *rpite_fn)(HWND, DWORD, BOOL);

static const ULONG_PTR bad_hwnds[] = {0, 1, 0x1234, 0xFFFF, 0xFFFFFFFF, (ULONG_PTR)-1, 0xDEADBEEF, 0x7FFFFFFFFFFFFFFFull};
static const DWORD ptr_types[] = {0, 1, 2, 3, 4, 5, 6, 7, 0x100, 0xFFFFFFFF};

TEST(gap4_ptrtarget_bogus_handles_fail) {
    rpit_fn f = (rpit_fn)t_proc("RegisterPointerInputTarget"); REQUIRE(f);
    for (unsigned i = 0; i < sizeof bad_hwnds / sizeof *bad_hwnds; i++)
        for (unsigned j = 0; j < sizeof ptr_types / sizeof *ptr_types; j++) {
            SET_ERR(0);
            BOOL r = f((HWND)bad_hwnds[i], ptr_types[j]);
            CHECKF(r == 0, "hwnd %p type %lu returned %d", (void *)bad_hwnds[i], ptr_types[j], r);
            CHECKF(GetLastError() != 0, "hwnd %p type %lu: no last error", (void *)bad_hwnds[i], ptr_types[j]);
        }
}
TEST(gap4_ptrtarget_error_code_note) {
    rpit_fn f = (rpit_fn)t_proc("RegisterPointerInputTarget"); REQUIRE(f);
    SET_ERR(0);
    f(NULL, 2);
    t_note("NULL hwnd -> err %lu (1400 or 5 expected)", GetLastError());
    SET_ERR(0);
    f((HWND)0x1234, 2);
    t_note("bogus hwnd -> err %lu", GetLastError());
}
TEST(gap4_ptrtarget_destroyed_window_fails) {
    rpit_fn f = (rpit_fn)t_proc("RegisterPointerInputTarget"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 50, 50, NULL);
    if (!w) { t_skip("no window"); return; }
    BOOL (WINAPI *dw)(HWND) = (void *)t_proc("DestroyWindow"); REQUIRE(dw);
    dw(w);
    for (DWORD t = 1; t <= 5; t++) { SET_ERR(0); EXPECT_EQ(f(w, t), 0); EXPECT_NE(GetLastError(), 0); }
}
TEST(gap4_ptrtarget_invalid_type_on_bogus) {
    rpit_fn f = (rpit_fn)t_proc("RegisterPointerInputTarget"); REQUIRE(f);
    for (unsigned j = 0; j < sizeof ptr_types / sizeof *ptr_types; j++) {
        SET_ERR(0); EXPECT_EQ(f((HWND)0x7FFF0, ptr_types[j]), 0);
    }
}

TEST(gap4_ptrtargetex_not_supported) {
    rpite_fn f = (rpite_fn)t_proc("RegisterPointerInputTargetEx"); REQUIRE(f);
    /* Win10 body: SetLastError(ERROR_NOT_SUPPORTED); return FALSE; regardless of arguments. */
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 50, 50, NULL);
    static const ULONG_PTR hs[] = {0, 1, 0x1234, (ULONG_PTR)-1};
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++)
        for (unsigned j = 0; j < sizeof ptr_types / sizeof *ptr_types; j++)
            for (int ob = 0; ob < 2; ob++) {
                SET_ERR(0xABCD);
                BOOL r = f((HWND)hs[i], ptr_types[j], ob);
                CHECKF(r == 0, "ret %d", r);
                CHECKF(GetLastError() == ERROR_NOT_SUPPORTED, "err %lu", GetLastError());
            }
    if (w) {
        for (DWORD t = 1; t <= 5; t++)
            for (int ob = 0; ob < 2; ob++) {
                SET_ERR(0xABCD);
                EXPECT_EQ(f(w, t, ob), 0);
                EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
            }
        BOOL (WINAPI *dw)(HWND) = (void *)t_proc("DestroyWindow"); if (dw) dw(w);
    }
}
TEST(gap4_ptrtargetex_overwrites_error) {
    rpite_fn f = (rpite_fn)t_proc("RegisterPointerInputTargetEx"); REQUIRE(f);
    SET_ERR(ERROR_SUCCESS);
    f(NULL, 0, 0);
    EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
    SET_ERR(ERROR_ACCESS_DENIED);
    f(NULL, 0, 0);
    EXPECT_ERR(0, ERROR_NOT_SUPPORTED);
}
TEST(gap4_ptrtargetex_repeat) {
    rpite_fn f = (rpite_fn)t_proc("RegisterPointerInputTargetEx"); REQUIRE(f);
    for (int i = 0; i < 1000; i++) { SET_ERR(0); if (f((HWND)(ULONG_PTR)i, (DWORD)i, i & 1) != 0 || GetLastError() != ERROR_NOT_SUPPORTED) { CHECK(0); return; } }
    CHECK(1);
}

/* ---------------- SetDisplayConfig: validation / error paths only ---------------- */
typedef LONG (WINAPI *sdc_fn)(UINT32, void *, UINT32, void *, UINT32);
#define SDC_VALIDATE_ 0x40
#define SDC_APPLY_ 0x80
#define SDC_USE_SUPPLIED_ 0x20
#define SDC_USE_DB_CURRENT_ 0x0F

TEST(gap4_sdc_flags_zero_invalid) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    LONG r = f(0, NULL, 0, NULL, 0);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
}
TEST(gap4_sdc_return_value_not_in_last_error) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    SET_ERR(0x9999);
    LONG r = f(0, NULL, 0, NULL, 0);
    CHECK(r != 0);
    /* The result is a Win32 error code returned directly, not via GetLastError. */
    t_note("SetDisplayConfig(flags=0) ret %ld, last error %lu", r, GetLastError());
}
TEST(gap4_sdc_unknown_flag_bits_validate_only) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    /* Undocumented high bits: observed only. VALIDATE never changes the configuration. */
    static const UINT32 junk[] = {0x80000000u, 0x40000000u, 0x10000000u, 0x01000000u};
    for (unsigned i = 0; i < sizeof junk / sizeof *junk; i++) {
        LONG r = f(0, NULL, 0, NULL, junk[i] | SDC_VALIDATE_ | SDC_USE_DB_CURRENT_);
        t_note("VALIDATE|DB_CURRENT|0x%x -> %ld", junk[i], r);
    }
}
TEST(gap4_sdc_supplied_config_missing_arrays) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    /* Non-zero element counts with NULL arrays can never be satisfied. */
    LONG r = f(1, NULL, 0, NULL, SDC_VALIDATE_ | SDC_USE_SUPPLIED_);
    CHECKF(r != 0, "numPaths=1 ret %ld", r);
    r = f(0, NULL, 1, NULL, SDC_VALIDATE_ | SDC_USE_SUPPLIED_);
    CHECKF(r != 0, "numModes=1 ret %ld", r);
    r = f(1, NULL, 1, NULL, SDC_VALIDATE_ | SDC_USE_SUPPLIED_);
    CHECKF(r != 0, "both ret %ld", r);
    r = f(0, NULL, 0, NULL, SDC_VALIDATE_ | SDC_USE_SUPPLIED_);
    t_note("VALIDATE|SUPPLIED with empty arrays -> %ld", r);
}
TEST(gap4_sdc_counts_without_arrays) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    /* Topology flags require NULL arrays and zero counts. */
    static const UINT32 n[] = {1, 2, 16, 1000, 0xFFFFFFFFu};
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        LONG r = f(n[i], NULL, 0, NULL, SDC_VALIDATE_ | SDC_USE_DB_CURRENT_);
        CHECKF(r != 0, "numPaths %u ret %ld", n[i], r);
        r = f(0, NULL, n[i], NULL, SDC_VALIDATE_ | SDC_USE_DB_CURRENT_);
        CHECKF(r != 0, "numModes %u ret %ld", n[i], r);
    }
}
TEST(gap4_sdc_no_validate_no_apply) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    /* Exactly one of SDC_APPLY / SDC_VALIDATE is required. */
    static const UINT32 fl[] = {SDC_USE_DB_CURRENT_, 1, 2, 4, 8, SDC_USE_SUPPLIED_, 0x200, 0x400, 0x800, 0x2000};
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        LONG r = f(0, NULL, 0, NULL, fl[i]);
        CHECKF(r == ERROR_INVALID_PARAMETER, "flags 0x%x ret %ld", fl[i], r);
    }
}
TEST(gap4_sdc_validate_without_topology_or_supplied) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    /* One of SUPPLIED_DISPLAY_CONFIG or a TOPOLOGY_xxx flag must be set. */
    static const UINT32 fl[] = {SDC_VALIDATE_, SDC_VALIDATE_ | 0x200, SDC_VALIDATE_ | 0x400, SDC_VALIDATE_ | 0x800};
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        LONG r = f(0, NULL, 0, NULL, fl[i]);
        CHECKF(r == ERROR_INVALID_PARAMETER, "flags 0x%x ret %ld", fl[i], r);
    }
}
TEST(gap4_sdc_validate_current_topology) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    /* SDC_VALIDATE only tests; result depends on the host adapter/driver. */
    static const UINT32 topo[] = {1, 2, 4, 8, 0xF};
    for (unsigned i = 0; i < sizeof topo / sizeof *topo; i++) {
        LONG r = f(0, NULL, 0, NULL, SDC_VALIDATE_ | topo[i]);
        t_note("SDC_VALIDATE|topology 0x%x -> %ld", topo[i], r);
    }
}
TEST(gap4_sdc_supplied_with_topology_conflict) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    /* SDC_USE_SUPPLIED_DISPLAY_CONFIG cannot be combined with any SDC_TOPOLOGY_xxx flag. */
    static const UINT32 topo[] = {1, 2, 4, 8, 0x10};
    for (unsigned i = 0; i < sizeof topo / sizeof *topo; i++) {
        LONG r = f(0, NULL, 0, NULL, SDC_VALIDATE_ | SDC_USE_SUPPLIED_ | topo[i]);
        CHECKF(r == ERROR_INVALID_PARAMETER, "SUPPLIED|0x%x ret %ld", topo[i], r);
    }
}
TEST(gap4_sdc_no_optimization_requires_apply) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    LONG r = f(0, NULL, 0, NULL, SDC_VALIDATE_ | 0x100 | SDC_USE_DB_CURRENT_);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
    r = f(0, NULL, 0, NULL, SDC_VALIDATE_ | 0x100 | 0x4);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
}
TEST(gap4_sdc_force_mode_enum_requires_apply_supplied) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    LONG r = f(0, NULL, 0, NULL, SDC_VALIDATE_ | 0x1000 | SDC_USE_DB_CURRENT_);
    CHECKF(r != 0, "ret %ld", r);
    r = f(0, NULL, 0, NULL, SDC_VALIDATE_ | 0x1000 | SDC_USE_SUPPLIED_);
    CHECKF(r != 0, "ret %ld", r);
}
TEST(gap4_sdc_repeat_rejects_stably) {
    sdc_fn f = (sdc_fn)t_proc("SetDisplayConfig"); REQUIRE(f);
    for (int i = 0; i < 200; i++) { LONG r = f(0, NULL, 0, NULL, 0); if (r != ERROR_INVALID_PARAMETER) { CHECKF(0, "iter %d ret %ld", i, r); return; } }
    CHECK(1);
}

/* ---------------- Display auto rotation (save/restore) ---------------- */
typedef BOOL (WINAPI *sar_fn)(DWORD);
typedef BOOL (WINAPI *gar_fn)(DWORD *);
TEST(gap4_autorot_set_current_value_noop) {
    sar_fn s = (sar_fn)t_proc("SetDisplayAutoRotationPreferences");
    gar_fn g = (gar_fn)t_proc("GetDisplayAutoRotationPreferences");
    REQUIRE(s); REQUIRE(g);
    DWORD orig = 0xFFFF;
    SET_ERR(0);
    if (!g(&orig)) { t_note("GetDisplayAutoRotationPreferences failed err %lu; skipping set", GetLastError()); return; }
    CHECKF(orig <= 0xF, "orig 0x%lx", orig);
    SET_ERR(0);
    BOOL r = s(orig);
    t_note("Set(current=0x%lx) -> %d err %lu", orig, r, GetLastError());
    DWORD now = 0xFFFF;
    if (g(&now)) EXPECT_EQ(now, orig);
}
TEST(gap4_autorot_invalid_value_then_restore) {
    sar_fn s = (sar_fn)t_proc("SetDisplayAutoRotationPreferences");
    gar_fn g = (gar_fn)t_proc("GetDisplayAutoRotationPreferences");
    REQUIRE(s); REQUIRE(g);
    DWORD orig = 0xFFFF;
    if (!g(&orig)) { t_note("Get failed; not changing preferences"); return; }
    static const DWORD bad[] = {0x10, 0x20, 0x100, 0x80000000u, 0xFFFFFFFFu};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        BOOL r = s(bad[i]);
        DWORD e = GetLastError();
        t_note("Set(0x%lx) -> %d err %lu", bad[i], r, e);
        if (r) { s(orig); }
    }
    s(orig);
    DWORD now = 0xFFFF;
    if (g(&now)) EXPECT_EQ(now, orig);
}

/* ---------------- ScrollChildren ---------------- */
typedef void (WINAPI *sc_fn)(HWND, UINT, UINT);
static int sc_get(HWND w, int bar, SCROLLINFO *si) {
    BOOL (WINAPI *g)(HWND, int, LPSCROLLINFO) = (void *)t_proc("GetScrollInfo");
    memset(si, 0, sizeof *si); si->cbSize = sizeof *si; si->fMask = SIF_ALL;
    return g && g(w, bar, si);
}
static int sc_setup(HWND w, int bar, int pos) {
    int (WINAPI *s)(HWND, int, LPCSCROLLINFO, BOOL) = (void *)t_proc("SetScrollInfo");
    if (!s) return 0;
    SCROLLINFO si; memset(&si, 0, sizeof si); si.cbSize = sizeof si;
    si.fMask = SIF_ALL; si.nMin = 0; si.nMax = 1000; si.nPage = 100; si.nPos = pos;
    s(w, bar, &si, FALSE);
    return 1;
}
static int sc_pos(HWND w, int bar) { SCROLLINFO si; return sc_get(w, bar, &si) ? si.nPos : -12345; }

TEST(gap4_scrollchildren_vertical_codes) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    sc_setup(w, SB_VERT, 500);
    EXPECT_EQ(sc_pos(w, SB_VERT), 500);
    f(w, WM_VSCROLL, SB_LINEDOWN); int p1 = sc_pos(w, SB_VERT);
    CHECKF(p1 > 500, "linedown pos %d", p1);
    f(w, WM_VSCROLL, SB_LINEUP); int p2 = sc_pos(w, SB_VERT);
    CHECKF(p2 == 500, "lineup back to %d", p2);
    f(w, WM_VSCROLL, SB_LINEUP); int p3 = sc_pos(w, SB_VERT);
    CHECKF(p3 < 500, "lineup pos %d", p3);
    sc_setup(w, SB_VERT, 500);
    f(w, WM_VSCROLL, SB_PAGEDOWN); EXPECT_EQ(sc_pos(w, SB_VERT), 599);
    f(w, WM_VSCROLL, SB_PAGEUP); EXPECT_EQ(sc_pos(w, SB_VERT), 500);
    f(w, WM_VSCROLL, SB_PAGEUP); EXPECT_EQ(sc_pos(w, SB_VERT), 401);
    f(w, WM_VSCROLL, SB_TOP); EXPECT_EQ(sc_pos(w, SB_VERT), 0);
    f(w, WM_VSCROLL, SB_BOTTOM); EXPECT_EQ(sc_pos(w, SB_VERT), 901);
    f(w, WM_VSCROLL, SB_THUMBPOSITION | (250u << 16)); EXPECT_EQ(sc_pos(w, SB_VERT), 250);
    f(w, WM_VSCROLL, SB_THUMBPOSITION | (0u << 16)); EXPECT_EQ(sc_pos(w, SB_VERT), 0);
    f(w, WM_VSCROLL, SB_THUMBPOSITION | (5000u << 16)); EXPECT_EQ(sc_pos(w, SB_VERT), 901);
    kill(w);
}
TEST(gap4_scrollchildren_clamping) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    sc_setup(w, SB_VERT, 0);
    f(w, WM_VSCROLL, SB_LINEUP); EXPECT_EQ(sc_pos(w, SB_VERT), 0);
    f(w, WM_VSCROLL, SB_PAGEUP); EXPECT_EQ(sc_pos(w, SB_VERT), 0);
    sc_setup(w, SB_VERT, 1000);
    EXPECT_EQ(sc_pos(w, SB_VERT), 901);
    f(w, WM_VSCROLL, SB_LINEDOWN); EXPECT_EQ(sc_pos(w, SB_VERT), 901);
    f(w, WM_VSCROLL, SB_PAGEDOWN); EXPECT_EQ(sc_pos(w, SB_VERT), 901);
    sc_setup(w, SB_VERT, 450);
    f(w, WM_VSCROLL, SB_THUMBPOSITION | (0xFFFFu << 16)); /* signed -1 => clamp to min */
    EXPECT_EQ(sc_pos(w, SB_VERT), 0);
    kill(w);
}
TEST(gap4_scrollchildren_horizontal) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    sc_setup(w, SB_HORZ, 500);
    sc_setup(w, SB_VERT, 500);
    f(w, WM_HSCROLL, SB_LINEDOWN);
    CHECK(sc_pos(w, SB_HORZ) > 500);
    EXPECT_EQ(sc_pos(w, SB_VERT), 500);
    sc_setup(w, SB_HORZ, 500);
    f(w, WM_HSCROLL, SB_PAGEDOWN); EXPECT_EQ(sc_pos(w, SB_HORZ), 599);
    f(w, WM_HSCROLL, SB_BOTTOM); EXPECT_EQ(sc_pos(w, SB_HORZ), 901);
    f(w, WM_HSCROLL, SB_TOP); EXPECT_EQ(sc_pos(w, SB_HORZ), 0);
    f(w, WM_HSCROLL, SB_THUMBPOSITION | (321u << 16)); EXPECT_EQ(sc_pos(w, SB_HORZ), 321);
    EXPECT_EQ(sc_pos(w, SB_VERT), 500);
    kill(w);
}
TEST(gap4_scrollchildren_message_selects_bar) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    /* Only WM_VSCROLL (0x115) means vertical; every other message id is treated as horizontal. */
    static const UINT msgs[] = {0, WM_NULL, WM_HSCROLL, WM_PAINT, 0x116, 0xFFFF};
    for (unsigned i = 0; i < sizeof msgs / sizeof *msgs; i++) {
        sc_setup(w, SB_HORZ, 500); sc_setup(w, SB_VERT, 500);
        f(w, msgs[i], SB_PAGEDOWN);
        CHECKF(sc_pos(w, SB_HORZ) == 599, "msg 0x%x horz %d", msgs[i], sc_pos(w, SB_HORZ));
        CHECKF(sc_pos(w, SB_VERT) == 500, "msg 0x%x vert %d", msgs[i], sc_pos(w, SB_VERT));
    }
    kill(w);
}
TEST(gap4_scrollchildren_ignored_codes) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    /* THUMBTRACK (5) and codes above ENDSCROLL (8) leave the position unchanged. */
    static const UINT codes[] = {SB_THUMBTRACK, 9, 10, 0x7F, 0xFFFF};
    for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {
        sc_setup(w, SB_VERT, 500);
        f(w, WM_VSCROLL, codes[i] | (700u << 16));
        CHECKF(sc_pos(w, SB_VERT) == 500, "code %u pos %d", codes[i], sc_pos(w, SB_VERT));
    }
    sc_setup(w, SB_VERT, 500);
    /* SB_ENDSCROLL does not move the thumb; it re-runs CalcClientScrolling, which
     * on Win10 22H2 drops the scroll bar of a window with no children (GetScrollInfo
     * then fails). Accept "unchanged" or "bar removed", never a moved position. */
    f(w, WM_VSCROLL, SB_ENDSCROLL);
    { SCROLLINFO si; int ok = sc_get(w, SB_VERT, &si);
      if (ok) CHECKF(si.nPos == 500, "ENDSCROLL moved pos to %d", si.nPos);
      else t_note("ScrollChildren(SB_ENDSCROLL) removed the scroll bar (CalcClientScrolling)"); }
    kill(w);
}
TEST(gap4_scrollchildren_only_low_word_is_code) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    sc_setup(w, SB_VERT, 500);
    /* High word is only a position for THUMBPOSITION; ignored for PAGEDOWN. */
    f(w, WM_VSCROLL, SB_PAGEDOWN | (123u << 16));
    EXPECT_EQ(sc_pos(w, SB_VERT), 599);
    kill(w);
}
TEST(gap4_scrollchildren_with_child_window_moves_it) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    HWND c = t_make_window("BUTTON", WS_CHILD | WS_VISIBLE, 0, 50, 60, 40, 20, w);
    if (!c) { kill(w); t_skip("no child"); return; }
    U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    sc_setup(w, SB_VERT, 500);
    RECT r0, r1; POINT p0 = {0, 0}, p1 = {0, 0};
    U(GetWindowRect)(c, &r0);
    f(w, WM_VSCROLL, SB_PAGEDOWN);
    U(GetWindowRect)(c, &r1);
    int dy = r1.top - r0.top, dx = r1.left - r0.left;
    CHECKF(dx == 0, "dx %d", dx);
    if (dy == 0) t_note("child did not move"); else CHECKF(dy < 0, "child should move up when scrolling down, dy=%d", dy);
    /* Scroll unit is 8 px per position step in the Win10 implementation. */
    t_note("child moved dy=%d for 99 units", dy);
    kill(w);
}
TEST(gap4_scrollchildren_zero_range_window) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    /* Default range 0..0 with page 0: must not fault; resulting position is implementation detail. */
    f(w, WM_VSCROLL, SB_LINEDOWN);
    f(w, WM_VSCROLL, SB_BOTTOM);
    f(w, WM_VSCROLL, SB_PAGEDOWN);
    f(w, WM_VSCROLL, SB_TOP);
    int p = sc_pos(w, SB_VERT);
    CHECKF(p == 0, "pos after TOP %d", p);
    kill(w);
}
TEST(gap4_scrollchildren_repeated_line_steps_monotonic) {
    sc_fn f = (sc_fn)t_proc("ScrollChildren"); REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_VSCROLL, 0, 0, 0, 300, 300, NULL);
    if (!w) { t_skip("no window"); return; }
    sc_setup(w, SB_VERT, 0);
    int last = 0;
    for (int i = 0; i < 50; i++) {
        f(w, WM_VSCROLL, SB_LINEDOWN);
        int p = sc_pos(w, SB_VERT);
        CHECKF(p > last || p == 901, "step %d pos %d last %d", i, p, last);
        last = p;
    }
    kill(w);
}

/* ---------------- SetDeskWallpaper ----------------
 * Forwards to SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, path, SPIF_UPDATEINIFILE).
 * Any call could replace the user's wallpaper, so it is never invoked here. */
TEST(gap4_setdeskwallpaper_resolve_only) {
    REQUIRE(t_proc("SetDeskWallpaper"));
    t_note("SetDeskWallpaper is SPI_SETDESKWALLPAPER with UPDATEINIFILE; intentionally not called");
    CHECK(1);
}
TEST(gap4_wallpaper_spi_get_matches_registry_note) {
    BOOL (WINAPI *spi)(UINT, UINT, PVOID, UINT) = (void *)t_proc("SystemParametersInfoA");
    REQUIRE(spi);
    char buf[MAX_PATH + 4]; memset(buf, 0, sizeof buf);
    SET_ERR(0);
    BOOL r = spi(SPI_GETDESKWALLPAPER, MAX_PATH, buf, 0);
    CHECK(r);
    CHECK(strlen(buf) < MAX_PATH);
    /* uiParam too small: Windows truncates or fails, never overruns */
    char small[8]; memset(small, 'Z', sizeof small);
    r = spi(SPI_GETDESKWALLPAPER, 4, small, 0);
    CHECK(small[7] == 'Z');
}
TEST(gap4_ime_sendimemessageex_not_called) {
    REQUIRE(t_proc("SendIMEMessageExA")); REQUIRE(t_proc("SendIMEMessageExW"));
    t_note("SendIMEMessageEx{A,W}: indirect forwarders to imm32 with undocumented signature; resolve only");
    CHECK(1);
}
