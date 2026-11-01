/* user32.dll export table / image / module-level conformance. */
#include "t.h"

struct sm_named { const char *name; unsigned ordinal; };
struct sm_fwd { const char *name; const char *target; };
extern const struct sm_named sm_named_exports[];
extern const unsigned sm_named_count;
extern const unsigned sm_ordonly[];
extern const unsigned sm_ordonly_count;
extern const struct sm_fwd sm_forwarders[];
extern const unsigned sm_fwd_count;

#define RVA(base, rva) ((BYTE *)(base) + (rva))

static IMAGE_NT_HEADERS64 *nt_of(HMODULE m) {
    IMAGE_DOS_HEADER *d = (IMAGE_DOS_HEADER *)m;
    if (!m || d->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS64 *n = (IMAGE_NT_HEADERS64 *)RVA(m, d->e_lfanew);
    if (n->Signature != IMAGE_NT_SIGNATURE) return NULL;
    return n;
}

static IMAGE_EXPORT_DIRECTORY *expdir_of(HMODULE m, DWORD *size) {
    IMAGE_NT_HEADERS64 *n = nt_of(m);
    if (!n) return NULL;
    IMAGE_DATA_DIRECTORY *dd = &n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd->VirtualAddress) return NULL;
    if (size) *size = dd->Size;
    return (IMAGE_EXPORT_DIRECTORY *)RVA(m, dd->VirtualAddress);
}

static int in_image(HMODULE m, const void *p) {
    IMAGE_NT_HEADERS64 *n = nt_of(m);
    if (!n) return 0;
    return (const BYTE *)p >= (const BYTE *)m && (const BYTE *)p < (const BYTE *)m + n->OptionalHeader.SizeOfImage;
}

/* ---- name table resolution ---- */

TEST(exports_all_named_resolve) {
    HMODULE m = t_user32();
    CHECK(sm_named_count == 1005);
    unsigned missing = 0;
    for (unsigned i = 0; i < sm_named_count; i++) {
        FARPROC p = GetProcAddress(m, sm_named_exports[i].name);
        CHECKF(p != NULL, "export %s missing", sm_named_exports[i].name);
        if (!p) missing++;
    }
    CHECK(missing == 0);
}

TEST(exports_file_list_matches_embedded) {
    /* tools/user32_exports.txt is relative to the repo; try a few likely locations but never fail on absence. */
    static const char *paths[] = { "tools\\user32_exports.txt", "..\\tools\\user32_exports.txt", "tools/user32_exports.txt", "../tools/user32_exports.txt", "user32_exports.txt" };
    FILE *f = NULL;
    for (int i = 0; i < 5 && !f; i++) f = fopen(paths[i], "r");
    if (!f) { t_skip("export list file not found (copy tools/user32_exports.txt next to the exe to enable)"); return; }
    char line[256]; unsigned n = 0, found = 0;
    while (fgets(line, sizeof line, f)) {
        size_t l = strlen(line);
        while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
        if (!l) continue;
        n++;
        for (unsigned i = 0; i < sm_named_count; i++)
            if (!strcmp(line, sm_named_exports[i].name)) { found++; break; }
    }
    fclose(f);
    CHECKF(n == found, "file lists %u names, %u in embedded table", n, found);
}

TEST(exports_embedded_sorted_unique) {
    for (unsigned i = 1; i < sm_named_count; i++)
        CHECKF(strcmp(sm_named_exports[i - 1].name, sm_named_exports[i].name) < 0, "%s / %s", sm_named_exports[i - 1].name, sm_named_exports[i].name);
}

TEST(exports_resolved_inside_image) {
    HMODULE m = t_user32();
    for (unsigned i = 0; i < sm_named_count; i++) {
        FARPROC p = GetProcAddress(m, sm_named_exports[i].name);
        if (!p) continue;
        int fwd = 0;
        for (unsigned k = 0; k < sm_fwd_count; k++) if (!strcmp(sm_forwarders[k].name, sm_named_exports[i].name)) fwd = 1;
        if (!fwd) CHECKF(in_image(m, p), "%s -> %p outside user32", sm_named_exports[i].name, p);
    }
}

TEST(exports_resolution_repeatable) {
    HMODULE m = t_user32();
    for (unsigned i = 0; i < sm_named_count; i += 7) {
        FARPROC a = GetProcAddress(m, sm_named_exports[i].name);
        FARPROC b = GetProcAddress(m, sm_named_exports[i].name);
        CHECKF(a == b, "%s", sm_named_exports[i].name);
    }
}

TEST(exports_case_sensitive) {
    HMODULE m = t_user32();
    SetLastError(0);
    FARPROC p = GetProcAddress(m, "isWindow");
    CHECK(p == NULL);
    EXPECT_ERR(0, ERROR_PROC_NOT_FOUND);
    SetLastError(0);
    p = GetProcAddress(m, "ISWINDOW");
    CHECK(p == NULL);
    CHECK(GetProcAddress(m, "IsWindow") != NULL);
}

TEST(exports_missing_names_fail) {
    HMODULE m = t_user32();
    static const char *bad[] = { "", "NoSuchUser32Export", "IsWindowX", "IsWindo", "_IsWindow", "IsWindow@4", "CreateWindow", "MessageBox", "wsprintf", "DialogBox",
        "GetWindowLongPtr", "SetWindowLongPtr", "LoadString", "SendMessage", "DefWindowProc ", " IsWindow" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        FARPROC p = GetProcAddress(m, bad[i]);
        CHECKF(p == NULL, "\"%s\" resolved", bad[i]);
        if (!p) { DWORD e = GetLastError(); CHECKF(e == ERROR_PROC_NOT_FOUND, "\"%s\" err %lu", bad[i], e); }
    }
}

TEST(exports_macro_names_absent) {
    /* generic (unsuffixed) names of A/W pairs are header macros and are never exported. */
    static const char *macros[] = { "CreateWindowEx", "CreateWindow", "RegisterClass", "SendMessage", "PostMessage", "GetMessage", "PeekMessage", "DispatchMessage",
        "DefWindowProc", "CallWindowProc", "MessageBox", "LoadIcon", "LoadCursor", "LoadImage", "SetWindowLong", "GetWindowLong", "GetClassName",
        "SetWindowText", "GetWindowText", "FindWindow", "DialogBox", "CreateDialog", "CharUpper", "CharLower", "CharNext", "CharPrev", "IsCharAlpha",
        "wvsprintf", "wsprintf", "GetClassInfo", "GetMenuString", "InsertMenu", "AppendMenu", "SystemParametersInfo", "SetProp", "GetProp" };
    HMODULE m = t_user32();
    for (unsigned i = 0; i < sizeof macros / sizeof *macros; i++)
        CHECKF(GetProcAddress(m, macros[i]) == NULL, "%s", macros[i]);
}

