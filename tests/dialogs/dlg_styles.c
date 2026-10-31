/* dialogs: DS_* behaviour, message ordering, WM_ENTERIDLE, resource-based creation */
#include "dlgutil.h"

#define INST GetModuleHandleA(NULL)
#define CDW(t, par, lp) CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)(t)->b, par, gdp, lp)

static int idle_n; static WPARAM idle_w; static LPARAM idle_l;
static LRESULT CALLBACK owner_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ENTERIDLE) { idle_n++; idle_w = w; idle_l = l; }
    return DefWindowProcW(h, m, w, l);
}
static HWND make_owner(int x, int y) {
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = owner_proc; wc.hInstance = INST; wc.lpszClassName = L"T32DlgOwner";
    RegisterClassW(&wc);
    return CreateWindowExW(0, L"T32DlgOwner", L"owner", WS_OVERLAPPEDWINDOW, x, y, 400, 300, NULL, NULL, INST, NULL);
}
static void unreg_owner(void) { UnregisterClassA("T32DlgOwner", INST); }
static void mini(DT *d, DWORD style) {
    dt_hdr(d, style, 0, 1, 10, 20, 100, 60, NULL, NULL, L"pos", 8, L"MS Shell Dlg");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 5, 5, 40, 14, 1, C_BUTTON, L"b");
}

/* ---------------- WM_ENTERIDLE ---------------- */
TEST(dialogs_modal_sends_enteridle_to_owner) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(RegisterClassW); REQUIRE(DestroyWindow);
    HWND own = make_owner(50, 50); REQUIRE(own);
    DT d; mini(&d, BASE_STYLE); ds_reset(); ds.tmo = 250; ds.endval = 3;
    idle_n = 0;
    EQ(DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, own, gdp, 0), 3);
    CHECK(idle_n >= 1);
    EQ(idle_w, MSGF_DIALOGBOX);
    EQ(idle_l, (LPARAM)ds.hinit);
    DestroyWindow(own); unreg_owner();
}
TEST(dialogs_modal_ds_noidlemsg_suppresses_enteridle) {
    REQUIRE(DialogBoxIndirectParamW); REQUIRE(RegisterClassW); REQUIRE(DestroyWindow);
    HWND own = make_owner(50, 50); REQUIRE(own);
    DT d; mini(&d, BASE_STYLE | DS_NOIDLEMSG); ds_reset(); ds.tmo = 250; ds.endval = 3;
    idle_n = 0;
    EQ(DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, own, gdp, 0), 3);
    EQ(idle_n, 0);
    DestroyWindow(own); unreg_owner();
}
TEST(dialogs_modeless_never_sends_enteridle) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(RegisterClassW); REQUIRE(DestroyWindow);
    HWND own = make_owner(50, 50); REQUIRE(own);
    DT d; mini(&d, BASE_STYLE); ds_reset();
    idle_n = 0;
    HWND h = CDW(&d, own, 0); REQUIRE(h);
    t_pump();
    EQ(idle_n, 0);
    DestroyWindow(h); DestroyWindow(own); unreg_owner();
}

