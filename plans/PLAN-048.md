# Plano de Execução: SPEC-048 (Integrated Dialogue and Decision Output)

## Objetivo
Criar o módulo `DecisionOutputRouter` para conectar a avaliação do estado cognitivo à geração real do texto via modelo local, respeitando os bloqueios do `SuggestionOrchestrator`.

## Etapas Verificáveis

### Etapa 1: Contrato do Router
- Criar `contracts/schemas/decision_routing.schema.json`.

### Etapa 2: C++ Routing
- Criar `cpp/core/decision_output_router.hpp`.
- Receber as instâncias injetadas do orquestrador e do gateway local.
- Formatar o prompt do sistema (System Prompt dinâmico) injetando as restrições e limites do `FunctionalSelfModel`.

### Etapa 3: Integração no Coordenador
- Adicionar a etapa final no `CognitiveCoordinator` (SPEC-045).
- Testar com fixture: Enviar um estado com "relevância alta" e orçamento zerado -> Deve gerar "silêncio". Enviar um com orçamento ok -> Deve invocar o modelo local.

## Arquivos Prováveis
- `cpp/core/decision_output_router.hpp`
- `cpp/tests/decision_output_router_test.cpp`