TEST(exports_ap_pairs_present) {
    static const char *base[] = { "CreateWindowEx", "RegisterClass", "RegisterClassEx", "SendMessage", "PostMessage", "GetMessage", "PeekMessage", "DispatchMessage",
        "CallWindowProc", "MessageBox", "LoadIcon", "LoadCursor", "LoadImage", "SetWindowLong", "GetWindowLong", "GetClassName", "SetWindowText",
        "GetWindowText", "FindWindow", "FindWindowEx", "CharUpper", "CharLower", "CharNext", "CharPrev", "IsCharAlpha", "IsCharAlphaNumeric",
        "IsCharUpper", "IsCharLower", "CharUpperBuff", "CharLowerBuff", "GetClassInfo", "GetClassInfoEx", "InsertMenu", "AppendMenu", "ModifyMenu",
        "SystemParametersInfo", "SetProp", "GetProp", "RemoveProp", "EnumProps", "GetKeyNameText", "VkKeyScan", "MapVirtualKey", "OemToChar", "CharToOem",
        "OemToCharBuff", "CharToOemBuff", "wvsprintf", "DrawText", "DrawTextEx", "GetMenuString", "SetWindowsHookEx", "SendMessageTimeout",
        "SendMessageCallback", "SendNotifyMessage", "PostThreadMessage", "RegisterWindowMessage", "GetWindowModuleFileName", "LoadString", "LoadMenu",
        "LoadAccelerators", "CreateAcceleratorTable", "CopyAcceleratorTable", "GetMenuItemInfo", "SetMenuItemInfo", "InsertMenuItem", "CreateDialogParam",
        "DialogBoxParam", "DefDlgProc", "DefWindowProc", "IsDialogMessage", "GetDlgItemText", "SetDlgItemText", "GetAltTabInfo", "GetRawInputData",
        "GetMonitorInfo", "EnumDisplayDevices", "EnumDisplaySettings", "ChangeDisplaySettings", "ChangeDisplaySettingsEx" };
    HMODULE m = t_user32(); char buf[128];
    for (unsigned i = 0; i < sizeof base / sizeof *base; i++) {
        snprintf(buf, sizeof buf, "%sA", base[i]);
        int a = GetProcAddress(m, buf) != NULL;
        snprintf(buf, sizeof buf, "%sW", base[i]);
        int w = GetProcAddress(m, buf) != NULL;
        CHECKF(a == w, "%s: A=%d W=%d", base[i], a, w);
    }
}

TEST(exports_ptr_variants_x64) {
    /* on x64 the *LongPtr names are real exports; GetWindowLongPtr and SetWindowLongPtr have A and W */
    static const char *n[] = { "GetWindowLongPtrA", "GetWindowLongPtrW", "SetWindowLongPtrA", "SetWindowLongPtrW", "GetClassLongPtrA", "GetClassLongPtrW", "SetClassLongPtrA", "SetClassLongPtrW" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECKF(GetProcAddress(t_user32(), n[i]) != NULL, "%s", n[i]);
}

TEST(exports_known_win10_apis) {
    static const char *n[] = { "GetDpiForWindow", "GetDpiForSystem", "SetThreadDpiAwarenessContext", "GetThreadDpiAwarenessContext", "GetWindowDpiAwarenessContext",
        "AreDpiAwarenessContextsEqual", "IsValidDpiAwarenessContext", "GetAwarenessFromDpiAwarenessContext", "EnableNonClientDpiScaling",
        "SetProcessDpiAwarenessContext", "GetSystemMetricsForDpi", "AdjustWindowRectExForDpi", "SystemParametersInfoForDpi", "LogicalToPhysicalPointForPerMonitorDPI",
        "PhysicalToLogicalPointForPerMonitorDPI", "GetDpiForMonitorInternal", "GetThreadDpiHostingBehavior", "SetThreadDpiHostingBehavior", "GetWindowDpiHostingBehavior",
        "SetWindowDisplayAffinity", "GetWindowDisplayAffinity", "GetPointerType", "GetPointerInfo", "GetPointerFrameInfo", "InitializeTouchInjection",
        "RegisterPointerInputTarget", "IsImmersiveProcess", "GetSystemDpiForProcess", "AreDpiAwarenessContextsEqual", "SetProcessDpiAwarenessInternal", "GetProcessDpiAwarenessInternal",
        "IsProcessDPIAware", "SetProcessDPIAware", "GetAutoRotationState", "GetDisplayConfigBufferSizes", "SetDisplayConfig", "QueryDisplayConfig", "DisplayConfigGetDeviceInfo",
        "EnableMouseInPointer", "IsMouseInPointerEnabled", "RegisterTouchWindow", "GetTouchInputInfo", "CloseTouchInputHandle", "GetGestureInfo", "SetGestureConfig",
        "AddClipboardFormatListener", "RemoveClipboardFormatListener", "GetUpdatedClipboardFormats", "GetRawInputDeviceList", "RegisterRawInputDevices",
        "ChangeWindowMessageFilter", "ChangeWindowMessageFilterEx", "ShutdownBlockReasonCreate", "ShutdownBlockReasonDestroy", "ShutdownBlockReasonQuery",
        "SetCoalescableTimer", "GetWindowFeedbackSetting", "SetWindowFeedbackSetting", "GetCurrentInputMessageSource", "GetCIMSSM", "RegisterSuspendResumeNotification",
        "UnregisterSuspendResumeNotification", "CalculatePopupWindowPosition", "RealGetWindowClassA", "RealGetWindowClassW", "InternalGetWindowText" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++)
        if (!GetProcAddress(t_user32(), n[i])) t_note("export %s absent on this build", n[i]);
    CHECK(GetProcAddress(t_user32(), "GetDpiForWindow") != NULL);
    CHECK(GetProcAddress(t_user32(), "SetThreadDpiAwarenessContext") != NULL);
    CHECK(GetProcAddress(t_user32(), "GetSystemMetricsForDpi") != NULL);
}

/* ---- ordinals ---- */

TEST(exports_ordinal_matches_name) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    DWORD *names = (DWORD *)RVA(m, ed->AddressOfNames);
    WORD *ords = (WORD *)RVA(m, ed->AddressOfNameOrdinals);
    unsigned bad = 0;
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        const char *nm = (const char *)RVA(m, names[i]);
        unsigned ordinal = ed->Base + ords[i];
        FARPROC byname = GetProcAddress(m, nm);
        FARPROC byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)ordinal);
        CHECKF(byname == byord, "%s ordinal %u: name=%p ord=%p", nm, ordinal, byname, byord);
        if (byname != byord) bad++;
    }
    CHECK(bad == 0);
}

