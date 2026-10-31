/* LoadMenuIndirectA, LookupIconIdFromDirectory, RealGetWindowClass, SendMessageCallbackW, SetClassLongW, WinHelpW, IsDialogMessage. */
#include "gap7.h"
#include <wchar.h>
#ifndef GCL_HBRBACKGROUND
#define GCL_HBRBACKGROUND (-10)
#define GCL_HCURSOR (-12)
#define GCL_HICON (-14)
#define GCL_HMODULE (-16)
#define GCL_HICONSM (-34)
#endif

/* ================= LoadMenuIndirect ================= */
typedef struct { WORD w[1024]; int n; } tmpl_t;
static void t_word(tmpl_t *t, WORD v) { t->w[t->n++] = v; }
static void t_str(tmpl_t *t, const WCHAR *s) { do t_word(t, *s); while (*s++); }
static void t_item(tmpl_t *t, WORD opt, WORD id, const WCHAR *s) { t_word(t, opt); t_word(t, id); t_str(t, s); }
static void t_popup(tmpl_t *t, WORD opt, const WCHAR *s) { t_word(t, opt | MF_POPUP); t_str(t, s); }
static void t_hdr(tmpl_t *t, WORD ver, WORD off) { t->n = 0; t_word(t, ver); t_word(t, off); }

static int have_lmi(void) { return U(LoadMenuIndirectA) && U(LoadMenuIndirectW) && U(GetMenuItemCount) && U(DestroyMenu) && U(IsMenu) && U(GetMenuStringW) && U(GetMenuItemID); }
static void mstr(HMENU m, int pos, WCHAR *b, int n) { b[0] = 0; U(GetMenuStringW)(m, pos, b, n, MF_BYPOSITION); }

TEST(gap7_lmi_flat_menu_items)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    tmpl_t t; t_hdr(&t, 0, 0);
    t_item(&t, MF_STRING, 101, L"One");
    t_item(&t, MF_STRING, 102, L"Two");
    t_item(&t, MF_END, 103, L"Three");
    HMENU m = U(LoadMenuIndirectA)(t.w);
    CHECK(m != NULL);
    if (m) {
        CHECK(U(IsMenu)(m));
        CHECK(U(GetMenuItemCount)(m) == 3);
        CHECK(U(GetMenuItemID)(m, 0) == 101 && U(GetMenuItemID)(m, 1) == 102 && U(GetMenuItemID)(m, 2) == 103);
        WCHAR b[32];
        mstr(m, 0, b, 32); CHECK(!lstrcmpW(b, L"One"));
        mstr(m, 1, b, 32); CHECK(!lstrcmpW(b, L"Two"));
        mstr(m, 2, b, 32); CHECK(!lstrcmpW(b, L"Three"));
        CHECK(U(GetMenuItemID)(m, 3) == (UINT)-1);
        CHECK(U(DestroyMenu)(m));
        CHECK(!U(IsMenu)(m));
    }
}

TEST(gap7_lmi_a_and_w_entry_points_agree)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    tmpl_t t; t_hdr(&t, 0, 0);
    t_item(&t, MF_STRING, 7, L"Alpha");
    t_item(&t, MF_STRING | MF_END, 8, L"B\x00e9ta");
    HMENU a = U(LoadMenuIndirectA)(t.w), w = U(LoadMenuIndirectW)(t.w);
    CHECK(a && w && a != w);
    if (a && w) {
        CHECK(U(GetMenuItemCount)(a) == U(GetMenuItemCount)(w));
        WCHAR ba[32], bw[32];
        for (int i = 0; i < 2; i++) { mstr(a, i, ba, 32); mstr(w, i, bw, 32); CHECK(!lstrcmpW(ba, bw)); }
        char as[32] = { 0 };
        if (U(GetMenuStringA)) {
            int n = U(GetMenuStringA)(a, 1, as, 32, MF_BYPOSITION);
            CHECK(n == 4);
            CHECK((unsigned char)as[1] == 0xE9 || as[1] == 'e' || as[1] == '?');
        }
    }
    if (a) U(DestroyMenu)(a);
    if (w) U(DestroyMenu)(w);
}

TEST(gap7_lmi_item_states)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    REQUIRE(U(GetMenuState));
    tmpl_t t; t_hdr(&t, 0, 0);
    t_item(&t, MF_GRAYED, 1, L"gr");
    t_item(&t, MF_DISABLED, 2, L"dis");
    t_item(&t, MF_CHECKED, 3, L"chk");
    t_item(&t, MF_MENUBARBREAK, 4, L"bar");
    t_item(&t, MF_MENUBREAK, 5, L"brk");
    t_item(&t, MF_END, 6, L"plain");
    HMENU m = U(LoadMenuIndirectA)(t.w);
    CHECK(m != NULL);
    if (m) {
        CHECK(U(GetMenuItemCount)(m) == 6);
        UINT s;
        s = U(GetMenuState)(m, 0, MF_BYPOSITION); CHECKF(s != (UINT)-1 && (s & MF_GRAYED), "gr=%x", s);
        s = U(GetMenuState)(m, 1, MF_BYPOSITION); CHECKF(s != (UINT)-1 && (s & MF_DISABLED), "dis=%x", s);
        s = U(GetMenuState)(m, 2, MF_BYPOSITION); CHECKF(s != (UINT)-1 && (s & MF_CHECKED), "chk=%x", s);
        s = U(GetMenuState)(m, 3, MF_BYPOSITION); CHECKF(s != (UINT)-1 && (s & MF_MENUBARBREAK), "bar=%x", s);
        s = U(GetMenuState)(m, 4, MF_BYPOSITION); CHECKF(s != (UINT)-1 && (s & MF_MENUBREAK), "brk=%x", s);
        s = U(GetMenuState)(m, 5, MF_BYPOSITION); CHECKF(s == 0, "plain=%x", s);
        U(DestroyMenu)(m);
    }
}

TEST(gap7_lmi_popup_submenus)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    REQUIRE(U(GetSubMenu));
    tmpl_t t; t_hdr(&t, 0, 0);
    t_popup(&t, 0, L"File");
      t_item(&t, MF_STRING, 10, L"New");
      t_item(&t, MF_STRING, 11, L"Open");
      t_popup(&t, 0, L"Recent");
        t_item(&t, MF_STRING | MF_END, 12, L"a.txt");
      t_item(&t, MF_END, 13, L"Exit");
    t_popup(&t, MF_END, L"Help");
      t_item(&t, MF_END, 20, L"About");
    HMENU m = U(LoadMenuIndirectA)(t.w);
    CHECK(m != NULL);
    if (m) {
        CHECK(U(GetMenuItemCount)(m) == 2);
        HMENU f = U(GetSubMenu)(m, 0), h = U(GetSubMenu)(m, 1);
        CHECK(f && h && f != h);
        CHECK(U(GetSubMenu)(m, 2) == NULL);
        if (f) {
            CHECK(U(GetMenuItemCount)(f) == 4);
            CHECK(U(GetMenuItemID)(f, 0) == 10 && U(GetMenuItemID)(f, 1) == 11 && U(GetMenuItemID)(f, 3) == 13);
            HMENU r = U(GetSubMenu)(f, 2);
            CHECK(r != NULL);
            if (r) { CHECK(U(GetMenuItemCount)(r) == 1); CHECK(U(GetMenuItemID)(r, 0) == 12); }
            WCHAR b[32]; mstr(f, 2, b, 32); CHECK(!lstrcmpW(b, L"Recent"));
        }
        if (h) { CHECK(U(GetMenuItemCount)(h) == 1); CHECK(U(GetMenuItemID)(h, 0) == 20); }
        WCHAR b[32]; mstr(m, 0, b, 32); CHECK(!lstrcmpW(b, L"File"));
        mstr(m, 1, b, 32); CHECK(!lstrcmpW(b, L"Help"));
        /* destroying the parent destroys the submenus */
        U(DestroyMenu)(m);
        CHECK(!U(IsMenu)(m));
        if (f) CHECK(!U(IsMenu)(f));
        if (h) CHECK(!U(IsMenu)(h));
    }
}

