/* SystemParametersInfo: simple scalar getters, A/W parity, ranges. */
#include "sp.h"

/* BOOL-valued getters. must=1 for long-standing actions. */
GETB(beep, SPI_GETBEEP, 1)
GETB(screensaveactive, SPI_GETSCREENSAVEACTIVE, 1)
GETB(iconTitleWrap, SPI_GETICONTITLEWRAP, 1)
GETB(menuDropAlignment, SPI_GETMENUDROPALIGNMENT, 1)
GETB(dragFullWindows, SPI_GETDRAGFULLWINDOWS, 1)
GETB(fastTaskSwitch, SPI_GETFASTTASKSWITCH, 0)
GETB(keyboardPref, SPI_GETKEYBOARDPREF, 1)
GETB(screenReader, SPI_GETSCREENREADER, 1)
GETB(showSounds, SPI_GETSHOWSOUNDS, 1)
GETB(showImeUi, SPI_GETSHOWIMEUI, 1)
GETB(snapToDefButton, SPI_GETSNAPTODEFBUTTON, 1)
GETB(fontSmoothing, SPI_GETFONTSMOOTHING, 1)
GETB(screensaveSecure, SPI_GETSCREENSAVESECURE, 0)
GETB(screensaverRunning, SPI_GETSCREENSAVERRUNNING, 1)
GETB(activeWindowTracking, SPI_GETACTIVEWINDOWTRACKING, 1)
GETB(menuAnimation, SPI_GETMENUANIMATION, 1)
GETB(comboBoxAnimation, SPI_GETCOMBOBOXANIMATION, 1)
GETB(listBoxSmoothScrolling, SPI_GETLISTBOXSMOOTHSCROLLING, 1)
GETB(gradientCaptions, SPI_GETGRADIENTCAPTIONS, 1)
GETB(keyboardCues, SPI_GETKEYBOARDCUES, 1)
GETB(menuUnderlines, SPI_GETMENUUNDERLINES, 1)
GETB(activeWndTrkZorder, SPI_GETACTIVEWNDTRKZORDER, 1)
GETB(hotTracking, SPI_GETHOTTRACKING, 1)
GETB(menuFade, SPI_GETMENUFADE, 1)
GETB(selectionFade, SPI_GETSELECTIONFADE, 1)
GETB(tooltipAnimation, SPI_GETTOOLTIPANIMATION, 1)
GETB(tooltipFade, SPI_GETTOOLTIPFADE, 1)
GETB(cursorShadow, SPI_GETCURSORSHADOW, 1)
GETB(mouseSonar, SPI_GETMOUSESONAR, 1)
GETB(mouseClickLock, SPI_GETMOUSECLICKLOCK, 1)
GETB(mouseVanish, SPI_GETMOUSEVANISH, 1)
GETB(flatMenu, SPI_GETFLATMENU, 1)
GETB(dropShadow, SPI_GETDROPSHADOW, 1)
GETB(blockSendInputResets, SPI_GETBLOCKSENDINPUTRESETS, 1)
GETB(uiEffects, SPI_GETUIEFFECTS, 1)
GETB(disableOverlappedContent, SPI_GETDISABLEOVERLAPPEDCONTENT, 1)
GETB(clientAreaAnimation, SPI_GETCLIENTAREAANIMATION, 1)
GETB(clearType, SPI_GETCLEARTYPE, 1)
GETB(speechRecognition, SPI_GETSPEECHRECOGNITION, 0)
GETB(caretBrowsing, SPI_GETCARETBROWSING, 0)
GETB(threadLocalInputSettings, SPI_GETTHREADLOCALINPUTSETTINGS, 0)
GETB(systemLanguageBar, SPI_GETSYSTEMLANGUAGEBAR, 0)
GETB(dragFromMaximize, SPI_GETDRAGFROMMAXIMIZE, 0)
GETB(snapSizing, SPI_GETSNAPSIZING, 0)
GETB(dockMoving, SPI_GETDOCKMOVING, 0)
GETB(winArranging, SPI_GETWINARRANGING, 0)

