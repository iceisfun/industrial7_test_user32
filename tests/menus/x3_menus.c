/* menus: second-pass depth. Documented lifetime rules (DeleteMenu / RemoveMenu / ModifyMenu /
 * DestroyMenu with submenus), GetMenuState popup protocol, MF_* / MFS_* aliases, handle churn,
 * invalid-handle returns, and LoadMenu from a data-file module. */
#include "mh.h"

TEST(menus_x3_flag_aliases) {
    EQ(MF_BYCOMMAND, 0); EQ(MF_BYPOSITION, 0x400); EQ(MF_SEPARATOR, 0x800);
    EQ(MF_SYSMENU, 0x2000); EQ(MF_HELP, 0x4000); EQ(MF_RIGHTJUSTIFY, 0x4000); EQ(MF_MOUSESELECT, 0x8000);
    EQ(MF_DEFAULT, 0x1000); EQ(MF_MENUBARBREAK, 0x20); EQ(MF_MENUBREAK, 0x40); EQ(MF_OWNERDRAW, 0x100);
    EQ(MFT_RIGHTJUSTIFY, MF_RIGHTJUSTIFY); EQ(MFT_SEPARATOR, MF_SEPARATOR); EQ(MFT_OWNERDRAW, MF_OWNERDRAW);
    EQ(MFT_MENUBREAK, MF_MENUBREAK); EQ(MFT_MENUBARBREAK, MF_MENUBARBREAK);
    EQ(MFS_GRAYED, 3); EQ(MFS_DISABLED, MFS_GRAYED); EQ(MFS_CHECKED, MF_CHECKED); EQ(MFS_HILITE, MF_HILITE);
    EQ(MFS_DEFAULT, MF_DEFAULT); EQ(MFS_ENABLED, MF_ENABLED); EQ(MFS_UNCHECKED, MF_UNCHECKED);
    EQ(MFS_UNHILITE, MF_UNHILITE);
}

TEST(menus_x3_deletemenu_destroys_submenu_removemenu_keeps) {
    REQ_CORE();
    HMENU p = M_NEW(), s1 = M_POP(), s2 = M_POP();
    REQUIRE(p && s1 && s2);
    CHECK(U(AppendMenuA)(p, MF_POPUP, (UINT_PTR)s1, "one"));
    CHECK(U(AppendMenuA)(p, MF_POPUP, (UINT_PTR)s2, "two"));
    EQ(M_CNT(p), 2);
    CHECK(U(RemoveMenu)(p, 1, MF_BYPOSITION));      /* detaches s2, does not destroy it */
    EQ(M_CNT(p), 1);
    CHECK(M_IS(s2));
    EQ(U(GetSubMenu)(p, 0), s1);
    CHECK(U(DeleteMenu)(p, 0, MF_BYPOSITION));      /* destroys s1 */
    CHECK(!M_IS(s1));
    EQ(M_CNT(p), 0);
    CHECK(M_IS(s2));
    CHECK(M_DESTROY(s2));
    CHECK(M_DESTROY(p));
}

TEST(menus_x3_destroymenu_is_recursive) {
    REQ_CORE();
    HMENU top = M_NEW(), a = M_POP(), b = M_POP();
    REQUIRE(top && a && b);
    CHECK(U(AppendMenuA)(a, MF_POPUP, (UINT_PTR)b, "inner"));
    CHECK(U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)a, "outer"));
    CHECK(M_DESTROY(top));
    CHECK(!M_IS(top)); CHECK(!M_IS(a)); CHECK(!M_IS(b));
}

TEST(menus_x3_modifymenu_replacing_popup_destroys_old_submenu) {
    REQ_CORE();
    HMENU p = M_NEW(), s = M_POP();
    REQUIRE(p && s);
    CHECK(U(AppendMenuA)(p, MF_POPUP, (UINT_PTR)s, "sub"));
    /* documented: replacing an item that opens a submenu destroys that submenu */
    CHECK(U(ModifyMenuA)(p, 0, MF_BYPOSITION | MF_STRING, 77, "plain"));
    CHECK(!M_IS(s));
    EQ(U(GetMenuItemID)(p, 0), 77);
    EQ(U(GetSubMenu)(p, 0), NULL);
    M_DESTROY(p);
}

