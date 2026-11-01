#include "ri.h"

/* System metrics for icons/cursors, window and class icons, CURSORINFO, SetCursor,
 * default layout, GetGuiResources and SetSystemCursor. */

#define RI_CTX_UNAWARE      ((DPI_AWARENESS_CONTEXT)-1)
#define RI_CTX_SYSTEM       ((DPI_AWARENESS_CONTEXT)-2)
#define RI_CTX_PERMON       ((DPI_AWARENESS_CONTEXT)-3)
#define RI_CTX_PERMON_V2    ((DPI_AWARENESS_CONTEXT)-4)

static HICON ri_mkicon(int w, DWORD rgb)
{
    ICONINFO ii; HICON ic;
    BYTE bits[16 * 64]; BITMAPINFO bi; void *pb = NULL;
    memset(bits, 0, sizeof bits);
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -w;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    ii.fIcon = TRUE; ii.xHotspot = ii.yHotspot = 0;
    ii.hbmMask = CreateBitmap(w, w, 1, 1, bits);
    ii.hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pb, NULL, 0);
    ic = U(CreateIconIndirect)(&ii);
    DeleteObject(ii.hbmMask); DeleteObject(ii.hbmColor);
    return ic;
}

/* ---- metrics ---------------------------------------------------------- */

TEST(resources_icons_sm_basic_positive)
{
    static const int idx[] = {SM_CXICON, SM_CYICON, SM_CXSMICON, SM_CYSMICON, SM_CXCURSOR, SM_CYCURSOR,
                              SM_CXICONSPACING, SM_CYICONSPACING, SM_CXMINIMIZED, SM_CYMINIMIZED};
    int i;
    REQUIRE(U(GetSystemMetrics));
    for (i = 0; i < 10; i++) CHECKF(U(GetSystemMetrics)(idx[i]) > 0, "metric %d = %d", idx[i], U(GetSystemMetrics)(idx[i]));
}

TEST(resources_icons_sm_relations)
{
    REQUIRE(U(GetSystemMetrics));
    CHECK(U(GetSystemMetrics)(SM_CXSMICON) <= U(GetSystemMetrics)(SM_CXICON));
    CHECK(U(GetSystemMetrics)(SM_CYSMICON) <= U(GetSystemMetrics)(SM_CYICON));
    CHECK(U(GetSystemMetrics)(SM_CXICONSPACING) >= U(GetSystemMetrics)(SM_CXICON));
    CHECK(U(GetSystemMetrics)(SM_CYICONSPACING) >= U(GetSystemMetrics)(SM_CYICON));
    RI_EQ(U(GetSystemMetrics)(SM_CXICON), U(GetSystemMetrics)(SM_CYICON));
    RI_EQ(U(GetSystemMetrics)(SM_CXSMICON), U(GetSystemMetrics)(SM_CYSMICON));
    RI_EQ(U(GetSystemMetrics)(SM_CXCURSOR), U(GetSystemMetrics)(SM_CYCURSOR));
}

TEST(resources_icons_sm_repeatable)
{
    int i, a, bad = 0;
    REQUIRE(U(GetSystemMetrics));
    a = U(GetSystemMetrics)(SM_CXICON);
    for (i = 0; i < 1000; i++) if (U(GetSystemMetrics)(SM_CXICON) != a) bad++;
    RI_EQ(bad, 0);
}

TEST(resources_icons_sm_for_dpi_table)
{
    static const struct { UINT dpi; int icon, small_, cursor; } t[] = {
        {96, 32, 16, 32}, {120, 40, 20, 40}, {144, 48, 24, 48}, {192, 64, 32, 64}, {288, 96, 48, 96} };
    int i;
    REQUIRE(U(GetSystemMetricsForDpi));
    for (i = 0; i < 5; i++) {
        CHECKF(U(GetSystemMetricsForDpi)(SM_CXICON, t[i].dpi) == t[i].icon, "dpi %u SM_CXICON=%d", t[i].dpi, U(GetSystemMetricsForDpi)(SM_CXICON, t[i].dpi));
        CHECKF(U(GetSystemMetricsForDpi)(SM_CYICON, t[i].dpi) == t[i].icon, "dpi %u SM_CYICON", t[i].dpi);
        CHECKF(U(GetSystemMetricsForDpi)(SM_CXSMICON, t[i].dpi) == t[i].small_, "dpi %u SM_CXSMICON=%d", t[i].dpi, U(GetSystemMetricsForDpi)(SM_CXSMICON, t[i].dpi));
        CHECKF(U(GetSystemMetricsForDpi)(SM_CYSMICON, t[i].dpi) == t[i].small_, "dpi %u SM_CYSMICON", t[i].dpi);
        if (t[i].dpi == 96 || t[i].dpi == 192) {
            CHECKF(U(GetSystemMetricsForDpi)(SM_CXCURSOR, t[i].dpi) == t[i].cursor, "dpi %u SM_CXCURSOR=%d", t[i].dpi, U(GetSystemMetricsForDpi)(SM_CXCURSOR, t[i].dpi));
            CHECKF(U(GetSystemMetricsForDpi)(SM_CYCURSOR, t[i].dpi) == t[i].cursor, "dpi %u SM_CYCURSOR", t[i].dpi);
        } else t_note("dpi %u SM_CXCURSOR=%d", t[i].dpi, U(GetSystemMetricsForDpi)(SM_CXCURSOR, t[i].dpi));
    }
}

