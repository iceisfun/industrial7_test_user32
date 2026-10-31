/* menus: TrackPopupMenu(Ex), menu loop notifications, owner draw, SC_* handling.
 * Every modal loop is bounded: a timer on the owner window drives scripted (posted, never injected)
 * keys then calls EndMenu, and a watchdog thread posts WM_CANCELMODE if anything is still running. */
#include "mh.h"

#define TID 77
#define MAXEV 600

typedef struct { UINT msg; WPARAM w; LPARAM l; ULONG_PTR a, b, c, d; } ev_t;
static ev_t g_ev[MAXEV];
static int g_nev;
typedef struct { UINT msg; WPARAM w; LPARAM l; } act_t;
static act_t g_script[16];
static int g_nscript, g_step;
static volatile LONG g_done;
static LRESULT g_menuchar_ret;
static int g_measure_w = 90, g_measure_h = 20;
static HMENU g_add_in_initpopup;          /* menu to which an item is appended during WM_INITMENUPOPUP */
static int g_added;
static int g_endmenu_ret = -1;

static void lg(UINT m, WPARAM w, LPARAM l, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d) {
    if (g_nev < MAXEV) { ev_t *e = &g_ev[g_nev++]; e->msg = m; e->w = w; e->l = l; e->a = a; e->b = b; e->c = c; e->d = d; }
}
static int cnt(UINT m) { int n = 0; for (int i = 0; i < g_nev; i++) if (g_ev[i].msg == m) n++; return n; }
static ev_t *nth(UINT m, int n) { for (int i = 0; i < g_nev; i++) if (g_ev[i].msg == m && n-- == 0) return &g_ev[i]; return NULL; }
static int idx(UINT m) { for (int i = 0; i < g_nev; i++) if (g_ev[i].msg == m) return i; return -1; }

static LRESULT CALLBACK tproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_TIMER:
        if (w == TID) {
            if (g_step < g_nscript) { U(PostMessageA)(h, g_script[g_step].msg, g_script[g_step].w, g_script[g_step].l); }
            else if (g_step == g_nscript) { g_endmenu_ret = (int)U(EndMenu)(); }
            else if (g_step < 40) { U(PostMessageA)(h, WM_CANCELMODE, 0, 0); }
            else U(KillTimer)(h, TID);
            g_step++;
            return 0;
        }
        break;
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *)l;
        lg(m, w, l, mi->CtlType, mi->itemID, mi->itemData, mi->CtlID);
        mi->itemWidth = g_measure_w; mi->itemHeight = g_measure_h;
        return TRUE;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)l;
        lg(m, w, l, d->CtlType, d->itemID, (d->itemAction << 16) | d->itemState, d->itemData);
        return TRUE;
    }
    case WM_MENUCHAR: lg(m, w, l, 0, 0, 0, 0); return g_menuchar_ret;
    case WM_INITMENUPOPUP:
        lg(m, w, l, 0, 0, 0, 0);
        if (g_add_in_initpopup && (HMENU)w == g_add_in_initpopup) { U(AppendMenuA)(g_add_in_initpopup, MF_STRING, 9000 + g_added, "dynamic"); g_added++; }
        return 0;
    case WM_INITMENU: case WM_MENUSELECT: case WM_ENTERMENULOOP: case WM_EXITMENULOOP: case WM_UNINITMENUPOPUP:
    case WM_COMMAND: case WM_SYSCOMMAND: case WM_MENUCOMMAND: case WM_CLOSE: case WM_NEXTMENU: case WM_MENURBUTTONUP:
    case WM_MENUDRAG: case WM_MENUGETOBJECT: case WM_CANCELMODE:
        lg(m, w, l, 0, 0, 0, 0);
        if (m == WM_CLOSE) return 0;
        break;
    }
    return DefWindowProcA(h, m, w, l);
}

static const char *tcls(void) {
    static int done;
    if (!done) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = tproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "T32MenuTrack";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        U(RegisterClassA)(&wc);
        done = 1;
    }
    return "T32MenuTrack";
}