TEST(menus_x3_modifymenu_to_popup_and_back_ids) {
    REQ_CORE();
    HMENU p = M_NEW(), s = M_POP();
    REQUIRE(p && s);
    U(AppendMenuA)(s, MF_STRING, 5, "x");
    U(AppendMenuA)(p, MF_STRING, 10, "item");
    CHECK(U(ModifyMenuW)(p, 10, MF_BYCOMMAND | MF_POPUP | MF_STRING, (UINT_PTR)s, L"popup"));
    EQ(U(GetSubMenu)(p, 0), s);
    EQ(U(GetMenuItemID)(p, 0), (UINT)-1);           /* documented: popup items report -1 */
    SetLastError(0);
    CHECK(!U(ModifyMenuW)(p, 9999, MF_BYCOMMAND | MF_STRING, 1, L"nope"));
    t_note("ModifyMenuW(missing id) err=%lu", GetLastError());
    CHECK(!U(ModifyMenuW)(p, 5, MF_BYPOSITION | MF_STRING, 1, L"nope"));
    M_DESTROY(p);
}

TEST(menus_x3_getmenustate_popup_item_count_in_high_byte) {
    REQ_CORE();
    HMENU p = M_NEW(), s = mh_make(3);
    REQUIRE(p && s);
    U(AppendMenuA)(p, MF_POPUP, (UINT_PTR)s, "sub");
    UINT st = U(GetMenuState)(p, 0, MF_BYPOSITION);
    CHECK(st != 0xFFFFFFFFu);
    CHECK(st & MF_POPUP);
    EQ((st >> 8) & 0xFF, 3);                        /* documented: high byte = item count of the popup */
    UINT st2 = U(GetMenuState)(p, 99, MF_BYPOSITION);
    EQ(st2, 0xFFFFFFFFu);
    M_DESTROY(p);
}

TEST(menus_x3_getmenustring_popup_by_position) {
    REQ_CORE();
    HMENU p = M_NEW(), s = mh_make(1);
    REQUIRE(p && s);
    U(AppendMenuA)(p, MF_POPUP | MF_STRING, (UINT_PTR)s, "Popup Text");
    char b[32] = "";
    EQ(U(GetMenuStringA)(p, 0, b, sizeof b, MF_BYPOSITION), 10);
    EXPECT_STR(b, "Popup Text");
    M_DESTROY(p);
}

TEST(menus_x3_insert_position_boundaries) {
    REQ_CORE();
    HMENU h = mh_make(3);
    REQUIRE(h);
    CHECK(U(InsertMenuA)(h, 3, MF_BYPOSITION | MF_STRING, 200, "at-count"));   /* == count appends */
    EQ(M_CNT(h), 4); EQ(U(GetMenuItemID)(h, 3), 200);
    CHECK(U(InsertMenuA)(h, (UINT)-1, MF_BYPOSITION | MF_STRING, 201, "minus1")); /* documented: -1 appends */
    EQ(M_CNT(h), 5); EQ(U(GetMenuItemID)(h, 4), 201);
    SetLastError(0);
    BOOL r = U(InsertMenuA)(h, 500, MF_BYPOSITION | MF_STRING, 202, "far");
    t_note("InsertMenuA(pos 500 of 5) -> %d err=%lu count=%d", r, GetLastError(), M_CNT(h));
    CHECK(U(InsertMenuA)(h, 0, MF_BYPOSITION | MF_STRING, 203, "front"));
    EQ(U(GetMenuItemID)(h, 0), 203);
    M_DESTROY(h);
}

TEST(menus_x3_handle_churn_unique_and_released) {
    REQ_CORE();
    enum { N = 1500 };
    static HMENU hs[N];
    int ok = 1;
    for (int i = 0; i < N; i++) { hs[i] = (i & 1) ? M_NEW() : M_POP(); if (!hs[i]) { ok = 0; break; } }
    CHECK(ok);
    for (int i = 0; i < N && hs[i]; i++) {
        if (!M_IS(hs[i])) { CHECKF(0, "IsMenu(%d) false", i); break; }
        if (i && hs[i] == hs[i - 1]) { CHECKF(0, "duplicate handle at %d", i); break; }
    }
    for (int i = 0; i < N; i++) if (hs[i]) M_DESTROY(hs[i]);
    for (int i = 0; i < N && hs[i]; i++) if (M_IS(hs[i])) { CHECKF(0, "IsMenu(%d) true after DestroyMenu", i); break; }
}

