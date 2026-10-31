/* DDEML: in-process server + client conversations (two instances on one thread). */
#include "dh.h"

#define SVC "T32DdeSvc"
#define TOP "T32Topic"
#define ITM "T32Item"

typedef struct {
    int who;            /* 0 = server, 1 = client, 2 = monitor */
    UINT type, fmt;
    HCONV conv;
    ULONG_PTR d1, d2;
    HDDEDATA hdata;
    char s1[96], s2[96];
    BYTE data[96];
    DWORD dlen;
} EV;
#define MAXEV 512
static EV g_ev[MAXEV];
static int g_nev;

static DWORD g_srv, g_cli, g_mon;
static HSZ g_hsvc, g_htop, g_hitm;       /* created by the client instance */
static HCONV g_conv;                      /* client side conversation */
static HCONV g_srvconv;                   /* server side conversation (seen in callbacks) */

/* server behaviour knobs */
static int s_connect_ok;
static int s_req_mode;                    /* 0 data, 1 NULL, 2 CBR_BLOCK */
static int s_xact_ret;                    /* value returned for POKE/EXECUTE */
static int s_advstart_ok;
static int s_advreq_n;
static int s_in_reqs;

static void hsz_str(DWORD inst, HSZ h, char *out, size_t n) {
    out[0] = 0;
    if (h && inst) U(DdeQueryStringA)(inst, h, out, (DWORD)n, CP_WINANSI);
}

static void log_ev(int who, UINT type, UINT fmt, HCONV c, HSZ h1, HSZ h2, HDDEDATA hd, ULONG_PTR d1, ULONG_PTR d2) {
    if (g_nev >= MAXEV) return;
    EV *e = &g_ev[g_nev++];
    memset(e, 0, sizeof *e);
    e->who = who; e->type = type; e->fmt = fmt; e->conv = c; e->d1 = d1; e->d2 = d2; e->hdata = hd;
    DWORD inst = who == 0 ? g_srv : who == 1 ? g_cli : 0;
    if (who != 2 && type != XTYP_ERROR) {
        hsz_str(inst, h1, e->s1, sizeof e->s1);
        hsz_str(inst, h2, e->s2, sizeof e->s2);
        if (hd && (type == XTYP_POKE || type == XTYP_EXECUTE || type == XTYP_ADVDATA || type == XTYP_XACT_COMPLETE)) {
            e->dlen = U(DdeGetData)(hd, e->data, sizeof e->data - 1, 0);
        }
    }
}

static HDDEDATA mkdata(DWORD inst, const char *s, HSZ item, UINT fmt) {
    return U(DdeCreateDataHandle)(inst, (LPBYTE)s, (DWORD)strlen(s) + 1, 0, item, fmt, 0);
}

static HDDEDATA CALLBACK srv_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA hd, ULONG_PTR d1, ULONG_PTR d2) {
    log_ev(0, t, f, c, h1, h2, hd, d1, d2);
    switch (t) {
    case XTYP_CONNECT: {
        char tp[64]; hsz_str(g_srv, h1, tp, sizeof tp);
        return (HDDEDATA)(ULONG_PTR)(s_connect_ok && !lstrcmpiA(tp, TOP) ? 1 : 0);
    }
    case XTYP_CONNECT_CONFIRM: g_srvconv = c; return NULL;
    case XTYP_REQUEST:
        if (s_req_mode == 1) return NULL;
        if (s_req_mode == 2) { s_req_mode = 0; g_srvconv = c; return (HDDEDATA)(LONG_PTR)-1; }  /* CBR_BLOCK once */
        return mkdata(g_srv, "reqdata", h2, f);
    case XTYP_POKE: case XTYP_EXECUTE: return (HDDEDATA)(ULONG_PTR)s_xact_ret;
    case XTYP_ADVSTART: return (HDDEDATA)(ULONG_PTR)(s_advstart_ok ? 1 : 0);
    case XTYP_ADVREQ: { char b[32]; sprintf(b, "adv%d", ++s_advreq_n); return mkdata(g_srv, b, h2, f); }
    case XTYP_DISCONNECT: return NULL;
    }
    return NULL;
}
static HDDEDATA CALLBACK cli_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA hd, ULONG_PTR d1, ULONG_PTR d2) {
    log_ev(1, t, f, c, h1, h2, hd, d1, d2);
    if (t == XTYP_ADVDATA) return (HDDEDATA)(ULONG_PTR)DDE_FACK;
    return NULL;
}
static HDDEDATA CALLBACK mon_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA hd, ULONG_PTR d1, ULONG_PTR d2) {
    log_ev(2, t, f, c, h1, h2, hd, d1, d2);
    return NULL;
}

static int count_ev(int who, UINT type) {
    int n = 0;
    for (int i = 0; i < g_nev; i++) if (g_ev[i].who == who && g_ev[i].type == type) n++;
    return n;
}
static EV *find_ev(int who, UINT type, int nth) {
    for (int i = 0; i < g_nev; i++) if (g_ev[i].who == who && g_ev[i].type == type && nth-- == 0) return &g_ev[i];
    return NULL;
}
/* pump until at least n events of the kind exist, bounded */
static int wait_ev(int who, UINT type, int n, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    while (count_ev(who, type) < n && (LONG)(GetTickCount() - end) < 0) dh_sleep_pump(10);
    return count_ev(who, type) >= n;
}

static int have_dde(void) {
    return U(DdeInitializeA) && U(DdeUninitialize) && U(DdeConnect) && U(DdeNameService) && U(DdeCreateStringHandleA) &&
           U(DdeClientTransaction) && U(DdeQueryStringA) && U(DdeGetData) && U(DdeFreeStringHandle) && U(DdeDisconnect) &&
           U(DdeCreateDataHandle) && U(DdeGetLastError) && U(DdeFreeDataHandle) && U(DdeQueryConvInfo);
}

