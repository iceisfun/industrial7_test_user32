/* Extra hooks, accelerator/menu interaction and DDEML negative tests. */
#include "dh.h"
#include <wchar.h>

#define X2CLS "T32X2Wnd"
static volatile LONG x2_cmd, x2_sys;
static WPARAM x2_cmd_wp;
static LRESULT CALLBACK x2_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND) { x2_cmd++; x2_cmd_wp = w; return 0; }
    if (m == WM_SYSCOMMAND) { x2_sys++; return 0; }
    return DefWindowProcA(h, m, w, l);
}
static void x2_kill(HWND h) { dh_destroy_window(h, X2CLS); }

/* ---------- accelerator whose command is a menu item ---------- */
static HWND x2_menu_window(HMENU *pm) {
    HMENU m; HWND h;
    x2_cmd = x2_sys = 0; x2_cmd_wp = 0;
    h = dh_create_window(X2CLS, x2_proc, WS_OVERLAPPEDWINDOW, 0, NULL);
    if (!h) return NULL;
    m = U(CreateMenu)();
    U(AppendMenuA)(m, MF_STRING, 500, "Item");
    if (!U(SetMenu)(h, m)) { U(DestroyMenu)(m); m = NULL; }
    *pm = m;
    return h;
}
static LRESULT x2_translate(HWND h, HACCEL acc, WPARAM vk) {
    MSG m; memset(&m, 0, sizeof m);
    m.hwnd = h; m.message = WM_KEYDOWN; m.wParam = vk; m.lParam = 1; m.time = GetTickCount();
    return U(TranslateAcceleratorA)(h, acc, &m);
}
TEST(accel_x2_menu_item_enabled_sends_command_with_accel_flag) {
    HMENU hm; HWND h; HACCEL acc; LRESULT r;
    ACCEL a __attribute__((aligned(8))) = { FVIRTKEY, 'M', 500 };
    REQUIRE(U(CreateAcceleratorTableA)); REQUIRE(U(TranslateAcceleratorA)); REQUIRE(U(CreateMenu)); REQUIRE(U(AppendMenuA)); REQUIRE(U(SetMenu));
    REQUIRE(U(DestroyAcceleratorTable)); REQUIRE(U(RegisterClassA)); REQUIRE(U(CreateWindowExA));
    h = x2_menu_window(&hm); REQUIRE(h);
    acc = U(CreateAcceleratorTableA)(&a, 1);
    CHECK(acc != NULL);
    r = x2_translate(h, acc, 'M');
    CHECK(r != 0);
    EQ(x2_cmd, 1);
    EQ(LOWORD(x2_cmd_wp), 500);
    EQ(HIWORD(x2_cmd_wp), 1);          /* 1 = came from an accelerator, 0 = from a menu */
    if (acc) U(DestroyAcceleratorTable)(acc);
    x2_kill(h);
}
TEST(accel_x2_menu_item_disabled_sends_no_command) {
    HMENU hm; HWND h; HACCEL acc; LRESULT r;
    ACCEL a __attribute__((aligned(8))) = { FVIRTKEY, 'M', 500 };
    REQUIRE(U(CreateAcceleratorTableA)); REQUIRE(U(TranslateAcceleratorA)); REQUIRE(U(CreateMenu)); REQUIRE(U(AppendMenuA)); REQUIRE(U(SetMenu));
    REQUIRE(U(EnableMenuItem)); REQUIRE(U(DestroyAcceleratorTable)); REQUIRE(U(RegisterClassA)); REQUIRE(U(CreateWindowExA));
    h = x2_menu_window(&hm); REQUIRE(h);
    if (!hm) { t_skip("no menu"); x2_kill(h); return; }
    acc = U(CreateAcceleratorTableA)(&a, 1);
    CHECK(acc != NULL);
    U(EnableMenuItem)(hm, 500, MF_BYCOMMAND | MF_GRAYED);
    r = x2_translate(h, acc, 'M');
    t_note("TranslateAccelerator for a grayed menu command -> %lld", (long long)r);
    EQ(x2_cmd, 0);                      /* documented: a disabled menu item receives no command */
    EQ(x2_sys, 0);
    U(EnableMenuItem)(hm, 500, MF_BYCOMMAND | MF_ENABLED);
    r = x2_translate(h, acc, 'M');
    CHECK(r != 0);
    EQ(x2_cmd, 1);
    if (acc) U(DestroyAcceleratorTable)(acc);
    x2_kill(h);
}

