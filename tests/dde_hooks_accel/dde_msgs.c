/* Low-level WM_DDE_* messages and the DDE lParam packing helpers. */
#include "dh.h"

#ifndef WM_DDE_FIRST
#define WM_DDE_FIRST 0x03E0
#endif

/* ---------- PackDDElParam / UnpackDDElParam / FreeDDElParam / ReuseDDElParam ---------- */
static const UINT g_packed_msgs[] = {WM_DDE_ADVISE, WM_DDE_ACK, WM_DDE_DATA, WM_DDE_POKE};
static const UINT g_plain_msgs[] = {WM_DDE_INITIATE, WM_DDE_TERMINATE, WM_DDE_UNADVISE, WM_DDE_REQUEST};

TEST(ddelparam_constants) {
    EQ(WM_DDE_INITIATE, 0x3E0); EQ(WM_DDE_TERMINATE, 0x3E1); EQ(WM_DDE_ADVISE, 0x3E2);
    EQ(WM_DDE_UNADVISE, 0x3E3); EQ(WM_DDE_ACK, 0x3E4); EQ(WM_DDE_DATA, 0x3E5);
    EQ(WM_DDE_REQUEST, 0x3E6); EQ(WM_DDE_POKE, 0x3E7); EQ(WM_DDE_EXECUTE, 0x3E8);
    EQ(WM_DDE_LAST, 0x3E8);
}
TEST(ddelparam_pack_roundtrip_packed_msgs) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam)); REQUIRE(U(FreeDDElParam));
    static const UINT_PTR vals[][2] = {{0, 0}, {1, 2}, {0x1234, 0x5678}, {0xFFFF, 0xFFFF}, {0x10000, 0x20000},
                                       {0x123456789ABCDEF0ull, 0x0FEDCBA987654321ull}, {~(UINT_PTR)0, ~(UINT_PTR)0}};
    for (unsigned m = 0; m < 4; m++)
        for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++) {
            LPARAM lp = U(PackDDElParam)(g_packed_msgs[m], vals[i][0], vals[i][1]);
            CHECKF(lp != 0, "msg 0x%x set %u", g_packed_msgs[m], i);
            if (!lp) continue;
            UINT_PTR lo = 0xAA, hi = 0xBB;
            BOOL ok = U(UnpackDDElParam)(g_packed_msgs[m], lp, &lo, &hi);
            CHECKF(ok, "unpack msg 0x%x set %u", g_packed_msgs[m], i);
            CHECKF(lo == vals[i][0] && hi == vals[i][1], "values msg 0x%x set %u: %llx %llx", g_packed_msgs[m], i, (unsigned long long)lo, (unsigned long long)hi);
            /* unpacking twice yields the same thing */
            lo = hi = 0;
            CHECK(U(UnpackDDElParam)(g_packed_msgs[m], lp, &lo, &hi));
            CHECK(lo == vals[i][0] && hi == vals[i][1]);
            CHECK(U(FreeDDElParam)(g_packed_msgs[m], lp));
        }
}
TEST(ddelparam_packed_is_a_handle) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(FreeDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_ACK, 1, 2);
    CHECK((ULONG_PTR)lp > 0xFFFF);
    UINT fl = GlobalFlags((HGLOBAL)lp);
    CHECK(fl != GMEM_INVALID_HANDLE);
    CHECK(GlobalSize((HGLOBAL)lp) >= 2 * sizeof(UINT_PTR));
    CHECK(U(FreeDDElParam)(WM_DDE_ACK, lp));
    t_note("GlobalFlags after free: 0x%x", GlobalFlags((HGLOBAL)lp));
}
TEST(ddelparam_plain_msgs_pack_into_32_bits) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam));
    for (unsigned m = 0; m < 4; m++) {
        UINT msg = g_plain_msgs[m];
        LPARAM lp = U(PackDDElParam)(msg, 0x1234, 0x5678);
        EQ(lp, 0x56781234);
        UINT_PTR lo = 0, hi = 0;
        CHECK(U(UnpackDDElParam)(msg, lp, &lo, &hi));
        EQ(lo, 0x1234); EQ(hi, 0x5678);
        /* only 16 bits of each survive */
        lp = U(PackDDElParam)(msg, 0x11234, 0x25678);
        EQ(lp, 0x56781234);
        CHECK(U(UnpackDDElParam)(msg, lp, &lo, &hi));
        EQ(lo, 0x1234); EQ(hi, 0x5678);
    }
}
TEST(ddelparam_plain_msgs_sign_extend) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_REQUEST, 0x1234, 0xFFFF);
    EQ(lp, (LPARAM)(LONG_PTR)(INT)0xFFFF1234);
    UINT_PTR lo = 0, hi = 0;
    CHECK(U(UnpackDDElParam)(WM_DDE_REQUEST, lp, &lo, &hi));
    EQ(lo, 0x1234); EQ(hi, 0xFFFF);
    lp = U(PackDDElParam)(WM_DDE_INITIATE, 0xFFFF, 0x8000);
    EQ(lp, (LPARAM)(LONG_PTR)(INT)0x8000FFFF);
}
TEST(ddelparam_execute_passes_hi_through) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam)); REQUIRE(U(FreeDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_EXECUTE, 0x55, 0x123456789ABCull);
    EQ(lp, 0x123456789ABCull);
    UINT_PTR lo = 0x77, hi = 0x88;
    CHECK(U(UnpackDDElParam)(WM_DDE_EXECUTE, lp, &lo, &hi));
    EQ(lo, 0);
    EQ(hi, 0x123456789ABCull);
    CHECK(U(FreeDDElParam)(WM_DDE_EXECUTE, lp));
}
TEST(ddelparam_other_messages_are_plain) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam));
    static const UINT msgs[] = {0, WM_USER, WM_DDE_FIRST - 1, WM_DDE_LAST + 1, WM_COMMAND, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof msgs / sizeof msgs[0]; i++) {
        LPARAM lp = U(PackDDElParam)(msgs[i], 0xABCD, 0x1234);
        CHECKF(lp == 0x1234ABCD, "msg 0x%x", msgs[i]);
        UINT_PTR lo = 0, hi = 0;
        CHECK(U(UnpackDDElParam)(msgs[i], lp, &lo, &hi));
        CHECKF(lo == 0xABCD && hi == 0x1234, "msg 0x%x", msgs[i]);
    }
}
TEST(ddelparam_unpack_null_outputs) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam)); REQUIRE(U(FreeDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_DATA, 11, 22);
    UINT_PTR lo = 0, hi = 0;
    CHECK(U(UnpackDDElParam)(WM_DDE_DATA, lp, &lo, NULL)); EQ(lo, 11);
    CHECK(U(UnpackDDElParam)(WM_DDE_DATA, lp, NULL, &hi)); EQ(hi, 22);
    CHECK(U(UnpackDDElParam)(WM_DDE_DATA, lp, NULL, NULL));
    U(FreeDDElParam)(WM_DDE_DATA, lp);
    CHECK(U(UnpackDDElParam)(WM_USER, 0x00020001, NULL, NULL));
}
TEST(ddelparam_unpack_invalid_handle) {
    REQUIRE(U(UnpackDDElParam));
    static const LPARAM bad[] = {0, 1, 0x1234, 0x12345678, (LPARAM)0xDEADBEEF, -1};
    for (unsigned m = 0; m < 4; m++)
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            UINT_PTR lo = 0x99, hi = 0x99;
            BOOL ok = U(UnpackDDElParam)(g_packed_msgs[m], bad[i], &lo, &hi);
            CHECKF(ok == FALSE, "msg 0x%x lp 0x%llx", g_packed_msgs[m], (unsigned long long)bad[i]);
            CHECKF(lo == 0 && hi == 0, "outputs zeroed msg 0x%x lp 0x%llx", g_packed_msgs[m], (unsigned long long)bad[i]);
        }
}
TEST(ddelparam_unpack_after_free) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam)); REQUIRE(U(FreeDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_POKE, 5, 6);
    CHECK(U(FreeDDElParam)(WM_DDE_POKE, lp));
    UINT_PTR lo = 9, hi = 9;
    BOOL ok = U(UnpackDDElParam)(WM_DDE_POKE, lp, &lo, &hi);
    t_note("Unpack after free -> %d lo=%llu hi=%llu", ok, (unsigned long long)lo, (unsigned long long)hi);
}
TEST(ddelparam_free_is_lenient) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(FreeDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_ADVISE, 1, 1);
    EQ(U(FreeDDElParam)(WM_DDE_ADVISE, lp), TRUE);
    EQ(U(FreeDDElParam)(WM_DDE_ADVISE, lp), TRUE);      /* double free is a no-op */
    EQ(U(FreeDDElParam)(WM_DDE_ACK, 0), TRUE);
    EQ(U(FreeDDElParam)(WM_DDE_ACK, 0x1234), TRUE);
    EQ(U(FreeDDElParam)(WM_DDE_DATA, (LPARAM)0xDEADBEEF), TRUE);
    EQ(U(FreeDDElParam)(WM_DDE_INITIATE, 0x10001), TRUE);
    EQ(U(FreeDDElParam)(WM_DDE_EXECUTE, 0x10001), TRUE);
    EQ(U(FreeDDElParam)(0, 0), TRUE);
}
TEST(ddelparam_free_ignored_for_plain_msgs) {
    REQUIRE(U(FreeDDElParam)); REQUIRE(U(PackDDElParam));
    /* one block per call: Win10 22H2 frees the block for some of these types, and a shared block would be freed twice
     * (heap corruption 0xc0000374). The blocks are not touched afterwards. */
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, 64);
    HGLOBAL h2 = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, 64);
    HGLOBAL h3 = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, 64);
    CHECK(h != NULL && h2 != NULL && h3 != NULL);
    /* WM_DDE_INITIATE and WM_DDE_EXECUTE carry no packed lParam */
    /* (a real GlobalAlloc block is not passed: Win10 22H2 corrupts the heap when one is handed to these types) */
    EQ(U(FreeDDElParam)(WM_DDE_INITIATE, 0), TRUE);
    EQ(U(FreeDDElParam)(WM_DDE_EXECUTE, 0), TRUE);
    (void)h2; (void)h3;
    /* a packed type frees the block: use a real packed block (a plain GlobalAlloc block is not one on Win10 22H2) */
    (void)h;
    { LPARAM pk = U(PackDDElParam)(WM_DDE_ACK, 1, 2);
      EQ(U(FreeDDElParam)(WM_DDE_ACK, pk), TRUE); }
    /* the freed block is not probed afterwards: GlobalFlags on it corrupts the heap on Win10 */
}
TEST(ddelparam_reuse_packed_to_packed) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam)); REQUIRE(U(ReuseDDElParam)); REQUIRE(U(FreeDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_DATA, 0x11, 0x22);
    LPARAM lp2 = U(ReuseDDElParam)(lp, WM_DDE_DATA, WM_DDE_ACK, 0x33, 0x44);
    EQ(lp2, lp);                          /* same block */
    UINT_PTR lo = 0, hi = 0;
    CHECK(U(UnpackDDElParam)(WM_DDE_ACK, lp2, &lo, &hi));
    EQ(lo, 0x33); EQ(hi, 0x44);
    CHECK(U(FreeDDElParam)(WM_DDE_ACK, lp2));
}
TEST(ddelparam_reuse_all_packed_pairs) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(UnpackDDElParam)); REQUIRE(U(ReuseDDElParam)); REQUIRE(U(FreeDDElParam));
    for (unsigned a = 0; a < 4; a++)
        for (unsigned b = 0; b < 4; b++) {
            LPARAM lp = U(PackDDElParam)(g_packed_msgs[a], 1, 2);
            LPARAM r = U(ReuseDDElParam)(lp, g_packed_msgs[a], g_packed_msgs[b], 0x1111111122222222ull, 0x3333);
            CHECKF(r == lp, "%u->%u same block", a, b);
            UINT_PTR lo = 0, hi = 0;
            CHECK(U(UnpackDDElParam)(g_packed_msgs[b], r, &lo, &hi));
            CHECKF(lo == 0x1111111122222222ull && hi == 0x3333, "%u->%u values", a, b);
            U(FreeDDElParam)(g_packed_msgs[b], r);
        }
}
TEST(ddelparam_reuse_packed_to_execute) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(ReuseDDElParam)); REQUIRE(U(UnpackDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_ACK, 1, 2);
    LPARAM r = U(ReuseDDElParam)(lp, WM_DDE_ACK, WM_DDE_EXECUTE, 0x10, 0xABCDEF12345ull);
    EQ(r, 0xABCDEF12345ull);
    /* the freed block is not probed (GlobalFlags on it corrupts the heap on Win10) */
}
TEST(ddelparam_reuse_packed_to_plain) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(ReuseDDElParam)); REQUIRE(U(UnpackDDElParam));
    LPARAM lp = U(PackDDElParam)(WM_DDE_DATA, 1, 2);
    LPARAM r = U(ReuseDDElParam)(lp, WM_DDE_DATA, WM_DDE_REQUEST, 0x1234, 0x5678);
    EQ(r, 0x56781234);
    /* the old block is freed by the reuse; it is not touched again (a
     * GlobalFlags probe of the freed handle was undefined behaviour and the
     * P0 run died here with heap corruption 0xc0000374) */
    lp = U(PackDDElParam)(WM_DDE_POKE, 1, 2);
    r = U(ReuseDDElParam)(lp, WM_DDE_POKE, WM_DDE_TERMINATE, 0xFFFF, 0xFFFF);
    EQ(r, (LPARAM)(LONG_PTR)(INT)0xFFFFFFFF);
    /* uiHi is truncated to 16 bits in this direction */
    lp = U(PackDDElParam)(WM_DDE_POKE, 1, 2);
    r = U(ReuseDDElParam)(lp, WM_DDE_POKE, WM_DDE_UNADVISE, 0x1234, 0x15678);
    EQ(r, 0x56781234);
}
TEST(ddelparam_reuse_from_plain) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(ReuseDDElParam)); REQUIRE(U(UnpackDDElParam)); REQUIRE(U(FreeDDElParam));
    /* a non-packed input has nothing to reuse: behaves like PackDDElParam */
    LPARAM r = U(ReuseDDElParam)(0x00020001, WM_DDE_REQUEST, WM_DDE_ACK, 7, 8);
    CHECK((ULONG_PTR)r > 0xFFFF);
    UINT_PTR lo = 0, hi = 0;
    CHECK(U(UnpackDDElParam)(WM_DDE_ACK, r, &lo, &hi));
    EQ(lo, 7); EQ(hi, 8);
    U(FreeDDElParam)(WM_DDE_ACK, r);
    r = U(ReuseDDElParam)(0x1234, WM_DDE_INITIATE, WM_DDE_TERMINATE, 0x11, 0x22);
    EQ(r, 0x220011);
    r = U(ReuseDDElParam)(0x1234, WM_DDE_REQUEST, WM_DDE_EXECUTE, 0x11, 0x99);
    EQ(r, 0x99);
}
TEST(ddelparam_reuse_zero_lparam) {
    REQUIRE(U(ReuseDDElParam));
    EQ(U(ReuseDDElParam)(0, WM_DDE_ACK, WM_DDE_DATA, 1, 2), 0);
}
TEST(ddelparam_stress_pack_free) {
    REQUIRE(U(PackDDElParam)); REQUIRE(U(FreeDDElParam)); REQUIRE(U(UnpackDDElParam));
    for (int i = 0; i < 20000; i++) {
        LPARAM lp = U(PackDDElParam)(WM_DDE_DATA, i, i * 3);
        UINT_PTR lo, hi;
        if (!lp || !U(UnpackDDElParam)(WM_DDE_DATA, lp, &lo, &hi) || lo != (UINT_PTR)i || hi != (UINT_PTR)i * 3) { CHECKF(0, "iteration %d", i); break; }
        U(FreeDDElParam)(WM_DDE_DATA, lp);
    }
}

