#include "t.h"
#include <setjmp.h>
#include <stdlib.h>
#include <psapi.h>

/* Harness. Three execution modes:
 *   exe                run every test; a supervisor process re-launches a worker (exe --worker N) so a
 *                      crash or hang costs one test, never the area. Per-test results go to a TSV.
 *   exe substr         legacy single-process run of the tests whose name contains substr (also used by
 *                      tests that spawn the exe as a child); a watchdog still bounds each test.
 *   exe --inproc       everything in one process (no supervisor).
 *   exe --list         print the test names.
 * Result-line grammar (stable, read by tools/summarize.py): RUN/OK/FAIL/SKIP per test, indented FAIL/NOTE/
 * SKIP detail lines, one SUMMARY line last. Environment: U32_TEST_TIMEOUT (s, default 300) per test,
 * U32_AREA_TIMEOUT (s, default 1500) per area, U32_MEM_MB (default 1024) private-bytes growth cap per test,
 * U32_TSV (path of the TSV; default results\<area>.tsv if results\ exists, else <area>.tsv).
 * TSV columns: idx, name, status (OK|FAIL|SKIP|HANG|CRASH|MEM|NOTRUN), checks_pass, checks_fail, ms. */

#define EX_RESUME 0x5A5Au
static HMODULE g_u32;
static volatile LONG g_pass, g_fail;
static int g_cur_fail, g_cur_skip;
static const char *g_cur;
static volatile int g_cur_idx = -1;
static jmp_buf g_jb;
static volatile int g_in_test;
static volatile DWORD g_main_tid, g_t_start;
static volatile LONG g_row_claimed;
static LONG g_p0, g_f0;
static HANDLE g_tsv = INVALID_HANDLE_VALUE, g_curf = INVALID_HANDLE_VALUE;
static DWORD g_limit_s = 300, g_mem_mb = 1024, g_area_s = 1500;
static int g_worker, g_have_tsv;
static SIZE_T g_mem_base;
static BOOL (WINAPI *p_mem)(HANDLE, PROCESS_MEMORY_COUNTERS *, DWORD);

HMODULE t_user32(void) { if (!g_u32) g_u32 = LoadLibraryA("user32.dll"); return g_u32; }
FARPROC t_proc(const char *n) { FARPROC p = GetProcAddress(t_user32(), n); return p; }
FARPROC t_proc_ord(unsigned o) { return GetProcAddress(t_user32(), (LPCSTR)(ULONG_PTR)o); }

#define MAXT 8192
static struct { const char *n; t_fn f; } g_t[MAXT];
static int g_nt;
void t_register(const char *n, t_fn f) { if (g_nt < MAXT) { g_t[g_nt].n = n; g_t[g_nt].f = f; g_nt++; } }

void t_check(int ok, const char *file, int line, const char *expr, const char *fmt, ...) {
    if (ok) { InterlockedIncrement(&g_pass); return; }
    InterlockedIncrement(&g_fail); g_cur_fail++;
    printf("  FAIL %s:%d: %s", file, line, expr);
    if (fmt) { va_list a; va_start(a, fmt); printf(" ("); vprintf(fmt, a); printf(")"); va_end(a); }
    printf("\n");
}
void t_note(const char *fmt, ...) { va_list a; va_start(a, fmt); printf("  NOTE "); vprintf(fmt, a); printf("\n"); va_end(a); }
void t_skip(const char *fmt, ...) {
    va_list a; va_start(a, fmt); printf("  SKIP "); vprintf(fmt, a); printf("\n"); va_end(a);
    g_cur_skip = 1;
}

static void tsv_row(int idx, const char *name, const char *st, long p, long f, DWORD ms) {
    char b[600]; DWORD w; int n;
    if (g_tsv == INVALID_HANDLE_VALUE) return;
    n = snprintf(b, sizeof b, "%d\t%s\t%s\t%ld\t%ld\t%lu\n", idx, name, st, p, f, ms);
    WriteFile(g_tsv, b, n, &w, NULL);
}
static void cur_set(int idx) {
    char b[32]; DWORD w; int n;
    if (g_curf == INVALID_HANDLE_VALUE) return;
    n = snprintf(b, sizeof b, "%11d\n", idx);
    SetFilePointer(g_curf, 0, NULL, FILE_BEGIN);
    WriteFile(g_curf, b, n, &w, NULL);
}