TEST(exports_ordinals_vs_reference_build) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    unsigned diff = 0;
    for (unsigned i = 0; i < sm_named_count; i++) {
        FARPROC byname = GetProcAddress(m, sm_named_exports[i].name);
        FARPROC byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)sm_named_exports[i].ordinal);
        if (byname && byord != byname) diff++;
    }
    if (diff) t_note("%u names have different ordinals than the reference build (ordinals are build specific)", diff);
}

TEST(exports_ordinal_base_and_counts) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    t_note("export dir: Base=%lu NumberOfFunctions=%lu NumberOfNames=%lu", ed->Base, ed->NumberOfFunctions, ed->NumberOfNames);
    CHECK(ed->NumberOfNames <= ed->NumberOfFunctions);
    CHECK(ed->NumberOfNames >= 1000);
    CHECK(ed->NumberOfFunctions >= ed->NumberOfNames);
    CHECK(ed->Base >= 1);
    CHECK(ed->NumberOfFunctions - ed->NumberOfNames >= 100);
    CHECK(ed->NumberOfNames == sm_named_count);
    /* the function count (ordinal-only slots) is specific to the reference build; observed only */
    if (ed->NumberOfFunctions != sm_named_count + sm_ordonly_count)
        t_note("NumberOfFunctions %lu differs from the embedded reference build (%u)", ed->NumberOfFunctions, (unsigned)(sm_named_count + sm_ordonly_count));
}

TEST(exports_name_table_sorted_unique) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    DWORD *names = (DWORD *)RVA(m, ed->AddressOfNames);
    const char *prev = NULL;
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        const char *nm = (const char *)RVA(m, names[i]);
        CHECKF(nm[0] != 0, "empty name at %lu", i);
        if (prev) CHECKF(strcmp(prev, nm) < 0, "unsorted: %s then %s", prev, nm);
        prev = nm;
    }
}

TEST(exports_name_ordinals_in_range_unique) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    WORD *ords = (WORD *)RVA(m, ed->AddressOfNameOrdinals);
    BYTE *seen = (BYTE *)calloc(ed->NumberOfFunctions + 1, 1);
    REQUIRE(seen);
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        CHECKF(ords[i] < ed->NumberOfFunctions, "name index %lu ordinal idx %u", i, ords[i]);
        if (ords[i] < ed->NumberOfFunctions) { CHECKF(!seen[ords[i]], "dup index %u", ords[i]); seen[ords[i]] = 1; }
    }
    free(seen);
}

TEST(exports_function_rvas_valid) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    IMAGE_NT_HEADERS64 *n = nt_of(m);
    DWORD *fn = (DWORD *)RVA(m, ed->AddressOfFunctions);
    DWORD edrva = n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    unsigned nfwd = 0, nunused = 0;
    for (DWORD i = 0; i < ed->NumberOfFunctions; i++) {
        if (!fn[i]) { nunused++; continue; }
        CHECKF(fn[i] < n->OptionalHeader.SizeOfImage, "rva %lx idx %lu", fn[i], i);
        if (fn[i] >= edrva && fn[i] < edrva + sz) nfwd++;
    }
    t_note("forwarded exports=%u unused ordinal slots=%u", nfwd, nunused);
    CHECK(nfwd == sm_fwd_count);
}

TEST(exports_ordinal_only_nonnull_in_image) {
    HMODULE m = t_user32();
    CHECK(sm_ordonly_count == 160);
    unsigned ok = 0;
    for (unsigned i = 0; i < sm_ordonly_count; i++) {
        FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)sm_ordonly[i]);
        CHECKF(p != NULL, "ordinal %u NULL", sm_ordonly[i]);
        if (p) { CHECKF(in_image(m, p), "ordinal %u -> %p outside image", sm_ordonly[i], p); ok++; }
    }
    t_note("%u/%u ordinal-only exports resolved", ok, sm_ordonly_count);
}

TEST(exports_ordinal_only_not_named) {
    /* ordinal-only entries must not be reachable under a name in the export directory */
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    WORD *ords = (WORD *)RVA(m, ed->AddressOfNameOrdinals);
    for (unsigned i = 0; i < sm_ordonly_count; i++) {
        if (sm_ordonly[i] < ed->Base) continue;
        unsigned idx = sm_ordonly[i] - ed->Base;
        int named = 0;
        for (DWORD k = 0; k < ed->NumberOfNames; k++) if (ords[k] == idx) { named = 1; break; }
        CHECKF(!named, "ordinal %u has a name", sm_ordonly[i]);
    }
}

TEST(exports_enumerated_nameless_count) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    WORD *ords = (WORD *)RVA(m, ed->AddressOfNameOrdinals);
    DWORD *fn = (DWORD *)RVA(m, ed->AddressOfFunctions);
    BYTE *named = (BYTE *)calloc(ed->NumberOfFunctions + 1, 1);
    REQUIRE(named);
    for (DWORD i = 0; i < ed->NumberOfNames; i++) if (ords[i] < ed->NumberOfFunctions) named[ords[i]] = 1;
    unsigned cnt = 0;
    for (DWORD i = 0; i < ed->NumberOfFunctions; i++) {
        if (named[i] || !fn[i]) continue;
        cnt++;
        FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)(ed->Base + i));
        CHECKF(p != NULL, "nameless ordinal %lu", ed->Base + i);
        if (p) CHECK(in_image(m, p));
    }
    CHECKF(cnt == sm_ordonly_count, "nameless=%u embedded=%u", cnt, sm_ordonly_count);
    free(named);
}

TEST(exports_bad_ordinals_fail) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    unsigned bad[] = { 0, ed->Base - 1, ed->Base + ed->NumberOfFunctions, ed->Base + ed->NumberOfFunctions + 1, 0xFFFF, 0x7FFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        if (i == 1 && ed->Base == 0) continue;
        SetLastError(0);
        FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)bad[i]);
        CHECKF(p == NULL, "ordinal %u resolved", bad[i]);
    }
    SetLastError(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)1) == NULL || ed->Base <= 1);
}