TEST(resources_icons_sm_for_dpi_monotonic)
{
    UINT d; int prev = 0, bad = 0;
    REQUIRE(U(GetSystemMetricsForDpi));
    for (d = 96; d <= 480; d += 12) {
        int v = U(GetSystemMetricsForDpi)(SM_CXICON, d);
        if (v < prev) bad++;
        prev = v;
    }
    RI_EQ(bad, 0);
}

TEST(resources_icons_sm_for_dpi_zero_note)
{
    int v;
    REQUIRE(U(GetSystemMetricsForDpi));
    SetLastError(0);
    v = U(GetSystemMetricsForDpi)(SM_CXICON, 0);
    t_note("GetSystemMetricsForDpi(SM_CXICON, 0) = %d err=%lu", v, GetLastError());
}

TEST(resources_icons_sm_for_dpi_matches_system)
{
    UINT dpi;
    REQUIRE(U(GetSystemMetricsForDpi)); REQUIRE(U(GetDpiForSystem));
    dpi = U(GetDpiForSystem)();
    CHECK(dpi >= 96);
    t_note("system dpi %u -> SM_CXICON %d", dpi, U(GetSystemMetricsForDpi)(SM_CXICON, dpi));
}

TEST(resources_icons_sm_thread_dpi_unaware)
{
    DPI_AWARENESS_CONTEXT old;
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(SetThreadDpiAwarenessContext));
    old = U(SetThreadDpiAwarenessContext)(RI_CTX_UNAWARE);
    if (!old) { t_skip("SetThreadDpiAwarenessContext failed"); return; }
    RI_EQ(U(GetSystemMetrics)(SM_CXICON), 32);
    RI_EQ(U(GetSystemMetrics)(SM_CYICON), 32);
    RI_EQ(U(GetSystemMetrics)(SM_CXSMICON), 16);
    RI_EQ(U(GetSystemMetrics)(SM_CYSMICON), 16);
    RI_EQ(U(GetSystemMetrics)(SM_CXCURSOR), 32);
    RI_EQ(U(GetSystemMetrics)(SM_CYCURSOR), 32);
    U(SetThreadDpiAwarenessContext)(old);
}

TEST(resources_icons_sm_thread_dpi_aware_matches_forDpi)
{
    DPI_AWARENESS_CONTEXT old;
    REQUIRE(U(GetSystemMetrics)); REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(GetDpiForSystem)); REQUIRE(U(GetSystemMetricsForDpi));
    old = U(SetThreadDpiAwarenessContext)(RI_CTX_SYSTEM);
    if (!old) { t_skip("SetThreadDpiAwarenessContext failed"); return; }
    RI_EQ(U(GetSystemMetrics)(SM_CXICON), U(GetSystemMetricsForDpi)(SM_CXICON, U(GetDpiForSystem)()));
    RI_EQ(U(GetSystemMetrics)(SM_CXSMICON), U(GetSystemMetricsForDpi)(SM_CXSMICON, U(GetDpiForSystem)()));
    RI_EQ(U(GetSystemMetrics)(SM_CXCURSOR), U(GetSystemMetricsForDpi)(SM_CXCURSOR, U(GetDpiForSystem)()));
    U(SetThreadDpiAwarenessContext)(old);
}

