# Plano de Execução: SPEC-049 (Supervised Action Feedback Loop)

## Objetivo
Implementar o pipeline assíncrono que aprova, executa e re-ingere o resultado de ações sugeridas.

## Etapas Verificáveis

### Etapa 1: Contrato do Resultado
- Modificar (se necessário) o schema `canonical_event` para suportar `action_result`.

### Etapa 2: Implementação C++ (Execution Pipeline)
- Criar `cpp/core/action_executor.hpp`.
- Integrar com o `CapabilityRegistry` para acionar o plugin/script externo da ação solicitada, se aprovado.
- Teste Unitário: Mock do plugin retornar falha, garantir que o executor injeta o `action_result` failed de volta no EventBus.

### Etapa 3: Feedback no Ciclo
- Modificar o `CognitiveCoordinator` para reconhecer o `action_result` e sinalizar ao `FunctionalSelfModel` a variação de confiança.

## Arquivos Prováveis
- `cpp/core/action_executor.hpp`
- `cpp/tests/action_executor_test.cpp`
