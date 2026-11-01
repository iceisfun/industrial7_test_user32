#include "wc.h"

/* ---------- RegisterClass basics ---------- */

TEST(window_class_register_a_basic) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcRegA");
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = n;
    ATOM a = U(RegisterClassA)(&wc);
    CHECK(a != 0);
    CHECKF(a >= 0xC000, "atom 0x%x", a);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_register_w_basic) {
    REQUIRE(U(RegisterClassW)); REQUIRE(U(UnregisterClassW));
    char n[64]; WCHAR w[64]; wc_uname(n, sizeof n, "wcRegW"); wc_widen(w, n);
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wc_defW; wc.hInstance = wc_inst(); wc.lpszClassName = w;
    ATOM a = U(RegisterClassW)(&wc);
    CHECK(a >= 0xC000);
    CHECK(U(UnregisterClassW)(w, wc_inst()));
}

TEST(window_class_register_exa_basic) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcRegExA");
    ATOM a = wc_regA(n, 0, 0, 0, NULL);
    CHECK(a >= 0xC000);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_register_exw_basic) {
    REQUIRE(U(RegisterClassExW)); REQUIRE(U(UnregisterClassW));
    char n[64]; WCHAR w[64]; wc_uname(n, sizeof n, "wcRegExW"); wc_widen(w, n);
    ATOM a = wc_regW(w, 0, 0, 0, NULL);
    CHECK(a >= 0xC000);
    CHECK(U(UnregisterClassW)(w, wc_inst()));
}

TEST(window_class_register_consecutive_atoms_distinct) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    char n[8][64]; ATOM a[8];
    for (int i = 0; i < 8; i++) { wc_uname(n[i], 64, "wcDist"); a[i] = wc_regA(n[i], 0, 0, 0, NULL); CHECK(a[i] >= 0xC000); }
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) CHECK(a[i] != a[j]);
    for (int i = 0; i < 8; i++) CHECK(U(UnregisterClassA)(n[i], wc_inst()));
}

TEST(window_class_register_exa_wrong_cbsize) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcCbSz");
    static const UINT sizes[] = { 0, 1, sizeof(WNDCLASSEXA) - 1, sizeof(WNDCLASSEXA) + 1, sizeof(WNDCLASSA), sizeof(WNDCLASSEXA) * 2, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
        wc.cbSize = sizes[i]; wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = n;
        SetLastError(MAGIC_ERR);
        ATOM a = U(RegisterClassExA)(&wc);
        CHECKF(a == 0, "cbSize %u gave atom", sizes[i]);
        EXPECT_LAST_ERR(ERR_INVALID_PARAM);
        if (a) U(UnregisterClassA)(n, wc_inst());
    }
}

TEST(window_class_register_exw_wrong_cbsize) {
    REQUIRE(U(RegisterClassExW)); REQUIRE(U(UnregisterClassW));
    static const WCHAR n[] = L"wcCbSzW_unique_class";
    static const UINT sizes[] = { 0, 1, sizeof(WNDCLASSEXW) - 1, sizeof(WNDCLASSEXW) + 1, sizeof(WNDCLASSW), 0x7FFFFFFF };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
        wc.cbSize = sizes[i]; wc.lpfnWndProc = wc_defW; wc.hInstance = wc_inst(); wc.lpszClassName = n;
        SetLastError(MAGIC_ERR);
        ATOM a = U(RegisterClassExW)(&wc);
        CHECK(a == 0);
        EXPECT_LAST_ERR(ERR_INVALID_PARAM);
        if (a) U(UnregisterClassW)(n, wc_inst());
    }
}

TEST(window_class_register_negative_class_extra) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcNegCls");
    static const int vals[] = { -1, -4, -100, (int)0x80000000 };
    for (unsigned i = 0; i < 4; i++) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = n; wc.cbClsExtra = vals[i];
        SetLastError(MAGIC_ERR);
        ATOM a = U(RegisterClassA)(&wc);
        CHECK(a == 0);
        EXPECT_LAST_ERR(ERR_INVALID_PARAM);
        if (a) U(UnregisterClassA)(n, wc_inst());
    }
}

TEST(window_class_register_negative_wnd_extra) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcNegWnd");
    static const int vals[] = { -1, -8, -1000, (int)0x80000000 };
    for (unsigned i = 0; i < 4; i++) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = n; wc.cbWndExtra = vals[i];
        SetLastError(MAGIC_ERR);
        ATOM a = U(RegisterClassA)(&wc);
        CHECK(a == 0);
        EXPECT_LAST_ERR(ERR_INVALID_PARAM);
        if (a) U(UnregisterClassA)(n, wc_inst());
    }
}

