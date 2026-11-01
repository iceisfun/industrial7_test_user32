# Headless runner: runs every build\*.exe, streams each area's output to stdout as it is produced and to
# results\<area>.txt, bounds each area with a time limit, prints one status line per area.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_areas.ps1 [-Root <dir>] [-TimeoutSec 1800] [-Only <area,area>]
# <Root> holds build\ (the exes); results\ is created there. The exes also write results\<area>.tsv
# (machine-readable per-test results: idx, name, status, checks_pass, checks_fail, ms).
param(
  [string]$Root = '',
  [int]$TimeoutSec = 1800,
  [string]$Only = ''
)
$ErrorActionPreference = 'Continue'
if (-not $Root) { $Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path) }
function Say([string]$m) { [Console]::WriteLine($m); [Console]::Out.Flush() }

$build = Join-Path $Root 'build'
$res = Join-Path $Root 'results'
New-Item -ItemType Directory -Force -Path $res | Out-Null
$exes = @(Get-ChildItem $build -Filter *.exe -ErrorAction SilentlyContinue | Sort-Object Name)
if ($Only) { $want = $Only.Split(','); $exes = @($exes | Where-Object { $want -contains $_.BaseName }) }
if ($exes.Count -eq 0) { Say "RUNNER ERROR no exes in $build"; exit 3 }
Say "RUNNER start: $($exes.Count) areas, area timeout ${TimeoutSec}s"
$rc = 0
foreach ($e in $exes) {
  $area = $e.BaseName
  Say "== AREA $area"
  $out = Join-Path $res ($area + '.txt')
  $err = Join-Path $res ($area + '.err')
  Remove-Item -Force -ErrorAction SilentlyContinue $out, $err, (Join-Path $res ($area + '.tsv')), (Join-Path $res ($area + '.tsv.cur'))
  $st = 'ERROR'; $code = -1
  try {
    $p = Start-Process -FilePath $e.FullName -WorkingDirectory $Root -RedirectStandardOutput $out -RedirectStandardError $err -PassThru
    $t0 = Get-Date
    $pending = ''
    $buf = New-Object char[] 4096
    $sr = $null
    $timedOut = $false
    while ($true) {
      if (-not $sr -and (Test-Path $out)) {
        try { $fs = [IO.File]::Open($out, 'Open', 'Read', 'ReadWrite,Delete'); $sr = New-Object IO.StreamReader($fs) } catch { $sr = $null }
      }
      $exited = $p.HasExited
      if ($sr) {
        while (($n = $sr.Read($buf, 0, $buf.Length)) -gt 0) {
          $pending += [string]::new($buf, 0, $n)
          while (($i = $pending.IndexOf("`n")) -ge 0) { Say ($pending.Substring(0, $i).TrimEnd("`r")); $pending = $pending.Substring($i + 1) }
        }
      }
      if ($exited) { break }
      if (((Get-Date) - $t0).TotalSeconds -gt $TimeoutSec) { try { $p.Kill() } catch {}; $timedOut = $true; break }
      Start-Sleep -Milliseconds 250
    }
    if ($pending) { Say $pending }
    if ($sr) { $sr.Close() }
    if ($timedOut) { $st = 'TIMEOUT'; $code = -1 } else { $st = 'OK'; $code = $p.ExitCode }
  } catch { Say "  STARTFAIL: $($_.Exception.Message)" }
  Say "AREA_STATUS $area $st $code"
  if (Test-Path $err) { foreach ($l in @(Get-Content $err)) { Say "  STDERR: $l" } }
  $tsv = Join-Path $res ($area + '.tsv')
  if (Test-Path $tsv) { foreach ($l in @(Get-Content $tsv)) { Say "TSV $area $l" } }
  $hasSummary = (Test-Path $out) -and (Select-String -Path $out -Pattern '^SUMMARY ' -Quiet)
  if (-not $hasSummary -or $st -ne 'OK') { $rc = 1 }
}
Say "RUNNER done rc=$rc"
exit $rc
