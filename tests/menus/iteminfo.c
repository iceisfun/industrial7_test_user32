/* menus: MENUITEMINFO insert/get/set (A and W), validation and field semantics */
#include "mh.h"

static BOOL ins_a(HMENU h, UINT pos, UINT mask, UINT id, const char *s, UINT type, UINT state) {
    MENUITEMINFOA mi; mh_zero_a(&mi, mask);
    mi.wID = id; mi.dwTypeData = (LPSTR)s; mi.fType = type; mi.fState = state;
    return U(InsertMenuItemA)(h, pos, TRUE, &mi);
}
static BOOL ins_w(HMENU h, UINT pos, UINT mask, UINT id, const WCHAR *s, UINT type, UINT state) {
    MENUITEMINFOW mi; mh_zero_w(&mi, mask);
    mi.wID = id; mi.dwTypeData = (LPWSTR)s; mi.fType = type; mi.fState = state;
    return U(InsertMenuItemW)(h, pos, TRUE, &mi);
}
/* Run Get/Set/Insert with a given (bad) struct and return error. */
static DWORD err_ins(HMENU h, MENUITEMINFOA *mi, BOOL *r) { SetLastError(0); *r = U(InsertMenuItemA)(h, 0, TRUE, mi); return GetLastError(); }
static DWORD err_set(HMENU h, MENUITEMINFOA *mi, BOOL *r) { SetLastError(0); *r = U(SetMenuItemInfoA)(h, 0, TRUE, mi); return GetLastError(); }
static DWORD err_get(HMENU h, MENUITEMINFOA *mi, BOOL *r) { SetLastError(0); *r = U(GetMenuItemInfoA)(h, 0, TRUE, mi); return GetLastError(); }

TEST(menus_mii_struct_layout) {
    CHECK(sizeof(MENUITEMINFOA) == 80); CHECK(sizeof(MENUITEMINFOW) == 80);
    EQ(offsetof(MENUITEMINFOA, hSubMenu), 24);
    EQ(offsetof(MENUITEMINFOA, hbmpChecked), 32);
    EQ(offsetof(MENUITEMINFOA, dwItemData), 48);
    EQ(offsetof(MENUITEMINFOA, dwTypeData), 56);
    EQ(offsetof(MENUITEMINFOA, cch), 64);
    EQ(offsetof(MENUITEMINFOA, hbmpItem), 72);
    CHECK(sizeof(MENUINFO) == 40);
}

TEST(menus_mii_cbsize_invalid_values) {
    REQ_INFO();
    static const UINT bad[] = { 0, 1, 4, 8, 44, 48, 0x47, 0x49, 0x4F, 0x51, 0x58, 0x60, 0x100, 0x1000, 0x7FFFFFFF, 0xFFFFFFFF };
    HMENU h = M_POP(); mh_add(h, 1, "x");
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUITEMINFOA mi; BOOL r; DWORD e;
        mh_zero_a(&mi, MIIM_ID | MIIM_STATE); mi.cbSize = bad[i];
        e = err_get(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "get cbSize=%u r=%d e=%lu", bad[i], r, e);
        mh_zero_a(&mi, MIIM_ID | MIIM_STATE); mi.cbSize = bad[i];
        e = err_set(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "set cbSize=%u r=%d e=%lu", bad[i], r, e);
        mh_zero_a(&mi, MIIM_ID | MIIM_STATE); mi.cbSize = bad[i];
        e = err_ins(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "ins cbSize=%u r=%d e=%lu", bad[i], r, e);
    }
    EQ(M_CNT(h), 1);
    M_DESTROY(h);
}

TEST(menus_mii_cbsize_valid_values) {
    REQ_INFO();
    HMENU h = M_POP();
    MENUITEMINFOA mi;
    mh_zero_a(&mi, MIIM_ID | MIIM_STRING); mi.cbSize = 0x48; mi.wID = 5; mi.dwTypeData = "old";
    CHECK(U(InsertMenuItemA)(h, 0xFFFFFFFF, TRUE, &mi));
    mh_zero_a(&mi, MIIM_ID | MIIM_STRING); mi.cbSize = 0x50; mi.wID = 6; mi.dwTypeData = "new";
    CHECK(U(InsertMenuItemA)(h, 0xFFFFFFFF, TRUE, &mi));
    EQ(M_CNT(h), 2);
    mh_zero_a(&mi, MIIM_ID); mi.cbSize = 0x48;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi)); EQ(mi.wID, 5); EQ(mi.cbSize, 0x48);
    mh_zero_a(&mi, MIIM_ID); mi.cbSize = 0x50;
    CHECK(U(GetMenuItemInfoA)(h, 1, TRUE, &mi)); EQ(mi.wID, 6); EQ(mi.cbSize, 0x50);
    mh_zero_a(&mi, MIIM_ID); mi.cbSize = 0x48; mi.wID = 7;
    CHECK(U(SetMenuItemInfoA)(h, 1, TRUE, &mi));
    EQ(U(GetMenuItemID)(h, 1), 7);
    M_DESTROY(h);
}

TEST(menus_mii_cbsize_restored_on_get) {
    REQ_INFO();
    HMENU h = mh_make(1);
    MENUITEMINFOW mi;
    mh_zero_w(&mi, MIIM_ID); mi.cbSize = 0x48;
    CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mi)); EQ(mi.cbSize, 0x48);
    mh_zero_w(&mi, MIIM_ID); mi.cbSize = 0x50;
    CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mi)); EQ(mi.cbSize, 0x50);
    M_DESTROY(h);
}

TEST(menus_mii_short_cbsize_leaves_hbmpitem_untouched) {
    REQ_INFO();
    HMENU h = M_POP(); HBITMAP bm = mh_bmp();
    MENUITEMINFOW in; mh_zero_w(&in, MIIM_ID | MIIM_BITMAP); in.wID = 3; in.hbmpItem = bm;
    CHECK(U(InsertMenuItemW)(h, 0, TRUE, &in));
    MENUITEMINFOW mi; mh_init_w(&mi, MIIM_ID | MIIM_BITMAP); mi.cbSize = 0x48;
    CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mi));
    EQ(mi.hbmpItem, 0xCCCCCCCCCCCCCCCCULL);
    mh_init_w(&mi, MIIM_ID | MIIM_BITMAP); mi.cbSize = 0x50;
    CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mi));
    CHECK(mi.hbmpItem == bm);
    M_DESTROY(h); DeleteObject(bm);
}

