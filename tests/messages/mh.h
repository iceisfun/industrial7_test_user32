/* Shared helpers for the messages area (header-only, all static). */
#ifndef MH_H
#define MH_H
#include "t.h"

#define SMA   U(SendMessageA)
#define SMW   U(SendMessageW)
#define PMA   U(PostMessageA)
#define PMW   U(PostMessageW)
#define PKA   U(PeekMessageA)
#define PKW   U(PeekMessageW)
#define GMA   U(GetMessageA)
#define GMW   U(GetMessageW)
#define DWPA  U(DefWindowProcA)
#define DWPW  U(DefWindowProcW)
#define CWEA  U(CreateWindowExA)
#define CWEW  U(CreateWindowExW)
#define DESTROYW U(DestroyWindow)
#define ISWND U(IsWindow)

#ifndef ERROR_MESSAGE_SYNC_ONLY
#define ERROR_MESSAGE_SYNC_ONLY 1159
#endif
#define E_BADFLAGS   1004
#define E_BADMSG_WT  0x3ea
#define E_BADTHREAD  1444
#define E_QUOTA      1816

#define WM_REC_A (WM_USER + 1)   /* echo: returns wParam + lParam */
#define WM_REC_B (WM_USER + 2)   /* ReplyMessage test */
#define WM_REC_C (WM_USER + 3)   /* returns thread id */
#define WM_REC_D (WM_USER + 4)   /* bounce back to g_mainwnd */
#define WM_REC_E (WM_USER + 5)   /* handled on main thread */
#define WM_REC_F (WM_USER + 6)   /* records InSendMessageEx */

typedef struct { HWND h; UINT m; WPARAM w; LPARAM l; DWORD tid; BYTE raw[96]; DWORD ins; } MH_EV;
#define MH_MAXEV 4096
#ifdef MH_IMPL
#define MHV
#else
#define MHV extern
#endif
MHV MH_EV mh_ev[MH_MAXEV];
MHV volatile LONG mh_nev;
MHV int mh_rec;                                   /* recording on/off */
typedef LRESULT (*MH_HOOK)(HWND, UINT, WPARAM, LPARAM, int *handled);
MHV MH_HOOK mh_hook;
MHV volatile LONG mh_flag[8];
MHV volatile LONG mh_ins_a, mh_ins_b;
MHV HWND mh_mainwnd;
MHV WNDPROC mh_pA, mh_pW;   /* the procedures the test classes were actually registered with */

static int mh_interesting(UINT m) {
    switch (m) {
    case WM_NCCREATE: case WM_CREATE: case WM_DESTROY: case WM_NCDESTROY: case WM_SIZE: case WM_MOVE:
    case WM_SHOWWINDOW: case WM_WINDOWPOSCHANGING: case WM_WINDOWPOSCHANGED: case WM_GETMINMAXINFO:
    case WM_NCCALCSIZE: case WM_ACTIVATE: case WM_ACTIVATEAPP: case WM_NCACTIVATE: case WM_SETFOCUS:
    case WM_KILLFOCUS: case WM_ENABLE: case WM_CLOSE: case WM_QUIT: case WM_SETTEXT: case WM_GETTEXT:
    case WM_GETTEXTLENGTH: case WM_SETFONT: case WM_GETFONT: case WM_SETICON: case WM_COPYDATA:
    case WM_NULL: case WM_PARENTNOTIFY: case WM_CHILDACTIVATE: case WM_QUERYENDSESSION:
    case WM_STYLECHANGING: case WM_STYLECHANGED: case WM_SETREDRAW: case WM_CAPTURECHANGED:
    case WM_TIMER: case WM_MOUSEACTIVATE: case WM_CHAR:
        return 1;
    }
    return m >= WM_USER;
}
static void mh_rec_event(HWND h, UINT m, WPARAM w, LPARAM l, int wide) {
    LONG i;
    if (!mh_rec || !mh_interesting(m)) return;
    i = InterlockedIncrement(&mh_nev) - 1;
    if (i >= MH_MAXEV) { InterlockedDecrement(&mh_nev); return; }
    mh_ev[i].h = h; mh_ev[i].m = m; mh_ev[i].w = w; mh_ev[i].l = l; mh_ev[i].tid = GetCurrentThreadId();
    mh_ev[i].ins = U(InSendMessageEx)(NULL);
    memset(mh_ev[i].raw, 0, sizeof mh_ev[i].raw);
    if (m == WM_SETTEXT && l) {
        size_t n = wide ? (lstrlenW((LPCWSTR)l) + 1) * 2 : (size_t)lstrlenA((LPCSTR)l) + 1;
        if (n > sizeof mh_ev[i].raw) n = sizeof mh_ev[i].raw;
        memcpy(mh_ev[i].raw, (void *)l, n);
    }
}
static void mh_clear(void) { mh_nev = 0; }
static int mh_count(UINT m) { int i, c = 0; for (i = 0; i < mh_nev; i++) if (mh_ev[i].m == m) c++; return c; }
static int mh_find(UINT m, int from) { int i; for (i = from; i < mh_nev; i++) if (mh_ev[i].m == m) return i; return -1; }
static void mh_dump(void) { int i; for (i = 0; i < mh_nev; i++) printf("    ev[%d] %p %04x %llx %llx\n", i, mh_ev[i].h, mh_ev[i].m, (unsigned long long)mh_ev[i].w, (unsigned long long)mh_ev[i].l); }

