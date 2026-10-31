#include "wc.h"

/* Property fixture: a plain top-level popup window */
struct pfx { char cls[64]; HWND h; ATOM atom; };
static int pfx_make(struct pfx *f) {
    memset(f, 0, sizeof *f);
    wc_uname(f->cls, sizeof f->cls, "wcPr");
    f->atom = wc_regA(f->cls, 0, 0, 0, NULL);
    if (!f->atom) return 0;
    f->h = wc_createA(f->cls, WS_POPUP, NULL);
    return f->h != NULL;
}
static void pfx_free(struct pfx *f) {
    if (f->h && U(IsWindow)(f->h)) U(DestroyWindow)(f->h);
    if (f->atom) U(UnregisterClassA)(f->cls, wc_inst());
}

/* ---- enumeration collectors ---- */
#define MAXE 300
struct ecol {
    int n;
    HWND hwnd[MAXE];
    ULONG_PTR key[MAXE];      /* string pointer value as passed */
    int isatom[MAXE];
    char nameA[MAXE][64];
    WCHAR nameW[MAXE][64];
    HANDLE data[MAXE];
    ULONG_PTR lp[MAXE];
    int calls;                /* every callback invocation, including foreign props */
    int stop_after;           /* 0 = never */
    int retval;               /* value returned by the callback */
    int remove_all;           /* remove every prop while enumerating */
};
static struct ecol g_e;

/* Entries not named "wc*" are foreign (set by the system); count them as calls only. */
#define FOREIGN_A(s) ((((ULONG_PTR)(s)) >> 16) != 0 && strncmp((const char *)(s), "wc", 2) && ((const char *)(s))[0] != '#')
#define FOREIGN_W(s) ((((ULONG_PTR)(s)) >> 16) != 0 && !(((const WCHAR *)(s))[0] == 'w' && ((const WCHAR *)(s))[1] == 'c') && ((const WCHAR *)(s))[0] != '#')
#define PRE(foreign) g_e.calls++; if (foreign) return (g_e.stop_after && g_e.calls >= g_e.stop_after) ? 0 : g_e.retval;
static void e_reset(void) { memset(&g_e, 0, sizeof g_e); g_e.retval = 1; }

static BOOL CALLBACK enumA(HWND h, LPCSTR s, HANDLE d) {
    PRE(FOREIGN_A(s))
    int i = g_e.n++;
    if (i >= MAXE) return FALSE;
    g_e.hwnd[i] = h; g_e.key[i] = (ULONG_PTR)s; g_e.data[i] = d;
    if (((ULONG_PTR)s >> 16) == 0) g_e.isatom[i] = 1; else lstrcpynA(g_e.nameA[i], s, 64);
    if (g_e.remove_all) U(RemovePropA)(h, s);
    if (g_e.stop_after && g_e.calls >= g_e.stop_after) return 0;
    return g_e.retval;
}
static BOOL CALLBACK enumW(HWND h, LPCWSTR s, HANDLE d) {
    PRE(FOREIGN_W(s))
    int i = g_e.n++;
    if (i >= MAXE) return FALSE;
    g_e.hwnd[i] = h; g_e.key[i] = (ULONG_PTR)s; g_e.data[i] = d;
    if (((ULONG_PTR)s >> 16) == 0) g_e.isatom[i] = 1; else lstrcpynW(g_e.nameW[i], s, 64);
    if (g_e.stop_after && g_e.calls >= g_e.stop_after) return 0;
    return g_e.retval;
}
static BOOL CALLBACK enumExA(HWND h, LPSTR s, HANDLE d, ULONG_PTR lp) {
    PRE(FOREIGN_A(s))
    int i = g_e.n++;
    if (i >= MAXE) return FALSE;
    g_e.hwnd[i] = h; g_e.key[i] = (ULONG_PTR)s; g_e.data[i] = d; g_e.lp[i] = lp;
    if (((ULONG_PTR)s >> 16) == 0) g_e.isatom[i] = 1; else lstrcpynA(g_e.nameA[i], s, 64);
    if (g_e.stop_after && g_e.calls >= g_e.stop_after) return 0;
    return g_e.retval;
}
static BOOL CALLBACK enumExW(HWND h, LPWSTR s, HANDLE d, ULONG_PTR lp) {
    PRE(FOREIGN_W(s))
    int i = g_e.n++;
    if (i >= MAXE) return FALSE;
    g_e.hwnd[i] = h; g_e.key[i] = (ULONG_PTR)s; g_e.data[i] = d; g_e.lp[i] = lp;
    if (((ULONG_PTR)s >> 16) == 0) g_e.isatom[i] = 1; else lstrcpynW(g_e.nameW[i], s, 64);
    if (g_e.stop_after && g_e.calls >= g_e.stop_after) return 0;
    return g_e.retval;
}
static int find_nameA(const char *n) {
    for (int i = 0; i < g_e.n && i < MAXE; i++) if (!g_e.isatom[i] && !lstrcmpiA(g_e.nameA[i], n)) return i;
    return -1;
}
static int find_nameW(const WCHAR *n) {
    for (int i = 0; i < g_e.n && i < MAXE; i++) if (!g_e.isatom[i] && !lstrcmpiW(g_e.nameW[i], n)) return i;
    return -1;
}
static int find_atom(unsigned a) {
    for (int i = 0; i < g_e.n && i < MAXE; i++) if (g_e.isatom[i] && g_e.key[i] == a) return i;
    return -1;
}

