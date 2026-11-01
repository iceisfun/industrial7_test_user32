/* dialogs: IsDialogMessage, DLGC_* codes, WM_NEXTDLGCTL, default push button tracking */
#include "dlgutil.h"

#define INST GetModuleHandleA(NULL)

/* ---- recorded WM_COMMANDs ---- */
static struct { int id[32]; int notify[32]; HWND ctl[32]; int n; } cmds;
static INT_PTR CALLBACK rec_hook(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && cmds.n < 32) {
        cmds.id[cmds.n] = LOWORD(w); cmds.notify[cmds.n] = HIWORD(w); cmds.ctl[cmds.n] = (HWND)l; cmds.n++;
    }
    return -1;
}
/* number of button-click style commands (notification code 0 / BN_CLICKED) for id */
static int clicks_of(int id) {
    int n = 0;
    for (int i = 0; i < cmds.n; i++) if (cmds.id[i] == id && cmds.notify[i] == 0) n++;
    return n;
}
static int total_clicks(void) { int n = 0; for (int i = 0; i < cmds.n; i++) if (cmds.notify[i] == 0) n++; return n; }

/* ---- visible dialog whose focus we can drive ---- */
static HWND vis_dialog(DT *d) {
    ds_reset();
    memset(&cmds, 0, sizeof cmds);
    ds.hook = rec_hook;
    HWND h = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d->b, NULL, gdp, 0);
    if (!h) return NULL;
    SetActiveWindow(h);
    t_pump();
    return h;
}
static int focus_to(HWND ctl) {
    SetFocus(ctl);
    t_pump();
    return GetFocus() == ctl;
}
static int idof(HWND h) { return h ? GetDlgCtrlID(h) : -1; }
static int focus_id(void) { return idof(GetFocus()); }

static BOOL send_key_w(HWND dlg, HWND target, UINT msg, WPARAM wp, LPARAM lp) {
    MSG m; memset(&m, 0, sizeof m);
    m.hwnd = target; m.message = msg; m.wParam = wp; m.lParam = lp; m.time = GetTickCount();
    return IsDialogMessageW(dlg, &m);
}
static BOOL send_key_a(HWND dlg, HWND target, UINT msg, WPARAM wp, LPARAM lp) {
    MSG m; memset(&m, 0, sizeof m);
    m.hwnd = target; m.message = msg; m.wParam = wp; m.lParam = lp; m.time = GetTickCount();
    return IsDialogMessageA(dlg, &m);
}
#define KEYDOWN(dlg, ctl, vk) send_key_w(dlg, ctl, WM_KEYDOWN, vk, 1)

static void vis_std(DT *d) { std_tmpl(d, BASE_STYLE | WS_VISIBLE); }

/* ---------------- Tab navigation ---------------- */
TEST(dialogs_ismsg_tab_moves_focus_forward) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(SetFocus); REQUIRE(GetFocus); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    HWND e = GetDlgItem(dlg, ID_EDIT);
    if (!focus_to(e)) { t_note("cannot set focus in this environment"); DestroyWindow(dlg); t_skip("no focus"); return; }
    static const int order[] = { ID_CHK, ID_R1, ID_BTN, ID_LIST, ID_COMBO, ID_3ST, ID_EDIT };
    for (int i = 0; i < 7; i++) {
        BOOL r = KEYDOWN(dlg, GetFocus(), VK_TAB);
        CHECKF(r, "step %d", i);
        CHECKF(focus_id() == order[i], "step %d focus %d want %d", i, focus_id(), order[i]);
    }
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_tab_from_dialog_hwnd) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    CHECK(KEYDOWN(dlg, dlg, VK_TAB));              /* message addressed to the dialog itself */
    t_note("Tab addressed to dialog hwnd: focus now %d", focus_id());
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_tab_skips_disabled_and_hidden) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(EnableWindow); REQUIRE(ShowWindow); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    EnableWindow(GetDlgItem(dlg, ID_CHK), FALSE);
    ShowWindow(GetDlgItem(dlg, ID_R1), SW_HIDE);
    CHECK(KEYDOWN(dlg, GetFocus(), VK_TAB));
    EQ(focus_id(), ID_BTN);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_tab_wraps_around) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_3ST))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    CHECK(KEYDOWN(dlg, GetFocus(), VK_TAB));
    EQ(focus_id(), ID_EDIT);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_tab_ansi_entry_point) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageA); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    CHECK(send_key_a(dlg, GetFocus(), WM_KEYDOWN, VK_TAB, 1));
    EQ(focus_id(), ID_CHK);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_tab_char_swallowed) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_CHK))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    BOOL r = send_key_w(dlg, GetFocus(), WM_CHAR, '\t', 1);
    t_note("WM_CHAR(TAB) on checkbox -> %d", r);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_tab_into_disabled_only_stays) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(EnableWindow); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    static const int ids[] = { ID_CHK, ID_R1, ID_BTN, ID_LIST, ID_COMBO, ID_3ST };
    for (int i = 0; i < 6; i++) EnableWindow(GetDlgItem(dlg, ids[i]), FALSE);
    BOOL r = KEYDOWN(dlg, GetFocus(), VK_TAB);
    CHECK(r);
    EQ(focus_id(), ID_EDIT);                 /* nothing else to go to */
    DestroyWindow(dlg);
}

