/* misc_ui: DDE lParam packing and DDEML argument / error paths (no server round trips) */
#include "mu.h"
#include <ddeml.h>
#include <dde.h>

#define WMDDE_INITIATE 0x3E0
#define WMDDE_TERMINATE 0x3E1
#define WMDDE_ADVISE 0x3E2
#define WMDDE_UNADVISE 0x3E3
#define WMDDE_ACK 0x3E4
#define WMDDE_DATA 0x3E5
#define WMDDE_REQUEST 0x3E6
#define WMDDE_POKE 0x3E7
#define WMDDE_EXECUTE 0x3E8

#define NEED_PACK() REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam)); REQUIRE(U(FreeDDElParam)); REQUIRE(U(ReuseDDElParam))

static int is_mem_msg(UINT m) { return m == WMDDE_ADVISE || m == WMDDE_ACK || m == WMDDE_DATA || m == WMDDE_POKE; }

/* ---------- packing: 16-bit pairs ---------- */
TEST(misc_ui_ddepack_two_words_roundtrip) {
    NEED_PACK();
    static const UINT msgs[] = { WMDDE_INITIATE, WMDDE_TERMINATE, WMDDE_UNADVISE, WMDDE_REQUEST, WM_NULL, WM_USER, 0x3DF, 0x3E9, 0x1000 };
    static const UINT_PTR vals[] = { 0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x1234 };
    for (int m = 0; m < (int)(sizeof msgs / sizeof msgs[0]); m++)
        for (int a = 0; a < 6; a++)
            for (int b = 0; b < 6; b++) {
                LPARAM l = U(PackDDElParam)(msgs[m], vals[a], vals[b]);
                LPARAM want = (LPARAM)(int)((UINT)(vals[a] & 0xFFFF) | ((UINT)vals[b] << 16));
                CHECKF(l == want, "msg %x lo %llx hi %llx -> %llx want %llx", msgs[m], (unsigned long long)vals[a], (unsigned long long)vals[b], (long long)l, (long long)want);
                UINT_PTR lo = 0xAA, hi = 0xBB;
                BOOL ok = U(UnpackDDElParam)(msgs[m], l, &lo, &hi);
                CHECK(ok);
                CHECKF(lo == (vals[a] & 0xFFFF), "unpack lo %llx", (unsigned long long)lo);
                CHECKF(hi == (vals[b] & 0xFFFF), "unpack hi %llx", (unsigned long long)hi);
                CHECK(U(FreeDDElParam)(msgs[m], l));
            }
}
TEST(misc_ui_ddepack_two_words_truncates_and_sign_extends) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_INITIATE, 0x12345678, 0x9ABCDEF0);
    CHECK(l == (LPARAM)(int)(0x5678 | (0xDEF00000u)));
    CHECK(l < 0);                                   /* top bit of HIWORD sign-extends to 64 bits */
    UINT_PTR lo, hi;
    CHECK(U(UnpackDDElParam)(WMDDE_INITIATE, l, &lo, &hi));
    CHECK(lo == 0x5678 && hi == 0xDEF0);
    l = U(PackDDElParam)(WMDDE_REQUEST, 0xFFFF, 0x7FFF);
    CHECK(l == 0x7FFFFFFF);
    l = U(PackDDElParam)(WMDDE_REQUEST, 0, 0x8000);
    CHECK((ULONGLONG)l == 0xFFFFFFFF80000000ULL);
    CHECK(U(UnpackDDElParam)(WMDDE_REQUEST, l, &lo, &hi) && lo == 0 && hi == 0x8000);
}
TEST(misc_ui_ddepack_unpack_null_outputs) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_TERMINATE, 7, 9);
    UINT_PTR v = 0xAA;
    CHECK(U(UnpackDDElParam)(WMDDE_TERMINATE, l, &v, NULL) && v == 7);
    v = 0xAA;
    CHECK(U(UnpackDDElParam)(WMDDE_TERMINATE, l, NULL, &v) && v == 9);
    CHECK(U(UnpackDDElParam)(WMDDE_TERMINATE, l, NULL, NULL));
    /* arbitrary lParam values decode as two words */
    CHECK(U(UnpackDDElParam)(WM_USER, (LPARAM)0x1122334455667788LL, &v, NULL) && v == 0x7788);
    UINT_PTR hi;
    CHECK(U(UnpackDDElParam)(WM_USER, (LPARAM)0x1122334455667788LL, NULL, &hi) && hi == 0x5566);
    CHECK(U(UnpackDDElParam)(WM_USER, (LPARAM)-1, &v, &hi) && v == 0xFFFF && hi == 0xFFFF);
    CHECK(U(UnpackDDElParam)(WM_USER, 0, &v, &hi) && v == 0 && hi == 0);
}
TEST(misc_ui_ddepack_free_two_word_is_noop_true) {
    NEED_PACK();
    for (UINT m = 0x3D0; m < 0x400; m++) {
        if (is_mem_msg(m)) continue;
        LPARAM l = U(PackDDElParam)(m, 1, 2);
        CHECKF(U(FreeDDElParam)(m, l), "msg %x", m);
        CHECKF(U(FreeDDElParam)(m, 0), "msg %x zero", m);
        CHECKF(U(FreeDDElParam)(m, (LPARAM)-1), "msg %x -1", m);
    }
}