TEST(menus_mii_null_struct) {
    REQ_INFO();
    HMENU h = mh_make(1);
    /* GetMenuItemInfo reads lpmii->cbSize before validating: NULL is not tested for it */
    SetLastError(0);
    CHECK(!U(SetMenuItemInfoA)(h, 0, TRUE, NULL)); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(SetMenuItemInfoW)(h, 0, TRUE, NULL)); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(InsertMenuItemA)(h, 0, TRUE, NULL)); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(InsertMenuItemW)(h, 0, TRUE, NULL)); EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    EQ(M_CNT(h), 1);
    M_DESTROY(h);
}

TEST(menus_mii_mask_invalid_bits) {
    REQ_INFO();
    static const UINT bad[] = { 0x200, 0x400, 0x800, 0x1000, 0x8000, 0x10000, 0x40000000, 0x80000000, 0xFFFFFFFF, 0x3FF, 0x2FF };
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUITEMINFOA mi; BOOL r; DWORD e;
        mh_zero_a(&mi, bad[i]);
        e = err_get(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "get mask 0x%x r=%d e=%lu", bad[i], r, e);
        mh_zero_a(&mi, bad[i]);
        e = err_set(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "set mask 0x%x r=%d e=%lu", bad[i], r, e);
        mh_zero_a(&mi, bad[i]);
        e = err_ins(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "ins mask 0x%x r=%d e=%lu", bad[i], r, e);
    }
    EQ(M_CNT(h), 1);
    M_DESTROY(h);
}

TEST(menus_mii_mask_type_conflicts) {
    REQ_INFO();
    static const UINT bad[] = { MIIM_TYPE | MIIM_STRING, MIIM_TYPE | MIIM_BITMAP, MIIM_TYPE | MIIM_FTYPE,
                                MIIM_TYPE | MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE, MIIM_TYPE | MIIM_STRING | MIIM_ID };
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUITEMINFOA mi; BOOL r; DWORD e; char buf[16];
        mh_zero_a(&mi, bad[i]); mi.dwTypeData = buf; mi.cch = 16;
        e = err_get(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "get mask 0x%x r=%d e=%lu", bad[i], r, e);
        mh_zero_a(&mi, bad[i]); mi.dwTypeData = "x";
        e = err_set(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "set mask 0x%x r=%d e=%lu", bad[i], r, e);
        mh_zero_a(&mi, bad[i]); mi.dwTypeData = "x";
        e = err_ins(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "ins mask 0x%x r=%d e=%lu", bad[i], r, e);
    }
    M_DESTROY(h);
}

TEST(menus_mii_mask_zero_get) {
    REQ_INFO();
    HMENU h = mh_make(2);
    MENUITEMINFOA mi; mh_init_a(&mi, 0);
    CHECK(U(GetMenuItemInfoA)(h, 1, TRUE, &mi));
    EQ(mi.fMask, 0);
    EQ(mi.wID, 0xCCCCCCCC);        /* nothing requested, nothing written */
    EQ(mi.fState, 0xCCCCCCCC);
    EQ(mi.dwItemData, 0xCCCCCCCCCCCCCCCCULL);
    mh_init_a(&mi, 0);
    CHECK(!U(GetMenuItemInfoA)(h, 2, TRUE, &mi));
    M_DESTROY(h);
}

TEST(menus_mii_get_missing_item_errors) {
    REQ_INFO();
    HMENU h = mh_make(2);
    MENUITEMINFOA mi; BOOL r; DWORD e;
    mh_zero_a(&mi, MIIM_ID); e = err_get(h, &mi, &r);
    CHECK(r);
    SetLastError(0); mh_zero_a(&mi, MIIM_ID);
    CHECK(!U(GetMenuItemInfoA)(h, 2, TRUE, &mi)); EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    SetLastError(0); mh_zero_a(&mi, MIIM_ID);
    CHECK(!U(GetMenuItemInfoA)(h, 0xFFFFFFFF, TRUE, &mi)); EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    SetLastError(0); mh_zero_a(&mi, MIIM_ID);
    CHECK(!U(GetMenuItemInfoA)(h, 0xFFFFFFFF, FALSE, &mi)); EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    SetLastError(0); mh_zero_a(&mi, MIIM_ID);
    CHECK(!U(GetMenuItemInfoA)(h, 4242, FALSE, &mi)); EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    SetLastError(0); mh_zero_a(&mi, MIIM_ID);
    CHECK(!U(GetMenuItemInfoW)(h, 4242, FALSE, (MENUITEMINFOW *)&mi)); EQ(GetLastError(), ERROR_MENU_ITEM_NOT_FOUND);
    M_DESTROY(h);
}

TEST(menus_mii_get_invalid_menu_errors) {
    REQ_INFO();
    for (int i = 0; i < MH_NBOGUS; i++) {
        MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_ID);
        SetLastError(0);
        CHECKF(!U(GetMenuItemInfoA)(mh_bogus[i], 0, TRUE, &mi), "bogus %d", i);
        EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_ID);
        SetLastError(0);
        CHECKF(!U(GetMenuItemInfoW)(mh_bogus[i], 0, TRUE, &mw), "bogus %d W", i);
        EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    }
}

TEST(menus_mii_set_insert_invalid_menu) {
    REQ_INFO();
    for (int i = 0; i < MH_NBOGUS; i++) {
        MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_ID | MIIM_STRING); mi.dwTypeData = "x"; mi.wID = 1;
        CHECKF(!U(SetMenuItemInfoA)(mh_bogus[i], 0, TRUE, &mi), "set bogus %d", i);
        CHECKF(!U(InsertMenuItemA)(mh_bogus[i], 0, TRUE, &mi), "ins bogus %d", i);
        MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_ID | MIIM_STRING); mw.dwTypeData = L"x"; mw.wID = 1;
        CHECKF(!U(SetMenuItemInfoW)(mh_bogus[i], 0, TRUE, &mw), "setw bogus %d", i);
        CHECKF(!U(InsertMenuItemW)(mh_bogus[i], 0, TRUE, &mw), "insw bogus %d", i);
    }
}

