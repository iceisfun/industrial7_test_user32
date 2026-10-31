/* menus: MENUINFO, default item, radio check, bitmaps, window menus, system menu, bar info */
#include "mh.h"

#define REQ_MI() do { if (!(U(GetMenuInfo) && U(SetMenuInfo) && U(CreatePopupMenu) && U(CreateMenu) && U(DestroyMenu) && U(AppendMenuA) && U(IsMenu))) { t_skip("missing menuinfo exports"); return; } } while (0)
#define ALLMIM (MIM_MAXHEIGHT | MIM_BACKGROUND | MIM_HELPID | MIM_MENUDATA | MIM_STYLE)

static void mi_init(MENUINFO *mi, DWORD mask) { memset(mi, 0xCC, sizeof *mi); mi->cbSize = sizeof *mi; mi->fMask = mask; }
static void mi_zero(MENUINFO *mi, DWORD mask) { memset(mi, 0, sizeof *mi); mi->cbSize = sizeof *mi; mi->fMask = mask; }

TEST(menus_info_defaults_new_menu) {
    REQ_MI();
    for (int pop = 0; pop < 2; pop++) {
        HMENU h = pop ? M_POP() : M_NEW();
        MENUINFO mi; mi_init(&mi, ALLMIM);
        CHECK(U(GetMenuInfo)(h, &mi));
        EQ(mi.cbSize, 40); EQ(mi.fMask, ALLMIM);
        EQ(mi.cyMax, 0); CHECK(mi.hbrBack == NULL); EQ(mi.dwContextHelpID, 0); EQ(mi.dwMenuData, 0);
        if (mi.dwStyle != 0) t_note("default dwStyle of new %s menu: 0x%lx", pop ? "popup" : "bar", mi.dwStyle);
        M_DESTROY(h);
    }
}

TEST(menus_info_cbsize_invalid) {
    REQ_MI();
    static const DWORD bad[] = { 0, 4, 0x10, 0x20, 0x24, 0x2C, 0x30, 0x50, 0x100, 0x7FFFFFFF, 0xFFFFFFFF };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUINFO mi; mi_zero(&mi, MIM_MAXHEIGHT); mi.cbSize = bad[i];
        SetLastError(0);
        CHECKF(!U(GetMenuInfo)(h, &mi), "get cbSize %lu", bad[i]); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        SetLastError(0);
        CHECKF(!U(SetMenuInfo)(h, &mi), "set cbSize %lu", bad[i]); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    }
    M_DESTROY(h);
}

TEST(menus_info_null_struct) {
    REQ_MI();
    HMENU h = M_POP();
    SetLastError(0);
    CHECK(!U(GetMenuInfo)(h, NULL)); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(SetMenuInfo)(h, NULL)); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    M_DESTROY(h);
}

TEST(menus_info_mask_invalid_bits) {
    REQ_MI();
    static const DWORD bad[] = { 0x20, 0x40, 0x80, 0x100, 0x1000, 0x10000, 0x40000000, 0x7FFFFFE0, 0x3F, 0x7FFFFFFF };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUINFO mi; mi_zero(&mi, bad[i]);
        SetLastError(0);
        CHECKF(!U(GetMenuInfo)(h, &mi), "get mask 0x%lx", bad[i]); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        SetLastError(0);
        CHECKF(!U(SetMenuInfo)(h, &mi), "set mask 0x%lx", bad[i]); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    }
    M_DESTROY(h);
}

TEST(menus_info_mask_zero_and_apply_only) {
    REQ_MI();
    HMENU h = M_POP();
    MENUINFO mi; mi_init(&mi, 0);
    CHECK(U(GetMenuInfo)(h, &mi));
    EQ(mi.cyMax, 0xCCCCCCCC); EQ(mi.dwMenuData, 0xCCCCCCCCCCCCCCCCULL);
    mi_zero(&mi, 0);
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_zero(&mi, MIM_APPLYTOSUBMENUS);
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_init(&mi, MIM_APPLYTOSUBMENUS);
    CHECK(U(GetMenuInfo)(h, &mi));
    M_DESTROY(h);
}

TEST(menus_info_invalid_menu_handles) {
    REQ_MI();
    for (int i = 0; i < MH_NBOGUS; i++) {
        MENUINFO mi; mi_zero(&mi, MIM_MAXHEIGHT);
        CHECKF(!U(GetMenuInfo)(mh_bogus[i], &mi), "get bogus %d", i);
        CHECKF(!U(SetMenuInfo)(mh_bogus[i], &mi), "set bogus %d", i);
    }
    HMENU h = M_POP(); M_DESTROY(h);
    MENUINFO mi; mi_zero(&mi, MIM_MAXHEIGHT);
    CHECK(!U(GetMenuInfo)(h, &mi)); CHECK(!U(SetMenuInfo)(h, &mi));
}

TEST(menus_info_maxheight_roundtrip) {
    REQ_MI();
    static const UINT v[] = { 0, 1, 16, 100, 300, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        MENUINFO mi; mi_zero(&mi, MIM_MAXHEIGHT); mi.cyMax = v[i];
        CHECKF(U(SetMenuInfo)(h, &mi), "set cyMax %u", v[i]);
        mi_init(&mi, MIM_MAXHEIGHT);
        CHECK(U(GetMenuInfo)(h, &mi));
        CHECKF(mi.cyMax == v[i], "cyMax %u -> %u", v[i], mi.cyMax);
        EQ(mi.fMask, MIM_MAXHEIGHT);
        EQ(mi.dwMenuData, 0xCCCCCCCCCCCCCCCCULL);     /* unrequested fields untouched */
        EQ(mi.dwContextHelpID, 0xCCCCCCCC);
    }
    M_DESTROY(h);
}

TEST(menus_info_menudata_roundtrip) {
    REQ_MI();
    static const ULONG_PTR v[] = { 0, 1, 0xFFFFFFFF, 0x100000000ULL, 0x123456789ABCDEF0ULL, (ULONG_PTR)-1 };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        MENUINFO mi; mi_zero(&mi, MIM_MENUDATA); mi.dwMenuData = v[i];
        CHECK(U(SetMenuInfo)(h, &mi));
        mi_init(&mi, MIM_MENUDATA);
        CHECK(U(GetMenuInfo)(h, &mi));
        CHECKF(mi.dwMenuData == v[i], "menudata %llx -> %llx", (unsigned long long)v[i], (unsigned long long)mi.dwMenuData);
    }
    M_DESTROY(h);
}

TEST(menus_info_helpid_roundtrip_and_contexthelp_api) {
    REQ_MI();
    REQUIRE(U(GetMenuContextHelpId)); REQUIRE(U(SetMenuContextHelpId));
    HMENU h = M_POP();
    MENUINFO mi; mi_zero(&mi, MIM_HELPID); mi.dwContextHelpID = 0x1234;
    CHECK(U(SetMenuInfo)(h, &mi));
    EQ(U(GetMenuContextHelpId)(h), 0x1234);
    CHECK(U(SetMenuContextHelpId)(h, 0xFEDC));
    mi_init(&mi, MIM_HELPID);
    CHECK(U(GetMenuInfo)(h, &mi)); EQ(mi.dwContextHelpID, 0xFEDC);
    CHECK(U(SetMenuContextHelpId)(h, 0));
    EQ(U(GetMenuContextHelpId)(h), 0);
    CHECK(U(SetMenuContextHelpId)(h, 0xFFFFFFFF));
    EQ(U(GetMenuContextHelpId)(h), 0xFFFFFFFF);
    CHECK(!U(SetMenuContextHelpId)(NULL, 1));
    EQ(U(GetMenuContextHelpId)(NULL), 0);
    EQ(U(GetMenuContextHelpId)((HMENU)0xDEADBEEF), 0);
    M_DESTROY(h);
    EQ(U(GetMenuContextHelpId)(h), 0);
}

