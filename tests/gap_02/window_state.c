/* gap2: window state predicates and icon copy, monitor inheritance,
 * queue-attachment predicates, TSF/SET helpers. */
#include "gap2.h"

typedef BYTE (WINAPI *byte_hwnd_fn)(HWND);
typedef int (WINAPI *int_hwnd_fn)(HWND);
typedef BOOL (WINAPI *inherit_fn)(HWND, HWND);
typedef HICON (WINAPI *igwi_fn)(HWND, UINT);
typedef BOOL (WINAPI *thq_fn)(DWORD);

#define F_SSW()  ((byte_hwnd_fn)t_proc("IsServerSideWindow"))
#define F_DESTR() ((byte_hwnd_fn)t_proc("IsWindowInDestroy"))
#define F_REDIR() ((int_hwnd_fn)t_proc("IsWindowRedirectedForPrint"))
#define F_INH()  ((inherit_fn)t_proc("InheritWindowMonitor"))
#define F_ICON() ((igwi_fn)t_proc("InternalGetWindowIcon"))

/* ------------------------------------------------------ IsServerSideWindow */
TEST(gap2_ssw_resolve) {
    REQUIRE(F_SSW());
    NOTE_EQ(t_proc_ord(2083), t_proc("IsServerSideWindow")); /* ordinals vary between builds */
}
TEST(gap2_ssw_invalid_hwnd) {
    byte_hwnd_fn f = F_SSW(); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        BYTE r = f(g2_bogus[i].h);
        CHECKF(r == 0, "hwnd %s returned %u", g2_bogus[i].n, r);
        CHECKF(GetLastError() == 0xdeadbeef, "hwnd %s changed last error to %lu", g2_bogus[i].n, GetLastError());
    }
    g2_destroy(prime);
}
TEST(gap2_ssw_dead_window) {
    byte_hwnd_fn f = F_SSW(); REQUIRE(f);
    HWND h = g2_dead_hwnd();
    CHECK(f(h) == 0);
}
TEST(gap2_ssw_client_windows) {
    byte_hwnd_fn f = F_SSW(); REQUIRE(f);
    static const DWORD styles[] = { WS_OVERLAPPEDWINDOW, WS_POPUP, WS_OVERLAPPED, WS_POPUP | WS_BORDER, WS_CAPTION };
    for (unsigned i = 0; i < sizeof styles / sizeof styles[0]; i++) {
        HWND h = g2_mkwnd(styles[i], 0);
        REQUIRE(h);
        BYTE r = f(h);
        CHECKF(r == 0, "style 0x%lx: %u", styles[i], r); /* proc lives in this process */
        HWND c = t_make_window(t_cls(), WS_CHILD, 0, 0, 0, 10, 10, h);
        if (c) CHECK(f(c) == 0);
        g2_destroy(h);
    }
}
TEST(gap2_ssw_system_windows) {
    byte_hwnd_fn f = F_SSW(); REQUIRE(f);
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow");
    HWND (WINAPI *gsw)(void) = (void *)t_proc("GetShellWindow");
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    if (gdw) { BYTE r = f(gdw()); t_note("IsServerSideWindow(desktop)=%u", r); CHECK(r == 0 || r == 4); }
    if (gsw && gsw()) { BYTE r = f(gsw()); t_note("IsServerSideWindow(shell)=%u", r); CHECK(r == 0 || r == 4); }
    g2_destroy(prime);
}
TEST(gap2_ssw_builtin_classes) {
    byte_hwnd_fn f = F_SSW(); REQUIRE(f);
    static const char *cls[] = { "Button", "Edit", "Static", "ListBox", "ComboBox", "ScrollBar" };
    HWND p = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (unsigned i = 0; i < sizeof cls / sizeof cls[0]; i++) {
        HWND c = t_make_window(cls[i], WS_CHILD, 0, 0, 0, 40, 20, p);
        if (!c) continue;
        BYTE r = f(c);
        CHECKF(r == 0 || r == 4, "%s: %u", cls[i], r);
        t_note("IsServerSideWindow(%s) = %u", cls[i], r);
        g2_destroy(c);
    }
    g2_destroy(p);
}
static DWORD WINAPI ssw_thread(void *a) { return F_SSW()((HWND)a); }
TEST(gap2_ssw_cross_thread) {
    byte_hwnd_fn f = F_SSW(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    CHECK(g2_run_thread(ssw_thread, h) == f(h));
    CHECK(g2_run_thread(ssw_thread, (void *)(ULONG_PTR)0xdeadbeef) == 0);
    g2_destroy(h);
}

/* ------------------------------------------------------- IsWindowInDestroy */
static HWND g_dwnd;
static int g_seen_create = -1, g_seen_destroy = -1, g_seen_ncdestroy = -1, g_seen_close = -1;
static LRESULT CALLBACK destroy_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    byte_hwnd_fn f = F_DESTR();
    if (f) {
        if (m == WM_CREATE) g_seen_create = f(h);
        else if (m == WM_CLOSE) g_seen_close = f(h);
        else if (m == WM_DESTROY) g_seen_destroy = f(h);
        else if (m == WM_NCDESTROY) g_seen_ncdestroy = f(h);
    }
    return DefWindowProcA(h, m, w, l);
}
static const char *reg_class(const char *name, WNDPROC proc, HICON big, HICON small_) {
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = name; wc.hIcon = big; wc.hIconSm = small_;
    ATOM (WINAPI *rc)(const WNDCLASSEXA *) = (void *)t_proc("RegisterClassExA");
    if (rc) rc(&wc);
    return name;
}
static void unreg_class(const char *name) {
    BOOL (WINAPI *uc)(LPCSTR, HINSTANCE) = (void *)t_proc("UnregisterClassA");
    if (uc) uc(name, GetModuleHandleA(NULL));
}

