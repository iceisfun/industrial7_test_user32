/* SystemParametersInfo setters: change in-session value (fWinIni = 0), verify, restore exactly. */
#include "sp.h"

/* BOOL via uiParam */
#define RT_BOOL_PARAM(tn, getspi, setspi) \
TEST(spset_boolparam_##tn) { UINT o; REQUIRE(U(SystemParametersInfoW)); \
    if (!sp_get_u32(getspi, &o)) { t_note(#tn ": get failed"); return; } \
    roundtrip_u32(getspi, setspi, TRUE, !o, #tn, 1); \
    roundtrip_u32(getspi, setspi, TRUE, o, #tn " same", 1); }
/* BOOL via pvParam value */
#define RT_BOOL_PV(tn, getspi, setspi, must) \
TEST(spset_boolpv_##tn) { UINT o; REQUIRE(U(SystemParametersInfoW)); \
    if (!sp_get_u32(getspi, &o)) { t_note(#tn ": get failed"); return; } \
    roundtrip_u32(getspi, setspi, FALSE, !o, #tn, must); \
    roundtrip_u32(getspi, setspi, FALSE, o, #tn " same", must); }

RT_BOOL_PARAM(beep, SPI_GETBEEP, SPI_SETBEEP)
RT_BOOL_PARAM(iconTitleWrap, SPI_GETICONTITLEWRAP, SPI_SETICONTITLEWRAP)
RT_BOOL_PARAM(menuDropAlignment, SPI_GETMENUDROPALIGNMENT, SPI_SETMENUDROPALIGNMENT)
RT_BOOL_PARAM(snapToDefButton, SPI_GETSNAPTODEFBUTTON, SPI_SETSNAPTODEFBUTTON)
RT_BOOL_PARAM(showSounds, SPI_GETSHOWSOUNDS, SPI_SETSHOWSOUNDS)
RT_BOOL_PARAM(keyboardPref, SPI_GETKEYBOARDPREF, SPI_SETKEYBOARDPREF)
RT_BOOL_PARAM(dragFullWindows, SPI_GETDRAGFULLWINDOWS, SPI_SETDRAGFULLWINDOWS)

RT_BOOL_PV(activeWindowTracking, SPI_GETACTIVEWINDOWTRACKING, SPI_SETACTIVEWINDOWTRACKING, 1)
RT_BOOL_PV(menuAnimation, SPI_GETMENUANIMATION, SPI_SETMENUANIMATION, 1)
RT_BOOL_PV(comboBoxAnimation, SPI_GETCOMBOBOXANIMATION, SPI_SETCOMBOBOXANIMATION, 1)
RT_BOOL_PV(listBoxSmoothScrolling, SPI_GETLISTBOXSMOOTHSCROLLING, SPI_SETLISTBOXSMOOTHSCROLLING, 1)
RT_BOOL_PV(gradientCaptions, SPI_GETGRADIENTCAPTIONS, SPI_SETGRADIENTCAPTIONS, 1)
RT_BOOL_PV(keyboardCues, SPI_GETKEYBOARDCUES, SPI_SETKEYBOARDCUES, 1)
RT_BOOL_PV(activeWndTrkZorder, SPI_GETACTIVEWNDTRKZORDER, SPI_SETACTIVEWNDTRKZORDER, 1)
RT_BOOL_PV(hotTracking, SPI_GETHOTTRACKING, SPI_SETHOTTRACKING, 1)
RT_BOOL_PV(menuFade, SPI_GETMENUFADE, SPI_SETMENUFADE, 1)
RT_BOOL_PV(selectionFade, SPI_GETSELECTIONFADE, SPI_SETSELECTIONFADE, 1)
RT_BOOL_PV(tooltipAnimation, SPI_GETTOOLTIPANIMATION, SPI_SETTOOLTIPANIMATION, 1)
RT_BOOL_PV(tooltipFade, SPI_GETTOOLTIPFADE, SPI_SETTOOLTIPFADE, 1)
RT_BOOL_PV(cursorShadow, SPI_GETCURSORSHADOW, SPI_SETCURSORSHADOW, 1)
RT_BOOL_PV(flatMenu, SPI_GETFLATMENU, SPI_SETFLATMENU, 1)
RT_BOOL_PV(dropShadow, SPI_GETDROPSHADOW, SPI_SETDROPSHADOW, 1)
RT_BOOL_PARAM(blockSendInputResets, SPI_GETBLOCKSENDINPUTRESETS, SPI_SETBLOCKSENDINPUTRESETS)
RT_BOOL_PV(clientAreaAnimation, SPI_GETCLIENTAREAANIMATION, SPI_SETCLIENTAREAANIMATION, 0)
RT_BOOL_PV(mouseSonar, SPI_GETMOUSESONAR, SPI_SETMOUSESONAR, 1)
RT_BOOL_PV(mouseVanish, SPI_GETMOUSEVANISH, SPI_SETMOUSEVANISH, 1)
RT_BOOL_PV(disableOverlappedContent, SPI_GETDISABLEOVERLAPPEDCONTENT, SPI_SETDISABLEOVERLAPPEDCONTENT, 0)
RT_BOOL_PV(caretBrowsing, SPI_GETCARETBROWSING, SPI_SETCARETBROWSING, 0)

/* Font smoothing */
TEST(spset_fontSmoothing) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETFONTSMOOTHING, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETFONTSMOOTHING, SPI_SETFONTSMOOTHING, TRUE, !o, "fontsmoothing", 1);
    roundtrip_u32(SPI_GETFONTSMOOTHING, SPI_SETFONTSMOOTHING, TRUE, o, "fontsmoothing same", 1); }
TEST(spset_fontSmoothingContrast) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETFONTSMOOTHINGCONTRAST, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETFONTSMOOTHINGCONTRAST, SPI_SETFONTSMOOTHINGCONTRAST, FALSE, 1200, "contrast1200", 1);
    roundtrip_u32(SPI_GETFONTSMOOTHINGCONTRAST, SPI_SETFONTSMOOTHINGCONTRAST, FALSE, 2200, "contrast2200", 1);
    roundtrip_u32(SPI_GETFONTSMOOTHINGCONTRAST, SPI_SETFONTSMOOTHINGCONTRAST, FALSE, o, "contrast same", 1); }
TEST(spset_fontSmoothingContrast_range) { UINT o, v; BOOL r; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETFONTSMOOTHINGCONTRAST, &o)) { CHECK(0); return; }
    r = sW(SPI_SETFONTSMOOTHINGCONTRAST, 0, (PVOID)(ULONG_PTR)999, 0);
    t_note("contrast 999 -> %d err=%lu", r, GetLastError());
    r = sW(SPI_SETFONTSMOOTHINGCONTRAST, 0, (PVOID)(ULONG_PTR)2201, 0);
    t_note("contrast 2201 -> %d err=%lu", r, GetLastError());
    sW(SPI_SETFONTSMOOTHINGCONTRAST, 0, (PVOID)(ULONG_PTR)o, 0);
    CHECK(sp_get_u32(SPI_GETFONTSMOOTHINGCONTRAST, &v)); CHECK(v == o); }
TEST(spset_fontSmoothingType) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETFONTSMOOTHINGTYPE, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETFONTSMOOTHINGTYPE, SPI_SETFONTSMOOTHINGTYPE, FALSE, FE_FONTSMOOTHINGSTANDARD, "fstype std", 1);
    roundtrip_u32(SPI_GETFONTSMOOTHINGTYPE, SPI_SETFONTSMOOTHINGTYPE, FALSE, FE_FONTSMOOTHINGCLEARTYPE, "fstype ct", 1);
    roundtrip_u32(SPI_GETFONTSMOOTHINGTYPE, SPI_SETFONTSMOOTHINGTYPE, FALSE, o, "fstype same", 1); }
TEST(spset_fontSmoothingOrientation) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETFONTSMOOTHINGORIENTATION, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETFONTSMOOTHINGORIENTATION, SPI_SETFONTSMOOTHINGORIENTATION, FALSE, !o, "fsorient", 1);
    roundtrip_u32(SPI_GETFONTSMOOTHINGORIENTATION, SPI_SETFONTSMOOTHINGORIENTATION, FALSE, o, "fsorient same", 1); }