/* ---------- CallNextHookEx ignores its hhk argument ---------- */
static volatile LONG x2_inner_calls, x2_outer_calls;
static HHOOK x2_h_outer;
static HHOOK x2_bogus_hhk;
static LRESULT CALLBACK x2_inner_proc(int c, WPARAM w, LPARAM l) {
    if (c >= 0) x2_inner_calls++;
    return U(CallNextHookEx)(NULL, c, w, l);
}
static LRESULT CALLBACK x2_outer_proc(int c, WPARAM w, LPARAM l) {
    if (c >= 0) x2_outer_calls++;
    return U(CallNextHookEx)(x2_bogus_hhk, c, w, l);
}
static void x2_callnext_case(HHOOK bogus) {
    HHOOK h_in, h_out; HWND w; MSG m;
    x2_inner_calls = x2_outer_calls = 0; x2_bogus_hhk = bogus;
    w = dh_create_window(X2CLS, x2_proc, WS_OVERLAPPEDWINDOW, 0, NULL); REQUIRE(w);
    h_in = U(SetWindowsHookExA)(WH_GETMESSAGE, x2_inner_proc, NULL, GetCurrentThreadId());
    h_out = U(SetWindowsHookExA)(WH_GETMESSAGE, x2_outer_proc, NULL, GetCurrentThreadId());
    CHECK(h_in != NULL && h_out != NULL);
    x2_h_outer = h_out;
    PostMessageA(w, WM_USER + 40, 0, 0);
    CHECK(PeekMessageA(&m, w, WM_USER + 40, WM_USER + 40, PM_REMOVE));
    EQ(x2_outer_calls, 1);
    EQ(x2_inner_calls, 1);              /* documented: the hhk parameter is ignored */
    if (h_out) U(UnhookWindowsHookEx)(h_out);
    if (h_in) U(UnhookWindowsHookEx)(h_in);
    x2_kill(w);
}
TEST(hook_x2_callnexthookex_null_hhk_still_chains) {
    REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(CallNextHookEx)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(U(RegisterClassA)); REQUIRE(U(CreateWindowExA));
    x2_callnext_case(NULL);
}
TEST(hook_x2_callnexthookex_garbage_hhk_still_chains) {
    REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(CallNextHookEx)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(U(RegisterClassA)); REQUIRE(U(CreateWindowExA));
    x2_callnext_case((HHOOK)(ULONG_PTR)0x12345678);
}

/* ---------- WH_HARDWARE (obsolete) ---------- */
static LRESULT CALLBACK x2_pass(int c, WPARAM w, LPARAM l) { return U(CallNextHookEx)(NULL, c, w, l); }
TEST(hook_x2_wh_hardware_install_observed) {
    HHOOK h;
    REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(UnhookWindowsHookEx));
    SetLastError(NOERROR);
    h = U(SetWindowsHookExA)(8 /* WH_HARDWARE */, x2_pass, NULL, GetCurrentThreadId());
    t_note("SetWindowsHookExA(WH_HARDWARE, this thread) -> %p err=%lu", h, GetLastError());
    if (h) EQ(U(UnhookWindowsHookEx)(h), TRUE);
}