static LONG CALLBACK veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_in_test) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x406D1388 || c == DBG_PRINTEXCEPTION_C || c == 0x4001000A) return EXCEPTION_CONTINUE_SEARCH;
    if ((c & 0xF0000000) != 0xC0000000) return EXCEPTION_CONTINUE_SEARCH;
    /* a fault on another thread cannot be longjmp'ed out of: let it be fatal (the supervisor records CRASH) */
    if (GetCurrentThreadId() != g_main_tid) return EXCEPTION_CONTINUE_SEARCH;
    printf("  FAIL exception 0x%08lx at %p in %s\n", c, ep->ExceptionRecord->ExceptionAddress, g_cur);
    InterlockedIncrement(&g_fail); g_cur_fail++;
    g_in_test = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void print_summary_inproc(int ran, int tskip, int tfail) {
    printf("SUMMARY tests=%d failed=%d skipped=%d checks_pass=%ld checks_fail=%ld\n", ran, tfail, tskip, (long)g_pass, (long)g_fail);
}
static int g_ran, g_tskip, g_tfail;

/* The watchdog ends a test that hangs or balloons: it claims the test's row, reports, and either lets the
 * supervisor resume after this test (worker) or prints the summary and exits (single-process runs). */
static void wd_kill(const char *status, const char *why) {
    DWORD el;
    if (InterlockedExchange(&g_row_claimed, 1)) return;
    el = GetTickCount() - g_t_start;
    printf("  FAIL %s: %s\n", status, why);
    printf("FAIL %s\n", g_cur);
    InterlockedIncrement(&g_fail);
    tsv_row(g_cur_idx, g_cur, status, g_pass - g_p0, g_fail - g_f0, el);
    if (g_worker) ExitProcess(EX_RESUME);
    g_ran++; g_tfail++;
    print_summary_inproc(g_ran, g_tskip, g_tfail);
    ExitProcess(1);
}
static DWORD WINAPI wd_thread(LPVOID p) {
    DWORD last_mem = 0;
    (void)p;
    for (;;) {
        DWORD now, el;
        Sleep(200);
        if (!g_in_test) continue;
        now = GetTickCount(); el = now - g_t_start;
        if (el > g_limit_s * 1000) {
            char b[96]; snprintf(b, sizeof b, "test exceeded %lus", g_limit_s);
            wd_kill("HANG", b);
        }
        if (p_mem && now - last_mem > 1000) {
            PROCESS_MEMORY_COUNTERS pmc; last_mem = now;
            pmc.cb = sizeof pmc;
            if (p_mem(GetCurrentProcess(), &pmc, sizeof pmc) && pmc.PagefileUsage > g_mem_base + (SIZE_T)g_mem_mb * 1048576) {
                char b[96]; snprintf(b, sizeof b, "test grew private memory by more than %lu MB", g_mem_mb);
                wd_kill("MEM", b);
            }
        }
    }
    return 0;
}

static void run_test(int i) {
    DWORD ms;
    const char *st;
    g_cur = g_t[i].n; g_cur_idx = i; g_cur_fail = g_cur_skip = 0;
    cur_set(i);
    printf("RUN  %s\n", g_cur);
    SetLastError(0);
    g_p0 = g_pass; g_f0 = g_fail; g_row_claimed = 0;
    if (p_mem) { PROCESS_MEMORY_COUNTERS pmc; pmc.cb = sizeof pmc; if (p_mem(GetCurrentProcess(), &pmc, sizeof pmc)) g_mem_base = pmc.PagefileUsage; }
    g_t_start = GetTickCount();
    if (!setjmp(g_jb)) { g_in_test = 1; g_t[i].f(); g_in_test = 0; }
    g_in_test = 0;
    ms = GetTickCount() - g_t_start;
    if (ms > 3000) printf("  SLOW %lu ms\n", ms);
    st = g_cur_fail ? "FAIL" : g_cur_skip ? "SKIP" : "OK";
    if (InterlockedExchange(&g_row_claimed, 1)) { Sleep(INFINITE); } /* the watchdog is reporting this test */
    g_ran++;
    if (g_cur_fail) g_tfail++; else if (g_cur_skip) g_tskip++;
    printf("%s %s\n", g_cur_fail ? "FAIL" : g_cur_skip ? "SKIP" : "OK  ", g_cur);
    tsv_row(i, g_cur, st, g_pass - g_p0, g_fail - g_f0, ms);
}

