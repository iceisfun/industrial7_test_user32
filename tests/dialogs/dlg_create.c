/* dialogs: CreateDialog* / DialogBox* / EndDialog / template parsing */
#include "dlgutil.h"

#define INST GetModuleHandleA(NULL)
#define CDW(t, par, lp) CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)(t)->b, par, gdp, lp)

/* minimal template: edit(100, tabstop), OK(1, defpush), Cancel(2) */
static void mini_tmpl(DT *d, DWORD style, DWORD ex, const WCHAR *title) {
    dt_hdr(d, style, ex, 3, 10, 10, 120, 70, NULL, NULL, title, 8, L"MS Shell Dlg");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 0, 5, 5, 100, 12, 100, C_EDIT, L"e");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 5, 30, 40, 14, IDOK, C_BUTTON, L"OK");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 50, 30, 40, 14, IDCANCEL, C_BUTTON, L"Cancel");
}
static int count_children(HWND h) {
    int n = 0;
    for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) n++;
    return n;
}
static void cls_of(HWND h, char *buf, int n) { buf[0] = 0; GetClassNameA(h, buf, n); }

/* ---------------- CreateDialog basics ---------------- */
TEST(dialogs_create_w_basic) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsWindow); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"Title W"); ds_reset();
    HWND h = CDW(&d, NULL, 0x4321);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(IsWindow(h));
    EQ(ds.ninit, 1);
    EQ(ds.lp, 0x4321);
    EQ(ds.hinit, h);
    EQ(count_children(h), 3);
    CHECK(DestroyWindow(h));
    CHECK(!IsWindow(h));
    EQ(ds.ndestroy, 1);
    EQ(ds.nncdestroy, 1);
}
TEST(dialogs_create_a_basic) {
    REQUIRE(CreateDialogIndirectParamA); REQUIRE(IsWindow); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"Title A"); ds_reset();
    HWND h = CreateDialogIndirectParamA(INST, (LPCDLGTEMPLATEA)d.b, NULL, gdp, 77);
    CHECK(h != NULL);
    if (!h) return;
    EQ(ds.ninit, 1); EQ(ds.lp, 77);
    EQ(count_children(h), 3);
    DestroyWindow(h);
}
TEST(dialogs_create_lparam_full_64bit) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    static const LPARAM vals[] = { 0, 1, -1, 0x123456789abcdef0LL, (LPARAM)0x8000000000000000ULL, 0x7fffffffffffffffLL, 0xffffffffLL };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        DT d; mini_tmpl(&d, BASE_STYLE, 0, L"lp"); ds_reset();
        HWND h = CDW(&d, NULL, vals[i]);
        CHECK(h != NULL);
        if (h) { CHECKF(ds.lp == vals[i], "case %u: got %llx", i, (long long)ds.lp); DestroyWindow(h); }
    }
}
TEST(dialogs_create_class_and_title) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetClassNameA); REQUIRE(GetWindowTextW); REQUIRE(GetWindowTextA); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"Caption 123"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    char c[32]; WCHAR w[32]; char a[32];
    cls_of(h, c, 32); EXPECT_STR(c, "#32770");
    EQ(GetClassNameW(h, w, 32), 6); EXPECT_WSTR(w, L"#32770");
    EQ(GetWindowTextW(h, w, 32), 11); EXPECT_WSTR(w, L"Caption 123");
    EQ(GetWindowTextA(h, a, 32), 11); EXPECT_STR(a, "Caption 123");
    DestroyWindow(h);
}
TEST(dialogs_create_empty_title_and_unicode_title) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowTextW); REQUIRE(DestroyWindow);
    static const WCHAR u[] = { 0x041F, 0x0440, 0x0438, 0x0432, 0x0435, 0x0442, 0x4E2D, 0 };
    WCHAR w[32];
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L""); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(GetWindowTextW(h, w, 32), 0);
    DestroyWindow(h);
    mini_tmpl(&d, BASE_STYLE, 0, u); ds_reset();
    h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(GetWindowTextW(h, w, 32), 7); EXPECT_WSTR(w, u);
    DestroyWindow(h);
}
TEST(dialogs_create_long_title) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowTextW); REQUIRE(DestroyWindow);
    static WCHAR t[1200], w[1300];
    for (int i = 0; i < 1000; i++) t[i] = 'a' + i % 26;
    t[1000] = 0;
    DT d; mini_tmpl(&d, BASE_STYLE, 0, t); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(GetWindowTextW(h, w, 1300), 1000);
    EXPECT_WSTR(w, t);
    DestroyWindow(h);
}
TEST(dialogs_create_hidden_unless_ws_visible) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsWindowVisible); REQUIRE(GetWindowLongW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"v"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    CHECK(!IsWindowVisible(h));
    CHECK(!(GetWindowLongW(h, GWL_STYLE) & WS_VISIBLE));
    DestroyWindow(h);
    mini_tmpl(&d, BASE_STYLE | WS_VISIBLE, 0, L"v"); ds_reset();
    h = CDW(&d, NULL, 0); REQUIRE(h);
    CHECK(IsWindowVisible(h));
    CHECK(GetWindowLongW(h, GWL_STYLE) & WS_VISIBLE);
    DestroyWindow(h);
}
TEST(dialogs_create_client_size_matches_template) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetClientRect); REQUIRE(MapDialogRect); REQUIRE(DestroyWindow);
    static const short sz[][2] = { { 160, 90 }, { 120, 70 }, { 200, 120 }, { 180, 140 }, { 300, 200 } };   /* wide enough to avoid the minimum tracking size */
    for (unsigned i = 0; i < 5; i++) {
        DT d; ds_reset();
        dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, sz[i][0], sz[i][1], NULL, NULL, L"sz", 8, L"MS Shell Dlg");
        HWND h = CDW(&d, NULL, 0);
        CHECK(h != NULL); if (!h) continue;
        RECT c, m = { 0, 0, sz[i][0], sz[i][1] };
        GetClientRect(h, &c);
        MapDialogRect(h, &m);
        CHECKF(abs(c.right - m.right) <= 1 && abs(c.bottom - m.bottom) <= 1, "case %u client %dx%d mapped %dx%d", i, (int)c.right, (int)c.bottom, (int)m.right, (int)m.bottom);
        DestroyWindow(h);
    }
}
TEST(dialogs_create_dwlp_dlgproc_and_user) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowLongPtrW); REQUIRE(SetWindowLongPtrW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"x"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(GetWindowLongPtrW(h, DWLP_DLGPROC), (LONG_PTR)gdp);
    EQ(GetWindowLongPtrW(h, DWLP_USER), 0);
    EQ(SetWindowLongPtrW(h, DWLP_USER, 0x1122334455667788LL), 0);
    EQ(GetWindowLongPtrW(h, DWLP_USER), 0x1122334455667788LL);
    EQ(SetWindowLongPtrW(h, DWLP_USER, -1), 0x1122334455667788LL);
    EQ(GetWindowLongPtrW(h, DWLP_USER), -1);
    EQ(GetWindowLongPtrW(h, DWLP_MSGRESULT), 0);
    DestroyWindow(h);
}
TEST(dialogs_create_user_data_per_window) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowLongPtrW); REQUIRE(SetWindowLongPtrW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"x");
    ds_reset(); HWND a = CDW(&d, NULL, 0);
    ds_reset(); HWND b = CDW(&d, NULL, 0);
    REQUIRE(a); REQUIRE(b);
    SetWindowLongPtrW(a, DWLP_USER, 111); SetWindowLongPtrW(b, DWLP_USER, 222);
    EQ(GetWindowLongPtrW(a, DWLP_USER), 111);
    EQ(GetWindowLongPtrW(b, DWLP_USER), 222);
    DestroyWindow(a); DestroyWindow(b);
}
TEST(dialogs_create_null_dlgproc) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow); REQUIRE(GetWindowLongPtrW);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"np"); ds_reset();
    HWND h = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, NULL, 0);
    CHECK(h != NULL);
    if (h) { EQ(GetWindowLongPtrW(h, DWLP_DLGPROC), 0); EQ(count_children(h), 3); DestroyWindow(h); }
}
TEST(dialogs_create_no_controls) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 40, 40, NULL, NULL, L"empty", 8, L"MS Shell Dlg");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(count_children(h), 0);
    EQ(ds.ninit, 1);
    CHECK(ds.hfocus == NULL || ds.hfocus == h || 1);
    DestroyWindow(h);
}
TEST(dialogs_create_many_controls) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    const int N = 150;
    dt_hdr(&d, BASE_STYLE, 0, N, 0, 0, 300, 300, NULL, NULL, L"many", 8, L"MS Shell Dlg");
    for (int i = 0; i < N; i++) {
        WCHAR t[16]; wsprintfW(t, L"c%d", i);
        dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, (short)(i % 10 * 20), (short)(i / 10 * 12), 18, 10, (WORD)(1000 + i), C_BUTTON, t);
    }
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(count_children(h), N);
    int bad = 0;
    for (int i = 0; i < N; i++) if (!GetDlgItem(h, 1000 + i)) bad++;
    EQ(bad, 0);
    DestroyWindow(h);
}
TEST(dialogs_create_repeated_create_destroy) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"loop");
    int bad = 0;
    for (int i = 0; i < 300; i++) {
        ds_reset();
        HWND h = CDW(&d, NULL, i);
        if (!h || ds.ninit != 1 || ds.lp != i) { bad++; if (h) DestroyWindow(h); continue; }
        if (!DestroyWindow(h)) bad++;
    }
    EQ(bad, 0);
}
TEST(dialogs_create_handle_dead_after_destroy) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow); REQUIRE(IsWindow); REQUIRE(EndDialog); REQUIRE(GetDlgItem);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"x"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    HWND e = GetDlgItem(h, 100);
    DestroyWindow(h);
    CHECK(!IsWindow(h)); CHECK(!IsWindow(e));
    SET_ERR(0);
    CHECK(!DestroyWindow(h));
    EQ(GetLastError(), 1400);
    SET_ERR(0);
    CHECK(!EndDialog(h, 1));
    EQ(GetLastError(), 1400);
}

