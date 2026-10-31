/* window_mgmt shared helpers */
#ifndef WM_H
#define WM_H
#include "t.h"

#define WM_TIMEOUT_MS 5000

UDECL(BOOL, IsTopLevelWindow, (HWND));
UDECL(BOOL, IsWindowArranged, (HWND));

/* Test window class: logs messages, can be told to fail creation. */
const char *wm_cls(void);
const WCHAR *wm_clsw(void);
extern int wm_fail_nccreate;   /* WM_NCCREATE returns FALSE */
extern int wm_fail_create;     /* WM_CREATE returns -1 */
extern int wm_minmax_on;       /* apply wm_minmax below on WM_GETMINMAXINFO */
extern MINMAXINFO wm_minmax, wm_mmi_seen;
extern int wm_mmi_count;
extern CREATESTRUCTA wm_cs_nc, wm_cs_cr;
extern char wm_name_nc[128], wm_name_cr[128];
extern int wm_seen_nc, wm_seen_cr;

HWND wm_mk(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
HWND wm_mkw(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
/* visible-less popup helper (hidden, small, never activates) */
HWND wm_hidden(void);
void wm_destroy(HWND h);

void wm_logclear(void);
int  wm_logcount(UINT msg);
int  wm_lognth(int n, UINT *msg, WPARAM *w, LPARAM *l);
int  wm_logtotal(void);
UINT wm_logwpflags(int n);
int  wm_logwpsize(int n, int *cx, int *cy);
extern int wm_wpc_mode, wm_ncc_mode;
HWND wm_loghwnd(int n);
int  wm_logcount_h(UINT msg, HWND h);
int  wm_logfind_h(UINT msg, HWND h, int from);
/* index of first occurrence of msg at or after 'from' or -1 */
int  wm_logfind(UINT msg, int from);

/* true when a is above b in the top-level z-order chain */
int wm_isabove(HWND a, HWND b);

/* Worker thread owning one window and a message pump. */
typedef struct wm_worker {
    HANDLE th, stop, ready;
    DWORD tid;
    HWND hwnd;
    DWORD style, ex;
    HWND parent;
    int show;
} wm_worker;
int  wm_worker_start(wm_worker *w, DWORD style, DWORD ex, int show);
void wm_worker_stop(wm_worker *w);

#define EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define NEED(x) REQUIRE(U(x))
#define ERR_INVALID_HWND 1400

#endif
