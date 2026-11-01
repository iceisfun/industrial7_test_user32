/* menus: handle lifetime, create/destroy, append/insert/delete/remove, queries */
#include "mh.h"

TEST(menus_create_popup_valid) {
    REQ_CORE();
    HMENU a = M_NEW(), b = M_POP();
    CHECK(a != NULL); CHECK(b != NULL); CHECK(a != b);
    CHECK(M_IS(a)); CHECK(M_IS(b));
    EQ(M_CNT(a), 0); EQ(M_CNT(b), 0);
    CHECK(M_DESTROY(a)); CHECK(M_DESTROY(b));
    CHECK(!M_IS(a)); CHECK(!M_IS(b));
}

TEST(menus_create_many_unique) {
    REQ_CORE();
    enum { N = 300 };
    HMENU h[N];
    for (int i = 0; i < N; i++) h[i] = (i & 1) ? M_NEW() : M_POP();
    int ok = 1, uniq = 1;
    for (int i = 0; i < N; i++) { if (!h[i] || !M_IS(h[i])) ok = 0; }
    for (int i = 0; i < N && uniq; i++) for (int j = i + 1; j < N; j++) if (h[i] == h[j]) { uniq = 0; break; }
    CHECK(ok); CHECK(uniq);
    for (int i = 0; i < N; i++) if (h[i]) CHECK(M_DESTROY(h[i]));
    for (int i = 0; i < N; i++) CHECK(!M_IS(h[i]));
}

TEST(menus_create_repeated_cycle) {
    REQ_CORE();
    for (int i = 0; i < 2000; i++) {
        HMENU h = M_POP();
        if (!h) { CHECK(h != NULL); return; }
        if (!M_DESTROY(h)) { CHECK(0); return; }
    }
    CHECK(1);
}

TEST(menus_ismenu_bogus_handles) {
    REQ_CORE();
    for (int i = 0; i < MH_NBOGUS; i++)
        CHECKF(!M_IS(mh_bogus[i]), "bogus %d (%p) accepted", i, (void *)mh_bogus[i]);
}

TEST(menus_ismenu_other_object_types) {
    REQ_CORE();
    REQUIRE(U(GetDesktopWindow));
    CHECK(!M_IS((HMENU)U(GetDesktopWindow)()));
    HBITMAP bm = mh_bmp();
    CHECK(!M_IS((HMENU)bm));
    DeleteObject(bm);
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (w) { CHECK(!M_IS((HMENU)w)); U(DestroyWindow)(w); }
    HDC dc = GetDC(NULL);
    CHECK(!M_IS((HMENU)dc));
    ReleaseDC(NULL, dc);
}

TEST(menus_ismenu_does_not_set_error_for_valid) {
    REQ_CORE();
    HMENU h = M_POP();
    SetLastError(0xABCD);
    CHECK(M_IS(h));
    EQ(GetLastError(), 0xABCD);
    M_DESTROY(h);
}

TEST(menus_destroy_twice) {
    REQ_CORE();
    HMENU h = M_NEW();
    CHECK(M_DESTROY(h));
    SetLastError(0);
    CHECK(!M_DESTROY(h));
    DWORD e = GetLastError();
    if (e != ERROR_INVALID_MENU_HANDLE) t_note("DestroyMenu(destroyed) error %lu (expected 1401)", e);
}

TEST(menus_destroy_bogus) {
    REQ_CORE();
    for (int i = 0; i < MH_NBOGUS; i++) CHECKF(!M_DESTROY(mh_bogus[i]), "bogus %d", i);
}

TEST(menus_destroy_null_error) {
    REQ_CORE();
    SetLastError(0);
    CHECK(!M_DESTROY(NULL));
    DWORD e = GetLastError();
    if (e != ERROR_INVALID_MENU_HANDLE) t_note("DestroyMenu(NULL) error %lu (expected 1401)", e);
}

TEST(menus_destroy_recursive_submenus) {
    REQ_CORE();
    HMENU top = M_NEW(), s1 = M_POP(), s2 = M_POP(), s3 = M_POP();
    U(AppendMenuA)(s2, MF_POPUP, (UINT_PTR)s3, "L3");
    U(AppendMenuA)(s1, MF_POPUP, (UINT_PTR)s2, "L2");
    CHECK(U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s1, "L1"));
    CHECK(M_IS(s1)); CHECK(M_IS(s2)); CHECK(M_IS(s3));
    CHECK(M_DESTROY(top));
    CHECK(!M_IS(top)); CHECK(!M_IS(s1)); CHECK(!M_IS(s2)); CHECK(!M_IS(s3));
}

TEST(menus_destroy_submenu_first_then_parent) {
    REQ_CORE();
    HMENU top = M_NEW(), s = M_POP();
    CHECK(U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "S"));
    CHECK(M_DESTROY(s));
    CHECK(!M_IS(s));
    /* parent keeps (possibly dangling) item; it must still be destroyable */
    CHECK(M_IS(top));
    CHECK(M_DESTROY(top));
    CHECK(!M_IS(top));
}

TEST(menus_all_ops_on_destroyed_handle) {
    REQ_CORE();
    HMENU h = mh_make(3);
    CHECK(M_DESTROY(h));
    char b[16];
    EQ(M_CNT(h), -1);
    CHECK(!U(AppendMenuA)(h, MF_STRING, 1, "x"));
    CHECK(!U(AppendMenuW)(h, MF_STRING, 1, L"x"));
    CHECK(!U(InsertMenuA)(h, 0, MF_BYPOSITION | MF_STRING, 1, "x"));
    CHECK(!U(InsertMenuW)(h, 0, MF_BYPOSITION | MF_STRING, 1, L"x"));
    CHECK(!U(ModifyMenuA)(h, 0, MF_BYPOSITION | MF_STRING, 1, "x"));
    CHECK(!U(DeleteMenu)(h, 0, MF_BYPOSITION));
    CHECK(!U(RemoveMenu)(h, 0, MF_BYPOSITION));
    EQ(U(GetMenuItemID)(h, 0), 0xFFFFFFFF);
    CHECK(U(GetSubMenu)(h, 0) == NULL);
    EQ(U(GetMenuStringA)(h, 0, b, 16, MF_BYPOSITION), 0);
    EQ(U(GetMenuState)(h, 0, MF_BYPOSITION), 0xFFFFFFFF);
    EQ(U(CheckMenuItem)(h, 0, MF_BYPOSITION | MF_CHECKED), 0xFFFFFFFF);
    EQ(EMI(h, 0, MF_BYPOSITION | MF_GRAYED), 0xFFFFFFFF);
}