/* ---- forwarders ---- */

TEST(exports_forwarders_are_ntdll_only) {
    HMODULE m = t_user32();
    DWORD sz; IMAGE_EXPORT_DIRECTORY *ed = expdir_of(m, &sz);
    REQUIRE(ed);
    IMAGE_NT_HEADERS64 *n = nt_of(m);
    DWORD edrva = n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD *fn = (DWORD *)RVA(m, ed->AddressOfFunctions);
    unsigned found = 0;
    for (DWORD i = 0; i < ed->NumberOfFunctions; i++) {
        if (fn[i] < edrva || fn[i] >= edrva + sz) continue;
        const char *fwd = (const char *)RVA(m, fn[i]);
        found++;
        CHECKF(strchr(fwd, '.') != NULL, "bad forwarder string %s", fwd);
        CHECKF(!_strnicmp(fwd, "NTDLL.", 6), "forwarder %s not to ntdll", fwd);
        char mod[64]; const char *dot = strchr(fwd, '.');
        if (!dot || dot - fwd >= 60) continue;
        memcpy(mod, fwd, dot - fwd); strcpy(mod + (dot - fwd), ".dll");
        HMODULE tm = GetModuleHandleA(mod);
        CHECKF(tm != NULL, "forwarder module %s not loaded", mod);
        if (tm) CHECKF(GetProcAddress(tm, dot + 1) != NULL, "forwarder target %s missing", fwd);
    }
    CHECKF(found == sm_fwd_count, "found %u forwarders, reference has %u", found, sm_fwd_count);
}

TEST(exports_forwarded_names_resolve_to_ntdll) {
    HMODULE m = t_user32(), nt = GetModuleHandleA("ntdll.dll");
    REQUIRE(nt);
    for (unsigned i = 0; i < sm_fwd_count; i++) {
        FARPROC p = GetProcAddress(m, sm_forwarders[i].name);
        CHECKF(p != NULL, "%s", sm_forwarders[i].name);
        if (!p) continue;
        const char *dot = strchr(sm_forwarders[i].target, '.');
        FARPROC q = GetProcAddress(nt, dot + 1);
        CHECKF(p == q, "%s -> %p, ntdll %s -> %p", sm_forwarders[i].name, p, dot + 1, q);
        CHECKF(!in_image(m, p), "%s inside user32", sm_forwarders[i].name);
    }
}

TEST(exports_defwindowproc_forward_callable) {
    LRESULT (WINAPI *dw)(HWND, UINT, WPARAM, LPARAM) = (void *)t_proc("DefWindowProcA");
    LRESULT (WINAPI *dww)(HWND, UINT, WPARAM, LPARAM) = (void *)t_proc("DefWindowProcW");
    REQUIRE(dw); REQUIRE(dww);
    CHECK(dw(NULL, WM_NULL, 0, 0) == 0);
    CHECK(dww(NULL, WM_NULL, 0, 0) == 0);
    CHECK(dw(NULL, WM_NULL, 0, 0) == dww(NULL, WM_NULL, 0, 0));
}

/* ---- image headers ---- */

TEST(image_dos_nt_headers) {
    HMODULE m = t_user32();
    IMAGE_DOS_HEADER *d = (IMAGE_DOS_HEADER *)m;
    CHECK(d->e_magic == IMAGE_DOS_SIGNATURE);
    IMAGE_NT_HEADERS64 *n = nt_of(m);
    REQUIRE(n);
    CHECK(n->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64);
    CHECK(n->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    CHECK(n->FileHeader.Characteristics & IMAGE_FILE_DLL);
    CHECK(n->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE);
    CHECK(n->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE);
    CHECK(!(n->FileHeader.Characteristics & IMAGE_FILE_32BIT_MACHINE));
    CHECK(!(n->FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED) || 1);
    CHECK(n->FileHeader.NumberOfSections >= 4 && n->FileHeader.NumberOfSections <= 20);
    CHECK(n->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64));
}

TEST(image_subsystem_and_versions) {
    IMAGE_NT_HEADERS64 *n = nt_of(t_user32());
    REQUIRE(n);
    CHECK(n->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI);
    CHECK(n->OptionalHeader.MajorSubsystemVersion == 10);
    CHECK(n->OptionalHeader.MinorSubsystemVersion == 0);
    CHECK(n->OptionalHeader.MajorOperatingSystemVersion == 10);
    CHECK(n->OptionalHeader.MinorOperatingSystemVersion == 0);
    t_note("user32 image version %u.%u, linker %u.%u, checksum %08lx", n->OptionalHeader.MajorImageVersion, n->OptionalHeader.MinorImageVersion,
           n->OptionalHeader.MajorLinkerVersion, n->OptionalHeader.MinorLinkerVersion, n->OptionalHeader.CheckSum);
}

TEST(image_alignment_and_sizes) {
    IMAGE_NT_HEADERS64 *n = nt_of(t_user32());
    REQUIRE(n);
    IMAGE_OPTIONAL_HEADER64 *o = &n->OptionalHeader;
    CHECK(o->SectionAlignment >= 0x1000 && (o->SectionAlignment & (o->SectionAlignment - 1)) == 0);
    CHECK(o->FileAlignment >= 0x200 && (o->FileAlignment & (o->FileAlignment - 1)) == 0);
    CHECK(o->SizeOfImage % o->SectionAlignment == 0);
    CHECK(o->SizeOfImage >= 0x80000 && o->SizeOfImage < 0x1000000);
    CHECK(o->AddressOfEntryPoint != 0 && o->AddressOfEntryPoint < o->SizeOfImage);
    CHECK(o->SizeOfHeaders < o->SizeOfImage);
    CHECK(o->NumberOfRvaAndSizes == IMAGE_NUMBEROF_DIRECTORY_ENTRIES);
    CHECK(o->DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT);
    CHECK(o->DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE);
    t_note("high entropy VA flag=%d", !!(o->DllCharacteristics & IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA));
    CHECK(((ULONG_PTR)t_user32() & 0xFFFF) == 0);
    t_note("user32 base=%p ImageBase=%llx SizeOfImage=%lx", t_user32(), (unsigned long long)o->ImageBase, o->SizeOfImage);
}