/* ---------- message flow between two windows of this thread ---------- */
#define LOGMAX 128
typedef struct { int who; UINT msg; WPARAM wp; LPARAM lp; UINT_PTR a, b; char s1[64], s2[64]; } MLOG;
static MLOG g_log[LOGMAX];
static int g_nlog;
static HWND g_cli, g_srv;
static ATOM g_aSvc, g_aTop, g_aItem;
static int g_srv_accept = 1;
static int g_srv_nak;
static HGLOBAL g_srv_poked, g_exec_h;

static const char *CLS_C = "T32DdeMsgClient", *CLS_S = "T32DdeMsgServer";

static void atom_name(ATOM a, char *out, size_t n) {
    out[0] = 0;
    if (a) GlobalGetAtomNameA(a, out, (int)n);
}
static MLOG *mlog(int who, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_nlog >= LOGMAX) return &g_log[LOGMAX - 1];
    MLOG *m = &g_log[g_nlog++];
    memset(m, 0, sizeof *m);
    m->who = who; m->msg = msg; m->wp = wp; m->lp = lp;
    return m;
}
static int mcount(int who, UINT msg) {
    int n = 0;
    for (int i = 0; i < g_nlog; i++) if (g_log[i].who == who && g_log[i].msg == msg) n++;
    return n;
}
static MLOG *mfind(int who, UINT msg, int nth) {
    for (int i = 0; i < g_nlog; i++) if (g_log[i].who == who && g_log[i].msg == msg && nth-- == 0) return &g_log[i];
    return NULL;
}
static int mwait(int who, UINT msg, int n, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    while (mcount(who, msg) < n && (LONG)(GetTickCount() - end) < 0) dh_sleep_pump(5);
    return mcount(who, msg) >= n;
}

