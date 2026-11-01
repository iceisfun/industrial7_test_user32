#include "im.h"

#define LX(l) ((int)(short)LOWORD(l))
#define LY(l) ((int)(short)HIWORD(l))
#define NEED_INPUT() do { REQUIRE(U(SendInput)); REQUIRE(U(SetCursorPos)); REQUIRE(U(GetCursorPos)); \
    if (!im_input_ok()) { t_skip("no synthetic input delivery on this desktop"); return; } } while (0)

/* ================= SetCapture / ReleaseCapture / GetCapture ================= */
TEST(input_mouse_capture_basic) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    HWND w = im_popup(100, 100, 200, 200);
    CHECK(U(GetCapture)() == NULL);
    HWND prev = U(SetCapture)(w);
    CHECK(prev == NULL);
    CHECK(U(GetCapture)() == w);
    CHECK(U(ReleaseCapture)());
    CHECK(U(GetCapture)() == NULL);
    im_destroy(w);
}
TEST(input_mouse_capture_returns_previous) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    HWND a = im_popup(100, 100, 100, 100), b = im_popup(300, 100, 100, 100);
    U(SetCapture)(a);
    CHECK(U(SetCapture)(b) == a);
    CHECK(U(GetCapture)() == b);
    CHECK(U(SetCapture)(a) == b);
    CHECK(U(GetCapture)() == a);
    U(ReleaseCapture)();
    CHECK(U(GetCapture)() == NULL);
    im_destroy(a); im_destroy(b);
}
TEST(input_mouse_capture_same_window_twice) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    HWND w = im_popup(100, 100, 100, 100);
    U(SetCapture)(w);
    im_clear();
    HWND prev = U(SetCapture)(w);
    CHECK(prev == w);
    CHECK(U(GetCapture)() == w);
    t_note("WM_CAPTURECHANGED on re-capture of same window: %d", im_count(WM_CAPTURECHANGED));
    U(ReleaseCapture)();
    im_destroy(w);
}
TEST(input_mouse_capture_changed_on_steal) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    HWND a = im_popup(100, 100, 100, 100), b = im_popup(300, 100, 100, 100);
    U(SetCapture)(a);
    im_clear();
    U(SetCapture)(b);
    imrec r;
    CHECKF(im_find(WM_CAPTURECHANGED, &r), "no WM_CAPTURECHANGED");
    if (im_find(WM_CAPTURECHANGED, &r)) {
        CHECK(r.h == a);
        CHECKF(r.l == (LPARAM)b, "lParam %llx want %p", (unsigned long long)r.l, b);
        CHECK(r.w == 0);
    }
    CHECK(im_count(WM_CAPTURECHANGED) == 1);
    U(ReleaseCapture)();
    im_destroy(a); im_destroy(b);
}
TEST(input_mouse_capture_changed_on_release) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    HWND a = im_popup(100, 100, 100, 100);
    U(SetCapture)(a);
    im_clear();
    CHECK(U(ReleaseCapture)());
    imrec r;
    CHECK(im_find(WM_CAPTURECHANGED, &r));
    if (im_find(WM_CAPTURECHANGED, &r)) { CHECK(r.h == a); CHECK(r.l == 0); }
    im_clear();
    SET_ERR(0);
    BOOL rr = U(ReleaseCapture)();
    t_note("ReleaseCapture with no capture -> %d err %lu, CAPTURECHANGED=%d", rr, GetLastError(), im_count(WM_CAPTURECHANGED));
    im_destroy(a);
}
TEST(input_mouse_capture_changed_is_synchronous) {
    REQUIRE(U(SetCapture)); REQUIRE(U(ReleaseCapture));
    HWND a = im_popup(100, 100, 100, 100), b = im_popup(300, 100, 100, 100);
    U(SetCapture)(a);
    im_clear();
    U(SetCapture)(b);
    /* no message pumping between: message must already be logged (sent, not posted) */
    CHECK(im_count(WM_CAPTURECHANGED) == 1);
    im_clear();
    U(ReleaseCapture)();
    CHECK(im_count(WM_CAPTURECHANGED) == 1);
    im_destroy(a); im_destroy(b);
}
TEST(input_mouse_capture_invalid_hwnd) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    HWND a = im_popup(100, 100, 100, 100);
    U(SetCapture)(a);
    static const ULONG_PTR bad[] = {1, 0xdeadbeef, 0x1234};
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        HWND r = U(SetCapture)((HWND)bad[i]);
        CHECKF(r == NULL, "i=%u r=%p", i, r);
        t_note("SetCapture(bogus %llx) err %lu, GetCapture=%p", (unsigned long long)bad[i], GetLastError(), U(GetCapture)());
        U(SetCapture)(a);
    }
    U(ReleaseCapture)();
    im_destroy(a);
}
TEST(input_mouse_capture_destroyed_window) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture));
    HWND a = im_popup(100, 100, 100, 100);
    im_destroy(a);
    SET_ERR(0);
    HWND r = U(SetCapture)(a);
    CHECK(r == NULL);
    CHECK(U(GetCapture)() == NULL);
}
TEST(input_mouse_capture_dies_with_window) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture));
    HWND a = im_popup(100, 100, 100, 100);
    U(SetCapture)(a);
    CHECK(U(GetCapture)() == a);
    im_destroy(a);
    CHECK(U(GetCapture)() == NULL);
}
TEST(input_mouse_capture_hidden_window) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture)); REQUIRE(U(ShowWindow));
    HWND a = im_popup(100, 100, 100, 100);
    U(ShowWindow)(a, SW_HIDE);
    HWND r = U(SetCapture)(a);
    CHECK(r == NULL);
    CHECK(U(GetCapture)() == a);
    U(ReleaseCapture)();
    im_destroy(a);
}
TEST(input_mouse_capture_hide_window_keeps_or_drops) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture)); REQUIRE(U(ShowWindow));
    HWND a = im_popup(100, 100, 100, 100);
    U(SetCapture)(a);
    U(ShowWindow)(a, SW_HIDE);
    t_note("GetCapture after hiding captured window = %s", U(GetCapture)() == a ? "still the window" : "NULL");
    U(ReleaseCapture)();
    im_destroy(a);
}
TEST(input_mouse_capture_disabled_window) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture)); REQUIRE(U(EnableWindow));
    HWND a = im_popup(100, 100, 100, 100);
    U(EnableWindow)(a, FALSE);
    HWND r = U(SetCapture)(a);
    CHECK(r == NULL);
    t_note("GetCapture for disabled window = %p (window %p)", U(GetCapture)(), a);
    U(ReleaseCapture)();
    U(EnableWindow)(a, TRUE);
    im_destroy(a);
}
TEST(input_mouse_capture_child_window) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    HWND p = im_popup(100, 100, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 50, 50, p);
    CHECK(U(SetCapture)(c) == NULL);
    CHECK(U(GetCapture)() == c);
    CHECK(U(SetCapture)(p) == c);
    CHECK(U(GetCapture)() == p);
    U(ReleaseCapture)();
    im_destroy(p);
}
TEST(input_mouse_capture_null_sets_release) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    HWND a = im_popup(100, 100, 100, 100);
    U(SetCapture)(a);
    SET_ERR(0);
    HWND r = U(SetCapture)(NULL);
    t_note("SetCapture(NULL) returned %p, GetCapture=%p, err %lu", r, U(GetCapture)(), GetLastError());
    U(ReleaseCapture)();
    im_destroy(a);
}
static HWND g_thr_wnd; static HANDLE g_thr_ready, g_thr_stop; static HWND g_thr_cap;
static DWORD WINAPI cap_thread(LPVOID a) {
    g_thr_wnd = im_popup(500, 100, 100, 100);
    g_thr_cap = U(SetCapture)(g_thr_wnd);
    SetEvent(g_thr_ready);
    while (WaitForSingleObject(g_thr_stop, 10) == WAIT_TIMEOUT) im_pump_ms(0);
    ReleaseCapture();
    im_destroy(g_thr_wnd);
    return 0;
}
TEST(input_mouse_capture_is_per_thread) {
    REQUIRE(U(SetCapture)); REQUIRE(U(GetCapture)); REQUIRE(U(ReleaseCapture));
    g_thr_ready = CreateEventA(NULL, TRUE, FALSE, NULL); g_thr_stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE h = CreateThread(NULL, 0, cap_thread, NULL, 0, NULL);
    CHECK(WaitForSingleObject(g_thr_ready, 5000) == WAIT_OBJECT_0);
    CHECK(g_thr_cap == NULL);
    CHECK(U(GetCapture)() == NULL);                /* other thread's capture is invisible here */
    HWND mine = im_popup(100, 100, 100, 100);
    SET_ERR(0);
    HWND r = U(SetCapture)(mine);
    CHECK(r == NULL);
    CHECK(U(GetCapture)() == mine);
    U(ReleaseCapture)();
    SET_ERR(0);
    r = U(SetCapture)(g_thr_wnd);
    t_note("SetCapture(window of other thread) -> %p GetCapture=%p err %lu", r, U(GetCapture)(), GetLastError());
    U(ReleaseCapture)();
    SetEvent(g_thr_stop);
    CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    CloseHandle(h); CloseHandle(g_thr_ready); CloseHandle(g_thr_stop);
    im_destroy(mine);
}
static DWORD WINAPI noqueue_thread(LPVOID a) {
    HWND *out = a;
    out[0] = GetCapture();
    out[1] = (HWND)(ULONG_PTR)ReleaseCapture();
    return 0;
}
TEST(input_mouse_capture_thread_without_queue) {
    REQUIRE(U(GetCapture));
    HWND out[2] = {(HWND)1, (HWND)1};
    HANDLE h = CreateThread(NULL, 0, noqueue_thread, out, 0, NULL);
    CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(out[0] == NULL);
    t_note("ReleaseCapture on fresh thread -> %llu", (unsigned long long)(ULONG_PTR)out[1]);
}
TEST(input_mouse_capture_redirects_mouse_messages) {
    NEED_INPUT(); REQUIRE(U(SetCapture)); REQUIRE(U(ReleaseCapture));
    imstate s; im_save(&s);
    HWND a = im_popup(100, 100, 150, 150), b = im_popup(400, 100, 150, 150);
    im_setpos(450, 150);          /* over b */
    U(SetCapture)(a);
    im_clear();
    im_setpos(460, 170);
    im_setpos(470, 180);
    imrec r;
    int got = im_find(WM_MOUSEMOVE, &r);
    CHECK(got);
    if (got) {
        CHECKF(r.h == a, "message went to %p, capture window %p (other %p)", r.h, a, b);
        CHECKF(LX(r.l) == 470 - 100 && LY(r.l) == 180 - 100, "coords relative to capture window: %d,%d", LX(r.l), LY(r.l));
    }
    for (int i = 0; i < im_nlog; i++) CHECK(!(im_log[i].m == WM_MOUSEMOVE && im_log[i].h == b));
    U(ReleaseCapture)();
    im_clear();
    im_setpos(450, 150); im_setpos(455, 155);
    if (im_find(WM_MOUSEMOVE, &r)) CHECK(r.h == b);
    im_destroy(a); im_destroy(b);
    im_restore(&s);
}
TEST(input_mouse_capture_button_to_captured_window_outside) {
    NEED_INPUT(); REQUIRE(U(SetCapture)); REQUIRE(U(ReleaseCapture));
    if (im_swapped()) { t_skip("swapped buttons"); return; }
    imstate s; im_save(&s);
    HWND a = im_popup(100, 100, 150, 150);
    im_setpos(600, 600);           /* not over any of our windows */
    U(SetCapture)(a);
    im_clear();
    INPUT in[2]; memset(in, 0, sizeof in);
    in[0].type = in[1].type = INPUT_MOUSE;
    in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    /* only meaningful if the capture window receives it: another app's window may sit at (600,600) */
    U(SendInput)(2, in, sizeof(INPUT));
    im_pump_ms(40);
    imrec r;
    if (im_find(WM_LBUTTONDOWN, &r)) CHECK(r.h == a && LX(r.l) == 500 && LY(r.l) == 500);
    else t_note("capture window did not receive button message while not foreground");
    U(ReleaseCapture)();
    im_destroy(a);
    im_restore(&s);
}

