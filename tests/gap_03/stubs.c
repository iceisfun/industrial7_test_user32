/* gap_03: export-presence/shape checks for undocumented, privileged or
 * dangerous exports (kernel-forwarding stubs). They are never called: their
 * signatures are not documented and several act on global input/system state
 * (LockWorkStation, font loading, RIM/MIT device plumbing). */
#include "t.h"

static HMODULE u32(void) { return t_user32(); }

static const IMAGE_EXPORT_DIRECTORY *exp_dir(HMODULE m, DWORD *size) {
    BYTE *b = (BYTE *)m;
    IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER *)b;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + dh->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    *size = dd->Size;
    return dd->VirtualAddress ? (IMAGE_EXPORT_DIRECTORY *)(b + dd->VirtualAddress) : NULL;
}
static int is_forwarded(HMODULE m, void *addr) {
    DWORD sz; BYTE *b = (BYTE *)m;
    IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER *)b;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + dh->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    sz = dd->Size;
    DWORD rva = (DWORD)((BYTE *)addr - b);
    return rva >= dd->VirtualAddress && rva < dd->VirtualAddress + sz;
}

struct stub { const char *name; const char *nt; };

static void check_stub(const char *name, const char *ntname, int expect_syscall_jmp) {
    FARPROC p = t_proc(name);
    if (!p) { t_skip("missing export: %s", name); return; }
    /* lives inside user32, is code, not a forwarder */
    HMODULE owner = NULL;
    BOOL ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
    CHECKF(ok && owner == u32(), "%s owner", name);
    CHECKF(!is_forwarded(u32(), (void *)p), "%s forwarded", name);
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery((void *)p, &mbi, sizeof mbi) == sizeof mbi);
    DWORD x = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    CHECKF(mbi.State == MEM_COMMIT && (mbi.Protect & x), "%s executable", name);
    CHECKF(mbi.Type == MEM_IMAGE, "%s image", name);
    /* export-by-name is case sensitive and exact */
    char tmp[128]; size_t n = strlen(name);
    memcpy(tmp, name, n + 1);
    tmp[0] ^= 0x20;   /* flip case of first letter */
    SetLastError(0);
    FARPROC q = GetProcAddress(u32(), tmp);
    CHECKF(q == NULL, "%s case-flipped found", name);
    CHECKF(GetLastError() == ERROR_PROC_NOT_FOUND, "%s err %lu", name, GetLastError());
    memcpy(tmp, name, n); tmp[n] = 'X'; tmp[n + 1] = 0;
    SetLastError(0);
    CHECKF(GetProcAddress(u32(), tmp) == NULL, "%s+X found", name);
    CHECK(GetLastError() == ERROR_PROC_NOT_FOUND);
    memcpy(tmp, name, n + 1); tmp[n - 2] = 0;
    SetLastError(0);
    CHECKF(GetProcAddress(u32(), tmp) == NULL, "%s prefix found", name);
    /* stable and 16-byte aligned (x64 function entries) */
    CHECK(GetProcAddress(u32(), name) == p);
    CHECK(((ULONG_PTR)p & 0xF) == 0);
    /* in the ordinal map: some ordinal resolves to the same address */
    DWORD sz;
    const IMAGE_EXPORT_DIRECTORY *ed = exp_dir(u32(), &sz);
    int found = 0;
    if (ed) {
        DWORD *names = (DWORD *)((BYTE *)u32() + ed->AddressOfNames);
        WORD *ords = (WORD *)((BYTE *)u32() + ed->AddressOfNameOrdinals);
        DWORD *funcs = (DWORD *)((BYTE *)u32() + ed->AddressOfFunctions);
        for (DWORD i = 0; i < ed->NumberOfNames; i++)
            if (!strcmp((char *)u32() + names[i], name)) {
                found = 1;
                FARPROC byord = GetProcAddress(u32(), (LPCSTR)(ULONG_PTR)(ords[i] + ed->Base));
                CHECKF(byord == p, "%s by ordinal", name);
                CHECK((BYTE *)u32() + funcs[ords[i]] == (BYTE *)p);
                t_note("%s ordinal %u at %p", name, ords[i] + ed->Base, (void *)p);
                break;
            }
    }
    CHECKF(found, "%s in export name table", name);
    /* Shape of the body: 6-byte "jmp [rip+disp]" through the win32u import */
    BYTE *c = (BYTE *)p;
    if (expect_syscall_jmp && !(c[0] == 0xFF && c[1] == 0x25))
        t_note("%s: not a bare import jump (%02x %02x ...)", name, c[0], c[1]);
    /* The forwarded win32u syscall stub exists */
    if (ntname) {
        HMODULE w = LoadLibraryA("win32u.dll");
        if (w) {
            FARPROC nt = GetProcAddress(w, ntname);
            if (!nt) t_note("win32u has no %s", ntname);
            else {
                /* syscall stub: mov r10,rcx; mov eax,imm32 */
                BYTE *s = (BYTE *)nt;
                CHECKF(s[0] == 0x4C && s[1] == 0x8B && s[2] == 0xD1 && s[3] == 0xB8, "%s syscall prologue", ntname);
            }
        }
    }
}

