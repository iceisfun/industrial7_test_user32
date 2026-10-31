#include "ctl.h"

#define LB_STD (LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL)

static HWND mklb(HWND p, DWORD style, int id)
{
    return c_ctl(p, "ListBox", "", style, id, 2, 2, 120, 100);
}
static void fill(HWND lb, const char *const *s, int n)
{
    for (int i = 0; i < n; i++) SA(lb, LB_ADDSTRING, 0, s[i]);
}
static const char *const k_fruit[] = { "Apple", "apricot", "Banana", "avocado", "Cherry", "date" };
#define NFRUIT 6

TEST(listbox_constants)
{
    EXPECT_EQ(LB_ERR, -1); EXPECT_EQ(LB_ERRSPACE, -2); EXPECT_EQ(LB_OKAY, 0);
    EXPECT_EQ(LB_ADDSTRING, 0x180); EXPECT_EQ(LB_INSERTSTRING, 0x181); EXPECT_EQ(LB_DELETESTRING, 0x182);
    EXPECT_EQ(LB_SELITEMRANGEEX, 0x183); EXPECT_EQ(LB_RESETCONTENT, 0x184); EXPECT_EQ(LB_SETSEL, 0x185);
    EXPECT_EQ(LB_SETCURSEL, 0x186); EXPECT_EQ(LB_GETSEL, 0x187); EXPECT_EQ(LB_GETCURSEL, 0x188);
    EXPECT_EQ(LB_GETTEXT, 0x189); EXPECT_EQ(LB_GETTEXTLEN, 0x18A); EXPECT_EQ(LB_GETCOUNT, 0x18B);
    EXPECT_EQ(LB_SELECTSTRING, 0x18C); EXPECT_EQ(LB_DIR, 0x18D); EXPECT_EQ(LB_GETTOPINDEX, 0x18E);
    EXPECT_EQ(LB_FINDSTRING, 0x18F); EXPECT_EQ(LB_GETSELCOUNT, 0x190); EXPECT_EQ(LB_GETSELITEMS, 0x191);
    EXPECT_EQ(LB_SETTABSTOPS, 0x192); EXPECT_EQ(LB_GETHORIZONTALEXTENT, 0x193);
    EXPECT_EQ(LB_SETHORIZONTALEXTENT, 0x194); EXPECT_EQ(LB_SETCOLUMNWIDTH, 0x195);
    EXPECT_EQ(LB_ADDFILE, 0x196); EXPECT_EQ(LB_SETTOPINDEX, 0x197); EXPECT_EQ(LB_GETITEMRECT, 0x198);
    EXPECT_EQ(LB_GETITEMDATA, 0x199); EXPECT_EQ(LB_SETITEMDATA, 0x19A); EXPECT_EQ(LB_SELITEMRANGE, 0x19B);
    EXPECT_EQ(LB_SETANCHORINDEX, 0x19C); EXPECT_EQ(LB_GETANCHORINDEX, 0x19D); EXPECT_EQ(LB_SETCARETINDEX, 0x19E);
    EXPECT_EQ(LB_GETCARETINDEX, 0x19F); EXPECT_EQ(LB_SETITEMHEIGHT, 0x1A0); EXPECT_EQ(LB_GETITEMHEIGHT, 0x1A1);
    EXPECT_EQ(LB_FINDSTRINGEXACT, 0x1A2); EXPECT_EQ(LB_SETLOCALE, 0x1A5); EXPECT_EQ(LB_GETLOCALE, 0x1A6);
    EXPECT_EQ(LB_SETCOUNT, 0x1A7); EXPECT_EQ(LB_INITSTORAGE, 0x1A8); EXPECT_EQ(LB_ITEMFROMPOINT, 0x1A9);
    EXPECT_EQ(LB_GETLISTBOXINFO, 0x1B2);
    EXPECT_EQ(LBN_ERRSPACE, -2); EXPECT_EQ(LBN_SELCHANGE, 1); EXPECT_EQ(LBN_DBLCLK, 2);
    EXPECT_EQ(LBN_SELCANCEL, 3); EXPECT_EQ(LBN_SETFOCUS, 4); EXPECT_EQ(LBN_KILLFOCUS, 5);
    EXPECT_EQ(LBS_NOTIFY, 1); EXPECT_EQ(LBS_SORT, 2); EXPECT_EQ(LBS_NOREDRAW, 4); EXPECT_EQ(LBS_MULTIPLESEL, 8);
    EXPECT_EQ(LBS_OWNERDRAWFIXED, 0x10); EXPECT_EQ(LBS_OWNERDRAWVARIABLE, 0x20); EXPECT_EQ(LBS_HASSTRINGS, 0x40);
    EXPECT_EQ(LBS_USETABSTOPS, 0x80); EXPECT_EQ(LBS_NOINTEGRALHEIGHT, 0x100); EXPECT_EQ(LBS_MULTICOLUMN, 0x200);
    EXPECT_EQ(LBS_WANTKEYBOARDINPUT, 0x400); EXPECT_EQ(LBS_EXTENDEDSEL, 0x800); EXPECT_EQ(LBS_DISABLENOSCROLL, 0x1000);
    EXPECT_EQ(LBS_NODATA, 0x2000); EXPECT_EQ(LBS_NOSEL, 0x4000);
}

