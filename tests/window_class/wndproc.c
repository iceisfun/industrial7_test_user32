#include "wc.h"

struct afx { char name[64]; WCHAR wname[64]; HWND h; ATOM atom; int wide; };
/* wide=0: ANSI class with wc_recA; wide=1: Unicode class with wc_recW; proc override optional */
static int afx_make(struct afx *f, int wide, WNDPROC proc, DWORD style) {
    memset(f, 0, sizeof *f);
    wc_uname(f->name, sizeof f->name, "wcPr");
    wc_widen(f->wname, f->name);
    f->wide = wide;
    if (wide) f->atom = wc_regW(f->wname, 0, 0, 0, proc ? proc : wc_recW);
    else f->atom = wc_regA(f->name, 0, 0, 0, proc ? proc : wc_recA);
    if (!f->atom) return 0;
    memset(&wc_rec, 0, sizeof wc_rec);
    f->h = wide ? wc_createW(f->wname, style, NULL) : wc_createA(f->name, style, NULL);
    return f->h != NULL;
}
static void afx_free(struct afx *f) {
    if (f->h && U(IsWindow)(f->h)) U(DestroyWindow)(f->h);
    if (f->atom) U(UnregisterClassA)(f->name, wc_inst());
}

/* ---------- IsWindowUnicode ---------- */