/* ---------------- position ---------------- */
static void rect_of(HWND h, RECT *r) { GetWindowRect(h, r); }
TEST(dialogs_position_relative_to_owner_client) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(RegisterClassW); REQUIRE(GetWindowRect); REQUIRE(DestroyWindow); REQUIRE(MoveWindow);
    HWND own = make_owner(100, 100); REQUIRE(own);
    DT d; mini(&d, BASE_STYLE); ds_reset();
    HWND d1 = CDW(&d, own, 0); REQUIRE(d1);
    RECT r1; rect_of(d1, &r1);
    MoveWindow(own, 160, 130, 400, 300, FALSE);
    HWND d2 = CDW(&d, own, 0); REQUIRE(d2);
    RECT r2; rect_of(d2, &r2);
    EQ(r2.left - r1.left, 60);
    EQ(r2.top - r1.top, 30);
    DestroyWindow(d1); DestroyWindow(d2); DestroyWindow(own); unreg_owner();
}
TEST(dialogs_position_absalign_ignores_owner) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(RegisterClassW); REQUIRE(GetWindowRect); REQUIRE(DestroyWindow); REQUIRE(MoveWindow);
    HWND own = make_owner(100, 100); REQUIRE(own);
    DT d; mini(&d, BASE_STYLE | DS_ABSALIGN); ds_reset();
    HWND d1 = CDW(&d, own, 0); REQUIRE(d1);
    RECT r1; rect_of(d1, &r1);
    MoveWindow(own, 160, 130, 400, 300, FALSE);
    HWND d2 = CDW(&d, own, 0); REQUIRE(d2);
    RECT r2; rect_of(d2, &r2);
    EQ(r2.left, r1.left);
    EQ(r2.top, r1.top);
    DestroyWindow(d1); DestroyWindow(d2); DestroyWindow(own); unreg_owner();
}
TEST(dialogs_position_without_owner_is_screen_relative) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowRect); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE); ds_reset();
    HWND h1 = CDW(&d, NULL, 0); REQUIRE(h1);
    HWND h2 = CDW(&d, NULL, 0); REQUIRE(h2);
    RECT a, b; rect_of(h1, &a); rect_of(h2, &b);
    EQ(a.left, b.left); EQ(a.top, b.top);
    t_note("ownerless dialog at %ld,%ld (template 10,20 dlu)", a.left, a.top);
    DestroyWindow(h1); DestroyWindow(h2);
}
TEST(dialogs_position_ds_center_inside_screen) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowRect); REQUIRE(GetSystemMetrics); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE | DS_CENTER); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    RECT r; rect_of(h, &r);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    CHECKF(r.left >= vx && r.top >= vy && r.right <= vx + vw && r.bottom <= vy + vh, "rect %ld,%ld-%ld,%ld", r.left, r.top, r.right, r.bottom);
    int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);
    t_note("DS_CENTER center=(%ld,%ld) primary screen center=(%d,%d)", (r.left + r.right) / 2, (r.top + r.bottom) / 2, cx / 2, cy / 2);
    DestroyWindow(h);
}
TEST(dialogs_position_ds_center_ignores_template_xy) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowRect); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE | DS_CENTER, 0, 0, 0, 0, 100, 60, NULL, NULL, L"c", 8, L"MS Shell Dlg");
    HWND h1 = CDW(&d, NULL, 0); REQUIRE(h1);
    dt_hdr(&d, BASE_STYLE | DS_CENTER, 0, 0, 200, 100, 100, 60, NULL, NULL, L"c", 8, L"MS Shell Dlg");
    HWND h2 = CDW(&d, NULL, 0); REQUIRE(h2);
    RECT a, b; rect_of(h1, &a); rect_of(h2, &b);
    EQ(a.left, b.left); EQ(a.top, b.top);
    DestroyWindow(h1); DestroyWindow(h2);
}
TEST(dialogs_position_ds_centermouse_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowRect); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE | DS_CENTERMOUSE); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    RECT r; rect_of(h, &r);
    POINT p; GetCursorPos(&p);
    t_note("DS_CENTERMOUSE center=(%ld,%ld) cursor=(%ld,%ld)", (r.left + r.right) / 2, (r.top + r.bottom) / 2, p.x, p.y);
    DestroyWindow(h);
}

/* ---------------- message ordering ---------------- */
static int pos_of(UINT m) { for (int i = 0; i < ds.nmsg; i++) if (ds.seq[i] == m) return i; return -1; }
TEST(dialogs_order_initdialog_before_showwindow) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE | WS_VISIBLE); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    int i = pos_of(WM_INITDIALOG), s = pos_of(WM_SHOWWINDOW);
    CHECK(i >= 0);
    CHECKF(s < 0 || i < s, "WM_INITDIALOG at %d, WM_SHOWWINDOW at %d", i, s);
    DestroyWindow(h);
}
TEST(dialogs_order_hidden_dialog_no_showwindow) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    t_pump();
    EQ(pos_of(WM_SHOWWINDOW), -1);
    DestroyWindow(h);
}
static int seq_n; static int s_dlg_destroy, s_dlg_ncdestroy, s_kid_destroy, s_kid_ncdestroy;
static LRESULT CALLBACK kid_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) s_kid_destroy = ++seq_n;
    if (m == WM_NCDESTROY) s_kid_ncdestroy = ++seq_n;
    return DefWindowProcW(h, m, w, l);
}
static INT_PTR CALLBACK order_hook(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) s_dlg_destroy = ++seq_n;
    if (m == WM_NCDESTROY) s_dlg_ncdestroy = ++seq_n;
    return -1;
}
TEST(dialogs_order_destroy_sequence_parent_first) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(RegisterClassW); REQUIRE(DestroyWindow);
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = kid_proc; wc.hInstance = INST; wc.lpszClassName = L"T32DlgKid";
    REQUIRE(RegisterClassW(&wc));
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 1, 0, 0, 100, 60, NULL, NULL, L"o", 8, L"MS Shell Dlg");
    dt_item(&d, WS_CHILD | WS_VISIBLE, 0, 2, 2, 20, 20, 5, L"T32DlgKid", L"k");
    seq_n = s_dlg_destroy = s_dlg_ncdestroy = s_kid_destroy = s_kid_ncdestroy = 0;
    ds.hook = order_hook;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    DestroyWindow(h);
    CHECKF(s_dlg_destroy && s_kid_destroy && s_kid_ncdestroy && s_dlg_ncdestroy, "%d %d %d %d", s_dlg_destroy, s_kid_destroy, s_kid_ncdestroy, s_dlg_ncdestroy);
    CHECK(s_dlg_destroy < s_kid_destroy);
    CHECK(s_kid_destroy < s_kid_ncdestroy);
    CHECK(s_kid_ncdestroy < s_dlg_ncdestroy);
    UnregisterClassA("T32DlgKid", INST);
}
TEST(dialogs_order_enddialog_destroys_after_loop) {
    REQUIRE(DialogBoxIndirectParamW);
    DT d; mini(&d, BASE_STYLE); ds_reset(); ds.end_in_init = 1; ds.endval = 7;
    EQ(DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0), 7);
    int i = pos_of(WM_INITDIALOG), dd = pos_of(WM_DESTROY), nd = pos_of(WM_NCDESTROY);
    CHECK(i >= 0 && dd > i && nd > dd);
}

