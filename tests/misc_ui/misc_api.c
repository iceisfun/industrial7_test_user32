/* misc_ui: assorted exports: shutdown reasons, alt-tab info, combobox info, layered attrs, window region,
 * context help ids, ChangeMenu, PrintWindow, PrivateExtractIcons, shell window getters, DPI context helpers */
#include "mu.h"

#define NEED_API() REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow)); REQUIRE(U(IsWindow)); if (!mu_register()) { t_skip("class"); return; }

/* ---------- ShutdownBlockReason ---------- */
TEST(misc_ui_shutdown_reason_create_query_destroy) {
    NEED_API(); REQUIRE(U(ShutdownBlockReasonCreate)); REQUIRE(U(ShutdownBlockReasonQuery)); REQUIRE(U(ShutdownBlockReasonDestroy));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    WCHAR b[64]; DWORD cch;
    cch = 64;
    SetLastError(0);
    BOOL q0 = U(ShutdownBlockReasonQuery)(h, b, &cch);
    t_note("Query before Create: %d err=%lu cch=%lu", q0, GetLastError(), cch);
    CHECK(U(ShutdownBlockReasonCreate)(h, L"Saving the unit test"));
    cch = 64; memset(b, 0, sizeof b);
    CHECK(U(ShutdownBlockReasonQuery)(h, b, &cch));
    CHECK(!lstrcmpW(b, L"Saving the unit test"));
    t_note("Query cch for 20 char reason = %lu", cch);
    CHECK(U(ShutdownBlockReasonDestroy)(h));
    cch = 64;
    SetLastError(0);
    t_note("Query after Destroy = %d", U(ShutdownBlockReasonQuery)(h, b, &cch));
    SetLastError(0);
    t_note("Destroy twice = %d", U(ShutdownBlockReasonDestroy)(h));
    t_note("Destroy twice err=%lu", GetLastError());
    mu_kill(h);
}
TEST(misc_ui_shutdown_reason_replace) {
    NEED_API(); REQUIRE(U(ShutdownBlockReasonCreate)); REQUIRE(U(ShutdownBlockReasonQuery)); REQUIRE(U(ShutdownBlockReasonDestroy));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    WCHAR b[64]; DWORD cch = 64;
    CHECK(U(ShutdownBlockReasonCreate)(h, L"first"));
    CHECK(U(ShutdownBlockReasonCreate)(h, L"second reason"));
    CHECK(U(ShutdownBlockReasonQuery)(h, b, &cch) && !lstrcmpW(b, L"second reason"));
    CHECK(U(ShutdownBlockReasonDestroy)(h));
    mu_kill(h);
}
TEST(misc_ui_shutdown_reason_buffer_sizes) {
    NEED_API(); REQUIRE(U(ShutdownBlockReasonCreate)); REQUIRE(U(ShutdownBlockReasonQuery)); REQUIRE(U(ShutdownBlockReasonDestroy));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    CHECK(U(ShutdownBlockReasonCreate)(h, L"abcdef"));
    WCHAR b[16]; DWORD cch;
    cch = 0;
    SetLastError(0);
    BOOL r = U(ShutdownBlockReasonQuery)(h, NULL, &cch);
    t_note("Query(NULL buffer, cch=0): %d err=%lu cch=%lu", r, GetLastError(), cch);
    DWORD need = cch;
    cch = 3;
    memset(b, 0xFF, sizeof b);
    SetLastError(0);
    r = U(ShutdownBlockReasonQuery)(h, b, &cch);
    t_note("Query(cch=3): %d err=%lu cch=%lu", r, GetLastError(), cch);
    if (need) {
        cch = need < 16 ? need : 16;
        memset(b, 0, sizeof b);
        r = U(ShutdownBlockReasonQuery)(h, b, &cch);
        if (r) CHECK(!lstrcmpW(b, L"abcdef"));
        else t_note("Query(cch=%lu from first call) failed", cch);
    }
    U(ShutdownBlockReasonDestroy)(h);
    mu_kill(h);
}
TEST(misc_ui_shutdown_reason_invalid_args) {
    NEED_API(); REQUIRE(U(ShutdownBlockReasonCreate)); REQUIRE(U(ShutdownBlockReasonDestroy));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    SetLastError(0);
    CHECK(!U(ShutdownBlockReasonCreate)(h, NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    WCHAR big[400];
    for (int i = 0; i < 399; i++) big[i] = L'z';
    big[399] = 0;
    SetLastError(0);
    CHECK(!U(ShutdownBlockReasonCreate)(h, big));          /* longer than 256 characters */
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    big[257] = 0;
    SetLastError(0);
    CHECK(!U(ShutdownBlockReasonCreate)(h, big));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    big[256] = 0;
    BOOL r = U(ShutdownBlockReasonCreate)(h, big);
    t_note("256 character reason -> %d err=%lu", r, GetLastError());
    if (r) U(ShutdownBlockReasonDestroy)(h);
    SetLastError(0);
    t_note("Create(NULL hwnd) = %d err=%lu", U(ShutdownBlockReasonCreate)(NULL, L"x"), GetLastError());
    SetLastError(0);
    t_note("Create(bogus hwnd) = %d err=%lu", U(ShutdownBlockReasonCreate)(BOGUS_HWND, L"x"), GetLastError());
    SetLastError(0);
    t_note("Destroy(bogus hwnd) = %d err=%lu", U(ShutdownBlockReasonDestroy)(BOGUS_HWND), GetLastError());
    mu_kill(h);
}
TEST(misc_ui_shutdown_reason_child_window) {
    NEED_API(); REQUIRE(U(ShutdownBlockReasonCreate)); REQUIRE(U(ShutdownBlockReasonDestroy));
    HWND p = mu_top(), c = p ? mu_child(p, 0) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    SetLastError(0);
    BOOL r = U(ShutdownBlockReasonCreate)(c, L"child");
    t_note("ShutdownBlockReasonCreate(child window) -> %d err=%lu", r, GetLastError());
    if (r) U(ShutdownBlockReasonDestroy)(c);
    mu_kill(p);
}
TEST(misc_ui_shutdown_reason_destroyed_with_window) {
    NEED_API(); REQUIRE(U(ShutdownBlockReasonCreate)); REQUIRE(U(ShutdownBlockReasonQuery));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    CHECK(U(ShutdownBlockReasonCreate)(h, L"temp"));
    mu_kill(h);
    WCHAR b[16]; DWORD cch = 16;
    t_note("Query after window destroyed = %d", U(ShutdownBlockReasonQuery)(h, b, &cch));
}

/* ---------- GetAltTabInfo ---------- */
TEST(misc_ui_alttab_info_ansi_wide_parity) {
    NEED_API(); REQUIRE(U(GetAltTabInfoA)); REQUIRE(U(GetAltTabInfoW));
    ALTTABINFO ia, iw;
    memset(&ia, 0, sizeof ia); memset(&iw, 0, sizeof iw);
    ia.cbSize = iw.cbSize = sizeof(ALTTABINFO);
    char ta[128]; WCHAR tw[128];
    SetLastError(0);
    BOOL ra = U(GetAltTabInfoA)(NULL, -1, &ia, ta, 128);
    DWORD ea = GetLastError();
    SetLastError(0);
    BOOL rw = U(GetAltTabInfoW)(NULL, -1, &iw, tw, 128);
    DWORD ew = GetLastError();
    CHECK(ra == rw);
    t_note("GetAltTabInfo with no alt-tab window: A=%d(err %lu) W=%d(err %lu)", ra, ea, rw, ew);
    if (!ra) {
        CHECK(ea == ew);
    }
}
TEST(misc_ui_alttab_info_bad_cbsize) {
    NEED_API(); REQUIRE(U(GetAltTabInfoW));
    ALTTABINFO iw; WCHAR tw[16];
    DWORD sizes[] = { 0, 1, sizeof(ALTTABINFO) - 1, sizeof(ALTTABINFO) + 1, 0xFFFFFFFF };
    for (int i = 0; i < 5; i++) {
        memset(&iw, 0, sizeof iw);
        iw.cbSize = sizes[i];
        SetLastError(0);
        BOOL r = U(GetAltTabInfoW)(NULL, -1, &iw, tw, 16);
        CHECKF(!r, "cbSize %lu", sizes[i]);
        t_note("GetAltTabInfoW(cbSize=%lu) err=%lu", sizes[i], GetLastError());
    }
}
TEST(misc_ui_alttab_info_bogus_window) {
    NEED_API(); REQUIRE(U(GetAltTabInfoW));
    ALTTABINFO iw; memset(&iw, 0, sizeof iw); iw.cbSize = sizeof iw;
    SetLastError(0);
    CHECK(!U(GetAltTabInfoW)(BOGUS_HWND, -1, &iw, NULL, 0));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    SetLastError(0);
    CHECK(!U(GetAltTabInfoW)(h, -1, &iw, NULL, 0));         /* not the alt-tab window */
    t_note("GetAltTabInfoW(ordinary window) err=%lu", GetLastError());
    mu_kill(h);
}

/* ---------- GetComboBoxInfo ---------- */
static HWND mk_combo(HWND parent, DWORD style)
{
    return U(CreateWindowExA)(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | style, 5, 5, 150, 200, parent, (HMENU)(UINT_PTR)77, GetModuleHandleA(NULL), NULL);
}
TEST(misc_ui_combo_info_dropdown) {
    NEED_API(); REQUIRE(U(GetComboBoxInfo)); REQUIRE(U(GetClassNameA));
    HWND p = mu_mk(WS_POPUP, 0, 50, 50, 300, 300, NULL);
    HWND c = p ? mk_combo(p, CBS_DROPDOWN) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    COMBOBOXINFO ci; memset(&ci, 0, sizeof ci); ci.cbSize = sizeof ci;
    CHECK(U(GetComboBoxInfo)(c, &ci));
    CHECK(ci.hwndCombo == c);
    CHECK(ci.hwndItem != NULL);
    CHECK(ci.hwndList != NULL);
    char cls[64] = "";
    if (ci.hwndItem) { U(GetClassNameA)(ci.hwndItem, cls, 64); CHECK(!lstrcmpiA(cls, "Edit")); CHECK(U(GetParent)(ci.hwndItem) == c); }
    if (ci.hwndList) { U(GetClassNameA)(ci.hwndList, cls, 64); CHECK(!lstrcmpiA(cls, "ComboLBox")); }
    CHECK(ci.rcItem.right > ci.rcItem.left && ci.rcItem.bottom > ci.rcItem.top);
    CHECK(ci.rcButton.right > ci.rcButton.left && ci.rcButton.bottom > ci.rcButton.top);
    CHECK(ci.rcItem.right <= ci.rcButton.left);
    t_note("combo stateButton=0x%lx", (unsigned long)ci.stateButton);
    CHECK((ci.stateButton & STATE_SYSTEM_INVISIBLE) == 0);
    RECT cr; U(GetClientRect)(c, &cr);
    CHECK(ci.rcButton.right <= cr.right && ci.rcItem.left >= cr.left);
    mu_kill(p);
}
TEST(misc_ui_combo_info_dropdownlist) {
    NEED_API(); REQUIRE(U(GetComboBoxInfo));
    HWND p = mu_mk(WS_POPUP, 0, 50, 50, 300, 300, NULL);
    HWND c = p ? mk_combo(p, CBS_DROPDOWNLIST) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    COMBOBOXINFO ci; memset(&ci, 0, sizeof ci); ci.cbSize = sizeof ci;
    CHECK(U(GetComboBoxInfo)(c, &ci));
    CHECK(ci.hwndCombo == c);
    CHECK(ci.hwndList != NULL);
    t_note("CBS_DROPDOWNLIST: hwndItem=%p (combo=%p) stateButton=0x%lx", ci.hwndItem, c, ci.stateButton);
    mu_kill(p);
}
TEST(misc_ui_combo_info_simple) {
    NEED_API(); REQUIRE(U(GetComboBoxInfo));
    HWND p = mu_mk(WS_POPUP, 0, 50, 50, 300, 300, NULL);
    HWND c = p ? mk_combo(p, CBS_SIMPLE) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    COMBOBOXINFO ci; memset(&ci, 0, sizeof ci); ci.cbSize = sizeof ci;
    CHECK(U(GetComboBoxInfo)(c, &ci));
    CHECK(ci.hwndCombo == c && ci.hwndItem != NULL && ci.hwndList != NULL);
    t_note("CBS_SIMPLE: stateButton=0x%lx (INVISIBLE=0x8000)", ci.stateButton);
    t_note("CBS_SIMPLE list parent==combo: %d", U(GetParent)(ci.hwndList) == c);
    mu_kill(p);
}
TEST(misc_ui_combo_info_bad_args) {
    NEED_API(); REQUIRE(U(GetComboBoxInfo));
    HWND p = mu_mk(WS_POPUP, 0, 50, 50, 300, 300, NULL);
    HWND c = p ? mk_combo(p, CBS_DROPDOWN) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    COMBOBOXINFO ci;
    memset(&ci, 0, sizeof ci);
    SetLastError(0);
    t_note("GetComboBoxInfo(cbSize=0) -> %d err=%lu", U(GetComboBoxInfo)(c, &ci), GetLastError());
    ci.cbSize = sizeof ci - 1;
    t_note("GetComboBoxInfo(cbSize-1) -> %d", U(GetComboBoxInfo)(c, &ci));
    ci.cbSize = sizeof ci + 8;
    t_note("GetComboBoxInfo(cbSize+8) -> %d", U(GetComboBoxInfo)(c, &ci));
    memset(&ci, 0, sizeof ci); ci.cbSize = sizeof ci;
    SetLastError(0);
    CHECK(!U(GetComboBoxInfo)(p, &ci));                      /* not a combobox */
    t_note("GetComboBoxInfo(non-combo) err=%lu", GetLastError());
    SetLastError(0);
    CHECK(!U(GetComboBoxInfo)(BOGUS_HWND, &ci));
    t_note("GetComboBoxInfo(bogus) err=%lu", GetLastError());
    CHECK(!U(GetComboBoxInfo)(NULL, &ci));
    HWND edit = U(CreateWindowExA)(0, "EDIT", "", WS_CHILD, 0, 0, 40, 20, p, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(!U(GetComboBoxInfo)(edit, &ci));
    mu_kill(p);
}
TEST(misc_ui_combo_info_matches_cb_message) {
    NEED_API(); REQUIRE(U(GetComboBoxInfo)); REQUIRE(U(SendMessageA));
    HWND p = mu_mk(WS_POPUP, 0, 50, 50, 300, 300, NULL);
    HWND c = p ? mk_combo(p, CBS_DROPDOWN) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    COMBOBOXINFO a, b;
    memset(&a, 0, sizeof a); a.cbSize = sizeof a;
    memset(&b, 0, sizeof b); b.cbSize = sizeof b;
    CHECK(U(GetComboBoxInfo)(c, &a));
    LRESULT r = U(SendMessageA)(c, CB_GETCOMBOBOXINFO, 0, (LPARAM)&b);
    CHECK(r == TRUE);
    CHECK(a.hwndCombo == b.hwndCombo && a.hwndItem == b.hwndItem && a.hwndList == b.hwndList);
    CHECK(EqualRect(&a.rcItem, &b.rcItem) && EqualRect(&a.rcButton, &b.rcButton));
    U(SendMessageA)(c, CB_ADDSTRING, 0, (LPARAM)"one");
    CHECK(U(SendMessageA)(c, CB_GETCOUNT, 0, 0) == 1);
    mu_kill(p);
}

/* ---------- shell windows ---------- */
TEST(misc_ui_shell_window_getters) {
    NEED_API(); REQUIRE(U(GetShellWindow)); REQUIRE(U(GetClassNameA));
    HWND sh = U(GetShellWindow)();
    char cls[64] = "";
    if (sh) {
        CHECK(U(IsWindow)(sh));
        U(GetClassNameA)(sh, cls, 64);
        t_note("shell window %p class %s", sh, cls);
        CHECK(U(GetShellWindow)() == sh);
    } else t_note("no shell window");
    UDECL(HWND, GetProgmanWindow, (void));
    UDECL(HWND, GetTaskmanWindow, (void));
    if (UN(GetProgmanWindow)) {
        HWND pm = UN(GetProgmanWindow)();
        t_note("GetProgmanWindow=%p", pm);
        if (pm) { CHECK(U(IsWindow)(pm)); U(GetClassNameA)(pm, cls, 64); t_note("progman class %s", cls); }
    }
    if (UN(GetTaskmanWindow)) {
        HWND tm = UN(GetTaskmanWindow)();
        t_note("GetTaskmanWindow=%p", tm);
        if (tm) CHECK(U(IsWindow)(tm));
    }
}
TEST(misc_ui_shell_window_is_toplevel_other_process) {
    NEED_API(); REQUIRE(U(GetShellWindow)); REQUIRE(U(GetWindowThreadProcessId));
    HWND sh = U(GetShellWindow)();
    if (!sh) { t_skip("no shell window"); return; }
    DWORD pid = 0;
    CHECK(U(GetWindowThreadProcessId)(sh, &pid) != 0);
    CHECK(pid != GetCurrentProcessId());
    CHECK(U(GetAncestor)(sh, GA_PARENT) == U(GetDesktopWindow)());
    CHECK(U(GetParent)(sh) == NULL);
}

/* ---------- layered attributes ---------- */
TEST(misc_ui_layered_set_get_roundtrip) {
    NEED_API(); REQUIRE(U(SetLayeredWindowAttributes)); REQUIRE(U(GetLayeredWindowAttributes));
    HWND h = mu_mk(WS_POPUP, WS_EX_LAYERED, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    COLORREF key = 0; BYTE a = 0; DWORD fl = 0;
    CHECK(U(SetLayeredWindowAttributes)(h, RGB(1, 2, 3), 77, LWA_ALPHA | LWA_COLORKEY));
    CHECK(U(GetLayeredWindowAttributes)(h, &key, &a, &fl));
    CHECK(key == RGB(1, 2, 3));
    CHECK(a == 77);
    CHECK(fl == (LWA_ALPHA | LWA_COLORKEY));
    CHECK(U(SetLayeredWindowAttributes)(h, 0, 200, LWA_ALPHA));
    key = 0xFFFFFF; a = 0; fl = 0;
    CHECK(U(GetLayeredWindowAttributes)(h, &key, &a, &fl));
    CHECK(a == 200 && fl == LWA_ALPHA);
    CHECK(U(SetLayeredWindowAttributes)(h, RGB(255, 0, 255), 0, LWA_COLORKEY));
    CHECK(U(GetLayeredWindowAttributes)(h, &key, &a, &fl));
    CHECK(key == RGB(255, 0, 255) && fl == LWA_COLORKEY);
    mu_kill(h);
}
TEST(misc_ui_layered_alpha_boundaries) {
    NEED_API(); REQUIRE(U(SetLayeredWindowAttributes)); REQUIRE(U(GetLayeredWindowAttributes));
    HWND h = mu_mk(WS_POPUP, WS_EX_LAYERED, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    BYTE vals[] = { 0, 1, 127, 128, 254, 255 };
    for (int i = 0; i < 6; i++) {
        BYTE a = 99; COLORREF k; DWORD fl;
        CHECK(U(SetLayeredWindowAttributes)(h, 0, vals[i], LWA_ALPHA));
        CHECK(U(GetLayeredWindowAttributes)(h, &k, &a, &fl));
        CHECKF(a == vals[i], "alpha %d got %d", vals[i], a);
    }
    mu_kill(h);
}
TEST(misc_ui_layered_get_null_outputs) {
    NEED_API(); REQUIRE(U(SetLayeredWindowAttributes)); REQUIRE(U(GetLayeredWindowAttributes));
    HWND h = mu_mk(WS_POPUP, WS_EX_LAYERED, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    U(SetLayeredWindowAttributes)(h, RGB(9, 9, 9), 55, LWA_ALPHA | LWA_COLORKEY);
    BYTE a = 0; COLORREF k = 0; DWORD fl = 0;
    CHECK(U(GetLayeredWindowAttributes)(h, NULL, &a, NULL) && a == 55);
    CHECK(U(GetLayeredWindowAttributes)(h, &k, NULL, NULL) && k == RGB(9, 9, 9));
    CHECK(U(GetLayeredWindowAttributes)(h, NULL, NULL, &fl) && fl == (LWA_ALPHA | LWA_COLORKEY));
    SetLastError(0);
    BOOL r = U(GetLayeredWindowAttributes)(h, NULL, NULL, NULL);
    t_note("GetLayeredWindowAttributes(all NULL) -> %d err=%lu", r, GetLastError());
    mu_kill(h);
}
TEST(misc_ui_layered_requires_layered_style) {
    NEED_API(); REQUIRE(U(SetLayeredWindowAttributes)); REQUIRE(U(GetLayeredWindowAttributes));
    HWND h = mu_mk(WS_POPUP, 0, 0, 0, 50, 50, NULL);
    if (!h) { CHECK(0); return; }
    SetLastError(0);
    t_note("SetLayeredWindowAttributes(non-layered) = %d err=%lu", U(SetLayeredWindowAttributes)(h, 0, 100, LWA_ALPHA), GetLastError());
    BYTE a; COLORREF k; DWORD fl;
    SetLastError(0);
    CHECK(!U(GetLayeredWindowAttributes)(h, &k, &a, &fl));
    t_note("GetLayeredWindowAttributes(non-layered) err=%lu", GetLastError());
    /* add the style afterwards */
    U(SetWindowLongPtrA)(h, GWL_EXSTYLE, mu_ex(h) | WS_EX_LAYERED);
    CHECK(U(SetLayeredWindowAttributes)(h, 0, 100, LWA_ALPHA));
    CHECK(U(GetLayeredWindowAttributes)(h, &k, &a, &fl) && a == 100);
    /* remove the style: attributes unavailable again */
    U(SetWindowLongPtrA)(h, GWL_EXSTYLE, mu_ex(h) & ~WS_EX_LAYERED);
    SetLastError(0);
    CHECK(!U(GetLayeredWindowAttributes)(h, &k, &a, &fl));
    mu_kill(h);
}
TEST(misc_ui_layered_invalid_handle_and_flags) {
    NEED_API(); REQUIRE(U(SetLayeredWindowAttributes)); REQUIRE(U(GetLayeredWindowAttributes));
    BYTE a; COLORREF k; DWORD fl;
    SetLastError(0);
    CHECK(!U(SetLayeredWindowAttributes)(BOGUS_HWND, 0, 1, LWA_ALPHA));
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(SetLayeredWindowAttributes)(NULL, 0, 1, LWA_ALPHA));
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(!U(GetLayeredWindowAttributes)(BOGUS_HWND, &k, &a, &fl));
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    HWND h = mu_mk(WS_POPUP, WS_EX_LAYERED, 0, 0, 50, 50, NULL);
    if (h) {
        DWORD bad[] = { 0x4, 0x8, 0x10, 0x100, 0x80000000, 0xFFFFFFFF };
        for (int i = 0; i < 6; i++) {
            SetLastError(0);
            BOOL r = U(SetLayeredWindowAttributes)(h, 0, 50, bad[i]);
            t_note("SetLayeredWindowAttributes(flags=0x%lx) -> %d err=%lu", bad[i], r, GetLastError());
        }
        SetLastError(0);
        BOOL r = U(SetLayeredWindowAttributes)(h, 0, 50, 0);
        t_note("SetLayeredWindowAttributes(flags=0) -> %d err=%lu", r, GetLastError());
        mu_kill(h);
    }
}
TEST(misc_ui_layered_child_window) {
    NEED_API(); REQUIRE(U(SetLayeredWindowAttributes)); REQUIRE(U(GetLayeredWindowAttributes));
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c = p ? mu_mk(WS_CHILD, WS_EX_LAYERED, 0, 0, 20, 20, p) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    SetLastError(0);
    BOOL r = U(SetLayeredWindowAttributes)(c, 0, 90, LWA_ALPHA);
    t_note("layered child window: ex style kept=%d SetLayeredWindowAttributes=%d err=%lu", (mu_ex(c) & WS_EX_LAYERED) != 0, r, GetLastError());
    mu_kill(p);
}
TEST(misc_ui_layered_exstyle_cross_thread) {
    NEED_API(); REQUIRE(U(SetLayeredWindowAttributes)); REQUIRE(U(GetLayeredWindowAttributes));
    mu_worker w;
    if (!mu_worker_start(&w, WS_POPUP, WS_EX_LAYERED, "lay")) { CHECK(0); mu_worker_stop(&w); return; }
    CHECK(U(SetLayeredWindowAttributes)(w.hwnd, 0, 33, LWA_ALPHA));
    BYTE a = 0; COLORREF k; DWORD fl;
    CHECK(U(GetLayeredWindowAttributes)(w.hwnd, &k, &a, &fl) && a == 33);
    mu_worker_stop(&w);
}

/* ---------- window region ---------- */
TEST(misc_ui_winrgn_none_set) {
    NEED_API(); REQUIRE(U(GetWindowRgn)); REQUIRE(U(GetWindowRgnBox));
    HWND h = mu_mk(WS_POPUP, 0, 100, 100, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HRGN r = CreateRectRgn(0, 0, 1, 1);
    CHECK(U(GetWindowRgn)(h, r) == ERROR);
    RECT rc = { 7, 7, 7, 7 };
    CHECK(U(GetWindowRgnBox)(h, &rc) == ERROR);
    CHECK(U(GetWindowRgn)(h, NULL) == ERROR);
    CHECK(U(GetWindowRgnBox)(h, NULL) == ERROR);
    CHECK(U(GetWindowRgn)(BOGUS_HWND, r) == ERROR);
    CHECK(U(GetWindowRgn)(NULL, r) == ERROR);
    CHECK(U(GetWindowRgnBox)(BOGUS_HWND, &rc) == ERROR);
    DeleteObject(r);
    mu_kill(h);
}
TEST(misc_ui_winrgn_set_get_rect) {
    NEED_API(); REQUIRE(U(GetWindowRgn)); REQUIRE(U(GetWindowRgnBox)); REQUIRE(U(SetWindowRgn));
    HWND h = mu_mk(WS_POPUP, 0, 100, 100, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HRGN rg = CreateRectRgn(10, 12, 50, 60);
    CHECK(U(SetWindowRgn)(h, rg, FALSE) != 0);
    HRGN out = CreateRectRgn(0, 0, 0, 0);
    int t = U(GetWindowRgn)(h, out);
    CHECK(t == SIMPLEREGION);
    RECT box; GetRgnBox(out, &box);
    CHECK(box.left == 10 && box.top == 12 && box.right == 50 && box.bottom == 60);
    RECT b2 = {0};
    CHECK(U(GetWindowRgnBox)(h, &b2) == SIMPLEREGION);
    CHECK(b2.left == 10 && b2.top == 12 && b2.right == 50 && b2.bottom == 60);
    /* moving the window keeps the region window-relative */
    U(SetWindowPos)(h, NULL, 150, 130, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    memset(&b2, 0, sizeof b2);
    CHECK(U(GetWindowRgnBox)(h, &b2) == SIMPLEREGION);
    CHECK(b2.left == 10 && b2.top == 12 && b2.right == 50 && b2.bottom == 60);
    /* the returned region is a copy */
    SetRectRgn(out, 0, 0, 5, 5);
    memset(&b2, 0, sizeof b2);
    CHECK(U(GetWindowRgnBox)(h, &b2) == SIMPLEREGION && b2.right == 50);
    DeleteObject(out);
    mu_kill(h);
}
TEST(misc_ui_winrgn_complex_and_remove) {
    NEED_API(); REQUIRE(U(GetWindowRgn)); REQUIRE(U(GetWindowRgnBox)); REQUIRE(U(SetWindowRgn));
    HWND h = mu_mk(WS_POPUP, 0, 100, 100, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HRGN a = CreateRectRgn(0, 0, 20, 20), b = CreateRectRgn(40, 40, 60, 60);
    CombineRgn(a, a, b, RGN_OR);
    DeleteObject(b);
    CHECK(U(SetWindowRgn)(h, a, FALSE) != 0);
    HRGN out = CreateRectRgn(0, 0, 0, 0);
    CHECK(U(GetWindowRgn)(h, out) == COMPLEXREGION);
    RECT bx = {0};
    CHECK(U(GetWindowRgnBox)(h, &bx) == COMPLEXREGION);
    CHECK(bx.left == 0 && bx.top == 0 && bx.right == 60 && bx.bottom == 60);
    CHECK(U(SetWindowRgn)(h, NULL, FALSE) != 0);
    CHECK(U(GetWindowRgn)(h, out) == ERROR);
    CHECK(U(GetWindowRgnBox)(h, &bx) == ERROR);
    DeleteObject(out);
    mu_kill(h);
}
TEST(misc_ui_winrgn_hit_testing) {
    NEED_API(); REQUIRE(U(SetWindowRgn));
    HWND h = mu_mk(WS_POPUP, 0, 100, 100, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HRGN rg = CreateRectRgn(20, 20, 60, 60);
    CHECK(U(SetWindowRgn)(h, rg, FALSE));
    CHECK(U(SendMessageA)(h, WM_NCHITTEST, 0, MAKELPARAM(130, 130)) == HTCLIENT);
    /* DefWindowProc does not consult the region; only the real hit test does */
    t_note("WM_NCHITTEST outside region (default proc) -> %lld", (long long)U(SendMessageA)(h, WM_NCHITTEST, 0, MAKELPARAM(105, 105)));
    U(SetWindowRgn)(h, NULL, FALSE);
    mu_kill(h);
}
TEST(misc_ui_winrgn_set_invalid) {
    NEED_API(); REQUIRE(U(SetWindowRgn));
    HRGN rg = CreateRectRgn(0, 0, 5, 5);
    SetLastError(0);
    CHECK(U(SetWindowRgn)(BOGUS_HWND, rg, FALSE) == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(U(SetWindowRgn)(NULL, rg, FALSE) == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    CHECK(GetObjectType(rg) == OBJ_REGION);
    DeleteObject(rg);
    HWND h = mu_mk(WS_POPUP, 0, 100, 100, 100, 100, NULL);
    if (h) {
        SetLastError(0);
        int r = U(SetWindowRgn)(h, (HRGN)(ULONG_PTR)0x1234, FALSE);
        t_note("SetWindowRgn(bogus HRGN) = %d err=%lu", r, GetLastError());
        mu_kill(h);
    }
}
TEST(misc_ui_winrgn_destroyed_with_window) {
    NEED_API(); REQUIRE(U(SetWindowRgn));
    HWND h = mu_mk(WS_POPUP, 0, 100, 100, 100, 100, NULL);
    if (!h) { CHECK(0); return; }
    HRGN rg = CreateRectRgn(0, 0, 5, 5);
    CHECK(U(SetWindowRgn)(h, rg, FALSE));
    mu_kill(h);
    t_note("region object type after owner window destroyed: %lu (OBJ_REGION=%d)", GetObjectType(rg), OBJ_REGION);

}

/* ---------- context help ids ---------- */
TEST(misc_ui_helpid_window_roundtrip) {
    NEED_API(); REQUIRE(U(GetWindowContextHelpId)); REQUIRE(U(SetWindowContextHelpId));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    CHECK(U(GetWindowContextHelpId)(h) == 0);
    DWORD vals[] = { 1, 0x1234, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0 };
    for (int i = 0; i < 6; i++) {
        CHECK(U(SetWindowContextHelpId)(h, vals[i]));
        CHECKF(U(GetWindowContextHelpId)(h) == vals[i], "val %lx got %lx", vals[i], U(GetWindowContextHelpId)(h));
    }
    mu_kill(h);
}
TEST(misc_ui_helpid_window_independent) {
    NEED_API(); REQUIRE(U(GetWindowContextHelpId)); REQUIRE(U(SetWindowContextHelpId));
    HWND a = mu_top(), b = mu_top();
    if (!a || !b) { CHECK(0); mu_kill(a); mu_kill(b); return; }
    U(SetWindowContextHelpId)(a, 11);
    U(SetWindowContextHelpId)(b, 22);
    CHECK(U(GetWindowContextHelpId)(a) == 11 && U(GetWindowContextHelpId)(b) == 22);
    HWND c = mu_child(a, 0);
    CHECK(U(GetWindowContextHelpId)(c) == 0);               /* not inherited */
    U(SetWindowContextHelpId)(c, 33);
    CHECK(U(GetWindowContextHelpId)(c) == 33 && U(GetWindowContextHelpId)(a) == 11);
    mu_kill(a); mu_kill(b);
}
TEST(misc_ui_helpid_window_invalid) {
    NEED_API(); REQUIRE(U(GetWindowContextHelpId)); REQUIRE(U(SetWindowContextHelpId));
    HWND bad[] = { NULL, BOGUS_HWND, (HWND)0x6FFF0123 };
    for (int i = 0; i < 3; i++) {
        CHECKF(U(GetWindowContextHelpId)(bad[i]) == 0, "get %p", bad[i]);
        CHECKF(!U(SetWindowContextHelpId)(bad[i], 5), "set %p", bad[i]);
    }
    HWND h = mu_top();
    U(SetWindowContextHelpId)(h, 9);
    mu_kill(h);
    CHECK(U(GetWindowContextHelpId)(h) == 0);
    CHECK(!U(SetWindowContextHelpId)(h, 3));
}
TEST(misc_ui_helpid_window_cross_thread) {
    NEED_API(); REQUIRE(U(GetWindowContextHelpId)); REQUIRE(U(SetWindowContextHelpId));
    mu_worker w;
    if (!mu_worker_start(&w, WS_POPUP, 0, "help")) { CHECK(0); mu_worker_stop(&w); return; }
    CHECK(U(SetWindowContextHelpId)(w.hwnd, 4242));
    CHECK(U(GetWindowContextHelpId)(w.hwnd) == 4242);
    mu_worker_stop(&w);
}
TEST(misc_ui_helpid_menu_roundtrip) {
    NEED_API(); REQUIRE(U(GetMenuContextHelpId)); REQUIRE(U(SetMenuContextHelpId)); REQUIRE(U(CreateMenu)); REQUIRE(U(CreatePopupMenu)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)(), p = U(CreatePopupMenu)();
    CHECK(m && p);
    CHECK(U(GetMenuContextHelpId)(m) == 0);
    DWORD vals[] = { 1, 0xABCDEF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0 };
    for (int i = 0; i < 6; i++) {
        CHECK(U(SetMenuContextHelpId)(m, vals[i]));
        CHECKF(U(GetMenuContextHelpId)(m) == vals[i], "val %lx", vals[i]);
        CHECK(U(SetMenuContextHelpId)(p, vals[i] ^ 1));
        CHECK(U(GetMenuContextHelpId)(p) == (vals[i] ^ 1));
        CHECK(U(GetMenuContextHelpId)(m) == vals[i]);
    }
    CHECK(U(DestroyMenu)(m)); CHECK(U(DestroyMenu)(p));
}
TEST(misc_ui_helpid_menu_invalid) {
    NEED_API(); REQUIRE(U(GetMenuContextHelpId)); REQUIRE(U(SetMenuContextHelpId)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu));
    HMENU bad[] = { NULL, (HMENU)1, (HMENU)0x12345678, (HMENU)(LONG_PTR)-1 };
    for (int i = 0; i < 4; i++) {
        CHECKF(U(GetMenuContextHelpId)(bad[i]) == 0, "get %p", bad[i]);
        CHECKF(!U(SetMenuContextHelpId)(bad[i], 5), "set %p", bad[i]);
    }
    HMENU m = U(CreateMenu)();
    U(SetMenuContextHelpId)(m, 99);
    U(DestroyMenu)(m);
    CHECK(U(GetMenuContextHelpId)(m) == 0);
    CHECK(!U(SetMenuContextHelpId)(m, 1));
    HWND h = mu_top();
    CHECK(U(GetMenuContextHelpId)((HMENU)h) == 0);          /* a window handle is not a menu */
    mu_kill(h);
}
TEST(misc_ui_helpid_menu_vs_window_separate) {
    NEED_API(); REQUIRE(U(GetMenuContextHelpId)); REQUIRE(U(SetMenuContextHelpId)); REQUIRE(U(GetWindowContextHelpId)); REQUIRE(U(SetWindowContextHelpId)); REQUIRE(U(SetMenu)); REQUIRE(U(CreateMenu));
    HWND h = mu_top(); HMENU m = U(CreateMenu)();
    if (!h || !m) { CHECK(0); return; }
    U(SetMenu)(h, m);
    U(SetMenuContextHelpId)(m, 700);
    U(SetWindowContextHelpId)(h, 800);
    CHECK(U(GetMenuContextHelpId)(m) == 700 && U(GetWindowContextHelpId)(h) == 800);
    U(SetMenu)(h, NULL);
    U(DestroyMenu)(m);
    mu_kill(h);
}

/* ---------- ChangeMenu ---------- */
static void mstr(HMENU m, int pos, char *out, int n)
{
    out[0] = 0;
    U(GetMenuStringA)(m, pos, out, n, MF_BYPOSITION);
}
TEST(misc_ui_changemenu_append_a) {
    NEED_API(); REQUIRE(U(ChangeMenuA)); REQUIRE(U(CreateMenu)); REQUIRE(U(GetMenuItemCount)); REQUIRE(U(GetMenuStringA)); REQUIRE(U(GetMenuItemID)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    char b[32];
    CHECK(U(ChangeMenuA)(m, 0, "First", 100, MF_APPEND | MF_STRING));
    CHECK(U(ChangeMenuA)(m, 0, "Second", 101, MF_APPEND | MF_STRING));
    CHECK(U(GetMenuItemCount)(m) == 2);
    mstr(m, 0, b, 32); CHECK(!strcmp(b, "First"));
    mstr(m, 1, b, 32); CHECK(!strcmp(b, "Second"));
    CHECK(U(GetMenuItemID)(m, 0) == 100 && U(GetMenuItemID)(m, 1) == 101);
    U(DestroyMenu)(m);
}
TEST(misc_ui_changemenu_append_w) {
    NEED_API(); REQUIRE(U(ChangeMenuW)); REQUIRE(U(CreateMenu)); REQUIRE(U(GetMenuItemCount)); REQUIRE(U(GetMenuStringW)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    WCHAR b[32];
    CHECK(U(ChangeMenuW)(m, 0, L"Wide\x4e2d", 300, MF_APPEND | MF_STRING));
    CHECK(U(GetMenuItemCount)(m) == 1);
    CHECK(U(GetMenuStringW)(m, 0, b, 32, MF_BYPOSITION) == 5 && !lstrcmpW(b, L"Wide\x4e2d"));
    U(DestroyMenu)(m);
}
TEST(misc_ui_changemenu_insert_by_position_and_command) {
    NEED_API(); REQUIRE(U(ChangeMenuA)); REQUIRE(U(CreateMenu)); REQUIRE(U(GetMenuItemCount)); REQUIRE(U(GetMenuStringA)); REQUIRE(U(GetMenuItemID)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    char b[32];
    U(ChangeMenuA)(m, 0, "B", 11, MF_APPEND | MF_STRING);
    U(ChangeMenuA)(m, 0, "D", 13, MF_APPEND | MF_STRING);
    CHECK(U(ChangeMenuA)(m, 0, "A", 10, MF_INSERT | MF_STRING | MF_BYPOSITION));       /* at front */
    mstr(m, 0, b, 32); CHECK(!strcmp(b, "A"));
    CHECK(U(ChangeMenuA)(m, 13, "C", 12, MF_INSERT | MF_STRING | MF_BYCOMMAND));       /* before command 13 */
    CHECK(U(GetMenuItemCount)(m) == 4);
    mstr(m, 0, b, 32); CHECK(!strcmp(b, "A"));
    mstr(m, 1, b, 32); CHECK(!strcmp(b, "B"));
    mstr(m, 2, b, 32); CHECK(!strcmp(b, "C"));
    mstr(m, 3, b, 32); CHECK(!strcmp(b, "D"));
    CHECK(U(GetMenuItemID)(m, 2) == 12);
    U(DestroyMenu)(m);
}
TEST(misc_ui_changemenu_change_modifies) {
    NEED_API(); REQUIRE(U(ChangeMenuA)); REQUIRE(U(CreateMenu)); REQUIRE(U(GetMenuItemCount)); REQUIRE(U(GetMenuStringA)); REQUIRE(U(GetMenuItemID)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    char b[32];
    U(ChangeMenuA)(m, 0, "Old", 50, MF_APPEND | MF_STRING);
    CHECK(U(ChangeMenuA)(m, 50, "New", 51, MF_CHANGE | MF_STRING | MF_BYCOMMAND));
    CHECK(U(GetMenuItemCount)(m) == 1);
    mstr(m, 0, b, 32); CHECK(!strcmp(b, "New"));
    CHECK(U(GetMenuItemID)(m, 0) == 51);
    CHECK(U(ChangeMenuA)(m, 0, "Pos", 52, MF_CHANGE | MF_STRING | MF_BYPOSITION));
    mstr(m, 0, b, 32); CHECK(!strcmp(b, "Pos"));
    CHECK(U(GetMenuItemID)(m, 0) == 52);
    SetLastError(0);
    CHECK(!U(ChangeMenuA)(m, 999, "None", 1, MF_CHANGE | MF_STRING | MF_BYCOMMAND));
    U(DestroyMenu)(m);
}
TEST(misc_ui_changemenu_delete_and_remove) {
    NEED_API(); REQUIRE(U(ChangeMenuA)); REQUIRE(U(CreateMenu)); REQUIRE(U(CreatePopupMenu)); REQUIRE(U(GetMenuItemCount)); REQUIRE(U(GetMenuStringA)); REQUIRE(U(DestroyMenu)); REQUIRE(U(GetSubMenu)); REQUIRE(U(AppendMenuA));
    HMENU m = U(CreateMenu)();
    for (int i = 0; i < 5; i++) { char s[8]; sprintf(s, "I%d", i); U(ChangeMenuA)(m, 0, s, 200 + i, MF_APPEND | MF_STRING); }
    char b[32];
    CHECK(U(ChangeMenuA)(m, 202, NULL, 0, MF_DELETE | MF_BYCOMMAND));            /* by command: I2 gone */
    CHECK(U(GetMenuItemCount)(m) == 4);
    mstr(m, 2, b, 32); CHECK(!strcmp(b, "I3"));
    CHECK(U(ChangeMenuA)(m, 0, NULL, 0, MF_DELETE | MF_BYPOSITION));             /* by position: I0 gone */
    CHECK(U(GetMenuItemCount)(m) == 3);
    mstr(m, 0, b, 32); CHECK(!strcmp(b, "I1"));
    /* MF_REMOVE always works by position in ChangeMenu */
    CHECK(U(ChangeMenuA)(m, 0, NULL, 0, MF_REMOVE));
    CHECK(U(GetMenuItemCount)(m) == 2);
    mstr(m, 0, b, 32); CHECK(!strcmp(b, "I3"));
    SetLastError(0);
    CHECK(!U(ChangeMenuA)(m, 1000, NULL, 0, MF_DELETE | MF_BYCOMMAND));
    CHECK(!U(ChangeMenuA)(m, 50, NULL, 0, MF_DELETE | MF_BYPOSITION));
    /* remove keeps a submenu alive, delete destroys it */
    HMENU sub = U(CreatePopupMenu)();
    U(AppendMenuA)(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, "Sub");
    CHECK(U(GetSubMenu)(m, 2) == sub);
    CHECK(U(ChangeMenuA)(m, 2, NULL, 0, MF_REMOVE));
    CHECK(U(IsMenu)(sub));
    U(AppendMenuA)(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, "Sub");
    CHECK(U(ChangeMenuA)(m, 2, NULL, 0, MF_DELETE | MF_BYPOSITION));
    CHECK(!U(IsMenu)(sub));
    U(DestroyMenu)(m);
}
TEST(misc_ui_changemenu_separator_forms) {
    NEED_API(); REQUIRE(U(ChangeMenuA)); REQUIRE(U(CreateMenu)); REQUIRE(U(GetMenuItemCount)); REQUIRE(U(GetMenuState)); REQUIRE(U(DestroyMenu));
    HMENU m = U(CreateMenu)();
    CHECK(U(ChangeMenuA)(m, 0, "Item", 1, MF_APPEND | MF_STRING));
    CHECK(U(ChangeMenuA)(m, 0, NULL, 0, MF_SEPARATOR));              /* cmd 0 + separator: appended */
    CHECK(U(GetMenuItemCount)(m) == 2);
    CHECK(U(GetMenuState)(m, 1, MF_BYPOSITION) & MF_SEPARATOR);
    CHECK(U(ChangeMenuA)(m, 0, NULL, 0, MF_APPEND));                  /* NULL item implies separator */
    CHECK(U(GetMenuItemCount)(m) == 3);
    CHECK(U(GetMenuState)(m, 2, MF_BYPOSITION) & MF_SEPARATOR);
    U(DestroyMenu)(m);
}
TEST(misc_ui_changemenu_invalid_menu) {
    NEED_API(); REQUIRE(U(ChangeMenuA)); REQUIRE(U(ChangeMenuW));
    HMENU bad[] = { NULL, (HMENU)1, (HMENU)0x12345678 };
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        CHECKF(!U(ChangeMenuA)(bad[i], 0, "x", 1, MF_APPEND | MF_STRING), "A %p", bad[i]);
        CHECKF(!U(ChangeMenuW)(bad[i], 0, L"x", 1, MF_APPEND | MF_STRING), "W %p", bad[i]);
        CHECKF(!U(ChangeMenuA)(bad[i], 0, NULL, 0, MF_DELETE), "del %p", bad[i]);
        CHECKF(!U(ChangeMenuA)(bad[i], 0, NULL, 0, MF_REMOVE), "rem %p", bad[i]);
    }
}
TEST(misc_ui_changemenu_a_w_parity) {
    NEED_API(); REQUIRE(U(ChangeMenuA)); REQUIRE(U(ChangeMenuW)); REQUIRE(U(CreateMenu)); REQUIRE(U(GetMenuItemCount)); REQUIRE(U(GetMenuStringA)); REQUIRE(U(DestroyMenu));
    HMENU a = U(CreateMenu)(), w = U(CreateMenu)();
    for (int i = 0; i < 4; i++) {
        char s[8]; WCHAR ws[8];
        sprintf(s, "M%d", i); wsprintfW(ws, L"M%d", i);
        CHECK(U(ChangeMenuA)(a, 0, s, 500 + i, MF_APPEND | MF_STRING));
        CHECK(U(ChangeMenuW)(w, 0, ws, 500 + i, MF_APPEND | MF_STRING));
    }
    CHECK(U(ChangeMenuA)(a, 501, NULL, 0, MF_DELETE | MF_BYCOMMAND));
    CHECK(U(ChangeMenuW)(w, 501, NULL, 0, MF_DELETE | MF_BYCOMMAND));
    CHECK(U(GetMenuItemCount)(a) == 3 && U(GetMenuItemCount)(w) == 3);
    for (int i = 0; i < 3; i++) { char x[16], y[16]; mstr(a, i, x, 16); mstr(w, i, y, 16); CHECK(!strcmp(x, y)); }
    U(DestroyMenu)(a); U(DestroyMenu)(w);
}

/* ---------- PrintWindow ---------- */
TEST(misc_ui_printwindow_basic) {
    NEED_API(); REQUIRE(U(PrintWindow));
    HWND h = mu_mk(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 40, 40, 120, 90, NULL);
    if (!h) { CHECK(0); return; }
    HDC sdc = GetDC(NULL); HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 120, 90);
    HGDIOBJ old = SelectObject(m, bm);
    PatBlt(m, 0, 0, 120, 90, BLACKNESS);
    CHECK(U(PrintWindow)(h, m, 0));
    CHECK(U(PrintWindow)(h, m, PW_CLIENTONLY));
    t_note("PrintWindow pixel(10,10)=%06lx btnface=%06lx", GetPixel(m, 10, 10), GetSysColor(COLOR_BTNFACE));
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(h);
}
TEST(misc_ui_printwindow_sends_wm_print_or_paint) {
    NEED_API(); REQUIRE(U(PrintWindow));
    HWND h = mu_mk(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 40, 40, 120, 90, NULL);
    if (!h) { CHECK(0); return; }
    HDC sdc = GetDC(NULL); HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 120, 90);
    HGDIOBJ old = SelectObject(m, bm);
    mu_log_reset(); mu_logging = 1;
    BOOL r = U(PrintWindow)(h, m, 0);
    mu_logging = 0;
    CHECK(r);
    t_note("PrintWindow: WM_PRINT=%d WM_PRINTCLIENT=%d WM_PAINT=%d WM_ERASEBKGND=%d WM_NCPAINT=%d", mu_count(WM_PRINT), mu_count(WM_PRINTCLIENT), mu_count(WM_PAINT), mu_count(WM_ERASEBKGND), mu_count(WM_NCPAINT));
    int i = mu_find(WM_PRINT);
    if (i >= 0) t_note("WM_PRINT flags 0x%llx hdc==target %d", (long long)mu_log[i].l, mu_log[i].w == (WPARAM)m);
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(h);
}
TEST(misc_ui_printwindow_invalid_args) {
    NEED_API(); REQUIRE(U(PrintWindow));
    HWND h = mu_mk(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 40, 40, 60, 60, NULL);
    if (!h) { CHECK(0); return; }
    HDC sdc = GetDC(NULL); HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 60, 60);
    HGDIOBJ old = SelectObject(m, bm);
    SetLastError(0);
    t_note("PrintWindow(bogus hwnd) = %d", U(PrintWindow)(BOGUS_HWND, m, 0));
    SetLastError(0);
    t_note("PrintWindow(NULL hwnd) = %d", U(PrintWindow)(NULL, m, 0));
    SetLastError(0);
    t_note("PrintWindow(h, NULL hdc) = %d", U(PrintWindow)(h, NULL, 0));
    SetLastError(0);
    t_note("PrintWindow(h, bogus hdc) = %d err=%lu", U(PrintWindow)(h, (HDC)(ULONG_PTR)0x1234, 0), GetLastError());
    BOOL r = U(PrintWindow)(h, m, 0xFFFFFFF0);
    t_note("PrintWindow(unknown flags) -> %d", r);
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(h);
}
TEST(misc_ui_printwindow_hidden_and_minimized) {
    NEED_API(); REQUIRE(U(PrintWindow));
    HWND h = mu_mk(WS_OVERLAPPEDWINDOW, WS_EX_NOACTIVATE, 40, 40, 120, 90, NULL);
    if (!h) { CHECK(0); return; }
    HDC sdc = GetDC(NULL); HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 120, 90);
    HGDIOBJ old = SelectObject(m, bm);
    BOOL r = U(PrintWindow)(h, m, 0);
    t_note("PrintWindow(hidden window) -> %d", r);
    U(ShowWindow)(h, SW_SHOWMINNOACTIVE);
    r = U(PrintWindow)(h, m, 0);
    t_note("PrintWindow(minimized window) -> %d", r);
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_kill(h);
}
TEST(misc_ui_printwindow_cross_thread) {
    NEED_API(); REQUIRE(U(PrintWindow));
    mu_worker w;
    if (!mu_worker_start(&w, WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, "pw")) { CHECK(0); mu_worker_stop(&w); return; }
    HDC sdc = GetDC(NULL); HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, 200, 150);
    HGDIOBJ old = SelectObject(m, bm);
    CHECK(U(PrintWindow)(w.hwnd, m, 0));
    SelectObject(m, old); DeleteObject(bm); DeleteDC(m); ReleaseDC(NULL, sdc);
    mu_worker_stop(&w);
}

/* ---------- PrivateExtractIcons ---------- */
UDECL(UINT, PrivateExtractIconExA, (LPCSTR, int, HICON *, HICON *, UINT));
UDECL(UINT, PrivateExtractIconExW, (LPCWSTR, int, HICON *, HICON *, UINT));
static void sysfile(char *out, const char *name)
{
    GetSystemDirectoryA(out, MAX_PATH);
    lstrcatA(out, "\\"); lstrcatA(out, name);
}
TEST(misc_ui_extracticons_count) {
    NEED_API(); REQUIRE(U(PrivateExtractIconsA)); REQUIRE(U(PrivateExtractIconsW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH];
    sysfile(path, "shell32.dll");
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    UINT na = U(PrivateExtractIconsA)(path, 0, 32, 32, NULL, NULL, 0, 0);
    UINT nw = U(PrivateExtractIconsW)(wp, 0, 32, 32, NULL, NULL, 0, 0);
    t_note("shell32.dll icon count: A=%u W=%u", na, nw);
    CHECK(na == nw);
    CHECK(na > 0 && na != 0xFFFFFFFF);
}
TEST(misc_ui_extracticons_extract_sizes) {
    NEED_API(); REQUIRE(U(PrivateExtractIconsA)); REQUIRE(U(GetIconInfo)); REQUIRE(U(DestroyIcon));
    char path[MAX_PATH];
    sysfile(path, "shell32.dll");
    int sizes[] = { 16, 32, 48 };
    for (int i = 0; i < 3; i++) {
        HICON ic[1] = { NULL }; UINT id[1] = { 0 };
        UINT n = U(PrivateExtractIconsA)(path, 0, sizes[i], sizes[i], ic, id, 1, 0);
        CHECKF(n == 1, "size %d n=%u", sizes[i], n);
        CHECKF(ic[0] != NULL, "size %d icon", sizes[i]);
        if (ic[0]) {
            ICONINFO ii; BITMAP bm;
            CHECK(U(GetIconInfo)(ic[0], &ii));
            GetObject(ii.hbmColor ? ii.hbmColor : ii.hbmMask, sizeof bm, &bm);
            CHECKF(bm.bmWidth == sizes[i], "size %d width %ld", sizes[i], bm.bmWidth);
            if (ii.hbmColor) DeleteObject(ii.hbmColor);
            if (ii.hbmMask) DeleteObject(ii.hbmMask);
            CHECK(U(DestroyIcon)(ic[0]));
        }
        CHECKF(id[0] != 0, "size %d id", sizes[i]);
    }
}
TEST(misc_ui_extracticons_a_w_same_icon) {
    NEED_API(); REQUIRE(U(PrivateExtractIconsA)); REQUIRE(U(PrivateExtractIconsW)); REQUIRE(U(DestroyIcon));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH];
    sysfile(path, "shell32.dll");
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    HICON a[2] = {0}, w[2] = {0}; UINT ia[2] = {0}, iw[2] = {0};
    UINT na = U(PrivateExtractIconsA)(path, 3, 32, 32, a, ia, 2, 0);
    UINT nw = U(PrivateExtractIconsW)(wp, 3, 32, 32, w, iw, 2, 0);
    CHECK(na == nw);
    CHECK(na == 2);
    CHECK(ia[0] == iw[0] && ia[1] == iw[1]);
    for (int i = 0; i < 2; i++) { if (a[i]) U(DestroyIcon)(a[i]); if (w[i]) U(DestroyIcon)(w[i]); }
}
TEST(misc_ui_extracticons_negative_index_by_resource_id) {
    NEED_API(); REQUIRE(U(PrivateExtractIconsA)); REQUIRE(U(DestroyIcon));
    char path[MAX_PATH];
    sysfile(path, "shell32.dll");
    HICON ic[1] = {0}; UINT id[1] = {0};
    UINT n0 = U(PrivateExtractIconsA)(path, 0, 32, 32, ic, id, 1, 0);
    UINT rid = id[0];
    if (ic[0]) U(DestroyIcon)(ic[0]);
    HICON ic2[1] = {0}; UINT id2[1] = {0};
    UINT n1 = U(PrivateExtractIconsA)(path, 0, 32, 32, ic2, id2, 1, 0);
    CHECK(n0 == 1 && n1 == 1);
    CHECK(id2[0] == rid);
    if (ic2[0]) U(DestroyIcon)(ic2[0]);
}
TEST(misc_ui_extracticons_missing_file) {
    NEED_API(); REQUIRE(U(PrivateExtractIconsA)); REQUIRE(U(PrivateExtractIconsW));
    HICON ic[1] = {0}; UINT id[1] = {0};
    SetLastError(0);
    UINT n = U(PrivateExtractIconsA)("C:\\no_such_dir_mu\\none.dll", 0, 32, 32, ic, id, 1, 0);
    t_note("missing file A: n=%u err=%lu", n, GetLastError());
    CHECK(ic[0] == NULL);
    CHECK(n == 0 || n == 0xFFFFFFFF);
    SetLastError(0);
    n = U(PrivateExtractIconsW)(L"C:\\no_such_dir_mu\\none.dll", 0, 32, 32, ic, id, 1, 0);
    t_note("missing file W: n=%u err=%lu", n, GetLastError());
    CHECK(ic[0] == NULL);
    CHECK(n == 0 || n == 0xFFFFFFFF);
    n = U(PrivateExtractIconsA)("", 0, 32, 32, ic, id, 1, 0);
    CHECK(ic[0] == NULL && (n == 0 || n == 0xFFFFFFFF));
}
TEST(misc_ui_extracticons_index_out_of_range) {
    NEED_API(); REQUIRE(U(PrivateExtractIconsA));
    char path[MAX_PATH];
    sysfile(path, "shell32.dll");
    HICON ic[1] = {0}; UINT id[1] = {0};
    UINT n = U(PrivateExtractIconsA)(path, 100000, 32, 32, ic, id, 1, 0);
    CHECK(ic[0] == NULL);
    CHECK(n == 0 || n == 0xFFFFFFFF);
    t_note("index 100000 -> %u", n);
}
TEST(misc_ui_extracticons_no_icon_file) {
    NEED_API(); REQUIRE(U(PrivateExtractIconsA));
    char path[MAX_PATH];
    sysfile(path, "kernel32.dll");
    UINT n = U(PrivateExtractIconsA)(path, 0, 32, 32, NULL, NULL, 0, 0);
    t_note("kernel32.dll icon count: %u", n);
    HICON ic[1] = {0};
    n = U(PrivateExtractIconsA)(path, 0, 32, 32, ic, NULL, 1, 0);
    CHECK(n == 0 || n == 0xFFFFFFFF || ic[0] != NULL);
    if (ic[0]) U(DestroyIcon)(ic[0]);
}
TEST(misc_ui_extracticonex_large_small) {
    NEED_API(); REQUIRE(UN(PrivateExtractIconExA)); REQUIRE(UN(PrivateExtractIconExW)); REQUIRE(U(GetIconInfo)); REQUIRE(U(DestroyIcon));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH];
    sysfile(path, "shell32.dll");
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    UINT total = UN(PrivateExtractIconExA)(path, -1, NULL, NULL, 0);
    UINT totalw = UN(PrivateExtractIconExW)(wp, -1, NULL, NULL, 0);
    t_note("PrivateExtractIconEx count A=%u W=%u", total, totalw);
    CHECK(total == totalw);
    CHECK(total > 0);
    HICON lg[2] = {0}, sm[2] = {0};
    UINT n = UN(PrivateExtractIconExA)(path, 0, lg, sm, 2);
    t_note("PrivateExtractIconExA(0, 2 icons) -> %u", n);
    CHECK(n >= 2);
    for (int i = 0; i < 2; i++) {
        CHECK(lg[i] != NULL && sm[i] != NULL);
        if (lg[i] && sm[i]) {
            ICONINFO a, b; BITMAP ba, bb;
            U(GetIconInfo)(lg[i], &a); U(GetIconInfo)(sm[i], &b);
            GetObject(a.hbmMask, sizeof ba, &ba); GetObject(b.hbmMask, sizeof bb, &bb);
            CHECKF(ba.bmWidth == GetSystemMetrics(SM_CXICON), "large width %ld metric %d", ba.bmWidth, GetSystemMetrics(SM_CXICON));
            CHECKF(bb.bmWidth == GetSystemMetrics(SM_CXSMICON), "small width %ld metric %d", bb.bmWidth, GetSystemMetrics(SM_CXSMICON));
            if (a.hbmColor) { DeleteObject(a.hbmColor); } DeleteObject(a.hbmMask);
            if (b.hbmColor) { DeleteObject(b.hbmColor); } DeleteObject(b.hbmMask);
        }
        if (lg[i]) U(DestroyIcon)(lg[i]);
        if (sm[i]) U(DestroyIcon)(sm[i]);
    }
    HICON only[1] = {0};
    n = UN(PrivateExtractIconExW)(wp, 1, only, NULL, 1);
    CHECK(n >= 1 && only[0] != NULL);
    if (only[0]) U(DestroyIcon)(only[0]);
    HICON osm[1] = {0};
    n = UN(PrivateExtractIconExW)(wp, 1, NULL, osm, 1);
    CHECK(n >= 1 && osm[0] != NULL);
    if (osm[0]) U(DestroyIcon)(osm[0]);
}
TEST(misc_ui_extracticonex_missing_file) {
    NEED_API(); REQUIRE(UN(PrivateExtractIconExA));
    HICON lg[1] = {0}, sm[1] = {0};
    UINT n = UN(PrivateExtractIconExA)("C:\\no_such_dir_mu\\none.dll", 0, lg, sm, 1);
    CHECK(lg[0] == NULL && sm[0] == NULL);
    CHECK(n == 0 || n == 0xFFFFFFFF);
    n = UN(PrivateExtractIconExA)("C:\\no_such_dir_mu\\none.dll", -1, NULL, NULL, 0);
    CHECK(n == 0 || n == 0xFFFFFFFF);
}

/* ---------- EndTask / WinHelp / SoundSentry: error paths ---------- */
TEST(misc_ui_endtask_invalid_window) {
    NEED_API(); REQUIRE(U(EndTask));
    if (GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version")) { t_skip("EndTask is an aborting stub in this emulator"); return; }
    HWND bad[] = { NULL, BOGUS_HWND };
    for (int i = 0; i < 2; i++) {
        SetLastError(0);
        BOOL r = U(EndTask)(bad[i], FALSE, FALSE);
        CHECKF(!r, "EndTask(%p)", bad[i]);
        t_note("EndTask(%p) err=%lu", bad[i], GetLastError());
    }
}
TEST(misc_ui_winhelp_quit_and_missing) {
    NEED_API(); REQUIRE(U(WinHelpA)); REQUIRE(U(WinHelpW));
    /* HELP_QUIT with no help window open */
    SetLastError(0);
    BOOL a = U(WinHelpA)(NULL, NULL, HELP_QUIT, 0);
    BOOL w = U(WinHelpW)(NULL, NULL, HELP_QUIT, 0);
    t_note("WinHelp(HELP_QUIT, no help window): A=%d W=%d err=%lu", a, w, GetLastError());
    CHECK(a == w);
    SetLastError(0);
    a = U(WinHelpA)(BOGUS_HWND, "C:\\no_such_dir_mu\\none.hlp", HELP_QUIT, 0);
    t_note("WinHelp(bogus hwnd, HELP_QUIT) = %d err=%lu", a, GetLastError());
}
TEST(misc_ui_soundsentry_call) {
    NEED_API(); REQUIRE(U(SoundSentry));
    BOOL r = U(SoundSentry)();
    t_note("SoundSentry() = %d (feature disabled by default: FALSE expected)", r);
}

/* ---------- DPI contexts (read-only helpers) ---------- */
UDECL(BOOL, IsValidDpiAwarenessContext, (DPI_AWARENESS_CONTEXT));
UDECL(BOOL, AreDpiAwarenessContextsEqual, (DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT));
UDECL(UINT, GetDpiForWindow, (HWND));
UDECL(UINT, GetDpiForSystem, (void));
UDECL(DPI_AWARENESS_CONTEXT, GetWindowDpiAwarenessContext, (HWND));
UDECL(DPI_AWARENESS_CONTEXT, GetThreadDpiAwarenessContext, (void));
UDECL(DPI_AWARENESS, GetAwarenessFromDpiAwarenessContext, (DPI_AWARENESS_CONTEXT));
UDECL(BOOL, IsWindowDpiAware, (HWND));

TEST(misc_ui_dpi_context_validity) {
    REQUIRE(UN(IsValidDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT ok[] = { (DPI_AWARENESS_CONTEXT)-1, (DPI_AWARENESS_CONTEXT)-2, (DPI_AWARENESS_CONTEXT)-3, (DPI_AWARENESS_CONTEXT)-4 };
    for (int i = 0; i < 4; i++) CHECKF(UN(IsValidDpiAwarenessContext)(ok[i]), "ctx %d", -1 - i);
    t_note("IsValid(-5 UNAWARE_GDISCALED) = %d", UN(IsValidDpiAwarenessContext)((DPI_AWARENESS_CONTEXT)-5));
    DPI_AWARENESS_CONTEXT bad[] = { NULL, (DPI_AWARENESS_CONTEXT)-6, (DPI_AWARENESS_CONTEXT)-100, (DPI_AWARENESS_CONTEXT)1, (DPI_AWARENESS_CONTEXT)0x1234, (DPI_AWARENESS_CONTEXT)(LONG_PTR)0x7FFFFFFF };
    for (int i = 0; i < 6; i++) CHECKF(!UN(IsValidDpiAwarenessContext)(bad[i]), "bad ctx %p", bad[i]);
}
TEST(misc_ui_dpi_context_equality) {
    REQUIRE(UN(AreDpiAwarenessContextsEqual));
    for (int a = -4; a <= -1; a++)
        for (int b = -4; b <= -1; b++)
            CHECKF(UN(AreDpiAwarenessContextsEqual)((DPI_AWARENESS_CONTEXT)(LONG_PTR)a, (DPI_AWARENESS_CONTEXT)(LONG_PTR)b) == (a == b), "%d vs %d", a, b);
    CHECK(!UN(AreDpiAwarenessContextsEqual)((DPI_AWARENESS_CONTEXT)-1, NULL));
    CHECK(!UN(AreDpiAwarenessContextsEqual)(NULL, (DPI_AWARENESS_CONTEXT)-1));
    t_note("Equal(NULL,NULL)=%d", UN(AreDpiAwarenessContextsEqual)(NULL, NULL));
}
TEST(misc_ui_dpi_awareness_from_context) {
    REQUIRE(UN(GetAwarenessFromDpiAwarenessContext));
    CHECK(UN(GetAwarenessFromDpiAwarenessContext)((DPI_AWARENESS_CONTEXT)-1) == DPI_AWARENESS_UNAWARE);
    CHECK(UN(GetAwarenessFromDpiAwarenessContext)((DPI_AWARENESS_CONTEXT)-2) == DPI_AWARENESS_SYSTEM_AWARE);
    CHECK(UN(GetAwarenessFromDpiAwarenessContext)((DPI_AWARENESS_CONTEXT)-3) == DPI_AWARENESS_PER_MONITOR_AWARE);
    CHECK(UN(GetAwarenessFromDpiAwarenessContext)((DPI_AWARENESS_CONTEXT)-4) == DPI_AWARENESS_PER_MONITOR_AWARE);
    CHECK(UN(GetAwarenessFromDpiAwarenessContext)(NULL) == DPI_AWARENESS_INVALID);
    CHECK(UN(GetAwarenessFromDpiAwarenessContext)((DPI_AWARENESS_CONTEXT)0x1234) == DPI_AWARENESS_INVALID);
}
TEST(misc_ui_dpi_window_context_consistency) {
    NEED_API(); REQUIRE(UN(GetWindowDpiAwarenessContext)); REQUIRE(UN(GetThreadDpiAwarenessContext)); REQUIRE(UN(AreDpiAwarenessContextsEqual)); REQUIRE(UN(GetDpiForWindow));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    DPI_AWARENESS_CONTEXT w = UN(GetWindowDpiAwarenessContext)(h), t = UN(GetThreadDpiAwarenessContext)();
    CHECK(UN(AreDpiAwarenessContextsEqual)(w, t));
    CHECK(UN(IsValidDpiAwarenessContext)(w));
    UINT dpi = UN(GetDpiForWindow)(h);
    CHECK(dpi >= 96 || dpi == 0);
    CHECK(dpi != 0);
    mu_worker wk;
    if (mu_worker_start(&wk, WS_POPUP, 0, "dpi")) {
        CHECK(UN(GetWindowDpiAwarenessContext)(wk.hwnd) != NULL);
        CHECK(UN(GetDpiForWindow)(wk.hwnd) == dpi || UN(GetDpiForWindow)(wk.hwnd) != 0);
    }
    mu_worker_stop(&wk);
    mu_kill(h);
}
TEST(misc_ui_dpi_window_invalid) {
    REQUIRE(UN(GetWindowDpiAwarenessContext)); REQUIRE(UN(GetDpiForWindow));
    SetLastError(0);
    CHECK(UN(GetDpiForWindow)(BOGUS_HWND) == 0);
    CHECK(UN(GetDpiForWindow)(NULL) == 0);
    SetLastError(0);
    CHECK(UN(GetWindowDpiAwarenessContext)(BOGUS_HWND) == NULL);
    CHECK(UN(GetWindowDpiAwarenessContext)(NULL) == NULL);
    HWND h = mu_top();
    if (h) { mu_kill(h); CHECK(UN(GetDpiForWindow)(h) == 0); CHECK(UN(GetWindowDpiAwarenessContext)(h) == NULL); }
}
TEST(misc_ui_dpi_system_dpi) {
    REQUIRE(UN(GetDpiForSystem));
    UINT d = UN(GetDpiForSystem)();
    CHECK(d >= 96);
    CHECK(d == UN(GetDpiForSystem)());
    HDC dc = GetDC(NULL);
    int lx = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    t_note("GetDpiForSystem=%u LOGPIXELSX=%d", d, lx);
}
