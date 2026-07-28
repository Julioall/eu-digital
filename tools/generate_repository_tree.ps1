[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$OutputPath,
    [switch]$Check
)

$ErrorActionPreference = 'Stop'
if (-not $RepositoryRoot) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..'
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
if (-not $OutputPath) {
    $OutputPath = Join-Path $RepositoryRoot 'REPOSITORY_TREE.txt'
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

$ExcludedSegments = @('.git', '.venv', '__pycache__', '.pytest_cache', '.mypy_cache', '.ruff_cache', 'build', 'out')
$Files = Get-ChildItem -LiteralPath $RepositoryRoot -Recurse -Force -File | Where-Object {
    $Relative = $_.FullName.Substring($RepositoryRoot.Length + 1)
    $Segments = $Relative -split '[\\/]'
    -not ($Segments | Where-Object { $_ -in $ExcludedSegments })
} | ForEach-Object {
    $_.FullName.Substring($RepositoryRoot.Length + 1).Replace('\', '/')
}

if ($OutputPath.StartsWith($RepositoryRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    $RelativeOutput = $OutputPath.Substring($RepositoryRoot.Length + 1).Replace('\', '/')
    $Files = @($Files) + $RelativeOutput
}

$Expected = @($Files | Sort-Object -Unique)
if ($Check) {
    if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
        Write-Error "Árvore ausente: $OutputPath"
        exit 1
    }
    $Actual = @(Get-Content -Encoding UTF8 -LiteralPath $OutputPath | Where-Object { $_ })
    $Difference = @(Compare-Object -ReferenceObject $Expected -DifferenceObject $Actual)
    if ($Difference.Count -gt 0) {
        $Difference | ForEach-Object {
            Write-Error "Árvore desatualizada [$($_.SideIndicator)]: $($_.InputObject)"
        }
        exit 1
    }
    Write-Output "Árvore atualizada: $($Expected.Count) arquivos"
    exit 0
}

$ParentDirectory = Split-Path -Parent $OutputPath
if (-not (Test-Path -LiteralPath $ParentDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $ParentDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $Expected -Encoding UTF8
Write-Output "Árvore gerada: $($Expected.Count) arquivos"
