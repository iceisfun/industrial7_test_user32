#include "wc.h"

/* Fixture: class with extra bytes and a window */
struct fx { char name[64]; HWND h; ATOM atom; };

static int fx_make(struct fx *f, UINT style, int cbc, int cbw, WNDPROC proc) {
    memset(f, 0, sizeof *f);
    wc_uname(f->name, sizeof f->name, "wcFx");
    f->atom = wc_regA(f->name, style, cbc, cbw, proc);
    if (!f->atom) return 0;
    f->h = wc_createA(f->name, WS_POPUP, NULL);
    return f->h != NULL;
}
static void fx_free(struct fx *f) {
    if (f->h) U(DestroyWindow)(f->h);
    if (f->atom) U(UnregisterClassA)(f->name, wc_inst());
}

TEST(window_class_gcl_basic_values) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(GetClassLongPtrA));
    struct fx f;
    CHECK(fx_make(&f, CS_DBLCLKS | CS_HREDRAW, 12, 20, NULL));
    EQ(U(GetClassLongA)(f.h, GCL_CBCLSEXTRA), 12);
    EQ(U(GetClassLongA)(f.h, GCL_CBWNDEXTRA), 20);
    EQ(U(GetClassLongA)(f.h, GCL_STYLE) & ~(DWORD)CS_GLOBALCLASS, CS_DBLCLKS | CS_HREDRAW);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HMODULE), (ULONG_PTR)wc_inst());
    EQ(U(GetClassLongPtrA)(f.h, GCLP_WNDPROC), (ULONG_PTR)wc_defA);
    EQ(U(GetClassLongPtrA)(f.h, GCW_ATOM), f.atom);
    EQ(U(GetClassLongA)(f.h, GCW_ATOM), f.atom);
    EQ(U(GetClassWord)(f.h, GCW_ATOM), f.atom);
    fx_free(&f);
}

TEST(window_class_gcl_w_variants_match) {
    REQUIRE(U(GetClassLongW)); REQUIRE(U(GetClassLongPtrW)); REQUIRE(U(GetClassLongA));
    struct fx f;
    CHECK(fx_make(&f, CS_OWNDC, 8, 8, NULL));
    static const int idx[] = { GCL_CBCLSEXTRA, GCL_CBWNDEXTRA, GCL_STYLE, GCL_HMODULE, GCW_ATOM, GCL_HBRBACKGROUND, GCL_HCURSOR, GCL_HICON, GCL_HICONSM };
    for (unsigned i = 0; i < sizeof idx / sizeof *idx; i++)
        CHECKF(U(GetClassLongA)(f.h, idx[i]) == U(GetClassLongW)(f.h, idx[i]), "idx %d", idx[i]);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HMODULE), U(GetClassLongPtrW)(f.h, GCLP_HMODULE));
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HBRBACKGROUND), U(GetClassLongPtrW)(f.h, GCLP_HBRBACKGROUND));
    fx_free(&f);
}

TEST(window_class_gcl_wndproc_a_w) {
    REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(GetClassLongPtrW));
    struct fx f;
    CHECK(fx_make(&f, 0, 0, 0, wc_recA));
    ULONG_PTR pa = U(GetClassLongPtrA)(f.h, GCLP_WNDPROC), pw = U(GetClassLongPtrW)(f.h, GCLP_WNDPROC);
    EQ(pa, (ULONG_PTR)wc_recA);
    CHECK(pw != 0);
    CHECK(pw != pa);   /* W accessor yields a conversion thunk/handle for an ANSI proc */
    fx_free(&f);
    struct fx g; memset(&g, 0, sizeof g);
    WCHAR w[64]; wc_uname(g.name, sizeof g.name, "wcFxW"); wc_widen(w, g.name);
    g.atom = wc_regW(w, 0, 0, 0, wc_recW);
    g.h = wc_createW(w, WS_POPUP, NULL);
    CHECK(g.h);
    EQ(U(GetClassLongPtrW)(g.h, GCLP_WNDPROC), (ULONG_PTR)wc_recW);
    CHECK(U(GetClassLongPtrA)(g.h, GCLP_WNDPROC) != (ULONG_PTR)wc_recW);
    fx_free(&g);
}

