/* dialogs: GetDlgItem / GetDlgCtrlID / text / int / check helpers */
#include "dlgutil.h"

#define ERR_INVALID_HWND 1400
#define ERR_CTRL_NOT_FOUND 1421

/* ---------------- GetDlgItem ---------------- */
TEST(dialogs_getdlgitem_valid_ids) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(GetDlgCtrlID); REQUIRE(GetParent); REQUIRE(DestroyWindow);
    HWND dlg = mk_std();
    REQUIRE(dlg);
    for (int id = ID_EDIT; id <= ID_3ST; id++) {
        SET_ERR(0xdead);
        HWND c = GetDlgItem(dlg, id);
        CHECKF(c != NULL, "id %d", id);
        if (c) {
            EQ(GetParent(c), dlg);
            EQ(GetDlgCtrlID(c), id);
            EQ(GetDlgItem(dlg, id), c);   /* repeat is stable */
        }
    }
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitem_distinct_handles) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND h[10];
    for (int i = 0; i < 10; i++) h[i] = GetDlgItem(dlg, ID_EDIT + i);
    for (int i = 0; i < 10; i++) for (int j = i + 1; j < 10; j++) CHECK(h[i] != h[j]);
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitem_missing_ids) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    static const int ids[] = { 0, 1, 2, 99, 110, 111, 1000, 0x7fff, 0x10000, 0x10064, 0x7fffffff, (int)0x80000000, -2, -100 };
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) {
        SET_ERR(0);
        HWND c = GetDlgItem(dlg, ids[i]);
        CHECKF(c == NULL, "id %d", ids[i]);
        EXPECT_ERR(c, ERR_CTRL_NOT_FOUND);
    }
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitem_id_not_truncated) {
    /* 32-bit ids: 0x10064 must not alias 100 */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CHECK(GetDlgItem(dlg, 0x10000 + ID_EDIT) == NULL);
    CHECK(GetDlgItem(dlg, ID_EDIT - 0x10000) == NULL);
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitem_null_dialog) {
    REQUIRE(GetDlgItem);
    SET_ERR(0);
    HWND c = GetDlgItem(NULL, 100);
    CHECK(c == NULL);
    EXPECT_ERR(c, ERR_INVALID_HWND);
}
TEST(dialogs_getdlgitem_bogus_dialog) {
    REQUIRE(GetDlgItem);
    static const ULONG_PTR bogus[] = { 0x12345678, 0xdeadbeef, 1, 2, 0xFFFF, 0x7fffffff, (ULONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof bogus / sizeof *bogus; i++) {
        SET_ERR(0);
        HWND c = GetDlgItem((HWND)bogus[i], 100);
        CHECKF(c == NULL, "bogus %llx", (unsigned long long)bogus[i]);
        EXPECT_ERR(c, ERR_INVALID_HWND);
    }
}
TEST(dialogs_getdlgitem_destroyed_dialog) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(DestroyWindow); REQUIRE(IsWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND e = GetDlgItem(dlg, ID_EDIT);
    CHECK(e != NULL);
    CHECK(DestroyWindow(dlg));
    CHECK(!IsWindow(dlg));
    CHECK(!IsWindow(e));
    SET_ERR(0);
    CHECK(GetDlgItem(dlg, ID_EDIT) == NULL);
    EXPECT_ERR(0, ERR_INVALID_HWND);
    SET_ERR(0);
    CHECK(GetDlgCtrlID(e) == 0);
    EXPECT_ERR(0, ERR_INVALID_HWND);
}
TEST(dialogs_getdlgitem_only_direct_children) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(CreateWindowExA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND st = GetDlgItem(dlg, ID_STATIC);
    HWND gc = CreateWindowExA(0, "STATIC", "grand", WS_CHILD, 0, 0, 10, 10, st, (HMENU)(ULONG_PTR)555, GetModuleHandleA(NULL), NULL);
    CHECK(gc != NULL);
    if (gc) {
        SET_ERR(0);
        CHECK(GetDlgItem(dlg, 555) == NULL);
        EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
        CHECK(GetDlgItem(st, 555) == gc);
        CHECK(GetDlgCtrlID(gc) == 555);
    }
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitem_plain_window_parent) {
    /* GetDlgItem works on any window, not just dialogs */
    REQUIRE(GetDlgItem); REQUIRE(CreateWindowExA); REQUIRE(DestroyWindow); REQUIRE(GetDlgCtrlID);
    HWND p = t_make_window(NULL, WS_POPUP, 0, 0, 0, 50, 50, NULL);
    REQUIRE(p);
    HWND c = CreateWindowExA(0, "BUTTON", "b", WS_CHILD, 0, 0, 10, 10, p, (HMENU)(ULONG_PTR)42, GetModuleHandleA(NULL), NULL);
    CHECK(c != NULL);
    CHECK(GetDlgItem(p, 42) == c);
    CHECK(GetDlgCtrlID(c) == 42);
    SET_ERR(0);
    CHECK(GetDlgItem(p, 43) == NULL);
    EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
    DestroyWindow(p);
}
TEST(dialogs_getdlgitem_changed_id) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(SetWindowLongPtrW); REQUIRE(GetDlgCtrlID); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND e = GetDlgItem(dlg, ID_EDIT);
    SetWindowLongPtrW(e, GWLP_ID, 555);
    EQ(GetDlgCtrlID(e), 555);
    EQ(GetDlgItem(dlg, 555), e);
    CHECK(GetDlgItem(dlg, ID_EDIT) == NULL);
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitem_duplicate_ids_first_in_zorder) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItem); REQUIRE(SetWindowLongPtrW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND e = GetDlgItem(dlg, ID_EDIT), s = GetDlgItem(dlg, ID_STATIC);
    SetWindowLongPtrW(s, GWLP_ID, ID_EDIT);   /* duplicate */
    HWND f = GetDlgItem(dlg, ID_EDIT);
    CHECK(f == e);                              /* first child in z-order wins */
    DestroyWindow(dlg);
}

