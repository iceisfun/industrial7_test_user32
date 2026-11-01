#include "cbcommon.h"
/* Open/Close/Empty state machine and error codes. */

TEST(clipboard_open_close_basic) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); CHECK(w);
    CHECK(OPEN(w));
    CHECK(CLOSE());
    cb_destroy(w);
}
TEST(clipboard_close_not_open) {
    CB_REQ_BASIC; cb_reset();
    SET_ERR(0); CHECK(!CLOSE()); EXPECT_ERR(0, ERR_NOT_OPEN);
    SET_ERR(0); CHECK(!CLOSE()); EXPECT_ERR(0, ERR_NOT_OPEN);
}
TEST(clipboard_close_twice) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    CHECK(OPEN(w)); CHECK(CLOSE());
    SET_ERR(0); CHECK(!CLOSE()); EXPECT_ERR(0, ERR_NOT_OPEN);
    cb_destroy(w);
}
TEST(clipboard_empty_not_open) {
    CB_REQ_BASIC; cb_reset();
    SET_ERR(0); CHECK(!EMPTY()); EXPECT_ERR(0, ERR_NOT_OPEN);
}
TEST(clipboard_empty_after_close) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    CHECK(OPEN(w)); CHECK(EMPTY()); CHECK(CLOSE());
    SET_ERR(0); CHECK(!EMPTY()); EXPECT_ERR(0, ERR_NOT_OPEN);
    cb_destroy(w);
}
TEST(clipboard_set_not_open) {
    CB_REQ_BASIC; cb_reset();
    HGLOBAL g = cb_text("x");
    SET_ERR(0); CHECK(SETD(CF_TEXT, g) == NULL); EXPECT_ERR(0, ERR_NOT_OPEN);
    GlobalFree(g);
}
TEST(clipboard_get_not_open) {
    CB_REQ_BASIC; cb_reset();
    SET_ERR(0); CHECK(GETD(CF_TEXT) == NULL); EXPECT_ERR(0, ERR_NOT_OPEN);
    SET_ERR(0); CHECK(GETD(CF_UNICODETEXT) == NULL); EXPECT_ERR(0, ERR_NOT_OPEN);
    SET_ERR(0); CHECK(GETD(0) == NULL); EXPECT_ERR(0, ERR_NOT_OPEN);
}
TEST(clipboard_open_null_window) {
    CB_REQ_BASIC; cb_reset();
    CHECK(OPEN(NULL));
    CHECK(U(GetOpenClipboardWindow) ? U(GetOpenClipboardWindow)() == NULL : 1);
    CHECK(EMPTY());
    CHECK(CLOSE());
}
TEST(clipboard_open_null_empty_owner_null) {
    CB_REQ_BASIC; REQUIRE(U(GetClipboardOwner)); cb_reset();
    HWND w = cb_win();
    CHECK(OPEN(w)); CHECK(EMPTY()); CHECK(CLOSE());
    CHECK(U(GetClipboardOwner)() == w);
    CHECK(OPEN(NULL)); CHECK(EMPTY()); CHECK(CLOSE());
    CHECK(U(GetClipboardOwner)() == NULL);
    cb_destroy(w);
}
TEST(clipboard_open_null_set_fails) {
    /* docs say a NULL owner makes SetClipboardData fail after EmptyClipboard;
     * the Win10 22H2 oracle accepts it (non-NULL handle, error 0), so the
     * outcome is only recorded. */
    CB_REQ_BASIC; cb_reset();
    CHECK(OPEN(NULL)); CHECK(EMPTY());
    HGLOBAL g = cb_text("abc");
    SET_ERR(0);
    HANDLE r = SETD(CF_TEXT, g);
    t_note("SetClipboardData with NULL owner -> %p err=%lu", r, GetLastError());
    if (!r) GlobalFree(g);
    CHECK(CLOSE());
    cb_reset();
}
TEST(clipboard_open_bogus_window) {
    CB_REQ_BASIC; cb_reset();
    SET_ERR(0); CHECK(!OPEN((HWND)(ULONG_PTR)0xdeadbeef)); EXPECT_ERR(0, ERR_INVALID_WINDOW);
    SET_ERR(0); CHECK(!OPEN((HWND)(ULONG_PTR)1)); EXPECT_ERR(0, ERR_INVALID_WINDOW);
    SET_ERR(0); CHECK(!OPEN((HWND)(ULONG_PTR)-1)); EXPECT_ERR(0, ERR_INVALID_WINDOW);
    SET_ERR(0); CHECK(!OPEN((HWND)(ULONG_PTR)0x123456789ull)); EXPECT_ERR(0, ERR_INVALID_WINDOW);
    SET_ERR(0); CHECK(!CLOSE()); EXPECT_ERR(0, ERR_NOT_OPEN);
}
TEST(clipboard_open_destroyed_window) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win(); cb_destroy(w);
    SET_ERR(0); CHECK(!OPEN(w)); EXPECT_ERR(0, ERR_INVALID_WINDOW);
}
TEST(clipboard_open_same_window_twice) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    CHECK(OPEN(w));
    CHECK(OPEN(w));      /* same window: allowed, no nesting count */
    CHECK(CLOSE());
    SET_ERR(0); CHECK(!CLOSE()); EXPECT_ERR(0, ERR_NOT_OPEN);
    cb_destroy(w);
}
TEST(clipboard_open_other_window_same_thread) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win(), b = cb_win();
    CHECK(OPEN(a));
    SET_ERR(0); BOOL r = OPEN(b);
    CHECK(!r);
    if (!r) EXPECT_ERR(0, ERR_ACCESS_DENIED);
    if (r) CLOSE();
    CHECK(CLOSE());
    CHECK(OPEN(b)); CHECK(CLOSE());
    cb_destroy(a); cb_destroy(b);
}
TEST(clipboard_open_null_then_window) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win();
    CHECK(OPEN(NULL));
    SET_ERR(0); BOOL r = OPEN(a);
    CHECK(!r);
    if (!r) EXPECT_ERR(0, ERR_ACCESS_DENIED);
    if (r) CLOSE();
    CHECK(CLOSE());
    cb_destroy(a);
}
TEST(clipboard_open_window_then_null) {
    CB_REQ_BASIC; cb_reset();
    HWND a = cb_win();
    CHECK(OPEN(a));
    SET_ERR(0); BOOL r = OPEN(NULL);
    CHECK(!r);
    if (!r) EXPECT_ERR(0, ERR_ACCESS_DENIED);
    if (r) CLOSE();
    CHECK(CLOSE());
    cb_destroy(a);
}
TEST(clipboard_getopenwindow) {
    CB_REQ_BASIC; REQUIRE(U(GetOpenClipboardWindow)); cb_reset();
    HWND w = cb_win();
    CHECK(U(GetOpenClipboardWindow)() == NULL);
    CHECK(OPEN(w));
    CHECK(U(GetOpenClipboardWindow)() == w);
    CHECK(CLOSE());
    CHECK(U(GetOpenClipboardWindow)() == NULL);
    cb_destroy(w);
}
TEST(clipboard_destroy_window_while_open) {
    CB_REQ_BASIC; REQUIRE(U(GetOpenClipboardWindow)); cb_reset();
    HWND w = cb_win();
    CHECK(OPEN(w));
    cb_destroy(w);
    CHECK(U(GetOpenClipboardWindow)() == NULL);
    SET_ERR(0); CHECK(!CLOSE()); EXPECT_ERR(0, ERR_NOT_OPEN);
    /* clipboard must be usable again */
    CHECK(OPEN(NULL)); CHECK(CLOSE());
}
TEST(clipboard_empty_sets_owner) {
    CB_REQ_BASIC; REQUIRE(U(GetClipboardOwner)); cb_reset();
    HWND a = cb_win(), b = cb_win();
    CHECK(OPEN(a)); CHECK(EMPTY()); CHECK(CLOSE());
    CHECK(U(GetClipboardOwner)() == a);
    CHECK(OPEN(b)); CHECK(U(GetClipboardOwner)() == a); CHECK(EMPTY());
    CHECK(U(GetClipboardOwner)() == b); CHECK(CLOSE());
    CHECK(U(GetClipboardOwner)() == b);
    cb_destroy(a); cb_destroy(b);
}
TEST(clipboard_owner_cleared_on_destroy) {
    CB_REQ_BASIC; REQUIRE(U(GetClipboardOwner)); cb_reset();
    HWND a = cb_win();
    CHECK(OPEN(a)); CHECK(EMPTY()); CHECK(CLOSE());
    CHECK(U(GetClipboardOwner)() == a);
    cb_destroy(a);
    CHECK(U(GetClipboardOwner)() == NULL);
}
TEST(clipboard_owner_without_empty_unchanged) {
    CB_REQ_BASIC; REQUIRE(U(GetClipboardOwner)); cb_reset();
    HWND a = cb_win(), b = cb_win();
    CHECK(OPEN(a)); CHECK(EMPTY()); CHECK(CLOSE());
    CHECK(OPEN(b)); CHECK(CLOSE());
    CHECK(U(GetClipboardOwner)() == a);
    cb_destroy(a); cb_destroy(b);
}
TEST(clipboard_empty_idempotent) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    CHECK(OPEN(w));
    for (int i = 0; i < 20; i++) CHECK(EMPTY());
    CHECK(COUNT() == 0);
    CHECK(CLOSE());
    cb_destroy(w);
}
TEST(clipboard_open_close_loop) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    int ok = 0;
    for (int i = 0; i < 500; i++) { for (int t = 0; t < 50; t++) { if (OPEN(w)) { if (CLOSE()) ok++; break; } Sleep(2); } }
    EXPECT_EQ(ok, 500);
    cb_destroy(w);
}
TEST(clipboard_open_close_loop_null) {
    CB_REQ_BASIC; cb_reset();
    int ok = 0;
    for (int i = 0; i < 500; i++) { for (int t = 0; t < 50; t++) { if (OPEN(NULL)) { if (CLOSE()) ok++; break; } Sleep(2); } }
    EXPECT_EQ(ok, 500);
}
TEST(clipboard_lasterror_preserved_on_success) {
    CB_REQ_BASIC; cb_reset();
    HWND w = cb_win();
    SET_ERR(0x1234);
    CHECK(OPEN(w));
    t_note("GetLastError after successful OpenClipboard: %lu", GetLastError());
    CHECK(CLOSE());
    cb_destroy(w);
}

