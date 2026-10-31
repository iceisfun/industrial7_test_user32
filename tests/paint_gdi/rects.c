#include "pg.h"
#include <limits.h>

/* ---- SetRect / SetRectEmpty / CopyRect ---- */
TEST(paint_gdi_rect_setrect_basic) {
    REQUIRE(U(SetRect));
    RECT r = {9,9,9,9};
    CHECK(U(SetRect)(&r, 1, 2, 3, 4));
    CHECK_RECT(r, 1, 2, 3, 4);
    CHECK(U(SetRect)(&r, -10, -20, 30, 40));
    CHECK_RECT(r, -10, -20, 30, 40);
    CHECK(U(SetRect)(&r, INT_MIN, INT_MIN, INT_MAX, INT_MAX));
    CHECK_RECT(r, INT_MIN, INT_MIN, INT_MAX, INT_MAX);
}
TEST(paint_gdi_rect_setrect_inverted_kept) {
    REQUIRE(U(SetRect));
    RECT r;
    CHECK(U(SetRect)(&r, 10, 20, 5, 6));
    CHECK_RECT(r, 10, 20, 5, 6);   /* no normalisation */
}
TEST(paint_gdi_rect_setrect_null) {
    REQUIRE(U(SetRect));
    /* NULL rect is tolerated by user32 (returns FALSE without faulting) */
    CHECK(U(SetRect)(NULL, 1, 2, 3, 4) == FALSE);
    REQUIRE(U(SetRectEmpty));
    CHECK(U(SetRectEmpty)(NULL) == FALSE);
}
TEST(paint_gdi_rect_setrectempty) {
    REQUIRE(U(SetRectEmpty));
    RECT r = {1,2,3,4};
    CHECK(U(SetRectEmpty)(&r));
    CHECK_RECT(r, 0, 0, 0, 0);
    CHECK(U(IsRectEmpty)(&r));
}
TEST(paint_gdi_rect_copyrect) {
    REQUIRE(U(CopyRect));
    RECT s = {5,6,7,8}, d = {0,0,0,0};
    CHECK(U(CopyRect)(&d, &s));
    CHECK_RECT(d, 5, 6, 7, 8);
    RECT inv = {10,10,-3,-3};
    CHECK(U(CopyRect)(&d, &inv));
    CHECK_RECT(d, 10, 10, -3, -3);
    CHECK(U(CopyRect)(&s, &s));
    CHECK_RECT(s, 5, 6, 7, 8);
}
TEST(paint_gdi_rect_copyrect_null) {
    REQUIRE(U(CopyRect));
    RECT s = {1,2,3,4};
    CHECK(U(CopyRect)(NULL, &s) == FALSE);
    CHECK(U(CopyRect)(&s, NULL) == FALSE);
    CHECK_RECT(s, 1, 2, 3, 4);
}

/* ---- Inflate / Offset ---- */
TEST(paint_gdi_rect_inflate) {
    REQUIRE(U(InflateRect));
    static const struct { RECT in; int dx, dy; RECT out; } t[] = {
        {{10,10,20,20},  1,  2, { 9, 8,21,22}},
        {{10,10,20,20}, -1, -2, {11,12,19,18}},
        {{10,10,20,20},  0,  0, {10,10,20,20}},
        {{10,10,20,20}, -5, -5, {15,15,15,15}},
        {{10,10,20,20}, -6, -6, {16,16,14,14}},   /* goes inverted, not clamped */
        {{0,0,0,0},      3,  3, {-3,-3, 3, 3}},
        {{-5,-5,5,5},    5,  0, {-10,-5,10, 5}},
        {{-5,-5,5,5},    0, -5, {-5, 0, 5, 0}},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT r = t[i].in;
        BOOL ok = U(InflateRect)(&r, t[i].dx, t[i].dy);
        CHECKF(ok, "case %u", i);
        CHECKF(r.left == t[i].out.left && r.top == t[i].out.top && r.right == t[i].out.right && r.bottom == t[i].out.bottom,
               "case %u got (%ld,%ld,%ld,%ld)", i, RECT_STR(r));
    }
    CHECK(U(InflateRect)(NULL, 1, 1) == FALSE);
}
TEST(paint_gdi_rect_inflate_wraps) {
    REQUIRE(U(InflateRect));
    RECT r = {0, 0, INT_MAX, 10};
    CHECK(U(InflateRect)(&r, 1, 0));
    CHECK(r.right == INT_MIN);   /* plain two's complement arithmetic */
    CHECK(r.left == -1);
}
TEST(paint_gdi_rect_offset) {
    REQUIRE(U(OffsetRect));
    RECT r = {1,2,3,4};
    CHECK(U(OffsetRect)(&r, 10, 20));
    CHECK_RECT(r, 11, 22, 13, 24);
    CHECK(U(OffsetRect)(&r, -11, -22));
    CHECK_RECT(r, 0, 0, 2, 2);
    CHECK(U(OffsetRect)(&r, 0, 0));
    CHECK_RECT(r, 0, 0, 2, 2);
    RECT e = {5,5,5,5};
    CHECK(U(OffsetRect)(&e, 1, 1));
    CHECK_RECT(e, 6, 6, 6, 6);
    RECT inv = {10,10,0,0};
    CHECK(U(OffsetRect)(&inv, 1, -1));
    CHECK_RECT(inv, 11, 9, 1, -1);
    CHECK(U(OffsetRect)(NULL, 1, 1) == FALSE);
}
TEST(paint_gdi_rect_offset_wraps) {
    REQUIRE(U(OffsetRect));
    RECT r = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
    CHECK(U(OffsetRect)(&r, 1, 1));
    CHECK_RECT(r, INT_MIN, INT_MIN, INT_MIN, INT_MIN);
}

