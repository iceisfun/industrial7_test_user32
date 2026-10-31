/* Shared helpers for the window_class area. */
#ifndef WC_H
#define WC_H
#include "t.h"

#define ERR_INVALID_INDEX     1413
#define ERR_INVALID_HWND      1400
#define ERR_CLASS_EXISTS      1410
#define ERR_CLASS_NOT_EXIST   1411
#define ERR_CLASS_HAS_WINDOWS 1412
#define ERR_INVALID_PARAM     87
#define MAGIC_ERR             0xDEAD0001u
#ifndef GCL_HMODULE
#define GCL_HMODULE (-16)
#define GCL_HBRBACKGROUND (-10)
#define GCL_HCURSOR (-12)
#define GCL_HICON (-14)
#define GCL_HICONSM (-34)
#endif

static inline HINSTANCE wc_inst(void) { return GetModuleHandleA(NULL); }

/* Unique class/prop names, never colliding across runs of the same exe. */
const char *wc_uname(char *buf, size_t n, const char *base);

LRESULT CALLBACK wc_defA(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK wc_defW(HWND, UINT, WPARAM, LPARAM);

/* Register an ANSI class with the given parameters. */
ATOM wc_regA(const char *name, UINT style, int cbCls, int cbWnd, WNDPROC proc);
ATOM wc_regW(const WCHAR *name, UINT style, int cbCls, int cbWnd, WNDPROC proc);
HWND wc_createA(const char *cls, DWORD style, HWND parent);
HWND wc_createW(const WCHAR *cls, DWORD style, HWND parent);
void wc_widen(WCHAR *dst, const char *src);

/* Record of the last message seen by the recording procs. */
struct wc_rec {
    int   count;
    UINT  msg;
    WPARAM wp;
    LPARAM lp;
    int   unicode;       /* 1 if the W proc saw it, 0 if A proc */
    char  textA[300];
    WCHAR textW[300];
    int   created;
    int   nccreate;
};
extern struct wc_rec wc_rec;
LRESULT CALLBACK wc_recA(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK wc_recW(HWND, UINT, WPARAM, LPARAM);

/* single-evaluation equality check */
#define EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define EXPECT_LAST_ERR(e) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(e), __FILE__, __LINE__, "GetLastError()==" #e, "got %lu want %lu", e_, (DWORD)(e)); } while (0)
int wc_is_wine(void);
#endif
