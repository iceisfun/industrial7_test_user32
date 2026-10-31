/* gap_05: TileChildWindows, SetWindowsHookExAW, SetWindowRgnEx, SetSystemMenu,
 * SetInternalWindowPos, SetWindowBand, SetWindowCompositionAttribute. */
#include "g5.h"

UDECL(WORD, TileChildWindows, (HWND, UINT));
UDECL(HHOOK, SetWindowsHookExAW, (int, HOOKPROC, HINSTANCE, DWORD, DWORD));
UDECL(int, SetWindowRgnEx, (HWND, HRGN, BOOL));
UDECL(BOOL, SetSystemMenu, (HWND, HMENU));
UDECL(BOOL, SetInternalWindowPos, (HWND, UINT, RECT *, POINT *));
UDECL(BOOL, SetWindowBand, (HWND, HWND, DWORD));
UDECL(BOOL, GetWindowBand, (HWND, DWORD *));
typedef struct { DWORD attr; PVOID data; SIZE_T size; } G5_WCAD;
typedef struct { DWORD state, flags, color, anim; } G5_ACCENT;
UDECL(BOOL, SetWindowCompositionAttribute, (HWND, G5_WCAD *));
UDECL(BOOL, GetWindowCompositionAttribute, (HWND, G5_WCAD *));

/* ---------------- TileChildWindows ---------------- */
static HWND tile_parent(void) {
    HWND p = t_make_window(NULL, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 0, 30, 30, 500, 400, NULL);
    if (p) U(ShowWindow)(p, SW_SHOWNOACTIVATE);
    return p;
}
static HWND tile_kid(HWND p, DWORD extra) {
    return t_make_window(NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | extra, 0, 1, 1, 50, 40, p);
}
static int rects_overlap(const RECT *a, const RECT *b) {
    return a->left < b->right && b->left < a->right && a->top < b->bottom && b->top < a->bottom;
}

