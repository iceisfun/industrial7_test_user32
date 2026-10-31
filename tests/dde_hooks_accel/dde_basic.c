/* DDEML: initialization, error codes, string handles, data handles, parameter validation. */
#include "dh.h"

static int dde_open(DWORD *id, DWORD flags) {
    *id = 0;
    return U(DdeInitializeA)(id, dh_null_cb, flags, 0) == DMLERR_NO_ERROR && *id != 0;
}

TEST(dde_init_a_basic) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    DWORD id = 0;
    UINT r = U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD, 0);
    EQ(r, 0);
    CHECK(id != 0);
    EQ(U(DdeUninitialize)(id), TRUE);
}
TEST(dde_init_w_basic) {
    REQUIRE(U(DdeInitializeW)); REQUIRE(U(DdeUninitialize));
    DWORD id = 0;
    UINT r = U(DdeInitializeW)(&id, dh_null_cb, APPCLASS_STANDARD, 0);
    EQ(r, 0);
    CHECK(id != 0);
    EQ(U(DdeUninitialize)(id), TRUE);
}
TEST(dde_init_a_reserved_nonzero) {
    REQUIRE(U(DdeInitializeA));
    DWORD id = 0;
    EQ(U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD, 1), E_DMLERR_INVALIDPARAMETER);
    EQ(id, 0);
    EQ(U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD, 0xFFFFFFFF), E_DMLERR_INVALIDPARAMETER);
    EQ(id, 0);
}
TEST(dde_init_w_reserved_nonzero) {
    REQUIRE(U(DdeInitializeW));
    DWORD id = 0;
    EQ(U(DdeInitializeW)(&id, dh_null_cb, APPCLASS_STANDARD, 1), E_DMLERR_INVALIDPARAMETER);
    EQ(id, 0);
    EQ(U(DdeInitializeW)(&id, dh_null_cb, APPCLASS_STANDARD, 0x80000000), E_DMLERR_INVALIDPARAMETER);
    EQ(id, 0);
}
TEST(dde_init_reinit_keeps_id) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    DWORD id = 0;
    EQ(U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD, 0), 0);
    DWORD first = id;
    /* A nonzero *pidInst re-initialises the same instance (filter update). */
    UINT r = U(DdeInitializeA)(&id, dh_null_cb, CBF_SKIP_REGISTRATIONS, 0);
    EQ(r, 0);
    EQ(id, first);
    r = U(DdeInitializeW)(&id, dh_null_cb, APPCLASS_STANDARD, 0);
    EQ(r, 0);
    EQ(id, first);
    EQ(U(DdeUninitialize)(id), TRUE);
    /* One uninitialize releases it: the instance is gone. */
    EQ(U(DdeUninitialize)(first), FALSE);
}
TEST(dde_init_reinit_bogus_id) {
    REQUIRE(U(DdeInitializeA));
    DWORD id = 0x12345;
    EQ(U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_STANDARD, 0), E_DMLERR_INVALIDPARAMETER);
    EQ(id, 0x12345);
    id = 0xFFFFFFFF;
    EQ(U(DdeInitializeW)(&id, dh_null_cb, APPCLASS_STANDARD, 0), E_DMLERR_INVALIDPARAMETER);
    EQ(id, 0xFFFFFFFF);
}
TEST(dde_init_distinct_ids) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    DWORD a, b, c;
    CHECK(dde_open(&a, APPCLASS_STANDARD));
    CHECK(dde_open(&b, APPCLASS_STANDARD));
    CHECK(dde_open(&c, APPCMD_CLIENTONLY));
    CHECK(a != b); CHECK(b != c); CHECK(a != c);
    EQ(U(DdeUninitialize)(b), TRUE);
    EQ(U(DdeUninitialize)(a), TRUE);
    EQ(U(DdeUninitialize)(c), TRUE);
}
TEST(dde_init_flag_table) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    static const DWORD flags[] = {
        APPCLASS_STANDARD, APPCMD_CLIENTONLY, APPCMD_FILTERINITS,
        CBF_FAIL_CONNECTIONS, CBF_FAIL_ADVISES, CBF_FAIL_EXECUTES, CBF_FAIL_POKES, CBF_FAIL_REQUESTS,
        CBF_FAIL_ALLSVRXACTIONS, CBF_SKIP_CONNECT_CONFIRMS, CBF_SKIP_REGISTRATIONS,
        CBF_SKIP_UNREGISTRATIONS, CBF_SKIP_DISCONNECTS, CBF_SKIP_ALLNOTIFICATIONS,
        MF_HSZ_INFO, MF_SENDMSGS, MF_POSTMSGS, MF_CALLBACKS, MF_ERRORS, MF_LINKS, MF_CONV,
        CBF_FAIL_ALLSVRXACTIONS | CBF_SKIP_ALLNOTIFICATIONS,
        APPCMD_CLIENTONLY | CBF_SKIP_ALLNOTIFICATIONS,
        CBF_SKIP_REGISTRATIONS | CBF_SKIP_UNREGISTRATIONS | CBF_SKIP_DISCONNECTS,
    };
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        DWORD id = 0;
        UINT r = U(DdeInitializeA)(&id, dh_null_cb, flags[i], 0);
        CHECKF(r == 0, "flags 0x%lx -> 0x%x", flags[i], r);
        CHECKF(id != 0, "flags 0x%lx", flags[i]);
        if (r == 0) CHECKF(U(DdeUninitialize)(id), "flags 0x%lx", flags[i]);
        id = 0;
        r = U(DdeInitializeW)(&id, dh_null_cb, flags[i], 0);
        CHECKF(r == 0, "W flags 0x%lx -> 0x%x", flags[i], r);
        if (r == 0) CHECKF(U(DdeUninitialize)(id), "W flags 0x%lx", flags[i]);
    }
}
TEST(dde_init_monitor) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    DWORD id = 0;
    UINT r = U(DdeInitializeA)(&id, dh_null_cb, APPCLASS_MONITOR | MF_CALLBACKS | MF_CONV | MF_ERRORS | MF_HSZ_INFO | MF_LINKS | MF_POSTMSGS | MF_SENDMSGS, 0);
    EQ(r, 0);
    if (r == 0) { CHECK(id != 0); EQ(U(DdeUninitialize)(id), TRUE); }
    else t_note("monitor init returned 0x%x", r);
}
TEST(dde_uninit_invalid_ids) {
    REQUIRE(U(DdeUninitialize));
    static const DWORD ids[] = {0, 1, 2, 0x7F, 0x80, 0x1234, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++)
        CHECKF(U(DdeUninitialize)(ids[i]) == FALSE, "id 0x%lx", ids[i]);
}
TEST(dde_uninit_twice) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    DWORD id;
    CHECK(dde_open(&id, 0));
    EQ(U(DdeUninitialize)(id), TRUE);
    EQ(U(DdeUninitialize)(id), FALSE);
    EQ(U(DdeUninitialize)(id), FALSE);
}
TEST(dde_init_uninit_loop) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    for (int i = 0; i < 200; i++) {
        DWORD id;
        if (!dde_open(&id, 0)) { CHECKF(0, "iteration %d", i); break; }
        if (!U(DdeUninitialize)(id)) { CHECKF(0, "uninit iteration %d", i); break; }
    }
}
TEST(dde_init_many_instances) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    DWORD ids[40]; int n = 0;
    for (; n < 40; n++) if (!dde_open(&ids[n], 0)) break;
    CHECK(n >= 16);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (ids[i] == ids[j]) { CHECKF(0, "duplicate id"); }
    for (int i = n - 1; i >= 0; i--) CHECK(U(DdeUninitialize)(ids[i]));
}
TEST(dde_getlasterror_invalid_instance) {
    REQUIRE(U(DdeGetLastError));
    EQ(U(DdeGetLastError)(0), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeGetLastError)(0xFFFFFFFF), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeGetLastError)(0x4242), E_DMLERR_INVALIDPARAMETER);
}
TEST(dde_getlasterror_fresh_is_zero) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    EQ(U(DdeGetLastError)(id), 0);
    EQ(U(DdeGetLastError)(id), 0);
    U(DdeUninitialize)(id);
}
TEST(dde_getlasterror_cleared_on_read) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    EQ(U(DdeFreeStringHandle)(id, (HSZ)0x1234), FALSE);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeGetLastError)(id), 0);
    U(DdeUninitialize)(id);
}
TEST(dde_getlasterror_invalid_after_uninit) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    U(DdeUninitialize)(id);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
}
TEST(dde_invalid_instance_sets_error_on_live_instance) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError)); REQUIRE(U(DdeCreateStringHandleA));
    DWORD id; CHECK(dde_open(&id, 0));
    EQ(U(DdeCreateStringHandleA)(0x7777, "x", CP_WINANSI), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    U(DdeUninitialize)(id);
}