/* ---- supervisor ---- */
typedef struct { int ok, fail, skip, hang, crash, mem, notrun, rows, last; long cp, cf; int have[MAXT]; } agg_t;
static agg_t g_agg;
static void tsv_load(const char *path, agg_t *a) {
    FILE *f = fopen(path, "r"); char line[700];
    memset(a, 0, sizeof *a); a->last = -1;
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        int idx; char name[512], st[16]; long p, q; unsigned long ms;
        if (line[0] == '#') continue;
        if (sscanf(line, "%d\t%511[^\t]\t%15s\t%ld\t%ld\t%lu", &idx, name, st, &p, &q, &ms) != 6) continue;
        if (idx < 0 || idx >= MAXT || a->have[idx]) continue;
        a->have[idx] = 1; a->rows++; if (idx > a->last) a->last = idx;
        a->cp += p; a->cf += q;
        if (!strcmp(st, "OK")) a->ok++; else if (!strcmp(st, "FAIL")) a->fail++; else if (!strcmp(st, "SKIP")) a->skip++;
        else if (!strcmp(st, "HANG")) a->hang++; else if (!strcmp(st, "CRASH")) a->crash++; else if (!strcmp(st, "MEM")) a->mem++;
        else if (!strcmp(st, "NOTRUN")) a->notrun++;
    }
    fclose(f);
}
static int read_cur(const char *path) {
    FILE *f = fopen(path, "r"); int v = -1;
    if (f) { if (fscanf(f, "%d", &v) != 1) v = -1; fclose(f); }
    return v;
}
static void append_raw(const char *path, int idx, const char *name, const char *st, long p, long q, DWORD ms) {
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    char b[600]; DWORD w; int n;
    if (h == INVALID_HANDLE_VALUE) return;
    n = snprintf(b, sizeof b, "%d\t%s\t%s\t%ld\t%ld\t%lu\n", idx, name, st, p, q, ms);
    WriteFile(h, b, n, &w, NULL); CloseHandle(h);
}

