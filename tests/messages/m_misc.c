/* RegisterWindowMessage, WM_COPYDATA, message-only windows, message filters, broadcast, message ranges, A/W procs. */
#include "mh.h"

#define MU(n) (WM_USER + (n))
#define NOERR 0xdeadbeef
#define SETE() SetLastError(NOERR)

/* ---- RegisterWindowMessage ---- */
TEST(messages_regmsg_range_and_stability) {
    UINT a, b;
    REQUIRE(U(RegisterWindowMessageA));
    a = U(RegisterWindowMessageA)("t32_msgs_stable_1");
    b = U(RegisterWindowMessageA)("t32_msgs_stable_1");
    CHECK(a >= 0xC000 && a <= 0xFFFF);
    EXPECT_EQ(a, b);
}

TEST(messages_regmsg_a_w_same_id) {
    UINT a, w;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE(U(RegisterWindowMessageW));
    a = U(RegisterWindowMessageA)("t32_msgs_aw");
    w = U(RegisterWindowMessageW)(L"t32_msgs_aw");
    CHECK(a != 0);
    EXPECT_EQ(a, w);
    w = U(RegisterWindowMessageW)(L"t32_msgs_wonly");
    a = U(RegisterWindowMessageA)("t32_msgs_wonly");
    CHECK(w != 0);
    EXPECT_EQ(a, w);
}

TEST(messages_regmsg_distinct_names_distinct_ids) {
    UINT a, b, c;
    REQUIRE(U(RegisterWindowMessageA));
    a = U(RegisterWindowMessageA)("t32_msgs_d1");
    b = U(RegisterWindowMessageA)("t32_msgs_d2");
    c = U(RegisterWindowMessageA)("t32_msgs_d3");
    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
}

TEST(messages_regmsg_case_sensitivity_observed) {
    UINT a, b;
    REQUIRE(U(RegisterWindowMessageA));
    a = U(RegisterWindowMessageA)("t32_msgs_CaSe");
    b = U(RegisterWindowMessageA)("T32_MSGS_case");
    CHECK(a != 0 && b != 0);
    t_note("RegisterWindowMessage case-insensitive: %s", a == b ? "yes" : "no");
}

TEST(messages_regmsg_empty_and_null) {
    UINT r;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE(U(RegisterWindowMessageW));
    SETE(); r = U(RegisterWindowMessageW)(L"");
    CHECK(r == 0); t_note("RegisterWindowMessageW(\"\") error %lu", GetLastError());
    SETE(); r = U(RegisterWindowMessageA)("");
    CHECK(r == 0); t_note("RegisterWindowMessageA(\"\") error %lu", GetLastError());
    SETE(); r = U(RegisterWindowMessageW)(NULL);
    CHECK(r == 0); t_note("RegisterWindowMessageW(NULL) error %lu", GetLastError());
    SETE(); r = U(RegisterWindowMessageA)(NULL);
    CHECK(r == 0);
    t_note("RegisterWindowMessageA(NULL) error %lu", GetLastError());
}

TEST(messages_regmsg_too_long_name) {
    static char big[600]; static WCHAR wbig[600]; UINT r; int i;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE(U(RegisterWindowMessageW));
    for (i = 0; i < 599; i++) { big[i] = 'a' + (i % 26); wbig[i] = L'a' + (i % 26); }
    SETE(); r = U(RegisterWindowMessageA)(big);
    CHECK(r == 0);
    t_note("RegisterWindowMessageA(599 chars) error %lu", GetLastError());
    SETE(); r = U(RegisterWindowMessageW)(wbig);
    CHECK(r == 0);
    big[255] = 0; wbig[255] = 0;
    r = U(RegisterWindowMessageA)(big);
    t_note("RegisterWindowMessageA(255 chars) -> %x", r);
    big[256] = 0;
}

TEST(messages_regmsg_shares_clipboard_format_namespace) {
    UINT m, c;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE(U(RegisterClipboardFormatA));
    m = U(RegisterWindowMessageA)("t32_msgs_clipshared");
    c = U(RegisterClipboardFormatA)("t32_msgs_clipshared");
    CHECK(m != 0);
    EXPECT_EQ(m, c);
    c = U(RegisterClipboardFormatW)(L"t32_msgs_clipshared2");
    m = U(RegisterWindowMessageW)(L"t32_msgs_clipshared2");
    EXPECT_EQ(m, c);
}

TEST(messages_regmsg_name_retrievable_via_clipboard_api) {
    UINT m; char b[64]; int r;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE(U(GetClipboardFormatNameA));
    m = U(RegisterWindowMessageA)("t32_msgs_nameback"); REQUIRE(m);
    memset(b, 0, sizeof b);
    r = U(GetClipboardFormatNameA)(m, b, 64);
    t_note("GetClipboardFormatName(registered window message) -> %d \"%s\"", r, b);
    if (r) EXPECT_STR(b, "t32_msgs_nameback");
}

TEST(messages_regmsg_deliverable_to_window) {
    UINT m; HWND h;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    m = U(RegisterWindowMessageA)("t32_msgs_deliver"); REQUIRE(m);
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_rec = 1;
    SMA(h, m, 11, 22);
    PMA(h, m, 33, 44); mh_drain();
    mh_rec = 0;
    EXPECT_EQ(mh_count(m), 2);
    { int k = mh_find(m, 0); CHECK(k >= 0 && mh_ev[k].w == 11 && mh_ev[k].l == 22);
      k = mh_find(m, k + 1); CHECK(k >= 0 && mh_ev[k].w == 33 && mh_ev[k].l == 44); }
    mh_kill(h);
}

