/* misc_ui: extra coverage: GetListBoxInfo, high contrast query, topmost normalisation, DDEML invalid-handle paths */
#include "mu.h"
#include <ddeml.h>

#define NEED_X() REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow)); REQUIRE(U(IsWindow)); if (!mu_register()) { t_skip("class"); return; }

/* ---------- GetListBoxInfo ---------- */
UDECL(DWORD, GetListBoxInfo, (HWND));

TEST(misc_ui_listboxinfo_matches_message) {
    NEED_X(); REQUIRE(UN(GetListBoxInfo)); REQUIRE(U(SendMessageA));
    HWND p = mu_mk(WS_POPUP, 0, 50, 50, 300, 300, NULL);
    HWND lb = p ? U(CreateWindowExA)(0, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 5, 5, 150, 120, p, (HMENU)(UINT_PTR)5, GetModuleHandleA(NULL), NULL) : NULL;
    if (!p || !lb) { CHECK(0); mu_kill(p); return; }
    for (int i = 0; i < 20; i++) U(SendMessageA)(lb, LB_ADDSTRING, 0, (LPARAM)"item");
    DWORD a = UN(GetListBoxInfo)(lb);
    DWORD b = (DWORD)U(SendMessageA)(lb, 0x01B2 /* LB_GETLISTBOXINFO */, 0, 0);
    CHECKF(a == b, "api %lu msg %lu", a, b);
    CHECK(a > 0 && a < 20);                      /* some, but not all, of 20 items fit in 120 px */
    DWORD a2 = UN(GetListBoxInfo)(lb);
    CHECK(a2 == a);                              /* repeatable */
    mu_kill(p);
}
TEST(misc_ui_listboxinfo_non_listbox) {
    NEED_X(); REQUIRE(UN(GetListBoxInfo));
    HWND h = mu_top(); if (!h) { CHECK(0); return; }
    t_note("GetListBoxInfo(non-listbox) = %lu", UN(GetListBoxInfo)(h));
    t_note("GetListBoxInfo(NULL) = %lu", UN(GetListBoxInfo)(NULL));
    t_note("GetListBoxInfo(bogus) = %lu", UN(GetListBoxInfo)(BOGUS_HWND));
    mu_kill(h);
}

/* ---------- high contrast query ---------- */
TEST(misc_ui_highcontrast_query_wide) {
    NEED_X(); REQUIRE(U(SystemParametersInfoW));
    static WCHAR scheme[512];
    HIGHCONTRASTW hc; memset(&hc, 0, sizeof hc);
    hc.cbSize = sizeof hc; hc.lpszDefaultScheme = scheme;
    CHECK(U(SystemParametersInfoW)(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0));
    CHECK(hc.cbSize == sizeof hc);
    t_note("HIGHCONTRAST flags=0x%lx scheme=%ls", hc.dwFlags, hc.lpszDefaultScheme ? hc.lpszDefaultScheme : L"(null)");
    /* only documented flag bits may be set */
    CHECK((hc.dwFlags & ~(DWORD)(HCF_HIGHCONTRASTON | HCF_AVAILABLE | HCF_HOTKEYACTIVE | HCF_CONFIRMHOTKEY | HCF_HOTKEYSOUND | HCF_INDICATOR | HCF_HOTKEYAVAILABLE | 0x400)) == 0);
}
TEST(misc_ui_highcontrast_query_ansi_wide_parity) {
    NEED_X(); REQUIRE(U(SystemParametersInfoA)); REQUIRE(U(SystemParametersInfoW));
    static WCHAR ws[512]; static char as[512];
    HIGHCONTRASTW w; memset(&w, 0, sizeof w); w.cbSize = sizeof w; w.lpszDefaultScheme = ws;
    HIGHCONTRASTA a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpszDefaultScheme = as;
    BOOL rw = U(SystemParametersInfoW)(SPI_GETHIGHCONTRAST, sizeof w, &w, 0);
    BOOL ra = U(SystemParametersInfoA)(SPI_GETHIGHCONTRAST, sizeof a, &a, 0);
    CHECK(rw == ra);
    if (rw && ra) CHECK(w.dwFlags == a.dwFlags);
}
TEST(misc_ui_highcontrast_query_bad_cbsize) {
    NEED_X(); REQUIRE(U(SystemParametersInfoW));
    static WCHAR scheme[512];
    HIGHCONTRASTW hc; memset(&hc, 0, sizeof hc);
    hc.cbSize = 4; hc.lpszDefaultScheme = scheme;
    SetLastError(0);
    BOOL r = U(SystemParametersInfoW)(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0);
    t_note("HIGHCONTRAST get with cbSize=4: %d err=%lu", r, GetLastError());
    hc.cbSize = 0;
    SetLastError(0);
    r = U(SystemParametersInfoW)(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0);
    t_note("HIGHCONTRAST get with cbSize=0: %d err=%lu", r, GetLastError());
}

