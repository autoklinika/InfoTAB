# InfoTAB — pre-migration read-only health and security checks.
# Does NOT write, erase, upload, export NVS or change the partition table.
# Private flash backup stays on Windows. Only metadata is printed.
param([string]$Port = 'COM6')

$ErrorActionPreference = 'Stop'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$esptool = Join-Path $env:USERPROFILE '.platformio\packages\tool-esptoolpy\esptool.py'
$backupVerifier = Join-Path $PSScriptRoot 'verify-flash-backup.py'
$layoutVerifier = Join-Path $PSScriptRoot 'preflight-migration.py'
$buildRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$candidate = Join-Path $buildRoot '.pio\build\infotab_tab5_16mb_audit\partitions.bin'
$backupFolder = Join-Path $env:LOCALAPPDATA 'InfoTAB\FlashBackups'

if ($Port -notmatch '^COM[0-9]+$') { throw 'Invalid serial port (expected COM6).' }
foreach ($required in @($python, $esptool, $backupVerifier, $layoutVerifier, $candidate)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing required file: $required" }
}
if (-not (Test-Path -LiteralPath $backupFolder)) { throw 'Private backup directory not found.' }
$backup = Get-ChildItem -LiteralPath $backupFolder -Filter 'InfoTAB-flash-16MiB-*.bin' -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($null -eq $backup) { throw 'No verified full-flash backup was found.' }

Write-Host '[InfoTAB] MIGRATION PREFLIGHT (READ ONLY)'
Write-Host '[InfoTAB] Checking the latest local backup; private image stays on this PC.'
& $python $backupVerifier $backup.FullName
if ($LASTEXITCODE -ne 0) { throw 'Backup integrity check FAILED. Migration blocked.' }

Write-Host '[InfoTAB] Checking 4MB -> 16MB partition-layout overlaps offline...'
& $python $layoutVerifier --backup $backup.FullName --candidate $candidate
if ($LASTEXITCODE -ne 0) { throw 'Partition overlap preflight FAILED. Migration blocked.' }

Write-Host '[InfoTAB] Reading ESP32-P4 security flags from ROM over USB (no writes)...'
Write-Host '[InfoTAB] Close serial monitor before this operation.'
& $python $esptool --chip esp32p4 --port $Port --no-stub get-security-info
if ($LASTEXITCODE -ne 0) { throw 'Security information could not be read. Migration blocked.' }

Write-Host '[InfoTAB] PREFLIGHT READ-ONLY FINISHED. NO FLASH WRITES.'
Write-Host '[InfoTAB] Migration remains BLOCKED pending security review and approved recovery procedure.'
Write-Host '[InfoTAB] If the screen remains black, briefly press the side power button.'