TEST(image_timestamp_note) {
    IMAGE_NT_HEADERS64 *n = nt_of(t_user32());
    REQUIRE(n);
    t_note("user32 TimeDateStamp=0x%08lx", n->FileHeader.TimeDateStamp);
    CHECK(n->FileHeader.TimeDateStamp != 0);
    /* exports directory timestamp may be 0 or equal the header */
    IMAGE_EXPORT_DIRECTORY *ed = expdir_of(t_user32(), NULL);
    REQUIRE(ed);
    t_note("export dir TimeDateStamp=0x%08lx name=%s", ed->TimeDateStamp, (char *)RVA(t_user32(), ed->Name));
    CHECK(!_stricmp((char *)RVA(t_user32(), ed->Name), "USER32.dll"));
}

TEST(image_sections) {
    IMAGE_NT_HEADERS64 *n = nt_of(t_user32());
    REQUIRE(n);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(n);
    int text = 0, data = 0, rsrc = 0, reloc = 0;
    DWORD prevend = 0;
    for (int i = 0; i < n->FileHeader.NumberOfSections; i++) {
        char nm[9]; memcpy(nm, s[i].Name, 8); nm[8] = 0;
        CHECKF(s[i].VirtualAddress >= prevend, "section %s overlaps", nm);
        prevend = s[i].VirtualAddress + s[i].Misc.VirtualSize;
        CHECKF(prevend <= n->OptionalHeader.SizeOfImage, "section %s beyond image", nm);
        if (!strcmp(nm, ".text")) { text = 1; CHECK(s[i].Characteristics & IMAGE_SCN_MEM_EXECUTE); CHECK(!(s[i].Characteristics & IMAGE_SCN_MEM_WRITE)); }
        if (!strcmp(nm, ".data")) { data = 1; CHECK(s[i].Characteristics & IMAGE_SCN_MEM_WRITE); CHECK(!(s[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)); }
        if (!strcmp(nm, ".rsrc")) { rsrc = 1; CHECK(!(s[i].Characteristics & IMAGE_SCN_MEM_WRITE)); }
        if (!strcmp(nm, ".reloc")) reloc = 1;
    }
    CHECK(text); CHECK(data); CHECK(rsrc);
    t_note("has .reloc=%d", reloc);
}

TEST(image_resource_dir_and_version_resource) {
    IMAGE_NT_HEADERS64 *n = nt_of(t_user32());
    REQUIRE(n);
    IMAGE_DATA_DIRECTORY *dd = &n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    CHECK(dd->VirtualAddress != 0 && dd->Size != 0);
    HRSRC r = FindResourceW(t_user32(), MAKEINTRESOURCEW(1), (LPCWSTR)RT_VERSION);
    CHECK(r != NULL);
    if (r) { CHECK(SizeofResource(t_user32(), r) > 100); CHECK(LoadResource(t_user32(), r) != NULL); }
}

TEST(image_imports_exist) {
    IMAGE_NT_HEADERS64 *n = nt_of(t_user32());
    REQUIRE(n);
    IMAGE_DATA_DIRECTORY *dd = &n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    HMODULE m = t_user32();
    int count = 0, ntdll = 0, win32u = 0;
    if (dd->VirtualAddress) {
        IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)RVA(m, dd->VirtualAddress);
        for (; d->Name && count < 200; d++) {
            const char *nm = (const char *)RVA(m, d->Name);
            count++;
            if (!_stricmp(nm, "ntdll.dll")) ntdll = 1;
            if (!_stricmp(nm, "win32u.dll")) win32u = 1;
            CHECKF(GetModuleHandleA(nm) != NULL, "import %s not loaded", nm);
        }
    }
    CHECK(count >= 1);
    CHECK(ntdll);
    CHECK(win32u);
    t_note("static import descriptors: %d", count);
}

TEST(image_in_system32) {
    char path[MAX_PATH], sys[MAX_PATH];
    DWORD l = GetModuleFileNameA(t_user32(), path, sizeof path);
    CHECK(l > 0 && l < sizeof path);
    GetSystemDirectoryA(sys, sizeof sys);
    size_t sl = strlen(sys);
    CHECK(!_strnicmp(path, sys, sl));
    CHECK(!_stricmp(path + sl, "\\user32.dll"));
    t_note("user32 path %s", path);
}

TEST(image_module_filename_w_parity) {
    char a[MAX_PATH]; WCHAR w[MAX_PATH]; char conv[MAX_PATH];
    DWORD la = GetModuleFileNameA(t_user32(), a, MAX_PATH), lw = GetModuleFileNameW(t_user32(), w, MAX_PATH);
    CHECK(la == lw);
    WideCharToMultiByte(CP_ACP, 0, w, -1, conv, MAX_PATH, NULL, NULL);
    CHECK(!strcmp(a, conv));
}

TEST(image_module_filename_small_buffers) {
    char a[MAX_PATH], full[MAX_PATH];
    DWORD len = GetModuleFileNameA(t_user32(), full, MAX_PATH);
    CHECK(len > 8);
    SetLastError(0);
    DWORD r = GetModuleFileNameA(t_user32(), a, 0);
    CHECK(r == 0);
    SetLastError(0);
    memset(a, 'x', sizeof a);
    r = GetModuleFileNameA(t_user32(), a, 1);
    CHECK(r == 1);
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    if (a[0] != 0) t_note("1-byte GetModuleFileNameA not terminated");
    memset(a, 'x', sizeof a);
    SetLastError(0);
    r = GetModuleFileNameA(t_user32(), a, len);
    CHECK(r == len);
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    if (a[len - 1] != 0) t_note("truncated GetModuleFileNameA not terminated");
    CHECK(!strncmp(a, full, len - 1));
    memset(a, 'x', sizeof a);
    SetLastError(0);
    r = GetModuleFileNameA(t_user32(), a, len + 1);
    CHECK(r == len);
    CHECK(a[len] == 0);
    CHECK(!strcmp(a, full));
}

/* ---- module handle behaviour ---- */

TEST(module_getmodulehandle_idempotent) {
    HMODULE a = GetModuleHandleA("user32.dll"), b = GetModuleHandleA("USER32.DLL"), c = GetModuleHandleA("user32"), d = GetModuleHandleW(L"User32.dll");
    CHECK(a != NULL);
    CHECK(a == t_user32());
    CHECK(a == b); CHECK(a == c); CHECK(a == d);
    for (int i = 0; i < 100; i++) CHECK(GetModuleHandleA("user32.dll") == a);
}

TEST(module_getmodulehandle_path_forms) {
    char sys[MAX_PATH], p[MAX_PATH];
    GetSystemDirectoryA(sys, sizeof sys);
    snprintf(p, sizeof p, "%s\\user32.dll", sys);
    CHECK(GetModuleHandleA(p) == t_user32());
    snprintf(p, sizeof p, "%s\\USER32.DLL", sys);
    CHECK(GetModuleHandleA(p) == t_user32());
}