TEST(listbox_classname_and_empty_state)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 1);
    char n[16];
    CHECK(lb != NULL);
    c_classname(lb, n, sizeof n);
    EXPECT_STR(n, "ListBox");
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETTOPINDEX, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETTEXT, 0, n), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETTEXTLEN, 0, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_DELETESTRING, 0, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETSEL, 0, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 0, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_SETITEMDATA, 0, 5), LB_ERR);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "a"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "a"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_SELECTSTRING, -1, "a"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, 0, 0), LB_ERR);
    RECT r;
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 1, &r), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_SETITEMDATA, 0, 5), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 0, 0), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_DELETESTRING, 0, 0), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    EXPECT_EQ(SA(lb, LB_SETTOPINDEX, 0, 0), 0);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_SETTOPINDEX, 1, 0), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    U(DestroyWindow)(p);
}

TEST(listbox_add_insert_delete_basic)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 1);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "one"), 0);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "two"), 1);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "three"), 2);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 3);
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, 1, "ins"), 1);
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, 0, "head"), 0);
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, -1, "tail"), 5);
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, 6, "tail2"), 6);   /* index == count appends */
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 7);
    static const char *want[] = { "head", "one", "ins", "two", "three", "tail", "tail2" };
    // after the inserts above: head, one, ins, two, three, tail, tail2
    for (int i = 0; i < 7; i++) CHECKF(c_lbtext_is(lb, i, want[i]), "item %d", i);
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, 8, "bad"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, 100, "bad"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, -2, "bad"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 7);
    /* delete returns the remaining count */
    EXPECT_EQ(SA(lb, LB_DELETESTRING, 0, 0), 6);
    EXPECT_EQ(SA(lb, LB_DELETESTRING, 5, 0), 5);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_DELETESTRING, 5, 0), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    EXPECT_EQ(SA(lb, LB_DELETESTRING, -1, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_DELETESTRING, 1000, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 5);
    CHECK(c_lbtext_is(lb, 0, "one"));
    CHECK(c_lbtext_is(lb, 4, "tail"));
    SA(lb, LB_RESETCONTENT, 0, 0);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "again"), 0);
    U(DestroyWindow)(p);
}

TEST(listbox_gettext_buffer_cases)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 1);
    SA(lb, LB_ADDSTRING, 0, "hello");
    SA(lb, LB_ADDSTRING, 0, "");
    char b[32];
    memset(b, 'x', sizeof b);
    EXPECT_EQ(SA(lb, LB_GETTEXT, 0, b), 5);
    EXPECT_STR(b, "hello");
    EXPECT_EQ(SA(lb, LB_GETTEXTLEN, 0, 0), 5);
    EXPECT_EQ(SA(lb, LB_GETTEXT, 1, b), 0);
    EXPECT_EQ(b[0], 0);
    EXPECT_EQ(SA(lb, LB_GETTEXTLEN, 1, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETTEXT, 2, b), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETTEXTLEN, 2, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETTEXT, -1, b), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETTEXTLEN, -1, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETTEXTLEN, 0x7FFFFFFF, 0), LB_ERR);
    /* embedded long string */
    static char big[4001], out[4002];
    memset(big, 'L', 4000); big[4000] = 0;
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, big), 2);
    EXPECT_EQ(SA(lb, LB_GETTEXTLEN, 2, 0), 4000);
    EXPECT_EQ(SA(lb, LB_GETTEXT, 2, out), 4000);
    CHECK(!strcmp(out, big));
    U(DestroyWindow)(p);
}