/* ---------------- GetDlgCtrlID ---------------- */
TEST(dialogs_getdlgctrlid_toplevel_zero) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgCtrlID); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SET_ERR(0);
    EQ(GetDlgCtrlID(dlg), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgctrlid_null_and_bogus) {
    REQUIRE(GetDlgCtrlID);
    SET_ERR(0);
    EQ(GetDlgCtrlID(NULL), 0);
    EXPECT_ERR(0, ERR_INVALID_HWND);
    SET_ERR(0);
    EQ(GetDlgCtrlID((HWND)0x12345678), 0);
    EXPECT_ERR(0, ERR_INVALID_HWND);
}
TEST(dialogs_getdlgctrlid_anon_static_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgCtrlID); REQUIRE(GetWindow); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND c = GetWindow(dlg, GW_CHILD);
    int last = 0, n = 0;
    for (; c; c = GetWindow(c, GW_HWNDNEXT)) { last = GetDlgCtrlID(c); n++; }
    EQ(n, 11);
    t_note("id of IDC_STATIC (0xFFFF in template) reads back as 0x%x", last);
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgctrlid_zorder_matches_template) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgCtrlID); REQUIRE(GetWindow); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    HWND c = GetWindow(dlg, GW_CHILD);
    for (int id = ID_EDIT; id <= ID_3ST; id++) {
        CHECKF(c && GetDlgCtrlID(c) == id, "pos id %d", id);
        if (c) c = GetWindow(c, GW_HWNDNEXT);
    }
    DestroyWindow(dlg);
}