TEST(messages_regmsg_ids_increase_or_unique_batch) {
    UINT ids[12]; int i, j; char nm[48];
    REQUIRE(U(RegisterWindowMessageA));
    for (i = 0; i < 12; i++) {
        sprintf(nm, "t32_msgs_batch_%d", i);
        ids[i] = U(RegisterWindowMessageA)(nm);
        CHECKF(ids[i] >= 0xC000 && ids[i] <= 0xFFFF, "id %x", ids[i]);
    }
    for (i = 0; i < 12; i++) for (j = i + 1; j < 12; j++) CHECK(ids[i] != ids[j]);
    for (i = 0; i < 12; i++) {
        sprintf(nm, "t32_msgs_batch_%d", i);
        EXPECT_EQ(U(RegisterWindowMessageA)(nm), ids[i]);
    }
}

TEST(messages_regmsg_known_system_messages_stable) {
    UINT a, b;
    REQUIRE(U(RegisterWindowMessageA));
    a = U(RegisterWindowMessageA)("TaskbarCreated");
    b = U(RegisterWindowMessageA)("TaskbarCreated");
    CHECK(a >= 0xC000 && a <= 0xFFFF);
    EXPECT_EQ(a, b);
    a = U(RegisterWindowMessageA)("commdlg_LBSelChangedNotify");
    CHECK(a >= 0xC000 && a <= 0xFFFF);
}

/* ---- WM_COPYDATA ---- */
static volatile ULONG_PTR cd_dw; static volatile DWORD cd_cb, cd_sum, cd_tid; static volatile LONG cd_n; static volatile WPARAM cd_w;
static LRESULT hk_copydata(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_COPYDATA) {
        COPYDATASTRUCT *c = (COPYDATASTRUCT *)l; DWORD i, s = 0;
        cd_dw = c->dwData; cd_cb = c->cbData; cd_w = w; cd_tid = GetCurrentThreadId();
        for (i = 0; i < c->cbData; i++) s = s * 31 + ((BYTE *)c->lpData)[i];
        cd_sum = s; InterlockedIncrement(&cd_n);
        *hd = 1;
        return TRUE;
    }
    return 0;
}
static DWORD sum_of(const BYTE *p, DWORD n) { DWORD i, s = 0; for (i = 0; i < n; i++) s = s * 31 + p[i]; return s; }

TEST(messages_copydata_same_thread) {
    HWND h, snd; COPYDATASTRUCT c; char data[] = "payload-123";
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset(); cd_n = 0;
    h = mh_mk(0, 0, NULL); snd = mh_mk(0, 0, NULL); REQUIRE(h && snd);
    c.dwData = 0x1234567; c.cbData = sizeof data; c.lpData = data;
    mh_hook = hk_copydata;
    EXPECT_EQ(SMA(h, WM_COPYDATA, (WPARAM)snd, (LPARAM)&c), TRUE);
    mh_hook = NULL;
    EXPECT_EQ(cd_n, 1); EXPECT_EQ(cd_dw, 0x1234567); EXPECT_EQ(cd_cb, sizeof data);
    EXPECT_EQ(cd_w, snd); EXPECT_EQ(cd_sum, sum_of((BYTE *)data, sizeof data));
    mh_kill(h); mh_kill(snd);
}

TEST(messages_copydata_dwdata_64bit) {
    HWND h; COPYDATASTRUCT c; char d = 1;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    c.dwData = (ULONG_PTR)0xFEDCBA9876543210ull; c.cbData = 1; c.lpData = &d;
    mh_hook = hk_copydata;
    SMA(h, WM_COPYDATA, 0, (LPARAM)&c);
    mh_hook = NULL;
    EXPECT_EQ(cd_dw, 0xFEDCBA9876543210ull);
    mh_kill(h);
}

TEST(messages_copydata_cross_thread_content) {
    MH_TH t; COPYDATASTRUCT c; static BYTE data[10000]; DWORD_PTR r = 0; int i;
    REQUIRE_MH();
    mh_reset(); cd_n = 0;
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    for (i = 0; i < 10000; i++) data[i] = (BYTE)(i * 7 + 3);
    c.dwData = 99; c.cbData = 10000; c.lpData = data;
    mh_hook = hk_copydata;
    CHECK(mh_xsend(t.hwnd, WM_COPYDATA, 0, (LPARAM)&c, &r));
    mh_hook = NULL;
    EXPECT_EQ(r, TRUE);
    EXPECT_EQ(cd_n, 1); EXPECT_EQ(cd_dw, 99); EXPECT_EQ(cd_cb, 10000);
    EXPECT_EQ(cd_sum, sum_of(data, 10000));
    EXPECT_EQ(cd_tid, t.tid);
    mh_th_stop(&t);
}

TEST(messages_copydata_cross_thread_large) {
    MH_TH t; COPYDATASTRUCT c; BYTE *data; DWORD_PTR r = 0; DWORD i, n = 1024 * 1024;
    REQUIRE_MH();
    mh_reset(); cd_n = 0;
    data = HeapAlloc(GetProcessHeap(), 0, n); REQUIRE(data);
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); HeapFree(GetProcessHeap(), 0, data); return; }
    for (i = 0; i < n; i++) data[i] = (BYTE)(i ^ (i >> 8));
    c.dwData = 1; c.cbData = n; c.lpData = data;
    mh_hook = hk_copydata;
    CHECK(mh_xsend(t.hwnd, WM_COPYDATA, 0, (LPARAM)&c, &r));
    mh_hook = NULL;
    EXPECT_EQ(cd_cb, n);
    EXPECT_EQ(cd_sum, sum_of(data, n));
    mh_th_stop(&t);
    HeapFree(GetProcessHeap(), 0, data);
}