/* srv_flags/cli_flags: DdeInitialize flags.  connect != 0 establishes g_conv. */
static int setup(DWORD srv_flags, DWORD cli_flags, int connect) {
    g_nev = 0; g_conv = g_srvconv = NULL; g_srv = g_cli = g_mon = 0;
    s_connect_ok = 1; s_req_mode = 0; s_xact_ret = DDE_FACK; s_advstart_ok = 1; s_advreq_n = 0;
    if (U(DdeInitializeA)(&g_srv, srv_cb, srv_flags, 0) != 0) return 0;
    if (U(DdeInitializeA)(&g_cli, cli_cb, cli_flags, 0) != 0) return 0;
    g_hsvc = U(DdeCreateStringHandleA)(g_cli, SVC, CP_WINANSI);
    g_htop = U(DdeCreateStringHandleA)(g_cli, TOP, CP_WINANSI);
    g_hitm = U(DdeCreateStringHandleA)(g_cli, ITM, CP_WINANSI);
    if (!g_hsvc || !g_htop || !g_hitm) return 0;
    if (!(srv_flags & APPCMD_CLIENTONLY)) {
        if (!U(DdeNameService)(g_srv, g_hsvc, NULL, DNS_REGISTER)) return 0;
    }
    if (connect) {
        g_conv = U(DdeConnect)(g_cli, g_hsvc, g_htop, NULL);
        if (!g_conv) return 0;
        dh_sleep_pump(20);
    }
    return 1;
}
static void teardown(void) {
    if (g_conv) { U(DdeDisconnect)(g_conv); g_conv = NULL; }
    dh_sleep_pump(30);
    if (g_srv && g_hsvc) U(DdeNameService)(g_srv, NULL, NULL, DNS_UNREGISTER);
    if (g_cli) {
        if (g_hsvc) U(DdeFreeStringHandle)(g_cli, g_hsvc);
        if (g_htop) U(DdeFreeStringHandle)(g_cli, g_htop);
        if (g_hitm) U(DdeFreeStringHandle)(g_cli, g_hitm);
    }
    g_hsvc = g_htop = g_hitm = NULL;
    dh_sleep_pump(10);
    if (g_mon) U(DdeUninitialize)(g_mon);
    if (g_cli) U(DdeUninitialize)(g_cli);
    if (g_srv) U(DdeUninitialize)(g_srv);
    g_srv = g_cli = g_mon = 0;
}
#define SETUP(sf, cf, conn) do { if (!have_dde()) { t_skip("missing DDEML exports"); return; } \
    if (!setup(sf, cf, conn)) { CHECKF(0, "setup failed (conv=%p)", g_conv); teardown(); return; } } while (0)

