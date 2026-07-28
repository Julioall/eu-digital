---
id: SPEC-021
title: World model e erro preditivo
status: blocked
phase: 4
dependencies: [SPEC-007, SPEC-009, SPEC-018]
adrs: []
contracts: []
---

# SPEC-021 — World model e erro preditivo

Status: blocked
Fase: 4
Dependências: SPEC-007, SPEC-009, SPEC-018

## Objetivo
Prever próximos eventos e estados e usar erro preditivo como sinal de novidade.

## Requisitos
- baseline n-gram/Markov;
- modelo incremental;
- distribuição de próximos eventos;
- log loss;
- detecção de drift;
- integração com saliência.

## Escopo negativo
Planejamento autônomo irrestrito.

## Critérios de aceite
- [ ] Supera baseline de frequência em conjunto de teste.
- [ ] Erro elevado aumenta saliência de forma auditável.
- [ ] Drift reduz confiança e inicia reaprendizagem.
