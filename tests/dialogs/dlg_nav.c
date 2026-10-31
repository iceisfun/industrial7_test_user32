/* dialogs: tab/group navigation, MapDialogRect, base units, SendDlgItemMessage, default id */
#include "dlgutil.h"

/* ---------------- GetNextDlgTabItem ---------------- */
static const int tabs[] = { ID_EDIT, ID_CHK, ID_R1, ID_BTN, ID_LIST, ID_COMBO, ID_3ST };
#define NTABS ((int)(sizeof tabs / sizeof *tabs))

TEST(dialogs_nexttab_null_start) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    EQ(GetNextDlgTabItem(dlg, NULL, FALSE), GetDlgItem(dlg, ID_EDIT));
    t_note("GetNextDlgTabItem(NULL, prev) -> %p (3state %p)", (void *)GetNextDlgTabItem(dlg, NULL, TRUE), (void *)GetDlgItem(dlg, ID_3ST));
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_forward_cycle) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    for (int i = 0; i < NTABS; i++) {
        HWND cur = GetDlgItem(dlg, tabs[i]);
        HWND nx = GetNextDlgTabItem(dlg, cur, FALSE);
        CHECKF(nx == GetDlgItem(dlg, tabs[(i + 1) % NTABS]), "from %d", tabs[i]);
    }
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_backward_cycle) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    for (int i = 0; i < NTABS; i++) {
        HWND cur = GetDlgItem(dlg, tabs[i]);
        HWND pv = GetNextDlgTabItem(dlg, cur, TRUE);
        CHECKF(pv == GetDlgItem(dlg, tabs[(i + NTABS - 1) % NTABS]), "from %d", tabs[i]);
    }
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_from_non_tabstop) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND st = GetDlgItem(dlg, ID_STATIC);
    EQ(GetNextDlgTabItem(dlg, st, FALSE), GetDlgItem(dlg, ID_CHK));
    EQ(GetNextDlgTabItem(dlg, st, TRUE), GetDlgItem(dlg, ID_EDIT));
    HWND r2 = GetDlgItem(dlg, ID_R2);
    EQ(GetNextDlgTabItem(dlg, r2, FALSE), GetDlgItem(dlg, ID_BTN));
    EQ(GetNextDlgTabItem(dlg, r2, TRUE), GetDlgItem(dlg, ID_R1));
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_skips_disabled) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(EnableWindow); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    EnableWindow(GetDlgItem(dlg, ID_CHK), FALSE);
    EQ(GetNextDlgTabItem(dlg, GetDlgItem(dlg, ID_EDIT), FALSE), GetDlgItem(dlg, ID_R1));
    EQ(GetNextDlgTabItem(dlg, GetDlgItem(dlg, ID_R1), TRUE), GetDlgItem(dlg, ID_EDIT));
    EnableWindow(GetDlgItem(dlg, ID_EDIT), FALSE);
    EQ(GetNextDlgTabItem(dlg, NULL, FALSE), GetDlgItem(dlg, ID_R1));
    EnableWindow(GetDlgItem(dlg, ID_CHK), TRUE);
    EQ(GetNextDlgTabItem(dlg, GetDlgItem(dlg, ID_3ST), FALSE), GetDlgItem(dlg, ID_CHK));
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_skips_hidden) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(ShowWindow); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    ShowWindow(GetDlgItem(dlg, ID_BTN), SW_HIDE);
    EQ(GetNextDlgTabItem(dlg, GetDlgItem(dlg, ID_R1), FALSE), GetDlgItem(dlg, ID_LIST));
    EQ(GetNextDlgTabItem(dlg, GetDlgItem(dlg, ID_LIST), TRUE), GetDlgItem(dlg, ID_R1));
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_removed_tabstop_style) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(SetWindowLongPtrW); REQUIRE(GetWindowLongPtrW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND c = GetDlgItem(dlg, ID_CHK);
    LONG_PTR st = GetWindowLongPtrW(c, GWL_STYLE);
    SetWindowLongPtrW(c, GWL_STYLE, st & ~WS_TABSTOP);
    EQ(GetNextDlgTabItem(dlg, GetDlgItem(dlg, ID_EDIT), FALSE), GetDlgItem(dlg, ID_R1));
    SetWindowLongPtrW(GetDlgItem(dlg, ID_STATIC), GWL_STYLE, GetWindowLongPtrW(GetDlgItem(dlg, ID_STATIC), GWL_STYLE) | WS_TABSTOP);
    EQ(GetNextDlgTabItem(dlg, GetDlgItem(dlg, ID_EDIT), FALSE), GetDlgItem(dlg, ID_STATIC));
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_single_tabstop_returns_itself) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(EnableWindow); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    for (int i = 1; i < NTABS; i++) EnableWindow(GetDlgItem(dlg, tabs[i]), FALSE);
    HWND e = GetDlgItem(dlg, ID_EDIT);
    EQ(GetNextDlgTabItem(dlg, e, FALSE), e);
    EQ(GetNextDlgTabItem(dlg, e, TRUE), e);
    EQ(GetNextDlgTabItem(dlg, NULL, FALSE), e);
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_no_tabstops) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(GetDlgItem); REQUIRE(EnableWindow); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    for (int i = 0; i < NTABS; i++) EnableWindow(GetDlgItem(dlg, tabs[i]), FALSE);
    HWND r = GetNextDlgTabItem(dlg, NULL, FALSE);
    t_note("with every tabstop disabled GetNextDlgTabItem(NULL) -> %p", (void *)r);
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_invalid_dialog) {
    REQUIRE(GetNextDlgTabItem);
    EQ(GetNextDlgTabItem(NULL, NULL, FALSE), NULL);
    EQ(GetNextDlgTabItem((HWND)0x12345678, NULL, TRUE), NULL);
}
TEST(dialogs_nexttab_bogus_control_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SET_ERR(0);
    HWND r = GetNextDlgTabItem(dlg, (HWND)0x12345678, FALSE);
    t_note("GetNextDlgTabItem(bogus hCtl) -> %p err=%lu", (void *)r, GetLastError());
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_dialog_as_control_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND r = GetNextDlgTabItem(dlg, dlg, FALSE);
    HWND r2 = GetNextDlgTabItem(dlg, dlg, TRUE);
    t_note("hCtl==hDlg: next=%p prev=%p", (void *)r, (void *)r2);
    DestroyWindow(dlg);
}
TEST(dialogs_nexttab_destroyed_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgTabItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    DestroyWindow(dlg);
    EQ(GetNextDlgTabItem(dlg, NULL, FALSE), NULL);
}