/* ---- IsRectEmpty / EqualRect / PtInRect ---- */
TEST(paint_gdi_rect_isempty) {
    REQUIRE(U(IsRectEmpty));
    static const struct { RECT r; BOOL e; } t[] = {
        {{0,0,0,0}, 1}, {{0,0,1,1}, 0}, {{0,0,1,0}, 1}, {{0,0,0,1}, 1},
        {{5,5,5,9}, 1}, {{5,5,9,5}, 1}, {{10,0,0,10}, 1}, {{0,10,10,0}, 1},
        {{-5,-5,-4,-4}, 0}, {{INT_MIN,INT_MIN,INT_MAX,INT_MAX}, 0}, {{1,1,0,0}, 1},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT r = t[i].r;
        CHECKF(!!U(IsRectEmpty)(&r) == t[i].e, "case %u", i);
    }
    CHECK(U(IsRectEmpty)(NULL) == TRUE);   /* NULL counts as empty */
}
TEST(paint_gdi_rect_equal) {
    REQUIRE(U(EqualRect));
    RECT a = {1,2,3,4}, b = {1,2,3,4}, c = {1,2,3,5}, d = {0,2,3,4}, e = {1,0,3,4}, f = {1,2,0,4};
    CHECK(U(EqualRect)(&a, &b));
    CHECK(U(EqualRect)(&a, &a));
    CHECK(!U(EqualRect)(&a, &c));
    CHECK(!U(EqualRect)(&a, &d));
    CHECK(!U(EqualRect)(&a, &e));
    CHECK(!U(EqualRect)(&a, &f));
    /* two different empty rects are not equal */
    RECT z1 = {0,0,0,0}, z2 = {1,1,1,1};
    CHECK(!U(EqualRect)(&z1, &z2));
    CHECK(U(EqualRect)(NULL, &a) == FALSE);
    CHECK(U(EqualRect)(&a, NULL) == FALSE);
    CHECK(U(EqualRect)(NULL, NULL) == FALSE);
}
TEST(paint_gdi_rect_ptinrect_edges) {
    REQUIRE(U(PtInRect));
    RECT r = {10, 20, 30, 40};
    CHECK(U(PtInRect)(&r, (POINT){10, 20}));      /* top-left inclusive */
    CHECK(U(PtInRect)(&r, (POINT){29, 39}));
    CHECK(!U(PtInRect)(&r, (POINT){30, 39}));     /* right exclusive */
    CHECK(!U(PtInRect)(&r, (POINT){29, 40}));     /* bottom exclusive */
    CHECK(!U(PtInRect)(&r, (POINT){30, 40}));
    CHECK(!U(PtInRect)(&r, (POINT){9, 20}));
    CHECK(!U(PtInRect)(&r, (POINT){10, 19}));
    CHECK(U(PtInRect)(&r, (POINT){20, 30}));
}
TEST(paint_gdi_rect_ptinrect_degenerate) {
    REQUIRE(U(PtInRect));
    RECT e = {5,5,5,5};
    CHECK(!U(PtInRect)(&e, (POINT){5, 5}));
    RECT inv = {30, 40, 10, 20};
    CHECK(!U(PtInRect)(&inv, (POINT){20, 30}));
    CHECK(!U(PtInRect)(&inv, (POINT){10, 20}));
    CHECK(!U(PtInRect)(&inv, (POINT){30, 40}));
    RECT w0 = {0, 0, 0, 10};
    CHECK(!U(PtInRect)(&w0, (POINT){0, 5}));
    RECT neg = {-10, -10, -5, -5};
    CHECK(U(PtInRect)(&neg, (POINT){-10, -10}));
    CHECK(U(PtInRect)(&neg, (POINT){-6, -6}));
    CHECK(!U(PtInRect)(&neg, (POINT){-5, -6}));
    RECT big = {INT_MIN, INT_MIN, INT_MAX, INT_MAX};
    CHECK(U(PtInRect)(&big, (POINT){INT_MIN, INT_MIN}));
    CHECK(U(PtInRect)(&big, (POINT){0, 0}));
    CHECK(!U(PtInRect)(&big, (POINT){INT_MAX, 0}));
    CHECK(U(PtInRect)(NULL, (POINT){0, 0}) == FALSE);
}