/* ---- string handles ---- */
TEST(dde_hsz_create_query_a) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ h = U(DdeCreateStringHandleA)(id, "T32HelloA", CP_WINANSI);
    CHECK(h != NULL);
    CHECK(((ULONG_PTR)h & ~(ULONG_PTR)0xFFFF) == 0 && (ULONG_PTR)h >= 0xC000);
    char buf[64] = "zz";
    EQ(U(DdeQueryStringA)(id, h, buf, sizeof buf, CP_WINANSI), 9);
    EXPECT_STR(buf, "T32HelloA");
    EQ(U(DdeQueryStringA)(id, h, NULL, 0, CP_WINANSI), 9);
    EQ(U(DdeQueryStringA)(id, h, buf, 10, CP_WINANSI), 9);
    EQ(U(DdeQueryStringA)(id, h, buf, 10, 0), 9);
    EQ(U(DdeFreeStringHandle)(id, h), TRUE);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_create_query_w) {
    REQUIRE(U(DdeInitializeW)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeQueryStringW)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id; (void)id;
    id = 0; CHECK(U(DdeInitializeW)(&id, dh_null_cb, 0, 0) == 0);
    HSZ h = U(DdeCreateStringHandleW)(id, L"T32HelloW", CP_WINUNICODE);
    CHECK(h != NULL);
    WCHAR buf[64] = L"zz";
    EQ(U(DdeQueryStringW)(id, h, buf, 64, CP_WINUNICODE), 9);
    EXPECT_WSTR(buf, L"T32HelloW");
    EQ(U(DdeQueryStringW)(id, h, NULL, 0, CP_WINUNICODE), 9);
    EQ(U(DdeQueryStringW)(id, h, buf, 10, 0), 9);
    U(DdeFreeStringHandle)(id, h);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_a_w_same_handle) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ a = U(DdeCreateStringHandleA)(id, "T32Parity", CP_WINANSI);
    HSZ w = U(DdeCreateStringHandleW)(id, L"T32Parity", CP_WINUNICODE);
    CHECK(a != NULL);
    EQ(a, w);
    HSZ a2 = U(DdeCreateStringHandleA)(id, "T32Parity", 0);
    EQ(a, a2);
    U(DdeFreeStringHandle)(id, a); U(DdeFreeStringHandle)(id, w); U(DdeFreeStringHandle)(id, a2);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_case_insensitive) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeCmpStringHandles)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ a = U(DdeCreateStringHandleA)(id, "T32CaseTest", CP_WINANSI);
    HSZ b = U(DdeCreateStringHandleA)(id, "t32casetest", CP_WINANSI);
    HSZ c = U(DdeCreateStringHandleA)(id, "T32CaseTesT2", CP_WINANSI);
    EQ(a, b);
    EQ(U(DdeCmpStringHandles)(a, b), 0);
    CHECK(a != c);
    NE(U(DdeCmpStringHandles)(a, c), 0);
    U(DdeFreeStringHandle)(id, a); U(DdeFreeStringHandle)(id, b); U(DdeFreeStringHandle)(id, c);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_empty_string) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    EQ(U(DdeCreateStringHandleA)(id, "", CP_WINANSI), NULL);
    EQ(U(DdeCreateStringHandleW)(id, L"", CP_WINUNICODE), NULL);
    EQ(U(DdeGetLastError)(id), 0);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_length_limits) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    char s[300]; WCHAR w[300];
    memset(s, 'q', 299); s[299] = 0;
    for (int i = 0; i < 299; i++) w[i] = 'q'; w[299] = 0;
    s[255] = 0; w[255] = 0;
    HSZ h255 = U(DdeCreateStringHandleA)(id, s, CP_WINANSI);
    CHECK(h255 != NULL);
    char out[300];
    if (h255) {
        EQ(U(DdeQueryStringA)(id, h255, out, sizeof out, CP_WINANSI), 255);
        EQ(U(DdeQueryStringA)(id, h255, NULL, 0, CP_WINANSI), 255);
        U(DdeFreeStringHandle)(id, h255);
    }
    HSZ hw255 = U(DdeCreateStringHandleW)(id, w, CP_WINUNICODE);
    CHECK(hw255 != NULL);
    if (hw255) U(DdeFreeStringHandle)(id, hw255);
    s[255] = 'q'; s[256] = 0; w[255] = 'q'; w[256] = 0;
    EQ(U(DdeCreateStringHandleA)(id, s, CP_WINANSI), NULL);
    EQ(U(DdeCreateStringHandleW)(id, w, CP_WINUNICODE), NULL);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_bad_codepage) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    static const int cps[] = {437, 850, 932, 1252, 1251, 65001, 1, -1, 0x7FFFFFFF, 1201};
    for (unsigned i = 0; i < sizeof cps / sizeof cps[0]; i++) {
        U(DdeGetLastError)(id);
        HSZ h = U(DdeCreateStringHandleA)(id, "T32BadCp", cps[i]);
        CHECKF(h == NULL, "cp %d", cps[i]);
        CHECKF(U(DdeGetLastError)(id) == E_DMLERR_INVALIDPARAMETER, "cp %d", cps[i]);
        h = U(DdeCreateStringHandleW)(id, L"T32BadCp", cps[i]);
        CHECKF(h == NULL, "W cp %d", cps[i]);
        U(DdeGetLastError)(id);
    }
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_query_buffer_sizes) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ h = U(DdeCreateStringHandleA)(id, "T32Buf5", CP_WINANSI);   /* 7 chars */
    char b[32];
    memset(b, 'x', sizeof b);
    EQ(U(DdeQueryStringA)(id, h, b, 8, CP_WINANSI), 7);       /* exact incl. NUL */
    EXPECT_STR(b, "T32Buf5");
    EQ(U(DdeQueryStringA)(id, h, b, 9, CP_WINANSI), 7);
    EQ(U(DdeQueryStringA)(id, h, b, 32, CP_WINANSI), 7);
    memset(b, 'x', sizeof b);
    DWORD r = U(DdeQueryStringA)(id, h, b, 7, CP_WINANSI);           /* exact-1 */
    t_note("query cchMax=7 (len 7) -> %lu, buf=\"%.8s\"", r, b);
    CHECK(r <= 7);
    CHECK(b[7] == 'x' || b[7] == 0 || b[6] == 0);
    memset(b, 'x', sizeof b);
    r = U(DdeQueryStringA)(id, h, b, 1, CP_WINANSI);
    t_note("query cchMax=1 -> %lu, buf[0]=%d", r, b[0]);
    CHECK(r <= 1);
    memset(b, 'x', sizeof b);
    r = U(DdeQueryStringA)(id, h, b, 0, CP_WINANSI);
    t_note("query cchMax=0 -> %lu, buf[0]=%d", r, b[0]);
    CHECK(r == 0);
    U(DdeFreeStringHandle)(id, h);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_query_null_hsz) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeQueryStringW)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    char a[8] = "zzzz"; WCHAR w[8] = L"zzzz";
    DWORD r = U(DdeQueryStringA)(id, NULL, a, 8, CP_WINANSI);
    t_note("DdeQueryStringA(NULL hsz) -> %lu", r);
    EQ(a[0], 0);
    r = U(DdeQueryStringW)(id, NULL, w, 8, CP_WINUNICODE);
    t_note("DdeQueryStringW(NULL hsz) -> %lu", r);
    EQ(w[0], 0);
    EQ(U(DdeGetLastError)(id), 0);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_query_invalid) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeQueryStringW)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    static const ULONG_PTR bad[] = {1, 2, 0x1234, 0xBFFF, 0x10000, 0xFFFFFFFF};
    char a[16]; WCHAR w[16];
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(DdeGetLastError)(id);
        DWORD r = U(DdeQueryStringA)(id, (HSZ)bad[i], a, 16, CP_WINANSI);
        CHECKF(r == 0, "hsz 0x%llx", (unsigned long long)bad[i]);
        CHECKF(U(DdeGetLastError)(id) == E_DMLERR_INVALIDPARAMETER, "hsz 0x%llx", (unsigned long long)bad[i]);
        r = U(DdeQueryStringW)(id, (HSZ)bad[i], w, 16, CP_WINUNICODE);
        CHECKF(r == 0, "W hsz 0x%llx", (unsigned long long)bad[i]);
        U(DdeGetLastError)(id);
    }
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_query_invalid_instance) {
    REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeQueryStringW));
    char a[8]; WCHAR w[8];
    EQ(U(DdeQueryStringA)(0, (HSZ)0xC001, a, 8, CP_WINANSI), 0);
    EQ(U(DdeQueryStringW)(0xFFFFFFFF, (HSZ)0xC001, w, 8, CP_WINUNICODE), 0);
}
TEST(dde_hsz_cross_instance) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeFreeStringHandle));
    DWORD a, b; CHECK(dde_open(&a, 0)); CHECK(dde_open(&b, 0));
    HSZ h = U(DdeCreateStringHandleA)(a, "T32Cross", CP_WINANSI);
    char buf[32];
    EQ(U(DdeQueryStringA)(b, h, buf, 32, CP_WINANSI), 8);
    EXPECT_STR(buf, "T32Cross");
    U(DdeFreeStringHandle)(b, h);
    U(DdeUninitialize)(a); U(DdeUninitialize)(b);
}
TEST(dde_hsz_free_semantics) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ h = U(DdeCreateStringHandleA)(id, "T32UniqueFreeTwice", CP_WINANSI);
    EQ(U(DdeFreeStringHandle)(id, h), TRUE);
    U(DdeGetLastError)(id);
    EQ(U(DdeFreeStringHandle)(id, h), FALSE);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    /* NULL handle is valid and a no-op. */
    EQ(U(DdeFreeStringHandle)(id, NULL), TRUE);
    EQ(U(DdeGetLastError)(id), 0);
    /* Bogus handles. */
    EQ(U(DdeFreeStringHandle)(id, (HSZ)0x55), FALSE);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    /* Invalid instance. */
    EQ(U(DdeFreeStringHandle)(0, h), FALSE);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_refcount_create_twice) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ a = U(DdeCreateStringHandleA)(id, "T32RefTwice", CP_WINANSI);
    HSZ b = U(DdeCreateStringHandleA)(id, "T32RefTwice", CP_WINANSI);
    EQ(a, b);
    EQ(U(DdeFreeStringHandle)(id, a), TRUE);   /* still one ref */
    EQ(U(DdeFreeStringHandle)(id, b), TRUE);
    EQ(U(DdeFreeStringHandle)(id, b), FALSE);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_keep_semantics) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeKeepStringHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ h = U(DdeCreateStringHandleA)(id, "T32KeepMe", CP_WINANSI);
    EQ(U(DdeKeepStringHandle)(id, h), TRUE);
    EQ(U(DdeFreeStringHandle)(id, h), TRUE);
    EQ(U(DdeFreeStringHandle)(id, h), TRUE);
    EQ(U(DdeFreeStringHandle)(id, h), FALSE);
    EQ(U(DdeKeepStringHandle)(id, NULL), TRUE);
    U(DdeGetLastError)(id);
    EQ(U(DdeKeepStringHandle)(id, (HSZ)0x77), FALSE);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeKeepStringHandle)(0, h), FALSE);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_cmp_values) {
    REQUIRE(U(DdeCmpStringHandles));
    EQ(U(DdeCmpStringHandles)(NULL, NULL), 0);
    EQ(U(DdeCmpStringHandles)((HSZ)0xC001, (HSZ)0xC001), 0);
    EQ(U(DdeCmpStringHandles)((HSZ)0xC001, (HSZ)0xC002), -1);
    EQ(U(DdeCmpStringHandles)((HSZ)0xC002, (HSZ)0xC001), 1);
    EQ(U(DdeCmpStringHandles)(NULL, (HSZ)0xC001), -1);
    EQ(U(DdeCmpStringHandles)((HSZ)0xC001, NULL), 1);
}
TEST(dde_hsz_unicode_roundtrip) {
    REQUIRE(U(DdeInitializeW)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeQueryStringW)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id = 0; CHECK(U(DdeInitializeW)(&id, dh_null_cb, 0, 0) == 0);
    static const WCHAR s[] = {0x4e2d, 0x6587, 0x00e9, 0x0416, 'T', '3', '2', 0};
    HSZ h = U(DdeCreateStringHandleW)(id, s, CP_WINUNICODE);
    CHECK(h != NULL);
    WCHAR out[32] = {0};
    EQ(U(DdeQueryStringW)(id, h, out, 32, CP_WINUNICODE), 7);
    CHECK(!lstrcmpW(out, s));
    HSZ h2 = U(DdeCreateStringHandleW)(id, s, 0);
    EQ(h, h2);
    U(DdeFreeStringHandle)(id, h); U(DdeFreeStringHandle)(id, h2);
    U(DdeUninitialize)(id);
}
TEST(dde_hsz_ansi_codepage_on_w_entry) {
    REQUIRE(U(DdeInitializeW)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeFreeStringHandle));
    DWORD id = 0; CHECK(U(DdeInitializeW)(&id, dh_null_cb, 0, 0) == 0);
    /* With CP_WINANSI the W entry point reads an ANSI string. */
    HSZ h = U(DdeCreateStringHandleW)(id, (LPCWSTR)"T32AnsiViaW", CP_WINANSI);
    t_note("DdeCreateStringHandleW(ansi string, CP_WINANSI) -> %p", h);
    if (h) {
        char buf[32] = {0};
        U(DdeQueryStringA)(id, h, buf, 32, CP_WINANSI);
        CHECK(!strcmp(buf, "T32AnsiViaW"));
        U(DdeFreeStringHandle)(id, h);
    }
    U(DdeUninitialize)(id);
}

