#include "ctl.h"

#define LB_STD (LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL)

static HWND mklb(HWND p, DWORD style, int id)
{
    return c_ctl(p, "ListBox", "", style, id, 2, 2, 120, 100);
}
static const char *const k_fruit[] = { "Apple", "apricot", "Banana", "avocado", "Cherry", "date" };

static HDC mkdc(HBITMAP *bm)
{
    HDC dc = CreateCompatibleDC(NULL);
    *bm = CreateBitmap(300, 300, 1, 1, NULL);
    SelectObject(dc, *bm);
    return dc;
}

/* ---- owner draw ---- */
TEST(listbox_ownerdraw_fixed_measure_at_create)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    c_logclear();
    c_mi_base = 18; c_mi_step = 0;
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 31);
    c_mi_base = 0;
    CHECK(lb != NULL);
    EXPECT_EQ(c_nmsg(WM_MEASUREITEM), 1);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_MEASUREITEM) {
        EXPECT_EQ(c_log[i].mi.CtlType, ODT_LISTBOX);
        EXPECT_EQ(c_log[i].mi.CtlID, 31);
        break;
    }
    EXPECT_EQ(SA(lb, LB_GETITEMHEIGHT, 0, 0), 18);
    SA(lb, LB_ADDSTRING, 0, "item");
    EXPECT_EQ(c_nmsg(WM_MEASUREITEM), 1);   /* fixed: not sent per item */
    U(DestroyWindow)(p);
}

TEST(listbox_ownerdraw_variable_no_measure_at_create)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    c_logclear();
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS, 32);
    t_note("variable ownerdraw: WM_MEASUREITEM at creation = %d", c_nmsg(WM_MEASUREITEM));
    int before = c_nmsg(WM_MEASUREITEM);
    SA(lb, LB_ADDSTRING, 0, "a");
    SA(lb, LB_INSERTSTRING, 0, "b");
    EXPECT_EQ(c_nmsg(WM_MEASUREITEM) - before, 2);
    U(DestroyWindow)(p);
}

TEST(listbox_ownerdraw_printclient_draws_items)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 33);
    for (int i = 0; i < 3; i++) { SA(lb, LB_ADDSTRING, 0, k_fruit[i]); SA(lb, LB_SETITEMDATA, i, 500 + i); }
    SA(lb, LB_SETCURSEL, 1, 0);
    HBITMAP bm; HDC dc = mkdc(&bm);
    c_logclear();
    SA(lb, WM_PRINTCLIENT, dc, PRF_CLIENT);
    int mask = 0, n = 0;
    LRESULT h = SA(lb, LB_GETITEMHEIGHT, 0, 0);
    RECT cl; U(GetClientRect)(lb, &cl);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_DRAWITEM) {
        DRAWITEMSTRUCT *d = &c_log[i].di;
        n++;
        EXPECT_EQ(d->CtlType, ODT_LISTBOX);
        EXPECT_EQ(d->CtlID, 33);
        EXPECT_EQ(d->hwndItem, lb);
        CHECK(d->hDC != NULL);
        if (d->itemID < 3) {
            mask |= 1 << d->itemID;
            EXPECT_EQ(d->itemData, 500 + d->itemID);
            EXPECT_EQ(d->rcItem.top, d->itemID * h);
            EXPECT_EQ(d->rcItem.bottom, (d->itemID + 1) * h);
            EXPECT_EQ(d->rcItem.right - d->rcItem.left, cl.right);
            CHECK(d->itemAction & ODA_DRAWENTIRE);
            if (d->itemID == 1) CHECK(d->itemState & ODS_SELECTED);
            else CHECK(!(d->itemState & ODS_SELECTED));
        }
    }
    EXPECT_EQ(mask, 7);
    CHECKF(n >= 3, "draw count %d", n);
    t_note("WM_DRAWITEM count for 3 items = %d", n);
    DeleteDC(dc); DeleteObject(bm);
    U(DestroyWindow)(p);
}

