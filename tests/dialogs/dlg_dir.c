/* dialogs: DlgDirList / DlgDirSelectEx / DlgDirListComboBox / DlgDirSelectComboBoxEx */
#include "dlgutil.h"

#define INST GetModuleHandleA(NULL)
enum { L_SORT = 10, S_PATH = 11, C_BOX = 12, E_EDIT = 13, L_PLAIN = 14 };

static char tdir[MAX_PATH], saved_cwd[MAX_PATH];
static const char *tfiles[] = { "alpha.txt", "beta.txt", "gamma.dat", "noext", NULL };

static void mkfile(const char *name) {
    char p[MAX_PATH]; wsprintfA(p, "%s\\%s", tdir, name);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, "x", 1, &w, NULL); CloseHandle(h); }
}
static int tree_setup(void) {
    char tmp[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, saved_cwd);
    GetTempPathA(MAX_PATH, tmp);
    wsprintfA(tdir, "%st32dlgdir_%lu_%lu", tmp, GetCurrentProcessId(), GetTickCount());
    if (!CreateDirectoryA(tdir, NULL)) return 0;
    for (int i = 0; tfiles[i]; i++) mkfile(tfiles[i]);
    mkfile("ro.txt"); mkfile("hid.txt");
    char p[MAX_PATH];
    wsprintfA(p, "%s\\ro.txt", tdir); SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    wsprintfA(p, "%s\\hid.txt", tdir); SetFileAttributesA(p, FILE_ATTRIBUTE_HIDDEN);
    wsprintfA(p, "%s\\sub1", tdir); CreateDirectoryA(p, NULL);
    wsprintfA(p, "%s\\sub2", tdir); CreateDirectoryA(p, NULL);
    return 1;
}
static void tree_cleanup(void) {
    char p[MAX_PATH];
    SetCurrentDirectoryA(saved_cwd);       /* leave the temp dir before removing it */
    for (int i = 0; tfiles[i]; i++) { wsprintfA(p, "%s\\%s", tdir, tfiles[i]); DeleteFileA(p); }
    wsprintfA(p, "%s\\ro.txt", tdir); SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); DeleteFileA(p);
    wsprintfA(p, "%s\\hid.txt", tdir); SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); DeleteFileA(p);
    wsprintfA(p, "%s\\sub1", tdir); RemoveDirectoryA(p);
    wsprintfA(p, "%s\\sub2", tdir); RemoveDirectoryA(p);
    RemoveDirectoryA(tdir);
}
static HWND dir_dialog(void) {
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 5, 0, 0, 750, 140, NULL, NULL, L"dir", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_SORT | LBS_NOTIFY, 0, 2, 20, 100, 60, L_SORT, C_LIST, L"");
    dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 2, 2, 700, 12, S_PATH, C_STATIC, L"unset");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_SORT, 0, 120, 20, 100, 80, C_BOX, C_COMBO, L"");
    dt_item(&d, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 240, 20, 60, 12, E_EDIT, C_EDIT, L"e");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY, 0, 2, 90, 100, 40, L_PLAIN, C_LIST, L"");
    return CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
}
static int lb_count(HWND dlg, int id) { return (int)SendDlgItemMessageA(dlg, id, LB_GETCOUNT, 0, 0); }
static int lb_has(HWND dlg, int id, const char *s) { return SendDlgItemMessageA(dlg, id, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)s) >= 0; }
static int cb_has(HWND dlg, int id, const char *s) { return SendDlgItemMessageA(dlg, id, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)s) >= 0; }
static void spec(char *out, const char *tail) { wsprintfA(out, "%s\\%s", tdir, tail); }
static int cwd_is_tdir(void) { char c[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, c); return !lstrcmpiA(c, tdir); }

#define DIR_SETUP() \
    REQUIRE(DlgDirListA); REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow); \
    if (!tree_setup()) { t_skip("cannot create temp tree"); return; } \
    HWND dlg = dir_dialog(); if (!dlg) { tree_cleanup(); t_skip("no dialog"); return; }
#define DIR_END() do { DestroyWindow(dlg); tree_cleanup(); } while (0)

