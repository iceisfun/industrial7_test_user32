/* Accelerator tables and TranslateAccelerator. */
#include "kbd.h"

#define NT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define ACREQ REQUIRE(U(CreateAcceleratorTableW)); REQUIRE(U(CopyAcceleratorTableW)); REQUIRE(U(DestroyAcceleratorTable))

static ACCEL mk(BYTE f, WORD key, WORD cmd) { ACCEL a; memset(&a, 0, sizeof a); a.fVirt = f; a.key = key; a.cmd = cmd; return a; }

static int same_entry(const ACCEL *a, const ACCEL *b)
{
    return (a->fVirt & 0x1F) == (b->fVirt & 0x1F) && a->key == b->key && a->cmd == b->cmd;
}

TEST(input_kbd_accel_create_copy_single) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100), out[4];
    HACCEL h;
    ACREQ;
    h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, NULL, 0), 1);
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, NULL, 100), 1);
    memset(out, 0xCC, sizeof out);
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, out, 4), 1);
    CHECK(same_entry(&in, &out[0]));
    EXPECT_EQ(out[1].key, 0xCCCC);          /* only one entry written */
    CHECK(U(DestroyAcceleratorTable)(h));
}
TEST(input_kbd_accel_create_copy_order_preserved) {
    ACCEL in[6], out[8];
    HACCEL h;
    int i;
    ACREQ;
    in[0] = mk(FVIRTKEY, VK_F1, 1); in[1] = mk(FVIRTKEY | FCONTROL, 'S', 2); in[2] = mk(FVIRTKEY | FALT | FSHIFT, VK_DELETE, 3);
    in[3] = mk(0, 'q', 4); in[4] = mk(FVIRTKEY | FNOINVERT, VK_ESCAPE, 5); in[5] = mk(FVIRTKEY | FCONTROL | FALT | FSHIFT, 'Z', 0xFFFF);
    h = U(CreateAcceleratorTableW)(in, 6);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, NULL, 0), 6);
    memset(out, 0, sizeof out);
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, out, 8), 6);
    for (i = 0; i < 6; i++) CHECKF(same_entry(&in[i], &out[i]), "entry %d: fVirt %02x key %04x cmd %04x", i, out[i].fVirt, out[i].key, out[i].cmd);
    CHECK(U(DestroyAcceleratorTable)(h));
}
TEST(input_kbd_accel_copy_partial_counts) {
    ACCEL in[5], out[8];
    HACCEL h;
    int i, n;
    ACREQ;
    for (i = 0; i < 5; i++) in[i] = mk(FVIRTKEY, VK_F1 + i, 10 + i);
    h = U(CreateAcceleratorTableW)(in, 5);
    CHECK(h != NULL);
    if (!h) return;
    for (n = 1; n <= 7; n++) {
        int want = n > 5 ? 5 : n;
        memset(out, 0xCC, sizeof out);
        EXPECT_EQ(U(CopyAcceleratorTableW)(h, out, n), want);
        for (i = 0; i < want; i++) CHECK(same_entry(&in[i], &out[i]));
        if (want < 8) EXPECT_EQ(out[want].key, 0xCCCC);
    }
    CHECK(U(DestroyAcceleratorTable)(h));
}
TEST(input_kbd_accel_copy_zero_count_with_buffer_observed) {
    ACCEL in = mk(FVIRTKEY, VK_F2, 5), out[2];
    HACCEL h;
    int r;
    ACREQ;
    h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(h != NULL);
    if (!h) return;
    memset(out, 0xCC, sizeof out);
    r = U(CopyAcceleratorTableW)(h, out, 0);
    t_note("CopyAcceleratorTableW(h, buf, 0) = %d, buf[0].key=%04x", r, out[0].key);
    CHECK(out[0].key == 0xCCCC);
    U(DestroyAcceleratorTable)(h);
}
TEST(input_kbd_accel_create_invalid_counts) {
    ACCEL in = mk(FVIRTKEY, VK_F2, 5);
    ACREQ;
    SET_ERR(0);
    CHECK(U(CreateAcceleratorTableW)(&in, 0) == NULL);
    t_note("CreateAcceleratorTableW(cAccel=0) error %lu", GetLastError());
    SET_ERR(0);
    CHECK(U(CreateAcceleratorTableW)(&in, -1) == NULL);
    t_note("CreateAcceleratorTableW(cAccel=-1) error %lu", GetLastError());
    SET_ERR(0);
    CHECK(U(CreateAcceleratorTableA)(&in, 0) == NULL);
}
TEST(input_kbd_accel_handles_distinct) {
    ACCEL in = mk(FVIRTKEY, VK_F3, 6);
    HACCEL a, b;
    ACREQ;
    a = U(CreateAcceleratorTableW)(&in, 1);
    b = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(a != NULL && b != NULL);
    CHECK(a != b);
    if (a) CHECK(U(DestroyAcceleratorTable)(a));
    if (b) CHECK(U(DestroyAcceleratorTable)(b));
}
TEST(input_kbd_accel_destroy_twice) {
    ACCEL in = mk(FVIRTKEY, VK_F3, 6);
    HACCEL h;
    ACREQ;
    h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(DestroyAcceleratorTable)(h));
    SET_ERR(0);
    CHECK(!U(DestroyAcceleratorTable)(h));
    NOTE_ERR(ERROR_INVALID_ACCEL_HANDLE);
}
TEST(input_kbd_accel_destroy_invalid) {
    static const ULONG_PTR v[] = { 0, 1, 0x1234, 0xDEADBEEF, 0xFFFFFFFF };
    int i;
    ACREQ;
    for (i = 0; i < NT(v); i++) {
        SET_ERR(0);
        CHECKF(!U(DestroyAcceleratorTable)((HACCEL)v[i]), "handle %p accepted", (void *)v[i]);
        NOTE_ERR(ERROR_INVALID_ACCEL_HANDLE);
    }
}
TEST(input_kbd_accel_copy_invalid_handle) {
    ACCEL out[2];
    ACREQ;
    SET_ERR(0);
    EXPECT_EQ(U(CopyAcceleratorTableW)(NULL, out, 2), 0);
    NOTE_ERR(ERROR_INVALID_ACCEL_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(U(CopyAcceleratorTableW)((HACCEL)(ULONG_PTR)0x1234, out, 2), 0);
    NOTE_ERR(ERROR_INVALID_ACCEL_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(U(CopyAcceleratorTableW)((HACCEL)(ULONG_PTR)0x1234, NULL, 0), 0);
    NOTE_ERR(ERROR_INVALID_ACCEL_HANDLE);
}
TEST(input_kbd_accel_copy_destroyed_handle) {
    ACCEL in = mk(FVIRTKEY, VK_F4, 7), out[2];
    HACCEL h;
    ACREQ;
    h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(DestroyAcceleratorTable)(h));
    SET_ERR(0);
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, out, 2), 0);
    NOTE_ERR(ERROR_INVALID_ACCEL_HANDLE);
}
TEST(input_kbd_accel_large_table) {
    ACCEL *in, *out;
    HACCEL h;
    int i, bad = 0;
    ACREQ;
    in = calloc(1000, sizeof(ACCEL)); out = calloc(1000, sizeof(ACCEL));
    CHECK(in && out);
    if (!in || !out) { free(in); free(out); return; }
    for (i = 0; i < 1000; i++) in[i] = mk(FVIRTKEY | ((i & 1) ? FCONTROL : 0), 'A' + (i % 26), i + 1);
    h = U(CreateAcceleratorTableW)(in, 1000);
    CHECK(h != NULL);
    if (h) {
        EXPECT_EQ(U(CopyAcceleratorTableW)(h, NULL, 0), 1000);
        EXPECT_EQ(U(CopyAcceleratorTableW)(h, out, 1000), 1000);
        for (i = 0; i < 1000; i++) if (!same_entry(&in[i], &out[i])) bad++;
        CHECK(bad == 0);
        CHECK(U(DestroyAcceleratorTable)(h));
    }
    free(in); free(out);
}
TEST(input_kbd_accel_many_create_destroy_cycles) {
    ACCEL in = mk(FVIRTKEY | FCONTROL, 'N', 9);
    int i, bad = 0;
    ACREQ;
    for (i = 0; i < 2000; i++) {
        HACCEL h = U(CreateAcceleratorTableW)(&in, 1);
        if (!h || !U(DestroyAcceleratorTable)(h)) bad++;
    }
    CHECK(bad == 0);
}
TEST(input_kbd_accel_flag_bits_roundtrip) {
    ACCEL in[5], out[5];
    HACCEL h;
    int i;
    ACREQ;
    in[0] = mk(FVIRTKEY, VK_F5, 1); in[1] = mk(FVIRTKEY | FNOINVERT, VK_F5, 2); in[2] = mk(FVIRTKEY | FSHIFT, VK_F5, 3);
    in[3] = mk(FVIRTKEY | FCONTROL, VK_F5, 4); in[4] = mk(FVIRTKEY | FALT, VK_F5, 5);
    h = U(CreateAcceleratorTableW)(in, 5);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, out, 5), 5);
    for (i = 0; i < 5; i++) EXPECT_EQ(out[i].fVirt & 0x1F, in[i].fVirt);
    U(DestroyAcceleratorTable)(h);
}
TEST(input_kbd_accel_ansi_roundtrip_ascii) {
    ACCEL in[3], outA[3], outW[3];
    HACCEL h;
    ACREQ; REQUIRE(U(CreateAcceleratorTableA)); REQUIRE(U(CopyAcceleratorTableA));
    in[0] = mk(0, 'a', 1); in[1] = mk(FVIRTKEY, 'B', 2); in[2] = mk(0, '7', 3);
    h = U(CreateAcceleratorTableA)(in, 3);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(CopyAcceleratorTableA)(h, NULL, 0), 3);
    EXPECT_EQ(U(CopyAcceleratorTableA)(h, outA, 3), 3);
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, outW, 3), 3);
    CHECK(same_entry(&in[0], &outA[0])); CHECK(same_entry(&in[1], &outA[1])); CHECK(same_entry(&in[2], &outA[2]));
    EXPECT_EQ(outW[0].key, 'a'); EXPECT_EQ(outW[1].key, 'B'); EXPECT_EQ(outW[2].key, '7');
    U(DestroyAcceleratorTable)(h);
}
TEST(input_kbd_accel_wide_table_read_as_ansi) {
    ACCEL in[2], out[2];
    HACCEL h;
    ACREQ; REQUIRE(U(CopyAcceleratorTableA));
    in[0] = mk(0, 'z', 11); in[1] = mk(FVIRTKEY, VK_F6, 12);
    h = U(CreateAcceleratorTableW)(in, 2);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(CopyAcceleratorTableA)(h, out, 2), 2);
    EXPECT_EQ(out[0].key, 'z');
    EXPECT_EQ(out[1].key, VK_F6);    /* virtual keys are not converted */
    U(DestroyAcceleratorTable)(h);
}
TEST(input_kbd_accel_ansi_high_char_observed) {
    ACCEL in = mk(0, 0xE9, 1), out;
    HACCEL h;
    ACREQ; REQUIRE(U(CreateAcceleratorTableA)); REQUIRE(U(CopyAcceleratorTableW));
    h = U(CreateAcceleratorTableA)(&in, 1);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(U(CopyAcceleratorTableW)(h, &out, 1), 1);
    t_note("ANSI 0xE9 accelerator became U+%04X (ACP %u)", out.key, GetACP());
    U(DestroyAcceleratorTable)(h);
}
TEST(input_kbd_accel_load_missing_resource) {
    HACCEL h;
    HMODULE exe = GetModuleHandleA(NULL);
    REQUIRE(U(LoadAcceleratorsW)); REQUIRE(U(LoadAcceleratorsA));
    SET_ERR(0);
    h = U(LoadAcceleratorsW)(exe, L"NO_SUCH_ACCEL");
    CHECK(h == NULL);
    t_note("LoadAcceleratorsW(missing) error %lu", GetLastError());
    h = U(LoadAcceleratorsA)(exe, "NO_SUCH_ACCEL");
    CHECK(h == NULL);
    h = U(LoadAcceleratorsW)(exe, MAKEINTRESOURCEW(4321));
    CHECK(h == NULL);
    h = U(LoadAcceleratorsA)(exe, MAKEINTRESOURCEA(4321));
    CHECK(h == NULL);
    h = U(LoadAcceleratorsW)(t_user32(), L"NO_SUCH_ACCEL");
    CHECK(h == NULL);
}
TEST(input_kbd_accel_load_bad_module) {
    HACCEL h;
    REQUIRE(U(LoadAcceleratorsW));
    h = U(LoadAcceleratorsW)((HINSTANCE)(ULONG_PTR)0x10000, MAKEINTRESOURCEW(1));
    CHECK(h == NULL);
}

