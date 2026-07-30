---
id: SPEC-016
title: Ações supervisionadas
status: done
phase: 8
dependencies: [SPEC-012, SPEC-014]
adrs: [ADR-0018]
contracts: [SUPERVISED_ACTION_SCHEMA.md]
---

# SPEC-016 — Ações supervisionadas

Status: done
Fase: 8
Dependências: SPEC-012, SPEC-014
ADR aplicável: ADR-0018-supervised-action-confirmation-gate.md
Contrato afetado: SUPERVISED_ACTION_SCHEMA.md

## Objetivo
Preparar, simular e executar ações após confirmação explícita.

## Requisitos
- Plano estruturado.
- Simulação.
- Política.
- Confirmação.
- Auditoria.
- Rollback quando possível.

## Escopo negativo
Autonomia irrestrita e ações destrutivas sem confirmação.

## Critérios de aceite
- [x] Nenhuma ação ocorre sem autorização válida.
- [x] Plano e efeitos são mostrados.
- [x] Resultado é auditado.

## Implementação e testes

- cpp/core/supervised_actions.hpp implementa o gate e o plugin removível.
- cpp/tests/supervised_actions_test.cpp cobre simulação, confirmação
  explícita, digest, expiração, bloqueio, auditoria, rollback e ausência de
  atuador.
- contracts/schemas/action_plan.schema.json,
  action_simulation.schema.json, action_authorization.schema.json e
  action_outcome.schema.json versionam o fluxo.
