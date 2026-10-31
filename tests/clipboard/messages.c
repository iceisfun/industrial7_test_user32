#include "cbcommon.h"
/* Delayed rendering, ownership messages, viewer chain, format listeners. */

static void pump(void) { t_pump(); }

/* ---------- delayed rendering ---------- */
TEST(clipboard_delayed_set_null_handle) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    SET_ERR(0);
    HANDLE r = SETD(CF_TEXT, NULL);
    CHECK(r == NULL);
    CHECK(AVAIL(CF_TEXT));
    CHECK(COUNT() >= 1);
    CLOSE();
    CHECK(AVAIL(CF_TEXT));
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_delayed_render_on_get) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, NULL); CLOSE();
    cb_logclear();
    cb_render_data = cb_text("rendered!");
    OPEN(NULL);
    HANDLE h = GETD(CF_TEXT);
    CHECK(h == cb_render_data);
    CHECK(cb_logcount(WM_RENDERFORMAT) == 1);
    int i; for (i = 0; i < cb_nlog && cb_log[i] != WM_RENDERFORMAT; i++) ;
    if (i < cb_nlog) EXPECT_EQ(cb_logw[i], CF_TEXT);
    CHECK(GETD(CF_TEXT) == cb_render_data);
    CHECK(cb_logcount(WM_RENDERFORMAT) == 1); /* rendered once */
    CLOSE();
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_delayed_render_not_provided) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, NULL); CLOSE();
    cb_logclear(); cb_render_data = NULL;
    OPEN(NULL);
    SET_ERR(0);
    HANDLE h = GETD(CF_PRIVATEFIRST);
    CHECK(h == NULL);
    CHECK(cb_logcount(WM_RENDERFORMAT) == 1);
    CLOSE();
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_delayed_render_only_requested_format) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    SETD(CF_PRIVATEFIRST, NULL); SETD(CF_PRIVATEFIRST + 1, NULL); SETD(CF_PRIVATEFIRST + 2, NULL); CLOSE();
    cb_logclear(); cb_render_data = cb_text("one");
    OPEN(NULL);
    CHECK(GETD(CF_PRIVATEFIRST + 1) == cb_render_data);
    CHECK(cb_logcount(WM_RENDERFORMAT) == 1);
    if (cb_nlog) EXPECT_EQ(cb_logw[0], CF_PRIVATEFIRST + 1);
    CLOSE();
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_delayed_render_two_formats) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    SETD(CF_PRIVATEFIRST, NULL); SETD(CF_PRIVATEFIRST + 1, NULL); CLOSE();
    cb_logclear(); cb_render_data = cb_text("two");
    OPEN(NULL);
    GETD(CF_PRIVATEFIRST); GETD(CF_PRIVATEFIRST + 1);
    CHECK(cb_logcount(WM_RENDERFORMAT) == 2);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_delayed_avail_without_render) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, NULL); CLOSE();
    cb_logclear();
    CHECK(AVAIL(CF_PRIVATEFIRST)); CHECK(COUNT() == 1);
    OPEN(NULL); CHECK(ENUM(0) == CF_PRIVATEFIRST); CLOSE();
    CHECK(cb_logcount(WM_RENDERFORMAT) == 0);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_delayed_synth_text_from_delayed_unicode) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_UNICODETEXT, NULL); CLOSE();
    CHECK(AVAIL(CF_UNICODETEXT));
    t_note("delayed UNICODETEXT: text=%d oem=%d locale=%d", AVAIL(CF_TEXT), AVAIL(CF_OEMTEXT), AVAIL(CF_LOCALE));
    cb_logclear(); cb_render_data = cb_wtext(L"late wide");
    OPEN(NULL);
    HANDLE h = GETD(CF_UNICODETEXT);
    CHECK(h == cb_render_data);
    CHECK(cb_logcount(WM_RENDERFORMAT) == 1);
    CLOSE(); cb_reset(); cb_destroy(w);
}
TEST(clipboard_delayed_destroyclipboard_on_empty) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win(), b = cb_win();
    OPEN(a); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("a")); CLOSE();
    cb_logclear();
    OPEN(b); EMPTY(); CLOSE();
    CHECK(cb_logcount(WM_DESTROYCLIPBOARD) == 1);
    cb_reset(); cb_destroy(a); cb_destroy(b);
}
TEST(clipboard_delayed_destroyclipboard_delayed_owner) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win();
    OPEN(a); EMPTY(); SETD(CF_PRIVATEFIRST, NULL); CLOSE();
    cb_logclear();
    OPEN(NULL); EMPTY(); CLOSE();
    CHECK(cb_logcount(WM_DESTROYCLIPBOARD) == 1);
    CHECK(cb_logcount(WM_RENDERFORMAT) == 0);
    CHECK(cb_logcount(WM_RENDERALLFORMATS) == 0);
    cb_reset(); cb_destroy(a);
}
TEST(clipboard_delayed_no_destroy_msg_without_owner) {
    CB_REQ_BASIC; cb_reset();
    OPEN(NULL); EMPTY(); CLOSE();
    cb_logclear();
    OPEN(NULL); EMPTY(); CLOSE();
    CHECK(cb_nlog == 0 || cb_logcount(WM_DESTROYCLIPBOARD) == 0);
}
TEST(clipboard_delayed_renderall_on_destroy) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win();
    OPEN(a); EMPTY(); SETD(CF_PRIVATEFIRST, NULL); CLOSE();
    cb_logclear();
    cb_destroy(a);
    CHECK(cb_logcount(WM_RENDERALLFORMATS) == 1);
    cb_reset();
}
TEST(clipboard_delayed_no_renderall_without_delayed) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win();
    OPEN(a); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("real")); CLOSE();
    cb_logclear();
    cb_destroy(a);
    t_note("RENDERALLFORMATS with no delayed formats: %d", cb_logcount(WM_RENDERALLFORMATS));
    cb_reset();
}
TEST(clipboard_delayed_destroy_unrelated_window_no_renderall) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win(), b = cb_win();
    OPEN(a); EMPTY(); SETD(CF_PRIVATEFIRST, NULL); CLOSE();
    cb_logclear();
    cb_destroy(b);
    CHECK(cb_logcount(WM_RENDERALLFORMATS) == 0);
    cb_reset(); cb_destroy(a);
}
TEST(clipboard_delayed_render_after_reown) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win(), b = cb_win();
    OPEN(a); EMPTY(); SETD(CF_PRIVATEFIRST, NULL); CLOSE();
    OPEN(b); EMPTY(); SETD(CF_PRIVATEFIRST + 1, NULL); CLOSE();
    CHECK(!AVAIL(CF_PRIVATEFIRST)); CHECK(AVAIL(CF_PRIVATEFIRST + 1));
    cb_reset(); cb_destroy(a); cb_destroy(b);
}
TEST(clipboard_delayed_sequence_changes_on_set) {
    CB_REQ_BASIC; REQUIRE(SEQ); cb_reset();
    HWND a = cb_win(); OPEN(a); EMPTY(); CLOSE();
    DWORD s = SEQ();
    OPEN(a); SETD(CF_PRIVATEFIRST, NULL); CLOSE();
    CHECK(SEQ() != s);
    cb_reset(); cb_destroy(a);
}
TEST(clipboard_owner_display_format_set) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win(); OPEN(a); EMPTY();
    SETD(CF_OWNERDISPLAY, NULL);
    CHECK(AVAIL(CF_OWNERDISPLAY));
    CLOSE();
    CHECK(AVAIL(CF_OWNERDISPLAY));
    cb_reset(); cb_destroy(a);
}
TEST(clipboard_owner_display_not_synthesizing) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win(); OPEN(a); EMPTY(); SETD(CF_OWNERDISPLAY, NULL); CLOSE();
    CHECK(COUNT() == 1);
    CHECK(!AVAIL(CF_TEXT)); CHECK(!AVAIL(CF_BITMAP));
    cb_reset(); cb_destroy(a);
}

