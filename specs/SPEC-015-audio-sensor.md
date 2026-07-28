---
id: SPEC-015
title: Áudio ambiente
status: future
phase: 9
dependencies: [SPEC-002, SPEC-023]
adrs: []
contracts: []
---

# SPEC-015 — Áudio ambiente

Status: future
Fase: 9
Dependências: SPEC-002, SPEC-023

## Objetivo
Capturar áudio local, detectar fala e produzir transcrições temporais.

## Requisitos
- VAD local.
- Transcrição local.
- Segmentos com confiança.
- Associação temporal com demais eventos.
- Áudio bruto referenciado.

## Escopo negativo
Inferência de verdade, intenção ou emoção apenas pela voz.

## Critérios de aceite
- [ ] Segmentos possuem timestamps.
- [ ] Transcrição falha não quebra timeline.
- [ ] Custo de processamento é medido.
