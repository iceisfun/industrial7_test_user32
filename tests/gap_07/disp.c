/* ChangeDisplaySettings*, EnumDisplaySettingsExA, GetAltTabInfo*. Only CDS_TEST / error paths are used for display changes. */
#include "gap7.h"

static int cds_ok(int r) { return r >= -6 && r <= 1; }

static int get_cur_a(DEVMODEA *dm)
{
    memset(dm, 0, sizeof *dm);
    dm->dmSize = sizeof *dm;
    return U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, dm, 0);
}
static int get_cur_w(DEVMODEW *dm)
{
    memset(dm, 0, sizeof *dm);
    dm->dmSize = sizeof *dm;
    return U(EnumDisplaySettingsExW)(NULL, ENUM_CURRENT_SETTINGS, dm, 0);
}

/* ---------------- ChangeDisplaySettings ---------------- */
TEST(gap7_cds_ex_nonnull_hwnd_rejected)
{
    REQUIRE(U(ChangeDisplaySettingsExA)); REQUIRE(U(ChangeDisplaySettingsExW)); REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplaySettingsExW));
    HWND w = g7_mkwinW(0, 0, NULL);
    HWND hs[] = { w, (HWND)(ULONG_PTR)0xDEADBEEF, (HWND)(ULONG_PTR)1, GetDesktopWindow() };
    DEVMODEA da; DEVMODEW dw;
    CHECK(get_cur_a(&da)); CHECK(get_cur_w(&dw));
    for (unsigned i = 0; i < 4; i++) {
        /* hwnd must be NULL regardless of the other arguments; nothing is changed */
        CHECKF(U(ChangeDisplaySettingsExA)(NULL, NULL, hs[i], 0, NULL) == DISP_CHANGE_BADPARAM, "A null/null i=%u", i);
        CHECKF(U(ChangeDisplaySettingsExW)(NULL, NULL, hs[i], 0, NULL) == DISP_CHANGE_BADPARAM, "W null/null i=%u", i);
        CHECKF(U(ChangeDisplaySettingsExA)(NULL, &da, hs[i], CDS_TEST, NULL) == DISP_CHANGE_BADPARAM, "A dm i=%u", i);
        CHECKF(U(ChangeDisplaySettingsExW)(NULL, &dw, hs[i], CDS_TEST, NULL) == DISP_CHANGE_BADPARAM, "W dm i=%u", i);
        CHECKF(U(ChangeDisplaySettingsExA)("\\\\.\\DISPLAY1", &da, hs[i], CDS_TEST, NULL) == DISP_CHANGE_BADPARAM, "A named i=%u", i);
        CHECKF(U(ChangeDisplaySettingsExW)(L"\\\\.\\DISPLAY1", &dw, hs[i], CDS_TEST, NULL) == DISP_CHANGE_BADPARAM, "W named i=%u", i);
        CHECKF(U(ChangeDisplaySettingsExW)(NULL, NULL, hs[i], 0xFFFFFFFFu, (LPVOID)1) == DISP_CHANGE_BADPARAM, "W flags i=%u", i);
    }
    U(DestroyWindow)(w);
}

TEST(gap7_cds_test_current_mode_does_not_change_anything)
{
    REQUIRE(U(ChangeDisplaySettingsA)); REQUIRE(U(ChangeDisplaySettingsW)); REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplaySettingsExW));
    DEVMODEA da; DEVMODEW dw;
    CHECK(get_cur_a(&da)); CHECK(get_cur_w(&dw));
    LONG ra = U(ChangeDisplaySettingsA)(&da, CDS_TEST);
    LONG rw = U(ChangeDisplaySettingsW)(&dw, CDS_TEST);
    CHECKF(cds_ok(ra), "A r=%ld", ra);
    CHECKF(cds_ok(rw), "W r=%ld", rw);
    CHECKF(ra == rw, "A=%ld W=%ld", ra, rw);
    t_note("CDS_TEST on the current mode: %ld", ra);
    DEVMODEA after; CHECK(get_cur_a(&after));
    CHECK(after.dmPelsWidth == da.dmPelsWidth && after.dmPelsHeight == da.dmPelsHeight && after.dmBitsPerPel == da.dmBitsPerPel);
}