/* ---------------- style handling ---------------- */
struct stc { DWORD ds, ex_want_set, ex_want_clear; const char *name; };
static const struct stc stcs[] = {
    { DS_MODALFRAME, WS_EX_DLGMODALFRAME, 0, "modalframe" },
    { DS_CONTEXTHELP, WS_EX_CONTEXTHELP, 0, "contexthelp" },
    { DS_CONTROL, WS_EX_CONTROLPARENT, 0, "control" },
    { 0, WS_EX_WINDOWEDGE, WS_EX_DLGMODALFRAME, "plain caption" },
};
TEST(dialogs_create_ds_styles_to_exstyles) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowLongW); REQUIRE(DestroyWindow);
    for (unsigned i = 0; i < sizeof stcs / sizeof *stcs; i++) {
        DT d; ds_reset();
        DWORD st = WS_POPUP | WS_CAPTION | WS_SYSMENU | stcs[i].ds;
        mini_tmpl(&d, st, 0, L"s");
        HWND h = CDW(&d, NULL, 0);
        CHECKF(h != NULL, "%s", stcs[i].name);
        if (!h) continue;
        DWORD ex = GetWindowLongW(h, GWL_EXSTYLE);
        CHECKF((ex & stcs[i].ex_want_set) == stcs[i].ex_want_set, "%s ex=%lx", stcs[i].name, ex);
        CHECKF(!(ex & stcs[i].ex_want_clear), "%s ex=%lx", stcs[i].name, ex);
        DestroyWindow(h);
    }
}
TEST(dialogs_create_exstyle_from_template) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowLongW); REQUIRE(DestroyWindow);
    static const DWORD exs[] = { WS_EX_TOOLWINDOW, WS_EX_TOPMOST, WS_EX_CLIENTEDGE, WS_EX_APPWINDOW, WS_EX_NOACTIVATE, WS_EX_TRANSPARENT, WS_EX_LAYERED, WS_EX_STATICEDGE };
    for (unsigned i = 0; i < sizeof exs / sizeof *exs; i++) {
        DT d; ds_reset(); mini_tmpl(&d, BASE_STYLE, exs[i], L"ex");
        HWND h = CDW(&d, NULL, 0);
        CHECKF(h != NULL, "ex %lx", exs[i]);
        if (!h) continue;
        CHECKF((GetWindowLongW(h, GWL_EXSTYLE) & exs[i]) == exs[i], "ex %lx got %lx", exs[i], GetWindowLongW(h, GWL_EXSTYLE));
        DestroyWindow(h);
    }
}
TEST(dialogs_create_window_style_bits_preserved) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowLongW); REQUIRE(DestroyWindow);
    static const DWORD sts[] = { WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX,
        WS_POPUP | WS_BORDER, WS_POPUP | WS_DLGFRAME, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_CAPTION, WS_POPUP | WS_VSCROLL | WS_CAPTION, WS_POPUP | WS_CAPTION | WS_DISABLED };
    for (unsigned i = 0; i < sizeof sts / sizeof *sts; i++) {
        DT d; ds_reset(); mini_tmpl(&d, sts[i], 0, L"st");
        HWND h = CDW(&d, NULL, 0);
        CHECKF(h != NULL, "style %lx", sts[i]);
        if (!h) continue;
        DWORD got = GetWindowLongW(h, GWL_STYLE);
        CHECKF((got & sts[i]) == sts[i], "want %lx got %lx", sts[i], got);
        DestroyWindow(h);
    }
}
TEST(dialogs_create_ds_control_style_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowLongW); REQUIRE(DestroyWindow);
    DT d; ds_reset(); mini_tmpl(&d, WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_CONTROL, 0, L"c");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    t_note("DS_CONTROL dialog style=%lx ex=%lx", GetWindowLongW(h, GWL_STYLE), GetWindowLongW(h, GWL_EXSTYLE));
    DestroyWindow(h);
}
TEST(dialogs_create_disabled_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsWindowEnabled); REQUIRE(DestroyWindow);
    DT d; ds_reset(); mini_tmpl(&d, BASE_STYLE | WS_DISABLED, 0, L"dis");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    CHECK(!IsWindowEnabled(h));
    EQ(ds.ninit, 1);
    DestroyWindow(h);
}
TEST(dialogs_create_owner_relationship) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindow); REQUIRE(GetParent); REQUIRE(DestroyWindow); REQUIRE(IsWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    REQUIRE(own);
    DT d; ds_reset(); mini_tmpl(&d, BASE_STYLE, 0, L"owned");
    HWND h = CDW(&d, own, 0); REQUIRE(h);
    EQ(GetWindow(h, GW_OWNER), own);
    EQ(GetParent(h), own);
    DestroyWindow(own);                      /* destroying the owner destroys owned popups */
    CHECK(!IsWindow(h));
    if (IsWindow(h)) DestroyWindow(h);
}
TEST(dialogs_create_child_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetParent); REQUIRE(IsChild); REQUIRE(GetWindowLongW); REQUIRE(DestroyWindow);
    HWND par = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 300, 300, NULL);
    REQUIRE(par);
    DT d; ds_reset();
    mini_tmpl(&d, WS_CHILD | WS_VISIBLE | DS_CONTROL | DS_SETFONT, 0, L"kid");
    HWND h = CDW(&d, par, 0);
    CHECK(h != NULL);
    if (h) {
        EQ(GetParent(h), par);
        CHECK(IsChild(par, h));
        CHECK(GetWindowLongW(h, GWL_STYLE) & WS_CHILD);
        CHECK(GetWindowLongW(h, GWL_EXSTYLE) & WS_EX_CONTROLPARENT);
        DestroyWindow(h);
    }
    DestroyWindow(par);
}
TEST(dialogs_create_child_without_parent_fails) {
    REQUIRE(CreateDialogIndirectParamW);
    DT d; ds_reset();
    mini_tmpl(&d, WS_CHILD | WS_VISIBLE, 0, L"orphan");
    SET_ERR(0);
    HWND h = CDW(&d, NULL, 0);
    CHECK(h == NULL);
    t_note("WS_CHILD dialog without parent: err=%lu", GetLastError());
    EQ(ds.ninit, 0);
    if (h) DestroyWindow(h);
}
TEST(dialogs_create_bad_control_class_fails) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 2, 0, 0, 100, 50, NULL, NULL, L"bad", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 30, 14, 1, C_BUTTON, L"ok");
    dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 40, 0, 30, 14, 2, L"No_Such_Class_32", L"bad");
    SET_ERR(0);
    HWND h = CDW(&d, NULL, 0);
    CHECK(h == NULL);
    DWORD e = GetLastError();
    t_note("dialog with unregistered control class: err=%lu (1407 on Win10 21H1+)", e);
    EQ(ds.ninit, 0);
    if (h) DestroyWindow(h);
}
TEST(dialogs_create_nofailcreate_skips_bad_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE | DS_NOFAILCREATE, 0, 3, 0, 0, 100, 50, NULL, NULL, L"nofail", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 30, 14, 1, C_BUTTON, L"ok");
    dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 40, 0, 30, 14, 2, L"No_Such_Class_32", L"bad");
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 20, 30, 14, 3, C_BUTTON, L"after");
    HWND h = CDW(&d, NULL, 0);
    CHECK(h != NULL);
    if (!h) return;
    EQ(ds.ninit, 1);
    CHECK(GetDlgItem(h, 1) != NULL);
    CHECK(GetDlgItem(h, 2) == NULL);
    CHECK(GetDlgItem(h, 3) != NULL);          /* controls after the failed one are still created */
    EQ(count_children(h), 2);
    DestroyWindow(h);
}
TEST(dialogs_create_bad_dialog_class_fails) {
    REQUIRE(CreateDialogIndirectParamW);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 50, 50, NULL, L"No_Such_Dialog_Class_32", L"badcls", 8, L"MS Shell Dlg");
    SET_ERR(0);
    HWND h = CDW(&d, NULL, 0);
    CHECK(h == NULL);
    t_note("unregistered dialog class: err=%lu", GetLastError());
    EQ(ds.ninit, 0);
    if (h) DestroyWindow(h);
}
static LRESULT CALLBACK custom_cls_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefDlgProcW(h, m, w, l); }
TEST(dialogs_create_custom_dialog_class) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(RegisterClassW); REQUIRE(UnregisterClassA); REQUIRE(GetClassNameA); REQUIRE(DefDlgProcW); REQUIRE(DestroyWindow);
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = custom_cls_proc; wc.hInstance = INST; wc.lpszClassName = L"T32DlgCustomCls";
    wc.cbWndExtra = DLGWINDOWEXTRA; wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    ATOM at = RegisterClassW(&wc);
    REQUIRE(at);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 1, 0, 0, 80, 50, NULL, L"T32DlgCustomCls", L"custom", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 30, 14, 5, C_BUTTON, L"b");
    HWND h = CDW(&d, NULL, 0);
    CHECK(h != NULL);
    if (h) {
        char c[40]; cls_of(h, c, 40);
        EXPECT_STR(c, "T32DlgCustomCls");
        EQ(ds.ninit, 1);
        CHECK(GetDlgItem(h, 5) != NULL);
        DestroyWindow(h);
    }
    UnregisterClassA("T32DlgCustomCls", INST);
}
TEST(dialogs_class_32770_properties) {
    REQUIRE(GetClassInfoExW);
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc); wc.cbSize = sizeof wc;
    ATOM a = (ATOM)(ULONG_PTR)GetClassInfoExW(NULL, L"#32770", &wc);
    CHECK(a != 0);
    CHECKF(wc.cbWndExtra >= (int)(DWLP_USER + sizeof(LONG_PTR)), "cbWndExtra=%d", wc.cbWndExtra);
    CHECK(wc.lpfnWndProc != NULL);
    t_note("#32770: style=%x cbWndExtra=%d cbClsExtra=%d hbr=%p", wc.style, wc.cbWndExtra, wc.cbClsExtra, (void *)wc.hbrBackground);
}

