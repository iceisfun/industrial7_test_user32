/* controls: button style flags that had no direct test (PUSHLIKE, LEFTTEXT/RIGHTBUTTON, FLAT,
 * MULTILINE, alignment bits, BM_SETSTYLE), LBS_STANDARD, and size-box scroll bars. */
#include "ctl.h"

#ifndef BS_PUSHBOX
#define BS_PUSHBOX 0x0A
#endif

TEST(controls_x3_button_style_constants) {
    EXPECT_EQ(BS_TEXT, 0); EXPECT_EQ(BS_USERBUTTON, 8); EXPECT_EQ(BS_RIGHTBUTTON, BS_LEFTTEXT);
    EXPECT_EQ(BS_LEFTTEXT, 0x20); EXPECT_EQ(BS_ICON, 0x40); EXPECT_EQ(BS_BITMAP, 0x80);
    EXPECT_EQ(BS_LEFT, 0x100); EXPECT_EQ(BS_RIGHT, 0x200); EXPECT_EQ(BS_CENTER, 0x300);
    EXPECT_EQ(BS_TOP, 0x400); EXPECT_EQ(BS_BOTTOM, 0x800); EXPECT_EQ(BS_VCENTER, 0xC00);
    EXPECT_EQ(BS_PUSHLIKE, 0x1000); EXPECT_EQ(BS_MULTILINE, 0x2000); EXPECT_EQ(BS_NOTIFY, 0x4000);
    EXPECT_EQ(BS_FLAT, 0x8000); EXPECT_EQ(BS_TYPEMASK, 0xF);
}

TEST(controls_x3_button_modifier_bits_kept_in_style) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongW));
    static const DWORD mods[] = { BS_LEFTTEXT, BS_PUSHLIKE, BS_FLAT, BS_MULTILINE, BS_NOTIFY,
                                  BS_LEFT, BS_RIGHT, BS_CENTER, BS_TOP, BS_BOTTOM, BS_VCENTER };
    HWND p = c_parent(300, 200);
    for (unsigned i = 0; i < sizeof mods / sizeof mods[0]; i++) {
        HWND b = c_ctl(p, "Button", "m", BS_AUTOCHECKBOX | mods[i], 10 + i, 0, 0, 80, 20);
        CHECKF(b != NULL, "create 0x%lx", mods[i]);
        if (!b) continue;
        DWORD st = (DWORD)U(GetWindowLongW)(b, GWL_STYLE);
        CHECKF((st & mods[i]) == mods[i], "style 0x%08lx lost 0x%lx", st, mods[i]);
        CHECKF((st & BS_TYPEMASK) == BS_AUTOCHECKBOX, "type 0x%lx", st & BS_TYPEMASK);
        U(DestroyWindow)(b);
    }
    U(DestroyWindow)(p);
}

TEST(controls_x3_pushlike_checkbox_toggles_via_click) {
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = c_ctl(p, "Button", "t", BS_AUTOCHECKBOX | BS_PUSHLIKE, 20, 0, 0, 80, 24);
    REQUIRE(b);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    SA(b, BM_CLICK, 0, 0);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_CHECKED);
    SA(b, BM_CLICK, 0, 0);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    SA(b, BM_SETCHECK, BST_CHECKED, 0);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0) & BST_CHECKED, BST_CHECKED);
    t_note("pushlike checkbox GetDlgCode 0x%llx", (unsigned long long)SA(b, WM_GETDLGCODE, 0, 0));
    U(DestroyWindow)(p);
}

