---
id: SPEC-011
title: Metacognição e curiosidade
status: blocked
phase: 4
dependencies: [SPEC-009, SPEC-010]
adrs: []
contracts: [HYPOTHESIS_SCHEMA.md]
---

# SPEC-011 — Metacognição e curiosidade

Status: blocked
Fase: 4
Dependências: SPEC-009, SPEC-010
Contratos: `HYPOTHESIS_SCHEMA.md`

## Objetivo
Avaliar hipóteses e gerar perguntas com ganho informacional.

## Requisitos
- Confiança calibrável.
- Evidência favorável e contrária.
- Alternativas.
- Orçamento de interrupção.
- Supressão de perguntas redundantes.
- Registro de resposta e atualização.

## Escopo negativo
Objetivos irrestritos ou busca autônoma externa.

## Critérios de aceite
- [ ] Toda pergunta referencia hipótese.
- [ ] Pergunta possui estimativa de ganho.
- [ ] Correções reduzem repetição.
- [ ] Sistema pode decidir permanecer em silêncio.