TEST(window_class_isunicode_ansi_class) {
    REQUIRE(U(IsWindowUnicode));
    struct afx f; CHECK(afx_make(&f, 0, NULL, WS_POPUP));
    EQ(U(IsWindowUnicode)(f.h), FALSE);
    afx_free(&f);
}
TEST(window_class_isunicode_wide_class) {
    REQUIRE(U(IsWindowUnicode));
    struct afx f; CHECK(afx_make(&f, 1, NULL, WS_POPUP));
    EQ(U(IsWindowUnicode)(f.h), TRUE);
    afx_free(&f);
}
TEST(window_class_isunicode_follows_class_not_create_call) {
    REQUIRE(U(IsWindowUnicode)); REQUIRE(U(CreateWindowExW)); REQUIRE(U(CreateWindowExA));
    struct afx a, w; CHECK(afx_make(&a, 0, NULL, WS_POPUP)); CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    HWND ha = U(CreateWindowExW)(0, a.wname, L"t", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    HWND hw = U(CreateWindowExA)(0, w.name, "t", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(ha && hw);
    if (ha) { EQ(U(IsWindowUnicode)(ha), FALSE); U(DestroyWindow)(ha); }
    if (hw) { EQ(U(IsWindowUnicode)(hw), TRUE); U(DestroyWindow)(hw); }
    afx_free(&a); afx_free(&w);
}
TEST(window_class_isunicode_bad_hwnd) {
    REQUIRE(U(IsWindowUnicode));
    static const HWND bad[] = { NULL, (HWND)1, (HWND)0x12345678, (HWND)0x20000 };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(MAGIC_ERR);
        EQ(U(IsWindowUnicode)(bad[i]), FALSE);
        t_note("IsWindowUnicode(%p) err %lu", bad[i], GetLastError());
    }
}
TEST(window_class_isunicode_destroyed) {
    REQUIRE(U(IsWindowUnicode));
    struct afx f; CHECK(afx_make(&f, 1, NULL, WS_POPUP));
    HWND h = f.h; U(DestroyWindow)(h);
    EQ(U(IsWindowUnicode)(h), FALSE);
    afx_free(&f);
}
TEST(window_class_isunicode_system_classes) {
    REQUIRE(U(IsWindowUnicode));
    static const char *cls[] = { "Button", "Edit", "Static", "ListBox", "ComboBox", "ScrollBar" };
    for (unsigned i = 0; i < 6; i++) {
        HWND ha = U(CreateWindowExA)(0, cls[i], "x", WS_POPUP, 0, 0, 40, 40, NULL, NULL, wc_inst(), NULL);
        CHECKF(ha != NULL, "%s", cls[i]);
        if (ha) { t_note("IsWindowUnicode(%s via CreateWindowExA) = %d", cls[i], U(IsWindowUnicode)(ha)); U(DestroyWindow)(ha); }
    }
}

/* ---------- CREATESTRUCT conversion ---------- */

TEST(window_class_createstruct_ansi_proc_wide_create) {
    REQUIRE(U(CreateWindowExW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    U(DestroyWindow)(a.h); a.h = NULL;
    memset(&wc_rec, 0, sizeof wc_rec);
    HWND h = U(CreateWindowExW)(0, a.wname, L"TitleW123", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    EQ(wc_rec.nccreate, 1);
    EQ(wc_rec.created, 1);
    EQ(wc_rec.unicode, 0);
    EXPECT_STR(wc_rec.textA, "TitleW123");
    if (h) U(DestroyWindow)(h);
    afx_free(&a);
}
TEST(window_class_createstruct_wide_proc_ansi_create) {
    REQUIRE(U(CreateWindowExA));
    struct afx w; CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    U(DestroyWindow)(w.h); w.h = NULL;
    memset(&wc_rec, 0, sizeof wc_rec);
    HWND h = U(CreateWindowExA)(0, w.name, "TitleA456", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    EQ(wc_rec.nccreate, 1);
    EQ(wc_rec.created, 1);
    EQ(wc_rec.unicode, 1);
    EXPECT_WSTR(wc_rec.textW, L"TitleA456");
    if (h) U(DestroyWindow)(h);
    afx_free(&w);
}
TEST(window_class_createstruct_same_flavour) {
    REQUIRE(U(CreateWindowExA));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    U(DestroyWindow)(a.h); a.h = NULL;
    memset(&wc_rec, 0, sizeof wc_rec);
    HWND h = U(CreateWindowExA)(0, a.name, "SameA", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    EXPECT_STR(wc_rec.textA, "SameA");
    if (h) U(DestroyWindow)(h);
    afx_free(&a);
}

/* ---------- message conversion ---------- */

TEST(window_class_msg_settext_w_to_ansi_proc) {
    REQUIRE(U(SendMessageW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    memset(&wc_rec, 0, sizeof wc_rec);
    EQ(U(SendMessageW)(a.h, WM_SETTEXT, 0, (LPARAM)L"fromW"), TRUE);
    EQ(wc_rec.count, 1);
    EQ(wc_rec.unicode, 0);
    EXPECT_STR(wc_rec.textA, "fromW");
    afx_free(&a);
}
TEST(window_class_msg_settext_a_to_wide_proc) {
    REQUIRE(U(SendMessageA));
    struct afx w; CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    memset(&wc_rec, 0, sizeof wc_rec);
    EQ(U(SendMessageA)(w.h, WM_SETTEXT, 0, (LPARAM)"fromA"), TRUE);
    EQ(wc_rec.count, 1);
    EQ(wc_rec.unicode, 1);
    EXPECT_WSTR(wc_rec.textW, L"fromA");
    afx_free(&w);
}
TEST(window_class_msg_settext_same_flavour) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(SendMessageW));
    struct afx a, w; CHECK(afx_make(&a, 0, NULL, WS_POPUP)); CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    memset(&wc_rec, 0, sizeof wc_rec);
    U(SendMessageA)(a.h, WM_SETTEXT, 0, (LPARAM)"sameA");
    EQ(wc_rec.unicode, 0); EXPECT_STR(wc_rec.textA, "sameA");
    U(SendMessageW)(w.h, WM_SETTEXT, 0, (LPARAM)L"sameW");
    EQ(wc_rec.unicode, 1); EXPECT_WSTR(wc_rec.textW, L"sameW");
    afx_free(&a); afx_free(&w);
}
TEST(window_class_msg_user_passthrough) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(SendMessageW));
    struct afx a, w; CHECK(afx_make(&a, 0, NULL, WS_POPUP)); CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    static const struct { WPARAM w; LPARAM l; } v[] = { {0, 0}, {1, 2}, {0xFFFFFFFFFFFFFFFFull, (LPARAM)-1}, {0x1122334455667788ull, 0x7766554433221100ll} };
    for (unsigned i = 0; i < 4; i++) {
        memset(&wc_rec, 0, sizeof wc_rec);
        EQ(U(SendMessageW)(a.h, WM_USER + 77, v[i].w, v[i].l), 0x1234);
        EQ(wc_rec.wp, v[i].w); EQ(wc_rec.lp, (ULONG_PTR)v[i].l); EQ(wc_rec.unicode, 0);
        memset(&wc_rec, 0, sizeof wc_rec);
        EQ(U(SendMessageA)(w.h, WM_USER + 77, v[i].w, v[i].l), 0x1234);
        EQ(wc_rec.wp, v[i].w); EQ(wc_rec.lp, (ULONG_PTR)v[i].l); EQ(wc_rec.unicode, 1);
    }
    afx_free(&a); afx_free(&w);
}

/* ---------- SetWindowLongPtr WNDPROC ---------- */

TEST(window_class_wndproc_get_returns_registered) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    EQ(U(GetWindowLongPtrA)(a.h, GWLP_WNDPROC), (ULONG_PTR)wc_recA);
    ULONG_PTR pw = U(GetWindowLongPtrW)(a.h, GWLP_WNDPROC);
    CHECK(pw != 0 && pw != (ULONG_PTR)wc_recA);
    afx_free(&a);
    struct afx w; CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    EQ(U(GetWindowLongPtrW)(w.h, GWLP_WNDPROC), (ULONG_PTR)wc_recW);
    ULONG_PTR pa = U(GetWindowLongPtrA)(w.h, GWLP_WNDPROC);
    CHECK(pa != 0 && pa != (ULONG_PTR)wc_recW);
    afx_free(&w);
}

TEST(window_class_wndproc_set_a_on_wide_window_switches_to_ansi) {
    REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(IsWindowUnicode)); REQUIRE(U(GetWindowLongPtrA));
    struct afx w; CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    EQ(U(IsWindowUnicode)(w.h), TRUE);
    ULONG_PTR old = U(SetWindowLongPtrA)(w.h, GWLP_WNDPROC, (LONG_PTR)wc_recA);
    CHECK(old != 0);
    EQ(U(IsWindowUnicode)(w.h), FALSE);
    EQ(U(GetWindowLongPtrA)(w.h, GWLP_WNDPROC), (ULONG_PTR)wc_recA);
    /* now an ANSI proc: text arrives narrow */
    memset(&wc_rec, 0, sizeof wc_rec);
    U(SendMessageW)(w.h, WM_SETTEXT, 0, (LPARAM)L"toA");
    EQ(wc_rec.unicode, 0); EXPECT_STR(wc_rec.textA, "toA");
    afx_free(&w);
}

TEST(window_class_wndproc_set_w_on_ansi_window_switches_to_wide) {
    REQUIRE(U(SetWindowLongPtrW)); REQUIRE(U(IsWindowUnicode)); REQUIRE(U(GetWindowLongPtrW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    EQ(U(IsWindowUnicode)(a.h), FALSE);
    ULONG_PTR old = U(SetWindowLongPtrW)(a.h, GWLP_WNDPROC, (LONG_PTR)wc_recW);
    CHECK(old != 0);
    EQ(U(IsWindowUnicode)(a.h), TRUE);
    EQ(U(GetWindowLongPtrW)(a.h, GWLP_WNDPROC), (ULONG_PTR)wc_recW);
    memset(&wc_rec, 0, sizeof wc_rec);
    U(SendMessageA)(a.h, WM_SETTEXT, 0, (LPARAM)"toW");
    EQ(wc_rec.unicode, 1); EXPECT_WSTR(wc_rec.textW, L"toW");
    afx_free(&a);
}

TEST(window_class_wndproc_old_proc_return_flavour) {
    REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    /* A set on an A window returns the real A proc */
    ULONG_PTR old = U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, (LONG_PTR)wc_defA);
    EQ(old, (ULONG_PTR)wc_recA);
    old = U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, (LONG_PTR)wc_recA);
    EQ(old, (ULONG_PTR)wc_defA);
    afx_free(&a);
    struct afx w; CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    old = U(SetWindowLongPtrW)(w.h, GWLP_WNDPROC, (LONG_PTR)wc_defW);
    EQ(old, (ULONG_PTR)wc_recW);
    old = U(SetWindowLongPtrW)(w.h, GWLP_WNDPROC, (LONG_PTR)wc_recW);
    EQ(old, (ULONG_PTR)wc_defW);
    afx_free(&w);
}

TEST(window_class_wndproc_flip_flop) {
    REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrW)); REQUIRE(U(IsWindowUnicode));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    for (int i = 0; i < 10; i++) {
        U(SetWindowLongPtrW)(a.h, GWLP_WNDPROC, (LONG_PTR)wc_recW);
        EQ(U(IsWindowUnicode)(a.h), TRUE);
        U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, (LONG_PTR)wc_recA);
        EQ(U(IsWindowUnicode)(a.h), FALSE);
    }
    EQ(U(GetWindowLongPtrA)(a.h, GWLP_WNDPROC), (ULONG_PTR)wc_recA);
    afx_free(&a);
}

