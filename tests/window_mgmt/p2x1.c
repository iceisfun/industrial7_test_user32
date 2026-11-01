/* window_mgmt P2 expansion: SetWindowPos notification/redraw flags, RedrawWindow,
 * ScrollWindow(Ex), LockWindowUpdate, GetWindowModuleFileName, window help ids,
 * CBT hook window lifecycle, DefWindowProc non-client hit-testing and
 * WM_SYSCOMMAND handling, EnableNonClientDpiScaling outside WM_NCCREATE.
 * Hard checks only where the behaviour is documented; the rest are notes. */
#include "wm.h"

UDECL(BOOL, EnableNonClientDpiScaling, (HWND));

/* ------------------------------------------------------------------ helpers */
static HWND px_popup(int x, int y, int w, int h) {
    return wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, x, y, w, h, NULL);
}

static void note_err(const char *what, DWORD want) {
    DWORD e = GetLastError();
    if (e != want) t_note("%s: GetLastError %lu (expected %lu)", what, (unsigned long)e, (unsigned long)want);
}

/* Paint-counting class family, one class per redraw style. */
static int px_paint, px_erase, px_ferase_last;
static LRESULT CALLBACK px_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        U(BeginPaint)(h, &ps);
        px_paint++;
        px_ferase_last = ps.fErase;
        U(EndPaint)(h, &ps);
        return 0;
    }
    if (m == WM_ERASEBKGND) px_erase++;
    return DefWindowProcA(h, m, w, l);
}
static const char *px_cls(UINT cs) {
    static char names[4][32];
    static int done[4];
    int i = ((cs & CS_HREDRAW) ? 1 : 0) | ((cs & CS_VREDRAW) ? 2 : 0);
    if (!done[i]) {
        WNDCLASSEXA wc;
        memset(&wc, 0, sizeof wc);
        wsprintfA(names[i], "WmX1Paint%d", i);
        wc.cbSize = sizeof wc;
        wc.style = cs & (CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = px_proc;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = names[i];
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        U(RegisterClassExA)(&wc);
        done[i] = 1;
    }
    return names[i];
}
static HWND px_mk(UINT cs, DWORD style, int x, int y, int w, int h, HWND parent) {
    DWORD ex = parent ? 0 : (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
    return U(CreateWindowExA)(ex, px_cls(cs), "p", style | WS_VISIBLE, x, y, w, h, parent, NULL,
                              GetModuleHandleA(NULL), NULL);
}
/* Drain pending paints so the update region starts empty. */
static void px_settle(HWND h) {
    t_pump();
    RECT r;
    if (U(GetUpdateRect)(h, &r, FALSE)) {
        U(RedrawWindow)(h, NULL, NULL, RDW_VALIDATE | RDW_NOERASE | RDW_NOFRAME | RDW_NOINTERNALPAINT);
    }
    px_paint = px_erase = px_ferase_last = 0;
}
static int rect_eq(const RECT *a, int l, int t, int r, int b) {
    return a->left == l && a->top == t && a->right == r && a->bottom == b;
}

/* ------------------------------------------------- SetWindowPos flag effects */
TEST(wm_x1_swp_nosendchanging_skips_changing) {
    NEED(SetWindowPos);
    HWND h = px_popup(400, 300, 120, 90);
    wm_logclear();
    CHECK(U(SetWindowPos)(h, NULL, 410, 310, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    EQ(wm_logcount(WM_WINDOWPOSCHANGING), 1);
    wm_logclear();
    CHECK(U(SetWindowPos)(h, NULL, 420, 320, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING));
    EQ(wm_logcount(WM_WINDOWPOSCHANGING), 0);
    EQ(wm_logcount(WM_WINDOWPOSCHANGED), 1);
    EQ(wm_logcount(WM_MOVE), 1);
    RECT r;
    CHECK(U(GetWindowRect)(h, &r));
    EQ(r.left, 420);
    EQ(r.top, 320);
    wm_destroy(h);
}

TEST(wm_x1_swp_nosendchanging_defeats_override) {
    NEED(SetWindowPos);
    HWND h = px_popup(400, 300, 120, 90);
    wm_wpc_mode = 1; /* WM_WINDOWPOSCHANGING rewrites the size to 77x66 */
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 150, 100, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    RECT r;
    CHECK(U(GetWindowRect)(h, &r));
    EQ(r.right - r.left, 77);
    EQ(r.bottom - r.top, 66);
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 150, 100, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING));
    wm_wpc_mode = 0;
    CHECK(U(GetWindowRect)(h, &r));
    EQ(r.right - r.left, 150);
    EQ(r.bottom - r.top, 100);
    wm_destroy(h);
}

TEST(wm_x1_swp_no_user_sizing_messages) {
    NEED(SetWindowPos);
    HWND h = px_popup(400, 300, 120, 90);
    wm_logclear();
    CHECK(U(SetWindowPos)(h, NULL, 410, 310, 160, 110, SWP_NOZORDER | SWP_NOACTIVATE));
    t_pump();
    /* WM_SIZING / WM_MOVING / WM_ENTERSIZEMOVE / WM_EXITSIZEMOVE belong to the interactive move/size loop */
    EQ(wm_logcount(0x0214), 0); /* WM_SIZING */
    EQ(wm_logcount(0x0216), 0); /* WM_MOVING */
    EQ(wm_logcount(0x0231), 0); /* WM_ENTERSIZEMOVE */
    EQ(wm_logcount(0x0232), 0); /* WM_EXITSIZEMOVE */
    EQ(wm_logcount(WM_SIZE), 1);
    EQ(wm_logcount(WM_MOVE), 1);
    wm_destroy(h);
}

TEST(wm_x1_swp_resize_redraw_class_styles) {
    NEED(SetWindowPos);
    NEED(GetUpdateRect);
    NEED(RedrawWindow);
    /* CS_HREDRAW: a width change invalidates the whole client area */
    HWND h = px_mk(CS_HREDRAW, WS_POPUP, 300, 200, 100, 80, NULL);
    px_settle(h);
    RECT r;
    CHECK(!U(GetUpdateRect)(h, &r, FALSE));
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 140, 80, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(U(GetUpdateRect)(h, &r, FALSE));
    CHECKF(rect_eq(&r, 0, 0, 140, 80), "HREDRAW width change update rect %ld,%ld,%ld,%ld",
           r.left, r.top, r.right, r.bottom);
    /* height-only change on an HREDRAW-only class: not documented to invalidate everything */
    px_settle(h);
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 140, 120, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    if (U(GetUpdateRect)(h, &r, FALSE) && rect_eq(&r, 0, 0, 140, 120))
        t_note("HREDRAW-only class repaints everything on a height-only change");
    wm_destroy(h);

    /* CS_VREDRAW: a height change invalidates the whole client area */
    h = px_mk(CS_VREDRAW, WS_POPUP, 300, 200, 100, 80, NULL);
    px_settle(h);
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 100, 130, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(U(GetUpdateRect)(h, &r, FALSE));
    CHECKF(rect_eq(&r, 0, 0, 100, 130), "VREDRAW height change update rect %ld,%ld,%ld,%ld",
           r.left, r.top, r.right, r.bottom);
    wm_destroy(h);

    /* neither style: growing does not have to repaint the retained area */
    h = px_mk(0, WS_POPUP, 300, 200, 100, 80, NULL);
    px_settle(h);
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 160, 80, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    if (U(GetUpdateRect)(h, &r, FALSE))
        t_note("plain class grow update rect %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    else
        t_note("plain class grow produced no update region");
    wm_destroy(h);
}

TEST(wm_x1_swp_noredraw_note) {
    NEED(SetWindowPos);
    HWND h = px_mk(CS_HREDRAW | CS_VREDRAW, WS_POPUP, 300, 200, 100, 80, NULL);
    px_settle(h);
    RECT r;
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 150, 120, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW));
    RECT wr;
    CHECK(U(GetWindowRect)(h, &wr));
    EQ(wr.right - wr.left, 150);
    EQ(wr.bottom - wr.top, 120);
    t_note("SWP_NOREDRAW on H+V redraw class: update region %s", U(GetUpdateRect)(h, &r, FALSE) ? "present" : "empty");
    wm_destroy(h);
}

TEST(wm_x1_swp_noownerzorder_note) {
    NEED(SetWindowPos);
    HWND owner = px_popup(300, 200, 150, 100);
    HWND owned = U(CreateWindowExA)(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wm_cls(), "o", WS_POPUP | WS_VISIBLE,
                                    320, 220, 80, 60, owner, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(owned != NULL);
    if (owned) {
        CHECK(wm_isabove(owned, owner));
        CHECK(U(SetWindowPos)(owner, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER));
        t_note("SWP_NOOWNERZORDER: owned above owner after owner raise: %d", wm_isabove(owned, owner));
        CHECK(U(SetWindowPos)(owner, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));
        CHECK(wm_isabove(owned, owner));
    }
    wm_destroy(owned);
    wm_destroy(owner);
}

/* ------------------------------------------------------------ RedrawWindow */
TEST(wm_x1_redraw_invalidate_validate) {
    NEED(RedrawWindow);
    NEED(GetUpdateRect);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 120, 90, NULL);
    px_settle(h);
    RECT r;
    CHECK(!U(GetUpdateRect)(h, &r, FALSE));
    CHECK(U(RedrawWindow)(h, NULL, NULL, RDW_INVALIDATE));
    CHECK(U(GetUpdateRect)(h, &r, FALSE));
    CHECKF(rect_eq(&r, 0, 0, 120, 90), "full client expected, got %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    EQ(px_paint, 0); /* nothing is delivered without a pump */
    CHECK(U(RedrawWindow)(h, NULL, NULL, RDW_VALIDATE));
    CHECK(!U(GetUpdateRect)(h, &r, FALSE));
    wm_destroy(h);
}

TEST(wm_x1_redraw_rect_and_region_precedence) {
    NEED(RedrawWindow);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 120, 90, NULL);
    px_settle(h);
    RECT r, prc = { 10, 12, 40, 44 };
    CHECK(U(RedrawWindow)(h, &prc, NULL, RDW_INVALIDATE));
    CHECK(U(GetUpdateRect)(h, &r, FALSE));
    CHECKF(rect_eq(&r, 10, 12, 40, 44), "rect update got %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    px_settle(h);
    /* the region wins over the rectangle when both are given */
    HRGN rg = CreateRectRgn(0, 0, 5, 6);
    CHECK(rg != NULL);
    CHECK(U(RedrawWindow)(h, &prc, rg, RDW_INVALIDATE));
    CHECK(U(GetUpdateRect)(h, &r, FALSE));
    CHECKF(rect_eq(&r, 0, 0, 5, 6), "region precedence got %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    DeleteObject(rg);
    wm_destroy(h);
}

TEST(wm_x1_redraw_updatenow_synchronous) {
    NEED(RedrawWindow);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 120, 90, NULL);
    px_settle(h);
    CHECK(U(RedrawWindow)(h, NULL, NULL, RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW));
    EQ(px_paint, 1);
    EQ(px_erase, 0);
    CHECK(px_ferase_last == 0);
    RECT r;
    CHECK(!U(GetUpdateRect)(h, &r, FALSE));
    px_paint = px_erase = px_ferase_last = 0;
    CHECK(U(RedrawWindow)(h, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW));
    EQ(px_paint, 1);
    EQ(px_erase, 1);
    /* the class has a background brush: DefWindowProc erases inside BeginPaint, so fErase is already FALSE */
    CHECK(px_ferase_last == 0);
    wm_destroy(h);
}

TEST(wm_x1_redraw_erasenow_without_paint) {
    NEED(RedrawWindow);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 120, 90, NULL);
    px_settle(h);
    CHECK(U(RedrawWindow)(h, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ERASENOW));
    EQ(px_erase, 1);
    EQ(px_paint, 0);
    RECT r;
    CHECK(U(GetUpdateRect)(h, &r, FALSE)); /* still invalid until painted */
    t_pump();
    EQ(px_paint, 1);
    wm_destroy(h);
}

TEST(wm_x1_redraw_children_flags) {
    NEED(RedrawWindow);
    HWND p = px_mk(0, WS_POPUP, 300, 200, 200, 140, NULL);
    HWND c = px_mk(0, WS_CHILD, 20, 20, 60, 40, p);
    CHECK(p && c);
    if (p && c) {
        RECT r;
        px_settle(p);
        px_settle(c);
        CHECK(U(RedrawWindow)(p, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN));
        CHECK(U(GetUpdateRect)(c, &r, FALSE));
        px_settle(p);
        px_settle(c);
        CHECK(U(RedrawWindow)(p, NULL, NULL, RDW_INVALIDATE | RDW_NOCHILDREN));
        CHECK(U(GetUpdateRect)(p, &r, FALSE));
        t_note("RDW_NOCHILDREN: child update region %s", U(GetUpdateRect)(c, &r, FALSE) ? "present" : "empty");
    }
    wm_destroy(c);
    wm_destroy(p);
}

TEST(wm_x1_redraw_internalpaint_note) {
    NEED(RedrawWindow);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 120, 90, NULL);
    px_settle(h);
    CHECK(U(RedrawWindow)(h, NULL, NULL, RDW_INTERNALPAINT));
    t_pump();
    t_note("RDW_INTERNALPAINT on a clean window delivered %d WM_PAINT", px_paint);
    wm_destroy(h);
}