/* ---- IntersectRect ---- */
static void isect_case(const char *nm, RECT a, RECT b, BOOL want, RECT w) {
    RECT d = {77,77,77,77};
    BOOL r = U(IntersectRect)(&d, &a, &b);
    CHECKF(!!r == want, "%s ret=%d want %d", nm, r, want);
    CHECKF(d.left == w.left && d.top == w.top && d.right == w.right && d.bottom == w.bottom,
           "%s got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)", nm, RECT_STR(d), RECT_STR(w));
    /* commutative */
    RECT d2 = {55,55,55,55};
    BOOL r2 = U(IntersectRect)(&d2, &b, &a);
    CHECKF(!!r2 == want && d2.left == w.left && d2.top == w.top && d2.right == w.right && d2.bottom == w.bottom, "%s commuted", nm);
}
TEST(paint_gdi_rect_intersect_overlap) {
    REQUIRE(U(IntersectRect));
    isect_case("partial", (RECT){0,0,10,10}, (RECT){5,5,15,15}, 1, (RECT){5,5,10,10});
    isect_case("contained", (RECT){0,0,10,10}, (RECT){2,3,4,5}, 1, (RECT){2,3,4,5});
    isect_case("identical", (RECT){0,0,10,10}, (RECT){0,0,10,10}, 1, (RECT){0,0,10,10});
    isect_case("1px", (RECT){0,0,10,10}, (RECT){9,9,20,20}, 1, (RECT){9,9,10,10});
    isect_case("negative", (RECT){-10,-10,0,0}, (RECT){-5,-5,5,5}, 1, (RECT){-5,-5,0,0});
    isect_case("strip", (RECT){0,0,100,10}, (RECT){40,-50,50,50}, 1, (RECT){40,0,50,10});
}
TEST(paint_gdi_rect_intersect_empty_result) {
    REQUIRE(U(IntersectRect));
    isect_case("touch-x", (RECT){0,0,10,10}, (RECT){10,0,20,10}, 0, (RECT){0,0,0,0});
    isect_case("touch-y", (RECT){0,0,10,10}, (RECT){0,10,10,20}, 0, (RECT){0,0,0,0});
    isect_case("apart", (RECT){0,0,10,10}, (RECT){50,50,60,60}, 0, (RECT){0,0,0,0});
    isect_case("corner", (RECT){0,0,10,10}, (RECT){10,10,20,20}, 0, (RECT){0,0,0,0});
}
TEST(paint_gdi_rect_intersect_degenerate_inputs) {
    REQUIRE(U(IntersectRect));
    isect_case("empty-src", (RECT){5,5,5,5}, (RECT){0,0,10,10}, 0, (RECT){0,0,0,0});
    isect_case("zero-width", (RECT){5,0,5,10}, (RECT){0,0,10,10}, 0, (RECT){0,0,0,0});
    isect_case("inverted", (RECT){10,10,0,0}, (RECT){0,0,10,10}, 0, (RECT){0,0,0,0});
    isect_case("both-inverted", (RECT){10,10,0,0}, (RECT){10,10,0,0}, 0, (RECT){0,0,0,0});
    isect_case("both-empty", (RECT){0,0,0,0}, (RECT){0,0,0,0}, 0, (RECT){0,0,0,0});
}
TEST(paint_gdi_rect_intersect_alias_and_null) {
    REQUIRE(U(IntersectRect));
    RECT a = {0,0,10,10}, b = {5,5,15,15};
    CHECK(U(IntersectRect)(&a, &a, &b));      /* dest aliases src1 */
    CHECK_RECT(a, 5, 5, 10, 10);
    RECT c = {0,0,10,10}, d = {5,5,15,15};
    CHECK(U(IntersectRect)(&d, &c, &d));      /* dest aliases src2 */
    CHECK_RECT(d, 5, 5, 10, 10);
    RECT x = {0,0,10,10}, y = {20,20,30,30};
    CHECK(!U(IntersectRect)(&x, &x, &y));
    CHECK_RECT(x, 0, 0, 0, 0);
    RECT s = {0,0,1,1};
    CHECK(U(IntersectRect)(NULL, &s, &s) == FALSE);
    RECT dd = {9,9,9,9};
    CHECK(U(IntersectRect)(&dd, NULL, &s) == FALSE);
    CHECK(U(IntersectRect)(&dd, &s, NULL) == FALSE);
    CHECK_RECT(dd, 9, 9, 9, 9);   /* untouched on NULL source */
}
TEST(paint_gdi_rect_intersect_extremes) {
    REQUIRE(U(IntersectRect));
    isect_case("full", (RECT){INT_MIN,INT_MIN,INT_MAX,INT_MAX}, (RECT){-1,-1,1,1}, 1, (RECT){-1,-1,1,1});
    isect_case("full-full", (RECT){INT_MIN,INT_MIN,INT_MAX,INT_MAX}, (RECT){INT_MIN,INT_MIN,INT_MAX,INT_MAX}, 1, (RECT){INT_MIN,INT_MIN,INT_MAX,INT_MAX});
}

