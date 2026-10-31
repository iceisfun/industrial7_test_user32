/* Key state queries, keyboard type, misc. keyboard info (no injection). */
#include "kbd.h"
#include <limits.h>

/* ---- VK constants ---- */
struct vkc { const char *n; int v, want; };
#define VKC(x, w) { #x, x, w }

TEST(input_kbd_vk_constants_basic) {
    static const struct vkc t[] = {
        VKC(VK_LBUTTON,1), VKC(VK_RBUTTON,2), VKC(VK_CANCEL,3), VKC(VK_MBUTTON,4), VKC(VK_XBUTTON1,5), VKC(VK_XBUTTON2,6),
        VKC(VK_BACK,8), VKC(VK_TAB,9), VKC(VK_CLEAR,0x0C), VKC(VK_RETURN,0x0D), VKC(VK_SHIFT,0x10), VKC(VK_CONTROL,0x11),
        VKC(VK_MENU,0x12), VKC(VK_PAUSE,0x13), VKC(VK_CAPITAL,0x14), VKC(VK_KANA,0x15), VKC(VK_HANGEUL,0x15), VKC(VK_JUNJA,0x17),
        VKC(VK_FINAL,0x18), VKC(VK_HANJA,0x19), VKC(VK_KANJI,0x19), VKC(VK_ESCAPE,0x1B), VKC(VK_CONVERT,0x1C), VKC(VK_NONCONVERT,0x1D),
        VKC(VK_ACCEPT,0x1E), VKC(VK_MODECHANGE,0x1F), VKC(VK_SPACE,0x20), VKC(VK_PRIOR,0x21), VKC(VK_NEXT,0x22), VKC(VK_END,0x23),
        VKC(VK_HOME,0x24), VKC(VK_LEFT,0x25), VKC(VK_UP,0x26), VKC(VK_RIGHT,0x27), VKC(VK_DOWN,0x28), VKC(VK_SELECT,0x29),
        VKC(VK_PRINT,0x2A), VKC(VK_EXECUTE,0x2B), VKC(VK_SNAPSHOT,0x2C), VKC(VK_INSERT,0x2D), VKC(VK_DELETE,0x2E), VKC(VK_HELP,0x2F),
        VKC(VK_LWIN,0x5B), VKC(VK_RWIN,0x5C), VKC(VK_APPS,0x5D), VKC(VK_SLEEP,0x5F),
        VKC(VK_NUMPAD0,0x60), VKC(VK_NUMPAD9,0x69), VKC(VK_MULTIPLY,0x6A), VKC(VK_ADD,0x6B), VKC(VK_SEPARATOR,0x6C),
        VKC(VK_SUBTRACT,0x6D), VKC(VK_DECIMAL,0x6E), VKC(VK_DIVIDE,0x6F),
        VKC(VK_NUMLOCK,0x90), VKC(VK_SCROLL,0x91),
    };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++)
        CHECKF(t[i].v == t[i].want, "%s = 0x%x want 0x%x", t[i].n, t[i].v, t[i].want);
}
TEST(input_kbd_vk_constants_function_keys) {
    int i;
    CHECK(VK_F1 == 0x70);
    for (i = 0; i < 24; i++) CHECKF(VK_F1 + i == 0x70 + i, "F%d", i + 1);
    CHECK(VK_F10 == 0x79); CHECK(VK_F12 == 0x7B); CHECK(VK_F13 == 0x7C); CHECK(VK_F24 == 0x87);
}
TEST(input_kbd_vk_constants_lr_and_browser) {
    static const struct vkc t[] = {
        VKC(VK_LSHIFT,0xA0), VKC(VK_RSHIFT,0xA1), VKC(VK_LCONTROL,0xA2), VKC(VK_RCONTROL,0xA3), VKC(VK_LMENU,0xA4), VKC(VK_RMENU,0xA5),
        VKC(VK_BROWSER_BACK,0xA6), VKC(VK_BROWSER_FORWARD,0xA7), VKC(VK_BROWSER_REFRESH,0xA8), VKC(VK_BROWSER_STOP,0xA9),
        VKC(VK_BROWSER_SEARCH,0xAA), VKC(VK_BROWSER_FAVORITES,0xAB), VKC(VK_BROWSER_HOME,0xAC),
        VKC(VK_VOLUME_MUTE,0xAD), VKC(VK_VOLUME_DOWN,0xAE), VKC(VK_VOLUME_UP,0xAF),
        VKC(VK_MEDIA_NEXT_TRACK,0xB0), VKC(VK_MEDIA_PREV_TRACK,0xB1), VKC(VK_MEDIA_STOP,0xB2), VKC(VK_MEDIA_PLAY_PAUSE,0xB3),
        VKC(VK_LAUNCH_MAIL,0xB4), VKC(VK_LAUNCH_MEDIA_SELECT,0xB5), VKC(VK_LAUNCH_APP1,0xB6), VKC(VK_LAUNCH_APP2,0xB7),
    };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++)
        CHECKF(t[i].v == t[i].want, "%s = 0x%x want 0x%x", t[i].n, t[i].v, t[i].want);
}
TEST(input_kbd_vk_constants_oem) {
    static const struct vkc t[] = {
        VKC(VK_OEM_1,0xBA), VKC(VK_OEM_PLUS,0xBB), VKC(VK_OEM_COMMA,0xBC), VKC(VK_OEM_MINUS,0xBD), VKC(VK_OEM_PERIOD,0xBE),
        VKC(VK_OEM_2,0xBF), VKC(VK_OEM_3,0xC0), VKC(VK_OEM_4,0xDB), VKC(VK_OEM_5,0xDC), VKC(VK_OEM_6,0xDD), VKC(VK_OEM_7,0xDE),
        VKC(VK_OEM_8,0xDF), VKC(VK_OEM_102,0xE2), VKC(VK_PROCESSKEY,0xE5), VKC(VK_PACKET,0xE7),
        VKC(VK_ATTN,0xF6), VKC(VK_CRSEL,0xF7), VKC(VK_EXSEL,0xF8), VKC(VK_EREOF,0xF9), VKC(VK_PLAY,0xFA), VKC(VK_ZOOM,0xFB),
        VKC(VK_NONAME,0xFC), VKC(VK_PA1,0xFD), VKC(VK_OEM_CLEAR,0xFE),
    };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++)
        CHECKF(t[i].v == t[i].want, "%s = 0x%x want 0x%x", t[i].n, t[i].v, t[i].want);
}
TEST(input_kbd_keyevent_and_mod_constants) {
    CHECK(KEYEVENTF_EXTENDEDKEY == 1); CHECK(KEYEVENTF_KEYUP == 2); CHECK(KEYEVENTF_UNICODE == 4); CHECK(KEYEVENTF_SCANCODE == 8);
    CHECK(MOD_ALT == 1); CHECK(MOD_CONTROL == 2); CHECK(MOD_SHIFT == 4); CHECK(MOD_WIN == 8); CHECK(MOD_NOREPEAT == 0x4000);
    CHECK(FVIRTKEY == 1); CHECK(FNOINVERT == 2); CHECK(FSHIFT == 4); CHECK(FCONTROL == 8); CHECK(FALT == 16);
    CHECK(KL_NAMELENGTH == 9); CHECK(HKL_NEXT == 1); CHECK(HKL_PREV == 0);
    CHECK(KLF_ACTIVATE == 1); CHECK(KLF_SUBSTITUTE_OK == 2); CHECK(KLF_REORDER == 8); CHECK(KLF_REPLACELANG == 0x10);
    CHECK(KLF_NOTELLSHELL == 0x80); CHECK(KLF_SETFORPROCESS == 0x100);
    CHECK(MAPVK_VK_TO_VSC == 0); CHECK(MAPVK_VSC_TO_VK == 1); CHECK(MAPVK_VK_TO_CHAR == 2); CHECK(MAPVK_VSC_TO_VK_EX == 3);
    CHECK(WM_KEYDOWN == 0x100); CHECK(WM_KEYUP == 0x101); CHECK(WM_CHAR == 0x102); CHECK(WM_DEADCHAR == 0x103);
    CHECK(WM_SYSKEYDOWN == 0x104); CHECK(WM_SYSKEYUP == 0x105); CHECK(WM_SYSCHAR == 0x106); CHECK(WM_SYSDEADCHAR == 0x107);
    CHECK(WM_HOTKEY == 0x312); CHECK(WM_UNICHAR == 0x109); CHECK(UNICODE_NOCHAR == 0xFFFF);
    CHECK(sizeof(INPUT) == 40); CHECK(sizeof(KEYBDINPUT) == 24); CHECK(sizeof(LASTINPUTINFO) == 8);
    CHECK(sizeof(ACCEL) == 6);
    CHECK(offsetof(INPUT, ki) == 8);
    CHECK(offsetof(KEYBDINPUT, dwExtraInfo) == 16);
}

