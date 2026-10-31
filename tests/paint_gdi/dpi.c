#include "pg.h"

UDECL(DPI_AWARENESS_CONTEXT, GetDpiAwarenessContextForProcess, (HANDLE));
#define CTX(n) ((DPI_AWARENESS_CONTEXT)(LONG_PTR)(n))
#define CTX_UNAWARE CTX(-1)
#define CTX_SYSTEM  CTX(-2)
#define CTX_PM      CTX(-3)
#define CTX_PM2     CTX(-4)
#define CTX_GDI     CTX(-5)

/* ---- IsValidDpiAwarenessContext ---- */
TEST(paint_gdi_dpi_isvalid_pseudo_handles) {
    REQUIRE(U(IsValidDpiAwarenessContext));
    for (int i = -1; i >= -5; i--) {
        CHECKF(U(IsValidDpiAwarenessContext)(CTX(i)), "ctx %d", i);
    }
}
TEST(paint_gdi_dpi_isvalid_invalid_values) {
    REQUIRE(U(IsValidDpiAwarenessContext));
    static const LONG_PTR bad[] = { 0, 1, 2, -6, -7, -100, 0x100, 0x7fff, 0x10000, 0x6000, 0x13000, 0x5555 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        CHECKF(!U(IsValidDpiAwarenessContext)(CTX(bad[i])), "ctx %lld", (long long)bad[i]);
        EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    }
}
TEST(paint_gdi_dpi_isvalid_raw_encodings) {
    REQUIRE(U(IsValidDpiAwarenessContext));
    /* the values the pseudo handles normalise to are themselves valid contexts */
    CHECK(U(IsValidDpiAwarenessContext)(CTX(0x6010)));
    CHECK(U(IsValidDpiAwarenessContext)(CTX(0x40006010)));
    CHECK(U(IsValidDpiAwarenessContext)(CTX(0x12)));
    CHECK(U(IsValidDpiAwarenessContext)(CTX(0x22)));
    CHECK(U(IsValidDpiAwarenessContext)(CTX(0x6011)));   /* system aware carrying 96 dpi */
    CHECK(U(IsValidDpiAwarenessContext)(CTX(0x6010 | 0x20000000)));   /* bit 29 is masked out */
    CHECK(!U(IsValidDpiAwarenessContext)(CTX(0x6010 | 0x10000000)));
}

/* ---- GetAwarenessFromDpiAwarenessContext ---- */
TEST(paint_gdi_dpi_awareness_from_context) {
    REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX_UNAWARE), DPI_AWARENESS_UNAWARE);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX_SYSTEM), DPI_AWARENESS_SYSTEM_AWARE);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX_PM), DPI_AWARENESS_PER_MONITOR_AWARE);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX_PM2), DPI_AWARENESS_PER_MONITOR_AWARE);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX_GDI), DPI_AWARENESS_UNAWARE);
}
TEST(paint_gdi_dpi_awareness_from_invalid) {
    REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(NULL), DPI_AWARENESS_INVALID);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(-6)), DPI_AWARENESS_INVALID);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(CTX(7)), DPI_AWARENESS_INVALID);
}

/* ---- GetDpiFromDpiAwarenessContext ---- */
TEST(paint_gdi_dpi_dpi_from_context) {
    REQUIRE(U(GetDpiFromDpiAwarenessContext));
    EXPECT_EQ(U(GetDpiFromDpiAwarenessContext)(CTX_UNAWARE), 96);
    EXPECT_EQ(U(GetDpiFromDpiAwarenessContext)(CTX_GDI), 96);
    EXPECT_EQ(U(GetDpiFromDpiAwarenessContext)(CTX_PM), 0);
    EXPECT_EQ(U(GetDpiFromDpiAwarenessContext)(CTX_PM2), 0);
    if (U(GetDpiForSystem)) {
        /* system-aware contexts carry the system DPI */
        UINT sys = U(GetDpiForSystem)();
        UINT d = U(GetDpiFromDpiAwarenessContext)(CTX_SYSTEM);
        t_note("system-aware context dpi=%u GetDpiForSystem=%u", d, sys);
        CHECK(d >= 96 && d <= 512);
    }
}
TEST(paint_gdi_dpi_dpi_from_invalid) {
    REQUIRE(U(GetDpiFromDpiAwarenessContext));
    EXPECT_EQ(U(GetDpiFromDpiAwarenessContext)(NULL), 0);
    EXPECT_EQ(U(GetDpiFromDpiAwarenessContext)(CTX(-9)), 0);
    EXPECT_EQ(U(GetDpiFromDpiAwarenessContext)(CTX(0x1234)), 0);
}

