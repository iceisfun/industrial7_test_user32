/* gap2: export presence for undocumented / privileged entry points, IMP* legacy
 * IME exports, input injection error paths, and LPK hook registration.
 * Injection functions are only ever called with deliberately invalid arguments
 * (NULL buffers / NULL devices) so that nothing can reach the input stream. */
#include "gap2.h"

static const char *const all_exports[] = {
    "GetWindowBand", "GetWindowCompositionAttribute", "GetWindowCompositionInfo",
    "GetWindowDpiHostingBehavior", "GetWindowMinimizeRect", "GetWindowProcessHandle",
    "GetWindowRgnEx", "GhostWindowFromHungWindow", "HandleDelegatedInput",
    "HungWindowFromGhostWindow", "IMPGetIMEA", "IMPGetIMEW", "IMPQueryIMEA", "IMPQueryIMEW",
    "IMPSetIMEA", "IMPSetIMEW", "InheritWindowMonitor", "InitDManipHook",
    "InitializeGenericHidInjection", "InitializeInputDeviceInjection", "InitializeLpkHooks",
    "InitializePointerDeviceInjection", "InitializePointerDeviceInjectionEx", "InjectDeviceInput",
    "InjectGenericHidInput", "InjectKeyboardInput", "InjectMouseInput", "InjectPointerInput",
    "InjectSyntheticPointerInput", "InjectTouchInput", "InputSpaceRegionFromPoint",
    "InternalGetWindowIcon", "IsOneCoreTransformMode", "IsQueueAttached", "IsSETEnabled",
    "IsServerSideWindow", "IsThreadMessageQueueAttached", "IsThreadTSFEventAware",
    "IsWindowInDestroy", "IsWindowRedirectedForPrint",
};
#define NEXP ((int)(sizeof all_exports / sizeof all_exports[0]))

TEST(gap2_exports_all_present) {
    for (int i = 0; i < NEXP; i++) CHECKF(t_proc(all_exports[i]) != NULL, "missing export %s", all_exports[i]);
}
TEST(gap2_exports_in_user32_image) {
    HMODULE u = t_user32();
    for (int i = 0; i < NEXP; i++) {
        FARPROC p = t_proc(all_exports[i]);
        if (!p) continue;
        HMODULE owner = NULL;
        BOOL ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
        CHECKF(ok && owner == u, "%s resolves outside user32 (owner %p)", all_exports[i], owner);
    }
}
TEST(gap2_exports_distinct_addresses) {
    int checked = 0;
    for (int i = 0; i < NEXP; i++) {
        for (int j = i + 1; j < NEXP; j++) {
            FARPROC a = t_proc(all_exports[i]), b = t_proc(all_exports[j]);
            if (!a || !b) continue;
            /* The only known alias pair is InjectPointerInput / InjectSyntheticPointerInput. */
            int alias = (!strcmp(all_exports[i], "InjectPointerInput") && !strcmp(all_exports[j], "InjectSyntheticPointerInput"));
            if (alias) CHECK(a == b);
            else CHECKF(a != b, "%s and %s share an address", all_exports[i], all_exports[j]);
            checked++;
        }
    }
    CHECK(checked > 0);
}
TEST(gap2_exports_lookup_case_sensitive) {
    HMODULE u = t_user32();
    CHECK(GetProcAddress(u, "getwindowband") == NULL);
    CHECK(GetProcAddress(u, "ISSERVERSIDEWINDOW") == NULL);
    CHECK(GetProcAddress(u, "InjectTouchInputA") == NULL);
    CHECK(GetProcAddress(u, "InjectTouchInputW") == NULL);
    CHECK(GetProcAddress(u, "IsWindowInDestroyA") == NULL);
    CHECK(GetProcAddress(u, "GetWindowBandW") == NULL);
    CHECK(GetProcAddress(u, "InternalGetWindowIconW") == NULL);
}
TEST(gap2_exports_repeat_resolution_stable) {
    for (int i = 0; i < NEXP; i++) {
        FARPROC a = t_proc(all_exports[i]);
        for (int k = 0; k < 10; k++) CHECKF(t_proc(all_exports[i]) == a, "%s unstable", all_exports[i]);
    }
}
TEST(gap2_exports_not_forwarded_strings) {
    /* An export forwarder points inside the export directory; a real function does not. */
    HMODULE u = t_user32();
    BYTE *base = (BYTE *)u;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    for (int i = 0; i < NEXP; i++) {
        FARPROC p = t_proc(all_exports[i]);
        if (!p) continue;
        DWORD rva = (DWORD)((BYTE *)p - base);
        CHECKF(!(rva >= dd.VirtualAddress && rva < dd.VirtualAddress + dd.Size), "%s is a forwarder", all_exports[i]);
    }
}

