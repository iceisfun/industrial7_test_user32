#include "ctl.h"

#ifndef BST_PUSHED
#define BST_PUSHED 4
#endif

static HWND mkbtn(HWND par, DWORD style, int id, const char *text)
{
    return c_ctl(par, "Button", text, style, id, 5, 5 + id * 2, 100, 24);
}

static struct { const char *name; DWORD style; DWORD dlgc_mask; DWORD dlgc_want; } k_btn_types[] = {
    { "push",        BS_PUSHBUTTON,      DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_BUTTON | DLGC_UNDEFPUSHBUTTON },
    { "defpush",     BS_DEFPUSHBUTTON,   DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_BUTTON | DLGC_DEFPUSHBUTTON },
    { "check",       BS_CHECKBOX,        DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_BUTTON },
    { "autocheck",   BS_AUTOCHECKBOX,    DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_BUTTON },
    { "3state",      BS_3STATE,          DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_BUTTON },
    { "auto3state",  BS_AUTO3STATE,      DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_BUTTON },
    { "radio",       BS_RADIOBUTTON,     DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_BUTTON | DLGC_RADIOBUTTON },
    { "autoradio",   BS_AUTORADIOBUTTON, DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_BUTTON | DLGC_RADIOBUTTON },
    { "groupbox",    BS_GROUPBOX,        DLGC_BUTTON | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, DLGC_STATIC },
    { "ownerdraw",   BS_OWNERDRAW,       DLGC_DEFPUSHBUTTON | DLGC_RADIOBUTTON | DLGC_STATIC, 0 },
};
#define NBTN ((int)(sizeof k_btn_types / sizeof k_btn_types[0]))

TEST(button_classname)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetClassNameA)); REQUIRE(U(GetClassNameW));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_PUSHBUTTON, 1, "x");
    char a[32]; WCHAR w[32];
    CHECK(b != NULL);
    c_classname(b, a, sizeof a);
    EXPECT_STR(a, "Button");
    memset(w, 0, sizeof w);
    CHECK(U(GetClassNameW)(b, w, 32) == 6);
    CHECK(!lstrcmpW(w, L"Button"));
    /* class lookup is case-insensitive */
    HWND b2 = c_ctl(p, "bUtTon", "y", BS_PUSHBUTTON, 2, 0, 0, 40, 20);
    CHECK(b2 != NULL);
    if (b2) { c_classname(b2, a, sizeof a); EXPECT_STR(a, "Button"); }
    U(DestroyWindow)(p);
}

TEST(button_style_type_bits)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 200);
    for (int i = 0; i < NBTN; i++) {
        HWND b = mkbtn(p, k_btn_types[i].style, 10 + i, "t");
        CHECKF(b != NULL, "type %s", k_btn_types[i].name);
        if (!b) continue;
        LONG st = U(GetWindowLongA)(b, GWL_STYLE);
        CHECKF((st & BS_TYPEMASK) == k_btn_types[i].style, "type %s style %08lx", k_btn_types[i].name, st);
        CHECKF((st & WS_CHILD) && (st & WS_VISIBLE), "%s", k_btn_types[i].name);
    }
    U(DestroyWindow)(p);
}

TEST(button_default_state_all_types)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    for (int i = 0; i < NBTN; i++) {
        HWND b = mkbtn(p, k_btn_types[i].style, 10 + i, "t");
        if (!b) { CHECKF(0, "create %s", k_btn_types[i].name); continue; }
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
        LRESULT st = SA(b, BM_GETSTATE, 0, 0);
        CHECKF((st & 3) == 0, "%s state %llx", k_btn_types[i].name, (long long)st);
        CHECKF(!(st & BST_PUSHED), "%s", k_btn_types[i].name);
        EXPECT_EQ(SA(b, BM_GETIMAGE, IMAGE_BITMAP, 0), 0);
        EXPECT_EQ(SA(b, BM_GETIMAGE, IMAGE_ICON, 0), 0);
    }
    U(DestroyWindow)(p);
}

