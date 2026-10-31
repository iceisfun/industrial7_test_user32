/* Desktop / window-station entry points. Only invalid-handle (error) paths are exercised for anything that could
 * switch desktops, lock the window station or change the logged-on user; undocumented exports are only resolved. */
#include "gap7.h"

UDECL(BOOL, SwitchDesktopWithFade, (HDESK, ULONG));
UDECL(BOOL, LockWindowStation, (HWINSTA));
UDECL(BOOL, UnlockWindowStation, (HWINSTA));
UDECL(BOOL, SetWindowStationUser, (HWINSTA, PLUID, PSID, DWORD));
UDECL(HDESK, GetInputDesktop, (void));

static const ULONG_PTR bad_handles[] = { 0, 0x4, 0x1234, 0xDEADBEEC, 0xDEADBEEF, 0x7FFFFFFC, (ULONG_PTR)-1 };
#define NBAD (sizeof bad_handles / sizeof bad_handles[0])

TEST(gap7_desk_switchdesktop_invalid_handles)
{
    REQUIRE(U(SwitchDesktop));
    for (unsigned i = 0; i < NBAD; i++) {
        SET_ERR(0);
        BOOL r = U(SwitchDesktop)((HDESK)bad_handles[i]);
        CHECKF(r == FALSE, "handle %llx r=%d", (unsigned long long)bad_handles[i], r);
        DWORD e = GetLastError();
        CHECKF(e != 0, "handle %llx no error", (unsigned long long)bad_handles[i]);
        if (e != ERROR_INVALID_HANDLE) t_note("SwitchDesktop(%llx): error %lu", (unsigned long long)bad_handles[i], e);
    }
}

TEST(gap7_desk_switchdesktop_wrong_object_type)
{
    REQUIRE(U(SwitchDesktop));
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    CHECK(ev != NULL);
    SET_ERR(0);
    BOOL r = U(SwitchDesktop)((HDESK)ev);
    CHECK(r == FALSE);
    t_note("SwitchDesktop(event handle): error %lu", GetLastError());
    CloseHandle(ev);
    HANDLE ev2 = CreateEventW(NULL, FALSE, FALSE, NULL);
    CloseHandle(ev2);                 /* closed handle */
    SET_ERR(0);
    CHECK(U(SwitchDesktop)((HDESK)ev2) == FALSE);
    CHECK(GetLastError() != 0);
}

TEST(gap7_desk_switchdesktop_closed_desktop_handle)
{
    REQUIRE(U(SwitchDesktop)); REQUIRE(U(CreateDesktopW)); REQUIRE(U(CloseDesktop));
    HDESK d = U(CreateDesktopW)(L"gap7_closed_desktop", NULL, NULL, 0, DESKTOP_SWITCHDESKTOP | DESKTOP_CREATEWINDOW, NULL);
    if (!d) { t_skip("cannot create a desktop (err %lu)", GetLastError()); return; }
    CHECK(U(CloseDesktop)(d));
    SET_ERR(0);
    /* the handle is closed: switching must fail, and it must not have switched anywhere */
    CHECK(U(SwitchDesktop)(d) == FALSE);
    CHECK(GetLastError() != 0);
}

TEST(gap7_desk_switchdesktopwithfade_invalid_handles)
{
    if (g7_is_wine()) { t_skip("export is an aborting stub under Wine"); return; }
    SwitchDesktopWithFade_pfn f = UN(SwitchDesktopWithFade);
    REQUIRE(f);
    static const ULONG fades[] = { 0, 1, 500, 0xFFFFFFFFu };
    for (unsigned i = 0; i < NBAD; i++)
        for (unsigned j = 0; j < 4; j++) {
            SET_ERR(0);
            BOOL r = f((HDESK)bad_handles[i], fades[j]);
            CHECKF(r == FALSE, "handle %llx fade %lu r=%d", (unsigned long long)bad_handles[i], fades[j], r);
        }
    SET_ERR(0);
    f((HDESK)0xDEADBEEC, 0);
    t_note("SwitchDesktopWithFade(bogus): error %lu", GetLastError());
}

TEST(gap7_winsta_lockwindowstation_invalid_handles)
{
    if (g7_is_wine()) { t_skip("export is an aborting stub under Wine"); return; }
    LockWindowStation_pfn f = UN(LockWindowStation);
    REQUIRE(f);
    for (unsigned i = 0; i < NBAD; i++) {
        SET_ERR(0);
        BOOL r = f((HWINSTA)bad_handles[i]);
        CHECKF(r == FALSE, "handle %llx r=%d", (unsigned long long)bad_handles[i], r);
        CHECKF(GetLastError() != 0, "handle %llx no error", (unsigned long long)bad_handles[i]);
    }
    SET_ERR(0);
    f((HWINSTA)0xDEADBEEC);
    t_note("LockWindowStation(bogus): error %lu (6=invalid handle, 5=access denied)", GetLastError());
}