TEST(menus_mii_state_invalid_bits) {
    REQ_INFO();
    static const UINT bad[] = { 0x4, 0x10, 0x20, 0x40, 0x100, 0x200, 0x400, 0x800, 0x2000, 0x4000, 0x8000, 0x10000, 0x80000000, 0xFFFFFFFF };
    static const UINT good[] = { 0, 1, 2, 3, 8, 0x80, 0x1000, 0x1083 | 0, 0x108B, MFS_GRAYED, MFS_CHECKED, MFS_HILITE, MFS_DEFAULT, MFS_DISABLED };
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUITEMINFOA mi; BOOL r; DWORD e;
        mh_zero_a(&mi, MIIM_STATE); mi.fState = bad[i];
        e = err_set(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "set state 0x%x r=%d e=%lu", bad[i], r, e);
        mh_zero_a(&mi, MIIM_STATE | MIIM_ID); mi.fState = bad[i]; mi.wID = 9;
        e = err_ins(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "ins state 0x%x r=%d e=%lu", bad[i], r, e);
        /* state bits are ignored when MIIM_STATE is not requested */
        mh_zero_a(&mi, MIIM_ID); mi.fState = bad[i]; mi.wID = 100;
        CHECKF(U(SetMenuItemInfoA)(h, 0, TRUE, &mi), "set w/o MIIM_STATE with state 0x%x", bad[i]);
    }
    EQ(M_CNT(h), 1);
    for (unsigned i = 0; i < sizeof good / sizeof *good; i++) {
        MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_STATE); mi.fState = good[i];
        CHECKF(U(SetMenuItemInfoA)(h, 0, TRUE, &mi), "good state 0x%x", good[i]);
    }
    M_DESTROY(h);
}

TEST(menus_mii_state_roundtrip) {
    REQ_INFO();
    static const UINT st[] = { 0, MFS_GRAYED, MFS_DISABLED, MFS_CHECKED, MFS_HILITE, MFS_DEFAULT, MFS_CHECKED | MFS_GRAYED, MFS_CHECKED | MFS_DEFAULT };
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof st / sizeof *st; i++) {
        MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_STATE); mi.fState = st[i];
        CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
        mh_init_a(&mi, MIIM_STATE);
        CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
        CHECKF((mi.fState & 0x108B) == st[i] || ((mi.fState & MFS_GRAYED) && (st[i] & MFS_GRAYED)), "state in 0x%x out 0x%x", st[i], mi.fState);
        EQ(mi.fState & ~0x108Bu, 0);
        EQ(mi.fMask, MIIM_STATE);
    }
    M_DESTROY(h);
}

TEST(menus_mii_ftype_invalid_bits) {
    REQ_INFO();
    static const UINT bad[] = { 0x1, 0x2, 0x8, 0x10, 0x80, 0x400, 0x1000, 0x8000, 0x40000, 0x80000, 0x80000000, 0xFFFFFFFF };
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUITEMINFOA mi; BOOL r; DWORD e;
        mh_zero_a(&mi, MIIM_FTYPE); mi.fType = bad[i];
        e = err_set(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "set ftype 0x%x r=%d e=%lu", bad[i], r, e);
        mh_zero_a(&mi, MIIM_FTYPE | MIIM_ID); mi.fType = bad[i]; mi.wID = 9;
        e = err_ins(h, &mi, &r); CHECKF(!r && e == ERROR_INVALID_PARAMETER, "ins ftype 0x%x r=%d e=%lu", bad[i], r, e);
        /* ignored when MIIM_FTYPE / MIIM_TYPE not requested */
        mh_zero_a(&mi, MIIM_ID); mi.fType = bad[i]; mi.wID = 100;
        CHECKF(U(SetMenuItemInfoA)(h, 0, TRUE, &mi), "set w/o FTYPE with type 0x%x", bad[i]);
    }
    EQ(M_CNT(h), 1);
    M_DESTROY(h);
}

TEST(menus_mii_ftype_bitmap_requires_miim_type) {
    REQ_INFO();
    HMENU h = mh_make(1);
    MENUITEMINFOA mi; BOOL r; DWORD e;
    mh_zero_a(&mi, MIIM_FTYPE); mi.fType = MFT_BITMAP;
    e = err_set(h, &mi, &r); CHECK(!r); EQ(e, ERROR_INVALID_PARAMETER);
    mh_zero_a(&mi, MIIM_FTYPE | MIIM_ID); mi.fType = MFT_BITMAP;
    e = err_ins(h, &mi, &r); CHECK(!r); EQ(e, ERROR_INVALID_PARAMETER);
    M_DESTROY(h);
}

TEST(menus_mii_ftype_valid_roundtrip) {
    REQ_INFO();
    static const UINT ft[] = { MFT_STRING, MFT_MENUBREAK, MFT_MENUBARBREAK, MFT_RADIOCHECK, MFT_RIGHTJUSTIFY,
                               MFT_OWNERDRAW, MFT_MENUBREAK | MFT_RADIOCHECK, MFT_STRING | MFT_RIGHTJUSTIFY };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof ft / sizeof *ft; i++) {
        MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_FTYPE | MIIM_ID); mi.fType = ft[i]; mi.wID = 10 + i;
        CHECKF(U(InsertMenuItemA)(h, 0xFFFFFFFF, TRUE, &mi), "insert ftype 0x%x", ft[i]);
        mh_init_a(&mi, MIIM_FTYPE);
        CHECK(U(GetMenuItemInfoA)(h, i, TRUE, &mi));
        EQ(mi.fType & ft[i], ft[i]);
        EQ(mi.fMask, MIIM_FTYPE);
    }
    M_DESTROY(h);
}

TEST(menus_mii_separator_item) {
    REQ_INFO();
    HMENU h = M_POP();
    CHECK(ins_a(h, 0, MIIM_FTYPE, 0, NULL, MFT_SEPARATOR, 0));
    EQ(M_CNT(h), 1);
    MENUITEMINFOA mi; mh_init_a(&mi, MIIM_FTYPE | MIIM_STRING); char b[8] = "zz"; mi.dwTypeData = b; mi.cch = 8;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(mi.fType & MFT_SEPARATOR);
    EQ(mi.cch, 0);
    EQ(b[0], 0);
    CHECK(mh_state(h, 0, MF_BYPOSITION) & MF_SEPARATOR);
    M_DESTROY(h);
}

TEST(menus_mii_insert_string_a_w_cross) {
    REQ_INFO();
    HMENU h = M_POP();
    CHECK(ins_a(h, 0xFFFFFFFF, MIIM_ID | MIIM_STRING, 1, "FromAnsi", 0, 0));
    CHECK(ins_w(h, 0xFFFFFFFF, MIIM_ID | MIIM_STRING, 2, L"FromWide", 0, 0));
    char a[32]; WCHAR w[32];
    MENUITEMINFOA ma; MENUITEMINFOW mw;
    for (int i = 0; i < 2; i++) {
        mh_zero_a(&ma, MIIM_STRING); ma.dwTypeData = a; ma.cch = 32;
        CHECK(U(GetMenuItemInfoA)(h, i, TRUE, &ma)); EQ(ma.cch, 8);
        mh_zero_w(&mw, MIIM_STRING); mw.dwTypeData = w; mw.cch = 32;
        CHECK(U(GetMenuItemInfoW)(h, i, TRUE, &mw)); EQ(mw.cch, 8);
        CHECK(!lstrcmpW(w, i ? L"FromWide" : L"FromAnsi"));
        CHECK(!strcmp(a, i ? "FromWide" : "FromAnsi"));
    }
    M_DESTROY(h);
}

