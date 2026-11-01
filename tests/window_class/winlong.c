#include "wc.h"

struct wfx { char name[64]; HWND h; ATOM atom; };
static int wfx_make(struct wfx *f, UINT cs, int cbc, int cbw, WNDPROC proc, DWORD style, DWORD ex, HWND parent, HMENU menu_or_id) {
    memset(f, 0, sizeof *f);
    wc_uname(f->name, sizeof f->name, "wcWl");
    f->atom = wc_regA(f->name, cs, cbc, cbw, proc);
    if (!f->atom) return 0;
    f->h = U(CreateWindowExA)(ex, f->name, "wl", style, 0, 0, 60, 60, parent, menu_or_id, wc_inst(), NULL);
    return f->h != NULL;
}
static void wfx_free(struct wfx *f) {
    if (f->h && U(IsWindow)(f->h)) U(DestroyWindow)(f->h);
    if (f->atom) U(UnregisterClassA)(f->name, wc_inst());
}

TEST(window_class_gwl_style_exstyle) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(GetWindowLongW)); REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrW));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP | WS_DISABLED, WS_EX_TOOLWINDOW, NULL, NULL));
    LONG st = U(GetWindowLongA)(f.h, GWL_STYLE), ex = U(GetWindowLongA)(f.h, GWL_EXSTYLE);
    CHECK((st & WS_POPUP) && (st & WS_DISABLED));
    CHECK(!(st & WS_VISIBLE));
    CHECK(!(st & WS_CHILD));
    CHECK(ex & WS_EX_TOOLWINDOW);
    EQ(U(GetWindowLongW)(f.h, GWL_STYLE), st);
    EQ(U(GetWindowLongW)(f.h, GWL_EXSTYLE), ex);
    EQ((DWORD)U(GetWindowLongPtrA)(f.h, GWL_STYLE), (DWORD)st);
    EQ((DWORD)U(GetWindowLongPtrW)(f.h, GWL_EXSTYLE), (DWORD)ex);
    wfx_free(&f);
}

TEST(window_class_gwl_style_set) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(SetWindowLongA)); REQUIRE(U(IsWindowEnabled));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    LONG st = U(GetWindowLongA)(f.h, GWL_STYLE);
    CHECK(U(IsWindowEnabled)(f.h));
    LONG old = U(SetWindowLongA)(f.h, GWL_STYLE, st | WS_DISABLED);
    EQ(old, st);
    CHECK(!U(IsWindowEnabled)(f.h));
    EQ(U(GetWindowLongA)(f.h, GWL_STYLE), st | WS_DISABLED);
    old = U(SetWindowLongA)(f.h, GWL_STYLE, st);
    EQ(old, st | WS_DISABLED);
    CHECK(U(IsWindowEnabled)(f.h));
    wfx_free(&f);
}