TEST(listbox_unicode_strings)
{
    REQUIRE(U(CreateWindowExW));
    HWND p = c_parent(300, 300);
    HWND lb = c_ctlw(p, L"ListBox", L"", LB_STD, 1, 0, 0, 120, 100);
    static const WCHAR u1[] = { 0x0416, 0x4E2D, 'x', 0 }, u2[] = { 'p', 'l', 'a', 'i', 'n', 0 };
    CHECK(U(IsWindowUnicode)(lb));
    EXPECT_EQ(SWm(lb, LB_ADDSTRING, 0, u1), 0);
    EXPECT_EQ(SWm(lb, LB_ADDSTRING, 0, u2), 1);
    WCHAR w[16]; memset(w, 0, sizeof w);
    EXPECT_EQ(SWm(lb, LB_GETTEXTLEN, 0, 0), 3);
    EXPECT_EQ(SWm(lb, LB_GETTEXT, 0, w), 3);
    CHECK(!lstrcmpW(w, u1));
    EXPECT_EQ(SWm(lb, LB_FINDSTRINGEXACT, -1, u1), 0);
    EXPECT_EQ(SWm(lb, LB_FINDSTRING, -1, L"pla"), 1);
    /* ANSI access to the same item (ascii only) */
    char a[16];
    EXPECT_EQ(SA(lb, LB_GETTEXT, 1, a), 5);
    EXPECT_STR(a, "plain");
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "ansi"), 2);
    memset(w, 0, sizeof w);
    EXPECT_EQ(SWm(lb, LB_GETTEXT, 2, w), 4);
    CHECK(!lstrcmpW(w, L"ansi"));
    U(DestroyWindow)(p);
}

TEST(listbox_single_selection)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 5);
    fill(lb, k_fruit, NFRUIT);
    c_logclear();
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, 2, 0), 2);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 2);
    EXPECT_EQ(SA(lb, LB_GETSEL, 2, 0), 1);
    EXPECT_EQ(SA(lb, LB_GETSEL, 1, 0), 0);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, 4, 0), 4);
    EXPECT_EQ(SA(lb, LB_GETSEL, 2, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETSEL, 4, 0), 1);
    /* out of range: error, the existing selection is left alone */
    SA(lb, LB_SETCURSEL, 4, 0);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, NFRUIT, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 4);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, 1000, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 4);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, -2, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 4);
    /* -1 clears the selection (and, per the documentation, reports LB_ERR) */
    SA(lb, LB_SETCURSEL, 3, 0);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, -1, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETSEL, 3, 0), 0);
    /* LB_GETSEL range check */
    EXPECT_EQ(SA(lb, LB_GETSEL, NFRUIT, 0), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETSEL, -1, 0), LB_ERR);
    /* multi-selection-only messages fail on a single-selection list box */
    EXPECT_EQ(SA(lb, LB_SETSEL, TRUE, 1), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), LB_ERR);
    int buf[4];
    EXPECT_EQ(SA(lb, LB_GETSELITEMS, 4, buf), LB_ERR);
    EXPECT_EQ(SA(lb, LB_SELITEMRANGE, TRUE, MAKELPARAM(0, 2)), LB_ERR);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_SELITEMRANGEEX, 0, 2), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_LB_MESSAGE);
    /* programmatic selection never notifies */
    EXPECT_EQ(c_nmsg(WM_COMMAND), 0);
    /* selection follows deletion */
    SA(lb, LB_SETCURSEL, 3, 0);
    SA(lb, LB_DELETESTRING, 3, 0);
    t_note("selection after deleting the selected item: %lld", (long long)SA(lb, LB_GETCURSEL, 0, 0));
    U(DestroyWindow)(p);
}

TEST(listbox_selection_shifts_with_insert_delete)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 5);
    fill(lb, k_fruit, NFRUIT);
    SA(lb, LB_SETCURSEL, 3, 0);
    SA(lb, LB_DELETESTRING, 0, 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 2);
    SA(lb, LB_INSERTSTRING, 0, "new");
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 3);
    CHECK(c_lbtext_is(lb, 3, "avocado"));
    SA(lb, LB_RESETCONTENT, 0, 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), LB_ERR);
    U(DestroyWindow)(p);
}

TEST(listbox_multiple_selection)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_MULTIPLESEL, 6);
    fill(lb, k_fruit, NFRUIT);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_SETSEL, TRUE, 1), 0);
    EXPECT_EQ(SA(lb, LB_SETSEL, TRUE, 3), 0);
    EXPECT_EQ(SA(lb, LB_SETSEL, TRUE, 5), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 3);
    EXPECT_EQ(SA(lb, LB_GETSEL, 1, 0), 1);
    EXPECT_EQ(SA(lb, LB_GETSEL, 2, 0), 0);
    int items[8]; memset(items, 0xff, sizeof items);
    EXPECT_EQ(SA(lb, LB_GETSELITEMS, 8, items), 3);
    CHECK(items[0] == 1 && items[1] == 3 && items[2] == 5);
    CHECK(items[3] == -1);   /* nothing written beyond the count */
    memset(items, 0xff, sizeof items);
    EXPECT_EQ(SA(lb, LB_GETSELITEMS, 2, items), 2);
    CHECK(items[0] == 1 && items[1] == 3 && items[2] == -1);
    EXPECT_EQ(SA(lb, LB_GETSELITEMS, 0, items), 0);
    EXPECT_EQ(SA(lb, LB_GETSELITEMS, 1, items), 1);
    /* deselect */
    EXPECT_EQ(SA(lb, LB_SETSEL, FALSE, 3), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 2);
    /* toggling: SETSEL with wParam != 0 selects, 0 deselects */
    SA(lb, LB_SETSEL, 7, 0);
    EXPECT_EQ(SA(lb, LB_GETSEL, 0, 0), 1);
    /* -1 applies to every item */
    EXPECT_EQ(SA(lb, LB_SETSEL, TRUE, -1), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), NFRUIT);
    EXPECT_EQ(SA(lb, LB_SETSEL, FALSE, -1), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 0);
    /* out of range */
    EXPECT_EQ(SA(lb, LB_SETSEL, TRUE, NFRUIT), LB_ERR);
    EXPECT_EQ(SA(lb, LB_SETSEL, TRUE, 100), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETSEL, 100, 0), LB_ERR);
    /* LB_SETCURSEL is not allowed on multiple-selection list boxes */
    EXPECT_EQ(SA(lb, LB_SETCURSEL, 1, 0), LB_ERR);
    U(DestroyWindow)(p);
}