TEST(button_getdlgcode_table)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    for (int i = 0; i < NBTN; i++) {
        HWND b = mkbtn(p, k_btn_types[i].style, 10 + i, "t");
        if (!b) { CHECKF(0, "create %s", k_btn_types[i].name); continue; }
        LRESULT c = SA(b, WM_GETDLGCODE, 0, 0);
        CHECKF(((DWORD)c & k_btn_types[i].dlgc_mask) == k_btn_types[i].dlgc_want,
               "%s dlgcode %llx want %lx", k_btn_types[i].name, (long long)c, k_btn_types[i].dlgc_want);
    }
    U(DestroyWindow)(p);
}

TEST(button_setcheck_checkbox)
{
    REQUIRE(U(CreateWindowExA));
    DWORD styles[] = { BS_CHECKBOX, BS_AUTOCHECKBOX, BS_3STATE, BS_AUTO3STATE, BS_RADIOBUTTON, BS_AUTORADIOBUTTON };
    HWND p = c_parent(300, 200);
    for (int i = 0; i < 6; i++) {
        HWND b = mkbtn(p, styles[i], 10 + i, "t");
        if (!b) { CHECKF(0, "create %d", i); continue; }
        EXPECT_EQ(SA(b, BM_SETCHECK, BST_CHECKED, 0), 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_CHECKED);
        CHECKF((SA(b, BM_GETSTATE, 0, 0) & 3) == BST_CHECKED, "style %d", i);
        EXPECT_EQ(SA(b, BM_SETCHECK, BST_UNCHECKED, 0), 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
        CHECKF((SA(b, BM_GETSTATE, 0, 0) & 3) == BST_UNCHECKED, "style %d", i);
        /* setting the same state twice is stable */
        SA(b, BM_SETCHECK, BST_CHECKED, 0); SA(b, BM_SETCHECK, BST_CHECKED, 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_CHECKED);
    }
    U(DestroyWindow)(p);
}

TEST(button_setcheck_indeterminate)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    DWORD styles[] = { BS_3STATE, BS_AUTO3STATE };
    for (int i = 0; i < 2; i++) {
        HWND b = mkbtn(p, styles[i], 10 + i, "t");
        if (!b) { CHECK(0); continue; }
        SA(b, BM_SETCHECK, BST_INDETERMINATE, 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_INDETERMINATE);
        CHECK((SA(b, BM_GETSTATE, 0, 0) & 3) == BST_INDETERMINATE);
        SA(b, BM_SETCHECK, BST_CHECKED, 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_CHECKED);
        SA(b, BM_SETCHECK, BST_UNCHECKED, 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    }
    /* plain checkbox given 2 : version dependent */
    HWND b = mkbtn(p, BS_AUTOCHECKBOX, 20, "t");
    if (b) { SA(b, BM_SETCHECK, BST_INDETERMINATE, 0); t_note("autocheckbox SETCHECK(2) -> GETCHECK %lld", (long long)SA(b, BM_GETCHECK, 0, 0)); }
    b = mkbtn(p, BS_AUTOCHECKBOX, 21, "t");
    if (b) { SA(b, BM_SETCHECK, 3, 0); t_note("autocheckbox SETCHECK(3) -> GETCHECK %lld", (long long)SA(b, BM_GETCHECK, 0, 0)); }
    U(DestroyWindow)(p);
}

TEST(button_setcheck_pushbutton_ignored)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    DWORD styles[] = { BS_PUSHBUTTON, BS_DEFPUSHBUTTON, BS_GROUPBOX, BS_OWNERDRAW };
    for (int i = 0; i < 4; i++) {
        HWND b = mkbtn(p, styles[i], 10 + i, "t");
        if (!b) { CHECK(0); continue; }
        SA(b, BM_SETCHECK, BST_CHECKED, 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    }
    U(DestroyWindow)(p);
}

TEST(button_click_autocheckbox_cycle)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_AUTOCHECKBOX, 7, "t");
    CHECK(b);
    c_logclear();
    for (int i = 0; i < 6; i++) {
        SA(b, BM_CLICK, 0, 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), (i & 1) ? BST_UNCHECKED : BST_CHECKED);
    }
    EXPECT_EQ(c_ncmd(7, BN_CLICKED), 6);
    /* the notification carries the control handle */
    int idx = c_first_cmd(7, BN_CLICKED);
    CHECK(idx >= 0 && c_log[idx].lp == (LPARAM)b);
    U(DestroyWindow)(p);
}