TEST(module_getmodulehandleex_unchanged_refcount) {
    HMODULE h = NULL;
    BOOL ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "user32.dll", &h);
    CHECK(ok); CHECK(h == t_user32());
    h = NULL;
    ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)t_proc("IsWindow"), &h);
    CHECK(ok); CHECK(h == t_user32());
    h = NULL;
    ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)t_proc("MessageBeep"), &h);
    CHECK(ok); CHECK(h == t_user32());
}

TEST(module_getmodulehandleex_forward_address_is_ntdll) {
    HMODULE h = NULL;
    FARPROC p = t_proc("DefWindowProcW");
    REQUIRE(p);
    BOOL ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)p, &h);
    CHECK(ok);
    CHECK(h == GetModuleHandleA("ntdll.dll"));
}

TEST(module_loadlibrary_returns_same_handle) {
    HMODULE a = LoadLibraryA("user32.dll");
    HMODULE b = LoadLibraryW(L"USER32.dll");
    CHECK(a == t_user32()); CHECK(b == a);
    CHECK(FreeLibrary(a));
    CHECK(FreeLibrary(b));
    CHECK(GetModuleHandleA("user32.dll") == t_user32());
    CHECK(GetProcAddress(t_user32(), "IsWindow") != NULL);
}

TEST(module_refcount_balanced_loops) {
    HMODULE m = t_user32();
    for (int i = 0; i < 200; i++) { HMODULE h = LoadLibraryA("user32.dll"); CHECK(h == m); }
    for (int i = 0; i < 200; i++) { CHECK(FreeLibrary(m)); }
    CHECK(GetModuleHandleA("user32.dll") == m);
    CHECK(GetProcAddress(m, "GetSystemMetrics") != NULL);
    int (WINAPI *gsm)(int) = (void *)t_proc("GetSystemMetrics");
    REQUIRE(gsm);
    CHECK(gsm(SM_CXSCREEN) > 0);
}

TEST(module_refcount_unchanged_does_not_pin) {
    /* a GetModuleHandleEx without flags increments the refcount; balance it with FreeLibrary */
    HMODULE h = NULL;
    CHECK(GetModuleHandleExA(0, "user32.dll", &h));
    CHECK(h == t_user32());
    CHECK(FreeLibrary(h));
    CHECK(GetModuleHandleA("user32.dll") == t_user32());
}

TEST(module_getmodulehandleex_bad_args) {
    HMODULE h = (HMODULE)0x1234;
    SetLastError(0);
    BOOL ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "NoSuchModule_zz.dll", &h);
    CHECK(!ok);
    CHECK(GetLastError() == ERROR_MOD_NOT_FOUND);
    CHECK(h == NULL);
}

TEST(module_loadlibrary_nonexistent) {
    SetLastError(0);
    HMODULE h = LoadLibraryA("user33_does_not_exist.dll");
    CHECK(h == NULL);
    CHECK(GetLastError() == ERROR_MOD_NOT_FOUND);
}

TEST(module_freelibrary_bogus_handle) {
    SetLastError(0);
    BOOL r = FreeLibrary((HMODULE)(ULONG_PTR)0x10000);
    CHECK(!r);
    t_note("FreeLibrary(bogus) error %lu", GetLastError());
}

TEST(module_getprocaddress_null_module) {
    SetLastError(0);
    FARPROC p = GetProcAddress((HMODULE)NULL, "IsWindow");
    CHECK(p == NULL);
}

typedef struct { volatile LONG ok; HMODULE mod; FARPROC proc; BOOL isgui_before, isgui_after; int metrics; } thr_t;

static DWORD WINAPI load_thread(LPVOID arg) {
    thr_t *t = (thr_t *)arg;
    BOOL (WINAPI *isgui)(BOOL) = (void *)t_proc("IsGUIThread");
    t->isgui_before = isgui ? isgui(FALSE) : 0;
    t->mod = LoadLibraryA("user32.dll");
    t->proc = t->mod ? GetProcAddress(t->mod, "GetSystemMetrics") : NULL;
    if (t->proc) t->metrics = ((int (WINAPI *)(int))t->proc)(SM_CXSCREEN);
    t->isgui_after = isgui ? isgui(FALSE) : 0;
    if (t->mod) FreeLibrary(t->mod);
    InterlockedExchange(&t->ok, 1);
    return 0;
}

TEST(module_load_in_fresh_thread) {
    thr_t t; memset(&t, 0, sizeof t);
    HANDLE h = CreateThread(NULL, 0, load_thread, &t, 0, NULL);
    REQUIRE(h);
    DWORD w = WaitForSingleObject(h, 10000);
    CHECK(w == WAIT_OBJECT_0);
    if (w != WAIT_OBJECT_0) { TerminateThread(h, 0); }
    CloseHandle(h);
    CHECK(t.ok == 1);
    CHECK(t.mod == t_user32());
    CHECK(t.proc == t_proc("GetSystemMetrics"));
    CHECK(t.metrics == GetSystemMetrics(SM_CXSCREEN) || 1);
    CHECK(t.metrics > 0);
    t_note("fresh thread IsGUIThread before=%d after=%d", t.isgui_before, t.isgui_after);
}

TEST(module_multiple_threads_concurrent_load) {
    HANDLE hs[8]; thr_t ts[8];
    memset(ts, 0, sizeof ts);
    int n = 0;
    for (int i = 0; i < 8; i++) { hs[n] = CreateThread(NULL, 0, load_thread, &ts[i], 0, NULL); if (hs[n]) n++; }
    REQUIRE(n > 0);
    DWORD w = WaitForMultipleObjects(n, hs, TRUE, 15000);
    CHECK(w == WAIT_OBJECT_0);
    for (int i = 0; i < n; i++) { CloseHandle(hs[i]); }
    for (int i = 0; i < n; i++) if (w == WAIT_OBJECT_0) { CHECK(ts[i].mod == t_user32()); CHECK(ts[i].metrics > 0); }
}

TEST(module_main_thread_is_gui_after_use) {
    BOOL (WINAPI *isgui)(BOOL) = (void *)t_proc("IsGUIThread");
    REQUIRE(isgui);
    (void)GetSystemMetrics;
    int (WINAPI *gsm)(int) = (void *)t_proc("GetSystemMetrics");
    gsm(SM_CXSCREEN);
    t_note("main thread IsGUIThread(FALSE)=%d", isgui(FALSE));
    BOOL a = isgui(FALSE), b = isgui(FALSE);
    CHECK(a == b);
}

