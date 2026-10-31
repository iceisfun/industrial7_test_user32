/* Helpers: a recording window class, foreground acquisition, key injection. */
#include "kbd.h"

struct klog g_klog[KLOG_MAX];
int g_nklog;
static unsigned char g_held[256];
static const char *g_kcls = "T32KbdWnd";
static int g_cls_done;
static int g_probe_state; /* 0 unknown, 1 ok, -1 failed */
static HWND g_guard;

static int is_logged(UINT m)
{
    return (m >= WM_KEYFIRST && m <= WM_UNICHAR) || m == WM_HOTKEY || m == WM_COMMAND || m == WM_SYSCOMMAND;
}

static LRESULT CALLBACK kproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (is_logged(m)) {
        if (g_nklog < KLOG_MAX) {
            INPUT_MESSAGE_SOURCE s = {0};
            struct klog *e = &g_klog[g_nklog++];
            BOOL (WINAPI *gs)(INPUT_MESSAGE_SOURCE *) = (void *)t_proc("GetCurrentInputMessageSource");
            if (gs) gs(&s);
            e->msg = m; e->wp = w; e->lp = l; e->dev = s.deviceType; e->org = s.originId;
            e->tick = GetTickCount();
            e->extra = U(GetMessageExtraInfo) ? (LPARAM)U(GetMessageExtraInfo)() : 0;
        }
        /* Deliberately no DefWindowProc for key traffic: avoids menu-mode and
         * system-key side effects. */
        if (m != WM_SYSCOMMAND) return 0;
    }
    return U(DefWindowProcA)(h, m, w, l);
}

static void ensure_cls(void)
{
    WNDCLASSA wc;
    if (g_cls_done) return;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = kproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = g_kcls;
    U(RegisterClassA)(&wc);
    g_cls_done = 1;
}

int kbd_is_us(void)
{
    char n[16] = "";
    BOOL (WINAPI *f)(LPSTR) = (void *)t_proc("GetKeyboardLayoutNameA");
    if (!f || !f(n)) return 0;
    return !lstrcmpiA(n, "00000409");
}
int kbd_is_english(void)
{
    HKL (WINAPI *f)(DWORD) = (void *)t_proc("GetKeyboardLayout");
    if (!f) return 0;
    return PRIMARYLANGID(LOWORD((ULONG_PTR)f(0))) == LANG_ENGLISH;
}

void kw_clear(void) { g_nklog = 0; }

HWND kw_plain(void)
{
    ensure_cls();
    return U(CreateWindowExA)(0, g_kcls, "kbdtest", WS_POPUP, 100, 100, 120, 80, NULL, NULL, GetModuleHandleA(NULL), NULL);
}

int kw_focus_ok(HWND w)
{
    return U(GetForegroundWindow)() == w && U(GetFocus)() == w;
}

void kw_pump(int ms, int want, int xlat)
{
    DWORD end = GetTickCount() + ms, quiet = 0;
    MSG m;
    for (;;) {
        int got = 0;
        while (U(PeekMessageA)(&m, NULL, 0, 0, PM_REMOVE)) {
            got = 1;
            if (xlat) U(TranslateMessage)(&m);
            U(DispatchMessageA)(&m);
        }
        if (g_nklog >= want) {
            if (got) quiet = GetTickCount();
            else if (!quiet) quiet = GetTickCount();
            else if (GetTickCount() - quiet > 40) break;
        }
        if ((LONG)(GetTickCount() - end) >= 0) break;
        U(MsgWaitForMultipleObjects)(0, NULL, FALSE, 10, QS_ALLINPUT);
    }
}

UINT kw_key(WORD vk, WORD scan, DWORD flags)
{
    INPUT in;
    UINT r;
    if (!(flags & KEYEVENTF_KEYUP) && g_guard && !kw_focus_ok(g_guard)) return 0;
    memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk; in.ki.wScan = scan; in.ki.dwFlags = flags;
    r = U(SendInput)(1, &in, sizeof in);
    if (!(flags & (KEYEVENTF_UNICODE | KEYEVENTF_SCANCODE)) && vk < 256) {
        if (flags & KEYEVENTF_KEYUP) g_held[vk] = 0; else g_held[vk] = 1;
    }
    return r;
}
UINT kw_down(WORD vk) { return kw_key(vk, 0, 0); }
UINT kw_up(WORD vk) { return kw_key(vk, 0, KEYEVENTF_KEYUP); }

void kw_release_all(void)
{
    int i;
    for (i = 255; i > 0; i--)
        if (g_held[i]) {
            DWORD fl = KEYEVENTF_KEYUP;
            if (i == VK_RCONTROL || i == VK_RMENU || (i >= VK_PRIOR && i <= VK_DOWN) || i == VK_INSERT || i == VK_DELETE) fl |= KEYEVENTF_EXTENDEDKEY;
            kw_key((WORD)i, 0, fl);
            g_held[i] = 0;
        }
}

