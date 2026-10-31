/* gap_04: SetDialogDpiChangeBehavior / SetDialogControlDpiChangeBehavior */
#include "t.h"

typedef BOOL (WINAPI *setdlg_fn)(HWND, DWORD, DWORD);
typedef DWORD (WINAPI *getdlg_fn)(HWND);

static INT_PTR CALLBACK nulldlgproc(HWND h, UINT m, WPARAM w, LPARAM l) { return FALSE; }

/* Hidden dialog from an in-memory template (no modal loop). */
static HWND make_dialog(void) {
    static WORD tmpl[32];
    memset(tmpl, 0, sizeof tmpl);
    DLGTEMPLATE *dt = (DLGTEMPLATE *)tmpl;
    dt->style = WS_POPUP | WS_CAPTION | DS_MODALFRAME;
    dt->cdit = 0; dt->x = 0; dt->y = 0; dt->cx = 60; dt->cy = 40;
    /* menu(0), class(0) already zero; title "t" */
    WORD *p = (WORD *)(dt + 1);
    p[0] = 0; p[1] = 0; p[2] = 't'; p[3] = 0;
    HWND (WINAPI *cd)(HINSTANCE, LPCDLGTEMPLATEW, HWND, DLGPROC, LPARAM) = (void *)t_proc("CreateDialogIndirectParamW");
    if (!cd) return NULL;
    return cd(GetModuleHandleA(NULL), (LPCDLGTEMPLATEW)tmpl, NULL, nulldlgproc, 0);
}
static void kill_(HWND h) { BOOL (WINAPI *d)(HWND) = (void *)t_proc("DestroyWindow"); if (d && h) d(h); }

#define NEED_DLG() \
    setdlg_fn setb = (setdlg_fn)t_proc("SetDialogDpiChangeBehavior"); \
    getdlg_fn getb = (getdlg_fn)t_proc("GetDialogDpiChangeBehavior"); \
    REQUIRE(setb); REQUIRE(getb); \
    HWND dlg = make_dialog(); \
    if (!dlg) { t_skip("cannot create dialog"); return; }