TEST(gap5_tile_bogus_parent_returns_zero) {
    REQUIRE(UN(TileChildWindows));
    HWND hs[] = { BOGUS_HWND, (HWND)(LONG_PTR)-1, (HWND)(ULONG_PTR)1, (HWND)(ULONG_PTR)0xdeadbeef };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0);
        WORD r = UN(TileChildWindows)(hs[i], 0);
        CHECKF(r == 0, "idx %u ret %u", i, r);
        t_note("TileChildWindows bogus idx %u err %lu", i, GetLastError());
    }
}
TEST(gap5_tile_destroyed_parent) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND k = tile_kid(p, 0); (void)k;
    g5_destroy(p);
    EXPECT_EQ(UN(TileChildWindows)(p, 0), 0);
}
TEST(gap5_tile_no_children) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    static const UINT f[] = { 0, 1, 2, 3, 4, 5, 7 };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        WORD r = UN(TileChildWindows)(p, f[i]);
        CHECKF(r == 0, "flags %u ret %u", f[i], r);
    }
    g5_destroy(p);
}
TEST(gap5_tile_counts_visible_children) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND k[6] = {0};
    for (int n = 1; n <= 6; n++) {
        k[n - 1] = tile_kid(p, 0);
        WORD r = UN(TileChildWindows)(p, 0);
        CHECKF(r == n, "n=%d ret %u", n, r);
    }
    for (int i = 0; i < 6; i++) g5_destroy(k[i]);
    g5_destroy(p);
}
TEST(gap5_tile_horizontal_and_vertical_counts) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND k[4];
    for (int i = 0; i < 4; i++) k[i] = tile_kid(p, 0);
    EXPECT_EQ(UN(TileChildWindows)(p, MDITILE_VERTICAL), 4);
    EXPECT_EQ(UN(TileChildWindows)(p, MDITILE_HORIZONTAL), 4);
    EXPECT_EQ(UN(TileChildWindows)(p, MDITILE_VERTICAL | MDITILE_ZORDER), 4);
    EXPECT_EQ(UN(TileChildWindows)(p, MDITILE_HORIZONTAL | MDITILE_ZORDER), 4);
    for (int i = 0; i < 4; i++) g5_destroy(k[i]);
    g5_destroy(p);
}
TEST(gap5_tile_hidden_child_excluded) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND a = tile_kid(p, 0), b = tile_kid(p, 0), c = tile_kid(p, 0);
    U(ShowWindow)(b, SW_HIDE);
    EXPECT_EQ(UN(TileChildWindows)(p, 0), 2);
    U(ShowWindow)(b, SW_SHOWNOACTIVATE);
    EXPECT_EQ(UN(TileChildWindows)(p, 0), 3);
    g5_destroy(a); g5_destroy(b); g5_destroy(c); g5_destroy(p);
}
TEST(gap5_tile_disabled_child) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND a = tile_kid(p, 0), b = tile_kid(p, WS_DISABLED);
    WORD r0 = UN(TileChildWindows)(p, 0);
    WORD r2 = UN(TileChildWindows)(p, MDITILE_SKIPDISABLED);
    t_note("disabled child: flags0 -> %u, SKIPDISABLED -> %u", r0, r2);
    CHECK(r2 <= r0);
    g5_destroy(a); g5_destroy(b); g5_destroy(p);
}
TEST(gap5_tile_does_not_count_grandchildren) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND a = tile_kid(p, 0), b = tile_kid(p, 0);
    HWND g = t_make_window(NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 10, 10, a);
    EXPECT_EQ(UN(TileChildWindows)(p, 0), 2);
    g5_destroy(g); g5_destroy(a); g5_destroy(b); g5_destroy(p);
}
TEST(gap5_tile_children_stay_inside_and_disjoint) {
    REQUIRE(UN(TileChildWindows));
    for (int n = 2; n <= 7; n++) {
        for (UINT fl = 0; fl <= 1; fl++) {
            HWND p = tile_parent(); CHECK(p != NULL);
            HWND k[7];
            for (int i = 0; i < n; i++) k[i] = tile_kid(p, 0);
            WORD r = UN(TileChildWindows)(p, fl);
            CHECKF(r == n, "n=%d fl=%u ret %u", n, fl, r);
            RECT cr; U(GetClientRect)(p, &cr);
            RECT rc[7];
            for (int i = 0; i < n; i++) {
                U(GetWindowRect)(k[i], &rc[i]);
                POINT pt0 = { rc[i].left, rc[i].top }, pt1 = { rc[i].right, rc[i].bottom };
                U(ScreenToClient)(p, &pt0); U(ScreenToClient)(p, &pt1);
                rc[i].left = pt0.x; rc[i].top = pt0.y; rc[i].right = pt1.x; rc[i].bottom = pt1.y;
                CHECKF(rc[i].left >= cr.left && rc[i].top >= cr.top && rc[i].right <= cr.right && rc[i].bottom <= cr.bottom,
                       "n=%d fl=%u kid %d [%ld,%ld,%ld,%ld] outside client [%ld,%ld,%ld,%ld]", n, fl, i,
                       rc[i].left, rc[i].top, rc[i].right, rc[i].bottom, cr.left, cr.top, cr.right, cr.bottom);
                CHECKF(rc[i].right > rc[i].left && rc[i].bottom > rc[i].top, "n=%d kid %d empty", n, i);
            }
            for (int i = 0; i < n; i++)
                for (int j = i + 1; j < n; j++)
                    CHECKF(!rects_overlap(&rc[i], &rc[j]), "n=%d fl=%u kids %d/%d overlap", n, fl, i, j);
            for (int i = 0; i < n; i++) g5_destroy(k[i]);
            g5_destroy(p);
        }
    }
}
TEST(gap5_tile_single_child_fills_area) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND a = tile_kid(p, 0);
    EXPECT_EQ(UN(TileChildWindows)(p, 0), 1);
    RECT cr, wr; U(GetClientRect)(p, &cr); U(GetWindowRect)(a, &wr);
    t_note("single tiled child %ldx%ld in client %ldx%ld", wr.right - wr.left, wr.bottom - wr.top, cr.right, cr.bottom);
    CHECK(wr.right - wr.left > 50 || wr.bottom - wr.top > 40);
    g5_destroy(a); g5_destroy(p);
}
TEST(gap5_tile_two_children_split_area) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND a = tile_kid(p, 0), b = tile_kid(p, 0);
    EXPECT_EQ(UN(TileChildWindows)(p, MDITILE_VERTICAL), 2);
    RECT ra, rb; U(GetWindowRect)(a, &ra); U(GetWindowRect)(b, &rb);
    t_note("vertical tile: a=(%ld,%ld,%ld,%ld) b=(%ld,%ld,%ld,%ld)", ra.left, ra.top, ra.right, ra.bottom, rb.left, rb.top, rb.right, rb.bottom);
    CHECK(!rects_overlap(&ra, &rb));
    EXPECT_EQ(UN(TileChildWindows)(p, MDITILE_HORIZONTAL), 2);
    U(GetWindowRect)(a, &ra); U(GetWindowRect)(b, &rb);
    CHECK(!rects_overlap(&ra, &rb));
    g5_destroy(a); g5_destroy(b); g5_destroy(p);
}
TEST(gap5_tile_matches_tilewindows) {
    REQUIRE(UN(TileChildWindows)); REQUIRE(U(TileWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND k[3];
    for (int i = 0; i < 3; i++) k[i] = tile_kid(p, 0);
    WORD a = UN(TileChildWindows)(p, MDITILE_VERTICAL);
    RECT r1[3], r2[3];
    for (int i = 0; i < 3; i++) U(GetWindowRect)(k[i], &r1[i]);
    WORD b = U(TileWindows)(p, MDITILE_VERTICAL, NULL, 0, NULL);
    for (int i = 0; i < 3; i++) U(GetWindowRect)(k[i], &r2[i]);
    EXPECT_EQ(a, 3); EXPECT_EQ(b, 3);
    for (int i = 0; i < 3; i++) CHECKF(!memcmp(&r1[i], &r2[i], sizeof(RECT)), "kid %d", i);
    for (int i = 0; i < 3; i++) g5_destroy(k[i]);
    g5_destroy(p);
}
TEST(gap5_tile_idempotent) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND k[4];
    for (int i = 0; i < 4; i++) k[i] = tile_kid(p, 0);
    UN(TileChildWindows)(p, 0);
    RECT r1[4], r2[4];
    for (int i = 0; i < 4; i++) U(GetWindowRect)(k[i], &r1[i]);
    for (int j = 0; j < 20; j++) EXPECT_EQ(UN(TileChildWindows)(p, 0), 4);
    for (int i = 0; i < 4; i++) { U(GetWindowRect)(k[i], &r2[i]); CHECKF(!memcmp(&r1[i], &r2[i], sizeof(RECT)), "kid %d", i); }
    for (int i = 0; i < 4; i++) g5_destroy(k[i]);
    g5_destroy(p);
}
TEST(gap5_tile_garbage_flags) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND a = tile_kid(p, 0), b = tile_kid(p, 0);
    static const UINT f[] = { 0x8, 0x10, 0x100, 0xffff, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        WORD r = UN(TileChildWindows)(p, f[i]);
        t_note("flags 0x%x -> %u", f[i], r);
    }
    g5_destroy(a); g5_destroy(b); g5_destroy(p);
}
TEST(gap5_tile_maximized_child_restored) {
    REQUIRE(UN(TileChildWindows));
    HWND p = tile_parent(); CHECK(p != NULL);
    HWND a = tile_kid(p, WS_MAXIMIZEBOX | WS_CAPTION), b = tile_kid(p, 0);
    U(ShowWindow)(a, SW_MAXIMIZE);
    UN(TileChildWindows)(p, 0);
    /* ArrangeWindows un-maximizes children first. */
    CHECK(!U(IsZoomed)(a));
    g5_destroy(a); g5_destroy(b); g5_destroy(p);
}

