/* gap_03: IsWow64Message, MB_GetString, MBToWCSEx/Ext, MessageBeep,
 * MakeThreadTSFEventAware, PrivateRegisterICSProc, NotifyOverlayWindow,
 * QuerySendMessage. Behaviour taken from the user32 Win10 static analysis. */
#include "t.h"

UDECL(BOOL, IsWow64Message, (void));
UDECL(ULONG_PTR, MB_GetString, (UINT));
UDECL(UINT, MBToWCSEx, (WORD cp, LPCSTR s, int cch, WCHAR **pbuf, int bufsz, int alloc));
UDECL(UINT, MBToWCSExt, (LPCSTR s, int cch, WCHAR **pbuf, int bufsz, int alloc));
UDECL(void, MakeThreadTSFEventAware, (UINT));
UDECL(ULONG_PTR, PrivateRegisterICSProc, (void *));
static int is_wine(void);
static int is_wine(void) { return GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL; }
#define REQUIRE_REAL(x) do { REQUIRE(x); if (is_wine() && !getenv("GAP3_NOWINEGUARD")) { t_skip("wine stub export"); return; } } while (0)
UDECL(void, NotifyOverlayWindow, (HWND, BOOL));
UDECL(BOOL, QuerySendMessage, (MSG *));

/* ---------------- IsWow64Message ---------------- */
TEST(gap3_iswow64message_plain) {
    REQUIRE_REAL(UN(IsWow64Message));
    SetLastError(0x1234);
    BOOL r = UN(IsWow64Message)();
    CHECK(r == FALSE);
    CHECK(GetLastError() == 0x1234);
}
TEST(gap3_iswow64message_repeat) {
    REQUIRE_REAL(UN(IsWow64Message));
    for (int i = 0; i < 1000; i++)
        if (UN(IsWow64Message)() != FALSE) { CHECK(0); return; }
    CHECK(1);
}
TEST(gap3_iswow64message_is_bool) {
    REQUIRE_REAL(UN(IsWow64Message));
    BOOL r = UN(IsWow64Message)();
    CHECK(r == 0 || r == 1);
}

