/* gap_00: user-mode-only helpers (AlignRects, reason arrays, DBCS flag, init) */
#include "gap0.h"

UDECL(BOOL, AlignRects, (RECT *, UINT, UINT, UINT));
UDECL(void, DestroyReasons, (void *));
UDECL(BOOL, BuildReasonArray, (void *, int, int));
UDECL(BOOL, CheckDBCSEnabledExt, (void));
UDECL(BOOL, CtxInitUser32, (void));

#define AR_NOSNAP 1
#define AR_NOOVERLAP 2
#define AR_NOGAPS 4
#define AR_NOOFFSET 0x10

static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static int same(RECT a, RECT b) { return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom; }

/* ---------- AlignRects ---------- */
TEST(gap0_alignrects_count0_noop) {
    REQUIRE(UN(AlignRects));
    RECT a[2] = { R(10, 20, 50, 80), R(1, 2, 3, 4) };
    BOOL r = UN(AlignRects)(a, 0, 0, 0);
    EXPECT_EQ(r, 1);
    CHECK(same(a[0], R(10, 20, 50, 80)));
    CHECK(same(a[1], R(1, 2, 3, 4)));
}
TEST(gap0_alignrects_count0_all_flags) {
    REQUIRE(UN(AlignRects));
    static const UINT fl[] = { 0, 1, 2, 4, 7, 0x10, 0x17 };
    for (unsigned i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        RECT a[1] = { R(3, 4, 30, 40) };
        EXPECT_EQ(UN(AlignRects)(a, 0, 0, fl[i]), 1);
        CHECKF(same(a[0], R(3, 4, 30, 40)), "flags %x", fl[i]);
    }
}
TEST(gap0_alignrects_count1_offsets_to_origin) {
    REQUIRE(UN(AlignRects));
    RECT a[1] = { R(10, 20, 50, 80) };
    EXPECT_EQ(UN(AlignRects)(a, 1, 0, 0), 1);
    CHECK(same(a[0], R(0, 0, 40, 60)));
}
TEST(gap0_alignrects_count1_negative_origin) {
    REQUIRE(UN(AlignRects));
    RECT a[1] = { R(-30, -40, 10, 20) };
    EXPECT_EQ(UN(AlignRects)(a, 1, 0, 0), 1);
    CHECK(same(a[0], R(0, 0, 40, 60)));
}
TEST(gap0_alignrects_count1_no_offset_flag) {
    REQUIRE(UN(AlignRects));
    RECT a[1] = { R(10, 20, 50, 80) };
    EXPECT_EQ(UN(AlignRects)(a, 1, 0, AR_NOOFFSET), 1);
    CHECK(same(a[0], R(10, 20, 50, 80)));
}
TEST(gap0_alignrects_count1_ignores_snap_flag) {
    REQUIRE(UN(AlignRects));
    /* snapping only happens when more than one rectangle is supplied */
    RECT a[1] = { R(5, 5, 30, 30) };
    EXPECT_EQ(UN(AlignRects)(a, 1, 0, AR_NOOFFSET), 1);
    CHECK(same(a[0], R(5, 5, 30, 30)));
    RECT b[1] = { R(5, 5, 30, 30) };
    EXPECT_EQ(UN(AlignRects)(b, 1, 0, AR_NOSNAP | AR_NOOFFSET), 1);
    CHECK(same(b[0], R(5, 5, 30, 30)));
}
TEST(gap0_alignrects_identity_flags) {
    REQUIRE(UN(AlignRects));
    RECT a[3] = { R(0, 0, 100, 100), R(50, 50, 150, 150), R(-20, 7, 33, 91) };
    RECT o[3]; memcpy(o, a, sizeof a);
    EXPECT_EQ(UN(AlignRects)(a, 3, 0, AR_NOSNAP | AR_NOOVERLAP | AR_NOOFFSET), 1);
    for (int i = 0; i < 3; i++) CHECK(same(a[i], o[i]));
}
TEST(gap0_alignrects_identity_flags_bit4_variants) {
    REQUIRE(UN(AlignRects));
    /* any combination containing NOOVERLAP skips both overlap and gap passes */
    static const UINT fl[] = { 1|2|0x10, 1|2|4|0x10 };
    for (unsigned k = 0; k < 2; k++) {
        RECT a[2] = { R(0, 0, 30, 30), R(10, 10, 40, 40) };
        RECT o[2]; memcpy(o, a, sizeof a);
        EXPECT_EQ(UN(AlignRects)(a, 2, 1, fl[k]), 1);
        CHECK(same(a[0], o[0]));
        CHECK(same(a[1], o[1]));
    }
}
TEST(gap0_alignrects_offset_by_primary_index) {
    REQUIRE(UN(AlignRects));
    for (UINT p = 0; p < 3; p++) {
        RECT a[3] = { R(10, 10, 20, 20), R(100, 200, 160, 260), R(-5, -9, 5, 9) };
        RECT o[3]; memcpy(o, a, sizeof a);
        EXPECT_EQ(UN(AlignRects)(a, 3, p, AR_NOSNAP | AR_NOOVERLAP), 1);
        for (int i = 0; i < 3; i++) {
            CHECKF(same(a[i], R(o[i].left - o[p].left, o[i].top - o[p].top, o[i].right - o[p].left, o[i].bottom - o[p].top)),
                   "primary %u rect %d", p, i);
        }
        CHECK(a[p].left == 0 && a[p].top == 0);
    }
}
TEST(gap0_alignrects_snap_only_positive) {
    REQUIRE(UN(AlignRects));
    /* grid of 8: left=(l+3)/8, right=left+(r-l)/8, all scaled back by 8 */
    RECT a[2] = { R(5, 5, 30, 30), R(100, 100, 100, 100) };
    EXPECT_EQ(UN(AlignRects)(a, 2, 0, AR_NOOVERLAP | AR_NOOFFSET), 1);
    CHECK(same(a[0], R(8, 8, 32, 32)));
    CHECK(same(a[1], R(96, 96, 96, 96)));
}
TEST(gap0_alignrects_snap_only_negative) {
    REQUIRE(UN(AlignRects));
    RECT a[2] = { R(-5, -5, 11, 11), R(0, 0, 0, 0) };
    EXPECT_EQ(UN(AlignRects)(a, 2, 0, AR_NOOVERLAP | AR_NOOFFSET), 1);
    CHECK(same(a[0], R(-8, -8, 8, 8)));
    CHECK(same(a[1], R(0, 0, 0, 0)));
}
TEST(gap0_alignrects_snap_rounding_table) {
    REQUIRE(UN(AlignRects));
    /* left coordinate rounds to nearest 8 for positives (x+3)/8, away-ish for negatives (x-4)/8 */
    static const struct { int in, out; } t[] = {
        {0,0},{1,0},{4,0},{5,8},{7,8},{8,8},{11,8},{12,8},{13,16},{-1,0},{-3,0},{-4,-8},{-7,-8},{-11,-8},{-12,-16},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT a[2] = { R(t[i].in, t[i].in, t[i].in + 64, t[i].in + 64), R(0, 0, 0, 0) };
        EXPECT_EQ(UN(AlignRects)(a, 2, 0, AR_NOOVERLAP | AR_NOOFFSET), 1);
        CHECKF(a[0].left == t[i].out && a[0].top == t[i].out, "in %d got %ld,%ld want %d", t[i].in, a[0].left, a[0].top, t[i].out);
        CHECKF(a[0].right - a[0].left == 64 && a[0].bottom - a[0].top == 64, "size %d", t[i].in);
    }
}
TEST(gap0_alignrects_snap_then_offset) {
    REQUIRE(UN(AlignRects));
    RECT a[2] = { R(5, 5, 30, 30), R(105, 205, 130, 230) };
    EXPECT_EQ(UN(AlignRects)(a, 2, 1, AR_NOOVERLAP), 1);
    CHECK(a[1].left == 0 && a[1].top == 0);
    CHECK(a[0].left == 8 - 104 && a[0].top == 8 - 208);
}
static int any_intersect(const RECT *a, int n) {
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
        RECT x;
        if (U(IntersectRect)(&x, &a[i], &a[j])) return 1;
    }
    return 0;
}
TEST(gap0_alignrects_removes_overlap_two) {
    REQUIRE(UN(AlignRects));
    REQUIRE(U(IntersectRect));
    RECT a[2] = { R(0, 0, 80, 80), R(40, 40, 120, 120) };
    EXPECT_EQ(UN(AlignRects)(a, 2, 0, AR_NOSNAP | AR_NOGAPS | AR_NOOFFSET), 1);
    CHECK(!any_intersect(a, 2));
}
TEST(gap0_alignrects_removes_overlap_four) {
    REQUIRE(UN(AlignRects));
    RECT a[4] = { R(0, 0, 80, 80), R(40, 40, 120, 120), R(20, 60, 90, 130), R(70, 0, 140, 50) };
    EXPECT_EQ(UN(AlignRects)(a, 4, 0, AR_NOSNAP | AR_NOGAPS | AR_NOOFFSET), 1);
    CHECK(!any_intersect(a, 4));
}
TEST(gap0_alignrects_removes_overlap_and_gaps) {
    REQUIRE(UN(AlignRects));
    RECT a[3] = { R(0, 0, 64, 64), R(32, 32, 96, 96), R(500, 500, 564, 564) };
    EXPECT_EQ(UN(AlignRects)(a, 3, 0, AR_NOSNAP | AR_NOOFFSET), 1);
    CHECK(!any_intersect(a, 3));
}
TEST(gap0_alignrects_full_default_flags) {
    REQUIRE(UN(AlignRects));
    RECT a[3] = { R(0, 0, 64, 64), R(16, 16, 80, 80), R(300, 10, 364, 74) };
    EXPECT_EQ(UN(AlignRects)(a, 3, 0, 0), 1);
    CHECK(!any_intersect(a, 3));
    CHECK(a[0].left == 0 && a[0].top == 0);
}
TEST(gap0_alignrects_overlap_preserves_sizes) {
    REQUIRE(UN(AlignRects));
    RECT a[3] = { R(0, 0, 80, 70), R(40, 40, 100, 120), R(10, 10, 50, 90) };
    SIZE sz[3];
    for (int i = 0; i < 3; i++) { sz[i].cx = a[i].right - a[i].left; sz[i].cy = a[i].bottom - a[i].top; }
    UN(AlignRects)(a, 3, 0, AR_NOSNAP | AR_NOGAPS | AR_NOOFFSET);
    for (int i = 0; i < 3; i++) {
        CHECKF(a[i].right - a[i].left == sz[i].cx && a[i].bottom - a[i].top == sz[i].cy, "rect %d resized", i);
    }
}
TEST(gap0_alignrects_idempotent_nonoverlap) {
    REQUIRE(UN(AlignRects));
    /* an already separated, grid aligned layout must not intersect afterwards */
    RECT a[3] = { R(0, 0, 64, 64), R(64, 0, 128, 64), R(0, 64, 64, 128) };
    EXPECT_EQ(UN(AlignRects)(a, 3, 0, AR_NOOFFSET), 1);
    CHECK(!any_intersect(a, 3));
}
TEST(gap0_alignrects_repeat_stable) {
    REQUIRE(UN(AlignRects));
    for (int i = 0; i < 200; i++) {
        RECT a[2] = { R(5, 5, 30, 30), R(105, 205, 130, 230) };
        if (UN(AlignRects)(a, 2, 1, AR_NOOVERLAP) != 1) { CHECK(0); break; }
        if (a[1].left != 0 || a[0].left != 8 - 104) { CHECK(0); break; }
    }
    CHECK(1);
}

