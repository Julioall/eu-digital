# Plano de Execução: SPEC-048 (Structured Cognitive Output and Dialogue)

## Pré-condições
- SPEC-045 implementada.
- Modelo local mock disponível nas ferramentas de build.

## Contratos Congelados
- `CanonicalEvent`.

## Arquivos por Etapa

### Etapa 1: Definição dos Contratos DTO
- **Ação:** Criar as estruturas de saída.
- **Arquivos:**
  - `contracts/schemas/cognitive_output.schema.json`
  - `cpp/core/contracts/cognitive_output_request.hpp`
  - `cpp/core/contracts/validated_dialogue_output.hpp`

### Etapa 2: A Política e o Renderizador
- **Ação:** Separar decisão de geração linguística.
- **Arquivos:**
  - `cpp/core/cognitive_decision_policy.hpp`
  - `cpp/core/local_language_renderer.hpp`
  - `cpp/core/ports/ipresentation_port.hpp`

### Etapa 3: Testes Focados (TDD)
- **Ação:** Escrever suíte rigorosa de timeouts e orçamentos.
- **Arquivos:**
  - `cpp/tests/cognitive_decision_policy_test.cpp` (verificar distinção entre resposta direta e sugestão).
  - `cpp/tests/local_language_renderer_test.cpp` (mock timeout -> fallback, json corrompido -> fallback, saída nula -> fallback).

### Etapa 4: Integração Final
- **Ação:** Acoplar no fim do `CognitiveCoordinator`.

## Comandos de Validação
```bash
cmake --build build/windows-msvc --target all
ctest --test-dir build/windows-msvc -R LanguageRenderer --output-on-failure
```

## Migrações
- Nenhuma.

## Rollback
- Remover arquivos criados do `CMakeLists.txt`.

## Evidências Esperadas
- Relatório de testes exibindo 100% de passagem nos casos degenerados de LLM (timeout, erro, json_parse_error).

## Critérios para Parar
- Se o LLM local demorar mais de 3 segundos para responder ao cancelamento (join da thread block), é preciso parar e reimplementar o binding do modelo usando subprocessos isolados em vez de chamadas FFI síncronas bloqueantes.
