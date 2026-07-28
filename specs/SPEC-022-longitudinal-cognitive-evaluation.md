---
id: SPEC-022
title: Avaliação longitudinal
status: future
phase: 6
dependencies: [SPEC-018, SPEC-019, SPEC-020, SPEC-021]
adrs: []
contracts: []
---

# SPEC-022 — Avaliação longitudinal

Status: future
Fase: 6
Dependências: SPEC-018, SPEC-019, SPEC-020, SPEC-021

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
- [ ] Resultados são reproduzíveis a partir de snapshots.
- [ ] Ganhos e perdas são reportados.
- [ ] Deriva de self-model é quantificada.
