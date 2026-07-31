$ErrorActionPreference = "Stop"

$workspaceDir = $PSScriptRoot
$buildDir = Join-Path $workspaceDir "build\Debug"
$executable = Join-Path $buildDir "eu_digital_desktop.exe"

if (-not (Test-Path $executable)) {
    Write-Host "Executavel nao encontrado em $executable." -ForegroundColor Red
    exit 1
}

$dataDir = Join-Path $env:APPDATA "EU-Digital"
if (-not (Test-Path $dataDir)) {
    New-Item -ItemType Directory -Path $dataDir | Out-Null
}

$manifestPath = Join-Path $dataDir "manifest.json"
$timelinePath = Join-Path $dataDir "timeline.sqlite"
$dummyEventPath = Join-Path $dataDir "dummy_event.json"

$wshShell = New-Object -ComObject WScript.Shell
$desktopPath = [Environment]::GetFolderPath("Desktop")
$shortcutPath = Join-Path $desktopPath "EU-Digital (Visual).lnk"

$shortcut = $wshShell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $executable

# Parametros para Desktop Mode (Qt)
$shortcut.Arguments = "--run `"$manifestPath`" `"$timelinePath`" `"$dummyEventPath`" `"session-1`" `"2026-07-31T12:00:00Z`""
$shortcut.WorkingDirectory = $dataDir
$shortcut.Description = "EU-Digital GUI"
$shortcut.Save()

Write-Host "Atalho Desktop criado com sucesso!" -ForegroundColor Green