TEST(menus_mii_string_length_query) {
    REQ_INFO();
    HMENU h = M_POP();
    ins_w(h, 0, MIIM_ID | MIIM_STRING, 1, L"Twelve chars", 0, 0);
    MENUITEMINFOA ma; mh_zero_a(&ma, MIIM_STRING); ma.dwTypeData = NULL; ma.cch = 0x77;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &ma)); EQ(ma.cch, 12); CHECK(ma.dwTypeData == NULL);
    MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_STRING); mw.dwTypeData = NULL; mw.cch = 0x77;
    CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mw)); EQ(mw.cch, 12); CHECK(mw.dwTypeData == NULL);
    /* cch == 0 with a non-NULL buffer: length only, buffer pointer reset */
    WCHAR w[4] = { 1, 1, 1, 1 };
    mh_zero_w(&mw, MIIM_STRING); mw.dwTypeData = w; mw.cch = 0;
    CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mw)); EQ(mw.cch, 12); CHECK(mw.dwTypeData == NULL); EQ(w[0], 1);
    M_DESTROY(h);
}

TEST(menus_mii_string_truncation_w) {
    REQ_INFO();
    HMENU h = M_POP();
    ins_w(h, 0, MIIM_ID | MIIM_STRING, 1, L"Hello", 0, 0);
    static const struct { UINT cch; UINT want; const WCHAR *s; } t[] = {
        { 1, 0, L"" }, { 2, 1, L"H" }, { 3, 2, L"He" }, { 5, 4, L"Hell" }, { 6, 5, L"Hello" }, { 7, 5, L"Hello" }, { 64, 5, L"Hello" } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[70]; wmemset(w, 0x5A5A, 70);
        MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_STRING); mw.dwTypeData = w; mw.cch = t[i].cch;
        CHECKF(U(GetMenuItemInfoW)(h, 0, TRUE, &mw), "cch %u", t[i].cch);
        CHECKF(mw.cch == t[i].want, "cch %u -> %u want %u", t[i].cch, mw.cch, t[i].want);
        CHECKF(!lstrcmpW(w, t[i].s), "cch %u text", t[i].cch);
        CHECKF(w[t[i].want + 1 < 70 ? t[i].want + 1 : 69] == 0x5A5A || t[i].want + 1 >= 70, "cch %u overrun", t[i].cch);
    }
    M_DESTROY(h);
}

TEST(menus_mii_string_truncation_a_full_buffers) {
    REQ_INFO();
    HMENU h = M_POP();
    ins_a(h, 0, MIIM_ID | MIIM_STRING, 1, "Hello", 0, 0);
    char b[16]; memset(b, 'Q', 16);
    MENUITEMINFOA ma; mh_zero_a(&ma, MIIM_STRING); ma.dwTypeData = b; ma.cch = 6;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &ma)); EQ(ma.cch, 5); EXPECT_STR(b, "Hello");
    memset(b, 'Q', 16);
    mh_zero_a(&ma, MIIM_STRING); ma.dwTypeData = b; ma.cch = 16;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &ma)); EQ(ma.cch, 5); EXPECT_STR(b, "Hello");
    memset(b, 'Q', 16);
    mh_zero_a(&ma, MIIM_STRING); ma.dwTypeData = b; ma.cch = 3;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &ma));
    t_note("A truncation cch=3: cch=%u text=\"%s\"", ma.cch, b);
    M_DESTROY(h);
}

TEST(menus_mii_string_on_nonstring_items) {
    REQ_INFO();
    HMENU h = M_POP(); HBITMAP bm = mh_bmp();
    ins_w(h, 0, MIIM_FTYPE, 0, NULL, MFT_SEPARATOR, 0);
    MENUITEMINFOW in; mh_zero_w(&in, MIIM_ID | MIIM_BITMAP); in.wID = 4; in.hbmpItem = bm;
    CHECK(U(InsertMenuItemW)(h, 1, TRUE, &in));
    for (int i = 0; i < 2; i++) {
        WCHAR w[8] = { 9, 9 };
        MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_STRING); mw.dwTypeData = w; mw.cch = 8;
        CHECK(U(GetMenuItemInfoW)(h, i, TRUE, &mw));
        EQ(mw.cch, 0); EQ(w[0], 0);
    }
    M_DESTROY(h); DeleteObject(bm);
}

TEST(menus_mii_set_ignores_cch_field) {
    REQ_INFO();
    HMENU h = M_POP();
    ins_a(h, 0, MIIM_STRING | MIIM_ID, 1, "x", 0, 0);
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_STRING); mi.dwTypeData = "LongerText"; mi.cch = 2;
    CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    char b[32]; EQ(U(GetMenuStringA)(h, 0, b, 32, MF_BYPOSITION), 10); EXPECT_STR(b, "LongerText");
    mh_zero_a(&mi, MIIM_STRING); mi.dwTypeData = "Short"; mi.cch = 1000;
    CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(U(GetMenuStringA)(h, 0, b, 32, MF_BYPOSITION), 5); EXPECT_STR(b, "Short");
    M_DESTROY(h);
}

TEST(menus_mii_set_single_fields_do_not_disturb_others) {
    REQ_INFO();
    HMENU h = M_POP();
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_DATA);
    mi.wID = 77; mi.dwTypeData = "keep"; mi.fState = MFS_CHECKED; mi.dwItemData = 0x1234;
    CHECK(U(InsertMenuItemA)(h, 0, TRUE, &mi));
    mh_zero_a(&mi, MIIM_DATA); mi.dwItemData = 0x9999;
    CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    mh_zero_a(&mi, MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_DATA); char b[16]; mi.dwTypeData = b; mi.cch = 16;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(mi.wID, 77); EXPECT_STR(b, "keep"); CHECK(mi.fState & MFS_CHECKED); EQ(mi.dwItemData, 0x9999);
    mh_zero_a(&mi, MIIM_STATE); mi.fState = MFS_GRAYED;
    CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    mh_zero_a(&mi, MIIM_ID | MIIM_STATE | MIIM_DATA);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(mi.wID, 77); CHECK(mi.fState & MFS_GRAYED); CHECK(!(mi.fState & MFS_CHECKED)); EQ(mi.dwItemData, 0x9999);
    mh_zero_a(&mi, MIIM_ID); mi.wID = 78;
    CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(U(GetMenuItemID)(h, 0), 78);
    U(GetMenuStringA)(h, 78, b, 16, 0); EXPECT_STR(b, "keep");
    M_DESTROY(h);
}