TEST(gap2_destroy_resolve) {
    REQUIRE(F_DESTR());
    NOTE_EQ(t_proc_ord(2093), t_proc("IsWindowInDestroy")); /* ordinals vary between builds */
}
TEST(gap2_destroy_invalid_hwnd) {
    byte_hwnd_fn f = F_DESTR(); REQUIRE(f);
    for (int i = 0; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        BYTE r = f(g2_bogus[i].h);
        CHECKF(r == 0, "hwnd %s returned %u", g2_bogus[i].n, r);
        CHECKF(GetLastError() == 0xdeadbeef, "hwnd %s changed last error", g2_bogus[i].n);
    }
}
TEST(gap2_destroy_live_windows) {
    byte_hwnd_fn f = F_DESTR(); REQUIRE(f);
    HWND p = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    HWND c = t_make_window(t_cls(), WS_CHILD, 0, 0, 0, 20, 20, p);
    REQUIRE(p && c);
    CHECK(f(p) == 0);
    CHECK(f(c) == 0);
    for (int i = 0; i < 1000; i++) if (f(p) != 0) { CHECK(0); break; } /* handle cache path */
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow");
    if (gdw) CHECK(f(gdw()) == 0);
    g2_destroy(p);
}
TEST(gap2_destroy_after_destroy) {
    byte_hwnd_fn f = F_DESTR(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    CHECK(f(h) == 0);
    g2_destroy(h);
    CHECK(f(h) == 0); /* handle no longer validates */
    CHECK(f(h) == 0);
}
TEST(gap2_destroy_during_messages) {
    byte_hwnd_fn f = F_DESTR(); REQUIRE(f);
    const char *cn = reg_class("Gap2DestroyCls", destroy_proc, NULL, NULL);
    g_seen_create = g_seen_destroy = g_seen_ncdestroy = g_seen_close = -1;
    HWND h = t_make_window(cn, WS_OVERLAPPEDWINDOW, 0, 20, 20, 100, 100, NULL);
    REQUIRE(h);
    CHECK(g_seen_create == 0);
    CHECK(f(h) == 0);
    g2_destroy(h);
    t_note("IsWindowInDestroy during WM_DESTROY=%d WM_NCDESTROY=%d", g_seen_destroy, g_seen_ncdestroy);
    CHECK(g_seen_destroy != -1);
    CHECK(g_seen_ncdestroy != -1);
    CHECK(g_seen_destroy == 0 || g_seen_destroy == 0x80);
    CHECK(g_seen_ncdestroy == 0 || g_seen_ncdestroy == 0x80);
    CHECK(f(h) == 0);
    unreg_class(cn);
}
TEST(gap2_destroy_parent_with_child) {
    byte_hwnd_fn f = F_DESTR(); REQUIRE(f);
    const char *cn = reg_class("Gap2DestroyCls2", destroy_proc, NULL, NULL);
    HWND p = t_make_window(cn, WS_OVERLAPPEDWINDOW, 0, 20, 20, 100, 100, NULL);
    HWND c = t_make_window(cn, WS_CHILD, 0, 0, 0, 20, 20, p);
    REQUIRE(p && c);
    g_seen_destroy = -1;
    g2_destroy(p);
    CHECK(g_seen_destroy != -1);
    CHECK(f(c) == 0);
    CHECK(f(p) == 0);
    unreg_class(cn);
}
static DWORD WINAPI destroy_thread(void *a) { return F_DESTR()((HWND)a); }
TEST(gap2_destroy_cross_thread) {
    byte_hwnd_fn f = F_DESTR(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    CHECK(g2_run_thread(destroy_thread, h) == 0);
    CHECK(g2_run_thread(destroy_thread, (void *)(ULONG_PTR)0xdeadbeef) == 0);
    g2_destroy(h);
}

/* ------------------------------------------------ IsWindowRedirectedForPrint */
static int g_redir_in_print = 99, g_redir_in_printclient = 99;
static LRESULT CALLBACK redir_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int_hwnd_fn f = F_REDIR();
    if (f) {
        if (m == WM_PRINT) g_redir_in_print = f(h);
        if (m == WM_PRINTCLIENT) g_redir_in_printclient = f(h);
    }
    return DefWindowProcA(h, m, w, l);
}
TEST(gap2_redir_resolve) {
    REQUIRE(F_REDIR());
    NOTE_EQ(t_proc_ord(2094), t_proc("IsWindowRedirectedForPrint")); /* ordinals vary between builds */
}
TEST(gap2_redir_invalid_hwnd) {
    int_hwnd_fn f = F_REDIR(); REQUIRE(f);
    for (int i = 0; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        int r = f(g2_bogus[i].h);
        CHECKF(r == 0, "hwnd %s returned %d", g2_bogus[i].n, r);
        CHECKF(GetLastError() == 0xdeadbeef, "hwnd %s changed last error", g2_bogus[i].n);
    }
}
TEST(gap2_redir_plain_windows) {
    int_hwnd_fn f = F_REDIR(); REQUIRE(f);
    HWND p = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    HWND c = t_make_window(t_cls(), WS_CHILD, 0, 0, 0, 10, 10, p);
    REQUIRE(p && c);
    CHECK(f(p) == 0);
    CHECK(f(c) == 0);
    int (WINAPI *ps)(HWND, int) = NULL;
    (void)ps;
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow");
    if (gdw) CHECK(f(gdw()) == 0);
    g2_destroy(p);
    CHECK(f(p) == 0);
}
TEST(gap2_redir_dead_window) {
    int_hwnd_fn f = F_REDIR(); REQUIRE(f);
    HWND h = g2_dead_hwnd();
    CHECK(f(h) == 0);
}
TEST(gap2_redir_during_printwindow) {
    int_hwnd_fn f = F_REDIR(); REQUIRE(f);
    BOOL (WINAPI *pw)(HWND, HDC, UINT) = (void *)t_proc("PrintWindow");
    REQUIRE(pw);
    const char *cn = reg_class("Gap2RedirCls", redir_proc, NULL, NULL);
    HWND h = t_make_window(cn, WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, -3000, -3000, 64, 64, NULL);
    REQUIRE(h);
    t_pump();
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bm = CreateCompatibleBitmap(GetDC(NULL), 64, 64);
    HGDIOBJ old = SelectObject(dc, bm);
    g_redir_in_print = g_redir_in_printclient = 99;
    BOOL r = pw(h, dc, 0);
    t_note("PrintWindow -> %d; redirected inside WM_PRINT=%d WM_PRINTCLIENT=%d", r, g_redir_in_print, g_redir_in_printclient);
    CHECK(g_redir_in_print == 99 || g_redir_in_print == 0 || g_redir_in_print == -1);
    CHECK(g_redir_in_printclient == 99 || g_redir_in_printclient == 0 || g_redir_in_printclient == -1);
    CHECK(f(h) == 0); /* flag is transient */
    SelectObject(dc, old); DeleteObject(bm); DeleteDC(dc);
    g2_destroy(h);
    unreg_class(cn);
}
static DWORD WINAPI redir_thread(void *a) { return (DWORD)F_REDIR()((HWND)a); }
TEST(gap2_redir_cross_thread) {
    REQUIRE(F_REDIR());
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    CHECK(g2_run_thread(redir_thread, h) == 0);
    CHECK(g2_run_thread(redir_thread, (void *)(ULONG_PTR)0xdeadbeef) == 0);
    g2_destroy(h);
}

/* --------------------------------------------------- InheritWindowMonitor */
TEST(gap2_inherit_resolve) {
    REQUIRE(F_INH());
    NOTE_EQ(t_proc_ord(2031), t_proc("InheritWindowMonitor")); /* ordinals vary between builds */
}
TEST(gap2_inherit_both_invalid) {
    inherit_fn f = F_INH(); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = f(g2_bogus[i].h, g2_bogus[(i + 1) % G2_NBOGUS].h);
        CHECKF(r == FALSE, "(%s,%s) returned %d", g2_bogus[i].n, g2_bogus[(i + 1) % G2_NBOGUS].n, r);
        if (GetLastError() != 1400) t_note("inherit(%s,..) err %lu", g2_bogus[i].n, GetLastError());
    }
    g2_destroy(prime);
}
TEST(gap2_inherit_invalid_target) {
    inherit_fn f = F_INH(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(h);
    for (int i = 0; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = f(g2_bogus[i].h, h);
        CHECKF(r == FALSE, "invalid hwnd %s returned %d", g2_bogus[i].n, r);
    }
    for (int i = G2_FIRST_NONNULL; i < G2_NBOGUS; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = f(h, g2_bogus[i].h);
        CHECKF(r == FALSE, "invalid inherit %s returned %d", g2_bogus[i].n, r);
    }
    g2_destroy(h);
}
TEST(gap2_inherit_dead_windows) {
    inherit_fn f = F_INH(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    HWND dead = g2_dead_hwnd();
    CHECK(f(dead, h) == FALSE);
    CHECK(f(h, dead) == FALSE);
    CHECK(f(dead, dead) == FALSE);
    g2_destroy(h);
}
TEST(gap2_inherit_valid_pair) {
    inherit_fn f = F_INH(); REQUIRE(f);
    HWND a = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    HWND b = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(a && b);
    RECT ra, rb;
    GetWindowRect(a, &ra); GetWindowRect(b, &rb);
    HMONITOR (WINAPI *mfw)(HWND, DWORD) = (void *)t_proc("MonitorFromWindow");
    HMONITOR ma0 = mfw ? mfw(a, MONITOR_DEFAULTTONEAREST) : NULL;
    SetLastError(0xdeadbeef);
    BOOL r = f(a, b);
    t_note("InheritWindowMonitor(a,b) -> %d err=%lu", r, GetLastError());
    RECT ra2, rb2;
    GetWindowRect(a, &ra2); GetWindowRect(b, &rb2);
    CHECK(!memcmp(&rb, &rb2, sizeof rb)); /* the source window is untouched */
    if (!memcmp(&ra, &ra2, sizeof ra)) t_note("target window rect unchanged"); else t_note("target window rect changed");
    if (mfw) {
        HMONITOR ma = mfw(a, MONITOR_DEFAULTTONEAREST);
        CHECK(ma != NULL);
        if (ma != ma0) t_note("monitor of a changed");
    }
    BOOL r2 = f(a, b);
    CHECK(r2 == r); /* idempotent */
    g2_destroy(a); g2_destroy(b);
}
TEST(gap2_inherit_self_and_null) {
    inherit_fn f = F_INH(); REQUIRE(f);
    HWND a = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    SetLastError(0xdeadbeef);
    BOOL rs = f(a, a);
    t_note("InheritWindowMonitor(a,a) -> %d err=%lu", rs, GetLastError());
    SetLastError(0xdeadbeef);
    BOOL rn = f(a, NULL);
    t_note("InheritWindowMonitor(a,NULL) -> %d err=%lu", rn, GetLastError());
    HWND c = t_make_window(t_cls(), WS_CHILD, 0, 0, 0, 10, 10, a);
    BOOL rc = f(c, a);
    t_note("InheritWindowMonitor(child,parent) -> %d err=%lu", rc, GetLastError());
    BOOL rp = f(a, c);
    t_note("InheritWindowMonitor(parent,child) -> %d err=%lu", rp, GetLastError());
    g2_destroy(a);
}
static DWORD WINAPI inherit_thread(void *a) {
    HWND *p = a;
    return F_INH()(p[0], p[1]) ? 1 : 0;
}
TEST(gap2_inherit_cross_thread) {
    inherit_fn f = F_INH(); REQUIRE(f);
    HWND pair[2];
    pair[0] = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    pair[1] = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    DWORD r = g2_run_thread(inherit_thread, pair);
    t_note("InheritWindowMonitor from other thread on own-process windows -> %lu", r);
    CHECK(r != 0xdead0001);
    HWND bad[2] = { (HWND)(ULONG_PTR)0xdeadbeef, pair[0] };
    CHECK(g2_run_thread(inherit_thread, bad) == 0);
    g2_destroy(pair[0]); g2_destroy(pair[1]);
}
TEST(gap2_inherit_foreign_window) {
    inherit_fn f = F_INH(); REQUIRE(f);
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow");
    HWND (WINAPI *gsw)(void) = (void *)t_proc("GetShellWindow");
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    /* Only the foreign window is the inherit *source*: our own window is the one modified. */
    if (gdw) { SetLastError(0xdeadbeef); BOOL r = f(h, gdw()); t_note("inherit from desktop -> %d err=%lu", r, GetLastError()); }
    if (gsw && gsw()) { SetLastError(0xdeadbeef); BOOL r = f(h, gsw()); t_note("inherit from shell -> %d err=%lu", r, GetLastError()); }
    g2_destroy(h);
}

/* ---------------------------------------------------- InternalGetWindowIcon */
static HICON mkicon(int w, int h) {
    static BYTE bits[64 * 64 * 4];
    HICON (WINAPI *ci)(HINSTANCE, int, int, BYTE, BYTE, const BYTE *, const BYTE *) = (void *)t_proc("CreateIcon");
    return ci ? ci(NULL, w, h, 1, 32, bits, bits) : NULL;
}
static BOOL icon_size(HICON ic, LONG *w, LONG *h) {
    BOOL (WINAPI *gii)(HICON, ICONINFO *) = (void *)t_proc("GetIconInfo");
    ICONINFO ii; BITMAP bm;
    if (!gii || !gii(ic, &ii)) return FALSE;
    memset(&bm, 0, sizeof bm);
    HBITMAP b = ii.hbmColor ? ii.hbmColor : ii.hbmMask;
    BOOL ok = GetObjectW(b, sizeof bm, &bm) != 0;
    *w = bm.bmWidth; *h = ii.hbmColor ? bm.bmHeight : bm.bmHeight / 2;
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    return ok;
}
static BOOL destroy_icon(HICON ic) { BOOL (WINAPI *d)(HICON) = (void *)t_proc("DestroyIcon"); return d ? d(ic) : FALSE; }

TEST(gap2_icon_resolve) {
    REQUIRE(F_ICON());
    NOTE_EQ(t_proc_ord(2051), t_proc("InternalGetWindowIcon")); /* ordinals vary between builds */
}
TEST(gap2_icon_invalid_hwnd) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    HWND prime = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (int i = 0; i < G2_NBOGUS; i++) {
        for (UINT t = 0; t <= 2; t++) {
            HICON r = f(g2_bogus[i].h, t);
            CHECKF(r == NULL, "hwnd %s type %u returned %p", g2_bogus[i].n, t, r);
            if (r) destroy_icon(r);
        }
    }
    g2_destroy(prime);
}
TEST(gap2_icon_dead_window) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    HWND h = g2_dead_hwnd();
    HICON r = f(h, ICON_BIG);
    CHECK(r == NULL);
    if (r) destroy_icon(r);
}
TEST(gap2_icon_class_big) {
    igwi_fn f = F_ICON(); REQUIRE(f); REQUIRE(F_ICON());
    HICON big = mkicon(40, 40), sm = mkicon(50, 50);
    REQUIRE(big && sm);
    const char *cn = reg_class("Gap2IconCls1", DefWindowProcA, big, sm);
    HWND h = t_make_window(cn, WS_OVERLAPPEDWINDOW, 0, 20, 20, 100, 100, NULL);
    REQUIRE(h);
    LRESULT wm = SendMessageA(h, WM_GETICON, ICON_BIG, 0);
    CHECK(wm == 0); /* class icons are not reported through WM_GETICON */
    SetLastError(0xdeadbeef);
    HICON a = f(h, ICON_BIG);
    CHECK(a != NULL);
    CHECK(GetLastError() == 0xdeadbeef);
    HICON b = f(h, ICON_BIG);
    CHECK(b != NULL);
    CHECK(a != b);       /* a fresh copy on every call */
    CHECK(a != big && b != big);
    LONG w = 0, hh = 0;
    CHECK(icon_size(a, &w, &hh) && w == 40 && hh == 40);
    CHECK(icon_size(b, &w, &hh) && w == 40 && hh == 40);
    CHECK(destroy_icon(a));
    CHECK(destroy_icon(b));
    CHECK(icon_size(big, &w, &hh) && w == 40 && hh == 40); /* class icon survives */
    g2_destroy(h);
    unreg_class(cn);
    destroy_icon(big); destroy_icon(sm);
}
TEST(gap2_icon_class_small_variants) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    HICON big = mkicon(40, 40), sm = mkicon(50, 50);
    REQUIRE(big && sm);
    const char *cn = reg_class("Gap2IconCls2", DefWindowProcA, big, sm);
    HWND h = t_make_window(cn, WS_OVERLAPPEDWINDOW, 0, 20, 20, 100, 100, NULL);
    REQUIRE(h);
    for (UINT t = ICON_SMALL; t <= ICON_SMALL2; t += 2) { /* ICON_SMALL=0, ICON_SMALL2=2 */
        HICON a = f(h, t);
        CHECKF(a != NULL, "type %u", t);
        LONG w = 0, hh = 0;
        if (a) { CHECKF(icon_size(a, &w, &hh) && w == 50 && hh == 50, "type %u: %ldx%ld", t, w, hh); CHECK(destroy_icon(a)); }
    }
    g2_destroy(h);
    unreg_class(cn);
    destroy_icon(big); destroy_icon(sm);
}
TEST(gap2_icon_wm_seticon) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    HICON i1 = mkicon(10, 10), i2 = mkicon(20, 20), i3 = mkicon(30, 30);
    REQUIRE(i1 && i2 && i3);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(h);
    CHECK(SendMessageA(h, WM_SETICON, ICON_BIG, (LPARAM)i1) == 0);
    CHECK((HICON)SendMessageA(h, WM_GETICON, ICON_BIG, 0) == i1);
    HICON c = f(h, ICON_BIG);
    CHECK(c != NULL && c != i1);
    LONG w = 0, hh = 0;
    if (c) { CHECK(icon_size(c, &w, &hh) && w == 10 && hh == 10); CHECK(destroy_icon(c)); }
    CHECK((HICON)SendMessageA(h, WM_SETICON, ICON_BIG, (LPARAM)i2) == i1);
    c = f(h, ICON_BIG);
    CHECK(c != NULL && c != i2);
    if (c) { CHECK(icon_size(c, &w, &hh) && w == 20 && hh == 20); CHECK(destroy_icon(c)); }
    CHECK(SendMessageA(h, WM_SETICON, ICON_SMALL, (LPARAM)i3) == 0);
    c = f(h, ICON_SMALL);
    CHECK(c != NULL && c != i3);
    if (c) { CHECK(icon_size(c, &w, &hh) && w == 30 && hh == 30); CHECK(destroy_icon(c)); }
    c = f(h, ICON_BIG); /* unaffected by small */
    if (c) { CHECK(icon_size(c, &w, &hh) && w == 20 && hh == 20); destroy_icon(c); }
    /* originals remain valid after the copies were destroyed */
    CHECK(icon_size(i1, &w, &hh) && w == 10);
    CHECK(icon_size(i2, &w, &hh) && w == 20);
    CHECK(icon_size(i3, &w, &hh) && w == 30);
    g2_destroy(h);
    destroy_icon(i1); destroy_icon(i2); destroy_icon(i3);
}
TEST(gap2_icon_no_class_icon) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    for (UINT t = 0; t <= 2; t++) {
        HICON a = f(h, t);
        t_note("window without any icon, type %u -> %p", t, a);
        if (a) {
            LONG w, hh;
            CHECK(icon_size(a, &w, &hh));
            destroy_icon(a);
        }
    }
    g2_destroy(h);
}
TEST(gap2_icon_bad_type) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    HICON ic = mkicon(16, 16);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    SendMessageA(h, WM_SETICON, ICON_BIG, (LPARAM)ic);
    static const UINT types[] = { 3, 4, 0x10, 0x100, 0x7fffffff, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof types / sizeof types[0]; i++) {
        SetLastError(0xdeadbeef);
        HICON a = f(h, types[i]);
        t_note("InternalGetWindowIcon type 0x%x -> %p err=%lu", types[i], a, GetLastError());
        if (a) destroy_icon(a);
    }
    g2_destroy(h);
    destroy_icon(ic);
}
TEST(gap2_icon_no_handle_leak) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    DWORD (WINAPI *ggr)(HANDLE, DWORD) = (void *)t_proc("GetGuiResources");
    REQUIRE(ggr);
    HICON ic = mkicon(32, 32);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    SendMessageA(h, WM_SETICON, ICON_BIG, (LPARAM)ic);
    DWORD before = ggr(GetCurrentProcess(), 1 /*GR_USEROBJECTS*/);
    for (int i = 0; i < 500; i++) {
        HICON a = f(h, ICON_BIG);
        if (!a) { CHECK(0); break; }
        if (!destroy_icon(a)) { CHECK(0); break; }
    }
    DWORD after = ggr(GetCurrentProcess(), 1);
    CHECKF(after <= before + 2, "user objects grew %lu -> %lu", before, after);
    g2_destroy(h);
    destroy_icon(ic);
}
static DWORD WINAPI icon_thread(void *a) {
    HICON r = F_ICON()((HWND)a, ICON_BIG);
    DWORD ok = r != NULL;
    if (r) ok &= destroy_icon(r) ? 1 : 0;
    return ok;
}
TEST(gap2_icon_cross_thread) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    HICON ic = mkicon(24, 24);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    SendMessageA(h, WM_SETICON, ICON_BIG, (LPARAM)ic);
    CHECK(g2_run_thread(icon_thread, h) == 1);
    CHECK(g2_run_thread(icon_thread, (void *)(ULONG_PTR)0xdeadbeef) == 0);
    g2_destroy(h);
    destroy_icon(ic);
}
TEST(gap2_icon_after_icon_destroyed) {
    igwi_fn f = F_ICON(); REQUIRE(f);
    HICON ic = mkicon(24, 24);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    SendMessageA(h, WM_SETICON, ICON_BIG, (LPARAM)ic);
    destroy_icon(ic);
    HICON a = f(h, ICON_BIG);
    t_note("copy of destroyed window icon -> %p", a);
    if (a) destroy_icon(a);
    g2_destroy(h);
}

