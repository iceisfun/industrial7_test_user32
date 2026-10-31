#include "im.h"

UDECL(HANDLE, CreateSyntheticPointerDevice, (int, ULONG, int));
UDECL(void, DestroySyntheticPointerDevice, (HANDLE));
#define HSYNTHETICPOINTERDEVICE HANDLE
#define BAD_ID_COUNT 4
static const UINT32 bad_ids[BAD_ID_COUNT] = {0x4242, 0x12345678, 0xfffffff0, 0x7ffffff0};

TEST(input_mouse_touch_struct_sizes_and_consts) {
    CHECK(sizeof(TOUCHINPUT) == 48);
    CHECK(sizeof(GESTUREINFO) == 56);
    CHECK(sizeof(GESTURECONFIG) == 12);
    CHECK(sizeof(POINTER_INFO) == 0x60);
    CHECK(sizeof(POINTER_TOUCH_INFO) == 0x90);
    CHECK(sizeof(POINTER_PEN_INFO) == 0x78);
    CHECK(sizeof(POINTER_DEVICE_INFO) == 0x214 + 4 || sizeof(POINTER_DEVICE_INFO) > 0);
    CHECK(sizeof(TOUCH_HIT_TESTING_INPUT) == 48);
    CHECK(sizeof(TOUCH_HIT_TESTING_PROXIMITY_EVALUATION) == 12);
    CHECK(sizeof(INPUT_MESSAGE_SOURCE) == 8);
    CHECK(TWF_FINETOUCH == 1 && TWF_WANTPALM == 2);
    CHECK(WM_TOUCH == 0x240 && WM_GESTURE == 0x119 && WM_GESTURENOTIFY == 0x11A && WM_TOUCHHITTESTING == 0x24D);
    CHECK(WM_POINTERDEVICECHANGE == 0x238 && WM_POINTERDEVICEINRANGE == 0x239 && WM_POINTERDEVICEOUTOFRANGE == 0x23A);
    CHECK(WM_POINTERUPDATE == 0x245 && WM_POINTERDOWN == 0x246 && WM_POINTERUP == 0x247 && WM_POINTERENTER == 0x249);
    CHECK(WM_POINTERLEAVE == 0x24A && WM_POINTERACTIVATE == 0x24B && WM_POINTERCAPTURECHANGED == 0x24C);
    CHECK(WM_POINTERWHEEL == 0x24E && WM_POINTERHWHEEL == 0x24F && WM_POINTERROUTEDTO == 0x251 && WM_POINTERROUTEDAWAY == 0x252 && WM_POINTERROUTEDRELEASED == 0x253);
    CHECK(PT_POINTER == 1 && PT_TOUCH == 2 && PT_PEN == 3 && PT_MOUSE == 4);
    CHECK(GID_BEGIN == 1 && GID_END == 2 && GID_ZOOM == 3 && GID_PAN == 4 && GID_ROTATE == 5 && GID_TWOFINGERTAP == 6 && GID_PRESSANDTAP == 7);
    CHECK(GC_ALLGESTURES == 1 && GC_ZOOM == 1 && GC_PAN == 1 && GC_ROTATE == 1 && GC_TWOFINGERTAP == 1 && GC_PRESSANDTAP == 1);
    CHECK(GC_PAN_WITH_SINGLE_FINGER_VERTICALLY == 2 && GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY == 4 && GC_PAN_WITH_GUTTER == 8 && GC_PAN_WITH_INERTIA == 16);
    CHECK(GCF_INCLUDE_ANCESTORS == 1);
    CHECK(TOUCHEVENTF_MOVE == 1 && TOUCHEVENTF_DOWN == 2 && TOUCHEVENTF_UP == 4 && TOUCHEVENTF_INRANGE == 8 && TOUCHEVENTF_PRIMARY == 0x10 && TOUCHEVENTF_NOCOALESCE == 0x20 && TOUCHEVENTF_PEN == 0x40 && TOUCHEVENTF_PALM == 0x80);
    CHECK(TOUCHINPUTMASKF_TIMEFROMSYSTEM == 1 && TOUCHINPUTMASKF_EXTRAINFO == 2 && TOUCHINPUTMASKF_CONTACTAREA == 4);
    CHECK(POINTER_FLAG_NONE == 0 && POINTER_FLAG_NEW == 1 && POINTER_FLAG_INRANGE == 2 && POINTER_FLAG_INCONTACT == 4 && POINTER_FLAG_FIRSTBUTTON == 0x10);
    CHECK(POINTER_FLAG_PRIMARY == 0x2000 && POINTER_FLAG_DOWN == 0x10000 && POINTER_FLAG_UPDATE == 0x20000 && POINTER_FLAG_UP == 0x40000);
    CHECK(TOUCH_HIT_TESTING_DEFAULT == 0 && TOUCH_HIT_TESTING_CLIENT == 1 && TOUCH_HIT_TESTING_NONE == 2);
    CHECK(TOUCH_HIT_TESTING_PROXIMITY_CLOSEST == 0 && TOUCH_HIT_TESTING_PROXIMITY_FARTHEST == 0xfff);
    CHECK(TOUCH_FEEDBACK_DEFAULT == 1 && TOUCH_FEEDBACK_INDIRECT == 2 && TOUCH_FEEDBACK_NONE == 3);
    CHECK(SM_DIGITIZER == 94 && SM_MAXIMUMTOUCHES == 95);
    CHECK(NID_INTEGRATED_TOUCH == 0x01 && NID_EXTERNAL_TOUCH == 0x02 && NID_INTEGRATED_PEN == 0x04 && NID_EXTERNAL_PEN == 0x08 && NID_MULTI_INPUT == 0x40 && NID_READY == 0x80);
}

