/* misc_ui: shared helpers (header-only, static). */
#ifndef MU_H
#define MU_H
#include "t.h"
#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

#define MU_CLS   "MU_TestCls"
#define MU_CLS_NB "MU_NoBrushCls"

#define MU_MAXLOG 512
typedef struct { UINT msg; WPARAM w; LPARAM l; } mu_rec;
static mu_rec mu_log[MU_MAXLOG];
static int mu_nlog;
static int mu_logging;
/* optional hook: return 1 and fill *res to swallow the message */
typedef int (*mu_hook_fn)(HWND, UINT, WPARAM, LPARAM, LRESULT *);
static mu_hook_fn mu_hook;

static void mu_log_reset(void) { mu_nlog = 0; }
static int mu_count(UINT msg) { int n = 0; for (int i = 0; i < mu_nlog && i < MU_MAXLOG; i++) if (mu_log[i].msg == msg) n++; return n; }
static int mu_find(UINT msg) { for (int i = 0; i < mu_nlog && i < MU_MAXLOG; i++) if (mu_log[i].msg == msg) return i; return -1; }

static LRESULT CALLBACK mu_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (mu_logging && mu_nlog < MU_MAXLOG) {
        mu_log[mu_nlog].msg = m; mu_log[mu_nlog].w = w; mu_log[mu_nlog].l = l; mu_nlog++;
    }
    if (mu_hook) { LRESULT r = 0; if (mu_hook(h, m, w, l, &r)) return r; }
    return DefWindowProcA(h, m, w, l);
}

static int mu_register(void)
{
    static int done;
    if (done) return done > 0;
    done = -1;
    ATOM (WINAPI *rc)(const WNDCLASSEXA *) = U(RegisterClassExA);
    if (!rc) return 0;
    WNDCLASSEXA wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = mu_proc; wc.hInstance = GetModuleHandleA(NULL);
    wc.cbWndExtra = 16;
    wc.hbrBackground = (HBRUSH)(ULONG_PTR)(COLOR_BTNFACE + 1);
    wc.lpszClassName = MU_CLS;
    if (!rc(&wc)) return 0;
    wc.hbrBackground = NULL; wc.lpszClassName = MU_CLS_NB;
    if (!rc(&wc)) return 0;
    done = 1;
    return 1;
}

static HWND mu_mk_cls(const char *cls, const char *title, DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent)
{
    HWND (WINAPI *cw)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) = U(CreateWindowExA);
    if (!cw || !mu_register()) return NULL;
    return cw(ex, cls, title, style, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
static HWND mu_mk(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent)
{
    return mu_mk_cls(MU_CLS, "t", style, ex, x, y, w, h, parent);
}
/* hidden top-level overlapped window at a fixed place */
static HWND mu_top(void) { return mu_mk(WS_OVERLAPPEDWINDOW, 0, 100, 100, 400, 300, NULL); }
static HWND mu_child(HWND parent, DWORD extra) { return mu_mk(WS_CHILD | extra, 0, 5, 5, 50, 40, parent); }
static void mu_kill(HWND h) { if (h && U(IsWindow) && U(IsWindow)(h)) U(DestroyWindow)(h); }
static int mu_pump_n(int n)
{
    MSG m; int c = 0;
    while (c < n && U(PeekMessageA)(&m, NULL, 0, 0, PM_REMOVE)) { U(TranslateMessage)(&m); U(DispatchMessageA)(&m); c++; }
    return c;
}
static void mu_sleep_pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    do { mu_pump_n(1000); Sleep(5); } while ((LONG)(end - GetTickCount()) > 0);
}
static LONG_PTR mu_style(HWND h) { return U(GetWindowLongPtrA)(h, GWL_STYLE); }
static LONG_PTR mu_ex(HWND h) { return U(GetWindowLongPtrA)(h, GWL_EXSTYLE); }

/* ---- worker thread owning one window and a message loop ---- */
typedef struct {
    HANDLE ready, thread;
    DWORD tid;
    HWND hwnd;
    DWORD style, ex;
    const char *title;
    HWND parent;
} mu_worker;
static DWORD WINAPI mu_worker_main(LPVOID p)
{
    mu_worker *w = p;
    MSG m;
    U(PeekMessageA)(&m, NULL, 0, 0, PM_NOREMOVE);
    w->hwnd = mu_mk_cls(MU_CLS, w->title ? w->title : "worker", w->style, w->ex, 20, 20, 200, 150, w->parent);
    SetEvent(w->ready);
    if (!w->hwnd) return 1;
    DWORD end = GetTickCount() + 15000;
    while ((LONG)(end - GetTickCount()) > 0) {
        BOOL r = U(PeekMessageA)(&m, NULL, 0, 0, PM_REMOVE);
        if (r) {
            if (m.message == WM_QUIT) break;
            U(TranslateMessage)(&m); U(DispatchMessageA)(&m);
        } else MsgWaitForMultipleObjects(0, NULL, FALSE, 20, QS_ALLINPUT);
    }
    if (U(IsWindow)(w->hwnd)) U(DestroyWindow)(w->hwnd);
    return 0;
}
static int mu_worker_start(mu_worker *w, DWORD style, DWORD ex, const char *title)
{
    memset(w, 0, sizeof *w);
    w->style = style; w->ex = ex; w->title = title;
    w->ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    w->thread = CreateThread(NULL, 0, mu_worker_main, w, 0, &w->tid);
    if (!w->thread || WaitForSingleObject(w->ready, 5000) != WAIT_OBJECT_0 || !w->hwnd) return 0;
    return 1;
}
static void mu_worker_stop(mu_worker *w)
{
    if (w->thread) {
        PostThreadMessageA(w->tid, WM_QUIT, 0, 0);
        if (WaitForSingleObject(w->thread, 5000) != WAIT_OBJECT_0) TerminateThread(w->thread, 0);
        CloseHandle(w->thread);
    }
    if (w->ready) CloseHandle(w->ready);
    memset(w, 0, sizeof *w);
}

#define BOGUS_HWND ((HWND)(ULONG_PTR)0x7FFF1234)
#endif