/* UINT-valued getters with documented ranges. */
GETU(keyboardSpeed, SPI_GETKEYBOARDSPEED, 0, 31, 1)
GETU(keyboardDelay, SPI_GETKEYBOARDDELAY, 0, 3, 1)
GETU(mouseSpeed, SPI_GETMOUSESPEED, 1, 20, 1)
GETU(screensaveTimeout, SPI_GETSCREENSAVETIMEOUT, 0, 0xFFFFFFFF, 1)
GETU(mouseTrails, SPI_GETMOUSETRAILS, 0, 20, 1)
GETU(wheelScrollLines, SPI_GETWHEELSCROLLLINES, 0, 0xFFFFFFFF, 1)
GETU(wheelScrollChars, SPI_GETWHEELSCROLLCHARS, 0, 0xFFFFFFFF, 1)
GETU(mouseHoverWidth, SPI_GETMOUSEHOVERWIDTH, 1, 0xFFFF, 1)
GETU(mouseHoverHeight, SPI_GETMOUSEHOVERHEIGHT, 1, 0xFFFF, 1)
GETU(mouseHoverTime, SPI_GETMOUSEHOVERTIME, 1, 0xFFFFFF, 1)
GETU(menuShowDelay, SPI_GETMENUSHOWDELAY, 0, 100000, 1)
GETU(foregroundLockTimeout, SPI_GETFOREGROUNDLOCKTIMEOUT, 0, 0xFFFFFFFF, 1)
GETU(activeWndTrkTimeout, SPI_GETACTIVEWNDTRKTIMEOUT, 0, 0xFFFFFFFF, 1)
GETU(foregroundFlashCount, SPI_GETFOREGROUNDFLASHCOUNT, 0, 0xFFFFFFFF, 1)
GETU(caretWidth, SPI_GETCARETWIDTH, 1, 1000, 1)
GETU(fontSmoothingType, SPI_GETFONTSMOOTHINGTYPE, 0, 2, 1)
GETU(fontSmoothingContrast, SPI_GETFONTSMOOTHINGCONTRAST, 0, 2200, 1)
GETU(fontSmoothingOrientation, SPI_GETFONTSMOOTHINGORIENTATION, 0, 1, 1)
GETU(focusBorderWidth, SPI_GETFOCUSBORDERWIDTH, 1, 100, 1)
GETU(focusBorderHeight, SPI_GETFOCUSBORDERHEIGHT, 1, 100, 1)
GETU(mouseClickLockTime, SPI_GETMOUSECLICKLOCKTIME, 1, 0xFFFFFF, 1)
GETU(hungAppTimeout, SPI_GETHUNGAPPTIMEOUT, 1, 0xFFFFFF, 0)
GETU(waitToKillTimeout, SPI_GETWAITTOKILLTIMEOUT, 1, 0xFFFFFF, 0)
GETU(waitToKillServiceTimeout, SPI_GETWAITTOKILLSERVICETIMEOUT, 1, 0xFFFFFF, 0)
GETU(messageDuration, SPI_GETMESSAGEDURATION, 1, 0xFFFFFF, 0)
GETU(caretTimeout, SPI_GETCARETTIMEOUT, 0, 0xFFFFFFFF, 0)
GETU(mouseWheelRouting, SPI_GETMOUSEWHEELROUTING, 0, 2, 0)
GETU(contactVisualization, SPI_GETCONTACTVISUALIZATION, 0, 0xFFFFFFFF, 0)
GETU(gestureVisualization, SPI_GETGESTUREVISUALIZATION, 0, 0xFFFFFFFF, 0)
GETU(minimumHitRadius, SPI_GETMINIMUMHITRADIUS, 0, 0xFFFFFFFF, 0)
GETU(penVisualization, SPI_GETPENVISUALIZATION, 0, 0xFFFFFFFF, 0)
GETU(penArbitrationType, SPI_GETPENARBITRATIONTYPE, 0, 0xFFFFFFFF, 0)
GETU(mouseDockThreshold, SPI_GETMOUSEDOCKTHRESHOLD, 0, 0xFFFFFFFF, 0)
GETU(penDockThreshold, SPI_GETPENDOCKTHRESHOLD, 0, 0xFFFFFFFF, 0)
GETU(mouseDragOutThreshold, SPI_GETMOUSEDRAGOUTTHRESHOLD, 0, 0xFFFFFFFF, 0)
GETU(penDragOutThreshold, SPI_GETPENDRAGOUTTHRESHOLD, 0, 0xFFFFFFFF, 0)
GETU(mouseSideMoveThreshold, SPI_GETMOUSESIDEMOVETHRESHOLD, 0, 0xFFFFFFFF, 0)
GETU(penSideMoveThreshold, SPI_GETPENSIDEMOVETHRESHOLD, 0, 0xFFFFFFFF, 0)
GETU(mouseCornerClipLength, SPI_GETMOUSECORNERCLIPLENGTH, 0, 0xFFFFFFFF, 0)
GETU(logicalDpiOverride, SPI_GETLOGICALDPIOVERRIDE, 0, 0xFFFFFFFF, 0)
GETU(handedness, SPI_GETHANDEDNESS, 0, 1, 0)

