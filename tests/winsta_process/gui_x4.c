/* GetGuiResources accounting by object kind (menus, accelerator tables, icons, hooks, pens)
 * and its peak/error protocols. The kernel counts lag behind the creating call by a short,
 * variable time on Win10, so every check polls (bounded) for the expected direction. */
#include "wsp.h"

#define GUI_USER 1
#define GUI_GDI 0
#define GUI_USER_PEAK 4
#define GUI_GDI_PEAK 2

static DWORD gr(DWORD flags) { return U(GetGuiResources)(GetCurrentProcess(), flags); }
/* wait until the count satisfies dir (+1: >= target, -1: <= target); returns the last count */
static DWORD gr_wait(DWORD flags, DWORD target, int dir, DWORD ms)
{
    DWORD t0 = GetTickCount(), v;
    for (;;) {
        v = gr(flags);
        if (dir > 0 ? v >= target : v <= target) return v;
        if (GetTickCount() - t0 >= ms) return v;
        t_pump();
        Sleep(20);
    }
}
/* settle: two equal readings 100 ms apart */
static DWORD gr_settle(DWORD flags)
{
    DWORD a = gr(flags), b;
    int i;
    for (i = 0; i < 20; i++) { Sleep(100); b = gr(flags); if (b == a) return b; a = b; }
    return a;
}
static LRESULT CALLBACK hk_proc(int c, WPARAM w, LPARAM l) { return U(CallNextHookEx)(NULL, c, w, l); }

static void user_kind(const char *what, DWORD base, DWORD after_create, DWORD after_destroy)
{
    CHECKF(after_create >= base + 1, "%s: base %lu after create %lu", what, base, after_create);
    CHECKF(after_destroy < after_create, "%s: after create %lu after destroy %lu", what, after_create, after_destroy);
}

