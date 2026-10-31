/* GetUserObjectInformation / SetUserObjectInformation / User object security */
#include "sp.h"

#define GUOW U(GetUserObjectInformationW)
#define GUOA U(GetUserObjectInformationA)

static HWINSTA ws(void) { return U(GetProcessWindowStation)(); }
static HDESK dk(void) { return U(GetThreadDesktop)(GetCurrentThreadId()); }

TEST(uoi_winsta_handle_valid) {
    REQUIRE(U(GetProcessWindowStation));
    CHECK(ws() != NULL);
    CHECK(ws() == ws());
}
TEST(uoi_desktop_handle_valid) {
    REQUIRE(U(GetThreadDesktop));
    CHECK(dk() != NULL);
    CHECK(dk() == dk());
}
TEST(uoi_winsta_name_w) {
    WCHAR buf[128]; DWORD need = 0; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    memset(buf, 0xCD, sizeof buf);
    r = GUOW(ws(), UOI_NAME, buf, sizeof buf, &need);
    CHECKF(r, "err=%lu", GetLastError());
    if (!r) return;
    CHECK(buf[0] != 0);
    CHECK(need == (wcslen(buf) + 1) * sizeof(WCHAR));
    t_note("window station name '%ls'", buf);
}
TEST(uoi_winsta_name_a) {
    char buf[128]; DWORD need = 0; BOOL r;
    REQUIRE(GUOA); REQUIRE(U(GetProcessWindowStation));
    memset(buf, 0xCD, sizeof buf);
    r = GUOA(ws(), UOI_NAME, buf, sizeof buf, &need);
    CHECKF(r, "err=%lu", GetLastError());
    if (!r) return;
    CHECK(buf[0] != 0);
    CHECK(need == strlen(buf) + 1);
}
TEST(uoi_winsta_name_ansi_unicode_parity) {
    WCHAR w[128]; char a[128], c[128]; DWORD nw, na;
    REQUIRE(GUOA); REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    CHECK(GUOW(ws(), UOI_NAME, w, sizeof w, &nw));
    CHECK(GUOA(ws(), UOI_NAME, a, sizeof a, &na));
    memset(c, 0, sizeof c);
    WideCharToMultiByte(CP_ACP, 0, w, -1, c, sizeof c, NULL, NULL);
    CHECK(!strcmp(a, c));
    CHECK(nw == na * 2);
}
TEST(uoi_winsta_name_interactive) {
    WCHAR buf[128];
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    if (GUOW(ws(), UOI_NAME, buf, sizeof buf, NULL)) {
        if (wcscmp(buf, L"WinSta0")) t_note("not running on WinSta0: '%ls'", buf);
        else CHECK(!wcscmp(buf, L"WinSta0"));
    } else CHECK(0);
}
TEST(uoi_winsta_name_buffer_edges) {
    WCHAR buf[128]; DWORD need, full, i; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    CHECK(GUOW(ws(), UOI_NAME, buf, sizeof buf, &full));
    CHECK(full >= 2 * sizeof(WCHAR));
    /* exact size works */
    memset(buf, 0, sizeof buf);
    need = 0;
    CHECK(GUOW(ws(), UOI_NAME, buf, full, &need));
    CHECK(need == full);
    /* every smaller size fails with the required size reported */
    for (i = 0; i < full; i++) {
        need = 0xFFFFFFFF;
        SetLastError(0xdeadbeef);
        r = GUOW(ws(), UOI_NAME, buf, i, &need);
        CHECKF(!r, "size %lu of %lu accepted", i, full);
        CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "size %lu err %lu", i, GetLastError());
        CHECKF(need == full, "size %lu need %lu want %lu", i, need, full);
    }
}
TEST(uoi_winsta_name_zero_size_null_buffer) {
    DWORD need = 0; BOOL r; DWORD full;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    GUOW(ws(), UOI_NAME, (WCHAR[64]){0}, 128, &full);
    SetLastError(0xdeadbeef);
    r = GUOW(ws(), UOI_NAME, NULL, 0, &need);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    CHECK(need == full);
}
TEST(uoi_winsta_name_ansi_buffer_edges) {
    char buf[128]; DWORD need, full, i; BOOL r;
    REQUIRE(GUOA); REQUIRE(U(GetProcessWindowStation));
    CHECK(GUOA(ws(), UOI_NAME, buf, sizeof buf, &full));
    for (i = 0; i < full; i++) {
        need = 0xFFFFFFFF;
        SetLastError(0xdeadbeef);
        r = GUOA(ws(), UOI_NAME, buf, i, &need);
        CHECKF(!r, "ANSI size %lu of %lu accepted", i, full);
        CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "ANSI size %lu err %lu", i, GetLastError());
        if (need != full && i == 0) t_note("ANSI needed length %lu vs successful length %lu", need, full);
        CHECKF(need >= full, "ANSI size %lu need %lu below %lu", i, need, full);
    }
    CHECK(GUOA(ws(), UOI_NAME, buf, full, &need));
}
TEST(uoi_winsta_type) {
    WCHAR w[64]; char a[64]; DWORD n;
    REQUIRE(GUOW); REQUIRE(GUOA); REQUIRE(U(GetProcessWindowStation));
    CHECK(GUOW(ws(), UOI_TYPE, w, sizeof w, &n));
    CHECK(!wcscmp(w, L"WindowStation"));
    CHECK(n == (wcslen(L"WindowStation") + 1) * 2);
    CHECK(GUOA(ws(), UOI_TYPE, a, sizeof a, &n));
    CHECK(!strcmp(a, "WindowStation"));
    CHECK(n == strlen("WindowStation") + 1);
}
TEST(uoi_desktop_type) {
    WCHAR w[64]; char a[64]; DWORD n;
    REQUIRE(GUOW); REQUIRE(GUOA); REQUIRE(U(GetThreadDesktop));
    CHECK(GUOW(dk(), UOI_TYPE, w, sizeof w, &n));
    CHECK(!wcscmp(w, L"Desktop"));
    CHECK(n == 16);
    CHECK(GUOA(ws() ? dk() : dk(), UOI_TYPE, a, sizeof a, &n));
    CHECK(!strcmp(a, "Desktop"));
    CHECK(n == 8);
}
TEST(uoi_desktop_name) {
    WCHAR w[128]; char a[128]; DWORD nw, na;
    REQUIRE(GUOW); REQUIRE(GUOA); REQUIRE(U(GetThreadDesktop));
    CHECK(GUOW(dk(), UOI_NAME, w, sizeof w, &nw));
    CHECK(w[0] != 0);
    CHECK(nw == (wcslen(w) + 1) * 2);
    CHECK(GUOA(dk(), UOI_NAME, a, sizeof a, &na));
    CHECK(nw == na * 2);
    if (wcscmp(w, L"Default")) t_note("desktop name '%ls'", w);
}
TEST(uoi_desktop_name_buffer_edges) {
    WCHAR buf[128]; DWORD need, full, i; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetThreadDesktop));
    CHECK(GUOW(dk(), UOI_NAME, buf, sizeof buf, &full));
    for (i = 0; i < full; i += 2) {
        need = 0;
        SetLastError(0xdeadbeef);
        r = GUOW(dk(), UOI_NAME, buf, i, &need);
        CHECKF(!r && GetLastError() == ERROR_INSUFFICIENT_BUFFER && need == full, "desktop size %lu", i);
    }
}
TEST(uoi_winsta_flags) {
    USEROBJECTFLAGS f; DWORD n = 0; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    memset(&f, 0xCD, sizeof f);
    r = GUOW(ws(), UOI_FLAGS, &f, sizeof f, &n);
    CHECKF(r, "err=%lu", GetLastError());
    if (!r) return;
    CHECK(sizeof f == 12);
    CHECK(n == sizeof f);
    CHECK(f.fInherit == FALSE || f.fInherit == TRUE);
    CHECK((f.dwFlags & ~(DWORD)WSF_VISIBLE) == 0);
    t_note("winsta flags: inherit=%d reserved=%d flags=0x%lx", f.fInherit, f.fReserved, f.dwFlags);
}
TEST(uoi_winsta_flags_visible_interactive) {
    USEROBJECTFLAGS f; WCHAR name[64];
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    if (!GUOW(ws(), UOI_NAME, name, sizeof name, NULL) || wcscmp(name, L"WinSta0")) { t_skip("not interactive window station"); return; }
    CHECK(GUOW(ws(), UOI_FLAGS, &f, sizeof f, NULL));
    CHECK(f.dwFlags & WSF_VISIBLE);
}
TEST(uoi_winsta_flags_size_edges) {
    USEROBJECTFLAGS f; DWORD n, i; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    for (i = 0; i < sizeof f; i++) {
        n = 0xFFFFFFFF;
        SetLastError(0xdeadbeef);
        r = GUOW(ws(), UOI_FLAGS, &f, i, &n);
        CHECKF(!r, "flags size %lu accepted", i);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER && i == 0) t_note("flags small buffer err %lu", GetLastError());
        CHECKF(n == sizeof f, "flags size %lu need %lu", i, n);
    }
    CHECK(GUOW(ws(), UOI_FLAGS, &f, sizeof f + 4, &n));
    CHECK(n == sizeof f);
}
TEST(uoi_desktop_flags) {
    USEROBJECTFLAGS f; DWORD n = 0; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetThreadDesktop));
    memset(&f, 0xCD, sizeof f);
    r = GUOW(dk(), UOI_FLAGS, &f, sizeof f, &n);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) { CHECK(n == sizeof f); t_note("desktop flags 0x%lx", f.dwFlags); }
}
TEST(uoi_desktop_flags_a_parity) {
    USEROBJECTFLAGS a, w; DWORD n;
    REQUIRE(GUOW); REQUIRE(GUOA); REQUIRE(U(GetThreadDesktop));
    CHECK(GUOW(dk(), UOI_FLAGS, &w, sizeof w, &n));
    CHECK(GUOA(dk(), UOI_FLAGS, &a, sizeof a, &n));
    CHECK(!memcmp(&a, &w, sizeof a));
}
TEST(uoi_user_sid) {
    BYTE buf[256]; DWORD n = 0; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    SetLastError(0xdeadbeef);
    r = GUOW(ws(), UOI_USER_SID, buf, sizeof buf, &n);
    t_note("UOI_USER_SID -> %d err=%lu need=%lu", r, GetLastError(), n);
    if (r && n >= 8) CHECK(IsValidSid((PSID)buf));
}
TEST(uoi_user_sid_zero_buffer) {
    DWORD n = 0xFFFFFFFF; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    SetLastError(0xdeadbeef);
    r = GUOW(ws(), UOI_USER_SID, NULL, 0, &n);
    t_note("UOI_USER_SID(NULL,0) -> %d err=%lu need=%lu", r, GetLastError(), n);
    if (!r) CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER || GetLastError() == ERROR_NOT_FOUND || GetLastError() == ERROR_INVALID_PARAMETER);
}
TEST(uoi_heapsize_desktop) {
    DWORD v = SENT32, n = 0; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetThreadDesktop));
    r = GUOW(dk(), UOI_HEAPSIZE, &v, sizeof v, &n);
    t_note("UOI_HEAPSIZE desktop -> %d err=%lu v=%lu n=%lu", r, GetLastError(), v, n);
    if (r) { CHECK(v > 0); CHECK(n == 4); }
}
TEST(uoi_io_desktop) {
    DWORD v = SENT32, n = 0; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetThreadDesktop));
    r = GUOW(dk(), UOI_IO, &v, sizeof v, &n);
    t_note("UOI_IO desktop -> %d err=%lu v=%lu", r, GetLastError(), v);
    if (r) { CHECK(v == 0 || v == 1); CHECK(n == 4); }
}
TEST(uoi_io_winsta) {
    DWORD v = SENT32, n = 0; BOOL r;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    r = GUOW(ws(), UOI_IO, &v, sizeof v, &n);
    t_note("UOI_IO winsta -> %d err=%lu v=%lu", r, GetLastError(), v);
}
TEST(uoi_invalid_handles) {
    static const HANDLE hs[] = { NULL, (HANDLE)(ULONG_PTR)-1, (HANDLE)(ULONG_PTR)-2, (HANDLE)(ULONG_PTR)0x12345678, (HANDLE)(ULONG_PTR)0xdeadbeefULL };
    WCHAR buf[64]; DWORD n; unsigned i;
    REQUIRE(GUOW);
    for (i = 0; i < sizeof hs / sizeof *hs; i++) {
        BOOL r;
        n = 0xFFFFFFFF;
        SetLastError(0xdeadbeef);
        r = GUOW(hs[i], UOI_NAME, buf, sizeof buf, &n);
        CHECKF(!r, "handle %p accepted", hs[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "handle %p err %lu", hs[i], GetLastError());
    }
}
TEST(uoi_process_handle_is_not_userobject) {
    WCHAR buf[64]; BOOL r;
    HANDLE h;
    REQUIRE(GUOW);
    SetLastError(0xdeadbeef);
    r = GUOW(GetCurrentProcess(), UOI_NAME, buf, sizeof buf, NULL);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
    h = CreateEventA(NULL, FALSE, FALSE, NULL);
    SetLastError(0xdeadbeef);
    r = GUOW(h, UOI_NAME, buf, sizeof buf, NULL);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
    CloseHandle(h);
}
TEST(uoi_invalid_index) {
    static const int idx[] = { 0, 8, 9, 10, 99, 100, -1, 0x7FFFFFFF, (int)0x80000000 };
    DWORD buf[16]; unsigned i;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    for (i = 0; i < sizeof idx / sizeof *idx; i++) {
        BOOL r;
        DWORD n = 0;
        SetLastError(0xdeadbeef);
        r = GUOW(ws(), idx[i], buf, sizeof buf, &n);
        CHECKF(!r, "UOI index %d accepted", idx[i]);
        if (!r && GetLastError() != ERROR_INVALID_PARAMETER) t_note("UOI index %d err=%lu", idx[i], GetLastError());
    }
}
TEST(uoi_invalid_index_ansi) {
    DWORD buf[16]; BOOL r;
    REQUIRE(GUOA); REQUIRE(U(GetProcessWindowStation));
    r = GUOA(ws(), 99, buf, sizeof buf, NULL);
    CHECK(!r);
}
TEST(uoi_repeat_calls_stable) {
    WCHAR a[64], b[64]; int i, bad = 0;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    CHECK(GUOW(ws(), UOI_NAME, a, sizeof a, NULL));
    for (i = 0; i < 2000; i++) {
        if (!GUOW(ws(), UOI_NAME, b, sizeof b, NULL) || wcscmp(a, b)) bad++;
    }
    CHECK(bad == 0);
}
TEST(uoi_other_thread_sees_same_winsta) {
    /* Window station is per-process: a second thread resolves the same name */
    struct S { WCHAR name[64]; BOOL ok; HWINSTA h; };
    static struct S s;
    HANDLE th; DWORD tid;
    REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    DWORD WINAPI thr(LPVOID p) {
        s.h = U(GetProcessWindowStation)();
        s.ok = GUOW(s.h, UOI_NAME, s.name, sizeof s.name, NULL);
        return 0;
    }
    memset(&s, 0, sizeof s);
    th = CreateThread(NULL, 0, thr, NULL, 0, &tid);
    CHECK(th != NULL);
    if (!th) return;
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    CloseHandle(th);
    CHECK(s.ok);
    CHECK(s.h == ws());
}
TEST(uoi_thread_desktop_matches_other_thread) {
    static HDESK d;
    HANDLE th; DWORD tid;
    REQUIRE(U(GetThreadDesktop));
    DWORD WINAPI thr(LPVOID p) { d = U(GetThreadDesktop)(GetCurrentThreadId()); return 0; }
    d = NULL;
    th = CreateThread(NULL, 0, thr, NULL, 0, &tid);
    CHECK(th != NULL);
    if (!th) return;
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    CloseHandle(th);
    CHECK(d != NULL);
    CHECK(d == dk());
}
TEST(getthreaddesktop_invalid_thread) {
    HDESK d;
    REQUIRE(U(GetThreadDesktop));
    SetLastError(0xdeadbeef);
    d = U(GetThreadDesktop)(0);
    CHECK(d == NULL);
    t_note("GetThreadDesktop(0) err=%lu", GetLastError());
    SetLastError(0xdeadbeef);
    d = U(GetThreadDesktop)(0xFFFFFFF0);
    CHECK(d == NULL);
    t_note("GetThreadDesktop(bogus) err=%lu", GetLastError());
}

/* ---- SetUserObjectInformation ---- */
TEST(setuoi_timerproc_exception_suppression_bad_args) {
    DWORD v = 1; BOOL r;
    BOOL (WINAPI *su)(HANDLE, int, PVOID, DWORD) = (void *)t_proc("SetUserObjectInformationW");
    REQUIRE(su);
    /* index 7 only accepts the current process pseudo handle and a 4 byte value */
    SetLastError(0xdeadbeef); r = su(NULL, 7, &v, 4);
    CHECK(!r); CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef); r = su(GetCurrentProcess(), 7, &v, 0);
    CHECK(!r); CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef); r = su(GetCurrentProcess(), 7, &v, 3);
    CHECK(!r); CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef); r = su(GetCurrentProcess(), 7, &v, 5);
    CHECK(!r); CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef); r = su(GetCurrentProcess(), 7, &v, 8);
    CHECK(!r); CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef); r = su(GetCurrentProcess(), 7, &v, 0xFFFFFFFF);
    CHECK(!r); CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef); r = su((HANDLE)(ULONG_PTR)-2, 7, &v, 4);
    CHECK(!r); CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
}
TEST(setuoi_timerproc_exception_suppression_wrong_handle_type) {
    DWORD v = 1; BOOL r;
    BOOL (WINAPI *su)(HANDLE, int, PVOID, DWORD) = (void *)t_proc("SetUserObjectInformationW");
    REQUIRE(su); REQUIRE(U(GetProcessWindowStation));
    SetLastError(0xdeadbeef);
    r = su(ws(), 7, &v, 4);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0xdeadbeef);
    r = su(GetCurrentThread(), 7, &v, 4);
    CHECK(!r);
}
TEST(setuoi_timerproc_exception_suppression_ok) {
    DWORD v = 1; BOOL r;
    BOOL (WINAPI *su)(HANDLE, int, PVOID, DWORD) = (void *)t_proc("SetUserObjectInformationW");
    BOOL (WINAPI *sua)(HANDLE, int, PVOID, DWORD) = (void *)t_proc("SetUserObjectInformationA");
    REQUIRE(su);
    r = su(GetCurrentProcess(), 7, &v, 4);
    CHECKF(r, "err=%lu", GetLastError());
    if (sua) { r = sua(GetCurrentProcess(), 7, &v, 4); CHECKF(r, "A err=%lu", GetLastError()); }
}
TEST(setuoi_invalid_handle) {
    DWORD v[4] = { 0 }; BOOL r;
    BOOL (WINAPI *su)(HANDLE, int, PVOID, DWORD) = (void *)t_proc("SetUserObjectInformationW");
    REQUIRE(su);
    SetLastError(0xdeadbeef);
    r = su((HANDLE)(ULONG_PTR)0x12345678, UOI_FLAGS, v, sizeof(USEROBJECTFLAGS));
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0xdeadbeef);
    r = su(NULL, UOI_FLAGS, v, sizeof(USEROBJECTFLAGS));
    CHECK(!r);
    t_note("SetUserObjectInformation(NULL) err=%lu", GetLastError());
}
TEST(setuoi_invalid_index) {
    DWORD v[4] = { 0 }; BOOL r;
    BOOL (WINAPI *su)(HANDLE, int, PVOID, DWORD) = (void *)t_proc("SetUserObjectInformationW");
    REQUIRE(su); REQUIRE(U(GetProcessWindowStation));
    SetLastError(0xdeadbeef);
    r = su(ws(), 99, v, sizeof v);
    CHECK(!r);
    t_note("SetUserObjectInformation(index 99) err=%lu", GetLastError());
    SetLastError(0xdeadbeef);
    r = su(ws(), -1, v, sizeof v);
    CHECK(!r);
    SetLastError(0xdeadbeef);
    r = su(ws(), UOI_NAME, v, sizeof v);
    CHECK(!r);
    t_note("SetUserObjectInformation(UOI_NAME) err=%lu", GetLastError());
    SetLastError(0xdeadbeef);
    r = su(ws(), UOI_TYPE, v, sizeof v);
    CHECK(!r);
}
TEST(setuoi_flags_bad_size) {
    /* current flags are read first so that a surprising success is an identity change */
    USEROBJECTFLAGS f; BOOL r;
    BOOL (WINAPI *su)(HANDLE, int, PVOID, DWORD) = (void *)t_proc("SetUserObjectInformationW");
    REQUIRE(su); REQUIRE(GUOW); REQUIRE(U(GetProcessWindowStation));
    if (!GUOW(ws(), UOI_FLAGS, &f, sizeof f, NULL)) { CHECK(0); return; }
    SetLastError(0xdeadbeef);
    r = su(ws(), UOI_FLAGS, &f, 0);
    CHECK(!r);
    t_note("SetUserObjectInformation(flags,size 0) err=%lu", GetLastError());
    SetLastError(0xdeadbeef);
    r = su(ws(), UOI_FLAGS, &f, sizeof f - 1);
    CHECK(!r);
    t_note("SetUserObjectInformation(flags,size 11) err=%lu", GetLastError());
    SetLastError(0xdeadbeef);
    r = su(ws(), UOI_FLAGS, &f, 0xFFFFFFFF);
    t_note("SetUserObjectInformation(flags,size -1) -> %d err=%lu", r, GetLastError());
}