/* Numeric settings */
TEST(spset_keyboardSpeed) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETKEYBOARDSPEED, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETKEYBOARDSPEED, SPI_SETKEYBOARDSPEED, TRUE, 0, "kbspeed0", 1);
    roundtrip_u32(SPI_GETKEYBOARDSPEED, SPI_SETKEYBOARDSPEED, TRUE, 31, "kbspeed31", 1);
    roundtrip_u32(SPI_GETKEYBOARDSPEED, SPI_SETKEYBOARDSPEED, TRUE, 15, "kbspeed15", 1);
    roundtrip_u32(SPI_GETKEYBOARDSPEED, SPI_SETKEYBOARDSPEED, TRUE, o, "kbspeed same", 1); }
TEST(spset_keyboardSpeed_invalid) { UINT o, v; BOOL r; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETKEYBOARDSPEED, &o)) { CHECK(0); return; }
    SetLastError(0xdeadbeef); r = sW(SPI_SETKEYBOARDSPEED, 32, NULL, 0);
    t_note("kbspeed 32 -> %d err=%lu", r, GetLastError());
    SetLastError(0xdeadbeef); r = sW(SPI_SETKEYBOARDSPEED, 0xFFFFFFFF, NULL, 0);
    t_note("kbspeed -1 -> %d err=%lu", r, GetLastError());
    sW(SPI_SETKEYBOARDSPEED, o, NULL, 0);
    CHECK(sp_get_u32(SPI_GETKEYBOARDSPEED, &v)); CHECK(v == o); }
