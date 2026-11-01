/* desktops: open/create/close/thread desktop/input desktop */
#include "wsp.h"

static BOOL name_a(HANDLE h, char *buf, DWORD n) { DWORD need; buf[0] = 0; return U(GetUserObjectInformationA)(h, UOI_NAME, buf, n, &need); }
static BOOL name_w(HANDLE h, WCHAR *buf, DWORD nbytes) { DWORD need; buf[0] = 0; return U(GetUserObjectInformationW)(h, UOI_NAME, buf, nbytes, &need); }
static BOOL inh(HANDLE h) { DWORD f = 0; GetHandleInformation(h, &f); return (f & HANDLE_FLAG_INHERIT) != 0; }

typedef struct { int n, found; char want[128]; } dctx;
static BOOL CALLBACK dcb(LPSTR s, LPARAM lp) { dctx *c = (dctx *)lp; c->n++; if (!lstrcmpiA(s, c->want)) c->found++; return TRUE; }
static int desk_listed(const char *n) {
    dctx c; memset(&c, 0, sizeof c); lstrcpyA(c.want, n);
    U(EnumDesktopsA)(U(GetProcessWindowStation)(), dcb, (LPARAM)&c);
    return c.found;
}
static HDESK mk(const char *tag, char *nbuf) {
    wsp_name(nbuf, 96, tag);
    return U(CreateDesktopA)(nbuf, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
}
#define NEED_DESK() do { REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetUserObjectInformationA)); REQUIRE(U(OpenDesktopA)); } while (0)

TEST(winsta_process_getthreaddesktop_stable) {
    REQUIRE(U(GetThreadDesktop));
    HDESK a = U(GetThreadDesktop)(GetCurrentThreadId());
    CHECK(a != NULL);
    EXPECT_EQ(a, U(GetThreadDesktop)(GetCurrentThreadId()));
}
TEST(winsta_process_getthreaddesktop_type) {
    REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetUserObjectInformationA)); REQUIRE(U(GetUserObjectInformationW));
    HDESK d = U(GetThreadDesktop)(GetCurrentThreadId());
    char a[32]; WCHAR w[32]; DWORD nd = 0;
    CHECK(U(GetUserObjectInformationA)(d, UOI_TYPE, a, sizeof a, &nd)); EXPECT_STR(a, "Desktop"); EXPECT_EQ(nd, 8);
    CHECK(U(GetUserObjectInformationW)(d, UOI_TYPE, w, sizeof w, &nd)); EXPECT_WSTR(w, L"Desktop"); EXPECT_EQ(nd, 16);
}
TEST(winsta_process_getthreaddesktop_name) {
    REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetUserObjectInformationA));
    char a[64]; CHECK(name_a(U(GetThreadDesktop)(GetCurrentThreadId()), a, sizeof a));
    CHECK(a[0] != 0);
    if (lstrcmpiA(a, "Default")) t_note("thread desktop is '%s', not Default", a);
}
TEST(winsta_process_getthreaddesktop_invalid_tid) {
    REQUIRE(U(GetThreadDesktop));
    SetLastError(0);
    CHECK(U(GetThreadDesktop)(0) == NULL);
    t_note("GetThreadDesktop(0) err %lu", GetLastError());
    SetLastError(0);
    CHECK(U(GetThreadDesktop)(0xfffffff0) == NULL);
    t_note("GetThreadDesktop(bogus) err %lu", GetLastError());
    SetLastError(0);
    CHECK(U(GetThreadDesktop)((DWORD)-1) == NULL);
}
TEST(winsta_process_getthreaddesktop_not_closeable) {
    REQUIRE(U(GetThreadDesktop)); REQUIRE(U(CloseDesktop));
    HDESK d = U(GetThreadDesktop)(GetCurrentThreadId());
    SetLastError(0);
    CHECK(!U(CloseDesktop)(d));
    t_note("CloseDesktop(thread desktop) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!CloseHandle(d));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    EXPECT_EQ(U(GetThreadDesktop)(GetCurrentThreadId()), d);
}
TEST(winsta_process_getthreaddesktop_duplicate_handle) {
    REQUIRE(U(GetThreadDesktop)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetUserObjectInformationA));
    HDESK d = U(GetThreadDesktop)(GetCurrentThreadId()), d2 = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), d, GetCurrentProcess(), (PHANDLE)&d2, 0, FALSE, DUPLICATE_SAME_ACCESS));
    if (d2) {
        EXPECT_NE(d2, d);
        char a[64], b[64];
        CHECK(name_a(d, a, sizeof a)); CHECK(name_a(d2, b, sizeof b)); EXPECT_STR(a, b);
        CHECK(U(CloseDesktop)(d2));
    }
}
TEST(winsta_process_opendesktop_default_a) {
    REQUIRE(U(OpenDesktopA)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetUserObjectInformationA));
    char n[64]; name_a(U(GetThreadDesktop)(GetCurrentThreadId()), n, sizeof n);
    HDESK d = U(OpenDesktopA)(n, 0, FALSE, DESKTOP_READOBJECTS);
    CHECK(d != NULL);
    if (d) {
        char g[64]; CHECK(name_a(d, g, sizeof g)); EXPECT_STR(g, n);
        EXPECT_NE(d, U(GetThreadDesktop)(GetCurrentThreadId()));
        CHECK(U(CloseDesktop)(d));
    }
}
TEST(winsta_process_opendesktop_default_w) {
    REQUIRE(U(OpenDesktopW)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetUserObjectInformationW));
    WCHAR n[64]; name_w(U(GetThreadDesktop)(GetCurrentThreadId()), n, sizeof n);
    HDESK d = U(OpenDesktopW)(n, 0, FALSE, DESKTOP_READOBJECTS);
    CHECK(d != NULL);
    if (d) { WCHAR g[64]; CHECK(name_w(d, g, sizeof g)); CHECK(!lstrcmpiW(g, n)); CHECK(U(CloseDesktop)(d)); }
}
TEST(winsta_process_opendesktop_case_insensitive) {
    REQUIRE(U(OpenDesktopA)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetUserObjectInformationA));
    char n[64], u[64]; name_a(U(GetThreadDesktop)(GetCurrentThreadId()), n, sizeof n);
    for (int i = 0;; i++) { u[i] = (char)toupper((unsigned char)n[i]); if (!n[i]) break; }
    HDESK d = U(OpenDesktopA)(u, 0, FALSE, DESKTOP_READOBJECTS);
    CHECK(d != NULL);
    if (d) U(CloseDesktop)(d);
}
TEST(winsta_process_opendesktop_nonexistent) {
    REQUIRE(U(OpenDesktopA)); REQUIRE(U(OpenDesktopW));
    SetLastError(0);
    CHECK(U(OpenDesktopA)("t32wsp_no_such_desktop", 0, FALSE, WSP_DESKTOP_ALL) == NULL);
    t_note("OpenDesktopA(missing) err %lu", GetLastError());
    EXPECT_ERR(0, ERROR_FILE_NOT_FOUND);
    SetLastError(0);
    CHECK(U(OpenDesktopW)(L"t32wsp_no_such_desktop", 0, FALSE, WSP_DESKTOP_ALL) == NULL);
}
TEST(winsta_process_opendesktop_empty_name) {
    REQUIRE(U(OpenDesktopA)); REQUIRE(U(OpenDesktopW));
    SetLastError(0);
    CHECK(U(OpenDesktopA)("", 0, TRUE, WSP_DESKTOP_ALL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(U(OpenDesktopW)(L"", 0, TRUE, WSP_DESKTOP_ALL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_opendesktop_backslash) {
    REQUIRE(U(OpenDesktopA)); REQUIRE(U(OpenDesktopW));
    SetLastError(0);
    CHECK(U(OpenDesktopA)("foo\\bar", 0, TRUE, WSP_DESKTOP_ALL) == NULL);
    EXPECT_ERR(0, ERROR_BAD_PATHNAME);
    SetLastError(0);
    CHECK(U(OpenDesktopW)(L"foo\\bar", 0, TRUE, WSP_DESKTOP_ALL) == NULL);
    EXPECT_ERR(0, ERROR_BAD_PATHNAME);
    SetLastError(0);
    CHECK(U(OpenDesktopA)("\\Default", 0, TRUE, WSP_DESKTOP_ALL) == NULL);
    EXPECT_ERR(0, ERROR_BAD_PATHNAME);
}
TEST(winsta_process_opendesktop_inherit_flag) {
    REQUIRE(U(OpenDesktopA)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetUserObjectInformationA));
    char n[64]; name_a(U(GetThreadDesktop)(GetCurrentThreadId()), n, sizeof n);
    for (int i = 0; i < 2; i++) {
        HDESK d = U(OpenDesktopA)(n, 0, i, DESKTOP_READOBJECTS);
        CHECK(d != NULL);
        if (!d) continue;
        EXPECT_EQ(inh(d), i);
        WSP_UOF f = { 9, 9, 9 }; DWORD nd = 0;
        CHECK(U(GetUserObjectInformationA)(d, UOI_FLAGS, &f, sizeof f, &nd));
        EXPECT_EQ(f.fInherit, i);
        EXPECT_EQ(nd, 12);
        U(CloseDesktop)(d);
    }
}
TEST(winsta_process_opendesktop_ex_flags) {
    REQUIRE(U(OpenDesktopA)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetUserObjectInformationA));
    char n[64]; name_a(U(GetThreadDesktop)(GetCurrentThreadId()), n, sizeof n);
    HDESK d = U(OpenDesktopA)(n, DF_ALLOWOTHERACCOUNTHOOK, FALSE, DESKTOP_READOBJECTS);
    t_note("OpenDesktopA(DF_ALLOWOTHERACCOUNTHOOK) -> %p err %lu", d, GetLastError());
    if (d) U(CloseDesktop)(d);
}
TEST(winsta_process_createdesktop_a_lifecycle) {
    NEED_DESK(); REQUIRE(U(EnumDesktopsA)); REQUIRE(U(GetProcessWindowStation));
    char n[96], g[128];
    HDESK d = mk("dA", n);
    if (!d) { t_skip("CreateDesktopA failed %lu", GetLastError()); return; }
    CHECK(name_a(d, g, sizeof g)); EXPECT_STR(g, n);
    EXPECT_EQ(desk_listed(n), 1);
    HDESK o = U(OpenDesktopA)(n, 0, FALSE, WSP_DESKTOP_ALL);
    CHECK(o != NULL); EXPECT_NE(o, d);
    CHECK(U(CloseDesktop)(d));
    CHECK(o && name_a(o, g, sizeof g));
    EXPECT_EQ(desk_listed(n), 1);
    if (o) CHECK(U(CloseDesktop)(o));
    SetLastError(0);
    HDESK gone = U(OpenDesktopA)(n, 0, FALSE, WSP_DESKTOP_ALL);
    CHECK(gone == NULL);
    if (gone) U(CloseDesktop)(gone);
    EXPECT_EQ(desk_listed(n), 0);
}
TEST(winsta_process_createdesktop_w_lifecycle) {
    REQUIRE(U(CreateDesktopW)); REQUIRE(U(OpenDesktopW)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetUserObjectInformationW));
    WCHAR n[96], g[128]; wsp_namew(n, 96, "dW");
    HDESK d = U(CreateDesktopW)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
    if (!d) { t_skip("CreateDesktopW failed %lu", GetLastError()); return; }
    CHECK(name_w(d, g, sizeof g)); CHECK(!lstrcmpW(g, n));
    HDESK o = U(OpenDesktopW)(n, 0, TRUE, DESKTOP_READOBJECTS);
    CHECK(o != NULL);
    if (o) U(CloseDesktop)(o);
    CHECK(U(CloseDesktop)(d));
    CHECK(U(OpenDesktopW)(n, 0, TRUE, DESKTOP_READOBJECTS) == NULL);
}
TEST(winsta_process_createdesktop_type_flags_inherit) {
    NEED_DESK();
    char n[96]; HDESK d = mk("dT", n);
    if (!d) { t_skip("CreateDesktopA failed"); return; }
    char t[32]; DWORD nd; WSP_UOF f = { 5, 5, 5 };
    CHECK(U(GetUserObjectInformationA)(d, UOI_TYPE, t, sizeof t, &nd)); EXPECT_STR(t, "Desktop");
    CHECK(U(GetUserObjectInformationA)(d, UOI_FLAGS, &f, sizeof f, &nd));
    EXPECT_EQ(f.fInherit, 0); EXPECT_EQ(f.dwFlags, 0);
    EXPECT_EQ(inh(d), 0);
    U(CloseDesktop)(d);
}
TEST(winsta_process_createdesktop_inherit_sa) {
    NEED_DESK();
    char n[96]; wsp_name(n, sizeof n, "dI");
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HDESK d = U(CreateDesktopA)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, &sa);
    if (!d) { t_skip("CreateDesktopA failed"); return; }
    EXPECT_EQ(inh(d), 1);
    WSP_UOF f; DWORD nd;
    CHECK(U(GetUserObjectInformationA)(d, UOI_FLAGS, &f, sizeof f, &nd)); EXPECT_EQ(f.fInherit, 1);
    U(CloseDesktop)(d);
}
TEST(winsta_process_createdesktop_existing_name) {
    NEED_DESK();
    char n[96], g1[128], g2[128]; HDESK a = mk("dE", n);
    if (!a) { t_skip("CreateDesktopA failed"); return; }
    SetLastError(0);
    HDESK b = U(CreateDesktopA)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
    t_note("CreateDesktop on existing name -> %p err %lu", b, GetLastError());
    CHECK(b != NULL);
    if (b) { EXPECT_NE(a, b); name_a(a, g1, sizeof g1); name_a(b, g2, sizeof g2); EXPECT_STR(g1, g2); CHECK(U(CloseDesktop)(b)); }
    CHECK(U(CloseDesktop)(a));
}
TEST(winsta_process_createdesktop_empty_name) {
    NEED_DESK(); REQUIRE(U(CreateDesktopW));
    SetLastError(0);
    CHECK(U(CreateDesktopA)("", NULL, NULL, 0, WSP_DESKTOP_ALL, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(U(CreateDesktopW)(L"", NULL, NULL, 0, WSP_DESKTOP_ALL, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_createdesktop_backslash) {
    NEED_DESK(); REQUIRE(U(CreateDesktopW));
    SetLastError(0);
    CHECK(U(CreateDesktopA)("foo\\bar", NULL, NULL, 0, WSP_DESKTOP_ALL, NULL) == NULL);
    EXPECT_ERR(0, ERROR_BAD_PATHNAME);
    SetLastError(0);
    CHECK(U(CreateDesktopW)(L"foo\\bar", NULL, NULL, 0, WSP_DESKTOP_ALL, NULL) == NULL);
    EXPECT_ERR(0, ERROR_BAD_PATHNAME);
}
TEST(winsta_process_createdesktop_bad_flags) {
    NEED_DESK();
    static const DWORD bad[] = { 0x2, 0x4, 0x80, 0x10000, 0x80000000u };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        char n[96]; wsp_name(n, sizeof n, "dF");
        SetLastError(0);
        HDESK d = U(CreateDesktopA)(n, NULL, NULL, bad[i], WSP_DESKTOP_ALL, NULL);
        t_note("CreateDesktop flags 0x%lx -> %p err %lu", (unsigned long)bad[i], d, GetLastError());
        if (d) U(CloseDesktop)(d);
    }
}
TEST(winsta_process_createdesktop_allowotheraccounthook) {
    NEED_DESK();
    char n[96]; wsp_name(n, sizeof n, "dH");
    SetLastError(0);
    HDESK d = U(CreateDesktopA)(n, NULL, NULL, DF_ALLOWOTHERACCOUNTHOOK, WSP_DESKTOP_ALL, NULL);
    t_note("DF_ALLOWOTHERACCOUNTHOOK -> %p err %lu", d, GetLastError());
    if (d) U(CloseDesktop)(d);
}
TEST(winsta_process_createdesktop_long_name) {
    NEED_DESK();
    char n[300]; memset(n, 'x', 259); n[259] = 0;
    char big[300]; wsp_name(big, sizeof big, "dL");
    memcpy(n, big, strlen(big));
    SetLastError(0);
    HDESK d = U(CreateDesktopA)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
    t_note("CreateDesktop 259-char name -> %p err %lu", d, GetLastError());
    if (d) {
        char g[400]; DWORD nd;
        CHECK(U(GetUserObjectInformationA)(d, UOI_NAME, g, sizeof g, &nd));
        EXPECT_STR(g, n);
        U(CloseDesktop)(d);
    }
}
TEST(winsta_process_createdesktopex_a_heapsize) {
    NEED_DESK(); REQUIRE(U(CreateDesktopExA));
    char n[96]; wsp_name(n, sizeof n, "dX");
    SetLastError(0);
    HDESK d = U(CreateDesktopExA)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL, 2048, NULL);
    if (!d) { t_note("CreateDesktopExA err %lu", GetLastError()); t_skip("CreateDesktopExA failed"); return; }
    char g[128]; CHECK(name_a(d, g, sizeof g)); EXPECT_STR(g, n);
    ULONG heap = 0; DWORD nd = 0;
    BOOL r = U(GetUserObjectInformationA)(d, 5 /* UOI_HEAPSIZE */, &heap, sizeof heap, &nd);
    t_note("UOI_HEAPSIZE of desktop created with 2048: ret %d value %lu need %lu", r, heap, nd);
    CHECK(U(CloseDesktop)(d));
}
TEST(winsta_process_createdesktopex_w_heapsize_zero) {
    REQUIRE(U(CreateDesktopExW)); REQUIRE(U(CloseDesktop));
    WCHAR n[96]; wsp_namew(n, 96, "dXW");
    HDESK d = U(CreateDesktopExW)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL, 0, NULL);
    if (!d) { t_skip("CreateDesktopExW failed %lu", GetLastError()); return; }
    CHECK(U(CloseDesktop)(d));
}
TEST(winsta_process_createdesktopex_empty_name) {
    REQUIRE(U(CreateDesktopExA)); REQUIRE(U(CreateDesktopExW));
    SetLastError(0);
    CHECK(U(CreateDesktopExA)("", NULL, NULL, 0, WSP_DESKTOP_ALL, NULL, 0, NULL) == NULL);
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(U(CreateDesktopExW)(L"a\\b", NULL, NULL, 0, WSP_DESKTOP_ALL, NULL, 0, NULL) == NULL);
    EXPECT_ERR(0, ERROR_BAD_PATHNAME);
}
TEST(winsta_process_createdesktopex_huge_heap) {
    REQUIRE(U(CreateDesktopExA)); REQUIRE(U(CloseDesktop));
    char n[96]; wsp_name(n, sizeof n, "dXH");
    SetLastError(0);
    HDESK d = U(CreateDesktopExA)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL, 0x7fffffff, NULL);
    t_note("CreateDesktopEx huge heap -> %p err %lu", d, GetLastError());
    if (d) U(CloseDesktop)(d);
}
TEST(winsta_process_closedesktop_invalid_handles) {
    REQUIRE(U(CloseDesktop));
    SetLastError(0);
    CHECK(!U(CloseDesktop)(NULL));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(CloseDesktop)((HDESK)INVALID_HANDLE_VALUE));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(CloseDesktop)((HDESK)(ULONG_PTR)0x7ffc));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_closedesktop_twice) {
    NEED_DESK();
    char n[96]; HDESK d = mk("dC", n);
    if (!d) { t_skip("CreateDesktopA failed"); return; }
    CHECK(U(CloseDesktop)(d));
    SetLastError(0);
    CHECK(!U(CloseDesktop)(d));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_closedesktop_closehandle_equiv) {
    NEED_DESK();
    char n[96]; HDESK d = mk("dCH", n);
    if (!d) { t_skip("CreateDesktopA failed"); return; }
    /* Win10: desktop handles are protected from CloseHandle (it fails); only CloseDesktop
     * releases them */
    SetLastError(0);
    BOOL ch = CloseHandle(d);
    t_note("CloseHandle(desktop) = %d err %lu", ch, GetLastError());
    CHECK(!ch);
    CHECK(U(CloseDesktop)(d));
    CHECK(U(OpenDesktopA)(n, 0, FALSE, DESKTOP_READOBJECTS) == NULL);
}
TEST(winsta_process_closedesktop_non_user_handle) {
    REQUIRE(U(CloseDesktop));
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    SetLastError(0);
    CHECK(!U(CloseDesktop)((HDESK)ev));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    HANDLE ev2 = ev; DWORD fl = 0;
    CHECK(GetHandleInformation(ev2, &fl));   /* event survived */
    CloseHandle(ev);
}
TEST(winsta_process_opendesktop_many_handles) {
    NEED_DESK();
    char n[96]; HDESK d = mk("dM", n);
    if (!d) { t_skip("CreateDesktopA failed"); return; }
    HDESK hs[64]; int k = 0;
    for (int i = 0; i < 64; i++) { hs[i] = U(OpenDesktopA)(n, 0, FALSE, DESKTOP_READOBJECTS); if (hs[i]) k++; }
    EXPECT_EQ(k, 64);
    for (int i = 0; i < 64; i++) for (int j = i + 1; j < 64; j++) if (hs[i] && hs[i] == hs[j]) CHECK(0);
    for (int i = 0; i < 64; i++) if (hs[i]) CHECK(U(CloseDesktop)(hs[i]));
    CHECK(U(CloseDesktop)(d));
    CHECK(U(OpenDesktopA)(n, 0, FALSE, DESKTOP_READOBJECTS) == NULL);
}
TEST(winsta_process_create_close_cycle) {
    NEED_DESK();
    for (int i = 0; i < 200; i++) {
        char n[96]; HDESK d = mk("dCy", n);
        if (!d) { CHECKF(0, "iteration %d err %lu", i, GetLastError()); break; }
        if (!U(CloseDesktop)(d)) { CHECK(0); break; }
    }
}
TEST(winsta_process_openinputdesktop_basic) {
    REQUIRE(U(OpenInputDesktop)); REQUIRE(U(CloseDesktop)); REQUIRE(U(GetUserObjectInformationA));
    HDESK d = U(OpenInputDesktop)(0, FALSE, DESKTOP_READOBJECTS);
    if (!d) { t_note("OpenInputDesktop err %lu", GetLastError()); t_skip("input desktop not accessible"); return; }
    char t[32], n[64]; DWORD nd;
    CHECK(U(GetUserObjectInformationA)(d, UOI_TYPE, t, sizeof t, &nd)); EXPECT_STR(t, "Desktop");
    CHECK(name_a(d, n, sizeof n));
    t_note("input desktop is '%s'", n);
    EXPECT_EQ(inh(d), 0);
    CHECK(U(CloseDesktop)(d));
}
TEST(winsta_process_openinputdesktop_inherit_and_flags) {
    REQUIRE(U(OpenInputDesktop)); REQUIRE(U(CloseDesktop));
    HDESK d = U(OpenInputDesktop)(0, TRUE, DESKTOP_READOBJECTS);
    if (!d) { t_skip("input desktop not accessible"); return; }
    EXPECT_EQ(inh(d), 1);
    U(CloseDesktop)(d);
    d = U(OpenInputDesktop)(DF_ALLOWOTHERACCOUNTHOOK, FALSE, DESKTOP_READOBJECTS);
    t_note("OpenInputDesktop(DF_ALLOWOTHERACCOUNTHOOK) -> %p err %lu", d, GetLastError());
    if (d) U(CloseDesktop)(d);
    SetLastError(0);
    d = U(OpenInputDesktop)(0x80, FALSE, DESKTOP_READOBJECTS);
    t_note("OpenInputDesktop(flags 0x80) -> %p err %lu", d, GetLastError());
    if (d) U(CloseDesktop)(d);
}
TEST(winsta_process_getdesktopwindow_basic) {
    REQUIRE(U(GetDesktopWindow)); REQUIRE(U(IsWindow)); REQUIRE(U(GetClassNameA)); REQUIRE(U(GetParent)); REQUIRE(U(GetWindowThreadProcessId));
    HWND h = U(GetDesktopWindow)();
    CHECK(h != NULL);
    EXPECT_EQ(h, U(GetDesktopWindow)());
    CHECK(U(IsWindow)(h));
    char c[32] = "";
    CHECK(U(GetClassNameA)(h, c, sizeof c) > 0);
    EXPECT_STR(c, "#32769");
    CHECK(U(GetParent)(h) == NULL);
    DWORD pid = 0;
    CHECK(U(GetWindowThreadProcessId)(h, &pid) != 0);
    CHECK(pid != GetCurrentProcessId());
}
static DWORD wk_deskwin(void *p) { *(HWND *)p = U(GetDesktopWindow)(); return 0; }
TEST(winsta_process_getdesktopwindow_worker_same) {
    REQUIRE(U(GetDesktopWindow));
    HWND w = NULL; DWORD r;
    CHECK(wsp_run(wk_deskwin, &w, 5000, &r));
    EXPECT_EQ(w, U(GetDesktopWindow)());
}
TEST(winsta_process_checkwindowthreaddesktop_export) {
    CHECK(t_proc("CheckWindowThreadDesktop") != NULL);
    CHECK(t_proc("GetInputDesktop") != NULL);
    CHECK(t_proc("OpenThreadDesktop") != NULL);
}
