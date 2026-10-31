/* gap_04: resolve-only / harmless-call coverage for internal and
 * undocumented user32 exports, plus ReasonCodeNeeds* bit tests. */
#include "t.h"

struct exp { const char *name; unsigned ord; };
static const struct exp g_exp[] = {
    {"RIMReadInput", 2223}, {"RIMRegisterForInput", 2224}, {"RIMRemoveInputObserver", 2225},
    {"RIMSetExtendedDeviceProperty", 2226}, {"RIMSetTestModeStatus", 2227},
    {"RIMUnregisterForInput", 2228}, {"RIMUpdateInputObserverRegistration", 2229},
    {"ReasonCodeNeedsBugID", 2234}, {"ReasonCodeNeedsComment", 2235}, {"RecordShutdownReason", 0},
    {"RegisterBSDRWindow", 2238}, {"RegisterDManipHook", 2245}, {"RegisterErrorReportingDialog", 2248},
    {"RegisterFrostWindow", 2249}, {"RegisterGhostWindow", 2250}, {"RegisterLogonProcess", 2252},
    {"RegisterPointerInputTarget", 2255}, {"RegisterPointerInputTargetEx", 2256},
    {"RegisterServicesProcess", 2259}, {"RegisterSessionPort", 2260}, {"RegisterSystemThread", 2263},
    {"RegisterTasklist", 2264}, {"ReleaseDwmHitTestWaiters", 2272}, {"RemoveInjectionDevice", 2274},
    {"RemoveThreadTSFEventAwareness", 2278}, {"RemoveVisualIdentifier", 2279}, {"ReportInertia", 2551},
    {"ResolveDesktopForWOW", 2281}, {"ScrollChildren", 2284}, {"SendIMEMessageExA", 2290},
    {"SendIMEMessageExW", 2291}, {"SetCoreWindow", 2571}, {"SetCursorContents", 2314},
    {"SetDebugErrorLevel", 2316}, {"SetDeskWallpaper", 2317}, {"SetDesktopColorTransform", 2318},
    {"SetDialogControlDpiChangeBehavior", 2319}, {"SetDialogDpiChangeBehavior", 2320},
    {"SetDisplayAutoRotationPreferences", 2321}, {"SetDisplayConfig", 2322},
};
#define NEXP ((int)(sizeof g_exp / sizeof g_exp[0]))

static int in_user32(FARPROC p) {
    HMODULE h = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h)) return 0;
    return h == t_user32();
}
static int is_exec(FARPROC p) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void *)p, &mbi, sizeof mbi)) return 0;
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}
static void check_export(const char *name, unsigned ord) {
    FARPROC p = t_proc(name);
    CHECKF(p != NULL, "%s not exported", name);
    if (!p) return;
    CHECKF(in_user32(p), "%s does not resolve inside user32 (forwarder?)", name);
    CHECKF(is_exec(p), "%s not executable", name);
    FARPROC p2 = t_proc(name);
    CHECKF(p == p2, "%s unstable address", name);
    if (ord) {
        FARPROC po = t_proc_ord(ord);
        if (po != p) t_note("%s: ordinal %u does not map to the named export on this build", name, ord);
    }
}