TEST(wm_x1_redraw_dead_window) {
    NEED(RedrawWindow);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 60, 40, NULL);
    wm_destroy(h);
    SetLastError(0);
    CHECK(!U(RedrawWindow)(h, NULL, NULL, RDW_INVALIDATE));
    note_err("RedrawWindow(dead)", ERR_INVALID_HWND);
}

/* ----------------------------------------------------------- ScrollWindowEx */
TEST(wm_x1_scrollex_invalidate_region) {
    NEED(ScrollWindowEx);
    NEED(GetUpdateRect);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 150, 100, NULL);
    px_settle(h);
    HRGN rg = CreateRectRgn(0, 0, 0, 0);
    RECT up, r;
    memset(&up, 0xCC, sizeof up);
    int rc = U(ScrollWindowEx)(h, 20, 0, NULL, NULL, rg, &up, SW_INVALIDATE);
    CHECKF(rc == SIMPLEREGION || rc == COMPLEXREGION, "ScrollWindowEx returned %d", rc);
    CHECKF(up.left <= 0 && up.right >= 20 && up.top <= 0 && up.bottom >= 100,
           "update rect must cover the exposed strip, got %ld,%ld,%ld,%ld", up.left, up.top, up.right, up.bottom);
    if (!rect_eq(&up, 0, 0, 20, 100))
        t_note("exposed strip rect is %ld,%ld,%ld,%ld", up.left, up.top, up.right, up.bottom);
    CHECK(U(GetUpdateRect)(h, &r, FALSE));
    CHECK(r.left <= 0 && r.right >= 20 && r.bottom >= 100);
    DeleteObject(rg);
    wm_destroy(h);
}