/* ----------------------------------------------------- legacy IME (IMP*) exports */
static const char *const imp_names[] = { "IMPGetIMEA", "IMPGetIMEW", "IMPQueryIMEA", "IMPQueryIMEW", "IMPSetIMEA", "IMPSetIMEW" };
TEST(gap2_imp_resolve_pairs) {
    for (int i = 0; i < 6; i += 2) {
        FARPROC a = t_proc(imp_names[i]), w = t_proc(imp_names[i + 1]);
        CHECKF(a != NULL, "%s", imp_names[i]);
        CHECKF(w != NULL, "%s", imp_names[i + 1]);
        if (a && w) CHECKF(a != w, "%s and %s must be separate entry points", imp_names[i], imp_names[i + 1]);
    }
}
TEST(gap2_imp_all_distinct) {
    for (int i = 0; i < 6; i++)
        for (int j = i + 1; j < 6; j++) {
            FARPROC a = t_proc(imp_names[i]), b = t_proc(imp_names[j]);
            if (a && b) CHECKF(a != b, "%s == %s", imp_names[i], imp_names[j]);
        }
}
TEST(gap2_imp_no_bare_names) {
    HMODULE u = t_user32();
    CHECK(GetProcAddress(u, "IMPGetIME") == NULL);
    CHECK(GetProcAddress(u, "IMPQueryIME") == NULL);
    CHECK(GetProcAddress(u, "IMPSetIME") == NULL);
}
TEST(gap2_imp_code_is_executable) {
    /* The stubs are tiny trampolines that live in user32's text; just check the pages are executable code. */
    for (int i = 0; i < 6; i++) {
        FARPROC p = t_proc(imp_names[i]);
        if (!p) continue;
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery((void *)p, &mbi, sizeof mbi) == sizeof mbi);
        CHECKF(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY), "%s not executable (protect 0x%lx)", imp_names[i], mbi.Protect);
        CHECK(mbi.State == MEM_COMMIT);
        CHECK(mbi.Type == MEM_IMAGE);
    }
}
/* IMPGetIME/IMPQueryIME/IMPSetIME forward through a function pointer filled in
 * by the IME client library; they are deliberately never called here. */

/* --------------------------------------------------------- resolution-only set */
static const char *const resolve_only[] = {
    "InitDManipHook", "InitializeGenericHidInjection", "InitializeInputDeviceInjection",
    "InitializePointerDeviceInjection", "InitializePointerDeviceInjectionEx", "InjectDeviceInput",
    "InjectGenericHidInput", "InjectKeyboardInput", "InjectMouseInput", "InputSpaceRegionFromPoint",
    "HandleDelegatedInput",
};
TEST(gap2_internal_resolve_only) {
    /* Undocumented / privileged: signatures are not published, and calling them could register
     * devices or inject input, so these are only resolved and inspected. */
    for (unsigned i = 0; i < sizeof resolve_only / sizeof resolve_only[0]; i++) {
        FARPROC p = t_proc(resolve_only[i]);
        CHECKF(p != NULL, "%s", resolve_only[i]);
        if (!p) continue;
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery((void *)p, &mbi, sizeof mbi) == sizeof mbi);
        CHECKF(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY), "%s not executable", resolve_only[i]);
    }
}
TEST(gap2_internal_stub_size_reasonable) {
    /* Kernel-call thunks are a few bytes; make sure the bytes at the entry decode as something other than int3 padding. */
    for (unsigned i = 0; i < sizeof resolve_only / sizeof resolve_only[0]; i++) {
        BYTE *p = (BYTE *)t_proc(resolve_only[i]);
        if (!p) continue;
        CHECKF(p[0] != 0xCC && p[0] != 0x00, "%s begins with 0x%02x", resolve_only[i], p[0]);
    }
}

/* ------------------------------------------------------- InitializeLpkHooks */
/* The table holds four function pointers (TabbedTextOut, PSMTextOut, DrawTextEx, EditControl);
 * a non-NULL entry would redirect this process's text output, so only an all-NULL table is passed. */