TEST(spset_keyboardDelay) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETKEYBOARDDELAY, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETKEYBOARDDELAY, SPI_SETKEYBOARDDELAY, TRUE, 0, "kbdelay0", 1);
    roundtrip_u32(SPI_GETKEYBOARDDELAY, SPI_SETKEYBOARDDELAY, TRUE, 3, "kbdelay3", 1);
    roundtrip_u32(SPI_GETKEYBOARDDELAY, SPI_SETKEYBOARDDELAY, TRUE, o, "kbdelay same", 1); }
TEST(spset_keyboardDelay_invalid) { UINT o, v; BOOL r; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETKEYBOARDDELAY, &o)) { CHECK(0); return; }
    SetLastError(0xdeadbeef); r = sW(SPI_SETKEYBOARDDELAY, 4, NULL, 0);
    t_note("kbdelay 4 -> %d err=%lu", r, GetLastError());
    sW(SPI_SETKEYBOARDDELAY, o, NULL, 0);
    CHECK(sp_get_u32(SPI_GETKEYBOARDDELAY, &v)); CHECK(v == o); }
TEST(spset_mouseSpeed) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETMOUSESPEED, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETMOUSESPEED, SPI_SETMOUSESPEED, FALSE, o, "mousespeed same", 1);
    roundtrip_u32(SPI_GETMOUSESPEED, SPI_SETMOUSESPEED, FALSE, o == 20 ? 19 : o + 1, "mousespeed +1", 1); }
TEST(spset_mouseSpeed_invalid) { UINT o, v; BOOL r; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETMOUSESPEED, &o)) { CHECK(0); return; }
    SetLastError(0xdeadbeef); r = sW(SPI_SETMOUSESPEED, 0, (PVOID)0, 0);
    t_note("mousespeed 0 -> %d err=%lu", r, GetLastError());
    SetLastError(0xdeadbeef); r = sW(SPI_SETMOUSESPEED, 0, (PVOID)21, 0);
    t_note("mousespeed 21 -> %d err=%lu", r, GetLastError());
    sW(SPI_SETMOUSESPEED, 0, (PVOID)(ULONG_PTR)o, 0);
    CHECK(sp_get_u32(SPI_GETMOUSESPEED, &v)); CHECK(v == o); }
