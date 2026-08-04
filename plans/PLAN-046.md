# Plano de Execução: SPEC-046 (Consistent Cognitive Snapshot and Replay)

Status: em execução após aceitação da ADR-0034

## Pré-condições

- SPEC-045 e SPEC-047 concluídas.
- SQLite e proteção local da SPEC-030 operacionais.
- ADR-0034 aceita.

## Contratos congelados

- `CognitiveCycleInput` 1.0;
- `CognitiveSnapshot` 1.0 permanece legado;
- `CognitiveSnapshot` 2.0 e `CognitiveStateBundle` 1.0 serão publicados antes
  da implementação.

## Etapa 1 — Contratos e testes

- Publicar schemas e fixtures v2 sem alterar silenciosamente o snapshot 1.0.
- Definir `ICognitiveStatePort` e DTOs de fragmento/bundle.
- Escrever primeiro testes de checksum, completude, restauração e fallback.

## Etapa 2 — Timeline e escrita atômica

- Preservar a transação SQLite existente e expor metadata dos dois blobs.
- Migrar `occurred_at`, `received_at` e sessão do evento de forma aditiva.
- Adicionar worker local limitado para DPAPI e escrita fora da thread do ciclo.
- Provar rollback da transação e retenção dos dois snapshots mais recentes.

## Etapa 3 — Coordenador e providers

- Capturar/restaurar estado idempotente do coordenador.
- Resolver providers por `CapabilityRegistry`, sem imports concretos no núcleo.
- Implementar checkpoint atômico do boundary de episódio para o teste realista.
- Recusar snapshot parcial quando qualquer provider stateful ativo não comprova
  checkpoint.

## Etapa 4 — RuntimeHost e recovery

- Criar coordenador antes da recuperação e restaurar estado validado.
- Tentar snapshot atual, anterior e por fim `full_timeline_replay_v0`.
- Reaplicar somente eventos externos com `replay_mode=true` e sem efeitos.
- Capturar a cada 100 eventos live e no encerramento limpo.

## Testes obrigatórios

- schema/checksum/version/fingerprint/expiração/cursor;
- blob corrompido e fallback anterior;
- provider ausente, falho, removido, reinstalado e substituído;
- replay repetido idempotente e metamórfico;
- equivalência execução contínua versus snapshot + replay;
- subprocesso com `abort()` entre snapshots;
- captura profunda abaixo de 5 ms e escrita assíncrona;
- cold replay quando DPAPI ou checkpoint completo está indisponível.

## Comandos de validação

```powershell
cmake --build build --config Debug --target all
ctest --test-dir build -C Debug -R "cognitive_snapshot|runtime_host|cognitive_recovery" --output-on-failure
uv run --frozen pytest -q
uv run --frozen mypy python/eu_digital_lab
```

## Migração

Migrar a timeline de forma aditiva. Não remover a tabela `cognitive_snapshots`
v2 já existente nem apagar blobs em rollback.

## Rollback

Definir `enable_cognitive_snapshots=false` e executar cold replay integral.
Dados persistidos permanecem sujeitos à gestão explícita da SPEC-030.

## Evidências esperadas

- artifact SQLite com dois blobs protegidos e metadata;
- log estruturado de snapshot aceito/rejeitado e `Replay of N events completed`;
- comparação de digest do estado contínuo e recuperado;
- processo auxiliar retornando falha por `abort()` e processo seguinte
  reconstruindo a mesma fronteira de episódio.

## Critérios para parar

- captura síncrona acima de 5 ms;
- necessidade de importar componente concreto no host/coordenador;
- restauração parcial sem rollback;
- replay que publique decisão, UI ou ação;
- ausência de checkpoint tratada como estado vazio.
