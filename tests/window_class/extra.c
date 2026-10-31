#include "wc.h"

TEST(window_class_x_name_with_spaces_preserved) {
    REQUIRE(U(GetClassNameA)); REQUIRE(U(UnregisterClassA)); REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow));
    char n[96]; wc_uname(n, sizeof n, "wc x sp ");
    strcat(n, " ");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    HWND h = wc_createA(n, WS_POPUP, NULL);
    CHECK(h != NULL);
    if (h) {
        char b[128] = "";
        int r = U(GetClassNameA)(h, b, sizeof b);
        EQ(r, strlen(n));
        CHECK(!strcmp(b, n));
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_x_register_unregister_cycles) {
    REQUIRE(U(UnregisterClassA)); REQUIRE(U(GetClassInfoExA));
    char n[64]; wc_uname(n, sizeof n, "wcXCycle");
    for (int i = 0; i < 300; i++) {
        ATOM a = wc_regA(n, 0, 4, 4, NULL);
        if (!a) { CHECKF(0, "register failed at cycle %d err %lu", i, GetLastError()); break; }
        CHECK(a >= 0xC000);
        WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
        CHECK(U(GetClassInfoExA)(wc_inst(), n, &o));
        if (!U(UnregisterClassA)(n, wc_inst())) { CHECKF(0, "unregister failed at %d", i); break; }
        o.cbSize = sizeof o;
        CHECK(!U(GetClassInfoExA)(wc_inst(), n, &o));
    }
}

TEST(window_class_x_userdata_many_values) {
    REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(DestroyWindow));
    char n[64]; wc_uname(n, sizeof n, "wcXUD");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    HWND h = wc_createA(n, WS_POPUP, NULL);
    CHECK(h != NULL);
    if (h) {
        LONG_PTR prev = 0;
        for (int i = 1; i < 2000; i++) {
            LONG_PTR v = (LONG_PTR)((unsigned long long)i * 0x100000001ull);
            LONG_PTR old = U(SetWindowLongPtrA)(h, GWLP_USERDATA, v);
            if (old != prev) { CHECKF(0, "old value mismatch at %d", i); break; }
            prev = v;
            if (U(GetWindowLongPtrA)(h, GWLP_USERDATA) != v) { CHECKF(0, "readback mismatch at %d", i); break; }
        }
        CHECK(1);
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_x_getclassinfo_case_insensitive_custom) {
    REQUIRE(U(GetClassInfoExA)); REQUIRE(U(UnregisterClassA));
    char n[64], up[64]; wc_uname(n, sizeof n, "wcXcase");
    strcpy(up, n); for (char *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    WNDCLASSEXA o; memset(&o, 0, sizeof o); o.cbSize = sizeof o;
    CHECK(U(GetClassInfoExA)(wc_inst(), up, &o));
    CHECK(U(UnregisterClassA)(up, wc_inst()));
    o.cbSize = sizeof o;
    CHECK(!U(GetClassInfoExA)(wc_inst(), n, &o));
}

TEST(window_class_x_wide_class_unicode_name_roundtrip) {
    REQUIRE(U(RegisterClassExW)); REQUIRE(U(GetClassNameW)); REQUIRE(U(CreateWindowExW));
    REQUIRE(U(UnregisterClassW)); REQUIRE(U(DestroyWindow));
    WCHAR n[64] = L"wcXUni_\x00e9\x4e2d\x0416_";
    WCHAR sfx[16]; wsprintfW(sfx, L"%lu", GetCurrentProcessId()); lstrcatW(n, sfx);
    ATOM a = wc_regW(n, 0, 0, 0, NULL);
    CHECK(a != 0);
    HWND h = wc_createW(n, WS_POPUP, NULL);
    CHECK(h != NULL);
    if (h) {
        WCHAR b[64] = {0};
        int r = U(GetClassNameW)(h, b, 64);
        EQ(r, lstrlenW(n));
        CHECK(!lstrcmpW(b, n));
        CHECK(U(IsWindowUnicode)(h));
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassW)(n, wc_inst()));
}

TEST(window_class_x_setprop_distinct_windows_counts) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(DestroyWindow));
    char n[64]; wc_uname(n, sizeof n, "wcXProp");
    CHECK(wc_regA(n, 0, 0, 0, NULL));
    HWND hs[8];
    for (int i = 0; i < 8; i++) {
        hs[i] = wc_createA(n, WS_POPUP, NULL);
        CHECK(hs[i] != NULL);
        if (hs[i]) CHECK(U(SetPropA)(hs[i], "wcXPropVal", (HANDLE)(ULONG_PTR)(i + 1)));
    }
    for (int i = 0; i < 8; i++)
        if (hs[i]) EQ(U(GetPropA)(hs[i], "wcXPropVal"), i + 1);
    for (int i = 0; i < 8; i++)
        if (hs[i]) { EQ(U(RemovePropA)(hs[i], "wcXPropVal"), i + 1); EQ(U(GetPropA)(hs[i], "wcXPropVal"), 0); U(DestroyWindow)(hs[i]); }
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_x_gcl_atom_matches_register) {
    REQUIRE(U(GetClassLongA)); REQUIRE(U(DestroyWindow));
    char n[64]; wc_uname(n, sizeof n, "wcXAtom");
    ATOM a = wc_regA(n, 0, 0, 0, NULL);
    CHECK(a != 0);
    HWND h = wc_createA(n, WS_POPUP, NULL);
    CHECK(h != NULL);
    if (h) {
        EQ(U(GetClassLongA)(h, GCW_ATOM) & 0xFFFF, a);
        EQ(U(GetClassWord)(h, GCW_ATOM), a);
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassA)(n, wc_inst()));
}

TEST(window_class_x_unregister_null_name) {
    REQUIRE(U(UnregisterClassA)); REQUIRE(U(UnregisterClassW));
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassA)(NULL, wc_inst()));
    t_note("UnregisterClassA(NULL) err %lu", GetLastError());
    SetLastError(MAGIC_ERR);
    CHECK(!U(UnregisterClassW)(NULL, wc_inst()));
    t_note("UnregisterClassW(NULL) err %lu", GetLastError());
}
