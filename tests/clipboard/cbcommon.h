#ifndef CBCOMMON_H
#define CBCOMMON_H
#include "t.h"
#define ERR_NOT_OPEN 1418
#define ERR_INVALID_PARAM 87
#define ERR_ACCESS_DENIED 5
#define ERR_INVALID_HANDLE 6
#define ERR_INVALID_WINDOW 1400

#define OPEN U(OpenClipboard)
#define CLOSE U(CloseClipboard)
#define EMPTY U(EmptyClipboard)
#define SETD U(SetClipboardData)
#define GETD U(GetClipboardData)
#define AVAIL U(IsClipboardFormatAvailable)
#define COUNT U(CountClipboardFormats)
#define ENUM U(EnumClipboardFormats)
#define SEQ U(GetClipboardSequenceNumber)

#define CB_REQ_BASIC do { REQUIRE(OPEN); REQUIRE(CLOSE); REQUIRE(EMPTY); REQUIRE(SETD); REQUIRE(GETD); \
    REQUIRE(AVAIL); REQUIRE(COUNT); REQUIRE(ENUM); REQUIRE(U(CreateWindowExA)); REQUIRE(U(DestroyWindow)); } while (0)

/* messages seen by the owner window proc */
#define CB_LOGMAX 256
extern UINT cb_log[CB_LOGMAX]; extern WPARAM cb_logw[CB_LOGMAX]; extern int cb_nlog;
extern HWND cb_next;
extern HANDLE cb_render_data; /* handle returned for WM_RENDERFORMAT (set by test) */
extern int cb_render_open_ok; /* result of GetOpenClipboardWindow-based check during render */
void cb_logclear(void);
int cb_logcount(UINT m);
HWND cb_win(void);            /* creates hidden window with the logging proc */
void cb_destroy(HWND h);
void cb_reset(void);          /* leave clipboard closed and empty */
HGLOBAL cb_mem(const void *p, size_t n);
HGLOBAL cb_text(const char *s);
HGLOBAL cb_wtext(const WCHAR *s);
int cb_has(UINT fmt);         /* format present in enumeration */
#endif