TEST(resources_icons_sm_defaultsize_follows_dpi_context)
{
    DPI_AWARENESS_CONTEXT old; int w = 0, h = 0;
    HICON ic;
    REQUIRE(U(CopyImage)); REQUIRE(U(SetThreadDpiAwarenessContext)); REQUIRE(U(CreateIconIndirect));
    ic = ri_mkicon(20, 0);
    CHECK(ic != NULL);
    if (!ic) return;
    old = U(SetThreadDpiAwarenessContext)(RI_CTX_UNAWARE);
    if (old) {
        HICON cp = U(CopyImage)(ic, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        CHECK(cp != NULL);
        if (cp) {
            ri_iconsize(cp, &w, &h, NULL, NULL);
            t_note("LR_DEFAULTSIZE copy in unaware context: %dx%d", w, h);
            CHECK(w > 0);
            U(DestroyIcon)(cp);
        }
        U(SetThreadDpiAwarenessContext)(old);
    }
    U(DestroyIcon)(ic);
}

/* ---- window and class icons ------------------------------------------ */

static const char *ri_regclass(const char *name, HICON big, HICON small_)
{
    WNDCLASSEXA wc;
    ATOM (WINAPI *rc)(const WNDCLASSEXA *) = (void *)t_proc("RegisterClassExA");
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = (WNDPROC)t_proc("DefWindowProcA");
    wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = name;
    wc.hIcon = big; wc.hIconSm = small_;
    if (!rc || !rc(&wc)) return NULL;
    return name;
}
static void ri_unreg(const char *name)
{
    BOOL (WINAPI *ur)(LPCSTR, HINSTANCE) = (void *)t_proc("UnregisterClassA");
    if (ur) ur(name, GetModuleHandleA(NULL));
}

TEST(resources_icons_wi_seticon_big_roundtrip)
{
    HWND w; HICON a, b;
    REQUIRE(U(SendMessageW)); REQUIRE(U(CreateIconIndirect)); REQUIRE(U(DestroyWindow));
    if (!ri_regclass("ri_wi_cls1", NULL, NULL)) { t_skip("RegisterClassEx"); return; }
    w = t_make_window("ri_wi_cls1", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    CHECK(w != NULL);
    a = ri_mkicon(32, 1); b = ri_mkicon(32, 2);
    if (w && a && b) {
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_BIG, 0), 0);
        RI_EQ(U(SendMessageW)(w, WM_SETICON, ICON_BIG, (LPARAM)a), 0);
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_BIG, 0), a);
        RI_EQ(U(SendMessageW)(w, WM_SETICON, ICON_BIG, (LPARAM)b), a);
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_BIG, 0), b);
        RI_EQ(U(SendMessageW)(w, WM_SETICON, ICON_BIG, 0), b);
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_BIG, 0), 0);
    }
    if (w) U(DestroyWindow)(w);
    if (a) U(DestroyIcon)(a);
    if (b) U(DestroyIcon)(b);
    ri_unreg("ri_wi_cls1");
}

TEST(resources_icons_wi_seticon_small_roundtrip)
{
    HWND w; HICON a, b;
    REQUIRE(U(SendMessageW)); REQUIRE(U(CreateIconIndirect)); REQUIRE(U(DestroyWindow));
    if (!ri_regclass("ri_wi_cls2", NULL, NULL)) { t_skip("RegisterClassEx"); return; }
    w = t_make_window("ri_wi_cls2", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    CHECK(w != NULL);
    a = ri_mkicon(16, 1); b = ri_mkicon(16, 2);
    if (w && a && b) {
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_SMALL, 0), 0);
        RI_EQ(U(SendMessageW)(w, WM_SETICON, ICON_SMALL, (LPARAM)a), 0);
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_SMALL, 0), a);
        RI_EQ(U(SendMessageW)(w, WM_SETICON, ICON_SMALL, (LPARAM)b), a);
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_SMALL, 0), b);
        RI_EQ(U(SendMessageW)(w, WM_SETICON, ICON_SMALL, 0), b);
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_SMALL, 0), 0);
    }
    if (w) U(DestroyWindow)(w);
    if (a) U(DestroyIcon)(a);
    if (b) U(DestroyIcon)(b);
    ri_unreg("ri_wi_cls2");
}

TEST(resources_icons_wi_big_and_small_independent)
{
    HWND w; HICON a, b;
    REQUIRE(U(SendMessageW)); REQUIRE(U(CreateIconIndirect)); REQUIRE(U(DestroyWindow));
    if (!ri_regclass("ri_wi_cls3", NULL, NULL)) { t_skip("RegisterClassEx"); return; }
    w = t_make_window("ri_wi_cls3", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    a = ri_mkicon(32, 1); b = ri_mkicon(16, 2);
    if (w && a && b) {
        U(SendMessageW)(w, WM_SETICON, ICON_BIG, (LPARAM)a);
        U(SendMessageW)(w, WM_SETICON, ICON_SMALL, (LPARAM)b);
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_BIG, 0), a);
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_SMALL, 0), b);
        /* ICON_SMALL2 returns the application-provided small icon */
        RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_SMALL2, 0), b);
    } else CHECK(0);
    if (w) U(DestroyWindow)(w);
    if (a) U(DestroyIcon)(a);
    if (b) U(DestroyIcon)(b);
    ri_unreg("ri_wi_cls3");
}

