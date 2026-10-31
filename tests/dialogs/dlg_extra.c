/* dialogs: notifications from controls, DM_REPOSITION, dialog text, repeated-state checks */
#include "dlgutil.h"

#define INST GetModuleHandleA(NULL)

static struct { int n; WORD id[64], code[64]; HWND ctl[64]; } nt;
static INT_PTR CALLBACK nt_hook(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && nt.n < 64) {
        nt.id[nt.n] = LOWORD(w); nt.code[nt.n] = HIWORD(w); nt.ctl[nt.n] = (HWND)l; nt.n++;
    }
    return -1;
}
static HWND mk_notify_dialog(void) {
    DT d; std_tmpl(&d, BASE_STYLE);
    ds_reset();
    ds.hook = nt_hook;
    memset(&nt, 0, sizeof nt);
    HWND h = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    nt.n = 0;                         /* drop anything sent during creation */
    return h;
}
static int nt_count(WORD id, WORD code) {
    int c = 0;
    for (int i = 0; i < nt.n; i++) if (nt.id[i] == id && nt.code[i] == code) c++;
    return c;
}

TEST(dialogs_extra_bm_click_pushbutton_sends_bn_clicked) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_notify_dialog(); REQUIRE(dlg);
    SendDlgItemMessageW(dlg, ID_BTN, BM_CLICK, 0, 0);
    EQ(nt_count(ID_BTN, BN_CLICKED), 1);
    int found = 0;
    for (int i = 0; i < nt.n; i++) if (nt.id[i] == ID_BTN && nt.code[i] == BN_CLICKED) found = (nt.ctl[i] == GetDlgItem(dlg, ID_BTN));
    CHECK(found);                    /* lParam of WM_COMMAND is the control handle */
    DestroyWindow(dlg);
}
TEST(dialogs_extra_bm_click_checkbox_toggles_and_notifies) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageW); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_notify_dialog(); REQUIRE(dlg);
    EQ(IsDlgButtonChecked(dlg, ID_CHK), 0);
    SendDlgItemMessageW(dlg, ID_CHK, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_CHK), 1);
    SendDlgItemMessageW(dlg, ID_CHK, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_CHK), 0);
    EQ(nt_count(ID_CHK, BN_CLICKED), 2);
    DestroyWindow(dlg);
}
TEST(dialogs_extra_checkdlgbutton_does_not_notify) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(CheckRadioButton); REQUIRE(DestroyWindow);
    HWND dlg = mk_notify_dialog(); REQUIRE(dlg);
    CHECK(CheckDlgButton(dlg, ID_CHK, BST_CHECKED));
    CHECK(CheckRadioButton(dlg, ID_R1, ID_R3, ID_R2));
    EQ(nt_count(ID_CHK, BN_CLICKED), 0);
    EQ(nt_count(ID_R2, BN_CLICKED), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_extra_disabled_button_click_then_enabled) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(EnableWindow); REQUIRE(SendDlgItemMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_notify_dialog(); REQUIRE(dlg);
    EnableWindow(GetDlgItem(dlg, ID_BTN), FALSE);
    SendDlgItemMessageW(dlg, ID_BTN, BM_CLICK, 0, 0);
    t_note("BM_CLICK on disabled button produced %d BN_CLICKED", nt_count(ID_BTN, BN_CLICKED));
    int before = nt_count(ID_BTN, BN_CLICKED);
    EnableWindow(GetDlgItem(dlg, ID_BTN), TRUE);
    SendDlgItemMessageW(dlg, ID_BTN, BM_CLICK, 0, 0);
    EQ(nt_count(ID_BTN, BN_CLICKED), before + 1);
    DestroyWindow(dlg);
}
TEST(dialogs_extra_edit_settext_notification_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_notify_dialog(); REQUIRE(dlg);
    SetDlgItemTextA(dlg, ID_EDIT, "changed");
    t_pump();
    t_note("EN_CHANGE count=%d EN_UPDATE count=%d after SetDlgItemText", nt_count(ID_EDIT, EN_CHANGE), nt_count(ID_EDIT, EN_UPDATE));
    EQ(nt_count(ID_BTN, BN_CLICKED), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_extra_listbox_notify_style_selchange) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_notify_dialog(); REQUIRE(dlg);
    SendDlgItemMessageW(dlg, ID_LIST, LB_ADDSTRING, 0, (LPARAM)L"one");
    SendDlgItemMessageW(dlg, ID_LIST, LB_ADDSTRING, 0, (LPARAM)L"two");
    EQ(SendDlgItemMessageW(dlg, ID_LIST, LB_SETCURSEL, 1, 0), 1);
    EQ(nt_count(ID_LIST, LBN_SELCHANGE), 0);            /* programmatic selection does not notify */
    EQ(SendDlgItemMessageW(dlg, ID_LIST, LB_GETCURSEL, 0, 0), 1);
    DestroyWindow(dlg);
}

