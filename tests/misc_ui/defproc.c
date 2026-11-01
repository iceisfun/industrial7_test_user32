/* misc_ui: DefWindowProc behaviours: hit testing, NC calc, text, syscommand, misc default handling */
#include "mu.h"

#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT 1
#define HOTKEYF_CONTROL 2
#define HOTKEYF_ALT 4
#endif
#define NEED_DP() REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow)); REQUIRE(U(IsWindow)); REQUIRE(U(SendMessageA)); \
    REQUIRE(U(DefWindowProcA)); if (!mu_register()) { t_skip("class"); return; }

static LRESULT ht(HWND h, int x, int y) { return U(SendMessageA)(h, WM_NCHITTEST, 0, MAKELPARAM((short)x, (short)y)); }

/* ---------- WM_NCHITTEST ---------- */
typedef struct { RECT wr, cr; int fx, fy, cap, mid_x, mid_y; } hitgeo;
static void geo(HWND h, hitgeo *g)
{
    POINT o = {0, 0};
    U(GetWindowRect)(h, &g->wr);
    U(GetClientRect)(h, &g->cr);
    U(ClientToScreen)(h, &o);
    OffsetRect(&g->cr, o.x, o.y);
    g->fx = U(GetSystemMetrics)(SM_CXSIZEFRAME) + U(GetSystemMetrics)(SM_CXPADDEDBORDER);
    g->fy = U(GetSystemMetrics)(SM_CYSIZEFRAME) + U(GetSystemMetrics)(SM_CXPADDEDBORDER);
    g->cap = U(GetSystemMetrics)(SM_CYCAPTION);
    g->mid_x = (g->wr.left + g->wr.right) / 2;
    g->mid_y = (g->wr.top + g->wr.bottom) / 2;
}
#define NEED_GEO() REQUIRE(U(GetWindowRect)); REQUIRE(U(GetClientRect)); REQUIRE(U(ClientToScreen)); REQUIRE(U(GetSystemMetrics))

TEST(misc_ui_nchittest_client_and_caption) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    CHECK(ht(h, (g.cr.left + g.cr.right) / 2, (g.cr.top + g.cr.bottom) / 2) == HTCLIENT);
    CHECK(ht(h, g.cr.left, g.cr.top) == HTCLIENT);
    CHECK(ht(h, g.cr.right - 1, g.cr.bottom - 1) == HTCLIENT);
    CHECK(ht(h, g.cr.left, g.cr.bottom - 1) == HTCLIENT);
    CHECK(ht(h, g.cr.right - 1, g.cr.top) == HTCLIENT);
    CHECK(ht(h, g.cr.left - 1, g.mid_y) != HTCLIENT);
    CHECK(ht(h, g.cr.right, g.mid_y) != HTCLIENT);
    CHECK(ht(h, g.mid_x, g.cr.bottom) != HTCLIENT);
    CHECK(ht(h, g.mid_x, g.cr.top - 1) != HTCLIENT);
    int capy = g.wr.top + g.fy + g.cap / 2;
    CHECK(ht(h, g.mid_x, capy) == HTCAPTION);
    CHECK(ht(h, g.mid_x - 20, capy) == HTCAPTION);
    mu_kill(h);
}
TEST(misc_ui_nchittest_outside_is_nowhere) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    CHECK(ht(h, g.wr.left - 1, g.mid_y) == HTNOWHERE);
    CHECK(ht(h, g.wr.right, g.mid_y) == HTNOWHERE);
    CHECK(ht(h, g.mid_x, g.wr.top - 1) == HTNOWHERE);
    CHECK(ht(h, g.mid_x, g.wr.bottom) == HTNOWHERE);
    CHECK(ht(h, g.wr.left - 100, g.wr.top - 100) == HTNOWHERE);
    CHECK(ht(h, g.wr.right + 1000, g.wr.bottom + 1000) == HTNOWHERE);
    CHECK(ht(h, -30000, -30000) == HTNOWHERE);
    CHECK(ht(h, 32767, 32767) == HTNOWHERE);
    mu_kill(h);
}
TEST(misc_ui_nchittest_sizing_borders) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    CHECK(ht(h, g.wr.left, g.mid_y) == HTLEFT);
    CHECK(ht(h, g.wr.left + 1, g.mid_y) == HTLEFT);
    CHECK(ht(h, g.wr.right - 1, g.mid_y) == HTRIGHT);
    CHECK(ht(h, g.mid_x, g.wr.bottom - 1) == HTBOTTOM);
    CHECK(ht(h, g.mid_x, g.wr.top) == HTTOP);
    CHECK(ht(h, g.wr.left, g.wr.top) == HTTOPLEFT);
    CHECK(ht(h, g.wr.right - 1, g.wr.top) == HTTOPRIGHT);
    CHECK(ht(h, g.wr.left, g.wr.bottom - 1) == HTBOTTOMLEFT);
    CHECK(ht(h, g.wr.right - 1, g.wr.bottom - 1) == HTBOTTOMRIGHT);
    mu_kill(h);
}
TEST(misc_ui_nchittest_no_thickframe_has_no_sizing) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    LRESULT r;
    r = ht(h, g.wr.left, g.mid_y);
    CHECKF(r != HTLEFT && r != HTRIGHT && r != HTTOPLEFT, "left edge -> %lld", (long long)r);
    r = ht(h, g.wr.right - 1, g.wr.bottom - 1);
    CHECKF(r != HTBOTTOMRIGHT && r != HTBOTTOM && r != HTRIGHT, "br corner -> %lld", (long long)r);
    CHECK(ht(h, g.mid_x, g.wr.top + 3 + g.cap / 2) == HTCAPTION);
    CHECK(ht(h, g.mid_x, g.mid_y) == HTCLIENT);
    mu_kill(h);
}
TEST(misc_ui_nchittest_close_button) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    int cx = U(GetSystemMetrics)(SM_CXSIZE);
    int y = g.wr.top + g.fy + g.cap / 2;
    LRESULT rc = ht(h, g.wr.right - g.fx - cx / 2, y);
    LRESULT rm = ht(h, g.wr.right - g.fx - cx - cx / 2, y);
    LRESULT rn = ht(h, g.wr.right - g.fx - 2 * cx - cx / 2, y);
    CHECKF(rc != HTCLIENT && rc != HTNOWHERE, "close area -> %lld", (long long)rc);
    t_note("close button area -> %lld (HTCLOSE=%d)", (long long)rc, HTCLOSE);
    t_note("max button -> %lld (HTMAXBUTTON=%d), min button -> %lld (HTMINBUTTON=%d)", (long long)rm, HTMAXBUTTON, (long long)rn, HTMINBUTTON);
    mu_kill(h);
}
TEST(misc_ui_nchittest_sysmenu) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    int y = g.wr.top + g.fy + g.cap / 2;
    LRESULT r = ht(h, g.wr.left + g.fx + 4, y);
    t_note("sysmenu area -> %lld (HTSYSMENU=%d)", (long long)r, HTSYSMENU);
    U(SetWindowLongPtrA)(h, GWL_STYLE, mu_style(h) & ~WS_SYSMENU);
    U(SetWindowPos)(h, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    r = ht(h, g.wr.left + g.fx + 4, y);
    CHECKF(r != HTSYSMENU, "after removing WS_SYSMENU -> %lld", (long long)r);
    mu_kill(h);
}
TEST(misc_ui_nchittest_menu_bar) {
    NEED_DP(); NEED_GEO(); REQUIRE(U(CreateMenu)); REQUIRE(U(AppendMenuA)); REQUIRE(U(SetMenu));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    HMENU m = U(CreateMenu)();
    U(AppendMenuA)(m, MF_STRING, 100, "&File");
    U(AppendMenuA)(m, MF_STRING, 101, "&Edit");
    CHECK(U(SetMenu)(h, m));
    hitgeo g; geo(h, &g);
    CHECK(ht(h, g.cr.left + 3, g.cr.top - 4) == HTMENU);
    CHECK(ht(h, g.cr.right - 5, g.cr.top - 4) == HTMENU);       /* the whole bar, not only the items */
    CHECK(ht(h, g.mid_x, g.cr.top + 3) == HTCLIENT);
    U(SetMenu)(h, NULL);
    U(DestroyMenu)(m);
    mu_kill(h);
}
TEST(misc_ui_nchittest_scrollbars) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    int my = (g.cr.top + g.cr.bottom) / 2, mx = (g.cr.left + g.cr.right) / 2;
    CHECK(ht(h, g.cr.right + 3, my) == HTVSCROLL);
    CHECK(ht(h, mx, g.cr.bottom + 3) == HTHSCROLL);
    LRESULT corner = ht(h, g.cr.right + 3, g.cr.bottom + 3);
    t_note("scrollbar corner -> %lld (HTSIZE/HTGROWBOX=%d)", (long long)corner, HTSIZE);
    CHECK(ht(h, mx, my) == HTCLIENT);
    mu_kill(h);
}
TEST(misc_ui_nchittest_border_only) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_POPUP | WS_BORDER, 0, 200, 150, 120, 80, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    CHECK(ht(h, g.wr.left, g.mid_y) == HTBORDER);
    CHECK(ht(h, g.wr.right - 1, g.mid_y) == HTBORDER);
    CHECK(ht(h, g.mid_x, g.wr.top) == HTBORDER);
    CHECK(ht(h, g.mid_x, g.wr.bottom - 1) == HTBORDER);
    CHECK(ht(h, g.mid_x, g.mid_y) == HTCLIENT);
    CHECK(ht(h, g.cr.left, g.cr.top) == HTCLIENT);
    mu_kill(h);
}
TEST(misc_ui_nchittest_popup_no_frame_is_all_client) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_POPUP, 0, 200, 150, 120, 80, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    CHECK(EqualRect(&g.wr, &g.cr));
    CHECK(ht(h, g.wr.left, g.wr.top) == HTCLIENT);
    CHECK(ht(h, g.wr.right - 1, g.wr.bottom - 1) == HTCLIENT);
    CHECK(ht(h, g.wr.right, g.wr.bottom) == HTNOWHERE);
    CHECK(ht(h, g.wr.left - 1, g.wr.top) == HTNOWHERE);
    mu_kill(h);
}
TEST(misc_ui_nchittest_client_edge) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_POPUP, WS_EX_CLIENTEDGE, 200, 150, 120, 80, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    CHECK(!EqualRect(&g.wr, &g.cr));
    t_note("client-edge-only left edge -> %lld (HTBORDER=%d)", (long long)ht(h, g.wr.left, g.mid_y), HTBORDER);
    CHECK(ht(h, g.mid_x, g.mid_y) == HTCLIENT);
    mu_kill(h);
}
TEST(misc_ui_nchittest_negative_coordinates) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, -600, -500, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    CHECK(g.wr.left < 0 || g.wr.top < 0);
    CHECK(ht(h, (g.cr.left + g.cr.right) / 2, (g.cr.top + g.cr.bottom) / 2) == HTCLIENT);
    CHECK(ht(h, g.mid_x, g.wr.top + g.fy + g.cap / 2) == HTCAPTION);
    CHECK(ht(h, g.wr.right + 5, g.mid_y) == HTNOWHERE);
    mu_kill(h);
}
TEST(misc_ui_nchittest_lparam_packing) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_POPUP, 0, 100, 100, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    /* high bits of lParam beyond 32 are ignored; x/y are signed 16 bit */
    LPARAM l = MAKELPARAM(120, 120);
    CHECK(U(SendMessageA)(h, WM_NCHITTEST, 0, l) == HTCLIENT);
    CHECK(U(SendMessageA)(h, WM_NCHITTEST, 0xFFFF, l) == HTCLIENT);          /* wParam unused */
    CHECK(U(SendMessageA)(h, WM_NCHITTEST, 0, l | 0x1234567800000000LL) == HTCLIENT);
    CHECK(U(SendMessageA)(h, WM_NCHITTEST, 0, MAKELPARAM(0xFFFF, 0xFFFF)) == HTNOWHERE);   /* (-1,-1) */
    mu_kill(h);
}
TEST(misc_ui_nchittest_child_window) {
    NEED_DP(); NEED_GEO();
    HWND p = mu_mk(WS_POPUP, 0, 100, 100, 200, 200, NULL);
    HWND c = p ? mu_mk(WS_CHILD, 0, 10, 10, 60, 40, p) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    hitgeo g; geo(c, &g);
    CHECK(ht(c, g.mid_x, g.mid_y) == HTCLIENT);
    CHECK(ht(c, g.wr.left - 1, g.mid_y) == HTNOWHERE);
    CHECK(ht(c, g.wr.right, g.mid_y) == HTNOWHERE);
    mu_kill(p);
}
TEST(misc_ui_nchittest_disabled_and_hidden_unchanged) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 200, 150, 400, 300, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    LRESULT a = ht(h, g.mid_x, g.mid_y);
    U(EnableWindow)(h, FALSE);
    LRESULT b = ht(h, g.mid_x, g.mid_y);
    CHECK(a == HTCLIENT && b == HTCLIENT);
    U(ShowWindow)(h, SW_SHOWNOACTIVATE);
    CHECK(ht(h, g.mid_x, g.mid_y) == HTCLIENT);
    U(ShowWindow)(h, SW_HIDE);
    mu_kill(h);
}
TEST(misc_ui_nchittest_ex_toolwindow_caption) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_POPUP | WS_CAPTION | WS_SYSMENU, WS_EX_TOOLWINDOW, 200, 150, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    int cy = U(GetSystemMetrics)(SM_CYSMCAPTION);
    CHECK(ht(h, g.mid_x, g.wr.top + 3 + cy / 2) == HTCAPTION);
    CHECK(ht(h, g.mid_x, g.mid_y) == HTCLIENT);
    mu_kill(h);
}