TEST(messages_copydata_zero_length) {
    MH_TH t; COPYDATASTRUCT c; DWORD_PTR r = 0;
    REQUIRE_MH();
    mh_reset(); cd_n = 0;
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    c.dwData = 5; c.cbData = 0; c.lpData = NULL;
    mh_hook = hk_copydata;
    mh_xsend(t.hwnd, WM_COPYDATA, 0, (LPARAM)&c, &r);
    mh_hook = NULL;
    t_note("WM_COPYDATA cbData=0 lpData=NULL cross-thread: handler calls %ld, result %llu", (long)cd_n, (unsigned long long)r);
    mh_th_stop(&t);
}

TEST(messages_copydata_post_fails_sync_only) {
    HWND h; COPYDATASTRUCT c; char d = 1; BOOL r;
    REQUIRE(PMA); REQUIRE(PMW); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    c.dwData = 1; c.cbData = 1; c.lpData = &d;
    SETE(); r = PMA(h, WM_COPYDATA, 0, (LPARAM)&c);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_MESSAGE_SYNC_ONLY);
    SETE(); r = PMW(h, WM_COPYDATA, 0, (LPARAM)&c);
    CHECK(r == FALSE); EXPECT_ERR(r, ERROR_MESSAGE_SYNC_ONLY);
    SETE(); r = U(PostThreadMessageA)(GetCurrentThreadId(), WM_COPYDATA, 0, (LPARAM)&c);
    CHECK(r == FALSE);
    t_note("PostThreadMessage(WM_COPYDATA) error %lu", GetLastError());
    mh_drain();
    mh_kill(h);
}

TEST(messages_copydata_a_w_windows_no_conversion) {
    HWND a, w; COPYDATASTRUCT c; char d[] = "ab\xE9z";
    REQUIRE(SMA); REQUIRE(SMW); REQUIRE_MH();
    mh_reset();
    a = mh_mk(0, 0, NULL); w = mh_mkw(0, 0, NULL); REQUIRE(a && w);
    c.dwData = 2; c.cbData = sizeof d; c.lpData = d;
    mh_hook = hk_copydata;
    cd_sum = 0; SMA(w, WM_COPYDATA, 0, (LPARAM)&c);
    EXPECT_EQ(cd_sum, sum_of((BYTE *)d, sizeof d));
    cd_sum = 0; SMW(a, WM_COPYDATA, 0, (LPARAM)&c);
    EXPECT_EQ(cd_sum, sum_of((BYTE *)d, sizeof d));
    mh_hook = NULL;
    mh_kill(a); mh_kill(w);
}

static LRESULT hk_cd_false(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_COPYDATA) { *hd = 1; return 0; }
    return 0;
}
TEST(messages_copydata_result_passthrough) {
    MH_TH t; COPYDATASTRUCT c; char d = 0; DWORD_PTR r = 5; HWND h;
    REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    c.dwData = 0; c.cbData = 1; c.lpData = &d;
    mh_hook = hk_cd_false;
    EXPECT_EQ(SMA(h, WM_COPYDATA, 0, (LPARAM)&c), 0);
    mh_hook = NULL;
    mh_kill(h);
    (void)t; (void)r;
}

/* ---- message-only windows ---- */
static HWND enum_target; static int enum_hit, enum_total;
static BOOL CALLBACK enum_cb(HWND h, LPARAM l) { enum_total++; if (h == enum_target) enum_hit++; return enum_total < 5000; }

TEST(messages_msgonly_create_and_basic_properties) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, HWND_MESSAGE); REQUIRE(h);
    CHECK(ISWND(h));
    CHECK(!U(IsWindowVisible)(h));
    {
        HWND par = U(GetParent)(h), anc = U(GetAncestor)(h, GA_PARENT);
        char cls[32] = "";
        t_note("message-only GetParent=%p GetAncestor(GA_PARENT)=%p", par, anc);
        if (anc) { U(GetClassNameA)(anc, cls, 32); t_note("message window parent class \"%s\"", cls); }
        if (!anc || anc == U(GetDesktopWindow)()) t_note("unexpected GA_PARENT for message-only window");
    }
    mh_kill(h);
}

TEST(messages_msgonly_not_enumerated_by_enumwindows) {
    HWND h; BOOL r;
    REQUIRE(CWEA); REQUIRE(U(EnumWindows)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, HWND_MESSAGE); REQUIRE(h);
    enum_target = h; enum_hit = enum_total = 0;
    r = U(EnumWindows)(enum_cb, 0);
    CHECK(r);
    EXPECT_EQ(enum_hit, 0);
    CHECK(enum_total > 0);
    mh_kill(h);
}