TEST(window_class_gwl_exstyle_set) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(SetWindowLongA)); REQUIRE(U(SetWindowLongPtrW));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    LONG ex = U(GetWindowLongA)(f.h, GWL_EXSTYLE);
    CHECK(!(ex & WS_EX_TOOLWINDOW));
    LONG old = U(SetWindowLongA)(f.h, GWL_EXSTYLE, ex | WS_EX_TOOLWINDOW);
    EQ(old, ex);
    CHECK(U(GetWindowLongA)(f.h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW);
    LONG_PTR o2 = U(SetWindowLongPtrW)(f.h, GWL_EXSTYLE, ex);
    EQ(o2, (LONG_PTR)(ex | WS_EX_TOOLWINDOW));
    CHECK(!(U(GetWindowLongA)(f.h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW));
    wfx_free(&f);
}

TEST(window_class_gwl_style_child_flag) {
    REQUIRE(U(GetWindowLongA));
    struct wfx p, c;
    CHECK(wfx_make(&p, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    CHECK(wfx_make(&c, 0, 0, 0, NULL, WS_CHILD | WS_VISIBLE, 0, p.h, (HMENU)(UINT_PTR)5));
    LONG st = U(GetWindowLongA)(c.h, GWL_STYLE);
    CHECK(st & WS_CHILD);
    CHECK(st & WS_VISIBLE);
    EQ(U(GetWindowLongA)(c.h, GWL_ID), 5);
    wfx_free(&c); wfx_free(&p);
}

TEST(window_class_gwl_id_child) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(GetDlgCtrlID));
    struct wfx p, c;
    CHECK(wfx_make(&p, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    CHECK(wfx_make(&c, 0, 0, 0, NULL, WS_CHILD, 0, p.h, (HMENU)(UINT_PTR)1234));
    EQ(U(GetWindowLongPtrA)(c.h, GWLP_ID), 1234);
    EQ(U(GetDlgCtrlID)(c.h), 1234);
    EQ(U(SetWindowLongPtrA)(c.h, GWLP_ID, 99), 1234);
    EQ(U(GetDlgCtrlID)(c.h), 99);
    EQ(U(GetWindowLongPtrA)(c.h, GWLP_ID), 99);
    /* 64-bit value kept by the Ptr variant, truncated by the 32-bit one */
    U(SetWindowLongPtrA)(c.h, GWLP_ID, 0x100000042LL);
    EQ(U(GetWindowLongPtrA)(c.h, GWLP_ID), 0x100000042LL);
    EQ(U(GetWindowLongA)(c.h, GWLP_ID), 0x42);
    EQ(U(GetWindowLongW)(c.h, GWLP_ID), 0x42);
    wfx_free(&c); wfx_free(&p);
}

TEST(window_class_gwl_id_toplevel_zero) {
    REQUIRE(U(GetWindowLongPtrA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_ID), 0);
    wfx_free(&f);
}

TEST(window_class_gwl_id_toplevel_menu_handle) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu)); REQUIRE(U(GetMenu));
    HMENU m = U(CreateMenu)();
    CHECK(m);
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_OVERLAPPED, 0, NULL, m));
    EQ(U(GetMenu)(f.h), (ULONG_PTR)m);
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_ID), (ULONG_PTR)m);
    wfx_free(&f);
    /* the menu was owned by the window and is destroyed with it */
    CHECK(!U(IsMenu)(m));
}

TEST(window_class_gwl_hinstance) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrW));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_HINSTANCE), (ULONG_PTR)wc_inst());
    EQ(U(GetWindowLongPtrW)(f.h, GWLP_HINSTANCE), (ULONG_PTR)wc_inst());
    wfx_free(&f);
}

TEST(window_class_gwl_hinstance_is_createwindow_arg) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(GetClassLongPtrA));
    char n[64]; wc_uname(n, sizeof n, "wcWlInst");
    CHECK(wc_regA(n, CS_GLOBALCLASS, 0, 0, NULL));
    /* Win10 RegisterClass/CreateWindow probe the hInstance as a PE image (version lookup);
     * a bogus address faults inside user32 (handled there, but the harness exception hook
     * counts it as a failure). Use a real module that is not the exe instead. */
    HINSTANCE fake = GetModuleHandleA("kernel32.dll");
    HWND h = U(CreateWindowExA)(0, n, "x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, fake, NULL);
    CHECK(h);
    if (h) {
        EQ(U(GetWindowLongPtrA)(h, GWLP_HINSTANCE), (ULONG_PTR)fake);
        EQ(U(GetClassLongPtrA)(h, GCLP_HMODULE), (ULONG_PTR)wc_inst());
        U(DestroyWindow)(h);
    }
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_gwl_32bit_access_to_pointer_indices) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(GetWindowLongW)); REQUIRE(U(SetWindowLongA)); REQUIRE(U(SetWindowLongW));
    struct wfx p, c;
    CHECK(wfx_make(&p, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    CHECK(wfx_make(&c, 0, 0, 0, NULL, WS_CHILD, 0, p.h, (HMENU)(UINT_PTR)3));
    static const int idx[] = { GWLP_WNDPROC, GWLP_HINSTANCE, GWLP_HWNDPARENT };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(GetWindowLongA)(c.h, idx[i]) == 0, "idx %d", idx[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "idx %d err %lu", idx[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECK(U(GetWindowLongW)(c.h, idx[i]) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_INDEX);
        SetLastError(MAGIC_ERR);
        CHECK(U(SetWindowLongA)(c.h, idx[i], 0x1234) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_INDEX);
        SetLastError(MAGIC_ERR);
        CHECK(U(SetWindowLongW)(c.h, idx[i], 0x1234) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    }
    /* unchanged afterwards */
    EQ(U(GetWindowLongPtrA)(c.h, GWLP_HWNDPARENT), (ULONG_PTR)p.h);
    EQ(U(GetWindowLongPtrA)(c.h, GWLP_HINSTANCE), (ULONG_PTR)wc_inst());
    wfx_free(&c); wfx_free(&p);
}

TEST(window_class_gwl_userdata) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(GetWindowLongA)); REQUIRE(U(SetWindowLongA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_USERDATA), 0);
    EQ(U(SetWindowLongPtrA)(f.h, GWLP_USERDATA, 0x1122334455667788LL), 0);
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_USERDATA), 0x1122334455667788LL);
    EQ(U(GetWindowLongA)(f.h, GWLP_USERDATA), 0x55667788);
    EQ(U(SetWindowLongPtrA)(f.h, GWLP_USERDATA, 7), 0x1122334455667788LL);
    EQ(U(SetWindowLongA)(f.h, GWLP_USERDATA, 8), 7);
    EQ(U(GetWindowLongA)(f.h, GWLP_USERDATA), 8);
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_USERDATA), 8);
    U(SetWindowLongPtrA)(f.h, GWLP_USERDATA, 0);
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_USERDATA), 0);
    wfx_free(&f);
}

