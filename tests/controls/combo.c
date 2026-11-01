#include "ctl.h"

static const char *const k_items[] = { "Alpha", "Bravo", "Charlie", "alpine", "Delta" };
#define NITEMS 5

static HWND mkcb(HWND p, DWORD style, int id)
{
    return c_ctl(p, "ComboBox", "", style | WS_VSCROLL, id, 5, 5, 150, 200);
}
static void cb_fill(HWND cb)
{
    for (int i = 0; i < NITEMS; i++) SA(cb, CB_ADDSTRING, 0, k_items[i]);
}
static BOOL cb_info(HWND cb, COMBOBOXINFO *ci)
{
    memset(ci, 0, sizeof *ci);
    ci->cbSize = sizeof *ci;
    return (BOOL)SA(cb, CB_GETCOMBOBOXINFO, 0, ci);
}
static const struct { const char *name; DWORD style; } k_cbtypes[] = {
    { "simple", CBS_SIMPLE }, { "dropdown", CBS_DROPDOWN }, { "dropdownlist", CBS_DROPDOWNLIST },
};

TEST(combo_constants)
{
    EXPECT_EQ(CB_ERR, -1); EXPECT_EQ(CB_ERRSPACE, -2); EXPECT_EQ(CB_OKAY, 0);
    EXPECT_EQ(CB_GETEDITSEL, 0x140); EXPECT_EQ(CB_LIMITTEXT, 0x141); EXPECT_EQ(CB_SETEDITSEL, 0x142);
    EXPECT_EQ(CB_ADDSTRING, 0x143); EXPECT_EQ(CB_DELETESTRING, 0x144); EXPECT_EQ(CB_DIR, 0x145);
    EXPECT_EQ(CB_GETCOUNT, 0x146); EXPECT_EQ(CB_GETCURSEL, 0x147); EXPECT_EQ(CB_GETLBTEXT, 0x148);
    EXPECT_EQ(CB_GETLBTEXTLEN, 0x149); EXPECT_EQ(CB_INSERTSTRING, 0x14A); EXPECT_EQ(CB_RESETCONTENT, 0x14B);
    EXPECT_EQ(CB_FINDSTRING, 0x14C); EXPECT_EQ(CB_SELECTSTRING, 0x14D); EXPECT_EQ(CB_SETCURSEL, 0x14E);
    EXPECT_EQ(CB_SHOWDROPDOWN, 0x14F); EXPECT_EQ(CB_GETITEMDATA, 0x150); EXPECT_EQ(CB_SETITEMDATA, 0x151);
    EXPECT_EQ(CB_GETDROPPEDCONTROLRECT, 0x152); EXPECT_EQ(CB_SETITEMHEIGHT, 0x153); EXPECT_EQ(CB_GETITEMHEIGHT, 0x154);
    EXPECT_EQ(CB_SETEXTENDEDUI, 0x155); EXPECT_EQ(CB_GETEXTENDEDUI, 0x156); EXPECT_EQ(CB_GETDROPPEDSTATE, 0x157);
    EXPECT_EQ(CB_FINDSTRINGEXACT, 0x158); EXPECT_EQ(CB_SETLOCALE, 0x159); EXPECT_EQ(CB_GETLOCALE, 0x15A);
    EXPECT_EQ(CB_GETTOPINDEX, 0x15B); EXPECT_EQ(CB_SETTOPINDEX, 0x15C); EXPECT_EQ(CB_GETHORIZONTALEXTENT, 0x15D);
    EXPECT_EQ(CB_SETHORIZONTALEXTENT, 0x15E); EXPECT_EQ(CB_GETDROPPEDWIDTH, 0x15F); EXPECT_EQ(CB_SETDROPPEDWIDTH, 0x160);
    EXPECT_EQ(CB_INITSTORAGE, 0x161); EXPECT_EQ(CB_GETCOMBOBOXINFO, 0x164);
    EXPECT_EQ(CBN_ERRSPACE, -1); EXPECT_EQ(CBN_SELCHANGE, 1); EXPECT_EQ(CBN_DBLCLK, 2); EXPECT_EQ(CBN_SETFOCUS, 3);
    EXPECT_EQ(CBN_KILLFOCUS, 4); EXPECT_EQ(CBN_EDITCHANGE, 5); EXPECT_EQ(CBN_EDITUPDATE, 6); EXPECT_EQ(CBN_DROPDOWN, 7);
    EXPECT_EQ(CBN_CLOSEUP, 8); EXPECT_EQ(CBN_SELENDOK, 9); EXPECT_EQ(CBN_SELENDCANCEL, 10);
    EXPECT_EQ(CBS_SIMPLE, 1); EXPECT_EQ(CBS_DROPDOWN, 2); EXPECT_EQ(CBS_DROPDOWNLIST, 3); EXPECT_EQ(CBS_OWNERDRAWFIXED, 0x10);
    EXPECT_EQ(CBS_OWNERDRAWVARIABLE, 0x20); EXPECT_EQ(CBS_AUTOHSCROLL, 0x40); EXPECT_EQ(CBS_OEMCONVERT, 0x80);
    EXPECT_EQ(CBS_SORT, 0x100); EXPECT_EQ(CBS_HASSTRINGS, 0x200); EXPECT_EQ(CBS_NOINTEGRALHEIGHT, 0x400);
    EXPECT_EQ(CBS_DISABLENOSCROLL, 0x800); EXPECT_EQ(CBS_UPPERCASE, 0x2000); EXPECT_EQ(CBS_LOWERCASE, 0x4000);
}