TEST(controls_x3_pushlike_radio_click_sets_check) {
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND r1 = c_ctl(p, "Button", "a", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP, 21, 0, 0, 60, 24);
    HWND r2 = c_ctl(p, "Button", "b", BS_AUTORADIOBUTTON | BS_PUSHLIKE, 22, 70, 0, 60, 24);
    REQUIRE(r1 && r2);
    SA(r1, BM_CLICK, 0, 0);
    EXPECT_EQ(SA(r1, BM_GETCHECK, 0, 0), BST_CHECKED);
    EXPECT_EQ(SA(r2, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    SA(r2, BM_CLICK, 0, 0);
    EXPECT_EQ(SA(r1, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    EXPECT_EQ(SA(r2, BM_GETCHECK, 0, 0), BST_CHECKED);
    U(DestroyWindow)(p);
}

TEST(controls_x3_bm_setstyle_changes_type) {
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = c_ctl(p, "Button", "s", BS_PUSHBUTTON, 23, 0, 0, 80, 24);
    REQUIRE(b);
    SA(b, BM_SETSTYLE, BS_AUTOCHECKBOX, TRUE);
    DWORD st = (DWORD)U(GetWindowLongW)(b, GWL_STYLE);
    EXPECT_EQ(st & BS_TYPEMASK, BS_AUTOCHECKBOX);
    SA(b, BM_CLICK, 0, 0);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_CHECKED);
    t_note("BM_SETSTYLE kept WS_CHILD|WS_VISIBLE: style 0x%08lx", st);
    U(DestroyWindow)(p);
}

TEST(controls_x3_button_obsolete_styles_note) {
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    static const DWORD t[] = { BS_USERBUTTON, BS_PUSHBOX };
    for (int i = 0; i < 2; i++) {
        HWND b = c_ctl(p, "Button", "o", t[i], 30 + i, 0, 0, 80, 24);
        t_note("Button style 0x%lx -> %p dlgcode 0x%llx", t[i], b, b ? (unsigned long long)SA(b, WM_GETDLGCODE, 0, 0) : 0ull);
        if (b) U(DestroyWindow)(b);
    }
    HWND b = c_ctl(p, "Button", "d", BS_PUSHBUTTON, 40, 0, 0, 80, 24);
    if (b) { LRESULT r = SA(b, 0xF8 /* BM_SETDONTCLICK */, TRUE, 0); t_note("BM_SETDONTCLICK -> %lld", (long long)r); }
    U(DestroyWindow)(p);
}

TEST(controls_x3_lbs_standard_sorts_and_notifies) {
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND lb = c_ctl(p, "ListBox", "", LBS_STANDARD, 50, 0, 0, 100, 100);
    REQUIRE(lb);
    DWORD st = (DWORD)U(GetWindowLongW)(lb, GWL_STYLE);
    /* WS_VSCROLL tracks the visible scroll bar: an empty list box does not show one (Win10 22H2), so it is checked below */
    EXPECT_EQ(st & (LBS_NOTIFY | LBS_SORT | WS_BORDER), LBS_NOTIFY | LBS_SORT | WS_BORDER);
    SA(lb, LB_ADDSTRING, 0, "pear"); SA(lb, LB_ADDSTRING, 0, "apple"); SA(lb, LB_ADDSTRING, 0, "mango");
    CHECK(c_lbtext_is(lb, 0, "apple")); CHECK(c_lbtext_is(lb, 1, "mango")); CHECK(c_lbtext_is(lb, 2, "pear"));
    for (int i = 0; i < 40; i++) SA(lb, LB_ADDSTRING, 0, "zzz");
    st = (DWORD)U(GetWindowLongW)(lb, GWL_STYLE);
    EXPECT_EQ(st & WS_VSCROLL, WS_VSCROLL);              /* overflowing list: the vertical scroll bar is present */
    U(DestroyWindow)(p);
}

TEST(controls_x3_multipleaddstring_note) {
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND lb = c_ctl(p, "ListBox", "", LBS_NOTIFY, 51, 0, 0, 100, 100);
    HWND cb = c_ctl(p, "ComboBox", "", CBS_DROPDOWNLIST | WS_VSCROLL, 52, 110, 0, 100, 150);
    REQUIRE(lb && cb);
    LRESULT r = SA(lb, 0x01B1 /* LB_MULTIPLEADDSTRING */, 0, "one");
    t_note("LB_MULTIPLEADDSTRING -> %lld count %lld", (long long)r, (long long)SA(lb, LB_GETCOUNT, 0, 0));
    r = SA(cb, 0x0163 /* CB_MULTIPLEADDSTRING */, 0, "one");
    t_note("CB_MULTIPLEADDSTRING -> %lld count %lld", (long long)r, (long long)SA(cb, CB_GETCOUNT, 0, 0));
    r = SA(cb, 0x1702 /* CB_GETMINVISIBLE */, 0, 0);
    t_note("CB_GETMINVISIBLE default -> %lld", (long long)r);
    r = SA(cb, 0x1701 /* CB_SETMINVISIBLE */, 5, 0);
    t_note("CB_SETMINVISIBLE(5) -> %lld, get -> %lld", (long long)r, (long long)SA(cb, 0x1702, 0, 0));
    U(DestroyWindow)(p);
}

TEST(controls_x3_scrollbar_sizebox_alignment_styles) {
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    static const DWORD s[] = { SBS_SIZEBOX, SBS_SIZEBOX | SBS_SIZEBOXTOPLEFTALIGN, SBS_SIZEBOX | SBS_SIZEBOXBOTTOMRIGHTALIGN,
                               SBS_SIZEGRIP };
    for (int i = 0; i < 4; i++) {
        HWND h = c_ctl(p, "ScrollBar", "", s[i], 60 + i, 10, 10, 20, 20);
        CHECKF(h != NULL, "style 0x%lx", s[i]);
        if (!h) continue;
        DWORD st = (DWORD)U(GetWindowLongW)(h, GWL_STYLE);
        CHECKF((st & s[i]) == s[i], "style 0x%08lx lost 0x%lx", st, s[i]);
        RECT r; U(GetWindowRect)(h, &r);
        t_note("sizebox style 0x%lx rect %ldx%ld", s[i], r.right - r.left, r.bottom - r.top);
        U(DestroyWindow)(h);
    }
    U(DestroyWindow)(p);
}