/* ---- TranslateAccelerator ---- */
static HWND ta_window(void)
{
    HWND w = kw_plain();
    if (w) U(ShowWindow)(w, SW_SHOWNOACTIVATE);
    kw_clear();
    return w;
}
static void ta_done(HWND w) { if (w) U(DestroyWindow)(w); kw_clear(); }
static MSG mkmsg(HWND w, UINT m, WPARAM wp, LPARAM lp) { MSG g; memset(&g, 0, sizeof g); g.hwnd = w; g.message = m; g.wParam = wp; g.lParam = lp; g.time = GetTickCount(); return g; }
#define TAREQ ACREQ; REQUIRE(U(TranslateAcceleratorW)); REQUIRE(U(TranslateAcceleratorA)); REQUIRE(U(ShowWindow))

TEST(input_kbd_ta_virtkey_match_sends_command) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    HWND w;
    MSG m;
    struct klog *c;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);
        CHECK(U(TranslateAcceleratorW)(w, h, &m) != 0);
        c = kw_find(WM_COMMAND, 0);
        CHECK(c != NULL);
        if (c) { EXPECT_EQ(LOWORD(c->wp), 100); EXPECT_EQ(HIWORD(c->wp), 1); EXPECT_EQ(c->lp, 0); }
        EXPECT_EQ(kw_count(WM_COMMAND), 1);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_ansi_variant_matches) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 101);
    HACCEL h;
    HWND w;
    MSG m;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);
        CHECK(U(TranslateAcceleratorA)(w, h, &m) != 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 1);
        if (kw_find(WM_COMMAND, 0)) EXPECT_EQ(LOWORD(kw_find(WM_COMMAND, 0)->wp), 101);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_msg_not_modified) {
    ACCEL in[2] = { mk(FVIRTKEY, VK_F9, 100), mk(0, 'x', 102) };
    HACCEL h;
    HWND w;
    MSG m, c;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(in, 2);
    CHECK(w && h);
    if (w && h) {
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 0x12345);
        c = m;
        U(TranslateAcceleratorW)(w, h, &m);
        CHECK(!memcmp(&m, &c, sizeof m));
        m = mkmsg(w, WM_CHAR, 'x', 1); c = m;
        U(TranslateAcceleratorA)(w, h, &m);
        CHECK(!memcmp(&m, &c, sizeof m));
        m = mkmsg(w, WM_CHAR, 'x', 1); c = m;
        U(TranslateAcceleratorW)(w, h, &m);
        CHECK(!memcmp(&m, &c, sizeof m));
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_nonmatching_key) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    HWND w;
    MSG m;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        m = mkmsg(w, WM_KEYDOWN, VK_F8, 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        m = mkmsg(w, WM_KEYDOWN, 'A', 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_modifier_required) {
    ACCEL in[3] = { mk(FVIRTKEY | FCONTROL, 'A', 1), mk(FVIRTKEY | FALT, VK_F9, 2), mk(FVIRTKEY | FSHIFT, VK_F10, 3) };
    HACCEL h;
    HWND w;
    MSG m;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(in, 3);
    CHECK(w && h);
    if (w && h) {
        /* no modifier is held in this thread's key state */
        m = mkmsg(w, WM_KEYDOWN, 'A', 1);       EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);     EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        m = mkmsg(w, WM_KEYDOWN, VK_F10, 1);    EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_ascii_entry_matches_wm_char) {
    ACCEL in = mk(0, 'x', 103);
    HACCEL h;
    HWND w;
    MSG m;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        m = mkmsg(w, WM_CHAR, 'x', 1);
        CHECK(U(TranslateAcceleratorW)(w, h, &m) != 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 1);
        if (kw_find(WM_COMMAND, 0)) { EXPECT_EQ(LOWORD(kw_find(WM_COMMAND, 0)->wp), 103); EXPECT_EQ(HIWORD(kw_find(WM_COMMAND, 0)->wp), 1); }
        kw_clear();
        m = mkmsg(w, WM_CHAR, 'X', 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        m = mkmsg(w, WM_CHAR, 'y', 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_only_key_down_style_messages) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    HWND w;
    MSG m;
    static const UINT msgs[] = { WM_KEYUP, WM_SYSKEYUP, WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_COMMAND, WM_TIMER, WM_NULL, WM_DEADCHAR, WM_SYSDEADCHAR, WM_KEYFIRST - 1, WM_USER };
    int i;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        for (i = 0; i < NT(msgs); i++) {
            m = mkmsg(w, msgs[i], VK_F9, 1);
            CHECKF(U(TranslateAcceleratorW)(w, h, &m) == 0, "message 0x%x translated", msgs[i]);
        }
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_null_window) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    MSG m;
    TAREQ;
    h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(h != NULL);
    if (h) {
        m = mkmsg(NULL, WM_KEYDOWN, VK_F9, 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(NULL, h, &m), 0);
        EXPECT_EQ(U(TranslateAcceleratorA)(NULL, h, &m), 0);
        U(DestroyAcceleratorTable)(h);
    }
}
TEST(input_kbd_ta_null_and_bogus_table) {
    HWND w;
    MSG m;
    TAREQ;
    w = ta_window();
    CHECK(w != NULL);
    if (w) {
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, NULL, &m), 0);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, (HACCEL)(ULONG_PTR)0x1234, &m), 0);
        EXPECT_EQ(U(TranslateAcceleratorA)(w, (HACCEL)(ULONG_PTR)0xDEADBEEF, &m), 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    ta_done(w);
}
TEST(input_kbd_ta_destroyed_table) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    HWND w;
    MSG m;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        CHECK(U(DestroyAcceleratorTable)(h));
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
    }
    ta_done(w);
}
TEST(input_kbd_ta_bogus_window) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    MSG m;
    TAREQ;
    h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(h != NULL);
    if (h) {
        m = mkmsg((HWND)(ULONG_PTR)0x1234, WM_KEYDOWN, VK_F9, 1);
        EXPECT_EQ(U(TranslateAcceleratorW)((HWND)(ULONG_PTR)0x1234, h, &m), 0);
        U(DestroyAcceleratorTable)(h);
    }
}
TEST(input_kbd_ta_destroyed_window) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    HWND w;
    MSG m;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        U(DestroyWindow)(w);
        kw_clear();
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);
        EXPECT_EQ(U(TranslateAcceleratorW)(w, h, &m), 0);
        EXPECT_EQ(kw_count(WM_COMMAND), 0);
        w = NULL;
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_repeat_flags_observed) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    HWND w;
    MSG m;
    int r1, r2;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);
        r1 = U(TranslateAcceleratorW)(w, h, &m);
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 0x40000001);   /* auto-repeat */
        r2 = U(TranslateAcceleratorW)(w, h, &m);
        CHECK(r1 != 0);
        t_note("auto-repeat WM_KEYDOWN translated: %d", r2);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_first_matching_entry_wins_observed) {
    ACCEL in[2] = { mk(FVIRTKEY, VK_F9, 201), mk(FVIRTKEY, VK_F9, 202) };
    HACCEL h;
    HWND w;
    MSG m;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(in, 2);
    CHECK(w && h);
    if (w && h) {
        m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);
        CHECK(U(TranslateAcceleratorW)(w, h, &m) != 0);
        CHECK(kw_count(WM_COMMAND) >= 1);
        if (kw_find(WM_COMMAND, 0)) t_note("duplicate accelerators: command %u (count %d)", LOWORD(kw_find(WM_COMMAND, 0)->wp), kw_count(WM_COMMAND));
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
TEST(input_kbd_ta_many_translate_calls) {
    ACCEL in = mk(FVIRTKEY, VK_F9, 100);
    HACCEL h;
    HWND w;
    MSG m;
    int i, ok = 0;
    TAREQ;
    w = ta_window(); h = U(CreateAcceleratorTableW)(&in, 1);
    CHECK(w && h);
    if (w && h) {
        for (i = 0; i < 500; i++) {
            m = mkmsg(w, WM_KEYDOWN, VK_F9, 1);
            if (U(TranslateAcceleratorW)(w, h, &m)) ok++;
        }
        EXPECT_EQ(ok, 500);
        EXPECT_EQ(kw_count(WM_COMMAND), 500);
    }
    if (h) U(DestroyAcceleratorTable)(h);
    ta_done(w);
}