/* ---------- Register / Unregister / IsTouchWindow ---------- */
TEST(input_mouse_touchwindow_lifecycle) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(UnregisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    HWND w = im_popup(100, 100, 100, 100);
    ULONG fl = 0xabcd;
    CHECK(!U(IsTouchWindow)(w, NULL));
    SET_ERR(0);
    CHECK(U(RegisterTouchWindow)(w, 0));
    CHECK(U(IsTouchWindow)(w, NULL));
    CHECK(U(IsTouchWindow)(w, &fl));
    if (fl != 0) t_note("IsTouchWindow flags after register(0) = %lu", fl);
    CHECK(U(UnregisterTouchWindow)(w));
    CHECK(!U(IsTouchWindow)(w, NULL));
    im_destroy(w);
}
TEST(input_mouse_touchwindow_flags_roundtrip) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(UnregisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    static const ULONG f[] = {TWF_FINETOUCH, TWF_WANTPALM, TWF_FINETOUCH | TWF_WANTPALM, 0};
    for (unsigned i = 0; i < 4; i++) {
        HWND w = im_popup(100, 100, 100, 100);
        ULONG fl = 0xabcd;
        CHECKF(U(RegisterTouchWindow)(w, f[i]), "register flags %lu", f[i]);
        CHECK(U(IsTouchWindow)(w, &fl));
        if (fl != f[i]) t_note("IsTouchWindow flags %lu want %lu", fl, f[i]);
        CHECK(U(UnregisterTouchWindow)(w));
        im_destroy(w);
    }
}
TEST(input_mouse_touchwindow_invalid_flags) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(UnregisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    HWND w = im_popup(100, 100, 100, 100);
    static const ULONG f[] = {4, 8, 0x10, 0x100, 0x80000000, 0xffffffff};
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        SET_ERR(0);
        BOOL r = U(RegisterTouchWindow)(w, f[i]);
        t_note("RegisterTouchWindow flags 0x%lx -> %d err %lu", f[i], r, GetLastError());
        if (r) U(UnregisterTouchWindow)(w);
    }
    CHECK(!U(IsTouchWindow)(w, NULL));
    im_destroy(w);
}
TEST(input_mouse_touchwindow_invalid_hwnd) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(UnregisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xdeadbeef};
    for (unsigned i = 0; i < 4; i++) {
        HWND h = (HWND)bad[i];
        SET_ERR(0);
        CHECKF(!U(RegisterTouchWindow)(h, 0), "register %llx", (unsigned long long)bad[i]);
        t_note("RegisterTouchWindow(%llx) err %lu", (unsigned long long)bad[i], GetLastError());
        SET_ERR(0);
        CHECKF(!U(UnregisterTouchWindow)(h), "unregister %llx", (unsigned long long)bad[i]);
        SET_ERR(0);
        CHECKF(!U(IsTouchWindow)(h, NULL), "is %llx", (unsigned long long)bad[i]);
    }
}
TEST(input_mouse_touchwindow_destroyed_hwnd) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(UnregisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    HWND w = im_popup(100, 100, 100, 100);
    CHECK(U(RegisterTouchWindow)(w, 0));
    im_destroy(w);
    CHECK(!U(IsTouchWindow)(w, NULL));
    CHECK(!U(RegisterTouchWindow)(w, 0));
    CHECK(!U(UnregisterTouchWindow)(w));
}
TEST(input_mouse_touchwindow_unregister_when_not_registered) {
    REQUIRE(U(UnregisterTouchWindow));
    HWND w = im_popup(100, 100, 100, 100);
    SET_ERR(0);
    BOOL r = U(UnregisterTouchWindow)(w);
    t_note("UnregisterTouchWindow on unregistered window -> %d err %lu", r, GetLastError());
    im_destroy(w);
}
TEST(input_mouse_touchwindow_register_twice) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(UnregisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    HWND w = im_popup(100, 100, 100, 100);
    CHECK(U(RegisterTouchWindow)(w, 0));
    SET_ERR(0);
    BOOL r = U(RegisterTouchWindow)(w, TWF_WANTPALM);
    ULONG fl = 0;
    U(IsTouchWindow)(w, &fl);
    t_note("second RegisterTouchWindow -> %d err %lu, flags now %lu", r, GetLastError(), fl);
    CHECK(U(IsTouchWindow)(w, NULL));
    U(UnregisterTouchWindow)(w);
    CHECK(!U(IsTouchWindow)(w, NULL));
    im_destroy(w);
}
TEST(input_mouse_touchwindow_child_window) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(UnregisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    HWND p = im_popup(100, 100, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 50, 50, p);
    CHECK(U(RegisterTouchWindow)(c, 0));
    CHECK(U(IsTouchWindow)(c, NULL));
    CHECK(!U(IsTouchWindow)(p, NULL));
    U(UnregisterTouchWindow)(c);
    im_destroy(p);
}
TEST(input_mouse_touchwindow_state_dies_with_window) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    for (int i = 0; i < 50; i++) {
        HWND w = im_popup(100, 100, 50, 50);
        if (!U(RegisterTouchWindow)(w, 0)) { CHECKF(0, "register iter %d", i); im_destroy(w); break; }
        im_destroy(w);
    }
    CHECK(1);
}
static HWND g_tw; static HANDLE g_tw_ready, g_tw_stop;
static DWORD WINAPI tw_thread(LPVOID a) {
    g_tw = im_popup(500, 100, 60, 60);
    SetEvent(g_tw_ready);
    while (WaitForSingleObject(g_tw_stop, 10) == WAIT_TIMEOUT) im_pump_ms(0);
    im_destroy(g_tw);
    return 0;
}
TEST(input_mouse_touchwindow_other_thread_window) {
    REQUIRE(U(RegisterTouchWindow)); REQUIRE(U(UnregisterTouchWindow)); REQUIRE(U(IsTouchWindow));
    g_tw_ready = CreateEventA(NULL, TRUE, FALSE, NULL); g_tw_stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE h = CreateThread(NULL, 0, tw_thread, NULL, 0, NULL);
    CHECK(WaitForSingleObject(g_tw_ready, 5000) == WAIT_OBJECT_0);
    SET_ERR(0);
    BOOL r = U(RegisterTouchWindow)(g_tw, 0);
    t_note("RegisterTouchWindow(window owned by another thread) -> %d err %lu", r, GetLastError());
    if (r) { CHECK(U(IsTouchWindow)(g_tw, NULL)); U(UnregisterTouchWindow)(g_tw); }
    SetEvent(g_tw_stop);
    CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    CloseHandle(h); CloseHandle(g_tw_ready); CloseHandle(g_tw_stop);
}