TEST(resources_icons_wi_icon_survives_window_destroy)
{
    HWND w; HICON a; ICONINFO ii;
    REQUIRE(U(SendMessageW)); REQUIRE(U(CreateIconIndirect)); REQUIRE(U(DestroyWindow));
    if (!ri_regclass("ri_wi_cls4", NULL, NULL)) { t_skip("RegisterClassEx"); return; }
    w = t_make_window("ri_wi_cls4", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    a = ri_mkicon(32, 1);
    if (w && a) {
        U(SendMessageW)(w, WM_SETICON, ICON_BIG, (LPARAM)a);
        CHECK(U(DestroyWindow)(w));
        w = NULL;
        CHECK(U(GetIconInfo)(a, &ii));
        DeleteObject(ii.hbmMask); DeleteObject(ii.hbmColor);
        RI_EQ(U(DestroyIcon)(a), TRUE);
        a = NULL;
    } else CHECK(0);
    if (w) U(DestroyWindow)(w);
    if (a) U(DestroyIcon)(a);
    ri_unreg("ri_wi_cls4");
}

TEST(resources_icons_wi_seticon_bad_which_note)
{
    HWND w; HICON a;
    REQUIRE(U(SendMessageW)); REQUIRE(U(CreateIconIndirect)); REQUIRE(U(DestroyWindow));
    if (!ri_regclass("ri_wi_cls5", NULL, NULL)) { t_skip("RegisterClassEx"); return; }
    w = t_make_window("ri_wi_cls5", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    a = ri_mkicon(32, 1);
    if (w && a) {
        LRESULT r = U(SendMessageW)(w, WM_SETICON, 3, (LPARAM)a);
        t_note("WM_SETICON wParam=3 -> %p; BIG=%p SMALL=%p", (void *)r, (void *)U(SendMessageW)(w, WM_GETICON, ICON_BIG, 0), (void *)U(SendMessageW)(w, WM_GETICON, ICON_SMALL, 0));
        r = U(SendMessageW)(w, WM_SETICON, ICON_SMALL2, (LPARAM)a);
        t_note("WM_SETICON ICON_SMALL2 -> %p; SMALL=%p", (void *)r, (void *)U(SendMessageW)(w, WM_GETICON, ICON_SMALL, 0));
        r = U(SendMessageW)(w, WM_GETICON, 77, 0);
        t_note("WM_GETICON wParam=77 -> %p", (void *)r);
    }
    if (w) U(DestroyWindow)(w);
    if (a) U(DestroyIcon)(a);
    ri_unreg("ri_wi_cls5");
}

TEST(resources_icons_wi_destroyed_window_message)
{
    HWND w;
    REQUIRE(U(SendMessageW)); REQUIRE(U(DestroyWindow));
    if (!ri_regclass("ri_wi_cls6", NULL, NULL)) { t_skip("RegisterClassEx"); return; }
    w = t_make_window("ri_wi_cls6", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    CHECK(w != NULL);
    if (w) U(DestroyWindow)(w);
    SetLastError(0);
    RI_EQ(U(SendMessageW)(w, WM_GETICON, ICON_BIG, 0), 0);
    RI_EQ(U(SendMessageW)(w, WM_SETICON, ICON_BIG, 0), 0);
    ri_unreg("ri_wi_cls6");
}

TEST(resources_icons_wi_class_icon_registered)
{
    HICON a, b; HWND w; WNDCLASSEXA wc;
    BOOL (WINAPI *gci)(HINSTANCE, LPCSTR, LPWNDCLASSEXA) = (void *)t_proc("GetClassInfoExA");
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(DestroyWindow));
    REQUIRE(gci);
    a = ri_mkicon(32, 1); b = ri_mkicon(16, 2);
    if (!a || !b || !ri_regclass("ri_wi_cls7", a, b)) { t_skip("setup"); return; }
    w = t_make_window("ri_wi_cls7", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    CHECK(w != NULL);
    if (w) {
        RI_EQ(U(GetClassLongPtrA)(w, GCLP_HICON), a);
        RI_EQ(U(GetClassLongPtrA)(w, GCLP_HICONSM), b);
        memset(&wc, 0, sizeof wc); wc.cbSize = sizeof wc;
        CHECK(gci(GetModuleHandleA(NULL), "ri_wi_cls7", &wc));
        RI_EQ(wc.hIcon, a); RI_EQ(wc.hIconSm, b);
        t_note("window with class icons: WM_GETICON BIG=%p SMALL=%p (class %p/%p)",
               (void *)U(SendMessageW)(w, WM_GETICON, ICON_BIG, 0), (void *)U(SendMessageW)(w, WM_GETICON, ICON_SMALL, 0), a, b);
        U(DestroyWindow)(w);
    }
    ri_unreg("ri_wi_cls7");
    U(DestroyIcon)(a); U(DestroyIcon)(b);
}

TEST(resources_icons_wi_class_icon_setclasslong)
{
    HICON a, b, c; HWND w;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(SetClassLongPtrA)); REQUIRE(U(DestroyWindow));
    a = ri_mkicon(32, 1); b = ri_mkicon(32, 2); c = ri_mkicon(16, 3);
    if (!a || !b || !c || !ri_regclass("ri_wi_cls8", a, NULL)) { t_skip("setup"); return; }
    w = t_make_window("ri_wi_cls8", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    CHECK(w != NULL);
    if (w) {
        RI_EQ(U(SetClassLongPtrA)(w, GCLP_HICON, (LONG_PTR)b), a);
        RI_EQ(U(GetClassLongPtrA)(w, GCLP_HICON), b);
        RI_EQ(U(SetClassLongPtrA)(w, GCLP_HICON, 0), b);
        RI_EQ(U(GetClassLongPtrA)(w, GCLP_HICON), 0);
        {
            ULONG_PTR prev = U(SetClassLongPtrA)(w, GCLP_HICONSM, (LONG_PTR)c);
            t_note("SetClassLongPtr(GCLP_HICONSM) previous=%p", (void *)prev);
            RI_EQ(U(GetClassLongPtrA)(w, GCLP_HICONSM), c);
        }
        U(DestroyWindow)(w);
    }
    ri_unreg("ri_wi_cls8");
    U(DestroyIcon)(a); U(DestroyIcon)(b); U(DestroyIcon)(c);
}

TEST(resources_icons_wi_class_icon_hiconsm_default_note)
{
    HICON a; HWND w;
    REQUIRE(U(CreateIconIndirect)); REQUIRE(U(GetClassLongPtrA)); REQUIRE(U(DestroyWindow));
    a = ri_mkicon(32, 1);
    if (!a || !ri_regclass("ri_wi_cls9", a, NULL)) { t_skip("setup"); return; }
    w = t_make_window("ri_wi_cls9", WS_OVERLAPPED, 0, 0, 0, 100, 100, NULL);
    if (w) {
        t_note("class with hIcon only: GCLP_HICONSM=%p", (void *)U(GetClassLongPtrA)(w, GCLP_HICONSM));
        U(DestroyWindow)(w);
    }
    ri_unreg("ri_wi_cls9");
    U(DestroyIcon)(a);
}

TEST(resources_icons_wi_class_icon_survives_unregister)
{
    HICON a; ICONINFO ii;
    REQUIRE(U(CreateIconIndirect));
    a = ri_mkicon(32, 1);
    if (!a || !ri_regclass("ri_wi_cls10", a, NULL)) { t_skip("setup"); return; }
    ri_unreg("ri_wi_cls10");
    CHECK(U(GetIconInfo)(a, &ii));
    DeleteObject(ii.hbmMask); DeleteObject(ii.hbmColor);
    RI_EQ(U(DestroyIcon)(a), TRUE);
}

/* ---- CURSORINFO / SetCursor ------------------------------------------ */

TEST(resources_icons_ci_getcursorinfo_basic)
{
    CURSORINFO ci; POINT pt;
    REQUIRE(U(GetCursorInfo));
    memset(&ci, 0xCC, sizeof ci);
    ci.cbSize = sizeof ci;
    CHECK(U(GetCursorInfo)(&ci));
    /* Win10 22H2 oracle: on success cbSize is returned as 0 in the output struct */
    t_note("GetCursorInfo output cbSize=%lu", (unsigned long)ci.cbSize);
    CHECK((ci.flags & ~(DWORD)(CURSOR_SHOWING | 2)) == 0);
    if (U(GetCursorPos) && U(GetCursorPos)(&pt)) {
        CHECK(abs(pt.x - ci.ptScreenPos.x) < 200 && abs(pt.y - ci.ptScreenPos.y) < 200);
    }
    if (ci.hCursor) {
        ICONINFO ii;
        CHECK(U(GetIconInfo)(ci.hCursor, &ii));
        RI_EQ(ii.fIcon, FALSE);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
    }
    t_note("CURSORINFO flags=0x%lx hCursor=%p pos=%ld,%ld", ci.flags, ci.hCursor, ci.ptScreenPos.x, ci.ptScreenPos.y);
}

TEST(resources_icons_ci_getcursorinfo_bad_cbsize)
{
    CURSORINFO ci;
    static const DWORD sizes[] = {0, 1, sizeof(CURSORINFO) - 1, sizeof(CURSORINFO) + 1, sizeof(CURSORINFO) * 2, 0xFFFFFFFF};
    int i;
    REQUIRE(U(GetCursorInfo));
    for (i = 0; i < 6; i++) {
        memset(&ci, 0, sizeof ci);
        ci.cbSize = sizes[i];
        SetLastError(0);
        /* win32k-side validation, not visible in the user32 decomp: informational only */
        t_note("GetCursorInfo cbSize=%lu -> %d err=%lu", sizes[i], U(GetCursorInfo)(&ci), GetLastError());
    }
}

TEST(resources_icons_ci_getcursorinfo_repeat)
{
    int i, bad = 0;
    CURSORINFO ci;
    REQUIRE(U(GetCursorInfo));
    for (i = 0; i < 1000; i++) { ci.cbSize = sizeof ci; if (!U(GetCursorInfo)(&ci)) bad++; }
    RI_EQ(bad, 0);
}

TEST(resources_icons_ci_setcursor_roundtrip)
{
    HCURSOR old, a, b, prev;
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorW));
    a = U(LoadCursorW)(NULL, (LPCWSTR)IDC_ARROW);
    b = U(LoadCursorW)(NULL, (LPCWSTR)IDC_CROSS);
    CHECK(a && b && a != b);
    old = U(GetCursor)();
    prev = U(SetCursor)(a);
    RI_EQ(prev, old);
    RI_EQ(U(GetCursor)(), a);
    prev = U(SetCursor)(b);
    RI_EQ(prev, a);
    RI_EQ(U(GetCursor)(), b);
    prev = U(SetCursor)(b);
    RI_EQ(prev, b);
    prev = U(SetCursor)(NULL);
    RI_EQ(prev, b);
    RI_EQ(U(GetCursor)(), NULL);
    prev = U(SetCursor)(a);
    RI_EQ(prev, NULL);
    U(SetCursor)(old);
}

TEST(resources_icons_ci_setcursor_invalid_handle_note)
{
    HCURSOR old, a, r;
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(LoadCursorW));
    a = U(LoadCursorW)(NULL, (LPCWSTR)IDC_ARROW);
    old = U(GetCursor)();
    U(SetCursor)(a);
    SetLastError(0);
    r = U(SetCursor)((HCURSOR)(ULONG_PTR)0x12345);
    t_note("SetCursor(bogus) -> %p err=%lu; GetCursor=%p (arrow %p)", r, GetLastError(), U(GetCursor)(), a);
    U(SetCursor)(a);
    r = U(SetCursor)((HCURSOR)(ULONG_PTR)-1);
    t_note("SetCursor(-1) -> %p; GetCursor=%p", r, U(GetCursor)());
    U(SetCursor)(old);
}

