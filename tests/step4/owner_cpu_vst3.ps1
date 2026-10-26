# 125A Dystorator - Windows owner CPU comparison (ABBA); diagnostics only, not ASIO.
# Requires two separately built, source-verified Windows x64 VST3 bundles and the Step3 VST3 host.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$HostExe,
 [Parameter(Mandatory=$true)][string]$BeforeVst3,
 [Parameter(Mandatory=$true)][string]$AfterVst3,
 [Parameter(Mandatory=$true)][string]$OutputDir
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
function Existing([string]$p) {
 if (!(Test-Path -LiteralPath $p)) { throw "Missing input: $p" }
 return (Resolve-Path -LiteralPath $p).ProviderPath
}
function Number([string]$s) {
 $n = [double]::Parse($s, $culture)
 if ([double]::IsInfinity($n) -or [double]::IsNaN($n)) { throw "Nonfinite number: $s" }
 return $n
}
function BinaryInfo([string]$dir) {
 $f = @(Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object { $_.Extension -ieq '.vst3' })
 if ($f.Count -ne 1) { throw "Need exactly one VST3 DLL inside: $dir" }
 return [ordered]@{ bundle=$dir; binary=$f[0].FullName; sha256=(Get-FileHash -LiteralPath $f[0].FullName -Algorithm SHA256).Hash }
}
$HostExe = Existing $HostExe
$BeforeVst3 = Existing $BeforeVst3
$AfterVst3 = Existing $AfterVst3
if (!(Test-Path -LiteralPath $HostExe -PathType Leaf)) { throw 'Host must be an executable file' }
if (!(Test-Path -LiteralPath $BeforeVst3 -PathType Container) -or
    !(Test-Path -LiteralPath $AfterVst3 -PathType Container)) { throw 'VST3 inputs must be bundle directories' }