TEST(menus_all_ops_on_bogus_handles) {
    REQ_CORE();
    char b[16];
    for (int i = 0; i < MH_NBOGUS; i++) {
        HMENU h = mh_bogus[i];
        EQ(M_CNT(h), -1);
        CHECK(!U(AppendMenuA)(h, MF_STRING, 1, "x"));
        CHECK(!U(InsertMenuW)(h, 0, MF_BYPOSITION | MF_STRING, 1, L"x"));
        CHECK(!U(ModifyMenuW)(h, 0, MF_BYPOSITION | MF_STRING, 1, L"x"));
        CHECK(!U(DeleteMenu)(h, 0, MF_BYPOSITION));
        CHECK(!U(RemoveMenu)(h, 1, 0));
        EQ(U(GetMenuItemID)(h, 0), 0xFFFFFFFF);
        CHECK(U(GetSubMenu)(h, 0) == NULL);
        EQ(U(GetMenuStringW)(h, 0, (LPWSTR)b, 8, MF_BYPOSITION), 0);
        EQ(U(GetMenuState)(h, 0, MF_BYPOSITION), 0xFFFFFFFF);
        EQ(U(CheckMenuItem)(h, 0, MF_BYPOSITION), 0xFFFFFFFF);
        EQ(EMI(h, 0, MF_BYPOSITION), 0xFFFFFFFF);
    }
}

TEST(menus_getmenuitemcount_basic) {
    REQ_CORE();
    HMENU h = M_POP();
    for (int i = 0; i < 50; i++) {
        EQ(M_CNT(h), i);
        CHECK(mh_add(h, i + 1, "x"));
    }
    EQ(M_CNT(h), 50);
    M_DESTROY(h);
}

TEST(menus_append_separators_count) {
    REQ_CORE();
    HMENU h = M_POP();
    for (int i = 0; i < 5; i++) CHECK(U(AppendMenuA)(h, MF_SEPARATOR, 0, NULL));
    EQ(M_CNT(h), 5);
    for (int i = 0; i < 5; i++) CHECK(U(GetMenuState)(h, i, MF_BYPOSITION) & MF_SEPARATOR);
    M_DESTROY(h);
}

TEST(menus_append_string_roundtrip_a) {
    REQ_CORE();
    static const char *tab[] = { "File", "&Open", "E&xit\tAlt+F4", "", " ", "a b  c", "Trailing ", "&&Amp", "Very long item text with spaces and punctuation, ok!" };
    HMENU h = M_POP();
    for (int i = 0; i < (int)(sizeof tab / sizeof *tab); i++) CHECK(U(AppendMenuA)(h, MF_STRING, 10 + i, tab[i]));
    for (int i = 0; i < (int)(sizeof tab / sizeof *tab); i++) {
        char b[128] = "zz";
        int n = U(GetMenuStringA)(h, i, b, sizeof b, MF_BYPOSITION);
        EQ(n, (int)strlen(tab[i]));
        EXPECT_STR(b, tab[i]);
    }
    M_DESTROY(h);
}

TEST(menus_append_string_roundtrip_w) {
    REQ_CORE();
    static const WCHAR *tab[] = { L"File", L"&Open", L"éè中文", L"Жук", L"emoji \xd83d\xde00 pair", L"", L"x\ty" };
    HMENU h = M_POP();
    int n = (int)(sizeof tab / sizeof *tab);
    for (int i = 0; i < n; i++) CHECK(U(AppendMenuW)(h, MF_STRING, 10 + i, tab[i]));
    for (int i = 0; i < n; i++) {
        WCHAR b[64]; wmemset(b, 0x7777, 64);
        int r = U(GetMenuStringW)(h, 10 + i, b, 64, MF_BYCOMMAND);
        EQ(r, (int)lstrlenW(tab[i]));
        CHECK(!lstrcmpW(b, tab[i]));
    }
    M_DESTROY(h);
}

TEST(menus_unicode_ansi_parity_ascii) {
    REQ_CORE();
    HMENU h = M_POP();
    CHECK(U(AppendMenuW)(h, MF_STRING, 1, L"WideAdded"));
    CHECK(U(AppendMenuA)(h, MF_STRING, 2, "AnsiAdded"));
    char a[64]; WCHAR w[64];
    EQ(U(GetMenuStringA)(h, 1, a, 64, 0), 9); EXPECT_STR(a, "WideAdded");
    EQ(U(GetMenuStringW)(h, 1, w, 64, 0), 9); CHECK(!lstrcmpW(w, L"WideAdded"));
    EQ(U(GetMenuStringA)(h, 2, a, 64, 0), 9); EXPECT_STR(a, "AnsiAdded");
    EQ(U(GetMenuStringW)(h, 2, w, 64, 0), 9); CHECK(!lstrcmpW(w, L"AnsiAdded"));
    M_DESTROY(h);
}

TEST(menus_getmenustring_truncation_a) {
    REQ_CORE();
    HMENU h = M_POP(); mh_add(h, 7, "Hello");
    char b[16];
    memset(b, 'Q', sizeof b);
    EQ(U(GetMenuStringA)(h, 7, b, 1, 0), 0);  EQ(b[0], 0); EQ(b[1], 'Q');
    memset(b, 'Q', sizeof b);
    EQ(U(GetMenuStringA)(h, 7, b, 2, 0), 1);  EXPECT_STR(b, "H"); EQ(b[2], 'Q');
    memset(b, 'Q', sizeof b);
    EQ(U(GetMenuStringA)(h, 7, b, 5, 0), 4);  EXPECT_STR(b, "Hell"); EQ(b[5], 'Q');
    memset(b, 'Q', sizeof b);
    EQ(U(GetMenuStringA)(h, 7, b, 6, 0), 5);  EXPECT_STR(b, "Hello"); EQ(b[6], 'Q');
    memset(b, 'Q', sizeof b);
    EQ(U(GetMenuStringA)(h, 7, b, 7, 0), 5);  EXPECT_STR(b, "Hello");
    EQ(U(GetMenuStringA)(h, 7, b, 16, 0), 5);
    EQ(U(GetMenuStringA)(h, 7, NULL, 0, 0), 5);
    M_DESTROY(h);
}