TEST(menus_info_background_brush) {
    REQ_MI();
    HMENU h = M_POP();
    HBRUSH br = CreateSolidBrush(RGB(10, 20, 30));
    MENUINFO mi; mi_zero(&mi, MIM_BACKGROUND); mi.hbrBack = br;
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_init(&mi, MIM_BACKGROUND);
    CHECK(U(GetMenuInfo)(h, &mi)); CHECK(mi.hbrBack == br);
    HBRUSH st = (HBRUSH)GetStockObject(WHITE_BRUSH);
    mi_zero(&mi, MIM_BACKGROUND); mi.hbrBack = st;
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_init(&mi, MIM_BACKGROUND);
    CHECK(U(GetMenuInfo)(h, &mi)); CHECK(mi.hbrBack == st);
    mi_zero(&mi, MIM_BACKGROUND); mi.hbrBack = NULL;
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_init(&mi, MIM_BACKGROUND);
    CHECK(U(GetMenuInfo)(h, &mi)); CHECK(mi.hbrBack == NULL);
    M_DESTROY(h); DeleteObject(br);
}

TEST(menus_info_background_invalid_brush) {
    REQ_MI();
    static const ULONG_PTR bad[] = { 1, 0x1234567, 0xDEADBEEF, 0xFFFFFFFF, (ULONG_PTR)-1 };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUINFO mi; mi_zero(&mi, MIM_BACKGROUND); mi.hbrBack = (HBRUSH)bad[i];
        SetLastError(0);
        CHECKF(!U(SetMenuInfo)(h, &mi), "bad brush %llx", (unsigned long long)bad[i]);
        EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        mi_zero(&mi, MIM_MAXHEIGHT); mi.hbrBack = (HBRUSH)bad[i]; mi.cyMax = 10;   /* not looked at */
        CHECK(U(SetMenuInfo)(h, &mi));
    }
    M_DESTROY(h);
}

TEST(menus_info_style_roundtrip) {
    REQ_MI();
    static const DWORD st[] = { 0, MNS_NOCHECK, MNS_MODELESS, MNS_DRAGDROP, MNS_AUTODISMISS, MNS_NOTIFYBYPOS, MNS_CHECKORBMP,
                                MNS_NOCHECK | MNS_NOTIFYBYPOS, 0xFC000000 };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof st / sizeof *st; i++) {
        MENUINFO mi; mi_zero(&mi, MIM_STYLE); mi.dwStyle = st[i];
        CHECKF(U(SetMenuInfo)(h, &mi), "style 0x%lx", st[i]);
        mi_init(&mi, MIM_STYLE);
        CHECK(U(GetMenuInfo)(h, &mi));
        CHECKF(mi.dwStyle == st[i], "style 0x%lx -> 0x%lx", st[i], mi.dwStyle);
    }
    M_DESTROY(h);
}

TEST(menus_info_style_invalid_low_bits) {
    REQ_MI();
    static const DWORD bad[] = { 1, 0x100, 0x10000, 0x3FFFFFF, 0x2000000, 0x80000001, 0xFFFFFFFF };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUINFO mi; mi_zero(&mi, MIM_STYLE); mi.dwStyle = bad[i];
        SetLastError(0);
        CHECKF(!U(SetMenuInfo)(h, &mi), "style 0x%lx accepted", bad[i]); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        mi_zero(&mi, MIM_MAXHEIGHT); mi.dwStyle = bad[i];      /* ignored without MIM_STYLE */
        CHECK(U(SetMenuInfo)(h, &mi));
    }
    M_DESTROY(h);
}

TEST(menus_info_get_style_only_high_bits) {
    REQ_MI();
    HMENU h = M_POP();
    MENUINFO mi; mi_zero(&mi, MIM_STYLE); mi.dwStyle = MNS_CHECKORBMP;
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_init(&mi, MIM_STYLE);
    CHECK(U(GetMenuInfo)(h, &mi));
    EQ(mi.dwStyle & 0x03FFFFFF, 0);
    EQ(mi.dwStyle & 0xFC000000, MNS_CHECKORBMP);
    M_DESTROY(h);
}

TEST(menus_info_combined_mask) {
    REQ_MI();
    HMENU h = M_POP();
    MENUINFO mi; mi_zero(&mi, ALLMIM); mi.cyMax = 77; mi.dwContextHelpID = 88; mi.dwMenuData = 99; mi.dwStyle = MNS_NOTIFYBYPOS; mi.hbrBack = NULL;
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_init(&mi, ALLMIM);
    CHECK(U(GetMenuInfo)(h, &mi));
    EQ(mi.cyMax, 77); EQ(mi.dwContextHelpID, 88); EQ(mi.dwMenuData, 99); EQ(mi.dwStyle, MNS_NOTIFYBYPOS); EQ(mi.hbrBack, 0);
    /* partial set leaves others */
    mi_zero(&mi, MIM_MAXHEIGHT); mi.cyMax = 5;
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_init(&mi, ALLMIM);
    CHECK(U(GetMenuInfo)(h, &mi));
    EQ(mi.cyMax, 5); EQ(mi.dwContextHelpID, 88); EQ(mi.dwMenuData, 99); EQ(mi.dwStyle, MNS_NOTIFYBYPOS);
    M_DESTROY(h);
}

TEST(menus_info_apply_to_submenus) {
    REQ_MI();
    HMENU top = M_NEW(), s1 = M_POP(), s2 = M_POP(), other = M_POP();
    U(AppendMenuA)(s1, MF_POPUP, (UINT_PTR)s2, "s2");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s1, "s1");
    MENUINFO mi; mi_zero(&mi, MIM_STYLE | MIM_MAXHEIGHT); mi.dwStyle = MNS_NOCHECK; mi.cyMax = 123;
    CHECK(U(SetMenuInfo)(top, &mi));
    mi_init(&mi, MIM_STYLE | MIM_MAXHEIGHT);
    CHECK(U(GetMenuInfo)(s1, &mi)); EQ(mi.dwStyle, 0); EQ(mi.cyMax, 0);
    mi_zero(&mi, MIM_STYLE | MIM_MAXHEIGHT | MIM_APPLYTOSUBMENUS); mi.dwStyle = MNS_NOTIFYBYPOS; mi.cyMax = 321;
    CHECK(U(SetMenuInfo)(top, &mi));
    MENUINFO g;
    mi_init(&g, MIM_STYLE | MIM_MAXHEIGHT);
    CHECK(U(GetMenuInfo)(top, &g)); EQ(g.dwStyle, MNS_NOTIFYBYPOS); EQ(g.cyMax, 321);
    mi_init(&g, MIM_STYLE | MIM_MAXHEIGHT);
    CHECK(U(GetMenuInfo)(s1, &g)); EQ(g.dwStyle, MNS_NOTIFYBYPOS); EQ(g.cyMax, 321);
    mi_init(&g, MIM_STYLE | MIM_MAXHEIGHT);
    CHECK(U(GetMenuInfo)(s2, &g)); EQ(g.dwStyle, MNS_NOTIFYBYPOS); EQ(g.cyMax, 321);
    mi_init(&g, MIM_STYLE | MIM_MAXHEIGHT);
    CHECK(U(GetMenuInfo)(other, &g)); EQ(g.dwStyle, 0); EQ(g.cyMax, 0);
    M_DESTROY(top); M_DESTROY(other);
}