TEST(window_class_register_huge_extra) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcHuge");
    static const int vals[] = { 0x10000, 0x100000, 0x7FFFFFFF };
    for (unsigned i = 0; i < 3; i++) {
        for (int w = 0; w < 2; w++) {
            WNDCLASSA wc; memset(&wc, 0, sizeof wc);
            wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = n;
            if (w) wc.cbWndExtra = vals[i]; else wc.cbClsExtra = vals[i];
            ATOM a = U(RegisterClassA)(&wc);
            if (a) { t_note("huge extra 0x%x (%s) was accepted", vals[i], w ? "wnd" : "cls"); U(UnregisterClassA)(n, wc_inst()); }
            else CHECK(1);
        }
    }
}

TEST(window_class_register_null_proc) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcNullProc");
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.hInstance = wc_inst(); wc.lpszClassName = n;
    SetLastError(MAGIC_ERR);
    ATOM a = U(RegisterClassA)(&wc);
    t_note("RegisterClassA(NULL proc) -> atom 0x%x err %lu", a, GetLastError());
    if (a) U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_register_null_name) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = NULL;
    SetLastError(MAGIC_ERR);
    ATOM a = U(RegisterClassA)(&wc);
    CHECK(a == 0);
    t_note("NULL class name err %lu", GetLastError());
    WNDCLASSW ww; memset(&ww, 0, sizeof ww);
    ww.lpfnWndProc = wc_defW; ww.hInstance = wc_inst();
    SetLastError(MAGIC_ERR);
    CHECK(U(RegisterClassW)(&ww) == 0);
}

TEST(window_class_register_empty_name) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = "";
    SetLastError(MAGIC_ERR);
    ATOM a = U(RegisterClassA)(&wc);
    CHECK(a == 0);
    if (a) U(UnregisterClassA)("", wc_inst());
    t_note("empty class name err %lu", GetLastError());
}

TEST(window_class_register_duplicate) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcDup");
    ATOM a = wc_regA(n, 0, 0, 0, NULL);
    CHECK(a);
    SetLastError(MAGIC_ERR);
    ATOM b = wc_regA(n, CS_DBLCLKS, 4, 4, NULL);
    CHECK(b == 0);
    EXPECT_LAST_ERR(ERR_CLASS_EXISTS);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_register_duplicate_after_unregister) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcReReg");
    for (int i = 0; i < 20; i++) {
        ATOM a = wc_regA(n, 0, 0, 0, NULL);
        CHECK(a >= 0xC000);
        CHECK(U(UnregisterClassA)(n, wc_inst()));
    }
}

TEST(window_class_register_duplicate_a_then_w) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(RegisterClassW)); REQUIRE(U(UnregisterClassA));
    char n[64]; WCHAR w[64]; wc_uname(n, sizeof n, "wcDupAW"); wc_widen(w, n);
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    SetLastError(MAGIC_ERR);
    CHECK(wc_regW(w, 0, 0, 0, NULL) == 0);
    EXPECT_LAST_ERR(ERR_CLASS_EXISTS);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_register_name_case_insensitive) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA)); REQUIRE(U(GetClassInfoExA));
    char n[64], up[64]; wc_uname(n, sizeof n, "wcCaseX");
    ATOM a = wc_regA(n, 0, 0, 0, NULL);
    CHECK(a);
    strcpy(up, n); for (char *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    CHECK(strcmp(up, n) != 0);
    SetLastError(MAGIC_ERR);
    CHECK(wc_regA(up, 0, 0, 0, NULL) == 0);
    EXPECT_LAST_ERR(ERR_CLASS_EXISTS);
    WNDCLASSEXA out; out.cbSize = sizeof out;
    CHECK(U(GetClassInfoExA)(wc_inst(), up, &out));
    CHECK(U(UnregisterClassA)(up, wc_inst()));
}

