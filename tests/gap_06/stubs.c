/* gap6: undocumented stubs, data exports, NLS/IME shims, token helper. */
#include "t.h"

static int readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!p) return 0;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return 0;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    if ((const char *)p + n > (const char *)mbi.BaseAddress + mbi.RegionSize) return 0;
    return 1;
}
static int in_user32(const void *p) {
    HMODULE m = t_user32();
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || !m) return 0;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return 0;
    return (HMODULE)mbi.AllocationBase == m;
}
static int is_exec(const void *p) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return 0;
    return mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}

/* VRipOutput, VTagOutput and WINNLSGetIMEHotkey all resolve to a bare
 * "xor eax,eax; ret" on Win10 x64: any arguments, result 0. */
typedef UINT_PTR (WINAPI *stub_fn)(UINT_PTR, UINT_PTR, UINT_PTR, UINT_PTR);

static const char *const stub_names[] = { "VRipOutput", "VTagOutput", "WINNLSGetIMEHotkey" };

TEST(gap6_stub_resolve) {
    for (int i = 0; i < 3; i++) {
        FARPROC p = t_proc(stub_names[i]);
        if (!p) { t_note("%s not exported", stub_names[i]); continue; }
        CHECKF(in_user32((void *)p), "%s outside user32", stub_names[i]);
        CHECKF(is_exec((void *)p), "%s not executable", stub_names[i]);
        CHECKF(GetProcAddress(t_user32(), stub_names[i]) == p, "unstable address");
    }
}
TEST(gap6_stub_no_ordinal_collision) {
    FARPROC a = t_proc("VRipOutput"), b = t_proc("VTagOutput"), c = t_proc("WINNLSGetIMEHotkey");
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    t_note("stub addrs %p %p %p (identical code folding expected)", a, b, c);
    CHECK(a != t_proc("GetWindowLongA"));
}
static void stub_zero(const char *n) {
    stub_fn f = (stub_fn)t_proc(n);
    if (!f) { t_skip("missing export: %s", n); return; }
    UINT_PTR args[][4] = {
        {0,0,0,0}, {1,2,3,4}, {(UINT_PTR)-1,(UINT_PTR)-1,(UINT_PTR)-1,(UINT_PTR)-1},
        {0x1000,0,0,0}, {0,0x1000,0,0}, {0xdeadbeef,0xcafebabe,0,0},
        {(UINT_PTR)GetDesktopWindow(),0,0,0}, {(UINT_PTR)GetCurrentProcess(),1,0,0},
    };
    for (unsigned i = 0; i < sizeof args / sizeof args[0]; i++) {
        SetLastError(0x1234);
        UINT_PTR r = f(args[i][0], args[i][1], args[i][2], args[i][3]);
        CHECKF((DWORD)r == 0, "%s args#%u returned 0x%llx", n, i, (unsigned long long)r);
        CHECKF(GetLastError() == 0x1234, "%s changed last error to %lu", n, GetLastError());
    }
}
TEST(gap6_vripoutput_returns_zero) { stub_zero("VRipOutput"); }
TEST(gap6_vtagoutput_returns_zero) { stub_zero("VTagOutput"); }
TEST(gap6_winnlsgetimehotkey_returns_zero) { stub_zero("WINNLSGetIMEHotkey"); }
TEST(gap6_winnlsgetimehotkey_window_args) {
    UINT (WINAPI *f)(HWND) = (void *)t_proc("WINNLSGetIMEHotkey");
    REQUIRE(f);
    HWND w = t_make_window(NULL, WS_OVERLAPPED, 0, 0, 0, 10, 10, NULL);
    if (w) {
        CHECK(f(w) == 0);
        DestroyWindow(w);
        CHECK(f(w) == 0); /* destroyed handle: still just 0 */
    }
    CHECK(f(NULL) == 0);
    CHECK(f((HWND)(UINT_PTR)0x7fffffff) == 0);
    CHECK(f(GetDesktopWindow()) == 0);
}
TEST(gap6_stub_repeated_calls) {
    stub_fn f = (stub_fn)t_proc("VRipOutput");
    REQUIRE(f);
    for (int i = 0; i < 20000; i++) if ((DWORD)f(i, i * 3, 0, 0) != 0) { CHECKF(0, "iter %d nonzero", i); break; }
    CHECK(1);
}
TEST(gap6_stub_ansi_string_args) {
    UINT (WINAPI *f)(const char *, ...) = (void *)t_proc("VTagOutput");
    REQUIRE(f);
    CHECK(f("format %d", 5) == 0);
    CHECK(f("") == 0);
    CHECK(f(NULL) == 0);
}

/* WINNLSEnableIME / WINNLSGetEnableStatus dispatch through an internal IMM
 * hook pointer; calling them without an IME-capable window is not
 * deterministic, so only resolve and sanity check the thunks. */
TEST(gap6_winnls_resolve) {
    const char *n[] = { "WINNLSEnableIME", "WINNLSGetEnableStatus" };
    for (int i = 0; i < 2; i++) {
        FARPROC p = t_proc(n[i]);
        if (!p) { t_note("%s not exported", n[i]); continue; }
        CHECKF(in_user32((void *)p), "%s outside user32", n[i]);
        CHECKF(is_exec((void *)p), "%s not exec", n[i]);
        CHECKF(readable((void *)p, 16), "%s unreadable", n[i]);
    }
    CHECK(t_proc("WINNLSEnableIME") != t_proc("WINNLSGetEnableStatus") || !t_proc("WINNLSEnableIME"));
}
TEST(gap6_winnls_export_order) {
    /* Both thunks are 14 bytes (mov rax,[rip+x]; jmp [rip+y]) on Win10 x64. */
    const unsigned char *p = (const unsigned char *)t_proc("WINNLSEnableIME");
    REQUIRE(p);
    if (readable(p, 14)) {
        if (p[0] == 0x48 && p[1] == 0x8b && p[2] == 0x05) {
            CHECK(p[7] == 0x48 && p[8] == 0xff && p[9] == 0x25);
        } else t_note("thunk bytes differ: %02x %02x %02x", p[0], p[1], p[2]);
    }
}

/* WaitForRedirectionStartComplete: no documented arguments, forwards to a
 * win32k call that may wait on session redirection. Not called. */
TEST(gap6_waitforredirectionstartcomplete_resolve) {
    FARPROC p = t_proc("WaitForRedirectionStartComplete");
    REQUIRE(p);
    CHECK(in_user32((void *)p));
    CHECK(is_exec((void *)p));
}