static LRESULT CALLBACK srv_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m < WM_DDE_FIRST || m > WM_DDE_LAST) return DefWindowProcA(h, m, w, l);
    MLOG *e = mlog(0, m, w, l);
    switch (m) {
    case WM_DDE_INITIATE: {
        ATOM aApp = LOWORD(l), aTop = HIWORD(l);
        atom_name(aApp, e->s1, sizeof e->s1); atom_name(aTop, e->s2, sizeof e->s2);
        if (g_srv_accept && (aApp == 0 || aApp == g_aSvc) && (aTop == 0 || aTop == g_aTop)) {
            ATOM a1 = GlobalAddAtomA("T32DdeMsgSvc"), a2 = GlobalAddAtomA("T32DdeMsgTop");
            SendMessageA((HWND)w, WM_DDE_ACK, (WPARAM)h, MAKELPARAM(a1, a2));
            GlobalDeleteAtom(a1); GlobalDeleteAtom(a2);
        }
        return 0;
    }
    case WM_DDE_REQUEST: {
        UINT_PTR fmt, item;
        e->a = 0; e->b = 0;
        if (U(UnpackDDElParam)(m, l, &fmt, &item)) { e->a = fmt; e->b = item; atom_name((ATOM)item, e->s1, sizeof e->s1); }
        if (g_srv_nak) {
            PostMessageA((HWND)w, WM_DDE_ACK, (WPARAM)h, U(PackDDElParam)(WM_DDE_ACK, 0, item));
        } else {
            HGLOBAL hd = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(DDEDATA) + 8);
            DDEDATA *d = GlobalLock(hd);
            d->unused = 0; d->fResponse = 1; d->fRelease = 1; d->fAckReq = 1; d->cfFormat = (SHORT)fmt;
            memcpy(d->Value, "datum01", 8);
            GlobalUnlock(hd);
            ATOM ai = GlobalAddAtomA(e->s1);
            PostMessageA((HWND)w, WM_DDE_DATA, (WPARAM)h, U(PackDDElParam)(WM_DDE_DATA, (UINT_PTR)hd, ai));
        }
        U(FreeDDElParam)(m, l);
        return 0;
    }
    case WM_DDE_POKE: {
        UINT_PTR hd, item;
        if (U(UnpackDDElParam)(m, l, &hd, &item)) {
            e->a = hd; e->b = item; atom_name((ATOM)item, e->s1, sizeof e->s1);
            DDEPOKE *p = GlobalLock((HGLOBAL)hd);
            if (p) { memcpy(e->s2, p->Value, 8); e->s2[8] = 0; GlobalUnlock((HGLOBAL)hd); }
            BOOL rel = FALSE;
            p = GlobalLock((HGLOBAL)hd); if (p) { rel = p->fRelease; GlobalUnlock((HGLOBAL)hd); }
            LPARAM r = U(ReuseDDElParam)(l, m, WM_DDE_ACK, g_srv_nak ? 0 : DDE_FACK, item);
            PostMessageA((HWND)w, WM_DDE_ACK, (WPARAM)h, r);
            if (rel) GlobalFree((HGLOBAL)hd);
            g_srv_poked = (HGLOBAL)hd;
        }
        return 0;
    }
    case WM_DDE_EXECUTE: {
        e->a = 0; e->b = l;
        char *s = GlobalLock((HGLOBAL)l);
        if (s) { lstrcpynA(e->s1, s, sizeof e->s1); GlobalUnlock((HGLOBAL)l); }
        LPARAM r = U(ReuseDDElParam)(l, m, WM_DDE_ACK, g_srv_nak ? 0 : DDE_FACK, l);
        PostMessageA((HWND)w, WM_DDE_ACK, (WPARAM)h, r);
        return 0;
    }
    case WM_DDE_ADVISE: {
        UINT_PTR hopt, item;
        if (U(UnpackDDElParam)(m, l, &hopt, &item)) {
            e->a = hopt; e->b = item; atom_name((ATOM)item, e->s1, sizeof e->s1);
            DDEADVISE *ad = GlobalLock((HGLOBAL)hopt);
            if (ad) { e->s2[0] = (char)(ad->cfFormat); e->s2[1] = ad->fDeferUpd ? 'D' : 'H'; e->s2[2] = ad->fAckReq ? 'A' : 'N'; GlobalUnlock((HGLOBAL)hopt); }
            PostMessageA((HWND)w, WM_DDE_ACK, (WPARAM)h, U(ReuseDDElParam)(l, m, WM_DDE_ACK, DDE_FACK, item));
            GlobalFree((HGLOBAL)hopt);
        }
        return 0;
    }
    case WM_DDE_UNADVISE: {
        UINT_PTR fmt, item;
        if (U(UnpackDDElParam)(m, l, &fmt, &item)) {
            e->a = fmt; e->b = item; atom_name((ATOM)item, e->s1, sizeof e->s1);
            PostMessageA((HWND)w, WM_DDE_ACK, (WPARAM)h, U(PackDDElParam)(WM_DDE_ACK, DDE_FACK, item));
        }
        return 0;
    }
    case WM_DDE_ACK: {
        UINT_PTR st, extra;
        if (U(UnpackDDElParam)(m, l, &st, &extra)) { e->a = st; e->b = extra; }
        U(FreeDDElParam)(m, l);
        return 0;
    }
    case WM_DDE_TERMINATE:
        PostMessageA((HWND)w, WM_DDE_TERMINATE, (WPARAM)h, 0);
        return 0;
    }
    return 0;
}
static LRESULT CALLBACK cli_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m < WM_DDE_FIRST || m > WM_DDE_LAST) return DefWindowProcA(h, m, w, l);
    MLOG *e = mlog(1, m, w, l);
    switch (m) {
    case WM_DDE_ACK: {
        if (mcount(1, WM_DDE_ACK) == 1 && mcount(1, WM_DDE_DATA) == 0 && !g_srv_poked && !g_exec_h && !mcount(0, WM_DDE_REQUEST)
            && mcount(0, WM_DDE_INITIATE) && !mcount(0, WM_DDE_POKE) && !mcount(0, WM_DDE_EXECUTE) && !mcount(0, WM_DDE_ADVISE) && !mcount(0, WM_DDE_UNADVISE)) {
            /* handshake reply: lParam carries two global atoms */
            atom_name(LOWORD(l), e->s1, sizeof e->s1); atom_name(HIWORD(l), e->s2, sizeof e->s2);
            GlobalDeleteAtom(LOWORD(l)); GlobalDeleteAtom(HIWORD(l));
            e->a = LOWORD(l); e->b = HIWORD(l);
            return 0;
        }
        UINT_PTR st, item;
        if (U(UnpackDDElParam)(m, l, &st, &item)) { e->a = st; e->b = item; atom_name((ATOM)item, e->s1, sizeof e->s1); }
        U(FreeDDElParam)(m, l);
        return 0;
    }
    case WM_DDE_DATA: {
        UINT_PTR hd, item;
        if (U(UnpackDDElParam)(m, l, &hd, &item)) {
            e->a = hd; e->b = item; atom_name((ATOM)item, e->s1, sizeof e->s1);
            DDEDATA *d = hd ? GlobalLock((HGLOBAL)hd) : NULL;
            BOOL ackreq = FALSE, rel = FALSE;
            if (d) { memcpy(e->s2, d->Value, 8); e->s2[8] = 0; ackreq = d->fAckReq; rel = d->fRelease; e->wp = w; GlobalUnlock((HGLOBAL)hd); }
            if (ackreq) PostMessageA((HWND)w, WM_DDE_ACK, (WPARAM)h, U(ReuseDDElParam)(l, m, WM_DDE_ACK, DDE_FACK, item));
            else U(FreeDDElParam)(m, l);
            if (rel && hd) GlobalFree((HGLOBAL)hd);
        }
        return 0;
    }
    case WM_DDE_TERMINATE:
        return 0;
    }
    return 0;
}

