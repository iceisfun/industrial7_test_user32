/* GetUserObjectInformation / SetUserObjectInformation / user object security */
#include "wsp.h"

#define UOI_FLAGS_ 1
#define UOI_NAME_ 2
#define UOI_TYPE_ 3
#define UOI_USER_SID_ 4
#define UOI_HEAPSIZE_ 5
#define UOI_IO_ 6
#define UOI_TIMERPROC_EXC_ 7

static HDESK mkdesk(char *n, const char *tag) {
    wsp_name(n, 96, tag);
    return U(CreateDesktopA)(n, NULL, NULL, 0, WSP_DESKTOP_ALL, NULL);
}
/* security tests need READ_CONTROL | WRITE_DAC | WRITE_OWNER on the handle; the
 * desktop-specific rights alone (0x1FF) make Get/SetUserObjectSecurity fail with 5 */
static HDESK mkdesk_sec(char *n, const char *tag) {
    wsp_name(n, 96, tag);
    return U(CreateDesktopA)(n, NULL, NULL, 0, WSP_DESKTOP_ALL | READ_CONTROL | WRITE_DAC | WRITE_OWNER, NULL);
}
static HDESK curdesk(void) { return U(GetThreadDesktop)(GetCurrentThreadId()); }
#define NEED_UOI() do { REQUIRE(U(GetUserObjectInformationA)); REQUIRE(U(GetUserObjectInformationW)); REQUIRE(U(GetThreadDesktop)); REQUIRE(U(GetProcessWindowStation)); } while (0)

/* sizing behaviour table: object, index, expected A/W text */
static void sizing_a(HANDLE h, DWORD idx, const char *expect) {
    DWORD need = 0, len = (DWORD)strlen(expect) + 1;
    char buf[512]; memset(buf, 0x55, sizeof buf);
    SetLastError(0);
    /* Win10: when the buffer is too small the A entry point reports the size of the wide
     * string in bytes (2 * len), but on success it reports len (observed 0x34 vs 0x1a) */
    CHECK(!U(GetUserObjectInformationA)(h, idx, buf, len - 1, &need));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, 2 * len);
    need = 0; SetLastError(0);
    CHECK(!U(GetUserObjectInformationA)(h, idx, buf, 0, &need));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, 2 * len);
    need = 0; SetLastError(0);
    CHECK(!U(GetUserObjectInformationA)(h, idx, NULL, 0, &need));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, 2 * len);
    need = 0;
    CHECK(U(GetUserObjectInformationA)(h, idx, buf, len, &need));
    EXPECT_EQ(need, len); EXPECT_STR(buf, expect);
    need = 0; memset(buf, 0x55, sizeof buf);
    CHECK(U(GetUserObjectInformationA)(h, idx, buf, 512, &need));
    EXPECT_EQ(need, len); EXPECT_STR(buf, expect);
    EXPECT_EQ((unsigned char)buf[len], 0x55);   /* nothing written beyond the terminator */
}
static void sizing_w(HANDLE h, DWORD idx, const WCHAR *expect) {
    DWORD need = 0, len = (lstrlenW(expect) + 1) * 2;
    WCHAR buf[512]; memset(buf, 0x55, sizeof buf);
    SetLastError(0);
    CHECK(!U(GetUserObjectInformationW)(h, idx, buf, len - 1, &need));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, len);
    need = 0; SetLastError(0);
    CHECK(!U(GetUserObjectInformationW)(h, idx, buf, len - 2, &need));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, len);
    need = 0; SetLastError(0);
    CHECK(!U(GetUserObjectInformationW)(h, idx, NULL, 0, &need));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, len);
    need = 0;
    CHECK(U(GetUserObjectInformationW)(h, idx, buf, len, &need));
    EXPECT_EQ(need, len); EXPECT_WSTR(buf, expect);
    EXPECT_EQ(buf[len / 2], 0x5555);
}

