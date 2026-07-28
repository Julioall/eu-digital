---
id: SPEC-007
title: Segmentação de episódios
status: blocked
phase: 2
dependencies: [SPEC-003, SPEC-004, SPEC-005, SPEC-006]
adrs: []
contracts: [EPISODE_SCHEMA.md]
---

# SPEC-007 — Segmentação de episódios

Status: blocked
Fase: 2
Dependências: SPEC-003, SPEC-004, SPEC-005, SPEC-006
Contratos: `EPISODE_SCHEMA.md`

## Objetivo
Agrupar eventos em episódios coerentes sem exigir rótulos prévios.

## Requisitos
- Baseline por tempo e mudança de contexto.
- Features de aplicação, documento, entrada e OCR.
- Limites explicáveis.
- Reprocessamento offline.
- Métricas contra conjunto anotado.

## Escopo negativo
Nomeação por LLM e aprendizagem de hábitos.

## Critérios de aceite
- [ ] Cada limite possui motivo.
- [ ] Segmentação é determinística com mesma configuração.
- [ ] Métrica baseline é registrada.