/* ---------- topmost normalisation through SetWindowPos ---------- */
TEST(misc_ui_topmost_setwindowpos_sets_and_clears_exstyle) {
    NEED_X(); REQUIRE(U(SetWindowPos));
    HWND h = mu_mk(WS_POPUP, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 10, 10, 60, 60, NULL);
    if (!h) { CHECK(0); return; }
    UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    CHECK((mu_ex(h) & WS_EX_TOPMOST) == 0);
    CHECK(U(SetWindowPos)(h, HWND_TOPMOST, 0, 0, 0, 0, f));
    CHECK((mu_ex(h) & WS_EX_TOPMOST) != 0);
    CHECK(U(SetWindowPos)(h, HWND_NOTOPMOST, 0, 0, 0, 0, f));
    CHECK((mu_ex(h) & WS_EX_TOPMOST) == 0);
    mu_kill(h);
}
TEST(misc_ui_topmost_created_flag_and_getwindow_order) {
    NEED_X(); REQUIRE(U(GetWindow));
    HWND n = mu_mk(WS_POPUP, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, 10, 10, 60, 60, NULL);
    HWND t = mu_mk(WS_POPUP, WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, 10, 10, 60, 60, NULL);
    if (!n || !t) { CHECK(0); mu_kill(n); mu_kill(t); return; }
    CHECK((mu_ex(t) & WS_EX_TOPMOST) != 0);
    int tp_above = 0, k = 0;
    for (HWND w = U(GetWindow)(n, GW_HWNDPREV); w && k < 20000; w = U(GetWindow)(w, GW_HWNDPREV), k++) if (w == t) tp_above = 1;
    CHECK(tp_above);                                    /* topmost windows precede non-topmost in z-order */
    mu_kill(n); mu_kill(t);
}
TEST(misc_ui_topmost_child_cannot_be_topmost_flag_roundtrip) {
    NEED_X();
    HWND p = mu_mk(WS_POPUP, 0, 0, 0, 100, 100, NULL);
    HWND c = p ? mu_child(p, 0) : NULL;
    if (!p || !c) { CHECK(0); mu_kill(p); return; }
    LONG_PTR old = mu_ex(c);
    U(SetWindowLongPtrA)(c, GWL_EXSTYLE, old | WS_EX_TOPMOST);
    t_note("child: TOPMOST bit after SetWindowLong = %d", (mu_ex(c) & WS_EX_TOPMOST) != 0);
    U(SetWindowLongPtrA)(c, GWL_EXSTYLE, old);
    CHECK((mu_ex(c) & WS_EX_TOPMOST) == (old & WS_EX_TOPMOST));
    mu_kill(p);
}