/* ---------- GetTouchInputInfo / CloseTouchInputHandle ---------- */
TEST(input_mouse_gettouchinputinfo_bad_cbsize) {
    REQUIRE(U(GetTouchInputInfo));
    TOUCHINPUT ti[2]; memset(ti, 0, sizeof ti);
    static const int sz[] = {0, 1, 24, 40, 47, 49, 56, 96, -1};
    for (unsigned i = 0; i < sizeof sz / sizeof *sz; i++) {
        SET_ERR(0);
        BOOL r = U(GetTouchInputInfo)((HTOUCHINPUT)(ULONG_PTR)0x1234, 1, ti, sz[i]);
        CHECKF(!r, "cbSize=%d", sz[i]);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        r = U(GetTouchInputInfo)(NULL, 1, ti, sz[i]);
        CHECK(!r);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(input_mouse_gettouchinputinfo_bad_handle) {
    REQUIRE(U(GetTouchInputInfo));
    TOUCHINPUT ti[2]; memset(ti, 0, sizeof ti);
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xdeadbeef};
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL r = U(GetTouchInputInfo)((HTOUCHINPUT)bad[i], 1, ti, sizeof(TOUCHINPUT));
        CHECKF(!r, "handle %llx", (unsigned long long)bad[i]);
        t_note("GetTouchInputInfo(bogus %llx) err %lu", (unsigned long long)bad[i], GetLastError());
        SET_ERR(0);
        r = U(GetTouchInputInfo)((HTOUCHINPUT)bad[i], 0, ti, sizeof(TOUCHINPUT));
        CHECK(!r);
    }
}
TEST(input_mouse_closetouchinputhandle_invalid) {
    REQUIRE(U(CloseTouchInputHandle));
    SET_ERR(0x1234);
    BOOL r = U(CloseTouchInputHandle)(NULL);
    CHECK(!r);
    CHECKF(GetLastError() == 0x1234, "NULL handle must not set an error, got %lu", GetLastError());
    static const ULONG_PTR bad[] = {1, 0x1234, 0xdeadbeef, 0x10000};
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        r = U(CloseTouchInputHandle)((HTOUCHINPUT)bad[i]);
        CHECKF(!r, "handle %llx", (unsigned long long)bad[i]);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}

/* ---------- Gestures ---------- */
TEST(input_mouse_getgestureinfo_invalid_params) {
    REQUIRE(U(GetGestureInfo));
    GESTUREINFO gi; memset(&gi, 0, sizeof gi);
    SET_ERR(0);
    CHECK(!U(GetGestureInfo)(NULL, NULL));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    static const UINT cb[] = {0, 1, 48, 55, 57, 64, 0xffffffff};
    for (unsigned i = 0; i < sizeof cb / sizeof *cb; i++) {
        gi.cbSize = cb[i];
        SET_ERR(0);
        BOOL r = U(GetGestureInfo)((HGESTUREINFO)(ULONG_PTR)0x1234, &gi);
        CHECKF(!r, "cbSize=%u", cb[i]);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(input_mouse_getgestureinfo_invalid_handle) {
    REQUIRE(U(GetGestureInfo));
    GESTUREINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xdeadbeef};
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL r = U(GetGestureInfo)((HGESTUREINFO)bad[i], &gi);
        CHECKF(!r, "handle %llx", (unsigned long long)bad[i]);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(input_mouse_closegestureinfohandle_invalid) {
    REQUIRE(U(CloseGestureInfoHandle));
    SET_ERR(0x4321);
    CHECK(!U(CloseGestureInfoHandle)(NULL));
    CHECK(GetLastError() == 0x4321);
    static const ULONG_PTR bad[] = {1, 0x1234, 0xdeadbeef};
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        BOOL r = U(CloseGestureInfoHandle)((HGESTUREINFO)bad[i]);
        CHECK(!r);
        EXPECT_ERR(r, ERROR_INVALID_HANDLE);
    }
}
TEST(input_mouse_getgestureextraargs_invalid) {
    REQUIRE(U(GetGestureExtraArgs));
    BYTE buf[16];
    SET_ERR(0);
    BOOL r = U(GetGestureExtraArgs)((HGESTUREINFO)(ULONG_PTR)0x1234, sizeof buf, buf);
    CHECK(!r);
    t_note("GetGestureExtraArgs(bogus) err %lu", GetLastError());
}
TEST(input_mouse_gestureconfig_set_get_roundtrip) {
    REQUIRE(U(SetGestureConfig)); REQUIRE(U(GetGestureConfig));
    HWND w = im_popup(100, 100, 100, 100);
    GESTURECONFIG cfg[2]; memset(cfg, 0, sizeof cfg);
    cfg[0].dwID = GID_PAN; cfg[0].dwWant = GC_PAN | GC_PAN_WITH_INERTIA; cfg[0].dwBlock = GC_PAN_WITH_GUTTER;
    cfg[1].dwID = GID_ZOOM; cfg[1].dwWant = 0; cfg[1].dwBlock = GC_ZOOM;
    SET_ERR(0);
    BOOL r = U(SetGestureConfig)(w, 0, 2, cfg, sizeof(GESTURECONFIG));
    t_note("SetGestureConfig -> %d err %lu", r, GetLastError());
    if (r) {
        GESTURECONFIG out[2]; memset(out, 0, sizeof out);
        UINT cnt = 2;
        out[0].dwID = GID_PAN; out[1].dwID = GID_ZOOM;
        SET_ERR(0);
        BOOL g = U(GetGestureConfig)(w, 0, 0, &cnt, out, sizeof(GESTURECONFIG));
        t_note("GetGestureConfig -> %d err %lu cnt %u", g, GetLastError(), cnt);
        if (g) {
            for (UINT i = 0; i < cnt && i < 2; i++) {
                if (out[i].dwID == GID_PAN) CHECK(out[i].dwWant == (GC_PAN | GC_PAN_WITH_INERTIA) && out[i].dwBlock == GC_PAN_WITH_GUTTER);
                if (out[i].dwID == GID_ZOOM) CHECK(out[i].dwBlock == GC_ZOOM);
            }
        }
    }
    im_destroy(w);
}
TEST(input_mouse_setgestureconfig_invalid) {
    REQUIRE(U(SetGestureConfig));
    HWND w = im_popup(100, 100, 100, 100);
    GESTURECONFIG cfg; memset(&cfg, 0, sizeof cfg); cfg.dwID = GID_PAN;
    static const UINT cb[] = {0, 1, 11, 13, 24};
    for (unsigned i = 0; i < sizeof cb / sizeof *cb; i++) {
        SET_ERR(0);
        BOOL r = U(SetGestureConfig)(w, 0, 1, &cfg, cb[i]);
        CHECKF(!r, "cbSize %u", cb[i]);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    SET_ERR(0);
    BOOL r = U(SetGestureConfig)((HWND)(ULONG_PTR)0xdead0, 0, 1, &cfg, sizeof cfg);
    CHECK(!r);
    t_note("SetGestureConfig(bogus hwnd) err %lu", GetLastError());
    SET_ERR(0);
    r = U(SetGestureConfig)(w, 0, 0, &cfg, sizeof cfg);
    t_note("SetGestureConfig(count 0) -> %d err %lu", r, GetLastError());
    im_destroy(w);
}
TEST(input_mouse_getgestureconfig_invalid) {
    REQUIRE(U(GetGestureConfig));
    HWND w = im_popup(100, 100, 100, 100);
    GESTURECONFIG cfg[4]; UINT cnt = 4;
    memset(cfg, 0, sizeof cfg);
    SET_ERR(0);
    CHECK(!U(GetGestureConfig)(w, 0, 0, &cnt, cfg, 5));
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(GetGestureConfig)((HWND)(ULONG_PTR)0xdead0, 0, 0, &cnt, cfg, sizeof(GESTURECONFIG)));
    t_note("GetGestureConfig(bogus hwnd) err %lu", GetLastError());
    im_destroy(w);
}

/* ---------- Window feedback ---------- */
TEST(input_mouse_windowfeedback_set_get) {
    REQUIRE(U(SetWindowFeedbackSetting)); REQUIRE(U(GetWindowFeedbackSetting));
    HWND w = im_popup(100, 100, 100, 100);
    BOOL v = FALSE; UINT32 sz = sizeof v;
    SET_ERR(0);
    BOOL r = U(GetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_CONTACTVISUALIZATION, 0, &sz, &v);
    t_note("GetWindowFeedbackSetting (unset) -> %d err %lu val %d", r, GetLastError(), v);
    BOOL on = FALSE;
    SET_ERR(0);
    r = U(SetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_CONTACTVISUALIZATION, 0, sizeof on, &on);
    CHECK(r);
    if (r) {
        v = TRUE; sz = sizeof v;
        r = U(GetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_CONTACTVISUALIZATION, 0, &sz, &v);
        CHECK(r);
        CHECK(v == FALSE);
        CHECK(sz == sizeof(BOOL));
        on = TRUE;
        CHECK(U(SetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_CONTACTVISUALIZATION, 0, sizeof on, &on));
        v = FALSE; sz = sizeof v;
        CHECK(U(GetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_CONTACTVISUALIZATION, 0, &sz, &v));
        CHECK(v == TRUE);
        /* restore default */
        CHECK(U(SetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_CONTACTVISUALIZATION, 0, 0, NULL));
        SET_ERR(0);
        sz = sizeof v;
        r = U(GetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_CONTACTVISUALIZATION, 0, &sz, &v);
        t_note("GetWindowFeedbackSetting after reset -> %d err %lu", r, GetLastError());
    }
    im_destroy(w);
}
TEST(input_mouse_windowfeedback_all_types) {
    REQUIRE(U(SetWindowFeedbackSetting)); REQUIRE(U(GetWindowFeedbackSetting));
    HWND w = im_popup(100, 100, 100, 100);
    static const FEEDBACK_TYPE t[] = {FEEDBACK_TOUCH_CONTACTVISUALIZATION, FEEDBACK_PEN_BARRELVISUALIZATION, FEEDBACK_PEN_TAP, FEEDBACK_PEN_DOUBLETAP,
        FEEDBACK_PEN_PRESSANDHOLD, FEEDBACK_PEN_RIGHTTAP, FEEDBACK_TOUCH_TAP, FEEDBACK_TOUCH_DOUBLETAP, FEEDBACK_TOUCH_PRESSANDHOLD,
        FEEDBACK_TOUCH_RIGHTTAP, FEEDBACK_GESTURE_PRESSANDTAP};
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        BOOL on = TRUE;
        SET_ERR(0);
        BOOL r = U(SetWindowFeedbackSetting)(w, t[i], 0, sizeof on, &on);
        CHECKF(r, "feedback type %d err %lu", t[i], GetLastError());
        if (r) {
            BOOL v = FALSE; UINT32 sz = sizeof v;
            CHECKF(U(GetWindowFeedbackSetting)(w, t[i], 0, &sz, &v) && v == TRUE, "get type %d", t[i]);
            U(SetWindowFeedbackSetting)(w, t[i], 0, 0, NULL);
        }
    }
    im_destroy(w);
}
TEST(input_mouse_windowfeedback_invalid) {
    REQUIRE(U(SetWindowFeedbackSetting)); REQUIRE(U(GetWindowFeedbackSetting));
    HWND w = im_popup(100, 100, 100, 100);
    BOOL on = TRUE; UINT32 sz = sizeof on;
    SET_ERR(0);
    BOOL r = U(SetWindowFeedbackSetting)(w, (FEEDBACK_TYPE)0, 0, sizeof on, &on);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(SetWindowFeedbackSetting)(w, (FEEDBACK_TYPE)500, 0, sizeof on, &on);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(SetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_TAP, 0, 3, &on);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(SetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_TAP, 0, 16, &on);
    CHECK(!r);
    SET_ERR(0);
    r = U(SetWindowFeedbackSetting)((HWND)(ULONG_PTR)0xdead0, FEEDBACK_TOUCH_TAP, 0, sizeof on, &on);
    CHECK(!r);
    t_note("SetWindowFeedbackSetting(bogus hwnd) err %lu", GetLastError());
    SET_ERR(0);
    r = U(GetWindowFeedbackSetting)(w, (FEEDBACK_TYPE)500, 0, &sz, &on);
    CHECK(!r);
    SET_ERR(0);
    r = U(GetWindowFeedbackSetting)((HWND)(ULONG_PTR)0xdead0, FEEDBACK_TOUCH_TAP, 0, &sz, &on);
    CHECK(!r);
    im_destroy(w);
}
TEST(input_mouse_windowfeedback_destroyed_window) {
    REQUIRE(U(SetWindowFeedbackSetting));
    HWND w = im_popup(100, 100, 100, 100);
    im_destroy(w);
    BOOL on = TRUE;
    SET_ERR(0);
    CHECK(!U(SetWindowFeedbackSetting)(w, FEEDBACK_TOUCH_TAP, 0, sizeof on, &on));
}