static int supervise(void) {
    char exe[MAX_PATH], path[MAX_PATH], curp[MAX_PATH + 8], area[128], cmd[MAX_PATH + 64];
    const char *e; int next = 0, workers = 0, area_to = 0;
    ULONGLONG t0 = GetTickCount64();
    HANDLE job; HANDLE hf;
    GetModuleFileNameA(NULL, exe, sizeof exe);
    { const char *b = strrchr(exe, '\\'); b = b ? b + 1 : exe; snprintf(area, sizeof area, "%s", b); char *d = strrchr(area, '.'); if (d) *d = 0; }
    e = getenv("U32_TSV");
    if (e && *e) snprintf(path, sizeof path, "%s", e);
    else { DWORD at = GetFileAttributesA("results"); if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) snprintf(path, sizeof path, "results\\%s.tsv", area); else snprintf(path, sizeof path, "%s.tsv", area); }
    snprintf(curp, sizeof curp, "%s.cur", path);
    hf = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf != INVALID_HANDLE_VALUE) { DWORD w; const char *h = "#idx\tname\tstatus\tpass\tfail\tms\n"; WriteFile(hf, h, (DWORD)strlen(h), &w, NULL); CloseHandle(hf); }
    hf = CreateFileA(curp, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(hf, "         -1\n", 13, &w, NULL); CloseHandle(hf); }
    SetEnvironmentVariableA("U32_TSV", path);
    SetEnvironmentVariableA("U32_CUR", curp);
    job = CreateJobObjectA(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li; memset(&li, 0, sizeof li);
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        li.ProcessMemoryLimit = (SIZE_T)(g_mem_mb + 1024) * 1048576;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li);
    }
    { HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); if (o && o != INVALID_HANDLE_VALUE) SetHandleInformation(o, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT); }
    while (next < g_nt) {
        STARTUPINFOA si; PROCESS_INFORMATION pi; DWORD code = 0, wr; int cur, stall_idx = -2; ULONGLONG stall_t = GetTickCount64(); int killed = 0;
        memset(&si, 0, sizeof si); si.cb = sizeof si;
        { HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); if (o && o != INVALID_HANDLE_VALUE) { si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = o; si.hStdError = o; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); if (!si.hStdInput) si.hStdInput = INVALID_HANDLE_VALUE; } }
        snprintf(cmd, sizeof cmd, "\"%s\" --worker %d", exe, next);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) { printf("FATAL cannot start worker (%lu)\n", GetLastError()); return 2; }
        if (job) AssignProcessToJobObject(job, pi.hProcess);
        ResumeThread(pi.hThread); CloseHandle(pi.hThread);
        workers++;
        for (;;) {
            wr = WaitForSingleObject(pi.hProcess, 1000);
            if (wr == WAIT_OBJECT_0) break;
            cur = read_cur(curp);
            if (cur != stall_idx) { stall_idx = cur; stall_t = GetTickCount64(); }
            if (GetTickCount64() - stall_t > (ULONGLONG)(g_limit_s + 60) * 1000) { TerminateProcess(pi.hProcess, 0xDEAD); killed = 1; }
            if (GetTickCount64() - t0 > (ULONGLONG)g_area_s * 1000) { TerminateProcess(pi.hProcess, 0xDEAD); killed = 2; }
        }
        GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess);
        tsv_load(path, &g_agg);
        cur = read_cur(curp);
        if (cur < next) cur = next;
        if (g_agg.last >= cur) { next = g_agg.last + 1; if (code != 0 && code != 1 && code != EX_RESUME && next < g_nt) { /* died after reporting: nothing lost */ } continue; }
        if (cur >= g_nt) break;
        {
            char why[96]; const char *st = killed == 2 ? "HANG" : killed == 1 ? "HANG" : "CRASH";
            if (killed == 2) snprintf(why, sizeof why, "area time budget %lus exhausted", g_area_s);
            else if (killed == 1) snprintf(why, sizeof why, "no progress for %lus", g_limit_s + 60);
            else snprintf(why, sizeof why, "worker died, exit code 0x%lx", code);
            printf("  FAIL %s: %s\nFAIL %s\n", st, why, g_t[cur].n);
            append_raw(path, cur, g_t[cur].n, st, 0, 1, 0);
            next = cur + 1;
            if (killed == 2) { area_to = 1; break; }
        }
        (void)workers;
    }
    tsv_load(path, &g_agg);
    if (area_to || GetTickCount64() - t0 > (ULONGLONG)g_area_s * 1000) {
        int n = 0;
        for (int i = 0; i < g_nt; i++) if (!g_agg.have[i]) { append_raw(path, i, g_t[i].n, "NOTRUN", 0, 0, 0); n++; }
        if (n) printf("  FAIL area timeout: %d tests not run\n", n);
        tsv_load(path, &g_agg);
    } else {
        int n = 0;
        for (int i = 0; i < g_nt; i++) if (!g_agg.have[i]) { append_raw(path, i, g_t[i].n, "NOTRUN", 0, 0, 0); n++; }
        if (n) { printf("  FAIL worker ended early: %d tests not run\n", n); tsv_load(path, &g_agg); }
    }
    {
        int ran = g_agg.rows - g_agg.notrun, fails = g_agg.fail + g_agg.hang + g_agg.crash + g_agg.mem + g_agg.notrun;
        printf("SUMMARY tests=%d failed=%d skipped=%d checks_pass=%ld checks_fail=%ld hang=%d crash=%d mem=%d notrun=%d workers=%d\n",
               ran, fails, g_agg.skip, g_agg.cp, g_agg.cf, g_agg.hang, g_agg.crash, g_agg.mem, g_agg.notrun, workers);
        return fails ? 1 : 0;
    }
}