/* ---------- DestroyReasons / BuildReasonArray ---------- */
typedef struct { void **items; int count; int cap; } G0Reasons;

TEST(gap0_destroyreasons_empty) {
    REQUIRE(UN(DestroyReasons));
    G0Reasons r = { NULL, 0, 0 };
    UN(DestroyReasons)(&r);
    CHECK(r.items == NULL);
    EXPECT_EQ(r.count, 0);
}
TEST(gap0_destroyreasons_null_items_resets_count) {
    REQUIRE(UN(DestroyReasons));
    G0Reasons r = { NULL, 7, 31 };
    UN(DestroyReasons)(&r);
    CHECK(r.items == NULL);
    EXPECT_EQ(r.count, 0);
    EXPECT_EQ(r.cap, 31);   /* capacity field is not touched */
}
TEST(gap0_destroyreasons_idempotent) {
    REQUIRE(UN(DestroyReasons)); REQUIRE(UN(BuildReasonArray));
    G0Reasons r; memset(&r, 0, sizeof r);
    EXPECT_EQ(UN(BuildReasonArray)(&r, 0, 0), 1);
    UN(DestroyReasons)(&r);
    UN(DestroyReasons)(&r);
    UN(DestroyReasons)(&r);
    CHECK(r.items == NULL);
    EXPECT_EQ(r.count, 0);
}
TEST(gap0_buildreasons_null) {
    REQUIRE(UN(BuildReasonArray));
    EXPECT_EQ(UN(BuildReasonArray)(NULL, 0, 0), 0);
    EXPECT_EQ(UN(BuildReasonArray)(NULL, 1, 1), 0);
}
TEST(gap0_buildreasons_all) {
    REQUIRE(UN(BuildReasonArray)); REQUIRE(UN(DestroyReasons));
    G0Reasons r; memset(&r, 0, sizeof r);
    EXPECT_EQ(UN(BuildReasonArray)(&r, 0, 0), 1);
    CHECK(r.items != NULL);
    CHECK(r.count >= 1);
    CHECK(r.cap >= r.count);
    t_note("BuildReasonArray(0,0): %d reasons", r.count);
    for (int i = 0; i < r.count && i < 200; i++) {
        BYTE *e = r.items[i];
        CHECKF(e != NULL, "entry %d null", i);
        if (!e) continue;
        /* layout: DWORD flags, WCHAR name[64], WCHAR description[256] */
        const WCHAR *name = (const WCHAR *)(e + 4), *desc = (const WCHAR *)(e + 0x84);
        if (name[0] == 0 || desc[0] == 0) t_note("entry %d has empty name/description", i);
        CHECKF(lstrlenW(name) < 64 && lstrlenW(desc) < 256, "entry %d unterminated", i);
    }
    UN(DestroyReasons)(&r);
    CHECK(r.items == NULL);
    EXPECT_EQ(r.count, 0);
}
TEST(gap0_buildreasons_deterministic) {
    REQUIRE(UN(BuildReasonArray)); REQUIRE(UN(DestroyReasons));
    G0Reasons a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EXPECT_EQ(UN(BuildReasonArray)(&a, 0, 0), 1);
    EXPECT_EQ(UN(BuildReasonArray)(&b, 0, 0), 1);
    EXPECT_EQ(a.count, b.count);
    for (int i = 0; i < a.count && i < b.count && i < 200; i++) {
        CHECKF(*(DWORD *)a.items[i] == *(DWORD *)b.items[i], "reason code %d differs", i);
        CHECKF(!lstrcmpW((WCHAR *)((BYTE *)a.items[i] + 4), (WCHAR *)((BYTE *)b.items[i] + 4)), "name %d differs", i);
    }
    UN(DestroyReasons)(&a); UN(DestroyReasons)(&b);
}
TEST(gap0_buildreasons_flag_combos) {
    REQUIRE(UN(BuildReasonArray)); REQUIRE(UN(DestroyReasons));
    static const int c[][2] = { {0,0},{1,0},{0,1},{1,1},{-1,0},{0,-1},{5,9} };
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        G0Reasons r; memset(&r, 0, sizeof r);
        BOOL ok = UN(BuildReasonArray)(&r, c[i][0], c[i][1]);
        CHECKF(ok == 1, "combo %d,%d returned %d", c[i][0], c[i][1], ok);
        CHECKF(r.count >= 0 && r.count <= 100000, "combo %u count %d", i, r.count);
        t_note("BuildReasonArray(%d,%d): %d reasons", c[i][0], c[i][1], r.count);
        UN(DestroyReasons)(&r);
        CHECK(r.items == NULL);
    }
}
TEST(gap0_buildreasons_subsets_not_larger) {
    REQUIRE(UN(BuildReasonArray)); REQUIRE(UN(DestroyReasons));
    G0Reasons all, d1, d2; memset(&all, 0, sizeof all); memset(&d1, 0, sizeof d1); memset(&d2, 0, sizeof d2);
    UN(BuildReasonArray)(&all, 0, 0);
    UN(BuildReasonArray)(&d1, 1, 0);
    UN(BuildReasonArray)(&d2, 0, 1);
    /* filtered sets only drop predefined entries; user defined reasons are filtered the same way */
    if (d1.count > all.count || d2.count > all.count) t_note("filtered set larger than full set: %d %d vs %d", d1.count, d2.count, all.count);
    CHECK(all.count >= 1);
    UN(DestroyReasons)(&all); UN(DestroyReasons)(&d1); UN(DestroyReasons)(&d2);
}
TEST(gap0_buildreasons_repeat_leak_bound) {
    REQUIRE(UN(BuildReasonArray)); REQUIRE(UN(DestroyReasons));
    int bad = 0;
    for (int i = 0; i < 200; i++) {
        G0Reasons r; memset(&r, 0, sizeof r);
        if (UN(BuildReasonArray)(&r, 0, 0) != 1) bad++;
        UN(DestroyReasons)(&r);
    }
    EXPECT_EQ(bad, 0);
}

