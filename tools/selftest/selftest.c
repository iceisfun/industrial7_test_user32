/* Harness self-test: deliberately failing, crashing and hanging tests.
 * Expected result (statuses): ok1 OK, fail1 FAIL, skip1 SKIP, crash_null CRASH-or-FAIL (exception caught),
 * crash_thread CRASH (worker dies), exit_in_test CRASH, hang1 HANG (only when U32_SELFTEST_HANG is set),
 * mem_hog MEM, heap_overflow CRASH, ok_after OK. Build: make selftest. Run: set U32_TEST_TIMEOUT=5 and U32_SELFTEST_HANG=1. */
#include "t.h"
#include <stdlib.h>
TEST(ok1) { CHECK(1); }
TEST(fail1) { CHECK(0); }
TEST(skip1) { t_skip("by design"); }
TEST(crash_null) { CHECK(1); *(volatile int *)0 = 1; }
static DWORD WINAPI boom(LPVOID p) { *(volatile int *)0 = 1; return 0; }
TEST(crash_thread) { HANDLE h = CreateThread(0, 0, boom, 0, 0, 0); WaitForSingleObject(h, INFINITE); }
TEST(exit_in_test) { ExitProcess(0); }
TEST(hang1) { if (!getenv("U32_SELFTEST_HANG")) { t_skip("hang disabled"); return; } for (;;) Sleep(100); }
TEST(mem_hog) {
    if (!getenv("U32_SELFTEST_HANG")) { t_skip("mem disabled"); return; }
    for (int i = 0; i < 100000; i++) { char *p = VirtualAlloc(NULL, 1 << 20, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); if (p) memset(p, 1, 1 << 20); Sleep(1); }
}
TEST(heap_overflow) {
    if (!getenv("U32_SELFTEST_HANG")) { t_skip("heap check disabled"); return; }
    char *p = HeapAlloc(GetProcessHeap(), 0, 32);
    memset(p, 0xAA, 32 + 48);   /* overruns into the next heap header: HeapValidate fails after the test */
}
TEST(ok_after) { CHECK(1); CHECK(1); }