/* ---- UnionRect ---- */
static void union_case(const char *nm, RECT a, RECT b, BOOL want, RECT w) {
    RECT d = {77,77,77,77};
    BOOL r = U(UnionRect)(&d, &a, &b);
    CHECKF(!!r == want, "%s ret=%d want %d", nm, r, want);
    CHECKF(d.left == w.left && d.top == w.top && d.right == w.right && d.bottom == w.bottom,
           "%s got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)", nm, RECT_STR(d), RECT_STR(w));
}
TEST(paint_gdi_rect_union_basic) {
    REQUIRE(U(UnionRect));
    union_case("overlap", (RECT){0,0,10,10}, (RECT){5,5,15,15}, 1, (RECT){0,0,15,15});
    union_case("apart", (RECT){0,0,1,1}, (RECT){100,100,101,101}, 1, (RECT){0,0,101,101});
    union_case("contained", (RECT){0,0,10,10}, (RECT){2,2,3,3}, 1, (RECT){0,0,10,10});
    union_case("negative", (RECT){-10,-10,-5,-5}, (RECT){5,5,10,10}, 1, (RECT){-10,-10,10,10});
    union_case("touching", (RECT){0,0,10,10}, (RECT){10,0,20,10}, 1, (RECT){0,0,20,10});
}
TEST(paint_gdi_rect_union_empty_operands) {
    REQUIRE(U(UnionRect));
    union_case("src1-empty", (RECT){50,50,50,50}, (RECT){1,2,3,4}, 1, (RECT){1,2,3,4});
    union_case("src2-empty", (RECT){1,2,3,4}, (RECT){50,50,50,50}, 1, (RECT){1,2,3,4});
    union_case("src1-inverted", (RECT){9,9,0,0}, (RECT){1,2,3,4}, 1, (RECT){1,2,3,4});
    union_case("both-empty", (RECT){5,5,5,5}, (RECT){7,7,7,7}, 0, (RECT){0,0,0,0});
    union_case("both-inverted", (RECT){9,9,0,0}, (RECT){8,8,1,1}, 0, (RECT){0,0,0,0});
}
TEST(paint_gdi_rect_union_alias_null) {
    REQUIRE(U(UnionRect));
    RECT a = {0,0,5,5}, b = {3,3,9,9};
    CHECK(U(UnionRect)(&a, &a, &b));
    CHECK_RECT(a, 0, 0, 9, 9);
    RECT s = {0,0,1,1};
    CHECK(U(UnionRect)(NULL, &s, &s) == FALSE);
    RECT d = {4,4,4,4};
    CHECK(U(UnionRect)(&d, NULL, &s) == FALSE);
    CHECK(U(UnionRect)(&d, &s, NULL) == FALSE);
    CHECK_RECT(d, 4, 4, 4, 4);
}