TEST(button_click_auto3state_cycle)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_AUTO3STATE, 8, "t");
    int want[] = { 1, 2, 0, 1, 2, 0 };
    for (int i = 0; i < 6; i++) {
        SA(b, BM_CLICK, 0, 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), want[i]);
    }
    U(DestroyWindow)(p);
}

TEST(button_click_manual_types_do_not_toggle)
{
    REQUIRE(U(CreateWindowExA));
    DWORD styles[] = { BS_CHECKBOX, BS_3STATE, BS_RADIOBUTTON, BS_PUSHBUTTON, BS_DEFPUSHBUTTON, BS_OWNERDRAW };
    HWND p = c_parent(300, 200);
    c_logclear();
    for (int i = 0; i < 6; i++) {
        HWND b = mkbtn(p, styles[i], 20 + i, "t");
        SA(b, BM_CLICK, 0, 0);
        EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
        EXPECT_EQ(c_ncmd(20 + i, BN_CLICKED), 1);
    }
    U(DestroyWindow)(p);
}

TEST(button_click_autoradio_group)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND r[5];
    r[0] = mkbtn(p, BS_AUTORADIOBUTTON | WS_GROUP, 31, "a");
    r[1] = mkbtn(p, BS_AUTORADIOBUTTON, 32, "b");
    r[2] = mkbtn(p, BS_AUTORADIOBUTTON, 33, "c");
    r[3] = mkbtn(p, BS_AUTORADIOBUTTON | WS_GROUP, 34, "d");
    r[4] = mkbtn(p, BS_AUTORADIOBUTTON, 35, "e");
    SA(r[3], BM_CLICK, 0, 0);
    SA(r[1], BM_CLICK, 0, 0);
    int want[5] = { 0, 1, 0, 1, 0 };
    for (int i = 0; i < 5; i++) EXPECT_EQ(SA(r[i], BM_GETCHECK, 0, 0), want[i]);
    SA(r[2], BM_CLICK, 0, 0);
    int want2[5] = { 0, 0, 1, 1, 0 };
    for (int i = 0; i < 5; i++) EXPECT_EQ(SA(r[i], BM_GETCHECK, 0, 0), want2[i]);
    SA(r[4], BM_CLICK, 0, 0);
    int want3[5] = { 0, 0, 1, 0, 1 };
    for (int i = 0; i < 5; i++) EXPECT_EQ(SA(r[i], BM_GETCHECK, 0, 0), want3[i]);
    /* clicking an already checked radio keeps it checked */
    SA(r[4], BM_CLICK, 0, 0);
    EXPECT_EQ(SA(r[4], BM_GETCHECK, 0, 0), 1);
    U(DestroyWindow)(p);
}

TEST(button_setstate)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    DWORD styles[] = { BS_PUSHBUTTON, BS_DEFPUSHBUTTON, BS_CHECKBOX, BS_AUTOCHECKBOX, BS_RADIOBUTTON, BS_OWNERDRAW };
    for (int i = 0; i < 6; i++) {
        HWND b = mkbtn(p, styles[i], 40 + i, "t");
        EXPECT_EQ(SA(b, BM_SETSTATE, TRUE, 0), 0);
        CHECKF(SA(b, BM_GETSTATE, 0, 0) & BST_PUSHED, "style %d", i);
        /* pushed does not alter the check bits */
        CHECK((SA(b, BM_GETSTATE, 0, 0) & 3) == 0);
        EXPECT_EQ(SA(b, BM_SETSTATE, FALSE, 0), 0);
        CHECKF(!(SA(b, BM_GETSTATE, 0, 0) & BST_PUSHED), "style %d", i);
        /* any non-zero wParam pushes */
        SA(b, BM_SETSTATE, 5, 0);
        CHECK(SA(b, BM_GETSTATE, 0, 0) & BST_PUSHED);
        SA(b, BM_SETSTATE, 0, 0);
        CHECK(!(SA(b, BM_GETSTATE, 0, 0) & BST_PUSHED));
    }
    U(DestroyWindow)(p);
}

