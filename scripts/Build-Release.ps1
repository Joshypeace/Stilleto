<#
.SYNOPSIS
    Builds Stilleto for distribution: the Windows installer (MSI) and a portable zip.

.DESCRIPTION
    1. Configures and builds the Release app with the vs2026 preset (test hooks OFF).
    2. Verifies the executable does not contain the end-to-end test hook.
    3. Ensures models\ggml-base.en.bin exists and matches the upstream SHA-1 (downloads it if missing).
    4. Produces, in dist\:
         Stilleto-<version>-Setup.msi      installer (Program Files, Start Menu, bundled model)
         Stilleto-<version>-Portable.zip   unzip-and-run copy (exe + models\ + Quick Start)
       and prints SHA-256 hashes of both.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\Build-Release.ps1
    powershell -ExecutionPolicy Bypass -File scripts\Build-Release.ps1 -Preset vs2022 -SkipBuild
#>
param(
    [string]$Preset = "vs2026",
    [string]$WixDir = "${env:ProgramFiles(x86)}\WiX Toolset v3.14\bin",
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $root

function Step([string]$text) { Write-Host "`n== $text" -ForegroundColor Yellow }

# Version from CMakeLists.txt: project(Stilleto VERSION x.y.z ...)
$version = ([regex]::Match((Get-Content CMakeLists.txt -Raw), 'project\(Stilleto VERSION (\d+\.\d+\.\d+)')).Groups[1].Value
if (-not $version) { throw 'Could not read the version from CMakeLists.txt' }
Write-Host "Stilleto $version"

$buildDir = Join-Path $root "out\build\$Preset"
$exe = Join-Path $buildDir 'Stilleto_artefacts\Release\Stilleto.exe'

if (-not $SkipBuild) {
    Step "Configuring ($Preset) with test hooks OFF"
    cmake --preset $Preset -DSTILLETO_TEST_HOOKS=OFF
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }

    Step 'Building Release'
    cmake --build $buildDir --config Release --target Stilleto --parallel 2
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
}

if (-not (Test-Path $exe)) { throw "Executable not found: $exe" }

Step 'Verifying the executable'
$bytes = [IO.File]::ReadAllBytes($exe)
$ascii = [Text.Encoding]::ASCII.GetString($bytes)
if ($ascii.Contains('STILLETO_TEST_AUDIO')) { throw 'This build contains the end-to-end test hook. Rebuild with -DSTILLETO_TEST_HOOKS=OFF.' }
Write-Host 'OK: no test hook; static MSVC runtime (no redistributable needed).'

Step 'Checking the bundled model'
$model = Join-Path $root 'models\ggml-base.en.bin'
$expectedSha1 = '137c40403d78fd54d454da0f9bd998f78703390c'
if (-not (Test-Path $model)) {
    Write-Host 'Downloading ggml-base.en.bin (142 MB) from huggingface.co ...'
    New-Item -ItemType Directory -Force (Split-Path $model) | Out-Null
    Invoke-WebRequest 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin' -OutFile $model
}
$sha1 = (Get-FileHash $model -Algorithm SHA1).Hash.ToLower()
if ($sha1 -ne $expectedSha1) { throw "Model checksum mismatch ($sha1). Delete $model and run again." }
Write-Host 'OK: ggml-base.en.bin matches the published SHA-1.'

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null
$quickStart = Join-Path $root 'installer\QuickStart.html'

Step 'Portable zip'
$stage = Join-Path $dist "Stilleto-$version-Portable"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $stage 'models') | Out-Null
Copy-Item $exe $stage
Copy-Item $model (Join-Path $stage 'models')
Copy-Item $quickStart (Join-Path $stage 'Stilleto Quick Start.html')
$zip = Join-Path $dist "Stilleto-$version-Portable.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Remove-Item $stage -Recurse -Force

Step 'Installer (WiX)'
foreach ($tool in 'candle.exe', 'light.exe') {
    if (-not (Test-Path (Join-Path $WixDir $tool))) { throw "WiX Toolset v3 not found in '$WixDir'. Install it from https://wixtoolset.org or pass -WixDir." }
}
$obj = Join-Path $root "out\installer"
New-Item -ItemType Directory -Force $obj | Out-Null
$wixobj = Join-Path $obj 'Stilleto.wixobj'
$msi = Join-Path $dist "Stilleto-$version-Setup.msi"

& (Join-Path $WixDir 'candle.exe') -nologo -arch x64 -ext WixUIExtension -ext WixUtilExtension `
    "-dVersion=$version" "-dAppExe=$exe" "-dModelFile=$model" "-dQuickStart=$quickStart" `
    "-dLicenseRtf=$(Join-Path $root 'installer\License.rtf')" -out $wixobj (Join-Path $root 'installer\Stilleto.wxs')
if ($LASTEXITCODE -ne 0) { throw 'candle (WiX compile) failed' }

& (Join-Path $WixDir 'light.exe') -nologo -spdb -ext WixUIExtension -ext WixUtilExtension -cultures:en-us `
    -sice:ICE61 -sice:ICE63 -out $msi $wixobj   # ICE61: AllowSameVersionUpgrades; ICE63: closing the tray app before RemoveExistingProducts. Both intended.
if ($LASTEXITCODE -ne 0) { throw 'light (WiX link) failed' }

Step 'Done'
foreach ($f in $msi, $zip) {
    $item = Get-Item $f
    $hash = (Get-FileHash $f -Algorithm SHA256).Hash
    Write-Host ("{0}  {1,8:N1} MB  SHA256 {2}" -f $item.Name, ($item.Length / 1MB), $hash)
}
