# Relatório de Execução

SPEC: SPEC-035 — Promoção nativa da aprendizagem incremental de padrões  
Agente: Codex  
Data: 2026-07-29  
Commit: será registrado após a criação dos commits separados

## Alterações realizadas

- Congelada a referência Python e criado o manifesto
  `cognition.pattern_learning.v1`.
- Implementado learner C++ com distância sobre features compartilhadas,
  suporte, estabilidade, recência, confiança, feedback e drift versionado.
- Criado plugin removível com `CapabilityDescriptor`.
- Criado runner JSON-lines, fixtures de desenvolvimento/holdout, baseline de
  chave exata, ablação e benchmark operacional.
- Atualizados maturidade, contrato, ADR, SPEC e comandos de desenvolvimento.

## Arquivos modificados

- `cpp/core/pattern_learner.hpp`
- `cpp/app/promotion_fixture_runner.cpp`
- `cpp/tests/pattern_learning_test.cpp`
- `python/tests/test_pattern_promotion.py`
- `CMakeLists.txt`
- `tools/validate_pattern_promotion.py`
- `validation/equivalence/pattern_learning_v1.jsonl`
- `validation/holdout/pattern_learning_v1_holdout.jsonl`
- `validation/reports/pattern_learning_v1.json`
- `validation/reports/pattern_learning_v1_divergences.json`
- `promotions/cognition.pattern_learning.v1.json`
- `docs/03-contracts/PATTERN_PROMOTION_CONTRACT.md`
- `docs/04-adrs/ADR-0031-atomic-pattern-learning-promotion.md`
- `specs/SPEC-035-pattern-learning-native-promotion.md`
- `contracts/fixtures/component_maturity.json`
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`
- `docs/08-roadmap/ROADMAP.md`
- `reports/SPEC-035-execution-report.md`

## Testes executados

- `cmake --build --preset dev` — aprovado.
- `ctest --test-dir build/dev --output-on-failure` — 17/17 aprovados.
- `PYTHONPATH=python python3 tools/validate_pattern_promotion.py` — equivalência,
  holdout, invariantes e performance aprovados.
- `PYTHONPATH=python python3 -m unittest discover -s python/tests -v` — 187/187
  aprovados.
- `PYTHONPATH=python python3 tools/validate_hybrid.py` — aprovado; release sem
  arquivos de runtime Python.
- `PYTHONPATH=python python3 tools/validate_contracts.py` — aprovado.
- `PYTHONPATH=python python3 tools/validate_component_maturity.py` — aprovado.
- `tools/validate_specs.ps1` — 45 SPECs válidas.
- `tests/documentation/Test-Documentation.ps1` — 13/13 verificações aprovadas.
- `tools/Invoke-WindowsRuntimeValidation.ps1` — Windows Runtime Preview,
  CMake/MSVC e CTest 17/17 aprovados.

## Resultados

O candidato C++ coincide semanticamente com a referência nos quatro casos de
desenvolvimento e nos dois casos do holdout. A medição final apresentou p50 de
6,61 ms, p95 de 7,67 ms, máximo de 8,06 ms, memória observada de 27,88 MiB e
throughput aproximado de 602 casos/s. São métricas operacionais, não evidência
cognitiva.

## Critérios de aceite

Os gates de suporte, feedback, drift, proveniência, baseline, ablação, holdout,
lifecycle, equivalência, suíte Python, CTest Linux/Windows, contratos,
documentação e validação híbrida passaram. A SPEC-035 foi marcada `done`; o
componente permanece `reference_status: frozen`, `native_status: equivalent` e
`product_status: unavailable`.

## Desvios

A promoção não foi adicionada a `promotions/registry.json`; a entrada requer
revisão humana com `approval_review_id`. O learner não foi conectado a world
model, workspace, diálogo ou ações.

## Riscos e pendências

- A equivalência não demonstra validade científica nem utilidade ecológica.
- O limite de RSS é medido operacionalmente pelo runner local e deve ser
  confirmado em hardware de referência.
- A aprovação final do registry continua pendente.

## Decisões tomadas

- Features ausentes não são convertidas em zero; a distância usa somente
  dimensões compartilhadas.
- Drift marca o parent como `superseded` e cria nova versão com proveniência.
- `promoted` é estado operacional do cluster, não nomeação ou verdade.

## Evidências

- `validation/reports/pattern_learning_v1.json`
- `validation/reports/pattern_learning_v1_divergences.json`
- `promotions/cognition.pattern_learning.v1.json`