static void uprop(char *buf, size_t n, const char *base) { wc_uname(buf, n, base); }

/* ---------- basic set/get ---------- */

TEST(window_class_prop_set_get_ansi) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcProp");
    SetLastError(MAGIC_ERR);
    CHECK(U(SetPropA)(f.h, n, (HANDLE)0x1234) == TRUE);
    EQ(U(GetPropA)(f.h, n), 0x1234);
    EQ(U(RemovePropA)(f.h, n), 0x1234);
    pfx_free(&f);
}

TEST(window_class_prop_set_get_wide) {
    REQUIRE(U(SetPropW)); REQUIRE(U(GetPropW)); REQUIRE(U(RemovePropW));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; WCHAR w[64]; uprop(n, sizeof n, "wcPropW"); wc_widen(w, n);
    CHECK(U(SetPropW)(f.h, w, (HANDLE)0x5678) == TRUE);
    EQ(U(GetPropW)(f.h, w), 0x5678);
    EQ(U(RemovePropW)(f.h, w), 0x5678);
    EQ(U(GetPropW)(f.h, w), 0);
    pfx_free(&f);
}

TEST(window_class_prop_a_w_parity) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropW)); REQUIRE(U(SetPropW)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(RemovePropW));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; WCHAR w[64]; uprop(n, sizeof n, "wcParity"); wc_widen(w, n);
    CHECK(U(SetPropA)(f.h, n, (HANDLE)0x11));
    EQ(U(GetPropW)(f.h, w), 0x11);
    CHECK(U(SetPropW)(f.h, w, (HANDLE)0x22));   /* same property: replaces */
    EQ(U(GetPropA)(f.h, n), 0x22);
    EQ(U(RemovePropW)(f.h, w), 0x22);
    EQ(U(GetPropA)(f.h, n), 0);
    CHECK(U(SetPropW)(f.h, w, (HANDLE)0x33));
    EQ(U(RemovePropA)(f.h, n), 0x33);
    EQ(U(GetPropW)(f.h, w), 0);
    pfx_free(&f);
}

TEST(window_class_prop_case_insensitive) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64], up[64]; uprop(n, sizeof n, "wcCaseProp");
    strcpy(up, n); for (char *p = up; *p; p++) *p = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
    CHECK(strcmp(n, up));
    CHECK(U(SetPropA)(f.h, n, (HANDLE)9));
    EQ(U(GetPropA)(f.h, up), 9);
    CHECK(U(SetPropA)(f.h, up, (HANDLE)10));
    EQ(U(GetPropA)(f.h, n), 10);
    e_reset();
    U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 1);
    EQ(U(RemovePropA)(f.h, up), 10);
    EQ(U(GetPropA)(f.h, n), 0);
    pfx_free(&f);
}

TEST(window_class_prop_missing_returns_null) {
    REQUIRE(U(GetPropA)); REQUIRE(U(GetPropW)); REQUIRE(U(RemovePropA)); REQUIRE(U(RemovePropW));
    struct pfx f; CHECK(pfx_make(&f));
    EQ(U(GetPropA)(f.h, "wcNeverSetProp_xyz"), 0);
    EQ(U(GetPropW)(f.h, L"wcNeverSetProp_xyz"), 0);
    EQ(U(RemovePropA)(f.h, "wcNeverSetProp_xyz"), 0);
    EQ(U(RemovePropW)(f.h, L"wcNeverSetProp_xyz"), 0);
    EQ(U(GetPropA)(f.h, MAKEINTATOM(0x2222)), 0);
    EQ(U(RemovePropA)(f.h, MAKEINTATOM(0x2222)), 0);
    pfx_free(&f);
}

