[CmdletBinding()]
param(
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..\..'
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$PowerShellExecutable = (Get-Process -Id $PID).Path
$TemporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("eu-digital-spec001-" + [guid]::NewGuid())
$Passed = 0
$Failed = 0

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

function Assert-ExitCode {
    param(
        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter(Mandatory)]
        [int]$Expected,

        [Parameter(Mandatory)]
        [int]$Actual
    )

    if ($Expected -eq $Actual) {
        $script:Passed++
        Write-Output "PASS: $Name"
        return
    }

    $script:Failed++
    Write-Output "FAIL: $Name (esperado=$Expected, obtido=$Actual)"
}

function Write-SpecFixture {
    param(
        [Parameter(Mandatory)]
        [string]$Content
    )

    Set-Content -LiteralPath (Join-Path $TemporaryRoot 'specs\SPEC-001-test.md') -Value $Content -Encoding UTF8
}

$ValidSpec = @'
---
id: SPEC-001
title: Spec de teste documental
status: ready
phase: 0
dependencies: []
adrs: [ADR-0001]
contracts: [TEST_CONTRACT.md]
---

# SPEC-001 — Spec de teste documental

## Objetivo

Validar o fluxo documental.

## Escopo negativo

- Não implementar código de domínio.

## Critérios de aceite

- [ ] O teste passa.
'@

try {
    New-Item -ItemType Directory -Path (Join-Path $TemporaryRoot 'specs') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $TemporaryRoot 'schemas') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $TemporaryRoot 'docs\04-adrs') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $TemporaryRoot 'docs\03-contracts') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'schemas\spec.schema.json') -Destination (Join-Path $TemporaryRoot 'schemas\spec.schema.json')
    Set-Content -LiteralPath (Join-Path $TemporaryRoot 'docs\04-adrs\ADR-0001-test.md') -Value "# ADR-0001`n`nStatus: aceito" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $TemporaryRoot 'docs\03-contracts\TEST_CONTRACT.md') -Value "# Contrato de teste" -Encoding UTF8

    $SpecValidator = Join-Path $RepositoryRoot 'tools\validate_specs.ps1'
    $ConfigValidator = Join-Path $RepositoryRoot 'tools\validate_config.ps1'
    $TreeGenerator = Join-Path $RepositoryRoot 'tools\generate_repository_tree.ps1'

    Write-SpecFixture -Content $ValidSpec
    $Code = Invoke-ValidationScript -ScriptPath $SpecValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'aceita SPEC válida' -Expected 0 -Actual $Code

    Write-SpecFixture -Content ($ValidSpec -replace '(?s)## Objetivo.*?(?=## Escopo negativo)', '')
    $Code = Invoke-ValidationScript -ScriptPath $SpecValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'rejeita SPEC sem objetivo' -Expected 1 -Actual $Code

    Write-SpecFixture -Content ($ValidSpec -replace '(?s)## Escopo negativo.*?(?=## Critérios de aceite)', '')
    $Code = Invoke-ValidationScript -ScriptPath $SpecValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'rejeita SPEC sem escopo negativo' -Expected 1 -Actual $Code

    Write-SpecFixture -Content ($ValidSpec -replace '(?s)## Critérios de aceite.*$', '')
    $Code = Invoke-ValidationScript -ScriptPath $SpecValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'rejeita SPEC sem critérios de aceite' -Expected 1 -Actual $Code

    Write-SpecFixture -Content ($ValidSpec -replace '(?s)^---.*?---\s*', '')
    $Code = Invoke-ValidationScript -ScriptPath $SpecValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'rejeita SPEC sem frontmatter' -Expected 1 -Actual $Code

    Write-SpecFixture -Content ($ValidSpec -replace 'dependencies: \[\]', 'dependencies: [SPEC-999]')
    $Code = Invoke-ValidationScript -ScriptPath $SpecValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'rejeita dependência inexistente' -Expected 1 -Actual $Code

    Write-SpecFixture -Content ($ValidSpec -replace 'adrs: \[ADR-0001\]', 'adrs: [ADR-9999]')
    $Code = Invoke-ValidationScript -ScriptPath $SpecValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'rejeita ADR inexistente' -Expected 1 -Actual $Code

    Write-SpecFixture -Content ($ValidSpec -replace 'contracts: \[TEST_CONTRACT.md\]', 'contracts: [MISSING_CONTRACT.md]')
    $Code = Invoke-ValidationScript -ScriptPath $SpecValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'rejeita contrato inexistente' -Expected 1 -Actual $Code

    $Code = Invoke-ValidationScript -ScriptPath $ConfigValidator -Arguments @('-RepositoryRoot', $RepositoryRoot)
    Assert-ExitCode -Name 'valida configuração normativa real' -Expected 0 -Actual $Code

    New-Item -ItemType Directory -Path (Join-Path $TemporaryRoot 'config') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'schemas\project.schema.json') -Destination (Join-Path $TemporaryRoot 'schemas\project.schema.json')
    $InvalidConfig = (Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $RepositoryRoot 'config\project.yaml')) -replace 'cloud_apis_allowed: false', 'cloud_apis_allowed: true'
    Set-Content -LiteralPath (Join-Path $TemporaryRoot 'config\project.yaml') -Value $InvalidConfig -Encoding UTF8
    $Code = Invoke-ValidationScript -ScriptPath $ConfigValidator -Arguments @('-RepositoryRoot', $TemporaryRoot)
    Assert-ExitCode -Name 'rejeita configuração fora do schema' -Expected 1 -Actual $Code

    Set-Content -LiteralPath (Join-Path $TemporaryRoot 'LICENSE') -Value 'test' -Encoding UTF8
    $TreePath = Join-Path $TemporaryRoot 'REPOSITORY_TREE.txt'
    $Code = Invoke-ValidationScript -ScriptPath $TreeGenerator -Arguments @('-RepositoryRoot', $TemporaryRoot, '-OutputPath', $TreePath)
    Assert-ExitCode -Name 'gera árvore do repositório' -Expected 0 -Actual $Code

    $TreeContent = @(Get-Content -Encoding UTF8 -LiteralPath $TreePath)
    if ($TreeContent -contains 'LICENSE') {
        $Passed++
        Write-Output 'PASS: árvore inclui LICENSE'
    }
    else {
        $Failed++
        Write-Output 'FAIL: árvore inclui LICENSE'
    }

    Set-Content -LiteralPath (Join-Path $TemporaryRoot 'novo-arquivo.txt') -Value 'test' -Encoding UTF8
    $Code = Invoke-ValidationScript -ScriptPath $TreeGenerator -Arguments @('-RepositoryRoot', $TemporaryRoot, '-OutputPath', $TreePath, '-Check')
    Assert-ExitCode -Name 'detecta árvore desatualizada' -Expected 1 -Actual $Code
}
finally {
    if (Test-Path -LiteralPath $TemporaryRoot) {
        Remove-Item -LiteralPath $TemporaryRoot -Recurse -Force
    }
}

Write-Output "Resultado: $Passed passou, $Failed falhou"
if ($Failed -gt 0) {
    exit 1
}
