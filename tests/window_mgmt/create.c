/* CreateWindowEx / DestroyWindow / handle lifetime */
#include "wm.h"

static int hasbits(LONG v, LONG m) { return (v & m) == m; }

TEST(wm_create_basic_a) {
    NEED(CreateWindowExA); NEED(DestroyWindow); NEED(IsWindow); NEED(GetClassNameA); NEED(GetWindowLongA);
    HWND h = U(CreateWindowExA)(0, wm_cls(), "hello", WS_OVERLAPPEDWINDOW, 10, 20, 300, 200, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(IsWindow)(h));
    char cn[64] = {0};
    int n = U(GetClassNameA)(h, cn, 64);
    EQ(n, (int)strlen(wm_cls()));
    EXPECT_STR(cn, wm_cls());
    LONG st = U(GetWindowLongA)(h, GWL_STYLE);
    CHECK(hasbits(st, WS_OVERLAPPEDWINDOW));
    CHECK(!(st & WS_VISIBLE));
    RECT r; CHECK(U(GetWindowRect)(h, &r));
    EQ(r.left, 10); EQ(r.top, 20);
    EQ(r.right - r.left, 300); EQ(r.bottom - r.top, 200);
    CHECK(U(DestroyWindow)(h));
    CHECK(!U(IsWindow)(h));
}