TEST(gap2_lpk_null_table_entries) {
    void (WINAPI *f)(void **) = (void *)t_proc("InitializeLpkHooks"); REQUIRE(f);
    void *tbl[4] = { NULL, NULL, NULL, NULL };
    f(tbl);
    CHECK(tbl[0] == NULL && tbl[1] == NULL && tbl[2] == NULL && tbl[3] == NULL); /* table is read-only input */
}
TEST(gap2_lpk_repeat) {
    void (WINAPI *f)(void **) = (void *)t_proc("InitializeLpkHooks"); REQUIRE(f);
    void *tbl[4] = { 0 };
    for (int i = 0; i < 1000; i++) f(tbl);
    CHECK(tbl[0] == NULL);
}
TEST(gap2_lpk_oversized_table) {
    void (WINAPI *f)(void **) = (void *)t_proc("InitializeLpkHooks"); REQUIRE(f);
    void *tbl[16] = { 0 };
    f(tbl);
    for (int i = 0; i < 16; i++) CHECK(tbl[i] == NULL);
}
TEST(gap2_lpk_text_output_unaffected) {
    void (WINAPI *f)(void **) = (void *)t_proc("InitializeLpkHooks"); REQUIRE(f);
    int (WINAPI *dt)(HDC, LPCSTR, int, LPRECT, UINT) = (void *)t_proc("DrawTextA");
    LONG (WINAPI *tto)(HDC, int, int, LPCSTR, int, int, const INT *, int) = (void *)t_proc("TabbedTextOutA");
    REQUIRE(dt); REQUIRE(tto);
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bm = CreateCompatibleBitmap(GetDC(NULL), 100, 40);
    HGDIOBJ old = SelectObject(dc, bm);
    RECT rc = { 0, 0, 100, 40 };
    int h1 = dt(dc, "Hello", -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE);
    LONG t1 = tto(dc, 0, 0, "A\tB", 3, 0, NULL, 0);
    void *tbl[4] = { 0 };
    f(tbl);
    RECT rc2 = { 0, 0, 100, 40 };
    int h2 = dt(dc, "Hello", -1, &rc2, DT_LEFT | DT_TOP | DT_SINGLELINE);
    LONG t2 = tto(dc, 0, 0, "A\tB", 3, 0, NULL, 0);
    CHECK(h1 == h2 && h1 > 0);
    CHECK(t1 == t2 && t1 != 0);
    SelectObject(dc, old); DeleteObject(bm); DeleteDC(dc);
}

/* ------------------------------------------------------ InjectTouchInput errors */
#define INJTOUCH() ((BOOL (WINAPI *)(UINT32, const void *))t_proc("InjectTouchInput"))
TEST(gap2_touch_null_contacts) {
    BOOL (WINAPI *f)(UINT32, const void *) = INJTOUCH(); REQUIRE(f);
    static const UINT32 counts[] = { 0, 1, 2, 10, 256, 257, 1000, 0x7fffffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = f(counts[i], NULL);
        CHECKF(r == FALSE, "count %u with NULL contacts returned %d", counts[i], r);
        DWORD e = GetLastError();
        if (e == 0xdeadbeef) t_note("count %u: last error untouched", counts[i]);
        else t_note("InjectTouchInput(%u, NULL) error %lu", counts[i], e);
    }
}
TEST(gap2_touch_zero_count) {
    BOOL (WINAPI *f)(UINT32, const void *) = INJTOUCH(); REQUIRE(f);
    BYTE buf[256]; memset(buf, 0, sizeof buf);
    SetLastError(0xdeadbeef);
    BOOL r = f(0, buf);
    t_note("InjectTouchInput(0, buf) -> %d err=%lu", r, GetLastError());
    CHECK(r == FALSE || r == TRUE);
    /* a zero-count call must not alter the caller's buffer */
    int dirty = 0;
    for (int i = 0; i < 256; i++) if (buf[i]) dirty = 1;
    CHECK(!dirty);
}
TEST(gap2_touch_repeat_failure_stable) {
    BOOL (WINAPI *f)(UINT32, const void *) = INJTOUCH(); REQUIRE(f);
    for (int i = 0; i < 500; i++) { if (f(1, NULL) != FALSE) { CHECK(0); break; } }
    DWORD e1, e2;
    SetLastError(0); f(1, NULL); e1 = GetLastError();
    SetLastError(0); f(1, NULL); e2 = GetLastError();
    CHECK(e1 == e2);
}
static DWORD WINAPI touch_thread(void *a) {
    BOOL (WINAPI *f)(UINT32, const void *) = INJTOUCH();
    SetLastError(0xdeadbeef);
    BOOL r = f(1, NULL);
    return r == FALSE ? GetLastError() : 0xffffffff;
}
TEST(gap2_touch_other_thread_failure) {
    REQUIRE(INJTOUCH());
    DWORD e = g2_run_thread(touch_thread, NULL);
    CHECK(e != 0xdead0001 && e != 0xffffffff);
    t_note("InjectTouchInput(1, NULL) on worker thread: error %lu", e);
}

