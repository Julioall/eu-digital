---
id: SPEC-007
title: Segmentação de episódios
status: done
phase: 2
dependencies: [SPEC-003, SPEC-004, SPEC-005, SPEC-006]
adrs: []
contracts: [EPISODE_SCHEMA.md, episode.schema.json]
---

# SPEC-007 — Segmentação de episódios

Status: done
Fase: 2
Dependências: SPEC-003, SPEC-004, SPEC-005, SPEC-006
Contratos: `EPISODE_SCHEMA.md`, `contracts/schemas/episode.schema.json`

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

## Hipótese e protocolo

- **Baseline:** `time_context_threshold_v1`, implementado no laboratório
  Python;
- **Hipótese:** lacunas temporais e mudanças de aplicação/documento observadas
  produzem limites reproduzíveis;
- **Métricas:** boundary precision, recall, F1 e WindowDiff contra episódios
  anotados;
- **Ablação:** desabilitar as divisões por aplicação/documento e comparar com
  o baseline somente temporal;
- **Falsificação:** a fusão não supera a melhor modalidade isolada em sessões
  anotadas de avaliação;
- **Promoção:** nenhuma implementação C++ é promovida por esta SPEC sem
  evidência científica e equivalência posteriores.

## Critérios de aceite
- [x] Cada limite possui motivo.
- [x] Segmentação é determinística com mesma configuração.
- [x] Métrica baseline é registrada.
