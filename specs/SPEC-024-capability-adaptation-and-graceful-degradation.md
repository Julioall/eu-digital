---
id: SPEC-024
title: Adaptação a ausência e novas capacidades
status: active
phase: 4
dependencies: [SPEC-010, SPEC-012, SPEC-018, SPEC-021, SPEC-023]
adrs: [ADR-0006, ADR-0008, ADR-0009]
contracts: [CAPABILITY_ADAPTATION_SCHEMA.md, capability_adaptation_event.schema.json, observability_profile.schema.json, capability_onboarding.schema.json]
---

# SPEC-024 — Adaptação a ausência e novas capacidades

Status: done
Fase: 4
Dependências: SPEC-010, SPEC-012, SPEC-018, SPEC-021, SPEC-023
ADRs aplicáveis: ADR-0006, ADR-0008, ADR-0009
Contratos: `CAPABILITY_ADAPTATION_SCHEMA.md`, `capability_adaptation_event.schema.json`, `observability_profile.schema.json`, `capability_onboarding.schema.json`

## Objetivo

Adaptar atenção, confiança, world model, objetivos e planejamento quando capacidades entram, degradam ou desaparecem.

## Requisitos

- observabilidade parcial explícita;
- recalibração de confiança;
- redistribuição de atenção;
- invalidação de previsões dependentes;
- busca de equivalentes;
- explicação de limitações;
- onboarding de nova modalidade;
- histórico de capacidades no self-model.

## Escopo negativo

- simular dados de sensor ausente;
- considerar ausência de observação como evento negativo;
- apagar memórias da modalidade removida;
- atribuir neuroplasticidade biológica ao sistema.

## Critérios de aceite

- [x] Remover visão reduz confiança de hipóteses visuais.
- [x] Remover áudio não interrompe episódios baseados em sistema.
- [x] Planos que exigem atuador ausente são bloqueados.
- [x] Nova modalidade inicia calibração antes de influenciar crenças estáveis.
- [x] O retorno de uma capacidade não recria identidade.
- [x] Ablation demonstra adaptação superior a uma política fixa.
