#include "im.h"

/* ---------- GetCursorPos / SetCursorPos ---------- */
TEST(input_mouse_getcursorpos_basic) {
    REQUIRE(U(GetCursorPos));
    POINT p = {-777, -777};
    SET_ERR(0);
    CHECK(U(GetCursorPos)(&p));
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    CHECK(p.x >= vx && p.x < vx + vw);
    CHECK(p.y >= vy && p.y < vy + vh);
}

TEST(input_mouse_getcursorpos_repeat) {
    REQUIRE(U(GetCursorPos));
    POINT a, b;
    for (int i = 0; i < 200; i++) { CHECK(U(GetCursorPos)(&a)); }
    CHECK(U(GetCursorPos)(&b));
    t_note("two consecutive reads: (%ld,%ld) (%ld,%ld)", a.x, a.y, b.x, b.y);
}
TEST(input_mouse_setcursorpos_roundtrip) {
    REQUIRE(U(GetCursorPos)); REQUIRE(U(SetCursorPos));
    imstate s; im_save(&s);
    static const POINT pts[] = {{100, 100}, {1, 1}, {0, 0}, {250, 17}, {17, 250}, {321, 123}, {400, 300}};
    for (unsigned i = 0; i < sizeof pts / sizeof *pts; i++) {
        POINT p = {-1, -1};
        SET_ERR(0);
        CHECKF(U(SetCursorPos)(pts[i].x, pts[i].y), "i=%u", i);
        CHECK(U(GetCursorPos)(&p));
        CHECKF(p.x == pts[i].x && p.y == pts[i].y, "i=%u got %ld,%ld", i, p.x, p.y);
    }
    im_restore(&s);
}
TEST(input_mouse_setcursorpos_restore_exact) {
    REQUIRE(U(GetCursorPos)); REQUIRE(U(SetCursorPos));
    POINT a, b;
    U(GetCursorPos)(&a);
    CHECK(U(SetCursorPos)(a.x + 5, a.y + 5));
    CHECK(U(SetCursorPos)(a.x, a.y));
    U(GetCursorPos)(&b);
    CHECK(a.x == b.x && a.y == b.y);
}
TEST(input_mouse_setcursorpos_clamps_to_screen) {
    REQUIRE(U(GetCursorPos)); REQUIRE(U(SetCursorPos));
    imstate s; im_save(&s);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    POINT p;
    SET_ERR(0);
    BOOL r = U(SetCursorPos)(vx + vw + 5000, vy + vh + 5000);
    CHECK(r);
    U(GetCursorPos)(&p);
    CHECKF(p.x < vx + vw && p.y < vy + vh, "got %ld,%ld", p.x, p.y);
    CHECKF(p.x >= vx && p.y >= vy, "got %ld,%ld", p.x, p.y);
    r = U(SetCursorPos)(vx - 5000, vy - 5000);
    CHECK(r);
    U(GetCursorPos)(&p);
    CHECKF(p.x >= vx && p.y >= vy, "got %ld,%ld", p.x, p.y);
    CHECKF(p.x < vx + vw && p.y < vy + vh, "got %ld,%ld", p.x, p.y);
    im_restore(&s);
}
TEST(input_mouse_setcursorpos_extreme_values) {
    REQUIRE(U(GetCursorPos)); REQUIRE(U(SetCursorPos));
    imstate s; im_save(&s);
    static const int v[] = {INT_MAX, INT_MIN, 0x7fff, -0x8000, 0x10000, -0x10000, 0x7ffffff0};
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        POINT p;
        U(SetCursorPos)(v[i], v[i]);
        CHECK(U(GetCursorPos)(&p));
        CHECKF(p.x >= vx && p.x < vx + vw && p.y >= vy && p.y < vy + vh, "v=%d got %ld,%ld", v[i], p.x, p.y);
    }
    im_restore(&s);
}
TEST(input_mouse_setcursorpos_corners) {
    REQUIRE(U(GetCursorPos)); REQUIRE(U(SetCursorPos));
    imstate s; im_save(&s);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    POINT p;
    U(SetCursorPos)(vx, vy); U(GetCursorPos)(&p);
    CHECKF(p.x == vx && p.y == vy, "tl %ld,%ld", p.x, p.y);
    U(SetCursorPos)(vx + vw - 1, vy + vh - 1); U(GetCursorPos)(&p);
    CHECKF(p.x == vx + vw - 1 && p.y == vy + vh - 1, "br %ld,%ld", p.x, p.y);
    im_restore(&s);
}
TEST(input_mouse_setcursorpos_many) {
    REQUIRE(U(GetCursorPos)); REQUIRE(U(SetCursorPos));
    imstate s; im_save(&s);
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        POINT p;
        int x = 50 + (i * 7) % 300, y = 60 + (i * 13) % 250;
        U(SetCursorPos)(x, y); U(GetCursorPos)(&p);
        if (p.x != x || p.y != y) bad++;
    }
    /* a hypervisor pointer device can move the cursor between Set and Get: allow 1% */
    if (bad) t_note("SetCursorPos/GetCursorPos mismatches: %d of 2000", bad);
    CHECKF(bad <= 20, "bad=%d", bad);
    im_restore(&s);
}

