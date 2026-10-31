/* gap2: window query exports (band, composition, minimize rect, process handle,
 * region, ghost/hung, DPI hosting behaviour). */
#include "gap2.h"

typedef BOOL (WINAPI *band_fn)(HWND, DWORD *);
typedef struct { DWORD attrib; PVOID data; SIZE_T cb; } G2_WCAD;
typedef BOOL (WINAPI *wca_fn)(HWND, G2_WCAD *);
typedef BOOL (WINAPI *wci_fn)(HWND, void *);
typedef BOOL (WINAPI *minrect_fn)(HWND, RECT *);
typedef HANDLE (WINAPI *wph_fn)(HWND, DWORD);
typedef int (WINAPI *rgnex_fn)(HWND, HRGN, UINT);
typedef HWND (WINAPI *hwnd1_fn)(HWND);

#define BAND_FN() ((band_fn)t_proc("GetWindowBand"))
#define WCA_FN() ((wca_fn)t_proc("GetWindowCompositionAttribute"))

/* ---------------------------------------------------------------- GetWindowBand */
TEST(gap2_band_resolve) {
    REQUIRE(BAND_FN());
    CHECK(t_proc("GetWindowBand") != NULL);
    CHECK(t_proc_ord(1981) == t_proc("GetWindowBand"));
    CHECK((ULONG_PTR)t_proc("GetWindowBand") != (ULONG_PTR)t_proc("GetWindowCompositionAttribute"));
}

TEST(gap2_band_invalid_hwnd) {
    band_fn f = BAND_FN(); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        DWORD band = 0xCCCCCCCC;
        SetLastError(0xdeadbeef);
        BOOL r = f(g2_bogus[i].h, &band);
        CHECKF(r == FALSE, "hwnd %s returned %d", g2_bogus[i].n, r);
        if (GetLastError() != 1400) t_note("band %s: GetLastError %lu (1400 expected)", g2_bogus[i].n, GetLastError());
        if (band != 0xCCCCCCCC) t_note("band %s: output modified to %lu on failure", g2_bogus[i].n, band);
    }
    g2_destroy(prime);
}

TEST(gap2_band_dead_window) {
    band_fn f = BAND_FN(); REQUIRE(f);
    HWND h = g2_dead_hwnd();
    DWORD band = 0xCCCCCCCC;
    SetLastError(0xdeadbeef);
    CHECK(f(h, &band) == FALSE);
    NOTE_ERR(1400);
}