TEST(window_class_gwl_userdata_words) {
    REQUIRE(U(GetWindowWord)); REQUIRE(U(SetWindowWord)); REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    U(SetWindowLongPtrA)(f.h, GWLP_USERDATA, 0x0001007B);
    EQ(U(GetWindowWord)(f.h, GWLP_USERDATA), 0x7B);
    EQ(U(SetWindowWord)(f.h, GWLP_USERDATA, 0x7C), 0x7B);
    EQ(U(GetWindowWord)(f.h, GWLP_USERDATA), 0x7C);
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_USERDATA), 0x0001007C);
    wfx_free(&f);
}

TEST(window_class_gww_pointer_indices_rejected) {
    REQUIRE(U(GetWindowWord)); REQUIRE(U(SetWindowWord));
    struct wfx p, c;
    CHECK(wfx_make(&p, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    CHECK(wfx_make(&c, 0, 0, 0, NULL, WS_CHILD, 0, p.h, (HMENU)(UINT_PTR)3));
    static const int idx[] = { GWLP_WNDPROC, GWLP_HINSTANCE, GWLP_HWNDPARENT, GWLP_ID };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(GetWindowWord)(c.h, idx[i]) == 0, "idx %d", idx[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "idx %d err %lu", idx[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetWindowWord)(c.h, idx[i], 0xbeef) == 0, "set idx %d", idx[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "set idx %d err %lu", idx[i], GetLastError());
    }
    EQ(U(GetWindowLongA)(c.h, GWLP_ID), 3);
    wfx_free(&c); wfx_free(&p);
}

TEST(window_class_gwl_hwndparent_semantics) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(GetParent)); REQUIRE(U(GetWindow));
    struct wfx p, c, o;
    CHECK(wfx_make(&p, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    CHECK(wfx_make(&c, 0, 0, 0, NULL, WS_CHILD, 0, p.h, (HMENU)(UINT_PTR)1));
    CHECK(wfx_make(&o, 0, 0, 0, NULL, WS_POPUP, 0, p.h, NULL));   /* owned popup */
    EQ(U(GetWindowLongPtrA)(p.h, GWLP_HWNDPARENT), 0);
    EQ(U(GetWindowLongPtrA)(c.h, GWLP_HWNDPARENT), (ULONG_PTR)p.h);
    EQ(U(GetWindowLongPtrA)(o.h, GWLP_HWNDPARENT), (ULONG_PTR)p.h);
    EQ(U(GetWindow)(o.h, GW_OWNER), (ULONG_PTR)p.h);
    EQ(U(GetParent)(o.h), (ULONG_PTR)p.h);
    wfx_free(&o); wfx_free(&c); wfx_free(&p);
}

TEST(window_class_gwl_set_hwndparent_on_toplevel_sets_owner) {
    REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(GetWindow)); REQUIRE(U(GetWindowLongPtrA));
    struct wfx p, o;
    CHECK(wfx_make(&p, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    CHECK(wfx_make(&o, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    EQ(U(GetWindow)(o.h, GW_OWNER), 0);
    EQ(U(SetWindowLongPtrA)(o.h, GWLP_HWNDPARENT, (LONG_PTR)p.h), 0);
    EQ(U(GetWindow)(o.h, GW_OWNER), (ULONG_PTR)p.h);
    EQ(U(GetWindowLongPtrA)(o.h, GWLP_HWNDPARENT), (ULONG_PTR)p.h);
    EQ(U(SetWindowLongPtrA)(o.h, GWLP_HWNDPARENT, 0), (ULONG_PTR)p.h);
    EQ(U(GetWindow)(o.h, GW_OWNER), 0);
    wfx_free(&o); wfx_free(&p);
}

/* ---- window extra bytes ---- */

TEST(window_class_wndextra_zero_and_roundtrip) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(SetWindowLongA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 32, NULL, WS_POPUP, 0, NULL, NULL));
    for (int i = 0; i < 32; i += 4) {
        SetLastError(MAGIC_ERR);
        EQ(U(GetWindowLongA)(f.h, i), 0);
        EXPECT_LAST_ERR(MAGIC_ERR);
    }
    for (int i = 0; i < 32; i += 4) EQ(U(SetWindowLongA)(f.h, i, 0x100 + i), 0);
    for (int i = 0; i < 32; i += 4) EQ(U(GetWindowLongA)(f.h, i), 0x100 + i);
    for (int i = 0; i < 32; i += 4) EQ(U(SetWindowLongA)(f.h, i, -i), 0x100 + i);
    for (int i = 0; i < 32; i += 4) EQ(U(GetWindowLongA)(f.h, i), (LONG)-i);
    wfx_free(&f);
}

TEST(window_class_wndextra_ptr) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrW)); REQUIRE(U(SetWindowLongPtrW));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 24, NULL, WS_POPUP, 0, NULL, NULL));
    const LONG_PTR v = 0x7766554433221100LL;
    EQ(U(SetWindowLongPtrA)(f.h, 0, v), 0);
    EQ(U(SetWindowLongPtrW)(f.h, 16, ~v), 0);
    EQ(U(GetWindowLongPtrA)(f.h, 0), v);
    EQ(U(GetWindowLongPtrW)(f.h, 0), v);
    EQ(U(GetWindowLongPtrA)(f.h, 16), ~v);
    EQ(U(GetWindowLongPtrA)(f.h, 8), 0);
    /* low/high halves visible through 32-bit access */
    EQ(U(GetWindowLongA)(f.h, 0), 0x33221100);
    EQ(U(GetWindowLongA)(f.h, 4), 0x77665544);
    wfx_free(&f);
}

