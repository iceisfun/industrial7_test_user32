#include "wm.h"

#define LOGMAX 8192
static struct { UINT m; WPARAM w; LPARAM l; HWND h; UINT wpf; int wpx, wpy, wpcx, wpcy; } g_log[LOGMAX];
int wm_wpc_mode, wm_ncc_mode;
static int g_nlog;
int wm_fail_nccreate, wm_fail_create, wm_minmax_on;
MINMAXINFO wm_minmax, wm_mmi_seen;
int wm_mmi_count;
CREATESTRUCTA wm_cs_nc, wm_cs_cr;
char wm_name_nc[128], wm_name_cr[128], wm_cls_nc[128], wm_cls_cr[128];
int wm_seen_nc, wm_seen_cr;

void wm_logclear(void) { g_nlog = 0; }
HWND wm_loghwnd(int n) { return (n >= 0 && n < g_nlog) ? g_log[n].h : NULL; }
int wm_logcount_h(UINT msg, HWND h) { int n = 0; for (int i = 0; i < g_nlog; i++) if (g_log[i].m == msg && g_log[i].h == h) n++; return n; }
int wm_logfind_h(UINT msg, HWND h, int from) { for (int i = from < 0 ? 0 : from; i < g_nlog; i++) if (g_log[i].m == msg && g_log[i].h == h) return i; return -1; }
UINT wm_logwpflags(int n) { return (n >= 0 && n < g_nlog) ? g_log[n].wpf : 0; }
int wm_logwpsize(int n, int *cx, int *cy) { if (n < 0 || n >= g_nlog) return 0; *cx = g_log[n].wpcx; *cy = g_log[n].wpcy; return 1; }
int wm_logtotal(void) { return g_nlog; }
int wm_logcount(UINT msg) { int n = 0; for (int i = 0; i < g_nlog; i++) if (g_log[i].m == msg) n++; return n; }
int wm_logfind(UINT msg, int from) { for (int i = from < 0 ? 0 : from; i < g_nlog; i++) if (g_log[i].m == msg) return i; return -1; }
int wm_lognth(int n, UINT *m, WPARAM *w, LPARAM *l) {
    if (n < 0 || n >= g_nlog) return 0;
    if (m) *m = g_log[n].m;
    if (w) *w = g_log[n].w;
    if (l) *l = g_log[n].l;
    return 1;
}

