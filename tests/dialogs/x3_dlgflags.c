/* dialogs: DS_FIXEDSYS / DS_USEPIXELS / DS_LOCALEDIT / DS_3DLOOK template flags (creation and
 * observation; behaviour that the SDK does not document is logged, not asserted). */
#include "dlgutil.h"

#define INST GetModuleHandleA(NULL)
#ifndef DS_USEPIXELS
#define DS_USEPIXELS 0x8000
#endif

static HWND x3_make(DWORD style, int withfont, HWND *child) {
    DT d;
    dt_hdr(&d, WS_POPUP | WS_CAPTION | DS_MODALFRAME | style, 0, 1, 10, 20, 100, 60, NULL, NULL, L"x3",
           8, withfont ? L"MS Shell Dlg" : NULL);
    if (!withfont) { /* dt_hdr only writes font info when a font name is given */ }
    dt_item(&d, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 10, 10, 40, 14, 7, C_BUTTON, L"b");
    ds_reset();
    HWND h = CreateDialogIndirectParamW(INST, (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
    if (h && child) *child = GetDlgItem(h, 7);
    return h;
}

TEST(dialogs_x3_flag_values) {
    EQ(DS_ABSALIGN, 0x1); EQ(DS_SYSMODAL, 0x2); EQ(DS_LOCALEDIT, 0x20); EQ(DS_SETFONT, 0x40);
    EQ(DS_MODALFRAME, 0x80); EQ(DS_NOIDLEMSG, 0x100); EQ(DS_SETFOREGROUND, 0x200); EQ(DS_3DLOOK, 0x4);
    EQ(DS_FIXEDSYS, 0x8); EQ(DS_NOFAILCREATE, 0x10); EQ(DS_CONTROL, 0x400); EQ(DS_CENTER, 0x800);
    EQ(DS_CENTERMOUSE, 0x1000); EQ(DS_CONTEXTHELP, 0x2000); EQ(DS_SHELLFONT, DS_SETFONT | DS_FIXEDSYS);
}

TEST(dialogs_x3_fixedsys_creates_and_scales_children) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow); REQUIRE(GetWindowRect);
    HWND c = NULL, c0 = NULL;
    HWND d = x3_make(DS_FIXEDSYS, 0, &c); REQUIRE(d); REQUIRE(c);
    RECT r, r0; GetWindowRect(c, &r);
    HWND d0 = x3_make(0, 0, &c0); REQUIRE(d0); REQUIRE(c0);
    GetWindowRect(c0, &r0);
    t_note("child size DS_FIXEDSYS %ldx%ld vs default %ldx%ld", r.right - r.left, r.bottom - r.top,
           r0.right - r0.left, r0.bottom - r0.top);
    CHECK(r.right > r.left && r.bottom > r.top);
    CHECK(IsWindow(d));
    DestroyWindow(d0); DestroyWindow(d);
}

TEST(dialogs_x3_usepixels_child_geometry_note) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow); REQUIRE(GetWindowRect); REQUIRE(ScreenToClient);
    HWND c = NULL;
    HWND d = x3_make(DS_USEPIXELS, 1, &c); REQUIRE(d); REQUIRE(c);
    RECT r; GetWindowRect(c, &r);
    POINT p = { r.left, r.top }; ScreenToClient(d, &p);
    t_note("DS_USEPIXELS child client origin (%ld,%ld) size %ldx%ld (template 10,10 40x14)", p.x, p.y,
           r.right - r.left, r.bottom - r.top);
    CHECK(IsWindow(d));
    DestroyWindow(d);
}

TEST(dialogs_x3_locale_edit_and_3dlook_styles_survive) {
    REQUIRE(CreateDialogIndirectParamW); REQUIRE(DestroyWindow); REQUIRE(GetWindowLongW);
    DWORD fl[] = { DS_LOCALEDIT, DS_3DLOOK, DS_CONTROL, DS_NOFAILCREATE };
    for (int i = 0; i < 4; i++) {
        HWND d = x3_make(fl[i], 1, NULL);
        CHECKF(d != NULL, "flag 0x%lx", fl[i]);
        if (!d) continue;
        DWORD st = GetWindowLongW(d, GWL_STYLE);
        t_note("template flag 0x%lx -> window style 0x%08lx (flag kept: %d)", fl[i], st, (st & fl[i]) == fl[i]);
        DestroyWindow(d);
    }
}