TEST(gap7_winsta_unlockwindowstation_invalid_handles)
{
    if (g7_is_wine()) { t_skip("export is an aborting stub under Wine"); return; }
    UnlockWindowStation_pfn f = UN(UnlockWindowStation);
    REQUIRE(f);
    for (unsigned i = 0; i < NBAD; i++) {
        SET_ERR(0);
        BOOL r = f((HWINSTA)bad_handles[i]);
        CHECKF(r == FALSE, "handle %llx r=%d", (unsigned long long)bad_handles[i], r);
        CHECKF(GetLastError() != 0, "handle %llx no error", (unsigned long long)bad_handles[i]);
    }
    SET_ERR(0);
    f((HWINSTA)0xDEADBEEC);
    t_note("UnlockWindowStation(bogus): error %lu", GetLastError());
}

TEST(gap7_winsta_lock_wrong_object_type)
{
    if (g7_is_wine()) { t_skip("export is an aborting stub under Wine"); return; }
    LockWindowStation_pfn l = UN(LockWindowStation);
    UnlockWindowStation_pfn u = UN(UnlockWindowStation);
    REQUIRE(l); REQUIRE(u);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    SET_ERR(0);
    CHECK(l((HWINSTA)ev) == FALSE);
    DWORD e1 = GetLastError();
    SET_ERR(0);
    CHECK(u((HWINSTA)ev) == FALSE);
    t_note("event handle as window station: lock err %lu, unlock err %lu", e1, GetLastError());
    CloseHandle(ev);
}

TEST(gap7_winsta_setwindowstationuser_invalid_handles)
{
    if (g7_is_wine()) { t_skip("export is an aborting stub under Wine"); return; }
    SetWindowStationUser_pfn f = UN(SetWindowStationUser);
    REQUIRE(f);
    for (unsigned i = 0; i < NBAD; i++) {
        LUID luid = { 0, 0 };
        SET_ERR(0);
        BOOL r = f((HWINSTA)bad_handles[i], &luid, NULL, 0);
        CHECKF(r == FALSE, "handle %llx r=%d", (unsigned long long)bad_handles[i], r);
        CHECKF(GetLastError() != 0, "handle %llx no error", (unsigned long long)bad_handles[i]);
        CHECK(luid.LowPart == 0 && luid.HighPart == 0);
    }
    SET_ERR(0);
    LUID l2 = { 1, 1 };
    f((HWINSTA)0xDEADBEEC, &l2, NULL, 0);
    t_note("SetWindowStationUser(bogus): error %lu", GetLastError());
}

TEST(gap7_winsta_setwindowstationuser_wrong_type)
{
    if (g7_is_wine()) { t_skip("export is an aborting stub under Wine"); return; }
    SetWindowStationUser_pfn f = UN(SetWindowStationUser);
    REQUIRE(f);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    LUID luid = { 0, 0 };
    SET_ERR(0);
    CHECK(f((HWINSTA)ev, &luid, NULL, 0) == FALSE);
    t_note("SetWindowStationUser(event): error %lu", GetLastError());
    CloseHandle(ev);
}

TEST(gap7_desk_getinputdesktop_observed)
{
    if (g7_is_wine()) { t_skip("export is an aborting stub under Wine"); return; }
    GetInputDesktop_pfn f = UN(GetInputDesktop);
    REQUIRE(f);
    SET_ERR(0);
    HDESK d = f();
    t_note("GetInputDesktop() = %p (err %lu)", d, GetLastError());
    HDESK d2 = f();
    CHECKF(d == d2 || (d && d2), "two calls: %p vs %p", d, d2);
    if (U(GetThreadDesktop)) {
        HDESK td = U(GetThreadDesktop)(GetCurrentThreadId());
        CHECK(td != NULL);
        t_note("GetThreadDesktop = %p", td);
    }
}

TEST(gap7_desk_exports_present_for_undocumented_entry_points)
{
    static const char *names[] = { "SwitchDesktop", "SwitchDesktopWithFade", "OpenThreadDesktop", "LockWindowStation", "UnlockWindowStation",
                                   "SetWindowStationUser", "GetInputDesktop", "UpdateDefaultDesktopThumbnail", "GetWinStationInfo" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        FARPROC p = t_proc(names[i]);
        CHECKF(p != NULL, "%s missing", names[i]);
        HMODULE m = NULL;
        CHECKF(p && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m) && m == t_user32(), "%s not inside user32", names[i]);
    }
}