TEST(window_class_wndproc_set_null) {
    REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrA));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    SetLastError(MAGIC_ERR);
    ULONG_PTR old = U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, 0);
    t_note("SetWindowLongPtrA(WNDPROC, NULL) -> %p err %lu; now %p", (void *)old, GetLastError(), (void *)U(GetWindowLongPtrA)(a.h, GWLP_WNDPROC));
    /* restore a sane proc so DestroyWindow cannot call a NULL proc */
    U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, (LONG_PTR)wc_defA);
    afx_free(&a);
}

TEST(window_class_wndproc_32bit_set_rejected) {
    REQUIRE(U(SetWindowLongA)); REQUIRE(U(GetWindowLongA)); REQUIRE(U(GetWindowLongPtrA));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    SetLastError(MAGIC_ERR);
    CHECK(U(SetWindowLongA)(a.h, GWLP_WNDPROC, 0xdeadbeef) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetWindowLongA)(a.h, GWLP_WNDPROC) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    EQ(U(GetWindowLongPtrA)(a.h, GWLP_WNDPROC), (ULONG_PTR)wc_recA);
    afx_free(&a);
}

/* ---------- CallWindowProc ---------- */

TEST(window_class_callwindowproc_direct) {
    REQUIRE(U(CallWindowProcA)); REQUIRE(U(CallWindowProcW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    memset(&wc_rec, 0, sizeof wc_rec);
    EQ(U(CallWindowProcA)(wc_recA, a.h, WM_USER + 77, 0x11, 0x22), 0x1234);
    EQ(wc_rec.count, 1); EQ(wc_rec.wp, 0x11); EQ(wc_rec.lp, 0x22); EQ(wc_rec.unicode, 0);
    memset(&wc_rec, 0, sizeof wc_rec);
    EQ(U(CallWindowProcW)(wc_recW, a.h, WM_USER + 77, 0x33, 0x44), 0x1234);
    EQ(wc_rec.count, 1); EQ(wc_rec.wp, 0x33); EQ(wc_rec.lp, 0x44); EQ(wc_rec.unicode, 1);
    afx_free(&a);
}

TEST(window_class_callwindowproc_real_proc_no_conversion) {
    REQUIRE(U(CallWindowProcA)); REQUIRE(U(CallWindowProcW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    /* A real function address is invoked as is: the W entry point hands the pointer over unchanged */
    memset(&wc_rec, 0, sizeof wc_rec);
    U(CallWindowProcW)(wc_recW, a.h, WM_SETTEXT, 0, (LPARAM)L"raw");
    EQ(wc_rec.unicode, 1); EXPECT_WSTR(wc_rec.textW, L"raw");
    memset(&wc_rec, 0, sizeof wc_rec);
    U(CallWindowProcA)(wc_recA, a.h, WM_SETTEXT, 0, (LPARAM)"raw");
    EQ(wc_rec.unicode, 0); EXPECT_STR(wc_rec.textA, "raw");
    afx_free(&a);
}

TEST(window_class_callwindowproc_handle_converts_w_to_a) {
    REQUIRE(U(CallWindowProcW)); REQUIRE(U(GetWindowLongPtrW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    WNDPROC viaW = (WNDPROC)U(GetWindowLongPtrW)(a.h, GWLP_WNDPROC);
    CHECK(viaW != NULL && viaW != wc_recA);
    memset(&wc_rec, 0, sizeof wc_rec);
    LRESULT r = U(CallWindowProcW)(viaW, a.h, WM_SETTEXT, 0, (LPARAM)L"conv1");
    EQ(r, TRUE);
    EQ(wc_rec.count, 1); EQ(wc_rec.unicode, 0);
    EXPECT_STR(wc_rec.textA, "conv1");
    afx_free(&a);
}

TEST(window_class_callwindowproc_handle_converts_a_to_w) {
    REQUIRE(U(CallWindowProcA)); REQUIRE(U(GetWindowLongPtrA));
    struct afx w; CHECK(afx_make(&w, 1, NULL, WS_POPUP));
    WNDPROC viaA = (WNDPROC)U(GetWindowLongPtrA)(w.h, GWLP_WNDPROC);
    CHECK(viaA != NULL && viaA != wc_recW);
    memset(&wc_rec, 0, sizeof wc_rec);
    LRESULT r = U(CallWindowProcA)(viaA, w.h, WM_SETTEXT, 0, (LPARAM)"conv2");
    EQ(r, TRUE);
    EQ(wc_rec.count, 1); EQ(wc_rec.unicode, 1);
    EXPECT_WSTR(wc_rec.textW, L"conv2");
    afx_free(&w);
}

TEST(window_class_callwindowproc_handle_passthrough_user_msg) {
    REQUIRE(U(CallWindowProcW)); REQUIRE(U(GetWindowLongPtrW));
    struct afx a; CHECK(afx_make(&a, 0, NULL, WS_POPUP));
    WNDPROC viaW = (WNDPROC)U(GetWindowLongPtrW)(a.h, GWLP_WNDPROC);
    memset(&wc_rec, 0, sizeof wc_rec);
    EQ(U(CallWindowProcW)(viaW, a.h, WM_USER + 77, 0xAB, 0xCD), 0x1234);
    EQ(wc_rec.wp, 0xAB); EQ(wc_rec.lp, 0xCD);
    afx_free(&a);
}

TEST(window_class_callwindowproc_defwindowproc_text) {
    REQUIRE(U(CallWindowProcA)); REQUIRE(U(CallWindowProcW));
    char n[64]; wc_uname(n, sizeof n, "wcCwpDef");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    HWND h = U(CreateWindowExA)(0, n, "inittext", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    EQ(U(CallWindowProcA)(U(DefWindowProcA), h, WM_GETTEXTLENGTH, 0, 0), 8);
    char b[32];
    EQ(U(CallWindowProcA)(U(DefWindowProcA), h, WM_GETTEXT, 32, (LPARAM)b), 8);
    EXPECT_STR(b, "inittext");
    WCHAR w[32];
    EQ(U(CallWindowProcW)(U(DefWindowProcW), h, WM_GETTEXT, 32, (LPARAM)w), 8);
    EXPECT_WSTR(w, L"inittext");
    U(DestroyWindow)(h);
    U(UnregisterClassA)(n, wc_inst());
}

/* subclass chain */
static int g_chain[8], g_nchain;
static WNDPROC g_old1, g_old2;
static LRESULT CALLBACK sub1(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 90) { g_chain[g_nchain++] = 1; return U(CallWindowProcA)(g_old1, h, m, w + 1, l); }
    return U(CallWindowProcA)(g_old1, h, m, w, l);
}
static LRESULT CALLBACK sub2(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 90) { g_chain[g_nchain++] = 2; return U(CallWindowProcA)(g_old2, h, m, w * 10, l); }
    return U(CallWindowProcA)(g_old2, h, m, w, l);
}
static LRESULT CALLBACK base90(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 90) { g_chain[g_nchain++] = 0; return (LRESULT)w + 1000; }
    return U(DefWindowProcA)(h, m, w, l);
}

TEST(window_class_subclass_chain) {
    REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(SendMessageA)); REQUIRE(U(CallWindowProcA));
    struct afx a; CHECK(afx_make(&a, 0, base90, WS_POPUP));
    g_nchain = 0;
    EQ(U(SendMessageA)(a.h, WM_USER + 90, 5, 0), 1005);
    EQ(g_nchain, 1);
    g_old1 = (WNDPROC)U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, (LONG_PTR)sub1);
    EQ(g_old1, base90);
    g_old2 = (WNDPROC)U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, (LONG_PTR)sub2);
    EQ(g_old2, sub1);
    g_nchain = 0;
    EQ(U(SendMessageA)(a.h, WM_USER + 90, 5, 0), 5 * 10 + 1 + 1000);
    EQ(g_nchain, 3);
    EQ(g_chain[0], 2); EQ(g_chain[1], 1); EQ(g_chain[2], 0);
    /* unsubclass */
    EQ(U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, (LONG_PTR)sub1), sub2);
    EQ(U(SetWindowLongPtrA)(a.h, GWLP_WNDPROC, (LONG_PTR)base90), sub1);
    g_nchain = 0;
    EQ(U(SendMessageA)(a.h, WM_USER + 90, 7, 0), 1007);
    afx_free(&a);
}