TEST(spset_wheelScrollLines) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETWHEELSCROLLLINES, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETWHEELSCROLLLINES, SPI_SETWHEELSCROLLLINES, TRUE, 7, "wheellines7", 1);
    roundtrip_u32(SPI_GETWHEELSCROLLLINES, SPI_SETWHEELSCROLLLINES, TRUE, 0, "wheellines0", 1);
    roundtrip_u32(SPI_GETWHEELSCROLLLINES, SPI_SETWHEELSCROLLLINES, TRUE, 0xFFFFFFFF, "wheellines-1", 0);
    roundtrip_u32(SPI_GETWHEELSCROLLLINES, SPI_SETWHEELSCROLLLINES, TRUE, o, "wheellines same", 1); }
TEST(spset_wheelScrollChars) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETWHEELSCROLLCHARS, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETWHEELSCROLLCHARS, SPI_SETWHEELSCROLLCHARS, TRUE, 5, "wheelchars5", 1);
    roundtrip_u32(SPI_GETWHEELSCROLLCHARS, SPI_SETWHEELSCROLLCHARS, TRUE, o, "wheelchars same", 1); }
TEST(spset_menuShowDelay) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETMENUSHOWDELAY, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETMENUSHOWDELAY, SPI_SETMENUSHOWDELAY, TRUE, 123, "menudelay", 1);
    roundtrip_u32(SPI_GETMENUSHOWDELAY, SPI_SETMENUSHOWDELAY, TRUE, 0, "menudelay0", 1);
    roundtrip_u32(SPI_GETMENUSHOWDELAY, SPI_SETMENUSHOWDELAY, TRUE, o, "menudelay same", 1); }
TEST(spset_mouseHover) { UINT w, h, t; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETMOUSEHOVERWIDTH, &w) || !sp_get_u32(SPI_GETMOUSEHOVERHEIGHT, &h) || !sp_get_u32(SPI_GETMOUSEHOVERTIME, &t)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETMOUSEHOVERWIDTH, SPI_SETMOUSEHOVERWIDTH, TRUE, 9, "hoverw", 1);
    roundtrip_u32(SPI_GETMOUSEHOVERHEIGHT, SPI_SETMOUSEHOVERHEIGHT, TRUE, 11, "hoverh", 1);
    roundtrip_u32(SPI_GETMOUSEHOVERTIME, SPI_SETMOUSEHOVERTIME, TRUE, 777, "hovert", 1);
    roundtrip_u32(SPI_GETMOUSEHOVERWIDTH, SPI_SETMOUSEHOVERWIDTH, TRUE, w, "hoverw same", 1);
    roundtrip_u32(SPI_GETMOUSEHOVERHEIGHT, SPI_SETMOUSEHOVERHEIGHT, TRUE, h, "hoverh same", 1);
    roundtrip_u32(SPI_GETMOUSEHOVERTIME, SPI_SETMOUSEHOVERTIME, TRUE, t, "hovert same", 1); }
TEST(spset_caretWidth) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETCARETWIDTH, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETCARETWIDTH, SPI_SETCARETWIDTH, FALSE, 3, "caret3", 1);
    roundtrip_u32(SPI_GETCARETWIDTH, SPI_SETCARETWIDTH, FALSE, o, "caret same", 1); }
TEST(spset_focusBorder) { UINT w, h; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETFOCUSBORDERWIDTH, &w) || !sp_get_u32(SPI_GETFOCUSBORDERHEIGHT, &h)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETFOCUSBORDERWIDTH, SPI_SETFOCUSBORDERWIDTH, FALSE, w + 1, "focusw", 1);
    roundtrip_u32(SPI_GETFOCUSBORDERHEIGHT, SPI_SETFOCUSBORDERHEIGHT, FALSE, h + 1, "focush", 1);
    CHECK((UINT)gsm(SM_CXFOCUSBORDER) == w);
    CHECK((UINT)gsm(SM_CYFOCUSBORDER) == h); }