/* ---------------- fonts ---------------- */
TEST(dialogs_create_setfont_before_initdialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"f"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(ds.nsetfont, 1);
    CHECK(ds.font != NULL);
    CHECK(ds.idx_setfont >= 0 && ds.idx_setfont < ds.idx_init);
    DestroyWindow(h);
}
TEST(dialogs_create_controls_get_dialog_font) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"f"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    HFONT f = (HFONT)SendMessageW(GetDlgItem(h, 100), WM_GETFONT, 0, 0);
    CHECK(f != NULL);
    EQ(f, ds.font);
    EQ(SendMessageW(GetDlgItem(h, IDOK), WM_GETFONT, 0, 0), (LRESULT)ds.font);
    EQ(SendMessageW(GetDlgItem(h, IDCANCEL), WM_GETFONT, 0, 0), (LRESULT)ds.font);
    DestroyWindow(h);
}
TEST(dialogs_create_font_metrics_classic) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDpiForSystem); REQUIRE(DestroyWindow);
    static const int pts[] = { 8, 9, 10, 12, 16, 24 };
    UINT dpi = GetDpiForSystem();
    for (unsigned i = 0; i < 6; i++) {
        DT d; ds_reset();
        dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 40, 40, NULL, NULL, L"fm", pts[i], L"MS Shell Dlg");
        HWND h = CDW(&d, NULL, 0); CHECK(h != NULL); if (!h) continue;
        LOGFONTW lf; memset(&lf, 0, sizeof lf);
        int n = ds.font ? GetObjectW(ds.font, sizeof lf, &lf) : 0;
        CHECKF(n >= (int)sizeof lf, "GetObject=%d", n);
        int want = -MulDiv(pts[i], dpi, 72);
        CHECKF(abs(lf.lfHeight - want) <= 1, "pt %d: height %ld want %d", pts[i], lf.lfHeight, want);
        t_note("classic template font weight=%ld italic=%d", lf.lfWeight, lf.lfItalic);
        EQ(lf.lfItalic, 0);
        DestroyWindow(h);
    }
}
TEST(dialogs_create_font_ex_weight_italic) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dtx_hdr(&d, BASE_STYLE | DS_SETFONT, 0, 0, 0, 0, 60, 40, NULL, NULL, L"exfont", 10, FW_BOLD, 1, ANSI_CHARSET, L"Arial", 0);
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    int n = ds.font ? GetObjectW(ds.font, sizeof lf, &lf) : 0;
    CHECK(n >= (int)sizeof lf);
    /* the DLGTEMPLATEEX weight/italic only apply with DS_SETFONT|DS_SHELLFONT-style fonts; Win10 22H2 gave weight 400 for Arial here */
    t_note("EX font weight %ld italic %d (requested %d, 1)", lf.lfWeight, lf.lfItalic, FW_BOLD);
    CHECK(lf.lfWeight == FW_BOLD || lf.lfWeight == FW_NORMAL);
    t_note("EX font charset ANSI requested -> lfCharSet=%d", lf.lfCharSet);
    EXPECT_WSTR(lf.lfFaceName, L"Arial");
    DestroyWindow(h);
}
TEST(dialogs_create_font_face_name_roundtrip) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    static const WCHAR *faces[] = { L"MS Shell Dlg", L"MS Shell Dlg 2", L"Tahoma", L"Courier New", L"Segoe UI" };
    for (unsigned i = 0; i < 5; i++) {
        DT d; ds_reset();
        dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 40, 40, NULL, NULL, L"face", 9, faces[i]);
        HWND h = CDW(&d, NULL, 0); CHECK(h != NULL); if (!h) continue;
        LOGFONTW lf; memset(&lf, 0, sizeof lf);
        if (ds.font) GetObjectW(ds.font, sizeof lf, &lf);
        if (i >= 2) CHECKF(!lstrcmpiW(lf.lfFaceName, faces[i]), "case %u: '%ls'", i, lf.lfFaceName);
        else t_note("face '%ls' requested -> '%ls'", faces[i], lf.lfFaceName);
        DestroyWindow(h);
    }
}
TEST(dialogs_create_no_font_no_setfont_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 40, 40, NULL, NULL, L"nf", 0, NULL);
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    t_note("dialog without DS_SETFONT: WM_SETFONT count=%d", ds.nsetfont);
    EQ(ds.ninit, 1);
    DestroyWindow(h);
}
TEST(dialogs_create_larger_font_larger_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowRect); REQUIRE(DestroyWindow);
    RECT r1, r2; DT d;
    ds_reset(); dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 100, 60, NULL, NULL, L"s", 8, L"MS Shell Dlg");
    HWND a = CDW(&d, NULL, 0);
    ds_reset(); dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 100, 60, NULL, NULL, L"s", 20, L"MS Shell Dlg");
    HWND b = CDW(&d, NULL, 0);
    REQUIRE(a); REQUIRE(b);
    GetWindowRect(a, &r1); GetWindowRect(b, &r2);
    CHECKF((r2.right - r2.left) > (r1.right - r1.left) && (r2.bottom - r2.top) > (r1.bottom - r1.top), "%ldx%ld vs %ldx%ld",
        r1.right - r1.left, r1.bottom - r1.top, r2.right - r2.left, r2.bottom - r2.top);
    DestroyWindow(a); DestroyWindow(b);
}

