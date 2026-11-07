# InfoTAB / Windows 11: manual Wi-Fi OTA uploader.
# Requires a narrow TCP callback firewall rule; NEVER changes Flash partition layout.
# OTA token is entered locally. Never share it in chat or commit to GitHub.
param([string]$DeviceIp = '192.168.1.38')

$ErrorActionPreference = 'Stop'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
$espota = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32\tools\espota.py'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$firmware = Join-Path $project '.pio\build\infotab_tab5\firmware.bin'
$firewallRuleName = 'InfoTAB-OTA-Callback-3233'
$callbackPort = 3233
$slotBytes = 1310720

foreach ($toolPath in @($python, $pio, $espota)) {
    if (-not (Test-Path -LiteralPath $toolPath)) { throw "Tool not found: $toolPath" }
}
if ($DeviceIp -notmatch '^192\.168\.1\.(?:[1-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-4])$') {
    throw 'InfoTAB IP must be on local 192.168.1.0/24 LAN.'
}

Push-Location $project
try {
    Write-Host '[InfoTAB] Building for current 1280 KiB OTA partition...'
    & $pio run -e infotab_tab5
    if ($LASTEXITCODE -ne 0) { throw 'Build failed; no OTA attempted.' }
    if (-not (Test-Path -LiteralPath $firmware)) { throw 'Firmware image missing.' }
    $size = (Get-Item -LiteralPath $firmware).Length
    if ($size -le 0 -or $size -gt $slotBytes) {
        throw "Firmware size $size is outside 1280 KiB slot capacity ($slotBytes)."
    }

    # UDP connect resolves local source IP without sending a datagram.
    $hostIp = & $python -c 'import socket,sys;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.connect((sys.argv[1],3232));print(s.getsockname()[0]);s.close()' $DeviceIp
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($hostIp)) {
        throw 'Could not choose Windows source IP for InfoTAB.'
    }
    $hostIp = "$hostIp".Trim()
    if ($hostIp -notmatch '^192\.168\.1\.[0-9]{1,3}$') {
        throw "Windows route selected unexpected IP $hostIp. Check VPN or adapter."
    }

    $fwRule = Get-NetFirewallRule -Name $firewallRuleName -ErrorAction SilentlyContinue
    if ($null -eq $fwRule -or $fwRule.Enabled -ne 'True' -or $fwRule.Action -ne 'Allow') {
        throw 'Missing OTA callback firewall rule. Run scripts/setup-ota-firewall-windows.ps1 first.'
    }
    $addrFilter = $fwRule | Get-NetFirewallAddressFilter
    if (-not (@($addrFilter.RemoteAddress) -contains $DeviceIp) -or
        -not (@($addrFilter.LocalAddress) -contains $hostIp)) {
        throw 'Firewall address mismatch. Re-run setup-ota-firewall-windows.ps1 with current DeviceIp.'
    }

    Write-Host ('[InfoTAB] UDP OTA invitation: ' + $DeviceIp + ':3232')
    Write-Host ('[InfoTAB] Windows TCP callback: ' + $hostIp + ':' + $callbackPort)
    Write-Host ("[InfoTAB] Firmware size: $size B / $slotBytes B")
    Write-Host '[InfoTAB] On the device select OTA WIFI > AKTYWUJ OTA.'
    Write-Host '[InfoTAB] Enter the one-time token shown on InfoTAB.'
    $secure = Read-Host 'Jednorazowe haslo OTA' -AsSecureString
    $ptr = [IntPtr]::Zero
    $token = ''
    try {
        $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
        $token = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
        if ($token -notmatch '^[A-HJ-NP-Z2-9]{18}$') {
            throw 'Invalid OTA token format; upload NOT started.'
        }
        # espota.py exposes auth in its process arguments briefly. Avoid debug logs.
        & $python $espota -i $DeviceIp -I $hostIp -p 3232 -P $callbackPort -t 15 -a $token -f $firmware
        if ($LASTEXITCODE -ne 0) {
            Write-Host '[InfoTAB] OTA transfer failed; check callback TCP port 3233, firewall and LAN route.'
            throw "Wi-Fi OTA transfer FAILED (exit code $LASTEXITCODE)."
        }
        Write-Host '[InfoTAB] WIFI OTA TRANSFER SUCCESS. Check device version after reboot.'
    }
    finally {
        $token = ''
        if ($ptr -ne [IntPtr]::Zero) {
            [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
        }
    }
}
finally { Pop-Location }