#define RESOLVE_TEST(n) TEST(gap4_resolve_##n) { REQUIRE(t_proc(#n)); \
    for (int i = 0; i < NEXP; i++) if (!strcmp(g_exp[i].name, #n)) check_export(g_exp[i].name, g_exp[i].ord); }

RESOLVE_TEST(RIMReadInput)
RESOLVE_TEST(RIMRegisterForInput)
RESOLVE_TEST(RIMRemoveInputObserver)
RESOLVE_TEST(RIMSetExtendedDeviceProperty)
RESOLVE_TEST(RIMSetTestModeStatus)
RESOLVE_TEST(RIMUnregisterForInput)
RESOLVE_TEST(RIMUpdateInputObserverRegistration)
RESOLVE_TEST(ReasonCodeNeedsBugID)
RESOLVE_TEST(ReasonCodeNeedsComment)
RESOLVE_TEST(RecordShutdownReason)
RESOLVE_TEST(RegisterBSDRWindow)
RESOLVE_TEST(RegisterDManipHook)
RESOLVE_TEST(RegisterErrorReportingDialog)
RESOLVE_TEST(RegisterFrostWindow)
RESOLVE_TEST(RegisterGhostWindow)
RESOLVE_TEST(RegisterLogonProcess)
RESOLVE_TEST(RegisterPointerInputTarget)
RESOLVE_TEST(RegisterPointerInputTargetEx)
RESOLVE_TEST(RegisterServicesProcess)
RESOLVE_TEST(RegisterSessionPort)
RESOLVE_TEST(RegisterSystemThread)
RESOLVE_TEST(RegisterTasklist)
RESOLVE_TEST(ReleaseDwmHitTestWaiters)
RESOLVE_TEST(RemoveInjectionDevice)
RESOLVE_TEST(RemoveThreadTSFEventAwareness)
RESOLVE_TEST(RemoveVisualIdentifier)
RESOLVE_TEST(ReportInertia)
RESOLVE_TEST(ResolveDesktopForWOW)
RESOLVE_TEST(ScrollChildren)
RESOLVE_TEST(SendIMEMessageExA)
RESOLVE_TEST(SendIMEMessageExW)
RESOLVE_TEST(SetCoreWindow)
RESOLVE_TEST(SetCursorContents)
RESOLVE_TEST(SetDebugErrorLevel)
RESOLVE_TEST(SetDeskWallpaper)
RESOLVE_TEST(SetDesktopColorTransform)
RESOLVE_TEST(SetDialogControlDpiChangeBehavior)
RESOLVE_TEST(SetDialogDpiChangeBehavior)
RESOLVE_TEST(SetDisplayAutoRotationPreferences)
RESOLVE_TEST(SetDisplayConfig)

TEST(gap4_resolve_all_distinct) {
    FARPROC p[64]; int n = 0;
    for (int i = 0; i < NEXP; i++) { p[n] = t_proc(g_exp[i].name); if (!p[n]) { t_note("missing %s", g_exp[i].name); continue; } n++; }
    CHECK(n > 0);
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) CHECK(p[i] != p[j]);
}
TEST(gap4_resolve_ascii_vs_wide_ime) {
    FARPROC a = t_proc("SendIMEMessageExA"), w = t_proc("SendIMEMessageExW");
    REQUIRE(a); REQUIRE(w);
    CHECK(a != w);
    CHECK(t_proc("SendIMEMessageEx") == NULL || t_proc("SendIMEMessageEx") != a);
    CHECK(t_proc("SendIMEMessageExAA") == NULL);
    CHECK(t_proc("SendIMEMessageExWW") == NULL);
}
TEST(gap4_resolve_nonexistent_neighbours) {
    REQUIRE(t_proc("ReasonCodeNeedsBugID"));
    static const char *bad[] = {"ReasonCodeNeedsBugIDA", "ReasonCodeNeedsBugIDW", "ReasonCodeNeedsComments",
        "RIMReadInputEx", "RegisterTasklistA", "SetDeskWallpaperA", "SetDeskWallpaperW", "RegisterFrostWindowEx", "reasoncodeneedsbugid"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) CHECKF(t_proc(bad[i]) == NULL, "%s unexpectedly exported", bad[i]);
}

/* ---------------- stub aliasing known from the Win10 image ---------------- */
TEST(gap4_alias_pointer_target_ex) {
    FARPROC a = t_proc("RegisterPointerInputTargetEx"), b = t_proc("UnregisterPointerInputTargetEx"), c = t_proc("GetPointerFrameArrivalTimes");
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    CHECK(a == b);
    CHECK(a == c);
}
TEST(gap4_alias_remove_injection_device) {
    FARPROC a = t_proc("RemoveInjectionDevice"), b = t_proc("DestroySyntheticPointerDevice");
    REQUIRE(a); REQUIRE(b);
    CHECK(a == b);
}
TEST(gap4_alias_debug_error_level) {
    FARPROC a = t_proc("SetDebugErrorLevel");
    REQUIRE(a);
    FARPROC b = t_proc("CheckBannedOneCoreTransformApi");
    if (b) CHECK(a == b); else t_note("CheckBannedOneCoreTransformApi missing");
    FARPROC c = t_proc("SetOCTBannedApiExemption");
    if (c) CHECK(a == c); else t_note("SetOCTBannedApiExemption missing");
}
TEST(gap4_debug_error_level_is_harmless_ret) {
    typedef void (WINAPI *fn_t)(DWORD);
    fn_t f = (fn_t)t_proc("SetDebugErrorLevel");
    REQUIRE(f);
    /* The Win10 body is a bare 'ret'. Calling it must not fault nor alter last-error. */
    const unsigned char *code = (const unsigned char *)f;
    if (code[0] == 0xC2 || code[0] == 0xC3) {
        SET_ERR(0x1234);
        f(0); f(1); f(0xFFFFFFFF);
        EXPECT_ERR(0, 0x1234);
    } else {
        t_note("SetDebugErrorLevel body differs on this build (first byte 0x%02x); not calling", code[0]);
    }
}

/* ---------------- ReasonCodeNeedsBugID / ReasonCodeNeedsComment ---------------- */
typedef UINT (WINAPI *rc_fn)(UINT);
#define BUG_BIT 0x02000000u     /* SHTDN_REASON_FLAG_DIRTY_PROBLEM_ID_REQUIRED */
#define COMMENT_BIT 0x01000000u /* SHTDN_REASON_FLAG_COMMENT_REQUIRED */

TEST(gap4_reasoncode_bug_each_bit) {
    rc_fn f = (rc_fn)t_proc("ReasonCodeNeedsBugID"); REQUIRE(f);
    for (int b = 0; b < 32; b++) { UINT v = 1u << b; EXPECT_EQ(f(v), b == 25 ? 1 : 0); }
}
TEST(gap4_reasoncode_comment_each_bit) {
    rc_fn f = (rc_fn)t_proc("ReasonCodeNeedsComment"); REQUIRE(f);
    for (int b = 0; b < 32; b++) { UINT v = 1u << b; EXPECT_EQ(f(v), b == 24 ? 1 : 0); }
}
TEST(gap4_reasoncode_bug_all_but_bit) {
    rc_fn f = (rc_fn)t_proc("ReasonCodeNeedsBugID"); REQUIRE(f);
    for (int b = 0; b < 32; b++) { UINT v = ~(1u << b); EXPECT_EQ(f(v), b == 25 ? 0 : 1); }
}
TEST(gap4_reasoncode_comment_all_but_bit) {
    rc_fn f = (rc_fn)t_proc("ReasonCodeNeedsComment"); REQUIRE(f);
    for (int b = 0; b < 32; b++) { UINT v = ~(1u << b); EXPECT_EQ(f(v), b == 24 ? 0 : 1); }
}
TEST(gap4_reasoncode_boundaries) {
    rc_fn bug = (rc_fn)t_proc("ReasonCodeNeedsBugID"), com = (rc_fn)t_proc("ReasonCodeNeedsComment");
    REQUIRE(bug); REQUIRE(com);
    EXPECT_EQ(bug(0), 0); EXPECT_EQ(com(0), 0);
    EXPECT_EQ(bug(0xFFFFFFFFu), 1); EXPECT_EQ(com(0xFFFFFFFFu), 1);
    EXPECT_EQ(bug(0x00FFFFFFu), 0); EXPECT_EQ(com(0x00FFFFFFu), 0);
    EXPECT_EQ(bug(0xFCFFFFFFu), 0); EXPECT_EQ(com(0xFCFFFFFFu), 0);
    EXPECT_EQ(bug(BUG_BIT), 1); EXPECT_EQ(bug(COMMENT_BIT), 0);
    EXPECT_EQ(com(COMMENT_BIT), 1); EXPECT_EQ(com(BUG_BIT), 0);
    EXPECT_EQ(bug(BUG_BIT | COMMENT_BIT), 1); EXPECT_EQ(com(BUG_BIT | COMMENT_BIT), 1);
}
TEST(gap4_reasoncode_known_reason_values) {
    rc_fn bug = (rc_fn)t_proc("ReasonCodeNeedsBugID"), com = (rc_fn)t_proc("ReasonCodeNeedsComment");
    REQUIRE(bug); REQUIRE(com);
    static const struct { UINT v; int b, c; const char *n; } t[] = {
        {0x00000000, 0, 0, "SHTDN_REASON_NONE"},
        {0x00040001, 0, 0, "MAJOR_APPLICATION|HUNG"},
        {0x00020003, 0, 0, "HARDWARE|INSTALLATION"},
        {0x80020002, 0, 0, "PLANNED hw maintenance"},
        {0x40000000, 0, 0, "USER_DEFINED"},
        {0x80000000, 0, 0, "PLANNED"},
        {0x04000000, 0, 0, "CLEAN_UI"},
        {0x08000000, 0, 0, "DIRTY_UI"},
        {0x01000000, 0, 1, "COMMENT_REQUIRED"},
        {0x02000000, 1, 0, "DIRTY_PROBLEM_ID_REQUIRED"},
        {0x03000000, 1, 1, "both flags"},
        {0x8004FFFF, 0, 0, "planned app other"},
        {0x83040001, 1, 1, "planned + both flags"},
        {0xFD000000, 0, 1, "all but bug"},
        {0xFE000000, 1, 0, "all but comment"},
        {0x0A000000, 1, 0, "DIRTY_UI|DIRTY_PROBLEM_ID"},
        {0x05000000, 0, 1, "CLEAN_UI|COMMENT"},
        {0x00FFFFFF, 0, 0, "low 24 bits"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        CHECKF(bug(t[i].v) == (UINT)t[i].b, "bug %s %08x", t[i].n, t[i].v);
        CHECKF(com(t[i].v) == (UINT)t[i].c, "comment %s %08x", t[i].n, t[i].v);
    }
}
TEST(gap4_reasoncode_random_values) {
    rc_fn bug = (rc_fn)t_proc("ReasonCodeNeedsBugID"), com = (rc_fn)t_proc("ReasonCodeNeedsComment");
    REQUIRE(bug); REQUIRE(com);
    UINT x = 0x12345678;
    for (int i = 0; i < 20000; i++) {
        x = x * 1664525u + 1013904223u;
        if (bug(x) != ((x >> 25) & 1)) { CHECKF(0, "bug %08x", x); break; }
        if (com(x) != ((x >> 24) & 1)) { CHECKF(0, "comment %08x", x); break; }
    }
    CHECK(1);
}
TEST(gap4_reasoncode_result_is_zero_or_one) {
    rc_fn bug = (rc_fn)t_proc("ReasonCodeNeedsBugID"), com = (rc_fn)t_proc("ReasonCodeNeedsComment");
    REQUIRE(bug); REQUIRE(com);
    UINT x = 7;
    for (int i = 0; i < 2000; i++) { x = x * 22695477u + 1u; UINT a = bug(x), b = com(x); CHECK(a <= 1 && b <= 1); }
}
TEST(gap4_reasoncode_preserves_last_error) {
    rc_fn bug = (rc_fn)t_proc("ReasonCodeNeedsBugID"), com = (rc_fn)t_proc("ReasonCodeNeedsComment");
    REQUIRE(bug); REQUIRE(com);
    SET_ERR(0x4242); bug(0xFFFFFFFF); com(0xFFFFFFFF); bug(0); com(0);
    EXPECT_ERR(0, 0x4242);
}
TEST(gap4_reasoncode_deterministic_repeat) {
    rc_fn bug = (rc_fn)t_proc("ReasonCodeNeedsBugID"); REQUIRE(bug);
    for (int i = 0; i < 1000; i++) { EXPECT_EQ(bug(BUG_BIT), 1); EXPECT_EQ(bug(COMMENT_BIT), 0); }
}
TEST(gap4_reasoncode_shutdown_constants_consistent) {
    rc_fn bug = (rc_fn)t_proc("ReasonCodeNeedsBugID"), com = (rc_fn)t_proc("ReasonCodeNeedsComment");
    REQUIRE(bug); REQUIRE(com);
    /* Public SHTDN_REASON_FLAG_* bits from winuser.h/reason.h */
    EXPECT_EQ(com(0x01000000), 1);   /* SHTDN_REASON_FLAG_COMMENT_REQUIRED */
    EXPECT_EQ(bug(0x02000000), 1);   /* SHTDN_REASON_FLAG_DIRTY_PROBLEM_ID_REQUIRED */
    EXPECT_EQ(bug(0x04000000), 0);   /* SHTDN_REASON_FLAG_CLEAN_UI */
    EXPECT_EQ(com(0x08000000), 0);   /* SHTDN_REASON_FLAG_DIRTY_UI */
}

/* ---------------- RecordShutdownReason (error paths only) ----------------
 * Requires the shutdown privilege; otherwise fails with ERROR_ACCESS_DENIED(5).
 * With the privilege, a NULL pointer or a cbSize other than 0x20/0x30 gives
 * ERROR_INVALID_PARAMETER(87). We only ever pass invalid arguments so nothing
 * is written to the event log. */
typedef DWORD (WINAPI *rsr_fn)(void *);
static void rsr_expect_fail(rsr_fn f, void *p, const char *what) {
    SET_ERR(0xBEEF);
    DWORD r = f(p);
    DWORD e = GetLastError();
    CHECKF(r == 0, "%s returned %lu", what, r);
    CHECKF(e == 5 || e == 87, "%s last error %lu", what, e);
}
TEST(gap4_recordshutdown_null) {
    rsr_fn f = (rsr_fn)t_proc("RecordShutdownReason"); REQUIRE(f);
    rsr_expect_fail(f, NULL, "NULL");
}
TEST(gap4_recordshutdown_bad_cbsize) {
    rsr_fn f = (rsr_fn)t_proc("RecordShutdownReason"); REQUIRE(f);
    static const DWORD sizes[] = {0, 1, 4, 0x1F, 0x21, 0x28, 0x2F, 0x31, 0x40, 0x10, 0x100, 0x60, 0x70, 0xFFFFFFFF, 0x80000020};
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        DWORD buf[16]; memset(buf, 0, sizeof buf); buf[0] = sizes[i];
        char w[48]; sprintf(w, "cbSize=0x%lx", sizes[i]);
        rsr_expect_fail(f, buf, w);
    }
}
TEST(gap4_recordshutdown_access_denied_vs_invalid_consistent) {
    rsr_fn f = (rsr_fn)t_proc("RecordShutdownReason"); REQUIRE(f);
    DWORD bad[16]; memset(bad, 0, sizeof bad); bad[0] = 3;
    SET_ERR(0); f(NULL); DWORD e1 = GetLastError();
    SET_ERR(0); f(bad); DWORD e2 = GetLastError();
    /* Without the privilege both are 5; with it both are 87. */
    CHECKF(e1 == e2, "NULL err %lu vs badsize err %lu", e1, e2);
    t_note("RecordShutdownReason invalid-arg error on this host: %lu (5=no shutdown privilege, 87=has it)", e1);
}
TEST(gap4_recordshutdown_repeat) {
    rsr_fn f = (rsr_fn)t_proc("RecordShutdownReason"); REQUIRE(f);
    for (int i = 0; i < 200; i++) { SET_ERR(0); DWORD r = f(NULL); if (r != 0) { CHECK(r == 0); break; } }
    CHECK(1);
}

/* ---------------- harmless calls with no side-effect paths ---------------- */
TEST(gap4_registersystemthread_reserved_nonzero_is_noop) {
    typedef void (WINAPI *fn_t)(DWORD, DWORD);
    fn_t f = (fn_t)t_proc("RegisterSystemThread"); REQUIRE(f);
    /* Win10 body only forwards to win32k when the second argument is 0, so a
     * non-zero second argument must neither fault nor touch last-error. */
    SET_ERR(0x777);
    f(0, 1); f(0xFFFFFFFF, 2); f(1, 0x80000000); f(0, 0xFFFFFFFF);
    EXPECT_ERR(0, 0x777);
}
TEST(gap4_removethreadtsf_noop_args) {
    typedef void (WINAPI *fn_t)(UINT);
    fn_t f = (fn_t)t_proc("RemoveThreadTSFEventAwareness"); REQUIRE(f);
    /* 0 is ignored, and any value touching reserved bits (mask 0xffffe8fc) is rejected
     * client-side without calling into the kernel. */
    SET_ERR(0x555);
    f(0);
    f(0x4); f(0x8); f(0x10); f(0x20); f(0x40); f(0x80); f(0x800);
    f(0x2000); f(0x10000); f(0x80000000); f(0xFFFFFFFF); f(0xFFFFE8FC);
    EXPECT_ERR(0, 0x555);
}