static HWND tw_make(HMENU bar) {
    HWND w = t_make_window(tcls(), WS_OVERLAPPEDWINDOW, 0, 120, 120, 520, 320, NULL);
    if (!w) return NULL;
    if (bar) U(SetMenu)(w, bar);
    U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    if (U(SetForegroundWindow)) U(SetForegroundWindow)(w);
    t_pump();
    return w;
}
static void tw_begin(void) { g_nev = 0; g_nscript = 0; g_step = 0; g_done = 0; g_menuchar_ret = 0; g_add_in_initpopup = NULL; g_added = 0; g_endmenu_ret = -1; }
static void tw_key(UINT msg, WPARAM vk) { if (g_nscript < 16) { g_script[g_nscript].msg = msg; g_script[g_nscript].w = vk; g_script[g_nscript].l = 1; g_nscript++; } }
static void tw_vk(WPARAM vk) { tw_key(WM_KEYDOWN, vk); tw_key(WM_KEYUP, vk); }

static HWND g_wd_hwnd;
static DWORD WINAPI watchdog(LPVOID p) {
    for (int i = 0; i < 100 && !g_done; i++) Sleep(50);              /* 5 s */
    for (int i = 0; i < 40 && !g_done; i++) { U(PostMessageA)(g_wd_hwnd, WM_CANCELMODE, 0, 0); Sleep(250); }
    return 0;
}
/* Starts timer + watchdog; call right before the blocking menu call. */
static HANDLE tw_arm(HWND w) {
    g_wd_hwnd = w; g_done = 0; g_step = 0;
    U(SetTimer)(w, TID, 120, NULL);
    return CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
}
static void tw_disarm(HWND w, HANDLE t) {
    g_done = 1;
    U(KillTimer)(w, TID);
    if (t) { WaitForSingleObject(t, 3000); CloseHandle(t); }
    t_pump();
}

#define REQ_TRACK() do { if (!(U(TrackPopupMenu) && U(CreatePopupMenu) && U(CreateMenu) && U(AppendMenuA) && U(PostMessageA) && U(SetTimer) && U(KillTimer) && U(EndMenu) && U(DestroyWindow) && U(RegisterClassA) && U(SetMenu) && U(ShowWindow) && U(GetMenuItemCount))) { t_skip("missing tracking exports"); return; } } while (0)

static UINT track(HWND w, HMENU m, UINT flags) {
    HANDLE t = tw_arm(w);
    UINT r = (UINT)U(TrackPopupMenu)(m, flags, 200, 200, 0, w, NULL);
    tw_disarm(w, t);
    return r;
}

TEST(menus_track_invalid_params) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(2);
    tw_begin();
    SetLastError(0);
    CHECK(!track(w, NULL, TPM_RETURNCMD));
    t_note("TrackPopupMenu(NULL menu) error %lu (1401 = ERROR_INVALID_MENU_HANDLE)", GetLastError());
    CHECK(!track(w, (HMENU)0xDEADBEEF, TPM_RETURNCMD));
    CHECK(!track(w, (HMENU)(LONG_PTR)-1, 0));
    HMENU gone = M_POP(); M_DESTROY(gone);
    CHECK(!track(w, gone, TPM_RETURNCMD));
    /* invalid owner windows */
    HANDLE t = tw_arm(w);
    SetLastError(0);
    BOOL r = U(TrackPopupMenu)(m, TPM_RETURNCMD, 100, 100, 0, NULL, NULL);
    DWORD e = GetLastError();
    tw_disarm(w, t);
    CHECK(!r);
    t_note("TrackPopupMenu(NULL hwnd) error %lu (1400 = ERROR_INVALID_WINDOW_HANDLE)", e);
    t = tw_arm(w);
    r = U(TrackPopupMenu)(m, TPM_RETURNCMD, 100, 100, 0, (HWND)0xDEADBEEF, NULL);
    tw_disarm(w, t);
    CHECK(!r);
    M_DESTROY(m);
    U(DestroyWindow)(w);
}

