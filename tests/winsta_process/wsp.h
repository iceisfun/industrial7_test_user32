/* helpers private to the winsta_process area */
#ifndef WSP_H
#define WSP_H
#include "t.h"

#define WSP_WINSTA_ALL 0x37F
#define WSP_DESKTOP_ALL 0x1FF
#define WSP_HERR_INVALID_HOOK_HANDLE 1404
#define WSP_HERR_INVALID_HOOK_FILTER 1171
#define WSP_HERR_INVALID_FILTER_PROC 1200
#define WSP_HERR_HOOK_NEEDS_HMOD 1428

typedef struct { BOOL fInherit; BOOL fReserved; DWORD dwFlags; } WSP_UOF;

/* unique object name per call */
static inline void wsp_name(char *buf, size_t n, const char *tag) {
    static volatile LONG ctr;
    snprintf(buf, n, "t32wsp_%s_%lu_%ld_%lu", tag, (unsigned long)GetCurrentProcessId(),
             (long)InterlockedIncrement(&ctr), (unsigned long)(GetTickCount() & 0xffff));
}
static inline void wsp_namew(WCHAR *buf, size_t n, const char *tag) {
    char a[96]; size_t i; wsp_name(a, sizeof a, tag);
    for (i = 0; i < n - 1 && a[i]; i++) buf[i] = (WCHAR)a[i];
    buf[i] = 0;
}

/* run fn(arg) on a new thread; returns 1 if it finished within ms */
typedef struct { DWORD (*fn)(void *); void *arg; DWORD ret; DWORD tid; } wsp_job;
static inline DWORD WINAPI wsp_thunk(LPVOID p) { wsp_job *j = p; j->tid = GetCurrentThreadId(); j->ret = j->fn(j->arg); return j->ret; }
static inline int wsp_run(DWORD (*fn)(void *), void *arg, DWORD ms, DWORD *ret) {
    wsp_job j = { fn, arg, 0xdeadbeef, 0 };
    HANDLE h = CreateThread(NULL, 0, wsp_thunk, &j, 0, NULL);
    DWORD w;
    if (!h) return 0;
    w = WaitForSingleObject(h, ms);
    CloseHandle(h);
    if (w != WAIT_OBJECT_0) return 0;
    if (ret) *ret = j.ret;
    return 1;
}

/* pump messages for up to ms or until *flag becomes nonzero */
static inline void wsp_pump_until(volatile LONG *flag, DWORD ms) {
    DWORD t0 = GetTickCount();
    for (;;) {
        t_pump();
        if (flag && *flag) return;
        if (GetTickCount() - t0 >= ms) return;
        Sleep(5);
    }
}
#endif
