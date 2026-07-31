# Plano de Execução: SPEC-045 (Integrated Headless Cognitive Cycle)

## Pré-condições
- SPEC-047 implementada (interfaces/portas disponíveis).
- `RuntimeHost` compilando.

## Contratos Congelados
- `CanonicalEvent`.

## Arquivos por Etapa

### Etapa 1: Estados e Fila
- **Ação:** Definir os estados e a estrutura da fila com limits de backpressure.
- **Arquivos:**
  - `cpp/core/cognitive_coordinator_types.hpp` (enums de estados: `queued`, etc).
  - `cpp/core/event_queue.hpp`

### Etapa 2: Implementação do Loop
- **Ação:** Criar o `CognitiveCoordinator`. Ele receberá o `CapabilityRegistry` no construtor.
- **Arquivos:**
  - `cpp/core/cognitive_coordinator.hpp`
  - `cpp/core/cognitive_coordinator.cpp`

### Etapa 3: Integração Segura e Timeout
- **Ação:** Integrar mecanismos de cancelamento/degradação.
- **Testes (TDD):** `cpp/tests/cognitive_coordinator_test.cpp`. Adicionar testes provando que se MockPort retorna falha, estado é `degraded` e ciclo continua. Adicionar teste de ablação.

### Etapa 4: Configuração no RuntimeHost
- **Ação:** Injetar o coordenador no host.
- **Arquivos:** `cpp/core/runtime_host.cpp`

## Comandos de Validação
```bash
cmake --build build/windows-msvc --target all
ctest --test-dir build/windows-msvc -R CognitiveCoordinator --output-on-failure
```

## Migrações
- Nenhuma.

## Rollback
- Desativar a flag de inicialização e apagar os binários novos do CMake.

## Evidências Esperadas
- Log detalhado demonstrando o evento fluindo do estado `queued` até `completed`.
- Teste de degradação explícito (Ablação aprovada).

## Critérios para Parar
- Se o tratamento de timeout envolver interrupção forçada (`pthread_cancel` / `TerminateThread`), abortar e revisar arquitetura para cancelamento cooperativo (`std::atomic_bool`).