TEST(menus_track_cancel_returncmd) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    tw_begin();
    UINT r = track(w, m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN);
    EQ(r, 0);                             /* cancelled: zero */
    EQ(cnt(WM_COMMAND), 0);               /* TPM_RETURNCMD never posts WM_COMMAND */
    CHECK(M_IS(m)); EQ(M_CNT(m), 3);
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_cancel_plain_result) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    tw_begin();
    UINT r = track(w, m, TPM_LEFTALIGN | TPM_TOPALIGN);
    t_note("TrackPopupMenu cancelled without TPM_RETURNCMD -> %u", r);
    EQ(cnt(WM_COMMAND), 0);
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_message_sequence) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    tw_begin();
    track(w, m, TPM_RETURNCMD);
    int e = idx(WM_ENTERMENULOOP), i = idx(WM_INITMENU), p = idx(WM_INITMENUPOPUP), x = idx(WM_EXITMENULOOP);
    t_note("order: enter=%d initmenu=%d initpopup=%d exit=%d total=%d", e, i, p, x, g_nev);
    if (e >= 0) { CHECK(g_ev[e].w == TRUE); CHECK(x > e); }
    if (x >= 0) CHECK(g_ev[x].w == TRUE);
    if (i >= 0) { CHECK((HMENU)g_ev[i].w == m); if (e >= 0) CHECK(e < i); }
    if (p >= 0) {
        CHECK((HMENU)g_ev[p].w == m);
        CHECK(HIWORD(g_ev[p].l) == 0);        /* not the window (system) menu */
        if (i >= 0) CHECK(i < p);
    }
    if (cnt(WM_UNINITMENUPOPUP)) { CHECK((HMENU)nth(WM_UNINITMENUPOPUP, 0)->w == m); }
    EQ(cnt(WM_ENTERMENULOOP), cnt(WM_EXITMENULOOP));
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_nonotify) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    tw_begin();
    track(w, m, TPM_RETURNCMD | TPM_NONOTIFY);
    EQ(cnt(WM_INITMENUPOPUP), 0);
    EQ(cnt(WM_MENUSELECT), 0);
    t_note("TPM_NONOTIFY: INITMENU=%d ENTER=%d EXIT=%d", cnt(WM_INITMENU), cnt(WM_ENTERMENULOOP), cnt(WM_EXITMENULOOP));
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_empty_menu) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = M_POP();
    tw_begin();
    UINT r = track(w, m, TPM_RETURNCMD);
    EQ(r, 0);
    CHECK(M_IS(m));
    r = track(w, m, 0);
    t_note("TrackPopupMenu on an empty menu (no RETURNCMD) -> %u", r);
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_repeat_stable) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(2);
    for (int i = 0; i < 4; i++) {
        tw_begin();
        UINT r = track(w, m, TPM_RETURNCMD);
        CHECKF(r == 0, "round %d -> %u", i, r);
        EQ(M_CNT(m), 2);
    }
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_keys_select_item) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    tw_begin();
    tw_vk(VK_DOWN); tw_vk(VK_DOWN); tw_vk(VK_RETURN);
    UINT r = track(w, m, TPM_RETURNCMD);
    if (r == 0) t_note("posted keys did not drive the menu loop (result 0)");
    else {
        CHECKF(r >= 100 && r <= 102, "result %u", r);
        t_note("posted VK_DOWN x2 + VK_RETURN selected command %u", r);
    }
    EQ(cnt(WM_COMMAND), 0);
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_keys_post_wm_command) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    tw_begin();
    tw_vk(VK_DOWN); tw_vk(VK_RETURN);
    UINT r = track(w, m, 0);
    int n = cnt(WM_COMMAND);
    if (n == 0) t_note("no WM_COMMAND (keys not honoured), result %u", r);
    else {
        EQ(n, 1);
        ev_t *e = nth(WM_COMMAND, 0);
        CHECK(LOWORD(e->w) >= 100 && LOWORD(e->w) <= 102);
        EQ(HIWORD(e->w), 0);              /* from a menu */
        EQ(e->l, 0);
    }
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_keys_notifybypos) {
    REQ_TRACK();
    REQUIRE(U(SetMenuInfo));
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    MENUINFO mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi; mi.fMask = MIM_STYLE; mi.dwStyle = MNS_NOTIFYBYPOS;
    CHECK(U(SetMenuInfo)(m, &mi));
    tw_begin();
    tw_vk(VK_DOWN); tw_vk(VK_DOWN); tw_vk(VK_RETURN);
    track(w, m, 0);
    if (cnt(WM_MENUCOMMAND) == 0) t_note("no WM_MENUCOMMAND (keys not honoured)");
    else {
        ev_t *e = nth(WM_MENUCOMMAND, 0);
        CHECK(e->w <= 2);                 /* zero-based position */
        CHECK((HMENU)e->l == m);
        EQ(cnt(WM_COMMAND), 0);
    }
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_menuchar_unmatched) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    tw_begin();
    g_menuchar_ret = MAKELONG(1, 2);     /* MNC_EXECUTE (2), item 1 */
    tw_key(WM_CHAR, 'z');
    UINT r = track(w, m, TPM_RETURNCMD);
    if (cnt(WM_MENUCHAR) == 0) t_note("no WM_MENUCHAR (char not delivered to menu loop), result %u", r);
    else {
        ev_t *e = nth(WM_MENUCHAR, 0);
        EQ(LOWORD(e->w), 'z');
        CHECK((HMENU)e->l == m);
        t_note("WM_MENUCHAR HIWORD(wParam)=0x%x", HIWORD(e->w));
        EQ(r, 101);                       /* handler asked to execute item index 1 */
    }
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_mnemonic_selects) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = M_POP();
    U(AppendMenuA)(m, MF_STRING, 1, "&Alpha"); U(AppendMenuA)(m, MF_STRING, 2, "&Beta"); U(AppendMenuA)(m, MF_STRING, 3, "&Gamma");
    tw_begin();
    tw_key(WM_CHAR, 'b');
    UINT r = track(w, m, TPM_RETURNCMD);
    if (r == 0) t_note("mnemonic char not honoured (result 0)");
    else EQ(r, 2);
    EQ(cnt(WM_MENUCHAR), 0);
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_initpopup_may_modify) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(2);
    tw_begin();
    g_add_in_initpopup = m;
    track(w, m, TPM_RETURNCMD);
    int n = cnt(WM_INITMENUPOPUP);
    EQ(M_CNT(m), 2 + g_added);
    if (n == 0) t_note("WM_INITMENUPOPUP not delivered");
    else CHECK(g_added >= 1);
    g_add_in_initpopup = NULL;
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_submenu_opens) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = M_POP(), s = M_POP();
    mh_add(s, 501, "child1"); mh_add(s, 502, "child2");
    U(AppendMenuA)(m, MF_POPUP, (UINT_PTR)s, "Parent");
    mh_add(m, 600, "after");
    tw_begin();
    tw_vk(VK_DOWN); tw_vk(VK_RIGHT);
    UINT r = track(w, m, TPM_RETURNCMD);
    EQ(r, 0);
    int n = cnt(WM_INITMENUPOPUP);
    t_note("INITMENUPOPUP count with VK_DOWN,VK_RIGHT: %d", n);
    for (int i = 0; i < n; i++) {
        ev_t *e = nth(WM_INITMENUPOPUP, i);
        CHECK((HMENU)e->w == m || (HMENU)e->w == s);
    }
    if (n >= 2) CHECK((HMENU)nth(WM_INITMENUPOPUP, 1)->w == s);
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_menuselect_reports_item) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(3);
    tw_begin();
    tw_vk(VK_DOWN);
    track(w, m, TPM_RETURNCMD);
    int n = cnt(WM_MENUSELECT);
    t_note("WM_MENUSELECT count %d", n);
    for (int i = 0; i < n; i++) {
        ev_t *e = nth(WM_MENUSELECT, i);
        UINT id = LOWORD(e->w), fl = HIWORD(e->w);
        if (fl == 0xFFFF && e->l == 0) continue;          /* menu closed */
        CHECKF((HMENU)e->l == m, "menuselect %d hmenu %p", i, (void *)e->l);
        CHECKF(id >= 100 && id <= 102, "menuselect %d id %u", i, id);
        CHECK(!(fl & MF_POPUP) && !(fl & MF_DISABLED));
        CHECK(!(fl & MF_MOUSESELECT));
    }
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_ownerdraw_measure_and_draw) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = M_POP();
    U(AppendMenuA)(m, MF_OWNERDRAW, 41, (LPCSTR)(ULONG_PTR)0x11111111AAAAULL);
    U(AppendMenuA)(m, MF_OWNERDRAW, 42, (LPCSTR)(ULONG_PTR)0x2222);
    mh_add(m, 43, "text");
    tw_begin();
    track(w, m, TPM_RETURNCMD);
    int n = cnt(WM_MEASUREITEM);
    t_note("WM_MEASUREITEM count %d, WM_DRAWITEM count %d", n, cnt(WM_DRAWITEM));
    if (n) EQ(n >= 2, 1);
    for (int i = 0; i < n; i++) {
        ev_t *e = nth(WM_MEASUREITEM, i);
        EQ(e->a, ODT_MENU);
        CHECK(e->b == 41 || e->b == 42);
        CHECK(e->c == 0x11111111AAAAULL || e->c == 0x2222);
        CHECK((e->b == 41) == (e->c == 0x11111111AAAAULL));
        EQ(e->d, 0);                          /* CtlID is unused for menus */
        EQ(e->w, 0);
    }
    for (int i = 0; i < cnt(WM_DRAWITEM); i++) {
        ev_t *e = nth(WM_DRAWITEM, i);
        EQ(e->a, ODT_MENU);
        CHECK(e->b == 41 || e->b == 42);
        EQ(e->w, 0);
    }
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_tpmparams) {
    REQ_TRACK();
    REQUIRE(U(TrackPopupMenuEx));
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(2);
    TPMPARAMS pm; memset(&pm, 0, sizeof pm); pm.cbSize = sizeof pm; pm.rcExclude.left = 190; pm.rcExclude.top = 190; pm.rcExclude.right = 260; pm.rcExclude.bottom = 215;
    tw_begin();
    HANDLE t = tw_arm(w);
    BOOL r = U(TrackPopupMenuEx)(m, TPM_RETURNCMD | TPM_VERTICAL, 200, 200, w, &pm);
    tw_disarm(w, t);
    EQ(r, 0);
    tw_begin();
    t = tw_arm(w);
    r = U(TrackPopupMenuEx)(m, TPM_RETURNCMD, 200, 200, w, NULL);
    tw_disarm(w, t);
    EQ(r, 0);
    tw_begin();
    pm.cbSize = 4;
    t = tw_arm(w);
    SetLastError(0);
    r = U(TrackPopupMenuEx)(m, TPM_RETURNCMD, 200, 200, w, &pm);
    DWORD e = GetLastError();
    tw_disarm(w, t);
    t_note("TrackPopupMenuEx with TPMPARAMS.cbSize=4 -> %d err %lu", r, e);
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_track_alignment_flags_cancel) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU m = mh_make(2);
    static const UINT f[] = { TPM_LEFTALIGN, TPM_CENTERALIGN, TPM_RIGHTALIGN, TPM_TOPALIGN, TPM_VCENTERALIGN, TPM_BOTTOMALIGN,
                              TPM_HORIZONTAL, TPM_VERTICAL, TPM_RIGHTBUTTON, TPM_HORPOSANIMATION, TPM_NOANIMATION, TPM_LAYOUTRTL,
                              TPM_RECURSE, TPM_NOANIMATION | TPM_CENTERALIGN | TPM_VCENTERALIGN };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        tw_begin();
        UINT r = track(w, m, f[i] | TPM_RETURNCMD);
        CHECKF(r == 0, "flags 0x%x -> %u", f[i], r);
    }
    M_DESTROY(m); U(DestroyWindow)(w);
}

