#include "mu.h"
mu_rec mu_log[MU_MAXLOG];
int mu_nlog;
int mu_logging;
mu_hook_fn mu_hook;

LRESULT CALLBACK mu_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (mu_logging && mu_nlog < MU_MAXLOG) {
        mu_log[mu_nlog].h = h; mu_log[mu_nlog].msg = m; mu_log[mu_nlog].w = w; mu_log[mu_nlog].l = l; mu_nlog++;
    }
    if (mu_hook) { LRESULT r = 0; if (mu_hook(h, m, w, l, &r)) return r; }
    return DefWindowProcA(h, m, w, l);
}

int mu_register(void)
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
    SetLastError(0);
    if (!rc(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 0;
    wc.hbrBackground = NULL; wc.lpszClassName = MU_CLS_NB;
    SetLastError(0);
    if (!rc(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 0;
    done = 1;
    return 1;
}