/* ---------------- SetWindowsHookExAW ---------------- */
static volatile LONG g_hk_calls;
static volatile UINT g_hk_msg;
static LRESULT CALLBACK hk_proc(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION && l && w == PM_REMOVE) { g_hk_calls++; g_hk_msg = ((MSG *)l)->message; }
    return U(CallNextHookEx)(NULL, code, w, l);
}
static LRESULT CALLBACK hk_nop(int code, WPARAM w, LPARAM l) { return U(CallNextHookEx)(NULL, code, w, l); }

static const int g_local_ids[] = { WH_MSGFILTER, WH_KEYBOARD, WH_GETMESSAGE, WH_CALLWNDPROC, WH_CBT, WH_MOUSE, WH_DEBUG, WH_SHELL, WH_FOREGROUNDIDLE, WH_CALLWNDPROCRET };

TEST(gap5_hookaw_local_install_unhook) {
    REQUIRE(UN(SetWindowsHookExAW)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(U(CallNextHookEx));
    for (DWORD ansi = 0; ansi <= 1; ansi++)
        for (unsigned i = 0; i < sizeof g_local_ids / sizeof g_local_ids[0]; i++) {
            SetLastError(0);
            HHOOK h = UN(SetWindowsHookExAW)(g_local_ids[i], hk_nop, NULL, GetCurrentThreadId(), ansi);
            int must = (g_local_ids[i] == WH_GETMESSAGE || g_local_ids[i] == WH_CALLWNDPROC || g_local_ids[i] == WH_MSGFILTER ||
                        g_local_ids[i] == WH_KEYBOARD || g_local_ids[i] == WH_MOUSE);
            if (must) CHECKF(h != NULL, "id %d ansi %lu err %lu", g_local_ids[i], ansi, GetLastError());
            else if (!h) t_note("local hook id %d ansi %lu refused err %lu", g_local_ids[i], ansi, GetLastError());
            if (h) CHECKF(U(UnhookWindowsHookEx)(h), "id %d unhook", g_local_ids[i]);
        }
}
TEST(gap5_hookaw_unhook_twice_fails) {
    REQUIRE(UN(SetWindowsHookExAW)); REQUIRE(U(UnhookWindowsHookEx));
    HHOOK h = UN(SetWindowsHookExAW)(WH_GETMESSAGE, hk_nop, NULL, GetCurrentThreadId(), 0);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(UnhookWindowsHookEx)(h));
    SetLastError(0);
    CHECK(!U(UnhookWindowsHookEx)(h));
    EXPECT_EQ(GetLastError(), ERR_INVALID_HOOK_HANDLE);
}
TEST(gap5_hookaw_distinct_handles) {
    REQUIRE(UN(SetWindowsHookExAW)); REQUIRE(U(UnhookWindowsHookEx));
    HHOOK h[4];
    for (int i = 0; i < 4; i++) h[i] = UN(SetWindowsHookExAW)(WH_GETMESSAGE, hk_nop, NULL, GetCurrentThreadId(), i & 1);
    for (int i = 0; i < 4; i++) CHECKF(h[i] != NULL, "hook %d", i);
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) CHECK(h[i] != h[j]);
    for (int i = 0; i < 4; i++) if (h[i]) CHECK(U(UnhookWindowsHookEx)(h[i]));
}
TEST(gap5_hookaw_getmessage_hook_fires) {
    REQUIRE(UN(SetWindowsHookExAW)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(U(PostThreadMessageA)); REQUIRE(U(PeekMessageA));
    for (DWORD ansi = 0; ansi <= 1; ansi++) {
        MSG m;
        U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);
        HHOOK h = UN(SetWindowsHookExAW)(WH_GETMESSAGE, hk_proc, NULL, GetCurrentThreadId(), ansi);
        CHECK(h != NULL);
        if (!h) return;
        g_hk_calls = 0; g_hk_msg = 0;
        CHECK(U(PostThreadMessageA)(GetCurrentThreadId(), WM_USER + 31, 5, 6));
        BOOL got = U(PeekMessageA)(&m, NULL, WM_USER + 31, WM_USER + 31, PM_REMOVE);
        CHECK(got);
        CHECKF(g_hk_calls >= 1, "ansi %lu hook calls %ld", ansi, g_hk_calls);
        EXPECT_EQ(g_hk_msg, WM_USER + 31);
        CHECK(U(UnhookWindowsHookEx)(h));
        /* After removal the hook no longer fires. */
        g_hk_calls = 0;
        U(PostThreadMessageA)(GetCurrentThreadId(), WM_USER + 32, 0, 0);
        U(PeekMessageA)(&m, NULL, WM_USER + 32, WM_USER + 32, PM_REMOVE);
        EXPECT_EQ(g_hk_calls, 0);
    }
}
TEST(gap5_hookaw_invalid_ids) {
    REQUIRE(UN(SetWindowsHookExAW));
    static const int ids[] = { -2, -3, -100, 15, 16, 100, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++)
        for (DWORD ansi = 0; ansi <= 1; ansi++) {
            SetLastError(0);
            HHOOK h = UN(SetWindowsHookExAW)(ids[i], hk_nop, NULL, GetCurrentThreadId(), ansi);
            CHECKF(h == NULL, "id %d ansi %lu", ids[i], ansi);
            if (GetLastError() != ERR_INVALID_HOOK_FILTER) t_note("invalid hook id %d: err %lu (expected %d)", ids[i], GetLastError(), ERR_INVALID_HOOK_FILTER);
            if (h) U(UnhookWindowsHookEx)(h);
        }
}
TEST(gap5_hookaw_global_needs_hmod) {
    REQUIRE(UN(SetWindowsHookExAW));
    static const int ids[] = { WH_GETMESSAGE, WH_CALLWNDPROC, WH_KEYBOARD, WH_MOUSE, WH_CBT, WH_SHELL, WH_MSGFILTER, WH_SYSMSGFILTER, WH_DEBUG };
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++)
        for (DWORD ansi = 0; ansi <= 1; ansi++) {
            SetLastError(0);
            HHOOK h = UN(SetWindowsHookExAW)(ids[i], hk_nop, NULL, 0, ansi);
            CHECKF(h == NULL, "id %d ansi %lu", ids[i], ansi);
            CHECKF(GetLastError() == ERR_HOOK_NEEDS_HMOD, "id %d err %lu", ids[i], GetLastError());
            if (h) U(UnhookWindowsHookEx)(h);
        }
}
TEST(gap5_hookaw_null_proc) {
    REQUIRE(UN(SetWindowsHookExAW));
    static const int ids[] = { WH_GETMESSAGE, WH_CALLWNDPROC, WH_KEYBOARD, WH_MSGFILTER };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0);
        HHOOK h = UN(SetWindowsHookExAW)(ids[i], NULL, NULL, GetCurrentThreadId(), 0);
        CHECKF(h == NULL, "id %d", ids[i]);
        t_note("null proc id %d err %lu", ids[i], GetLastError());
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(gap5_hookaw_bogus_thread) {
    REQUIRE(UN(SetWindowsHookExAW));
    static const DWORD tids[] = { 0xfffffff0, 0x7ffffff0, 0xffffffff, 0x00fffffc };
    for (unsigned i = 0; i < 4; i++) {
        SetLastError(0);
        HHOOK h = UN(SetWindowsHookExAW)(WH_GETMESSAGE, hk_nop, NULL, tids[i], 0);
        CHECKF(h == NULL, "tid 0x%lx", tids[i]);
        t_note("bogus tid 0x%lx err %lu", tids[i], GetLastError());
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(gap5_hookaw_bogus_hmod) {
    REQUIRE(UN(SetWindowsHookExAW));
    static const ULONG_PTR mods[] = { 0x1234, 0x10000, 0xdeadbeef0000ULL };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        HHOOK h = UN(SetWindowsHookExAW)(WH_GETMESSAGE, hk_nop, (HINSTANCE)mods[i], GetCurrentThreadId(), 0);
        CHECKF(h == NULL, "hmod %llx", (unsigned long long)mods[i]);
        t_note("bogus hmod %llx err %lu", (unsigned long long)mods[i], GetLastError());
        if (h) U(UnhookWindowsHookEx)(h);
    }
}
TEST(gap5_hookaw_real_hmod_wrong_thread_fields) {
    REQUIRE(UN(SetWindowsHookExAW));
    /* A real module handle with the current thread: allowed (hook proc is in this module). */
    HHOOK h = UN(SetWindowsHookExAW)(WH_GETMESSAGE, hk_nop, GetModuleHandleA(NULL), GetCurrentThreadId(), 0);
    t_note("hook with exe hmod: %p err %lu", h, GetLastError());
    if (h) CHECK(U(UnhookWindowsHookEx)(h));
}
TEST(gap5_hookaw_parity_with_ex_wrappers) {
    REQUIRE(UN(SetWindowsHookExAW)); REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(SetWindowsHookExW));
    for (int id = -3; id <= 17; id++) {
        SetLastError(0);
        HHOOK a = U(SetWindowsHookExA)(id, hk_nop, NULL, GetCurrentThreadId());
        DWORD ea = GetLastError();
        SetLastError(0);
        HHOOK w = U(SetWindowsHookExW)(id, hk_nop, NULL, GetCurrentThreadId());
        DWORD ew = GetLastError();
        SetLastError(0);
        HHOOK x = UN(SetWindowsHookExAW)(id, hk_nop, NULL, GetCurrentThreadId(), 0);
        DWORD ex = GetLastError();
        CHECKF((a != NULL) == (w != NULL), "id %d A/W success mismatch", id);
        CHECKF((x != NULL) == (w != NULL), "id %d AW/W success mismatch", id);
        if (!w) { CHECKF(ea == ew, "id %d err A %lu W %lu", id, ea, ew); CHECKF(ex == ew, "id %d err AW %lu W %lu", id, ex, ew); }
        if (a) U(UnhookWindowsHookEx)(a);
        if (w) U(UnhookWindowsHookEx)(w);
        if (x) U(UnhookWindowsHookEx)(x);
    }
}
static HHOOK g_many[64];
TEST(gap5_hookaw_many_hooks) {
    REQUIRE(UN(SetWindowsHookExAW)); REQUIRE(U(UnhookWindowsHookEx));
    int n = 0;
    for (; n < 64; n++) {
        g_many[n] = UN(SetWindowsHookExAW)(WH_CALLWNDPROC, hk_nop, NULL, GetCurrentThreadId(), n & 1);
        if (!g_many[n]) break;
    }
    CHECKF(n == 64, "only %d hooks created", n);
    for (int i = 0; i < n; i++) CHECKF(U(UnhookWindowsHookEx)(g_many[i]), "hook %d", i);
}
TEST(gap5_hookaw_other_thread_local_hook_same_process) {
    REQUIRE(UN(SetWindowsHookExAW));
    /* Thread-specific hook targeting a thread that does not exist any more. */
    DWORD tid = 0;
    HANDLE ht = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GetCurrentThreadId, NULL, 0, &tid);
    if (!ht) { t_skip("thread"); return; }
    WaitForSingleObject(ht, 5000); CloseHandle(ht);
    SetLastError(0);
    HHOOK h = UN(SetWindowsHookExAW)(WH_GETMESSAGE, hk_nop, NULL, tid, 0);
    CHECK(h == NULL);
    t_note("dead thread hook err %lu", GetLastError());
    if (h) U(UnhookWindowsHookEx)(h);
}

