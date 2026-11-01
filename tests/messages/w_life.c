/* Window creation / destruction / state-change message sequences and text, font, icon, enable messages. */
#include "mh.h"

#define MU(n) (WM_USER + (n))
#define NOERR 0xdeadbeef
#define SETE() SetLastError(NOERR)

static int seq_is(const UINT *want, int n) {
    int i;
    if (mh_nev != n) return 0;
    for (i = 0; i < n; i++) if (mh_ev[i].m != want[i]) return 0;
    return 1;
}
static int before(UINT a, UINT b) {
    int ia = mh_find(a, 0), ib = mh_find(b, 0);
    return ia >= 0 && ib >= 0 && ia < ib;
}
static void dump_if_bad(int ok) { if (!ok) mh_dump(); }

/* ---- creation ---- */
TEST(messages_create_hidden_overlapped_sequence) {
    HWND h; int ok;
    UINT want[] = { WM_NCCREATE, WM_NCCALCSIZE, WM_CREATE };
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1;
    h = mh_mk(WS_OVERLAPPEDWINDOW, 0, NULL); REQUIRE(h);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_NCCREATE), 1);
    EXPECT_EQ(mh_count(WM_NCCALCSIZE), 1);
    EXPECT_EQ(mh_count(WM_CREATE), 1);
    CHECK(before(WM_NCCREATE, WM_NCCALCSIZE));
    CHECK(before(WM_NCCALCSIZE, WM_CREATE));
    EXPECT_EQ(mh_count(WM_SHOWWINDOW), 0);
    EXPECT_EQ(mh_count(WM_SIZE), 0);
    EXPECT_EQ(mh_count(WM_MOVE), 0);
    if (mh_count(WM_GETMINMAXINFO)) CHECK(before(WM_GETMINMAXINFO, WM_NCCREATE));
    else t_note("no WM_GETMINMAXINFO during creation");
    ok = mh_nev >= 3 && mh_ev[mh_nev - 1].m == WM_CREATE;
    CHECK(ok); dump_if_bad(ok);
    (void)want;
    mh_kill(h);
}

TEST(messages_create_hidden_child_sequence) {
    HWND p, c;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    mh_rec = 1;
    c = mh_mk(WS_CHILD, 0, p); REQUIRE(c);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_NCCREATE), 1);
    EXPECT_EQ(mh_count(WM_CREATE), 1);
    CHECK(before(WM_NCCREATE, WM_NCCALCSIZE));
    CHECK(before(WM_NCCALCSIZE, WM_CREATE));
    EXPECT_EQ(mh_count(WM_SHOWWINDOW), 0);
    mh_kill(p);
}

TEST(messages_create_visible_popup_sequence) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1;
    h = CWEA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, MH_CLSA, "v", WS_POPUP | WS_VISIBLE, 10, 10, 100, 60, NULL, NULL, GetModuleHandleA(NULL), NULL);
    REQUIRE(h);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_NCCREATE), 1);
    EXPECT_EQ(mh_count(WM_CREATE), 1);
    EXPECT_EQ(mh_count(WM_SHOWWINDOW), 1);
    CHECK(before(WM_CREATE, WM_SHOWWINDOW));
    CHECK(before(WM_CREATE, WM_WINDOWPOSCHANGING));
    CHECK(before(WM_WINDOWPOSCHANGING, WM_WINDOWPOSCHANGED));
    CHECK(before(WM_CREATE, WM_SIZE));
    CHECK(before(WM_CREATE, WM_MOVE));
    {
        int k = mh_find(WM_SHOWWINDOW, 0);
        if (k >= 0) { EXPECT_EQ(mh_ev[k].w, 1); EXPECT_EQ(mh_ev[k].l, 0); }
    }
    CHECK(U(IsWindowVisible)(h));
    mh_kill(h);
}

static LRESULT hk_nccreate_false(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_NCCREATE) { *hd = 1; return FALSE; }
    return 0;
}
TEST(messages_create_nccreate_false_aborts) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1; mh_hook = hk_nccreate_false;
    SETE();
    h = mh_mk(0, 0, NULL);
    mh_hook = NULL; mh_rec = 0;
    CHECK(h == NULL);
    EXPECT_EQ(mh_count(WM_NCCREATE), 1);
    EXPECT_EQ(mh_count(WM_CREATE), 0);
    EXPECT_EQ(mh_count(WM_DESTROY), 0);
    EXPECT_EQ(mh_count(WM_NCDESTROY), 1);
    if (h) mh_kill(h);
}

static LRESULT hk_create_minus1(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_CREATE) { *hd = 1; return -1; }
    return 0;
}
TEST(messages_create_wm_create_minus1_aborts) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1; mh_hook = hk_create_minus1;
    h = mh_mk(0, 0, NULL);
    mh_hook = NULL; mh_rec = 0;
    CHECK(h == NULL);
    EXPECT_EQ(mh_count(WM_CREATE), 1);
    EXPECT_EQ(mh_count(WM_NCDESTROY), 1);
    if (mh_count(WM_DESTROY) != 1) t_note("WM_DESTROY count after WM_CREATE==-1: %d", mh_count(WM_DESTROY));
    CHECK(before(WM_CREATE, WM_NCDESTROY));
    if (h) mh_kill(h);
}

static LRESULT hk_create_other(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_CREATE) { *hd = 1; return 5; }   /* anything but -1 continues */
    return 0;
}
TEST(messages_create_wm_create_nonzero_continues) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_hook = hk_create_other;
    h = mh_mk(0, 0, NULL);
    mh_hook = NULL;
    CHECK(h != NULL);
    mh_kill(h);
}

static CREATESTRUCTA cs_nc, cs_cr; static int cs_have_nc, cs_have_cr;
static BYTE cs_name[128], cs_cls[128]; static int cs_cls_atom;
static void cs_copystr(BYTE *dst, const void *src, int wide) {
    size_t n = wide ? (lstrlenW(src) + 1) * 2 : (size_t)lstrlenA(src) + 1;
    if (n > 120) n = 120;
    memcpy(dst, src, n);
}
static void cs_save(CREATESTRUCTA *dst, LPARAM l, int wide) {
    const CREATESTRUCTA *s = (const CREATESTRUCTA *)l;
    *dst = *s;
    memset(cs_name, 0, sizeof cs_name); memset(cs_cls, 0, sizeof cs_cls);
    cs_cls_atom = IS_INTRESOURCE(s->lpszClass);
    if (s->lpszName) {
        cs_copystr(cs_name, s->lpszName, wide);
    }
    if (!cs_cls_atom && s->lpszClass) cs_copystr(cs_cls, s->lpszClass, wide);
}
static LRESULT hk_cs(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_NCCREATE) { cs_save(&cs_nc, l, U(IsWindowUnicode)(h)); cs_have_nc = 1; }
    if (m == WM_CREATE) { cs_save(&cs_cr, l, U(IsWindowUnicode)(h)); cs_have_cr = 1; }
    return 0;
}
TEST(messages_create_createstruct_fields) {
    HWND h; int tag = 0; HINSTANCE hi = GetModuleHandleA(NULL);
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); cs_have_nc = cs_have_cr = 0; mh_hook = hk_cs;
    h = CWEA(WS_EX_TOOLWINDOW, MH_CLSA, "title-x", WS_POPUP, 11, 22, 133, 44, NULL, NULL, hi, &tag);
    mh_hook = NULL;
    REQUIRE(h);
    CHECK(cs_have_nc && cs_have_cr);
    EXPECT_EQ(cs_nc.lpCreateParams, &tag);
    EXPECT_EQ(cs_cr.lpCreateParams, &tag);
    EXPECT_EQ(cs_cr.hInstance, hi);
    EXPECT_EQ(cs_cr.hwndParent, NULL);
    EXPECT_EQ(cs_cr.x, 11); EXPECT_EQ(cs_cr.y, 22); EXPECT_EQ(cs_cr.cx, 133); EXPECT_EQ(cs_cr.cy, 44);
    EXPECT_EQ(cs_nc.x, 11); EXPECT_EQ(cs_nc.cx, 133);
    CHECK((cs_cr.style & WS_POPUP) != 0);
    CHECK((cs_cr.dwExStyle & WS_EX_TOOLWINDOW) != 0);
    EXPECT_STR((char *)cs_name, "title-x");
    CHECK(!cs_cls_atom && !strcmp((char *)cs_cls, MH_CLSA));
    mh_kill(h);
}