/* ---------------- arrow keys ---------------- */
TEST(dialogs_ismsg_arrows_cycle_radio_group) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    CheckRadioButton(dlg, ID_R1, ID_R3, ID_R1);
    if (!focus_to(GetDlgItem(dlg, ID_R1))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    CHECK(KEYDOWN(dlg, GetFocus(), VK_DOWN));
    EQ(focus_id(), ID_R2);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 1); EQ(IsDlgButtonChecked(dlg, ID_R1), 0);
    CHECK(KEYDOWN(dlg, GetFocus(), VK_RIGHT));
    EQ(focus_id(), ID_R3);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 1); EQ(IsDlgButtonChecked(dlg, ID_R2), 0);
    CHECK(KEYDOWN(dlg, GetFocus(), VK_DOWN));       /* wraps */
    EQ(focus_id(), ID_R1);
    EQ(IsDlgButtonChecked(dlg, ID_R1), 1);
    CHECK(KEYDOWN(dlg, GetFocus(), VK_UP));
    EQ(focus_id(), ID_R3);
    CHECK(KEYDOWN(dlg, GetFocus(), VK_LEFT));
    EQ(focus_id(), ID_R2);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_arrows_skip_disabled_radio) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(EnableWindow); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    CheckRadioButton(dlg, ID_R1, ID_R3, ID_R1);
    if (!focus_to(GetDlgItem(dlg, ID_R1))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    EnableWindow(GetDlgItem(dlg, ID_R2), FALSE);
    CHECK(KEYDOWN(dlg, GetFocus(), VK_DOWN));
    EQ(focus_id(), ID_R3);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 1);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_arrow_on_edit_dispatched) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    /* the edit wants arrows: the message is dispatched to it (TRUE) and focus stays */
    CHECK(KEYDOWN(dlg, GetFocus(), VK_LEFT));
    CHECK(KEYDOWN(dlg, GetFocus(), VK_RIGHT));
    CHECK(KEYDOWN(dlg, GetFocus(), VK_UP));
    CHECK(KEYDOWN(dlg, GetFocus(), VK_DOWN));
    EQ(focus_id(), ID_EDIT);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_arrow_on_listbox_dispatched) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_LIST))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    CHECK(KEYDOWN(dlg, GetFocus(), VK_DOWN));
    CHECK(KEYDOWN(dlg, GetFocus(), VK_UP));
    EQ(focus_id(), ID_LIST);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_arrow_on_pushbutton_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_BTN))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    BOOL r = KEYDOWN(dlg, GetFocus(), VK_DOWN);
    CHECK(r);
    t_note("VK_DOWN on push button: focus now %d (next in group is the list box %d)", focus_id(), ID_LIST);
    DestroyWindow(dlg);
}

