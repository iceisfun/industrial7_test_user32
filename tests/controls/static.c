#include "ctl.h"

#ifndef SS_REALSIZECONTROL
#define SS_REALSIZECONTROL 0x40
#endif

static HWND mkst(HWND par, DWORD style, int id, const char *text)
{
    return c_ctl(par, "Static", text, style, id, 4, 4 + id, 120, 30);
}

static const struct { const char *name; DWORD type; } k_ss_types[] = {
    { "left", SS_LEFT }, { "center", SS_CENTER }, { "right", SS_RIGHT }, { "icon", SS_ICON },
    { "blackrect", SS_BLACKRECT }, { "grayrect", SS_GRAYRECT }, { "whiterect", SS_WHITERECT },
    { "blackframe", SS_BLACKFRAME }, { "grayframe", SS_GRAYFRAME }, { "whiteframe", SS_WHITEFRAME },
    { "simple", SS_SIMPLE }, { "leftnowordwrap", SS_LEFTNOWORDWRAP }, { "ownerdraw", SS_OWNERDRAW },
    { "bitmap", SS_BITMAP }, { "etchedhorz", SS_ETCHEDHORZ }, { "etchedvert", SS_ETCHEDVERT },
    { "etchedframe", SS_ETCHEDFRAME },
};
#define NSS ((int)(sizeof k_ss_types / sizeof k_ss_types[0]))

TEST(static_constants)
{
    EXPECT_EQ(SS_LEFT, 0); EXPECT_EQ(SS_CENTER, 1); EXPECT_EQ(SS_RIGHT, 2); EXPECT_EQ(SS_ICON, 3);
    EXPECT_EQ(SS_BLACKRECT, 4); EXPECT_EQ(SS_GRAYRECT, 5); EXPECT_EQ(SS_WHITERECT, 6);
    EXPECT_EQ(SS_BLACKFRAME, 7); EXPECT_EQ(SS_GRAYFRAME, 8); EXPECT_EQ(SS_WHITEFRAME, 9);
    EXPECT_EQ(SS_USERITEM, 10); EXPECT_EQ(SS_SIMPLE, 11); EXPECT_EQ(SS_LEFTNOWORDWRAP, 12);
    EXPECT_EQ(SS_OWNERDRAW, 13); EXPECT_EQ(SS_BITMAP, 14); EXPECT_EQ(SS_ENHMETAFILE, 15);
    EXPECT_EQ(SS_ETCHEDHORZ, 16); EXPECT_EQ(SS_ETCHEDVERT, 17); EXPECT_EQ(SS_ETCHEDFRAME, 18);
    EXPECT_EQ(SS_TYPEMASK, 0x1F); EXPECT_EQ(SS_NOPREFIX, 0x80); EXPECT_EQ(SS_NOTIFY, 0x100);
    EXPECT_EQ(SS_CENTERIMAGE, 0x200); EXPECT_EQ(SS_RIGHTJUST, 0x400); EXPECT_EQ(SS_REALSIZEIMAGE, 0x800);
    EXPECT_EQ(SS_SUNKEN, 0x1000); EXPECT_EQ(SS_EDITCONTROL, 0x2000); EXPECT_EQ(SS_ENDELLIPSIS, 0x4000);
    EXPECT_EQ(SS_PATHELLIPSIS, 0x8000); EXPECT_EQ(SS_WORDELLIPSIS, 0xC000); EXPECT_EQ(SS_ELLIPSISMASK, 0xC000);
    EXPECT_EQ(STM_SETICON, 0x170); EXPECT_EQ(STM_GETICON, 0x171);
    EXPECT_EQ(STM_SETIMAGE, 0x172); EXPECT_EQ(STM_GETIMAGE, 0x173);
    EXPECT_EQ(STN_CLICKED, 0); EXPECT_EQ(STN_DBLCLK, 1); EXPECT_EQ(STN_ENABLE, 2); EXPECT_EQ(STN_DISABLE, 3);
}

TEST(static_classname)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetClassNameA));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_LEFT, 1, "x");
    char a[32];
    CHECK(s != NULL);
    c_classname(s, a, sizeof a);
    EXPECT_STR(a, "Static");
    /* truncated class-name buffer */
    EXPECT_EQ(U(GetClassNameA)(s, a, 3), 2);
    EXPECT_STR(a, "St");
    EXPECT_EQ(U(GetClassNameA)(s, a, 7), 6);
    EXPECT_EQ(U(GetClassNameA)(s, a, 1), 0);
    U(DestroyWindow)(p);
}