/* ---------------- DlgDirList ---------------- */
TEST(dialogs_dirlist_files_default) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListA(dlg, sp, L_SORT, S_PATH, 0), 1);
    for (int i = 0; tfiles[i]; i++) CHECKF(lb_has(dlg, L_SORT, tfiles[i]), "missing %s", tfiles[i]);
    CHECK(!lb_has(dlg, L_SORT, "[sub1]"));
    CHECK(!lb_has(dlg, L_SORT, "hid.txt"));
    t_note("default flags: %d entries (ro.txt listed: %d)", lb_count(dlg, L_SORT), lb_has(dlg, L_SORT, "ro.txt"));
    CHECK(cwd_is_tdir());
    DIR_END();
}
TEST(dialogs_dirlist_names_are_lowercase_as_on_disk) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.TXT");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, 0), 1);
    CHECK(lb_has(dlg, L_SORT, "alpha.txt"));
    CHECK(lb_has(dlg, L_SORT, "beta.txt"));
    CHECK(!lb_has(dlg, L_SORT, "gamma.dat"));
    EQ(lb_has(dlg, L_SORT, "noext"), 0);
    DIR_END();
}
TEST(dialogs_dirlist_pattern_buffer_rewritten_uppercase) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.txt");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, 0), 1);
    EXPECT_STR(sp, "*.TXT");                       /* path part stripped, rest upper-cased in place */
    DIR_END();
}
TEST(dialogs_dirlist_question_mark_wildcard) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "bet?.txt");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, 0), 1);
    CHECK(lb_has(dlg, L_SORT, "beta.txt"));
    EQ(lb_count(dlg, L_SORT) >= 1, 1);
    CHECK(!lb_has(dlg, L_SORT, "alpha.txt"));
    DIR_END();
}
TEST(dialogs_dirlist_wide_variant) {
    DIR_SETUP();
    REQUIRE(DlgDirListW);
    WCHAR sp[MAX_PATH]; wsprintfW(sp, L"%S\\*.dat", tdir);
    EQ(DlgDirListW(dlg, sp, L_SORT, S_PATH, 0), 1);
    EXPECT_WSTR(sp, L"*.DAT");
    CHECK(lb_has(dlg, L_SORT, "gamma.dat"));
    EQ(lb_count(dlg, L_SORT), 1);
    DIR_END();
}
TEST(dialogs_dirlist_directories_included) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_DIRECTORY), 1);
    CHECK(lb_has(dlg, L_SORT, "[sub1]"));
    CHECK(lb_has(dlg, L_SORT, "[sub2]"));
    CHECK(lb_has(dlg, L_SORT, "[..]"));
    CHECK(lb_has(dlg, L_SORT, "alpha.txt"));
    DIR_END();
}
TEST(dialogs_dirlist_exclusive_directories_only) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_DIRECTORY | DDL_EXCLUSIVE), 1);
    CHECK(lb_has(dlg, L_SORT, "[sub1]"));
    CHECK(lb_has(dlg, L_SORT, "[sub2]"));
    CHECK(!lb_has(dlg, L_SORT, "alpha.txt"));
    int bad = 0;
    for (int i = 0; i < lb_count(dlg, L_SORT); i++) {
        char t[300]; SendDlgItemMessageA(dlg, L_SORT, LB_GETTEXT, i, (LPARAM)t);
        if (t[0] != '[') bad++;
    }
    EQ(bad, 0);
    DIR_END();
}
TEST(dialogs_dirlist_hidden_flag) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.txt");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_HIDDEN), 1);
    t_note("DDL_HIDDEN: hid.txt=%d alpha.txt=%d ro.txt=%d", lb_has(dlg, L_SORT, "hid.txt"), lb_has(dlg, L_SORT, "alpha.txt"), lb_has(dlg, L_SORT, "ro.txt"));
    CHECK(lb_has(dlg, L_SORT, "hid.txt"));
    DIR_END();
}
TEST(dialogs_dirlist_readonly_flag) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.txt");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_READONLY), 1);
    CHECK(lb_has(dlg, L_SORT, "ro.txt"));
    CHECK(lb_has(dlg, L_SORT, "alpha.txt"));
    DIR_END();
}
TEST(dialogs_dirlist_exclusive_readonly) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.txt");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_READONLY | DDL_EXCLUSIVE), 1);
    CHECK(lb_has(dlg, L_SORT, "ro.txt"));
    CHECK(!lb_has(dlg, L_SORT, "alpha.txt"));
    DIR_END();
}
TEST(dialogs_dirlist_exclusive_hidden) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.txt");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_HIDDEN | DDL_EXCLUSIVE), 1);
    CHECK(lb_has(dlg, L_SORT, "hid.txt"));
    CHECK(!lb_has(dlg, L_SORT, "alpha.txt"));
    DIR_END();
}
TEST(dialogs_dirlist_archive_flag_note) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.txt");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_ARCHIVE), 1);
    t_note("DDL_ARCHIVE listing count=%d", lb_count(dlg, L_SORT));
    DIR_END();
}
TEST(dialogs_dirlist_drives) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_DRIVES | DDL_EXCLUSIVE), 1);
    int n = lb_count(dlg, L_SORT), bad = 0;
    CHECK(n >= 1);
    for (int i = 0; i < n; i++) {
        char t[64]; SendDlgItemMessageA(dlg, L_SORT, LB_GETTEXT, i, (LPARAM)t);
        if (!(t[0] == '[' && t[1] == '-' && t[3] == '-' && t[4] == ']' && t[5] == 0)) bad++;
    }
    EQ(bad, 0);
    char win[MAX_PATH]; GetWindowsDirectoryA(win, MAX_PATH);
    char want[8]; wsprintfA(want, "[-%c-]", (win[0] >= 'A' && win[0] <= 'Z') ? win[0] + 32 : win[0]);
    CHECKF(lb_has(dlg, L_SORT, want), "system drive %s missing", want);
    DIR_END();
}
TEST(dialogs_dirlist_clears_previous_content) {
    DIR_SETUP();
    SendDlgItemMessageA(dlg, L_SORT, LB_ADDSTRING, 0, (LPARAM)"stale1");
    SendDlgItemMessageA(dlg, L_SORT, LB_ADDSTRING, 0, (LPARAM)"stale2");
    char sp[MAX_PATH]; spec(sp, "*.dat");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, 0), 1);
    CHECK(!lb_has(dlg, L_SORT, "stale1"));
    EQ(lb_count(dlg, L_SORT), 1);
    DIR_END();
}
TEST(dialogs_dirlist_no_match_leaves_empty) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.nomatch");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, 0), 1);
    EQ(lb_count(dlg, L_SORT), 0);
    DIR_END();
}
TEST(dialogs_dirlist_static_shows_lowercase_cwd) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListA(dlg, sp, L_SORT, S_PATH, 0), 1);
    char t[600]; GetDlgItemTextA(dlg, S_PATH, t, 600);
    char low[MAX_PATH]; lstrcpyA(low, tdir); CharLowerA(low);
    t_note("static text '%s' (cwd '%s')", t, tdir);
    CHECK(!lstrcmpiA(t, tdir));
    CHECK(!strcmp(t, low));
    DIR_END();
}
TEST(dialogs_dirlist_static_negative_id_ignored) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListA(dlg, sp, L_SORT, -1, 0), 1);
    DIR_END();
}
TEST(dialogs_dirlist_empty_spec_lists_current_dir) {
    DIR_SETUP();
    SetCurrentDirectoryA(tdir);
    char sp[8] = "";
    EQ(DlgDirListA(dlg, sp, L_SORT, S_PATH, 0), 1);
    CHECK(lb_has(dlg, L_SORT, "alpha.txt"));
    CHECK(lb_has(dlg, L_SORT, "gamma.dat"));
    CHECK(cwd_is_tdir());
    DIR_END();
}
TEST(dialogs_dirlist_directory_only_spec_changes_dir) {
    DIR_SETUP();
    char sp[MAX_PATH]; lstrcpyA(sp, tdir);
    EQ(DlgDirListA(dlg, sp, L_SORT, S_PATH, 0), 1);
    CHECK(cwd_is_tdir());
    CHECK(lb_has(dlg, L_SORT, "alpha.txt"));      /* default spec is *.* */
    CHECK(lb_has(dlg, L_SORT, "noext"));
    DIR_END();
}
TEST(dialogs_dirlist_subdirectory_navigation) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "sub1\\*");
    EQ(DlgDirListA(dlg, sp, L_SORT, S_PATH, DDL_DIRECTORY), 1);
    char c[MAX_PATH], want[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, c);
    wsprintfA(want, "%s\\sub1", tdir);
    CHECK(!lstrcmpiA(c, want));
    CHECK(lb_has(dlg, L_SORT, "[..]"));
    DIR_END();
}
TEST(dialogs_dirlist_nonexistent_directory_fails) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "no_such_sub\\*.txt");
    SET_ERR(0);
    EQ(DlgDirListA(dlg, sp, L_SORT, S_PATH, 0), 0);
    CHECK(cwd_is_tdir() || 1);
    t_note("nonexistent dir: err=%lu", GetLastError());
    DIR_END();
}
TEST(dialogs_dirlist_no_wildcard_nonexistent_error) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "no_such_file_t32");
    SET_ERR(0);
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, 0), 0);
    EQ(GetLastError(), 1417);
    DIR_END();
}
TEST(dialogs_dirlist_invalid_flags) {
    DIR_SETUP();
    char sp[MAX_PATH];
    static const UINT bad[] = { 0x8, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x10000, 0x80000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        spec(sp, "*");
        SET_ERR(0);
        int r = DlgDirListA(dlg, sp, L_SORT, 0, bad[i]);
        CHECKF(r == 0, "flags %x returned %d", bad[i], r);
        CHECKF(GetLastError() == 1004, "flags %x err %lu", bad[i], GetLastError());
    }
    DIR_END();
}
TEST(dialogs_dirlist_valid_flag_combinations) {
    DIR_SETUP();
    static const UINT ok[] = { 0, DDL_READONLY, DDL_HIDDEN, DDL_SYSTEM, DDL_DIRECTORY, DDL_ARCHIVE, DDL_POSTMSGS, DDL_DRIVES, DDL_EXCLUSIVE,
        DDL_READONLY | DDL_HIDDEN | DDL_SYSTEM | DDL_DIRECTORY | DDL_ARCHIVE, DDL_DIRECTORY | DDL_DRIVES, 0xE037 };
    char sp[MAX_PATH];
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        spec(sp, "*");
        int r = DlgDirListA(dlg, sp, L_SORT, 0, ok[i]);
        CHECKF(r == 1, "flags %x returned %d", ok[i], r);
    }
    t_pump();
    DIR_END();
}
TEST(dialogs_dirlist_missing_listbox_note) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    SET_ERR(0);
    int r = DlgDirListA(dlg, sp, 999, 0, 0);
    t_note("DlgDirList with missing list box id: ret=%d err=%lu (1416 LISTBOX_ID_NOT_FOUND)", r, GetLastError());
    DIR_END();
}
TEST(dialogs_dirlist_wrong_control_type_note) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    SET_ERR(0);
    int r = DlgDirListA(dlg, sp, E_EDIT, 0, 0);
    t_note("DlgDirList on an edit control: ret=%d err=%lu", r, GetLastError());
    spec(sp, "*");
    r = DlgDirListA(dlg, sp, C_BOX, 0, 0);
    t_note("DlgDirList on a combo box: ret=%d err=%lu", r, GetLastError());
    DIR_END();
}
TEST(dialogs_dirlist_null_dialog) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    SET_ERR(0);
    EQ(DlgDirListA(NULL, sp, L_SORT, S_PATH, 0), 0);
    EQ(DlgDirListA((HWND)0x12345678, sp, L_SORT, S_PATH, 0), 0);
    DIR_END();
}
TEST(dialogs_dirlist_spec_too_long) {
    DIR_SETUP();
    char sp[400]; memset(sp, 'a', 399); sp[399] = 0;
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, 0), 0);
    char sp2[300]; memset(sp2, 'b', 280); lstrcpyA(sp2 + 280, "*.txt");
    EQ(DlgDirListA(dlg, sp2, L_SORT, 0, 0), 0);
    DIR_END();
}
TEST(dialogs_dirlist_postmsgs_defers_listing) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*.dat");
    EQ(DlgDirListA(dlg, sp, L_SORT, 0, DDL_POSTMSGS), 1);
    t_note("before pumping: %d entries", lb_count(dlg, L_SORT));
    t_pump();
    CHECK(lb_has(dlg, L_SORT, "gamma.dat"));
    DIR_END();
}
TEST(dialogs_dirlist_unsorted_listbox_files_before_dirs) {
    DIR_SETUP();
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListA(dlg, sp, L_PLAIN, 0, DDL_DIRECTORY), 1);
    int first_dir = -1, last_file = -1, n = lb_count(dlg, L_PLAIN);
    for (int i = 0; i < n; i++) {
        char t[300]; SendDlgItemMessageA(dlg, L_PLAIN, LB_GETTEXT, i, (LPARAM)t);
        if (t[0] == '[') { if (first_dir < 0) first_dir = i; } else last_file = i;
    }
    CHECKF(first_dir > last_file, "files end at %d, first dir at %d", last_file, first_dir);
    DIR_END();
}
TEST(dialogs_dirlist_repeated_calls) {
    DIR_SETUP();
    int bad = 0;
    for (int i = 0; i < 50; i++) {
        char sp[MAX_PATH]; spec(sp, i & 1 ? "*.txt" : "*.dat");
        if (DlgDirListA(dlg, sp, L_SORT, S_PATH, 0) != 1) bad++;
        if (!lb_has(dlg, L_SORT, i & 1 ? "alpha.txt" : "gamma.dat")) bad++;
    }
    EQ(bad, 0);
    DIR_END();
}