/* ---------- Pointer API: invalid ids ---------- */
TEST(input_mouse_pointer_info_constants_sanity) {
    REQUIRE(U(IsMouseInPointerEnabled));
    BOOL a = U(IsMouseInPointerEnabled)();
    BOOL b = U(IsMouseInPointerEnabled)();
    CHECK(a == b);
    CHECK(a == TRUE || a == FALSE);
    CHECK(a == FALSE);
}
TEST(input_mouse_enablemouseinpointer_toggle) {
    REQUIRE(U(EnableMouseInPointer)); REQUIRE(U(IsMouseInPointerEnabled));
    BOOL orig = U(IsMouseInPointerEnabled)();
    SET_ERR(0);
    BOOL r = U(EnableMouseInPointer)(orig);
    CHECK(r);
    CHECK(U(IsMouseInPointerEnabled)() == orig);
    if (!orig) {
        r = U(EnableMouseInPointer)(TRUE);
        CHECK(r);
        CHECK(U(IsMouseInPointerEnabled)());
        r = U(EnableMouseInPointer)(FALSE);
        t_note("EnableMouseInPointer(FALSE) after enabling -> %d, enabled now %d", r, U(IsMouseInPointerEnabled)());
        if (U(IsMouseInPointerEnabled)()) t_note("mouse-in-pointer could not be disabled again");
    }
}
#define INVALID_ID_CALL(label, expr) do { \
    for (int i_ = 0; i_ < BAD_ID_COUNT; i_++) { UINT32 id = bad_ids[i_]; (void)id; SET_ERR(0); BOOL r_ = (expr); \
        CHECKF(!r_, "%s id=0x%x", label, id); } } while (0)