TEST(listbox_extended_selection_ranges)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_EXTENDEDSEL, 7);
    fill(lb, k_fruit, NFRUIT);
    EXPECT_EQ(SA(lb, LB_SELITEMRANGE, TRUE, MAKELPARAM(1, 3)), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 3);
    CHECK(SA(lb, LB_GETSEL, 1, 0) == 1 && SA(lb, LB_GETSEL, 3, 0) == 1 && SA(lb, LB_GETSEL, 0, 0) == 0 && SA(lb, LB_GETSEL, 4, 0) == 0);
    EXPECT_EQ(SA(lb, LB_SELITEMRANGE, FALSE, MAKELPARAM(2, 2)), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 2);
    EXPECT_EQ(SA(lb, LB_SELITEMRANGE, FALSE, MAKELPARAM(0, 5)), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 0);
    /* last beyond the end is clipped */
    EXPECT_EQ(SA(lb, LB_SELITEMRANGE, TRUE, MAKELPARAM(4, 100)), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 2);
    SA(lb, LB_SELITEMRANGE, FALSE, MAKELPARAM(0, 100));
    /* RANGEEX: first < last selects ; first > last deselects */
    EXPECT_EQ(SA(lb, LB_SELITEMRANGEEX, 0, 3), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 4);
    EXPECT_EQ(SA(lb, LB_SELITEMRANGEEX, 2, 1), 0);
    EXPECT_EQ(SA(lb, LB_GETSELCOUNT, 0, 0), 2);
    CHECK(SA(lb, LB_GETSEL, 0, 0) == 1 && SA(lb, LB_GETSEL, 1, 0) == 0 && SA(lb, LB_GETSEL, 2, 0) == 0 && SA(lb, LB_GETSEL, 3, 0) == 1);
    /* anchor and caret */
    EXPECT_EQ(SA(lb, LB_SETANCHORINDEX, 2, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETANCHORINDEX, 0, 0), 2);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_SETANCHORINDEX, NFRUIT, 0), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    EXPECT_EQ(SA(lb, LB_GETANCHORINDEX, 0, 0), 2);
    EXPECT_EQ(SA(lb, LB_SETCARETINDEX, 4, FALSE), 0);
    EXPECT_EQ(SA(lb, LB_GETCARETINDEX, 0, 0), 4);
    /* in a multi-select box LB_GETCURSEL returns the focus item */
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 4);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_SETCARETINDEX, NFRUIT, FALSE), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    U(DestroyWindow)(p);
}

TEST(listbox_noselection_style)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_NOSEL, 8);
    fill(lb, k_fruit, NFRUIT);
    LRESULT r = SA(lb, LB_SETCURSEL, 2, 0);
    t_note("LBS_NOSEL LB_SETCURSEL(2) -> %lld, GETCURSEL=%lld, GETSEL(2)=%lld", (long long)r,
           (long long)SA(lb, LB_GETCURSEL, 0, 0), (long long)SA(lb, LB_GETSEL, 2, 0));
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), NFRUIT);
    U(DestroyWindow)(p);
}

TEST(listbox_sorted)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_SORT, 9);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "delta"), 0);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "alpha"), 0);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "charlie"), 1);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "bravo"), 1);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "echo"), 4);
    static const char *want[] = { "alpha", "bravo", "charlie", "delta", "echo" };
    for (int i = 0; i < 5; i++) CHECKF(c_lbtext_is(lb, i, want[i]), "sorted %d", i);
    /* case-insensitive ordering for letters of different case */
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "Zulu"), 5);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "BETA"), 1);
    CHECK(c_lbtext_is(lb, 1, "BETA"));
    /* LB_INSERTSTRING ignores the sort order */
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, 0, "zzz-first"), 0);
    CHECK(c_lbtext_is(lb, 0, "zzz-first"));
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, -1, "aaa-last"), SA(lb, LB_GETCOUNT, 0, 0) - 1);
    U(DestroyWindow)(p);
}

