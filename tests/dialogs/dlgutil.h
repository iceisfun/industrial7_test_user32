/* Shared helpers for the dialogs area: in-memory template builder, a generic
 * dialog procedure and typed-call macros. Everything is static so each .c
 * file gets its own copy. */
#ifndef DLGUTIL_H
#define DLGUTIL_H
#include "t.h"

/* Route user32 calls through GetProcAddress (never link user32). */
#define SetTimer U(SetTimer)
#define KillTimer U(KillTimer)
#define EndDialog U(EndDialog)
#define GetDlgItem U(GetDlgItem)
#define GetDlgCtrlID U(GetDlgCtrlID)
#define GetDlgItemInt U(GetDlgItemInt)
#define SetDlgItemInt U(SetDlgItemInt)
#define GetDlgItemTextA U(GetDlgItemTextA)
#define GetDlgItemTextW U(GetDlgItemTextW)
#define SetDlgItemTextA U(SetDlgItemTextA)
#define SetDlgItemTextW U(SetDlgItemTextW)
#define CheckDlgButton U(CheckDlgButton)
#define CheckRadioButton U(CheckRadioButton)
#define IsDlgButtonChecked U(IsDlgButtonChecked)
#define GetNextDlgTabItem U(GetNextDlgTabItem)
#define GetNextDlgGroupItem U(GetNextDlgGroupItem)
#define SendDlgItemMessageA U(SendDlgItemMessageA)
#define SendDlgItemMessageW U(SendDlgItemMessageW)
#define MapDialogRect U(MapDialogRect)
#define GetDialogBaseUnits U(GetDialogBaseUnits)
#define DefDlgProcA U(DefDlgProcA)
#define DefDlgProcW U(DefDlgProcW)
#define CreateDialogIndirectParamA U(CreateDialogIndirectParamA)
#define CreateDialogIndirectParamW U(CreateDialogIndirectParamW)
#define CreateDialogParamA U(CreateDialogParamA)
#define CreateDialogParamW U(CreateDialogParamW)
#define DialogBoxIndirectParamA U(DialogBoxIndirectParamA)
#define DialogBoxIndirectParamW U(DialogBoxIndirectParamW)
#define DialogBoxParamA U(DialogBoxParamA)
#define DialogBoxParamW U(DialogBoxParamW)
#define IsDialogMessageA U(IsDialogMessageA)
#define IsDialogMessageW U(IsDialogMessageW)
#define DestroyWindow U(DestroyWindow)
#define IsWindow U(IsWindow)
#define IsWindowVisible U(IsWindowVisible)
#define IsWindowEnabled U(IsWindowEnabled)
#define EnableWindow U(EnableWindow)
#define ShowWindow U(ShowWindow)
#define GetWindowTextA U(GetWindowTextA)
#define GetWindowTextW U(GetWindowTextW)
#define SetWindowTextA U(SetWindowTextA)
#define GetWindowTextLengthA U(GetWindowTextLengthA)
#define GetClassNameA U(GetClassNameA)
#define GetClassNameW U(GetClassNameW)
#define GetParent U(GetParent)
#define GetWindow U(GetWindow)
#define GetFocus U(GetFocus)
#define SetFocus U(SetFocus)
#define GetActiveWindow U(GetActiveWindow)
#define SetActiveWindow U(SetActiveWindow)
#define GetWindowLongW U(GetWindowLongW)
#define SetWindowLongW U(SetWindowLongW)
#define GetWindowLongPtrW U(GetWindowLongPtrW)
#define SetWindowLongPtrW U(SetWindowLongPtrW)
#define GetWindowLongPtrA U(GetWindowLongPtrA)
#define SetWindowLongPtrA U(SetWindowLongPtrA)
#define GetWindowRect U(GetWindowRect)
#define GetClientRect U(GetClientRect)
#define SendMessageA U(SendMessageA)
#define SendMessageW U(SendMessageW)
#define PostMessageA U(PostMessageA)
#define PostMessageW U(PostMessageW)
#define PeekMessageA U(PeekMessageA)
#define PeekMessageW U(PeekMessageW)
#define GetMessageA U(GetMessageA)
#define TranslateMessage U(TranslateMessage)
#define DispatchMessageA U(DispatchMessageA)
#define DispatchMessageW U(DispatchMessageW)
#define PostQuitMessage U(PostQuitMessage)
#define GetCurrentThreadId_ GetCurrentThreadId
#define EnumThreadWindows U(EnumThreadWindows)
#define EnumChildWindows U(EnumChildWindows)
#define RegisterClassA U(RegisterClassA)
#define RegisterClassW U(RegisterClassW)
#define UnregisterClassA U(UnregisterClassA)
#define GetClassInfoA U(GetClassInfoA)
#define GetClassInfoW U(GetClassInfoW)
#define CreateWindowExA U(CreateWindowExA)
#define CreateWindowExW U(CreateWindowExW)
#define DefWindowProcA U(DefWindowProcA)
#define DefWindowProcW U(DefWindowProcW)
#define CallWindowProcA U(CallWindowProcA)
#define SetWindowPos U(SetWindowPos)
#define MoveWindow U(MoveWindow)
#define GetSystemMetrics U(GetSystemMetrics)
#define GetWindowThreadProcessId U(GetWindowThreadProcessId)
#define SetForegroundWindow U(SetForegroundWindow)
#define GetDC U(GetDC)
#define ReleaseDC U(ReleaseDC)
#define MessageBoxA U(MessageBoxA)
#define MessageBoxW U(MessageBoxW)
#define MessageBoxExA U(MessageBoxExA)
#define MessageBoxExW U(MessageBoxExW)
#define MessageBoxIndirectA U(MessageBoxIndirectA)
#define MessageBoxIndirectW U(MessageBoxIndirectW)
#define SetWindowsHookExW U(SetWindowsHookExW)
#define UnhookWindowsHookEx U(UnhookWindowsHookEx)
#define CallNextHookEx U(CallNextHookEx)
#define IsChild U(IsChild)
#define GetWindowTextLengthW U(GetWindowTextLengthW)
#define SetWindowTextW U(SetWindowTextW)
#define GetLastActivePopup U(GetLastActivePopup)
#define FindWindowA U(FindWindowA)
#define GetForegroundWindow U(GetForegroundWindow)
#define IsIconic U(IsIconic)
#define GetDesktopWindow U(GetDesktopWindow)
#define LoadIconW U(LoadIconW)
#define LoadCursorW U(LoadCursorW)
#define MsgWaitForMultipleObjects U(MsgWaitForMultipleObjects)
#define GetWindowInfo U(GetWindowInfo)
#define UpdateWindow U(UpdateWindow)
#define SetWindowLongPtrW_ U(SetWindowLongPtrW)
#define GetCursorPos U(GetCursorPos)
#define PostThreadMessageA U(PostThreadMessageA)
#define ClientToScreen U(ClientToScreen)
#define RedrawWindow U(RedrawWindow)
#define GetWindowThreadProcessId_ U(GetWindowThreadProcessId)
#define GetMonitorInfoW U(GetMonitorInfoW)
#define MonitorFromWindow U(MonitorFromWindow)
#define GetClassInfoExW U(GetClassInfoExW)
#define GetClassInfoExA U(GetClassInfoExA)
#define IsWindowUnicode U(IsWindowUnicode)
#define GetWindowContextHelpId U(GetWindowContextHelpId)
#define SetWindowContextHelpId U(SetWindowContextHelpId)
#define GetSysColorBrush U(GetSysColorBrush)
#define GetSysColor U(GetSysColor)
#define SystemParametersInfoW U(SystemParametersInfoW)
#define GetCapture U(GetCapture)
#define GetDpiForSystem U(GetDpiForSystem)
#define GetWindowRgn U(GetWindowRgn)
#define ScreenToClient U(ScreenToClient)
#define MapWindowPoints U(MapWindowPoints)
#define GetMenu U(GetMenu)
#define DlgDirListA U(DlgDirListA)
#define DlgDirListW U(DlgDirListW)
#define DlgDirSelectExA U(DlgDirSelectExA)
#define DlgDirSelectExW U(DlgDirSelectExW)
#define DlgDirListComboBoxA U(DlgDirListComboBoxA)
#define DlgDirListComboBoxW U(DlgDirListComboBoxW)
#define DlgDirSelectComboBoxExA U(DlgDirSelectComboBoxExA)
#define DlgDirSelectComboBoxExW U(DlgDirSelectComboBoxExW)

