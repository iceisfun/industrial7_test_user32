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

Copy `build\*.exe`, `run_all.bat` and `tools\summarize.ps1` to one folder (keep `build\` as a subfolder), log on to an
interactive desktop and run `run_all.bat`. Output goes to `results\<area>.txt`; a table is printed at the end.
Re-aggregate later with `powershell -File tools\summarize.ps1 -Dir results` or `python tools\summarize.py results`.
A missing `SUMMARY` line means the exe crashed or was killed. Do not touch the machine while it runs (windows flash,
the foreground changes). Pass a substring to an exe to run only matching tests.

## Coverage

`COVERAGE.md` lists exports exercised and those with only presence checks; regenerate with `python3 tools/coverage.py`.
