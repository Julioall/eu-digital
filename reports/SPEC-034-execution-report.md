# Relatório de Execução

SPEC: SPEC-034 — Promoção nativa da memória episódica  
Agente: Codex  
Data: 2026-07-30
Commit: pendente de criação

## Alterações realizadas

- Congelada a referência Python e criado o manifesto
  `cognition.episodic_memory.v1`.
- Implementado armazenamento C++ de episódios imutáveis por ID, recuperação
  contextual/temporal, consulta por embedding opcional e relações explícitas.
- Implementada retenção limitada determinística sem gerar fatos, resumos ou
  generalizações semânticas.
- Exposto plugin C++ removível com `CapabilityDescriptor` próprio.
- Criados runner JSON-lines, fixtures de equivalência, holdout bloqueado,
  baseline cronológico, ablação e benchmark operacional.

## Arquivos modificados

- `cpp/core/episodic_memory.hpp`
- `cpp/app/promotion_fixture_runner.cpp`
- `cpp/tests/episodic_memory_test.cpp`
- `CMakeLists.txt`
- `python/tests/test_episodic_memory_promotion.py`
- `tools/validate_memory_promotion.py`
- `validation/equivalence/episodic_memory_v1.jsonl`
- `validation/holdout/episodic_memory_v1_holdout.jsonl`
- `validation/holdout/episodic_memory_manifest.json`
- `validation/reports/episodic_memory_v1.json`
- `validation/reports/episodic_memory_v1_divergences.json`
- `promotions/cognition.episodic_memory.v1.json`
- `docs/03-contracts/EPISODIC_MEMORY_PROMOTION_CONTRACT.md`
- `docs/04-adrs/ADR-0030-atomic-episodic-memory-promotion.md`
- `specs/SPEC-034-episodic-memory-native-promotion.md`
- `contracts/fixtures/component_maturity.json`
- `docs/06-operations/DEVELOPMENT_COMMANDS.md`
- `docs/08-roadmap/ROADMAP.md`

## Testes executados

- `cmake --build --preset dev` — aprovado.
- `ctest --test-dir build/dev --output-on-failure` — 17/17 aprovados.
- `PYTHONPATH=python python3 tools/validate_memory_promotion.py` — equivalência,
  holdout, invariantes e performance aprovados.
- `PYTHONPATH=python python3 -m unittest discover -s python/tests -v` — 187/187
  aprovados.
- `PYTHONPATH=python python3 tools/validate_hybrid.py` — aprovado; release sem
  arquivos de runtime Python.
- CTest Windows/MSVC/vcpkg — 17/17 aprovados.
- Validação de contratos, maturidade, 45 SPECs e documentação — aprovadas.

## Resultados

O candidato C++ coincidiu semanticamente com a referência nos quatro casos de
desenvolvimento e nos dois casos do holdout. A última medição registrada
apresentou p50 de 13,1875 ms, p95 de 15,8789 ms, máximo de 17,8301 ms,
memória de 0,1953 MiB e throughput de 293,90 casos/s. São métricas
operacionais, não evidência cognitiva.

## Critérios de aceite

Os gates locais de equivalência, recuperação, proveniência, relações,
retenção, ablação, holdout, lifecycle e benchmark passaram. O componente foi
atualizado para `reference_status: frozen`, `native_status: equivalent` e
`product_status: unavailable`.

## Desvios

A promoção não foi inserida em `promotions/registry.json`, porque esse registro
exige uma revisão humana com `approval_review_id`. Nenhum identificador foi
inventado; a capacidade não está disponível no produto.

## Riscos e pendências

- A validade científica requer ground truth independente e avaliação ecológica;
  equivalência Python/C++ não é prova suficiente.
- Consolidação semântica e criação de conhecimento permanecem na SPEC-020 e
  não foram antecipadas.
- A aprovação final do registry requer decisão humana registrada.

## Decisões tomadas

- Embeddings são vetores locais opcionais de avaliação, não dependência do
  runtime nem modelo obrigatório.
- Duplicidade por `episode_id` não substitui a fonte já armazenada.
- Consulta sem filtros usa fallback cronológico determinístico.
- Ausência de embedding exclui apenas a pontuação de embedding; não vira
  observação negativa sobre o episódio.

## Continuação da execução — 2026-07-30

- Corrigida a validação C++ para rejeitar episódios incompletos e consultas
  com embedding vazio.
- Hashes de fixtures agora são estáveis entre LF e CRLF; o pipeline Python,
  o harness de datasets e os testes de promoção usam a mesma semântica.
- `python -m unittest discover -s python/tests -v`: 187/187 aprovados.
- Runner isolado C++ e teste nativo de memória: aprovados.
- Validação SPEC-034: equivalência, holdout, invariantes e performance
  aprovados; p50 13,1875 ms, p95 15,8789 ms, máximo 17,8301 ms,
  memória 0,1953 MiB e throughput 293,90 casos/s.
- Validação híbrida MSVC com SQLite3 nativo via vcpkg: 187/187 testes Python,
  17/17 testes CTest e instalação do runtime sem arquivos Python.

As métricas acima são operacionais e não constituem evidência cognitiva.

## Evidências

- `validation/reports/episodic_memory_v1.json`
- `validation/reports/episodic_memory_v1_divergences.json`
- `promotions/cognition.episodic_memory.v1.json`
- `validation/holdout/episodic_memory_manifest.json`
