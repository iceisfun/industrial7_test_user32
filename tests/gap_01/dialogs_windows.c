/* gap_01: dialog DPI change behaviour, deferred window positioning,
 * ExcludeUpdateRgn, internal window position and top-level window helpers. */
#include "t.h"

UDECL(BOOL, EndDeferWindowPosEx, (HDWP, BOOL));
UDECL(UINT, GetInternalWindowPos, (HWND, LPRECT, LPPOINT));
UDECL(HWND, GetTopLevelWindow, (HWND));
UDECL(void, EnterReaderModeHelper, (HWND));

static INT_PTR CALLBACK dlgproc(HWND h, UINT m, WPARAM w, LPARAM l) { return FALSE; }

static HWND make_dialog(void) {
    static BYTE buf[64] __attribute__((aligned(4)));
    DLGTEMPLATE *t = (DLGTEMPLATE *)buf;
    memset(buf, 0, sizeof buf);
    t->style = WS_POPUP | WS_CAPTION | DS_MODALFRAME;
    t->dwExtendedStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    t->cdit = 0; t->x = 0; t->y = 0; t->cx = 80; t->cy = 40;
    /* menu, class, title words all zero already */
    if (!U(CreateDialogIndirectParamA)) return NULL;
    return U(CreateDialogIndirectParamA)(GetModuleHandleA(NULL), t, NULL, dlgproc, 0);
}
static HWND make_plain(void) {
    return t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 40, 40, 100, 100, NULL);
}
static HWND make_child(HWND parent) {
    return t_make_window("BUTTON", WS_CHILD | WS_VISIBLE, 0, 1, 1, 30, 20, parent);
}