#define STUBTEST(tname, nm, nt, jmp) \
    TEST(gap3_stub_##tname) { check_stub(nm, nt, jmp); }

STUBTEST(lockworkstation, "LockWorkStation", "NtUserLockWorkStation", 1)
STUBTEST(mitgetcursorupdatehandle, "MITGetCursorUpdateHandle", "NtMITGetCursorUpdateHandle", 1)
STUBTEST(mitsetforegroundroutinginfo, "MITSetForegroundRoutingInfo", "NtMITSetForegroundRoutingInfo", 1)
STUBTEST(mitsetinputdelegationmode, "MITSetInputDelegationMode", "NtMITSetInputDelegationMode", 1)
STUBTEST(mitsetlastinputrecipient, "MITSetLastInputRecipient", "NtMITSetLastInputRecipient", 1)
STUBTEST(mitsynthesizetouchinput, "MITSynthesizeTouchInput", "NtMITSynthesizeTouchInput", 1)
STUBTEST(mappointsbyvisualidentifier, "MapPointsByVisualIdentifier", "NtUserMapPointsByVisualIdentifier", 1)
STUBTEST(mapvisualrelativepoints, "MapVisualRelativePoints", "NtMapVisualRelativePoints", 1)
STUBTEST(paintmenubar, "PaintMenuBar", "NtUserPaintMenuBar", 1)
STUBTEST(paintmonitor, "PaintMonitor", "NtUserPaintMonitor", 1)
STUBTEST(querybsdrwindow, "QueryBSDRWindow", "NtUserQueryBSDRWindow", 1)
STUBTEST(querysendmessage, "QuerySendMessage", "NtUserQuerySendMessage", 1)
STUBTEST(rimaddinputobserver, "RIMAddInputObserver", "NtRIMAddInputObserver", 1)
STUBTEST(rimaresiblingdevices, "RIMAreSiblingDevices", "NtRIMAreSiblingDevices", 1)
STUBTEST(rimdeviceiocontrol, "RIMDeviceIoControl", "NtRIMDeviceIoControl", 1)
STUBTEST(rimenablemonitormappingfordevice, "RIMEnableMonitorMappingForDevice", "NtRIMEnableMonitorMappingForDevice", 1)
STUBTEST(rimfreeinputbuffer, "RIMFreeInputBuffer", "NtRIMFreeInputBuffer", 1)
STUBTEST(rimgetdevicepreparseddata, "RIMGetDevicePreparsedData", "NtRIMGetDevicePreparsedData", 1)
STUBTEST(rimgetdevicepreparseddatalockfree, "RIMGetDevicePreparsedDataLockfree", "NtRIMGetDevicePreparsedDataLockfree", 1)
STUBTEST(rimgetdeviceproperties, "RIMGetDeviceProperties", "NtRIMGetDeviceProperties", 1)
STUBTEST(rimgetdevicepropertieslockfree, "RIMGetDevicePropertiesLockfree", "NtRIMGetDevicePropertiesLockfree", 1)
STUBTEST(rimgetphysicaldevicerect, "RIMGetPhysicalDeviceRect", "NtRIMGetPhysicalDeviceRect", 1)
STUBTEST(rimgetsourceprocessid, "RIMGetSourceProcessId", "NtRIMGetSourceProcessId", 1)
STUBTEST(rimobservenextinput, "RIMObserveNextInput", "NtRIMObserveNextInput", 1)
STUBTEST(rimonpnpnotification, "RIMOnPnpNotification", "NtRIMOnPnpNotification", 1)
STUBTEST(rimontimernotification, "RIMOnTimerNotification", "NtRIMOnTimerNotification", 1)
STUBTEST(rimquerydevicepath, "RIMQueryDevicePath", "NtRIMQueryDevicePath", 1)
/* non-bare wrappers (client-side logic in user32) */
STUBTEST(loadlocalfonts, "LoadLocalFonts", NULL, 0)
STUBTEST(loadremotefonts, "LoadRemoteFonts", NULL, 0)
STUBTEST(loadkeyboardlayoutex, "LoadKeyboardLayoutEx", NULL, 0)
STUBTEST(menuwindowproca, "MenuWindowProcA", NULL, 0)
STUBTEST(menuwindowprocw, "MenuWindowProcW", NULL, 0)
STUBTEST(iswow64message, "IsWow64Message", NULL, 0)
STUBTEST(mbtowcsex, "MBToWCSEx", NULL, 0)
STUBTEST(mbtowcsext, "MBToWCSExt", NULL, 0)
STUBTEST(mb_getstring, "MB_GetString", NULL, 0)
STUBTEST(makethreadtsfeventaware, "MakeThreadTSFEventAware", NULL, 0)
STUBTEST(messagebeep, "MessageBeep", NULL, 0)
STUBTEST(notifyoverlaywindow, "NotifyOverlayWindow", NULL, 0)
STUBTEST(privateregisterics, "PrivateRegisterICSProc", NULL, 0)

static const char *all_names[] = {
 "IsWow64Message","LoadKeyboardLayoutEx","LoadLocalFonts","LoadRemoteFonts","LockWorkStation","MBToWCSEx","MBToWCSExt",
 "MB_GetString","MITGetCursorUpdateHandle","MITSetForegroundRoutingInfo","MITSetInputDelegationMode","MITSetLastInputRecipient",
 "MITSynthesizeTouchInput","MakeThreadTSFEventAware","MapPointsByVisualIdentifier","MapVisualRelativePoints","MenuWindowProcA",
 "MenuWindowProcW","MessageBeep","NotifyOverlayWindow","PaintMenuBar","PaintMonitor","PrivateRegisterICSProc","QueryBSDRWindow",
 "QuerySendMessage","RIMAddInputObserver","RIMAreSiblingDevices","RIMDeviceIoControl","RIMEnableMonitorMappingForDevice",
 "RIMFreeInputBuffer","RIMGetDevicePreparsedData","RIMGetDevicePreparsedDataLockfree","RIMGetDeviceProperties",
 "RIMGetDevicePropertiesLockfree","RIMGetPhysicalDeviceRect","RIMGetSourceProcessId","RIMObserveNextInput","RIMOnPnpNotification",
 "RIMOnTimerNotification","RIMQueryDevicePath" };
#define NALL (sizeof all_names / sizeof all_names[0])

TEST(gap3_all_exports_distinct_addresses) {
    FARPROC a[NALL];
    int n = 0;
    for (unsigned i = 0; i < NALL; i++) {
        a[i] = t_proc(all_names[i]);
        if (!a[i]) { t_note("missing %s", all_names[i]); continue; }
        n++;
    }
    if (!n) { t_skip("none present"); return; }
    for (unsigned i = 0; i < NALL; i++)
        for (unsigned j = i + 1; j < NALL; j++)
            if (a[i] && a[j]) CHECKF(a[i] != a[j], "%s == %s", all_names[i], all_names[j]);
}
TEST(gap3_all_exports_all_present) {
    int miss = 0;
    for (unsigned i = 0; i < NALL; i++) if (!t_proc(all_names[i])) { miss++; t_note("missing %s", all_names[i]); }
    CHECK(miss == 0);
}
TEST(gap3_all_exports_stub_sizes_gap) {
    /* Bare syscall jump stubs are laid out 16 bytes apart in groups; just assert they are within the image */
    HMODULE m = u32();
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)m + ((IMAGE_DOS_HEADER *)m)->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
    for (unsigned i = 0; i < NALL; i++) {
        FARPROC p = t_proc(all_names[i]);
        if (!p) continue;
        ULONG_PTR off = (ULONG_PTR)p - (ULONG_PTR)m;
        CHECKF(off > 0x1000 && off < size, "%s", all_names[i]);
    }
}
TEST(gap3_all_exports_loadlibrary_same_module) {
    HMODULE m2 = GetModuleHandleA("user32.dll");
    CHECK(m2 == u32());
    for (unsigned i = 0; i < NALL; i++) {
        FARPROC p = GetProcAddress(m2, all_names[i]);
        FARPROC q = t_proc(all_names[i]);
        CHECKF(p == q, "%s", all_names[i]);
    }
}
TEST(gap3_all_exports_null_name_lookup) {
    SetLastError(0);
    CHECK(GetProcAddress(u32(), "") == NULL);
    CHECK(GetLastError() == ERROR_PROC_NOT_FOUND);
    SetLastError(0);
    CHECK(GetProcAddress(u32(), (LPCSTR)(ULONG_PTR)0xFFFF) == NULL);
    {   /* The loader's error for an out-of-range ordinal is not the same on every
         * Windows build: accept the plausible codes (Win10 22H2 gives ERROR_INVALID_ORDINAL, 182) and record which one. */
        DWORD e = GetLastError();
        CHECKF(e == ERROR_PROC_NOT_FOUND || e == ERROR_INVALID_PARAMETER || e == ERROR_INVALID_ORDINAL, "out-of-range ordinal error %lu", e);
        t_note("GetProcAddress(ordinal 0xFFFF) error %lu", e);
    }
}
