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

function Invoke-ValidationScript {
    param(
        [Parameter(Mandatory)]
        [string]$ScriptPath,

        [string[]]$Arguments = @()
    )

    $ProcessArguments = @(
        '-NoProfile',
        '-NonInteractive',
        '-ExecutionPolicy',
        'Bypass',
        '-File',
        $ScriptPath
    ) + $Arguments
    $Process = Start-Process -FilePath $PowerShellExecutable -ArgumentList $ProcessArguments -Wait -PassThru -NoNewWindow
    return [int]$Process.ExitCode
}

$Checks = @(
    @{ Name = 'SPECs'; Script = 'validate_specs.ps1'; Arguments = @('-RepositoryRoot', $RepositoryRoot) },
    @{ Name = 'Configuração'; Script = 'validate_config.ps1'; Arguments = @('-RepositoryRoot', $RepositoryRoot) },
    @{ Name = 'Árvore'; Script = 'generate_repository_tree.ps1'; Arguments = @('-RepositoryRoot', $RepositoryRoot, '-Check') }
)

$Failed = $false
foreach ($Check in $Checks) {
    $ScriptPath = Join-Path $PSScriptRoot $Check.Script
    $ExitCode = Invoke-ValidationScript -ScriptPath $ScriptPath -Arguments @($Check.Arguments)
    if ($ExitCode -ne 0) {
        Write-Error "$($Check.Name): falhou."
        $Failed = $true
    }
}

if ($Failed) {
    exit 1
}

Write-Output 'Validação documental concluída.'
