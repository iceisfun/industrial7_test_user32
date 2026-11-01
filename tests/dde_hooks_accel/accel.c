/* Accelerator tables: CreateAcceleratorTable / CopyAcceleratorTable / DestroyAcceleratorTable /
 * LoadAccelerators / TranslateAccelerator, driven by synthetic MSG structures. */
#include "dh.h"

static const char *ACLS = "T32AccelWnd";
static int g_cmd_count, g_sys_count;
static WPARAM g_cmd_wp, g_sys_wp;
static LPARAM g_cmd_lp, g_sys_lp;
static HWND g_cmd_hwnd;

static LRESULT CALLBACK accel_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND) { g_cmd_count++; g_cmd_wp = w; g_cmd_lp = l; g_cmd_hwnd = h; return 0; }
    if (m == WM_SYSCOMMAND) { g_sys_count++; g_sys_wp = w; g_sys_lp = l; return 0; }
    return DefWindowProcA(h, m, w, l);
}
static void reset_cmd(void) { g_cmd_count = g_sys_count = 0; g_cmd_wp = g_sys_wp = 0; g_cmd_lp = g_sys_lp = 0; g_cmd_hwnd = NULL; }
static HWND mkwnd(void) {
    reset_cmd();
    return dh_create_window(ACLS, accel_proc, WS_OVERLAPPEDWINDOW, 0, NULL);
}
static void killwnd(HWND h) { dh_destroy_window(h, ACLS); }
static int mods_down(void) {
    return (GetKeyState(VK_SHIFT) & 0x8000) || (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000);
}
static int need_accel(void) {
    return U(CreateAcceleratorTableA) && U(CreateAcceleratorTableW) && U(DestroyAcceleratorTable) && U(CopyAcceleratorTableA) &&
           U(CopyAcceleratorTableW) && U(TranslateAcceleratorA) && U(TranslateAcceleratorW) && U(RegisterClassA) &&
           U(CreateWindowExA) && U(DestroyWindow) && U(UnregisterClassA);
}
/* Win32k-side last-error for a bad table handle is not visible in the user32 stubs: accept the plausible codes. */
#define BADACCEL_ERR(msg) do { DWORD e_ = GetLastError(); \
    t_check(e_ == ERROR_INVALID_ACCEL_HANDLE || e_ == ERROR_INVALID_PARAMETER || e_ == ERROR_INVALID_HANDLE, __FILE__, __LINE__, "bad accel handle error", "%s err %lu", msg, e_); \
    if (e_ != ERROR_INVALID_ACCEL_HANDLE) t_note("bad accel handle -> err %lu (%s)", e_, msg); } while (0)
#define NEED() do { if (!need_accel()) { t_skip("missing exports"); return; } } while (0)

static HACCEL mk1(BYTE virt, WORD key, WORD cmd) {
    ACCEL a __attribute__((aligned(8))) = {virt, key, cmd};
    return U(CreateAcceleratorTableA)(&a, 1);
}
static LRESULT ta(HWND w, HACCEL h, UINT msg, WPARAM wp) {
    MSG m; memset(&m, 0, sizeof m);
    m.hwnd = w; m.message = msg; m.wParam = wp; m.lParam = 1; m.time = GetTickCount();
    reset_cmd();
    return U(TranslateAcceleratorA)(w, h, &m);
}
static LRESULT taw(HWND w, HACCEL h, UINT msg, WPARAM wp) {
    MSG m; memset(&m, 0, sizeof m);
    m.hwnd = w; m.message = msg; m.wParam = wp; m.lParam = 1; m.time = GetTickCount();
    reset_cmd();
    return U(TranslateAcceleratorW)(w, h, &m);
}