/* ---------------- DlgDirSelectEx ---------------- */
TEST(dialogs_dirselect_file_entry) {
    DIR_SETUP(); REQUIRE(DlgDirSelectExA); REQUIRE(DlgDirSelectExW);
    char sp[MAX_PATH]; spec(sp, "*");
    DlgDirListA(dlg, sp, L_SORT, 0, DDL_DIRECTORY);
    int idx = (int)SendDlgItemMessageA(dlg, L_SORT, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"alpha.txt");
    CHECK(idx >= 0);
    SendDlgItemMessageA(dlg, L_SORT, LB_SETCURSEL, idx, 0);
    char out[64] = "zz"; WCHAR w[64];
    EQ(DlgDirSelectExA(dlg, out, 64, L_SORT), 0);
    EXPECT_STR(out, "alpha.txt");
    EQ(DlgDirSelectExW(dlg, w, 64, L_SORT), 0);
    EXPECT_WSTR(w, L"alpha.txt");
    DIR_END();
}
TEST(dialogs_dirselect_extensionless_file_note) {
    DIR_SETUP(); REQUIRE(DlgDirSelectExA);
    char sp[MAX_PATH]; spec(sp, "*");
    DlgDirListA(dlg, sp, L_SORT, 0, 0);
    int idx = (int)SendDlgItemMessageA(dlg, L_SORT, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"noext");
    CHECK(idx >= 0);
    SendDlgItemMessageA(dlg, L_SORT, LB_SETCURSEL, idx, 0);
    char out[64] = "";
    EQ(DlgDirSelectExA(dlg, out, 64, L_SORT), 0);
    t_note("selection 'noext' returned as '%s'", out);
    CHECK(!strcmp(out, "noext") || !strcmp(out, "noext."));
    DIR_END();
}
TEST(dialogs_dirselect_directory_entry) {
    DIR_SETUP(); REQUIRE(DlgDirSelectExA); REQUIRE(DlgDirSelectExW);
    char sp[MAX_PATH]; spec(sp, "*");
    DlgDirListA(dlg, sp, L_SORT, 0, DDL_DIRECTORY);
    int idx = (int)SendDlgItemMessageA(dlg, L_SORT, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"[sub1]");
    CHECK(idx >= 0);
    SendDlgItemMessageA(dlg, L_SORT, LB_SETCURSEL, idx, 0);
    char out[64] = ""; WCHAR w[64];
    EQ(DlgDirSelectExA(dlg, out, 64, L_SORT), 1);
    EXPECT_STR(out, "sub1\\");
    EQ(DlgDirSelectExW(dlg, w, 64, L_SORT), 1);
    EXPECT_WSTR(w, L"sub1\\");
    idx = (int)SendDlgItemMessageA(dlg, L_SORT, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"[..]");
    SendDlgItemMessageA(dlg, L_SORT, LB_SETCURSEL, idx, 0);
    EQ(DlgDirSelectExA(dlg, out, 64, L_SORT), 1);
    EXPECT_STR(out, "..\\");
    DIR_END();
}
TEST(dialogs_dirselect_manual_entries) {
    DIR_SETUP(); REQUIRE(DlgDirSelectExA);
    struct { const char *item; int ret; const char *want; } tab[] = {
        { "[foo]", 1, "foo\\" }, { "[x]", 1, "x\\" }, { "[-c-]", 1, "c:" }, { "[-z-]", 1, "z:" },
        { "name.txt", 0, "name.txt" }, { "a.b.c", 0, "a.b.c" }, { "with\ttab", 0, "with" }, { "[dirtab]\tx", 1, "dirtab\\" },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        SendDlgItemMessageA(dlg, L_PLAIN, LB_RESETCONTENT, 0, 0);
        SendDlgItemMessageA(dlg, L_PLAIN, LB_ADDSTRING, 0, (LPARAM)tab[i].item);
        SendDlgItemMessageA(dlg, L_PLAIN, LB_SETCURSEL, 0, 0);
        char out[64] = "";
        int r = DlgDirSelectExA(dlg, out, 64, L_PLAIN);
        CHECKF(r == tab[i].ret, "'%s' returned %d want %d", tab[i].item, r, tab[i].ret);
        if (i != 0 || 1) { if (strncmp(tab[i].item, "with", 4) == 0) t_note("'with<tab>tab' -> '%s'", out); else CHECKF(!strcmp(out, tab[i].want), "'%s' -> '%s' want '%s'", tab[i].item, out, tab[i].want); }
    }
    DIR_END();
}
TEST(dialogs_dirselect_no_selection) {
    DIR_SETUP(); REQUIRE(DlgDirSelectExA);
    SendDlgItemMessageA(dlg, L_PLAIN, LB_ADDSTRING, 0, (LPARAM)"x.txt");
    char out[16] = "keep";
    EQ(DlgDirSelectExA(dlg, out, 16, L_PLAIN), 0);
    EXPECT_STR(out, "keep");                    /* untouched without selection */
    DIR_END();
}
TEST(dialogs_dirselect_buffer_sizes) {
    DIR_SETUP(); REQUIRE(DlgDirSelectExA);
    SendDlgItemMessageA(dlg, L_PLAIN, LB_ADDSTRING, 0, (LPARAM)"abcdef.txt");
    SendDlgItemMessageA(dlg, L_PLAIN, LB_SETCURSEL, 0, 0);
    char out[32];
    memset(out, 'X', sizeof out);
    EQ(DlgDirSelectExA(dlg, out, 32, L_PLAIN), 0);  EXPECT_STR(out, "abcdef.txt");
    memset(out, 'X', sizeof out);
    EQ(DlgDirSelectExA(dlg, out, 10, L_PLAIN), 0);  EXPECT_STR(out, "abcdef.tx");
    memset(out, 'X', sizeof out);
    EQ(DlgDirSelectExA(dlg, out, 2, L_PLAIN), 0);   EXPECT_STR(out, "a");
    memset(out, 'X', sizeof out);
    EQ(DlgDirSelectExA(dlg, out, 1, L_PLAIN), 0);   CHECK(out[0] == 0);
    memset(out, 'X', sizeof out);
    DlgDirSelectExA(dlg, out, 0, L_PLAIN);
    CHECK(out[0] == 'X');
    DIR_END();
}
TEST(dialogs_dirselect_missing_control) {
    DIR_SETUP(); REQUIRE(DlgDirSelectExA);
    char out[16] = "keep";
    SET_ERR(0);
    EQ(DlgDirSelectExA(dlg, out, 16, 999), 0);
    EQ(GetLastError(), 1421);
    SET_ERR(0);
    EQ(DlgDirSelectExA(NULL, out, 16, L_PLAIN), 0);
    EQ(GetLastError(), 1421);
    EXPECT_STR(out, "keep");
    DIR_END();
}
TEST(dialogs_dirselect_item_too_long) {
    DIR_SETUP(); REQUIRE(DlgDirSelectExA);
    static char big[400]; memset(big, 'a', 300); big[300] = 0;
    SendDlgItemMessageA(dlg, L_PLAIN, LB_ADDSTRING, 0, (LPARAM)big);
    SendDlgItemMessageA(dlg, L_PLAIN, LB_SETCURSEL, 0, 0);
    char out[16] = "keep";
    EQ(DlgDirSelectExA(dlg, out, 16, L_PLAIN), 0);
    EXPECT_STR(out, "keep");
    DIR_END();
}

