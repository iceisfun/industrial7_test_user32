#include "im.h"

#define NEED_INPUT() do { REQUIRE(U(SendInput)); REQUIRE(U(SetCursorPos)); REQUIRE(U(GetCursorPos)); \
    if (!im_input_ok()) { t_skip("no synthetic input delivery on this desktop"); return; } } while (0)

#define VENDOR_PAGE 0xFF42

static UINT n_registered(void) {
    UINT n = 0xabcd;
    UINT r = U(GetRegisteredRawInputDevices)(NULL, &n, sizeof(RAWINPUTDEVICE));
    return (r == 0 || r == (UINT)-1) ? n : r;
}
static void unreg_all_vendor(HWND w) {
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.dwFlags = RIDEV_REMOVE; d.hwndTarget = NULL;
    for (int u = 1; u <= 12; u++) { d.usUsage = u; U(RegisterRawInputDevices)(&d, 1, sizeof d); }
    d.usUsagePage = 1; d.usUsage = 2; U(RegisterRawInputDevices)(&d, 1, sizeof d);
}

TEST(input_mouse_rawinput_constants_and_sizes) {
    CHECK(sizeof(RAWINPUTHEADER) == 24);
    CHECK(sizeof(RAWINPUTDEVICE) == 16);
    CHECK(sizeof(RAWINPUTDEVICELIST) == 16);
    CHECK(sizeof(RAWMOUSE) == 24);
    CHECK(sizeof(RAWKEYBOARD) == 16);
    CHECK(sizeof(RID_DEVICE_INFO) == 32);
    CHECK(sizeof(RID_DEVICE_INFO_MOUSE) == 16);
    CHECK(sizeof(RID_DEVICE_INFO_KEYBOARD) == 24);
    CHECK(RIDEV_REMOVE == 1 && RIDEV_EXCLUDE == 0x10 && RIDEV_PAGEONLY == 0x20 && RIDEV_NOLEGACY == 0x30);
    CHECK(RIDEV_INPUTSINK == 0x100 && RIDEV_CAPTUREMOUSE == 0x200 && RIDEV_NOHOTKEYS == 0x200 && RIDEV_APPKEYS == 0x400 && RIDEV_DEVNOTIFY == 0x2000);
    CHECK(RIM_TYPEMOUSE == 0 && RIM_TYPEKEYBOARD == 1 && RIM_TYPEHID == 2);
    CHECK(RIM_INPUT == 0 && RIM_INPUTSINK == 1);
    CHECK(RID_INPUT == 0x10000003 && RID_HEADER == 0x10000005);
    CHECK(RIDI_PREPARSEDDATA == 0x20000005 && RIDI_DEVICENAME == 0x20000007 && RIDI_DEVICEINFO == 0x2000000b);
    CHECK(WM_INPUT == 0xFF && WM_INPUT_DEVICE_CHANGE == 0xFE);
    CHECK(GIDC_ARRIVAL == 1 && GIDC_REMOVAL == 2);
    CHECK(MOUSE_MOVE_RELATIVE == 0 && MOUSE_MOVE_ABSOLUTE == 1 && MOUSE_VIRTUAL_DESKTOP == 2 && MOUSE_ATTRIBUTES_CHANGED == 4);
    CHECK(RI_MOUSE_LEFT_BUTTON_DOWN == 1 && RI_MOUSE_LEFT_BUTTON_UP == 2 && RI_MOUSE_RIGHT_BUTTON_DOWN == 4 && RI_MOUSE_RIGHT_BUTTON_UP == 8);
    CHECK(RI_MOUSE_MIDDLE_BUTTON_DOWN == 0x10 && RI_MOUSE_MIDDLE_BUTTON_UP == 0x20 && RI_MOUSE_WHEEL == 0x400);
    CHECK(RI_MOUSE_BUTTON_4_DOWN == 0x40 && RI_MOUSE_BUTTON_5_DOWN == 0x100);
}

/* ---------- DefRawInputProc (user-mode, verified against disassembly) ---------- */
TEST(input_mouse_defrawinputproc_header_size) {
    REQUIRE(U(DefRawInputProc));
    PRAWINPUT dummy[2] = {NULL, NULL};
    CHECK(U(DefRawInputProc)(dummy, 0, sizeof(RAWINPUTHEADER)) == 0);
    CHECK(U(DefRawInputProc)(dummy, 1, sizeof(RAWINPUTHEADER)) == 0);
    CHECK(U(DefRawInputProc)(dummy, 2, sizeof(RAWINPUTHEADER)) == 0);
    CHECK(U(DefRawInputProc)(dummy, -1, sizeof(RAWINPUTHEADER)) == 0);
    static const UINT bad[] = {0, 1, 8, 16, 23, 25, 32, 40, 0xffffffff};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        LRESULT r = U(DefRawInputProc)(dummy, 1, bad[i]);
        CHECKF(r == -1, "cbSizeHeader=%u r=%lld", bad[i], (long long)r);
    }
}