TEST(listbox_ownerdraw_deleteitem_messages)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 34);
    for (int i = 0; i < 4; i++) { SA(lb, LB_ADDSTRING, 0, "x"); SA(lb, LB_SETITEMDATA, i, 900 + i); }
    c_logclear();
    SA(lb, LB_DELETESTRING, 1, 0);
    EXPECT_EQ(c_nmsg(WM_DELETEITEM), 1);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_DELETEITEM) {
        EXPECT_EQ(c_log[i].de.CtlType, ODT_LISTBOX);
        EXPECT_EQ(c_log[i].de.CtlID, 34);
        EXPECT_EQ(c_log[i].de.itemID, 1);
        EXPECT_EQ(c_log[i].de.hwndItem, lb);
        EXPECT_EQ(c_log[i].de.itemData, 901);
    }
    c_logclear();
    SA(lb, LB_RESETCONTENT, 0, 0);
    EXPECT_EQ(c_nmsg(WM_DELETEITEM), 3);
    /* no message for a failed delete */
    c_logclear();
    SA(lb, LB_DELETESTRING, 0, 0);
    EXPECT_EQ(c_nmsg(WM_DELETEITEM), 0);
    /* destroying the control reports the remaining items */
    for (int i = 0; i < 5; i++) SA(lb, LB_ADDSTRING, 0, "y");
    c_logclear();
    U(DestroyWindow)(lb);
    EXPECT_EQ(c_nmsg(WM_DELETEITEM), 5);
    U(DestroyWindow)(p);
}

TEST(listbox_plain_list_sends_no_deleteitem)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 35);
    for (int i = 0; i < 3; i++) SA(lb, LB_ADDSTRING, 0, "p");
    c_logclear();
    SA(lb, LB_DELETESTRING, 0, 0);
    SA(lb, LB_RESETCONTENT, 0, 0);
    EXPECT_EQ(c_nmsg(WM_DELETEITEM), 0);
    EXPECT_EQ(c_nmsg(WM_MEASUREITEM), 0);
    EXPECT_EQ(c_nmsg(WM_DRAWITEM), 0);
    U(DestroyWindow)(p);
}

TEST(listbox_ownerdraw_sort_compareitem)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWFIXED | LBS_SORT, 36);   /* no LBS_HASSTRINGS */
    c_logclear();
    SA(lb, LB_ADDSTRING, 0, 30);
    SA(lb, LB_ADDSTRING, 0, 10);
    SA(lb, LB_ADDSTRING, 0, 20);
    SA(lb, LB_ADDSTRING, 0, 5);
    CHECK(c_nmsg(WM_COMPAREITEM) >= 3);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 0, 0), 5);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 1, 0), 10);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 2, 0), 20);
    EXPECT_EQ(SA(lb, LB_GETITEMDATA, 3, 0), 30);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_COMPAREITEM) {
        EXPECT_EQ(c_log[i].ci.CtlType, ODT_LISTBOX);
        EXPECT_EQ(c_log[i].ci.CtlID, 36);
        EXPECT_EQ(c_log[i].ci.hwndItem, lb);
        break;
    }
    U(DestroyWindow)(p);
}

TEST(listbox_ownerdraw_sort_hasstrings_no_compareitem)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_OWNERDRAWFIXED | LBS_SORT | LBS_HASSTRINGS, 37);
    c_logclear();
    SA(lb, LB_ADDSTRING, 0, "pear"); SA(lb, LB_ADDSTRING, 0, "fig"); SA(lb, LB_ADDSTRING, 0, "kiwi");
    EXPECT_EQ(c_nmsg(WM_COMPAREITEM), 0);
    CHECK(c_lbtext_is(lb, 0, "fig"));
    CHECK(c_lbtext_is(lb, 1, "kiwi"));
    CHECK(c_lbtext_is(lb, 2, "pear"));
    U(DestroyWindow)(p);
}