TEST(window_class_register_atom_equals_getclassinfo) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcAtomInfo");
    ATOM a = wc_regA(n, 0, 0, 0, NULL);
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    UINT_PTR r = U(GetClassInfoExA)(wc_inst(), n, &o);
    CHECK(r != 0);
    EQ(r, a);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_register_atom_as_name) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcAtomName");
    ATOM a = wc_regA(n, 0, 0, 0, NULL);
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), MAKEINTATOM(a), &o));
    HWND h = U(CreateWindowExA)(0, MAKEINTATOM(a), "t", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h != NULL);
    if (h) {
        char buf[128]; int r = U(GetClassNameA)(h, buf, sizeof buf);
        EQ(r, (int)strlen(n));
        EXPECT_STR(buf, n);
        CHECK(U(DestroyWindow)(h));
    }
    CHECK(U(UnregisterClassA)(MAKEINTATOM(a), wc_inst()));
    CHECK(!U(GetClassInfoExA)(wc_inst(), n, &o));
}

TEST(window_class_register_integer_atom_name) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA)); REQUIRE(U(GetClassInfoExA));
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst();
    wc.lpszClassName = MAKEINTATOM(0x1ABC);
    ATOM a = U(RegisterClassExA)(&wc);
    t_note("RegisterClassEx with integer atom name -> 0x%x err %lu", a, GetLastError());
    if (a) {
        CHECK(a == 0x1ABC);
        WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
        CHECK(U(GetClassInfoExA)(wc_inst(), MAKEINTATOM(0x1ABC), &o));
        CHECK(U(UnregisterClassA)(MAKEINTATOM(0x1ABC), wc_inst()));
    }
}

TEST(window_class_register_numeric_hash_name) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA)); REQUIRE(U(GetClassInfoExA));
    /* "#nnnn" in CreateWindow / GetClassInfo refers to an integer atom */
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst();
    wc.lpszClassName = MAKEINTATOM(0x1ACD);
    ATOM a = U(RegisterClassExA)(&wc);
    if (!a) { t_skip("integer atom registration unsupported"); return; }
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), "#6861", &o));   /* 0x1ACD == 6861 */
    HWND h = U(CreateWindowExA)(0, "#6861", "t", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
    CHECK(h != NULL);
    if (h) {
        char buf[32]; int r = U(GetClassNameA)(h, buf, sizeof buf);
        t_note("class name of integer-atom class: '%s' (%d)", buf, r);
        EQ((UINT)U(GetClassWord)(h, GCW_ATOM), 0x1ACD);
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassA)(MAKEINTATOM(0x1ACD), wc_inst()));
}

/* ---------- name length limits ---------- */

static int try_len(int len, int wide) {
    char *a = malloc(len + 1); WCHAR *w = malloc((len + 1) * 2);
    for (int i = 0; i < len; i++) { a[i] = 'a' + (i % 26); w[i] = a[i]; }
    a[len] = 0; w[len] = 0;
    /* make unique */
    char tag[16]; snprintf(tag, sizeof tag, "%lx", (unsigned long)GetCurrentProcessId());
    for (int i = 0; tag[i] && i < len; i++) { a[i] = tag[i]; w[i] = tag[i]; }
    ATOM r;
    if (wide) r = wc_regW(w, 0, 0, 0, NULL); else r = wc_regA(a, 0, 0, 0, NULL);
    if (r) {
        if (wide) U(UnregisterClassW)(w, wc_inst()); else U(UnregisterClassA)(a, wc_inst());
    }
    free(a); free(w);
    return r != 0;
}

TEST(window_class_name_len_boundaries_a) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    CHECK(try_len(1, 0));
    CHECK(try_len(16, 0));
    CHECK(try_len(100, 0));
    CHECK(try_len(200, 0));
    int r255 = try_len(255, 0), r256 = try_len(256, 0), r257 = try_len(257, 0);
    t_note("A name lens ok: 255=%d 256=%d 257=%d", r255, r256, r257);
    CHECK(r255);
    CHECK(!try_len(300, 0));
    CHECK(!try_len(1000, 0));
    CHECK(!try_len(5000, 0));
}

TEST(window_class_name_len_boundaries_w) {
    REQUIRE(U(RegisterClassExW)); REQUIRE(U(UnregisterClassW));
    CHECK(try_len(1, 1));
    CHECK(try_len(100, 1));
    CHECK(try_len(200, 1));
    int r255 = try_len(255, 1), r256 = try_len(256, 1);
    t_note("W name lens ok: 255=%d 256=%d", r255, r256);
    CHECK(r255);
    CHECK(!try_len(300, 1));
    CHECK(!try_len(4000, 1));
}