TEST(listbox_sorted_many_unsorted_input)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_SORT, 9);
    char s[16];
    for (int i = 0; i < 500; i++) { sprintf(s, "k%04d", (i * 37) % 500); SA(lb, LB_ADDSTRING, 0, s); }
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 500);
    int ok = 1;
    for (int i = 0; i < 500; i++) { sprintf(s, "k%04d", i); if (!c_lbtext_is(lb, i, s)) { ok = 0; break; } }
    CHECK(ok);
    U(DestroyWindow)(p);
}

TEST(listbox_itemdata)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 10);
    fill(lb, k_fruit, 4);
    for (int i = 0; i < 4; i++) EXPECT_EQ(SA(lb, LB_GETITEMDATA, i, 0), 0);
    for (int i = 0; i < 4; i++) CHECK(SA(lb, LB_SETITEMDATA, i, 1000 + i) != LB_ERR);
    for (int i = 0; i < 4; i++) EXPECT_EQ(SA(lb, LB_GETITEMDATA, i, 0), 1000 + i);
    EXPECT_EQ(SA(lb, LB_SETITEMDATA, 0, 55), 1);   /* success value is exactly 1 */
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_SETITEMDATA, 4, 1), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 4, 0), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, -1, 0), LB_ERR);
    /* index -1 stores the value into every item */
    EXPECT_EQ(SA(lb, LB_SETITEMDATA, -1, 77), 1);
    for (int k = 0; k < 4; k++) EXPECT_EQ(SA(lb, LB_GETITEMDATA, k, 0), 77);
    for (int k = 0; k < 4; k++) SA(lb, LB_SETITEMDATA, k, 1000 + k);
    /* full pointer-sized values survive */
    LPARAM big = (LPARAM)0x123456789ABCDEF0LL;
    SA(lb, LB_SETITEMDATA, 2, big);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 2, 0), big);
    /* data follows the item across delete/insert */
    SA(lb, LB_DELETESTRING, 0, 0);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 0, 0), 1001);
    SA(lb, LB_INSERTSTRING, 0, "z");
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 1, 0), 1001);
    /* LB_ADDSTRING returns index, data via sorted position */
    U(DestroyWindow)(p);
}

TEST(listbox_itemdata_sorted_follows_item)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_SORT, 10);
    int i;
    i = (int)SA(lb, LB_ADDSTRING, 0, "m"); SA(lb, LB_SETITEMDATA, i, 13);
    i = (int)SA(lb, LB_ADDSTRING, 0, "c"); SA(lb, LB_SETITEMDATA, i, 3);
    i = (int)SA(lb, LB_ADDSTRING, 0, "x"); SA(lb, LB_SETITEMDATA, i, 24);
    i = (int)SA(lb, LB_ADDSTRING, 0, "a"); SA(lb, LB_SETITEMDATA, i, 1);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 0, 0), 1);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 1, 0), 3);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 2, 0), 13);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 3, 0), 24);
    U(DestroyWindow)(p);
}

TEST(listbox_find_string_prefix)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 11);
    fill(lb, k_fruit, NFRUIT);   /* Apple apricot Banana avocado Cherry date */
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "a"), 0);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, 0, "a"), 1);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, 1, "a"), 3);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, 3, "a"), 0);   /* wraps */
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "ap"), 0);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, 0, "ap"), 1);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, 1, "ap"), 0);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "APR"), 1);        /* case-insensitive */
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "ban"), 2);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "Bananas"), LB_ERR);  /* longer than the item */
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "z"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "ch"), 4);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, 4, "ch"), 4);          /* only match is the start item: wraps to it */
    EXPECT_EQ(SA(lb, LB_FINDSTRING, -1, "date"), 5);
    EXPECT_EQ(SA(lb, LB_FINDSTRING, 5, "date"), 5);
    /* selection is not changed by FINDSTRING */
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), LB_ERR);
    U(DestroyWindow)(p);
}

TEST(listbox_find_string_exact)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 11);
    fill(lb, k_fruit, NFRUIT);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "apple"), 0);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "APRICOT"), 1);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "app"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "applee"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "avocado"), 3);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, 3, "avocado"), 3);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, 2, "avocado"), 3);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, 0, "apple"), 0);   /* wraps back to the start item */
    SA(lb, LB_ADDSTRING, 0, "dup"); SA(lb, LB_ADDSTRING, 0, "DUP");
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "dup"), 6);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, 6, "dup"), 7);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, 7, "dup"), 6);
    U(DestroyWindow)(p);
}