TEST(static_style_type_bits)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 200);
    for (int i = 0; i < NSS; i++) {
        HWND s = mkst(p, k_ss_types[i].type, 10 + i, "t");
        CHECKF(s != NULL, "%s", k_ss_types[i].name);
        if (!s) continue;
        CHECKF((U(GetWindowLongA)(s, GWL_STYLE) & SS_TYPEMASK) == k_ss_types[i].type, "%s", k_ss_types[i].name);
    }
    U(DestroyWindow)(p);
}

TEST(static_dlgcode_is_static)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    for (int i = 0; i < NSS; i++) {
        HWND s = mkst(p, k_ss_types[i].type, 10 + i, "t");
        if (!s) continue;
        CHECKF((SA(s, WM_GETDLGCODE, 0, 0) & DLGC_STATIC) != 0, "%s", k_ss_types[i].name);
    }
    U(DestroyWindow)(p);
}

TEST(static_text_roundtrip)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_LEFT, 1, "Label");
    char buf[128];
    EXPECT_EQ(SA(s, WM_GETTEXTLENGTH, 0, 0), 5);
    EXPECT_EQ(SA(s, WM_GETTEXT, 128, buf), 5);
    EXPECT_STR(buf, "Label");
    EXPECT_EQ(SA(s, WM_SETTEXT, 0, "A longer label & more"), TRUE);
    EXPECT_EQ(SA(s, WM_GETTEXTLENGTH, 0, 0), 21);
    EXPECT_EQ(SA(s, WM_GETTEXT, 128, buf), 21);
    EXPECT_STR(buf, "A longer label & more");
    memset(buf, 'z', sizeof buf);
    EXPECT_EQ(SA(s, WM_GETTEXT, 22, buf), 21);
    memset(buf, 'z', sizeof buf);
    EXPECT_EQ(SA(s, WM_GETTEXT, 21, buf), 20);
    EXPECT_EQ(buf[20], 0);
    memset(buf, 'z', sizeof buf);
    EXPECT_EQ(SA(s, WM_GETTEXT, 1, buf), 0);
    EXPECT_EQ(buf[0], 0);
    EXPECT_EQ(SA(s, WM_GETTEXT, 0, buf), 0);
    EXPECT_EQ(SA(s, WM_SETTEXT, 0, ""), TRUE);
    EXPECT_EQ(SA(s, WM_GETTEXTLENGTH, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(static_text_long)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_LEFT, 1, "");
    static char big[20001], out[20002];
    memset(big, 'q', 20000);
    big[20000] = 0;
    EXPECT_EQ(SA(s, WM_SETTEXT, 0, big), TRUE);
    EXPECT_EQ(SA(s, WM_GETTEXTLENGTH, 0, 0), 20000);
    EXPECT_EQ(SA(s, WM_GETTEXT, 20002, out), 20000);
    CHECK(!strcmp(out, big));
    U(DestroyWindow)(p);
}

TEST(static_text_wide)
{
    REQUIRE(U(CreateWindowExW)); REQUIRE(U(GetWindowTextW)); REQUIRE(U(SetWindowTextW));
    HWND p = c_parent(300, 200);
    static const WCHAR u[] = { 'A', 0x0416, 0x4E2D, 0x1F600 >> 10, 'z', 0 };
    HWND s = c_ctlw(p, L"Static", u, SS_LEFT, 5, 0, 0, 100, 20);
    WCHAR w[32]; memset(w, 0, sizeof w);
    CHECK(s != NULL);
    EXPECT_EQ(U(GetWindowTextW)(s, w, 32), 5);
    CHECK(!lstrcmpW(w, u));
    CHECK(U(SetWindowTextW)(s, L"new"));
    EXPECT_EQ(SWm(s, WM_GETTEXTLENGTH, 0, 0), 3);
    char a[16];
    EXPECT_EQ(SA(s, WM_GETTEXT, 16, a), 3);
    EXPECT_STR(a, "new");
    U(DestroyWindow)(p);
}