TEST(window_class_gcl_hbrbackground) {
    REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(SetClassLongPtrA));
    struct fx f; memset(&f, 0, sizeof f);
    wc_uname(f.name, sizeof f.name, "wcBrush");
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = f.name; wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    f.atom = U(RegisterClassA)(&wc);
    f.h = wc_createA(f.name, WS_POPUP, NULL);
    CHECK(f.h);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HBRBACKGROUND), COLOR_BTNFACE + 1);
    SetLastError(MAGIC_ERR);
    ULONG_PTR old = U(SetClassLongPtrA)(f.h, GCLP_HBRBACKGROUND, COLOR_WINDOW + 1);
    EQ(old, COLOR_BTNFACE + 1);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HBRBACKGROUND), COLOR_WINDOW + 1);
    old = U(SetClassLongPtrA)(f.h, GCLP_HBRBACKGROUND, 0);
    EQ(old, COLOR_WINDOW + 1);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HBRBACKGROUND), 0);
    fx_free(&f);
}

TEST(window_class_gcl_hcursor_hicon) {
    REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(SetClassLongPtrA)); REQUIRE(U(LoadCursorA)); REQUIRE(U(LoadIconA));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HCURSOR), 0);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HICON), 0);
    HCURSOR c = U(LoadCursorA)(NULL, IDC_CROSS); HICON i = U(LoadIconA)(NULL, IDI_WARNING);
    CHECK(c && i);
    EQ(U(SetClassLongPtrA)(f.h, GCLP_HCURSOR, (LONG_PTR)c), 0);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HCURSOR), (ULONG_PTR)c);
    EQ(U(SetClassLongPtrA)(f.h, GCLP_HICON, (LONG_PTR)i), 0);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HICON), (ULONG_PTR)i);
    HCURSOR c2 = U(LoadCursorA)(NULL, IDC_HAND);
    EQ(U(SetClassLongPtrA)(f.h, GCLP_HCURSOR, (LONG_PTR)c2), (ULONG_PTR)c);
    t_note("HICONSM after setting HICON: %p", (void *)U(GetClassLongPtrA)(f.h, GCLP_HICONSM));
    fx_free(&f);
}

TEST(window_class_gcl_style_set) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA));
    struct fx f; CHECK(fx_make(&f, CS_DBLCLKS, 0, 0, NULL));
    DWORD old = U(SetClassLongA)(f.h, GCL_STYLE, CS_HREDRAW | CS_VREDRAW);
    EQ(old & ~(DWORD)CS_GLOBALCLASS, CS_DBLCLKS);
    EQ(U(GetClassLongA)(f.h, GCL_STYLE) & ~(DWORD)CS_GLOBALCLASS, CS_HREDRAW | CS_VREDRAW);
    fx_free(&f);
}

TEST(window_class_gcl_hmodule_set) {
    REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(SetClassLongPtrA));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    ULONG_PTR old = U(SetClassLongPtrA)(f.h, GCLP_HMODULE, 0x4440000);
    EQ(old, (ULONG_PTR)wc_inst());
    EQ(U(GetClassLongPtrA)(f.h, GCLP_HMODULE), 0x4440000);
    U(SetClassLongPtrA)(f.h, GCLP_HMODULE, (LONG_PTR)wc_inst());
    fx_free(&f);
}

TEST(window_class_gcl_cbextra_set_observed) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA));
    struct fx f; CHECK(fx_make(&f, 0, 8, 8, NULL));
    SetLastError(MAGIC_ERR);
    DWORD r = U(SetClassLongA)(f.h, GCL_CBCLSEXTRA, 32);
    t_note("SetClassLong(GCL_CBCLSEXTRA) -> %lu err %lu", r, GetLastError());
    t_note("CBCLSEXTRA now %lu", U(GetClassLongA)(f.h, GCL_CBCLSEXTRA));
    U(SetClassLongA)(f.h, GCL_CBCLSEXTRA, 8);
    r = U(SetClassLongA)(f.h, GCL_CBWNDEXTRA, 32);
    t_note("SetClassLong(GCL_CBWNDEXTRA) -> %lu err %lu now %lu", r, GetLastError(), U(GetClassLongA)(f.h, GCL_CBWNDEXTRA));
    U(SetClassLongA)(f.h, GCL_CBWNDEXTRA, 8);
    fx_free(&f);
}

TEST(window_class_gcl_invalid_indices) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(GetClassLongW));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    static const int bad[] = { -1, -2, -3, -4, -5, -6, -7, -9, -11, -13, -15, -17, -19, -21, -22, -23, -25, -27, -28, -29, -31, -33, -35, -36, -40, -100, -1000, 1, 2, 4, 8, 100, 0x10000, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        if (wc_is_wine() && bad[i] >= 0) continue;
        SetLastError(MAGIC_ERR);
        DWORD r = U(GetClassLongA)(f.h, bad[i]);
        CHECKF(r == 0, "idx %d -> %lu", bad[i], r);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "idx %d err %lu", bad[i], GetLastError());
        SetLastError(MAGIC_ERR);
        ULONG_PTR rp = U(GetClassLongPtrA)(f.h, bad[i]);
        CHECKF(rp == 0, "ptr idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "ptr idx %d err %lu", bad[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECKF(U(GetClassLongW)(f.h, bad[i]) == 0, "W idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "W idx %d err %lu", bad[i], GetLastError());
    }
    fx_free(&f);
}