TEST(window_class_prop_64bit_value) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcProp64");
    static const ULONG_PTR vals[] = { 1, 0x7fffffff, 0x80000000, 0xffffffff, 0x100000000ull, 0x1122334455667788ull, 0xffffffffffffffffull, 0x8000000000000000ull };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        CHECK(U(SetPropA)(f.h, n, (HANDLE)vals[i]));
        EQ(U(GetPropA)(f.h, n), vals[i]);
    }
    EQ(U(RemovePropA)(f.h, n), 0x8000000000000000ull);
    pfx_free(&f);
}

TEST(window_class_prop_null_data) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(EnumPropsA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcPropNull");
    SetLastError(MAGIC_ERR);
    BOOL r = U(SetPropA)(f.h, n, NULL);
    t_note("SetPropA with NULL data -> %d err %lu", r, GetLastError());
    EQ(U(GetPropA)(f.h, n), 0);
    e_reset(); U(EnumPropsA)(f.h, enumA);
    t_note("props listed after NULL-data SetProp: %d", g_e.n);
    U(RemovePropA)(f.h, n);
    pfx_free(&f);
}

TEST(window_class_prop_overwrite) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(EnumPropsA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcPropOvr");
    for (int i = 1; i <= 10; i++) { CHECK(U(SetPropA)(f.h, n, (HANDLE)(ULONG_PTR)(i * 100))); EQ(U(GetPropA)(f.h, n), i * 100); }
    e_reset(); U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 1);
    EQ(g_e.data[0], 1000);
    EQ(U(RemovePropA)(f.h, n), 1000);
    e_reset(); U(EnumPropsA)(f.h, enumA); EQ(g_e.n, 0);
    pfx_free(&f);
}

TEST(window_class_prop_independent_windows) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    HWND h2 = wc_createA(f.cls, WS_POPUP, NULL);
    CHECK(h2);
    char n[64]; uprop(n, sizeof n, "wcPropIndep");
    CHECK(U(SetPropA)(f.h, n, (HANDLE)1));
    EQ(U(GetPropA)(h2, n), 0);
    CHECK(U(SetPropA)(h2, n, (HANDLE)2));
    EQ(U(GetPropA)(f.h, n), 1);
    EQ(U(GetPropA)(h2, n), 2);
    EQ(U(RemovePropA)(f.h, n), 1);
    EQ(U(GetPropA)(h2, n), 2);
    U(RemovePropA)(h2, n);
    U(DestroyWindow)(h2);
    pfx_free(&f);
}

TEST(window_class_prop_not_inherited_by_child) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    HWND c = U(CreateWindowExA)(0, f.cls, "c", WS_CHILD, 0, 0, 10, 10, f.h, (HMENU)1, wc_inst(), NULL);
    CHECK(c);
    char n[64]; uprop(n, sizeof n, "wcPropChild");
    U(SetPropA)(f.h, n, (HANDLE)5);
    EQ(U(GetPropA)(c, n), 0);
    U(RemovePropA)(f.h, n);
    pfx_free(&f);
}

/* ---------- atoms ---------- */

TEST(window_class_prop_global_atom_roundtrip) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcPropAtom");
    ATOM a = GlobalAddAtomA(n);
    CHECK(a >= 0xC000);
    CHECK(U(SetPropA)(f.h, MAKEINTATOM(a), (HANDLE)0xA7));
    EQ(U(GetPropA)(f.h, MAKEINTATOM(a)), 0xA7);
    EQ(U(GetPropA)(f.h, n), 0xA7);                 /* by name too */
    CHECK(U(SetPropA)(f.h, n, (HANDLE)0xA8));      /* by name replaces */
    EQ(U(GetPropA)(f.h, MAKEINTATOM(a)), 0xA8);
    WCHAR w[64]; wc_widen(w, n);
    EQ(U(GetPropW)(f.h, w), 0xA8);
    EQ(U(GetPropW)(f.h, MAKEINTATOM(a)), 0xA8);
    EQ(U(RemovePropA)(f.h, MAKEINTATOM(a)), 0xA8);
    EQ(U(GetPropA)(f.h, n), 0);
    pfx_free(&f);
    GlobalDeleteAtom(a);
}

TEST(window_class_prop_integer_atom) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(EnumPropsA));
    struct pfx f; CHECK(pfx_make(&f));
    static const unsigned atoms[] = { 1, 2, 0x100, 0x1234, 0xBFFF };
    for (unsigned i = 0; i < 5; i++) {
        SetLastError(MAGIC_ERR);
        BOOL r = U(SetPropA)(f.h, MAKEINTATOM(atoms[i]), (HANDLE)(ULONG_PTR)(0x1000 + i));
        CHECKF(r, "atom 0x%x err %lu", atoms[i], GetLastError());
    }
    for (unsigned i = 0; i < 5; i++) EQ(U(GetPropA)(f.h, MAKEINTATOM(atoms[i])), 0x1000 + i);
    e_reset(); U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 5);
    for (unsigned i = 0; i < 5; i++) {
        int k = find_atom(atoms[i]);
        CHECKF(k >= 0, "atom 0x%x enumerated as integer", atoms[i]);
        if (k >= 0) EQ(g_e.data[k], 0x1000 + i);
    }
    for (unsigned i = 0; i < 5; i++) EQ(U(RemovePropA)(f.h, MAKEINTATOM(atoms[i])), 0x1000 + i);
    pfx_free(&f);
}

