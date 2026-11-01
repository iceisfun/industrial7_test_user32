#include "cbcommon.h"
UINT cb_log[CB_LOGMAX]; WPARAM cb_logw[CB_LOGMAX]; int cb_nlog;
HANDLE cb_render_data;
int cb_render_open_ok;
void cb_logclear(void) { cb_nlog = 0; }
int cb_logcount(UINT m) { int n = 0; for (int i = 0; i < cb_nlog; i++) if (cb_log[i] == m) n++; return n; }
HWND cb_next;
static LRESULT CALLBACK cbproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if ((m == WM_DRAWCLIPBOARD || m == WM_CHANGECBCHAIN) && cb_next && U(IsWindow)(cb_next)) {
        if (m == WM_CHANGECBCHAIN && (HWND)w == cb_next) { cb_next = (HWND)l; }
        else { DWORD_PTR rr; U(SendMessageTimeoutA)(cb_next, m, w, l, SMTO_ABORTIFHUNG, 2000, &rr); }
    }
    switch (m) {
    case WM_RENDERFORMAT: case WM_RENDERALLFORMATS: case WM_DESTROYCLIPBOARD:
    case WM_DRAWCLIPBOARD: case WM_CHANGECBCHAIN: case WM_CLIPBOARDUPDATE:
    case WM_ASKCBFORMATNAME: case WM_PAINTCLIPBOARD: case WM_SIZECLIPBOARD:
        if (cb_nlog < CB_LOGMAX) { cb_log[cb_nlog] = m; cb_logw[cb_nlog++] = w; }
        if (m == WM_RENDERFORMAT && cb_render_data)
            U(SetClipboardData)((UINT)w, cb_render_data);
        if (m == WM_RENDERALLFORMATS && cb_render_data)
            U(SetClipboardData)(CF_TEXT, cb_render_data);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}
HWND cb_win(void) {
    static int reg;
    if (!reg) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = cbproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "CbTestOwner";
        U(RegisterClassA)(&wc); reg = 1;
    }
    return U(CreateWindowExA)(0, "CbTestOwner", "cb", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
}
void cb_destroy(HWND h) { if (h) U(DestroyWindow)(h); }
void cb_reset(void) {
    if (!OPEN) return;
    int n = 0;
    while (n++ < 8 && CLOSE()) ;
    cb_render_data = NULL;
    for (int t = 0; t < 100; t++) { if (OPEN(NULL)) { EMPTY(); CLOSE(); break; } Sleep(10); }
}
HGLOBAL cb_mem(const void *p, size_t n) {
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n ? n : 1);
    if (g && p) { void *d = GlobalLock(g); memcpy(d, p, n); GlobalUnlock(g); }
    return g;
}
HGLOBAL cb_text(const char *s) { return cb_mem(s, strlen(s) + 1); }
HGLOBAL cb_wtext(const WCHAR *s) { return cb_mem(s, (lstrlenW(s) + 1) * 2); }
int cb_has(UINT fmt) {
    UINT f = 0; int found = 0, n = 0;
    while (n++ < 1000 && (f = ENUM(f))) if (f == fmt) found = 1;
    return found;
}

int cb_run_child(const char *test, DWORD timeout_ms, DWORD *exit_code) {
    char exe[MAX_PATH], cmd[MAX_PATH + 96], tmp[MAX_PATH], path[MAX_PATH];
    STARTUPINFOA si; PROCESS_INFORMATION pi; SECURITY_ATTRIBUTES sa;
    HANDLE out; int ok = 0;
    if (!GetModuleFileNameA(NULL, exe, sizeof exe)) return 0;
    GetTempPathA(sizeof tmp, tmp);
    snprintf(path, sizeof path, "%su32child_%lu.txt", tmp, GetCurrentProcessId());
    memset(&sa, 0, sizeof sa); sa.nLength = sizeof sa; sa.bInheritHandle = TRUE;
    out = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = INVALID_HANDLE_VALUE; si.hStdOutput = out; si.hStdError = out;
    snprintf(cmd, sizeof cmd, "\"%s\" %s", exe, test);
    SetEnvironmentVariableA("U32_CB_CHILD", "1");
    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_OBJECT_0) {
            ok = 1;
            if (exit_code) GetExitCodeProcess(pi.hProcess, exit_code);
        } else TerminateProcess(pi.hProcess, 0xDEAD);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    SetEnvironmentVariableA("U32_CB_CHILD", NULL);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    /* replay the child's output indented so it cannot be mistaken for this exe's own result lines */
    {
        FILE *f = fopen(path, "r"); char line[512]; int n = 0;
        if (f) {
            while (fgets(line, sizeof line, f) && n++ < 60) { line[strcspn(line, "\r\n")] = 0; printf("    child| %s\n", line); }
            fclose(f);
        }
        DeleteFileA(path);
    }
    return ok;
}
