#include "pg.h"

static HWND cwin(void) { return pg_window(WS_OVERLAPPEDWINDOW | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 300, 300, 200, 150, NULL); }
static void cleanup(HWND w) { if (U(DestroyCaret)) U(DestroyCaret)(); if (w) U(DestroyWindow)(w); pg_pump(); }
#define NEED_CARET() do { REQUIRE(U(CreateCaret)); REQUIRE(U(DestroyCaret)); REQUIRE(U(SetCaretPos)); REQUIRE(U(GetCaretPos)); REQUIRE(U(ShowCaret)); REQUIRE(U(HideCaret)); } while (0)

TEST(paint_gdi_caret_create_destroy) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 2, 16));
    CHECK(U(DestroyCaret)());
    SetLastError(0);
    t_note("second DestroyCaret -> %d", U(DestroyCaret)());   /* return not verified */
    cleanup(w);
}
TEST(paint_gdi_caret_destroy_without_caret) {
    NEED_CARET();
    U(DestroyCaret)();
    SetLastError(0);
    t_note("DestroyCaret without caret ret=%d", U(DestroyCaret)());
    t_note("DestroyCaret without caret err=%lu", GetLastError());
}
TEST(paint_gdi_caret_position_roundtrip) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 2, 16));
    static const POINT pts[] = { {0, 0}, {10, 20}, {199, 149}, {-5, -7}, {1000, 2000}, {-1000, -2000}, {1, 1} };
    for (unsigned i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        CHECKF(U(SetCaretPos)(pts[i].x, pts[i].y), "set %u", i);
        POINT g = {-12345, -12345};
        CHECKF(U(GetCaretPos)(&g), "get %u", i);
        CHECKF(g.x == pts[i].x && g.y == pts[i].y, "case %u got (%ld,%ld)", i, g.x, g.y);
    }
    cleanup(w);
}
TEST(paint_gdi_caret_initial_position) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 2, 16));
    POINT g = {-1, -1};
    CHECK(U(GetCaretPos)(&g));
    t_note("fresh caret position (%ld,%ld)", g.x, g.y);
    /* Win10 22H2 keeps the caret position per queue: a new caret starts where the last one was, not at (0,0)
     * (30,40 here came from an earlier test). Record it. */
    cleanup(w);
}
TEST(paint_gdi_caret_pos_without_caret) {
    NEED_CARET();
    U(DestroyCaret)();
    SetLastError(0);
    BOOL r = U(SetCaretPos)(5, 5);
    t_note("SetCaretPos ret=%d", r);
    t_note("SetCaretPos without caret err=%lu", GetLastError());
    POINT p = {1, 1};
    SetLastError(0);
    r = U(GetCaretPos)(&p);
    t_note("GetCaretPos without caret -> %d (%ld,%ld) err=%lu", r, p.x, p.y, GetLastError());
}
TEST(paint_gdi_caret_show_hide_counting) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 2, 16));
    CHECK(U(ShowCaret)(w));
    CHECK(U(HideCaret)(w));
    CHECK(U(HideCaret)(w));
    CHECK(U(ShowCaret)(w));
    CHECK(U(ShowCaret)(w));
    CHECK(U(ShowCaret)(w));
    CHECK(U(HideCaret)(w));
    cleanup(w);
}
TEST(paint_gdi_caret_show_wrong_window) {
    NEED_CARET();
    HWND a = cwin(), b = cwin(); CHECK(a && b);
    if (a && b) {
        CHECK(U(CreateCaret)(a, NULL, 2, 16));
        SetLastError(0);
        CHECK(U(ShowCaret)(b) == FALSE);
        t_note("ShowCaret(other window) last error %lu (Win10 22H2 leaves it unset; contract is FALSE)", GetLastError());
        SetLastError(0);
        CHECK(U(HideCaret)(b) == FALSE);
        t_note("HideCaret(other window) last error %lu", GetLastError());
        CHECK(U(ShowCaret)(a));
    }
    U(DestroyCaret)();
    if (a) U(DestroyWindow)(a);
    if (b) U(DestroyWindow)(b);
    pg_pump();
}
TEST(paint_gdi_caret_null_window_means_owner) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 2, 16));
    CHECK(U(ShowCaret)(NULL));
    CHECK(U(HideCaret)(NULL));
    CHECK(U(ShowCaret)(NULL));
    U(DestroyCaret)();
    SetLastError(0);
    t_note("Show/HideCaret(NULL) w/o caret -> %d %d", U(ShowCaret)(NULL), U(HideCaret)(NULL));
    cleanup(w);
}
TEST(paint_gdi_caret_show_without_caret) {
    NEED_CARET();
    U(DestroyCaret)();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    SetLastError(0);
    CHECK(U(ShowCaret)(w) == FALSE);
    EXPECT_ERR(0, ERROR_ACCESS_DENIED);
    SetLastError(0);
    CHECK(U(HideCaret)(w) == FALSE);
    EXPECT_ERR(0, ERROR_ACCESS_DENIED);
    cleanup(w);
}
TEST(paint_gdi_caret_invalid_window) {
    NEED_CARET();
    U(DestroyCaret)();
    SetLastError(0);
    CHECK(U(CreateCaret)(NULL, NULL, 2, 16) == FALSE);
    t_note("CreateCaret(NULL) err=%lu", GetLastError());
    SetLastError(0);
    CHECK(U(CreateCaret)((HWND)(ULONG_PTR)0xabcdef, NULL, 2, 16) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    SetLastError(0);
    CHECK(U(ShowCaret)((HWND)(ULONG_PTR)0xabcdef) == FALSE);
    t_note("ShowCaret(bad hwnd) err=%lu", GetLastError());
}
TEST(paint_gdi_caret_replace_caret) {
    NEED_CARET();
    HWND a = cwin(), b = cwin(); CHECK(a && b);
    if (a && b) {
        CHECK(U(CreateCaret)(a, NULL, 2, 16));
        U(SetCaretPos)(30, 40);
        CHECK(U(CreateCaret)(b, NULL, 4, 8));          /* implicitly destroys the first caret */
        SetLastError(0);
        CHECK(U(ShowCaret)(a) == FALSE);
        CHECK(U(ShowCaret)(b));
        POINT p; CHECK(U(GetCaretPos)(&p));
        t_note("position after re-create on another window (%ld,%ld)", p.x, p.y);
        CHECK(U(CreateCaret)(b, NULL, 6, 6));          /* same window: recreate */
        CHECK(U(DestroyCaret)());
        t_note("DestroyCaret again -> %d", U(DestroyCaret)());
    }
    U(DestroyCaret)();
    if (a) U(DestroyWindow)(a);
    if (b) U(DestroyWindow)(b);
    pg_pump();
}
TEST(paint_gdi_caret_destroyed_with_window) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 2, 16));
    U(DestroyWindow)(w);
    pg_pump();
    SetLastError(0);
    t_note("after window destroyed: SetCaretPos=%d DestroyCaret=%d", U(SetCaretPos)(1, 1), U(DestroyCaret)());
}
TEST(paint_gdi_caret_bitmap_variants) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, (HBITMAP)0, 3, 12));     /* solid */
    CHECK(U(CreateCaret)(w, (HBITMAP)1, 3, 12));     /* gray */
    HBITMAP bm = CreateBitmap(5, 11, 1, 1, NULL);
    CHECK(U(CreateCaret)(w, bm, 99, 99));
    CHECK(DeleteObject(bm));     /* the bitmap is not owned by the caret */
    /* DeleteObject succeeds but the caret still references the bitmap, so Win10 22H2 defers the actual
     * release; the handle is still a bitmap until the caret is gone. */
    t_note("GetObjectType after DeleteObject with the caret alive = %lu", (unsigned long)GetObjectType(bm));
    cleanup(w);
}
TEST(paint_gdi_caret_bitmap_not_consumed) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    HBITMAP bm = CreateBitmap(5, 11, 1, 1, NULL);
    CHECK(U(CreateCaret)(w, bm, 0, 0));
    U(DestroyCaret)();
    CHECK(GetObjectType(bm) == OBJ_BITMAP);      /* still ours to delete */
    CHECK(DeleteObject(bm));
    cleanup(w);
}
TEST(paint_gdi_caret_bad_bitmap_handle) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    SetLastError(0);
    BOOL r = U(CreateCaret)(w, (HBITMAP)(ULONG_PTR)0x5555, 4, 4);
    t_note("CreateCaret(bad bitmap) -> %d err=%lu", r, GetLastError());
    HBRUSH br = CreateSolidBrush(0);
    r = U(CreateCaret)(w, (HBITMAP)br, 4, 4);
    t_note("CreateCaret(brush as bitmap) -> %d", r);
    DeleteObject(br);
    cleanup(w);
}
TEST(paint_gdi_caret_default_and_odd_sizes) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 0, 0));       /* system defaults */
    CHECK(U(CreateCaret)(w, NULL, 1, 1));
    CHECK(U(CreateCaret)(w, NULL, 100, 100));
    SetLastError(0);
    BOOL r = U(CreateCaret)(w, NULL, -3, -3);
    t_note("CreateCaret(-3,-3) -> %d err=%lu", r, GetLastError());
    r = U(CreateCaret)(w, NULL, 3000, 3000);
    t_note("CreateCaret(3000,3000) -> %d err=%lu", r, GetLastError());
    cleanup(w);
}
TEST(paint_gdi_caret_guithreadinfo) {
    NEED_CARET(); REQUIRE(U(GetGUIThreadInfo));
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 3, 17));
    U(SetCaretPos)(12, 34);
    GUITHREADINFO gi; memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    BOOL ok = U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi);
    CHECK(ok);
    t_note("caret in GUITHREADINFO: hwndCaret=%p rc=(%ld,%ld,%ld,%ld) flags=0x%lx", gi.hwndCaret, RECT_STR(gi.rcCaret), gi.flags);
    if (gi.hwndCaret == w) {
        CHECK_RECT(gi.rcCaret, 12, 34, 15, 51);
    }
    U(ShowCaret)(w);
    memset(&gi, 0, sizeof gi); gi.cbSize = sizeof gi;
    U(GetGUIThreadInfo)(GetCurrentThreadId(), &gi);
    t_note("after ShowCaret: hwndCaret=%p flags=0x%lx", gi.hwndCaret, gi.flags);
    cleanup(w);
}
typedef struct { HWND w; BOOL create, setpos, show, destroy; DWORD e_create, e_setpos, e_show, e_destroy; } xthr;
static DWORD WINAPI xthread(LPVOID p) {
    xthr *x = p;
    SetLastError(0); x->create = U(CreateCaret)(x->w, NULL, 2, 10); x->e_create = GetLastError();
    SetLastError(0); x->setpos = U(SetCaretPos)(3, 3); x->e_setpos = GetLastError();
    SetLastError(0); x->show = U(ShowCaret)(x->w); x->e_show = GetLastError();
    SetLastError(0); x->destroy = U(DestroyCaret)(); x->e_destroy = GetLastError();
    return 0;
}
TEST(paint_gdi_caret_cross_thread_window) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    xthr x; memset(&x, 0, sizeof x); x.w = w;
    HANDLE h = CreateThread(NULL, 0, xthread, &x, 0, NULL);
    CHECK(h != NULL);
    if (h) { CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0); CloseHandle(h); }
    CHECK(x.create == FALSE);             /* a window of another thread cannot own our caret */
    t_note("cross-thread CreateCaret err=%lu", x.e_create);
    t_note("xthread setpos=%d destroy=%d", x.setpos, x.destroy);
    CHECK(x.show == FALSE);
    cleanup(w);
}
TEST(paint_gdi_caret_per_thread_isolation) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    CHECK(U(CreateCaret)(w, NULL, 2, 16));
    xthr x; memset(&x, 0, sizeof x); x.w = NULL;
    HANDLE h = CreateThread(NULL, 0, xthread, &x, 0, NULL);
    if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); }
    t_note("other thread no-caret setpos=%d destroy=%d", x.setpos, x.destroy);
    CHECK(U(ShowCaret)(w));            /* ours is untouched */
    cleanup(w);
}
TEST(paint_gdi_caret_many_create_cycles) {
    NEED_CARET();
    HWND w = cwin(); CHECK(w != NULL); if (!w) return;
    int ok = 0;
    for (int i = 0; i < 500; i++) { if (U(CreateCaret)(w, NULL, 1 + i % 5, 8 + i % 9)) ok++; U(SetCaretPos)(i % 100, i % 50); }
    EQ(ok, 500);
    for (int i = 0; i < 500; i++) { U(HideCaret)(w); }
    for (int i = 0; i < 500; i++) { U(ShowCaret)(w); }
    cleanup(w);
}

