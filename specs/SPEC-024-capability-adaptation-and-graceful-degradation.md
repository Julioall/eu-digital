---
id: SPEC-024
title: Adaptação a ausência e novas capacidades
status: blocked
phase: 4
dependencies: [SPEC-010, SPEC-012, SPEC-018, SPEC-021, SPEC-023]
adrs: [ADR-0006, ADR-0008, ADR-0009]
contracts: []
---

# SPEC-024 — Adaptação a ausência e novas capacidades

Status: blocked
Fase: 4
Dependências: SPEC-010, SPEC-012, SPEC-018, SPEC-021, SPEC-023
ADRs aplicáveis: ADR-0006, ADR-0008, ADR-0009

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

- [ ] Remover visão reduz confiança de hipóteses visuais.
- [ ] Remover áudio não interrompe episódios baseados em sistema.
- [ ] Planos que exigem atuador ausente são bloqueados.
- [ ] Nova modalidade inicia calibração antes de influenciar crenças estáveis.
- [ ] O retorno de uma capacidade não recria identidade.
- [ ] Ablation demonstra adaptação superior a uma política fixa.