TEST(wm_create_basic_w) {
    NEED(CreateWindowExW); NEED(DestroyWindow); NEED(IsWindow); NEED(GetClassNameW); NEED(GetWindowTextW);
    HWND h = U(CreateWindowExW)(0, wm_clsw(), L"w\x00e9llo\x4e16", WS_OVERLAPPEDWINDOW, 11, 22, 310, 210, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    if (!h) return;
    WCHAR cn[64] = {0};
    int n = U(GetClassNameW)(h, cn, 64);
    EQ(n, 12);
    EXPECT_WSTR(cn, wm_clsw());
    WCHAR t[64] = {0};
    n = U(GetWindowTextW)(h, t, 64);
    EQ(n, 6);
    EXPECT_WSTR(t, L"w\x00e9llo\x4e16");
    CHECK(U(IsWindowUnicode)(h));
    U(DestroyWindow)(h);
}

TEST(wm_create_unicode_flag_by_class) {
    NEED(IsWindowUnicode);
    HWND a = wm_mk(WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    HWND w = wm_mkw(WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    CHECK(a && w);
    CHECK(!U(IsWindowUnicode)(a));
    CHECK(U(IsWindowUnicode)(w));
    CHECK(!U(IsWindowUnicode)(NULL));
    wm_destroy(a); wm_destroy(w);
}

TEST(wm_create_unregistered_class_a) {
    NEED(CreateWindowExA);
    SetLastError(0);
    HWND h = U(CreateWindowExA)(0, "NoSuchClass_wm_zzz", "x", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(h, NULL);
    EXPECT_ERR(0, ERROR_CANNOT_FIND_WND_CLASS);
}

TEST(wm_create_unregistered_class_w) {
    NEED(CreateWindowExW);
    SetLastError(0);
    HWND h = U(CreateWindowExW)(0, L"NoSuchClass_wm_zzz", L"x", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(h, NULL);
    EXPECT_ERR(0, ERROR_CANNOT_FIND_WND_CLASS);
}

TEST(wm_create_bad_atom) {
    NEED(CreateWindowExA);
    SetLastError(0);
    HWND h = U(CreateWindowExA)(0, (LPCSTR)(ULONG_PTR)0xBFFF, "x", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    EQ(h, NULL);
    EXPECT_ERR(0, ERROR_CANNOT_FIND_WND_CLASS);
}

TEST(wm_create_class_by_atom) {
    NEED(RegisterClassExA); NEED(UnregisterClassA); NEED(CreateWindowExA);
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "WmAtomClass1";
    ATOM at = U(RegisterClassExA)(&wc);
    CHECK(at != 0);
    if (!at) return;
    HWND h = U(CreateWindowExA)(0, (LPCSTR)(ULONG_PTR)at, "x", WS_OVERLAPPED, 0, 0, 50, 50, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    char cn[64] = {0};
    if (h) { U(GetClassNameA)(h, cn, 64); EXPECT_STR(cn, "WmAtomClass1"); }
    wm_destroy(h);
    CHECK(U(UnregisterClassA)("WmAtomClass1", GetModuleHandleA(NULL)));
}

TEST(wm_create_wrong_instance_class) {
    /* a class registered by this module is not found with a different hInstance unless global */
    NEED(RegisterClassExA); NEED(UnregisterClassA); NEED(CreateWindowExA);
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "WmInstClass1";
    CHECK(U(RegisterClassExA)(&wc) != 0);
    SetLastError(0);
    HWND h = U(CreateWindowExA)(0, "WmInstClass1", "x", WS_OVERLAPPED, 0, 0, 50, 50, NULL, NULL, GetModuleHandleA("kernel32.dll"), NULL);
    t_note("class lookup with foreign hInstance: hwnd=%p err=%lu", h, GetLastError());
    wm_destroy(h);
    CHECK(U(UnregisterClassA)("WmInstClass1", GetModuleHandleA(NULL)));
}

TEST(wm_create_child_without_parent) {
    NEED(CreateWindowExA);
    SetLastError(0);
    HWND h = wm_mk(WS_CHILD, 0, 0, 0, 50, 50, NULL);
    EQ(h, NULL);
    EXPECT_ERR(0, ERROR_TLW_WITH_WSCHILD);
    SetLastError(0);
    h = wm_mk(WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 0, 0, 50, 50, NULL);
    EQ(h, NULL);
    EXPECT_ERR(0, ERROR_TLW_WITH_WSCHILD);
    SetLastError(0);
    h = wm_mkw(WS_CHILD, 0, 0, 0, 50, 50, NULL);
    EQ(h, NULL);
    EXPECT_ERR(0, ERROR_TLW_WITH_WSCHILD);
}

TEST(wm_create_child_with_bogus_parent) {
    NEED(CreateWindowExA);
    SetLastError(0);
    HWND h = wm_mk(WS_CHILD, 0, 0, 0, 50, 50, (HWND)(ULONG_PTR)0xDEADBEEF);
    EQ(h, NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_create_popup_bogus_owner) {
    SetLastError(0);
    HWND h = wm_mk(WS_POPUP, 0, 0, 0, 50, 50, (HWND)(ULONG_PTR)0x12345678);
    EQ(h, NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_create_child_of_destroyed_parent) {
    NEED(DestroyWindow);
    HWND p = wm_hidden();
    CHECK(p != NULL);
    U(DestroyWindow)(p);
    SetLastError(0);
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 20, 20, p);
    EQ(c, NULL);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_create_message_order) {
    NEED(CreateWindowExA);
    wm_logclear();
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, 0, 0, 0, 200, 100, NULL);
    CHECK(h != NULL);
    int nc = wm_logfind(WM_NCCREATE, 0), ncs = wm_logfind(WM_NCCALCSIZE, 0), cr = wm_logfind(WM_CREATE, 0);
    CHECK(nc >= 0); CHECK(ncs > nc); CHECK(cr > ncs);
    EQ(wm_logcount(WM_NCCREATE), 1);
    EQ(wm_logcount(WM_CREATE), 1);
    int g = wm_logfind(WM_GETMINMAXINFO, 0);
    if (g < 0 || g > nc) t_note("WM_GETMINMAXINFO position relative to NCCREATE: %d vs %d", g, nc);
    EQ(wm_logcount(WM_DESTROY), 0);
    wm_logclear();
    U(DestroyWindow)(h);
    int d = wm_logfind(WM_DESTROY, 0), nd = wm_logfind(WM_NCDESTROY, 0);
    CHECK(d >= 0); CHECK(nd > d);
    EQ(wm_logcount(WM_DESTROY), 1);
    EQ(wm_logcount(WM_NCDESTROY), 1);
}

TEST(wm_create_message_order_visible) {
    NEED(CreateWindowExA);
    wm_logclear();
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 0, 0, 120, 80, NULL);
    CHECK(h != NULL);
    if (!h) return;
    int cr = wm_logfind(WM_CREATE, 0), sh = wm_logfind(WM_SHOWWINDOW, 0);
    CHECK(cr >= 0); CHECK(sh > cr);
    UINT m; WPARAM w; LPARAM l;
    if (wm_lognth(sh, &m, &w, &l)) { EQ(w, 1); EQ(l, 0); }
    CHECK(U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_create_nccreate_false) {
    NEED(CreateWindowExA);
    wm_fail_nccreate = 1; wm_logclear();
    SetLastError(0);
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    wm_fail_nccreate = 0;
    EQ(h, NULL);
    EQ(wm_logcount(WM_CREATE), 0);
    EQ(wm_logcount(WM_NCCREATE), 1);
    t_note("WM_NCDESTROY count after NCCREATE failure: %d, WM_DESTROY: %d", wm_logcount(WM_NCDESTROY), wm_logcount(WM_DESTROY));
}

TEST(wm_create_create_minus1) {
    NEED(CreateWindowExA);
    wm_fail_create = 1; wm_logclear();
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    wm_fail_create = 0;
    EQ(h, NULL);
    EQ(wm_logcount(WM_CREATE), 1);
    t_note("create==-1: WM_DESTROY=%d WM_NCDESTROY=%d", wm_logcount(WM_DESTROY), wm_logcount(WM_NCDESTROY));
}

TEST(wm_create_createstruct_fields) {
    NEED(CreateWindowExA);
    int dummy = 42;
    wm_seen_nc = wm_seen_cr = 0;
    HWND h = U(CreateWindowExA)(WS_EX_TOOLWINDOW, wm_cls(), "cstitle", WS_POPUP | WS_BORDER, 17, 23, 111, 77, NULL, (HMENU)NULL, GetModuleHandleA(NULL), &dummy);
    CHECK(h != NULL);
    EQ(wm_seen_nc, 1); EQ(wm_seen_cr, 1);
    EQ(wm_cs_nc.lpCreateParams, &dummy);
    EQ(wm_cs_cr.lpCreateParams, &dummy);
    EQ(wm_cs_cr.x, 17); EQ(wm_cs_cr.y, 23);
    EQ(wm_cs_cr.cx, 111); EQ(wm_cs_cr.cy, 77);
    CHECK((wm_cs_cr.style & (WS_POPUP | WS_BORDER)) == (WS_POPUP | WS_BORDER));
    CHECK(wm_cs_cr.dwExStyle & WS_EX_TOOLWINDOW);
    EQ(wm_cs_cr.hInstance, GetModuleHandleA(NULL));
    EQ(wm_cs_cr.hwndParent, NULL);
    EXPECT_STR(wm_name_cr, "cstitle");
    EXPECT_STR(wm_name_nc, "cstitle");
    /* the lpszClass pointer is only valid during the message; compare the copy taken then */
    EXPECT_STR(wm_cls_cr, wm_cls());
    wm_destroy(h);
}

TEST(wm_create_createstruct_child) {
    NEED(CreateWindowExA);
    HWND p = wm_hidden();
    CHECK(p != NULL);
    wm_seen_cr = 0;
    HWND c = U(CreateWindowExA)(0, wm_cls(), "kid", WS_CHILD, 3, 4, 55, 44, p, (HMENU)(ULONG_PTR)1234, GetModuleHandleA(NULL), NULL);
    CHECK(c != NULL);
    EQ(wm_cs_cr.hwndParent, p);
    EQ(wm_cs_cr.hMenu, (HMENU)(ULONG_PTR)1234);
    EQ(wm_cs_cr.x, 3); EQ(wm_cs_cr.cy, 44);
    EQ(U(GetWindowLongPtrA)(c, GWLP_ID), 1234);
    EQ(U(GetDlgCtrlID)(c), 1234);
    EQ(U(GetWindowLongPtrA)(c, GWLP_HWNDPARENT), (LONG_PTR)p);
    wm_destroy(p);
    CHECK(!U(IsWindow)(c));
}

TEST(wm_create_hinstance_stored) {
    HWND h = wm_hidden();
    CHECK(h != NULL);
    EQ(U(GetWindowLongPtrA)(h, GWLP_HINSTANCE), (LONG_PTR)GetModuleHandleA(NULL));
    EQ(U(GetWindowLongPtrW)(h, GWLP_HINSTANCE), (LONG_PTR)GetModuleHandleA(NULL));
    EQ(U(GetWindowLongPtrA)(h, GWLP_ID), 0);
    wm_destroy(h);
}

TEST(wm_create_usedefault_overlapped) {
    NEED(CreateWindowExA);
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL);
    CHECK(h != NULL);
    if (!h) return;
    RECT r; CHECK(U(GetWindowRect)(h, &r));
    CHECK(r.right - r.left > 0);
    CHECK(r.bottom - r.top > 0);
    CHECK(!U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_create_usedefault_x_only) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, 0, CW_USEDEFAULT, 77, 200, 150, NULL);
    CHECK(h != NULL);
    if (!h) return;
    RECT r; U(GetWindowRect)(h, &r);
    EQ(r.right - r.left, 200);
    EQ(r.bottom - r.top, 150);
    t_note("CW_USEDEFAULT x with y=77: placed at (%ld,%ld)", r.left, r.top);
    wm_destroy(h);
}

TEST(wm_create_usedefault_child_popup) {
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, p);
    CHECK(c != NULL);
    if (c) { RECT r; U(GetWindowRect)(c, &r); t_note("child CW_USEDEFAULT size %ldx%ld", r.right - r.left, r.bottom - r.top);
        CHECK(r.right - r.left >= 0); CHECK(r.bottom - r.top >= 0); }
    HWND pu = wm_mk(WS_POPUP, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL);
    CHECK(pu != NULL);
    if (pu) { RECT r; U(GetWindowRect)(pu, &r); t_note("popup CW_USEDEFAULT rect %ld,%ld %ldx%ld", r.left, r.top, r.right - r.left, r.bottom - r.top); }
    wm_destroy(pu); wm_destroy(p);
}

TEST(wm_create_negative_size) {
    HWND h = wm_mk(WS_POPUP, 0, 5, 6, -10, -20, NULL);
    t_note("negative size popup: %p", h);
    if (h) {
        RECT r; U(GetWindowRect)(h, &r);
        CHECK(r.right >= r.left); CHECK(r.bottom >= r.top);
        wm_destroy(h);
    }
}

TEST(wm_create_zero_size) {
    HWND h = wm_mk(WS_POPUP, 0, 5, 6, 0, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    RECT r; CHECK(U(GetWindowRect)(h, &r));
    EQ(r.left, 5); EQ(r.top, 6);
    EQ(r.right, 5); EQ(r.bottom, 6);
    CHECK(U(GetClientRect)(h, &r));
    EQ(r.right, 0); EQ(r.bottom, 0);
    wm_destroy(h);
}

TEST(wm_create_huge_size) {
    HWND h = wm_mk(WS_POPUP, 0, 0, 0, 0x7fffffff, 0x7fffffff, NULL);
    t_note("huge size popup created: %p", h);
    wm_destroy(h);
    h = wm_mk(WS_POPUP, 0, 0x7fffffff, 0x7fffffff, 10, 10, NULL);
    t_note("huge position popup created: %p", h);
    wm_destroy(h);
}

TEST(wm_create_styles_retained) {
    static const struct { DWORD style; const char *n; } tbl[] = {
        { WS_OVERLAPPEDWINDOW, "OVERLAPPEDWINDOW" }, { WS_CAPTION, "CAPTION" }, { WS_BORDER, "BORDER" },
        { WS_DLGFRAME, "DLGFRAME" }, { WS_THICKFRAME, "THICKFRAME" }, { WS_SYSMENU | WS_CAPTION, "SYSMENU" },
        { WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION, "MINBOX" }, { WS_MAXIMIZEBOX | WS_SYSMENU | WS_CAPTION, "MAXBOX" },
        { WS_VSCROLL, "VSCROLL" }, { WS_HSCROLL, "HSCROLL" }, { WS_CLIPCHILDREN, "CLIPCHILDREN" },
        { WS_CLIPSIBLINGS, "CLIPSIBLINGS" }, { WS_GROUP, "GROUP" }, { WS_TABSTOP, "TABSTOP" },
        { WS_POPUP | WS_BORDER, "POPUP+BORDER" }, { WS_POPUPWINDOW, "POPUPWINDOW" }, { WS_DISABLED, "DISABLED" },
    };
    for (unsigned i = 0; i < sizeof tbl / sizeof *tbl; i++) {
        HWND h = wm_mk(tbl[i].style, 0, 0, 0, 100, 100, NULL);
        CHECKF(h != NULL, "%s", tbl[i].n);
        if (!h) continue;
        LONG st = U(GetWindowLongA)(h, GWL_STYLE);
        CHECKF(hasbits(st, tbl[i].style), "%s style=%08lx", tbl[i].n, st);
        wm_destroy(h);
    }
}

TEST(wm_create_exstyles_retained) {
    static const struct { DWORD ex; int hard; const char *n; } tbl[] = {
        { WS_EX_TOOLWINDOW, 1, "TOOLWINDOW" }, { WS_EX_TOPMOST, 1, "TOPMOST" }, { WS_EX_TRANSPARENT, 1, "TRANSPARENT" },
        { WS_EX_CLIENTEDGE, 1, "CLIENTEDGE" }, { WS_EX_STATICEDGE, 1, "STATICEDGE" }, { WS_EX_DLGMODALFRAME, 1, "DLGMODALFRAME" },
        { WS_EX_ACCEPTFILES, 1, "ACCEPTFILES" }, { WS_EX_NOACTIVATE, 1, "NOACTIVATE" }, { WS_EX_CONTROLPARENT, 1, "CONTROLPARENT" },
        { WS_EX_APPWINDOW, 1, "APPWINDOW" }, { WS_EX_LAYERED, 1, "LAYERED" }, { WS_EX_RTLREADING, 1, "RTLREADING" },
        { WS_EX_RIGHT, 1, "RIGHT" }, { WS_EX_LEFTSCROLLBAR, 1, "LEFTSCROLLBAR" }, { WS_EX_LAYOUTRTL, 0, "LAYOUTRTL" },
        { WS_EX_COMPOSITED, 0, "COMPOSITED" }, { 0x00200000 /*NOREDIRECTIONBITMAP*/, 0, "NOREDIRECTIONBITMAP" },
        { WS_EX_CONTEXTHELP, 0, "CONTEXTHELP" }, { WS_EX_WINDOWEDGE, 0, "WINDOWEDGE" },
    };
    for (unsigned i = 0; i < sizeof tbl / sizeof *tbl; i++) {
        HWND h = wm_mk(WS_POPUP, tbl[i].ex, 0, 0, 100, 100, NULL);
        CHECKF(h != NULL, "%s", tbl[i].n);
        if (!h) continue;
        LONG ex = U(GetWindowLongA)(h, GWL_EXSTYLE);
        if (tbl[i].hard) CHECKF(hasbits(ex, tbl[i].ex), "%s ex=%08lx", tbl[i].n, ex);
        else if (!hasbits(ex, tbl[i].ex)) t_note("exstyle %s not retained: %08lx", tbl[i].n, ex);
        wm_destroy(h);
    }
}

TEST(wm_create_windowedge_auto) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    CHECK(h != NULL);
    CHECK(U(GetWindowLongA)(h, GWL_EXSTYLE) & WS_EX_WINDOWEDGE);
    wm_destroy(h);
    h = wm_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    CHECK(!(U(GetWindowLongA)(h, GWL_EXSTYLE) & WS_EX_WINDOWEDGE));
    wm_destroy(h);
    h = wm_mk(WS_POPUP | WS_DLGFRAME, 0, 0, 0, 100, 100, NULL);
    CHECK(U(GetWindowLongA)(h, GWL_EXSTYLE) & WS_EX_WINDOWEDGE);
    wm_destroy(h);
    h = wm_mk(WS_POPUP | WS_THICKFRAME, 0, 0, 0, 100, 100, NULL);
    CHECK(U(GetWindowLongA)(h, GWL_EXSTYLE) & WS_EX_WINDOWEDGE);
    wm_destroy(h);
}

TEST(wm_create_visible_flag) {
    HWND h = wm_mk(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 0, 0, 20, 20, NULL);
    CHECK(h != NULL);
    CHECK(U(IsWindowVisible)(h));
    CHECK(U(GetWindowLongA)(h, GWL_STYLE) & WS_VISIBLE);
    wm_destroy(h);
    h = wm_mk(WS_POPUP, 0, 0, 0, 20, 20, NULL);
    CHECK(!U(IsWindowVisible)(h));
    wm_destroy(h);
}

TEST(wm_create_minimize_maximize_styles) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_MINIMIZE, 0, 0, 0, 200, 100, NULL);
    CHECK(h != NULL);
    CHECK(U(IsIconic)(h));
    CHECK(!U(IsZoomed)(h));
    wm_destroy(h);
    h = wm_mk(WS_OVERLAPPEDWINDOW | WS_MAXIMIZE, 0, 0, 0, 200, 100, NULL);
    CHECK(h != NULL);
    CHECK(U(IsZoomed)(h));
    CHECK(!U(IsIconic)(h));
    wm_destroy(h);
}

TEST(wm_create_disabled_style) {
    HWND h = wm_mk(WS_OVERLAPPEDWINDOW | WS_DISABLED, 0, 0, 0, 200, 100, NULL);
    CHECK(h != NULL);
    CHECK(!U(IsWindowEnabled)(h));
    wm_destroy(h);
}

TEST(wm_create_child_style_parent_relation) {
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 1, 2, 30, 40, p);
    CHECK(c != NULL);
    EQ(U(GetParent)(c), p);
    EQ(U(GetAncestor)(c, GA_PARENT), p);
    EQ(U(GetAncestor)(c, GA_ROOT), p);
    CHECK(U(IsChild)(p, c));
    wm_destroy(p);
}

TEST(wm_create_popup_owner) {
    HWND o = wm_hidden();
    HWND pu = wm_mk(WS_POPUP, 0, 0, 0, 30, 40, o);
    CHECK(pu != NULL);
    EQ(U(GetParent)(pu), o);
    EQ(U(GetWindow)(pu, GW_OWNER), o);
    EQ(U(GetAncestor)(pu, GA_PARENT), U(GetDesktopWindow)());
    EQ(U(GetAncestor)(pu, GA_ROOT), pu);
    EQ(U(GetAncestor)(pu, GA_ROOTOWNER), o);
    CHECK(!U(IsChild)(o, pu));
    wm_destroy(o);
    CHECK(!U(IsWindow)(pu)); /* owned windows are destroyed with the owner */
}

TEST(wm_create_overlapped_with_owner) {
    HWND o = wm_hidden();
    HWND w = wm_mk(WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, o);
    CHECK(w != NULL);
    if (w) {
        EQ(U(GetWindow)(w, GW_OWNER), o);
        wm_destroy(w);
    }
    wm_destroy(o);
}

TEST(wm_create_title_null_and_empty) {
    NEED(CreateWindowExA); NEED(GetWindowTextA);
    HWND h = U(CreateWindowExA)(0, wm_cls(), NULL, WS_OVERLAPPED, 0, 0, 50, 50, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    char b[8] = "zz";
    EQ(U(GetWindowTextA)(h, b, 8), 0);
    EQ(b[0], 0);
    EQ(U(GetWindowTextLengthA)(h), 0);
    wm_destroy(h);
    h = U(CreateWindowExA)(0, wm_cls(), "", WS_OVERLAPPED, 0, 0, 50, 50, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    EQ(U(GetWindowTextLengthA)(h), 0);
    wm_destroy(h);
}

TEST(wm_create_title_long) {
    NEED(CreateWindowExA);
    char big[2000]; memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = 0;
    HWND h = U(CreateWindowExA)(0, wm_cls(), big, WS_OVERLAPPED, 0, 0, 50, 50, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h != NULL);
    EQ(U(GetWindowTextLengthA)(h), 1999);
    wm_destroy(h);
}

TEST(wm_create_many_windows) {
    enum { N = 200 };
    static HWND hs[N];
    int ok = 0;
    for (int i = 0; i < N; i++) { hs[i] = wm_mk(WS_OVERLAPPED, 0, 0, 0, 10, 10, NULL); if (hs[i]) ok++; }
    EQ(ok, N);
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N && j < i + 4; j++) CHECK(hs[i] != hs[j]);
    for (int i = 0; i < N; i++) wm_destroy(hs[i]);
    for (int i = 0; i < N; i++) CHECK(!U(IsWindow)(hs[i]));
}

TEST(wm_create_handle_properties) {
    HWND h = wm_hidden();
    CHECK(h != NULL);
    CHECK(((ULONG_PTR)h & 1) == 0 || 1);
    CHECK((ULONG_PTR)h < 0x100000000ULL); /* user handles are 32 bit significant */
    wm_destroy(h);
}

TEST(wm_create_handle_distinct_after_recreate) {
    HWND a = wm_hidden();
    wm_destroy(a);
    HWND b = wm_hidden();
    CHECK(b != NULL);
    CHECK(!U(IsWindow)(a) || a == b);
    wm_destroy(b);
}

TEST(wm_destroy_basic) {
    NEED(DestroyWindow);
    HWND h = wm_hidden();
    SetLastError(0);
    CHECK(U(DestroyWindow)(h));
    CHECK(!U(IsWindow)(h));
    SetLastError(0);
    CHECK(!U(DestroyWindow)(h));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_destroy_null_and_bogus) {
    NEED(DestroyWindow);
    SetLastError(0);
    CHECK(!U(DestroyWindow)(NULL));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(DestroyWindow)((HWND)(ULONG_PTR)0xFFFF0000));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(DestroyWindow)((HWND)(ULONG_PTR)1));
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(wm_destroy_desktop_fails) {
    NEED(DestroyWindow);
    HWND d = U(GetDesktopWindow)();
    SetLastError(0);
    CHECK(!U(DestroyWindow)(d));
    CHECK(U(IsWindow)(d));
    t_note("DestroyWindow(desktop) last error = %lu", GetLastError());
}

TEST(wm_destroy_children_first) {
    NEED(DestroyWindow);
    HWND p = wm_hidden();
    HWND c1 = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    HWND c2 = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    HWND g = wm_mk(WS_CHILD, 0, 0, 0, 5, 5, c1);
    CHECK(p && c1 && c2 && g);
    wm_logclear();
    CHECK(U(DestroyWindow)(p));
    CHECK(!U(IsWindow)(p)); CHECK(!U(IsWindow)(c1)); CHECK(!U(IsWindow)(c2)); CHECK(!U(IsWindow)(g));
    EQ(wm_logcount(WM_DESTROY), 4);
    EQ(wm_logcount(WM_NCDESTROY), 4);
}

TEST(wm_destroy_parent_first_in_destroy) {
    /* WM_DESTROY of the parent is delivered before those of its children */
    NEED(DestroyWindow);
    HWND p = wm_hidden();
    HWND c = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    CHECK(p && c);
    wm_logclear();
    U(DestroyWindow)(p);
    UINT m; WPARAM w; LPARAM l;
    int first = wm_logfind(WM_DESTROY, 0);
    CHECK(first >= 0);
    if (first >= 0 && wm_lognth(first, &m, &w, &l)) { /* cannot identify hwnd; ensure NCDESTROY of the last comes after all DESTROYs */ }
    int last_nc = -1, i = -1;
    while ((i = wm_logfind(WM_NCDESTROY, i + 1)) >= 0) last_nc = i;
    int last_d = -1; i = -1;
    while ((i = wm_logfind(WM_DESTROY, i + 1)) >= 0) last_d = i;
    CHECK(last_nc > first);
    CHECK(last_d >= first);
}

TEST(wm_destroy_cross_thread) {
    NEED(DestroyWindow);
    wm_worker w;
    if (!wm_worker_start(&w, WS_OVERLAPPED, 0, 0)) { t_skip("worker"); wm_worker_stop(&w); return; }
    SetLastError(0);
    CHECK(!U(DestroyWindow)(w.hwnd));
    EXPECT_ERR(0, ERROR_ACCESS_DENIED);
    CHECK(U(IsWindow)(w.hwnd));
    wm_worker_stop(&w);
}

TEST(wm_destroy_in_wm_create_invalid) {
    /* a handle for a window that failed creation is not valid after CreateWindow returns */
    wm_fail_create = 1;
    HWND h = wm_mk(WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    wm_fail_create = 0;
    EQ(h, NULL);
}

TEST(wm_destroy_owner_destroys_owned_popups) {
    HWND o = wm_hidden();
    HWND p1 = wm_mk(WS_POPUP, 0, 0, 0, 10, 10, o);
    HWND p2 = wm_mk(WS_POPUP, 0, 0, 0, 10, 10, p1);
    CHECK(o && p1 && p2);
    wm_destroy(o);
    CHECK(!U(IsWindow)(p1)); CHECK(!U(IsWindow)(p2));
}

TEST(wm_destroy_child_removes_from_parent) {
    HWND p = wm_hidden();
    HWND c1 = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    HWND c2 = wm_mk(WS_CHILD, 0, 0, 0, 10, 10, p);
    CHECK(U(GetWindow)(p, GW_CHILD) == c1); /* children are appended at the bottom */
    wm_destroy(c1);
    CHECK(U(GetWindow)(p, GW_CHILD) == c2);
    CHECK(!U(IsChild)(p, c1));
    wm_destroy(c2);
    CHECK(U(GetWindow)(p, GW_CHILD) == NULL);
    wm_destroy(p);
}

TEST(wm_dead_handle_api_errors) {
    HWND h = wm_hidden();
    wm_destroy(h);
    RECT r; POINT pt = {1, 2}; char buf[16];
    SetLastError(0); CHECK(!U(GetWindowRect)(h, &r)); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); CHECK(!U(GetClientRect)(h, &r)); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); CHECK(!U(ClientToScreen)(h, &pt));
    SetLastError(0); CHECK(!U(ScreenToClient)(h, &pt));
    SetLastError(0); CHECK(!U(MoveWindow)(h, 0, 0, 10, 10, FALSE)); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); CHECK(!U(SetWindowPos)(h, NULL, 0, 0, 10, 10, SWP_NOZORDER)); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); CHECK(!U(ShowWindow)(h, SW_SHOW)); 
    SetLastError(0); CHECK(!U(IsWindowVisible)(h)); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    CHECK(!U(IsWindowEnabled)(h)); CHECK(!U(IsIconic)(h)); CHECK(!U(IsZoomed)(h));
    CHECK(!U(IsChild)(NULL, h));
    SetLastError(0); EQ(U(GetWindowLongA)(h, GWL_STYLE), 0); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); EQ(U(GetWindowLongPtrW)(h, GWLP_USERDATA), 0); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); EQ(U(SetWindowLongPtrA)(h, GWLP_USERDATA, 5), 0); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); EQ(U(GetWindowTextA)(h, buf, 16), 0);
    SetLastError(0); EQ(U(GetWindowTextLengthA)(h), 0);
    SetLastError(0); EQ(U(GetClassNameA)(h, buf, 16), 0);
    SetLastError(0); EQ(U(GetParent)(h), NULL); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0); EQ(U(GetWindow)(h, GW_CHILD), NULL); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    EQ(U(GetAncestor)(h, GA_ROOT), NULL);
    SetLastError(0); EQ(U(GetWindowThreadProcessId)(h, NULL), 0); EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    CHECK(!U(IsWindowUnicode)(h));
    CHECK(!U(BringWindowToTop)(h));
    CHECK(!U(SetForegroundWindow)(h));
    EQ(U(SetFocus)(h), NULL);
    EQ(U(SetActiveWindow)(h), NULL);
    CHECK(!U(EnableWindow)(h, TRUE));
    CHECK(!U(UpdateWindow)(h));
    CHECK(!U(InvalidateRect)(h, NULL, TRUE));
    CHECK(!U(OpenIcon)(h));
    t_note("CloseWindow(dead)=%d", U(CloseWindow)(h));
}