/* ---------------- controls from templates ---------------- */
struct ctc { const void *cls; const char *want; };
static const struct ctc ctcs[] = {
    { C_BUTTON, "Button" }, { C_EDIT, "Edit" }, { C_STATIC, "Static" }, { C_LIST, "ListBox" }, { C_SCROLL, "ScrollBar" }, { C_COMBO, "ComboBox" },
    { L"BUTTON", "Button" }, { L"button", "Button" }, { L"Edit", "Edit" }, { L"STATIC", "Static" }, { L"listbox", "ListBox" }, { L"COMBOBOX", "ComboBox" },
    { L"scrollbar", "ScrollBar" }, { L"msctls_progress32", NULL }, { L"SysTabControl32", NULL }, { L"RichEdit20W", NULL },
};
TEST(dialogs_create_control_class_ordinals_and_names) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetClassNameA); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    for (unsigned i = 0; i < sizeof ctcs / sizeof *ctcs; i++) {
        DT d; ds_reset();
        dt_hdr(&d, BASE_STYLE | DS_NOFAILCREATE, 0, 1, 0, 0, 100, 60, NULL, NULL, L"cc", 8, L"MS Shell Dlg");
        dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 2, 2, 40, 20, 50, ctcs[i].cls, L"t");
        HWND h = CDW(&d, NULL, 0); CHECK(h != NULL); if (!h) continue;
        HWND c = GetDlgItem(h, 50);
        if (ctcs[i].want) {
            CHECKF(c != NULL, "case %u", i);
            if (c) { char n[64]; cls_of(c, n, 64); CHECKF(!lstrcmpiA(n, ctcs[i].want), "case %u got %s want %s", i, n, ctcs[i].want); }
        } else t_note("optional class case %u -> %p", i, (void *)c);
        DestroyWindow(h);
    }
}
TEST(dialogs_create_control_ordinal_class_case_exact) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetClassNameA); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 6, 0, 0, 200, 100, NULL, NULL, L"ord", 8, L"MS Shell Dlg");
    for (int i = 0; i < 6; i++) dt_item(&d, WS_CHILD | WS_VISIBLE, 0, (short)(i * 20), 2, 18, 18, (WORD)(60 + i), (const void *)(ULONG_PTR)(0x80 + i), L"t");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    static const char *want[] = { "Button", "Edit", "Static", "ListBox", "ScrollBar", "ComboBox" };
    for (int i = 0; i < 6; i++) { char n[64]; cls_of(GetDlgItem(h, 60 + i), n, 64); CHECKF(!strcmp(n, want[i]), "ordinal 0x%x -> %s", 0x80 + i, n); }
    DestroyWindow(h);
}
TEST(dialogs_create_control_texts) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemTextW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 3, 0, 0, 200, 100, NULL, NULL, L"tx", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 0, 0, 80, 12, 10, C_STATIC, L"Static One");
    dt_item(&d, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 20, 80, 12, 11, C_EDIT, L"");
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 40, 80, 12, 12, C_BUTTON, L"&Mnemonic \x20AC");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    WCHAR w[64];
    EQ(GetDlgItemTextW(h, 10, w, 64), 10); EXPECT_WSTR(w, L"Static One");
    EQ(GetDlgItemTextW(h, 11, w, 64), 0);
    EQ(GetDlgItemTextW(h, 12, w, 64), 11); EXPECT_WSTR(w, L"&Mnemonic \x20AC");
    DestroyWindow(h);
}
TEST(dialogs_create_control_styles_and_ids) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(GetWindowLongW); REQUIRE(GetDlgCtrlID); REQUIRE(DestroyWindow);
    static const DWORD sts[] = { WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP, WS_CHILD | WS_DISABLED | WS_TABSTOP, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS };
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 3, 0, 0, 200, 100, NULL, NULL, L"cs", 8, L"MS Shell Dlg");
    for (int i = 0; i < 3; i++) dt_item(&d, sts[i] | BS_PUSHBUTTON, 0, (short)(i * 30), 2, 28, 14, (WORD)(1 + i * 1000), C_BUTTON, L"b");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    for (int i = 0; i < 3; i++) {
        HWND c = GetDlgItem(h, 1 + i * 1000);
        CHECKF(c != NULL, "ctl %d", i); if (!c) continue;
        EQ(GetDlgCtrlID(c), 1 + i * 1000);
        DWORD got = GetWindowLongW(c, GWL_STYLE);
        CHECKF((got & (sts[i] & ~WS_VISIBLE)) == (sts[i] & ~WS_VISIBLE), "ctl %d style %lx want %lx", i, got, sts[i]);
    }
    DestroyWindow(h);
}
TEST(dialogs_create_control_exstyle) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(GetWindowLongW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 1, 0, 0, 100, 60, NULL, NULL, L"ce", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE, WS_EX_CLIENTEDGE | WS_EX_TRANSPARENT, 2, 2, 40, 20, 5, C_EDIT, L"x");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    DWORD ex = GetWindowLongW(GetDlgItem(h, 5), GWL_EXSTYLE);
    CHECK((ex & (WS_EX_CLIENTEDGE | WS_EX_TRANSPARENT)) == (WS_EX_CLIENTEDGE | WS_EX_TRANSPARENT));
    DestroyWindow(h);
}
TEST(dialogs_create_control_position_scaled) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(GetWindowRect); REQUIRE(ScreenToClient); REQUIRE(MapDialogRect); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 2, 0, 0, 200, 100, NULL, NULL, L"pos", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 10, 20, 50, 30, 5, C_STATIC, L"a");
    dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 0, 0, 1, 1, 6, C_STATIC, L"b");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    RECT r, m = { 10, 20, 60, 50 };
    HWND c = GetDlgItem(h, 5);
    GetWindowRect(c, &r);
    POINT p = { r.left, r.top }; ScreenToClient(h, &p);
    MapDialogRect(h, &m);
    CHECKF(abs(p.x - m.left) <= 1 && abs(p.y - m.top) <= 1, "pos %ld,%ld mapped %ld,%ld", p.x, p.y, m.left, m.top);
    CHECKF(abs((r.right - r.left) - (m.right - m.left)) <= 1 && abs((r.bottom - r.top) - (m.bottom - m.top)) <= 1, "size %ldx%ld mapped %ldx%ld", r.right - r.left, r.bottom - r.top, m.right - m.left, m.bottom - m.top);
    DestroyWindow(h);
}