/* ---------------- text ---------------- */
TEST(dialogs_dlgitemtext_initial) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemTextA); REQUIRE(GetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[64]; WCHAR w[64];
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 64), 4); EXPECT_STR(a, "edit");
    EQ(GetDlgItemTextW(dlg, ID_EDIT, w, 64), 4); EXPECT_WSTR(w, L"edit");
    EQ(GetDlgItemTextA(dlg, ID_STATIC, a, 64), 11); EXPECT_STR(a, "static text");
    EQ(GetDlgItemTextA(dlg, ID_CHK, a, 64), 5); EXPECT_STR(a, "check");
    EQ(GetDlgItemTextA(dlg, ID_BTN, a, 64), 4); EXPECT_STR(a, "push");
    EQ(GetDlgItemTextA(dlg, ID_LIST, a, 64), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_dlgitemtext_buffer_edges_ansi) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[16];
    memset(a, 'X', sizeof a);
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 5), 4); EXPECT_STR(a, "edit");        /* exact */
    memset(a, 'X', sizeof a);
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 4), 3); EXPECT_STR(a, "edi");         /* exact-1 */
    memset(a, 'X', sizeof a);
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 2), 1); EXPECT_STR(a, "e");
    memset(a, 'X', sizeof a);
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 1), 0); CHECK(a[0] == 0);              /* 1 -> empty */
    memset(a, 'X', sizeof a);
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 0), 0); if (a[0] != 'X') t_note("cch 0 wrote the buffer");
    memset(a, 'X', sizeof a);
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 6), 4); EXPECT_STR(a, "edit"); CHECK(a[5] == 'X'); /* no overrun */
    DestroyWindow(dlg);
}
TEST(dialogs_dlgitemtext_buffer_edges_wide) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    WCHAR w[16];
    for (int i = 0; i < 16; i++) w[i] = 'X';
    EQ(GetDlgItemTextW(dlg, ID_EDIT, w, 5), 4); EXPECT_WSTR(w, L"edit");
    for (int i = 0; i < 16; i++) w[i] = 'X';
    EQ(GetDlgItemTextW(dlg, ID_EDIT, w, 4), 3); EXPECT_WSTR(w, L"edi");
    for (int i = 0; i < 16; i++) w[i] = 'X';
    EQ(GetDlgItemTextW(dlg, ID_EDIT, w, 1), 0); CHECK(w[0] == 0);
    for (int i = 0; i < 16; i++) w[i] = 'X';
    EQ(GetDlgItemTextW(dlg, ID_EDIT, w, 0), 0); if (w[0] != 'X') t_note("cch 0 wrote the buffer (W)");
    DestroyWindow(dlg);
}
TEST(dialogs_dlgitemtext_missing_control_clears_buffer) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemTextA); REQUIRE(GetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[8]; WCHAR w[8];
    memset(a, 'X', sizeof a);
    SET_ERR(0);
    EQ(GetDlgItemTextA(dlg, 999, a, 8), 0);
    EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
    CHECK(a[0] == 0);
    for (int i = 0; i < 8; i++) w[i] = 'X';
    EQ(GetDlgItemTextW(dlg, 999, w, 8), 0);
    CHECK(w[0] == 0);
    memset(a, 'X', sizeof a);
    EQ(GetDlgItemTextA(dlg, 999, a, 0), 0);
    CHECK(a[0] == 'X');                        /* cch 0: not written */
    DestroyWindow(dlg);
}
TEST(dialogs_dlgitemtext_null_dialog) {
    REQUIRE(GetDlgItemTextA); REQUIRE(GetDlgItemTextW); REQUIRE(SetDlgItemTextA); REQUIRE(SetDlgItemTextW);
    char a[8]; WCHAR w[8]; memset(a, 'X', 8); w[0] = 'X';
    SET_ERR(0);
    EQ(GetDlgItemTextA(NULL, 100, a, 8), 0);
    EXPECT_ERR(0, ERR_INVALID_HWND);
    CHECK(a[0] == 0);
    EQ(GetDlgItemTextW(NULL, 100, w, 8), 0);
    CHECK(w[0] == 0);
    SET_ERR(0);
    CHECK(!SetDlgItemTextA(NULL, 100, "x"));
    EXPECT_ERR(0, ERR_INVALID_HWND);
    CHECK(!SetDlgItemTextW(NULL, 100, L"x"));
}
TEST(dialogs_setdlgitemtext_roundtrip) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemTextA); REQUIRE(SetDlgItemTextW); REQUIRE(GetDlgItemTextA); REQUIRE(GetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[64]; WCHAR w[64];
    CHECK(SetDlgItemTextA(dlg, ID_EDIT, "Hello ANSI"));
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 64), 10); EXPECT_STR(a, "Hello ANSI");
    EQ(GetDlgItemTextW(dlg, ID_EDIT, w, 64), 10); EXPECT_WSTR(w, L"Hello ANSI");
    CHECK(SetDlgItemTextW(dlg, ID_EDIT, L"Hello Wide"));
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 64), 10); EXPECT_STR(a, "Hello Wide");
    CHECK(SetDlgItemTextW(dlg, ID_EDIT, L""));
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 64), 0); EXPECT_STR(a, "");
    CHECK(SetDlgItemTextA(dlg, ID_STATIC, "s"));
    EQ(GetDlgItemTextW(dlg, ID_STATIC, w, 64), 1);
    CHECK(SetDlgItemTextA(dlg, ID_BTN, "OK&Go"));
    EQ(GetDlgItemTextA(dlg, ID_BTN, a, 64), 5); EXPECT_STR(a, "OK&Go");   /* mnemonic text preserved verbatim */
    DestroyWindow(dlg);
}
TEST(dialogs_setdlgitemtext_unicode_text) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemTextW); REQUIRE(GetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    static const WCHAR s[] = { 0x0416, 0x4E2D, 0x20AC, 0xD83D, 0xDE00, 'z', 0 };
    WCHAR w[16];
    CHECK(SetDlgItemTextW(dlg, ID_EDIT, s));
    EQ(GetDlgItemTextW(dlg, ID_EDIT, w, 16), 6);
    EXPECT_WSTR(w, s);
    EQ(GetDlgItemTextW(dlg, ID_EDIT, w, 4), 3);    /* truncation mid-surrogate allowed; count only */
    DestroyWindow(dlg);
}
TEST(dialogs_setdlgitemtext_missing_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemTextA); REQUIRE(SetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SET_ERR(0);
    CHECK(!SetDlgItemTextA(dlg, 999, "x"));
    EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
    SET_ERR(0);
    CHECK(!SetDlgItemTextW(dlg, 0, L"x"));
    EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
    DestroyWindow(dlg);
}
TEST(dialogs_setdlgitemtext_null_string) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemTextA); REQUIRE(SetDlgItemTextW); REQUIRE(GetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[16] = "zz";
    BOOL r = SetDlgItemTextA(dlg, ID_EDIT, NULL);
    t_note("SetDlgItemTextA(NULL string) -> %d", r);
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 16), 0);
    CHECK(a[0] == 0);
    SetDlgItemTextA(dlg, ID_EDIT, "back");
    r = SetDlgItemTextW(dlg, ID_EDIT, NULL);
    EQ(GetDlgItemTextA(dlg, ID_EDIT, a, 16), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_setdlgitemtext_long_text) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemTextA); REQUIRE(GetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    static char big[20000]; static char out[20000];
    memset(big, 'a', 19000); big[19000] = 0;
    SetDlgItemTextA(dlg, ID_STATIC, big);    /* static has no length limit */
    EQ(GetDlgItemTextA(dlg, ID_STATIC, out, sizeof out), 19000);
    EQ(GetDlgItemTextA(dlg, ID_STATIC, out, 100), 99);
    EQ(strlen(out), 99);
    DestroyWindow(dlg);
}
TEST(dialogs_dlgitemtext_repeated) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemTextA); REQUIRE(GetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[32], b[32]; int bad = 0;
    for (int i = 0; i < 500; i++) {
        sprintf(b, "iter%d", i);
        if (!SetDlgItemTextA(dlg, ID_EDIT, b)) bad++;
        if (GetDlgItemTextA(dlg, ID_EDIT, a, 32) != (UINT)strlen(b) || strcmp(a, b)) bad++;
    }
    EQ(bad, 0);
    DestroyWindow(dlg);
}

