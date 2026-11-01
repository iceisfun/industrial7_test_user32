/* window stations: open/create/close/enum/process station */
#include "wsp.h"

static int wsp_lstreq(const char *a, const char *b) { return lstrcmpiA(a, b) == 0; }

static BOOL get_name_a(HANDLE h, char *buf, DWORD n) {
    DWORD need = 0;
    buf[0] = 0;
    return U(GetUserObjectInformationA)(h, UOI_NAME, buf, n, &need);
}
static BOOL get_name_w(HANDLE h, WCHAR *buf, DWORD nbytes) {
    DWORD need = 0;
    buf[0] = 0;
    return U(GetUserObjectInformationW)(h, UOI_NAME, buf, nbytes, &need);
}
static BOOL h_inherit(HANDLE h) { DWORD f = 0; GetHandleInformation(h, &f); return (f & HANDLE_FLAG_INHERIT) != 0; }

typedef struct { int n, found, stopat; LPARAM lp; BOOL lpok; char want[128]; WCHAR wwant[128]; } enumctx;
static BOOL CALLBACK enum_a(LPSTR name, LPARAM lp) {
    enumctx *c = (enumctx *)lp; c->n++;
    if (c->want[0] && wsp_lstreq(name, c->want)) c->found++;
    return !(c->stopat && c->n >= c->stopat);
}
static BOOL CALLBACK enum_w(LPWSTR name, LPARAM lp) {
    enumctx *c = (enumctx *)lp; c->n++;
    if (c->wwant[0] && !lstrcmpiW(name, c->wwant)) c->found++;
    return !(c->stopat && c->n >= c->stopat);
}
static int ws_enum_count_a(const char *name) {
    enumctx c; memset(&c, 0, sizeof c);
    lstrcpynA(c.want, name, sizeof c.want);
    U(EnumWindowStationsA)(enum_a, (LPARAM)&c);
    return c.found;
}
static int dt_enum_count_a(HWINSTA ws, const char *name) {
    enumctx c; memset(&c, 0, sizeof c);
    lstrcpynA(c.want, name, sizeof c.want);
    U(EnumDesktopsA)(ws, enum_a, (LPARAM)&c);
    return c.found;
}