TEST(listbox_select_string)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 11);
    fill(lb, k_fruit, NFRUIT);
    c_logclear();
    EXPECT_EQ(SA(lb, LB_SELECTSTRING, -1, "ban"), 2);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 2);
    EXPECT_EQ(SA(lb, LB_SELECTSTRING, 2, "a"), 3);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 3);
    EXPECT_EQ(SA(lb, LB_SELECTSTRING, -1, "nomatch"), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 3);   /* unchanged on failure */
    EXPECT_EQ(c_nmsg(WM_COMMAND), 0);
    U(DestroyWindow)(p);
}

TEST(listbox_top_index_and_item_rect)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetClientRect));
    HWND p = c_parent(300, 400);
    HWND lb = mklb(p, LB_STD, 12);
    char s[16];
    for (int i = 0; i < 40; i++) { sprintf(s, "item %d", i); SA(lb, LB_ADDSTRING, 0, s); }
    LRESULT h = SA(lb, LB_GETITEMHEIGHT, 0, 0);
    RECT cl, r;
    CHECK(h > 4 && h < 100);
    U(GetClientRect)(lb, &cl);
    EXPECT_EQ(SA(lb, LB_GETTOPINDEX, 0, 0), 0);
    /* returns 1 when the item is (at least partly) inside the client area */
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 0, &r), 1);
    EXPECT_EQ(r.left, 0); EXPECT_EQ(r.top, 0); EXPECT_EQ(r.right, cl.right); EXPECT_EQ(r.bottom, h);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 3, &r), 1);
    EXPECT_EQ(r.top, 3 * h); EXPECT_EQ(r.bottom, 4 * h);
    /* scrolled out of view: 0, but the rectangle is still computed */
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 39, &r), 0);
    EXPECT_EQ(r.top, 39 * h); EXPECT_EQ(r.bottom, 40 * h);
    EXPECT_EQ(SA(lb, LB_SETTOPINDEX, 10, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETTOPINDEX, 0, 0), 10);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 10, &r), 1);
    EXPECT_EQ(r.top, 0);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 12, &r), 1);
    EXPECT_EQ(r.top, 2 * h);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 5, &r), 0);
    EXPECT_EQ(r.top, -5 * h);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 40, &r), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    EXPECT_EQ(r.right - r.left, 0);
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_SETTOPINDEX, 40, 0), LB_ERR);
    EXPECT_ERR(0, ERROR_INVALID_INDEX);
    EXPECT_EQ(SA(lb, LB_SETTOPINDEX, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETTOPINDEX, 0, 0), 0);
    /* top index cannot go past the last full page */
    EXPECT_EQ(SA(lb, LB_SETTOPINDEX, 39, 0), 0);
    LRESULT t = SA(lb, LB_GETTOPINDEX, 0, 0);
    CHECK(t > 0 && t <= 39);
    t_note("LB_SETTOPINDEX(39) with 40 items -> top %lld (page=%d)", (long long)t, (int)(cl.bottom / h));
    U(DestroyWindow)(p);
}

TEST(listbox_item_height_roundtrip)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 13);
    fill(lb, k_fruit, 3);
    EXPECT_EQ(SA(lb, LB_SETITEMHEIGHT, 0, 30), 0);
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 0, 0), 30);
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 2, 0), 30);   /* fixed: same for all items */
    EXPECT_EQ(SA(lb, LB_SETITEMHEIGHT, 0, 1), 0);
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 0, 0), 1);
    EXPECT_EQ(SA(lb, LB_SETITEMHEIGHT, 0, 255), 0);
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 0, 0), 255);
    EXPECT_EQ(SA(lb, LB_SETITEMHEIGHT, 0, 256), LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 0, 0), 255);
    EXPECT_EQ(SA(lb, LB_SETITEMHEIGHT, 0, MAKELPARAM(300, 0)), LB_ERR);
    t_note("LB_SETITEMHEIGHT(0 height) -> %lld", (long long)SA(lb, LB_SETITEMHEIGHT, 0, 0));
    RECT r;
    SA(lb, LB_SETITEMHEIGHT, 0, 20);
    SA(lb, LB_GETITEMRECT, 2, &r);
    EXPECT_EQ(r.top, 40); EXPECT_EQ(r.bottom, 60);
    U(DestroyWindow)(p);
}

TEST(listbox_item_height_variable)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS, 13);
    c_mi_base = 10; c_mi_step = 5;
    c_logclear();
    for (int i = 0; i < 4; i++) SA(lb, LB_ADDSTRING, 0, "v");
    c_mi_base = 0;
    EXPECT_EQ(c_nmsg(WM_MEASUREITEM), 4);
    int ids = 0;
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_MEASUREITEM) {
        CHECK(c_log[i].mi.CtlType == ODT_LISTBOX && c_log[i].mi.CtlID == 13);
        ids |= 1 << c_log[i].mi.itemID;
    }
    EXPECT_EQ(ids, 0xF);
    for (int i = 0; i < 4; i++) EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, i, 0), 10 + 5 * i);
    EXPECT_EQ(SA(lb, LB_SETITEMHEIGHT, 2, 40), 0);
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 2, 0), 40);
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 1, 0), 15);
    RECT r;
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 3, &r), 1);
    EXPECT_EQ(r.top, 10 + 15 + 40);
    EXPECT_EQ(r.bottom - r.top, 25);
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 4, 0), LB_ERR);
    U(DestroyWindow)(p);
}