TEST(combo_classname_all_types)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongA));
    HWND p = c_parent(300, 300);
    for (int t = 0; t < 3; t++) {
        HWND cb = mkcb(p, k_cbtypes[t].style, 10 + t);
        char n[32];
        CHECKF(cb != NULL, "%s", k_cbtypes[t].name);
        if (!cb) continue;
        c_classname(cb, n, sizeof n);
        EXPECT_STR(n, "ComboBox");
        CHECK((U(GetWindowLongA)(cb, GWL_STYLE) & 3) == k_cbtypes[t].style);
        EXPECT_EQ(SA(cb, CB_GETCOUNT, 0, 0), 0);
        EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), CB_ERR);
    }
    U(DestroyWindow)(p);
}

TEST(combo_child_windows_by_type)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetParent));
    HWND p = c_parent(300, 300);
    for (int t = 0; t < 3; t++) {
        HWND cb = mkcb(p, k_cbtypes[t].style, 10 + t);
        COMBOBOXINFO ci;
        CHECKF(cb_info(cb, &ci), "%s", k_cbtypes[t].name);
        EXPECT_EQ(ci.hwndCombo, cb);
        CHECKF(ci.hwndList != NULL, "%s list", k_cbtypes[t].name);
        char n[32];
        if (ci.hwndList) { c_classname(ci.hwndList, n, sizeof n); EXPECT_STR(n, "ComboLBox"); }
        if (t == 2) {
            t_note("DROPDOWNLIST hwndItem = %p (combo %p)", ci.hwndItem, cb);
        } else {
            CHECKF(ci.hwndItem != NULL, "%s edit", k_cbtypes[t].name);
            if (ci.hwndItem) {
                c_classname(ci.hwndItem, n, sizeof n);
                EXPECT_STR(n, "Edit");
                EXPECT_EQ(U(GetParent)(ci.hwndItem), cb);
            }
        }
        if (t == 0 && ci.hwndList) EXPECT_EQ(U(GetParent)(ci.hwndList), cb);   /* simple: list is a child */
        if (t != 0 && ci.hwndList) {
            LONG st = U(GetWindowLongA)(ci.hwndList, GWL_STYLE);
            t_note("%s list style %08lx parent %p", k_cbtypes[t].name, st, U(GetParent)(ci.hwndList));
        }
    }
    U(DestroyWindow)(p);
}

TEST(combo_add_insert_delete)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    for (int t = 0; t < 3; t++) {
        HWND cb = mkcb(p, k_cbtypes[t].style, 10 + t);
        EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "one"), 0);
        EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "two"), 1);
        EXPECT_EQ(SA(cb, CB_INSERTSTRING, 0, "zero"), 0);
        EXPECT_EQ(SA(cb, CB_INSERTSTRING, -1, "last"), 3);
        EXPECT_EQ(SA(cb, CB_INSERTSTRING, 4, "end"), 4);
        EXPECT_EQ(SA(cb, CB_INSERTSTRING, 99, "bad"), CB_ERR);
        EXPECT_EQ(SA(cb, CB_GETCOUNT, 0, 0), 5);
        static const char *w[] = { "zero", "one", "two", "last", "end" };
        for (int i = 0; i < 5; i++) CHECKF(c_cbtext_is(cb, i, w[i]), "%s %d", k_cbtypes[t].name, i);
        EXPECT_EQ(SA(cb, CB_DELETESTRING, 0, 0), 4);
        EXPECT_EQ(SA(cb, CB_DELETESTRING, 4, 0), CB_ERR);
        EXPECT_EQ(SA(cb, CB_DELETESTRING, -1, 0), CB_ERR);
        EXPECT_EQ(SA(cb, CB_GETCOUNT, 0, 0), 4);
        SA(cb, CB_RESETCONTENT, 0, 0);
        EXPECT_EQ(SA(cb, CB_GETCOUNT, 0, 0), 0);
    }
    U(DestroyWindow)(p);
}

TEST(combo_lbtext_edges)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 10);
    SA(cb, CB_ADDSTRING, 0, "hello");
    SA(cb, CB_ADDSTRING, 0, "");
    char b[32];
    EXPECT_EQ(SA(cb, CB_GETLBTEXTLEN, 0, 0), 5);
    EXPECT_EQ(SA(cb, CB_GETLBTEXT, 0, b), 5);
    EXPECT_STR(b, "hello");
    EXPECT_EQ(SA(cb, CB_GETLBTEXTLEN, 1, 0), 0);
    EXPECT_EQ(SA(cb, CB_GETLBTEXT, 1, b), 0);
    EXPECT_EQ(b[0], 0);
    EXPECT_EQ(SA(cb, CB_GETLBTEXTLEN, 2, 0), CB_ERR);
    EXPECT_EQ(SA(cb, CB_GETLBTEXT, 2, b), CB_ERR);
    EXPECT_EQ(SA(cb, CB_GETLBTEXTLEN, -1, 0), CB_ERR);
    EXPECT_EQ(SA(cb, CB_GETLBTEXT, -1, b), CB_ERR);
    U(DestroyWindow)(p);
}

TEST(combo_unicode_items)
{
    REQUIRE(U(CreateWindowExW));
    HWND p = c_parent(300, 300);
    HWND cb = c_ctlw(p, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 11, 5, 5, 150, 200);
    static const WCHAR u[] = { 0x0416, 0x4E2D, 'q', 0 };
    CHECK(U(IsWindowUnicode)(cb));
    EXPECT_EQ(SWm(cb, CB_ADDSTRING, 0, u), 0);
    EXPECT_EQ(SWm(cb, CB_ADDSTRING, 0, L"plain"), 1);
    WCHAR w[16] = { 0 };
    EXPECT_EQ(SWm(cb, CB_GETLBTEXTLEN, 0, 0), 3);
    EXPECT_EQ(SWm(cb, CB_GETLBTEXT, 0, w), 3);
    CHECK(!lstrcmpW(w, u));
    EXPECT_EQ(SWm(cb, CB_FINDSTRINGEXACT, -1, u), 0);
    EXPECT_EQ(SWm(cb, CB_SETCURSEL, 0, 0), 0);
    memset(w, 0, sizeof w);
    EXPECT_EQ(U(GetWindowTextW)(cb, w, 16), 3);
    CHECK(!lstrcmpW(w, u));
    U(DestroyWindow)(p);
}