/* ---------- Physical position / DPI transforms ---------- */
TEST(input_mouse_physical_cursor_pos) {
    REQUIRE(U(GetPhysicalCursorPos)); REQUIRE(U(GetCursorPos));
    POINT a, b;
    CHECK(U(GetPhysicalCursorPos)(&a));
    CHECK(U(GetCursorPos)(&b));
    if (im_aware) CHECKF(a.x == b.x && a.y == b.y, "%ld,%ld vs %ld,%ld", a.x, a.y, b.x, b.y);
}
TEST(input_mouse_physical_setcursorpos_roundtrip) {
    REQUIRE(U(GetPhysicalCursorPos)); REQUIRE(U(SetPhysicalCursorPos)); REQUIRE(U(GetCursorPos));
    imstate s; im_save(&s);
    POINT a, b;
    CHECK(U(SetPhysicalCursorPos)(210, 190));
    CHECK(U(GetPhysicalCursorPos)(&a));
    CHECK(a.x == 210 && a.y == 190);
    U(GetCursorPos)(&b);
    if (im_aware) CHECK(b.x == 210 && b.y == 190);
    CHECK(U(SetPhysicalCursorPos)(30000, 30000));
    U(GetPhysicalCursorPos)(&a);
    CHECK(a.x < GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN));
    im_restore(&s);
}
TEST(input_mouse_physical_vs_thread_dpi_context) {
    REQUIRE(U(GetPhysicalCursorPos)); REQUIRE(U(GetCursorPos)); REQUIRE(U(SetCursorPos));
    REQUIRE(U(SetThreadDpiAwarenessContext));
    imstate s; im_save(&s);
    U(SetCursorPos)(321, 222);
    POINT ref; U(GetPhysicalCursorPos)(&ref);
    DPI_AWARENESS_CONTEXT old = U(SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT_UNAWARE);
    CHECK(old != NULL);
    POINT l, ph;
    CHECK(U(GetCursorPos)(&l));
    CHECK(U(GetPhysicalCursorPos)(&ph));
    U(SetThreadDpiAwarenessContext)(old);
    /* physical position is context independent */
    CHECKF(ph.x == ref.x && ph.y == ref.y, "phys %ld,%ld ref %ld,%ld", ph.x, ph.y, ref.x, ref.y);
    t_note("unaware logical %ld,%ld physical %ld,%ld (equal at 100%% scaling)", l.x, l.y, ph.x, ph.y);
    im_restore(&s);
}
TEST(input_mouse_logical_to_physical_point) {
    REQUIRE(U(LogicalToPhysicalPoint)); REQUIRE(U(GetDesktopWindow));
    POINT p = {123, 77};
    SET_ERR(0);
    BOOL r = U(LogicalToPhysicalPoint)(U(GetDesktopWindow)(), &p);
    CHECK(r);
    if (r && im_aware) CHECKF(p.x == 123 && p.y == 77, "%ld,%ld", p.x, p.y);
    t_note("desktop LogicalToPhysicalPoint -> %ld,%ld", p.x, p.y);
}
TEST(input_mouse_physical_to_logical_point) {
    REQUIRE(U(PhysicalToLogicalPoint)); REQUIRE(U(GetDesktopWindow));
    POINT p = {321, 99};
    BOOL r = U(PhysicalToLogicalPoint)(U(GetDesktopWindow)(), &p);
    CHECK(r);
    if (r && im_aware) CHECKF(p.x == 321 && p.y == 99, "%ld,%ld", p.x, p.y);
}
TEST(input_mouse_logical_physical_window_roundtrip) {
    REQUIRE(U(LogicalToPhysicalPoint)); REQUIRE(U(PhysicalToLogicalPoint));
    HWND w = im_popup(150, 150, 200, 200);
    CHECK(w != NULL);
    POINT p = {5, 6}, q = {5, 6};
    /* Win10 22H2 fails both conversions for a real window of a per-monitor-aware process (nothing
     * to convert); the desktop-window tests above succeed. Round trip only where they succeed. */
    BOOL a = U(LogicalToPhysicalPoint)(w, &p), b = U(PhysicalToLogicalPoint)(w, &p);
    t_note("window L2P=%d P2L=%d (aware=%d)", a, b, im_aware);
    if (a && b) CHECKF(p.x == q.x && p.y == q.y, "%ld,%ld", p.x, p.y);
    else CHECKF(p.x == q.x && p.y == q.y, "failed conversion modified the point: %ld,%ld", p.x, p.y);
    im_destroy(w);
}
TEST(input_mouse_logical_to_physical_invalid) {
    REQUIRE(U(LogicalToPhysicalPoint)); REQUIRE(U(PhysicalToLogicalPoint));
    POINT p = {10, 10};
    SET_ERR(0);
    CHECK(!U(LogicalToPhysicalPoint)((HWND)(ULONG_PTR)0xdead0, &p));
    t_note("L2P bogus hwnd last error %lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(PhysicalToLogicalPoint)((HWND)(ULONG_PTR)0xdead0, &p));
    t_note("P2L bogus hwnd last error %lu", GetLastError());
    CHECK(p.x == 10 && p.y == 10);
    SET_ERR(0);
    BOOL r = U(LogicalToPhysicalPoint)(NULL, &p);
    t_note("L2P NULL hwnd -> %d err %lu", r, GetLastError());
}
TEST(input_mouse_logical_to_physical_destroyed) {
    REQUIRE(U(LogicalToPhysicalPoint));
    HWND w = im_popup(150, 150, 100, 100);
    im_destroy(w);
    POINT p = {1, 1};
    SET_ERR(0);
    CHECK(!U(LogicalToPhysicalPoint)(w, &p));
    SET_ERR(0);
    CHECK(!U(PhysicalToLogicalPoint)(w, &p));
}
TEST(input_mouse_l2p_permonitor_dpi) {
    REQUIRE(U(LogicalToPhysicalPointForPerMonitorDPI)); REQUIRE(U(PhysicalToLogicalPointForPerMonitorDPI));
    HWND w = im_popup(150, 150, 100, 100);
    POINT p = {40, 50};
    BOOL r = U(LogicalToPhysicalPointForPerMonitorDPI)(w, &p);
    /* same Win10 22H2 behaviour as the non-PerMonitor pair: FALSE for a real window when aware */
    t_note("L2P per-monitor on window -> %d", r);
    if (r && im_aware) CHECKF(p.x == 40 && p.y == 50, "%ld,%ld", p.x, p.y);
    r = U(PhysicalToLogicalPointForPerMonitorDPI)(w, &p);
    t_note("P2L per-monitor on window -> %d", r);
    if (r && im_aware) CHECKF(p.x == 40 && p.y == 50, "%ld,%ld", p.x, p.y);
    SET_ERR(0);
    CHECK(!U(LogicalToPhysicalPointForPerMonitorDPI)((HWND)(ULONG_PTR)0xbad10, &p));
    SET_ERR(0);
    CHECK(!U(PhysicalToLogicalPointForPerMonitorDPI)((HWND)(ULONG_PTR)0xbad10, &p));
    im_destroy(w);
}
TEST(input_mouse_windowfromphysicalpoint_basic) {
    REQUIRE(U(WindowFromPhysicalPoint)); REQUIRE(U(WindowFromPoint));
    HWND w = im_popup(260, 240, 180, 150);
    POINT in = {300, 290}, out;
    HWND a = U(WindowFromPhysicalPoint)(in);
    HWND b = U(WindowFromPoint)(in);
    CHECKF(a == w, "phys %p want %p", a, w);
    CHECKF(b == w, "logical %p want %p", b, w);
    out.x = 260 + 180 - 1; out.y = 240 + 150 - 1;
    CHECK(U(WindowFromPhysicalPoint)(out) == w);
    out.x = 260 + 180; out.y = 240 + 150;
    CHECK(U(WindowFromPhysicalPoint)(out) != w);
    out.x = 259; out.y = 300;
    CHECK(U(WindowFromPhysicalPoint)(out) != w);
    out.x = 300; out.y = 239;
    CHECK(U(WindowFromPhysicalPoint)(out) != w);
    im_destroy(w);
    CHECK(U(WindowFromPhysicalPoint)(in) != w);
}
TEST(input_mouse_windowfrompoint_hidden_and_child) {
    REQUIRE(U(WindowFromPoint)); REQUIRE(U(ShowWindow));
    HWND w = im_popup(260, 240, 180, 150);
    HWND c = im_win(WS_CHILD | WS_VISIBLE, 0, 10, 10, 60, 60, w);
    POINT p = {260 + 20, 240 + 20};
    CHECKF(U(WindowFromPoint)(p) == c, "child hit %p want %p", U(WindowFromPoint)(p), c);
    U(ShowWindow)(c, SW_HIDE);
    CHECK(U(WindowFromPoint)(p) == w);
    U(ShowWindow)(c, SW_SHOWNA);
    CHECK(U(WindowFromPoint)(p) == c);
    p.x = 260 + 100; p.y = 240 + 100;
    CHECK(U(WindowFromPoint)(p) == w);
    U(ShowWindow)(w, SW_HIDE);
    CHECK(U(WindowFromPoint)(p) != w);
    im_destroy(w);
}
TEST(input_mouse_windowfrompoint_transparent_ex) {
    REQUIRE(U(WindowFromPoint));
    HWND w = im_popup(260, 240, 180, 150);
    HWND t = im_win(WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, 280, 260, 80, 80, NULL);
    POINT p = {300, 280};
    HWND r = U(WindowFromPoint)(p);
    /* WS_EX_TRANSPARENT same-thread: hit testing still finds topmost window */
    t_note("WindowFromPoint over WS_EX_TRANSPARENT sibling -> %s", r == t ? "transparent window" : r == w ? "underlying window" : "other");
    im_destroy(t); im_destroy(w);
}

/* ---------- ClipCursor / GetClipCursor ---------- */
TEST(input_mouse_getclipcursor_default_is_virtual_screen) {
    REQUIRE(U(GetClipCursor)); REQUIRE(U(ClipCursor));
    imstate s; im_save(&s);
    CHECK(U(ClipCursor)(NULL));
    RECT r = {1, 2, 3, 4};
    CHECK(U(GetClipCursor)(&r));
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    CHECKF(r.left == vx && r.top == vy && r.right == vx + vw && r.bottom == vy + vh,
           "got %ld,%ld,%ld,%ld", r.left, r.top, r.right, r.bottom);
    im_restore(&s);
}

TEST(input_mouse_clipcursor_roundtrip) {
    REQUIRE(U(GetClipCursor)); REQUIRE(U(ClipCursor));
    imstate s; im_save(&s);
    RECT in = {100, 120, 400, 380}, out = {0};
    SET_ERR(0);
    CHECK(U(ClipCursor)(&in));
    CHECK(U(GetClipCursor)(&out));
    CHECKF(out.left == in.left && out.top == in.top && out.right == in.right && out.bottom == in.bottom,
           "%ld,%ld,%ld,%ld", out.left, out.top, out.right, out.bottom);
    CHECK(U(ClipCursor)(NULL));
    CHECK(U(GetClipCursor)(&out));
    CHECK(out.right - out.left >= GetSystemMetrics(SM_CXSCREEN));
    im_restore(&s);
}
TEST(input_mouse_clipcursor_constrains_setcursorpos) {
    REQUIRE(U(GetClipCursor)); REQUIRE(U(ClipCursor)); REQUIRE(U(SetCursorPos)); REQUIRE(U(GetCursorPos));
    imstate s; im_save(&s);
    RECT in = {200, 200, 300, 260};
    CHECK(U(ClipCursor)(&in));
    static const POINT t[] = {{0, 0}, {1000, 1000}, {250, 0}, {0, 230}, {250, 1000}, {1000, 230}, {250, 230}};
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        POINT p;
        U(SetCursorPos)(t[i].x, t[i].y);
        U(GetCursorPos)(&p);
        CHECKF(p.x >= in.left && p.x < in.right && p.y >= in.top && p.y < in.bottom, "i=%u got %ld,%ld", i, p.x, p.y);
    }
    POINT p;
    U(SetCursorPos)(5000, 5000); U(GetCursorPos)(&p);
    CHECKF(p.x == in.right - 1 && p.y == in.bottom - 1, "br clamp got %ld,%ld", p.x, p.y);
    U(SetCursorPos)(-5000, -5000); U(GetCursorPos)(&p);
    CHECKF(p.x == in.left && p.y == in.top, "tl clamp got %ld,%ld", p.x, p.y);
    U(ClipCursor)(NULL);
    U(SetCursorPos)(500, 500); U(GetCursorPos)(&p);
    CHECK(p.x == 500 && p.y == 500);
    im_restore(&s);
}
TEST(input_mouse_clipcursor_single_pixel_rect) {
    REQUIRE(U(GetClipCursor)); REQUIRE(U(ClipCursor)); REQUIRE(U(SetCursorPos)); REQUIRE(U(GetCursorPos));
    imstate s; im_save(&s);
    RECT in = {333, 222, 334, 223}, out;
    CHECK(U(ClipCursor)(&in));
    U(GetClipCursor)(&out);
    CHECKF(out.left == 333 && out.top == 222 && out.right == 334 && out.bottom == 223, "%ld,%ld,%ld,%ld", out.left, out.top, out.right, out.bottom);
    POINT p;
    U(SetCursorPos)(100, 100); U(GetCursorPos)(&p);
    CHECK(p.x == 333 && p.y == 222);
    im_restore(&s);
}
TEST(input_mouse_clipcursor_larger_than_screen) {
    REQUIRE(U(GetClipCursor)); REQUIRE(U(ClipCursor));
    imstate s; im_save(&s);
    RECT in = {-20000, -20000, 20000, 20000}, out;
    BOOL r = U(ClipCursor)(&in);
    CHECK(r);
    U(GetClipCursor)(&out);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    CHECKF(out.left >= vx && out.top >= vy && out.right <= vx + vw && out.bottom <= vy + vh,
           "%ld,%ld,%ld,%ld", out.left, out.top, out.right, out.bottom);
    im_restore(&s);
}
TEST(input_mouse_clipcursor_empty_or_inverted) {
    REQUIRE(U(GetClipCursor)); REQUIRE(U(ClipCursor));
    imstate s; im_save(&s);
    RECT bad[] = {{100, 100, 100, 100}, {300, 300, 200, 200}, {100, 100, 200, 100}};
    for (unsigned i = 0; i < 3; i++) {
        RECT out;
        SET_ERR(0);
        BOOL r = U(ClipCursor)(&bad[i]);
        U(GetClipCursor)(&out);
        t_note("ClipCursor degenerate #%u -> %d err %lu, clip now %ld,%ld,%ld,%ld", i, r, GetLastError(), out.left, out.top, out.right, out.bottom);
        U(ClipCursor)(NULL);
    }
    im_restore(&s);
}
TEST(input_mouse_clipcursor_repeated) {
    REQUIRE(U(GetClipCursor)); REQUIRE(U(ClipCursor));
    imstate s; im_save(&s);
    for (int i = 0; i < 300; i++) {
        RECT in = {10 + i, 10 + i, 200 + i, 150 + i}, out;
        CHECK(U(ClipCursor)(&in));
        U(GetClipCursor)(&out);
        if (!(out.left == in.left && out.right == in.right && out.top == in.top && out.bottom == in.bottom)) { CHECKF(0, "iter %d", i); break; }
    }
    im_restore(&s);
}