/* ---- Security descriptors (read only) ---- */
TEST(secdesc_get_dacl_winsta) {
    BYTE buf[2048]; DWORD n = 0; SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION; BOOL r;
    REQUIRE(U(GetUserObjectSecurity)); REQUIRE(U(GetProcessWindowStation));
    r = U(GetUserObjectSecurity)(ws(), &si, buf, sizeof buf, &n);
    if (!r) { t_note("GetUserObjectSecurity err=%lu", GetLastError()); if (GetLastError() == ERROR_ACCESS_DENIED) return; CHECK(0); return; }
    CHECK(n >= sizeof(SECURITY_DESCRIPTOR_RELATIVE));
    CHECK(IsValidSecurityDescriptor((PSECURITY_DESCRIPTOR)buf));
    {
        BOOL present = FALSE, def = FALSE; PACL acl = NULL;
        CHECK(GetSecurityDescriptorDacl((PSECURITY_DESCRIPTOR)buf, &present, &acl, &def));
        CHECK(present);
    }
}
TEST(secdesc_get_owner_winsta) {
    BYTE buf[2048]; DWORD n = 0; SECURITY_INFORMATION si = OWNER_SECURITY_INFORMATION; BOOL r;
    REQUIRE(U(GetUserObjectSecurity)); REQUIRE(U(GetProcessWindowStation));
    r = U(GetUserObjectSecurity)(ws(), &si, buf, sizeof buf, &n);
    if (!r) { t_note("owner: err=%lu", GetLastError()); return; }
    CHECK(IsValidSecurityDescriptor((PSECURITY_DESCRIPTOR)buf));
    {
        PSID o = NULL; BOOL def = FALSE;
        CHECK(GetSecurityDescriptorOwner((PSECURITY_DESCRIPTOR)buf, &o, &def));
        CHECK(o != NULL && IsValidSid(o));
    }
}
TEST(secdesc_get_size_query) {
    DWORD n = 0; SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION; BOOL r;
    BYTE buf[2048]; DWORD n2 = 0;
    REQUIRE(U(GetUserObjectSecurity)); REQUIRE(U(GetProcessWindowStation));
    SetLastError(0xdeadbeef);
    r = U(GetUserObjectSecurity)(ws(), &si, NULL, 0, &n);
    if (r || GetLastError() == ERROR_ACCESS_DENIED) { t_note("size query r=%d err=%lu", r, GetLastError()); return; }
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    CHECK(n > 0);
    if (n <= sizeof buf) {
        CHECK(U(GetUserObjectSecurity)(ws(), &si, buf, n, &n2));
        CHECK(n2 <= n);
        if (n > 0) {
            SetLastError(0xdeadbeef);
            r = U(GetUserObjectSecurity)(ws(), &si, buf, n - 1, &n2);
            CHECK(!r);
            CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
        }
    }
}
TEST(secdesc_get_desktop_dacl) {
    BYTE buf[2048]; DWORD n = 0; SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION; BOOL r;
    REQUIRE(U(GetUserObjectSecurity)); REQUIRE(U(GetThreadDesktop));
    r = U(GetUserObjectSecurity)(dk(), &si, buf, sizeof buf, &n);
    if (!r) { t_note("desktop sd err=%lu", GetLastError()); return; }
    CHECK(IsValidSecurityDescriptor((PSECURITY_DESCRIPTOR)buf));
}
TEST(secdesc_get_invalid_handle) {
    BYTE buf[256]; DWORD n = 0; SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION; BOOL r;
    REQUIRE(U(GetUserObjectSecurity));
    SetLastError(0xdeadbeef);
    r = U(GetUserObjectSecurity)((HANDLE)(ULONG_PTR)0x12345678, &si, buf, sizeof buf, &n);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0xdeadbeef);
    r = U(GetUserObjectSecurity)(NULL, &si, buf, sizeof buf, &n);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0xdeadbeef);
    r = U(GetUserObjectSecurity)(GetCurrentProcess(), &si, buf, sizeof buf, &n);
    CHECK(!r);
}
TEST(secdesc_set_invalid_handle) {
    SECURITY_DESCRIPTOR sd; SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION; BOOL r;
    REQUIRE(U(SetUserObjectSecurity));
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetLastError(0xdeadbeef);
    r = U(SetUserObjectSecurity)((HANDLE)(ULONG_PTR)0x12345678, &si, &sd);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0xdeadbeef);
    r = U(SetUserObjectSecurity)(NULL, &si, &sd);
    CHECK(!r);
    SetLastError(0xdeadbeef);
    r = U(SetUserObjectSecurity)(GetCurrentProcess(), &si, &sd);
    CHECK(!r);
}
TEST(secdesc_roundtrip_identity_winsta) {
    /* write back exactly what was read; skip if the handle lacks WRITE_DAC */
    BYTE buf[2048]; DWORD n = 0; SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION; BOOL r;
    BYTE buf2[2048]; DWORD n2 = 0;
    REQUIRE(U(GetUserObjectSecurity)); REQUIRE(U(SetUserObjectSecurity)); REQUIRE(U(GetProcessWindowStation));
    if (!U(GetUserObjectSecurity)(ws(), &si, buf, sizeof buf, &n)) { t_skip("cannot read DACL"); return; }
    r = U(SetUserObjectSecurity)(ws(), &si, (PSECURITY_DESCRIPTOR)buf);
    if (!r) { t_note("write back DACL: err=%lu", GetLastError()); return; }
    CHECK(U(GetUserObjectSecurity)(ws(), &si, buf2, sizeof buf2, &n2));
    CHECK(n2 == n);
    CHECK(!memcmp(buf, buf2, n < n2 ? n : n2));
}