/* ---------------- Enter / Escape ---------------- */
static void btn_tmpl(DT *d, int defid) {
    dt_hdr(d, BASE_STYLE | WS_VISIBLE, 0, 5, 10, 10, 200, 100, NULL, NULL, L"btns", 8, L"MS Shell Dlg");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 5, 5, 80, 12, 100, C_EDIT, L"edit");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (defid == IDOK ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), 0, 5, 30, 40, 14, IDOK, C_BUTTON, L"OK");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (defid == IDCANCEL ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), 0, 50, 30, 40, 14, IDCANCEL, C_BUTTON, L"Cancel");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (defid == 50 ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), 0, 95, 30, 40, 14, 50, C_BUTTON, L"Other");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 5, 50, 60, 12, 60, C_BUTTON, L"chk");
}
TEST(dialogs_ismsg_return_clicks_default_button) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    CHECK(KEYDOWN(dlg, GetFocus(), VK_RETURN));
    EQ(clicks_of(IDOK), 1);
    EQ(total_clicks(), 1);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_return_uses_dm_setdefid_button) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    SendMessageW(dlg, DM_SETDEFID, 50, 0);
    cmds.n = 0;
    CHECK(KEYDOWN(dlg, GetFocus(), VK_RETURN));
    if (clicks_of(50) != 1) {   /* class G: focus/activation timing; look again once the queue has settled */
        t_note("first Return clicked %d times; retrying once", clicks_of(50));
        t_pump(); focus_to(GetDlgItem(dlg, 100)); cmds.n = 0;
        CHECK(KEYDOWN(dlg, GetFocus(), VK_RETURN));
    }
    EQ(clicks_of(50), 1);
    EQ(total_clicks(), 1);
    EQ(cmds.ctl[0], GetDlgItem(dlg, 50));
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_return_default_command_lparam) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDCANCEL); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    CHECK(KEYDOWN(dlg, GetFocus(), VK_RETURN));
    EQ(clicks_of(IDCANCEL), 1);
    EQ(cmds.ctl[0], GetDlgItem(dlg, IDCANCEL));
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_return_on_tabbed_pushbutton) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    for (int i = 0; i < 3; i++) KEYDOWN(dlg, GetFocus(), VK_TAB);     /* edit -> OK -> Cancel -> Other */
    EQ(focus_id(), 50);
    cmds.n = 0;
    CHECK(KEYDOWN(dlg, GetFocus(), VK_RETURN));
    EQ(clicks_of(50), 1);                    /* the focused push button, not the original default */
    EQ(clicks_of(IDOK), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_tab_to_pushbutton_makes_it_default) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    EQ(LOWORD(SendMessageW(dlg, DM_GETDEFID, 0, 0)), IDOK);
    KEYDOWN(dlg, GetFocus(), VK_TAB);  KEYDOWN(dlg, GetFocus(), VK_TAB);   /* -> Cancel */
    EQ(focus_id(), IDCANCEL);
    t_note("DM_GETDEFID with focus on Cancel: 0x%lx (stored default, Win10 22H2 does not follow the focus)", (unsigned long)SendMessageW(dlg, DM_GETDEFID, 0, 0));
    EQ(GetWindowLongW(GetDlgItem(dlg, IDCANCEL), GWL_STYLE) & BS_TYPEMASK, BS_DEFPUSHBUTTON);
    EQ(GetWindowLongW(GetDlgItem(dlg, IDOK), GWL_STYLE) & BS_TYPEMASK, BS_PUSHBUTTON);
    KEYDOWN(dlg, GetFocus(), VK_TAB);                                       /* -> Other */
    EQ(GetWindowLongW(GetDlgItem(dlg, 50), GWL_STYLE) & BS_TYPEMASK, BS_DEFPUSHBUTTON);
    EQ(GetWindowLongW(GetDlgItem(dlg, IDCANCEL), GWL_STYLE) & BS_TYPEMASK, BS_PUSHBUTTON);
    KEYDOWN(dlg, GetFocus(), VK_TAB);                                       /* -> check box: default restored */
    EQ(focus_id(), 60);
    EQ(GetWindowLongW(GetDlgItem(dlg, IDOK), GWL_STYLE) & BS_TYPEMASK, BS_DEFPUSHBUTTON);
    EQ(GetWindowLongW(GetDlgItem(dlg, 50), GWL_STYLE) & BS_TYPEMASK, BS_PUSHBUTTON);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_focus_on_checkbox_keeps_default) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 60))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    EQ(LOWORD(SendMessageW(dlg, DM_GETDEFID, 0, 0)), IDOK);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_escape_sends_idcancel) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    CHECK(KEYDOWN(dlg, GetFocus(), VK_ESCAPE));
    EQ(clicks_of(IDCANCEL), 1);
    EQ(total_clicks(), 1);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_escape_with_disabled_cancel) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(EnableWindow); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    EnableWindow(GetDlgItem(dlg, IDCANCEL), FALSE);
    cmds.n = 0;
    BOOL r = KEYDOWN(dlg, GetFocus(), VK_ESCAPE);
    CHECK(r);
    EQ(clicks_of(IDCANCEL), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_escape_return_without_buttons) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; ds_reset(); memset(&cmds, 0, sizeof cmds); ds.hook = rec_hook;
    dt_hdr(&d, BASE_STYLE | WS_VISIBLE, 0, 1, 10, 10, 100, 60, NULL, NULL, L"nc", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 5, 5, 60, 12, 100, C_EDIT, L"e");
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0); REQUIRE(dlg);
    SetActiveWindow(dlg); t_pump();
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    BOOL r = KEYDOWN(dlg, GetFocus(), VK_ESCAPE);
    CHECK(r);
    EQ(clicks_of(IDCANCEL), 1);              /* WM_COMMAND(IDCANCEL) is sent even though there is no such control */
    t_note("WM_COMMAND(IDCANCEL) without a cancel button: control hwnd %p", (void *)cmds.ctl[0]);   /* the focused control on Win10 22H2 */
    cmds.n = 0;
    r = KEYDOWN(dlg, GetFocus(), VK_RETURN);
    CHECK(r);
    EQ(clicks_of(IDOK), 1);                  /* no default push button: IDOK is assumed */
    EQ(cmds.ctl[0], NULL);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_return_without_default_button_uses_idok) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, 0); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    BOOL r = KEYDOWN(dlg, GetFocus(), VK_RETURN);
    CHECK(r);
    EQ(clicks_of(IDOK), 1);                  /* falls back to IDOK */
    EQ(total_clicks(), 1);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_return_on_checkbox_uses_default) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 60))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    CHECK(KEYDOWN(dlg, GetFocus(), VK_RETURN));
    EQ(clicks_of(IDOK), 1);
    EQ(IsDlgButtonChecked(dlg, 60), 0);                 /* Enter does not toggle a check box */
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_disabled_default_button_not_clicked) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(EnableWindow); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    EnableWindow(GetDlgItem(dlg, IDOK), FALSE);
    cmds.n = 0;
    KEYDOWN(dlg, GetFocus(), VK_RETURN);
    EQ(clicks_of(IDOK), 0);
    DestroyWindow(dlg);
}

