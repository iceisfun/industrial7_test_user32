/* gap_00: CreateDialogIndirectParamAorW / DialogBoxIndirectParamAorW */
#include "gap0.h"

UDECL(HWND, CreateDialogIndirectParamAorW, (HINSTANCE, LPCDLGTEMPLATEW, HWND, DLGPROC, LPARAM, UINT));
UDECL(INT_PTR, DialogBoxIndirectParamAorW, (HINSTANCE, LPCDLGTEMPLATEW, HWND, DLGPROC, LPARAM, UINT));

static INT_PTR g_endval;
static int g_init_count, g_unicode, g_destroy_count;
static LPARAM g_init_lp;
static HWND g_init_hwnd;
static BOOL g_parent_enabled_in;
static HWND g_parent_probe;

static INT_PTR CALLBACK dp_end(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) {
        g_init_count++; g_init_lp = l; g_init_hwnd = h;
        g_unicode = U(IsWindowUnicode)(h);
        if (g_parent_probe) g_parent_enabled_in = U(IsWindowEnabled)(g_parent_probe);
        U(EndDialog)(h, g_endval);
        return TRUE;
    }
    if (m == WM_DESTROY) g_destroy_count++;
    return FALSE;
}
static INT_PTR CALLBACK dp_keep(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) { g_init_count++; g_init_lp = l; g_init_hwnd = h; return TRUE; }
    if (m == WM_DESTROY) g_destroy_count++;
    return FALSE;
}
static void reset(void) { g_endval = 0; g_init_count = 0; g_unicode = -1; g_init_lp = 0; g_init_hwnd = NULL; g_destroy_count = 0; g_parent_probe = NULL; g_parent_enabled_in = 2; }

#define DLGSTYLE (WS_POPUP | WS_CAPTION | WS_SYSMENU)
static DWORD buf[512];
static void *tmpl(DWORD style, const WCHAR *title, int nb) { memset(buf, 0, sizeof buf); g0_build_dlg(buf, style, 0, title, nb); return buf; }
#define H() GetModuleHandleA(NULL)