/* ---------- packing: global memory ---------- */
TEST(misc_ui_ddepack_memory_types_allocate) {
    NEED_PACK();
    static const UINT msgs[] = { WMDDE_ADVISE, WMDDE_ACK, WMDDE_DATA, WMDDE_POKE };
    for (int i = 0; i < 4; i++) {
        LPARAM l = U(PackDDElParam)(msgs[i], 0x1122334455667788ULL, 0x8899AABBCCDDEEFFULL);
        CHECKF(l != 0 && (ULONGLONG)l > 0xFFFF, "msg %x l=%llx", msgs[i], (long long)l);
        if (!l) continue;
        SIZE_T sz = GlobalSize((HGLOBAL)l);
        CHECKF(sz >= 16, "msg %x size %llu", msgs[i], (unsigned long long)sz);
        UINT_PTR lo = 0, hi = 0;
        CHECK(U(UnpackDDElParam)(msgs[i], l, &lo, &hi));
        CHECK(lo == 0x1122334455667788ULL);
        CHECK(hi == 0x8899AABBCCDDEEFFULL);
        /* unpack twice: non destructive */
        lo = hi = 0;
        CHECK(U(UnpackDDElParam)(msgs[i], l, &lo, &hi) && lo == 0x1122334455667788ULL && hi == 0x8899AABBCCDDEEFFULL);
        CHECK(U(FreeDDElParam)(msgs[i], l));
    }
}
TEST(misc_ui_ddepack_memory_free_invalidates) {
    NEED_PACK();
    static const UINT msgs[] = { WMDDE_ADVISE, WMDDE_ACK, WMDDE_DATA, WMDDE_POKE };
    for (int i = 0; i < 4; i++) {
        LPARAM l = U(PackDDElParam)(msgs[i], 5, 6);
        CHECK(U(FreeDDElParam)(msgs[i], l));
        UINT_PTR lo = 0x77, hi = 0x88;
        BOOL ok = U(UnpackDDElParam)(msgs[i], l, &lo, &hi);
        CHECKF(!ok, "unpack of freed block, msg %x", msgs[i]);
        CHECK(lo == 0 && hi == 0);
        CHECK(U(FreeDDElParam)(msgs[i], l));              /* double free is harmless */
    }
}
TEST(misc_ui_ddepack_memory_unpack_invalid_lparam) {
    NEED_PACK();
    static const LPARAM bad[] = { 0, 1, 0x1234, 0xFFFF, (LPARAM)-1 };
    static const UINT msgs[] = { WMDDE_ADVISE, WMDDE_ACK, WMDDE_DATA, WMDDE_POKE };
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 5; j++) {
            UINT_PTR lo = 0x77, hi = 0x88;
            BOOL ok = U(UnpackDDElParam)(msgs[i], bad[j], &lo, &hi);
            CHECKF(!ok, "msg %x lparam %llx", msgs[i], (long long)bad[j]);
            CHECKF(lo == 0 && hi == 0, "outputs not zeroed msg %x lparam %llx", msgs[i], (long long)bad[j]);
            CHECK(!U(UnpackDDElParam)(msgs[i], bad[j], NULL, NULL));
            CHECK(U(FreeDDElParam)(msgs[i], bad[j]));
        }
}
TEST(misc_ui_ddepack_memory_null_outputs) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_ACK, 11, 22);
    UINT_PTR v = 0;
    CHECK(U(UnpackDDElParam)(WMDDE_ACK, l, &v, NULL) && v == 11);
    CHECK(U(UnpackDDElParam)(WMDDE_ACK, l, NULL, &v) && v == 22);
    CHECK(U(UnpackDDElParam)(WMDDE_ACK, l, NULL, NULL));
    U(FreeDDElParam)(WMDDE_ACK, l);
}
TEST(misc_ui_ddepack_free_with_wrong_message_leaks_nothing_harmful) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_DATA, 1, 2);
    /* freeing as a two-word message does not touch the block */
    CHECK(U(FreeDDElParam)(WMDDE_INITIATE, l));
    UINT_PTR lo = 0, hi = 0;
    CHECK(U(UnpackDDElParam)(WMDDE_DATA, l, &lo, &hi) && lo == 1 && hi == 2);
    CHECK(U(FreeDDElParam)(WMDDE_DATA, l));
}
TEST(misc_ui_ddepack_many_alloc_free) {
    NEED_PACK();
    enum { N = 500 };
    static LPARAM ls[N];
    for (int i = 0; i < N; i++) {
        ls[i] = U(PackDDElParam)(WMDDE_ACK, i, i * 3);
        if (!ls[i]) { CHECK(0); break; }
    }
    for (int i = 0; i < N; i++) {
        UINT_PTR lo = 0, hi = 0;
        if (!ls[i]) continue;
        CHECK(U(UnpackDDElParam)(WMDDE_ACK, ls[i], &lo, &hi) && lo == (UINT_PTR)i && hi == (UINT_PTR)i * 3);
        CHECK(U(FreeDDElParam)(WMDDE_ACK, ls[i]));
    }
}
TEST(misc_ui_ddepack_execute_passes_through) {
    NEED_PACK();
    static const LPARAM vals[] = { 0, 1, 0x1234, (LPARAM)0x123456789ABCDEFLL, (LPARAM)-1 };
    for (int i = 0; i < 5; i++) {
        CHECK(U(PackDDElParam)(WMDDE_EXECUTE, 0x55, vals[i]) == vals[i]);
        CHECK(U(PackDDElParam)(WMDDE_EXECUTE, 0, vals[i]) == vals[i]);
        UINT_PTR lo = 0x66, hi = 0x77;
        CHECK(U(UnpackDDElParam)(WMDDE_EXECUTE, vals[i], &lo, &hi));
        CHECK(lo == 0);
        CHECK(hi == (UINT_PTR)vals[i]);
        CHECK(U(UnpackDDElParam)(WMDDE_EXECUTE, vals[i], NULL, NULL));
        CHECK(U(FreeDDElParam)(WMDDE_EXECUTE, vals[i]));
    }
}

/* ---------- ReuseDDElParam ---------- */
TEST(misc_ui_ddepack_reuse_memory_to_memory_keeps_block) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_ADVISE, 1, 2);
    LPARAM r = U(ReuseDDElParam)(l, WMDDE_ADVISE, WMDDE_DATA, 30, 40);
    CHECK(r == l);
    UINT_PTR lo = 0, hi = 0;
    CHECK(U(UnpackDDElParam)(WMDDE_DATA, r, &lo, &hi) && lo == 30 && hi == 40);
    r = U(ReuseDDElParam)(r, WMDDE_ACK, WMDDE_POKE, 50, 60);
    CHECK(r == l);
    CHECK(U(UnpackDDElParam)(WMDDE_POKE, r, &lo, &hi) && lo == 50 && hi == 60);
    CHECK(U(FreeDDElParam)(WMDDE_POKE, r));
}
TEST(misc_ui_ddepack_reuse_memory_to_words_frees_block) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_ACK, 1, 2);
    LPARAM r = U(ReuseDDElParam)(l, WMDDE_ACK, WMDDE_TERMINATE, 0x11, 0x22);
    CHECK(r == (LPARAM)((0x22 << 16) | 0x11));
    UINT_PTR lo = 5, hi = 5;
    CHECK(!U(UnpackDDElParam)(WMDDE_ACK, l, &lo, &hi));        /* old block is gone */
    CHECK(U(UnpackDDElParam)(WMDDE_TERMINATE, r, &lo, &hi) && lo == 0x11 && hi == 0x22);
}
TEST(misc_ui_ddepack_reuse_words_to_memory_allocates) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_TERMINATE, 1, 2);
    LPARAM r = U(ReuseDDElParam)(l, WMDDE_TERMINATE, WMDDE_ACK, 0x1111, 0x2222);
    CHECK(r != 0 && (ULONGLONG)r > 0xFFFF);
    UINT_PTR lo = 0, hi = 0;
    CHECK(U(UnpackDDElParam)(WMDDE_ACK, r, &lo, &hi) && lo == 0x1111 && hi == 0x2222);
    CHECK(U(FreeDDElParam)(WMDDE_ACK, r));
}
TEST(misc_ui_ddepack_reuse_words_to_words) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_INITIATE, 1, 2);
    LPARAM r = U(ReuseDDElParam)(l, WMDDE_INITIATE, WMDDE_UNADVISE, 0x3333, 0x4444);
    CHECK(r == (LPARAM)((0x4444 << 16) | 0x3333));
    LPARAM e = U(ReuseDDElParam)(l, WMDDE_REQUEST, WMDDE_EXECUTE, 9, 0x5555);
    CHECK(e == 0x5555);
}
TEST(misc_ui_ddepack_reuse_to_execute_frees_block) {
    NEED_PACK();
    LPARAM l = U(PackDDElParam)(WMDDE_DATA, 1, 2);
    LPARAM r = U(ReuseDDElParam)(l, WMDDE_DATA, WMDDE_EXECUTE, 9, 0xABCD);
    CHECK(r == 0xABCD);
    /* The packed block is freed by the reuse; Win10 22H2 corrupts the heap (0xc0000374) if it is unpacked
     * afterwards, so the freed handle must not be touched. */
}
TEST(misc_ui_ddepack_reuse_null_lparam) {
    NEED_PACK();
    CHECK(U(ReuseDDElParam)(0, WMDDE_ACK, WMDDE_DATA, 1, 2) == 0);
    LPARAM x = U(ReuseDDElParam)(0, WMDDE_ACK, WMDDE_TERMINATE, 1, 2);
    CHECK(x == (LPARAM)((2 << 16) | 1));
}