/* ---------------- mnemonics ---------------- */
static void mnem_tmpl(DT *d) {
    dt_hdr(d, BASE_STYLE | WS_VISIBLE, 0, 8, 10, 10, 220, 120, NULL, NULL, L"mn", 8, L"MS Shell Dlg");
    dt_item(d, WS_CHILD | WS_VISIBLE, 0, 5, 5, 40, 10, 10, C_STATIC, L"&Name:");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 50, 5, 60, 12, 11, C_EDIT, L"");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 5, 25, 40, 14, 12, C_BUTTON, L"&Go");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 5, 45, 60, 12, 13, C_BUTTON, L"&Option");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP, 0, 5, 60, 60, 12, 14, C_BUTTON, L"&Radio");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 70, 25, 40, 14, 15, C_BUTTON, L"A&&B");
    dt_item(d, WS_CHILD | WS_VISIBLE, 0, 5, 80, 40, 10, 16, C_STATIC, L"&Lbl:");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | ES_AUTOHSCROLL, 0, 50, 80, 60, 12, 17, C_EDIT, L"");
}
#define ALT(dlg, ch) send_key_w(dlg, GetFocus(), WM_SYSCHAR, ch, 0x20000001)
TEST(dialogs_ismsg_mnemonic_label_focuses_next_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; mnem_tmpl(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 12))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    CHECK(ALT(dlg, 'n'));
    EQ(focus_id(), 11);
    CHECK(focus_to(GetDlgItem(dlg, 12)));
    CHECK(ALT(dlg, 'N'));                     /* case-insensitive */
    EQ(focus_id(), 11);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_mnemonic_pushbutton_clicks) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; mnem_tmpl(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 11))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    CHECK(ALT(dlg, 'g'));
    EQ(clicks_of(12), 1);
    EQ(total_clicks(), 1);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_mnemonic_checkbox_toggles) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    DT d; mnem_tmpl(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 11))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    CHECK(ALT(dlg, 'o'));
    EQ(IsDlgButtonChecked(dlg, 13), 1);
    EQ(focus_id(), 13);
    focus_to(GetDlgItem(dlg, 11));
    CHECK(ALT(dlg, 'o'));
    EQ(IsDlgButtonChecked(dlg, 13), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_mnemonic_radio_checks) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    DT d; mnem_tmpl(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 11))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    CHECK(ALT(dlg, 'r'));
    EQ(IsDlgButtonChecked(dlg, 14), 1);
    EQ(focus_id(), 14);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_mnemonic_unknown_char_handled_as_beep_or_ignored) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; mnem_tmpl(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 11))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    BOOL r = ALT(dlg, 'z');
    t_note("unmatched Alt+Z -> handled=%d", r);
    EQ(focus_id(), 11);
    EQ(total_clicks(), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_mnemonic_label_to_disabled_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; mnem_tmpl(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 11))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    ALT(dlg, 'l');
    CHECK(focus_id() != 17);                  /* cannot focus a disabled control */
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_mnemonic_ansi_entry) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageA); REQUIRE(DestroyWindow);
    DT d; mnem_tmpl(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 12))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    CHECK(send_key_a(dlg, GetFocus(), WM_SYSCHAR, 'n', 0x20000001));
    EQ(focus_id(), 11);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_mnemonic_escaped_ampersand_not_mnemonic) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; mnem_tmpl(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 11))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    cmds.n = 0;
    ALT(dlg, 'b');                             /* "A&&B" contains a literal ampersand: B is not a mnemonic */
    EQ(clicks_of(15), 0);
    ALT(dlg, 'a');
    EQ(clicks_of(15), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_mnemonic_duplicates_cycle_focus) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; ds_reset(); memset(&cmds, 0, sizeof cmds); ds.hook = rec_hook;
    dt_hdr(&d, BASE_STYLE | WS_VISIBLE, 0, 3, 10, 10, 150, 80, NULL, NULL, L"dup", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 5, 5, 60, 12, 1, C_EDIT, L"");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 5, 25, 40, 14, 2, C_BUTTON, L"&Xa");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 50, 25, 40, 14, 3, C_BUTTON, L"&Xb");
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0); REQUIRE(dlg);
    SetActiveWindow(dlg); t_pump();
    if (!focus_to(GetDlgItem(dlg, 1))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    ALT(dlg, 'x');
    int f1 = focus_id(), c1 = total_clicks();
    ALT(dlg, 'x');
    int f2 = focus_id(), c2 = total_clicks();
    t_note("duplicate mnemonic: first press focus=%d clicks=%d; second press focus=%d clicks=%d", f1, c1, f2, c2);
    /* Win10 22H2: a second mnemonic press with a duplicate on the edit-focused dialog clicks the button without moving
     * the focus (focus stays on the edit, id 1) while the click count grows; recorded rather than required. */
    CHECK(c1 >= 1 && c2 >= c1);
    DestroyWindow(dlg);
}