TEST(spset_foregroundFlashCount) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETFOREGROUNDFLASHCOUNT, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETFOREGROUNDFLASHCOUNT, SPI_SETFOREGROUNDFLASHCOUNT, FALSE, 5, "flash5", 1);
    roundtrip_u32(SPI_GETFOREGROUNDFLASHCOUNT, SPI_SETFOREGROUNDFLASHCOUNT, FALSE, o, "flash same", 1); }
TEST(spset_foregroundLockTimeout) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETFOREGROUNDLOCKTIMEOUT, &o)) { CHECK(0); return; }
    /* needs the caller to be allowed to set foreground; failure is not a defect */
    roundtrip_u32(SPI_GETFOREGROUNDLOCKTIMEOUT, SPI_SETFOREGROUNDLOCKTIMEOUT, FALSE, o, "fglock same", 0); }
TEST(spset_activeWndTrkTimeout) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETACTIVEWNDTRKTIMEOUT, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETACTIVEWNDTRKTIMEOUT, SPI_SETACTIVEWNDTRKTIMEOUT, FALSE, 250, "trk250", 1);
    roundtrip_u32(SPI_GETACTIVEWNDTRKTIMEOUT, SPI_SETACTIVEWNDTRKTIMEOUT, FALSE, o, "trk same", 1); }
TEST(spset_screensaveTimeout) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETSCREENSAVETIMEOUT, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETSCREENSAVETIMEOUT, SPI_SETSCREENSAVETIMEOUT, TRUE, o + 60, "sstimeout", 1);
    roundtrip_u32(SPI_GETSCREENSAVETIMEOUT, SPI_SETSCREENSAVETIMEOUT, TRUE, o, "sstimeout same", 1); }
TEST(spset_screensaveActive) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETSCREENSAVEACTIVE, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETSCREENSAVEACTIVE, SPI_SETSCREENSAVEACTIVE, TRUE, o, "ssactive same", 1);
    /* toggling: the transition to active arms the idle timer, so only flip active -> inactive */
    if (o) roundtrip_u32(SPI_GETSCREENSAVEACTIVE, SPI_SETSCREENSAVEACTIVE, TRUE, 0, "ssactive off", 1); }
TEST(spset_mouseClickLockTime) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETMOUSECLICKLOCKTIME, &o)) { CHECK(0); return; }
    roundtrip_u32(SPI_GETMOUSECLICKLOCKTIME, SPI_SETMOUSECLICKLOCKTIME, TRUE, 2000, "cllocktime", 1);
    roundtrip_u32(SPI_GETMOUSECLICKLOCKTIME, SPI_SETMOUSECLICKLOCKTIME, TRUE, o, "cllocktime same", 1); }
TEST(spset_messageDuration) { UINT o; REQUIRE(U(SystemParametersInfoW));
    if (!sp_get_u32(SPI_GETMESSAGEDURATION, &o)) { t_note("messageduration unsupported"); return; }
    roundtrip_u32(SPI_GETMESSAGEDURATION, SPI_SETMESSAGEDURATION, FALSE, o, "msgdur same", 0); }

