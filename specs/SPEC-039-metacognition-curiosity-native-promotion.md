---
id: SPEC-039
title: Promoção nativa da metacognição e curiosidade
status: future
phase: beta
dependencies: [SPEC-011, SPEC-023, SPEC-026, SPEC-027, SPEC-029, SPEC-038]
adrs: [ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011]
contracts: [METACOGNITION_CURIOSITY_SCHEMA.md, metacognitive_assessment.schema.json, curiosity_question.schema.json, curiosity_response.schema.json]
---

# SPEC-039 — Promoção nativa da metacognição e curiosidade

## Objetivo

Promover avaliação explícita de confiança, alternativas, lacunas e ganho
informacional, com perguntas locais limitadas e respostas inconclusivas
preservadas.

## Escopo negativo

Não simular emoção, consciência ou intenção; não converter curiosidade em ação,
não pressionar o usuário, não tratar silêncio como evidência negativa e não
usar fallback semântico por regras para fingir diálogo.

## Escopo

Inclui assessments, hipóteses, ganho informacional, orçamento de perguntas,
feedback e calibração, com correção/defer/silence futuros tratados por contrato.

## Protocolo científico

Hipótese: assessments e ganho informacional reduzem perguntas redundantes e
melhoram calibração. Baseline: pergunta fixa. Métricas: calibração, redundância,
correção, supressão e ganho. Ablação: sem assessment e sem orçamento.
Falsificação: não reduzir redundância ou piorar calibração.

## Critérios de aceite

- [ ] Cada pergunta referencia assessment, evidência e ganho esperado.
- [ ] Inconclusão não vira observação negativa.
- [ ] Orçamento, cooldown e supressão são versionados e auditáveis.
- [ ] Holdout, ablação, baseline, replay e benchmarks passam.
- [ ] Nenhuma pergunta executa ação ou declara emoção real.

## Saída

Candidato nativo de metacognição/curiosidade, ainda sem diálogo generativo ou
disponibilidade de produto.