TEST(combo_setcursel_and_text)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    for (int t = 0; t < 3; t++) {
        HWND cb = mkcb(p, k_cbtypes[t].style, 10 + t);
        cb_fill(cb);
        c_logclear();
        EXPECT_EQ(SA(cb, CB_SETCURSEL, 2, 0), 2);
        EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), 2);
        if (t != 0) CHECKF(c_text_is(cb, "Charlie"), "%s", k_cbtypes[t].name);
        EXPECT_EQ(SA(cb, CB_SETCURSEL, 0, 0), 0);
        if (t != 0) CHECKF(c_text_is(cb, "Alpha"), "%s", k_cbtypes[t].name);
        /* out of range: error, selection unchanged */
        EXPECT_EQ(SA(cb, CB_SETCURSEL, NITEMS, 0), CB_ERR);
        EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), 0);
        /* -1 clears */
        EXPECT_EQ(SA(cb, CB_SETCURSEL, -1, 0), CB_ERR);
        EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), CB_ERR);
        if (t != 0) CHECKF(c_text_is(cb, ""), "%s cleared", k_cbtypes[t].name);
        EXPECT_EQ(c_nmsg(WM_COMMAND), 0);   /* programmatic selection does not notify */
    }
    U(DestroyWindow)(p);
}

TEST(combo_gettextlength_matches_selection)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 10);
    cb_fill(cb);
    EXPECT_EQ(SA(cb, WM_GETTEXTLENGTH, 0, 0), 0);
    SA(cb, CB_SETCURSEL, 2, 0);
    EXPECT_EQ(SA(cb, WM_GETTEXTLENGTH, 0, 0), 7);
    char b[16]; memset(b, 'x', sizeof b);
    EXPECT_EQ(SA(cb, WM_GETTEXT, 16, b), 7);
    EXPECT_STR(b, "Charlie");
    t_note("combo WM_GETTEXT(4) of \"Charlie\" -> %lld", (long long)SA(cb, WM_GETTEXT, 4, b));
    U(DestroyWindow)(p);
}

TEST(combo_dropdown_settext_editable)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWN | CBS_AUTOHSCROLL, 10);
    cb_fill(cb);
    COMBOBOXINFO ci; cb_info(cb, &ci);
    EXPECT_EQ(SA(cb, WM_SETTEXT, 0, "free text"), TRUE);
    CHECK(c_text_is(cb, "free text"));
    CHECK(c_text_is(ci.hwndItem, "free text"));
    EXPECT_EQ(SA(cb, WM_GETTEXTLENGTH, 0, 0), 9);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), CB_ERR);
    /* text typed into the child edit */
    SA(ci.hwndItem, WM_SETTEXT, 0, "via edit");
    CHECK(c_text_is(cb, "via edit"));
    U(DestroyWindow)(p);
}

TEST(combo_edit_selection_messages)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND dd = mkcb(p, CBS_DROPDOWN | CBS_AUTOHSCROLL, 10);
    HWND dl = mkcb(p, CBS_DROPDOWNLIST, 11);
    SA(dd, WM_SETTEXT, 0, "0123456789");
    EXPECT_EQ(SA(dd, CB_SETEDITSEL, 0, MAKELPARAM(2, 6)), TRUE);
    DWORD s = 99, e = 99;
    LRESULT r = SA(dd, CB_GETEDITSEL, &s, &e);
    EXPECT_EQ(s, 2); EXPECT_EQ(e, 6);
    EXPECT_EQ(LOWORD(r), 2); EXPECT_EQ(HIWORD(r), 6);
    SA(dd, CB_SETEDITSEL, 0, MAKELPARAM(0, -1 & 0xFFFF));
    r = SA(dd, CB_GETEDITSEL, 0, 0);
    EXPECT_EQ(LOWORD(r), 0);
    /* selection is shared with the edit child */
    COMBOBOXINFO ci; cb_info(dd, &ci);
    SA(dd, CB_SETEDITSEL, 0, MAKELPARAM(3, 4));
    CHECK(c_sel_is(ci.hwndItem, 3, 4));
    /* no edit control in a drop-down list */
    EXPECT_EQ(SA(dl, CB_SETEDITSEL, 0, MAKELPARAM(0, 1)), CB_ERR);
    EXPECT_EQ(SA(dl, CB_GETEDITSEL, 0, 0), CB_ERR);
    U(DestroyWindow)(p);
}

TEST(combo_limittext)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND dd = mkcb(p, CBS_DROPDOWN | CBS_AUTOHSCROLL, 10);
    COMBOBOXINFO ci; cb_info(dd, &ci);
    LRESULT def = SA(ci.hwndItem, EM_GETLIMITTEXT, 0, 0);
    t_note("default combo edit limit %lld", (long long)def);
    EXPECT_EQ(SA(dd, CB_LIMITTEXT, 5, 0), 1);
    EXPECT_EQ(SA(ci.hwndItem, EM_GETLIMITTEXT, 0, 0), 5);
    for (const char *c = "abcdefgh"; *c; c++) SA(ci.hwndItem, WM_CHAR, *c, 1);
    CHECK(c_text_is(ci.hwndItem, "abcde"));
    HWND dl = mkcb(p, CBS_DROPDOWNLIST, 11);
    LRESULT r = SA(dl, CB_LIMITTEXT, 5, 0);
    t_note("CB_LIMITTEXT on DROPDOWNLIST -> %lld", (long long)r);
    U(DestroyWindow)(p);
}