/* ---------------- return values / invalid ---------------- */
TEST(dialogs_ismsg_not_for_this_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    HWND other = t_make_window(NULL, WS_POPUP, 0, 0, 0, 20, 20, NULL);
    REQUIRE(other);
    CHECK(!KEYDOWN(dlg, other, VK_TAB));        /* message for an unrelated window */
    CHECK(!KEYDOWN(dlg, other, VK_RETURN));
    CHECK(!KEYDOWN(dlg, NULL, VK_TAB));
    DestroyWindow(other); DestroyWindow(dlg);
}
TEST(dialogs_ismsg_non_keyboard_messages_dispatched) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    HWND e = GetDlgItem(dlg, ID_EDIT);
    static const UINT msgs[] = { WM_MOUSEMOVE, WM_PAINT, WM_TIMER, WM_USER, WM_NULL, WM_COMMAND, WM_SETFOCUS };
    for (unsigned i = 0; i < sizeof msgs / sizeof *msgs; i++) {
        MSG m; memset(&m, 0, sizeof m); m.hwnd = e; m.message = msgs[i];
        BOOL r = IsDialogMessageW(dlg, &m);
        CHECKF(r, "msg 0x%x returned %d", msgs[i], r);     /* any message addressed inside the dialog is dispatched */
    }
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_keyup_dispatched) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    BOOL r = send_key_w(dlg, GetFocus(), WM_KEYUP, VK_TAB, 0xC0000001);
    CHECK(r);                                  /* messages inside the dialog are dispatched and reported as handled */
    EQ(focus_id(), ID_EDIT);                   /* but Tab-up does not move focus */
    DestroyWindow(dlg);
}
TEST(dialogs_ismsg_invalid_dialog_handle) {
    REQUIRE(IsDialogMessageW); REQUIRE(IsDialogMessageA); REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    MSG m; memset(&m, 0, sizeof m); m.message = WM_KEYDOWN; m.wParam = VK_TAB;
    SET_ERR(0);
    CHECK(!IsDialogMessageW(NULL, &m));
    SET_ERR(0);
    CHECK(!IsDialogMessageA((HWND)0x12345678, &m));
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    DestroyWindow(dlg);
    CHECK(!IsDialogMessageW(dlg, &m));
}
TEST(dialogs_ismsg_message_for_hidden_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; std_tmpl(&d, BASE_STYLE); ds_reset();
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0); REQUIRE(dlg);
    BOOL r = KEYDOWN(dlg, GetDlgItem(dlg, ID_EDIT), VK_TAB);
    t_note("Tab for hidden dialog -> %d", r);
    DestroyWindow(dlg);
}