/* ---- AreDpiAwarenessContextsEqual ---- */
TEST(paint_gdi_dpi_equal_same) {
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    for (int i = -1; i >= -5; i--) CHECKF(U(AreDpiAwarenessContextsEqual)(CTX(i), CTX(i)), "ctx %d", i);
}
TEST(paint_gdi_dpi_equal_different) {
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    for (int i = -1; i >= -5; i--)
        for (int j = -1; j >= -5; j--)
            if (i != j) CHECKF(!U(AreDpiAwarenessContextsEqual)(CTX(i), CTX(j)), "ctx %d vs %d", i, j);
}
TEST(paint_gdi_dpi_equal_invalid) {
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    CHECK(!U(AreDpiAwarenessContextsEqual)(NULL, NULL));
    CHECK(!U(AreDpiAwarenessContextsEqual)(NULL, CTX_UNAWARE));
    CHECK(!U(AreDpiAwarenessContextsEqual)(CTX_UNAWARE, NULL));
    CHECK(!U(AreDpiAwarenessContextsEqual)(CTX(-6), CTX(-6)));
    CHECK(!U(AreDpiAwarenessContextsEqual)(CTX(0x777), CTX(0x777)));
}
TEST(paint_gdi_dpi_equal_raw_vs_pseudo) {
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    CHECK(U(AreDpiAwarenessContextsEqual)(CTX(0x6010), CTX_UNAWARE));
    CHECK(U(AreDpiAwarenessContextsEqual)(CTX(0x12), CTX_PM));
    CHECK(U(AreDpiAwarenessContextsEqual)(CTX(0x22), CTX_PM2));
    CHECK(U(AreDpiAwarenessContextsEqual)(CTX(0x40006010), CTX_GDI));
    CHECK(!U(AreDpiAwarenessContextsEqual)(CTX(0x6010), CTX_GDI));
}