/* ---- data handles ---- */
TEST(dde_data_create_basic) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeGetData)); REQUIRE(U(DdeFreeDataHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    const char src[] = "hello world";
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)src, sizeof src, 0, NULL, CF_TEXT, 0);
    CHECK(d != NULL);
    DWORD sz = U(DdeGetData)(d, NULL, 0, 0);
    CHECK(sz >= sizeof src);
    char buf[64] = {0};
    DWORD got = U(DdeGetData)(d, (LPBYTE)buf, sizeof buf, 0);
    CHECK(got >= sizeof src);
    CHECK(!memcmp(buf, src, sizeof src));
    EQ(U(DdeFreeDataHandle)(d), TRUE);
    U(DdeUninitialize)(id);
}
TEST(dde_data_getdata_offsets_and_limits) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeGetData)); REQUIRE(U(DdeFreeDataHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    BYTE src[16]; for (int i = 0; i < 16; i++) src[i] = (BYTE)(0xA0 + i);
    HDDEDATA d = U(DdeCreateDataHandle)(id, src, 16, 0, NULL, CF_PRIVATEFIRST, 0);
    CHECK(d != NULL);
    BYTE out[32];
    memset(out, 0, sizeof out);
    EQ(U(DdeGetData)(d, out, 3, 0), 3);
    CHECK(!memcmp(out, src, 3)); EQ(out[3], 0);
    memset(out, 0, sizeof out);
    EQ(U(DdeGetData)(d, out, 4, 4), 4);
    CHECK(!memcmp(out, src + 4, 4));
    EQ(U(DdeGetData)(d, out, 0, 0), 0);
    DWORD full = U(DdeGetData)(d, NULL, 0, 0);
    CHECK(full >= 16);
    DWORD tail = U(DdeGetData)(d, NULL, 0, 8);
    EQ(tail, full - 8);
    /* offset beyond the block */
    U(DdeGetLastError)(id);
    EQ(U(DdeGetData)(d, out, 8, 0x10000), 0);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    U(DdeFreeDataHandle)(d);
    U(DdeUninitialize)(id);
}
TEST(dde_data_invalid_flags) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    static const UINT bad[] = {2, 3, 4, 0x100, 0x8000, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        U(DdeGetLastError)(id);
        HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"abcd", 4, 0, NULL, CF_TEXT, bad[i]);
        CHECKF(d == NULL, "afCmd 0x%x", bad[i]);
        CHECKF(U(DdeGetLastError)(id) == E_DMLERR_INVALIDPARAMETER, "afCmd 0x%x", bad[i]);
    }
    U(DdeUninitialize)(id);
}
TEST(dde_data_cb_all_ones) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    EQ(U(DdeCreateDataHandle)(id, (LPBYTE)"x", 0xFFFFFFFF, 0, NULL, CF_TEXT, 0), NULL);
    U(DdeUninitialize)(id);
}
TEST(dde_data_invalid_instance) {
    REQUIRE(U(DdeCreateDataHandle));
    EQ(U(DdeCreateDataHandle)(0, (LPBYTE)"abcd", 4, 0, NULL, CF_TEXT, 0), NULL);
    EQ(U(DdeCreateDataHandle)(0xDEAD, (LPBYTE)"abcd", 4, 0, NULL, CF_TEXT, 0), NULL);
}
TEST(dde_data_null_source_allocates) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeGetData)); REQUIRE(U(DdeFreeDataHandle)); REQUIRE(U(DdeAccessData)); REQUIRE(U(DdeUnaccessData));
    DWORD id; CHECK(dde_open(&id, 0));
    HDDEDATA d = U(DdeCreateDataHandle)(id, NULL, 100, 0, NULL, CF_PRIVATEFIRST, 0);
    CHECK(d != NULL);
    CHECK(U(DdeGetData)(d, NULL, 0, 0) >= 100);
    DWORD sz = 0;
    LPBYTE p = U(DdeAccessData)(d, &sz);
    CHECK(p != NULL); CHECK(sz >= 100);
    if (p) memset(p, 0x5A, 100);
    EQ(U(DdeUnaccessData)(d), TRUE);
    BYTE out[100]; memset(out, 0, 100);
    EQ(U(DdeGetData)(d, out, 100, 0), 100);
    CHECK(out[0] == 0x5A && out[99] == 0x5A);
    U(DdeFreeDataHandle)(d);
    /* zero-size text block gets at least a terminator slot */
    d = U(DdeCreateDataHandle)(id, NULL, 0, 0, NULL, CF_TEXT, 0);
    CHECK(d != NULL);
    if (d) { t_note("zero-length CF_TEXT block size=%lu", U(DdeGetData)(d, NULL, 0, 0)); U(DdeFreeDataHandle)(d); }
    U(DdeUninitialize)(id);
}
TEST(dde_data_access_unaccess) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeFreeDataHandle)); REQUIRE(U(DdeAccessData)); REQUIRE(U(DdeUnaccessData));
    DWORD id; CHECK(dde_open(&id, 0));
    const char src[] = "access-me";
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)src, sizeof src, 0, NULL, CF_TEXT, 0);
    DWORD sz = 0;
    LPBYTE p = U(DdeAccessData)(d, &sz);
    CHECK(p != NULL);
    CHECK(sz >= sizeof src);
    if (p) CHECK(!memcmp(p, src, sizeof src));
    LPBYTE p2 = U(DdeAccessData)(d, NULL);
    CHECK(p2 != NULL);
    EQ(U(DdeUnaccessData)(d), TRUE);
    EQ(U(DdeUnaccessData)(d), TRUE);
    U(DdeFreeDataHandle)(d);
    U(DdeUninitialize)(id);
}
TEST(dde_data_invalid_handles) {
    REQUIRE(U(DdeAccessData)); REQUIRE(U(DdeUnaccessData)); REQUIRE(U(DdeFreeDataHandle)); REQUIRE(U(DdeGetData)); REQUIRE(U(DdeAddData));
    DWORD sz = 77; BYTE buf[8];
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xC000, 0xDEADBEEF};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HDDEDATA h = (HDDEDATA)bad[i];
        CHECKF(U(DdeAccessData)(h, &sz) == NULL, "h=%p", h);
        CHECKF(U(DdeUnaccessData)(h) == FALSE, "h=%p", h);
        CHECKF(U(DdeFreeDataHandle)(h) == FALSE, "h=%p", h);
        CHECKF(U(DdeGetData)(h, buf, 8, 0) == 0, "h=%p", h);
        CHECKF(U(DdeAddData)(h, buf, 8, 0) == NULL, "h=%p", h);
    }
}
TEST(dde_data_free_twice) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeFreeDataHandle)); REQUIRE(U(DdeGetData));
    DWORD id; CHECK(dde_open(&id, 0));
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"abcd", 4, 0, NULL, CF_TEXT, 0);
    EQ(U(DdeFreeDataHandle)(d), TRUE);
    EQ(U(DdeFreeDataHandle)(d), FALSE);
    BYTE b[4];
    EQ(U(DdeGetData)(d, b, 4, 0), 0);
    U(DdeUninitialize)(id);
}
TEST(dde_data_appowned) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeFreeDataHandle)); REQUIRE(U(DdeGetData));
    DWORD id; CHECK(dde_open(&id, 0));
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"owned", 6, 0, NULL, CF_TEXT, HDATA_APPOWNED);
    CHECK(d != NULL);
    BYTE b[8] = {0};
    CHECK(U(DdeGetData)(d, b, 8, 0) >= 6);
    BOOL f1 = U(DdeFreeDataHandle)(d);
    DWORD after = U(DdeGetData)(d, b, 8, 0);
    t_note("APPOWNED: first free=%d, GetData after free=%lu", f1, after);
    BOOL f2 = U(DdeFreeDataHandle)(d);
    t_note("APPOWNED: second free=%d", f2);
    U(DdeUninitialize)(id);
}
TEST(dde_data_add_data) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeAddData)); REQUIRE(U(DdeGetData)); REQUIRE(U(DdeFreeDataHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"abcd", 4, 0, NULL, CF_PRIVATEFIRST, HDATA_APPOWNED);
    CHECK(d != NULL);
    HDDEDATA d2 = U(DdeAddData)(d, (LPBYTE)"WXYZ", 4, 4);
    CHECK(d2 != NULL);
    if (d2) {
        BYTE out[16]; memset(out, 0, 16);
        CHECK(U(DdeGetData)(d2, out, 8, 0) == 8);
        CHECK(!memcmp(out, "abcdWXYZ", 8));
        /* overwrite inside the block */
        d2 = U(DdeAddData)(d2, (LPBYTE)"--", 2, 1);
        CHECK(d2 != NULL);
        if (d2) { memset(out, 0, 16); U(DdeGetData)(d2, out, 8, 0); CHECK(!memcmp(out, "a--dWXYZ", 8)); }
        U(DdeFreeDataHandle)(d2);
    }
    U(DdeUninitialize)(id);
}
TEST(dde_data_with_item_hsz) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeDataHandle)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetData));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ item = U(DdeCreateStringHandleA)(id, "T32DataItem", CP_WINANSI);
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"1234", 4, 0, item, CF_TEXT, 0);
    CHECK(d != NULL);
    BYTE b[8] = {0};
    CHECK(U(DdeGetData)(d, b, 4, 0) == 4);
    CHECK(!memcmp(b, "1234", 4));
    U(DdeFreeDataHandle)(d);
    U(DdeFreeStringHandle)(id, item);
    U(DdeUninitialize)(id);
}
TEST(dde_data_large_block) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeGetData)); REQUIRE(U(DdeFreeDataHandle));
    DWORD id; CHECK(dde_open(&id, 0));
    DWORD n = 4 * 1024 * 1024;
    BYTE *src = VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(src != NULL);
    if (src) {
        for (DWORD i = 0; i < n; i += 4096) src[i] = (BYTE)(i >> 12);
        HDDEDATA d = U(DdeCreateDataHandle)(id, src, n, 0, NULL, CF_PRIVATEFIRST, 0);
        CHECK(d != NULL);
        if (d) {
            CHECK(U(DdeGetData)(d, NULL, 0, 0) >= n);
            BYTE b = 0;
            EQ(U(DdeGetData)(d, &b, 1, 4096 * 3), 1);
            EQ(b, 3);
            U(DdeFreeDataHandle)(d);
        }
        VirtualFree(src, 0, MEM_RELEASE);
    }
    U(DdeUninitialize)(id);
}