TEST(menus_mii_itemdata_64bit) {
    REQ_INFO();
    static const ULONG_PTR d[] = { 0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000ULL, 0x123456789ABCDEF0ULL, (ULONG_PTR)-1 };
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof d / sizeof *d; i++) {
        MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_DATA); mi.dwItemData = d[i];
        CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
        mh_init_a(&mi, MIIM_DATA);
        CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
        CHECKF(mi.dwItemData == d[i], "itemdata %llx -> %llx", (unsigned long long)d[i], (unsigned long long)mi.dwItemData);
        MENUITEMINFOW mw; mh_init_w(&mw, MIIM_DATA);
        CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mw)); CHECK(mw.dwItemData == d[i]);
    }
    M_DESTROY(h);
}

TEST(menus_mii_id_boundaries) {
    REQ_INFO();
    static const UINT ids[] = { 0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    HMENU h = M_POP();
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) {
        CHECK(ins_a(h, 0xFFFFFFFF, MIIM_ID | MIIM_STRING, ids[i], "i", 0, 0));
        MENUITEMINFOA mi; mh_init_a(&mi, MIIM_ID);
        CHECK(U(GetMenuItemInfoA)(h, i, TRUE, &mi));
        EQ(mi.wID, ids[i]);
        EQ(U(GetMenuItemID)(h, i), ids[i]);
    }
    for (unsigned i = 0; i < sizeof ids / sizeof *ids - 1; i++) {
        MENUITEMINFOA mi; mh_init_a(&mi, MIIM_ID);
        CHECKF(U(GetMenuItemInfoA)(h, ids[i], FALSE, &mi), "find id %x", ids[i]);
        EQ(mi.wID, ids[i]);
    }
    M_DESTROY(h);
}

TEST(menus_mii_unrequested_fields_untouched) {
    REQ_INFO();
    HMENU h = mh_make(1);
    MENUITEMINFOA mi; mh_init_a(&mi, MIIM_ID);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(mi.wID, 100); EQ(mi.fMask, MIIM_ID);
    EQ(mi.fType, 0xCCCCCCCC); EQ(mi.fState, 0xCCCCCCCC);
    EQ(mi.dwItemData, 0xCCCCCCCCCCCCCCCCULL);
    EQ(mi.hbmpChecked, 0xCCCCCCCCCCCCCCCCULL);
    EQ(mi.hbmpItem, 0xCCCCCCCCCCCCCCCCULL);
    EQ(mi.cch, 0xCCCCCCCC);
    EQ(mi.hSubMenu, 0);       /* always written (NULL when not requested / no submenu) */
    M_DESTROY(h);
}

TEST(menus_mii_get_fmask_preserved) {
    REQ_INFO();
    static const UINT masks[] = { MIIM_ID, MIIM_STATE, MIIM_SUBMENU, MIIM_CHECKMARKS, MIIM_DATA, MIIM_STRING, MIIM_FTYPE,
                                  MIIM_BITMAP, MIIM_ID | MIIM_STATE | MIIM_DATA, MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE | MIIM_ID };
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof masks / sizeof *masks; i++) {
        MENUITEMINFOA mi; char b[16]; mh_zero_a(&mi, masks[i]); mi.dwTypeData = b; mi.cch = 16;
        CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
        CHECKF(mi.fMask == masks[i], "mask in 0x%x out 0x%x", masks[i], mi.fMask);
    }
    M_DESTROY(h);
}

TEST(menus_mii_get_type_mask_semantics) {
    REQ_INFO();
    HMENU h = mh_make(1);
    MENUITEMINFOA mi; char b[16]; mh_zero_a(&mi, MIIM_TYPE); mi.dwTypeData = b; mi.cch = 16;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(mi.fMask & MIIM_TYPE);
    EQ(mi.fMask & (MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE), 0);
    EQ(mi.fType & (MFT_BITMAP | MFT_SEPARATOR | MFT_OWNERDRAW), 0);
    EQ(mi.cch, 5); EXPECT_STR(b, "Item0");
    M_DESTROY(h);
}

TEST(menus_mii_get_type_on_bitmap_item) {
    REQ_INFO();
    HMENU h = M_POP(); HBITMAP bm = mh_bmp();
    MENUITEMINFOA in; mh_zero_a(&in, MIIM_TYPE | MIIM_ID); in.fType = MFT_BITMAP; in.dwTypeData = (LPSTR)bm; in.wID = 9;
    CHECK(U(InsertMenuItemA)(h, 0, TRUE, &in));
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_TYPE); 
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(mi.fType & MFT_BITMAP);
    CHECK(mi.dwTypeData == (LPSTR)bm);
    mh_zero_a(&mi, MIIM_BITMAP);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(mi.hbmpItem == bm);
    CHECK(mh_state(h, 9, 0) & MF_BITMAP);
    M_DESTROY(h); DeleteObject(bm);
}

TEST(menus_mii_bitmap_with_string) {
    REQ_INFO();
    HMENU h = M_POP(); HBITMAP bm = mh_bmp();
    MENUITEMINFOW in; mh_zero_w(&in, MIIM_ID | MIIM_STRING | MIIM_BITMAP); in.wID = 9; in.dwTypeData = L"WithBitmap"; in.hbmpItem = bm;
    CHECK(U(InsertMenuItemW)(h, 0, TRUE, &in));
    WCHAR w[32]; MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE); mw.dwTypeData = w; mw.cch = 32;
    CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mw));
    CHECK(mw.hbmpItem == bm); EQ(mw.cch, 10); CHECK(!lstrcmpW(w, L"WithBitmap"));
    EQ(mw.fType & MFT_BITMAP, 0);
    M_DESTROY(h); DeleteObject(bm);
}

TEST(menus_mii_bitmap_pseudo_handles) {
    REQ_INFO();
    static const ULONG_PTR v[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, (ULONG_PTR)-1 };  /* HBMMENU_* and callback */
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_BITMAP); mw.hbmpItem = (HBITMAP)v[i];
        CHECKF(U(SetMenuItemInfoW)(h, 0, TRUE, &mw), "pseudo bitmap %llu", (unsigned long long)v[i]);
        mh_zero_w(&mw, MIIM_BITMAP);
        CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mw));
        if ((ULONG_PTR)mw.hbmpItem != v[i]) t_note("pseudo bitmap %llu read back as %p", (unsigned long long)v[i], (void *)mw.hbmpItem);
    }
    M_DESTROY(h);
}