TEST(messages_create_createstruct_child_id_and_parent) {
    HWND p, c;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    cs_have_cr = 0; mh_hook = hk_cs;
    c = CWEA(0, MH_CLSA, "kid", WS_CHILD, 1, 2, 30, 40, p, (HMENU)(ULONG_PTR)0x77, GetModuleHandleA(NULL), NULL);
    mh_hook = NULL;
    REQUIRE(c);
    EXPECT_EQ(cs_cr.hMenu, 0x77);
    EXPECT_EQ(cs_cr.hwndParent, p);
    EXPECT_EQ(cs_cr.cx, 30); EXPECT_EQ(cs_cr.cy, 40);
    EXPECT_EQ(U(GetDlgCtrlID)(c), 0x77);
    mh_kill(p);
}

TEST(messages_create_createstruct_wide_window_text_is_wide) {
    HWND h; static WCHAR got[32];
    REQUIRE(CWEW); REQUIRE_MH();
    mh_reset(); cs_have_cr = 0; mh_hook = hk_cs;
    h = CWEW(0, MH_CLSW, L"wide-title", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    mh_hook = NULL;
    REQUIRE(h);
    CHECK(cs_have_cr);
    if (cs_have_cr) { memcpy(got, cs_name, 11 * 2); got[11] = 0; EXPECT_WSTR(got, L"wide-title"); }
    mh_kill(h);
}

TEST(messages_create_ansi_title_to_wide_proc) {
    HWND h; static WCHAR got[32];
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); cs_have_cr = 0; mh_hook = hk_cs;
    h = CWEA(0, "MsgTestW", "ansi-title", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    mh_hook = NULL;
    if (!h) { t_skip("cannot create W class window with CreateWindowExA"); return; }
    if (cs_have_cr) { memcpy(got, cs_name, 11 * 2); got[11] = 0; EXPECT_WSTR(got, L"ansi-title"); }
    CHECK(U(IsWindowUnicode)(h));
    mh_kill(h);
}

/* ---- WM_SETTEXT family ---- */
TEST(messages_text_setwindowtext_sends_settext) {
    HWND h; int k;
    REQUIRE(U(SetWindowTextA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    CHECK(U(SetWindowTextA)(h, "hello") == TRUE);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_SETTEXT), 1);
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0 && !strcmp((char *)mh_ev[k].raw, "hello"));
    mh_kill(h);
}

TEST(messages_text_setwindowtextw_sends_wide_to_wide_proc) {
    HWND h; int k;
    REQUIRE(U(SetWindowTextW)); REQUIRE_MH();
    mh_reset();
    h = mh_mkw(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    CHECK(U(SetWindowTextW)(h, L"wide") == TRUE);
    mh_rec = 0;
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0 && !memcmp(mh_ev[k].raw, L"wide", 10));
    mh_kill(h);
}

TEST(messages_text_setwindowtexta_on_wide_proc_converted) {
    HWND h; int k;
    REQUIRE(U(SetWindowTextA)); REQUIRE_MH();
    mh_reset();
    h = mh_mkw(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    CHECK(U(SetWindowTextA)(h, "conv"));
    mh_rec = 0;
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0 && !memcmp(mh_ev[k].raw, L"conv", 10));
    mh_kill(h);
}

TEST(messages_text_getwindowtext_sends_gettext) {
    HWND h; char b[32];
    REQUIRE(U(GetWindowTextA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    EXPECT_EQ(U(GetWindowTextA)(h, b, 32), 2);
    EXPECT_STR(b, "mh");
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_GETTEXT), 1);
    mh_rec = 1;
    EXPECT_EQ(U(GetWindowTextLengthA)(h), 2);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_GETTEXTLENGTH), 1);
    mh_kill(h);
}

static LRESULT hk_gettext_custom(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_GETTEXT) { lstrcpynA((char *)l, "custom", (int)w); *hd = 1; return 6; }
    if (m == WM_GETTEXTLENGTH) { *hd = 1; return 6; }
    return 0;
}
TEST(messages_text_getwindowtext_uses_handler) {
    HWND h; char b[32];
    REQUIRE(U(GetWindowTextA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_hook = hk_gettext_custom;
    EXPECT_EQ(U(GetWindowTextA)(h, b, 32), 6);
    EXPECT_STR(b, "custom");
    EXPECT_EQ(U(GetWindowTextLengthA)(h), 6);
    mh_hook = NULL;
    mh_kill(h);
}

TEST(messages_text_getwindowtext_buffer_edges_a) {
    HWND h; char b[16];
    REQUIRE(U(GetWindowTextA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetWindowTextA)(h, "abcdef");
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetWindowTextA)(h, b, 0), 0); EXPECT_EQ(b[0], 'z');
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetWindowTextA)(h, b, 1), 0); EXPECT_EQ(b[0], 0);
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetWindowTextA)(h, b, 6), 5); EXPECT_STR(b, "abcde");
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetWindowTextA)(h, b, 7), 6); EXPECT_STR(b, "abcdef");
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetWindowTextA)(h, b, 16), 6); EXPECT_STR(b, "abcdef");
    EXPECT_EQ(b[7], 'z');
    EXPECT_EQ(U(GetWindowTextLengthA)(h), 6);
    mh_kill(h);
}

TEST(messages_text_getwindowtext_buffer_edges_w) {
    HWND h; WCHAR b[16]; int i;
    REQUIRE(U(GetWindowTextW)); REQUIRE_MH();
    mh_reset();
    h = mh_mkw(0, 0, NULL); REQUIRE(h);
    U(SetWindowTextW)(h, L"abcdef");
    for (i = 0; i < 16; i++) b[i] = 0x7a7a;
    EXPECT_EQ(U(GetWindowTextW)(h, b, 0), 0); EXPECT_EQ(b[0], 0x7a7a);
    EXPECT_EQ(U(GetWindowTextW)(h, b, 1), 0); EXPECT_EQ(b[0], 0);
    EXPECT_EQ(U(GetWindowTextW)(h, b, 6), 5); EXPECT_WSTR(b, L"abcde");
    EXPECT_EQ(U(GetWindowTextW)(h, b, 7), 6); EXPECT_WSTR(b, L"abcdef");
    EXPECT_EQ(U(GetWindowTextLengthW)(h), 6);
    mh_kill(h);
}

