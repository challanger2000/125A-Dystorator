param(
    [Parameter(Mandatory=$true)][string]$JuceDir,
    [Parameter(Mandatory=$true)][string]$SdkDir,
    [Parameter(Mandatory=$true)][string]$TesterDir,
    [Parameter(Mandatory=$true)][string]$DetoursDir,
    [Parameter(Mandatory=$true)][string]$EvidenceDir,
    [ValidateSet('full','cpu')][string]$Scope = 'full'
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = (Get-Location).Path
$sourceCommit = (& git rev-parse HEAD).Trim()
$baselineCommit = '398204e26d84c3f40bcd1c0af55dfa8e97893ca7'
$branch = (& git branch --show-current).Trim()
if ($branch -ne 'dev/compact-stereo-on-ir-fix-20261026') { throw 'Wrong QA branch' }
& git merge-base --is-ancestor $baselineCommit HEAD
if ($LASTEXITCODE -ne 0) { throw 'Pinned baseline is not an ancestor' }
New-Item -ItemType Directory -Force $EvidenceDir | Out-Null
$EvidenceDir = [IO.Path]::GetFullPath($EvidenceDir)
$checks = [System.Collections.Generic.List[object]]::new()
$notRun = @('Actual owner i9-11900K machine', 'Real ASIO device/DAW/Studio One playback and recorded dropouts', 'Licensed real DI and level-matched listening', 'Musical approval of ECO/STANDARD', 'Manual GUI/control acceptance')
$infraFailure = $null
$functionalReference = '8a442d9d2a3ad10ba331b7cc849136db78ca08ac'
function Check-Native([string]$name, [string]$exe, [string[]]$arguments, [int]$expectedExit = 0) {
    $file = Join-Path $EvidenceDir ($name + '.txt')
    ('COMMAND: ' + $exe + ' ' + ($arguments | ConvertTo-Json -Compress)) | Set-Content $file -Encoding utf8
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        & $exe @arguments *>> $file
        $code = $LASTEXITCODE
        if ($null -eq $code) { throw 'Native exit code absent' }
        $status = if ($code -eq $expectedExit) { 'PASS' } else { 'FAIL' }
        $detail = ''
    } catch {
        $code = -1; $status = 'FAIL'; $detail = $_.Exception.Message
        $detail | Add-Content $file -Encoding utf8
    }
    $watch.Stop()
    $checks.Add([ordered]@{name=$name;status=$status;exitCode=$code;expectedExit=$expectedExit;seconds=$watch.Elapsed.TotalSeconds;log=[IO.Path]::GetFileName($file);detail=$detail})
    Write-Host "$status $name exit=$code expected=$expectedExit"
    return ($status -eq 'PASS')
}
function Require-Build([string]$name, [string[]]$arguments) {
    if (-not (Check-Native $name 'cmake' $arguments)) { throw "Required build/configure failed: $name" }
}
function Configure-Integration([string]$qaMain, [bool]$trace = $false) {
    $traceFlag = if ($trace) { 'ON' } else { 'OFF' }
    Require-Build ('configure-' + $qaMain.Replace('.cpp','')) @('-S','tests/ir_presets','-B','build/win-integration','-G','Visual Studio 17 2022','-A','x64',
        "-DJUCE_SOURCE_DIR=$JuceDir", "-DIR_PRODUCT_SOURCE_DIR=$root/Source", "-DIR_TEST_MAIN=$qaMain", "-DIR_TRACE_CONVOLUTION=$traceFlag",
        '-DIR_DISCARD_RIGHT_MUTATION=OFF','-DIR_BUILD_VST3_HOST=ON',"-DSTEP3_DETOURS_DIR=$DetoursDir",'-DCMAKE_BUILD_TYPE=Release')
    $targets = if ($Scope -eq 'cpu') { @('compact_vst3_host') } else { @('ir_presets_test','compact_vst3_host') }
    Require-Build ('build-' + $qaMain.Replace('.cpp','')) (@('--build','build/win-integration','--config','Release','--target') + $targets + @('--parallel','2'))
}
try {
    $machine = Get-CimInstance Win32_Processor | Select-Object -Property @('Name','NumberOfCores','NumberOfLogicalProcessors','MaxClockSpeed')
    [ordered]@{sourceCommit=$sourceCommit;baselineCommit=$baselineCommit;branch=$branch;utc=[DateTime]::UtcNow.ToString('o');runner=$env:RUNNER_NAME;runnerOs=$env:RUNNER_OS;cpu=$machine;os=[Environment]::OSVersion.VersionString;
        juceCommit=(& git -C $JuceDir rev-parse HEAD).Trim();sdkCommit=(& git -C $SdkDir rev-parse HEAD).Trim();testerCommit=(& git -C $TesterDir rev-parse HEAD).Trim();detoursCommit=(& git -C $DetoursDir rev-parse HEAD).Trim();
        scope=$Scope;functionalReference=$functionalReference;hardwareQualification='Windows shared CI runner, NOT owner i9-11900K/ASIO';NOT_RUN=$notRun} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $EvidenceDir 'metadata.json') -Encoding utf8
    if ((& git -C $JuceDir rev-parse HEAD).Trim() -ne '51d11a2be6d5c97ccf12b4e5e827006e19f0555a') { throw 'JUCE pin mismatch' }
    if ((& git -C $SdkDir rev-parse HEAD).Trim() -ne '3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96') { throw 'SDK pin mismatch' }
    if ((& git -C $TesterDir rev-parse HEAD).Trim() -ne '3dcf3fe607008e520dcf20d1f793d9c534a9b9e1') { throw 'Tester pin mismatch' }
    if ((& git -C $DetoursDir rev-parse HEAD).Trim() -ne 'e4bfd6b03e50de46b47abfbd1e46b384f0c5f833') { throw 'Detours pin mismatch' }

    if ($Scope -eq 'cpu') {
        & git diff --quiet $functionalReference HEAD '--' 'Source' 'CMakeLists.txt'
        if ($LASTEXITCODE -ne 0) { throw 'CPU-only requires byte-identical product Source/CMake to the Windows functional reference' }
        $referenceFile = "docs/qa/step3-windows-$functionalReference/results.json"
        $reference = Get-Content $referenceFile -Raw | ConvertFrom-Json
        if ($reference.sourceCommit -ne $functionalReference) { throw 'Wrong functional evidence source' }
        $functionalGates = @('build-plugin','native-ctest','tone-regression','fault-regression','ir-preset-ownership-regressions','quality-modes','quality-pcm','quality-stress','step3_pcm','mono_reference','stereo_quality_reference','step3_alias','large_ir_heap','packaged-vst3-host','tester-job-object-selftest','steinberg-validator-selftest','125a-tester-product')
        foreach ($name in $functionalGates) {
            $gate = @($reference.checks | Where-Object { $_.name -eq $name -and $_.status -eq 'PASS' })
            if ($gate.Count -ne 1) { throw "No verified prior functional gate: $name" }
        }
        # Preserve the actual product tree identity, not just a handwritten claim.
        $beforeTree = (& git rev-parse "${functionalReference}:Source").Trim()
        $afterTree = (& git rev-parse 'HEAD:Source').Trim()
        if ($beforeTree -ne $afterTree) { throw 'Source tree identity differs from functional evidence' }
        [ordered]@{referenceCommit=$functionalReference;referenceEvidence=$referenceFile;sourceTree=$afterTree;status='PASS';detail='Source and root CMake identical; prior functional checks remain separately attributed'} | ConvertTo-Json | Set-Content (Join-Path $EvidenceDir 'functional-reference.json') -Encoding utf8
    }

    New-Item -ItemType Directory -Force build/win-before | Out-Null
    if (-not (Check-Native 'archive-baseline' 'git' @('archive',$baselineCommit,'--output=build/win-before.tar'))) { throw 'Cannot extract pinned baseline' }
    if (-not (Check-Native 'extract-baseline' 'tar' @('-xf','build/win-before.tar','-C','build/win-before'))) { throw 'Cannot extract pinned baseline archive' }
    Require-Build 'configure-before-plugin' @('-S','build/win-before','-B','build/win-plugin-before','-G','Visual Studio 17 2022','-A','x64',"-DFETCHCONTENT_SOURCE_DIR_JUCE=$JuceDir")
    Require-Build 'build-before-plugin' @('--build','build/win-plugin-before','--config','Release','--target','DjentyDonkey_VST3','--parallel','2')
    Require-Build 'configure-plugin' @('-S','.','-B','build/win-plugin','-G','Visual Studio 17 2022','-A','x64',"-DFETCHCONTENT_SOURCE_DIR_JUCE=$JuceDir")
    $productTargets = if ($Scope -eq 'cpu') { @('DjentyDonkey_VST3') } else { @('DjentyDonkey_VST3','dystorator_qa_suite','tone_regress','dsp_test') }
    Require-Build 'build-plugin' (@('--build','build/win-plugin','--config','Release','--target') + $productTargets + @('--parallel','2'))
    if ($Scope -eq 'full') { $null = Check-Native 'native-ctest' 'ctest' @('--test-dir','build/win-plugin','-C','Release','-V') }
    Configure-Integration 'ir_presets_test.cpp' ($Scope -eq 'full')
    $integrationExe = "$root/build/win-integration/ir_presets_test_artefacts/Release/ir_presets_test.exe"
    $hostExe = "$root/build/win-integration/compact_vst3_host_artefacts/Release/compact_vst3_host.exe"
    $plugin = "$root/build/win-plugin/DjentyDonkey_artefacts/Release/VST3/Djenty Donkey.vst3"
    $beforePlugin = "$root/build/win-plugin-before/DjentyDonkey_artefacts/Release/VST3/Djenty Donkey.vst3"
    # Fail early on broken instrumentation/host loading. These are required
    # functional gates, unlike measured CPU deadline failures retained below.
    $heapValid = Check-Native 'standalone-heap-selftest' $hostExe @('step3-heap-selftest')
    if (-not $heapValid) { throw 'Required Windows heap observer self-test failed; inspect fatal diagnostics' }
    $hostValid = Check-Native 'packaged-vst3-host' $hostExe @($plugin,'step3')
    if (-not $hostValid) { throw 'Required packaged VST3 host gate failed; inspect fatal/PCM/heap diagnostics' }
    if ($Scope -eq 'full') {
    $null = Check-Native 'tone-regression' "$root/build/win-plugin/Release/tone_regress.exe" @()
    $null = Check-Native 'fault-regression' "$root/build/win-plugin/Release/dsp_test.exe" @('fault')
    $null = Check-Native 'ir-preset-ownership-regressions' $integrationExe @()
    foreach ($qaMain in @('quality_audio.cpp','step3_pcm.cpp','mono_reference.cpp','stereo_quality_reference.cpp','step3_alias.cpp','large_ir_heap.cpp')) {
        Configure-Integration $qaMain
        if ($qaMain -eq 'quality_audio.cpp') {
            foreach ($case in @('modes','pcm','stress')) { $null = Check-Native ('quality-' + $case) $integrationExe @($case) }
        } else { $null = Check-Native ($qaMain.Replace('.cpp','')) $integrationExe @() }
    }
    }
    if (-not (Check-Native 'cpu-setup-before' $hostExe @($beforePlugin,'step3-cpu-setup'))) { throw 'Before CPU fixture failed actual component-state preflight' }
    if (-not (Check-Native 'cpu-setup-after' $hostExe @($plugin,'step3-cpu-setup'))) { throw 'After CPU fixture failed actual component-state preflight' }
    # Same host executable, complete before/after Release VST3, same flags and runner.
    # A single deadline miss is FAIL; preserve exit 1 rather than accepting it as PASS.
    $null = Check-Native 'cpu-before' $hostExe @($beforePlugin,'step3-cpu','before')
    $null = Check-Native 'cpu-after' $hostExe @($plugin,'step3-cpu','after')

    if ($Scope -eq 'full') {
    Require-Build 'configure-125a-tester' @('-S',$TesterDir,'-B','build/win-tester','-G','Visual Studio 17 2022','-A','x64',"-DFETCHCONTENT_SOURCE_DIR_VST3SDK=$SdkDir",'-DSMTG_CREATE_PLUGIN_LINK=OFF')
    $testerTargets = @('PluginTester125ADispatcher','PluginTester125ALauncher','PluginTester125AWorker','PluginTester125AReloadProbe','PluginTester125AStateProbe','PluginTester125AEditorLifecycleProbe','PluginTester125AIOEventProbe','PluginTester125AValidatorWrapper','PluginTester125AGuardSelfTest','validator','PluginTester125AQAGoodControl','PluginTester125AQABadLifecycle','PluginTester125AQABadNaN','PluginTester125AQABadState','PluginTester125AQABadIO','PluginTester125AQAGoodEditor','PluginTester125AQABadEditor')
    Require-Build 'build-125a-tester' (@('--build','build/win-tester','--config','Release','--target') + $testerTargets + @('--parallel','2'))
    $tester = "$root/build/win-tester/Release/125A_Plugin_Tester.exe"
    $validator = "$root/build/win-tester/Release/125A_Plugin_Tester_SteinbergValidator.exe"
    $guard = "$root/build/win-tester/Release/125A_Plugin_Tester_GuardSelfTest.exe"
    $testerValid = Check-Native 'tester-job-object-selftest' $tester @('--guard-selftest',$guard)
    $testerValid = (Check-Native 'steinberg-validator-selftest' $validator @('-selftest')) -and $testerValid
    [Environment]::SetEnvironmentVariable('125A_PLUGIN_TESTER_REPORT_DIR', (Join-Path $EvidenceDir 'tester-selftest'))
    foreach ($fixtureName in @('GoodControl','BadLifecycle','BadNaN','BadState','BadIO','GoodEditor','BadEditor')) {
        $fixture = Get-ChildItem build/win-tester -Recurse -Directory -Filter "125A_QA_$fixtureName.vst3" | Select-Object -First 1
        if (-not $fixture) { throw "Missing tester regression fixture $fixtureName" }
        $expected = if ($fixtureName.StartsWith('Good')) { 0 } else { 1 }
        $testerValid = (Check-Native ('tester-fixture-' + $fixtureName) $tester @($fixture.FullName) $expected) -and $testerValid
        $expectedText = @{
            GoodControl='0 WARNING / 0 FAIL'; BadLifecycle='[FAIL] Audio torture processing state (32-bit) - setProcessing(true) failed';
            BadNaN='[FAIL] Audio Silence process (32-bit)'; BadState='[FAIL] Component state roundtrip - setState rejected the state returned by getState';
            BadIO='[FAIL] I/O, sidechain and event isolation'; GoodEditor='[PASS] Editor lifecycle / GUI runtime';
            BadEditor='last reached stage: class 125A QA Bad Editor - cycle 2 - removed'
        }
        $fixtureReports = Get-ChildItem ([Environment]::GetEnvironmentVariable('125A_PLUGIN_TESTER_REPORT_DIR')) -File -Filter "125A_QA_${fixtureName}_*125A_QA_Report.txt"
        $oracle = $false
        foreach ($reportFile in $fixtureReports) {
            if ((Get-Content $reportFile.FullName -Raw).Contains($expectedText[$fixtureName])) { $oracle=$true }
        }
        $oracleStatus=if($oracle){'PASS'}else{'FAIL'}
        $checks.Add([ordered]@{name=('tester-fixture-oracle-'+$fixtureName);status=$oracleStatus;requiredText=$expectedText[$fixtureName]})
        $testerValid=$oracle -and $testerValid
    }
    [Environment]::SetEnvironmentVariable('125A_PLUGIN_TESTER_REPORT_DIR', (Join-Path $EvidenceDir 'tester-product'))
    $null = Check-Native '125a-tester-product' $tester @($plugin)
    [ordered]@{selfValidation=if($testerValid){'PASS'}else{'FAIL'};productInterpretation=if($testerValid){'See product report and separate validator diagnostics'}else{'INCONCLUSIVE: tester self-validation failed'}} | ConvertTo-Json | Set-Content (Join-Path $EvidenceDir 'tester-validity.json') -Encoding utf8
    }
    Get-ChildItem "$root/build/win-plugin/DjentyDonkey_artefacts/Release/VST3" -Recurse -File | ForEach-Object { Get-FileHash $_.FullName -Algorithm SHA256 } | Format-List | Out-File (Join-Path $EvidenceDir 'product-artifact-sha256.txt') -Encoding utf8
} catch {
    $infraFailure = $_.Exception.ToString()
    $infraFailure | Set-Content (Join-Path $EvidenceDir 'infrastructure-failure.txt') -Encoding utf8
    Write-Error $infraFailure -ErrorAction Continue
 } finally {
    $required = @('build-before-plugin','build-plugin','native-ctest','tone-regression','fault-regression','ir-preset-ownership-regressions','quality-modes','quality-pcm','quality-stress','step3_pcm','mono_reference','stereo_quality_reference','step3_alias','large_ir_heap','standalone-heap-selftest','packaged-vst3-host','cpu-setup-before','cpu-setup-after','cpu-before','cpu-after','tester-job-object-selftest','steinberg-validator-selftest','125a-tester-product')
    foreach ($name in $required) {
        if (-not @($checks | Where-Object { $_.name -eq $name }).Count) {
            $checks.Add([ordered]@{name=$name;status='NOT RUN';detail=if ($Scope -eq 'cpu' -and -not $infraFailure) { 'NOT RUN in CPU-only scope; prior functional evidence linked separately' } else { 'Not reached after infrastructure/build failure' };exitCode=$null})
        }
    }
    [ordered]@{sourceCommit=$sourceCommit;baselineCommit=$baselineCommit;scope=$Scope;functionalReference=$functionalReference;checks=$checks;NOT_RUN=$notRun;infrastructureFailure=$infraFailure;release='DEVELOPMENT ONLY - NO FINAL'} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $EvidenceDir 'results.json') -Encoding utf8
}
if ($infraFailure -or @($checks | Where-Object { $_.status -eq 'FAIL' }).Count -gt 0) { exit 1 }
exit 0