/* ================= TrackMouseEvent ================= */
static BOOL tme(HWND h, DWORD fl, DWORD hover) {
    TRACKMOUSEEVENT t; memset(&t, 0, sizeof t);
    t.cbSize = sizeof t; t.dwFlags = fl; t.hwndTrack = h; t.dwHoverTime = hover;
    return U(TrackMouseEvent)(&t);
}
static DWORD tme_query(HWND h, HWND *tracked, DWORD *hover) {
    TRACKMOUSEEVENT t; memset(&t, 0, sizeof t);
    t.cbSize = sizeof t; t.dwFlags = TME_QUERY; t.hwndTrack = h; t.dwHoverTime = 0x7777;
    if (!U(TrackMouseEvent)(&t)) return 0xdead;
    if (tracked) *tracked = t.hwndTrack;
    if (hover) *hover = t.dwHoverTime;
    return t.dwFlags;
}
TEST(input_mouse_trackmouseevent_bad_cbsize) {
    REQUIRE(U(TrackMouseEvent));
    HWND w = im_popup(100, 100, 100, 100);
    static const DWORD sz[] = {0, 1, 8, 16, 20, 23, 25, 32, 48};
    for (unsigned i = 0; i < sizeof sz / sizeof *sz; i++) {
        TRACKMOUSEEVENT t[2]; memset(t, 0, sizeof t);
        t[0].cbSize = sz[i]; t[0].dwFlags = TME_LEAVE; t[0].hwndTrack = w;
        SET_ERR(0);
        BOOL r = U(TrackMouseEvent)(&t[0]);
        CHECKF(!r, "cbSize=%lu", sz[i]);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
    im_destroy(w);
}
TEST(input_mouse_trackmouseevent_bad_hwnd) {
    REQUIRE(U(TrackMouseEvent));
    static const ULONG_PTR bad[] = {0, 1, 0xdeadbeef, 0x1234};
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL r = tme((HWND)bad[i], TME_LEAVE, 0);
        CHECKF(!r, "hwnd=%llx", (unsigned long long)bad[i]);
        EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    }
}
TEST(input_mouse_trackmouseevent_destroyed_hwnd) {
    REQUIRE(U(TrackMouseEvent));
    HWND w = im_popup(100, 100, 100, 100);
    im_destroy(w);
    SET_ERR(0);
    BOOL r = tme(w, TME_LEAVE, 0);
    CHECK(!r);
    EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
}
TEST(input_mouse_trackmouseevent_bad_flags) {
    REQUIRE(U(TrackMouseEvent));
    HWND w = im_popup(100, 100, 100, 100);
    static const DWORD f[] = {0x4, 0x8, 0x20, 0x100, 0x80000, 0x20000000, 0x7fffffff};
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        SET_ERR(0);
        BOOL r = tme(w, f[i] | TME_LEAVE, 0);
        t_note("TrackMouseEvent flags 0x%lx|LEAVE -> %d err %lu", f[i], r, GetLastError());
        tme(w, TME_LEAVE | TME_CANCEL, 0);
    }
    SET_ERR(0);
    BOOL r = tme(w, 0, 0);
    t_note("TrackMouseEvent flags 0 -> %d err %lu", r, GetLastError());
    im_destroy(w);
}
TEST(input_mouse_trackmouseevent_query_untracked) {
    REQUIRE(U(TrackMouseEvent));
    HWND w = im_popup(100, 100, 100, 100);
    HWND tr = (HWND)1; DWORD hv = 0;
    DWORD fl = tme_query(w, &tr, &hv);
    CHECKF(fl == 0, "flags 0x%lx", fl);
    t_note("untracked query hwndTrack=%p hover=0x%lx", tr, hv);
    im_destroy(w);
}
TEST(input_mouse_trackmouseevent_query_tracked_leave) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(200, 200);
    CHECK(tme(w, TME_LEAVE, 0));
    HWND tr = NULL; DWORD hv;
    DWORD fl = tme_query(w, &tr, &hv);
    CHECKF(fl == TME_LEAVE, "flags 0x%lx", fl);
    CHECK(tr == w);
    CHECK(tme(w, TME_LEAVE | TME_CANCEL, 0));
    fl = tme_query(w, &tr, &hv);
    CHECKF(fl == 0, "flags after cancel 0x%lx", fl);
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_leave_message) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(200, 200);
    im_clear();
    CHECK(tme(w, TME_LEAVE, 0));
    CHECK(im_count(WM_MOUSELEAVE) == 0);
    im_setpos(210, 210);
    CHECK(im_count(WM_MOUSELEAVE) == 0);
    im_setpos(700, 600);
    im_pump_until(WM_MOUSELEAVE, 500);
    CHECKF(im_count(WM_MOUSELEAVE) == 1, "leave=%d", im_count(WM_MOUSELEAVE));
    imrec r;
    if (im_find(WM_MOUSELEAVE, &r)) { CHECK(r.h == w); CHECK(r.w == 0 && r.l == 0); }
    /* one-shot: tracking is reset after the message */
    CHECK(tme_query(w, NULL, NULL) == 0);
    im_clear();
    im_setpos(200, 200); im_setpos(700, 600);
    im_pump_ms(50);
    CHECK(im_count(WM_MOUSELEAVE) == 0);
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_leave_twice_single_message) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(200, 200);
    im_clear();
    CHECK(tme(w, TME_LEAVE, 0));
    CHECK(tme(w, TME_LEAVE, 0));
    im_setpos(700, 600);
    im_pump_until(WM_MOUSELEAVE, 500);
    im_pump_ms(30);
    CHECKF(im_count(WM_MOUSELEAVE) == 1, "leave=%d", im_count(WM_MOUSELEAVE));
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_cancel_suppresses_leave) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(200, 200);
    im_clear();
    CHECK(tme(w, TME_LEAVE, 0));
    CHECK(tme(w, TME_LEAVE | TME_CANCEL, 0));
    im_setpos(700, 600);
    im_pump_ms(100);
    CHECKF(im_count(WM_MOUSELEAVE) == 0, "leave=%d", im_count(WM_MOUSELEAVE));
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_leave_when_cursor_outside) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(700, 600);
    im_clear();
    BOOL r = tme(w, TME_LEAVE, 0);
    CHECK(r);
    im_pump_until(WM_MOUSELEAVE, 500);
    t_note("TME_LEAVE with cursor already outside: WM_MOUSELEAVE count %d", im_count(WM_MOUSELEAVE));
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_hover) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(200, 200);
    im_pump_ms(20);
    im_clear();
    CHECK(tme(w, TME_HOVER, 100));
    HWND tr; DWORD hv;
    DWORD fl = tme_query(w, &tr, &hv);
    CHECKF(fl == TME_HOVER, "flags 0x%lx", fl);
    CHECKF(hv == 100, "hover time %lu", hv);
    im_pump_until(WM_MOUSEHOVER, 1500);
    imrec r;
    int got = im_find(WM_MOUSEHOVER, &r);
    t_note("WM_MOUSEHOVER delivered: %d", got);
    if (got) {
        CHECK(r.h == w);
        CHECKF(LX(r.l) == 100 && LY(r.l) == 100, "hover pos %d,%d", LX(r.l), LY(r.l));
        CHECK(im_count(WM_MOUSEHOVER) == 1);
        /* one-shot */
        CHECKF(tme_query(w, NULL, NULL) == 0, "tracking after hover");
    }
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_hover_default_time) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent)); REQUIRE(U(SystemParametersInfoA));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(200, 200);
    CHECK(tme(w, TME_HOVER, HOVER_DEFAULT));
    HWND tr; DWORD hv;
    DWORD fl = tme_query(w, &tr, &hv);
    CHECKF(fl == TME_HOVER, "flags 0x%lx", fl);
    UINT sys = 0;
    U(SystemParametersInfoA)(SPI_GETMOUSEHOVERTIME, 0, &sys, 0);
    t_note("hover time after HOVER_DEFAULT: %lu, system hover time %u", hv, sys);
    tme(w, TME_HOVER | TME_CANCEL, 0);
    CHECKF(tme_query(w, NULL, NULL) == 0, "after cancel");
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_hover_and_leave_combined) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(200, 200);
    CHECK(tme(w, TME_HOVER | TME_LEAVE, 100));
    DWORD fl = tme_query(w, NULL, NULL);
    CHECKF(fl == (TME_HOVER | TME_LEAVE), "flags 0x%lx", fl);
    CHECK(tme(w, TME_HOVER | TME_CANCEL, 0));
    fl = tme_query(w, NULL, NULL);
    CHECKF(fl == TME_LEAVE, "after hover cancel flags 0x%lx", fl);
    CHECK(tme(w, TME_LEAVE | TME_CANCEL, 0));
    CHECKF(tme_query(w, NULL, NULL) == 0, "after leave cancel");
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_nonclient) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_win(WS_POPUP | WS_VISIBLE | WS_BORDER, WS_EX_TOPMOST | WS_EX_NOACTIVATE, 100, 100, 200, 200, NULL);
    im_pump_ms(10);
    im_setpos(300, 102);   /* inside the 1px... top border region/client edge; harmless move */
    im_setpos(110, 110);
    im_clear();
    CHECK(tme(w, TME_LEAVE | TME_NONCLIENT, 0));
    DWORD fl = tme_query(w, NULL, NULL);
    CHECKF(fl == (TME_LEAVE | TME_NONCLIENT) || fl == TME_LEAVE, "flags 0x%lx", fl);
    tme(w, TME_LEAVE | TME_NONCLIENT | TME_CANCEL, 0);
    tme(w, TME_LEAVE | TME_CANCEL, 0);
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_destroy_while_tracking) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND w = im_popup(100, 100, 200, 200);
    im_setpos(200, 200);
    CHECK(tme(w, TME_LEAVE | TME_HOVER, 100));
    im_destroy(w);
    SET_ERR(0);
    CHECK(!tme(w, TME_QUERY, 0));
    im_setpos(700, 600);
    im_pump_ms(50);
    im_restore(&s);
}
TEST(input_mouse_trackmouseevent_two_windows_exclusive) {
    NEED_INPUT(); REQUIRE(U(TrackMouseEvent));
    imstate s; im_save(&s);
    HWND a = im_popup(100, 100, 200, 200), b = im_popup(400, 100, 200, 200);
    im_setpos(200, 200);
    CHECK(tme(a, TME_LEAVE, 0));
    CHECK(tme(b, TME_LEAVE, 0));
    HWND tr = NULL;
    DWORD fl = tme_query(b, &tr, NULL);
    t_note("after tracking a then b: query(b) flags=0x%lx track=%p (a=%p b=%p)", fl, tr, a, b);
    DWORD fa = tme_query(a, &tr, NULL);
    t_note("query(a) flags=0x%lx track=%p", fa, tr);
    tme(a, TME_LEAVE | TME_CANCEL, 0); tme(b, TME_LEAVE | TME_CANCEL, 0);
    im_destroy(a); im_destroy(b);
    im_restore(&s);
}