TEST(messages_text_empty_title) {
    HWND h; char b[8] = "zzzz"; LRESULT r;
    REQUIRE(U(GetWindowTextA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    CHECK(U(SetWindowTextA)(h, ""));
    EXPECT_EQ(U(GetWindowTextA)(h, b, 8), 0);
    EXPECT_EQ(b[0], 0);
    EXPECT_EQ(U(GetWindowTextLengthA)(h), 0);
    r = SMA(h, WM_GETTEXTLENGTH, 0, 0);
    EXPECT_EQ(r, 0);
    mh_kill(h);
}

TEST(messages_text_long_string_roundtrip) {
    HWND h; static char big[5000], out[5000]; LRESULT r;
    REQUIRE(U(SetWindowTextA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    memset(big, 'q', 4000); big[4000] = 0;
    CHECK(U(SetWindowTextA)(h, big));
    r = SMA(h, WM_GETTEXTLENGTH, 0, 0); EXPECT_EQ(r, 4000);
    r = SMA(h, WM_GETTEXT, 5000, (LPARAM)out); EXPECT_EQ(r, 4000);
    CHECK(!strcmp(out, big));
    r = SMA(h, WM_GETTEXT, 4000, (LPARAM)out); EXPECT_EQ(r, 3999);
    EXPECT_EQ(out[3999], 0);
    mh_kill(h);
}

TEST(messages_text_settext_return_true) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    EXPECT_EQ(SMA(h, WM_SETTEXT, 0, (LPARAM)"one"), TRUE);
    EXPECT_EQ(SMW(h, WM_SETTEXT, 0, (LPARAM)L"two"), TRUE);
    EXPECT_EQ(SMA(h, WM_SETTEXT, 99, (LPARAM)"wparam ignored"), TRUE);
    mh_kill(h);
}

TEST(messages_text_invalid_hwnd) {
    char b[8]; int r;
    REQUIRE(U(GetWindowTextA)); REQUIRE(U(SetWindowTextA)); REQUIRE(U(GetWindowTextLengthA));
    SETE(); r = U(GetWindowTextA)((HWND)0xDEADBEEF, b, 8);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    SETE(); r = U(GetWindowTextLengthA)((HWND)0xDEADBEEF);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    SETE(); r = U(SetWindowTextA)((HWND)0xDEADBEEF, "x");
    CHECK(r == 0);
    t_note("SetWindowText invalid hwnd error %lu", GetLastError());
}

TEST(messages_text_gettext_invalid_hwnd_zeroes_buffer) {
    WCHAR wb[4] = { 0x7a7a, 0x7a7a, 0x7a7a, 0 }; char ab[4] = "zzz"; int r;
    REQUIRE(U(GetWindowTextW)); REQUIRE(U(GetWindowTextA));
    SETE(); r = U(GetWindowTextW)((HWND)0xDEADBEEF, wb, 4);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    EXPECT_EQ(wb[0], 0);
    EXPECT_EQ(wb[1], 0x7a7a);
    SETE(); r = U(GetWindowTextA)((HWND)0xDEADBEEF, ab, 4);
    CHECK(r == 0); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    EXPECT_EQ(ab[0], 0);
    EXPECT_EQ(ab[1], 'z');
    wb[0] = 0x7a7a;
    r = U(GetWindowTextW)((HWND)0xDEADBEEF, wb, 0);
    CHECK(r == 0); EXPECT_EQ(wb[0], 0x7a7a);   /* nMaxCount == 0: buffer untouched */
    r = U(GetWindowTextW)((HWND)0xDEADBEEF, NULL, 4);
    CHECK(r == 0);
}

/* ---- WM_SETFONT / WM_GETFONT ---- */
TEST(messages_font_default_proc_ignores) {
    HWND h; HFONT f;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    f = GetStockObject(DEFAULT_GUI_FONT);
    EXPECT_EQ(SMA(h, WM_SETFONT, (WPARAM)f, TRUE), 0);
    EXPECT_EQ(SMA(h, WM_GETFONT, 0, 0), 0);
    mh_kill(h);
}

TEST(messages_font_builtin_controls_store_font) {
    static const char *cls[] = { "STATIC", "EDIT", "BUTTON", "LISTBOX", "COMBOBOX" };
    int i; HFONT f1 = GetStockObject(DEFAULT_GUI_FONT), f2 = GetStockObject(SYSTEM_FIXED_FONT);
    HWND p;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    for (i = 0; i < 5; i++) {
        HWND c = CWEA(0, cls[i], "c", WS_CHILD, 0, 0, 80, 60, p, NULL, GetModuleHandleA(NULL), NULL);
        if (!c) { t_note("cannot create %s", cls[i]); continue; }
        if (SMA(c, WM_GETFONT, 0, 0) != 0) t_note("%s initial WM_GETFONT nonzero", cls[i]);
        SMA(c, WM_SETFONT, (WPARAM)f1, FALSE);
        CHECKF((HFONT)SMA(c, WM_GETFONT, 0, 0) == f1, "%s font1", cls[i]);
        SMA(c, WM_SETFONT, (WPARAM)f2, TRUE);
        CHECKF((HFONT)SMA(c, WM_GETFONT, 0, 0) == f2, "%s font2", cls[i]);
        SMA(c, WM_SETFONT, 0, FALSE);
        if (SMA(c, WM_GETFONT, 0, 0) != 0) t_note("%s WM_GETFONT after WM_SETFONT(NULL) nonzero", cls[i]);
        DESTROYW(c);
    }
    mh_kill(p);
}

/* ---- WM_SETICON / WM_GETICON ---- */
TEST(messages_icon_set_get_small_big) {
    HWND h; HICON i1, i2, prev;
    REQUIRE(U(LoadIconW)); REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    i1 = U(LoadIconW)(NULL, (LPCWSTR)IDI_APPLICATION);
    i2 = U(LoadIconW)(NULL, (LPCWSTR)IDI_ERROR);
    REQUIRE(i1 && i2);
    EXPECT_EQ(SMA(h, WM_GETICON, ICON_BIG, 0), 0);
    EXPECT_EQ(SMA(h, WM_GETICON, ICON_SMALL, 0), 0);
    prev = (HICON)SMA(h, WM_SETICON, ICON_BIG, (LPARAM)i1);
    EXPECT_EQ(prev, 0);
    EXPECT_EQ(SMA(h, WM_GETICON, ICON_BIG, 0), i1);
    EXPECT_EQ(SMA(h, WM_GETICON, ICON_SMALL, 0), 0);
    prev = (HICON)SMA(h, WM_SETICON, ICON_SMALL, (LPARAM)i2);
    EXPECT_EQ(prev, 0);
    EXPECT_EQ(SMA(h, WM_GETICON, ICON_SMALL, 0), i2);
    EXPECT_EQ(SMA(h, WM_GETICON, ICON_BIG, 0), i1);
    prev = (HICON)SMA(h, WM_SETICON, ICON_BIG, (LPARAM)i2);
    EXPECT_EQ(prev, i1);
    prev = (HICON)SMA(h, WM_SETICON, ICON_BIG, 0);
    EXPECT_EQ(prev, i2);
    EXPECT_EQ(SMA(h, WM_GETICON, ICON_BIG, 0), 0);
    prev = (HICON)SMA(h, WM_SETICON, ICON_SMALL, 0);
    EXPECT_EQ(prev, i2);
    mh_kill(h);
}

TEST(messages_icon_a_w_parity) {
    HWND h; HICON i1;
    REQUIRE(U(LoadIconW)); REQUIRE_MH();
    mh_reset();
    h = mh_mkw(0, 0, NULL); REQUIRE(h);
    i1 = U(LoadIconW)(NULL, (LPCWSTR)IDI_WARNING); REQUIRE(i1);
    EXPECT_EQ(SMW(h, WM_SETICON, ICON_SMALL, (LPARAM)i1), 0);
    EXPECT_EQ(SMA(h, WM_GETICON, ICON_SMALL, 0), i1);
    EXPECT_EQ(SMW(h, WM_GETICON, ICON_SMALL, 0), i1);
    SMW(h, WM_SETICON, ICON_SMALL, 0);
    mh_kill(h);
}

/* ---- enable / disable ---- */
TEST(messages_enable_sequence_and_return) {
    HWND h; int k;
    REQUIRE(U(EnableWindow)); REQUIRE(U(IsWindowEnabled)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    CHECK(U(IsWindowEnabled)(h));
    mh_rec = 1;
    EXPECT_EQ(U(EnableWindow)(h, FALSE), 0);
    k = mh_find(WM_ENABLE, 0);
    CHECK(k >= 0 && mh_ev[k].w == 0);
    CHECK(!U(IsWindowEnabled)(h));
    CHECK((U(GetWindowLongA)(h, GWL_STYLE) & WS_DISABLED) != 0);
    EXPECT_EQ(mh_count(WM_ENABLE), 1);
    EXPECT_NE(U(EnableWindow)(h, FALSE), 0);        /* already disabled */
    EXPECT_EQ(mh_count(WM_ENABLE), 1);
    EXPECT_NE(U(EnableWindow)(h, TRUE), 0);         /* was disabled */
    EXPECT_EQ(mh_count(WM_ENABLE), 2);
    k = mh_find(WM_ENABLE, k + 1);
    CHECK(k >= 0 && mh_ev[k].w == 1);
    EXPECT_EQ(U(EnableWindow)(h, TRUE), 0);         /* already enabled */
    EXPECT_EQ(mh_count(WM_ENABLE), 2);
    mh_rec = 0;
    CHECK(U(IsWindowEnabled)(h));
    CHECK((U(GetWindowLongA)(h, GWL_STYLE) & WS_DISABLED) == 0);
    mh_kill(h);
}

TEST(messages_enable_nonzero_bool_values) {
    HWND h;
    REQUIRE(U(EnableWindow)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(EnableWindow)(h, 0);
    CHECK(!U(IsWindowEnabled)(h));
    U(EnableWindow)(h, 0x12345);   /* BOOL: any nonzero is documented as TRUE; the exact handling is observed only */
    if (!U(IsWindowEnabled)(h)) t_note("EnableWindow(h, 0x12345) did not enable");
    U(EnableWindow)(h, 0);
    U(EnableWindow)(h, -1);
    if (!U(IsWindowEnabled)(h)) t_note("EnableWindow(h, -1) did not enable");
    U(EnableWindow)(h, TRUE);
    CHECK(U(IsWindowEnabled)(h));
    mh_kill(h);
}

TEST(messages_enable_created_disabled_no_message) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1;
    h = mh_mk(WS_DISABLED, 0, NULL); REQUIRE(h);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_ENABLE), 0);
    CHECK(!U(IsWindowEnabled)(h));
    mh_kill(h);
}

TEST(messages_enable_parent_disables_child_effective) {
    HWND p, c;
    REQUIRE(U(EnableWindow)); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    c = mh_mk(WS_CHILD, 0, p); REQUIRE(c);
    U(EnableWindow)(p, FALSE);
    CHECK(!U(IsWindowEnabled)(p));
    CHECK(U(IsWindowEnabled)(c));            /* IsWindowEnabled only tests the window's own WS_DISABLED */
    CHECK((U(GetWindowLongA)(c, GWL_STYLE) & WS_DISABLED) == 0);
    U(EnableWindow)(p, TRUE);
    CHECK(U(IsWindowEnabled)(c));
    mh_kill(p);
}

TEST(messages_enable_invalid_hwnd) {
    BOOL r;
    REQUIRE(U(EnableWindow));
    SETE(); r = U(EnableWindow)((HWND)0xDEADBEEF, FALSE);
    CHECK(r == FALSE);
    EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    SETE(); r = U(IsWindowEnabled)((HWND)0xDEADBEEF);
    CHECK(r == FALSE);
}

/* ---- show / hide ---- */
TEST(messages_show_hide_messages) {
    HWND h; int k;
    REQUIRE(U(ShowWindow)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, NULL); REQUIRE(h);
    mh_rec = 1;
    EXPECT_EQ(U(ShowWindow)(h, SW_SHOWNA), 0);
    k = mh_find(WM_SHOWWINDOW, 0);
    CHECK(k >= 0);
    if (k >= 0) { EXPECT_EQ(mh_ev[k].w, 1); EXPECT_EQ(mh_ev[k].l, 0); }
    CHECK(before(WM_SHOWWINDOW, WM_WINDOWPOSCHANGED));
    CHECK(U(IsWindowVisible)(h));
    mh_clear();
    EXPECT_NE(U(ShowWindow)(h, SW_HIDE), 0);
    k = mh_find(WM_SHOWWINDOW, 0);
    CHECK(k >= 0);
    if (k >= 0) { EXPECT_EQ(mh_ev[k].w, 0); EXPECT_EQ(mh_ev[k].l, 0); }
    CHECK(!U(IsWindowVisible)(h));
    mh_clear();
    EXPECT_EQ(U(ShowWindow)(h, SW_HIDE), 0);
    EXPECT_EQ(mh_count(WM_SHOWWINDOW), 0);
    mh_rec = 0;
    mh_kill(h);
}

TEST(messages_show_repeat_show_no_message) {
    HWND h;
    REQUIRE(U(ShowWindow)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, NULL); REQUIRE(h);
    U(ShowWindow)(h, SW_SHOWNA);
    mh_rec = 1; mh_clear();
    EXPECT_NE(U(ShowWindow)(h, SW_SHOWNA), 0);
    /* docs: no change of visibility, so no WM_SHOWWINDOW; Win10 22H2 delivered one for a repeated
     * SW_SHOWNA on this no-activate popup (oracle run). Record rather than assert. */
    t_note("repeat SW_SHOWNA delivered %d WM_SHOWWINDOW", mh_count(WM_SHOWWINDOW));
    CHECK(mh_count(WM_SHOWWINDOW) <= 1);
    mh_rec = 0;
    mh_kill(h);
}

TEST(messages_show_child_parent_hide_notification) {
    HWND p, c; int k, found = 0;
    REQUIRE(U(ShowWindow)); REQUIRE_MH();
    mh_reset();
    p = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, NULL); REQUIRE(p);
    c = mh_mk(WS_CHILD | WS_VISIBLE, 0, p); REQUIRE(c);
    U(ShowWindow)(p, SW_SHOWNA);
    mh_rec = 1; mh_clear();
    U(ShowWindow)(p, SW_HIDE);
    for (k = 0; k < mh_nev; k++) if (mh_ev[k].m == WM_SHOWWINDOW && mh_ev[k].h == c) {
        found++;
        EXPECT_EQ(mh_ev[k].w, 0);
        t_note("child WM_SHOWWINDOW lParam on parent hide = %llu", (unsigned long long)mh_ev[k].l);
    }
    mh_rec = 0;
    if (found != 1) t_note("child saw %d WM_SHOWWINDOW on parent hide", found);
    mh_kill(p);
}

TEST(messages_show_returns_previous_visibility) {
    HWND h;
    REQUIRE(U(ShowWindow)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, NULL); REQUIRE(h);
    EXPECT_EQ(U(ShowWindow)(h, SW_HIDE), 0);
    EXPECT_EQ(U(ShowWindow)(h, SW_SHOWNA), 0);
    EXPECT_NE(U(ShowWindow)(h, SW_SHOWNA), 0);
    EXPECT_NE(U(ShowWindow)(h, SW_HIDE), 0);
    mh_kill(h);
}

/* ---- WM_WINDOWPOSCHANGING / CHANGED / SIZE / MOVE ---- */
static WINDOWPOS wp_ing, wp_ed;
static LRESULT hk_wpos(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_WINDOWPOSCHANGING) wp_ing = *(WINDOWPOS *)l;
    if (m == WM_WINDOWPOSCHANGED) wp_ed = *(WINDOWPOS *)l;
    return 0;
}
TEST(messages_pos_setwindowpos_sequence_and_fields) {
    HWND h; int ki, ke, ks, km; RECT rc, cr; POINT pt;
    REQUIRE(U(SetWindowPos)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    memset(&wp_ing, 0, sizeof wp_ing); memset(&wp_ed, 0, sizeof wp_ed);
    mh_rec = 1; mh_hook = hk_wpos;
    CHECK(U(SetWindowPos)(h, NULL, 33, 44, 150, 90, SWP_NOZORDER | SWP_NOACTIVATE));
    mh_hook = NULL; mh_rec = 0;
    ki = mh_find(WM_WINDOWPOSCHANGING, 0); ke = mh_find(WM_WINDOWPOSCHANGED, 0);
    ks = mh_find(WM_SIZE, 0); km = mh_find(WM_MOVE, 0);
    CHECK(ki >= 0 && ke > ki);
    CHECK(ks > ke); CHECK(km > ke);
    EXPECT_EQ(wp_ing.x, 33); EXPECT_EQ(wp_ing.y, 44); EXPECT_EQ(wp_ing.cx, 150); EXPECT_EQ(wp_ing.cy, 90);
    CHECK((wp_ing.flags & SWP_NOZORDER) != 0);
    CHECK((wp_ing.flags & SWP_NOACTIVATE) != 0);
    EXPECT_EQ(wp_ed.x, 33); EXPECT_EQ(wp_ed.y, 44); EXPECT_EQ(wp_ed.cx, 150); EXPECT_EQ(wp_ed.cy, 90);
    EXPECT_EQ(wp_ed.hwnd, h);
    U(GetWindowRect)(h, &rc);
    EXPECT_EQ(rc.left, 33); EXPECT_EQ(rc.top, 44); EXPECT_EQ(rc.right - rc.left, 150); EXPECT_EQ(rc.bottom - rc.top, 90);
    U(GetClientRect)(h, &cr);
    if (ks >= 0) {
        EXPECT_EQ(mh_ev[ks].w, SIZE_RESTORED);
        EXPECT_EQ(LOWORD(mh_ev[ks].l), cr.right);
        EXPECT_EQ(HIWORD(mh_ev[ks].l), cr.bottom);
    }
    pt.x = 0; pt.y = 0; U(ClientToScreen)(h, &pt);
    if (km >= 0) {
        EXPECT_EQ((short)LOWORD(mh_ev[km].l), pt.x);
        EXPECT_EQ((short)HIWORD(mh_ev[km].l), pt.y);
    }
    mh_kill(h);
}

TEST(messages_pos_move_only_no_size) {
    HWND h;
    REQUIRE(U(SetWindowPos)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    U(SetWindowPos)(h, NULL, 50, 50, 100, 100, SWP_NOZORDER | SWP_NOACTIVATE);
    mh_rec = 1; mh_clear();
    CHECK(U(SetWindowPos)(h, NULL, 70, 80, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE));
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_MOVE), 1);
    EXPECT_EQ(mh_count(WM_SIZE), 0);
    mh_kill(h);
}

