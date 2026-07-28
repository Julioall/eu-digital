[CmdletBinding()]
param(
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..'
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$PowerShellExecutable = (Get-Process -Id $PID).Path

$Checks = @(
    @{ Name = 'SPECs'; Script = 'validate_specs.ps1'; Arguments = @('-RepositoryRoot', $RepositoryRoot) },
    @{ Name = 'Configuração'; Script = 'validate_config.ps1'; Arguments = @('-RepositoryRoot', $RepositoryRoot) },
    @{ Name = 'Árvore'; Script = 'generate_repository_tree.ps1'; Arguments = @('-RepositoryRoot', $RepositoryRoot, '-Check') }
)

$Failed = $false
foreach ($Check in $Checks) {
    $ScriptPath = Join-Path $PSScriptRoot $Check.Script
    & $PowerShellExecutable -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $ScriptPath @($Check.Arguments)
    if ($LASTEXITCODE -ne 0) {
        Write-Error "$($Check.Name): falhou."
        $Failed = $true
    }
}

if ($Failed) {
    exit 1
}

Write-Output 'Validação documental concluída.'