TEST(messages_msgonly_findwindowex_message_parent) {
    HWND h, f; char title[48];
    REQUIRE(CWEA); REQUIRE(U(FindWindowExA)); REQUIRE_MH();
    mh_reset();
    sprintf(title, "t32_msgonly_%lu_%lu", GetCurrentProcessId(), GetTickCount());
    h = CWEA(0, MH_CLSA, title, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleA(NULL), NULL);
    REQUIRE(h);
    f = U(FindWindowExA)(HWND_MESSAGE, NULL, MH_CLSA, title);
    EXPECT_EQ(f, h);
    f = U(FindWindowExA)(HWND_MESSAGE, NULL, NULL, title);
    EXPECT_EQ(f, h);
    f = U(FindWindowExA)(HWND_MESSAGE, h, MH_CLSA, title);
    EXPECT_EQ(f, 0);
    f = U(FindWindowExA)(HWND_MESSAGE, NULL, MH_CLSA, "t32_no_such_title_zzz");
    EXPECT_EQ(f, 0);
    f = U(FindWindowExA)(U(GetDesktopWindow)(), NULL, MH_CLSA, title);
    t_note("FindWindowEx(desktop, message-only title) -> %p", f);
    f = U(FindWindowExA)(NULL, NULL, MH_CLSA, title);
    t_note("FindWindowEx(NULL,NULL, message-only title) -> %p (docs say message-only windows are searched)", f);
    mh_kill(h);
}

TEST(messages_msgonly_findwindow_top_level_only) {
    HWND h, f; char title[48];
    REQUIRE(CWEA); REQUIRE(U(FindWindowA)); REQUIRE_MH();
    mh_reset();
    sprintf(title, "t32_msgonly2_%lu_%lu", GetCurrentProcessId(), GetTickCount());
    h = CWEA(0, MH_CLSA, title, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleA(NULL), NULL);
    REQUIRE(h);
    f = U(FindWindowA)(MH_CLSA, title);
    t_note("FindWindowA(message-only) -> %p", f);
    mh_kill(h);
    f = U(FindWindowA)(MH_CLSA, title);
    EXPECT_EQ(f, 0);
}

TEST(messages_msgonly_child_of_message_only) {
    HWND p, c;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, HWND_MESSAGE); REQUIRE(p);
    c = mh_mk(WS_CHILD, 0, p);
    if (c) {
        EXPECT_EQ(U(GetParent)(c), p);
        EXPECT_EQ(SMA(c, WM_REC_A, 1, 1), 2);
    } else t_note("cannot create child of message-only window (err %lu)", GetLastError());
    mh_kill(p);
}

TEST(messages_msgonly_destroy_sequence) {
    HWND h;
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset(); mh_rec = 1;
    h = mh_mk(0, 0, HWND_MESSAGE); REQUIRE(h);
    DESTROYW(h);
    mh_rec = 0;
    EXPECT_EQ(mh_count(WM_NCCREATE), 1);
    EXPECT_EQ(mh_count(WM_CREATE), 1);
    EXPECT_EQ(mh_count(WM_DESTROY), 1);
    EXPECT_EQ(mh_count(WM_NCDESTROY), 1);
}