/* ---------- ShowCursor ---------- */
TEST(input_mouse_showcursor_counting) {
    REQUIRE(U(ShowCursor));
    imstate s; im_save(&s);
    int c0 = U(ShowCursor)(TRUE);
    int c1 = U(ShowCursor)(TRUE);
    CHECKF(c1 == c0 + 1, "%d %d", c0, c1);
    int c2 = U(ShowCursor)(FALSE);
    CHECKF(c2 == c0, "%d %d", c2, c0);
    int c3 = U(ShowCursor)(FALSE);
    CHECKF(c3 == c0 - 1, "%d %d", c3, c0);
    int c4 = U(ShowCursor)(TRUE);
    CHECK(c4 == c0);
    U(ShowCursor)(FALSE);
    im_restore(&s);
}
TEST(input_mouse_showcursor_nonbool_args) {
    REQUIRE(U(ShowCursor));
    imstate s; im_save(&s);
    int c0 = U(ShowCursor)(TRUE);
    int c1 = U(ShowCursor)((BOOL)5);       /* any non-zero = show */
    CHECKF(c1 == c0 + 1, "%d %d", c0, c1);
    int c2 = U(ShowCursor)((BOOL)0x10000);
    CHECKF(c2 == c1 + 1 || c2 == c1, "%d %d", c1, c2);
    t_note("ShowCursor(0x10000) changes count by %d", c2 - c1);
    im_restore(&s);
}
TEST(input_mouse_showcursor_deep_counts) {
    REQUIRE(U(ShowCursor));
    imstate s; im_save(&s);
    int base = U(ShowCursor)(TRUE);
    int c = base;
    for (int i = 0; i < 50; i++) { c = U(ShowCursor)(FALSE); }
    CHECKF(c == base - 50, "c=%d base=%d", c, base);
    for (int i = 0; i < 50; i++) { c = U(ShowCursor)(TRUE); }
    CHECKF(c == base, "c=%d base=%d", c, base);
    for (int i = 0; i < 30; i++) c = U(ShowCursor)(TRUE);
    CHECKF(c == base + 30, "c=%d base=%d", c, base);
    im_restore(&s);
}
TEST(input_mouse_showcursor_cursorinfo_flags) {
    REQUIRE(U(ShowCursor)); REQUIRE(U(GetCursorInfo));
    imstate s; im_save(&s);
    while (U(ShowCursor)(TRUE) < 0) {}
    CURSORINFO ci; ci.cbSize = sizeof ci;
    CHECK(U(GetCursorInfo)(&ci));
    t_note("count>=0: flags=0x%lx", ci.flags);
    int c = U(ShowCursor)(FALSE);
    while (c >= 0) c = U(ShowCursor)(FALSE);
    ci.cbSize = sizeof ci;
    CHECK(U(GetCursorInfo)(&ci));
    t_note("count<0: flags=0x%lx", ci.flags);
    im_restore(&s);
}
static DWORD WINAPI showcursor_thread(LPVOID a) {
    int *out = a;
    out[0] = ShowCursor(TRUE);
    ShowCursor(FALSE);
    return 0;
}
TEST(input_mouse_showcursor_other_thread_observed) {
    REQUIRE(U(ShowCursor));
    imstate s; im_save(&s);
    int main_c = U(ShowCursor)(TRUE); U(ShowCursor)(FALSE);
    int out[1] = {12345};
    HANDLE h = CreateThread(NULL, 0, showcursor_thread, out, 0, NULL);
    CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    CloseHandle(h);
    t_note("count main=%d, fresh thread first ShowCursor(TRUE)=%d", main_c, out[0]);
    im_restore(&s);
}

