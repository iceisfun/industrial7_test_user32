#include "cbcommon.h"
UINT cb_log[CB_LOGMAX]; WPARAM cb_logw[CB_LOGMAX]; int cb_nlog;
HANDLE cb_render_data;
int cb_render_open_ok;
void cb_logclear(void) { cb_nlog = 0; }
int cb_logcount(UINT m) { int n = 0; for (int i = 0; i < cb_nlog; i++) if (cb_log[i] == m) n++; return n; }
HWND cb_next;
static LRESULT CALLBACK cbproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if ((m == WM_DRAWCLIPBOARD || m == WM_CHANGECBCHAIN) && cb_next && U(IsWindow)(cb_next)) {
        if (m == WM_CHANGECBCHAIN && (HWND)w == cb_next) { cb_next = (HWND)l; }
        else { DWORD_PTR rr; U(SendMessageTimeoutA)(cb_next, m, w, l, SMTO_ABORTIFHUNG, 2000, &rr); }
    }
    switch (m) {
    case WM_RENDERFORMAT: case WM_RENDERALLFORMATS: case WM_DESTROYCLIPBOARD:
    case WM_DRAWCLIPBOARD: case WM_CHANGECBCHAIN: case WM_CLIPBOARDUPDATE:
    case WM_ASKCBFORMATNAME: case WM_PAINTCLIPBOARD: case WM_SIZECLIPBOARD:
        if (cb_nlog < CB_LOGMAX) { cb_log[cb_nlog] = m; cb_logw[cb_nlog++] = w; }
        if (m == WM_RENDERFORMAT && cb_render_data)
            U(SetClipboardData)((UINT)w, cb_render_data);
        if (m == WM_RENDERALLFORMATS && cb_render_data)
            U(SetClipboardData)(CF_TEXT, cb_render_data);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}
HWND cb_win(void) {
    static int reg;
    if (!reg) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = cbproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "CbTestOwner";
        U(RegisterClassA)(&wc); reg = 1;
    }
    return U(CreateWindowExA)(0, "CbTestOwner", "cb", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
}
void cb_destroy(HWND h) { if (h) U(DestroyWindow)(h); }
void cb_reset(void) {
    if (!OPEN) return;
    int n = 0;
    while (n++ < 8 && CLOSE()) ;
    cb_render_data = NULL;
    for (int t = 0; t < 100; t++) { if (OPEN(NULL)) { EMPTY(); CLOSE(); break; } Sleep(10); }
}
HGLOBAL cb_mem(const void *p, size_t n) {
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n ? n : 1);
    if (g && p) { void *d = GlobalLock(g); memcpy(d, p, n); GlobalUnlock(g); }
    return g;
}
HGLOBAL cb_text(const char *s) { return cb_mem(s, strlen(s) + 1); }
HGLOBAL cb_wtext(const WCHAR *s) { return cb_mem(s, (lstrlenW(s) + 1) * 2); }
int cb_has(UINT fmt) {
    UINT f = 0; int found = 0, n = 0;
    while (n++ < 1000 && (f = ENUM(f))) if (f == fmt) found = 1;
    return found;
}