TEST(messages_msgonly_setparent_roundtrip) {
    HWND h, old;
    REQUIRE(U(SetParent)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    old = U(SetParent)(h, HWND_MESSAGE);
    t_note("SetParent(h, HWND_MESSAGE) returned %p", old);
    if (old) {
        CHECK(!U(IsWindowVisible)(h));
        U(SetParent)(h, NULL);
    }
    mh_kill(h);
}

/* ---- ChangeWindowMessageFilter ---- */
TEST(messages_filter_global_add_remove) {
    UINT m;
    REQUIRE(U(ChangeWindowMessageFilter)); REQUIRE(U(RegisterWindowMessageA));
    m = U(RegisterWindowMessageA)("t32_msgs_filter1"); REQUIRE(m);
    CHECK(U(ChangeWindowMessageFilter)(m, MSGFLT_ADD) == TRUE);
    if (!U(ChangeWindowMessageFilter)(m, MSGFLT_ADD)) t_note("repeated MSGFLT_ADD failed, err %lu", GetLastError());
    CHECK(U(ChangeWindowMessageFilter)(m, MSGFLT_REMOVE) == TRUE);
    if (!U(ChangeWindowMessageFilter)(m, MSGFLT_REMOVE)) t_note("repeated MSGFLT_REMOVE failed, err %lu", GetLastError());
}

TEST(messages_filter_global_user_range) {
    REQUIRE(U(ChangeWindowMessageFilter));
    CHECK(U(ChangeWindowMessageFilter)(WM_USER + 300, MSGFLT_ADD) == TRUE);
    CHECK(U(ChangeWindowMessageFilter)(WM_USER + 300, MSGFLT_REMOVE) == TRUE);
    CHECK(U(ChangeWindowMessageFilter)(WM_APP + 5, MSGFLT_ADD) == TRUE);
    CHECK(U(ChangeWindowMessageFilter)(WM_APP + 5, MSGFLT_REMOVE) == TRUE);
}

TEST(messages_filter_global_invalid_flag) {
    UINT m; BOOL r; DWORD bad[] = { 0, 3, 4, 99, 0xFFFFFFFF };
    int i;
    REQUIRE(U(ChangeWindowMessageFilter)); REQUIRE(U(RegisterWindowMessageA));
    m = U(RegisterWindowMessageA)("t32_msgs_filter2"); REQUIRE(m);
    for (i = 0; i < 5; i++) {
        SETE(); r = U(ChangeWindowMessageFilter)(m, bad[i]);
        t_note("ChangeWindowMessageFilter flag %lx error %lu", bad[i], GetLastError());
    }
}

TEST(messages_filter_global_low_messages_observed) {
    UINT msgs[] = { WM_NULL, WM_COPYDATA, 0x49, WM_DROPFILES, WM_CLOSE, WM_GETTEXT };
    int i;
    REQUIRE(U(ChangeWindowMessageFilter));
    for (i = 0; i < 6; i++) {
        BOOL r; SETE();
        r = U(ChangeWindowMessageFilter)(msgs[i], MSGFLT_ADD);
        t_note("ChangeWindowMessageFilter(0x%x, ADD) -> %d err %lu", msgs[i], r, GetLastError());
        if (r) U(ChangeWindowMessageFilter)(msgs[i], MSGFLT_REMOVE);
    }
}

TEST(messages_filter_ex_window_allow_disallow_reset) {
    HWND h; UINT m; CHANGEFILTERSTRUCT cfs;
    REQUIRE(U(ChangeWindowMessageFilterEx)); REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    m = U(RegisterWindowMessageA)("t32_msgs_filter3"); REQUIRE(m);
    memset(&cfs, 0, sizeof cfs); cfs.cbSize = sizeof cfs; cfs.ExtStatus = 0xFFFF;
    CHECK(U(ChangeWindowMessageFilterEx)(h, m, MSGFLT_ALLOW, &cfs) == TRUE);
    t_note("ALLOW first: ExtStatus %lu", cfs.ExtStatus);
    cfs.ExtStatus = 0xFFFF;
    CHECK(U(ChangeWindowMessageFilterEx)(h, m, MSGFLT_ALLOW, &cfs) == TRUE);
    t_note("ALLOW again: ExtStatus %lu", cfs.ExtStatus);
    cfs.ExtStatus = 0xFFFF;
    CHECK(U(ChangeWindowMessageFilterEx)(h, m, MSGFLT_DISALLOW, &cfs) == TRUE);
    t_note("DISALLOW: ExtStatus %lu", cfs.ExtStatus);
    CHECK(U(ChangeWindowMessageFilterEx)(h, m, MSGFLT_RESET, NULL) == TRUE);
    CHECK(U(ChangeWindowMessageFilterEx)(h, m, MSGFLT_ALLOW, NULL) == TRUE);
    CHECK(U(ChangeWindowMessageFilterEx)(h, m, MSGFLT_RESET, NULL) == TRUE);
    mh_kill(h);
}

TEST(messages_filter_ex_invalid_params) {
    HWND h; UINT m; CHANGEFILTERSTRUCT cfs; BOOL r;
    REQUIRE(U(ChangeWindowMessageFilterEx)); REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    m = U(RegisterWindowMessageA)("t32_msgs_filter4"); REQUIRE(m);
    SETE(); r = U(ChangeWindowMessageFilterEx)((HWND)0xDEADBEEF, m, MSGFLT_ALLOW, NULL);
    CHECK(r == FALSE);
    t_note("ChangeWindowMessageFilterEx invalid hwnd error %lu", GetLastError());
    SETE(); r = U(ChangeWindowMessageFilterEx)(h, m, 7, NULL);
    t_note("ChangeWindowMessageFilterEx action 7 error %lu", GetLastError());
    memset(&cfs, 0, sizeof cfs); cfs.cbSize = 3;
    SETE(); r = U(ChangeWindowMessageFilterEx)(h, m, MSGFLT_ALLOW, &cfs);
    t_note("ChangeWindowMessageFilterEx bad cbSize -> %d error %lu", r, GetLastError());
    U(ChangeWindowMessageFilterEx)(h, m, MSGFLT_RESET, NULL);
    mh_kill(h);
}

TEST(messages_filter_ex_null_hwnd_process_wide) {
    UINT m;
    REQUIRE(U(ChangeWindowMessageFilterEx)); REQUIRE(U(RegisterWindowMessageA));
    m = U(RegisterWindowMessageA)("t32_msgs_filter5"); REQUIRE(m);
    BOOL r; SETE();
    r = U(ChangeWindowMessageFilterEx)(NULL, m, MSGFLT_ALLOW, NULL);
    t_note("ChangeWindowMessageFilterEx(NULL hwnd) -> %d err %lu", r, GetLastError());
    if (r) U(ChangeWindowMessageFilterEx)(NULL, m, MSGFLT_RESET, NULL);
}

TEST(messages_filter_does_not_block_same_process_messages) {
    HWND h; UINT m;
    REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    m = U(RegisterWindowMessageA)("t32_msgs_filter6"); REQUIRE(m);
    mh_rec = 1;
    SMA(h, m, 1, 2); PMA(h, m, 3, 4); mh_drain();
    mh_rec = 0;
    EXPECT_EQ(mh_count(m), 2);
    mh_kill(h);
}

/* ---- Broadcast ---- */
TEST(messages_broadcast_system_message_ignore_current) {
    UINT m; DWORD rec; LONG r;
    REQUIRE(U(BroadcastSystemMessageA)); REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    m = U(RegisterWindowMessageA)("t32_msgs_bsm1"); REQUIRE(m);
    rec = BSM_APPLICATIONS;
    SETE();
    r = U(BroadcastSystemMessageA)(BSF_IGNORECURRENTTASK | BSF_POSTMESSAGE, &rec, m, 0, 0);
    CHECK(r != -1);
    t_note("BroadcastSystemMessageA(IGNORECURRENTTASK|POSTMESSAGE) -> %ld rec=%lx err=%lu", r, rec, GetLastError());
    mh_drain();
}

TEST(messages_broadcast_system_message_reaches_own_window) {
    UINT m; DWORD rec; LONG r; HWND h;
    REQUIRE(U(BroadcastSystemMessageA)); REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    m = U(RegisterWindowMessageA)("t32_msgs_bsm2"); REQUIRE(m);
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    rec = BSM_APPLICATIONS;
    mh_rec = 1;
    r = U(BroadcastSystemMessageA)(BSF_NOHANG | BSF_FORCEIFHUNG, &rec, m, 7, 8);
    mh_drain();
    mh_rec = 0;
    t_note("BroadcastSystemMessageA(own) -> %ld", r);
    CHECK(r != -1);
    CHECK(mh_count(m) >= 1);
    mh_kill(h);
}

static LRESULT hk_bsm_deny(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == (UINT)mh_flag[3]) { *hd = 1; return BROADCAST_QUERY_DENY; }
    return 0;
}
TEST(messages_broadcast_query_deny) {
    UINT m; DWORD rec; LONG r; HWND h;
    REQUIRE(U(BroadcastSystemMessageA)); REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    m = U(RegisterWindowMessageA)("t32_msgs_bsm3"); REQUIRE(m);
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_flag[3] = (LONG)m;
    mh_hook = hk_bsm_deny;
    rec = BSM_APPLICATIONS;
    SETE();
    r = U(BroadcastSystemMessageA)(BSF_QUERY | BSF_NOHANG, &rec, m, 0, 0);
    mh_hook = NULL; mh_flag[3] = 0;
    CHECK(r == 0);
    t_note("BSF_QUERY with denying window -> %ld, rec %lx", r, rec);
    mh_kill(h);
}

