/* Shared helpers for the menus area (static, header-only). */
#ifndef MH_H
#define MH_H
#include "t.h"
#include <wchar.h>

#define REQ_CORE() do { \
    if (!(U(CreateMenu) && U(CreatePopupMenu) && U(DestroyMenu) && U(IsMenu) && \
          U(AppendMenuA) && U(AppendMenuW) && U(InsertMenuA) && U(InsertMenuW) && \
          U(ModifyMenuA) && U(ModifyMenuW) && U(DeleteMenu) && U(RemoveMenu) && \
          U(GetMenuItemCount) && U(GetMenuItemID) && U(GetSubMenu) && \
          U(GetMenuStringA) && U(GetMenuStringW) && U(GetMenuState) && \
          U(CheckMenuItem) && U(EnableMenuItem))) { t_skip("missing core menu export"); return; } } while (0)
#define REQ_INFO() do { \
    if (!(U(CreateMenu) && U(CreatePopupMenu) && U(DestroyMenu) && U(IsMenu) && U(AppendMenuA) && \
          U(InsertMenuItemA) && U(InsertMenuItemW) && U(GetMenuItemInfoA) && U(GetMenuItemInfoW) && \
          U(SetMenuItemInfoA) && U(SetMenuItemInfoW) && U(GetMenuItemCount))) { t_skip("missing iteminfo export"); return; } } while (0)

#define EQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define EMI (UINT)U(EnableMenuItem)
#define M_NEW()    U(CreateMenu)()
#define M_POP()    U(CreatePopupMenu)()
#define M_DESTROY(h) U(DestroyMenu)(h)
#define M_IS(h)    U(IsMenu)(h)
#define M_CNT(h)   U(GetMenuItemCount)(h)

static const MENUITEMINFOA *mh_dummy_unused;

/* Bogus handles that must never be accepted as menus. */
static HMENU const mh_bogus[] = {
    (HMENU)0, (HMENU)1, (HMENU)2, (HMENU)(LONG_PTR)-1, (HMENU)(LONG_PTR)-2, (HMENU)0xDEADBEEF,
    (HMENU)0x10001, (HMENU)0x7FFFFFFF, (HMENU)0x80000000, (HMENU)0xFFFF, (HMENU)0x10000,
    (HMENU)0x12345678, (HMENU)0x00010002, (HMENU)(ULONG_PTR)0xFFFFFFFF00000000ULL,
    (HMENU)(ULONG_PTR)0x8000000000000000ULL, (HMENU)(ULONG_PTR)0x00007FFF00000001ULL,
};
#define MH_NBOGUS (int)(sizeof mh_bogus / sizeof mh_bogus[0])

static int mh_str(HMENU h, UINT id, UINT fl, char *buf, int cch) { return U(GetMenuStringA)(h, id, buf, cch, fl); }

/* Append a string item (A). */
static BOOL mh_add(HMENU h, UINT id, const char *s) { return U(AppendMenuA)(h, MF_STRING, id, s); }
/* Build a menu with N string items "Item0".. with ids 100+i */
static HMENU mh_make(int n) {
    HMENU h = M_POP(); char b[32];
    for (int i = 0; i < n && h; i++) { wsprintfA(b, "Item%d", i); U(AppendMenuA)(h, MF_STRING, 100 + i, b); }
    return h;
}
static UINT mh_state(HMENU h, UINT id, UINT by) { return U(GetMenuState)(h, id, by); }

static HBITMAP mh_bmp(void) { return CreateBitmap(16, 16, 1, 1, NULL); }

/* Fill a MENUITEMINFOA with a sentinel pattern and set cbSize/mask. */
static void mh_init_a(MENUITEMINFOA *mi, UINT mask) {
    memset(mi, 0xCC, sizeof *mi);
    mi->cbSize = sizeof *mi; mi->fMask = mask;
}
static void mh_init_w(MENUITEMINFOW *mi, UINT mask) {
    memset(mi, 0xCC, sizeof *mi);
    mi->cbSize = sizeof *mi; mi->fMask = mask;
}
/* Zeroed insert structure */
static void mh_zero_a(MENUITEMINFOA *mi, UINT mask) { memset(mi, 0, sizeof *mi); mi->cbSize = sizeof *mi; mi->fMask = mask; }
static void mh_zero_w(MENUITEMINFOW *mi, UINT mask) { memset(mi, 0, sizeof *mi); mi->cbSize = sizeof *mi; mi->fMask = mask; }
#endif