TEST(window_class_wndextra_bounds) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(SetWindowLongA)); REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 16, NULL, WS_POPUP, 0, NULL, NULL));
    /* LONG valid up to offset 12, LONG_PTR valid up to offset 8 */
    CHECK(U(SetWindowLongPtrA)(f.h, 8, 6) == 0);
    EQ(U(GetWindowLongPtrA)(f.h, 8), 6);
    CHECK(U(SetWindowLongA)(f.h, 12, 5) == 0);
    EQ(U(GetWindowLongA)(f.h, 12), 5);
    EQ(U(GetWindowLongPtrA)(f.h, 8), 6 | (5LL << 32));
    static const int badL[] = { 13, 14, 15, 16, 17, 20, 100, 0x10000, 0x7fffffff };
    for (unsigned i = 0; i < sizeof badL / sizeof *badL; i++) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(GetWindowLongA)(f.h, badL[i]) == 0, "get %d", badL[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "get %d err %lu", badL[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetWindowLongA)(f.h, badL[i], 1) == 0, "set %d", badL[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "set %d err %lu", badL[i], GetLastError());
    }
    static const int badP[] = { 9, 12, 16, 24 };
    for (unsigned i = 0; i < sizeof badP / sizeof *badP; i++) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(GetWindowLongPtrA)(f.h, badP[i]) == 0, "getptr %d", badP[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "getptr %d err %lu", badP[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetWindowLongPtrA)(f.h, badP[i], 1) == 0, "setptr %d", badP[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "setptr %d err %lu", badP[i], GetLastError());
    }
    /* failed sets do not clobber neighbouring data */
    EQ(U(GetWindowLongA)(f.h, 12), 5);
    wfx_free(&f);
}