TEST(menus_endmenu_outside_loop) {
    REQUIRE(U(EndMenu));
    SetLastError(0);
    BOOL r = U(EndMenu)();
    t_note("EndMenu outside a menu loop -> %d err %lu", r, GetLastError());
}

TEST(menus_track_system_menu) {
    REQ_TRACK();
    REQUIRE(U(GetSystemMenu));
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU s = U(GetSystemMenu)(w, FALSE);
    CHECK(s != NULL);
    if (s) {
        tw_begin();
        UINT r = track(w, s, TPM_RETURNCMD);
        EQ(r, 0);
        int p = idx(WM_INITMENUPOPUP);
        if (p >= 0) {
            CHECK((HMENU)g_ev[p].w == s);
            t_note("sysmenu INITMENUPOPUP HIWORD(lParam)=%u", HIWORD(g_ev[p].l));
        }
        EQ(cnt(WM_SYSCOMMAND), 0);
        U(GetSystemMenu)(w, TRUE);
    }
    U(DestroyWindow)(w);
}

TEST(menus_keymenu_enters_menubar_loop) {
    REQ_TRACK();
    HMENU bar = mh_bar(3);
    HWND w = tw_make(bar);
    if (!w) { CHECK(0); return; }
    tw_begin();
    HANDLE t = tw_arm(w);
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_KEYMENU, 0);
    tw_disarm(w, t);
    int e = idx(WM_ENTERMENULOOP), i = idx(WM_INITMENU), x = idx(WM_EXITMENULOOP);
    t_note("keymenu: enter=%d initmenu=%d exit=%d total=%d", e, i, x, g_nev);
    if (e >= 0) { EQ(g_ev[e].w, FALSE); CHECK(x > e); }
    if (x >= 0) EQ(g_ev[x].w, FALSE);
    if (i >= 0) CHECK((HMENU)g_ev[i].w == bar);
    EQ(cnt(WM_ENTERMENULOOP), cnt(WM_EXITMENULOOP));
    U(DestroyWindow)(w);
}