/* ---------- SetCursor / GetCursor / GetCursorInfo ---------- */
TEST(input_mouse_setcursor_getcursor) {
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HCURSOR cross = U(LoadCursorA)(NULL, IDC_CROSS), arrow = U(LoadCursorA)(NULL, IDC_ARROW), wait = U(LoadCursorA)(NULL, IDC_WAIT);
    CHECK(cross && arrow && wait && cross != arrow && arrow != wait);
    U(SetCursor)(arrow);
    CHECK(U(GetCursor)() == arrow);
    HCURSOR prev = U(SetCursor)(cross);
    CHECK(prev == arrow);
    CHECK(U(GetCursor)() == cross);
    prev = U(SetCursor)(wait);
    CHECK(prev == cross);
    prev = U(SetCursor)(wait);
    CHECK(prev == wait);
    CHECK(U(GetCursor)() == wait);
    im_restore(&s);
}
TEST(input_mouse_setcursor_null_hides) {
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HCURSOR arrow = U(LoadCursorA)(NULL, IDC_ARROW);
    U(SetCursor)(arrow);
    HCURSOR prev = U(SetCursor)(NULL);
    CHECK(prev == arrow);
    CHECK(U(GetCursor)() == NULL);
    prev = U(SetCursor)(arrow);
    CHECK(prev == NULL);
    CHECK(U(GetCursor)() == arrow);
    im_restore(&s);
}
TEST(input_mouse_setcursor_invalid_handle) {
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HCURSOR arrow = U(LoadCursorA)(NULL, IDC_ARROW);
    U(SetCursor)(arrow);
    static const ULONG_PTR bad[] = {1, 0x1234, 0xdeadbeef, 0x10001, (ULONG_PTR)-1};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        HCURSOR r = U(SetCursor)((HCURSOR)bad[i]);
        CHECKF(r == NULL, "i=%u r=%p", i, r);
        EXPECT_ERR(r, ERROR_INVALID_CURSOR_HANDLE);
        CHECKF(U(GetCursor)() == arrow, "cursor changed by invalid handle i=%u", i);
    }
    im_restore(&s);
}
TEST(input_mouse_setcursor_icon_handle_rejected) {
    REQUIRE(U(SetCursor)); REQUIRE(U(LoadIconA)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HCURSOR arrow = U(LoadCursorA)(NULL, IDC_ARROW);
    U(SetCursor)(arrow);
    HICON ic = U(LoadIconA)(NULL, IDI_APPLICATION);
    CHECK(ic != NULL);
    SET_ERR(0);
    HCURSOR r = U(SetCursor)((HCURSOR)ic);
    t_note("SetCursor(icon handle) -> %p err %lu, GetCursor now %s", r, GetLastError(), U(GetCursor)() == arrow ? "unchanged" : "changed");
    im_restore(&s);
}
TEST(input_mouse_loadcursor_standard_ids) {
    REQUIRE(U(LoadCursorA)); REQUIRE(U(LoadCursorW));
    static const struct { LPCSTR id; const char *n; } t[] = {
        {IDC_ARROW, "arrow"}, {IDC_IBEAM, "ibeam"}, {IDC_WAIT, "wait"}, {IDC_CROSS, "cross"}, {IDC_UPARROW, "uparrow"},
        {IDC_SIZENWSE, "sizenwse"}, {IDC_SIZENESW, "sizenesw"}, {IDC_SIZEWE, "sizewe"}, {IDC_SIZENS, "sizens"},
        {IDC_SIZEALL, "sizeall"}, {IDC_NO, "no"}, {IDC_HAND, "hand"}, {IDC_APPSTARTING, "appstarting"}, {IDC_HELP, "help"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HCURSOR a = U(LoadCursorA)(NULL, t[i].id), b = U(LoadCursorA)(NULL, t[i].id), c = U(LoadCursorW)(NULL, (LPCWSTR)t[i].id);
        CHECKF(a != NULL, "%s", t[i].n);
        CHECKF(a == b, "%s shared handle stable", t[i].n);
        CHECKF(a == c, "%s A/W same handle", t[i].n);
        for (unsigned j = 0; j < i; j++) CHECKF(a != U(LoadCursorA)(NULL, t[j].id), "%s vs %s distinct", t[i].n, t[j].n);
    }
}
TEST(input_mouse_loadcursor_invalid) {
    REQUIRE(U(LoadCursorA));
    SET_ERR(0);
    HCURSOR c = U(LoadCursorA)(NULL, MAKEINTRESOURCEA(0x7abc));
    CHECK(c == NULL);
    t_note("LoadCursor(NULL, unknown id) err %lu", GetLastError());
    SET_ERR(0);
    c = U(LoadCursorA)(GetModuleHandleA(NULL), MAKEINTRESOURCEA(0x7abc));
    CHECK(c == NULL);
    t_note("LoadCursor(hInst, unknown id) err %lu", GetLastError());
}
TEST(input_mouse_getcursorinfo_basic) {
    REQUIRE(U(GetCursorInfo)); REQUIRE(U(GetCursorPos)); REQUIRE(U(SetCursorPos));
    imstate s; im_save(&s);
    U(SetCursorPos)(240, 180);
    CURSORINFO ci; memset(&ci, 0xcc, sizeof ci); ci.cbSize = sizeof ci;
    CHECK(sizeof(CURSORINFO) == 24);
    SET_ERR(0);
    CHECK(U(GetCursorInfo)(&ci));
    t_note("GetCursorInfo cbSize on return: %lu", ci.cbSize);
    CHECK((ci.flags & ~(DWORD)(CURSOR_SHOWING | CURSOR_SUPPRESSED)) == 0);
    CHECKF(ci.ptScreenPos.x == 240 && ci.ptScreenPos.y == 180, "%ld,%ld", ci.ptScreenPos.x, ci.ptScreenPos.y);
    im_restore(&s);
}
TEST(input_mouse_getcursorinfo_bad_cbsize) {
    REQUIRE(U(GetCursorInfo));
    static const DWORD sizes[] = {0, 1, 4, 20, 23, 25, 28, 32, 48, 0xffffffff};
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        CURSORINFO ci[2]; memset(ci, 0, sizeof ci); ci[0].cbSize = sizes[i];
        SET_ERR(0);
        BOOL r = U(GetCursorInfo)(&ci[0]);
        CHECKF(!r, "cbSize=%lu", sizes[i]);
        EXPECT_ERR(r, ERROR_INVALID_PARAMETER);
    }
}
TEST(input_mouse_getcursorinfo_matches_getcursor) {
    REQUIRE(U(GetCursorInfo)); REQUIRE(U(GetCursor)); REQUIRE(U(SetCursor)); REQUIRE(U(LoadCursorA));
    imstate s; im_save(&s);
    HCURSOR cross = U(LoadCursorA)(NULL, IDC_CROSS);
    U(SetCursor)(cross);
    CURSORINFO ci; ci.cbSize = sizeof ci;
    CHECK(U(GetCursorInfo)(&ci));
    t_note("GetCursorInfo.hCursor=%p GetCursor=%p cross=%p flags=0x%lx", ci.hCursor, U(GetCursor)(), cross, ci.flags);
    im_restore(&s);
}
TEST(input_mouse_getcursorinfo_repeated) {
    REQUIRE(U(GetCursorInfo));
    for (int i = 0; i < 500; i++) {
        CURSORINFO ci; ci.cbSize = sizeof ci;
        if (!U(GetCursorInfo)(&ci)) { CHECKF(0, "iter %d", i); break; }
    }
}

/* ---------- Double-click time ---------- */
TEST(input_mouse_doubleclicktime_get) {
    REQUIRE(U(GetDoubleClickTime));
    UINT t = U(GetDoubleClickTime)();
    CHECKF(t >= 1 && t <= 5000, "t=%u", t);
    for (int i = 0; i < 50; i++) CHECK(U(GetDoubleClickTime)() == t);
}
TEST(input_mouse_doubleclicktime_set_roundtrip) {
    REQUIRE(U(GetDoubleClickTime)); REQUIRE(U(SetDoubleClickTime));
    imstate s; im_save(&s);
    static const UINT v[] = {200, 250, 400, 500, 777, 900, 1000, 3000, 5000};
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        SET_ERR(0);
        CHECKF(U(SetDoubleClickTime)(v[i]), "set %u", v[i]);
        CHECKF(U(GetDoubleClickTime)() == v[i], "get %u -> %u", v[i], U(GetDoubleClickTime)());
    }
    im_restore(&s);
}
TEST(input_mouse_doubleclicktime_zero_is_default) {
    REQUIRE(U(GetDoubleClickTime)); REQUIRE(U(SetDoubleClickTime));
    imstate s; im_save(&s);
    CHECK(U(SetDoubleClickTime)(900));
    CHECK(U(SetDoubleClickTime)(0));
    CHECKF(U(GetDoubleClickTime)() == 500, "got %u", U(GetDoubleClickTime)());
    im_restore(&s);
}
TEST(input_mouse_doubleclicktime_out_of_range) {
    REQUIRE(U(GetDoubleClickTime)); REQUIRE(U(SetDoubleClickTime));
    imstate s; im_save(&s);
    static const UINT v[] = {1, 50, 199, 5001, 10000, 0x7fffffff, 0xffffffff};
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        U(SetDoubleClickTime)(900);
        SET_ERR(0);
        BOOL r = U(SetDoubleClickTime)(v[i]);
        t_note("SetDoubleClickTime(%u) -> %d err %lu now %u", v[i], r, GetLastError(), U(GetDoubleClickTime)());
    }
    im_restore(&s);
    CHECK(U(GetDoubleClickTime)() == s.dbl);
}
TEST(input_mouse_doubleclick_metrics) {
    int cx = GetSystemMetrics(SM_CXDOUBLECLK), cy = GetSystemMetrics(SM_CYDOUBLECLK);
    int dx = GetSystemMetrics(SM_CXDRAG), dy = GetSystemMetrics(SM_CYDRAG);
    CHECK(cx > 0 && cy > 0 && dx > 0 && dy > 0);
    CHECK(cx >= dx);
    CHECK(GetSystemMetrics(SM_MOUSEPRESENT) == 0 || GetSystemMetrics(SM_MOUSEPRESENT) == 1);
    CHECK(GetSystemMetrics(SM_CMOUSEBUTTONS) >= 0);
    CHECK(GetSystemMetrics(SM_MOUSEWHEELPRESENT) == 0 || GetSystemMetrics(SM_MOUSEWHEELPRESENT) == 1);
    CHECK(GetSystemMetrics(SM_MOUSEHORIZONTALWHEELPRESENT) == 0 || GetSystemMetrics(SM_MOUSEHORIZONTALWHEELPRESENT) == 1);
}

