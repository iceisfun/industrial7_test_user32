@echo off
rem Runs every build\*.exe from an interactive desktop session; output in results\<area>.txt
setlocal
cd /d "%~dp0"
if not exist results mkdir results
for %%F in (build\*.exe) do (
  echo == %%~nF
  "%%F" > "results\%%~nF.txt" 2>&1
  echo exit=%ERRORLEVEL% >> "results\%%~nF.txt"
)
powershell -NoProfile -ExecutionPolicy Bypass -File tools\summarize.ps1 -Dir results
endlocal