TEST(resources_icons_ci_setcursor_destroyed_note)
{
    HICON c; HCURSOR old, r;
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(CreateIconIndirect));
    c = ri_mkicon(32, 1);
    if (!c) { CHECK(0); return; }
    old = U(GetCursor)();
    U(DestroyIcon)(c);
    SetLastError(0);
    r = U(SetCursor)((HCURSOR)c);
    t_note("SetCursor(destroyed) -> %p err=%lu; GetCursor=%p", r, GetLastError(), U(GetCursor)());
    U(SetCursor)(old);
}

TEST(resources_icons_ci_setcursor_custom_cursor)
{
    BYTE a[128], x[128]; HCURSOR c, old;
    REQUIRE(U(SetCursor)); REQUIRE(U(GetCursor)); REQUIRE(U(CreateCursor));
    memset(a, 0xFF, sizeof a); memset(x, 0, sizeof x);
    c = U(CreateCursor)(NULL, 0, 0, 32, 32, a, x);
    CHECK(c != NULL);
    if (!c) return;
    old = U(GetCursor)();
    U(SetCursor)(c);
    RI_EQ(U(GetCursor)(), c);
    U(SetCursor)(old);
    CHECK(U(DestroyCursor)(c));
}

TEST(resources_icons_ci_showcursor_counter)
{
    int a, b, c;
    REQUIRE(U(ShowCursor));
    a = U(ShowCursor)(TRUE);
    b = U(ShowCursor)(FALSE);
    c = U(ShowCursor)(FALSE);
    RI_EQ(b, a - 1);
    RI_EQ(c, b - 1);
    RI_EQ(U(ShowCursor)(TRUE), b);
    RI_EQ(U(ShowCursor)(TRUE), a);
    U(ShowCursor)(FALSE);
}

