# Builds the distributable package for 125A Dystorator.
# Run from the project root after build.bat:  powershell -ExecutionPolicy Bypass -File installer\make_package.ps1
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$version  = "0.1.0"
$root     = Split-Path -Parent $PSScriptRoot
$build    = Join-Path $env:USERPROFILE "vab-build\Dystorator_artefacts\Release"
$dist     = Join-Path $root "dist"
$stageDir = Join-Path $dist "125A-Dystorator-$version-Windows"
$iscc     = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") |
            Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup (ISCC.exe) not found" }

# Ensure UTF-8 with BOM for Inno Setup text parsing
$utf8Bom = New-Object System.Text.UTF8Encoding($true)
foreach ($f in "125A-Dystorator.iss", "README.txt") {
    $p = Join-Path $PSScriptRoot $f
    [IO.File]::WriteAllText($p, [IO.File]::ReadAllText($p), $utf8Bom)
}

# Zip a folder with standard "/" entry separators and UTF-8 names
function New-Zip ([string] $sourceDir, [string] $zipPath, [bool] $includeBaseDir) {
    if (Test-Path $zipPath) { Remove-Item $zipPath }
    $base = if ($includeBaseDir) { Split-Path $sourceDir } else { $sourceDir }
    $zip = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create, [Text.Encoding]::UTF8)
    try {
        Get-ChildItem $sourceDir -Recurse -File | ForEach-Object {
            $name = $_.FullName.Substring($base.Length).TrimStart('\').Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $name, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $zip.Dispose() }
}

if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory -Force $stageDir, "$stageDir\Portable", "$stageDir\source" | Out-Null

# 1) installer
& $iscc /Qp (Join-Path $PSScriptRoot "125A-Dystorator.iss")
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }
Copy-Item (Join-Path $dist "125A-Dystorator-Setup-$version.exe") $stageDir

# 2) portable
Copy-Item (Join-Path $build "Standalone\125A Dystorator.exe") "$stageDir\Portable"
Copy-Item (Join-Path $build "VST3\125A Dystorator.vst3") "$stageDir\Portable" -Recurse

# 3) docs
Copy-Item (Join-Path $PSScriptRoot "README.txt") (Join-Path $stageDir "README.txt")
Copy-Item (Join-Path $PSScriptRoot "THIRD-PARTY-NOTICES.txt") $stageDir
Copy-Item (Join-Path $root "LICENSE") $stageDir

# 4) source code (required by the AGPL/GPL licenses of JUCE and the ASIO SDK)
$srcStage = Join-Path $dist "src-stage\125A-Dystorator-source-$version"
if (Test-Path (Split-Path $srcStage)) { Remove-Item (Split-Path $srcStage) -Recurse -Force }
New-Item -ItemType Directory -Force $srcStage | Out-Null
foreach ($item in "CMakeLists.txt", "build.bat", "README.md", "Source", "tests", "docs", "spice", "installer") {
    Copy-Item (Join-Path $root $item) $srcStage -Recurse
}
# LTspice runs leave waveform / database files next to the netlists (can be GBs); ship the netlists only
Get-ChildItem (Join-Path $srcStage "spice") -Include *.raw, *.db -Recurse | Remove-Item
New-Zip $srcStage "$stageDir\source\125A-Dystorator-source-$version.zip" $true
Remove-Item (Split-Path $srcStage) -Recurse -Force

# 5) final zip
$zip = Join-Path $dist "125A-Dystorator-$version-Windows.zip"
New-Zip $stageDir $zip $true

Get-Item $zip, (Join-Path $dist "125A-Dystorator-Setup-$version.exe") | ForEach-Object { "{0}  {1:N1} MB" -f $_.Name, ($_.Length / 1MB) }