TEST(window_class_wndextra_none_negative_invalid) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(SetWindowLongA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    static const int bad[] = { 0, 4, 8, -1, -2, -3, -5, -7, -9, -10, -11, -13, -14, -15, -17, -18, -19, -22, -23, -24, -25, -26, -27, -28, -29, -30, -31, -32, -33, -34, -35, -100, -1000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(GetWindowLongA)(f.h, bad[i]) == 0, "idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "idx %d err %lu", bad[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetWindowLongA)(f.h, bad[i], 0x11) == 0, "set idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "set idx %d err %lu", bad[i], GetLastError());
    }
    wfx_free(&f);
}

TEST(window_class_wndextra_words) {
    REQUIRE(U(GetWindowWord)); REQUIRE(U(SetWindowWord)); REQUIRE(U(SetWindowLongA)); REQUIRE(U(GetWindowLongA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 8, NULL, WS_POPUP, 0, NULL, NULL));
    U(SetWindowLongA)(f.h, 0, 0xAABBCCDD);
    EQ(U(GetWindowWord)(f.h, 0), 0xCCDD);
    EQ(U(GetWindowWord)(f.h, 2), 0xAABB);
    EQ(U(SetWindowWord)(f.h, 2, 0x1111), 0xAABB);
    EQ(U(GetWindowLongA)(f.h, 0), 0x1111CCDD);
    EQ(U(SetWindowWord)(f.h, 6, 0x2222), 0);
    EQ(U(GetWindowWord)(f.h, 6), 0x2222);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetWindowWord)(f.h, 8) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetWindowWord)(f.h, 7) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(SetWindowWord)(f.h, 8, 1) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    wfx_free(&f);
}

TEST(window_class_wndextra_per_window) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(SetWindowLongA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 8, NULL, WS_POPUP, 0, NULL, NULL));
    HWND h2 = wc_createA(f.name, WS_POPUP, NULL);
    CHECK(h2);
    U(SetWindowLongA)(f.h, 0, 11);
    U(SetWindowLongA)(h2, 0, 22);
    EQ(U(GetWindowLongA)(f.h, 0), 11);
    EQ(U(GetWindowLongA)(h2, 0), 22);
    U(DestroyWindow)(h2);
    wfx_free(&f);
}

TEST(window_class_wndextra_values_visible_in_wm_create) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(SetWindowLongA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 4, NULL, WS_POPUP, 0, NULL, NULL));
    CHECK(f.h);
    /* fresh window: extra zeroed even after a previous window wrote values */
    U(SetWindowLongA)(f.h, 0, 0x55);
    U(DestroyWindow)(f.h);
    f.h = wc_createA(f.name, WS_POPUP, NULL);
    EQ(U(GetWindowLongA)(f.h, 0), 0);
    wfx_free(&f);
}

TEST(window_class_gwl_bad_hwnd) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(GetWindowLongW)); REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrW));
    REQUIRE(U(SetWindowLongA)); REQUIRE(U(SetWindowLongW)); REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrW));
    REQUIRE(U(GetWindowWord)); REQUIRE(U(SetWindowWord));
    static const HWND bad[] = { NULL, (HWND)1, (HWND)0x12345678, (HWND)0x20000 };
    static const int idx[] = { GWL_STYLE, GWL_ID, GWLP_USERDATA, 0 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        for (unsigned j = 0; j < 4; j++) {
#define BADCALL(call) do { SetLastError(MAGIC_ERR); CHECKF((call) == 0, "%s hwnd=%p idx=%d", #call, bad[i], idx[j]); \
    CHECKF(GetLastError() == ERR_INVALID_HWND, "%s hwnd=%p idx=%d err %lu", #call, bad[i], idx[j], GetLastError()); } while (0)
            BADCALL(U(GetWindowLongA)(bad[i], idx[j]));
            BADCALL(U(GetWindowLongW)(bad[i], idx[j]));
            BADCALL(U(GetWindowLongPtrA)(bad[i], idx[j]));
            BADCALL(U(GetWindowLongPtrW)(bad[i], idx[j]));
            BADCALL(U(SetWindowLongA)(bad[i], idx[j], 1));
            BADCALL(U(SetWindowLongW)(bad[i], idx[j], 1));
            BADCALL(U(SetWindowLongPtrA)(bad[i], idx[j], 1));
            BADCALL(U(SetWindowLongPtrW)(bad[i], idx[j], 1));
        }
        { unsigned j = 0;
        CHECK(U(GetWindowWord)(bad[i], idx[0]) == 0);
        CHECK(U(SetWindowWord)(bad[i], idx[0], 1) == 0); (void)j; }
    }
}

