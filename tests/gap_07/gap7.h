/* Shared helpers for the gap_07 area. */
#ifndef GAP7_H
#define GAP7_H
#include "t.h"

#define G7_LOGMAX 256
typedef struct { HWND h; UINT m; WPARAM w; LPARAM l; } g7_ev;
extern g7_ev g7_log[G7_LOGMAX];
extern int g7_nlog;
extern UINT g7_watch_lo, g7_watch_hi;       /* messages in [lo,hi] are logged */
extern LRESULT g7_ret;                      /* value the logging proc returns for logged msgs */

/* Register (once) a wide / ansi class whose proc logs messages in [watch_lo,watch_hi]. */
const WCHAR *g7_clsW(void);
const char  *g7_clsA(void);
HWND g7_mkwinW(DWORD style, DWORD ex, HWND parent);   /* hidden top-level by default; window text L"g7" */
HWND g7_mkwinA(DWORD style, DWORD ex, HWND parent);
void g7_reset(UINT lo, UINT hi, LRESULT ret);
int  g7_count(HWND h, UINT m);
void g7_pump_ms(int ms);                    /* pump messages for up to ms (bounded) */
void g7_drain(void);                        /* drop queued system messages for this thread */
int g7_is_wine(void);                       /* the Wine stubs for some undocumented exports abort the process */
/* Run fn on a new thread with its own queue; returns thread id via *tid. */
#endif
