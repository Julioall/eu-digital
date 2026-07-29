# Relatório de Execução

SPEC: SPEC-019 — Propriocepção e agência digital
Agente: Codex
Data: 2026-07-29
Commit: feat: complete SPEC-019 digital proprioception and agency

## Alterações realizadas

O bloqueio documental foi resolvido com ADR-0019 e cinco contratos versionados.
Foi implementada a referência Python do laboratório para estado corporal
funcional, intenção, cópia eferente, resultado observado, atribuição, erro
preditivo, baseline passivo e macro-F1.

## Arquivos modificados

- python/eu_digital_lab/digital_proprioception_agency.py
- python/tests/test_digital_proprioception_agency.py
- python/eu_digital_lab/__init__.py
- contracts/schemas/digital_body_state.schema.json
- contracts/schemas/action_intention.schema.json
- contracts/schemas/efference_copy.schema.json
- contracts/schemas/agency_action_outcome.schema.json
- contracts/schemas/agency_attribution.schema.json
- docs/03-contracts/DIGITAL_PROPRIOCEPTION_AGENCY_SCHEMA.md
- docs/04-adrs/ADR-0019-digital-proprioception-agency-loop.md
- docs/02-architecture/COMPONENT_CATALOG.md
- docs/05-governance/OPEN_QUESTIONS.md
- docs/06-operations/DEVELOPMENT_COMMANDS.md
- contracts/README.md
- specs/SPEC-019-digital-proprioception-and-agency.md

## Testes executados

- validação JSON dos cinco schemas de propriocepção/agência
- python tools/validate_contracts.py
- ruff check nos arquivos novos
- mypy nos arquivos novos
- python tools/validate_hybrid.py

## Resultados

- Schemas e contrato canônico: passaram.
- Ruff e mypy dos arquivos novos: passaram.
- Testes Python: 136 passaram.
- CTest: 10/10 passaram.
- Build e instalação híbridos: passaram; release sem Python.
- Lint e mypy globais continuam com violações preexistentes registradas nas
  fases anteriores; nenhum erro novo foi introduzido pela SPEC-019.
- Validadores documentais PowerShell não executados porque pwsh não está
  instalado neste ambiente.

## Critérios de aceite

- [x] Distingue efeitos próprios e externos acima do baseline em fixture
  determinística: tratamento correlacionado e baseline passivo comparados por
  macro-F1.
- [x] Toda ação possui intenção e resultado correlacionados quando a
  correlação está disponível; ausência permanece ambígua.
- [x] Ablation do loop reduz atribuição de agência: passive_observer_v0 não
  cria efference copy e perde o caso próprio no teste comparativo.

## Desvios

Nenhum desvio funcional. A implementação permanece Python de laboratório,
conforme a separação metodológica; promoção para C++ não foi antecipada.

## Riscos e pendências

- A métrica usa fixtures com ground truth; validade ecológica exige estudo
  posterior com holdout e sessões controladas.
- Correlação temporal incompleta produz ambiguidade, por desenho.
- O módulo não afirma intenção subjetiva, consciência, emoção ou self
  fenomenal.

## Decisões tomadas

- external só pode ser atribuído por observation_origin explicit_external.
- ausência de action_id/correlation_id não é efeito externo.
- treatment e baseline usam a mesma interface e políticas identificáveis.
- a referência Python não é ground truth científico nem promoção automática.

## Evidências

- ADR: docs/04-adrs/ADR-0019-digital-proprioception-agency-loop.md.
- Contratos: contracts/schemas/digital_body_state.schema.json,
  action_intention.schema.json, efference_copy.schema.json,
  agency_action_outcome.schema.json e agency_attribution.schema.json.
- Teste: python/tests/test_digital_proprioception_agency.py.