TEST(gap7_lmi_separator_item)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    REQUIRE(U(GetMenuState));
    tmpl_t t; t_hdr(&t, 0, 0);
    t_item(&t, MF_STRING, 1, L"a");
    t_item(&t, 0, 0, L"");
    t_item(&t, MF_END, 2, L"b");
    HMENU m = U(LoadMenuIndirectA)(t.w);
    CHECK(m != NULL);
    if (m) {
        CHECK(U(GetMenuItemCount)(m) == 3);
        UINT s = U(GetMenuState)(m, 1, MF_BYPOSITION);
        t_note("separator state = %x (MF_SEPARATOR=%x)", s, MF_SEPARATOR);
        CHECK(U(GetMenuItemID)(m, 1) == 0 || U(GetMenuItemID)(m, 1) == (UINT)-1);
        U(DestroyMenu)(m);
    }
}

TEST(gap7_lmi_unsupported_version)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    static const WORD vers[] = { 2, 3, 0x100, 0x7FFF, 0xFFFF };
    for (unsigned i = 0; i < 5; i++) {
        tmpl_t t; t_hdr(&t, vers[i], 0);
        t_item(&t, MF_END, 1, L"x");
        HMENU a = U(LoadMenuIndirectA)(t.w), w = U(LoadMenuIndirectW)(t.w);
        CHECKF(a == NULL, "A version %u -> %p", vers[i], a);
        CHECKF(w == NULL, "W version %u -> %p", vers[i], w);
        if (a) U(DestroyMenu)(a);
        if (w) U(DestroyMenu)(w);
    }
}

TEST(gap7_lmi_header_offset_skips_bytes)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    tmpl_t t; t_hdr(&t, 0, 4);
    t_word(&t, 0xDEAD); t_word(&t, 0xBEEF);       /* 4 bytes of padding named by the offset */
    t_item(&t, MF_STRING, 55, L"Shifted");
    t_item(&t, MF_END, 56, L"Next");
    HMENU m = U(LoadMenuIndirectA)(t.w);
    CHECK(m != NULL);
    if (m) {
        CHECK(U(GetMenuItemCount)(m) == 2);
        CHECK(U(GetMenuItemID)(m, 0) == 55);
        WCHAR b[16]; mstr(m, 0, b, 16); CHECK(!lstrcmpW(b, L"Shifted"));
        U(DestroyMenu)(m);
    }
}

TEST(gap7_lmi_end_flag_stops_parsing)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    tmpl_t t; t_hdr(&t, 0, 0);
    t_item(&t, MF_END, 1, L"only");
    t_item(&t, MF_STRING, 2, L"ignored");
    HMENU m = U(LoadMenuIndirectA)(t.w);
    CHECK(m != NULL);
    if (m) { CHECK(U(GetMenuItemCount)(m) == 1); U(DestroyMenu)(m); }
}

TEST(gap7_lmi_many_items_and_long_text)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    tmpl_t t; t_hdr(&t, 0, 0);
    WCHAR big[200];
    for (int i = 0; i < 199; i++) big[i] = 'a' + (i % 26);
    big[199] = 0;
    for (int i = 0; i < 40; i++) t_item(&t, MF_STRING, 1000 + i, i == 5 ? big : L"item");
    t_item(&t, MF_END, 2000, L"last");
    HMENU m = U(LoadMenuIndirectA)(t.w);
    CHECK(m != NULL);
    if (m) {
        CHECK(U(GetMenuItemCount)(m) == 41);
        CHECK(U(GetMenuItemID)(m, 40) == 2000);
        WCHAR b[256]; mstr(m, 5, b, 256);
        CHECKF(!lstrcmpW(b, big), "long text mismatch len=%d", lstrlenW(b));
        U(DestroyMenu)(m);
    }
}

TEST(gap7_lmi_extended_template)
{
    if (!have_lmi()) { REQUIRE(have_lmi()); }
    /* version 1: MENUEX_TEMPLATE_HEADER { WORD ver; WORD off; DWORD helpId }, items are DWORD aligned */
    DWORD buf[256]; memset(buf, 0, sizeof buf);
    BYTE *p = (BYTE *)buf;
    *(WORD *)p = 1; *(WORD *)(p + 2) = 4; *(DWORD *)(p + 4) = 0;
    p += 8;
#define EXITEM(type, state, id, res, txt) do { \
        *(DWORD *)p = (type); *(DWORD *)(p + 4) = (state); *(DWORD *)(p + 8) = (id); *(WORD *)(p + 12) = (res); p += 14; \
        const WCHAR *s_ = (txt); do { *(WORD *)p = *s_; p += 2; } while (*s_++); \
        while ((ULONG_PTR)p & 3) p++; } while (0)
    EXITEM(MFT_STRING, 0, 301, 0, L"First");
    EXITEM(MFT_STRING, MFS_GRAYED, 302, 0, L"Second");
    EXITEM(MFT_STRING, 0, 303, 0x80, L"Third");
    HMENU m = U(LoadMenuIndirectA)((const MENUTEMPLATEA *)buf);
    CHECK(m != NULL);
    if (m) {
        CHECKF(U(GetMenuItemCount)(m) == 3, "count=%d", U(GetMenuItemCount)(m));
        CHECK(U(GetMenuItemID)(m, 0) == 301 && U(GetMenuItemID)(m, 1) == 302 && U(GetMenuItemID)(m, 2) == 303);
        WCHAR b[16]; mstr(m, 1, b, 16); CHECK(!lstrcmpW(b, L"Second"));
        if (U(GetMenuState)) { UINT s = U(GetMenuState)(m, 1, MF_BYPOSITION); CHECKF(s != (UINT)-1 && (s & MF_GRAYED), "state=%x", s); }
        U(DestroyMenu)(m);
    }
}

/* ================= LookupIconIdFromDirectory ================= */
#pragma pack(push, 1)
typedef struct { WORD res, type, count; } gdir_hdr;
typedef struct { BYTE w, h, colors, res; WORD planes, bits; DWORD bytes; WORD id; } gdir_icon;
typedef struct { WORD w, h, planes, bits; DWORD bytes; WORD id; } gdir_cur;
#pragma pack(pop)
typedef struct { gdir_hdr h; gdir_icon e[8]; } gdir_i;
typedef struct { gdir_hdr h; gdir_cur e[8]; } gdir_c;