TEST(button_setstate_with_check)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_CHECKBOX, 50, "t");
    SA(b, BM_SETCHECK, BST_CHECKED, 0);
    SA(b, BM_SETSTATE, TRUE, 0);
    LRESULT s = SA(b, BM_GETSTATE, 0, 0);
    CHECK((s & 3) == BST_CHECKED);
    CHECK(s & BST_PUSHED);
    SA(b, BM_SETSTATE, FALSE, 0);
    s = SA(b, BM_GETSTATE, 0, 0);
    CHECK((s & 3) == BST_CHECKED);
    CHECK(!(s & BST_PUSHED));
    U(DestroyWindow)(p);
}

TEST(button_setstyle)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_PUSHBUTTON, 60, "t");
    LONG before = U(GetWindowLongA)(b, GWL_STYLE);
    EXPECT_EQ(SA(b, BM_SETSTYLE, BS_CHECKBOX, TRUE), 0);
    LONG st = U(GetWindowLongA)(b, GWL_STYLE);
    CHECK((st & BS_TYPEMASK) == BS_CHECKBOX);
    /* high word (window style) is not modified */
    CHECK((st & 0xFFFF0000) == (before & 0xFFFF0000));
    SA(b, BM_SETSTYLE, BS_AUTORADIOBUTTON, FALSE);
    CHECK((U(GetWindowLongA)(b, GWL_STYLE) & BS_TYPEMASK) == BS_AUTORADIOBUTTON);
    SA(b, BM_SETSTYLE, BS_DEFPUSHBUTTON, FALSE);
    CHECK((U(GetWindowLongA)(b, GWL_STYLE) & BS_TYPEMASK) == BS_DEFPUSHBUTTON);
    CHECK(SA(b, WM_GETDLGCODE, 0, 0) & DLGC_DEFPUSHBUTTON);
    U(DestroyWindow)(p);
}

