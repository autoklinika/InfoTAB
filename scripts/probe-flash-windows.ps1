# InfoTAB / M5Stack Tab5 - read-only SPI Flash identification.
# This script must NEVER erase, write, or dump NVS credentials.
# Device may enter bootloader mode and may need a short press of power
# after the test. Use the USB-C data cable and exclusive access to COM6.

param(
    [string]$Port = 'COM6'
)

$ErrorActionPreference = 'Stop'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$esptool = Join-Path $env:USERPROFILE '.platformio\packages\tool-esptoolpy\esptool.py'

if (-not (Test-Path $python)) {
    throw "PlatformIO private Python not found: $python"
}
if (-not (Test-Path $esptool)) {
    throw "PlatformIO esptool.py not found: $esptool. Run a normal InfoTAB Build first."
}
if ($Port -notmatch '^COM[0-9]+$') {
    throw 'Expected a serial port such as COM6.'
}

Write-Host '[InfoTAB] ESP32-P4 Flash ID probe (READ ONLY)'
Write-Host '[InfoTAB] Close PlatformIO Serial Monitor before starting.'
Write-Host '[InfoTAB] No write, erase, firmware upload, or NVS read is performed.'
& $python $esptool --chip esp32p4 --port $Port flash-id
if ($LASTEXITCODE -ne 0) {
    throw "Flash ID query failed. Close Serial Monitor, check $Port and enter download mode if required."
}
Write-Host '[InfoTAB] FLASH ID PROBE FINISHED. NO FLASH WRITES.'
Write-Host '[InfoTAB] Verify the detected Flash size in the esptool output.'
Write-Host '[InfoTAB] If screen remains black, briefly press the side power button.'