TEST(gap4_dlgdpi_default_is_zero) {
    NEED_DLG();
    EXPECT_EQ(getb(dlg), 0);
    kill_(dlg);
}
TEST(gap4_dlgdpi_set_each_flag) {
    NEED_DLG();
    static const DWORD flags[] = {1, 2, 4};
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(setb(dlg, 7, 0), 1);
        EXPECT_EQ(getb(dlg), 0);
        SET_ERR(0);
        EXPECT_EQ(setb(dlg, flags[i], flags[i]), 1);
        EXPECT_EQ(getb(dlg), flags[i]);
        EXPECT_EQ(setb(dlg, flags[i], 0), 1);
        EXPECT_EQ(getb(dlg), 0);
    }
    kill_(dlg);
}
TEST(gap4_dlgdpi_mask_semantics) {
    NEED_DLG();
    /* Only bits in mask are changed, using the corresponding bit of values. */
    EXPECT_EQ(setb(dlg, 7, 7), 1); EXPECT_EQ(getb(dlg), 7);
    EXPECT_EQ(setb(dlg, 3, 0), 1); EXPECT_EQ(getb(dlg), 4);
    EXPECT_EQ(setb(dlg, 1, 3), 1); EXPECT_EQ(getb(dlg), 5);
    EXPECT_EQ(setb(dlg, 2, 5), 1); EXPECT_EQ(getb(dlg), 5);
    EXPECT_EQ(setb(dlg, 0, 7), 1); EXPECT_EQ(getb(dlg), 5);
    EXPECT_EQ(setb(dlg, 0, 0), 1); EXPECT_EQ(getb(dlg), 5);
    EXPECT_EQ(setb(dlg, 4, 0), 1); EXPECT_EQ(getb(dlg), 1);
    EXPECT_EQ(setb(dlg, 7, 2), 1); EXPECT_EQ(getb(dlg), 2);
    EXPECT_EQ(setb(dlg, 7, 0), 1); EXPECT_EQ(getb(dlg), 0);
    kill_(dlg);
}
TEST(gap4_dlgdpi_mask_table) {
    NEED_DLG();
    for (DWORD init = 0; init < 8; init++)
        for (DWORD mask = 0; mask < 8; mask++)
            for (DWORD val = 0; val < 8; val++) {
                setb(dlg, 7, 0);
                setb(dlg, 7, init);
                BOOL r = setb(dlg, mask, val);
                DWORD want = (init & ~mask) | (mask & val);
                CHECKF(r == 1 && getb(dlg) == want, "init %lu mask %lu val %lu got %lu want %lu", init, mask, val, getb(dlg), want);
            }
    setb(dlg, 7, 0);
    kill_(dlg);
}
TEST(gap4_dlgdpi_high_bits_ignored) {
    NEED_DLG();
    EXPECT_EQ(setb(dlg, 0xFFF8, 0xFFFF), 1);
    EXPECT_EQ(getb(dlg), 0);
    EXPECT_EQ(setb(dlg, 0xFFFF, 0xFFFF), 1);
    EXPECT_EQ(getb(dlg), 7);
    EXPECT_EQ(setb(dlg, 0xFFFF, 0), 1);
    EXPECT_EQ(getb(dlg), 0);
    EXPECT_EQ(setb(dlg, 0x8, 0x8), 1);
    EXPECT_EQ(getb(dlg), 0);
    EXPECT_EQ(setb(dlg, 0x10, 0x10), 1);
    EXPECT_EQ(getb(dlg), 0);
    EXPECT_EQ(setb(dlg, 0x1, 0xFFF9), 1);
    EXPECT_EQ(getb(dlg), 1);
    setb(dlg, 7, 0);
    kill_(dlg);
}
TEST(gap4_dlgdpi_wide_args_truncated) {
    NEED_DLG();
    /* The Win10 implementation takes the mask/values as 16-bit quantities. */
    BOOL r = setb(dlg, 0x10002, 0x10002);
    if (r) t_note("mask 0x10002 accepted, behaviour now %lu", getb(dlg));
    else t_note("mask 0x10002 rejected, err %lu", GetLastError());
    setb(dlg, 7, 0);
    kill_(dlg);
}
TEST(gap4_dlgdpi_repeat_idempotent) {
    NEED_DLG();
    for (int i = 0; i < 200; i++) {
        EXPECT_EQ(setb(dlg, 7, 5), 1);
        EXPECT_EQ(getb(dlg), 5);
    }
    setb(dlg, 7, 0);
    kill_(dlg);
}
TEST(gap4_dlgdpi_independent_dialogs) {
    NEED_DLG();
    HWND d2 = make_dialog();
    if (!d2) { kill_(dlg); t_skip("no second dialog"); return; }
    EXPECT_EQ(setb(dlg, 7, 1), 1);
    EXPECT_EQ(setb(d2, 7, 6), 1);
    EXPECT_EQ(getb(dlg), 1);
    EXPECT_EQ(getb(d2), 6);
    kill_(dlg);
    EXPECT_EQ(getb(d2), 6);
    kill_(d2);
}
TEST(gap4_dlgdpi_destroyed_handle) {
    NEED_DLG();
    kill_(dlg);
    SET_ERR(0);
    EXPECT_EQ(setb(dlg, 7, 7), 0);
    DWORD e = GetLastError();
    if (e != ERROR_INVALID_WINDOW_HANDLE) t_note("destroyed dialog last error %lu (expected 1400)", e);
    SET_ERR(0);
    EXPECT_EQ(setb(dlg, 0, 0), 0);
}
TEST(gap4_dlgdpi_invalid_handles) {
    setdlg_fn setb = (setdlg_fn)t_proc("SetDialogDpiChangeBehavior"); REQUIRE(setb);
    static const ULONG_PTR hs[] = {0, 1, 2, 0x10, 0x1234, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF, (ULONG_PTR)-1, (ULONG_PTR)-2, 0x7FFFFFFFFFFFFFFFull, 0xDEADBEEF};
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        SET_ERR(0);
        BOOL r = setb((HWND)hs[i], 7, 7);
        CHECKF(r == 0, "handle %p returned %d", (void *)hs[i], r);
        DWORD e = GetLastError();
        if (e != ERROR_INVALID_WINDOW_HANDLE) t_note("handle %p last error %lu (1400 expected)", (void *)hs[i], e);
    }
}
TEST(gap4_dlgdpi_plain_window_is_not_dialog) {
    setdlg_fn setb = (setdlg_fn)t_proc("SetDialogDpiChangeBehavior"); REQUIRE(setb);
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 50, 50, NULL);
    if (!w) { t_skip("no window"); return; }
    SET_ERR(0);
    EXPECT_EQ(setb(w, 7, 7), 0);
    t_note("plain window: last error %lu", GetLastError());
    kill_(w);
}
TEST(gap4_dlgdpi_child_control_is_not_dialog) {
    setdlg_fn setb = (setdlg_fn)t_proc("SetDialogDpiChangeBehavior"); REQUIRE(setb);
    HWND p = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 100, 100, NULL);
    if (!p) { t_skip("no window"); return; }
    HWND b = t_make_window("BUTTON", WS_CHILD | WS_VISIBLE, 0, 0, 0, 20, 20, p);
    if (b) { EXPECT_EQ(setb(b, 7, 7), 0); }
    kill_(p);
}
TEST(gap4_dlgdpi_desktop_window_other_process) {
    setdlg_fn setb = (setdlg_fn)t_proc("SetDialogDpiChangeBehavior"); REQUIRE(setb);
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow"); REQUIRE(gdw);
    HWND d = gdw();
    DWORD pid = 0;
    DWORD (WINAPI *gwt)(HWND, LPDWORD) = (void *)t_proc("GetWindowThreadProcessId");
    if (gwt) gwt(d, &pid);
    SET_ERR(0);
    EXPECT_EQ(setb(d, 7, 7), 0);
    DWORD e = GetLastError();
    if (pid != GetCurrentProcessId()) EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    HWND (WINAPI *gsw)(void) = (void *)t_proc("GetShellWindow");
    HWND sh = gsw ? gsw() : NULL;
    if (sh) {
        pid = 0; if (gwt) gwt(sh, &pid);
        if (pid != GetCurrentProcessId()) {
            SET_ERR(0);
            EXPECT_EQ(setb(sh, 7, 7), 0);
            EXPECT_ERR(0, ERROR_ACCESS_DENIED);
        }
    }
}