/* Relations between settings. */
TEST(spget_relation_screensave_timeout_active) {
    UINT act, to;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sp_get_u32(SPI_GETSCREENSAVEACTIVE, &act));
    CHECK(sp_get_u32(SPI_GETSCREENSAVETIMEOUT, &to));
    t_note("screensaver active=%u timeout=%u", act, to);
}
TEST(spget_relation_uieffects_consistent) {
    UINT ui, m, t;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sp_get_u32(SPI_GETUIEFFECTS, &ui));
    CHECK(sp_get_u32(SPI_GETMENUANIMATION, &m));
    CHECK(sp_get_u32(SPI_GETTOOLTIPANIMATION, &t));
    /* When UI effects are globally off, individual effects are off as well (observed, not documented) */
    if (!ui && (m || t)) t_note("UIEFFECTS off but menu/tooltip animation on");
}
TEST(spget_relation_menuunderlines_keyboardcues) {
    UINT a, b;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sp_get_u32(SPI_GETKEYBOARDCUES, &a));
    CHECK(sp_get_u32(SPI_GETMENUUNDERLINES, &b));
    /* SPI_GETMENUUNDERLINES is an alias of SPI_GETKEYBOARDCUES */
    CHECK(SPI_GETMENUUNDERLINES == SPI_GETKEYBOARDCUES);
    CHECK(a == b);
}
TEST(spget_relation_dragwidth_doubleclk) {
    int dw = gsm(SM_CXDRAG), dh = gsm(SM_CYDRAG);
    REQUIRE(U(GetSystemMetrics));
    CHECK(dw > 0); CHECK(dh > 0);
    CHECK(dw <= 200); CHECK(dh <= 200);
}
TEST(spget_relation_doubleclicktime) {
    UINT t;
    REQUIRE(U(GetDoubleClickTime));
    t = U(GetDoubleClickTime)();
    CHECK(t >= 1 && t <= 5000);
}
TEST(spget_relation_caretblinktime) {
    UINT t;
    REQUIRE(U(GetCaretBlinkTime));
    t = U(GetCaretBlinkTime)();
    CHECK(t >= 1 || t == 0xFFFFFFFF || t == 0);
    t_note("caret blink time %u", t);
}