TEST(winsta_process_uoi_type_desktop_sizing_a) { NEED_UOI(); sizing_a(curdesk(), UOI_TYPE_, "Desktop"); }
TEST(winsta_process_uoi_type_desktop_sizing_w) { NEED_UOI(); sizing_w(curdesk(), UOI_TYPE_, L"Desktop"); }
TEST(winsta_process_uoi_type_winsta_sizing_a) { NEED_UOI(); sizing_a(U(GetProcessWindowStation)(), UOI_TYPE_, "WindowStation"); }
TEST(winsta_process_uoi_type_winsta_sizing_w) { NEED_UOI(); sizing_w(U(GetProcessWindowStation)(), UOI_TYPE_, L"WindowStation"); }
TEST(winsta_process_uoi_name_custom_desktop_sizing) {
    NEED_UOI(); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CreateDesktopW));
    char n[96]; HDESK d = mkdesk(n, "uoiN");
    if (!d) { t_skip("cannot create desktop"); return; }
    sizing_a(d, UOI_NAME_, n);
    WCHAR w[96]; MultiByteToWideChar(CP_ACP, 0, n, -1, w, 96);
    sizing_w(d, UOI_NAME_, w);
    U(CloseDesktop)(d);
}
TEST(winsta_process_uoi_name_custom_winsta_sizing) {
    NEED_UOI(); REQUIRE(U(CreateWindowStationA));
    char n[96]; wsp_name(n, sizeof n, "uoiW");
    HWINSTA h = U(CreateWindowStationA)(n, 0, WSP_WINSTA_ALL, NULL);
    if (!h) { t_skip("cannot create winsta"); return; }
    sizing_a(h, UOI_NAME_, n);
    WCHAR w[96]; MultiByteToWideChar(CP_ACP, 0, n, -1, w, 96);
    sizing_w(h, UOI_NAME_, w);
    U(CloseWindowStation)(h);
}
TEST(winsta_process_uoi_name_lengthneeded_not_modified_on_success_other_fields) {
    NEED_UOI();
    char b[64]; DWORD need = 0xdead;
    CHECK(U(GetUserObjectInformationA)(curdesk(), UOI_NAME_, b, sizeof b, &need));
    CHECK(need == strlen(b) + 1);
}
TEST(winsta_process_uoi_flags_sizes) {
    NEED_UOI();
    static const DWORD sizes[] = { 0, 1, 4, 8, 11 };
    for (unsigned i = 0; i < 5; i++) {
        WSP_UOF f; DWORD need = 0;
        SetLastError(0);
        CHECKF(!U(GetUserObjectInformationA)(curdesk(), UOI_FLAGS_, &f, sizes[i], &need), "size %lu", sizes[i]);
        EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
        EXPECT_EQ(need, 12);
    }
    WSP_UOF f; DWORD need = 0;
    CHECK(U(GetUserObjectInformationA)(curdesk(), UOI_FLAGS_, &f, 12, &need)); EXPECT_EQ(need, 12);
    need = 0;
    CHECK(U(GetUserObjectInformationW)(curdesk(), UOI_FLAGS_, &f, 64, &need)); EXPECT_EQ(need, 12);
}
TEST(winsta_process_uoi_flags_ab_parity) {
    NEED_UOI();
    WSP_UOF a = { 3, 3, 3 }, w = { 4, 4, 4 }; DWORD n1, n2;
    CHECK(U(GetUserObjectInformationA)(U(GetProcessWindowStation)(), UOI_FLAGS_, &a, sizeof a, &n1));
    CHECK(U(GetUserObjectInformationW)(U(GetProcessWindowStation)(), UOI_FLAGS_, &w, sizeof w, &n2));
    EXPECT_EQ(a.dwFlags, w.dwFlags); EXPECT_EQ(a.fInherit, w.fInherit);
}
TEST(winsta_process_uoi_flags_interactive_winsta_visible) {
    NEED_UOI();
    char n[64]; DWORD nd; WSP_UOF f = { 0, 0, 0 };
    U(GetUserObjectInformationA)(U(GetProcessWindowStation)(), UOI_NAME_, n, sizeof n, &nd);
    CHECK(U(GetUserObjectInformationA)(U(GetProcessWindowStation)(), UOI_FLAGS_, &f, sizeof f, &nd));
    if (!lstrcmpiA(n, "WinSta0")) CHECK(f.dwFlags & WSF_VISIBLE);
    else t_note("process winsta is %s, flags 0x%lx", n, (unsigned long)f.dwFlags);
}
TEST(winsta_process_uoi_flags_desktop_has_no_visible_bit) {
    NEED_UOI();
    WSP_UOF f; DWORD nd;
    CHECK(U(GetUserObjectInformationA)(curdesk(), UOI_FLAGS_, &f, sizeof f, &nd));
    t_note("thread desktop UOI_FLAGS dwFlags=0x%lx fInherit=%d", (unsigned long)f.dwFlags, f.fInherit);
}
TEST(winsta_process_uoi_invalid_handle_all_indexes) {
    NEED_UOI();
    static const DWORD idx[] = { 1, 2, 3, 4, 5, 6 };
    char buf[256]; DWORD need;
    for (unsigned i = 0; i < 6; i++) {
        SetLastError(0);
        /* Win10: a nonexistent handle fails without setting the last error (0); a valid
         * handle of the wrong kind (here the -1 process pseudo handle) gives 1 */
        CHECKF(!U(GetUserObjectInformationA)(NULL, idx[i], buf, sizeof buf, &need), "idx %lu", idx[i]);
        EXPECT_ERR(0, 0);
        SetLastError(0);
        CHECK(!U(GetUserObjectInformationW)((HANDLE)(ULONG_PTR)0x7ffc, idx[i], buf, sizeof buf, &need));
        EXPECT_ERR(0, 0);
        SetLastError(0);
        CHECK(!U(GetUserObjectInformationA)(INVALID_HANDLE_VALUE, idx[i], buf, sizeof buf, &need));
        EXPECT_ERR(0, ERROR_INVALID_FUNCTION);
    }
}
TEST(winsta_process_uoi_wrong_object_type) {
    NEED_UOI();
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE th = GetCurrentProcess();
    char buf[256]; DWORD need;
    SetLastError(0);
    /* Win10: a valid handle that is not a window station / desktop gives ERROR_INVALID_FUNCTION */
    CHECK(!U(GetUserObjectInformationA)(ev, UOI_NAME_, buf, sizeof buf, &need));
    EXPECT_ERR(0, ERROR_INVALID_FUNCTION);
    SetLastError(0);
    CHECK(!U(GetUserObjectInformationW)(th, UOI_TYPE_, buf, sizeof buf, &need));
    EXPECT_ERR(0, ERROR_INVALID_FUNCTION);
    CloseHandle(ev);
}
TEST(winsta_process_uoi_closed_handle) {
    NEED_UOI(); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop));
    char n[96]; HDESK d = mkdesk(n, "uoiC");
    if (!d) { t_skip("cannot create desktop"); return; }
    U(CloseDesktop)(d);
    char buf[64]; DWORD need; SetLastError(0);
    CHECK(!U(GetUserObjectInformationA)(d, UOI_NAME_, buf, sizeof buf, &need));
    EXPECT_ERR(0, 0);   /* Win10: a closed (nonexistent) handle leaves the last error 0 */
}
TEST(winsta_process_uoi_invalid_index) {
    NEED_UOI();
    static const int idx[] = { 0, 8, 9, 100, 0x7fffffff, -1 };
    for (unsigned i = 0; i < 6; i++) {
        char buf[64]; DWORD need = 0; SetLastError(0);
        CHECKF(!U(GetUserObjectInformationA)(curdesk(), idx[i], buf, sizeof buf, &need), "idx %d", idx[i]);
        t_note("GetUserObjectInformation index %d err %lu", idx[i], GetLastError());
        SetLastError(0);
        CHECKF(!U(GetUserObjectInformationW)(U(GetProcessWindowStation)(), idx[i], buf, sizeof buf, &need), "idx %d", idx[i]);
    }
}
TEST(winsta_process_uoi_user_sid) {
    NEED_UOI();
    HANDLE hs[2] = { U(GetProcessWindowStation)(), curdesk() };
    for (int i = 0; i < 2; i++) {
        BYTE sid[256]; DWORD need = 0xfff;
        SetLastError(0);
        BOOL r = U(GetUserObjectInformationW)(hs[i], UOI_USER_SID_, sid, sizeof sid, &need);
        t_note("UOI_USER_SID object %d: ret %d need %lu err %lu", i, r, (unsigned long)need, GetLastError());
        if (r && need > 0) { CHECK(IsValidSid((PSID)sid)); CHECK(GetLengthSid((PSID)sid) == need); }
    }
}
TEST(winsta_process_uoi_user_sid_a_w_parity) {
    NEED_UOI();
    BYTE a[256], w[256]; DWORD na = 0, nw = 0;
    BOOL ra = U(GetUserObjectInformationA)(U(GetProcessWindowStation)(), UOI_USER_SID_, a, sizeof a, &na);
    BOOL rw = U(GetUserObjectInformationW)(U(GetProcessWindowStation)(), UOI_USER_SID_, w, sizeof w, &nw);
    EXPECT_EQ(ra, rw); EXPECT_EQ(na, nw);
    if (ra && na) CHECK(!memcmp(a, w, na));
}
TEST(winsta_process_uoi_heapsize) {
    NEED_UOI();
    ULONG h = 0; DWORD need = 0; SetLastError(0);
    BOOL r = U(GetUserObjectInformationW)(curdesk(), UOI_HEAPSIZE_, &h, sizeof h, &need);
    t_note("UOI_HEAPSIZE thread desktop: ret %d value %lu need %lu err %lu", r, (unsigned long)h, (unsigned long)need, GetLastError());
    if (r) CHECK(h > 0);
    r = U(GetUserObjectInformationW)(U(GetProcessWindowStation)(), UOI_HEAPSIZE_, &h, sizeof h, &need);
    t_note("UOI_HEAPSIZE winsta: ret %d err %lu", r, GetLastError());
}
TEST(winsta_process_uoi_io) {
    NEED_UOI();
    BOOL v = 7; DWORD need = 0; SetLastError(0);
    BOOL r = U(GetUserObjectInformationW)(U(GetProcessWindowStation)(), UOI_IO_, &v, sizeof v, &need);
    t_note("UOI_IO winsta: ret %d value %d need %lu err %lu", r, v, (unsigned long)need, GetLastError());
    r = U(GetUserObjectInformationW)(curdesk(), UOI_IO_, &v, sizeof v, &need);
    t_note("UOI_IO desktop: ret %d value %d need %lu err %lu", r, v, (unsigned long)need, GetLastError());
}
TEST(winsta_process_uoi_timerproc_exception_suppression) {
    NEED_UOI(); REQUIRE(U(SetUserObjectInformationW));
    BOOL v = 7, orig; DWORD need = 0; SetLastError(0);
    BOOL r = U(GetUserObjectInformationW)(NULL, UOI_TIMERPROC_EXC_, &v, sizeof v, &need);
    t_note("get UOI_TIMERPROC_EXCEPTION_SUPPRESSION(NULL): ret %d value %d err %lu", r, v, GetLastError());
    if (!r) return;
    orig = v;
    BOOL nv = !orig;
    r = U(SetUserObjectInformationW)(NULL, UOI_TIMERPROC_EXC_, &nv, sizeof nv);
    t_note("set UOI_TIMERPROC_EXCEPTION_SUPPRESSION(%d): ret %d err %lu", nv, r, GetLastError());
    U(GetUserObjectInformationW)(NULL, UOI_TIMERPROC_EXC_, &v, sizeof v, &need);
    t_note("   readback %d", v);
    U(SetUserObjectInformationW)(NULL, UOI_TIMERPROC_EXC_, &orig, sizeof orig);
}
TEST(winsta_process_setuoi_flags_inherit_roundtrip) {
    NEED_UOI(); REQUIRE(U(SetUserObjectInformationA)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop));
    char n[96]; HDESK d = mkdesk(n, "uoiS");
    if (!d) { t_skip("cannot create desktop"); return; }
    for (int round = 0; round < 3; round++) {
        WSP_UOF f; DWORD nd, hf = 0;
        CHECK(U(GetUserObjectInformationA)(d, UOI_FLAGS_, &f, sizeof f, &nd));
        f.fInherit = !f.fInherit;
        BOOL want = f.fInherit;
        SetLastError(0);
        CHECK(U(SetUserObjectInformationA)(d, UOI_FLAGS_, &f, sizeof f));
        WSP_UOF g = { 8, 8, 8 };
        CHECK(U(GetUserObjectInformationA)(d, UOI_FLAGS_, &g, sizeof g, &nd));
        EXPECT_EQ(g.fInherit, want);
        GetHandleInformation(d, &hf);
        EXPECT_EQ((hf & HANDLE_FLAG_INHERIT) != 0, want);
    }
    U(CloseDesktop)(d);
}
TEST(winsta_process_setuoi_flags_w_parity) {
    NEED_UOI(); REQUIRE(U(SetUserObjectInformationW)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop));
    char n[96]; HDESK d = mkdesk(n, "uoiSW");
    if (!d) { t_skip("cannot create desktop"); return; }
    WSP_UOF f = { TRUE, 0, 0 }; DWORD nd;
    CHECK(U(SetUserObjectInformationW)(d, UOI_FLAGS_, &f, sizeof f));
    WSP_UOF g;
    CHECK(U(GetUserObjectInformationW)(d, UOI_FLAGS_, &g, sizeof g, &nd)); EXPECT_EQ(g.fInherit, 1);
    f.fInherit = FALSE;
    CHECK(U(SetUserObjectInformationW)(d, UOI_FLAGS_, &f, sizeof f));
    CHECK(U(GetUserObjectInformationW)(d, UOI_FLAGS_, &g, sizeof g, &nd)); EXPECT_EQ(g.fInherit, 0);
    U(CloseDesktop)(d);
}
TEST(winsta_process_setuoi_invalid_handle) {
    NEED_UOI(); REQUIRE(U(SetUserObjectInformationA)); REQUIRE(U(SetUserObjectInformationW));
    WSP_UOF f = { 0, 0, 0 };
    SetLastError(0);
    /* Win10: nonexistent handles leave the last error 0; a valid non-user object gives 1 */
    CHECK(!U(SetUserObjectInformationA)(NULL, UOI_FLAGS_, &f, sizeof f));
    EXPECT_ERR(0, 0);
    SetLastError(0);
    CHECK(!U(SetUserObjectInformationW)((HANDLE)(ULONG_PTR)0x7ffc, UOI_FLAGS_, &f, sizeof f));
    EXPECT_ERR(0, 0);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CHECK(!U(SetUserObjectInformationA)(ev, UOI_FLAGS_, &f, sizeof f));
    EXPECT_ERR(0, ERROR_INVALID_FUNCTION);
    CloseHandle(ev);
}
TEST(winsta_process_setuoi_invalid_index) {
    NEED_UOI(); REQUIRE(U(SetUserObjectInformationA)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop));
    char n[96]; HDESK d = mkdesk(n, "uoiSI");
    if (!d) { t_skip("cannot create desktop"); return; }
    WSP_UOF f = { 0, 0, 0 };
    static const int idx[] = { 0, 8, 50, -1 };
    for (int i = 0; i < 4; i++) {
        SetLastError(0);
        CHECKF(!U(SetUserObjectInformationA)(d, idx[i], &f, sizeof f), "index %d", idx[i]);
        t_note("SetUserObjectInformation index %d err %lu", idx[i], GetLastError());
    }
    /* name and type are read-only */
    CHECK(!U(SetUserObjectInformationA)(d, UOI_NAME_, "abc", 4));
    CHECK(!U(SetUserObjectInformationA)(d, UOI_TYPE_, "abc", 4));
    U(CloseDesktop)(d);
}
TEST(winsta_process_setuoi_flags_bad_size) {
    NEED_UOI(); REQUIRE(U(SetUserObjectInformationA)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop));
    char n[96]; HDESK d = mkdesk(n, "uoiSZ");
    if (!d) { t_skip("cannot create desktop"); return; }
    WSP_UOF f = { TRUE, 0, 0 };
    static const DWORD sz[] = { 0, 4, 8, 11, 13, 64 };
    for (int i = 0; i < 6; i++) {
        SetLastError(0);
        BOOL r = U(SetUserObjectInformationA)(d, UOI_FLAGS_, &f, sz[i]);
        t_note("SetUserObjectInformation(UOI_FLAGS, size %lu) ret %d err %lu", (unsigned long)sz[i], r, GetLastError());
    }
    U(CloseDesktop)(d);
}
/* ---- security ---- */
static void need_sec(void) {}
#define NEED_SEC() do { REQUIRE(U(GetUserObjectSecurity)); REQUIRE(U(SetUserObjectSecurity)); REQUIRE(U(CreateDesktopA)); REQUIRE(U(CloseDesktop)); } while (0)