/* ---------- a hook installed by a thread that then exits ---------- */
static volatile LONG x2_orphan_calls;
static LRESULT CALLBACK x2_orphan_proc(int c, WPARAM w, LPARAM l) {
    if (c >= 0) x2_orphan_calls++;
    return U(CallNextHookEx)(NULL, c, w, l);
}
typedef struct { DWORD target; HHOOK h; } X2ORPH;
static DWORD WINAPI x2_orphan_thread(LPVOID p) {
    X2ORPH *o = (X2ORPH *)p;
    o->h = U(SetWindowsHookExA)(WH_GETMESSAGE, x2_orphan_proc, NULL, o->target);
    return 0;
}
TEST(hook_x2_hook_installed_by_exited_thread_observed) {
    X2ORPH o; HANDLE th; HWND w; MSG m; BOOL un;
    REQUIRE(U(SetWindowsHookExA)); REQUIRE(U(UnhookWindowsHookEx)); REQUIRE(U(RegisterClassA)); REQUIRE(U(CreateWindowExA));
    x2_orphan_calls = 0; o.target = GetCurrentThreadId(); o.h = NULL;
    w = dh_create_window(X2CLS, x2_proc, WS_OVERLAPPEDWINDOW, 0, NULL); REQUIRE(w);
    th = CreateThread(NULL, 0, x2_orphan_thread, &o, 0, NULL); REQUIRE(th);
    WaitForSingleObject(th, 5000); CloseHandle(th);
    CHECK(o.h != NULL);
    PostMessageA(w, WM_USER + 41, 0, 0);
    CHECK(PeekMessageA(&m, w, WM_USER + 41, WM_USER + 41, PM_REMOVE));
    t_note("hook installed by an exited thread: %ld call(s) on retrieval", (long)x2_orphan_calls);
    SetLastError(NOERROR);
    un = o.h ? U(UnhookWindowsHookEx)(o.h) : -1;
    t_note("UnhookWindowsHookEx(hook of exited thread) -> %d err=%lu", un, GetLastError());
    x2_kill(w);
}

/* ---------- shell hook window registration ---------- */
TEST(hook_x2_shell_hook_window_registration) {
    HWND w; BOOL r; UINT msg;
    REQUIRE(U(RegisterShellHookWindow)); REQUIRE(U(DeregisterShellHookWindow)); REQUIRE(U(RegisterWindowMessageA));
    REQUIRE(U(RegisterClassA)); REQUIRE(U(CreateWindowExA));
    msg = U(RegisterWindowMessageA)("SHELLHOOK");
    CHECK(msg >= 0xC000 && msg <= 0xFFFF);
    SetLastError(NOERROR);
    r = U(RegisterShellHookWindow)(NULL);
    CHECK(r == FALSE);
    t_note("RegisterShellHookWindow(NULL) err=%lu", GetLastError());
    SetLastError(NOERROR);
    r = U(DeregisterShellHookWindow)(NULL);
    CHECK(r == FALSE);
    t_note("DeregisterShellHookWindow(NULL) err=%lu", GetLastError());
    w = dh_create_window(X2CLS, x2_proc, WS_OVERLAPPEDWINDOW, 0, NULL); REQUIRE(w);
    SetLastError(NOERROR);
    r = U(RegisterShellHookWindow)(w);
    t_note("RegisterShellHookWindow(own window) -> %d err=%lu", r, GetLastError());
    if (r) {
        SetLastError(NOERROR);
        EQ(U(DeregisterShellHookWindow)(w), TRUE);
        SetLastError(NOERROR);
        r = U(DeregisterShellHookWindow)(w);
        t_note("second DeregisterShellHookWindow -> %d err=%lu", r, GetLastError());
    }
    x2_kill(w);
}