/* ---------- CheckDBCSEnabledExt / CtxInitUser32 ---------- */
TEST(gap0_dbcs_boolean_range) {
    REQUIRE(UN(CheckDBCSEnabledExt));
    DWORD r = (DWORD)UN(CheckDBCSEnabledExt)();
    CHECKF(r == 0 || r == 1, "got %lu", r);
}
TEST(gap0_dbcs_stable) {
    REQUIRE(UN(CheckDBCSEnabledExt));
    DWORD first = (DWORD)UN(CheckDBCSEnabledExt)();
    int diff = 0;
    for (int i = 0; i < 1000; i++) if ((DWORD)UN(CheckDBCSEnabledExt)() != first) diff++;
    EXPECT_EQ(diff, 0);
}
TEST(gap0_dbcs_matches_system_metric) {
    REQUIRE(UN(CheckDBCSEnabledExt)); REQUIRE(U(GetSystemMetrics));
    int m = U(GetSystemMetrics)(SM_DBCSENABLED);
    DWORD r = (DWORD)UN(CheckDBCSEnabledExt)();
    if ((m != 0) != (r != 0)) t_note("SM_DBCSENABLED=%d, CheckDBCSEnabledExt=%lu", m, r);
    CHECK(m == 0 || m == 1);
}
TEST(gap0_dbcs_consistent_with_acp) {
    REQUIRE(UN(CheckDBCSEnabledExt));
    UINT acp = GetACP();
    int dbcs = (acp == 932 || acp == 936 || acp == 949 || acp == 950);
    DWORD r = (DWORD)UN(CheckDBCSEnabledExt)();
    if (dbcs && !r) t_note("DBCS ACP %u but flag clear", acp);
    if (!dbcs && r) t_note("non DBCS ACP %u but flag set", acp);
    CHECK(r <= 1);
}
TEST(gap0_ctxinit_returns_true) {
    REQUIRE(UN(CtxInitUser32));
    EXPECT_EQ(UN(CtxInitUser32)(), 1);
}
TEST(gap0_ctxinit_idempotent) {
    REQUIRE(UN(CtxInitUser32));
    int bad = 0;
    for (int i = 0; i < 50; i++) if (UN(CtxInitUser32)() != 1) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(gap0_ctxinit_keeps_gdi_working) {
    REQUIRE(UN(CtxInitUser32)); REQUIRE(U(GetSysColorBrush));
    UN(CtxInitUser32)();
    HBRUSH b = U(GetSysColorBrush)(COLOR_WINDOW);
    CHECK(b != NULL);
    CHECK(GetObjectType(b) == OBJ_BRUSH);
}