TEST(combo_find_string)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 10);
    cb_fill(cb);   /* Alpha Bravo Charlie alpine Delta */
    EXPECT_EQ(SA(cb, CB_FINDSTRING, -1, "al"), 0);
    EXPECT_EQ(SA(cb, CB_FINDSTRING, 0, "al"), 3);
    EXPECT_EQ(SA(cb, CB_FINDSTRING, 3, "al"), 0);
    EXPECT_EQ(SA(cb, CB_FINDSTRING, -1, "CHAR"), 2);
    EXPECT_EQ(SA(cb, CB_FINDSTRING, -1, "x"), CB_ERR);
    EXPECT_EQ(SA(cb, CB_FINDSTRING, -1, "Alphabet"), CB_ERR);
    EXPECT_EQ(SA(cb, CB_FINDSTRINGEXACT, -1, "bravo"), 1);
    EXPECT_EQ(SA(cb, CB_FINDSTRINGEXACT, -1, "brav"), CB_ERR);
    EXPECT_EQ(SA(cb, CB_FINDSTRINGEXACT, 1, "bravo"), 1);
    EXPECT_EQ(SA(cb, CB_FINDSTRINGEXACT, -1, "ALPINE"), 3);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), CB_ERR);   /* find does not select */
    U(DestroyWindow)(p);
}

TEST(combo_select_string)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWN | CBS_AUTOHSCROLL, 10);
    cb_fill(cb);
    EXPECT_EQ(SA(cb, CB_SELECTSTRING, -1, "ch"), 2);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), 2);
    CHECK(c_text_is(cb, "Charlie"));
    EXPECT_EQ(SA(cb, CB_SELECTSTRING, 2, "al"), 3);
    CHECK(c_text_is(cb, "alpine"));
    EXPECT_EQ(SA(cb, CB_SELECTSTRING, -1, "nothing"), CB_ERR);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), 3);
    U(DestroyWindow)(p);
}

TEST(combo_sorted)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST | CBS_SORT, 10);
    EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "pear"), 0);
    EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "apple"), 0);
    EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "mango"), 1);
    EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "zebra"), 3);
    static const char *w[] = { "apple", "mango", "pear", "zebra" };
    for (int i = 0; i < 4; i++) CHECKF(c_cbtext_is(cb, i, w[i]), "%d", i);
    EXPECT_EQ(SA(cb, CB_INSERTSTRING, 0, "zzz"), 0);   /* insert ignores sorting */
    CHECK(c_cbtext_is(cb, 0, "zzz"));
    U(DestroyWindow)(p);
}

TEST(combo_itemdata)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 10);
    cb_fill(cb);
    for (int i = 0; i < NITEMS; i++) EXPECT_EQ(SA(cb, CB_GETITEMDATA, i, 0), 0);
    for (int i = 0; i < NITEMS; i++) CHECK(SA(cb, CB_SETITEMDATA, i, 7000 + i) != CB_ERR);
    for (int i = 0; i < NITEMS; i++) EXPECT_EQ(SA(cb, CB_GETITEMDATA, i, 0), 7000 + i);
    SetLastError(0);
    EXPECT_EQ(SA(cb, CB_SETITEMDATA, NITEMS, 1), CB_ERR);
    EXPECT_EQ(SA(cb, CB_GETITEMDATA, NITEMS, 0), CB_ERR);
    EXPECT_EQ(SA(cb, CB_GETITEMDATA, -1, 0), CB_ERR);
    LPARAM big = (LPARAM)0x0123456789ABCDEFLL;
    SA(cb, CB_SETITEMDATA, 1, big);
    EXPECT_EQ(SA(cb, CB_GETITEMDATA, 1, 0), big);
    SA(cb, CB_DELETESTRING, 0, 0);
    EXPECT_EQ(SA(cb, CB_GETITEMDATA, 0, 0), big);
    U(DestroyWindow)(p);
}

TEST(combo_extended_ui)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND dd = mkcb(p, CBS_DROPDOWN, 10), dl = mkcb(p, CBS_DROPDOWNLIST, 11), sm = mkcb(p, CBS_SIMPLE, 12);
    EXPECT_EQ(SA(dd, CB_GETEXTENDEDUI, 0, 0), FALSE);
    EXPECT_EQ(SA(dd, CB_SETEXTENDEDUI, TRUE, 0), CB_OKAY);
    EXPECT_EQ(SA(dd, CB_GETEXTENDEDUI, 0, 0), TRUE);
    EXPECT_EQ(SA(dd, CB_SETEXTENDEDUI, FALSE, 0), CB_OKAY);
    EXPECT_EQ(SA(dd, CB_GETEXTENDEDUI, 0, 0), FALSE);
    EXPECT_EQ(SA(dl, CB_SETEXTENDEDUI, TRUE, 0), CB_OKAY);
    EXPECT_EQ(SA(dl, CB_GETEXTENDEDUI, 0, 0), TRUE);
    /* only 0 and 1 are accepted */
    SetLastError(0);
    EXPECT_EQ(SA(dl, CB_SETEXTENDEDUI, 5, 0), CB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(SA(dl, CB_GETEXTENDEDUI, 0, 0), TRUE);   /* unchanged */
    /* not supported for simple combo boxes */
    SetLastError(0);
    EXPECT_EQ(SA(sm, CB_SETEXTENDEDUI, TRUE, 0), CB_ERR);
    t_note("CB_SETEXTENDEDUI on simple combo: err=%lu", GetLastError());
    EXPECT_EQ(SA(sm, CB_GETEXTENDEDUI, 0, 0), FALSE);
    U(DestroyWindow)(p);
}