static WNDPROC g_edit_old;
static int g_edit_seen_gettext, g_edit_seen_settext_a;
static LRESULT CALLBACK edit_sub_a(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_GETTEXT) g_edit_seen_gettext++;
    if (m == WM_SETTEXT) g_edit_seen_settext_a++;
    return U(CallWindowProcA)(g_edit_old, h, m, w, l);
}

TEST(window_class_subclass_system_edit_ansi_proc) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(IsWindowUnicode)); REQUIRE(U(SendMessageW)); REQUIRE(U(SendMessageA));
    HWND h = U(CreateWindowExA)(0, "Edit", "orig", WS_POPUP, 0, 0, 80, 20, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (!h) return;
    g_edit_old = (WNDPROC)U(SetWindowLongPtrA)(h, GWLP_WNDPROC, (LONG_PTR)edit_sub_a);
    CHECK(g_edit_old != NULL);
    EQ(U(IsWindowUnicode)(h), FALSE);
    g_edit_seen_gettext = g_edit_seen_settext_a = 0;
    EQ(U(SendMessageA)(h, WM_SETTEXT, 0, (LPARAM)"newtext"), TRUE);
    EQ(g_edit_seen_settext_a, 1);
    char b[32] = "";
    EQ(U(SendMessageA)(h, WM_GETTEXT, 32, (LPARAM)b), 7);
    EXPECT_STR(b, "newtext");
    EQ(g_edit_seen_gettext, 1);
    /* W sender: subclass still sees one message, text is converted for the ANSI proc */
    WCHAR w[32] = L"";
    EQ(U(SendMessageW)(h, WM_GETTEXT, 32, (LPARAM)w), 7);
    EXPECT_WSTR(w, L"newtext");
    EQ(g_edit_seen_gettext, 2);
    U(SetWindowLongPtrA)(h, GWLP_WNDPROC, (LONG_PTR)g_edit_old);
    U(DestroyWindow)(h);
}

/* ---------- DefWindowProc ---------- */

struct dfx { char name[64]; HWND h; };
static int dfx_make(struct dfx *f, const char *title, DWORD style) {
    memset(f, 0, sizeof *f);
    wc_uname(f->name, sizeof f->name, "wcDef");
    if (!wc_regA(f->name, 0, 0, 0, NULL)) return 0;
    f->h = U(CreateWindowExA)(0, f->name, title, style, 0, 0, 60, 40, NULL, NULL, wc_inst(), NULL);
    return f->h != NULL;
}
static void dfx_free(struct dfx *f) { if (f->h) U(DestroyWindow)(f->h); U(UnregisterClassA)(f->name, wc_inst()); }

TEST(window_class_defproc_text_basic) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(SendMessageW));
    struct dfx f; CHECK(dfx_make(&f, "hello", WS_POPUP));
    char b[32]; WCHAR w[32];
    EQ(U(SendMessageA)(f.h, WM_GETTEXTLENGTH, 0, 0), 5);
    EQ(U(SendMessageW)(f.h, WM_GETTEXTLENGTH, 0, 0), 5);
    EQ(U(SendMessageA)(f.h, WM_GETTEXT, 32, (LPARAM)b), 5);
    EXPECT_STR(b, "hello");
    EQ(U(SendMessageW)(f.h, WM_GETTEXT, 32, (LPARAM)w), 5);
    EXPECT_WSTR(w, L"hello");
    EQ(U(SendMessageA)(f.h, WM_SETTEXT, 0, (LPARAM)"changed text"), TRUE);
    EQ(U(SendMessageA)(f.h, WM_GETTEXTLENGTH, 0, 0), 12);
    EQ(U(SendMessageW)(f.h, WM_SETTEXT, 0, (LPARAM)L"wide"), TRUE);
    EQ(U(SendMessageA)(f.h, WM_GETTEXT, 32, (LPARAM)b), 4);
    EXPECT_STR(b, "wide");
    dfx_free(&f);
}

TEST(window_class_defproc_gettext_buffer_sizes) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(SendMessageW));
    struct dfx f; CHECK(dfx_make(&f, "abcdef", WS_POPUP));
    char b[16]; WCHAR w[16];
    /* wParam is the buffer size including the terminator */
    static const struct { int size; int ret; const char *str; } t[] = {
        {7, 6, "abcdef"}, {8, 6, "abcdef"}, {100, 6, "abcdef"}, {6, 5, "abcde"}, {2, 1, "a"}, {1, 0, ""} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        memset(b, 'x', sizeof b);
        int n = (int)U(SendMessageA)(f.h, WM_GETTEXT, t[i].size, (LPARAM)b);
        CHECKF(n == t[i].ret, "size %d ret %d want %d", t[i].size, n, t[i].ret);
        CHECKF(!strcmp(b, t[i].str), "size %d str '%s'", t[i].size, b);
        for (int k = 0; k < 16; k++) w[k] = 'x';
        n = (int)U(SendMessageW)(f.h, WM_GETTEXT, t[i].size, (LPARAM)w);
        CHECKF(n == t[i].ret, "W size %d ret %d want %d", t[i].size, n, t[i].ret);
    }
    memset(b, 'x', sizeof b);
    EQ(U(SendMessageA)(f.h, WM_GETTEXT, 0, (LPARAM)b), 0);
    CHECK(b[0] == 'x');
    dfx_free(&f);
}