/* single-evaluation equality check (EXPECT_EQ evaluates its arguments twice) */
#define EQ(a, b) do { ULONG_PTR a__ = (ULONG_PTR)(a), b__ = (ULONG_PTR)(b); \
    t_check(a__ == b__, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a__, (unsigned long long)b__); } while (0)

/* ---- template builder ---- */
typedef struct { BYTE b[16384]; int n; } DT;
static void dt_b(DT *d, const void *p, int n) { memcpy(d->b + d->n, p, n); d->n += n; }
static void dt_w(DT *d, WORD v) { dt_b(d, &v, 2); }
static void dt_d(DT *d, DWORD v) { dt_b(d, &v, 4); }
static void dt_al(DT *d) { while (d->n & 3) d->b[d->n++] = 0; }
static void dt_s(DT *d, const WCHAR *s) { dt_b(d, s, (lstrlenW(s) + 1) * 2); }
/* NULL -> 0x0000, small value -> ordinal (0xFFFF, v), else string */
static void dt_os(DT *d, const void *p) {
    if (!p) dt_w(d, 0);
    else if ((ULONG_PTR)p < 0x10000) { dt_w(d, 0xFFFF); dt_w(d, (WORD)(ULONG_PTR)p); }
    else dt_s(d, (const WCHAR *)p);
}
/* classic DLGTEMPLATE header; font != NULL (or style has DS_SETFONT) appends font info */
static void dt_hdr(DT *d, DWORD style, DWORD ex, WORD n, short x, short y, short cx, short cy,
                   const void *menu, const void *cls, const WCHAR *title, int pt, const WCHAR *font) {
    memset(d, 0, sizeof *d);
    if (font) style |= DS_SETFONT;
    dt_d(d, style); dt_d(d, ex); dt_w(d, n);
    dt_w(d, (WORD)x); dt_w(d, (WORD)y); dt_w(d, (WORD)cx); dt_w(d, (WORD)cy);
    dt_os(d, menu); dt_os(d, cls); dt_s(d, title ? title : L"");
    if (style & DS_SETFONT) { dt_w(d, (WORD)pt); dt_s(d, font ? font : L"MS Shell Dlg"); }
}
static void dt_item_x(DT *d, DWORD style, DWORD ex, short x, short y, short cx, short cy, WORD id,
                      const void *cls, const void *title, const void *extra, WORD nextra) {
    dt_al(d);
    dt_d(d, style); dt_d(d, ex);
    dt_w(d, (WORD)x); dt_w(d, (WORD)y); dt_w(d, (WORD)cx); dt_w(d, (WORD)cy);
    dt_w(d, id);
    dt_os(d, cls); dt_os(d, title);
    dt_w(d, nextra);
    if (nextra) dt_b(d, extra, nextra);
}
static void dt_item(DT *d, DWORD style, DWORD ex, short x, short y, short cx, short cy, WORD id,
                    const void *cls, const void *title) {
    dt_item_x(d, style, ex, x, y, cx, cy, id, cls, title, NULL, 0);
}
/* DLGTEMPLATEEX */
static void dtx_hdr(DT *d, DWORD style, DWORD ex, WORD n, short x, short y, short cx, short cy,
                    const void *menu, const void *cls, const WCHAR *title, int pt, int weight,
                    int italic, int charset, const WCHAR *font, DWORD helpid) {
    memset(d, 0, sizeof *d);
    if (font) style |= DS_SETFONT;
    dt_w(d, 1); dt_w(d, 0xFFFF); dt_d(d, helpid); dt_d(d, ex); dt_d(d, style); dt_w(d, n);
    dt_w(d, (WORD)x); dt_w(d, (WORD)y); dt_w(d, (WORD)cx); dt_w(d, (WORD)cy);
    dt_os(d, menu); dt_os(d, cls); dt_s(d, title ? title : L"");
    if (style & (DS_SETFONT | DS_SHELLFONT)) {
        dt_w(d, (WORD)pt); dt_w(d, (WORD)weight); d->b[d->n++] = (BYTE)italic; d->b[d->n++] = (BYTE)charset;
        dt_s(d, font ? font : L"MS Shell Dlg");
    }
}
static void dtx_item(DT *d, DWORD style, DWORD ex, short x, short y, short cx, short cy, DWORD id,
                     const void *cls, const void *title, DWORD helpid, const void *extra, WORD nextra) {
    dt_al(d);
    dt_d(d, helpid); dt_d(d, ex); dt_d(d, style);
    dt_w(d, (WORD)x); dt_w(d, (WORD)y); dt_w(d, (WORD)cx); dt_w(d, (WORD)cy);
    dt_d(d, id);
    dt_os(d, cls); dt_os(d, title);
    dt_w(d, nextra);
    if (nextra) dt_b(d, extra, nextra);
}