TEST(messages_broadcast_exw_variants) {
    UINT m; DWORD rec; LONG r;
    REQUIRE(U(BroadcastSystemMessageExW)); REQUIRE(U(BroadcastSystemMessageExA)); REQUIRE(U(RegisterWindowMessageA)); REQUIRE_MH();
    mh_reset();
    m = U(RegisterWindowMessageA)("t32_msgs_bsm4"); REQUIRE(m);
    rec = BSM_APPLICATIONS;
    r = U(BroadcastSystemMessageExW)(BSF_IGNORECURRENTTASK | BSF_NOHANG, &rec, m, 0, 0, NULL);
    CHECK(r != -1);
    rec = BSM_APPLICATIONS;
    r = U(BroadcastSystemMessageExA)(BSF_IGNORECURRENTTASK | BSF_NOHANG, &rec, m, 0, 0, NULL);
    CHECK(r != -1);
    t_note("BroadcastSystemMessageEx NULL info -> %ld", r);
}

TEST(messages_broadcast_system_message_bad_flags) {
    UINT m; DWORD rec; LONG r;
    REQUIRE(U(BroadcastSystemMessageA)); REQUIRE(U(RegisterWindowMessageA));
    m = U(RegisterWindowMessageA)("t32_msgs_bsm5"); REQUIRE(m);
    rec = BSM_APPLICATIONS;
    SETE(); r = U(BroadcastSystemMessageA)(0x80000000u, &rec, m, 0, 0);
    t_note("BroadcastSystemMessage bad flags -> %ld err %lu", r, GetLastError());
    CHECK(r <= 0);
}

/* ---- message numeric ranges ---- */
static LRESULT hk_echo_range(HWND h, UINT m, WPARAM w, LPARAM l, int *hd) {
    if (m == WM_APP || m == WM_APP + 1 || m == 0xBFFF || m == 0xC000 || m == 0xC001 || m == 0xFFFF || m == 0x10000 || m == 0x1FFFF || m == WM_USER + 100) {
        *hd = 1; return (LRESULT)(m ^ (w + 1));
    }
    return 0;
}
TEST(messages_range_user_app_registered_boundaries) {
    HWND h; UINT m[] = { WM_USER + 100, WM_APP, WM_APP + 1, 0xBFFF, 0xC000, 0xC001, 0xFFFF, 0x10000, 0x1FFFF }; int i;
    REQUIRE(SMA); REQUIRE(SMW); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    mh_hook = hk_echo_range;
    for (i = 0; i < 9; i++) {
        LRESULT r = SMA(h, m[i], 5, 0);
        CHECKF(r == (LRESULT)(m[i] ^ 6), "msg %x -> %llx", m[i], (unsigned long long)r);
        r = SMW(h, m[i], 9, 0);
        CHECKF(r == (LRESULT)(m[i] ^ 10), "W msg %x -> %llx", m[i], (unsigned long long)r);
    }
    mh_hook = NULL;
    mh_kill(h);
}

