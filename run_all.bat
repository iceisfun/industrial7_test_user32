@echo off
rem Headless run of every build\*.exe: results (text and TSV) in results\, progress on stdout, exit code 0 only
rem when every area produced a SUMMARY. No prompts, no pause, no dialogs. Extra arguments go to the runner
rem (for example: -Only menus,messages -TimeoutSec 900).
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_areas.ps1 -Root "%~dp0." %*
set RC=%ERRORLEVEL%
powershell -NoProfile -ExecutionPolicy Bypass -File tools\summarize.ps1 -Dir results
exit /b %RC%