/* ---------------- GetDlgItemInt / SetDlgItemInt ---------------- */
struct inttc { const WCHAR *txt; BOOL sgn; UINT val; BOOL tr; };
static const struct inttc inttc[] = {
    { L"0", 0, 0, 1 }, { L"0", 1, 0, 1 }, { L"1", 0, 1, 1 }, { L"123", 0, 123, 1 }, { L"123", 1, 123, 1 },
    { L"007", 0, 7, 1 }, { L"  42", 0, 42, 1 }, { L"      42", 1, 42, 1 },
    { L"42 ", 0, 42, 0 }, { L"42x", 0, 42, 0 }, { L"4 2", 0, 4, 0 },
    { L"+5", 0, 0, 0 }, { L"+5", 1, 0, 0 },
    { L"-5", 0, 0, 0 }, { L"-5", 1, (UINT)-5, 1 }, { L"--5", 1, 5, 1 }, { L"---5", 1, (UINT)-5, 1 },
    { L"- 5", 1, 0, 0 }, { L"-", 1, 0, 0 }, { L"-", 0, 0, 0 },
    { L"-0", 1, 0, 1 },
    { L"", 0, 0, 0 }, { L"", 1, 0, 0 }, { L"   ", 0, 0, 0 },
    { L"abc", 0, 0, 0 }, { L"a1", 0, 0, 0 }, { L"0x10", 0, 0, 0 }, { L"1e3", 0, 1, 0 },
    { L"4294967295", 0, 0xFFFFFFFFu, 1 },
    { L"4294967296", 0, 0, 0 }, { L"4294967299", 0, 0, 0 }, { L"4294967300", 0, 0, 0 },
    { L"9999999999", 0, 0, 0 }, { L"99999999999999999999", 0, 0, 0 },
    { L"2147483647", 1, 2147483647u, 1 }, { L"2147483648", 1, 0, 0 },
    { L"-2147483647", 1, 0x80000001u, 1 }, { L"-2147483648", 1, 0x80000000u, 2 }, { L"-2147483649", 1, 0, 0 },
    { L"-4294967295", 1, 0, 0 },
    { L"2147483648", 0, 0x80000000u, 1 }, { L"3000000000", 0, 3000000000u, 1 },
    { L"4294967290", 0, 4294967290u, 1 },
    { L"0000000000000000000000000000000000000000005", 0, 5, 1 },
    { L"\x0661\x0662\x0663", 0, 123, 1 },                  /* Arabic-Indic digits fold to ASCII */
    { L"\xFF11\xFF12", 0, 12, 1 },                         /* fullwidth digits */
    { L"\x2212" L"7", 1, (UINT)-7, 1 },                    /* U+2212 minus */
    { L"\x2212" L"7", 0, 0, 0 },
    { L"-\x0661", 1, (UINT)-1, 1 },
};
TEST(dialogs_getdlgitemint_table) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemInt); REQUIRE(SetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    for (unsigned i = 0; i < sizeof inttc / sizeof *inttc; i++) {
        BOOL tr = 0x55;
        SetDlgItemTextW(dlg, ID_EDIT, inttc[i].txt);
        UINT v = GetDlgItemInt(dlg, ID_EDIT, &tr, inttc[i].sgn);
        if (inttc[i].tr == 2) {   /* INT_MIN special path: translated flag not verified, value only */
            CHECKF(v == inttc[i].val, "case %u: got val=0x%x want 0x%x", i, v, inttc[i].val);
            t_note("case %u: translated=%d", i, tr);
            continue;
        }
        CHECKF(v == inttc[i].val && tr == inttc[i].tr, "case %u: got val=0x%x tr=%d want val=0x%x tr=%d", i, v, tr, inttc[i].val, inttc[i].tr);
    }
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitemint_null_translated) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemInt); REQUIRE(SetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SetDlgItemTextW(dlg, ID_EDIT, L"77");
    EQ(GetDlgItemInt(dlg, ID_EDIT, NULL, 0), 77);
    EQ(GetDlgItemInt(dlg, ID_EDIT, NULL, 1), 77);
    SetDlgItemTextW(dlg, ID_EDIT, L"zz");
    EQ(GetDlgItemInt(dlg, ID_EDIT, NULL, 1), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitemint_missing_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemInt); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    BOOL tr = 1;
    SET_ERR(0);
    EQ(GetDlgItemInt(dlg, 999, &tr, 0), 0);
    EQ(tr, 0);                                 /* translated is always reset */
    EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
    tr = 1;
    SET_ERR(0);
    EQ(GetDlgItemInt(NULL, 100, &tr, 1), 0);
    EQ(tr, 0);
    EXPECT_ERR(0, ERR_INVALID_HWND);
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitemint_signed_flag_nonzero_values) {
    /* any non-zero bSigned behaves as TRUE */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemInt); REQUIRE(SetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SetDlgItemTextW(dlg, ID_EDIT, L"-9");
    BOOL tr;
    static const BOOL flags[] = { 1, 2, 0x100, -1, 0x7fffffff };
    for (unsigned i = 0; i < 5; i++) {
        tr = 0;
        UINT v = GetDlgItemInt(dlg, ID_EDIT, &tr, flags[i]);
        CHECKF(v == (UINT)-9 && tr == 1, "flag %d", flags[i]);
    }
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitemint_46_char_truncation) {
    /* the text is read into a 47-character buffer, so a long number is cut to 46 chars */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemInt); REQUIRE(SetDlgItemTextW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    WCHAR b[80]; int i; BOOL tr;
    for (i = 0; i < 45; i++) b[i] = '0';
    b[45] = '5'; b[46] = 0;
    SetDlgItemTextW(dlg, ID_EDIT, b);
    tr = 0; EQ(GetDlgItemInt(dlg, ID_EDIT, &tr, 0), 5); EQ(tr, 1);
    for (i = 0; i < 49; i++) b[i] = '0';
    b[49] = '7'; b[50] = 0;
    SetDlgItemTextW(dlg, ID_EDIT, b);
    tr = 0; EQ(GetDlgItemInt(dlg, ID_EDIT, &tr, 0), 0); EQ(tr, 1);   /* trailing 7 cut off */
    DestroyWindow(dlg);
}
TEST(dialogs_getdlgitemint_on_static_and_button) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(GetDlgItemInt); REQUIRE(SetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    BOOL tr = 0;
    SetDlgItemTextA(dlg, ID_STATIC, "314");
    EQ(GetDlgItemInt(dlg, ID_STATIC, &tr, 0), 314); EQ(tr, 1);
    SetDlgItemTextA(dlg, ID_BTN, "-2");
    EQ(GetDlgItemInt(dlg, ID_BTN, &tr, 1), (UINT)-2); EQ(tr, 1);
    tr = 1;
    EQ(GetDlgItemInt(dlg, ID_LIST, &tr, 1), 0); EQ(tr, 0);
    DestroyWindow(dlg);
}
struct setint { UINT v; BOOL sgn; const char *want; };
static const struct setint setints[] = {
    { 0, 0, "0" }, { 0, 1, "0" }, { 1, 0, "1" }, { 9, 1, "9" }, { 10, 0, "10" }, { 99, 0, "99" }, { 100, 1, "100" },
    { 12345, 0, "12345" }, { 2147483647u, 0, "2147483647" }, { 2147483647u, 1, "2147483647" },
    { 0x80000000u, 0, "2147483648" }, { 0x80000000u, 1, "-2147483648" },
    { 0xFFFFFFFFu, 0, "4294967295" }, { 0xFFFFFFFFu, 1, "-1" },
    { (UINT)-42, 1, "-42" }, { (UINT)-42, 0, "4294967254" }, { 4000000000u, 0, "4000000000" },
    { 1000000000u, 1, "1000000000" }, { 0x80000001u, 1, "-2147483647" }, { 0x80000001u, 0, "2147483649" },
    { 5, 0x100, "5" }, { (UINT)-5, 0x100, "-5" },
};
TEST(dialogs_setdlgitemint_table) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemInt); REQUIRE(GetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    for (unsigned i = 0; i < sizeof setints / sizeof *setints; i++) {
        char a[40] = "";
        BOOL r = SetDlgItemInt(dlg, ID_EDIT, setints[i].v, setints[i].sgn);
        GetDlgItemTextA(dlg, ID_EDIT, a, 40);
        CHECKF(r && !strcmp(a, setints[i].want), "case %u: got '%s' want '%s' r=%d", i, a, setints[i].want, r);
    }
    DestroyWindow(dlg);
}
TEST(dialogs_setdlgitemint_roundtrip) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemInt); REQUIRE(GetDlgItemInt); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    static const UINT vs[] = { 0, 1, 2, 9, 10, 255, 256, 65535, 65536, 0x7fffffff, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF };
    int bad = 0;
    for (unsigned i = 0; i < sizeof vs / sizeof *vs; i++) for (int sg = 0; sg < 2; sg++) {
        BOOL tr = 0;
        SetDlgItemInt(dlg, ID_EDIT, vs[i], sg);
        UINT v = GetDlgItemInt(dlg, ID_EDIT, &tr, sg);
        if (v != vs[i] || !tr) bad++;
    }
    EQ(bad, 0);
    DestroyWindow(dlg);
}
TEST(dialogs_setdlgitemint_missing_control) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemInt); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SET_ERR(0);
    CHECK(!SetDlgItemInt(dlg, 999, 5, 0));
    EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
    SET_ERR(0);
    CHECK(!SetDlgItemInt(NULL, 100, 5, 0));
    EXPECT_ERR(0, ERR_INVALID_HWND);
    DestroyWindow(dlg);
}
TEST(dialogs_setdlgitemint_on_static) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(SetDlgItemInt); REQUIRE(GetDlgItemTextA); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    char a[32];
    CHECK(SetDlgItemInt(dlg, ID_STATIC, 17, 0));
    GetDlgItemTextA(dlg, ID_STATIC, a, 32); EXPECT_STR(a, "17");
    CHECK(SetDlgItemInt(dlg, ID_BTN, (UINT)-17, 1));
    GetDlgItemTextA(dlg, ID_BTN, a, 32); EXPECT_STR(a, "-17");
    DestroyWindow(dlg);
}