/* common ANSI/Unicode window procedures */
static LRESULT mh_common(HWND h, UINT m, WPARAM w, LPARAM l, int wide, int *handled) {
    *handled = 1;
    switch (m) {
    case WM_REC_A: return (LRESULT)(w + l);
    case WM_REC_C: return (LRESULT)GetCurrentThreadId();
    case WM_REC_F: mh_ins_a = (LONG)U(InSendMessageEx)(NULL); return U(InSendMessage)() ? 1 : 0;
    case WM_REC_B:
        mh_ins_a = (LONG)U(InSendMessageEx)(NULL);
        U(ReplyMessage)(0x55);
        mh_ins_b = (LONG)U(InSendMessageEx)(NULL);
        mh_flag[2] = U(ReplyMessage)(0x99) ? 1 : 2;
        Sleep(30);
        mh_flag[0] = 1;
        return 0x66;
    case WM_REC_D:
        if (mh_mainwnd) {
            DWORD_PTR r = 0;
            U(SendMessageTimeoutA)(mh_mainwnd, WM_REC_E, 7, 9, SMTO_NORMAL, 4000, &r);
            return (LRESULT)r;
        }
        return -1;
    case WM_USER + 7: Sleep((DWORD)w); return 0x77;
    case WM_USER + 8: return w ? 1 + SMA(h, m, w - 1, 0) : 0;
    case WM_USER + 10: lstrcpyA((LPSTR)l, "from-worker"); return 11;
    case WM_USER + 9: return (LRESULT)0x1122334455667788ull;
    case WM_REC_E: mh_flag[1] = 1; return (LRESULT)(w * 100 + l);
    }
    *handled = 0;
    return 0;
}
static LRESULT CALLBACK mh_procA(HWND h, UINT m, WPARAM w, LPARAM l) {
    int hd; LRESULT r;
    mh_rec_event(h, m, w, l, 0);
    r = mh_common(h, m, w, l, 0, &hd);
    if (hd) return r;
    if (mh_hook) { r = mh_hook(h, m, w, l, &hd); if (hd) return r; }
    return DWPA(h, m, w, l);
}
static LRESULT CALLBACK mh_procW(HWND h, UINT m, WPARAM w, LPARAM l) {
    int hd; LRESULT r;
    mh_rec_event(h, m, w, l, 1);
    r = mh_common(h, m, w, l, 1, &hd);
    if (hd) return r;
    if (mh_hook) { r = mh_hook(h, m, w, l, &hd); if (hd) return r; }
    return DWPW(h, m, w, l);
}
#define MH_CLSA "MsgTestA"
#define MH_CLSW L"MsgTestW"
static int mh_init(void) {
    static int done, ok;
    if (done) return ok;
    done = 1;
    if (!U(RegisterClassExA) || !U(RegisterClassExW) || !CWEA || !CWEW || !SMA || !SMW || !PMA || !PKA || !DWPA || !DWPW) return 0;
    {
        WNDCLASSEXA a; WNDCLASSEXW w;
        if (!mh_pA) { mh_pA = mh_procA; mh_pW = mh_procW; }
        memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpfnWndProc = mh_procA; a.hInstance = GetModuleHandleA(NULL); a.lpszClassName = MH_CLSA;
        memset(&w, 0, sizeof w); w.cbSize = sizeof w; w.lpfnWndProc = mh_procW; w.hInstance = GetModuleHandleA(NULL); w.lpszClassName = MH_CLSW;
        ok = (U(RegisterClassExA)(&a) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) &&
             (U(RegisterClassExW)(&w) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
    }
    return ok;
}
/* single-evaluation versions (the harness macros evaluate their arguments twice) */
#undef EXPECT_EQ
#undef EXPECT_NE
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define EXPECT_NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)
#define REQUIRE_MH() do { if (!mh_init()) { t_skip("cannot register test classes"); return; } } while (0)