TEST(window_class_defproc_notext) {
    REQUIRE(U(SendMessageA));
    struct dfx f; CHECK(dfx_make(&f, NULL, WS_POPUP));
    char b[8] = "zz";
    EQ(U(SendMessageA)(f.h, WM_GETTEXTLENGTH, 0, 0), 0);
    EQ(U(SendMessageA)(f.h, WM_GETTEXT, 8, (LPARAM)b), 0);
    CHECK(b[0] == 0);
    dfx_free(&f);
}

TEST(window_class_defproc_windowtext_api_parity) {
    REQUIRE(U(SetWindowTextA)); REQUIRE(U(SetWindowTextW)); REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetWindowTextW)); REQUIRE(U(GetWindowTextLengthA)); REQUIRE(U(GetWindowTextLengthW));
    struct dfx f; CHECK(dfx_make(&f, "init", WS_POPUP));
    char b[32]; WCHAR w[32];
    CHECK(U(SetWindowTextW)(f.h, L"WideSet"));
    EQ(U(GetWindowTextA)(f.h, b, 32), 7); EXPECT_STR(b, "WideSet");
    EQ(U(GetWindowTextLengthA)(f.h), 7);
    EQ(U(GetWindowTextLengthW)(f.h), 7);
    dfx_free(&f);
}
    

TEST(window_class_gettext_api_sizes) {
    REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetWindowTextW));
    struct dfx f; CHECK(dfx_make(&f, "abcdef", WS_POPUP));
    char b[16]; WCHAR w[16];
    static const struct { int size; int ret; } t[] = { {7, 6}, {100, 6}, {6, 5}, {2, 1}, {1, 0}, {0, 0} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        memset(b, 'x', sizeof b);
        EQ(U(GetWindowTextA)(f.h, b, t[i].size), t[i].ret);
        if (t[i].size > 0) CHECKF(strlen(b) == (size_t)t[i].ret, "size %d len %d", t[i].size, (int)strlen(b));
        else CHECK(b[0] == 'x');
        for (int k = 0; k < 16; k++) w[k] = 'x';
        EQ(U(GetWindowTextW)(f.h, w, t[i].size), t[i].ret);
        if (t[i].size > 0) CHECK((int)lstrlenW(w) == t[i].ret);
    }
    dfx_free(&f);
}

TEST(window_class_gettext_bad_hwnd) {
    REQUIRE(U(GetWindowTextA)); REQUIRE(U(GetWindowTextW)); REQUIRE(U(GetWindowTextLengthA)); REQUIRE(U(GetWindowTextLengthW)); REQUIRE(U(SetWindowTextA)); REQUIRE(U(SetWindowTextW));
    static const HWND bad[] = { NULL, (HWND)1, (HWND)0x12345678 };
    for (unsigned i = 0; i < 3; i++) {
        char b[8] = "q"; WCHAR w[8] = L"q";
        SetLastError(MAGIC_ERR);
        EQ(U(GetWindowTextA)(bad[i], b, 8), 0);
        t_note("err %lu", GetLastError());
        SetLastError(MAGIC_ERR);
        EQ(U(GetWindowTextW)(bad[i], w, 8), 0);
        t_note("err %lu", GetLastError());
        SetLastError(MAGIC_ERR);
        EQ(U(GetWindowTextLengthA)(bad[i]), 0);
        t_note("err %lu", GetLastError());
        SetLastError(MAGIC_ERR);
        EQ(U(GetWindowTextLengthW)(bad[i]), 0);
        t_note("err %lu", GetLastError());
        SetLastError(MAGIC_ERR);
        EQ(U(SetWindowTextA)(bad[i], "x"), FALSE);
        t_note("err %lu", GetLastError());
        SetLastError(MAGIC_ERR);
        EQ(U(SetWindowTextW)(bad[i], L"x"), FALSE);
        t_note("err %lu", GetLastError());
    }
}

TEST(window_class_defproc_close_destroys) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(IsWindow));
    struct dfx f; CHECK(dfx_make(&f, "c", WS_POPUP));
    HWND h = f.h;
    U(SendMessageA)(h, WM_CLOSE, 0, 0);
    CHECK(!U(IsWindow)(h));
    f.h = NULL;
    dfx_free(&f);
}

TEST(window_class_defproc_unhandled_messages_zero) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(SendMessageW));
    struct dfx f; CHECK(dfx_make(&f, "u", WS_POPUP));
    static const UINT msgs[] = { WM_NULL, WM_USER, WM_USER + 1, WM_APP, WM_APP + 100, 0xC000, 0xC123, 0xBFFF };
    for (unsigned i = 0; i < sizeof msgs / sizeof *msgs; i++) {
        EQ(U(SendMessageA)(f.h, msgs[i], 0, 0), 0);
        EQ(U(SendMessageW)(f.h, msgs[i], 0x123, 0x456), 0);
        EQ(U(SendMessageA)(f.h, msgs[i], (WPARAM)-1, (LPARAM)-1), 0);
    }
    dfx_free(&f);
}

TEST(window_class_defproc_direct_calls) {
    REQUIRE(U(DefWindowProcA)); REQUIRE(U(DefWindowProcW));
    struct dfx f; CHECK(dfx_make(&f, "direct", WS_POPUP));
    EQ(U(DefWindowProcA)(f.h, WM_NULL, 0, 0), 0);
    EQ(U(DefWindowProcW)(f.h, WM_NULL, 0, 0), 0);
    EQ(U(DefWindowProcA)(f.h, WM_USER + 5, 1, 2), 0);
    EQ(U(DefWindowProcA)(f.h, WM_GETTEXTLENGTH, 0, 0), 6);
    EQ(U(DefWindowProcW)(f.h, WM_GETTEXTLENGTH, 0, 0), 6);
    char b[16]; WCHAR w[16];
    EQ(U(DefWindowProcA)(f.h, WM_GETTEXT, 16, (LPARAM)b), 6); EXPECT_STR(b, "direct");
    EQ(U(DefWindowProcW)(f.h, WM_GETTEXT, 16, (LPARAM)w), 6); EXPECT_WSTR(w, L"direct");
    EQ(U(DefWindowProcA)(f.h, WM_SETTEXT, 0, (LPARAM)"fromDefA"), TRUE);
    EQ(U(DefWindowProcW)(f.h, WM_GETTEXT, 16, (LPARAM)w), 8); EXPECT_WSTR(w, L"fromDefA");
    EQ(U(DefWindowProcW)(f.h, WM_SETTEXT, 0, (LPARAM)L"fromDefW"), TRUE);
    EQ(U(DefWindowProcA)(f.h, WM_GETTEXT, 16, (LPARAM)b), 8); EXPECT_STR(b, "fromDefW");
    dfx_free(&f);
}

