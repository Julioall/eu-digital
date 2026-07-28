---
id: SPEC-019
title: Propriocepção e agência digital
status: blocked
phase: 4
dependencies: [SPEC-002, SPEC-010, SPEC-012, SPEC-018]
adrs: [ADR-0006]
contracts: []
---

# SPEC-019 — Propriocepção e agência digital

Status: blocked
Fase: 4
Dependências: SPEC-002, SPEC-010, SPEC-012, SPEC-018
ADRs: ADR-0006

## Objetivo
Representar estado interno e aprender contingências entre ações próprias e efeitos.

## Requisitos
- DigitalBodyState;
- ActionIntention;
- EfferenceCopy;
- ActionOutcome;
- AgencyAttribution;
- ações internas reversíveis;
- previsão de efeito.

## Escopo negativo
Ações destrutivas e alegação de self fenomenal.

## Critérios de aceite
- [ ] Distingue efeitos próprios e externos acima do baseline.
- [ ] Toda ação possui intenção e resultado correlacionados.
- [ ] Ablation do loop reduz atribuição de agência.