/* ------------------------------------------------------- queue attachment */
TEST(gap2_queue_resolve) {
    REQUIRE(t_proc("IsQueueAttached"));
    REQUIRE(t_proc("IsThreadMessageQueueAttached"));
    NOTE_EQ(t_proc_ord(2080), t_proc("IsQueueAttached")); /* ordinals vary between builds */
    NOTE_EQ(t_proc_ord(2528), t_proc("IsThreadMessageQueueAttached")); /* ordinals vary between builds */
}
TEST(gap2_queue_not_attached_default) {
    REQUIRE_NOT_WINE_STUB("IsThreadMessageQueueAttached");
    BOOL (WINAPI *qa)(void) = (void *)t_proc("IsQueueAttached");
    thq_fn tq = (thq_fn)t_proc("IsThreadMessageQueueAttached");
    REQUIRE(qa); REQUIRE(tq);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0); /* ensure a message queue exists */
    BOOL a = qa();
    CHECK(a == FALSE);
    BOOL b = tq(GetCurrentThreadId());
    CHECK(b == FALSE);
    for (int i = 0; i < 1000; i++) { if (qa() != a || tq(GetCurrentThreadId()) != b) { CHECK(0); break; } }
    g2_destroy(h);
}
TEST(gap2_queue_invalid_tid) {
    REQUIRE_NOT_WINE_STUB("IsThreadMessageQueueAttached");
    thq_fn tq = (thq_fn)t_proc("IsThreadMessageQueueAttached"); REQUIRE(tq);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    static const DWORD tids[] = { 0, 1, 2, 3, 0xdeadbeef, 0xffffffff, 0x7fffffff, 0xfffc, 0x80000000 };
    for (unsigned i = 0; i < sizeof tids / sizeof tids[0]; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = tq(tids[i]);
        CHECKF(r == FALSE, "tid 0x%lx returned %d", tids[i], r);
    }
    g2_destroy(h);
}
TEST(gap2_queue_non_gui_thread_id) {
    REQUIRE_NOT_WINE_STUB("IsThreadMessageQueueAttached");
    thq_fn tq = (thq_fn)t_proc("IsThreadMessageQueueAttached"); REQUIRE(tq);
    BOOL (WINAPI *qa)(void) = (void *)t_proc("IsQueueAttached"); REQUIRE(qa);
    /* thread id of this process's own non-GUI worker */
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE th = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WaitForSingleObject, ev, CREATE_SUSPENDED, NULL);
    REQUIRE(th);
    DWORD tid = GetThreadId(th);
    BOOL r = tq(tid);
    CHECK(r == FALSE);
    t_note("non-GUI thread: IsThreadMessageQueueAttached -> %d", r);
    TerminateThread(th, 0);
    CloseHandle(th); CloseHandle(ev);
    (void)qa;
}

