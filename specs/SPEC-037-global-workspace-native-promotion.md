---
id: SPEC-037
title: Promoção nativa do workspace global
status: future
phase: beta
dependencies: [SPEC-010, SPEC-023, SPEC-026, SPEC-027, SPEC-029, SPEC-036]
adrs: [ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011]
contracts: [GLOBAL_WORKSPACE_SCHEMA.md, workspace_candidate.schema.json, workspace_snapshot.schema.json, workspace_broadcast.schema.json]
---

# SPEC-037 — Promoção nativa do workspace global

## Objetivo

Promover a seleção limitada e auditável de candidatos observados para um
workspace global local, com broadcast versionado, expiração e capacidade
limitada.

## Escopo negativo

Não transformar saliência em intenção, não criar tarefas fixas, não executar
ações, não substituir a cognição por regras e não enviar conteúdo para a nuvem.

## Escopo

Inclui competição determinística entre candidatos, prioridade, ocupação,
expiração, broadcast e ablações de capacidade, preservando origem e sessão.

## Protocolo científico

Hipótese: seleção limitada melhora disponibilidade de sinais relevantes sem
churn excessivo. Baseline: FIFO. Métricas: precisão/recall de seleção, churn,
ocupação e latência. Ablação: capacidade reduzida e sem priorização.
Falsificação: não superar FIFO ou aumentar churn acima do limite.

## Critérios de aceite

- [ ] Referência, invariantes, holdout e hashes são congelados.
- [ ] Snapshot e broadcast C++ validam os contratos compartilhados.
- [ ] Empates, expiração, ausência e remoção de capacidade são determinísticos.
- [ ] Baseline, ablação, métricas científicas e operacionais passam.
- [ ] O workspace não cria fatos nem planos e permanece removível.
- [ ] Nenhuma integração com diálogo, avatar ou ação é antecipada.

## Saída

Candidato nativo equivalente para seleção local limitada, ainda indisponível no
produto até revisão de promoção.