/* ---- GetKeyState / GetAsyncKeyState ---- */
TEST(input_kbd_getkeystate_value_shape) {
    int vk, bad = 0;
    REQUIRE(U(GetKeyState));
    for (vk = 0; vk < 256; vk++) {
        SHORT s = U(GetKeyState)(vk);
        int down = s < 0, bit7 = (s >> 7) & 1;
        if ((s & 0x7E) || down != bit7) { bad++; t_note("vk 0x%02x odd GetKeyState 0x%04x", vk, (unsigned short)s); }
    }
    CHECK(bad == 0);
}
TEST(input_kbd_getkeystate_out_of_range) {
    static const int v[] = { -1, 256, 257, 0x1000, 0x10000, 1000000, INT_MAX, INT_MIN, -1000000 };
    int i;
    REQUIRE(U(GetKeyState));
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        SHORT s = U(GetKeyState)(v[i]);
        if (s != 0) t_note("GetKeyState(%d) = 0x%04x (expected 0)", v[i], (unsigned short)s);
    }
    CHECK(1);
}
TEST(input_kbd_getkeystate_vk_zero_and_ff) {
    REQUIRE(U(GetKeyState));
    /* VK 0 and 0xFF are not valid keys; never reported as pressed */
    CHECK(U(GetKeyState)(0) >= 0);
    CHECK(U(GetKeyState)(0xFF) >= 0);
}
TEST(input_kbd_getasynckeystate_out_of_range) {
    REQUIRE(U(GetAsyncKeyState));
    EXPECT_EQ(U(GetAsyncKeyState)(1000000), 0);
    EXPECT_EQ(U(GetAsyncKeyState)(-1000000), 0);
    EXPECT_EQ(U(GetAsyncKeyState)(256), 0);
    EXPECT_EQ(U(GetAsyncKeyState)(0x7FFFFFFF), 0);
    EXPECT_EQ(U(GetAsyncKeyState)((int)0x80000000), 0);
    EXPECT_EQ(U(GetAsyncKeyState)(-1), 0);
}
TEST(input_kbd_getasynckeystate_value_shape) {
    int vk, bad = 0;
    REQUIRE(U(GetAsyncKeyState));
    for (vk = 0; vk < 256; vk++) {
        SHORT s = U(GetAsyncKeyState)(vk);
        if (s & 0x7FFE) { bad++; t_note("vk 0x%02x odd GetAsyncKeyState 0x%04x", vk, (unsigned short)s); }
    }
    CHECK(bad == 0);
}
TEST(input_kbd_getasynckeystate_vk0) {
    REQUIRE(U(GetAsyncKeyState));
    CHECK((U(GetAsyncKeyState)(0) & 0x8000) == 0);
    CHECK((U(GetAsyncKeyState)(0xFF) & 0x8000) == 0);
}
TEST(input_kbd_getkeystate_no_lasterror_clobber) {
    REQUIRE(U(GetKeyState)); REQUIRE(U(GetAsyncKeyState));
    SET_ERR(0x1234);
    U(GetKeyState)(VK_SHIFT);
    U(GetAsyncKeyState)(VK_SHIFT);
    EXPECT_EQ(GetLastError(), 0x1234);
}