TEST(input_mouse_pointer_invalid_id_basic_getters) {
    REQUIRE(U(GetPointerType)); REQUIRE(U(GetPointerInfo)); REQUIRE(U(GetPointerCursorId));
    POINTER_INPUT_TYPE pt = 0; POINTER_INFO pi; UINT32 cid = 0;
    memset(&pi, 0, sizeof pi);
    INVALID_ID_CALL("GetPointerType", U(GetPointerType)(id, &pt));
    INVALID_ID_CALL("GetPointerInfo", U(GetPointerInfo)(id, &pi));
    INVALID_ID_CALL("GetPointerCursorId", U(GetPointerCursorId)(id, &cid));
    CHECK(pt == 0);
}
TEST(input_mouse_pointer_invalid_id_touch_pen_getters) {
    REQUIRE(U(GetPointerTouchInfo)); REQUIRE(U(GetPointerPenInfo));
    POINTER_TOUCH_INFO ti; POINTER_PEN_INFO pe;
    memset(&ti, 0, sizeof ti); memset(&pe, 0, sizeof pe);
    INVALID_ID_CALL("GetPointerTouchInfo", U(GetPointerTouchInfo)(id, &ti));
    INVALID_ID_CALL("GetPointerPenInfo", U(GetPointerPenInfo)(id, &pe));
}
TEST(input_mouse_pointer_invalid_id_history_getters) {
    REQUIRE(U(GetPointerInfoHistory)); REQUIRE(U(GetPointerTouchInfoHistory)); REQUIRE(U(GetPointerPenInfoHistory));
    UINT32 cnt;
    POINTER_INFO pi[4]; POINTER_TOUCH_INFO ti[4]; POINTER_PEN_INFO pe[4];
    memset(pi, 0, sizeof pi); memset(ti, 0, sizeof ti); memset(pe, 0, sizeof pe);
    cnt = 4; INVALID_ID_CALL("GetPointerInfoHistory", (cnt = 4, U(GetPointerInfoHistory)(id, &cnt, pi)));
    cnt = 4; INVALID_ID_CALL("GetPointerTouchInfoHistory", (cnt = 4, U(GetPointerTouchInfoHistory)(id, &cnt, ti)));
    cnt = 4; INVALID_ID_CALL("GetPointerPenInfoHistory", (cnt = 4, U(GetPointerPenInfoHistory)(id, &cnt, pe)));
    cnt = 0; INVALID_ID_CALL("GetPointerInfoHistory(count query)", (cnt = 0, U(GetPointerInfoHistory)(id, &cnt, NULL)));
}
TEST(input_mouse_pointer_invalid_id_frame_getters) {
    REQUIRE(U(GetPointerFrameInfo)); REQUIRE(U(GetPointerFrameInfoHistory));
    REQUIRE(U(GetPointerFrameTouchInfo)); REQUIRE(U(GetPointerFrameTouchInfoHistory));
    REQUIRE(U(GetPointerFramePenInfo)); REQUIRE(U(GetPointerFramePenInfoHistory));
    UINT32 cnt, ent;
    POINTER_INFO pi[4]; POINTER_TOUCH_INFO ti[4]; POINTER_PEN_INFO pe[4];
    memset(pi, 0, sizeof pi); memset(ti, 0, sizeof ti); memset(pe, 0, sizeof pe);
    INVALID_ID_CALL("GetPointerFrameInfo", (cnt = 4, U(GetPointerFrameInfo)(id, &cnt, pi)));
    INVALID_ID_CALL("GetPointerFrameInfoHistory", (ent = 4, cnt = 4, U(GetPointerFrameInfoHistory)(id, &ent, &cnt, pi)));
    INVALID_ID_CALL("GetPointerFrameTouchInfo", (cnt = 4, U(GetPointerFrameTouchInfo)(id, &cnt, ti)));
    INVALID_ID_CALL("GetPointerFrameTouchInfoHistory", (ent = 4, cnt = 4, U(GetPointerFrameTouchInfoHistory)(id, &ent, &cnt, ti)));
    INVALID_ID_CALL("GetPointerFramePenInfo", (cnt = 4, U(GetPointerFramePenInfo)(id, &cnt, pe)));
    INVALID_ID_CALL("GetPointerFramePenInfoHistory", (ent = 4, cnt = 4, U(GetPointerFramePenInfoHistory)(id, &ent, &cnt, pe)));
}
TEST(input_mouse_pointer_invalid_id_misc) {
    REQUIRE(U(SkipPointerFrameMessages)); REQUIRE(U(GetPointerInputTransform));
    INVALID_ID_CALL("SkipPointerFrameMessages", U(SkipPointerFrameMessages)(id));
    UINT32 m[9];
    memset(m, 0, sizeof m);
    INVALID_ID_CALL("GetPointerInputTransform", U(GetPointerInputTransform)(id, 1, m));
    if (U(GetRawPointerDeviceData)) {
        POINTER_DEVICE_PROPERTY props[4]; LONG vals[4];
        memset(props, 0, sizeof props); memset(vals, 0, sizeof vals);
        INVALID_ID_CALL("GetRawPointerDeviceData", U(GetRawPointerDeviceData)(id, 1, 4, props, vals));
    }
}
TEST(input_mouse_pointer_id_zero) {
    REQUIRE(U(GetPointerType)); REQUIRE(U(GetPointerInfo));
    POINTER_INPUT_TYPE pt = 0; POINTER_INFO pi; memset(&pi, 0, sizeof pi);
    SET_ERR(0);
    CHECK(!U(GetPointerType)(0, &pt));
    SET_ERR(0);
    CHECK(!U(GetPointerInfo)(0, &pi));
    UINT32 cid;
    if (U(GetPointerCursorId)) { SET_ERR(0); CHECK(!U(GetPointerCursorId)(0, &cid)); }
}
TEST(input_mouse_pointer_invalid_id_error_code) {
    REQUIRE(U(GetPointerType)); REQUIRE(U(GetPointerInfo));
    POINTER_INPUT_TYPE pt; POINTER_INFO pi;
    SET_ERR(0);
    BOOL r = U(GetPointerType)(0x4242, &pt);
    CHECK(!r);
    t_note("GetPointerType(invalid id) err %lu", GetLastError());
    SET_ERR(0);
    r = U(GetPointerInfo)(0x4242, &pi);
    CHECK(!r);
    t_note("GetPointerInfo(invalid id) err %lu", GetLastError());
}
TEST(input_mouse_pointerdevices_enumeration) {
    REQUIRE(U(GetPointerDevices));
    UINT32 n = 0xdead;
    SET_ERR(0);
    BOOL r = U(GetPointerDevices)(&n, NULL);
    t_note("GetPointerDevices(count) -> %d count %u err %lu", r, n, GetLastError());
    if (r && n && n < 64) {
        POINTER_DEVICE_INFO *d = calloc(n + 2, sizeof *d);
        UINT32 c = n;
        CHECK(U(GetPointerDevices)(&c, d));
        CHECK(c == n);
        for (UINT32 i = 0; i < c; i++) {
            CHECK(d[i].device != NULL);
            CHECK(d[i].pointerDeviceType >= 1 && d[i].pointerDeviceType <= 4);
            if (U(GetPointerDevice)) {
                POINTER_DEVICE_INFO one; memset(&one, 0, sizeof one);
                CHECK(U(GetPointerDevice)(d[i].device, &one));
                CHECK(one.device == d[i].device && one.pointerDeviceType == d[i].pointerDeviceType);
            }
            if (U(GetPointerDeviceRects)) {
                RECT dev, disp;
                CHECK(U(GetPointerDeviceRects)(d[i].device, &dev, &disp));
                CHECK(dev.right > dev.left && dev.bottom > dev.top);
            }
            if (U(GetPointerDeviceProperties)) {
                UINT32 pc = 0;
                CHECK(U(GetPointerDeviceProperties)(d[i].device, &pc, NULL));
            }
            if (U(GetPointerDeviceCursors)) {
                UINT32 cc = 0;
                CHECK(U(GetPointerDeviceCursors)(d[i].device, &cc, NULL));
            }
        }
        if (n > 1) {
            c = n - 1;
            SET_ERR(0);
            CHECK(!U(GetPointerDevices)(&c, d));
            EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
            CHECK(c == n);
        }
        free(d);
    }
}
TEST(input_mouse_pointerdevices_invalid_device_handles) {
    REQUIRE(U(GetPointerDevice));
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xdeadbeef};
    for (unsigned i = 0; i < 4; i++) {
        HANDLE h = (HANDLE)bad[i];
        POINTER_DEVICE_INFO info; RECT r1, r2; UINT32 cnt = 1;
        memset(&info, 0, sizeof info);
        SET_ERR(0);
        CHECKF(!U(GetPointerDevice)(h, &info), "GetPointerDevice %llx", (unsigned long long)bad[i]);
        if (U(GetPointerDeviceRects)) { SET_ERR(0); CHECK(!U(GetPointerDeviceRects)(h, &r1, &r2)); }
        if (U(GetPointerDeviceProperties)) { SET_ERR(0); cnt = 0; CHECK(!U(GetPointerDeviceProperties)(h, &cnt, NULL)); }
        if (U(GetPointerDeviceCursors)) { SET_ERR(0); cnt = 0; CHECK(!U(GetPointerDeviceCursors)(h, &cnt, NULL)); }
    }
}
TEST(input_mouse_pointerdevices_null_count_param) {
    REQUIRE(U(GetPointerDevices));
    SET_ERR(0);
    t_note("GetPointerDevices(NULL,NULL) skipped: kernel-probed pointer");
    CHECK(1);
}
TEST(input_mouse_register_pointerdevice_notifications) {
    REQUIRE(U(RegisterPointerDeviceNotifications));
    HWND w = im_popup(100, 100, 100, 100);
    SET_ERR(0);
    BOOL r = U(RegisterPointerDeviceNotifications)(w, FALSE);
    t_note("RegisterPointerDeviceNotifications(w, FALSE) -> %d err %lu", r, GetLastError());
    SET_ERR(0);
    r = U(RegisterPointerDeviceNotifications)((HWND)(ULONG_PTR)0xdead0, FALSE);
    t_note("RegisterPointerDeviceNotifications(bogus hwnd) -> %d err %lu", r, GetLastError());
    SET_ERR(0);
    r = U(RegisterPointerDeviceNotifications)(NULL, FALSE);
    t_note("RegisterPointerDeviceNotifications(NULL) -> %d err %lu", r, GetLastError());
    im_destroy(w);
}
TEST(input_mouse_inittouchinjection_invalid_counts) {
    REQUIRE(U(InitializeTouchInjection));
    SET_ERR(0);
    { BOOL rr = U(InitializeTouchInjection)(0, TOUCH_FEEDBACK_DEFAULT); t_note("InitializeTouchInjection(0, TOUCH_FEEDBACK_DEFAULT) -> %d err %lu", rr, GetLastError()); if (rr) t_note("unexpected success"); }
    SET_ERR(0);
    { BOOL rr = U(InitializeTouchInjection)(257, TOUCH_FEEDBACK_DEFAULT); t_note("InitializeTouchInjection(257, TOUCH_FEEDBACK_DEFAULT) -> %d err %lu", rr, GetLastError()); if (rr) t_note("unexpected success"); }
    SET_ERR(0);
    { BOOL rr = U(InitializeTouchInjection)(0xffffffff, TOUCH_FEEDBACK_NONE); t_note("InitializeTouchInjection(0xffffffff, TOUCH_FEEDBACK_NONE) -> %d err %lu", rr, GetLastError()); if (rr) t_note("unexpected success"); }
    SET_ERR(0);
    { BOOL rr = U(InitializeTouchInjection)(1000, 0); t_note("InitializeTouchInjection(1000, 0) -> %d err %lu", rr, GetLastError()); if (rr) t_note("unexpected success"); }
    SET_ERR(0);
    BOOL r = U(InitializeTouchInjection)(1, 0x77);
    t_note("InitializeTouchInjection(1, bad mode) -> %d err %lu", r, GetLastError());
}
TEST(input_mouse_createsyntheticpointerdevice_invalid_type) {
    REQUIRE(UN(CreateSyntheticPointerDevice));
    static const POINTER_INPUT_TYPE types[] = {0, PT_POINTER, PT_MOUSE, (POINTER_INPUT_TYPE)99};
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        HSYNTHETICPOINTERDEVICE h = UN(CreateSyntheticPointerDevice)(types[i], 1, POINTER_FEEDBACK_NONE);
        CHECKF(h == NULL, "type %d created device %p", types[i], h);
        if (h && UN(DestroySyntheticPointerDevice)) UN(DestroySyntheticPointerDevice)(h);
        t_note("CreateSyntheticPointerDevice(type %d) err %lu", types[i], GetLastError());
    }
    SET_ERR(0);
    HSYNTHETICPOINTERDEVICE h = UN(CreateSyntheticPointerDevice)(PT_TOUCH, 0, POINTER_FEEDBACK_NONE);
    CHECK(h == NULL);
    if (h && UN(DestroySyntheticPointerDevice)) UN(DestroySyntheticPointerDevice)(h);
}