TEST(menus_getmenustring_truncation_w) {
    REQ_CORE();
    HMENU h = M_POP(); U(AppendMenuW)(h, MF_STRING, 7, L"Hello");
    WCHAR b[16];
    wmemset(b, L'Q', 16);
    EQ(U(GetMenuStringW)(h, 7, b, 1, 0), 0);  EQ(b[0], 0); EQ(b[1], L'Q');
    wmemset(b, L'Q', 16);
    EQ(U(GetMenuStringW)(h, 7, b, 2, 0), 1);  CHECK(!lstrcmpW(b, L"H")); EQ(b[2], L'Q');
    wmemset(b, L'Q', 16);
    EQ(U(GetMenuStringW)(h, 7, b, 5, 0), 4);  CHECK(!lstrcmpW(b, L"Hell")); EQ(b[5], L'Q');
    wmemset(b, L'Q', 16);
    EQ(U(GetMenuStringW)(h, 7, b, 6, 0), 5);  CHECK(!lstrcmpW(b, L"Hello")); EQ(b[6], L'Q');
    EQ(U(GetMenuStringW)(h, 7, b, 16, 0), 5);
    EQ(U(GetMenuStringW)(h, 7, NULL, 0, 0), 5);
    M_DESTROY(h);
}

TEST(menus_getmenustring_nonstring_items) {
    REQ_CORE();
    HMENU h = M_POP(); HBITMAP bm = mh_bmp();
    U(AppendMenuA)(h, MF_SEPARATOR, 0, NULL);
    U(AppendMenuA)(h, MF_BITMAP, 5, (LPCSTR)bm);
    char b[16] = "keep";
    EQ(U(GetMenuStringA)(h, 0, b, 16, MF_BYPOSITION), 0);
    EQ(b[0], 0);
    strcpy(b, "keep");
    EQ(U(GetMenuStringA)(h, 1, b, 16, MF_BYPOSITION), 0);
    EQ(b[0], 0);
    EQ(U(GetMenuStringA)(h, 0, NULL, 0, MF_BYPOSITION), 0);
    M_DESTROY(h); DeleteObject(bm);
}

TEST(menus_getmenustring_missing_item) {
    REQ_CORE();
    HMENU h = mh_make(2);
    char b[16] = "keep";
    SetLastError(0);
    EQ(U(GetMenuStringA)(h, 99, b, 16, MF_BYCOMMAND), 0);
    EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    EQ(b[0], 0);
    SetLastError(0);
    EQ(U(GetMenuStringA)(h, 2, b, 16, MF_BYPOSITION), 0);
    EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    SetLastError(0);
    EQ(U(GetMenuStringA)(h, 0xFFFFFFFF, b, 16, MF_BYPOSITION), 0);
    EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    SetLastError(0);
    EQ(U(GetMenuStringA)(h, 0xFFFFFFFF, b, 16, MF_BYCOMMAND), 0);
    EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    M_DESTROY(h);
}

TEST(menus_getmenustring_invalid_menu_error) {
    REQ_CORE();
    char b[16];
    SetLastError(0);
    EQ(U(GetMenuStringA)(NULL, 0, b, 16, MF_BYPOSITION), 0);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}

TEST(menus_getmenustring_flags_other_bits_ignored) {
    REQ_CORE();
    HMENU h = mh_make(3);
    char b[16];
    /* only MF_BYPOSITION (0x400) is looked at */
    EQ(U(GetMenuStringA)(h, 1, b, 16, MF_BYPOSITION | MF_STRING | MF_CHECKED), 5);
    EXPECT_STR(b, "Item1");
    EQ(U(GetMenuStringA)(h, 101, b, 16, MF_STRING | MF_GRAYED), 5);
    EXPECT_STR(b, "Item1");
    M_DESTROY(h);
}

TEST(menus_getmenustring_bycommand_vs_byposition) {
    REQ_CORE();
    HMENU h = M_POP();
    mh_add(h, 1, "pos0-id1"); mh_add(h, 0, "pos1-id0"); mh_add(h, 2, "pos2-id2"); mh_add(h, 1, "pos3-id1");
    char b[32];
    U(GetMenuStringA)(h, 1, b, 32, MF_BYCOMMAND); EXPECT_STR(b, "pos0-id1");
    U(GetMenuStringA)(h, 1, b, 32, MF_BYPOSITION); EXPECT_STR(b, "pos1-id0");
    U(GetMenuStringA)(h, 0, b, 32, MF_BYCOMMAND); EXPECT_STR(b, "pos1-id0");
    U(GetMenuStringA)(h, 2, b, 32, MF_BYCOMMAND); EXPECT_STR(b, "pos2-id2");
    U(GetMenuStringA)(h, 3, b, 32, MF_BYPOSITION); EXPECT_STR(b, "pos3-id1");
    M_DESTROY(h);
}

TEST(menus_long_string_roundtrip) {
    REQ_CORE();
    enum { L = 1500 };
    char *s = malloc(L + 1), *o = malloc(L + 8);
    for (int i = 0; i < L; i++) s[i] = 'a' + (i % 26);
    s[L] = 0;
    HMENU h = M_POP();
    CHECK(U(AppendMenuA)(h, MF_STRING, 1, s));
    EQ(U(GetMenuStringA)(h, 1, o, L + 8, 0), L);
    CHECK(!strcmp(o, s));
    EQ(U(GetMenuStringA)(h, 1, NULL, 0, 0), L);
    M_DESTROY(h); free(s); free(o);
}

