# InfoTAB OTA - one-time Windows Defender Firewall configuration.
# Allows ONLY TCP callback to PlatformIO Python on port 3233 from the given InfoTAB IP.
# Safe to re-run if InfoTAB's DHCP address changes. Requires UAC elevation.

param(
    [string]$DeviceIp = '192.168.1.38'
)

$ErrorActionPreference = 'Stop'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$ruleName = 'InfoTAB-OTA-Callback-3233'
$callbackPort = 3233

if ($DeviceIp -notmatch '^192\.168\.1\.(?:[1-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-4])$') {
    throw 'For this setup enter InfoTAB LAN IPv4 192.168.1.x only.'
}
if (-not (Test-Path -LiteralPath $python)) {
    throw "PlatformIO private Python not found: $python"
}

$localIp = & $python -c 'import socket,sys;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.connect((sys.argv[1],3232));print(s.getsockname()[0]);s.close()' $DeviceIp
if ($LASTEXITCODE -ne 0 -or -not ($localIp -match '^192\.168\.1\.[0-9]+$')) {
    throw "No suitable local route to InfoTAB $DeviceIp. Route-selected local address: $localIp"
}
$localIp = $localIp.Trim()
Write-Host "[InfoTAB] Wi-Fi OTA route: Windows $localIp -> Tab5 $DeviceIp"

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$elevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $elevated) {
    Write-Host '[InfoTAB] Asking Windows UAC for one-time firewall setup permission.'
    $scriptPath = $MyInvocation.MyCommand.Path
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $scriptPath + '" -DeviceIp ' + $DeviceIp
    $proc = Start-Process -FilePath 'powershell.exe' -Verb RunAs -ArgumentList $arguments -Wait -PassThru
    if ($proc.ExitCode -ne 0) { throw "Firewall configuration exited with code $($proc.ExitCode)" }
    Write-Host '[InfoTAB] Firewall setup completed in administrator window.'
    return
}

$existing = Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
if ($existing) {
    Remove-NetFirewallRule -Name $ruleName -ErrorAction Stop
}
$rule = @{
    Name = $ruleName
    DisplayName = 'InfoTAB Wi-Fi OTA TCP callback (restricted)'
    Description = 'Only local Python PlatformIO, TCP 3233, from the InfoTAB IP. For ESP OTA callback.'
    Enabled = 'True'
    Direction = 'Inbound'
    Action = 'Allow'
    Protocol = 'TCP'
    LocalPort = $callbackPort
    Program = $python
    LocalAddress = $localIp
    RemoteAddress = $DeviceIp
    Profile = 'Any'
    ErrorAction = 'Stop'
}
New-NetFirewallRule @rule | Out-Null

Write-Host '[InfoTAB] FIREWALL RULE PASS'
Write-Host "[InfoTAB] Allowed: TCP $callbackPort from InfoTAB $DeviceIp to local Windows $localIp"
Write-Host "[InfoTAB] Program scoped: $python"
Write-Host '[InfoTAB] No access allowed from arbitrary remote IPs and no UDP inbound rule added.'