/* ---- Process-wide state ---- */
TEST(winsta_other_apis_consistent) {
    HWINSTA h1, h2;
    REQUIRE(U(GetProcessWindowStation)); REQUIRE(U(OpenWindowStationW)); REQUIRE(U(CloseWindowStation));
    h1 = ws();
    CHECK(h1 != NULL);
    h2 = U(OpenWindowStationW)(L"WinSta0", FALSE, WINSTA_READATTRIBUTES);
    if (h2) {
        WCHAR a[64], b[64];
        CHECK(GUOW(h2, UOI_NAME, a, sizeof a, NULL));
        CHECK(GUOW(h1, UOI_NAME, b, sizeof b, NULL));
        CHECK(!wcscmp(a, L"WinSta0"));
        CHECK(U(CloseWindowStation)(h2));
    } else t_note("OpenWindowStation(WinSta0) err=%lu", GetLastError());
    /* the process window station handle is unchanged */
    CHECK(ws() == h1);
}
TEST(winsta_open_nonexistent) {
    HWINSTA h;
    REQUIRE(U(OpenWindowStationW));
    SetLastError(0xdeadbeef);
    h = U(OpenWindowStationW)(L"NoSuchWindowStation_t32", FALSE, WINSTA_READATTRIBUTES);
    CHECK(h == NULL);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
}
TEST(winsta_close_invalid) {
    BOOL r;
    REQUIRE(U(CloseWindowStation));
    SetLastError(0xdeadbeef);
    r = U(CloseWindowStation)((HWINSTA)(ULONG_PTR)0x12345678);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0xdeadbeef);
    r = U(CloseWindowStation)(NULL);
    CHECK(!r);
}
TEST(desktop_open_nonexistent) {
    HDESK h;
    REQUIRE(U(OpenDesktopW));
    SetLastError(0xdeadbeef);
    h = U(OpenDesktopW)(L"NoSuchDesktop_t32", 0, FALSE, DESKTOP_READOBJECTS);
    CHECK(h == NULL);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
}
TEST(desktop_close_invalid) {
    BOOL r;
    REQUIRE(U(CloseDesktop));
    SetLastError(0xdeadbeef);
    r = U(CloseDesktop)((HDESK)(ULONG_PTR)0x12345678);
    CHECK(!r);
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
}
TEST(desktop_close_current_thread_desktop_refused) {
    BOOL r;
    REQUIRE(U(CloseDesktop)); REQUIRE(U(GetThreadDesktop));
    SetLastError(0xdeadbeef);
    r = U(CloseDesktop)(dk());
    CHECK(!r);
    CHECK(GetLastError() == ERROR_BUSY || GetLastError() == ERROR_INVALID_HANDLE || GetLastError() == ERROR_ACCESS_DENIED);
    CHECK(dk() != NULL);
}
TEST(closewinsta_process_winsta_refused) {
    BOOL r;
    REQUIRE(U(CloseWindowStation)); REQUIRE(U(GetProcessWindowStation));
    SetLastError(0xdeadbeef);
    r = U(CloseWindowStation)(ws());
    CHECK(!r);
    CHECK(ws() != NULL);
}