TEST(static_nchittest)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowRect));
    HWND p = c_parent(300, 200);
    HWND a = mkst(p, SS_LEFT, 1, "plain");
    HWND b = mkst(p, SS_LEFT | SS_NOTIFY, 2, "notify");
    RECT r;
    U(GetWindowRect)(a, &r);
    EXPECT_EQ(SA(a, WM_NCHITTEST, 0, MAKELPARAM(r.left + 5, r.top + 5)), HTTRANSPARENT);
    U(GetWindowRect)(b, &r);
    EXPECT_EQ(SA(b, WM_NCHITTEST, 0, MAKELPARAM(r.left + 5, r.top + 5)), HTCLIENT);
    U(DestroyWindow)(p);
}

TEST(static_notify_clicked)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND a = mkst(p, SS_LEFT, 1, "plain");
    HWND b = mkst(p, SS_LEFT | SS_NOTIFY, 2, "notify");
    c_logclear();
    SA(a, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(3, 3));
    SA(a, WM_LBUTTONUP, 0, MAKELPARAM(3, 3));
    SA(a, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(3, 3));
    EXPECT_EQ(c_nmsg(WM_COMMAND), 0);
    c_logclear();
    SA(b, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(3, 3));
    SA(b, WM_LBUTTONUP, 0, MAKELPARAM(3, 3));
    EXPECT_EQ(c_ncmd(2, STN_CLICKED), 1);
    int i = c_first_cmd(2, STN_CLICKED);
    CHECK(i >= 0 && c_log[i].lp == (LPARAM)b);
    c_logclear();
    SA(b, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(3, 3));
    EXPECT_EQ(c_ncmd(2, STN_DBLCLK), 1);
    U(DestroyWindow)(p);
}

TEST(static_notify_enable_disable)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkst(p, SS_LEFT | SS_NOTIFY, 3, "notify");
    HWND a = mkst(p, SS_LEFT, 4, "plain");
    c_logclear();
    U(EnableWindow)(b, FALSE);
    EXPECT_EQ(c_ncmd(3, STN_DISABLE), 1);
    U(EnableWindow)(b, TRUE);
    EXPECT_EQ(c_ncmd(3, STN_ENABLE), 1);
    c_logclear();
    U(EnableWindow)(a, FALSE);
    U(EnableWindow)(a, TRUE);
    EXPECT_EQ(c_nmsg(WM_COMMAND), 0);
    U(DestroyWindow)(p);
}

TEST(static_icon_setget)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(LoadIconA));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_ICON, 6, "");
    HICON i1 = U(LoadIconA)(NULL, IDI_APPLICATION), i2 = U(LoadIconA)(NULL, IDI_WARNING);
    CHECK(i1 && i2);
    EXPECT_EQ(SA(s, STM_GETICON, 0, 0), 0);
    EXPECT_EQ(SA(s, STM_SETICON, i1, 0), 0);
    EXPECT_EQ(SA(s, STM_GETICON, 0, 0), i1);
    EXPECT_EQ(SA(s, STM_GETIMAGE, IMAGE_ICON, 0), i1);
    EXPECT_EQ(SA(s, STM_SETICON, i2, 0), i1);
    EXPECT_EQ(SA(s, STM_GETICON, 0, 0), i2);
    EXPECT_EQ(SA(s, STM_SETIMAGE, IMAGE_ICON, i1), i2);
    EXPECT_EQ(SA(s, STM_GETIMAGE, IMAGE_ICON, 0), i1);
    EXPECT_EQ(SA(s, STM_SETICON, NULL, 0), i1);
    EXPECT_EQ(SA(s, STM_GETICON, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(static_bitmap_setget)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_BITMAP, 7, "");
    HBITMAP b1 = CreateBitmap(10, 10, 1, 1, NULL), b2 = CreateBitmap(12, 12, 1, 1, NULL);
    EXPECT_EQ(SA(s, STM_GETIMAGE, IMAGE_BITMAP, 0), 0);
    EXPECT_EQ(SA(s, STM_SETIMAGE, IMAGE_BITMAP, b1), 0);
    EXPECT_EQ(SA(s, STM_GETIMAGE, IMAGE_BITMAP, 0), b1);
    EXPECT_EQ(SA(s, STM_SETIMAGE, IMAGE_BITMAP, b2), b1);
    EXPECT_EQ(SA(s, STM_GETIMAGE, IMAGE_BITMAP, 0), b2);
    EXPECT_EQ(SA(s, STM_SETIMAGE, IMAGE_BITMAP, NULL), b2);
    U(DestroyWindow)(p);
    DeleteObject(b1); DeleteObject(b2);
}

