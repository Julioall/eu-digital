# Plano de Execução: SPEC-049 (Supervised Action Cycle Integration)

## Pré-condições
- SPEC-016 totalmente validada e contratos disponíveis.
- SPEC-048 (Decisão Estruturada) implementada.

## Contratos Congelados
- `ActionPlan`, `ActionSimulation`, `ActionAuthorization`, `ActionOutcome` (SPEC-016).
- `CanonicalEvent`.

## Arquivos por Etapa

### Etapa 1: Ingestão de Outcome
- **Ação:** Criar o sensor nativo de conversão de `ActionOutcome` para `CanonicalEvent`.
- **Arquivos:**
  - `cpp/core/action_outcome_sensor.hpp`

### Etapa 2: Idempotência e Autorização
- **Ação:** Implementar o controle de Idempotency Key vinculado ao digest do plano, roteando da decisão para a especificação de ações.
- **Arquivos:**
  - `cpp/core/action_dispatcher.hpp`
- **Testes Antes do Código:** `cpp/tests/action_dispatcher_test.cpp`. Garantir que submeter a mesma key duas vezes bloqueia a segunda silenciosamente.

### Etapa 3: Integração no Self-Model
- **Ação:** Atualizar o `FunctionalSelfModel` (via adaptador, SPEC-047) para aplicar a função de decaimento de confiabilidade ao processar o evento de outcome.
- **Arquivos:**
  - `cpp/core/functional_self_model.cpp` (Ajustar lógica de peso).

## Comandos de Validação
```bash
cmake --build build/windows-msvc --target all
ctest --test-dir build/windows-msvc -R ActionDispatcher --output-on-failure
```

## Migrações
- Nenhuma.

## Rollback
- Desconectar o ActionDispatcher da saída de Decisões do Coordenador.

## Evidências Esperadas
- Relatório de testes exibindo a curva de decaimento: falha não zera a capacidade, 3 falhas zeram.
- Teste passando de crash gerando `outcome_unknown`.

## Critérios para Parar
- Se o tratamento de Idempotência necessitar lock distribuído complexo, interromper e revisar se um cache simples em memória baseado em Hash resolve.
