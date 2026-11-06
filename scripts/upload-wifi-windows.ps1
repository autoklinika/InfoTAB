# InfoTAB - push compiled firmware by Wi-Fi to manually armed local OTA.
# First install an OTA-capable build through USB/COM6. After that this script uses Wi-Fi.
# Never paste the one-time token into chat, GitHub, or a file.
param(
    [string]$DeviceIp = '192.168.1.38'
)

$ErrorActionPreference = 'Stop'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
$espota = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32\tools\espota.py'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$firmware = Join-Path $root '.pio\build\infotab_tab5\firmware.bin'

foreach ($path in @($python, $pio, $espota)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing PlatformIO tool: $path"
    }
}
if ($DeviceIp -notmatch '^[0-9]{1,3}(\.[0-9]{1,3}){3}$') {
    throw 'Specify the current InfoTAB IPv4 address, for example -DeviceIp 192.168.1.38'
}

Push-Location $root
try {
    Write-Host '[InfoTAB] Building current default 4MB-layout firmware (OTA slot 1280KiB)...'
    & $pio run -e infotab_tab5
    if ($LASTEXITCODE -ne 0) { throw 'Build FAILED. OTA upload not attempted.' }
    if (-not (Test-Path -LiteralPath $firmware)) { throw 'firmware.bin is missing.' }
    $size = (Get-Item -LiteralPath $firmware).Length
    if ($size -gt 1310720) {
        throw "Firmware too large for existing 1280KiB OTA slot: $size bytes. No upload."
    }

    $hostIp = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop |
        Where-Object { $_.IPAddress -like '192.168.1.*' -and $_.AddressState -eq 'Preferred' } |
        Select-Object -First 1 -ExpandProperty IPAddress
    if ([string]::IsNullOrEmpty($hostIp)) {
        throw 'Windows computer has no preferred IPv4 in 192.168.1.0/24. No upload.'
    }

    Write-Host '[InfoTAB] On the touchscreen open OTA WIFI and tap AKTYWUJ OTA.'
    Write-Host '[InfoTAB] Type the one-time 18-character token shown on InfoTAB.'
    Write-Host '[InfoTAB] The device accepts updates for 5 minutes only.'
    $secure = Read-Host 'Jednorazowe haslo OTA' -AsSecureString
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    $token = ''
    try {
        $token = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
        if ($token -notmatch '^[A-Z2-9]{18}$') {
            throw 'Invalid OTA token length/format. No upload attempted.'
        }
        Write-Host "[InfoTAB] Sending $size bytes via Wi-Fi to $DeviceIp:3232 from $hostIp ..."
        # Password is passed only to the local espota process; Windows process
        # command-line inspection may still expose it briefly. LAN dev use only.
        & $python $espota -i $DeviceIp -I $hostIp -p 3232 -a $token -f $firmware
        if ($LASTEXITCODE -ne 0) { throw "Wi-Fi OTA transfer FAILED (exit code $LASTEXITCODE)" }
        Write-Host '[InfoTAB] WIFI OTA TRANSFER SUCCESS. Check the new version on the device.'
    }
    finally {
        $token = ''
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
    }
}
finally { Pop-Location }