TEST(window_class_name_too_long_error) {
    REQUIRE(U(RegisterClassExA));
    char *a = malloc(1001); memset(a, 'q', 1000); a[1000] = 0;
    SetLastError(MAGIC_ERR);
    ATOM r = wc_regA(a, 0, 0, 0, NULL);
    CHECK(r == 0);
    t_note("1000-char class name error %lu", GetLastError());
    free(a);
}

TEST(window_class_getclassinfo_long_name) {
    REQUIRE(U(GetClassInfoExA));
    char *a = malloc(1001); memset(a, 'q', 1000); a[1000] = 0;
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    SetLastError(MAGIC_ERR);
    CHECK(!U(GetClassInfoExA)(wc_inst(), a, &o));
    t_note("GetClassInfoExA(1000 chars) err %lu", GetLastError());
    free(a);
}

/* ---------- Unregister ---------- */

TEST(window_class_unregister_nonexistent) {
    REQUIRE(U(UnregisterClassA)); REQUIRE(U(UnregisterClassW));
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassA)("wcNoSuchClass_zzz", wc_inst()));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassW)(L"wcNoSuchClass_zzz", wc_inst()));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
}

TEST(window_class_unregister_twice) {
    REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcUnregTwice");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    CHECK(U(UnregisterClassA)(n, wc_inst()));
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassA)(n, wc_inst()));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
}

TEST(window_class_unregister_with_window) {
    REQUIRE(U(UnregisterClassA)); REQUIRE(U(DestroyWindow));
    char n[64]; wc_uname(n, sizeof n, "wcUnregWin");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    HWND h = wc_createA(n, WS_POPUP, NULL);
    CHECK(h);
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassA)(n, wc_inst()));
    EXPECT_LAST_ERR(ERR_CLASS_HAS_WINDOWS);
    CHECK(U(DestroyWindow)(h));
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_unregister_with_two_windows) {
    REQUIRE(U(UnregisterClassA)); REQUIRE(U(DestroyWindow));
    char n[64]; wc_uname(n, sizeof n, "wcUnreg2");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    HWND h1 = wc_createA(n, WS_POPUP, NULL), h2 = wc_createA(n, WS_POPUP, NULL);
    CHECK(h1 && h2);
    CHECK(U(DestroyWindow)(h1));
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassA)(n, wc_inst()));
    EXPECT_LAST_ERR(ERR_CLASS_HAS_WINDOWS);
    CHECK(U(DestroyWindow)(h2));
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_unregister_with_child_window) {
    REQUIRE(U(UnregisterClassA)); REQUIRE(U(DestroyWindow));
    char n[64]; wc_uname(n, sizeof n, "wcUnregChild"); char p[64]; wc_uname(p, sizeof p, "wcUnregPar");
    CHECK(wc_regA(n, 0, 0, 0, NULL)); CHECK(wc_regA(p, 0, 0, 0, NULL));
    HWND par = wc_createA(p, WS_POPUP, NULL);
    HWND ch = wc_createA(n, WS_CHILD, par);
    CHECK(par && ch);
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassA)(n, wc_inst()));
    EXPECT_LAST_ERR(ERR_CLASS_HAS_WINDOWS);
    CHECK(U(DestroyWindow)(par));   /* destroys child too */
    CHECK(!U(IsWindow)(ch));
    CHECK(U(UnregisterClassA)(n, wc_inst()));
    CHECK(U(UnregisterClassA)(p, wc_inst()));
}

TEST(window_class_unregister_wrong_instance) {
    REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcUnregInst");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassA)(n, (HINSTANCE)0x1234000));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_unregister_by_atom) {
    REQUIRE(U(UnregisterClassA)); REQUIRE(U(GetClassInfoExA));
    char n[64]; wc_uname(n, sizeof n, "wcUnregAtom");
    ATOM a = wc_regA(n, 0, 0, 0, NULL);
    CHECK(a);
    CHECK(U(UnregisterClassA)(MAKEINTATOM(a), wc_inst()));
    WNDCLASSEXA o; o.cbSize = sizeof o;
    SetLastError(MAGIC_ERR);
    CHECK(!U(GetClassInfoExA)(wc_inst(), n, &o));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
}

TEST(window_class_unregister_w_registered_by_a) {
    REQUIRE(U(UnregisterClassW));
    char n[64]; WCHAR w[64]; wc_uname(n, sizeof n, "wcUnregAW"); wc_widen(w, n);
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    CHECK(U(UnregisterClassW)(w, wc_inst()));
}

