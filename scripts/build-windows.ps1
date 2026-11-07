# InfoTAB Windows 11: fix esptool 5.0.0 / Click 8.2+ and compile firmware.
# Only changes the PlatformIO private Python environment. Does NOT flash COM6.
$ErrorActionPreference = 'Stop'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

if (-not (Test-Path $python) -or -not (Test-Path $pio)) {
    throw 'PlatformIO IDE is not installed in the expected Windows user directory.'
}
if (-not (Test-Path (Join-Path $project 'platformio.ini'))) {
    throw 'Cannot find InfoTAB platformio.ini. Check repository checkout.'
}

Write-Host '[InfoTAB] Configuring Click 8.1.8 in PlatformIO private Python...'
& $python -m pip install 'click==8.1.8'
if ($LASTEXITCODE -ne 0) { throw 'pip failed to install Click 8.1.8' }

Write-Host '[InfoTAB] Verifying Click installation...'
& $python -m pip show click
if ($LASTEXITCODE -ne 0) { throw 'Click verification failed' }

Push-Location $project
try {
    Write-Host '[InfoTAB] Building ESP32-P4 firmware (no upload)...'
    & $pio run -e infotab_tab5
    if ($LASTEXITCODE -ne 0) { throw "InfoTAB Build failed (exit code $LASTEXITCODE)" }
    $firmware = Join-Path $project '.pio\build\infotab_tab5\firmware.bin'
    if (-not (Test-Path -LiteralPath $firmware)) { throw 'Compiled firmware.bin not found.' }
    $binarySize = (Get-Item -LiteralPath $firmware).Length
    $slotLimit = 1310720
    if ($binarySize -gt $slotLimit) {
        throw "Firmware $binarySize B exceeds the current 1280KiB OTA slot. No upload."
    }
    Write-Host "[InfoTAB] Firmware size: $binarySize B / $slotLimit B (current OTA slot)"
    Write-Host '[InfoTAB] BUILD SUCCESS. Upload was NOT executed.'
}
finally {
    Pop-Location
}