/* ---------- GetRawInputBuffer ---------- */
TEST(input_mouse_getrawinputbuffer_params) {
    REQUIRE(U(GetRawInputBuffer));
    UINT sz = 0;
    SET_ERR(0);
    UINT r = U(GetRawInputBuffer)(NULL, NULL, sizeof(RAWINPUTHEADER));
    CHECK(r == (UINT)-1);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    static const UINT bad[] = {0, 1, 16, 23, 25, 32, 48};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        sz = 100;
        r = U(GetRawInputBuffer)(NULL, &sz, bad[i]);
        CHECKF(r == (UINT)-1, "cbSizeHeader=%u r=%u", bad[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(input_mouse_getrawinputbuffer_no_pending) {
    REQUIRE(U(GetRawInputBuffer));
    HWND w = im_popup(100, 100, 100, 100);
    im_pump_ms(10);
    UINT sz = 4096;
    char buf[4096] __attribute__((aligned(8)));
    SET_ERR(0);
    UINT r = U(GetRawInputBuffer)((PRAWINPUT)buf, &sz, sizeof(RAWINPUTHEADER));
    CHECKF(r == 0 || r == (UINT)-1, "r=%u", r);
    t_note("GetRawInputBuffer with nothing queued: r=%u size=%u err=%lu", r, sz, GetLastError());
    im_destroy(w);
}

/* ---------- RegisterRawInputDevices / GetRegisteredRawInputDevices ---------- */
TEST(input_mouse_registerrawinput_invalid_params) {
    REQUIRE(U(RegisterRawInputDevices));
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 1;
    SET_ERR(0);
    BOOL r = U(RegisterRawInputDevices)(&d, 1, sizeof d - 1);
    CHECK(!r); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(RegisterRawInputDevices)(&d, 1, 0);
    CHECK(!r); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(RegisterRawInputDevices)(&d, 1, sizeof d + 8);
    CHECK(!r); EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(RegisterRawInputDevices)(&d, 0, sizeof d);
    t_note("RegisterRawInputDevices(count 0) -> %d err %lu", r, GetLastError());
    SET_ERR(0);
    r = U(RegisterRawInputDevices)(NULL, 0, sizeof d);
    t_note("RegisterRawInputDevices(NULL, 0) -> %d err %lu", r, GetLastError());
}
TEST(input_mouse_registerrawinput_sink_requires_hwnd) {
    REQUIRE(U(RegisterRawInputDevices));
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 1; d.dwFlags = RIDEV_INPUTSINK; d.hwndTarget = NULL;
    UINT before = U(GetRegisteredRawInputDevices) ? n_registered() : 0;
    SET_ERR(0);
    BOOL r = U(RegisterRawInputDevices)(&d, 1, sizeof d);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    if (U(GetRegisteredRawInputDevices)) CHECK(n_registered() == before);
}
TEST(input_mouse_registerrawinput_remove_with_hwnd_fails) {
    REQUIRE(U(RegisterRawInputDevices));
    HWND w = im_popup(100, 100, 100, 100);
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 1; d.dwFlags = RIDEV_REMOVE; d.hwndTarget = w;
    SET_ERR(0);
    BOOL r = U(RegisterRawInputDevices)(&d, 1, sizeof d);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    im_destroy(w);
}
TEST(input_mouse_registerrawinput_bad_hwnd) {
    REQUIRE(U(RegisterRawInputDevices));
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 1; d.dwFlags = 0; d.hwndTarget = (HWND)(ULONG_PTR)0xdead00;
    SET_ERR(0);
    BOOL r = U(RegisterRawInputDevices)(&d, 1, sizeof d);
    CHECK(!r);
    t_note("RegisterRawInputDevices(bogus hwnd) err %lu", GetLastError());
    unreg_all_vendor(NULL);
}
TEST(input_mouse_registerrawinput_destroyed_hwnd) {
    REQUIRE(U(RegisterRawInputDevices));
    HWND w = im_popup(100, 100, 100, 100);
    im_destroy(w);
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 1; d.hwndTarget = w;
    SET_ERR(0);
    BOOL r = U(RegisterRawInputDevices)(&d, 1, sizeof d);
    CHECK(!r);
    unreg_all_vendor(NULL);
}
TEST(input_mouse_registerrawinput_roundtrip) {
    REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRegisteredRawInputDevices));
    HWND w = im_popup(100, 100, 100, 100);
    UINT base = n_registered();
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 7; d.dwFlags = 0; d.hwndTarget = w;
    SET_ERR(0);
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    UINT n = n_registered();
    CHECKF(n == base + 1, "base %u now %u", base, n);
    RAWINPUTDEVICE out[16]; memset(out, 0xee, sizeof out);
    UINT cnt = 16;
    UINT r = U(GetRegisteredRawInputDevices)(out, &cnt, sizeof(RAWINPUTDEVICE));
    CHECKF(r == n, "r=%u n=%u", r, n);
    int found = 0;
    for (UINT i = 0; i < r && i < 16; i++)
        if (out[i].usUsagePage == VENDOR_PAGE && out[i].usUsage == 7) {
            found++;
            CHECK(out[i].hwndTarget == w);
            CHECKF(out[i].dwFlags == 0, "flags 0x%lx", out[i].dwFlags);
        }
    CHECK(found == 1);
    d.dwFlags = RIDEV_REMOVE; d.hwndTarget = NULL;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    CHECK(n_registered() == base);
    im_destroy(w);
}
TEST(input_mouse_registerrawinput_flags_preserved) {
    REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRegisteredRawInputDevices));
    HWND w = im_popup(100, 100, 100, 100);
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 8; d.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY; d.hwndTarget = w;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    RAWINPUTDEVICE out[16]; UINT cnt = 16;
    UINT r = U(GetRegisteredRawInputDevices)(out, &cnt, sizeof(RAWINPUTDEVICE));
    for (UINT i = 0; i < r && i < 16; i++)
        if (out[i].usUsagePage == VENDOR_PAGE && out[i].usUsage == 8) {
            /* Win10 22H2 echoes RIDEV_INPUTSINK but not RIDEV_DEVNOTIFY (0x2000) back */
            CHECKF((out[i].dwFlags & RIDEV_INPUTSINK) == RIDEV_INPUTSINK && (out[i].dwFlags & ~(DWORD)(RIDEV_INPUTSINK | RIDEV_DEVNOTIFY)) == 0, "flags 0x%lx", out[i].dwFlags);
            t_note("registered flags read back: 0x%lx", out[i].dwFlags);
            CHECK(out[i].hwndTarget == w);
        }
    d.dwFlags = RIDEV_REMOVE; d.hwndTarget = NULL;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    im_destroy(w);
}
TEST(input_mouse_registerrawinput_reregister_replaces) {
    REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRegisteredRawInputDevices));
    HWND a = im_popup(100, 100, 100, 100), b = im_popup(300, 100, 100, 100);
    UINT base = n_registered();
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 9; d.hwndTarget = a;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    d.hwndTarget = b;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    CHECKF(n_registered() == base + 1, "count %u base %u", n_registered(), base);
    RAWINPUTDEVICE out[16]; UINT cnt = 16;
    UINT r = U(GetRegisteredRawInputDevices)(out, &cnt, sizeof(RAWINPUTDEVICE));
    for (UINT i = 0; i < r && i < 16; i++)
        if (out[i].usUsagePage == VENDOR_PAGE && out[i].usUsage == 9) CHECK(out[i].hwndTarget == b);
    d.dwFlags = RIDEV_REMOVE; d.hwndTarget = NULL;
    U(RegisterRawInputDevices)(&d, 1, sizeof d);
    CHECK(n_registered() == base);
    im_destroy(a); im_destroy(b);
}
TEST(input_mouse_registerrawinput_multiple_in_one_call) {
    REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRegisteredRawInputDevices));
    HWND w = im_popup(100, 100, 100, 100);
    UINT base = n_registered();
    RAWINPUTDEVICE d[3]; memset(d, 0, sizeof d);
    for (int i = 0; i < 3; i++) { d[i].usUsagePage = VENDOR_PAGE; d[i].usUsage = 1 + i; d[i].hwndTarget = w; }
    CHECK(U(RegisterRawInputDevices)(d, 3, sizeof(RAWINPUTDEVICE)));
    CHECKF(n_registered() == base + 3, "count %u base %u", n_registered(), base);
    for (int i = 0; i < 3; i++) { d[i].dwFlags = RIDEV_REMOVE; d[i].hwndTarget = NULL; }
    CHECK(U(RegisterRawInputDevices)(d, 3, sizeof(RAWINPUTDEVICE)));
    CHECK(n_registered() == base);
    im_destroy(w);
}
TEST(input_mouse_registerrawinput_destroy_window_unregisters_or_keeps) {
    REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRegisteredRawInputDevices));
    HWND w = im_popup(100, 100, 100, 100);
    UINT base = n_registered();
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 5; d.hwndTarget = w;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    im_destroy(w);
    UINT n = n_registered();
    t_note("registrations after target window destroyed: base %u, now %u", base, n);
    unreg_all_vendor(NULL);
    CHECK(n_registered() == base);
}
TEST(input_mouse_getregistered_buffer_too_small) {
    REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRegisteredRawInputDevices));
    HWND w = im_popup(100, 100, 100, 100);
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = VENDOR_PAGE; d.usUsage = 6; d.hwndTarget = w;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    UINT n = n_registered();
    RAWINPUTDEVICE out[16];
    UINT cnt = 0;
    SET_ERR(0);
    /* Win10 22H2 rejects a non-NULL buffer with a zero count as ERROR_INVALID_PARAMETER (count left
     * 0); the size query form is a NULL buffer, which returns 0 and the required count. */
    UINT r = U(GetRegisteredRawInputDevices)(NULL, &cnt, sizeof(RAWINPUTDEVICE));
    CHECKF(r == 0, "size query ret %u", r);
    CHECKF(cnt == n, "required count %u want %u", cnt, n);
    cnt = 0;
    SET_ERR(0);
    r = U(GetRegisteredRawInputDevices)(out, &cnt, sizeof(RAWINPUTDEVICE));
    CHECK(r == (UINT)-1);
    t_note("GetRegisteredRawInputDevices(buf, count 0) err %lu count %u", GetLastError(), cnt);
    if (n > 1) {
        cnt = n - 1;
        SET_ERR(0);
        r = U(GetRegisteredRawInputDevices)(out, &cnt, sizeof(RAWINPUTDEVICE));
        CHECK(r == (UINT)-1);
        EXPECT_ERR(r, ERROR_INSUFFICIENT_BUFFER);
        CHECK(cnt == n);
    }
    cnt = n;
    r = U(GetRegisteredRawInputDevices)(out, &cnt, sizeof(RAWINPUTDEVICE));
    CHECKF(r == n, "exact buffer r=%u n=%u", r, n);
    d.dwFlags = RIDEV_REMOVE; d.hwndTarget = NULL;
    U(RegisterRawInputDevices)(&d, 1, sizeof d);
    im_destroy(w);
}
TEST(input_mouse_getregistered_bad_cbsize) {
    REQUIRE(U(GetRegisteredRawInputDevices));
    RAWINPUTDEVICE out[4]; UINT cnt = 4;
    static const UINT bad[] = {0, 1, 8, 15, 17, 24, 32};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        cnt = 4;
        SET_ERR(0);
        UINT r = U(GetRegisteredRawInputDevices)(out, &cnt, bad[i]);
        CHECKF(r == (UINT)-1, "cb=%u r=%u", bad[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(input_mouse_getregistered_query_count_only) {
    REQUIRE(U(GetRegisteredRawInputDevices));
    UINT cnt = 0x5555;
    SET_ERR(0);
    UINT r = U(GetRegisteredRawInputDevices)(NULL, &cnt, sizeof(RAWINPUTDEVICE));
    CHECK(r == 0);
    CHECK(cnt != 0x5555);
    UINT again = 0x6666;
    r = U(GetRegisteredRawInputDevices)(NULL, &again, sizeof(RAWINPUTDEVICE));
    CHECK(again == cnt);
}
TEST(input_mouse_registerrawinput_mouse_usage_then_remove) {
    REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRegisteredRawInputDevices));
    HWND w = im_popup(100, 100, 100, 100);
    UINT base = n_registered();
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = 1; d.usUsage = 2; d.dwFlags = RIDEV_INPUTSINK; d.hwndTarget = w;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    CHECK(n_registered() == base + 1);
    d.dwFlags = RIDEV_REMOVE; d.hwndTarget = NULL;
    CHECK(U(RegisterRawInputDevices)(&d, 1, sizeof d));
    CHECK(n_registered() == base);
    SET_ERR(0);
    BOOL r = U(RegisterRawInputDevices)(&d, 1, sizeof d);
    t_note("removing a non-registered device again -> %d err %lu", r, GetLastError());
    im_destroy(w);
}

/* ---------- WM_INPUT delivery / GetRawInputData ---------- */
static struct {
    int n; UINT type; DWORD size; UINT wp; HANDLE dev; RAWMOUSE mouse; UINT retsize; UINT ret; DWORD err;
    UINT hdr_ret; UINT hdr_size; UINT bad_hdr_ret; DWORD bad_hdr_err; UINT bad_cmd_ret; DWORD bad_cmd_err;
    UINT small_ret; DWORD small_err; UINT small_need; UINT sizeonly_ret; UINT sizeonly_val;
    UINT all[8]; int allsz;
} wi;
static LRESULT hook_input(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m != WM_INPUT) return 0;
    char buf[256] __attribute__((aligned(8)));
    RAWINPUT *ri = (RAWINPUT *)buf;
    UINT sz = 0;
    HRAWINPUT hr = (HRAWINPUT)l;
    wi.n++;
    wi.wp = (UINT)w;
    wi.sizeonly_ret = U(GetRawInputData)(hr, RID_INPUT, NULL, &sz, sizeof(RAWINPUTHEADER));
    wi.sizeonly_val = sz;
    UINT sz2 = 0;
    SetLastError(0);
    wi.bad_hdr_ret = U(GetRawInputData)(hr, RID_INPUT, NULL, &sz2, 16);
    wi.bad_hdr_err = GetLastError();
    SetLastError(0);
    wi.bad_cmd_ret = U(GetRawInputData)(hr, 0x12345, NULL, &sz2, sizeof(RAWINPUTHEADER));
    wi.bad_cmd_err = GetLastError();
    if (sz > 1) {
        UINT small = 8;
        SetLastError(0);
        wi.small_ret = U(GetRawInputData)(hr, RID_INPUT, buf, &small, sizeof(RAWINPUTHEADER));
        wi.small_err = GetLastError();
        wi.small_need = small;
    }
    RAWINPUTHEADER hdr; UINT hs = sizeof hdr;
    wi.hdr_ret = U(GetRawInputData)(hr, RID_HEADER, &hdr, &hs, sizeof(RAWINPUTHEADER));
    wi.hdr_size = hs;
    if (sz <= sizeof buf) {
        wi.retsize = sz;
        wi.ret = U(GetRawInputData)(hr, RID_INPUT, buf, &sz, sizeof(RAWINPUTHEADER));
        wi.err = GetLastError();
        wi.type = ri->header.dwType; wi.size = ri->header.dwSize; wi.dev = ri->header.hDevice;
        if (ri->header.dwType == RIM_TYPEMOUSE) wi.mouse = ri->data.mouse;
    }
    return 0; /* fall through to DefWindowProc, which is required for cleanup */
}
static void reg_mouse(HWND w, int on) {
    RAWINPUTDEVICE d; memset(&d, 0, sizeof d);
    d.usUsagePage = 1; d.usUsage = 2;
    if (on) { d.dwFlags = RIDEV_INPUTSINK; d.hwndTarget = w; } else d.dwFlags = RIDEV_REMOVE;
    BOOL r = U(RegisterRawInputDevices)(&d, 1, sizeof d);
    if (!r) t_note("mouse register(%d) failed err %lu", on, GetLastError());
}
TEST(input_mouse_rawinput_wm_input_relative_move) {
    NEED_INPUT(); REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRawInputData));
    imstate s; im_save(&s);
    HWND w = im_popup(300, 300, 200, 200);
    im_setpos(380, 380);
    reg_mouse(w, 1);
    memset(&wi, 0, sizeof wi);
    im_hook = hook_input;
    INPUT in; memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_MOVE; in.mi.dx = 17; in.mi.dy = -9; in.mi.dwExtraInfo = 0x4242;
    CHECK(U(SendInput)(1, &in, sizeof in) == 1);
    im_pump_until(WM_INPUT, 1000);
    im_hook = NULL;
    reg_mouse(w, 0);
    if (wi.n < 1) {
        /* class G: raw mouse delivery for injected input depends on the session having a raw
         * mouse device (VM pointer device); the absolute-move test above already tolerates it */
        im_destroy(w); im_restore(&s);
        t_skip("no WM_INPUT delivered for injected relative move");
        return;
    }
    if (wi.n >= 1) {
        CHECK(wi.sizeonly_ret == 0);
        CHECKF(wi.sizeonly_val == sizeof(RAWINPUT) || wi.sizeonly_val >= sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE), "size %u", wi.sizeonly_val);
        CHECKF(wi.ret == wi.retsize, "ret %u size %u err %lu", wi.ret, wi.retsize, wi.err);
        CHECK(wi.type == RIM_TYPEMOUSE);
        CHECKF(wi.size == wi.retsize, "header dwSize %lu vs %u", wi.size, wi.retsize);
        CHECK(wi.wp == RIM_INPUT || wi.wp == RIM_INPUTSINK);
        CHECKF(wi.mouse.usFlags == MOUSE_MOVE_RELATIVE, "usFlags 0x%x", wi.mouse.usFlags);
        CHECKF(wi.mouse.lLastX == 17 && wi.mouse.lLastY == -9, "raw delta %ld,%ld (unaccelerated)", wi.mouse.lLastX, wi.mouse.lLastY);
        CHECKF(wi.mouse.ulExtraInformation == 0x4242, "extra 0x%lx", wi.mouse.ulExtraInformation);
        CHECK(wi.hdr_ret == sizeof(RAWINPUTHEADER) || wi.hdr_ret == 0);
        CHECKF(wi.hdr_size == sizeof(RAWINPUTHEADER), "RID_HEADER size %u", wi.hdr_size);
        CHECKF(wi.bad_hdr_ret == (UINT)-1, "bad header ret %u", wi.bad_hdr_ret);
        CHECKF(wi.bad_hdr_err == ERROR_INVALID_PARAMETER, "bad header err %lu", wi.bad_hdr_err);
        CHECKF(wi.bad_cmd_ret == (UINT)-1, "bad command ret %u", wi.bad_cmd_ret);
        CHECKF(wi.bad_cmd_err == ERROR_INVALID_PARAMETER, "bad command err %lu", wi.bad_cmd_err);
        CHECKF(wi.small_ret == (UINT)-1, "small buffer ret %u", wi.small_ret);
        CHECKF(wi.small_err == ERROR_INSUFFICIENT_BUFFER, "small buffer err %lu", wi.small_err);
        CHECKF(wi.small_need == wi.retsize, "required size %u vs %u", wi.small_need, wi.retsize);
    }
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_rawinput_wm_input_absolute_move) {
    NEED_INPUT(); REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRawInputData));
    imstate s; im_save(&s);
    HWND w = im_popup(300, 300, 200, 200);
    reg_mouse(w, 1);
    memset(&wi, 0, sizeof wi);
    im_hook = hook_input;
    INPUT in; memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE; in.mi.dx = 20000; in.mi.dy = 30000;
    U(SendInput)(1, &in, sizeof in);
    im_pump_until(WM_INPUT, 1000);
    im_hook = NULL;
    reg_mouse(w, 0);
    if (wi.n >= 1 && wi.type == RIM_TYPEMOUSE) {
        CHECKF(wi.mouse.usFlags & MOUSE_MOVE_ABSOLUTE, "usFlags 0x%x", wi.mouse.usFlags);
        CHECKF(wi.mouse.lLastX == 20000 && wi.mouse.lLastY == 30000, "abs %ld,%ld", wi.mouse.lLastX, wi.mouse.lLastY);
    } else t_note("no absolute WM_INPUT observed (n=%d)", wi.n);
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_rawinput_wm_input_buttons_and_wheel) {
    NEED_INPUT(); REQUIRE(U(RegisterRawInputDevices)); REQUIRE(U(GetRawInputData));
    if (im_swapped()) { t_skip("swapped buttons"); return; }
    imstate s; im_save(&s);
    HWND w = im_popup(300, 300, 200, 200);
    im_setpos(380, 380);
    reg_mouse(w, 1);
    static const struct { DWORD fl; DWORD data; USHORT want; const char *n; } t[] = {
        {MOUSEEVENTF_LEFTDOWN, 0, RI_MOUSE_LEFT_BUTTON_DOWN, "ldown"}, {MOUSEEVENTF_LEFTUP, 0, RI_MOUSE_LEFT_BUTTON_UP, "lup"},
        {MOUSEEVENTF_RIGHTDOWN, 0, RI_MOUSE_RIGHT_BUTTON_DOWN, "rdown"}, {MOUSEEVENTF_RIGHTUP, 0, RI_MOUSE_RIGHT_BUTTON_UP, "rup"},
        {MOUSEEVENTF_MIDDLEDOWN, 0, RI_MOUSE_MIDDLE_BUTTON_DOWN, "mdown"}, {MOUSEEVENTF_MIDDLEUP, 0, RI_MOUSE_MIDDLE_BUTTON_UP, "mup"},
        {MOUSEEVENTF_WHEEL, 120, RI_MOUSE_WHEEL, "wheel"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        memset(&wi, 0, sizeof wi);
        im_clear();
        im_hook = hook_input;
        INPUT in; memset(&in, 0, sizeof in);
        in.type = INPUT_MOUSE; in.mi.dwFlags = t[i].fl; in.mi.mouseData = t[i].data;
        U(SendInput)(1, &in, sizeof in);
        im_pump_until(WM_INPUT, 500);
        im_hook = NULL;
        if (wi.n >= 1 && wi.type == RIM_TYPEMOUSE) {
            CHECKF((wi.mouse.usButtonFlags & t[i].want) == t[i].want, "%s: usButtonFlags 0x%x", t[i].n, wi.mouse.usButtonFlags);
            if (t[i].fl == MOUSEEVENTF_WHEEL) CHECKF((short)wi.mouse.usButtonData == 120, "wheel data %d", (short)wi.mouse.usButtonData);
        } else t_note("%s: no WM_INPUT observed", t[i].n);
    }
    reg_mouse(w, 0);
    { INPUT in[3]; memset(in, 0, sizeof in); for (int i = 0; i < 3; i++) in[i].type = INPUT_MOUSE;
      in[0].mi.dwFlags = MOUSEEVENTF_LEFTUP; in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP; in[2].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
      U(SendInput)(3, in, sizeof(INPUT)); }
    im_pump_ms(20);
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_rawinput_not_delivered_when_unregistered) {
    NEED_INPUT(); REQUIRE(U(RegisterRawInputDevices));
    imstate s; im_save(&s);
    HWND w = im_popup(300, 300, 200, 200);
    im_setpos(380, 380);
    im_clear();
    INPUT in; memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_MOVE; in.mi.dx = 5;
    U(SendInput)(1, &in, sizeof in);
    im_pump_ms(60);
    CHECKF(im_count(WM_INPUT) == 0, "WM_INPUT count %d without registration", im_count(WM_INPUT));
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_getrawinputdata_invalid_handle) {
    REQUIRE(U(GetRawInputData));
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xdeadbeef};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        UINT sz = 0;
        char buf[128];
        SET_ERR(0);
        UINT r = U(GetRawInputData)((HRAWINPUT)bad[i], RID_INPUT, NULL, &sz, sizeof(RAWINPUTHEADER));
        CHECKF(r == (UINT)-1, "h=%llx r=%u", (unsigned long long)bad[i], r);
        t_note("GetRawInputData(bogus %llx) err %lu", (unsigned long long)bad[i], GetLastError());
        sz = sizeof buf;
        r = U(GetRawInputData)((HRAWINPUT)bad[i], RID_HEADER, buf, &sz, sizeof(RAWINPUTHEADER));
        CHECK(r == (UINT)-1);
    }
}
TEST(input_mouse_getrawinputdata_bad_header_size_invalid_handle) {
    REQUIRE(U(GetRawInputData));
    UINT sz = 0;
    SET_ERR(0);
    UINT r = U(GetRawInputData)((HRAWINPUT)(ULONG_PTR)0x1234, RID_INPUT, NULL, &sz, 8);
    CHECK(r == (UINT)-1);
    t_note("bad cbSizeHeader + bad handle err %lu", GetLastError());
}