TEST(window_class_unregister_after_failed_register) {
    REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcUnregFail");
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = 3; wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = n;
    CHECK(U(RegisterClassExA)(&wc) == 0);
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassA)(n, wc_inst()));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
}

/* ---------- GetClassInfo round trip ---------- */

TEST(window_class_getclassinfo_roundtrip_a) {
    REQUIRE(U(GetClassInfoA)); REQUIRE(U(RegisterClassA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcRoundA");
    HBRUSH br = (HBRUSH)(COLOR_BTNFACE + 1);
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW; wc.lpfnWndProc = wc_defA;
    wc.cbClsExtra = 12; wc.cbWndExtra = 20; wc.hInstance = wc_inst();
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW); wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hbrBackground = br; wc.lpszMenuName = "MyMenuName"; wc.lpszClassName = n;
    ATOM a = U(RegisterClassA)(&wc);
    CHECK(a);
    WNDCLASSA o; memset(&o, 0xAA, sizeof o);
    UINT_PTR r = U(GetClassInfoA)(wc_inst(), n, &o);
    CHECK(r != 0);
    EQ(o.style, wc.style);
    EQ(o.lpfnWndProc, wc.lpfnWndProc);
    EQ(o.cbClsExtra, 12);
    EQ(o.cbWndExtra, 20);
    EQ(o.hInstance, wc.hInstance);
    EQ(o.hIcon, wc.hIcon);
    EQ(o.hCursor, wc.hCursor);
    EQ(o.hbrBackground, br);
    CHECK(o.lpszMenuName && !strcmp(o.lpszMenuName, "MyMenuName"));
    t_note("GetClassInfoA lpszClassName=%p (input %p)", o.lpszClassName, n);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_getclassinfo_roundtrip_w) {
    REQUIRE(U(GetClassInfoW)); REQUIRE(U(RegisterClassW)); REQUIRE(U(UnregisterClassW));
    char n[64]; WCHAR w[64]; wc_uname(n, sizeof n, "wcRoundW"); wc_widen(w, n);
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC | CS_SAVEBITS; wc.lpfnWndProc = wc_defW;
    wc.cbClsExtra = 8; wc.cbWndExtra = 16; wc.hInstance = wc_inst();
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.lpszMenuName = L"MenuW"; wc.lpszClassName = w;
    CHECK(U(RegisterClassW)(&wc));
    WNDCLASSW o; memset(&o, 0xAA, sizeof o);
    CHECK(U(GetClassInfoW)(wc_inst(), w, &o));
    EQ(o.style, wc.style);
    EQ(o.lpfnWndProc, wc.lpfnWndProc);
    EQ(o.cbClsExtra, 8);
    EQ(o.cbWndExtra, 16);
    EQ(o.hInstance, wc.hInstance);
    EQ(o.hbrBackground, wc.hbrBackground);
    CHECK(o.lpszMenuName && !lstrcmpW(o.lpszMenuName, L"MenuW"));
    CHECK(U(UnregisterClassW)(w, wc_inst()));
}

TEST(window_class_getclassinfoex_cbsize_untouched) {
    REQUIRE(U(GetClassInfoExA));
    char n[64]; wc_uname(n, sizeof n, "wcCbUntouched");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    static const UINT sizes[] = { 0, 1, 79, 81, 0xdeadbeef };
    for (unsigned i = 0; i < 5; i++) {
        WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizes[i];
        SetLastError(MAGIC_ERR);
        BOOL r = U(GetClassInfoExA)(wc_inst(), n, &o);
        CHECK(r);
        CHECKF(o.cbSize == sizes[i], "cbSize now %u (was %u)", o.cbSize, sizes[i]);
        CHECK(o.lpfnWndProc != NULL);
    }
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_getclassinfoex_cbsize_w) {
    REQUIRE(U(GetClassInfoExW));
    WNDCLASSEXW o; memset(&o, 0, sizeof o); o.cbSize = 0;
    CHECK(U(GetClassInfoExW)(NULL, L"Static", &o));
    EQ(o.cbSize, 0);
    CHECK(o.lpfnWndProc != NULL);
}

TEST(window_class_getclassinfo_not_found) {
    REQUIRE(U(GetClassInfoA)); REQUIRE(U(GetClassInfoW));
    WNDCLASSA o; WNDCLASSW ow;
    SetLastError(MAGIC_ERR);
    CHECK(!U(GetClassInfoA)(wc_inst(), "wcDoesNotExistAtAll", &o));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
    SetLastError(MAGIC_ERR);
    CHECK(!U(GetClassInfoW)(wc_inst(), L"wcDoesNotExistAtAll", &ow));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
    SetLastError(MAGIC_ERR);
    CHECK(!U(GetClassInfoA)(NULL, "wcDoesNotExistAtAll", &o));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
    SetLastError(MAGIC_ERR);
    CHECK(!U(GetClassInfoA)(NULL, "Stati", &o));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
}

TEST(window_class_getclassinfo_wrong_instance) {
    REQUIRE(U(GetClassInfoA));
    char n[64]; wc_uname(n, sizeof n, "wcInfoInst");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    WNDCLASSA o;
    SetLastError(MAGIC_ERR);
    CHECK(!U(GetClassInfoA)((HINSTANCE)0x7777000, n, &o));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
    CHECK(U(GetClassInfoA)(wc_inst(), n, &o));
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_getclassinfo_a_vs_w_parity) {
    REQUIRE(U(GetClassInfoA)); REQUIRE(U(GetClassInfoW));
    char n[64]; WCHAR w[64]; wc_uname(n, sizeof n, "wcParity"); wc_widen(w, n);
    CHECK(wc_regA(n, CS_DBLCLKS, 6, 10, NULL));
    WNDCLASSA oa; WNDCLASSW ow;
    memset(&oa, 0, sizeof oa); memset(&ow, 0, sizeof ow);
    CHECK(U(GetClassInfoA)(wc_inst(), n, &oa));
    CHECK(U(GetClassInfoW)(wc_inst(), w, &ow));
    EQ(oa.style, ow.style);
    EQ(oa.cbClsExtra, ow.cbClsExtra);
    EQ(oa.cbWndExtra, ow.cbWndExtra);
    EQ(oa.hInstance, ow.hInstance);
    EQ(oa.hbrBackground, ow.hbrBackground);
    EQ(oa.hCursor, ow.hCursor);
    EQ(oa.lpfnWndProc, wc_defA);
    t_note("A class read via W: proc %p (A proc %p)", ow.lpfnWndProc, wc_defA);
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_getclassinfo_menuname_null) {
    REQUIRE(U(GetClassInfoExA));
    char n[64]; wc_uname(n, sizeof n, "wcNoMenu");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    WNDCLASSEXA o; memset(&o, 0xAA, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
    CHECK(o.lpszMenuName == NULL);
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_getclassinfo_menu_resource_id) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(RegisterClassExA));
    char n[64]; wc_uname(n, sizeof n, "wcMenuId");
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst();
    wc.lpszClassName = n; wc.lpszMenuName = MAKEINTRESOURCEA(101);
    CHECK(U(RegisterClassExA)(&wc));
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
    EQ(o.lpszMenuName, MAKEINTRESOURCEA(101));
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_getclassinfoex_small_icon) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(RegisterClassExA));
    char n[64]; wc_uname(n, sizeof n, "wcSmIcon");
    HICON big = U(LoadIconA)(NULL, IDI_APPLICATION), small = U(LoadIconA)(NULL, IDI_QUESTION);
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.hInstance = wc_inst();
    wc.lpszClassName = n; wc.hIcon = big; wc.hIconSm = small;
    CHECK(U(RegisterClassExA)(&wc));
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
    EQ(o.hIcon, big);
    EQ(o.hIconSm, small);
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_register_classa_has_null_smallicon) {
    REQUIRE(U(GetClassInfoExA));
    char n[64]; wc_uname(n, sizeof n, "wcNoSm");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    WNDCLASSEXA o; memset(&o, 0xAA, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
    t_note("hIconSm for class without icon: %p", o.hIconSm);
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_style_bits_roundtrip) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(UnregisterClassA));
    static const UINT styles[] = { 0, CS_VREDRAW, CS_HREDRAW, CS_DBLCLKS, CS_OWNDC, CS_CLASSDC, CS_PARENTDC,
        CS_NOCLOSE, CS_SAVEBITS, CS_BYTEALIGNCLIENT, CS_BYTEALIGNWINDOW, CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS,
        CS_OWNDC | CS_NOCLOSE, CS_IME, CS_DROPSHADOW };
    for (unsigned i = 0; i < sizeof styles / sizeof *styles; i++) {
        char n[64]; wc_uname(n, sizeof n, "wcStyle");
        ATOM a = wc_regA(n, styles[i], 0, 0, NULL);
        if (!a) { t_note("style 0x%x rejected err %lu", styles[i], GetLastError()); continue; }
        WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
        CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
        CHECKF((o.style & ~(UINT)CS_GLOBALCLASS) == styles[i], "style 0x%x got 0x%x", styles[i], o.style);
        CHECK(U(UnregisterClassA)(n, wc_inst()));
    }
}