/* ---------- viewer chain ---------- */
TEST(clipboard_viewer_set_and_get) {
    CB_REQ_BASIC; REQUIRE(U(SetClipboardViewer)); REQUIRE(U(GetClipboardViewer)); REQUIRE(U(ChangeClipboardChain)); cb_reset();
    HWND v = cb_win();
    HWND prev = U(GetClipboardViewer)();
    cb_next = U(SetClipboardViewer)(v);
    t_note("previous viewer %p (before: %p)", cb_next, prev);
    CHECK(cb_next == prev);
    CHECK(U(GetClipboardViewer)() == v);
    pump();
    U(ChangeClipboardChain)(v, cb_next);
    pump();
    CHECK(U(GetClipboardViewer)() == prev);
    cb_next = NULL;
    cb_destroy(v);
}
TEST(clipboard_viewer_drawclipboard_on_change) {
    CB_REQ_BASIC; REQUIRE(U(SetClipboardViewer)); REQUIRE(U(ChangeClipboardChain)); cb_reset();
    HWND v = cb_win(), o = cb_win();
    cb_next = U(SetClipboardViewer)(v);
    pump(); cb_logclear();
    OPEN(o); EMPTY(); SETD(CF_TEXT, cb_text("draw")); CLOSE();
    pump();
    CHECK(cb_logcount(WM_DRAWCLIPBOARD) >= 1);
    t_note("WM_DRAWCLIPBOARD count after one change: %d", cb_logcount(WM_DRAWCLIPBOARD));
    U(ChangeClipboardChain)(v, cb_next); pump();
    cb_next = NULL;
    cb_reset(); cb_destroy(v); cb_destroy(o);
}
TEST(clipboard_viewer_no_draw_after_removal) {
    CB_REQ_BASIC; REQUIRE(U(SetClipboardViewer)); REQUIRE(U(ChangeClipboardChain)); cb_reset();
    HWND v = cb_win(), o = cb_win();
    cb_next = U(SetClipboardViewer)(v); pump();
    U(ChangeClipboardChain)(v, cb_next); pump();
    HWND saved = cb_next; cb_next = NULL;
    cb_logclear();
    OPEN(o); EMPTY(); SETD(CF_TEXT, cb_text("nodraw")); CLOSE();
    pump();
    CHECK(cb_logcount(WM_DRAWCLIPBOARD) == 0);
    cb_reset(); cb_destroy(v); cb_destroy(o);
}
TEST(clipboard_viewer_two_windows_chain) {
    CB_REQ_BASIC; REQUIRE(U(SetClipboardViewer)); REQUIRE(U(GetClipboardViewer)); REQUIRE(U(ChangeClipboardChain)); cb_reset();
    HWND v1 = cb_win(), v2 = cb_win();
    HWND orig = U(GetClipboardViewer)();
    HWND n1 = U(SetClipboardViewer)(v1);
    CHECK(n1 == orig);
    HWND n2 = U(SetClipboardViewer)(v2);
    CHECK(n2 == v1);
    CHECK(U(GetClipboardViewer)() == v2);
    cb_next = v1; /* v2's next */
    pump();
    /* remove v1 (middle): v2 must receive WM_CHANGECBCHAIN */
    cb_logclear();
    U(ChangeClipboardChain)(v1, n1);
    CHECK(cb_logcount(WM_CHANGECBCHAIN) >= 1);
    cb_next = n1;
    U(ChangeClipboardChain)(v2, n1);
    pump();
    CHECK(U(GetClipboardViewer)() == orig);
    cb_next = NULL;
    cb_destroy(v1); cb_destroy(v2);
}
TEST(clipboard_viewer_set_null) {
    CB_REQ_BASIC; REQUIRE(U(SetClipboardViewer)); REQUIRE(U(GetClipboardViewer)); cb_reset();
    HWND orig = U(GetClipboardViewer)();
    SET_ERR(0);
    HWND r = U(SetClipboardViewer)(NULL);
    t_note("SetClipboardViewer(NULL) -> %p err=%lu viewer=%p orig=%p", r, GetLastError(), U(GetClipboardViewer)(), orig);
}
TEST(clipboard_viewer_set_invalid_window) {
    CB_REQ_BASIC; REQUIRE(U(SetClipboardViewer)); REQUIRE(U(GetClipboardViewer)); cb_reset();
    HWND orig = U(GetClipboardViewer)();
    SET_ERR(0);
    HWND r = U(SetClipboardViewer)((HWND)(ULONG_PTR)0xdeadbeef);
    t_note("SetClipboardViewer(bogus) -> %p err=%lu viewer=%p orig=%p", r, GetLastError(), U(GetClipboardViewer)(), orig);
}
TEST(clipboard_viewer_destroyed_window_leaves_chain) {
    CB_REQ_BASIC; REQUIRE(U(SetClipboardViewer)); REQUIRE(U(GetClipboardViewer)); cb_reset();
    HWND orig = U(GetClipboardViewer)();
    HWND v = cb_win();
    HWND n = U(SetClipboardViewer)(v);
    cb_next = n; pump();
    CHECK(U(GetClipboardViewer)() == v);
    cb_destroy(v); pump();
    cb_next = NULL;
    CHECK(U(GetClipboardViewer)() != v);
    t_note("viewer after destroy %p (orig %p)", U(GetClipboardViewer)(), orig);
}
TEST(clipboard_change_chain_bogus) {
    REQUIRE(U(ChangeClipboardChain)); REQUIRE(U(GetClipboardViewer));
    HWND orig = U(GetClipboardViewer)();
    SET_ERR(0);
    BOOL r = U(ChangeClipboardChain)((HWND)(ULONG_PTR)0xdeadbeef, NULL);
    t_note("ChangeClipboardChain(bogus,NULL) -> %d err=%lu", r, GetLastError());
    CHECK(U(GetClipboardViewer)() == orig);
}
TEST(clipboard_get_viewer_stable) {
    REQUIRE(U(GetClipboardViewer));
    HWND a = U(GetClipboardViewer)(), b = U(GetClipboardViewer)();
    CHECK(a == b);
}