TEST(messages_pos_size_only_no_move) {
    HWND h;
    REQUIRE(U(SetWindowPos)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    U(SetWindowPos)(h, NULL, 50, 50, 100, 100, SWP_NOZORDER | SWP_NOACTIVATE);
    mh_rec = 1; mh_clear();
    CHECK(U(SetWindowPos)(h, NULL, 0, 0, 120, 110, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE));
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_MOVE), 0);
    EXPECT_EQ(mh_count(WM_SIZE), 1);
    mh_kill(h);
}

TEST(messages_pos_no_change_sends_no_size_move) {
    HWND h;
    REQUIRE(U(SetWindowPos)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    U(SetWindowPos)(h, NULL, 50, 50, 100, 100, SWP_NOZORDER | SWP_NOACTIVATE);
    mh_rec = 1; mh_clear();
    CHECK(U(SetWindowPos)(h, NULL, 50, 50, 100, 100, SWP_NOZORDER | SWP_NOACTIVATE));
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_MOVE), 0);
    EXPECT_EQ(mh_count(WM_SIZE), 0);
    mh_kill(h);
}

static LRESULT hk_widen(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_WINDOWPOSCHANGING) { WINDOWPOS *p = (WINDOWPOS *)l; if (!(p->flags & SWP_NOSIZE)) p->cx = 222; }
    return 0;
}
TEST(messages_pos_changing_can_modify_size) {
    HWND h; RECT rc;
    REQUIRE(U(SetWindowPos)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    mh_hook = hk_widen;
    U(SetWindowPos)(h, NULL, 20, 20, 100, 70, SWP_NOZORDER | SWP_NOACTIVATE);
    mh_hook = NULL;
    U(GetWindowRect)(h, &rc);
    EXPECT_EQ(rc.right - rc.left, 222);
    EXPECT_EQ(rc.bottom - rc.top, 70);
    mh_kill(h);
}

static LRESULT hk_nomove(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_WINDOWPOSCHANGING) { WINDOWPOS *p = (WINDOWPOS *)l; p->flags |= SWP_NOMOVE; }
    return 0;
}
TEST(messages_pos_changing_can_veto_move) {
    HWND h; RECT rc;
    REQUIRE(U(SetWindowPos)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    U(SetWindowPos)(h, NULL, 40, 40, 100, 70, SWP_NOZORDER | SWP_NOACTIVATE);
    mh_hook = hk_nomove;
    U(SetWindowPos)(h, NULL, 300, 300, 100, 70, SWP_NOZORDER | SWP_NOACTIVATE);
    mh_hook = NULL;
    U(GetWindowRect)(h, &rc);
    EXPECT_EQ(rc.left, 40); EXPECT_EQ(rc.top, 40);
    mh_kill(h);
}

TEST(messages_pos_movewindow_sends_messages) {
    HWND h; RECT rc;
    REQUIRE(U(MoveWindow)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    mh_rec = 1; mh_clear();
    CHECK(U(MoveWindow)(h, 60, 70, 130, 95, FALSE));
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_WINDOWPOSCHANGING), 1);
    EXPECT_EQ(mh_count(WM_WINDOWPOSCHANGED), 1);
    EXPECT_EQ(mh_count(WM_MOVE), 1);
    EXPECT_EQ(mh_count(WM_SIZE), 1);
    U(GetWindowRect)(h, &rc);
    EXPECT_EQ(rc.left, 60); EXPECT_EQ(rc.right, 190); EXPECT_EQ(rc.bottom, 165);
    mh_kill(h);
}