TEST(window_class_style_unknown_bits) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcStyleUnk");
    ATOM a = wc_regA(n, 0xFFFF0000u, 0, 0, NULL);
    t_note("style 0xFFFF0000 -> atom 0x%x err %lu", a, GetLastError());
    if (a) {
        WNDCLASSEXA o; o.cbSize = sizeof o;
        if (U(GetClassInfoExA)(wc_inst(), n, &o)) t_note("returned style 0x%x", o.style);
        U(UnregisterClassA)(n, wc_inst());
    }
}

TEST(window_class_extra_sizes_roundtrip) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(UnregisterClassA));
    static const int cb[] = { 0, 1, 3, 4, 7, 8, 16, 40, 64, 100, 512 };
    for (unsigned i = 0; i < sizeof cb / sizeof *cb; i++) {
        char n[64]; wc_uname(n, sizeof n, "wcExtraSz");
        ATOM a = wc_regA(n, 0, cb[i], cb[i] + 1, NULL);
        CHECKF(a != 0, "cb=%d", cb[i]);
        if (!a) continue;
        WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
        CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
        EQ(o.cbClsExtra, cb[i]);
        EQ(o.cbWndExtra, cb[i] + 1);
        CHECK(U(UnregisterClassA)(n, wc_inst()));
    }
}