/* ---- name service / connect / misc validation ---- */
TEST(dde_nameservice_invalid_instance) {
    REQUIRE(U(DdeNameService));
    EQ(U(DdeNameService)(0, NULL, NULL, DNS_REGISTER), NULL);
    EQ(U(DdeNameService)(0xFFFFFFFF, NULL, NULL, DNS_UNREGISTER), NULL);
}
TEST(dde_nameservice_param_errors) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeNameService)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ h = U(DdeCreateStringHandleA)(id, "T32NsParam", CP_WINANSI);
    /* register with NULL name */
    U(DdeGetLastError)(id);
    EQ(U(DdeNameService)(id, NULL, NULL, DNS_REGISTER), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    /* hsz2 must be NULL */
    EQ(U(DdeNameService)(id, h, h, DNS_REGISTER), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    /* bogus hsz1 */
    EQ(U(DdeNameService)(id, (HSZ)0x1111, NULL, DNS_REGISTER), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    U(DdeFreeStringHandle)(id, h);
    U(DdeUninitialize)(id);
}
TEST(dde_nameservice_register_unregister) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeNameService)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, CBF_SKIP_ALLNOTIFICATIONS));
    HSZ h = U(DdeCreateStringHandleA)(id, "T32NsReg", CP_WINANSI);
    EQ(U(DdeNameService)(id, h, NULL, DNS_REGISTER), (HDDEDATA)1);
    EQ(U(DdeNameService)(id, h, NULL, DNS_REGISTER), (HDDEDATA)1);   /* idempotent from the caller's view */
    EQ(U(DdeNameService)(id, h, NULL, DNS_UNREGISTER), (HDDEDATA)1);
    EQ(U(DdeNameService)(id, h, NULL, DNS_UNREGISTER), (HDDEDATA)1);
    EQ(U(DdeNameService)(id, NULL, NULL, DNS_UNREGISTER), (HDDEDATA)1);  /* unregister all */
    EQ(U(DdeNameService)(id, NULL, NULL, DNS_FILTERON), (HDDEDATA)1);
    EQ(U(DdeNameService)(id, NULL, NULL, DNS_FILTEROFF), (HDDEDATA)1);
    EQ(U(DdeNameService)(id, h, NULL, 0), (HDDEDATA)1);
    EQ(U(DdeGetLastError)(id), 0);
    U(DdeFreeStringHandle)(id, h);
    U(DdeUninitialize)(id);
}
TEST(dde_nameservice_clientonly_register_fails) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeNameService)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, APPCMD_CLIENTONLY));
    HSZ h = U(DdeCreateStringHandleA)(id, "T32NsClientOnly", CP_WINANSI);
    EQ(U(DdeNameService)(id, h, NULL, DNS_REGISTER), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_DLL_USAGE);
    U(DdeFreeStringHandle)(id, h);
    U(DdeUninitialize)(id);
}
TEST(dde_connect_invalid_instance) {
    REQUIRE(U(DdeConnect)); REQUIRE(U(DdeConnectList));
    EQ(U(DdeConnect)(0, NULL, NULL, NULL), NULL);
    EQ(U(DdeConnect)(0xBAD, (HSZ)0xC001, (HSZ)0xC002, NULL), NULL);
    EQ(U(DdeConnectList)(0, NULL, NULL, NULL, NULL), NULL);
}
TEST(dde_connect_no_server) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeConnect)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ s = U(DdeCreateStringHandleA)(id, "T32NoSuchService_A1B2", CP_WINANSI);
    HSZ t = U(DdeCreateStringHandleA)(id, "T32NoSuchTopic", CP_WINANSI);
    EQ(U(DdeConnect)(id, s, t, NULL), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_NO_CONV_ESTABLISHED);
    CONVCONTEXT cc; memset(&cc, 0, sizeof cc); cc.cb = sizeof cc; cc.iCodePage = CP_WINANSI;
    EQ(U(DdeConnect)(id, s, t, &cc), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_NO_CONV_ESTABLISHED);
    U(DdeFreeStringHandle)(id, s); U(DdeFreeStringHandle)(id, t);
    U(DdeUninitialize)(id);
}
TEST(dde_connectlist_no_server) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeConnectList)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ s = U(DdeCreateStringHandleA)(id, "T32NoSuchService_C3D4", CP_WINANSI);
    HSZ t = U(DdeCreateStringHandleA)(id, "T32NoSuchTopic", CP_WINANSI);
    EQ(U(DdeConnectList)(id, s, t, NULL, NULL), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_NO_CONV_ESTABLISHED);
    U(DdeFreeStringHandle)(id, s); U(DdeFreeStringHandle)(id, t);
    U(DdeUninitialize)(id);
}
TEST(dde_connect_bogus_hsz) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeConnect)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    EQ(U(DdeConnect)(id, (HSZ)0x1234, (HSZ)0x4321, NULL), NULL);
    DWORD e = U(DdeGetLastError)(id);
    CHECKF(e == E_DMLERR_INVALIDPARAMETER || e == E_DMLERR_NO_CONV_ESTABLISHED, "err 0x%lx", e);
    t_note("bogus-hsz connect error 0x%lx", e);
    U(DdeUninitialize)(id);
}
TEST(dde_connect_clientonly_bad_context_size) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeConnect)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    HSZ s = U(DdeCreateStringHandleA)(id, "T32NoSuchService_E5F6", CP_WINANSI);
    HSZ t = U(DdeCreateStringHandleA)(id, "T32NoSuchTopic", CP_WINANSI);
    CONVCONTEXT cc; memset(&cc, 0, sizeof cc); cc.cb = 3;
    EQ(U(DdeConnect)(id, s, t, &cc), NULL);
    DWORD e = U(DdeGetLastError)(id);
    t_note("DdeConnect with CONVCONTEXT.cb=3 -> error 0x%lx", e);
    U(DdeFreeStringHandle)(id, s); U(DdeFreeStringHandle)(id, t);
    U(DdeUninitialize)(id);
}
TEST(dde_conv_functions_invalid_handle) {
    REQUIRE(U(DdeDisconnect)); REQUIRE(U(DdeDisconnectList)); REQUIRE(U(DdeReconnect)); REQUIRE(U(DdeQueryConvInfo));
    REQUIRE(U(DdeSetUserHandle)); REQUIRE(U(DdeQueryNextServer)); REQUIRE(U(DdeImpersonateClient)); REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    static const ULONG_PTR bad[] = {0, 1, 0x1234, 0xDEADBEEF};
    CONVINFO ci; 
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HCONV h = (HCONV)bad[i];
        CHECKF(U(DdeDisconnect)(h) == FALSE, "h=%p", h);
        CHECKF(U(DdeDisconnectList)((HCONVLIST)h) == FALSE, "h=%p", h);
        CHECKF(U(DdeReconnect)(h) == NULL, "h=%p", h);
        memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
        CHECKF(U(DdeQueryConvInfo)(h, QID_SYNC, &ci) == 0, "h=%p", h);
        CHECKF(U(DdeSetUserHandle)(h, QID_SYNC, 5) == FALSE, "h=%p", h);
        CHECKF(U(DdeQueryNextServer)((HCONVLIST)h, NULL) == NULL, "h=%p", h);
        CHECKF(U(DdeImpersonateClient)(h) == FALSE, "h=%p", h);
    }
    U(DdeUninitialize)(id);
}
TEST(dde_clienttransaction_invalid) {
    REQUIRE(U(DdeClientTransaction)); REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError));
    DWORD id; CHECK(dde_open(&id, 0));
    DWORD res = 0x55;
    U(DdeGetLastError)(id);
    EQ(U(DdeClientTransaction)(NULL, 0, NULL, (HSZ)0xC001, CF_TEXT, XTYP_REQUEST, 100, &res), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeClientTransaction)(NULL, 0, (HCONV)0x1234, (HSZ)0xC001, CF_TEXT, XTYP_REQUEST, 100, &res), NULL);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdeClientTransaction)((LPBYTE)"x", 1, (HCONV)0xBEEF, NULL, CF_TEXT, XTYP_EXECUTE, 100, NULL), NULL);
    U(DdeUninitialize)(id);
}
TEST(dde_abandon_invalid) {
    REQUIRE(U(DdeAbandonTransaction)); REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    DWORD id; CHECK(dde_open(&id, 0));
    EQ(U(DdeAbandonTransaction)(id, (HCONV)0x1234, 5), FALSE);
    EQ(U(DdeAbandonTransaction)(0, NULL, 0), FALSE);
    EQ(U(DdeAbandonTransaction)(0xBAD, (HCONV)0x99, 1), FALSE);
    BOOL r = U(DdeAbandonTransaction)(id, NULL, 0);
    t_note("DdeAbandonTransaction(valid inst, NULL, 0) -> %d", r);
    U(DdeUninitialize)(id);
}
TEST(dde_enablecallback_validation) {
    REQUIRE(U(DdeEnableCallback)); REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError));
    EQ(U(DdeEnableCallback)(0, NULL, EC_ENABLEALL), FALSE);
    DWORD id; CHECK(dde_open(&id, 0));
    static const UINT badcmd[] = {1, 3, 4, 5, 0x10, 0x81, 0x100, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof badcmd / sizeof badcmd[0]; i++) {
        U(DdeGetLastError)(id);
        CHECKF(U(DdeEnableCallback)(id, NULL, badcmd[i]) == FALSE, "cmd 0x%x", badcmd[i]);
        CHECKF(U(DdeGetLastError)(id) == E_DMLERR_INVALIDPARAMETER, "cmd 0x%x", badcmd[i]);
    }
    /* Valid command, bogus conversation. */
    U(DdeGetLastError)(id);
    EQ(U(DdeEnableCallback)(id, (HCONV)0x1234, EC_ENABLEALL), FALSE);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    /* Valid global commands never leave an error behind. */
    U(DdeEnableCallback)(id, NULL, EC_ENABLEALL);
    U(DdeEnableCallback)(id, NULL, EC_DISABLE);
    U(DdeEnableCallback)(id, NULL, EC_ENABLEALL);
    EQ(U(DdeGetLastError)(id), 0);
    U(DdeUninitialize)(id);
}
TEST(dde_post_advise_validation) {
    REQUIRE(U(DdePostAdvise)); REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError));
    EQ(U(DdePostAdvise)(0, NULL, NULL), FALSE);
    DWORD id; CHECK(dde_open(&id, 0));
    EQ(U(DdePostAdvise)(id, (HSZ)0x1234, NULL), FALSE);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    EQ(U(DdePostAdvise)(id, NULL, (HSZ)0x1234), FALSE);
    EQ(U(DdeGetLastError)(id), E_DMLERR_INVALIDPARAMETER);
    U(DdeUninitialize)(id);
}
TEST(dde_quality_of_service_stubs) {
    UDECL(BOOL, DdeGetQualityOfService, (HWND, const SECURITY_QUALITY_OF_SERVICE *, PSECURITY_QUALITY_OF_SERVICE));
    REQUIRE(U(DdeSetQualityOfService)); REQUIRE(UN(DdeGetQualityOfService));
    SECURITY_QUALITY_OF_SERVICE q, p; memset(&q, 0, sizeof q); memset(&p, 0, sizeof p);
    q.Length = sizeof q; q.ImpersonationLevel = SecurityImpersonation;
    EQ(U(DdeSetQualityOfService)(NULL, &q, &p), TRUE);
    EQ(U(DdeSetQualityOfService)(NULL, NULL, NULL), TRUE);
    if (dh_is_wine()) { t_note("skipping DdeGetQualityOfService (unimplemented stub aborts the process)"); return; }
    EQ(UN(DdeGetQualityOfService)(NULL, &q, &p), TRUE);
    EQ(UN(DdeGetQualityOfService)((HWND)0x1234, NULL, NULL), TRUE);
}
TEST(dde_query_next_server_empty) {
    REQUIRE(U(DdeQueryNextServer));
    EQ(U(DdeQueryNextServer)(NULL, NULL), NULL);
    EQ(U(DdeQueryNextServer)(NULL, (HCONV)0x55), NULL);
}
TEST(dde_other_thread_cannot_use_instance) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle));
    static DWORD g_id; static HSZ g_made; static BOOL g_uninit; static HSZ g_h;
    DWORD id; CHECK(dde_open(&id, 0));
    g_id = id;
    HSZ h = U(DdeCreateStringHandleA)(id, "T32Thread", CP_WINANSI);
    CHECK(h != NULL);
    g_h = h;
    DWORD WINAPI th(LPVOID p) {
        g_made = U(DdeCreateStringHandleA)(g_id, "T32ThreadX", CP_WINANSI);
        g_uninit = U(DdeUninitialize)(g_id);
        return 0;
    }
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    CHECK(t != NULL);
    if (t) { CHECK(WaitForSingleObject(t, 10000) == WAIT_OBJECT_0); CloseHandle(t); }
    EQ(g_made, NULL);
    EQ(g_uninit, FALSE);
    EQ(U(DdeFreeStringHandle)(id, h), TRUE);
    EQ(U(DdeUninitialize)(id), TRUE);
}
TEST(dde_other_thread_can_init_own_instance) {
    REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize));
    static DWORD g_id2; static UINT g_r; static BOOL g_u;
    DWORD id; CHECK(dde_open(&id, 0));
    DWORD WINAPI th(LPVOID p) {
        g_id2 = 0;
        g_r = U(DdeInitializeA)(&g_id2, dh_null_cb, 0, 0);
        g_u = g_r == 0 ? U(DdeUninitialize)(g_id2) : 0;
        return 0;
    }
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    if (t) { WaitForSingleObject(t, 10000); CloseHandle(t); }
    EQ(g_r, 0);
    CHECK(g_id2 != 0);
    EQ(g_u, TRUE);
    U(DdeUninitialize)(id);
}
TEST(dde_impersonate_window_null) {
    UDECL(BOOL, ImpersonateDdeClientWindow, (HWND, HWND));
    REQUIRE(UN(ImpersonateDdeClientWindow));
    SetLastError(0);
    EQ(UN(ImpersonateDdeClientWindow)(NULL, NULL), FALSE);
    SetLastError(0);
    EQ(UN(ImpersonateDdeClientWindow)((HWND)0x12345678, (HWND)0x1234), FALSE);
    t_note("ImpersonateDdeClientWindow bogus -> err %lu", GetLastError());
}
