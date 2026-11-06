# InfoTAB ESP32-P4 — full 16 MiB Flash backup to Windows user profile.
# READ ONLY: calls esptool read-flash, never write-flash or erase-flash.
# Backup includes sensitive NVS Wi-Fi credentials; NEVER upload it to GitHub/ChatGPT.

param(
    [string]$Port = 'COM6'
)

$ErrorActionPreference = 'Stop'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$esptool = Join-Path $env:USERPROFILE '.platformio\packages\tool-esptoolpy\esptool.py'
$verify = Join-Path $PSScriptRoot 'verify-flash-backup.py'

if ($Port -notmatch '^COM[0-9]+$') { throw 'Expected COM port, e.g. COM6.' }
foreach ($path in @($python, $esptool, $verify)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing required tool: $path" }
}
if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    throw 'Windows LOCALAPPDATA is not available. Aborting backup.'
}

$directory = Join-Path $env:LOCALAPPDATA 'InfoTAB\FlashBackups'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$unique = [guid]::NewGuid().ToString('N').Substring(0,8)
$filename = "InfoTAB-flash-16MiB-$timestamp-$unique.bin"
$target = Join-Path $directory $filename
$partial = "$target.partial"

Write-Host '[InfoTAB] Full 16 MiB backup via USB-C (READ ONLY).'
Write-Host '[InfoTAB] Close Serial Monitor; make sure Tab5 is on COM6.'
Write-Host '[InfoTAB] Sensitive Wi-Fi credentials may be inside the backup.'
Write-Host '[InfoTAB] Storage: Windows LOCALAPPDATA, OUTSIDE Git repository.'
Write-Host '[InfoTAB] Never send this .bin to GitHub, chat, or public cloud.'
Write-Host '[InfoTAB] This command does not erase, write, or update flash.'
$completed = $false
try {
    & $python $esptool --chip esp32p4 --port $Port read-flash 0x0 0x1000000 $partial
    if ($LASTEXITCODE -ne 0) { throw "esptool read-flash failed (code $LASTEXITCODE)" }

    & $python $verify $partial
    if ($LASTEXITCODE -ne 0) { throw "Full-flash backup verification failed (code $LASTEXITCODE)" }

    Move-Item -LiteralPath $partial -Destination $target -ErrorAction Stop
    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    $completed = $true
    Write-Host '[InfoTAB] BACKUP VERIFIED - READ ONLY PASS.'
    Write-Host "[InfoTAB] Backup stored at: $target"
    Write-Host "[InfoTAB] SHA256: $hash"
    Write-Host '[InfoTAB] Flash has NOT been modified.'
    Write-Host '[InfoTAB] Keep the backup private; enable BitLocker/device encryption.'
    Write-Host '[InfoTAB] If the screen stays black, briefly press the side power button.'
}
finally {
    if (-not $completed -and (Test-Path -LiteralPath $partial)) {
        Remove-Item -LiteralPath $partial -Force
        Write-Host '[InfoTAB] Incomplete sensitive backup removed.'
    }
}
