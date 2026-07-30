---
id: SPEC-036
title: Promoção nativa do world model e erro preditivo
status: future
phase: beta
dependencies: [SPEC-021, SPEC-023, SPEC-026, SPEC-027, SPEC-029, SPEC-035]
adrs: [ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011]
contracts: [WORLD_MODEL_PREDICTION_SCHEMA.md, prediction_error.schema.json, prediction_drift.schema.json]
---

# SPEC-036 — Promoção nativa do world model e erro preditivo

## Objetivo

Promover um mecanismo local de previsão de transições observadas e cálculo de
erro preditivo, usando padrões promovidos como entrada e preservando incerteza,
calibração e drift.

## Escopo negativo

Não inferir estados não observados como fatos, não controlar sensores, não
executar ações, não chamar modelo generativo e não alegar compreensão do mundo.

## Escopo

Inclui previsão determinística, distribuição top-k, erro observado versus
predito, calibração, drift explícito, replay e degradação quando faltarem
observações ou padrões.

## Protocolo científico

Hipótese: padrões promovidos melhoram previsão de transições em holdout sem
ocultar incerteza. Baseline: frequência/exata. Métricas: log loss, top-k,
calibração, erro preditivo e drift. Ablação: sem padrões e sem contexto.
Falsificação: o tratamento não supera o baseline ou apresenta calibração pior.

## Critérios de aceite

- [ ] Referência congelada, fixtures, ground truth, invariantes e holdout têm
      hashes registrados.
- [ ] Candidato C++ reproduz os schemas de previsão, erro e drift.
- [ ] Ausência de observação não gera estado negativo nem previsão certa.
- [ ] Baseline, ablação, calibração e benchmarks p50/p95/máximo passam.
- [ ] Plugin é removível e a maturidade permanece indisponível sem promoção.
- [ ] Não há integração com workspace, self-model, diálogo ou ações.

## Saída

Candidato nativo auditável para previsão local; não é implementação de produto
até promoção aprovada.