/* ---------------- GetNextDlgGroupItem ---------------- */
TEST(dialogs_nextgroup_radio_cycle_forward) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgGroupItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND r1 = GetDlgItem(dlg, ID_R1), r2 = GetDlgItem(dlg, ID_R2), r3 = GetDlgItem(dlg, ID_R3);
    EQ(GetNextDlgGroupItem(dlg, r1, FALSE), r2);
    EQ(GetNextDlgGroupItem(dlg, r2, FALSE), r3);
    EQ(GetNextDlgGroupItem(dlg, r3, FALSE), r1);     /* wraps inside group */
    DestroyWindow(dlg);
}
TEST(dialogs_nextgroup_radio_cycle_backward) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgGroupItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND r1 = GetDlgItem(dlg, ID_R1), r2 = GetDlgItem(dlg, ID_R2), r3 = GetDlgItem(dlg, ID_R3);
    EQ(GetNextDlgGroupItem(dlg, r3, TRUE), r2);
    EQ(GetNextDlgGroupItem(dlg, r2, TRUE), r1);
    EQ(GetNextDlgGroupItem(dlg, r1, TRUE), r3);
    DestroyWindow(dlg);
}
TEST(dialogs_nextgroup_single_member_group) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgGroupItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND c = GetDlgItem(dlg, ID_CHK);          /* WS_GROUP, followed by another WS_GROUP */
    EQ(GetNextDlgGroupItem(dlg, c, FALSE), c);
    EQ(GetNextDlgGroupItem(dlg, c, TRUE), c);
    DestroyWindow(dlg);
}
TEST(dialogs_nextgroup_skips_disabled_and_hidden) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgGroupItem); REQUIRE(GetDlgItem); REQUIRE(EnableWindow); REQUIRE(ShowWindow); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND r1 = GetDlgItem(dlg, ID_R1), r2 = GetDlgItem(dlg, ID_R2), r3 = GetDlgItem(dlg, ID_R3);
    EnableWindow(r2, FALSE);
    EQ(GetNextDlgGroupItem(dlg, r1, FALSE), r3);
    EQ(GetNextDlgGroupItem(dlg, r3, TRUE), r1);
    EnableWindow(r2, TRUE);
    ShowWindow(r2, SW_HIDE);
    EQ(GetNextDlgGroupItem(dlg, r1, FALSE), r3);
    EQ(GetNextDlgGroupItem(dlg, r3, TRUE), r1);
    DestroyWindow(dlg);
}
TEST(dialogs_nextgroup_invalid_args) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgGroupItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    EQ(GetNextDlgGroupItem(NULL, NULL, FALSE), NULL);
    EQ(GetNextDlgGroupItem((HWND)0x12345678, NULL, FALSE), NULL);
    EQ(GetNextDlgGroupItem(dlg, (HWND)0x12345678, FALSE), NULL);
    EQ(GetNextDlgGroupItem(dlg, (HWND)0x12345678, TRUE), NULL);
    DestroyWindow(dlg);
}
TEST(dialogs_nextgroup_null_and_dialog_start_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgGroupItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND a = GetNextDlgGroupItem(dlg, NULL, FALSE);
    HWND b = GetNextDlgGroupItem(dlg, NULL, TRUE);
    HWND c = GetNextDlgGroupItem(dlg, dlg, FALSE);
    t_note("group item from NULL: next=%p (edit=%p) prev=%p; from dialog: %p", (void *)a, (void *)GetDlgItem(dlg, ID_EDIT), (void *)b, (void *)c);
    CHECK(a != NULL);
    DestroyWindow(dlg);
}
TEST(dialogs_nextgroup_edit_group_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetNextDlgGroupItem); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND e = GetDlgItem(dlg, ID_EDIT), s = GetDlgItem(dlg, ID_STATIC);
    HWND n = GetNextDlgGroupItem(dlg, e, FALSE);
    t_note("group of edit: next(edit)=%p static=%p", (void *)n, (void *)s);
    DestroyWindow(dlg);
}