TEST(static_icon_wrong_image_type)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(LoadIconA));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_ICON, 8, "");
    HBITMAP b = CreateBitmap(10, 10, 1, 1, NULL);
    SetLastError(0);
    LRESULT r = SA(s, STM_SETIMAGE, IMAGE_BITMAP, b);
    t_note("SS_ICON STM_SETIMAGE(IMAGE_BITMAP) -> %llx err=%lu", (long long)r, GetLastError());
    t_note("SS_ICON STM_GETIMAGE(IMAGE_BITMAP) -> %llx", (long long)SA(s, STM_GETIMAGE, IMAGE_BITMAP, 0));
    SetLastError(0);
    r = SA(s, STM_SETIMAGE, 99, b);
    t_note("SS_ICON STM_SETIMAGE(99) -> %llx err=%lu", (long long)r, GetLastError());
    U(DestroyWindow)(p);
    DeleteObject(b);
}

TEST(static_ownerdraw_printclient)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND s = c_ctl(p, "Static", "od", SS_OWNERDRAW, 9, 2, 2, 70, 25);
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bm = CreateBitmap(200, 100, 1, 1, NULL);
    HGDIOBJ old = SelectObject(dc, bm);
    c_logclear();
    SA(s, WM_PRINTCLIENT, dc, PRF_CLIENT);
    int n = c_nmsg(WM_DRAWITEM);
    t_note("SS_OWNERDRAW WM_PRINTCLIENT produced %d WM_DRAWITEM", n);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_DRAWITEM) {
        EXPECT_EQ(c_log[i].di.CtlType, ODT_STATIC);
        EXPECT_EQ(c_log[i].di.CtlID, 9);
        EXPECT_EQ(c_log[i].di.hwndItem, s);
        EXPECT_EQ(c_log[i].di.rcItem.right, 70);
        EXPECT_EQ(c_log[i].di.rcItem.bottom, 25);
        break;
    }
    SelectObject(dc, old); DeleteObject(bm); DeleteDC(dc);
    U(DestroyWindow)(p);
}

TEST(static_font)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_LEFT, 11, "f");
    HFONT f = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    EXPECT_EQ(SA(s, WM_GETFONT, 0, 0), 0);
    SA(s, WM_SETFONT, f, FALSE);
    EXPECT_EQ(SA(s, WM_GETFONT, 0, 0), f);
    SA(s, WM_SETFONT, NULL, FALSE);
    EXPECT_EQ(SA(s, WM_GETFONT, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(static_ellipsis_style_preserved)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 200);
    DWORD el[] = { SS_ENDELLIPSIS, SS_PATHELLIPSIS, SS_WORDELLIPSIS };
    for (int i = 0; i < 3; i++) {
        HWND s = mkst(p, SS_LEFT | el[i], 20 + i, "C:\\a\\very\\long\\path\\name\\that\\does\\not\\fit\\file.txt");
        CHECK(s != NULL);
        if (!s) continue;
        CHECK((U(GetWindowLongA)(s, GWL_STYLE) & SS_ELLIPSISMASK) == el[i]);
        /* the control keeps the full text; ellipsis is paint-only */
        EXPECT_EQ(SA(s, WM_GETTEXTLENGTH, 0, 0), 51);
    }
    U(DestroyWindow)(p);
}

TEST(static_wm_setstyle_via_setwindowlong)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetWindowLongA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_LEFT, 30, "x");
    LONG st = U(GetWindowLongA)(s, GWL_STYLE);
    U(SetWindowLongA)(s, GWL_STYLE, (st & ~SS_TYPEMASK) | SS_CENTER);
    CHECK((U(GetWindowLongA)(s, GWL_STYLE) & SS_TYPEMASK) == SS_CENTER);
    CHECK(U(GetWindowLongA)(s, GWL_ID) == 30);
    U(SetWindowLongA)(s, GWL_ID, 77);
    CHECK(U(GetWindowLongA)(s, GWL_ID) == 77);
    U(DestroyWindow)(p);
}