TEST(resources_icons_ci_set_system_cursor_invalid)
{
    HCURSOR c;
    BOOL r;
    REQUIRE(U(SetSystemCursor)); REQUIRE(U(CopyIcon)); REQUIRE(U(LoadCursorW));
    /* a handle that is not a cursor must be rejected without touching any system cursor */
    SetLastError(0);
    {
        BOOL rb = U(SetSystemCursor)((HCURSOR)(ULONG_PTR)0x1234, OCR_UP);
        t_note("SetSystemCursor(bogus handle) = %d err=%lu", rb, GetLastError());
        if (rb) U(SystemParametersInfoW)(SPI_SETCURSORS, 0, NULL, 0);
    }
    c = U(CopyIcon)(U(LoadCursorW)(NULL, (LPCWSTR)IDC_ARROW));
    CHECK(c != NULL);
    SetLastError(0);
    r = U(SetSystemCursor)(c, 12345);
    t_note("SetSystemCursor(bad id) = %d err=%lu", r, GetLastError());
    if (r && U(SystemParametersInfoW)) U(SystemParametersInfoW)(SPI_SETCURSORS, 0, NULL, 0);
    else if (c) U(DestroyCursor)(c);
}

TEST(resources_icons_ci_set_system_cursor_replace_restore)
{
    HCURSOR c, before, after;
    BOOL ok;
    REQUIRE(U(SetSystemCursor)); REQUIRE(U(CopyIcon)); REQUIRE(U(LoadCursorW));
    REQUIRE(U(SystemParametersInfoW));
    before = U(LoadCursorW)(NULL, (LPCWSTR)IDC_UPARROW);
    c = U(CopyIcon)(before);
    CHECK(c != NULL);
    if (!c) return;
    ok = U(SetSystemCursor)(c, OCR_UP);   /* consumes c */
    CHECK(ok);
    after = U(LoadCursorW)(NULL, (LPCWSTR)IDC_UPARROW);
    CHECK(after != NULL);
    t_note("OCR_UP before=%p after=%p", before, after);
    /* restore the user's cursor scheme */
    CHECK(U(SystemParametersInfoW)(SPI_SETCURSORS, 0, NULL, 0));
    CHECK(U(LoadCursorW)(NULL, (LPCWSTR)IDC_UPARROW) != NULL);
}

