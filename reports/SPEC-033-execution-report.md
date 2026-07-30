# Relatório de Execução

SPEC: SPEC-033 — Promoção nativa da segmentação de episódios  
Agente: Codex  
Data: 2026-07-29  
Commit: pendente de criação

## Alterações realizadas

- Congelada a referência Python e criada a promoção
  `cognition.episode_segmentation.v1`.
- Implementado o segmentador C++ determinístico, com UUID5, fingerprint de
  configuração, razões de fronteira e contrato de episódio completo.
- Exposto `EpisodeSegmentationPlugin` com lifecycle removível e
  `CapabilityDescriptor` próprio.
- Criados fixtures de equivalência e holdout separado com hashes registrados.
- Criado o runner C++ de JSON-lines e o validador de promoção com equivalência,
  invariantes, baseline, ablação e métricas p50/p95/máximo.

## Arquivos modificados

- `cpp/core/digest.hpp`
- `cpp/core/episode_segmenter.hpp`
- `cpp/app/promotion_fixture_runner.cpp`
- `cpp/tests/episode_segmentation_test.cpp`
- `CMakeLists.txt`
- `python/tests/test_episode_promotion.py`
- `tools/validate_episode_promotion.py`
- `validation/equivalence/episode_segmentation_v1.jsonl`
- `validation/holdout/episode_segmentation_v1_holdout.jsonl`
- `validation/holdout/manifest.json`
- `validation/reports/episode_segmentation_v1.json`
- `promotions/cognition.episode_segmentation.v1.json`
- `docs/03-contracts/EPISODE_PROMOTION_CONTRACT.md`
- `docs/04-adrs/ADR-0029-atomic-episode-segmentation-promotion.md`
- `specs/SPEC-033-episode-segmentation-native-promotion.md`
- `contracts/fixtures/component_maturity.json`
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`
- `docs/08-roadmap/ROADMAP.md`

## Testes executados

- `cmake --build --preset dev` — aprovado.
- `ctest --test-dir build/dev --output-on-failure` — 15/15 aprovados.
- `PYTHONPATH=python python3 tools/validate_episode_promotion.py` — equivalência,
  holdout, invariantes e performance aprovados.
- `PYTHONPATH=python python3 -m unittest python.tests.test_episode_promotion -v` —
  aprovado.
- `PYTHONPATH=python python3 -m unittest discover -s python/tests -v` — 183/183
  aprovados.
- `PYTHONPATH=python python3 tools/validate_hybrid.py` — aprovado; release sem
  arquivos de runtime Python.
- CTest Windows/MSVC/vcpkg — 15/15 aprovados.
- Validação de contratos, maturidade, 34 SPECs e documentação — aprovadas.

## Resultados

O candidato C++ coincide semanticamente com a referência nos quatro casos de
desenvolvimento e nos dois casos do holdout. A medição local registrada no
relatório gerado apresentou p50 de 6,06 ms, p95 de 6,99 ms, máximo de 7,25 ms,
RSS máximo observado de 27,63 MiB e throughput de aproximadamente 650 casos/s.
Esses números são operacionais e não evidência de validade cognitiva.

## Critérios de aceite

Os gates automatizados de equivalência, invariantes, baseline, ablação,
holdout, contrato, lifecycle e benchmark passaram. A maturidade foi atualizada
para `reference_status: frozen`, `native_status: equivalent` e
`product_status: unavailable`.

## Desvios

A promoção não foi inserida em `promotions/registry.json`, pois esse registro
exige uma revisão humana e `approval_review_id`. Nenhum identificador foi
inventado. Portanto, a SPEC permanece em validação documental para fins de
promoção, embora o candidato esteja equivalente nos gates automatizados.

## Riscos e pendências

- A validade científica depende de ground truth independente e estudos futuros;
  a referência Python não é oráculo.
- A capacidade ainda não é conectada ao runtime de produto nem às demais
  capacidades cognitivas.
- A aprovação final requer decisão humana registrada.

## Decisões tomadas

- A promoção é atômica e não antecipa memória, padrões ou diálogo.
- Contexto ausente preserva o último contexto observado; não gera evidência
  negativa.
- O holdout é imutável para esta versão e qualquer mudança exige nova promoção.

## Evidências

- `validation/reports/episode_segmentation_v1.json`
- `validation/reports/episode_segmentation_v1_divergences.json`
- `promotions/cognition.episode_segmentation.v1.json`
- `validation/holdout/manifest.json`
