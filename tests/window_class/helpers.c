#include "wc.h"

const char *wc_uname(char *buf, size_t n, const char *base) {
    static unsigned ctr;
    snprintf(buf, n, "%s_%lu_%u", base, (unsigned long)GetCurrentProcessId(), ++ctr);
    return buf;
}

LRESULT CALLBACK wc_defA(HWND h, UINT m, WPARAM w, LPARAM l) { return U(DefWindowProcA)(h, m, w, l); }
LRESULT CALLBACK wc_defW(HWND h, UINT m, WPARAM w, LPARAM l) { return U(DefWindowProcW)(h, m, w, l); }

void wc_widen(WCHAR *dst, const char *src) { while ((*dst++ = (unsigned char)*src++)); }

ATOM wc_regA(const char *name, UINT style, int cbCls, int cbWnd, WNDPROC proc) {
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.style = style; wc.cbClsExtra = cbCls; wc.cbWndExtra = cbWnd;
    wc.lpfnWndProc = proc ? proc : wc_defA; wc.hInstance = wc_inst(); wc.lpszClassName = name;
    return U(RegisterClassExA) ? U(RegisterClassExA)(&wc) : 0;
}
ATOM wc_regW(const WCHAR *name, UINT style, int cbCls, int cbWnd, WNDPROC proc) {
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.style = style; wc.cbClsExtra = cbCls; wc.cbWndExtra = cbWnd;
    wc.lpfnWndProc = proc ? proc : wc_defW; wc.hInstance = wc_inst(); wc.lpszClassName = name;
    return U(RegisterClassExW) ? U(RegisterClassExW)(&wc) : 0;
}
HWND wc_createA(const char *cls, DWORD style, HWND parent) {
    return U(CreateWindowExA)(0, cls, "wc", style, 0, 0, 50, 50, parent, NULL, wc_inst(), NULL);
}
HWND wc_createW(const WCHAR *cls, DWORD style, HWND parent) {
    return U(CreateWindowExW)(0, cls, L"wc", style, 0, 0, 50, 50, parent, NULL, wc_inst(), NULL);
}

struct wc_rec wc_rec;
LRESULT CALLBACK wc_recA(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE || m == WM_CREATE) {
        CREATESTRUCTA *cs = (CREATESTRUCTA *)l;
        if (m == WM_NCCREATE) wc_rec.nccreate++; else wc_rec.created++;
        wc_rec.unicode = 0;
        if (cs->lpszName) { strncpy(wc_rec.textA, cs->lpszName, sizeof wc_rec.textA - 1); }
        wc_rec.msg = m;
    } else if (m == WM_SETTEXT) {
        wc_rec.count++; wc_rec.msg = m; wc_rec.unicode = 0;
        strncpy(wc_rec.textA, (const char *)l, sizeof wc_rec.textA - 1);
        return TRUE;
    } else if (m == WM_USER + 77) {
        wc_rec.count++; wc_rec.msg = m; wc_rec.wp = w; wc_rec.lp = l; wc_rec.unicode = 0;
        return 0x1234;
    }
    return U(DefWindowProcA)(h, m, w, l);
}
LRESULT CALLBACK wc_recW(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE || m == WM_CREATE) {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)l;
        if (m == WM_NCCREATE) wc_rec.nccreate++; else wc_rec.created++;
        wc_rec.unicode = 1;
        if (cs->lpszName) { wcsncpy(wc_rec.textW, cs->lpszName, 299); }
        wc_rec.msg = m;
    } else if (m == WM_SETTEXT) {
        wc_rec.count++; wc_rec.msg = m; wc_rec.unicode = 1;
        wcsncpy(wc_rec.textW, (const WCHAR *)l, 299);
        return TRUE;
    } else if (m == WM_USER + 77) {
        wc_rec.count++; wc_rec.msg = m; wc_rec.wp = w; wc_rec.lp = l; wc_rec.unicode = 1;
        return 0x1234;
    }
    return U(DefWindowProcW)(h, m, w, l);
}

int wc_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