/* ---- default layout --------------------------------------------------- */

TEST(resources_icons_dl_get_set_roundtrip)
{
    DWORD orig = 99, v = 77;
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout));
    CHECK(U(GetProcessDefaultLayout)(&orig));
    CHECK(orig == 0 || orig == LAYOUT_RTL);
    CHECK(U(SetProcessDefaultLayout)(LAYOUT_RTL));
    CHECK(U(GetProcessDefaultLayout)(&v));
    RI_EQ(v, LAYOUT_RTL);
    CHECK(U(SetProcessDefaultLayout)(0));
    CHECK(U(GetProcessDefaultLayout)(&v));
    RI_EQ(v, 0);
    CHECK(U(SetProcessDefaultLayout)(orig));
    CHECK(U(GetProcessDefaultLayout)(&v));
    RI_EQ(v, orig);
}

TEST(resources_icons_dl_idempotent)
{
    DWORD orig = 0, v = 0; int i;
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout));
    U(GetProcessDefaultLayout)(&orig);
    for (i = 0; i < 10; i++) {
        CHECK(U(SetProcessDefaultLayout)(LAYOUT_RTL));
        CHECK(U(GetProcessDefaultLayout)(&v) && v == LAYOUT_RTL);
    }
    U(SetProcessDefaultLayout)(orig);
}

TEST(resources_icons_dl_invalid_values_note)
{
    static const DWORD vals[] = {2, 3, 4, 8, 0x10, 0x100, 0x80000000, 0xFFFFFFFF};
    DWORD orig = 0, v = 0; int i;
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout));
    U(GetProcessDefaultLayout)(&orig);
    for (i = 0; i < 8; i++) {
        BOOL r;
        SetLastError(0);
        r = U(SetProcessDefaultLayout)(vals[i]);
        v = 0xDEAD;
        U(GetProcessDefaultLayout)(&v);
        t_note("SetProcessDefaultLayout(0x%lx) = %d err=%lu -> layout 0x%lx", vals[i], r, GetLastError(), v);
        U(SetProcessDefaultLayout)(orig);
    }
    U(GetProcessDefaultLayout)(&v);
    RI_EQ(v, orig);
}