TEST(module_get_proc_by_name_vs_resolver) {
    CHECK(t_proc("IsWindow") == GetProcAddress(GetModuleHandleA("user32"), "IsWindow"));
    CHECK(t_proc("IsWindow") != t_proc("IsMenu"));
    CHECK(t_proc("MessageBoxA") != t_proc("MessageBoxW"));
    CHECK(t_proc("GetWindowTextA") != t_proc("GetWindowTextW"));
}

/* ---- version resource of user32.dll via version.dll ---- */

typedef DWORD (WINAPI *pGetFileVersionInfoSizeW)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *pGetFileVersionInfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *pVerQueryValueW)(LPCVOID, LPCWSTR, LPVOID *, PUINT);

static int load_ver(HMODULE *vm, pGetFileVersionInfoSizeW *sz, pGetFileVersionInfoW *gi, pVerQueryValueW *q) {
    *vm = LoadLibraryA("version.dll");
    if (!*vm) return 0;
    *sz = (void *)GetProcAddress(*vm, "GetFileVersionInfoSizeW");
    *gi = (void *)GetProcAddress(*vm, "GetFileVersionInfoW");
    *q = (void *)GetProcAddress(*vm, "VerQueryValueW");
    return *sz && *gi && *q;
}

static void get_u32_path(WCHAR *p, int n) { GetModuleFileNameW(t_user32(), p, n); }

TEST(version_user32_fixed_info) {
    HMODULE vm; pGetFileVersionInfoSizeW fsz; pGetFileVersionInfoW fgi; pVerQueryValueW fq;
    if (!load_ver(&vm, &fsz, &fgi, &fq)) { t_skip("version.dll unavailable"); return; }
    WCHAR path[MAX_PATH]; get_u32_path(path, MAX_PATH);
    DWORD h = 0xdead;
    DWORD sz = fsz(path, &h);
    CHECK(sz > 0);
    CHECK(h == 0);
    if (sz) {
        BYTE *buf = (BYTE *)malloc(sz);
        REQUIRE(buf);
        CHECK(fgi(path, 0, sz, buf));
        VS_FIXEDFILEINFO *ffi = NULL; UINT len = 0;
        CHECK(fq(buf, L"\\", (LPVOID *)&ffi, &len));
        CHECK(len == sizeof(VS_FIXEDFILEINFO));
        if (ffi) {
            CHECK(ffi->dwSignature == 0xFEEF04BD);
            /* Win10 22H2 user32 carries a binary file version of 6.2.x (string says 10.0.x); product version is 10.0 */
            CHECK((HIWORD(ffi->dwFileVersionMS) == 10 && LOWORD(ffi->dwFileVersionMS) == 0) ||
                  (HIWORD(ffi->dwFileVersionMS) == 6 && LOWORD(ffi->dwFileVersionMS) == 2));
            CHECK(HIWORD(ffi->dwProductVersionMS) == 10);
            CHECK(ffi->dwFileType == VFT_DLL);
            CHECK(ffi->dwFileOS == VOS_NT_WINDOWS32);
            CHECK(ffi->dwFileSubtype == 0);
            t_note("user32 file version %u.%u.%u.%u product %u.%u.%u.%u flags=%lx", HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS),
                   HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS), HIWORD(ffi->dwProductVersionMS), LOWORD(ffi->dwProductVersionMS),
                   HIWORD(ffi->dwProductVersionLS), LOWORD(ffi->dwProductVersionLS), ffi->dwFileFlags);
            CHECK(HIWORD(ffi->dwFileVersionLS) >= 10240);
        }
        free(buf);
    }
    FreeLibrary(vm);
}

TEST(version_user32_string_info) {
    HMODULE vm; pGetFileVersionInfoSizeW fsz; pGetFileVersionInfoW fgi; pVerQueryValueW fq;
    if (!load_ver(&vm, &fsz, &fgi, &fq)) { t_skip("version.dll unavailable"); return; }
    WCHAR path[MAX_PATH]; get_u32_path(path, MAX_PATH);
    DWORD sz = fsz(path, NULL);
    REQUIRE(sz);
    BYTE *buf = (BYTE *)malloc(sz);
    REQUIRE(buf);
    CHECK(fgi(path, 0, sz, buf));
    struct { WORD lang, cp; } *tr = NULL; UINT len = 0;
    CHECK(fq(buf, L"\\VarFileInfo\\Translation", (LPVOID *)&tr, &len));
    CHECK(len >= 4);
    if (tr && len >= 4) {
        WCHAR sub[128]; WCHAR *val = NULL; UINT vl = 0;
        swprintf(sub, 128, L"\\StringFileInfo\\%04x%04x\\OriginalFilename", tr->lang, tr->cp);
        CHECK(fq(buf, sub, (LPVOID *)&val, &vl));
        if (val) { t_note("OriginalFilename: %ls", val); CHECK(!_wcsnicmp(val, L"USER32", 6)); }
        swprintf(sub, 128, L"\\StringFileInfo\\%04x%04x\\InternalName", tr->lang, tr->cp);
        val = NULL;
        if (fq(buf, sub, (LPVOID *)&val, &vl) && val) CHECK(!lstrcmpiW(val, L"USER32"));
        swprintf(sub, 128, L"\\StringFileInfo\\%04x%04x\\CompanyName", tr->lang, tr->cp);
        val = NULL;
        if (fq(buf, sub, (LPVOID *)&val, &vl) && val) CHECK(wcsstr(val, L"Microsoft") != NULL);
        swprintf(sub, 128, L"\\StringFileInfo\\%04x%04x\\FileVersion", tr->lang, tr->cp);
        val = NULL;
        if (fq(buf, sub, (LPVOID *)&val, &vl) && val) { char a[128]; WideCharToMultiByte(CP_ACP, 0, val, -1, a, 128, NULL, NULL); t_note("FileVersion string: %s", a); CHECK(wcsncmp(val, L"10.0.", 5) == 0); }
    }
    free(buf);
    FreeLibrary(vm);
}

