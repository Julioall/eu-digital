# Plano de Execução: SPEC-047 (Component Wiring, Ports and Contracts)

## Pré-condições
- `SPEC-023` implementada (CapabilityRegistry disponível).
- Testes unitários atuais passando (`ctest`).

## Contratos Congelados
- `CanonicalEvent` não será modificado.
- `PortResult<T>` 1.0 será aditivo; as assinaturas legadas permanecem até a
  migração coordenada pela SPEC-045.

## Decisão contratual de 2026-08-04

Foram aprovados DTOs 1.0 aditivos para episódio, memória, workspace,
metacognição e decisão. Adapters concretos rejeitam entradas legadas
insuficientes em vez de fabricar campos. As assinaturas anteriores permanecem
compiláveis até a migração da SPEC-045.

## Arquivos por Etapa

### Etapa 1: Portas Virtuais (Abstrações)
- **Ação:** Criar as interfaces virtuais C++.
- **Arquivos:** 
  - `cpp/core/ports/iepisode_boundary_port.hpp`
  - `cpp/core/ports/iprediction_port.hpp`
  - `cpp/core/ports/imemory_write_port.hpp`
  - `cpp/core/ports/imemory_retrieval_port.hpp`
  - `cpp/core/ports/iworkspace_selection_port.hpp`
  - `cpp/core/ports/imetacognition_port.hpp`
  - `cpp/core/ports/iself_model_query_port.hpp`
  - `cpp/core/ports/ipattern_learning_port.hpp`
- **Testes (TDD):** `cpp/tests/ports_mock_test.cpp` (verificar GMock instantiation).

### Etapa 2: DTOs Imutáveis
- **Ação:** Criar os Data Transfer Objects.
- **Arquivos:**
  - `cpp/core/contracts/episode_update.hpp`
  - `cpp/core/contracts/memory_write_result.hpp`
  - `cpp/core/contracts/prediction_assessment.hpp`
  - `cpp/core/contracts/pattern_learning.hpp`
  - `cpp/core/contracts/port_result.hpp`
  - `contracts/schemas/port_result.schema.json`
  - `cpp/core/contracts/cognitive_port_requests.hpp`
  - `contracts/schemas/episode_port_request.schema.json`
  - `contracts/schemas/workspace_port_request.schema.json`
  - `contracts/schemas/metacognition_port_request.schema.json`
  - `contracts/schemas/cognitive_decision_request.schema.json`
  - (etc)

### Etapa 3: Adaptadores
- **Ação:** Criar adaptadores finais.
- **Arquivos:**
  - `cpp/core/adapters/world_model_adapter.hpp` (implementa `IPredictionPort`, wrap `WorldModel`)
  - `cpp/core/adapters/episodic_memory_adapter.hpp`
  - `cpp/core/adapters/pattern_learner_adapter.hpp`
  - `cpp/tests/world_model_adapter_test.cpp`
  - `cpp/tests/pattern_learner_adapter_test.cpp`
  - `cpp/tests/port_result_test.cpp`

### Etapa 3.1: Remoção de dados fabricados
- **Ação:** adicionar operações 1.0 suficientes e mapear cada campo para o
  componente promovido correspondente.
- **Compatibilidade:** manter assinaturas legadas; adapters concretos retornam
  `PortResult<T>` falho quando a entrada não pode ser representada fielmente.
- **Teste:** `cpp/tests/cognitive_port_requests_test.cpp` e testes focados dos
  seis adapters afetados.
- **Limite:** não migrar o `CognitiveCoordinator`; integração pertence à
  SPEC-045.

### Etapa 4: Integração no Registry
- **Ação:** Registrar os adaptadores no startup.
- **Arquivos:** `cpp/core/runtime_host.cpp`

## Comandos de Validação
```bash
cmake --build build/windows-msvc --target all
ctest --test-dir build/windows-msvc --output-on-failure
cmake --build build/perf --target port_dispatch_benchmark
./build/perf/port_dispatch_benchmark
```

## Migrações
- Nenhuma migração de banco de dados necessária.

## Rollback
- Remover a inclusão da pasta `cpp/core/ports/` e `cpp/core/adapters/` do `CMakeLists.txt` e reverter o PR.

## Evidências Esperadas
- C++ headers sem violações de ODR.
- Testes de TDD passando, incluindo testes explícitos de ausência de módulo e substituição.

## Critérios para Parar
- Se a compilação falhar devido a dependências circulares entre as DTOs, abortar e revisar a inclusão dos headers.
- Se o overhead mediano de despacho virtual exceder 1%, manter a SPEC
  `in_progress` e não mascarar o resultado ajustando o benchmark.
- Se uma porta não possuir dados suficientes para delegar fielmente ao módulo
  promovido, registrar a lacuna contratual e não fabricar valores no adapter.
