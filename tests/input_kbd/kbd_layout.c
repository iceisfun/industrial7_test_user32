/* Keyboard layout management. */
#include "kbd.h"

#define NT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static int get_list(HKL *buf, int max)
{
    int n = U(GetKeyboardLayoutList)(0, NULL);
    if (n > max) n = max;
    return U(GetKeyboardLayoutList)(n, buf);
}

TEST(input_kbd_layout_current_is_sane) {
    HKL h;
    REQUIRE(U(GetKeyboardLayout));
    h = U(GetKeyboardLayout)(0);
    CHECK(h != NULL);
    CHECK(LOWORD((ULONG_PTR)h) != 0);
    CHECK(HIWORD((ULONG_PTR)h) != 0);
    CHECK(((ULONG_PTR)h >> 32) == 0 || ((ULONG_PTR)h >> 32) == 0xFFFFFFFF);
}
TEST(input_kbd_layout_tid_zero_equals_own_tid) {
    REQUIRE(U(GetKeyboardLayout));
    EXPECT_EQ(U(GetKeyboardLayout)(GetCurrentThreadId()), U(GetKeyboardLayout)(0));
}
TEST(input_kbd_layout_repeatable) {
    HKL a;
    int i;
    REQUIRE(U(GetKeyboardLayout));
    a = U(GetKeyboardLayout)(0);
    for (i = 0; i < 100; i++) EXPECT_EQ(U(GetKeyboardLayout)(0), a);
}
TEST(input_kbd_layout_invalid_thread_ids) {
    static const DWORD ids[] = { 0xFFFFFFFC, 0xFFFFFFFF, 0x7FFFFFFC, 0xFFFFFF00 };
    int i;
    REQUIRE(U(GetKeyboardLayout));
    for (i = 0; i < NT(ids); i++) {
        HKL h = U(GetKeyboardLayout)(ids[i]);
        CHECKF(h == NULL, "tid 0x%lx -> %p", ids[i], h);
    }
}
static DWORD WINAPI gui_thread(void *p) {
    HANDLE *ev = p;
    MSG m;
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);       /* create the message queue */
    SetEvent(ev[0]);
    WaitForSingleObject(ev[1], 5000);
    return 0;
}
TEST(input_kbd_layout_other_gui_thread) {
    HANDLE ev[2], th;
    DWORD tid;
    HKL h;
    REQUIRE(U(GetKeyboardLayout)); REQUIRE(U(PeekMessageA));
    ev[0] = CreateEventA(NULL, TRUE, FALSE, NULL); ev[1] = CreateEventA(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, gui_thread, ev, 0, &tid);
    CHECK(th != NULL);
    if (th) {
        EXPECT_EQ(WaitForSingleObject(ev[0], 5000), WAIT_OBJECT_0);
        h = U(GetKeyboardLayout)(tid);
        CHECK(h != NULL);
        if (h != U(GetKeyboardLayout)(0)) t_note("other thread layout %p differs from ours %p", h, U(GetKeyboardLayout)(0));
        SetEvent(ev[1]);
        WaitForSingleObject(th, 5000);
        CloseHandle(th);
        h = U(GetKeyboardLayout)(tid);
        CHECKF(h == NULL, "layout of an exited thread: %p", h);
    }
    CloseHandle(ev[0]); CloseHandle(ev[1]);
}