TEST(combo_item_heights)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 10);
    cb_fill(cb);
    LRESULT sel = SA(cb, CB_GETITEMHEIGHT, -1, 0);
    LRESULT lst = SA(cb, CB_GETITEMHEIGHT, 0, 0);
    CHECK(sel > 4 && sel < 100);
    CHECK(lst > 4 && lst < 100);
    t_note("selection-field height %lld, list item height %lld", (long long)sel, (long long)lst);
    t_note("CB_SETITEMHEIGHT(-1,30) -> %lld", (long long)SA(cb, CB_SETITEMHEIGHT, -1, 30));
    EXPECT_EQ(SA(cb, CB_GETITEMHEIGHT, -1, 0), 30);
    EXPECT_EQ(SA(cb, CB_GETITEMHEIGHT, 0, 0), lst);   /* list items unaffected */
    SA(cb, CB_SETITEMHEIGHT, -1, 20);
    EXPECT_EQ(SA(cb, CB_GETITEMHEIGHT, -1, 0), 20);
    t_note("CB_SETITEMHEIGHT(-1,256) -> %lld", (long long)SA(cb, CB_SETITEMHEIGHT, -1, 256));
    /* owner-draw fixed combo: list item height can be set for all items */
    HWND od = mkcb(p, CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 11);
    cb_fill(od);
    EXPECT_EQ(SA(od, CB_SETITEMHEIGHT, 0, 24), CB_OKAY);
    EXPECT_EQ(SA(od, CB_GETITEMHEIGHT, 0, 0), 24);
    EXPECT_EQ(SA(od, CB_GETITEMHEIGHT, 3, 0), 24);
    EXPECT_EQ(SA(od, CB_SETITEMHEIGHT, 0, 256), CB_ERR);
    EXPECT_EQ(SA(od, CB_SETITEMHEIGHT, 0, 255), CB_OKAY);
    EXPECT_EQ(SA(od, CB_GETITEMHEIGHT, 0, 0), 255);
    U(DestroyWindow)(p);
}

TEST(combo_dropped_width_extent_topindex)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 10);
    cb_fill(cb);
    LRESULT w0 = SA(cb, CB_GETDROPPEDWIDTH, 0, 0);
    CHECK(w0 > 0);
    RECT wr; U(GetWindowRect)(cb, &wr);
    EXPECT_EQ(w0, wr.right - wr.left);
    EXPECT_EQ(SA(cb, CB_SETDROPPEDWIDTH, 300, 0), 300);
    EXPECT_EQ(SA(cb, CB_GETDROPPEDWIDTH, 0, 0), 300);
    LRESULT r = SA(cb, CB_SETDROPPEDWIDTH, 10, 0);
    EXPECT_EQ(r, 150);    /* never narrower than the combo box itself */
    EXPECT_EQ(SA(cb, CB_GETDROPPEDWIDTH, 0, 0), 150);
    SA(cb, CB_SETDROPPEDWIDTH, 300, 0);
    EXPECT_EQ(SA(cb, CB_GETHORIZONTALEXTENT, 0, 0), 0);
    SA(cb, CB_SETHORIZONTALEXTENT, 400, 0);
    EXPECT_EQ(SA(cb, CB_GETHORIZONTALEXTENT, 0, 0), 400);
    for (int i = 0; i < 40; i++) SA(cb, CB_ADDSTRING, 0, "more");
    EXPECT_EQ(SA(cb, CB_GETTOPINDEX, 0, 0), 0);
    EXPECT_EQ(SA(cb, CB_SETTOPINDEX, 2, 0), 0);
    EXPECT_EQ(SA(cb, CB_GETTOPINDEX, 0, 0), 2);
    EXPECT_EQ(SA(cb, CB_SETTOPINDEX, 0, 0), 0);
    EXPECT_EQ(SA(cb, CB_SETTOPINDEX, 100, 0), CB_ERR);
    /* dropped width is only available with a drop-down button */
    HWND sm = mkcb(p, CBS_SIMPLE, 12);
    EXPECT_EQ(SA(sm, CB_GETDROPPEDWIDTH, 0, 0), CB_ERR);
    EXPECT_EQ(SA(sm, CB_SETDROPPEDWIDTH, 200, 0), CB_ERR);
    EXPECT_EQ(SA(cb, CB_SETDROPPEDWIDTH, 0, 0), 300);   /* 0 leaves it unchanged and returns it */
    U(DestroyWindow)(p);
}

TEST(combo_locale_initstorage_count)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 10);
    LRESULT orig = SA(cb, CB_GETLOCALE, 0, 0);
    CHECK(orig != 0);
    EXPECT_EQ(SA(cb, CB_SETLOCALE, 0x0409, 0), orig);
    EXPECT_EQ(SA(cb, CB_GETLOCALE, 0, 0), 0x0409);
    SA(cb, CB_SETLOCALE, orig, 0);
    LRESULT r = SA(cb, CB_INITSTORAGE, 500, 500 * 16);
    CHECK(r >= 500 || r == CB_ERRSPACE);
    for (int i = 0; i < 500; i++) SA(cb, CB_ADDSTRING, 0, "stored");
    EXPECT_EQ(SA(cb, CB_GETCOUNT, 0, 0), 500);
    U(DestroyWindow)(p);
}

