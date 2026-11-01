/* dde_hooks_accel: run one named test of this exe in a child process under a hard timeout. */
#include "t.h"

int dh_run_child(const char *test, DWORD timeout_ms, DWORD *exit_code) {
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
    SetEnvironmentVariableA("U32_DH_CHILD", "1");
    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        /* Keep the message loop running while the child works: the child's DDE broadcasts (WM_DDE_INITIATE,
         * TERMINATE) are sent to every top-level window of the desktop, including this process's, and a
         * blocked thread here would stall the child until its own timeout. */
        DWORD w = WAIT_TIMEOUT, t0 = GetTickCount();
        while (GetTickCount() - t0 < timeout_ms) {
            w = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 50, QS_ALLINPUT);
            if (w == WAIT_OBJECT_0) break;
            MSG m; while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        }
        if (w == WAIT_OBJECT_0) {
            ok = 1;
            if (exit_code) GetExitCodeProcess(pi.hProcess, exit_code);
        } else TerminateProcess(pi.hProcess, 0xDEAD);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    SetEnvironmentVariableA("U32_DH_CHILD", NULL);
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