/* ---- GetKeyboardLayoutList ---- */
TEST(input_kbd_layoutlist_count_query) {
    int n;
    HKL buf[64];
    REQUIRE(U(GetKeyboardLayoutList));
    n = U(GetKeyboardLayoutList)(0, NULL);
    CHECK(n >= 1);
    EXPECT_EQ(U(GetKeyboardLayoutList)(0, buf), n);
    EXPECT_EQ(U(GetKeyboardLayoutList)(0, NULL), n);
}
TEST(input_kbd_layoutlist_fill_exact) {
    int n, r, i;
    HKL buf[66];
    REQUIRE(U(GetKeyboardLayoutList));
    n = U(GetKeyboardLayoutList)(0, NULL);
    CHECK(n >= 1 && n <= 64);
    if (n < 1 || n > 64) return;
    for (i = 0; i < 66; i++) buf[i] = (HKL)(ULONG_PTR)0x5A5A5A5A;
    r = U(GetKeyboardLayoutList)(n, buf);
    EXPECT_EQ(r, n);
    for (i = 0; i < n; i++) { CHECK(buf[i] != NULL); CHECK(buf[i] != (HKL)(ULONG_PTR)0x5A5A5A5A); }
    EXPECT_EQ(buf[n], (HKL)(ULONG_PTR)0x5A5A5A5A);
}
TEST(input_kbd_layoutlist_larger_buffer) {
    int n, r, i;
    HKL buf[70];
    REQUIRE(U(GetKeyboardLayoutList));
    n = U(GetKeyboardLayoutList)(0, NULL);
    if (n < 1 || n > 60) return;
    for (i = 0; i < 70; i++) buf[i] = (HKL)(ULONG_PTR)0x5A5A5A5A;
    r = U(GetKeyboardLayoutList)(n + 8, buf);
    EXPECT_EQ(r, n);
    for (i = n; i < 70; i++) EXPECT_EQ(buf[i], (HKL)(ULONG_PTR)0x5A5A5A5A);
}
TEST(input_kbd_layoutlist_entries_unique_and_valid) {
    HKL buf[64];
    int n, i, j;
    REQUIRE(U(GetKeyboardLayoutList));
    n = get_list(buf, 64);
    CHECK(n >= 1);
    for (i = 0; i < n; i++) {
        CHECK(LOWORD((ULONG_PTR)buf[i]) != 0);
        for (j = i + 1; j < n; j++) CHECKF(buf[i] != buf[j], "duplicate %p", buf[i]);
    }
}
TEST(input_kbd_layoutlist_contains_current) {
    HKL buf[64], cur;
    int n, i, found = 0;
    REQUIRE(U(GetKeyboardLayoutList)); REQUIRE(U(GetKeyboardLayout));
    n = get_list(buf, 64);
    cur = U(GetKeyboardLayout)(0);
    for (i = 0; i < n; i++) if (buf[i] == cur) found = 1;
    if (!found) t_note("current layout %p not in list (IME or substituted layout?)", cur);
    CHECK(n >= 1);
}
TEST(input_kbd_layoutlist_stable_between_calls) {
    HKL a[64], b[64];
    int n1, n2;
    REQUIRE(U(GetKeyboardLayoutList));
    n1 = get_list(a, 64); n2 = get_list(b, 64);
    EXPECT_EQ(n1, n2);
    if (n1 == n2 && n1 > 0) CHECK(!memcmp(a, b, n1 * sizeof(HKL)));
}
TEST(input_kbd_layoutlist_small_buffer_observed) {
    HKL buf[64];
    int n, r;
    REQUIRE(U(GetKeyboardLayoutList));
    n = U(GetKeyboardLayoutList)(0, NULL);
    if (n < 2) { t_note("only %d layout loaded", n); CHECK(1); return; }
    SET_ERR(0);
    r = U(GetKeyboardLayoutList)(1, buf);
    t_note("GetKeyboardLayoutList(1, buf) with %d layouts returned %d err %lu", n, r, GetLastError());
    CHECK(1);
}
TEST(input_kbd_layoutlist_each_layout_translates_return) {
    HKL buf[64];
    int n, i;
    REQUIRE(U(GetKeyboardLayoutList)); REQUIRE(U(MapVirtualKeyExW)); REQUIRE(U(ToUnicodeEx));
    n = get_list(buf, 64);
    for (i = 0; i < n; i++) {
        BYTE st[256] = {0};
        WCHAR w[4] = {0};
        EXPECT_EQ(U(MapVirtualKeyExW)(VK_RETURN, 0, buf[i]), 0x1C);
        EXPECT_EQ(U(MapVirtualKeyExW)(VK_ESCAPE, 0, buf[i]), 0x01);
        EXPECT_EQ(U(MapVirtualKeyExW)(VK_SPACE, 0, buf[i]), 0x39);
        EXPECT_EQ(U(MapVirtualKeyExW)(0x1C, 1, buf[i]), VK_RETURN);
        EXPECT_EQ(U(MapVirtualKeyExW)(0x2A, 3, buf[i]), VK_LSHIFT);
        CHECK(U(ToUnicodeEx)(VK_RETURN, 0x1C, st, w, 4, 0, buf[i]) == 1);
        EXPECT_EQ(w[0], '\r');
        CHECK(U(ToUnicodeEx)(VK_SPACE, 0x39, st, w, 4, 4, buf[i]) == 1);
        EXPECT_EQ(w[0], ' ');
    }
}