/* ---------- DDEML ---------- */
static HDDEDATA CALLBACK dd_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA d, ULONG_PTR a, ULONG_PTR b) { return (HDDEDATA)0; }

#define NEED_DDE() REQUIRE(U(DdeInitializeA)); REQUIRE(U(DdeUninitialize)); REQUIRE(U(DdeGetLastError)); \
    REQUIRE(U(DdeCreateStringHandleA)); REQUIRE(U(DdeFreeStringHandle))

static DWORD dd_init(DWORD flags)
{
    DWORD id = 0;
    UINT r = U(DdeInitializeA)(&id, dd_cb, flags, 0);
    if (r != DMLERR_NO_ERROR) return 0;
    return id;
}

TEST(misc_ui_dde_init_basic_lifecycle) {
    NEED_DDE();
    DWORD id = 0xFFFFFFFF;
    id = 0;
    UINT r = U(DdeInitializeA)(&id, dd_cb, APPCLASS_STANDARD, 0);
    CHECK(r == DMLERR_NO_ERROR);
    CHECK(id != 0);
    CHECK(U(DdeGetLastError)(id) == DMLERR_NO_ERROR);
    CHECK(U(DdeUninitialize)(id));
    CHECK(!U(DdeUninitialize)(id));
}
TEST(misc_ui_dde_init_wide_basic_lifecycle) {
    NEED_DDE(); REQUIRE(U(DdeInitializeW));
    DWORD id = 0;
    CHECK(U(DdeInitializeW)(&id, dd_cb, APPCLASS_STANDARD, 0) == DMLERR_NO_ERROR);
    CHECK(id != 0);
    CHECK(U(DdeUninitialize)(id));
}
TEST(misc_ui_dde_init_reserved_param_nonzero) {
    NEED_DDE(); REQUIRE(U(DdeInitializeW));
    DWORD id = 0;
    CHECK(U(DdeInitializeA)(&id, dd_cb, APPCLASS_STANDARD, 1) == DMLERR_INVALIDPARAMETER);
    CHECK(id == 0);
    CHECK(U(DdeInitializeW)(&id, dd_cb, APPCLASS_STANDARD, 0x1234) == DMLERR_INVALIDPARAMETER);
    CHECK(id == 0);
    CHECK(U(DdeInitializeA)(&id, dd_cb, APPCLASS_STANDARD, (DWORD)-1) == DMLERR_INVALIDPARAMETER);
    CHECK(id == 0);
}
TEST(misc_ui_dde_init_bogus_instance_id) {
    NEED_DDE();
    DWORD bogus[] = { 0x12345678, 0xFFFFFFFF, 1, 0x7FFFFFFF, 0x80000000 };
    for (int i = 0; i < 5; i++) {
        DWORD id = bogus[i];
        UINT r = U(DdeInitializeA)(&id, dd_cb, APPCLASS_STANDARD, 0);
        CHECKF(r == DMLERR_INVALIDPARAMETER, "id %lx -> %x", bogus[i], r);
        CHECK(id == bogus[i]);
    }
}
TEST(misc_ui_dde_init_reinit_updates_flags) {
    NEED_DDE();
    DWORD id = 0;
    CHECK(U(DdeInitializeA)(&id, dd_cb, APPCLASS_STANDARD, 0) == DMLERR_NO_ERROR);
    DWORD first = id;
    CHECK(U(DdeInitializeA)(&id, dd_cb, APPCLASS_STANDARD | CBF_SKIP_ALLNOTIFICATIONS, 0) == DMLERR_NO_ERROR);
    CHECK(id == first);
    CHECK(U(DdeUninitialize)(id));
}
TEST(misc_ui_dde_init_multiple_instances_distinct) {
    NEED_DDE();
    DWORD a = dd_init(APPCLASS_STANDARD), b = dd_init(APPCLASS_STANDARD), c = dd_init(APPCMD_CLIENTONLY);
    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
    CHECK(U(DdeUninitialize)(b));
    CHECK(U(DdeGetLastError)(a) == DMLERR_NO_ERROR);
    CHECK(U(DdeGetLastError)(b) == DMLERR_INVALIDPARAMETER);
    CHECK(U(DdeUninitialize)(a));
    CHECK(U(DdeUninitialize)(c));
}
TEST(misc_ui_dde_init_many_instances) {
    NEED_DDE();
    DWORD ids[40]; int n = 0;
    for (int i = 0; i < 40; i++) { ids[i] = dd_init(APPCMD_CLIENTONLY); if (!ids[i]) break; n++; }
    t_note("created %d DDEML instances before failure/limit", n);
    CHECK(n >= 8);
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) CHECK(ids[i] != ids[j]);
    for (int i = 0; i < n; i++) CHECK(U(DdeUninitialize)(ids[i]));
}
TEST(misc_ui_dde_init_null_callback) {
    NEED_DDE();
    DWORD id = 0;
    UINT r = U(DdeInitializeA)(&id, NULL, APPCLASS_STANDARD, 0);
    t_note("DdeInitialize(pfnCallback=NULL) -> 0x%x id=%lx", r, id);
    if (r == DMLERR_NO_ERROR && id) U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_init_monitor_flags) {
    NEED_DDE();
    DWORD id = 0;
    UINT r = U(DdeInitializeA)(&id, dd_cb, APPCLASS_MONITOR | MF_CALLBACKS, 0);
    t_note("APPCLASS_MONITOR init -> 0x%x", r);
    if (r == DMLERR_NO_ERROR && id) CHECK(U(DdeUninitialize)(id));
}
TEST(misc_ui_dde_uninit_invalid) {
    NEED_DDE();
    DWORD bad[] = { 0, 1, 0x12345678, 0xFFFFFFFF, 0x80000001 };
    for (int i = 0; i < 5; i++) CHECKF(!U(DdeUninitialize)(bad[i]), "id %lx", bad[i]);
}
TEST(misc_ui_dde_getlasterror_semantics) {
    NEED_DDE();
    DWORD bad[] = { 0, 1, 0x12345678, 0xFFFFFFFF };
    for (int i = 0; i < 4; i++) CHECKF(U(DdeGetLastError)(bad[i]) == DMLERR_INVALIDPARAMETER, "id %lx", bad[i]);
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id);
    CHECK(U(DdeGetLastError)(id) == DMLERR_NO_ERROR);
    /* provoke an error, read it, and see that reading clears it */
    HSZ h = U(DdeCreateStringHandleA)(id, "MuErrClear", CP_WINANSI);
    CHECK(h != NULL);
    CHECK(U(DdeFreeStringHandle)(id, h));
    t_note("DdeFreeStringHandle on a released handle -> %d", U(DdeFreeStringHandle)(id, h));   /* TRUE on Win10 22H2, error still recorded */
    CHECK(U(DdeGetLastError)(id) == DMLERR_INVALIDPARAMETER);
    CHECK(U(DdeGetLastError)(id) == DMLERR_NO_ERROR);
    CHECK(U(DdeGetLastError)(id) == DMLERR_NO_ERROR);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_error_is_per_instance) {
    NEED_DDE();
    DWORD a = dd_init(APPCLASS_STANDARD), b = dd_init(APPCLASS_STANDARD);
    CHECK(a && b);
    HSZ h = U(DdeCreateStringHandleA)(a, "MuPerInst", CP_WINANSI);
    U(DdeFreeStringHandle)(a, h);
    U(DdeFreeStringHandle)(a, h);                    /* error on a */
    CHECK(U(DdeGetLastError)(b) == DMLERR_NO_ERROR);
    CHECK(U(DdeGetLastError)(a) == DMLERR_INVALIDPARAMETER);
    U(DdeUninitialize)(a); U(DdeUninitialize)(b);
}