/* ---------------- SetWindowRgnEx ---------------- */
TEST(gap5_wrgnex_bogus_hwnd_keeps_region) {
    REQUIRE(UN(SetWindowRgnEx));
    HWND hs[] = { NULL, BOGUS_HWND, (HWND)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 3; i++) {
        HRGN rg = CreateRectRgn(0, 0, 10, 10);
        CHECK(rg != NULL);
        SetLastError(0);
        int r = UN(SetWindowRgnEx)(hs[i], rg, FALSE);
        CHECKF(r == 0, "idx %u ret %d", i, r);
        CHECKF(GetObjectType(rg) == OBJ_REGION, "idx %u region was freed on failure", i);
        t_note("SetWindowRgnEx bogus idx %u err %lu", i, GetLastError());
        DeleteObject(rg);
    }
}
TEST(gap5_wrgnex_destroyed_hwnd_keeps_region) {
    REQUIRE(UN(SetWindowRgnEx));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    g5_destroy(h);
    HRGN rg = CreateRectRgn(0, 0, 10, 10);
    EXPECT_EQ(UN(SetWindowRgnEx)(h, rg, TRUE), 0);
    CHECK(GetObjectType(rg) == OBJ_REGION);
    DeleteObject(rg);
}
TEST(gap5_wrgnex_valid_window_observe) {
    REQUIRE(UN(SetWindowRgnEx)); REQUIRE(U(GetWindowRgn)); REQUIRE(U(SetWindowRgn));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    HRGN rg = CreateRectRgn(0, 0, 50, 50);
    int r = UN(SetWindowRgnEx)(h, rg, FALSE);
    HRGN probe = CreateRectRgn(0, 0, 1, 1);
    int t = U(GetWindowRgn)(h, probe);
    t_note("SetWindowRgnEx valid: ret %d, GetWindowRgn type %d, region alive %d", r, t, GetObjectType(rg) == OBJ_REGION);
    DeleteObject(probe);
    U(SetWindowRgn)(h, NULL, FALSE);
    if (GetObjectType(rg) == OBJ_REGION) DeleteObject(rg);
    g5_destroy(h);
}
TEST(gap5_wrgnex_null_region_clears) {
    REQUIRE(UN(SetWindowRgnEx)); REQUIRE(U(GetWindowRgn));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    int r = UN(SetWindowRgnEx)(h, NULL, FALSE);
    HRGN probe = CreateRectRgn(0, 0, 1, 1);
    int t = U(GetWindowRgn)(h, probe);
    t_note("SetWindowRgnEx(NULL rgn): ret %d, GetWindowRgn -> %d", r, t);
    CHECK(t == ERROR || t == 0);
    DeleteObject(probe);
    g5_destroy(h);
}