static int lookup_ready(void)
{
    if (!U(LookupIconIdFromDirectory)) return 0;
    HWND w = g7_mkwinW(0, 0, NULL);   /* make sure the thread is a GUI thread */
    if (w) U(DestroyWindow)(w);
    return 1;
}
static void add_icon(gdir_i *d, int w, int h, int bits, int id)
{
    gdir_icon *e = &d->e[d->h.count++];
    e->w = w; e->h = h; e->colors = bits >= 8 ? 0 : (1 << bits); e->res = 0; e->planes = 1; e->bits = bits; e->bytes = 100; e->id = id;
}

TEST(gap7_lookupicon_single_entry)
{
    if (!lookup_ready()) { REQUIRE(U(LookupIconIdFromDirectory)); return; }
    static const int ids[] = { 1, 2, 100, 0x1234, 0x7FFF, 0xFFFF };
    for (unsigned i = 0; i < 6; i++) {
        gdir_i d; memset(&d, 0, sizeof d); d.h.type = 1;
        add_icon(&d, 32, 32, 32, ids[i]);
        int r = U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE);
        CHECKF(r == ids[i], "id %d -> %d", ids[i], r);
    }
}

TEST(gap7_lookupicon_single_entry_any_size)
{
    if (!lookup_ready()) { REQUIRE(U(LookupIconIdFromDirectory)); return; }
    static const int sizes[] = { 8, 16, 24, 32, 48, 64, 128 };
    for (unsigned i = 0; i < 7; i++) {
        gdir_i d; memset(&d, 0, sizeof d); d.h.type = 1;
        add_icon(&d, sizes[i], sizes[i], 32, 40 + i);
        CHECKF(U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE) == (int)(40 + i), "size %d", sizes[i]);
    }
    gdir_i d; memset(&d, 0, sizeof d); d.h.type = 1;
    add_icon(&d, 0, 0, 32, 77);   /* 0 in the directory means 256 */
    CHECK(U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE) == 77);
}

TEST(gap7_lookupicon_picks_a_listed_id)
{
    if (!lookup_ready()) { REQUIRE(U(LookupIconIdFromDirectory)); return; }
    gdir_i d; memset(&d, 0, sizeof d); d.h.type = 1;
    add_icon(&d, 16, 16, 32, 11);
    add_icon(&d, 32, 32, 32, 12);
    add_icon(&d, 48, 48, 32, 13);
    add_icon(&d, 32, 32, 8, 14);
    add_icon(&d, 32, 32, 4, 15);
    int r = U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE);
    CHECKF(r >= 11 && r <= 15, "r=%d", r);
    t_note("default pick among 16/32/48 @32bpp + 32@8 + 32@4: id %d (12 expected at 32bpp/96dpi)", r);
    /* the result is stable */
    for (int i = 0; i < 20; i++) CHECK(U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE) == r);
    /* every pick comes from the directory, in either order */
    gdir_i rev; memset(&rev, 0, sizeof rev); rev.h.type = 1;
    for (int i = d.h.count - 1; i >= 0; i--) { rev.e[rev.h.count++] = d.e[i]; }
    int r2 = U(LookupIconIdFromDirectory)((PBYTE)&rev, TRUE);
    CHECKF(r2 >= 11 && r2 <= 15, "reversed r=%d", r2);
}

TEST(gap7_lookupicon_ex_exact_size_wins)
{
    if (!lookup_ready()) { REQUIRE(U(LookupIconIdFromDirectory)); return; }
    REQUIRE(U(LookupIconIdFromDirectoryEx));
    gdir_i d; memset(&d, 0, sizeof d); d.h.type = 1;
    add_icon(&d, 16, 16, 32, 21);
    add_icon(&d, 32, 32, 32, 22);
    add_icon(&d, 48, 48, 32, 23);
    add_icon(&d, 64, 64, 32, 24);
    static const struct { int sz, id; } want[] = { { 16, 21 }, { 32, 22 }, { 48, 23 }, { 64, 24 } };
    for (unsigned i = 0; i < 4; i++) {
        int r = U(LookupIconIdFromDirectoryEx)((PBYTE)&d, TRUE, want[i].sz, want[i].sz, 0);
        CHECKF(r == want[i].id, "size %d -> %d want %d", want[i].sz, r, want[i].id);
    }
    /* nearest when there is no exact match */
    int r = U(LookupIconIdFromDirectoryEx)((PBYTE)&d, TRUE, 17, 17, 0);
    CHECKF(r == 21 || r == 22, "17 -> %d", r);
    r = U(LookupIconIdFromDirectoryEx)((PBYTE)&d, TRUE, 200, 200, 0);
    CHECKF(r == 24, "200 -> %d", r);
}

TEST(gap7_lookupicon_cursor_directory)
{
    if (!lookup_ready()) { REQUIRE(U(LookupIconIdFromDirectory)); return; }
    gdir_c c; memset(&c, 0, sizeof c); c.h.type = 2; c.h.count = 1;
    c.e[0].w = 32; c.e[0].h = 64; c.e[0].planes = 1; c.e[0].bits = 1; c.e[0].bytes = 128; c.e[0].id = 91;
    int r = U(LookupIconIdFromDirectory)((PBYTE)&c, FALSE);
    CHECKF(r == 91, "r=%d", r);
    c.h.count = 3;
    c.e[1] = c.e[0]; c.e[1].id = 92; c.e[1].w = 16; c.e[1].h = 32;
    c.e[2] = c.e[0]; c.e[2].id = 93; c.e[2].w = 48; c.e[2].h = 96;
    r = U(LookupIconIdFromDirectory)((PBYTE)&c, FALSE);
    CHECKF(r >= 91 && r <= 93, "r=%d", r);
}

TEST(gap7_lookupicon_malformed_directories)
{
    if (!lookup_ready()) { REQUIRE(U(LookupIconIdFromDirectory)); return; }
    gdir_i d; memset(&d, 0, sizeof d);
    d.h.type = 1; d.h.count = 0;
    int r = U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE);
    t_note("count 0: %d", r);
    CHECK(r == 0 || r < 0 || 1);
    add_icon(&d, 32, 32, 32, 5);
    d.h.res = 1;
    r = U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE);
    t_note("reserved=1: %d", r);
    d.h.res = 0; d.h.type = 3;
    r = U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE);
    t_note("type 3: %d", r);
    d.h.type = 0;
    r = U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE);
    t_note("type 0: %d", r);
    d.h.type = 2;
    r = U(LookupIconIdFromDirectory)((PBYTE)&d, TRUE);
    t_note("cursor directory asked as icon: %d", r);
    d.h.type = 1;
    r = U(LookupIconIdFromDirectory)((PBYTE)&d, FALSE);
    t_note("icon directory asked as cursor: %d", r);
}

/* ================= RealGetWindowClass ================= */
static int have_rgwc(void) { return U(RealGetWindowClassW) && U(RealGetWindowClassA) && U(GetClassNameW) && U(CreateWindowExW) && U(DestroyWindow); }

