---
id: SPEC-020
title: Consolidação e esquecimento
status: done
phase: 3
dependencies: [SPEC-008, SPEC-018]
adrs: [ADR-0007, ADR-0020]
contracts: [MEMORY_CONSOLIDATION_SCHEMA.md]
---

# SPEC-020 — Consolidação e esquecimento

Status: done
Fase: 3
Dependências: SPEC-008, SPEC-018
ADRs aplicáveis: ADR-0007, ADR-0020
Contrato afetado: MEMORY_CONSOLIDATION_SCHEMA.md

## Objetivo
Consolidar episódios em conhecimento semântico, aplicar replay e controlar retenção.

## Requisitos
- fila de consolidação;
- replay;
- generalização com proveniência;
- reconciliação;
- decay e arquivamento;
- métricas de retenção.

## Escopo negativo
Apagar fonte de uma crença consolidada.

## Critérios de aceite
- [x] Replay reduz esquecimento no corpus.
- [x] Conhecimento consolidado aponta para episódios.
- [x] Políticas de retenção são reversíveis em teste.

## Implementação e testes

- python/eu_digital_lab/memory_consolidation.py implementa replay,
  reconciliação, conhecimento versionado e retenção reversível.
- python/tests/test_memory_consolidation.py cobre baseline, retenção,
  restauração, proveniência, contradições e custo.
- Os três schemas de consolidação versionam conhecimento, replay e retenção.