TEST(menus_info_apply_flag_not_stored) {
    REQ_MI();
    HMENU h = M_POP();
    MENUINFO mi; mi_zero(&mi, MIM_MAXHEIGHT | MIM_APPLYTOSUBMENUS); mi.cyMax = 9;
    CHECK(U(SetMenuInfo)(h, &mi));
    mi_init(&mi, MIM_MAXHEIGHT | MIM_APPLYTOSUBMENUS);
    CHECK(U(GetMenuInfo)(h, &mi));
    EQ(mi.cyMax, 9);
    M_DESTROY(h);
}

/* --- default item --- */

TEST(menus_default_none_set) {
    REQ_CORE();
    REQUIRE(U(GetMenuDefaultItem)); REQUIRE(U(SetMenuDefaultItem));
    HMENU h = mh_make(3);
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 0xFFFFFFFF);
    EQ(U(GetMenuDefaultItem)(h, FALSE, 0), 0xFFFFFFFF);
    EQ(U(GetMenuDefaultItem)(h, TRUE, GMDI_USEDISABLED), 0xFFFFFFFF);
    HMENU e = M_POP();
    EQ(U(GetMenuDefaultItem)(e, TRUE, 0), 0xFFFFFFFF);
    M_DESTROY(e); M_DESTROY(h);
}

TEST(menus_default_set_get_by_pos_and_cmd) {
    REQ_CORE();
    REQUIRE(U(GetMenuDefaultItem)); REQUIRE(U(SetMenuDefaultItem));
    HMENU h = mh_make(4);
    CHECK(U(SetMenuDefaultItem)(h, 2, TRUE));
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 2);
    EQ(U(GetMenuDefaultItem)(h, FALSE, 0), 102);
    CHECK(mh_state(h, 2, MF_BYPOSITION) & MF_DEFAULT);
    CHECK(U(SetMenuDefaultItem)(h, 103, FALSE));
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 3);
    CHECK(!(mh_state(h, 2, MF_BYPOSITION) & MF_DEFAULT));
    CHECK(mh_state(h, 103, 0) & MF_DEFAULT);
    CHECK(U(SetMenuDefaultItem)(h, 0, TRUE));
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 0);
    CHECK(!(mh_state(h, 3, MF_BYPOSITION) & MF_DEFAULT));
    M_DESTROY(h);
}

TEST(menus_default_clear_with_minus_one) {
    REQ_CORE();
    REQUIRE(U(GetMenuDefaultItem)); REQUIRE(U(SetMenuDefaultItem));
    HMENU h = mh_make(2);
    U(SetMenuDefaultItem)(h, 1, TRUE);
    CHECK(U(SetMenuDefaultItem)(h, 0xFFFFFFFF, TRUE));
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 0xFFFFFFFF);
    CHECK(!(mh_state(h, 1, MF_BYPOSITION) & MF_DEFAULT));
    U(SetMenuDefaultItem)(h, 1, TRUE);
    CHECK(U(SetMenuDefaultItem)(h, 0xFFFFFFFF, FALSE));
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 0xFFFFFFFF);
    M_DESTROY(h);
}

TEST(menus_default_invalid_menu) {
    REQ_CORE();
    REQUIRE(U(GetMenuDefaultItem)); REQUIRE(U(SetMenuDefaultItem));
    for (int i = 0; i < MH_NBOGUS; i++) {
        CHECKF(!U(SetMenuDefaultItem)(mh_bogus[i], 0, TRUE), "set bogus %d", i);
        EQ(U(GetMenuDefaultItem)(mh_bogus[i], TRUE, 0), 0xFFFFFFFF);
    }
}

TEST(menus_default_missing_item_note) {
    REQ_CORE();
    REQUIRE(U(GetMenuDefaultItem)); REQUIRE(U(SetMenuDefaultItem));
    HMENU h = mh_make(2);
    U(SetMenuDefaultItem)(h, 0, TRUE);
    BOOL r = U(SetMenuDefaultItem)(h, 5, TRUE);
    t_note("SetMenuDefaultItem(pos 5 of 2) -> %d, default now %u", r, U(GetMenuDefaultItem)(h, TRUE, 0));
    r = U(SetMenuDefaultItem)(h, 999, FALSE);
    t_note("SetMenuDefaultItem(cmd 999) -> %d, default now %u", r, U(GetMenuDefaultItem)(h, TRUE, 0));
    M_DESTROY(h);
}

TEST(menus_default_disabled_item_flags) {
    REQ_CORE();
    REQUIRE(U(GetMenuDefaultItem)); REQUIRE(U(SetMenuDefaultItem));
    HMENU h = mh_make(3);
    U(SetMenuDefaultItem)(h, 1, TRUE);
    EMI(h, 1, MF_BYPOSITION | MF_GRAYED);
    EQ(U(GetMenuDefaultItem)(h, TRUE, GMDI_USEDISABLED), 1);
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 0xFFFFFFFF);
    EQ(U(GetMenuDefaultItem)(h, FALSE, GMDI_USEDISABLED), 101);
    EMI(h, 1, MF_BYPOSITION | MF_ENABLED);
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 1);
    M_DESTROY(h);
}

TEST(menus_default_popup_item_gointopopups) {
    REQ_CORE();
    REQUIRE(U(GetMenuDefaultItem)); REQUIRE(U(SetMenuDefaultItem));
    HMENU top = M_NEW(), s = M_POP();
    mh_add(s, 55, "a"); mh_add(s, 56, "b");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "P");
    CHECK(U(SetMenuDefaultItem)(top, 0, TRUE));
    EQ(U(GetMenuDefaultItem)(top, TRUE, 0), 0);
    t_note("default with GMDI_GOINTOPOPUPS and no default in submenu: %u", U(GetMenuDefaultItem)(top, TRUE, GMDI_GOINTOPOPUPS));
    U(SetMenuDefaultItem)(s, 1, TRUE);
    EQ(U(GetMenuDefaultItem)(top, TRUE, 0), 0);
    t_note("default with GMDI_GOINTOPOPUPS and default in submenu: %u", U(GetMenuDefaultItem)(top, TRUE, GMDI_GOINTOPOPUPS));
    M_DESTROY(top);
}

/* --- radio check --- */

TEST(menus_radio_basic_by_position) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    HMENU h = mh_make(5);
    CHECK(U(CheckMenuRadioItem)(h, 0, 4, 2, MF_BYPOSITION));
    for (int i = 0; i < 5; i++) {
        UINT st = mh_state(h, i, MF_BYPOSITION);
        CHECKF(!!(st & MF_CHECKED) == (i == 2), "item %d state 0x%x", i, st);
    }
    CHECK(mh_state(h, 2, MF_BYPOSITION) & MF_USECHECKBITMAPS);   /* MFT_RADIOCHECK */
    CHECK(U(CheckMenuRadioItem)(h, 0, 4, 4, MF_BYPOSITION));
    for (int i = 0; i < 5; i++) CHECKF(!!(mh_state(h, i, MF_BYPOSITION) & MF_CHECKED) == (i == 4), "item %d", i);
    M_DESTROY(h);
}

