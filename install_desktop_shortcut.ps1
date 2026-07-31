$ErrorActionPreference = "Stop"

$workspaceDir = $PSScriptRoot
$buildDir = Join-Path $workspaceDir "build\Debug"
$executable = Join-Path $buildDir "eu_digital_runtime.exe"

if (-not (Test-Path $executable)) {
    Write-Host "Executavel nao encontrado em $executable. Certifique-se de que o build foi concluido com sucesso." -ForegroundColor Red
    exit 1
}

# Configurações do ambiente de runtime
$dataDir = Join-Path $env:APPDATA "EU-Digital"
if (-not (Test-Path $dataDir)) {
    New-Item -ItemType Directory -Path $dataDir | Out-Null
}

$manifestPath = Join-Path $dataDir "manifest.json"
$timelinePath = Join-Path $dataDir "timeline.sqlite"
$dummyEventPath = Join-Path $dataDir "dummy_event.json"

# Cria manifest minimalista se não existir
if (-not (Test-Path $manifestPath)) {
    $manifestContent = @"
{
    "schema_version": "1.0",
    "runtime_id": "desktop-1",
    "runtime_version": "1.0.0",
    "build": {
        "platform": "win32",
        "compiler": "msvc",
        "profile": "Debug",
        "commit": "local",
        "python_runtime_dependency": false
    },
    "contract_versions": {
        "system.activity": "1.0"
    },
    "promoted_components": [],
    "optional_capabilities": []
}
"@
    Set-Content -Path $manifestPath -Value $manifestContent
}

# Cria um evento canônico dummy para o parametro
if (-not (Test-Path $dummyEventPath)) {
    $dummyEventContent = @"
{
    "schema_version": "1.0",
    "event_id": "dummy-1",
    "source": "system",
    "event_type": "system.startup",
    "occurred_at": "2026-07-31T12:00:00Z",
    "monotonic_ns": 0,
    "received_at": "2026-07-31T12:00:00Z",
    "session_id": "session-1",
    "actor_id": null,
    "context": {},
    "payload": {},
    "quality": {},
    "provenance": {},
    "privacy_class": "system",
    "tags": []
}
"@
    Set-Content -Path $dummyEventPath -Value $dummyEventContent
}

# Cria atalho na Área de Trabalho
$wshShell = New-Object -ComObject WScript.Shell
$desktopPath = [Environment]::GetFolderPath("Desktop")
$shortcutPath = Join-Path $desktopPath "EU-Digital Runtime.lnk"

$shortcut = $wshShell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $executable

# Parametros para deixar o runtime em execução consumindo a timeline
$shortcut.Arguments = "--run `"$manifestPath`" `"$timelinePath`" `"$dummyEventPath`" `"session-1`" `"2026-07-31T12:00:00Z`""
$shortcut.WorkingDirectory = $dataDir
$shortcut.Description = "EU-Digital Cognitive Runtime"
$shortcut.Save()

Write-Host "Instalação concluída!" -ForegroundColor Green
Write-Host "Um atalho chamado 'EU-Digital Runtime' foi criado na sua Área de Trabalho."
Write-Host "Ele já está configurado com os parâmetros corretos para iniciar sem fechar instantaneamente."