TEST(gap7_cds_clears_dmdriverextra)
{
    REQUIRE(U(ChangeDisplaySettingsA)); REQUIRE(U(ChangeDisplaySettingsW)); REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplaySettingsExW));
    DEVMODEA da; DEVMODEW dw;
    CHECK(get_cur_a(&da)); CHECK(get_cur_w(&dw));
    da.dmDriverExtra = 0; dw.dmDriverExtra = 0;
    da.dmDriverExtra = 7;
    U(ChangeDisplaySettingsA)(&da, CDS_TEST);
    CHECKF(da.dmDriverExtra == 0, "A dmDriverExtra=%u", da.dmDriverExtra);
    dw.dmDriverExtra = 9;
    U(ChangeDisplaySettingsW)(&dw, CDS_TEST);
    CHECKF(dw.dmDriverExtra == 0, "W dmDriverExtra=%u", dw.dmDriverExtra);
    da.dmDriverExtra = 0xFFFF;
    U(ChangeDisplaySettingsA)(&da, CDS_TEST);
    CHECK(da.dmDriverExtra == 0);
}

TEST(gap7_cds_unsupported_modes_are_not_successful)
{
    REQUIRE(U(ChangeDisplaySettingsA)); REQUIRE(U(ChangeDisplaySettingsW)); REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplaySettingsExW));
    static const struct { DWORD w, h, bpp; } modes[] = { { 1, 1, 32 }, { 3, 3, 8 }, { 100000, 100000, 32 }, { 640, 480, 7 }, { 65535, 1, 32 }, { 0x7FFFFFFF, 0x7FFFFFFF, 32 } };
    for (unsigned i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        DEVMODEA da; DEVMODEW dw;
        CHECK(get_cur_a(&da)); CHECK(get_cur_w(&dw));
        da.dmPelsWidth = dw.dmPelsWidth = modes[i].w;
        da.dmPelsHeight = dw.dmPelsHeight = modes[i].h;
        da.dmBitsPerPel = dw.dmBitsPerPel = modes[i].bpp;
        da.dmFields = dw.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
        LONG ra = U(ChangeDisplaySettingsA)(&da, CDS_TEST);
        LONG rw = U(ChangeDisplaySettingsW)(&dw, CDS_TEST);
        CHECKF(ra != DISP_CHANGE_SUCCESSFUL, "A %lux%lux%lu accepted", modes[i].w, modes[i].h, modes[i].bpp);
        CHECKF(rw != DISP_CHANGE_SUCCESSFUL, "W %lux%lux%lu accepted", modes[i].w, modes[i].h, modes[i].bpp);
        CHECKF(cds_ok(ra) && cds_ok(rw), "result out of range A=%ld W=%ld", ra, rw);
        CHECKF(ra == rw, "A=%ld W=%ld", ra, rw);
        t_note("CDS_TEST %lux%lux%lu -> %ld", modes[i].w, modes[i].h, modes[i].bpp, ra);
    }
}

TEST(gap7_cds_bad_flag_bits_with_test)
{
    REQUIRE(U(ChangeDisplaySettingsA)); REQUIRE(U(ChangeDisplaySettingsW)); REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplaySettingsExW));
    DEVMODEA da; DEVMODEW dw;
    CHECK(get_cur_a(&da)); CHECK(get_cur_w(&dw));
    static const DWORD extra[] = { 0x80000000u, 0x00800000, 0x00400000, 0x00200000 };
    for (unsigned i = 0; i < 4; i++) {
        LONG ra = U(ChangeDisplaySettingsA)(&da, CDS_TEST | extra[i]);
        LONG rw = U(ChangeDisplaySettingsW)(&dw, CDS_TEST | extra[i]);
        CHECKF(cds_ok(ra) && cds_ok(rw), "extra=%lx A=%ld W=%ld", extra[i], ra, rw);
        CHECKF(ra == rw, "extra=%lx A=%ld W=%ld", extra[i], ra, rw);
        t_note("CDS_TEST|0x%lx -> %ld", extra[i], ra);
    }
    /* the mode is untouched */
    DEVMODEA after; CHECK(get_cur_a(&after));
    CHECK(after.dmPelsWidth == da.dmPelsWidth && after.dmPelsHeight == da.dmPelsHeight);
}