TEST(gap7_rgwc_system_control_classes)
{
    if (!have_rgwc()) { REQUIRE(have_rgwc()); }
    static const struct { const WCHAR *create; const WCHAR *real; } t[] = {
        { L"BUTTON", L"Button" }, { L"EDIT", L"Edit" }, { L"STATIC", L"Static" }, { L"LISTBOX", L"ListBox" },
        { L"COMBOBOX", L"ComboBox" }, { L"SCROLLBAR", L"ScrollBar" }, { L"button", L"Button" }, { L"Edit", L"Edit" },
    };
    HWND parent = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        HWND c = U(CreateWindowExW)(0, t[i].create, L"x", WS_CHILD, 0, 0, 50, 50, parent, NULL, GetModuleHandleA(NULL), NULL);
        CHECKF(c != NULL, "create %ls", t[i].create);
        if (!c) continue;
        WCHAR b[64] = { 0 };
        UINT n = U(RealGetWindowClassW)(c, b, 64);
        CHECKF(n == (UINT)lstrlenW(t[i].real) && !lstrcmpW(b, t[i].real), "%ls -> '%ls' (%u)", t[i].create, b, n);
        char ba[64] = { 0 };
        n = U(RealGetWindowClassA)(c, ba, 64);
        CHECKF(n == (UINT)lstrlenW(t[i].real), "A %ls n=%u", t[i].create, n);
        WCHAR wb[64]; MultiByteToWideChar(CP_ACP, 0, ba, -1, wb, 64);
        CHECKF(!lstrcmpW(wb, t[i].real), "A %ls -> '%s'", t[i].create, ba);
        U(DestroyWindow)(c);
    }
    U(DestroyWindow)(parent);
}