/* ---------- DDEML: calls with an instance id that was never issued ---------- */
TEST(dde_x2_bogus_instance_id_calls_fail) {
    DWORD ids[2] = { 0, 0x7ead1234 }; int i;
    REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeKeepStringHandle)); REQUIRE(U(DdeQueryStringA));
    REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeConnect)); REQUIRE(U(DdeNameService)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeGetLastError));
    for (i = 0; i < 2; i++) {
        DWORD id = ids[i]; char buf[16] = "zz";
        HSZ h;
        SetLastError(NOERROR);
        h = U(DdeCreateStringHandleA)(id, "T32X2Bogus", CP_WINANSI);
        EQ(h, 0);
        EQ(U(DdeFreeStringHandle)(id, (HSZ)(ULONG_PTR)0x1234), FALSE);
        EQ(U(DdeKeepStringHandle)(id, (HSZ)(ULONG_PTR)0x1234), FALSE);
        EQ(U(DdeQueryStringA)(id, (HSZ)(ULONG_PTR)0x1234, buf, sizeof buf, CP_WINANSI), 0);
        EQ(U(DdeUninitialize)(id), FALSE);
        EQ(U(DdeConnect)(id, (HSZ)(ULONG_PTR)0x1234, (HSZ)(ULONG_PTR)0x1235, NULL), 0);
        EQ(U(DdeNameService)(id, (HSZ)(ULONG_PTR)0x1234, 0, DNS_REGISTER), 0);
        EQ(U(DdeCreateDataHandle)(id, (LPBYTE)"abcd", 4, 0, 0, CF_TEXT, 0), 0);
        t_note("DdeGetLastError(idInst=0x%lx) = 0x%lx", id, U(DdeGetLastError)(id));
    }
}
TEST(dde_x2_reinitialize_with_unknown_instance_fails) {
    DWORD id; UINT r;
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    id = 0x00510001;                           /* never handed out */
    r = U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD | APPCMD_CLIENTONLY, 0);
    CHECK(r != DMLERR_NO_ERROR);
    t_note("DdeInitializeA(nonzero unknown *pidInst) -> 0x%x (DMLERR_INVALIDPARAMETER=0x4006)", r);
    if (r == 0) U(DdeUninitialize)(id);
}
TEST(dde_x2_initialize_reserved_nonzero_observed) {
    DWORD id = 0; UINT r;
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    r = U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD | APPCMD_CLIENTONLY, 0x1234);
    t_note("DdeInitializeA(ulReserved=0x1234) -> 0x%x", r);
    if (r == 0) U(DdeUninitialize)(id);
}

/* ---------- DDEML: string handle length limit (atom-backed, 255 chars) ---------- */
TEST(dde_x2_string_handle_length_limit) {
    DWORD id = 0; char s[300]; HSZ h255, h256; DWORD e;
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetLastError));
    EQ(U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD | APPCMD_CLIENTONLY, 0), 0);
    memset(s, 'q', sizeof s); s[255] = 0;
    h255 = U(DdeCreateStringHandleA)(id, s, CP_WINANSI);
    CHECK(h255 != NULL);                       /* 255 characters is the documented maximum */
    s[255] = 'q'; s[256] = 0;
    h256 = U(DdeCreateStringHandleA)(id, s, CP_WINANSI);
    e = U(DdeGetLastError)(id);
    t_note("256-character string handle -> %p, DdeGetLastError=0x%lx", h256, e);
    if (h256) U(DdeFreeStringHandle)(id, h256);
    if (h255) U(DdeFreeStringHandle)(id, h255);
    U(DdeUninitialize)(id);
}

