/* Shared helpers for the dde_hooks_accel area. */
#ifndef DH_H
#define DH_H
#include "t.h"
#include <ddeml.h>
#include <dde.h>

#ifndef DMLERR_NO_ERROR
#define DMLERR_NO_ERROR 0
#endif
#define E_DMLERR_INVALIDPARAMETER 0x4006
#define E_DMLERR_NO_CONV_ESTABLISHED 0x400A
#define E_DMLERR_DLL_USAGE 0x4004

#ifndef TIMERV_DEFAULT_COALESCING
#define TIMERV_DEFAULT_COALESCING 0
#endif
#ifndef TIMERV_NO_COALESCING
#define TIMERV_NO_COALESCING 0xFFFFFFFFu
#endif
#ifndef TIMERV_MAX_COALESCING
#define TIMERV_MAX_COALESCING 0x7FFFFFF5u
#endif
#ifndef USER_TIMER_MINIMUM
#define USER_TIMER_MINIMUM 0x0000000A
#endif
#ifndef USER_TIMER_MAXIMUM
#define USER_TIMER_MAXIMUM 0x7FFFFFFF
#endif
#ifndef FLASTKEY
#define FLASTKEY 0x80
#endif

/* Single-evaluation equality helpers (arguments may have side effects). */
#define EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)

static int dh_is_wine(void) {
    HMODULE n = GetModuleHandleA("ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version") != NULL;
}

/* A DDE callback that accepts nothing and answers nothing. */
static HDDEDATA CALLBACK dh_null_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA d, ULONG_PTR d1, ULONG_PTR d2) {
    return NULL;
}

/* Pump messages for up to ms milliseconds or until *flag becomes nonzero. */
static int dh_pump_until(volatile LONG *flag, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    while (!*flag && (LONG)(GetTickCount() - end) < 0) {
        MSG m;
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        if (*flag) break;
        MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
    }
    return *flag != 0;
}
static void dh_sleep_pump(DWORD ms) {
    volatile LONG never = 0;
    dh_pump_until(&never, ms);
}

/* Window class used by tests that need their own wndproc. */
typedef LRESULT (CALLBACK *dh_wndproc)(HWND, UINT, WPARAM, LPARAM);
static HWND dh_create_window(const char *cls, dh_wndproc proc, DWORD style, DWORD ex, HWND parent) {
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = cls;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    U(RegisterClassA)(&wc);
    return U(CreateWindowExA)(ex, cls, "dh", style, 20, 20, 160, 120, parent, NULL, GetModuleHandleA(NULL), NULL);
}
static void dh_destroy_window(HWND h, const char *cls) {
    if (h) U(DestroyWindow)(h);
    if (cls) U(UnregisterClassA)(cls, GetModuleHandleA(NULL));
}
#endif
