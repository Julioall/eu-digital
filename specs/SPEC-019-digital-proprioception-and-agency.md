---
id: SPEC-019
title: Propriocepção e agência digital
status: done
phase: 4
dependencies: [SPEC-002, SPEC-010, SPEC-012, SPEC-018]
adrs: [ADR-0006, ADR-0019]
contracts: [DIGITAL_PROPRIOCEPTION_AGENCY_SCHEMA.md]
---

# SPEC-019 — Propriocepção e agência digital

Status: done
Fase: 4
Dependências: SPEC-002, SPEC-010, SPEC-012, SPEC-018
ADRs aplicáveis: ADR-0006, ADR-0019
Contrato afetado: DIGITAL_PROPRIOCEPTION_AGENCY_SCHEMA.md

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

## Hipótese e protocolo

- H7: a previsão vinculada a ações reversíveis melhora atribuição própria e
  externa contra o observador passivo;
- baseline: passive_observer_v0, pela mesma interface e sem cópia eferente;
- métricas: macro-F1 de atribuição em holdout, erro preditivo e adaptação;
- ablação: remover o loop e manter o observador passivo;
- falsificação: o tratamento não supera o baseline, ou ausência de correlação
  é classificada como efeito externo;
- limite: atribuição operacional não demonstra intenção subjetiva ou
  consciência.

## Critérios de aceite
- [x] Distingue efeitos próprios e externos acima do baseline.
- [x] Toda ação possui intenção e resultado correlacionados.
- [x] Ablation do loop reduz atribuição de agência.

## Implementação e testes

- python/eu_digital_lab/digital_proprioception_agency.py implementa a
  referência determinística e as políticas treatment/baseline.
- python/tests/test_digital_proprioception_agency.py cobre contratos, estado,
  correlação própria, externo explícito, ambiguidade, ablação e replay.
- Os cinco schemas de propriocepção/agência versionam as estruturas públicas.