TEST(menus_radio_by_command) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    HMENU h = mh_make(5);
    CHECK(U(CheckMenuRadioItem)(h, 100, 104, 103, MF_BYCOMMAND));
    for (int i = 0; i < 5; i++) CHECKF(!!(mh_state(h, i, MF_BYPOSITION) & MF_CHECKED) == (i == 3), "item %d", i);
    CHECK(U(CheckMenuRadioItem)(h, 101, 102, 101, MF_BYCOMMAND));
    CHECK(mh_state(h, 1, MF_BYPOSITION) & MF_CHECKED);
    CHECK(mh_state(h, 3, MF_BYPOSITION) & MF_CHECKED);     /* outside the range: untouched */
    M_DESTROY(h);
}

TEST(menus_radio_outside_range_untouched) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    HMENU h = mh_make(6);
    U(CheckMenuItem)(h, 0, MF_BYPOSITION | MF_CHECKED);
    U(CheckMenuItem)(h, 5, MF_BYPOSITION | MF_CHECKED);
    U(CheckMenuItem)(h, 2, MF_BYPOSITION | MF_CHECKED);
    CHECK(U(CheckMenuRadioItem)(h, 1, 4, 3, MF_BYPOSITION));
    CHECK(mh_state(h, 0, MF_BYPOSITION) & MF_CHECKED);
    CHECK(mh_state(h, 5, MF_BYPOSITION) & MF_CHECKED);
    CHECK(!(mh_state(h, 2, MF_BYPOSITION) & MF_CHECKED));
    CHECK(mh_state(h, 3, MF_BYPOSITION) & MF_CHECKED);
    M_DESTROY(h);
}

TEST(menus_radio_check_outside_range_fails) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    HMENU h = mh_make(5);
    CHECK(!U(CheckMenuRadioItem)(h, 0, 2, 4, MF_BYPOSITION));
    CHECK(!U(CheckMenuRadioItem)(h, 1, 2, 0, MF_BYPOSITION));
    CHECK(!U(CheckMenuRadioItem)(h, 1, 2, 0xFFFFFFFF, MF_BYPOSITION));
    for (int i = 0; i < 5; i++) CHECK(!(mh_state(h, i, MF_BYPOSITION) & MF_CHECKED));
    M_DESTROY(h);
}

TEST(menus_radio_empty_range_error) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    HMENU h = mh_make(3);
    SetLastError(0);
    CHECK(!U(CheckMenuRadioItem)(h, 2, 1, 1, MF_BYPOSITION));         /* first > last */
    EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    SetLastError(0);
    CHECK(!U(CheckMenuRadioItem)(h, 10, 12, 11, MF_BYPOSITION));      /* range has no items */
    EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    SetLastError(0);
    CHECK(!U(CheckMenuRadioItem)(h, 500, 502, 501, MF_BYCOMMAND));
    EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    M_DESTROY(h);
}

TEST(menus_radio_skips_separators) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    HMENU h = M_POP();
    mh_add(h, 1, "a"); U(AppendMenuA)(h, MF_SEPARATOR, 0, NULL); mh_add(h, 2, "b");
    CHECK(U(CheckMenuRadioItem)(h, 0, 2, 2, MF_BYPOSITION));
    CHECK(!(mh_state(h, 1, MF_BYPOSITION) & MF_CHECKED));
    CHECK(mh_state(h, 2, MF_BYPOSITION) & MF_CHECKED);
    SetLastError(0);
    CHECK(!U(CheckMenuRadioItem)(h, 1, 1, 1, MF_BYPOSITION));         /* only a separator in range */
    EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    M_DESTROY(h);
}

TEST(menus_radio_invalid_menu) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    for (int i = 0; i < MH_NBOGUS; i++) CHECKF(!U(CheckMenuRadioItem)(mh_bogus[i], 0, 1, 0, MF_BYPOSITION), "bogus %d", i);
}

TEST(menus_radio_single_item_range) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    HMENU h = mh_make(3);
    CHECK(U(CheckMenuRadioItem)(h, 1, 1, 1, MF_BYPOSITION));
    CHECK(mh_state(h, 1, MF_BYPOSITION) & MF_CHECKED);
    CHECK(U(CheckMenuRadioItem)(h, 1, 1, 1, MF_BYPOSITION));      /* idempotent */
    CHECK(mh_state(h, 1, MF_BYPOSITION) & MF_CHECKED);
    M_DESTROY(h);
}

TEST(menus_radio_in_nested_submenu_by_command) {
    REQ_CORE();
    REQUIRE(U(CheckMenuRadioItem));
    HMENU top = M_NEW(), s = M_POP();
    mh_add(s, 31, "a"); mh_add(s, 32, "b"); mh_add(s, 33, "c");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "S");
    CHECK(U(CheckMenuRadioItem)(top, 31, 33, 32, MF_BYCOMMAND));
    CHECK(mh_state(s, 1, MF_BYPOSITION) & MF_CHECKED);
    CHECK(!(mh_state(s, 0, MF_BYPOSITION) & MF_CHECKED));
    M_DESTROY(top);
}

/* --- item bitmaps --- */

TEST(menus_itembitmaps_roundtrip) {
    REQ_INFO();
    REQUIRE(U(SetMenuItemBitmaps));
    HMENU h = mh_make(3); HBITMAP un = mh_bmp(), ch = mh_bmp();
    CHECK(U(SetMenuItemBitmaps)(h, 1, MF_BYPOSITION, un, ch));
    MENUITEMINFOA mi; mh_init_a(&mi, MIIM_CHECKMARKS);
    CHECK(U(GetMenuItemInfoA)(h, 1, TRUE, &mi));
    CHECK(mi.hbmpUnchecked == un); CHECK(mi.hbmpChecked == ch);
    mh_init_a(&mi, MIIM_CHECKMARKS);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(mi.hbmpUnchecked == NULL && mi.hbmpChecked == NULL);
    CHECK(U(SetMenuItemBitmaps)(h, 102, MF_BYCOMMAND, ch, un));
    mh_init_a(&mi, MIIM_CHECKMARKS);
    CHECK(U(GetMenuItemInfoA)(h, 2, TRUE, &mi));
    CHECK(mi.hbmpUnchecked == ch); CHECK(mi.hbmpChecked == un);
    CHECK(U(SetMenuItemBitmaps)(h, 1, MF_BYPOSITION, NULL, NULL));
    mh_init_a(&mi, MIIM_CHECKMARKS);
    CHECK(U(GetMenuItemInfoA)(h, 1, TRUE, &mi));
    CHECK(mi.hbmpUnchecked == NULL && mi.hbmpChecked == NULL);
    M_DESTROY(h); DeleteObject(un); DeleteObject(ch);
}

TEST(menus_itembitmaps_failures) {
    REQ_INFO();
    REQUIRE(U(SetMenuItemBitmaps));
    HMENU h = mh_make(2); HBITMAP b = mh_bmp();
    CHECK(!U(SetMenuItemBitmaps)(h, 2, MF_BYPOSITION, b, b));
    CHECK(!U(SetMenuItemBitmaps)(h, 999, MF_BYCOMMAND, b, b));
    CHECK(!U(SetMenuItemBitmaps)(h, 0xFFFFFFFF, MF_BYPOSITION, b, b));
    SetLastError(0);
    CHECK(!U(SetMenuItemBitmaps)(h, 0, MF_BYPOSITION, (HBITMAP)0xDEADBEEF, b));
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(SetMenuItemBitmaps)(h, 0, MF_BYPOSITION, b, (HBITMAP)0xDEADBEEF));
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    for (int i = 0; i < MH_NBOGUS; i++) CHECKF(!U(SetMenuItemBitmaps)(mh_bogus[i], 0, MF_BYPOSITION, b, b), "bogus %d", i);
    M_DESTROY(h); DeleteObject(b);
}

