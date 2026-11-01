/* Shared helpers for the dde_hooks_accel area. */
#ifndef DH_H
#define DH_H
#include "t.h"
#include <ddeml.h>
#include <dde.h>

/* user32 is never linked: route every direct call through GetProcAddress. */
#define DefWindowProcA (*(__typeof__(&DefWindowProcA))t_proc("DefWindowProcA"))
#define DispatchMessageA (*(__typeof__(&DispatchMessageA))t_proc("DispatchMessageA"))
#define EnableWindow (*(__typeof__(&EnableWindow))t_proc("EnableWindow"))
#define EndDialog (*(__typeof__(&EndDialog))t_proc("EndDialog"))
#define EndMenu (*(__typeof__(&EndMenu))t_proc("EndMenu"))
#define FindWindowA (*(__typeof__(&FindWindowA))t_proc("FindWindowA"))
#define GetActiveWindow (*(__typeof__(&GetActiveWindow))t_proc("GetActiveWindow"))
#define GetFocus (*(__typeof__(&GetFocus))t_proc("GetFocus"))
#define GetKeyState (*(__typeof__(&GetKeyState))t_proc("GetKeyState"))
#define GetWindowRect (*(__typeof__(&GetWindowRect))t_proc("GetWindowRect"))
#define IsIconic (*(__typeof__(&IsIconic))t_proc("IsIconic"))
#define IsWindow (*(__typeof__(&IsWindow))t_proc("IsWindow"))
#define IsZoomed (*(__typeof__(&IsZoomed))t_proc("IsZoomed"))
#define KillTimer (*(__typeof__(&KillTimer))t_proc("KillTimer"))
#define MoveWindow (*(__typeof__(&MoveWindow))t_proc("MoveWindow"))
#define MsgWaitForMultipleObjects (*(__typeof__(&MsgWaitForMultipleObjects))t_proc("MsgWaitForMultipleObjects"))
#define PeekMessageA (*(__typeof__(&PeekMessageA))t_proc("PeekMessageA"))
#define PeekMessageW (*(__typeof__(&PeekMessageW))t_proc("PeekMessageW"))
#define PostMessageA (*(__typeof__(&PostMessageA))t_proc("PostMessageA"))
#define PostMessageW (*(__typeof__(&PostMessageW))t_proc("PostMessageW"))
#define PostQuitMessage (*(__typeof__(&PostQuitMessage))t_proc("PostQuitMessage"))
#define PostThreadMessageA (*(__typeof__(&PostThreadMessageA))t_proc("PostThreadMessageA"))
#define SendMessageA (*(__typeof__(&SendMessageA))t_proc("SendMessageA"))
#define SendMessageW (*(__typeof__(&SendMessageW))t_proc("SendMessageW"))
#define SetActiveWindow (*(__typeof__(&SetActiveWindow))t_proc("SetActiveWindow"))
#define SetFocus (*(__typeof__(&SetFocus))t_proc("SetFocus"))
#define SetForegroundWindow (*(__typeof__(&SetForegroundWindow))t_proc("SetForegroundWindow"))
#define SetTimer (*(__typeof__(&SetTimer))t_proc("SetTimer"))
#define SetWindowPos (*(__typeof__(&SetWindowPos))t_proc("SetWindowPos"))
#define TranslateMessage (*(__typeof__(&TranslateMessage))t_proc("TranslateMessage"))
#define UnregisterClassA (*(__typeof__(&UnregisterClassA))t_proc("UnregisterClassA"))
#define UnregisterClassW (*(__typeof__(&UnregisterClassW))t_proc("UnregisterClassW"))
#define CreatePopupMenu (*(__typeof__(&CreatePopupMenu))t_proc("CreatePopupMenu"))
#define AppendMenuA (*(__typeof__(&AppendMenuA))t_proc("AppendMenuA"))
#define DestroyMenu (*(__typeof__(&DestroyMenu))t_proc("DestroyMenu"))
#define TrackPopupMenu (*(__typeof__(&TrackPopupMenu))t_proc("TrackPopupMenu"))
#define MessageBoxA (*(__typeof__(&MessageBoxA))t_proc("MessageBoxA"))
#define DialogBoxIndirectParamA (*(__typeof__(&DialogBoxIndirectParamA))t_proc("DialogBoxIndirectParamA"))
#define RegisterClassW (*(__typeof__(&RegisterClassW))t_proc("RegisterClassW"))

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
/* dhchild.c: run a named test of this exe in a child process (output replayed, hard timeout) */
int dh_run_child(const char *test, DWORD timeout_ms, DWORD *exit_code);
#endif
