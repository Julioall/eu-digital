[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$VcpkgRoot = $env:VCPKG_INSTALLATION_ROOT
)

$ErrorActionPreference = 'Stop'

if (-not $RepositoryRoot) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..'
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path

function Get-VisualStudioInstallation {
    $vswhereCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    )
    foreach ($candidate in $vswhereCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $installation = & $candidate -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($LASTEXITCODE -eq 0 -and $installation) {
                return $installation.Trim()
            }
        }
    }

    $fallbacks = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\18\Community'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\BuildTools'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Community')
    )
    foreach ($fallback in $fallbacks) {
        if (Test-Path -LiteralPath (Join-Path $fallback 'VC\Auxiliary\Build\vcvars64.bat')) {
            return $fallback
        }
    }
    throw 'Instalação do Visual Studio com ferramentas C++ x64 não localizada.'
}

function Import-VcVarsEnvironment {
    param([Parameter(Mandatory)][string]$InstallationPath)

    $vcvars = Join-Path $InstallationPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) {
        throw "vcvars64.bat não localizado: $vcvars"
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'cmd.exe'
    $startInfo.Arguments = "/d /s /c `"call `"$vcvars`" x64 && set`""
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.UseShellExecute = $false
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $output = $process.StandardOutput.ReadToEnd()
    $errorOutput = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "Falha ao carregar o ambiente MSVC: $errorOutput"
    }

    foreach ($line in ($output -split "`r?`n")) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
}

function Invoke-NativeChecked {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [string[]]$ArgumentList = @()
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath terminou com código $LASTEXITCODE"
    }
}

if (-not $VcpkgRoot) {
    throw 'Informe -VcpkgRoot ou defina VCPKG_INSTALLATION_ROOT.'
}
$VcpkgRoot = (Resolve-Path -LiteralPath $VcpkgRoot).Path
$sqliteLibrary = Join-Path $VcpkgRoot 'installed\x64-windows\lib\sqlite3.lib'
$toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if (-not (Test-Path -LiteralPath $sqliteLibrary -PathType Leaf)) {
    throw "SQLite x64-windows não localizado: $sqliteLibrary"
}
if (-not (Test-Path -LiteralPath $toolchain -PathType Leaf)) {
    throw "Toolchain vcpkg não localizado: $toolchain"
}

$installation = Get-VisualStudioInstallation
Import-VcVarsEnvironment -InstallationPath $installation

$sdkResourceCompiler = Get-ChildItem -Path (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin') -Filter rc.exe -File -Recurse |
    Where-Object { $_.Directory.Name -eq 'x64' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if ($null -eq $sdkResourceCompiler) {
    throw 'rc.exe do Windows SDK não localizado.'
}
$env:Path = "$($sdkResourceCompiler.Directory.FullName);$env:Path"

$windowsKitRoot = $sdkResourceCompiler.Directory.Parent.Parent.Parent.FullName
$sdkVersion = $sdkResourceCompiler.Directory.Parent.Name
$sdkLibRoot = Join-Path $windowsKitRoot "Lib\$sdkVersion"
$sdkIncludeRoot = Join-Path $windowsKitRoot 'Include'
$msvcRoot = Join-Path $installation 'VC\Tools\MSVC'
$msvcLib = Get-ChildItem $msvcRoot -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'lib\x64' } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
    Select-Object -First 1
$sdkLibs = @(
    (Join-Path $sdkLibRoot 'um\x64'),
    (Join-Path $sdkLibRoot 'ucrt\x64')
) | Where-Object { Test-Path -LiteralPath $_ -PathType Container }
if (-not $msvcLib -or $sdkLibs.Count -lt 2) {
    throw 'Bibliotecas do MSVC/Windows SDK não foram localizadas.'
}
$libDirectories = @($msvcLib) + @($sdkLibs)
$env:LIB = (($libDirectories -join ';') + ";$env:LIB")
$sdkIncludes = @(
    (Join-Path $sdkIncludeRoot "$sdkVersion\um"),
    (Join-Path $sdkIncludeRoot "$sdkVersion\shared"),
    (Join-Path $sdkIncludeRoot "$sdkVersion\ucrt")
) | Where-Object { Test-Path -LiteralPath $_ -PathType Container }
$env:INCLUDE = (($sdkIncludes -join ';') + ";$env:INCLUDE")

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
}
else {
    $cmake = (Get-ChildItem (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe') -ErrorAction SilentlyContinue |
        Select-Object -First 1).FullName
}
if (-not $cmake) {
    throw 'cmake não localizado no PATH ou em Program Files\CMake\bin.'
}
$cmakeDirectory = Split-Path -Parent $cmake
$env:Path = "$cmakeDirectory;$env:Path"

$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninjaCommand) {
    $ninjaPath = $ninjaCommand.Source
}
if (-not $ninjaCommand) {
    $profileRoot = [Environment]::GetFolderPath('UserProfile')
    $ninja = Get-Item -Path (Join-Path $profileRoot 'AppData\Local\Microsoft\WinGet\Packages\*\ninja.exe') -ErrorAction SilentlyContinue |
        Select-Object -First 1
    $wingetRoots = @()
    if ($env:LOCALAPPDATA) {
        $wingetRoots += Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    }
    if ($env:USERPROFILE) {
        $wingetRoots += Join-Path $env:USERPROFILE 'AppData\Local\Microsoft\WinGet\Packages'
    }
    $wingetRoots += 'C:\Users'
    if (-not $ninja) {
        foreach ($wingetRoot in $wingetRoots) {
            if (Test-Path -LiteralPath $wingetRoot -PathType Container) {
                $ninja = Get-ChildItem -LiteralPath $wingetRoot -Filter ninja.exe -File -Recurse -ErrorAction SilentlyContinue |
                    Select-Object -First 1
                if ($ninja) {
                    break
                }
            }
        }
    }
    if ($ninja) {
        $ninjaPath = $ninja.FullName
        $env:Path = "$($ninja.Directory.FullName);$env:Path"
    }
}
if (-not $ninjaPath) {
    throw 'ninja não localizado no PATH ou nos pacotes do WinGet.'
}
$env:VCPKG_INSTALLATION_ROOT = $VcpkgRoot

Push-Location $RepositoryRoot
try {
    Invoke-NativeChecked -FilePath $cmake -ArgumentList @(
        '--fresh', '--preset', 'windows-dev',
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
        "-DCMAKE_MAKE_PROGRAM=$ninjaPath"
    )
    Invoke-NativeChecked -FilePath $cmake -ArgumentList @('--build', '--preset', 'windows-dev')
    Invoke-NativeChecked -FilePath 'ctest' -ArgumentList @('--preset', 'windows-dev')
}
finally {
    Pop-Location
}

Write-Output 'Windows Runtime Preview validation passed.'