/* ---------- creation / destruction ---------- */
TEST(accel_create_single) {
    NEED();
    ACCEL a __attribute__((aligned(8))) = {FVIRTKEY, 'A', 100};
    HACCEL h = U(CreateAcceleratorTableA)(&a, 1);
    CHECK(h != NULL);
    EQ(U(DestroyAcceleratorTable)(h), TRUE);
    h = U(CreateAcceleratorTableW)(&a, 1);
    CHECK(h != NULL);
    EQ(U(DestroyAcceleratorTable)(h), TRUE);
}
TEST(accel_create_zero_count) {
    NEED();
    ACCEL a __attribute__((aligned(8))) = {FVIRTKEY, 'A', 100};
    SetLastError(0);
    EQ(U(CreateAcceleratorTableA)(&a, 0), NULL);
    t_note("CreateAcceleratorTableA(cEntries=0) err=%lu", GetLastError());
    SetLastError(0);
    EQ(U(CreateAcceleratorTableW)(&a, 0), NULL);
    DWORD e = GetLastError();
    t_note("CreateAcceleratorTableW(cEntries=0) err=%lu", e);
    EQ(e, ERROR_INVALID_PARAMETER);
}
TEST(accel_create_negative_count_w) {
    NEED();
    ACCEL a __attribute__((aligned(8))) = {FVIRTKEY, 'A', 100};
    SetLastError(0);
    HACCEL h = U(CreateAcceleratorTableW)(&a, -1);
    EQ(h, NULL);
    t_note("CreateAcceleratorTableW(-1) err=%lu", GetLastError());
    if (h) U(DestroyAcceleratorTable)(h);
}
TEST(accel_create_distinct_handles) {
    NEED();
    HACCEL a = mk1(FVIRTKEY, 'A', 1), b = mk1(FVIRTKEY, 'A', 1), c = mk1(FVIRTKEY, 'A', 1);
    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
    U(DestroyAcceleratorTable)(a); U(DestroyAcceleratorTable)(b); U(DestroyAcceleratorTable)(c);
}
TEST(accel_destroy_twice) {
    NEED();
    HACCEL h = mk1(FVIRTKEY, 'B', 7);
    EQ(U(DestroyAcceleratorTable)(h), TRUE);
    SetLastError(0);
    EQ(U(DestroyAcceleratorTable)(h), FALSE);
    BADACCEL_ERR("destroy twice");
}
TEST(accel_destroy_invalid_handles) {
    NEED();
    static const ULONG_PTR bad[] = {0, 1, 2, 0x1234, 0xFFFF, 0x10000, 0xDEADBEEF, ~(ULONG_PTR)0};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(U(DestroyAcceleratorTable)((HACCEL)bad[i]) == FALSE, "h=0x%llx", (unsigned long long)bad[i]);
        BADACCEL_ERR("destroy bogus");
    }
}
TEST(accel_destroy_wrong_object_type) {
    NEED();
    HWND w = mkwnd();
    SetLastError(0);
    EQ(U(DestroyAcceleratorTable)((HACCEL)w), FALSE);
    BADACCEL_ERR("destroy window handle");
    CHECK(IsWindow(w));
    killwnd(w);
}
TEST(accel_copy_count_query) {
    NEED();
    ACCEL a[3] __attribute__((aligned(8))) = {{FVIRTKEY, 'A', 1}, {FVIRTKEY | FCONTROL, 'B', 2}, {0, 'c', 3}};
    HACCEL h = U(CreateAcceleratorTableA)(a, 3);
    CHECK(h != NULL);
    EQ(U(CopyAcceleratorTableA)(h, NULL, 0), 3);
    EQ(U(CopyAcceleratorTableW)(h, NULL, 0), 3);
    EQ(U(CopyAcceleratorTableA)(h, NULL, 100), 3);
    U(DestroyAcceleratorTable)(h);
}
TEST(accel_copy_roundtrip) {
    NEED();
    ACCEL a[5] __attribute__((aligned(8))) = {{FVIRTKEY, 'A', 11}, {FVIRTKEY | FCONTROL, 'S', 22}, {FVIRTKEY | FSHIFT | FALT, VK_F5, 33},
                  {0, 'z', 44}, {FVIRTKEY | FNOINVERT, VK_ESCAPE, 55}};
    ACCEL src[5] __attribute__((aligned(8))); memcpy(src, a, sizeof a);
    HACCEL h = U(CreateAcceleratorTableA)(a, 5);
    ACCEL out[8] __attribute__((aligned(8))); memset(out, 0xEE, sizeof out);
    EQ(U(CopyAcceleratorTableA)(h, out, 8), 5);
    for (int i = 0; i < 5; i++) {
        CHECKF((out[i].fVirt & ~FLASTKEY) == src[i].fVirt, "A fVirt[%d] 0x%x vs 0x%x", i, out[i].fVirt, src[i].fVirt);
        CHECKF(out[i].key == src[i].key, "A key[%d] 0x%x vs 0x%x", i, out[i].key, src[i].key);
        CHECKF(out[i].cmd == src[i].cmd, "A cmd[%d]", i);
    }
    memset(out, 0xEE, sizeof out);
    EQ(U(CopyAcceleratorTableW)(h, out, 8), 5);
    for (int i = 0; i < 5; i++) {
        CHECKF((out[i].fVirt & ~FLASTKEY) == src[i].fVirt, "W fVirt[%d]", i);
        CHECKF(out[i].key == src[i].key, "W key[%d]", i);
        CHECKF(out[i].cmd == src[i].cmd, "W cmd[%d]", i);
    }
    U(DestroyAcceleratorTable)(h);
}
TEST(accel_copy_fvirt_last_flag) {
    NEED();
    ACCEL a[2] __attribute__((aligned(8))) = {{FVIRTKEY, 'A', 1}, {FVIRTKEY, 'B', 2}};
    HACCEL h = U(CreateAcceleratorTableA)(a, 2);
    ACCEL out[2] __attribute__((aligned(8))) = {{0}};
    EQ(U(CopyAcceleratorTableA)(h, out, 2), 2);
    t_note("fVirt of copied entries: 0x%02x 0x%02x (FLASTKEY=0x80)", out[0].fVirt, out[1].fVirt);
    CHECK((out[0].fVirt & FLASTKEY) == 0);
    U(DestroyAcceleratorTable)(h);
}
TEST(accel_copy_buffer_sizes) {
    NEED();
    ACCEL a[4] __attribute__((aligned(8))) = {{FVIRTKEY, 'A', 1}, {FVIRTKEY, 'B', 2}, {FVIRTKEY, 'C', 3}, {FVIRTKEY, 'D', 4}};
    HACCEL h = U(CreateAcceleratorTableA)(a, 4);
    ACCEL out[6] __attribute__((aligned(8)));
    static const int req[] = {0, 1, 2, 3, 4, 5, 6};
    static const int want[] = {0, 1, 2, 3, 4, 4, 4};
    for (int i = 0; i < 7; i++) {
        memset(out, 0, sizeof out);
        int n = U(CopyAcceleratorTableA)(h, out, req[i]);
        CHECKF(n == want[i], "request %d got %d", req[i], n);
        for (int j = 0; j < want[i]; j++) CHECKF(out[j].cmd == j + 1, "request %d entry %d", req[i], j);
        for (int j = want[i]; j < 6; j++) CHECKF(out[j].cmd == 0, "request %d untouched %d", req[i], j);
    }
    U(DestroyAcceleratorTable)(h);
}
TEST(accel_copy_invalid_handle) {
    NEED();
    ACCEL out[2] __attribute__((aligned(8)));
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xDEADBEEF};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(U(CopyAcceleratorTableA)((HACCEL)bad[i], out, 2) == 0, "A h=0x%llx", (unsigned long long)bad[i]);
        BADACCEL_ERR("copy A bogus");
        SetLastError(0);
        CHECKF(U(CopyAcceleratorTableW)((HACCEL)bad[i], NULL, 0) == 0, "W h=0x%llx", (unsigned long long)bad[i]);
        BADACCEL_ERR("copy W bogus");
    }
}
TEST(accel_copy_after_destroy) {
    NEED();
    HACCEL h = mk1(FVIRTKEY, 'A', 1);
    U(DestroyAcceleratorTable)(h);
    ACCEL out[1] __attribute__((aligned(8)));
    SetLastError(0);
    EQ(U(CopyAcceleratorTableA)(h, out, 1), 0);
    BADACCEL_ERR("copy after destroy");
}
TEST(accel_create_in_place_key_conversion) {
    NEED();
    /* The A entry point rewrites the key of non-virtual-key entries in the caller's array. ASCII is unchanged. */
    ACCEL a[2] __attribute__((aligned(8))) = {{0, 'q', 5}, {FVIRTKEY, 'Q', 6}};
    HACCEL h = U(CreateAcceleratorTableA)(a, 2);
    EQ(a[0].key, 'q');
    EQ(a[1].key, 'Q');
    EQ(a[0].cmd, 5); EQ(a[1].cmd, 6);
    U(DestroyAcceleratorTable)(h);
}
TEST(accel_a_w_parity_ascii_chars) {
    NEED();
    for (WORD ch = 0x20; ch < 0x7F; ch++) {
        ACCEL aa __attribute__((aligned(8))) = {0, ch, 900}, aw = {0, ch, 900};
        HACCEL ha = U(CreateAcceleratorTableA)(&aa, 1), hw = U(CreateAcceleratorTableW)(&aw, 1);
        ACCEL oa __attribute__((aligned(8))) = {0}, ow = {0};
        if (!ha || !hw) { CHECKF(0, "create 0x%x", ch); if (ha) U(DestroyAcceleratorTable)(ha); if (hw) U(DestroyAcceleratorTable)(hw); continue; }
        U(CopyAcceleratorTableA)(ha, &oa, 1); U(CopyAcceleratorTableW)(hw, &ow, 1);
        CHECKF(oa.key == ch && ow.key == ch, "key 0x%x -> A 0x%x W 0x%x", ch, oa.key, ow.key);
        U(DestroyAcceleratorTable)(ha); U(DestroyAcceleratorTable)(hw);
    }
}
TEST(accel_wide_key_beyond_ascii) {
    NEED();
    ACCEL w __attribute__((aligned(8))) = {0, 0x20AC, 3};     /* euro sign as a wide character */
    HACCEL h = U(CreateAcceleratorTableW)(&w, 1);
    CHECK(h != NULL);
    ACCEL o __attribute__((aligned(8))) = {0};
    EQ(U(CopyAcceleratorTableW)(h, &o, 1), 1);
    EQ(o.key, 0x20AC);
    U(DestroyAcceleratorTable)(h);
}
TEST(accel_virtkey_codes_roundtrip) {
    NEED();
    for (WORD vk = 1; vk < 0xFF; vk++) {
        ACCEL a __attribute__((aligned(8))) = {FVIRTKEY, vk, vk};
        HACCEL h = U(CreateAcceleratorTableW)(&a, 1);
        if (!h) { CHECKF(0, "vk 0x%x", vk); continue; }
        ACCEL o __attribute__((aligned(8))) = {0};
        int n = U(CopyAcceleratorTableW)(h, &o, 1);
        CHECKF(n == 1 && o.key == vk && o.cmd == vk, "vk 0x%x", vk);
        U(DestroyAcceleratorTable)(h);
    }
}
TEST(accel_all_modifier_combinations) {
    NEED();
    for (BYTE m = 0; m < 0x20; m++) {
        BYTE f = (BYTE)(m & (FVIRTKEY | FNOINVERT | FSHIFT | FCONTROL | FALT));
        ACCEL a __attribute__((aligned(8))) = {f, 'M', 1234};
        HACCEL h = U(CreateAcceleratorTableW)(&a, 1);
        CHECKF(h != NULL, "flags 0x%x", f);
        if (!h) continue;
        ACCEL o __attribute__((aligned(8))) = {0};
        U(CopyAcceleratorTableW)(h, &o, 1);
        CHECKF((o.fVirt & ~FLASTKEY) == f, "flags 0x%x -> 0x%x", f, o.fVirt);
        U(DestroyAcceleratorTable)(h);
    }
}
TEST(accel_cmd_extremes) {
    NEED();
    static const WORD cmds[] = {0, 1, 0x7FFF, 0x8000, 0xFFFF, 0xF000, 0xF060};
    for (unsigned i = 0; i < sizeof cmds / sizeof cmds[0]; i++) {
        ACCEL a __attribute__((aligned(8))) = {FVIRTKEY, 'C', cmds[i]};
        HACCEL h = U(CreateAcceleratorTableW)(&a, 1);
        ACCEL o __attribute__((aligned(8))) = {0};
        CHECKF(h && U(CopyAcceleratorTableW)(h, &o, 1) == 1 && o.cmd == cmds[i], "cmd 0x%x", cmds[i]);
        if (h) U(DestroyAcceleratorTable)(h);
    }
}
TEST(accel_many_entries) {
    NEED();
    static const int counts[] = {2, 10, 100, 500, 1000};
    for (unsigned c = 0; c < sizeof counts / sizeof counts[0]; c++) {
        int n = counts[c];
        ACCEL *a = calloc(n, sizeof *a);
        for (int i = 0; i < n; i++) { a[i].fVirt = FVIRTKEY; a[i].key = (WORD)(0x30 + i % 10); a[i].cmd = (WORD)(i + 1); }
        HACCEL h = U(CreateAcceleratorTableW)(a, n);
        CHECKF(h != NULL, "n=%d", n);
        if (h) {
            CHECKF(U(CopyAcceleratorTableW)(h, NULL, 0) == n, "count n=%d", n);
            ACCEL *o = calloc(n, sizeof *o);
            CHECKF(U(CopyAcceleratorTableW)(h, o, n) == n, "copy n=%d", n);
            int bad = 0;
            for (int i = 0; i < n; i++) if (o[i].cmd != a[i].cmd || o[i].key != a[i].key) bad++;
            CHECKF(bad == 0, "n=%d mismatches %d", n, bad);
            free(o);
            U(DestroyAcceleratorTable)(h);
        }
        free(a);
    }
}
TEST(accel_huge_table_observed) {
    NEED();
    int n = 20000;
    ACCEL *a = calloc(n, sizeof *a);
    for (int i = 0; i < n; i++) { a[i].fVirt = FVIRTKEY; a[i].key = 'A'; a[i].cmd = (WORD)i; }
    SetLastError(0);
    HACCEL h = U(CreateAcceleratorTableW)(a, n);
    t_note("CreateAcceleratorTableW(%d entries) -> %p err=%lu", n, h, GetLastError());
    if (h) {
        t_note("copy count = %d", U(CopyAcceleratorTableW)(h, NULL, 0));
        U(DestroyAcceleratorTable)(h);
    }
    free(a);
}
TEST(accel_create_destroy_loop) {
    NEED();
    int bad = 0;
    for (int i = 0; i < 5000; i++) {
        HACCEL h = mk1(FVIRTKEY, 'A' + i % 26, (WORD)i);
        if (!h || !U(DestroyAcceleratorTable)(h)) { bad++; break; }
    }
    EQ(bad, 0);
}
TEST(accel_handle_leak_pattern) {
    NEED();
    HACCEL hs[64];
    for (int i = 0; i < 64; i++) hs[i] = mk1(FVIRTKEY, 'A', (WORD)i);
    for (int i = 0; i < 64; i++) CHECKF(hs[i] != NULL, "%d", i);
    for (int i = 63; i >= 0; i--) if (hs[i]) CHECKF(U(DestroyAcceleratorTable)(hs[i]), "%d", i);
}