TEST(wm_x1_scrollex_no_invalidate_flag) {
    NEED(ScrollWindowEx);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 150, 100, NULL);
    px_settle(h);
    RECT up, r;
    memset(&up, 0, sizeof up);
    int rc = U(ScrollWindowEx)(h, 0, 15, NULL, NULL, NULL, &up, 0);
    CHECKF(rc != 0, "ScrollWindowEx failed");
    CHECKF(up.bottom - up.top >= 15 && up.right - up.left >= 150, "exposed rect %ld,%ld,%ld,%ld", up.left, up.top, up.right, up.bottom);
    CHECK(!U(GetUpdateRect)(h, &r, FALSE)); /* no SW_INVALIDATE: nothing invalidated */
    wm_destroy(h);
}

TEST(wm_x1_scrollex_children) {
    NEED(ScrollWindowEx);
    HWND p = px_mk(0, WS_POPUP, 300, 200, 200, 140, NULL);
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE, 0, 30, 30, 40, 20, p);
    CHECK(p && c);
    if (p && c) {
        POINT pt;
        RECT r;
        px_settle(p);
        U(ScrollWindowEx)(p, 15, 5, NULL, NULL, NULL, NULL, 0);
        U(GetWindowRect)(c, &r);
        pt.x = r.left; pt.y = r.top;
        U(ScreenToClient)(p, &pt);
        EQ(pt.x, 30);
        EQ(pt.y, 30);
        wm_logclear();
        U(ScrollWindowEx)(p, 15, 5, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN);
        U(GetWindowRect)(c, &r);
        pt.x = r.left; pt.y = r.top;
        U(ScreenToClient)(p, &pt);
        EQ(pt.x, 45);
        EQ(pt.y, 35);
        CHECK(wm_logcount_h(WM_MOVE, c) >= 1);
    }
    wm_destroy(c);
    wm_destroy(p);
}