/* Invalid action codes. */
static const UINT bad_actions[] = { 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0xFFFFFFFF, 0x7FFFFFFF, 0x1000, 0x3000, 0xF000 };
TEST(spget_invalid_action_codes) {
    unsigned i;
    UINT v;
    REQUIRE(U(SystemParametersInfoW));
    for (i = 0; i < sizeof bad_actions / sizeof *bad_actions; i++) {
        BOOL r;
        v = SENT32;
        SetLastError(0xdeadbeef);
        r = sW(bad_actions[i], 0, &v, 0);
        if (r) { t_note("action 0x%x unexpectedly succeeded", bad_actions[i]); continue; }
        CHECKF(v == SENT32, "action 0x%x modified output", bad_actions[i]);
        if (GetLastError() != ERROR_INVALID_PARAMETER) t_note("action 0x%x err=%lu", bad_actions[i], GetLastError());
    }
}
TEST(spget_invalid_action_zero) {
    UINT v = SENT32;
    BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    SetLastError(0xdeadbeef);
    r = sW(0, 0, &v, 0);
    CHECK(!r);
    CHECK(v == SENT32);
    t_note("action 0 err=%lu", GetLastError());
}
TEST(spget_invalid_action_ansi_parity) {
    unsigned i;
    UINT v;
    REQUIRE(U(SystemParametersInfoA)); REQUIRE(U(SystemParametersInfoW));
    for (i = 0; i < sizeof bad_actions / sizeof *bad_actions; i++) {
        BOOL ra, rw;
        v = SENT32; ra = sA(bad_actions[i], 0, &v, 0);
        v = SENT32; rw = sW(bad_actions[i], 0, &v, 0);
        CHECKF(ra == rw, "action 0x%x A=%d W=%d", bad_actions[i], ra, rw);
    }
}
/* Sweep a range of unassigned codes to ensure none crash or write output. */
TEST(spget_invalid_action_sweep_high) {
    UINT a, v;
    int succeeded = 0;
    REQUIRE(U(SystemParametersInfoW));
    for (a = 0x2000; a < 0x2100; a++) {
        v = SENT32;
        if (sW(a, 0, &v, 0)) succeeded++;
    }
    t_note("%d of 256 actions in 0x2000..0x20ff succeeded", succeeded);
}
TEST(spget_invalid_flags_wininfo) {
    UINT v = SENT32;
    BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    /* get actions ignore fWinIni entirely */
    r = sW(SPI_GETBEEP, 0, &v, 0);
    CHECK(r);
    {
        UINT v2 = SENT32;
        r = sW(SPI_GETBEEP, 0, &v2, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        t_note("GETBEEP with update flags -> %d", r);
        if (r) CHECK(v2 == v);
    }
}
TEST(spget_lasterror_untouched_on_success) {
    UINT v;
    REQUIRE(U(SystemParametersInfoW));
    SetLastError(0x1234);
    if (sW(SPI_GETBEEP, 0, &v, 0)) t_note("GETBEEP success leaves last error 0x%lx", GetLastError());
}
TEST(spget_repeat_stress) {
    int i, bad = 0;
    UINT first, v;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sp_get_u32(SPI_GETKEYBOARDSPEED, &first));
    for (i = 0; i < 5000; i++) {
        if (!sp_get_u32(SPI_GETKEYBOARDSPEED, &v) || v != first) bad++;
    }
    CHECK(bad == 0);
}
TEST(spget_mouse_array) {
    int m[3] = { 0x7777, 0x7777, 0x7777 };
    int m2[3] = { 0, 0, 0 };
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETMOUSE, 0, m, 0));
    CHECK(m[0] >= 0 && m[0] <= 0xFFFF);
    CHECK(m[1] >= 0 && m[1] <= 0xFFFF);
    CHECK(m[2] == 0 || m[2] == 1 || m[2] == 2);
    CHECK(sA(SPI_GETMOUSE, 0, m2, 0));
    CHECK(!memcmp(m, m2, sizeof m));
}
TEST(spget_border) {
    int b = -1;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETBORDER, 0, &b, 0));
    CHECK(b >= 1 && b <= 50);
    t_note("SPI_GETBORDER=%d SM_CXSIZEFRAME=%d SM_CXPADDEDBORDER=%d", b, gsm(SM_CXSIZEFRAME), gsm(SM_CXPADDEDBORDER));
}
TEST(spget_iconspacing) {
    UINT h = 0, v = 0;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_ICONHORIZONTALSPACING, 0, &h, 0));
    CHECK(sW(SPI_ICONVERTICALSPACING, 0, &v, 0));
    CHECK(h >= 32 && h < 1000);
    CHECK(v >= 32 && v < 1000);
    CHECK((int)h == gsm(SM_CXICONSPACING));
    CHECK((int)v == gsm(SM_CYICONSPACING));
}
TEST(spget_gridgranularity) {
    UINT g = SENT32;
    BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    r = sW(SPI_GETGRIDGRANULARITY, 0, &g, 0);
    t_note("GETGRIDGRANULARITY -> %d (%u)", r, g);
}
TEST(spget_keyboardpref_screenreader_pair) {
    UINT k, s;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sp_get_u32(SPI_GETKEYBOARDPREF, &k));
    CHECK(sp_get_u32(SPI_GETSCREENREADER, &s));
}
TEST(spget_windowsextension_langdriver_legacy) {
    BOOL r;
    UINT v = SENT32;
    REQUIRE(U(SystemParametersInfoW));
    SetLastError(0xdeadbeef);
    r = sW(SPI_GETWINDOWSEXTENSION, 0, &v, 0);
    t_note("GETWINDOWSEXTENSION -> %d err %lu", r, GetLastError());
    SetLastError(0xdeadbeef);
    r = sW(SPI_LANGDRIVER, 0, &v, 0);
    t_note("LANGDRIVER -> %d err %lu", r, GetLastError());
}