TEST(button_text_roundtrip_ansi)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_PUSHBUTTON, 70, "Hello");
    char buf[64];
    EXPECT_EQ(SA(b, WM_GETTEXTLENGTH, 0, 0), 5);
    EXPECT_EQ(SA(b, WM_GETTEXT, 64, buf), 5);
    EXPECT_STR(buf, "Hello");
    EXPECT_EQ(SA(b, WM_SETTEXT, 0, "&Open file"), TRUE);
    EXPECT_EQ(SA(b, WM_GETTEXTLENGTH, 0, 0), 10);
    EXPECT_EQ(SA(b, WM_GETTEXT, 64, buf), 10);
    EXPECT_STR(buf, "&Open file");
    /* buffer boundaries: exact, exact-1, 1, 0 */
    memset(buf, 'x', sizeof buf);
    EXPECT_EQ(SA(b, WM_GETTEXT, 11, buf), 10);
    EXPECT_STR(buf, "&Open file");
    memset(buf, 'x', sizeof buf);
    EXPECT_EQ(SA(b, WM_GETTEXT, 10, buf), 9);
    EXPECT_STR(buf, "&Open fil");
    memset(buf, 'x', sizeof buf);
    EXPECT_EQ(SA(b, WM_GETTEXT, 1, buf), 0);
    EXPECT_EQ(buf[0], 0);
    memset(buf, 'x', sizeof buf);
    EXPECT_EQ(SA(b, WM_GETTEXT, 0, buf), 0);
    /* empty text */
    EXPECT_EQ(SA(b, WM_SETTEXT, 0, ""), TRUE);
    EXPECT_EQ(SA(b, WM_GETTEXTLENGTH, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(button_text_ansi_wide_parity)
{
    REQUIRE(U(CreateWindowExW)); REQUIRE(U(GetWindowTextW)); REQUIRE(U(GetWindowTextA));
    HWND p = c_parent(300, 200);
    HWND b = c_ctlw(p, L"Button", L"Wide Text 123", BS_PUSHBUTTON, 71, 0, 0, 80, 20);
    char a[64]; WCHAR w[64];
    CHECK(b != NULL);
    CHECK(U(IsWindowUnicode)(b));
    EXPECT_EQ(U(GetWindowTextA)(b, a, 64), 13);
    EXPECT_STR(a, "Wide Text 123");
    EXPECT_EQ(U(GetWindowTextW)(b, w, 64), 13);
    CHECK(!lstrcmpW(w, L"Wide Text 123"));
    EXPECT_EQ(SWm(b, WM_GETTEXTLENGTH, 0, 0), 13);
    EXPECT_EQ(SA(b, WM_GETTEXTLENGTH, 0, 0), 13);
    /* Unicode text round trips exactly through the W interface */
    static const WCHAR uni[] = { 0x0416, 0x4E2D, 0x20AC, 'a', 0 };
    EXPECT_EQ(SWm(b, WM_SETTEXT, 0, uni), TRUE);
    memset(w, 0, sizeof w);
    EXPECT_EQ(SWm(b, WM_GETTEXT, 64, w), 4);
    CHECK(!lstrcmpW(w, uni));
    EXPECT_EQ(SWm(b, WM_GETTEXTLENGTH, 0, 0), 4);
    /* ANSI length is a code-page dependent conversion: observe only */
    t_note("ANSI text length of 4 wide chars: %lld", (long long)SA(b, WM_GETTEXTLENGTH, 0, 0));
    /* ANSI window: W getter converts */
    HWND ba = mkbtn(p, BS_PUSHBUTTON, 72, "ascii");
    CHECK(!U(IsWindowUnicode)(ba));
    EXPECT_EQ(U(GetWindowTextW)(ba, w, 64), 5);
    CHECK(!lstrcmpW(w, L"ascii"));
    U(DestroyWindow)(p);
}

TEST(button_image_bitmap)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_PUSHBUTTON | BS_BITMAP, 80, "");
    HBITMAP bm1 = CreateBitmap(8, 8, 1, 1, NULL), bm2 = CreateBitmap(16, 16, 1, 1, NULL);
    CHECK(bm1 && bm2);
    EXPECT_EQ(SA(b, BM_GETIMAGE, IMAGE_BITMAP, 0), 0);
    EXPECT_EQ(SA(b, BM_SETIMAGE, IMAGE_BITMAP, bm1), 0);
    EXPECT_EQ(SA(b, BM_GETIMAGE, IMAGE_BITMAP, 0), bm1);
    /* returns the previous handle */
    EXPECT_EQ(SA(b, BM_SETIMAGE, IMAGE_BITMAP, bm2), bm1);
    EXPECT_EQ(SA(b, BM_GETIMAGE, IMAGE_BITMAP, 0), bm2);
    EXPECT_EQ(SA(b, BM_SETIMAGE, IMAGE_BITMAP, NULL), bm2);
    EXPECT_EQ(SA(b, BM_GETIMAGE, IMAGE_BITMAP, 0), 0);
    U(DestroyWindow)(p);
    DeleteObject(bm1); DeleteObject(bm2);
}

TEST(button_image_icon)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(LoadIconA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_PUSHBUTTON | BS_ICON, 81, "");
    HICON i1 = U(LoadIconA)(NULL, IDI_APPLICATION), i2 = U(LoadIconA)(NULL, IDI_INFORMATION);
    CHECK(i1 && i2);
    EXPECT_EQ(SA(b, BM_SETIMAGE, IMAGE_ICON, i1), 0);
    EXPECT_EQ(SA(b, BM_GETIMAGE, IMAGE_ICON, 0), i1);
    EXPECT_EQ(SA(b, BM_SETIMAGE, IMAGE_ICON, i2), i1);
    EXPECT_EQ(SA(b, BM_GETIMAGE, IMAGE_ICON, 0), i2);
    U(DestroyWindow)(p);
}