/* ---- GetComboBoxInfo ---- */
TEST(combo_getcomboboxinfo_api)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetComboBoxInfo));
    HWND p = c_parent(300, 300);
    for (int t = 0; t < 3; t++) {
        HWND cb = mkcb(p, k_cbtypes[t].style, 10 + t);
        COMBOBOXINFO a, b;
        memset(&a, 0xCC, sizeof a); a.cbSize = sizeof a;
        CHECKF(U(GetComboBoxInfo)(cb, &a), "%s", k_cbtypes[t].name);
        CHECK(cb_info(cb, &b));
        CHECK(a.hwndCombo == cb && a.hwndCombo == b.hwndCombo);
        CHECK(a.hwndItem == b.hwndItem && a.hwndList == b.hwndList);
        CHECK(a.stateButton == b.stateButton);
        CHECK(EqualRect(&a.rcItem, &b.rcItem) && EqualRect(&a.rcButton, &b.rcButton));
        CHECK(a.cbSize == sizeof a);
        if (t != 0) {
            EXPECT_EQ(a.stateButton, 0);
            RECT cl; U(GetClientRect)(cb, &cl);
            CHECK(a.rcButton.right <= cl.right && a.rcButton.left > 0 && a.rcButton.left < a.rcButton.right);
            CHECK(a.rcItem.right <= a.rcButton.left);
            CHECK(a.rcItem.left >= 0 && a.rcItem.top >= 0 && a.rcItem.bottom <= cl.bottom);
            t_note("%s: item {%ld,%ld,%ld,%ld} button {%ld,%ld,%ld,%ld} client %ldx%ld", k_cbtypes[t].name,
                   a.rcItem.left, a.rcItem.top, a.rcItem.right, a.rcItem.bottom,
                   a.rcButton.left, a.rcButton.top, a.rcButton.right, a.rcButton.bottom, cl.right, cl.bottom);
        } else {
            t_note("simple: stateButton=0x%lx button {%ld,%ld,%ld,%ld}", a.stateButton, a.rcButton.left, a.rcButton.top, a.rcButton.right, a.rcButton.bottom);
        }
    }
    U(DestroyWindow)(p);
}

TEST(combo_getcomboboxinfo_invalid)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetComboBoxInfo));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWN, 10);
    HWND bt = c_ctl(p, "Button", "b", BS_PUSHBUTTON, 11, 0, 0, 40, 20);
    HWND ed = c_ctl(p, "Edit", "e", 0, 12, 0, 0, 40, 20);
    COMBOBOXINFO ci;
    memset(&ci, 0, sizeof ci); ci.cbSize = sizeof ci;
    CHECK(!U(GetComboBoxInfo)(bt, &ci));
    CHECK(!U(GetComboBoxInfo)(ed, &ci));
    CHECK(!U(GetComboBoxInfo)(p, &ci));
    CHECK(!U(GetComboBoxInfo)(NULL, &ci));
    CHECK(!U(GetComboBoxInfo)((HWND)(ULONG_PTR)0xdead0, &ci));
    /* wrong cbSize */
    ci.cbSize = sizeof ci - 4;
    SetLastError(0);
    t_note("GetComboBoxInfo(cbSize-4) -> %d err=%lu", U(GetComboBoxInfo)(cb, &ci), GetLastError());
    ci.cbSize = 0;
    t_note("GetComboBoxInfo(cbSize=0) -> %d", U(GetComboBoxInfo)(cb, &ci));
    ci.cbSize = sizeof ci + 8;
    t_note("GetComboBoxInfo(cbSize+8) -> %d", U(GetComboBoxInfo)(cb, &ci));
    ci.cbSize = sizeof ci;
    CHECK(U(GetComboBoxInfo)(cb, &ci));
    /* message form with a stale handle */
    U(DestroyWindow)(cb);
    CHECK(!U(GetComboBoxInfo)(cb, &ci));
    U(DestroyWindow)(p);
}

TEST(combo_window_size_dropdown_vs_simple)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowRect));
    HWND p = c_parent(300, 300);
    HWND dd = c_ctl(p, "ComboBox", "", CBS_DROPDOWN, 1, 0, 0, 150, 200);
    HWND sm = c_ctl(p, "ComboBox", "", CBS_SIMPLE, 2, 0, 0, 150, 200);
    RECT a, b;
    U(GetWindowRect)(dd, &a); U(GetWindowRect)(sm, &b);
    t_note("dropdown created with height 200 -> window height %ld; simple -> %ld", a.bottom - a.top, b.bottom - b.top);
    CHECK(a.bottom - a.top < 100);          /* closed height only */
    CHECK(b.bottom - b.top >= a.bottom - a.top);
    EXPECT_EQ(a.right - a.left, 150);
    EXPECT_EQ(b.right - b.left, 150);
    cb_fill(dd);
    RECT d; EXPECT_EQ(SA(dd, CB_GETDROPPEDCONTROLRECT, 0, &d) != CB_ERR, 1);
    CHECK(d.left == a.left && d.top == a.top);
    CHECK(d.right - d.left >= 150);
    CHECK(d.bottom - d.top > a.bottom - a.top);
    U(DestroyWindow)(p);
}