TEST(window_class_prop_hash_string_is_integer_atom) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    EQ(GlobalAddAtomA("#4321"), 4321);
    GlobalDeleteAtom(4321);
    CHECK(U(SetPropA)(f.h, "#4321", (HANDLE)0xBEEF));
    EQ(U(GetPropA)(f.h, MAKEINTATOM(4321)), 0xBEEF);
    EQ(U(GetPropW)(f.h, L"#4321"), 0xBEEF);
    EQ(U(RemovePropA)(f.h, MAKEINTATOM(4321)), 0xBEEF);
    pfx_free(&f);
}

TEST(window_class_prop_name_creates_global_atom) {
    REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcPropGA");
    EQ(GlobalFindAtomA(n), 0);
    CHECK(U(SetPropA)(f.h, n, (HANDLE)1));
    ATOM a = GlobalFindAtomA(n);
    CHECK(a >= 0xC000);
    char buf[64]; UINT l = GlobalGetAtomNameA(a, buf, 64);
    EQ(l, strlen(n));
    EXPECT_STR(buf, n);
    U(RemovePropA)(f.h, n);
    t_note("global atom after RemoveProp: %u", GlobalFindAtomA(n));
    pfx_free(&f);
    t_note("global atom after DestroyWindow: %u", GlobalFindAtomA(n));
}

TEST(window_class_prop_get_does_not_create_atom) {
    REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcPropNoAtom");
    EQ(U(GetPropA)(f.h, n), 0);
    EQ(U(RemovePropA)(f.h, n), 0);
    EQ(GlobalFindAtomA(n), 0);
    pfx_free(&f);
}

/* ---------- invalid parameters ---------- */

TEST(window_class_prop_bad_hwnd) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(SetPropW)); REQUIRE(U(GetPropW)); REQUIRE(U(RemovePropW));
    static const HWND bad[] = { NULL, (HWND)1, (HWND)0x12345678, (HWND)0x20000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(MAGIC_ERR);
        CHECKF(U(SetPropA)(bad[i], "wcBadHwndProp", (HANDLE)1) == FALSE, "set %p", bad[i]);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(SetPropW)(bad[i], L"wcBadHwndProp", (HANDLE)1) == FALSE);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(GetPropA)(bad[i], "wcBadHwndProp") == NULL);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(GetPropW)(bad[i], L"wcBadHwndProp") == NULL);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        SetLastError(MAGIC_ERR);
        CHECK(U(GetPropA)(bad[i], MAKEINTATOM(0x300)) == NULL);
        EXPECT_LAST_ERR(ERR_INVALID_HWND);
        CHECK(U(RemovePropA)(bad[i], "wcBadHwndProp") == NULL);
        CHECK(U(RemovePropW)(bad[i], L"wcBadHwndProp") == NULL);
        CHECK(U(RemovePropA)(bad[i], MAKEINTATOM(0x300)) == NULL);
    }
}

TEST(window_class_prop_destroyed_window) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(EnumPropsA));
    struct pfx f; CHECK(pfx_make(&f));
    HWND h = f.h;
    char n[64]; uprop(n, sizeof n, "wcPropDead");
    CHECK(U(SetPropA)(h, n, (HANDLE)3));
    U(DestroyWindow)(h);
    SetLastError(MAGIC_ERR);
    CHECK(U(GetPropA)(h, n) == NULL);
    EXPECT_LAST_ERR(ERR_INVALID_HWND);
    SetLastError(MAGIC_ERR);
    CHECK(U(SetPropA)(h, n, (HANDLE)4) == FALSE);
    EXPECT_LAST_ERR(ERR_INVALID_HWND);
    CHECK(U(RemovePropA)(h, n) == NULL);
    e_reset();
    EQ(U(EnumPropsA)(h, enumA), -1);
    EQ(g_e.n, 0);
    pfx_free(&f);
}