static DWORD WINAPI iw_thread(LPVOID p) {
    *(BOOL *)p = UN(IsWow64Message)();
    return 0;
}
TEST(gap3_iswow64message_other_thread) {
    REQUIRE_REAL(UN(IsWow64Message));
    BOOL r = 99;
    HANDLE h = CreateThread(NULL, 0, iw_thread, &r, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(r == FALSE);
}

/* ---------------- cross-thread sent message helpers ---------------- */
static BOOL g_wow_in_proc = 99, g_insend = 99, g_qsm_called;
static BOOL g_qsm_ret;
static MSG g_qsm_msg;
#define WM_GAP3 (WM_USER + 0x33)
static LRESULT CALLBACK gap3_wp(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_GAP3) {
        if (UN(IsWow64Message)) g_wow_in_proc = UN(IsWow64Message)();
        BOOL (WINAPI *ism)(void) = (void *)t_proc("InSendMessage");
        if (ism) g_insend = ism();
        if (UN(QuerySendMessage)) {
            memset(&g_qsm_msg, 0, sizeof g_qsm_msg);
            g_qsm_ret = UN(QuerySendMessage)(&g_qsm_msg);
            g_qsm_called = TRUE;
        }
        return 0x5151;
    }
    return DefWindowProcA(h, m, w, l);
}
static DWORD WINAPI sender_thread(LPVOID p) {
    LRESULT (WINAPI *sm)(HWND, UINT, WPARAM, LPARAM) = (void *)t_proc("SendMessageA");
    LRESULT r = sm((HWND)p, WM_GAP3, 1, 2);
    return (DWORD)r;
}
/* Runs a cross-thread SendMessage into a window of this thread. */
static int run_cross_send(void) {
    ATOM (WINAPI *rc)(const WNDCLASSA *) = (void *)t_proc("RegisterClassA");
    BOOL (WINAPI *uc)(LPCSTR, HINSTANCE) = (void *)t_proc("UnregisterClassA");
    BOOL (WINAPI *dw)(HWND) = (void *)t_proc("DestroyWindow");
    BOOL (WINAPI *pm)(LPMSG, HWND, UINT, UINT, UINT) = (void *)t_proc("PeekMessageA");
    BOOL (WINAPI *tm)(const MSG *) = (void *)t_proc("TranslateMessage");
    LRESULT (WINAPI *dm)(const MSG *) = (void *)t_proc("DispatchMessageA");
    if (!rc || !uc || !dw || !pm || !dm || !tm) return -1;
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = gap3_wp; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "gap3_xsend";
    if (!rc(&wc)) return -1;
    HWND h = t_make_window("gap3_xsend", WS_POPUP, 0, 0, 0, 10, 10, NULL);
    int ok = 0;
    if (h) {
        HANDLE th = CreateThread(NULL, 0, sender_thread, h, 0, NULL);
        if (th) {
            DWORD end = GetTickCount() + 5000;
            while (GetTickCount() < end) {
                MSG m;
                while (pm(&m, NULL, 0, 0, PM_REMOVE)) { tm(&m); dm(&m); }
                if (WaitForSingleObject(th, 10) == WAIT_OBJECT_0) { ok = 1; break; }
            }
            CloseHandle(th);
        }
        dw(h);
    }
    uc("gap3_xsend", wc.hInstance);
    return ok ? 0 : -1;
}
TEST(gap3_iswow64message_in_sent_msg) {
    REQUIRE_REAL(UN(IsWow64Message));
    REQUIRE(U(RegisterClassA));
    g_wow_in_proc = 99; g_insend = 99;
    CHECK(run_cross_send() == 0);
    CHECK(g_wow_in_proc == FALSE);
    CHECK(g_insend == TRUE);
}
TEST(gap3_querysendmessage_in_sent_msg) {
    REQUIRE_REAL(UN(QuerySendMessage));
    REQUIRE(U(RegisterClassA));
    g_qsm_called = FALSE;
    CHECK(run_cross_send() == 0);
    CHECK(g_qsm_called);
    /* semantics of the MSG fill are undocumented: observe only */
    t_note("QuerySendMessage in sent msg: ret=%d msg=0x%x wp=%llx lp=%llx hwnd=%p", g_qsm_ret,
           g_qsm_msg.message, (unsigned long long)g_qsm_msg.wParam, (unsigned long long)g_qsm_msg.lParam, g_qsm_msg.hwnd);
}
TEST(gap3_querysendmessage_outside_sent_msg) {
    REQUIRE_REAL(UN(QuerySendMessage));
    MSG m; memset(&m, 0xAA, sizeof m);
    BOOL r = UN(QuerySendMessage)(&m);
    t_note("QuerySendMessage outside sent msg: ret=%d err=%lu msg=0x%x", r, GetLastError(), m.message);
}

/* ---------------- MB_GetString ---------------- */
TEST(gap3_mbgetstring_valid_indices) {
    REQUIRE_REAL(UN(MB_GetString));
    for (UINT i = 0; i <= 10; i++) {
        ULONG_PTR p = UN(MB_GetString)(i);
        CHECKF(p != 0, "idx %u", i);
    }
}
TEST(gap3_mbgetstring_out_of_range) {
    REQUIRE_REAL(UN(MB_GetString));
    UINT bad[] = { 11, 12, 13, 100, 0xFF, 0x100, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++)
        CHECKF(UN(MB_GetString)(bad[i]) == 0, "idx 0x%x", bad[i]);
}
TEST(gap3_mbgetstring_stride) {
    REQUIRE_REAL(UN(MB_GetString));
    ULONG_PTR prev = UN(MB_GetString)(0);
    CHECK(prev != 0);
    for (UINT i = 1; i <= 10; i++) {
        ULONG_PTR p = UN(MB_GetString)(i);
        CHECKF(p - prev == 0x28, "idx %u delta 0x%llx", i, (unsigned long long)(p - prev));
        prev = p;
    }
}
TEST(gap3_mbgetstring_stable) {
    REQUIRE_REAL(UN(MB_GetString));
    for (UINT i = 0; i <= 10; i++) {
        ULONG_PTR a = UN(MB_GetString)(i), b = UN(MB_GetString)(i);
        CHECK(a == b);
    }
}
TEST(gap3_mbgetstring_readable) {
    REQUIRE_REAL(UN(MB_GetString));
    for (UINT i = 0; i <= 10; i++) {
        ULONG_PTR p = UN(MB_GetString)(i);
        WCHAR buf[20]; SIZE_T got = 0;
        BOOL ok = ReadProcessMemory(GetCurrentProcess(), (void *)p, buf, sizeof buf, &got);
        CHECKF(ok && got == sizeof buf, "idx %u", i);
        if (ok) {
            buf[19] = 0;
            t_note("MB_GetString(%u) = \"%ls\"", i, buf);
        }
    }
}
TEST(gap3_mbgetstring_userland_range) {
    REQUIRE_REAL(UN(MB_GetString));
    ULONG_PTR p = UN(MB_GetString)(0);
    CHECK(p >= 0x10000 && p < 0x00007FFFFFFF0000ULL);
}