/* ---------------------------------------------- InjectSyntheticPointerInput errors */
#define INJSYN() ((BOOL (WINAPI *)(HANDLE, const void *, UINT32))t_proc("InjectSyntheticPointerInput"))
TEST(gap2_synptr_null_device) {
    BOOL (WINAPI *f)(HANDLE, const void *, UINT32) = INJSYN(); REQUIRE(f);
    static const UINT32 counts[] = { 0, 1, 2, 256, 0xffffffff };
    for (unsigned i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = f(NULL, NULL, counts[i]);
        CHECKF(r == FALSE, "NULL device, NULL info, count %u returned %d", counts[i], r);
        t_note("InjectSyntheticPointerInput(NULL,NULL,%u) error %lu", counts[i], GetLastError());
    }
}
TEST(gap2_synptr_bogus_devices) {
    BOOL (WINAPI *f)(HANDLE, const void *, UINT32) = INJSYN(); REQUIRE(f);
    static const ULONG_PTR devs[] = { 1, 2, 4, 0x1234, 0xdeadbeef, (ULONG_PTR)-1, (ULONG_PTR)-2, 0x7fffffff };
    for (unsigned i = 0; i < sizeof devs / sizeof devs[0]; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = f((HANDLE)devs[i], NULL, 1);
        CHECKF(r == FALSE, "device %p returned %d", (void *)devs[i], r);
        DWORD e = GetLastError();
        t_note("InjectSyntheticPointerInput(%p,NULL,1) error %lu", (void *)devs[i], e);
        SetLastError(0xdeadbeef);
        CHECKF(f((HANDLE)devs[i], NULL, 0) == FALSE, "device %p count 0", (void *)devs[i]);
    }
}
TEST(gap2_synptr_alias_matches_injectpointer) {
    FARPROC a = t_proc("InjectPointerInput"), b = t_proc("InjectSyntheticPointerInput");
    REQUIRE(a); REQUIRE(b);
    CHECK(a == b);
    BOOL (WINAPI *f)(HANDLE, const void *, UINT32) = (void *)a;
    SetLastError(0xdeadbeef);
    CHECK(f(NULL, NULL, 0) == FALSE);
    DWORD e1 = GetLastError();
    SetLastError(0xdeadbeef);
    CHECK(((BOOL (WINAPI *)(HANDLE, const void *, UINT32))b)(NULL, NULL, 0) == FALSE);
    CHECK(GetLastError() == e1);
}
TEST(gap2_synptr_after_destroyed_device) {
    /* A device handle that has been destroyed must not be accepted any more. Creating a pointer
     * device is not done here, so use the closest stand-in: a stale kernel-object style handle. */
    BOOL (WINAPI *f)(HANDLE, const void *, UINT32) = INJSYN(); REQUIRE(f);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(ev != NULL);
    SetLastError(0xdeadbeef);
    BOOL r = f(ev, NULL, 1); /* an event handle is not a synthetic pointer device */
    CHECK(r == FALSE);
    t_note("InjectSyntheticPointerInput(event handle) error %lu", GetLastError());
    CloseHandle(ev);
    SetLastError(0xdeadbeef);
    CHECK(f(ev, NULL, 1) == FALSE); /* closed */
}
TEST(gap2_synptr_wrong_object_types) {
    BOOL (WINAPI *f)(HANDLE, const void *, UINT32) = INJSYN(); REQUIRE(f);
    HANDLE objs[] = { GetCurrentProcess(), GetCurrentThread(), GetModuleHandleA(NULL), GetProcessHeap(), (HANDLE)GetDesktopWindow() };
    for (unsigned i = 0; i < sizeof objs / sizeof objs[0]; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = f(objs[i], NULL, 1);
        CHECKF(r == FALSE, "object %u returned %d", i, r);
    }
}
static DWORD WINAPI synptr_thread(void *a) {
    BOOL (WINAPI *f)(HANDLE, const void *, UINT32) = INJSYN();
    return f(NULL, NULL, 1) ? 1 : 0;
}
TEST(gap2_synptr_other_thread) {
    REQUIRE(INJSYN());
    CHECK(g2_run_thread(synptr_thread, NULL) == 0);
}

/* -------------------------------------------------- consistency with documented siblings */
TEST(gap2_touch_vs_initialize_touch_injection) {
    BOOL (WINAPI *f)(UINT32, const void *) = INJTOUCH(); REQUIRE(f);
    /* Without InitializeTouchInjection having been called by this process the injection stream does not exist;
     * InitializeTouchInjection itself is exercised by another area and is intentionally not called here. */
    SetLastError(0xdeadbeef);
    CHECK(f(1, NULL) == FALSE);
    CHECK(f(0, NULL) == FALSE);
}
TEST(gap2_inject_names_not_in_other_modules) {
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    CHECK(GetProcAddress(k32, "InjectTouchInput") == NULL);
    CHECK(GetProcAddress(k32, "InjectSyntheticPointerInput") == NULL);
    CHECK(GetProcAddress(k32, "IsWindowInDestroy") == NULL);
}