TEST(resources_icons_dl_window_layout_note)
{
    DWORD orig = 0; HWND w; LONG ex;
    REQUIRE(U(GetProcessDefaultLayout)); REQUIRE(U(SetProcessDefaultLayout)); REQUIRE(U(GetWindowLongW)); REQUIRE(U(DestroyWindow));
    U(GetProcessDefaultLayout)(&orig);
    U(SetProcessDefaultLayout)(LAYOUT_RTL);
    w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (w) {
        ex = U(GetWindowLongW)(w, GWL_EXSTYLE);
        t_note("window created under RTL default layout: exstyle=0x%lx layoutrtl=%d", ex, !!(ex & WS_EX_LAYOUTRTL));
        U(DestroyWindow)(w);
    }
    U(SetProcessDefaultLayout)(0);
    w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    if (w) {
        ex = U(GetWindowLongW)(w, GWL_EXSTYLE);
        CHECK((ex & WS_EX_LAYOUTRTL) == 0);
        U(DestroyWindow)(w);
    }
    U(SetProcessDefaultLayout)(orig);
}

/* ---- GetGuiResources -------------------------------------------------- */

TEST(resources_icons_gr_basic)
{
    HBITMAP b; DWORD g0, g1, g2;
    REQUIRE(U(GetGuiResources));
    g0 = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    b = CreateBitmap(4, 4, 1, 1, NULL);
    g1 = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    RI_EQ(g1, g0 + 1);
    DeleteObject(b);
    g2 = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    RI_EQ(g2, g0);
}

TEST(resources_icons_gr_peak_ge_current)
{
    REQUIRE(U(GetGuiResources));
    CHECK(U(GetGuiResources)(GetCurrentProcess(), 2) >= U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS));
    CHECK(U(GetGuiResources)(GetCurrentProcess(), 4) >= U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS));
}

TEST(resources_icons_gr_window_counted)
{
    DWORD u0, u1, u2; HWND w;
    REQUIRE(U(GetGuiResources)); REQUIRE(U(DestroyWindow));
    u0 = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 50, 50, NULL);
    CHECK(w != NULL);
    u1 = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    /* the first window of a thread may also create a hidden IME window: only require growth */
    CHECK(u1 >= u0 + 1);
    if (w) U(DestroyWindow)(w);
    u2 = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    CHECK(u2 < u1);
}

TEST(resources_icons_gr_menu_counted)
{
    DWORD u0, u1, u2; HMENU m;
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateMenu)); REQUIRE(U(DestroyMenu));
    u0 = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    m = U(CreateMenu)();
    CHECK(m != NULL);
    u1 = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    RI_EQ(u1, u0 + 1);
    if (m) U(DestroyMenu)(m);
    u2 = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    RI_EQ(u2, u0);
}

TEST(resources_icons_gr_invalid_args)
{
    REQUIRE(U(GetGuiResources));
    SetLastError(0);
    RI_EQ(U(GetGuiResources)(NULL, GR_GDIOBJECTS), 0);
    t_note("GetGuiResources(NULL) err=%lu", GetLastError());
    RI_EQ(U(GetGuiResources)((HANDLE)(ULONG_PTR)0x12345678, GR_GDIOBJECTS), 0);
    SetLastError(0);
    RI_EQ(U(GetGuiResources)(GetCurrentProcess(), 3), 0);
    t_note("GetGuiResources(flag 3) err=%lu", GetLastError());
    RI_EQ(U(GetGuiResources)(GetCurrentProcess(), 0xFF), 0);
    RI_EQ(U(GetGuiResources)(GetCurrentProcess(), 0xFFFFFFFF), 0);
}

TEST(resources_icons_gr_cursor_shared_not_counted)
{
    DWORD u0, u1; int i;
    REQUIRE(U(GetGuiResources)); REQUIRE(U(LoadCursorW));
    U(LoadCursorW)(NULL, (LPCWSTR)IDC_ARROW);
    u0 = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    for (i = 0; i < 100; i++) U(LoadCursorW)(NULL, (LPCWSTR)IDC_ARROW);
    u1 = U(GetGuiResources)(GetCurrentProcess(), GR_USEROBJECTS);
    RI_EQ(u1, u0);
}

TEST(resources_icons_gr_destroy_icon_with_gdi_bitmaps)
{
    DWORD g0, g1, g2; HICON ic;
    REQUIRE(U(GetGuiResources)); REQUIRE(U(CreateIconIndirect));
    /* Win10 22H2 oracle: the first icon creation leaves 4 process-wide cached GDI objects behind
     * after DestroyIcon (GDI count 0 -> 4). Warm the caches first so the baseline is steady. */
    ic = ri_mkicon(32, 1);
    if (ic) U(DestroyIcon)(ic);
    g0 = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    ic = ri_mkicon(32, 1);
    g1 = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECK(ic != NULL);
    t_note("GDI objects: before %lu, with icon %lu", g0, g1);
    if (ic) U(DestroyIcon)(ic);
    g2 = U(GetGuiResources)(GetCurrentProcess(), GR_GDIOBJECTS);
    RI_EQ(g2, g0);
}