/* cross-thread contention */
static HWND g_xw; static volatile LONG g_state; static BOOL g_r1, g_r2; static DWORD g_e1;
static HWND g_ow;
static DWORD WINAPI xthread(LPVOID p) {
    HWND w = cb_win();
    SetLastError(0);
    g_r1 = OPEN(w); g_e1 = GetLastError();
    if (g_r1) CLOSE();
    SetLastError(0);
    g_r2 = OPEN(NULL); if (g_r2) CLOSE();
    g_ow = U(GetOpenClipboardWindow) ? U(GetOpenClipboardWindow)() : NULL;
    cb_destroy(w);
    return 0;
}
TEST(clipboard_open_cross_thread_contention) {
    CB_REQ_BASIC; REQUIRE(U(GetOpenClipboardWindow)); cb_reset();
    HWND w = cb_win();
    CHECK(OPEN(w));
    HANDLE h = CreateThread(NULL, 0, xthread, NULL, 0, NULL);
    CHECK(h);
    if (h) { CHECK(WaitForSingleObject(h, 8000) == WAIT_OBJECT_0); CloseHandle(h); }
    CHECK(g_r1 == FALSE); EXPECT_EQ(g_e1, ERR_ACCESS_DENIED);
    CHECK(g_r2 == FALSE);
    CHECK(g_ow == w);
    CHECK(CLOSE());
    cb_destroy(w);
}
static DWORD WINAPI xthread_open(LPVOID p) {
    HWND w = cb_win();
    BOOL r = OPEN(w);
    g_state = 1;
    DWORD t = GetTickCount();
    while (g_state != 2 && GetTickCount() - t < 5000) Sleep(5);
    if (r) CLOSE();
    cb_destroy(w);
    return r;
}
TEST(clipboard_other_thread_holds_open) {
    CB_REQ_BASIC; cb_reset();
    g_state = 0;
    HANDLE h = CreateThread(NULL, 0, xthread_open, NULL, 0, NULL);
    DWORD t = GetTickCount();
    while (g_state != 1 && GetTickCount() - t < 5000) Sleep(5);
    EXPECT_EQ(g_state, 1);
    HWND w = cb_win();
    SET_ERR(0); CHECK(!OPEN(w)); EXPECT_ERR(0, ERR_ACCESS_DENIED);
    SET_ERR(0); CHECK(!OPEN(NULL)); EXPECT_ERR(0, ERR_ACCESS_DENIED);
    SET_ERR(0); CHECK(!CLOSE()); EXPECT_ERR(0, ERR_NOT_OPEN);
    SET_ERR(0); CHECK(!EMPTY()); EXPECT_ERR(0, ERR_NOT_OPEN);
    SET_ERR(0); CHECK(GETD(CF_TEXT) == NULL); EXPECT_ERR(0, ERR_NOT_OPEN);
    /* query functions that do not need the clipboard open still work */
    CHECK(COUNT() >= 0);
    g_state = 2;
    if (h) { WaitForSingleObject(h, 8000); CloseHandle(h); }
    CHECK(OPEN(w)); CHECK(CLOSE());
    cb_destroy(w);
}
static DWORD WINAPI xthread_exit_open(LPVOID p) {
    OPEN(NULL);  /* thread exits without closing */
    return 0;
}
TEST(clipboard_open_released_on_thread_exit) {
    CB_REQ_BASIC; cb_reset();
    HANDLE h = CreateThread(NULL, 0, xthread_exit_open, NULL, 0, NULL);
    if (h) { WaitForSingleObject(h, 8000); CloseHandle(h); }
    int ok = 0;
    for (int i = 0; i < 100 && !ok; i++) { if (OPEN(NULL)) { ok = 1; CLOSE(); } else Sleep(20); }
    CHECK(ok);
}