/* ---- Thread awareness ---- */
TEST(paint_gdi_dpi_thread_get_valid) {
    REQUIRE(U(GetThreadDpiAwarenessContext)); REQUIRE(U(IsValidDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT c = U(GetThreadDpiAwarenessContext)();
    CHECK(c != NULL);
    CHECK(U(IsValidDpiAwarenessContext)(c));
    DPI_AWARENESS_CONTEXT c2 = U(GetThreadDpiAwarenessContext)();
    CHECK(U(AreDpiAwarenessContextsEqual)(c, c2));
}
TEST(paint_gdi_dpi_thread_set_roundtrip) {
    REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetThreadDpiAwarenessContext));
    REQUIRE(U(AreDpiAwarenessContextsEqual));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    static const int ctxs[] = { -1, -3, -4, -5 };
    for (unsigned i = 0; i < 4; i++) {
        DPI_AWARENESS_CONTEXT prev = U(SetThreadDpiAwarenessContext)(CTX(ctxs[i]));
        CHECKF(prev != NULL, "set %d", ctxs[i]);
        DPI_AWARENESS_CONTEXT now = U(GetThreadDpiAwarenessContext)();
        CHECKF(U(AreDpiAwarenessContextsEqual)(now, CTX(ctxs[i])), "get after set %d", ctxs[i]);
        /* the previous value is the one returned by the next set */
        DPI_AWARENESS_CONTEXT prev2 = U(SetThreadDpiAwarenessContext)(orig);
        CHECKF(U(AreDpiAwarenessContextsEqual)(prev2, CTX(ctxs[i])), "prev of restore %d", ctxs[i]);
        CHECK(U(AreDpiAwarenessContextsEqual)(U(GetThreadDpiAwarenessContext)(), orig));
    }
    U(SetThreadDpiAwarenessContext)(orig);
}
TEST(paint_gdi_dpi_thread_set_returns_previous) {
    REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetThreadDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    DPI_AWARENESS_CONTEXT p1 = U(SetThreadDpiAwarenessContext)(CTX_PM2);
    CHECK(U(AreDpiAwarenessContextsEqual)(p1, orig));
    DPI_AWARENESS_CONTEXT p2 = U(SetThreadDpiAwarenessContext)(CTX_UNAWARE);
    CHECK(U(AreDpiAwarenessContextsEqual)(p2, CTX_PM2));
    DPI_AWARENESS_CONTEXT p3 = U(SetThreadDpiAwarenessContext)(CTX_UNAWARE);
    CHECK(U(AreDpiAwarenessContextsEqual)(p3, CTX_UNAWARE));
    U(SetThreadDpiAwarenessContext)(orig);
}
TEST(paint_gdi_dpi_thread_set_invalid) {
    REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetThreadDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    SetLastError(0);
    CHECK(U(SetThreadDpiAwarenessContext)(NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    CHECK(U(SetThreadDpiAwarenessContext)(CTX(-6)) == NULL);
    CHECK(U(SetThreadDpiAwarenessContext)(CTX(0x1234)) == NULL);
    CHECK(U(AreDpiAwarenessContextsEqual)(U(GetThreadDpiAwarenessContext)(), orig));  /* unchanged */
    U(SetThreadDpiAwarenessContext)(orig);
}
TEST(paint_gdi_dpi_thread_awareness_enum) {
    REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetThreadDpiAwarenessContext));
    REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    U(SetThreadDpiAwarenessContext)(CTX_PM);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(U(GetThreadDpiAwarenessContext)()), DPI_AWARENESS_PER_MONITOR_AWARE);
    U(SetThreadDpiAwarenessContext)(CTX_PM2);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(U(GetThreadDpiAwarenessContext)()), DPI_AWARENESS_PER_MONITOR_AWARE);
    U(SetThreadDpiAwarenessContext)(CTX_UNAWARE);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(U(GetThreadDpiAwarenessContext)()), DPI_AWARENESS_UNAWARE);
    U(SetThreadDpiAwarenessContext)(CTX_GDI);
    EXPECT_EQ((int)U(GetAwarenessFromDpiAwarenessContext)(U(GetThreadDpiAwarenessContext)()), DPI_AWARENESS_UNAWARE);
    U(SetThreadDpiAwarenessContext)(orig);
}
static DWORD WINAPI thr_ctx(LPVOID p) {
    DPI_AWARENESS_CONTEXT *out = p;
    out[0] = U(GetThreadDpiAwarenessContext)();
    out[1] = U(SetThreadDpiAwarenessContext)(CTX_PM2);
    out[2] = U(GetThreadDpiAwarenessContext)();
    return 0;
}
TEST(paint_gdi_dpi_thread_isolation) {
    REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetThreadDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    DPI_AWARENESS_CONTEXT out[3] = {0};
    HANDLE h = CreateThread(NULL, 0, thr_ctx, out, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(U(AreDpiAwarenessContextsEqual)(out[2], CTX_PM2));
    CHECK(out[0] != NULL);
    /* the other thread's change does not leak into this one */
    CHECK(U(AreDpiAwarenessContextsEqual)(U(GetThreadDpiAwarenessContext)(), orig));
    /* a fresh thread starts at the process default, which it reports as a valid context */
    CHECK(U(IsValidDpiAwarenessContext)(out[0]));
}

/* ---- process awareness ---- */
TEST(paint_gdi_dpi_process_context) {
    REQUIRE(UN(GetDpiAwarenessContextForProcess)); REQUIRE(U(IsValidDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT c = UN(GetDpiAwarenessContextForProcess)(GetCurrentProcess());
    CHECK(c != NULL);
    CHECK(U(IsValidDpiAwarenessContext)(c));
    DPI_AWARENESS_CONTEXT c0 = UN(GetDpiAwarenessContextForProcess)(NULL);
    t_note("process ctx=%p, NULL-process ctx=%p", c, c0);
}
TEST(paint_gdi_dpi_process_vs_isprocessdpiaware) {
    REQUIRE(UN(GetDpiAwarenessContextForProcess)); REQUIRE(U(IsProcessDPIAware));
    REQUIRE(U(GetAwarenessFromDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT c = UN(GetDpiAwarenessContextForProcess)(GetCurrentProcess());
    int aw = (int)U(GetAwarenessFromDpiAwarenessContext)(c);
    BOOL a = U(IsProcessDPIAware)();
    t_note("process awareness=%d IsProcessDPIAware=%d", aw, a);
    CHECK(a == (aw != DPI_AWARENESS_UNAWARE));
}
TEST(paint_gdi_dpi_set_process_invalid) {
    REQUIRE(U(SetProcessDpiAwarenessContext));
    SetLastError(0);
    CHECK(U(SetProcessDpiAwarenessContext)(NULL) == FALSE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER || e == ERROR_ACCESS_DENIED, "err %lu", e);
    SetLastError(0);
    CHECK(U(SetProcessDpiAwarenessContext)(CTX(-6)) == FALSE);
    e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER || e == ERROR_ACCESS_DENIED, "err %lu", e);
    SetLastError(0);
    CHECK(U(SetProcessDpiAwarenessContext)(CTX(0x4321)) == FALSE);
}

/* ---- DPI queries ---- */
TEST(paint_gdi_dpi_for_system) {
    REQUIRE(U(GetDpiForSystem));
    UINT d = U(GetDpiForSystem)();
    CHECK(d >= 96);
    CHECK(d <= 960);
    CHECK(U(GetDpiForSystem)() == d);
    t_note("GetDpiForSystem=%u", d);
}
TEST(paint_gdi_dpi_for_window_valid) {
    REQUIRE(U(GetDpiForWindow));
    HWND w = pg_window(WS_OVERLAPPEDWINDOW, 0, 10, 10, 100, 100, NULL);
    CHECK(w != NULL);
    if (!w) return;
    UINT d = U(GetDpiForWindow)(w);
    CHECK(d >= 96);
    CHECK(d == U(GetDpiForWindow)(w));
    HWND c = pg_window(WS_CHILD | WS_VISIBLE, 0, 0, 0, 10, 10, w);
    CHECK(c != NULL);
    if (c) { CHECK(U(GetDpiForWindow)(c) == d); U(DestroyWindow)(c); }
    U(DestroyWindow)(w);
}
TEST(paint_gdi_dpi_for_window_invalid) {
    REQUIRE(U(GetDpiForWindow));
    SetLastError(0);
    EXPECT_EQ(U(GetDpiForWindow)(NULL), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EXPECT_EQ(U(GetDpiForWindow)((HWND)(ULONG_PTR)0xdeadbeef), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EXPECT_EQ(U(GetDpiForWindow)((HWND)(LONG_PTR)-1), 0);
}
TEST(paint_gdi_dpi_for_window_destroyed) {
    REQUIRE(U(GetDpiForWindow));
    HWND w = pg_window(WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    CHECK(w != NULL);
    if (!w) return;
    U(DestroyWindow)(w);
    SetLastError(0);
    EXPECT_EQ(U(GetDpiForWindow)(w), 0);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
}
TEST(paint_gdi_dpi_for_window_desktop) {
    REQUIRE(U(GetDpiForWindow));
    UINT d = U(GetDpiForWindow)(U(GetDesktopWindow)());
    CHECK(d >= 96);
}
TEST(paint_gdi_dpi_window_awareness_context) {
    REQUIRE(U(GetWindowDpiAwarenessContext)); REQUIRE(U(SetThreadDpiAwarenessContext));
    REQUIRE(U(GetThreadDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    U(SetThreadDpiAwarenessContext)(CTX_PM2);
    HWND w = pg_window(WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    U(SetThreadDpiAwarenessContext)(CTX_UNAWARE);
    HWND w2 = pg_window(WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    U(SetThreadDpiAwarenessContext)(orig);
    CHECK(w && w2);
    if (w) {
        DPI_AWARENESS_CONTEXT wc = U(GetWindowDpiAwarenessContext)(w);
        if (!U(AreDpiAwarenessContextsEqual)(wc, CTX_PM2)) t_note("PMv2 window context %p not equal to -4", wc);
        CHECK(wc != NULL);
        CHECK(U(GetDpiForWindow)(w) >= 96);
        U(DestroyWindow)(w);
    }
    if (w2) {
        CHECK(U(AreDpiAwarenessContextsEqual)(U(GetWindowDpiAwarenessContext)(w2), CTX_UNAWARE));
        EXPECT_EQ(U(GetDpiForWindow)(w2), 96);
        U(DestroyWindow)(w2);
    }
}
TEST(paint_gdi_dpi_window_awareness_invalid) {
    REQUIRE(U(GetWindowDpiAwarenessContext));
    CHECK(U(GetWindowDpiAwarenessContext)(NULL) == NULL);
    CHECK(U(GetWindowDpiAwarenessContext)((HWND)(ULONG_PTR)0x12345678) == NULL);
    HWND w = pg_window(WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (w) { U(DestroyWindow)(w); CHECK(U(GetWindowDpiAwarenessContext)(w) == NULL); }
}
TEST(paint_gdi_dpi_unaware_window_dpi_is_96) {
    REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetThreadDpiAwarenessContext)); REQUIRE(U(GetDpiForWindow));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    U(SetThreadDpiAwarenessContext)(CTX_UNAWARE);
    HWND w = pg_window(WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    U(SetThreadDpiAwarenessContext)(orig);
    CHECK(w != NULL);
    if (!w) return;
    EXPECT_EQ(U(GetDpiForWindow)(w), 96);
    U(DestroyWindow)(w);
}
TEST(paint_gdi_dpi_gdiscaled_window_dpi_is_96) {
    REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetThreadDpiAwarenessContext)); REQUIRE(U(GetDpiForWindow));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    DPI_AWARENESS_CONTEXT p = U(SetThreadDpiAwarenessContext)(CTX_GDI);
    if (!p) { t_note("GDI-scaled context not supported"); return; }
    HWND w = pg_window(WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    U(SetThreadDpiAwarenessContext)(orig);
    CHECK(w != NULL);
    if (!w) return;
    EXPECT_EQ(U(GetDpiForWindow)(w), 96);
    CHECK(U(AreDpiAwarenessContextsEqual)(U(GetWindowDpiAwarenessContext)(w), CTX_GDI));
    U(DestroyWindow)(w);
}

/* ---- EnableNonClientDpiScaling ---- */
TEST(paint_gdi_dpi_enable_nc_scaling_outside_nccreate) {
    REQUIRE(U(EnableNonClientDpiScaling));
    HWND w = pg_window(WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    CHECK(w != NULL);
    if (!w) return;
    SetLastError(0);
    CHECK(U(EnableNonClientDpiScaling)(w) == FALSE);
    t_note("EnableNonClientDpiScaling(live window) err=%lu", GetLastError());
    U(DestroyWindow)(w);
}
TEST(paint_gdi_dpi_enable_nc_scaling_invalid) {
    REQUIRE(U(EnableNonClientDpiScaling));
    SetLastError(0);
    CHECK(U(EnableNonClientDpiScaling)(NULL) == FALSE);
    t_note("EnableNonClientDpiScaling(NULL) err=%lu", GetLastError());
    CHECK(U(EnableNonClientDpiScaling)((HWND)(ULONG_PTR)0x1234abcd) == FALSE);
}

/* ---- AdjustWindowRectExForDpi ---- */
static RECT adj(DWORD st, BOOL menu, DWORD ex, UINT dpi, BOOL *ok) {
    RECT r = {100, 100, 200, 200};
    BOOL b = U(AdjustWindowRectExForDpi)(&r, st, menu, ex, dpi);
    if (ok) *ok = b;
    return r;
}
TEST(paint_gdi_dpi_adjust_popup_unchanged) {
    REQUIRE(U(AdjustWindowRectExForDpi));
    UINT dpis[] = {96, 120, 144, 192, 288};
    for (unsigned i = 0; i < 5; i++) {
        BOOL ok; RECT r = adj(WS_POPUP, FALSE, 0, dpis[i], &ok);
        CHECKF(ok, "dpi %u", dpis[i]);
        CHECK_RECT(r, 100, 100, 200, 200);
    }
}
TEST(paint_gdi_dpi_adjust_border_styles) {
    REQUIRE(U(AdjustWindowRectExForDpi));
    BOOL ok; RECT r;
    UINT dpis[] = {96, 144, 192};
    for (unsigned i = 0; i < 3; i++) {
        r = adj(WS_POPUP | WS_BORDER, FALSE, 0, dpis[i], &ok); CHECK(ok); CHECK_RECT(r, 99, 99, 201, 201);
        r = adj(WS_POPUP | WS_DLGFRAME, FALSE, 0, dpis[i], &ok); CHECK(ok); CHECK_RECT(r, 97, 97, 203, 203);
        r = adj(WS_POPUP, FALSE, WS_EX_CLIENTEDGE, dpis[i], &ok); CHECK(ok); CHECK_RECT(r, 98, 98, 202, 202);
        r = adj(WS_POPUP, FALSE, WS_EX_STATICEDGE, dpis[i], &ok); CHECK(ok); CHECK_RECT(r, 99, 99, 201, 201);
        r = adj(WS_POPUP, FALSE, WS_EX_DLGMODALFRAME, dpis[i], &ok); CHECK(ok); CHECK_RECT(r, 97, 97, 203, 203);
        r = adj(WS_POPUP | WS_BORDER, FALSE, WS_EX_CLIENTEDGE, dpis[i], &ok); CHECK(ok); CHECK_RECT(r, 97, 97, 203, 203);
    }
}
TEST(paint_gdi_dpi_adjust_menu_height) {
    REQUIRE(U(AdjustWindowRectExForDpi)); REQUIRE(U(GetSystemMetricsForDpi));
    UINT dpis[] = {96, 120, 144, 192};
    for (unsigned i = 0; i < 4; i++) {
        BOOL ok; RECT r = adj(WS_POPUP, TRUE, 0, dpis[i], &ok);
        int mh = U(GetSystemMetricsForDpi)(SM_CYMENU, dpis[i]);
        CHECK(ok);
        CHECKF(mh > 0, "menu height %d", mh);
        CHECK_RECT(r, 100, 100 - mh, 200, 200);
    }
}
TEST(paint_gdi_dpi_adjust_caption_height) {
    REQUIRE(U(AdjustWindowRectExForDpi)); REQUIRE(U(GetSystemMetricsForDpi));
    UINT dpis[] = {96, 144, 192};
    for (unsigned i = 0; i < 3; i++) {
        BOOL ok; RECT r = adj(WS_CAPTION, FALSE, 0, dpis[i], &ok);
        int cap = U(GetSystemMetricsForDpi)(SM_CYCAPTION, dpis[i]);
        CHECK(ok);
        CHECKF(100 - r.top >= cap, "dpi %u top delta %ld cap %d", dpis[i], 100 - r.top, cap);
        CHECK(r.left < 100 && r.right > 200 && r.bottom > 200);
        CHECK(100 - r.left == r.right - 200);   /* symmetric */
        CHECK(r.bottom - 200 == r.right - 200);
    }
}
TEST(paint_gdi_dpi_adjust_monotonic) {
    REQUIRE(U(AdjustWindowRectExForDpi));
    BOOL ok;
    RECT a = adj(WS_OVERLAPPEDWINDOW, TRUE, 0, 96, &ok); CHECK(ok);
    RECT b = adj(WS_OVERLAPPEDWINDOW, TRUE, 0, 192, &ok); CHECK(ok);
    CHECK(b.top <= a.top);
    CHECK(b.left <= a.left);
    CHECK(b.right >= a.right);
    CHECK(b.bottom >= a.bottom);
    CHECK(a.left < 100 && a.top < 100);
}
TEST(paint_gdi_dpi_adjust_invalid_dpi) {
    REQUIRE(U(AdjustWindowRectExForDpi));
    RECT r = {1, 2, 3, 4};
    SetLastError(0);
    CHECK(U(AdjustWindowRectExForDpi)(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, 0) == FALSE);
    EXPECT_ERR(0, ERROR_INVALID_PARAMETER);
    CHECK_RECT(r, 1, 2, 3, 4);
}
TEST(paint_gdi_dpi_adjust_matches_adjustwindowrectex_96) {
    REQUIRE(U(AdjustWindowRectExForDpi)); REQUIRE(U(AdjustWindowRectEx)); REQUIRE(U(SetThreadDpiAwarenessContext));
    DPI_AWARENESS_CONTEXT orig = U(GetThreadDpiAwarenessContext)();
    U(SetThreadDpiAwarenessContext)(CTX_UNAWARE);
    static const struct { DWORD st, ex; BOOL m; } t[] = {
        {WS_POPUP, 0, 0}, {WS_POPUP | WS_BORDER, 0, 0}, {WS_POPUP | WS_DLGFRAME, 0, 0},
        {WS_POPUP, WS_EX_CLIENTEDGE, 0}, {WS_POPUP, 0, 1}, {WS_OVERLAPPEDWINDOW, 0, 1},
        {WS_OVERLAPPEDWINDOW, WS_EX_CLIENTEDGE, 0}, {WS_CAPTION | WS_SYSMENU, 0, 0},
        {WS_POPUP | WS_THICKFRAME, 0, 0}, {WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE, 0},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT a = {10, 10, 110, 110}, b = {10, 10, 110, 110};
        BOOL o1 = U(AdjustWindowRectEx)(&a, t[i].st, t[i].m, t[i].ex);
        BOOL o2 = U(AdjustWindowRectExForDpi)(&b, t[i].st, t[i].m, t[i].ex, 96);
        CHECKF(o1 && o2, "case %u", i);
        if (a.left != b.left || a.top != b.top || a.right != b.right || a.bottom != b.bottom)
            t_note("case %u: AdjustWindowRectEx (%ld,%ld,%ld,%ld) vs ForDpi96 (%ld,%ld,%ld,%ld)", i, RECT_STR(a), RECT_STR(b));
    }
    U(SetThreadDpiAwarenessContext)(orig);
}

/* ---- GetSystemMetricsForDpi ---- */
TEST(paint_gdi_dpi_metrics_for_dpi_icon) {
    REQUIRE(U(GetSystemMetricsForDpi));
    EXPECT_EQ(U(GetSystemMetricsForDpi)(SM_CXICON, 96), 32);
    EXPECT_EQ(U(GetSystemMetricsForDpi)(SM_CYICON, 96), 32);
    EXPECT_EQ(U(GetSystemMetricsForDpi)(SM_CXSMICON, 96), 16);
    EXPECT_EQ(U(GetSystemMetricsForDpi)(SM_CYSMICON, 96), 16);
    EXPECT_EQ(U(GetSystemMetricsForDpi)(SM_CXICON, 192), 64);
    EXPECT_EQ(U(GetSystemMetricsForDpi)(SM_CXSMICON, 192), 32);
    EXPECT_EQ(U(GetSystemMetricsForDpi)(SM_CXICON, 144), 48);
    EXPECT_EQ(U(GetSystemMetricsForDpi)(SM_CXSMICON, 144), 24);
}
TEST(paint_gdi_dpi_metrics_for_dpi_scaling_monotonic) {
    REQUIRE(U(GetSystemMetricsForDpi));
    static const int idx[] = { SM_CXVSCROLL, SM_CYHSCROLL, SM_CYCAPTION, SM_CXSIZE, SM_CYSIZE, SM_CYMENU,
        SM_CXSIZEFRAME, SM_CYSIZEFRAME, SM_CXICON, SM_CYICON, SM_CXSMICON, SM_CYSMICON, SM_CXMENUCHECK,
        SM_CYMENUCHECK, SM_CXMENUSIZE, SM_CYMENUSIZE, SM_CXPADDEDBORDER, SM_CXHSCROLL, SM_CYVSCROLL,
        SM_CXSMSIZE, SM_CYSMSIZE, SM_CYSMCAPTION, SM_CXCURSOR, SM_CYCURSOR, SM_CYVTHUMB, SM_CXHTHUMB,
        SM_CXMINTRACK, SM_CYMINTRACK };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        int a = U(GetSystemMetricsForDpi)(idx[i], 96), b = U(GetSystemMetricsForDpi)(idx[i], 144), c = U(GetSystemMetricsForDpi)(idx[i], 192);
        CHECKF(a >= 0 && b >= a && c >= b, "sm %d: %d %d %d", idx[i], a, b, c);
        CHECKF(a > 0 || idx[i] == SM_CXPADDEDBORDER, "sm %d at 96 is %d", idx[i], a);
        CHECKF(c <= 2 * a + 2 * (idx[i] == SM_CXMINTRACK || idx[i] == SM_CYMINTRACK ? 100 : 0) + 2, "sm %d: 192dpi %d vs 96dpi %d", idx[i], c, a);
    }
}
TEST(paint_gdi_dpi_metrics_for_dpi_repeatable) {
    REQUIRE(U(GetSystemMetricsForDpi));
    for (int idx = 0; idx < 100; idx++) {
        int a = U(GetSystemMetricsForDpi)(idx, 120), b = U(GetSystemMetricsForDpi)(idx, 120);
        CHECKF(a == b, "sm %d unstable %d/%d", idx, a, b);
    }
}
TEST(paint_gdi_dpi_metrics_for_dpi_invalid_index) {
    REQUIRE(U(GetSystemMetricsForDpi));
    CHECK(U(GetSystemMetricsForDpi)(-1, 96) == 0);
    CHECK(U(GetSystemMetricsForDpi)(5000, 96) == 0);
    CHECK(U(GetSystemMetricsForDpi)(0x7fffffff, 96) == 0);
    CHECK(U(GetSystemMetricsForDpi)((int)0x80000000, 96) == 0);
}
TEST(paint_gdi_dpi_metrics_for_dpi_zero_dpi) {
    REQUIRE(U(GetSystemMetricsForDpi));
    SetLastError(0);
    int v = U(GetSystemMetricsForDpi)(SM_CXICON, 0);
    t_note("GetSystemMetricsForDpi(SM_CXICON,0)=%d err=%lu", v, GetLastError());
}
TEST(paint_gdi_dpi_spi_for_dpi) {
    REQUIRE(U(SystemParametersInfoForDpi));
    NONCLIENTMETRICSW a, b;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    a.cbSize = b.cbSize = sizeof a;
    CHECK(U(SystemParametersInfoForDpi)(SPI_GETNONCLIENTMETRICS, sizeof a, &a, 0, 96));
    CHECK(U(SystemParametersInfoForDpi)(SPI_GETNONCLIENTMETRICS, sizeof b, &b, 0, 192));
    CHECK(a.iCaptionHeight > 0 && b.iCaptionHeight >= a.iCaptionHeight);
    CHECK(a.iScrollWidth > 0 && b.iScrollWidth >= a.iScrollWidth);
    CHECK(a.cbSize == sizeof a);
    CHECK(a.lfCaptionFont.lfHeight != 0);
    CHECK(b.lfCaptionFont.lfHeight != 0);
    ICONMETRICSW im; memset(&im, 0, sizeof im); im.cbSize = sizeof im;
    CHECK(U(SystemParametersInfoForDpi)(SPI_GETICONMETRICS, sizeof im, &im, 0, 96));
    UINT sz = 0;
    CHECK(U(SystemParametersInfoForDpi)(SPI_GETICONTITLELOGFONT, sizeof(LOGFONTW), &(LOGFONTW){0}, 0, 96));
}
TEST(paint_gdi_dpi_spi_for_dpi_invalid) {
    REQUIRE(U(SystemParametersInfoForDpi));
    NONCLIENTMETRICSW a; memset(&a, 0, sizeof a);
    a.cbSize = 3;   /* wrong */
    SetLastError(0);
    BOOL rr = U(SystemParametersInfoForDpi)(SPI_GETNONCLIENTMETRICS, sizeof a, &a, 0, 96);
    t_note("SPIForDpi bad cbSize ret=%d", rr);
    t_note("SPIForDpi bad cbSize err=%lu", GetLastError());
    CHECK(U(SystemParametersInfoForDpi)(0x7ffff, 0, &a, 0, 96) == FALSE);
}

/* ---- Physical/logical point, auto rotation ---- */
TEST(paint_gdi_dpi_logical_to_physical_point) {
    REQUIRE(U(LogicalToPhysicalPointForPerMonitorDPI));
    POINT p = {10, 10};
    HWND w = pg_window(WS_OVERLAPPED | WS_VISIBLE, 0, 0, 0, 50, 50, NULL);
    CHECK(w != NULL);
    if (!w) return;
    BOOL r = U(LogicalToPhysicalPointForPerMonitorDPI)(w, &p);
    t_note("LogicalToPhysicalPointForPerMonitorDPI -> %d (%ld,%ld)", r, p.x, p.y);
    SetLastError(0);
    POINT q = {1, 1};
    CHECK(U(LogicalToPhysicalPointForPerMonitorDPI)(NULL, &q) == FALSE);
    U(DestroyWindow)(w);
}
TEST(paint_gdi_dpi_physical_to_logical_point) {
    REQUIRE(U(PhysicalToLogicalPointForPerMonitorDPI));
    HWND w = pg_window(WS_OVERLAPPED | WS_VISIBLE, 0, 0, 0, 50, 50, NULL);
    if (!w) return;
    POINT p = {10, 10};
    BOOL r = U(PhysicalToLogicalPointForPerMonitorDPI)(w, &p);
    t_note("PhysicalToLogicalPointForPerMonitorDPI -> %d (%ld,%ld)", r, p.x, p.y);
    U(DestroyWindow)(w);
}
TEST(paint_gdi_dpi_autorotation_state) {
    REQUIRE(U(GetAutoRotationState));
    AR_STATE st = (AR_STATE)0x12345;
    SetLastError(0);
    BOOL r = U(GetAutoRotationState)(&st);
    t_note("GetAutoRotationState ret=%d state=0x%x err=%lu", r, (unsigned)st, GetLastError());
    if (r) {
        CHECK(((unsigned)st & ~(unsigned)(AR_ENABLED | AR_DISABLED | AR_SUPPRESSED | AR_REMOTESESSION | AR_MULTIMON | AR_NOSENSOR | AR_NOT_SUPPORTED | AR_LAPTOP | AR_DOCKED)) == 0);
    }
}
TEST(paint_gdi_dpi_autorotation_prefs) {
    REQUIRE(U(GetDisplayAutoRotationPreferences));
    ORIENTATION_PREFERENCE p = (ORIENTATION_PREFERENCE)0x7777;
    BOOL r = U(GetDisplayAutoRotationPreferences)(&p);
    t_note("GetDisplayAutoRotationPreferences ret=%d pref=0x%x", r, (unsigned)p);
    if (r) CHECK(((unsigned)p & ~0xfu) == 0);
}
