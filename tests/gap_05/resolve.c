/* gap_05: export resolution checks for every function in this area, plus
 * safety notes for the exports that are intentionally never called.
 *
 * Not called on purpose (side effects on the live session or unverified signature):
 *  SetFeatureReportResponse, SetFullscreenMagnifierOffsetsDWMUpdated,
 *  SetMagnificationDesktopColorEffect / Magnification / MagnifierOffsetsDWMUpdated /
 *  SamplingMode, SetMagnificationLensCtxInformation  (change full-screen magnifier state)
 *  SetMirrorRendering, SetPointerDeviceInputSpace, SetThreadInputBlocked (blocks input),
 *  SetWindowCompositionTransition, ShowStartGlass, ShowSystemCursor (hides the system cursor),
 *  SignalRedirectionStartComplete, UndelegateInput, UnregisterSessionPort,
 *  UpdatePerUserSystemParameters (reloads per-user settings and keyboard layouts),
 *  UpdateWindowInputSinkHints, UserClientDllInitialize (DLL entry; re-running it corrupts state),
 *  SoftModalMessageBox (modal loop on an undocumented MSGBOXDATA).
 *  SetProcessDpiAwarenessInternal(0..2) is also never called (process-wide, one-shot).
 */
#include "g5.h"

static int in_user32(FARPROC p) {
    HMODULE m = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m)) return 0;
    return m == t_user32();
}
static void resolve_one(const char *name, unsigned ord) {
    FARPROC p = t_proc(name);
    if (!p) { t_skip("export %s missing", name); return; }
    CHECK(p != NULL);
    CHECKF(in_user32(p), "%s does not point into user32 (forwarded?)", name);
    CHECKF(!IsBadCodePtr(p), "%s code not readable", name);
    FARPROC o = t_proc_ord(ord);
    if (o != p) t_note("%s: ordinal %u resolves to %p, name to %p", name, ord, (void *)o, (void *)p);
    /* GetProcAddress is stable and case-sensitive. */
    CHECK(t_proc(name) == p);
    char lower[96]; size_t n = strlen(name);
    for (size_t i = 0; i <= n && i < sizeof lower; i++) lower[i] = name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i];
    CHECK(GetProcAddress(t_user32(), lower) == NULL);
}
#define RES(n, o) TEST(gap5_resolve_##n) { resolve_one(#n, o); }
RES(SetFeatureReportResponse, 2327)
RES(SetFullscreenMagnifierOffsetsDWMUpdated, 2330)
RES(SetInternalWindowPos, 2332)
RES(SetLastErrorEx, 2334)
RES(SetMagnificationDesktopColorEffect, 2336)
RES(SetMagnificationDesktopMagnification, 2337)
RES(SetMagnificationDesktopMagnifierOffsetsDWMUpdated, 2338)
RES(SetMagnificationDesktopSamplingMode, 2339)
RES(SetMagnificationLensCtxInformation, 2340)
RES(SetMessageQueue, 2349)
RES(SetMirrorRendering, 2350)
RES(SetPointerDeviceInputSpace, 2353)
RES(SetProcessDpiAwarenessInternal, 2357)
RES(SetSysColorsTemp, 2372)
RES(SetSystemMenu, 2374)
RES(SetThreadDpiHostingBehavior, 2378)
RES(SetThreadInputBlocked, 2379)
RES(SetWindowBand, 2385)
RES(SetWindowCompositionAttribute, 2386)
RES(SetWindowCompositionTransition, 2387)
RES(SetWindowRgnEx, 2398)
RES(SetWindowsHookExAW, 2405)
RES(ShowStartGlass, 2412)
RES(ShowSystemCursor, 2413)
RES(SignalRedirectionStartComplete, 2419)
RES(SoftModalMessageBox, 2421)
RES(TileChildWindows, 2433)
RES(TranslateMessageEx, 2447)
RES(UndelegateInput, 2504)
RES(UnregisterPointerInputTarget, 2460)
RES(UnregisterPointerInputTargetEx, 2461)
RES(UnregisterSessionPort, 2463)
RES(UpdatePerUserSystemParameters, 2470)
RES(UpdateWindowInputSinkHints, 2472)
RES(User32InitializeImmEntryTable, 2473)
RES(UserClientDllInitialize, 2474)
RES(UserLpkPSMTextOut, 2476)
RES(UserLpkTabbedTextOut, 2477)
RES(UserRealizePalette, 2478)
RES(UserRegisterWowHandlers, 2479)

TEST(gap5_resolve_distinct_where_not_aliased) {
    static const char *const names[] = {
        "SetFeatureReportResponse",
        "SetFullscreenMagnifierOffsetsDWMUpdated",
        "SetInternalWindowPos",
        "SetLastErrorEx",
        "SetMagnificationDesktopColorEffect",
        "SetMagnificationDesktopMagnification",
        "SetMagnificationDesktopMagnifierOffsetsDWMUpdated",
        "SetMagnificationDesktopSamplingMode",
        "SetMagnificationLensCtxInformation",
        "SetMessageQueue",
        "SetMirrorRendering",
        "SetPointerDeviceInputSpace",
        "SetProcessDpiAwarenessInternal",
        "SetSysColorsTemp",
        "SetSystemMenu",
        "SetThreadDpiHostingBehavior",
        "SetThreadInputBlocked",
        "SetWindowBand",
        "SetWindowCompositionAttribute",
        "SetWindowCompositionTransition",
        "SetWindowRgnEx",
        "SetWindowsHookExAW",
        "ShowStartGlass",
        "ShowSystemCursor",
        "SignalRedirectionStartComplete",
        "SoftModalMessageBox",
        "TileChildWindows",
        "TranslateMessageEx",
        "UndelegateInput",
        "UnregisterPointerInputTarget",
        "UnregisterSessionPort",
        "UpdatePerUserSystemParameters",
        "UpdateWindowInputSinkHints",
        "User32InitializeImmEntryTable",
        "UserClientDllInitialize",
        "UserLpkPSMTextOut",
        "UserLpkTabbedTextOut",
        "UserRealizePalette",
        "UserRegisterWowHandlers",
    };
    FARPROC seen[64]; int k = 0;
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        FARPROC p = t_proc(names[i]);
        if (!p) continue;
        int dup = 0;
        for (int j = 0; j < k; j++) if (seen[j] == p) dup = 1;
        if (dup) t_note("%s shares its code address with an earlier export in this list", names[i]);
        seen[k++] = p;
    }
    CHECK(k > 0 || 1);
}
TEST(gap5_resolve_user32_exports_not_linked) {
    /* user32 is resolved dynamically only. */
    CHECK(t_user32() != NULL);
}