/* Double-click time */
TEST(spset_doubleClickTime) {
    UINT o; REQUIRE(U(GetDoubleClickTime)); REQUIRE(U(SetDoubleClickTime));
    o = U(GetDoubleClickTime)();
    CHECK(sW(SPI_SETDOUBLECLICKTIME, 300, NULL, 0));
    CHECK(U(GetDoubleClickTime)() == 300);
    CHECK(sW(SPI_SETDOUBLECLICKTIME, 1000, NULL, 0));
    CHECK(U(GetDoubleClickTime)() == 1000);
    CHECK(U(SetDoubleClickTime)(450));
    CHECK(U(GetDoubleClickTime)() == 450);
    CHECK(sW(SPI_SETDOUBLECLICKTIME, 0, NULL, 0));
    CHECK(U(GetDoubleClickTime)() == 500);
    CHECK(U(SetDoubleClickTime)(o));
    CHECK(U(GetDoubleClickTime)() == o);
}
TEST(spset_doubleClickTime_max) {
    UINT o; REQUIRE(U(GetDoubleClickTime)); REQUIRE(U(SetDoubleClickTime));
    o = U(GetDoubleClickTime)();
    CHECK(U(SetDoubleClickTime)(5000));
    CHECK(U(GetDoubleClickTime)() == 5000);
    U(SetDoubleClickTime)(6000);
    t_note("after SetDoubleClickTime(6000): %u", U(GetDoubleClickTime)());
    U(SetDoubleClickTime)(1);
    t_note("after SetDoubleClickTime(1): %u", U(GetDoubleClickTime)());
    CHECK(U(SetDoubleClickTime)(o));
    CHECK(U(GetDoubleClickTime)() == o);
}
TEST(spset_doubleClickSize) {
    int ow = gsm(SM_CXDOUBLECLK), oh = gsm(SM_CYDOUBLECLK);
    REQUIRE(U(SystemParametersInfoW));
    CHECK(ow > 0 && oh > 0);
    CHECK(sW(SPI_SETDOUBLECLKWIDTH, ow + 3, NULL, 0));
    CHECK(gsm(SM_CXDOUBLECLK) == ow + 3);
    CHECK(sW(SPI_SETDOUBLECLKWIDTH, ow, NULL, 0));
    CHECK(sW(SPI_SETDOUBLECLKHEIGHT, oh + 5, NULL, 0));
    CHECK(gsm(SM_CYDOUBLECLK) == oh + 5);
    CHECK(sW(SPI_SETDOUBLECLKHEIGHT, oh, NULL, 0));
    CHECK(gsm(SM_CXDOUBLECLK) == ow && gsm(SM_CYDOUBLECLK) == oh);
}
TEST(spset_dragSize) {
    int ow = gsm(SM_CXDRAG), oh = gsm(SM_CYDRAG);
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_SETDRAGWIDTH, ow + 2, NULL, 0));
    CHECK(gsm(SM_CXDRAG) == ow + 2);
    CHECK(sW(SPI_SETDRAGWIDTH, ow, NULL, 0));
    CHECK(sW(SPI_SETDRAGHEIGHT, oh + 2, NULL, 0));
    CHECK(gsm(SM_CYDRAG) == oh + 2);
    CHECK(sW(SPI_SETDRAGHEIGHT, oh, NULL, 0));
    CHECK(gsm(SM_CXDRAG) == ow && gsm(SM_CYDRAG) == oh);
}
TEST(spset_iconSpacing) {
    UINT oh = 0, ov = 0, v;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_ICONHORIZONTALSPACING, 0, &oh, 0));
    CHECK(sW(SPI_ICONVERTICALSPACING, 0, &ov, 0));
    CHECK(sW(SPI_ICONHORIZONTALSPACING, oh + 8, NULL, 0));
    CHECK(gsm(SM_CXICONSPACING) == (int)oh + 8);
    CHECK(sW(SPI_ICONHORIZONTALSPACING, oh, NULL, 0));
    CHECK(sW(SPI_ICONVERTICALSPACING, ov + 8, NULL, 0));
    CHECK(gsm(SM_CYICONSPACING) == (int)ov + 8);
    CHECK(sW(SPI_ICONVERTICALSPACING, ov, NULL, 0));
    v = 0; sW(SPI_ICONHORIZONTALSPACING, 0, &v, 0); CHECK(v == oh);
    v = 0; sW(SPI_ICONVERTICALSPACING, 0, &v, 0); CHECK(v == ov);
}
TEST(spset_iconSpacing_small) {
    UINT oh = 0, v = 0; BOOL r;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_ICONHORIZONTALSPACING, 0, &oh, 0));
    r = sW(SPI_ICONHORIZONTALSPACING, 10, NULL, 0);
    t_note("icon horizontal spacing 10 -> %d, metric %d", r, gsm(SM_CXICONSPACING));
    if (r) CHECK(gsm(SM_CXICONSPACING) >= 32);
    sW(SPI_ICONHORIZONTALSPACING, oh, NULL, 0);
    sW(SPI_ICONHORIZONTALSPACING, 0, &v, 0);
    CHECK(v == oh);
}