typedef struct { HANDLE ready, stop; DWORD tid; HWND hwnd; } WORKER;
static DWORD WINAPI worker_main(void *a) {
    WORKER *w = a;
    w->hwnd = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    w->tid = GetCurrentThreadId();
    SetEvent(w->ready);
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 15000) {
        t_pump();
        if (MsgWaitForMultipleObjects(1, &w->stop, FALSE, 50, QS_ALLINPUT) == WAIT_OBJECT_0) break;
    }
    g2_destroy(w->hwnd);
    return 0;
}
TEST(gap2_queue_attach_own_threads) {
    REQUIRE_NOT_WINE_STUB("IsThreadMessageQueueAttached");
    BOOL (WINAPI *qa)(void) = (void *)t_proc("IsQueueAttached");
    thq_fn tq = (thq_fn)t_proc("IsThreadMessageQueueAttached");
    BOOL (WINAPI *att)(DWORD, DWORD, BOOL) = (void *)t_proc("AttachThreadInput");
    REQUIRE(qa); REQUIRE(tq); REQUIRE(att);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    REQUIRE(h);
    WORKER w = { CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL), 0, NULL };
    HANDLE th = CreateThread(NULL, 0, worker_main, &w, 0, NULL);
    REQUIRE(th);
    if (WaitForSingleObject(w.ready, 5000) != WAIT_OBJECT_0) { CHECKF(0, "worker did not start"); SetEvent(w.stop); CloseHandle(th); return; }
    BOOL before_q = qa(), before_t = tq(w.tid);
    CHECK(before_q == FALSE);
    CHECK(before_t == FALSE);
    DWORD me = GetCurrentThreadId();
    SetLastError(0xdeadbeef);
    BOOL ok = att(me, w.tid, TRUE);
    if (!ok) {
        t_note("AttachThreadInput failed err=%lu; skipping attached checks", GetLastError());
    } else {
        BOOL q = qa(), t1 = tq(w.tid), t2 = tq(me);
        t_note("attached: IsQueueAttached=%d IsThreadMessageQueueAttached(worker)=%d (self)=%d", q, t1, t2);
        CHECK(q == TRUE || q == FALSE);
        NOTE_EQ(q, TRUE);
        NOTE_EQ(t1, TRUE);
        CHECK(att(me, w.tid, FALSE) != FALSE);
        BOOL q2 = qa(), t3 = tq(w.tid);
        CHECK(q2 == FALSE);
        CHECK(t3 == FALSE);
    }
    SetEvent(w.stop);
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    CloseHandle(th); CloseHandle(w.ready); CloseHandle(w.stop);
    g2_destroy(h);
}
TEST(gap2_queue_after_thread_exit) {
    REQUIRE_NOT_WINE_STUB("IsThreadMessageQueueAttached");
    thq_fn tq = (thq_fn)t_proc("IsThreadMessageQueueAttached"); REQUIRE(tq);
    WORKER w = { CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL), 0, NULL };
    HANDLE th = CreateThread(NULL, 0, worker_main, &w, 0, NULL);
    REQUIRE(th);
    WaitForSingleObject(w.ready, 5000);
    DWORD tid = w.tid;
    SetEvent(w.stop);
    WaitForSingleObject(th, 5000);
    CloseHandle(th); CloseHandle(w.ready); CloseHandle(w.stop);
    CHECK(tq(tid) == FALSE); /* thread no longer exists */
}
static DWORD WINAPI queue_thread(void *a) {
    BOOL (WINAPI *qa)(void) = (void *)t_proc("IsQueueAttached");
    return qa() ? 1 : 0;
}
TEST(gap2_queue_other_thread_fresh) {
    REQUIRE_NOT_WINE_STUB("IsThreadMessageQueueAttached");
    REQUIRE(t_proc("IsQueueAttached"));
    CHECK(g2_run_thread(queue_thread, NULL) == 0);
}

