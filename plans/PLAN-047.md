# Plano de Execução: SPEC-047 (Component Wiring, Ports and Contracts)

## Pré-condições
- `SPEC-023` implementada (CapabilityRegistry disponível).
- Testes unitários atuais passando (`ctest`).

## Contratos Congelados
- `CanonicalEvent` não será modificado.

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
- **Testes (TDD):** `cpp/tests/ports_mock_test.cpp` (verificar GMock instantiation).

### Etapa 2: DTOs Imutáveis
- **Ação:** Criar os Data Transfer Objects.
- **Arquivos:**
  - `cpp/core/contracts/episode_update.hpp`
  - `cpp/core/contracts/memory_write_result.hpp`
  - `cpp/core/contracts/prediction_assessment.hpp`
  - (etc)

### Etapa 3: Adaptadores
- **Ação:** Criar adaptadores finais.
- **Arquivos:**
  - `cpp/core/adapters/world_model_adapter.hpp` (implementa `IPredictionPort`, wrap `WorldModel`)
  - `cpp/core/adapters/episodic_memory_adapter.hpp`
  - `cpp/tests/world_model_adapter_test.cpp`

### Etapa 4: Integração no Registry
- **Ação:** Registrar os adaptadores no startup.
- **Arquivos:** `cpp/core/runtime_host.cpp`

## Comandos de Validação
```bash
cmake --build build/windows-msvc --target all
ctest --test-dir build/windows-msvc --output-on-failure
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
