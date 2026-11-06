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

    # Use the actual Windows source IP selected by the route to InfoTAB.
    # A VPN/virtual NIC may have a different 192.168.1.x address.
    $hostIp = & $python -c 'import socket,sys;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.connect((sys.argv[1],3232));print(s.getsockname()[0]);s.close()' $DeviceIp
    if ($LASTEXITCODE -ne 0 -or $hostIp -notmatch '^192\.168\.1\.[0-9]+

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
        Write-Host "[InfoTAB] UDP OTA invitation: $DeviceIp:3232"
        Write-Host "[InfoTAB] TCP callback listener: ${hostIp}:3233 (Windows inbound rule configured)"
        Write-Host "[InfoTAB] Sending $size bytes (firmware)..."
        # Password is passed only to the local espota process; Windows process
        # command-line inspection may still expose it briefly. LAN dev use only.
        & $python $espota -i $DeviceIp -I $hostIp -p 3232 -P 3233 -t 15 -a $token -f $firmware
        if ($LASTEXITCODE -ne 0) {
            Write-Host '[InfoTAB] If espota says "No response from device" after authentication:'
            Write-Host '[InfoTAB] Check Windows inbound TCP callback port 3233, actual route IP, and local network isolation.'
            Write-Host '[InfoTAB] The working firmware on Tab5 remains in the current OTA slot.'
            throw "Wi-Fi OTA transfer FAILED (exit code $LASTEXITCODE)"
        }
        Write-Host '[InfoTAB] WIFI OTA TRANSFER SUCCESS. Check the new version on the device.'
    }
    finally {
        $token = ''
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
    }
}
finally { Pop-Location }
) {
        throw "Could not find a local route to InfoTAB $DeviceIp. No upload."
    }
    $hostIp = $hostIp.Trim()

    $fwRule = Get-NetFirewallRule -Name 'InfoTAB-OTA-Callback-3233' -ErrorAction SilentlyContinue
    if (-not $fwRule -or $fwRule.Enabled -ne 'True') {
        throw 'Missing narrow OTA TCP 3233 Windows Firewall rule. Run .\scripts\setup-ota-firewall-windows.ps1 first.'
    }
    $addressFilter = $fwRule | Get-NetFirewallAddressFilter
    if (-not ($addressFilter.RemoteAddress -contains $DeviceIp) -or
        -not ($addressFilter.LocalAddress -contains $hostIp)) {
        throw "OTA firewall rule address differs from current route. Re-run .\scripts\setup-ota-firewall-windows.ps1 -DeviceIp $DeviceIp"
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