/* ---------------- ANSI/Unicode dialog procedure parity ---------------- */
static char got_a[64]; static WCHAR got_w[64]; static int got_kind;
static INT_PTR CALLBACK settext_hook(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SETTEXT && l) {
        if (((const char *)l)[1] == 0) { got_kind = 2; lstrcpynW(got_w, (const WCHAR *)l, 64); }
        else { got_kind = 1; lstrcpynA(got_a, (const char *)l, 64); }
    }
    return -1;
}
TEST(dialogs_dlgproc_text_conversion_ansi_dialog) {
    REQUIRE(CreateDialogIndirectParamA); REQUIRE(SendMessageA); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE); ds_reset(); ds.hook = settext_hook;
    HWND h = CreateDialogIndirectParamA(INST, (LPCDLGTEMPLATEA)d.b, NULL, gdp, 0); REQUIRE(h);
    got_kind = 0; SendMessageA(h, WM_SETTEXT, 0, (LPARAM)"abcd");
    EQ(got_kind, 1); EXPECT_STR(got_a, "abcd");
    got_kind = 0; SendMessageW(h, WM_SETTEXT, 0, (LPARAM)L"efgh");
    EQ(got_kind, 1); EXPECT_STR(got_a, "efgh");              /* ANSI procedure receives ANSI text */
    DestroyWindow(h);
}
TEST(dialogs_dlgproc_text_conversion_wide_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SendMessageA); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE); ds_reset(); ds.hook = settext_hook;
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    got_kind = 0; SendMessageW(h, WM_SETTEXT, 0, (LPARAM)L"abcd");
    EQ(got_kind, 2); EXPECT_WSTR(got_w, L"abcd");
    got_kind = 0; SendMessageA(h, WM_SETTEXT, 0, (LPARAM)"efgh");
    EQ(got_kind, 2); EXPECT_WSTR(got_w, L"efgh");            /* wide procedure receives wide text */
    DestroyWindow(h);
}
TEST(dialogs_dlgproc_window_unicode_flag_note) {
    REQUIRE(CreateDialogIndirectParamA); REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsWindowUnicode); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE); ds_reset();
    HWND a = CreateDialogIndirectParamA(INST, (LPCDLGTEMPLATEA)d.b, NULL, gdp, 0);
    HWND w = CDW(&d, NULL, 0);
    REQUIRE(a); REQUIRE(w);
    t_note("IsWindowUnicode: A-created dialog=%d W-created dialog=%d", IsWindowUnicode(a), IsWindowUnicode(w));
    CHECK(IsWindowUnicode(w));
    DestroyWindow(a); DestroyWindow(w);
}
TEST(dialogs_dwlp_dlgproc_ansi_wide_read) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetWindowLongPtrW); REQUIRE(GetWindowLongPtrA); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    LONG_PTR pw = GetWindowLongPtrW(h, DWLP_DLGPROC), pa = GetWindowLongPtrA(h, DWLP_DLGPROC);
    EQ(pw, (LONG_PTR)gdp);
    t_note("DWLP_DLGPROC via W=%p via A=%p (proc=%p)", (void *)pw, (void *)pa, (void *)gdp);
    DestroyWindow(h);
}
static int p2_n;
static INT_PTR CALLBACK p2proc(HWND hh, UINT m, WPARAM w, LPARAM l) { if (m == WM_USER + 40) { p2_n++; return 1; } return 0; }
TEST(dialogs_set_dwlp_dlgproc_swap) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetWindowLongPtrW); REQUIRE(SendMessageW); REQUIRE(DestroyWindow);
    DT d; mini(&d, BASE_STYLE); ds_reset();
    HWND h = CDW(&d, NULL, 0); REQUIRE(h);
    p2_n = 0;
    LONG_PTR old = SetWindowLongPtrW(h, DWLP_DLGPROC, (LONG_PTR)p2proc);
    EQ(old, (LONG_PTR)gdp);
    SendMessageW(h, WM_USER + 40, 0, 0);
    EQ(p2_n, 1);
    SetWindowLongPtrW(h, DWLP_DLGPROC, (LONG_PTR)gdp);
    SendMessageW(h, WM_USER + 40, 0, 0);
    EQ(p2_n, 1);
    DestroyWindow(h);
}