/* ---------------- CheckDlgButton / IsDlgButtonChecked / CheckRadioButton ---------------- */
TEST(dialogs_checkdlgbutton_basic) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    EQ(IsDlgButtonChecked(dlg, ID_CHK), BST_UNCHECKED);
    CHECK(CheckDlgButton(dlg, ID_CHK, BST_CHECKED));
    EQ(IsDlgButtonChecked(dlg, ID_CHK), BST_CHECKED);
    CHECK(CheckDlgButton(dlg, ID_CHK, BST_UNCHECKED));
    EQ(IsDlgButtonChecked(dlg, ID_CHK), BST_UNCHECKED);
    CHECK(CheckDlgButton(dlg, ID_CHK, BST_CHECKED));
    CHECK(CheckDlgButton(dlg, ID_CHK, BST_CHECKED));   /* idempotent */
    EQ(IsDlgButtonChecked(dlg, ID_CHK), BST_CHECKED);
    DestroyWindow(dlg);
}
TEST(dialogs_checkdlgbutton_3state) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CHECK(CheckDlgButton(dlg, ID_3ST, BST_INDETERMINATE));
    EQ(IsDlgButtonChecked(dlg, ID_3ST), BST_INDETERMINATE);
    CHECK(CheckDlgButton(dlg, ID_3ST, BST_CHECKED));
    EQ(IsDlgButtonChecked(dlg, ID_3ST), BST_CHECKED);
    CHECK(CheckDlgButton(dlg, ID_3ST, BST_UNCHECKED));
    EQ(IsDlgButtonChecked(dlg, ID_3ST), BST_UNCHECKED);
    DestroyWindow(dlg);
}
TEST(dialogs_checkdlgbutton_large_value_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CheckDlgButton(dlg, ID_3ST, 3);
    t_note("3state after CheckDlgButton(3) -> %u", IsDlgButtonChecked(dlg, ID_3ST));
    CheckDlgButton(dlg, ID_CHK, BST_INDETERMINATE);
    t_note("plain checkbox after CheckDlgButton(INDETERMINATE) -> %u", IsDlgButtonChecked(dlg, ID_CHK));
    CheckDlgButton(dlg, ID_CHK, 0xFFFFFFFF);
    t_note("plain checkbox after CheckDlgButton(-1) -> %u", IsDlgButtonChecked(dlg, ID_CHK));
    DestroyWindow(dlg);
}
TEST(dialogs_checkdlgbutton_missing) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SET_ERR(0);
    CHECK(!CheckDlgButton(dlg, 999, 1));
    EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
    SET_ERR(0);
    EQ(IsDlgButtonChecked(dlg, 999), 0);
    EXPECT_ERR(0, ERR_CTRL_NOT_FOUND);
    SET_ERR(0);
    CHECK(!CheckDlgButton(NULL, ID_CHK, 1));
    EXPECT_ERR(0, ERR_INVALID_HWND);
    EQ(IsDlgButtonChecked(NULL, ID_CHK), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_checkdlgbutton_on_non_button) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CHECK(CheckDlgButton(dlg, ID_EDIT, 1));            /* found -> TRUE even if not a button */
    EQ(IsDlgButtonChecked(dlg, ID_EDIT), 0);
    CHECK(CheckDlgButton(dlg, ID_STATIC, 1));
    EQ(IsDlgButtonChecked(dlg, ID_STATIC), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_checkdlgbutton_pushbutton) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CHECK(CheckDlgButton(dlg, ID_BTN, 1));
    t_note("push button check state after set: %u", IsDlgButtonChecked(dlg, ID_BTN));
    DestroyWindow(dlg);
}
TEST(dialogs_checkdlgbutton_radio_no_group_effect) {
    /* CheckDlgButton only touches the one button */
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CheckDlgButton(dlg, ID_R1, 1);
    CheckDlgButton(dlg, ID_R2, 1);
    EQ(IsDlgButtonChecked(dlg, ID_R1), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_checkradiobutton_basic) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckRadioButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CHECK(CheckRadioButton(dlg, ID_R1, ID_R3, ID_R2));
    EQ(IsDlgButtonChecked(dlg, ID_R1), 0);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 0);
    CHECK(CheckRadioButton(dlg, ID_R1, ID_R3, ID_R3));
    EQ(IsDlgButtonChecked(dlg, ID_R1), 0);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 0);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 1);
    CHECK(CheckRadioButton(dlg, ID_R1, ID_R3, ID_R1));
    EQ(IsDlgButtonChecked(dlg, ID_R1), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_checkradiobutton_clears_multiple) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckRadioButton); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CheckDlgButton(dlg, ID_R1, 1); CheckDlgButton(dlg, ID_R2, 1); CheckDlgButton(dlg, ID_R3, 1);
    CHECK(CheckRadioButton(dlg, ID_R1, ID_R3, ID_R2));
    EQ(IsDlgButtonChecked(dlg, ID_R1), 0);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_checkradiobutton_range_limits) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckRadioButton); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CheckDlgButton(dlg, ID_R3, 1); CheckDlgButton(dlg, ID_CHK, 1);
    /* range covers only R1..R2: R3 and CHK untouched */
    CHECK(CheckRadioButton(dlg, ID_R1, ID_R2, ID_R1));
    EQ(IsDlgButtonChecked(dlg, ID_R1), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 0);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 1);
    EQ(IsDlgButtonChecked(dlg, ID_CHK), 1);
    /* single-element range */
    CHECK(CheckRadioButton(dlg, ID_R2, ID_R2, ID_R2));
    EQ(IsDlgButtonChecked(dlg, ID_R2), 1);
    CHECK(CheckRadioButton(dlg, ID_R2, ID_R2, 0));
    EQ(IsDlgButtonChecked(dlg, ID_R2), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_checkradiobutton_check_outside_range) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckRadioButton); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CheckDlgButton(dlg, ID_R2, 1);
    CHECK(CheckRadioButton(dlg, ID_R1, ID_R3, 9999));     /* nonexistent check id: all cleared, still TRUE */
    EQ(IsDlgButtonChecked(dlg, ID_R1), 0);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 0);
    EQ(IsDlgButtonChecked(dlg, ID_R3), 0);
    CHECK(CheckRadioButton(dlg, ID_R1, ID_R2, ID_R3));    /* id of a control outside the range: only clears inside */
    EQ(IsDlgButtonChecked(dlg, ID_R3), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_checkradiobutton_reversed_range) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckRadioButton); REQUIRE(CheckDlgButton); REQUIRE(IsDlgButtonChecked); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CheckDlgButton(dlg, ID_R1, 1);
    CHECK(CheckRadioButton(dlg, ID_R3, ID_R1, ID_R2));    /* first>last: nothing matches */
    EQ(IsDlgButtonChecked(dlg, ID_R1), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_checkradiobutton_bad_dialog) {
    REQUIRE(CheckRadioButton);
    SET_ERR(0);
    CHECK(!CheckRadioButton(NULL, 100, 105, 101));
    EXPECT_ERR(0, ERR_INVALID_HWND);
    CHECK(!CheckRadioButton((HWND)0x1234abcd, 100, 105, 101));
}
TEST(dialogs_checkradiobutton_wide_range_includes_nonbuttons) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(CheckRadioButton); REQUIRE(IsDlgButtonChecked); REQUIRE(SendDlgItemMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    CHECK(CheckRadioButton(dlg, 0, 0x7fffffff, ID_R2));
    EQ(IsDlgButtonChecked(dlg, ID_R2), 1);
    EQ(IsDlgButtonChecked(dlg, ID_CHK), 0);
    EQ(IsDlgButtonChecked(dlg, ID_3ST), 0);
    CHECK(CheckRadioButton(dlg, (int)0x80000000, 0x7fffffff, ID_R3));
    EQ(IsDlgButtonChecked(dlg, ID_R3), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 0);
    DestroyWindow(dlg);
}
TEST(dialogs_autoradio_click_unchecks_siblings) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(IsDlgButtonChecked); REQUIRE(SendDlgItemMessageW); REQUIRE(DestroyWindow);
    HWND dlg = mk_std(); REQUIRE(dlg);
    SendDlgItemMessageW(dlg, ID_R1, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_R1), 1);
    SendDlgItemMessageW(dlg, ID_R2, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_R1), 0);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 1);
    SendDlgItemMessageW(dlg, ID_CHK, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_CHK), 1);
    EQ(IsDlgButtonChecked(dlg, ID_R2), 1);   /* checkbox is in another group */
    SendDlgItemMessageW(dlg, ID_CHK, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_CHK), 0);
    SendDlgItemMessageW(dlg, ID_3ST, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_3ST), 1);
    SendDlgItemMessageW(dlg, ID_3ST, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_3ST), 2);
    SendDlgItemMessageW(dlg, ID_3ST, BM_CLICK, 0, 0);
    EQ(IsDlgButtonChecked(dlg, ID_3ST), 0);
    DestroyWindow(dlg);
}