/* ---------------- SetSystemMenu ---------------- */
TEST(gap5_sysmenu_bogus_hwnd) {
    REQUIRE(UN(SetSystemMenu));
    HMENU m = U(CreatePopupMenu)();
    HWND hs[] = { NULL, BOGUS_HWND, (HWND)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        BOOL r = UN(SetSystemMenu)(hs[i], m);
        CHECKF(r == FALSE, "idx %u", i);
        t_note("SetSystemMenu bogus hwnd idx %u err %lu", i, GetLastError());
    }
    if (U(IsMenu)(m)) U(DestroyMenu)(m);
}
TEST(gap5_sysmenu_destroyed_hwnd) {
    REQUIRE(UN(SetSystemMenu));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    g5_destroy(h);
    HMENU m = U(CreatePopupMenu)();
    EXPECT_EQ(UN(SetSystemMenu)(h, m), FALSE);
    if (U(IsMenu)(m)) U(DestroyMenu)(m);
}
TEST(gap5_sysmenu_bogus_menu) {
    REQUIRE(UN(SetSystemMenu)); REQUIRE(U(GetSystemMenu));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    HMENU before = U(GetSystemMenu)(h, FALSE);
    SetLastError(0);
    BOOL r = UN(SetSystemMenu)(h, (HMENU)(ULONG_PTR)0x12344);
    t_note("SetSystemMenu bogus menu: %d err %lu", r, GetLastError());
    HMENU after = U(GetSystemMenu)(h, FALSE);
    t_note("system menu unchanged after bogus set: %d", after == before);
    g5_destroy(h);
}
TEST(gap5_sysmenu_replace_observe) {
    REQUIRE(UN(SetSystemMenu)); REQUIRE(U(GetSystemMenu)); REQUIRE(U(AppendMenuA)); REQUIRE(U(GetMenuItemCount));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    HMENU m = U(CreatePopupMenu)();
    U(AppendMenuA)(m, MF_STRING, SC_CLOSE, "Close");
    U(AppendMenuA)(m, MF_STRING, SC_MOVE, "Move");
    BOOL r = UN(SetSystemMenu)(h, m);
    HMENU now = U(GetSystemMenu)(h, FALSE);
    t_note("SetSystemMenu valid: ret %d, GetSystemMenu==m %d, items %d", r, now == m, now ? U(GetMenuItemCount)(now) : -1);
    g5_destroy(h);
    if (U(IsMenu)(m)) U(DestroyMenu)(m);
}
TEST(gap5_sysmenu_null_menu_observe) {
    REQUIRE(UN(SetSystemMenu)); REQUIRE(U(GetSystemMenu));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    SetLastError(0);
    BOOL r = UN(SetSystemMenu)(h, NULL);
    t_note("SetSystemMenu(h, NULL): %d err %lu", r, GetLastError());
    g5_destroy(h);
}
TEST(gap5_sysmenu_window_without_sysmenu_observe) {
    REQUIRE(UN(SetSystemMenu));
    HWND h = g5_top(WS_POPUP, 0); CHECK(h != NULL);
    HMENU m = U(CreatePopupMenu)();
    SetLastError(0);
    BOOL r = UN(SetSystemMenu)(h, m);
    t_note("SetSystemMenu on popup: %d err %lu", r, GetLastError());
    g5_destroy(h);
    if (U(IsMenu)(m)) U(DestroyMenu)(m);
}
TEST(gap5_sysmenu_child_window_observe) {
    REQUIRE(UN(SetSystemMenu));
    HWND p = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(p != NULL);
    HWND c = t_make_window(NULL, WS_CHILD, 0, 0, 0, 10, 10, p);
    HMENU m = U(CreatePopupMenu)();
    SetLastError(0);
    BOOL r = UN(SetSystemMenu)(c, m);
    t_note("SetSystemMenu on child: %d err %lu", r, GetLastError());
    g5_destroy(p);
    if (U(IsMenu)(m)) U(DestroyMenu)(m);
}