/* ---- GetKeyboardState ---- */
TEST(input_kbd_getkeyboardstate_fills_all) {
    BYTE st[256 + 8];
    int i, bad = 0;
    REQUIRE(U(GetKeyboardState));
    memset(st, 0xCC, sizeof st);
    CHECK(U(GetKeyboardState)(st));
    for (i = 0; i < 256; i++) if (st[i] & 0x7E) bad++;
    CHECK(bad == 0);
    for (i = 256; i < 264; i++) EXPECT_EQ(st[i], 0xCC); /* no overrun */
}
TEST(input_kbd_getkeyboardstate_stable_repeat) {
    BYTE a[256], b[256];
    int i, diff = 0;
    REQUIRE(U(GetKeyboardState));
    CHECK(U(GetKeyboardState)(a));
    for (i = 0; i < 20; i++) {
        CHECK(U(GetKeyboardState)(b));
        if (memcmp(a, b, 256)) { diff++; memcpy(a, b, 256); }
    }
    CHECKF(diff <= 3, "state changed %d times without input", diff);
}
TEST(input_kbd_getkeyboardstate_matches_getkeystate) {
    BYTE st[256];
    int vk, bad = 0;
    REQUIRE(U(GetKeyboardState)); REQUIRE(U(GetKeyState));
    CHECK(U(GetKeyboardState)(st));
    for (vk = 7; vk < 256; vk++) {
        SHORT s = U(GetKeyState)(vk);
        if (((s < 0) != !!(st[vk] & 0x80)) || ((s & 1) != (st[vk] & 1))) {
            bad++; t_note("vk 0x%02x mismatch state 0x%02x vs GetKeyState 0x%04x", vk, st[vk], (unsigned short)s);
        }
    }
    CHECK(bad == 0);
}
TEST(input_kbd_getkeyboardstate_generic_consistency) {
    BYTE st[256];
    REQUIRE(U(GetKeyboardState));
    CHECK(U(GetKeyboardState)(st));
    CHECKF(!!(st[VK_SHIFT] & 0x80) == (!!(st[VK_LSHIFT] & 0x80) || !!(st[VK_RSHIFT] & 0x80)), "shift %02x %02x %02x", st[VK_SHIFT], st[VK_LSHIFT], st[VK_RSHIFT]);
    CHECKF(!!(st[VK_CONTROL] & 0x80) == (!!(st[VK_LCONTROL] & 0x80) || !!(st[VK_RCONTROL] & 0x80)), "ctrl");
    CHECKF(!!(st[VK_MENU] & 0x80) == (!!(st[VK_LMENU] & 0x80) || !!(st[VK_RMENU] & 0x80)), "alt");
}
static DWORD WINAPI kst_thread(void *p) {
    BYTE st[256]; int i, bad = 0;
    int *res = p;
    memset(st, 0xCC, sizeof st);
    res[0] = U(GetKeyboardState)(st);
    for (i = 0; i < 256; i++) if (st[i] & 0x7E) bad++;
    res[1] = bad;
    res[2] = U(GetKeyState)(VK_F20) == 0 || 1;
    res[3] = (U(GetAsyncKeyState)(VK_F20) & 0x7FFE) == 0;
    return 0;
}
TEST(input_kbd_getkeyboardstate_worker_thread) {
    int res[4] = {0};
    HANDLE h;
    REQUIRE(U(GetKeyboardState));
    h = CreateThread(NULL, 0, kst_thread, res, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(res[0]);
    CHECK(res[1] == 0);
    CHECK(res[3]);
}
static DWORD WINAPI kst_thread2(void *p) {
    SHORT *r = p;
    int i;
    for (i = 0; i < 256; i++) { SHORT s = U(GetKeyState)(i); if ((s & 0x7E) || (s < 0) != ((s >> 7) & 1)) r[0]++; }
    return 0;
}
TEST(input_kbd_getkeystate_worker_thread_shape) {
    SHORT r[2] = {0, 0};
    HANDLE h;
    REQUIRE(U(GetKeyState));
    h = CreateThread(NULL, 0, kst_thread2, r, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(r[0] == 0);
}
TEST(input_kbd_setkeyboardstate_roundtrip) {
    BYTE orig[256], mod[256], got[256];
    BOOL ok1, ok2;
    REQUIRE(U(GetKeyboardState)); REQUIRE(U(SetKeyboardState));
    CHECK(U(GetKeyboardState)(orig));
    memcpy(mod, orig, 256);
    mod[VK_F20] = 0x81;
    mod[VK_F21] = 0x01;
    ok1 = U(SetKeyboardState)(mod);
    CHECK(ok1);
    CHECK(U(GetKeyboardState)(got));
    if (got[VK_F20] != 0x81 || got[VK_F21] != 0x01)
        t_note("SetKeyboardState not reflected by GetKeyboardState: F20=%02x F21=%02x", got[VK_F20], got[VK_F21]);
    ok2 = U(SetKeyboardState)(orig);
    CHECK(ok2);
    CHECK(U(GetKeyboardState)(got));
    if (got[VK_F20] & 0x80) t_note("F20 still down after restore");
}
TEST(input_kbd_setkeyboardstate_all_zero_restore) {
    BYTE orig[256], z[256], got[256];
    REQUIRE(U(GetKeyboardState)); REQUIRE(U(SetKeyboardState));
    CHECK(U(GetKeyboardState)(orig));
    memset(z, 0, sizeof z);
    CHECK(U(SetKeyboardState)(z));
    CHECK(U(GetKeyboardState)(got));
    CHECK(U(SetKeyboardState)(orig));
    t_note("after zeroing, Shift/Ctrl/Alt bits: %02x %02x %02x", got[VK_SHIFT], got[VK_CONTROL], got[VK_MENU]);
}
TEST(input_kbd_setkeyboardstate_masks_junk_bits) {
    BYTE orig[256], mod[256], got[256];
    REQUIRE(U(GetKeyboardState)); REQUIRE(U(SetKeyboardState));
    CHECK(U(GetKeyboardState)(orig));
    memcpy(mod, orig, 256);
    mod[VK_F22] = 0xFF;
    CHECK(U(SetKeyboardState)(mod));
    CHECK(U(GetKeyboardState)(got));
    CHECKF((got[VK_F22] & 0x7E) == 0, "F22 state 0x%02x", got[VK_F22]);
    CHECK(U(SetKeyboardState)(orig));
}

/* ---- GetKeyboardType ---- */
TEST(input_kbd_keyboardtype_basic) {
    int t, sub, fk;
    REQUIRE(U(GetKeyboardType));
    t = U(GetKeyboardType)(0); sub = U(GetKeyboardType)(1); fk = U(GetKeyboardType)(2);
    CHECKF(t >= 1 && t <= 7, "type %d", t);
    CHECKF(fk > 0 && fk <= 48, "function keys %d", fk);
    t_note("keyboard type %d subtype %d fkeys %d", t, sub, fk);
    if (t == 4) EXPECT_EQ(fk, 12);
    if (t == 1 || t == 3) EXPECT_EQ(fk, 10);
    if (t == 7) CHECK(fk == 12 || fk == 10 || fk == 24);
}
TEST(input_kbd_keyboardtype_invalid_flags) {
    static const int v[] = { 3, 4, 5, 100, 0x100, -1, -100, 0x7FFFFFF0 };
    int i;
    REQUIRE(U(GetKeyboardType));
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        int r = U(GetKeyboardType)(v[i]);
        CHECKF(r == 0, "GetKeyboardType(%d) = %d", v[i], r);
    }
}
TEST(input_kbd_keyboardtype_repeatable) {
    int i, a, b, c;
    REQUIRE(U(GetKeyboardType));
    a = U(GetKeyboardType)(0); b = U(GetKeyboardType)(1); c = U(GetKeyboardType)(2);
    for (i = 0; i < 50; i++) {
        EXPECT_EQ(U(GetKeyboardType)(0), a);
        EXPECT_EQ(U(GetKeyboardType)(1), b);
        EXPECT_EQ(U(GetKeyboardType)(2), c);
    }
}

/* ---- GetKBCodePage / GetLastInputInfo / input source ---- */
TEST(input_kbd_kbcodepage_is_oem) {
    UINT r;
    REQUIRE(U(GetKBCodePage));
    r = U(GetKBCodePage)();
    EXPECT_EQ(r, GetOEMCP());
    CHECK(r != 0);
    EXPECT_EQ(U(GetKBCodePage)(), r);
}
TEST(input_kbd_lastinputinfo_basic) {
    LASTINPUTINFO li;
    DWORD now;
    REQUIRE(U(GetLastInputInfo));
    li.cbSize = sizeof li; li.dwTime = 0xDEADBEEF;
    SET_ERR(0xdeadbeef);
    CHECK(U(GetLastInputInfo)(&li));
    EXPECT_EQ(GetLastError(), 0xdeadbeef);
    now = GetTickCount();
    CHECKF((LONG)(now - li.dwTime) >= 0, "dwTime %lu is in the future of %lu", li.dwTime, now);
    CHECKF(li.dwTime != 0xDEADBEEF, "dwTime not written");
}
TEST(input_kbd_lastinputinfo_bad_size) {
    static const UINT sz[] = { 0, 1, 4, 7, 9, 12, 16, 0x100, 0xFFFFFFFF };
    int i;
    LASTINPUTINFO li[4];
    REQUIRE(U(GetLastInputInfo));
    for (i = 0; i < (int)(sizeof sz / sizeof sz[0]); i++) {
        memset(li, 0x55, sizeof li);
        li[0].cbSize = sz[i];
        SET_ERR(0);
        CHECKF(!U(GetLastInputInfo)(li), "cbSize %u accepted", sz[i]);
        EXPECT_EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(li[0].dwTime, 0x55555555);
    }
}
TEST(input_kbd_lastinputinfo_monotonic) {
    LASTINPUTINFO a, b;
    int i;
    REQUIRE(U(GetLastInputInfo));
    a.cbSize = b.cbSize = sizeof a;
    CHECK(U(GetLastInputInfo)(&a));
    for (i = 0; i < 100; i++) {
        CHECK(U(GetLastInputInfo)(&b));
        CHECKF((LONG)(b.dwTime - a.dwTime) >= 0, "went backwards");
        a = b; a.cbSize = sizeof a;
    }
}
TEST(input_kbd_inputsource_outside_message) {
    INPUT_MESSAGE_SOURCE s;
    REQUIRE(U(GetCurrentInputMessageSource));
    memset(&s, 0x77, sizeof s);
    CHECK(U(GetCurrentInputMessageSource)(&s));
    EXPECT_EQ(s.deviceType, IMDT_UNAVAILABLE);
    EXPECT_EQ(s.originId, IMO_UNAVAILABLE);
}
static DWORD WINAPI src_thread(void *p) {
    INPUT_MESSAGE_SOURCE s; DWORD *r = p;
    memset(&s, 0x77, sizeof s);
    r[0] = U(GetCurrentInputMessageSource)(&s);
    r[1] = s.deviceType; r[2] = s.originId;
    return 0;
}
TEST(input_kbd_inputsource_worker_thread) {
    DWORD r[3] = {0};
    HANDLE h;
    REQUIRE(U(GetCurrentInputMessageSource));
    h = CreateThread(NULL, 0, src_thread, r, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    WaitForSingleObject(h, 5000);
    CloseHandle(h);
    CHECK(r[0]);
    EXPECT_EQ(r[1], IMDT_UNAVAILABLE);
    EXPECT_EQ(r[2], IMO_UNAVAILABLE);
}
TEST(input_kbd_inputsource_posted_key_message) {
    HWND w;
    struct klog *e;
    REQUIRE(U(GetCurrentInputMessageSource)); REQUIRE(U(PostMessageA));
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    kw_clear();
    U(PostMessageA)(w, WM_KEYDOWN, VK_F5, 1);
    U(PostMessageA)(w, WM_KEYUP, VK_F5, 0xC0000001);
    U(SendMessageA)(w, WM_CHAR, 'x', 1);
    kw_pump(300, 3, 0);
    EXPECT_EQ(kw_count(WM_KEYDOWN), 1);
    EXPECT_EQ(kw_count(WM_KEYUP), 1);
    EXPECT_EQ(kw_count(WM_CHAR), 1);
    for (e = g_klog; e < g_klog + g_nklog; e++) {
        if (e->msg == WM_COMMAND) continue;
        EXPECT_EQ(e->dev, IMDT_UNAVAILABLE);
        EXPECT_EQ(e->org, IMO_UNAVAILABLE);
    }
    e = kw_find(WM_KEYDOWN, 0);
    if (e) { EXPECT_EQ(e->wp, VK_F5); EXPECT_EQ(e->lp, 1); }
    e = kw_find(WM_KEYUP, 0);
    if (e) EXPECT_EQ((UINT)e->lp, 0xC0000001);
    U(DestroyWindow)(w);
    kw_clear();
}
TEST(input_kbd_getkeystate_async_independent_of_posted_msgs) {
    HWND w;
    REQUIRE(U(PostMessageA)); REQUIRE(U(GetAsyncKeyState));
    w = kw_plain();
    CHECK(w != NULL);
    if (!w) return;
    U(PostMessageA)(w, WM_KEYDOWN, VK_F16, 1);
    kw_pump(150, 1, 0);
    /* synthetic messages never press keys */
    CHECK((U(GetAsyncKeyState)(VK_F16) & 0x8000) == 0);
    CHECK(U(GetKeyState)(VK_F16) >= 0);
    U(DestroyWindow)(w);
    kw_clear();
}