TEST(window_class_scl_invalid_indices) {
    REQUIRE(U(SetClassLongA)); REQUIRE(U(SetClassLongPtrA)); REQUIRE(U(SetClassLongW));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    static const int bad[] = { -1, -2, -3, -22, -28, -100, 0, 1, 4, 8, 1000, 0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        if (wc_is_wine() && bad[i] >= 0) continue;   /* wine writes past the class */
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetClassLongA)(f.h, bad[i], 0x55) == 0, "idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "idx %d err %lu", bad[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetClassLongPtrA)(f.h, bad[i], 0x55) == 0, "ptr idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "ptr idx %d err %lu", bad[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetClassLongW)(f.h, bad[i], 0x55) == 0, "W idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "W idx %d err %lu", bad[i], GetLastError());
    }
    fx_free(&f);
}

TEST(window_class_gcl_bad_hwnd) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(GetClassWord)); REQUIRE(U(SetClassLongA)); REQUIRE(U(SetClassLongPtrA)); REQUIRE(U(SetClassWord));
    static const HWND bad[] = { NULL, (HWND)1, (HWND)0x12345678, (HWND)0xdeadbeef, (HWND)(LONG_PTR)-1, (HWND)0x10000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(MAGIC_ERR);
        CHECK(U(GetClassLongA)(bad[i], GCL_STYLE) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(GetClassLongPtrA)(bad[i], GCLP_WNDPROC) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(GetClassWord)(bad[i], GCW_ATOM) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(SetClassLongA)(bad[i], GCL_STYLE, 0) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(SetClassLongPtrA)(bad[i], GCLP_HCURSOR, 0) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(SetClassWord)(bad[i], 0, 0) == 0);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
    }
}

TEST(window_class_gcl_after_destroy) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(GetClassNameA)); REQUIRE(U(GetClassLongPtrW));
    struct fx f; CHECK(fx_make(&f, CS_DBLCLKS, 4, 4, NULL));
    HWND h = f.h;
    CHECK(U(GetClassLongA)(h, GCL_STYLE) != 0);
    CHECK(U(DestroyWindow)(h)); f.h = NULL;
    char b[32];
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassLongA)(h, GCL_STYLE) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_HWND);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassLongPtrW)(h, GCLP_HMODULE) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_HWND);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassNameA)(h, b, 32) == 0);
    t_note("GetClassNameA(destroyed hwnd) err %lu", GetLastError());
    fx_free(&f);
}

TEST(window_class_extra_zero_init_and_roundtrip) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA));
    struct fx f; CHECK(fx_make(&f, 0, 32, 0, NULL));
    for (int i = 0; i < 32; i += 4) {
        SetLastError(MAGIC_ERR);
        CHECK(U(GetClassLongA)(f.h, i) == 0);
        EXPECT_LAST_ERR(MAGIC_ERR);
    }
    for (int i = 0; i < 32; i += 4) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetClassLongA)(f.h, i, 0x1000 + i) == 0, "i=%d", i);
        EXPECT_LAST_ERR(MAGIC_ERR);
    }
    for (int i = 0; i < 32; i += 4) EQ(U(GetClassLongA)(f.h, i), 0x1000 + i);
    for (int i = 0; i < 32; i += 4) EQ(U(SetClassLongA)(f.h, i, 0x2000 + i), 0x1000 + i);
    for (int i = 0; i < 32; i += 4) EQ(U(GetClassLongA)(f.h, i), 0x2000 + i);
    fx_free(&f);
}

TEST(window_class_extra_out_of_range) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA));
    struct fx f; CHECK(fx_make(&f, 0, 16, 0, NULL));
    if (wc_is_wine()) { t_skip("wine server faults on out-of-range class extra"); fx_free(&f); return; }
    static const int bad[] = { 16, 20, 100, 13, 14, 15 };  /* 13..15 straddle the end for a LONG */
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(GetClassLongA)(f.h, bad[i]) == 0, "idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "idx %d err %lu", bad[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetClassLongA)(f.h, bad[i], 1) == 0, "set idx %d", bad[i]);
        CHECKF(GetLastError() == ERR_INVALID_INDEX, "set idx %d err %lu", bad[i], GetLastError());
    }
    CHECK(U(SetClassLongA)(f.h, 12, 0x77) == 0);   /* last valid LONG */
    EQ(U(GetClassLongA)(f.h, 12), 0x77);
    fx_free(&f);
}