/* ---------- WM_NCCALCSIZE ---------- */
TEST(misc_ui_nccalcsize_false_matches_client) {
    NEED_DP(); NEED_GEO();
    DWORD styles[] = { WS_OVERLAPPEDWINDOW, WS_POPUP, WS_POPUP | WS_BORDER, WS_POPUP | WS_CAPTION, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, WS_POPUP | WS_THICKFRAME };
    DWORD exs[] = { 0, WS_EX_CLIENTEDGE, WS_EX_TOOLWINDOW, WS_EX_STATICEDGE, WS_EX_WINDOWEDGE };
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 5; j++) {
            HWND h = mu_mk(styles[i], exs[j], 150, 150, 300, 220, NULL);
            if (!h) { CHECK(0); continue; }
            hitgeo g; geo(h, &g);
            RECT r = g.wr;
            LRESULT res = U(SendMessageA)(h, WM_NCCALCSIZE, FALSE, (LPARAM)&r);
            CHECKF(res == 0, "style %lx ex %lx res=%lld", styles[i], exs[j], (long long)res);
            CHECKF(EqualRect(&r, &g.cr), "style %lx ex %lx got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)", styles[i], exs[j],
                   r.left, r.top, r.right, r.bottom, g.cr.left, g.cr.top, g.cr.right, g.cr.bottom);
            mu_kill(h);
        }
}
TEST(misc_ui_nccalcsize_true_matches_client) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 150, 150, 300, 220, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    NCCALCSIZE_PARAMS p; WINDOWPOS wp;
    memset(&p, 0, sizeof p); memset(&wp, 0, sizeof wp);
    p.rgrc[0] = g.wr; p.rgrc[1] = g.wr; p.rgrc[2] = g.cr;
    wp.hwnd = h; wp.x = g.wr.left; wp.y = g.wr.top; wp.cx = g.wr.right - g.wr.left; wp.cy = g.wr.bottom - g.wr.top;
    p.lppos = &wp;
    LRESULT res = U(SendMessageA)(h, WM_NCCALCSIZE, TRUE, (LPARAM)&p);
    t_note("NCCALCSIZE(TRUE) default returned 0x%llx", (long long)res);
    CHECK(EqualRect(&p.rgrc[0], &g.cr));
    mu_kill(h);
}
TEST(misc_ui_nccalcsize_resize_grows_client) {
    NEED_DP(); NEED_GEO();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 150, 150, 300, 220, NULL);
    if (!h) { CHECK(0); return; }
    hitgeo g; geo(h, &g);
    RECT bigger = g.wr; bigger.right += 100; bigger.bottom += 50;
    U(SendMessageA)(h, WM_NCCALCSIZE, FALSE, (LPARAM)&bigger);
    CHECK(bigger.right - bigger.left == g.cr.right - g.cr.left + 100);
    CHECK(bigger.bottom - bigger.top == g.cr.bottom - g.cr.top + 50);
    CHECK(bigger.left == g.cr.left && bigger.top == g.cr.top);
    mu_kill(h);
}
TEST(misc_ui_nccalcsize_adjustwindowrect_consistency) {
    NEED_DP(); NEED_GEO(); REQUIRE(U(AdjustWindowRectEx)); REQUIRE(U(AdjustWindowRect));
    DWORD styles[] = { WS_OVERLAPPEDWINDOW, WS_POPUP | WS_BORDER, WS_POPUP | WS_CAPTION | WS_SYSMENU, WS_POPUP | WS_THICKFRAME, WS_POPUP };
    DWORD exs[] = { 0, WS_EX_CLIENTEDGE, WS_EX_TOOLWINDOW, WS_EX_DLGMODALFRAME };
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 4; j++) {
            RECT rc = { 0, 0, 200, 100 };
            CHECK(U(AdjustWindowRectEx)(&rc, styles[i], FALSE, exs[j]));
            HWND h = mu_mk(styles[i], exs[j], 100, 100, rc.right - rc.left, rc.bottom - rc.top, NULL);
            if (!h) { CHECK(0); continue; }
            RECT c; U(GetClientRect)(h, &c);
            CHECKF(c.right == 200 && c.bottom == 100, "style %lx ex %lx client %ldx%ld", styles[i], exs[j], c.right, c.bottom);
            mu_kill(h);
        }
    RECT a = { 0, 0, 200, 100 }, b = { 0, 0, 200, 100 };
    U(AdjustWindowRect)(&a, WS_OVERLAPPEDWINDOW, FALSE);
    U(AdjustWindowRectEx)(&b, WS_OVERLAPPEDWINDOW, FALSE, 0);
    CHECK(EqualRect(&a, &b));
}
TEST(misc_ui_nccalcsize_adjustwindowrect_menu) {
    NEED_DP(); REQUIRE(U(AdjustWindowRectEx));
    RECT a = { 0, 0, 200, 100 }, b = { 0, 0, 200, 100 };
    U(AdjustWindowRectEx)(&a, WS_OVERLAPPEDWINDOW, FALSE, 0);
    U(AdjustWindowRectEx)(&b, WS_OVERLAPPEDWINDOW, TRUE, 0);
    t_note("AdjustWindowRectEx menu delta: %ld (SM_CYMENU=%d)", a.top - b.top, U(GetSystemMetrics)(SM_CYMENU));
    CHECK(b.top < a.top);
    CHECK(b.left == a.left && b.right == a.right && b.bottom == a.bottom);
    RECT c = { 0, 0, 50, 50 }, d = { 0, 0, 50, 50 };
    U(AdjustWindowRectEx)(&c, WS_CHILD, FALSE, 0);
    U(AdjustWindowRectEx)(&d, WS_CHILD, TRUE, 0);
    t_note("WS_CHILD with bMenu: top %ld vs %ld", c.top, d.top);
    /* the bare popup does not change the rectangle */
    RECT e = { 5, 6, 55, 66 };
    U(AdjustWindowRectEx)(&e, WS_POPUP, FALSE, 0);
    CHECK(e.left == 5 && e.top == 6 && e.right == 55 && e.bottom == 66);
}