/* ---- DM_REPOSITION ---- */
static void get_mon_rect(HWND h, RECT *r) {
    MONITORINFO mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    HMONITOR mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
    if (mon && GetMonitorInfoW(mon, &mi)) *r = mi.rcMonitor;
    else { r->left = 0; r->top = 0; r->right = GetSystemMetrics(SM_CXSCREEN); r->bottom = GetSystemMetrics(SM_CYSCREEN); }
}
TEST(dialogs_extra_dm_reposition_pulls_dialog_on_screen) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetWindowPos); REQUIRE(GetWindowRect); REQUIRE(SendMessageW);
    REQUIRE(MonitorFromWindow); REQUIRE(GetMonitorInfoW); REQUIRE(DestroyWindow);
    DT d; dt_hdr(&d, BASE_STYLE | WS_VISIBLE, 0, 0, 0, 0, 120, 60, NULL, NULL, L"repos", 8, L"MS Shell Dlg");
    ds_reset();
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    REQUIRE(dlg);
    RECT mon, r; get_mon_rect(dlg, &mon);
    SetWindowPos(dlg, NULL, mon.right - 20, mon.bottom - 20, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    GetWindowRect(dlg, &r);
    int w = r.right - r.left, h = r.bottom - r.top;
    CHECK(r.right > mon.right);                          /* precondition: mostly off screen */
    if (w < mon.right - mon.left && h < mon.bottom - mon.top) {
        SendMessageW(dlg, DM_REPOSITION, 0, 0);
        GetWindowRect(dlg, &r);
        CHECKF(r.left >= mon.left && r.top >= mon.top && r.right <= mon.right && r.bottom <= mon.bottom,
               "rect %ld,%ld,%ld,%ld outside monitor %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom, mon.left, mon.top, mon.right, mon.bottom);
        EQ(r.right - r.left, w); EQ(r.bottom - r.top, h);   /* size unchanged */
    } else t_skip("dialog larger than monitor");
    DestroyWindow(dlg);
}
TEST(dialogs_extra_dm_reposition_keeps_onscreen_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetWindowPos); REQUIRE(GetWindowRect); REQUIRE(SendMessageW);
    REQUIRE(MonitorFromWindow); REQUIRE(GetMonitorInfoW); REQUIRE(DestroyWindow);
    DT d; dt_hdr(&d, BASE_STYLE | WS_VISIBLE, 0, 0, 0, 0, 120, 60, NULL, NULL, L"repos2", 8, L"MS Shell Dlg");
    ds_reset();
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    REQUIRE(dlg);
    RECT mon, a, b; get_mon_rect(dlg, &mon);
    SetWindowPos(dlg, NULL, mon.left + 40, mon.top + 40, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    GetWindowRect(dlg, &a);
    SendMessageW(dlg, DM_REPOSITION, 0, 0);
    GetWindowRect(dlg, &b);
    CHECK(b.right - b.left == a.right - a.left && b.bottom - b.top == a.bottom - a.top);
    CHECK(b.left >= mon.left && b.top >= mon.top && b.right <= mon.right && b.bottom <= mon.bottom);
    DestroyWindow(dlg);
}

/* ---- window text of the dialog ---- */
TEST(dialogs_extra_dialog_title_text_roundtrip) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowTextW); REQUIRE(SetWindowTextA); REQUIRE(GetWindowTextA); REQUIRE(DestroyWindow);
    DT d; std_tmpl(&d, BASE_STYLE); ds_reset();
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0); REQUIRE(dlg);
    WCHAR w[32]; char a[32];
    EQ(GetWindowTextW(dlg, w, 32), 3); CHECK(!lstrcmpW(w, L"Std"));
    CHECK(SetWindowTextA(dlg, "renamed"));
    EQ(GetWindowTextA(dlg, a, 32), 7); CHECK(!strcmp(a, "renamed"));
    EQ(GetWindowTextA(dlg, a, 4), 3); CHECK(!strcmp(a, "ren"));
    DestroyWindow(dlg);
}
TEST(dialogs_extra_dwlp_user_and_msgresult_start_zero) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowLongPtrW); REQUIRE(DestroyWindow);
    DT d; std_tmpl(&d, BASE_STYLE); ds_reset();
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0); REQUIRE(dlg);
    EQ(GetWindowLongPtrW(dlg, DWLP_USER), 0);
    EQ(GetWindowLongPtrW(dlg, DWLP_MSGRESULT), 0);
    CHECK(GetWindowLongPtrW(dlg, DWLP_DLGPROC) != 0);
    EQ(DWLP_MSGRESULT, 0); EQ(DWLP_DLGPROC, sizeof(LRESULT)); EQ(DWLP_USER, sizeof(LRESULT) + sizeof(DLGPROC));
    DestroyWindow(dlg);
}
TEST(dialogs_extra_getdlgitemtext_exact_lengths_after_resize) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemTextW); REQUIRE(GetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    WCHAR w[300], big[260];
    for (int i = 0; i < 255; i++) big[i] = L'a' + (i % 26);
    big[255] = 0;
    CHECK(SetDlgItemTextW(dlg, ID_STATIC, big));
    EQ(GetDlgItemTextW(dlg, ID_STATIC, w, 300), 255);
    CHECK(!lstrcmpW(w, big));
    EQ(GetDlgItemTextW(dlg, ID_STATIC, w, 256), 255);
    EQ(GetDlgItemTextW(dlg, ID_STATIC, w, 255), 254);
    EQ(GetDlgItemTextW(dlg, ID_STATIC, w, 1), 0);
    CHECK(w[0] == 0);
    DestroyWindow(dlg);
}
TEST(dialogs_extra_getdlgitemint_translated_cleared_on_empty) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemInt); REQUIRE(SetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    BOOL tr = 1;
    SetDlgItemTextA(dlg, ID_EDIT, "");
    EQ(GetDlgItemInt(dlg, ID_EDIT, &tr, FALSE), 0); EQ(tr, 0);
    tr = 1;
    EQ(GetDlgItemInt(dlg, ID_EDIT, &tr, TRUE), 0); EQ(tr, 0);
    SetDlgItemTextA(dlg, ID_EDIT, "12");
    tr = 0;
    EQ(GetDlgItemInt(dlg, ID_EDIT, &tr, TRUE), 12); EQ(tr, 1);
    DestroyWindow(dlg);
}