TEST(menus_append_flags_state_bits) {
    REQ_CORE();
    HMENU h = M_POP();
    CHECK(U(AppendMenuA)(h, MF_STRING | MF_CHECKED, 1, "c"));
    CHECK(U(AppendMenuA)(h, MF_STRING | MF_GRAYED, 2, "g"));
    CHECK(U(AppendMenuA)(h, MF_STRING | MF_DISABLED, 3, "d"));
    CHECK(U(AppendMenuA)(h, MF_STRING | MF_MENUBREAK, 4, "mb"));
    CHECK(U(AppendMenuA)(h, MF_STRING | MF_MENUBARBREAK, 5, "mbb"));
    CHECK(U(AppendMenuA)(h, MF_STRING | MF_HILITE, 6, "h"));
    CHECK(U(AppendMenuA)(h, MF_STRING | MF_USECHECKBITMAPS, 7, "r"));
    CHECK(U(AppendMenuA)(h, MF_STRING, 8, "plain"));
    CHECK(mh_state(h, 1, 0) & MF_CHECKED);
    CHECK(mh_state(h, 2, 0) & (MF_GRAYED | MF_DISABLED));
    CHECK(mh_state(h, 3, 0) & (MF_GRAYED | MF_DISABLED));
    CHECK(mh_state(h, 4, 0) & MF_MENUBREAK);
    CHECK(mh_state(h, 5, 0) & MF_MENUBARBREAK);
    CHECK(mh_state(h, 6, 0) & MF_HILITE);
    CHECK(mh_state(h, 7, 0) & MF_USECHECKBITMAPS);
    UINT p = mh_state(h, 8, 0);
    EQ(p & (MF_CHECKED | MF_GRAYED | MF_DISABLED | MF_HILITE | MF_MENUBREAK | MF_MENUBARBREAK | MF_POPUP | MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW), 0);
    CHECK(!(mh_state(h, 1, 0) & (MF_GRAYED | MF_DISABLED)));
    M_DESTROY(h);
}

TEST(menus_append_popup_item) {
    REQ_CORE();
    HMENU top = M_NEW(), s = M_POP();
    mh_add(s, 50, "child");
    CHECK(U(AppendMenuA)(top, MF_POPUP | MF_STRING, (UINT_PTR)s, "Parent"));
    EQ(M_CNT(top), 1);
    CHECK(U(GetSubMenu)(top, 0) == s);
    EQ(U(GetMenuItemID)(top, 0), 0xFFFFFFFF);
    UINT st = U(GetMenuState)(top, 0, MF_BYPOSITION);
    CHECK(st & MF_POPUP);
    EQ(st >> 8, 1);    /* high byte: number of items in the popup */
    char b[16];
    EQ(U(GetMenuStringA)(top, 0, b, 16, MF_BYPOSITION), 6); EXPECT_STR(b, "Parent");
    M_DESTROY(top);
}

TEST(menus_getmenustate_popup_count_in_high_byte) {
    REQ_CORE();
    HMENU top = M_NEW(), s = M_POP();
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "P");
    for (int i = 0; i < 7; i++) {
        EQ(U(GetMenuState)(top, 0, MF_BYPOSITION) >> 8, i);
        mh_add(s, 1 + i, "i");
    }
    EQ(U(GetMenuState)(top, 0, MF_BYPOSITION) >> 8, 7);
    M_DESTROY(top);
}

TEST(menus_getsubmenu_edges) {
    REQ_CORE();
    HMENU top = M_NEW(), s = M_POP();
    mh_add(top, 1, "plain");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "sub");
    CHECK(U(GetSubMenu)(top, 0) == NULL);
    CHECK(U(GetSubMenu)(top, 1) == s);
    CHECK(U(GetSubMenu)(top, 2) == NULL);
    CHECK(U(GetSubMenu)(top, -1) == NULL);
    CHECK(U(GetSubMenu)(top, 0x7FFFFFFF) == NULL);
    CHECK(U(GetSubMenu)(top, (int)0x80000000) == NULL);
    CHECK(U(GetSubMenu)(NULL, 0) == NULL);
    CHECK(U(GetSubMenu)(s, 0) == NULL);     /* empty submenu */
    M_DESTROY(top);
}

TEST(menus_getmenuitemid_edges) {
    REQ_CORE();
    HMENU h = M_POP(), s = M_POP();
    mh_add(h, 0, "zero"); mh_add(h, 77, "seventyseven"); mh_add(h, 0xFFFF, "ffff"); mh_add(h, 0x7FFFFFFF, "big");
    U(AppendMenuA)(h, MF_POPUP, (UINT_PTR)s, "sub");
    U(AppendMenuA)(h, MF_SEPARATOR, 0, NULL);
    EQ(U(GetMenuItemID)(h, 0), 0);
    EQ(U(GetMenuItemID)(h, 1), 77);
    EQ(U(GetMenuItemID)(h, 2), 0xFFFF);
    EQ(U(GetMenuItemID)(h, 3), 0x7FFFFFFF);
    EQ(U(GetMenuItemID)(h, 4), 0xFFFFFFFF);
    EQ(U(GetMenuItemID)(h, 5), 0);
    SetLastError(0);
    EQ(U(GetMenuItemID)(h, 6), 0xFFFFFFFF);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EQ(U(GetMenuItemID)(h, -1), 0xFFFFFFFF);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    EQ(U(GetMenuItemID)(h, 0x7FFFFFFF), 0xFFFFFFFF);
    EQ(U(GetMenuItemID)(h, (int)0x80000000), 0xFFFFFFFF);
    EQ(U(GetMenuItemID)(NULL, 0), 0xFFFFFFFF);
    M_DESTROY(h);
}

TEST(menus_command_id_ffffffff_not_findable) {
    REQ_CORE();
    HMENU h = M_POP();
    mh_add(h, 0xFFFFFFFF, "neg");
    EQ(U(GetMenuState)(h, 0xFFFFFFFF, MF_BYCOMMAND), 0xFFFFFFFF);
    char b[8];
    SetLastError(0);
    EQ(U(GetMenuStringA)(h, 0xFFFFFFFF, b, 8, MF_BYCOMMAND), 0);
    CHECK(!U(DeleteMenu)(h, 0xFFFFFFFF, MF_BYCOMMAND));
    EQ(M_CNT(h), 1);
    EQ(U(GetMenuItemID)(h, 0), 0xFFFFFFFF);
    M_DESTROY(h);
}