/* ---- keyboard driven notifications ---- */
TEST(listbox_keyboard_navigation_notifies)
{
    if (GetAsyncKeyState(VK_SHIFT) < 0 || GetAsyncKeyState(VK_CONTROL) < 0 || GetAsyncKeyState(VK_MENU) < 0) { t_skip("modifier key held"); return; }
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 41);
    for (int i = 0; i < 6; i++) SA(lb, LB_ADDSTRING, 0, k_fruit[i]);
    SA(lb, LB_SETCURSEL, 0, 0);
    c_logclear();
    SA(lb, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 1);
    EXPECT_EQ(c_ncmd(41, LBN_SELCHANGE), 1);
    int i = c_first_cmd(41, LBN_SELCHANGE);
    CHECK(i >= 0 && c_log[i].lp == (LPARAM)lb);
    SA(lb, WM_KEYDOWN, VK_END, 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 5);
    SA(lb, WM_KEYDOWN, VK_DOWN, 0);   /* already at the end */
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 5);
    SA(lb, WM_KEYDOWN, VK_UP, 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 4);
    SA(lb, WM_KEYDOWN, VK_HOME, 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 0);
    SA(lb, WM_KEYDOWN, VK_UP, 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 0);
    CHECK(c_ncmd(41, LBN_SELCHANGE) >= 4);
    SA(lb, WM_KEYDOWN, VK_NEXT, 0);
    LRESULT pg = SA(lb, LB_GETCURSEL, 0, 0);
    CHECK(pg > 0);
    SA(lb, WM_KEYDOWN, VK_PRIOR, 0);
    CHECK(SA(lb, LB_GETCURSEL, 0, 0) < pg);
    U(DestroyWindow)(p);
}

TEST(listbox_keyboard_notify_requires_lbs_notify)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, WS_VSCROLL | LBS_NOINTEGRALHEIGHT, 42);   /* no LBS_NOTIFY */
    for (int i = 0; i < 6; i++) SA(lb, LB_ADDSTRING, 0, k_fruit[i]);
    SA(lb, LB_SETCURSEL, 0, 0);
    c_logclear();
    SA(lb, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 1);
    EXPECT_EQ(c_ncmd(42, LBN_SELCHANGE), 0);
    U(DestroyWindow)(p);
}

TEST(listbox_char_typeahead)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 43);
    for (int i = 0; i < 6; i++) SA(lb, LB_ADDSTRING, 0, k_fruit[i]);
    c_logclear();
    SA(lb, WM_CHAR, 'b', 1);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 2);
    EXPECT_EQ(c_ncmd(43, LBN_SELCHANGE), 1);
    SA(lb, WM_CHAR, 'C', 1);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 4);
    SA(lb, WM_CHAR, 'd', 1);
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 5);
    U(DestroyWindow)(p);
}

TEST(listbox_wantkeyboardinput_vkeytoitem)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_WANTKEYBOARDINPUT, 44);
    for (int i = 0; i < 4; i++) SA(lb, LB_ADDSTRING, 0, k_fruit[i]);
    SA(lb, LB_SETCURSEL, 1, 0);
    c_logclear();
    SA(lb, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(c_nmsg(WM_VKEYTOITEM), 1);
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == WM_VKEYTOITEM) {
        EXPECT_EQ(LOWORD(c_log[i].wp), VK_DOWN);
        EXPECT_EQ(HIWORD(c_log[i].wp), 1);
        EXPECT_EQ(c_log[i].lp, lb);
    }
    /* the parent returned -1: default processing moved the selection */
    EXPECT_EQ(SA(lb, LB_GETCURSEL, 0, 0), 2);
    U(DestroyWindow)(p);
}

/* ---- LBS_NODATA ---- */
TEST(listbox_nodata_setcount)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_NODATA | LBS_OWNERDRAWFIXED, 51);
    CHECK(lb != NULL);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    CHECK(SA(lb, LB_SETCOUNT, 1000, 0) != LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 1000);
    CHECK(SA(lb, LB_SETCOUNT, 10, 0) != LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 10);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, 9, 0), 9);
    EXPECT_EQ(SA(lb, LB_SETCURSEL, 10, 0), LB_ERR);
    CHECK(SA(lb, LB_SETCOUNT, 0, 0) != LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    CHECK(SA(lb, LB_SETCOUNT, 100000, 0) != LB_ERR);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 100000);
    EXPECT_EQ(SA(lb, LB_SETSEL, TRUE, 5), LB_ERR);   /* single selection */
    RECT r;
    EXPECT_EQ(SA(lb, LB_GETITEMRECT, 99999, &r), 0);
    U(DestroyWindow)(p);
}