TEST(listbox_integral_height)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetClientRect));
    HWND p = c_parent(300, 300);
    HWND a = c_ctl(p, "ListBox", "", WS_BORDER | LBS_NOTIFY, 1, 0, 0, 100, 97);
    HWND b = c_ctl(p, "ListBox", "", WS_BORDER | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 2, 0, 0, 100, 97);
    RECT r;
    SA(a, LB_ADDSTRING, 0, "x");
    LRESULT h = SA(a, LB_GETITEMHEIGHT, 0, 0);
    U(GetClientRect)(a, &r);
    CHECKF(r.bottom % h == 0, "client height %ld not multiple of %lld", r.bottom, (long long)h);
    U(GetClientRect)(b, &r);
    CHECK(r.bottom == 97 - 2 * (int)U(GetSystemMetrics)(SM_CYBORDER));
    U(DestroyWindow)(p);
}

TEST(listbox_horizontal_extent_columns_tabs)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | WS_HSCROLL, 14);
    fill(lb, k_fruit, 3);
    EXPECT_EQ(SA(lb, LB_GETHORIZONTALEXTENT, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_SETHORIZONTALEXTENT, 500, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETHORIZONTALEXTENT, 0, 0), 500);
    SA(lb, LB_SETHORIZONTALEXTENT, 0, 0);
    EXPECT_EQ(SA(lb, LB_GETHORIZONTALEXTENT, 0, 0), 0);
    /* tab stops require LBS_USETABSTOPS */
    int ts[2] = { 20, 40 };
    HWND tl = mklb(p, LB_STD | LBS_USETABSTOPS, 15);
    CHECK(SA(tl, LB_SETTABSTOPS, 2, ts) != 0);
    CHECK(SA(tl, LB_SETTABSTOPS, 0, 0) != 0);
    CHECK(SA(tl, LB_SETTABSTOPS, 1, ts) != 0);
    SetLastError(0);
    LRESULT r = SA(lb, LB_SETTABSTOPS, 2, ts);
    t_note("LB_SETTABSTOPS without LBS_USETABSTOPS -> %lld err=%lu", (long long)r, GetLastError());
    U(DestroyWindow)(p);
}

TEST(listbox_multicolumn_item_rect)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = c_ctl(p, "ListBox", "", LBS_NOTIFY | LBS_MULTICOLUMN | LBS_NOINTEGRALHEIGHT | WS_HSCROLL, 16, 0, 0, 200, 60);
    for (int i = 0; i < 30; i++) SA(lb, LB_ADDSTRING, 0, "col");
    EXPECT_EQ(SA(lb, LB_SETCOLUMNWIDTH, 50, 0), 0);
    RECT r;
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 0, &r), 1);
    EXPECT_EQ(r.right - r.left, 50);
    LRESULT h = SA(lb, LB_GETITEMHEIGHT, 0, 0);
    int per_col = 0;
    for (int i = 1; i < 30; i++) {
        SA(lb, LB_GETITEMRECT, i, &r);
        if (r.left > 0) { per_col = i; break; }
    }
    CHECK(per_col >= 1);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, per_col, &r), 1);   /* first item of the 2nd column */
    EXPECT_EQ(r.left, 50);
    EXPECT_EQ(r.top, 0);
    EXPECT_EQ(r.right - r.left, 50);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, per_col - 1, &r), 1);
    EXPECT_EQ(r.left, 0);
    EXPECT_EQ(r.top, (per_col - 1) * h);
    EXPECT_EQ(SA(lb, LB_SETCOLUMNWIDTH, 80, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, per_col, &r), 1);
    EXPECT_EQ(r.left, 80);
    U(DestroyWindow)(p);
}

TEST(listbox_itemfrompoint)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 17);
    fill(lb, k_fruit, NFRUIT);
    LRESULT h = SA(lb, LB_GETITEMHEIGHT, 0, 0);
    LRESULT r = SA(lb, LB_ITEMFROMPOINT, 0, MAKELPARAM(5, 1));
    EXPECT_EQ(LOWORD(r), 0); EXPECT_EQ(HIWORD(r), 0);
    r = SA(lb, LB_ITEMFROMPOINT, 0, MAKELPARAM(5, (int)h * 2 + 1));
    EXPECT_EQ(LOWORD(r), 2); EXPECT_EQ(HIWORD(r), 0);
    r = SA(lb, LB_ITEMFROMPOINT, 0, MAKELPARAM(5, (int)h * 5 + 1));
    EXPECT_EQ(LOWORD(r), 5); EXPECT_EQ(HIWORD(r), 0);
    /* outside the client area */
    r = SA(lb, LB_ITEMFROMPOINT, 0, MAKELPARAM(500, 1));
    EXPECT_EQ(HIWORD(r), 1);
    r = SA(lb, LB_ITEMFROMPOINT, 0, MAKELPARAM(5, 900));
    EXPECT_EQ(HIWORD(r), 1);
    U(DestroyWindow)(p);
}

