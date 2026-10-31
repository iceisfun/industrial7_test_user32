#include "pg.h"
pg_rec pg_r;
static LRESULT CALLBACK recproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SYSCOLORCHANGE: pg_r.syscolor++; break;
    case WM_ERASEBKGND:
        pg_r.erase++; pg_r.erase_wp = w; pg_r.erase_dctype = GetObjectType((HDC)w); GetClipBox((HDC)w, &pg_r.erase_clip);
        if (pg_r.norder < 64) pg_r.order[pg_r.norder++] = WM_ERASEBKGND;
        if (pg_r.erase_ret >= 0) return pg_r.erase_ret;
        break;
    case WM_NCPAINT:
        pg_r.ncpaint++; pg_r.ncpaint_wp = w; if (pg_r.norder < 64) pg_r.order[pg_r.norder++] = WM_NCPAINT;
        break;
    case WM_PAINT:
        pg_r.paint++; if (pg_r.norder < 64) pg_r.order[pg_r.norder++] = WM_PAINT;
        pg_r.paint_wp = w;
        if (w) { pg_r.prt_paint_wp_hdc = (GetObjectType((HDC)w) == OBJ_DC || GetObjectType((HDC)w) == OBJ_MEMDC); return 0; }
        GetUpdateRect(h, &pg_r.upd_at_paint, FALSE);
        if (pg_r.do_paint == 1) {
            PAINTSTRUCT ps; BeginPaint(h, &ps);
            pg_r.ps_rc = ps.rcPaint; pg_r.ps_erase = ps.fErase;
            EndPaint(h, &ps);
            return 0;
        }
        if (pg_r.do_paint == 2) return 0;
        break;
    }
    return DefWindowProcA(h, m, w, l);
}
static ATOM g_atom;
static void reg(void) {
    if (g_atom) return;
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = recproc; wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "PGRecClass"; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = 0;
    g_atom = U(RegisterClassA)(&wc);
}
HWND pg_recwindow(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent) {
    reg();
    return U(CreateWindowExA)(ex, "PGRecClass", "pg", style, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
HWND pg_window(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent) {
    return t_make_window(NULL, style, ex, x, y, w, h, parent);
}
void pg_reset(void) { memset(&pg_r, 0, sizeof pg_r); pg_r.do_paint = 1; pg_r.erase_ret = -1; }
void pg_pump(void) { t_pump(); }
void pg_flush(HWND h) {
    int n = 0;
    t_pump();
    while (n++ < 10 && U(GetUpdateRect)(h, NULL, FALSE)) {
        U(UpdateWindow)(h);
        t_pump();
        if (U(GetUpdateRect)(h, NULL, FALSE)) { U(ValidateRect)(h, NULL); }
    }
}
HWND pg_topwin(int x, int y, int w, int h) {
    HWND hw = pg_recwindow(WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, x, y, w, h, NULL);
    if (hw) { U(ShowWindow)(hw, SW_SHOWNA); pg_flush(hw); }
    return hw;
}
HWND pg_topwin_cls(const char *cls, int x, int y, int w, int h, HWND parent, DWORD style) {
    return U(CreateWindowExA)(parent ? 0 : (WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE), cls, "pg", style, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
BOOL pg_visible_probe(HDC dc, int x, int y) { return PtVisible(dc, x, y); }

BOOL pg_scaled(void) {
    /* DPI-unaware process on a scaled desktop: screen metrics are virtualised while DC origins are physical */
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (!U(EnumDisplaySettingsW) || !U(EnumDisplaySettingsW)(NULL, ENUM_CURRENT_SETTINGS, &dm)) return FALSE;
    return (int)dm.dmPelsWidth != U(GetSystemMetrics)(SM_CXSCREEN) || (int)dm.dmPelsHeight != U(GetSystemMetrics)(SM_CYSCREEN);
}

void pg_gui(void) {
    /* make this thread a GUI thread (some DPI APIs report nothing for a thread that never touched win32k) */
    static int done;
    if (done) return;
    HWND w = t_make_window(NULL, WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0, 0, 1, 1, NULL);
    if (w) { if (U(DestroyWindow)) U(DestroyWindow)(w); t_pump(); }
    done = 1;
}