TEST(listbox_setcount_on_regular_list_fails)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 52);
    SA(lb, LB_ADDSTRING, 0, "a");
    SetLastError(0);
    EXPECT_EQ(SA(lb, LB_SETCOUNT, 50, 0), LB_ERR);
    t_note("LB_SETCOUNT on a regular list: err=%lu", GetLastError());
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 1);
    U(DestroyWindow)(p);
}

/* ---- directory listing ---- */
static void mkfile(const char *dir, const char *name)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s", dir, name);
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
static void rmfile(const char *dir, const char *name)
{
    char path[MAX_PATH];
    wsprintfA(path, "%s%s", dir, name);
    DeleteFileA(path);
}
static int mk_dirfixture(char *dir)
{
    char tmp[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tmp)) return 0;
    wsprintfA(dir, "%sctl_lbdir_%lu\\", tmp, GetCurrentProcessId());
    if (!CreateDirectoryA(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    mkfile(dir, "aaa.tst"); mkfile(dir, "bbb.tst"); mkfile(dir, "ccc.dat");
    char sub[MAX_PATH];
    wsprintfA(sub, "%ssubdir", dir);
    CreateDirectoryA(sub, NULL);
    return 1;
}
static void rm_dirfixture(const char *dir)
{
    char sub[MAX_PATH];
    rmfile(dir, "aaa.tst"); rmfile(dir, "bbb.tst"); rmfile(dir, "ccc.dat");
    wsprintfA(sub, "%ssubdir", dir);
    RemoveDirectoryA(sub);
    char d[MAX_PATH]; lstrcpyA(d, dir);
    d[lstrlenA(d) - 1] = 0;
    RemoveDirectoryA(d);
}

TEST(listbox_dir_files_pattern)
{
    REQUIRE(U(CreateWindowExA));
    char dir[MAX_PATH], pat[MAX_PATH];
    if (!mk_dirfixture(dir)) { t_skip("cannot create temp dir"); return; }
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_SORT, 61);
    wsprintfA(pat, "%s*.tst", dir);
    LRESULT r = SA(lb, LB_DIR, DDL_READWRITE, pat);
    CHECKF(r != LB_ERR && r != LB_ERRSPACE, "ret %lld", (long long)r);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 2);
    CHECK(c_lbtext_is(lb, 0, "aaa.tst"));
    CHECK(c_lbtext_is(lb, 1, "bbb.tst"));
    /* second listing appends */
    wsprintfA(pat, "%s*.dat", dir);
    SA(lb, LB_DIR, DDL_READWRITE, pat);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 3);
    CHECK(SA(lb, LB_FINDSTRINGEXACT, -1, "ccc.dat") >= 0);
    /* no match: nothing added */
    SA(lb, LB_RESETCONTENT, 0, 0);
    wsprintfA(pat, "%s*.zzz", dir);
    r = SA(lb, LB_DIR, DDL_READWRITE, pat);
    t_note("LB_DIR with no match -> %lld", (long long)r);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 0);
    U(DestroyWindow)(p);
    rm_dirfixture(dir);
}

