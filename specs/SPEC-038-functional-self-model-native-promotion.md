---
id: SPEC-038
title: Promoção nativa do self-model funcional
status: future
phase: beta
dependencies: [SPEC-012, SPEC-023, SPEC-026, SPEC-027, SPEC-029, SPEC-037]
adrs: [ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011]
contracts: [FUNCTIONAL_SELF_MODEL_SCHEMA.md, self_model.schema.json, functional_self_model_snapshot.schema.json, self_model_decision.schema.json]
---

# SPEC-038 — Promoção nativa do self-model funcional

## Objetivo

Promover um modelo funcional e auditável das próprias capacidades, limitações,
versões e mudanças de observabilidade, sem afirmar identidade, consciência ou
experiência subjetiva.

## Escopo negativo

Não alegar self fenomenal, emoção, intenção ou consciência; não inventar
capacidade ausente; não executar planos e não esconder remoções ou falhas.

## Escopo

Inclui snapshots versionados, decisões condicionadas a capabilities,
invalidação de planos incompatíveis e histórico de mudanças de capacidade.

## Protocolo científico

Hipótese: um self-model funcional reduz decisões incompatíveis com capacidades
disponíveis. Baseline: decisão sem snapshot. Métricas: incompatibilidades,
explicabilidade, estabilidade e recuperação. Ablação: remoção do snapshot e
capacidade ausente. Falsificação: não reduzir incompatibilidades.

## Critérios de aceite

- [ ] Contratos de snapshot/decisão são reproduzidos por C++.
- [ ] Ausência, falha, remoção e reinstalação atualizam versão e histórico.
- [ ] Decisões explicam limitações sem inventar observações.
- [ ] Holdout, ablação, replay e benchmarks passam.
- [ ] O mecanismo é removível e não declara estados mentais reais.

## Saída

Candidato funcional nativo e auditável, sem disponibilidade de produto antes da
promoção formal.
