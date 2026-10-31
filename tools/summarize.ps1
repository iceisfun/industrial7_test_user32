param([string]$Dir = "results")
$tot = @{t=0;f=0;s=0;cp=0;cf=0}
"{0,-20} {1,6} {2,6} {3,6} {4,8} {5,8}  exit" -f "area","tests","failed","skip","chk_ok","chk_fail"
foreach ($f in Get-ChildItem $Dir -Filter *.txt | Sort-Object Name) {
  $txt = Get-Content $f.FullName
  $s = $txt | Where-Object { $_ -match '^SUMMARY ' } | Select-Object -Last 1
  $ex = ($txt | Where-Object { $_ -match '^exit=' } | Select-Object -Last 1)
  if ($s -match 'tests=(\d+) failed=(\d+) skipped=(\d+) checks_pass=(\d+) checks_fail=(\d+)') {
    "{0,-20} {1,6} {2,6} {3,6} {4,8} {5,8}  {6}" -f $f.BaseName,$matches[1],$matches[2],$matches[3],$matches[4],$matches[5],$ex
    $tot.t += [int]$matches[1]; $tot.f += [int]$matches[2]; $tot.s += [int]$matches[3]; $tot.cp += [int]$matches[4]; $tot.cf += [int]$matches[5]
  } else { "{0,-20} NO SUMMARY (crash/hang?)  {1}" -f $f.BaseName,$ex }
  $txt | Where-Object { $_ -match '^FAIL ' } | ForEach-Object { "    $_" }
}
"{0,-20} {1,6} {2,6} {3,6} {4,8} {5,8}" -f "TOTAL",$tot.t,$tot.f,$tot.s,$tot.cp,$tot.cf