/* ---------- LoadAccelerators ---------- */
TEST(accel_load_missing_a) {
    REQUIRE(U(LoadAcceleratorsA));
    SetLastError(0);
    HACCEL h = U(LoadAcceleratorsA)(GetModuleHandleA(NULL), "T32NoSuchAccel");
    EQ(h, NULL);
    DWORD e = GetLastError();
    t_note("LoadAcceleratorsA missing name err=%lu", e);
    CHECK(e == ERROR_RESOURCE_DATA_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_NAME_NOT_FOUND);
}
TEST(accel_load_missing_w) {
    REQUIRE(U(LoadAcceleratorsW));
    SetLastError(0);
    EQ(U(LoadAcceleratorsW)(GetModuleHandleA(NULL), L"T32NoSuchAccel"), NULL);
    DWORD e = GetLastError();
    CHECK(e == ERROR_RESOURCE_DATA_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_NAME_NOT_FOUND);
}
TEST(accel_load_missing_by_id) {
    REQUIRE(U(LoadAcceleratorsA)); REQUIRE(U(LoadAcceleratorsW));
    static const WORD ids[] = {1, 100, 0x7777, 0xFFFF};
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        CHECKF(U(LoadAcceleratorsA)(GetModuleHandleA(NULL), MAKEINTRESOURCEA(ids[i])) == NULL, "A id %u", ids[i]);
        CHECKF(U(LoadAcceleratorsW)(GetModuleHandleA(NULL), MAKEINTRESOURCEW(ids[i])) == NULL, "W id %u", ids[i]);
    }
}
TEST(accel_load_null_name) {
    REQUIRE(U(LoadAcceleratorsA)); REQUIRE(U(LoadAcceleratorsW));
    SetLastError(0);
    EQ(U(LoadAcceleratorsA)(GetModuleHandleA(NULL), NULL), NULL);
    EQ(U(LoadAcceleratorsW)(GetModuleHandleA(NULL), NULL), NULL);
}
TEST(accel_load_from_bad_module) {
    REQUIRE(U(LoadAcceleratorsA)); REQUIRE(U(LoadAcceleratorsW));
    SetLastError(0);
    EQ(U(LoadAcceleratorsA)((HINSTANCE)(ULONG_PTR)0x10000, "X"), NULL);
    EQ(U(LoadAcceleratorsW)((HINSTANCE)(ULONG_PTR)0x10000, L"X"), NULL);
}
TEST(accel_load_from_user32) {
    REQUIRE(U(LoadAcceleratorsA));
    HMODULE u = t_user32();
    HACCEL h = U(LoadAcceleratorsA)(u, "T32NoSuchAccel");
    EQ(h, NULL);
}