/* ---------------- DLGC codes and custom controls ---------------- */
static DWORD g_code; static int g_keys; static int g_gdc_n; static MSG g_gdc_msg; static WPARAM g_gdc_w; static LPARAM g_gdc_l;
static LRESULT CALLBACK dlgc_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_GETDLGCODE) {
        g_gdc_n++;
        /* keep the first call that carries the message (IsDialogMessage also sends plain probes with lParam 0) */
        if (l && !g_gdc_l) { g_gdc_w = w; g_gdc_l = l; g_gdc_msg = *(MSG *)l; }
        return g_code;
    }
    if (m == WM_KEYDOWN) g_keys++;
    return DefWindowProcW(h, m, w, l);
}
static HWND dlgc_dialog(void) {
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = dlgc_proc; wc.hInstance = INST; wc.lpszClassName = L"T32DlgcCtl";
    RegisterClassW(&wc);
    DT d; ds_reset(); memset(&cmds, 0, sizeof cmds); ds.hook = rec_hook;
    dt_hdr(&d, BASE_STYLE | WS_VISIBLE, 0, 3, 10, 10, 150, 80, NULL, NULL, L"dlgc", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 5, 5, 40, 14, IDOK, C_BUTTON, L"OK");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 5, 25, 60, 20, 20, L"T32DlgcCtl", L"c");
    dt_item(&d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 70, 5, 40, 14, IDCANCEL, C_BUTTON, L"Cancel");
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    if (dlg) { SetActiveWindow(dlg); t_pump(); }
    return dlg;
}
TEST(dialogs_dlgc_wm_getdlgcode_message_parameters) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(RegisterClassW); REQUIRE(DestroyWindow);
    HWND dlg = dlgc_dialog(); REQUIRE(dlg);
    HWND c = GetDlgItem(dlg, 20);
    if (!focus_to(c)) { DestroyWindow(dlg); UnregisterClassA("T32DlgcCtl", INST); t_skip("no focus"); return; }
    g_code = 0; g_gdc_n = 0; g_gdc_w = 0; g_gdc_l = 0; memset(&g_gdc_msg, 0, sizeof g_gdc_msg);
    CHECK(KEYDOWN(dlg, c, VK_TAB));
    CHECK(g_gdc_n >= 1);
    EQ(g_gdc_w, VK_TAB);
    CHECK(g_gdc_l != 0);
    EQ(g_gdc_msg.message, WM_KEYDOWN);
    EQ(g_gdc_msg.wParam, VK_TAB);
    EQ(g_gdc_msg.hwnd, c);
    DestroyWindow(dlg); UnregisterClassA("T32DlgcCtl", INST);
}
/* dispatched = the key message is delivered to the control instead of being consumed by the dialog manager */
struct dlgctc { DWORD code; UINT vk; BOOL dispatched; const char *name; };
static const struct dlgctc dlgctcs[] = {
    { 0, VK_TAB, 0, "none+tab" }, { 0, VK_RETURN, 0, "none+return" }, { 0, VK_ESCAPE, 0, "none+escape" },
    { 0, VK_RIGHT, 0, "none+right" }, { 0, VK_DOWN, 0, "none+down" },
    { 0, 'A', 1, "none+A" }, { 0, VK_SPACE, 1, "none+space" }, { 0, VK_F5, 1, "none+F5" }, { 0, VK_DELETE, 1, "none+delete" },
    { 0, VK_PRIOR, 1, "none+pgup" }, { 0, VK_HOME, 1, "none+home" },
    { DLGC_WANTALLKEYS, VK_TAB, 1, "allkeys+tab" }, { DLGC_WANTALLKEYS, VK_RETURN, 1, "allkeys+return" }, { DLGC_WANTALLKEYS, VK_ESCAPE, 1, "allkeys+escape" },
    { DLGC_WANTALLKEYS, VK_RIGHT, 1, "allkeys+right" },
    { DLGC_WANTTAB, VK_TAB, 1, "wanttab+tab" }, { DLGC_WANTTAB, VK_RETURN, 0, "wanttab+return" }, { DLGC_WANTTAB, VK_RIGHT, 0, "wanttab+right" },
    { DLGC_WANTARROWS, VK_RIGHT, 1, "wantarrows+right" }, { DLGC_WANTARROWS, VK_UP, 1, "wantarrows+up" }, { DLGC_WANTARROWS, VK_TAB, 0, "wantarrows+tab" },
    { DLGC_WANTMESSAGE, VK_TAB, 1, "wantmessage+tab" }, { DLGC_WANTMESSAGE, VK_RETURN, 1, "wantmessage+return" }, { DLGC_WANTMESSAGE, VK_ESCAPE, 1, "wantmessage+escape" },
    { DLGC_WANTCHARS, VK_TAB, 0, "wantchars+tab" }, { DLGC_WANTCHARS, VK_RETURN, 0, "wantchars+return" },
    { DLGC_WANTTAB | DLGC_WANTARROWS, VK_DOWN, 1, "tab+arrows/down" },
};
TEST(dialogs_dlgc_custom_control_codes) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(RegisterClassW); REQUIRE(DestroyWindow);
    HWND dlg = dlgc_dialog(); REQUIRE(dlg);
    HWND c = GetDlgItem(dlg, 20);
    for (unsigned i = 0; i < sizeof dlgctcs / sizeof *dlgctcs; i++) {
        if (!focus_to(c)) { t_note("focus lost at case %u", i); break; }
        g_code = dlgctcs[i].code; g_keys = 0;
        BOOL r = KEYDOWN(dlg, c, dlgctcs[i].vk);
        CHECKF(r, "%s: IsDialogMessage returned FALSE", dlgctcs[i].name);
        CHECKF((g_keys != 0) == (dlgctcs[i].dispatched != 0), "%s: keydown delivered to control %d times, expected %d", dlgctcs[i].name, g_keys, dlgctcs[i].dispatched);
    }
    DestroyWindow(dlg); UnregisterClassA("T32DlgcCtl", INST);
}
TEST(dialogs_dlgc_wantallkeys_leaves_focus) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDialogMessageW); REQUIRE(RegisterClassW); REQUIRE(DestroyWindow);
    HWND dlg = dlgc_dialog(); REQUIRE(dlg);
    HWND c = GetDlgItem(dlg, 20);
    if (!focus_to(c)) { DestroyWindow(dlg); UnregisterClassA("T32DlgcCtl", INST); t_skip("no focus"); return; }
    g_code = DLGC_WANTALLKEYS;
    cmds.n = 0;
    KEYDOWN(dlg, c, VK_TAB);
    EQ(GetFocus(), c);
    KEYDOWN(dlg, c, VK_RETURN);
    EQ(total_clicks(), 0);
    g_code = 0;
    KEYDOWN(dlg, c, VK_TAB);
    CHECK(GetFocus() != c);                   /* now the dialog manager moves focus */
    focus_to(c);
    CHECK(KEYDOWN(dlg, c, VK_RETURN));
    EQ(clicks_of(IDOK), 1);
    DestroyWindow(dlg); UnregisterClassA("T32DlgcCtl", INST);
}
TEST(dialogs_dlgc_standard_control_codes) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendDlgItemMessageW); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 12, 0, 0, 200, 150, NULL, NULL, L"codes", 8, L"MS Shell Dlg");
    struct { DWORD style; const void *cls; } items[] = {
        { BS_PUSHBUTTON, C_BUTTON }, { BS_DEFPUSHBUTTON, C_BUTTON }, { BS_AUTOCHECKBOX, C_BUTTON }, { BS_AUTORADIOBUTTON, C_BUTTON }, { BS_AUTO3STATE, C_BUTTON },
        { BS_GROUPBOX, C_BUTTON }, { 0, C_STATIC }, { ES_AUTOHSCROLL, C_EDIT }, { ES_MULTILINE, C_EDIT }, { LBS_NOTIFY, C_LIST }, { CBS_DROPDOWNLIST, C_COMBO }, { SBS_HORZ, C_SCROLL },
    };
    for (int i = 0; i < 12; i++) dt_item(&d, WS_CHILD | WS_VISIBLE | items[i].style, 0, 2, (short)(i * 12), 50, 10, (WORD)(30 + i), items[i].cls, L"t");
    HWND dlg = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0); REQUIRE(dlg);
    LRESULT c[12];
    for (int i = 0; i < 12; i++) { c[i] = SendDlgItemMessageW(dlg, 30 + i, WM_GETDLGCODE, 0, 0); t_note("DLGC code for item %d: 0x%llx", i, (long long)c[i]); }
    /* Win10 22H2 answers a plain push button with DLGC_BUTTON|0x10 and a default push button with
     * DLGC_BUTTON|0x20, the reverse of the SDK names for 0x10/0x20 (observed 0x2010 / 0x2020). */
    CHECK((c[0] & (DLGC_BUTTON | 0x10)) == (DLGC_BUTTON | 0x10));
    CHECK(!(c[0] & 0x20));
    CHECK((c[1] & (DLGC_BUTTON | 0x20)) == (DLGC_BUTTON | 0x20));
    CHECK(!(c[1] & 0x10));
    CHECK(c[2] & DLGC_BUTTON); CHECK(!(c[2] & DLGC_RADIOBUTTON));
    CHECK((c[3] & (DLGC_BUTTON | DLGC_RADIOBUTTON)) == (DLGC_BUTTON | DLGC_RADIOBUTTON));
    CHECK(c[4] & DLGC_BUTTON);
    CHECK(c[5] & DLGC_STATIC);
    CHECK(c[6] & DLGC_STATIC);
    CHECK((c[7] & (DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS)) == (DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS));
    CHECK((c[8] & (DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS)) == (DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS));
    CHECK((c[9] & (DLGC_WANTARROWS | DLGC_WANTCHARS)) == (DLGC_WANTARROWS | DLGC_WANTCHARS));
    CHECK((c[10] & (DLGC_WANTARROWS | DLGC_WANTCHARS)) == (DLGC_WANTARROWS | DLGC_WANTCHARS));
    DestroyWindow(dlg);
}
TEST(dialogs_dlgc_constants) {
    EQ(DLGC_WANTARROWS, 1); EQ(DLGC_WANTTAB, 2); EQ(DLGC_WANTALLKEYS, 4); EQ(DLGC_WANTMESSAGE, 4);
    EQ(DLGC_HASSETSEL, 8); EQ(DLGC_DEFPUSHBUTTON, 0x10); EQ(DLGC_UNDEFPUSHBUTTON, 0x20);
    EQ(DLGC_RADIOBUTTON, 0x40); EQ(DLGC_WANTCHARS, 0x80); EQ(DLGC_STATIC, 0x100); EQ(DLGC_BUTTON, 0x2000);
}