/* ---------- string handles ---------- */
TEST(misc_ui_dde_string_create_query) {
    NEED_DDE(); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeQueryStringW));
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ h = U(DdeCreateStringHandleA)(id, "MuStringOne", CP_WINANSI);
    CHECK(h != NULL);
    CHECK((ULONG_PTR)h <= 0xFFFF);                    /* handles are 16-bit atoms */
    CHECK((ULONG_PTR)h >= 0xC000);
    char b[64]; WCHAR w[64];
    CHECK(U(DdeQueryStringA)(id, h, NULL, 0, CP_WINANSI) == 11);
    memset(b, 'x', sizeof b);
    CHECK(U(DdeQueryStringA)(id, h, b, 64, CP_WINANSI) == 11 && !strcmp(b, "MuStringOne"));
    memset(b, 'x', sizeof b);
    CHECK(U(DdeQueryStringA)(id, h, b, 12, CP_WINANSI) == 11 && !strcmp(b, "MuStringOne"));
    memset(b, 'x', sizeof b);
    CHECK(U(DdeQueryStringA)(id, h, b, 64, 0) == 11 && !strcmp(b, "MuStringOne"));
    DWORD n = U(DdeQueryStringA)(id, h, b, 5, CP_WINANSI);
    t_note("DdeQueryStringA with cchMax=5 on 11 char string: returned %lu, text \"%.8s\"", n, b);
    CHECK(U(DdeQueryStringW)(id, h, NULL, 0, CP_WINUNICODE) == 11);
    CHECK(U(DdeQueryStringW)(id, h, NULL, 0, 0) == 11);
    for (int i = 0; i < 64; i++) w[i] = L'x';
    CHECK(U(DdeQueryStringW)(id, h, w, 64, CP_WINUNICODE) == 11 && !lstrcmpW(w, L"MuStringOne"));
    for (int i = 0; i < 64; i++) w[i] = L'x';
    CHECK(U(DdeQueryStringW)(id, h, w, 64, 0) == 11 && !lstrcmpW(w, L"MuStringOne"));
    CHECK(U(DdeFreeStringHandle)(id, h));
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_string_wide_create) {
    NEED_DDE(); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeQueryStringW)); REQUIRE(U(DdeQueryStringA));
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ h = U(DdeCreateStringHandleW)(id, L"MuWideStr", CP_WINUNICODE);
    CHECK(h != NULL);
    HSZ h0 = U(DdeCreateStringHandleW)(id, L"MuWideStr", 0);
    CHECK(h0 == h);
    HSZ ha = U(DdeCreateStringHandleA)(id, "MuWideStr", CP_WINANSI);
    CHECK(ha == h);                                   /* same atom regardless of codepage entry */
    WCHAR w[32]; char a[32];
    CHECK(U(DdeQueryStringW)(id, h, w, 32, CP_WINUNICODE) == 9 && !lstrcmpW(w, L"MuWideStr"));
    CHECK(U(DdeQueryStringA)(id, h, a, 32, CP_WINANSI) == 9 && !strcmp(a, "MuWideStr"));
    CHECK(U(DdeFreeStringHandle)(id, h));
    CHECK(U(DdeFreeStringHandle)(id, h0));
    CHECK(U(DdeFreeStringHandle)(id, ha));
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_string_case_insensitive_same_handle) {
    NEED_DDE(); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeCmpStringHandles));
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ a = U(DdeCreateStringHandleA)(id, "MuCaseTest", CP_WINANSI);
    HSZ b = U(DdeCreateStringHandleA)(id, "MUCASETEST", CP_WINANSI);
    CHECK(a != NULL && a == b);
    CHECK(U(DdeCmpStringHandles)(a, b) == 0);
    char buf[32];
    U(DdeQueryStringA)(id, b, buf, 32, CP_WINANSI);
    t_note("case of stored string: \"%s\"", buf);
    CHECK(U(DdeFreeStringHandle)(id, a));
    CHECK(U(DdeFreeStringHandle)(id, b));
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_string_refcount_free_keep) {
    NEED_DDE(); REQUIRE(U(DdeKeepStringHandle)); REQUIRE(U(DdeQueryStringA));
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ h = U(DdeCreateStringHandleA)(id, "MuRefcount", CP_WINANSI);
    CHECK(h);
    CHECK(U(DdeKeepStringHandle)(id, h));
    CHECK(U(DdeKeepStringHandle)(id, h));
    char b[32];
    CHECK(U(DdeFreeStringHandle)(id, h));
    CHECK(U(DdeFreeStringHandle)(id, h));
    CHECK(U(DdeQueryStringA)(id, h, b, 32, CP_WINANSI) == 10);        /* still alive: 3 refs, 2 freed */
    CHECK(U(DdeFreeStringHandle)(id, h));
    /* one too many: Win10 22H2 returns TRUE but still records DMLERR_INVALIDPARAMETER (checked below) */
    t_note("DdeFreeStringHandle on a released handle -> %d", U(DdeFreeStringHandle)(id, h));
    CHECK(U(DdeGetLastError)(id) == DMLERR_INVALIDPARAMETER);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_string_empty_is_null_handle) {
    NEED_DDE(); REQUIRE(U(DdeCreateStringHandleW)); REQUIRE(U(DdeQueryStringA)); REQUIRE(U(DdeKeepStringHandle));
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    CHECK(U(DdeCreateStringHandleA)(id, "", CP_WINANSI) == NULL);
    CHECK(U(DdeCreateStringHandleW)(id, L"", CP_WINUNICODE) == NULL);
    CHECK(U(DdeGetLastError)(id) == DMLERR_NO_ERROR);
    /* NULL handle is a valid operand */
    CHECK(U(DdeFreeStringHandle)(id, NULL));
    CHECK(U(DdeKeepStringHandle)(id, NULL));
    char b[8] = "zzzzzzz";
    DWORD n = U(DdeQueryStringA)(id, NULL, b, 8, CP_WINANSI);
    t_note("DdeQueryStringA(NULL hsz) -> %lu, b[0]=%d", n, b[0]);
    CHECK(b[0] == 0);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_string_invalid_instance) {
    NEED_DDE(); REQUIRE(U(DdeKeepStringHandle)); REQUIRE(U(DdeQueryStringA));
    char b[8];
    CHECK(U(DdeCreateStringHandleA)(0, "x", CP_WINANSI) == NULL);
    CHECK(U(DdeCreateStringHandleA)(0x12345678, "x", CP_WINANSI) == NULL);
    CHECK(!U(DdeFreeStringHandle)(0, (HSZ)0xC001));
    CHECK(!U(DdeFreeStringHandle)(0x12345678, (HSZ)0xC001));
    CHECK(!U(DdeKeepStringHandle)(0x12345678, (HSZ)0xC001));
    CHECK(U(DdeQueryStringA)(0x12345678, (HSZ)0xC001, b, 8, CP_WINANSI) == 0);
}
TEST(misc_ui_dde_string_after_uninit) {
    NEED_DDE();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ h = U(DdeCreateStringHandleA)(id, "MuAfterUninit", CP_WINANSI);
    CHECK(U(DdeUninitialize)(id));
    CHECK(U(DdeCreateStringHandleA)(id, "MuAfterUninit2", CP_WINANSI) == NULL);
    CHECK(!U(DdeFreeStringHandle)(id, h));
    CHECK(U(DdeGetLastError)(id) == DMLERR_INVALIDPARAMETER);
}
TEST(misc_ui_dde_string_max_length) {
    NEED_DDE();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    char s[400];
    memset(s, 'q', sizeof s);
    s[255] = 0;
    HSZ h = U(DdeCreateStringHandleA)(id, s, CP_WINANSI);
    t_note("255-char string handle: %p", h);
    if (h) U(DdeFreeStringHandle)(id, h);
    s[256] = 'q'; s[256] = 0;
    s[300] = 0;
    memset(s, 'q', 300);
    s[300] = 0;
    h = U(DdeCreateStringHandleA)(id, s, CP_WINANSI);
    CHECK(h == NULL);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_string_compare_by_handle_value) {
    NEED_DDE(); REQUIRE(U(DdeCmpStringHandles));
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ a = U(DdeCreateStringHandleA)(id, "MuCmpZZZ", CP_WINANSI);
    HSZ b = U(DdeCreateStringHandleA)(id, "MuCmpAAA", CP_WINANSI);
    CHECK(a && b && a != b);
    CHECK(U(DdeCmpStringHandles)(a, a) == 0);
    CHECK(U(DdeCmpStringHandles)(NULL, NULL) == 0);
    CHECK(U(DdeCmpStringHandles)(NULL, a) == -1);
    CHECK(U(DdeCmpStringHandles)(a, NULL) == 1);
    CHECK(U(DdeCmpStringHandles)(a, b) == ((ULONG_PTR)a < (ULONG_PTR)b ? -1 : 1));
    CHECK(U(DdeCmpStringHandles)(b, a) == ((ULONG_PTR)b < (ULONG_PTR)a ? -1 : 1));
    CHECK(U(DdeCmpStringHandles)((HSZ)5, (HSZ)9) == -1);
    CHECK(U(DdeCmpStringHandles)((HSZ)9, (HSZ)5) == 1);
    U(DdeFreeStringHandle)(id, a); U(DdeFreeStringHandle)(id, b);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_string_shared_between_instances) {
    NEED_DDE(); REQUIRE(U(DdeQueryStringA));
    DWORD a = dd_init(APPCLASS_STANDARD), b = dd_init(APPCLASS_STANDARD);
    CHECK(a && b);
    HSZ ha = U(DdeCreateStringHandleA)(a, "MuShared", CP_WINANSI);
    HSZ hb = U(DdeCreateStringHandleA)(b, "MuShared", CP_WINANSI);
    CHECK(ha && ha == hb);
    char buf[16];
    CHECK(U(DdeQueryStringA)(b, ha, buf, 16, CP_WINANSI) == 8);
    CHECK(U(DdeFreeStringHandle)(a, ha));
    CHECK(U(DdeQueryStringA)(b, hb, buf, 16, CP_WINANSI) == 8);
    CHECK(U(DdeFreeStringHandle)(b, hb));
    U(DdeUninitialize)(a); U(DdeUninitialize)(b);
}