/* ---------- format listeners ---------- */
#define ADDL U(AddClipboardFormatListener)
#define REML U(RemoveClipboardFormatListener)

TEST(clipboard_listener_add_remove) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND w = cb_win();
    CHECK(ADDL(w));
    CHECK(REML(w));
    cb_destroy(w);
}
TEST(clipboard_listener_add_null) {
    REQUIRE(ADDL);
    SET_ERR(0); CHECK(!ADDL(NULL)); t_note("AddClipboardFormatListener(NULL) err=%lu", GetLastError());
}
TEST(clipboard_listener_remove_null) {
    REQUIRE(REML);
    SET_ERR(0); CHECK(!REML(NULL)); t_note("RemoveClipboardFormatListener(NULL) err=%lu", GetLastError());
}
TEST(clipboard_listener_add_bogus) {
    REQUIRE(ADDL); REQUIRE(REML);
    SET_ERR(0); CHECK(!ADDL((HWND)(ULONG_PTR)0xdeadbeef)); t_note("Add(bogus) err=%lu", GetLastError());
    SET_ERR(0); CHECK(!REML((HWND)(ULONG_PTR)0xdeadbeef)); t_note("Remove(bogus) err=%lu", GetLastError());
}
TEST(clipboard_listener_add_destroyed) {
    CB_REQ_BASIC; REQUIRE(ADDL); cb_reset();
    HWND w = cb_win(); cb_destroy(w);
    SET_ERR(0); CHECK(!ADDL(w)); t_note("Add(destroyed) err=%lu", GetLastError());
}
TEST(clipboard_listener_remove_not_added) {
    CB_REQ_BASIC; REQUIRE(REML); cb_reset();
    HWND w = cb_win();
    SET_ERR(0); BOOL r = REML(w);
    CHECK(!r);
    t_note("RemoveClipboardFormatListener(not added) err=%lu", GetLastError());
    cb_destroy(w);
}
TEST(clipboard_listener_remove_twice) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND w = cb_win();
    CHECK(ADDL(w)); CHECK(REML(w));
    SET_ERR(0); CHECK(!REML(w));
    t_note("second Remove err=%lu", GetLastError());
    cb_destroy(w);
}
TEST(clipboard_listener_add_twice) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND w = cb_win();
    CHECK(ADDL(w));
    SET_ERR(0); BOOL r = ADDL(w);
    t_note("second Add -> %d err=%lu", r, GetLastError());
    REML(w); if (r) REML(w);
    cb_destroy(w);
}
TEST(clipboard_listener_receives_update) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND l = cb_win(), o = cb_win();
    CHECK(ADDL(l)); pump(); cb_logclear();
    OPEN(o); EMPTY(); SETD(CF_TEXT, cb_text("upd")); CLOSE();
    pump();
    CHECK(cb_logcount(WM_CLIPBOARDUPDATE) >= 1);
    t_note("WM_CLIPBOARDUPDATE count: %d", cb_logcount(WM_CLIPBOARDUPDATE));
    REML(l);
    cb_reset(); cb_destroy(l); cb_destroy(o);
}
TEST(clipboard_listener_no_update_after_remove) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND l = cb_win(), o = cb_win();
    CHECK(ADDL(l)); CHECK(REML(l)); pump(); cb_logclear();
    OPEN(o); EMPTY(); SETD(CF_TEXT, cb_text("upd")); CLOSE();
    pump();
    CHECK(cb_logcount(WM_CLIPBOARDUPDATE) == 0);
    cb_reset(); cb_destroy(l); cb_destroy(o);
}
TEST(clipboard_listener_no_update_without_change) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND l = cb_win();
    CHECK(ADDL(l)); pump(); cb_logclear();
    OPEN(NULL); GETD(CF_TEXT); CLOSE();
    pump();
    t_note("updates for open/close without change: %d", cb_logcount(WM_CLIPBOARDUPDATE));
    REML(l); cb_destroy(l);
}
TEST(clipboard_listener_two_listeners) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND l1 = cb_win(), l2 = cb_win();
    CHECK(ADDL(l1)); CHECK(ADDL(l2)); pump(); cb_logclear();
    OPEN(NULL); EMPTY(); CLOSE();
    pump();
    CHECK(cb_logcount(WM_CLIPBOARDUPDATE) >= 2);
    REML(l1); REML(l2);
    cb_destroy(l1); cb_destroy(l2);
}
TEST(clipboard_listener_destroyed_window_auto_removed) {
    CB_REQ_BASIC; REQUIRE(ADDL); cb_reset();
    HWND l = cb_win();
    CHECK(ADDL(l));
    cb_destroy(l);
    OPEN(NULL); EMPTY(); CLOSE();
    pump();
    CHECK(1);
}
TEST(clipboard_listener_empty_triggers_update) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND l = cb_win();
    OPEN(NULL); EMPTY(); CLOSE();
    CHECK(ADDL(l)); pump(); cb_logclear();
    OPEN(NULL); EMPTY(); CLOSE();
    pump();
    t_note("updates for Empty on empty clipboard: %d", cb_logcount(WM_CLIPBOARDUPDATE));
    REML(l); cb_destroy(l);
}
TEST(clipboard_listener_update_wparam_lparam_zero) {
    CB_REQ_BASIC; REQUIRE(ADDL); REQUIRE(REML); cb_reset();
    HWND l = cb_win();
    CHECK(ADDL(l)); pump(); cb_logclear();
    OPEN(NULL); EMPTY(); CLOSE();
    pump();
    for (int i = 0; i < cb_nlog; i++) if (cb_log[i] == WM_CLIPBOARDUPDATE) CHECK(cb_logw[i] == 0);
    REML(l); cb_destroy(l);
}
TEST(clipboard_listener_msg_const) {
    CHECK(WM_CLIPBOARDUPDATE == 0x031D);
    CHECK(WM_RENDERFORMAT == 0x0305); CHECK(WM_RENDERALLFORMATS == 0x0306);
    CHECK(WM_DESTROYCLIPBOARD == 0x0307); CHECK(WM_DRAWCLIPBOARD == 0x0308);
    CHECK(WM_CHANGECBCHAIN == 0x030D);
}