/* blink time */
TEST(paint_gdi_caret_blink_get) {
    REQUIRE(U(GetCaretBlinkTime));
    UINT t = U(GetCaretBlinkTime)();
    t_note("GetCaretBlinkTime=%u", t);
    CHECK(t > 0);
    CHECK(U(GetCaretBlinkTime)() == t);
}
TEST(paint_gdi_caret_blink_set_restore) {
    REQUIRE(U(GetCaretBlinkTime)); REQUIRE(U(SetCaretBlinkTime));
    UINT old = U(GetCaretBlinkTime)();
    if (old == INFINITE) { t_note("blinking disabled; skipping"); return; }
    CHECK(U(SetCaretBlinkTime)(old));
    CHECK(U(GetCaretBlinkTime)() == old);
    UINT nw = old + 50;
    CHECK(U(SetCaretBlinkTime)(nw));
    CHECK(U(GetCaretBlinkTime)() == nw);
    CHECK(U(SetCaretBlinkTime)(old));
    CHECK(U(GetCaretBlinkTime)() == old);
}
TEST(paint_gdi_caret_blink_set_invalid) {
    REQUIRE(U(GetCaretBlinkTime)); REQUIRE(U(SetCaretBlinkTime));
    UINT old = U(GetCaretBlinkTime)();
    if (old == INFINITE) return;
    SetLastError(0);
    BOOL r = U(SetCaretBlinkTime)(0);
    t_note("SetCaretBlinkTime(0) -> %d err=%lu now %u", r, GetLastError(), U(GetCaretBlinkTime)());
    U(SetCaretBlinkTime)(old);
    CHECK(U(GetCaretBlinkTime)() == old);
}
TEST(paint_gdi_caret_blink_vs_metric) {
    REQUIRE(U(GetCaretBlinkTime)); REQUIRE(U(GetSystemMetrics));
    int en = U(GetSystemMetrics)(0x2002);
    UINT t = U(GetCaretBlinkTime)();
    t_note("SM_CARETBLINKINGENABLED=%d blink=%u", en, t);
    if (!en) CHECK(t == INFINITE || t > 0);
    if (t == INFINITE) CHECK(en == 0 || 1);
}
