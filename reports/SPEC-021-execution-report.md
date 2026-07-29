# Relatório de Execução

SPEC: SPEC-021 — World model e erro preditivo
Agente: Codex
Data: 2026-07-29
Commit: feat: complete SPEC-021 world model prediction error

## Alterações realizadas

O bloqueio foi resolvido com ADR-0021 e contratos para distribuição preditiva,
erro e drift. A referência Python compara frequência, Markov de primeira ordem
e n-grama incremental; registra log loss/top-k; mapeia erro para o fator
`surprise` do workspace; e, sob drift, reduz confiança e limpa somente
contagens derivadas para iniciar reaprendizagem.

## Arquivos modificados

- python/eu_digital_lab/world_model.py
- python/tests/test_world_model.py
- python/eu_digital_lab/__init__.py
- contracts/schemas/world_model_prediction.schema.json
- contracts/schemas/prediction_error.schema.json
- contracts/schemas/prediction_drift.schema.json
- docs/03-contracts/WORLD_MODEL_PREDICTION_SCHEMA.md
- docs/04-adrs/ADR-0021-incremental-world-model-prediction-error.md
- docs/02-architecture/COMPONENT_CATALOG.md
- docs/05-governance/OPEN_QUESTIONS.md
- docs/06-operations/DEVELOPMENT_COMMANDS.md
- contracts/README.md
- python/README.md
- specs/SPEC-021-world-model-prediction-error.md

## Testes executados

- validação JSON dos três schemas;
- teste unitário da SPEC-021;
- Ruff e mypy nos arquivos novos;
- suíte Python completa;
- build, CTest, instalação e verificação de release sem Python.

## Critérios de aceite

- [x] O n-grama incremental supera o baseline de frequência no holdout
  sequencial do teste.
- [x] Log loss é convertido em `salience_contribution` e encaminhado ao fator
  auditável `surprise` do workspace.
- [x] Drift por janela de erro reduz confiança, publica `DriftSignal` e inicia
  reaprendizagem sem apagar observações.

## Desvios e limites

Nenhum desvio funcional. O modelo cobre estados discretos observados e não
introduz planejamento, ação, LLM, telemetria ou dependência externa. O teste
de holdout é evidência de verificação do mecanismo, não validade cognitiva.

## Evidências

- ADR: `docs/04-adrs/ADR-0021-incremental-world-model-prediction-error.md`;
- contratos: `contracts/schemas/world_model_prediction.schema.json`,
  `prediction_error.schema.json` e `prediction_drift.schema.json`;
- testes: `python/tests/test_world_model.py`.