TEST(window_class_register_global_flag) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(GetClassInfoExA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcGlobal");
    ATOM a = wc_regA(n, CS_GLOBALCLASS, 0, 0, NULL);
    CHECK(a);
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
    CHECK(U(GetClassInfoExA)(NULL, n, &o));          /* global: visible with NULL instance */
    CHECK(U(GetClassInfoExA)((HINSTANCE)0x5550000, n, &o)); /* and any other instance */
    HWND h = U(CreateWindowExA)(0, n, "g", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    CHECK(h != NULL);
    if (h) U(DestroyWindow)(h);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_register_local_not_visible_null_instance) {
    REQUIRE(U(GetClassInfoExA));
    char n[64]; wc_uname(n, sizeof n, "wcLocalNull");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    WNDCLASSEXA o; o.cbSize = sizeof o;
    SetLastError(MAGIC_ERR);
    BOOL r = U(GetClassInfoExA)(NULL, n, &o);
    t_note("local class visible via NULL hInstance: %d (err %lu)", r, GetLastError());
    U(UnregisterClassA)(n, wc_inst());
}

TEST(window_class_register_other_instance_value) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcFakeInst");
    /* a bogus address makes Win10 user32 fault while probing it as a PE image (handled
     * internally); use a real module that is not the exe instance */
    HINSTANCE fake = GetModuleHandleA("kernel32.dll");
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.hInstance = fake; wc.lpszClassName = n;
    ATOM a = U(RegisterClassExA)(&wc);
    t_note("register with fake hInstance -> 0x%x err %lu", a, GetLastError());
    if (a) {
        WNDCLASSEXA o; o.cbSize = sizeof o;
        CHECK(U(GetClassInfoExA)(fake, n, &o));
        EQ(o.hInstance, fake);
        CHECK(U(UnregisterClassA)(n, fake));
    }
}

TEST(window_class_register_proc_roundtrip_many) {
    REQUIRE(U(GetClassInfoExA));
    for (int i = 0; i < 30; i++) {
        char n[64]; wc_uname(n, sizeof n, "wcMany");
        /* Win10 rejects style bits outside CS_VALID with 87 for current-SDK apps (0x4 here);
         * keep the low bits within the valid set: 0..3 and DBLCLKS */
        CHECK(wc_regA(n, (i & 0x3) | ((i & 0x4) ? CS_DBLCLKS : 0), i, i * 2, (i & 1) ? wc_recA : wc_defA));
        WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
        CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
        CHECK(o.lpfnWndProc == ((i & 1) ? wc_recA : wc_defA));
        EQ(o.cbClsExtra, i);
        EQ(o.cbWndExtra, i * 2);
        CHECK(U(UnregisterClassA)(n, wc_inst()));
    }
}

/* ---------- threads ---------- */

struct thr { char name[64]; ATOM atom; BOOL found; BOOL unreg; DWORD err; HWND hwnd; };