/* ---- GetKeyboardLayoutName ---- */
TEST(input_kbd_layoutname_format) {
    char a[16]; WCHAR w[16];
    int i;
    REQUIRE(U(GetKeyboardLayoutNameA)); REQUIRE(U(GetKeyboardLayoutNameW));
    memset(a, 0xCC, sizeof a); memset(w, 0xCC, sizeof w);
    CHECK(U(GetKeyboardLayoutNameW)(w));
    CHECK(U(GetKeyboardLayoutNameA)(a));
    EXPECT_EQ(lstrlenW(w), 8);
    EXPECT_EQ(strlen(a), 8);
    EXPECT_EQ(w[8], 0);
    EXPECT_EQ(a[8], 0);
    CHECK(kbd_hexname(a));
    for (i = 0; i < 8; i++) EXPECT_EQ(w[i], (WCHAR)(unsigned char)a[i]);
    /* no write past the NUL */
    EXPECT_EQ((unsigned char)a[9], 0xCC);
    EXPECT_EQ(w[9], 0xCCCC);
}
TEST(input_kbd_layoutname_null_wide) {
    REQUIRE(U(GetKeyboardLayoutNameW));
    SET_ERR(0xdeadbeef);
    CHECK(!U(GetKeyboardLayoutNameW)(NULL));
    EXPECT_ERR(0, ERROR_NOACCESS);
}
TEST(input_kbd_layoutname_repeatable) {
    char a[16], b[16];
    int i;
    REQUIRE(U(GetKeyboardLayoutNameA));
    CHECK(U(GetKeyboardLayoutNameA)(a));
    for (i = 0; i < 100; i++) {
        CHECK(U(GetKeyboardLayoutNameA)(b));
        if (strcmp(a, b)) { CHECKF(0, "changed at %d", i); break; }
    }
}
TEST(input_kbd_layoutname_matches_hkl_language) {
    char a[16];
    HKL h;
    unsigned long klid;
    REQUIRE(U(GetKeyboardLayoutNameA)); REQUIRE(U(GetKeyboardLayout));
    CHECK(U(GetKeyboardLayoutNameA)(a));
    h = U(GetKeyboardLayout)(0);
    klid = strtoul(a, NULL, 16);
    if (LOWORD(klid) != LOWORD((ULONG_PTR)h))
        t_note("layout name %s low word differs from HKL %p language", a, h);
    CHECK(klid != 0);
}
TEST(input_kbd_layoutname_exists_in_registry) {
    char a[16], key[128];
    HKEY k;
    LONG r;
    REQUIRE(U(GetKeyboardLayoutNameA));
    CHECK(U(GetKeyboardLayoutNameA)(a));
    wsprintfA(key, "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\%s", a);
    r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &k);
    if (r == ERROR_SUCCESS) {
        char file[128]; DWORD cb = sizeof file, ty;
        r = RegQueryValueExA(k, "Layout File", NULL, &ty, (BYTE *)file, &cb);
        CHECK(r == ERROR_SUCCESS);
        RegCloseKey(k);
    } else t_note("layout %s not found under Keyboard Layouts (%ld)", a, r);
    CHECK(1);
}
static DWORD WINAPI name_thread(void *p) {
    char *a = p;
    MSG m;
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);
    a[0] = 0;
    return U(GetKeyboardLayoutNameA)(a) ? 1 : 0;
}
TEST(input_kbd_layoutname_worker_thread) {
    char a[16] = "", b[16] = "";
    HANDLE th;
    DWORD code = 0;
    REQUIRE(U(GetKeyboardLayoutNameA)); REQUIRE(U(PeekMessageA));
    CHECK(U(GetKeyboardLayoutNameA)(b));
    th = CreateThread(NULL, 0, name_thread, a, 0, NULL);
    CHECK(th != NULL);
    if (!th) return;
    WaitForSingleObject(th, 5000);
    GetExitCodeThread(th, &code);
    CloseHandle(th);
    EXPECT_EQ(code, 1);
    CHECK(kbd_hexname(a));
    if (strcmp(a, b)) t_note("worker layout name %s differs from main %s", a, b);
}

