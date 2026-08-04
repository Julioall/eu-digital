# Plano de Execução: SPEC-045 (Integrated Headless Cognitive Cycle)

Status: concluído em 2026-08-04

## Pré-condições
- SPEC-047 concluída e contratos 1.0 aprovados.
- ADR-0033 aceita.
- `RuntimeHost` compilando.

## Contratos congelados

- `CognitiveCycleInput` 1.0;
- `CognitiveCycleStage` 1.0;
- `CognitiveCycleResult` 1.0;
- `PortInvocationContext` 1.0;
- respostas auxiliares 1.0 de episódio, features, saliência e hipótese.

## Arquivos por Etapa

### Etapa 1: Estados, contratos e fila — concluída

- Estados e DTOs estão em `cpp/core/contracts/cognitive_cycle_v1.hpp`.
- A fila limitada `drop_newest_v1` permanece encapsulada no coordenador.

### Etapa 2: Implementação do loop — concluída

- `CognitiveCoordinator` resolve somente portas pelo `CapabilityRegistry`.
- O pipeline inclui as fronteiras auxiliares aprovadas pela ADR-0033.

### Etapa 3: Resiliência — concluída

- Timeout cooperativo, degradação, validação de saída, ablação, duplicata,
  reentrada e backpressure possuem testes focados.

### Etapa 4: RuntimeHost — concluída

- O host produz entrada versionada, persiste o resultado e impede reentrada.
- `enable_cognitive_coordinator=false` implementa o rollback documentado.

## Comandos de Validação
```bash
cmake --build build --config Debug --target all
ctest --test-dir build -C Debug --output-on-failure
uv run --frozen pytest -q
```

## Migrações
- Nenhuma.

## Rollback
- Desativar a flag de inicialização e apagar os binários novos do CMake.

## Evidências

- `cpp/tests/cognitive_coordinator_test.cpp`;
- `cpp/tests/cognitive_cycle_contracts_test.cpp`;
- `cpp/tests/runtime_host_test.cpp`;
- `cpp/benchmarks/cognitive_coordinator_queue_benchmark.cpp`;
- `docs/07-research/SPEC-045_ORCHESTRATION_VALIDATION.md`;
- relatório de execução da SPEC-045 em `reports/`.

## Critérios para Parar
- Se o tratamento de timeout envolver interrupção forçada (`pthread_cancel` / `TerminateThread`), abortar e revisar arquitetura para cancelamento cooperativo (`std::atomic_bool`).
