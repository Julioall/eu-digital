# Relatório de Execução

SPEC: SPEC-036 — Promoção nativa do world model e erro preditivo  
Agente: Codex  
Data: 2026-07-30  
Commit: pendente de criação

## Alterações realizadas

- Implementado `WorldModel` C++ local, determinístico e incremental com as
  políticas de frequência, Markov de primeira ordem e n-grama incremental.
- Implementados distribuição completa, top-k, log loss, contribuição limitada
  de saliência, confiança, drift explícito e início de reaprendizagem.
- Adicionada entrada explícita de padrões promovidos como vocabulário simbólico
  de estados; status e confiança são validados, sem inferir fatos ou ações.
- Exposto `WorldModelPlugin` removível com `CapabilityDescriptor`; nenhum
  workspace, self-model, diálogo, sensor, atuador ou modelo generativo foi
  integrado.
- Criados runner JSON-lines, fixtures de desenvolvimento e holdout bloqueado,
  baseline de frequência, ablação sem contexto, calibração e benchmark.

## Arquivos modificados

- `cpp/core/world_model.hpp`
- `cpp/app/promotion_fixture_runner.cpp`
- `cpp/tests/world_model_test.cpp`
- `CMakeLists.txt`
- `python/tests/test_world_model_promotion.py`
- `tools/validate_world_model_promotion.py`
- `validation/equivalence/world_model_prediction_v1.jsonl`
- `validation/holdout/world_model_prediction_v1_holdout.jsonl`
- `validation/reports/world_model_prediction_v1.json`
- `validation/reports/world_model_prediction_v1_divergences.json`
- `promotions/cognition.world_model.v1.json`
- `contracts/fixtures/component_maturity.json`
- `docs/03-contracts/WORLD_MODEL_PREDICTION_SCHEMA.md`
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`
- `python/README.md`
- `specs/SPEC-036-world-model-prediction-native-promotion.md`
- `docs/08-roadmap/ROADMAP.md`

## Testes executados

- Build C++ MSVC/vcpkg — aprovado.
- CTest Windows/MSVC/vcpkg — 18/18 aprovados.
- `PYTHONPATH=python python -m unittest discover -s python/tests` — 189/189
  aprovados.
- `PYTHONPATH=python python tools/validate_world_model_promotion.py` —
  equivalência, holdout, invariantes, baseline, ablação, calibração e
  performance aprovados.
- Validação de contratos, maturidade, 45 SPECs e documentação — aprovada.

## Resultados

O candidato C++ coincidiu semanticamente com a referência Python no conjunto
de desenvolvimento e no holdout bloqueado. O tratamento incremental apresentou
log loss médio de 0,8655 nos casos de desenvolvimento, contra 1,0358 do
baseline de frequência; a ablação sem contexto apresentou 1,0358. A medição
operacional mais recente apresentou p50 de 18,5355 ms, p95 de 38,1871 ms,
máximo de 67,6674 ms, memória medida de 0,1953 MiB pelo medidor Windows e
throughput de 231,80 casos/s. Essas métricas são operacionais, não evidência
cognitiva.

## Critérios de aceite

Os gates automatizados de referência congelada, hashes, equivalência, holdout,
incerteza na ausência de observação, drift, baseline, ablação, calibração,
lifecycle, contratos, documentação e CTest passaram. O componente foi
atualizado para `reference_status: frozen`, `native_status: equivalent` e
`product_status: unavailable`.

## Desvios e pendências

A promoção não foi inserida em `promotions/registry.json`, pois esse registro
exige revisão humana identificável. Nenhum `approval_review_id` foi inventado;
portanto, a capacidade continua indisponível no produto. A equivalência Python/
C++ não demonstra validade científica nem validade ecológica.

## Evidências

- `validation/reports/world_model_prediction_v1.json`
- `validation/reports/world_model_prediction_v1_divergences.json`
- `promotions/cognition.world_model.v1.json`
- `validation/holdout/world_model_prediction_v1_holdout.jsonl`
