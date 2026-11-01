/* dialogs: process-level guards so a stuck modal box can never wait for a human.
 *  - a watchdog thread closes any #32770 owned by this process that stays visible
 *    for too long (cross-thread PostMessage works against a running modal loop);
 *  - dlg_run_child() runs one named test of this exe in a child process with a
 *    hard timeout, for sequences that have been seen to hang. */
#include "dlgutil.h"

#define WD_LIMIT_MS 25000

/* the shared per-test watchdog timer (see dlgutil.h) */
UINT_PTR wd_id; DWORD wd_t0;
VOID CALLBACK wd_proc(HWND h, UINT m, UINT_PTR id, DWORD t) {
    HWND a;
    if (GetTickCount() - wd_t0 < 19000) return;
    a = GetActiveWindow();
    if (a) EndDialog(a, -999);
}

static HWND wd_cur[16]; static int wd_nc;
static BOOL CALLBACK wd_en(HWND h, LPARAM l) {
    DWORD pid = 0; char cls[32];
    (void)l;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    if (!GetClassNameA(h, cls, sizeof cls) || strcmp(cls, "#32770")) return TRUE;
    if (!IsWindowVisible(h)) return TRUE;
    if (wd_nc < 16) wd_cur[wd_nc++] = h;
    return TRUE;
}
static DWORD WINAPI wd_thread(LPVOID p) {
    struct { HWND h; DWORD first; } seen[16], nxt[16];
    int n = 0;
    (void)p;
    for (;;) {
        Sleep(1000);
        DWORD now = GetTickCount();
        if (!U(EnumWindows)) continue;
        wd_nc = 0;
        U(EnumWindows)(wd_en, 0);
        int nn = 0;
        for (int i = 0; i < wd_nc; i++) {
            DWORD first = now;
            for (int j = 0; j < n; j++) if (seen[j].h == wd_cur[i]) first = seen[j].first;
            nxt[nn].h = wd_cur[i]; nxt[nn].first = first; nn++;
            if (now - first > WD_LIMIT_MS) {
                /* the dialog has outlived any legitimate test: close it */
                PostMessageW(wd_cur[i], WM_COMMAND, IDCANCEL, 0);
                PostMessageW(wd_cur[i], WM_COMMAND, IDOK, 0);
                PostMessageW(wd_cur[i], WM_CLOSE, 0, 0);
            }
        }
        memcpy(seen, nxt, sizeof nxt); n = nn;
    }
    return 0;
}
static void __attribute__((constructor)) wd_start(void) {
    HANDLE h = CreateThread(NULL, 0, wd_thread, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

int dlg_run_child(const char *test, DWORD timeout_ms, DWORD *exit_code) {
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
    SetEnvironmentVariableA("U32_DLG_CHILD", "1");
    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_OBJECT_0) {
            ok = 1;
            if (exit_code) GetExitCodeProcess(pi.hProcess, exit_code);
        } else TerminateProcess(pi.hProcess, 0xDEAD);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    SetEnvironmentVariableA("U32_DLG_CHILD", NULL);
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
