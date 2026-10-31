#ifndef GAP0_H
#define GAP0_H
#include "t.h"

#ifndef BROADCAST_QUERY_DENY
#define BROADCAST_QUERY_DENY 0x424D5144
#endif
#ifndef DC_ACTIVE
#define DC_ACTIVE 0x0001
#define DC_SMALLCAP 0x0002
#define DC_ICON 0x0004
#define DC_TEXT 0x0008
#define DC_INBUTTON 0x0010
#define DC_GRADIENT 0x0020
#endif

/* Shared state recorded by the common window procedure. */
extern int g0_bsm_count;
extern UINT g0_bsm_msg;
extern LRESULT g0_deny;
extern int g0_create_count;
extern CREATESTRUCTW g0_last_cs;
extern WCHAR g0_last_cls[64];

const WCHAR *g0_cls(void);
HWND g0_mk(DWORD style, DWORD ex, int x, int y, int w, int h, HWND parent);
void g0_pump_ms(int ms);

/* 32bpp memory surface helper. */
typedef struct { HDC dc; HBITMAP bmp, old; DWORD *bits; int w, h; } G0Surf;
int g0_surf_make(G0Surf *s, int w, int h, DWORD fill);
void g0_surf_free(G0Surf *s);
DWORD g0_px(G0Surf *s, int x, int y);   /* 0x00RRGGBB */
DWORD g0_rgb(COLORREF c);               /* COLORREF -> 0x00RRGGBB */

/* DLGTEMPLATE builder (wide strings, DWORD aligned). Returns size in bytes. */
size_t g0_build_dlg(void *buf, DWORD style, DWORD ex, const WCHAR *title, int nbuttons);
#endif