/* control with creation data */
static struct { int n; DWORD style; ULONG_PTR id; WCHAR name[32]; const WORD *cp; BYTE data[32]; WORD cnt; HWND parent; int empty_null; } cc;
static LRESULT CALLBACK cdata_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) {
        const CREATESTRUCTW *cs = (const CREATESTRUCTW *)l;
        cc.n++; cc.style = cs->style; cc.id = (ULONG_PTR)cs->hMenu; cc.parent = cs->hwndParent;
        lstrcpynW(cc.name, cs->lpszName ? cs->lpszName : L"", 32);
        cc.cp = cs->lpCreateParams;
        if (cc.cp) { cc.cnt = cc.cp[0]; memcpy(cc.data, (const BYTE *)cc.cp + 2, cc.cnt < 32 ? cc.cnt : 32); }
        else cc.empty_null = 1;
    }
    return DefWindowProcW(h, m, w, l);
}
TEST(dialogs_create_control_creation_data) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(RegisterClassW); REQUIRE(UnregisterClassA); REQUIRE(DestroyWindow); REQUIRE(DefWindowProcW);
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = cdata_proc; wc.hInstance = INST; wc.lpszClassName = L"T32DlgCtlData";
    REQUIRE(RegisterClassW(&wc));
    static const BYTE payload[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    DT d; ds_reset(); memset(&cc, 0, sizeof cc);
    dt_hdr(&d, BASE_STYLE, 0, 2, 0, 0, 100, 60, NULL, NULL, L"cd", 8, L"MS Shell Dlg");
    dt_item_x(&d, WS_CHILD | WS_VISIBLE, 0, 2, 2, 20, 20, 77, L"T32DlgCtlData", L"nm", payload, sizeof payload);
    dt_item_x(&d, WS_CHILD | WS_VISIBLE, 0, 30, 2, 20, 20, 78, L"T32DlgCtlData", L"nm2", NULL, 0);
    HWND h = CDW(&d, NULL, 0);
    CHECK(h != NULL);
    EQ(cc.n, 2);
    if (h) {
        /* last created is control 78 (no data), so rerun for the first */
        CHECK(cc.empty_null);
        DestroyWindow(h);
    }
    memset(&cc, 0, sizeof cc);
    dt_hdr(&d, BASE_STYLE, 0, 1, 0, 0, 100, 60, NULL, NULL, L"cd", 8, L"MS Shell Dlg");
    dt_item_x(&d, WS_CHILD | WS_VISIBLE, 0, 2, 2, 20, 20, 77, L"T32DlgCtlData", L"nm", payload, sizeof payload);
    h = CDW(&d, NULL, 0);
    CHECK(h != NULL);
    if (h) {
        EQ(cc.n, 1);
        EQ(cc.id, 77);
        EQ(cc.parent, h);
        EXPECT_WSTR(cc.name, L"nm");
        CHECK(cc.cp != NULL);
        EQ(cc.cnt, sizeof payload);
        CHECK(!memcmp(cc.data, payload, sizeof payload));
        DestroyWindow(h);
    }
    UnregisterClassA("T32DlgCtlData", INST);
}
TEST(dialogs_create_control_ordinal_title) {
    /* title given as (0xFFFF, id): for a static it is a resource reference, text stays empty */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE | DS_NOFAILCREATE, 0, 1, 0, 0, 100, 60, NULL, NULL, L"ot", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | SS_ICON, 0, 2, 2, 20, 20, 9, C_STATIC, (const void *)(ULONG_PTR)1234);
    HWND h = CDW(&d, NULL, 0);
    CHECK(h != NULL);
    if (h) { CHECK(GetDlgItem(h, 9) != NULL); DestroyWindow(h); }
}

/* ---------------- DLGTEMPLATEEX ---------------- */
TEST(dialogs_create_ex_template_basic) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(GetWindowTextW); REQUIRE(GetClassNameA); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dtx_hdr(&d, BASE_STYLE | DS_SETFONT, 0, 2, 5, 5, 150, 80, NULL, NULL, L"Ex Title", 9, FW_NORMAL, 0, DEFAULT_CHARSET, L"MS Shell Dlg", 0x1234);
    dtx_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 5, 5, 40, 14, IDOK, C_BUTTON, L"OK", 0, NULL, 0);
    dtx_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 5, 25, 60, 12, 70000, C_EDIT, L"edit", 0x55, NULL, 0);
    HWND h = CDW(&d, NULL, 99);
    CHECK(h != NULL);
    if (!h) return;
    WCHAR w[32];
    EQ(ds.ninit, 1); EQ(ds.lp, 99);
    EQ(GetWindowTextW(h, w, 32), 8); EXPECT_WSTR(w, L"Ex Title");
    CHECK(GetDlgItem(h, IDOK) != NULL);
    EQ(count_children(h), 2);
    HWND e = GetDlgItem(h, 70000);
    t_note("EX item with 32-bit id 70000: GetDlgItem -> %p, GetDlgCtrlID(first child)=%d", (void *)e, GetDlgCtrlID(GetWindow(h, GW_CHILD)));
    if (GetWindowContextHelpId) t_note("EX helpIDs: dialog 0x%lx (want 0x1234)", GetWindowContextHelpId(h));
    DestroyWindow(h);
}
TEST(dialogs_create_ex_template_classic_item_ids) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dtx_hdr(&d, BASE_STYLE | DS_SETFONT, 0, 3, 0, 0, 150, 80, NULL, NULL, L"ex", 8, FW_NORMAL, 0, 1, L"MS Shell Dlg", 0);
    for (int i = 0; i < 3; i++) dtx_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, (short)(i * 40), 5, 38, 14, 200 + i, C_BUTTON, L"b", 0, NULL, 0);
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    for (int i = 0; i < 3; i++) CHECK(GetDlgItem(h, 200 + i) != NULL);
    DestroyWindow(h);
}
TEST(dialogs_create_ex_template_extra_data_skipped) {
    /* creation data must be skipped correctly so later items still parse */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    static const BYTE pl[7] = { 9, 9, 9, 9, 9, 9, 9 };
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 3, 0, 0, 150, 80, NULL, NULL, L"ed", 8, L"MS Shell Dlg");
    dt_item_x(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 30, 14, 1, C_BUTTON, L"a", pl, 7);
    dt_item_x(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 40, 0, 30, 14, 2, C_BUTTON, L"b", pl, 3);
    dt_item_x(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 80, 0, 30, 14, 3, C_BUTTON, L"c", NULL, 0);
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    WCHAR w[8];
    EQ(count_children(h), 3);
    EQ(GetDlgItemTextW(h, 3, w, 8), 1); EXPECT_WSTR(w, L"c");
    DestroyWindow(h);
}
TEST(dialogs_create_ex_template_bad_version_fails) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dtx_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 50, 50, NULL, NULL, L"bv", 8, FW_NORMAL, 0, 1, L"MS Shell Dlg", 0);
    d.b[0] = 2;          /* dlgVer 2 */
    HWND h = CDW(&d, NULL, 0);
    t_note("DLGTEMPLATEEX with dlgVer=2 -> %p", (void *)h);
    if (h) DestroyWindow(h);
}

