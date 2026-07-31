---
id: SPEC-049
title: Supervised Action Cycle Integration
status: draft
phase: design
dependencies: [SPEC-016, SPEC-045]
adrs: []
contracts: []
---

# SPEC-049 — Supervised Action Cycle Integration

Status: draft  
Owner: humano  
Fase: design  
Dependências: SPEC-016 (Supervised Actions), SPEC-045 (Integrated Cycle)  
ADRs aplicáveis: Nenhuma  
Contratos afetados: Nenhum, usa os existentes.

## Problema
A SPEC-016 já define de forma robusta como uma ação é planejada, simulada, autorizada e executada com rollbacks e checksums. Porém, a SPEC-049 original falhou ao tentar reinventar essa roda criando um `ActionExecutor`. O problema real a ser resolvido é como o `CognitiveDecision` do novo ciclo cognitivo (SPEC-045) interage com a infraestrutura madura da SPEC-016, e como o feedback (falha ou sucesso) realimenta o self-model de maneira segura, sem destruir a confiança após uma única falha temporária.

## Objetivo
Amarrar o pipeline cognitivo (que decide agir) ao pipeline de ações (que executa) reaproveitando integralmente os contratos e etapas da SPEC-016. Estabelecer o fluxo de ingestão do `ActionOutcome` de volta para o `EventBus` e definir a política de classificação de falhas antes de degradar o `FunctionalSelfModel`.

## Resultado observável
Ao decidir executar uma ação, o coordenador despacha um `ActionPlan` (via porta). Após a execução, o EventBus emite um `CanonicalEvent` do tipo `action_outcome`. O self-model ajusta gradativamente sua confiabilidade na ação com base em uma política e não arbitrariamente, respeitando a chave de idempotência para evitar loops de execução.

## Requisitos funcionais
- Conectar a saída do Coordenador Cognitivo à entrada de `ActionPlan`.
- O ciclo deve prover uma `idempotency key` para cada decisão de ação, garantindo a semântica de execução *at most once*.
- Integrar a re-entrada do `ActionOutcome` no `EventBus` como um sensor nativo.
- Impedir que falhas operacionais temporárias (ex: permissão negada, timeout) desativem instantaneamente a capacidade no Self Model.
- Definir estado `outcome_unknown` para casos de crash durante execução.

## Requisitos não funcionais
- Nenhuma modificação nos fluxos estruturados da SPEC-016.
- A re-entrada do evento de outcome deve possuir marcação estrita (proveniência) para não desencadear um loop infinito de "Decide agir -> Falha -> Analisa a falha -> Decide agir para corrigir a falha...".

## Entradas
- `CognitiveDecision` ordenando uma ação.

## Saídas
- `ActionOutcome` convertido em `CanonicalEvent`.

## Fluxo
1. `CognitiveDecision` produz intenção de agir e envia para `ActionPlan`.
2. Segue SPEC-016: Simulacro -> Autorização (com expiração e digest) -> Resolução -> Execução pelo plugin.
3. Se crasha durante: estado fica `outcome_unknown`.
4. Resulta em `ActionOutcome`.
5. Mapeado para `CanonicalEvent`. Entra no `EventBus`.
6. Ciclo Cognitivo reinicia. O SelfModel aplica a *Política de Confiabilidade* sobre a falha e atualiza seu peso interno.

## Estados e transições
- Todos os estados e transições herdam da SPEC-016.
- Novo estado auxiliar persistente: `outcome_unknown`.

## Erros esperados
- Tratados integralmente pelo rollback da SPEC-016.

## Escopo negativo
- Não recriar autorização ou execução.
- Não permitir ações destrutivas irreversíveis (bloqueado pela política).
- Não criar "agentes filhos" autônomos dentro do plugin de ação.

## Critérios de aceite
- [ ] A arquitetura prova reuso 100% de `ActionPlan`, `ActionSimulation`, e `ActionAuthorization`.
- [ ] O sistema aplica *idempotency key* vinculada ao digest do plano; repetição de processamento é barrada silenciosamente (semântica "at most once").
- [ ] Se o sistema morre (kill) após autorizar, mas antes do resultado voltar, no religamento o registro expira e é considerado `outcome_unknown`.
- [ ] O `FunctionalSelfModel` não zera sua probabilidade de sucesso por causa de um único erro de permissão (deve haver uma função de decaimento de confiança).

## Plano de testes

### Unitários
- Mock do pipeline de ação retornando `outcome_unknown`. Validar reação da memória e self-model.
- Teste de decaimento: simular 3 falhas seguidas -> confiança cai; 1 sucesso -> sobe.

### Integração
- Forçar kill no meio da execução. Religamento gera notificação de `outcome_unknown`.

### Contrato
- O `ActionOutcome` original se converte sem perdas para `CanonicalEvent`.

### Desempenho
- I/O é irrelevante, já operado assincronamente.

### Recuperação
- Se ocorre `outcome_unknown`, o sistema emite alerta visual na próxima sessão, exigindo reconciliação manual (auditoria completa).

## Migração
- Nenhuma estrutural.

## Rollback
- Reverter o roteamento de saída de decisão, barrando o acionamento de `ActionPlan`.

## Evidências de conclusão
- Log completo do fluxo de idempotência impedindo a execução dupla de um mesmo digest.
