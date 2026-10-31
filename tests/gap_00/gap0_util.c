#include "gap0.h"

int g0_bsm_count;
UINT g0_bsm_msg;
LRESULT g0_deny;
int g0_create_count;
CREATESTRUCTW g0_last_cs;
WCHAR g0_last_cls[64];

static LRESULT CALLBACK g0_wp(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CREATE || m == WM_NCCREATE) {
        if (m == WM_CREATE) { g0_create_count++; g0_last_cs = *(CREATESTRUCTW *)l; }
    }
    if (g0_bsm_msg && m == g0_bsm_msg) {
        g0_bsm_count++;
        if (g0_deny) return g0_deny;
        return 1;
    }
    return U(DefWindowProcW)(h, m, w, l);
}

const WCHAR *g0_cls(void) {
    static int done;
    static const WCHAR name[] = L"Gap0TestClassW";
    if (!done) {
        WNDCLASSW wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = g0_wp;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = name;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        if (U(RegisterClassW)) U(RegisterClassW)(&wc);
        done = 1;
    }
    return name;
}

HWND g0_mk(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent) {
    if (!U(CreateWindowExW)) return NULL;
    return U(CreateWindowExW)(ex, g0_cls(), L"gap0", style, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}

void g0_pump_ms(int ms) {
    DWORD end = GetTickCount() + ms;
    do { t_pump(); Sleep(5); } while ((int)(end - GetTickCount()) > 0);
}

int g0_surf_make(G0Surf *s, int w, int h, DWORD fill) {
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    memset(s, 0, sizeof *s);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    s->dc = CreateCompatibleDC(NULL);
    if (!s->dc) return 0;
    s->bmp = CreateDIBSection(s->dc, &bi, DIB_RGB_COLORS, (void **)&s->bits, NULL, 0);
    if (!s->bmp || !s->bits) { DeleteDC(s->dc); s->dc = NULL; return 0; }
    s->old = SelectObject(s->dc, s->bmp);
    s->w = w; s->h = h;
    for (int i = 0; i < w * h; i++) s->bits[i] = fill;
    GdiFlush();
    return 1;
}
void g0_surf_free(G0Surf *s) {
    if (s->dc) { SelectObject(s->dc, s->old); DeleteObject(s->bmp); DeleteDC(s->dc); }
    memset(s, 0, sizeof *s);
}
DWORD g0_px(G0Surf *s, int x, int y) { GdiFlush(); return s->bits[y * s->w + x] & 0xFFFFFF; }
DWORD g0_rgb(COLORREF c) { return ((DWORD)GetRValue(c) << 16) | ((DWORD)GetGValue(c) << 8) | GetBValue(c); }

static BYTE *al4(BYTE *base, BYTE *p) { size_t o = p - base; o = (o + 3) & ~(size_t)3; return base + o; }
size_t g0_build_dlg(void *buf, DWORD style, DWORD ex, const WCHAR *title, int nb) {
    BYTE *b = buf, *p = b;
    DLGTEMPLATE *t = (DLGTEMPLATE *)p;
    t->style = style; t->dwExtendedStyle = ex; t->cdit = (WORD)nb;
    t->x = 0; t->y = 0; t->cx = 100; t->cy = 60;
    p += sizeof *t;
    *(WORD *)p = 0; p += 2;                 /* no menu */
    *(WORD *)p = 0; p += 2;                 /* default class */
    size_t n = (lstrlenW(title) + 1) * 2; memcpy(p, title, n); p += n;
    for (int i = 0; i < nb; i++) {
        p = al4(b, p);
        DLGITEMTEMPLATE *it = (DLGITEMTEMPLATE *)p;
        it->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON; it->dwExtendedStyle = 0;
        it->x = 5 + i * 30; it->y = 5; it->cx = 25; it->cy = 12; it->id = (WORD)(IDOK + i);
        p += sizeof *it;
        *(WORD *)p = 0xFFFF; p += 2; *(WORD *)p = 0x0080; p += 2;   /* button class atom */
        *(WORD *)p = 'O'; p += 2; *(WORD *)p = 0; p += 2;           /* title "O" */
        *(WORD *)p = 0; p += 2;                                     /* creation data */
    }
    return (size_t)(p - b);
}