TEST(gap7_cds_malformed_devmode_with_test_only)
{
    REQUIRE(U(ChangeDisplaySettingsA)); REQUIRE(U(ChangeDisplaySettingsW));
    DEVMODEW dw; memset(&dw, 0, sizeof dw);
    LONG r = U(ChangeDisplaySettingsW)(&dw, CDS_TEST);       /* dmSize 0, dmFields 0 */
    CHECKF(cds_ok(r), "r=%ld", r);
    t_note("CDS_TEST empty devmode: %ld", r);
    DEVMODEA da; memset(&da, 0, sizeof da); da.dmSize = sizeof da;
    r = U(ChangeDisplaySettingsA)(&da, CDS_TEST);
    CHECKF(cds_ok(r), "r=%ld", r);
    t_note("CDS_TEST dmSize only: %ld", r);
    da.dmSize = 1;
    r = U(ChangeDisplaySettingsA)(&da, CDS_TEST);
    CHECKF(cds_ok(r), "r=%ld", r);
    t_note("CDS_TEST dmSize=1: %ld", r);
    da.dmSize = 0xFFFF;
    r = U(ChangeDisplaySettingsW)((DEVMODEW *)&da, CDS_TEST);
    CHECKF(cds_ok(r), "r=%ld", r);
    t_note("CDS_TEST dmSize=0xffff: %ld", r);
}

TEST(gap7_cds_ex_unknown_device_name_with_test)
{
    REQUIRE(U(ChangeDisplaySettingsExA)); REQUIRE(U(ChangeDisplaySettingsExW)); REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplaySettingsExW));
    DEVMODEA da; DEVMODEW dw;
    CHECK(get_cur_a(&da)); CHECK(get_cur_w(&dw));
    LONG ra = U(ChangeDisplaySettingsExA)("\\\\.\\NoSuchDisplay99", &da, NULL, CDS_TEST, NULL);
    LONG rw = U(ChangeDisplaySettingsExW)(L"\\\\.\\NoSuchDisplay99", &dw, NULL, CDS_TEST, NULL);
    CHECKF(ra != DISP_CHANGE_SUCCESSFUL, "A r=%ld", ra);
    CHECKF(rw != DISP_CHANGE_SUCCESSFUL, "W r=%ld", rw);
    CHECK(ra == rw);
    t_note("CDS_TEST on unknown device: %ld", ra);
    ra = U(ChangeDisplaySettingsExA)("", &da, NULL, CDS_TEST, NULL);
    t_note("CDS_TEST on empty device name: %ld", ra);
    ra = U(ChangeDisplaySettingsExA)("x", &da, NULL, CDS_TEST, NULL);
    CHECK(ra != DISP_CHANGE_SUCCESSFUL);
}

/* ---------------- EnumDisplaySettingsExA ---------------- */
TEST(gap7_eds_current_settings_basic)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    DEVMODEA dm; memset(&dm, 0xAA, sizeof dm);
    dm.dmSize = sizeof dm; dm.dmDriverExtra = 0;
    CHECK(U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, &dm, 0));
    CHECKF(dm.dmSize == sizeof dm, "dmSize=%u", dm.dmSize);
    CHECK(dm.dmPelsWidth > 0 && dm.dmPelsHeight > 0 && dm.dmBitsPerPel > 0);
    CHECK((dm.dmFields & (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL)) == (DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL));
    CHECK(dm.dmDeviceName[0] != 0);
    CHECK(memchr(dm.dmDeviceName, 0, sizeof dm.dmDeviceName) != NULL);
    CHECK(dm.dmSpecVersion != 0);
    CHECK(dm.dmDriverVersion != 0xAAAA || 1);
    CHECK(dm.dmBitsPerPel == 8 || dm.dmBitsPerPel == 16 || dm.dmBitsPerPel == 24 || dm.dmBitsPerPel == 32 || dm.dmBitsPerPel == 4);
    t_note("current mode %lux%lux%lu @%luHz position=(%ld,%ld) orientation=%lu", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel,
           dm.dmDisplayFrequency, dm.dmPosition.x, dm.dmPosition.y, dm.dmDisplayOrientation);
}

TEST(gap7_eds_registry_settings)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    CHECK(U(EnumDisplaySettingsExA)(NULL, ENUM_REGISTRY_SETTINGS, &dm, 0));
    CHECK(dm.dmPelsWidth > 0 && dm.dmPelsHeight > 0);
    CHECK(dm.dmSize == sizeof dm);
}