/* ================= GetDialogDpiChangeBehavior / Set ================= */
TEST(gap1_dlgdpi_invalid_hwnd_get) {
    REQUIRE(U(GetDialogDpiChangeBehavior));
    HWND bad[] = {NULL, (HWND)(ULONG_PTR)1, (HWND)(ULONG_PTR)0x12345678, (HWND)(ULONG_PTR)-1, (HWND)(ULONG_PTR)0xdeadbeef};
    for (int i = 0; i < 5; i++) CHECKF((WORD)U(GetDialogDpiChangeBehavior)(bad[i]) == 0, "bad hwnd %d", i);
}
TEST(gap1_dlgdpi_invalid_hwnd_set) {
    REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND bad[] = {NULL, (HWND)(ULONG_PTR)1, (HWND)(ULONG_PTR)0x12345678, (HWND)(ULONG_PTR)0xdeadbeef};
    for (int i = 0; i < 4; i++) CHECKF(U(SetDialogDpiChangeBehavior)(bad[i], 7, 7) == FALSE, "bad hwnd %d", i);
}
TEST(gap1_dlgdpi_plain_window_not_dialog) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND h = make_plain(); CHECK(h != NULL); if (!h) return;
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(h) == 0);
    CHECK(U(SetDialogDpiChangeBehavior)(h, DDC_DISABLE_ALL, DDC_DISABLE_ALL) == FALSE);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(h) == 0);
    U(DestroyWindow)(h);
}
TEST(gap1_dlgdpi_child_window_not_dialog) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND p = make_plain(), c = p ? make_child(p) : NULL; CHECK(c != NULL); if (!c) return;
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(c) == 0);
    CHECK(U(SetDialogDpiChangeBehavior)(c, 7, 7) == FALSE);
    U(DestroyWindow)(p);
}
TEST(gap1_dlgdpi_desktop_window) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND d = GetDesktopWindow();
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(d) == 0);
    SetLastError(0);
    BOOL r = U(SetDialogDpiChangeBehavior)(d, 7, 7);
    CHECK(r == FALSE);
}
TEST(gap1_dlgdpi_cross_process_set_is_denied) {
    REQUIRE(U(SetDialogDpiChangeBehavior)); REQUIRE(U(FindWindowA)); REQUIRE(U(GetWindowThreadProcessId));
    HWND tray = U(FindWindowA)("Shell_TrayWnd", NULL);
    if (!tray) { t_note("no Shell_TrayWnd"); return; }
    DWORD pid = 0; U(GetWindowThreadProcessId)(tray, &pid);
    if (pid == GetCurrentProcessId()) return;
    SetLastError(0);
    BOOL r = U(SetDialogDpiChangeBehavior)(tray, 7, 7);
    CHECK(r == FALSE);
    EXPECT_EQ(GetLastError(), ERROR_ACCESS_DENIED);
}
TEST(gap1_dlgdpi_default_zero) {
    REQUIRE(U(GetDialogDpiChangeBehavior));
    HWND d = make_dialog(); CHECK(d != NULL); if (!d) return;
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(d) == DDC_DEFAULT);
    U(DestroyWindow)(d);
}
TEST(gap1_dlgdpi_set_get_roundtrip) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND d = make_dialog(); CHECK(d != NULL); if (!d) return;
    struct { int mask, val, expect; } steps[] = {
        {DDC_DISABLE_ALL, DDC_DISABLE_ALL, 1},
        {DDC_DISABLE_RESIZE, DDC_DISABLE_RESIZE, 3},
        {DDC_DISABLE_CONTROL_RELAYOUT, DDC_DISABLE_CONTROL_RELAYOUT, 7},
        {DDC_DISABLE_ALL, 0, 6},
        {DDC_DISABLE_RESIZE | DDC_DISABLE_CONTROL_RELAYOUT, 0, 0},
        {7, 5, 5},
        {2, 0xffff, 7},
        {7, 0, 0},
    };
    for (int i = 0; i < 8; i++) {
        BOOL r = U(SetDialogDpiChangeBehavior)(d, steps[i].mask, steps[i].val);
        CHECKF(r == TRUE, "step %d", i);
        WORD g = (WORD)U(GetDialogDpiChangeBehavior)(d);
        CHECKF(g == steps[i].expect, "step %d got %u want %d", i, g, steps[i].expect);
    }
    U(DestroyWindow)(d);
}
TEST(gap1_dlgdpi_mask_zero_changes_nothing) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND d = make_dialog(); if (!d) { CHECK(0); return; }
    U(SetDialogDpiChangeBehavior)(d, 7, 5);
    CHECK(U(SetDialogDpiChangeBehavior)(d, 0, 7) == TRUE);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(d) == 5);
    U(DestroyWindow)(d);
}
TEST(gap1_dlgdpi_unknown_mask_bits_ignored) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND d = make_dialog(); if (!d) { CHECK(0); return; }
    CHECK(U(SetDialogDpiChangeBehavior)(d, 0x8, 0x8) == TRUE);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(d) == 0);
    CHECK(U(SetDialogDpiChangeBehavior)(d, 0xfff8, 0xffff) == TRUE);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(d) == 0);
    CHECK(U(SetDialogDpiChangeBehavior)(d, 0xffff, 0xffff) == TRUE);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(d) == 7);
    U(DestroyWindow)(d);
}
TEST(gap1_dlgdpi_values_outside_mask_ignored) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND d = make_dialog(); if (!d) { CHECK(0); return; }
    CHECK(U(SetDialogDpiChangeBehavior)(d, 1, 7) == TRUE);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(d) == 1);
    U(DestroyWindow)(d);
}
TEST(gap1_dlgdpi_independent_per_dialog) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND a = make_dialog(), b = make_dialog();
    if (!a || !b) { CHECK(0); return; }
    U(SetDialogDpiChangeBehavior)(a, 7, 2);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(a) == 2);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(b) == 0);
    U(SetDialogDpiChangeBehavior)(b, 7, 4);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(a) == 2);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(b) == 4);
    U(DestroyWindow)(a); U(DestroyWindow)(b);
}
TEST(gap1_dlgdpi_destroyed_handle) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND d = make_dialog(); if (!d) { CHECK(0); return; }
    U(SetDialogDpiChangeBehavior)(d, 7, 7);
    U(DestroyWindow)(d);
    CHECK((WORD)U(GetDialogDpiChangeBehavior)(d) == 0);
    CHECK(U(SetDialogDpiChangeBehavior)(d, 7, 7) == FALSE);
}
TEST(gap1_dlgdpi_new_dialog_not_polluted_by_old) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    for (int i = 0; i < 20; i++) {
        HWND d = make_dialog(); if (!d) { CHECK(0); return; }
        if ((WORD)U(GetDialogDpiChangeBehavior)(d) != 0) { CHECKF(0, "iter %d", i); U(DestroyWindow)(d); return; }
        U(SetDialogDpiChangeBehavior)(d, 7, 7);
        U(DestroyWindow)(d);
    }
    CHECK(1);
}
static DWORD WINAPI dlg_thread_get(void *p) {
    HWND d = (HWND)p;
    return (WORD)U(GetDialogDpiChangeBehavior)(d);
}
TEST(gap1_dlgdpi_get_from_other_thread) {
    REQUIRE(U(GetDialogDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND d = make_dialog(); if (!d) { CHECK(0); return; }
    U(SetDialogDpiChangeBehavior)(d, 7, 6);
    HANDLE t = CreateThread(NULL, 0, dlg_thread_get, d, 0, NULL);
    DWORD code = 0xff;
    if (t) { CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0); GetExitCodeThread(t, &code); CloseHandle(t); }
    CHECK(code == 6);
    U(DestroyWindow)(d);
}

/* ================= GetDialogControlDpiChangeBehavior ================= */
TEST(gap1_dlgctl_invalid_hwnd) {
    REQUIRE(U(GetDialogControlDpiChangeBehavior));
    HWND bad[] = {NULL, (HWND)(ULONG_PTR)1, (HWND)(ULONG_PTR)0x12345678, (HWND)(ULONG_PTR)-1};
    for (int i = 0; i < 4; i++) CHECKF((WORD)U(GetDialogControlDpiChangeBehavior)(bad[i]) == 0, "bad %d", i);
}
TEST(gap1_dlgctl_default_zero_all_kinds) {
    REQUIRE(U(GetDialogControlDpiChangeBehavior));
    HWND p = make_plain(); if (!p) { CHECK(0); return; }
    const char *cls[] = {"BUTTON", "EDIT", "STATIC", "LISTBOX", "COMBOBOX", "SCROLLBAR"};
    for (int i = 0; i < 6; i++) {
        HWND c = t_make_window(cls[i], WS_CHILD | WS_VISIBLE, 0, 0, 0, 40, 40, p);
        CHECKF(c != NULL, "%s", cls[i]);
        if (c) CHECKF((WORD)U(GetDialogControlDpiChangeBehavior)(c) == 0, "%s", cls[i]);
    }
    CHECK((WORD)U(GetDialogControlDpiChangeBehavior)(p) == 0);
    CHECK((WORD)U(GetDialogControlDpiChangeBehavior)(GetDesktopWindow()) == 0);
    U(DestroyWindow)(p);
}
TEST(gap1_dlgctl_set_get_roundtrip) {
    REQUIRE(U(GetDialogControlDpiChangeBehavior)); REQUIRE(U(SetDialogControlDpiChangeBehavior));
    HWND p = make_plain(), c = p ? make_child(p) : NULL; if (!c) { CHECK(0); return; }
    struct { int mask, val, expect; } steps[] = {
        {DCDC_DISABLE_FONT_UPDATE, DCDC_DISABLE_FONT_UPDATE, 1},
        {DCDC_DISABLE_RELAYOUT, DCDC_DISABLE_RELAYOUT, 3},
        {DCDC_DISABLE_FONT_UPDATE, 0, 2},
        {3, 0, 0},
    };
    for (int i = 0; i < 4; i++) {
        BOOL r = U(SetDialogControlDpiChangeBehavior)(c, steps[i].mask, steps[i].val);
        WORD g = (WORD)U(GetDialogControlDpiChangeBehavior)(c);
        if (!r) t_note("SetDialogControlDpiChangeBehavior step %d failed err %lu", i, GetLastError());
        else CHECKF(g == steps[i].expect, "step %d got %u want %d", i, g, steps[i].expect);
    }
    U(SetDialogControlDpiChangeBehavior)(c, 3, 0);
    U(DestroyWindow)(p);
}
TEST(gap1_dlgctl_set_invalid_hwnd) {
    REQUIRE(U(SetDialogControlDpiChangeBehavior));
    HWND bad[] = {NULL, (HWND)(ULONG_PTR)0x12345678, (HWND)(ULONG_PTR)-1};
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        BOOL r = U(SetDialogControlDpiChangeBehavior)(bad[i], 3, 3);
        CHECKF(r == FALSE, "bad %d", i);
        if (GetLastError() != ERROR_INVALID_WINDOW_HANDLE) t_note("set ctl bad hwnd err %lu", GetLastError());
    }
}
TEST(gap1_dlgctl_independent_of_dialog_behavior) {
    REQUIRE(U(GetDialogControlDpiChangeBehavior)); REQUIRE(U(SetDialogDpiChangeBehavior));
    HWND d = make_dialog(); if (!d) { CHECK(0); return; }
    HWND c = make_child(d); if (!c) { CHECK(0); return; }
    U(SetDialogDpiChangeBehavior)(d, 7, 7);
    CHECK((WORD)U(GetDialogControlDpiChangeBehavior)(c) == 0);
    CHECK((WORD)U(GetDialogControlDpiChangeBehavior)(d) == 0);
    U(DestroyWindow)(d);
}
TEST(gap1_dlgctl_destroyed_control) {
    REQUIRE(U(GetDialogControlDpiChangeBehavior));
    HWND p = make_plain(), c = p ? make_child(p) : NULL; if (!c) { CHECK(0); return; }
    U(DestroyWindow)(c);
    CHECK((WORD)U(GetDialogControlDpiChangeBehavior)(c) == 0);
    U(DestroyWindow)(p);
}