/* ---------- Touch hit testing (user-mode helpers) ---------- */
static TOUCH_HIT_TESTING_INPUT mk_input(int x, int y, int half) {
    TOUCH_HIT_TESTING_INPUT in; memset(&in, 0, sizeof in);
    in.pointerId = 1;
    in.point.x = x; in.point.y = y;
    in.boundingBox.left = x - half; in.boundingBox.right = x + half;
    in.boundingBox.top = y - half; in.boundingBox.bottom = y + half;
    in.nonOccludedBoundingBox = in.boundingBox;
    in.orientation = 0;
    return in;
}
TEST(input_mouse_touchhit_evaluate_null_args) {
    REQUIRE(U(EvaluateProximityToRect)); REQUIRE(U(EvaluateProximityToPolygon));
    TOUCH_HIT_TESTING_INPUT in = mk_input(100, 100, 10);
    TOUCH_HIT_TESTING_PROXIMITY_EVALUATION ev;
    RECT r = {90, 90, 110, 110};
    POINT poly[3] = {{0, 0}, {100, 0}, {0, 100}};
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToRect)(NULL, &in, &ev));
    EXPECT_ERR(0, ERROR_BAD_ARGUMENTS);
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToRect)(&r, NULL, &ev));
    EXPECT_ERR(0, ERROR_BAD_ARGUMENTS);
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToRect)(&r, &in, NULL));
    EXPECT_ERR(0, ERROR_BAD_ARGUMENTS);
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToPolygon)(3, NULL, &in, &ev));
    EXPECT_ERR(0, ERROR_BAD_ARGUMENTS);
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToPolygon)(3, poly, NULL, &ev));
    EXPECT_ERR(0, ERROR_BAD_ARGUMENTS);
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToPolygon)(3, poly, &in, NULL));
    EXPECT_ERR(0, ERROR_BAD_ARGUMENTS);
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToPolygon)(2, poly, &in, &ev));
    EXPECT_ERR(0, ERROR_BAD_ARGUMENTS);
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToPolygon)(0, poly, &in, &ev));
    SET_ERR(0);
    CHECK(!U(EvaluateProximityToPolygon)(1, poly, &in, &ev));
}
TEST(input_mouse_touchhit_evaluate_rect_point_inside) {
    REQUIRE(U(EvaluateProximityToRect));
    TOUCH_HIT_TESTING_INPUT in = mk_input(100, 100, 10);
    TOUCH_HIT_TESTING_PROXIMITY_EVALUATION ev; memset(&ev, 0xcd, sizeof ev);
    RECT r = {50, 50, 150, 150};
    SET_ERR(0);
    BOOL ok = U(EvaluateProximityToRect)(&r, &in, &ev);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        CHECKF(ev.score == 0, "score %u", ev.score);
        CHECK(ev.adjustedPoint.x == 100 && ev.adjustedPoint.y == 100);
    }
}
TEST(input_mouse_touchhit_evaluate_rect_degenerate) {
    REQUIRE(U(EvaluateProximityToRect));
    TOUCH_HIT_TESTING_INPUT in = mk_input(100, 100, 10);
    TOUCH_HIT_TESTING_PROXIMITY_EVALUATION ev; memset(&ev, 0xcd, sizeof ev);
    RECT zero_w = {120, 50, 120, 150}, zero_h = {50, 70, 150, 70};
    BOOL ok = U(EvaluateProximityToRect)(&zero_w, &in, &ev);
    CHECK(ok);
    if (ok) CHECKF(ev.score == TOUCH_HIT_TESTING_PROXIMITY_FARTHEST, "score %u", ev.score);
    memset(&ev, 0xcd, sizeof ev);
    ok = U(EvaluateProximityToRect)(&zero_h, &in, &ev);
    CHECK(ok);
    if (ok) CHECKF(ev.score == TOUCH_HIT_TESTING_PROXIMITY_FARTHEST, "score %u", ev.score);
}
TEST(input_mouse_touchhit_evaluate_rect_outside) {
    REQUIRE(U(EvaluateProximityToRect));
    TOUCH_HIT_TESTING_INPUT in = mk_input(100, 100, 10);
    TOUCH_HIT_TESTING_PROXIMITY_EVALUATION ev; memset(&ev, 0, sizeof ev);
    RECT near_r = {105, 108, 140, 140}, far_r = {400, 400, 440, 440};
    BOOL ok = U(EvaluateProximityToRect)(&near_r, &in, &ev);
    CHECK(ok);
    UINT16 near_score = ev.score;
    memset(&ev, 0, sizeof ev);
    ok = U(EvaluateProximityToRect)(&far_r, &in, &ev);
    CHECK(ok);
    if (ev.score < near_score) t_note("far rect scored better than near rect (%u < %u)", ev.score, near_score);
    CHECK(ev.score <= TOUCH_HIT_TESTING_PROXIMITY_FARTHEST);
    t_note("rect proximity: near=%u far=%u", near_score, ev.score);
}
TEST(input_mouse_touchhit_evaluate_polygon) {
    REQUIRE(U(EvaluateProximityToPolygon));
    TOUCH_HIT_TESTING_INPUT in = mk_input(100, 100, 10);
    TOUCH_HIT_TESTING_PROXIMITY_EVALUATION ev; memset(&ev, 0xcd, sizeof ev);
    POINT sq[4] = {{50, 50}, {150, 50}, {150, 150}, {50, 150}};
    SET_ERR(0);
    BOOL ok = U(EvaluateProximityToPolygon)(4, sq, &in, &ev);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        CHECKF(ev.score == 0, "score inside polygon %u", ev.score);
        CHECK(ev.adjustedPoint.x == 100 && ev.adjustedPoint.y == 100);
    }
    POINT far_sq[4] = {{500, 500}, {600, 500}, {600, 600}, {500, 600}};
    memset(&ev, 0xcd, sizeof ev);
    ok = U(EvaluateProximityToPolygon)(4, far_sq, &in, &ev);
    CHECK(ok);
    if (ok) CHECKF(ev.score <= TOUCH_HIT_TESTING_PROXIMITY_FARTHEST, "score %u", ev.score);
}
TEST(input_mouse_touchhit_pack_evaluation) {
    REQUIRE(U(PackTouchHitTestingProximityEvaluation));
    TOUCH_HIT_TESTING_INPUT in = mk_input(100, 100, 10);
    TOUCH_HIT_TESTING_PROXIMITY_EVALUATION ev; memset(&ev, 0, sizeof ev);
    ev.score = 5; ev.adjustedPoint.x = 103; ev.adjustedPoint.y = 98;
    LRESULT r = U(PackTouchHitTestingProximityEvaluation)(&in, &ev);
    LRESULT want = (LRESULT)(int)((((UINT)5 * 0x400 + (3 & 0x3ff)) * 0x400) + ((UINT)(-2) & 0x3ff));
    CHECKF(r == want, "r=0x%llx want 0x%llx", (long long)r, (long long)want);
    ev.score = 0; ev.adjustedPoint = in.point;
    CHECK(U(PackTouchHitTestingProximityEvaluation)(&in, &ev) == 0);
    ev.score = 0xfff;
    ev.adjustedPoint.x = 100; ev.adjustedPoint.y = 100;
    r = U(PackTouchHitTestingProximityEvaluation)(&in, &ev);
    want = (LRESULT)(int)(unsigned)(0xfffu * 0x400u * 0x400u);
    CHECKF(r == 0xfff00000 || r == (LRESULT)(int)0xfff00000 || r == want, "r=0x%llx", (long long)r);
    ev.score = 0x1000; /* out of range => failure value */
    r = U(PackTouchHitTestingProximityEvaluation)(&in, &ev);
    CHECKF(r == 0xfff00000, "r=0x%llx", (long long)r);
    ev.score = 1; ev.adjustedPoint.x = 100 + 0x1ff; ev.adjustedPoint.y = 100;
    r = U(PackTouchHitTestingProximityEvaluation)(&in, &ev);
    CHECKF(r == 0xfff00000, "x delta 0x1ff r=0x%llx", (long long)r);
    ev.adjustedPoint.x = 100 + 0x1fe;
    r = U(PackTouchHitTestingProximityEvaluation)(&in, &ev);
    CHECKF(r != 0xfff00000, "x delta 0x1fe r=0x%llx", (long long)r);
    ev.adjustedPoint.x = 100; ev.adjustedPoint.y = 100 - 0x1ff;
    r = U(PackTouchHitTestingProximityEvaluation)(&in, &ev);
    CHECKF(r == 0xfff00000, "y delta -0x1ff r=0x%llx", (long long)r);
}
TEST(input_mouse_touchhit_pack_fields_roundtrip) {
    REQUIRE(U(PackTouchHitTestingProximityEvaluation));
    TOUCH_HIT_TESTING_INPUT in = mk_input(300, 200, 10);
    int bad = 0;
    for (int dx = -30; dx <= 30; dx += 15)
        for (int dy = -30; dy <= 30; dy += 15)
            for (int sc = 0; sc <= 0xfff; sc += 0x555) {
                TOUCH_HIT_TESTING_PROXIMITY_EVALUATION ev; memset(&ev, 0, sizeof ev);
                ev.score = sc; ev.adjustedPoint.x = 300 + dx; ev.adjustedPoint.y = 200 + dy;
                LRESULT r = U(PackTouchHitTestingProximityEvaluation)(&in, &ev);
                unsigned u = (unsigned)r;
                int gdy = (int)(u & 0x3ff); if (gdy & 0x200) gdy -= 0x400;
                int gdx = (int)((u >> 10) & 0x3ff); if (gdx & 0x200) gdx -= 0x400;
                unsigned gsc = (u >> 20) & 0xfff;
                if (!(gdx == dx && gdy == dy && gsc == (unsigned)sc)) { if (!bad++) t_note("first mismatch dx %d dy %d sc %d -> %d %d %u", dx, dy, sc, gdx, gdy, gsc); }
            }
    CHECKF(bad == 0, "%d mismatches", bad);
}
TEST(input_mouse_touchhit_register_window) {
    REQUIRE(U(RegisterTouchHitTestingWindow));
    HWND w = im_popup(100, 100, 100, 100);
    SET_ERR(0);
    BOOL r = U(RegisterTouchHitTestingWindow)(w, TOUCH_HIT_TESTING_DEFAULT);
    t_note("RegisterTouchHitTestingWindow(w, DEFAULT) -> %d err %lu", r, GetLastError());
    SET_ERR(0);
    r = U(RegisterTouchHitTestingWindow)((HWND)(ULONG_PTR)0xdead0, TOUCH_HIT_TESTING_DEFAULT);
    t_note("RegisterTouchHitTestingWindow(bogus hwnd) -> %d err %lu", r, GetLastError());
    SET_ERR(0);
    r = U(RegisterTouchHitTestingWindow)(NULL, TOUCH_HIT_TESTING_DEFAULT);
    t_note("RegisterTouchHitTestingWindow(NULL) -> %d err %lu", r, GetLastError());
    im_destroy(w);
}