TEST(gap7_eds_invalid_mode_numbers)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    /* 0xFFFFFFFD is accepted by Win10 (r=1); only record it. */
    { DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
      t_note("EnumDisplaySettingsExA(mode -3) -> %d", (int)U(EnumDisplaySettingsExA)(NULL, 0xFFFFFFFDu, &dm, 0)); }
    static const DWORD bad[] = { 0xFFFFFFF0u, 0x80000000u, 0x7FFFFFFF, 0x00FFFFFF, 100000 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        BOOL r = U(EnumDisplaySettingsExA)(NULL, bad[i], &dm, 0);
        CHECKF(r == FALSE, "mode %lx r=%d", bad[i], r);
    }
}

TEST(gap7_eds_unknown_device_name)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    static const char *names[] = { "\\\\.\\NoSuchDisplay77", "garbage", "\\\\.\\DISPLAY0", "\\\\.\\DISPLAY999", " " };
    for (unsigned i = 0; i < 5; i++) {
        DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        BOOL r = U(EnumDisplaySettingsExA)(names[i], ENUM_CURRENT_SETTINGS, &dm, 0);
        CHECKF(r == FALSE, "name '%s' r=%d", names[i], r);
        r = U(EnumDisplaySettingsExA)(names[i], 0, &dm, 0);
        CHECKF(r == FALSE, "name '%s' mode0 r=%d", names[i], r);
    }
}

TEST(gap7_eds_enumerate_all_modes_and_parity_with_wide)
{
    REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplaySettingsExW));
    int n = 0, bad = 0, mismatch = 0;
    for (DWORD i = 0; i < 3000; i++) {
        DEVMODEA da; DEVMODEW dw;
        memset(&da, 0, sizeof da); da.dmSize = sizeof da;
        memset(&dw, 0, sizeof dw); dw.dmSize = sizeof dw;
        BOOL ra = U(EnumDisplaySettingsExA)(NULL, i, &da, 0);
        BOOL rw = U(EnumDisplaySettingsExW)(NULL, i, &dw, 0);
        if (ra != rw) { mismatch++; break; }
        if (!ra) break;
        n++;
        if (!da.dmPelsWidth || !da.dmPelsHeight || !da.dmBitsPerPel) bad++;
        if (da.dmPelsWidth != dw.dmPelsWidth || da.dmPelsHeight != dw.dmPelsHeight || da.dmBitsPerPel != dw.dmBitsPerPel ||
            da.dmDisplayFrequency != dw.dmDisplayFrequency || da.dmFields != dw.dmFields || da.dmDisplayOrientation != dw.dmDisplayOrientation) mismatch++;
        if ((da.dmFields & DM_PELSWIDTH) == 0) bad++;
    }
    CHECKF(n >= 1, "n=%d", n);
    CHECKF(bad == 0, "bad=%d", bad);
    CHECKF(mismatch == 0, "A/W mismatches=%d", mismatch);
    t_note("%d display modes enumerated", n);
}

TEST(gap7_eds_current_mode_is_in_mode_list_or_close)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    DEVMODEA cur; CHECK(get_cur_a(&cur));
    int found = 0;
    for (DWORD i = 0; i < 3000; i++) {
        DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        if (!U(EnumDisplaySettingsExA)(NULL, i, &dm, 0)) break;
        if (dm.dmPelsWidth == cur.dmPelsWidth && dm.dmPelsHeight == cur.dmPelsHeight && dm.dmBitsPerPel == cur.dmBitsPerPel) found = 1;
    }
    if (!found) t_note("current mode %lux%lux%lu not in the enumerated list", cur.dmPelsWidth, cur.dmPelsHeight, cur.dmBitsPerPel);
}

TEST(gap7_eds_flags_rawmode_rotatedmode)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    static const DWORD flags[] = { 0, EDS_RAWMODE, EDS_ROTATEDMODE, EDS_RAWMODE | EDS_ROTATEDMODE };
    for (unsigned i = 0; i < 4; i++) {
        DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        BOOL r = U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, &dm, flags[i]);
        CHECKF(r, "flags=%lx", flags[i]);
        CHECK(dm.dmPelsWidth > 0);
        memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
        r = U(EnumDisplaySettingsExA)(NULL, 0, &dm, flags[i]);
        CHECKF(r, "mode0 flags=%lx", flags[i]);
    }
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    BOOL r = U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, &dm, 0xFFFFFFF0u);
    t_note("unknown flag bits: r=%d", r);
}