static int flow_begin(void) {
    g_nlog = 0; g_srv_accept = 1; g_srv_nak = 0; g_srv_poked = NULL; g_exec_h = NULL;
    g_cli = dh_create_window(CLS_C, cli_proc, WS_OVERLAPPED, 0, NULL);
    g_srv = dh_create_window(CLS_S, srv_proc, WS_OVERLAPPED, 0, NULL);
    g_aSvc = GlobalAddAtomA("T32DdeMsgSvc");
    g_aTop = GlobalAddAtomA("T32DdeMsgTop");
    g_aItem = GlobalAddAtomA("T32DdeMsgItem");
    return g_cli && g_srv && g_aSvc && g_aTop && g_aItem;
}
static void flow_end(void) {
    dh_sleep_pump(20);
    dh_destroy_window(g_cli, CLS_C); dh_destroy_window(g_srv, CLS_S);
    if (g_aSvc) GlobalDeleteAtom(g_aSvc);
    if (g_aTop) GlobalDeleteAtom(g_aTop);
    if (g_aItem) GlobalDeleteAtom(g_aItem);
    g_cli = g_srv = NULL; g_aSvc = g_aTop = g_aItem = 0;
}
static int flow_need(void) {
    return U(PackDDElParam) && U(UnpackDDElParam) && U(FreeDDElParam) && U(ReuseDDElParam) && U(PostMessageA) && U(SendMessageA)
           && U(RegisterClassA) && U(CreateWindowExA) && U(DestroyWindow) && U(UnregisterClassA);
}
static void handshake(void) {
    SendMessageA(g_srv, WM_DDE_INITIATE, (WPARAM)g_cli, MAKELPARAM(g_aSvc, g_aTop));
}
#define FLOW_BEGIN() do { if (!flow_need()) { t_skip("missing exports"); return; } \
    if (!flow_begin()) { CHECKF(0, "flow setup"); flow_end(); return; } } while (0)