/* ---------------- MapDialogRect / base units ---------------- */
static void map_base(HWND dlg, int *bx, int *by) {
    RECT r = { 4, 8, 4, 8 };
    MapDialogRect(dlg, &r);
    *bx = r.left; *by = r.top;
}
TEST(dialogs_mapdialogrect_formula) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(MapDialogRect); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    int bx, by; map_base(dlg, &bx, &by);
    CHECK(bx > 0 && by > 0);
    int bad = 0;
    for (int v = -50; v <= 400; v++) {
        RECT r = { v, v, v + 3, v * 2 };
        CHECK(MapDialogRect(dlg, &r));
        if (r.left != (bx * v + 2) / 4 || r.right != (bx * (v + 3) + 2) / 4 ||
            r.top != (by * v + 4) / 8 || r.bottom != (by * (v * 2) + 4) / 8) bad++;
    }
    EQ(bad, 0);
    DestroyWindow(dlg);
}
TEST(dialogs_mapdialogrect_zero_and_identity_units) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(MapDialogRect); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    RECT z = { 0, 0, 0, 0 };
    CHECK(MapDialogRect(dlg, &z));
    CHECK(z.left == 0 && z.top == 0 && z.right == 0 && z.bottom == 0);
    RECT r = { 4, 8, 8, 16 };
    int bx, by; map_base(dlg, &bx, &by);
    MapDialogRect(dlg, &r);
    EQ(r.left, bx); EQ(r.right, 2 * bx); EQ(r.top, by); EQ(r.bottom, 2 * by);
    DestroyWindow(dlg);
}
TEST(dialogs_mapdialogrect_independent_axes) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(MapDialogRect); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    int bx, by; map_base(dlg, &bx, &by);
    RECT r = { 100, 0, 0, 100 };
    MapDialogRect(dlg, &r);
    EQ(r.left, (bx * 100 + 2) / 4); EQ(r.top, 0);
    EQ(r.right, 0); EQ(r.bottom, (by * 100 + 4) / 8);
    DestroyWindow(dlg);
}
TEST(dialogs_mapdialogrect_repeatable) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(MapDialogRect); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    RECT a = { 7, 11, 70, 110 }, b = a;
    MapDialogRect(dlg, &a); MapDialogRect(dlg, &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    DestroyWindow(dlg);
}
TEST(dialogs_mapdialogrect_invalid) {
    REQUIRE(MapDialogRect); REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    RECT r = { 1, 2, 3, 4 };
    CHECK(!MapDialogRect(NULL, &r));
    CHECK(r.left == 1 && r.top == 2 && r.right == 3 && r.bottom == 4);
    CHECK(!MapDialogRect((HWND)0x12345678, &r));
    HWND p = t_make_window(NULL, WS_POPUP, 0, 0, 0, 20, 20, NULL);
    if (p) { CHECK(!MapDialogRect(p, &r)); DestroyWindow(p); }       /* ordinary window is not a dialog */
    HWND dlg = mk_std(); REQUIRE(dlg);
    DestroyWindow(dlg);
    CHECK(!MapDialogRect(dlg, &r));
    CHECK(r.left == 1 && r.top == 2 && r.right == 3 && r.bottom == 4);
}
TEST(dialogs_mapdialogrect_control_is_not_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(MapDialogRect); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    RECT r = { 1, 2, 3, 4 };
    CHECK(!MapDialogRect(GetDlgItem(dlg, ID_EDIT), &r));
    DestroyWindow(dlg);
}
TEST(dialogs_mapdialogrect_font_changes_units) {
    /* a dialog with a larger font maps to larger pixel units than one with a smaller font */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(MapDialogRect); REQUIRE(DestroyWindow);
    DT d; HWND a, b; int bx1, by1, bx2, by2;
    ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 50, 50, NULL, NULL, L"f", 8, L"MS Shell Dlg");
    a = CreateDialogIndirectParamW(GetModuleHandleA(NULL), (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 50, 50, NULL, NULL, L"f", 24, L"MS Shell Dlg");
    b = CreateDialogIndirectParamW(GetModuleHandleA(NULL), (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    REQUIRE(a); REQUIRE(b);
    map_base(a, &bx1, &by1); map_base(b, &bx2, &by2);
    CHECKF(bx2 > bx1 && by2 > by1, "8pt=%dx%d 24pt=%dx%d", bx1, by1, bx2, by2);
    DestroyWindow(a); DestroyWindow(b);
}
TEST(dialogs_mapdialogrect_nofont_matches_base_units_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(MapDialogRect); REQUIRE(GetDialogBaseUnits); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 50, 50, NULL, NULL, L"nf", 0, NULL);
    HWND a = CreateDialogIndirectParamW(GetModuleHandleA(NULL), (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    REQUIRE(a);
    int bx, by; map_base(a, &bx, &by);
    LONG u = GetDialogBaseUnits();
    t_note("no-font dialog units %dx%d, GetDialogBaseUnits %dx%d", bx, by, LOWORD(u), HIWORD(u));
    DestroyWindow(a);
}
TEST(dialogs_getdialogbaseunits_sane) {
    REQUIRE(GetDialogBaseUnits);
    LONG u = GetDialogBaseUnits();
    int x = LOWORD(u), y = HIWORD(u);
    CHECKF(x >= 3 && x <= 40, "x=%d", x);
    CHECKF(y >= 6 && y <= 80, "y=%d", y);
    CHECK(y > x);                              /* system font is taller than wide */
    t_note("dialog base units %dx%d", x, y);
}
TEST(dialogs_getdialogbaseunits_stable) {
    REQUIRE(GetDialogBaseUnits);
    LONG u = GetDialogBaseUnits();
    int bad = 0;
    for (int i = 0; i < 1000; i++) if (GetDialogBaseUnits() != u) bad++;
    EQ(bad, 0);
}

/* ---------------- SendDlgItemMessage ---------------- */
TEST(dialogs_senddlgitemmsg_settext_gettext) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageA); REQUIRE(SendDlgItemMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[32]; WCHAR w[32];
    EQ(SendDlgItemMessageA(dlg, ID_EDIT, WM_SETTEXT, 0, (LPARAM)"abc"), TRUE);
    EQ(SendDlgItemMessageA(dlg, ID_EDIT, WM_GETTEXTLENGTH, 0, 0), 3);
    EQ(SendDlgItemMessageW(dlg, ID_EDIT, WM_GETTEXTLENGTH, 0, 0), 3);
    EQ(SendDlgItemMessageA(dlg, ID_EDIT, WM_GETTEXT, 32, (LPARAM)a), 3); EXPECT_STR(a, "abc");
    EQ(SendDlgItemMessageW(dlg, ID_EDIT, WM_GETTEXT, 32, (LPARAM)w), 3); EXPECT_WSTR(w, L"abc");
    EQ(SendDlgItemMessageW(dlg, ID_EDIT, WM_SETTEXT, 0, (LPARAM)L"wide!"), TRUE);
    EQ(SendDlgItemMessageA(dlg, ID_EDIT, WM_GETTEXT, 32, (LPARAM)a), 5); EXPECT_STR(a, "wide!");
    EQ(SendDlgItemMessageW(dlg, ID_EDIT, WM_GETTEXT, 3, (LPARAM)w), 2); EXPECT_WSTR(w, L"wi");
    DestroyWindow(dlg);
}
TEST(dialogs_senddlgitemmsg_listbox_ansi_wide) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageA); REQUIRE(SendDlgItemMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[32]; WCHAR w[32];
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_ADDSTRING, 0, (LPARAM)"one"), 0);
    EQ(SendDlgItemMessageW(dlg, ID_LIST, LB_ADDSTRING, 0, (LPARAM)L"two"), 1);
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_ADDSTRING, 0, (LPARAM)"three"), 2);
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_GETCOUNT, 0, 0), 3);
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_GETTEXT, 1, (LPARAM)a), 3); EXPECT_STR(a, "two");
    EQ(SendDlgItemMessageW(dlg, ID_LIST, LB_GETTEXT, 2, (LPARAM)w), 5); EXPECT_WSTR(w, L"three");
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_GETTEXTLEN, 0, 0), 3);
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_SETCURSEL, 2, 0), 2);
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_GETCURSEL, 0, 0), 2);
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"two"), 1);
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_GETTEXT, 99, (LPARAM)a), LB_ERR);
    SendDlgItemMessageA(dlg, ID_LIST, LB_RESETCONTENT, 0, 0);
    EQ(SendDlgItemMessageA(dlg, ID_LIST, LB_GETCOUNT, 0, 0), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_senddlgitemmsg_combo) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[32];
    EQ(SendDlgItemMessageA(dlg, ID_COMBO, CB_ADDSTRING, 0, (LPARAM)"alpha"), 0);
    EQ(SendDlgItemMessageA(dlg, ID_COMBO, CB_ADDSTRING, 0, (LPARAM)"beta"), 1);
    EQ(SendDlgItemMessageA(dlg, ID_COMBO, CB_GETCOUNT, 0, 0), 2);
    EQ(SendDlgItemMessageA(dlg, ID_COMBO, CB_SETCURSEL, 1, 0), 1);
    EQ(SendDlgItemMessageA(dlg, ID_COMBO, CB_GETLBTEXT, 1, (LPARAM)a), 4); EXPECT_STR(a, "beta");
    SendDlgItemMessageA(dlg, ID_COMBO, CB_SETITEMDATA, 0, 0x1234);
    EQ(SendDlgItemMessageA(dlg, ID_COMBO, CB_GETITEMDATA, 0, 0), 0x1234);
    DestroyWindow(dlg);
}
TEST(dialogs_senddlgitemmsg_missing_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageA); REQUIRE(SendDlgItemMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SET_ERR(0);
    EQ(SendDlgItemMessageA(dlg, 999, WM_GETTEXTLENGTH, 0, 0), 0);
    EXPECT_ERR(0, 1421);
    SET_ERR(0);
    EQ(SendDlgItemMessageW(dlg, 999, LB_GETCOUNT, 0, 0), 0);
    EXPECT_ERR(0, 1421);
    DestroyWindow(dlg);
}
TEST(dialogs_senddlgitemmsg_null_dialog) {
    REQUIRE(SendDlgItemMessageA); REQUIRE(SendDlgItemMessageW);
    SET_ERR(0);
    EQ(SendDlgItemMessageA(NULL, 100, WM_GETTEXTLENGTH, 0, 0), 0);
    EQ(SendDlgItemMessageW(NULL, 100, WM_GETTEXTLENGTH, 0, 0), 0);
    EQ(SendDlgItemMessageW((HWND)(ULONG_PTR)-1, 100, WM_GETTEXTLENGTH, 0, 0), 0);
    EQ(SendDlgItemMessageW((HWND)0x12345678, 100, WM_GETTEXTLENGTH, 0, 0), 0);
}
TEST(dialogs_senddlgitemmsg_unknown_message) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    EQ(SendDlgItemMessageA(dlg, ID_EDIT, WM_USER + 0x1234, 1, 2), 0);
    EQ(SendDlgItemMessageA(dlg, ID_STATIC, WM_APP + 5, 1, 2), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_senddlgitemmsg_getfont_setfont) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    /* the dialog manager sends WM_SETFONT to every control with the dialog font */
    HFONT df = (HFONT)SendDlgItemMessageA(dlg, ID_EDIT, WM_GETFONT, 0, 0);
    HFONT bf = (HFONT)SendDlgItemMessageA(dlg, ID_BTN, WM_GETFONT, 0, 0);
    HFONT sf = (HFONT)SendDlgItemMessageA(dlg, ID_STATIC, WM_GETFONT, 0, 0);
    CHECK(df != NULL);
    EQ(df, bf); EQ(df, sf);
    EQ(df, ds.font);                     /* same font the dialog procedure received */
    DestroyWindow(dlg);
}
TEST(dialogs_senddlgitemmsg_bm_getcheck) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    EQ(SendDlgItemMessageA(dlg, ID_CHK, BM_GETCHECK, 0, 0), 0);
    SendDlgItemMessageA(dlg, ID_CHK, BM_SETCHECK, 1, 0);
    EQ(SendDlgItemMessageA(dlg, ID_CHK, BM_GETCHECK, 0, 0), 1);
    EQ(SendDlgItemMessageA(dlg, ID_CHK, BM_GETSTATE, 0, 0) & 3, 1);
    DestroyWindow(dlg);
}