TEST(menus_itembitmaps_flags_only_byposition_bit) {
    REQ_INFO();
    REQUIRE(U(SetMenuItemBitmaps));
    HMENU h = mh_make(2); HBITMAP b = mh_bmp();
    /* uFlags & MF_BYPOSITION only; MF_CHECKED etc. do not matter */
    CHECK(U(SetMenuItemBitmaps)(h, 1, MF_BYPOSITION | MF_CHECKED | MF_GRAYED, b, b));
    CHECK(U(SetMenuItemBitmaps)(h, 101, MF_CHECKED | MF_GRAYED, b, b));
    M_DESTROY(h); DeleteObject(b);
}

TEST(menus_checkmark_dimensions) {
    REQUIRE(U(GetMenuCheckMarkDimensions));
    LONG v = U(GetMenuCheckMarkDimensions)();
    int w = LOWORD(v), hgt = HIWORD(v);
    CHECK(w > 0 && w < 200); CHECK(hgt > 0 && hgt < 200);
    for (int i = 0; i < 5; i++) EQ(U(GetMenuCheckMarkDimensions)(), v);
    if (U(GetSystemMetrics)) {
        int cx = U(GetSystemMetrics)(SM_CXMENUCHECK), cy = U(GetSystemMetrics)(SM_CYMENUCHECK);
        if (cx != w || cy != hgt) t_note("check mark dims %dx%d vs metrics %dx%d", w, hgt, cx, cy);
    }
}

/* --- templates --- */

typedef struct { WORD *p; } tw;
static void t_hdr(tw *t, WORD ver) { *t->p++ = ver; *t->p++ = 0; }
static void t_item(tw *t, WORD opt, WORD id, const WCHAR *s) {
    *t->p++ = opt;
    if (!(opt & MF_POPUP)) *t->p++ = id;
    while (*s) *t->p++ = *s++;
    *t->p++ = 0;
}

TEST(menus_loadindirect_basic) {
    REQ_CORE();
    REQUIRE(U(LoadMenuIndirectW));
    WORD buf[256]; tw t = { buf };
    t_hdr(&t, 0);
    t_item(&t, MF_POPUP, 0, L"&File");
      t_item(&t, 0, 100, L"&Open");
      t_item(&t, MF_GRAYED, 101, L"Gray");
      t_item(&t, MF_CHECKED, 102, L"Checked");
      t_item(&t, MF_MENUBREAK, 103, L"Break");
      t_item(&t, MF_END, 104, L"Exit");
    t_item(&t, MF_POPUP | MF_END, 0, L"&Edit");
      t_item(&t, MF_POPUP, 0, L"Nested");
        t_item(&t, MF_END, 300, L"Deep");
      t_item(&t, MF_END, 201, L"Copy");
    HMENU m = U(LoadMenuIndirectW)(buf);
    CHECK(m != NULL);
    if (!m) return;
    CHECK(M_IS(m));
    EQ(M_CNT(m), 2);
    HMENU f = U(GetSubMenu)(m, 0), e = U(GetSubMenu)(m, 1);
    CHECK(f && e && M_IS(f) && M_IS(e));
    EQ(M_CNT(f), 5); EQ(M_CNT(e), 2);
    WCHAR w[32];
    EQ(U(GetMenuStringW)(m, 0, w, 32, MF_BYPOSITION), 5); CHECK(!lstrcmpW(w, L"&File"));
    EQ(U(GetMenuStringW)(m, 1, w, 32, MF_BYPOSITION), 5); CHECK(!lstrcmpW(w, L"&Edit"));
    EQ(U(GetMenuItemID)(f, 0), 100); EQ(U(GetMenuItemID)(f, 4), 104);
    EQ(U(GetMenuStringW)(f, 3, w, 32, MF_BYPOSITION), 5); CHECK(!lstrcmpW(w, L"Break"));
    CHECK(mh_state(f, 1, MF_BYPOSITION) & MF_GRAYED);
    CHECK(mh_state(f, 2, MF_BYPOSITION) & MF_CHECKED);
    CHECK(mh_state(f, 3, MF_BYPOSITION) & MF_MENUBREAK);
    CHECK(!(mh_state(f, 0, MF_BYPOSITION) & (MF_GRAYED | MF_CHECKED)));
    HMENU n = U(GetSubMenu)(e, 0);
    CHECK(n != NULL); EQ(M_CNT(n), 1); EQ(U(GetMenuItemID)(n, 0), 300);
    EQ(U(GetMenuItemID)(e, 1), 201);
    CHECK(mh_state(m, 300, 0) != 0xFFFFFFFF);
    CHECK(M_DESTROY(m));
    CHECK(!M_IS(m)); CHECK(!M_IS(f)); CHECK(!M_IS(e)); CHECK(!M_IS(n));
}

TEST(menus_loadindirect_a_and_w_same_template_format) {
    REQ_CORE();
    REQUIRE(U(LoadMenuIndirectW)); REQUIRE(U(LoadMenuIndirectA));
    WORD buf[64]; tw t = { buf };
    t_hdr(&t, 0);
    t_item(&t, 0, 5, L"One");
    t_item(&t, MF_END, 6, L"Two");
    HMENU a = U(LoadMenuIndirectA)(buf), w = U(LoadMenuIndirectW)(buf);
    CHECK(a && w); CHECK(a != w);
    if (a && w) {
        EQ(M_CNT(a), 2); EQ(M_CNT(w), 2);
        WCHAR s1[8], s2[8];
        U(GetMenuStringW)(a, 0, s1, 8, MF_BYPOSITION); U(GetMenuStringW)(w, 0, s2, 8, MF_BYPOSITION);
        CHECK(!lstrcmpW(s1, s2)); CHECK(!lstrcmpW(s1, L"One"));
    }
    if (a) M_DESTROY(a);
    if (w) M_DESTROY(w);
}

TEST(menus_loadindirect_bad_versions) {
    REQ_CORE();
    REQUIRE(U(LoadMenuIndirectW));
    static const WORD ver[] = { 2, 3, 0x100, 0x7FFF, 0xFFFF };
    for (unsigned i = 0; i < sizeof ver / sizeof *ver; i++) {
        WORD buf[32]; tw t = { buf };
        t_hdr(&t, ver[i]);
        t_item(&t, MF_END, 1, L"x");
        HMENU m = U(LoadMenuIndirectW)(buf);
        CHECKF(m == NULL, "version %u gave a menu", ver[i]);
        if (m) M_DESTROY(m);
    }
}

TEST(menus_loadindirect_header_offset) {
    REQ_CORE();
    REQUIRE(U(LoadMenuIndirectW));
    WORD buf[64]; tw t = { buf };
    *t.p++ = 0; *t.p++ = 4;           /* 4 bytes of extra header */
    *t.p++ = 0xAAAA; *t.p++ = 0xBBBB;
    t_item(&t, MF_END, 77, L"Offset");
    HMENU m = U(LoadMenuIndirectW)(buf);
    CHECK(m != NULL);
    if (m) { EQ(M_CNT(m), 1); EQ(U(GetMenuItemID)(m, 0), 77); M_DESTROY(m); }
}