/* Cross-thread (same process) dialog */
struct xt { HWND dlg; HANDLE ready, quit; };
static DWORD WINAPI xt_thread(LPVOID a) {
    struct xt *x = a;
    x->dlg = make_dialog();
    SetEvent(x->ready);
    WaitForSingleObject(x->quit, 8000);
    kill_(x->dlg);
    return 0;
}
TEST(gap4_dlgdpi_other_thread_same_process) {
    setdlg_fn setb = (setdlg_fn)t_proc("SetDialogDpiChangeBehavior");
    getdlg_fn getb = (getdlg_fn)t_proc("GetDialogDpiChangeBehavior");
    REQUIRE(setb); REQUIRE(getb);
    struct xt x; x.dlg = NULL;
    x.ready = CreateEventA(NULL, TRUE, FALSE, NULL); x.quit = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE th = CreateThread(NULL, 0, xt_thread, &x, 0, NULL);
    if (!th) { t_skip("thread"); return; }
    if (WaitForSingleObject(x.ready, 5000) == WAIT_OBJECT_0 && x.dlg) {
        EXPECT_EQ(setb(x.dlg, 7, 3), 1);
        EXPECT_EQ(getb(x.dlg), 3);
        EXPECT_EQ(setb(x.dlg, 1, 0), 1);
        EXPECT_EQ(getb(x.dlg), 2);
        setb(x.dlg, 7, 0);
    } else CHECK(0);
    SetEvent(x.quit);
    WaitForSingleObject(th, 5000);
    CloseHandle(th); CloseHandle(x.ready); CloseHandle(x.quit);
}
TEST(gap4_dlgdpi_after_thread_exit) {
    setdlg_fn setb = (setdlg_fn)t_proc("SetDialogDpiChangeBehavior"); REQUIRE(setb);
    struct xt x; x.dlg = NULL;
    x.ready = CreateEventA(NULL, TRUE, FALSE, NULL); x.quit = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE th = CreateThread(NULL, 0, xt_thread, &x, 0, NULL);
    if (!th) { t_skip("thread"); return; }
    WaitForSingleObject(x.ready, 5000);
    SetEvent(x.quit);
    WaitForSingleObject(th, 5000);
    SET_ERR(0);
    EXPECT_EQ(setb(x.dlg, 7, 7), 0); /* window died with its thread */
    CloseHandle(th); CloseHandle(x.ready); CloseHandle(x.quit);
}

/* ---------------- SetDialogControlDpiChangeBehavior ---------------- */
#define NEED_CTL() \
    setdlg_fn setc = (setdlg_fn)t_proc("SetDialogControlDpiChangeBehavior"); \
    getdlg_fn getc = (getdlg_fn)t_proc("GetDialogControlDpiChangeBehavior"); \
    REQUIRE(setc); REQUIRE(getc); \
    HWND par = make_dialog(); \
    if (!par) { t_skip("no dialog"); return; } \
    HWND ctl = t_make_window("BUTTON", WS_CHILD, 0, 0, 0, 20, 20, par); \
    if (!ctl) { kill_(par); t_skip("no control"); return; }

