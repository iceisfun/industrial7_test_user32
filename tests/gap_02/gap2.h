/* Shared helpers for the gap_02 area. */
#ifndef GAP2_H
#define GAP2_H
#include "t.h"

/* Observed-only comparison: logged, never fails. */
#define NOTE_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    if (a_ != b_) t_note("%s:%d: %s got 0x%llx, expected 0x%llx", __FILE__, __LINE__, #a, \
        (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define NOTE_ERR(want) do { DWORD e_ = GetLastError(); \
    if (e_ != (DWORD)(want)) t_note("%s:%d: GetLastError got %lu, expected %lu", __FILE__, __LINE__, e_, (DWORD)(want)); } while (0)

/* Window handles that can never name a live window. */
static const struct { HWND h; const char *n; } g2_bogus[] = {
    { (HWND)0, "NULL" }, { (HWND)1, "1" }, { (HWND)2, "2" }, { (HWND)0x10, "0x10" },
    { (HWND)0x1234, "0x1234" }, { (HWND)0xdeadbeef, "0xdeadbeef" }, { (HWND)(LONG_PTR)-1, "-1" },
    { (HWND)(LONG_PTR)-2, "-2" }, { (HWND)(LONG_PTR)-3, "-3" }, { (HWND)0xffff, "0xffff" },
    { (HWND)0x7fffffff, "0x7fffffff" }, { (HWND)0x00ffff00, "0x00ffff00" },
};
#define G2_NBOGUS ((int)(sizeof g2_bogus / sizeof g2_bogus[0]))
/* Same list without NULL (a few APIs treat NULL specially). */
#define G2_FIRST_NONNULL 1

static HWND g2_mkwnd(DWORD style, DWORD ex) {
    return t_make_window(t_cls(), style, ex, 20, 20, 200, 150, NULL);
}
static void g2_destroy(HWND h) {
    BOOL (WINAPI *d)(HWND) = (void *)t_proc("DestroyWindow");
    if (d && h) d(h);
}
/* A handle that was valid and has been destroyed. */
static HWND g2_dead_hwnd(void) {
    HWND h = g2_mkwnd(WS_OVERLAPPEDWINDOW, 0);
    g2_destroy(h);
    return h;
}

/* Run fn(arg) on a fresh thread, wait at most 5 s. Returns thread exit code or 0xdead0001 on timeout. */
typedef DWORD (WINAPI *g2_thread_fn)(void *);
static DWORD g2_run_thread(g2_thread_fn fn, void *arg) {
    DWORD code = 0xdead0001;
    HANDLE th = CreateThread(NULL, 0, fn, arg, 0, NULL);
    if (!th) return 0xdead0002;
    if (WaitForSingleObject(th, 5000) == WAIT_OBJECT_0) GetExitCodeThread(th, &code);
    CloseHandle(th);
    return code;
}
#endif
