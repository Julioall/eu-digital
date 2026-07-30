---
id: SPEC-021
title: World model e erro preditivo
status: done
phase: 4
dependencies: [SPEC-007, SPEC-009, SPEC-018]
adrs: [ADR-0021]
contracts: [WORLD_MODEL_PREDICTION_SCHEMA.md, world_model_prediction.schema.json, prediction_error.schema.json, prediction_drift.schema.json]
---

# SPEC-021 — World model e erro preditivo

Status: done
Fase: 4
Dependências: SPEC-007, SPEC-009, SPEC-018
ADRs aplicáveis: ADR-0005, ADR-0008, ADR-0021
Contratos: `WORLD_MODEL_PREDICTION_SCHEMA.md`, `world_model_prediction.schema.json`, `prediction_error.schema.json`, `prediction_drift.schema.json`

## Objetivo
Prever próximos eventos e estados e usar erro preditivo como sinal de novidade.

## Requisitos
- baseline n-gram/Markov;
- modelo incremental;
- distribuição de próximos eventos;
- log loss;
- detecção de drift;
- integração com saliência.

## Escopo negativo
Planejamento autônomo irrestrito.

## Critérios de aceite
- [x] Supera baseline de frequência em conjunto de teste.
- [x] Erro elevado aumenta saliência de forma auditável.
- [x] Drift reduz confiança e inicia reaprendizagem.