/* ---------------- DM_GETDEFID / DM_SETDEFID ---------------- */
TEST(dialogs_dm_getdefid_none) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    LRESULT r = SendMessageW(dlg, DM_GETDEFID, 0, 0);
    t_note("DM_GETDEFID with no default button -> 0x%llx", (unsigned long long)r);
    DestroyWindow(dlg);
}
TEST(dialogs_dm_setdefid_roundtrip) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(GetDlgItem); REQUIRE(GetWindowLongPtrW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    EQ(SendMessageW(dlg, DM_SETDEFID, ID_BTN, 0), TRUE);
    LRESULT r = SendMessageW(dlg, DM_GETDEFID, 0, 0);
    EQ(HIWORD(r), DC_HASDEFID);
    EQ(LOWORD(r), ID_BTN);
    EQ(GetWindowLongPtrW(GetDlgItem(dlg, ID_BTN), GWL_STYLE) & BS_TYPEMASK, BS_DEFPUSHBUTTON);
    DestroyWindow(dlg);
}
TEST(dialogs_dm_setdefid_switch) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(GetDlgItem); REQUIRE(GetWindowLongPtrW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    DT d; ds_reset();
    DestroyWindow(dlg);
    dt_hdr(&d, BASE_STYLE, 0, 2, 0, 0, 100, 60, NULL, NULL, L"d", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 5, 5, 40, 14, 1, C_BUTTON, L"A");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 50, 5, 40, 14, 2, C_BUTTON, L"B");
    dlg = CreateDialogIndirectParamW(GetModuleHandleA(NULL), (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    REQUIRE(dlg);
    SendMessageW(dlg, DM_SETDEFID, 1, 0);
    EQ(LOWORD(SendMessageW(dlg, DM_GETDEFID, 0, 0)), 1);
    EQ(GetWindowLongPtrW(GetDlgItem(dlg, 1), GWL_STYLE) & BS_TYPEMASK, BS_DEFPUSHBUTTON);
    SendMessageW(dlg, DM_SETDEFID, 2, 0);
    EQ(LOWORD(SendMessageW(dlg, DM_GETDEFID, 0, 0)), 2);
    EQ(GetWindowLongPtrW(GetDlgItem(dlg, 2), GWL_STYLE) & BS_TYPEMASK, BS_DEFPUSHBUTTON);
    EQ(GetWindowLongPtrW(GetDlgItem(dlg, 1), GWL_STYLE) & BS_TYPEMASK, BS_PUSHBUTTON);
    DestroyWindow(dlg);
}
TEST(dialogs_dm_defpushbutton_style_initial) {
    /* a BS_DEFPUSHBUTTON in the template becomes the default id */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 2, 0, 0, 100, 60, NULL, NULL, L"d", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 5, 5, 40, 14, 11, C_BUTTON, L"A");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 50, 5, 40, 14, 12, C_BUTTON, L"B");
    HWND dlg = CreateDialogIndirectParamW(GetModuleHandleA(NULL), (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    REQUIRE(dlg);
    LRESULT r = SendMessageW(dlg, DM_GETDEFID, 0, 0);
    t_note("DM_GETDEFID with template defpushbutton -> 0x%llx", (unsigned long long)r);
    DestroyWindow(dlg);
}
TEST(dialogs_dm_setdefid_clear_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SendMessageW(dlg, DM_SETDEFID, ID_BTN, 0);
    LRESULT r = SendMessageW(dlg, DM_SETDEFID, 0, 0);
    t_note("DM_SETDEFID(0) -> %lld, GETDEFID -> 0x%llx", (long long)r, (long long)SendMessageW(dlg, DM_GETDEFID, 0, 0));
    DestroyWindow(dlg);
}
TEST(dialogs_dm_setdefid_bad_id_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    LRESULT r = SendMessageW(dlg, DM_SETDEFID, 9999, 0);
    t_note("DM_SETDEFID(nonexistent) -> %lld GETDEFID -> 0x%llx", (long long)r, (long long)SendMessageW(dlg, DM_GETDEFID, 0, 0));
    r = SendMessageW(dlg, DM_SETDEFID, ID_EDIT, 0);
    t_note("DM_SETDEFID(edit) -> %lld GETDEFID -> 0x%llx", (long long)r, (long long)SendMessageW(dlg, DM_GETDEFID, 0, 0));
    DestroyWindow(dlg);
}
TEST(dialogs_dm_getdefid_on_plain_window) {
    REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    HWND p = t_make_window(NULL, WS_POPUP, 0, 0, 0, 20, 20, NULL);
    REQUIRE(p);
    EQ(SendMessageW(p, DM_GETDEFID, 0, 0), 0);   /* DefWindowProc ignores it */
    DestroyWindow(p);
}
TEST(dialogs_dm_message_constants) {
    EQ(DM_GETDEFID, WM_USER);
    EQ(DM_SETDEFID, WM_USER + 1);
    EQ(DM_REPOSITION, WM_USER + 2);
    EQ(DC_HASDEFID, 0x534B);
    EQ(DLGWINDOWEXTRA, 30);
}
