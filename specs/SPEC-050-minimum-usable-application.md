---
id: SPEC-050
title: Desktop Application and Vertical Slice
status: in_progress
phase: product_beta
dependencies: [SPEC-028, SPEC-030, SPEC-031, SPEC-042, SPEC-045, SPEC-046, SPEC-048]
adrs: [ADR-0016, ADR-0026, ADR-0027, ADR-0032, ADR-0034, ADR-0035, ADR-0037]
contracts: [DESKTOP_RUNTIME_CONTRACT.md, PRIVACY_STORAGE_CONTRACTS.md, DIALOGUE_AVATAR_SCHEMA.md, consent_policy.schema.json, desktop_session_state.schema.json, desktop_performance_sample.schema.json, runtime_health.schema.json]
---

# SPEC-050 — Desktop Application and Vertical Slice

## Objetivo

Empacotar o runtime cognitivo C++ e o shell Qt em `eu_digital_desktop.exe`,
oferecendo um produto local mínimo que inicia deny-by-default, mantém GUI e
trabalho cognitivo separados, permite pausa/revogação e continua operacional
sem sensores ou modelo opcionais.

## Resultado observável

O usuário inicia a aplicação, revisa consentimento por sensor/finalidade antes
de qualquer captura e encontra o processo na bandeja. O host publica estado
operacional versionado, responde dentro dos limites de UI, recupera uma sessão
anterior interrompida e encerra sem deixar captura ou marker ativo.

## Requisitos funcionais

- Usar um único entrypoint Qt e uma thread cognitiva com shutdown cooperativo.
- Persistir consentimento somente por `ConsentLedger` + DPAPI; `QSettings` pode
  guardar apenas preferências visuais não sensíveis.
- Construir/iniciar cada sensor somente após `capture_allowed(sensor, purpose)`.
- Pausa global e revogação devem interromper novas capturas e preservar grants
  independentes dos demais sensores.
- Publicar `DesktopSessionState` 1.0 em cada transição relevante.
- Usar manifesto do pacote e diretório de dados do usuário; nenhum placeholder
  pode ser criado pelo executável.
- Detectar marker de shutdown incompleto, entrar em `degraded`, executar o
  recovery da ADR-0034 e informar localmente sem conteúdo sensorial.
- Continuar em modo degradado quando renderer/modelo estiver ausente.
- Apresentação de diálogo deve permanecer pela porta assíncrona da ADR-0035.

## Requisitos não funcionais

- Tray activation p99 menor que 50 ms.
- Frame time p95 menor que 16,6 ms e p99 menor que 33 ms.
- Idle CPU menor que 1% sem eventos e sem modelo carregado.
- Shutdown com limite explícito e sem deadlock em stress com watchdog.
- TSan é gate adicional quando suportado, não substituto dos testes de liveness.

## Estados e transições

- `onboarding`: shell disponível, sensores inexistentes e consentimento pendente.
- `starting`: configuração, ledger e runtime sendo validados.
- `running`: runtime ativo e somente sensores consentidos em operação.
- `paused`: runtime ativo, nenhuma captura nova.
- `degraded`: capability opcional ausente ou recovery/ledger requer atenção.
- `stopping`: captura interrompida e componentes drenando.
- `stopped`: thread encerrada e marker removido após commit limpo.

## Escopo negativo

- Não implementar a experiência de atividade/cards da SPEC-053.
- Não reativar a SPEC-052 nem conectar UI diretamente ao Ollama.
- Não baixar, embutir ou selecionar outro modelo.
- Não adicionar atuador ou ação real.
- Não usar telemetria externa nem armazenar conteúdo sensorial em logs/markers.
- Não alegar aprendizado a partir de métricas de UI, CPU ou estabilidade.

## Critérios de aceite

- [x] Primeira execução sem consentimento publica `onboarding` e produz zero
  captura/evento de sensor.
- [x] Grants são independentes por sensor/finalidade; pausa e revogação bloqueiam
  novas capturas antes do event bus e persistem via DPAPI.
- [x] Ausência de modelo inicia graciosamente em `degraded` e mantém timeline,
  privacidade, diagnóstico e cognição disponíveis.
- [x] Manifesto/timestamps/placeholders não são fabricados pelo desktop.
- [x] Marker órfão aciona recovery e aviso; shutdown limpo remove o marker.
- [x] Stress concorrente de start/pause/resume/stop termina sob watchdog sem
  deadlock, diagnóstico tardio ou thread restante.
- [x] Tray p99 < 50 ms, frame p95 < 16,6 ms, frame p99 < 33 ms e idle CPU < 1%
  em amostras `DesktopPerformanceSample` válidas.
- [ ] IME pt-BR, DPI 100–250%, multi-monitor, keyboard navigation, UI Automation,
  tray, click-through, transparência e suspend/resume possuem evidência Windows.
- [x] Build Qt, testes headless, integração desktop, lint, tipos e suites globais
  passam; relatório de execução é atualizado.

## Plano de testes

- Unitários: máquina de estados, ledger/store, marker e DTOs.
- Contrato: schemas válidos, campos extras/tipos/versões rejeitados.
- Integração: primeira execução, grants parciais, pausa, revogação, modelo ausente,
  marker órfão e shutdown limpo com diretórios isolados.
- Concorrência: stress cross-thread com watchdog e TSan quando disponível.
- Performance: probes locais de tray, frame, idle e shutdown, sem inferência.
- Plataforma: matriz manual/automatizada da ADR-0032 no Windows.

## Migração

O booleano legado `QSettings/DesktopRuntime/consent_granted` não concede acesso.
Na primeira execução desta versão ele é ignorado e removido após o ledger
criptografado ser criado ou o usuário negar. Nenhum grant é inferido do valor
legado.

## Rollback

Desabilitar o target Qt e executar o runtime headless. Não apagar ledger,
timeline, snapshots ou marker durante rollback; recovery continua local.