TEST(window_class_defproc_bad_hwnd) {
    REQUIRE(U(DefWindowProcA)); REQUIRE(U(DefWindowProcW));
    static const HWND bad[] = { NULL, (HWND)1, (HWND)0x12345678 };
    for (unsigned i = 0; i < 3; i++) {
        EQ(U(DefWindowProcA)(bad[i], WM_NULL, 0, 0), 0);
        EQ(U(DefWindowProcW)(bad[i], WM_USER, 1, 2), 0);
        EQ(U(DefWindowProcA)(bad[i], WM_GETTEXTLENGTH, 0, 0), 0);
    }
}

TEST(window_class_defproc_queryopen_and_create) {
    REQUIRE(U(DefWindowProcA));
    struct dfx f; CHECK(dfx_make(&f, "q", WS_POPUP));
    EQ(U(DefWindowProcA)(f.h, WM_QUERYOPEN, 0, 0), TRUE);
    EQ(U(DefWindowProcA)(f.h, WM_DESTROY, 0, 0), 0);
    dfx_free(&f);
}

TEST(window_class_defproc_seticon_geticon) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(LoadIconA));
    struct dfx f; CHECK(dfx_make(&f, "i", WS_POPUP));
    HICON big = U(LoadIconA)(NULL, IDI_APPLICATION), small = U(LoadIconA)(NULL, IDI_ERROR);
    CHECK(big && small);
    EQ(U(SendMessageA)(f.h, WM_GETICON, ICON_BIG, 0) == 0 || 1, 1);
    LRESULT old = U(SendMessageA)(f.h, WM_SETICON, ICON_BIG, (LPARAM)big);
    t_note("previous big icon %p", (void *)old);
    EQ(U(SendMessageA)(f.h, WM_GETICON, ICON_BIG, 0), (ULONG_PTR)big);
    old = U(SendMessageA)(f.h, WM_SETICON, ICON_SMALL, (LPARAM)small);
    EQ(U(SendMessageA)(f.h, WM_GETICON, ICON_SMALL, 0), (ULONG_PTR)small);
    EQ(U(SendMessageA)(f.h, WM_SETICON, ICON_BIG, 0), (ULONG_PTR)big);
    EQ(U(SendMessageA)(f.h, WM_GETICON, ICON_BIG, 0), 0);
    EQ(U(SendMessageA)(f.h, WM_SETICON, ICON_SMALL, 0), (ULONG_PTR)small);
    dfx_free(&f);
}

/* ---------- window creation failure paths ---------- */

TEST(window_class_create_unknown_class) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(CreateWindowExW));
    SetLastError(MAGIC_ERR);
    HWND h = U(CreateWindowExA)(0, "wcNoSuchClassForCreate", "x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h == NULL);
    EXPECT_LAST_ERR(1407);   /* ERROR_CANNOT_FIND_WND_CLASS */
    SetLastError(MAGIC_ERR);
    h = U(CreateWindowExW)(0, L"wcNoSuchClassForCreate", L"x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h == NULL);
    EXPECT_LAST_ERR(1407);
    SetLastError(MAGIC_ERR);
    h = U(CreateWindowExA)(0, MAKEINTATOM(0xBFFE), "x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h == NULL);
    t_note("CreateWindowEx with unknown integer atom: err %lu", GetLastError());
}

TEST(window_class_create_after_unregister) {
    REQUIRE(U(CreateWindowExA));
    char n[64]; wc_uname(n, sizeof n, "wcGone");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    HWND h = wc_createA(n, WS_POPUP, NULL);
    CHECK(h);
    U(DestroyWindow)(h);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
    SetLastError(MAGIC_ERR);
    CHECK(wc_createA(n, WS_POPUP, NULL) == NULL);
    EXPECT_LAST_ERR(1407);
}

static LRESULT CALLBACK abort_nccreate(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) return FALSE;
    return U(DefWindowProcA)(h, m, w, l);
}
static int g_abort_create_msgs;
static LRESULT CALLBACK abort_create(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CREATE) return -1;
    if (m == WM_DESTROY) g_abort_create_msgs++;
    return U(DefWindowProcA)(h, m, w, l);
}

TEST(window_class_create_aborted_by_nccreate) {
    REQUIRE(U(CreateWindowExA));
    char n[64]; wc_uname(n, sizeof n, "wcAbortNc");
    CHECK(wc_regA(n, 0, 0, 0, abort_nccreate));
    HWND h = wc_createA(n, WS_POPUP, NULL);
    CHECK(h == NULL);
    CHECK(U(UnregisterClassA)(n, wc_inst()));   /* no window left behind */
}

TEST(window_class_create_aborted_by_create) {
    REQUIRE(U(CreateWindowExA));
    char n[64]; wc_uname(n, sizeof n, "wcAbortCr");
    CHECK(wc_regA(n, 0, 0, 0, abort_create));
    g_abort_create_msgs = 0;
    HWND h = wc_createA(n, WS_POPUP, NULL);
    CHECK(h == NULL);
    t_note("WM_DESTROY seen after WM_CREATE == -1: %d", g_abort_create_msgs);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

/* ---------- system classes ---------- */

static const char *sys_classes[] = { "Button", "ComboBox", "ComboLBox", "Edit", "ListBox", "MDIClient", "ScrollBar", "Static",
                                     "#32768", "#32769", "#32770", "#32771", "#32772" };
#define NSYS (sizeof sys_classes / sizeof *sys_classes)

TEST(window_class_sys_getclassinfo_exists) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(GetClassInfoExW));
    for (unsigned i = 0; i < NSYS; i++) {
        WNDCLASSEXA a; memset(&a, 0, sizeof a); a.cbSize = sizeof a;
        WCHAR w[32]; wc_widen(w, sys_classes[i]);
        WNDCLASSEXW ww; memset(&ww, 0, sizeof ww); ww.cbSize = sizeof ww;
        SetLastError(MAGIC_ERR);
        UINT_PTR r = U(GetClassInfoExA)(NULL, sys_classes[i], &a);
        CHECKF(r != 0, "%s (err %lu)", sys_classes[i], GetLastError());
        CHECKF(U(GetClassInfoExW)(NULL, w, &ww) != 0, "W %s", sys_classes[i]);
        if (!r) continue;
        CHECKF(a.lpfnWndProc != NULL, "%s proc", sys_classes[i]);
        t_note("%-8s atom=0x%x style=0x%x cbCls=%d cbWnd=%d hInst=%p cursor=%p bg=%p proc=%p", sys_classes[i], (unsigned)r, a.style, a.cbClsExtra, a.cbWndExtra, a.hInstance, a.hCursor, a.hbrBackground, a.lpfnWndProc);
    }
}