TEST(listbox_dir_directories_and_dirselect)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(DlgDirSelectExA));
    char dir[MAX_PATH], pat[MAX_PATH];
    if (!mk_dirfixture(dir)) { t_skip("cannot create temp dir"); return; }
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_SORT, 62);
    wsprintfA(pat, "%s*", dir);
    SA(lb, LB_DIR, DDL_READWRITE | DDL_DIRECTORY, pat);
    CHECK(SA(lb, LB_FINDSTRINGEXACT, -1, "[subdir]") >= 0);
    CHECK(SA(lb, LB_FINDSTRINGEXACT, -1, "aaa.tst") >= 0);
    CHECK(SA(lb, LB_FINDSTRINGEXACT, -1, "bbb.tst") >= 0);
    CHECK(SA(lb, LB_FINDSTRINGEXACT, -1, "ccc.dat") >= 0);
    t_note("'[..]' present: %d, count=%lld", SA(lb, LB_FINDSTRINGEXACT, -1, "[..]") >= 0, (long long)SA(lb, LB_GETCOUNT, 0, 0));
    /* directory-only listing */
    SA(lb, LB_RESETCONTENT, 0, 0);
    SA(lb, LB_DIR, DDL_DIRECTORY | DDL_EXCLUSIVE, pat);
    CHECK(SA(lb, LB_FINDSTRINGEXACT, -1, "[subdir]") >= 0);
    EXPECT_EQ(SA(lb, LB_FINDSTRINGEXACT, -1, "aaa.tst"), LB_ERR);
    /* DlgDirSelectEx strips the brackets of a directory entry */
    char buf[MAX_PATH];
    int idx = (int)SA(lb, LB_FINDSTRINGEXACT, -1, "[subdir]");
    SA(lb, LB_SETCURSEL, idx, 0);
    memset(buf, 'x', sizeof buf);
    EXPECT_EQ(U(DlgDirSelectExA)(p, buf, MAX_PATH, 62), TRUE);
    EXPECT_STR(buf, "subdir\\");
    SA(lb, LB_RESETCONTENT, 0, 0);
    wsprintfA(pat, "%s*.tst", dir);
    SA(lb, LB_DIR, DDL_READWRITE, pat);
    SA(lb, LB_SETCURSEL, 0, 0);
    EXPECT_EQ(U(DlgDirSelectExA)(p, buf, MAX_PATH, 62), FALSE);
    EXPECT_STR(buf, "aaa.tst");
    U(DestroyWindow)(p);
    rm_dirfixture(dir);
}

TEST(listbox_dir_drives)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 63);
    LRESULT r = SA(lb, LB_DIR, DDL_DRIVES | DDL_EXCLUSIVE, "*");
    CHECK(r != LB_ERR);
    LRESULT n = SA(lb, LB_GETCOUNT, 0, 0);
    CHECK(n >= 1);
    for (int i = 0; i < n; i++) {
        char b[32]; b[0] = 0;
        SA(lb, LB_GETTEXT, i, b);
        CHECKF(strlen(b) == 5 && b[0] == '[' && b[1] == '-' && b[3] == '-' && b[4] == ']', "drive entry \"%s\"", b);
    }
    U(DestroyWindow)(p);
}

/* ---- misc ---- */
TEST(listbox_getlistboxinfo_matches_message)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetListBoxInfo));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 71);
    for (int i = 0; i < 30; i++) SA(lb, LB_ADDSTRING, 0, "row");
    DWORD a = U(GetListBoxInfo)(lb);
    LRESULT b = SA(lb, LB_GETLISTBOXINFO, 0, 0);
    EXPECT_EQ(a, (DWORD)b);
    t_note("GetListBoxInfo (single column) = %lu", a);
    HWND mc = c_ctl(p, "ListBox", "", LBS_NOTIFY | LBS_MULTICOLUMN | LBS_NOINTEGRALHEIGHT, 72, 0, 0, 200, 64);
    for (int i = 0; i < 30; i++) SA(mc, LB_ADDSTRING, 0, "row");
    SA(mc, LB_SETCOLUMNWIDTH, 40, 0);
    DWORD m = U(GetListBoxInfo)(mc);
    LRESULT h = SA(mc, LB_GETITEMHEIGHT, 0, 0);
    t_note("GetListBoxInfo (multicolumn, client 64, item %lld) = %lu", (long long)h, m);
    CHECK(m >= 1);
    EXPECT_EQ(U(GetListBoxInfo)(p) == 0 || 1, 1);
    U(DestroyWindow)(p);
}