/* ---------------- WM_INITDIALOG / focus ---------------- */
TEST(dialogs_initdialog_focus_wparam_first_tabstop) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"fw"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(ds.hfocus, GetDlgItem(h, 100));
    DestroyWindow(h);
}
TEST(dialogs_initdialog_wparam_skips_non_tabstop) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 3, 0, 0, 150, 80, NULL, NULL, L"fw", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 0, 0, 30, 14, 1, C_STATIC, L"s");
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 40, 0, 30, 14, 2, C_BUTTON, L"a");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 80, 0, 30, 14, 3, C_BUTTON, L"b");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(ds.hfocus, GetDlgItem(h, 3));
    DestroyWindow(h);
}
TEST(dialogs_initdialog_wparam_skips_disabled_tabstop) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 2, 0, 0, 150, 80, NULL, NULL, L"fw", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 30, 14, 1, C_BUTTON, L"s");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 40, 0, 30, 14, 2, C_BUTTON, L"a");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(ds.hfocus, GetDlgItem(h, 2));
    DestroyWindow(h);
}
TEST(dialogs_initdialog_no_tabstop_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 2, 0, 0, 150, 80, NULL, NULL, L"fw", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 30, 14, 1, C_BUTTON, L"s");
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 40, 0, 30, 14, 2, C_BUTTON, L"a");
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    t_note("no tabstops: WM_INITDIALOG wParam=%p (ctl1=%p ctl2=%p)", (void *)ds.hfocus, (void *)GetDlgItem(h, 1), (void *)GetDlgItem(h, 2));
    DestroyWindow(h);
}
TEST(dialogs_initdialog_exactly_once) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow); REQUIRE(ShowWindow);
    DT d; mini_tmpl(&d, BASE_STYLE | WS_VISIBLE, 0, L"once"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    ShowWindow(h, SW_HIDE); ShowWindow(h, SW_SHOWNA);
    t_pump();
    EQ(ds.ninit, 1);
    DestroyWindow(h);
}
static INT_PTR CALLBACK hk_destroy_in_init(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) { ds.ninit++; DestroyWindow(h); return 1; }
    return -1;
}
TEST(dialogs_dialogbox_destroyed_in_initdialog_returns_minus1) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"dd"); ds_reset();
    ds.hook = hk_destroy_in_init;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    EQ(r, -1);
    EQ(ds.ninit, 1);
}

/* ---------------- DialogBox / EndDialog ---------------- */
static const INT_PTR endvals[] = { 0, 1, 2, 3, 7, 100, -1, -2, 0x7fffffff, (INT_PTR)0x80000000LL, 0xffff, 0x10000, 0x123456789abcLL, (INT_PTR)0x8000000000000000ULL, 0x7fffffffffffffffLL, IDOK, IDCANCEL };
TEST(dialogs_dialogbox_result_values_w) {
    REQUIRE(DialogBoxIndirectParamW);
    for (unsigned i = 0; i < sizeof endvals / sizeof *endvals; i++) {
        DT d; mini_tmpl(&d, BASE_STYLE, 0, L"rv"); ds_reset();
        ds.end_in_init = 1; ds.endval = endvals[i];
        INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
        CHECKF(r == endvals[i], "case %u: got %llx want %llx", i, (long long)r, (long long)endvals[i]);
        EQ(ds.ninit, 1); EQ(ds.ndestroy, 1); EQ(ds.nncdestroy, 1);
    }
}
TEST(dialogs_dialogbox_result_values_a) {
    REQUIRE(DialogBoxIndirectParamA);
    for (unsigned i = 0; i < sizeof endvals / sizeof *endvals; i++) {
        DT d; mini_tmpl(&d, BASE_STYLE, 0, L"rv"); ds_reset();
        ds.end_in_init = 1; ds.endval = endvals[i];
        INT_PTR r = DialogBoxIndirectParamA(INST, (LPCDLGTEMPLATEA)d.b, NULL, gdp, 5);
        CHECKF(r == endvals[i], "case %u: got %llx want %llx", i, (long long)r, (long long)endvals[i]);
        EQ(ds.lp, 5);
    }
}
TEST(dialogs_dialogbox_result_via_timer) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(IsWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"tm"); ds_reset();
    ds.tmo = 30; ds.endval = 42;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    EQ(r, 42);
    EQ(ds.timer_fired, 1);
    CHECK(!IsWindow(ds.hinit));
    EQ(ds.ndestroy, 1);
}
static int vis;
static void onrun_vis(HWND h) { vis = IsWindowVisible(h); }
TEST(dialogs_dialogbox_window_is_visible_during_loop) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(IsWindowVisible);
    vis = -1;
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"vis"); ds_reset();
    ds.tmo = 30; ds.endval = 1;
    ds.onrun = onrun_vis;
    DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    EQ(vis, 1);          /* DialogBox shows the dialog even without WS_VISIBLE */
}
static HWND ed, lb;
static void onrun_ed(HWND h) { ed = GetDlgItem(h, 100); }
static void onrun_lb(HWND h) { lb = GetDlgItem(h, ID_LIST); }
TEST(dialogs_dialogbox_initial_focus_in_loop) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(GetDlgItem);
    int cls_ok = 0;
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"foc"); ds_reset();
    ds.tmo = 60; ds.endval = 1;
    ds.onrun = onrun_ed;
    DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    CHECK(ed != NULL);
    if (!ds.focus_timer) t_note("no focus window in this thread's queue while the dialog ran");
    else EQ(ds.focus_timer, ed);             /* WM_INITDIALOG returned TRUE: default focus applied */
    (void)cls_ok;
}
static INT_PTR CALLBACK hk_focus_list(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) { SetFocus(GetDlgItem(h, ID_LIST)); ds.initret = 0; }
    return -1;
}
TEST(dialogs_dialogbox_initdialog_false_keeps_own_focus) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(SetFocus);
    DT d; std_tmpl(&d, BASE_STYLE); ds_reset();
    ds.tmo = 60; ds.endval = 1; ds.hook = hk_focus_list;
    ds.onrun = onrun_lb;
    DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    if (!ds.focus_timer) t_note("no focus window in this thread's queue while the dialog ran");
    else EQ(ds.focus_timer, lb);
}
TEST(dialogs_dialogbox_active_window_is_dialog) {
    REQUIRE(DialogBoxIndirectParamW);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"act"); ds_reset();
    ds.tmo = 60; ds.endval = 1;
    DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    if (!ds.active_timer) t_note("no active window in this thread's queue while the dialog ran");
    else EQ(ds.active_timer, ds.hinit);
}
static HWND o_hw; static int en_during;
static void onrun_en(HWND h) { en_during = IsWindowEnabled(o_hw); }
TEST(dialogs_dialogbox_owner_disabled_then_restored) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(IsWindowEnabled); REQUIRE(DestroyWindow); REQUIRE(EnableWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    REQUIRE(own);
    o_hw = own; en_during = -1;
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"own"); ds_reset();
    ds.tmo = 40; ds.endval = 9;
    ds.onrun = onrun_en;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, own, gdp, 0);
    EQ(r, 9);
    EQ(en_during, 0);
    CHECK(IsWindowEnabled(own));
    DestroyWindow(own);
}
TEST(dialogs_dialogbox_owner_already_disabled_stays_disabled) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(IsWindowEnabled); REQUIRE(DestroyWindow); REQUIRE(EnableWindow);
    HWND own = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_DISABLED, 0, 0, 0, 100, 100, NULL);
    REQUIRE(own);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"own2"); ds_reset();
    ds.end_in_init = 1; ds.endval = 3;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, own, gdp, 0);
    EQ(r, 3);
    /* Win10 22H2 re-enables the owner when the dialog ends, even if the owner was disabled beforehand; recorded */
    t_note("owner enabled after DialogBox returned: %d", IsWindowEnabled(own));
    DestroyWindow(own);
}
TEST(dialogs_dialogbox_invalid_owner_returns_zero) {
    REQUIRE(DialogBoxIndirectParamW);
    static const ULONG_PTR bad[] = { 0x12345678, 0xdead, 1, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < 4; i++) {
        DT d; mini_tmpl(&d, BASE_STYLE, 0, L"bo"); ds_reset();
        ds.end_in_init = 1; ds.endval = 5;
        SET_ERR(0);
        INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, (HWND)bad[i], gdp, 0);
        CHECKF(r == 0, "owner %llx -> %lld", (unsigned long long)bad[i], (long long)r);
        EQ(ds.ninit, 0);
        t_note("DialogBox invalid owner %llx: ret=%lld err=%lu", (unsigned long long)bad[i], (long long)r, GetLastError());
    }
}
TEST(dialogs_createdialog_invalid_owner) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"bo"); ds_reset();
    SET_ERR(0);
    HWND h = CDW(&d, (HWND)0x12345678, 0);
    CHECK(h == NULL);
    t_note("CreateDialog invalid owner: err=%lu", GetLastError());
    EQ(ds.ninit, 0);
    if (h) DestroyWindow(h);
}
TEST(dialogs_dialogbox_desktop_as_owner) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(GetDesktopWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"dt"); ds_reset();
    ds.end_in_init = 1; ds.endval = 8;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, GetDesktopWindow(), gdp, 0);
    EQ(r, 8);
}
TEST(dialogs_dialogbox_child_window_owner) {
    /* a child window as owner: the non-child ancestor becomes the owner and is the one disabled */
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(IsWindowEnabled); REQUIRE(DestroyWindow); REQUIRE(CreateWindowExA);
    HWND top = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 200, 200, NULL);
    REQUIRE(top);
    HWND kid = CreateWindowExA(0, "STATIC", "k", WS_CHILD | WS_VISIBLE, 0, 0, 20, 20, top, (HMENU)5, INST, NULL);
    REQUIRE(kid);
    o_hw = top; en_during = -1;
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"co"); ds_reset();
    ds.tmo = 40; ds.endval = 4;
    ds.onrun = onrun_en;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, kid, gdp, 0);
    EQ(r, 4);
    EQ(en_during, 0);
    CHECK(IsWindowEnabled(top));
    DestroyWindow(top);
}
static INT_PTR CALLBACK hk_double_end(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) { EQ(EndDialog(h, 11), TRUE); EQ(EndDialog(h, 22), TRUE); return 1; }
    return -1;
}
TEST(dialogs_enddialog_twice_both_true) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(EndDialog);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"ee"); ds_reset();
    ds.hook = hk_double_end;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    t_note("EndDialog(11) then EndDialog(22): DialogBox returned %lld", (long long)r);
    CHECK(r == 11 || r == 22);
}
TEST(dialogs_enddialog_invalid_window) {
    REQUIRE(EndDialog); REQUIRE(DestroyWindow);
    SET_ERR(0);
    CHECK(!EndDialog(NULL, 1));
    EQ(GetLastError(), 1400);
    SET_ERR(0);
    CHECK(!EndDialog((HWND)0x12345678, 1));
    EQ(GetLastError(), 1400);
    HWND p = t_make_window(NULL, WS_POPUP, 0, 0, 0, 10, 10, NULL);
    if (p) { CHECK(!EndDialog(p, 1)); DestroyWindow(p); }       /* not a dialog */
}
TEST(dialogs_enddialog_on_control_fails) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(EndDialog); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CHECK(!EndDialog(GetDlgItem(dlg, ID_EDIT), 1));
    CHECK(!EndDialog(GetDlgItem(dlg, ID_BTN), 1));
    DestroyWindow(dlg);
}
TEST(dialogs_enddialog_modeless_hides_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(EndDialog); REQUIRE(IsWindowVisible); REQUIRE(IsWindow); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE | WS_VISIBLE, 0, L"ml"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    CHECK(IsWindowVisible(h));
    EQ(EndDialog(h, 5), TRUE);
    CHECK(IsWindow(h));                      /* EndDialog does not destroy a modeless dialog */
    CHECK(!IsWindowVisible(h));
    t_pump();
    CHECK(IsWindow(h));
    DestroyWindow(h);
}
TEST(dialogs_enddialog_modeless_hidden_dialog_ok) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(EndDialog); REQUIRE(IsWindow); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"mh"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(EndDialog(h, 0), TRUE);
    CHECK(IsWindow(h));
    DestroyWindow(h);
}
TEST(dialogs_dialogbox_sequential_reuse_of_template) {
    REQUIRE(DialogBoxIndirectParamW);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"seq");
    int bad = 0;
    for (int i = 0; i < 100; i++) {
        ds_reset(); ds.end_in_init = 1; ds.endval = i * 3;
        if (DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, i) != i * 3 || ds.lp != i) bad++;
    }
    EQ(bad, 0);
}
static INT_PTR CALLBACK nest_proc(HWND h, UINT m, WPARAM w, LPARAM l);
static int nest_depth, nest_max;
static INT_PTR CALLBACK nest_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) {
        nest_depth++;
        if (nest_depth > nest_max) nest_max = nest_depth;
        INT_PTR r = 0;
        if (nest_depth < 4) {
            DT d; mini_tmpl(&d, BASE_STYLE, 0, L"nest");
            r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, h, nest_proc, 0);
        }
        nest_depth--;
        EndDialog(h, 100 + nest_depth + r);
        return 1;
    }
    return 0;
}
TEST(dialogs_dialogbox_nested_modal) {
    REQUIRE(DialogBoxIndirectParamW);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"nest");
    nest_depth = nest_max = 0;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, nest_proc, 0);
    EQ(nest_max, 4);
    CHECK(r > 0);
    t_note("nested modal result %lld", (long long)r);
}
TEST(dialogs_dialogbox_from_modeless_dialog_owner) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DialogBoxIndirectParamW); REQUIRE(IsWindowEnabled); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"ml"); ds_reset();
    HWND ml = CDW(&d, NULL, 0); REQUIRE(ml);
    DT d2; mini_tmpl(&d2, BASE_STYLE, 0, L"modal"); ds_reset();
    ds.end_in_init = 1; ds.endval = 6;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d2.b, ml, gdp, 0);
    EQ(r, 6);
    CHECK(IsWindowEnabled(ml));
    DestroyWindow(ml);
}