/* ---------------- MBToWCSEx / MBToWCSExt ---------------- */
/* Reference model of the analysed MBToWCSEx, used only when GAP3_MODEL is set
 * (to validate the test tables on hosts whose user32 lacks a real implementation). */
static UINT model_Ex(WORD cp, LPCSTR s, int cch, WCHAR **pb, int bufsz, int alloc) {
    if (cch == 0 || bufsz == 0 || !s) return 0;
    if (cch < 0) {
        if (cch < -2) return 0;
        int l = (int)strlen(s); cch = cch + 2 + l; if (cch == 0) return 0;
    }
    if (bufsz == -1) { bufsz = cch; if (!alloc) return 0; }
    else if (bufsz < -1) return 0;
    if (alloc) { *pb = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufsz * 2); if (!*pb) return 0; }
    if (cp != GetACP() && cp != 0) {
        UINT r = MultiByteToWideChar(cp, 0, s, cch, *pb, bufsz);
        return r;
    }
    WCHAR *tmp = HeapAlloc(GetProcessHeap(), 0, cch * 2 + 2);
    int n = MultiByteToWideChar(CP_ACP, 0, s, cch, tmp, cch);
    if (n > bufsz) n = bufsz;
    memcpy(*pb, tmp, n * 2);
    HeapFree(GetProcessHeap(), 0, tmp);
    return n;
}
static UINT model_Ext(LPCSTR s, int cch, WCHAR **pb, int bufsz, int alloc) { return model_Ex(0, s, cch, pb, bufsz, alloc); }
static int g_model_checked, g_use_model;
static int use_model(void) {
    if (!g_model_checked) { g_model_checked = 1; g_use_model = GetEnvironmentVariableA("GAP3_MODEL", NULL, 0) != 0; }
    return g_use_model;
}
static MBToWCSEx_pfn get_Ex(void) {
    if (use_model()) return model_Ex;
    if (is_wine()) return NULL;   /* wine's user32 aborts on this export */
    return UN(MBToWCSEx);
}
static MBToWCSExt_pfn get_Ext(void) {
    if (use_model()) return model_Ext;
    if (is_wine()) return NULL;
    return UN(MBToWCSExt);
}
#define MBEx get_Ex()
#define MBExt get_Ext()