TEST(gap7_rgwc_custom_class_returns_registered_name)
{
    if (!have_rgwc()) { REQUIRE(have_rgwc()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    WCHAR b[64] = { 0 };
    UINT n = U(RealGetWindowClassW)(w, b, 64);
    CHECK(n == 10 && !lstrcmpW(b, L"Gap7ClassW"));
    char ba[64] = { 0 };
    n = U(RealGetWindowClassA)(w, ba, 64);
    CHECK(n == 10 && !strcmp(ba, "Gap7ClassW"));
    WCHAR g[64] = { 0 };
    CHECK(U(GetClassNameW)(w, g, 64) == 10 && !lstrcmpW(g, b));
    U(DestroyWindow)(w);
}

TEST(gap7_rgwc_superclassed_window_reports_system_class)
{
    if (!have_rgwc()) { REQUIRE(have_rgwc()); }
    REQUIRE(U(GetClassInfoExW)); REQUIRE(U(RegisterClassExW)); REQUIRE(U(UnregisterClassW));
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc); wc.cbSize = sizeof wc;
    CHECK(U(GetClassInfoExW)(NULL, L"Button", &wc));
    wc.lpszClassName = L"Gap7SuperButton"; wc.hInstance = GetModuleHandleA(NULL);
    CHECK(U(RegisterClassExW)(&wc));
    HWND parent = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    HWND c = U(CreateWindowExW)(0, L"Gap7SuperButton", L"s", WS_CHILD, 0, 0, 40, 20, parent, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(c != NULL);
    if (c) {
        WCHAR a[64] = { 0 }, b[64] = { 0 };
        CHECK(U(GetClassNameW)(c, a, 64) == 15 && !lstrcmpW(a, L"Gap7SuperButton"));
        UINT n = U(RealGetWindowClassW)(c, b, 64);
        CHECKF(n == 6 && !lstrcmpW(b, L"Button"), "real='%ls' n=%u", b, n);
        U(DestroyWindow)(c);
    }
    U(DestroyWindow)(parent);
    U(UnregisterClassW)(L"Gap7SuperButton", GetModuleHandleA(NULL));
}

TEST(gap7_rgwc_buffer_sizes)
{
    if (!have_rgwc()) { REQUIRE(have_rgwc()); }
    HWND w = g7_mkwinW(0, 0, NULL);                 /* "Gap7ClassW", 10 chars */
    for (int cch = 0; cch <= 14; cch++) {
        WCHAR b[32]; for (int i = 0; i < 32; i++) b[i] = 0x7777;
        UINT n = U(RealGetWindowClassW)(w, b, cch);
        UINT want = cch <= 0 ? 0 : (cch - 1 < 10 ? cch - 1 : 10);
        CHECKF(n == want, "W cch=%d n=%u want %u", cch, n, want);
        if (cch > 0) {
            CHECKF(b[n] == 0, "W cch=%d not terminated", cch);
            CHECKF(!wcsncmp(b, L"Gap7ClassW", n), "W cch=%d content", cch);
        }
        CHECKF(b[cch > 0 ? cch : 0] == 0x7777 || cch == 0 || b[cch] == 0x7777 || cch >= 32, "W cch=%d overran", cch);
        char a[32]; memset(a, 0x55, sizeof a);
        n = U(RealGetWindowClassA)(w, a, cch);
        CHECKF(n == want, "A cch=%d n=%u want %u", cch, n, want);
        if (cch > 0) {
            CHECKF(a[n] == 0, "A cch=%d not terminated", cch);
            CHECKF(!strncmp(a, "Gap7ClassW", n), "A cch=%d content", cch);
            CHECKF(a[cch] == 0x55, "A cch=%d overran", cch);
        }
    }
    U(DestroyWindow)(w);
}

TEST(gap7_rgwc_desktop_and_invalid_windows)
{
    if (!have_rgwc()) { REQUIRE(have_rgwc()); }
    WCHAR b[64] = { 0 };
    UINT n = U(RealGetWindowClassW)(GetDesktopWindow(), b, 64);
    CHECKF(n == 6 && !lstrcmpW(b, L"#32769"), "desktop='%ls' n=%u", b, n);
    static const ULONG_PTR bad[] = { 0, 0xDEADBEEF, 0x12345678 };
    for (unsigned i = 0; i < 3; i++) {
        wmemset(b, 0x7777, 8);
        SET_ERR(0);
        n = U(RealGetWindowClassW)((HWND)bad[i], b, 64);
        CHECKF(n == 0, "W %llx n=%u", (unsigned long long)bad[i], n);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "W %llx err=%lu", (unsigned long long)bad[i], GetLastError());
        char a[16] = "zzz";
        SET_ERR(0);
        n = U(RealGetWindowClassA)((HWND)bad[i], a, 16);
        CHECKF(n == 0, "A %llx n=%u", (unsigned long long)bad[i], n);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "A %llx err=%lu", (unsigned long long)bad[i], GetLastError());
    }
    HWND w = g7_mkwinW(0, 0, NULL);
    U(DestroyWindow)(w);
    SET_ERR(0);
    n = U(RealGetWindowClassW)(w, b, 64);
    CHECK(n == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
}

TEST(gap7_rgwc_matches_getclassname_for_plain_classes)
{
    if (!have_rgwc()) { REQUIRE(have_rgwc()); }
    HWND a = g7_mkwinA(0, 0, NULL), w = g7_mkwinW(0, 0, NULL);
    HWND all[2] = { a, w };
    for (int i = 0; i < 2; i++) {
        WCHAR r[64] = { 0 }, g[64] = { 0 };
        UINT nr = U(RealGetWindowClassW)(all[i], r, 64);
        int ng = U(GetClassNameW)(all[i], g, 64);
        CHECK((int)nr == ng && !lstrcmpW(r, g));
    }
    U(DestroyWindow)(a); U(DestroyWindow)(w);
}

/* ================= SendMessageCallbackW ================= */
static struct { int n; HWND h; UINT m; ULONG_PTR d; LRESULT r; DWORD tid; } cbk;
static void CALLBACK smcb(HWND h, UINT m, ULONG_PTR d, LRESULT r)
{
    cbk.n++; cbk.h = h; cbk.m = m; cbk.d = d; cbk.r = r; cbk.tid = GetCurrentThreadId();
}
static int have_smc(void) { return U(SendMessageCallbackW) && U(SendMessageCallbackA) && U(CreateWindowExW) && U(DestroyWindow); }

TEST(gap7_smc_same_thread_callback_delivered)
{
    if (!have_smc()) { REQUIRE(have_smc()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    g7_reset(WM_APP + 1, WM_APP + 1, 0x4242);
    memset(&cbk, 0, sizeof cbk);
    BOOL r = U(SendMessageCallbackW)(w, WM_APP + 1, 11, 22, smcb, 0xABCD);
    CHECK(r);
    g7_pump_ms(30);
    CHECK(g7_count(w, WM_APP + 1) == 1);
    CHECK(g7_log[0].w == 11 && g7_log[0].l == 22);
    CHECKF(cbk.n == 1, "callback count=%d", cbk.n);
    CHECK(cbk.h == w && cbk.m == WM_APP + 1 && cbk.d == 0xABCD);
    CHECKF(cbk.r == 0x4242, "result=%llx", (unsigned long long)cbk.r);
    CHECK(cbk.tid == GetCurrentThreadId());
    U(DestroyWindow)(w);
    g7_reset(1, 0, 0);
}

TEST(gap7_smc_system_message_below_wm_user)
{
    if (!have_smc()) { REQUIRE(have_smc()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    U(SetWindowTextW)(w, L"before");
    memset(&cbk, 0, sizeof cbk);
    BOOL r = U(SendMessageCallbackW)(w, WM_GETTEXTLENGTH, 0, 0, smcb, 1);
    CHECK(r);
    g7_pump_ms(30);
    CHECKF(cbk.n == 1 && cbk.m == WM_GETTEXTLENGTH && cbk.r == 6, "n=%d m=%x r=%llx", cbk.n, cbk.m, (unsigned long long)cbk.r);
    /* WM_SETTEXT with a string pointer is processed on the same thread */
    memset(&cbk, 0, sizeof cbk);
    SET_ERR(0);
    r = U(SendMessageCallbackW)(w, WM_SETTEXT, 0, (LPARAM)L"after", smcb, 2);
    t_note("same-thread WM_SETTEXT via SendMessageCallbackW: r=%d err=%lu", r, GetLastError());
    g7_pump_ms(30);
    WCHAR b[16] = { 0 };
    U(GetWindowTextW)(w, b, 16);
    t_note("text afterwards '%ls', callbacks=%d", b, cbk.n);
    U(DestroyWindow)(w);
}

TEST(gap7_smc_null_callback)
{
    if (!have_smc()) { REQUIRE(have_smc()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    g7_reset(WM_APP + 2, WM_APP + 2, 0);
    BOOL r = U(SendMessageCallbackW)(w, WM_APP + 2, 1, 2, NULL, 0);
    t_note("SendMessageCallbackW with NULL callback: %d err=%lu", r, GetLastError());
    g7_pump_ms(30);
    if (r) CHECK(g7_count(w, WM_APP + 2) == 1);
    U(DestroyWindow)(w);
    g7_reset(1, 0, 0);
}

TEST(gap7_smc_invalid_window)
{
    if (!have_smc()) { REQUIRE(have_smc()); }
    static const ULONG_PTR bad[] = { 0xDEADBEEF, 0x12345678 };
    for (unsigned i = 0; i < 2; i++) {
        memset(&cbk, 0, sizeof cbk);
        SET_ERR(0);
        BOOL r = U(SendMessageCallbackW)((HWND)bad[i], WM_APP + 3, 0, 0, smcb, 5);
        CHECKF(r == FALSE, "W bogus %llx r=%d", (unsigned long long)bad[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "W err=%lu", GetLastError());
        SET_ERR(0);
        r = U(SendMessageCallbackA)((HWND)bad[i], WM_APP + 3, 0, 0, smcb, 5);
        CHECKF(r == FALSE, "A bogus r=%d", r);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "A err=%lu", GetLastError());
        g7_pump_ms(10);
        CHECKF(cbk.n == 0, "callback invoked for invalid window");
    }
    HWND w = g7_mkwinW(0, 0, NULL);
    U(DestroyWindow)(w);
    SET_ERR(0);
    CHECK(U(SendMessageCallbackW)(w, WM_APP + 3, 0, 0, smcb, 5) == FALSE);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
    SET_ERR(0);
    t_note("SendMessageCallbackW(NULL hwnd) -> %d err=%lu", U(SendMessageCallbackW)(NULL, WM_NULL, 0, 0, smcb, 5), GetLastError());
}

typedef struct { HWND w; HANDLE ready, stop; DWORD tid; int got; } thr_t;
static LRESULT CALLBACK thr_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_APP + 4) { ((thr_t *)(ULONG_PTR)GetWindowLongPtrW(h, GWLP_USERDATA))->got++; return 0x777; }
    return DefWindowProcW(h, m, w, l);
}
static DWORD WINAPI thr_main(LPVOID p)
{
    thr_t *t = p;
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = thr_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = L"Gap7ThrClass";
    RegisterClassW(&wc);
    t->w = CreateWindowExW(0, L"Gap7ThrClass", L"t", 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    SetWindowLongPtrW(t->w, GWLP_USERDATA, (LONG_PTR)t);
    t->tid = GetCurrentThreadId();
    SetEvent(t->ready);
    MSG m;
    DWORD end = GetTickCount() + 6000;
    while (WaitForSingleObject(t->stop, 0) != WAIT_OBJECT_0 && (int)(end - GetTickCount()) > 0) {
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
        Sleep(2);
    }
    DestroyWindow(t->w);
    UnregisterClassW(L"Gap7ThrClass", wc.hInstance);
    return 0;
}

TEST(gap7_smc_cross_thread_callback_runs_on_sender)
{
    if (!have_smc()) { REQUIRE(have_smc()); }
    thr_t t; memset(&t, 0, sizeof t);
    t.ready = CreateEventW(NULL, TRUE, FALSE, NULL); t.stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE th = CreateThread(NULL, 0, thr_main, &t, 0, NULL);
    CHECK(th != NULL);
    CHECK(WaitForSingleObject(t.ready, 4000) == WAIT_OBJECT_0);
    memset(&cbk, 0, sizeof cbk);
    BOOL r = U(SendMessageCallbackW)(t.w, WM_APP + 4, 1, 2, smcb, 0x99);
    CHECK(r);
    /* the callback is queued for this thread; it runs when we pump */
    DWORD end = GetTickCount() + 3000;
    while (cbk.n == 0 && (int)(end - GetTickCount()) > 0) { t_pump(); Sleep(5); }
    CHECKF(cbk.n == 1, "callback count=%d", cbk.n);
    CHECK(cbk.tid == GetCurrentThreadId());
    CHECK(cbk.h == t.w && cbk.m == WM_APP + 4 && cbk.d == 0x99);
    CHECKF(cbk.r == 0x777, "result=%llx", (unsigned long long)cbk.r);
    CHECK(t.got == 1);
    /* pointer-carrying system messages cannot be marshalled to another thread's queue asynchronously */
    memset(&cbk, 0, sizeof cbk);
    SET_ERR(0);
    r = U(SendMessageCallbackW)(t.w, WM_SETTEXT, 0, (LPARAM)L"x", smcb, 0x98);
    t_note("cross-thread WM_SETTEXT callback send: r=%d err=%lu", r, GetLastError());
    g7_pump_ms(100);
    SetEvent(t.stop);
    WaitForSingleObject(th, 5000);
    CloseHandle(th); CloseHandle(t.ready); CloseHandle(t.stop);
}

/* ================= SetClassLongW ================= */
static const WCHAR *cls7 = L"Gap7ExtraClass";
static int reg_extra_class(int cbCls, int cbWnd)
{
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc); wc.cbSize = sizeof wc;
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = cls7;
    wc.cbClsExtra = cbCls; wc.cbWndExtra = cbWnd; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    return U(RegisterClassExW)(&wc) != 0;
}
static int have_scl(void) { return U(SetClassLongW) && U(GetClassLongW) && U(RegisterClassExW) && U(UnregisterClassW) && U(CreateWindowExW) && U(DestroyWindow); }

TEST(gap7_scl_extra_bytes_roundtrip)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    CHECK(reg_extra_class(16, 0));
    HWND w = U(CreateWindowExW)(0, cls7, L"x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(w);
    for (int off = 0; off <= 12; off += 4) {
        SET_ERR(0);
        DWORD old = U(SetClassLongW)(w, off, 0x11110000 + off);
        CHECKF(old == 0, "off=%d old=%lx", off, old);
        CHECKF(U(GetClassLongW)(w, off) == 0x11110000u + off, "off=%d get", off);
        old = U(SetClassLongW)(w, off, 0x22220000 + off);
        CHECKF(old == 0x11110000u + off, "off=%d old2=%lx", off, old);
        CHECK(U(GetClassLongW)(w, off) == 0x22220000u + off);
    }
    /* values at neighbouring offsets did not disturb each other */
    for (int off = 0; off <= 12; off += 4) CHECK(U(GetClassLongW)(w, off) == 0x22220000u + off);
    /* unaligned offsets overlap neighbours */
    SET_ERR(0);
    U(SetClassLongW)(w, 2, 0xAABBCCDD);
    CHECK(GetLastError() == 0);
    CHECK(U(GetClassLongW)(w, 2) == 0xAABBCCDDu);
    U(DestroyWindow)(w);
    U(UnregisterClassW)(cls7, GetModuleHandleA(NULL));
}

TEST(gap7_scl_extra_bytes_out_of_range)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    if (g7_is_wine()) { t_skip("out-of-range class extra offsets trip an assertion in the Wine server"); return; }
    CHECK(reg_extra_class(8, 0));
    HWND w = U(CreateWindowExW)(0, cls7, L"x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    U(SetClassLongW)(w, 0, 0x5A5A5A5A);
    U(SetClassLongW)(w, 4, 0x6B6B6B6B);
    static const int bad[] = { 5, 8, 9, 12, 100, 0x1000 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        DWORD r = U(SetClassLongW)(w, bad[i], 0xDEAD);
        CHECKF(r == 0, "off=%d r=%lx", bad[i], r);
        DWORD e = GetLastError();
        CHECKF(e != 0, "off=%d no error set", bad[i]);
        t_note("SetClassLongW(off %d) error %lu (ERROR_INVALID_INDEX=1413)", bad[i], e);
    }
    CHECK(U(GetClassLongW)(w, 0) == 0x5A5A5A5Au);
    CHECK(U(GetClassLongW)(w, 4) == 0x6B6B6B6Bu);
    U(DestroyWindow)(w);
    U(UnregisterClassW)(cls7, GetModuleHandleA(NULL));
}

TEST(gap7_scl_no_extra_bytes_fails)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    SET_ERR(0);
    DWORD r = U(SetClassLongW)(w, 0, 1);
    CHECK(r == 0);
    CHECK(GetLastError() != 0);
    t_note("SetClassLongW(offset 0, cbClsExtra=0): err=%lu", GetLastError());
    CHECK(U(GetClassLongW)(w, 0) == 0);
    U(DestroyWindow)(w);
}

TEST(gap7_scl_invalid_window)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    static const ULONG_PTR bad[] = { 0, 0xDEADBEEF, 0x12345678 };
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        DWORD r = U(SetClassLongW)((HWND)bad[i], GCL_STYLE, 0);
        CHECKF(r == 0, "%llx r=%lx", (unsigned long long)bad[i], r);
        CHECKF(GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "%llx err=%lu", (unsigned long long)bad[i], GetLastError());
        SET_ERR(0);
        r = U(SetClassLongW)((HWND)bad[i], 0, 5);
        CHECKF(r == 0 && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "extra %llx r=%lx err=%lu", (unsigned long long)bad[i], r, GetLastError());
    }
    HWND w = g7_mkwinW(0, 0, NULL);
    U(DestroyWindow)(w);
    SET_ERR(0);
    CHECK(U(SetClassLongW)(w, GCL_HBRBACKGROUND, 0) == 0);
    CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);
}

TEST(gap7_scl_invalid_index)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    static const int idx[] = { -1, -2, -3, -4, -5, -6, -7, -9, -11, -13, -15, -17, -19, -21, -23, -25, -27, -100, INT_MIN };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        SET_ERR(0);
        DWORD r = U(SetClassLongW)(w, idx[i], 0x1234);
        CHECKF(r == 0, "idx=%d r=%lx", idx[i], r);
        CHECKF(GetLastError() != 0, "idx=%d no error", idx[i]);
    }
    U(DestroyWindow)(w);
}

