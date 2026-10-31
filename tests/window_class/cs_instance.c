#include "wc.h"

/* ---------- class style behaviours ---------- */

TEST(window_class_cs_owndc_stable_dc) {
    REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    char n[64]; wc_uname(n, sizeof n, "wcOwnDC");
    CHECK(wc_regA(n, CS_OWNDC, 0, 0, NULL));
    HWND h1 = wc_createA(n, WS_POPUP, NULL), h2 = wc_createA(n, WS_POPUP, NULL);
    CHECK(h1 && h2);
    HDC a = U(GetDC)(h1), b = U(GetDC)(h1), c = U(GetDC)(h2);
    CHECK(a && b && c);
    CHECK(a == b);
    CHECK(a != c);
    U(ReleaseDC)(h1, a); U(ReleaseDC)(h1, b);
    HDC d = U(GetDC)(h1);
    CHECK(d == a);          /* the DC persists for the window's life */
    U(ReleaseDC)(h1, d); U(ReleaseDC)(h2, c);
    U(DestroyWindow)(h1); U(DestroyWindow)(h2);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_cs_classdc_shared) {
    REQUIRE(U(GetDC)); REQUIRE(U(ReleaseDC));
    char n[64]; wc_uname(n, sizeof n, "wcClsDC");
    CHECK(wc_regA(n, CS_CLASSDC, 0, 0, NULL));
    HWND h1 = wc_createA(n, WS_POPUP, NULL), h2 = wc_createA(n, WS_POPUP, NULL);
    CHECK(h1 && h2);
    HDC a = U(GetDC)(h1);
    CHECK(a);
    U(ReleaseDC)(h1, a);
    HDC b = U(GetDC)(h2);
    CHECK(b);
    CHECK(a == b);
    U(ReleaseDC)(h2, b);
    U(DestroyWindow)(h1); U(DestroyWindow)(h2);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_cs_noclose_disables_close) {
    REQUIRE(U(GetSystemMenu)); REQUIRE(U(GetMenuState)); REQUIRE(U(CreateWindowExA));
    char n[64], m[64]; wc_uname(n, sizeof n, "wcNoClose"); wc_uname(m, sizeof m, "wcWithClose");
    CHECK(wc_regA(n, CS_NOCLOSE, 0, 0, NULL));
    CHECK(wc_regA(m, 0, 0, 0, NULL));
    HWND h1 = U(CreateWindowExA)(0, n, "a", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc_inst(), NULL);
    HWND h2 = U(CreateWindowExA)(0, m, "b", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc_inst(), NULL);
    CHECK(h1 && h2);
    if (h1 && h2) {
        HMENU s1 = U(GetSystemMenu)(h1, FALSE), s2 = U(GetSystemMenu)(h2, FALSE);
        CHECK(s1 && s2);
        UINT st1 = U(GetMenuState)(s1, SC_CLOSE, MF_BYCOMMAND), st2 = U(GetMenuState)(s2, SC_CLOSE, MF_BYCOMMAND);
        CHECK(st1 != 0xFFFFFFFF && st2 != 0xFFFFFFFF);
        CHECK(st1 & MF_GRAYED);
        CHECK(!(st2 & MF_GRAYED));
    }
    if (h1) U(DestroyWindow)(h1);
    if (h2) U(DestroyWindow)(h2);
    U(UnregisterClassA)(n, wc_inst()); U(UnregisterClassA)(m, wc_inst());
}

/* ---------- hInstance semantics ---------- */