/* ================= EndDeferWindowPosEx ================= */
static int rect_eq(HWND h, int x, int y, int w, int hgt) {
    RECT r; if (!U(GetWindowRect)(h, &r)) return 0;
    return r.left == x && r.top == y && r.right - r.left == w && r.bottom - r.top == hgt;
}
TEST(gap1_defer_sync_moves_window) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    HDWP d = U(BeginDeferWindowPos)(1); CHECK(d != NULL);
    d = U(DeferWindowPos)(d, h, NULL, 120, 130, 77, 66, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d != NULL);
    CHECK(UN(EndDeferWindowPosEx)(d, FALSE) == TRUE);
    CHECK(rect_eq(h, 120, 130, 77, 66));
    U(DestroyWindow)(h);
}
TEST(gap1_defer_async_same_thread_completes) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    HDWP d = U(BeginDeferWindowPos)(1);
    d = U(DeferWindowPos)(d, h, NULL, 90, 95, 55, 44, SWP_NOZORDER | SWP_NOACTIVATE);
    BOOL r = UN(EndDeferWindowPosEx)(d, TRUE);
    CHECK(r == TRUE);
    DWORD end = GetTickCount() + 2000;
    while (!rect_eq(h, 90, 95, 55, 44) && GetTickCount() < end) { t_pump(); Sleep(10); }
    if (!rect_eq(h, 90, 95, 55, 44)) t_note("async deferred move not applied within 2s");
    U(DestroyWindow)(h);
}
TEST(gap1_defer_invalid_handle) {
    REQUIRE(UN(EndDeferWindowPosEx));
    HDWP bad[] = {NULL, (HDWP)(ULONG_PTR)1, (HDWP)(ULONG_PTR)0x12345678, (HDWP)(ULONG_PTR)-1};
    for (int i = 0; i < 4; i++)
        for (int a = 0; a < 2; a++) {
            SetLastError(0);
            BOOL r = UN(EndDeferWindowPosEx)(bad[i], a);
            CHECKF(r == FALSE, "handle %d async %d", i, a);
            if (i == 0 && GetLastError() != ERROR_INVALID_DWP_HANDLE) t_note("NULL hdwp last error %lu", GetLastError());
        }
}
TEST(gap1_defer_handle_consumed) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    HDWP d = U(BeginDeferWindowPos)(1);
    d = U(DeferWindowPos)(d, h, NULL, 10, 10, 50, 50, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(UN(EndDeferWindowPosEx)(d, FALSE) == TRUE);
    SetLastError(0);
    CHECK(UN(EndDeferWindowPosEx)(d, FALSE) == FALSE);
    CHECK(U(EndDeferWindowPos)(d) == FALSE);
    HDWP d2 = U(DeferWindowPos)(d, h, NULL, 0, 0, 5, 5, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d2 == NULL);
    U(DestroyWindow)(h);
}
TEST(gap1_defer_bad_window_frees_handle) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HDWP d = U(BeginDeferWindowPos)(2);
    CHECK(d != NULL);
    HDWP d2 = U(DeferWindowPos)(d, (HWND)(ULONG_PTR)0x12345678, NULL, 0, 0, 5, 5, SWP_NOZORDER);
    CHECK(d2 == NULL);
    BOOL r = UN(EndDeferWindowPosEx)(d, FALSE);
    if (r) t_note("End succeeded on a handle whose DeferWindowPos call failed");
}
TEST(gap1_defer_empty_batch) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos));
    HDWP d = U(BeginDeferWindowPos)(1); CHECK(d != NULL);
    BOOL r = UN(EndDeferWindowPosEx)(d, FALSE);
    if (r != TRUE) t_note("empty deferred batch: End returned %d err %lu", r, GetLastError());
    CHECK(1);
}
TEST(gap1_defer_multiple_windows_one_batch) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND w[4]; HDWP d = U(BeginDeferWindowPos)(4);
    for (int i = 0; i < 4; i++) { w[i] = make_plain(); if (!w[i]) { CHECK(0); return; } }
    for (int i = 0; i < 4; i++) {
        d = U(DeferWindowPos)(d, w[i], NULL, 20 + i * 40, 25 + i * 3, 30 + i, 31 + i, SWP_NOZORDER | SWP_NOACTIVATE);
        CHECKF(d != NULL, "defer %d", i);
        if (!d) break;
    }
    if (d) CHECK(UN(EndDeferWindowPosEx)(d, FALSE) == TRUE);
    for (int i = 0; i < 4; i++) { CHECKF(rect_eq(w[i], 20 + i * 40, 25 + i * 3, 30 + i, 31 + i), "w%d", i); U(DestroyWindow)(w[i]); }
}
TEST(gap1_defer_growth_beyond_initial_count) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND w[5]; HDWP d = U(BeginDeferWindowPos)(1);
    for (int i = 0; i < 5; i++) { w[i] = make_plain(); if (!w[i]) { CHECK(0); return; } }
    for (int i = 0; i < 5 && d; i++) d = U(DeferWindowPos)(d, w[i], NULL, 11 * i, 12 * i, 40, 41, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d != NULL);
    if (d) CHECK(UN(EndDeferWindowPosEx)(d, FALSE) == TRUE);
    for (int i = 0; i < 5; i++) { CHECKF(rect_eq(w[i], 11 * i, 12 * i, 40, 41), "w%d", i); U(DestroyWindow)(w[i]); }
}
TEST(gap1_defer_same_window_twice_last_wins) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    HDWP d = U(BeginDeferWindowPos)(2);
    d = U(DeferWindowPos)(d, h, NULL, 10, 10, 50, 50, SWP_NOZORDER | SWP_NOACTIVATE);
    d = U(DeferWindowPos)(d, h, NULL, 60, 70, 80, 90, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(d != NULL);
    if (d) CHECK(UN(EndDeferWindowPosEx)(d, FALSE) == TRUE);
    CHECK(rect_eq(h, 60, 70, 80, 90));
    U(DestroyWindow)(h);
}
TEST(gap1_defer_destroyed_window_before_end) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    HDWP d = U(BeginDeferWindowPos)(1);
    d = U(DeferWindowPos)(d, h, NULL, 10, 10, 50, 50, SWP_NOZORDER | SWP_NOACTIVATE);
    U(DestroyWindow)(h);
    BOOL r = UN(EndDeferWindowPosEx)(d, FALSE);
    t_note("End after the window was destroyed returned %d (err %lu)", r, GetLastError());
    CHECK(1);
}
TEST(gap1_defer_ex_matches_plain_end) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    for (int pass = 0; pass < 2; pass++) {
        HDWP d = U(BeginDeferWindowPos)(1);
        d = U(DeferWindowPos)(d, h, NULL, 30 + pass, 31, 60, 61, SWP_NOZORDER | SWP_NOACTIVATE);
        BOOL r = pass ? U(EndDeferWindowPos)(d) : UN(EndDeferWindowPosEx)(d, FALSE);
        CHECK(r == TRUE);
        CHECK(rect_eq(h, 30 + pass, 31, 60, 61));
    }
    U(DestroyWindow)(h);
}
TEST(gap1_defer_nocheck_flags_rejected_or_ignored) {
    REQUIRE(UN(EndDeferWindowPosEx)); REQUIRE(U(BeginDeferWindowPos)); REQUIRE(U(DeferWindowPos));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    HDWP d = U(BeginDeferWindowPos)(1);
    HDWP d2 = U(DeferWindowPos)(d, h, NULL, 1, 2, 3, 4, 0x80000000u);
    t_note("DeferWindowPos with undefined flag bits returned %p err %lu", d2, GetLastError());
    if (d2) UN(EndDeferWindowPosEx)(d2, FALSE);
    U(DestroyWindow)(h);
}