TEST(ddemsg_initiate_ack_handshake) {
    FLOW_BEGIN();
    handshake();
    CHECK(mcount(0, WM_DDE_INITIATE) == 1);
    MLOG *s = mfind(0, WM_DDE_INITIATE, 0);
    if (s) { EQ(s->wp, (WPARAM)g_cli); EXPECT_STR(s->s1, "T32DdeMsgSvc"); EXPECT_STR(s->s2, "T32DdeMsgTop"); }
    CHECK(mcount(1, WM_DDE_ACK) == 1);
    MLOG *c = mfind(1, WM_DDE_ACK, 0);
    if (c) { EQ(c->wp, (WPARAM)g_srv); EXPECT_STR(c->s1, "T32DdeMsgSvc"); EXPECT_STR(c->s2, "T32DdeMsgTop"); }
    flow_end();
}
TEST(ddemsg_initiate_wildcard_atoms) {
    FLOW_BEGIN();
    SendMessageA(g_srv, WM_DDE_INITIATE, (WPARAM)g_cli, MAKELPARAM(0, 0));
    CHECK(mcount(0, WM_DDE_INITIATE) == 1);
    CHECK(mcount(1, WM_DDE_ACK) == 1);
    MLOG *s = mfind(0, WM_DDE_INITIATE, 0);
    if (s) { EQ(s->s1[0], 0); EQ(s->s2[0], 0); }
    flow_end();
}
TEST(ddemsg_initiate_no_matching_topic) {
    FLOW_BEGIN();
    ATOM other = GlobalAddAtomA("T32DdeMsgOther");
    SendMessageA(g_srv, WM_DDE_INITIATE, (WPARAM)g_cli, MAKELPARAM(g_aSvc, other));
    CHECK(mcount(0, WM_DDE_INITIATE) == 1);
    EQ(mcount(1, WM_DDE_ACK), 0);
    GlobalDeleteAtom(other);
    flow_end();
}
TEST(ddemsg_request_data_ack) {
    FLOW_BEGIN();
    handshake();
    BOOL p = PostMessageA(g_srv, WM_DDE_REQUEST, (WPARAM)g_cli, U(PackDDElParam)(WM_DDE_REQUEST, CF_TEXT, g_aItem));
    CHECK(p);
    CHECK(mwait(1, WM_DDE_DATA, 1, 2000));
    MLOG *s = mfind(0, WM_DDE_REQUEST, 0);
    if (s) { EQ(s->wp, (WPARAM)g_cli); EQ(s->a, CF_TEXT); EQ(s->b, g_aItem); EXPECT_STR(s->s1, "T32DdeMsgItem"); }
    MLOG *c = mfind(1, WM_DDE_DATA, 0);
    if (c) { EQ(c->wp, (WPARAM)g_srv); EXPECT_STR(c->s1, "T32DdeMsgItem"); EXPECT_STR(c->s2, "datum01"); }
    CHECK(mwait(0, WM_DDE_ACK, 1, 2000));
    s = mfind(0, WM_DDE_ACK, 0);
    if (s) { EQ(s->a, DDE_FACK); }
    flow_end();
}
TEST(ddemsg_request_nak) {
    FLOW_BEGIN();
    handshake();
    g_srv_nak = 1;
    PostMessageA(g_srv, WM_DDE_REQUEST, (WPARAM)g_cli, U(PackDDElParam)(WM_DDE_REQUEST, CF_TEXT, g_aItem));
    CHECK(mwait(1, WM_DDE_ACK, 2, 2000));
    MLOG *c = mfind(1, WM_DDE_ACK, 1);
    if (c) { EQ(c->a & DDE_FACK, 0); EQ(c->b, g_aItem); }
    EQ(mcount(1, WM_DDE_DATA), 0);
    flow_end();
}
TEST(ddemsg_poke_ack) {
    FLOW_BEGIN();
    handshake();
    HGLOBAL hp = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(DDEPOKE) + 8);
    DDEPOKE *p = GlobalLock(hp);
    p->unused = 0; p->fRelease = 1; p->cfFormat = CF_TEXT; memcpy(p->Value, "pokeval", 8);
    GlobalUnlock(hp);
    BOOL ok = PostMessageA(g_srv, WM_DDE_POKE, (WPARAM)g_cli, U(PackDDElParam)(WM_DDE_POKE, (UINT_PTR)hp, g_aItem));
    CHECK(ok);
    CHECK(mwait(1, WM_DDE_ACK, 2, 2000));
    MLOG *s = mfind(0, WM_DDE_POKE, 0);
    if (s) { t_note("poke handle posted %p received %p", hp, (void *)s->a); EQ(s->b, g_aItem); EXPECT_STR(s->s1, "T32DdeMsgItem"); EXPECT_STR(s->s2, "pokeval"); }
    MLOG *c = mfind(1, WM_DDE_ACK, 1);
    if (c) { EQ(c->a, DDE_FACK); EQ(c->b, g_aItem); }
    t_note("poke block after server release: flags 0x%x", GlobalFlags(hp));
    flow_end();
}
TEST(ddemsg_execute_ack) {
    FLOW_BEGIN();
    handshake();
    static const char cmd[] = "[Exec(1,2)]";
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof cmd);
    char *p = GlobalLock(h); memcpy(p, cmd, sizeof cmd); GlobalUnlock(h);
    g_exec_h = h;
    BOOL ok = PostMessageA(g_srv, WM_DDE_EXECUTE, (WPARAM)g_cli, (LPARAM)h);
    CHECK(ok);
    CHECK(mwait(1, WM_DDE_ACK, 2, 2000));
    MLOG *s = mfind(0, WM_DDE_EXECUTE, 0);
    if (s) { EQ(s->wp, (WPARAM)g_cli); EXPECT_STR(s->s1, cmd); }
    MLOG *c = mfind(1, WM_DDE_ACK, 1);
    if (c) { EQ(c->a, DDE_FACK); EQ(c->b, (UINT_PTR)h); }
    GlobalFree(h);
    flow_end();
}
TEST(ddemsg_advise_unadvise) {
    FLOW_BEGIN();
    handshake();
    HGLOBAL ho = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(DDEADVISE));
    DDEADVISE *a = GlobalLock(ho);
    a->reserved = 0; a->fDeferUpd = 0; a->fAckReq = 1; a->cfFormat = CF_TEXT;
    GlobalUnlock(ho);
    BOOL ok = PostMessageA(g_srv, WM_DDE_ADVISE, (WPARAM)g_cli, U(PackDDElParam)(WM_DDE_ADVISE, (UINT_PTR)ho, g_aItem));
    CHECK(ok);
    CHECK(mwait(1, WM_DDE_ACK, 2, 2000));
    MLOG *s = mfind(0, WM_DDE_ADVISE, 0);
    if (s) { EQ(s->b, g_aItem); EQ((unsigned char)s->s2[0], CF_TEXT); EQ(s->s2[1], 'H'); EQ(s->s2[2], 'A'); EXPECT_STR(s->s1, "T32DdeMsgItem"); }
    MLOG *c = mfind(1, WM_DDE_ACK, 1);
    if (c) EQ(c->a, DDE_FACK);
    /* now unadvise */
    ok = PostMessageA(g_srv, WM_DDE_UNADVISE, (WPARAM)g_cli, U(PackDDElParam)(WM_DDE_UNADVISE, CF_TEXT, g_aItem));
    CHECK(ok);
    CHECK(mwait(1, WM_DDE_ACK, 3, 2000));
    s = mfind(0, WM_DDE_UNADVISE, 0);
    if (s) { EQ(s->a, CF_TEXT); EQ(s->b, g_aItem); }
    c = mfind(1, WM_DDE_ACK, 2);
    if (c) { EQ(c->a, DDE_FACK); EQ(c->b, g_aItem); }
    flow_end();
}
TEST(ddemsg_terminate_exchange) {
    FLOW_BEGIN();
    handshake();
    BOOL ok = PostMessageA(g_srv, WM_DDE_TERMINATE, (WPARAM)g_cli, 0);
    CHECK(ok);
    CHECK(mwait(1, WM_DDE_TERMINATE, 1, 2000));
    MLOG *s = mfind(0, WM_DDE_TERMINATE, 0);
    if (s) EQ(s->wp, (WPARAM)g_cli);
    MLOG *c = mfind(1, WM_DDE_TERMINATE, 0);
    if (c) EQ(c->wp, (WPARAM)g_srv);
    flow_end();
}
TEST(ddemsg_post_to_invalid_window) {
    REQUIRE(U(PostMessageA));
    SetLastError(0);
    /* Win10 22H2 oracle: PostMessage of a WM_DDE_* message to an invalid
     * window reports success (TRUE); recorded, not asserted. */
    BOOL pr = PostMessageA((HWND)0x12345678, WM_DDE_TERMINATE, 0, 0);
    t_note("PostMessage(WM_DDE_TERMINATE) to an invalid window -> %d err=%lu", pr, GetLastError());
}
TEST(ddemsg_post_after_destroy) {
    FLOW_BEGIN();
    handshake();
    HWND dead = g_srv;
    U(DestroyWindow)(g_srv);
    g_srv = NULL;
    SetLastError(0);
    BOOL pr = PostMessageA(dead, WM_DDE_TERMINATE, (WPARAM)g_cli, 0);   /* Win10 22H2 oracle: TRUE, as for an invalid handle */
    t_note("PostMessage(WM_DDE_TERMINATE) to a destroyed window -> %d err=%lu", pr, GetLastError());
    flow_end();
}
TEST(ddemsg_impersonate_window_errors) {
    UDECL(BOOL, ImpersonateDdeClientWindow, (HWND, HWND));
    REQUIRE(UN(ImpersonateDdeClientWindow));
    FLOW_BEGIN();
    SetLastError(0);
    EQ(UN(ImpersonateDdeClientWindow)(g_cli, NULL), FALSE);
    SetLastError(0);
    EQ(UN(ImpersonateDdeClientWindow)(NULL, g_srv), FALSE);
    SetLastError(0);
    BOOL r = UN(ImpersonateDdeClientWindow)(g_cli, g_srv);
    t_note("ImpersonateDdeClientWindow without a conversation -> %d (err %lu)", r, GetLastError());
    /* Win10 22H2 oracle: succeeds (TRUE, error 0) even without a
     * conversation; recorded, not asserted. Never stay impersonating. */
    if (r) RevertToSelf();
    handshake();
    SetLastError(0);
    r = UN(ImpersonateDdeClientWindow)(g_cli, g_srv);
    t_note("ImpersonateDdeClientWindow after handshake -> %d (err %lu)", r, GetLastError());
    if (r) RevertToSelf();
    flow_end();
}