TEST(wm_x1_scroll_legacy_and_dead) {
    NEED(ScrollWindow);
    NEED(ScrollWindowEx);
    HWND h = px_mk(0, WS_POPUP, 300, 200, 100, 80, NULL);
    px_settle(h);
    CHECK(U(ScrollWindow)(h, 5, 5, NULL, NULL));
    wm_destroy(h);
    SetLastError(0);
    CHECK(!U(ScrollWindow)(h, 5, 5, NULL, NULL));
    note_err("ScrollWindow(dead)", ERR_INVALID_HWND);
    SetLastError(0);
    EQ(U(ScrollWindowEx)(h, 5, 5, NULL, NULL, NULL, NULL, SW_INVALIDATE), 0);
    note_err("ScrollWindowEx(dead)", ERR_INVALID_HWND);
}

/* --------------------------------------------------------- LockWindowUpdate */
TEST(wm_x1_lockwindowupdate_single_lock) {
    NEED(LockWindowUpdate);
    HWND a = px_popup(300, 200, 80, 60);
    HWND b = px_popup(400, 200, 80, 60);
    CHECK(U(LockWindowUpdate)(a));
    SetLastError(0);
    CHECK(!U(LockWindowUpdate)(b)); /* another window is already locked */
    CHECK(U(LockWindowUpdate)(NULL));
    CHECK(U(LockWindowUpdate)(b));
    CHECK(U(LockWindowUpdate)(NULL));
    wm_destroy(a);
    wm_destroy(b);
}

TEST(wm_x1_lockwindowupdate_dead_window) {
    NEED(LockWindowUpdate);
    HWND a = px_popup(300, 200, 80, 60);
    wm_destroy(a);
    SetLastError(0);
    CHECK(!U(LockWindowUpdate)(a));
    note_err("LockWindowUpdate(dead)", ERR_INVALID_HWND);
    U(LockWindowUpdate)(NULL);
}