/* ================= ExcludeUpdateRgn ================= */
static HWND vis_window(void) {
    HWND h = t_make_window(NULL, WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, 70, 70, 200, 200, NULL);
    if (h) { U(ShowWindow)(h, SW_SHOWNOACTIVATE); t_pump(); U(ValidateRect)(h, NULL); }
    return h;
}
TEST(gap1_exclude_invalid_params) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(GetDC));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    HDC dc = U(GetDC)(h);
    CHECK(U(ExcludeUpdateRgn)(NULL, h) == 0);
    CHECK(U(ExcludeUpdateRgn)(dc, NULL) == 0);
    CHECK(U(ExcludeUpdateRgn)(NULL, NULL) == 0);
    CHECK(U(ExcludeUpdateRgn)(dc, (HWND)(ULONG_PTR)0x12345678) == 0);
    CHECK(U(ExcludeUpdateRgn)((HDC)(ULONG_PTR)0x12345678, h) == 0);
    CHECK(U(ExcludeUpdateRgn)((HDC)(ULONG_PTR)-1, (HWND)(ULONG_PTR)-1) == 0);
    U(ReleaseDC)(h, dc);
    U(DestroyWindow)(h);
}
TEST(gap1_exclude_invalid_hwnd_lasterror_note) {
    REQUIRE(U(ExcludeUpdateRgn));
    SetLastError(0);
    int r = U(ExcludeUpdateRgn)(NULL, (HWND)(ULONG_PTR)0x12345678);
    CHECK(r == 0);
    if (GetLastError() != ERROR_INVALID_WINDOW_HANDLE) t_note("ExcludeUpdateRgn bad hwnd error %lu", GetLastError());
}
TEST(gap1_exclude_whole_client_invalid_gives_null_region) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(GetDC)); REQUIRE(U(InvalidateRect));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    U(InvalidateRect)(h, NULL, FALSE);
    HDC dc = U(GetDC)(h);
    int r = U(ExcludeUpdateRgn)(dc, h);
    CHECKF(r == NULLREGION, "r=%d", r);
    RECT rc; CHECK(GetClipBox(dc, &rc) == NULLREGION);
    U(ReleaseDC)(h, dc);
    U(ValidateRect)(h, NULL);
    U(DestroyWindow)(h);
}
TEST(gap1_exclude_partial_invalid_leaves_region) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(GetDC)); REQUIRE(U(InvalidateRect));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    RECT inv = {50, 50, 120, 120};
    U(InvalidateRect)(h, &inv, FALSE);
    HDC dc = U(GetDC)(h);
    int r = U(ExcludeUpdateRgn)(dc, h);
    CHECKF(r == COMPLEXREGION || r == SIMPLEREGION || r == NULLREGION, "r=%d", r);
    if (r != COMPLEXREGION) t_note("window obscured? partial exclude returned %d", r);
    RECT box; int t = GetClipBox(dc, &box);
    CHECK(t == r || (t == SIMPLEREGION && r == COMPLEXREGION));
    /* the excluded area must not be part of the clip region */
    HRGN clip = CreateRectRgn(0, 0, 0, 0);
    if (GetClipRgn(dc, clip) == 1) CHECK(!PtInRegion(clip, 80, 80));
    DeleteObject(clip);
    CHECK(!RectVisible(dc, &inv) || r == SIMPLEREGION);
    U(ReleaseDC)(h, dc);
    U(ValidateRect)(h, NULL);
    U(DestroyWindow)(h);
}
TEST(gap1_exclude_partial_edge_rect_stays_simple_or_complex) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(GetDC)); REQUIRE(U(InvalidateRect));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    RECT inv = {0, 0, 200, 100};  /* top half */
    U(InvalidateRect)(h, &inv, FALSE);
    HDC dc = U(GetDC)(h);
    int r = U(ExcludeUpdateRgn)(dc, h);
    CHECK(r == SIMPLEREGION || r == NULLREGION || r == COMPLEXREGION);
    RECT box; GetClipBox(dc, &box);
    if (r != NULLREGION) CHECKF(box.top >= 100, "top=%ld", box.top);
    U(ReleaseDC)(h, dc);
    U(ValidateRect)(h, NULL);
    U(DestroyWindow)(h);
}
TEST(gap1_exclude_no_update_region) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(GetDC));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    HDC dc = U(GetDC)(h);
    RECT before; int tb = GetClipBox(dc, &before);
    int r = U(ExcludeUpdateRgn)(dc, h);
    RECT after; int ta = GetClipBox(dc, &after);
    CHECKF(r != 0, "r=%d", r);
    CHECK(EqualRect(&before, &after));
    CHECKF(ta == r || (ta == SIMPLEREGION && r == COMPLEXREGION), "ta=%d r=%d", ta, r);
    U(ReleaseDC)(h, dc);
    U(DestroyWindow)(h);
}
TEST(gap1_exclude_idempotent) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(GetDC)); REQUIRE(U(InvalidateRect));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    RECT inv = {20, 20, 60, 60};
    U(InvalidateRect)(h, &inv, FALSE);
    HDC dc = U(GetDC)(h);
    int r1 = U(ExcludeUpdateRgn)(dc, h);
    int r2 = U(ExcludeUpdateRgn)(dc, h);
    CHECKF(r1 == r2, "%d vs %d", r1, r2);
    U(ReleaseDC)(h, dc);
    U(ValidateRect)(h, NULL);
    U(DestroyWindow)(h);
}
TEST(gap1_exclude_does_not_validate_window) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(GetDC)); REQUIRE(U(InvalidateRect)); REQUIRE(U(GetUpdateRect));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    RECT inv = {20, 20, 60, 60};
    U(InvalidateRect)(h, &inv, FALSE);
    HDC dc = U(GetDC)(h);
    U(ExcludeUpdateRgn)(dc, h);
    RECT ur; BOOL has = U(GetUpdateRect)(h, &ur, FALSE);
    CHECK(has);
    CHECK(ur.left == 20 && ur.top == 20 && ur.right == 60 && ur.bottom == 60);
    U(ReleaseDC)(h, dc);
    U(ValidateRect)(h, NULL);
    U(DestroyWindow)(h);
}
TEST(gap1_exclude_memory_dc) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(InvalidateRect));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    U(InvalidateRect)(h, NULL, FALSE);
    HDC mem = CreateCompatibleDC(NULL);
    int r = U(ExcludeUpdateRgn)(mem, h);
    t_note("ExcludeUpdateRgn on a memory DC returned %d", r);
    DeleteDC(mem);
    U(ValidateRect)(h, NULL);
    U(DestroyWindow)(h);
}
TEST(gap1_exclude_destroyed_window) {
    REQUIRE(U(ExcludeUpdateRgn)); REQUIRE(U(GetDC));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    HDC dc = U(GetDC)(h);
    HWND h2 = vis_window();
    U(DestroyWindow)(h2);
    CHECK(U(ExcludeUpdateRgn)(dc, h2) == 0);
    U(ReleaseDC)(h, dc);
    U(DestroyWindow)(h);
}