TEST(winsta_process_getprocwinsta_stable) {
    REQUIRE(U(GetProcessWindowStation));
    HWINSTA a = U(GetProcessWindowStation)(), b = U(GetProcessWindowStation)();
    CHECK(a != NULL);
    EXPECT_EQ(a, b);
    for (int i = 0; i < 100; i++) if (U(GetProcessWindowStation)() != a) { CHECK(0); break; }
}
TEST(winsta_process_getprocwinsta_type_a) {
    REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(GetUserObjectInformationA));
    char b[64] = ""; DWORD need = 0;
    CHECK(U(GetUserObjectInformationA)(U(GetProcessWindowStation)(), UOI_TYPE, b, sizeof b, &need));
    EXPECT_STR(b, "WindowStation");
    EXPECT_EQ(need, 14);
}
TEST(winsta_process_getprocwinsta_type_w) {
    REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(GetUserObjectInformationW));
    WCHAR b[64]; DWORD need = 0;
    CHECK(U(GetUserObjectInformationW)(U(GetProcessWindowStation)(), UOI_TYPE, b, sizeof b, &need));
    EXPECT_WSTR(b, L"WindowStation");
    EXPECT_EQ(need, 28);
}
TEST(winsta_process_getprocwinsta_name_parity) {
    REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(GetUserObjectInformationA)); REQUIRE(U(GetUserObjectInformationW));
    HWINSTA ws = U(GetProcessWindowStation)();
    char a[128]; WCHAR w[128]; char conv[128];
    CHECK(get_name_a(ws, a, sizeof a));
    CHECK(get_name_w(ws, w, sizeof w));
    WideCharToMultiByte(CP_ACP, 0, w, -1, conv, sizeof conv, NULL, NULL);
    EXPECT_STR(a, conv);
    CHECK(a[0] != 0);
    if (!wsp_lstreq(a, "WinSta0")) t_note("process window station is '%s', not WinSta0", a);
}
TEST(winsta_process_closeprocwinsta_fails) {
    REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(CloseWindowStation));
    HWINSTA ws = U(GetProcessWindowStation)();
    SetLastError(0);
    CHECK(!U(CloseWindowStation)(ws));
    t_note("CloseWindowStation(process winsta) last error %lu", GetLastError());
    EXPECT_EQ(U(GetProcessWindowStation)(), ws);
    char b[64];
    CHECK(get_name_a(ws, b, sizeof b));   /* still usable */
}
static DWORD wk_getwinsta(void *p) { *(HWINSTA *)p = U(GetProcessWindowStation)(); return 0; }
TEST(winsta_process_getprocwinsta_worker_same) {
    REQUIRE(U(GetProcessWindowStation));
    HWINSTA w = NULL; DWORD r;
    CHECK(wsp_run(wk_getwinsta, &w, 5000, &r));
    EXPECT_EQ(w, U(GetProcessWindowStation)());
}
TEST(winsta_process_openwinsta_a_current) {
    REQUIRE(U(OpenWindowStationA)); REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(CloseWindowStation));
    char n[128], n2[128];
    get_name_a(U(GetProcessWindowStation)(), n, sizeof n);
    HWINSTA h = U(OpenWindowStationA)(n, FALSE, WINSTA_READATTRIBUTES);
    CHECK(h != NULL);
    if (h) {
        EXPECT_NE(h, U(GetProcessWindowStation)());
        CHECK(get_name_a(h, n2, sizeof n2));
        CHECK(wsp_lstreq(n, n2));
        CHECK(U(CloseWindowStation)(h));
    }
}
TEST(winsta_process_openwinsta_w_current) {
    REQUIRE(U(OpenWindowStationW)); REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(CloseWindowStation));
    WCHAR n[128], n2[128];
    get_name_w(U(GetProcessWindowStation)(), n, sizeof n);
    HWINSTA h = U(OpenWindowStationW)(n, FALSE, WINSTA_READATTRIBUTES);
    CHECK(h != NULL);
    if (h) {
        CHECK(get_name_w(h, n2, sizeof n2));
        CHECK(!lstrcmpiW(n, n2));
        CHECK(U(CloseWindowStation)(h));
    }
}
TEST(winsta_process_openwinsta_case_insensitive) {
    REQUIRE(U(OpenWindowStationA)); REQUIRE(U(GetProcessWindowStation));
    char n[128], up[128], lo[128];
    get_name_a(U(GetProcessWindowStation)(), n, sizeof n);
    for (int i = 0; ; i++) { up[i] = (char)toupper((unsigned char)n[i]); lo[i] = (char)tolower((unsigned char)n[i]); if (!n[i]) break; }
    HWINSTA a = U(OpenWindowStationA)(up, FALSE, WINSTA_READATTRIBUTES);
    HWINSTA b = U(OpenWindowStationA)(lo, FALSE, WINSTA_READATTRIBUTES);
    CHECK(a != NULL); CHECK(b != NULL);
    if (a) U(CloseWindowStation)(a);
    if (b) U(CloseWindowStation)(b);
}
TEST(winsta_process_openwinsta_nonexistent_a) {
    REQUIRE(U(OpenWindowStationA));
    SetLastError(0);
    CHECK(U(OpenWindowStationA)("t32wsp_no_such_station", FALSE, WINSTA_ALL_ACCESS) == NULL);
    EXPECT_ERR(0, ERROR_FILE_NOT_FOUND);
}
TEST(winsta_process_openwinsta_nonexistent_w) {
    REQUIRE(U(OpenWindowStationW));
    SetLastError(0);
    CHECK(U(OpenWindowStationW)(L"t32wsp_no_such_station", FALSE, WINSTA_ALL_ACCESS) == NULL);
    EXPECT_ERR(0, ERROR_FILE_NOT_FOUND);
}
TEST(winsta_process_openwinsta_empty_name) {
    REQUIRE(U(OpenWindowStationA)); REQUIRE(U(OpenWindowStationW));
    SetLastError(0);
    CHECK(U(OpenWindowStationA)("", FALSE, WINSTA_ALL_ACCESS) == NULL);
    EXPECT_ERR(0, ERROR_FILE_NOT_FOUND);
    SetLastError(0);
    CHECK(U(OpenWindowStationW)(L"", FALSE, WINSTA_ALL_ACCESS) == NULL);
    EXPECT_ERR(0, ERROR_FILE_NOT_FOUND);
}
TEST(winsta_process_openwinsta_backslash) {
    REQUIRE(U(OpenWindowStationA)); REQUIRE(U(OpenWindowStationW));
    SetLastError(0);
    CHECK(U(OpenWindowStationA)("foo\\bar", FALSE, WINSTA_ALL_ACCESS) == NULL);
    EXPECT_ERR(0, ERROR_PATH_NOT_FOUND);
    SetLastError(0);
    CHECK(U(OpenWindowStationW)(L"foo\\bar", FALSE, WINSTA_ALL_ACCESS) == NULL);
    EXPECT_ERR(0, ERROR_PATH_NOT_FOUND);
}
TEST(winsta_process_openwinsta_inherit_flag) {
    REQUIRE(U(OpenWindowStationA)); REQUIRE(U(OpenWindowStationW)); REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(GetUserObjectInformationA));
    char n[128]; WCHAR nw[128];
    get_name_a(U(GetProcessWindowStation)(), n, sizeof n);
    get_name_w(U(GetProcessWindowStation)(), nw, sizeof nw);
    for (int inh = 0; inh < 2; inh++) {
        HWINSTA a = U(OpenWindowStationA)(n, inh, WINSTA_READATTRIBUTES);
        HWINSTA w = U(OpenWindowStationW)(nw, inh, WINSTA_READATTRIBUTES);
        CHECK(a && w);
        if (a) { EXPECT_EQ(h_inherit(a), inh); WSP_UOF f; DWORD nd;
            CHECK(U(GetUserObjectInformationA)(a, UOI_FLAGS, &f, sizeof f, &nd)); EXPECT_EQ(f.fInherit, inh); EXPECT_EQ(nd, sizeof f);
            U(CloseWindowStation)(a); }
        if (w) { EXPECT_EQ(h_inherit(w), inh); U(CloseWindowStation)(w); }
    }
}
TEST(winsta_process_createwinsta_a_lifecycle) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(OpenWindowStationA)); REQUIRE(U(CloseWindowStation)); REQUIRE(U(EnumWindowStationsA));
    char n[96], got[128];
    wsp_name(n, sizeof n, "wsA");
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_note("CreateWindowStationA failed, err %lu", GetLastError()); t_skip("cannot create window station"); return; }
    CHECK(get_name_a(h, got, sizeof got)); EXPECT_STR(got, n);
    CHECK(ws_enum_count_a(n) == 1);
    HWINSTA o = U(OpenWindowStationA)(n, FALSE, WINSTA_ALL_ACCESS);
    CHECK(o != NULL); EXPECT_NE(o, h);
    CHECK(U(CloseWindowStation)(h));
    /* still alive: second handle open */
    char got2[128];
    CHECK(o && get_name_a(o, got2, sizeof got2));
    CHECK(U(CloseWindowStation)(o));
    /* last handle gone, station is destroyed */
    SetLastError(0);
    HWINSTA gone = U(OpenWindowStationA)(n, FALSE, WINSTA_ALL_ACCESS);
    CHECK(gone == NULL);
    if (gone) U(CloseWindowStation)(gone);
    EXPECT_ERR(0, ERROR_FILE_NOT_FOUND);
    CHECK(ws_enum_count_a(n) == 0);
}
TEST(winsta_process_createwinsta_w_lifecycle) {
    REQUIRE(U(CreateWindowStationW)); REQUIRE(U(OpenWindowStationW)); REQUIRE(U(CloseWindowStation));
    WCHAR n[96], got[128];
    wsp_namew(n, 96, "wsW");
    HWINSTA h = U(CreateWindowStationW)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create window station: %lu", GetLastError()); return; }
    CHECK(get_name_w(h, got, sizeof got)); CHECK(!lstrcmpW(got, n));
    HWINSTA o = U(OpenWindowStationW)(n, TRUE, WINSTA_READATTRIBUTES);
    CHECK(o != NULL);
    if (o) U(CloseWindowStation)(o);
    CHECK(U(CloseWindowStation)(h));
    CHECK(U(OpenWindowStationW)(n, TRUE, WINSTA_READATTRIBUTES) == NULL);
}
TEST(winsta_process_createwinsta_type_and_flags) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(GetUserObjectInformationA)); REQUIRE(U(CloseWindowStation));
    char n[96]; wsp_name(n, sizeof n, "wsT");
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create window station"); return; }
    char t[64] = ""; DWORD nd = 0; WSP_UOF f = { 7, 7, 7 };
    CHECK(U(GetUserObjectInformationA)(h, UOI_TYPE, t, sizeof t, &nd)); EXPECT_STR(t, "WindowStation");
    CHECK(U(GetUserObjectInformationA)(h, UOI_FLAGS, &f, sizeof f, &nd));
    EXPECT_EQ(f.fInherit, 0);
    CHECK(!(f.dwFlags & WSF_VISIBLE));   /* only the interactive station is visible */
    EXPECT_EQ(h_inherit(h), 0);
    U(CloseWindowStation)(h);
}
TEST(winsta_process_createwinsta_inherit_sa) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(GetUserObjectInformationA)); REQUIRE(U(CloseWindowStation));
    char n[96]; wsp_name(n, sizeof n, "wsI");
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, &sa);
    if (!h) { t_skip("cannot create window station"); return; }
    EXPECT_EQ(h_inherit(h), 1);
    WSP_UOF f; DWORD nd;
    CHECK(U(GetUserObjectInformationA)(h, UOI_FLAGS, &f, sizeof f, &nd));
    EXPECT_EQ(f.fInherit, 1);
    U(CloseWindowStation)(h);
}
TEST(winsta_process_createwinsta_existing_name) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(CloseWindowStation));
    char n[96], g1[128], g2[128]; wsp_name(n, sizeof n, "wsE");
    HWINSTA a = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!a) { t_skip("cannot create window station"); return; }
    SetLastError(0);
    HWINSTA b = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    DWORD le = GetLastError();
    CHECK(b != NULL);
    t_note("second CreateWindowStation on existing name: handle %p err %lu", b, le);
    if (b) {
        EXPECT_NE(a, b);
        get_name_a(a, g1, sizeof g1); get_name_a(b, g2, sizeof g2);
        EXPECT_STR(g1, g2);
        CHECK(U(CloseWindowStation)(b));
    }
    CHECK(U(CloseWindowStation)(a));
}
TEST(winsta_process_createwinsta_backslash) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(CreateWindowStationW));
    SetLastError(0);
    HWINSTA h = U(CreateWindowStationA)("foo\\bar", 0, WSP_WINSTA_ALL, NULL);
    CHECK(h == NULL);
    DWORD le = GetLastError();
    CHECKF(le == ERROR_PATH_NOT_FOUND || le == ERROR_ACCESS_DENIED || le == ERROR_BAD_PATHNAME, "err %lu", le);
    SetLastError(0);
    h = U(CreateWindowStationW)(L"foo\\bar", 0, WSP_WINSTA_ALL, NULL);
    CHECK(h == NULL);
    le = GetLastError();
    CHECKF(le == ERROR_PATH_NOT_FOUND || le == ERROR_ACCESS_DENIED || le == ERROR_BAD_PATHNAME, "err %lu", le);
}
TEST(winsta_process_createwinsta_unnamed) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(CloseWindowStation));
    HWINSTA h = U(CreateWindowStationA)(NULL, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_note("CreateWindowStation(NULL) failed err %lu", GetLastError()); t_skip("unnamed winsta not creatable"); return; }
    char n[128]; CHECK(get_name_a(h, n, sizeof n));
    CHECK(n[0] != 0);
    t_note("unnamed window station is named '%s'", n);
    CHECK(U(CloseWindowStation)(h));
}
TEST(winsta_process_closewinsta_invalid_handles) {
    REQUIRE(U(CloseWindowStation));
    SetLastError(0);
    CHECK(!U(CloseWindowStation)(NULL));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(CloseWindowStation)((HWINSTA)INVALID_HANDLE_VALUE));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(CloseWindowStation)((HWINSTA)(ULONG_PTR)0x7ffc));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_closewinsta_twice) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(CloseWindowStation));
    char n[96]; wsp_name(n, sizeof n, "wsC");
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create window station"); return; }
    CHECK(U(CloseWindowStation)(h));
    SetLastError(0);
    CHECK(!U(CloseWindowStation)(h));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_closewinsta_wrong_object_type) {
    REQUIRE(U(CloseWindowStation)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetProcessWindowStation));
    HDESK d = U(GetThreadDesktop)(GetCurrentThreadId());
    SetLastError(0);
    CHECK(!U(CloseWindowStation)((HWINSTA)d));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(CloseDesktop)((HDESK)U(GetProcessWindowStation)()));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CHECK(!U(CloseWindowStation)((HWINSTA)ev));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    CHECK(CloseHandle(ev));
}
TEST(winsta_process_closewinsta_via_closehandle) {
    REQUIRE(U(CreateWindowStationA));
    char n[96]; wsp_name(n, sizeof n, "wsH");
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create window station"); return; }
    CHECK(CloseHandle(h));
    CHECK(U(OpenWindowStationA)(n, FALSE, WINSTA_READATTRIBUTES) == NULL);
}
TEST(winsta_process_setprocwinsta_roundtrip) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(SetProcessWindowStation)); REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(CloseWindowStation));
    HWINSTA orig = U(GetProcessWindowStation)();
    char n[96]; wsp_name(n, sizeof n, "wsS");
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create window station"); return; }
    CHECK(U(SetProcessWindowStation)(h));
    HWINSTA cur = U(GetProcessWindowStation)();
    EXPECT_EQ(cur, h);
    SetLastError(0);
    CHECK(!U(CloseWindowStation)(h));                     /* in use as the process station */
    CHECK(U(SetProcessWindowStation)(orig));
    EXPECT_EQ(U(GetProcessWindowStation)(), orig);
    CHECK(U(CloseWindowStation)(h));
}
TEST(winsta_process_setprocwinsta_invalid) {
    REQUIRE(U(SetProcessWindowStation)); REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(GetThreadDesktop));
    HWINSTA orig = U(GetProcessWindowStation)();
    SetLastError(0);
    CHECK(!U(SetProcessWindowStation)(NULL));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(SetProcessWindowStation)((HWINSTA)(ULONG_PTR)0x7ffc));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(SetProcessWindowStation)((HWINSTA)INVALID_HANDLE_VALUE));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    CHECK(!U(SetProcessWindowStation)((HWINSTA)U(GetThreadDesktop)(GetCurrentThreadId())));
    EXPECT_EQ(U(GetProcessWindowStation)(), orig);
}
TEST(winsta_process_setprocwinsta_same) {
    REQUIRE(U(SetProcessWindowStation)); REQUIRE(U(GetProcessWindowStation));
    HWINSTA orig = U(GetProcessWindowStation)();
    CHECK(U(SetProcessWindowStation)(orig));
    EXPECT_EQ(U(GetProcessWindowStation)(), orig);
}
TEST(winsta_process_setprocwinsta_atoms_per_station) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(SetProcessWindowStation)); REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(CloseWindowStation));
    HWINSTA orig = U(GetProcessWindowStation)();
    char n[96]; wsp_name(n, sizeof n, "wsAt");
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create window station"); return; }
    ATOM a1 = GlobalAddAtomA("t32wsp_atom_probe");
    CHECK(a1 != 0);
    CHECK(U(SetProcessWindowStation)(h));
    char buf[64] = "";
    UINT r = GlobalGetAtomNameA(a1, buf, sizeof buf);   /* atom table belongs to the winsta */
    /* Win10: the global atom table is not switched by SetProcessWindowStation; the atom
     * still resolves from the new station (observed), so no per-station isolation here */
    t_note("atom resolved after SetProcessWindowStation: %u '%s'", r, buf);
    CHECK(r == 17 && !strcmp(buf, "t32wsp_atom_probe"));
    ATOM a2 = GlobalAddAtomA("t32wsp_atom_probe2");
    CHECK(a2 != 0);
    CHECK(U(SetProcessWindowStation)(orig));
    CHECK(GlobalGetAtomNameA(a1, buf, sizeof buf) == 17);
    EXPECT_STR(buf, "t32wsp_atom_probe");
    GlobalDeleteAtom(a1);
    CHECK(U(CloseWindowStation)(h));
}
TEST(winsta_process_enumwinstations_a_contains_current) {
    REQUIRE(U(EnumWindowStationsA)); REQUIRE(U(GetProcessWindowStation));
    enumctx c; memset(&c, 0, sizeof c);
    get_name_a(U(GetProcessWindowStation)(), c.want, sizeof c.want);
    SetLastError(0);
    CHECK(U(EnumWindowStationsA)(enum_a, (LPARAM)&c));
    CHECK(c.n >= 1);
    EXPECT_EQ(c.found, 1);
}
TEST(winsta_process_enumwinstations_w_contains_current) {
    REQUIRE(U(EnumWindowStationsW)); REQUIRE(U(GetProcessWindowStation));
    enumctx c; memset(&c, 0, sizeof c);
    get_name_w(U(GetProcessWindowStation)(), c.wwant, sizeof c.wwant);
    CHECK(U(EnumWindowStationsW)(enum_w, (LPARAM)&c));
    CHECK(c.n >= 1);
    EXPECT_EQ(c.found, 1);
}
TEST(winsta_process_enumwinstations_ab_parity) {
    REQUIRE(U(EnumWindowStationsA)); REQUIRE(U(EnumWindowStationsW));
    enumctx a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    U(EnumWindowStationsA)(enum_a, (LPARAM)&a);
    U(EnumWindowStationsW)(enum_w, (LPARAM)&w);
    EXPECT_EQ(a.n, w.n);
}
TEST(winsta_process_enumwinstations_stop_early) {
    REQUIRE(U(EnumWindowStationsA));
    enumctx c; memset(&c, 0, sizeof c); c.stopat = 1;
    U(EnumWindowStationsA)(enum_a, (LPARAM)&c);
    EXPECT_EQ(c.n, 1);
}
static BOOL CALLBACK enum_lparam(LPSTR n, LPARAM lp) { enumctx *c = (enumctx *)lp; c->lp = lp; c->lpok = (lp == (LPARAM)c); c->n++; return TRUE; }
TEST(winsta_process_enumwinstations_lparam) {
    REQUIRE(U(EnumWindowStationsA));
    enumctx c; memset(&c, 0, sizeof c);
    U(EnumWindowStationsA)(enum_lparam, (LPARAM)&c);
    CHECK(c.n >= 1); CHECK(c.lpok);
}
TEST(winsta_process_enumwinstations_sees_created) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(EnumWindowStationsW)); REQUIRE(U(CloseWindowStation));
    WCHAR n[96]; wsp_namew(n, 96, "wsEn");
    HWINSTA h = U(CreateWindowStationW)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create window station"); return; }
    enumctx c; memset(&c, 0, sizeof c); lstrcpyW(c.wwant, n);
    U(EnumWindowStationsW)(enum_w, (LPARAM)&c);
    EXPECT_EQ(c.found, 1);
    U(CloseWindowStation)(h);
    memset(&c, 0, sizeof c); lstrcpyW(c.wwant, n);
    U(EnumWindowStationsW)(enum_w, (LPARAM)&c);
    EXPECT_EQ(c.found, 0);
}
TEST(winsta_process_enumdesktops_a_default_present) {
    REQUIRE(U(EnumDesktopsA)); REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetUserObjectInformationA));
    char dn[128]; get_name_a(U(GetThreadDesktop)(GetCurrentThreadId()), dn, sizeof dn);
    enumctx c; memset(&c, 0, sizeof c); lstrcpyA(c.want, dn);
    CHECK(U(EnumDesktopsA)(U(GetProcessWindowStation)(), enum_a, (LPARAM)&c));
    CHECK(c.n >= 1);
    EXPECT_EQ(c.found, 1);
}
TEST(winsta_process_enumdesktops_w_default_present) {
    REQUIRE(U(EnumDesktopsW)); REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(GetThreadDesktop));
    WCHAR dn[128]; get_name_w(U(GetThreadDesktop)(GetCurrentThreadId()), dn, sizeof dn);
    enumctx c; memset(&c, 0, sizeof c); lstrcpyW(c.wwant, dn);
    CHECK(U(EnumDesktopsW)(U(GetProcessWindowStation)(), enum_w, (LPARAM)&c));
    EXPECT_EQ(c.found, 1);
}
TEST(winsta_process_enumdesktops_ab_parity) {
    REQUIRE(U(EnumDesktopsA)); REQUIRE(U(EnumDesktopsW)); REQUIRE(U(GetProcessWindowStation));
    enumctx a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    U(EnumDesktopsA)(U(GetProcessWindowStation)(), enum_a, (LPARAM)&a);
    U(EnumDesktopsW)(U(GetProcessWindowStation)(), enum_w, (LPARAM)&w);
    EXPECT_EQ(a.n, w.n);
}
TEST(winsta_process_enumdesktops_stop_early) {
    REQUIRE(U(EnumDesktopsA)); REQUIRE(U(GetProcessWindowStation));
    enumctx c; memset(&c, 0, sizeof c); c.stopat = 1;
    U(EnumDesktopsA)(U(GetProcessWindowStation)(), enum_a, (LPARAM)&c);
    EXPECT_EQ(c.n, 1);
}
TEST(winsta_process_enumdesktops_invalid_winsta) {
    REQUIRE(U(EnumDesktopsA)); REQUIRE(U(EnumDesktopsW));
    enumctx c; memset(&c, 0, sizeof c);
    SetLastError(0);
    CHECK(!U(EnumDesktopsA)((HWINSTA)(ULONG_PTR)0x7ffc, enum_a, (LPARAM)&c));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(EnumDesktopsW)((HWINSTA)INVALID_HANDLE_VALUE, enum_w, (LPARAM)&c));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    EXPECT_EQ(c.n, 0);
}
TEST(winsta_process_enumdesktops_null_winsta) {
    REQUIRE(U(EnumDesktopsA));
    enumctx c; memset(&c, 0, sizeof c);
    SetLastError(0);
    BOOL r = U(EnumDesktopsA)(NULL, enum_a, (LPARAM)&c);
    t_note("EnumDesktopsA(NULL winsta): ret %d count %d err %lu", r, c.n, GetLastError());
}
TEST(winsta_process_enumdesktops_empty_new_winsta) {
    REQUIRE(U(CreateWindowStationA)); REQUIRE(U(EnumDesktopsA)); REQUIRE(U(CloseWindowStation));
    char n[96]; wsp_name(n, sizeof n, "wsEd");
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create window station"); return; }
    enumctx c; memset(&c, 0, sizeof c);
    U(EnumDesktopsA)(h, enum_a, (LPARAM)&c);
    EXPECT_EQ(c.n, 0);
    U(CloseWindowStation)(h);
}
TEST(winsta_process_lockwinsta_noop_missing) {
    /* exported but we never call Lock/Unlock: just verify exports exist */
    CHECK(t_proc("LockWindowStation") != NULL);
    CHECK(t_proc("UnlockWindowStation") != NULL);
    CHECK(t_proc("SetWindowStationUser") != NULL);
}
