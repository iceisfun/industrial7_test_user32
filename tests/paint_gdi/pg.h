/* helpers local to the paint_gdi area */
#ifndef PG_H
#define PG_H
#include "t.h"
#ifndef DCX_USESTYLE
#define DCX_USESTYLE 0x10000
#endif
#define EQ(a, b) do { ULONG_PTR v_ = (ULONG_PTR)(a); EXPECT_EQ(v_, b); } while (0)

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
    WPARAM ncpaint_wp, paint_wp, erase_wp; int erase_dctype; RECT erase_clip; int syscolor;
    int prt_paint_wp_hdc;
} pg_rec;
extern pg_rec pg_r;

HWND pg_window(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
HWND pg_recwindow(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
void pg_reset(void);
void pg_flush(HWND h);     /* drains paint state so that the update region is empty */
void pg_pump(void);
/* visible topmost no-activate popup with recording proc; client == window rect */
HWND pg_topwin(int x, int y, int w, int h);
HWND pg_topwin_cls(const char *cls, int x, int y, int w, int h, HWND parent, DWORD style);
BOOL pg_visible_probe(HDC dc, int x, int y);
BOOL pg_scaled(void);
void pg_gui(void);
#endif
