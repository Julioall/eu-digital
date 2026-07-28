---
id: SPEC-017
title: Sandbox e corpus de avaliação
status: done
phase: 0.5
dependencies: [SPEC-001]
adrs: [ADR-0005, ADR-0008]
contracts: []
---

# SPEC-017 — Sandbox e corpus de avaliação

Status: done
Fase: 0.5
Dependências: SPEC-001
ADRs: ADR-0005, ADR-0008

## Objetivo
Criar ambiente reprodutível e corpus anotado para testar sensores, episódios, padrões e agência.

## Requisitos
- gerador de rotinas sintéticas;
- ground truth de eventos, episódios e causalidade;
- ferramenta de anotação humana;
- separação treino, desenvolvimento e teste;
- versionamento de corpus.

## Escopo negativo
Inferência cognitiva de produção.

## Critérios de aceite
- [x] Uma sessão é reproduzível por seed.
- [x] Ground truth possui schema versionado.
- [x] Concordância entre anotadores é calculada.
- [x] Corpus não depende do LLM.