TEST(window_class_sys_getclassinfo_case_insensitive) {
    REQUIRE(U(GetClassInfoExA));
    static const char *v[][2] = { {"Button", "BUTTON"}, {"Button", "button"}, {"Edit", "EDIT"}, {"Static", "sTaTiC"}, {"ListBox", "LISTBOX"}, {"ComboBox", "combobox"}, {"ScrollBar", "SCROLLBAR"} };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        WNDCLASSEXA a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); a.cbSize = b.cbSize = sizeof a;
        UINT_PTR ra = U(GetClassInfoExA)(NULL, v[i][0], &a), rb = U(GetClassInfoExA)(NULL, v[i][1], &b);
        CHECKF(ra && rb, "%s/%s", v[i][0], v[i][1]);
        if (ra && rb) { CHECK(ra == rb); CHECK(a.lpfnWndProc == b.lpfnWndProc); CHECK(a.style == b.style); CHECK(a.cbWndExtra == b.cbWndExtra); }
    }
}

TEST(window_class_sys_getclassinfo_by_atom) {
    REQUIRE(U(GetClassInfoExA));
    for (unsigned i = 0; i < NSYS; i++) {
        WNDCLASSEXA a; memset(&a, 0, sizeof a); a.cbSize = sizeof a;
        UINT_PTR atom = U(GetClassInfoExA)(NULL, sys_classes[i], &a);
        if (!atom) continue;
        WNDCLASSEXA b; memset(&b, 0, sizeof b); b.cbSize = sizeof b;
        UINT_PTR r2 = U(GetClassInfoExA)(NULL, MAKEINTATOM(atom), &b);
        CHECKF(r2 != 0, "%s by atom 0x%x", sys_classes[i], (unsigned)atom);
        if (r2) CHECK(b.lpfnWndProc == a.lpfnWndProc);
    }
}

TEST(window_class_sys_dialog_class_extra) {
    REQUIRE(U(GetClassInfoExA));
    WNDCLASSEXA a; memset(&a, 0, sizeof a); a.cbSize = sizeof a;
    CHECK(U(GetClassInfoExA)(NULL, "#32770", &a));
    CHECKF(a.cbWndExtra >= DLGWINDOWEXTRA, "dialog class cbWndExtra %d", a.cbWndExtra);
}

TEST(window_class_sys_misspelled_not_found) {
    REQUIRE(U(GetClassInfoExA));
    static const char *bad[] = { "Butto", "Buttons", "Edi", "Statics", "#32767", "#32773", "# 32768", "#", "#0", "wc#32768" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        WNDCLASSEXA a; a.cbSize = sizeof a;
        SetLastError(MAGIC_ERR);
        UINT_PTR r = U(GetClassInfoExA)(NULL, bad[i], &a);
        CHECKF(r == 0, "'%s' unexpectedly found", bad[i]);
        if (!r) { CHECKF(GetLastError() == ERR_CLASS_NOT_EXIST, "'%s' err %lu", bad[i], GetLastError()); }
    }
}

TEST(window_class_sys_getclassinfo_w_proc_flavour) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(GetClassInfoExW));
    for (unsigned i = 0; i < 8; i++) {
        WNDCLASSEXA a; memset(&a, 0, sizeof a); a.cbSize = sizeof a;
        WNDCLASSEXW w; memset(&w, 0, sizeof w); w.cbSize = sizeof w;
        WCHAR wn[32]; wc_widen(wn, sys_classes[i]);
        if (!U(GetClassInfoExA)(NULL, sys_classes[i], &a) || !U(GetClassInfoExW)(NULL, wn, &w)) continue;
        t_note("%s proc A=%p W=%p", sys_classes[i], a.lpfnWndProc, w.lpfnWndProc);
        CHECK(a.style == w.style);
        CHECK(a.cbClsExtra == w.cbClsExtra);
        CHECK(a.cbWndExtra == w.cbWndExtra);
    }
}

TEST(window_class_sys_button_window) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SendMessageA));
    HWND h = U(CreateWindowExA)(0, "Button", "chk", WS_POPUP | BS_AUTOCHECKBOX, 0, 0, 80, 20, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (!h) return;
    EQ(U(SendMessageA)(h, BM_GETCHECK, 0, 0), BST_UNCHECKED);
    U(SendMessageA)(h, BM_SETCHECK, BST_CHECKED, 0);
    EQ(U(SendMessageA)(h, BM_GETCHECK, 0, 0), BST_CHECKED);
    U(SendMessageA)(h, BM_SETCHECK, BST_INDETERMINATE, 0);
    t_note("BM_GETCHECK after INDETERMINATE on auto checkbox: %lld", (long long)U(SendMessageA)(h, BM_GETCHECK, 0, 0));
    char b[16]; EQ(U(GetWindowTextA)(h, b, 16), 3); EXPECT_STR(b, "chk");
    EQ(U(GetWindowLongA)(h, GWL_STYLE) & BS_TYPEMASK, BS_AUTOCHECKBOX);
    U(DestroyWindow)(h);
}

TEST(window_class_sys_edit_window) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SendMessageA)); REQUIRE(U(SendMessageW));
    HWND h = U(CreateWindowExW)(0, L"Edit", L"abc", WS_POPUP, 0, 0, 80, 20, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (!h) return;
    EQ(U(SendMessageA)(h, WM_GETTEXTLENGTH, 0, 0), 3);
    t_note("EM_GETLIMITTEXT default %lld", (long long)U(SendMessageW)(h, EM_GETLIMITTEXT, 0, 0));
    U(SendMessageA)(h, EM_SETSEL, 1, 2);
    DWORD s = 0, e = 0;
    U(SendMessageA)(h, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
    EQ(s, 1); EQ(e, 2);
    U(SendMessageW)(h, EM_REPLACESEL, TRUE, (LPARAM)L"XYZ");
    char b[16]; U(GetWindowTextA)(h, b, 16);
    EXPECT_STR(b, "aXYZc");
    U(DestroyWindow)(h);
}

TEST(window_class_sys_static_window) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SendMessageA));
    HWND h = U(CreateWindowExA)(0, "Static", "label", WS_POPUP | SS_LEFT, 0, 0, 80, 20, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (!h) return;
    char b[16]; EQ(U(GetWindowTextA)(h, b, 16), 5); EXPECT_STR(b, "label");
    EQ(U(SetWindowTextA)(h, "other"), TRUE);
    EQ(U(GetWindowTextLengthA)(h), 5);
    U(DestroyWindow)(h);
}