TEST(messages_pos_child_move_is_parent_relative) {
    HWND p, c; int km; 
    REQUIRE(U(MoveWindow)); REQUIRE_MH();
    mh_reset();
    p = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(p);
    c = mh_mk(WS_CHILD, 0, p); REQUIRE(c);
    U(MoveWindow)(p, 200, 200, 300, 200, FALSE);
    mh_rec = 1; mh_clear();
    U(MoveWindow)(c, 17, 23, 50, 40, FALSE);
    mh_rec = 0;
    km = mh_find(WM_MOVE, 0);
    CHECK(km >= 0);
    if (km >= 0) { EXPECT_EQ((short)LOWORD(mh_ev[km].l), 17); EXPECT_EQ((short)HIWORD(mh_ev[km].l), 23); }
    mh_kill(p);
}

static LRESULT hk_nccalc_shrink(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_NCCALCSIZE) {
        /* Win10 22H2 sends the creation-time WM_NCCALCSIZE with wParam FALSE (lParam is a RECT *),
         * so handle both forms. */
        RECT *r = w ? &((NCCALCSIZE_PARAMS *)l)->rgrc[0] : (RECT *)l;
        r->left += 7; r->top += 5; r->right -= 3; r->bottom -= 2;
        *hd = 1; return 0;
    }
    return 0;
}
TEST(messages_nccalcsize_custom_client_area) {
    HWND h; RECT cr; 
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_hook = hk_nccalc_shrink;
    h = CWEA(WS_EX_TOOLWINDOW, MH_CLSA, "n", WS_POPUP, 10, 10, 100, 60, NULL, NULL, GetModuleHandleA(NULL), NULL);
    mh_hook = NULL;
    REQUIRE(h);
    U(GetClientRect)(h, &cr);
    EXPECT_EQ(cr.left, 0); EXPECT_EQ(cr.top, 0);
    EXPECT_EQ(cr.right, 100 - 7 - 3);
    EXPECT_EQ(cr.bottom, 60 - 5 - 2);
    { POINT pt = {0, 0}; U(ClientToScreen)(h, &pt); EXPECT_EQ(pt.x, 17); EXPECT_EQ(pt.y, 15); }
    mh_kill(h);
}