TEST(menus_getmenustate_missing) {
    REQ_CORE();
    HMENU h = mh_make(2);
    EQ(U(GetMenuState)(h, 999, MF_BYCOMMAND), 0xFFFFFFFF);
    EQ(U(GetMenuState)(h, 2, MF_BYPOSITION), 0xFFFFFFFF);
    EQ(U(GetMenuState)(h, 0xFFFFFFFF, MF_BYPOSITION), 0xFFFFFFFF);
    EQ(U(GetMenuState)(h, 0x80000000, MF_BYPOSITION), 0xFFFFFFFF);
    EQ(U(GetMenuState)(h, 100, MF_BYCOMMAND), 0);
    EQ(U(GetMenuState)(h, 1, MF_BYPOSITION), 0);
    M_DESTROY(h);
}

TEST(menus_getmenustate_high_flag_bits_invalid) {
    REQ_CORE();
    HMENU h = mh_make(2);
    SetLastError(0);
    U(GetMenuState)(h, 100, 0x10000);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    U(GetMenuState)(h, 100, 0x80000000);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    U(GetMenuState)(h, 100, 0xFFFF0000 | MF_BYPOSITION);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    U(GetMenuState)(h, 100, 0xFFFF);        /* low bits only: accepted */
    EQ(GetLastError(), 0);
    M_DESTROY(h);
}

TEST(menus_checkmenuitem_basic) {
    REQ_CORE();
    HMENU h = mh_make(3);
    EQ(U(CheckMenuItem)(h, 100, MF_BYCOMMAND | MF_CHECKED), MF_UNCHECKED);
    CHECK(mh_state(h, 100, 0) & MF_CHECKED);
    EQ(U(CheckMenuItem)(h, 100, MF_BYCOMMAND | MF_CHECKED), MF_CHECKED);
    EQ(U(CheckMenuItem)(h, 0, MF_BYPOSITION | MF_UNCHECKED), MF_CHECKED);
    CHECK(!(mh_state(h, 100, 0) & MF_CHECKED));
    EQ(U(CheckMenuItem)(h, 0, MF_BYPOSITION | MF_UNCHECKED), MF_UNCHECKED);
    EQ(U(CheckMenuItem)(h, 2, MF_BYPOSITION | MF_CHECKED), MF_UNCHECKED);
    CHECK(mh_state(h, 2, MF_BYPOSITION) & MF_CHECKED);
    CHECK(!(mh_state(h, 1, MF_BYPOSITION) & MF_CHECKED));
    M_DESTROY(h);
}

TEST(menus_checkmenuitem_missing_items) {
    REQ_CORE();
    HMENU h = mh_make(2);
    EQ(U(CheckMenuItem)(h, 5, MF_BYCOMMAND | MF_CHECKED), 0xFFFFFFFF);
    EQ(U(CheckMenuItem)(h, 2, MF_BYPOSITION | MF_CHECKED), 0xFFFFFFFF);
    EQ(U(CheckMenuItem)(h, 0xFFFFFFFF, MF_BYPOSITION | MF_CHECKED), 0xFFFFFFFF);
    EQ(U(CheckMenuItem)(NULL, 0, MF_BYPOSITION | MF_CHECKED), 0xFFFFFFFF);
    EQ(M_CNT(h), 2);
    M_DESTROY(h);
}

TEST(menus_checkmenuitem_only_bit_8_matters) {
    REQ_CORE();
    HMENU h = mh_make(2);
    /* MF_GRAYED is not a check state: item stays unchecked */
    U(CheckMenuItem)(h, 100, MF_BYCOMMAND | MF_GRAYED);
    CHECK(!(mh_state(h, 100, 0) & MF_CHECKED));
    EQ(U(CheckMenuItem)(h, 100, MF_BYCOMMAND | MF_CHECKED | MF_GRAYED), MF_UNCHECKED);
    CHECK(mh_state(h, 100, 0) & MF_CHECKED);
    M_DESTROY(h);
}

TEST(menus_enablemenuitem_basic) {
    REQ_CORE();
    HMENU h = mh_make(3);
    EQ(EMI(h, 100, MF_BYCOMMAND | MF_GRAYED), MF_ENABLED);
    CHECK(mh_state(h, 100, 0) & MF_GRAYED);
    CHECK(EMI(h, 100, MF_BYCOMMAND | MF_GRAYED) != 0xFFFFFFFF);
    CHECK(EMI(h, 100, MF_BYCOMMAND | MF_GRAYED) & MF_GRAYED);
    CHECK(EMI(h, 100, MF_BYCOMMAND | MF_ENABLED) & MF_GRAYED);
    EQ(mh_state(h, 100, 0) & (MF_GRAYED | MF_DISABLED), 0);
    EQ(EMI(h, 100, MF_BYCOMMAND | MF_ENABLED), MF_ENABLED);
    CHECK(EMI(h, 1, MF_BYPOSITION | MF_DISABLED) == MF_ENABLED);
    CHECK(mh_state(h, 101, 0) & (MF_DISABLED | MF_GRAYED));
    M_DESTROY(h);
}

TEST(menus_enablemenuitem_missing) {
    REQ_CORE();
    HMENU h = mh_make(1);
    EQ(EMI(h, 7, MF_BYCOMMAND | MF_GRAYED), 0xFFFFFFFF);
    EQ(EMI(h, 1, MF_BYPOSITION | MF_GRAYED), 0xFFFFFFFF);
    EQ(EMI(h, 0xFFFFFFFF, MF_BYCOMMAND), 0xFFFFFFFF);
    EQ(EMI(NULL, 0, MF_BYPOSITION), 0xFFFFFFFF);
    M_DESTROY(h);
}