/* ---------- WM_GETTEXT / WM_SETTEXT ---------- */
TEST(misc_ui_text_gettext_buffer_sizes) {
    NEED_DP(); REQUIRE(U(SetWindowTextA)); REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetWindowTextLengthA));
    HWND h = mu_mk_cls(MU_CLS, "hello", WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    char b[32];
    CHECK(U(SendMessageA)(h, WM_GETTEXTLENGTH, 0, 0) == 5);
    memset(b, 'x', sizeof b);
    CHECK(U(SendMessageA)(h, WM_GETTEXT, 6, (LPARAM)b) == 5 && !strcmp(b, "hello"));
    memset(b, 'x', sizeof b);
    CHECK(U(SendMessageA)(h, WM_GETTEXT, 5, (LPARAM)b) == 4 && !strcmp(b, "hell"));
    memset(b, 'x', sizeof b);
    CHECK(U(SendMessageA)(h, WM_GETTEXT, 2, (LPARAM)b) == 1 && !strcmp(b, "h"));
    memset(b, 'x', sizeof b);
    CHECK(U(SendMessageA)(h, WM_GETTEXT, 1, (LPARAM)b) == 0 && b[0] == 0);
    memset(b, 'x', sizeof b);
    CHECK(U(SendMessageA)(h, WM_GETTEXT, 0, (LPARAM)b) == 0);
    memset(b, 'x', sizeof b);
    CHECK(U(SendMessageA)(h, WM_GETTEXT, 32, (LPARAM)b) == 5 && !strcmp(b, "hello"));
    /* GetWindowText wrappers */
    memset(b, 'x', sizeof b);
    CHECK(U(GetWindowTextA)(h, b, 6) == 5 && !strcmp(b, "hello"));
    memset(b, 'x', sizeof b);
    CHECK(U(GetWindowTextA)(h, b, 5) == 4 && !strcmp(b, "hell"));
    memset(b, 'x', sizeof b);
    CHECK(U(GetWindowTextA)(h, b, 1) == 0 && b[0] == 0);
    memset(b, 'x', sizeof b);
    CHECK(U(GetWindowTextA)(h, b, 0) == 0 && b[0] == 'x');
    CHECK(U(GetWindowTextLengthA)(h) == 5);
    mu_kill(h);
}
TEST(misc_ui_text_gettext_wide_buffer_sizes) {
    NEED_DP(); REQUIRE(U(GetWindowTextW)); REQUIRE(U(GetWindowTextLengthW)); REQUIRE(U(SendMessageW));
    HWND h = mu_mk_cls(MU_CLS, "hello", WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    WCHAR w[32];
    for (int i = 0; i < 32; i++) w[i] = L'x';
    CHECK(U(GetWindowTextW)(h, w, 6) == 5 && !lstrcmpW(w, L"hello"));
    for (int i = 0; i < 32; i++) w[i] = L'x';
    { int n3 = U(GetWindowTextW)(h, w, 3);   /* Win10 22H2 GetWindowTextW(small buffer): recorded, invariants only */
      t_note("GetWindowTextW(buf 3) -> %d w[0..2]=%04x %04x %04x", n3, w[0], w[1], w[2]);
      /* Win10 22H2 returns the full length (5) from the wide call on a small buffer and fills it without a
       * terminator; what must hold is that nothing is written past the buffer and the prefix is right. */
      CHECK(w[3] == L'x' && w[0] == L'h' && w[1] == L'e'); }
    for (int i = 0; i < 32; i++) w[i] = L'x';
    { int n1 = U(GetWindowTextW)(h, w, 1);
      t_note("GetWindowTextW(buf 1) -> %d w[0]=%04x", n1, w[0]);
      CHECK(w[1] == L'x'); }
    for (int i = 0; i < 32; i++) w[i] = L'x';
    CHECK(U(GetWindowTextW)(h, w, 0) == 0 && w[0] == L'x');
    CHECK(U(GetWindowTextLengthW)(h) == 5);
    CHECK(U(SendMessageW)(h, WM_GETTEXTLENGTH, 0, 0) == 5);
    for (int i = 0; i < 32; i++) w[i] = L'x';
    CHECK(U(SendMessageW)(h, WM_GETTEXT, 32, (LPARAM)w) == 5 && !lstrcmpW(w, L"hello"));
    mu_kill(h);
}
TEST(misc_ui_text_settext_roundtrip_ascii) {
    NEED_DP(); REQUIRE(U(SetWindowTextA)); REQUIRE(U(SetWindowTextW)); REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetWindowTextW));
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    char a[64]; WCHAR w[64];
    CHECK(U(SetWindowTextA)(h, "ansi text"));
    CHECK(U(GetWindowTextA)(h, a, 64) == 9 && !strcmp(a, "ansi text"));
    CHECK(U(GetWindowTextW)(h, w, 64) == 9 && !lstrcmpW(w, L"ansi text"));
    CHECK(U(SetWindowTextW)(h, L"wide text!"));
    CHECK(U(GetWindowTextA)(h, a, 64) == 10 && !strcmp(a, "wide text!"));
    CHECK(U(GetWindowTextW)(h, w, 64) == 10 && !lstrcmpW(w, L"wide text!"));
    CHECK(U(SetWindowTextA)(h, ""));
    CHECK(U(GetWindowTextA)(h, a, 64) == 0 && a[0] == 0);
    CHECK(U(GetWindowTextLengthA)(h) == 0);
    CHECK(U(SetWindowTextA)(h, "abc"));
    CHECK(U(SetWindowTextA)(h, NULL));
    CHECK(U(GetWindowTextA)(h, a, 64) == 0);
    mu_kill(h);
}
TEST(misc_ui_text_settext_message_return) {
    NEED_DP();
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(U(SendMessageA)(h, WM_SETTEXT, 0, (LPARAM)"abc") == TRUE);
    CHECK(U(SendMessageA)(h, WM_SETTEXT, 12345, (LPARAM)"abcd") == TRUE);   /* wParam ignored */
    char b[16];
    CHECK(U(SendMessageA)(h, WM_GETTEXT, 16, (LPARAM)b) == 4 && !strcmp(b, "abcd"));
    CHECK(U(SendMessageW)(h, WM_SETTEXT, 0, (LPARAM)L"wxyz!") == TRUE);
    CHECK(U(SendMessageA)(h, WM_GETTEXT, 16, (LPARAM)b) == 5 && !strcmp(b, "wxyz!"));
    mu_kill(h);
}
TEST(misc_ui_text_long_text) {
    NEED_DP(); REQUIRE(U(SetWindowTextA)); REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetWindowTextLengthA));
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    enum { N = 20000 };
    char *s = malloc(N + 1), *o = malloc(N + 8);
    for (int i = 0; i < N; i++) s[i] = 'a' + i % 26;
    s[N] = 0;
    CHECK(U(SetWindowTextA)(h, s));
    int len = U(GetWindowTextLengthA)(h);
    CHECKF(len == N, "len=%d", len);
    memset(o, 0, N + 8);
    int got = U(GetWindowTextA)(h, o, N + 1);
    CHECKF(got == N, "got=%d", got);
    CHECK(!memcmp(o, s, N));
    int small = U(GetWindowTextA)(h, o, 100);
    CHECK(small == 99);
    free(s); free(o);
    mu_kill(h);
}
TEST(misc_ui_text_nonascii_wide_roundtrip) {
    NEED_DP(); REQUIRE(U(SetWindowTextW)); REQUIRE(U(GetWindowTextW)); REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetWindowTextLengthA)); REQUIRE(U(GetWindowTextLengthW));
    REQUIRE(U(RegisterClassExW)); REQUIRE(U(CreateWindowExW));
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = L"MU_WideTextCls";
    U(RegisterClassExW)(&wc);
    HWND h = U(CreateWindowExW)(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, 50, 50, NULL, NULL, wc.hInstance, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(U(IsWindowUnicode)(h));
    static const WCHAR txt[] = { 'a', 0x00e9, 0x4e2d, 0x6587, 0xd83d, 0xde00, 'z', 0 };
    CHECK(U(SetWindowTextW)(h, txt));
    WCHAR w[32];
    CHECK(U(GetWindowTextW)(h, w, 32) == 7);
    CHECK(!lstrcmpW(w, txt));
    CHECK(U(GetWindowTextLengthW)(h) == 7);
    char a[64];
    int n = U(GetWindowTextA)(h, a, 64);
    int la = U(GetWindowTextLengthA)(h);
    t_note("non-ASCII ANSI view: GetWindowTextA=%d GetWindowTextLengthA=%d first=%d last=%d", n, la, (unsigned char)a[0], n > 0 ? (unsigned char)a[n - 1] : -1);
    CHECK(la >= n);
    U(DestroyWindow)(h);
    U(UnregisterClassW)(wc.lpszClassName, wc.hInstance);
}
TEST(misc_ui_text_defwindowproc_a_vs_w) {
    NEED_DP(); REQUIRE(U(DefWindowProcW)); REQUIRE(U(GetWindowTextA));
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    char a[32]; WCHAR w[32];
    CHECK(U(DefWindowProcW)(h, WM_SETTEXT, 0, (LPARAM)L"fromW") == TRUE);
    CHECK(U(GetWindowTextA)(h, a, 32) == 5 && !strcmp(a, "fromW"));
    CHECK(U(DefWindowProcA)(h, WM_SETTEXT, 0, (LPARAM)"fromA") == TRUE);
    CHECK(U(GetWindowTextA)(h, a, 32) == 5 && !strcmp(a, "fromA"));
    CHECK(U(DefWindowProcW)(h, WM_GETTEXTLENGTH, 0, 0) == 5);
    CHECK(U(DefWindowProcA)(h, WM_GETTEXTLENGTH, 0, 0) == 5);
    memset(w, 0, sizeof w);
    CHECK(U(DefWindowProcW)(h, WM_GETTEXT, 32, (LPARAM)w) == 5 && !lstrcmpW(w, L"fromA"));
    memset(a, 0, sizeof a);
    CHECK(U(DefWindowProcA)(h, WM_GETTEXT, 32, (LPARAM)a) == 5 && !strcmp(a, "fromA"));
    memset(w, 0, sizeof w);
    CHECK(U(DefWindowProcW)(h, WM_GETTEXT, 3, (LPARAM)w) == 2 && !lstrcmpW(w, L"fr"));
    CHECK(U(DefWindowProcW)(h, WM_GETTEXT, 1, (LPARAM)w) == 0 && w[0] == 0);
    mu_kill(h);
}
static int hook_text(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT *r)
{
    if (m == WM_GETTEXT) {
        const char *s = "custom";
        if (w) { lstrcpynA((char *)l, s, (int)w); *r = lstrlenA((char *)l); } else *r = 0;
        return 1;
    }
    if (m == WM_GETTEXTLENGTH) { *r = 6; return 1; }
    return 0;
}
TEST(misc_ui_text_custom_gettext_and_internal) {
    NEED_DP(); REQUIRE(U(GetWindowTextA)); REQUIRE(U(InternalGetWindowText)); REQUIRE(U(GetWindowTextLengthA));
    HWND h = mu_mk_cls(MU_CLS, "realtext", WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    mu_hook = hook_text;
    char a[32];
    CHECK(U(GetWindowTextA)(h, a, 32) == 6 && !strcmp(a, "custom"));
    CHECK(U(GetWindowTextLengthA)(h) == 6);
    WCHAR w[32];
    int n = U(InternalGetWindowText)(h, w, 32);
    CHECK(n == 8 && !lstrcmpW(w, L"realtext"));
    mu_hook = NULL;
    mu_kill(h);
}
TEST(misc_ui_text_internalgetwindowtext_sizes) {
    NEED_DP(); REQUIRE(U(InternalGetWindowText));
    HWND h = mu_mk_cls(MU_CLS, "abcdef", WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    WCHAR w[16];
    for (int i = 0; i < 16; i++) w[i] = L'x';
    CHECK(U(InternalGetWindowText)(h, w, 16) == 6 && !lstrcmpW(w, L"abcdef"));
    for (int i = 0; i < 16; i++) w[i] = L'x';
    CHECK(U(InternalGetWindowText)(h, w, 7) == 6 && !lstrcmpW(w, L"abcdef"));
    for (int i = 0; i < 16; i++) w[i] = L'x';
    int n = U(InternalGetWindowText)(h, w, 6);
    CHECK(n == 5 && !lstrcmpW(w, L"abcde"));
    for (int i = 0; i < 16; i++) w[i] = L'x';
    n = U(InternalGetWindowText)(h, w, 1);
    CHECK(n == 0 && w[0] == 0);
    for (int i = 0; i < 16; i++) w[i] = L'x';
    n = U(InternalGetWindowText)(h, w, 0);
    CHECK(n == 0 && w[0] == 0);
    for (int i = 0; i < 16; i++) w[i] = L'x';
    CHECK(U(InternalGetWindowText)(BOGUS_HWND, w, 16) == 0 && w[0] == 0);
    for (int i = 0; i < 16; i++) w[i] = L'x';
    CHECK(U(InternalGetWindowText)(NULL, w, 16) == 0 && w[0] == 0);
    mu_kill(h);
}
TEST(misc_ui_text_getwindowtext_invalid_handle) {
    NEED_DP(); REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetWindowTextW)); REQUIRE(U(GetWindowTextLengthA)); REQUIRE(U(GetWindowTextLengthW)); REQUIRE(U(SetWindowTextA));
    HWND bad[] = { NULL, BOGUS_HWND, (HWND)0x6FFF0123 };
    for (int i = 0; i < 3; i++) {
        char a[8] = "xxxxxxx"; WCHAR w[8] = L"xxxxxxx";
        SetLastError(0);
        CHECKF(U(GetWindowTextA)(bad[i], a, 8) == 0, "A %p", bad[i]);
        CHECKF(U(GetWindowTextW)(bad[i], w, 8) == 0, "W %p", bad[i]);
        CHECKF(U(GetWindowTextLengthA)(bad[i]) == 0, "LenA %p", bad[i]);
        CHECKF(U(GetWindowTextLengthW)(bad[i]) == 0, "LenW %p", bad[i]);
        SetLastError(0);
        CHECKF(!U(SetWindowTextA)(bad[i], "x"), "Set %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "Set %p err=%lu", bad[i], GetLastError());
    }
}
TEST(misc_ui_text_cross_thread_gettext_sends_message) {
    NEED_DP(); REQUIRE(U(GetWindowTextA));
    static volatile LONG nget; static volatile DWORD last_tid;
    int hk(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT *r) { if (m == WM_GETTEXT) { InterlockedIncrement(&nget); last_tid = GetCurrentThreadId(); } return 0; }
    mu_worker wk;
    if (!mu_worker_start(&wk, WS_POPUP, 0, "other thread")) { CHECK(0); mu_worker_stop(&wk); return; }
    nget = 0;
    mu_hook = hk;
    char a[64] = "";
    CHECK(U(GetWindowTextA)(wk.hwnd, a, 64) == 12);
    CHECK(!strcmp(a, "other thread"));
    CHECK(nget >= 1);
    CHECK(last_tid == wk.tid);
    mu_hook = NULL;
    int n = nget;
    CHECK(U(GetWindowTextLengthA)(wk.hwnd) == 12);
    CHECK(U(SetWindowTextA)(wk.hwnd, "changed"));
    CHECK(U(GetWindowTextA)(wk.hwnd, a, 64) == 7 && !strcmp(a, "changed"));
    mu_worker_stop(&wk);
}
TEST(misc_ui_text_other_process_window) {
    NEED_DP(); REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetShellWindow)); REQUIRE(U(GetDesktopWindow));
    char a[64] = "xx";
    int n = U(GetWindowTextA)(U(GetDesktopWindow)(), a, 64);
    CHECK(n == 0 && a[0] == 0);
    HWND sh = U(GetShellWindow)();
    if (sh) {
        n = U(GetWindowTextA)(sh, a, 64);
        t_note("shell window text: \"%s\" (%d)", a, n);
        DWORD pid = 0; U(GetWindowThreadProcessId)(sh, &pid);
        CHECK(pid != GetCurrentProcessId());
    }
}