/* ---------- TranslateAccelerator ---------- */
TEST(accel_translate_simple_virtkey) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    LRESULT r = ta(w, h, WM_KEYDOWN, 'A');
    CHECK(r != 0);
    EQ(g_cmd_count, 1);
    EQ(LOWORD(g_cmd_wp), 100);
    EQ(HIWORD(g_cmd_wp), 1);
    EQ(g_cmd_lp, 0);
    EQ(g_cmd_hwnd, w);
    EQ(g_sys_count, 0);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_w_variant) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 101);
    CHECK(taw(w, h, WM_KEYDOWN, 'A') != 0);
    EQ(g_cmd_count, 1);
    EQ(LOWORD(g_cmd_wp), 101);
    EQ(HIWORD(g_cmd_wp), 1);
    EQ(taw(w, h, WM_KEYDOWN, 'B'), 0);
    EQ(g_cmd_count, 0);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_nonmatching_key) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    for (WPARAM k = 0; k < 256; k++) {
        if (k == 'A') continue;
        LRESULT r = ta(w, h, WM_KEYDOWN, k);
        if (r != 0 || g_cmd_count != 0) { CHECKF(0, "key 0x%x translated", (unsigned)k); break; }
    }
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_message_filter) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    /* Only KEYDOWN, CHAR, SYSKEYDOWN and SYSCHAR are ever examined. */
    static const UINT ignored[] = {WM_KEYUP, WM_SYSKEYUP, WM_DEADCHAR, WM_SYSDEADCHAR, WM_PAINT, WM_NULL, WM_COMMAND, WM_TIMER, WM_LBUTTONDOWN, WM_USER, WM_IME_KEYDOWN, WM_UNICHAR};
    for (unsigned i = 0; i < sizeof ignored / sizeof ignored[0]; i++) {
        LRESULT r = ta(w, h, ignored[i], 'A');
        CHECKF(r == 0 && g_cmd_count == 0, "msg 0x%x", ignored[i]);
    }
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_char_does_not_match_virtkey_entry) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    EQ(ta(w, h, WM_CHAR, 'A'), 0);
    EQ(g_cmd_count, 0);
    EQ(ta(w, h, WM_CHAR, 'a'), 0);
    EQ(g_cmd_count, 0);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_char_entry) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(0, 'x', 200);
    CHECK(ta(w, h, WM_CHAR, 'x') != 0);
    EQ(g_cmd_count, 1);
    EQ(LOWORD(g_cmd_wp), 200);
    EQ(HIWORD(g_cmd_wp), 1);
    EQ(ta(w, h, WM_KEYDOWN, 'X'), 0);
    EQ(g_cmd_count, 0);
    EQ(ta(w, h, WM_CHAR, 'y'), 0);
    LRESULT r = ta(w, h, WM_CHAR, 'X');
    t_note("char entry 'x' vs WM_CHAR 'X' -> %lld", (long long)r);
    r = ta(w, h, WM_SYSCHAR, 'x');
    t_note("char entry 'x' vs WM_SYSCHAR 'x' -> %lld", (long long)r);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_char_entry_w) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    ACCEL a __attribute__((aligned(8))) = {0, 0x20AC, 201};
    HACCEL h = U(CreateAcceleratorTableW)(&a, 1);
    LRESULT r = taw(w, h, WM_CHAR, 0x20AC);
    CHECK(r != 0);
    EQ(g_cmd_count, 1);
    EQ(LOWORD(g_cmd_wp), 201);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_modifier_required_not_held) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    static const BYTE mods[] = {FCONTROL, FSHIFT, FALT, FCONTROL | FSHIFT, FCONTROL | FALT, FSHIFT | FALT, FCONTROL | FSHIFT | FALT};
    for (unsigned i = 0; i < sizeof mods; i++) {
        HACCEL h = mk1(FVIRTKEY | mods[i], 'A', 300);
        LRESULT r = ta(w, h, WM_KEYDOWN, 'A');
        CHECKF(r == 0 && g_cmd_count == 0, "modifiers 0x%x translated without being held", mods[i]);
        U(DestroyAcceleratorTable)(h);
    }
    killwnd(w);
}
TEST(accel_translate_noinvert_flag) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY | FNOINVERT, 'Z', 301);
    CHECK(ta(w, h, WM_KEYDOWN, 'Z') != 0);
    EQ(LOWORD(g_cmd_wp), 301);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_first_match_wins) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    ACCEL a[3] __attribute__((aligned(8))) = {{FVIRTKEY, 'M', 11}, {FVIRTKEY, 'M', 22}, {FVIRTKEY, 'N', 33}};
    HACCEL h = U(CreateAcceleratorTableA)(a, 3);
    CHECK(ta(w, h, WM_KEYDOWN, 'M') != 0);
    EQ(g_cmd_count, 1);
    EQ(LOWORD(g_cmd_wp), 11);
    CHECK(ta(w, h, WM_KEYDOWN, 'N') != 0);
    EQ(LOWORD(g_cmd_wp), 33);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_function_keys_table) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    static const WORD keys[] = {VK_F1, VK_F5, VK_F12, VK_F24, VK_ESCAPE, VK_RETURN, VK_TAB, VK_SPACE, VK_DELETE, VK_INSERT, VK_HOME, VK_END,
                                VK_PRIOR, VK_NEXT, VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN, VK_BACK, VK_NUMPAD0, VK_NUMPAD9, VK_ADD, VK_DECIMAL,
                                VK_OEM_PLUS, VK_OEM_MINUS, VK_OEM_COMMA, VK_OEM_PERIOD, '0', '9', 'Z'};
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        HACCEL h = mk1(FVIRTKEY, keys[i], (WORD)(1000 + i));
        LRESULT r = ta(w, h, WM_KEYDOWN, keys[i]);
        CHECKF(r != 0 && g_cmd_count == 1 && LOWORD(g_cmd_wp) == 1000 + i, "vk 0x%x", keys[i]);
        U(DestroyAcceleratorTable)(h);
    }
    killwnd(w);
}
TEST(accel_translate_null_args) {
    NEED();
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    MSG m; memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_KEYDOWN; m.wParam = 'A';
    reset_cmd();
    EQ(U(TranslateAcceleratorA)(NULL, h, &m), 0);
    EQ(U(TranslateAcceleratorW)(NULL, h, &m), 0);
    EQ(g_cmd_count, 0);
    EQ(U(TranslateAcceleratorA)(w, NULL, &m), 0);
    EQ(U(TranslateAcceleratorW)(w, NULL, &m), 0);
    EQ(g_cmd_count, 0);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_invalid_window) {
    NEED();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    MSG m; memset(&m, 0, sizeof m); m.message = WM_KEYDOWN; m.wParam = 'A';
    SetLastError(0);
    EQ(U(TranslateAcceleratorA)((HWND)0x12345678, h, &m), 0);
    t_note("TranslateAccelerator invalid hwnd err=%lu", GetLastError());
    U(DestroyAcceleratorTable)(h);
}
TEST(accel_translate_invalid_table) {
    NEED();
    HWND w = mkwnd();
    MSG m; memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_KEYDOWN; m.wParam = 'A';
    SetLastError(0);
    EQ(U(TranslateAcceleratorA)(w, (HACCEL)0x1234, &m), 0);
    t_note("TranslateAccelerator bad table err=%lu", GetLastError());
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    U(DestroyAcceleratorTable)(h);
    SetLastError(0);
    EQ(U(TranslateAcceleratorA)(w, h, &m), 0);
    t_note("TranslateAccelerator destroyed table err=%lu", GetLastError());
    EQ(g_cmd_count, 0);
    killwnd(w);
}
TEST(accel_translate_wparam_preserved) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    MSG m; memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_KEYDOWN; m.wParam = 'A'; m.lParam = 0x1E0001;
    U(TranslateAcceleratorA)(w, h, &m);
    EQ(m.wParam, 'A'); EQ(m.lParam, 0x1E0001); EQ(m.message, WM_KEYDOWN); EQ(m.hwnd, w);
    m.message = WM_CHAR; m.wParam = 'q';
    U(TranslateAcceleratorA)(w, h, &m);
    EQ(m.wParam, 'q');
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_posted_message_loop) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    ACCEL a[2] __attribute__((aligned(8))) = {{FVIRTKEY, VK_F7, 701}, {FVIRTKEY, VK_F8, 702}};
    HACCEL h = U(CreateAcceleratorTableA)(a, 2);
    /* A private window that is never shown does not receive key messages from the system, so these
     * are synthetic posts. */
    PostMessageA(w, WM_KEYDOWN, VK_F7, 1);
    PostMessageA(w, WM_KEYDOWN, VK_F9, 1);
    PostMessageA(w, WM_KEYDOWN, VK_F8, 1);
    PostMessageA(w, WM_KEYUP, VK_F8, 1);
    int translated = 0, passed = 0, seen = 0;
    MSG m;
    DWORD end = GetTickCount() + 2000;
    while (seen < 4 && (LONG)(GetTickCount() - end) < 0) {
        if (!PeekMessageA(&m, w, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) { Sleep(1); continue; }
        seen++;
        if (U(TranslateAcceleratorA)(w, h, &m)) translated++;
        else { passed++; TranslateMessage(&m); DispatchMessageA(&m); }
    }
    EQ(seen, 4);
    EQ(translated, 2);
    EQ(passed, 2);
    EQ(g_cmd_count, 2);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_from_child_window) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HWND child = U(CreateWindowExA)(0, ACLS, "c", WS_CHILD | WS_VISIBLE, 0, 0, 20, 20, w, (HMENU)5, GetModuleHandleA(NULL), NULL);
    CHECK(child != NULL);
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    reset_cmd();
    MSG m; memset(&m, 0, sizeof m); m.hwnd = child; m.message = WM_KEYDOWN; m.wParam = 'A';
    LRESULT r = U(TranslateAcceleratorA)(w, h, &m);
    CHECK(r != 0);
    EQ(g_cmd_hwnd, w);        /* WM_COMMAND goes to the window passed in, not msg.hwnd */
    EQ(LOWORD(g_cmd_wp), 100);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_disabled_window) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    EnableWindow(w, FALSE);
    LRESULT r = ta(w, h, WM_KEYDOWN, 'A');
    t_note("TranslateAccelerator on disabled window -> %lld, WM_COMMAND count %d", (long long)r, g_cmd_count);
    EnableWindow(w, TRUE);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_system_command_range) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    /* A command id that names a system-menu item is delivered as WM_SYSCOMMAND on Windows. */
    HACCEL h = mk1(FVIRTKEY, VK_F11, SC_MINIMIZE);
    LRESULT r = ta(w, h, WM_KEYDOWN, VK_F11);
    t_note("accelerator cmd=SC_MINIMIZE -> ret %lld, WM_COMMAND=%d WM_SYSCOMMAND=%d (wp=0x%llx)", (long long)r, g_cmd_count, g_sys_count, (unsigned long long)g_sys_wp);
    CHECK(g_cmd_count + g_sys_count <= 1);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_syskey_without_alt_entry) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    LRESULT r = ta(w, h, WM_SYSKEYDOWN, 'A');
    t_note("WM_SYSKEYDOWN 'A' vs plain entry -> %lld (cmd count %d)", (long long)r, g_cmd_count);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_repeat_calls) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'R', 55);
    int ok = 0;
    for (int i = 0; i < 1000; i++) if (ta(w, h, WM_KEYDOWN, 'R') && g_cmd_count == 1 && LOWORD(g_cmd_wp) == 55) ok++;
    EQ(ok, 1000);
    U(DestroyAcceleratorTable)(h);
    killwnd(w);
}
TEST(accel_translate_after_window_destroy) {
    NEED();
    HWND w = mkwnd();
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    killwnd(w);
    MSG m; memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_KEYDOWN; m.wParam = 'A';
    SetLastError(0);
    EQ(U(TranslateAcceleratorA)(w, h, &m), 0);
    t_note("TranslateAccelerator on destroyed hwnd err=%lu", GetLastError());
    U(DestroyAcceleratorTable)(h);
}
TEST(accel_translate_table_survives_window) {
    NEED();
    if (mods_down()) { t_skip("modifier key held"); return; }
    HACCEL h = mk1(FVIRTKEY, 'A', 100);
    HWND w1 = mkwnd();
    CHECK(ta(w1, h, WM_KEYDOWN, 'A') != 0);
    killwnd(w1);
    HWND w2 = mkwnd();
    CHECK(ta(w2, h, WM_KEYDOWN, 'A') != 0);
    EQ(g_cmd_hwnd, w2);
    killwnd(w2);
    U(DestroyAcceleratorTable)(h);
}
