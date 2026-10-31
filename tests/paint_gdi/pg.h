/* helpers local to the paint_gdi area */
#ifndef PG_H
#define PG_H
#include "t.h"

#define PG_R(l,t,r,b) ((RECT){(l),(t),(r),(b)})
#define RECT_STR(r) (r).left,(r).top,(r).right,(r).bottom
#define CHECK_RECT(r, l, t, rr, b) \
    CHECKF((r).left == (l) && (r).top == (t) && (r).right == (rr) && (r).bottom == (b), \
           "got (%ld,%ld,%ld,%ld) want (%d,%d,%d,%d)", (r).left, (r).top, (r).right, (r).bottom, (int)(l), (int)(t), (int)(rr), (int)(b))

/* Window with a recording window procedure. */
typedef struct pg_rec {
    int paint, erase, ncpaint, nccalc;
    int order[64]; int norder;
    RECT upd_at_paint; RECT ps_rc; BOOL ps_erase;
    int do_paint;   /* 1: BeginPaint/EndPaint in WM_PAINT; 0: leave to DefWindowProc; 2: ignore */
    int erase_ret;
    int in_wm_paint_rgn_type;
} pg_rec;
extern pg_rec pg_r;

HWND pg_window(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
HWND pg_recwindow(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
void pg_reset(void);
void pg_flush(HWND h);     /* drains paint state so that the update region is empty */
void pg_pump(void);
#endif