TEST(gap2_band_valid_toplevel) {
    band_fn f = BAND_FN(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(h);
    DWORD b1 = 0xCCCCCCCC, b2 = 0xCCCCCCCC;
    BOOL r1 = f(h, &b1), r2 = f(h, &b2);
    CHECK(r1 == r2);
    CHECK(b1 == b2);
    if (!r1) t_note("GetWindowBand failed on a valid window, error %lu", GetLastError());
    else {
        t_note("band of plain top-level window = %lu", b1);
        CHECK(b1 <= 32); /* ZBID values are small enumerators */
    }
    g2_destroy(h);
}

TEST(gap2_band_style_variants) {
    band_fn f = BAND_FN(); REQUIRE(f);
    static const DWORD ex[] = { 0, WS_EX_TOPMOST, WS_EX_TOOLWINDOW, WS_EX_NOACTIVATE, WS_EX_LAYERED, WS_EX_TRANSPARENT };
    DWORD first = 0; int have = 0;
    for (unsigned i = 0; i < sizeof ex / sizeof ex[0]; i++) {
        HWND h = g2_mkwnd(WS_POPUP, ex[i]);
        if (!h) continue;
        DWORD b = 0xCCCCCCCC;
        BOOL r = f(h, &b);
        if (!r) t_note("ex 0x%lx: failed err %lu", ex[i], GetLastError());
        else {
            if (!have) { first = b; have = 1; }
            if (b != first) t_note("ex 0x%lx band %lu differs from base %lu", ex[i], b, first);
            CHECK(b <= 32);
        }
        g2_destroy(h);
    }
}

TEST(gap2_band_child_and_desktop) {
    band_fn f = BAND_FN(); REQUIRE(f);
    HWND p = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    HWND c = t_make_window(t_cls(), WS_CHILD, 0, 0, 0, 50, 50, p);
    REQUIRE(p && c);
    DWORD bp = 0xCCCCCCCC, bc = 0xCCCCCCCC, bd = 0xCCCCCCCC;
    BOOL rp = f(p, &bp), rc = f(c, &bc);
    t_note("parent r=%d band=%lu, child r=%d band=%lu", rp, bp, rc, bc);
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow");
    if (gdw) { BOOL rd = f(gdw(), &bd); t_note("desktop r=%d band=%lu", rd, bd); }
    g2_destroy(p);
}

static DWORD WINAPI band_thread(void *arg) {
    HWND h = (HWND)arg;
    DWORD b = 0xCCCCCCCC;
    BOOL r = BAND_FN()(h, &b);
    return r ? 1 : 0;
}
TEST(gap2_band_cross_thread) {
    band_fn f = BAND_FN(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    DWORD b = 0xCCCCCCCC;
    BOOL here = f(h, &b);
    DWORD there = g2_run_thread(band_thread, h);
    CHECKF(there != 0xdead0001, "thread timed out");
    if ((BOOL)there != here) t_note("band cross-thread %lu vs local %d", there, here);
    DWORD bogus = g2_run_thread(band_thread, (void *)(ULONG_PTR)0xdeadbeef);
    CHECK(bogus == 0);
    g2_destroy(h);
}

TEST(gap2_band_repeat) {
    band_fn f = BAND_FN(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    DWORD b0 = 0xCCCCCCCC; BOOL r0 = f(h, &b0);
    int diff = 0;
    for (int i = 0; i < 2000; i++) { DWORD b = 0xCCCCCCCC; BOOL r = f(h, &b); if (r != r0 || b != b0) diff++; }
    CHECK(diff == 0);
    g2_destroy(h);
}

/* ------------------------------------------------ GetWindowCompositionAttribute */
TEST(gap2_wca_resolve) {
    REQUIRE(WCA_FN());
    CHECK(t_proc_ord(1982) == t_proc("GetWindowCompositionAttribute"));
}

TEST(gap2_wca_invalid_hwnd) {
    wca_fn f = WCA_FN(); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        DWORD v = 0xCCCCCCCC;
        G2_WCAD d = { 1 /*WCA_NCRENDERING_ENABLED*/, &v, sizeof v };
        SetLastError(0xdeadbeef);
        BOOL r = f(g2_bogus[i].h, &d);
        CHECKF(r == FALSE, "hwnd %s returned %d", g2_bogus[i].n, r);
        if (v != 0xCCCCCCCC) t_note("wca %s modified output", g2_bogus[i].n);
        if (GetLastError() != 1400) t_note("wca %s: error %lu (1400 expected)", g2_bogus[i].n, GetLastError());
    }
    g2_destroy(prime);
}

TEST(gap2_wca_dead_window) {
    wca_fn f = WCA_FN(); REQUIRE(f);
    HWND h = g2_dead_hwnd();
    DWORD v = 0xCCCCCCCC;
    G2_WCAD d = { 1, &v, sizeof v };
    CHECK(f(h, &d) == FALSE);
    CHECK(v == 0xCCCCCCCC);
}

TEST(gap2_wca_attribute_sweep) {
    wca_fn f = WCA_FN(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(h);
    int ok = 0;
    for (DWORD a = 0; a <= 40; a++) {
        BYTE buf[64]; memset(buf, 0xCC, sizeof buf);
        G2_WCAD d = { a, buf, sizeof buf };
        SetLastError(0xdeadbeef);
        BOOL r = f(h, &d);
        BYTE buf2[64]; memset(buf2, 0xCC, sizeof buf2);
        G2_WCAD d2 = { a, buf2, sizeof buf2 };
        BOOL r2 = f(h, &d2);
        CHECKF(r == r2, "attrib %lu unstable result", a);
        CHECKF(!r || !memcmp(buf, buf2, sizeof buf), "attrib %lu unstable data", a);
        if (r) { ok++; t_note("attrib %lu ok, first dword 0x%08lx", a, *(DWORD *)buf); }
    }
    t_note("%d attributes readable", ok);
    g2_destroy(h);
}

TEST(gap2_wca_cb_variants) {
    wca_fn f = WCA_FN(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    static const SIZE_T cbs[] = { 0, 1, 2, 3, 4, 8, 16, 0x1000 };
    BYTE buf[0x1000 + 32];
    for (unsigned i = 0; i < sizeof cbs / sizeof cbs[0]; i++) {
        memset(buf, 0xCC, sizeof buf);
        G2_WCAD d = { 1, buf, cbs[i] };
        SetLastError(0xdeadbeef);
        BOOL r = f(h, &d);
        t_note("NCRENDERING_ENABLED cb=%llu -> %d err=%lu", (unsigned long long)cbs[i], r, GetLastError());
        int over = 0;
        for (SIZE_T k = cbs[i]; k < cbs[i] + 16; k++) if (buf[k] != 0xCC) over = 1;
        CHECKF(!over, "cb=%llu: wrote past cbData", (unsigned long long)cbs[i]);
    }
    g2_destroy(h);
}

TEST(gap2_wca_bad_attribs) {
    wca_fn f = WCA_FN(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    static const DWORD at[] = { 0, 0x1000, 0x7fffffff, 0x80000000, 0xffffffff, 100, 255 };
    for (unsigned i = 0; i < sizeof at / sizeof at[0]; i++) {
        DWORD v = 0xCCCCCCCC;
        G2_WCAD d = { at[i], &v, sizeof v };
        SetLastError(0xdeadbeef);
        BOOL r = f(h, &d);
        CHECKF(r == FALSE, "attrib 0x%lx accepted", at[i]);
        if (r == FALSE) NOTE_ERR(87);
    }
    g2_destroy(h);
}

TEST(gap2_wca_cloaked) {
    wca_fn f = WCA_FN(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);
    REQUIRE(h);
    t_pump();
    DWORD v = 0xCCCCCCCC;
    G2_WCAD d = { 14 /*WCA_CLOAKED*/, &v, sizeof v };
    if (f(h, &d)) { t_note("WCA_CLOAKED = 0x%lx for visible window", v); CHECK(v == 0 || v == 0xCCCCCCCC); }
    else t_note("WCA_CLOAKED unsupported, err %lu", GetLastError());
    g2_destroy(h);
}

/* ---------------------------------------------------- GetWindowCompositionInfo */
TEST(gap2_wci_resolve) {
    REQUIRE(t_proc("GetWindowCompositionInfo"));
    CHECK(t_proc_ord(1983) == t_proc("GetWindowCompositionInfo"));
}
TEST(gap2_wci_invalid_hwnd) {
    wci_fn f = (wci_fn)t_proc("GetWindowCompositionInfo"); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        BYTE buf[512]; memset(buf, 0, sizeof buf);
        SetLastError(0xdeadbeef);
        BOOL r = f(g2_bogus[i].h, buf);
        t_note("GetWindowCompositionInfo(%s) -> %d err=%lu", g2_bogus[i].n, r, GetLastError());
        CHECKF(r == FALSE, "hwnd %s returned %d", g2_bogus[i].n, r);
    }
    g2_destroy(prime);
}

/* ----------------------------------------------------- GetWindowMinimizeRect */
TEST(gap2_minrect_resolve) {
    REQUIRE(t_proc("GetWindowMinimizeRect"));
    CHECK(t_proc_ord(1995) == t_proc("GetWindowMinimizeRect"));
}
TEST(gap2_minrect_invalid_hwnd) {
    minrect_fn f = (minrect_fn)t_proc("GetWindowMinimizeRect"); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        RECT rc = { 0x5a5a, 0x5a5a, 0x5a5a, 0x5a5a };
        SetLastError(0xdeadbeef);
        BOOL r = f(g2_bogus[i].h, &rc);
        t_note("GetWindowMinimizeRect(%s) -> %d err=%lu rc=%ld,%ld", g2_bogus[i].n, r, GetLastError(), rc.left, rc.top);
        CHECKF(r == FALSE, "hwnd %s returned %d", g2_bogus[i].n, r);
        CHECKF(rc.left == 0x5a5a && rc.right == 0x5a5a, "rect modified on failure");
    }
    g2_destroy(prime);
}
TEST(gap2_minrect_valid) {
    minrect_fn f = (minrect_fn)t_proc("GetWindowMinimizeRect"); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(h);
    RECT rc = { 0x5a5a, 0x5a5a, 0x5a5a, 0x5a5a }, rc2 = rc;
    SetLastError(0xdeadbeef);
    BOOL r = f(h, &rc), r2 = f(h, &rc2);
    t_note("minrect valid -> %d err=%lu rc=(%ld,%ld,%ld,%ld)", r, GetLastError(), rc.left, rc.top, rc.right, rc.bottom);
    CHECK(r == r2);
    CHECK(!memcmp(&rc, &rc2, sizeof rc));
    g2_destroy(h);
}
TEST(gap2_minrect_dead_window) {
    minrect_fn f = (minrect_fn)t_proc("GetWindowMinimizeRect"); REQUIRE(f);
    HWND h = g2_dead_hwnd();
    RECT rc = { 7, 7, 7, 7 };
    CHECK(f(h, &rc) == FALSE);
    CHECK(rc.left == 7 && rc.top == 7 && rc.right == 7 && rc.bottom == 7);
}

/* ---------------------------------------------------- GetWindowProcessHandle */
TEST(gap2_wph_resolve) {
    REQUIRE(t_proc("GetWindowProcessHandle"));
    CHECK(t_proc_ord(2000) == t_proc("GetWindowProcessHandle"));
}
TEST(gap2_wph_invalid_hwnd) {
    wph_fn f = (wph_fn)t_proc("GetWindowProcessHandle"); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        HANDLE hp = f(g2_bogus[i].h, PROCESS_QUERY_LIMITED_INFORMATION);
        CHECKF(hp == NULL, "hwnd %s returned %p", g2_bogus[i].n, hp);
        if (hp) CloseHandle(hp);
    }
    g2_destroy(prime);
}
TEST(gap2_wph_own_window) {
    wph_fn f = (wph_fn)t_proc("GetWindowProcessHandle"); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(h);
    static const DWORD acc[] = { PROCESS_QUERY_LIMITED_INFORMATION, SYNCHRONIZE, PROCESS_QUERY_INFORMATION, 0 };
    for (unsigned i = 0; i < sizeof acc / sizeof acc[0]; i++) {
        SetLastError(0xdeadbeef);
        HANDLE hp = f(h, acc[i]);
        t_note("GetWindowProcessHandle(own, 0x%lx) -> %p err=%lu", acc[i], hp, GetLastError());
        if (hp && hp != INVALID_HANDLE_VALUE) {
            DWORD pid = GetProcessId(hp);
            if (pid) { CHECKF(pid == GetCurrentProcessId(), "pid %lu vs %lu", pid, GetCurrentProcessId()); CloseHandle(hp); }
        }
    }
    g2_destroy(h);
}
TEST(gap2_wph_dead_window) {
    wph_fn f = (wph_fn)t_proc("GetWindowProcessHandle"); REQUIRE(f);
    HWND h = g2_dead_hwnd();
    HANDLE hp = f(h, PROCESS_QUERY_LIMITED_INFORMATION);
    CHECK(hp == NULL);
    if (hp) CloseHandle(hp);
}

/* ------------------------------------------------------------ GetWindowRgnEx */
TEST(gap2_rgnex_resolve) {
    REQUIRE(t_proc("GetWindowRgnEx"));
    CHECK(t_proc_ord(2007) == t_proc("GetWindowRgnEx"));
}
TEST(gap2_rgnex_invalid_hwnd) {
    rgnex_fn f = (rgnex_fn)t_proc("GetWindowRgnEx"); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        HRGN rg = CreateRectRgn(1, 2, 30, 40);
        SetLastError(0xdeadbeef);
        int r = f(g2_bogus[i].h, rg, 0);
        CHECKF(r == 0, "hwnd %s returned %d", g2_bogus[i].n, r);
        RECT box; GetRgnBox(rg, &box);
        CHECKF(box.left == 1 && box.top == 2 && box.right == 30 && box.bottom == 40, "region modified");
        DeleteObject(rg);
    }
    g2_destroy(prime);
}
TEST(gap2_rgnex_no_region) {
    rgnex_fn f = (rgnex_fn)t_proc("GetWindowRgnEx"); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    HRGN rg = CreateRectRgn(0, 0, 5, 5);
    int r = f(h, rg, 0);
    CHECK(r == 0); /* no region assigned to window: same as GetWindowRgn */
    int (WINAPI *gwr)(HWND, HRGN) = (void *)t_proc("GetWindowRgn");
    if (gwr) CHECK(gwr(h, rg) == 0);
    DeleteObject(rg);
    g2_destroy(h);
}
TEST(gap2_rgnex_with_region) {
    rgnex_fn f = (rgnex_fn)t_proc("GetWindowRgnEx"); REQUIRE(f);
    int (WINAPI *swr)(HWND, HRGN, BOOL) = (void *)t_proc("SetWindowRgn");
    int (WINAPI *gwr)(HWND, HRGN) = (void *)t_proc("GetWindowRgn");
    REQUIRE(swr); REQUIRE(gwr);
    HWND h = g2_mkwnd(WS_POPUP, 0);
    HRGN set = CreateRectRgn(10, 10, 60, 50);
    CHECK(swr(h, set, FALSE) != 0); /* set is now owned by the system */
    HRGN out = CreateRectRgn(0, 0, 1, 1);
    int ref = gwr(h, out);
    RECT refbox; GetRgnBox(out, &refbox);
    CHECK(ref == SIMPLEREGION);
    CHECK(refbox.left == 10 && refbox.top == 10 && refbox.right == 60 && refbox.bottom == 50);
    HRGN out2 = CreateRectRgn(0, 0, 1, 1);
    SetLastError(0xdeadbeef);
    int r = f(h, out2, 0);
    t_note("GetWindowRgnEx with region -> %d (GetWindowRgn gave %d)", r, ref);
    if (r) {
        RECT box; GetRgnBox(out2, &box);
        t_note("box %ld,%ld,%ld,%ld", box.left, box.top, box.right, box.bottom);
    }
    SetLastError(0xdeadbeef);
    int rn = f(h, NULL, 0);
    CHECK(rn == 0);
    DeleteObject(out); DeleteObject(out2);
    g2_destroy(h);
}
TEST(gap2_rgnex_bad_hrgn) {
    rgnex_fn f = (rgnex_fn)t_proc("GetWindowRgnEx"); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    static const HRGN bad[] = { (HRGN)0, (HRGN)1, (HRGN)0xdeadbeef, (HRGN)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 4; i++) {
        int r = f(h, bad[i], 0);
        CHECKF(r == 0, "hrgn %p returned %d", bad[i], r);
    }
    g2_destroy(h);
}

/* ------------------------------------------------ Ghost / Hung window lookups */
TEST(gap2_ghost_resolve) {
    REQUIRE(t_proc("GhostWindowFromHungWindow"));
    REQUIRE(t_proc("HungWindowFromGhostWindow"));
    CHECK(t_proc_ord(2015) == t_proc("GhostWindowFromHungWindow"));
    CHECK(t_proc_ord(2020) == t_proc("HungWindowFromGhostWindow"));
    CHECK(t_proc("GhostWindowFromHungWindow") != t_proc("HungWindowFromGhostWindow"));
}
TEST(gap2_ghost_invalid_hwnd) {
    hwnd1_fn g = (hwnd1_fn)t_proc("GhostWindowFromHungWindow");
    hwnd1_fn u = (hwnd1_fn)t_proc("HungWindowFromGhostWindow");
    REQUIRE(g); REQUIRE(u);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        HWND r1 = g(g2_bogus[i].h);
        HWND r2 = u(g2_bogus[i].h);
        CHECKF(r1 == NULL, "Ghost(%s) = %p", g2_bogus[i].n, r1);
        CHECKF(r2 == NULL, "Hung(%s) = %p", g2_bogus[i].n, r2);
    }
    g2_destroy(prime);
}
TEST(gap2_ghost_normal_window) {
    hwnd1_fn g = (hwnd1_fn)t_proc("GhostWindowFromHungWindow");
    hwnd1_fn u = (hwnd1_fn)t_proc("HungWindowFromGhostWindow");
    REQUIRE(g); REQUIRE(u);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);
    REQUIRE(h);
    t_pump();
    CHECK(g(h) == NULL);  /* responsive window has no ghost */
    CHECK(u(h) == NULL);  /* and is not itself a ghost */
    HWND child = t_make_window(t_cls(), WS_CHILD | WS_VISIBLE, 0, 0, 0, 20, 20, h);
    CHECK(g(child) == NULL);
    CHECK(u(child) == NULL);
    for (int i = 0; i < 500; i++) { if (g(h) || u(h)) { CHECK(0); break; } }
    g2_destroy(h);
}
TEST(gap2_ghost_dead_and_desktop) {
    hwnd1_fn g = (hwnd1_fn)t_proc("GhostWindowFromHungWindow");
    hwnd1_fn u = (hwnd1_fn)t_proc("HungWindowFromGhostWindow");
    REQUIRE(g); REQUIRE(u);
    HWND d = g2_dead_hwnd();
    CHECK(g(d) == NULL);
    CHECK(u(d) == NULL);
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow");
    if (gdw) { CHECK(g(gdw()) == NULL); CHECK(u(gdw()) == NULL); }
    HWND (WINAPI *fw)(LPCSTR, LPCSTR) = (void *)t_proc("FindWindowA");
    if (fw) {
        HWND gh = fw("Ghost", NULL);
        if (gh) { HWND hung = u(gh); t_note("ghost window %p stands for hung window %p", gh, hung); }
    }
}
static DWORD WINAPI ghost_thread(void *a) {
    hwnd1_fn g = (hwnd1_fn)t_proc("GhostWindowFromHungWindow");
    hwnd1_fn u = (hwnd1_fn)t_proc("HungWindowFromGhostWindow");
    return (g((HWND)a) == NULL && u((HWND)a) == NULL) ? 1 : 0;
}
TEST(gap2_ghost_cross_thread) {
    REQUIRE(t_proc("GhostWindowFromHungWindow")); REQUIRE(t_proc("HungWindowFromGhostWindow"));
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    CHECK(g2_run_thread(ghost_thread, h) == 1);
    CHECK(g2_run_thread(ghost_thread, (void *)(ULONG_PTR)0xdeadbeef) == 1);
    g2_destroy(h);
}