TEST(window_class_prop_name_length_limits) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(SetPropW)); REQUIRE(U(GetPropW)); REQUIRE(U(RemovePropW));
    struct pfx f; CHECK(pfx_make(&f));
    static const int lens[] = { 1, 2, 100, 254, 255, 256, 257, 300, 1000, 5000 };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        int len = lens[i];
        char *a = malloc(len + 1); WCHAR *w = malloc((len + 1) * 2);
        for (int k = 0; k < len; k++) { a[k] = 'a' + (k % 26); w[k] = a[k]; }
        a[len] = 0; w[len] = 0;
        char tag[12]; snprintf(tag, sizeof tag, "%lx_", (unsigned long)GetCurrentProcessId());
        for (int k = 0; tag[k] && k < len; k++) { a[k] = tag[k]; w[k] = tag[k]; }
        SetLastError(MAGIC_ERR);
        BOOL ra = U(SetPropA)(f.h, a, (HANDLE)(ULONG_PTR)len);
        DWORD ea = GetLastError();
        if (len <= 255) {
            CHECKF(ra, "len %d should work (err %lu)", len, ea);
            EQ(U(GetPropA)(f.h, a), len);
            EQ(U(GetPropW)(f.h, w), len);
            EQ(U(RemovePropW)(f.h, w), len);
        } else if (len > 256) {
            CHECKF(!ra, "len %d should fail", len);
            t_note("SetPropA len %d err %lu", len, ea);
            CHECK(U(GetPropA)(f.h, a) == NULL);
        } else {
            t_note("SetPropA len 256 -> %d err %lu", ra, ea);
        }
        if (ra) U(RemovePropA)(f.h, a);
        SetLastError(MAGIC_ERR);
        BOOL rw = U(SetPropW)(f.h, w, (HANDLE)5);
        if (len <= 255) CHECKF(rw, "W len %d", len); else if (len > 256) CHECKF(!rw, "W len %d should fail", len);
        if (rw) U(RemovePropW)(f.h, w);
        free(a); free(w);
    }
    pfx_free(&f);
}

TEST(window_class_prop_empty_name) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    SetLastError(MAGIC_ERR);
    BOOL r = U(SetPropA)(f.h, "", (HANDLE)1);
    CHECK(r == FALSE);       /* GlobalAddAtom of an empty string fails */
    t_note("SetPropA(\"\") err %lu", GetLastError());
    EQ(U(GetPropA)(f.h, ""), 0);
    pfx_free(&f);
}

TEST(window_class_prop_atom_zero) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    SetLastError(MAGIC_ERR);
    BOOL r = U(SetPropA)(f.h, MAKEINTATOM(0), (HANDLE)1);
    t_note("SetPropA(atom 0) -> %d err %lu", r, GetLastError());
    if (r) U(RemovePropA)(f.h, MAKEINTATOM(0));
    pfx_free(&f);
}

TEST(window_class_prop_many) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(EnumPropsA));
    struct pfx f; CHECK(pfx_make(&f));
    enum { N = 200 };
    static char names[N][40];
    int okc = 0;
    for (int i = 0; i < N; i++) { snprintf(names[i], 40, "wcMany%lx_%d", (unsigned long)GetCurrentProcessId(), i); if (U(SetPropA)(f.h, names[i], (HANDLE)(ULONG_PTR)(i + 1))) okc++; }
    EQ(okc, N);
    int good = 0;
    for (int i = 0; i < N; i++) if ((ULONG_PTR)U(GetPropA)(f.h, names[i]) == (ULONG_PTR)(i + 1)) good++;
    EQ(good, N);
    e_reset(); U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, N);
    for (int i = 0; i < N; i++) EQ(U(RemovePropA)(f.h, names[i]), i + 1);
    e_reset(); U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 0);
    pfx_free(&f);
}

/* ---------- enumeration ---------- */

TEST(window_class_enumprops_empty) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(EnumPropsW)); REQUIRE(U(EnumPropsExA)); REQUIRE(U(EnumPropsExW));
    struct pfx f; CHECK(pfx_make(&f));
    int r;
    /* result is -1 exactly when the callback was never invoked, else the last callback result */
    e_reset(); r = U(EnumPropsA)(f.h, enumA);   EQ(r, g_e.calls ? 1 : -1); EQ(g_e.n, 0);
    e_reset(); r = U(EnumPropsW)(f.h, enumW);   EQ(r, g_e.calls ? 1 : -1); EQ(g_e.n, 0);
    e_reset(); r = U(EnumPropsExA)(f.h, enumExA, 5); EQ(r, g_e.calls ? 1 : -1); EQ(g_e.n, 0);
    e_reset(); r = U(EnumPropsExW)(f.h, enumExW, 5); EQ(r, g_e.calls ? 1 : -1); EQ(g_e.n, 0);
    t_note("system-owned properties on a fresh window: %d", g_e.calls);
    pfx_free(&f);
}