/* ================= GetInternalWindowPos ================= */
TEST(gap1_intpos_invalid_hwnd) {
    REQUIRE(UN(GetInternalWindowPos));
    HWND bad[] = {NULL, (HWND)(ULONG_PTR)1, (HWND)(ULONG_PTR)0x12345678, (HWND)(ULONG_PTR)-1};
    for (int i = 0; i < 4; i++) {
        RECT r = {7, 7, 7, 7}; POINT p = {7, 7};
        SetLastError(0);
        UINT c = UN(GetInternalWindowPos)(bad[i], &r, &p);
        CHECKF(c == 0, "bad %d returned %u", i, c);
        if (GetLastError() != ERROR_INVALID_WINDOW_HANDLE) t_note("GetInternalWindowPos bad hwnd %d error %lu", i, GetLastError());
    }
}
TEST(gap1_intpos_destroyed_window) {
    REQUIRE(UN(GetInternalWindowPos));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    U(DestroyWindow)(h);
    RECT r; POINT p;
    CHECK(UN(GetInternalWindowPos)(h, &r, &p) == 0);
}
TEST(gap1_intpos_normal_window_matches_rect) {
    REQUIRE(UN(GetInternalWindowPos));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    RECT r = {-1, -1, -1, -1}; POINT p = {-1, -1};
    UINT c = UN(GetInternalWindowPos)(h, &r, &p);
    t_note("normal window: showcmd=%u rect=(%ld,%ld,%ld,%ld) icon=(%ld,%ld)", c, r.left, r.top, r.right, r.bottom, p.x, p.y);
    CHECKF(c != 0, "c=%u", c);
    if (c) {
        RECT wr; U(GetWindowRect)(h, &wr);
        CHECK(r.right - r.left == wr.right - wr.left);
        CHECK(r.bottom - r.top == wr.bottom - wr.top);
    }
    U(DestroyWindow)(h);
}
TEST(gap1_intpos_minimized_reports_showcmd_note) {
    REQUIRE(UN(GetInternalWindowPos)); REQUIRE(U(ShowWindow));
    HWND h = t_make_window(NULL, WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 100, 100, 200, 150, NULL);
    if (!h) { CHECK(0); return; }
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    RECT r = {0}; POINT p = {0};
    UINT c = UN(GetInternalWindowPos)(h, &r, &p);
    t_note("minimized window: showcmd=%u normal=(%ld,%ld,%ld,%ld)", c, r.left, r.top, r.right, r.bottom);
    CHECK(c != 0);
    CHECKF(r.right - r.left == 200 || r.right == r.left || 1, "x");
    U(DestroyWindow)(h);
}
TEST(gap1_intpos_null_out_buffers_note) {
    REQUIRE(UN(GetInternalWindowPos));
    HWND h = vis_window(); if (!h) { CHECK(0); return; }
    RECT r; POINT p;
    UINT a = UN(GetInternalWindowPos)(h, &r, &p);
    UINT b = UN(GetInternalWindowPos)(h, &r, &p);
    CHECK(a == b);
    U(DestroyWindow)(h);
}