TEST(winsta_process_x4_guires_menu_counted) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu));
    DWORD base, c, d;
    HMENU m;
    { HWND w = t_make_window(NULL, WS_POPUP, 0, 0, 0, 8, 8, NULL); if (w) U(DestroyWindow)(w); }   /* implicit per-thread objects */
    base = gr_settle(GUI_USER);
    if (!base) { t_skip("GetGuiResources reports no user objects"); return; }
    m = U(CreateMenu)();
    CHECK(m != NULL);
    c = gr_wait(GUI_USER, base + 1, 1, 3000);
    CHECK(U(DestroyMenu)(m));
    d = gr_wait(GUI_USER, c - 1, -1, 3000);
    user_kind("menu", base, c, d);
}
TEST(winsta_process_x4_guires_accel_counted) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateAcceleratorTableW)); REQUIRE(U(DestroyAcceleratorTable));
    DWORD base, c, d;
    ACCEL ac[2] __attribute__((aligned(8)));
    HACCEL h;
    ac[0].fVirt = FVIRTKEY; ac[0].key = 'A'; ac[0].cmd = 101;
    ac[1].fVirt = FVIRTKEY | FCONTROL; ac[1].key = 'B'; ac[1].cmd = 102;
    base = gr_settle(GUI_USER);
    if (!base) { t_skip("GetGuiResources reports no user objects"); return; }
    h = U(CreateAcceleratorTableW)(ac, 2);
    CHECK(h != NULL);
    c = gr_wait(GUI_USER, base + 1, 1, 3000);
    CHECK(U(DestroyAcceleratorTable)(h));
    d = gr_wait(GUI_USER, c - 1, -1, 3000);
    user_kind("accelerator table", base, c, d);
}
TEST(winsta_process_x4_guires_icon_counted) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CopyIcon)); REQUIRE(U(DestroyIcon)); REQUIRE(U(LoadIconA));
    DWORD base, c, d;
    HICON src = U(LoadIconA)(NULL, IDI_APPLICATION), cp;
    CHECK(src != NULL);
    base = gr_settle(GUI_USER);
    if (!base || !src) { t_skip("no baseline or no icon"); return; }
    cp = U(CopyIcon)(src);
    CHECK(cp != NULL);
    CHECK(cp != src);
    c = gr_wait(GUI_USER, base + 1, 1, 3000);
    CHECK(U(DestroyIcon)(cp));
    d = gr_wait(GUI_USER, c - 1, -1, 3000);
    user_kind("icon copy", base, c, d);
}
TEST(winsta_process_x4_guires_hook_counted) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(U(CallNextHookEx));
    DWORD base, c, d;
    HHOOK h;
    base = gr_settle(GUI_USER);
    if (!base) { t_skip("GetGuiResources reports no user objects"); return; }
    h = U(SetWindowsHookExA)(WH_GETMESSAGE, hk_proc, NULL, GetCurrentThreadId());
    CHECK(h != NULL);
    c = gr_wait(GUI_USER, base + 1, 1, 3000);
    CHECK(U(UnhookWindowsHookEx)(h));
    d = gr_wait(GUI_USER, c - 1, -1, 3000);
    user_kind("hook", base, c, d);
}
TEST(winsta_process_x4_guires_gdi_pens_counted) {
    REQUIRE(U(GetGuiResources));
    enum { N = 10 };
    HPEN p[N];
    DWORD base, c, d;
    int i;
    base = gr_settle(GUI_GDI);
    if (!base) { t_skip("GetGuiResources reports no GDI objects"); return; }
    for (i = 0; i < N; i++) { p[i] = CreatePen(PS_SOLID, 1, RGB(i, 0, 0)); CHECK(p[i] != NULL); }
    c = gr_wait(GUI_GDI, base + N, 1, 3000);
    CHECKF(c >= base + N, "base %lu after %d pens %lu", base, N, c);
    for (i = 0; i < N; i++) if (p[i]) CHECK(DeleteObject(p[i]));
    d = gr_wait(GUI_GDI, c - N, -1, 3000);
    CHECKF(d <= c - N, "after create %lu after delete %lu", c, d);
}
TEST(winsta_process_x4_guires_peak_never_below_current_or_decreasing) {
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu));
    HMENU m[20];
    DWORD cur, pk1, pk2, gcur, gpk;
    int i;
    for (i = 0; i < 20; i++) m[i] = U(CreateMenu)();
    gr_wait(GUI_USER, gr(GUI_USER), 1, 200);
    cur = gr(GUI_USER); pk1 = gr(GUI_USER_PEAK);
    CHECKF(pk1 >= cur, "user peak %lu < current %lu", pk1, cur);
    for (i = 0; i < 20; i++) if (m[i]) U(DestroyMenu)(m[i]);
    gr_wait(GUI_USER, cur - 20, -1, 3000);
    pk2 = gr(GUI_USER_PEAK);
    CHECKF(pk2 >= pk1, "user peak decreased %lu -> %lu", pk1, pk2);
    gcur = gr(GUI_GDI); gpk = gr(GUI_GDI_PEAK);
    CHECKF(gpk >= gcur, "gdi peak %lu < current %lu", gpk, gcur);
}
TEST(winsta_process_x4_guires_closed_handle_and_rights) {
    REQUIRE(U(GetGuiResources));
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    DWORD r;
    if (!h) { t_skip("OpenProcess failed: %lu", GetLastError()); return; }
    r = U(GetGuiResources)(h, GUI_USER);
    CHECKF(r != 0, "user objects through a real process handle: %lu", r);
    CloseHandle(h);
    SetLastError(0);
    r = U(GetGuiResources)(h, GUI_USER);
    CHECK(r == 0);
    t_note("GetGuiResources(closed handle) err %lu", GetLastError());
    h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (h) {
        SetLastError(0);
        r = U(GetGuiResources)(h, GUI_USER);
        t_note("GetGuiResources(SYNCHRONIZE-only handle) = %lu err %lu", r, GetLastError());
        CloseHandle(h);
    }
}
TEST(winsta_process_x4_guires_flags_matrix) {
    REQUIRE(U(GetGuiResources));
    static const DWORD bad[] = { 3, 5, 6, 7, 8, 16, 0x100, 0x80000000u, 0xFFFFFFFFu };
    unsigned i;
    DWORD u = gr(GUI_USER);
    CHECK(u > 0);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWORD r; SetLastError(0);
        r = U(GetGuiResources)(GetCurrentProcess(), bad[i]);
        t_note("GetGuiResources(flags 0x%lx) = %lu err %lu", bad[i], r, GetLastError());
    }
    CHECK(gr(GUI_USER) > 0);
    CHECK(gr(GUI_GDI) > 0);
}
