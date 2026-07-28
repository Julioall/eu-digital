# Relatório de Execução

SPEC: SPEC-010
Agente: Codex
Data: 2026-07-28
Commit: trabalho local não commitado

## Alterações realizadas

- criada ADR-0012 para fixar a semântica de workspace limitado, auditável e
  local;
- adicionados contratos versionados de candidato, item selecionado, snapshot e
  broadcast;
- implementada referência Python de competição por capacidade, score
  determinístico, expiração, limite de recurso, justificativas e estado
  observável;
- adicionado broadcast interno como `CanonicalEvent` local de tipo
  `workspace.selection.v1`, por uma porta injetada;
- adicionados baseline FIFO selecionável pela mesma configuração, ablação de
  fatores, métricas de seleção e invariantes metamórficas;
- reforçado o validador compartilhado para respeitar limites máximos de schema.

## Arquivos modificados

- `docs/04-adrs/ADR-0012-bounded-auditable-global-workspace.md`;
- `contracts/schemas/workspace_*.schema.json`;
- `docs/03-contracts/GLOBAL_WORKSPACE_SCHEMA.md`;
- `python/eu_digital_lab/global_workspace.py`;
- `python/eu_digital_lab/schema_validation.py`;
- `python/tests/test_global_workspace.py`;
- `specs/SPEC-010-global-workspace.md`;
- documentação operacional, de contratos e do laboratório Python.

## Testes executados

```text
PYTHONPATH=python python3 -m unittest python.tests.test_global_workspace -v
PYTHONPATH=python python3 -m unittest discover -s python/tests -v
python3 -m compileall -q python
python3 tools/validate_contracts.py
python3 tools/check_promotions.py
ruff check (arquivos da SPEC-010)
mypy (módulos da SPEC-010)
python3 tools/validate_hybrid.py
```

## Resultados

- 12 testes específicos aprovados;
- suíte Python completa: 91 testes aprovados;
- Ruff direcionado e mypy aprovados;
- schemas do workspace parseados e contratos compartilhados validados;
- gate de promoção aprovado;
- fluxo híbrido completo aprovado, com 8/8 CTest e release sem runtime Python.

## Critérios de aceite

- [x] itens competem por capacidade;
- [x] seleção é auditável;
- [x] itens antigos expiram;
- [x] mudança explícita de prioridade altera o conteúdo ativo.

## Desvios

O mecanismo é uma referência de laboratório Python. Não há persistência de
longo prazo, promoção C++, ação, diálogo, modelo pesado ou alegação de
consciência nesta SPEC.

## Riscos e pendências

- a hipótese precisa de sessões anotadas e holdout congelado para validade
  científica; testes unitários verificam contrato e determinismo, não eficácia
  ecológica;
- pesos são baseline configurável e não foram aprendidos;
- qualquer porta C++ exige promoção pela SPEC-026 e validação independente.

## Decisões tomadas

- ausência de fator é representada em `missing_factors`, não por valor zero;
- desempates por `candidate_id` e relógio injetado tornam replay determinístico;
- FIFO é um controle selecionável pela mesma interface para ablações causais;
- broadcast recebe somente um publisher local injetado e preserva snapshots
  versionados.

## Evidências

- ADR: `docs/04-adrs/ADR-0012-bounded-auditable-global-workspace.md`;
- contratos: `docs/03-contracts/GLOBAL_WORKSPACE_SCHEMA.md`;
- implementação: `python/eu_digital_lab/global_workspace.py`;
- testes: `python/tests/test_global_workspace.py`;
- SPEC/protocolo: `specs/SPEC-010-global-workspace.md`.