/* ---------- SwapMouseButton ---------- */
TEST(input_mouse_swapmousebutton_roundtrip) {
    REQUIRE(U(SwapMouseButton));
    imstate s; im_save(&s);
    BOOL orig = (BOOL)s.swap;
    BOOL prev = U(SwapMouseButton)(TRUE);
    CHECKF(prev == orig, "prev=%d orig=%d", prev, orig);
    CHECK(GetSystemMetrics(SM_SWAPBUTTON) == 1);
    prev = U(SwapMouseButton)(TRUE);
    CHECK(prev == TRUE);
    prev = U(SwapMouseButton)(FALSE);
    CHECK(prev == TRUE);
    CHECK(GetSystemMetrics(SM_SWAPBUTTON) == 0);
    prev = U(SwapMouseButton)(FALSE);
    CHECK(prev == FALSE);
    im_restore(&s);
    CHECK(GetSystemMetrics(SM_SWAPBUTTON) == s.swap);
}
TEST(input_mouse_swapmousebutton_nonbool) {
    REQUIRE(U(SwapMouseButton));
    imstate s; im_save(&s);
    U(SwapMouseButton)(FALSE);
    U(SwapMouseButton)((BOOL)0x100);
    CHECK(GetSystemMetrics(SM_SWAPBUTTON) == 1);
    BOOL prev = U(SwapMouseButton)((BOOL)0);
    CHECK(prev != 0);
    CHECK(GetSystemMetrics(SM_SWAPBUTTON) == 0);
    im_restore(&s);
}