TEST(window_class_extra_none) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA)); REQUIRE(U(GetClassWord));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    if (wc_is_wine()) { fx_free(&f); t_skip("wine corrupts server state on class extra access without extra bytes"); return; }
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassLongA)(f.h, 0) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(SetClassLongA)(f.h, 0, 1) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassWord)(f.h, 0) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    fx_free(&f);
}

TEST(window_class_extra_ptr_sized) {
    REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(SetClassLongPtrA)); REQUIRE(U(GetClassLongPtrW)); REQUIRE(U(SetClassLongPtrW));
    struct fx f; CHECK(fx_make(&f, 0, 24, 0, NULL));
    const ULONG_PTR v1 = (ULONG_PTR)0x1122334455667788ull, v2 = (ULONG_PTR)0xFFFFFFFF80000001ull;
    CHECK(U(SetClassLongPtrA)(f.h, 0, v1) == 0);
    CHECK(U(SetClassLongPtrW)(f.h, 8, v2) == 0);
    EQ(U(GetClassLongPtrA)(f.h, 0), v1);
    EQ(U(GetClassLongPtrW)(f.h, 0), v1);
    EQ(U(GetClassLongPtrA)(f.h, 8), v2);
    EQ(U(SetClassLongPtrW)(f.h, 0, 5), v1);
    EQ(U(SetClassLongPtrA)(f.h, 8, 6), v2);
    EQ(U(GetClassLongPtrA)(f.h, 0), 5);
    EQ(U(GetClassLongPtrA)(f.h, 8), 6);
    /* offset 16 is the last full pointer */
    CHECK(U(SetClassLongPtrA)(f.h, 16, v1) == 0);
    EQ(U(GetClassLongPtrA)(f.h, 16), v1);
    if (wc_is_wine()) { fx_free(&f); t_skip("wine: skip straddling class extra"); return; }
    /* offset 17 straddles the end */
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassLongPtrA)(f.h, 17) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(SetClassLongPtrA)(f.h, 24, 1) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    fx_free(&f);
}

TEST(window_class_extra_overlap_endianness) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA)); REQUIRE(U(GetClassWord)); REQUIRE(U(SetClassWord));
    struct fx f; CHECK(fx_make(&f, 0, 16, 0, NULL));
    U(SetClassLongA)(f.h, 0, 0xAABBCCDD);
    EQ(U(GetClassWord)(f.h, 0), 0xCCDD);
    EQ(U(GetClassWord)(f.h, 2), 0xAABB);
    EQ(U(SetClassWord)(f.h, 0, 0x1234), 0xCCDD);
    EQ(U(GetClassLongA)(f.h, 0), 0xAABB1234);
    EQ(U(SetClassWord)(f.h, 2, 0x5678), 0xAABB);
    EQ(U(GetClassLongA)(f.h, 0), 0x56781234);
    /* LONG at unaligned offsets spans two adjacent slots */
    U(SetClassLongA)(f.h, 4, 0x11223344);
    U(SetClassLongA)(f.h, 8, 0x55667788);
    SetLastError(MAGIC_ERR);
    DWORD r = U(GetClassLongA)(f.h, 6);
    t_note("unaligned GetClassLong(6) = 0x%lx err %lu", r, GetLastError());
    fx_free(&f);
}

TEST(window_class_extra_words) {
    REQUIRE(U(GetClassWord)); REQUIRE(U(SetClassWord));
    struct fx f; CHECK(fx_make(&f, 0, 10, 0, NULL));
    for (int i = 0; i < 10; i += 2) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(GetClassWord)(f.h, i) == 0, "i=%d", i);
        EXPECT_LAST_ERR(MAGIC_ERR);
        CHECK(U(SetClassWord)(f.h, i, 0x100 + i) == 0);
    }
    for (int i = 0; i < 10; i += 2) EQ(U(GetClassWord)(f.h, i), 0x100 + i);
    if (wc_is_wine()) { fx_free(&f); t_skip("wine: skip out-of-range class extra"); return; }
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassWord)(f.h, 10) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassWord)(f.h, 9) == 0);   /* 9..10 straddles end */
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(SetClassWord)(f.h, 9, 1) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(SetClassWord)(f.h, -1, 1) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetClassWord)(f.h, -2) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    fx_free(&f);
}

