#include "im.h"

imrec im_log[IM_MAXREC];
int im_nlog;
im_hook_t im_hook;
int im_aware;

static int is_logged(UINT m) {
    return (m >= 0x200 && m <= 0x20E) || (m >= 0xA0 && m <= 0xAD) || m == WM_MOUSELEAVE || m == WM_NCMOUSELEAVE ||
           m == WM_MOUSEHOVER || m == WM_NCMOUSEHOVER || m == WM_CAPTURECHANGED || m == WM_SETCURSOR ||
           m == WM_MOUSEACTIVATE || m == WM_INPUT || m == WM_NCHITTEST || m == WM_APPCOMMAND ||
           m == WM_CONTEXTMENU || m == WM_ACTIVATE || m == WM_KILLFOCUS || m == WM_SETFOCUS ||
           (m >= 0x240 && m <= 0x24A) || (m >= 0x119 && m <= 0x119);
}

static LRESULT CALLBACK im_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (is_logged(m) && m != WM_NCHITTEST && im_nlog < IM_MAXREC) {
        im_log[im_nlog].m = m; im_log[im_nlog].w = w; im_log[im_nlog].l = l; im_log[im_nlog].h = h;
        im_log[im_nlog].tid = GetCurrentThreadId();
        im_nlog++;
    }
    if (im_hook) {
        int handled = 0;
        LRESULT r = im_hook(h, m, w, l, &handled);
        if (handled) return r;
    }
    /* never let default processing open menus / modal loops from synthetic clicks */
    switch (m) {
    case WM_NCLBUTTONDOWN: case WM_NCLBUTTONUP: case WM_NCRBUTTONDOWN: case WM_NCRBUTTONUP:
    case WM_NCMBUTTONDOWN: case WM_NCMBUTTONUP: case WM_NCLBUTTONDBLCLK: case WM_CONTEXTMENU:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK: case WM_NCXBUTTONDOWN: case WM_NCXBUTTONUP:
    case WM_APPCOMMAND:
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static const char *reg(const char *name, UINT cs, LPCSTR cursor) {
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    ATOM (WINAPI *rc)(const WNDCLASSA *) = (void *)t_proc("RegisterClassA");
    HCURSOR (WINAPI *lc)(HINSTANCE, LPCSTR) = (void *)t_proc("LoadCursorA");
    wc.style = cs; wc.lpfnWndProc = im_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = name;
    wc.hCursor = lc ? lc(NULL, cursor) : NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (rc) rc(&wc);
    return name;
}
const char *im_cls(void) { static const char *n; if (!n) n = reg("IM_DblClkWnd", CS_DBLCLKS, IDC_CROSS); return n; }
const char *im_cls_plain(void) { static const char *n; if (!n) n = reg("IM_PlainWnd", 0, IDC_ARROW); return n; }

HWND im_win_cls(const char *cls, DWORD st, DWORD ex, int x, int y, int w, int h, HWND parent) {
    HWND (WINAPI *cw)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) = (void *)t_proc("CreateWindowExA");
    if (!cw) return NULL;
    return cw(ex, cls, "im", st, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
HWND im_win(DWORD st, DWORD ex, int x, int y, int w, int h, HWND parent) {
    return im_win_cls(im_cls(), st, ex, x, y, w, h, parent);
}
HWND im_popup(int x, int y, int w, int h) {
    HWND hw = im_win(WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST | WS_EX_NOACTIVATE, x, y, w, h, NULL);
    im_pump_ms(10);
    return hw;
}
void im_destroy(HWND h) {
    BOOL (WINAPI *dw)(HWND) = (void *)t_proc("DestroyWindow");
    if (h && dw) dw(h);
    im_pump_ms(1);
}
void im_pump_ms(int ms) {
    DWORD end = GetTickCount() + ms;
    BOOL (WINAPI *pm)(LPMSG, HWND, UINT, UINT, UINT) = (void *)t_proc("PeekMessageA");
    BOOL (WINAPI *tm)(const MSG *) = (void *)t_proc("TranslateMessage");
    LRESULT (WINAPI *dm)(const MSG *) = (void *)t_proc("DispatchMessageA");
    MSG m;
    if (!pm) return;
    do {
        int n = 0;
        while (n++ < 2000 && pm(&m, NULL, 0, 0, PM_REMOVE)) { if (tm) tm(&m); if (dm) dm(&m); }
        if (ms > 0) Sleep(2);
    } while ((int)(end - GetTickCount()) > 0);
}
void im_pump_until(UINT msg, int ms) {
    DWORD end = GetTickCount() + ms;
    while (im_count(msg) == 0 && (int)(end - GetTickCount()) > 0) im_pump_ms(10);
}
void im_clear(void) { im_nlog = 0; }
int im_count(UINT msg) { int c = 0; for (int i = 0; i < im_nlog; i++) if (im_log[i].m == msg) c++; return c; }
int im_find(UINT msg, imrec *o) { for (int i = im_nlog - 1; i >= 0; i--) if (im_log[i].m == msg) { if (o) *o = im_log[i]; return 1; } return 0; }
int im_first(UINT msg, imrec *o) { for (int i = 0; i < im_nlog; i++) if (im_log[i].m == msg) { if (o) *o = im_log[i]; return 1; } return 0; }

void im_save(imstate *s) {
    memset(s, 0, sizeof *s);
    BOOL (WINAPI *gcp)(LPPOINT) = (void *)t_proc("GetCursorPos");
    BOOL (WINAPI *gcc)(LPRECT) = (void *)t_proc("GetClipCursor");
    UINT (WINAPI *gdc)(void) = (void *)t_proc("GetDoubleClickTime");
    int (WINAPI *gsm)(int) = (void *)t_proc("GetSystemMetrics");
    HCURSOR (WINAPI *gc)(void) = (void *)t_proc("GetCursor");
    if (gcp) gcp(&s->pos);
    if (gcc) gcc(&s->clip);
    if (gdc) s->dbl = gdc();
    if (gsm) s->swap = gsm(SM_SWAPBUTTON);
    if (gc) s->cur = gc();
    {   /* display counter: probe and put back */
        int (WINAPI *sc)(BOOL) = (void *)t_proc("ShowCursor");
        if (sc) { s->cnt = sc(TRUE); sc(FALSE); }
    }
}
void im_restore(imstate *s) {
    BOOL (WINAPI *scp)(int, int) = (void *)t_proc("SetCursorPos");
    BOOL (WINAPI *cc)(const RECT *) = (void *)t_proc("ClipCursor");
    BOOL (WINAPI *sdc)(UINT) = (void *)t_proc("SetDoubleClickTime");
    BOOL (WINAPI *smb)(BOOL) = (void *)t_proc("SwapMouseButton");
    HCURSOR (WINAPI *sc)(HCURSOR) = (void *)t_proc("SetCursor");
    int (WINAPI *shc)(BOOL) = (void *)t_proc("ShowCursor");
    if (cc) cc(NULL);
    if (sdc) sdc(s->dbl);
    if (smb) smb(s->swap);
    if (shc) {
        int c = shc(TRUE), guard = 0;
        while (c > s->cnt && guard++ < 300) c = shc(FALSE);
        while (c < s->cnt && guard++ < 600) c = shc(TRUE);
    }
    if (sc && s->cur) sc(s->cur);
    if (scp) scp(s->pos.x, s->pos.y);
}

int im_near(int a, int b, int tol) { int d = a - b; return d >= -tol && d <= tol; }
int im_swapped(void) { int (WINAPI *gsm)(int) = (void *)t_proc("GetSystemMetrics"); return gsm ? gsm(SM_SWAPBUTTON) : 0; }

void im_setpos(int x, int y) {
    BOOL (WINAPI *scp)(int, int) = (void *)t_proc("SetCursorPos");
    if (scp) scp(x, y);
    im_pump_ms(15);
}
void im_abs_move(int px, int py) {
    UINT (WINAPI *si)(UINT, LPINPUT, int) = (void *)t_proc("SendInput");
    int (WINAPI *gsm)(int) = (void *)t_proc("GetSystemMetrics");
    INPUT in; memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE;
    in.mi.dx = (LONG)(((long long)px * 65536 + 32768) / gsm(SM_CXSCREEN));
    in.mi.dy = (LONG)(((long long)py * 65536 + 32768) / gsm(SM_CYSCREEN));
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    if (si) si(1, &in, sizeof in);
    im_pump_ms(15);
}
void im_click_btn(DWORD down, DWORD up, DWORD data) {
    UINT (WINAPI *si)(UINT, LPINPUT, int) = (void *)t_proc("SendInput");
    INPUT in[2]; memset(in, 0, sizeof in);
    in[0].type = in[1].type = INPUT_MOUSE;
    in[0].mi.dwFlags = down; in[0].mi.mouseData = data;
    in[1].mi.dwFlags = up;   in[1].mi.mouseData = data;
    if (si) si(2, in, sizeof(INPUT));
    im_pump_ms(15);
}

int im_input_ok(void) {
    static int state; /* 0 unknown, 1 ok, -1 not */
    if (state) return state > 0;
    HWND w = im_popup(300, 300, 200, 200);
    imstate s; im_save(&s);
    if (!w) { state = -1; return 0; }
    im_clear();
    im_setpos(350, 350);
    im_setpos(360, 365);
    state = im_count(WM_MOUSEMOVE) ? 1 : -1;
    im_destroy(w);
    im_restore(&s);
    im_clear();
    if (state < 0) t_note("no WM_MOUSEMOVE observed after SetCursorPos: input-dependent checks are skipped");
    return state > 0;
}

/* Process-wide: make coordinates physical so logical == physical. */
static void __attribute__((constructor)) im_dpi(void) {
    typedef BOOL (WINAPI *spdac_t)(HANDLE);
    HMODULE u = LoadLibraryA("user32.dll");
    spdac_t f = u ? (spdac_t)GetProcAddress(u, "SetProcessDpiAwarenessContext") : NULL;
    if (f && f((HANDLE)-4)) im_aware = 1;
}