/* ---------------- SetInternalWindowPos ---------------- */
TEST(gap5_siwp_bogus_hwnd) {
    REQUIRE(UN(SetInternalWindowPos));
    RECT rc = { 10, 20, 300, 200 }; POINT pt = { 5, 6 };
    HWND hs[] = { NULL, BOGUS_HWND, (HWND)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        BOOL r = UN(SetInternalWindowPos)(hs[i], SW_SHOWNORMAL, &rc, &pt);
        CHECKF(r == FALSE, "idx %u", i);
        t_note("SetInternalWindowPos bogus idx %u err %lu", i, GetLastError());
    }
}
TEST(gap5_siwp_destroyed_hwnd) {
    REQUIRE(UN(SetInternalWindowPos));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    g5_destroy(h);
    RECT rc = { 10, 20, 300, 200 }; POINT pt = { 5, 6 };
    EXPECT_EQ(UN(SetInternalWindowPos)(h, SW_SHOWNORMAL, &rc, &pt), FALSE);
}
TEST(gap5_siwp_valid_observe_placement) {
    REQUIRE(UN(SetInternalWindowPos)); REQUIRE(U(GetWindowPlacement));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    RECT rc = { 11, 22, 311, 222 }; POINT pt = { 5, 6 };
    SetLastError(0);
    BOOL r = UN(SetInternalWindowPos)(h, SW_SHOWNORMAL, &rc, &pt);
    WINDOWPLACEMENT wp; memset(&wp, 0, sizeof wp); wp.length = sizeof wp;
    BOOL g = U(GetWindowPlacement)(h, &wp);
    t_note("SetInternalWindowPos valid: ret %d err %lu; placement: ok %d show %u normal=(%ld,%ld,%ld,%ld) min=(%ld,%ld)",
           r, GetLastError(), g, wp.showCmd, wp.rcNormalPosition.left, wp.rcNormalPosition.top,
           wp.rcNormalPosition.right, wp.rcNormalPosition.bottom, wp.ptMinPosition.x, wp.ptMinPosition.y);
    CHECK(g);
    g5_destroy(h);
}
TEST(gap5_siwp_window_stays_hidden) {
    REQUIRE(UN(SetInternalWindowPos));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    RECT rc = { 11, 22, 311, 222 }; POINT pt = { 5, 6 };
    UN(SetInternalWindowPos)(h, SW_SHOWNORMAL, &rc, &pt);
    t_note("window visible after SetInternalWindowPos: %d", U(IsWindowVisible)(h));
    g5_destroy(h);
}

