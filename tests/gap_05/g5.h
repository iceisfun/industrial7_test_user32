#ifndef G5_H
#define G5_H
#include "../../common/t.h"

#define ERR_NOT_SUPPORTED 50
#define ERR_INVALID_PARAMETER 87
#define ERR_ACCESS_DENIED 5
#define ERR_INVALID_HOOK_HANDLE 1404
#define ERR_INVALID_HOOK_FILTER 1171
#define ERR_HOOK_NEEDS_HMOD 1428

/* A window handle that is certainly not valid. */
#define BOGUS_HWND ((HWND)(ULONG_PTR)0x12344)

/* Run fn(arg) on a fresh thread; returns 1 if it finished within 10s. */
static inline DWORD WINAPI g5_thunk(LPVOID p) { void **a = p; ((void (*)(void *))a[0])(a[1]); return 0; }
static inline int g5_run_thread(void (*fn)(void *), void *arg) {
    void *a[2] = { (void *)fn, arg };
    HANDLE h = CreateThread(NULL, 0, g5_thunk, a, 0, NULL);
    if (!h) return 0;
    DWORD r = WaitForSingleObject(h, 10000);
    CloseHandle(h);
    return r == WAIT_OBJECT_0;
}

/* A hidden-by-default top level test window. Returns NULL if creation fails. */
static inline HWND g5_top(DWORD style, int show) {
    HWND h = t_make_window(NULL, style, 0, 20, 20, 300, 200, NULL);
    if (h && show) U(ShowWindow)(h, SW_SHOWNOACTIVATE);
    return h;
}
static inline void g5_destroy(HWND h) { if (h && U(IsWindow)(h)) U(DestroyWindow)(h); }
#endif