/* ---------- GetRawInputDeviceList / GetRawInputDeviceInfo ---------- */
TEST(input_mouse_getrawinputdevicelist_count) {
    REQUIRE(U(GetRawInputDeviceList));
    UINT n = 0xfeed;
    SET_ERR(0);
    UINT r = U(GetRawInputDeviceList)(NULL, &n, sizeof(RAWINPUTDEVICELIST));
    CHECKF(r == 0, "r=%u", r);
    CHECK(n != 0xfeed);
    UINT n2 = 0;
    r = U(GetRawInputDeviceList)(NULL, &n2, sizeof(RAWINPUTDEVICELIST));
    CHECK(n2 == n);
    t_note("raw input devices: %u", n);
}
TEST(input_mouse_getrawinputdevicelist_bad_cbsize) {
    REQUIRE(U(GetRawInputDeviceList));
    static const UINT bad[] = {0, 1, 8, 12, 15, 17, 24, 32};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        UINT n = 4;
        SET_ERR(0);
        UINT r = U(GetRawInputDeviceList)(NULL, &n, bad[i]);
        CHECKF(r == (UINT)-1, "cb=%u r=%u", bad[i], r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(input_mouse_getrawinputdevicelist_fill_and_small) {
    REQUIRE(U(GetRawInputDeviceList));
    UINT n = 0;
    U(GetRawInputDeviceList)(NULL, &n, sizeof(RAWINPUTDEVICELIST));
    if (n == 0) { t_note("no raw input devices"); return; }
    if (n > 256) n = 256;
    RAWINPUTDEVICELIST *l = calloc(n + 4, sizeof *l);
    UINT cnt = n;
    SET_ERR(0);
    UINT r = U(GetRawInputDeviceList)(l, &cnt, sizeof(RAWINPUTDEVICELIST));
    CHECKF(r == n, "r=%u n=%u", r, n);
    for (UINT i = 0; i < r; i++) {
        CHECKF(l[i].hDevice != NULL, "device %u null handle", i);
        CHECKF(l[i].dwType <= RIM_TYPEHID, "device %u type %lu", i, l[i].dwType);
    }
    if (n >= 1) {
        cnt = n - 1;
        SET_ERR(0);
        r = U(GetRawInputDeviceList)(l, &cnt, sizeof(RAWINPUTDEVICELIST));
        CHECK(r == (UINT)-1);
        EXPECT_ERR(r, ERROR_INSUFFICIENT_BUFFER);
        CHECKF(cnt == n, "required %u want %u", cnt, n);
    }
    cnt = 0;
    SET_ERR(0);
    r = U(GetRawInputDeviceList)(l, &cnt, sizeof(RAWINPUTDEVICELIST));
    CHECK(r == (UINT)-1);
    EXPECT_ERR(r, ERROR_INSUFFICIENT_BUFFER);
    cnt = n + 3;
    r = U(GetRawInputDeviceList)(l, &cnt, sizeof(RAWINPUTDEVICELIST));
    CHECKF(r == n, "oversized buffer r=%u", r);
    free(l);
}
static RAWINPUTDEVICELIST *get_list(UINT *n) {
    UINT c = 0;
    U(GetRawInputDeviceList)(NULL, &c, sizeof(RAWINPUTDEVICELIST));
    if (c > 128) c = 128;
    RAWINPUTDEVICELIST *l = calloc(c + 1, sizeof *l);
    UINT cnt = c;
    UINT r = U(GetRawInputDeviceList)(l, &cnt, sizeof(RAWINPUTDEVICELIST));
    *n = (r == (UINT)-1) ? 0 : r;
    return l;
}
TEST(input_mouse_getrawinputdeviceinfo_name_a_w_parity) {
    REQUIRE(U(GetRawInputDeviceList)); REQUIRE(U(GetRawInputDeviceInfoA)); REQUIRE(U(GetRawInputDeviceInfoW));
    UINT n; RAWINPUTDEVICELIST *l = get_list(&n);
    if (!n) { t_note("no raw devices"); free(l); return; }
    for (UINT i = 0; i < n && i < 8; i++) {
        UINT na = 0, nw = 0;
        SET_ERR(0);
        UINT ra = U(GetRawInputDeviceInfoA)(l[i].hDevice, RIDI_DEVICENAME, NULL, &na);
        UINT rw = U(GetRawInputDeviceInfoW)(l[i].hDevice, RIDI_DEVICENAME, NULL, &nw);
        CHECKF(ra == 0 && rw == 0, "size query returns %u/%u", ra, rw);
        /* Win10 22H2 reports the ANSI name size as twice the wide size (bytes of the wide name) but
         * copies and returns the real character count (oracle run), so accept both. */
        CHECKF(na > 0 && (na == nw || na == 2 * nw), "A size %u W size %u", na, nw);
        t_note("RIDI_DEVICENAME query sizes A=%u W=%u", na, nw);
        if (na == 0 || na > 1024) continue;
        char *a = calloc(na + 2, 1); WCHAR *w = calloc(nw + 2, sizeof(WCHAR));
        UINT ca = na, cw = nw;
        ra = U(GetRawInputDeviceInfoA)(l[i].hDevice, RIDI_DEVICENAME, a, &ca);
        rw = U(GetRawInputDeviceInfoW)(l[i].hDevice, RIDI_DEVICENAME, w, &cw);
        CHECKF(ra == nw && rw == nw, "copy returns %u/%u want %u/%u", ra, rw, nw, nw);
        CHECKF(strncmp(a, "\\\\?\\", 4) == 0, "name '%s'", a);
        char conv[1100]; int cl = WideCharToMultiByte(CP_ACP, 0, w, -1, conv, sizeof conv, NULL, NULL);
        CHECK(cl > 0 && strcmp(conv, a) == 0);
        CHECK(lstrlenA(a) + 1 == (int)nw);
        CHECK(lstrlenW(w) + 1 == (int)nw);
        /* buffer too small by one */
        UINT sm = nw - 1;
        SET_ERR(0);
        ra = U(GetRawInputDeviceInfoA)(l[i].hDevice, RIDI_DEVICENAME, a, &sm);
        t_note("ANSI name with count %u (one short): ret %u err %lu required %u", nw - 1, ra, GetLastError(), sm);
        if (na == nw) {   /* only where the A size is exact is the short-buffer contract unambiguous */
            CHECK(ra == (UINT)-1);
            EXPECT_ERR(ra, ERROR_INSUFFICIENT_BUFFER);
            CHECKF(sm == na, "required %u want %u", sm, na);
        }
        sm = nw - 1;
        SET_ERR(0);
        rw = U(GetRawInputDeviceInfoW)(l[i].hDevice, RIDI_DEVICENAME, w, &sm);
        CHECK(rw == (UINT)-1);
        EXPECT_ERR(rw, ERROR_INSUFFICIENT_BUFFER);
        CHECK(sm == nw);
        sm = 0;
        SET_ERR(0);
        ra = U(GetRawInputDeviceInfoA)(l[i].hDevice, RIDI_DEVICENAME, a, &sm);
        CHECK(ra == (UINT)-1);
        free(a); free(w);
    }
    free(l);
}
TEST(input_mouse_getrawinputdeviceinfo_deviceinfo) {
    REQUIRE(U(GetRawInputDeviceList)); REQUIRE(U(GetRawInputDeviceInfoA)); REQUIRE(U(GetRawInputDeviceInfoW));
    UINT n; RAWINPUTDEVICELIST *l = get_list(&n);
    for (UINT i = 0; i < n && i < 8; i++) {
        RID_DEVICE_INFO info; memset(&info, 0xcd, sizeof info);
        info.cbSize = sizeof info;
        UINT sz = sizeof info;
        SET_ERR(0);
        UINT r = U(GetRawInputDeviceInfoW)(l[i].hDevice, RIDI_DEVICEINFO, &info, &sz);
        CHECKF(r == sizeof info, "r=%u err %lu", r, GetLastError());
        CHECKF(info.dwType == l[i].dwType, "type %lu list type %lu", info.dwType, l[i].dwType);
        CHECK(info.cbSize == sizeof info);
        RID_DEVICE_INFO ia; memset(&ia, 0xcd, sizeof ia); ia.cbSize = sizeof ia; sz = sizeof ia;
        r = U(GetRawInputDeviceInfoA)(l[i].hDevice, RIDI_DEVICEINFO, &ia, &sz);
        CHECK(r == sizeof ia);
        CHECK(memcmp(&ia, &info, sizeof ia) == 0);
        if (info.dwType == RIM_TYPEMOUSE) {
            CHECKF(info.mouse.dwNumberOfButtons >= 1 && info.mouse.dwNumberOfButtons <= 32, "buttons %lu", info.mouse.dwNumberOfButtons);
            CHECK(info.mouse.dwSampleRate < 100000);
        } else if (info.dwType == RIM_TYPEKEYBOARD) {
            CHECK(info.keyboard.dwNumberOfFunctionKeys <= 128);
            CHECK(info.keyboard.dwNumberOfKeysTotal > 0);
        } else if (info.dwType == RIM_TYPEHID) {
            CHECK(info.hid.usUsagePage != 0);
        }
        /* undersized buffers */
        sz = sizeof info - 1;
        SET_ERR(0);
        r = U(GetRawInputDeviceInfoW)(l[i].hDevice, RIDI_DEVICEINFO, &info, &sz);
        CHECK(r == (UINT)-1);
        EXPECT_ERR(r, ERROR_INSUFFICIENT_BUFFER);
        CHECKF(sz == sizeof info, "required %u", sz);
        /* size-only query */
        sz = 0;
        r = U(GetRawInputDeviceInfoW)(l[i].hDevice, RIDI_DEVICEINFO, NULL, &sz);
        CHECK(r == 0);
        CHECKF(sz == sizeof info, "size-only %u", sz);
    }
    free(l);
}
TEST(input_mouse_getrawinputdeviceinfo_cbsize_field_must_match) {
    REQUIRE(U(GetRawInputDeviceList)); REQUIRE(U(GetRawInputDeviceInfoW));
    UINT n; RAWINPUTDEVICELIST *l = get_list(&n);
    if (n) {
        RID_DEVICE_INFO info; memset(&info, 0, sizeof info);
        info.cbSize = 7;
        UINT sz = sizeof info;
        SET_ERR(0);
        UINT r = U(GetRawInputDeviceInfoW)(l[0].hDevice, RIDI_DEVICEINFO, &info, &sz);
        t_note("RIDI_DEVICEINFO with cbSize=7 -> %u err %lu", r, GetLastError());
    }
    free(l);
}
TEST(input_mouse_getrawinputdeviceinfo_preparsed) {
    REQUIRE(U(GetRawInputDeviceList)); REQUIRE(U(GetRawInputDeviceInfoW));
    UINT n; RAWINPUTDEVICELIST *l = get_list(&n);
    for (UINT i = 0; i < n && i < 8; i++) {
        UINT sz = 0;
        SET_ERR(0);
        UINT r = U(GetRawInputDeviceInfoW)(l[i].hDevice, RIDI_PREPARSEDDATA, NULL, &sz);
        t_note("device %u (type %lu) preparsed size query -> %u size %u err %lu", i, l[i].dwType, r, sz, GetLastError());
        if (r == 0 && sz > 0 && sz < 65536) {
            void *b = malloc(sz);
            UINT c = sz;
            r = U(GetRawInputDeviceInfoW)(l[i].hDevice, RIDI_PREPARSEDDATA, b, &c);
            CHECKF(r == sz, "copy r=%u size=%u", r, sz);
            free(b);
        }
    }
    free(l);
}
TEST(input_mouse_getrawinputdeviceinfo_invalid) {
    REQUIRE(U(GetRawInputDeviceInfoW)); REQUIRE(U(GetRawInputDeviceInfoA));
    UINT sz = 0;
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xdeadbeef};
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        sz = 0;
        UINT r = U(GetRawInputDeviceInfoW)((HANDLE)bad[i], RIDI_DEVICENAME, NULL, &sz);
        CHECKF(r == (UINT)-1, "h=%llx r=%u", (unsigned long long)bad[i], r);
        t_note("DeviceInfoW(bogus %llx) err %lu", (unsigned long long)bad[i], GetLastError());
        SET_ERR(0);
        r = U(GetRawInputDeviceInfoA)((HANDLE)bad[i], RIDI_DEVICEINFO, NULL, &sz);
        CHECK(r == (UINT)-1);
    }
    UINT n; RAWINPUTDEVICELIST *l = NULL;
    if (U(GetRawInputDeviceList)) l = get_list(&n); else n = 0;
    if (n) {
        SET_ERR(0);
        sz = 0;
        UINT r = U(GetRawInputDeviceInfoW)(l[0].hDevice, 0x20000001, NULL, &sz);
        CHECK(r == (UINT)-1);
        t_note("bad uiCommand err %lu", GetLastError());
        SET_ERR(0);
        r = U(GetRawInputDeviceInfoW)(l[0].hDevice, 0, NULL, &sz);
        CHECK(r == (UINT)-1);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    free(l);
}
TEST(input_mouse_getrawinputdeviceinfo_repeated) {
    REQUIRE(U(GetRawInputDeviceList)); REQUIRE(U(GetRawInputDeviceInfoW));
    UINT n; RAWINPUTDEVICELIST *l = get_list(&n);
    for (int i = 0; i < 200 && n; i++) {
        UINT sz = 0;
        if (U(GetRawInputDeviceInfoW)(l[i % n].hDevice, RIDI_DEVICENAME, NULL, &sz) != 0 || sz == 0) { CHECKF(0, "iter %d", i); break; }
    }
    free(l);
}
