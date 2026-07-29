# Relatório de Execução

SPEC: SPEC-016 — Ações supervisionadas
Agente: Codex
Data: 2026-07-29
Commit: feat: complete SPEC-016 supervised actions

## Alterações realizadas

Foi implementado um controlador C++ local com fluxo prepare, authorize,
execute e rollback. A execução depende de política permitida, confirmação
explícita não expirada e correspondência exata de plan_id e plan_digest.
Simulação, efeitos, ausência de atuador, resultado e rollback são auditáveis.

## Arquivos modificados

- cpp/core/supervised_actions.hpp
- cpp/tests/supervised_actions_test.cpp
- CMakeLists.txt
- contracts/schemas/action_plan.schema.json
- contracts/schemas/action_simulation.schema.json
- contracts/schemas/action_authorization.schema.json
- contracts/schemas/action_outcome.schema.json
- docs/03-contracts/SUPERVISED_ACTION_SCHEMA.md
- docs/04-adrs/ADR-0018-supervised-action-confirmation-gate.md
- docs/02-architecture/COMPONENT_CATALOG.md
- docs/05-governance/OPEN_QUESTIONS.md
- docs/06-operations/DEVELOPMENT_COMMANDS.md
- contracts/README.md
- specs/SPEC-016-supervised-actions.md

## Testes executados

- validação JSON dos quatro schemas de ação
- python tools/validate_contracts.py
- python -m unittest discover -s python/tests -v
- cmake --build build/dev --target supervised_actions_test
- ctest --test-dir build/dev -R supervised_actions --output-on-failure
- python tools/validate_hybrid.py
- ruff check .
- ruff format --check .
- mypy python/eu_digital_lab python/tests

## Resultados

- Schemas de ação: JSON válido.
- Contrato canônico: passou.
- Testes Python: 128 passaram.
- Teste nativo supervised_actions: passou.
- Build, CTest completo e instalação híbrida: passam; release sem Python.
- ruff check e ruff format: permanecem bloqueados pelas violações
  preexistentes registradas no relatório da SPEC-015.
- mypy: permanece bloqueado pelos 6 erros preexistentes registrados no
  relatório da SPEC-015.
- Validadores documentais PowerShell: não executados porque pwsh não está
  instalado neste ambiente.

## Critérios de aceite

- [x] Nenhuma ação ocorre sem autorização válida.
- [x] Plano e efeitos são mostrados.
- [x] Resultado é auditado.

## Desvios

Nenhum desvio funcional. O identificador da capacidade usa
actuation.supervised para respeitar o teste arquitetural que rejeita padrões
de importação actions.* no núcleo.

## Riscos e pendências

- Nenhum atuador concreto foi habilitado; integrações reais exigem adaptador,
  política, simulação e rollback próprios.
- Rollback é melhor esforço e pode falhar; a falha é registrada separadamente.
- O lint e os tipos Python globais permanecem pendentes em alterações
  anteriores e não foram misturados neste commit.

## Decisões tomadas

- Simulação nunca chama execução.
- A autorização é consumida após tentativa de execução.
- Ausência de fornecedor resulta em bloqueio explícito.
- O controlador não abre aplicativos, manipula arquivos nem envia dados.

## Evidências

- ADR: docs/04-adrs/ADR-0018-supervised-action-confirmation-gate.md.
- Contratos: contracts/schemas/action_plan.schema.json,
  action_simulation.schema.json, action_authorization.schema.json e
  action_outcome.schema.json.
- Teste nativo: cpp/tests/supervised_actions_test.cpp.