TEST(window_class_gwl_after_destroy) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 8, NULL, WS_POPUP, 0, NULL, NULL));
    HWND h = f.h;
    U(SetWindowLongPtrA)(h, 0, 0x99);
    U(DestroyWindow)(h);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetWindowLongPtrA)(h, 0) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_HWND);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetWindowLongPtrA)(h, GWL_STYLE) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_HWND);
    SetLastError(MAGIC_ERR);
    CHECK(U(SetWindowLongPtrA)(h, GWLP_USERDATA, 5) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_HWND);
    wfx_free(&f);
}

TEST(window_class_gwl_desktop_window) {
    REQUIRE(U(GetDesktopWindow)); REQUIRE(U(GetWindowLongPtrA));
    HWND d = U(GetDesktopWindow)();
    CHECK(d);
    LONG st = U(GetWindowLongA)(d, GWL_STYLE);
    CHECK(st & WS_VISIBLE);
    CHECK(st & WS_POPUP);
    EQ(U(GetWindowLongPtrA)(d, GWLP_HWNDPARENT), 0);
    char b[64]; U(GetClassNameA)(d, b, 64);
    EXPECT_STR(b, "#32769");
}

struct tctx { HWND h; LONG_PTR got_ud; LONG_PTR old; LONG_PTR got_extra; };
static DWORD WINAPI worker(void *p) {
    struct tctx *c = p;
    c->old = U(SetWindowLongPtrA)(c->h, GWLP_USERDATA, 0x4242);
    c->got_ud = U(GetWindowLongPtrA)(c->h, GWLP_USERDATA);
    U(SetWindowLongPtrA)(c->h, 0, 0x77);
    c->got_extra = U(GetWindowLongPtrA)(c->h, 0);
    return 0;
}
TEST(window_class_gwl_setuserdata_other_thread) {
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(SetWindowLongPtrA));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 8, NULL, WS_POPUP, 0, NULL, NULL));
    struct tctx t = { f.h, 0, 0, 0 };
    HANDLE th = CreateThread(NULL, 0, worker, &t, 0, NULL);
    CHECK(th && WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    if (th) CloseHandle(th);
    EQ(t.old, 0); EQ(t.got_ud, 0x4242); EQ(t.got_extra, 0x77);
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_USERDATA), 0x4242);
    EQ(U(GetWindowLongPtrA)(f.h, 0), 0x77);
    wfx_free(&f);
}

TEST(window_class_gwl_style_visible_toggle_via_showwindow) {
    REQUIRE(U(GetWindowLongA)); REQUIRE(U(ShowWindow)); REQUIRE(U(IsWindowVisible));
    struct wfx f; CHECK(wfx_make(&f, 0, 0, 0, NULL, WS_POPUP, 0, NULL, NULL));
    CHECK(!(U(GetWindowLongA)(f.h, GWL_STYLE) & WS_VISIBLE));
    U(ShowWindow)(f.h, SW_SHOWNOACTIVATE);
    CHECK(U(GetWindowLongA)(f.h, GWL_STYLE) & WS_VISIBLE);
    U(ShowWindow)(f.h, SW_HIDE);
    CHECK(!(U(GetWindowLongA)(f.h, GWL_STYLE) & WS_VISIBLE));
    wfx_free(&f);
}
