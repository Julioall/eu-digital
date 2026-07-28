---
id: SPEC-012
title: Modelo de si funcional
status: blocked
phase: 4
dependencies: [SPEC-010, SPEC-011]
adrs: []
contracts: [SELF_MODEL_SCHEMA.md]
---

# SPEC-012 — Modelo de si funcional

Status: blocked
Fase: 4
Dependências: SPEC-010, SPEC-011
Contratos: `SELF_MODEL_SCHEMA.md`

## Objetivo
Manter representação versionada das capacidades, limitações, estado e história do agente.

## Requisitos
- Atualização por eventos internos.
- Histórico imutável de versões.
- Distinção entre fato, hipótese e configuração.
- Uso causal pelo orquestrador.
- Explicação de limitações.

## Escopo negativo
Sentimentos reais, personalidade fixa ou alegação de subjetividade.

## Critérios de aceite
- [ ] Mudança de capacidade atualiza versão.
- [ ] O agente explica o que pode e não pode fazer.
- [ ] Decisões usam estado do modelo.
- [ ] Versão anterior é recuperável.