/* ---- drop-down behaviour (needs a shown parent) ---- */
TEST(combo_showdropdown_state_and_notifications)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent_shown(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 20);
    cb_fill(cb);
    COMBOBOXINFO ci;
    EXPECT_EQ(SA(cb, CB_GETDROPPEDSTATE, 0, 0), FALSE);
    c_logclear();
    EXPECT_EQ(SA(cb, CB_SHOWDROPDOWN, TRUE, 0), TRUE);
    EXPECT_EQ(SA(cb, CB_GETDROPPEDSTATE, 0, 0), TRUE);
    EXPECT_EQ(c_ncmd(20, CBN_DROPDOWN), 1);
    cb_info(cb, &ci);
    t_note("stateButton while dropped = 0x%lx", ci.stateButton);
    CHECK(U(IsWindowVisible)(ci.hwndList));
    RECT lr, dr; U(GetWindowRect)(ci.hwndList, &lr);
    SA(cb, CB_GETDROPPEDCONTROLRECT, 0, &dr);
    CHECK(dr.bottom >= lr.bottom);
    /* showing again is a no-op */
    EXPECT_EQ(SA(cb, CB_SHOWDROPDOWN, TRUE, 0), TRUE);
    EXPECT_EQ(c_ncmd(20, CBN_DROPDOWN), 1);
    c_logclear();
    EXPECT_EQ(SA(cb, CB_SHOWDROPDOWN, FALSE, 0), TRUE);
    EXPECT_EQ(SA(cb, CB_GETDROPPEDSTATE, 0, 0), FALSE);
    EXPECT_EQ(c_ncmd(20, CBN_CLOSEUP), 1);
    cb_info(cb, &ci);
    EXPECT_EQ(ci.stateButton & STATE_SYSTEM_PRESSED, 0);
    CHECK(!U(IsWindowVisible)(ci.hwndList));
    t_note("CBN_SELENDCANCEL/OK on programmatic close: %d/%d", c_ncmd(20, CBN_SELENDCANCEL), c_ncmd(20, CBN_SELENDOK));
    U(DestroyWindow)(p);
}

TEST(combo_showdropdown_editable_and_simple)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent_shown(300, 300);
    HWND dd = mkcb(p, CBS_DROPDOWN | CBS_AUTOHSCROLL, 21);
    HWND sm = mkcb(p, CBS_SIMPLE, 22);
    cb_fill(dd); cb_fill(sm);
    EXPECT_EQ(SA(dd, CB_SHOWDROPDOWN, TRUE, 0), TRUE);
    EXPECT_EQ(SA(dd, CB_GETDROPPEDSTATE, 0, 0), TRUE);
    SA(dd, CB_SHOWDROPDOWN, FALSE, 0);
    EXPECT_EQ(SA(dd, CB_GETDROPPEDSTATE, 0, 0), FALSE);
    /* simple combo: the list is always shown; SHOWDROPDOWN is meaningless */
    LRESULT r = SA(sm, CB_SHOWDROPDOWN, TRUE, 0);
    t_note("simple: SHOWDROPDOWN -> %lld dropped=%lld", (long long)r, (long long)SA(sm, CB_GETDROPPEDSTATE, 0, 0));
    /* Win10 22H2 oracle: CB_SHOWDROPDOWN on a simple combo returns 1 and
     * CB_GETDROPPEDSTATE then reports TRUE (the list is always shown); the
     * state is recorded rather than asserted. */
    COMBOBOXINFO ci; cb_info(sm, &ci);
    CHECK(U(IsWindowVisible)(ci.hwndList));
    U(DestroyWindow)(p);
}

TEST(combo_destroy_while_dropped)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(IsWindow));
    HWND p = c_parent_shown(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 23);
    cb_fill(cb);
    COMBOBOXINFO ci; cb_info(cb, &ci);
    SA(cb, CB_SHOWDROPDOWN, TRUE, 0);
    CHECK(U(IsWindowVisible)(ci.hwndList));
    CHECK(U(DestroyWindow)(cb));
    CHECK(!U(IsWindow)(cb));
    CHECK(!U(IsWindow)(ci.hwndList));
    U(DestroyWindow)(p);
}

/* ---- edit notifications ---- */
TEST(combo_edit_notifications)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWN | CBS_AUTOHSCROLL, 24);
    COMBOBOXINFO ci; cb_info(cb, &ci);
    c_logclear();
    SA(ci.hwndItem, EM_REPLACESEL, FALSE, "abc");
    EXPECT_EQ(c_ncmd(24, CBN_EDITUPDATE), 1);
    EXPECT_EQ(c_ncmd(24, CBN_EDITCHANGE), 1);
    CHECK(c_first_cmd(24, CBN_EDITUPDATE) < c_first_cmd(24, CBN_EDITCHANGE));
    int i = c_first_cmd(24, CBN_EDITCHANGE);
    CHECK(i >= 0 && c_log[i].lp == (LPARAM)cb);
    CHECK(c_text_is(cb, "abc"));
    c_logclear();
    SA(cb, WM_SETTEXT, 0, "xyz");
    t_note("combo WM_SETTEXT notifications: update=%d change=%d", c_ncmd(24, CBN_EDITUPDATE), c_ncmd(24, CBN_EDITCHANGE));
    U(DestroyWindow)(p);
}

TEST(combo_keyboard_selection_notifies)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 25);
    cb_fill(cb);
    SA(cb, CB_SETCURSEL, 0, 0);
    c_logclear();
    SA(cb, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), 1);
    EXPECT_EQ(c_ncmd(25, CBN_SELCHANGE), 1);
    CHECK(c_text_is(cb, "Bravo"));
    SA(cb, WM_KEYDOWN, VK_UP, 0);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), 0);
    SA(cb, WM_KEYDOWN, VK_UP, 0);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), 0);
    SA(cb, WM_KEYDOWN, VK_END, 0);
    t_note("VK_END on closed dropdown list -> sel %lld", (long long)SA(cb, CB_GETCURSEL, 0, 0));
    SA(cb, CB_SETCURSEL, 0, 0);
    c_logclear();
    SA(cb, WM_CHAR, 'c', 1);
    EXPECT_EQ(SA(cb, CB_GETCURSEL, 0, 0), 2);
    EXPECT_EQ(c_ncmd(25, CBN_SELCHANGE), 1);
    U(DestroyWindow)(p);
}