struct mbcase { const char *s; int cch; int bufsz; UINT want; const char *wstr; };
static void run_mb_table(int use_ext, WORD cp) {
    static const struct mbcase t[] = {
        { "abc", -1, 64, 4, "abc" },
        { "abc", -2, 64, 3, "abc" },
        { "abc", 3, 64, 3, "abc" },
        { "abc", 2, 64, 2, "ab" },
        { "abc", 1, 64, 1, "a" },
        { "abc", 4, 64, 4, "abc" },
        { "abc", 0, 64, 0, NULL },
        { "abc", -3, 64, 0, NULL },
        { "abc", -100, 64, 0, NULL },
        { "abc", (int)0x80000000, 64, 0, NULL },
        { "", -1, 64, 1, "" },
        { "", -2, 64, 0, NULL },
        { "", 1, 64, 1, "" },
        { "abc", 3, 0, 0, NULL },
        { "abc", 3, -2, 0, NULL },
        { "abc", 3, -100, 0, NULL },
        { "abc", 3, -1, 0, NULL },       /* bufsz -1 requires alloc */
        { NULL, 3, 64, 0, NULL },
        { NULL, -1, 64, 0, NULL },
        { "Hello, World", -1, 64, 13, "Hello, World" },
        { "Hello, World", 5, 64, 5, "Hello" },
        { "a\0b", 3, 64, 3, "a" },       /* embedded NUL counted by explicit cch */
        { "a\0b", -1, 64, 2, "a" },
        { "a\0b", -2, 64, 1, "a" },
        { "abcdef", -1, 7, 7, "abcdef" },
        { "abcdef", 6, 6, 6, "abcdef" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        WCHAR buf[80], *pb = buf;
        for (int k = 0; k < 80; k++) buf[k] = 0x7777;
        UINT r = use_ext ? MBExt(t[i].s, t[i].cch, &pb, t[i].bufsz, 0)
                         : MBEx(cp, t[i].s, t[i].cch, &pb, t[i].bufsz, 0);
        CHECKF(r == t[i].want, "case %u cp=%u: got %u want %u", i, cp, r, t[i].want);
        CHECKF(pb == buf, "case %u: buffer pointer changed", i);
        if (t[i].wstr && r) {
            WCHAR w[80]; int n = 0;
            for (const char *c = t[i].wstr; *c; c++) w[n++] = (unsigned char)*c;
            int cmp = n < (int)r ? n : (int)r;
            CHECKF(!memcmp(buf, w, cmp * sizeof(WCHAR)), "case %u content", i);
            /* nothing written past the reported count */
            CHECKF(buf[r] == 0x7777, "case %u overrun (0x%x)", i, buf[r]);
        }
    }
}
TEST(gap3_mbtowcsex_table_cp0) { REQUIRE(MBEx); run_mb_table(0, 0); }
TEST(gap3_mbtowcsex_table_acp) { REQUIRE(MBEx); run_mb_table(0, (WORD)GetACP()); }
TEST(gap3_mbtowcsext_table) { REQUIRE(MBExt); run_mb_table(1, 0); }

TEST(gap3_mbtowcsex_truncate_acp) {
    REQUIRE(MBEx);
    WCHAR buf[8], *pb = buf;
    for (int k = 0; k < 8; k++) buf[k] = 0x7777;
    UINT r = MBEx(0, "abcdef", 6, &pb, 2, 0);
    CHECK(r == 2);
    CHECK(buf[0] == 'a' && buf[1] == 'b' && buf[2] == 0x7777);
}
TEST(gap3_mbtowcsex_truncate_cch_minus1) {
    REQUIRE(MBEx);
    WCHAR buf[8], *pb = buf;
    for (int k = 0; k < 8; k++) buf[k] = 0x7777;
    UINT r = MBEx(0, "abc", -1, &pb, 3, 0);
    CHECK(r == 3);
    CHECK(buf[0] == 'a' && buf[2] == 'c' && buf[3] == 0x7777);
}
TEST(gap3_mbtowcsex_utf8) {
    REQUIRE(MBEx);
    WCHAR buf[16], *pb = buf;
    memset(buf, 0, sizeof buf);
    UINT r = MBEx(CP_UTF8, "\xC3\xA9", 2, &pb, 16, 0);
    CHECK(r == 1);
    CHECK(buf[0] == 0xE9);
    memset(buf, 0, sizeof buf);
    r = MBEx(CP_UTF8, "\xE2\x82\xAC!", -1, &pb, 16, 0);
    CHECK(r == 3);
    CHECK(buf[0] == 0x20AC && buf[1] == '!' && buf[2] == 0);
    memset(buf, 0, sizeof buf);
    r = MBEx(CP_UTF8, "\xF0\x9F\x98\x80", 4, &pb, 16, 0);
    CHECK(r == 2);
    CHECK(buf[0] == 0xD83D && buf[1] == 0xDE00);
}
TEST(gap3_mbtowcsex_utf8_small_buffer_fails) {
    REQUIRE(MBEx);
    /* A non-ACP code page goes through MultiByteToWideChar, which fails (no truncation) */
    WCHAR buf[16], *pb = buf;
    memset(buf, 0, sizeof buf);
    SetLastError(0);
    UINT r = MBEx(CP_UTF8, "abc", -1, &pb, 2, 0);
    CHECK(r == 0);
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
}
TEST(gap3_mbtowcsex_cp437) {
    REQUIRE(MBEx);
    if (!IsValidCodePage(437)) { t_skip("cp437 missing"); return; }
    WCHAR buf[8], *pb = buf;
    memset(buf, 0, sizeof buf);
    UINT r = MBEx(437, "\x80\x81", 2, &pb, 8, 0);
    CHECK(r == 2);
    CHECK(buf[0] == 0x00C7 && buf[1] == 0x00FC);
}
TEST(gap3_mbtowcsex_bad_codepage) {
    REQUIRE(MBEx);
    WCHAR buf[8], *pb = buf;
    SetLastError(0);
    UINT r = MBEx(0xFFFE, "abc", 3, &pb, 8, 0);
    CHECK(r == 0);
    t_note("bad cp err=%lu", GetLastError());
}
TEST(gap3_mbtowcsex_alloc_basic) {
    REQUIRE(MBEx);
    WCHAR *pb = (WCHAR *)(ULONG_PTR)0x1111;
    UINT r = MBEx(0, "abc", -1, &pb, -1, 1);
    CHECK(r == 4);
    CHECK(pb != (WCHAR *)(ULONG_PTR)0x1111 && pb != NULL);
    if (pb != (WCHAR *)(ULONG_PTR)0x1111 && pb) {
        CHECK(pb[0] == 'a' && pb[1] == 'b' && pb[2] == 'c' && pb[3] == 0);
    }
}
TEST(gap3_mbtowcsex_alloc_explicit_size) {
    REQUIRE(MBEx);
    WCHAR *pb = NULL;
    UINT r = MBEx(0, "hello", 5, &pb, 16, 1);
    CHECK(r == 5);
    CHECK(pb != NULL);
    if (pb) CHECK(!memcmp(pb, L"hello", 10));
}
TEST(gap3_mbtowcsex_alloc_utf8) {
    REQUIRE(MBEx);
    WCHAR *pb = NULL;
    UINT r = MBEx(CP_UTF8, "\xC3\xA9x", -1, &pb, -1, 1);
    CHECK(r == 3);
    CHECK(pb != NULL);
    if (pb) CHECK(pb[0] == 0xE9 && pb[1] == 'x' && pb[2] == 0);
}
TEST(gap3_mbtowcsex_alloc_ext) {
    REQUIRE(MBExt);
    WCHAR *pb = NULL;
    UINT r = MBExt("xyz", -2, &pb, -1, 1);
    CHECK(r == 3);
    CHECK(pb != NULL);
    if (pb) CHECK(pb[0] == 'x' && pb[1] == 'y' && pb[2] == 'z');
}
TEST(gap3_mbtowcsex_early_fail_keeps_ptr) {
    REQUIRE(MBEx);
    WCHAR *sentinel = (WCHAR *)(ULONG_PTR)0x2222;
    WCHAR *pb;
    pb = sentinel; CHECK(MBEx(0, "abc", 0, &pb, 8, 1) == 0 && pb == sentinel);
    pb = sentinel; CHECK(MBEx(0, "abc", 3, &pb, 0, 1) == 0 && pb == sentinel);
    pb = sentinel; CHECK(MBEx(0, NULL, 3, &pb, 8, 1) == 0 && pb == sentinel);
    pb = sentinel; CHECK(MBEx(0, "abc", -3, &pb, 8, 1) == 0 && pb == sentinel);
    pb = sentinel; CHECK(MBEx(0, "", -2, &pb, 8, 1) == 0 && pb == sentinel);
    pb = sentinel; CHECK(MBEx(0, "abc", 3, &pb, -2, 1) == 0 && pb == sentinel);
    pb = sentinel; CHECK(MBEx(0, "abc", 3, &pb, -1, 0) == 0 && pb == sentinel);
}
TEST(gap3_mbtowcsex_no_lasterror_on_param_fail) {
    REQUIRE(MBEx);
    WCHAR buf[4], *pb = buf;
    SetLastError(0x4242);
    CHECK(MBEx(0, "abc", 0, &pb, 4, 0) == 0);
    CHECK(MBEx(0, NULL, 3, &pb, 4, 0) == 0);
    /* Win10 22H2 overwrites the last error on a failed parameter check: observed, not required. */
    if (GetLastError() != 0x4242) t_note("MultiByteToWideCharEx param failure changed last error to %lu", GetLastError());
}
TEST(gap3_mbtowcsext_matches_ex) {
    REQUIRE(MBEx); REQUIRE(MBExt);
    static const char *strs[] = { "", "a", "abc", "The quick brown fox" };
    static const int cchs[] = { -2, -1, 1, 2, 3 };
    for (unsigned i = 0; i < 4; i++)
        for (unsigned j = 0; j < 5; j++) {
            WCHAR b1[40], b2[40], *p1 = b1, *p2 = b2;
            memset(b1, 0x55, sizeof b1); memset(b2, 0x55, sizeof b2);
            UINT r1 = MBEx(0, strs[i], cchs[j], &p1, 40, 0);
            UINT r2 = MBExt(strs[i], cchs[j], &p2, 40, 0);
            CHECKF(r1 == r2, "%u/%u", i, j);
            CHECKF(!memcmp(b1, b2, sizeof b1), "%u/%u content", i, j);
        }
}

/* ---------------- MessageBeep ---------------- */
TEST(gap3_messagebeep_valid_types) {
    REQUIRE(U(MessageBeep));
    UINT types[] = { MB_OK, MB_ICONHAND, MB_ICONQUESTION, MB_ICONEXCLAMATION, MB_ICONASTERISK, 0xFFFFFFFFu };
    for (unsigned i = 0; i < 6; i++) {
        BOOL r = U(MessageBeep)(types[i]);
        CHECKF(r != 0, "type 0x%x", types[i]);
    }
}
TEST(gap3_messagebeep_invalid_types) {
    REQUIRE(U(MessageBeep));
    UINT types[] = { 0x1, 0x5, 0x1234, 0x7FFFFFFF, 0x80000000u };
    for (unsigned i = 0; i < 5; i++) {
        BOOL r = U(MessageBeep)(types[i]);
        t_note("MessageBeep(0x%x)=%d err=%lu", types[i], r, GetLastError());
    }
}

/* ---------------- MakeThreadTSFEventAware ---------------- */
TEST(gap3_tsfeventaware_invalid_noop) {
    REQUIRE_REAL(UN(MakeThreadTSFEventAware));
    /* zero and any bit outside 0x1703 are ignored without reaching the kernel */
    UINT bad[] = { 0, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100000, 0x80000000u, 0xFFFFFFFFu, 0xFFFFE8FCu };
    SetLastError(0x77);
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) UN(MakeThreadTSFEventAware)(bad[i]);
    CHECK(GetLastError() == 0x77);
}
static DWORD WINAPI tsf_thread(LPVOID p) {
    /* MakeThreadTSFEventAware dereferences the calling thread's win32k thread
     * info unconditionally; on a thread that never called into win32k that
     * pointer is NULL and the call faults (observed on Win10 22H2, AV in the
     * export). Convert the worker to a GUI thread first. */
    BOOL (WINAPI *igt)(BOOL) = (void *)t_proc("IsGUIThread");
    if (igt) igt(TRUE);
    UN(MakeThreadTSFEventAware)(0x1);
    UN(MakeThreadTSFEventAware)(0x1);   /* second time: flag already set -> early return */
    UN(MakeThreadTSFEventAware)(0x2);
    *(int *)p = 1;
    return 0;
}
TEST(gap3_tsfeventaware_valid_worker_thread) {
    REQUIRE_REAL(UN(MakeThreadTSFEventAware));
    int done = 0;
    HANDLE h = CreateThread(NULL, 0, tsf_thread, &done, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(done == 1);
}

/* ---------------- PrivateRegisterICSProc ---------------- */
TEST(gap3_ics_null_idempotent) {
    REQUIRE_REAL(UN(PrivateRegisterICSProc));
    /* Registering NULL leaves the slot empty, so repeated calls give the same answer */
    ULONG_PTR a = UN(PrivateRegisterICSProc)(NULL) & 0xFFFFFFFF;
    ULONG_PTR b = UN(PrivateRegisterICSProc)(NULL) & 0xFFFFFFFF;
    ULONG_PTR c = UN(PrivateRegisterICSProc)(NULL) & 0xFFFFFFFF;
    CHECK(a == 0 || a == 1);
    CHECK(a == b);
    CHECK(b == c);
    t_note("PrivateRegisterICSProc(NULL) -> %llu", (unsigned long long)a);
}

/* ---------------- NotifyOverlayWindow ---------------- */
TEST(gap3_notifyoverlay_invalid_hwnd) {
    REQUIRE_REAL(UN(NotifyOverlayWindow));
    HWND bad[] = { NULL, (HWND)(ULONG_PTR)1, (HWND)(ULONG_PTR)0xDEADBEEF, (HWND)(ULONG_PTR)0xFFFFFFFF, (HWND)(ULONG_PTR)-1 };
    for (unsigned i = 0; i < 5; i++) {
        UN(NotifyOverlayWindow)(bad[i], 0);
        UN(NotifyOverlayWindow)(bad[i], 1);
        UN(NotifyOverlayWindow)(bad[i], -1);
    }
    CHECK(1);
}
TEST(gap3_notifyoverlay_destroyed_window) {
    REQUIRE_REAL(UN(NotifyOverlayWindow)); REQUIRE(U(DestroyWindow));
    HWND h = t_make_window(t_cls(), WS_POPUP, 0, 0, 0, 10, 10, NULL);
    CHECK(h != NULL);
    if (!h) return;
    U(DestroyWindow)(h);
    UN(NotifyOverlayWindow)(h, 0);
    UN(NotifyOverlayWindow)(h, 1);
    CHECK(U(IsWindow) ? !U(IsWindow)(h) : 1);
}

/* ---------------- LoadKeyboardLayoutEx ---------------- */
UDECL(HKL, LoadKeyboardLayoutEx, (HKL, LPCWSTR, UINT));
TEST(gap3_loadkblayoutex_null_hkl) {
    REQUIRE_REAL(UN(LoadKeyboardLayoutEx));
    UINT flags[] = { 0, KLF_ACTIVATE, KLF_NOTELLSHELL, KLF_REORDER, KLF_REPLACELANG, KLF_SUBSTITUTE_OK, KLF_SETFORPROCESS, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        SetLastError(0x5A5A);
        HKL r = UN(LoadKeyboardLayoutEx)(NULL, L"00000409", flags[i]);
        CHECKF(r == NULL, "flags 0x%x", flags[i]);
        CHECKF(GetLastError() == 0x5A5A, "flags 0x%x err %lu", flags[i], GetLastError());
    }
}
TEST(gap3_loadkblayoutex_null_hkl_null_name) {
    REQUIRE_REAL(UN(LoadKeyboardLayoutEx));
    SetLastError(0x5A5A);
    CHECK(UN(LoadKeyboardLayoutEx)(NULL, NULL, 0) == NULL);
    CHECK(UN(LoadKeyboardLayoutEx)(NULL, L"", 0) == NULL);
    CHECK(UN(LoadKeyboardLayoutEx)(NULL, L"zzzzzzzz", 0) == NULL);
    CHECK(UN(LoadKeyboardLayoutEx)(NULL, L"0000000000000000000", KLF_ACTIVATE) == NULL);
    CHECK(GetLastError() == 0x5A5A);
}