/* ------------------------------------------------- GetWindowModuleFileName */
TEST(wm_x1_modfname_own_window) {
    NEED(GetWindowModuleFileNameA);
    NEED(GetWindowModuleFileNameW);
    HWND h = wm_hidden();
    char a[MAX_PATH * 2], b[MAX_PATH * 2];
    WCHAR w[MAX_PATH * 2];
    UINT n = U(GetWindowModuleFileNameA)(h, a, sizeof a);
    DWORD m = GetModuleFileNameA(NULL, b, sizeof b);
    CHECKF(n > 0, "GetWindowModuleFileNameA returned 0");
    EQ(n, m);
    CHECK(!lstrcmpiA(a, b));
    UINT nw = U(GetWindowModuleFileNameW)(h, w, MAX_PATH * 2);
    EQ(nw, n);
    if (nw) {
        char conv[MAX_PATH * 2];
        WideCharToMultiByte(CP_ACP, 0, w, -1, conv, sizeof conv, NULL, NULL);
        CHECK(!lstrcmpiA(conv, a));
    }
    wm_destroy(h);
}

TEST(wm_x1_modfname_buffer_protocol_note) {
    NEED(GetWindowModuleFileNameA);
    HWND h = wm_hidden();
    char a[MAX_PATH * 2], small[8];
    UINT full = U(GetWindowModuleFileNameA)(h, a, sizeof a);
    memset(small, 'Z', sizeof small);
    UINT n = U(GetWindowModuleFileNameA)(h, small, sizeof small);
    t_note("GetWindowModuleFileNameA full=%u small(8)=%u last=%02x term=%d", full, n,
           (unsigned char)small[7], memchr(small, 0, sizeof small) != NULL);
    CHECK(n <= sizeof small);
    char one[1] = { 'Z' };
    n = U(GetWindowModuleFileNameA)(h, one, 0);
    t_note("GetWindowModuleFileNameA(cch=0)=%u", n);
    wm_destroy(h);
}

TEST(wm_x1_modfname_invalid_windows) {
    NEED(GetWindowModuleFileNameA);
    char a[MAX_PATH];
    HWND h = wm_hidden();
    wm_destroy(h);
    SetLastError(0);
    EQ(U(GetWindowModuleFileNameA)(h, a, sizeof a), 0);
    t_note("GetWindowModuleFileName(dead) error %lu", (unsigned long)GetLastError());
    SetLastError(0);
    EQ(U(GetWindowModuleFileNameA)(NULL, a, sizeof a), 0);
    t_note("GetWindowModuleFileName(NULL) error %lu", (unsigned long)GetLastError());
    HWND d = U(GetDesktopWindow)();
    UINT n = U(GetWindowModuleFileNameA)(d, a, sizeof a);
    t_note("GetWindowModuleFileName(desktop window, other process) = %u", n);
}

TEST(wm_x1_modfname_instance_override_note) {
    NEED(GetWindowModuleFileNameA);
    HMODULE k = GetModuleHandleA("kernel32.dll");
    /* a class registered for the exe is not found with another hInstance unless it is CS_GLOBALCLASS */
    WNDCLASSA gc; memset(&gc, 0, sizeof gc);
    gc.style = CS_GLOBALCLASS; gc.lpfnWndProc = DefWindowProcA; gc.hInstance = GetModuleHandleA(NULL); gc.lpszClassName = "WmX1GlobalMod";
    U(RegisterClassA)(&gc);
    HWND h = U(CreateWindowExA)(0, "WmX1GlobalMod", "m", WS_OVERLAPPEDWINDOW, 50, 50, 100, 80, NULL, NULL, k, NULL);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        char a[MAX_PATH], b[MAX_PATH];
        UINT n = U(GetWindowModuleFileNameA)(h, a, sizeof a);
        DWORD m = GetModuleFileNameA(k, b, sizeof b);
        t_note("window created with kernel32 hInstance: GWMFN=%u (%s) kernel32=%lu (%s)", n, n ? a : "", (unsigned long)m, b);
    }
    wm_destroy(h);
}

/* ------------------------------------------------------- window help ids */
TEST(wm_x1_help_id_roundtrip) {
    NEED(SetWindowContextHelpId);
    NEED(GetWindowContextHelpId);
    HWND h = wm_hidden();
    EQ(U(GetWindowContextHelpId)(h), 0);
    CHECK(U(SetWindowContextHelpId)(h, 0x1234));
    EQ(U(GetWindowContextHelpId)(h), 0x1234);
    CHECK(U(SetWindowContextHelpId)(h, 0xFFFFFFFFu));
    EQ(U(GetWindowContextHelpId)(h), 0xFFFFFFFFu);
    CHECK(U(SetWindowContextHelpId)(h, 0));
    EQ(U(GetWindowContextHelpId)(h), 0);
    /* per window */
    HWND g = wm_hidden();
    CHECK(U(SetWindowContextHelpId)(h, 77));
    EQ(U(GetWindowContextHelpId)(g), 0);
    wm_destroy(g);
    wm_destroy(h);
}