/* ---------- DDEML: unregister all service names with a NULL name ---------- */
static volatile LONG x2_conn_cb;
static HDDEDATA CALLBACK x2_srv_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA d, ULONG_PTR d1, ULONG_PTR d2) {
    if (t == XTYP_CONNECT) x2_conn_cb++;
    return NULL;                               /* refuse: we only count connect attempts */
}
TEST(dde_x2_nameservice_null_name_unregisters_all) {
    DWORD srv = 0, cli = 0; HSZ n1, n2; HCONV c; LONG before, after;
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle));
    REQUIRE(U(DdeNameService)); REQUIRE(U(DdeConnect));
    if (dh_is_wine()) { t_skip("DDEML timing under Wine"); return; }
    EQ(U(DdeInitializeA)(&srv, x2_srv_cb, APPCLASS_STANDARD, 0), 0);
    EQ(U(DdeInitializeA)(&cli, dh_null_cb, APPCLASS_STANDARD | APPCMD_CLIENTONLY, 0), 0);
    n1 = U(DdeCreateStringHandleA)(srv, "T32X2SvcOne", CP_WINANSI);
    n2 = U(DdeCreateStringHandleA)(srv, "T32X2SvcTwo", CP_WINANSI);
    CHECK(n1 && n2);
    CHECK(U(DdeNameService)(srv, n1, 0, DNS_REGISTER) != 0);
    CHECK(U(DdeNameService)(srv, n2, 0, DNS_REGISTER) != 0);
    x2_conn_cb = 0;
    {
        HSZ topic = U(DdeCreateStringHandleA)(cli, "T32X2Topic", CP_WINANSI);
        HSZ svc1 = U(DdeCreateStringHandleA)(cli, "T32X2SvcOne", CP_WINANSI);
        HSZ svc2 = U(DdeCreateStringHandleA)(cli, "T32X2SvcTwo", CP_WINANSI);
        c = U(DdeConnect)(cli, svc1, topic, NULL);
        if (c) U(DdeDisconnect)(c);
        before = x2_conn_cb;
        CHECK(before >= 1);                    /* registered: the server callback saw XTYP_CONNECT */
        CHECK(U(DdeNameService)(srv, 0, 0, DNS_UNREGISTER) != 0);
        c = U(DdeConnect)(cli, svc1, topic, NULL);
        CHECK(c == NULL);
        c = U(DdeConnect)(cli, svc2, topic, NULL);
        CHECK(c == NULL);
        after = x2_conn_cb;
        EQ(after, before);                     /* unregistered: no XTYP_CONNECT delivered any more */
        if (topic) U(DdeFreeStringHandle)(cli, topic);
        if (svc1) U(DdeFreeStringHandle)(cli, svc1);
        if (svc2) U(DdeFreeStringHandle)(cli, svc2);
    }
    if (n1) U(DdeFreeStringHandle)(srv, n1);
    if (n2) U(DdeFreeStringHandle)(srv, n2);
    U(DdeUninitialize)(cli);
    U(DdeUninitialize)(srv);
}

/* ---------- DDEML: CBF_FAIL_SELFCONNECTIONS ---------- */
TEST(dde_x2_fail_selfconnections_blocks_own_service) {
    DWORD id = 0; HSZ svc, topic; HCONV c;
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle));
    REQUIRE(U(DdeNameService)); REQUIRE(U(DdeConnect));
    if (dh_is_wine()) { t_skip("DDEML timing under Wine"); return; }
    EQ(U(DdeInitializeA)(&id, x2_srv_cb, APPCLASS_STANDARD | CBF_FAIL_SELFCONNECTIONS, 0), 0);
    svc = U(DdeCreateStringHandleA)(id, "T32X2SelfSvc", CP_WINANSI);
    topic = U(DdeCreateStringHandleA)(id, "T32X2Topic", CP_WINANSI);
    CHECK(U(DdeNameService)(id, svc, 0, DNS_REGISTER) != 0);
    x2_conn_cb = 0;
    c = U(DdeConnect)(id, svc, topic, NULL);
    CHECK(c == NULL);
    EQ(x2_conn_cb, 0);                         /* the DLL must not offer the connect to this same instance */
    t_note("self connect with CBF_FAIL_SELFCONNECTIONS: err=0x%lx", U(DdeGetLastError)(id));
    if (c) U(DdeDisconnect)(c);
    U(DdeNameService)(id, svc, 0, DNS_UNREGISTER);
    if (svc) U(DdeFreeStringHandle)(id, svc);
    if (topic) U(DdeFreeStringHandle)(id, topic);
    U(DdeUninitialize)(id);
}