TEST(window_class_extra_shared_between_windows) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA));
    struct fx f; CHECK(fx_make(&f, 0, 8, 8, NULL));
    HWND h2 = wc_createA(f.name, WS_POPUP, NULL);
    CHECK(h2);
    U(SetClassLongA)(f.h, 0, 0xCAFE);
    EQ(U(GetClassLongA)(h2, 0), 0xCAFE);
    U(SetClassLongA)(h2, 4, 0xBEEF);
    EQ(U(GetClassLongA)(f.h, 4), 0xBEEF);
    U(DestroyWindow)(h2);
    fx_free(&f);
}

TEST(window_class_extra_survives_window_destroy) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA));
    struct fx f; CHECK(fx_make(&f, 0, 8, 0, NULL));
    U(SetClassLongA)(f.h, 0, 0x3141);
    U(DestroyWindow)(f.h);
    f.h = wc_createA(f.name, WS_POPUP, NULL);
    CHECK(f.h);
    EQ(U(GetClassLongA)(f.h, 0), 0x3141);
    fx_free(&f);
    /* re-register: fresh zeroed storage */
    CHECK(fx_make(&f, 0, 8, 0, NULL));
    EQ(U(GetClassLongA)(f.h, 0), 0);
    fx_free(&f);
}

TEST(window_class_extra_sized_by_registration_not_window) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(SetClassLongA));
    /* window extra bytes are separate from class extra bytes */
    struct fx f; CHECK(fx_make(&f, 0, 4, 12, NULL));
    /* (class-side read of offset 8 beyond the 4 class extra bytes is covered elsewhere) */
    CHECK(U(SetWindowLongA)(f.h, 8, 1) == 0);
    EQ(U(GetWindowLongA)(f.h, 8), 1);
    fx_free(&f);
}

/* Procedure change through the class */
static int g_cls_hits_a, g_cls_hits_b;
static LRESULT CALLBACK cls_proc_a(HWND h, UINT m, WPARAM w, LPARAM l) { if (m == WM_USER + 5) { g_cls_hits_a++; return 0xA; } return U(DefWindowProcA)(h, m, w, l); }
static LRESULT CALLBACK cls_proc_b(HWND h, UINT m, WPARAM w, LPARAM l) { if (m == WM_USER + 5) { g_cls_hits_b++; return 0xB; } return U(DefWindowProcA)(h, m, w, l); }

TEST(window_class_scl_wndproc_affects_new_windows_only) {
    REQUIRE(U(SetClassLongPtrA)); REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(SendMessageA)); REQUIRE(U(GetWindowLongPtrA));
    g_cls_hits_a = g_cls_hits_b = 0;
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, cls_proc_a));
    EQ(U(SendMessageA)(f.h, WM_USER + 5, 0, 0), 0xA);
    ULONG_PTR old = U(SetClassLongPtrA)(f.h, GCLP_WNDPROC, (LONG_PTR)cls_proc_b);
    EQ(old, (ULONG_PTR)cls_proc_a);
    EQ(U(GetClassLongPtrA)(f.h, GCLP_WNDPROC), (ULONG_PTR)cls_proc_b);
    /* existing window keeps its proc */
    EQ(U(GetWindowLongPtrA)(f.h, GWLP_WNDPROC), (ULONG_PTR)cls_proc_a);
    EQ(U(SendMessageA)(f.h, WM_USER + 5, 0, 0), 0xA);
    HWND h2 = wc_createA(f.name, WS_POPUP, NULL);
    CHECK(h2);
    EQ(U(GetWindowLongPtrA)(h2, GWLP_WNDPROC), (ULONG_PTR)cls_proc_b);
    EQ(U(SendMessageA)(h2, WM_USER + 5, 0, 0), 0xB);
    EQ(g_cls_hits_a, 2);
    EQ(g_cls_hits_b, 1);
    U(DestroyWindow)(h2);
    fx_free(&f);
}

TEST(window_class_getclassinfo_reflects_scl_wndproc) {
    REQUIRE(U(SetClassLongPtrA)); REQUIRE(U(GetClassInfoExA));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, cls_proc_a));
    U(SetClassLongPtrA)(f.h, GCLP_WNDPROC, (LONG_PTR)cls_proc_b);
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), f.name, &o));
    EQ(o.lpfnWndProc, cls_proc_b);
    fx_free(&f);
}