if ($BeforeVst3 -ieq $AfterVst3) { throw 'Identical input bundle paths' }
if (Test-Path -LiteralPath $OutputDir) {
 if (@(Get-ChildItem -LiteralPath $OutputDir -Force).Count -gt 0) { throw 'Refusing to overwrite existing evidence' }
} else { New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null }
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).ProviderPath
$beforeInfo = BinaryInfo $BeforeVst3
$afterInfo = BinaryInfo $AfterVst3
if ($beforeInfo.sha256 -eq $afterInfo.sha256) { throw 'No binary change: hashes match' }
$metadata = [ordered]@{
 timestampUTC = (Get-Date).ToUniversalTime().ToString('o')
 machine=$env:COMPUTERNAME
 cpu=@(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed)
 windows=(Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber)
 powerPlan=((& powercfg /GETACTIVESCHEME) | Out-String).Trim()
 hostExecutable=$HostExe
 hostSha256=(Get-FileHash -LiteralPath $HostExe -Algorithm SHA256).Hash
 before=$beforeInfo
 after=$afterInfo
 sourceProvenance='NOT VERIFIED by this script; compare binaries to pinned build manifests'
 fixture='48 cases per run, 160 timed callbacks per case'
 disclaimer='Synthetic host wall-time; NOT ASIO, Studio One, or a FINAL CPU/quality approval'
}
$metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDir 'provenance.json') -Encoding UTF8
$expr = '^VST3_CPU version=(?<ver>\S+) fs=(?<fs>\S+) lanes=(?<lanes>\d+) quality=(?<quality>\d+) frames=(?<frames>\d+) mode=(?<mode>\S+) mean_us=(?<mean>\S+) p95_us=(?<p95>\S+) p99_us=(?<p99>\S+) max_us=(?<max>\S+) deadline_us=(?<deadline>\S+) overruns=(?<over>\d+) schedule_misses=(?<late>\d+) left_energy=(?<left>\S+) right_energy=(?<right>\S+) component_state=(?<state>\S+) status=(?<status>\S+)$'
$rows = New-Object 'System.Collections.Generic.List[object]'
$sequence = @('before','after','after','before')
for ($r=0; $r -lt 4; $r++) {
 $role = $sequence[$r]
 $bundle = if ($role -eq 'before') { $BeforeVst3 } else { $AfterVst3 }
 $prefix = ('run-{0:00}-{1}' -f ($r+1),$role)
 $stdout = Join-Path $OutputDir ($prefix + '.stdout.txt')
 $stderr = Join-Path $OutputDir ($prefix + '.stderr.txt')
 Write-Host ("Running {0}" -f $prefix)
 $p = Start-Process -FilePath $HostExe -ArgumentList @(('"' + $bundle + '"'),'step3-cpu',$role) -NoNewWindow -Wait -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 1) { throw "Host error: $prefix exit=$($p.ExitCode)" }
 $lines = @([IO.File]::ReadAllLines($stdout) | Where-Object { $_ -match '^VST3_CPU ' })
 if ($lines.Count -ne 48) { throw "Incomplete CPU matrix: $prefix has $($lines.Count)/48" }
 $seen = @{}
 $failureCount = 0
 foreach ($line in $lines) {
  if ($line -notmatch $expr) { throw "Invalid CPU record: $line" }
  $m = $Matches
  $key = "$($m.fs)/$($m.lanes)/$($m.quality)/$($m.frames)/$($m.mode)"
  if ($seen.ContainsKey($key)) { throw "Repeated case: $prefix/$key" }
  $seen[$key] = $true
  $mean = Number $m.mean
  $deadline = Number $m.deadline
  $left = Number $m.left
  $right = Number $m.right
  $over = [int]$m.over
  $late = [int]$m.late
  $expected = if ($over -gt 0 -or $late -gt 0) { 'FAIL' } else { 'PASS' }
  if ($m.state -ne 'PASS' -or $left -le 1e-8 -or $right -le 1e-8 -or
      $deadline -le 0 -or $mean -le 0 -or $over -gt 160 -or $late -gt 160 -or $m.status -ne $expected) {
      throw "Invalid audio, state or deadline oracle: $prefix/$key"
  }
  if ($m.status -eq 'FAIL') { $failureCount++ }
  $rows.Add([pscustomobject]@{
    run=$r+1; role=$role; caseKey=$key; quality=[int]$m.quality
    meanUs=$mean; p95Us=(Number $m.p95); p99Us=(Number $m.p99)
    maxUs=(Number $m.max); deadlineUs=$deadline; overruns=$over
    scheduleMisses=$late; leftEnergy=$left; rightEnergy=$right; status=$m.status
  })
 }
 if (($p.ExitCode -eq 1) -ne ($failureCount -gt 0)) { throw "Host exit/status inconsistency: $prefix" }
}
$rows | Export-Csv -LiteralPath (Join-Path $OutputDir 'all-192-cases.csv') -NoTypeInformation -Encoding UTF8
$comparison = New-Object 'System.Collections.Generic.List[object]'
$groups = @($rows | Group-Object caseKey)
if ($groups.Count -ne 48) { throw "Incomplete unique test coverage: $($groups.Count)/48" }
foreach ($group in $groups) {
 $b = @($group.Group | Where-Object { $_.role -eq 'before' })
 $a = @($group.Group | Where-Object { $_.role -eq 'after' })
 if ($b.Count -ne 2 -or $a.Count -ne 2) { throw "Unbalanced matrix: $($group.Name)" }
 $bm = ($b[0].meanUs + $b[1].meanUs)/2.0
 $am = ($a[0].meanUs + $a[1].meanUs)/2.0
 $comparison.Add([pscustomobject]@{
  caseKey=$group.Name; quality=$b[0].quality
  beforeMeanUs=$bm; afterMeanUs=$am
  meanSavingPercent=(100.0 * ($bm-$am)/$bm)
  beforeRepeatDriftPercent=(100.0 * [math]::Abs($b[0].meanUs-$b[1].meanUs)/$bm)
  afterRepeatDriftPercent=(100.0 * [math]::Abs($a[0].meanUs-$a[1].meanUs)/$am)
  beforeFailedRuns=@($b | Where-Object { $_.status -eq 'FAIL' }).Count
  afterFailedRuns=@($a | Where-Object { $_.status -eq 'FAIL' }).Count
 })
}
$comparison | Export-Csv -LiteralPath (Join-Path $OutputDir 'paired-abba.csv') -NoTypeInformation -Encoding UTF8
$summary = [ordered]@{
 beforeDeadlineFailures=@($rows | Where-Object { $_.role -eq 'before' -and $_.status -eq 'FAIL' }).Count
 afterDeadlineFailures=@($rows | Where-Object { $_.role -eq 'after' -and $_.status -eq 'FAIL' }).Count
 meanSavingByQuality=@{}
 note='ABBA test, two repetitions per role; inspect repeat drift, overruns, p95/p99/max; NOT ASIO'
}
foreach ($quality in @(0,1,2)) {
 $selected = @($comparison | Where-Object { $_.quality -eq $quality })
 $summary.meanSavingByQuality["$quality"] = ($selected | Measure-Object -Property meanSavingPercent -Average).Average
}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDir 'summary.json') -Encoding UTF8
Write-Host ("192 case records saved in: {0}" -f $OutputDir)
