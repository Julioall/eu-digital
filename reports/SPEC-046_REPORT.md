# Relatório de Execução

SPEC: SPEC-046 — Consistent Cognitive Snapshot and Replay
Agente: Codex
Data: 2026-08-04
Commit: não aplicável

## Alterações realizadas

- Publicados `CognitiveSnapshot` 2.0 e `CognitiveStateBundle` 1.0 com fixture
  canônico compartilhado, checksum SHA-256 e validação estrita.
- Criada `ICognitiveStatePort`, resolvida por `CapabilityRegistry`, sem importar
  adapters concretos no núcleo.
- Implementados checkpoint e restauração atômica da fronteira de episódio e do
  conjunto de eventos concluídos pelo coordenador.
- Implementada captura completa com orçamento de 5 ms; ausência ou falha de
  qualquer provider stateful impede snapshot parcial.
- Implementado worker assíncrono limitado a um item pendente. Serialização,
  DPAPI e transação SQLite ficam fora da thread do ciclo; plaintext acima de 4
  MiB é recusado sem truncamento.
- Migrada a timeline de forma aditiva para a versão 3, preservando
  `occurred_at`, `received_at` e sessão do evento.
- Integrado restore com validação de versão, checksum, fingerprint, expiração,
  cursor e providers; o host tenta os dois blobs mais recentes e então usa cold
  replay.
- Replay agora reaplica eventos externos no coordenador com
  `replay_mode=true`, sem publicar decisão, UI, ação ou novo snapshot.
- Adicionado diagnóstico estruturado por `cognitive_recovery_json()`.
- Corrigido o perfil mínimo do host para manter disponíveis as capacidades já
  registradas pela composition root.

## Arquivos modificados

- Contratos: `contracts/schemas/cognitive_snapshot_v2.schema.json`,
  `contracts/schemas/cognitive_state_bundle.schema.json` e
  `contracts/fixtures/cognitive_snapshot_v2.json`.
- Núcleo: `cognitive_snapshot.hpp`, `cognitive_state_manager.hpp`,
  `async_cognitive_snapshot_writer.hpp`, `cognitive_coordinator.hpp`,
  `timeline_store.hpp`, `runtime_host.hpp`, `capability_runtime.hpp`, DTOs e
  porta de estado.
- Adapter/composição: `episode_segmenter_adapter.hpp` e
  `desktop_controller.cpp`.
- Testes C++ e Python registrados no `CMakeLists.txt`.
- Documentação: ADR-0034, SPEC-046, PLAN-046, arquitetura de referência,
  comandos de desenvolvimento e árvore do repositório.

## Testes executados

- Build completo do núcleo: `cmake --build build --target all --config Release`.
- CTest completo do núcleo: 44 de 44 passaram.
- Crash realista: `cognitive_recovery_crash` passou com subprocesso chamando
  `abort()`, blob corrompido mais recente e comparação contra execução contínua.
- Build Qt: `eu_digital_desktop` e `desktop_integration_test` compilaram.
- CTest no build Qt para os cinco alvos de persistência/recovery: 5 de 5
  passaram após disponibilizar `sqlite3.dll` no diretório de build.
- Python: `uv run --frozen pytest -q` — 241 passaram.
- Tipos: `uv run --frozen mypy python/eu_digital_lab` — sem problemas nos 28
  arquivos.
- Lint focado nos arquivos Python alterados — passou.
- Contratos e documentação — passaram; 54 SPECs e uma configuração válidas.
- `git diff --check` — passou.

## Resultados

- A fronteira de episódio após snapshot + replay é byte a byte igual à execução
  contínua com os mesmos três eventos.
- O evento posterior ao cursor é reaplicado exatamente uma vez; eventos internos
  `cognitive.cycle.result` não são reprocessados.
- Lixo binário, snapshot 1.0 incompatível e snapshot expirado não causam crash:
  produzem fallback anterior ou `cold_replay` com reason code estruturado.
- O submit assíncrono permanece abaixo do gate de 5 ms no teste; um provider que
  excede o orçamento de captura é recusado. A transação vazia prova rollback sem
  substituir o snapshot anterior.
- Remoção, reinstalação, substituição, ausência e falha de providers foram
  verificadas.

## Critérios de aceite

- [x] Envelope versionado exige timestamp, checksum, fingerprint e cursor.
- [x] Escrita SQLite atômica com retenção dos dois últimos registros.
- [x] Morte por `abort()` entre snapshots recompõe a segmentação exata.
- [x] Corrupção usa fallback gracioso.
- [x] Expiração configurável força cold replay.
- [x] Checksum, versão, schema estrito, desempenho e recovery possuem testes.

## Desvios

- O lint global continua com 57 débitos preexistentes fora dos arquivos
  alterados; o lint focado deste incremento passa.
- No preset Qt, `qt_avatar_shell` e `desktop_integration` ficam bloqueados antes
  do corpo dos testes no ambiente offscreen e excedem timeout. O build dos dois
  binários passa; os cinco testes Qt que exercitam a SPEC-046 passam. Esse
  comportamento não ocorre no build principal e não foi alterado por esta SPEC.

## Riscos e pendências

- Snapshots permanecem desabilitados por padrão. Providers stateful ainda sem
  `ICognitiveStatePort` obrigam cold replay, conforme ADR-0034; isso preserva
  correção sem fingir completude.
- O warning preexistente em `privacy_storage.hpp` sobre comparação de faixa de
  `uint32_t` permanece fora do escopo desta SPEC.
- Não há pendência crítica da SPEC-046.

## Decisões tomadas

- Snapshot 1.0 permanece legado e nunca é restaurado automaticamente.
- Timeline é a única fonte da verdade; snapshot é acelerador reversível.
- Estado parcial é sempre rejeitado e restauração falha executa rollback.
- O limite inicial de plaintext é 4 MiB e só deve mudar com nova evidência.
- `enable_cognitive_snapshots=false` é o rollback operacional sem apagar dados.

## Evidências

- `cognitive_snapshot_v2_test`: serialização C++ idêntica ao fixture e checksum
  corrompido rejeitado.
- `cognitive_state_manager_test`: completude, rollback e orçamento de captura.
- `async_cognitive_snapshot_writer_test`: DPAPI/SQLite fora da submissão,
  limite de tamanho e leitura do blob protegido.
- `cognitive_recovery_crash_test`: processo abortado, fallback do blob corrupto,
  replay de um evento e igualdade com baseline contínuo.
- `runtime_host_test`: versão incompatível, expiração, cold replay e logs
  estruturados.