static HWND mh_mk(DWORD st, DWORD ex, HWND parent) {
    return CWEA(ex, MH_CLSA, "mh", st, 10, 10, 120, 80, parent, NULL, GetModuleHandleA(NULL), NULL);
}
static HWND mh_mkw(DWORD st, DWORD ex, HWND parent) {
    return CWEW(ex, MH_CLSW, L"mh", st, 10, 10, 120, 80, parent, NULL, GetModuleHandleA(NULL), NULL);
}
static void mh_kill(HWND h) { if (h && ISWND && ISWND(h)) DESTROYW(h); }
/* drain every message of this thread (including WM_QUIT) */
static int mh_drain(void) {
    MSG m; int n = 0;
    while (n < 20000 && PKA(&m, NULL, 0, 0, PM_REMOVE)) { n++; if (m.message != WM_QUIT) { U(TranslateMessage)(&m); U(DispatchMessageA)(&m); } }
    return n;
}
static void mh_reset(void) { mh_hook = NULL; mh_rec = 0; mh_nev = 0; mh_drain(); }

/* worker thread with its own queue and one window */
typedef struct {
    HANDLE ready, stop, th;
    HWND hwnd; DWORD tid; int wide; DWORD style; HWND parent;
} MH_TH;
static DWORD WINAPI mh_thproc(LPVOID p) {
    MH_TH *t = (MH_TH *)p;
    MSG m; DWORD t0 = GetTickCount();
    PKA(&m, NULL, 0, 0, PM_NOREMOVE);
    t->hwnd = t->wide ? mh_mkw(t->style, 0, t->parent) : mh_mk(t->style, 0, t->parent);
    SetEvent(t->ready);
    for (;;) {
        DWORD r = U(MsgWaitForMultipleObjects)(1, &t->stop, FALSE, 500, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) break;
        while (PKA(&m, NULL, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) goto out;
            U(TranslateMessage)(&m); U(DispatchMessageA)(&m);
        }
        if (GetTickCount() - t0 > 30000) break;
    }
out:
    if (t->hwnd && ISWND(t->hwnd)) DESTROYW(t->hwnd);
    return 0;
}
static int mh_th_start(MH_TH *t, int wide, DWORD style) {
    memset(t, 0, sizeof *t);
    t->wide = wide; t->style = style;
    t->ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    t->stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    t->th = CreateThread(NULL, 0, mh_thproc, t, 0, &t->tid);
    if (!t->th) return 0;
    if (WaitForSingleObject(t->ready, 5000) != WAIT_OBJECT_0) return 0;
    return t->hwnd != NULL;
}
static void mh_th_stop(MH_TH *t) {
    SetEvent(t->stop);
    if (t->th) { WaitForSingleObject(t->th, 5000); CloseHandle(t->th); }
    CloseHandle(t->ready); CloseHandle(t->stop);
    memset(t, 0, sizeof *t);
}
/* bounded cross-thread send */
static int mh_xsend(HWND h, UINT m, WPARAM w, LPARAM l, DWORD_PTR *res) {
    DWORD_PTR r = 0;
    LRESULT ok = U(SendMessageTimeoutA)(h, m, w, l, SMTO_NORMAL, 5000, &r);
    if (res) *res = r;
    return ok != 0;
}
/* pump until predicate flag set or timeout (ms) */
static int mh_pump_until(volatile LONG *flag, DWORD ms) {
    DWORD t0 = GetTickCount();
    while (!*flag && GetTickCount() - t0 < ms) {
        MSG m;
        while (PKA(&m, NULL, 0, 0, PM_REMOVE)) { if (m.message == WM_QUIT) break; U(TranslateMessage)(&m); U(DispatchMessageA)(&m); }
        if (!*flag) Sleep(2);
    }
    return *flag != 0;
}
#endif
