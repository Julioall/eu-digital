# Relatório de Execução

SPEC: SPEC-022 — Avaliação longitudinal
Agente: Codex
Data: 2026-07-29
Commit: feat: complete SPEC-022 longitudinal evaluation

## Alterações realizadas

O bloqueio foi resolvido com ADR-0022 e contratos para protocolo congelado,
snapshots e relatório. A referência Python exige hash do holdout, aceita os
checkpoints 7/30/90, separa métricas cognitivas e operacionais, calcula curva
de retenção/calibração, reporta ganhos/perdas contra o primeiro snapshot e
quantifica deriva de versão/digest do self-model. O relatório é determinístico
e pode ser reconstruído por replay local dos snapshots.

## Arquivos modificados

- python/eu_digital_lab/longitudinal_evaluation.py
- python/tests/test_longitudinal_evaluation.py
- python/eu_digital_lab/__init__.py
- contracts/schemas/longitudinal_protocol.schema.json
- contracts/schemas/longitudinal_snapshot.schema.json
- contracts/schemas/longitudinal_report.schema.json
- docs/03-contracts/LONGITUDINAL_EVALUATION_SCHEMA.md
- docs/04-adrs/ADR-0022-frozen-longitudinal-evaluation.md
- docs/02-architecture/COMPONENT_CATALOG.md
- docs/05-governance/OPEN_QUESTIONS.md
- docs/06-operations/DEVELOPMENT_COMMANDS.md
- contracts/README.md
- python/README.md
- specs/SPEC-022-longitudinal-cognitive-evaluation.md

## Testes executados

- validação JSON dos três schemas;
- testes unitários da SPEC-022;
- Ruff e mypy nos arquivos novos e exportações;
- suíte Python completa;
- build, CTest, instalação e verificação de release sem Python.

## Critérios de aceite

- [x] O relatório é idêntico quando reconstruído a partir dos mesmos
  snapshots.
- [x] Curva de retenção, calibração e ganhos/perdas são reportados por
  categoria, com baseline cronológico explícito.
- [x] Delta de versão e mudanças de digest do self-model são quantificados,
  preservando proveniência.

## Desvios e limites

Nenhum desvio funcional. A referência não coleta dados, não altera métricas
após observação, não envia dados para a nuvem e não interpreta drift como
consciência ou identidade fenomenal. Validade ecológica exige a coleta real
posterior prevista nos gates científicos.

## Evidências

- ADR: `docs/04-adrs/ADR-0022-frozen-longitudinal-evaluation.md`;
- contratos: `contracts/schemas/longitudinal_protocol.schema.json`,
  `longitudinal_snapshot.schema.json` e `longitudinal_report.schema.json`;
- testes: `python/tests/test_longitudinal_evaluation.py`.