#define C_BUTTON ((const void *)0x0080)
#define C_EDIT   ((const void *)0x0081)
#define C_STATIC ((const void *)0x0082)
#define C_LIST   ((const void *)0x0083)
#define C_SCROLL ((const void *)0x0084)
#define C_COMBO  ((const void *)0x0085)

#define BASE_STYLE (WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME)

/* ---- generic dialog procedure ---- */
typedef struct {
    int ninit; LPARAM lp; HWND hfocus; HWND hinit; INT_PTR initret;
    int tmo; INT_PTR endval; int timer_fired;
    int nmsg; UINT seq[128];
    int ndestroy, nncdestroy, nsetfont; HFONT font;
    int idx_setfont, idx_init;
    int ncolor; HWND colorhwnd; UINT colormsg;
    int end_in_init; HWND focus_timer, active_timer; void (*onrun)(HWND);
    INT_PTR (CALLBACK *hook)(HWND, UINT, WPARAM, LPARAM); /* optional extra handler, returns -1 for unhandled */
} DS;
static DS ds;
/* watchdog: if a modal loop is still running 20s after the last ds_reset, end the active dialog */
static UINT_PTR wd_id; static DWORD wd_t0;
static VOID CALLBACK wd_proc(HWND h, UINT m, UINT_PTR id, DWORD t) {
    HWND a;
    /* a repeating timer left over from an earlier test must not end a dialog of a later one: act only when this
     * test has really been running for the limit */
    if (GetTickCount() - wd_t0 < 19000) return;
    a = GetActiveWindow();
    if (a) EndDialog(a, -999);
}
static void ds_reset(void) {
    memset(&ds, 0, sizeof ds); ds.initret = 1; ds.idx_setfont = ds.idx_init = -1;
    if (wd_id) KillTimer(NULL, wd_id);
    wd_t0 = GetTickCount();
    wd_id = SetTimer(NULL, 0, 20000, wd_proc);
}
static INT_PTR CALLBACK gdp(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ds.nmsg < 128) ds.seq[ds.nmsg++] = m;
    if (ds.hook) { INT_PTR r = ds.hook(h, m, w, l); if (r != -1) return r; }
    switch (m) {
    case WM_SETFONT: ds.nsetfont++; ds.font = (HFONT)w; if (ds.idx_setfont < 0) ds.idx_setfont = ds.nmsg - 1; return 0;
    case WM_INITDIALOG:
        ds.ninit++; ds.lp = l; ds.hfocus = (HWND)w; ds.hinit = h; ds.idx_init = ds.nmsg - 1;
        if (ds.end_in_init) EndDialog(h, ds.endval);
        else if (ds.tmo) SetTimer(h, 77, ds.tmo, NULL);
        return ds.initret;
    case WM_TIMER:
        if (w == 77) {
            KillTimer(h, 77); ds.timer_fired++;
            ds.focus_timer = GetFocus(); ds.active_timer = GetActiveWindow();
            if (ds.onrun) ds.onrun(h);
            EndDialog(h, ds.endval); return 1;
        }
        return 0;
    case WM_DESTROY: ds.ndestroy++; return 0;
    case WM_NCDESTROY: ds.nncdestroy++; return 0;
    case WM_CTLCOLORDLG: case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: case WM_CTLCOLOREDIT:
        ds.ncolor++; ds.colorhwnd = (HWND)l; ds.colormsg = m; return 0;
    }
    return 0;
}