TEST(window_class_enumprops_bad_hwnd) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(EnumPropsW)); REQUIRE(U(EnumPropsExA)); REQUIRE(U(EnumPropsExW));
    static const HWND bad[] = { NULL, (HWND)1, (HWND)0x12345678 };
    for (unsigned i = 0; i < 3; i++) {
        e_reset(); EQ(U(EnumPropsA)(bad[i], enumA), -1); EQ(g_e.n, 0);
        e_reset(); EQ(U(EnumPropsW)(bad[i], enumW), -1); EQ(g_e.n, 0);
        e_reset(); EQ(U(EnumPropsExA)(bad[i], enumExA, 1), -1); EQ(g_e.n, 0);
        e_reset(); EQ(U(EnumPropsExW)(bad[i], enumExW, 1), -1); EQ(g_e.n, 0);
    }
}

TEST(window_class_enumprops_a_names_and_data) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n1[64], n2[64], n3[64]; uprop(n1, sizeof n1, "wcEnA1"); uprop(n2, sizeof n2, "wcEnA2"); uprop(n3, sizeof n3, "wcEnA3");
    CHECK(U(SetPropA)(f.h, n1, (HANDLE)0x11));
    CHECK(U(SetPropA)(f.h, n2, (HANDLE)0x22));
    CHECK(U(SetPropA)(f.h, n3, (HANDLE)0x33));
    e_reset();
    int r = U(EnumPropsA)(f.h, enumA);
    EQ(r, 1);          /* last callback result */
    EQ(g_e.n, 3);
    int k1 = find_nameA(n1), k2 = find_nameA(n2), k3 = find_nameA(n3);
    CHECK(k1 >= 0 && k2 >= 0 && k3 >= 0);
    if (k1 >= 0) EQ(g_e.data[k1], 0x11);
    if (k2 >= 0) EQ(g_e.data[k2], 0x22);
    if (k3 >= 0) EQ(g_e.data[k3], 0x33);
    for (int i = 0; i < g_e.n; i++) EQ(g_e.hwnd[i], (ULONG_PTR)f.h);
    U(RemovePropA)(f.h, n1); U(RemovePropA)(f.h, n2); U(RemovePropA)(f.h, n3);
    pfx_free(&f);
}

TEST(window_class_enumprops_w_names_and_data) {
    REQUIRE(U(EnumPropsW)); REQUIRE(U(SetPropW)); REQUIRE(U(RemovePropW));
    struct pfx f; CHECK(pfx_make(&f));
    char n1[64], n2[64]; WCHAR w1[64], w2[64]; uprop(n1, sizeof n1, "wcEnW1"); uprop(n2, sizeof n2, "wcEnW2");
    wc_widen(w1, n1); wc_widen(w2, n2);
    CHECK(U(SetPropW)(f.h, w1, (HANDLE)0x41));
    CHECK(U(SetPropW)(f.h, w2, (HANDLE)0x42));
    e_reset();
    EQ(U(EnumPropsW)(f.h, enumW), 1);
    EQ(g_e.n, 2);
    int k1 = find_nameW(w1), k2 = find_nameW(w2);
    CHECK(k1 >= 0 && k2 >= 0);
    if (k1 >= 0) EQ(g_e.data[k1], 0x41);
    if (k2 >= 0) EQ(g_e.data[k2], 0x42);
    U(RemovePropW)(f.h, w1); U(RemovePropW)(f.h, w2);
    pfx_free(&f);
}

TEST(window_class_enumprops_a_vs_w_on_same_props) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(EnumPropsW)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; WCHAR w[64]; uprop(n, sizeof n, "wcEnAW"); wc_widen(w, n);
    CHECK(U(SetPropA)(f.h, n, (HANDLE)7));
    e_reset(); U(EnumPropsA)(f.h, enumA);
    CHECK(find_nameA(n) >= 0);
    e_reset(); U(EnumPropsW)(f.h, enumW);
    CHECK(find_nameW(w) >= 0);
    U(RemovePropA)(f.h, n);
    pfx_free(&f);
}

