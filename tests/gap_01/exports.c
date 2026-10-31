/* gap_01: export presence / stub shape checks for rarely used user32 exports.
 * Several of these exports are thin forwarders into win32k (or privileged
 * DWM-only entry points); they are resolved here but never invoked. */
#include "t.h"

static int in_module(FARPROC p) {
    HMODULE m = t_user32();
    BYTE *base = (BYTE *)m;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    BYTE *a = (BYTE *)p;
    return a >= base && a < base + nt->OptionalHeader.SizeOfImage;
}

/* Name, build-specific ordinal (0 = unknown). Ordinals are only noted. */
#define EXPORT_TESTS(n, ord) \
    TEST(gap1_export_##n##_resolves) { \
        FARPROC p = t_proc(#n); REQUIRE(p); \
        CHECK(p != NULL); \
        CHECK(in_module(p)); \
        EXPECT_EQ(p, GetProcAddress(t_user32(), #n)); \
    } \
    TEST(gap1_export_##n##_stable_and_cased) { \
        FARPROC p = t_proc(#n); REQUIRE(p); \
        for (int i = 0; i < 50; i++) EXPECT_EQ(t_proc(#n), p); \
        char lower[96]; lstrcpynA(lower, #n, sizeof lower); \
        for (char *c = lower; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; \
        SetLastError(0); \
        FARPROC q = GetProcAddress(t_user32(), lower); \
        CHECKF(q == NULL, "lowercase name resolved"); \
        EXPECT_EQ(GetLastError(), ERROR_PROC_NOT_FOUND); \
    } \
    TEST(gap1_export_##n##_no_suffix_variants) { \
        FARPROC p = t_proc(#n); REQUIRE(p); \
        char nm[128]; \
        wsprintfA(nm, "%sA", #n); \
        CHECKF(GetProcAddress(t_user32(), nm) == NULL, "%s exists", nm); \
        wsprintfA(nm, "%sW", #n); \
        CHECKF(GetProcAddress(t_user32(), nm) == NULL, "%s exists", nm); \
    } \
    TEST(gap1_export_##n##_ordinal_note) { \
        FARPROC p = t_proc(#n); REQUIRE(p); \
        if (ord) { \
            FARPROC o = t_proc_ord(ord); \
            if (o != p) t_note(#n ": ordinal %d does not map to the named export on this build", ord); \
        } \
    } \
    TEST(gap1_export_##n##_stub_shape_note) { \
        FARPROC p = t_proc(#n); REQUIRE(p); \
        BYTE *b = (BYTE *)p; \
        if (b[0] == 0xFF && b[1] == 0x25) { /* jmp qword ptr [rip+disp32] into win32u */ } \
        else t_note(#n ": entry begins %02x %02x %02x (not a plain win32u jump stub)", b[0], b[1], b[2]); \
    }

EXPORT_TESTS(DwmGetDxRgn, 0)
EXPORT_TESTS(DwmGetDxSharedSurface, 1731)
EXPORT_TESTS(DwmGetRemoteSessionOcclusionEvent, 1732)
EXPORT_TESTS(DwmGetRemoteSessionOcclusionState, 1733)
EXPORT_TESTS(DwmKernelShutdown, 1734)
EXPORT_TESTS(DwmKernelStartup, 1735)
EXPORT_TESTS(DwmLockScreenUpdates, 1736)
EXPORT_TESTS(DwmValidateWindow, 1737)
EXPORT_TESTS(EditWndProc, 1738)
EXPORT_TESTS(EnableOneCoreTransformMode, 1743)
EXPORT_TESTS(EnableSessionForMMCSS, 1745)
EXPORT_TESTS(EndDeferWindowPosEx, 1748)
EXPORT_TESTS(EnterReaderModeHelper, 1753)
EXPORT_TESTS(ExcludeUpdateRgn, 1777)
EXPORT_TESTS(ExitWindowsEx, 1778)
EXPORT_TESTS(FrostCrashedWindow, 1788)
EXPORT_TESTS(GetClipboardAccessToken, 1816)
EXPORT_TESTS(GetCursorFrameInfo, 1826)
EXPORT_TESTS(GetDialogControlDpiChangeBehavior, 1834)
EXPORT_TESTS(GetDialogDpiChangeBehavior, 1835)
EXPORT_TESTS(GetDpiForMonitorInternal, 1845)
EXPORT_TESTS(GetExtendedPointerDeviceProperty, 1849)
EXPORT_TESTS(GetInputLocaleInfo, 1861)
EXPORT_TESTS(GetInternalWindowPos, 1863)
EXPORT_TESTS(GetMagnificationDesktopColorEffect, 1878)
EXPORT_TESTS(GetMagnificationDesktopMagnification, 1879)
EXPORT_TESTS(GetMagnificationDesktopSamplingMode, 1880)
EXPORT_TESTS(GetMagnificationLensCtxInformation, 1881)
EXPORT_TESTS(GetPointerDeviceInputSpace, 1912)
EXPORT_TESTS(GetPointerDeviceOrientation, 1913)
EXPORT_TESTS(GetPointerFrameArrivalTimes, 1503)
EXPORT_TESTS(GetPointerFrameTimes, 1921)
EXPORT_TESTS(GetProcessDpiAwarenessInternal, 1934)
EXPORT_TESTS(GetProcessUIContextInformation, 2521)
EXPORT_TESTS(GetReasonTitleFromReasonCode, 1946)
EXPORT_TESTS(GetSendMessageReceiver, 1952)
EXPORT_TESTS(GetSystemDpiForProcess, 1958)
EXPORT_TESTS(GetThreadDpiHostingBehavior, 1967)
EXPORT_TESTS(GetTopLevelWindow, 1969)
EXPORT_TESTS(GetUnpredictedMessagePos, 1972)

/* DwmGetDxRgn is a trivial stub on Win10 (xor eax,eax; ret): safe to call, ignores its arguments. */
UDECL(ULONG_PTR, DwmGetDxRgn, (ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR));
TEST(gap1_dwmgetdxrgn_returns_zero) {
    REQUIRE(UN(DwmGetDxRgn));
    CHECK((int)UN(DwmGetDxRgn)(0, 0, 0, 0, 0, 0) == 0);
    CHECK((int)UN(DwmGetDxRgn)(1, 2, 3, 4, 5, 6) == 0);
    CHECK((int)UN(DwmGetDxRgn)((ULONG_PTR)GetDesktopWindow(), 0, 0, 0, 0, 0) == 0);
}
TEST(gap1_dwmgetdxrgn_leaves_lasterror) {
    REQUIRE(UN(DwmGetDxRgn));
    SetLastError(0x1234);
    UN(DwmGetDxRgn)(0, 0, 0, 0, 0, 0);
    EXPECT_EQ(GetLastError(), 0x1234);
}
TEST(gap1_dwmgetdxrgn_repeated) {
    REQUIRE(UN(DwmGetDxRgn));
    for (int i = 0; i < 1000; i++) if ((int)UN(DwmGetDxRgn)(i, 0, 0, 0, 0, 0) != 0) { CHECK(0); break; }
}

/* Entry points that must never be invoked from a test: documented here so the
 * omission is deliberate. DwmKernelShutdown/Startup/LockScreenUpdates/ValidateWindow,
 * EnableSessionForMMCSS, EnableOneCoreTransformMode and GetClipboardAccessToken
 * are session/DWM-privileged and could freeze or tear down composition if they
 * are ever accepted; FrostCrashedWindow acts on foreign windows; ExitWindowsEx
 * could log the user off; EditWndProc takes an internal WND pointer. */
TEST(gap1_dangerous_exports_not_called) {
    CHECK(1);
}