TEST(menus_loadindirect_many_items) {
    REQ_CORE();
    REQUIRE(U(LoadMenuIndirectW));
    static WORD buf[20000]; tw t = { buf };
    t_hdr(&t, 0);
    for (int i = 0; i < 200; i++) {
        WCHAR s[16]; wsprintfW(s, L"Item %d", i);
        t_item(&t, i == 199 ? MF_END : 0, 1000 + i, s);
    }
    HMENU m = U(LoadMenuIndirectW)(buf);
    CHECK(m != NULL);
    if (m) {
        EQ(M_CNT(m), 200); EQ(U(GetMenuItemID)(m, 199), 1199);
        WCHAR w[32]; EQ(U(GetMenuStringW)(m, 150, w, 32, MF_BYPOSITION), 8); CHECK(!lstrcmpW(w, L"Item 150"));
        M_DESTROY(m);
    }
}

TEST(menus_loadindirect_separator_item_note) {
    REQ_CORE();
    REQUIRE(U(LoadMenuIndirectW));
    WORD buf[64]; tw t = { buf };
    t_hdr(&t, 0);
    t_item(&t, 0, 1, L"a");
    t_item(&t, 0, 0, L"");
    t_item(&t, MF_END, 2, L"b");
    HMENU m = U(LoadMenuIndirectW)(buf);
    CHECK(m != NULL);
    if (m) {
        EQ(M_CNT(m), 3);
        UINT st = mh_state(m, 1, MF_BYPOSITION);
        t_note("template item (0,0,\"\") state 0x%x (separator bit %s)", st, (st & MF_SEPARATOR) ? "set" : "clear");
        M_DESTROY(m);
    }
}

TEST(menus_loadmenu_missing_resource) {
    REQUIRE(U(LoadMenuA)); REQUIRE(U(LoadMenuW));
    HINSTANCE hi = GetModuleHandleA(NULL);
    SetLastError(0);
    CHECK(U(LoadMenuA)(hi, "NO_SUCH_MENU_RESOURCE") == NULL);
    DWORD e = GetLastError();
    /* this executable carries no menu resources (possibly no resource section at all) */
    CHECK(e == ERROR_RESOURCE_NAME_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_DATA_NOT_FOUND);
    SetLastError(0);
    CHECK(U(LoadMenuW)(hi, L"NO_SUCH_MENU_RESOURCE") == NULL);
    e = GetLastError();
    CHECK(e == ERROR_RESOURCE_NAME_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_DATA_NOT_FOUND);
    SetLastError(0);
    CHECK(U(LoadMenuA)(hi, MAKEINTRESOURCEA(31999)) == NULL);
    e = GetLastError();
    CHECK(e == ERROR_RESOURCE_NAME_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_DATA_NOT_FOUND);
    SetLastError(0);
    CHECK(U(LoadMenuW)(NULL, MAKEINTRESOURCEW(31999)) == NULL);
    e = GetLastError();
    CHECK(e == ERROR_RESOURCE_NAME_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_DATA_NOT_FOUND);
}

/* --- window menus --- */

TEST(menus_window_setmenu_getmenu) {
    REQ_CORE();
    REQUIRE(U(SetMenu)); REQUIRE(U(GetMenu)); REQUIRE(U(DestroyWindow));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    CHECK(w != NULL); if (!w) return;
    CHECK(U(GetMenu)(w) == NULL);
    HMENU a = mh_bar(2), b = mh_bar(3);
    CHECK(U(SetMenu)(w, a)); CHECK(U(GetMenu)(w) == a);
    CHECK(U(SetMenu)(w, b)); CHECK(U(GetMenu)(w) == b);
    CHECK(M_IS(a));                         /* replaced menu is not destroyed */
    CHECK(U(SetMenu)(w, NULL)); CHECK(U(GetMenu)(w) == NULL);
    CHECK(M_IS(b));
    CHECK(U(SetMenu)(w, b)); CHECK(U(GetMenu)(w) == b);
    U(DestroyWindow)(w);
    CHECK(!M_IS(b));                        /* window owned menu is destroyed with it */
    CHECK(M_IS(a));
    M_DESTROY(a);
}

