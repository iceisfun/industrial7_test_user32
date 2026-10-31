/* Shared helpers for the input_kbd area. */
#ifndef KBD_H
#define KBD_H
#include "t.h"

#define KCHECK_NAMELEN 9

/* Logged keyboard-ish message. */
struct klog {
    UINT msg; WPARAM wp; LPARAM lp;
    DWORD dev, org;   /* GetCurrentInputMessageSource at delivery */
    DWORD tick;
};
#define KLOG_MAX 512
extern struct klog g_klog[KLOG_MAX];
extern int g_nklog;

/* Is the active layout exactly the US layout 00000409? */
int kbd_is_us(void);
/* Is the layout language English? */
int kbd_is_english(void);

/* US layout (00000409) HKL; loads it if needed. Pair with kbd_us_release(). NULL if unavailable. */
HKL kbd_us_hkl(void);
void kbd_us_release(void);

/* Create a visible popup that owns foreground+focus, or NULL (and t_skip) if
 * injection cannot be used on this desktop. */
HWND kw_begin(void);
void kw_end(HWND w);
HWND kw_plain(void);               /* just a window, no focus requirement */

/* Pump the queue for up to ms, translating if 'xlat'; stop once 'want'
 * entries are logged and the queue stays quiet for a short while. */
void kw_pump(int ms, int want, int xlat);
void kw_clear(void);
/* Send one key event through SendInput. Returns the SendInput result. */
UINT kw_key(WORD vk, WORD scan, DWORD flags);
UINT kw_down(WORD vk);
UINT kw_up(WORD vk);
int kw_count(UINT msg);
struct klog *kw_find(UINT msg, int nth);
/* Release anything still held. */
void kw_release_all(void);
int kw_focus_ok(HWND w);

/* Informational error-code comparison (never fails the run). */
#define NOTE_ERR(e) do { DWORD e_ = GetLastError(); if (e_ != (DWORD)(e)) t_note("%s:%d last error %lu, expected %lu", __FILE__, __LINE__, e_, (DWORD)(e)); } while (0)

static inline int kbd_hexname(const char *s)
{
    int i;
    if (strlen(s) != 8) return 0;
    for (i = 0; i < 8; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'A' && s[i] <= 'F') || (s[i] >= 'a' && s[i] <= 'f'))) return 0;
    return 1;
}

#ifndef MAPVK_VK_TO_VSC_EX
#define MAPVK_VK_TO_VSC_EX 4
#endif
#ifndef VK_PACKET
#define VK_PACKET 0xE7
#endif
#ifndef KEYEVENTF_UNICODE
#define KEYEVENTF_UNICODE 0x0004
#endif
#ifndef KEYEVENTF_SCANCODE
#define KEYEVENTF_SCANCODE 0x0008
#endif
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
#ifndef WM_UNICHAR
#define WM_UNICHAR 0x109
#endif
#ifndef ERROR_HOTKEY_ALREADY_REGISTERED
#define ERROR_HOTKEY_ALREADY_REGISTERED 1409
#endif
#ifndef ERROR_HOTKEY_NOT_REGISTERED
#define ERROR_HOTKEY_NOT_REGISTERED 1419
#endif
#ifndef ERROR_INVALID_ACCEL_HANDLE
#define ERROR_INVALID_ACCEL_HANDLE 1403
#endif
#ifndef ERROR_INVALID_KEYBOARD_HANDLE
#define ERROR_INVALID_KEYBOARD_HANDLE 1457
#endif
#ifndef ERROR_WINDOW_OF_OTHER_THREAD
#define ERROR_WINDOW_OF_OTHER_THREAD 1408
#endif
#ifndef ERROR_INVALID_WINDOW_HANDLE
#define ERROR_INVALID_WINDOW_HANDLE 1400
#endif

#endif