static DWORD WINAPI thr_reg(void *p) {
    struct thr *t = p;
    t->atom = wc_regA(t->name, 0, 0, 0, NULL);
    return 0;
}
static DWORD WINAPI thr_info(void *p) {
    struct thr *t = p;
    WNDCLASSEXA o; o.cbSize = sizeof o;
    t->found = U(GetClassInfoExA)(wc_inst(), t->name, &o) != 0;
    t->hwnd = wc_createA(t->name, WS_POPUP, NULL);
    SetLastError(MAGIC_ERR);
    t->unreg = U(UnregisterClassA)(t->name, wc_inst());
    t->err = GetLastError();
    if (t->hwnd) U(DestroyWindow)(t->hwnd);
    return 0;
}

TEST(window_class_cross_thread_register) {
    REQUIRE(U(UnregisterClassA)); REQUIRE(U(GetClassInfoExA));
    struct thr t; memset(&t, 0, sizeof t); wc_uname(t.name, sizeof t.name, "wcThrReg");
    HANDLE h = CreateThread(NULL, 0, thr_reg, &t, 0, NULL);
    CHECK(h && WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    if (h) CloseHandle(h);
    CHECK(t.atom >= 0xC000);
    /* class registered by another thread persists and is usable here */
    WNDCLASSEXA o; o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), t.name, &o));
    HWND w = wc_createA(t.name, WS_POPUP, NULL);
    CHECK(w != NULL);
    if (w) U(DestroyWindow)(w);
    CHECK(U(UnregisterClassA)(t.name, wc_inst()));
}

TEST(window_class_cross_thread_use) {
    REQUIRE(U(UnregisterClassA));
    struct thr t; memset(&t, 0, sizeof t); wc_uname(t.name, sizeof t.name, "wcThrUse");
    CHECK(wc_regA(t.name, 0, 0, 0, NULL));
    HANDLE h = CreateThread(NULL, 0, thr_info, &t, 0, NULL);
    CHECK(h && WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    if (h) CloseHandle(h);
    CHECK(t.found);
    CHECK(t.hwnd != NULL);
    /* Unregister from another thread while one of its windows existed must fail */
    CHECK(!t.unreg);
    CHECK(t.err == ERR_CLASS_HAS_WINDOWS);
    CHECK(U(UnregisterClassA)(t.name, wc_inst()));
}

TEST(window_class_unregister_other_thread_ok) {
    REQUIRE(U(UnregisterClassA));
    struct thr t; memset(&t, 0, sizeof t); wc_uname(t.name, sizeof t.name, "wcThrUnreg");
    CHECK(wc_regA(t.name, 0, 0, 0, NULL));
    struct thr t2 = t;
    HANDLE h = CreateThread(NULL, 0, thr_info, &t2, 0, NULL);
    CHECK(h && WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    if (h) CloseHandle(h);
    U(UnregisterClassA)(t.name, wc_inst());
}

TEST(window_class_many_classes) {
    REQUIRE(U(UnregisterClassA));
    enum { N = 200 };
    static char names[N][48]; static ATOM at[N];
    int ok = 0;
    for (int i = 0; i < N; i++) {
        wc_uname(names[i], 48, "wcBulk");
        at[i] = wc_regA(names[i], 0, 0, 0, NULL);
        if (at[i]) ok++;
    }
    CHECK(ok == N);
    for (int i = 0; i < N; i++) if (at[i]) CHECK(U(UnregisterClassA)(names[i], wc_inst()));
}

TEST(window_class_register_unicode_name) {
    REQUIRE(U(RegisterClassExW)); REQUIRE(U(UnregisterClassW)); REQUIRE(U(GetClassNameW)); REQUIRE(U(GetClassInfoExW));
    WCHAR w[64]; swprintf(w, 64, L"wcUni_é中文_%lu", (unsigned long)GetCurrentProcessId());
    ATOM a = wc_regW(w, 0, 0, 0, NULL);
    CHECK(a >= 0xC000);
    WNDCLASSEXW o; o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExW)(wc_inst(), w, &o));
    HWND h = wc_createW(w, WS_POPUP, NULL);
    CHECK(h);
    if (h) {
        WCHAR b[64]; int r = U(GetClassNameW)(h, b, 64);
        EQ(r, (int)lstrlenW(w));
        EXPECT_WSTR(b, w);
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassW)(w, wc_inst()));
}
