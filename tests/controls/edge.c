/* Additional controls edge cases (documented behaviour only). */
#include "ctl.h"

TEST(ctledge_scrollinfo_pos_clamp_to_page)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo)); REQUIRE(U(GetScrollInfo));
    HWND p = c_parent(300, 300);
    SCROLLINFO si = { sizeof si, SIF_RANGE | SIF_PAGE | SIF_POS, 0, 100, 10, 500, 0 };
    U(SetScrollInfo)(p, SB_VERT, &si, FALSE);
    SCROLLINFO g = { sizeof g, SIF_ALL };
    CHECK(U(GetScrollInfo)(p, SB_VERT, &g));
    EXPECT_EQ(g.nMin, 0); EXPECT_EQ(g.nMax, 100); EXPECT_EQ(g.nPage, 10);
    EXPECT_EQ(g.nPos, 91);
    si.nPos = -50; si.fMask = SIF_POS;
    U(SetScrollInfo)(p, SB_VERT, &si, FALSE);
    g.fMask = SIF_POS; U(GetScrollInfo)(p, SB_VERT, &g);
    EXPECT_EQ(g.nPos, 0);
    U(DestroyWindow)(p);
}

TEST(ctledge_scrollinfo_page_larger_than_range)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollInfo)); REQUIRE(U(GetScrollInfo));
    HWND p = c_parent(300, 300);
    SCROLLINFO si = { sizeof si, SIF_RANGE | SIF_PAGE | SIF_POS, 0, 10, 50, 7, 0 };
    U(SetScrollInfo)(p, SB_HORZ, &si, FALSE);
    SCROLLINFO g = { sizeof g, SIF_ALL };
    CHECK(U(GetScrollInfo)(p, SB_HORZ, &g));
    EXPECT_EQ(g.nPos, 0);
    CHECK(g.nPage <= 11);
    U(DestroyWindow)(p);
}

TEST(ctledge_scrollinfo_bad_size_or_bar)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetScrollInfo));
    HWND p = c_parent(300, 300);
    SCROLLINFO g = { sizeof g - 4, SIF_ALL };
    SetLastError(0);
    EXPECT_EQ(U(GetScrollInfo)(p, SB_VERT, &g), FALSE);
    g.cbSize = sizeof g;
    EXPECT_EQ(U(GetScrollInfo)(p, 17, &g), FALSE);
    U(DestroyWindow)(p);
}

TEST(ctledge_scroll_range_roundtrip)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetScrollRange)); REQUIRE(U(GetScrollRange));
    HWND p = c_parent(300, 300);
    CHECK(U(SetScrollRange)(p, SB_VERT, -20, 80, FALSE));
    int mn = 99, mx = 99;
    CHECK(U(GetScrollRange)(p, SB_VERT, &mn, &mx));
    EXPECT_EQ(mn, -20); EXPECT_EQ(mx, 80);
    U(DestroyWindow)(p);
}

TEST(ctledge_button_checkbox_cycle)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = c_ctl(p, "BUTTON", "cb", BS_AUTOCHECKBOX, 5, 0, 0, 100, 20);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    SA(b, BM_CLICK, 0, 0);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_CHECKED);
    SA(b, BM_CLICK, 0, 0);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    SA(b, BM_SETCHECK, BST_INDETERMINATE, 0);
    /* plain checkbox ignores the indeterminate value or keeps it; must be one of the legal states */
    LRESULT r = SA(b, BM_GETCHECK, 0, 0);
    t_note("autocheckbox after BM_SETCHECK(INDETERMINATE) -> %d", (int)r);
    U(DestroyWindow)(p);
}

TEST(ctledge_button_3state_cycle)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND b = c_ctl(p, "BUTTON", "t", BS_AUTO3STATE, 5, 0, 0, 100, 20);
    EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    SA(b, BM_CLICK, 0, 0); EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_CHECKED);
    SA(b, BM_CLICK, 0, 0); EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_INDETERMINATE);
    SA(b, BM_CLICK, 0, 0); EXPECT_EQ(SA(b, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    U(DestroyWindow)(p);
}

TEST(ctledge_listbox_deletestring_bounds)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = c_ctl(p, "LISTBOX", "", LBS_NOTIFY, 6, 0, 0, 100, 100);
    SA(lb, LB_ADDSTRING, 0, "a"); SA(lb, LB_ADDSTRING, 0, "b");
    EXPECT_EQ(SA(lb, LB_DELETESTRING, 2, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_DELETESTRING, -1, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_DELETESTRING, 0, 0), 1);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 1);
    SA(lb, LB_RESETCONTENT, 0, 0);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETTEXTLEN, 0, 0), LB_ERR);
    U(DestroyWindow)(p);
}

TEST(ctledge_combo_delete_and_count)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = c_ctl(p, "COMBOBOX", "", CBS_DROPDOWNLIST, 7, 0, 0, 100, 100);
    EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "x"), 0);
    EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "y"), 1);
    EXPECT_EQ(SA(cb, CB_GETCOUNT, 0, 0), 2);
    EXPECT_EQ(SA(cb, CB_DELETESTRING, 5, 0), CB_ERR);
    EXPECT_EQ(SA(cb, CB_DELETESTRING, 0, 0), 1);
    EXPECT_EQ(SA(cb, CB_FINDSTRINGEXACT, -1, "y"), 0);
    EXPECT_EQ(SA(cb, CB_FINDSTRINGEXACT, -1, "zz"), CB_ERR);
    SA(cb, CB_RESETCONTENT, 0, 0);
    EXPECT_EQ(SA(cb, CB_GETCOUNT, 0, 0), 0);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), CB_ERR);
    U(DestroyWindow)(p);
}

TEST(ctledge_edit_gettextlength_unicode_parity)
{
    REQUIRE(U(CreateWindowExW));
    HWND p = c_parent(300, 200);
    HWND e = c_ctlw(p, L"EDIT", L"h\x00e9llo\x4e16", ES_AUTOHSCROLL, 8, 0, 0, 100, 20);
    EXPECT_EQ(SWm(e, WM_GETTEXTLENGTH, 0, 0), 6);
    wchar_t w[16]; EXPECT_EQ(SWm(e, WM_GETTEXT, 16, w), 6);
    EXPECT_EQ(w[5], 0x4e16);
    wchar_t w2[4]; EXPECT_EQ(SWm(e, WM_GETTEXT, 4, w2), 3);
    EXPECT_EQ(w2[3], 0);
    EXPECT_EQ(SWm(e, WM_GETTEXT, 0, w2), 0);
    U(DestroyWindow)(p);
}

TEST(ctledge_static_settext_roundtrip)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 200);
    HWND s = c_ctl(p, "STATIC", "one", SS_LEFT, 9, 0, 0, 100, 20);
    CHECK(c_text_is(s, "one"));
    SA(s, WM_SETTEXT, 0, "two");
    CHECK(c_text_is(s, "two"));
    EXPECT_EQ(SA(s, WM_GETTEXTLENGTH, 0, 0), 3);
    U(DestroyWindow)(p);
}