TEST(button_nchittest_transparency)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowRect));
    HWND p = c_parent(300, 200);
    HWND g = c_ctl(p, "Button", "grp", BS_GROUPBOX, 90, 5, 5, 100, 80);
    HWND b = c_ctl(p, "Button", "btn", BS_PUSHBUTTON, 91, 150, 5, 100, 40);
    RECT r;
    U(GetWindowRect)(g, &r);
    EXPECT_EQ(SA(g, WM_NCHITTEST, 0, MAKELPARAM(r.left + 40, r.top + 40)), HTTRANSPARENT);
    U(GetWindowRect)(b, &r);
    EXPECT_EQ(SA(b, WM_NCHITTEST, 0, MAKELPARAM(r.left + 40, r.top + 20)), HTCLIENT);
    U(DestroyWindow)(p);
}

TEST(button_ownerdraw_printclient)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = c_ctl(p, "Button", "od", BS_OWNERDRAW, 95, 5, 5, 90, 30);
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bm = CreateBitmap(200, 100, 1, 1, NULL);
    HGDIOBJ old = SelectObject(dc, bm);
    c_logclear();
    SA(b, WM_PRINTCLIENT, dc, PRF_CLIENT);
    int n = c_nmsg(WM_DRAWITEM);
    CHECK(n >= 1);
    if (n >= 1) {
        DRAWITEMSTRUCT *d = &c_log[0].di;
        for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_DRAWITEM) { d = &c_log[i].di; break; }
        EXPECT_EQ(d->CtlType, ODT_BUTTON);
        EXPECT_EQ(d->CtlID, 95);
        EXPECT_EQ(d->hwndItem, b);
        CHECK(d->hDC != NULL);
        EXPECT_EQ(d->rcItem.left, 0); EXPECT_EQ(d->rcItem.top, 0);
        EXPECT_EQ(d->rcItem.right, 90); EXPECT_EQ(d->rcItem.bottom, 30);
        CHECK(!(d->itemState & ODS_DISABLED));
        CHECK(d->itemAction & (ODA_DRAWENTIRE | ODA_SELECT | ODA_FOCUS));
    }
    /* disabled state is reflected */
    U(EnableWindow)(b, FALSE);
    c_logclear();
    SA(b, WM_PRINTCLIENT, dc, PRF_CLIENT);
    for (int i = 0; i < c_nlogent; i++)
        if (c_log[i].msg == WM_DRAWITEM) { CHECK(c_log[i].di.itemState & ODS_DISABLED); break; }
    /* pushed state */
    U(EnableWindow)(b, TRUE);
    SA(b, BM_SETSTATE, TRUE, 0);
    c_logclear();
    SA(b, WM_PRINTCLIENT, dc, PRF_CLIENT);
    for (int i = 0; i < c_nlogent; i++)
        if (c_log[i].msg == WM_DRAWITEM) { CHECK(c_log[i].di.itemState & ODS_SELECTED); break; }
    SelectObject(dc, old);
    DeleteObject(bm); DeleteDC(dc);
    U(DestroyWindow)(p);
}

TEST(button_dlgbutton_helpers)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(CheckDlgButton)); REQUIRE(U(IsDlgButtonChecked));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_AUTOCHECKBOX, 100, "x");
    HWND t = mkbtn(p, BS_AUTO3STATE, 101, "y");
    EXPECT_EQ(U(IsDlgButtonChecked)(p, 100), BST_UNCHECKED);
    EXPECT_EQ(U(CheckDlgButton)(p, 100, BST_CHECKED), TRUE);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_CHECKED);
    EXPECT_EQ(U(IsDlgButtonChecked)(p, 100), BST_CHECKED);
    EXPECT_EQ(U(CheckDlgButton)(p, 101, BST_INDETERMINATE), TRUE);
    EXPECT_EQ(U(IsDlgButtonChecked)(p, 101), BST_INDETERMINATE);
    EXPECT_EQ(U(CheckDlgButton)(p, 100, BST_UNCHECKED), TRUE);
    EXPECT_EQ(U(IsDlgButtonChecked)(p, 100), BST_UNCHECKED);
    /* missing child id */
    EXPECT_EQ(U(CheckDlgButton)(p, 9999, BST_CHECKED), FALSE);
    EXPECT_EQ(U(IsDlgButtonChecked)(p, 9999), 0);
    (void)t;
    U(DestroyWindow)(p);
}