TEST(menus_enable_in_nested_submenu_by_command) {
    REQ_CORE();
    HMENU top = M_NEW(), s1 = M_POP(), s2 = M_POP();
    mh_add(s2, 900, "deep");
    U(AppendMenuA)(s1, MF_POPUP, (UINT_PTR)s2, "L2");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s1, "L1");
    /* command-id lookup descends into popups */
    CHECK(mh_state(top, 900, 0) != 0xFFFFFFFF);
    EQ(EMI(top, 900, MF_GRAYED), MF_ENABLED);
    CHECK(mh_state(s2, 0, MF_BYPOSITION) & MF_GRAYED);
    EQ(U(CheckMenuItem)(top, 900, MF_CHECKED), MF_UNCHECKED);
    CHECK(mh_state(s2, 900, 0) & MF_CHECKED);
    char b[16];
    EQ(U(GetMenuStringA)(top, 900, b, 16, 0), 4); EXPECT_STR(b, "deep");
    /* by position is not recursive */
    EQ(mh_state(top, 1, MF_BYPOSITION), 0xFFFFFFFF);
    EQ(M_CNT(top), 1);
    M_DESTROY(top);
}

TEST(menus_deletemenu_bycommand_in_submenu) {
    REQ_CORE();
    HMENU top = M_NEW(), s = M_POP();
    mh_add(s, 41, "a"); mh_add(s, 42, "b");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "S");
    CHECK(U(DeleteMenu)(top, 41, MF_BYCOMMAND));
    EQ(M_CNT(s), 1);
    EQ(M_CNT(top), 1);
    EQ(U(GetMenuItemID)(s, 0), 42);
    M_DESTROY(top);
}

TEST(menus_insert_positions) {
    REQ_CORE();
    HMENU h = M_POP();
    U(InsertMenuA)(h, 0, MF_BYPOSITION | MF_STRING, 1, "A");   /* A */
    U(InsertMenuA)(h, 0, MF_BYPOSITION | MF_STRING, 2, "B");   /* B A */
    U(InsertMenuA)(h, 1, MF_BYPOSITION | MF_STRING, 3, "C");   /* B C A */
    U(InsertMenuA)(h, 3, MF_BYPOSITION | MF_STRING, 4, "D");   /* B C A D */
    U(InsertMenuA)(h, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, 5, "E"); /* B C A D E */
    EQ(M_CNT(h), 5);
    UINT want[5] = { 2, 3, 1, 4, 5 };
    for (int i = 0; i < 5; i++) EQ(U(GetMenuItemID)(h, i), want[i]);
    M_DESTROY(h);
}

TEST(menus_insert_before_command_item) {
    REQ_CORE();
    HMENU h = M_POP();
    mh_add(h, 1, "A"); mh_add(h, 2, "B");
    CHECK(U(InsertMenuA)(h, 2, MF_BYCOMMAND | MF_STRING, 3, "C"));  /* before B */
    UINT want[3] = { 1, 3, 2 };
    for (int i = 0; i < 3; i++) EQ(U(GetMenuItemID)(h, i), want[i]);
    CHECK(U(InsertMenuA)(h, 1, MF_BYCOMMAND | MF_STRING, 4, "D"));  /* before A */
    EQ(U(GetMenuItemID)(h, 0), 4);
    EQ(M_CNT(h), 4);
    M_DESTROY(h);
}

TEST(menus_insert_beyond_end_note) {
    REQ_CORE();
    HMENU h = mh_make(2);
    BOOL r = U(InsertMenuA)(h, 50, MF_BYPOSITION | MF_STRING, 9, "far");
    t_note("InsertMenu pos 50 on 2-item menu -> %d, count %d", r, M_CNT(h));
    BOOL r2 = U(InsertMenuA)(h, 77, MF_BYCOMMAND | MF_STRING, 10, "nocmd");
    t_note("InsertMenu BYCOMMAND missing id -> %d, count %d", r2, M_CNT(h));
    M_DESTROY(h);
}

TEST(menus_insert_w_parity) {
    REQ_CORE();
    HMENU h = M_POP();
    CHECK(U(InsertMenuW)(h, 0, MF_BYPOSITION | MF_STRING, 1, L"WideOne"));
    CHECK(U(InsertMenuW)(h, 0, MF_BYPOSITION | MF_STRING, 2, L"ÄÖÜ"));
    WCHAR w[16];
    EQ(U(GetMenuStringW)(h, 0, w, 16, MF_BYPOSITION), 3);
    CHECK(!lstrcmpW(w, L"ÄÖÜ"));
    EQ(U(GetMenuStringW)(h, 1, w, 16, MF_BYPOSITION), 7);
    M_DESTROY(h);
}

TEST(menus_modify_string_in_place) {
    REQ_CORE();
    HMENU h = mh_make(3);
    CHECK(U(ModifyMenuA)(h, 1, MF_BYPOSITION | MF_STRING, 555, "Changed"));
    char b[32];
    U(GetMenuStringA)(h, 1, b, 32, MF_BYPOSITION); EXPECT_STR(b, "Changed");
    EQ(U(GetMenuItemID)(h, 1), 555);
    EQ(M_CNT(h), 3);
    CHECK(U(ModifyMenuA)(h, 100, MF_BYCOMMAND | MF_STRING, 100, "ByCmd"));
    U(GetMenuStringA)(h, 0, b, 32, MF_BYPOSITION); EXPECT_STR(b, "ByCmd");
    CHECK(U(ModifyMenuW)(h, 2, MF_BYPOSITION | MF_STRING, 7, L"WideMod"));
    WCHAR w[32];
    EQ(U(GetMenuStringW)(h, 2, w, 32, MF_BYPOSITION), 7);
    M_DESTROY(h);
}

TEST(menus_modify_to_separator_and_back) {
    REQ_CORE();
    HMENU h = mh_make(2);
    CHECK(U(ModifyMenuA)(h, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL));
    CHECK(mh_state(h, 0, MF_BYPOSITION) & MF_SEPARATOR);
    char b[8] = "x";
    t_note("GetMenuString of item modified to separator -> %d", U(GetMenuStringA)(h, 0, b, 8, MF_BYPOSITION));
    CHECK(U(ModifyMenuA)(h, 0, MF_BYPOSITION | MF_STRING, 12, "again"));
    CHECK(!(mh_state(h, 0, MF_BYPOSITION) & MF_SEPARATOR));
    EQ(U(GetMenuStringA)(h, 0, b, 8, MF_BYPOSITION), 5);
    M_DESTROY(h);
}