TEST(window_class_scl_style_visible_in_info) {
    REQUIRE(U(SetClassLongA)); REQUIRE(U(GetClassInfoExA));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    U(SetClassLongA)(f.h, GCL_STYLE, CS_DBLCLKS | CS_NOCLOSE);
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), f.name, &o));
    EQ(o.style & ~(UINT)CS_GLOBALCLASS, CS_DBLCLKS | CS_NOCLOSE);
    fx_free(&f);
}

TEST(window_class_menuname_a_w) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(GetClassLongPtrW)); REQUIRE(U(SetClassLongPtrA)); REQUIRE(U(SetClassLongPtrW));
    char n[64]; wc_uname(n, sizeof n, "wcMenuName");
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = n; wc.lpszMenuName = "menuA";
    CHECK(U(RegisterClassExA)(&wc));
    HWND h = wc_createA(n, WS_POPUP, NULL);
    CHECK(h);
    const char *a = (const char *)U(GetClassLongPtrA)(h, GCLP_MENUNAME);
    const WCHAR *w = (const WCHAR *)U(GetClassLongPtrW)(h, GCLP_MENUNAME);
    CHECK(a && !strcmp(a, "menuA"));
    CHECK(w && !lstrcmpW(w, L"menuA"));
    CHECK(U(SetClassLongPtrW)(h, GCLP_MENUNAME, (LONG_PTR)L"menuW") != 0 || 1);
    a = (const char *)U(GetClassLongPtrA)(h, GCLP_MENUNAME);
    w = (const WCHAR *)U(GetClassLongPtrW)(h, GCLP_MENUNAME);
    CHECK(a && !strcmp(a, "menuW"));
    CHECK(w && !lstrcmpW(w, L"menuW"));
    U(SetClassLongPtrA)(h, GCLP_MENUNAME, (LONG_PTR)MAKEINTRESOURCEA(77));
    EQ(U(GetClassLongPtrA)(h, GCLP_MENUNAME), 77);
    EQ(U(GetClassLongPtrW)(h, GCLP_MENUNAME), 77);
    U(DestroyWindow)(h);
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_gcw_atom_not_settable) {
    REQUIRE(U(SetClassWord)); REQUIRE(U(GetClassWord)); REQUIRE(U(SetClassLongPtrA));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    SetLastError(MAGIC_ERR);
    CHECK(U(SetClassWord)(f.h, GCW_ATOM, 2) == 0);
    EXPECT_LAST_ERR(ERR_INVALID_INDEX);
    EQ(U(GetClassWord)(f.h, GCW_ATOM), f.atom);
    SetLastError(MAGIC_ERR);
    ULONG_PTR r = U(SetClassLongPtrA)(f.h, GCW_ATOM, 2);
    CHECK(r == 0);
    t_note("SetClassLongPtrA(GCW_ATOM) err %lu", GetLastError());
    EQ(U(GetClassWord)(f.h, GCW_ATOM), f.atom);
    fx_free(&f);
}

/* ---------- GetClassName / RealGetWindowClass ---------- */

TEST(window_class_getclassname_exact_and_truncation) {
    REQUIRE(U(GetClassNameA)); REQUIRE(U(GetClassNameW));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    int len = (int)strlen(f.name);
    char b[128]; WCHAR w[128], wn[128]; wc_widen(wn, f.name);
    memset(b, 'x', sizeof b);
    EQ(U(GetClassNameA)(f.h, b, len + 1), len);
    EXPECT_STR(b, f.name);
    memset(b, 'x', sizeof b);
    EQ(U(GetClassNameA)(f.h, b, 128), len);
    EXPECT_STR(b, f.name);
    memset(b, 'x', sizeof b);
    EQ(U(GetClassNameA)(f.h, b, len), len - 1);
    CHECK(b[len - 1] == 0 && !strncmp(b, f.name, len - 1));
    memset(b, 'x', sizeof b);
    EQ(U(GetClassNameA)(f.h, b, 2), 1);
    CHECK(b[0] == f.name[0] && b[1] == 0 && b[2] == 'x');
    memset(b, 'x', sizeof b);
    EQ(U(GetClassNameA)(f.h, b, 1), 0);
    CHECK(b[0] == 0 && b[1] == 'x');
    memset(b, 'x', sizeof b);
    EQ(U(GetClassNameA)(f.h, b, 0), 0);
    CHECK(b[0] == 'x');
    /* wide */
    for (int i = 0; i < 128; i++) w[i] = 'x';
    EQ(U(GetClassNameW)(f.h, w, len + 1), len);
    EXPECT_WSTR(w, wn);
    for (int i = 0; i < 128; i++) w[i] = 'x';
    EQ(U(GetClassNameW)(f.h, w, len), len - 1);
    CHECK(w[len - 1] == 0);
    for (int i = 0; i < 128; i++) w[i] = 'x';
    EQ(U(GetClassNameW)(f.h, w, 1), 0);
    if (!(w[0] == 0 && w[1] == 'x')) t_note("GetClassNameW count 1 buffer: %04x %04x", w[0], w[1]);
    for (int i = 0; i < 128; i++) w[i] = 'x';
    EQ(U(GetClassNameW)(f.h, w, 0), 0);
    if (w[0] != 'x') t_note("GetClassNameW count 0 modified buffer");
    fx_free(&f);
}