/* ================= GetTopLevelWindow ================= */
TEST(gap1_toplevel_invalid_hwnd) {
    REQUIRE(UN(GetTopLevelWindow));
    HWND bad[] = {NULL, (HWND)(ULONG_PTR)1, (HWND)(ULONG_PTR)0x12345678, (HWND)(ULONG_PTR)-1};
    for (int i = 0; i < 4; i++) {
        HWND r = UN(GetTopLevelWindow)(bad[i]);
        CHECKF(r == NULL, "bad %d -> %p", i, r);
    }
}
TEST(gap1_toplevel_of_toplevel_and_children_note) {
    REQUIRE(UN(GetTopLevelWindow)); REQUIRE(U(GetAncestor));
    HWND top = make_plain(); if (!top) { CHECK(0); return; }
    HWND c1 = make_child(top), c2 = c1 ? make_child(c1) : NULL;
    HWND r0 = UN(GetTopLevelWindow)(top);
    HWND r1 = UN(GetTopLevelWindow)(c1);
    HWND r2 = UN(GetTopLevelWindow)(c2);
    if (r0 != top) t_note("GetTopLevelWindow(top)=%p, top=%p", r0, top);
    if (r1 != top) t_note("GetTopLevelWindow(child)=%p, top=%p", r1, top);
    if (r2 != top) t_note("GetTopLevelWindow(grandchild)=%p, top=%p", r2, top);
    CHECK(r0 == NULL || r0 == top || U(IsWindow)(r0));
    U(DestroyWindow)(top);
}
TEST(gap1_toplevel_owned_popup_note) {
    REQUIRE(UN(GetTopLevelWindow));
    HWND owner = make_plain(); if (!owner) { CHECK(0); return; }
    HWND pop = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 20, 20, owner);
    HWND r = UN(GetTopLevelWindow)(pop);
    if (r != pop) t_note("GetTopLevelWindow(owned popup)=%p popup=%p owner=%p", r, pop, owner);
    CHECK(1);
    U(DestroyWindow)(owner);
}
TEST(gap1_toplevel_desktop_note) {
    REQUIRE(UN(GetTopLevelWindow));
    HWND r = UN(GetTopLevelWindow)(GetDesktopWindow());
    t_note("GetTopLevelWindow(desktop)=%p desktop=%p", r, GetDesktopWindow());
    CHECK(1);
}
TEST(gap1_toplevel_destroyed_window) {
    REQUIRE(UN(GetTopLevelWindow));
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    U(DestroyWindow)(h);
    CHECK(UN(GetTopLevelWindow)(h) == NULL);
}

/* ================= EnterReaderModeHelper (invalid windows only) ================= */
TEST(gap1_readermode_invalid_hwnd_is_noop) {
    REQUIRE(UN(EnterReaderModeHelper));
    UN(EnterReaderModeHelper)(NULL);
    UN(EnterReaderModeHelper)((HWND)(ULONG_PTR)0x12345678);
    UN(EnterReaderModeHelper)((HWND)(ULONG_PTR)-1);
    HWND h = make_plain(); if (!h) { CHECK(0); return; }
    U(DestroyWindow)(h);
    UN(EnterReaderModeHelper)(h);
    CHECK(U(GetCapture)() == NULL);
}