/* ---------------- resource based creation ---------------- */
TEST(dialogs_resource_missing_createdialog) {
    REQUIRE(CreateDialogParamA); REQUIRE(CreateDialogParamW);
    ds_reset();
    SET_ERR(0);
    HWND h = CreateDialogParamA(INST, "NoSuchDialogResource", NULL, gdp, 0);
    CHECK(h == NULL);
    t_note("CreateDialogParamA missing name: err=%lu", GetLastError());
    SET_ERR(0);
    h = CreateDialogParamW(INST, L"NoSuchDialogResource", NULL, gdp, 0);
    CHECK(h == NULL);
    SET_ERR(0);
    h = CreateDialogParamA(INST, MAKEINTRESOURCEA(4321), NULL, gdp, 0);
    CHECK(h == NULL);
    t_note("CreateDialogParamA missing ordinal: err=%lu", GetLastError());
    SET_ERR(0);
    h = CreateDialogParamW(INST, MAKEINTRESOURCEW(4321), NULL, gdp, 0);
    CHECK(h == NULL);
    EQ(ds.ninit, 0);
}
TEST(dialogs_resource_missing_dialogbox) {
    REQUIRE(DialogBoxParamA); REQUIRE(DialogBoxParamW);
    ds_reset();
    SET_ERR(0);
    EQ(DialogBoxParamA(INST, "NoSuchDialogResource", NULL, gdp, 0), -1);
    t_note("DialogBoxParamA missing name: err=%lu (1814 / 1813 expected)", GetLastError());
    SET_ERR(0);
    EQ(DialogBoxParamW(INST, L"NoSuchDialogResource", NULL, gdp, 0), -1);
    SET_ERR(0);
    EQ(DialogBoxParamA(INST, MAKEINTRESOURCEA(4321), NULL, gdp, 0), -1);
    SET_ERR(0);
    EQ(DialogBoxParamW(INST, MAKEINTRESOURCEW(4321), NULL, gdp, 0), -1);
    EQ(ds.ninit, 0);
}
TEST(dialogs_resource_missing_in_system_module) {
    REQUIRE(DialogBoxParamW); REQUIRE(CreateDialogParamW);
    ds_reset();
    HINSTANCE k = GetModuleHandleA("kernel32.dll");
    REQUIRE(k);
    EQ(DialogBoxParamW(k, MAKEINTRESOURCEW(1), NULL, gdp, 0), -1);
    CHECK(CreateDialogParamW(k, MAKEINTRESOURCEW(1), NULL, gdp, 0) == NULL);
    EQ(ds.ninit, 0);
}
TEST(dialogs_resource_missing_with_invalid_owner) {
    REQUIRE(DialogBoxParamW);
    ds_reset();
    SET_ERR(0);
    INT_PTR r = DialogBoxParamW(INST, L"NoSuchDialogResource", (HWND)0x12345678, gdp, 0);
    t_note("missing resource + invalid owner: ret=%lld err=%lu", (long long)r, GetLastError());
    CHECK(r == -1 || r == 0);
}
TEST(dialogs_indirect_zero_cdit_garbage_free) {
    /* a header-only template is valid and yields an empty dialog */
    REQUIRE(DialogBoxIndirectParamW);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 10, 10, NULL, NULL, L"", 0, NULL);
    ds.end_in_init = 1; ds.endval = 12;
    EQ(DialogBoxIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0), 12);
}
TEST(dialogs_indirect_menu_ordinal_template) {
    /* template naming a menu resource that does not exist: creation outcome is observed, not asserted */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetMenu); REQUIRE(DestroyWindow);
    DT d; ds_reset();
    dt_hdr(&d, BASE_STYLE, 0, 0, 0, 0, 60, 40, (const void *)(ULONG_PTR)9999, NULL, L"menu", 8, L"MS Shell Dlg");
    HWND h = CDW(&d, NULL, 0);
    t_note("template with missing menu 9999 -> dialog %p", (void *)h);
    if (h) { t_note("GetMenu=%p", (void *)GetMenu(h)); DestroyWindow(h); }
}