TEST(window_class_enumprops_ex_lparam) {
    REQUIRE(U(EnumPropsExA)); REQUIRE(U(EnumPropsExW)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n1[64], n2[64]; uprop(n1, sizeof n1, "wcExL1"); uprop(n2, sizeof n2, "wcExL2");
    CHECK(U(SetPropA)(f.h, n1, (HANDLE)0x51));
    CHECK(U(SetPropA)(f.h, n2, (HANDLE)0x52));
    e_reset();
    EQ(U(EnumPropsExA)(f.h, enumExA, (LPARAM)0x1122334455667788LL), 1);
    EQ(g_e.n, 2);
    for (int i = 0; i < g_e.n; i++) EQ(g_e.lp[i], 0x1122334455667788ULL);
    CHECK(find_nameA(n1) >= 0 && find_nameA(n2) >= 0);
    e_reset();
    EQ(U(EnumPropsExW)(f.h, enumExW, (LPARAM)-1), 1);
    EQ(g_e.n, 2);
    for (int i = 0; i < g_e.n; i++) EQ(g_e.lp[i], (ULONG_PTR)-1);
    e_reset();
    EQ(U(EnumPropsExA)(f.h, enumExA, 0), 1);
    EQ(g_e.lp[0], 0);
    U(RemovePropA)(f.h, n1); U(RemovePropA)(f.h, n2);
    pfx_free(&f);
}

TEST(window_class_enumprops_stop_on_zero) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(EnumPropsExA)); REQUIRE(U(EnumPropsW)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[5][48];
    for (int i = 0; i < 5; i++) { uprop(n[i], 48, "wcStop"); CHECK(U(SetPropA)(f.h, n[i], (HANDLE)(ULONG_PTR)(i + 1))); }
    e_reset(); g_e.stop_after = 2;
    EQ(U(EnumPropsA)(f.h, enumA), 0);
    EQ(g_e.calls, 2);
    e_reset(); g_e.stop_after = 1;
    EQ(U(EnumPropsExA)(f.h, enumExA, 0), 0);
    EQ(g_e.calls, 1);
    e_reset(); g_e.stop_after = 4;
    EQ(U(EnumPropsW)(f.h, enumW), 0);
    EQ(g_e.calls, 4);
    for (int i = 0; i < 5; i++) U(RemovePropA)(f.h, n[i]);
    pfx_free(&f);
}

TEST(window_class_enumprops_return_values) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcEnRet");
    CHECK(U(SetPropA)(f.h, n, (HANDLE)1));
    static const int rv[] = { 1, 2, 77, -5, 0x7fffffff };
    for (unsigned i = 0; i < 5; i++) {
        e_reset(); g_e.retval = rv[i];
        EQ((LONG)U(EnumPropsA)(f.h, enumA), (LONG)rv[i]);
    }
    U(RemovePropA)(f.h, n);
    pfx_free(&f);
}

TEST(window_class_enumprops_removal_in_callback) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[6][48];
    for (int i = 0; i < 6; i++) { uprop(n[i], 48, "wcEnRm"); CHECK(U(SetPropA)(f.h, n[i], (HANDLE)(ULONG_PTR)(i + 1))); }
    e_reset(); g_e.remove_all = 1;
    U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 6);       /* the list is a snapshot */
    e_reset(); U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 0);
    pfx_free(&f);
}

TEST(window_class_enumprops_mixed_atoms_and_names) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(EnumPropsW)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcMix");
    CHECK(U(SetPropA)(f.h, n, (HANDLE)0xA));
    CHECK(U(SetPropA)(f.h, MAKEINTATOM(0x77), (HANDLE)0xB));
    e_reset(); U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 2);
    int kn = find_nameA(n), ka = find_atom(0x77);
    CHECK(kn >= 0); CHECK(ka >= 0);
    if (kn >= 0) EQ(g_e.data[kn], 0xA);
    if (ka >= 0) EQ(g_e.data[ka], 0xB);
    e_reset(); U(EnumPropsW)(f.h, enumW);
    EQ(g_e.n, 2);
    CHECK(find_atom(0x77) >= 0);
    U(RemovePropA)(f.h, n); U(RemovePropA)(f.h, MAKEINTATOM(0x77));
    pfx_free(&f);
}

TEST(window_class_enumprops_only_this_window) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    HWND h2 = wc_createA(f.cls, WS_POPUP, NULL);
    char n1[64], n2[64]; uprop(n1, sizeof n1, "wcOnly1"); uprop(n2, sizeof n2, "wcOnly2");
    U(SetPropA)(f.h, n1, (HANDLE)1); U(SetPropA)(h2, n2, (HANDLE)2);
    e_reset(); U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 1); CHECK(find_nameA(n1) == 0);
    e_reset(); U(EnumPropsA)(h2, enumA);
    EQ(g_e.n, 1); CHECK(find_nameA(n2) == 0);
    U(RemovePropA)(f.h, n1); U(RemovePropA)(h2, n2);
    U(DestroyWindow)(h2);
    pfx_free(&f);
}