/* SPI_SETMOUSE / mouse trails */
TEST(spset_mouse_identity) {
    int o[3] = { 0 }, n[3] = { 0 };
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETMOUSE, 0, o, 0));
    CHECK(sW(SPI_SETMOUSE, 0, o, 0));
    CHECK(sW(SPI_GETMOUSE, 0, n, 0));
    CHECK(!memcmp(o, n, sizeof o));
}
TEST(spset_mouse_threshold_roundtrip) {
    int o[3] = { 0 }, m[3], n[3] = { 0 };
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETMOUSE, 0, o, 0));
    m[0] = o[0] ? 0 : 6; m[1] = o[1] ? 0 : 10; m[2] = o[2];
    CHECK(sW(SPI_SETMOUSE, 0, m, 0));
    CHECK(sW(SPI_GETMOUSE, 0, n, 0));
    CHECK(!memcmp(m, n, sizeof m));
    CHECK(sW(SPI_SETMOUSE, 0, o, 0));
    CHECK(sW(SPI_GETMOUSE, 0, n, 0));
    CHECK(!memcmp(o, n, sizeof o));
}
TEST(spset_mouseTrails_identity) {
    UINT o, v;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sp_get_u32(SPI_GETMOUSETRAILS, &o));
    CHECK(sW(SPI_SETMOUSETRAILS, o, NULL, 0));
    CHECK(sp_get_u32(SPI_GETMOUSETRAILS, &v));
    CHECK(v == o);
}

/* SPI_SETICONTITLELOGFONT identity */
TEST(spset_iconTitleLogFont_identity) {
    LOGFONTW o, n;
    REQUIRE(U(SystemParametersInfoW));
    CHECK(sW(SPI_GETICONTITLELOGFONT, sizeof o, &o, 0));
    CHECK(sW(SPI_SETICONTITLELOGFONT, sizeof o, &o, 0));
    memset(&n, 0, sizeof n);
    CHECK(sW(SPI_GETICONTITLELOGFONT, sizeof n, &n, 0));
    CHECK(o.lfHeight == n.lfHeight && o.lfWeight == n.lfWeight);
    CHECK(!wcscmp(o.lfFaceName, n.lfFaceName));
}

/* Setting NULL / invalid pointers where the action takes a pointer is not tested (kernel fault paths). */

/* Invalid values for SET actions that take structs: cbSize validation without changing state. */
TEST(spset_struct_badsize_rejected) {
    static const struct { UINT spi; UINT size; const char *n; } t[] = {
        { SPI_SETFILTERKEYS, sizeof(FILTERKEYS), "filterkeys" },
        { SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), "stickykeys" },
        { SPI_SETMOUSEKEYS, sizeof(MOUSEKEYS), "mousekeys" },
        { SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), "togglekeys" },
        { SPI_SETACCESSTIMEOUT, sizeof(ACCESSTIMEOUT), "accesstimeout" },
        { SPI_SETMINIMIZEDMETRICS, sizeof(MINIMIZEDMETRICS), "minimizedmetrics" },
        { SPI_SETANIMATION, sizeof(ANIMATIONINFO), "animation" },
        { SPI_SETICONMETRICS, sizeof(ICONMETRICSW), "iconmetrics" },
    };
    unsigned i;
    union { char raw[1024]; ULONGLONG a; } buf;
    REQUIRE(U(SystemParametersInfoW));
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        BOOL r;
        /* current contents are used as the payload so that an (unexpected) acceptance is an identity change */
        memset(&buf, 0, sizeof buf);
        *(UINT *)buf.raw = t[i].size;
        if (!sW(t[i].spi - 1, t[i].size, &buf, 0)) { t_note("%s: get failed, skipping", t[i].n); continue; }
        *(UINT *)buf.raw = t[i].size + 1;
        SetLastError(0xdeadbeef);
        r = sW(t[i].spi, t[i].size + 1, &buf, 0);
        CHECKF(!r, "%s: set with wrong size accepted", t[i].n);
        *(UINT *)buf.raw = 0;
        SetLastError(0xdeadbeef);
        r = sW(t[i].spi, 0, &buf, 0);
        CHECKF(!r, "%s: set with cbSize 0 accepted", t[i].n);
    }
}