TEST(menus_mii_bitmap_invalid_handles) {
    REQ_INFO();
    static const ULONG_PTR v[] = { 12, 13, 0x100, 0x1234567, 0xDEADBEEF, 0x7FFFFFFF, 0xFFFFFFFF, (ULONG_PTR)-2 };
    HMENU h = mh_make(1);
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_BITMAP); mw.hbmpItem = (HBITMAP)v[i];
        SetLastError(0);
        CHECKF(!U(SetMenuItemInfoW)(h, 0, TRUE, &mw), "bad bitmap %llx accepted", (unsigned long long)v[i]);
        EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        /* not looked at when MIIM_BITMAP is absent */
        mh_zero_w(&mw, MIIM_ID); mw.hbmpItem = (HBITMAP)v[i]; mw.wID = 100;
        CHECKF(U(SetMenuItemInfoW)(h, 0, TRUE, &mw), "bitmap ignored w/o MIIM_BITMAP");
    }
    M_DESTROY(h);
}

TEST(menus_mii_bitmap_wrong_gdi_type) {
    REQ_INFO();
    HMENU h = mh_make(1);
    HBRUSH br = CreateSolidBrush(RGB(1, 2, 3));
    MENUITEMINFOW mw; mh_zero_w(&mw, MIIM_BITMAP); mw.hbmpItem = (HBITMAP)br;
    t_note("MIIM_BITMAP with an HBRUSH accepted: %d", U(SetMenuItemInfoW)(h, 0, TRUE, &mw));
    DeleteObject(br);
    HBITMAP bm = mh_bmp();
    DeleteObject(bm);
    mh_zero_w(&mw, MIIM_BITMAP); mw.hbmpItem = bm;
    SetLastError(0);
    CHECK(!U(SetMenuItemInfoW)(h, 0, TRUE, &mw));    /* deleted bitmap handle */
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    M_DESTROY(h);
}

TEST(menus_mii_checkmarks_roundtrip) {
    REQ_INFO();
    HMENU h = mh_make(1); HBITMAP a = mh_bmp(), b = mh_bmp();
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_CHECKMARKS); mi.hbmpChecked = a; mi.hbmpUnchecked = b;
    CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    mh_init_a(&mi, MIIM_CHECKMARKS);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(mi.hbmpChecked == a); CHECK(mi.hbmpUnchecked == b);
    mh_zero_a(&mi, MIIM_CHECKMARKS);   /* clear */
    CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    mh_init_a(&mi, MIIM_CHECKMARKS);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(mi.hbmpChecked == NULL); CHECK(mi.hbmpUnchecked == NULL);
    M_DESTROY(h); DeleteObject(a); DeleteObject(b);
}

TEST(menus_mii_checkmarks_invalid) {
    REQ_INFO();
    HMENU h = mh_make(1); HBITMAP a = mh_bmp();
    static const ULONG_PTR bad[] = { 1, 12, 0xDEADBEEF, 0x1234567, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_CHECKMARKS); mi.hbmpChecked = (HBITMAP)bad[i]; mi.hbmpUnchecked = a;
        SetLastError(0);
        CHECKF(!U(SetMenuItemInfoA)(h, 0, TRUE, &mi), "bad checked %llx", (unsigned long long)bad[i]);
        EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        mh_zero_a(&mi, MIIM_CHECKMARKS); mi.hbmpChecked = a; mi.hbmpUnchecked = (HBITMAP)bad[i];
        SetLastError(0);
        CHECKF(!U(SetMenuItemInfoA)(h, 0, TRUE, &mi), "bad unchecked %llx", (unsigned long long)bad[i]);
        EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        mh_zero_a(&mi, MIIM_ID); mi.hbmpChecked = (HBITMAP)bad[i]; mi.wID = 100;
        CHECK(U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    }
    M_DESTROY(h); DeleteObject(a);
}

TEST(menus_mii_submenu_valid_and_invalid) {
    REQ_INFO();
    HMENU top = mh_make(1), s = M_POP();
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_SUBMENU); mi.hSubMenu = s;
    CHECK(U(SetMenuItemInfoA)(top, 0, TRUE, &mi));
    CHECK(U(GetSubMenu)(top, 0) == s);
    mh_init_a(&mi, MIIM_SUBMENU);
    CHECK(U(GetMenuItemInfoA)(top, 0, TRUE, &mi)); CHECK(mi.hSubMenu == s);
    CHECK(mh_state(top, 0, MF_BYPOSITION) & MF_POPUP);
    for (int i = 1; i < MH_NBOGUS; i++) {
        mh_zero_a(&mi, MIIM_SUBMENU); mi.hSubMenu = mh_bogus[i];
        SetLastError(0);
        CHECKF(!U(SetMenuItemInfoA)(top, 0, TRUE, &mi), "bogus submenu %d", i);
        EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        mh_zero_a(&mi, MIIM_SUBMENU | MIIM_ID); mi.hSubMenu = mh_bogus[i]; mi.wID = 3;
        CHECKF(!U(InsertMenuItemA)(top, 0, TRUE, &mi), "insert bogus submenu %d", i);
    }
    CHECK(U(GetSubMenu)(top, 0) == s);
    mh_zero_a(&mi, MIIM_ID); mi.hSubMenu = (HMENU)0xDEADBEEF; mi.wID = 4;
    CHECK(U(SetMenuItemInfoA)(top, 0, TRUE, &mi));    /* ignored without MIIM_SUBMENU */
    M_DESTROY(top);
}

TEST(menus_mii_submenu_get_reports_null_when_unrequested) {
    REQ_INFO();
    HMENU top = M_NEW(), s = M_POP();
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "S");
    MENUITEMINFOA mi; mh_init_a(&mi, MIIM_ID);
    CHECK(U(GetMenuItemInfoA)(top, 0, TRUE, &mi));
    CHECK(mi.hSubMenu == NULL);
    mh_init_a(&mi, MIIM_SUBMENU);
    CHECK(U(GetMenuItemInfoA)(top, 0, TRUE, &mi));
    CHECK(mi.hSubMenu == s);
    M_DESTROY(top);
}

TEST(menus_mii_submenu_clear_with_null) {
    REQ_INFO();
    HMENU top = M_NEW(), s = M_POP();
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "S");
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_SUBMENU); mi.hSubMenu = NULL;
    CHECK(U(SetMenuItemInfoA)(top, 0, TRUE, &mi));
    CHECK(U(GetSubMenu)(top, 0) == NULL);
    CHECK(!(mh_state(top, 0, MF_BYPOSITION) & MF_POPUP));
    t_note("submenu still alive after detach via MIIM_SUBMENU=NULL: %d", M_IS(s));
    M_DESTROY(top); if (M_IS(s)) M_DESTROY(s);
}