TEST(static_destroyed)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(IsWindow));
    HWND p = c_parent(300, 200);
    HWND s = mkst(p, SS_LEFT, 31, "x");
    U(DestroyWindow)(s);
    CHECK(!U(IsWindow)(s));
    EXPECT_EQ(SA(s, WM_GETTEXTLENGTH, 0, 0), 0);
    EXPECT_EQ(SA(s, STM_GETICON, 0, 0), 0);
    char b[8] = "zzz";
    EXPECT_EQ(SA(s, WM_GETTEXT, 8, b), 0);
    U(DestroyWindow)(p);
}

/* ---- built-in class registry ---- */
static const char *k_builtin[] = {
    "Button", "Static", "Edit", "ListBox", "ComboBox", "ScrollBar", "ComboLBox", "MDIClient", "#32770", "#32768", "#32769", "#32771", "#32772", "IME"
};

TEST(controls_builtin_classes_getclassinfo)
{
    REQUIRE(U(GetClassInfoA)); REQUIRE(U(GetClassInfoExA)); REQUIRE(U(GetClassInfoW));
    for (unsigned i = 0; i < sizeof k_builtin / sizeof k_builtin[0]; i++) {
        WNDCLASSA wc; WNDCLASSEXA wx;
        memset(&wc, 0, sizeof wc); memset(&wx, 0, sizeof wx); wx.cbSize = sizeof wx;
        SetLastError(0);
        BOOL r = U(GetClassInfoA)(NULL, k_builtin[i], &wc);
        if (i < 9) CHECKF(r, "%s err=%lu", k_builtin[i], GetLastError());
        else t_note("GetClassInfo(%s) -> %d", k_builtin[i], r);
        if (r) CHECKF(wc.lpfnWndProc != NULL, "%s", k_builtin[i]);
        BOOL rx = U(GetClassInfoExA)(NULL, k_builtin[i], &wx);
        if (i < 9) CHECKF(rx, "%s", k_builtin[i]);
        if (rx) CHECK(wx.lpfnWndProc == wc.lpfnWndProc);
        if (r) t_note("class %s style=%08x cbCls=%d cbWnd=%d", k_builtin[i], wc.style, wc.cbClsExtra, wc.cbWndExtra);
    }
}

TEST(controls_classinfo_unknown_class)
{
    REQUIRE(U(GetClassInfoA)); REQUIRE(U(GetClassInfoExW));
    WNDCLASSA wc; WNDCLASSEXW wx;
    SetLastError(0);
    CHECK(!U(GetClassInfoA)(NULL, "NoSuchControlClassXyz", &wc));
    EXPECT_ERR(0, ERROR_CLASS_DOES_NOT_EXIST);
    memset(&wx, 0, sizeof wx); wx.cbSize = sizeof wx;
    SetLastError(0);
    CHECK(!U(GetClassInfoExW)(NULL, L"NoSuchControlClassXyz", &wx));
    EXPECT_ERR(0, ERROR_CLASS_DOES_NOT_EXIST);
}

TEST(controls_classinfo_case_insensitive)
{
    REQUIRE(U(GetClassInfoA));
    const char *v[] = { "button", "BUTTON", "BuTtOn", "static", "EDIT", "listbox", "COMBOBOX", "scrollbar" };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        WNDCLASSA wc;
        CHECKF(U(GetClassInfoA)(NULL, v[i], &wc), "%s", v[i]);
    }
}

TEST(controls_createwindow_unknown_class_fails)
{
    REQUIRE(U(CreateWindowExA));
    SetLastError(0);
    HWND h = U(CreateWindowExA)(0, "NoSuchControlClassXyz", "x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(h == NULL);
    EXPECT_ERR(0, ERROR_CANNOT_FIND_WND_CLASS);
}

TEST(controls_class_same_proc_a_w)
{
    REQUIRE(U(GetClassInfoA)); REQUIRE(U(GetClassInfoW));
    WNDCLASSA a; WNDCLASSW w;
    CHECK(U(GetClassInfoA)(NULL, "Edit", &a));
    CHECK(U(GetClassInfoW)(NULL, L"Edit", &w));
    /* different entry points for ANSI and Unicode callers are acceptable; just must be non-null */
    CHECK(a.lpfnWndProc && w.lpfnWndProc);
}