int kw_count(UINT msg)
{
    int i, n = 0;
    for (i = 0; i < g_nklog; i++) if (g_klog[i].msg == msg) n++;
    return n;
}
struct klog *kw_find(UINT msg, int nth)
{
    int i;
    for (i = 0; i < g_nklog; i++)
        if (g_klog[i].msg == msg && nth-- == 0) return &g_klog[i];
    return NULL;
}

/* Drain until the queue has been quiet for a while (late deliveries of earlier injections). */
static void kw_settle(void)
{
    DWORD end = GetTickCount() + 1200;
    int last = -1;
    DWORD quiet = GetTickCount();
    while ((LONG)(GetTickCount() - end) < 0) {
        kw_pump(30, 100000, 0);
        if (g_nklog != last) { last = g_nklog; quiet = GetTickCount(); }
        else if (GetTickCount() - quiet > 150) break;
    }
}

HWND kw_begin(void)
{
    HWND w;
    DWORD end;
    if (!U(SendInput) || !U(SetForegroundWindow) || !U(CreateWindowExA) || !U(RegisterClassA) || !U(PeekMessageA)) {
        t_skip("missing exports for injection"); return NULL;
    }
    if (g_probe_state < 0) { t_skip("key injection unavailable on this desktop"); return NULL; }
    ensure_cls();
    w = U(CreateWindowExA)(0, g_kcls, "kbdtest", WS_POPUP | WS_VISIBLE, 100, 100, 160, 100, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!w) { t_skip("cannot create window"); return NULL; }
    U(SetForegroundWindow)(w);
    U(SetFocus)(w);
    end = GetTickCount() + 2000;
    while (!kw_focus_ok(w) && (LONG)(GetTickCount() - end) < 0) {
        kw_pump(30, 1000, 0);
        U(SetForegroundWindow)(w);
        U(SetFocus)(w);
    }
    if (!kw_focus_ok(w)) {
        g_guard = NULL;
        U(DestroyWindow)(w);
        t_skip("window cannot become foreground");
        return NULL;
    }
    kw_settle();
    kw_clear();
    g_guard = w;
    if (g_probe_state == 0) {
        /* Probe: VK_F13 must arrive as WM_KEYDOWN/UP in our window. */
        kw_down(VK_F13); kw_up(VK_F13);
        kw_pump(1500, 2, 0);
        g_probe_state = (kw_count(WM_KEYDOWN) == 1 && kw_count(WM_KEYUP) == 1) ? 1 : -1;
        kw_release_all();
        kw_settle();
        kw_clear();
        if (g_probe_state < 0) {
            g_guard = NULL;
            U(DestroyWindow)(w);
            t_skip("probe key was not delivered; injection unavailable");
            return NULL;
        }
    }
    return w;
}

void kw_end(HWND w)
{
    kw_release_all();
    g_guard = NULL;
    kw_settle();
    if (w) U(DestroyWindow)(w);
    kw_pump(10, 1000, 0);
    kw_clear();
}

/* ---- US layout acquisition (explicit-HKL variants make tables layout agnostic) ---- */
static int g_us_refs, g_us_loaded_by_us;
static HKL g_us_hkl;

HKL kbd_us_hkl(void)
{
    HKL list[64];
    int n, i, had = 0;
    HKL (WINAPI *lk)(LPCWSTR, UINT) = (void *)t_proc("LoadKeyboardLayoutW");
    int (WINAPI *gl)(int, HKL *) = (void *)t_proc("GetKeyboardLayoutList");
    HKL (WINAPI *gk)(DWORD) = (void *)t_proc("GetKeyboardLayout");
    if (g_us_refs++) return g_us_hkl;
    g_us_hkl = NULL; g_us_loaded_by_us = 0;
    if (!lk || !gl || !gk) return NULL;
    if (kbd_is_us()) { g_us_hkl = gk(0); return g_us_hkl; }
    n = gl(64, list);
    for (i = 0; i < n; i++) if ((ULONG_PTR)list[i] == 0x04090409) had = 1;
    g_us_hkl = lk(L"00000409", 0);
    if (g_us_hkl && !had) g_us_loaded_by_us = 1;
    return g_us_hkl;
}
void kbd_us_release(void)
{
    BOOL (WINAPI *ul)(HKL) = (void *)t_proc("UnloadKeyboardLayout");
    if (g_us_refs <= 0) return;
    if (--g_us_refs) return;
    if (g_us_loaded_by_us && ul && g_us_hkl) ul(g_us_hkl);
    g_us_loaded_by_us = 0; g_us_hkl = NULL;
}

UINT kw_input(const INPUT *in, UINT n)
{
    UINT i;
    if (g_guard && !kw_focus_ok(g_guard)) return 0;
    for (i = 0; i < n; i++)
        if (in[i].type == INPUT_KEYBOARD && !(in[i].ki.dwFlags & (KEYEVENTF_UNICODE | KEYEVENTF_SCANCODE)) && in[i].ki.wVk < 256)
            g_held[in[i].ki.wVk] = (in[i].ki.dwFlags & KEYEVENTF_KEYUP) ? 0 : 1;
    return U(SendInput)(n, (INPUT *)in, sizeof(INPUT));
}