TEST(listbox_initstorage)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 18);
    LRESULT r = SA(lb, LB_INITSTORAGE, 1000, 1000 * 20);
    CHECKF(r >= 1000 || r == LB_ERRSPACE, "ret %lld", (long long)r);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    r = SA(lb, LB_INITSTORAGE, 0, 0);
    t_note("LB_INITSTORAGE(0,0) = %lld", (long long)r);
    for (int i = 0; i < 1000; i++) SA(lb, LB_ADDSTRING, 0, "preallocated item");
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 1000);
    U(DestroyWindow)(p);
}

TEST(listbox_many_items)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 19);
    char s[32];
    for (int i = 0; i < 20000; i++) { sprintf(s, "row %d", i); if (SA(lb, LB_ADDSTRING, 0, s) != i) { CHECKF(0, "add %d", i); break; } }
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 20000);
    CHECK(c_lbtext_is(lb, 0, "row 0"));
    CHECK(c_lbtext_is(lb, 19999, "row 19999"));
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "row 12345"), 12345);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, 19999, 0), 19999);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 19999);
    SA(lb, LB_RESETCONTENT, 0, 0);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(listbox_locale)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 20);
    LRESULT orig = SA(lb, LB_GETLOCALE, 0, 0);
    CHECK(orig != 0);
    t_note("default LB locale = 0x%llx", (long long)orig);
    LRESULT prev = SA(lb, LB_SETLOCALE, 0x0409, 0);
    EXPECT_EQ(prev, orig);
    EXPECT_EQ(SA(lb, LB_GETLOCALE, 0, 0), 0x0409);
    EXPECT_EQ(SA(lb, LB_SETLOCALE, 0x0407, 0), 0x0409);
    EXPECT_EQ(SA(lb, LB_GETLOCALE, 0, 0), 0x0407);
    EXPECT_EQ(SA(lb, LB_SETLOCALE, orig, 0), 0x0407);
    EXPECT_EQ(SA(lb, LB_GETLOCALE, 0, 0), orig);
    U(DestroyWindow)(p);
}

TEST(listbox_hasstrings_ownerdraw_no_strings)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWFIXED, 21);   /* no LBS_HASSTRINGS: lParam is item data */
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, 0x1111), 0);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, 0x2222), 1);
    EXPECT_EQ(SA(lb, LB_INSERTSTRING, 1, 0x3333), 1);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 3);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 0, 0), 0x1111);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 1, 0), 0x3333);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 2, 0), 0x2222);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, 0x2222), 2);   /* compares item data */
    t_note("LB_FINDSTRING on data-only list: %lld", (long long)SA(lb, LB_FINDSTRING, -1, 0x2222));
    U(DestroyWindow)(p);
}

TEST(listbox_destroyed_handle)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(IsWindow));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 22);
    SA(lb, LB_ADDSTRING, 0, "x");
    U(DestroyWindow)(lb);
    CHECK(!U(IsWindow)(lb));
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    EXPECT_EQ(SA(lb, LB_ADDSTRING, 0, "y"), 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 0);
    U(DestroyWindow)(p);
}

TEST(listbox_wm_getdlgcode_and_font)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 23);
    LRESULT c = SA(lb, WM_GETDLGCODE, 0, 0);
    CHECK(c & DLGC_WANTARROWS);
    CHECK(c & DLGC_WANTCHARS);
    HFONT f = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    EXPECT_EQ(SA(lb, WM_GETFONT, 0, 0), 0);
    SA(lb, WM_SETFONT, f, TRUE);
    EXPECT_EQ(SA(lb, WM_GETFONT, 0, 0), f);
    LRESULT h1 = SA(lb, LB_GETITEMHEIGHT, 0, 0);
    TEXTMETRICA tm; HDC dc = CreateCompatibleDC(NULL); SelectObject(dc, f); GetTextMetricsA(dc, &tm); DeleteDC(dc);
    SA(lb, LB_ADDSTRING, 0, "x");
    LRESULT h2 = SA(lb, LB_GETITEMHEIGHT, 0, 0);
    CHECKF(h2 == tm.tmHeight, "item height %lld vs font %ld (before %lld)", (long long)h2, tm.tmHeight, (long long)h1);
    U(DestroyWindow)(p);
}