TEST(wm_x1_help_id_child_and_dead) {
    NEED(SetWindowContextHelpId);
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 20, 20, p);
    CHECK(c != NULL);
    if (c) {
        CHECK(U(SetWindowContextHelpId)(c, 9));
        EQ(U(GetWindowContextHelpId)(c), 9);
        EQ(U(GetWindowContextHelpId)(p), 0);
    }
    wm_destroy(p);
    SetLastError(0);
    CHECK(!U(SetWindowContextHelpId)(p, 5));
    note_err("SetWindowContextHelpId(dead)", ERR_INVALID_HWND);
    EQ(U(GetWindowContextHelpId)(p), 0);
    SetLastError(0);
    CHECK(!U(SetWindowContextHelpId)(NULL, 5));
    EQ(U(GetWindowContextHelpId)(NULL), 0);
}

/* ------------------------------------------------------------ CBT hook */
static HHOOK cbt_hook;
static int cbt_veto_create, cbt_veto_destroy, cbt_veto_minmax;
static int cbt_create_n, cbt_destroy_n, cbt_minmax_n, cbt_seen_nc_at_create;
static HWND cbt_create_hwnd, cbt_track, cbt_minmax_hwnd;
static int cbt_x, cbt_y, cbt_cx, cbt_cy;
static LPARAM cbt_minmax_lp;
static LRESULT CALLBACK cbt_proc(int code, WPARAM w, LPARAM l) {
    if (code == HCBT_CREATEWND) {
        CBT_CREATEWNDA *c = (CBT_CREATEWNDA *)l;
        if (c && c->lpcs && c->lpcs->lpszClass && !IS_INTRESOURCE(c->lpcs->lpszClass) &&
            !lstrcmpA(c->lpcs->lpszClass, wm_cls())) {
            cbt_create_n++;
            cbt_create_hwnd = (HWND)w;
            cbt_seen_nc_at_create = wm_seen_nc;
            cbt_x = c->lpcs->x; cbt_y = c->lpcs->y; cbt_cx = c->lpcs->cx; cbt_cy = c->lpcs->cy;
            if (cbt_veto_create) return 1;
        }
    } else if (code == HCBT_DESTROYWND) {
        if ((HWND)w == cbt_track) {
            cbt_destroy_n++;
            if (cbt_veto_destroy) return 1;
        }
    } else if (code == HCBT_MINMAX) {
        if ((HWND)w == cbt_track) {
            cbt_minmax_n++;
            cbt_minmax_hwnd = (HWND)w;
            cbt_minmax_lp = l;
            if (cbt_veto_minmax) return 1;
        }
    }
    return CallNextHookEx(cbt_hook, code, w, l);
}
static int cbt_install(void) {
    cbt_create_n = cbt_destroy_n = cbt_minmax_n = 0;
    cbt_veto_create = cbt_veto_destroy = cbt_veto_minmax = 0;
    cbt_create_hwnd = cbt_track = cbt_minmax_hwnd = NULL;
    cbt_hook = U(SetWindowsHookExA)(WH_CBT, cbt_proc, NULL, GetCurrentThreadId());
    return cbt_hook != NULL;
}
static void cbt_remove(void) {
    if (cbt_hook) U(UnhookWindowsHookEx)(cbt_hook);
    cbt_hook = NULL;
    cbt_track = NULL;
}

TEST(wm_x1_cbt_create_precedes_nccreate) {
    NEED(SetWindowsHookExA);
    if (!cbt_install()) { t_skip("cannot install WH_CBT"); return; }
    int nc0 = wm_seen_nc;
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 311, 222, 123, 77, NULL);
    cbt_remove();
    CHECK(h != NULL);
    EQ(cbt_create_n, 1);
    EQ(cbt_create_hwnd, h);
    EQ(cbt_seen_nc_at_create, nc0);          /* hook ran before WM_NCCREATE */
    EQ(wm_seen_nc, nc0 + 1);
    EQ(cbt_x, 311); EQ(cbt_y, 222); EQ(cbt_cx, 123); EQ(cbt_cy, 77);
    wm_destroy(h);
}

TEST(wm_x1_cbt_create_veto) {
    NEED(SetWindowsHookExA);
    if (!cbt_install()) { t_skip("cannot install WH_CBT"); return; }
    cbt_veto_create = 1;
    int nc0 = wm_seen_nc;
    SetLastError(0);
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 311, 222, 123, 77, NULL);
    cbt_remove();
    EQ(cbt_create_n, 1);
    CHECK(h == NULL);
    t_note("CBT create veto: WM_NCCREATE delivered %d time(s), error %lu", wm_seen_nc - nc0, (unsigned long)GetLastError());
    wm_destroy(h);
}