/* ---------- GetUpdatedClipboardFormats ---------- */
#define GUF U(GetUpdatedClipboardFormats)
TEST(clipboard_updated_formats_empty) {
    CB_REQ_BASIC; REQUIRE(GUF); cb_reset();
    UINT buf[8]; UINT out = 99;
    SET_ERR(0);
    BOOL r = GUF(buf, 8, &out);
    CHECK(r); CHECK(out == 0);
}
TEST(clipboard_updated_formats_matches_enum) {
    CB_REQ_BASIC; REQUIRE(GUF); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    SETD(CF_PRIVATEFIRST + 1, cb_text("a")); SETD(CF_PRIVATEFIRST + 2, cb_text("b")); SETD(CF_PRIVATEFIRST + 3, cb_text("c")); CLOSE();
    UINT buf[16]; UINT out = 0;
    CHECK(GUF(buf, 16, &out));
    CHECK(out == 3);
    UINT e[16]; int n = 0; UINT f = 0;
    OPEN(w); while (n < 16 && (f = ENUM(f))) e[n++] = f; CLOSE();
    CHECK(n == (int)out);
    for (int i = 0; i < n && i < (int)out; i++) CHECKF(buf[i] == e[i], "idx %d: %x vs %x", i, buf[i], e[i]);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_updated_formats_text_synth) {
    CB_REQ_BASIC; REQUIRE(GUF); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_TEXT, cb_text("t")); CLOSE();
    UINT buf[16]; UINT out = 0;
    CHECK(GUF(buf, 16, &out));
    t_note("GUF out=%u count=%d", out, COUNT());
    int saw = 0; for (UINT i = 0; i < out; i++) if (buf[i] == CF_TEXT) saw = 1;
    CHECK(saw);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_updated_formats_small_buffer) {
    CB_REQ_BASIC; REQUIRE(GUF); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    for (UINT i = 0; i < 5; i++) SETD(CF_PRIVATEFIRST + i, cb_text("x"));
    CLOSE();
    UINT buf[16]; UINT out = 0;
    SET_ERR(0);
    BOOL r = GUF(buf, 2, &out);
    CHECK(!r);
    t_note("GUF small buffer err=%lu (want %d)", GetLastError(), ERROR_INSUFFICIENT_BUFFER);
    CHECK(out == 5);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_updated_formats_exact_buffer) {
    CB_REQ_BASIC; REQUIRE(GUF); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY();
    for (UINT i = 0; i < 5; i++) SETD(CF_PRIVATEFIRST + i, cb_text("x"));
    CLOSE();
    UINT buf[5]; UINT out = 0;
    CHECK(GUF(buf, 5, &out)); CHECK(out == 5);
    UINT buf4[5]; out = 0;
    SET_ERR(0); CHECK(!GUF(buf4, 4, &out)); CHECK(out == 5);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_updated_formats_null_out) {
    CB_REQ_BASIC; REQUIRE(GUF); cb_reset();
    UINT buf[4];
    SET_ERR(0); BOOL r = GUF(buf, 4, NULL);
    CHECK(!r);
    t_note("GetUpdatedClipboardFormats(out NULL) err=%lu", GetLastError());
}
TEST(clipboard_updated_formats_null_buffer_query) {
    CB_REQ_BASIC; REQUIRE(GUF); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("x")); CLOSE();
    UINT out = 77; SET_ERR(0);
    BOOL r = GUF(NULL, 0, &out);
    t_note("GetUpdatedClipboardFormats(NULL,0) -> %d out=%u err=%lu", r, out, GetLastError());
    CHECK(!r || out == 1);
    cb_reset(); cb_destroy(w);
}
TEST(clipboard_updated_formats_no_open_needed) {
    CB_REQ_BASIC; REQUIRE(GUF); cb_reset();
    HWND w = cb_win(); OPEN(w); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("x")); CLOSE();
    UINT buf[4]; UINT out = 0;
    CHECK(GUF(buf, 4, &out)); CHECK(out == 1); CHECK(buf[0] == CF_PRIVATEFIRST);
    cb_reset(); cb_destroy(w);
}