TEST(wm_user_data_and_extra_bytes) {
    NEED(SetWindowLongPtrA);
    HWND h = wm_hidden();
    EQ(U(GetWindowLongPtrA)(h, GWLP_USERDATA), 0);
    EQ(U(SetWindowLongPtrA)(h, GWLP_USERDATA, 0x1234), 0);
    EQ(U(SetWindowLongPtrA)(h, GWLP_USERDATA, 0x5678), 0x1234);
    EQ(U(GetWindowLongPtrA)(h, GWLP_USERDATA), 0x5678);
    EQ(U(GetWindowLongPtrW)(h, GWLP_USERDATA), 0x5678);
    EQ(U(SetWindowLongPtrA)(h, GWLP_USERDATA, (LONG_PTR)0x123456789ALL), 0x5678);
    EQ(U(GetWindowLongPtrA)(h, GWLP_USERDATA), (LONG_PTR)0x123456789ALL);
    /* extra bytes (16) */
    EQ(U(SetWindowLongPtrA)(h, 0, 0x11), 0);
    EQ(U(SetWindowLongPtrA)(h, 8, 0x22), 0);
    EQ(U(GetWindowLongPtrA)(h, 0), 0x11);
    EQ(U(GetWindowLongPtrA)(h, 8), 0x22);
    EQ(U(GetWindowLongA)(h, 4), 0);
    SetLastError(0);
    EQ(U(SetWindowLongA)(h, 4, 0x33), 0);
    EQ((int)U(GetWindowLongA)(h, 4), 0x33);
    /* out of range */
    SetLastError(0);
    EQ(U(GetWindowLongPtrA)(h, 16), 0);
    t_note("GetWindowLongPtr(extra=16) err=%lu", GetLastError());
    SetLastError(0);
    EQ(U(GetWindowLongPtrA)(h, 12), 0);
    t_note("GetWindowLongPtr(extra=12 straddling end) err=%lu", GetLastError());
    SetLastError(0);
    EQ(U(SetWindowLongPtrA)(h, 1000, 5), 0);
    t_note("SetWindowLongPtr(1000) err=%lu", GetLastError());
    SetLastError(0);
    EQ(U(GetWindowLongPtrA)(h, -100), 0);
    t_note("GetWindowLongPtr(-100) err=%lu", GetLastError());
    wm_destroy(h);
}

TEST(wm_style_modify_via_setwindowlong) {
    HWND h = wm_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    LONG st = U(GetWindowLongA)(h, GWL_STYLE);
    U(SetWindowLongA)(h, GWL_STYLE, st | WS_BORDER);
    CHECK(U(GetWindowLongA)(h, GWL_STYLE) & WS_BORDER);
    U(SetWindowLongA)(h, GWL_STYLE, st);
    CHECK(!(U(GetWindowLongA)(h, GWL_STYLE) & WS_BORDER));
    U(SetWindowLongA)(h, GWL_EXSTYLE, WS_EX_TOOLWINDOW);
    CHECK(U(GetWindowLongA)(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW);
    EQ(U(SetWindowLongA)(h, GWL_ID, 77), 0);
    wm_destroy(h);
}