TEST(gap4_ctldpi_default_zero) {
    NEED_CTL();
    EXPECT_EQ(getc(ctl), 0);
    kill_(par);
}
TEST(gap4_ctldpi_roundtrip) {
    NEED_CTL();
    SET_ERR(0);
    BOOL r = setc(ctl, 3, 3);
    if (!r) { t_note("SetDialogControlDpiChangeBehavior on child button failed, err %lu", GetLastError()); kill_(par); return; }
    EXPECT_EQ(getc(ctl), 3);
    EXPECT_EQ(setc(ctl, 1, 0), 1);
    EXPECT_EQ(getc(ctl), 2);
    EXPECT_EQ(setc(ctl, 2, 0), 1);
    EXPECT_EQ(getc(ctl), 0);
    EXPECT_EQ(setc(ctl, 0, 3), 1);
    EXPECT_EQ(getc(ctl), 0);
    EXPECT_EQ(setc(ctl, 1, 1), 1);
    EXPECT_EQ(getc(ctl), 1);
    EXPECT_EQ(setc(ctl, 3, 0), 1);
    EXPECT_EQ(getc(ctl), 0);
    kill_(par);
}
TEST(gap4_ctldpi_mask_table) {
    NEED_CTL();
    if (!setc(ctl, 3, 0)) { t_note("set failed err %lu", GetLastError()); kill_(par); return; }
    for (DWORD init = 0; init < 4; init++)
        for (DWORD mask = 0; mask < 4; mask++)
            for (DWORD val = 0; val < 4; val++) {
                setc(ctl, 3, init);
                BOOL r = setc(ctl, mask, val);
                DWORD want = (init & ~mask) | (mask & val);
                CHECKF(r == 1 && getc(ctl) == want, "init %lu mask %lu val %lu got %lu want %lu", init, mask, val, getc(ctl), want);
            }
    setc(ctl, 3, 0);
    kill_(par);
}
TEST(gap4_ctldpi_invalid_bits_note) {
    NEED_CTL();
    SET_ERR(0);
    BOOL r = setc(ctl, 0xFFFF, 0xFFFF);
    t_note("mask 0xFFFF: ret %d err %lu now %lu", r, GetLastError(), getc(ctl));
    setc(ctl, 3, 0);
    r = setc(ctl, 4, 4);
    t_note("mask 4: ret %d now %lu", r, getc(ctl));
    setc(ctl, 3, 0);
    kill_(par);
}
TEST(gap4_ctldpi_invalid_handles) {
    setdlg_fn setc = (setdlg_fn)t_proc("SetDialogControlDpiChangeBehavior"); REQUIRE(setc);
    static const ULONG_PTR hs[] = {0, 1, 0x1234, 0xFFFFFFFF, (ULONG_PTR)-1, 0xDEADBEEF};
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        SET_ERR(0);
        BOOL r = setc((HWND)hs[i], 3, 3);
        CHECKF(r == 0, "handle %p returned %d", (void *)hs[i], r);
        DWORD e = GetLastError();
        if (e != ERROR_INVALID_WINDOW_HANDLE) t_note("ctl handle %p last error %lu", (void *)hs[i], e);
    }
}
TEST(gap4_ctldpi_destroyed_handle) {
    NEED_CTL();
    kill_(par); /* destroys the child as well */
    SET_ERR(0);
    EXPECT_EQ(setc(ctl, 3, 3), 0);
    SET_ERR(0);
    EXPECT_EQ(setc(par, 3, 3), 0);
}
TEST(gap4_ctldpi_desktop_window) {
    setdlg_fn setc = (setdlg_fn)t_proc("SetDialogControlDpiChangeBehavior"); REQUIRE(setc);
    HWND (WINAPI *gdw)(void) = (void *)t_proc("GetDesktopWindow"); REQUIRE(gdw);
    SET_ERR(0);
    EXPECT_EQ(setc(gdw(), 3, 3), 0);
    t_note("desktop window control behaviour: err %lu", GetLastError());
}
TEST(gap4_ctldpi_toplevel_window) {
    NEED_CTL();
    SET_ERR(0);
    BOOL r = setc(par, 3, 3);
    t_note("dialog as control: ret %d err %lu", r, GetLastError());
    if (r) setc(par, 3, 0);
    kill_(par);
}
TEST(gap4_ctldpi_independent_controls) {
    NEED_CTL();
    HWND c2 = t_make_window("BUTTON", WS_CHILD, 0, 25, 0, 20, 20, par);
    if (!c2) { kill_(par); t_skip("no ctl2"); return; }
    if (!setc(ctl, 3, 1)) { t_note("set failed"); kill_(par); return; }
    EXPECT_EQ(getc(c2), 0);
    EXPECT_EQ(setc(c2, 3, 2), 1);
    EXPECT_EQ(getc(ctl), 1);
    EXPECT_EQ(getc(c2), 2);
    kill_(par);
}
TEST(gap4_ctldpi_dialog_and_control_independent) {
    NEED_CTL();
    setdlg_fn setb = (setdlg_fn)t_proc("SetDialogDpiChangeBehavior");
    getdlg_fn getb = (getdlg_fn)t_proc("GetDialogDpiChangeBehavior");
    REQUIRE(setb); REQUIRE(getb);
    EXPECT_EQ(setb(par, 7, 5), 1);
    if (setc(ctl, 3, 2)) {
        EXPECT_EQ(getb(par), 5);
        EXPECT_EQ(getc(ctl), 2);
    }
    setb(par, 7, 0);
    kill_(par);
}