TEST(listbox_getlistboxinfo_bad_window)
{
    REQUIRE(U(GetListBoxInfo));
    SetLastError(0);
    DWORD r = U(GetListBoxInfo)(NULL);
    t_note("GetListBoxInfo(NULL) = %lu err=%lu", r, GetLastError());
    r = U(GetListBoxInfo)((HWND)(ULONG_PTR)0xdead0);
    t_note("GetListBoxInfo(bogus) = %lu err=%lu", r, GetLastError());
    CHECK(1);
}

TEST(listbox_redraw_and_noredraw)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_NOREDRAW, 73);
    for (int i = 0; i < 5; i++) SA(lb, LB_ADDSTRING, 0, "z");
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 5);
    EXPECT_EQ(SA(lb, WM_SETREDRAW, FALSE, 0), 0);
    SA(lb, LB_ADDSTRING, 0, "z");
    EXPECT_EQ(SA(lb, WM_SETREDRAW, TRUE, 0), 0);
    EXPECT_EQ(SA(lb, LB_GETCOUNT, 0, 0), 6);
    U(DestroyWindow)(p);
}

TEST(listbox_scrollbar_info_follows_content)
{
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetScrollInfo));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD, 74);
    SCROLLINFO si;
    for (int i = 0; i < 100; i++) SA(lb, LB_ADDSTRING, 0, "row");
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_ALL;
    CHECK(U(GetScrollInfo)(lb, SB_VERT, &si));
    EXPECT_EQ(si.nMin, 0);
    EXPECT_EQ(si.nMax, 99);
    CHECK(si.nPage > 0 && si.nPage < 100);
    EXPECT_EQ(si.nPos, 0);
    SA(lb, LB_SETTOPINDEX, 20, 0);
    memset(&si, 0, sizeof si); si.cbSize = sizeof si; si.fMask = SIF_POS;
    U(GetScrollInfo)(lb, SB_VERT, &si);
    EXPECT_EQ(si.nPos, 20);
    SA(lb, WM_VSCROLL, SB_TOP, 0);
    EXPECT_EQ(SA(lb, LB_GETTOPINDEX, 0, 0), 0);
    SA(lb, WM_VSCROLL, SB_LINEDOWN, 0);
    EXPECT_EQ(SA(lb, LB_GETTOPINDEX, 0, 0), 1);
    SA(lb, WM_VSCROLL, SB_PAGEDOWN, 0);
    CHECK(SA(lb, LB_GETTOPINDEX, 0, 0) > 2);
    SA(lb, WM_VSCROLL, SB_BOTTOM, 0);
    LRESULT top = SA(lb, LB_GETTOPINDEX, 0, 0);
    CHECK(top > 80);
    SA(lb, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, 7), 0);
    EXPECT_EQ(SA(lb, LB_GETTOPINDEX, 0, 0), 7);
    /* few items: scroll bar not needed */
    SA(lb, LB_RESETCONTENT, 0, 0);
    SA(lb, LB_ADDSTRING, 0, "only");
    t_note("WS_VSCROLL after shrinking to 1 item: %d", (U(GetWindowLongA)(lb, GWL_STYLE) & WS_VSCROLL) != 0);
    U(DestroyWindow)(p);
}

TEST(listbox_wm_char_ignored_when_ctrl_sequences)
{
    REQUIRE(U(CreateWindowExA));
    HWND p = c_parent(300, 300);
    HWND lb = mklb(p, LB_STD | LBS_MULTIPLESEL, 75);
    for (int i = 0; i < 4; i++) SA(lb, LB_ADDSTRING, 0, k_fruit[i]);
    /* space toggles the item with the caret in a multi-select list */
    SA(lb, LB_SETCARETINDEX, 2, FALSE);
    c_logclear();
    SA(lb, WM_CHAR, ' ', 1);
    t_note("space in multi-select list: GETSEL(2)=%lld notifications=%d", (long long)SA(lb, LB_GETSEL, 2, 0), c_ncmd(75, LBN_SELCHANGE));
    U(DestroyWindow)(p);
}
