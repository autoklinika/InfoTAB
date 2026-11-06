# InfoTAB — Windows 11 read-only 16MB partition candidate verification.
# Builds an alternate PlatformIO environment and parses its local build output.
# Does NOT upload, erase, read or write connected Tab5 flash.
$ErrorActionPreference = 'Stop'

$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$audit = Join-Path $PSScriptRoot 'audit-partitions.py'

foreach ($item in @($python, $pio, $audit, (Join-Path $root 'platformio.ini'))) {
    if (-not (Test-Path $item)) { throw "Missing required file: $item" }
}

Write-Host '[InfoTAB] Installing compatible Click only in the private PlatformIO Python...'
& $python -m pip install 'click==8.1.8'
if ($LASTEXITCODE -ne 0) { throw 'Click installation failed' }

Push-Location $root
try {
    Write-Host '[InfoTAB] BUILD 16MB CANDIDATE (no upload)...'
    & $pio run -e infotab_tab5_16mb_audit
    if ($LASTEXITCODE -ne 0) { throw '16MB candidate Build failed' }

    Write-Host '[InfoTAB] Auditing generated 16MB partitions.bin...'
    & $python $audit --env infotab_tab5_16mb_audit
    if ($LASTEXITCODE -ne 0) { throw '16MB candidate partition audit FAILED' }

    Write-Host '[InfoTAB] 16MB BUILD + AUDIT PASS. USB UPLOAD WAS NOT EXECUTED.'
    Write-Host '[InfoTAB] Existing InfoTAB Wi-Fi NVS and firmware remain unchanged.'
}
finally { Pop-Location }