/* ---------- modeless ---------- */
TEST(gap0_createdlg_w_basic) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    reset();
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"Gap0 Dialog", 0), NULL, dp_keep, 0x1234, 0);
    CHECK(d != NULL);
    if (!d) return;
    EXPECT_EQ(g_init_count, 1);
    EXPECT_EQ(g_init_lp, 0x1234);
    EXPECT_EQ(g_init_hwnd, d);
    WCHAR b[64]; U(GetWindowTextW)(d, b, 64);
    EXPECT_WSTR(b, L"Gap0 Dialog");
    CHECK(U(IsWindowUnicode)(d));
    CHECK(!U(IsWindowVisible)(d));
    WCHAR cls[32]; U(GetClassNameW)(d, cls, 32);
    EXPECT_WSTR(cls, L"#32770");
    CHECK(U(GetWindowLongPtrW)(d, DWLP_DLGPROC) == (LONG_PTR)dp_keep);
    CHECK(U(DestroyWindow)(d));
    EXPECT_EQ(g_destroy_count, 1);
    CHECK(!U(IsWindow)(d));
}
TEST(gap0_createdlg_a_flag_is_ansi) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    reset();
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"Ansi", 0), NULL, dp_keep, 0, 2);
    REQUIRE(d);
    CHECK(!U(IsWindowUnicode)(d));
    char b[16]; U(GetWindowTextA)(d, b, 16);
    EXPECT_STR(b, "Ansi");
    U(DestroyWindow)(d);
}
TEST(gap0_createdlg_flag_mask) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    /* only bit 1 of the flags selects ANSI */
    static const struct { UINT f; int ansi; } t[] = {
        { 0, 0 }, { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 0 }, { 0xFFFFFFFDu, 0 }, { 0xFFFFFFFEu, 1 }, { 0x80000000u, 0 }, { 0x80000002u, 1 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        reset();
        HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"m", 0), NULL, dp_keep, 0, t[i].f);
        CHECKF(d != NULL, "flags %x", t[i].f);
        if (!d) continue;
        CHECKF(!!U(IsWindowUnicode)(d) == !t[i].ansi, "flags %x unicode=%d", t[i].f, U(IsWindowUnicode)(d));
        U(DestroyWindow)(d);
    }
}
TEST(gap0_createdlg_matches_public_wrappers) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(CreateDialogIndirectParamA)); REQUIRE(U(CreateDialogIndirectParamW)); REQUIRE(U(DestroyWindow));
    reset();
    HWND a = U(CreateDialogIndirectParamA)(H(), tmpl(DLGSTYLE, L"p", 0), NULL, dp_keep, 5);
    HWND w = U(CreateDialogIndirectParamW)(H(), tmpl(DLGSTYLE, L"p", 0), NULL, dp_keep, 5);
    REQUIRE(a); REQUIRE(w);
    EXPECT_EQ(U(IsWindowUnicode)(a), FALSE);
    EXPECT_EQ(U(IsWindowUnicode)(w), TRUE);
    U(DestroyWindow)(a); U(DestroyWindow)(w);
}
TEST(gap0_createdlg_controls_created) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    reset();
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"ctl", 3), NULL, dp_keep, 0, 0);
    REQUIRE(d);
    for (int i = 0; i < 3; i++) {
        HWND c = U(GetDlgItem)(d, IDOK + i);
        CHECKF(c != NULL, "control %d", i);
        if (!c) continue;
        WCHAR cl[16]; U(GetClassNameW)(c, cl, 16);
        CHECK(!lstrcmpiW(cl, L"button"));
        EXPECT_EQ(U(GetParent)(c), d);
    }
    CHECK(U(GetDlgItem)(d, 999) == NULL);
    U(DestroyWindow)(d);
}
TEST(gap0_createdlg_null_dlgproc) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"np", 0), NULL, NULL, 0, 0);
    CHECK(d != NULL);
    if (d) {
        CHECK(U(GetWindowLongPtrW)(d, DWLP_DLGPROC) == 0);
        U(DestroyWindow)(d);
    }
}
TEST(gap0_createdlg_bogus_parent) {
    REQUIRE(UN(CreateDialogIndirectParamAorW));
    reset();
    SET_ERR(0);
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(WS_CHILD | WS_VISIBLE, L"x", 0), (HWND)(ULONG_PTR)0xDEAD0, dp_keep, 0, 0);
    CHECK(d == NULL);
    EXPECT_ERR(d, ERROR_INVALID_WINDOW_HANDLE);
    EXPECT_EQ(g_init_count, 0);
}
TEST(gap0_createdlg_child_dialog_with_parent) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    reset();
    HWND p = g0_mk(WS_POPUP, 0, 0, 0, 200, 150, NULL);
    REQUIRE(p);
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(WS_CHILD, L"kid", 0), p, dp_keep, 0, 0);
    REQUIRE(d);
    EXPECT_EQ(U(GetParent)(d), p);
    U(DestroyWindow)(p);
    CHECK(!U(IsWindow)(d));
}
TEST(gap0_createdlg_owned_popup_dialog) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    reset();
    HWND p = g0_mk(WS_POPUP, 0, 0, 0, 200, 150, NULL);
    REQUIRE(p);
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"own", 0), p, dp_keep, 0, 0);
    REQUIRE(d);
    EXPECT_EQ(U(GetWindow)(d, GW_OWNER), p);
    U(DestroyWindow)(p);
    CHECK(!U(IsWindow)(d));
}
TEST(gap0_createdlg_long_title) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    WCHAR t[300]; for (int i = 0; i < 299; i++) t[i] = L'a' + (i % 26); t[299] = 0;
    reset();
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, t, 0), NULL, dp_keep, 0, 0);
    REQUIRE(d);
    EXPECT_EQ(U(GetWindowTextLengthW)(d), 299);
    WCHAR b[300]; U(GetWindowTextW)(d, b, 300);
    EXPECT_WSTR(b, t);
    U(DestroyWindow)(d);
}
TEST(gap0_createdlg_unicode_title) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    static const WCHAR t[] = { 0x4E2D, 0x6587, 0x00E9, 'Z', 0 };
    reset();
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, t, 0), NULL, dp_keep, 0, 0);
    REQUIRE(d);
    WCHAR b[16]; U(GetWindowTextW)(d, b, 16);
    EXPECT_WSTR(b, t);
    U(DestroyWindow)(d);
}
TEST(gap0_createdlg_initparam_full_width) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    reset();
    LPARAM lp = (LPARAM)0x123456789ABCDEF0LL;
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"w", 0), NULL, dp_keep, lp, 0);
    REQUIRE(d);
    EXPECT_EQ(g_init_lp, lp);
    U(DestroyWindow)(d);
}
TEST(gap0_createdlg_null_hinstance) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    reset();
    HWND d = UN(CreateDialogIndirectParamAorW)(NULL, tmpl(DLGSTYLE, L"ni", 0), NULL, dp_keep, 0, 0);
    CHECK(d != NULL);
    if (d) U(DestroyWindow)(d);
}
TEST(gap0_createdlg_style_bit8000_observed) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    reset();
    SET_ERR(0);
    HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE | 0x8000, L"b", 0), NULL, dp_keep, 0, 0);
    t_note("dialog with reserved style 0x8000: %p err %lu", (void *)d, d ? 0 : GetLastError());
    if (d) U(DestroyWindow)(d);
    CHECK(1);
}
TEST(gap0_createdlg_repeat) {
    REQUIRE(UN(CreateDialogIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    int bad = 0;
    for (int i = 0; i < 200; i++) {
        reset();
        HWND d = UN(CreateDialogIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"r", 1), NULL, dp_keep, 0, i & 2);
        if (!d || !U(DestroyWindow)(d)) bad++;
    }
    EXPECT_EQ(bad, 0);
}

/* ---------- modal (every dialog ends itself in WM_INITDIALOG) ---------- */
TEST(gap0_dialogbox_returns_endvalue) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(EndDialog));
    reset(); g_endval = 4242;
    INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"m", 0), NULL, dp_end, 0x77, 0);
    EXPECT_EQ(r, 4242);
    EXPECT_EQ(g_init_count, 1);
    EXPECT_EQ(g_init_lp, 0x77);
    EXPECT_EQ(g_unicode, TRUE);
    EXPECT_EQ(g_destroy_count, 1);
    CHECK(!U(IsWindow)(g_init_hwnd));
}
TEST(gap0_dialogbox_ansi_flag) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(EndDialog));
    reset(); g_endval = 9;
    INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"m", 0), NULL, dp_end, 0, 2);
    EXPECT_EQ(r, 9);
    EXPECT_EQ(g_unicode, FALSE);
}
TEST(gap0_dialogbox_flag_mask) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(EndDialog));
    static const struct { UINT f; int ansi; } t[] = { {0,0},{1,0},{2,1},{3,1},{0xFFFFFFFDu,0},{0xFFFFFFFFu,1} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        reset(); g_endval = 1;
        INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"m", 0), NULL, dp_end, 0, t[i].f);
        CHECKF(r == 1, "flags %x ret %lld", t[i].f, (long long)r);
        CHECKF(g_unicode == !t[i].ansi, "flags %x unicode %d", t[i].f, g_unicode);
    }
}
TEST(gap0_dialogbox_endvalues) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(EndDialog));
    static const INT_PTR v[] = { 0, 1, -1, -2, 0x7FFFFFFF, (INT_PTR)0x80000000LL, (INT_PTR)0x123456789ALL, (INT_PTR)0x7FFFFFFFFFFFFFFFLL, (INT_PTR)0x8000000000000000ULL };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        reset(); g_endval = v[i];
        INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"v", 0), NULL, dp_end, 0, 0);
        CHECKF(r == v[i], "want %lld got %lld", (long long)v[i], (long long)r);
    }
}
TEST(gap0_dialogbox_matches_public_wrappers) {
    REQUIRE(U(DialogBoxIndirectParamA)); REQUIRE(U(DialogBoxIndirectParamW)); REQUIRE(U(EndDialog));
    reset(); g_endval = 31;
    INT_PTR a = U(DialogBoxIndirectParamA)(H(), tmpl(DLGSTYLE, L"p", 0), NULL, dp_end, 0);
    int ua = g_unicode;
    INT_PTR w = U(DialogBoxIndirectParamW)(H(), tmpl(DLGSTYLE, L"p", 0), NULL, dp_end, 0);
    int uw = g_unicode;
    EXPECT_EQ(a, 31); EXPECT_EQ(w, 31);
    EXPECT_EQ(ua, FALSE); EXPECT_EQ(uw, TRUE);
}
TEST(gap0_dialogbox_invalid_parent_returns_zero) {
    REQUIRE(UN(DialogBoxIndirectParamAorW));
    reset(); g_endval = 5;
    SET_ERR(0);
    INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"i", 0), (HWND)(ULONG_PTR)0xDEAD0, dp_end, 0, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(g_init_count, 0);
    t_note("DialogBoxIndirectParam invalid parent err %lu", GetLastError());
}
TEST(gap0_dialogbox_destroyed_parent_returns_zero) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    HWND p = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    REQUIRE(p);
    U(DestroyWindow)(p);
    reset(); g_endval = 5;
    INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"i", 0), p, dp_end, 0, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(g_init_count, 0);
}
TEST(gap0_dialogbox_disables_and_restores_owner) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    HWND p = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    REQUIRE(p);
    reset(); g_endval = 3; g_parent_probe = p;
    INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"o", 0), p, dp_end, 0, 0);
    EXPECT_EQ(r, 3);
    t_note("owner enabled inside WM_INITDIALOG: %d", g_parent_enabled_in);
    CHECK(U(IsWindowEnabled)(p));
    CHECK(U(IsWindow)(p));
    U(DestroyWindow)(p);
}
TEST(gap0_dialogbox_child_parent_uses_toplevel_owner) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(DestroyWindow));
    HWND p = g0_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    REQUIRE(p);
    HWND c = g0_mk(WS_CHILD, 0, 0, 0, 20, 20, p);
    REQUIRE(c);
    reset(); g_endval = 8;
    INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"c", 0), c, dp_end, 0, 0);
    EXPECT_EQ(r, 8);
    CHECK(U(IsWindowEnabled)(p));
    CHECK(U(IsWindowEnabled)(c));
    U(DestroyWindow)(p);
}
TEST(gap0_dialogbox_with_controls) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(EndDialog));
    reset(); g_endval = 77;
    INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"ctl", 4), NULL, dp_end, 0, 0);
    EXPECT_EQ(r, 77);
    EXPECT_EQ(g_init_count, 1);
}
TEST(gap0_dialogbox_repeat) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(EndDialog));
    int bad = 0;
    for (int i = 0; i < 100; i++) {
        reset(); g_endval = i;
        if (UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"r", 1), NULL, dp_end, 0, i & 2) != i) bad++;
        if (g_destroy_count != 1) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap0_dialogbox_endialog_twice_ignored) {
    REQUIRE(UN(DialogBoxIndirectParamAorW)); REQUIRE(U(EndDialog));
    reset(); g_endval = 12;
    INT_PTR r = UN(DialogBoxIndirectParamAorW)(H(), tmpl(DLGSTYLE, L"t", 0), NULL, dp_end, 0, 0);
    EXPECT_EQ(r, 12);
    SET_ERR(0);
    BOOL ok = U(EndDialog)(g_init_hwnd, 5);     /* dialog is gone */
    EXPECT_EQ(ok, FALSE);
}