TEST(messages_range_post_boundaries_preserved) {
    HWND h; UINT m[] = { WM_USER, WM_USER + 1, WM_APP, 0xBFFF, 0xC000, 0xFFFF }; int i; MSG msg;
    REQUIRE(PMA); REQUIRE(PKA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    for (i = 0; i < 6; i++) {
        CHECKF(PMA(h, m[i], i, ~(LPARAM)i), "post %x", m[i]);
        CHECK(PKA(&msg, NULL, 0, 0, PM_REMOVE));
        EXPECT_EQ(msg.message, m[i]); EXPECT_EQ(msg.wParam, i); EXPECT_EQ(msg.lParam, ~(LPARAM)i);
    }
    mh_kill(h);
}

TEST(messages_range_post_high_bits_observed) {
    HWND h; BOOL r; MSG m;
    REQUIRE(PMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    SETE(); r = PMA(h, 0x20000, 0, 0);
    t_note("PostMessage msg 0x20000 -> %d err %lu", r, GetLastError());
    SETE(); r = PMA(h, 0x10000, 0, 0);
    t_note("PostMessage msg 0x10000 -> %d err %lu", r, GetLastError());
    while (PKA(&m, NULL, 0, 0, PM_REMOVE)) {}
    mh_kill(h);
}

TEST(messages_range_wm_user_constants) {
    CHECK(WM_USER == 0x400);
    CHECK(WM_APP == 0x8000);
    CHECK(WM_NULL == 0 && WM_QUIT == 0x12 && WM_COPYDATA == 0x4A);
}

/* ---- cross-thread pointer params within a process ---- */
TEST(messages_user_message_pointer_not_marshalled_in_process) {
    MH_TH t; char buf[32]; DWORD_PTR r = 0;
    REQUIRE_MH();
    mh_reset();
    if (!mh_th_start(&t, 0, 0)) { CHECK(0); mh_th_stop(&t); return; }
    memset(buf, 0, sizeof buf);
    CHECK(mh_xsend(t.hwnd, WM_USER + 10, 0, (LPARAM)buf, &r));
    EXPECT_EQ(r, 11);
    EXPECT_STR(buf, "from-worker");
    mh_th_stop(&t);
}

/* ---- A/W window procedures ---- */
TEST(messages_aw_setwindowlongptr_changes_unicode) {
    HWND h; LONG_PTR old, cur; int k;
    REQUIRE(U(SetWindowLongPtrW)); REQUIRE(U(SetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrA)); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    CHECK(!U(IsWindowUnicode)(h));
    old = U(SetWindowLongPtrW)(h, GWLP_WNDPROC, (LONG_PTR)mh_pW);
    CHECK(old != 0);
    CHECK(U(IsWindowUnicode)(h));
    mh_rec = 1;
    SMA(h, WM_SETTEXT, 0, (LPARAM)"flip");
    mh_rec = 0;
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0 && !memcmp(mh_ev[k].raw, L"flip", 10));
    cur = U(SetWindowLongPtrA)(h, GWLP_WNDPROC, (LONG_PTR)mh_pA);
    CHECK(cur != 0);
    CHECK(!U(IsWindowUnicode)(h));
    mh_clear(); mh_rec = 1;
    SMW(h, WM_SETTEXT, 0, (LPARAM)L"back");
    mh_rec = 0;
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0 && !memcmp(mh_ev[k].raw, "back", 5));
    mh_kill(h);
}

TEST(messages_aw_getwindowlongptr_returns_proc) {
    HWND a, w; LONG_PTR pa;
    REQUIRE(U(GetWindowLongPtrA)); REQUIRE(U(GetWindowLongPtrW)); REQUIRE_MH();
    mh_reset();
    a = mh_mk(0, 0, NULL); w = mh_mkw(0, 0, NULL); REQUIRE(a && w);
    pa = U(GetWindowLongPtrA)(a, GWLP_WNDPROC);
    EXPECT_EQ(pa, (LONG_PTR)mh_pA);
    EXPECT_EQ(U(GetWindowLongPtrW)(w, GWLP_WNDPROC), (LONG_PTR)mh_pW);
    t_note("GetWindowLongPtrW on ANSI window -> %llx (proc %p)", (unsigned long long)U(GetWindowLongPtrW)(a, GWLP_WNDPROC), mh_pA);
    t_note("GetWindowLongPtrA on wide window -> %llx (proc %p)", (unsigned long long)U(GetWindowLongPtrA)(w, GWLP_WNDPROC), mh_pW);
    mh_kill(a); mh_kill(w);
}

TEST(messages_aw_callwindowproc_converts_via_window_proc_handle) {
    HWND w; LONG_PTR p; int k;
    REQUIRE(U(CallWindowProcA)); REQUIRE(U(GetWindowLongPtrA)); REQUIRE_MH();
    mh_reset();
    w = mh_mkw(0, 0, NULL); REQUIRE(w);
    p = U(GetWindowLongPtrA)(w, GWLP_WNDPROC);
    mh_rec = 1;
    U(CallWindowProcA)((WNDPROC)p, w, WM_SETTEXT, 0, (LPARAM)"callA");
    mh_rec = 0;
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0);
    if (k >= 0) CHECK(!memcmp(mh_ev[k].raw, L"callA", 12));
    mh_kill(w);
}

TEST(messages_aw_callwindowproc_wide_to_ansi_window) {
    HWND a; LONG_PTR p; int k;
    REQUIRE(U(CallWindowProcW)); REQUIRE(U(GetWindowLongPtrW)); REQUIRE_MH();
    mh_reset();
    a = mh_mk(0, 0, NULL); REQUIRE(a);
    p = U(GetWindowLongPtrW)(a, GWLP_WNDPROC);
    mh_rec = 1;
    U(CallWindowProcW)((WNDPROC)p, a, WM_SETTEXT, 0, (LPARAM)L"callW");
    mh_rec = 0;
    k = mh_find(WM_SETTEXT, 0);
    CHECK(k >= 0);
    if (k >= 0) CHECK(!memcmp(mh_ev[k].raw, "callW", 6));
    mh_kill(a);
}

TEST(messages_aw_post_char_parity) {
    HWND a, w; int ka, kw;
    REQUIRE(PMA); REQUIRE(PMW); REQUIRE_MH();
    mh_reset();
    a = mh_mk(0, 0, NULL); w = mh_mkw(0, 0, NULL); REQUIRE(a && w);
    mh_rec = 1;
    PMA(w, WM_CHAR, 'x', 0);
    PMW(a, WM_CHAR, 'y', 0);
    mh_drain();
    mh_rec = 0;
    ka = -1; kw = -1;
    { int i; for (i = 0; i < mh_nev; i++) if (mh_ev[i].m == WM_CHAR) { if (mh_ev[i].h == w) kw = i; if (mh_ev[i].h == a) ka = i; } }
    CHECK(kw >= 0 && mh_ev[kw].w == 'x');
    CHECK(ka >= 0 && mh_ev[ka].w == 'y');
    mh_kill(a); mh_kill(w);
}

