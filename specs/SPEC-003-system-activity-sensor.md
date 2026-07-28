---
id: SPEC-003
title: Sensor de atividade do sistema
status: done
phase: 1
dependencies: [SPEC-002, SPEC-023]
adrs: []
contracts: []
---

# SPEC-003 — Sensor de atividade do sistema

Status: done
Fase: 1
Dependências: SPEC-002, SPEC-023

## Objetivo
Capturar processos, janela ativa, abertura, fechamento e mudança de foco.

## Requisitos
- Adaptador Windows inicial.
- Polling ou hooks encapsulados.
- Eventos normalizados.
- Health check.
- Reconexão automática.

## Escopo negativo
Conteúdo da tela e automação de aplicativos.

## Critérios de aceite
- [x] Detecta mudança de janela ativa.
- [x] Detecta início e fim de processo observável.
- [x] Uso médio de CPU abaixo do orçamento definido no plano.
- [x] Falha de permissão não encerra o agente.
