# Validate all PowerShell scripts for parser errors before any execution.
# Uses the native Windows PowerShell / PowerShell 7 AST parser. No side effects.
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$scripts = Get-ChildItem -LiteralPath (Join-Path $project 'scripts') -Filter '*.ps1' -File
if ($scripts.Count -eq 0) { throw 'No PowerShell scripts found.' }
$failed = $false
foreach ($file in $scripts) {
    $tokens = $null
    $parseErrors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        $file.FullName, [ref]$tokens, [ref]$parseErrors
    )
    if ($parseErrors.Count -gt 0) {
        $failed = $true
        Write-Host ('[InfoTAB] SYNTAX FAIL: ' + $file.Name)
        foreach ($problem in $parseErrors) {
            Write-Host ('  Line ' + $problem.Extent.StartLineNumber + ': ' + $problem.Message)
        }
    } else {
        Write-Host ('[InfoTAB] SYNTAX PASS: ' + $file.Name)
    }
}
if ($failed) { throw 'PowerShell syntax validation failed. Do not run OTA.' }
Write-Host '[InfoTAB] POWERSHELL SYNTAX PASS (all scripts)'