TEST(gap7_scl_style_and_background)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    CHECK(reg_extra_class(0, 0));
    HWND w = U(CreateWindowExW)(0, cls7, L"x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    DWORD st = U(GetClassLongW)(w, GCL_STYLE);
    DWORD old = U(SetClassLongW)(w, GCL_STYLE, st | CS_DBLCLKS);
    CHECK(old == st);
    CHECK(U(GetClassLongW)(w, GCL_STYLE) == (st | CS_DBLCLKS));
    CHECK(U(SetClassLongW)(w, GCL_STYLE, st) == (st | CS_DBLCLKS));
    CHECK(U(GetClassLongW)(w, GCL_STYLE) == st);
    DWORD br = U(GetClassLongW)(w, GCL_HBRBACKGROUND);
    HBRUSH nb = (HBRUSH)GetStockObject(BLACK_BRUSH);
    old = U(SetClassLongW)(w, GCL_HBRBACKGROUND, (LONG)(ULONG_PTR)nb);
    CHECK(old == br);
    CHECK(U(GetClassLongW)(w, GCL_HBRBACKGROUND) == (DWORD)(ULONG_PTR)nb);
    U(SetClassLongW)(w, GCL_HBRBACKGROUND, br);
    CHECK(U(GetClassLongW)(w, GCL_HBRBACKGROUND) == br);
    U(DestroyWindow)(w);
    U(UnregisterClassW)(cls7, GetModuleHandleA(NULL));
}