/* ---------- handle value properties ---------- */
TEST(misc_ui_handle_values_fit_32bit_and_roundtrip) {
    NEED_X();
    HWND hs[8];
    for (int i = 0; i < 8; i++) hs[i] = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    for (int i = 0; i < 8; i++) {
        if (!hs[i]) { CHECK(0); continue; }
        CHECKF(((ULONG_PTR)hs[i] >> 32) == 0, "handle %p exceeds 32 bits", hs[i]);
        HWND rt = (HWND)(LONG_PTR)(LONG)(ULONG_PTR)hs[i];     /* sign-extend round trip */
        CHECK(U(IsWindow)(rt) || ((LONG)(ULONG_PTR)hs[i]) < 0);
        HWND rt2 = (HWND)(ULONG_PTR)(DWORD)(ULONG_PTR)hs[i];
        CHECK(rt2 == hs[i]);
        CHECK(U(IsWindow)(rt2));
        for (int j = 0; j < i; j++) CHECK(hs[i] != hs[j]);
    }
    for (int i = 0; i < 8; i++) mu_kill(hs[i]);
}
TEST(misc_ui_handle_stale_not_reused_immediately) {
    NEED_X();
    HWND a = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    if (!a) { CHECK(0); return; }
    mu_kill(a);
    CHECK(!U(IsWindow)(a));
    HWND b = mu_mk(WS_POPUP, 0, 0, 0, 10, 10, NULL);
    CHECK(b != NULL);
    CHECK(!U(IsWindow)(a));                              /* stale handle must not resolve to the new window */
    CHECK(U(IsWindow)(b));
    mu_kill(b);
}

/* ---------- DDEML invalid conversation handles ---------- */
static HDDEDATA CALLBACK x_cb(UINT t, UINT f, HCONV c, HSZ a, HSZ b, HDDEDATA d, ULONG_PTR x, ULONG_PTR y) { return NULL; }
static DWORD x_init(void)
{
    DWORD id = 0;
    if (!U(DdeInitializeA)) return 0;
    if (U(DdeInitializeA)(&id, x_cb, APPCMD_CLIENTONLY | CBF_SKIP_ALLNOTIFICATIONS, 0) != DMLERR_NO_ERROR) return 0;
    return id;
}
TEST(misc_ui_dde_conv_null_clienttransaction) {
    REQUIRE(U(DdeClientTransaction)); REQUIRE(U(DdeUninitialize));
    DWORD id = x_init(); CHECK(id != 0); if (!id) return;
    CHECK(U(DdeClientTransaction)(NULL, 0, NULL, NULL, 0, XTYP_REQUEST, 100, NULL) == NULL);
    CHECK(U(DdeClientTransaction)(NULL, 0, (HCONV)(ULONG_PTR)0x12345678, NULL, 0, XTYP_REQUEST, 100, NULL) == NULL);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_conv_null_queryconvinfo) {
    REQUIRE(U(DdeQueryConvInfo)); REQUIRE(U(DdeUninitialize));
    DWORD id = x_init(); CHECK(id != 0); if (!id) return;
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    CHECK(U(DdeQueryConvInfo)(NULL, QID_SYNC, &ci) == 0);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_conv_null_setuserhandle) {
    REQUIRE(U(DdeSetUserHandle)); REQUIRE(U(DdeUninitialize));
    DWORD id = x_init(); CHECK(id != 0); if (!id) return;
    CHECK(!U(DdeSetUserHandle)(NULL, QID_SYNC, 5));
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_conv_null_disconnect) {
    REQUIRE(U(DdeDisconnect)); REQUIRE(U(DdeUninitialize));
    DWORD id = x_init(); CHECK(id != 0); if (!id) return;
    CHECK(!U(DdeDisconnect)(NULL));
    CHECK(!U(DdeDisconnect)((HCONV)(ULONG_PTR)0x12345678));
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_impersonate_invalid) {
    REQUIRE(U(DdeImpersonateClient)); REQUIRE(U(DdeUninitialize));
    DWORD id = x_init(); CHECK(id != 0); if (!id) return;
    CHECK(!U(DdeImpersonateClient)(NULL));
    CHECK(!U(DdeImpersonateClient)((HCONV)(ULONG_PTR)0x4321));
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_nextserver_invalid) {
    REQUIRE(U(DdeQueryNextServer)); REQUIRE(U(DdeUninitialize));
    DWORD id = x_init(); CHECK(id != 0); if (!id) return;
    CHECK(U(DdeQueryNextServer)(NULL, NULL) == NULL);
    CHECK(U(DdeQueryNextServer)((HCONVLIST)(ULONG_PTR)0x4321, NULL) == NULL);
    U(DdeUninitialize)(id);
}