TEST(menus_modify_missing_item) {
    REQ_CORE();
    HMENU h = mh_make(2);
    SetLastError(0);
    CHECK(!U(ModifyMenuA)(h, 2, MF_BYPOSITION | MF_STRING, 1, "x"));
    DWORD e1 = GetLastError();
    SetLastError(0);
    CHECK(!U(ModifyMenuA)(h, 999, MF_BYCOMMAND | MF_STRING, 1, "x"));
    DWORD e2 = GetLastError();
    CHECK(!U(ModifyMenuA)(h, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, 1, "x"));
    EQ(M_CNT(h), 2);
    t_note("ModifyMenu missing: pos err %lu, cmd err %lu", e1, e2);
    M_DESTROY(h);
}

TEST(menus_modify_preserves_state_flags_note) {
    REQ_CORE();
    HMENU h = mh_make(1);
    U(CheckMenuItem)(h, 100, MF_CHECKED);
    EMI(h, 100, MF_GRAYED);
    CHECK(U(ModifyMenuA)(h, 100, MF_BYCOMMAND | MF_STRING, 100, "mod"));
    /* the mask used by ModifyMenu includes MIIM_STATE, so the old state is replaced */
    EQ(mh_state(h, 100, 0) & (MF_CHECKED | MF_GRAYED | MF_DISABLED), 0);
    CHECK(U(ModifyMenuA)(h, 100, MF_BYCOMMAND | MF_STRING | MF_CHECKED | MF_GRAYED, 100, "mod2"));
    UINT st = mh_state(h, 100, 0);
    CHECK(st & MF_CHECKED); CHECK(st & (MF_GRAYED | MF_DISABLED));
    M_DESTROY(h);
}

TEST(menus_modify_popup_item_to_string_note) {
    REQ_CORE();
    HMENU top = M_NEW(), s = M_POP();
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "S");
    CHECK(U(ModifyMenuA)(top, 0, MF_BYPOSITION | MF_STRING, 5, "plain"));
    t_note("GetSubMenu after ModifyMenu(MF_STRING) of popup item: %p", (void *)U(GetSubMenu)(top, 0));
    EQ(M_CNT(top), 1);
    t_note("IsMenu(old submenu) after ModifyMenu replaced popup item: %d", M_IS(s));
    M_DESTROY(top); if (M_IS(s)) M_DESTROY(s);
}

TEST(menus_deletemenu_item_basic) {
    REQ_CORE();
    HMENU h = mh_make(4);
    CHECK(U(DeleteMenu)(h, 101, MF_BYCOMMAND));
    EQ(M_CNT(h), 3);
    EQ(U(GetMenuState)(h, 101, 0), 0xFFFFFFFF);
    CHECK(U(DeleteMenu)(h, 0, MF_BYPOSITION));
    EQ(U(GetMenuItemID)(h, 0), 102);
    CHECK(U(DeleteMenu)(h, 1, MF_BYPOSITION));
    EQ(M_CNT(h), 1);
    CHECK(U(DeleteMenu)(h, 0, MF_BYPOSITION));
    EQ(M_CNT(h), 0);
    CHECK(!U(DeleteMenu)(h, 0, MF_BYPOSITION));
    CHECK(M_IS(h));
    M_DESTROY(h);
}

TEST(menus_deletemenu_missing_items) {
    REQ_CORE();
    HMENU h = mh_make(2);
    CHECK(!U(DeleteMenu)(h, 2, MF_BYPOSITION));
    CHECK(!U(DeleteMenu)(h, 0xFFFFFFFF, MF_BYPOSITION));
    CHECK(!U(DeleteMenu)(h, 55, MF_BYCOMMAND));
    CHECK(!U(DeleteMenu)(NULL, 0, MF_BYPOSITION));
    EQ(M_CNT(h), 2);
    M_DESTROY(h);
}

TEST(menus_deletemenu_destroys_submenu) {
    REQ_CORE();
    HMENU top = M_NEW(), s = M_POP(), s2 = M_POP();
    U(AppendMenuA)(s, MF_POPUP, (UINT_PTR)s2, "inner");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "outer");
    CHECK(U(DeleteMenu)(top, 0, MF_BYPOSITION));
    CHECK(!M_IS(s)); CHECK(!M_IS(s2));
    EQ(M_CNT(top), 0);
    M_DESTROY(top);
}

TEST(menus_removemenu_keeps_submenu) {
    REQ_CORE();
    HMENU top = M_NEW(), s = M_POP();
    mh_add(s, 1, "k");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "outer");
    CHECK(U(RemoveMenu)(top, 0, MF_BYPOSITION));
    CHECK(M_IS(s));
    EQ(M_CNT(top), 0);
    EQ(M_CNT(s), 1);
    CHECK(M_DESTROY(s));
    M_DESTROY(top);
}

TEST(menus_removemenu_by_command_and_missing) {
    REQ_CORE();
    HMENU h = mh_make(3);
    CHECK(U(RemoveMenu)(h, 101, MF_BYCOMMAND));
    EQ(M_CNT(h), 2);
    CHECK(!U(RemoveMenu)(h, 101, MF_BYCOMMAND));
    CHECK(!U(RemoveMenu)(h, 2, MF_BYPOSITION));
    CHECK(!U(RemoveMenu)(h, 0xFFFFFFFF, MF_BYPOSITION));
    CHECK(U(RemoveMenu)(h, 1, MF_BYPOSITION));
    EQ(U(GetMenuItemID)(h, 0), 100);
    M_DESTROY(h);
}