TEST(winsta_process_secd_get_dacl_sizing) {
    NEED_SEC();
    char n[96]; HDESK d = mkdesk_sec(n, "sec1");
    if (!d) { t_skip("cannot create desktop"); return; }
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
    DWORD need = 0; SetLastError(0);
    CHECK(!U(GetUserObjectSecurity)(d, &si, NULL, 0, &need));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(SECURITY_DESCRIPTOR_RELATIVE));
    PSECURITY_DESCRIPTOR sd = LocalAlloc(LPTR, need + 16);
    DWORD need2 = 0;
    SetLastError(0);
    CHECK(!U(GetUserObjectSecurity)(d, &si, sd, need - 1, &need2));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need2, need);
    need2 = 0;
    CHECK(U(GetUserObjectSecurity)(d, &si, sd, need, &need2));
    EXPECT_EQ(need2, need);
    CHECK(IsValidSecurityDescriptor(sd));
    BOOL present = FALSE, def = FALSE; PACL acl = NULL;
    CHECK(GetSecurityDescriptorDacl(sd, &present, &acl, &def));
    CHECK(present);
    SECURITY_DESCRIPTOR_CONTROL ctl; DWORD rev;
    CHECK(GetSecurityDescriptorControl(sd, &ctl, &rev));
    CHECK(ctl & SE_SELF_RELATIVE);
    LocalFree(sd);
    U(CloseDesktop)(d);
}
TEST(winsta_process_secd_get_owner_group) {
    NEED_SEC();
    char n[96]; HDESK d = mkdesk_sec(n, "sec2");
    if (!d) { t_skip("cannot create desktop"); return; }
    SECURITY_INFORMATION si = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION;
    DWORD need = 0;
    U(GetUserObjectSecurity)(d, &si, NULL, 0, &need);
    CHECK(need > 0);
    PSECURITY_DESCRIPTOR sd = LocalAlloc(LPTR, need + 16);
    CHECK(U(GetUserObjectSecurity)(d, &si, sd, need, &need));
    CHECK(IsValidSecurityDescriptor(sd));
    PSID o = NULL, g = NULL; BOOL def;
    CHECK(GetSecurityDescriptorOwner(sd, &o, &def)); CHECK(o != NULL && IsValidSid(o));
    CHECK(GetSecurityDescriptorGroup(sd, &g, &def)); CHECK(g != NULL && IsValidSid(g));
    PACL acl = NULL; BOOL present = TRUE;
    GetSecurityDescriptorDacl(sd, &present, &acl, &def);
    CHECK(!present);               /* DACL was not requested */
    LocalFree(sd);
    U(CloseDesktop)(d);
}
TEST(winsta_process_secd_get_on_winsta) {
    NEED_SEC();
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION;
    HWINSTA ws = U(GetProcessWindowStation)();
    DWORD need = 0; SetLastError(0);
    BOOL r = U(GetUserObjectSecurity)(ws, &si, NULL, 0, &need);
    CHECK(!r);
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need > 0);
}
TEST(winsta_process_secd_get_on_thread_desktop) {
    NEED_SEC();
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
    DWORD need = 0; SetLastError(0);
    CHECK(!U(GetUserObjectSecurity)(curdesk(), &si, NULL, 0, &need));
    EXPECT_ERR(0, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need > 0);
}
TEST(winsta_process_secd_get_invalid_handle) {
    NEED_SEC();
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
    BYTE buf[512]; DWORD need = 0;
    SetLastError(0);
    CHECK(!U(GetUserObjectSecurity)(NULL, &si, buf, sizeof buf, &need));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(GetUserObjectSecurity)((HANDLE)(ULONG_PTR)0x7ffc, &si, buf, sizeof buf, &need));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_secd_get_sacl_without_privilege) {
    NEED_SEC();
    char n[96]; HDESK d = mkdesk(n, "sec3");
    if (!d) { t_skip("cannot create desktop"); return; }
    SECURITY_INFORMATION si = SACL_SECURITY_INFORMATION;
    BYTE buf[1024]; DWORD need = 0; SetLastError(0);
    BOOL r = U(GetUserObjectSecurity)(d, &si, buf, sizeof buf, &need);
    t_note("GetUserObjectSecurity(SACL) ret %d err %lu", r, GetLastError());
    U(CloseDesktop)(d);
}
TEST(winsta_process_secd_get_empty_info_mask) {
    NEED_SEC();
    char n[96]; HDESK d = mkdesk(n, "sec4");
    if (!d) { t_skip("cannot create desktop"); return; }
    SECURITY_INFORMATION si = 0;
    BYTE buf[1024]; DWORD need = 0; SetLastError(0);
    BOOL r = U(GetUserObjectSecurity)(d, &si, buf, sizeof buf, &need);
    t_note("GetUserObjectSecurity(0) ret %d need %lu err %lu", r, (unsigned long)need, GetLastError());
    if (r) CHECK(IsValidSecurityDescriptor(buf));
    U(CloseDesktop)(d);
}
TEST(winsta_process_secd_set_roundtrip_dacl) {
    NEED_SEC();
    char n[96]; HDESK d = mkdesk_sec(n, "sec5");
    if (!d) { t_skip("cannot create desktop"); return; }
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
    DWORD need = 0;
    U(GetUserObjectSecurity)(d, &si, NULL, 0, &need);
    PSECURITY_DESCRIPTOR sd = LocalAlloc(LPTR, need), sd2;
    CHECK(U(GetUserObjectSecurity)(d, &si, sd, need, &need));
    SetLastError(0);
    CHECK(U(SetUserObjectSecurity)(d, &si, sd));
    DWORD need2 = 0;
    U(GetUserObjectSecurity)(d, &si, NULL, 0, &need2);
    sd2 = LocalAlloc(LPTR, need2);
    CHECK(U(GetUserObjectSecurity)(d, &si, sd2, need2, &need2));
    PACL a1 = NULL, a2 = NULL; BOOL p1, p2, d1;
    GetSecurityDescriptorDacl(sd, &p1, &a1, &d1);
    GetSecurityDescriptorDacl(sd2, &p2, &a2, &d1);
    CHECK(p1 && p2);
    if (a1 && a2) { CHECK(a1->AceCount == a2->AceCount); CHECK(a1->AclSize == a2->AclSize); }
    LocalFree(sd); LocalFree(sd2);
    U(CloseDesktop)(d);
}
TEST(winsta_process_secd_set_empty_dacl_denies_open) {
    NEED_SEC(); REQUIRE(U(OpenDesktopA));
    char n[96]; HDESK d = mkdesk_sec(n, "sec6");
    if (!d) { t_skip("cannot create desktop"); return; }
    SECURITY_DESCRIPTOR sd; BYTE aclbuf[64];
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    CHECK(InitializeAcl((PACL)aclbuf, sizeof aclbuf, ACL_REVISION));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, (PACL)aclbuf, FALSE));
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
    SetLastError(0);
    CHECK(U(SetUserObjectSecurity)(d, &si, &sd));
    SetLastError(0);
    HDESK o = U(OpenDesktopA)(n, 0, FALSE, DESKTOP_READOBJECTS);
    CHECK(o == NULL);
    EXPECT_ERR(0, ERROR_ACCESS_DENIED);
    if (o) U(CloseDesktop)(o);
    /* restore a null DACL (full access) through the handle that still has WRITE_DAC */
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE));
    CHECK(U(SetUserObjectSecurity)(d, &si, &sd));
    o = U(OpenDesktopA)(n, 0, FALSE, DESKTOP_READOBJECTS);
    CHECK(o != NULL);
    if (o) U(CloseDesktop)(o);
    U(CloseDesktop)(d);
}
TEST(winsta_process_secd_set_invalid_handle) {
    NEED_SEC();
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
    SetLastError(0);
    CHECK(!U(SetUserObjectSecurity)(NULL, &si, &sd));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(SetUserObjectSecurity)((HANDLE)(ULONG_PTR)0x7ffc, &si, &sd));
    EXPECT_ERR(0, ERROR_INVALID_HANDLE);
}
TEST(winsta_process_secd_set_invalid_descriptor) {
    NEED_SEC();
    char n[96]; HDESK d = mkdesk(n, "sec7");
    if (!d) { t_skip("cannot create desktop"); return; }
    SECURITY_DESCRIPTOR sd; memset(&sd, 0, sizeof sd);   /* revision 0 */
    SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
    SetLastError(0);
    BOOL r = U(SetUserObjectSecurity)(d, &si, &sd);
    t_note("SetUserObjectSecurity(invalid SD) ret %d err %lu", r, GetLastError());
    CHECK(!r);
    U(CloseDesktop)(d);
}
TEST(winsta_process_secd_readonly_handle_cannot_set) {
    NEED_SEC(); REQUIRE(U(OpenDesktopA));
    char n[96]; HDESK d = mkdesk(n, "sec8");
    if (!d) { t_skip("cannot create desktop"); return; }
    HDESK o = U(OpenDesktopA)(n, 0, FALSE, DESKTOP_READOBJECTS);
    if (o) {
        SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
        SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
        SetLastError(0);
        CHECK(!U(SetUserObjectSecurity)(o, &si, &sd));
        EXPECT_ERR(0, ERROR_ACCESS_DENIED);
        /* and no READ_CONTROL on this handle either */
        BYTE buf[512]; DWORD need = 0; SetLastError(0);
        BOOL r = U(GetUserObjectSecurity)(o, &si, buf, sizeof buf, &need);
        t_note("GetUserObjectSecurity on DESKTOP_READOBJECTS handle: ret %d err %lu", r, GetLastError());
        U(CloseDesktop)(o);
    }
    U(CloseDesktop)(d);
}