/* ---------- data handles ---------- */
#define NEED_DATA() NEED_DDE(); REQUIRE(U(DdeCreateDataHandle)); REQUIRE(U(DdeFreeDataHandle)); REQUIRE(U(DdeGetData)); \
    REQUIRE(U(DdeAccessData)); REQUIRE(U(DdeUnaccessData)); REQUIRE(U(DdeAddData))

TEST(misc_ui_dde_data_create_access_free) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ item = U(DdeCreateStringHandleA)(id, "MuItem", CP_WINANSI);
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"hello", 6, 0, item, CF_TEXT, 0);
    CHECK(d != NULL);
    if (d) {
        DWORD sz = 0;
        LPBYTE p = U(DdeAccessData)(d, &sz);
        CHECK(p != NULL);
        CHECK(sz >= 6);
        if (sz != 6) t_note("DdeAccessData size %lu for 6 byte payload", sz);
        if (p) CHECK(!memcmp(p, "hello", 6));
        CHECK(U(DdeUnaccessData)(d));
        CHECK(U(DdeGetData)(d, NULL, 0, 0) >= 6);
        char b[16]; memset(b, 'x', sizeof b);
        CHECK(U(DdeGetData)(d, (LPBYTE)b, 16, 0) >= 6);
        CHECK(!memcmp(b, "hello", 6));
        memset(b, 'x', sizeof b);
        CHECK(U(DdeGetData)(d, (LPBYTE)b, 3, 0) == 3);
        CHECK(!memcmp(b, "hel", 3) && b[3] == 'x');
        memset(b, 'x', sizeof b);
        CHECK(U(DdeGetData)(d, (LPBYTE)b, 16, 2) >= 4);
        CHECK(!memcmp(b, "llo", 4));
        CHECK(U(DdeFreeDataHandle)(d));
        CHECK(!U(DdeFreeDataHandle)(d));
    }
    U(DdeFreeStringHandle)(id, item);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_create_without_item) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"abcdefgh", 8, 0, NULL, CF_TEXT, 0);
    CHECK(d != NULL);
    if (d) {
        DWORD sz = 0;
        LPBYTE p = U(DdeAccessData)(d, &sz);
        CHECK(p && sz >= 8);
        if (p) CHECK(!memcmp(p, "abcdefgh", 8));
        U(DdeUnaccessData)(d);
        CHECK(U(DdeGetData)(d, NULL, 0, 0) >= 8);
        CHECK(U(DdeFreeDataHandle)(d));
    }
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_create_with_offset) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ item = U(DdeCreateStringHandleA)(id, "MuOffItem", CP_WINANSI);
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"XYZ", 3, 5, item, CF_TEXT, 0);
    CHECK(d != NULL);
    if (d) {
        DWORD sz = 0;
        LPBYTE p = U(DdeAccessData)(d, &sz);
        CHECK(p && sz >= 8);
        if (p && sz >= 8) CHECK(!memcmp(p + 5, "XYZ", 3));
        U(DdeUnaccessData)(d);
        U(DdeFreeDataHandle)(d);
    }
    U(DdeFreeStringHandle)(id, item);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_create_null_source) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ item = U(DdeCreateStringHandleA)(id, "MuNullSrc", CP_WINANSI);
    HDDEDATA d = U(DdeCreateDataHandle)(id, NULL, 100, 0, item, CF_PRIVATEFIRST, 0);
    CHECK(d != NULL);
    if (d) {
        DWORD sz = 0;
        LPBYTE p = U(DdeAccessData)(d, &sz);
        CHECK(p && sz >= 100);
        if (p) memset(p, 0x5A, sz);
        U(DdeUnaccessData)(d);
        CHECK(U(DdeGetData)(d, NULL, 0, 0) >= 100);
        CHECK(U(DdeFreeDataHandle)(d));
    }
    HDDEDATA z = U(DdeCreateDataHandle)(id, NULL, 0, 0, item, CF_TEXT, 0);
    t_note("zero-length CF_TEXT data handle: %p", z);
    if (z) U(DdeFreeDataHandle)(z);
    U(DdeFreeStringHandle)(id, item);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_create_bad_args) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    CHECK(U(DdeCreateDataHandle)(id, NULL, 0xFFFFFFFF, 0, NULL, CF_TEXT, 0) == NULL);
    U(DdeGetLastError)(id);
    UINT bad[] = { 2, 3, 4, 0x10, 0x8000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (int i = 0; i < 7; i++) {
        HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"abc", 4, 0, NULL, CF_TEXT, bad[i]);
        CHECKF(d == NULL, "afCmd %x", bad[i]);
        CHECKF(U(DdeGetLastError)(id) == DMLERR_INVALIDPARAMETER, "afCmd %x err", bad[i]);
    }
    CHECK(U(DdeCreateDataHandle)(0, (LPBYTE)"abc", 4, 0, NULL, CF_TEXT, 0) == NULL);
    CHECK(U(DdeCreateDataHandle)(0x12345678, (LPBYTE)"abc", 4, 0, NULL, CF_TEXT, 0) == NULL);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_appowned) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ item = U(DdeCreateStringHandleA)(id, "MuAppOwned", CP_WINANSI);
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"own", 4, 0, item, CF_TEXT, HDATA_APPOWNED);
    CHECK(d != NULL);
    if (d) {
        DWORD sz = 0;
        LPBYTE p = U(DdeAccessData)(d, &sz);
        CHECK(p && !memcmp(p, "own", 4));
        U(DdeUnaccessData)(d);
        CHECK(U(DdeFreeDataHandle)(d));
        BOOL again = U(DdeFreeDataHandle)(d);
        p = U(DdeAccessData)(d, &sz);
        t_note("app-owned handle after DdeFreeDataHandle: second free=%d, access=%p", again, p);
        if (p) U(DdeUnaccessData)(d);
    }
    U(DdeFreeStringHandle)(id, item);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_invalid_handles) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HDDEDATA bad[] = { NULL, (HDDEDATA)1, (HDDEDATA)0x12345678, (HDDEDATA)(LONG_PTR)-1 };
    for (int i = 0; i < 4; i++) {
        DWORD sz = 0x77;
        char b[4];
        CHECKF(!U(DdeFreeDataHandle)(bad[i]), "free %p", bad[i]);
        CHECKF(U(DdeAccessData)(bad[i], &sz) == NULL, "access %p", bad[i]);
        CHECKF(!U(DdeUnaccessData)(bad[i]), "unaccess %p", bad[i]);
        CHECKF(U(DdeGetData)(bad[i], (LPBYTE)b, 4, 0) == 0, "getdata %p", bad[i]);
        CHECKF(U(DdeAddData)(bad[i], (LPBYTE)"x", 1, 0) == NULL, "adddata %p", bad[i]);
    }
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_getdata_offsets) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"0123456789", 10, 0, NULL, CF_TEXT, 0);
    CHECK(d != NULL);
    if (d) {
        DWORD total = U(DdeGetData)(d, NULL, 0, 0);
        CHECK(total >= 10);
        CHECK(U(DdeGetData)(d, NULL, 0, 4) == total - 4);
        char b[32]; memset(b, 'x', sizeof b);
        CHECK(U(DdeGetData)(d, (LPBYTE)b, 4, 4) == 4);
        CHECK(!memcmp(b, "4567", 4));
        CHECK(U(DdeGetData)(d, (LPBYTE)b, 0, 4) == 0);
        U(DdeGetLastError)(id);
        CHECK(U(DdeGetData)(d, NULL, 0, total) == 0);
        CHECK(U(DdeGetLastError)(id) == DMLERR_INVALIDPARAMETER);
        CHECK(U(DdeGetData)(d, (LPBYTE)b, 4, total + 100) == 0);
        U(DdeFreeDataHandle)(d);
    }
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_adddata_grows) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ item = U(DdeCreateStringHandleA)(id, "MuAdd", CP_WINANSI);
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"abc", 3, 0, item, CF_TEXT, 0);
    CHECK(d != NULL);
    if (d) {
        HDDEDATA d2 = U(DdeAddData)(d, (LPBYTE)"DEFGHIJ", 7, 3);
        CHECK(d2 != NULL);
        if (d2) {
            DWORD sz = 0;
            LPBYTE p = U(DdeAccessData)(d2, &sz);
            CHECK(p && sz >= 10);
            if (p && sz >= 10) CHECK(!memcmp(p, "abcDEFGHIJ", 10));
            U(DdeUnaccessData)(d2);
            /* overwrite in the middle */
            HDDEDATA d3 = U(DdeAddData)(d2, (LPBYTE)"--", 2, 1);
            CHECK(d3 != NULL);
            p = U(DdeAccessData)(d3, &sz);
            if (p && sz >= 10) CHECK(!memcmp(p, "a--DEFGHIJ", 10));
            U(DdeUnaccessData)(d3);
            /* null source only grows */
            HDDEDATA d4 = U(DdeAddData)(d3, NULL, 50, 10);
            CHECK(d4 != NULL);
            CHECK(U(DdeGetData)(d4, NULL, 0, 0) >= 60);
            U(DdeFreeDataHandle)(d4);
        }
    }
    U(DdeFreeStringHandle)(id, item);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_large) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    enum { N = 1 << 20 };
    LPBYTE src = malloc(N);
    for (int i = 0; i < N; i++) src[i] = (BYTE)(i * 7);
    HDDEDATA d = U(DdeCreateDataHandle)(id, src, N, 0, NULL, CF_PRIVATEFIRST + 1, 0);
    CHECK(d != NULL);
    if (d) {
        DWORD sz = 0;
        LPBYTE p = U(DdeAccessData)(d, &sz);
        CHECK(p && sz >= N);
        if (p) CHECK(!memcmp(p, src, N));
        U(DdeUnaccessData)(d);
        U(DdeFreeDataHandle)(d);
    }
    free(src);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_data_freed_by_uninitialize) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HDDEDATA d = U(DdeCreateDataHandle)(id, (LPBYTE)"abc", 4, 0, NULL, CF_TEXT, 0);
    CHECK(d != NULL);
    CHECK(U(DdeUninitialize)(id));
    DWORD sz;
    t_note("data handle after DdeUninitialize: access=%p free=%d", U(DdeAccessData)(d, &sz), U(DdeFreeDataHandle)(d));
}
TEST(misc_ui_dde_data_handle_value_properties) {
    NEED_DATA();
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HDDEDATA a = U(DdeCreateDataHandle)(id, (LPBYTE)"a", 2, 0, NULL, CF_TEXT, 0);
    HDDEDATA b = U(DdeCreateDataHandle)(id, (LPBYTE)"b", 2, 0, NULL, CF_TEXT, 0);
    CHECK(a && b && a != b);
    CHECK(((ULONG_PTR)a >> 32) == 0 && ((ULONG_PTR)b >> 32) == 0);
    t_note("data handle values %p %p", a, b);
    U(DdeFreeDataHandle)(a); U(DdeFreeDataHandle)(b);
    U(DdeUninitialize)(id);
}

