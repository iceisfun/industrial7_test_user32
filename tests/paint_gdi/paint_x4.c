/* Window paint messages that the other paint_gdi files do not drive directly:
 * WM_SYNCPAINT, WM_DISPLAYCHANGE, WM_SETREDRAW interplay with the update region. */
#include "pg.h"

TEST(paint_gdi_x4_syncpaint_default_proc_survives) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(IsWindow));
    HWND w = pg_topwin(40, 40, 120, 90);
    CHECK(w != NULL); if (!w) return;
    LRESULT r = U(SendMessageA)(w, 0x0088 /* WM_SYNCPAINT */, 0, 0);
    t_note("WM_SYNCPAINT -> %lld", (long long)r);
    CHECK(U(IsWindow)(w));
    pg_flush(w);
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));
    U(DestroyWindow)(w);
}
TEST(paint_gdi_x4_syncpaint_posted_keeps_queue_sane) {
    REQUIRE(U(PostMessageA)); REQUIRE(U(PeekMessageA));
    HWND w = pg_topwin(40, 40, 120, 90);
    CHECK(w != NULL); if (!w) return;
    CHECK(U(PostMessageA)(w, 0x0088, 0, 0));
    pg_pump();
    CHECK(U(IsWindow)(w));
    pg_flush(w);
    U(DestroyWindow)(w);
}
TEST(paint_gdi_x4_displaychange_default_proc) {
    REQUIRE(U(SendMessageA));
    HWND w = pg_topwin(40, 40, 120, 90);
    CHECK(w != NULL); if (!w) return;
    LRESULT r = U(SendMessageA)(w, 0x007E /* WM_DISPLAYCHANGE */, 32, MAKELONG(1024, 768));
    t_note("WM_DISPLAYCHANGE (sent to own window) -> %lld", (long long)r);
    CHECK(U(IsWindow)(w));
    pg_flush(w);
    U(DestroyWindow)(w);
}
TEST(paint_gdi_x4_setredraw_returns_zero_and_keeps_style) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(GetWindowLongA));
    HWND w = pg_topwin(40, 40, 120, 90);
    CHECK(w != NULL); if (!w) return;
    CHECK(U(SendMessageA)(w, WM_SETREDRAW, FALSE, 0) == 0);
    CHECK(U(SendMessageA)(w, WM_SETREDRAW, TRUE, 0) == 0);
    CHECK(U(SendMessageA)(w, WM_SETREDRAW, 7, 0) == 0);        /* any non-zero enables */
    CHECK((U(GetWindowLongA)(w, GWL_STYLE) & WS_VISIBLE) != 0);
    pg_flush(w);
    U(DestroyWindow)(w);
}
TEST(paint_gdi_x4_setredraw_off_invalidate_then_on) {
    REQUIRE(U(SendMessageA)); REQUIRE(U(InvalidateRect)); REQUIRE(U(GetUpdateRect));
    HWND w = pg_topwin(40, 40, 120, 90);
    CHECK(w != NULL); if (!w) return;
    pg_reset();
    pg_r.do_paint = 1;
    U(SendMessageA)(w, WM_SETREDRAW, FALSE, 0);
    CHECK(U(InvalidateRect)(w, NULL, TRUE));
    BOOL inv_off = U(GetUpdateRect)(w, NULL, FALSE);
    pg_pump();
    int paints_off = pg_r.paint;
    t_note("redraw off: update region present=%d, WM_PAINT delivered=%d", inv_off, paints_off);
    U(SendMessageA)(w, WM_SETREDRAW, TRUE, 0);
    CHECK(U(InvalidateRect)(w, NULL, TRUE));
    CHECK(U(GetUpdateRect)(w, NULL, FALSE));
    pg_pump();
    CHECK(pg_r.paint >= 1);
    CHECK(!U(GetUpdateRect)(w, NULL, FALSE));
    pg_r.do_paint = 0;
    U(DestroyWindow)(w);
}