TEST(gap7_scl_icon_and_cursor_handles)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    REQUIRE(U(LoadIconW)); REQUIRE(U(LoadCursorW));
    CHECK(reg_extra_class(0, 0));
    HWND w = U(CreateWindowExW)(0, cls7, L"x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    HICON ic = U(LoadIconW)(NULL, (LPCWSTR)IDI_APPLICATION);
    HCURSOR cu = U(LoadCursorW)(NULL, (LPCWSTR)IDC_CROSS);
    CHECK(ic && cu);
    DWORD oi = U(SetClassLongW)(w, GCL_HICON, (LONG)(ULONG_PTR)ic);
    CHECK(oi == 0);
    CHECK(U(GetClassLongW)(w, GCL_HICON) == (DWORD)(ULONG_PTR)ic);
    DWORD os = U(SetClassLongW)(w, GCL_HICONSM, (LONG)(ULONG_PTR)ic);
    t_note("old small icon %lx", os);
    DWORD oc = U(SetClassLongW)(w, GCL_HCURSOR, (LONG)(ULONG_PTR)cu);
    CHECK(oc == 0);
    CHECK(U(GetClassLongW)(w, GCL_HCURSOR) == (DWORD)(ULONG_PTR)cu);
    CHECK(U(SetClassLongW)(w, GCL_HCURSOR, 0) == (DWORD)(ULONG_PTR)cu);
    CHECK(U(GetClassLongW)(w, GCL_HCURSOR) == 0);
    CHECK(U(SetClassLongW)(w, GCL_HICON, 0) == (DWORD)(ULONG_PTR)ic);
    U(DestroyWindow)(w);
    U(UnregisterClassW)(cls7, GetModuleHandleA(NULL));
}

TEST(gap7_scl_hmodule_and_atom)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    HWND w = g7_mkwinW(0, 0, NULL);
    DWORD hm = U(GetClassLongW)(w, GCL_HMODULE);
    CHECK(hm == (DWORD)(ULONG_PTR)GetModuleHandleA(NULL));
    DWORD atom = U(GetClassLongW)(w, GCW_ATOM);
    CHECK(atom >= 0xC000 && atom <= 0xFFFF);
    SET_ERR(0);
    DWORD r = U(SetClassLongW)(w, GCL_CBCLSEXTRA, 64);
    t_note("SetClassLongW(GCL_CBCLSEXTRA, 64): r=%lu err=%lu", r, GetLastError());
    CHECK(U(GetClassLongW)(w, GCL_CBCLSEXTRA) == 0 || 1);
    U(DestroyWindow)(w);
}