TEST(button_checkradiobutton_range)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(CheckRadioButton)); REQUIRE(U(IsDlgButtonChecked));
    HWND p = c_parent(300, 300);
    HWND r[4];
    for (int i = 0; i < 4; i++) r[i] = mkbtn(p, BS_RADIOBUTTON, 110 + i, "r");
    SA(r[0], BM_SETCHECK, 1, 0);
    EXPECT_EQ(U(CheckRadioButton)(p, 110, 112, 111), TRUE);
    EXPECT_EQ(SA(r[0], BM_GETCHECK, 0, 0), 0);
    EXPECT_EQ(SA(r[1], BM_GETCHECK, 0, 0), 1);
    EXPECT_EQ(SA(r[2], BM_GETCHECK, 0, 0), 0);
    EXPECT_EQ(SA(r[3], BM_GETCHECK, 0, 0), 0);
    /* a control outside the range is untouched */
    SA(r[3], BM_SETCHECK, 1, 0);
    U(CheckRadioButton)(p, 110, 112, 112);
    EXPECT_EQ(SA(r[3], BM_GETCHECK, 0, 0), 1);
    EXPECT_EQ(SA(r[2], BM_GETCHECK, 0, 0), 1);
    EXPECT_EQ(SA(r[1], BM_GETCHECK, 0, 0), 0);
    /* check id outside range: all in range get cleared, still TRUE (no validation) */
    EXPECT_EQ(U(CheckRadioButton)(p, 110, 112, 500), TRUE);
    EXPECT_EQ(SA(r[2], BM_GETCHECK, 0, 0), 0);
    /* invalid dialog handle fails */
    EXPECT_EQ(U(CheckRadioButton)((HWND)(ULONG_PTR)0xdead0, 110, 112, 111), FALSE);
    EXPECT_EQ(U(CheckRadioButton)(NULL, 110, 112, 111), FALSE);
    U(DestroyWindow)(p);
}

TEST(button_notify_clicked_code_values)
{
    EXPECT_EQ(BN_CLICKED, 0); EXPECT_EQ(BN_PAINT, 1); EXPECT_EQ(BN_HILITE, 2);
    EXPECT_EQ(BN_UNHILITE, 3); EXPECT_EQ(BN_DISABLE, 4); EXPECT_EQ(BN_DOUBLECLICKED, 5);
    EXPECT_EQ(BN_SETFOCUS, 6); EXPECT_EQ(BN_KILLFOCUS, 7);
    EXPECT_EQ(BM_GETCHECK, 0xF0); EXPECT_EQ(BM_SETCHECK, 0xF1); EXPECT_EQ(BM_GETSTATE, 0xF2);
    EXPECT_EQ(BM_SETSTATE, 0xF3); EXPECT_EQ(BM_SETSTYLE, 0xF4); EXPECT_EQ(BM_CLICK, 0xF5);
    EXPECT_EQ(BM_GETIMAGE, 0xF6); EXPECT_EQ(BM_SETIMAGE, 0xF7);
    EXPECT_EQ(BST_UNCHECKED, 0); EXPECT_EQ(BST_CHECKED, 1); EXPECT_EQ(BST_INDETERMINATE, 2);
    EXPECT_EQ(BST_PUSHED, 4); EXPECT_EQ(BST_FOCUS, 8);
}

TEST(button_notify_style_bs_notify)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_PUSHBUTTON | BS_NOTIFY, 120, "n");
    c_logclear();
    SA(b, BM_CLICK, 0, 0);
    EXPECT_EQ(c_ncmd(120, BN_CLICKED), 1);
    /* BN_PUSHED/BN_UNPUSHED are only for BS_NOTIFY and implementation dependent: observe */
    t_note("BS_NOTIFY BM_CLICK: HILITE=%d UNHILITE=%d", c_ncmd(120, BN_HILITE), c_ncmd(120, BN_UNHILITE));
    U(DestroyWindow)(p);
}

