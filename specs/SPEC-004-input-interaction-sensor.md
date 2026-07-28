---
id: SPEC-004
title: Sensor de interação
status: done
phase: 1
dependencies: [SPEC-002, SPEC-023]
adrs: []
contracts: []
---

# SPEC-004 — Sensor de interação

Status: done
Fase: 1
Dependências: SPEC-002, SPEC-023

## Objetivo
Registrar eventos de teclado, mouse, atalhos, clipboard e atividade de entrada.

## Requisitos
- Suporte a eventos brutos e agregados configurável.
- Associação com janela ativa.
- Taxa de digitação, pausas e atalhos.
- Payload versionado.

## Escopo negativo
Interpretação de intenção e execução de entrada.

## Critérios de aceite
- [x] Eventos possuem contexto de janela.
- [x] Alto volume é agregado sem perda de métricas.
- [x] Clipboard produz evento separado.