/* ---- SubtractRect ---- */
static void sub_case(const char *nm, RECT a, RECT b, BOOL want, RECT w) {
    RECT d = {77,77,77,77};
    BOOL r = U(SubtractRect)(&d, &a, &b);
    CHECKF(!!r == want, "%s ret=%d want %d", nm, r, want);
    CHECKF(d.left == w.left && d.top == w.top && d.right == w.right && d.bottom == w.bottom,
           "%s got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)", nm, RECT_STR(d), RECT_STR(w));
}
TEST(paint_gdi_rect_subtract_trim) {
    REQUIRE(U(SubtractRect));
    RECT a = {0,0,10,10};
    sub_case("trim-right",  a, (RECT){5,-5,20,20},  1, (RECT){0,0,5,10});
    sub_case("trim-left",   a, (RECT){-5,-5,5,20},  1, (RECT){5,0,10,10});
    sub_case("trim-bottom", a, (RECT){-5,5,20,20},  1, (RECT){0,0,10,5});
    sub_case("trim-top",    a, (RECT){-5,-5,20,5},  1, (RECT){0,5,10,10});
    sub_case("trim-right-flush", a, (RECT){5,0,10,10}, 1, (RECT){0,0,5,10});
}
TEST(paint_gdi_rect_subtract_unchanged) {
    REQUIRE(U(SubtractRect));
    RECT a = {0,0,10,10};
    sub_case("disjoint", a, (RECT){20,20,30,30}, 1, a);
    sub_case("touching", a, (RECT){10,0,20,10}, 1, a);
    sub_case("hole-in-middle", a, (RECT){3,3,6,6}, 1, a);
    sub_case("corner-bite", a, (RECT){5,5,20,20}, 1, a);
    sub_case("narrow-strip-in-middle", a, (RECT){4,-5,6,20}, 1, a);
    sub_case("empty-src2", a, (RECT){5,5,5,5}, 1, a);
}
TEST(paint_gdi_rect_subtract_all) {
    REQUIRE(U(SubtractRect));
    RECT a = {0,0,10,10};
    sub_case("cover", a, (RECT){-1,-1,11,11}, 0, (RECT){0,0,0,0});
    sub_case("identical", a, a, 0, (RECT){0,0,0,0});
}
TEST(paint_gdi_rect_subtract_empty_src1) {
    REQUIRE(U(SubtractRect));
    sub_case("empty-src1", (RECT){5,5,5,5}, (RECT){0,0,10,10}, 0, (RECT){5,5,5,5});
    sub_case("inverted-src1", (RECT){9,9,1,1}, (RECT){0,0,10,10}, 0, (RECT){9,9,1,1});
    RECT s = {0,0,1,1};
    CHECK(U(SubtractRect)(NULL, &s, &s) == FALSE);
    RECT d = {4,4,4,4};
    CHECK(U(SubtractRect)(&d, NULL, &s) == FALSE);
    CHECK(U(SubtractRect)(&d, &s, NULL) == FALSE);
    CHECK_RECT(d, 4, 4, 4, 4);
}
TEST(paint_gdi_rect_subtract_alias) {
    REQUIRE(U(SubtractRect));
    RECT a = {0,0,10,10}, b = {5,-5,20,20};
    CHECK(U(SubtractRect)(&a, &a, &b));
    CHECK_RECT(a, 0, 0, 5, 10);
}

/* ---- relations ---- */
TEST(paint_gdi_rect_relations_random) {
    REQUIRE(U(IntersectRect)); REQUIRE(U(UnionRect)); REQUIRE(U(PtInRect)); REQUIRE(U(IsRectEmpty));
    unsigned seed = 12345;
    #define RND() (seed = seed * 1103515245u + 12345u, (int)((seed >> 16) & 0x3f) - 20)
    for (int i = 0; i < 2000; i++) {
        RECT a = {RND(), RND(), RND(), RND()}, b = {RND(), RND(), RND(), RND()}, in, un;
        BOOL hi = U(IntersectRect)(&in, &a, &b);
        BOOL hu = U(UnionRect)(&un, &a, &b);
        if (hi != !U(IsRectEmpty)(&in)) { CHECKF(0, "iter %d isect/empty mismatch", i); break; }
        BOOL ea = U(IsRectEmpty)(&a), eb = U(IsRectEmpty)(&b);
        if (hu != !(ea && eb)) { CHECKF(0, "iter %d union ret", i); break; }
        POINT p = {RND(), RND()};
        BOOL pa = U(PtInRect)(&a, p), pb = U(PtInRect)(&b, p), pi = U(PtInRect)(&in, p);
        if (pi != (pa && pb)) { CHECKF(0, "iter %d ptinrect on intersection", i); break; }
        if ((pa || pb) && !U(PtInRect)(&un, p)) { CHECKF(0, "iter %d point in a|b not in union", i); break; }
    }
    CHECK(1);
}