TEST(window_class_instance_null_registration) {
    REQUIRE(U(RegisterClassA)); REQUIRE(U(GetClassInfoA)); REQUIRE(U(UnregisterClassA));
    char n[64]; wc_uname(n, sizeof n, "wcInstNull");
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wc_defA; wc.hInstance = NULL; wc.lpszClassName = n;
    SetLastError(MAGIC_ERR);
    ATOM a = U(RegisterClassA)(&wc);
    CHECK(a != 0);
    if (a) {
        WNDCLASSA o;
        SetLastError(MAGIC_ERR);
        BOOL r0 = U(GetClassInfoA)(NULL, n, &o);
        t_note("GetClassInfoA(NULL) on NULL-instance class: %d err %lu", r0, GetLastError());
        BOOL r1 = U(GetClassInfoA)(wc_inst(), n, &o);
        t_note("GetClassInfoA(exe) on NULL-instance class: %d (hInstance %p)", r1, o.hInstance);
        HWND h = U(CreateWindowExA)(0, n, "x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
        CHECK(h != NULL);
        if (h) U(DestroyWindow)(h);
        h = U(CreateWindowExA)(0, n, "x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc_inst(), NULL);
        t_note("CreateWindow with exe instance on NULL-instance class: %p", h);
        if (h) U(DestroyWindow)(h);
        SetLastError(MAGIC_ERR);
        ATOM a2 = U(RegisterClassA)(&wc);
        CHECK(a2 == 0);
        EXPECT_LAST_ERR(ERR_CLASS_EXISTS);
        BOOL u = U(UnregisterClassA)(n, NULL);
        if (!u) u = U(UnregisterClassA)(n, wc_inst());
        CHECK(u);
    }
}

TEST(window_class_instance_other_module) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(GetClassInfoExA)); REQUIRE(U(UnregisterClassA));
    HINSTANCE k32 = GetModuleHandleA("kernel32.dll");
    char n[64]; wc_uname(n, sizeof n, "wcInstK32");
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.hInstance = k32; wc.lpszClassName = n;
    ATOM a = U(RegisterClassExA)(&wc);
    CHECK(a != 0);
    if (!a) return;
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(k32, n, &o));
    EQ(o.hInstance, (ULONG_PTR)k32);
    SetLastError(MAGIC_ERR);
    CHECK(!U(GetClassInfoExA)((HINSTANCE)0x6660000, n, &o));
    EXPECT_LAST_ERR(ERR_CLASS_NOT_EXIST);
    HWND h = U(CreateWindowExA)(0, n, "x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, k32, NULL);
    CHECK(h != NULL);
    if (h) {
        EQ(U(GetClassLongPtrA)(h, GCLP_HMODULE), (ULONG_PTR)k32);
        EQ(U(GetWindowLongPtrA)(h, GWLP_HINSTANCE), (ULONG_PTR)k32);
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassA)(n, k32));
}

TEST(window_class_instance_same_name_different_modules) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA)); REQUIRE(U(GetClassInfoExA));
    HINSTANCE k32 = GetModuleHandleA("kernel32.dll");
    char n[64]; wc_uname(n, sizeof n, "wcInstTwo");
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.lpszClassName = n;
    wc.hInstance = wc_inst(); wc.cbWndExtra = 4;
    CHECK(U(RegisterClassExA)(&wc));
    wc.hInstance = k32; wc.cbWndExtra = 8;
    ATOM b = U(RegisterClassExA)(&wc);
    CHECK(b != 0);
    if (b) {
        WNDCLASSEXA o; o.cbSize = sizeof o;
        CHECK(U(GetClassInfoExA)(wc_inst(), n, &o)); EQ(o.cbWndExtra, 4);
        CHECK(U(GetClassInfoExA)(k32, n, &o)); EQ(o.cbWndExtra, 8);
        CHECK(U(UnregisterClassA)(n, k32));
    }
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_instance_global_vs_local_same_name) {
    REQUIRE(U(RegisterClassExA)); REQUIRE(U(UnregisterClassA));
    HINSTANCE k32 = GetModuleHandleA("kernel32.dll");
    char n[64]; wc_uname(n, sizeof n, "wcInstGL");
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.lpszClassName = n; wc.hInstance = wc_inst();
    wc.style = CS_GLOBALCLASS;
    CHECK(U(RegisterClassExA)(&wc));
    wc.hInstance = k32; wc.style = 0;
    SetLastError(MAGIC_ERR);
    ATOM b = U(RegisterClassExA)(&wc);
    CHECK(b == 0);                      /* a global class of that name blocks local ones */
    EXPECT_LAST_ERR(ERR_CLASS_EXISTS);
    if (b) U(UnregisterClassA)(n, k32);
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_instance_user32_registration_rejected) {
    REQUIRE(U(RegisterClassExA));
    HMODULE u32 = t_user32();
    char n[64]; wc_uname(n, sizeof n, "wcInstU32");
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = wc_defA; wc.lpszClassName = n; wc.hInstance = u32;
    SetLastError(MAGIC_ERR);
    ATOM a = U(RegisterClassExA)(&wc);
    CHECK(a == 0);
    EXPECT_LAST_ERR(ERR_INVALID_PARAM);
    wc.style = CS_GLOBALCLASS;
    SetLastError(MAGIC_ERR);
    a = U(RegisterClassExA)(&wc);
    CHECK(a == 0);
    EXPECT_LAST_ERR(ERR_INVALID_PARAM);
    if (a) U(UnregisterClassA)(n, u32);
}