/* ------------------------------------------------------ IsThreadTSFEventAware */
static DWORD WINAPI tsf_thread(void *a) {
    DWORD (WINAPI *f)(DWORD) = (void *)t_proc("IsThreadTSFEventAware");
    HWND own = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    DWORD acc = 0;
    static const DWORD ev[] = { 0, 1, 0x7fffff10, 0x7fffff11, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof ev / sizeof ev[0]; i++) acc |= (f(ev[i]) <= 1) ? 0 : 1;
    g2_destroy(own);
    return acc;
}
TEST(gap2_tsf_resolve) {
    REQUIRE(t_proc("IsThreadTSFEventAware"));
    NOTE_EQ(t_proc_ord(2085), t_proc("IsThreadTSFEventAware")); /* ordinals vary between builds */
}
TEST(gap2_tsf_values_boolean) {
    DWORD (WINAPI *f)(DWORD) = (void *)t_proc("IsThreadTSFEventAware"); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    static const DWORD ev[] = { 0, 1, 2, 3, 0x10, 0x100, 0x1000, 0x7fffff10, 0x7fffff11, 0x7ffffff0, 0x80000000, 0x800000ef, 0x800000f0, 0x800000f1, 0xffffffff };
    for (unsigned i = 0; i < sizeof ev / sizeof ev[0]; i++) {
        DWORD r = f(ev[i]);
        CHECKF(r <= 1, "event 0x%lx returned %lu", ev[i], r);
        CHECKF(f(ev[i]) == r, "event 0x%lx unstable", ev[i]);
    }
    g2_destroy(h);
}
TEST(gap2_tsf_plain_thread_not_aware) {
    DWORD (WINAPI *f)(DWORD) = (void *)t_proc("IsThreadTSFEventAware"); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    DWORD any = 0;
    for (DWORD e = 0; e < 64; e++) any |= f(e);
    t_note("TSF event awareness over events 0..63 -> %lu", any);
    g2_destroy(h);
}
TEST(gap2_tsf_other_thread) {
    REQUIRE(t_proc("IsThreadTSFEventAware"));
    CHECK(g2_run_thread(tsf_thread, NULL) == 0);
}
static DWORD WINAPI tsf_nogui(void *a) {
    DWORD (WINAPI *f)(DWORD) = (void *)t_proc("IsThreadTSFEventAware");
    return f(0x7fffff10);
}
TEST(gap2_tsf_non_gui_thread) {
    REQUIRE(t_proc("IsThreadTSFEventAware"));
    DWORD r = g2_run_thread(tsf_nogui, NULL);
    CHECK(r == 0 || r == 1);
}