/* ---------------- SetWindowBand ---------------- */
TEST(gap5_band_bogus_hwnd) {
    REQUIRE(UN(SetWindowBand));
    HWND hs[] = { NULL, BOGUS_HWND, (HWND)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        BOOL r = UN(SetWindowBand)(hs[i], NULL, 1);
        CHECKF(r == FALSE, "idx %u", i);
        t_note("SetWindowBand bogus idx %u err %lu", i, GetLastError());
    }
}
TEST(gap5_band_destroyed_hwnd) {
    REQUIRE(UN(SetWindowBand));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    g5_destroy(h);
    EXPECT_EQ(UN(SetWindowBand)(h, NULL, 1), FALSE);
}
TEST(gap5_band_invalid_band_values) {
    REQUIRE(UN(SetWindowBand));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    static const DWORD bands[] = { 0, 0x100, 0xffff, 0x7fffffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof bands / sizeof bands[0]; i++) {
        SetLastError(0);
        BOOL r = UN(SetWindowBand)(h, NULL, bands[i]);
        t_note("SetWindowBand own window band 0x%lx -> %d err %lu", bands[i], r, GetLastError());
    }
    g5_destroy(h);
}
TEST(gap5_band_get_default_for_normal_window) {
    REQUIRE(UN(GetWindowBand));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    DWORD band = 0xcccccccc;
    BOOL r = UN(GetWindowBand)(h, &band);
    t_note("GetWindowBand normal window -> %d band %lu", r, band);
    g5_destroy(h);
}
TEST(gap5_band_set_same_band_observe) {
    REQUIRE(UN(SetWindowBand)); REQUIRE(UN(GetWindowBand));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    DWORD band = 0xcccccccc;
    UN(GetWindowBand)(h, &band);
    SetLastError(0);
    BOOL r = UN(SetWindowBand)(h, NULL, band == 0xcccccccc ? 1 : band);
    t_note("SetWindowBand(same band) -> %d err %lu", r, GetLastError());
    g5_destroy(h);
}