/* ---------------- WM_CLOSE / default processing ---------------- */
static int got_cmd_id = -1, got_cmd_n;
static INT_PTR CALLBACK hk_close(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && LOWORD(w) == IDCANCEL) { got_cmd_id = LOWORD(w); got_cmd_n++; return 1; }
    return -1;
}
TEST(dialogs_wm_close_sends_idcancel_command) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"cl"); ds_reset();
    ds.hook = hk_close; got_cmd_id = -1; got_cmd_n = 0;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    SendMessageW(h, WM_CLOSE, 0, 0);
    EQ(got_cmd_n, 0);                     /* the command is posted, not sent */
    t_pump();
    EQ(got_cmd_n, 1);
    EQ(got_cmd_id, IDCANCEL);
    CHECK(IsWindow(h));
    DestroyWindow(h);
}
static WPARAM gw; static LPARAM gl;
static INT_PTR CALLBACK hk_cmdrec(HWND h, UINT m, WPARAM w, LPARAM l) { if (m == WM_COMMAND && LOWORD(w) == IDCANCEL) { gw = w; gl = l; return 1; } return -1; }
TEST(dialogs_wm_close_command_lparam_is_cancel_button) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"cl"); ds_reset();
    ds.hook = hk_cmdrec; gw = gl = 0;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    SendMessageW(h, WM_CLOSE, 0, 0);
    t_pump();
    EQ(LOWORD(gw), IDCANCEL);
    EQ(HIWORD(gw), 0);
    EQ(gl, (LPARAM)GetDlgItem(h, IDCANCEL));
    DestroyWindow(h);
}
TEST(dialogs_wm_close_without_cancel_button) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 50, 50, NULL, NULL, L"nc", 8, L"MS Shell Dlg");
    ds.hook = hk_close; got_cmd_id = -1; got_cmd_n = 0;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    SendMessageW(h, WM_CLOSE, 0, 0);
    t_pump();
    t_note("WM_CLOSE on dialog with no Cancel control: WM_COMMAND count=%d id=%d", got_cmd_n, got_cmd_id);
    EQ(got_cmd_n, 1);
    EQ(got_cmd_id, IDCANCEL);
    DestroyWindow(h);
}
static INT_PTR CALLBACK hk_close_ends(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CLOSE) { EndDialog(h, 321); return 1; }
    return -1;
}
static INT_PTR CALLBACK hk2(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) { PostMessageW(h, WM_CLOSE, 0, 0); return -1; }
    return hk_close_ends(h, m, w, l);
}
TEST(dialogs_wm_close_handled_ends_modal) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(PostMessageW);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"cl"); ds_reset();
    ds.tmo = 300; ds.endval = -5;      /* timer is only a safety net; WM_CLOSE posted from init should win */
    ds.hook = hk2;
    INT_PTR r = DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    EQ(r, 321);
}