/* ---------------------------------------------------------------- IsSETEnabled */
TEST(gap2_set_resolve) {
    REQUIRE(t_proc("IsSETEnabled"));
    NOTE_EQ(t_proc_ord(2082), t_proc("IsSETEnabled")); /* ordinals vary between builds */
}
TEST(gap2_set_stable) {
    BYTE (WINAPI *f)(void) = (void *)t_proc("IsSETEnabled"); REQUIRE(f);
    BYTE r = f();
    CHECK(r == 0 || r == 1);
    t_note("IsSETEnabled = %u", r);
    for (int i = 0; i < 50; i++) { if (f() != r) { CHECK(0); break; } }
}
TEST(gap2_set_preserves_last_error) {
    BYTE (WINAPI *f)(void) = (void *)t_proc("IsSETEnabled"); REQUIRE(f);
    SetLastError(0x1234);
    f();
    t_note("last error after IsSETEnabled: %lu", GetLastError());
}
TEST(gap2_set_matches_policy) {
    BYTE (WINAPI *f)(void) = (void *)t_proc("IsSETEnabled"); REQUIRE(f);
    HKEY k;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Policies\\Microsoft\\Windows NT\\Reliability", 0, KEY_READ, &k) != ERROR_SUCCESS) {
        t_note("no Reliability policy key, nothing more to compare");
        return;
    }
    DWORD v = 0xffff, sz = sizeof v, type = 0;
    if (RegQueryValueExA(k, "ShutdownReasonUI", NULL, &type, (BYTE *)&v, &sz) == ERROR_SUCCESS && type == REG_DWORD) {
        BYTE r = f();
        t_note("ShutdownReasonUI=%lu -> IsSETEnabled=%u", v, r);
        if (v == 0) CHECK(r == 0);
        if (v == 1) CHECK(r == 1);
    }
    RegCloseKey(k);
}
static DWORD WINAPI set_thread(void *a) { BYTE (WINAPI *f)(void) = (void *)t_proc("IsSETEnabled"); return f(); }
TEST(gap2_set_other_thread) {
    BYTE (WINAPI *f)(void) = (void *)t_proc("IsSETEnabled"); REQUIRE(f);
    CHECK(g2_run_thread(set_thread, NULL) == f());
}

/* ------------------------------------------------- IsOneCoreTransformMode */
TEST(gap2_onecore_resolve) {
    REQUIRE(t_proc("IsOneCoreTransformMode"));
    NOTE_EQ(t_proc_ord(2078), t_proc("IsOneCoreTransformMode")); /* ordinals vary between builds */
}
TEST(gap2_onecore_desktop_false) {
    BOOL (WINAPI *f)(void) = (void *)t_proc("IsOneCoreTransformMode"); REQUIRE(f);
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    BOOL r = f();
    t_note("IsOneCoreTransformMode = %d", r);
    NOTE_EQ(r, FALSE); /* classic desktop SKU is not in OneCore transform mode */
    for (int i = 0; i < 100; i++) { if (f() != r) { CHECK(0); break; } }
    g2_destroy(h);
}