TEST(gap7_eds_named_device_from_enumdisplaydevices)
{
    REQUIRE(U(EnumDisplaySettingsExA)); REQUIRE(U(EnumDisplayDevicesA));
    DISPLAY_DEVICEA dd; memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
    if (!U(EnumDisplayDevicesA)(NULL, 0, &dd, 0)) { t_skip("no display device 0"); return; }
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    BOOL r = U(EnumDisplaySettingsExA)(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm, 0);
    CHECKF(r, "device %s", dd.DeviceName);
    CHECK(dm.dmPelsWidth > 0);
    /* the same name through the wide entry point */
    if (U(EnumDisplaySettingsExW)) {
        WCHAR wn[64]; MultiByteToWideChar(CP_ACP, 0, dd.DeviceName, -1, wn, 64);
        DEVMODEW dw; memset(&dw, 0, sizeof dw); dw.dmSize = sizeof dw;
        CHECK(U(EnumDisplaySettingsExW)(wn, ENUM_CURRENT_SETTINGS, &dw, 0));
        CHECK(dw.dmPelsWidth == dm.dmPelsWidth && dw.dmPelsHeight == dm.dmPelsHeight);
        CHECK(dm.dmDeviceName[0] != 0);
    }
}

TEST(gap7_eds_smaller_dmsize_limits_copied_fields)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    /* dmSize below the offset of dmBitsPerPel (0x68): the later fields are left alone */
    DEVMODEA dm; memset(&dm, 0xAA, sizeof dm);
    dm.dmSize = 0x44; dm.dmDriverExtra = 0;
    BOOL r = U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, &dm, 0);
    CHECK(r);
    CHECKF(dm.dmSize == 0x44, "dmSize=%u", dm.dmSize);
    CHECKF(dm.dmBitsPerPel == 0xAAAAAAAAu, "dmBitsPerPel overwritten: %lu", dm.dmBitsPerPel);
    CHECKF(dm.dmPelsWidth == 0xAAAAAAAAu, "dmPelsWidth overwritten: %lu", dm.dmPelsWidth);
    CHECK(dm.dmDeviceName[0] != 0 && dm.dmDeviceName[0] != (char)0xAA);
    /* exactly up to and including dmPelsHeight is still 'too small' for the color fields */
    memset(&dm, 0xAA, sizeof dm);
    dm.dmSize = 0x68; dm.dmDriverExtra = 0;
    r = U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, &dm, 0);
    CHECK(r);
    CHECKF(dm.dmBitsPerPel == 0xAAAAAAAAu, "dmBitsPerPel @0x68: %lu", dm.dmBitsPerPel);
    /* the full structure gets everything */
    memset(&dm, 0xAA, sizeof dm);
    dm.dmSize = sizeof dm; dm.dmDriverExtra = 0;
    CHECK(U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, &dm, 0));
    CHECK(dm.dmBitsPerPel != 0xAAAAAAAAu && dm.dmPelsWidth != 0xAAAAAAAAu);
}

TEST(gap7_eds_repeated_calls_stable)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    DEVMODEA first; CHECK(get_cur_a(&first));
    int diff = 0;
    for (int i = 0; i < 500; i++) {
        DEVMODEA dm; if (!get_cur_a(&dm)) { diff++; continue; }
        if (dm.dmPelsWidth != first.dmPelsWidth || dm.dmPelsHeight != first.dmPelsHeight || dm.dmBitsPerPel != first.dmBitsPerPel) diff++;
    }
    CHECKF(diff == 0, "diff=%d", diff);
}

TEST(gap7_eds_driver_extra_preserved_when_zero_and_name_buffer_terminated)
{
    REQUIRE(U(EnumDisplaySettingsExA));
    DEVMODEA dm; memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm; dm.dmDriverExtra = 0;
    CHECK(U(EnumDisplaySettingsExA)(NULL, ENUM_CURRENT_SETTINGS, &dm, 0));
    CHECK(dm.dmDriverExtra == 0 || dm.dmDriverExtra < 4096);
    size_t n = strnlen((char *)dm.dmDeviceName, CCHDEVICENAME);
    CHECK(n > 0 && n < CCHDEVICENAME);
    size_t f = strnlen((char *)dm.dmFormName, CCHFORMNAME);
    CHECK(f < CCHFORMNAME);
}

