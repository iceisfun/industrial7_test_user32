# user32 conformance corpus

Black-box tests for Windows 10 x64 `user32.dll`. `user32` is never linked: each test loads it with
`LoadLibrary`, resolves entry points with `GetProcAddress` (`U(Name)` gives a pointer typed from the
SDK header), and calls through the pointer.

    make                 # cross-compile build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]   # on Windows; optional test-name filter

Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last.
Exit code is 1 if any test failed. `NOTE` lines record observed, undocumented behaviour.
Run each exe from an interactive desktop session. `tools/user32_exports.txt` lists the 1005 named exports.

## Areas

Each directory under `tests/` builds into `build/<area>.exe`:
clipboard, controls, core_smoke (harness smoke), dde_hooks_accel (DDE, hooks, accelerators, timers), dialogs,
input_kbd, input_mouse, menus, messages, misc_ui, paint_gdi, resources_icons, strings_misc (string/char APIs,
wsprintf, export-table checks), sysparams, window_class, window_mgmt, winsta_process (window stations, desktops,
process/thread), and gap_00..gap_07 (exports not covered by the themed areas: broadcast messages, stubs, DPI,
undocumented exports, etc.).

## Building

    ./build.sh            # same as: make -j8   (needs x86_64-w64-mingw32-gcc)
    make build/<area>.exe # one area

(`build.sh` is simply `make clean && make -j8`; there is no other build step.)

## Running everything on Windows

Copy `build\*.exe`, `run_all.bat` and `tools\` to one folder (keep `build\` as a subfolder), log on to an
interactive desktop and run `run_all.bat` (no prompts; extra arguments go to the runner, e.g.
`run_all.bat -Only menus,messages -TimeoutSec 900`). Progress streams to stdout, per-area output goes to
`results\<area>.txt`, per-test results to `results\<area>.tsv` (idx, name, status, checks_pass, checks_fail, ms;
status is OK, FAIL, SKIP, HANG, CRASH, MEM or NOTRUN), and a table is printed at the end. The batch file exits 0
only if every area produced a SUMMARY.
Re-aggregate later with `powershell -File tools\summarize.ps1 -Dir results` or `python tools\summarize.py results`;
compare runs with `python tools\cmp_runs.py runA runB ...` (transcripts or results directories).
Do not touch the machine while it runs (windows flash, the foreground changes). A substring argument to an exe
runs only the matching tests in one process.

## Harness

`common/t.c` runs each area under a supervisor: the exe re-launches itself as a worker (`exe --worker N`) inside a
job object, so a crash, hang, `ExitProcess` or memory blow-up in one test is recorded for that test (HANG, CRASH,
MEM) and the run resumes at the next test. Every result line is flushed as it happens. Limits (environment):
`U32_TEST_TIMEOUT` seconds per test (default 300), `U32_AREA_TIMEOUT` per area (1500), `U32_MEM_MB` private-byte
growth per test (1024), `U32_TSV` TSV path. `exe --list` prints the test names; `exe --inproc` runs without a
supervisor. `make selftest` builds `build/selftest/selftest.exe`, a deliberately failing/crashing/hanging area that
exercises the containment (run it with `U32_TEST_TIMEOUT=6 U32_MEM_MB=200 U32_SELFTEST_HANG=1`).

## Coverage

`COVERAGE.md` lists exports exercised and those with only presence checks; regenerate with `python3 tools/coverage.py`.