TEST(wm_x1_cbt_destroy_notification_and_veto) {
    NEED(SetWindowsHookExA);
    HWND h = wm_mk(WS_POPUP, WS_EX_TOOLWINDOW, 311, 222, 60, 40, NULL);
    CHECK(h != NULL);
    if (!h) return;
    if (!cbt_install()) { wm_destroy(h); t_skip("cannot install WH_CBT"); return; }
    cbt_track = h;
    cbt_veto_destroy = 1;
    SetLastError(0);
    BOOL ok = U(DestroyWindow)(h);
    EQ(cbt_destroy_n, 1);
    CHECK(!ok);
    CHECK(U(IsWindow)(h));
    cbt_veto_destroy = 0;
    CHECK(U(DestroyWindow)(h));
    EQ(cbt_destroy_n, 2);
    CHECK(!U(IsWindow)(h));
    cbt_remove();
}

TEST(wm_x1_cbt_minmax_veto) {
    NEED(SetWindowsHookExA);
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 200, 150, 200, 150, NULL);
    CHECK(h != NULL);
    if (!h) return;
    if (!cbt_install()) { wm_destroy(h); t_skip("cannot install WH_CBT"); return; }
    cbt_track = h;
    cbt_veto_minmax = 1;
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    int vetoed_seen = cbt_minmax_n;
    BOOL iconic_veto = U(IsIconic)(h);
    if (!vetoed_seen) {
        t_note("HCBT_MINMAX not delivered for ShowWindow(SW_SHOWMINNOACTIVE)");
    } else {
        EQ(cbt_minmax_hwnd, h);
        t_note("HCBT_MINMAX lParam 0x%llx", (unsigned long long)cbt_minmax_lp);
        CHECK(!iconic_veto);
        cbt_veto_minmax = 0;
        U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
        CHECK(U(IsIconic)(h));
    }
    cbt_remove();
    wm_destroy(h);
}

/* ------------------------------------------- DefWindowProc WM_NCHITTEST */
static LRESULT hit(HWND h, int x, int y) {
    return U(SendMessageA)(h, WM_NCHITTEST, 0, MAKELPARAM((short)x, (short)y));
}

TEST(wm_x1_hittest_overlapped_frame) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 200, 400, 300, NULL);
    CHECK(h != NULL);
    if (!h) return;
    RECT r, c;
    POINT o = { 0, 0 };
    U(GetWindowRect)(h, &r);
    U(GetClientRect)(h, &c);
    U(ClientToScreen)(h, &o);
    EQ(hit(h, o.x + c.right / 2, o.y + c.bottom / 2), HTCLIENT);
    int midy = (r.top + r.bottom) / 2, midx = (r.left + r.right) / 2;
    EQ(hit(h, r.left, midy), HTLEFT);
    EQ(hit(h, r.right - 1, midy), HTRIGHT);
    EQ(hit(h, midx, r.bottom - 1), HTBOTTOM);
    EQ(hit(h, r.right - 1, r.bottom - 1), HTBOTTOMRIGHT);
    EQ(hit(h, r.left, r.bottom - 1), HTBOTTOMLEFT);
    TITLEBARINFO ti;
    memset(&ti, 0, sizeof ti);
    ti.cbSize = sizeof ti;
    if (U(GetTitleBarInfo)(h, &ti)) {
        int cy = (ti.rcTitleBar.top + ti.rcTitleBar.bottom) / 2;
        EQ(hit(h, ti.rcTitleBar.left + 60, cy), HTCAPTION);
    }
    LRESULT out = hit(h, r.left - 60, midy);
    t_note("WM_NCHITTEST outside the window rect -> %lld", (long long)out);
    wm_destroy(h);
}

TEST(wm_x1_hittest_scrollbars) {
    HWND h = wm_mk(WS_POPUP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 200, 300, 220, NULL);
    CHECK(h != NULL);
    if (!h) return;
    RECT c;
    POINT lt = { 0, 0 };
    U(GetClientRect)(h, &c);
    U(ClientToScreen)(h, &lt);
    EQ(hit(h, lt.x + c.right / 2, lt.y + c.bottom / 2), HTCLIENT);
    EQ(hit(h, lt.x + c.right + 3, lt.y + c.bottom / 2), HTVSCROLL);
    EQ(hit(h, lt.x + c.right / 2, lt.y + c.bottom + 3), HTHSCROLL);
    wm_destroy(h);
}