/* ---------- conversations: no-server error paths ---------- */
TEST(misc_ui_dde_connect_no_server) {
    NEED_DDE(); REQUIRE(U(DdeConnect));
    DWORD id = dd_init(APPCMD_CLIENTONLY | CBF_SKIP_ALLNOTIFICATIONS);
    CHECK(id); if (!id) return;
    HSZ svc = U(DdeCreateStringHandleA)(id, "MuNoSuchService_7f3a", CP_WINANSI);
    HSZ top = U(DdeCreateStringHandleA)(id, "MuTopic", CP_WINANSI);
    HCONV c = U(DdeConnect)(id, svc, top, NULL);
    CHECK(c == NULL);
    CHECK(U(DdeGetLastError)(id) == DMLERR_NO_CONV_ESTABLISHED);
    U(DdeFreeStringHandle)(id, svc); U(DdeFreeStringHandle)(id, top);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_connect_invalid_instance) {
    NEED_DDE(); REQUIRE(U(DdeConnect));
    CHECK(U(DdeConnect)(0, NULL, NULL, NULL) == NULL);
    CHECK(U(DdeConnect)(0x12345678, (HSZ)0xC001, (HSZ)0xC002, NULL) == NULL);
    CHECK(U(DdeGetLastError)(0x12345678) == DMLERR_INVALIDPARAMETER);
}
TEST(misc_ui_dde_connectlist_no_server) {
    NEED_DDE(); REQUIRE(U(DdeConnectList)); REQUIRE(U(DdeQueryNextServer)); REQUIRE(U(DdeDisconnectList));
    DWORD id = dd_init(APPCMD_CLIENTONLY | CBF_SKIP_ALLNOTIFICATIONS);
    CHECK(id); if (!id) return;
    HSZ svc = U(DdeCreateStringHandleA)(id, "MuNoSuchService_9c1b", CP_WINANSI);
    HCONVLIST cl = U(DdeConnectList)(id, svc, NULL, NULL, NULL);
    t_note("DdeConnectList with no server -> %p", cl);
    if (cl) U(DdeDisconnectList)(cl);
    t_note("DdeConnectList no server: last error 0x%x", U(DdeGetLastError)(id));
    CHECK(U(DdeQueryNextServer)(NULL, NULL) == NULL);
    CHECK(!U(DdeDisconnectList)(NULL));
    CHECK(U(DdeConnectList)(0x12345678, NULL, NULL, NULL, NULL) == NULL);
    U(DdeFreeStringHandle)(id, svc);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_conv_apis_null_conv) {
    NEED_DDE(); REQUIRE(U(DdeDisconnect)); REQUIRE(U(DdeReconnect)); REQUIRE(U(DdeQueryConvInfo)); REQUIRE(U(DdeSetUserHandle));
    REQUIRE(U(DdeImpersonateClient)); REQUIRE(U(DdeAbandonTransaction)); REQUIRE(U(DdeClientTransaction));
    DWORD id = dd_init(APPCMD_CLIENTONLY);
    CHECK(id); if (!id) return;
    HCONV bogus[] = { NULL };
    for (int i = 0; i < 1; i++) {
        CHECKF(!U(DdeDisconnect)(bogus[i]), "disconnect %p", bogus[i]);
        CHECKF(U(DdeReconnect)(bogus[i]) == NULL, "reconnect %p", bogus[i]);
        CONVINFO ci; memset(&ci, 0, sizeof ci); ci.cb = sizeof ci;
        CHECKF(U(DdeQueryConvInfo)(bogus[i], QID_SYNC, &ci) == 0, "queryconvinfo %p", bogus[i]);
        CHECKF(!U(DdeSetUserHandle)(bogus[i], QID_SYNC, 5), "setuserhandle %p", bogus[i]);
        CHECKF(!U(DdeImpersonateClient)(bogus[i]), "impersonate %p", bogus[i]);
        DWORD res = 0x77;
        HDDEDATA r = U(DdeClientTransaction)(NULL, 0, bogus[i], NULL, 0, XTYP_REQUEST, 100, &res);
        CHECKF(r == NULL, "transaction %p", bogus[i]);
    }
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_clienttransaction_bad_args) {
    NEED_DDE(); REQUIRE(U(DdeClientTransaction));
    DWORD id = dd_init(APPCMD_CLIENTONLY);
    CHECK(id); if (!id) return;
    UINT types[] = { 0, XTYP_ADVSTART, XTYP_EXECUTE, XTYP_POKE, XTYP_REQUEST, 0xFFFF, XTYP_CONNECT };
    for (int i = 0; i < 7; i++) {
        HDDEDATA r = U(DdeClientTransaction)(NULL, 0, NULL, NULL, CF_TEXT, types[i], 50, NULL);
        CHECKF(r == NULL, "type %x", types[i]);
    }
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_abandon_and_enablecallback) {
    NEED_DDE(); REQUIRE(U(DdeAbandonTransaction)); REQUIRE(U(DdeEnableCallback));
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    BOOL a1 = U(DdeAbandonTransaction)(id, NULL, 0);
    t_note("DdeAbandonTransaction(all)=%d", a1);
    t_note("DdeAbandonTransaction(bogus instance) = %d", U(DdeAbandonTransaction)(0x12345678, NULL, 0));
    BOOL e1 = U(DdeEnableCallback)(id, NULL, EC_ENABLEALL);
    t_note("DdeEnableCallback(EC_ENABLEALL) = %d", e1);
    BOOL e2 = U(DdeEnableCallback)(id, NULL, EC_QUERYWAITING);
    t_note("DdeEnableCallback(EC_QUERYWAITING) = %d", e2);
    CHECK(!U(DdeEnableCallback)(0x12345678, NULL, EC_ENABLEALL));
    CHECK(!U(DdeEnableCallback)(0, NULL, EC_ENABLEALL));
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_nameservice_register_unregister) {
    NEED_DDE(); REQUIRE(U(DdeNameService));
    DWORD id = dd_init(APPCLASS_STANDARD | CBF_SKIP_REGISTRATIONS | CBF_SKIP_UNREGISTRATIONS);
    CHECK(id); if (!id) return;
    HSZ svc = U(DdeCreateStringHandleA)(id, "MuNameSvc_44e1", CP_WINANSI);
    HDDEDATA r = U(DdeNameService)(id, svc, NULL, DNS_REGISTER);
    CHECK(r != NULL);
    r = U(DdeNameService)(id, svc, NULL, DNS_UNREGISTER);
    CHECK(r != NULL);
    r = U(DdeNameService)(id, svc, NULL, DNS_UNREGISTER);
    t_note("unregister twice -> %p", r);
    r = U(DdeNameService)(id, NULL, NULL, DNS_UNREGISTER);
    CHECK(r != NULL);                             /* unregister all */
    r = U(DdeNameService)(id, svc, (HSZ)0xC001, DNS_REGISTER);
    t_note("DdeNameService(hsz2 != NULL) -> %p err 0x%x", r, U(DdeGetLastError)(id));
    CHECK(U(DdeNameService)(0x12345678, svc, NULL, DNS_REGISTER) == NULL);
    CHECK(U(DdeNameService)(0, svc, NULL, DNS_REGISTER) == NULL);
    U(DdeFreeStringHandle)(id, svc);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_nameservice_client_only) {
    NEED_DDE(); REQUIRE(U(DdeNameService));
    DWORD id = dd_init(APPCMD_CLIENTONLY);
    CHECK(id); if (!id) return;
    HSZ svc = U(DdeCreateStringHandleA)(id, "MuClientOnlySvc", CP_WINANSI);
    HDDEDATA r = U(DdeNameService)(id, svc, NULL, DNS_REGISTER);
    t_note("DdeNameService on CLIENTONLY instance -> %p err 0x%x", r, U(DdeGetLastError)(id));
    if (r) U(DdeNameService)(id, svc, NULL, DNS_UNREGISTER);
    U(DdeFreeStringHandle)(id, svc);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_postadvise_no_links) {
    NEED_DDE(); REQUIRE(U(DdePostAdvise));
    DWORD id = dd_init(APPCLASS_STANDARD);
    CHECK(id); if (!id) return;
    HSZ t = U(DdeCreateStringHandleA)(id, "MuAdvTopic", CP_WINANSI), i = U(DdeCreateStringHandleA)(id, "MuAdvItem", CP_WINANSI);
    BOOL r = U(DdePostAdvise)(id, t, i);
    t_note("DdePostAdvise with no links -> %d", r);
    CHECK(!U(DdePostAdvise)(0x12345678, t, i));
    CHECK(!U(DdePostAdvise)(0, t, i));
    U(DdeFreeStringHandle)(id, t); U(DdeFreeStringHandle)(id, i);
    U(DdeUninitialize)(id);
}
TEST(misc_ui_dde_qos_bad_args) {
    NEED_DDE(); REQUIRE(U(DdeSetQualityOfService));
    SECURITY_QUALITY_OF_SERVICE q; memset(&q, 0, sizeof q); q.Length = sizeof q; q.ImpersonationLevel = SecurityImpersonation;
    BOOL r = U(DdeSetQualityOfService)(BOGUS_HWND, &q, NULL);
    t_note("DdeSetQualityOfService(bogus hwnd) -> %d", r);
    r = U(DdeSetQualityOfService)(NULL, &q, NULL);
    t_note("DdeSetQualityOfService(NULL hwnd) -> %d", r);
    t_note("DdeSetQualityOfService(NULL hwnd) err=%lu", GetLastError());
}
TEST(misc_ui_dde_message_constants) {
    CHECK(WM_DDE_INITIATE == 0x3E0 && WM_DDE_TERMINATE == 0x3E1 && WM_DDE_ADVISE == 0x3E2 && WM_DDE_UNADVISE == 0x3E3);
    CHECK(WM_DDE_ACK == 0x3E4 && WM_DDE_DATA == 0x3E5 && WM_DDE_REQUEST == 0x3E6 && WM_DDE_POKE == 0x3E7 && WM_DDE_EXECUTE == 0x3E8);
    CHECK(DMLERR_NO_ERROR == 0 && DMLERR_FIRST == 0x4000 && DMLERR_INVALIDPARAMETER == 0x4006 && DMLERR_NO_CONV_ESTABLISHED == 0x400A);
}