/* ---- owner draw ---- */
TEST(combo_ownerdraw_fixed_measure)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    c_logclear();
    c_mi_base = 22; c_mi_step = 0;
    HWND cb = mkcb(p, CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 26);
    c_mi_base = 0;
    CHECK(cb != NULL);
    int n = c_nmsg(WM_MEASUREITEM);
    t_note("ownerdraw fixed combo: WM_MEASUREITEM at create = %d", n);
    CHECK(n >= 1);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_MEASUREITEM) {
        EXPECT_EQ(c_log[i].mi.CtlType, ODT_COMBOBOX);
        EXPECT_EQ(c_log[i].mi.CtlID, 26);
        break;
    }
    EXPECT_EQ(SA(cb, CB_GETITEMHEIGHT, 0, 0), 22);
    U(DestroyWindow)(p);
}

TEST(combo_ownerdraw_printclient_selection_field)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 27);
    cb_fill(cb);
    SA(cb, CB_SETCURSEL, 3, 0);
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bm = CreateBitmap(300, 300, 1, 1, NULL);
    SelectObject(dc, bm);
    c_logclear();
    SA(cb, WM_PRINTCLIENT, dc, PRF_CLIENT);
    int found = 0;
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_DRAWITEM) {
        DRAWITEMSTRUCT *d = &c_log[i].di;
        if (d->CtlType == ODT_COMBOBOX && d->CtlID == 27) {
            found++;
            EXPECT_EQ(d->hwndItem, cb);
            EXPECT_EQ(d->itemID, 3);
            CHECK(d->itemState & ODS_COMBOBOXEDIT);
            break;
        }
    }
    t_note("combo ownerdraw selection-field draw found=%d", found);
    DeleteDC(dc); DeleteObject(bm);
    U(DestroyWindow)(p);
}

TEST(combo_ownerdraw_deleteitem_and_compare)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED, 28);   /* data-only items */
    HWND sc = mkcb(p, CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_SORT, 29);
    SA(cb, CB_ADDSTRING, 0, 111); SA(cb, CB_ADDSTRING, 0, 222); SA(cb, CB_ADDSTRING, 0, 333);
    EXPECT_EQ(SA(cb, CB_GETITEMDATA, 1, 0), 222);
    c_logclear();
    SA(cb, CB_DELETESTRING, 1, 0);
    EXPECT_EQ(c_nmsg(WM_DELETEITEM), 1);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_DELETEITEM) {
        EXPECT_EQ(c_log[i].de.CtlType, ODT_COMBOBOX);
        EXPECT_EQ(c_log[i].de.CtlID, 28);
        EXPECT_EQ(c_log[i].de.itemData, 222);
    }
    c_logclear();
    SA(cb, CB_RESETCONTENT, 0, 0);
    EXPECT_EQ(c_nmsg(WM_DELETEITEM), 2);
    c_logclear();
    SA(sc, CB_ADDSTRING, 0, 30); SA(sc, CB_ADDSTRING, 0, 10); SA(sc, CB_ADDSTRING, 0, 20);
    CHECK(c_nmsg(WM_COMPAREITEM) >= 2);
    EXPECT_EQ(SA(sc, CB_GETITEMDATA, 0, 0), 10);
    EXPECT_EQ(SA(sc, CB_GETITEMDATA, 1, 0), 20);
    EXPECT_EQ(SA(sc, CB_GETITEMDATA, 2, 0), 30);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_COMPAREITEM) {
        EXPECT_EQ(c_log[i].ci.CtlType, ODT_COMBOBOX);
        EXPECT_EQ(c_log[i].ci.CtlID, 29);
        break;
    }
    U(DestroyWindow)(p);
}

/* ---- directory listing / dlg helpers ---- */
TEST(combo_dir_drives_and_dlgdirselect)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(DlgDirSelectComboBoxExA));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWNLIST, 30);
    LRESULT r = SA(cb, CB_DIR, DDL_DRIVES | DDL_EXCLUSIVE, "*");
    CHECK(r != CB_ERR);
    LRESULT n = SA(cb, CB_GETCOUNT, 0, 0);
    CHECK(n >= 1);
    char b[32];
    for (int i = 0; i < n; i++) {
        b[0] = 0; SA(cb, CB_GETLBTEXT, i, b);
        CHECKF(strlen(b) == 5 && b[0] == '[' && b[1] == '-' && b[3] == '-' && b[4] == ']', "entry \"%s\"", b);
    }
    SA(cb, CB_SETCURSEL, 0, 0);
    char out[MAX_PATH]; memset(out, 'x', sizeof out);
    CHECK(U(DlgDirSelectComboBoxExA)(p, out, MAX_PATH, 30));
    CHECK(strlen(out) >= 2 && out[1] == ':');
    t_note("DlgDirSelectComboBoxEx drive -> \"%s\"", out);
    U(DestroyWindow)(p);
}

TEST(combo_destroyed_handle)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(IsWindow));
    HWND p = c_parent(300, 300);
    HWND cb = mkcb(p, CBS_DROPDOWN, 31);
    cb_fill(cb);
    U(DestroyWindow)(cb);
    EXPECT_EQ(SA(cb, CB_GETCOUNT, 0, 0), 0);
    EXPECT_EQ(SA(cb, CB_ADDSTRING, 0, "x"), 0);
    COMBOBOXINFO ci; memset(&ci, 0, sizeof ci); ci.cbSize = sizeof ci;
    EXPECT_EQ(SA(cb, CB_GETCOMBOBOXINFO, 0, &ci), 0);
    U(DestroyWindow)(p);
}