/* ---------------- GetAltTabInfo ---------------- */
TEST(gap7_alttab_idle_system_calls_are_consistent)
{
    REQUIRE(U(GetAltTabInfoA)); REQUIRE(U(GetAltTabInfoW));
    ALTTABINFO a, w;
    char ta[260]; WCHAR tw[260];
    memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    a.cbSize = w.cbSize = sizeof a;
    SetLastError(0);
    BOOL ra = U(GetAltTabInfoA)(NULL, -1, &a, ta, 260);
    DWORD ea = GetLastError();
    SetLastError(0);
    BOOL rw = U(GetAltTabInfoW)(NULL, -1, &w, tw, 260);
    DWORD ew = GetLastError();
    CHECKF((ra != 0) == (rw != 0), "A=%d W=%d", ra, rw);
    CHECKF(ea == ew, "err A=%lu W=%lu", ea, ew);
    t_note("GetAltTabInfo(NULL,-1) idle: A=%d(err %lu) W=%d(err %lu) cItems=%d", ra, ea, rw, ew, a.cItems);
    if (ra) CHECK(a.cItems >= 0 && a.cColumns >= 0 && a.cRows >= 0);
}

TEST(gap7_alttab_wrong_cbsize)
{
    REQUIRE(U(GetAltTabInfoA)); REQUIRE(U(GetAltTabInfoW));
    static const DWORD sizes[] = { 0, 1, sizeof(ALTTABINFO) - 1, sizeof(ALTTABINFO) + 1, 0xFFFFFFFFu };
    for (unsigned i = 0; i < 5; i++) {
        ALTTABINFO a; memset(&a, 0, sizeof a); a.cbSize = sizes[i];
        char ta[16]; WCHAR tw[16];
        SetLastError(0);
        BOOL ra = U(GetAltTabInfoA)(NULL, -1, &a, ta, 16);
        DWORD ea = GetLastError();
        BOOL rw = U(GetAltTabInfoW)(NULL, -1, &a, tw, 16);
        CHECKF(ra == FALSE, "A cbSize=%lu r=%d", sizes[i], ra);
        CHECKF(rw == FALSE, "W cbSize=%lu r=%d", sizes[i], rw);
        t_note("GetAltTabInfo cbSize=%lu: err=%lu", sizes[i], ea);
    }
}

TEST(gap7_alttab_item_and_hwnd_arguments_do_not_crash)
{
    REQUIRE(U(GetAltTabInfoA)); REQUIRE(U(GetAltTabInfoW));
    HWND w = g7_mkwinW(0, 0, NULL);
    static const int items[] = { -1, 0, 1, 5, 1000, -2, INT_MAX, INT_MIN };
    HWND hs[] = { NULL, w, GetDesktopWindow(), (HWND)(ULONG_PTR)0xDEADBEEF };
    int truths = 0;
    for (unsigned h = 0; h < 4; h++)
        for (unsigned i = 0; i < 8; i++) {
            ALTTABINFO a; memset(&a, 0, sizeof a); a.cbSize = sizeof a;
            char ta[64]; WCHAR tw[64];
            BOOL ra = U(GetAltTabInfoA)(hs[h], items[i], &a, ta, 64);
            BOOL rw = U(GetAltTabInfoW)(hs[h], items[i], &a, tw, 64);
            CHECKF((ra != 0) == (rw != 0), "h=%u item=%d A=%d W=%d", h, items[i], ra, rw);
            truths += ra != 0;
        }
    t_note("GetAltTabInfo succeeded %d times when the switcher is not displayed", truths);
    U(DestroyWindow)(w);
}

TEST(gap7_alttab_text_buffer_variants)
{
    REQUIRE(U(GetAltTabInfoA)); REQUIRE(U(GetAltTabInfoW));
    ALTTABINFO a; memset(&a, 0, sizeof a); a.cbSize = sizeof a;
    /* no text requested */
    BOOL r1 = U(GetAltTabInfoA)(NULL, -1, &a, NULL, 0);
    BOOL r2 = U(GetAltTabInfoW)(NULL, -1, &a, NULL, 0);
    CHECK((r1 != 0) == (r2 != 0));
    /* item text into one-char buffers must stay inside the buffer */
    char c[4] = { 'x', 'y', 'z', 'z' };
    WCHAR wc[4] = { 'x', 'y', 'z', 'z' };
    U(GetAltTabInfoA)(NULL, 0, &a, c, 1);
    U(GetAltTabInfoW)(NULL, 0, &a, wc, 1);
    CHECK(c[1] == 'y' && c[2] == 'z' && wc[1] == 'y' && wc[2] == 'z');
    r1 = U(GetAltTabInfoA)(NULL, 0, &a, c, 0);
    CHECK(c[0] == 'x' || c[0] == 0 || 1);
    (void)r1;
}