static LRESULT CALLBACK wm_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    /* skip noisy / timing dependent messages */
    if (m != WM_NCHITTEST && m != WM_SETCURSOR && m != WM_PAINT && m != WM_ERASEBKGND &&
        m != WM_NCPAINT && m != WM_MOUSEMOVE && m != WM_TIMER && m != 0x0118 /*SYSTIMER*/ &&
        m != WM_GETICON && m != WM_NCMOUSEMOVE && m != 0x0090 && m != 0x0091 && m != 0x0092 &&
        m != WM_IME_SETCONTEXT && m != WM_IME_NOTIFY && m != 0x0093 && m != 0x0094) {
        if (g_nlog < LOGMAX) { g_log[g_nlog].wpf = 0;
            if ((m == WM_WINDOWPOSCHANGING || m == WM_WINDOWPOSCHANGED) && l) {
                WINDOWPOS *wp = (WINDOWPOS *)l;
                g_log[g_nlog].wpf = wp->flags; g_log[g_nlog].wpx = wp->x; g_log[g_nlog].wpy = wp->y;
                g_log[g_nlog].wpcx = wp->cx; g_log[g_nlog].wpcy = wp->cy;
            }
            g_log[g_nlog].m = m; g_log[g_nlog].h = h; g_log[g_nlog].w = w; g_log[g_nlog].l = l; g_nlog++; }
    }
    if (m == WM_NCCREATE) { CREATESTRUCTA *c = (CREATESTRUCTA *)l; wm_cs_nc = *c; wm_seen_nc++; wm_cls_nc[0] = 0; if (c->lpszClass && !IS_INTRESOURCE(c->lpszClass)) lstrcpynA(wm_cls_nc, c->lpszClass, 128); wm_name_nc[0] = 0; if (c->lpszName && !IS_INTRESOURCE(c->lpszName)) lstrcpynA(wm_name_nc, c->lpszName, 128); }
    if (m == WM_CREATE) { CREATESTRUCTA *c = (CREATESTRUCTA *)l; wm_cs_cr = *c; wm_seen_cr++; wm_cls_cr[0] = 0; if (c->lpszClass && !IS_INTRESOURCE(c->lpszClass)) lstrcpynA(wm_cls_cr, c->lpszClass, 128); wm_name_cr[0] = 0; if (c->lpszName && !IS_INTRESOURCE(c->lpszName)) lstrcpynA(wm_name_cr, c->lpszName, 128); }
    if (m == WM_NCCREATE && wm_fail_nccreate) return FALSE;
    if (m == WM_CREATE && wm_fail_create) return -1;
    if (m == WM_WINDOWPOSCHANGING && wm_wpc_mode) {
        WINDOWPOS *wp = (WINDOWPOS *)l;
        if (wm_wpc_mode == 1) { wp->cx = 77; wp->cy = 66; }
        else if (wm_wpc_mode == 2) wp->flags |= SWP_NOMOVE;
        else if (wm_wpc_mode == 3) wp->flags |= SWP_NOSIZE;
    }
    if (m == WM_WINDOWPOSCHANGED && wm_wpc_mode == 4) return 0;   /* swallow: no WM_SIZE / WM_MOVE */
    if (m == WM_NCCALCSIZE && wm_ncc_mode) {
        RECT *r = w ? &((NCCALCSIZE_PARAMS *)l)->rgrc[0] : (RECT *)l;
        r->left += 10; r->top += 12; r->right -= 14; r->bottom -= 16;
        return 0;
    }
    if (m == WM_GETMINMAXINFO) {
        MINMAXINFO *p = (MINMAXINFO *)l;
        if (w == 0 && l) { wm_mmi_seen = *p; wm_mmi_count++; }
        if (wm_minmax_on) { *p = wm_minmax; return 0; }
    }
    return DefWindowProcA(h, m, w, l);
}
static LRESULT CALLBACK wm_procw(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

const char *wm_cls(void) {
    static int done;
    static const char *n = "WmTestClassA";
    if (!done) {
        WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc; wc.lpfnWndProc = wm_proc; wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = n; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.cbWndExtra = 16;
        U(RegisterClassExA)(&wc);
        done = 1;
    }
    return n;
}
const WCHAR *wm_clsw(void) {
    static int done;
    static const WCHAR n[] = L"WmTestClassW";
    if (!done) {
        WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc; wc.lpfnWndProc = wm_procw; wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = n; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        U(RegisterClassExW)(&wc);
        done = 1;
    }
    return n;
}

HWND wm_mk(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent) {
    return U(CreateWindowExA)(ex, wm_cls(), "wm", style, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
HWND wm_mkw(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent) {
    return U(CreateWindowExW)(ex, wm_clsw(), L"wm", style, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
HWND wm_hidden(void) { return wm_mk(WS_OVERLAPPEDWINDOW, 0, 50, 50, 200, 150, NULL); }
void wm_destroy(HWND h) { if (h && U(IsWindow)(h)) U(DestroyWindow)(h); }

int wm_isabove(HWND a, HWND b) {
    HWND c = a; int n = 0;
    while (c && n++ < 10000) {
        c = U(GetWindow)(c, GW_HWNDNEXT);
        if (c == b) return 1;
    }
    return 0;
}

static DWORD WINAPI worker_main(LPVOID p) {
    wm_worker *w = p;
    MSG m;
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE); /* create queue */
    w->hwnd = wm_mk(w->style, w->ex, 60, 60, 150, 100, w->parent);
    if (w->hwnd && w->show) U(ShowWindow)(w->hwnd, SW_SHOWNA);
    SetEvent(w->ready);
    for (;;) {
        DWORD r = MsgWaitForMultipleObjects(1, &w->stop, FALSE, WM_TIMEOUT_MS * 4, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT || r == WAIT_FAILED) break;
        while (U(PeekMessageA)(&m, NULL, 0, 0, PM_REMOVE)) { U(TranslateMessage)(&m); U(DispatchMessageA)(&m); }
    }
    if (w->hwnd && U(IsWindow)(w->hwnd)) U(DestroyWindow)(w->hwnd);
    return 0;
}
int wm_worker_start(wm_worker *w, DWORD style, DWORD ex, int show) {
    wm_cls();
    w->parent = NULL;
    w->style = style; w->ex = ex; w->show = show; w->hwnd = NULL;
    w->stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    w->ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    w->th = CreateThread(NULL, 0, worker_main, w, 0, &w->tid);
    if (!w->th) return 0;
    if (WaitForSingleObject(w->ready, WM_TIMEOUT_MS) != WAIT_OBJECT_0) return 0;
    return w->hwnd != NULL;
}
void wm_worker_stop(wm_worker *w) {
    if (w->stop) SetEvent(w->stop);
    if (w->th) { WaitForSingleObject(w->th, WM_TIMEOUT_MS); CloseHandle(w->th); }
    if (w->stop) CloseHandle(w->stop);
    if (w->ready) CloseHandle(w->ready);
    memset(w, 0, sizeof *w);
}