static RECT nc_in; static WPARAM nc_w;
static LRESULT hk_nccalc_spy(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_NCCALCSIZE && !nc_w) { nc_w = w + 1; nc_in = w ? ((NCCALCSIZE_PARAMS *)l)->rgrc[0] : *(RECT *)l; }
    return 0;
}
TEST(messages_nccalcsize_input_rect_is_window_rect) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); nc_w = 0; memset(&nc_in, 0, sizeof nc_in); mh_hook = hk_nccalc_spy;
    h = CWEA(WS_EX_TOOLWINDOW, MH_CLSA, "n", WS_POPUP, 31, 41, 110, 66, NULL, NULL, GetModuleHandleA(NULL), NULL);
    mh_hook = NULL;
    REQUIRE(h);
    /* first WM_NCCALCSIZE of a creation: TRUE on older systems, FALSE (plain RECT) on Win10 22H2 */
    t_note("first WM_NCCALCSIZE wParam=%d", (int)(nc_w - 1));
    CHECK(nc_w == 1 || nc_w == 2);
    EXPECT_EQ(nc_in.left, 31); EXPECT_EQ(nc_in.top, 41);
    EXPECT_EQ(nc_in.right, 141); EXPECT_EQ(nc_in.bottom, 107);
    mh_kill(h);
}

/* ---- destroy ---- */
static int in_destroy_valid, in_ncdestroy_valid;
static LRESULT hk_valid(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_DESTROY) in_destroy_valid = ISWND(h) ? 1 : 0;
    if (m == WM_NCDESTROY) in_ncdestroy_valid = ISWND(h) ? 1 : 0;
    return 0;
}
TEST(messages_destroy_sequence_visible_popup) {
    HWND h; BOOL r;
    REQUIRE(DESTROYW); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, NULL); REQUIRE(h);
    mh_rec = 1; mh_clear(); mh_hook = hk_valid; in_destroy_valid = in_ncdestroy_valid = -1;
    r = DESTROYW(h);
    mh_hook = NULL; mh_rec = 0;
    CHECK(r == TRUE);
    EXPECT_EQ(mh_count(WM_DESTROY), 1);
    EXPECT_EQ(mh_count(WM_NCDESTROY), 1);
    CHECK(before(WM_DESTROY, WM_NCDESTROY));
    CHECK(mh_nev > 0 && mh_ev[mh_nev - 1].m == WM_NCDESTROY);
    EXPECT_EQ(in_destroy_valid, 1);
    t_note("IsWindow inside WM_NCDESTROY = %d", in_ncdestroy_valid);
    CHECK(!ISWND(h));
}

TEST(messages_destroy_parent_child_order) {
    HWND p, c, g; int ip, ic, ig, ncp, ncc, ncg;
    REQUIRE(DESTROYW); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    c = mh_mk(WS_CHILD, 0, p); g = mh_mk(WS_CHILD, 0, c); REQUIRE(c && g);
    mh_rec = 1; mh_clear();
    DESTROYW(p);
    mh_rec = 0;
    ip = ic = ig = ncp = ncc = ncg = -1;
    { int k; for (k = 0; k < mh_nev; k++) {
        if (mh_ev[k].m == WM_DESTROY) { if (mh_ev[k].h == p) ip = k; if (mh_ev[k].h == c) ic = k; if (mh_ev[k].h == g) ig = k; }
        if (mh_ev[k].m == WM_NCDESTROY) { if (mh_ev[k].h == p) ncp = k; if (mh_ev[k].h == c) ncc = k; if (mh_ev[k].h == g) ncg = k; }
    } }
    CHECK(ip >= 0 && ic >= 0 && ig >= 0 && ncp >= 0 && ncc >= 0 && ncg >= 0);
    CHECK(ip < ic); CHECK(ic < ig);
    CHECK(ig < ncp); CHECK(ic < ncp);
    CHECK(ncp > ncc && ncp > ncg);
    CHECK(!ISWND(c)); CHECK(!ISWND(g)); CHECK(!ISWND(p));
}