TEST(wm_x1_hittest_menu_bar) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 200, 400, 300, NULL);
    CHECK(h != NULL);
    if (!h) return;
    HMENU m = U(CreateMenu)();
    U(AppendMenuA)(m, MF_STRING, 1001, "File");
    CHECK(U(SetMenu)(h, m));
    POINT o = { 0, 0 };
    U(ClientToScreen)(h, &o);
    RECT r;
    U(GetWindowRect)(h, &r);
    EQ(hit(h, r.left + 100, o.y - 3), HTMENU);
    wm_destroy(h); /* destroys the attached menu */
}

TEST(wm_x1_hittest_child_border) {
    HWND p = px_popup(300, 200, 200, 150);
    HWND c = wm_mk(WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 20, 20, 80, 60, p);
    CHECK(p && c);
    if (p && c) {
        RECT cr, cc;
        POINT o = { 0, 0 };
        U(GetWindowRect)(c, &cr);
        U(GetClientRect)(c, &cc);
        U(ClientToScreen)(c, &o);
        EQ(hit(c, o.x + cc.right / 2, o.y + cc.bottom / 2), HTCLIENT);
        t_note("child WS_BORDER edge pixel -> %lld", (long long)hit(c, cr.left, cr.top + 30));
    }
    wm_destroy(c);
    wm_destroy(p);
}

/* -------------------------------------------- DefWindowProc WM_SYSCOMMAND */
TEST(wm_x1_syscommand_min_max_restore) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 200, 150, 300, 200, NULL);
    CHECK(h != NULL);
    if (!h) return;
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    CHECK(U(IsIconic)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    CHECK(!U(IsIconic)(h));
    CHECK(!U(IsZoomed)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    CHECK(U(IsZoomed)(h));
    CHECK(!U(IsIconic)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    CHECK(!U(IsZoomed)(h));
    CHECK(U(IsWindowVisible)(h));
    /* a restore of a normal window changes nothing */
    RECT a, b;
    U(GetWindowRect)(h, &a);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    U(GetWindowRect)(h, &b);
    CHECK(rect_eq(&b, a.left, a.top, a.right, a.bottom));
    wm_destroy(h);
}

TEST(wm_x1_syscommand_close_destroys) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 200, 150, 300, 200, NULL);
    CHECK(h != NULL);
    if (!h) return;
    wm_logclear();
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_CLOSE, 0);
    t_pump();
    CHECK(wm_logcount_h(WM_CLOSE, h) >= 1);
    CHECK(!U(IsWindow)(h));
    wm_destroy(h);
}

TEST(wm_x1_syscommand_low_nibble_ignored) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 200, 150, 300, 200, NULL);
    CHECK(h != NULL);
    if (!h) return;
    /* the low four bits of wParam are used internally by the system */
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MINIMIZE | 0x2, 0);
    CHECK(U(IsIconic)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE | 0x1, 0);
    CHECK(!U(IsIconic)(h));
    wm_destroy(h);
}

TEST(wm_x1_syscommand_box_gating_note) {
    HWND h = wm_mk(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, WS_EX_NOACTIVATE, 200, 150, 300, 200, NULL);
    CHECK(h != NULL);
    if (!h) return;
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    t_note("SC_MAXIMIZE without WS_MAXIMIZEBOX: zoomed=%d", U(IsZoomed)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    t_note("SC_MINIMIZE without WS_MINIMIZEBOX: iconic=%d", U(IsIconic)(h));
    wm_destroy(h);
}

/* ------------------------------------------------------------------ DPI */
static BOOL nc_in_nccreate_result = -1;
static LRESULT CALLBACK nc_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) {
        EnableNonClientDpiScaling_pfn f = UN(EnableNonClientDpiScaling);
        if (f) {
            SetLastError(0);
            nc_in_nccreate_result = f(h);
        }
    }
    return DefWindowProcA(h, m, w, l);
}
TEST(wm_x1_dpi_enable_nc_scaling_context_note) {
    REQUIRE(UN(EnableNonClientDpiScaling));
    HWND h = wm_hidden();
    SetLastError(0);
    BOOL r = UN(EnableNonClientDpiScaling)(h);
    t_note("EnableNonClientDpiScaling outside WM_NCCREATE (unaware thread) -> %d, error %lu", r, (unsigned long)GetLastError());
    wm_destroy(h);
    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = nc_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "WmX1NcDpi";
    if (U(RegisterClassExA)(&wc)) {
        HWND w = U(CreateWindowExA)(0, "WmX1NcDpi", "n", WS_OVERLAPPEDWINDOW, 50, 50, 100, 80, NULL, NULL, wc.hInstance, NULL);
        t_note("EnableNonClientDpiScaling inside WM_NCCREATE (unaware thread) -> %d, error %lu", nc_in_nccreate_result, (unsigned long)GetLastError());
        wm_destroy(w);
        U(UnregisterClassA)("WmX1NcDpi", wc.hInstance);
    }
}