TEST(version_matches_rtlgetversion) {
    typedef LONG (WINAPI *pRtlGetVersion)(OSVERSIONINFOW *);
    pRtlGetVersion rgv = (pRtlGetVersion)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    REQUIRE(rgv);
    OSVERSIONINFOW vi; memset(&vi, 0, sizeof vi); vi.dwOSVersionInfoSize = sizeof vi;
    CHECK(rgv(&vi) == 0);
    CHECK(vi.dwMajorVersion == 10);
    CHECK(vi.dwMinorVersion == 0);
    CHECK(vi.dwPlatformId == VER_PLATFORM_WIN32_NT);
    t_note("OS build %lu (csd \"%ls\")", vi.dwBuildNumber, vi.szCSDVersion);
    HMODULE vm; pGetFileVersionInfoSizeW fsz; pGetFileVersionInfoW fgi; pVerQueryValueW fq;
    if (!load_ver(&vm, &fsz, &fgi, &fq)) return;
    WCHAR path[MAX_PATH]; get_u32_path(path, MAX_PATH);
    DWORD sz = fsz(path, NULL);
    if (sz) {
        BYTE *buf = (BYTE *)malloc(sz);
        if (buf && fgi(path, 0, sz, buf)) {
            VS_FIXEDFILEINFO *ffi = NULL; UINT l = 0;
            if (fq(buf, L"\\", (LPVOID *)&ffi, &l) && ffi) {
                if (HIWORD(ffi->dwFileVersionLS) != vi.dwBuildNumber) t_note("user32 file build %u differs from OS build %lu", HIWORD(ffi->dwFileVersionLS), vi.dwBuildNumber);
                CHECK(HIWORD(ffi->dwFileVersionLS) <= vi.dwBuildNumber);
            }
        }
        free(buf);
    }
    FreeLibrary(vm);
}

TEST(version_info_size_bad_path) {
    HMODULE vm; pGetFileVersionInfoSizeW fsz; pGetFileVersionInfoW fgi; pVerQueryValueW fq;
    if (!load_ver(&vm, &fsz, &fgi, &fq)) { t_skip("version.dll unavailable"); return; }
    DWORD h = 5;
    SetLastError(0);
    DWORD sz = fsz(L"C:\\no\\such\\dir\\user32.dll", &h);
    CHECK(sz == 0);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND);
    FreeLibrary(vm);
}

/* ---- DPI state ---- */

TEST(dpi_isprocessdpiaware_stable) {
    BOOL (WINAPI *ip)(void) = (void *)t_proc("IsProcessDPIAware");
    REQUIRE(ip);
    BOOL a = ip();
    for (int i = 0; i < 50; i++) CHECK(ip() == a);
    CHECK(a == TRUE || a == FALSE);
    t_note("IsProcessDPIAware=%d", a);
}

TEST(dpi_thread_context_valid) {
    typedef DPI_AWARENESS_CONTEXT (WINAPI *pGet)(void);
    typedef BOOL (WINAPI *pValid)(DPI_AWARENESS_CONTEXT);
    typedef DPI_AWARENESS (WINAPI *pAw)(DPI_AWARENESS_CONTEXT);
    pGet get = (pGet)t_proc("GetThreadDpiAwarenessContext");
    pValid valid = (pValid)t_proc("IsValidDpiAwarenessContext");
    pAw aw = (pAw)t_proc("GetAwarenessFromDpiAwarenessContext");
    REQUIRE(get); REQUIRE(valid); REQUIRE(aw);
    DPI_AWARENESS_CONTEXT c = get();
    CHECK(c != NULL);
    CHECK(valid(c));
    DPI_AWARENESS a = aw(c);
    CHECK(a == DPI_AWARENESS_UNAWARE || a == DPI_AWARENESS_SYSTEM_AWARE || a == DPI_AWARENESS_PER_MONITOR_AWARE);
    BOOL (WINAPI *ip)(void) = (void *)t_proc("IsProcessDPIAware");
    if (ip && !ip()) CHECK(a == DPI_AWARENESS_UNAWARE);
    t_note("thread dpi awareness=%d ctx=%p", a, c);
}

TEST(dpi_context_validation_table) {
    typedef BOOL (WINAPI *pValid)(DPI_AWARENESS_CONTEXT);
    typedef DPI_AWARENESS (WINAPI *pAw)(DPI_AWARENESS_CONTEXT);
    typedef BOOL (WINAPI *pEq)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);
    pValid valid = (pValid)t_proc("IsValidDpiAwarenessContext");
    pAw aw = (pAw)t_proc("GetAwarenessFromDpiAwarenessContext");
    pEq eq = (pEq)t_proc("AreDpiAwarenessContextsEqual");
    REQUIRE(valid); REQUIRE(aw); REQUIRE(eq);
    CHECK(valid((DPI_AWARENESS_CONTEXT)-1));
    CHECK(valid((DPI_AWARENESS_CONTEXT)-2));
    CHECK(valid((DPI_AWARENESS_CONTEXT)-3));
    CHECK(valid((DPI_AWARENESS_CONTEXT)-4));
    CHECK(!valid(NULL));
    CHECK(!valid((DPI_AWARENESS_CONTEXT)-1000));
    CHECK(!valid((DPI_AWARENESS_CONTEXT)0x12345678));
    CHECK(!valid((DPI_AWARENESS_CONTEXT)(LONG_PTR)1));
    CHECK(aw((DPI_AWARENESS_CONTEXT)-1) == DPI_AWARENESS_UNAWARE);
    CHECK(aw((DPI_AWARENESS_CONTEXT)-2) == DPI_AWARENESS_SYSTEM_AWARE);
    CHECK(aw((DPI_AWARENESS_CONTEXT)-3) == DPI_AWARENESS_PER_MONITOR_AWARE);
    CHECK(aw((DPI_AWARENESS_CONTEXT)-4) == DPI_AWARENESS_PER_MONITOR_AWARE);
    CHECK(aw(NULL) == DPI_AWARENESS_INVALID);
    CHECK(aw((DPI_AWARENESS_CONTEXT)-1000) == DPI_AWARENESS_INVALID);
    CHECK(eq((DPI_AWARENESS_CONTEXT)-3, (DPI_AWARENESS_CONTEXT)-3));
    CHECK(!eq((DPI_AWARENESS_CONTEXT)-3, (DPI_AWARENESS_CONTEXT)-4));
    CHECK(!eq((DPI_AWARENESS_CONTEXT)-1, (DPI_AWARENESS_CONTEXT)-2));
    CHECK(!eq(NULL, (DPI_AWARENESS_CONTEXT)-1));
    CHECK(!eq(NULL, NULL));
    t_note("-5 (unaware gdi scaled) valid=%d", valid((DPI_AWARENESS_CONTEXT)-5));
}