TEST(window_class_sys_listbox_window) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SendMessageA));
    HWND h = U(CreateWindowExA)(0, "ListBox", "", WS_POPUP, 0, 0, 80, 60, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (!h) return;
    EQ(U(SendMessageA)(h, LB_GETCOUNT, 0, 0), 0);
    EQ(U(SendMessageA)(h, LB_ADDSTRING, 0, (LPARAM)"one"), 0);
    EQ(U(SendMessageA)(h, LB_ADDSTRING, 0, (LPARAM)"two"), 1);
    EQ(U(SendMessageW)(h, LB_ADDSTRING, 0, (LPARAM)L"three"), 2);
    EQ(U(SendMessageA)(h, LB_GETCOUNT, 0, 0), 3);
    char b[16]; EQ(U(SendMessageA)(h, LB_GETTEXT, 2, (LPARAM)b), 5); EXPECT_STR(b, "three");
    EQ(U(SendMessageA)(h, LB_GETTEXTLEN, 0, 0), 3);
    U(DestroyWindow)(h);
}

TEST(window_class_sys_combobox_window) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SendMessageA));
    HWND h = U(CreateWindowExA)(0, "ComboBox", "", WS_POPUP | CBS_DROPDOWNLIST, 0, 0, 100, 100, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (!h) return;
    EQ(U(SendMessageA)(h, CB_GETCOUNT, 0, 0), 0);
    EQ(U(SendMessageA)(h, CB_ADDSTRING, 0, (LPARAM)"a"), 0);
    EQ(U(SendMessageA)(h, CB_ADDSTRING, 0, (LPARAM)"b"), 1);
    EQ(U(SendMessageA)(h, CB_GETCOUNT, 0, 0), 2);
    EQ(U(SendMessageA)(h, CB_SETCURSEL, 1, 0), 1);
    EQ(U(SendMessageA)(h, CB_GETCURSEL, 0, 0), 1);
    U(DestroyWindow)(h);
}

TEST(window_class_sys_scrollbar_window) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(SendMessageA));
    HWND h = U(CreateWindowExA)(0, "ScrollBar", "", WS_POPUP | SBS_HORZ, 0, 0, 100, 20, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (!h) return;
    U(SendMessageA)(h, SBM_SETRANGE, 10, 50);
    int mn = 0, mx = 0;
    U(SendMessageA)(h, SBM_GETRANGE, (WPARAM)&mn, (LPARAM)&mx);
    EQ(mn, 10); EQ(mx, 50);
    U(SendMessageA)(h, SBM_SETPOS, 25, FALSE);
    EQ(U(SendMessageA)(h, SBM_GETPOS, 0, 0), 25);
    U(DestroyWindow)(h);
}

/* Superclassing a built-in class */
static WNDPROC g_btn_orig;
static int g_btn_hits;
static LRESULT CALLBACK super_btn_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == BM_GETCHECK) g_btn_hits++;
    return U(CallWindowProcA)(g_btn_orig, h, m, w, l);
}

TEST(window_class_superclass_button) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(RegisterClassExA)); REQUIRE(U(SendMessageA)); REQUIRE(U(IsWindowUnicode));
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc); wc.cbSize = sizeof wc;
    CHECK(U(GetClassInfoExA)(NULL, "Button", &wc));
    g_btn_orig = wc.lpfnWndProc;
    char n[64]; wc_uname(n, sizeof n, "wcSuperBtn2");
    wc.lpszClassName = n; wc.hInstance = wc_inst(); wc.lpfnWndProc = super_btn_proc; wc.style &= ~CS_GLOBALCLASS;
    int cbw = wc.cbWndExtra;
    CHECK(U(RegisterClassExA)(&wc));
    HWND h = U(CreateWindowExA)(0, n, "super", WS_POPUP | BS_CHECKBOX, 0, 0, 80, 20, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (h) {
        g_btn_hits = 0;
        U(SendMessageA)(h, BM_SETCHECK, BST_CHECKED, 0);
        EQ(U(SendMessageA)(h, BM_GETCHECK, 0, 0), BST_CHECKED);
        EQ(g_btn_hits, 1);
        WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
        CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
        EQ(o.cbWndExtra, cbw);
        EQ(o.lpfnWndProc, super_btn_proc);
        char c[64]; U(GetClassNameA)(h, c, 64);
        EXPECT_STR(c, n);
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_sys_window_wndproc_nonnull) {
    REQUIRE(U(CreateWindowExA)); REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrW)); REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(GetClassLongPtrW));
    static const char *cls[] = { "Button", "Edit", "Static", "ListBox", "ComboBox", "ScrollBar" };
    for (unsigned i = 0; i < 6; i++) {
        HWND h = U(CreateWindowExA)(0, cls[i], "x", WS_POPUP, 0, 0, 40, 40, NULL, NULL, wc_inst(), NULL);
        CHECKF(h != NULL, "%s", cls[i]);
        if (!h) continue;
        CHECK(U(GetWindowLongPtrA)(h, GWLP_WNDPROC) != 0);
        CHECK(U(GetWindowLongPtrW)(h, GWLP_WNDPROC) != 0);
        CHECK(U(GetClassLongPtrA)(h, GCLP_WNDPROC) != 0);
        CHECK(U(GetClassLongPtrW)(h, GCLP_WNDPROC) != 0);
        EQ(U(GetClassLongPtrA)(h, GCLP_HMODULE) != 0, 1);
        EQ(U(GetWindowLongPtrA)(h, GWLP_HINSTANCE), (ULONG_PTR)wc_inst());
        U(DestroyWindow)(h);
    }
}

TEST(window_class_sys_register_over_system_name) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    /* a process-global registration under a system class name collides */
    static const char *names[] = { "Button", "EDIT", "static", "#32768" };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(MAGIC_ERR);
        ATOM a = wc_regA(names[i], CS_GLOBALCLASS, 0, 0, NULL);
        CHECKF(a == 0, "global class '%s' registered", names[i]);
        if (!a) EXPECT_LAST_ERR(ERR_CLASS_EXISTS);
        else U(UnregisterClassA)(names[i], wc_inst());
    }
}

TEST(window_class_sys_local_class_shadows_nothing_global) {
    REQUIRE(U(GetClassInfoExA));
    /* a global system class is visible through any instance handle */
    WNDCLASSEXA a; memset(&a, 0, sizeof a); a.cbSize = sizeof a;
    CHECK(U(GetClassInfoExA)(wc_inst(), "Button", &a));
    CHECK(U(GetClassInfoExA)(GetModuleHandleA("kernel32.dll"), "Edit", &a));
    CHECK(U(GetClassInfoExA)((HINSTANCE)0x1230000, "Static", &a));
}