/* ---------------------------------------------- GetWindowDpiHostingBehavior */
#define DPIH() ((int (WINAPI *)(HWND))t_proc("GetWindowDpiHostingBehavior"))
TEST(gap2_dpih_resolve) {
    REQUIRE(DPIH());
    CHECK(t_proc("GetThreadDpiHostingBehavior") != NULL);
    CHECK(t_proc("SetThreadDpiHostingBehavior") != NULL);
}
TEST(gap2_dpih_invalid_hwnd) {
    int (WINAPI *f)(HWND) = DPIH(); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0); /* make this a GUI thread */
    for (int i = 0; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        int r = f(g2_bogus[i].h);
        CHECKF(r == -1, "hwnd %s returned %d", g2_bogus[i].n, r);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "hwnd %s: error %lu", g2_bogus[i].n, GetLastError());
    }
    g2_destroy(prime);
}
TEST(gap2_dpih_dead_window) {
    int (WINAPI *f)(HWND) = DPIH(); REQUIRE(f);
    HWND h = g2_dead_hwnd();
    SetLastError(0xdeadbeef);
    CHECK(f(h) == -1);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
}
TEST(gap2_dpih_default_window) {
    int (WINAPI *f)(HWND) = DPIH(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(h);
    SetLastError(0xdeadbeef);
    int r = f(h);
    CHECK(r == 0); /* DPI_HOSTING_BEHAVIOR_DEFAULT */
    CHECK(GetLastError() == 0xdeadbeef); /* success path leaves last error alone */
    HWND c = t_make_window(t_cls(), WS_CHILD, 0, 0, 0, 10, 10, h);
    CHECK(f(c) == 0);
    HWND po = t_make_window(t_cls(), WS_POPUP, 0, 0, 0, 10, 10, h);
    CHECK(f(po) == 0);
    for (int i = 0; i < 1000; i++) if (f(h) != 0) { CHECK(0); break; }
    g2_destroy(h);
}
TEST(gap2_dpih_desktop_window) {
    int (WINAPI *f)(HWND) = DPIH(); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow");
    HWND (WINAPI *gsw)(void) = (void *)t_proc("GetShellWindow");
    if (gdw) { int r = f(gdw()); t_note("desktop hosting behavior %d", r); CHECK(r == 0 || r == 1); }
    if (gsw && gsw()) { int r = f(gsw()); t_note("shell hosting behavior %d", r); CHECK(r == 0 || r == 1 || r == -1); }
    g2_destroy(prime);
}
TEST(gap2_dpih_thread_setting) {
    int (WINAPI *f)(HWND) = DPIH(); REQUIRE(f);
    int (WINAPI *get)(void) = (void *)t_proc("GetThreadDpiHostingBehavior");
    int (WINAPI *set)(int) = (void *)t_proc("SetThreadDpiHostingBehavior");
    REQUIRE(get); REQUIRE(set);
    int orig = get();
    CHECK(orig == 0);
    int prev = set(1);
    CHECK(prev == orig);
    CHECK(get() == 1);
    HWND mixed = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    int rm = f(mixed);
    t_note("window created under MIXED hosting reports %d", rm);
    CHECK(rm == 0 || rm == 1);
    CHECK(set(orig) == 1);
    CHECK(get() == orig);
    HWND def = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    CHECK(f(def) == 0);
    if (mixed) CHECK(f(mixed) == rm); /* creation-time property, unaffected by later thread changes */
    SetLastError(0xdeadbeef);
    int r = set(2);
    t_note("SetThreadDpiHostingBehavior(2) -> %d err=%lu", r, GetLastError());
    set(orig);
    CHECK(get() == orig);
    g2_destroy(mixed); g2_destroy(def);
}
static DWORD WINAPI dpih_thread(void *a) {
    int (WINAPI *f)(HWND) = DPIH();
    HWND own = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    DWORD ok = 0;
    if (own) {
        if (f((HWND)a) == 0) ok |= 1;
        SetLastError(0xdeadbeef);
        if (f((HWND)(ULONG_PTR)0xdeadbeef) == -1 && GetLastError() == 87) ok |= 2;
        if (f(own) == 0) ok |= 4;
        g2_destroy(own);
    }
    return ok;
}
TEST(gap2_dpih_cross_thread) {
    REQUIRE(DPIH());
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    CHECK(g2_run_thread(dpih_thread, h) == 7);
    g2_destroy(h);
}
static DWORD WINAPI dpih_nogui_thread(void *a) {
    int (WINAPI *f)(HWND) = DPIH();
    return (DWORD)f((HWND)a);
}
TEST(gap2_dpih_non_gui_thread) {
    REQUIRE(DPIH());
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    DWORD r = g2_run_thread(dpih_nogui_thread, h);
    t_note("non-GUI thread query of live window -> %d", (int)r);
    CHECK((int)r == 0 || (int)r == -1);
    r = g2_run_thread(dpih_nogui_thread, (void *)(ULONG_PTR)0xdeadbeef);
    t_note("non-GUI thread query of bogus window -> %d", (int)r);
    CHECK((int)r == 0 || (int)r == -1);
    g2_destroy(h);
}
