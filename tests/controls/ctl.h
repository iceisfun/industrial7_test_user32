/* Shared helpers for the controls area. Everything resolves through U(). */
#ifndef CTL_H
#define CTL_H
#include "t.h"

#undef EXPECT_EQ
#undef EXPECT_NE
/* single-evaluation versions (arguments often are SendMessage calls) */
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define EXPECT_NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)

#define SA(h, m, w, l) ((LRESULT)U(SendMessageA)((h), (m), (WPARAM)(w), (LPARAM)(l)))
#define SWm(h, m, w, l) ((LRESULT)U(SendMessageW)((h), (m), (WPARAM)(w), (LPARAM)(l)))

#define LOG_MAX 1024
typedef struct {
    UINT msg; WPARAM wp; LPARAM lp;
    DRAWITEMSTRUCT di; MEASUREITEMSTRUCT mi; DELETEITEMSTRUCT de; COMPAREITEMSTRUCT ci;
} c_logent;
extern c_logent c_log[LOG_MAX];
extern int c_nlogent;
extern int c_logging;

static void c_logclear(void) { c_nlogent = 0; }

static LRESULT CALLBACK c_parent_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    int rec = 0, ret_set = 0; LRESULT ret = 0;
    switch (m) {
    case WM_COMMAND: case WM_HSCROLL: case WM_VSCROLL:
    case WM_DRAWITEM: case WM_MEASUREITEM: case WM_DELETEITEM: case WM_COMPAREITEM:
    case WM_VKEYTOITEM: case WM_CHARTOITEM:
        rec = 1; break;
    }
    if (rec && c_logging && c_nlogent < LOG_MAX) {
        c_logent *e = &c_log[c_nlogent++];
        memset(e, 0, sizeof *e);
        e->msg = m; e->wp = w; e->lp = l;
        if (m == WM_DRAWITEM) e->di = *(DRAWITEMSTRUCT *)l;
        else if (m == WM_MEASUREITEM) e->mi = *(MEASUREITEMSTRUCT *)l;
        else if (m == WM_DELETEITEM) e->de = *(DELETEITEMSTRUCT *)l;
        else if (m == WM_COMPAREITEM) e->ci = *(COMPAREITEMSTRUCT *)l;
    }
    if (m == WM_COMPAREITEM) {
        COMPAREITEMSTRUCT *c = (COMPAREITEMSTRUCT *)l;
        return c->itemData1 < c->itemData2 ? -1 : c->itemData1 > c->itemData2 ? 1 : 0;
    }
    if (m == WM_DRAWITEM) return TRUE;
    if (m == WM_VKEYTOITEM || m == WM_CHARTOITEM) return -1;
    (void)ret_set; (void)ret;
    return U(DefWindowProcA)(h, m, w, l);
}

static const char *c_parent_cls(void)
{
    static int done;
    if (!done) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = c_parent_proc;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = "CtlTestParent";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        U(RegisterClassA)(&wc);
        done = 1;
    }
    return "CtlTestParent";
}

/* Hidden top-level parent with a client area of about w x h. */
static HWND c_parent(int w, int h)
{
    return U(CreateWindowExA)(0, c_parent_cls(), "ctlparent", WS_OVERLAPPEDWINDOW,
                              20, 20, w, h, NULL, NULL, GetModuleHandleA(NULL), NULL);
}
/* Same, but shown without activation (for tests that need painting). */
static HWND c_parent_shown(int w, int h)
{
    HWND p = c_parent(w, h);
    if (p) { U(ShowWindow)(p, SW_SHOWNOACTIVATE); U(UpdateWindow)(p); t_pump(); }
    return p;
}

static HWND c_ctl(HWND par, const char *cls, const char *text, DWORD style, int id,
                  int x, int y, int w, int h)
{
    return U(CreateWindowExA)(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                              par, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
}
static HWND c_ctlw(HWND par, const wchar_t *cls, const wchar_t *text, DWORD style, int id,
                   int x, int y, int w, int h)
{
    return U(CreateWindowExW)(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                              par, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
}

static int c_ncmd(int id, int code)
{
    int n = 0;
    for (int i = 0; i < c_nlogent; i++)
        if (c_log[i].msg == WM_COMMAND && LOWORD(c_log[i].wp) == (WORD)id && HIWORD(c_log[i].wp) == (WORD)code) n++;
    return n;
}
static int c_nmsg(UINT m)
{
    int n = 0;
    for (int i = 0; i < c_nlogent; i++) if (c_log[i].msg == m) n++;
    return n;
}
static int c_first_cmd(int id, int code)
{
    for (int i = 0; i < c_nlogent; i++)
        if (c_log[i].msg == WM_COMMAND && LOWORD(c_log[i].wp) == (WORD)id && HIWORD(c_log[i].wp) == (WORD)code) return i;
    return -1;
}

/* Text helpers */
static void c_gettext(HWND h, char *buf, int n) { buf[0] = 0; SA(h, WM_GETTEXT, n, buf); }
static int c_text_is(HWND h, const char *s)
{
    char buf[4096]; c_gettext(h, buf, sizeof buf);
    return !strcmp(buf, s);
}
static int c_lbtext_is(HWND lb, int idx, const char *s)
{
    char buf[512]; buf[0] = 0;
    LRESULT r = SA(lb, LB_GETTEXT, idx, buf);
    return r == (LRESULT)strlen(s) && !strcmp(buf, s);
}
static int c_cbtext_is(HWND cb, int idx, const char *s)
{
    char buf[512]; buf[0] = 0;
    LRESULT r = SA(cb, CB_GETLBTEXT, idx, buf);
    return r == (LRESULT)strlen(s) && !strcmp(buf, s);
}
static void c_getsel(HWND e, int *s, int *en)
{
    DWORD a = 0xdeadbeef, b = 0xdeadbeef;
    SA(e, EM_GETSEL, &a, &b);
    *s = (int)a; *en = (int)b;
}
static int c_sel_is(HWND e, int s, int en)
{
    int a, b; c_getsel(e, &a, &b);
    return a == s && b == en;
}
static void c_classname(HWND h, char *buf, int n) { buf[0] = 0; U(GetClassNameA)(h, buf, n); }
#endif