TEST(menus_window_setmenu_invalid) {
    REQ_CORE();
    REQUIRE(U(SetMenu)); REQUIRE(U(GetMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    HMENU a = mh_bar(1);
    CHECK(!U(SetMenu)(NULL, a));
    CHECK(!U(SetMenu)((HWND)0xDEADBEEF, a));
    CHECK(!U(SetMenu)((HWND)(LONG_PTR)-1, a));
    for (int i = 1; i < MH_NBOGUS; i++) CHECKF(!U(SetMenu)(w, mh_bogus[i]), "bogus menu %d", i);
    CHECK(U(GetMenu)(w) == NULL);
    HMENU gone = M_NEW(); M_DESTROY(gone);
    CHECK(!U(SetMenu)(w, gone));
    CHECK(U(GetMenu)(NULL) == NULL);
    CHECK(U(GetMenu)((HWND)0xDEADBEEF) == NULL);
    U(DestroyWindow)(w);
    M_DESTROY(a);
}

TEST(menus_window_getmenu_after_destroy) {
    REQ_CORE();
    REQUIRE(U(SetMenu)); REQUIRE(U(GetMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    U(SetMenu)(w, mh_bar(1));
    U(DestroyWindow)(w);
    CHECK(U(GetMenu)(w) == NULL);
    CHECK(!U(SetMenu)(w, NULL));
}

TEST(menus_window_menu_changes_client_area) {
    REQ_CORE();
    REQUIRE(U(SetMenu)); REQUIRE(U(GetClientRect)); REQUIRE(U(GetWindowRect));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    RECT c0, c1, c2, w0, w1;
    U(GetClientRect)(w, &c0); U(GetWindowRect)(w, &w0);
    HMENU m = mh_bar(3);
    CHECK(U(SetMenu)(w, m));
    U(GetClientRect)(w, &c1); U(GetWindowRect)(w, &w1);
    CHECKF(c1.bottom < c0.bottom, "client height %ld -> %ld", c0.bottom, c1.bottom);
    CHECK(c1.right == c0.right);
    CHECK(w1.right - w1.left == w0.right - w0.left);
    CHECK(w1.bottom - w1.top == w0.bottom - w0.top);
    CHECK(U(SetMenu)(w, NULL));
    U(GetClientRect)(w, &c2);
    CHECK(c2.bottom == c0.bottom);
    U(DestroyWindow)(w); M_DESTROY(m);
}

TEST(menus_window_drawmenubar) {
    REQ_CORE();
    REQUIRE(U(DrawMenuBar)); REQUIRE(U(SetMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, mh_bar(2));
    if (!w) { CHECK(0); return; }
    CHECK(U(DrawMenuBar)(w));
    CHECK(U(DrawMenuBar)(w));
    U(DestroyWindow)(w);
    CHECK(!U(DrawMenuBar)(w));
    CHECK(!U(DrawMenuBar)(NULL));
    CHECK(!U(DrawMenuBar)((HWND)0xDEADBEEF));
    HWND w2 = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (w2) { CHECK(U(DrawMenuBar)(w2)); U(DestroyWindow)(w2); }
}

TEST(menus_window_drawmenubar_invalid_last_error) {
    REQ_CORE();
    REQUIRE(U(DrawMenuBar));
    SetLastError(0);
    CHECK(!U(DrawMenuBar)(NULL));
    DWORD e = GetLastError();
    if (e != ERROR_INVALID_WINDOW_HANDLE) t_note("DrawMenuBar(NULL) error %lu (expected 1400)", e);
}

TEST(menus_window_child_style_menu_note) {
    REQ_CORE();
    REQUIRE(U(SetMenu));
    HWND p = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    HWND c = p ? t_make_window(NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 50, 50, p) : NULL;
    if (!c) { if (p) U(DestroyWindow)(p); CHECK(0); return; }
    HMENU m = mh_bar(1);
    BOOL r = U(SetMenu)(c, m);
    t_note("SetMenu on a child window -> %d (err %lu)", r, GetLastError());
    U(DestroyWindow)(p);
    if (M_IS(m)) M_DESTROY(m);
}

TEST(menus_window_one_menu_in_two_windows_note) {
    REQ_CORE();
    REQUIRE(U(SetMenu));
    HWND a = mh_win(WS_OVERLAPPEDWINDOW, NULL), b = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!a || !b) { CHECK(0); return; }
    HMENU m = mh_bar(1);
    CHECK(U(SetMenu)(a, m));
    BOOL r = U(SetMenu)(b, m);
    t_note("SetMenu of a menu already used by another window -> %d; owner a:%p b:%p", r, (void *)U(GetMenu)(a), (void *)U(GetMenu)(b));
    U(DestroyWindow)(a); U(DestroyWindow)(b);
    if (M_IS(m)) M_DESTROY(m);
}

/* --- menu bar geometry --- */

TEST(menus_bar_getmenuitemrect_window_bar) {
    REQ_CORE();
    REQUIRE(U(GetMenuItemRect)); REQUIRE(U(GetWindowRect));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, mh_bar(4));
    if (!w) { CHECK(0); return; }
    HMENU m = U(GetMenu)(w);
    RECT wr, r[4]; U(GetWindowRect)(w, &wr);
    for (int i = 0; i < 4; i++) {
        memset(&r[i], 0xCC, sizeof r[i]);
        CHECKF(U(GetMenuItemRect)(w, m, i, &r[i]), "item %d", i);
        CHECKF(r[i].right > r[i].left && r[i].bottom > r[i].top, "item %d empty", i);
        CHECKF(r[i].left >= wr.left && r[i].right <= wr.right && r[i].top >= wr.top && r[i].bottom <= wr.bottom, "item %d outside window", i);
    }
    for (int i = 1; i < 4; i++) {
        CHECKF(r[i].left >= r[i - 1].right, "item %d overlaps previous", i);
        CHECK(r[i].top == r[0].top && r[i].bottom == r[0].bottom);
    }
    RECT bad = { 1, 2, 3, 4 };
    CHECK(!U(GetMenuItemRect)(w, m, 4, &bad));
    CHECK(!U(GetMenuItemRect)(w, m, 0xFFFFFFFF, &bad));
    CHECK(!U(GetMenuItemRect)(w, NULL, 0, &bad));
    CHECK(!U(GetMenuItemRect)(w, (HMENU)0xDEADBEEF, 0, &bad));
    U(DestroyWindow)(w);
}

TEST(menus_bar_menuitemfrompoint) {
    REQ_CORE();
    REQUIRE(U(GetMenuItemRect)); REQUIRE(U(MenuItemFromPoint));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, mh_bar(3));
    if (!w) { CHECK(0); return; }
    HMENU m = U(GetMenu)(w);
    for (int i = 0; i < 3; i++) {
        RECT r; POINT p;
        if (!U(GetMenuItemRect)(w, m, i, &r)) { CHECK(0); continue; }
        p.x = (r.left + r.right) / 2; p.y = (r.top + r.bottom) / 2;
        int idx = U(MenuItemFromPoint)(w, m, p);
        CHECKF(idx == i, "item %d centre -> %d", i, idx);
    }
    POINT far_ = { -30000, -30000 };
    CHECK(U(MenuItemFromPoint)(w, m, far_) == -1);
    POINT far2 = { 30000, 30000 };
    CHECK(U(MenuItemFromPoint)(w, m, far2) == -1);
    U(DestroyWindow)(w);
}

TEST(menus_bar_getmenubarinfo) {
    REQ_CORE();
    REQUIRE(U(GetMenuBarInfo)); REQUIRE(U(GetMenuItemRect)); REQUIRE(U(GetWindowRect));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, mh_bar(3));
    if (!w) { CHECK(0); return; }
    HMENU m = U(GetMenu)(w);
    MENUBARINFO mbi; memset(&mbi, 0, sizeof mbi); mbi.cbSize = sizeof mbi;
    CHECK(U(GetMenuBarInfo)(w, OBJID_MENU, 0, &mbi));
    CHECK(mbi.hMenu == m);
    CHECK(mbi.rcBar.right > mbi.rcBar.left && mbi.rcBar.bottom > mbi.rcBar.top);
    CHECK(!mbi.fBarFocused); CHECK(!mbi.fFocused);
    RECT wr; U(GetWindowRect)(w, &wr);
    CHECK(mbi.rcBar.left >= wr.left && mbi.rcBar.right <= wr.right && mbi.rcBar.top >= wr.top && mbi.rcBar.bottom <= wr.bottom);
    for (int i = 1; i <= 3; i++) {
        MENUBARINFO b2; memset(&b2, 0, sizeof b2); b2.cbSize = sizeof b2;
        CHECKF(U(GetMenuBarInfo)(w, OBJID_MENU, i, &b2), "item %d", i);
        RECT ir; U(GetMenuItemRect)(w, m, i - 1, &ir);
        CHECKF(b2.rcBar.left == ir.left && b2.rcBar.right == ir.right && b2.rcBar.top == ir.top && b2.rcBar.bottom == ir.bottom, "item %d rect", i);
    }
    MENUBARINFO b3; memset(&b3, 0, sizeof b3); b3.cbSize = sizeof b3;
    CHECK(!U(GetMenuBarInfo)(w, OBJID_MENU, 4, &b3));
    memset(&b3, 0, sizeof b3); b3.cbSize = sizeof b3 - 1;
    CHECK(!U(GetMenuBarInfo)(w, OBJID_MENU, 0, &b3));
    memset(&b3, 0, sizeof b3); b3.cbSize = 0;
    CHECK(!U(GetMenuBarInfo)(w, OBJID_MENU, 0, &b3));
    memset(&b3, 0, sizeof b3); b3.cbSize = sizeof b3;
    CHECK(!U(GetMenuBarInfo)(NULL, OBJID_MENU, 0, &b3));
    CHECK(!U(GetMenuBarInfo)((HWND)0xDEADBEEF, OBJID_MENU, 0, &b3));
    U(DestroyWindow)(w);
}

TEST(menus_bar_getmenubarinfo_no_menu_and_sysmenu) {
    REQ_CORE();
    REQUIRE(U(GetMenuBarInfo)); REQUIRE(U(GetSystemMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    MENUBARINFO mbi; memset(&mbi, 0, sizeof mbi); mbi.cbSize = sizeof mbi;
    BOOL r = U(GetMenuBarInfo)(w, OBJID_MENU, 0, &mbi);
    t_note("GetMenuBarInfo(OBJID_MENU) on window without menu -> %d", r);
    memset(&mbi, 0, sizeof mbi); mbi.cbSize = sizeof mbi;
    r = U(GetMenuBarInfo)(w, OBJID_SYSMENU, 0, &mbi);
    t_note("GetMenuBarInfo(OBJID_SYSMENU) -> %d hMenu=%p sys=%p", r, (void *)mbi.hMenu, (void *)U(GetSystemMenu)(w, FALSE));
    U(DestroyWindow)(w);
}

TEST(menus_bar_hilite_menu_item) {
    REQ_CORE();
    REQUIRE(U(HiliteMenuItem));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, mh_bar(3));
    if (!w) { CHECK(0); return; }
    HMENU m = U(GetMenu)(w);
    CHECK(U(HiliteMenuItem)(w, m, 1, MF_BYPOSITION | MF_HILITE));
    UINT st = mh_state(m, 1, MF_BYPOSITION);
    t_note("state after HiliteMenuItem(MF_HILITE) 0x%x", st);
    CHECK(U(HiliteMenuItem)(w, m, 1, MF_BYPOSITION | MF_UNHILITE));
    t_note("state after HiliteMenuItem(MF_UNHILITE) 0x%x", mh_state(m, 1, MF_BYPOSITION));
    CHECK(U(HiliteMenuItem)(w, m, 201, MF_BYCOMMAND | MF_HILITE));
    CHECK(U(HiliteMenuItem)(w, m, 201, MF_BYCOMMAND | MF_UNHILITE));
    BOOL r = U(HiliteMenuItem)(w, m, 9, MF_BYPOSITION | MF_HILITE);
    t_note("HiliteMenuItem on missing position -> %d", r);
    CHECK(!U(HiliteMenuItem)(w, (HMENU)0xDEADBEEF, 0, MF_BYPOSITION | MF_HILITE));
    U(DestroyWindow)(w);
}

/* --- system menu --- */

TEST(menus_sys_getsystemmenu_invalid_window) {
    REQ_CORE();
    REQUIRE(U(GetSystemMenu));
    CHECK(U(GetSystemMenu)(NULL, FALSE) == NULL);
    CHECK(U(GetSystemMenu)(NULL, TRUE) == NULL);
    CHECK(U(GetSystemMenu)((HWND)0xDEADBEEF, FALSE) == NULL);
    CHECK(U(GetSystemMenu)((HWND)(LONG_PTR)-1, FALSE) == NULL);
}

TEST(menus_sys_contents) {
    REQ_CORE();
    REQUIRE(U(GetSystemMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    HMENU s = U(GetSystemMenu)(w, FALSE);
    CHECK(s != NULL && M_IS(s));
    if (s) {
        static const UINT ids[] = { SC_RESTORE, SC_MOVE, SC_SIZE, SC_MINIMIZE, SC_MAXIMIZE, SC_CLOSE };
        for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++)
            CHECKF(mh_state(s, ids[i], MF_BYCOMMAND) != 0xFFFFFFFF, "missing SC 0x%x", ids[i]);
        CHECK(M_CNT(s) >= 6);
        t_note("system menu item count %d", M_CNT(s));
        EQ(U(GetMenuItemID)(s, M_CNT(s) - 1), SC_CLOSE);
        char b[64];
        int n = U(GetMenuStringA)(s, SC_CLOSE, b, 64, MF_BYCOMMAND);
        CHECK(n > 0);
        CHECK(U(GetSystemMenu)(w, FALSE) == s);        /* stable handle */
    }
    U(DestroyWindow)(w);
}

TEST(menus_sys_distinct_per_window) {
    REQ_CORE();
    REQUIRE(U(GetSystemMenu));
    HWND a = mh_win(WS_OVERLAPPEDWINDOW, NULL), b = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!a || !b) { CHECK(0); return; }
    HMENU sa = U(GetSystemMenu)(a, FALSE), sb = U(GetSystemMenu)(b, FALSE);
    CHECK(sa && sb && sa != sb);
    U(DestroyWindow)(a); U(DestroyWindow)(b);
}

TEST(menus_sys_revert_returns_null_and_resets) {
    REQ_CORE();
    REQUIRE(U(GetSystemMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    HMENU s = U(GetSystemMenu)(w, FALSE);
    CHECK(s != NULL);
    int base = M_CNT(s);
    CHECK(U(AppendMenuA)(s, MF_STRING, 0x1234, "Custom entry"));
    EQ(M_CNT(s), base + 1);
    CHECK(U(GetSystemMenu)(w, FALSE) == s);
    EQ(M_CNT(U(GetSystemMenu)(w, FALSE)), base + 1);
    CHECK(U(GetSystemMenu)(w, TRUE) == NULL);
    t_note("old system menu handle valid after revert: %d", M_IS(s));
    HMENU s2 = U(GetSystemMenu)(w, FALSE);
    CHECK(s2 != NULL && M_IS(s2));
    if (s2) {
        EQ(M_CNT(s2), base);
        EQ(mh_state(s2, 0x1234, 0), 0xFFFFFFFF);
        CHECK(mh_state(s2, SC_CLOSE, 0) != 0xFFFFFFFF);
    }
    U(DestroyWindow)(w);
}

TEST(menus_sys_revert_twice) {
    REQ_CORE();
    REQUIRE(U(GetSystemMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    CHECK(U(GetSystemMenu)(w, TRUE) == NULL);
    CHECK(U(GetSystemMenu)(w, TRUE) == NULL);
    CHECK(U(GetSystemMenu)(w, FALSE) != NULL);
    CHECK(U(GetSystemMenu)(w, TRUE) == NULL);
    U(DestroyWindow)(w);
}

TEST(menus_sys_modify_enable_close) {
    REQ_CORE();
    REQUIRE(U(GetSystemMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    HMENU s = U(GetSystemMenu)(w, FALSE);
    if (s) {
        UINT prev = EMI(s, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
        CHECK(prev != 0xFFFFFFFF);
        CHECK(mh_state(s, SC_CLOSE, 0) & MF_GRAYED);
        EMI(s, SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
        CHECK(!(mh_state(s, SC_CLOSE, 0) & MF_GRAYED));
    }
    U(GetSystemMenu)(w, TRUE);
    U(DestroyWindow)(w);
}

TEST(menus_sys_window_styles_note) {
    REQ_CORE();
    REQUIRE(U(GetSystemMenu));
    static const struct { const char *n; DWORD st; } t[] = {
        { "WS_POPUP", WS_POPUP }, { "WS_CAPTION", WS_CAPTION }, { "WS_CAPTION|SYSMENU", WS_CAPTION | WS_SYSMENU },
        { "WS_POPUP|SYSMENU", WS_POPUP | WS_SYSMENU }, { "WS_OVERLAPPED", WS_OVERLAPPED } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HWND w = mh_win(t[i].st, NULL);
        if (!w) continue;
        HMENU s = U(GetSystemMenu)(w, FALSE);
        t_note("GetSystemMenu for %s -> %p", t[i].n, (void *)s);
        U(DestroyWindow)(w);
    }
    HWND w = mh_win(WS_CAPTION | WS_SYSMENU, NULL);
    if (w) { CHECK(U(GetSystemMenu)(w, FALSE) != NULL); U(DestroyWindow)(w); }
}

TEST(menus_sys_after_window_destroyed) {
    REQ_CORE();
    REQUIRE(U(GetSystemMenu));
    HWND w = mh_win(WS_OVERLAPPEDWINDOW, NULL);
    if (!w) { CHECK(0); return; }
    HMENU s = U(GetSystemMenu)(w, FALSE);
    U(DestroyWindow)(w);
    CHECK(U(GetSystemMenu)(w, FALSE) == NULL);
    t_note("system menu handle valid after DestroyWindow: %d", s ? M_IS(s) : -1);
}