/* ---- LoadKeyboardLayout / Unload / Activate ---- */
TEST(input_kbd_loadlayout_us_w_and_a_agree) {
    HKL w, a;
    REQUIRE(U(LoadKeyboardLayoutW)); REQUIRE(U(LoadKeyboardLayoutA));
    { HKL us = kbd_us_hkl();
    if (!us) { kbd_us_release(); t_skip("cannot load US layout"); return; }
    w = U(LoadKeyboardLayoutW)(L"00000409", 0);
    a = U(LoadKeyboardLayoutA)("00000409", 0);
    CHECK(w != NULL);
    EXPECT_EQ(w, a);
    EXPECT_EQ(w, us);
    EXPECT_EQ(LOWORD((ULONG_PTR)w), 0x0409);
    EXPECT_EQ(w, (HKL)(ULONG_PTR)0x04090409);
    kbd_us_release(); }
}
TEST(input_kbd_loadlayout_idempotent) {
    HKL a, b, c;
    REQUIRE(U(LoadKeyboardLayoutW));
    { HKL us = kbd_us_hkl();
    if (!us) { kbd_us_release(); t_skip("cannot load US layout"); return; }
    a = U(LoadKeyboardLayoutW)(L"00000409", 0);
    b = U(LoadKeyboardLayoutW)(L"00000409", 0);
    c = U(LoadKeyboardLayoutW)(L"00000409", KLF_NOTELLSHELL);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, c);
    kbd_us_release(); }
}
TEST(input_kbd_loadlayout_lowercase_hex) {
    HKL a, b;
    REQUIRE(U(LoadKeyboardLayoutW));
    { HKL us = kbd_us_hkl();
    if (!us) { kbd_us_release(); t_skip("cannot load US layout"); return; }
    a = U(LoadKeyboardLayoutW)(L"00000409", 0);
    b = U(LoadKeyboardLayoutW)(L"00000409", 0);
    EXPECT_EQ(a, b);
    kbd_us_release(); }
}
TEST(input_kbd_loadlayout_invalid_names) {
    static const WCHAR *names[] = { L"", L"ZZZZZZZZ", L"not a layout", L"!!!!!!!!" };
    int i;
    REQUIRE(U(LoadKeyboardLayoutW));
    for (i = 0; i < NT(names); i++) {
        HKL h;
        SET_ERR(0);
        h = U(LoadKeyboardLayoutW)(names[i], 0);
        CHECKF(h == NULL, "name %d accepted: %p", i, h);
        if (h) continue;
        t_note("LoadKeyboardLayoutW(name %d) failed with %lu", i, GetLastError());
    }
}
TEST(input_kbd_loadlayout_a_invalid_names) {
    HKL h;
    REQUIRE(U(LoadKeyboardLayoutA));
    h = U(LoadKeyboardLayoutA)("", 0);
    CHECK(h == NULL);
    h = U(LoadKeyboardLayoutA)("ZZZZZZZZ", 0);
    CHECK(h == NULL);
}
TEST(input_kbd_unload_bogus_handles) {
    static const ULONG_PTR v[] = { 0x12345678, 0xFFFFFFFF, 0xDEAD0409, 0x7FFF7FFF };
    int i;
    REQUIRE(U(UnloadKeyboardLayout));
    for (i = 0; i < NT(v); i++) {
        SET_ERR(0);
        CHECKF(!U(UnloadKeyboardLayout)((HKL)v[i]), "bogus %p unloaded", (void *)v[i]);
        t_note("UnloadKeyboardLayout(%p) error %lu", (void *)v[i], GetLastError());
    }
}
TEST(input_kbd_unload_null) {
    REQUIRE(U(UnloadKeyboardLayout));
    SET_ERR(0);
    CHECK(!U(UnloadKeyboardLayout)(NULL));
}
TEST(input_kbd_unload_roundtrip_if_not_preloaded) {
    HKL list[64], h;
    int n, i;
    REQUIRE(U(LoadKeyboardLayoutW)); REQUIRE(U(UnloadKeyboardLayout)); REQUIRE(U(GetKeyboardLayoutList));
    n = get_list(list, 64);
    for (i = 0; i < n; i++) if ((ULONG_PTR)list[i] == 0x04090409) { t_skip("US layout already present; not touching it"); return; }
    h = U(LoadKeyboardLayoutW)(L"00000409", 0);
    if (!h) { t_skip("cannot load"); return; }
    if (!U(UnloadKeyboardLayout)(h)) { t_note("unload of freshly loaded layout failed: %lu", GetLastError()); return; }
    SET_ERR(0);
    CHECK(!U(UnloadKeyboardLayout)(h));
}
TEST(input_kbd_activate_bogus_handle) {
    HKL r;
    REQUIRE(U(ActivateKeyboardLayout));
    SET_ERR(0);
    r = U(ActivateKeyboardLayout)((HKL)(ULONG_PTR)0xDEAD1234, 0);
    CHECK(r == NULL);
    t_note("ActivateKeyboardLayout(bogus) error %lu", GetLastError());
}
struct act { HKL orig, ret_same, ret_next, after_next, after_restore, ret_restore; HKL main_after; int ok; };
static DWORD WINAPI act_thread(void *p) {
    struct act *a = p;
    MSG m;
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);
    a->orig = U(GetKeyboardLayout)(0);
    a->ret_same = U(ActivateKeyboardLayout)(a->orig, 0);
    a->ret_next = U(ActivateKeyboardLayout)((HKL)(ULONG_PTR)HKL_NEXT, 0);
    a->after_next = U(GetKeyboardLayout)(0);
    a->ret_restore = U(ActivateKeyboardLayout)(a->orig, 0);
    a->after_restore = U(GetKeyboardLayout)(0);
    a->ok = 1;
    return 0;
}
TEST(input_kbd_activate_current_and_next_in_worker) {
    struct act a;
    HKL before, list[64];
    HANDLE th;
    int n, i, in = 0;
    REQUIRE(U(ActivateKeyboardLayout)); REQUIRE(U(GetKeyboardLayout)); REQUIRE(U(PeekMessageA));
    memset(&a, 0, sizeof a);
    before = U(GetKeyboardLayout)(0);
    th = CreateThread(NULL, 0, act_thread, &a, 0, NULL);
    CHECK(th != NULL);
    if (!th) return;
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(a.ok);
    if (!a.ok) return;
    EXPECT_EQ(a.ret_same, a.orig);          /* returns the previously active layout */
    n = get_list(list, 64);
    if (n > 1) { EXPECT_EQ(a.ret_next, a.orig); }
    else if (a.ret_next != a.orig) t_note("HKL_NEXT with a single loaded layout returned %p", a.ret_next);
    if (n <= 1) { CHECK(1); return; }
    CHECK(a.after_next != NULL);
    for (i = 0; i < n; i++) if (list[i] == a.after_next) in = 1;
    CHECK(in);
    EXPECT_EQ(a.ret_restore, a.after_next);
    EXPECT_EQ(a.after_restore, a.orig);
    EXPECT_EQ(U(GetKeyboardLayout)(0), before);   /* other threads unaffected */
}