/* ---------- Input message source ---------- */
TEST(input_mouse_currentinputmessagesource_outside_message) {
    REQUIRE(U(GetCurrentInputMessageSource)); REQUIRE(U(GetCIMSSM));
    INPUT_MESSAGE_SOURCE s; memset(&s, 0xcd, sizeof s);
    SET_ERR(0);
    CHECK(U(GetCurrentInputMessageSource)(&s));
    CHECKF(s.deviceType == IMDT_UNAVAILABLE && s.originId == IMO_UNAVAILABLE, "type %d origin %d", s.deviceType, s.originId);
    memset(&s, 0xcd, sizeof s);
    CHECK(U(GetCIMSSM)(&s));
    t_note("GetCIMSSM outside message: type %d origin %d", s.deviceType, s.originId);
}
static INPUT_MESSAGE_SOURCE g_src; static int g_src_ok, g_src_seen;
static LRESULT hook_src(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_LBUTTONDOWN || m == WM_MOUSEMOVE) {
        memset(&g_src, 0xcd, sizeof g_src);
        g_src_ok = U(GetCurrentInputMessageSource)(&g_src);
        g_src_seen++;
    }
    return 0;
}
TEST(input_mouse_currentinputmessagesource_injected_mouse) {
    REQUIRE(U(GetCurrentInputMessageSource)); REQUIRE(U(SendInput)); REQUIRE(U(SetCursorPos));
    if (!im_input_ok()) { t_skip("no synthetic input delivery"); return; }
    imstate s; im_save(&s);
    HWND w = im_popup(300, 300, 200, 200);
    im_setpos(350, 350);
    im_hook = hook_src;
    g_src_seen = 0;
    INPUT in; memset(&in, 0, sizeof in); in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_MOVE; in.mi.dx = 4; in.mi.dy = 4;
    U(SendInput)(1, &in, sizeof in);
    im_pump_ms(60);
    im_hook = NULL;
    if (g_src_seen) {
        CHECK(g_src_ok);
        CHECKF(g_src.deviceType == IMDT_MOUSE, "device type %d", g_src.deviceType);
        CHECKF(g_src.originId == IMO_INJECTED, "origin %d (SendInput is injected)", g_src.originId);
    } else t_note("no mouse message observed");
    im_destroy(w);
    im_restore(&s);
}