TEST(menus_mii_insert_positions_and_defaults) {
    REQ_INFO();
    HMENU h = M_POP();
    CHECK(ins_a(h, 0, MIIM_STRING, 0, "noid", 0, 0));
    MENUITEMINFOA mi; mh_init_a(&mi, MIIM_ID | MIIM_STATE | MIIM_DATA);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(mi.wID, 0); EQ(mi.fState, 0); EQ(mi.dwItemData, 0);
    CHECK(ins_a(h, 0, MIIM_ID, 2, NULL, 0, 0));     /* at 0 */
    CHECK(ins_a(h, 2, MIIM_ID, 3, NULL, 0, 0));     /* at end */
    CHECK(ins_a(h, 0xFFFFFFFF, MIIM_ID, 4, NULL, 0, 0));
    CHECK(ins_a(h, 1, MIIM_ID, 5, NULL, 0, 0));
    UINT want[5] = { 2, 5, 0, 3, 4 };
    EQ(M_CNT(h), 5);
    for (int i = 0; i < 5; i++) EQ(U(GetMenuItemID)(h, i), want[i]);
    M_DESTROY(h);
}

TEST(menus_mii_insert_bycommand_before_item) {
    REQ_INFO();
    HMENU h = mh_make(2);
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_ID | MIIM_STRING); mi.wID = 9; mi.dwTypeData = "ins";
    CHECK(U(InsertMenuItemA)(h, 101, FALSE, &mi));
    EQ(U(GetMenuItemID)(h, 0), 100); EQ(U(GetMenuItemID)(h, 1), 9); EQ(U(GetMenuItemID)(h, 2), 101);
    M_DESTROY(h);
}

TEST(menus_mii_set_by_command_and_position) {
    REQ_INFO();
    HMENU h = mh_make(3);
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_STRING); mi.dwTypeData = "cmd";
    CHECK(U(SetMenuItemInfoA)(h, 101, FALSE, &mi));
    char b[16];
    U(GetMenuStringA)(h, 1, b, 16, MF_BYPOSITION); EXPECT_STR(b, "cmd");
    mi.dwTypeData = "pos";
    CHECK(U(SetMenuItemInfoA)(h, 2, TRUE, &mi));
    U(GetMenuStringA)(h, 2, b, 16, MF_BYPOSITION); EXPECT_STR(b, "pos");
    CHECK(!U(SetMenuItemInfoA)(h, 3, TRUE, &mi));
    CHECK(!U(SetMenuItemInfoA)(h, 999, FALSE, &mi));
    CHECK(!U(SetMenuItemInfoA)(h, 0xFFFFFFFF, TRUE, &mi));
    CHECK(!U(SetMenuItemInfoA)(h, 0xFFFFFFFF, FALSE, &mi));
    M_DESTROY(h);
}

TEST(menus_mii_fbyposition_is_boolean) {
    REQ_INFO();
    HMENU h = mh_make(2);
    MENUITEMINFOA mi;
    /* any non-zero BOOL means "by position" */
    static const BOOL v[] = { 1, 2, 0x400, -1, 0x80000000 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        mh_init_a(&mi, MIIM_ID);
        CHECKF(U(GetMenuItemInfoA)(h, 1, v[i], &mi), "fByPosition=%d", v[i]);
        EQ(mi.wID, 101);
    }
    M_DESTROY(h);
}

TEST(menus_mii_default_item_via_state) {
    REQ_INFO();
    REQUIRE(U(GetMenuDefaultItem));
    HMENU h = mh_make(3);
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_STATE); mi.fState = MFS_DEFAULT;
    CHECK(U(SetMenuItemInfoA)(h, 1, TRUE, &mi));
    EQ(U(GetMenuDefaultItem)(h, TRUE, 0), 1);
    EQ(U(GetMenuDefaultItem)(h, FALSE, 0), 101);
    CHECK(mh_state(h, 1, MF_BYPOSITION) & MF_DEFAULT);
    mh_zero_a(&mi, MIIM_STATE); mi.fState = MFS_DEFAULT;
    CHECK(U(SetMenuItemInfoA)(h, 2, TRUE, &mi));      /* moves the default */
    t_note("default after setting MFS_DEFAULT on a second item: pos %u (state of first 0x%x)", U(GetMenuDefaultItem)(h, TRUE, 0), mh_state(h, 1, MF_BYPOSITION));
    M_DESTROY(h);
}

TEST(menus_mii_roundtrip_all_fields_w) {
    REQ_INFO();
    HMENU h = M_POP(), s = M_POP(); HBITMAP a = mh_bmp(), b = mh_bmp(), ib = mh_bmp();
    MENUITEMINFOW in; mh_zero_w(&in, MIIM_ID | MIIM_STATE | MIIM_SUBMENU | MIIM_CHECKMARKS | MIIM_STRING | MIIM_FTYPE | MIIM_DATA | MIIM_BITMAP);
    in.wID = 0x4321; in.fState = MFS_CHECKED | MFS_DEFAULT; in.hSubMenu = s; in.hbmpChecked = a; in.hbmpUnchecked = b;
    in.dwTypeData = L"Allé"; in.fType = MFT_RADIOCHECK | MFT_RIGHTJUSTIFY; in.dwItemData = 0x1122334455667788ULL; in.hbmpItem = ib;
    CHECK(U(InsertMenuItemW)(h, 0, TRUE, &in));
    WCHAR w[32];
    MENUITEMINFOW mi; mh_init_w(&mi, 0x1FF & ~MIIM_TYPE); mi.dwTypeData = w; mi.cch = 32;
    CHECK(U(GetMenuItemInfoW)(h, 0, TRUE, &mi));
    EQ(mi.wID, 0x4321);
    EQ(mi.fState & (MFS_CHECKED | MFS_DEFAULT), MFS_CHECKED | MFS_DEFAULT);
    CHECK(mi.hSubMenu == s); CHECK(mi.hbmpChecked == a); CHECK(mi.hbmpUnchecked == b);
    CHECK(!lstrcmpW(w, L"Allé")); EQ(mi.cch, 4);
    EQ(mi.fType & (MFT_RADIOCHECK | MFT_RIGHTJUSTIFY), MFT_RADIOCHECK | MFT_RIGHTJUSTIFY);
    EQ(mi.dwItemData, 0x1122334455667788ULL); CHECK(mi.hbmpItem == ib);
    M_DESTROY(h); DeleteObject(a); DeleteObject(b); DeleteObject(ib);
}