TEST(messages_aw_dispatch_message_a_w_reach_same_proc) {
    HWND w; MSG m;
    REQUIRE(U(DispatchMessageA)); REQUIRE(U(DispatchMessageW)); REQUIRE_MH();
    mh_reset();
    w = mh_mkw(0, 0, NULL); REQUIRE(w);
    memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_REC_A; m.wParam = 4; m.lParam = 5;
    EXPECT_EQ(U(DispatchMessageA)(&m), 9);
    EXPECT_EQ(U(DispatchMessageW)(&m), 9);
    mh_kill(w);
}

TEST(messages_aw_defwindowproc_a_w_gettext) {
    HWND h; char ab[16]; WCHAR wb[16]; LRESULT r;
    REQUIRE(DWPA); REQUIRE(DWPW); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    U(SetWindowTextA)(h, "defproc");
    memset(ab, 0, sizeof ab); memset(wb, 0, sizeof wb);
    r = DWPA(h, WM_GETTEXT, 16, (LPARAM)ab);
    EXPECT_EQ(r, 7); EXPECT_STR(ab, "defproc");
    r = DWPW(h, WM_GETTEXT, 16, (LPARAM)wb);
    EXPECT_EQ(r, 7); EXPECT_WSTR(wb, L"defproc");
    EXPECT_EQ(DWPA(h, WM_GETTEXTLENGTH, 0, 0), 7);
    EXPECT_EQ(DWPW(h, WM_GETTEXTLENGTH, 0, 0), 7);
    mh_kill(h);
}

TEST(messages_aw_defwindowproc_wm_null_and_unknown) {
    HWND h;
    REQUIRE(DWPA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    EXPECT_EQ(DWPA(h, WM_NULL, 1, 2), 0);
    EXPECT_EQ(DWPW(h, WM_NULL, 1, 2), 0);
    EXPECT_EQ(DWPA(h, WM_USER + 500, 1, 2), 0);
    EXPECT_EQ(DWPA(h, 0xC123, 1, 2), 0);
    EXPECT_EQ(DWPA(h, WM_QUERYOPEN, 0, 0), 1);
    EXPECT_EQ(DWPA(h, WM_ERASEBKGND, 0, 0), 0);
    EXPECT_EQ(DWPA(h, WM_NCHITTEST, 0, MAKELPARAM(-32000, -32000)), HTNOWHERE);
    mh_kill(h);
}

TEST(messages_aw_defwindowproc_syscommand_unknown_returns_zero) {
    HWND h;
    REQUIRE(DWPA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    EXPECT_EQ(DWPA(h, WM_SYSCOMMAND, 0xF0F0 + 1, 0), 0);
    mh_kill(h);
}

TEST(messages_aw_defwindowproc_invalid_hwnd) {
    LRESULT r;
    REQUIRE(DWPA);
    SETE(); r = DWPA((HWND)0xDEADBEEF, WM_NULL, 0, 0);
    EXPECT_EQ(r, 0);
    SETE(); r = DWPW((HWND)0xDEADBEEF, WM_GETTEXTLENGTH, 0, 0);
    EXPECT_EQ(r, 0);
}

TEST(messages_defwindowproc_wm_setredraw_and_getdlgcode) {
    HWND h;
    REQUIRE(SMA); REQUIRE_MH();
    mh_reset();
    h = mh_mk(0, 0, NULL); REQUIRE(h);
    EXPECT_EQ(SMA(h, WM_GETDLGCODE, 0, 0), 0);
    EXPECT_EQ(SMA(h, WM_SETREDRAW, TRUE, 0), 0);
    mh_kill(h);
}

TEST(messages_wm_gettext_on_unnamed_window_class_defaults) {
    HWND h; char b[8] = "zz";
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    h = CWEA(0, MH_CLSA, NULL, 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    REQUIRE(h);
    EXPECT_EQ(SMA(h, WM_GETTEXTLENGTH, 0, 0), 0);
    EXPECT_EQ(SMA(h, WM_GETTEXT, 8, (LPARAM)b), 0);
    EXPECT_EQ(b[0], 0);
    mh_kill(h);
}

TEST(messages_wm_setfont_on_edit_then_gettext_unaffected) {
    HWND p, e; char b[16];
    REQUIRE(CWEA); REQUIRE_MH();
    mh_reset();
    p = mh_mk(0, 0, NULL); REQUIRE(p);
    e = CWEA(0, "EDIT", "edit-text", WS_CHILD, 0, 0, 80, 20, p, NULL, GetModuleHandleA(NULL), NULL);
    if (!e) { t_note("no EDIT class"); mh_kill(p); return; }
    EXPECT_EQ(SMA(e, WM_GETTEXTLENGTH, 0, 0), 9);
    EXPECT_EQ(SMA(e, WM_GETTEXT, 16, (LPARAM)b), 9);
    EXPECT_STR(b, "edit-text");
    EXPECT_EQ(SMA(e, WM_SETTEXT, 0, (LPARAM)"zz"), TRUE);
    EXPECT_EQ(SMA(e, WM_GETTEXTLENGTH, 0, 0), 2);
    EXPECT_EQ(SMW(e, WM_GETTEXTLENGTH, 0, 0), 2);
    mh_kill(p);
}