/* ---------- WM_SYSCOMMAND ---------- */
static int g_close_seen;
static int hook_swallow_close(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT *r)
{
    if (m == WM_CLOSE) { g_close_seen++; *r = 0; return 1; }
    return 0;
}
TEST(misc_ui_syscommand_minimize_maximize_restore) {
    NEED_DP(); REQUIRE(U(IsIconic)); REQUIRE(U(IsZoomed)); REQUIRE(U(ShowWindow));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(!U(IsIconic)(h) && !U(IsZoomed)(h));
    CHECK(U(SendMessageA)(h, WM_SYSCOMMAND, SC_MINIMIZE, 0) == 0);
    CHECK(U(IsIconic)(h));
    CHECK((mu_style(h) & WS_MINIMIZE) != 0);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    CHECK(!U(IsIconic)(h) && !U(IsZoomed)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    CHECK(U(IsZoomed)(h) && !U(IsIconic)(h));
    CHECK((mu_style(h) & WS_MAXIMIZE) != 0);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    CHECK(!U(IsZoomed)(h) && !U(IsIconic)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    t_note("SC_MAXIMIZE on iconic window: iconic=%d zoomed=%d", U(IsIconic)(h), U(IsZoomed)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    t_note("SC_RESTORE x1 after min+max: iconic=%d zoomed=%d", U(IsIconic)(h), U(IsZoomed)(h));
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    mu_kill(h);
}
TEST(misc_ui_syscommand_low_bits_ignored) {
    NEED_DP(); REQUIRE(U(IsIconic));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    for (int low = 0; low < 16; low += 5) {
        U(SendMessageA)(h, WM_SYSCOMMAND, SC_MINIMIZE | low, 0);
        CHECKF(U(IsIconic)(h), "low=%d", low);
        U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE | low, 0);
        CHECKF(!U(IsIconic)(h), "restore low=%d", low);
    }
    mu_kill(h);
}
TEST(misc_ui_syscommand_close_sends_wm_close) {
    NEED_DP();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    mu_hook = hook_swallow_close; g_close_seen = 0;
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_CLOSE, 0);
    CHECK(g_close_seen == 1);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_CLOSE | 0xF, 0);
    CHECK(g_close_seen == 2);
    CHECK(U(IsWindow)(h));
    mu_hook = NULL;
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_CLOSE, 0);
    CHECK(!U(IsWindow)(h));
}
TEST(misc_ui_syscommand_close_no_sysmenu) {
    NEED_DP();
    HWND h = mu_mk(WS_POPUP | WS_CAPTION, 0, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    mu_hook = hook_swallow_close; g_close_seen = 0;
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_CLOSE, 0);
    t_note("SC_CLOSE on window without WS_SYSMENU: WM_CLOSE seen=%d", g_close_seen);
    mu_hook = NULL;
    mu_kill(h);
}
TEST(misc_ui_syscommand_close_cs_noclose) {
    NEED_DP(); REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = mu_proc; wc.hInstance = GetModuleHandleA(NULL);
    wc.style = CS_NOCLOSE; wc.lpszClassName = "MU_NoCloseCls";
    CHECK(U(RegisterClassExA)(&wc) != 0);
    HWND h = mu_mk_cls("MU_NoCloseCls", "nc", WS_OVERLAPPEDWINDOW, 0, 100, 100, 300, 200, NULL);
    CHECK(h != NULL);
    if (h) {
        mu_hook = hook_swallow_close; g_close_seen = 0;
        U(SendMessageA)(h, WM_SYSCOMMAND, SC_CLOSE, 0);
        t_note("CS_NOCLOSE class: WM_SYSCOMMAND SC_CLOSE -> WM_CLOSE seen=%d", g_close_seen);
        U(SendMessageA)(h, WM_CLOSE, 0, 0);
        CHECK(g_close_seen >= 1);
        mu_hook = NULL;
        mu_kill(h);
    }
    U(UnregisterClassA)("MU_NoCloseCls", GetModuleHandleA(NULL));
}
TEST(misc_ui_syscommand_minimize_without_box) {
    NEED_DP(); REQUIRE(U(IsIconic));
    HWND h = mu_mk(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, WS_EX_NOACTIVATE, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    t_note("SC_MINIMIZE without WS_MINIMIZEBOX: iconic=%d", U(IsIconic)(h));
    U(ShowWindow)(h, SW_RESTORE);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    t_note("SC_MAXIMIZE without WS_MAXIMIZEBOX: zoomed=%d", U(IsZoomed)(h));
    U(ShowWindow)(h, SW_RESTORE);
    mu_kill(h);
}
TEST(misc_ui_syscommand_not_sent_by_showwindow) {
    NEED_DP(); REQUIRE(U(ShowWindow));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    mu_log_reset(); mu_logging = 1;
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    U(ShowWindow)(h, SW_RESTORE);
    mu_logging = 0;
    CHECK(mu_count(WM_SYSCOMMAND) == 0);
    CHECK(mu_count(WM_SIZE) >= 1);
    int i = mu_find(WM_SIZE);
    if (i >= 0) CHECK(mu_log[i].w == SIZE_MINIMIZED);
    mu_kill(h);
}
TEST(misc_ui_syscommand_wm_size_types) {
    NEED_DP();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    mu_log_reset(); mu_logging = 1;
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    U(SendMessageA)(h, WM_SYSCOMMAND, SC_RESTORE, 0);
    mu_logging = 0;
    int seen[4] = {0};
    for (int i = 0; i < mu_nlog; i++) if (mu_log[i].msg == WM_SIZE && mu_log[i].w < 4) seen[mu_log[i].w]++;
    CHECK(seen[SIZE_MAXIMIZED] >= 1);
    CHECK(seen[SIZE_MINIMIZED] >= 1);
    CHECK(seen[SIZE_RESTORED] >= 2);
    mu_kill(h);
}
TEST(misc_ui_wm_close_default_destroys) {
    NEED_DP();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    mu_log_reset(); mu_logging = 1;
    CHECK(U(SendMessageA)(h, WM_CLOSE, 0, 0) == 0);
    mu_logging = 0;
    CHECK(!U(IsWindow)(h));
    CHECK(mu_count_h(h, WM_DESTROY) == 1 && mu_count_h(h, WM_NCDESTROY) == 1);
}
TEST(misc_ui_closewindow_openicon) {
    NEED_DP(); REQUIRE(U(CloseWindow)); REQUIRE(U(OpenIcon)); REQUIRE(U(IsIconic));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(U(CloseWindow)(h));
    CHECK(U(IsIconic)(h));
    CHECK(U(CloseWindow)(h));                  /* already minimized */
    CHECK(U(IsIconic)(h));
    CHECK(U(OpenIcon)(h));
    CHECK(!U(IsIconic)(h));
    CHECK(U(OpenIcon)(h));                     /* not minimized: still TRUE */
    CHECK(!U(IsIconic)(h));
    mu_kill(h);
}
TEST(misc_ui_closewindow_openicon_invalid) {
    NEED_DP(); REQUIRE(U(CloseWindow)); REQUIRE(U(OpenIcon));
    HWND bad[] = { NULL, BOGUS_HWND, (HWND)0x6FFF0123 };
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        CHECKF(!U(CloseWindow)(bad[i]), "CloseWindow %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "CloseWindow err %lu", GetLastError());
        SetLastError(0);
        CHECKF(!U(OpenIcon)(bad[i]), "OpenIcon %p", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "OpenIcon err %lu", GetLastError());
    }
}
TEST(misc_ui_closewindow_hidden_window_becomes_visible_icon) {
    NEED_DP(); REQUIRE(U(CloseWindow)); REQUIRE(U(IsIconic));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, WS_EX_NOACTIVATE, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(!U(IsWindowVisible)(h));
    CHECK(U(CloseWindow)(h));
    CHECK(U(IsIconic)(h));
    t_note("CloseWindow on hidden: visible=%d", U(IsWindowVisible)(h));
    mu_kill(h);
}
TEST(misc_ui_closewindow_cross_thread) {
    NEED_DP(); REQUIRE(U(CloseWindow)); REQUIRE(U(OpenIcon));
    mu_worker wk;
    if (!mu_worker_start(&wk, WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, "x")) { CHECK(0); mu_worker_stop(&wk); return; }
    CHECK(U(CloseWindow)(wk.hwnd));
    CHECK(U(IsIconic)(wk.hwnd));
    CHECK(U(OpenIcon)(wk.hwnd));
    CHECK(!U(IsIconic)(wk.hwnd));
    mu_worker_stop(&wk);
}

/* ---------- misc default handling ---------- */
TEST(misc_ui_def_setredraw) {
    NEED_DP();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(U(SendMessageA)(h, WM_SETREDRAW, FALSE, 0) == 0);
    t_note("after WM_SETREDRAW(FALSE): WS_VISIBLE=%d IsWindowVisible=%d", (mu_style(h) & WS_VISIBLE) != 0, U(IsWindowVisible)(h));
    CHECK(U(SendMessageA)(h, WM_SETREDRAW, TRUE, 0) == 0);
    CHECK((mu_style(h) & WS_VISIBLE) != 0);
    CHECK(U(IsWindowVisible)(h));
    mu_kill(h);
}
TEST(misc_ui_def_ncactivate_returns_true) {
    NEED_DP();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(U(SendMessageA)(h, WM_NCACTIVATE, TRUE, (LPARAM)-1) == TRUE);
    CHECK(U(SendMessageA)(h, WM_NCACTIVATE, FALSE, (LPARAM)-1) == TRUE);
    CHECK(U(SendMessageA)(h, WM_NCACTIVATE, TRUE, 0) == TRUE);
    CHECK(U(SendMessageA)(h, WM_NCACTIVATE, FALSE, 0) == TRUE);
    mu_kill(h);
}
TEST(misc_ui_def_queryendsession) {
    NEED_DP();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(U(SendMessageA)(h, WM_QUERYENDSESSION, 0, 0) == TRUE);
    CHECK(U(SendMessageA)(h, WM_QUERYENDSESSION, 0, ENDSESSION_CLOSEAPP) == TRUE);
    CHECK(U(SendMessageA)(h, WM_QUERYENDSESSION, 0, ENDSESSION_LOGOFF) == TRUE);
    CHECK(U(SendMessageA)(h, WM_ENDSESSION, FALSE, 0) == 0);
    CHECK(U(SendMessageA)(h, WM_ENDSESSION, FALSE, ENDSESSION_LOGOFF) == 0);
    CHECK(U(IsWindow)(h));
    mu_kill(h);
}
TEST(misc_ui_def_queryopen_and_misc_defaults) {
    NEED_DP();
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    CHECK(U(SendMessageA)(h, WM_QUERYOPEN, 0, 0) == TRUE);
    CHECK(U(SendMessageA)(h, WM_NULL, 0, 0) == 0);
    CHECK(U(SendMessageA)(h, WM_NULL, 0xFFFFFFFF, -1) == 0);
    CHECK(U(SendMessageA)(h, WM_SHOWWINDOW, 0, 0) == 0);
    CHECK(U(SendMessageA)(h, WM_SETFONT, 0, 0) == 0);
    CHECK(U(SendMessageA)(h, WM_GETFONT, 0, 0) == 0);
    CHECK(U(SendMessageA)(h, WM_USER + 5, 1, 2) == 0);
    CHECK(U(SendMessageA)(h, 0xBFFF, 1, 2) == 0);
    CHECK(U(SendMessageA)(h, 0xC123, 1, 2) == 0);
    mu_kill(h);
}
TEST(misc_ui_def_geticon_seticon) {
    NEED_DP(); REQUIRE(U(LoadIconA)); REQUIRE(U(GetClassLongPtrA));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    HICON i1 = U(LoadIconA)(NULL, IDI_APPLICATION), i2 = U(LoadIconA)(NULL, IDI_INFORMATION), i3 = U(LoadIconA)(NULL, IDI_ERROR);
    CHECK(i1 && i2 && i3 && i1 != i2 && i2 != i3);
    CHECK(U(SendMessageA)(h, WM_GETICON, ICON_BIG, 0) == 0);
    t_note("initial WM_GETICON(ICON_SMALL)=%p", (void *)U(SendMessageA)(h, WM_GETICON, ICON_SMALL, 0));
    CHECK(U(SendMessageA)(h, WM_SETICON, ICON_BIG, (LPARAM)i1) == 0);
    CHECK((HICON)U(SendMessageA)(h, WM_GETICON, ICON_BIG, 0) == i1);
    t_note("WM_GETICON(ICON_SMALL) after big set: %p", (void *)U(SendMessageA)(h, WM_GETICON, ICON_SMALL, 0));
    t_note("WM_SETICON(ICON_SMALL) prev: %p", (void *)U(SendMessageA)(h, WM_SETICON, ICON_SMALL, (LPARAM)i2));
    CHECK((HICON)U(SendMessageA)(h, WM_GETICON, ICON_SMALL, 0) == i2);
    CHECK((HICON)U(SendMessageA)(h, WM_GETICON, ICON_BIG, 0) == i1);          /* independent */
    CHECK((HICON)U(SendMessageA)(h, WM_SETICON, ICON_BIG, (LPARAM)i3) == i1);   /* returns previous */
    CHECK((HICON)U(SendMessageA)(h, WM_GETICON, ICON_BIG, 0) == i3);
    CHECK((HICON)U(SendMessageA)(h, WM_SETICON, ICON_SMALL, 0) == i2);
    t_note("after clearing, WM_GETICON(ICON_SMALL)=%p", (void *)U(SendMessageA)(h, WM_GETICON, ICON_SMALL, 0));
    CHECK((HICON)U(SendMessageA)(h, WM_SETICON, ICON_BIG, 0) == i3);
    CHECK(U(SendMessageA)(h, WM_GETICON, ICON_BIG, 0) == 0);
    CHECK(U(GetClassLongPtrA)(h, GCLP_HICON) == 0);
    mu_kill(h);
}
TEST(misc_ui_def_geticon_class_fallback) {
    NEED_DP(); REQUIRE(U(LoadIconA)); REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    HICON big = U(LoadIconA)(NULL, IDI_QUESTION);
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = mu_proc; wc.hInstance = GetModuleHandleA(NULL);
    wc.hIcon = big; wc.lpszClassName = "MU_IconCls";
    CHECK(U(RegisterClassExA)(&wc) != 0);
    HWND h = mu_mk_cls("MU_IconCls", "i", WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    if (h) {
        HICON got = (HICON)U(SendMessageA)(h, WM_GETICON, ICON_BIG, 0);
        t_note("class icon fallback for ICON_BIG: %d", got == big);
        HICON sm = (HICON)U(SendMessageA)(h, WM_GETICON, ICON_SMALL, 0);
        t_note("class has hIcon only: WM_GETICON(ICON_SMALL)=%p ICON_SMALL2=%p", (void *)sm, (void *)U(SendMessageA)(h, WM_GETICON, 2, 0));
        CHECK(U(GetClassLongPtrA)(h, GCLP_HICON) == (ULONG_PTR)big);
        mu_kill(h);
    } else CHECK(0);
    U(UnregisterClassA)("MU_IconCls", GetModuleHandleA(NULL));
}
TEST(misc_ui_def_seticon_bad_type) {
    NEED_DP(); REQUIRE(U(LoadIconA));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    HICON i1 = U(LoadIconA)(NULL, IDI_APPLICATION);
    WPARAM types[] = { 3, 4, 100, 0xFFFF, (WPARAM)-1 };
    for (int k = 0; k < 5; k++) {
        LRESULT r = U(SendMessageA)(h, WM_SETICON, types[k], (LPARAM)i1);
        LRESULT g = U(SendMessageA)(h, WM_GETICON, types[k], 0);
        t_note("WM_GETICON type %llx -> %llx", (unsigned long long)types[k], (long long)g);
        t_note("WM_SETICON type %llx -> %llx", (unsigned long long)types[k], (long long)r);
    }
    CHECK(U(SendMessageA)(h, WM_GETICON, ICON_BIG, 0) == 0);
    mu_kill(h);
}
static HWND g_ctl_parent;
static int g_setcursor_n; static WPARAM g_setcursor_w; static LPARAM g_setcursor_l; static LRESULT g_setcursor_ret;
static int hook_setcursor(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT *r)
{
    if (m == WM_SETCURSOR && h == g_ctl_parent) { g_setcursor_n++; g_setcursor_w = w; g_setcursor_l = l; *r = g_setcursor_ret; return 1; }
    return 0;
}
TEST(misc_ui_def_setcursor_forwarded_to_parent) {
    NEED_DP(); REQUIRE(U(GetCursor));
    HWND p = mu_mk(WS_POPUP, 0, 100, 100, 200, 200, NULL);
    HWND c = p ? mu_child(p, WS_VISIBLE) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    HCURSOR saved = U(GetCursor)();
    g_ctl_parent = p; g_setcursor_n = 0; g_setcursor_ret = 0;
    mu_hook = hook_setcursor;
    LPARAM lp = MAKELPARAM(HTCLIENT, WM_MOUSEMOVE);
    LRESULT r = U(SendMessageA)(c, WM_SETCURSOR, (WPARAM)c, lp);
    CHECK(g_setcursor_n == 1);
    CHECK(g_setcursor_w == (WPARAM)c);
    CHECK(g_setcursor_l == lp);
    t_note("child WM_SETCURSOR with parent returning FALSE -> %lld", (long long)r);
    g_setcursor_ret = TRUE; g_setcursor_n = 0;
    r = U(SendMessageA)(c, WM_SETCURSOR, (WPARAM)c, lp);
    CHECK(g_setcursor_n == 1);
    CHECK(r == TRUE);                       /* parent halts processing */
    mu_hook = NULL;
    /* top-level with no parent: returns FALSE for client area with no class cursor */
    r = U(SendMessageA)(p, WM_SETCURSOR, (WPARAM)p, lp);
    t_note("top-level WM_SETCURSOR(HTCLIENT, no class cursor) -> %lld", (long long)r);
    U(SetCursor)(saved);
    mu_kill(p);
}
TEST(misc_ui_def_contextmenu_forwarded_to_parent) {
    NEED_DP();
    static HWND gp, gc; static int n; static WPARAM w; static LPARAM l;
    int hk(HWND h, UINT m, WPARAM ww, LPARAM ll, LRESULT *r) { if (m == WM_CONTEXTMENU && h == gp) { n++; w = ww; l = ll; *r = 0; return 1; } return 0; }
    HWND p = mu_mk(WS_POPUP, 0, 100, 100, 200, 200, NULL);
    HWND c = p ? mu_child(p, WS_VISIBLE) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    gp = p; gc = c; n = 0;
    mu_hook = hk;
    LPARAM pos = MAKELPARAM(123, 456);
    U(SendMessageA)(c, WM_CONTEXTMENU, (WPARAM)c, pos);
    CHECK(n == 1);
    CHECK(w == (WPARAM)c);
    CHECK(l == pos);
    HWND g = mu_child(c, 0);
    n = 0;
    U(SendMessageA)(g, WM_CONTEXTMENU, (WPARAM)g, pos);
    CHECK(n == 1);                          /* travels up through two levels */
    t_note("2-level context menu forward: wParam is %s", w == (WPARAM)g ? "original window" : w == (WPARAM)c ? "intermediate window" : "other");
    mu_hook = NULL;
    mu_kill(p);
}
TEST(misc_ui_def_getobject_unknown) {
    NEED_DP();
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    LONG ids[] = { 0x12345, 0x7FFFFFF0, 0x1000, 99 };
    for (int i = 0; i < 4; i++) {
        LRESULT r = U(SendMessageA)(h, WM_GETOBJECT, 0, ids[i]);
        CHECKF(r == 0, "objid %lx -> %llx", ids[i], (long long)r);
    }
    LRESULT r = U(SendMessageA)(h, WM_GETOBJECT, 0, (LPARAM)OBJID_CLIENT);
    t_note("WM_GETOBJECT(OBJID_CLIENT) default -> %llx", (long long)r);
    mu_kill(h);
}
TEST(misc_ui_def_erasebkgnd_class_brush) {
    NEED_DP(); REQUIRE(U(GetSysColor));
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HDC sdc = GetDC(NULL);
    HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 32, 32);
    HGDIOBJ old = SelectObject(m, bm);
    PatBlt(m, 0, 0, 32, 32, BLACKNESS);
    LRESULT r = U(SendMessageA)(h, WM_ERASEBKGND, (WPARAM)m, 0);
    CHECK(r != 0);
    COLORREF px = GetPixel(m, 4, 4);
    CHECKF(px == U(GetSysColor)(COLOR_BTNFACE), "pixel %06lx want %06lx", px, U(GetSysColor)(COLOR_BTNFACE));
    CHECK(GetPixel(m, 31, 31) == U(GetSysColor)(COLOR_BTNFACE));
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(h);
}
TEST(misc_ui_def_erasebkgnd_no_brush) {
    NEED_DP();
    HWND h = mu_mk_cls(MU_CLS_NB, "nb", WS_POPUP, 0, 0, 0, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HDC sdc = GetDC(NULL);
    HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 32, 32);
    HGDIOBJ old = SelectObject(m, bm);
    PatBlt(m, 0, 0, 32, 32, BLACKNESS);
    LRESULT r = U(SendMessageA)(h, WM_ERASEBKGND, (WPARAM)m, 0);
    CHECK(r == 0);
    CHECK(GetPixel(m, 4, 4) == RGB(0, 0, 0));
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(h);
}
TEST(misc_ui_def_erasebkgnd_custom_brush_and_clip) {
    NEED_DP(); REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    HBRUSH br = CreateSolidBrush(RGB(10, 20, 30));
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = mu_proc; wc.hInstance = GetModuleHandleA(NULL);
    wc.hbrBackground = br; wc.lpszClassName = "MU_BrushCls";
    CHECK(U(RegisterClassExA)(&wc) != 0);
    HWND h = mu_mk_cls("MU_BrushCls", "b", WS_POPUP, 0, 0, 0, 100, 100, NULL);
    if (h) {
        HDC sdc = GetDC(NULL);
        HDC m = CreateCompatibleDC(sdc);
        HBITMAP bm = CreateCompatibleBitmap(sdc, 32, 32);
        HGDIOBJ old = SelectObject(m, bm);
        PatBlt(m, 0, 0, 32, 32, WHITENESS);
        IntersectClipRect(m, 8, 8, 16, 16);
        CHECK(U(SendMessageA)(h, WM_ERASEBKGND, (WPARAM)m, 0) != 0);
        CHECK(GetPixel(m, 10, 10) == RGB(10, 20, 30));
        /* Win10 22H2 DefWindowProc WM_ERASEBKGND on a caller DC: the fill ignores the DC clip region; recorded. */
        t_note("erase with clip 8..16: pixel(2,2)=%06lx pixel(20,20)=%06lx", GetPixel(m, 2, 2), GetPixel(m, 20, 20));
        SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
        mu_kill(h);
    } else CHECK(0);
    U(UnregisterClassA)("MU_BrushCls", GetModuleHandleA(NULL));
    DeleteObject(br);
}
TEST(misc_ui_def_ctlcolor_brushes) {
    NEED_DP(); REQUIRE(U(GetSysColorBrush)); REQUIRE(U(GetSysColor));
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c = p ? mu_child(p, 0) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    HDC sdc = GetDC(NULL);
    HDC m = CreateCompatibleDC(sdc);
    static const struct { UINT msg; const char *n; } t[] = {
        { WM_CTLCOLORMSGBOX, "MSGBOX" }, { WM_CTLCOLOREDIT, "EDIT" }, { WM_CTLCOLORLISTBOX, "LISTBOX" }, { WM_CTLCOLORBTN, "BTN" },
        { WM_CTLCOLORDLG, "DLG" }, { WM_CTLCOLORSCROLLBAR, "SCROLLBAR" }, { WM_CTLCOLORSTATIC, "STATIC" } };
    for (int i = 0; i < 7; i++) {
        SetBkColor(m, RGB(1, 2, 3)); SetTextColor(m, RGB(4, 5, 6));
        HBRUSH b = (HBRUSH)U(SendMessageA)(p, t[i].msg, (WPARAM)m, (LPARAM)c);
        CHECKF(b != NULL && GetObjectType(b) == OBJ_BRUSH, "%s brush %p", t[i].n, b);
        HBRUSH s1 = U(GetSysColorBrush)(COLOR_WINDOW), s2 = U(GetSysColorBrush)(COLOR_BTNFACE), s3 = U(GetSysColorBrush)(COLOR_SCROLLBAR);
        const char *which = b == s1 ? "WINDOW" : b == s2 ? "BTNFACE" : b == s3 ? "SCROLLBAR" : "other";
        t_note("WM_CTLCOLOR%s default -> %s brush, bk=%06lx text=%06lx", t[i].n, which, GetBkColor(m), GetTextColor(m));
    }
    SetBkColor(m, RGB(1, 2, 3));
    HBRUSH b = (HBRUSH)U(SendMessageA)(p, WM_CTLCOLOREDIT, (WPARAM)m, (LPARAM)c);
    t_note("CTLCOLOREDIT brush==WINDOW %d bk==WINDOW %d text==WINDOWTEXT %d", b == U(GetSysColorBrush)(COLOR_WINDOW),
           GetBkColor(m) == U(GetSysColor)(COLOR_WINDOW), GetTextColor(m) == U(GetSysColor)(COLOR_WINDOWTEXT));
    DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(p);
}
TEST(misc_ui_def_print_and_printclient) {
    NEED_DP();
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HDC sdc = GetDC(NULL);
    HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 100, 100);
    HGDIOBJ old = SelectObject(m, bm);
    PatBlt(m, 0, 0, 100, 100, BLACKNESS);
    mu_log_reset(); mu_logging = 1;
    U(SendMessageA)(h, WM_PRINT, (WPARAM)m, PRF_CLIENT | PRF_ERASEBKGND);
    mu_logging = 0;
    int ie = mu_find_h(h, WM_ERASEBKGND), ic = mu_find_h(h, WM_PRINTCLIENT), ip = mu_find_h(h, WM_PAINT);
    CHECK(ie >= 0);
    CHECK(ic >= 0);
    if (ie >= 0) CHECK(mu_log[ie].w == (WPARAM)m);
    if (ic >= 0) { CHECK(mu_log[ic].w == (WPARAM)m); CHECK(mu_log[ic].l & PRF_CLIENT); }
    if (ie >= 0 && ic >= 0) CHECK(ie < ic);
    CHECK(GetPixel(m, 20, 20) == U(GetSysColor)(COLOR_BTNFACE));
    t_note("WM_PRINT(CLIENT|ERASEBKGND): WM_PAINT forwarded: %d", ip >= 0);
    /* only client flag: no erase */
    PatBlt(m, 0, 0, 100, 100, BLACKNESS);
    mu_log_reset(); mu_logging = 1;
    U(SendMessageA)(h, WM_PRINT, (WPARAM)m, PRF_CLIENT);
    mu_logging = 0;
    t_note("WM_PRINT(PRF_CLIENT): WM_ERASEBKGND count %d", mu_count_h(h, WM_ERASEBKGND));
    CHECK(mu_count_h(h, WM_PRINTCLIENT) == 1);
    /* no flags: nothing */
    mu_log_reset(); mu_logging = 1;
    U(SendMessageA)(h, WM_PRINT, (WPARAM)m, 0);
    mu_logging = 0;
    CHECK(mu_count_h(h, WM_PRINTCLIENT) == 0 && mu_count_h(h, WM_ERASEBKGND) == 0);
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(h);
}
TEST(misc_ui_def_printclient_becomes_paint) {
    NEED_DP();
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HDC sdc = GetDC(NULL);
    HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 100, 100);
    HGDIOBJ old = SelectObject(m, bm);
    mu_log_reset(); mu_logging = 1;
    LRESULT r = U(SendMessageA)(h, WM_PRINTCLIENT, (WPARAM)m, PRF_CLIENT);
    mu_logging = 0;
    int ip = mu_find_h(h, WM_PAINT);
    t_note("WM_PRINTCLIENT default ret=%llx, produced WM_PAINT=%d (wParam==hdc: %d)", (long long)r, ip >= 0, ip >= 0 && mu_log[ip].w == (WPARAM)m);
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(h);
}
TEST(misc_ui_def_minmaxinfo_defaults) {
    NEED_DP(); REQUIRE(U(GetSystemMetrics));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 100, 100, 300, 200, NULL);
    if (!h) { CHECK(0); return; }
    MINMAXINFO mi; memset(&mi, 0, sizeof mi);
    CHECK(U(SendMessageA)(h, WM_GETMINMAXINFO, 0, (LPARAM)&mi) == 0);
    t_note("DefWindowProc fills MINMAXINFO: mintrack==SM_*MINTRACK: %d", mi.ptMinTrackSize.x == U(GetSystemMetrics)(SM_CXMINTRACK) && mi.ptMinTrackSize.y == U(GetSystemMetrics)(SM_CYMINTRACK));
    t_note("MINMAXINFO: maxsize %ldx%ld maxpos %ld,%ld mintrack %ldx%ld maxtrack %ldx%ld", mi.ptMaxSize.x, mi.ptMaxSize.y, mi.ptMaxPosition.x, mi.ptMaxPosition.y,
           mi.ptMinTrackSize.x, mi.ptMinTrackSize.y, mi.ptMaxTrackSize.x, mi.ptMaxTrackSize.y);
    mu_kill(h);
}
TEST(misc_ui_def_sethotkey_roundtrip) {
    NEED_DP();
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    CHECK(U(SendMessageA)(h, WM_GETHOTKEY, 0, 0) == 0);
    WPARAM hk = MAKEWORD(VK_F24, HOTKEYF_CONTROL | HOTKEYF_ALT | HOTKEYF_SHIFT);
    LRESULT r = U(SendMessageA)(h, WM_SETHOTKEY, hk, 0);
    t_note("WM_SETHOTKEY -> %lld", (long long)r);
    if (r == 1) CHECK(U(SendMessageA)(h, WM_GETHOTKEY, 0, 0) == (LRESULT)hk);
    U(SendMessageA)(h, WM_SETHOTKEY, 0, 0);
    CHECK(U(SendMessageA)(h, WM_GETHOTKEY, 0, 0) == 0);
    mu_kill(h);
}
TEST(misc_ui_create_wm_create_minus1_fails) {
    NEED_DP();
    int hk(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT *r) { if (m == WM_CREATE) { *r = -1; return 1; } return 0; }
    mu_hook = hk;
    mu_log_reset(); mu_logging = 1;
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    mu_logging = 0;
    mu_hook = NULL;
    CHECK(h == NULL);
    CHECK(mu_count(WM_DESTROY) == 0 || mu_count(WM_DESTROY) == 1);
    t_note("WM_CREATE=-1: WM_DESTROY seen %d, WM_NCDESTROY seen %d", mu_count(WM_DESTROY), mu_count(WM_NCDESTROY));
    if (h) mu_kill(h);
}
TEST(misc_ui_create_wm_nccreate_false_fails) {
    NEED_DP();
    int hk(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT *r) { if (m == WM_NCCREATE) { *r = FALSE; return 1; } return 0; }
    mu_hook = hk;
    mu_log_reset(); mu_logging = 1;
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    mu_logging = 0;
    mu_hook = NULL;
    CHECK(h == NULL);
    CHECK(mu_count(WM_CREATE) == 0);
    t_note("WM_NCCREATE=FALSE: WM_NCDESTROY seen %d", mu_count(WM_NCDESTROY));
    if (h) mu_kill(h);
}
TEST(misc_ui_create_message_order) {
    NEED_DP();
    mu_log_reset(); mu_logging = 1;
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, 0, 10, 10, 200, 100, NULL);
    mu_logging = 0;
    if (!h) { CHECK(0); return; }
    UINT want[] = { WM_GETMINMAXINFO, WM_NCCREATE, WM_NCCALCSIZE, WM_CREATE };
    int pos[4];
    for (int i = 0; i < 4; i++) pos[i] = mu_find_h(h, want[i]);
    if (pos[0] < 0) pos[0] = mu_find(WM_GETMINMAXINFO);
    CHECK(pos[1] >= 0 && pos[2] >= 0 && pos[3] >= 0);
    CHECK(pos[1] < pos[2] && pos[2] < pos[3]);
    int first = -1;
    for (int i = 0; i < mu_nlog; i++) if (mu_log[i].h == h) { first = i; break; }
    CHECK(first >= 0 && (mu_log[first].msg == WM_GETMINMAXINFO || mu_log[first].msg == WM_NCCREATE));
    mu_kill(h);
}