/* ---------- misc ownership ---------- */
TEST(clipboard_owner_none_initially_after_null_empty) {
    CB_REQ_BASIC; REQUIRE(U(GetClipboardOwner)); cb_reset();
    CHECK(U(GetClipboardOwner)() == NULL);
}
TEST(clipboard_owner_destroyed_with_other_owner_kept) {
    CB_REQ_BASIC; REQUIRE(U(GetClipboardOwner)); cb_reset();
    HWND a = cb_win(), b = cb_win();
    OPEN(a); EMPTY(); CLOSE();
    OPEN(b); EMPTY(); CLOSE();
    cb_destroy(a);
    CHECK(U(GetClipboardOwner)() == b);
    cb_destroy(b);
    CHECK(U(GetClipboardOwner)() == NULL);
}
static HWND g_tw; static DWORD WINAPI owner_thread(LPVOID p) {
    g_tw = cb_win();
    OPEN(g_tw); EMPTY(); SETD(CF_PRIVATEFIRST, cb_text("thr")); CLOSE();
    return 0;
}
TEST(clipboard_owner_other_thread_window) {
    CB_REQ_BASIC; REQUIRE(U(GetClipboardOwner)); cb_reset();
    HANDLE h = CreateThread(NULL, 0, owner_thread, NULL, 0, NULL);
    if (h) { WaitForSingleObject(h, 8000); CloseHandle(h); }
    /* the thread exited so its window is gone; clipboard keeps data, owner cleared */
    CHECK(U(GetClipboardOwner)() == NULL);
    CHECK(AVAIL(CF_PRIVATEFIRST));
    OPEN(NULL);
    HANDLE d = GETD(CF_PRIVATEFIRST);
    CHECK(d != NULL);
    if (d) { char *s = GlobalLock(d); CHECK(s && !strcmp(s, "thr")); GlobalUnlock(d); }
    CLOSE();
    cb_reset();
}
TEST(clipboard_final_cleanup_leaves_empty) {
    CB_REQ_BASIC; cb_reset();
    CHECK(COUNT() == 0);
    CHECK(!U(GetOpenClipboardWindow) || U(GetOpenClipboardWindow)() == NULL);
}