TEST(menus_keymenu_mnemonic_opens_popup) {
    REQ_TRACK();
    HMENU bar = M_NEW(), file = M_POP();
    mh_add(file, 301, "&New"); mh_add(file, 302, "&Open");
    U(AppendMenuA)(bar, MF_POPUP, (UINT_PTR)file, "&File");
    U(AppendMenuA)(bar, MF_STRING, 303, "&Help");
    HWND w = tw_make(bar);
    if (!w) { CHECK(0); return; }
    tw_begin();
    HANDLE t = tw_arm(w);
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_KEYMENU, 'f');
    tw_disarm(w, t);
    int n = cnt(WM_INITMENUPOPUP);
    t_note("SC_KEYMENU 'f': INITMENUPOPUP=%d total=%d", n, g_nev);
    if (n >= 1) { CHECK((HMENU)nth(WM_INITMENUPOPUP, 0)->w == file); EQ(HIWORD(nth(WM_INITMENUPOPUP, 0)->l), 0); }
    EQ(cnt(WM_ENTERMENULOOP), cnt(WM_EXITMENULOOP));
    U(DestroyWindow)(w);
}

TEST(menus_measureitem_on_menubar) {
    REQ_TRACK();
    HMENU bar = M_NEW();
    U(AppendMenuA)(bar, MF_OWNERDRAW, 51, (LPCSTR)(ULONG_PTR)0xA1);
    U(AppendMenuA)(bar, MF_OWNERDRAW, 52, (LPCSTR)(ULONG_PTR)0xA2);
    tw_begin();
    g_measure_w = 70; g_measure_h = 22;
    HWND w = tw_make(bar);
    if (!w) { CHECK(0); return; }
    U(DrawMenuBar)(w);
    t_pump();
    int n = cnt(WM_MEASUREITEM);
    t_note("menubar: MEASUREITEM=%d DRAWITEM=%d", n, cnt(WM_DRAWITEM));
    for (int i = 0; i < n; i++) {
        ev_t *e = nth(WM_MEASUREITEM, i);
        EQ(e->a, ODT_MENU);
        CHECK((e->b == 51 && e->c == 0xA1) || (e->b == 52 && e->c == 0xA2));
    }
    RECT r0, r1;
    if (U(GetMenuItemRect)(w, bar, 0, &r0) && U(GetMenuItemRect)(w, bar, 1, &r1))
        t_note("owner-draw bar item widths %ld,%ld heights %ld,%ld (measured 70x22)", r0.right - r0.left, r1.right - r1.left, r0.bottom - r0.top, r1.bottom - r1.top);
    g_measure_w = 90; g_measure_h = 20;
    U(DestroyWindow)(w);
}