TEST(ddeconv_connect_basic) {
    SETUP(0, 0, 1);
    CHECK(g_conv != NULL);
    EV *e = find_ev(0, XTYP_CONNECT, 0);
    CHECK(e != NULL);
    if (e) {
        EXPECT_STR(e->s1, TOP);
        EXPECT_STR(e->s2, SVC);
        EQ(e->conv, NULL);
        CHECK(e->d1 != 0);                 /* PCONVCONTEXT */
        EQ(e->d2, 0);                      /* different instance */
    }
    CHECK(wait_ev(0, XTYP_CONNECT_CONFIRM, 1, 1000));
    e = find_ev(0, XTYP_CONNECT_CONFIRM, 0);
    if (e) {
        EXPECT_STR(e->s1, TOP);
        EXPECT_STR(e->s2, SVC);
        CHECK(e->conv != NULL);
        EQ(e->d2, 0);
    }
    teardown();
}
TEST(ddeconv_connect_rejected) {
    SETUP(0, 0, 0);
    s_connect_ok = 0;
    EQ(U(DdeConnect)(g_cli, g_hsvc, g_htop, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_NO_CONV_ESTABLISHED);
    CHECK(count_ev(0, XTYP_CONNECT) >= 1);
    EQ(count_ev(0, XTYP_CONNECT_CONFIRM), 0);
    teardown();
}
TEST(ddeconv_connect_wrong_topic) {
    SETUP(0, 0, 0);
    HSZ bad = U(DdeCreateStringHandleA)(g_cli, "T32WrongTopic", CP_WINANSI);
    HCONV c = U(DdeConnect)(g_cli, g_hsvc, bad, NULL);
    EQ(c, NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_NO_CONV_ESTABLISHED);
    CHECK(count_ev(0, XTYP_CONNECT) >= 1);
    if (c) U(DdeDisconnect)(c);
    U(DdeFreeStringHandle)(g_cli, bad);
    teardown();
}
TEST(ddeconv_connect_unregistered_service) {
    SETUP(0, 0, 0);
    U(DdeNameService)(g_srv, g_hsvc, NULL, DNS_UNREGISTER);
    dh_sleep_pump(20);
    EQ(U(DdeConnect)(g_cli, g_hsvc, g_htop, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_NO_CONV_ESTABLISHED);
    EQ(count_ev(0, XTYP_CONNECT), 0);
    teardown();
}
TEST(ddeconv_connect_context_passed) {
    SETUP(0, 0, 0);
    CONVCONTEXT cc; memset(&cc, 0, sizeof cc);
    cc.cb = sizeof cc; cc.iCodePage = CP_WINANSI; cc.wCountryID = 1; cc.dwLangID = 0x0409;
    g_conv = U(DdeConnect)(g_cli, g_hsvc, g_htop, &cc);
    CHECK(g_conv != NULL);
    EV *e = find_ev(0, XTYP_CONNECT, 0);
    CHECK(e != NULL);
    if (e && e->d1) {
        CONVCONTEXT *p = (CONVCONTEXT *)e->d1;
        t_note("server saw CONVCONTEXT cb=%lu cp=%d country=%lu lang=0x%lx", p->cb, p->iCodePage, p->wCountryID, p->dwLangID);
    }
    teardown();
}
TEST(ddeconv_request_sync) {
    SETUP(0, 0, 1);
    DWORD res = 0xAAAA;
    HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, &res);
    CHECK(d != NULL);
    if (d) {
        BYTE b[32] = {0};
        DWORD n = U(DdeGetData)(d, b, 32, 0);
        CHECK(n >= 8);
        CHECK(!memcmp(b, "reqdata", 8));
        EQ(U(DdeFreeDataHandle)(d), TRUE);
    }
    EV *e = find_ev(0, XTYP_REQUEST, 0);
    CHECK(e != NULL);
    if (e) {
        EQ(e->fmt, CF_TEXT);
        EXPECT_STR(e->s1, TOP);
        EXPECT_STR(e->s2, ITM);
        CHECK(e->conv != NULL);
    }
    t_note("sync request pdwResult=0x%lx", res);
    teardown();
}
TEST(ddeconv_request_formats) {
    SETUP(0, 0, 1);
    static const UINT fmts[] = {CF_TEXT, CF_UNICODETEXT, CF_PRIVATEFIRST, CF_DIB, 0xC123};
    for (unsigned i = 0; i < sizeof fmts / sizeof fmts[0]; i++) {
        HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, fmts[i], XTYP_REQUEST, 3000, NULL);
        CHECKF(d != NULL, "fmt %u", fmts[i]);
        if (d) U(DdeFreeDataHandle)(d);
        EV *e = find_ev(0, XTYP_REQUEST, (int)i);
        CHECKF(e && e->fmt == fmts[i], "fmt %u logged", fmts[i]);
    }
    teardown();
}
TEST(ddeconv_request_not_processed) {
    SETUP(0, 0, 1);
    s_req_mode = 1;
    DWORD res = 0;
    HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, &res);
    EQ(d, NULL);
    EQ(U(DdeGetLastError)(g_cli), 0x4009);   /* DMLERR_NOTPROCESSED */
    teardown();
}
TEST(ddeconv_request_missing_item) {
    SETUP(0, 0, 1);
    EQ(U(DdeClientTransaction)(NULL, 0, g_conv, NULL, CF_TEXT, XTYP_REQUEST, 1000, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeClientTransaction)(NULL, 0, g_conv, NULL, CF_TEXT, XTYP_ADVSTART, 1000, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeClientTransaction)(NULL, 0, g_conv, NULL, CF_TEXT, XTYP_ADVSTOP, 1000, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeClientTransaction)((LPBYTE)"x", 2, g_conv, NULL, CF_TEXT, XTYP_POKE, 1000, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_INVALIDPARAMETER);
    teardown();
}
TEST(ddeconv_bad_transaction_type) {
    SETUP(0, 0, 1);
    static const UINT types[] = {XTYP_CONNECT, XTYP_DISCONNECT, XTYP_REGISTER, XTYP_ADVREQ, XTYP_XACT_COMPLETE, 0, 0x1234, XTYP_ADVDATA};
    for (unsigned i = 0; i < sizeof types / sizeof types[0]; i++) {
        U(DdeGetLastError)(g_cli);
        HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, types[i], 500, NULL);
        CHECKF(d == NULL, "type 0x%x", types[i]);
        CHECKF(U(DdeGetLastError)(g_cli) == E_DMLERR_INVALIDPARAMETER, "type 0x%x", types[i]);
    }
    EQ(count_ev(0, XTYP_REQUEST), 0);
    teardown();
}
TEST(ddeconv_request_timeout_blocked) {
    SETUP(0, 0, 1);
    s_req_mode = 2;   /* server answers CBR_BLOCK the first time */
    HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 300, NULL);
    EQ(d, NULL);
    DWORD e = U(DdeGetLastError)(g_cli);
    t_note("blocked request error 0x%lx", e);
    CHECK(e != 0);
    if (g_srvconv) U(DdeEnableCallback)(g_srv, g_srvconv, EC_ENABLEALL);
    dh_sleep_pump(100);
    teardown();
}
TEST(ddeconv_poke) {
    SETUP(0, 0, 1);
    DWORD res = 0;
    HDDEDATA r = U(DdeClientTransaction)((LPBYTE)"pokedata", 9, g_conv, g_hitm, CF_TEXT, XTYP_POKE, 3000, &res);
    CHECK(r != NULL);
    EQ(res, DDE_FACK);
    EV *e = find_ev(0, XTYP_POKE, 0);
    CHECK(e != NULL);
    if (e) {
        EQ(e->fmt, CF_TEXT);
        EXPECT_STR(e->s1, TOP);
        EXPECT_STR(e->s2, ITM);
        CHECK(e->dlen >= 9);
        CHECK(!memcmp(e->data, "pokedata", 9));
    }
    teardown();
}
TEST(ddeconv_poke_not_processed) {
    SETUP(0, 0, 1);
    s_xact_ret = DDE_FNOTPROCESSED;
    DWORD res = 0xFFFF;
    HDDEDATA r = U(DdeClientTransaction)((LPBYTE)"pokedata", 9, g_conv, g_hitm, CF_TEXT, XTYP_POKE, 3000, &res);
    EQ(r, NULL);
    EQ(U(DdeGetLastError)(g_cli), 0x4009);
    t_note("poke NAK pdwResult=0x%lx", res);
    teardown();
}
TEST(ddeconv_poke_busy) {
    SETUP(0, 0, 1);
    s_xact_ret = DDE_FBUSY;
    DWORD res = 0;
    HDDEDATA r = U(DdeClientTransaction)((LPBYTE)"pokedata", 9, g_conv, g_hitm, CF_TEXT, XTYP_POKE, 3000, &res);
    EQ(r, NULL);
    DWORD e = U(DdeGetLastError)(g_cli);
    t_note("poke busy error 0x%lx res=0x%lx", e, res);
    CHECK(e == 0x4001 || e == 0x4009);
    teardown();
}
TEST(ddeconv_poke_data_handle) {
    SETUP(0, 0, 1);
    HDDEDATA d = U(DdeCreateDataHandle)(g_cli, (LPBYTE)"viahandle", 10, 0, g_hitm, CF_TEXT, 0);
    CHECK(d != NULL);
    DWORD res = 0;
    HDDEDATA r = U(DdeClientTransaction)((LPBYTE)d, 0xFFFFFFFF, g_conv, g_hitm, CF_TEXT, XTYP_POKE, 3000, &res);
    CHECK(r != NULL);
    EV *e = find_ev(0, XTYP_POKE, 0);
    CHECK(e && e->dlen >= 10 && !memcmp(e->data, "viahandle", 10));
    teardown();
}
TEST(ddeconv_execute) {
    SETUP(0, 0, 1);
    static const char cmd[] = "[open(\"x.txt\")][close()]";
    DWORD res = 0;
    HDDEDATA r = U(DdeClientTransaction)((LPBYTE)cmd, sizeof cmd, g_conv, NULL, 0, XTYP_EXECUTE, 3000, &res);
    CHECK(r != NULL);
    EQ(res, DDE_FACK);
    EV *e = find_ev(0, XTYP_EXECUTE, 0);
    CHECK(e != NULL);
    if (e) {
        EXPECT_STR(e->s1, TOP);
        CHECK(e->dlen >= sizeof cmd);
        CHECK(!memcmp(e->data, cmd, sizeof cmd));
        EQ(e->s2[0], 0);
    }
    teardown();
}
TEST(ddeconv_execute_rejected) {
    SETUP(0, 0, 1);
    s_xact_ret = DDE_FNOTPROCESSED;
    HDDEDATA r = U(DdeClientTransaction)((LPBYTE)"[nope]", 7, g_conv, NULL, 0, XTYP_EXECUTE, 3000, NULL);
    EQ(r, NULL);
    EQ(U(DdeGetLastError)(g_cli), 0x4009);
    teardown();
}
TEST(ddeconv_execute_unicode_instance) {
    if (!have_dde() || !U(DdeInitializeW)) { t_skip("missing exports"); return; }
    setup(0, 0, 0);
    DWORD wc = 0;
    UINT r = U(DdeInitializeW)(&wc, cli_cb, 0, 0);
    CHECK(r == 0);
    if (r == 0) {
        HSZ s = U(DdeCreateStringHandleW)(wc, L"" SVC, CP_WINUNICODE);
        HSZ t = U(DdeCreateStringHandleW)(wc, L"" TOP, CP_WINUNICODE);
        EQ(s, g_hsvc);
        HCONV c = U(DdeConnect)(wc, s, t, NULL);
        CHECK(c != NULL);
        if (c) {
            static const WCHAR cmd[] = L"[wide(1)]";
            HDDEDATA x = U(DdeClientTransaction)((LPBYTE)cmd, sizeof cmd, c, NULL, 0, XTYP_EXECUTE, 3000, NULL);
            t_note("unicode execute -> %p", x);
            EV *e = find_ev(0, XTYP_EXECUTE, 0);
            if (e) t_note("server received %lu bytes: first=0x%02x 0x%02x", e->dlen, e->data[0], e->data[1]);
            U(DdeDisconnect)(c);
        }
        U(DdeFreeStringHandle)(wc, s); U(DdeFreeStringHandle)(wc, t);
        dh_sleep_pump(20);
        U(DdeUninitialize)(wc);
    }
    teardown();
}
TEST(ddeconv_advise_hot) {
    SETUP(0, 0, 1);
    DWORD res = 0;
    HDDEDATA r = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_ADVSTART, 3000, &res);
    CHECK(r != NULL);
    EV *e = find_ev(0, XTYP_ADVSTART, 0);
    CHECK(e != NULL);
    if (e) { EXPECT_STR(e->s1, TOP); EXPECT_STR(e->s2, ITM); EQ(e->fmt, CF_TEXT); }
    EQ(U(DdePostAdvise)(g_srv, g_htop, g_hitm), TRUE);
    CHECK(wait_ev(1, XTYP_ADVDATA, 1, 2000));
    CHECK(count_ev(0, XTYP_ADVREQ) >= 1);
    e = find_ev(1, XTYP_ADVDATA, 0);
    if (e) {
        EXPECT_STR(e->s1, TOP); EXPECT_STR(e->s2, ITM);
        EQ(e->fmt, CF_TEXT);
        CHECK(e->hdata != NULL);
        CHECK(e->dlen >= 5 && !memcmp(e->data, "adv1", 5));
    }
    /* second post */
    EQ(U(DdePostAdvise)(g_srv, g_htop, g_hitm), TRUE);
    CHECK(wait_ev(1, XTYP_ADVDATA, 2, 2000));
    e = find_ev(1, XTYP_ADVDATA, 1);
    CHECK(e && e->dlen >= 5 && !memcmp(e->data, "adv2", 5));
    /* stop */
    r = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_ADVSTOP, 3000, &res);
    CHECK(r != NULL);
    CHECK(count_ev(0, XTYP_ADVSTOP) >= 1);
    int before = count_ev(1, XTYP_ADVDATA);
    U(DdePostAdvise)(g_srv, g_htop, g_hitm);
    dh_sleep_pump(150);
    EQ(count_ev(1, XTYP_ADVDATA), before);
    teardown();
}
TEST(ddeconv_advise_warm_nodata) {
    SETUP(0, 0, 1);
    HDDEDATA r = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_ADVSTART | XTYPF_NODATA, 3000, NULL);
    CHECK(r != NULL);
    EV *e = find_ev(0, XTYP_ADVSTART, 0);
    CHECK(e != NULL);
    if (e) CHECK((e->fmt & XTYPF_NODATA) != 0);
    EQ(U(DdePostAdvise)(g_srv, g_htop, g_hitm), TRUE);
    CHECK(wait_ev(1, XTYP_ADVDATA, 1, 2000));
    e = find_ev(1, XTYP_ADVDATA, 0);
    if (e) { EQ(e->hdata, NULL); }
    EQ(count_ev(0, XTYP_ADVREQ), 0);       /* no data requested for a warm link until the client asks */
    teardown();
}
TEST(ddeconv_advise_refused) {
    SETUP(0, 0, 1);
    s_advstart_ok = 0;
    HDDEDATA r = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_ADVSTART, 3000, NULL);
    EQ(r, NULL);
    DWORD e = U(DdeGetLastError)(g_cli);
    t_note("refused advise error 0x%lx", e);
    CHECK(e != 0);
    teardown();
}
TEST(ddeconv_postadvise_no_links) {
    SETUP(0, 0, 1);
    U(DdeGetLastError)(g_srv);
    BOOL r = U(DdePostAdvise)(g_srv, g_htop, g_hitm);
    t_note("DdePostAdvise with no links -> %d", r);
    EQ(count_ev(1, XTYP_ADVDATA), 0);
    teardown();
}
TEST(ddeconv_postadvise_wildcard_item) {
    SETUP(0, 0, 1);
    U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_ADVSTART, 3000, NULL);
    BOOL r = U(DdePostAdvise)(g_srv, g_htop, NULL);    /* NULL item: all items of the topic */
    t_note("PostAdvise(NULL item) -> %d", r);
    CHECK(wait_ev(1, XTYP_ADVDATA, 1, 2000));
    teardown();
}
TEST(ddeconv_async_request) {
    SETUP(0, 0, 1);
    DWORD id = 0;
    HDDEDATA r = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, TIMEOUT_ASYNC, &id);
    CHECK(r != NULL);
    CHECK(id != 0);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    if (!count_ev(1, XTYP_XACT_COMPLETE)) {
        UINT q = U(DdeQueryConvInfo)(g_conv, id, &ci);
        t_note("pending transaction query -> %u, wType=0x%x wStatus=0x%x", q, ci.wType, ci.wStatus);
    }
    CHECK(wait_ev(1, XTYP_XACT_COMPLETE, 1, 3000));
    EV *e = find_ev(1, XTYP_XACT_COMPLETE, 0);
    if (e) {
        EQ(e->d1, id);
        CHECK(e->hdata != NULL);
        CHECK(e->dlen >= 8 && !memcmp(e->data, "reqdata", 8));
        EXPECT_STR(e->s2, ITM);
        CHECK(e->conv == g_conv);
    }
    teardown();
}
TEST(ddeconv_async_unique_ids) {
    SETUP(0, 0, 1);
    DWORD a = 0, b = 0, c = 0;
    CHECK(U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, TIMEOUT_ASYNC, &a));
    CHECK(U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, TIMEOUT_ASYNC, &b));
    CHECK(U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, TIMEOUT_ASYNC, &c));
    CHECK(a != 0 && b != 0 && c != 0);
    CHECK(a != b && b != c && a != c);
    CHECK(wait_ev(1, XTYP_XACT_COMPLETE, 3, 4000));
    teardown();
}
TEST(ddeconv_async_poke_and_execute) {
    SETUP(0, 0, 1);
    DWORD a = 0, b = 0;
    CHECK(U(DdeClientTransaction)((LPBYTE)"p", 2, g_conv, g_hitm, CF_TEXT, XTYP_POKE, TIMEOUT_ASYNC, &a));
    CHECK(U(DdeClientTransaction)((LPBYTE)"[x]", 4, g_conv, NULL, 0, XTYP_EXECUTE, TIMEOUT_ASYNC, &b));
    CHECK(a != 0 && b != 0 && a != b);
    CHECK(wait_ev(1, XTYP_XACT_COMPLETE, 2, 4000));
    EV *e = find_ev(1, XTYP_XACT_COMPLETE, 0);
    if (e) CHECK(e->d1 == a || e->d1 == b);
    CHECK(count_ev(0, XTYP_POKE) >= 1 && count_ev(0, XTYP_EXECUTE) >= 1);
    teardown();
}
TEST(ddeconv_async_abandon) {
    SETUP(0, 0, 1);
    DWORD id = 0;
    CHECK(U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, TIMEOUT_ASYNC, &id));
    EQ(U(DdeAbandonTransaction)(g_cli, g_conv, id), TRUE);
    dh_sleep_pump(200);
    EQ(count_ev(1, XTYP_XACT_COMPLETE), 0);
    /* abandoning again / unknown id fails */
    U(DdeGetLastError)(g_cli);
    EQ(U(DdeAbandonTransaction)(g_cli, g_conv, id), FALSE);
    EQ(U(DdeAbandonTransaction)(g_cli, g_conv, 0x7777), FALSE);
    DWORD ge = U(DdeGetLastError)(g_cli);
    CHECK(ge == E_DMLERR_INVALIDPARAMETER || ge == 0x4011);
    teardown();
}
TEST(ddeconv_abandon_wrong_instance) {
    SETUP(0, 0, 1);
    DWORD id = 0;
    CHECK(U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, TIMEOUT_ASYNC, &id));
    EQ(U(DdeAbandonTransaction)(g_srv, g_conv, id), FALSE);
    EQ(U(DdeAbandonTransaction)(g_cli, g_conv, id), TRUE);
    teardown();
}
TEST(ddeconv_queryconvinfo_sync) {
    SETUP(0, 0, 1);
    CONVINFO ci; memset(&ci, 0xCC, sizeof ci); ci.cb = sizeof ci;
    UINT q = U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci);
    CHECK(q != 0);
    t_note("DdeQueryConvInfo returned %u (sizeof CONVINFO=%u)", q, (unsigned)sizeof ci);
    CHECK((ci.wStatus & ST_CONNECTED) != 0);
    CHECK((ci.wStatus & ST_CLIENT) != 0);
    CHECK((ci.wStatus & ST_ISLOCAL) != 0);
    CHECK((ci.wStatus & ST_TERMINATED) == 0);
    EQ(ci.wConvst, XST_CONNECTED);
    EQ(ci.wType, 0);
    EQ(ci.wLastError, 0);
    CHECK(ci.hConvPartner != NULL || 1);
    EQ(ci.hszSvcPartner, g_hsvc);
    EQ(ci.hszTopic, g_htop);
    EQ(ci.ConvCtxt.cb, sizeof(CONVCONTEXT));
    t_note("conv codepage=%d hwnd=%p partner=%p", ci.ConvCtxt.iCodePage, ci.hwnd, ci.hwndPartner);
    teardown();
}
TEST(ddeconv_queryconvinfo_server_side) {
    SETUP(0, 0, 1);
    CHECK(wait_ev(0, XTYP_CONNECT_CONFIRM, 1, 1000));
    CHECK(g_srvconv != NULL);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    UINT q = U(DdeQueryConvInfo)(g_srvconv, QID_SYNC, &ci);
    CHECK(q != 0);
    CHECK((ci.wStatus & ST_CONNECTED) != 0);
    CHECK((ci.wStatus & ST_CLIENT) == 0);
    t_note("server conv status=0x%x", ci.wStatus);
    teardown();
}
TEST(ddeconv_queryconvinfo_cb_sizes) {
    SETUP(0, 0, 1);
    CONVINFO ci; 
    memset(&ci, 0xCC, sizeof ci); ci.cb = sizeof ci + 1;
    U(DdeGetLastError)(g_cli);
    EQ(U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci), 0);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_INVALIDPARAMETER);
    memset(&ci, 0xCC, sizeof ci); ci.cb = 8;
    UINT q = U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci);
    CHECK(q != 0);
    EQ(ci.cb, 8);
    BYTE *raw = (BYTE *)&ci;
    EQ(raw[8], 0xCC);      /* bytes beyond cb untouched */
    EQ(raw[sizeof ci - 1], 0xCC);
    teardown();
}
TEST(ddeconv_queryconvinfo_bad_transaction) {
    SETUP(0, 0, 1);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    U(DdeGetLastError)(g_cli);
    EQ(U(DdeQueryConvInfo)(g_conv, 0x4242, &ci), 0);
    DWORD ge = U(DdeGetLastError)(g_cli);
    CHECK(ge == E_DMLERR_INVALIDPARAMETER || ge == 0x4011);   /* 0x4011 = DMLERR_UNFOUND_QUEUE_ID */
    t_note("bad transaction id error 0x%lx", ge);
    teardown();
}
TEST(ddeconv_user_handle) {
    SETUP(0, 0, 1);
    EQ(U(DdeSetUserHandle)(g_conv, QID_SYNC, 0xABCD1234), TRUE);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    CHECK(U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci));
    EQ(ci.hUser, 0xABCD1234);
    EQ(U(DdeSetUserHandle)(g_conv, QID_SYNC, 0), TRUE);
    CHECK(U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci));
    EQ(ci.hUser, 0);
    EQ(U(DdeSetUserHandle)(g_conv, 0x5555, 1), FALSE);
    teardown();
}
TEST(ddeconv_user_handle_per_transaction) {
    SETUP(0, 0, 1);
    DWORD id = 0;
    CHECK(U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, TIMEOUT_ASYNC, &id));
    if (!count_ev(1, XTYP_XACT_COMPLETE)) {
        BOOL s = U(DdeSetUserHandle)(g_conv, id, 0x5151);
        t_note("DdeSetUserHandle on pending transaction -> %d", s);
    }
    CHECK(wait_ev(1, XTYP_XACT_COMPLETE, 1, 3000));
    teardown();
}
TEST(ddeconv_disconnect_client) {
    SETUP(0, 0, 1);
    HCONV c = g_conv;
    EQ(U(DdeDisconnect)(c), TRUE);
    g_conv = NULL;
    CHECK(wait_ev(0, XTYP_DISCONNECT, 1, 2000));
    /* the handle is dead */
    EQ(U(DdeDisconnect)(c), FALSE);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    EQ(U(DdeQueryConvInfo)(c, QID_SYNC, &ci), 0);
    EQ(U(DdeClientTransaction)(NULL, 0, c, g_hitm, CF_TEXT, XTYP_REQUEST, 500, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_INVALIDPARAMETER);
    teardown();
}
TEST(ddeconv_disconnect_server_side) {
    SETUP(0, 0, 1);
    CHECK(wait_ev(0, XTYP_CONNECT_CONFIRM, 1, 1000));
    CHECK(g_srvconv != NULL);
    EQ(U(DdeDisconnect)(g_srvconv), TRUE);
    CHECK(wait_ev(1, XTYP_DISCONNECT, 1, 2000));
    EV *e = find_ev(1, XTYP_DISCONNECT, 0);
    if (e) CHECK(e->conv == g_conv);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    UINT q = U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci);
    t_note("terminated client conv query=%u status=0x%x", q, ci.wStatus);
    CHECK(q != 0);
    CHECK((ci.wStatus & ST_CONNECTED) == 0);
    CHECK((ci.wStatus & ST_TERMINATED) != 0);
    /* a transaction on the terminated conversation fails */
    HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 500, NULL);
    EQ(d, NULL);
    DWORD err = U(DdeGetLastError)(g_cli);
    EQ(err, E_DMLERR_NO_CONV_ESTABLISHED);
    teardown();
}
TEST(ddeconv_reconnect) {
    SETUP(0, 0, 1);
    /* Reconnecting a live conversation yields NULL. */
    EQ(U(DdeReconnect)(g_conv), NULL);
    CHECK(wait_ev(0, XTYP_CONNECT_CONFIRM, 1, 1000));
    if (g_srvconv) {
        U(DdeDisconnect)(g_srvconv);
        CHECK(wait_ev(1, XTYP_DISCONNECT, 1, 2000));
        HCONV n = U(DdeReconnect)(g_conv);
        t_note("DdeReconnect after server disconnect -> %p (old %p)", n, g_conv);
        if (n) {
            CHECK(n != NULL);
            g_conv = n;
            HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, NULL);
            CHECK(d != NULL);
            if (d) U(DdeFreeDataHandle)(d);
        }
    }
    teardown();
}
TEST(ddeconv_connectlist) {
    SETUP(0, 0, 0);
    HCONVLIST l = U(DdeConnectList)(g_cli, g_hsvc, g_htop, NULL, NULL);
    CHECK(l != NULL);
    if (l) {
        HCONV c1 = U(DdeQueryNextServer)(l, NULL);
        CHECK(c1 != NULL);
        HCONV c2 = c1 ? U(DdeQueryNextServer)(l, c1) : NULL;
        EQ(c2, NULL);
        if (c1) {
            CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
            CHECK(U(DdeQueryConvInfo)(c1, QID_SYNC, &ci));
            CHECK((ci.wStatus & ST_INLIST) != 0);
            EQ(ci.hConvList, l);
            HDDEDATA d = U(DdeClientTransaction)(NULL, 0, c1, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, NULL);
            CHECK(d != NULL);
            if (d) U(DdeFreeDataHandle)(d);
        }
        EQ(U(DdeDisconnectList)(l), TRUE);
        EQ(U(DdeDisconnectList)(l), FALSE);
        if (c1) EQ(U(DdeDisconnect)(c1), FALSE);
    }
    dh_sleep_pump(50);
    teardown();
}
TEST(ddeconv_connectlist_no_match_after_unregister) {
    SETUP(0, 0, 0);
    U(DdeNameService)(g_srv, g_hsvc, NULL, DNS_UNREGISTER);
    dh_sleep_pump(20);
    EQ(U(DdeConnectList)(g_cli, g_hsvc, g_htop, NULL, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_NO_CONV_ESTABLISHED);
    teardown();
}
TEST(ddeconv_enablecallback_blocked_state) {
    SETUP(0, 0, 1);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    BOOL r = U(DdeEnableCallback)(g_cli, g_conv, EC_DISABLE);
    t_note("DdeEnableCallback(EC_DISABLE) -> %d", r);
    CHECK(U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci));
    CHECK((ci.wStatus & ST_BLOCKED) != 0);
    r = U(DdeEnableCallback)(g_cli, g_conv, EC_ENABLEALL);
    memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    CHECK(U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci));
    CHECK((ci.wStatus & ST_BLOCKED) == 0);
    teardown();
}
TEST(ddeconv_enablecallback_instance_wide) {
    SETUP(0, 0, 1);
    U(DdeEnableCallback)(g_cli, NULL, EC_DISABLE);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    CHECK(U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci));
    t_note("after instance-wide disable status=0x%x", ci.wStatus);
    CHECK((ci.wStatus & ST_BLOCKED) != 0);
    U(DdeEnableCallback)(g_cli, NULL, EC_ENABLEALL);
    memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    CHECK(U(DdeQueryConvInfo)(g_conv, QID_SYNC, &ci));
    CHECK((ci.wStatus & ST_BLOCKED) == 0);
    teardown();
}
TEST(ddeconv_enablecallback_querywaiting) {
    SETUP(0, 0, 1);
    BOOL w = U(DdeEnableCallback)(g_cli, g_conv, EC_QUERYWAITING);
    t_note("EC_QUERYWAITING (idle) -> %d", w);
    CHECK(w == FALSE);
    teardown();
}
TEST(ddeconv_flag_fail_connections) {
    SETUP(CBF_FAIL_CONNECTIONS, 0, 0);
    EQ(U(DdeConnect)(g_cli, g_hsvc, g_htop, NULL), NULL);
    EQ(U(DdeGetLastError)(g_cli), E_DMLERR_NO_CONV_ESTABLISHED);
    EQ(count_ev(0, XTYP_CONNECT), 0);
    teardown();
}
TEST(ddeconv_flag_fail_requests) {
    SETUP(CBF_FAIL_REQUESTS, 0, 1);
    HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 2000, NULL);
    EQ(d, NULL);
    EQ(count_ev(0, XTYP_REQUEST), 0);
    EQ(U(DdeGetLastError)(g_cli), 0x4009);
    teardown();
}
TEST(ddeconv_flag_fail_pokes) {
    SETUP(CBF_FAIL_POKES, 0, 1);
    HDDEDATA d = U(DdeClientTransaction)((LPBYTE)"x", 2, g_conv, g_hitm, CF_TEXT, XTYP_POKE, 2000, NULL);
    EQ(d, NULL);
    EQ(count_ev(0, XTYP_POKE), 0);
    EQ(U(DdeGetLastError)(g_cli), 0x4009);
    teardown();
}
TEST(ddeconv_flag_fail_executes) {
    SETUP(CBF_FAIL_EXECUTES, 0, 1);
    HDDEDATA d = U(DdeClientTransaction)((LPBYTE)"[x]", 4, g_conv, NULL, 0, XTYP_EXECUTE, 2000, NULL);
    EQ(d, NULL);
    EQ(count_ev(0, XTYP_EXECUTE), 0);
    EQ(U(DdeGetLastError)(g_cli), 0x4009);
    teardown();
}
TEST(ddeconv_flag_fail_advises) {
    SETUP(CBF_FAIL_ADVISES, 0, 1);
    HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_ADVSTART, 2000, NULL);
    EQ(d, NULL);
    EQ(count_ev(0, XTYP_ADVSTART), 0);
    teardown();
}
TEST(ddeconv_flag_fail_allsvrxactions) {
    SETUP(CBF_FAIL_ADVISES | CBF_FAIL_EXECUTES | CBF_FAIL_POKES | CBF_FAIL_REQUESTS, 0, 1);
    EQ(U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 1000, NULL), NULL);
    EQ(U(DdeClientTransaction)((LPBYTE)"x", 2, g_conv, g_hitm, CF_TEXT, XTYP_POKE, 1000, NULL), NULL);
    EQ(U(DdeClientTransaction)((LPBYTE)"[x]", 4, g_conv, NULL, 0, XTYP_EXECUTE, 1000, NULL), NULL);
    EQ(U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_ADVSTART, 1000, NULL), NULL);
    EQ(count_ev(0, XTYP_REQUEST) + count_ev(0, XTYP_POKE) + count_ev(0, XTYP_EXECUTE) + count_ev(0, XTYP_ADVSTART), 0);
    teardown();
}
TEST(ddeconv_flag_skip_connect_confirms) {
    SETUP(CBF_SKIP_CONNECT_CONFIRMS, 0, 1);
    CHECK(g_conv != NULL);
    dh_sleep_pump(150);
    EQ(count_ev(0, XTYP_CONNECT_CONFIRM), 0);
    CHECK(count_ev(0, XTYP_CONNECT) >= 1);
    teardown();
}
TEST(ddeconv_flag_skip_disconnects) {
    SETUP(CBF_SKIP_DISCONNECTS, 0, 1);
    U(DdeDisconnect)(g_conv); g_conv = NULL;
    dh_sleep_pump(200);
    EQ(count_ev(0, XTYP_DISCONNECT), 0);
    teardown();
}
TEST(ddeconv_register_notifications) {
    SETUP(0, 0, 0);
    dh_sleep_pump(150);
    HSZ nn = U(DdeCreateStringHandleA)(g_srv, "T32OtherSvc", CP_WINANSI);
    g_nev = 0;
    EQ(U(DdeNameService)(g_srv, nn, NULL, DNS_REGISTER), (HDDEDATA)1);
    int got = wait_ev(1, XTYP_REGISTER, 1, 1500);
    t_note("XTYP_REGISTER delivered to second instance: %d", got);
    EV *e = find_ev(1, XTYP_REGISTER, 0);
    if (e) EXPECT_STR(e->s1, "T32OtherSvc");
    EQ(U(DdeNameService)(g_srv, nn, NULL, DNS_UNREGISTER), (HDDEDATA)1);
    got = wait_ev(1, XTYP_UNREGISTER, 1, 1500);
    t_note("XTYP_UNREGISTER delivered: %d", got);
    U(DdeFreeStringHandle)(g_srv, nn);
    teardown();
}
TEST(ddeconv_skip_registrations_flag) {
    SETUP(0, CBF_SKIP_REGISTRATIONS | CBF_SKIP_UNREGISTRATIONS, 0);
    HSZ nn = U(DdeCreateStringHandleA)(g_srv, "T32OtherSvc2", CP_WINANSI);
    g_nev = 0;
    U(DdeNameService)(g_srv, nn, NULL, DNS_REGISTER);
    dh_sleep_pump(250);
    U(DdeNameService)(g_srv, nn, NULL, DNS_UNREGISTER);
    dh_sleep_pump(250);
    EQ(count_ev(1, XTYP_REGISTER), 0);
    EQ(count_ev(1, XTYP_UNREGISTER), 0);
    U(DdeFreeStringHandle)(g_srv, nn);
    teardown();
}
TEST(ddeconv_clientonly_cannot_serve) {
    SETUP(APPCMD_CLIENTONLY, 0, 0);
    HSZ s = U(DdeCreateStringHandleA)(g_srv, "T32CoSvc", CP_WINANSI);
    EQ(U(DdeNameService)(g_srv, s, NULL, DNS_REGISTER), NULL);
    EQ(U(DdeGetLastError)(g_srv), E_DMLERR_DLL_USAGE);
    U(DdeFreeStringHandle)(g_srv, s);
    teardown();
}
TEST(ddeconv_clientonly_can_connect) {
    SETUP(0, APPCMD_CLIENTONLY, 1);
    CHECK(g_conv != NULL);
    HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, NULL);
    CHECK(d != NULL);
    if (d) U(DdeFreeDataHandle)(d);
    teardown();
}
TEST(ddeconv_monitor_sees_traffic) {
    if (!have_dde()) { t_skip("missing"); return; }
    g_nev = 0; g_mon = 0;
    UINT r = U(DdeInitializeA)(&g_mon, mon_cb, APPCLASS_MONITOR | MF_CALLBACKS | MF_CONV | MF_ERRORS | MF_HSZ_INFO | MF_LINKS | MF_POSTMSGS | MF_SENDMSGS, 0);
    if (r != 0) { t_note("monitor init failed 0x%x", r); g_mon = 0; return; }
    if (!setup(0, 0, 1)) { CHECKF(0, "setup"); teardown(); return; }
    HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, NULL);
    if (d) U(DdeFreeDataHandle)(d);
    dh_sleep_pump(50);
    int mon = 0;
    for (int i = 0; i < g_nev; i++) if (g_ev[i].who == 2) mon++;
    t_note("monitor callback events seen: %d", mon);
    teardown();
}
TEST(ddeconv_impersonate_client) {
    SETUP(0, 0, 1);
    CHECK(wait_ev(0, XTYP_CONNECT_CONFIRM, 1, 1000));
    if (g_srvconv && U(DdeImpersonateClient)) {
        BOOL ok = U(DdeImpersonateClient)(g_srvconv);
        t_note("DdeImpersonateClient on in-process server conv -> %d (err %lu)", ok, GetLastError());
        if (ok) RevertToSelf();
        /* the client side conversation is not a server conversation */
        EQ(U(DdeImpersonateClient)(g_conv), FALSE);
    }
    teardown();
}
TEST(ddeconv_repeated_requests) {
    SETUP(0, 0, 1);
    int ok = 0;
    for (int i = 0; i < 100; i++) {
        HDDEDATA d = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, NULL);
        if (d) { ok++; U(DdeFreeDataHandle)(d); }
    }
    EQ(ok, 100);
    EQ(count_ev(0, XTYP_REQUEST), 100);
    teardown();
}
TEST(ddeconv_connect_disconnect_cycle) {
    SETUP(0, 0, 0);
    int ok = 0;
    for (int i = 0; i < 25; i++) {
        HCONV c = U(DdeConnect)(g_cli, g_hsvc, g_htop, NULL);
        if (c) { ok++; if (!U(DdeDisconnect)(c)) CHECKF(0, "disconnect %d", i); }
        dh_sleep_pump(5);
    }
    EQ(ok, 25);
    teardown();
}
TEST(ddeconv_two_conversations) {
    SETUP(0, 0, 1);
    HCONV c2 = U(DdeConnect)(g_cli, g_hsvc, g_htop, NULL);
    t_note("second connect to same service/topic -> %p (first %p)", c2, g_conv);
    if (c2) CHECK(c2 != g_conv);
    HDDEDATA a = U(DdeClientTransaction)(NULL, 0, g_conv, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, NULL);
    HDDEDATA b = c2 ? U(DdeClientTransaction)(NULL, 0, c2, g_hitm, CF_TEXT, XTYP_REQUEST, 3000, NULL) : NULL;
    CHECK(a != NULL); if (c2) CHECK(b != NULL);
    if (a) U(DdeFreeDataHandle)(a);
    if (b) U(DdeFreeDataHandle)(b);
    if (c2) U(DdeDisconnect)(c2);
    teardown();
}
TEST(ddeconv_uninitialize_with_open_conv) {
    SETUP(0, 0, 1);
    HCONV c = g_conv; g_conv = NULL;
    DWORD cli = g_cli;
    HSZ hs = g_hsvc, ht = g_htop, hi = g_hitm;
    EQ(U(DdeUninitialize)(cli), TRUE);
    g_cli = 0;
    dh_sleep_pump(50);
    CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
    EQ(U(DdeQueryConvInfo)(c, QID_SYNC, &ci), 0);
    CHECK(wait_ev(0, XTYP_DISCONNECT, 1, 1500));
    g_hsvc = g_htop = g_hitm = NULL;
    (void)hs; (void)ht; (void)hi;
    teardown();
}
TEST(ddeconv_service_name_instance_specific) {
    SETUP(0, 0, 0);
    /* An instance-specific name "Svc(hex)" is also registered; connecting to it works. */
    HCONV c = U(DdeConnect)(g_cli, g_hsvc, g_htop, NULL);
    CHECK(c != NULL);
    if (c) {
        CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
        CHECK(U(DdeQueryConvInfo)(c, QID_SYNC, &ci));
        char buf[96] = {0};
        U(DdeQueryStringA)(g_cli, ci.hszServiceReq, buf, sizeof buf, CP_WINANSI);
        t_note("hszServiceReq = \"%s\"", buf);
        buf[0] = 0;
        U(DdeQueryStringA)(g_cli, ci.hszSvcPartner, buf, sizeof buf, CP_WINANSI);
        t_note("hszSvcPartner = \"%s\"", buf);
        U(DdeDisconnect)(c);
    }
    teardown();
}