TEST(menus_x3_invalid_handle_returns) {
    REQ_CORE();
    REQUIRE(U(GetMenuInfo)); REQUIRE(U(SetMenuInfo)); REQUIRE(U(GetMenuItemInfoA)); REQUIRE(U(InsertMenuItemA));
    REQUIRE(U(GetMenuDefaultItem)); REQUIRE(U(SetMenuDefaultItem)); REQUIRE(U(TrackPopupMenu));
    HWND w = t_make_window(NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 50, 50, NULL);
    for (int i = 0; i < MH_NBOGUS; i++) {
        HMENU b = mh_bogus[i];
        if (!b) continue;
        MENUINFO mi = { sizeof mi, MIM_STYLE };
        MENUITEMINFOA ii; mh_init_a(&ii, MIIM_ID);
        SetLastError(0);
        CHECKF(!U(GetMenuInfo)(b, &mi), "GetMenuInfo %p", b); DWORD e1 = GetLastError();
        SetLastError(0);
        CHECKF(!U(SetMenuInfo)(b, &mi), "SetMenuInfo %p", b);
        CHECKF(!U(GetMenuItemInfoA)(b, 0, TRUE, &ii), "GetMenuItemInfoA %p", b);
        CHECKF(!U(InsertMenuItemA)(b, 0, TRUE, &ii), "InsertMenuItemA %p", b);
        CHECKF(U(GetMenuDefaultItem)(b, TRUE, 0) == (UINT)-1, "GetMenuDefaultItem %p", b);
        CHECKF(!U(SetMenuDefaultItem)(b, 0, TRUE), "SetMenuDefaultItem %p", b);
        if (w) {
            SetLastError(0);
            CHECKF(U(TrackPopupMenu)(b, TPM_RETURNCMD | TPM_NONOTIFY, 0, 0, 0, w, NULL) == 0, "TrackPopupMenu %p", b);
        }
        if (i == 2) t_note("GetMenuInfo(bogus) err=%lu (documented as invalid menu handle 1401)", e1);
    }
    if (w) U(DestroyWindow)(w);
}

static BOOL CALLBACK x3_first_menu(HMODULE m, LPCWSTR type, LPWSTR name, LONG_PTR lp) {
    *(LPWSTR *)lp = name; return FALSE;
}
TEST(menus_x3_loadmenu_from_datafile_module) {
    REQ_CORE(); REQUIRE(U(LoadMenuA)); REQUIRE(U(LoadMenuW));
    WCHAR path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n > MAX_PATH - 16) { t_skip("no system dir"); return; }
    lstrcatW(path, L"\\notepad.exe");
    HMODULE m = LoadLibraryExW(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!m) { t_skip("notepad.exe not loadable as data file (err %lu)", GetLastError()); return; }
    LPWSTR name = NULL;
    EnumResourceNamesW(m, MAKEINTRESOURCEW(4), x3_first_menu, (LONG_PTR)&name);
    if (!name) { t_note("no RT_MENU resource in notepad.exe"); FreeLibrary(m); return; }
    HMENU hw = U(LoadMenuW)(m, name);
    t_note("LoadMenuW(datafile notepad) -> %p err=%lu items=%d", hw, GetLastError(), hw ? M_CNT(hw) : -1);
    if (hw) {
        CHECK(M_IS(hw));
        CHECK(M_CNT(hw) > 0);
        if ((ULONG_PTR)name < 0x10000) {
            HMENU ha = U(LoadMenuA)(m, (LPCSTR)name);
            CHECK(ha != NULL);
            if (ha) { EQ(M_CNT(ha), M_CNT(hw)); M_DESTROY(ha); }
        }
        CHECK(M_DESTROY(hw));
        CHECK(!M_IS(hw));
    }
    FreeLibrary(m);
}
