#include "gap7.h"

g7_ev g7_log[G7_LOGMAX];
int g7_nlog;
UINT g7_watch_lo = 1, g7_watch_hi = 0;
LRESULT g7_ret;

static LRESULT CALLBACK g7_procW(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m >= g7_watch_lo && m <= g7_watch_hi) {
        if (g7_nlog < G7_LOGMAX) { g7_log[g7_nlog].h = h; g7_log[g7_nlog].m = m; g7_log[g7_nlog].w = w; g7_log[g7_nlog].l = l; g7_nlog++; }
        return g7_ret;
    }
    return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK g7_procA(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m >= g7_watch_lo && m <= g7_watch_hi) {
        if (g7_nlog < G7_LOGMAX) { g7_log[g7_nlog].h = h; g7_log[g7_nlog].m = m; g7_log[g7_nlog].w = w; g7_log[g7_nlog].l = l; g7_nlog++; }
        return g7_ret;
    }
    return DefWindowProcA(h, m, w, l);
}

const WCHAR *g7_clsW(void)
{
    static int done;
    static const WCHAR name[] = L"Gap7ClassW";
    if (!done) {
        WNDCLASSW wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = g7_procW; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = name;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        ATOM (WINAPI *rc)(const WNDCLASSW *) = (void *)t_proc("RegisterClassW");
        if (rc) rc(&wc);
        done = 1;
    }
    return name;
}
const char *g7_clsA(void)
{
    static int done;
    if (!done) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = g7_procA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "Gap7ClassA";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        ATOM (WINAPI *rc)(const WNDCLASSA *) = (void *)t_proc("RegisterClassA");
        if (rc) rc(&wc);
        done = 1;
    }
    return "Gap7ClassA";
}
HWND g7_mkwinW(DWORD style, DWORD ex, HWND parent)
{
    HWND (WINAPI *cw)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) = (void *)t_proc("CreateWindowExW");
    if (!cw) return NULL;
    return cw(ex, g7_clsW(), L"g7", style, 10, 10, 120, 80, parent, NULL, GetModuleHandleA(NULL), NULL);
}
HWND g7_mkwinA(DWORD style, DWORD ex, HWND parent)
{
    HWND (WINAPI *cw)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) = (void *)t_proc("CreateWindowExA");
    if (!cw) return NULL;
    return cw(ex, g7_clsA(), "g7", style, 10, 10, 120, 80, parent, NULL, GetModuleHandleA(NULL), NULL);
}
void g7_reset(UINT lo, UINT hi, LRESULT ret) { g7_nlog = 0; g7_watch_lo = lo; g7_watch_hi = hi; g7_ret = ret; }
int g7_count(HWND h, UINT m)
{
    int n = 0;
    for (int i = 0; i < g7_nlog; i++) if ((!h || g7_log[i].h == h) && g7_log[i].m == m) n++;
    return n;
}
void g7_pump_ms(int ms)
{
    DWORD end = GetTickCount() + ms;
    do { t_pump(); Sleep(5); } while ((int)(end - GetTickCount()) > 0);
}

int g7_is_wine(void)
{
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}
