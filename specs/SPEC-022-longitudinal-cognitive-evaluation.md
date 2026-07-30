---
id: SPEC-022
title: Avaliação longitudinal
status: done
phase: 6
dependencies: [SPEC-018, SPEC-019, SPEC-020, SPEC-021]
adrs: [ADR-0005, ADR-0008, ADR-0022]
contracts: [LONGITUDINAL_EVALUATION_SCHEMA.md, longitudinal_protocol.schema.json, longitudinal_snapshot.schema.json, longitudinal_report.schema.json]
---

# SPEC-022 — Avaliação longitudinal

Status: done
Fase: 6
Dependências: SPEC-018, SPEC-019, SPEC-020, SPEC-021
ADRs aplicáveis: ADR-0005, ADR-0008, ADR-0022
Contratos: `LONGITUDINAL_EVALUATION_SCHEMA.md`, `longitudinal_protocol.schema.json`, `longitudinal_snapshot.schema.json`, `longitudinal_report.schema.json`

## Objetivo
Avaliar evolução, retenção, deriva, contradição e utilidade em 7, 30 e 90 dias.

## Requisitos
- snapshots;
- testes congelados;
- curvas de retenção;
- métricas de calibração;
- relatório de mudança;
- comparação com baseline cronológico.

## Escopo negativo
Alterar métricas após observar resultados.

## Critérios de aceite
- [x] Resultados são reproduzíveis a partir de snapshots.
- [x] Ganhos e perdas são reportados.
- [x] Deriva de self-model é quantificada.