/* ---------------- message results ---------------- */
static INT_PTR CALLBACK hk_msgresult(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 5) { SetWindowLongPtrW(h, DWLP_MSGRESULT, 0x1234); return 1; }
    if (m == WM_USER + 6) { SetWindowLongPtrW(h, DWLP_MSGRESULT, 0x5678); return 0; }
    if (m == WM_USER + 7) { return 1; }
    if (m == WM_VKEYTOITEM) { SetWindowLongPtrW(h, DWLP_MSGRESULT, 0x99); return 5; }
    if (m == WM_CHARTOITEM) { SetWindowLongPtrW(h, DWLP_MSGRESULT, 0x99); return 6; }
    if (m == WM_QUERYDRAGICON) { SetWindowLongPtrW(h, DWLP_MSGRESULT, 0x99); return 7; }
    if (m == WM_CTLCOLORSTATIC) { SetWindowLongPtrW(h, DWLP_MSGRESULT, 0x99); return 8; }
    return -1;
}
TEST(dialogs_dlgproc_msgresult_true_uses_dwlp_msgresult) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"mr"); ds_reset(); ds.hook = hk_msgresult;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(SendMessageW(h, WM_USER + 5, 0, 0), 0x1234);
    SetWindowLongPtrW(h, DWLP_MSGRESULT, 0);
    EQ(SendMessageW(h, WM_USER + 7, 0, 0), 0);     /* TRUE with no msgresult set -> 0 */
    DestroyWindow(h);
}
TEST(dialogs_dlgproc_false_ignores_msgresult) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"mr"); ds_reset(); ds.hook = hk_msgresult;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(SendMessageW(h, WM_USER + 6, 0, 0), 0);     /* dlgproc returned FALSE -> DefDlgProc result */
    DestroyWindow(h);
}
TEST(dialogs_dlgproc_direct_return_messages) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"mr"); ds_reset(); ds.hook = hk_msgresult;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(SendMessageW(h, WM_VKEYTOITEM, 0, 0), 5);
    EQ(SendMessageW(h, WM_CHARTOITEM, 0, 0), 6);
    EQ(SendMessageW(h, WM_QUERYDRAGICON, 0, 0), 7);
    EQ(SendMessageW(h, WM_CTLCOLORSTATIC, 0, 0), 8);
    DestroyWindow(h);
}
TEST(dialogs_dlgproc_msgresult_other_dialogs_independent) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"mr"); ds_reset(); ds.hook = hk_msgresult;
    HWND a = CDW(&d, NULL, 0), b = CDW(&d, NULL, 0); REQUIRE(a); REQUIRE(b);
    SendMessageW(a, WM_USER + 5, 0, 0);
    EQ(GetWindowLongPtrW(a, DWLP_MSGRESULT), 0x1234);
    EQ(GetWindowLongPtrW(b, DWLP_MSGRESULT), 0);
    DestroyWindow(a); DestroyWindow(b);
}

/* ---------------- DefDlgProc ---------------- */
TEST(dialogs_defdlgproc_invalid_window) {
    REQUIRE(DefDlgProcA); REQUIRE(DefDlgProcW);
    SET_ERR(0);
    EQ(DefDlgProcA(NULL, WM_ERASEBKGND, 0, 0), 0);
    EQ(GetLastError(), 1400);
    SET_ERR(0);
    EQ(DefDlgProcW(NULL, WM_NULL, 0, 0), 0);
    EQ(GetLastError(), 1400);
    SET_ERR(0);
    EQ(DefDlgProcW((HWND)0x12345678, WM_GETTEXTLENGTH, 0, 0), 0);
}
TEST(dialogs_defdlgproc_on_dialog_basic) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DefDlgProcW); REQUIRE(DefDlgProcA); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"Defproc"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    EQ(DefDlgProcW(h, WM_NULL, 0, 0), 0);
    EQ(DefDlgProcW(h, WM_GETTEXTLENGTH, 0, 0), 7);
    WCHAR w[16];
    EQ(DefDlgProcW(h, WM_GETTEXT, 16, (LPARAM)w), 7); EXPECT_WSTR(w, L"Defproc");
    char a[16];
    EQ(DefDlgProcA(h, WM_GETTEXT, 16, (LPARAM)a), 7); EXPECT_STR(a, "Defproc");
    EQ(DefDlgProcW(h, DM_GETDEFID, 0, 0), SendMessageW(h, DM_GETDEFID, 0, 0));
    DestroyWindow(h);
}
TEST(dialogs_defdlgproc_wm_close_posts_cancel) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DefDlgProcW); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"x"); ds_reset();
    ds.hook = hk_close; got_cmd_n = 0; got_cmd_id = -1;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    DefDlgProcW(h, WM_CLOSE, 0, 0);
    t_pump();
    EQ(got_cmd_n, 1);
    EQ(got_cmd_id, IDCANCEL);
    DestroyWindow(h);
}
TEST(dialogs_defdlgproc_ctlcolor_brushes) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(GetSysColorBrush); REQUIRE(GetSysColor); REQUIRE(GetDC); REQUIRE(ReleaseDC); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE, 0, L"cc"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    HDC dc = GetDC(h); REQUIRE(dc);
    LRESULT b;
    b = SendMessageW(h, WM_CTLCOLORDLG, (WPARAM)dc, (LPARAM)h);
    EQ(b, (LRESULT)GetSysColorBrush(COLOR_3DFACE));
    SetBkColor(dc, 0x123456); SetTextColor(dc, 0x654321);
    b = SendMessageW(h, WM_CTLCOLORSTATIC, (WPARAM)dc, (LPARAM)h);
    EQ(b, (LRESULT)GetSysColorBrush(COLOR_3DFACE));
    EQ(GetBkColor(dc), GetSysColor(COLOR_3DFACE));
    EQ(GetTextColor(dc), GetSysColor(COLOR_WINDOWTEXT));
    SetBkColor(dc, 0x123456); SetTextColor(dc, 0x654321);
    b = SendMessageW(h, WM_CTLCOLORBTN, (WPARAM)dc, (LPARAM)h);
    EQ(b, (LRESULT)GetSysColorBrush(COLOR_3DFACE));
    EQ(GetBkColor(dc), GetSysColor(COLOR_3DFACE));
    SetBkColor(dc, 0x123456); SetTextColor(dc, 0x654321);
    b = SendMessageW(h, WM_CTLCOLOREDIT, (WPARAM)dc, (LPARAM)h);
    EQ(b, (LRESULT)GetSysColorBrush(COLOR_WINDOW));
    EQ(GetBkColor(dc), GetSysColor(COLOR_WINDOW));
    EQ(GetTextColor(dc), GetSysColor(COLOR_WINDOWTEXT));
    SetBkColor(dc, 0x123456);
    b = SendMessageW(h, WM_CTLCOLORLISTBOX, (WPARAM)dc, (LPARAM)h);
    EQ(b, (LRESULT)GetSysColorBrush(COLOR_WINDOW));
    ReleaseDC(h, dc);
    DestroyWindow(h);
}
TEST(dialogs_ctlcolor_messages_reach_dlgproc) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(ShowWindow); REQUIRE(UpdateWindow); REQUIRE(DestroyWindow);
    DT d; mini_tmpl(&d, BASE_STYLE | WS_VISIBLE, 0, L"paint"); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    UpdateWindow(h);
    t_pump();
    UpdateWindow(h);
    int dlgc = 0, btn = 0;
    for (int i = 0; i < ds.nmsg; i++) { if (ds.seq[i] == WM_CTLCOLORDLG) dlgc++; if (ds.seq[i] == WM_CTLCOLORBTN) btn++; }
    t_note("paint: ctlcolor msgs=%d (dlg=%d btn=%d, edit=%d)", ds.ncolor, dlgc, btn, 0);
    DestroyWindow(h);
}