TEST(messages_destroy_twice_fails) {
    HWND h; BOOL r;
    REQUIRE(DESTROYW); REQUIRE_MH();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    CHECK(DESTROYW(h) == TRUE);
    SETE(); r = DESTROYW(h);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(messages_destroy_invalid_and_null) {
    BOOL r;
    REQUIRE(DESTROYW);
    SETE(); r = DESTROYW((HWND)0xDEADBEEF);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
    SETE(); r = DESTROYW(NULL);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(messages_destroy_desktop_denied) {
    BOOL r;
    REQUIRE(DESTROYW); REQUIRE(U(GetDesktopWindow));
    SETE(); r = DESTROYW(U(GetDesktopWindow)());
    CHECK(r == FALSE);
    t_note("DestroyWindow(desktop) error %lu", GetLastError());
}

TEST(messages_destroy_other_thread_window_denied) {
    MH_TH t; BOOL r;
    REQUIRE(DESTROYW); REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    SETE(); r = DESTROYW(t.hwnd);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_ACCESS_DENIED);
    CHECK(ISWND(t.hwnd));
    mh_th_stop(&t);
}

TEST(messages_destroy_handle_invalid_after) {
    HWND h; char b[8];
    REQUIRE(DESTROYW); REQUIRE_MH();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    DESTROYW(h);
    CHECK(!ISWND(h));
    SETE();
    EXPECT_EQ(U(GetWindowTextA)(h, b, 8), 0);
    EXPECT_ERR(0, ERROR_INVALID_WINDOW_HANDLE);
    CHECK(!U(IsWindowVisible)(h));
    CHECK(!U(IsWindowEnabled)(h));
    EXPECT_EQ(U(GetParent)(h), 0);
}

TEST(messages_destroy_nonrecursive_in_wm_destroy) {
    HWND h;
    REQUIRE(DESTROYW); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    DESTROYW(h);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_DESTROY), 1);
    EXPECT_EQ(mh_count(WM_NCDESTROY), 1);
}

/* ---- WM_CLOSE ---- */
TEST(messages_close_default_destroys) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    EXPECT_EQ(SMA(h, WM_CLOSE, 0, 0), 0);
    mh_rec = 0;
    CHECK(!ISWND(h));
    EXPECT_EQ(mh_count(WM_CLOSE), 1);
    EXPECT_EQ(mh_count(WM_DESTROY), 1);
    CHECK(before(WM_CLOSE, WM_DESTROY));
}

static LRESULT hk_close_veto(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_CLOSE) { *hd = 1; return 0; }
    return 0;
}
TEST(messages_close_handler_can_veto) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_hook = hk_close_veto;
    SMA(h, WM_CLOSE, 0, 0);
    PMA(h, WM_CLOSE, 0, 0); mh_drain();
    mh_hook = NULL;
    CHECK(ISWND(h));
    mh_kill(h);
}

TEST(messages_close_posted_default_destroys) {
    HWND h;
    REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    CHECK(PMA(h, WM_CLOSE, 0, 0));
    CHECK(ISWND(h));
    mh_drain();
    CHECK(!ISWND(h));
}

TEST(messages_close_wm_syscommand_close_gives_wm_close) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_OVERLAPPEDWINDOW, 0, NULL); REQUIRE(h);
    mh_rec = 1; mh_hook = hk_close_veto;
    SMA(h, WM_SYSCOMMAND, SC_CLOSE, 0);
    mh_drain();                       /* WM_CLOSE may be sent or posted */
    mh_hook = NULL; mh_rec = 0;
    EXPECT_EQ(mh_count(WM_CLOSE), 1);
    CHECK(ISWND(h));
    mh_kill(h);
}

/* ---- WM_PARENTNOTIFY ---- */
TEST(messages_parentnotify_on_child_create_destroy) {
    HWND p, c; int k;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    mh_rec = 1; mh_clear();
    c = CWEA(0, MH_CLSA, "k", WS_CHILD, 0, 0, 10, 10, p, (HMENU)(ULONG_PTR)0x1234, GetModuleHandleA(NULL), NULL);
    REQUIRE(c);
    k = -1;
    { int i; for (i = 0; i < mh_nev; i++) if (mh_ev[i].m == WM_PARENTNOTIFY && mh_ev[i].h == p) { k = i; break; } }
    CHECK(k >= 0);
    if (k >= 0) {
        EXPECT_EQ(LOWORD(mh_ev[k].w), WM_CREATE);
        EXPECT_EQ(HIWORD(mh_ev[k].w), 0x1234);
        EXPECT_EQ(mh_ev[k].l, c);
    }
    mh_clear();
    DESTROYW(c);
    k = -1;
    { int i; for (i = 0; i < mh_nev; i++) if (mh_ev[i].m == WM_PARENTNOTIFY && mh_ev[i].h == p) { k = i; break; } }
    CHECK(k >= 0);
    if (k >= 0) { EXPECT_EQ(LOWORD(mh_ev[k].w), WM_DESTROY); EXPECT_EQ(HIWORD(mh_ev[k].w), 0x1234); }
    mh_rec = 0;
    mh_kill(p);
}

TEST(messages_parentnotify_suppressed_by_exstyle) {
    HWND p, c; int i, n = 0;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    mh_rec = 1; mh_clear();
    c = mh_mk(WS_CHILD, WS_EX_NOPARENTNOTIFY, p); REQUIRE(c);
    DESTROYW(c);
    for (i = 0; i < mh_nev; i++) if (mh_ev[i].m == WM_PARENTNOTIFY) n++;
    mh_rec = 0;
    EXPECT_EQ(n, 0);
    mh_kill(p);
}