TEST(menus_mii_roundtrip_all_fields_a) {
    REQ_INFO();
    HMENU h = M_POP(), s = M_POP(); HBITMAP a = mh_bmp(), b = mh_bmp();
    MENUITEMINFOA in; mh_zero_a(&in, MIIM_ID | MIIM_STATE | MIIM_SUBMENU | MIIM_CHECKMARKS | MIIM_STRING | MIIM_FTYPE | MIIM_DATA);
    in.wID = 0x1234; in.fState = MFS_GRAYED; in.hSubMenu = s; in.hbmpChecked = a; in.hbmpUnchecked = b;
    in.dwTypeData = "AnsiAll"; in.fType = MFT_MENUBARBREAK; in.dwItemData = 0xAABBCCDD;
    CHECK(U(InsertMenuItemA)(h, 0, TRUE, &in));
    char buf[32];
    MENUITEMINFOA mi; mh_init_a(&mi, 0x1FF & ~(MIIM_TYPE | MIIM_BITMAP)); mi.dwTypeData = buf; mi.cch = 32;
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(mi.wID, 0x1234); CHECK(mi.fState & MFS_GRAYED); CHECK(mi.hSubMenu == s);
    CHECK(mi.hbmpChecked == a && mi.hbmpUnchecked == b); EXPECT_STR(buf, "AnsiAll"); EQ(mi.cch, 7);
    CHECK(mi.fType & MFT_MENUBARBREAK); EQ(mi.dwItemData, 0xAABBCCDD);
    M_DESTROY(h); DeleteObject(a); DeleteObject(b);
}

TEST(menus_mii_get_by_command_descends_into_popups) {
    REQ_INFO();
    HMENU top = M_NEW(), s = M_POP(), s2 = M_POP();
    mh_add(s2, 700, "deep"); mh_add(s, 600, "mid");
    U(AppendMenuA)(s, MF_POPUP, (UINT_PTR)s2, "P2");
    U(AppendMenuA)(top, MF_POPUP, (UINT_PTR)s, "P1");
    MENUITEMINFOA mi; char b[16];
    mh_zero_a(&mi, MIIM_ID | MIIM_STRING); mi.dwTypeData = b; mi.cch = 16;
    CHECK(U(GetMenuItemInfoA)(top, 700, FALSE, &mi)); EXPECT_STR(b, "deep");
    mh_zero_a(&mi, MIIM_ID | MIIM_STRING); mi.dwTypeData = b; mi.cch = 16;
    CHECK(U(GetMenuItemInfoA)(top, 600, FALSE, &mi)); EXPECT_STR(b, "mid");
    mh_zero_a(&mi, MIIM_ID);
    CHECK(!U(GetMenuItemInfoA)(top, 700, TRUE, &mi));
    M_DESTROY(top);
}

TEST(menus_mii_repeated_get_stable) {
    REQ_INFO();
    HMENU h = mh_make(1);
    for (int i = 0; i < 500; i++) {
        MENUITEMINFOW mi; WCHAR w[16]; mh_zero_w(&mi, MIIM_ID | MIIM_STRING | MIIM_STATE); mi.dwTypeData = w; mi.cch = 16;
        if (!U(GetMenuItemInfoW)(h, 0, TRUE, &mi) || mi.wID != 100 || mi.cch != 5 || lstrcmpW(w, L"Item0")) { CHECK(0); break; }
    }
    M_DESTROY(h);
}

TEST(menus_mii_insert_many_set_all) {
    REQ_INFO();
    HMENU h = M_POP();
    for (int i = 0; i < 400; i++) {
        WCHAR t[16]; wsprintfW(t, L"w%d", i);
        if (!ins_w(h, 0xFFFFFFFF, MIIM_ID | MIIM_STRING, 1000 + i, t, 0, 0)) { CHECK(0); break; }
    }
    EQ(M_CNT(h), 400);
    int bad = 0;
    for (int i = 0; i < 400; i++) {
        WCHAR t[16], g[16]; wsprintfW(t, L"w%d", i);
        MENUITEMINFOW mi; mh_zero_w(&mi, MIIM_ID | MIIM_STRING); mi.dwTypeData = g; mi.cch = 16;
        if (!U(GetMenuItemInfoW)(h, 1000 + i, FALSE, &mi) || mi.wID != 1000 + i || lstrcmpW(t, g)) bad++;
    }
    EQ(bad, 0);
    M_DESTROY(h);
}

TEST(menus_mii_ownerdraw_item) {
    REQ_INFO();
    HMENU h = M_POP();
    MENUITEMINFOA in; mh_zero_a(&in, MIIM_FTYPE | MIIM_ID | MIIM_DATA); in.fType = MFT_OWNERDRAW; in.wID = 12; in.dwItemData = 0xBADC0DE;
    CHECK(U(InsertMenuItemA)(h, 0, TRUE, &in));
    MENUITEMINFOA mi; mh_init_a(&mi, MIIM_FTYPE | MIIM_DATA | MIIM_ID);
    CHECK(U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(mi.fType & MFT_OWNERDRAW); EQ(mi.dwItemData, 0xBADC0DE); EQ(mi.wID, 12);
    M_DESTROY(h);
}

TEST(menus_mii_set_string_null_note) {
    REQ_INFO();
    HMENU h = mh_make(1);
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_STRING); mi.dwTypeData = NULL;
    BOOL r = U(SetMenuItemInfoA)(h, 0, TRUE, &mi);
    char b[16]; int n = U(GetMenuStringA)(h, 0, b, 16, MF_BYPOSITION);
    t_note("SetMenuItemInfo MIIM_STRING with NULL text -> %d, string len now %d", r, n);
    M_DESTROY(h);
}

TEST(menus_mii_mask_zero_set_insert_note) {
    REQ_INFO();
    HMENU h = mh_make(1);
    MENUITEMINFOA mi; mh_zero_a(&mi, 0);
    BOOL r1 = U(SetMenuItemInfoA)(h, 0, TRUE, &mi);
    BOOL r2 = U(InsertMenuItemA)(h, 0, TRUE, &mi);
    t_note("mask 0: Set -> %d Insert -> %d count %d", r1, r2, M_CNT(h));
    M_DESTROY(h);
}

TEST(menus_mii_get_after_destroy_menu) {
    REQ_INFO();
    HMENU h = mh_make(1);
    M_DESTROY(h);
    MENUITEMINFOA mi; mh_zero_a(&mi, MIIM_ID);
    SetLastError(0);
    CHECK(!U(GetMenuItemInfoA)(h, 0, TRUE, &mi));
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    CHECK(!U(SetMenuItemInfoA)(h, 0, TRUE, &mi));
    CHECK(!U(InsertMenuItemA)(h, 0, TRUE, &mi));
}