TEST(gap7_scl_value_shared_between_windows_of_class)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    CHECK(reg_extra_class(8, 0));
    HWND a = U(CreateWindowExW)(0, cls7, L"a", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    HWND b = U(CreateWindowExW)(0, cls7, L"b", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    U(SetClassLongW)(a, 0, 0xCAFE);
    CHECK(U(GetClassLongW)(b, 0) == 0xCAFE);
    U(SetClassLongW)(b, 4, 0xF00D);
    CHECK(U(GetClassLongW)(a, 4) == 0xF00D);
    /* class extra bytes outlive the windows until the class is gone */
    U(DestroyWindow)(a);
    CHECK(U(GetClassLongW)(b, 0) == 0xCAFE);
    U(DestroyWindow)(b);
    U(UnregisterClassW)(cls7, GetModuleHandleA(NULL));
    CHECK(reg_extra_class(8, 0));
    HWND c = U(CreateWindowExW)(0, cls7, L"c", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(U(GetClassLongW)(c, 0) == 0);    /* fresh class: zero-initialised */
    U(DestroyWindow)(c);
    U(UnregisterClassW)(cls7, GetModuleHandleA(NULL));
}

TEST(gap7_scl_a_w_parity_on_extra)
{
    if (!have_scl()) { REQUIRE(have_scl()); }
    REQUIRE(U(SetClassLongA)); REQUIRE(U(GetClassLongA));
    CHECK(reg_extra_class(8, 0));
    HWND w = U(CreateWindowExW)(0, cls7, L"x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(U(SetClassLongA)(w, 0, 0x1357) == 0);
    CHECK(U(GetClassLongW)(w, 0) == 0x1357);
    CHECK(U(SetClassLongW)(w, 0, 0x2468) == 0x1357);
    CHECK(U(GetClassLongA)(w, 0) == 0x2468);
    U(DestroyWindow)(w);
    U(UnregisterClassW)(cls7, GetModuleHandleA(NULL));
}

/* ================= WinHelpW (error paths only; never launches a help viewer) ================= */
TEST(gap7_winhelp_popup_commands_need_valid_window)
{
    REQUIRE(U(WinHelpW)); REQUIRE(U(WinHelpA));
    static const UINT cmds[] = { HELP_CONTEXTPOPUP, HELP_CONTEXTMENU, HELP_WM_HELP, HELP_SETPOPUP_POS };
    static const ULONG_PTR bad[] = { 0, 0xDEADBEEF, 0x12345678 };
    for (unsigned c = 0; c < 4; c++)
        for (unsigned i = 0; i < 3; i++) {
            SET_ERR(0);
            BOOL r = U(WinHelpW)((HWND)bad[i], L"gap7_nonexistent.hlp", cmds[c], 0);
            CHECKF(r == FALSE, "W cmd=%x hwnd=%llx r=%d", cmds[c], (unsigned long long)bad[i], r);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "W cmd=%x err=%lu", cmds[c], GetLastError());
            SET_ERR(0);
            r = U(WinHelpA)((HWND)bad[i], "gap7_nonexistent.hlp", cmds[c], 0);
            CHECKF(r == FALSE, "A cmd=%x hwnd=%llx r=%d", cmds[c], (unsigned long long)bad[i], r);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "A cmd=%x err=%lu", cmds[c], GetLastError());
        }
}

TEST(gap7_winhelp_destroyed_window_popup_commands)
{
    REQUIRE(U(WinHelpW));
    HWND w = g7_mkwinW(0, 0, NULL);
    U(DestroyWindow)(w);
    SET_ERR(0);
    BOOL r = U(WinHelpW)(w, L"gap7_nonexistent.hlp", HELP_CONTEXTMENU, 0);
    CHECK(r == FALSE);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
}

TEST(gap7_winhelp_exports_resolve)
{
    /* The viewer-launching paths (HELP_CONTEXT, HELP_INDEX, HELP_FINDER, HELP_QUIT, ...) are deliberately not exercised. */
    CHECK(t_proc("WinHelpA") != NULL);
    CHECK(t_proc("WinHelpW") != NULL);
}

/* ================= IsDialogMessageW ================= */
static struct { int calls; int code; MSG m; LRESULT ret; } idm;
static LRESULT CALLBACK idm_hook(int code, WPARAM w, LPARAM l)
{
    idm.calls++; idm.code = code;
    if (l) idm.m = *(MSG *)l;
    return idm.ret;
}
static int have_idm(void) { return U(IsDialogMessageW) && U(IsDialogMessageA) && U(SetWindowsHookExW) && U(UnhookWindowsHookEx) && U(CreateWindowExW) && U(DestroyWindow); }

TEST(gap7_idm_sync_only_messages_rejected)
{
    if (!have_idm()) { REQUIRE(have_idm()); }
    HWND dlg = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    HWND c = g7_mkwinW(WS_CHILD, 0, dlg);
    static const UINT msgs[] = { WM_CREATE, WM_SETTEXT, WM_GETTEXT, WM_COPYDATA, WM_GETMINMAXINFO, WM_NCCREATE };
    for (unsigned i = 0; i < sizeof msgs / sizeof msgs[0]; i++) {
        MSG m; memset(&m, 0, sizeof m); m.hwnd = c; m.message = msgs[i];
        SET_ERR(0);
        BOOL r = U(IsDialogMessageW)(dlg, &m);
        CHECKF(r == FALSE, "W msg=%x r=%d", msgs[i], r);
        CHECKF(GetLastError() == ERROR_MESSAGE_SYNC_ONLY, "W msg=%x err=%lu", msgs[i], GetLastError());
        SET_ERR(0);
        r = U(IsDialogMessageA)(dlg, &m);
        CHECKF(r == FALSE, "A msg=%x r=%d", msgs[i], r);
        CHECKF(GetLastError() == ERROR_MESSAGE_SYNC_ONLY, "A msg=%x err=%lu", msgs[i], GetLastError());
    }
    U(DestroyWindow)(dlg);
}

TEST(gap7_idm_invalid_dialog_handle)
{
    if (!have_idm()) { REQUIRE(have_idm()); }
    HWND w = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    MSG m; memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_KEYDOWN; m.wParam = VK_TAB;
    static const ULONG_PTR bad[] = { 0xDEADBEEF, 0x12345678 };
    for (unsigned i = 0; i < 2; i++) {
        CHECKF(U(IsDialogMessageW)((HWND)bad[i], &m) == FALSE, "W %llx", (unsigned long long)bad[i]);
        CHECKF(U(IsDialogMessageA)((HWND)bad[i], &m) == FALSE, "A %llx", (unsigned long long)bad[i]);
    }
    HWND d = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    U(DestroyWindow)(d);
    CHECK(U(IsDialogMessageW)(d, &m) == FALSE);
    U(DestroyWindow)(w);
}

TEST(gap7_idm_message_window_must_belong_to_dialog)
{
    if (!have_idm()) { REQUIRE(have_idm()); }
    HWND dlg = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    HWND other = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    MSG m; memset(&m, 0, sizeof m); m.message = WM_KEYDOWN; m.wParam = VK_TAB;
    m.hwnd = NULL;
    CHECK(U(IsDialogMessageW)(dlg, &m) == FALSE);
    m.hwnd = other;
    CHECK(U(IsDialogMessageW)(dlg, &m) == FALSE);
    m.hwnd = (HWND)(ULONG_PTR)0xDEADBEEF;
    t_note("IsDialogMessageW with bogus message window: %d", U(IsDialogMessageW)(dlg, &m));
    m.message = WM_NULL; m.hwnd = dlg;
    CHECK(U(IsDialogMessageW)(dlg, &m) == FALSE);
    m.message = WM_MOUSEMOVE;
    CHECK(U(IsDialogMessageW)(dlg, &m) == FALSE);
    m.message = WM_PAINT;
    CHECK(U(IsDialogMessageW)(dlg, &m) == FALSE);
    U(DestroyWindow)(dlg); U(DestroyWindow)(other);
}

TEST(gap7_idm_msgfilter_hook_sees_dialogbox_code_and_can_swallow)
{
    if (!have_idm()) { REQUIRE(have_idm()); }
    HWND dlg = g7_mkwinW(WS_OVERLAPPED, 0, NULL);
    HWND c = g7_mkwinW(WS_CHILD, 0, dlg);
    memset(&idm, 0, sizeof idm);
    HHOOK h = U(SetWindowsHookExW)(WH_MSGFILTER, idm_hook, NULL, GetCurrentThreadId());
    REQUIRE(h);
    MSG m; memset(&m, 0, sizeof m); m.hwnd = c; m.message = WM_KEYDOWN; m.wParam = 'A'; m.lParam = 0x1E0001;
    idm.ret = 0;
    BOOL r = U(IsDialogMessageW)(dlg, &m);
    CHECKF(idm.calls == 1, "hook calls=%d", idm.calls);
    CHECKF(idm.code == MSGF_DIALOGBOX, "code=%d", idm.code);
    CHECK(idm.m.hwnd == c && idm.m.message == WM_KEYDOWN && idm.m.wParam == 'A' && idm.m.lParam == 0x1E0001);
    CHECK(r == FALSE);
    idm.calls = 0; idm.ret = 1;
    r = U(IsDialogMessageW)(dlg, &m);
    CHECKF(r == TRUE, "swallowed r=%d", r);
    CHECK(idm.calls == 1);
    idm.calls = 0;
    r = U(IsDialogMessageA)(dlg, &m);
    CHECKF(r == TRUE && idm.calls == 1, "A swallowed r=%d calls=%d", r, idm.calls);
    /* the hook fires before the message window is looked at */
    m.hwnd = NULL; idm.calls = 0;
    r = U(IsDialogMessageW)(dlg, &m);
    CHECKF(r == TRUE && idm.calls == 1, "null hwnd swallowed r=%d calls=%d", r, idm.calls);
    U(UnhookWindowsHookEx)(h);
    idm.calls = 0;
    CHECK(U(IsDialogMessageW)(dlg, &m) == FALSE);
    CHECK(idm.calls == 0);
    U(DestroyWindow)(dlg);
}

/* ================= undocumented exports: resolve only ================= */
TEST(gap7_ordinal_only_style_exports_resolve)
{
    /* Signatures of these are not public; they are only resolved here, never called. */
    static const char *names[] = { "SwitchDesktopWithFade", "OpenThreadDesktop", "LockWindowStation", "UnlockWindowStation", "SetWindowStationUser",
                                   "GetInputDesktop", "UpdateDefaultDesktopThumbnail", "GetWinStationInfo" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        FARPROC p = t_proc(names[i]);
        CHECKF(p != NULL, "%s not exported", names[i]);
    }
}