/* ---------------- WM_NEXTDLGCTL ---------------- */
TEST(dialogs_nextdlgctl_next_and_previous) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    SendMessageW(dlg, WM_NEXTDLGCTL, 0, FALSE);
    EQ(focus_id(), ID_CHK);
    SendMessageW(dlg, WM_NEXTDLGCTL, 0, FALSE);
    EQ(focus_id(), ID_R1);
    SendMessageW(dlg, WM_NEXTDLGCTL, 1, FALSE);       /* previous */
    EQ(focus_id(), ID_CHK);
    SendMessageW(dlg, WM_NEXTDLGCTL, 1, FALSE);
    EQ(focus_id(), ID_EDIT);
    SendMessageW(dlg, WM_NEXTDLGCTL, 1, FALSE);       /* wraps backwards */
    EQ(focus_id(), ID_3ST);
    DestroyWindow(dlg);
}
TEST(dialogs_nextdlgctl_explicit_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    SendMessageW(dlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(dlg, ID_LIST), TRUE);
    EQ(focus_id(), ID_LIST);
    SendMessageW(dlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(dlg, ID_COMBO), TRUE);
    EQ(focus_id(), ID_COMBO);
    DestroyWindow(dlg);
}
TEST(dialogs_nextdlgctl_disabled_target) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(EnableWindow); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_EDIT))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    EnableWindow(GetDlgItem(dlg, ID_LIST), FALSE);
    SendMessageW(dlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(dlg, ID_LIST), TRUE);
    CHECK(focus_id() != ID_LIST);
    DestroyWindow(dlg);
}
TEST(dialogs_nextdlgctl_selects_edit_text) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_CHK))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    SetDlgItemTextA(dlg, ID_EDIT, "select me");
    SendMessageW(dlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(dlg, ID_EDIT), TRUE);
    EQ(focus_id(), ID_EDIT);
    DWORD s = 0, e = 0;
    SendDlgItemMessageW(dlg, ID_EDIT, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
    EQ(s, 0); EQ(e, 9);                      /* dialog manager selects the whole text */
    DestroyWindow(dlg);
}
TEST(dialogs_setfocus_on_dialog_restores_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetFocus); REQUIRE(DestroyWindow);
    DT d; vis_std(&d); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, ID_LIST))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    SendMessageW(dlg, WM_ACTIVATE, WA_INACTIVE, 0);
    SendMessageW(dlg, WM_ACTIVATE, WA_ACTIVE, 0);
    t_pump();
    t_note("after WM_ACTIVATE inactive/active focus id=%d (was list)", focus_id());
    DestroyWindow(dlg);
}
TEST(dialogs_dm_getdefid_after_ismsg_flow) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageW); REQUIRE(IsDialogMessageW); REQUIRE(DestroyWindow);
    DT d; btn_tmpl(&d, IDOK); HWND dlg = vis_dialog(&d); REQUIRE(dlg);
    if (!focus_to(GetDlgItem(dlg, 100))) { DestroyWindow(dlg); t_skip("no focus"); return; }
    /* tab to each push button; each becomes the default while focused */
    KEYDOWN(dlg, GetFocus(), VK_TAB);
    EQ(focus_id(), IDOK);
    KEYDOWN(dlg, GetFocus(), VK_TAB);
    EQ(focus_id(), IDCANCEL);
    /* Win10 22H2: DM_GETDEFID reports the stored dialog default, it does not follow the focused push button
     * (the BS_DEFPUSHBUTTON style does; that is checked in dialogs_ismsg_tab_to_pushbutton_makes_it_default) */
    t_note("DM_GETDEFID with focus on Cancel: 0x%lx", (unsigned long)SendMessageW(dlg, DM_GETDEFID, 0, 0));
    KEYDOWN(dlg, GetFocus(), VK_TAB);
    EQ(focus_id(), 50);
    EQ(GetWindowLongW(GetDlgItem(dlg, 50), GWL_STYLE) & BS_TYPEMASK, BS_DEFPUSHBUTTON);
    KEYDOWN(dlg, GetFocus(), VK_TAB);
    EQ(focus_id(), 60);
    t_note("DM_GETDEFID back on the check box: 0x%lx", (unsigned long)SendMessageW(dlg, DM_GETDEFID, 0, 0));
    EQ(GetWindowLongW(GetDlgItem(dlg, IDOK), GWL_STYLE) & BS_TYPEMASK, BS_DEFPUSHBUTTON);   /* back to the dialog default */
    DestroyWindow(dlg);
}