/* ---------------- SetWindowCompositionAttribute ---------------- */
TEST(gap5_wca_bogus_hwnd) {
    REQUIRE(UN(SetWindowCompositionAttribute));
    G5_ACCENT a = { 0, 0, 0, 0 };
    G5_WCAD d = { 19, &a, sizeof a };
    HWND hs[] = { NULL, BOGUS_HWND, (HWND)(LONG_PTR)-1 };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        BOOL r = UN(SetWindowCompositionAttribute)(hs[i], &d);
        CHECKF(r == FALSE, "idx %u", i);
        t_note("SetWindowCompositionAttribute bogus idx %u err %lu", i, GetLastError());
    }
}
TEST(gap5_wca_destroyed_hwnd) {
    REQUIRE(UN(SetWindowCompositionAttribute));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    g5_destroy(h);
    G5_ACCENT a = { 0, 0, 0, 0 };
    G5_WCAD d = { 19, &a, sizeof a };
    EXPECT_EQ(UN(SetWindowCompositionAttribute)(h, &d), FALSE);
}
TEST(gap5_wca_accent_disabled_observe) {
    REQUIRE(UN(SetWindowCompositionAttribute));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    G5_ACCENT a = { 0, 0, 0, 0 };
    G5_WCAD d = { 19, &a, sizeof a };
    SetLastError(0);
    BOOL r = UN(SetWindowCompositionAttribute)(h, &d);
    t_note("ACCENT_DISABLED -> %d err %lu", r, GetLastError());
    g5_destroy(h);
}
TEST(gap5_wca_invalid_attribute_observe) {
    REQUIRE(UN(SetWindowCompositionAttribute));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    G5_ACCENT a = { 0, 0, 0, 0 };
    static const DWORD attrs[] = { 0, 0x7fff, 0x10000, 0x7fffffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof attrs / sizeof attrs[0]; i++) {
        G5_WCAD d = { attrs[i], &a, sizeof a };
        SetLastError(0);
        BOOL r = UN(SetWindowCompositionAttribute)(h, &d);
        t_note("WCA attribute 0x%lx -> %d err %lu", attrs[i], r, GetLastError());
    }
    g5_destroy(h);
}
TEST(gap5_wca_wrong_size_observe) {
    REQUIRE(UN(SetWindowCompositionAttribute));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    G5_ACCENT a = { 0, 0, 0, 0 };
    static const SIZE_T sz[] = { 0, 1, 4, 15, 17, 0x1000 };
    for (unsigned i = 0; i < 4; i++) { /* only sizes <= the buffer so the probe stays in bounds */
        G5_WCAD d = { 19, &a, sz[i] };
        SetLastError(0);
        BOOL r = UN(SetWindowCompositionAttribute)(h, &d);
        t_note("ACCENT size %llu -> %d err %lu", (unsigned long long)sz[i], r, GetLastError());
    }
    g5_destroy(h);
}
TEST(gap5_wca_get_roundtrip_observe) {
    REQUIRE(UN(SetWindowCompositionAttribute)); REQUIRE(UN(GetWindowCompositionAttribute));
    HWND h = g5_top(WS_OVERLAPPEDWINDOW, 0); CHECK(h != NULL);
    G5_ACCENT a = { 0, 0, 0, 0 }, out = { 0xaa, 0xbb, 0xcc, 0xdd };
    G5_WCAD d = { 19, &a, sizeof a }, g = { 19, &out, sizeof out };
    BOOL s = UN(SetWindowCompositionAttribute)(h, &d);
    BOOL r = UN(GetWindowCompositionAttribute)(h, &g);
    t_note("WCA roundtrip: set %d get %d -> state=%lu flags=%lu", s, r, out.state, out.flags);
    g5_destroy(h);
}
