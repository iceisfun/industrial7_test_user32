/* gap_00: export presence, placement inside user32, and ordinals for the
 * undocumented / syscall-forwarding exports. These are resolved only; none of
 * them is called because their signatures are not public or their effect is
 * global (input blocking, console/IME control, shutdown, DComposition, ...). */
#include "gap0.h"

static int in_user32(FARPROC p) {
    HMODULE m = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m)) return 0;
    return m == t_user32();
}

#define RES(name, ord) \
TEST(gap0_resolve_##name) { \
    FARPROC p = t_proc(#name); \
    REQUIRE(p); \
    CHECK(p != NULL); \
    CHECK(in_user32(p)); \
    if (ord) { FARPROC o = t_proc_ord(ord); if (o != p) t_note(#name ": ordinal %d resolves to %p, name to %p", ord, (void *)o, (void *)p); } \
    CHECK(GetProcAddress(t_user32(), #name "_NoSuchSuffix") == NULL); \
}

RES(AddVisualIdentifier, 1506)
RES(AlignRects, 1510)
RES(AllowForegroundActivation, 1511)
RES(AnyPopup, 1514)
RES(BlockInput, 1522)
RES(BroadcastSystemMessageW, 1528)
RES(BuildReasonArray, 1529)
RES(CalcMenuBar, 1530)
RES(CalculatePopupWindowPosition, 1531)
RES(CancelShutdown, 1538)
RES(CascadeChildWindows, 1539)
RES(CheckBannedOneCoreTransformApi, 0)
RES(CheckDBCSEnabledExt, 1574)
RES(CheckProcessForClipboardAccess, 1578)
RES(CheckProcessSession, 1579)
RES(CliImmSetHotKey, 1584)
RES(ClientThreadSetup, 0)
RES(ConsoleControl, 1594)
RES(ControlMagnification, 1595)
RES(CreateDCompositionHwndTarget, 1606)
RES(CreateDialogIndirectParamAorW, 1612)
RES(CreateSystemThreads, 1625)
RES(CreateWindowInBand, 1628)
RES(CreateWindowInBandEx, 1629)
RES(CreateWindowIndirect, 1630)
RES(CsrBroadcastSystemMessageExW, 1633)
RES(CtxInitUser32, 1634)
RES(DeferWindowPosAndBand, 1678)
RES(DelegateInput, 2503)
RES(DestroyDCompositionHwndTarget, 1684)
RES(DestroyReasons, 1687)
RES(DialogBoxIndirectParamAorW, 1691)
RES(DisplayExitWindowsWarnings, 0)
RES(DoSoundConnect, 1709)
RES(DoSoundDisconnect, 1710)
RES(DragObject, 1712)
RES(DrawCaptionTempA, 1715)
RES(DrawCaptionTempW, 1716)
RES(DrawFrame, 1719)
RES(DrawMenuBarTemp, 1724)

TEST(gap0_resolve_case_sensitive) {
    REQUIRE(t_proc("AnyPopup"));
    CHECK(GetProcAddress(t_user32(), "anypopup") == NULL);
    CHECK(GetProcAddress(t_user32(), "ANYPOPUP") == NULL);
    CHECK(GetProcAddress(t_user32(), "AlignRects ") == NULL);
    CHECK(GetProcAddress(t_user32(), "") == NULL);
}
TEST(gap0_resolve_ordinal_out_of_range) {
    CHECK(GetProcAddress(t_user32(), (LPCSTR)(ULONG_PTR)0xFFFF) == NULL);
    CHECK(GetProcAddress(t_user32(), (LPCSTR)(ULONG_PTR)0) == NULL);
}
TEST(gap0_resolve_stable_across_calls) {
    FARPROC a = t_proc("DrawFrame");
    REQUIRE(a);
    for (int i = 0; i < 100; i++) if (t_proc("DrawFrame") != a) { CHECK(0); return; }
    CHECK(1);
}
TEST(gap0_resolve_distinct_a_w_pairs) {
    FARPROC a = t_proc("DrawCaptionTempA"), w = t_proc("DrawCaptionTempW");
    REQUIRE(a); REQUIRE(w);
    CHECK(a != w);
}
TEST(gap0_resolve_distinct_inband) {
    FARPROC a = t_proc("CreateWindowInBand"), b = t_proc("CreateWindowInBandEx"), c = t_proc("CreateWindowIndirect");
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    CHECK(a != b); CHECK(a != c); CHECK(b != c);
}
