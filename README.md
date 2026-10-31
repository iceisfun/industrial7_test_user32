# user32 conformance corpus

Black-box tests for Windows 10 x64 `user32.dll`. `user32` is never linked: each test loads it with
`LoadLibrary`, resolves entry points with `GetProcAddress` (`U(Name)` gives a pointer typed from the
SDK header), and calls through the pointer.

    make                 # cross-compile build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]   # on Windows; optional test-name filter

Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last.
Exit code is 1 if any test failed. `NOTE` lines record observed, undocumented behaviour.
Run each exe from an interactive desktop session. `tools/user32_exports.txt` lists the 1005 named exports.
