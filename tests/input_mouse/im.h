/* input_mouse: shared helpers (state save/restore, logging window, bounded pumping). */
#ifndef IM_H
#define IM_H
#include "t.h"

typedef struct { UINT m; WPARAM w; LPARAM l; HWND h; DWORD tid; } imrec;
#define IM_MAXREC 4096
extern imrec im_log[IM_MAXREC];
extern int im_nlog;

/* Hook called from the window procedure before default handling.
 * Return non-zero in *handled to return the value instead of DefWindowProc. */
typedef LRESULT (*im_hook_t)(HWND, UINT, WPARAM, LPARAM, int *handled);
extern im_hook_t im_hook;

extern int im_aware;                       /* process made per-monitor DPI aware */
const char *im_cls(void);                  /* logging class (CS_DBLCLKS, IDC_CROSS class cursor) */
const char *im_cls_plain(void);            /* logging class without CS_DBLCLKS, arrow cursor */
HWND im_win(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
HWND im_win_cls(const char *cls, DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
/* Visible topmost popup at (x,y) on the primary monitor, not activated by the system for input checks. */
HWND im_popup(int x, int y, int w, int h);
void im_destroy(HWND h);
void im_pump_ms(int ms);                   /* dispatch this thread's messages for ms (bounded) */
void im_pump_until(UINT msg, int ms);      /* pump until a message of this kind was logged */
void im_clear(void);
int im_count(UINT msg);
int im_find(UINT msg, imrec *out);         /* last record of msg; 0 if none */
int im_first(UINT msg, imrec *out);

/* Global state save/restore. */
typedef struct {
    POINT pos; RECT clip; UINT dbl; int swap; HCURSOR cur; int cnt;
} imstate;
void im_save(imstate *s);
void im_restore(imstate *s);

int im_input_ok(void);                     /* cursor moves are observed as WM_MOUSEMOVE in our own window */
int im_swapped(void);
void im_abs_move(int px, int py);          /* SendInput absolute move to pixel */
void im_click_btn(DWORD down, DWORD up, DWORD data);
int im_near(int a, int b, int tol);
void im_setpos(int x, int y);              /* SetCursorPos + pump */

#define IM_TEST_SIZE_ASSERT(t, n) _Static_assert(sizeof(t) == (n), #t)
#endif