/* standard control set */
enum { ID_EDIT = 100, ID_STATIC, ID_CHK, ID_R1, ID_R2, ID_R3, ID_BTN, ID_LIST, ID_COMBO, ID_3ST };
static void std_tmpl(DT *d, DWORD style) {
    dt_hdr(d, style, 0, 11, 10, 10, 200, 120, NULL, NULL, L"Std", 8, L"MS Shell Dlg");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 0, 5, 5, 60, 12, ID_EDIT, C_EDIT, L"edit");
    dt_item(d, WS_CHILD | WS_VISIBLE, 0, 70, 5, 60, 12, ID_STATIC, C_STATIC, L"static text");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_AUTOCHECKBOX, 0, 5, 20, 60, 12, ID_CHK, C_BUTTON, L"check");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON, 0, 5, 35, 40, 12, ID_R1, C_BUTTON, L"r1");
    dt_item(d, WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 0, 50, 35, 40, 12, ID_R2, C_BUTTON, L"r2");
    dt_item(d, WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 0, 95, 35, 40, 12, ID_R3, C_BUTTON, L"r3");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_PUSHBUTTON, 0, 5, 50, 40, 14, ID_BTN, C_BUTTON, L"push");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LBS_NOTIFY, 0, 5, 68, 60, 30, ID_LIST, C_LIST, L"");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 70, 68, 60, 60, ID_COMBO, C_COMBO, L"");
    dt_item(d, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTO3STATE, 0, 70, 50, 60, 12, ID_3ST, C_BUTTON, L"tri");
    dt_item(d, WS_CHILD | WS_VISIBLE, 0, 140, 5, 40, 12, 0xFFFF, C_STATIC, L"anon");
}
/* hidden modeless dialog from the standard template; caller destroys */
static HWND mk_std(void) {
    DT d; std_tmpl(&d, BASE_STYLE);
    ds_reset();
    return CreateDialogIndirectParamW(GetModuleHandleA(NULL), (LPCDLGTEMPLATEW)d.b, NULL, gdp, 0);
}
static void wstr_ascii(char *dst, const WCHAR *s) { while ((*dst++ = (char)*s++)); }
/* wdog.c */
int dlg_run_child(const char *test, DWORD timeout_ms, DWORD *exit_code);
#endif