/* ================= WM_NCHITTEST via DefWindowProc ================= */
static LRESULT hit(HWND w, int x, int y) {
    return SendMessageA(w, WM_NCHITTEST, 0, MAKELPARAM((WORD)(short)x, (WORD)(short)y));
}
TEST(input_mouse_nchittest_thickframe_edges) {
    REQUIRE(U(GetWindowRect));
    HWND w = im_win(WS_POPUP | WS_VISIBLE | WS_THICKFRAME, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 220, 160, NULL);
    RECT r; U(GetWindowRect)(w, &r);
    int mx = (r.left + r.right) / 2, my = (r.top + r.bottom) / 2;
    CHECK(hit(w, mx, r.top) == HTTOP);
    CHECK(hit(w, mx, r.bottom - 1) == HTBOTTOM);
    CHECK(hit(w, r.left, my) == HTLEFT);
    CHECK(hit(w, r.right - 1, my) == HTRIGHT);
    CHECK(hit(w, r.left, r.top) == HTTOPLEFT);
    CHECK(hit(w, r.right - 1, r.top) == HTTOPRIGHT);
    CHECK(hit(w, r.left, r.bottom - 1) == HTBOTTOMLEFT);
    CHECK(hit(w, r.right - 1, r.bottom - 1) == HTBOTTOMRIGHT);
    CHECK(hit(w, mx, my) == HTCLIENT);
    im_destroy(w);
}
TEST(input_mouse_nchittest_outside_is_nowhere) {
    REQUIRE(U(GetWindowRect));
    HWND w = im_win(WS_POPUP | WS_VISIBLE | WS_THICKFRAME, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 220, 160, NULL);
    RECT r; U(GetWindowRect)(w, &r);
    int my = (r.top + r.bottom) / 2, mx = (r.left + r.right) / 2;
    CHECK(hit(w, r.left - 1, my) == HTNOWHERE);
    CHECK(hit(w, r.right, my) == HTNOWHERE);
    CHECK(hit(w, mx, r.top - 1) == HTNOWHERE);
    CHECK(hit(w, mx, r.bottom) == HTNOWHERE);
    CHECK(hit(w, -10, -10) == HTNOWHERE);
    CHECK(hit(w, -1, -1) == HTNOWHERE);
    CHECK(hit(w, 0x7fff, 0x7fff) == HTNOWHERE);
    CHECK(hit(w, -0x8000, -0x8000) == HTNOWHERE);
    im_destroy(w);
}
TEST(input_mouse_nchittest_caption) {
    REQUIRE(U(GetWindowRect)); REQUIRE(U(ClientToScreen));
    HWND w = im_win(WS_POPUP | WS_VISIBLE | WS_CAPTION, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 260, 200, NULL);
    RECT r; U(GetWindowRect)(w, &r);
    POINT o = {0, 0}; U(ClientToScreen)(w, &o);
    int mx = (r.left + r.right) / 2;
    CHECK(o.y - r.top > 8);
    CHECK(hit(w, mx, o.y - 4) == HTCAPTION);
    CHECK(hit(w, mx, o.y + 4) == HTCLIENT);
    CHECK(hit(w, mx, (o.y + r.bottom) / 2) == HTCLIENT);
    im_destroy(w);
}
TEST(input_mouse_nchittest_thin_border) {
    REQUIRE(U(GetWindowRect));
    HWND w = im_win(WS_POPUP | WS_VISIBLE | WS_BORDER, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 160, 120, NULL);
    RECT r; U(GetWindowRect)(w, &r);
    int my = (r.top + r.bottom) / 2, mx = (r.left + r.right) / 2;
    CHECK(hit(w, r.left, my) == HTBORDER);
    CHECK(hit(w, r.right - 1, my) == HTBORDER);
    CHECK(hit(w, mx, r.top) == HTBORDER);
    CHECK(hit(w, mx, r.bottom - 1) == HTBORDER);
    CHECK(hit(w, mx, my) == HTCLIENT);
    im_destroy(w);
}
TEST(input_mouse_nchittest_plain_popup_all_client) {
    HWND w = im_win(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 160, 120, NULL);
    CHECK(hit(w, 300, 250) == HTCLIENT);
    CHECK(hit(w, 459, 369) == HTCLIENT);
    CHECK(hit(w, 460, 370) == HTNOWHERE);
    CHECK(hit(w, 299, 250) == HTNOWHERE);
    im_destroy(w);
}
TEST(input_mouse_nchittest_child_window) {
    HWND p = im_popup(300, 250, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 20, 30, 60, 50, p);
    CHECK(hit(c, 300 + 20 + 5, 250 + 30 + 5) == HTCLIENT);
    CHECK(hit(c, 300 + 20 + 59, 250 + 30 + 49) == HTCLIENT);
    CHECK(hit(c, 300 + 20 + 60, 250 + 30 + 5) == HTNOWHERE);
    CHECK(hit(c, 300 + 5, 250 + 5) == HTNOWHERE);
    CHECK(hit(p, 300 + 25, 250 + 35) == HTCLIENT);
    im_destroy(p);
}
TEST(input_mouse_nchittest_static_transparent) {
    REQUIRE(U(CreateWindowExA));
    HWND p = im_popup(300, 250, 200, 200);
    HWND st = U(CreateWindowExA)(0, "STATIC", "x", WS_CHILD | WS_VISIBLE, 20, 30, 60, 50, p, NULL, GetModuleHandleA(NULL), NULL);
    HWND gb = U(CreateWindowExA)(0, "BUTTON", "g", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 100, 30, 60, 50, p, NULL, GetModuleHandleA(NULL), NULL);
    HWND bt = U(CreateWindowExA)(0, "BUTTON", "b", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 100, 60, 30, p, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(st && gb && bt);
    LRESULT a = hit(st, 300 + 25, 250 + 35), b = hit(gb, 300 + 130, 250 + 55), c = hit(bt, 300 + 40, 250 + 115);
    t_note("NCHITTEST static=%lld groupbox=%lld pushbutton=%lld", (long long)a, (long long)b, (long long)c);
    CHECK(c == HTCLIENT);
    CHECK(a == HTTRANSPARENT || a == HTCLIENT);
    im_destroy(p);
}
TEST(input_mouse_nchittest_disabled_window) {
    REQUIRE(U(EnableWindow));
    HWND w = im_win(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 160, 120, NULL);
    U(EnableWindow)(w, FALSE);
    LRESULT r = hit(w, 350, 300);
    t_note("NCHITTEST on disabled window -> %lld", (long long)r);
    CHECK(r == HTCLIENT);
    U(EnableWindow)(w, TRUE);
    im_destroy(w);
}
TEST(input_mouse_nchittest_lparam_sign_extension) {
    HWND w = im_win(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 0, 0, 100, 100, NULL);
    CHECK(hit(w, 50, 50) == HTCLIENT);
    /* x=0xFFFF as unsigned would be far right; as signed it is -1 and outside */
    LRESULT r = SendMessageA(w, WM_NCHITTEST, 0, MAKELPARAM(0xFFFF, 50));
    CHECK(r == HTNOWHERE);
    r = SendMessageA(w, WM_NCHITTEST, 0, MAKELPARAM(50, 0xFFFF));
    CHECK(r == HTNOWHERE);
    im_destroy(w);
}
TEST(input_mouse_nchittest_hidden_window) {
    REQUIRE(U(ShowWindow));
    HWND w = im_win(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 160, 120, NULL);
    U(ShowWindow)(w, SW_HIDE);
    LRESULT r = hit(w, 350, 300);
    t_note("NCHITTEST on hidden window -> %lld", (long long)r);
    CHECK(r == HTCLIENT);
    im_destroy(w);
}
TEST(input_mouse_nchittest_posted_vs_sent_results) {
    HWND w = im_win(WS_POPUP | WS_VISIBLE | WS_THICKFRAME, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 220, 160, NULL);
    RECT r; U(GetWindowRect)(w, &r);
    int mx = (r.left + r.right) / 2, my = (r.top + r.bottom) / 2;
    /* SendMessageW parity */
    LRESULT a = SendMessageW(w, WM_NCHITTEST, 0, MAKELPARAM(mx, my));
    LRESULT b = SendMessageA(w, WM_NCHITTEST, 0, MAKELPARAM(mx, my));
    CHECK(a == b && a == HTCLIENT);
    a = SendMessageW(w, WM_NCHITTEST, 0, MAKELPARAM(r.left, my));
    CHECK(a == HTLEFT);
    im_destroy(w);
}

/* ================= WM_SETCURSOR ================= */
static HWND g_sc_parent; static int g_sc_parent_ret; static UINT g_sc_seen;
static HWND g_sc_wp; static LPARAM g_sc_lp;
static LRESULT hook_setcursor(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (h == g_sc_parent && m == WM_SETCURSOR) {
        g_sc_seen++; g_sc_wp = (HWND)w; g_sc_lp = l;
        if (g_sc_parent_ret >= 0) { *hd = 1; return g_sc_parent_ret; }
    }
    return 0;
}
TEST(input_mouse_setcursor_msg_class_cursor_client) {
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HWND w = im_popup(300, 250, 100, 100);
    U(SetCursor)(U(LoadCursorA)(NULL, IDC_WAIT));
    LRESULT r = SendMessageA(w, WM_SETCURSOR, (WPARAM)w, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    CHECKF(U(GetCursor)() == U(LoadCursorA)(NULL, IDC_CROSS), "cursor %p want class cross %p", U(GetCursor)(), U(LoadCursorA)(NULL, IDC_CROSS));
    t_note("DefWindowProc WM_SETCURSOR(HTCLIENT) returned %lld", (long long)r);
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_setcursor_msg_plain_class_arrow) {
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HWND w = im_win_cls(im_cls_plain(), WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 100, 100, NULL);
    U(SetCursor)(U(LoadCursorA)(NULL, IDC_WAIT));
    SendMessageA(w, WM_SETCURSOR, (WPARAM)w, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    CHECK(U(GetCursor)() == U(LoadCursorA)(NULL, IDC_ARROW));
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_setcursor_msg_nonclient_sizing_cursors) {
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HWND w = im_win_cls(im_cls_plain(), WS_POPUP | WS_VISIBLE | WS_THICKFRAME, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 100, 100, NULL);
    static const struct { int ht; LPCSTR id; const char *n; int hard; } t[] = {
        {HTLEFT, IDC_SIZEWE, "left", 1}, {HTRIGHT, IDC_SIZEWE, "right", 1}, {HTTOP, IDC_SIZENS, "top", 1}, {HTBOTTOM, IDC_SIZENS, "bottom", 1},
        {HTTOPLEFT, IDC_SIZENWSE, "topleft", 1}, {HTBOTTOMRIGHT, IDC_SIZENWSE, "bottomright", 1},
        {HTTOPRIGHT, IDC_SIZENESW, "topright", 1}, {HTBOTTOMLEFT, IDC_SIZENESW, "bottomleft", 1},
        {HTCAPTION, IDC_ARROW, "caption", 1}, {HTBORDER, IDC_ARROW, "border", 1},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        U(SetCursor)(U(LoadCursorA)(NULL, IDC_WAIT));
        SendMessageA(w, WM_SETCURSOR, (WPARAM)w, MAKELPARAM(t[i].ht, WM_MOUSEMOVE));
        HCURSOR want = U(LoadCursorA)(NULL, t[i].id);
        CHECKF(U(GetCursor)() == want, "%s: cursor %p want %p", t[i].n, U(GetCursor)(), want);
    }
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_setcursor_msg_forwarded_to_parent) {
    REQUIRE(U(SetCursor));
    imstate s; im_save(&s);
    HWND p = im_popup(300, 250, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 80, 80, p);
    g_sc_parent = p; g_sc_parent_ret = TRUE; g_sc_seen = 0; g_sc_wp = NULL;
    im_hook = hook_setcursor;
    LRESULT r = SendMessageA(c, WM_SETCURSOR, (WPARAM)c, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
    im_hook = NULL;
    CHECKF(g_sc_seen == 1, "parent saw %u", g_sc_seen);
    CHECK(g_sc_wp == c);
    CHECKF(LOWORD(g_sc_lp) == HTCLIENT && HIWORD(g_sc_lp) == WM_LBUTTONDOWN, "lParam 0x%llx", (unsigned long long)g_sc_lp);
    CHECKF(r == TRUE, "child result %lld when parent handled", (long long)r);
    g_sc_parent = NULL;
    im_destroy(p);
    im_restore(&s);
}
TEST(input_mouse_setcursor_msg_parent_false_lets_child_set) {
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HWND p = im_popup(300, 250, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 80, 80, p);
    g_sc_parent = p; g_sc_parent_ret = FALSE; g_sc_seen = 0;
    im_hook = hook_setcursor;
    U(SetCursor)(U(LoadCursorA)(NULL, IDC_WAIT));
    SendMessageA(c, WM_SETCURSOR, (WPARAM)c, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    im_hook = NULL;
    CHECK(g_sc_seen == 1);
    CHECKF(U(GetCursor)() == U(LoadCursorA)(NULL, IDC_CROSS), "child class cursor applied after parent FALSE");
    g_sc_parent = NULL;
    im_destroy(p);
    im_restore(&s);
}
TEST(input_mouse_setcursor_msg_toplevel_not_forwarded) {
    REQUIRE(U(SetCursor));
    HWND p = im_popup(300, 250, 200, 200);
    HWND o = im_popup(550, 250, 100, 100);
    g_sc_parent = o; g_sc_parent_ret = TRUE; g_sc_seen = 0;
    im_hook = hook_setcursor;
    SendMessageA(p, WM_SETCURSOR, (WPARAM)p, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    im_hook = NULL;
    CHECK(g_sc_seen == 0);
    g_sc_parent = NULL;
    im_destroy(p); im_destroy(o);
}
TEST(input_mouse_setcursor_sent_on_mouse_move) {
    NEED_INPUT(); REQUIRE(U(GetCursor));
    imstate s; im_save(&s);
    HWND w = im_popup(300, 250, 200, 200);
    im_setpos(310, 260);
    im_clear();
    im_setpos(330, 280);
    im_setpos(340, 290);
    imrec r;
    int got = im_find(WM_SETCURSOR, &r);
    CHECK(got);
    if (got) {
        CHECK(r.h == w);
        CHECK((HWND)r.w == w);
        CHECKF(LOWORD(r.l) == HTCLIENT, "hit test code %d", (int)(short)LOWORD(r.l));
        CHECKF(HIWORD(r.l) == WM_MOUSEMOVE, "mouse msg 0x%x", HIWORD(r.l));
    }
    CHECK(U(GetCursor)() == U(LoadCursorA)(NULL, IDC_CROSS));
    im_destroy(w);
    im_restore(&s);
}
TEST(input_mouse_setcursor_msg_on_button_down) {
    NEED_INPUT();
    if (im_swapped()) { t_skip("swapped buttons"); return; }
    imstate s; im_save(&s);
    HWND w = im_popup(300, 250, 200, 200);
    im_setpos(350, 300);
    im_clear();
    INPUT in[2]; memset(in, 0, sizeof in);
    in[0].type = in[1].type = INPUT_MOUSE;
    in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    U(SendInput)(2, in, sizeof(INPUT));
    im_pump_ms(40);
    int found = 0;
    for (int i = 0; i < im_nlog; i++)
        if (im_log[i].m == WM_SETCURSOR && HIWORD(im_log[i].l) == WM_LBUTTONDOWN) found++;
    CHECKF(found >= 1, "no WM_SETCURSOR with WM_LBUTTONDOWN");
    im_destroy(w);
    im_restore(&s);
}

/* ================= WM_MOUSEACTIVATE ================= */
static HWND g_ma_parent; static LRESULT g_ma_ret;
static LRESULT hook_ma(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (h == g_ma_parent && m == WM_MOUSEACTIVATE) { *hd = 1; return g_ma_ret; }
    return 0;
}
TEST(input_mouse_mouseactivate_toplevel_default) {
    HWND w = im_win(WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST, 300, 250, 100, 100, NULL);
    LRESULT r = SendMessageA(w, WM_MOUSEACTIVATE, (WPARAM)w, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
    CHECKF(r == MA_ACTIVATE, "r=%lld", (long long)r);
    r = SendMessageA(w, WM_MOUSEACTIVATE, (WPARAM)w, MAKELPARAM(HTCAPTION, WM_LBUTTONDOWN));
    t_note("DefWindowProc MOUSEACTIVATE HTCAPTION -> %lld", (long long)r);
    r = SendMessageA(w, WM_MOUSEACTIVATE, (WPARAM)w, MAKELPARAM(HTCLIENT, WM_RBUTTONDOWN));
    t_note("DefWindowProc MOUSEACTIVATE rbutton -> %lld", (long long)r);
    im_destroy(w);
}
TEST(input_mouse_mouseactivate_noactivate_exstyle) {
    HWND w = im_win(WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST | WS_EX_NOACTIVATE, 300, 250, 100, 100, NULL);
    LRESULT r = SendMessageA(w, WM_MOUSEACTIVATE, (WPARAM)w, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
    t_note("WS_EX_NOACTIVATE DefWindowProc MOUSEACTIVATE -> %lld", (long long)r);
    CHECK(r == MA_NOACTIVATE || r == MA_ACTIVATE);
    im_destroy(w);
}
TEST(input_mouse_mouseactivate_child_forwards_to_parent) {
    HWND p = im_popup(300, 250, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 80, 80, p);
    static const LRESULT vals[] = {MA_ACTIVATE, MA_ACTIVATEANDEAT, MA_NOACTIVATE, MA_NOACTIVATEANDEAT};
    g_ma_parent = p;
    im_hook = hook_ma;
    for (unsigned i = 0; i < 4; i++) {
        g_ma_ret = vals[i];
        LRESULT r = SendMessageA(c, WM_MOUSEACTIVATE, (WPARAM)p, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
        CHECKF(r == vals[i], "parent returned %lld, child DefWindowProc returned %lld", (long long)vals[i], (long long)r);
    }
    im_hook = NULL;
    g_ma_parent = NULL;
    im_destroy(p);
}
TEST(input_mouse_mouseactivate_child_default) {
    HWND p = im_popup(300, 250, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 80, 80, p);
    LRESULT r = SendMessageA(c, WM_MOUSEACTIVATE, (WPARAM)p, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
    CHECKF(r == MA_ACTIVATE || r == MA_NOACTIVATE, "r=%lld", (long long)r);
    t_note("child WM_MOUSEACTIVATE default (parent has NOACTIVATE exstyle) -> %lld", (long long)r);
    im_destroy(p);
}

/* ================= Wheel propagation through DefWindowProc ================= */
static HWND g_wh_parent; static UINT g_wh_msg; static WPARAM g_wh_w; static LPARAM g_wh_l; static int g_wh_n;
static LRESULT hook_wheel(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (h == g_wh_parent && (m == WM_MOUSEWHEEL || m == WM_MOUSEHWHEEL)) { g_wh_n++; g_wh_msg = m; g_wh_w = w; g_wh_l = l; *hd = 1; return 7; }
    return 0;
}
TEST(input_mouse_wheel_defwindowproc_forwards_to_parent) {
    HWND p = im_popup(300, 250, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 80, 80, p);
    static const UINT msgs[] = {WM_MOUSEWHEEL, WM_MOUSEHWHEEL};
    g_wh_parent = p;
    im_hook = hook_wheel;
    for (unsigned i = 0; i < 2; i++) {
        g_wh_n = 0;
        WPARAM w = MAKEWPARAM(MK_CONTROL, (WORD)(short)-120);
        LPARAM l = MAKELPARAM(333, 444);
        LRESULT r = SendMessageA(c, msgs[i], w, l);
        CHECKF(g_wh_n == 1, "msg 0x%x forwarded %d times", msgs[i], g_wh_n);
        CHECK(g_wh_msg == msgs[i] && g_wh_w == w && g_wh_l == l);
        /* Win10 22H2: DefWindowProc forwards exactly once but returns 0, not the parent's result */
        t_note("child DefWindowProc wheel result %lld (parent returned 7)", (long long)r);
        CHECK(r == 7 || r == 0);
    }
    im_hook = NULL;
    g_wh_parent = NULL;
    im_destroy(p);
}
TEST(input_mouse_wheel_toplevel_defwindowproc_result) {
    HWND w = im_win(WS_POPUP | WS_VISIBLE, WS_EX_NOACTIVATE | WS_EX_TOPMOST, 300, 250, 100, 100, NULL);
    LRESULT r = SendMessageA(w, WM_MOUSEWHEEL, MAKEWPARAM(0, 120), MAKELPARAM(310, 260));
    t_note("top-level DefWindowProc(WM_MOUSEWHEEL) -> %lld", (long long)r);
    r = SendMessageA(w, WM_MOUSEHWHEEL, MAKEWPARAM(0, 120), MAKELPARAM(310, 260));
    t_note("top-level DefWindowProc(WM_MOUSEHWHEEL) -> %lld", (long long)r);
    CHECK(1);
    im_destroy(w);
}
TEST(input_mouse_mouse_messages_to_defwindowproc_no_crash) {
    HWND p = im_popup(300, 250, 200, 200);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 80, 80, p);
    static const UINT m[] = {WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK, WM_RBUTTONDOWN, WM_RBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_MOUSELEAVE};
    for (unsigned i = 0; i < sizeof m / sizeof *m; i++) {
        LRESULT r = SendMessageA(c, m[i], 0, MAKELPARAM(5, 5));
        CHECKF(r == 0, "msg 0x%x -> %lld", m[i], (long long)r);
    }
    im_destroy(p);
}
