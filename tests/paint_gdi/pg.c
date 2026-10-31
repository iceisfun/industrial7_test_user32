#include "pg.h"
pg_rec pg_r;
static LRESULT CALLBACK recproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND:
        pg_r.erase++; if (pg_r.norder < 64) pg_r.order[pg_r.norder++] = WM_ERASEBKGND;
        if (pg_r.erase_ret >= 0) return pg_r.erase_ret;
        break;
    case WM_NCPAINT:
        pg_r.ncpaint++; if (pg_r.norder < 64) pg_r.order[pg_r.norder++] = WM_NCPAINT;
        break;
    case WM_PAINT:
        pg_r.paint++; if (pg_r.norder < 64) pg_r.order[pg_r.norder++] = WM_PAINT;
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