void t_wait_pump(DWORD ms) {
    DWORD t0 = GetTickCount();
    do { t_pump(); Sleep(5); } while (GetTickCount() - t0 < ms);
}
int t_wait_until(int (*pred)(void *), void *ctx, DWORD timeout_ms) {
    DWORD t0 = GetTickCount();
    for (;;) {
        t_pump();
        if (pred(ctx)) return 1;
        if (GetTickCount() - t0 >= timeout_ms) return 0;
        Sleep(5);
    }
}

static const char *g_cls = "T32TestClass";
static LRESULT CALLBACK defproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }
const char *t_cls(void) {
    static int done;
    if (!done) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = defproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = g_cls;
        wc.hCursor = NULL; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        ATOM (WINAPI *rc)(const WNDCLASSA *) = (void *)t_proc("RegisterClassA");
        if (rc) rc(&wc);
        done = 1;
    }
    return g_cls;
}
HWND t_make_window(const char *cls, DWORD st, DWORD ex, int x, int y, int w, int h, HWND parent) {
    HWND (WINAPI *cw)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) = (void *)t_proc("CreateWindowExA");
    if (!cw) return NULL;
    if (!cls) cls = t_cls();
    return cw(ex, cls, "t", st, x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}
void t_pump(void) {
    BOOL (WINAPI *pm)(LPMSG, HWND, UINT, UINT, UINT) = (void *)t_proc("PeekMessageA");
    BOOL (WINAPI *tm)(const MSG *) = (void *)t_proc("TranslateMessage");
    LRESULT (WINAPI *dm)(const MSG *) = (void *)t_proc("DispatchMessageA");
    MSG m; int n = 0;
    if (!pm) return;
    while (n++ < 10000 && pm(&m, NULL, 0, 0, PM_REMOVE)) { if (tm) tm(&m); if (dm) dm(&m); }
}


static DWORD envnum(const char *n, DWORD d) { const char *e = getenv(n); if (e && *e) { long v = atol(e); if (v > 0) return (DWORD)v; } return d; }

int main(int argc, char **argv) {
    const char *filter = NULL;
    int start = 0, inproc = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    g_limit_s = envnum("U32_TEST_TIMEOUT", g_limit_s);
    g_mem_mb = envnum("U32_MEM_MB", g_mem_mb);
    g_area_s = envnum("U32_AREA_TIMEOUT", g_area_s);
    if (argc > 1 && !strcmp(argv[1], "--list")) { for (int i = 0; i < g_nt; i++) printf("%s\n", g_t[i].n); return 0; }
    if (argc > 2 && !strcmp(argv[1], "--worker")) { g_worker = 1; start = atoi(argv[2]); }
    else if (argc > 1 && !strcmp(argv[1], "--inproc")) { inproc = 1; }
    else if (argc > 1 && strncmp(argv[1], "--", 2)) { filter = argv[1]; }
    else if (g_nt > 0) {
        int rc = supervise();
        return rc;
    }
    if (g_worker || inproc) {
        const char *p = getenv("U32_TSV");
        if (p && *p) {
            g_tsv = CreateFileA(p, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        p = getenv("U32_CUR");
        if (p && *p) g_curf = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    g_main_tid = GetCurrentThreadId();
    { HMODULE k = GetModuleHandleA("kernel32.dll"); if (k) p_mem = (void *)GetProcAddress(k, "K32GetProcessMemoryInfo"); }
    AddVectoredExceptionHandler(1, veh);
    if (!t_user32()) { printf("FATAL cannot load user32.dll\n"); return 2; }
    { HANDLE h = CreateThread(NULL, 0, wd_thread, NULL, 0, NULL); if (h) CloseHandle(h); }
    for (int i = start; i < g_nt; i++) {
        if (filter && !strstr(g_t[i].n, filter)) continue;
        run_test(i);
    }
    if (g_worker) return 0;
    print_summary_inproc(g_ran, g_tskip, g_tfail);
    return g_tfail ? 1 : 0;
}