TEST(button_disabled_click)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_AUTOCHECKBOX, 130, "n");
    U(EnableWindow)(b, FALSE);
    CHECK(!U(IsWindowEnabled)(b));
    c_logclear();
    SA(b, BM_CLICK, 0, 0);
    t_note("BM_CLICK on disabled autocheckbox: clicked=%d check=%lld", c_ncmd(130, BN_CLICKED), (long long)SA(b, BM_GETCHECK, 0, 0));
    /* BM_SETCHECK still works while disabled */
    SA(b, BM_SETCHECK, 1, 0);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), 1);
    U(EnableWindow)(b, TRUE);
    CHECK(U(IsWindowEnabled)(b));
    U(DestroyWindow)(p);
}

TEST(button_destroyed_handle_messages)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(IsWindow));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_AUTOCHECKBOX, 140, "n");
    U(DestroyWindow)(b);
    CHECK(!U(IsWindow)(b));
    SetLastError(0);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), 0);
    EXPECT_EQ(SA(b, BM_CLICK, 0, 0), 0);
    EXPECT_EQ(SA((HWND)(ULONG_PTR)0x12345678, BM_GETCHECK, 0, 0), 0);
    EXPECT_EQ(SA(NULL, BM_GETCHECK, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(button_parent_destroy_destroys_children)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(IsWindow));
    HWND p = c_parent(300, 200);
    HWND b[3];
    for (int i = 0; i < 3; i++) b[i] = mkbtn(p, BS_PUSHBUTTON, 150 + i, "n");
    U(DestroyWindow)(p);
    for (int i = 0; i < 3; i++) CHECK(!U(IsWindow)(b[i]));
}

TEST(button_many_create_destroy)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    for (int i = 0; i < 300; i++) {
        HWND b = mkbtn(p, k_btn_types[i % NBTN].style, 1, "loop");
        if (!b) { CHECKF(0, "create %d", i); break; }
        SA(b, BM_SETCHECK, 1, 0);
        U(DestroyWindow)(b);
    }
    CHECK(1);
    U(DestroyWindow)(p);
}

TEST(button_wm_setfont_getfont)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_PUSHBUTTON, 160, "f");
    HFONT f = GetStockObject(DEFAULT_GUI_FONT) ? (HFONT)GetStockObject(DEFAULT_GUI_FONT) : NULL;
    EXPECT_EQ(SA(b, WM_GETFONT, 0, 0), 0);
    SA(b, WM_SETFONT, f, TRUE);
    EXPECT_EQ(SA(b, WM_GETFONT, 0, 0), f);
    SA(b, WM_SETFONT, NULL, TRUE);
    EXPECT_EQ(SA(b, WM_GETFONT, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(button_enable_disable_messages)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = mkbtn(p, BS_PUSHBUTTON, 170, "e");
    CHECK(U(IsWindowEnabled)(b));
    EXPECT_EQ(U(EnableWindow)(b, FALSE), FALSE);   /* previously enabled -> returns 0 */
    EXPECT_EQ(U(EnableWindow)(b, FALSE), TRUE);    /* previously disabled -> non-zero */
    CHECK(U(GetWindowLongA)(b, GWL_STYLE) & WS_DISABLED);
    EXPECT_EQ(U(EnableWindow)(b, TRUE), TRUE);
    CHECK(!(U(GetWindowLongA)(b, GWL_STYLE) & WS_DISABLED));
    U(DestroyWindow)(p);
}

TEST(button_sizing_via_ctl_size_and_clientrect)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetClientRect));
    HWND p = c_parent(300, 200);
    HWND b = c_ctl(p, "Button", "sz", BS_PUSHBUTTON, 180, 3, 4, 77, 33);
    RECT r;
    CHECK(U(GetClientRect)(b, &r));
    EXPECT_EQ(r.left, 0); EXPECT_EQ(r.top, 0);
    EXPECT_EQ(r.right, 77); EXPECT_EQ(r.bottom, 33);
    U(DestroyWindow)(p);
}