TEST(menus_submenu_shared_second_parent_note) {
    REQ_CORE();
    HMENU a = M_NEW(), b = M_NEW(), s = M_POP();
    CHECK(U(AppendMenuA)(a, MF_POPUP, (UINT_PTR)s, "S"));
    BOOL r = U(AppendMenuA)(b, MF_POPUP, (UINT_PTR)s, "S2");
    t_note("append same popup to second parent -> %d", r);
    M_DESTROY(a);
    t_note("submenu alive after first parent destroyed: %d", M_IS(s));
    M_DESTROY(b);
    CHECK(!M_IS(s));
}

TEST(menus_append_invalid_flags_combo_note) {
    REQ_CORE();
    HMENU h = M_POP();
    BOOL r = U(AppendMenuA)(h, MF_POPUP, (UINT_PTR)0xDEADBEEF, "badpopup");
    t_note("AppendMenu MF_POPUP bogus handle -> %d (count %d)", r, M_CNT(h));
    r = U(AppendMenuA)(h, MF_POPUP, 0, "nullpopup");
    t_note("AppendMenu MF_POPUP NULL handle -> %d (count %d)", r, M_CNT(h));
    M_DESTROY(h);
}

TEST(menus_append_bitmap_item) {
    REQ_CORE();
    HMENU h = M_POP(); HBITMAP bm = mh_bmp();
    CHECK(U(AppendMenuA)(h, MF_BITMAP, 31, (LPCSTR)bm));
    EQ(M_CNT(h), 1);
    CHECK(mh_state(h, 31, 0) & MF_BITMAP);
    EQ(U(GetMenuItemID)(h, 0), 31);
    M_DESTROY(h); DeleteObject(bm);
}

TEST(menus_append_ownerdraw_data) {
    REQ_CORE();
    REQUIRE(U(GetMenuItemInfoA));
    HMENU h = M_POP();
    ULONG_PTR data = (ULONG_PTR)0x123456789ABCDEF0ULL;
    CHECK(U(AppendMenuA)(h, MF_OWNERDRAW, 8, (LPCSTR)data));
    CHECK(mh_state(h, 8, 0) & MF_OWNERDRAW);
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_DATA | MIIM_ID | MIIM_FTYPE);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(mi.dwItemData, data);
    EQ(mi.wID, 8);
    CHECK(mi.fType & MFT_OWNERDRAW);
    M_DESTROY(h);
}

TEST(menus_item_limit_many_items) {
    REQ_CORE();
    HMENU h = M_POP();
    int n = 3000, ok = 0;
    /* Win10 22H2 oracle guest refused further items after 1168 (desktop heap in that session), so
     * append until the first refusal and exercise whatever count was reached. */
    for (int i = 0; i < n; i++) { if (!U(AppendMenuA)(h, MF_STRING, i + 1, "item")) break; ok++; }
    t_note("menu accepted %d items before AppendMenu refused (err %lu)", ok, GetLastError());
    CHECK(ok >= 1000);
    n = ok;
    EQ(M_CNT(h), n);
    EQ(U(GetMenuItemID)(h, n - 1), n);
    EQ(U(GetMenuItemID)(h, 999), 1000);
    for (int i = 0; i < n; i++) if (!U(DeleteMenu)(h, 0, MF_BYPOSITION)) { CHECK(0); break; }
    EQ(M_CNT(h), 0);
    M_DESTROY(h);
}

TEST(menus_duplicate_ids_first_wins) {
    REQ_CORE();
    HMENU h = M_POP();
    mh_add(h, 5, "first"); mh_add(h, 5, "second"); mh_add(h, 5, "third");
    char b[16];
    U(GetMenuStringA)(h, 5, b, 16, MF_BYCOMMAND); EXPECT_STR(b, "first");
    CHECK(U(DeleteMenu)(h, 5, MF_BYCOMMAND));
    U(GetMenuStringA)(h, 5, b, 16, MF_BYCOMMAND); EXPECT_STR(b, "second");
    U(CheckMenuItem)(h, 5, MF_CHECKED);
    CHECK(mh_state(h, 0, MF_BYPOSITION) & MF_CHECKED);
    CHECK(!(mh_state(h, 1, MF_BYPOSITION) & MF_CHECKED));
    M_DESTROY(h);
}

TEST(menus_flag_by_position_constants) {
    REQ_CORE();
    EQ(MF_BYCOMMAND, 0); EQ(MF_BYPOSITION, 0x400);
    EQ(MF_GRAYED, 1); EQ(MF_DISABLED, 2); EQ(MF_CHECKED, 8);
    EQ(MF_POPUP, 0x10); EQ(MF_SEPARATOR, 0x800); EQ(MF_DEFAULT, 0x1000);
    EQ(MF_HILITE, 0x80); EQ(MF_OWNERDRAW, 0x100); EQ(MF_BITMAP, 4);
}

TEST(menus_cross_thread_use) {
    REQ_CORE();
    struct Ctx { HMENU h; } c = { 0 };
    DWORD WINAPI th(LPVOID p) { ((struct Ctx *)p)->h = M_POP(); U(AppendMenuA)(((struct Ctx *)p)->h, MF_STRING, 1, "fromthread"); return 0; }
    HANDLE t = CreateThread(NULL, 0, th, &c, 0, NULL);
    CHECK(t != NULL);
    if (!t) return;
    CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
    CloseHandle(t);
    CHECK(c.h != NULL);
    t_note("menu created by exited thread still valid: %d", M_IS(c.h));
    if (M_IS(c.h)) {
        CHECK(mh_add(c.h, 2, "mainthread"));
        EQ(M_CNT(c.h), 2);
        char b[16]; U(GetMenuStringA)(c.h, 1, b, 16, 0); EXPECT_STR(b, "fromthread");
        CHECK(M_DESTROY(c.h));
    }
}

TEST(menus_cross_thread_destroy) {
    REQ_CORE();
    static HMENU g_h;
    DWORD WINAPI th(LPVOID p) { U(DestroyMenu)(g_h); return 0; }
    g_h = M_POP();
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    if (!t) { CHECK(0); return; }
    CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
    CloseHandle(t);
    t_note("DestroyMenu from non-creating thread -> menu gone: %d", !M_IS(g_h));
    if (M_IS(g_h)) M_DESTROY(g_h);
}