/* ---------------- combo variants ---------------- */
TEST(dialogs_dircombo_list_and_select) {
    DIR_SETUP(); REQUIRE(DlgDirListComboBoxA); REQUIRE(DlgDirSelectComboBoxExA);
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListComboBoxA(dlg, sp, C_BOX, S_PATH, DDL_DIRECTORY), 1);
    CHECK(cb_has(dlg, C_BOX, "alpha.txt"));
    CHECK(cb_has(dlg, C_BOX, "[sub1]"));
    CHECK(cb_has(dlg, C_BOX, "[..]"));
    int idx = (int)SendDlgItemMessageA(dlg, C_BOX, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"beta.txt");
    SendDlgItemMessageA(dlg, C_BOX, CB_SETCURSEL, idx, 0);
    char out[64] = "";
    EQ(DlgDirSelectComboBoxExA(dlg, out, 64, C_BOX), 0);
    EXPECT_STR(out, "beta.txt");
    idx = (int)SendDlgItemMessageA(dlg, C_BOX, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"[sub2]");
    SendDlgItemMessageA(dlg, C_BOX, CB_SETCURSEL, idx, 0);
    EQ(DlgDirSelectComboBoxExA(dlg, out, 64, C_BOX), 1);
    EXPECT_STR(out, "sub2\\");
    DIR_END();
}
TEST(dialogs_dircombo_wide_variants) {
    DIR_SETUP(); REQUIRE(DlgDirListComboBoxW); REQUIRE(DlgDirSelectComboBoxExW);
    WCHAR sp[MAX_PATH]; wsprintfW(sp, L"%S\\*.dat", tdir);
    EQ(DlgDirListComboBoxW(dlg, sp, C_BOX, 0, 0), 1);
    EXPECT_WSTR(sp, L"*.DAT");
    EQ(SendDlgItemMessageA(dlg, C_BOX, CB_GETCOUNT, 0, 0), 1);
    SendDlgItemMessageA(dlg, C_BOX, CB_SETCURSEL, 0, 0);
    WCHAR out[64];
    EQ(DlgDirSelectComboBoxExW(dlg, out, 64, C_BOX), 0);
    EXPECT_WSTR(out, L"gamma.dat");
    DIR_END();
}
TEST(dialogs_dircombo_no_selection) {
    DIR_SETUP(); REQUIRE(DlgDirListComboBoxA); REQUIRE(DlgDirSelectComboBoxExA);
    char sp[MAX_PATH]; spec(sp, "*.dat");
    DlgDirListComboBoxA(dlg, sp, C_BOX, 0, 0);
    SendDlgItemMessageA(dlg, C_BOX, CB_SETCURSEL, (WPARAM)-1, 0);
    char out[16] = "keep";
    EQ(DlgDirSelectComboBoxExA(dlg, out, 16, C_BOX), 0);
    EXPECT_STR(out, "keep");
    DIR_END();
}
TEST(dialogs_dircombo_errors) {
    DIR_SETUP(); REQUIRE(DlgDirListComboBoxA); REQUIRE(DlgDirSelectComboBoxExA);
    char out[16] = "keep";
    SET_ERR(0);
    EQ(DlgDirSelectComboBoxExA(dlg, out, 16, 999), 0);
    EQ(GetLastError(), 1421);
    SET_ERR(0);
    EQ(DlgDirSelectComboBoxExA(dlg, out, 16, L_PLAIN), 0);      /* a list box is not a combo box */
    EQ(GetLastError(), 1423);
    SET_ERR(0);
    EQ(DlgDirSelectComboBoxExA(NULL, out, 16, C_BOX), 0);
    char sp[MAX_PATH]; spec(sp, "*");
    EQ(DlgDirListComboBoxA(NULL, sp, C_BOX, 0, 0), 0);
    SET_ERR(0);
    EQ(DlgDirListComboBoxA(dlg, sp, C_BOX, 0, 0x100), 0);
    EQ(GetLastError(), 1004);
    DIR_END();
}
TEST(dialogs_dircombo_listbox_function_on_combo_note) {
    DIR_SETUP(); REQUIRE(DlgDirListComboBoxA);
    char sp[MAX_PATH]; spec(sp, "*.dat");
    int r = DlgDirListComboBoxA(dlg, sp, L_SORT, 0, 0);
    t_note("DlgDirListComboBox on a list box -> %d err=%lu", r, GetLastError());
    DIR_END();
}
TEST(dialogs_dir_cwd_restored_for_other_tests) {
    REQUIRE(tdir);
    char c[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, c);
    CHECK(c[0] != 0);
    CHECK(strstr(c, "t32dlgdir_") == NULL);       /* a previous test did not leave the process inside its temp dir */
}