/* ---- style change messages ---- */
static STYLESTRUCT ss_ing, ss_ed; static WPARAM ss_w;
static LRESULT hk_style(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_STYLECHANGING) { ss_ing = *(STYLESTRUCT *)l; ss_w = w; }
    if (m == WM_STYLECHANGED) ss_ed = *(STYLESTRUCT *)l;
    return 0;
}
TEST(messages_style_changing_changed) {
    HWND h; LONG old;
    REQUIRE(U(SetWindowLongA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    old = U(GetWindowLongA)(h, GWL_STYLE);
    memset(&ss_ing, 0, sizeof ss_ing); memset(&ss_ed, 0, sizeof ss_ed);
    mh_rec = 1; mh_hook = hk_style;
    U(SetWindowLongA)(h, GWL_STYLE, old | WS_DISABLED);
    mh_hook = NULL; mh_rec = 0;
    EXPECT_EQ(mh_count(WM_STYLECHANGING), 1);
    EXPECT_EQ(mh_count(WM_STYLECHANGED), 1);
    CHECK(before(WM_STYLECHANGING, WM_STYLECHANGED));
    EXPECT_EQ(ss_w, (WPARAM)(LONG)GWL_STYLE);
    EXPECT_EQ(ss_ing.styleOld, (DWORD)old);
    EXPECT_EQ(ss_ing.styleNew, (DWORD)(old | WS_DISABLED));
    EXPECT_EQ(ss_ed.styleNew, (DWORD)(old | WS_DISABLED));
    mh_kill(h);
}

static LRESULT hk_style_override(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_STYLECHANGING && w == (WPARAM)(LONG)GWL_STYLE) ((STYLESTRUCT *)l)->styleNew &= ~WS_DISABLED;
    return 0;
}
TEST(messages_style_changing_handler_can_override) {
    HWND h;
    REQUIRE(U(SetWindowLongA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(WS_POPUP, WS_EX_TOOLWINDOW, NULL); REQUIRE(h);
    mh_hook = hk_style_override;
    U(SetWindowLongA)(h, GWL_STYLE, U(GetWindowLongA)(h, GWL_STYLE) | WS_DISABLED);
    mh_hook = NULL;
    CHECK((U(GetWindowLongA)(h, GWL_STYLE) & WS_DISABLED) == 0);
    mh_kill(h);
}

/* ---- focus / activation (conditional on the desktop allowing it) ---- */
TEST(messages_focus_set_focus_messages) {
    HWND a, b; int ka, kb;
    REQUIRE(U(SetFocus)); REQUIRE(U(GetFocus)); REQUIRE_MH();
    mh_reset();
    a = mh_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW, NULL);
    b = mh_mk(WS_CHILD | WS_VISIBLE, 0, a);
    REQUIRE(a && b);
    mh_drain();
    U(SetFocus)(a);
    if (U(GetFocus)() != a) { t_note("SetFocus on top-level did not take effect (focus %p)", U(GetFocus)()); mh_kill(a); return; }
    mh_rec = 1; mh_clear();
    U(SetFocus)(b);
    mh_rec = 0;
    if (U(GetFocus)() != b) { t_note("SetFocus on child did not take effect"); mh_kill(a); return; }
    ka = -1; kb = -1;
    { int i; for (i = 0; i < mh_nev; i++) {
        if (mh_ev[i].m == WM_KILLFOCUS && mh_ev[i].h == a) ka = i;
        if (mh_ev[i].m == WM_SETFOCUS && mh_ev[i].h == b) kb = i;
    } }
    CHECK(ka >= 0 && kb > ka);
    if (ka >= 0) EXPECT_EQ(mh_ev[ka].w, b);
    if (kb >= 0) EXPECT_EQ(mh_ev[kb].w, a);
    mh_clear(); mh_rec = 1;
    U(SetFocus)(b);   /* already focused: no messages */
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_SETFOCUS), 0);
    EXPECT_EQ(mh_count(WM_KILLFOCUS), 0);
    mh_kill(a);
}

TEST(messages_focus_setfocus_returns_previous) {
    HWND a, b, r;
    REQUIRE(U(SetFocus)); REQUIRE(U(GetFocus)); REQUIRE_MH();
    mh_reset();
    a = mh_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW, NULL);
    b = mh_mk(WS_CHILD | WS_VISIBLE, 0, a);
    REQUIRE(a && b);
    U(SetFocus)(a);
    if (U(GetFocus)() != a) { t_note("no focus on this desktop"); mh_kill(a); return; }
    r = U(SetFocus)(b);
    if (U(GetFocus)() == b) EXPECT_EQ(r, a);
    mh_kill(a);
}

TEST(messages_focus_setfocus_invalid_window) {
    HWND r;
    REQUIRE(U(SetFocus));
    SETE(); r = U(SetFocus)((HWND)0xDEADBEEF);
    CHECK(r == NULL);
    EXPECT_ERR(r, ERROR_INVALID_WINDOW_HANDLE);
}

TEST(messages_focus_destroy_focus_window_sends_killfocus) {
    HWND a; 
    REQUIRE(U(SetFocus)); REQUIRE_MH();
    mh_reset();
    a = mh_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW, NULL); REQUIRE(a);
    U(SetFocus)(a);
    if (U(GetFocus)() != a) { t_note("no focus on this desktop"); mh_kill(a); return; }
    mh_rec = 1; mh_clear();
    DESTROYW(a);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_KILLFOCUS), 1);
    CHECK(before(WM_KILLFOCUS, WM_DESTROY));
    CHECK(U(GetFocus)() != a);
}

TEST(messages_activate_setactivewindow_messages) {
    HWND a, b; int i, wa = -1; 
    REQUIRE(U(SetActiveWindow)); REQUIRE(U(GetActiveWindow)); REQUIRE_MH();
    mh_reset();
    a = mh_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW, NULL);
    b = mh_mk(WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW, NULL);
    REQUIRE(a && b);
    mh_drain();
    mh_rec = 1; mh_clear();
    U(SetActiveWindow)(a);
    mh_rec = 0;
    if (U(GetActiveWindow)() != a) { t_note("SetActiveWindow(a) had no effect (active %p)", U(GetActiveWindow)()); mh_kill(a); mh_kill(b); return; }
    mh_clear(); mh_rec = 1;
    U(SetActiveWindow)(b);
    mh_rec = 0;
    if (U(GetActiveWindow)() != b) { t_note("SetActiveWindow(b) had no effect"); mh_kill(a); mh_kill(b); return; }
    {
        int ina = -1, acta = -1, actb = -1, nca = -1;
        for (i = 0; i < mh_nev; i++) {
            if (mh_ev[i].m == WM_ACTIVATE && mh_ev[i].h == a) { acta = i; EXPECT_EQ(LOWORD(mh_ev[i].w), WA_INACTIVE); EXPECT_EQ(mh_ev[i].l, b); }
            if (mh_ev[i].m == WM_ACTIVATE && mh_ev[i].h == b) { actb = i; EXPECT_EQ(LOWORD(mh_ev[i].w), WA_ACTIVE); EXPECT_EQ(mh_ev[i].l, a); }
            if (mh_ev[i].m == WM_NCACTIVATE && mh_ev[i].h == a) nca = i;
        }
        CHECK(acta >= 0 && actb > acta);
        (void)ina; (void)nca; (void)wa;
    }
    mh_kill(a); mh_kill(b);
}

/* ---- ordering of destroy relative to posted messages ---- */
TEST(messages_destroy_posted_message_after_destroy_not_delivered) {
    HWND h;
    REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    PMA(h, MU(60), 0, 0);
    mh_rec = 1;
    DESTROYW(h);
    mh_drain();
    mh_rec = 0;
    EXPECT_EQ(mh_count(MU(60)), 0);
}

TEST(messages_create_two_windows_distinct_handles_and_messages) {
    HWND a, b;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1;
    a = mh_mk(0, 0, NULL); b = mh_mk(0, 0, NULL);
    mh_rec = 0;
    REQUIRE(a && b);
    CHECK(a != b);
    EXPECT_EQ(mh_count(WM_CREATE), 2);
    EXPECT_EQ(mh_ev[mh_find(WM_CREATE, 0)].h, a);
    mh_kill(a); mh_kill(b);
}

TEST(messages_create_window_handle_valid_during_wm_create) {
    HWND h; 
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1; in_destroy_valid = -1;
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 0;
    { int k = mh_find(WM_CREATE, 0); CHECK(k >= 0 && mh_ev[k].h == h); }
    { int k = mh_find(WM_NCCREATE, 0); CHECK(k >= 0 && mh_ev[k].h == h); }
    mh_kill(h);
}
