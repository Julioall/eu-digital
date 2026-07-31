---
id: SPEC-043
title: Orquestração sugestiva e decisão de interrupção
status: done
phase: beta
dependencies: [SPEC-039, SPEC-040, SPEC-042]
adrs: [ADR-0003, ADR-0004, ADR-0005, ADR-0009, ADR-0010, ADR-0011]
contracts: [dialogue_notice.schema.json, dialogue_feedback.schema.json, avatar_view_state.schema.json, observability_profile.schema.json, local_model_request.schema.json, local_model_response.schema.json, suggestion_decision.schema.json]
---

# SPEC-043 — Orquestração sugestiva e decisão de interrupção

## Objetivo

Conectar observações consentidas, episódios, memória, padrões, previsões,
self-model e metacognição a sugestões explicáveis, corrigíveis e silenciosas
quando apropriado.

## Escopo negativo

Não executar ferramentas ou ações, não transformar sugestão em tarefa fixa, não
interromper sem orçamento, não esconder incerteza e não usar fallback semântico
quando o modelo estiver ausente.

## Escopo

Inclui `SuggestionDecision` versionado com policy, orçamento antes/depois,
supressão, override, evidência, confiança e ganho informacional; limites
configuráveis de interrupção; feedback correct/defer/silence e entrega ao
avatar.

## Protocolo

Baseline: entrega fixa de notificações. Métricas: aceitação, correção,
redundância, silêncio, ganho e interrupções. Ablação: sem metacognição e sem
orçamento. Falsificação: piora de correção, excesso de interrupções ou falta de
explicação.

## Critérios de aceite

- [x] Toda sugestão tem evidência, confiança, motivo e ação corretiva.
- [x] Política padrão limita 3/15min, cooldown 5min, 30min após correção e
      8/dia, com versão e configuração auditáveis.
- [x] Ausência de modelo degrada explicitamente sem fallback semântico.
- [x] Nenhuma sugestão executa ação no sistema.
- [x] Holdout, ablação, replay, acessibilidade e benchmarks passam.

## Saída

Product Beta sugestivo, local e corrigível, ainda sem ações autônomas.

## Evidência

O `SuggestionOrchestrator` nativo em `cpp/core/suggestion_orchestrator.hpp`
implementa policy versionada, budget, cooldown, redundancy suppression,
model-absent degradation e zero-action invariant. A referência Python em
`python/reference/suggestion_orchestrator.py` valida a mesma semântica.
C++ 75/75 testes e Python 26/26 testes passam. O contrato
`suggestion_decision.schema.json` garante `action_proposed: false`.