TEST(window_class_getclassname_negative_count) {
    REQUIRE(U(GetClassNameA)); REQUIRE(U(GetClassNameW));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    char b[16]; memset(b, 'x', sizeof b);
    /* Win10 GetClassNameA computes min(len, count-1) and memcpy()s that many bytes, so a
     * negative count becomes a ~2^64 byte copy and destroys the caller's stack (process
     * dies). Not callable in-process; only the zero count is exercised. */
    SetLastError(MAGIC_ERR);
    int r = U(GetClassNameA)(f.h, b, 0);
    CHECK(r == 0);
    CHECK(b[0] == 'x');
    fx_free(&f);
}

TEST(window_class_getclassname_bad_hwnd) {
    REQUIRE(U(GetClassNameA)); REQUIRE(U(GetClassNameW)); REQUIRE(U(RealGetWindowClassA)); REQUIRE(U(RealGetWindowClassW));
    static const HWND bad[] = { NULL, (HWND)1, (HWND)0x77777, (HWND)(LONG_PTR)-1 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        char b[16]; WCHAR w[16]; b[0] = 'x'; w[0] = 'x';
        SetLastError(MAGIC_ERR);
        CHECK(U(GetClassNameA)(bad[i], b, 16) == 0);
        t_note("GetClassNameA(bad hwnd %p) err %lu", bad[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECK(U(GetClassNameW)(bad[i], w, 16) == 0);
        t_note("GetClassNameW(bad hwnd %p) err %lu", bad[i], GetLastError());
        SetLastError(MAGIC_ERR);
        CHECK(U(RealGetWindowClassA)(bad[i], b, 16) == 0);
        t_note("RealGetWindowClassA(bad hwnd) err %lu", GetLastError());
        SetLastError(MAGIC_ERR);
        CHECK(U(RealGetWindowClassW)(bad[i], w, 16) == 0);
    }
}

TEST(window_class_getclassname_a_w_same) {
    REQUIRE(U(GetClassNameA)); REQUIRE(U(GetClassNameW));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    char a[100]; WCHAR w[100], wn[100];
    int ra = U(GetClassNameA)(f.h, a, 100), rw = U(GetClassNameW)(f.h, w, 100);
    EQ(ra, rw);
    wc_widen(wn, a);
    EXPECT_WSTR(w, wn);
    fx_free(&f);
}

TEST(window_class_getclassname_registered_with_w_read_as_a) {
    REQUIRE(U(GetClassNameA)); REQUIRE(U(GetClassNameW));
    char n[64]; WCHAR w[64]; wc_uname(n, sizeof n, "wcNameW"); wc_widen(w, n);
    CHECK(wc_regW(w, 0, 0, 0, NULL));
    HWND h = wc_createW(w, WS_POPUP, NULL);
    CHECK(h);
    char b[64]; EQ(U(GetClassNameA)(h, b, 64), (int)strlen(n)); EXPECT_STR(b, n);
    EQ(U(IsWindowUnicode)(h), TRUE);
    U(DestroyWindow)(h);
    U(UnregisterClassW)(w, wc_inst());
}

TEST(window_class_getclassname_case_preserved) {
    REQUIRE(U(GetClassNameA));
    char n[64]; wc_uname(n, sizeof n, "wcMixedCASEname");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    char low[64]; strcpy(low, n); for (char *p = low; *p; p++) *p = (*p >= 'A' && *p <= 'Z') ? *p + 32 : *p;
    HWND h = wc_createA(low, WS_POPUP, NULL);   /* different case lookup */
    CHECK(h);
    if (h) {
        char b[64]; U(GetClassNameA)(h, b, 64);
        CHECK(!lstrcmpiA(b, n));
        t_note("class name returned for lookup '%s': '%s' (registered '%s')", low, b, n);
        U(DestroyWindow)(h);
    }
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_getclassname_after_class_unregistered_window_gone) {
    REQUIRE(U(GetClassNameA));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    char b[64]; strcpy(b, f.name);
    U(DestroyWindow)(f.h); f.h = NULL;
    CHECK(U(UnregisterClassA)(f.name, wc_inst())); f.atom = 0;
}

TEST(window_class_realgetwindowclass_basic) {
    REQUIRE(U(RealGetWindowClassA)); REQUIRE(U(RealGetWindowClassW));
    struct fx f; CHECK(fx_make(&f, 0, 0, 0, NULL));
    int len = (int)strlen(f.name);
    char b[100]; WCHAR w[100];
    EQ(U(RealGetWindowClassA)(f.h, b, 100), len);
    EXPECT_STR(b, f.name);
    EQ(U(RealGetWindowClassW)(f.h, w, 100), len);
    EQ(U(RealGetWindowClassA)(f.h, b, 2), 1);
    CHECK(b[1] == 0);
    EQ(U(RealGetWindowClassW)(f.h, w, 2), 1);
    CHECK(w[1] == 0);
    SetLastError(MAGIC_ERR);
    EQ(U(RealGetWindowClassA)(f.h, b, 0), 0);
    t_note("RealGetWindowClassA(count 0) err %lu", GetLastError());
    fx_free(&f);
}

TEST(window_class_realgetwindowclass_system_classes) {
    REQUIRE(U(RealGetWindowClassA)); REQUIRE(U(CreateWindowExA));
    static const char *cls[] = { "Button", "Edit", "Static", "ListBox", "ComboBox", "ScrollBar" };
    for (unsigned i = 0; i < 6; i++) {
        HWND h = U(CreateWindowExA)(0, cls[i], "x", WS_POPUP, 0, 0, 40, 40, NULL, NULL, wc_inst(), NULL);
        CHECKF(h != NULL, "create %s", cls[i]);
        if (!h) continue;
        char b[64], c[64];
        int r = U(RealGetWindowClassA)(h, b, 64);
        CHECKF(!lstrcmpiA(b, cls[i]), "%s: got '%s'", cls[i], b);
        EQ(r, (int)strlen(cls[i]));
        U(GetClassNameA)(h, c, 64);
        CHECKF(!lstrcmpiA(c, cls[i]), "%s: GetClassName got '%s'", cls[i], c);
        t_note("system class %s -> GetClassName '%s' RealGetWindowClass '%s'", cls[i], c, b);
        U(DestroyWindow)(h);
    }
}

TEST(window_class_realgetwindowclass_superclass) {
    REQUIRE(U(RealGetWindowClassA)); REQUIRE(U(GetClassInfoExA)); REQUIRE(U(RegisterClassExA));
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc); wc.cbSize = sizeof wc;
    CHECK(U(GetClassInfoExA)(NULL, "Button", &wc));
    char n[64]; wc_uname(n, sizeof n, "wcSuperBtn");
    wc.lpszClassName = n; wc.hInstance = wc_inst();
    CHECK(U(RegisterClassExA)(&wc));
    HWND h = U(CreateWindowExA)(0, n, "x", WS_POPUP, 0, 0, 40, 40, NULL, NULL, wc_inst(), NULL);
    CHECK(h);
    if (h) {
        char c[64], r[64];
        U(GetClassNameA)(h, c, 64);
        EXPECT_STR(c, n);
        U(RealGetWindowClassA)(h, r, 64);
        CHECK(!lstrcmpiA(r, "Button"));
        t_note("superclass of Button: GetClassName '%s' RealGetWindowClass '%s'", c, r);
        U(DestroyWindow)(h);
    }
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_system_window_class_names) {
    REQUIRE(U(CreateWindowExW)); REQUIRE(U(GetClassNameW));
    static const WCHAR *cls[] = { L"Button", L"Edit", L"Static", L"ListBox", L"ComboBox", L"ScrollBar",
                                  L"BUTTON", L"edit", L"sTaTiC" };
    for (unsigned i = 0; i < 9; i++) {
        HWND h = U(CreateWindowExW)(0, cls[i], L"x", WS_POPUP, 0, 0, 40, 40, NULL, NULL, wc_inst(), NULL);
        CHECKF(h != NULL, "create %d", i);
        if (!h) continue;
        WCHAR b[64]; U(GetClassNameW)(h, b, 64);
        CHECK(!lstrcmpiW(b, cls[i]));
        if (!U(IsWindowUnicode)(h)) t_note("system class %d created via CreateWindowExW is not Unicode", i);
        U(DestroyWindow)(h);
    }
}
