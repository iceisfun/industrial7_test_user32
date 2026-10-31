/* Additional menu edge-case tests. */
#include "mh.h"

TEST(menus_extra_bogus_isMenu_after_destroy) {
    REQ_CORE();
    HMENU h = M_POP();
    CHECK(h && M_IS(h));
    CHECK(M_DESTROY(h));
    CHECK(!M_IS(h));
    SetLastError(0);
    CHECK(!M_DESTROY(h));
    CHECK(M_CNT(h) == -1);
}

TEST(menus_extra_count_and_ids_roundtrip) {
    REQ_CORE();
    HMENU h = M_POP();
    for (int i = 0; i < 50; i++) CHECK(U(AppendMenuA)(h, MF_STRING, 1000 + i, "x"));
    EQ(M_CNT(h), 50);
    for (int i = 0; i < 50; i++) EQ(U(GetMenuItemID)(h, i), 1000 + i);
    for (int i = 0; i < 25; i++) CHECK(U(DeleteMenu)(h, 0, MF_BYPOSITION));
    EQ(M_CNT(h), 25);
    EQ(U(GetMenuItemID)(h, 0), 1025);
    M_DESTROY(h);
}

TEST(menus_extra_string_truncation) {
    REQ_CORE();
    HMENU h = M_POP();
    char b[16];
    U(AppendMenuA)(h, MF_STRING, 5, "Hello");
    EQ(U(GetMenuStringA)(h, 5, NULL, 0, MF_BYCOMMAND), 5);
    memset(b, 'Z', sizeof b);
    EQ(U(GetMenuStringA)(h, 5, b, 6, MF_BYCOMMAND), 5);
    CHECK(strcmp(b, "Hello") == 0);
    memset(b, 'Z', sizeof b);
    EQ(U(GetMenuStringA)(h, 5, b, 3, MF_BYCOMMAND), 2);
    CHECK(strcmp(b, "He") == 0);
    M_DESTROY(h);
}

TEST(menus_extra_checkitem_return) {
    REQ_CORE();
    HMENU h = M_POP();
    U(AppendMenuA)(h, MF_STRING, 7, "a");
    EQ(U(CheckMenuItem)(h, 7, MF_BYCOMMAND | MF_CHECKED), MF_UNCHECKED);
    EQ(U(CheckMenuItem)(h, 7, MF_BYCOMMAND | MF_UNCHECKED), MF_CHECKED);
    EQ((UINT)U(CheckMenuItem)(h, 99, MF_BYCOMMAND | MF_CHECKED), 0xFFFFFFFFu);
    EQ((UINT)U(EnableMenuItem)(h, 99, MF_BYCOMMAND | MF_GRAYED), 0xFFFFFFFFu);
    EQ((UINT)U(EnableMenuItem)(h, 7, MF_BYCOMMAND | MF_GRAYED), MF_ENABLED);
    EQ((UINT)U(EnableMenuItem)(h, 7, MF_BYCOMMAND | MF_ENABLED), MF_GRAYED);
    M_DESTROY(h);
}

TEST(menus_extra_popup_submenu_destroyed_with_parent) {
    REQ_CORE();
    HMENU top = M_NEW(), sub = M_POP();
    U(AppendMenuA)(top, MF_POPUP | MF_STRING, (UINT_PTR)sub, "sub");
    EQ(U(GetSubMenu)(top, 0), sub);
    CHECK(M_DESTROY(top));
    CHECK(!M_IS(top));
    CHECK(!M_IS(sub));
}

TEST(menus_extra_removemenu_keeps_submenu) {
    REQ_CORE();
    HMENU top = M_NEW(), sub = M_POP();
    U(AppendMenuA)(top, MF_POPUP | MF_STRING, (UINT_PTR)sub, "sub");
    CHECK(U(RemoveMenu)(top, 0, MF_BYPOSITION));
    CHECK(M_IS(sub));
    M_DESTROY(top);
    CHECK(M_IS(sub));
    M_DESTROY(sub);
}

TEST(menus_extra_separator_state) {
    REQ_CORE();
    HMENU h = M_POP();
    U(AppendMenuA)(h, MF_SEPARATOR, 0, NULL);
    UINT s = U(GetMenuState)(h, 0, MF_BYPOSITION);
    CHECK(s != 0xFFFFFFFFu);
    CHECK(s & MF_SEPARATOR);
    EQ(U(GetMenuItemID)(h, 0), 0);
    M_DESTROY(h);
}

TEST(menus_extra_checkradio) {
    REQ_CORE();
    if (!U(CheckMenuRadioItem)) { t_skip("no CheckMenuRadioItem"); return; }
    HMENU h = mh_make(4);
    CHECK(U(CheckMenuRadioItem)(h, 100, 103, 102, MF_BYCOMMAND));
    CHECK(mh_state(h, 102, MF_BYCOMMAND) & MF_CHECKED);
    CHECK(!(mh_state(h, 101, MF_BYCOMMAND) & MF_CHECKED));
    CHECK(U(CheckMenuRadioItem)(h, 0, 3, 1, MF_BYPOSITION));
    CHECK(mh_state(h, 1, MF_BYPOSITION) & MF_CHECKED);
    CHECK(!(mh_state(h, 2, MF_BYPOSITION) & MF_CHECKED));
    M_DESTROY(h);
}

TEST(menus_extra_defaultitem) {
    REQ_CORE();
    if (!U(SetMenuDefaultItem) || !U(GetMenuDefaultItem)) { t_skip("no default item"); return; }
    HMENU h = mh_make(3);
    EQ(U(GetMenuDefaultItem)(h, FALSE, 0), 0xFFFFFFFFu);
    CHECK(U(SetMenuDefaultItem)(h, 1, TRUE));
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 1);
    EQ(U(GetMenuDefaultItem)(h, FALSE, 0), 101);
    CHECK(U(SetMenuDefaultItem)(h, 0xFFFFFFFF, TRUE));
    EQ(U(GetMenuDefaultItem)(h, FALSE, 0), 0xFFFFFFFFu);
    M_DESTROY(h);
}