TEST(menus_syscommand_close_reaches_wm_close) {
    REQ_TRACK();
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    tw_begin();
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_CLOSE, 0);
    EQ(cnt(WM_SYSCOMMAND), 1);
    EQ(cnt(WM_CLOSE), 1);
    CHECK(U(IsWindow)(w));
    U(DestroyWindow)(w);
}

TEST(menus_syscommand_minimize_maximize_restore) {
    REQ_TRACK();
    REQUIRE(U(IsIconic)); REQUIRE(U(IsZoomed));
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    tw_begin();
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    CHECK(U(IsIconic)(w)); CHECK(!U(IsZoomed)(w));
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_RESTORE, 0);
    CHECK(!U(IsIconic)(w)); CHECK(!U(IsZoomed)(w));
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    CHECK(U(IsZoomed)(w)); CHECK(!U(IsIconic)(w));
    U(SendMessageA)(w, WM_SYSCOMMAND, SC_RESTORE, 0);
    CHECK(!U(IsZoomed)(w));
    EQ(cnt(WM_SYSCOMMAND), 4);
    ev_t *e = nth(WM_SYSCOMMAND, 0);
    EQ(e->w & 0xFFF0, SC_MINIMIZE);
    U(DestroyWindow)(w);
}

TEST(menus_syscommand_custom_item_passes_through) {
    REQ_TRACK();
    REQUIRE(U(GetSystemMenu));
    HWND w = tw_make(NULL);
    if (!w) { CHECK(0); return; }
    HMENU s = U(GetSystemMenu)(w, FALSE);
    if (s) U(AppendMenuA)(s, MF_STRING, 0x0100, "Custom");
    tw_begin();
    U(SendMessageA)(w, WM_SYSCOMMAND, 0x0100, 0);
    EQ(cnt(WM_SYSCOMMAND), 1);
    EQ(nth(WM_SYSCOMMAND, 0)->w, 0x0100);
    U(GetSystemMenu)(w, TRUE);
    U(DestroyWindow)(w);
}