TEST(window_class_enumprops_data_pointer_sized) {
    REQUIRE(U(EnumPropsA)); REQUIRE(U(SetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcEnBig");
    U(SetPropA)(f.h, n, (HANDLE)0xFEDCBA9876543210ull);
    e_reset(); U(EnumPropsA)(f.h, enumA);
    EQ(g_e.n, 1);
    EQ(g_e.data[0], 0xFEDCBA9876543210ull);
    U(RemovePropA)(f.h, n);
    pfx_free(&f);
}

/* ---------- threads ---------- */

struct pth { HWND h; char name[64]; HANDLE got; BOOL set; HANDLE removed; };
static DWORD WINAPI pth_set(void *p) {
    struct pth *t = p;
    t->set = U(SetPropA)(t->h, t->name, (HANDLE)0x7777);
    t->got = U(GetPropA)(t->h, t->name);
    return 0;
}
static DWORD WINAPI pth_remove(void *p) {
    struct pth *t = p;
    t->removed = U(RemovePropA)(t->h, t->name);
    return 0;
}

TEST(window_class_prop_cross_thread) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    struct pfx f; CHECK(pfx_make(&f));
    struct pth t; memset(&t, 0, sizeof t); t.h = f.h; uprop(t.name, sizeof t.name, "wcPropThr");
    HANDLE th = CreateThread(NULL, 0, pth_set, &t, 0, NULL);
    CHECK(th && WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    if (th) CloseHandle(th);
    CHECK(t.set);
    EQ(t.got, 0x7777);
    EQ(U(GetPropA)(f.h, t.name), 0x7777);
    th = CreateThread(NULL, 0, pth_remove, &t, 0, NULL);
    CHECK(th && WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    if (th) CloseHandle(th);
    EQ(t.removed, 0x7777);
    EQ(U(GetPropA)(f.h, t.name), 0);
    pfx_free(&f);
}

/* ---------- misc ---------- */

TEST(window_class_prop_survives_window_text_and_style_changes) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA)); REQUIRE(U(SetWindowTextA)); REQUIRE(U(ShowWindow));
    struct pfx f; CHECK(pfx_make(&f));
    char n[64]; uprop(n, sizeof n, "wcPropKeep");
    U(SetPropA)(f.h, n, (HANDLE)0x99);
    U(SetWindowTextA)(f.h, "hello");
    U(ShowWindow)(f.h, SW_SHOWNOACTIVATE);
    U(ShowWindow)(f.h, SW_HIDE);
    EQ(U(GetPropA)(f.h, n), 0x99);
    U(RemovePropA)(f.h, n);
    pfx_free(&f);
}

static int g_ncdestroy_seen;
static HANDLE g_ncdestroy_prop;
static char g_ncd_name[64];
static LRESULT CALLBACK ncd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCDESTROY) { g_ncdestroy_seen++; g_ncdestroy_prop = U(GetPropA)(h, g_ncd_name); }
    return U(DefWindowProcA)(h, m, w, l);
}

TEST(window_class_prop_available_in_wm_ncdestroy) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA));
    char cls[64]; wc_uname(cls, sizeof cls, "wcNcd"); uprop(g_ncd_name, sizeof g_ncd_name, "wcNcdProp");
    CHECK(wc_regA(cls, 0, 0, 0, ncd_proc));
    HWND h = wc_createA(cls, WS_POPUP, NULL);
    CHECK(h);
    CHECK(U(SetPropA)(h, g_ncd_name, (HANDLE)0x5A));
    g_ncdestroy_seen = 0; g_ncdestroy_prop = NULL;
    CHECK(U(DestroyWindow)(h));
    EQ(g_ncdestroy_seen, 1);
    EQ(g_ncdestroy_prop, 0x5A);
    CHECK(U(UnregisterClassA)(cls, wc_inst()));
}

static char g_pc_name[64];
static LRESULT CALLBACK pc_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) U(SetPropA)(h, g_pc_name, (HANDLE)0x42);
    return U(DefWindowProcA)(h, m, w, l);
}

TEST(window_class_prop_set_during_create) {
    REQUIRE(U(SetPropA)); REQUIRE(U(GetPropA)); REQUIRE(U(RemovePropA));
    uprop(g_pc_name, sizeof g_pc_name, "wcPropCreate");
    char cls[64]; wc_uname(cls, sizeof cls, "wcPcCls");
    CHECK(wc_regA(cls, 0, 0, 0, pc_proc));
    HWND h = wc_createA(cls, WS_POPUP, NULL);
    CHECK(h);
    if (h) {
        EQ(U(GetPropA)(h, g_pc_name), 0x42);
        U(DestroyWindow)(h);
    }
    CHECK(U(UnregisterClassA)(cls, wc_inst()));
}
